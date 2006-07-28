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
#include <gpac/nodes_svg.h>

static void gf_smil_timing_null_timed_function(SMIL_Timing_RTI *rti, Fixed normalized_scene_time)
{
}

void gf_smil_timing_init_runtime_info(SVGElement *timed_elt)
{
	SMIL_Timing_RTI *rti;
	if (!timed_elt->timing) return;
	if (timed_elt->timing->runtime) return;

	GF_SAFEALLOC(rti, sizeof(SMIL_Timing_RTI))
	rti->timed_elt = timed_elt;
	rti->status = SMIL_STATUS_STARTUP;
	rti->intervals = gf_list_new();
	rti->activation = gf_smil_timing_null_timed_function;
	rti->freeze = gf_smil_timing_null_timed_function;
	rti->restore = gf_smil_timing_null_timed_function;
	rti->scene_time = -1;

	timed_elt->timing->runtime = rti;

	gf_list_add(timed_elt->sgprivate->scenegraph->smil_timed_elements, rti);
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
	gf_list_del_item(timed_elt->sgprivate->scenegraph->smil_timed_elements, rti);
	free(rti);
	timed_elt->timing->runtime = NULL;
}

Bool gf_smil_timing_is_active(GF_Node *node) 
{
	SVGElement *e = (SVGElement *)node;
	if (!e->timing || !e->timing->runtime) return 0;
	return (e->timing->runtime->status == SMIL_STATUS_ACTIVE);
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
			if ( GF_SMIL_TIME_IS_SPECIFIED_CLOCK(end->type) )  {
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
			if (GF_SMIL_TIME_IS_CLOCK(begin->type) ) {
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
	
	if (!count) {
		if (rti->current_interval)
			gf_smil_timing_add_new_interval(rti, rti->current_interval, rti->current_interval->begin, 0);
		return;
	}

	for (i = 0; i < count; i ++) {
		SMIL_Interval *existing_interval = NULL;
		begin = gf_list_get(rti->timed_elt->timing->begin, i);
		/* this is not an acceptable value for a begin */
		if (! GF_SMIL_TIME_IS_CLOCK(begin->type) ) continue;

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

Bool gf_sg_notify_smil_timed_elements(GF_SceneGraph *sg)
{
	SMIL_Timing_RTI *rti;
	Double scene_time;
	u32 active_count = 0, i = 0;

	if (!sg) return 0;
	scene_time = sg->GetSceneTime(sg->SceneCallback);

	sg->reeval_timing = 0;
	while((rti = gf_list_enum(sg->smil_timed_elements, &i))) {
		active_count += gf_smil_timing_notify_time(rti, scene_time);
	}
	/*in case an anim triggers another one previously inactivated...
	TODO FIXME: it would be much better to stack anim as active/inactive*/
	while (sg->reeval_timing) {
		sg->reeval_timing = 0;
		i = 0;
		while((rti = gf_list_enum(sg->smil_timed_elements, &i))) {
			/*this means the anim has been, modified, re-evaluate it*/
			if (rti->scene_time==-1) 
				active_count += gf_smil_timing_notify_time(rti, scene_time);
		}
	}
	return (active_count>0);
}

/* Returns 1 when a rendering traversal is required!!! */
Bool gf_smil_timing_notify_time(SMIL_Timing_RTI *rti, Double scene_time)
{
	Bool ret = 0;
	GF_DOM_Event evt;
	
	if (rti->scene_time == scene_time) return 0;
	rti->scene_time = scene_time;
	rti->cycle_number++;
	rti->evaluate = NULL;	

//	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("\n", gf_node_get_name(node), scene_time));
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
			return 0;
		}
	} 

waiting_to_begin:
	if (rti->status == SMIL_STATUS_WAITING_TO_BEGIN) {
		if (scene_time >= rti->current_interval->begin) {
			rti->status = SMIL_STATUS_ACTIVE;
			memset(&evt, 0, sizeof(evt));
			evt.type = SVG_DOM_EVT_BEGIN;
//			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[DOM Events] Firing event %s.beginEvent at time %g\n", gf_node_get_name(node), scene_time));
			gf_dom_event_fire((GF_Node *)rti->timed_elt, NULL, &evt);
		}
		else return ret;
	}

	if (rti->status == SMIL_STATUS_ACTIVE) {
		u32 cur_id;
//		Fixed simple_time;

		if (rti->current_interval->active_duration >= 0 
			&& scene_time >= (rti->current_interval->begin + rti->current_interval->active_duration)) {
			rti->status = SMIL_STATUS_POST_ACTIVE;
			ret = 1;
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
				memset(&evt, 0, sizeof(evt));
				evt.type = SVG_DOM_EVT_BEGIN;
//				fprintf(stdout, "Time %f - Firing DOM %s.beginEvent\n", scene_time, rti->timed_elt->sgprivate->NodeName);
				gf_dom_event_fire((GF_Node *)rti->timed_elt, NULL, &evt);
			} 
		}

		ret = 1;
		
		//simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
		cur_id = rti->current_interval->nb_iterations;
		rti->evaluate = rti->activation;
		if (!rti->postpone) {
			Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
			rti->evaluate(rti, simple_time);
		}
		if (cur_id < rti->current_interval->nb_iterations) {
			memset(&evt, 0, sizeof(evt));
			evt.type = SVG_DOM_EVT_REPEAT;
			evt.detail = rti->current_interval->nb_iterations;
//			fprintf(stdout, "Time %f - Firing DOM %s.repeatEvent\n", scene_time, rti->timed_elt->sgprivate->NodeName);
			gf_dom_event_fire((GF_Node *)rti->timed_elt, NULL, &evt);
		}
	}

post_active:
	if (rti->status == SMIL_STATUS_POST_ACTIVE) {
		if (rti->timed_elt->timing->fill == SMIL_FILL_FREEZE) {
			rti->status = SMIL_STATUS_FROZEN;
			rti->first_frozen = rti->cycle_number;
		} else {
			rti->status = SMIL_STATUS_DONE;
			rti->evaluate = rti->restore;
			if (!rti->postpone) {
				Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
				rti->evaluate(rti, simple_time);
			}
		}
		memset(&evt, 0, sizeof(evt));
		evt.type = SVG_DOM_EVT_END;
//		fprintf(stdout, "Time %f - Firing DOM %s.endEvent\n", scene_time, rti->timed_elt->sgprivate->NodeName);
		gf_dom_event_fire((GF_Node *)rti->timed_elt, NULL, &evt);
	}

	if (rti->status == SMIL_STATUS_FROZEN) {
		rti->evaluate = rti->freeze;
		if (!rti->postpone) {
			Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
			rti->evaluate(rti, simple_time);
		}
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

				rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
				rti->evaluate = NULL;
				goto waiting_to_begin;
			}
		}
	}
	return ret;
}

Fixed gf_smil_timing_get_normalized_simple_time(SMIL_Timing_RTI *rti, Double scene_time) 
{
	Double activeTime;
	Double simpleTime;
	Fixed normalizedSimpleTime;

	activeTime			 = scene_time - rti->current_interval->begin;
	if (rti->current_interval->active_duration != -1 && activeTime > rti->current_interval->active_duration) return FIX_ONE;

	if (rti->current_interval->simple_duration>0) {
		rti->current_interval->nb_iterations = (u32)floor(activeTime / rti->current_interval->simple_duration);
	} else {
		rti->current_interval->nb_iterations = 0;
	}
	
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
	
	if (/*!field ||*/ !e->timing) return;
	rti = e->timing->runtime;
	if (!rti) return;

	/*recompute interval list*/
	rti->scene_time = -1;
	node->sgprivate->scenegraph->reeval_timing = 1;
	gf_smil_timing_refresh_interval_list(rti);
}
