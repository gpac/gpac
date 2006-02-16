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
#include <gpac/nodes_svg.h>

static void gf_smil_timing_null_timed_function(SMIL_Timing_RTI *rti, Fixed normalized_scene_time)
{
}

void gf_smil_timing_init_runtime_info(SVGElement *timed_elt)
{
	SMIL_Timing_RTI *rti;
	if (!timed_elt->timing) return;

	GF_SAFEALLOC(rti, sizeof(SMIL_Timing_RTI))
	rti->timed_elt = timed_elt;
	rti->status = SMIL_STATUS_STARTUP;
	rti->intervals = gf_list_new();
	rti->activation = gf_smil_timing_null_timed_function;
	rti->freeze = gf_smil_timing_null_timed_function;
	rti->restore = gf_smil_timing_null_timed_function;

	timed_elt->timing->runtime = rti;
}

void gf_smil_timing_delete_runtime_info(SVGElement *timed_elt)
{
	u32 i;
	SMIL_Timing_RTI *rti;

	if (!timed_elt->timing || !timed_elt->timing->runtime) return;
	rti = timed_elt->timing->runtime;
	for (i = 0; i<gf_list_count(rti->intervals); i++) {
		SMIL_Interval *interval = gf_list_get(rti->intervals, i);
		free(interval);
	}
	gf_list_del(rti->intervals);
	free(rti);
	timed_elt->timing->runtime = NULL;
}

/* computes the active duration for the given interval,
   assumes that the values of begin and end have been set
   and that begin is defined (i.e. a positive value)*/
