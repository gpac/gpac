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

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_svg_sa.h>
#include <gpac/nodes_svg_sani.h>
#include <gpac/nodes_svg_da.h>

/**************************************************************************************
 * Each GF_Node holds the (SVG/SMIL) animation elements which target itself in a list *
 * The following are the generic functions to manipulate this list:					  *
 *  - add a new animation to the list,                                                *
 *  - get an animation from the list,                                                 *
 *  - remove an animation from the list,                                              *
 *  - count the animations in the list,                                               *
 *  - delete the list                                                                 *
 **************************************************************************************/
GF_Err gf_node_animation_add(GF_Node *node, void *animation)
{
	if (!node || !animation) return GF_BAD_PARAM;
	if (!node->sgprivate->interact) GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
	if (!node->sgprivate->interact->animations) node->sgprivate->interact->animations = gf_list_new();
	return gf_list_add(node->sgprivate->interact->animations, animation);
}

GF_Err gf_node_animation_del(GF_Node *node)
{
	if (!node || !node->sgprivate->interact || !node->sgprivate->interact->animations) return GF_BAD_PARAM;
	gf_list_del(node->sgprivate->interact->animations);
	node->sgprivate->interact->animations = NULL;
	return GF_OK;
}

u32 gf_node_animation_count(GF_Node *node)
{
	if (!node || !node->sgprivate->interact|| !node->sgprivate->interact->animations) return 0;
	return gf_list_count(node->sgprivate->interact->animations);
}

void *gf_node_animation_get(GF_Node *node, u32 i)
{
	if (!node || !node->sgprivate->interact || !node->sgprivate->interact->animations) return 0;
	return gf_list_get(node->sgprivate->interact->animations, i);
}

GF_Err gf_node_animation_rem(GF_Node *node, u32 i)
{
	if (!node || !node->sgprivate->interact || !node->sgprivate->interact->animations) return GF_OK;
	return gf_list_rem(node->sgprivate->interact->animations, i);
}
/**************************************************************************************
 * End of Generic GF_Node animations list                                             *
 **************************************************************************************/


/**************************************************************************************
 * Helping functions for animation                                                    *
 **************************************************************************************/
/* Sets the pointer to the attribute value with the pointer 
   to the value which passed (if unspecified) */
void gf_svg_attributes_resolve_unspecified(GF_FieldInfo *in, GF_FieldInfo *p, GF_FieldInfo *t)
{
	if (in->fieldType == 0) {
		if (p->fieldType == SVG_Transform_datatype) {
			/* if the input value is not specified, and the presentation value is of type Transform,
			   then we should use the default identity transform instead of the presentation value */
			*in = *t;
		} else {
			*in = *p;
		}
	}
}

/* Replaces the pointer to the attribute value with the pointer 
   to the value which is inherited (if inherited) */
void gf_svg_attributes_resolve_inherit(GF_FieldInfo *in, GF_FieldInfo *prop)
{
	if (gf_svg_is_inherit(in)) *in = *prop;
}

/* Replaces the pointer to the attribute value with the pointer 
   to the value of the color attribute (if the current value is set to currentColor) */
void gf_svg_attributes_resolve_currentColor(GF_FieldInfo *in, GF_FieldInfo *current_color)
{
	if ((in->fieldType == SVG_Paint_datatype) && gf_svg_is_current_color(in)) {
		*in = *current_color;
	} 
}

/**************************************************************************************
 * The main function doing evaluation of the animation is: gf_smil_anim_evaluate      *
 * Depending on the timing status of the animation it calls:                          *
 * - gf_smil_anim_animate                                                             *
 * - gf_smil_anim_animate_with_fraction												  *
 * - gf_smil_anim_freeze															  *
 * - gf_smil_anim_remove															  *
 *																					  *
 * The gf_smil_anim_animate consists in												  *
 * - interpolating using gf_smil_anim_compute_interpolation_value					  *
 * - accumulating using gf_smil_anim_apply_accumulate				                  *
 * - applying additive behavior                                                       *
 *																					  *
 * Depending on the animation attributes, one of the following functions is called    *
 * by the function gf_smil_anim_compute_interpolation_value                           *
 * - gf_smil_anim_set                                                                 *
 * - gf_smil_anim_animate_using_values                                                *
 * - gf_smil_anim_animate_from_to                                                     *
 * - gf_smil_anim_animate_from_by                                                     *
 * - gf_smil_anim_animate_using_path                                                  *
 *                                                                                    *
 * In most animation methods, the important step in the animation is to resolve       *
 *  the inherit and currentColor values to perform further interpolation, i.e. calls: *
 *	gf_svg_attributes_resolve_currentColor(&info, &rai->owner->current_color_value);  *
 *	gf_svg_attributes_resolve_inherit(&info, &rai->owner->parent_presentation_value); *
 *                                                                                    *
 **************************************************************************************/
static void gf_smil_anim_set(SMIL_Anim_RTI *rai)
{
	GF_FieldInfo to_info;
	SMILAnimationAttributesPointers *animp = rai->animp;
	if (!animp) return;
	if (!animp->to || !animp->to->type) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, 
		   ("[SMIL Animation] Time %f - Animation %s - applying set animation\n", 
		   gf_node_get_scene_time((GF_Node*)rai->anim_elt), 
		   gf_node_get_name((GF_Node *)rai->anim_elt)));

	to_info.fieldType = animp->to->type;
	to_info.far_ptr   = animp->to->value;
	/* we do not need to resolve currentColor values or inherit values here,
	   because no further interpolation is required for the animation and 
	   because inheritance is applied after animations in SVG_Render_base. */

	gf_svg_attributes_copy(&rai->interpolated_value, &to_info, 0);
	rai->interpolated_value_changed = 1;
}

