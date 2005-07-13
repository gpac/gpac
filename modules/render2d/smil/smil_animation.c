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

/* TODO: handle several begin and end times */

static void SMIL_findInterval(SMIL_AnimationStack *stack, Double sceneTime)
{
	if (stack->status == SMIL_STATUS_STARTUP) {

		stack->simple_duration = -1; /* negative values mean indefinite */
		stack->active_duration = -1;
		stack->is_active_duration_clamped_to_min = 0;
		stack->begin = -1;
		stack->end = -1;

		/* Step 1: determine a value for begin */
		/* if no begin is specified, the default timing is equivalent to an offset value of "0".*/
		if (gf_list_count(*stack->begins)) {
			/* begin values should be sorted first */
			/* assumption there is only one value */
			SMIL_BeginOrEndValue *begin = (SMIL_BeginOrEndValue *)gf_list_get(*stack->begins, 0);
			if (begin->type == SMILBeginOrEnd_offset_value) {
				stack->begin = begin->clock_value;
			}
		} else {
			stack->begin = 0;
		}

		/* Step 2: determine a value for end */
		if (stack->ends && gf_list_count(*stack->ends)) {
			/* end values should be sorted first */
			/* assumption: if there is a value, there is only one value */
			stack->end = ((SMIL_BeginOrEndValue *)gf_list_get(*stack->ends, 0))->clock_value;
		} 
	} else {
		stack->begin = -1;
	}
}

static void SMIL_calcActiveDur(SMIL_AnimationStack *stack)
{
	Bool clamp_active_duration = 1;

	if (!stack->dur) {
		/* for the set and SVG discard element */
		/* active_duration stays indefinite */
		return;
	}

	/* Step 3: determine the simple duration */
	if (stack->dur->type != SMILMinMaxDurRepeatDur_clock_value) {
		/* simple_duration is not defined, so stays indefinite */
		/* we can ignore repeatCount to compute active_duration */

		/* Step 2: determine the active duration */
		if (stack->repeatDur->type != SMILMinMaxDurRepeatDur_clock_value) {
			/* active_duration stays indefinite */
		} else {
			stack->active_duration = stack->repeatDur->clock_value;
		}
	} else {
		/* simple_duration is defined */
		stack->simple_duration = stack->dur->clock_value;

		/* Step 2: determine the active duration */
		if (*(stack->repeatCount) < 0) {
			/* use repeatDur only to determine the active duration */
			if (stack->repeatDur->type != SMILMinMaxDurRepeatDur_clock_value) {
				/* active_duration stays indefinite */
			} else {
				stack->active_duration = stack->repeatDur->clock_value;
			}
		} else {
			if (stack->repeatDur->type != SMILMinMaxDurRepeatDur_clock_value) {
				/* use repeatCount only to determine the active duration */
				stack->active_duration = FIX2FLT(*stack->repeatCount) * stack->simple_duration;
			} else {
				/* use repeatCount and repeatDur to determine the active duration */
				stack->active_duration = MIN(stack->repeatDur->clock_value, FIX2FLT(*stack->repeatCount) * stack->simple_duration);
			}
		}
	}

	/* Step 4: clamp the active duration with min and max */
	/* testing for presence of min and max because some elements may not have them: eg SVG audio */
	if (stack->max && stack->min && stack->max->type == SMILMinMaxDurRepeatDur_clock_value && stack->min->type == SMILMinMaxDurRepeatDur_clock_value) {
		if (stack->max->clock_value < stack->min->clock_value) clamp_active_duration = 0;
	}
	if (clamp_active_duration) {
		if (stack->min && stack->min->type == SMILMinMaxDurRepeatDur_clock_value) {
			if (stack->active_duration >= 0 && stack->active_duration <= stack->min->clock_value) {
				stack->is_active_duration_clamped_to_min = 1;
				stack->active_duration = stack->min->clock_value;
			}
		}
		if (stack->max && stack->max->type == SMILMinMaxDurRepeatDur_clock_value) {
			stack->active_duration = MIN(stack->max->clock_value, stack->active_duration);
		}
	}

	/* Step 5: if end is defined, clamp active duration to end-begin */
	if (stack->end >= 0) {
		stack->active_duration = MIN(stack->active_duration, (stack->end - stack->begin));
	} else {
		/* so that we have an end */
		stack->end = stack->begin + stack->active_duration;
	}
}

void *SMIL_GetLastSpecifiedValue(SMIL_AnimationStack *stack)
{
	void *value;
	if (stack->values && gf_list_count(stack->values->values)) {
		/* Ignore from/to/by*/
		u32 nbValues = gf_list_count(stack->values->values);
		value = gf_list_get(stack->values->values, nbValues - 1);
	} else { 
		/* Use 'from'/'to'/'by'*/
		if (stack->to && stack->to->datatype != 0) {
			value = stack->to->value;
		} else {
			/* TODO: no 'values', no 'to' ? */
			fprintf(stderr, "Error: Cannot compute last specified value on 'to'-less animation\n");
			value = stack->from->value;
		}
	}
	return value;
}

