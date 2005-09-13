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

/* New Timing functions */
/* 
  We suppose that the time value to compare to is either a real clock value or an unresolved one.
  We suppose that all wallclock value have been converted to clock value (i.e. using document begin wallclock)
  We suppose also that the begin values are sorted.
*/
static void SMIL_getTimeValueGreaterThan(GF_List *timeValues, SMIL_BeginOrEndValue toCompare, Bool strict, SMIL_BeginOrEndValue *result)
{
	u32 i;
	u32 count = gf_list_count(timeValues);
	if (toCompare.type >= SMILBeginOrEnd_event) {
		/* The time value to compare to is either unresolved or indefinite, nothing will be greater */
		result->type = SMILBeginOrEnd_indefinite;
		result->clock = 0;
		return;
	}
	for (i = 0; i < count; i++) {
		SMIL_BeginOrEndValue *b = gf_list_get(timeValues, i);
		if (b->type <= SMILBeginOrEnd_wallclock) {
			if ((strict && b->clock > toCompare.clock) 
			||  (!strict && b->clock >= toCompare.clock)) {
				/* found a value */
				result->type  = b->type;
				result->clock = b->clock;
				return;
			}
		} else {
			/* found an unresolved value */
			result->type  = b->type;
			result->clock = b->clock;
			return;
		}
	}

	result->type = SMILBeginOrEnd_indefinite;
	result->clock = 0;
}

