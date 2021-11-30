/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2004-2012
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
#include <gpac/nodes_svg.h>

#ifndef GPAC_DISABLE_LOG
u32 time_spent_in_anim = 0;
#endif

#ifndef GPAC_DISABLE_SVG


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
	if (!node->sgprivate->interact) {
		GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
		if (!node->sgprivate->interact) return GF_OUT_OF_MEM;
	}
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

	if (!animp->to) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SMIL,
		       ("[SMIL Animation] Animation     %s - set element without to attribute\n",
		        gf_node_get_log_name((GF_Node *)rai->anim_elt)));
		return;
	}
	if (!animp->to->type) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SMIL,
		       ("[SMIL Animation] Animation     %s - set element with an unparsed to attribute\n",
		        gf_node_get_log_name((GF_Node *)rai->anim_elt)));
		return;
	}

	if (rai->change_detection_mode) {
		/* if the set has been applied, unless next animations are additive we don't need
		   to apply it again */
		if (rai->previous_coef > 0) rai->interpolated_value_changed = 0;
		else rai->interpolated_value_changed = 1;
		return;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
		       ("[SMIL Animation] Time %f - Animation     %s - applying set animation\n",
		        gf_node_get_scene_time((GF_Node*)rai->anim_elt),
		        gf_node_get_log_name((GF_Node *)rai->anim_elt)));

		to_info.fieldType = animp->to->type;
		to_info.far_ptr   = animp->to->value;
		/* we do not need to resolve currentColor values or inherit values here,
		   because no further interpolation is required for the animation and
		   because inheritance is applied after animations in the compositor. */

		gf_svg_attributes_copy(&rai->interpolated_value, &to_info, 0);
		rai->previous_coef = FIX_ONE;
	}
}

static void gf_smil_anim_use_keypoints_keytimes(SMIL_Anim_RTI *rai, Fixed normalized_simple_time,
        Fixed *interpolation_coefficient, u32 *keyValueIndex)
{
	SMILAnimationAttributesPointers *animp = rai->animp;
	u32 keyTimeIndex = 0;
	Fixed interval_duration;

	*interpolation_coefficient = normalized_simple_time;

	/* Computing new interpolation coefficient */
	if (rai->key_times_count) {
		Fixed keyTimeBefore = 0, keyTimeAfter=0;
		for (keyTimeIndex = rai->previous_keytime_index; keyTimeIndex< rai->key_times_count; keyTimeIndex++) {
			Fixed *t = (Fixed *)gf_list_get(*animp->keyTimes, keyTimeIndex);
			if (normalized_simple_time < *t) {
				Fixed *tm1;
				rai->previous_keytime_index = keyTimeIndex;
				tm1 = (Fixed *) gf_list_get(*animp->keyTimes, keyTimeIndex-1);
				if (tm1) keyTimeBefore = *tm1;
				else keyTimeBefore = 0;
				keyTimeAfter = *t;
				break;
			}
		}
		keyTimeIndex--;
		interval_duration = keyTimeAfter - keyTimeBefore;
		if (keyValueIndex) *keyValueIndex = keyTimeIndex;
		if (interval_duration)
			*interpolation_coefficient = gf_divfix(normalized_simple_time - keyTimeBefore, interval_duration);
		else
			*interpolation_coefficient = FIX_ONE;
		if (!rai->change_detection_mode)
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - Using Key Times: index %d, interval duration %.2f, coeff: %.2f\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt),
			        gf_node_get_log_name((GF_Node *)rai->anim_elt),
			        keyTimeIndex,
			        interval_duration,
			        interpolation_coefficient));
	}

	if (rai->anim_elt->sgprivate->tag == TAG_SVG_animateMotion && rai->key_points_count) {
		Fixed *p1;
		p1 = (Fixed *)gf_list_get(*animp->keyPoints, keyTimeIndex);
		if (animp->calcMode && *animp->calcMode == SMIL_CALCMODE_DISCRETE) {
			*interpolation_coefficient = *p1;
		} else {
			Fixed *p2 = (Fixed *)gf_list_get(*animp->keyPoints, keyTimeIndex+1);
			*interpolation_coefficient = gf_mulfix(FIX_ONE - *interpolation_coefficient, *p1)
			                             + gf_mulfix(*interpolation_coefficient, (p2 ? *p2 : *p1));
		}
		if (keyValueIndex) *keyValueIndex = 0;
		if (!rai->change_detection_mode)
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - Using Key Points: key Point Index %d, coeff: %.2f\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), keyTimeIndex, *interpolation_coefficient));
	}
}

