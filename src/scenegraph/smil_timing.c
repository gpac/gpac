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
#include <gpac/events.h>
#include <gpac/nodes_svg_sa.h>
#include <gpac/nodes_svg_sani.h>
#include <gpac/nodes_svg_da.h>

static void gf_smil_timing_null_timed_function(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, u32 state)
{
}

static s32 gf_smil_timing_find_interval_index(SMIL_Timing_RTI *rti, Double scene_time)
{
	s32 index = -1;
	u32 i, count;
	count = gf_list_count(rti->intervals);
	if (rti->current_interval) i = rti->current_interval_index + 1;
	else i = 0;
	for (; i<count; i++) {
		SMIL_Interval *interval = (SMIL_Interval *)gf_list_get(rti->intervals, i);
		if (interval->begin <= scene_time) {
			index = i;
			break;
		}
	}
	return index;
}

/* computes the active duration for the given interval,
   assumes that the values of begin and end have been set
   and that begin is defined (i.e. a positive value)*/
static void gf_smil_timing_compute_active_duration(SMIL_Timing_RTI *rti, SMIL_Interval *interval)
{
	Bool clamp_active_duration;
	Bool isDurDefined, isRepeatCountDefined, isRepeatDurDefined, isMinDefined, isMaxDefined, isRepeatDurIndefinite, isRepeatCountIndefinite, isMediaDuration;
	SMILTimingAttributesPointers *timingp = rti->timingp;

	if (!timingp) return;
	
	switch (gf_node_get_tag((GF_Node *)rti->timed_elt)) {
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_discard:
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_discard:
#endif
	case TAG_SVG_discard:
		interval->active_duration = -1;
		return;
	}

	isDurDefined = (timingp->dur && timingp->dur->type == SMIL_DURATION_DEFINED);
	isMediaDuration = (timingp->dur && (timingp->dur->type == SMIL_DURATION_MEDIA) && (rti->media_duration>=0) );
	isRepeatCountDefined = (timingp->repeatCount && timingp->repeatCount->type == SMIL_REPEATCOUNT_DEFINED);
	isRepeatCountIndefinite = (timingp->repeatCount && timingp->repeatCount->type == SMIL_REPEATCOUNT_INDEFINITE);
	isRepeatDurDefined = (timingp->repeatDur && timingp->repeatDur->type == SMIL_DURATION_DEFINED);
	isRepeatDurIndefinite = (timingp->repeatDur && timingp->repeatDur->type == SMIL_DURATION_INDEFINITE);

	/* Step 1: Computing active duration using repeatDur and repeatCount */
	if (isDurDefined || isMediaDuration) {
		interval->simple_duration = isMediaDuration ? rti->media_duration : timingp->dur->clock_value;

		if (isRepeatCountDefined && !isRepeatDurDefined) {
			interval->active_duration = FIX2FLT(timingp->repeatCount->count) * interval->simple_duration;
		} else if (!isRepeatCountDefined && isRepeatDurDefined) {
			interval->active_duration = timingp->repeatDur->clock_value;
		} else if (!isRepeatCountDefined && !isRepeatDurDefined) {
			if (isRepeatDurIndefinite || isRepeatCountIndefinite) {
				interval->active_duration = -1;
			} else {
				interval->active_duration = interval->simple_duration;
			}
		} else {
			interval->active_duration = MIN(timingp->repeatDur->clock_value, 
										FIX2FLT(timingp->repeatCount->count) * interval->simple_duration);
		}			
	} else {

		/* simple_duration is indefinite */
		interval->simple_duration = -1;
		
		/* we can ignore repeatCount to compute active_duration */
		if (!isRepeatDurDefined) {
			interval->active_duration = -1;
		} else {
			interval->active_duration = timingp->repeatDur->clock_value;
		}
	}

	/* Step 2: clamp the active duration with min and max */
	clamp_active_duration = 1;
	/* testing for presence of min and max because some elements may not have them: eg SVG audio */
	isMinDefined = (timingp->min && timingp->min->type == SMIL_DURATION_DEFINED);
	isMaxDefined = (timingp->max && timingp->max->type == SMIL_DURATION_DEFINED);
	if (isMinDefined && isMaxDefined && 
		timingp->max->clock_value < timingp->min->clock_value) {
		clamp_active_duration = 0;
	}
	if (clamp_active_duration) {
		if (isMinDefined) {
			if (interval->active_duration >= 0 && interval->active_duration <= timingp->min->clock_value) {
				interval->active_duration = timingp->min->clock_value;
			}
		}
		if (isMaxDefined) {
			if ((interval->active_duration >= 0 && interval->active_duration >= timingp->max->clock_value) ||
				interval->active_duration == -1) {
				interval->active_duration = timingp->max->clock_value;
			}
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

void gf_smil_set_media_duration(SMIL_Timing_RTI *rti, Double media_duration)
{
	rti->media_duration = media_duration;
	gf_smil_timing_compute_active_duration(rti, rti->current_interval);
}


static void gf_smil_timing_add_new_interval(SMIL_Timing_RTI *rti, SMIL_Interval *interval, Double begin, u32 index)
{
	u32 j, end_count;
	SMIL_Time *end;
	SMILTimingAttributesPointers *timingp = rti->timingp;

	if (!timingp) return;

	if (!interval) {
		GF_SAFEALLOC(interval, SMIL_Interval)
		interval->begin = begin;
		gf_list_add(rti->intervals, interval);
	}

	end_count = (timingp->end ? gf_list_count(*timingp->end) : 0);
	/* trying to find a matching end */
	if (end_count > index) {
		for (j = index; j < end_count; j++) {
			end = (SMIL_Time*)gf_list_get(*timingp->end, j);
			if ( GF_SMIL_TIME_IS_CLOCK(end->type) )  {
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
	SMILTimingAttributesPointers *timingp = rti->timingp;

	if (!timingp) return;

	count = gf_list_count(rti->intervals);
	for (i = 0; i<count; i++) {
		SMIL_Interval *interval = (SMIL_Interval *)gf_list_get(rti->intervals, i);
		free(interval);
	}
	gf_list_reset(rti->intervals);

	count = (timingp->begin ? gf_list_count(*timingp->begin) : 0);
	if (count) {
		for (i = 0; i < count; i ++) {
			begin = (SMIL_Time*)gf_list_get(*timingp->begin, i);
			if (GF_SMIL_TIME_IS_CLOCK(begin->type) ) {
				/* we create an acceptable only if begin is unspecified or defined (clock or wallclock) */
				gf_smil_timing_add_new_interval(rti, NULL, begin->clock, i);
			} else {
				/* this is not an acceptable value for a begin */
			}
		} 
	}
	/*conditional has a default begin value of indefinite*/
	else if (rti->timed_elt->sgprivate->tag != TAG_SVG_conditional) {
		gf_smil_timing_add_new_interval(rti, NULL, 0, 0);
	}
}

GF_EXPORT
void gf_smil_timing_init_runtime_info(GF_Node *timed_elt)
{
	s32 interval_index;
	GF_SceneGraph *sg;
	SMIL_Timing_RTI *rti;
	SMILTimingAttributesPointers *timingp = NULL;
	u32 tag = gf_node_get_tag(timed_elt);

	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		SVGAllAttributes all_atts;
		SVGTimedAnimBaseElement *e = (SVGTimedAnimBaseElement *)timed_elt;
		gf_svg_flatten_attributes((SVG_Element *)e, &all_atts);
		e->timingp = malloc(sizeof(SMILTimingAttributesPointers));
		e->timingp->begin		= all_atts.begin;
		e->timingp->clipBegin	= all_atts.clipBegin;
		e->timingp->clipEnd		= all_atts.clipEnd;
		e->timingp->dur			= all_atts.dur;
		e->timingp->end			= all_atts.end;
		e->timingp->fill		= all_atts.smil_fill;
		e->timingp->max			= all_atts.max;
		e->timingp->min			= all_atts.min;
		e->timingp->repeatCount = all_atts.repeatCount;
		e->timingp->repeatDur	= all_atts.repeatDur;
		e->timingp->restart		= all_atts.restart;
		timingp = e->timingp;
	} 
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
		SVG_SA_Element *e = (SVG_SA_Element *)timed_elt;
		e->timingp = malloc(sizeof(SMILTimingAttributesPointers));
		e->timingp->begin		= &e->timing->begin;
		e->timingp->clipBegin	= &e->timing->clipBegin;
		e->timingp->clipEnd		= &e->timing->clipEnd;
		e->timingp->dur			= &e->timing->dur;
		e->timingp->end			= &e->timing->end;
		e->timingp->fill		= &e->timing->fill;
		e->timingp->max			= &e->timing->max;
		e->timingp->min			= &e->timing->min;
		e->timingp->repeatCount = &e->timing->repeatCount;
		e->timingp->repeatDur	= &e->timing->repeatDur;
		e->timingp->restart		= &e->timing->restart;
		timingp = e->timingp;
	}
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		SVG_SANI_Element *e = (SVG_SANI_Element *)timed_elt;
		e->timingp = malloc(sizeof(SMILTimingAttributesPointers));
		e->timingp->begin		= &e->timing->begin;
		e->timingp->clipBegin	= &e->timing->clipBegin;
		e->timingp->clipEnd		= &e->timing->clipEnd;
		e->timingp->dur			= &e->timing->dur;
		e->timingp->end			= &e->timing->end;
		e->timingp->fill		= &e->timing->fill;
		e->timingp->max			= &e->timing->max;
		e->timingp->min			= &e->timing->min;
		e->timingp->repeatCount = &e->timing->repeatCount;
		e->timingp->repeatDur	= &e->timing->repeatDur;
		e->timingp->restart		= &e->timing->restart;
		timingp = e->timingp;
	}
#endif
	else {
		return;
	}

	if (!timingp) return;

	GF_SAFEALLOC(rti, SMIL_Timing_RTI)
	rti->timed_elt = timed_elt;
	rti->timingp = timingp;
	rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
	rti->evaluate_status = SMIL_TIMING_EVAL_NONE;	
	rti->intervals = gf_list_new();
	rti->current_interval = NULL;
	rti->evaluate = gf_smil_timing_null_timed_function;
	rti->scene_time = -1;
	gf_smil_timing_init_interval_list(rti);
	interval_index = gf_smil_timing_find_interval_index(rti, GF_MAX_DOUBLE);
	if (interval_index >= 0) {
		rti->current_interval_index = interval_index;
		rti->current_interval = (SMIL_Interval*)gf_list_get(rti->intervals, rti->current_interval_index);
	} 
	rti->media_duration = -1;
	timingp->runtime = rti;

	sg = timed_elt->sgprivate->scenegraph;
	while (sg->parent_scene) sg = sg->parent_scene;
	gf_list_add(sg->smil_timed_elements, rti);
}

void gf_smil_timing_delete_runtime_info(GF_Node *timed_elt, SMIL_Timing_RTI *rti)
{
	GF_SceneGraph *sg;
	u32 i;

	if (!rti || !timed_elt) return;
	for (i = 0; i<gf_list_count(rti->intervals); i++) {
		SMIL_Interval *interval = (SMIL_Interval *)gf_list_get(rti->intervals, i);
		free(interval);
	}
	gf_list_del(rti->intervals);
	sg = timed_elt->sgprivate->scenegraph;
	while (sg->parent_scene) sg = sg->parent_scene;
	gf_list_del_item(sg->smil_timed_elements, rti);
	free(rti);
}

GF_EXPORT
Bool gf_smil_timing_is_active(GF_Node *node) 
{
	SMILTimingAttributesPointers *timingp = NULL;
	u32 tag = gf_node_get_tag(node);

	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		timingp = ((SVGTimedAnimBaseElement *)node)->timingp;
	}
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
		timingp = ((SVG_SA_Element *)node)->timingp;
	}
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		timingp = ((SVG_SANI_Element *)node)->timingp;
	}
#endif
	else {
		return 0;
	}

	if (!timingp || !timingp->runtime) return 0;
	return (timingp->runtime->status == SMIL_STATUS_ACTIVE);
}