static void gf_smil_anim_animate_using_values(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributesPointers *animp = rai->animp;
	GF_List *values = NULL;
	GF_FieldInfo value_info, value_info_next;
	u32 keyValueIndex;
	u32 nbValues;
	Fixed interval_duration;
	Fixed interpolation_coefficient;
	u32 real_calcMode;

	u32 tag = gf_node_get_tag(rai->anim_elt);

	if (!animp) return;

	values = animp->values->values;

	memset(&value_info, 0, sizeof(GF_FieldInfo));
	value_info.fieldType = animp->values->type;
	value_info_next = value_info;

	real_calcMode = (gf_svg_attribute_is_interpolatable(animp->values->type)?
						(animp->calcMode ? *animp->calcMode : SMIL_CALCMODE_LINEAR):
						SMIL_CALCMODE_DISCRETE
					);

	nbValues = gf_list_count(values);
	if (nbValues == 1) {
		if (rai->previous_key_index != 0) {
			value_info.far_ptr = gf_list_get(values, 0);
			/* no further interpolation needed 
			   therefore no need to resolve inherit and currentColor */
			gf_svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
			rai->previous_key_index = 0;
			rai->interpolated_value_changed = 1;
		}
		return;
	}

	/* Computing new key value index and interpolation coefficient */
	if (animp->keyTimes && gf_list_count(*animp->keyTimes)) {
		u32 keyTimeIndex;
		Fixed keyTimeBefore = 0, keyTimeAfter=0; 
		u32 keyTimesCount = gf_list_count(*animp->keyTimes);
		for (keyTimeIndex = rai->previous_keytime_index; keyTimeIndex<keyTimesCount; keyTimeIndex++) {
			Fixed *tm1, *t = (Fixed *)gf_list_get(*animp->keyTimes, keyTimeIndex);
			if (normalized_simple_time < *t) {
				rai->previous_keytime_index = keyTimeIndex;
				tm1 = (Fixed *) gf_list_get(*animp->keyTimes, keyTimeIndex-1);
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Anim] Using Key Times: index %d, interval duration %.2f, coeff: %.2f\n", keyTimeIndex, interval_duration, interpolation_coefficient));
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Anim] No KeyTimes: key index %d, coeff: %.2f\n", keyValueIndex, FIX2FLT(interpolation_coefficient)));
	}

	switch (tag) {
#ifdef GPAC_ENABLE_SVG_SA_BASE
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_animateMotion:
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_animateMotion:
#endif
	{
		SVG_SA_animateMotionElement *am = (SVG_SA_animateMotionElement *)rai->anim_elt;
		if (gf_list_count(am->keyPoints)) {
			interpolation_coefficient = *(Fixed *)gf_list_get(am->keyPoints, keyValueIndex);
			keyValueIndex = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Anim] Using Key Points: key Value Index %d, coeff: %.2f\n", keyValueIndex, interpolation_coefficient));
		}
	}
		break;
#endif
	case TAG_SVG_animateMotion:
		/* TODO */
		break;
	default:
		break;
	}

	if (rai->previous_key_index == (s32)keyValueIndex &&
		rai->previous_coef == interpolation_coefficient) return;

	rai->previous_key_index = keyValueIndex;
	rai->previous_coef = interpolation_coefficient;
	rai->interpolated_value_changed = 1;

	switch (real_calcMode) {
	case SMIL_CALCMODE_DISCRETE:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying discrete animation using values (key value index: %d)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt), keyValueIndex));
		value_info.far_ptr = gf_list_get(values, keyValueIndex);
		/* no further interpolation needed  
		   therefore no need to resolve inherit and currentColor */
		gf_svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
		break;
	case SMIL_CALCMODE_PACED:
		/* TODO: at the moment assume it is linear */
	case SMIL_CALCMODE_SPLINE:
		/* TODO: at the moment assume it is linear */
	case SMIL_CALCMODE_LINEAR:
		if (keyValueIndex == nbValues - 1) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying linear animation using values (setting last key value: %d)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt), keyValueIndex));
			value_info.far_ptr = gf_list_get(values, nbValues - 1);
			/* no further interpolation needed  
			   therefore no need to resolve inherit and currentColor */
			gf_svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
		} else {
			
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying linear animation using values (key value indices: %d)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt), keyValueIndex, keyValueIndex+1));
			value_info.far_ptr = gf_list_get(values, keyValueIndex);
			if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(animp->values->type)) {
				gf_svg_attributes_resolve_currentColor(&value_info, &rai->owner->current_color_value);			
				gf_svg_attributes_resolve_inherit(&value_info, &rai->owner->parent_presentation_value);
			}
			
			value_info_next.far_ptr = gf_list_get(values, keyValueIndex+1);			
			if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(animp->values->type)) {
				gf_svg_attributes_resolve_currentColor(&value_info_next, &rai->owner->current_color_value);
				gf_svg_attributes_resolve_inherit(&value_info_next, &rai->owner->parent_presentation_value);
			}

			gf_svg_attributes_interpolate(&value_info, &value_info_next, &rai->interpolated_value, interpolation_coefficient, 1);
		}
		break;
	}
}

static void gf_smil_anim_animate_from_to(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	GF_FieldInfo from_info, to_info;
	SMILAnimationAttributesPointers *animp = rai->animp;

	if (!animp) return;
	
	if (rai->previous_coef == normalized_simple_time) return;

	rai->previous_coef = normalized_simple_time;

	if (animp->from) {
		from_info.fieldType = animp->from->type;
		from_info.far_ptr = animp->from->value;
	} else {
		from_info.fieldType = 0;
	}
	gf_svg_attributes_resolve_unspecified(&from_info, &rai->owner->presentation_value, &rai->default_transform_value);
	if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(from_info.fieldType)) {
		gf_svg_attributes_resolve_currentColor(&from_info, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&from_info, &rai->owner->parent_presentation_value);
	}

	to_info.fieldType = animp->to->type;
	to_info.far_ptr = animp->to->value;
	gf_svg_attributes_resolve_unspecified(&to_info, &rai->owner->presentation_value, &rai->default_transform_value);
	if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(to_info.fieldType)) {
		gf_svg_attributes_resolve_currentColor(&to_info, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&to_info, &rai->owner->parent_presentation_value);
	}

	switch ((animp->calcMode ? *animp->calcMode : SMIL_CALCMODE_LINEAR)) {
	case SMIL_CALCMODE_DISCRETE:
		{
			/* before half of the duration stay at 'from' and then switch to 'to' */
			s32 useFrom = (normalized_simple_time<=FIX_ONE/2);
			if (useFrom == rai->previous_key_index) return;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying from-to animation (using %s value)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt), (useFrom?"from":"to")));
			gf_svg_attributes_copy(&rai->interpolated_value, (useFrom?&from_info:&to_info), 0);
			rai->previous_key_index = useFrom;
		}
		break;
	case SMIL_CALCMODE_SPLINE:
	case SMIL_CALCMODE_PACED:
	case SMIL_CALCMODE_LINEAR:
	default:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying from-to animation (linear interpolation, using coefficient %f)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt), normalized_simple_time));
		gf_svg_attributes_interpolate(&from_info, &to_info, &rai->interpolated_value, normalized_simple_time, 1);
		break;
	}
	rai->interpolated_value_changed = 1;
}

