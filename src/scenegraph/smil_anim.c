/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato - Jean Le Feuvre
 *    Copyright (c)2004-200X ENST - All rights reserved
 *
 *  This file is part of GPAC / SVG Scene Graph sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.	
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/scenegraph_svg.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg.h>

static void gf_smil_anim_animate_using_values(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributes *anim = rai->anim_elt->anim;
	GF_List *values = anim->values.values;
	SMIL_Timing_RTI *timing = rai->anim_elt->timing->runtime;

	GF_FieldInfo value_info, value_info_next;
	u32 keyValueIndex;
	u32 nbValues;

	Fixed interval_duration;
	Fixed interpolation_coefficient;

	memset(&value_info, 0, sizeof(GF_FieldInfo));
	value_info.fieldType = anim->values.type;
	value_info.eventType = anim->values.transform_type;
	value_info_next = value_info;

	nbValues = gf_list_count(values);
	if (nbValues == 1) {
		if (rai->previous_key_index != 0) {
			value_info.far_ptr = gf_list_get(values, 0);
			gf_svg_attributes_pointer_update(&value_info, &rai->owner->presentation_value, &rai->owner->current_color_value);
			svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
			rai->previous_key_index = 0;
			rai->target_value_changed = 1;
		}
		return;
	}

	if (gf_list_count(anim->keyTimes)) {
		u32 keyTimeIndex;
		Fixed keyTimeBefore, keyTimeAfter=0; 
		u32 keyTimesCount = gf_list_count(anim->keyTimes);
		for (keyTimeIndex = rai->last_keytime_index; keyTimeIndex<keyTimesCount; keyTimeIndex++) {
			Fixed *t = gf_list_get(anim->keyTimes, keyTimeIndex);
			if (normalized_simple_time < *t) {
				rai->last_keytime_index = keyTimeIndex;
				keyTimeBefore = *(Fixed *) gf_list_get(anim->keyTimes, keyTimeIndex-1);
				keyTimeAfter = *t;
				break;
			}
		}
		keyTimeIndex--;
		keyValueIndex = keyTimeIndex;
		interval_duration = keyTimeAfter - keyTimeBefore;
		if (interval_duration) interpolation_coefficient = gf_divfix(normalized_simple_time - keyTimeBefore, interval_duration);
		else interpolation_coefficient = 1;
//		fprintf(stdout, "Using Key Times: index %d, interval duration %.2f, coeff: %.2f\n", keyTimeIndex, interval_duration, interpolation_coefficient);
	} else {
		if (anim->calcMode == SMIL_CALCMODE_DISCRETE) {
			keyValueIndex = (u32)FIX2INT(normalized_simple_time*nbValues);			
			interpolation_coefficient = normalized_simple_time*nbValues - keyValueIndex;
		} else {
			keyValueIndex = (u32)FIX2INT(normalized_simple_time*(nbValues-1));
			interpolation_coefficient = normalized_simple_time*(nbValues - 1) - keyValueIndex;
		}
//		fprintf(stdout, "Not Using Key Times: key value index %d, interval duration %.2f, coeff: %.2f\n", keyValueIndex, interval_duration, interpolation_coefficient);
	}

	if (gf_node_get_tag((GF_Node *)rai->anim_elt) == TAG_SVG_animateMotion) {
		SVGanimateMotionElement *am = (SVGanimateMotionElement *)rai->anim_elt;
		if (gf_list_count(am->keyPoints)) {
			interpolation_coefficient = *(Fixed *)gf_list_get(am->keyPoints, keyValueIndex);
			keyValueIndex = 0;
//			fprintf(stdout, "Using Key Points: key Value Index %d, coeff: %.2f\n", keyValueIndex, interpolation_coefficient);
		}
	}

	if (rai->previous_key_index == (s32)keyValueIndex &&
		rai->previous_coef == interpolation_coefficient) return;

	rai->previous_key_index = keyValueIndex;
	rai->target_value_changed = 1;

	switch (anim->calcMode) {
	case SMIL_CALCMODE_DISCRETE:
		value_info.far_ptr = gf_list_get(values, keyValueIndex);
		gf_svg_attributes_pointer_update(&value_info, &rai->owner->presentation_value, &rai->owner->current_color_value);
		svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
		break;
	case SMIL_CALCMODE_PACED:
		/* TODO: at the moment assume it is linear */
	case SMIL_CALCMODE_SPLINE:
		/* TODO: at the moment assume it is linear */
	case SMIL_CALCMODE_LINEAR:
		rai->previous_coef = interpolation_coefficient;
		if (keyValueIndex == nbValues - 1) {
			value_info.far_ptr = gf_list_get(values, nbValues - 1);
			gf_svg_attributes_pointer_update(&value_info, &rai->owner->presentation_value, &rai->owner->current_color_value);
			svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
		} else {
			value_info.far_ptr = gf_list_get(values, keyValueIndex);
			gf_svg_attributes_pointer_update(&value_info, &rai->owner->presentation_value, &rai->owner->current_color_value);
			value_info_next.far_ptr = gf_list_get(values, keyValueIndex+1);
			gf_svg_attributes_pointer_update(&value_info_next, &rai->owner->presentation_value, &rai->owner->current_color_value);
			svg_attributes_interpolate(&value_info, &value_info_next, &rai->interpolated_value, interpolation_coefficient, 1);
		}
		break;
	}
}