void gf_smil_timing_end_notif(SMIL_Timing_RTI *rti, Double clock)
{
	if (rti->current_interval && (rti->current_interval_index>=0) && (rti->current_interval->active_duration<0) ) {
		rti->current_interval->end = clock;
		rti->current_interval->active_duration = rti->current_interval->end - rti->current_interval->begin;
	}
}

/* assumes that the list of time values is sorted */
static void gf_smil_timing_refresh_interval_list(SMIL_Timing_RTI *rti)
{
	u32 i, count, j, count2;
	SMIL_Time *begin;
	SMILTimingAttributesPointers *timingp = rti->timingp;

	if (!timingp) return;

	count = (timingp->begin ? gf_list_count(*timingp->begin) : 0);
	if (!count) {
		if (rti->current_interval)
			gf_smil_timing_add_new_interval(rti, rti->current_interval, rti->current_interval->begin, 0);
		return;
	}

	for (i = 0; i < count; i ++) {
		SMIL_Interval *existing_interval = NULL;
		begin = (SMIL_Time*)gf_list_get(*timingp->begin, i);
		/* this is not an acceptable value for a begin */
		if (! GF_SMIL_TIME_IS_CLOCK(begin->type) ) continue;

		count2 = gf_list_count(rti->intervals);
		for (j=0; j<count2; j++) {
			existing_interval = (SMIL_Interval *)gf_list_get(rti->intervals, j);
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

Bool gf_sg_notify_smil_timed_elements(GF_SceneGraph *sg)
{
	SMIL_Timing_RTI *rti;
	u32 active_count = 0, i = 0;
	s32 ret;
	if (!sg) return 0;

	sg->update_smil_timing = 0;
	while((rti = (SMIL_Timing_RTI *)gf_list_enum(sg->smil_timed_elements, &i))) {
		//scene_time = rti->timed_elt->sgprivate->scenegraph->GetSceneTime(rti->timed_elt->sgprivate->scenegraph->userpriv);
		ret = gf_smil_timing_notify_time(rti, gf_node_get_scene_time((GF_Node*)rti->timed_elt));
		if (ret == -1) // special case for discard element
			i--;
		else 
			active_count += ret;
	}
	/*in case an anim triggers another one previously inactivated...
	TODO FIXME: it would be much better to stack anim as active/inactive*/
	while (sg->update_smil_timing) {
		sg->update_smil_timing = 0;
		i = 0;
		while((rti = (SMIL_Timing_RTI *)gf_list_enum(sg->smil_timed_elements, &i))) {
			/*this means the anim has been, modified, re-evaluate it*/
			if (rti->scene_time==-1) {
				ret = gf_smil_timing_notify_time(rti, gf_node_get_scene_time((GF_Node*)rti->timed_elt) );
				if (ret == -1) // special case for discard element
					i--;
				else 
					active_count += ret;
			}
		}
	}
	return (active_count>0);
}

static Bool gf_smil_discard(SMIL_Timing_RTI *rti, Fixed scene_time)
{
	u32 nb_inst;
	SMIL_Time *begin;
	SMILTimingAttributesPointers *timingp = rti->timingp;
	GF_Node *target;
	u32 tag = gf_node_get_tag(rti->timed_elt);

	if (!timingp) return 0;
	
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		target = ((SVGTimedAnimBaseElement *)rti->timed_elt)->xlinkp->href->target;
	} 
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
		target = ((SVG_SA_Element *)rti->timed_elt)->xlinkp->href->target;
	} 
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		target = ((SVG_SANI_Element *)rti->timed_elt)->xlinkp->href->target;
	}
#endif
	else {
		return 0;
	}
	
	begin = (timingp->begin ? (SMIL_Time *)gf_list_get(*timingp->begin, 0) : NULL);

	if (!begin) return 0;
	if (!GF_SMIL_TIME_IS_CLOCK(begin->type) ) return 0;
	if (!target) return 0;

	if (begin->clock > scene_time) return 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SVG Composer] discarding element %s at time %f\n", gf_node_get_name(target), scene_time));
	
	/*this takes care of cases where discard is a child of its target*/
	gf_node_register(rti->timed_elt, NULL);
	nb_inst = gf_node_get_num_instances(rti->timed_elt);
	gf_node_replace(target, NULL, 0);
	if (nb_inst == gf_node_get_num_instances(rti->timed_elt)) {
		gf_node_unregister(rti->timed_elt, NULL);
		/*after this the stack may be free'd*/
		gf_node_replace(rti->timed_elt, NULL, 0);
	} else {
		gf_node_unregister(rti->timed_elt, NULL);
	}
	return 1;
}