static void gf_smil_timing_compute_active_duration(SMIL_Timing_RTI *rti, SMIL_Interval *interval)
{
	SVGElement *e;
	Bool clamp_active_duration = 1;

	e = rti->timed_elt;

	if (gf_node_get_tag((GF_Node *)e) == TAG_SVG_discard) {
		interval->active_duration = -1;
		return;
	}

	/* Step 1: Computing active duration using repeatDur and repeatCount */
	if (e->timing->dur.type == SMIL_DURATION_DEFINED) { /* simple_duration is defined */

		interval->simple_duration = e->timing->dur.clock_value;

		if (e->timing->repeatCount.type == SMIL_REPEATCOUNT_INDEFINITE ||
			e->timing->repeatDur.type == SMIL_DURATION_INDEFINITE) {
				interval->active_duration = -1;
		} else {
			if (e->timing->repeatCount.type == SMIL_REPEATCOUNT_DEFINED 
				&& e->timing->repeatDur.type != SMIL_DURATION_DEFINED) {
				interval->active_duration = FIX2FLT(e->timing->repeatCount.count) * interval->simple_duration;
			} else if (e->timing->repeatCount.type != SMIL_REPEATCOUNT_DEFINED 
					   && e->timing->repeatDur.type == SMIL_DURATION_DEFINED) {
				interval->active_duration = e->timing->repeatDur.clock_value;
			} else if (e->timing->repeatCount.type == SMIL_REPEATCOUNT_UNSPECIFIED 
					   && e->timing->repeatDur.type == SMIL_DURATION_UNSPECIFIED) {
				interval->active_duration = interval->simple_duration;
			} else {
				interval->active_duration = MIN(e->timing->repeatDur.clock_value, 
												FIX2FLT(e->timing->repeatCount.count) * interval->simple_duration);
			}			
		}
	} else {
		/* simple_duration is indefinite (it cannot be unspecified) */
		interval->simple_duration = -1;
		
		/* we can ignore repeatCount to compute active_duration */

		if (e->timing->repeatDur.type == SMIL_DURATION_UNSPECIFIED ||
			e->timing->repeatDur.type == SMIL_DURATION_INDEFINITE) {
			interval->active_duration = -1;
		} else { // (e->timing->repeatDur.type != SMIL_DURATION_DEFINED)
			interval->active_duration = e->timing->repeatDur.clock_value;
		}
	}

	/* Step 2: clamp the active duration with min and max */
	/* testing for presence of min and max because some elements may not have them: eg SVG audio */
	if (e->timing->max.type == SMIL_DURATION_DEFINED 
		&& e->timing->min.type == SMIL_DURATION_DEFINED
		&& e->timing->max.clock_value < e->timing->min.clock_value) {
		clamp_active_duration = 0;
	}
	if (clamp_active_duration) {
		if (e->timing->min.type == SMIL_DURATION_DEFINED) {
			if (interval->active_duration >= 0 
				&& interval->active_duration <= e->timing->min.clock_value) {
				interval->active_duration = e->timing->min.clock_value;
			}
		}
		if (e->timing->max.type == SMIL_DURATION_DEFINED) {
			interval->active_duration = MIN(e->timing->max.clock_value, interval->active_duration);
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

void gf_smil_timing_end_notif(SMIL_Timing_RTI *rti, Double clock)
{
	if (rti->current_interval && (rti->current_interval_index>=0) && (rti->current_interval->active_duration<0) ) {
		rti->current_interval->end = clock;
		rti->current_interval->active_duration = rti->current_interval->end - rti->current_interval->begin;
	}
}

static void gf_smil_timing_add_new_interval(SMIL_Timing_RTI *rti, SMIL_Interval *interval, Double begin, u32 index)
{
	u32 j, end_count;
	SMIL_Time *end;


	if (!interval) {
		GF_SAFEALLOC(interval, sizeof(SMIL_Interval));
		interval->begin = begin;
		gf_list_add(rti->intervals, interval);
	}

	end_count = gf_list_count(rti->timed_elt->timing->end);
	/* trying to find a matching end */
	if (end_count > index) {
		for (j = index; j < end_count; j++) {
			end = gf_list_get(rti->timed_elt->timing->end, j);
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
	gf_smil_timing_compute_active_duration(rti, interval);
}

/* assumes that the list of time values is sorted */
static void gf_smil_timing_init_interval_list(SMIL_Timing_RTI *rti)
{
	u32 i, count;
	SMIL_Time *begin;

	count = gf_list_count(rti->intervals);
	for (i = 0; i<count; i++) {
		SMIL_Interval *interval = gf_list_get(rti->intervals, i);
		free(interval);
	}
	gf_list_reset(rti->intervals);

	count = gf_list_count(rti->timed_elt->timing->begin);
	if (count) {
		for (i = 0; i < count; i ++) {
			begin = gf_list_get(rti->timed_elt->timing->begin, i);
			if (begin->type < SMIL_TIME_EVENT) {
				/* we create an acceptable only if begin is unspecified or defined (clock or wallclock) */
				gf_smil_timing_add_new_interval(rti, NULL, begin->clock, i);
			} else {
				/* this is not an acceptable value for a begin */
			}
		} 
	} else {
		gf_smil_timing_add_new_interval(rti, NULL, 0, 0);
	}
}

/* assumes that the list of time values is sorted */
static void gf_smil_timing_refresh_interval_list(SMIL_Timing_RTI *rti)
{
	u32 i, count, j, count2;
	SMIL_Time *begin;

	count = gf_list_count(rti->timed_elt->timing->begin);
	for (i = 0; i < count; i ++) {
		SMIL_Interval *existing_interval = NULL;
		begin = gf_list_get(rti->timed_elt->timing->begin, i);
		/* this is not an acceptable value for a begin */
		if (begin->type >= SMIL_TIME_EVENT) continue;

		count2 = gf_list_count(rti->intervals);
		for (j=0; j<count2; j++) {
			existing_interval = gf_list_get(rti->intervals, j);
			if (existing_interval->begin==begin->clock) break;
			existing_interval = NULL;
		}
		/* we create an acceptable only if begin is unspecified or defined (clock or wallclock) */
		gf_smil_timing_add_new_interval(rti, existing_interval, begin->clock, i);
	}
}

#if 0
static void gf_smil_timing_print_interval(SMIL_Interval *interval)
{
	fprintf(stdout, "Current Interval - ");
	fprintf(stdout, "Begin: %f", interval->begin);
	fprintf(stdout, " - End: %f", interval->end);
	fprintf(stdout, "\n");
	fprintf(stdout, "Duration - Simple %f, Active %f\n",interval->simple_duration, interval->active_duration);
	fprintf(stdout, "\n");
}
#endif

static s32 gf_smil_timing_find_interval_index(SMIL_Timing_RTI *rti, Double scene_time)
{
	s32 index = -1;
	u32 i, count;
	count = gf_list_count(rti->intervals);
	if (rti->current_interval) i = rti->current_interval_index + 1;
	else i = 0;
	for (; i<count; i++) {
		SMIL_Interval *interval = gf_list_get(rti->intervals, i);
		if (interval->begin <= scene_time) {
			index = i;
			break;
		}
	}
	return index;
}

void gf_smil_timing_notify_time(SMIL_Timing_RTI *rti, Double scene_time)
{
	GF_DOM_Event evt;

	rti->cycle_number++;
//	fprintf(stdout, "Scene Time: %f - Timing Stack: %8x, Status: %d\n", scene_time, rti, rti->status);

	if (rti->status == SMIL_STATUS_STARTUP) {
		s32 interval_index;
		gf_smil_timing_init_interval_list(rti);
		rti->current_interval = NULL;
		interval_index = gf_smil_timing_find_interval_index(rti, GF_MAX_DOUBLE);
		if (interval_index >= 0) {
			rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
			rti->current_interval_index = interval_index;
			rti->current_interval = gf_list_get(rti->intervals, rti->current_interval_index);
			//gf_smil_timing_print_interval(stack->current_interval);
		} else {
			return;
		}
	} 

waiting_to_begin:
	if (rti->status == SMIL_STATUS_WAITING_TO_BEGIN) {
		if (scene_time >= rti->current_interval->begin) {
			rti->status = SMIL_STATUS_ACTIVE;
			memset(&evt, 0, sizeof(evt));
			evt.type = SVG_DOM_EVT_BEGIN;
			gf_sg_fire_dom_event((GF_Node *)rti->timed_elt, NULL, &evt);
		}
		else return;
	}

	if (rti->status == SMIL_STATUS_ACTIVE) {
		if (rti->current_interval->active_duration >= 0 
			&& scene_time >= (rti->current_interval->begin + rti->current_interval->active_duration)) {
			rti->status = SMIL_STATUS_POST_ACTIVE;
			goto post_active;
		}

		if (rti->timed_elt->timing->restart == SMIL_RESTART_ALWAYS) {
			s32 interval_index;
			interval_index = gf_smil_timing_find_interval_index(rti, scene_time);
			
			if (interval_index >= 0 &&
				interval_index != rti->current_interval_index) {
				/* intervals are different, use the new one */
				rti->current_interval_index = interval_index;
				rti->current_interval = gf_list_get(rti->intervals, rti->current_interval_index);
//				gf_smil_timing_print_interval(stack->current_interval);
				/* TODO notify time dependencies */
			} 
		}

		rti->activation(rti, gf_smil_timing_get_normalized_simple_time(rti, scene_time));
	}

post_active:
	if (rti->status == SMIL_STATUS_POST_ACTIVE) {
		if (rti->timed_elt->timing->fill == SMIL_FILL_FREEZE) {
			rti->status = SMIL_STATUS_FROZEN;
			rti->first_frozen = rti->cycle_number;
		} else {
			rti->status = SMIL_STATUS_DONE;
			rti->restore(rti, gf_smil_timing_get_normalized_simple_time(rti, scene_time));
		}
		memset(&evt, 0, sizeof(evt));
		evt.type = SVG_DOM_EVT_END;
		gf_sg_fire_dom_event((GF_Node *)rti->timed_elt, NULL, &evt);
	}

	if (rti->status == SMIL_STATUS_FROZEN) {
		rti->freeze(rti, gf_smil_timing_get_normalized_simple_time(rti, scene_time));
	}

	if ((rti->status == SMIL_STATUS_DONE) || (rti->status == SMIL_STATUS_FROZEN)) {
		if (rti->timed_elt->timing->restart != SMIL_RESTART_NEVER) { /* Check changes in begin or end attributes */
			s32 interval_index;

			interval_index = gf_smil_timing_find_interval_index(rti, scene_time);
			if (interval_index >= 0 && interval_index != rti->current_interval_index) {
				/* intervals are different, use the new one */
				rti->current_interval_index = interval_index;
				rti->current_interval = gf_list_get(rti->intervals, rti->current_interval_index);
//				gf_smil_timing_print_interval(stack->current_interval);
				/* TODO notify time dependencies */
				rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
				goto waiting_to_begin;
			}
		}
	}
}

Fixed gf_smil_timing_get_normalized_simple_time(SMIL_Timing_RTI *rti, Double scene_time) 
{
	Double activeTime;
	Double simpleTime;
	Fixed normalizedSimpleTime;

	activeTime			 = scene_time - rti->current_interval->begin;
	rti->current_interval->nb_iterations = (u32)floor(activeTime / rti->current_interval->simple_duration);
	simpleTime			 = activeTime - rti->current_interval->simple_duration * rti->current_interval->nb_iterations;

	/* to be sure clamp simpleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(rti->current_interval->simple_duration, simpleTime);
	normalizedSimpleTime = FLT2FIX(simpleTime / rti->current_interval->simple_duration);

	return normalizedSimpleTime;
}

void gf_smil_timing_modified(GF_Node *node, GF_FieldInfo *field)
{
	SVGElement *e = (SVGElement *)node;
	SMIL_Timing_RTI *rti;
	
	if (!field || !e->timing) return;
	rti = e->timing->runtime;
	if (!rti) return;

	/*recompute interval list*/
	gf_smil_timing_refresh_interval_list(rti);
}