static void gf_smil_anim_set(SMIL_Anim_RTI *rai)
{
	GF_FieldInfo to_info;
	if (!rai->anim_elt->anim->to.type) return;
	to_info.fieldType = rai->anim_elt->anim->to.type;
	to_info.eventType = rai->anim_elt->anim->to.transform_type;
	to_info.far_ptr   = rai->anim_elt->anim->to.value;
	gf_svg_attributes_pointer_update(&to_info, &rai->owner->presentation_value, &rai->owner->current_color_value);

	svg_attributes_copy(&rai->interpolated_value, &to_info, 0);
	rai->target_value_changed = 1;
}

static void gf_smil_anim_animate_from_to(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributes *anim = rai->anim_elt->anim;
	SMIL_Timing_RTI *timing = rai->anim_elt->timing->runtime;

	GF_FieldInfo from_info, to_info;
	
	if (rai->previous_coef == normalized_simple_time) return;

	rai->previous_coef = normalized_simple_time;

	from_info.fieldType = anim->from.type;
	from_info.eventType = anim->from.transform_type;
	from_info.far_ptr = anim->from.value;
	gf_svg_attributes_pointer_update(&from_info, &rai->owner->presentation_value, &rai->owner->current_color_value);

	to_info.fieldType = anim->to.type;
	to_info.eventType = anim->to.transform_type;
	to_info.far_ptr = anim->to.value;
	gf_svg_attributes_pointer_update(&to_info, &rai->owner->presentation_value, &rai->owner->current_color_value);

	switch (anim->calcMode) {
	case SMIL_CALCMODE_DISCRETE:
		{
			/* before half of the duration stay at 'from' and then switch to 'to' */
			s32 useFrom = (normalized_simple_time<=FIX_ONE/2);
			if (useFrom == rai->previous_key_index) return;
			svg_attributes_copy(&rai->interpolated_value, (useFrom?&from_info:&to_info), 0);
			rai->previous_key_index = useFrom;
		}
		break;
	case SMIL_CALCMODE_SPLINE:
	case SMIL_CALCMODE_PACED:
	case SMIL_CALCMODE_LINEAR:
	default:
		svg_attributes_interpolate(&from_info, &to_info, &rai->interpolated_value, normalized_simple_time, 1);
		break;
	}
	rai->target_value_changed = 1;
}