static void gf_smil_anim_animate_using_values(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributesPointers *animp = rai->animp;
	GF_List *values;
	GF_FieldInfo value_info, value_info_next;
	u32 keyValueIndex;
	Fixed interpolation_coefficient;
	u32 real_calcMode;

	values = animp->values->values;

	memset(&value_info, 0, sizeof(GF_FieldInfo));
	value_info.fieldType = animp->values->type;
	value_info_next = value_info;

	real_calcMode = (gf_svg_attribute_is_interpolatable(animp->values->type)?
	                 (animp->calcMode ? *animp->calcMode : SMIL_CALCMODE_LINEAR):
	                 SMIL_CALCMODE_DISCRETE
	                );

	if (rai->values_count == 1) {
		if (rai->change_detection_mode) {
			/* Since we have only 1 value, the previous key index should always be 0,
			   unless the animation has not started or is reset (-1) */
			if (rai->previous_key_index == 0) rai->interpolated_value_changed = 0;
			else rai->interpolated_value_changed = 1;
			return;
		} else {
			value_info.far_ptr = gf_list_get(values, 0);
			/* no further interpolation needed
			   therefore no need to resolve inherit and currentColor */
			gf_svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
			rai->previous_key_index = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - Using values[0] as interpolation value\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt)));
			return;
		}
	}

	/* Computing new key value index and interpolation coefficient */
	if (!rai->key_times_count) {
		if (real_calcMode == SMIL_CALCMODE_DISCRETE) {
			if (normalized_simple_time == FIX_ONE) {
				keyValueIndex = rai->values_count-1;
				interpolation_coefficient = FIX_ONE;
			} else {
				Fixed tmp = normalized_simple_time*rai->values_count;
				Fixed tmp_floor = gf_floor(tmp);
				if ((tmp - tmp_floor) == 0 && tmp) {
					keyValueIndex = FIX2INT(tmp_floor) - 1;
				} else {
					keyValueIndex = FIX2INT(tmp_floor);
				}
				interpolation_coefficient = tmp - INT2FIX(keyValueIndex);
			}
		} else {
			Fixed tmp = normalized_simple_time*(rai->values_count-1);
			if (normalized_simple_time == FIX_ONE) {
				keyValueIndex = rai->values_count-2;
			} else {
				keyValueIndex = FIX2INT(gf_floor(tmp));
			}
			interpolation_coefficient = tmp - INT2FIX(keyValueIndex);
		}
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL, ("[SMIL Animation] Time %f - Animation     %s - No KeyTimes: key index %d, coeff: %.2f\n", gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), keyValueIndex, FIX2FLT(interpolation_coefficient)));
	} else {
		gf_smil_anim_use_keypoints_keytimes(rai, normalized_simple_time, &interpolation_coefficient, &keyValueIndex);
	}

	if (rai->change_detection_mode) {
		if (real_calcMode == SMIL_CALCMODE_DISCRETE && rai->previous_key_index == (s32)keyValueIndex && rai->previous_coef != -FIX_ONE) {
			rai->interpolated_value_changed = 0;
		} else if (rai->previous_key_index == (s32)keyValueIndex && rai->previous_coef == interpolation_coefficient)
			rai->interpolated_value_changed = 0;
		else
			rai->interpolated_value_changed = 1;
	} else {
		rai->previous_key_index = keyValueIndex;
		rai->previous_coef = interpolation_coefficient;

		switch (real_calcMode) {
		case SMIL_CALCMODE_DISCRETE:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - applying discrete animation using values (key value index: %d)\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), keyValueIndex));
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
			if (keyValueIndex == rai->values_count - 1) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
				       ("[SMIL Animation] Time %f - Animation     %s - applying linear animation using values (setting last key value: %d)\n",
				        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), keyValueIndex));
				value_info.far_ptr = gf_list_get(values, rai->values_count - 1);
				/* no further interpolation needed
				   therefore no need to resolve inherit and currentColor */
				gf_svg_attributes_copy(&rai->interpolated_value, &value_info, 0);
			} else {

				GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
				       ("[SMIL Animation] Time %f - Animation     %s - applying linear animation using values (key value indices: %d, %d / coeff: %f)\n",
				        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), keyValueIndex, keyValueIndex+1, interpolation_coefficient));
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

				gf_svg_attributes_interpolate(&value_info,
				                              &value_info_next,
				                              &rai->interpolated_value,
				                              interpolation_coefficient, 1);
			}
			break;
		}
	}
}

static void gf_smil_anim_animate_from_to(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	GF_FieldInfo from_info, to_info;
	SMILAnimationAttributesPointers *animp = rai->animp;
	Fixed interpolation_coefficient;
	s32 useFrom = (normalized_simple_time<=FIX_ONE/2);
	u32 real_calcMode;

	real_calcMode = (animp->to && gf_svg_attribute_is_interpolatable(animp->to->type)?
	                 (animp->calcMode ? *animp->calcMode : SMIL_CALCMODE_LINEAR):
	                 SMIL_CALCMODE_DISCRETE
	                );

	if (rai->change_detection_mode) {
		if (rai->previous_coef == normalized_simple_time)
			rai->interpolated_value_changed = 0;
		else {
			if (real_calcMode == SMIL_CALCMODE_DISCRETE &&
			        useFrom == rai->previous_key_index) {
				rai->interpolated_value_changed = 0;
			} else {
				rai->interpolated_value_changed = 1;
			}
		}
	} else {

		if (animp->from) {
			from_info.fieldType = animp->from->type;
			from_info.far_ptr = animp->from->value;
		} else {
			from_info.fieldType = 0;
			from_info.far_ptr = NULL;
		}

		if (rai->is_first_anim)
			gf_svg_attributes_resolve_unspecified(&from_info,
			                                      &rai->owner->specified_value,
			                                      &rai->default_transform_value);
		else
			gf_svg_attributes_resolve_unspecified(&from_info,
			                                      &rai->owner->presentation_value,
			                                      &rai->default_transform_value);

		if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(from_info.fieldType)) {
			gf_svg_attributes_resolve_currentColor(&from_info, &rai->owner->current_color_value);
			gf_svg_attributes_resolve_inherit(&from_info, &rai->owner->parent_presentation_value);
		}
		if (animp->to) {
			to_info.fieldType = animp->to->type;
			to_info.far_ptr = animp->to->value;
		} else {
			to_info.fieldType = 0;
			to_info.far_ptr = NULL;
		}

		if (rai->is_first_anim)
			gf_svg_attributes_resolve_unspecified(&to_info,
			                                      &rai->owner->specified_value,
			                                      &rai->default_transform_value);
		else
			gf_svg_attributes_resolve_unspecified(&to_info,
			                                      &rai->owner->presentation_value,
			                                      &rai->default_transform_value);

		if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(to_info.fieldType)) {
			gf_svg_attributes_resolve_currentColor(&to_info, &rai->owner->current_color_value);
			gf_svg_attributes_resolve_inherit(&to_info, &rai->owner->parent_presentation_value);
		}

		gf_smil_anim_use_keypoints_keytimes(rai, normalized_simple_time, &interpolation_coefficient, NULL);

		rai->previous_coef = interpolation_coefficient;

		switch (real_calcMode) {
		case SMIL_CALCMODE_DISCRETE:
		{
			/* before half of the duration stay at 'from' and then switch to 'to' */
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - applying from-to animation (using %s value)\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), (useFrom?"from":"to")));
			gf_svg_attributes_copy(&rai->interpolated_value, (useFrom?&from_info:&to_info), 0);
			rai->previous_key_index = useFrom;
		}
		break;
		case SMIL_CALCMODE_SPLINE:
		case SMIL_CALCMODE_PACED:
		case SMIL_CALCMODE_LINEAR:
		default:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - applying from-to animation (linear interpolation, using coefficient %f)\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), interpolation_coefficient));
			gf_svg_attributes_interpolate(&from_info, &to_info, &rai->interpolated_value, interpolation_coefficient, 1);
			break;
		}
	}
}