static void gf_smil_anim_animate_from_by(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	Fixed from_coef;
	GF_FieldInfo from_info, by_info;
	SMILAnimationAttributesPointers *animp = rai->animp;
	if (!animp) return;

	if (rai->previous_coef == normalized_simple_time) return;

	rai->previous_coef = normalized_simple_time;

	if (animp->from) {
		from_info.fieldType = animp->from->type;
		from_info.far_ptr = animp->from->value;
		from_coef = FIX_ONE;
	} else {
		from_info.fieldType = 0;
		/* this is a by animation only, then, it is always additive, 
		   we don't need the from value*/
		from_coef = 0; 
	}
	gf_svg_attributes_resolve_unspecified(&from_info, &rai->owner->presentation_value, &rai->default_transform_value);
	if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(from_info.fieldType)) {
		gf_svg_attributes_resolve_currentColor(&from_info, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&from_info, &rai->owner->parent_presentation_value);
	}

	if (animp->by) {
		by_info.fieldType = animp->by->type;
		by_info.far_ptr = animp->by->value;
	} else {
		by_info.fieldType = 0;
	}
	if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(from_info.fieldType)) {
		gf_svg_attributes_resolve_currentColor(&by_info, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&by_info, &rai->owner->parent_presentation_value);
	}

	switch ((animp->calcMode ? *animp->calcMode : SMIL_CALCMODE_LINEAR)) {
	case SMIL_CALCMODE_DISCRETE:
		{
			/* before half of the duration stay at 'from' and then switch to 'to' */
			s32 useFrom = (normalized_simple_time<=FIX_ONE/2);
			if (useFrom == rai->previous_key_index) return;
			if (useFrom) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying from-by animation (setting from)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
				gf_svg_attributes_muladd(from_coef, &from_info, 0, &by_info, &rai->interpolated_value, 0);
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying from-by animation (setting from+by)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
				gf_svg_attributes_muladd(from_coef, &from_info, FIX_ONE, &by_info, &rai->interpolated_value, 0);
			}
			rai->previous_key_index = useFrom;
		}
		break;
	case SMIL_CALCMODE_SPLINE:
	case SMIL_CALCMODE_PACED:
	case SMIL_CALCMODE_LINEAR:
	default:
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying from-by animation (linear interpolation between from and from+by, coef: %f)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt), normalized_simple_time));
		gf_svg_attributes_muladd(from_coef, &from_info, normalized_simple_time, &by_info, &rai->interpolated_value, 0);
		break;
	}
	rai->interpolated_value_changed = 1;
}

static Bool gf_svg_compute_path_anim(SMIL_Anim_RTI *rai, GF_Matrix2D *m, Fixed normalized_simple_time) 
{
	Bool res = 0;
	Fixed offset;
	offset = gf_mulfix(normalized_simple_time, rai->length);
	gf_mx2d_init(*m);
	res = gf_path_iterator_get_transform(rai->path_iterator, offset, 1, m, 1, 0);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Anim] Time: %f - Offset: %f, Position: %f, %f\n", normalized_simple_time, offset, ((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[2], ((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[5]));
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying path animation (coef: %f)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt), normalized_simple_time));
	res = gf_svg_compute_path_anim(rai, (GF_Matrix2D*)rai->interpolated_value.far_ptr, normalized_simple_time);
	if (res) rai->interpolated_value_changed = 1;
}

static void gf_smil_anim_compute_interpolation_value(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributesPointers *animp = rai->animp;
	u32 tag = gf_node_get_tag(rai->anim_elt);
	if (!animp) return;

	if (rai->path) {
		gf_smil_anim_animate_using_path(rai, normalized_simple_time);
	} 
#ifdef GPAC_ENABLE_SVG_SA
	else if (tag == TAG_SVG_SA_set) gf_smil_anim_set(rai);
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if (tag == TAG_SVG_SANI_set) gf_smil_anim_set(rai);
#endif
	else if (tag == TAG_SVG_set) gf_smil_anim_set(rai);
	
	else if (animp->values && gf_list_count(animp->values->values)) {
		/* Ignore 'from'/'to'/'by'*/
		gf_smil_anim_animate_using_values(rai, normalized_simple_time);
	} else if ((animp->by && animp->by->type) && (!animp->to || animp->to->type == 0)) {
		/* 'to' is not specified but 'by' is, so this is a 'by' animation or a 'from'-'by' animation */
		gf_smil_anim_animate_from_by(rai, normalized_simple_time);
	} else { 
		/* Ignore 'by' if specified */
		gf_smil_anim_animate_from_to(rai, normalized_simple_time);
	}
}

static SMIL_Anim_RTI *gf_smil_anim_get_anim_runtime_from_timing(SMIL_Timing_RTI *rti)
{
	GF_Node *n = rti->timed_elt;
	u32 i, j;
	GF_Node *target = NULL;

	u32 tag = gf_node_get_tag(n);
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		target = ((SVGTimedAnimBaseElement *)n)->xlinkp->href->target;
	}
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
		target = ((SVG_SA_Element *)n)->xlinkp->href->target;
	} 
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		target = ((SVG_SANI_Element *)n)->xlinkp->href->target;
	}
#endif

	if (!target) return NULL;

	for (i = 0; i < gf_node_animation_count(target); i++) {
		SMIL_Anim_RTI *rai_tmp;
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get(target, i);
		j=0;
		while ((rai_tmp = (SMIL_Anim_RTI *)gf_list_enum(aa->anims, &j))) {
			if (rai_tmp->timingp->runtime == rti) {
				return rai_tmp;
			}
		}
	}
	return NULL;
}