static void gf_smil_anim_animate_from_by(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributes *anim = rai->anim_elt->anim;
	SMIL_Timing_RTI *timing = rai->anim_elt->timing->runtime;

	GF_FieldInfo from_info, by_info;
	
	if (rai->previous_coef == normalized_simple_time) return;

	rai->previous_coef = normalized_simple_time;

	from_info.fieldType = anim->from.type;
	from_info.eventType = anim->from.transform_type;
	from_info.far_ptr = anim->from.value;
	gf_svg_attributes_pointer_update(&from_info, &rai->owner->presentation_value, &rai->owner->current_color_value);

	by_info.fieldType = anim->by.type;
	by_info.eventType = anim->by.transform_type;
	by_info.far_ptr = anim->by.value;
	gf_svg_attributes_pointer_update(&by_info, &rai->owner->presentation_value, &rai->owner->current_color_value);

	switch (anim->calcMode) {
	case SMIL_CALCMODE_DISCRETE:
		{
			/* before half of the duration stay at 'from' and then switch to 'to' */
			s32 useFrom = (normalized_simple_time<=FIX_ONE/2);
			if (useFrom == rai->previous_key_index) return;
			if (useFrom) {
				svg_attributes_copy(&rai->interpolated_value, &from_info, 0);
			} else {
				svg_attributes_muladd(FIX_ONE, &from_info, FIX_ONE, &by_info, &rai->interpolated_value, 0);
			}
			rai->previous_key_index = useFrom;
		}
		break;
	case SMIL_CALCMODE_SPLINE:
	case SMIL_CALCMODE_PACED:
	case SMIL_CALCMODE_LINEAR:
	default:
		svg_attributes_muladd(FIX_ONE, &from_info, normalized_simple_time, &by_info, &rai->interpolated_value, 0);
		break;
	}
	rai->target_value_changed = 1;
}

