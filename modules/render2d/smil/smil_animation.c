/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / SMIL Rendering sub-project
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
#include "smil_stacks.h"

#ifndef GPAC_DISABLE_SMIL

/* computes the active duration for the given interval,
   assumes that the values of smil_begin and smil_end have been set
   and that begin is defined (i.e. a positive value)*/
static void SMIL_calcActiveDur(SMIL_AnimationStack *stack, SMIL_Interval *interval)
{
	Bool clamp_active_duration = 1;

	if (!stack->dur) {
		/* for the SVGdiscardElement */
		interval->active_duration = -1;
		return;
	}

	/* Step 1: Computing active duration using repeatDur and repeatCount */
	if (stack->dur->type == SMIL_DURATION_DEFINED) { /* simple_duration is defined */

		interval->simple_duration = stack->dur->clock_value;

		if (stack->repeatCount->type == SMIL_REPEATCOUNT_INDEFINITE ||
			stack->repeatDur->type == SMIL_DURATION_INDEFINITE) {
				interval->active_duration = -1;
		} else {
			if (stack->repeatCount->type == SMIL_REPEATCOUNT_DEFINED 
				&& stack->repeatDur->type != SMIL_DURATION_DEFINED) {
				interval->active_duration = FIX2FLT(stack->repeatCount->count) * interval->simple_duration;
			} else if (stack->repeatCount->type != SMIL_REPEATCOUNT_DEFINED 
					   && stack->repeatDur->type == SMIL_DURATION_DEFINED) {
				interval->active_duration = stack->repeatDur->clock_value;
			} else if (stack->repeatCount->type == SMIL_REPEATCOUNT_UNSPECIFIED 
					   && stack->repeatDur->type == SMIL_DURATION_UNSPECIFIED) {
				interval->active_duration = interval->simple_duration;
			} else {
				interval->active_duration = MIN(stack->repeatDur->clock_value, 
												FIX2FLT(stack->repeatCount->count) * interval->simple_duration);
			}			
		}
	} else {
		/* simple_duration is indefinite (it cannot be unspecified) */
		interval->simple_duration = -1;
		
		/* we can ignore repeatCount to compute active_duration */

		if (stack->repeatDur->type == SMIL_DURATION_UNSPECIFIED ||
			stack->repeatDur->type == SMIL_DURATION_INDEFINITE) {
			interval->active_duration = -1;
		} else { // (stack->repeatDur->type != SMIL_DURATION_DEFINED)
			interval->active_duration = stack->repeatDur->clock_value;
		}
	}

	/* Step 2: clamp the active duration with min and max */
	/* testing for presence of min and max because some elements may not have them: eg SVG audio */
	if (stack->max 
		&& stack->min 
		&& stack->max->type == SMIL_DURATION_DEFINED 
		&& stack->min->type == SMIL_DURATION_DEFINED
		&& stack->max->clock_value < stack->min->clock_value) {
		clamp_active_duration = 0;
	}
	if (clamp_active_duration) {
		if (stack->min && stack->min->type == SMIL_DURATION_DEFINED) {
			if (interval->active_duration >= 0 
				&& interval->active_duration <= stack->min->clock_value) {
				interval->active_duration = stack->min->clock_value;
			}
		}
		if (stack->max 
			&& stack->max->type == SMIL_DURATION_DEFINED) {
			interval->active_duration = MIN(stack->max->clock_value, interval->active_duration);
		}
	}

	/* Step 3: if end is defined in the document, clamp active duration to end-begin 
	otherwise return*/
	if (interval->end < 0) {
		/* interval->active_duration stays as is */
	} else {
		if (interval->active_duration >= 0)
			interval->active_duration = MIN(interval->active_duration, interval->end - interval->begin);
		else 
			interval->active_duration = interval->end - interval->begin;
	}
}