static void gf_smil_anim_get_last_specified_value(SMIL_Anim_RTI *rai)
{
	SMILAnimationAttributesPointers *animp = rai->animp;
	u32 tag = gf_node_get_tag(rai->anim_elt);

	if (!animp) return;

	if (rai->path) {
		/*TODO CHECK WITH CYRIL !! */
//		if (!rai->last_specified_value.far_ptr) rai->last_specified_value.far_ptr = malloc(sizeof(GF_Matrix2D));
//		gf_svg_compute_path_anim(rai, rai->last_specified_value.far_ptr, FIX_ONE);
		return;
	} else if ((tag == TAG_SVG_set)
#ifdef GPAC_ENABLE_SVG_SANI
			|| (tag == TAG_SVG_SA_set)
#endif
#ifdef GPAC_ENABLE_SVG_SANI
			|| (tag == TAG_SVG_SANI_set)
#endif
		) { 		
		if (animp->to) {
			rai->last_specified_value.fieldType = animp->to->type;
			rai->last_specified_value.far_ptr   = animp->to->value;
		} else {
			/* TODO ??? */
		} 
		return;
	}

	if (animp->values && gf_list_count(animp->values->values)) {
		/* Ignore from/to/by*/
		rai->last_specified_value.fieldType = animp->values->type;
		rai->last_specified_value.far_ptr = gf_list_last(animp->values->values);
	} else if ((animp->by && animp->by->type) && (!animp->to || animp->to->type == 0)) {
		rai->last_specified_value.fieldType = animp->by->type;
		rai->last_specified_value.far_ptr   = animp->by->value;
	} else { 
		rai->last_specified_value.fieldType = animp->to->type;
		rai->last_specified_value.far_ptr   = animp->to->value;
	}
	if (gf_svg_is_inherit(&rai->last_specified_value)) {
		rai->last_specified_value.fieldType = rai->owner->presentation_value.fieldType;
		rai->last_specified_value.far_ptr = rai->owner->presentation_value.far_ptr;
	}
	if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(rai->last_specified_value.fieldType)) {
		gf_svg_attributes_resolve_currentColor(&rai->last_specified_value, &rai->owner->current_color_value);
		gf_svg_attributes_resolve_inherit(&rai->last_specified_value, &rai->owner->parent_presentation_value);
	}
}

/* if the animation behavior is accumulative and this is not the first iteration,
   then we modify the interpolation value as follows:
    interpolation value += last specified value * number of iterations completed */
static void gf_smil_anim_apply_accumulate(SMIL_Anim_RTI *rai)
{
	u32 nb_iterations;
	
	SMILAnimationAttributesPointers *animp = rai->animp;
	SMILTimingAttributesPointers *timingp = rai->timingp;

	if (!animp || !timingp) return;

	nb_iterations = (timingp->runtime->current_interval ? timingp->runtime->current_interval->nb_iterations : 1);

	if ((animp->accumulate && *animp->accumulate == SMIL_ACCUMULATE_SUM) && nb_iterations > 0) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying accumulation (iteration #%d)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt), nb_iterations));
		gf_svg_attributes_muladd(FIX_ONE, &rai->interpolated_value, INT2FIX(nb_iterations), &rai->last_specified_value, &rai->interpolated_value, 1);
		rai->interpolated_value_changed = 1;
	} 
}

static void gf_smil_anim_animate(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	SMIL_Anim_RTI *rai = gf_smil_anim_get_anim_runtime_from_timing(rti);
	SMILAnimationAttributesPointers *animp = rai->animp;

	if (!rai || !animp) return;

	rai->interpolated_value_changed = 0;

	gf_smil_anim_compute_interpolation_value(rai, normalized_simple_time);
	
	if (rai->interpolated_value_changed) gf_smil_anim_apply_accumulate(rai);
	
	/* Apply additive behavior if required
		PV = (additive == sum ? PV + animp->IV : animp->IV); */
	if (animp->additive && *animp->additive == SMIL_ADDITIVE_SUM) {
		/* if the additive behavior is on, any change to either the base value or the interpolated value
		   requires changing the presentation value */
		if (rai->owner->presentation_value_changed || rai->interpolated_value_changed) {		
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying additive behavior\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
			gf_svg_attributes_add(&rai->owner->presentation_value, &rai->interpolated_value, &rai->owner->presentation_value, 1);
			rai->owner->presentation_value_changed = 1;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying additive behavior (nothing to be done)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
		}
	} else {
		/* if the additive behavior is off, and the interpolation value has not changed 
		   the presentation value does not need to be changed */
		if (rai->interpolated_value_changed) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying non-additive behavior\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
			gf_svg_attributes_copy(&rai->owner->presentation_value, &rai->interpolated_value, 1);
			rai->owner->presentation_value_changed = 1;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying non-additive behavior (nothing to be done)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
			rai->owner->presentation_value_changed = 0;
		}
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
	SMILAnimationAttributesPointers *animp = rai->animp;
	if (!rai || !animp) return;

	/* We do the accumulation only once and store the result in interpolated value */
	if (rti->cycle_number == rti->first_frozen) {
		rai->interpolated_value_changed = 0;
		gf_smil_anim_compute_interpolation_value(rai, normalized_simple_time);
		if (rai->interpolated_value_changed) gf_smil_anim_apply_accumulate(rai);
	} 
	/* we still need to apply additive/replace behavior even when frozen 
	   because we don't know how many other animations have run during this cycle,
	   on this attribute, before the current one, which might have changed the underlying value. */
	if (animp->additive && *animp->additive == SMIL_ADDITIVE_SUM) {
		if (rai->owner->presentation_value_changed || rai->interpolated_value_changed) {		
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying additive freeze behavior\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
			gf_svg_attributes_add(&rai->owner->presentation_value, &rai->interpolated_value, &rai->owner->presentation_value, 1);
			rai->owner->presentation_value_changed = 1;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying additive freeze behavior (nothing done)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
		}
	} else {
		if (rai->interpolated_value_changed) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying freeze behavior\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
			gf_svg_attributes_copy(&rai->owner->presentation_value, &rai->interpolated_value, 1);
			rai->owner->presentation_value_changed = 1;
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying freeze behavior (nothing done)\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));			
			rai->owner->presentation_value_changed = 0;
		}
	}
}