static void gf_smil_anim_animate_from_by(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	Fixed from_coef;
	GF_FieldInfo from_info, by_info;
	SMILAnimationAttributesPointers *animp = rai->animp;
	s32 useFrom = (normalized_simple_time<=FIX_ONE/2);

	if (rai->change_detection_mode) {
		if (rai->previous_coef == normalized_simple_time)
			rai->interpolated_value_changed = 0;
		else {
			if (animp->calcMode &&
			        *animp->calcMode == SMIL_CALCMODE_DISCRETE &&
			        useFrom == rai->previous_key_index) {
				rai->interpolated_value_changed = 0;
			} else {
				rai->interpolated_value_changed = 1;
			}
		}
	} else {
		rai->previous_coef = normalized_simple_time;

		if (animp->from) {
			from_info.fieldType = animp->from->type;
			from_info.far_ptr = animp->from->value;
			from_coef = FIX_ONE;
		} else {
			from_info.fieldType = 0;
			from_info.far_ptr = NULL;
			/* this is a by animation only, then, it is always additive,
			   we don't need the from value*/
			from_coef = 0;
		}

		if (rai->is_first_anim)
			gf_svg_attributes_resolve_unspecified(&from_info,
			                                      &rai->owner->specified_value,
			                                      &rai->default_transform_value);
		else
			gf_svg_attributes_resolve_unspecified(&from_info,
			                                      &rai->owner->presentation_value,
			                                      &rai->default_transform_value);

		if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(from_info.fieldType)) {
			gf_svg_attributes_resolve_currentColor(&from_info, &rai->owner->current_color_value);
			gf_svg_attributes_resolve_inherit(&from_info, &rai->owner->parent_presentation_value);
		}

		if (animp->by) {
			by_info.fieldType = animp->by->type;
			by_info.far_ptr = animp->by->value;
		} else {
			by_info.fieldType = 0;
			by_info.far_ptr = NULL;
		}

		if (rai->owner->is_property && gf_svg_attribute_is_interpolatable(from_info.fieldType)) {
			gf_svg_attributes_resolve_currentColor(&by_info, &rai->owner->current_color_value);
			gf_svg_attributes_resolve_inherit(&by_info, &rai->owner->parent_presentation_value);
		}

		switch ((animp->calcMode ? *animp->calcMode : SMIL_CALCMODE_LINEAR)) {
		case SMIL_CALCMODE_DISCRETE:
		{
			/* before half of the duration stay at 'from' and then switch to 'to' */
			if (useFrom) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
				       ("[SMIL Animation] Time %f - Animation     %s - applying from-by animation (setting from)",
				        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt)));
				gf_svg_attributes_muladd(from_coef, &from_info, 0, &by_info, &rai->interpolated_value, 0);
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
				       ("[SMIL Animation] Time %f - Animation     %s - applying from-by animation (setting from+by)",
				        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt)));
				gf_svg_attributes_muladd(from_coef, &from_info, FIX_ONE, &by_info, &rai->interpolated_value, 0);
			}
			rai->previous_key_index = useFrom;
		}
		break;
		case SMIL_CALCMODE_SPLINE:
		case SMIL_CALCMODE_PACED:
		case SMIL_CALCMODE_LINEAR:
		default:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - applying from-by animation (linear interpolation between from and from+by, coef: %f)\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), normalized_simple_time));
			gf_svg_attributes_muladd(from_coef, &from_info, normalized_simple_time, &by_info, &rai->interpolated_value, 0);
			break;
		}
	}
}

static void gf_svg_compute_path_anim(SMIL_Anim_RTI *rai, GF_Matrix2D *m, Fixed normalized_simple_time)
{
	Fixed offset;
	offset = gf_mulfix(normalized_simple_time, rai->length);
	gf_mx2d_init(*m);

	gf_path_iterator_get_transform(rai->path_iterator, offset, 1, m, 1, 0);
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL, ("offset: %f, position: (%f, %f)", offset, ((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[2], ((GF_Matrix2D *)rai->interpolated_value.far_ptr)->m[5]));
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
}

static void gf_smil_anim_animate_using_path(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	Fixed interpolation_coefficient;

	gf_smil_anim_use_keypoints_keytimes(rai, normalized_simple_time, &interpolation_coefficient, NULL);

	if (rai->change_detection_mode) {
		if (rai->previous_coef == interpolation_coefficient)
			rai->interpolated_value_changed = 0;
		else {
			rai->interpolated_value_changed = 1;
		}
	} else {
		rai->previous_coef = interpolation_coefficient;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
		       ("[SMIL Animation] Time %f - Animation     %s - applying path animation (coef: %f)\n",
		        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), normalized_simple_time));

		gf_svg_compute_path_anim(rai, (GF_Matrix2D*)rai->interpolated_value.far_ptr, interpolation_coefficient);
	}
}

static void gf_smil_anim_compute_interpolation_value(SMIL_Anim_RTI *rai, Fixed normalized_simple_time)
{
	SMILAnimationAttributesPointers *animp = rai->animp;

	if (rai->path) {
		gf_smil_anim_animate_using_path(rai, normalized_simple_time);
	} else if (rai->anim_elt->sgprivate->tag == TAG_SVG_set) {
		gf_smil_anim_set(rai);
	} else if (rai->values_count) {
		/* Ignore 'from'/'to'/'by'*/
		gf_smil_anim_animate_using_values(rai, normalized_simple_time);
	} else if ((animp->by && animp->by->type) && (!animp->to || animp->to->type == 0)) {
		/* 'to' is not specified but 'by' is, so this is a 'by' animation or a 'from'-'by' animation */
		gf_smil_anim_animate_from_by(rai, normalized_simple_time);
	} else {
		/* Ignore 'by' if specified */
		gf_smil_anim_animate_from_to(rai, normalized_simple_time);
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_SMIL, GF_LOG_DEBUG)) {
		char *str = gf_svg_dump_attribute(rai->anim_elt, &rai->interpolated_value);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL, ("[SMIL Animation] Time %f - Animation     %s - Interpolation value changed for attribute %s, new value: %s \n",
		       gf_node_get_scene_time(rai->anim_elt), gf_node_get_log_name(rai->anim_elt),
		       gf_svg_get_attribute_name(rai->anim_elt, rai->owner->presentation_value.fieldIndex), str)
		);

		if (str) gf_free(str);
	}
#endif
}

