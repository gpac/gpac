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
										 SMIL_Time toCompare, 
										 Bool strict, 
										 SMIL_Time *result)
{
	u32 i;
	u32 count = gf_list_count(timeValues);
	if (toCompare.type >= SMIL_TIME_EVENT) {
		/* The time value to compare to is either unresolved or indefinite, nothing will be greater */
		result->type = SMIL_TIME_INDEFINITE;
		result->clock = 0;
		return;
	}
	for (i = 0; i < count; i++) {
		SMIL_Time *b = gf_list_get(timeValues, i);
		if (b->type <= SMIL_TIME_WALLCLOCK) {
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
		result->type = SMIL_TIME_CLOCK;
		result->clock = 0;
	} else {
		result->type = SMIL_TIME_INDEFINITE;
		result->clock = 0;
	}
}

static void SMIL_calcActiveDur(SMIL_AnimationStack *stack)
{
	Bool clamp_active_duration = 1;

	stack->currentInterval.active_duration = -1;

	if (!stack->dur) {
		/* for the SVGdiscardElement */
		/* active_duration stays indefinite */
		return;
	}

	/* Step 3: determine the simple duration */
	if (stack->dur->type != SMIL_DURATION_VALUE) {
		/* simple_duration is not defined, so stays indefinite */
		/* we can ignore repeatCount to compute active_duration */

		/* Step 2: determine the active duration */
		if (stack->repeatDur->type == SMIL_DURATION_UNSPECIFIED) {
			/* active_duration stays indefinite */
			stack->currentInterval.active_duration = -1;
		} else {
			if (stack->repeatDur->type != SMIL_DURATION_VALUE) {
				/* active_duration stays indefinite */
				stack->currentInterval.active_duration = -1;
			} else {
				stack->currentInterval.active_duration = stack->repeatDur->clock_value;
			}
		}
	} else {
		/* simple_duration is defined */
		stack->currentInterval.simple_duration = stack->dur->clock_value;

		/* Step 2: determine the active duration */
		if (*(stack->repeatCount) < 0) {
			/* use repeatDur only to determine the active duration */
			if (stack->repeatDur->type != SMIL_DURATION_VALUE) {
				/* active_duration stays indefinite */
			} else {
				stack->currentInterval.active_duration = stack->repeatDur->clock_value;
				*(stack->repeatCount) = FLT2FIX(stack->repeatDur->clock_value/stack->currentInterval.simple_duration);
			}
		} else {
			if (stack->repeatDur->type == SMIL_DURATION_UNSPECIFIED) {
				/* use repeatCount only to determine the active duration */
				stack->currentInterval.active_duration = FIX2FLT(*stack->repeatCount) * stack->currentInterval.simple_duration;
			} else {
				if (stack->repeatDur->type != SMIL_DURATION_VALUE) {
					/* indefinite or media means indefinite */
					stack->currentInterval.active_duration = -1;
				} else {
					/* use repeatCount and repeatDur to determine the active duration */
					stack->currentInterval.active_duration = MIN(stack->repeatDur->clock_value, FIX2FLT(*stack->repeatCount) * stack->currentInterval.simple_duration);
				}
			}
		}
	}

	/* Step 4: clamp the active duration with min and max */
	/* testing for presence of min and max because some elements may not have them: eg SVG audio */
	if (stack->max && stack->min && stack->max->type == SMIL_DURATION_VALUE && stack->min->type == SMIL_DURATION_VALUE) {
		if (stack->max->clock_value < stack->min->clock_value) clamp_active_duration = 0;
	}
	if (clamp_active_duration) {
		if (stack->min && stack->min->type == SMIL_DURATION_VALUE) {
			if (stack->currentInterval.active_duration >= 0 && stack->currentInterval.active_duration <= stack->min->clock_value) {
				stack->currentInterval.is_active_duration_clamped_to_min = 1;
				stack->currentInterval.active_duration = stack->min->clock_value;
			}
		}
		if (stack->max && stack->max->type == SMIL_DURATION_VALUE) {
			stack->currentInterval.active_duration = MIN(stack->max->clock_value, stack->currentInterval.active_duration);
		}
	}

	/* Step 5: if end is defined, clamp active duration to end-begin */
	if (stack->currentInterval.end.type < SMIL_TIME_EVENT) {
		if (stack->currentInterval.begin.type < SMIL_TIME_EVENT) {
			stack->currentInterval.active_duration = MIN(stack->currentInterval.active_duration, (stack->currentInterval.end.clock - stack->currentInterval.begin.clock ));
		} else {
			fprintf(stderr, "begin is indefinite but end is finite ... ");
		}
	} else {
		/* so that we have an end */
		if (stack->currentInterval.begin.type < SMIL_TIME_EVENT) {
			stack->currentInterval.end.type = SMIL_TIME_CLOCK;
			if (stack->currentInterval.active_duration < 0) {
				stack->currentInterval.end.clock = GF_MAX_DOUBLE;
			} else {
				stack->currentInterval.end.clock = stack->currentInterval.begin.clock + stack->currentInterval.active_duration;
			}
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
	SMIL_Time tmpBegin, tmpEnd;
	SMIL_Time beginAfter;

	memset(&tmpBegin, 0, sizeof(SMIL_Time));
	memset(&tmpEnd, 0, sizeof(SMIL_Time));
	memset(&beginAfter, 0, sizeof(SMIL_Time));

	if (!first) {
		/* we already had an animation running, now we look for the next one */
		beginAfter.clock = interval->end.clock;
		beginAfter.type  = SMIL_TIME_CLOCK;
	} else {
		/* first time this animation runs */
		beginAfter.clock = GF_MIN_DOUBLE;
		beginAfter.type  = SMIL_TIME_CLOCK;
	}

	interval->end.type = SMIL_TIME_INDEFINITE;

	while (1) {
		SMIL_getTimeValueGreaterThan(1, *stack->begins, beginAfter, 0, &tmpBegin);
		if (tmpBegin.type >= SMIL_TIME_EVENT) {
			/* Interval not found since it starts either by an unresolved value or an indefinite one */
			interval->is_valid = 0;
			return;
		} else {
			interval->begin.type = tmpBegin.type;
			interval->begin.clock = tmpBegin.clock;
		}
		if (!stack->ends || gf_list_count(*stack->ends) == 0) {
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
		if ((tmpEnd.type < SMIL_TIME_EVENT && tmpEnd.clock > 0) 
			|| (tmpEnd.type >= SMIL_TIME_EVENT)) {
			interval->is_valid = 1;
			interval->allocation_cycle = stack->cycle_number;
			interval->begin = tmpBegin;
			interval->end = tmpEnd;
			return;
		} else if (*(stack->restart) == SMIL_RESTART_NEVER) {
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
	case SMIL_TIME_EVENT:
		if (a.begin.event != b.begin.event) return 1;
		if (a.begin.parameter != b.begin.parameter) return 1;
		if (a.begin.element != b.begin.element) return 1;
		/* no break; because events can have an offset */
	case SMIL_TIME_CLOCK:
	case SMIL_TIME_WALLCLOCK:
		if (a.begin.clock != b.begin.clock) return 1;
		break;
	case SMIL_TIME_INDEFINITE:
	default:
		/* nothing to do */
		break;
	}
	switch(a.end.type) {
	case SMIL_TIME_EVENT:
		if (a.end.event != b.end.event) return 1;
		if (a.end.parameter != b.end.parameter) return 1;
		if (a.end.element != b.end.element) return 1;
		/* no break; because events can have an offset */
	case SMIL_TIME_CLOCK:
	case SMIL_TIME_WALLCLOCK:
		if (a.end.clock != b.end.clock) return 1;
		break;
	case SMIL_TIME_INDEFINITE:
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
		&& *stack->accumulate == SMIL_ACCUMULATE_SUM
		&& stack->currentInterval.nb_iterations > 0) {
		void *last_specified_value = SMIL_GetLastSpecifiedValue(stack);
		if (last_specified_value)
			stack->ApplyAccumulate(stack->currentInterval.nb_iterations, stack->tmp_value, last_specified_value, stack->tmp_value);
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

	activeTime			 = sceneTime - stack->currentInterval.begin.clock;
	stack->currentInterval.nb_iterations = (u32)floor(activeTime / stack->currentInterval.simple_duration);
	simpleTime			 = activeTime - stack->currentInterval.simple_duration * stack->currentInterval.nb_iterations;
	
	/* to be sure clamp simpleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(stack->currentInterval.simple_duration, simpleTime);
	normalizedSimpleTime = FLT2FIX(simpleTime / stack->currentInterval.simple_duration);

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
			interval_duration = FLT2FIX(stack->currentInterval.simple_duration) / nbValues;
			keyValueIndex = (u32)floor(normalizedSimpleTime*nbValues);			
		} else {
			interval_duration = FLT2FIX(stack->currentInterval.simple_duration) / (nbValues-1);
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

	activeTime			 = sceneTime - stack->currentInterval.begin.clock;
	stack->currentInterval.nb_iterations = (u32)floor(activeTime / stack->currentInterval.simple_duration);
	simpleTime			 = activeTime - stack->currentInterval.simple_duration * stack->currentInterval.nb_iterations;
	
	/* to be sure clamp cycleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(stack->currentInterval.simple_duration, simpleTime);

	interpolation_coefficient = FLT2FIX(simpleTime/stack->currentInterval.simple_duration);
	
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
				s32 useFrom = (simpleTime<=stack->currentInterval.simple_duration/2);
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
	case SMIL_TIME_CLOCK:
		fprintf(stdout, "Clock: %f", stack->currentInterval.begin.clock);
		break;
	case SMIL_TIME_WALLCLOCK:
		fprintf(stdout, "Wallclock: %f", stack->currentInterval.begin.clock);
		break;
	case SMIL_TIME_EVENT:
		fprintf(stdout, "Event %x.%d + %f", stack->currentInterval.begin.element, stack->currentInterval.begin.event, stack->currentInterval.begin.clock);
		break;
	case SMIL_TIME_INDEFINITE:
		fprintf(stdout, "indefinite");
		break;
	}
	fprintf(stdout, " - End ");
	switch(stack->currentInterval.end.type){
	case SMIL_TIME_CLOCK:
		fprintf(stdout, "Clock: %f", stack->currentInterval.end.clock);
		break;
	case SMIL_TIME_WALLCLOCK:
		fprintf(stdout, "Wallclock: %f", stack->currentInterval.end.clock);
		break;
	case SMIL_TIME_EVENT:
		fprintf(stdout, "Event %x.%d + %f", stack->currentInterval.end.element, stack->currentInterval.begin.event, stack->currentInterval.begin.clock);
		break;
	case SMIL_TIME_INDEFINITE:
		fprintf(stdout, "indefinite");
		break;
	}
	fprintf(stdout, "\n");
	fprintf(stdout, "Repeat - Dur (%d,%f), Count %f\n",stack->repeatDur->type, stack->repeatDur->clock_value, *stack->repeatCount);
	fprintf(stdout, "Duration - Simple %f, Active %f\n",stack->currentInterval.simple_duration, stack->currentInterval.active_duration);
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
//			printCurrentInterval(stack);
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
//				printCurrentInterval(stack);
			} 
		}
		if (stack->currentInterval.begin.type <= SMIL_TIME_WALLCLOCK &&
			sceneTime >= stack->currentInterval.begin.clock) stack->status = SMIL_STATUS_ACTIVE;
	}

	if (stack->status == SMIL_STATUS_ACTIVE) {
		if ((stack->currentInterval.end.type <= SMIL_TIME_WALLCLOCK && sceneTime >= stack->currentInterval.end.clock)
			|| !stack->dur) {
			stack->status = SMIL_STATUS_POST_ACTIVE;
			goto post_active;
		}

		if (0 && *(stack->restart) == SMIL_RESTART_ALWAYS) {
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
			if (stack->target_value_changed) {
				stack->Invalidate(stack);
				stack->target_value_changed = 0;
			}
		}

		/* Register the animation in the renderer */
		gf_list_add(stack->compositor->svg_animated_attributes, stack->targetAttribute);
	}

post_active:
	if (stack->status == SMIL_STATUS_POST_ACTIVE) {
		s32 item;
		if (item = gf_list_find(stack->compositor->svg_animated_attributes, stack->targetAttribute) > 0) {
			/* Unregister the animation in the renderer */
			gf_list_rem(stack->compositor->svg_animated_attributes, item);
		}

		if (stack->Set && stack->fill) {
			if (*stack->fill == SMIL_FILL_FREEZE) {
				//fprintf(stdout, "setting final animation value\n");
				void * last = SMIL_GetLastSpecifiedValue(stack);
				if (last) {
					if (stack->Compare(stack->tmp_value, last)) {
						stack->Set(stack->tmp_value, last);
						stack->target_value_changed = 1;
					}
					stack->currentInterval.nb_iterations--;
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

		stack->status = SMIL_STATUS_DONE;
//		fprintf(stdout, "Animation done %8x.\n", stack);

		/* Return required because we don't want the animation to be unregistered from 
		   the list of animated attributes during this cycle */
		return;
	}

	if (stack->status == SMIL_STATUS_DONE) {
		s32 item;
		if (item = gf_list_find(stack->compositor->svg_animated_attributes, stack->targetAttribute) >= 0) {
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
//				printCurrentInterval(stack);
				/* TODO notify time dependencies */
				goto waiting_to_begin;
			}
		}
		/*FIXME - when an animation is known to be done (that is, cannot be re-started without external events), 
		these two lines should be called*/
//		gf_sr_unregister_time_node(stack->compositor, &stack->time_handle);
//		gf_sr_invalidate(stack->compositor, NULL);
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
	stack->last_keytime_index = 0;

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