/* assumes that the list of begin values is sorted */
static void SMIL_initIntervalList(SMIL_AnimationStack *stack)
{
	u32 i, j, begin_count, end_count;
	SMIL_Time *begin, *end;
	SMIL_Interval *interval;

	gf_list_reset(stack->intervals);
	begin_count = gf_list_count(*stack->begins);
	end_count = gf_list_count(*stack->ends);
	if (begin_count) {
		for (i = 0; i < begin_count; i ++) {
			begin = gf_list_get(*stack->begins, i);
			if (begin->type < SMIL_TIME_EVENT) {
				/* we create an acceptable only if begin is unspecified or defined (clock or wallclock) */
				GF_SAFEALLOC(interval, sizeof(SMIL_Interval));
				gf_list_add(stack->intervals, interval);
				interval->begin = begin->clock;
				/* trying to find a matching end */
				if (end_count > i) {
					for (j = 0; j < end_count; j++) {
						end = gf_list_get(*stack->ends, j);
						if (end->type == SMIL_TIME_CLOCK || 
							end->type == SMIL_TIME_WALLCLOCK) {
							if( end->clock >= interval->begin) {
								interval->end = end->clock;
								break;
							}
						} else {
							/* an unresolved or indefinite value is always good */
							interval->end = -1;
							break;
						}
					}	
				} else {
					interval->end = -1;
				}
				SMIL_calcActiveDur(stack, interval);
			} else {
				/* this is not an acceptable value for a begin */
			}
		} 
	} else {
		GF_SAFEALLOC(interval, sizeof(SMIL_Interval));
		gf_list_add(stack->intervals, interval);
		interval->begin = 0;
		/* trying to find a matching end */
		if (end_count > 0) {
			for (j = 0; j < end_count; j++) {
				end = gf_list_get(*stack->ends, j);
				if (end->type == SMIL_TIME_CLOCK || end->type == SMIL_TIME_WALLCLOCK) {
					if (end->clock >= interval->begin) {
						interval->end = end->clock;
						break;
					}
				} else {
					/* an unresolved or indefinite value is always good */
					interval->end = -1;
					break;
				}
			}	
		} else {
			interval->end = -1;
		}
		SMIL_calcActiveDur(stack, interval);
	}
}

static void printInterval(SMIL_Interval *interval)
{
	fprintf(stdout, "Current Interval - ");
	fprintf(stdout, "Begin: %f", interval->begin);
	fprintf(stdout, " - End: %f", interval->end);
	fprintf(stdout, "\n");
	fprintf(stdout, "Duration - Simple %f, Active %f\n",interval->simple_duration, interval->active_duration);
	fprintf(stdout, "\n");
}

static s32 SMIL_findIntervalIndex(SMIL_AnimationStack *stack, Double sceneTime)
{
	s32 index = -1;
	u32 i, count;
	count = gf_list_count(stack->intervals);
	if (stack->current_interval) i = stack->current_interval_index + 1;
	else i = 0;
	for (; i<count; i++) {
		SMIL_Interval *interval = gf_list_get(stack->intervals, i);
		if (interval->begin <= sceneTime) {
			index = i;
			break;
		}
	}
	return index;
}

/* beginning of animation functions */
void *SMIL_GetLastSpecifiedValue(SMIL_AnimationStack *stack)
{
	void *value = NULL;
	if (stack->values && gf_list_count(stack->values->values)) {
		/* Ignore from/to/by*/
		u32 nbValues = gf_list_count(stack->values->values);
		value = gf_list_get(stack->values->values, nbValues - 1);
	} else if (stack->path) {
		fprintf(stderr, "Error: Cannot compute last specified value for path animation\n");
	} else { 
		/* Use 'from'/'to'/'by'*/
		if (stack->to && stack->to->datatype != 0) {
			value = stack->to->value;
		} else {
			/* TODO: no 'values', no 'to' ? */
			fprintf(stderr, "Error: Cannot compute last specified value on 'to'-less animation\n");
		}
	}
	return value;
}

static void SMIL_ApplyAccumulate(SMIL_AnimationStack *stack)
{
	if (stack->accumulate 
		&& *stack->accumulate == SMIL_ACCUMULATE_SUM
		&& stack->current_interval->nb_iterations > 0) {
		void *last_specified_value = SMIL_GetLastSpecifiedValue(stack);
		if (last_specified_value)
			stack->ApplyAccumulate(stack->current_interval->nb_iterations, stack->tmp_value, last_specified_value, stack->tmp_value);
	}
}