static Bool gf_svg_compute_path_anim(SMIL_Anim_RTI *rai, GF_Matrix2D *m, Fixed normalized_simple_time) 
{
	Bool res = 0;
	Fixed offset;
	offset = gf_mulfix(normalized_simple_time, rai->length);
	gf_mx2d_init(*m);
	res = gf_path_iterator_get_transform(rai->path_iterator, offset, 1, m, 1, 0);
//	fprintf(stdout, "Time: %f - Offset: %f, Position: %f, %f\n", normalized_simple_time, offset, ((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[2], ((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[5]);
	switch (rai->rotate) {
	case SVG_NUMBER_AUTO:
		break;
	case SVG_NUMBER_AUTO_REVERSE:
		gf_mx2d_add_rotation(m, m->m[2], m->m[5], GF_PI);
		break;
	default:
		((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[0] = 1;
		((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[1] = 0;
		((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[3] = 0;
		((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[4] = 1;
	}
	return res;
}

static void gf_smil_anim_animate_using_path(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	Bool res = 0;
	gf_svg_compute_path_anim(rai, rai->interpolated_value.far_ptr, normalized_simple_time);
	if (res) rai->target_value_changed = 1;
}

static void gf_smil_anim_discard(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	GF_Node *targetNode = (GF_Node *)rai->anim_elt->xlink->href.target;
	if (!targetNode) return;
	/* deletes the node and replace all references to that node to NULL */
	gf_node_replace(targetNode, NULL, 0);
	/* sets the node to NULL in case of future calls to discard */
	rai->anim_elt->xlink->href.target = NULL;
	/* TODO: The animation (discard) does not need to run again */
	/* TODO: Invalidate the scene ?*/
}

static void gf_smil_anim_get_last_specified_value(SMIL_Anim_RTI *rai)
{
	SVGElement *e = rai->anim_elt;
	void *value = NULL;

	if (rai->path) {
		if (!rai->last_specified_value.far_ptr) rai->last_specified_value.far_ptr = malloc(sizeof(GF_Matrix2D));
		gf_svg_compute_path_anim(rai, rai->last_specified_value.far_ptr, FIX_ONE);
	} else if (gf_node_get_tag((GF_Node *)rai->anim_elt) == TAG_SVG_set) { 		
		rai->last_specified_value.fieldType = e->anim->to.type;
		rai->last_specified_value.eventType = e->anim->to.transform_type;
		rai->last_specified_value.far_ptr   = e->anim->to.value;
		return;
	}

	if (gf_list_count(e->anim->values.values)) {
		/* Ignore from/to/by*/
		rai->last_specified_value.fieldType = e->anim->values.type;
		rai->last_specified_value.eventType = e->anim->values.transform_type;
		rai->last_specified_value.far_ptr = gf_list_last(e->anim->values.values);
	} else if (e->anim->by.type && (e->anim->to.type == 0)) {
		rai->last_specified_value.fieldType = e->anim->by.type;
		rai->last_specified_value.eventType = e->anim->by.transform_type;
		rai->last_specified_value.far_ptr   = e->anim->by.value;
	} else { 
		rai->last_specified_value.fieldType = e->anim->to.type;
		rai->last_specified_value.eventType = e->anim->to.transform_type;
		rai->last_specified_value.far_ptr   = e->anim->to.value;
	}
	if (gf_svg_is_property_inherited(&rai->last_specified_value)) {
		rai->last_specified_value.fieldType = rai->owner->presentation_value.fieldType;
		rai->last_specified_value.eventType = rai->owner->presentation_value.eventType;
		rai->last_specified_value.far_ptr = rai->owner->presentation_value.far_ptr;
	}
	gf_svg_attributes_pointer_update(&rai->last_specified_value, &rai->owner->presentation_value, &rai->owner->current_color_value);
}

/* TODO: fix this ... */
static void gf_smil_anim_get_last_nb_iterations(SMIL_Anim_RTI *rai, u32 *nb_iterations)
{
	if (rai->anim_elt->timing->repeatCount.type) {
		*nb_iterations = (u32)rai->anim_elt->timing->repeatCount.count;
	} else {
		*nb_iterations = (u32)rai->anim_elt->timing->repeatDur.clock_value;
	}
}

static void gf_smil_anim_apply_accumulate(SMIL_Anim_RTI *rai)
{
	u32 nb_iterations = rai->anim_elt->timing->runtime->current_interval->nb_iterations;
	if (rai->anim_elt->anim->accumulate == SMIL_ACCUMULATE_SUM && nb_iterations > 0) {
		svg_attributes_muladd(INT2FIX(nb_iterations), &rai->last_specified_value, FIX_ONE, &rai->interpolated_value, &rai->interpolated_value, 1);
	} 
}

static SMIL_Anim_RTI *gf_smil_anim_get_anim_runtime_from_timing(SMIL_Timing_RTI *rti)
{
	SVGElement *e = rti->timed_elt;
	u32 i, j;
	if (!e->xlink->href.target) return NULL;
	for (i = 0; i < gf_node_animation_count((GF_Node *)e->xlink->href.target); i++) {
		SMIL_AttributeAnimations *aa = gf_node_animation_get((GF_Node *)e->xlink->href.target, i);
		for (j= 0; j < gf_list_count(aa->anims); j++) {
			SMIL_Anim_RTI *rai_tmp = gf_list_get(aa->anims, j);
			if (rai_tmp->anim_elt->timing->runtime == rti) {						
				return rai_tmp;
			}
		}
	}
	return NULL;
}

/* Compute the Animation Value (AV), possibly based on the Underlying Value (read-only) in the case 
of from-less/to-less/inherit values animations */
static void gf_smil_anim_compute_interpolation_value(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SVGElement *e = rai->anim_elt;

	if (gf_node_get_tag((GF_Node *)rai->anim_elt) == TAG_SVG_discard) {
		/* TODO */
		return;
	} else if (rai->path) {
		gf_smil_anim_animate_using_path(rai, normalized_simple_time);
		return;
	} else if (gf_node_get_tag((GF_Node *)rai->anim_elt) == TAG_SVG_set) { 		
		gf_smil_anim_set(rai);
		return;
	}

	if (gf_list_count(e->anim->values.values)) {
		/* Ignore from/to/by*/
		gf_smil_anim_animate_using_values(rai, normalized_simple_time);
	} else if (e->anim->by.type && (e->anim->to.type == 0)) {
		/* no to but a by, this is a by or a from-by animation */
		gf_smil_anim_animate_from_by(rai, normalized_simple_time);
	} else { 
		gf_smil_anim_animate_from_to(rai, normalized_simple_time);
	}
}

static void gf_smil_anim_animate(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	SMIL_Anim_RTI *rai = gf_smil_anim_get_anim_runtime_from_timing(rti);

	if (!rai) return;

//	fprintf(stdout, "Animation: %x @ time : %f\n", rti, normalized_simple_time);

	rai->target_value_changed = 0;
	gf_smil_anim_compute_interpolation_value(rai, normalized_simple_time);
	if (rai->target_value_changed) gf_smil_anim_apply_accumulate(rai);
	/* Apply additive behavior if required
		PV = (additive == sum ? PV + anim->IV : anim->IV); */
	if (rai->anim_elt->anim->additive == SMIL_ADDITIVE_SUM) {
		svg_attributes_add(&rai->owner->presentation_value, &rai->interpolated_value, &rai->owner->presentation_value, 1);
	} else {
		svg_attributes_copy(&rai->owner->presentation_value, &rai->interpolated_value, 1);
	}
}

static void gf_smil_anim_freeze(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	SVGElement *e = rti->timed_elt;
	SMIL_Anim_RTI *rai = gf_smil_anim_get_anim_runtime_from_timing(rti);

	/* we do the accumulation only once and store the result in interpolated value */
	if (rti->cycle_number == rti->first_frozen) {
		rai->target_value_changed = 0;
		gf_smil_anim_compute_interpolation_value(rai, FIX_ONE);
		if (rai->target_value_changed) gf_smil_anim_apply_accumulate(rai);
	} 
	if (rai->anim_elt->anim->additive == SMIL_ADDITIVE_SUM) {
		svg_attributes_add(&rai->owner->presentation_value, &rai->interpolated_value, &rai->owner->presentation_value, 1);
	} else {
		svg_attributes_copy(&rai->owner->presentation_value, &rai->interpolated_value, 1);
	}
}

static void gf_smil_anim_restore(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	SVGElement *e = rti->timed_elt;
	SMIL_Anim_RTI *rai = gf_smil_anim_get_anim_runtime_from_timing(rti);
	svg_attributes_copy(&rai->owner->presentation_value, &rai->owner->saved_dom_value, 0);
	//gf_list_del_item(rai->owner->anims, rai);
}

static void gf_smil_anim_init_runtime_info(SVGElement *e)
{
	u32 i;
	GF_FieldInfo target_attribute;
	SMIL_AttributeAnimations *aa = NULL;
	SMIL_Anim_RTI *rai;

	if (e->anim->attributeName.name) {
		gf_node_get_field_by_name((GF_Node *)e->xlink->href.target, e->anim->attributeName.name, &target_attribute);
	} else {
		/* if attributeName is not specified it is a transform attribute */
		gf_node_get_field_by_name((GF_Node *)e->xlink->href.target, "transform", &target_attribute);
	}

	GF_SAFEALLOC(rai, sizeof(SMIL_Anim_RTI))
	rai->anim_elt = e;	
	rai->interpolated_value = target_attribute;
	rai->interpolated_value.far_ptr = svg_create_value_from_attributetype(target_attribute.fieldType, 0);
	rai->previous_key_index = -1;
	rai->previous_coef = -1;

	if (gf_node_get_tag((GF_Node *)e) == TAG_SVG_animateMotion) {
		SVGanimateMotionElement *am = (SVGanimateMotionElement *)e;
		rai->rotate = am->rotate.type;
		if (e->anim->to.type == 0 && e->anim->by.type == 0 && e->anim->values.type == 0) {
			rai->path = gf_path_new();
			if (gf_list_count(am->path.points)) {
				gf_path_init_from_svg(rai->path, am->path.commands, am->path.points);
				rai->path_iterator = gf_path_iterator_new(rai->path);
				rai->length = gf_path_iterator_get_length(rai->path_iterator);
			} else {
				u32 count;
				count = gf_list_count(((SVGElement *)e)->children);
				for (i = 0; i < count; i++) {
					GF_Node *child = gf_list_get(((SVGElement *)e)->children, i);
					if (gf_node_get_tag((GF_Node *)child) == TAG_SVG_mpath) {
						SVGmpathElement *mpath = (SVGmpathElement *)child;
						GF_Node *used_path = NULL;
						if (mpath->xlink && mpath->xlink->href.target) used_path = (GF_Node *)mpath->xlink->href.target;
						else if (mpath->xlink->href.iri) used_path= gf_sg_find_node_by_name(gf_node_get_graph((GF_Node *)mpath), mpath->xlink->href.iri);
						if (used_path && gf_node_get_tag(used_path) == TAG_SVG_path) {
							SVGpathElement *used_path_elt = (SVGpathElement *)used_path;
							gf_path_init_from_svg(rai->path, used_path_elt->d.commands, used_path_elt->d.points);
							rai->path_iterator = gf_path_iterator_new(rai->path);
							rai->length = gf_path_iterator_get_length(rai->path_iterator);
						}
						break;
					}
				}
			}
		}
	}

	/* for all animations, check if there is already one animation on this attribute,
	   if yes, get the list and append the new animation
	   if no, create a list and add the new animation. */
	for (i = 0; i < gf_node_animation_count((GF_Node *)e->xlink->href.target); i++) {
		aa = gf_node_animation_get((GF_Node *)e->xlink->href.target, i);
		if (aa->presentation_value.fieldIndex == target_attribute.fieldIndex) {
			gf_list_add(aa->anims, rai);
			break;
		}
		aa = NULL;
	}
	if (!aa) {
		GF_SAFEALLOC(aa, sizeof(SMIL_AttributeAnimations))

		/* Save the DOM value before any animation starts */
		aa->saved_dom_value = target_attribute;
		aa->saved_dom_value.far_ptr = svg_create_value_from_attributetype(target_attribute.fieldType, 0);
		svg_attributes_copy(&aa->saved_dom_value, &target_attribute, 0);

		/* Stores the pointer to the presentation value */
		aa->presentation_value = target_attribute;

		aa->anims = gf_list_new();
		gf_list_add(aa->anims, rai);
		gf_node_animation_add((GF_Node *)e->xlink->href.target, aa);
	}
	rai->owner = aa;

	e->timing->runtime->activation = gf_smil_anim_animate;
	e->timing->runtime->freeze = gf_smil_anim_freeze;
	e->timing->runtime->restore = gf_smil_anim_restore;
	gf_smil_anim_get_last_specified_value(rai);
}

static void gf_smil_anim_delete_runtime_info(SMIL_Anim_RTI *rai)
{
	gf_svg_delete_attribute_value(rai->interpolated_value.fieldType, rai->interpolated_value.far_ptr);
	if (rai->path) gf_path_del(rai->path);
	if (rai->path_iterator) gf_path_iterator_del(rai->path_iterator);
	free(rai);
}

void gf_smil_anim_delete_animations(SVGElement *e)
{
	u32 i, j;
	for (i = 0; i < gf_node_animation_count((GF_Node *)e); i ++) {
		SMIL_AttributeAnimations *aa = gf_node_animation_get((GF_Node *)e->xlink->href.target, i);
		gf_svg_delete_attribute_value(aa->saved_dom_value.fieldType, aa->saved_dom_value.far_ptr);
		for (j = 0; j < gf_list_count(aa->anims); j++) {
			SMIL_Anim_RTI *rai = gf_list_get(aa->anims, i);
			gf_smil_anim_delete_runtime_info(rai);
		}
		gf_list_del(aa->anims);
	}
	gf_node_animation_del((GF_Node *)e);
}

void gf_smil_anim_init_node(GF_Node *node)
{
	SVGElement *anim_elt = (SVGElement *)node;
	
	if (!anim_elt->xlink) return;
	if (anim_elt->xlink->href.type == SVG_IRI_IRI) {
		if (!anim_elt->xlink->href.iri) { 
			fprintf(stderr, "Error: IRI not initialized\n");
			return;
		} else {
			GF_Node *n;
			
			n = gf_sg_find_node_by_name(gf_node_get_graph(node), anim_elt->xlink->href.iri);
			if (n) {
				anim_elt->xlink->href.type = SVG_IRI_ELEMENTID;
				anim_elt->xlink->href.target = (SVGElement *)n;
			} else {
				return;
			}
		}
	} 

	if (!anim_elt->xlink->href.target) return;

	gf_smil_timing_init_runtime_info(anim_elt);
	gf_smil_anim_init_runtime_info(anim_elt);
}

/* TODO: update for elliptical arcs */		
void gf_path_init_from_svg(GF_Path *path, GF_List *commands, GF_List *points)
{
	u32 i, j, command_count, points_count;
	SVG_Point orig, ct_orig, ct_end, end, *tmp;
	command_count = gf_list_count(commands);
	points_count = gf_list_count(points);

	for (i=0, j=0; i<command_count; i++) {
		u8 *command = gf_list_get(commands, i);
		switch (*command) {
		case 0: /* Move To */
			tmp = gf_list_get(points, j);
			orig = *tmp;
			gf_path_add_move_to(path, orig.x, orig.y);
			j++;
			/*provision for nextCurveTo when no curve is specified:
				"If there is no previous command or if the previous command was not an C, c, S or s, 
				assume the first control point is coincident with the current point.
			*/
			ct_orig = orig;
			break;
		case 1: /* Line To */
			tmp = gf_list_get(points, j);
			end = *tmp;

			gf_path_add_line_to(path, end.x, end.y);
			j++;
			orig = end;
			/*cf above*/
			ct_orig = orig;
			break;
		case 2: /* Curve To */
			tmp = gf_list_get(points, j);
			ct_orig = *tmp;
			tmp = gf_list_get(points, j+1);
			ct_end = *tmp;
			tmp = gf_list_get(points, j+2);
			end = *tmp;
			gf_path_add_cubic_to(path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			ct_orig = ct_end;
			orig = end;
			j+=3;
			break;
		case 3: /* Next Curve To */
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			tmp = gf_list_get(points, j);
			ct_end = *tmp;
			tmp = gf_list_get(points, j+1);
			end = *tmp;
			gf_path_add_cubic_to(path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			ct_orig = ct_end;
			orig = end;
			j+=2;
			break;
		case 4: /* Quadratic Curve To */
			tmp = gf_list_get(points, j);
			ct_orig = *tmp;
			tmp = gf_list_get(points, j+1);
			end = *tmp;
			gf_path_add_quadratic_to(path, ct_orig.x, ct_orig.y, end.x, end.y);			
			orig = end;
			j+=2;
			break;
		case 5: /* Next Quadratic Curve To */
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			tmp = gf_list_get(points, j);
			end = *tmp;
			gf_path_add_quadratic_to(path, ct_orig.x, ct_orig.y, end.x, end.y);
			orig = end;
			j++;
			break;
		case 6: /* Close */
			gf_path_close(path);
			break;
		}
	}	
}