static void gf_smil_anim_remove(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	SMIL_Anim_RTI *rai = gf_smil_anim_get_anim_runtime_from_timing(rti);
	if (!rai) return;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Time %f - Animation %s - applying remove behavior\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_name((GF_Node *)rai->anim_elt)));
	gf_svg_attributes_copy(&rai->owner->presentation_value, &rai->owner->specified_value, 0);
	rai->owner->presentation_value_changed = 1;
}

static void gf_smil_anim_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_simple_time, u32 state)
{
	switch (state) {
	case SMIL_TIMING_EVAL_UPDATE: 
	case SMIL_TIMING_EVAL_REPEAT:
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
/**************************************************************************************
 **************************************************************************************/

GF_EXPORT
void gf_svg_apply_animations(GF_Node *node, SVGPropertiesPointers *render_svg_props)
{
	u32 count_all, i;

	/*TODO FIXME - THIS IS WRONG, we're changing orders of animations which may corrupt the visual result*/
	/* Perform all the animations on this node */
	count_all = gf_node_animation_count(node);
	for (i = 0; i < count_all; i++) {
		u32 j, count;
		
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get(node, i);		
		count = gf_list_count(aa->anims);
		if (!count) continue;
	
		/* For the given animated attribute, resetting the presentation value,
		   computed during the previous rendering cycle, to the specified value,
		   this is needed because if the first animation is additive, 
		   it will add to the specified value, not to the previously animated value	*/
		gf_svg_attributes_copy(&(aa->presentation_value), &(aa->specified_value), 0);
		aa->presentation_value_changed = 0;

		if (aa->is_property) {
			/* Storing the pointer to the parent presentation value, 
			   i.e. the presentation value issued at the parent level in the tree */
			aa->parent_presentation_value = aa->presentation_value;
			if ((node->sgprivate->tag>=GF_NODE_RANGE_FIRST_SVG) && (node->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG)) {
				aa->parent_presentation_value.far_ptr = gf_svg_get_property_pointer((SVG_Element *)node, aa->orig_dom_ptr, render_svg_props); 
			} 
#ifdef GPAC_ENABLE_SVG_SA
			else if ((node->sgprivate->tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (node->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
				aa->parent_presentation_value.far_ptr = gf_svg_sa_get_property_pointer(render_svg_props, 
																					((SVG_SA_Element*)node)->properties,
																					aa->orig_dom_ptr);
			}
#endif
			/* Storing also the pointer to the presentation value of the color property 
			   (special handling of the keyword 'currentColor' if used in animation values) */
			aa->current_color_value.fieldType = SVG_Paint_datatype;
			if ((node->sgprivate->tag>=GF_NODE_RANGE_FIRST_SVG) && (node->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG)) {
				GF_FieldInfo info;
				gf_svg_get_attribute_by_tag(node, TAG_SVG_ATT_color, 1, 1, &info);
				aa->current_color_value.far_ptr = info.far_ptr;
			} 
#ifdef GPAC_ENABLE_SVG_SA
			else if ((node->sgprivate->tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (node->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
				aa->current_color_value.far_ptr = &((SVG_SA_Element*)node)->properties->color;
			} 
#endif
		}

		/* Performing all the animations targetting the given attribute */
		for (j = 0; j < count; j++) {
			Double scene_time;
			SMIL_Anim_RTI *rai = (SMIL_Anim_RTI *)gf_list_get(aa->anims, j);			
			SMIL_Timing_RTI *rti = rai->timingp->runtime;

			//scene_time = gf_node_get_scene_time(node);
			scene_time = rti->scene_time;

			if (rti->evaluate_status) {
				Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
				rti->evaluate(rti, simple_time, rti->evaluate_status);
			}
		}
		if (aa->presentation_value_changed) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Animation] Presentation value changed\n"));
			gf_node_dirty_set(node, aa->dirty_flags, 0);
		}
	}

}

#ifdef GPAC_ENABLE_SVG_SANI

GF_EXPORT
void gf_svg_sani_apply_animations(GF_Node *node)
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
		aa->presentation_value_changed = 0;

		for (j = 0; j < count; j++) {
			SMIL_Anim_RTI *rai = (SMIL_Anim_RTI *)gf_list_get(aa->anims, j);			
			SMIL_Timing_RTI *rti = ((SVG_SANI_Element *)rai->anim_elt)->timingp->runtime;
			//Double scene_time = gf_node_get_scene_time(node);
			Double scene_time = rti->scene_time;

			if (rti->evaluate_status) {
				Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
				rti->evaluate(rti, simple_time, rti->evaluate_status);
			}
		}

		if (aa->presentation_value_changed) {
			gf_node_dirty_set(node, aa->dirty_flags, 0);
		}
	}
}
#else
GF_EXPORT
void gf_svg_sani_apply_animations(GF_Node *node)
{
}
#endif


void gf_smil_anim_init_runtime_info(GF_Node *e)
{
	u32 i;
	GF_FieldInfo target_attribute;
	SMIL_AttributeAnimations *aa = NULL;
	SMIL_Anim_RTI *rai;
	XLinkAttributesPointers *xlinkp = NULL;
	SMILAnimationAttributesPointers *animp = NULL;
	SMILTimingAttributesPointers *timingp = NULL;
	GF_Node *target = NULL;
	u32 tag;

	/* Filling animation structures to be independent of the SVG Element structure */
	tag = gf_node_get_tag(e);
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		animp = ((SVGTimedAnimBaseElement *)e)->animp;
		timingp = ((SVGTimedAnimBaseElement *)e)->timingp;
		xlinkp = ((SVGTimedAnimBaseElement *)e)->xlinkp;
	} 
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
		animp = ((SVG_SA_Element *)e)->animp;
		timingp = ((SVG_SA_Element *)e)->timingp;
		xlinkp = ((SVG_SA_Element *)e)->xlinkp;
	} 
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		animp = ((SVG_SANI_Element *)e)->animp;
		timingp = ((SVG_SANI_Element *)e)->timingp;
		xlinkp = ((SVG_SANI_Element *)e)->xlinkp;
	} 