void gf_smil_anim_set_anim_runtime_in_timing(GF_Node *n)
{
	u32 i, j;
	SVGTimedAnimBaseElement *timed_elt = NULL;
	SMIL_Timing_RTI *rti = NULL;
	GF_Node *target = NULL;

	if (!n) return;
	timed_elt = (SVGTimedAnimBaseElement *)n;

	if (!gf_svg_is_animation_tag(n->sgprivate->tag)) return;

	target = timed_elt->xlinkp->href->target;
	if (!target) return;

	if (timed_elt->timingp) rti = timed_elt->timingp->runtime;
	if (!rti) return;

	rti->rai = NULL;

	for (i = 0; i < gf_node_animation_count(target); i++) {
		SMIL_Anim_RTI *rai_tmp;
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get(target, i);
		j=0;
		while ((rai_tmp = (SMIL_Anim_RTI *)gf_list_enum(aa->anims, &j))) {
			if (rai_tmp->timingp->runtime == rti) {
				rti->rai = rai_tmp;
				return;
			}
		}
	}
}

static void gf_smil_anim_get_last_specified_value(SMIL_Anim_RTI *rai)
{
	SMILAnimationAttributesPointers *animp = rai->animp;

	if (!animp) return;

	if (rai->path) {
		if (!rai->last_specified_value.far_ptr) {
			rai->last_specified_value.far_ptr = gf_malloc(sizeof(GF_Matrix2D));
			rai->last_specified_value.fieldType = SVG_Matrix2D_datatype;
		}
		gf_svg_compute_path_anim(rai, rai->last_specified_value.far_ptr, FIX_ONE);
		return;
	} else if (rai->anim_elt->sgprivate->tag == TAG_SVG_set) {
		if (animp->to) {
			rai->last_specified_value.fieldType = animp->to->type;
			rai->last_specified_value.far_ptr   = animp->to->value;
		} else {
			/* TODO ??? */
			GF_LOG(GF_LOG_ERROR, GF_LOG_SMIL,
			       ("[SMIL Animation] Animation     %s - set element without to attribute\n",
			        gf_node_get_log_name((GF_Node *)rai->anim_elt)));
		}
		return;
	}

	if (rai->values_count) {
		/* Ignore from/to/by*/
		rai->last_specified_value.fieldType = animp->values->type;
		rai->last_specified_value.far_ptr = gf_list_last(animp->values->values);
	} else if ((animp->by && animp->by->type) && (!animp->to || animp->to->type == 0)) {
		rai->last_specified_value.fieldType = animp->by->type;
		rai->last_specified_value.far_ptr   = animp->by->value;
	} else if (animp->to) {
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

	nb_iterations = (timingp->runtime->current_interval ? timingp->runtime->current_interval->nb_iterations : 1);

	if (rai->change_detection_mode) {
		if ((animp->accumulate && *animp->accumulate == SMIL_ACCUMULATE_SUM)
		        && nb_iterations > 0
		        && rai->previous_iteration != (s32) nb_iterations) {
			/* if we actually do accumulation and the number of iteration is different,
			then we force the result as changed regardless of the result of the interpolation
			(TODO: check if this need to be improved)*/
			rai->interpolated_value_changed = 1;
		} else {
			/* if we don't accumulate we leave the value of interpolated_value_changed unchanged */
		}
	} else {
		if (nb_iterations > 0 && rai->previous_iteration != (s32) nb_iterations) {
			rai->previous_iteration = nb_iterations;
		}

		if ((animp->accumulate && *animp->accumulate == SMIL_ACCUMULATE_SUM) && nb_iterations > 0) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - applying accumulation (iteration #%d)\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt), nb_iterations));

			gf_svg_attributes_muladd(FIX_ONE, &rai->interpolated_value,
			                         INT2FIX(nb_iterations), &rai->last_specified_value,
			                         &rai->interpolated_value, 1);

			if ((animp->from) && animp->by && (rai->last_specified_value.far_ptr == animp->by->value)) {
				/* this is a from-by animation, the last specified value is not the 'by' value but actually 'from'+'by',
				we need to add nb_iterations times from to the interpolated_value
				see (animate-elem-210-t.svg (upper two circles in the mid column, after 9s/14s */
				GF_FieldInfo from_info;
				from_info.fieldType = rai->animp->from->type;
				from_info.far_ptr = rai->animp->from->value;
				gf_svg_attributes_muladd(FIX_ONE, &rai->interpolated_value,
				                         INT2FIX(nb_iterations), &from_info,
				                         &rai->interpolated_value, 1);
			}
		}
	}
}

static void gf_smil_apply_additive(SMIL_Anim_RTI *rai)
{
	SMILAnimationAttributesPointers *animp = rai->animp;
	if (rai->change_detection_mode) return;
	else {
		/* Apply additive behavior if required
			PV = (additive == sum ? PV + animp->IV : animp->IV); */
		if (animp->additive && *animp->additive == SMIL_ADDITIVE_SUM) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - applying additive behavior\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt)));

			gf_svg_attributes_add((rai->is_first_anim ? &rai->owner->specified_value : &rai->owner->presentation_value),
			                      &rai->interpolated_value,
			                      &rai->owner->presentation_value,
			                      1);

#ifndef GPAC_DISABLE_LOG
			if (gf_log_tool_level_on(GF_LOG_SMIL, GF_LOG_DEBUG)) {
				char *str = gf_svg_dump_attribute((GF_Node*)rai->anim_elt, &rai->owner->presentation_value);
				GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL, ("[SMIL Animation] Time %f - Animation     %s - Presentation value changed for attribute %s, new value: %s\n",
				       gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node*)rai->anim_elt),
				       gf_svg_get_attribute_name((GF_Node*)rai->anim_elt, rai->owner->presentation_value.fieldIndex), str)
				);
				if (str) gf_free(str);
			}
#endif

		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
			       ("[SMIL Animation] Time %f - Animation     %s - applying non-additive behavior\n",
			        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt)));

			/* FIXME: if we switch pointers to avoid copying values,
			we need to modify the address in the DOM node as well,
			we also need to take care about change detections. Not easy!!

			void *tmp = rai->owner->presentation_value.far_ptr;
			rai->owner->presentation_value.far_ptr = rai->interpolated_value.far_ptr;
			rai->interpolated_value.far_ptr = tmp;
			*/

			gf_svg_attributes_copy(&rai->owner->presentation_value, &rai->interpolated_value, 1);
#ifndef GPAC_DISABLE_LOG
			if (gf_log_tool_level_on(GF_LOG_SMIL, GF_LOG_DEBUG)) {
				char *str = gf_svg_dump_attribute((GF_Node*)rai->anim_elt, &rai->owner->presentation_value);

				GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL, ("[SMIL Animation] Time %f - Animation     %s - Presentation value changed for attribute %s, new value: %s\n",
				       gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node*)rai->anim_elt),
				       gf_svg_get_attribute_name((GF_Node*)rai->anim_elt, rai->owner->presentation_value.fieldIndex), str)
				);

				if (str) gf_free(str);
			}