/*Animations are apoplied in their begin order. Whenever an anim (re)starts, it is placed at the end of the queue 
(potentially after frozen animations)*/
static void gf_smil_reorder_anim(SMIL_Timing_RTI *rti)
{
	SMIL_Anim_RTI *rai = (SMIL_Anim_RTI *) gf_smil_anim_get_anim_runtime_from_timing(rti);
	if (rai) {
		gf_list_del_item(rai->owner->anims, rai);
		gf_list_add(rai->owner->anims, rai);
		gf_smil_anim_reset_variables(rai);
	}
}

/* Returns:
	0 if no rendering traversal is required, 
	1 if a rendering traversal is required!!!,
   -1 if the time node is a discard which has been deleted!! */
s32 gf_smil_timing_notify_time(SMIL_Timing_RTI *rti, Double scene_time)
{
	Fixed simple_time;
	s32 ret = 0;
	GF_DOM_Event evt;
	SMILTimingAttributesPointers *timingp = rti->timingp;

	if (!timingp) return 0;
	
	if (rti->scene_time == scene_time) return 0;
	rti->scene_time = scene_time;
	rti->cycle_number++;

	/* for fraction events, we indicate that the scene needs redraw */
	if (rti->evaluate_status == SMIL_TIMING_EVAL_FRACTION) 
		return 1;

	if (rti->evaluate_status == SMIL_TIMING_EVAL_DISCARD) {
		/* TODO: FIX ME discarding should send a begin event ? */
		/* -1 is a special case when the discard is evaluated */
		if (gf_smil_discard(rti, FLT2FIX(rti->scene_time))) return -1;
		else return 0;
	}

waiting_to_begin:
	if (rti->status == SMIL_STATUS_WAITING_TO_BEGIN) {
		if (rti->current_interval && scene_time >= rti->current_interval->begin) {			
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing] Time %f - Activating timed element %s\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
			rti->status = SMIL_STATUS_ACTIVE;

			memset(&evt, 0, sizeof(evt));
			evt.type = GF_EVENT_BEGIN_EVENT;
			evt.smil_event_time = rti->current_interval->begin;
			gf_dom_event_fire((GF_Node *)rti->timed_elt, NULL, &evt);

			if (rti->timed_elt->sgprivate->tag==TAG_SVG_conditional) {
				SVG_Element *e = (SVG_Element *)rti->timed_elt;
				/*activate conditional*/
				if (e->children) gf_node_render(e->children->node, NULL);
				rti->status = SMIL_STATUS_DONE;
			} else {
				gf_smil_reorder_anim(rti);
			}
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing] Time %f - Evaluating timed element %s - Not starting\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
			return ret;
		}
	}

	if (rti->status == SMIL_STATUS_ACTIVE) {
		u32 cur_id;

		if (rti->current_interval->active_duration >= 0 
			&& scene_time >= (rti->current_interval->begin + rti->current_interval->active_duration)) {

			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing] Time %f - Stopping timed element %s\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
			memset(&evt, 0, sizeof(evt));
			evt.type = GF_EVENT_END_EVENT;
			evt.smil_event_time = rti->current_interval->begin + rti->current_interval->active_duration;
			gf_dom_event_fire((GF_Node *)rti->timed_elt, NULL, &evt);

			ret = rti->postpone;

			if (timingp->fill && *timingp->fill == SMIL_FILL_FREEZE) {
				rti->status = SMIL_STATUS_FROZEN;
				rti->first_frozen = rti->cycle_number;
				rti->evaluate_status = SMIL_TIMING_EVAL_FREEZE;
				if (!rti->postpone) {
					Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
					rti->evaluate(rti, simple_time, rti->evaluate_status);
				}
			} else {
				rti->status = SMIL_STATUS_DONE;
				rti->first_frozen = rti->cycle_number;
				rti->evaluate_status = SMIL_TIMING_EVAL_REMOVE;
				if (!rti->postpone) {
					Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
					rti->evaluate(rti, simple_time, rti->evaluate_status);
				}
			}

		}
		/*special case for unspecified simpleDur*/
		else if (rti->current_interval->simple_duration==-1) {
			ret = rti->postpone;
			if (rti->current_interval->active_duration<=0) {
				rti->status = SMIL_STATUS_FROZEN;
				rti->first_frozen = rti->cycle_number;
				rti->evaluate_status = SMIL_TIMING_EVAL_FREEZE;
				if (!rti->postpone) {
					Fixed simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
					rti->evaluate(rti, simple_time, rti->evaluate_status);
				}
			}
		} else { // the animation is still active 


			if (!timingp->restart || *timingp->restart == SMIL_RESTART_ALWAYS) {
				s32 interval_index;
				interval_index = gf_smil_timing_find_interval_index(rti, scene_time);
				
				if (interval_index >= 0 &&
					interval_index != rti->current_interval_index) {
					/* intervals are different, use the new one */
					rti->current_interval_index = interval_index;
					rti->current_interval = (SMIL_Interval*)gf_list_get(rti->intervals, rti->current_interval_index);
				
					gf_smil_reorder_anim(rti);

					memset(&evt, 0, sizeof(evt));
					evt.type = GF_EVENT_BEGIN_EVENT;
					evt.smil_event_time = rti->current_interval->begin;
					gf_dom_event_fire((GF_Node *)rti->timed_elt, NULL, &evt);

				
				} 
			}

			ret = rti->postpone;
			
			cur_id = rti->current_interval->nb_iterations;
			simple_time = gf_smil_timing_get_normalized_simple_time(rti, scene_time);
			if (cur_id < rti->current_interval->nb_iterations) {
				memset(&evt, 0, sizeof(evt));
				evt.type = GF_EVENT_REPEAT_EVENT;
				evt.smil_event_time = rti->current_interval->begin + rti->current_interval->nb_iterations*rti->current_interval->simple_duration;
				evt.detail = rti->current_interval->nb_iterations;
				gf_dom_event_fire((GF_Node *)rti->timed_elt, NULL, &evt);

				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing] Time %f - Repeating timed element %s\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
				rti->evaluate_status = SMIL_TIMING_EVAL_REPEAT;		
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing] Time %f - Updating timed element %s\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
				rti->evaluate_status = SMIL_TIMING_EVAL_UPDATE;
			}

			if (!rti->postpone) {
				rti->evaluate(rti, simple_time, rti->evaluate_status);
			}	
		}
	}

	if ((rti->status == SMIL_STATUS_DONE) || (rti->status == SMIL_STATUS_FROZEN)) {
		if (!timingp->restart || *timingp->restart != SMIL_RESTART_NEVER) { /* Check changes in begin or end attributes */
			s32 interval_index;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing] Time %f - Checking timed element %s for restart\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
			interval_index = gf_smil_timing_find_interval_index(rti, scene_time);
			if (interval_index >= 0 && interval_index != rti->current_interval_index) {
				/* intervals are different, use the new one */
				rti->current_interval_index = interval_index;
				rti->current_interval = (SMIL_Interval*)gf_list_get(rti->intervals, rti->current_interval_index);

				rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
				rti->evaluate_status = SMIL_TIMING_EVAL_NONE;
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

	if (!rti->current_interval) return 0;

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
	GF_SceneGraph *sg;
	SMILTimingAttributesPointers *timingp = NULL;
	SMIL_Timing_RTI *rti;

	u32 tag = gf_node_get_tag(node);
	
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		timingp = ((SVGTimedAnimBaseElement *)node)->timingp;
	} 
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
		timingp = ((SVG_SA_Element *)node)->timingp;
	}
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		timingp = ((SVG_SANI_Element *)node)->timingp;
	}