#endif
	else {
		return;
	}
	/* from this point, the animation node 'e' should not be used */

	target = xlinkp->href->target;

	memset(&target_attribute, 0, sizeof(GF_FieldInfo));
	if (animp->attributeName && animp->attributeName->name) {
		/* Filling the target_attribute structure with info on the animated attribute (type, pointer to data, ...)
		NOTE: in the mode Dynamic Allocation of Attributes, this means that the animated 
		attribute is created with a default value, if it was not specified on the target element */
		gf_node_get_field_by_name(target, animp->attributeName->name, &target_attribute);
	} else {
		/* All animation elements should have a target attribute except for animateMotion
		cf http://www.w3.org/mid/u403c21ajf1sjqtk58g0g38eaep9f9g2ss@hive.bjoern.hoehrmann.de
		"For animateMotion, the attributeName is implied and cannot be specified; 
		animateTransform requires specification of the attribute name and any attribute that is
		a transform-like attribute can be a target, e.g. gradientTransform."*/

		switch (tag) {
#ifdef GPAC_ENABLE_SVG_SA_BASE
#ifdef GPAC_ENABLE_SVG_SA
		case TAG_SVG_SA_animateMotion:
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		case TAG_SVG_SANI_animateMotion:
#endif
		{
			SVGTransformableElement *tr_e = (SVGTransformableElement *)target;
			if (!tr_e->motionTransform) {
				tr_e->motionTransform = (GF_Matrix2D*)malloc(sizeof(GF_Matrix2D));
				gf_mx2d_init(*tr_e->motionTransform);
			}
			gf_node_get_field_by_name((GF_Node *)tr_e, "motionTransform", &target_attribute);
		}
			break;
#endif
		case TAG_SVG_animateMotion:
			/* Explicit creation of the pseudo 'motionTransform' attribute since it cannot be specified */
			gf_svg_get_attribute_by_tag(target, TAG_SVG_ATT_motionTransform, 1, 0, &target_attribute);
			gf_mx2d_init(*(GF_Matrix2D *)target_attribute.far_ptr);
			break;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[SMIL Animation] Missing attributeName attribute on element %s\n", gf_node_get_name((GF_Node*)e) ));
			return;
		}
	}

	if ((!animp->values || !gf_list_count(animp->values->values)) && /* 'values' attribute not specified */
		(!animp->to || animp->to->type == 0) &&						 /* 'to' attribute not specified */
		(!animp->from || animp->from->type == 0) &&					 /* 'from' attribute not specified */
		(animp->by && animp->by->type != 0)) {						 /* 'by' attribute specified */		
		/* if this is a 'by' animation without from the animation is defined to be additive
		   see http://www.w3.org/TR/2005/REC-SMIL2-20051213/animation.html#AnimationNS-FromToBy
		   we override the additive attribute */
		if (!animp->additive) { 
			/* this case can only happen with dynamic allocation of attributes */
			GF_FieldInfo info;
			gf_svg_get_attribute_by_tag(e, TAG_SVG_ATT_additive, 1, 0, &info);
			animp->additive = info.far_ptr;
		}
		*animp->additive = SMIL_ADDITIVE_SUM;
	} 

	/* Creation and setup of the runtime structure for animation */
	GF_SAFEALLOC(rai, SMIL_Anim_RTI)

	rai->anim_elt = e;	
	rai->animp = animp;
	rai->timingp = timingp;
	rai->xlinkp = xlinkp;

	gf_mx2d_init(rai->identity);
	rai->default_transform_value.far_ptr = &rai->identity;
	rai->default_transform_value.fieldType = SVG_Transform_datatype;

	/* the interpolated value has the same type as the target attribute,
	   but we need to create a new pointer to hold its value */
	rai->interpolated_value = target_attribute;
	rai->interpolated_value.far_ptr = gf_svg_create_attribute_value(target_attribute.fieldType);

	/* there has not been any interpolation yet, so the previous key index and interpolation coefficient
	   shall not be set*/
	rai->previous_key_index = -1;
	rai->previous_coef = -1;

	/* For animateMotion, we need to retrieve the value of the rotate attribute, retrieve the path either
	from the 'path' attribute or from the 'mpath' element, and then initialize the path iterator*/
	if ((tag == TAG_SVG_animateMotion)
#ifdef GPAC_ENABLE_SVG_SA
		|| (tag == TAG_SVG_SA_animateMotion)
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		|| (tag == TAG_SVG_SANI_animateMotion)
#endif
		) {
		GF_Path *the_path = NULL;
		GF_ChildNodeItem *child = NULL;

#ifdef GPAC_ENABLE_SVG_SA
		if (tag == TAG_SVG_SA_animateMotion) {
			SVG_SA_animateMotionElement *am = (SVG_SA_animateMotionElement *)e;
			rai->rotate = am->rotate.type;
			the_path = &am->path;
			child = am->children;
		} else 
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		if (tag == TAG_SVG_SANI_animateMotion) {
			SVG_SANI_animateMotionElement *am = (SVG_SANI_animateMotionElement *)e;
			rai->rotate = am->rotate.type;
			the_path = &am->path;
			child = am->children;
		} else 
#endif
		{
			GF_FieldInfo info;
			if (gf_svg_get_attribute_by_tag(e, TAG_SVG_ATT_rotate, 0, 0, &info) == GF_OK) {
				rai->rotate = ((SVG_Rotate *)info.far_ptr)->type;
			} else {
				rai->rotate = SVG_NUMBER_VALUE;
			}
			if (gf_svg_get_attribute_by_tag(e, TAG_SVG_ATT_path, 0, 0, &info) == GF_OK) {
				the_path = ((SVG_PathData *)info.far_ptr);
			}
			child = ((SVG_Element *)e)->children;
		}

		if ((!animp->to || animp->to->type == 0) && 
			(!animp->by || animp->by->type == 0) && 
			(!animp->values || animp->values->type == 0)) {
#if USE_GF_PATH
			if (!gf_path_is_empty(the_path)) {
				rai->path = the_path;
				rai->path_iterator = gf_path_iterator_new(rai->path);
				rai->length = gf_path_iterator_get_length(rai->path_iterator);
			} 
#else
			rai->path = gf_path_new();
			if (gf_list_count(the_path->points)) {
				gf_svg_path_build(rai->path, the_path->commands, the_path->points);
				rai->path_iterator = gf_path_iterator_new(rai->path);
				rai->length = gf_path_iterator_get_length(rai->path_iterator);
			} 
#endif
			else {
				while (child) {
					GF_Node *used_path = NULL;
					u32 child_tag = gf_node_get_tag(child->node);
#ifdef GPAC_ENABLE_SVG_SA
					if (child_tag == TAG_SVG_SA_mpath) {
						SVG_SA_mpathElement *mpath = (SVG_SA_mpathElement *)child->node;
						if (mpath->xlink->href.target) used_path = (GF_Node *)mpath->xlink->href.target;
						else if (mpath->xlink->href.iri) used_path = (GF_Node *)gf_sg_find_node_by_name(gf_node_get_graph((GF_Node *)mpath), mpath->xlink->href.iri);
						if (used_path && gf_node_get_tag(used_path) == TAG_SVG_SA_path) {
							SVG_SA_pathElement *used_path_elt = (SVG_SA_pathElement *)used_path;
#if USE_GF_PATH
							rai->path = &used_path_elt->d;
#else
							gf_svg_path_build(rai->path, used_path_elt->d.commands, used_path_elt->d.points);
#endif
							rai->path_iterator = gf_path_iterator_new(rai->path);
							rai->length = gf_path_iterator_get_length(rai->path_iterator);
						}
						break;
					} else 
#endif
					if (child_tag == TAG_SVG_mpath) {
						GF_FieldInfo info;
						SVG_Element *mpath = (SVG_Element *)child->node;
						if (gf_svg_get_attribute_by_tag(child->node, TAG_SVG_ATT_xlink_href, 0, 0, &info) == GF_OK) {
							SVG_IRI *iri = (SVG_IRI *)info.far_ptr;
							if (iri->target) used_path = iri->target;
							else if (iri->iri) used_path = (GF_Node *)gf_sg_find_node_by_name(gf_node_get_graph(child->node), iri->iri);
							if (used_path && gf_node_get_tag(used_path) == TAG_SVG_path) {
								gf_svg_get_attribute_by_tag(used_path, TAG_SVG_ATT_d, 1, 0, &info);
#if USE_GF_PATH
								rai->path = (SVG_PathData *)info.far_ptr;
#else
								gf_svg_path_build(rai->path, ((SVG_PathData *)info.far_ptr)->commands, ((SVG_PathData *)info.far_ptr)->points);
#endif
								rai->path_iterator = gf_path_iterator_new(rai->path);
								rai->length = gf_path_iterator_get_length(rai->path_iterator);
							}
						} 
						break;
					}
					child = child->next;
				}
			}
		} 
	}

	/* for all animations, check if there is already one animation on this attribute,
	   if yes, get the list and append the new animation runtime info
	   if no, create a list and add the new animation runtime info. */
	for (i = 0; i < gf_node_animation_count(target); i++) {
		aa = (SMIL_AttributeAnimations *)gf_node_animation_get(target, i);
		if (aa->presentation_value.fieldIndex == target_attribute.fieldIndex) {
			gf_list_add(aa->anims, rai);
			break;
		}
		aa = NULL;
	}
	if (!aa) {
		GF_SAFEALLOC(aa, SMIL_AttributeAnimations)

		/* We copy (one copy for all animations on the same attribute) the DOM specified 
		   value before any animation starts (because the animation will override it), 
		   we also save the initial memory address of the specified value (orig_dom_ptr)
		   for inheritance hack */
		aa->specified_value = target_attribute;
		aa->is_property = gf_svg_is_property(target, &target_attribute);
		aa->orig_dom_ptr = aa->specified_value.far_ptr;
		aa->specified_value.far_ptr = gf_svg_create_attribute_value(target_attribute.fieldType);
		gf_svg_attributes_copy(&aa->specified_value, &target_attribute, 0);

		/* Now, the initial memory address for the specified value hold the presentation value,
		   and the presentation value is initialized */
		aa->presentation_value = target_attribute;
		
		aa->anims = gf_list_new();
		gf_list_add(aa->anims, rai);
		gf_node_animation_add(target, aa);

		/* determine what the rendering will need to do when the animation runs */
		if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
			aa->dirty_flags = gf_svg_get_rendering_flag_if_modified((SVG_Element *)target, &target_attribute);
		}