static void SMIL_AnimWithValues(SMIL_AnimationStack *stack, Double sceneTime) 
{
	u32 i;
	u32 nbValues;
	Double activeTime;
	Double simpleTime;
	Fixed normalizedSimpleTime;
	Fixed interval_duration;

	nbValues = gf_list_count(stack->values->values);

	activeTime = sceneTime - stack->begin;
	stack->nb_iterations = (u32)floor(activeTime / stack->simple_duration);
	simpleTime = activeTime - stack->simple_duration * stack->nb_iterations;
	
	/* to be sure clamp cycleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(stack->simple_duration, simpleTime);
	normalizedSimpleTime = FLT2FIX(simpleTime / stack->simple_duration);

	switch (*stack->calcMode) {
	case SMILCalcMode_discrete:
		interval_duration = FLT2FIX(stack->simple_duration) / nbValues;
		if (interval_duration != -1) {
			for (i=0; i<nbValues-1; i++) {
				if (simpleTime <= (i+1)*interval_duration) break; 
			}
			stack->SetDiscreteValueAndAccumulate(stack, gf_list_get(stack->values->values, i));
		} else {
			stack->SetDiscreteValueAndAccumulate(stack, gf_list_get(stack->values->values, 0));
		}
		break;
	case SMILCalcMode_paced:
		/* TODO: at the moment assume it is linear */
		/* Compute the sum of the distances between the consecutive values
		   and the distance of each interval */
		/* Determine in which interval we are based on the distance */
		/* Interpolate using this coefficient */
	case SMILCalcMode_spline:
		/* TODO: at the moment assume it is linear */
	case SMILCalcMode_linear:
		if (stack->keyTimes && gf_list_count(*(stack->keyTimes))) {
			Fixed keyTimeBefore, keyTimeAfter; 
			u32 keyCount = gf_list_count(*(stack->keyTimes));
			for (i=stack->last_keytime_index;i<keyCount;i++) {
				Fixed *t = gf_list_get(*(stack->keyTimes), i);
				if (normalizedSimpleTime < *t) {
					stack->last_keytime_index = i;
					keyTimeBefore = *(Fixed *) gf_list_get(*(stack->keyTimes), i-1);
					keyTimeAfter = *t;
					break;
				}
			}
			interval_duration = keyTimeAfter - keyTimeBefore;
			stack->InterpolateAndAccumulate(stack, gf_divfix(normalizedSimpleTime - keyTimeBefore, interval_duration),
							gf_list_get(stack->values->values, i-1),
							gf_list_get(stack->values->values, i));
		} else {
			interval_duration = FLT2FIX(stack->simple_duration) / (nbValues-1);
			if (interval_duration != -1) {
				for (i=0; i<nbValues; i++) {
					if (simpleTime < (i+1)*interval_duration) break; 
				}
				stack->InterpolateAndAccumulate(stack, gf_divfix(FLT2FIX(simpleTime) - i*interval_duration, interval_duration) ,
								gf_list_get(stack->values->values, i),
								gf_list_get(stack->values->values, i+1));
			} else {
				stack->SetDiscreteValueAndAccumulate(stack, gf_list_get(stack->values->values, 0));
			}
		}
		break;
	}
}

static void SMIL_AnimWithFromToBy(SMIL_AnimationStack *stack, Double sceneTime) 
{
	Double activeTime;
	Double simpleTime;

	activeTime = sceneTime - stack->begin;
	stack->nb_iterations = (u32)floor(activeTime / stack->simple_duration);
	simpleTime = activeTime - stack->simple_duration * stack->nb_iterations;
	
	/* to be sure clamp cycleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(stack->simple_duration, simpleTime);

	/* if to is specified, ignore 'by' */
	if (stack->to->datatype != 0) {
		/* use 'to' */
		if (stack->from->datatype != 0) {
			/* simple 'from'-'to' animation */
			/* TODO: timing may use keyTimes/keySplines ... */
			switch (*stack->calcMode) {
			case SMILCalcMode_discrete:
				/* before half of the duration stay at 'from' and then switch to 'to' */
				stack->SetDiscreteValueAndAccumulate(stack, (simpleTime<=stack->simple_duration/2?stack->from->value:stack->to->value));
				break;
			case SMILCalcMode_linear:
				stack->InterpolateAndAccumulate(stack, FLT2FIX(simpleTime/stack->simple_duration),
												stack->from->value, stack->to->value);
				break;
			case SMILCalcMode_paced:
				stack->InterpolateAndAccumulate(stack, FLT2FIX(simpleTime/stack->simple_duration),
												stack->from->value, stack->to->value);
				break;
			case SMILCalcMode_spline:
				stack->InterpolateAndAccumulate(stack, FLT2FIX(simpleTime/stack->simple_duration),
												stack->from->value, stack->to->value);
				break;
			}
		} else {
			/* animation with 'to' but without 'from' */
			/* warning: additional constraints */
			/* TODO: */
		}
	} else {
		/* animation without to */
		/* use by : this may be supported only with attributes supporting addition*/
		if (stack->from->datatype != 0) {
			/* from-by animation */
		} else {
			if (stack->by->datatype != 0) {
				/* by animation */
				/* warning: additional constraints */
			} else {
				/* no animation */
				return;
			}
		}
	}
}