static void SMIL_ApplyAdditive(SMIL_AnimationStack *stack)
{
	if (stack->additive && *stack->additive == SMIL_ADDITIVE_SUM) {
		Bool attribute_already_animated = 0;
		u32 i, count;
		count = gf_list_count(stack->compositor->svg_animated_attributes);
		for (i = 0; i < count; i++) {
			void *anAttr = gf_list_get(stack->compositor->svg_animated_attributes, i);
			if (anAttr == stack->targetAttribute) {
				attribute_already_animated = 1;
				break;
			}
		}
		if (!attribute_already_animated) { /* first animation in this rendering cycle, use DOM value as underlying value */
			stack->ApplyAdditive(stack->base_value, stack->tmp_value, stack->targetAttribute);
		} else { /* this is not the first animation, we should sum using the presentation value as underlying value */
			stack->ApplyAdditive(stack->targetAttribute, stack->tmp_value, stack->targetAttribute);
		}
	} else { // SMILAdditiveValue_replace
		stack->Assign(stack->targetAttribute, stack->tmp_value);
	}
}

void SMIL_AnimWithValues(SMIL_AnimationStack *stack, Double sceneTime) 
{
	u32 keyValueIndex;
	u32 nbValues;
	Double activeTime;
	Double simpleTime;
	Fixed normalizedSimpleTime;
	Fixed interval_duration;
	Fixed interpolation_coefficient;
	void *target = stack->tmp_value;

	nbValues = gf_list_count(stack->values->values);
	if (nbValues == 1) {
		if (stack->previous_key_index != 0) {
			stack->Set(target, gf_list_get(stack->values->values, 0));
			stack->previous_key_index = 0;
			stack->target_value_changed = 1;
		}
		return;
	}

	activeTime			 = sceneTime - stack->current_interval->begin;
	stack->current_interval->nb_iterations = (u32)floor(activeTime / stack->current_interval->simple_duration);
	simpleTime			 = activeTime - stack->current_interval->simple_duration * stack->current_interval->nb_iterations;
	
	/* to be sure clamp simpleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(stack->current_interval->simple_duration, simpleTime);
	normalizedSimpleTime = FLT2FIX(simpleTime / stack->current_interval->simple_duration);

//	fprintf(stdout, "Simple Time: %.2f, normalized %.2f\n", simpleTime, normalizedSimpleTime);

	if (stack->keyTimes && gf_list_count(*(stack->keyTimes))) {
		u32 keyTimeIndex;
		Fixed keyTimeBefore, keyTimeAfter; 
		u32 keyTimesCount = gf_list_count(*(stack->keyTimes));
		for (keyTimeIndex=stack->last_keytime_index;keyTimeIndex<keyTimesCount;keyTimeIndex++) {
			Fixed *t = gf_list_get(*(stack->keyTimes), keyTimeIndex);
			if (normalizedSimpleTime < *t) {
				stack->last_keytime_index = keyTimeIndex;
				keyTimeBefore = *(Fixed *) gf_list_get(*(stack->keyTimes), keyTimeIndex-1);
				keyTimeAfter = *t;
				break;
			}
		}
		keyTimeIndex--;
		keyValueIndex = keyTimeIndex;
		interval_duration = keyTimeAfter - keyTimeBefore;
		if (interval_duration) interpolation_coefficient = gf_divfix(normalizedSimpleTime - keyTimeBefore, interval_duration);
		else interpolation_coefficient = 1;
//		fprintf(stdout, "Using Key Times: index %d, interval duration %.2f, coeff: %.2f\n", keyTimeIndex, interval_duration, interpolation_coefficient);
	} else {
		if (*stack->calcMode == SMIL_CALCMODE_DISCRETE) {
			interval_duration = FLT2FIX(stack->current_interval->simple_duration) / nbValues;
			keyValueIndex = (u32)floor(normalizedSimpleTime*nbValues);			
		} else {
			interval_duration = FLT2FIX(stack->current_interval->simple_duration) / (nbValues-1);
			keyValueIndex = (u32)floor(normalizedSimpleTime*(nbValues-1));			
		}
		interpolation_coefficient = gf_divfix(FLT2FIX(simpleTime) - keyValueIndex*interval_duration, interval_duration);
//		fprintf(stdout, "Not Using Key Times: key value index %d, interval duration %.2f, coeff: %.2f\n", keyValueIndex, interval_duration, interpolation_coefficient);
	}

	if (stack->keyPoints && gf_list_count(*(stack->keyPoints))) {
		interpolation_coefficient = *(Fixed *)gf_list_get(*(stack->keyPoints), keyValueIndex);
		keyValueIndex = 0;
//		fprintf(stdout, "Using Key Points: key Value Index %d, coeff: %.2f\n", keyValueIndex, interpolation_coefficient);
	}

	if (stack->previous_key_index == keyValueIndex &&
		stack->previous_coef == interpolation_coefficient) return;

	stack->previous_key_index = keyValueIndex;
	stack->target_value_changed = 1;

	switch (*stack->calcMode) {
	case SMIL_CALCMODE_DISCRETE:
		stack->Set(target, gf_list_get(stack->values->values, keyValueIndex));
		break;
	case SMIL_CALCMODE_PACED:
		/* TODO: at the moment assume it is linear */
	case SMIL_CALCMODE_SPLINE:
		/* TODO: at the moment assume it is linear */
	case SMIL_CALCMODE_LINEAR:
		stack->previous_coef = interpolation_coefficient;
		if (keyValueIndex == nbValues - 1) {
			stack->Set(target, gf_list_get(stack->values->values, nbValues - 1));
		} else {
			stack->Interpolate(interpolation_coefficient,
							   gf_list_get(stack->values->values, keyValueIndex),
							   gf_list_get(stack->values->values, keyValueIndex+1),
							   target);
		}
		break;
	}
}