#endif
		}
	}
}

static void gf_smil_anim_animate(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	if (!rti || !rti->rai || !rti->rai->animp) return;
	SMIL_Anim_RTI *rai = rti->rai;

	gf_smil_anim_compute_interpolation_value(rai, normalized_simple_time);
	gf_smil_anim_apply_accumulate(rai);
	gf_smil_apply_additive(rai);
}

void gf_smil_anim_reset_variables(SMIL_Anim_RTI *rai)
{
	if (!rai) return;
	/* we reset all the animation parameters to force computation of next interpolation value
	   when the animation restarts */
	rai->interpolated_value_changed = 0;
	rai->previous_key_index = -1;
	rai->previous_coef = -FIX_ONE;
	rai->previous_iteration = -1;
	rai->previous_keytime_index = 0;
	rai->anim_done = 0;
}

/* copy/paste of the animate function
 TODO: check if computations of interpolation value can be avoided.
*/
static void gf_smil_anim_freeze(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	if (!rti || !rti->rai || !rti->rai->animp) return;
	SMIL_Anim_RTI *rai = rti->rai;

	if (rai->change_detection_mode) {
		if (rai->anim_done == 0)
			rai->interpolated_value_changed = 1;
		else
			rai->interpolated_value_changed = 0;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
		       ("[SMIL Animation] Time %f - Animation     %s - applying freeze behavior\n",
		        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt)));

		gf_smil_anim_compute_interpolation_value(rai, normalized_simple_time);
		gf_smil_anim_apply_accumulate(rai);
		gf_smil_apply_additive(rai);
		rai->anim_done = 1;
	}
}

static void gf_smil_anim_remove(SMIL_Timing_RTI *rti, Fixed normalized_simple_time)
{
	SMIL_Anim_RTI *rai = rti->rai;
	if (!rai) return;

	if (rai->change_detection_mode) {
		if (rai->anim_done == 0)
			rai->interpolated_value_changed = 1;
		else
			rai->interpolated_value_changed = 0;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL,
		       ("[SMIL Animation] Time %f - Animation     %s - applying remove behavior\n",
		        gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node *)rai->anim_elt)));

		/* TODO: see if we can avoid this copy by switching pointers */
		gf_svg_attributes_copy(&rai->owner->presentation_value, &rai->owner->specified_value, 0);
		/* TODO: check if we need to apply additive behavior even in fill='remove'
		   maybe (see animate-elem-211-t.svg) */

		rai->anim_done = 1;

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_SMIL, GF_LOG_DEBUG)) {
			char *str = gf_svg_dump_attribute((GF_Node*)rai->anim_elt, &rai->owner->presentation_value);

			GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL, ("[SMIL Animation] Time %f - Animation     %s - Presentation value changed for attribute %s, new value: %s\n",
			       gf_node_get_scene_time((GF_Node*)rai->anim_elt), gf_node_get_log_name((GF_Node*)rai->anim_elt),
			       gf_svg_get_attribute_name((GF_Node*)rai->anim_elt, rai->owner->presentation_value.fieldIndex), str)
			);
			if (str) gf_free(str);
		}
#endif

	}
}

static void gf_smil_anim_evaluate(SMIL_Timing_RTI *rti, Fixed normalized_simple_time, GF_SGSMILTimingEvalState state)
{
	SMIL_Anim_RTI *rai = rti->rai;
	switch (state) {
	case SMIL_TIMING_EVAL_REPEAT:
		/* we are starting a new cycle of animation, therefore we need to reset the previous state variables
		   like previous_keytime_index ... */
		gf_smil_anim_reset_variables(rai);
	case SMIL_TIMING_EVAL_UPDATE:
		gf_smil_anim_animate(rti, normalized_simple_time);
		break;
	case SMIL_TIMING_EVAL_FREEZE:
		gf_smil_anim_freeze(rti, normalized_simple_time);
		break;
	case SMIL_TIMING_EVAL_REMOVE:
		gf_smil_anim_remove(rti, normalized_simple_time);
		break;
	case SMIL_TIMING_EVAL_FRACTION:
		gf_smil_anim_animate(rti, normalized_simple_time);
		rti->evaluate_status = SMIL_TIMING_EVAL_NONE;
		break;
		/*
			discard should be done before in smil_notify_time
			case SMIL_TIMING_EVAL_DISCARD:
				break;
		*/
	default:
		break;
	}
}
/**************************************************************************************
 **************************************************************************************/