void SMIL_Update_Animation(GF_TimeNode *timenode)
{
	Double sceneTime = 0;
	SMIL_AnimationStack *stack = gf_node_get_private((GF_Node *)timenode->obj);

	if (stack->status == SMIL_STATUS_STARTUP) {
		/* TODO: SaveBaseValue should be done at Init of the stack not at the start of each animation 
		 Should we come again in the STARTUP state ?
		 if yes, save is broken
		 if no, save is correct but anyway could be done at stack init 
		*/
		//fprintf(stdout, "Saving base value ...\n");
		stack->SaveBaseValue(stack);
	}

	sceneTime = gf_node_get_scene_time(timenode->obj);

	if (stack->status == SMIL_STATUS_STARTUP || stack->status == SMIL_STATUS_DONE) {
		SMIL_findInterval(stack, sceneTime);
		SMIL_calcActiveDur(stack);
		if (stack->begin != -1) stack->status = SMIL_STATUS_WAITING_TO_BEGIN;
		else return;
	}


	if (stack->status == SMIL_STATUS_WAITING_TO_BEGIN) {
		if (sceneTime >= stack->begin) stack->status = SMIL_STATUS_ACTIVE;
		else return;
	}

	if (stack->status == SMIL_STATUS_ACTIVE && 
		stack->active_duration >= 0 && 
		sceneTime >= (stack->begin + stack->active_duration)) {
		stack->status = SMIL_STATUS_POST_ACTIVE;
	}

	if (stack->status == SMIL_STATUS_POST_ACTIVE) {
		if (*stack->fill == SMILFill_freeze) {
			//fprintf(stdout, "setting final animation value\n");
			stack->RestoreValue(stack, 0);
		} else {
			//fprintf(stdout, "resetting to initial animation value\n");
			stack->RestoreValue(stack, 1);
		}
		stack->status = SMIL_STATUS_DONE;
	} else if (stack->status == SMIL_STATUS_ACTIVE) {
		if (stack->values && gf_list_count(stack->values->values)) {
			/* Ignore 'from'/'to'/'by'*/
			SMIL_AnimWithValues(stack, sceneTime);
		} else { 
			/* Use 'from'/'to'/'by'*/
			if (stack->from && stack->to && stack->by) { 
				/* the animation element has 'from', 'by' and 'to' attributes (not necessarily with a value) */
				SMIL_AnimWithFromToBy(stack, sceneTime);
			} else {
				if (stack->to) {
					/* the animation element has no 'to' attribute */
					/* This is a 'set' element */
					stack->SetDiscreteValueAndAccumulate(stack, stack->to->value);
				} else {
					/* the animation element has no 'to' attributes*/
					/* this is SVG discard element */
					GF_Node *targetNode = (GF_Node *)stack->target_element;
					if (sceneTime >= stack->begin) {
						/* TODO: check if a node can only have one parent so index is 0 
						   case of the use element ?!*/
						gf_node_unregister(targetNode, gf_node_get_parent(targetNode, 0));				
					}
				}
			}
		}
	}
	/*NOTE: node invalidation in SVG+SMILAnim is done at the SVG animation subclassing, to differenciate 
	between animations modifying geometry and others*/
}

static void SMIL_Destroy_AnimationStack(GF_Node *node)
{
	SMIL_AnimationStack *stack = gf_node_get_private(node);
	if (stack->time_handle.is_registered) {
		gf_sr_unregister_time_node(stack->compositor, &stack->time_handle);
	}
	stack->DeleteStack(stack);
	free(stack);
}

SMIL_AnimationStack *SMIL_Init_AnimationStack(Render2D *sr, GF_Node *node, void (*UpdateTimeNode)(GF_TimeNode *))
{
	SMIL_AnimationStack *stack;
	SAFEALLOC(stack, sizeof(SMIL_AnimationStack))
	stack->time_handle.UpdateTimeNode = UpdateTimeNode;
	stack->time_handle.obj = node;
	stack->compositor = sr->compositor;
	stack->status = SMIL_STATUS_STARTUP;
	stack->last_keytime_index = 1;

	gf_node_set_private(node, stack);
	gf_node_set_predestroy_function(node, SMIL_Destroy_AnimationStack);	
	gf_sr_register_time_node(stack->compositor, &stack->time_handle);
	return stack;
}

void SMIL_Modified_Animation(GF_Node *node)
{
	SMIL_AnimationStack *stack = (SMIL_AnimationStack *) gf_node_get_private(node);
	if (!stack) return;

	SMIL_Update_Animation(&stack->time_handle);

	stack->time_handle.needs_unregister = 0;
	if (!stack->time_handle.is_registered) {
		gf_sr_register_time_node(stack->compositor, &stack->time_handle);
	}
}

#endif // GPAC_DISABLE_SMIL