void SMIL_AnimWithFromToBy(SMIL_AnimationStack *stack, Double sceneTime) 
{
	void *target = stack->tmp_value;
	void *fromValue;

	Double activeTime;
	Double simpleTime;
	Fixed interpolation_coefficient;

	activeTime			 = sceneTime - stack->current_interval->begin;
	stack->current_interval->nb_iterations = (u32)floor(activeTime / stack->current_interval->simple_duration);
	simpleTime			 = activeTime - stack->current_interval->simple_duration * stack->current_interval->nb_iterations;
	
	/* to be sure clamp cycleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(stack->current_interval->simple_duration, simpleTime);

	interpolation_coefficient = FLT2FIX(simpleTime/stack->current_interval->simple_duration);
	
	if (*(stack->calcMode) != SMIL_CALCMODE_DISCRETE
		&& stack->previous_coef == interpolation_coefficient) return;

	stack->previous_coef = interpolation_coefficient;

	if (stack->from->datatype != 0) {
		fromValue = stack->from->value;
	} else {
		/* Warning: animation without from uses the underlying value:
			either DOM value or presentation value: TO FIX*/
		fromValue = stack->base_value;
	}

	/* if to is specified, ignore 'by' */
	if (stack->to->datatype != 0) {
		void *toValue = stack->to->value;

		switch (*stack->calcMode) {
		case SMIL_CALCMODE_DISCRETE:
			{
				/* before half of the duration stay at 'from' and then switch to 'to' */
				s32 useFrom = (simpleTime<=stack->current_interval->simple_duration/2);
				if (useFrom == stack->previous_key_index) return;
				stack->Set(target, (useFrom?fromValue:toValue));
				stack->previous_key_index = useFrom;
			}
			break;
		case SMIL_CALCMODE_SPLINE:
		case SMIL_CALCMODE_PACED:
		case SMIL_CALCMODE_LINEAR:
			stack->Interpolate(interpolation_coefficient,
					           fromValue, 
							   toValue,
							   target);
			break;
		}
		stack->target_value_changed = 1;
	} else {
		/* animation without to */
		/* use by : this may be supported only with attributes supporting addition*/
		if (stack->from->datatype != 0) {
			/* from-by animation */
		} else {
			if (stack->by->datatype != 0) {
				/* by animation */
				/* warning: additional constraints */
				/* TODO: */
			} else {
				/* no animation */
				return;
			}
		}
	}
}

void SMIL_AnimSet(SMIL_AnimationStack *stack, Double sceneTime) 
{
	if (stack->to) {
		/* the animation element has no 'from' attribute */
		/* This is a 'set' element */
		if (stack->Compare(stack->tmp_value, stack->to->value)) {
			stack->Set(stack->tmp_value, stack->to->value);
			stack->target_value_changed = 1;
		}
	} else {
		/* the animation element has no 'to' attributes*/
		/* this is SVG discard element */
		GF_Node *targetNode = (GF_Node *)stack->target_element;
		if (sceneTime >= stack->current_interval->begin) {
			/* TODO: check if a node can only have one parent so index is 0 
			   case of the use element ?!*/
			gf_node_unregister(targetNode, gf_node_get_parent(targetNode, 0));				
		}
	}
}