static void SMIL_calcActiveDur2(SMIL_AnimationStack *stack)
{
	Bool clamp_active_duration = 1;

	if (!stack->dur) {
		/* for the SVG discard element */
		/* active_duration stays indefinite */
		return;
	}

	/* Step 3: determine the simple duration */
	if (stack->dur->type != SMILMinMaxDurRepeatDur_value) {
		/* simple_duration is not defined, so stays indefinite */
		/* we can ignore repeatCount to compute active_duration */

		/* Step 2: determine the active duration */
		if (stack->repeatDur->type != SMILMinMaxDurRepeatDur_value) {
			/* active_duration stays indefinite */
			stack->active_duration = GF_MAX_DOUBLE;
		} else {
			stack->active_duration = stack->repeatDur->clock_value;
		}
	} else {
		/* simple_duration is defined */
		stack->simple_duration = stack->dur->clock_value;

		/* Step 2: determine the active duration */
		if (*(stack->repeatCount) < 0) {
			/* use repeatDur only to determine the active duration */
			if (stack->repeatDur->type != SMILMinMaxDurRepeatDur_value) {
				/* active_duration stays indefinite */
			} else {
				stack->active_duration = stack->repeatDur->clock_value;
				*(stack->repeatCount) = FLT2FIX(stack->repeatDur->clock_value/stack->simple_duration);
			}
		} else {
			if (stack->repeatDur->type != SMILMinMaxDurRepeatDur_value) {
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
	if (stack->max && stack->min && stack->max->type == SMILMinMaxDurRepeatDur_value && stack->min->type == SMILMinMaxDurRepeatDur_value) {
		if (stack->max->clock_value < stack->min->clock_value) clamp_active_duration = 0;
	}
	if (clamp_active_duration) {
		if (stack->min && stack->min->type == SMILMinMaxDurRepeatDur_value) {
			if (stack->active_duration >= 0 && stack->active_duration <= stack->min->clock_value) {
				stack->is_active_duration_clamped_to_min = 1;
				stack->active_duration = stack->min->clock_value;
			}
		}
		if (stack->max && stack->max->type == SMILMinMaxDurRepeatDur_value) {
			stack->active_duration = MIN(stack->max->clock_value, stack->active_duration);
		}
	}

	/* Step 5: if end is defined, clamp active duration to end-begin */
	if (stack->currentInterval.end.type < SMILBeginOrEnd_event) {
		if (stack->currentInterval.begin.type < SMILBeginOrEnd_event) {
			stack->active_duration = MIN(stack->active_duration, (stack->currentInterval.end.clock - stack->currentInterval.begin.clock ));
		} else {
			fprintf(stderr, "begin is indefinite but end is finite ... ");
		}
	} else {
		/* so that we have an end */
		if (stack->currentInterval.begin.type < SMILBeginOrEnd_event) {
			stack->currentInterval.end.type = SMILBeginOrEnd_clock;
			stack->currentInterval.end.clock = stack->currentInterval.begin.clock + stack->active_duration;
		} else {
			fprintf(stderr, "begin and end are indefinite ... ");
		}
	}
}

/* function defined according to SMIL Anim spec */
static void SMIL_findFirstInterval2(SMIL_AnimationStack *stack, Double sceneTime, SMIL_Interval *interval)
{
	/* Assumption that begin and end values are sorted
	   with unresolved > indefinite > any time */
	SMIL_BeginOrEndValue tmpBegin, tmpEnd;
	SMIL_BeginOrEndValue beginAfter;

	memset(&tmpBegin, 0, sizeof(SMIL_BeginOrEndValue));
	memset(&tmpEnd, 0, sizeof(SMIL_BeginOrEndValue));
	memset(&beginAfter, 0, sizeof(SMIL_BeginOrEndValue));

	if (stack->currentInterval.is_valid) {
		/* we already had an animation running, now we look for the next one */
		beginAfter.clock = stack->currentInterval.end.clock;
		beginAfter.type  = SMILBeginOrEnd_clock;
	} else {
		/* first time this animation runs */
		beginAfter.clock = GF_MIN_DOUBLE;
		beginAfter.type  = SMILBeginOrEnd_clock;
	}

	stack->currentInterval.end.type = SMILBeginOrEnd_indefinite;

	while (1) {
		SMIL_getTimeValueGreaterThan(*stack->begins, beginAfter, 0, &tmpBegin);
		if (tmpBegin.type >= SMILBeginOrEnd_event) {
			/* Interval not found since it starts either by an unresolved value or an indefinite one */
			interval->is_valid = 0;
			return;
		} else {
			stack->currentInterval.begin.type = tmpBegin.type;
			stack->currentInterval.begin.clock = tmpBegin.clock;
		}
		if (gf_list_count(*stack->ends) == 0) {
			SMIL_calcActiveDur2(stack);
			tmpEnd.type = stack->currentInterval.end.type;
			tmpEnd.clock = stack->currentInterval.end.clock;

		} else {
			SMIL_getTimeValueGreaterThan(*stack->ends, tmpBegin, 0, &tmpEnd);
			if (tmpBegin.clock == tmpEnd.clock) {
				if (tmpEnd.clock == stack->currentInterval.end.clock) {
					SMIL_getTimeValueGreaterThan(*stack->ends, tmpBegin, 1, &tmpEnd);					
				}
			}
		}
		if ((tmpEnd.type < SMILBeginOrEnd_event && tmpEnd.clock > 0) 
			|| (tmpEnd.type >= SMILBeginOrEnd_event)) {
			interval->is_valid = 1;
			interval->allocation_cycle = stack->cycle_number;
			interval->begin = tmpBegin;
			interval->end = tmpEnd;
			return;
		} else if (*(stack->restart) == SMILRestart_never) {
			interval->is_valid = 0;
			return;
		} else {
			beginAfter.type = tmpEnd.type;
			beginAfter.clock = tmpEnd.clock;
		}
	}
}

/* returns 0 if the two intervals are the same (except cycle_number)
   returns 1 otherwise */
static u32 compareInterval(SMIL_Interval a, SMIL_Interval b)
{
	if (a.is_valid == 0 || b.is_valid == 0) return 1;
	if (a.begin.type != b.begin.type) return 1;
	if (a.end.type != b.end.type) return 1;
	switch(a.begin.type) {
	case SMILBeginOrEnd_event:
		if (a.begin.event != b.begin.event) return 1;
		if (a.begin.parameter != b.begin.parameter) return 1;
		if (a.begin.element != b.begin.element) return 1;
		/* no break; because events can have an offset */
	case SMILBeginOrEnd_clock:
	case SMILBeginOrEnd_wallclock:
		if (a.begin.clock != b.begin.clock) return 1;
		break;
	case SMILBeginOrEnd_indefinite:
	default:
		/* nothing to do */
		break;
	}
	switch(a.end.type) {
	case SMILBeginOrEnd_event:
		if (a.end.event != b.end.event) return 1;
		if (a.end.parameter != b.end.parameter) return 1;
		if (a.end.element != b.end.element) return 1;
		/* no break; because events can have an offset */
	case SMILBeginOrEnd_clock:
	case SMILBeginOrEnd_wallclock:
		if (a.end.clock != b.end.clock) return 1;
		break;
	case SMILBeginOrEnd_indefinite:
	default:
		/* nothing to do */
		break;
	}
	return 0;
}

/************************** OLD Code *********************************/

static void SMIL_findFirstInterval(SMIL_AnimationStack *stack, Double sceneTime)
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
			if (begin->type == SMILBeginOrEnd_clock) {
				stack->begin = begin->clock;
			}
		} else {
			stack->begin = 0;
		}

		/* Step 2: determine a value for end */
		if (stack->ends && gf_list_count(*stack->ends)) {
			/* end values should be sorted first */
			/* assumption: if there is a value, there is only one value */
			stack->end = ((SMIL_BeginOrEndValue *)gf_list_get(*stack->ends, 0))->clock;
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
	if (stack->dur->type != SMILMinMaxDurRepeatDur_value) {
		/* simple_duration is not defined, so stays indefinite */
		/* we can ignore repeatCount to compute active_duration */

		/* Step 2: determine the active duration */
		if (stack->repeatDur->type != SMILMinMaxDurRepeatDur_value) {
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
			if (stack->repeatDur->type != SMILMinMaxDurRepeatDur_value) {
				/* active_duration stays indefinite */
			} else {
				stack->active_duration = stack->repeatDur->clock_value;
			}
		} else {
			if (stack->repeatDur->type != SMILMinMaxDurRepeatDur_value) {
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
	if (stack->max && stack->min && stack->max->type == SMILMinMaxDurRepeatDur_value && stack->min->type == SMILMinMaxDurRepeatDur_value) {
		if (stack->max->clock_value < stack->min->clock_value) clamp_active_duration = 0;
	}
	if (clamp_active_duration) {
		if (stack->min && stack->min->type == SMILMinMaxDurRepeatDur_value) {
			if (stack->active_duration >= 0 && stack->active_duration <= stack->min->clock_value) {
				stack->is_active_duration_clamped_to_min = 1;
				stack->active_duration = stack->min->clock_value;
			}
		}
		if (stack->max && stack->max->type == SMILMinMaxDurRepeatDur_value) {
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

/* end of OLD timing functions */

/* beginning of animation functions */
void *SMIL_GetLastSpecifiedValue(SMIL_AnimationStack *stack)
{
	void *value = NULL;
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

/* GPAC entry point into the SMIL animation model */
void SMIL_Update_Animation(GF_TimeNode *timenode)
{
	SMIL_AnimationStack *stack = gf_node_get_private((GF_Node *)timenode->obj);
	Double sceneTime = gf_node_get_scene_time(timenode->obj);

	stack->cycle_number++;
	fprintf(stdout, "Scene Time: %f - Anim: %8x, Status: %d\n", sceneTime, stack, stack->status);

	if (stack->status == SMIL_STATUS_STARTUP) {
		stack->SaveBaseValue(stack);
		SMIL_findFirstInterval2(stack, sceneTime, &(stack->currentInterval));
		if (stack->currentInterval.is_valid) stack->status = SMIL_STATUS_WAITING_TO_BEGIN;
	} 

waiting_to_begin:
	if (stack->status == SMIL_STATUS_WAITING_TO_BEGIN) {
		if (0) { /* Check changes in begin or end attributes */
			SMIL_Interval tmpInterval;
			SMIL_findFirstInterval2(stack, sceneTime, &tmpInterval);
			if (compareInterval(tmpInterval, stack->currentInterval)) {
				/* intervals are different */
				/* TODO notify time dependencies */
			} 
		}
		if (stack->currentInterval.begin.type <= SMILBeginOrEnd_wallclock &&
			sceneTime >= stack->currentInterval.begin.clock) stack->status = SMIL_STATUS_ACTIVE;
	}

	if (stack->status == SMIL_STATUS_ACTIVE) {
		if (0) { /* Check changes in begin or end attributes */
			SMIL_Interval tmpInterval;
			SMIL_findFirstInterval2(stack, sceneTime, &tmpInterval);
			if (compareInterval(tmpInterval, stack->currentInterval)) {
				/* intervals are different */
				/* TODO notify time dependencies */
			} 
		}
		/* Perform the animation */
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
					if (sceneTime >= stack->currentInterval.begin.clock) {
						/* TODO: check if a node can only have one parent so index is 0 
						   case of the use element ?!*/
						gf_node_unregister(targetNode, gf_node_get_parent(targetNode, 0));				
					}
				}
			}
		}

		if (
			(stack->currentInterval.end.type <= SMILBeginOrEnd_wallclock &&
			sceneTime >= stack->currentInterval.end.clock)
			||
			!stack->dur) stack->status = SMIL_STATUS_POST_ACTIVE;
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
	}

	if (stack->status == SMIL_STATUS_DONE) {
		SMIL_Interval tmpInterval;
		SMIL_findFirstInterval2(stack, sceneTime, &tmpInterval);
		if (tmpInterval.is_valid && !compareInterval(tmpInterval, stack->currentInterval)) {
			stack->currentInterval.allocation_cycle = tmpInterval.allocation_cycle;
			stack->currentInterval.is_valid = 1;
			stack->currentInterval.begin = tmpInterval.begin;
			stack->currentInterval.end = tmpInterval.end;
			stack->status = SMIL_STATUS_WAITING_TO_BEGIN;
			goto waiting_to_begin;
		}
	}

}

void SMIL_Update_Animation2(GF_TimeNode *timenode)
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
		SMIL_findFirstInterval2(stack, sceneTime, &(stack->currentInterval));
		SMIL_findFirstInterval(stack, sceneTime);
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
	GF_SAFEALLOC(stack, sizeof(SMIL_AnimationStack))
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
/* end of animation functions */

#endif // GPAC_DISABLE_SMIL