#endif
	else {
		return;
	}
	
	if (/*!field ||*/ !timingp) return;
	rti = timingp->runtime;
	if (!rti) return;

	/*recompute interval list*/
	rti->scene_time = -1;
	sg = node->sgprivate->scenegraph;
	while (sg->parent_scene) sg = sg->parent_scene;
	sg->update_smil_timing = 1;
	gf_smil_timing_refresh_interval_list(rti);
	if (!rti->current_interval) {
		s32 interval_index;
		interval_index = gf_smil_timing_find_interval_index(rti, GF_MAX_DOUBLE);
		if (interval_index >= 0) {
			rti->current_interval_index = interval_index;
			rti->current_interval = (SMIL_Interval*)gf_list_get(rti->intervals, rti->current_interval_index);
		} 
	}

}

Bool gf_svg_resolve_smil_times(GF_SceneGraph *sg, void *anim_parent, 
							GF_List *smil_times, Bool is_end, const char *node_name)
{
	u32 i, done, count;

	done = 0;
	count = gf_list_count(smil_times);
	for (i=0; i<count; i++) {
		SMIL_Time *t = (SMIL_Time *)gf_list_get(smil_times, i);
		/*not an event*/
		if (t->type != GF_SMIL_TIME_EVENT) {
			done++;
			continue;
		}
		if (!t->element_id) {
			/*event source is anim parent*/
			if (!t->element) t->element = (GF_Node *)anim_parent;
			done++;
			continue;
		}
		if (node_name && strcmp(node_name, t->element_id)) continue;
	
		t->element = gf_sg_find_node_by_name(sg, t->element_id);
		if (t->element) {
			free(t->element_id);
			t->element_id = NULL;
			done++;
		}
	}
	if (done!=count) return 0;
	return 1;
}