void SMIL_InitAnimateFunction(SMIL_AnimationStack *stack)
{
	/* Perform the animation */
	if (stack->values && gf_list_count(stack->values->values)) {
		/* Ignore 'from'/'to'/'by'*/
		stack->Animate = SMIL_AnimWithValues;
	} else if (stack->path) {
		fprintf(stderr,"Warning: Path animation not supported\n");
		return;
	} else { 
		/* Use 'from'/'to'/'by'*/
		if (stack->from && stack->to && stack->by) { 
			/* the animation element has 'from', 'by' and 'to' attributes (not necessarily with a value) */
			stack->Animate = SMIL_AnimWithFromToBy;
		} else {
			stack->Animate = SMIL_AnimSet;
		}
	}
}

/* GPAC entry point into the SMIL animation model */
void SMIL_Update_Animation(GF_TimeNode *timenode)
{
	SMIL_AnimationStack *stack = gf_node_get_private((GF_Node *)timenode->obj);
	Double sceneTime = gf_node_get_scene_time(timenode->obj);

	stack->cycle_number++;
//	fprintf(stdout, "Scene Time: %f - Anim: %8x, Status: %d\n", sceneTime, stack, stack->status);

	if (stack->status == SMIL_STATUS_STARTUP) {
		s32 interval_index;
		stack->InitStackValues(stack);
		SMIL_initIntervalList(stack);
		stack->current_interval = NULL;
		interval_index = SMIL_findIntervalIndex(stack, GF_MAX_DOUBLE);
		if (interval_index >= 0) {
			stack->status = SMIL_STATUS_WAITING_TO_BEGIN;
			stack->current_interval_index = interval_index;
			stack->current_interval = gf_list_get(stack->intervals, stack->current_interval_index);
//			printInterval(stack->current_interval);
		} else {
			gf_sr_unregister_time_node(stack->compositor, &stack->time_handle);
			gf_sr_invalidate(stack->compositor, NULL);
			stack->time_handle.is_registered = 0;
			//fprintf(stdout, "Current Interval not resolved\n");
			return;
		}
	} 

waiting_to_begin:
	if (stack->status == SMIL_STATUS_WAITING_TO_BEGIN) {
		if (sceneTime >= stack->current_interval->begin) stack->status = SMIL_STATUS_ACTIVE;
		else return;
	}

	if (stack->status == SMIL_STATUS_ACTIVE) {
		if (stack->current_interval->active_duration >= 0 
			&& sceneTime >= (stack->current_interval->begin + stack->current_interval->active_duration)) {
			stack->status = SMIL_STATUS_POST_ACTIVE;
			goto post_active;
		}

		if (*stack->restart == SMIL_RESTART_ALWAYS) {
			s32 interval_index;
			interval_index = SMIL_findIntervalIndex(stack, sceneTime);
			
			if (interval_index >= 0 &&
				interval_index != stack->current_interval_index) {
				/* intervals are different, use the new one */
				stack->current_interval_index = interval_index;
				stack->current_interval = gf_list_get(stack->intervals, stack->current_interval_index);
//				printInterval(stack->current_interval);
				/* TODO notify time dependencies */
			} 
		}

		if (stack->Animate) {		

			stack->Animate(stack, sceneTime);
			SMIL_ApplyAccumulate(stack);
			SMIL_ApplyAdditive(stack);

			/*NOTE: node invalidation in SVG+SMILAnim is done at the SVG animation subclassing, to differenciate 
			between animations modifying geometry and others*/
			if (stack->target_value_changed) {
				stack->Invalidate(stack);
				stack->target_value_changed = 0;
			}
		}

		/* Register the animation in the renderer */
		gf_list_add(stack->compositor->svg_animated_attributes, stack->targetAttribute);
		stack->is_registered_in_cycle = 1;
	}

post_active:
	if (stack->status == SMIL_STATUS_POST_ACTIVE) {
		s32 item;
		if (stack->is_registered_in_cycle && (item = gf_list_find(stack->compositor->svg_animated_attributes, stack->targetAttribute)) > 0) {
			/* Unregister the animation in the renderer */
			gf_list_rem(stack->compositor->svg_animated_attributes, item);
		}

		if (stack->Set && stack->fill) {
			if (*stack->fill == SMIL_FILL_FREEZE) {
				//fprintf(stdout, "setting final animation value\n");
				void * last = SMIL_GetLastSpecifiedValue(stack);
				if (last) {
					stack->current_interval->nb_iterations--;
					SMIL_ApplyAccumulate(stack);
					SMIL_ApplyAdditive(stack);
					if (stack->target_value_changed) {
						stack->Invalidate(stack);
						stack->target_value_changed = 0;
					}
				}
			} else {
				if (stack->Compare(stack->targetAttribute, stack->base_value)) {
					//fprintf(stdout, "resetting to initial animation value\n");
					stack->Assign(stack->targetAttribute, stack->base_value);
					stack->Invalidate(stack);
				}
			}
		}

		/* Register the animation in the renderer */
		gf_list_add(stack->compositor->svg_animated_attributes, stack->targetAttribute);
		stack->is_registered_in_cycle = 1;

		stack->status = SMIL_STATUS_DONE;
//		fprintf(stdout, "Animation done %8x.\n", stack);

		/* return required because we don't want the animation to be unregistered from 
		   the list of animated attributes during this cycle */
		return;
	}

	if (stack->status == SMIL_STATUS_DONE) {
		s32 item;
		if (stack->is_registered_in_cycle && (item = gf_list_find(stack->compositor->svg_animated_attributes, stack->targetAttribute)) >= 0) {
			/* Unregister the animation in the renderer */
			gf_list_rem(stack->compositor->svg_animated_attributes, item);
		}

		if (*stack->restart != SMIL_RESTART_NEVER) { /* Check changes in begin or end attributes */
			s32 interval_index;
			interval_index = SMIL_findIntervalIndex(stack, sceneTime);
			if (interval_index >= 0 && interval_index != stack->current_interval_index) {
				/* intervals are different, use the new one */
				stack->current_interval_index = interval_index;
				stack->current_interval = gf_list_get(stack->intervals, stack->current_interval_index);
//				printInterval(stack->current_interval);
				/* TODO notify time dependencies */
				stack->status = SMIL_STATUS_WAITING_TO_BEGIN;
				goto waiting_to_begin;
			} 
		} else {
			/* when an animation is known to be done (that is, cannot be re-started without external events), 
			these two lines should be called*/
			gf_sr_unregister_time_node(stack->compositor, &stack->time_handle);
			gf_sr_invalidate(stack->compositor, NULL);
			stack->time_handle.is_registered = 0;
		}
	}
}