#ifdef GPAC_ENABLE_SVG_SA
		else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
			aa->dirty_flags = gf_svg_sa_get_rendering_flag_if_modified((SVG_SA_Element *)target, &target_attribute);
		} 
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
			aa->dirty_flags = gf_svg_sani_get_rendering_flag_if_modified((SVG_SANI_Element *)target, &target_attribute);
		}
#endif
	}
	rai->owner = aa;
	gf_smil_anim_get_last_specified_value(rai);

	/* for animation (unlike other timed elements like video), the interpolation cannot be done 
	during timing evaluation, because due to inheritance, interpolation can only be computed 
	during scene tree traversal, therefore we need to postpone evaluation of the timed element */
	timingp->runtime->postpone = 1;
	timingp->runtime->evaluate = gf_smil_anim_evaluate;
}

void gf_smil_anim_delete_runtime_info(SMIL_Anim_RTI *rai)
{
	gf_svg_delete_attribute_value(rai->interpolated_value.fieldType, 
								  rai->interpolated_value.far_ptr, 
								  rai->anim_elt->sgprivate->scenegraph);
#if USE_GF_PATH	
#else
	if (rai->path) gf_path_del(rai->path);
#endif
	if (rai->path_iterator) gf_path_iterator_del(rai->path_iterator);
	free(rai);
}