GF_EXPORT
void gf_svg_apply_animations(GF_Node *node, SVGPropertiesPointers *render_svg_props)
{
	u32 count_all, i;
	u32 active_anim;
#ifndef GPAC_DISABLE_LOG
	u32 time=0;

	if (gf_log_tool_level_on(GF_LOG_RTI, GF_LOG_DEBUG)) {
		time = gf_sys_clock();
	}
#endif

	/* Perform all the animations on this node */
	count_all = gf_node_animation_count(node);
	for (i = 0; i < count_all; i++) {
		GF_FieldInfo info;
		s32 j;
		u32 count;
		SMIL_AttributeAnimations *aa;


		aa = (SMIL_AttributeAnimations *)gf_node_animation_get(node, i);
		count = gf_list_count(aa->anims);
		if (!count) continue;

		aa->presentation_value_changed = 0;

		if (aa->is_property) {
			/* Storing the pointer to the parent presentation value,
			   i.e. the presentation value produced at the parent level in the tree */
			aa->parent_presentation_value = aa->presentation_value;
			aa->parent_presentation_value.far_ptr =
			    gf_svg_get_property_pointer((SVG_Element *)node, aa->orig_dom_ptr, render_svg_props);

			/* Storing also the pointer to the presentation value of the color property
			   (special handling of the keyword 'currentColor' if used in animation values) */
			gf_node_get_attribute_by_tag(node, TAG_SVG_ATT_color, 1, 1, &info);
			aa->current_color_value.far_ptr = info.far_ptr;
		}

		/* We start with the last animation (TODO in the execution order), then scan in the reverse order
		up to the first animation which is not additive, to determine if the presentation value will change
		We evaluate each animation, but only in the 'change_detection_mode' */
		for (j = count-1; j >= 0; j--) {
			SMIL_Anim_RTI *rai = (SMIL_Anim_RTI *)gf_list_get(aa->anims, j);
			SMIL_Timing_RTI *rti = rai->timingp->runtime;

			rai->interpolated_value_changed = 0;

			/* The evaluate_status has been updated when notifying the new scene time to this animation,
			   i.e. before the scene tree traversal */
			if (rti->evaluate_status) {
				rai->change_detection_mode = 1;
				rti->evaluate(rti, rti->normalized_simple_time, rti->evaluate_status);
				aa->presentation_value_changed += rai->interpolated_value_changed;
				if (!rai->animp->additive || *rai->animp->additive == SMIL_ADDITIVE_REPLACE) {
					/* we don't need to check previous animations since this one will overwrite it */
					j--;
					break;
				}
			}
		}

		active_anim = 0;
		if (aa->presentation_value_changed) {
			/* If the result of all the combined animations will produce a different result compared to the previous frame,
			we start in the forward order from the j were the previous step stopped (i.e. the first anim in replace mode)
			and evaluate each animation, in the computation mode (change_detection_mode = 0)*/
			for (j++; j<(s32)count; j++) {
				SMIL_Anim_RTI *rai = (SMIL_Anim_RTI *)gf_list_get(aa->anims, j);
				SMIL_Timing_RTI *rti = rai->timingp->runtime;

				if (j == 0) rai->is_first_anim = 1;
				else rai->is_first_anim = 0;

				if (rti->evaluate_status) {
					rai->change_detection_mode = 0;
					rti->evaluate(rti, rti->normalized_simple_time, rti->evaluate_status);
					active_anim++;
				}
			}

			/* DEBUG: uncomment this line to remove animation effect, and keep animation computation */
//		gf_svg_attributes_copy(&aa->presentation_value, &aa->specified_value, 0);

#ifndef GPAC_DISABLE_LOG
			if (gf_log_tool_level_on(GF_LOG_SMIL, GF_LOG_DEBUG)) {
				char *str = gf_svg_dump_attribute(node, &aa->presentation_value);

				GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL, ("[SMIL Animation] Time %f - Element %s - Presentation value changed for attribute %s, new value: %s - dirty flags %x\n",
				       gf_node_get_scene_time(node), gf_node_get_log_name(node),
				       gf_svg_get_attribute_name(node, aa->presentation_value.fieldIndex), str, aa->dirty_flags)
				);

				if (str) gf_free(str);
			}
#endif

		} else {
			/* DEBUG: uncomment this line to remove animation effect, and keep animation computation */
//			gf_svg_attributes_copy(&aa->presentation_value, &aa->specified_value, 0);
		}

		/* we only set dirty flags when a real flag is set to avoid unnecessary computation
		   for example, it is not necessary to set it when the anim is an animateTransform
		   since there is no associated flag */
		if (aa->dirty_flags) {
			if (aa->presentation_value_changed) {
				gf_node_dirty_set(node, aa->dirty_flags, aa->dirty_parents);
			} else {
				/* WARNING - This does not work for use elements because apply_animations may be called several times */
				if (active_anim) gf_node_dirty_clear(node, aa->dirty_flags);
			}
		}
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_RTI, GF_LOG_DEBUG)) {
		time_spent_in_anim += gf_sys_clock() - time;
	}
#endif
}


GF_Node *gf_smil_anim_get_target(GF_Node *e)
{
	XLinkAttributesPointers *xlinkp = NULL;
	if (!gf_svg_is_animation_tag(e->sgprivate->tag)) return NULL;
	xlinkp = ((SVGTimedAnimBaseElement *)e)->xlinkp;
	return (xlinkp && xlinkp->href) ? xlinkp->href->target : NULL;
}