static void SMIL_Destroy_AnimationStack(GF_Node *node)
{
	SMIL_AnimationStack *stack = gf_node_get_private(node);
	if (stack->time_handle.is_registered) {
		gf_sr_unregister_time_node(stack->compositor, &stack->time_handle);
	}
	while (gf_list_count(stack->intervals) > 0) {
		SMIL_Interval *interval = gf_list_get(stack->intervals, 0);
		gf_list_rem(stack->intervals, 0);
		free(interval);
	}
	gf_list_del(stack->intervals);
	stack->DeleteStack(stack);
	free(stack);
}

SMIL_AnimationStack *SMIL_Init_AnimationStack(Render2D *sr, GF_Node *node)
{
	SMIL_AnimationStack *stack;
	GF_SAFEALLOC(stack, sizeof(SMIL_AnimationStack))
	stack->time_handle.UpdateTimeNode = SMIL_Update_Animation;
	stack->time_handle.obj = node;
	stack->compositor = sr->compositor;
	stack->status = SMIL_STATUS_STARTUP;
	stack->last_keytime_index = 0;
	stack->intervals = gf_list_new();

	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, SMIL_Destroy_AnimationStack);	
	gf_sr_register_time_node(stack->compositor, &stack->time_handle);
	return stack;
}

void SMIL_Modified_Animation(GF_Node *node)
{
	SMIL_AnimationStack *stack = (SMIL_AnimationStack *) gf_node_get_private(node);
	if (!stack) return;

	stack->status = SMIL_STATUS_STARTUP;
	while (gf_list_count(stack->intervals) > 0) {
		SMIL_Interval *interval = gf_list_get(stack->intervals, 0);
		gf_list_rem(stack->intervals, 0);
		free(interval);
	}

	stack->time_handle.needs_unregister = 0;
	if (!stack->time_handle.is_registered) {
		gf_sr_register_time_node(stack->compositor, &stack->time_handle);
	}
}
/* end of animation functions */

#endif // GPAC_DISABLE_SMIL


