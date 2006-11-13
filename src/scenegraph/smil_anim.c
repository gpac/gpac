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

GF_Err gf_node_animation_add(GF_Node *node, void *animation)
{
	if (!node || !animation) return GF_BAD_PARAM;
	if (!node->sgprivate->animations) node->sgprivate->animations = gf_list_new();
	return gf_list_add(node->sgprivate->animations, animation);
}

GF_Err gf_node_animation_del(GF_Node *node)
{
	if (!node || !node->sgprivate->animations) return GF_BAD_PARAM;
	gf_list_del(node->sgprivate->animations);
	node->sgprivate->animations = NULL;
	return GF_OK;
}

u32 gf_node_animation_count(GF_Node *node)
{
	if (!node || !node->sgprivate->animations) return 0;
	return gf_list_count(node->sgprivate->animations);
}

void *gf_node_animation_get(GF_Node *node, u32 i)
{
	if (!node || !node->sgprivate->animations) return 0;
	return gf_list_get(node->sgprivate->animations, i);
}

GF_Err gf_node_animation_rem(GF_Node *node, u32 i)
{
	if (!node || !node->sgprivate->animations) return GF_OK;
	return gf_list_rem(node->sgprivate->animations, i);
}

void gf_svg_attributes_resolve_unspecified(GF_FieldInfo *in, GF_FieldInfo *p, GF_FieldInfo *t)
{
	if (in->fieldType == 0) {
		if (p->fieldType == SVG_Matrix_datatype) {
			/* if the input value is not specified, and the presentation value is of type Matrix,
			   then we should use the default identity transform instead of the presentation value */
			*in = *t;
		} else {
			*in = *p;
		}
	}
}

/* 
	Replaces the far pointer of the attribute value with either:
     - the pointer to the value which is inherited (if inherited)
	 - otherwise no replacement, the attribute value can be used as is.
*/
void gf_svg_attributes_resolve_inherit(GF_FieldInfo *in, GF_FieldInfo *prop)
{
	if (gf_svg_is_inherit(in)) *in = *prop;
}

/* 
	Replaces the far pointer of the attribute value with either:
	 - the pointer to the value of the color attribute (if this attribute is set to currentColor) 
	 - otherwise no replacement, the attribute value can be used as is.
*/
void gf_svg_attributes_resolve_currentColor(GF_FieldInfo *in, GF_FieldInfo *current_color)
{
	if ((in->fieldType == SVG_Paint_datatype) && gf_svg_is_current_color(in)) {
		*in = *current_color;
	} 
}

/* The important steps in the animation is to resolve the inherit (and currentColor) values 
   to perform the interpolation. The function used for that is gf_svg_attributes_pointer_update */
