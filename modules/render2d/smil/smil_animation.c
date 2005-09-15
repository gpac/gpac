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
static void SMIL_getTimeValueGreaterThan(Bool begin, 
										 GF_List *timeValues, 
										 SMIL_BeginOrEndValue toCompare, 
										 Bool strict, 
										 SMIL_BeginOrEndValue *result)
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

	if (begin) {
		result->type = SMILBeginOrEnd_clock;
		result->clock = 0;
	} else {
		result->type = SMILBeginOrEnd_indefinite;
		result->clock = 0;
	}
}

static void SMIL_calcActiveDur(SMIL_AnimationStack *stack)
{
	Bool clamp_active_duration = 1;

	if (!stack->dur) {
		/* for the SVGdiscardElement */
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
static void SMIL_findInterval(SMIL_AnimationStack *stack, Bool first, Double sceneTime, SMIL_Interval *interval)
{
	/* Assumption that begin and end values are sorted
	   with unresolved > indefinite > any time */
	SMIL_BeginOrEndValue tmpBegin, tmpEnd;
	SMIL_BeginOrEndValue beginAfter;

	memset(&tmpBegin, 0, sizeof(SMIL_BeginOrEndValue));
	memset(&tmpEnd, 0, sizeof(SMIL_BeginOrEndValue));
	memset(&beginAfter, 0, sizeof(SMIL_BeginOrEndValue));

	if (!first) {
		/* we already had an animation running, now we look for the next one */
		beginAfter.clock = interval->end.clock;
		beginAfter.type  = SMILBeginOrEnd_clock;
	} else {
		/* first time this animation runs */
		beginAfter.clock = GF_MIN_DOUBLE;
		beginAfter.type  = SMILBeginOrEnd_clock;
	}

	interval->end.type = SMILBeginOrEnd_indefinite;

	while (1) {
		SMIL_getTimeValueGreaterThan(1, *stack->begins, beginAfter, 0, &tmpBegin);
		if (tmpBegin.type >= SMILBeginOrEnd_event) {
			/* Interval not found since it starts either by an unresolved value or an indefinite one */
			interval->is_valid = 0;
			return;
		} else {
			interval->begin.type = tmpBegin.type;
			interval->begin.clock = tmpBegin.clock;
		}
		if (gf_list_count(*stack->ends) == 0) {
			SMIL_calcActiveDur(stack);
			tmpEnd.type = interval->end.type;
			tmpEnd.clock = interval->end.clock;
		} else {
			SMIL_getTimeValueGreaterThan(0, *stack->ends, tmpBegin, 0, &tmpEnd);
			if (tmpBegin.clock == tmpEnd.clock) {
				if (tmpEnd.clock == interval->end.clock) {
					SMIL_getTimeValueGreaterThan(0, *stack->ends, tmpBegin, 1, &tmpEnd);
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
		&& *stack->accumulate == SMILAccumulateValue_sum 
		&& stack->nb_iterations > 0) {
		void *last_specified_value = SMIL_GetLastSpecifiedValue(stack);
		if (last_specified_value)
			stack->ApplyAccumulate(stack->nb_iterations, stack->tmp_value, last_specified_value, stack->tmp_value);
	}
}

static void SMIL_ApplyAdditive(SMIL_AnimationStack *stack)
{
	if (stack->additive && *stack->additive == SMILAdditiveValue_sum) {
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
	u32 i;
	u32 nbValues;
	Double activeTime;
	Double simpleTime;
	Fixed normalizedSimpleTime;
	Fixed interval_duration;
	void *target;

	nbValues = gf_list_count(stack->values->values);

	activeTime			 = sceneTime - stack->currentInterval.begin.clock;
	simpleTime			 = activeTime - stack->simple_duration * stack->nb_iterations;
	stack->nb_iterations = (u32)floor(activeTime / stack->simple_duration);
	
	/* to be sure clamp simpleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(stack->simple_duration, simpleTime);
	normalizedSimpleTime = FLT2FIX(simpleTime / stack->simple_duration);

	target = stack->tmp_value;

	switch (*stack->calcMode) {
	case SMILCalcMode_discrete:
		interval_duration = FLT2FIX(stack->simple_duration) / nbValues;
		if (nbValues > 1) {
			i = (u32)floor((simpleTime*(nbValues - 1))/stack->simple_duration);	
			stack->Set(target, gf_list_get(stack->values->values, i));
		} else {
			stack->Set(target, gf_list_get(stack->values->values, 0));
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
			if (i == keyCount) i--;
			interval_duration = keyTimeAfter - keyTimeBefore;
			if (stack->keyPoints && gf_list_count(*(stack->keyPoints))) {
				stack->Interpolate(*(Fixed *)gf_list_get(*(stack->keyPoints), i),
								   gf_list_get(stack->values->values, 0),
								   gf_list_get(stack->values->values, 1),
								   target);
			} else {
				stack->Interpolate(gf_divfix(normalizedSimpleTime - keyTimeBefore, interval_duration),
								   gf_list_get(stack->values->values, i-1),
								   gf_list_get(stack->values->values, i),
								   target);
			}
		} else {
			interval_duration = FLT2FIX(stack->simple_duration) / (nbValues-1);
			if (nbValues > 1) {
				i = (u32)floor((simpleTime*(nbValues - 1))/stack->simple_duration);
				//fprintf(stdout, "using key #%d\n", i);
				if (i == nbValues - 1) {
					stack->Set(target, gf_list_get(stack->values->values, nbValues - 1));
				} else {
					stack->Interpolate(gf_divfix(FLT2FIX(simpleTime) - i*interval_duration, interval_duration),
									   gf_list_get(stack->values->values, i),
									   gf_list_get(stack->values->values, i+1),
									   target);
				}
			} else {
				stack->Set(target, gf_list_get(stack->values->values, 0));
			}
		}
		break;
	}
}

void SMIL_AnimWithFromToBy(SMIL_AnimationStack *stack, Double sceneTime) 
{
	Double activeTime;
	Double simpleTime;

	activeTime = sceneTime - stack->currentInterval.begin.clock;
	stack->nb_iterations = (u32)floor(activeTime / stack->simple_duration);
	simpleTime = activeTime - stack->simple_duration * stack->nb_iterations;
	
	/* to be sure clamp cycleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(stack->simple_duration, simpleTime);

	/* if to is specified, ignore 'by' */
	if (stack->to->datatype != 0) {
		/* use 'to' */
		void *fromValue, *toValue, *target;

		target = stack->tmp_value;

		if (stack->from->datatype != 0) {
			/* simple 'from'-'to' animation */
			fromValue = stack->from->value;
		} else {
			/* animation with 'to' but without 'from' */
			/* warning: additional constraints */
			/* TODO: */
			fromValue = stack->base_value;
		}

		toValue = stack->to->value;

		/* TODO: timing may use keyTimes/keySplines ... */
		switch (*stack->calcMode) {
		case SMILCalcMode_discrete:
			/* before half of the duration stay at 'from' and then switch to 'to' */
			stack->Set(target, (simpleTime<=stack->simple_duration/2?fromValue:toValue));
			break;
		case SMILCalcMode_linear:
			stack->Interpolate(FLT2FIX(simpleTime/stack->simple_duration),
					           fromValue, 
							   toValue,
							   target);
			break;
		case SMILCalcMode_paced:
			stack->Interpolate(FLT2FIX(simpleTime/stack->simple_duration),
					           fromValue, 
							   toValue,
							   target);
			break;
		case SMILCalcMode_spline:
			stack->Interpolate(FLT2FIX(simpleTime/stack->simple_duration),
					           fromValue, 
							   toValue,
							   target);
			break;
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
		stack->Set(stack->tmp_value, stack->to->value);
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

static void printCurrentInterval(SMIL_AnimationStack *stack)
{
	fprintf(stdout, "Current Interval - ");
	fprintf(stdout, "Begin ");
	switch(stack->currentInterval.begin.type){
	case SMILBeginOrEnd_clock:
		fprintf(stdout, "Clock: %f", stack->currentInterval.begin.clock);
		break;
	case SMILBeginOrEnd_wallclock:
		fprintf(stdout, "Wallclock: %f", stack->currentInterval.begin.clock);
		break;
	case SMILBeginOrEnd_event:
		fprintf(stdout, "Event %x.%d + %f", stack->currentInterval.begin.element, stack->currentInterval.begin.event, stack->currentInterval.begin.clock);
		break;
	case SMILBeginOrEnd_indefinite:
		fprintf(stdout, "indefinite");
		break;
	}
	fprintf(stdout, " - End ");
	switch(stack->currentInterval.end.type){
	case SMILBeginOrEnd_clock:
		fprintf(stdout, "Clock: %f", stack->currentInterval.end.clock);
		break;
	case SMILBeginOrEnd_wallclock:
		fprintf(stdout, "Wallclock: %f", stack->currentInterval.end.clock);
		break;
	case SMILBeginOrEnd_event:
		fprintf(stdout, "Event %x.%d + %f", stack->currentInterval.end.element, stack->currentInterval.begin.event, stack->currentInterval.begin.clock);
		break;
	case SMILBeginOrEnd_indefinite:
		fprintf(stdout, "indefinite");
		break;
	}
	fprintf(stdout, "\n");
	fprintf(stdout, "Repeat - Dur (%d,%f), Count %f\n",stack->repeatDur->type, stack->repeatDur->clock_value, *stack->repeatCount);
	fprintf(stdout, "Duration - Simple %f, Active %f\n",stack->simple_duration, stack->active_duration);
	fprintf(stdout, "\n");
}

/* GPAC entry point into the SMIL animation model */
void SMIL_Update_Animation(GF_TimeNode *timenode)
{
	SMIL_AnimationStack *stack = gf_node_get_private((GF_Node *)timenode->obj);
	Double sceneTime = gf_node_get_scene_time(timenode->obj);

	stack->cycle_number++;
//	fprintf(stdout, "Scene Time: %f - Anim: %8x, Status: %d\n", sceneTime, stack, stack->status);

	if (stack->status == SMIL_STATUS_STARTUP) {
		stack->InitStackValues(stack);
		SMIL_findInterval(stack, 1, sceneTime, &(stack->currentInterval));
		if (stack->currentInterval.is_valid) {
			stack->status = SMIL_STATUS_WAITING_TO_BEGIN;
			//printCurrentInterval(stack);
		} else {
			stack->status = SMIL_STATUS_DONE;
			//fprintf(stdout, "Current Interval not resolved\n");
		}
	} 

waiting_to_begin:
	if (stack->status == SMIL_STATUS_WAITING_TO_BEGIN) {
		if (0) { /* Check changes in begin or end attributes */
			SMIL_Interval tmpInterval;
			SMIL_findInterval(stack, 0, sceneTime, &tmpInterval);
			if (compareInterval(tmpInterval, stack->currentInterval)) {
				/* intervals are different */
				/* TODO notify time dependencies */
				printCurrentInterval(stack);
			} 
		}
		if (stack->currentInterval.begin.type <= SMILBeginOrEnd_wallclock &&
			sceneTime >= stack->currentInterval.begin.clock) stack->status = SMIL_STATUS_ACTIVE;
	}

	if (stack->status == SMIL_STATUS_ACTIVE) {
		if (*(stack->restart) == SMILRestart_always) {
			SMIL_Interval tmpInterval;
			SMIL_findInterval(stack, 0, sceneTime, &tmpInterval);
			if (compareInterval(tmpInterval, stack->currentInterval)) {
				/* intervals are different */
				/* TODO notify time dependencies */
				stack->currentInterval.allocation_cycle = tmpInterval.allocation_cycle;
				stack->currentInterval.is_valid = 1;
				stack->currentInterval.begin = tmpInterval.begin;
				stack->currentInterval.end = tmpInterval.end;
//				printCurrentInterval(stack);
			} 
		}

		if (stack->Animate) {		

			stack->Animate(stack, sceneTime);
			SMIL_ApplyAccumulate(stack);
			SMIL_ApplyAdditive(stack);

			/*NOTE: node invalidation in SVG+SMILAnim is done at the SVG animation subclassing, to differenciate 
			between animations modifying geometry and others*/
			stack->Invalidate(stack);
			gf_node_dirty_set(stack->target_element, 0, 0);
			gf_sr_invalidate(stack->compositor, NULL);
		}

		/* Register the animation in the renderer */
		gf_list_add(stack->compositor->svg_animated_attributes, stack->targetAttribute);

		if ((stack->currentInterval.end.type <= SMILBeginOrEnd_wallclock && sceneTime >= stack->currentInterval.end.clock)
			|| !stack->dur) stack->status = SMIL_STATUS_POST_ACTIVE;
	}

	if (stack->status == SMIL_STATUS_POST_ACTIVE) {
		/* Unregister the animation in the renderer */
		gf_list_rem(stack->compositor->svg_animated_attributes,
					gf_list_find(stack->compositor->svg_animated_attributes, stack->targetAttribute));

		if (stack->Set) {
			if (*stack->fill == SMILFill_freeze) {
				//fprintf(stdout, "setting final animation value\n");
				void * last = SMIL_GetLastSpecifiedValue(stack);
				if (last) {
					stack->Set(stack->tmp_value, last);
					stack->nb_iterations--;
					SMIL_ApplyAccumulate(stack);
					SMIL_ApplyAdditive(stack);
				}
			} else {
				//fprintf(stdout, "resetting to initial animation value\n");
				stack->Assign(stack->targetAttribute, stack->base_value);
			}
		}

		/* Register the animation in the renderer */
		gf_list_add(stack->compositor->svg_animated_attributes, stack->targetAttribute);

		stack->status = SMIL_STATUS_DONE;
		fprintf(stdout, "Terminated Animation %8x\n", stack);

		/* Return required because we don't want the animation to be unregistered from 
		   the list of animated attributes during this cycle */
		return;
	}

	if (stack->status == SMIL_STATUS_DONE) {
		s32 item;
		if (item = gf_list_find(stack->compositor->svg_animated_attributes, stack->targetAttribute) > 0) {
			/* Unregister the animation in the renderer */
			gf_list_rem(stack->compositor->svg_animated_attributes, item);
		}

		if (0) { /* Check changes in begin or end attributes */
			SMIL_Interval tmpInterval;
			SMIL_findInterval(stack, 1, sceneTime, &tmpInterval);
			if (tmpInterval.is_valid && !compareInterval(tmpInterval, stack->currentInterval)) {
				stack->currentInterval.allocation_cycle = tmpInterval.allocation_cycle;
				stack->currentInterval.is_valid = 1;
				stack->currentInterval.begin = tmpInterval.begin;
				stack->currentInterval.end = tmpInterval.end;
				stack->status = SMIL_STATUS_WAITING_TO_BEGIN;
				printCurrentInterval(stack);
				/* TODO notify time dependencies */
				goto waiting_to_begin;
			}
		}
	}
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

SMIL_AnimationStack *SMIL_Init_AnimationStack(Render2D *sr, GF_Node *node)
{
	SMIL_AnimationStack *stack;
	GF_SAFEALLOC(stack, sizeof(SMIL_AnimationStack))
	stack->time_handle.UpdateTimeNode = SMIL_Update_Animation;
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