/* Attributes from the animation elements are not easy to use during runtime,
   the runtime info is a set of easy to use structures.
   This function initializes them (interpolation values ...)
   Needs to be called after gf_smil_timing_init_runtime_info */
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

	if (!e) return;

	/* Filling animation structures to be independent of the SVG Element structure */
	animp = ((SVGTimedAnimBaseElement *)e)->animp;
	timingp = ((SVGTimedAnimBaseElement *)e)->timingp;
	if (!animp || !timingp) return;
	xlinkp = ((SVGTimedAnimBaseElement *)e)->xlinkp;

	target = xlinkp->href->target;

	memset(&target_attribute, 0, sizeof(GF_FieldInfo));
	if (animp->attributeName && (animp->attributeName->name || animp->attributeName->tag)) {
		/* Filling the target_attribute structure with info on the animated attribute (type, pointer to data, ...)
		NOTE: the animated attribute is created with a default value, if it was not specified on the target element */
		if (animp->attributeName->tag) {
			gf_node_get_attribute_by_tag(target, animp->attributeName->tag, 1, 1, &target_attribute);
		} else {
			gf_node_get_field_by_name(target, animp->attributeName->name, &target_attribute);
		}
	} else {
		/* All animation elements should have a target attribute except for animateMotion
		cf http://www.w3.org/mid/u403c21ajf1sjqtk58g0g38eaep9f9g2ss@hive.bjoern.hoehrmann.de
		"For animateMotion, the attributeName is implied and cannot be specified;
		animateTransform requires specification of the attribute name and any attribute that is
		a transform-like attribute can be a target, e.g. gradientTransform."*/

		switch (e->sgprivate->tag) {
		case TAG_SVG_animateMotion:
			/* Explicit creation of the pseudo 'motionTransform' attribute since it cannot be specified */
			gf_node_get_attribute_by_tag(target, TAG_SVG_ATT_motionTransform, 1, 0, &target_attribute);
			gf_mx2d_init(*(GF_Matrix2D *)target_attribute.far_ptr);
			break;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_SMIL,
			       ("[SMIL Animation] Missing attributeName attribute on element %s\n",
			        gf_node_get_log_name((GF_Node*)e) ));
			return;
		}
	}

	if (animp->attributeType && *animp->attributeType == SMIL_ATTRIBUTETYPE_CSS) {
		/* see example animate-elem-219-t.svg from the SVG test suite, upper row */
		if (!gf_svg_is_property(target, &target_attribute)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_SMIL,
			       ("[SMIL Animation] Using CSS attributeType for an animation on an attribute which is not a property %s\n",
			        gf_node_get_log_name((GF_Node*)e) ));
			return;
		}
	}

	/* Creation and setup of the runtime structure for animation */
	GF_SAFEALLOC(rai, SMIL_Anim_RTI)
	if (!rai) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SMIL, ("[SMIL Animation] Failed to allocated SMIL anim RTI\n"));
		return;
	}

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
	gf_smil_anim_reset_variables(rai);

	rai->values_count = (animp->values ? gf_list_count(animp->values->values) : 0);
	rai->key_times_count = (animp->keyTimes ? gf_list_count(*animp->keyTimes) : 0);
	rai->key_points_count = (animp->keyPoints ? gf_list_count(*animp->keyPoints) : 0);
	rai->key_splines_count = (animp->keySplines ? gf_list_count(*animp->keySplines) : 0);


	if (!rai->values_count &&										 /* 'values' attribute not specified */
	        (!animp->to || animp->to->type == 0) &&						 /* 'to' attribute not specified */
	        (!animp->from || animp->from->type == 0) &&					 /* 'from' attribute not specified */
	        (animp->by && animp->by->type != 0)) {						 /* 'by' attribute specified */
		/* if this is a 'by' animation without from the animation is defined to be additive
		   see http://www.w3.org/TR/2005/REC-SMIL2-20051213/animation.html#AnimationNS-FromToBy
		   we override the additive attribute */
		if (!animp->additive) {
			/* this case can only happen with dynamic allocation of attributes */
			GF_FieldInfo info;
			gf_node_get_attribute_by_tag(e, TAG_SVG_ATT_additive, 1, 0, &info);
			animp->additive = info.far_ptr;
		}
		if (*animp->additive == SMIL_ADDITIVE_REPLACE) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_SMIL, ("[SMIL Animation] Warning: by-animations cannot use additive=\"replace\"\n"));
		}
		*animp->additive = SMIL_ADDITIVE_SUM;
	}

	/*TODO
	http://www.w3.org/TR/2005/REC-SMIL2-20051213/animation.html#animationNS-ToAnimation
		To animation defines its own kind of additive semantics, so the additive attribute is ignored.
	*/

	/*TODO
	http://www.w3.org/TR/2005/REC-SMIL2-20051213/animation.html#animationNS-ToAnimation
		Because to animation is defined in terms of absolute values of the target attribute,
		cumulative animation is not defined:
	*/

	/* TODO
	http://www.w3.org/TR/2005/REC-SMIL2-20051213/animation.html#animationNS-setElement
	The set element is non-additive. The additive and accumulate attributes are not allowed,
	and will be ignored if specified.
	*/

	/* For animateMotion, we need to retrieve the value of the rotate attribute, retrieve the path either
	from the 'path' attribute or from the 'mpath' element, and then initialize the path iterator*/
	if (e->sgprivate->tag == TAG_SVG_animateMotion) {
		GF_Path *the_path = NULL;
		GF_ChildNodeItem *child = NULL;

		GF_FieldInfo info;
		if (gf_node_get_attribute_by_tag(e, TAG_SVG_ATT_rotate, 0, 0, &info) == GF_OK) {
			rai->rotate = ((SVG_Rotate *)info.far_ptr)->type;
		} else {
			rai->rotate = SVG_NUMBER_VALUE;
		}
		if (gf_node_get_attribute_by_tag(e, TAG_SVG_ATT_path, 0, 0, &info) == GF_OK) {
			the_path = ((SVG_PathData *)info.far_ptr);
		}
		child = ((SVG_Element *)e)->children;

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
					if (child_tag == TAG_SVG_mpath) {
						if (gf_node_get_attribute_by_tag(child->node, TAG_XLINK_ATT_href, 0, 0, &info) == GF_OK) {
							XMLRI *iri = (XMLRI *)info.far_ptr;
							if (iri->target) used_path = iri->target;
							else if (iri->string) used_path =
								    (GF_Node *)gf_sg_find_node_by_name(gf_node_get_graph(child->node), iri->string);
							if (used_path && gf_node_get_tag(used_path) == TAG_SVG_path) {
								gf_node_get_attribute_by_tag(used_path, TAG_SVG_ATT_d, 1, 0, &info);
#if USE_GF_PATH
								rai->path = (SVG_PathData *)info.far_ptr;
#else
								gf_svg_path_build(rai->path,
								                  ((SVG_PathData *)info.far_ptr)->commands,
								                  ((SVG_PathData *)info.far_ptr)->points);
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
		if (!aa) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SMIL, ("[SMIL Animation] Failed to allocated SMIL attribue ani\n"));
			return;
		}

		/* We determine if the animated attribute is a property since this changes quite a lot the animation model */
		aa->is_property = gf_svg_is_property(target, &target_attribute);
		aa->current_color_value.fieldType = SVG_Paint_datatype;

		/* We copy (one copy for all animations on the same attribute) the DOM specified
		   value before any animation starts (because the animation will override it),
		   we also save the initial memory address of the specified value (orig_dom_ptr)
		   for inheritance hack */
		aa->specified_value = target_attribute;
		aa->orig_dom_ptr = aa->specified_value.far_ptr;
		aa->specified_value.far_ptr = gf_svg_create_attribute_value(target_attribute.fieldType);
		gf_svg_attributes_copy(&aa->specified_value, &target_attribute, 0);

		/* Now, the initial memory address of the specified value holds the presentation value,
		   and the presentation value is initialized */
		aa->presentation_value = target_attribute;

		aa->anims = gf_list_new();
		gf_list_add(aa->anims, rai);
		gf_node_animation_add(target, aa);

		/* determine what the rendering will need to do when the animation runs */
		aa->dirty_flags = gf_svg_get_modification_flags((SVG_Element *)target, &target_attribute);

		/* If the animation will result in a change of geometry or of the display property,
		   this animation will require traversing the tree, we need to inform the parents of the target node */
		aa->dirty_parents = 0;
		if (aa->dirty_flags & (GF_SG_SVG_GEOMETRY_DIRTY | GF_SG_SVG_DISPLAY_DIRTY)) aa->dirty_parents = 1;
	}

	rai->owner = aa;
	gf_smil_anim_get_last_specified_value(rai);

	/* for animation (unlike other timed elements like video), the evaluation (i.e. interpolation) cannot be done
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
	if (rai->path) {
		gf_svg_delete_attribute_value(rai->last_specified_value.fieldType,
		                              rai->last_specified_value.far_ptr,
		                              rai->anim_elt->sgprivate->scenegraph);
#if USE_GF_PATH
#else
		if (rai->path) gf_path_del(rai->path);
#endif

	}

	if (rai->path_iterator) gf_path_iterator_del(rai->path_iterator);
	gf_free(rai);
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
			gf_svg_delete_attribute_value(aa->specified_value.fieldType,
			                              aa->specified_value.far_ptr,
			                              target->sgprivate->scenegraph);
			aa->presentation_value.far_ptr = aa->orig_dom_ptr;
			gf_node_animation_rem((GF_Node *)target, i);
			gf_free(aa);
		}
	}
}

void gf_smil_anim_delete_animations(GF_Node *e)
{
	u32 i, j;

	for (i = 0; i < gf_node_animation_count(e); i ++) {
		SMIL_Anim_RTI *rai;
		SMIL_AttributeAnimations *aa = (SMIL_AttributeAnimations *)gf_node_animation_get(e, i);
		gf_svg_delete_attribute_value(aa->specified_value.fieldType,
		                              aa->specified_value.far_ptr,
		                              e->sgprivate->scenegraph);
		j=0;
		while ((rai = (SMIL_Anim_RTI *)gf_list_enum(aa->anims, &j))) {
			rai->xlinkp->href->target = NULL;
			gf_smil_anim_delete_runtime_info(rai);
		}
		gf_list_del(aa->anims);
		gf_free(aa);
	}
	gf_node_animation_del(e);
}

void gf_smil_anim_init_discard(GF_Node *node)
{
	SVGAllAttributes all_atts;
	XLinkAttributesPointers *xlinkp = NULL;
	SVGTimedAnimBaseElement *e = (SVGTimedAnimBaseElement *)node;
	gf_smil_timing_init_runtime_info(node);

	gf_svg_flatten_attributes((SVG_Element *)e, &all_atts);
	GF_SAFEALLOC(e->xlinkp, XLinkAttributesPointers);
	if (!e->xlinkp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SMIL,("[SMIL] Error creating anim xlink attrib\n"));
		return;
	}
	xlinkp = e->xlinkp;
	xlinkp->href = all_atts.xlink_href;
	xlinkp->type = all_atts.xlink_type;

	e->timingp->runtime->evaluate_status = SMIL_TIMING_EVAL_DISCARD;
}

void gf_smil_anim_init_node(GF_Node *node)
{
	XLinkAttributesPointers *xlinkp;
	SMILAnimationAttributesPointers *animp = NULL;
	SVGAllAttributes all_atts;
	SVGTimedAnimBaseElement *e = (SVGTimedAnimBaseElement *)node;

	gf_svg_flatten_attributes((SVG_Element *)e, &all_atts);
	e->xlinkp = gf_malloc(sizeof(XLinkAttributesPointers));
	xlinkp = e->xlinkp;
	xlinkp->href = all_atts.xlink_href;
	xlinkp->type = all_atts.xlink_type;

	/*perform init of default values
	  When the xlink:href attribute of animation is not set, the target defaults to the parent element */
	if (!xlinkp->href) {
		GF_FieldInfo info;
		gf_node_get_attribute_by_tag((GF_Node *)node, TAG_XLINK_ATT_href, 1, 0, &info);
		xlinkp->href = info.far_ptr;
		xlinkp->href->type = XMLRI_ELEMENTID;
		xlinkp->href->target = gf_node_get_parent(node, 0);
	}
	if (xlinkp->href->type == XMLRI_STRING) {
		if (!xlinkp->href->string) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SMIL,("Error: IRI not initialized\n"));
			return;
		} else {
			GF_Node *n;

			n = (GF_Node*)gf_sg_find_node_by_name(gf_node_get_graph(node), xlinkp->href->string);
			if (n) {
				xlinkp->href->type = XMLRI_ELEMENTID;
				xlinkp->href->target = n;
				gf_node_register_iri(node->sgprivate->scenegraph, xlinkp->href);
			} else {
				return;
			}
		}
	}
	if (!xlinkp->href->target) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_SMIL,("Trying to initialize an animation when the target is not known\n"));
		return;
	}

	// We may not have an attribute name, when using an animateMotion element
	if (node->sgprivate->tag != TAG_SVG_animateMotion && !all_atts.attributeName) {
		goto end_init;
	}

	/* if an attribute (to, from or by) is present but its type is not set
	(e.g. it could not be determined before, the target was not known), we try to get the type from the target */
	if ( (all_atts.to && (all_atts.to->type==0))
	        || (all_atts.from && (all_atts.from->type==0))
	        || (all_atts.by && (all_atts.by->type==0))
	   ) {
		GF_FieldInfo info;
		if (gf_node_get_attribute_by_name((GF_Node *)xlinkp->href->target, all_atts.attributeName->name, 0, 1, 1, &info)==GF_OK) {
			u32 anim_value_type = info.fieldType;
			u32 i;
			for (i=0; i<3; i++) {
				u32 tag = 0;
				switch (i) {
				case 0:
					tag=TAG_SVG_ATT_to;
					break;
				case 1:
					tag=TAG_SVG_ATT_from;
					break;
				case 2:
					tag=TAG_SVG_ATT_by;
					break;
				}
				if (gf_node_get_attribute_by_tag((GF_Node *)node, tag, 0, 0, &info)==GF_OK) {
					SMIL_AnimateValue *attval = info.far_ptr;
					if (attval->type==0) {
						SVG_String string = attval->value;
						attval->value = NULL;
						if (string) {
							gf_svg_parse_attribute((GF_Node *)node, &info, string, anim_value_type);
							gf_free(string);
						}
					}
				}
			}
		}
	}

	e->animp = gf_malloc(sizeof(SMILAnimationAttributesPointers));
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
	animp->type			 = all_atts.transform_type;
	animp->values		 = all_atts.values;
	if (node->sgprivate->tag == TAG_SVG_animateMotion) {
		e->animp->keyPoints = all_atts.keyPoints;
		e->animp->origin = all_atts.origin;
		e->animp->path = all_atts.path;
		e->animp->rotate = all_atts.rotate;
	} else {
		e->animp->keyPoints = NULL;
		e->animp->origin = NULL;
		e->animp->path = NULL;
		e->animp->rotate = NULL;
	}

end_init:
	gf_smil_timing_init_runtime_info(node);
	gf_smil_anim_init_runtime_info(node);
	gf_smil_anim_set_anim_runtime_in_timing(node);
}



#endif /*GPAC_DISABLE_SVG*/