static void gf_smil_anim_animate_using_values(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributes *anim = rai->anim_elt->anim;
	GF_List *values = anim->values.values;

	GF_FieldInfo value_info, value_info_next;
	u32 keyValueIndex;
	u32 nbValues;

	Fixed interval_duration;
	Fixed interpolation_coefficient;

	u32 real_calcMode;

	memset(&value_info, 0, sizeof(GF_FieldInfo));
	value_info.fieldType = anim->values.type;
	value_info.eventType = anim->values.transform_type;
	value_info_next = value_info;

	real_calcMode = (gf_svg_attribute_is_interpolatable(anim->values.type)?anim->calcMode:SMIL_CALCMODE_DISCRETE);

	nbValues = gf_list_count(values);
	if (nbValues == 1) {
		if (rai->previous_key_index != 0) {
			value_info.far_ptr = gf_list_get(values, 0);
			//gf_svg_attributes_resolve_currentColor(&value_info, &rai->owner->current_color_value);
			//gf_svg_attributes_resolve_inherit(&value_info, &rai->owner->parent_presentation_value);
			gf_svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
			rai->previous_key_index = 0;
			rai->target_value_changed = 1;
		}
		return;
	}

	if (gf_list_count(anim->keyTimes)) {
		u32 keyTimeIndex;
		Fixed keyTimeBefore = 0, keyTimeAfter=0; 
		u32 keyTimesCount = gf_list_count(anim->keyTimes);
		for (keyTimeIndex = rai->previous_keytime_index; keyTimeIndex<keyTimesCount; keyTimeIndex++) {
			Fixed *tm1, *t = (Fixed *)gf_list_get(anim->keyTimes, keyTimeIndex);
			if (normalized_simple_time < *t) {
				rai->previous_keytime_index = keyTimeIndex;
				tm1 = (Fixed *) gf_list_get(anim->keyTimes, keyTimeIndex-1);
				if (tm1) keyTimeBefore = *tm1; 
				else keyTimeBefore = 0;
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
		if (real_calcMode) {
			Fixed tmp = normalized_simple_time*nbValues;
			if (normalized_simple_time == FIX_ONE) {
				keyValueIndex = nbValues-1;
			} else {
				keyValueIndex = FIX2INT(gf_floor(tmp));
			}
			interpolation_coefficient = tmp - INT2FIX(keyValueIndex);
		} else {
			Fixed tmp = normalized_simple_time*(nbValues-1);
			if (normalized_simple_time == FIX_ONE) {
				keyValueIndex = nbValues-2;
			} else {
				keyValueIndex = FIX2INT(gf_floor(tmp));
			}
			interpolation_coefficient = tmp - INT2FIX(keyValueIndex);
		}
		//fprintf(stdout, "No KeyTimes: key index %d, coeff: %.2f\n", keyValueIndex, FIX2FLT(interpolation_coefficient));
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
	rai->previous_coef = interpolation_coefficient;
	rai->target_value_changed = 1;

	switch (real_calcMode) {
	case SMIL_CALCMODE_DISCRETE:
		value_info.far_ptr = gf_list_get(values, keyValueIndex);
		//gf_svg_attributes_resolve_currentColor(&value_info, &rai->owner->current_color_value);
		//gf_svg_attributes_resolve_inherit(&value_info, &rai->owner->parent_presentation_value);
		gf_svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
		break;
	case SMIL_CALCMODE_PACED:
		/* TODO: at the moment assume it is linear */
	case SMIL_CALCMODE_SPLINE:
		/* TODO: at the moment assume it is linear */
	case SMIL_CALCMODE_LINEAR:
		if (keyValueIndex == nbValues - 1) {
			value_info.far_ptr = gf_list_get(values, nbValues - 1);
			//gf_svg_attributes_resolve_currentColor(&value_info, &rai->owner->current_color_value);
			//gf_svg_attributes_resolve_inherit(&value_info, &rai->owner->parent_presentation_value);
			gf_svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
		} else {
			
			value_info.far_ptr = gf_list_get(values, keyValueIndex);
			if (gf_svg_attribute_is_interpolatable(anim->values.type)) {
				gf_svg_attributes_resolve_currentColor(&value_info, &rai->owner->current_color_value);			
				gf_svg_attributes_resolve_inherit(&value_info, &rai->owner->parent_presentation_value);
			}
			
			value_info_next.far_ptr = gf_list_get(values, keyValueIndex+1);			
			if (gf_svg_attribute_is_interpolatable(anim->values.type)) {
				gf_svg_attributes_resolve_currentColor(&value_info_next, &rai->owner->current_color_value);
				gf_svg_attributes_resolve_inherit(&value_info_next, &rai->owner->parent_presentation_value);
			}

			gf_svg_attributes_interpolate(&value_info, &value_info_next, &rai->interpolated_value, interpolation_coefficient, 1);
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
//	gf_svg_attributes_resolve_currentColor(&to_info, &rai->owner->current_color_value);
//	gf_svg_attributes_resolve_inherit(&to_info, &rai->owner->parent_presentation_value);

	gf_svg_attributes_copy(&rai->interpolated_value, &to_info, 0);
	rai->target_value_changed = 1;
}

static void gf_smil_anim_animate_from_to(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributes *anim = rai->anim_elt->anim;

	GF_FieldInfo from_info, to_info;
	
	if (rai->previous_coef == normalized_simple_time) return;

	rai->previous_coef = normalized_simple_time;

	from_info.fieldType = anim->from.type;
	from_info.eventType = anim->from.transform_type;
	from_info.far_ptr = anim->from.value;
	gf_svg_attributes_resolve_unspecified(&from_info, &rai->owner->presentation_value, &rai->default_transform_value);
	if (gf_svg_attribute_is_interpolatable(anim->from.type)) {
		gf_svg_attributes_resolve_currentColor(&from_info, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&from_info, &rai->owner->parent_presentation_value);
	}

	to_info.fieldType = anim->to.type;
	to_info.eventType = anim->to.transform_type;
	to_info.far_ptr = anim->to.value;
	gf_svg_attributes_resolve_unspecified(&to_info, &rai->owner->presentation_value, &rai->default_transform_value);
	if (gf_svg_attribute_is_interpolatable(anim->from.type)) {
		gf_svg_attributes_resolve_currentColor(&to_info, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&to_info, &rai->owner->parent_presentation_value);
	}

	switch (anim->calcMode) {
	case SMIL_CALCMODE_DISCRETE:
		{
			/* before half of the duration stay at 'from' and then switch to 'to' */
			s32 useFrom = (normalized_simple_time<=FIX_ONE/2);
			if (useFrom == rai->previous_key_index) return;
			gf_svg_attributes_copy(&rai->interpolated_value, (useFrom?&from_info:&to_info), 0);
			rai->previous_key_index = useFrom;
		}
		break;
	case SMIL_CALCMODE_SPLINE:
	case SMIL_CALCMODE_PACED:
	case SMIL_CALCMODE_LINEAR:
	default:
		gf_svg_attributes_interpolate(&from_info, &to_info, &rai->interpolated_value, normalized_simple_time, 1);
		break;
	}
	rai->target_value_changed = 1;
}

static void gf_smil_anim_animate_from_by(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributes *anim = rai->anim_elt->anim;
	GF_FieldInfo from_info, by_info;
	
	if (rai->previous_coef == normalized_simple_time) return;

	rai->previous_coef = normalized_simple_time;

	from_info.fieldType = anim->from.type;
	from_info.eventType = anim->from.transform_type;
	from_info.far_ptr = anim->from.value;
	gf_svg_attributes_resolve_unspecified(&from_info, &rai->owner->presentation_value, &rai->default_transform_value);
	if (gf_svg_attribute_is_interpolatable(from_info.fieldType)) {
		gf_svg_attributes_resolve_currentColor(&from_info, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&from_info, &rai->owner->parent_presentation_value);
	}

	by_info.fieldType = anim->by.type;
	by_info.eventType = anim->by.transform_type;
	by_info.far_ptr = anim->by.value;
	if (gf_svg_attribute_is_interpolatable(from_info.fieldType)) {
		gf_svg_attributes_resolve_currentColor(&by_info, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&by_info, &rai->owner->parent_presentation_value);
	}

	switch (anim->calcMode) {
	case SMIL_CALCMODE_DISCRETE:
		{
			/* before half of the duration stay at 'from' and then switch to 'to' */
			s32 useFrom = (normalized_simple_time<=FIX_ONE/2);
			if (useFrom == rai->previous_key_index) return;
			if (useFrom) {
				gf_svg_attributes_copy(&rai->interpolated_value, &from_info, 0);
			} else {
				gf_svg_attributes_muladd(FIX_ONE, &from_info, FIX_ONE, &by_info, &rai->interpolated_value, 0);
			}
			rai->previous_key_index = useFrom;
		}
		break;
	case SMIL_CALCMODE_SPLINE:
	case SMIL_CALCMODE_PACED:
	case SMIL_CALCMODE_LINEAR:
	default:
		gf_svg_attributes_muladd(FIX_ONE, &from_info, normalized_simple_time, &by_info, &rai->interpolated_value, 0);
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
		m->m[0] = FIX_ONE;
		m->m[1] = 0;
		m->m[3] = 0;
		m->m[4] = FIX_ONE;
	}
	return res;
}

static void gf_smil_anim_animate_using_path(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	Bool res = 0;
	res = gf_svg_compute_path_anim(rai, (GF_Matrix2D*)rai->interpolated_value.far_ptr, normalized_simple_time);
	if (res) rai->target_value_changed = 1;
}


static void gf_smil_anim_get_last_specified_value(SMIL_Anim_RTI *rai)
{
	SVGElement *e = rai->anim_elt;

	if (rai->path) {
		/*TODO CHECK WITH CYRIL !! */
//		if (!rai->last_specified_value.far_ptr) rai->last_specified_value.far_ptr = malloc(sizeof(GF_Matrix2D));
//		gf_svg_compute_path_anim(rai, rai->last_specified_value.far_ptr, FIX_ONE);
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
	if (gf_svg_is_inherit(&rai->last_specified_value)) {
		rai->last_specified_value.fieldType = rai->owner->presentation_value.fieldType;
		rai->last_specified_value.eventType = rai->owner->presentation_value.eventType;
		rai->last_specified_value.far_ptr = rai->owner->presentation_value.far_ptr;
	}
	if (gf_svg_attribute_is_interpolatable(rai->last_specified_value.fieldType)) {
		gf_svg_attributes_resolve_currentColor(&rai->last_specified_value, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&rai->last_specified_value, &rai->owner->parent_presentation_value);
	}
}

static SMIL_Anim_RTI *gf_smil_anim_get_anim_runtime_from_timing(SMIL_Timing_RTI *rti)
{
	SVGElement *e = rti->timed_elt;
	u32 i, j;
	if (!e->xlink->href.target) return NULL;
	for (i = 0; i < gf_node_animation_count((GF_Node *)e->xlink->href.target); i++) {
		SMIL_Anim_RTI *rai_tmp;
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get((GF_Node *)e->xlink->href.target, i);
		j=0;
		while ((rai_tmp = (SMIL_Anim_RTI *)gf_list_enum(aa->anims, &j))) {
			if (rai_tmp->anim_elt->timing->runtime == rti) {						
				return rai_tmp;
			}
		}
	}
	return NULL;
}

static void gf_smil_anim_compute_interpolation_value(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SVGElement *e = rai->anim_elt;

	if (rai->path) {
		gf_smil_anim_animate_using_path(rai, normalized_simple_time);
		return;
	} else if (gf_node_get_tag((GF_Node *)rai->anim_elt) == TAG_SVG_set) { 		
		gf_smil_anim_set(rai);
		return;
	}

	if (gf_list_count(e->anim->values.values)) {
		/* Ignore 'from'/'to'/'by'*/
		gf_smil_anim_animate_using_values(rai, normalized_simple_time);
	} else if (e->anim->by.type && (e->anim->to.type == 0)) {
		/* 'to' is not specified but 'by' is, so this is a 'by' animation or a 'from'-'by' animation */
		gf_smil_anim_animate_from_by(rai, normalized_simple_time);
	} else { 
		/* Ignore 'by' if specified */
		gf_smil_anim_animate_from_to(rai, normalized_simple_time);
	}
}

/* if the animation behavior is accumulative and this is not the first iteration,
   then we modify the interpolation value as follows:
    interpolation value += last specified value * number of iterations completed */
static void gf_smil_anim_apply_accumulate(SMIL_Anim_RTI *rai)
{
	u32 nb_iterations = (rai->anim_elt->timing->runtime->current_interval?rai->anim_elt->timing->runtime->current_interval->nb_iterations:1);
	if (rai->anim_elt->anim->accumulate == SMIL_ACCUMULATE_SUM && nb_iterations > 0) {
		gf_svg_attributes_muladd(FIX_ONE, &rai->interpolated_value, INT2FIX(nb_iterations), &rai->last_specified_value, &rai->interpolated_value, 1);
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
		gf_svg_attributes_add(&rai->owner->presentation_value, &rai->interpolated_value, &rai->owner->presentation_value, 1);
	} else {
		gf_svg_attributes_copy(&rai->owner->presentation_value, &rai->interpolated_value, 1);
	}
}

static void gf_smil_anim_animate_with_fraction(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	gf_smil_anim_animate(rti, rti->fraction);
	rti->evaluate_status = SMIL_TIMING_EVAL_NONE;
}

/* copy/paste of the animate function except for the optimization which consists in 
   not recomputing the interpolation value */
static void gf_smil_anim_freeze(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	SMIL_Anim_RTI *rai = gf_smil_anim_get_anim_runtime_from_timing(rti);
	if (!rai) return;

//	fprintf(stdout, "Freeze: %x @ time : %f\n", rti, normalized_simple_time);

	/* We do the accumulation only once and store the result in interpolated value */
	if (rti->cycle_number == rti->first_frozen) {
		rai->target_value_changed = 0;
		gf_smil_anim_compute_interpolation_value(rai, normalized_simple_time);
		if (rai->target_value_changed) gf_smil_anim_apply_accumulate(rai);
	} 
	/* 
		We still need to apply additive/replace behavior even when frozen 
		because we don't know how many other animations have run during this cycle,
		on this attribute, before the current one, which might have changed the underlying value. 
	*/
	if (rai->anim_elt->anim->additive == SMIL_ADDITIVE_SUM) {
		gf_svg_attributes_add(&rai->owner->presentation_value, &rai->interpolated_value, &rai->owner->presentation_value, 1);
	} else {
		gf_svg_attributes_copy(&rai->owner->presentation_value, &rai->interpolated_value, 1);
	}
}

static void gf_smil_anim_remove(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	SMIL_Anim_RTI *rai = gf_smil_anim_get_anim_runtime_from_timing(rti);
	if (!rai) return;

//	fprintf(stdout, "Restore: %x @ time : %f\n", rti, normalized_simple_time);

	gf_svg_attributes_copy(&rai->owner->presentation_value, &rai->owner->specified_value, 0);
	//gf_list_del_item(rai->owner->anims, rai);
}

static void gf_smil_anim_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_simple_time, u32 state)
{
	switch (state) {
	case SMIL_TIMING_EVAL_UPDATE: 
	case SMIL_TIMING_EVAL_RESTART:
		gf_smil_anim_animate(rti, normalized_simple_time);
		break;
	case SMIL_TIMING_EVAL_FREEZE: 
		gf_smil_anim_freeze(rti, normalized_simple_time);
		break;
	case SMIL_TIMING_EVAL_REMOVE: 
		gf_smil_anim_remove(rti, normalized_simple_time);
		break;
	case SMIL_TIMING_EVAL_FRACTION: 
		gf_smil_anim_animate_with_fraction(rti, normalized_simple_time);
		break;
/*
	discard should be done before in smil_notify_time
	case SMIL_TIMING_EVAL_DISCARD:
		break;
*/
	}
}

GF_EXPORT
void gf_svg_apply_animations(GF_Node *node, SVGPropertiesPointers *render_svg_props)
{
	u32 count_all, i;

	/*TODO FIXME - THIS IS WRONG, we're changing orders of animations which may corrupt the visual result*/
	/* Perform all the animations on this node */
	count_all = gf_node_animation_count(node);
	for (i = 0; i < count_all; i++) {
		/* Performing the animations for a given animated attribute */
		u32 j, count;
		
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get(node, i);		
		count = gf_list_count(aa->anims);
		if (!count) continue;
	
		/* Resetting the presentation value computed during the previous rendering cycle 
		   to the specified value */
		gf_svg_attributes_copy(&(aa->presentation_value), &(aa->specified_value), 0);

		/* Storing the pointer to the parent presentation value, 
		   i.e. the presentation value issued at the parent level in the tree */
		aa->parent_presentation_value = aa->presentation_value;
		aa->parent_presentation_value.far_ptr = gf_svg_get_property_pointer(render_svg_props, 
																			((SVGElement*)node)->properties,
																			aa->orig_dom_ptr);

		/* Storing also the pointer to the presentation value of the color property 
		   (special handling of the keyword 'currentColor' if used in animation values) */
		aa->current_color_value.fieldType = SVG_Paint_datatype;
		aa->current_color_value.far_ptr = &((SVGElement*)node)->properties->color;

		for (j = 0; j < count; j++) {
			SMIL_Anim_RTI *rai = (SMIL_Anim_RTI *)gf_list_get(aa->anims, j);			
			SMIL_Timing_RTI *rti = rai->anim_elt->timing->runtime;
			//Double scene_time = gf_node_get_scene_time(node);
			Double scene_time = rti->scene_time;

			if (rti->evaluate_status) {
				Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
				rti->evaluate(rti, simple_time, rti->evaluate_status);
			}
		}

		/*TODO FIXME, we need a finer granularity here 
		  and we must know if the animated attribute has changed or not (freeze)...*/
		gf_node_dirty_set(node, GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_SVG_APPEARANCE_DIRTY, 0);
	}

}

void gf_smil_anim_init_runtime_info(SVGElement *e)
{
	u32 i;
	GF_FieldInfo target_attribute;
	SMIL_AttributeAnimations *aa = NULL;
	SMIL_Anim_RTI *rai;

	if (e->anim->attributeName.name) {
		gf_node_get_field_by_name((GF_Node *)e->xlink->href.target, e->anim->attributeName.name, &target_attribute);
	} else {
		/* 
		All animation elements should have a target attribute except for animateMotion
		cf http://www.w3.org/mid/u403c21ajf1sjqtk58g0g38eaep9f9g2ss@hive.bjoern.hoehrmann.de
		"For animateMotion the attributeName is implied and cannot be specified; 
		animateTransform requires specification of the attribute name and any attribute that is
		a transform-like attribute can be a target, e.g. gradientTransform."
		*/
		if (gf_node_get_tag((GF_Node *)e) == TAG_SVG_animateMotion) {
			SVGTransformableElement *tr_e = (SVGTransformableElement *)e->xlink->href.target;
			/* 
			   Initialization of the pseudo attribute motionTransform, 
			   which holds the supplemental matrix until all animateMotion are done 
			*/
			if (!tr_e->motionTransform) {
				tr_e->motionTransform = (GF_Matrix2D*)malloc(sizeof(GF_Matrix2D));
				gf_mx2d_init(*tr_e->motionTransform);
			}
			gf_node_get_field_by_name((GF_Node *)tr_e, "motionTransform", &target_attribute);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[SVG] missing attributeName attribute on %s\n", e->sgprivate->NodeName));
			return;
		}
	}

	if (!gf_list_count(e->anim->values.values) && (e->anim->to.type == 0) && e->anim->by.type) {
		/* if this is a 'by' animation without from the animation is defined to be additive
		   see http://www.w3.org/TR/2005/REC-SMIL2-20051213/animation.html#AnimationNS-FromToBy
		   we override the additive attribute */
		e->anim->additive = SMIL_ADDITIVE_SUM;
	} 

	GF_SAFEALLOC(rai, SMIL_Anim_RTI)

	rai->anim_elt = e;	

	gf_mx2d_init(rai->identity);
	rai->default_transform_value.far_ptr = &rai->identity;
	rai->default_transform_value.fieldType = SVG_Matrix_datatype;

	rai->interpolated_value = target_attribute;
	rai->interpolated_value.far_ptr = gf_svg_create_attribute_value(target_attribute.fieldType, 0);
	rai->previous_key_index = -1;
	rai->previous_coef = -1;

	if (gf_node_get_tag((GF_Node *)e) == TAG_SVG_animateMotion) {
		SVGanimateMotionElement *am = (SVGanimateMotionElement *)e;

		rai->rotate = am->rotate.type;
		if (e->anim->to.type == 0 && e->anim->by.type == 0 && e->anim->values.type == 0) {
			rai->path = gf_path_new();
			if (gf_list_count(am->path.points)) {
				gf_svg_path_build(rai->path, am->path.commands, am->path.points);
				rai->path_iterator = gf_path_iterator_new(rai->path);
				rai->length = gf_path_iterator_get_length(rai->path_iterator);
			} else {
				u32 count;
				count = gf_list_count(((SVGElement *)e)->children);
				for (i = 0; i < count; i++) {
					GF_Node *child = (GF_Node *)gf_list_get(((SVGElement *)e)->children, i);
					if (gf_node_get_tag((GF_Node *)child) == TAG_SVG_mpath) {
						SVGmpathElement *mpath = (SVGmpathElement *)child;
						GF_Node *used_path = NULL;
						if (mpath->xlink && mpath->xlink->href.target) used_path = (GF_Node *)mpath->xlink->href.target;
						else if (mpath->xlink->href.iri) used_path = (GF_Node *)gf_sg_find_node_by_name(gf_node_get_graph((GF_Node *)mpath), mpath->xlink->href.iri);
						if (used_path && gf_node_get_tag(used_path) == TAG_SVG_path) {
							SVGpathElement *used_path_elt = (SVGpathElement *)used_path;
							gf_svg_path_build(rai->path, used_path_elt->d.commands, used_path_elt->d.points);
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
		aa = (SMIL_AttributeAnimations *)gf_node_animation_get((GF_Node *)e->xlink->href.target, i);
		if (aa->presentation_value.fieldIndex == target_attribute.fieldIndex) {
			gf_list_add(aa->anims, rai);
			break;
		}
		aa = NULL;
	}
	if (!aa) {
		GF_SAFEALLOC(aa, SMIL_AttributeAnimations)

		/* 
			Save the DOM specified value before any animation starts 
			We save also the memory address of the pointer (orig_dom_ptr)
		*/
		aa->specified_value = target_attribute;
		aa->orig_dom_ptr = aa->specified_value.far_ptr;
		aa->specified_value.far_ptr = gf_svg_create_attribute_value(target_attribute.fieldType, 0);
		gf_svg_attributes_copy(&aa->specified_value, &target_attribute, 0);

		/* Stores the pointer to the presentation value */
		aa->presentation_value = target_attribute;

		aa->anims = gf_list_new();
		gf_list_add(aa->anims, rai);
		gf_node_animation_add((GF_Node *)e->xlink->href.target, aa);
	}
	rai->owner = aa;

	e->timing->runtime->postpone = 1;
	e->timing->runtime->evaluate = gf_smil_anim_evaluate;
	gf_smil_anim_get_last_specified_value(rai);
}

void gf_smil_anim_delete_runtime_info(SMIL_Anim_RTI *rai)
{
	gf_svg_delete_attribute_value(rai->interpolated_value.fieldType, rai->interpolated_value.far_ptr, rai->anim_elt->sgprivate->scenegraph);
	if (rai->path) gf_path_del(rai->path);
	if (rai->path_iterator) gf_path_iterator_del(rai->path_iterator);
	free(rai);
}

void gf_smil_anim_remove_from_target(SVGElement *anim, SVGElement *target)
{
	u32 i, j;
	if (!target) return;
	for (i = 0; i < gf_node_animation_count((GF_Node *)target); i ++) {
		SMIL_Anim_RTI *rai;
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get((GF_Node *)target, i);
		j=0;
		while ((rai = (SMIL_Anim_RTI *)gf_list_enum(aa->anims, &j))) {
			if (rai->anim_elt == anim) {
				gf_list_rem(aa->anims, j-1);
				gf_smil_anim_delete_runtime_info(rai);
				break;
			}
		}
		if (gf_list_count(aa->anims) == 0) {
			gf_list_del(aa->anims);
			gf_svg_delete_attribute_value(aa->specified_value.fieldType, aa->specified_value.far_ptr, target->sgprivate->scenegraph);
			aa->presentation_value.far_ptr = aa->orig_dom_ptr;
			gf_node_animation_rem((GF_Node *)target, i);
			free(aa);
		}
	}
}

void gf_smil_anim_delete_animations(SVGElement *e)
{
	u32 i, j;
	for (i = 0; i < gf_node_animation_count((GF_Node *)e); i ++) {
		SMIL_Anim_RTI *rai;
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get((GF_Node *)e, i);
		gf_svg_delete_attribute_value(aa->specified_value.fieldType, aa->specified_value.far_ptr, e->sgprivate->scenegraph);
		j=0;
		while ((rai = (SMIL_Anim_RTI *)gf_list_enum(aa->anims, &j))) {
			gf_smil_anim_delete_runtime_info(rai);
		}
		gf_list_del(aa->anims);
		free(aa);
	}
	gf_node_animation_del((GF_Node *)e);
}

void gf_smil_anim_init_discard(GF_Node *node)
{
	SVGdiscardElement *discard = (SVGdiscardElement *)node;
	gf_smil_timing_init_runtime_info((SVGElement *)discard);
	discard->timing->runtime->evaluate_status = SMIL_TIMING_EVAL_DISCARD;
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
			
			n = (GF_Node*)gf_sg_find_node_by_name(gf_node_get_graph(node), anim_elt->xlink->href.iri);
			if (n) {
				anim_elt->xlink->href.type = SVG_IRI_ELEMENTID;
				anim_elt->xlink->href.target = (SVGElement *)n;
				gf_svg_register_iri(node->sgprivate->scenegraph, &anim_elt->xlink->href);
			} else {
				return;
			}
		}
	} 

	if (!anim_elt->xlink->href.target) return;
	gf_smil_timing_init_runtime_info(anim_elt);
	if (anim_elt->anim) {
		gf_smil_anim_init_runtime_info(anim_elt);	
	}
}

/* TODO: update for elliptical arcs */		
GF_EXPORT
void gf_svg_path_build(GF_Path *path, GF_List *commands, GF_List *points)
{
	u32 i, j, command_count, points_count;
	SVG_Point orig, ct_orig, ct_end, end, *tmp;
	command_count = gf_list_count(commands);
	points_count = gf_list_count(points);
	orig.x = orig.y = ct_orig.x = ct_orig.y = 0;

	for (i=0, j=0; i<command_count; i++) {
		u8 *command = (u8 *)gf_list_get(commands, i);
		switch (*command) {
		case SVG_PATHCOMMAND_M: /* Move To */
			tmp = (SVG_Point*)gf_list_get(points, j);
			orig = *tmp;
			gf_path_add_move_to(path, orig.x, orig.y);
			j++;
			/*provision for nextCurveTo when no curve is specified:
				"If there is no previous command or if the previous command was not an C, c, S or s, 
				assume the first control point is coincident with the current point.
			*/
			ct_orig = orig;
			break;
		case SVG_PATHCOMMAND_L: /* Line To */
			tmp = (SVG_Point*)gf_list_get(points, j);
			end = *tmp;

			gf_path_add_line_to(path, end.x, end.y);
			j++;
			orig = end;
			/*cf above*/
			ct_orig = orig;
			break;
		case SVG_PATHCOMMAND_C: /* Curve To */
			tmp = (SVG_Point*)gf_list_get(points, j);
			ct_orig = *tmp;
			tmp = (SVG_Point*)gf_list_get(points, j+1);
			ct_end = *tmp;
			tmp = (SVG_Point*)gf_list_get(points, j+2);
			end = *tmp;
			gf_path_add_cubic_to(path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			ct_orig = ct_end;
			orig = end;
			j+=3;
			break;
		case SVG_PATHCOMMAND_S: /* Next Curve To */
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			tmp = (SVG_Point*)gf_list_get(points, j);
			ct_end = *tmp;
			tmp = (SVG_Point*)gf_list_get(points, j+1);
			end = *tmp;
			gf_path_add_cubic_to(path, ct_orig.x, ct_orig.y, ct_end.x, ct_end.y, end.x, end.y);
			ct_orig = ct_end;
			orig = end;
			j+=2;
			break;
		case SVG_PATHCOMMAND_Q: /* Quadratic Curve To */
			tmp = (SVG_Point*)gf_list_get(points, j);
			ct_orig = *tmp;
			tmp = (SVG_Point*)gf_list_get(points, j+1);
			end = *tmp;
			gf_path_add_quadratic_to(path, ct_orig.x, ct_orig.y, end.x, end.y);			
			orig = end;
			j+=2;
			break;
		case SVG_PATHCOMMAND_T: /* Next Quadratic Curve To */
			ct_orig.x = 2*orig.x - ct_orig.x;
			ct_orig.y = 2*orig.y - ct_orig.y;
			tmp = (SVG_Point*)gf_list_get(points, j);
			end = *tmp;
			gf_path_add_quadratic_to(path, ct_orig.x, ct_orig.y, end.x, end.y);
			orig = end;
			j++;
			break;
		case SVG_PATHCOMMAND_Z: /* Close */
			gf_path_close(path);
			break;
		}
	}	
}