void gf_smil_anim_remove_from_target(GF_Node *anim, GF_Node *target)
{
	u32 i, j;
	if (!target) return;
	for (i = 0; i < gf_node_animation_count((GF_Node *)target); i ++) {
		SMIL_Anim_RTI *rai;
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get((GF_Node *)target, i);
		j=0;
		while ((rai = (SMIL_Anim_RTI *)gf_list_enum(aa->anims, &j))) {
			if ((GF_Node *)rai->anim_elt == anim) {
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

void gf_smil_anim_delete_animations(GF_Node *e)
{
	u32 i, j;

	for (i = 0; i < gf_node_animation_count(e); i ++) {
		SMIL_Anim_RTI *rai;
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get(e, i);
		gf_svg_delete_attribute_value(aa->specified_value.fieldType, aa->specified_value.far_ptr, e->sgprivate->scenegraph);
		j=0;
		while ((rai = (SMIL_Anim_RTI *)gf_list_enum(aa->anims, &j))) {
			rai->xlinkp->href->target = NULL;
			gf_smil_anim_delete_runtime_info(rai);
		}		
		gf_list_del(aa->anims);
		free(aa);
	}
	gf_node_animation_del(e);
}

void gf_smil_anim_init_discard(GF_Node *node)
{
	u32 tag = gf_node_get_tag(node);
	gf_smil_timing_init_runtime_info(node);
	
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		((SVGTimedAnimBaseElement *)node)->timingp->runtime->evaluate_status = SMIL_TIMING_EVAL_DISCARD;
	}
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
		((SVG_SA_Element *)node)->timingp->runtime->evaluate_status = SMIL_TIMING_EVAL_DISCARD;
	}
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		((SVG_SANI_Element *)node)->timingp->runtime->evaluate_status = SMIL_TIMING_EVAL_DISCARD;
	}
#endif
}

void gf_smil_anim_init_node(GF_Node *node)
{
	XLinkAttributesPointers *xlinkp = NULL;
	SMILAnimationAttributesPointers *animp = NULL;

	u32 tag = gf_node_get_tag(node);
	
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		SVGAllAttributes all_atts;
		SVGTimedAnimBaseElement *e = (SVGTimedAnimBaseElement *)node;
		gf_svg_flatten_attributes((SVG_Element *)e, &all_atts);
		e->xlinkp = malloc(sizeof(XLinkAttributesPointers));
		xlinkp = e->xlinkp;
		xlinkp->href = all_atts.xlink_href;
		xlinkp->type = all_atts.xlink_type;		

		e->animp = malloc(sizeof(SMILAnimationAttributesPointers));
		animp = e->animp;
		animp->accumulate	 = all_atts.accumulate;
		animp->additive		 = all_atts.additive;
		animp->attributeName = all_atts.attributeName;
		animp->attributeType = all_atts.attributeType;
		animp->by			 = all_atts.by;
		animp->calcMode		 = all_atts.calcMode;
		animp->from			 = all_atts.from;
		animp->keySplines	 = all_atts.keySplines;
		animp->keyTimes		 = all_atts.keyTimes;
		animp->lsr_enabled	 = all_atts.lsr_enabled;
		animp->to			 = all_atts.to;
		animp->type			 = all_atts.type;
		animp->values		 = all_atts.values;
	} 
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
		SVG_SA_Element *e = (SVG_SA_Element *)node;
		e->xlinkp = malloc(sizeof(XLinkAttributesPointers));
		e->xlinkp->href	= &e->xlink->href;
		e->xlinkp->type = &e->xlink->type;
		xlinkp = e->xlinkp;
		
		e->animp = malloc(sizeof(SMILAnimationAttributesPointers));
		e->animp->accumulate	= &e->anim->accumulate;
		e->animp->additive		= &e->anim->additive;
		e->animp->attributeName = &e->anim->attributeName;
		e->animp->attributeType = &e->anim->attributeType;
		e->animp->by			= &e->anim->by;
		e->animp->calcMode		= &e->anim->calcMode;
		e->animp->from			= &e->anim->from;
		e->animp->keySplines	= &e->anim->keySplines;
		e->animp->keyTimes		= &e->anim->keyTimes;
		e->animp->lsr_enabled	= &e->anim->lsr_enabled;
		e->animp->to			= &e->anim->to;
		e->animp->type			= &e->anim->type;
		e->animp->values		= &e->anim->values;
		animp = e->animp;
	}
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		SVG_SANI_Element *e = (SVG_SANI_Element *)node;
		e->xlinkp = malloc(sizeof(XLinkAttributesPointers));
		e->xlinkp->href	= &e->xlink->href;
		e->xlinkp->type = &e->xlink->type;
		xlinkp = e->xlinkp;
		
		e->animp = malloc(sizeof(SMILAnimationAttributesPointers));
		e->animp->accumulate	= &e->anim->accumulate;
		e->animp->additive		= &e->anim->additive;
		e->animp->attributeName = &e->anim->attributeName;
		e->animp->attributeType = &e->anim->attributeType;
		e->animp->by			= &e->anim->by;
		e->animp->calcMode		= &e->anim->calcMode;
		e->animp->from			= &e->anim->from;
		e->animp->keySplines	= &e->anim->keySplines;
		e->animp->keyTimes		= &e->anim->keyTimes;
		e->animp->lsr_enabled	= &e->anim->lsr_enabled;
		e->animp->to			= &e->anim->to;
		e->animp->type			= &e->anim->type;
		e->animp->values		= &e->anim->values;
		animp = e->animp;
	}
#endif
	else {
		return;
	}
	
	if (xlinkp->href->type == SVG_IRI_IRI) {
		if (!xlinkp->href->iri) { 
			fprintf(stderr, "Error: IRI not initialized\n");
			return;
		} else {
			GF_Node *n;
			
			n = (GF_Node*)gf_sg_find_node_by_name(gf_node_get_graph(node), xlinkp->href->iri);
			if (n) {
				xlinkp->href->type = SVG_IRI_ELEMENTID;
				xlinkp->href->target = n;
				gf_svg_register_iri(node->sgprivate->scenegraph, xlinkp->href);
			} else {
				return;
			}
		}
	} 
	if (!xlinkp->href->target) return;

	gf_smil_timing_init_runtime_info(node);
	gf_smil_anim_init_runtime_info(node);
}