void gf_smil_timing_insert_clock(GF_Node *elt, Bool is_end, Double clock)
{
	u32 i, count, found;
	SVGTimedAnimBaseElement *set = (SVGTimedAnimBaseElement*)elt;
	SMIL_Time *begin;
	GF_List *l;
	GF_SAFEALLOC(begin, SMIL_Time);

	begin->type = GF_SMIL_TIME_EVENT_RESOLVED;
	begin->clock = clock;

	l = is_end ? *set->timingp->end : *set->timingp->begin;

	found = 0;
	count = gf_list_count(l);
	for (i=0; i<count; i++) {
		SMIL_Time *first = (SMIL_Time *)gf_list_get(l, i);
		/*remove past instanciations*/
		if ((first->type==GF_SMIL_TIME_EVENT_RESOLVED) && (first->clock < begin->clock)) {
			gf_list_rem(l, i);
			free(first);
			i--;
			count--;
			continue;
		}
		if ( (first->type == GF_SMIL_TIME_INDEFINITE) 
			|| ( (first->type == GF_SMIL_TIME_CLOCK) && (first->clock > begin->clock) ) 
		) {
			gf_list_insert(l, begin, i);
			found = 1;
			break;
		}
	}
	if (!found) gf_list_add(l, begin);
	gf_node_changed(elt, NULL);
}

