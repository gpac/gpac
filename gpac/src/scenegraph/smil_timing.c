/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2004-2022
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
#include <gpac/nodes_svg.h>

#ifndef GPAC_DISABLE_SVG

static void gf_smil_timing_null_timed_function(SMIL_Timing_RTI *rti, Fixed normalized_scene_time, GF_SGSMILTimingEvalState state)
{
}

static void gf_smil_timing_print_interval(SMIL_Timing_RTI *rti, Bool current, SMIL_Interval *interval)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - ", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, (current ? "Current " : "   Next "));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("Interval - "));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("begin: %.2f", interval->begin));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, (" - end: %.2f", interval->end));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, (" - simple dur: %.2f - active dur: %.2f\n",interval->simple_duration, interval->active_duration));
}

/* Computes the active duration for the given interval,
   assumes that the values of begin and end have been set (>0 for real duration or -1 if infinite)
   and that begin is defined (i.e. a positive value, not infinite)*/
static void gf_smil_timing_compute_active_duration(SMIL_Timing_RTI *rti, SMIL_Interval *interval)
{
	Bool clamp_active_duration;
	Bool isDurDefined, isRepeatCountDefined, isRepeatDurDefined, isMinDefined, isMaxDefined, isRepeatDurIndefinite, isRepeatCountIndefinite, isMediaDuration;
	SMILTimingAttributesPointers *timingp = rti->timingp;

	/* TODO: check if the test on begin is right and needed */
	if (!timingp/* || interval->begin == -1*/) return;

	switch (gf_node_get_tag((GF_Node *)rti->timed_elt)) {
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
			interval->repeat_duration = FIX2FLT(timingp->repeatCount->count) * interval->simple_duration;
		} else if (!isRepeatCountDefined && isRepeatDurDefined) {
			interval->repeat_duration = timingp->repeatDur->clock_value;
		} else if (!isRepeatCountDefined && !isRepeatDurDefined) {
			if (isRepeatDurIndefinite || isRepeatCountIndefinite) {
				interval->repeat_duration = -1;
			} else {
				interval->repeat_duration = interval->simple_duration;
			}
		} else {
			interval->repeat_duration = MIN(timingp->repeatDur->clock_value,
			                                FIX2FLT(timingp->repeatCount->count) * interval->simple_duration);
		}
	} else {

		/* simple_duration is indefinite */
		interval->simple_duration = -1;

		/* we can ignore repeatCount to compute active_duration */
		if (!isRepeatDurDefined) {
			interval->repeat_duration = -1;
		} else {
			interval->repeat_duration = timingp->repeatDur->clock_value;
		}
	}

	interval->active_duration = interval->repeat_duration;
	/* Step 2: if end is defined in the document, clamp active duration to end-begin
	otherwise return*/
	if (interval->end < 0) {
		/* interval->active_duration stays as is */
	} else {
		if (interval->active_duration >= 0)
			interval->active_duration = MIN(interval->active_duration, interval->end - interval->begin);
		else
			interval->active_duration = interval->end - interval->begin;
	}

	/* min and max check should be checked last,
	   to ensure that they have greater priority than the end attribute
	   see (animate-elem-223-t.svg) */
	/* Step 3: clamp the active duration with min and max */
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
			if ((interval->active_duration >= 0) &&
			        (interval->active_duration <= timingp->min->clock_value)) {
				/* see http://www.w3.org/TR/2005/REC-SMIL2-20051213/smil-timing.html#Timing-MinMax
				  - if repeat duration or simple duration is smaller than min,
				  then the (active ? / simple ?) duration shall be set to min
				  (cf 6th row in animate-elem-65-t.svg)
				  - if the min > dur > end, the element is played normally for its simple duration
				  and then is frozen or not shown depending on the value of the fill attribute.
				  (cf animate-elem-222-t.svg)*/
				interval->active_duration = timingp->min->clock_value;
				interval->min_active = 1;
			}
		}
		if (isMaxDefined) {
			if ((interval->active_duration >= 0 && interval->active_duration >= timingp->max->clock_value) ||
			        interval->active_duration == -1) {
				interval->active_duration = timingp->max->clock_value;
			}
		}
	}

}

/* This should be called when the dur attribute is set to media and when the media duration is known.
   The function recomputes the active duration of the current interval according to the given media duration */
GF_EXPORT
void gf_smil_set_media_duration(SMIL_Timing_RTI *rti, Double media_duration)
{
	rti->media_duration = media_duration;
	gf_smil_timing_compute_active_duration(rti, rti->current_interval);
}

/* the end value of this interval needs to be initialized before computing the active duration
   the begin value must be >= 0
   The result can be:
    - a positive value meaning that a resolved and non-indefinite value was found
	- the value -1 meaning indefinite or unresolved
		TODO: we should make a difference between indefinite and unresolved because
		if an interval is created with a value of indefinite, this value should not
		be replaced by a resolved event. (Not sure ?!!)
	- the value -2 meaning that a valid end value (including indefinite) could not be found
*/
static void gf_smil_timing_get_interval_end(SMIL_Timing_RTI *rti, SMIL_Interval *interval)
{
	u32 end_count, j;

	/* we set the value to indicate that this is an illegal end,
	   if it stays like that after searching through the values,
	   then the whole interval must be discarded */
	interval->end = -2;

	end_count = (rti->timingp->end ? gf_list_count(*rti->timingp->end) : 0);
	/* trying to find a matching end */
	if (end_count > 0) {
		for (j = 0; j < end_count; j++) {
			SMIL_Time *end = (SMIL_Time*)gf_list_get(*rti->timingp->end, j);
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
}

static void gf_smil_timing_get_first_interval(SMIL_Timing_RTI *rti)
{
	u32 i, count;
	if (!rti || !rti->current_interval) return;
	
	memset(rti->current_interval, 0, sizeof(SMIL_Interval));
	rti->current_interval->begin = -1;
	count = (rti->timingp->begin ? gf_list_count(*rti->timingp->begin) : 0);
	for (i = 0; i < count; i ++) {
		SMIL_Time *begin = (SMIL_Time*)gf_list_get(*rti->timingp->begin, i);
		if (GF_SMIL_TIME_IS_CLOCK(begin->type)) {
			rti->current_interval->begin = begin->clock;
			break;
		}
	}
	/*In SVG, if no 'begin' is specified, the default timing of the time container
	is equivalent to an offset value of '0'.*/
	if (rti->current_interval->begin == -1 && count == 0) {
		/* except for LASeR Conditional element*/
		if (rti->timed_elt->sgprivate->tag != TAG_LSR_conditional) {
			rti->current_interval->begin = 0;
		} else {
			return;
		}
	}

	/* this is the first time we check the interval */
	gf_smil_timing_get_interval_end(rti, rti->current_interval);
	if ((0) && rti->current_interval->end == -2) {
		/* TODO: check if the interval can be discarded (i.e. if end is specified with an invalid end value (return -2)),
		   probably yes, but next time we call the evaluation of interval, we should call get_first_interval */
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Wrong Interval\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
		rti->current_interval->begin = -1;
		rti->current_interval->end = -1;
		return;
	}

	gf_smil_timing_compute_active_duration(rti, rti->current_interval);
	gf_smil_timing_print_interval(rti, 1, rti->current_interval);
}

static Bool gf_smil_timing_get_next_interval(SMIL_Timing_RTI *rti, Bool current, SMIL_Interval *interval, Double scene_time)
{
	u32 i, count;
	if (!interval) return GF_FALSE;
	
	memset(interval, 0, sizeof(SMIL_Interval));
	interval->begin = -1;

	count = (rti->timingp->begin ? gf_list_count(*rti->timingp->begin) : 0);
	for (i = 0; i < count; i ++) {
		SMIL_Time *begin = (SMIL_Time*)gf_list_get(*rti->timingp->begin, i);
		if (GF_SMIL_TIME_IS_CLOCK(begin->type)) {
			if (rti->current_interval->begin != -1 && begin->clock <= rti->current_interval->begin) continue;
//			if (rti->current_interval->begin == -1 || begin->clock <= scene_time) {
			interval->begin = begin->clock;
			break;
//			}
		}
	}
	if (interval->begin != -1) {
		gf_smil_timing_get_interval_end(rti, interval);
		if (interval->end == -2) {
			/* this is a wrong interval see first animation in animate-elem-201-t.svg */
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Wrong Interval\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
			interval->begin = -1;
			interval->end = -1;
			return 0;
		}
		gf_smil_timing_compute_active_duration(rti, interval);
		gf_smil_timing_print_interval(rti, current, interval);
		return 1;
	} else {
		return 0;
	}
}

/* To reduce the process of notifying the time to all timed elements, we add to the scene graph
   only the timed elements which have a resolved current interval, other timed elements will be
   added at runtime when an event leads to the creation of a new interval.
   We also insert the new timed element in the order of the current_interval begin value, to stop
   the notification of time when not necessary */
static Bool gf_smil_timing_add_to_sg(GF_SceneGraph *sg, SMIL_Timing_RTI *rti)
{
	if (rti->current_interval->begin != -1) {
		SMIL_Timing_RTI *cur_rti = NULL;
		u32 i;

		for (i = 0; i < gf_list_count(sg->smil_timed_elements); i++) {
			cur_rti = (SMIL_Timing_RTI *)gf_list_get(sg->smil_timed_elements, i);
			if (cur_rti->current_interval->begin > rti->current_interval->begin) break;
		}
		gf_list_insert(sg->smil_timed_elements, rti, i);
		return 1;
	}
	return 0;
}

/* when a timed element restarts, since the list of timed elements in the scene graph,
   to which scene time is notified at each rendering cycle, is sorted, we need to remove
   and reinsert this timed element as if it was a new one, to make sure the sorting is correct */
static void gf_smil_mark_modified(SMIL_Timing_RTI *rti, Bool remove)
{
	GF_SceneGraph * sg = rti->timed_elt->sgprivate->scenegraph;
	while (sg->parent_scene) sg = sg->parent_scene;
	if (remove) {
		gf_list_del_item(sg->modified_smil_timed_elements, rti);
	} else {
		if (gf_list_find(sg->modified_smil_timed_elements, rti) == -1) {
			gf_list_add(sg->modified_smil_timed_elements, rti);
		}
	}
}

/* Attributes from the timed elements are not easy to use during runtime,
   the runtime info is a set of easy to use structures.
   This function initializes them (intervals, status ...)
   and registers the element with the scenegraph */
GF_EXPORT
void gf_smil_timing_init_runtime_info(GF_Node *timed_elt)
{
	GF_SceneGraph *sg;
	SMIL_Timing_RTI *rti;
	SMILTimingAttributesPointers *timingp;
	u32 tag = gf_node_get_tag(timed_elt);
	SVGAllAttributes all_atts;
	SVGTimedAnimBaseElement *e = (SVGTimedAnimBaseElement *)timed_elt;

	gf_svg_flatten_attributes((SVG_Element *)e, &all_atts);
	e->timingp = gf_malloc(sizeof(SMILTimingAttributesPointers));
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
	if (!timingp) return;

	if (tag == TAG_SVG_audio || tag == TAG_SVG_video) {
		/* if the dur attribute is not set, then it should be set to media
		   as this is the default for media elements see
		   http://www.w3.org/TR/2005/REC-SMIL2-20051213/smil-timing.html#Timing-DurValueSemantics
		   "For simple media elements that specify continuous media (i.e. media with an inherent notion of time),
		   the implicit duration is the intrinsic duration of the media itself - e.g. video and audio files
		   have a defined duration."
		TODO: Check if this should work with the animation element */
		if (!e->timingp->dur) {
			GF_FieldInfo info;
			gf_node_get_attribute_by_tag((GF_Node *)e, TAG_SVG_ATT_dur, 1, 0, &info);
			e->timingp->dur = (SMIL_Duration *)info.far_ptr;
			e->timingp->dur->type = SMIL_DURATION_MEDIA;
		}
	}

	GF_SAFEALLOC(rti, SMIL_Timing_RTI)
	if (!rti) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPTIME, ("[SMIL Timing] Failed to alloc SMIL timing RTI\n"));
		return;
	}
	timingp->runtime = rti;
	rti->timed_elt = timed_elt;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Initialization\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));

	rti->timingp = timingp;
	rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
	rti->evaluate_status = SMIL_TIMING_EVAL_NONE;
	rti->evaluate = gf_smil_timing_null_timed_function;
	rti->scene_time = -1;
	rti->force_reevaluation = 0;
	rti->media_duration = -1;

	GF_SAFEALLOC(rti->current_interval, SMIL_Interval);
	if (!rti->current_interval) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPTIME, ("[SMIL Timing] Failed to alloc SMIL timing current interval\n"));
		return;
	}
	gf_smil_timing_get_first_interval(rti);
	GF_SAFEALLOC(rti->next_interval, SMIL_Interval);
	if (!rti->next_interval) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPTIME, ("[SMIL Timing] Failed to alloc SMIL timing next interval\n"));
		return;
	}
	gf_smil_timing_get_next_interval(rti, 0, rti->next_interval, rti->current_interval->begin);

	/* Now that the runtime info for this timed element is initialized, we can tell the scene graph that it can start
	   notifying the scene time to this element. Because of the 'animation' element, we can have many scene graphs
	   sharing the same scene time, we therefore add this timed element to the rootmost scene graph. */
	sg = timed_elt->sgprivate->scenegraph;
	while (sg->parent_scene) sg = sg->parent_scene;
	gf_smil_timing_add_to_sg(sg, rti);
}


static void gf_smil_timing_reset_time_list(GF_List *times)
{
	GF_DOMEventTarget *evt;
	u32 i;
	for (i=0; i<gf_list_count(times); i++) {
		SMIL_Time *t = gf_list_get(times, i);
		if (!t->listener) continue;

		/*detach the listener from the observed node*/
		evt = t->listener->sgprivate->UserPrivate;
		t->listener->sgprivate->UserPrivate = NULL;
		gf_dom_listener_del(t->listener, evt);

		/*release our listener*/
		gf_node_unregister(t->listener, NULL);
		t->listener = NULL;
	}
}

void gf_smil_timing_delete_runtime_info(GF_Node *timed_elt, SMIL_Timing_RTI *rti)
{
	GF_SceneGraph *sg;

	if (!rti || !timed_elt) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Destruction\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
	gf_free(rti->current_interval);
	gf_free(rti->next_interval);

	/* we inform the rootmost scene graph that this node will not need notification of the scene time anymore */
	sg = timed_elt->sgprivate->scenegraph;
	while (sg->parent_scene) sg = sg->parent_scene;
	gf_list_del_item(sg->smil_timed_elements, rti);
	gf_list_del_item(sg->modified_smil_timed_elements, rti);

	/*remove all associated listeners*/
	if (rti->timingp->begin) gf_smil_timing_reset_time_list(* rti->timingp->begin);
	if (rti->timingp->end) gf_smil_timing_reset_time_list(* rti->timingp->end);

	gf_free(rti);
}

GF_EXPORT
Bool gf_smil_timing_is_active(GF_Node *node)
{
	SMILTimingAttributesPointers *timingp = ((SVGTimedAnimBaseElement *)node)->timingp;
	if (!timingp || !timingp->runtime) return 0;
	return (timingp->runtime->status == SMIL_STATUS_ACTIVE);
}

Bool gf_smil_notify_timed_elements(GF_SceneGraph *sg)
{
	SMIL_Timing_RTI *rti;
	u32 active_count, i;
	s32 ret;
	Bool do_loop;
	if (!sg) return 0;

	active_count = 0;

	/*
		Note: whenever a timed element is active, we trigger a gf_node_dirty_parent_graph so that the parent graph
		is aware that some modifications may happen in the subtree. This is needed for cases where the subtree
		is in an offscreen surface, to force retraversing of the subtree and thus apply the animation.

	*/

	/* notify the new scene time to the register timed elements
	   this might modify other timed elements or the element itself
	   in which case it will be added to the list of modified elements */
	i = 0;
	do_loop = 1;
	while(do_loop && (rti = (SMIL_Timing_RTI *)gf_list_enum(sg->smil_timed_elements, &i))) {
		ret = gf_smil_timing_notify_time(rti, gf_node_get_scene_time((GF_Node*)rti->timed_elt) );
		switch (ret) {
		case -1:
			/* special case for discard element
			   when a discard element is executed, it automatically removes itself from the list of timed element
			   in the scene graph, we need to fix the index i. */
			i--;
			break;
		case -2:
			/* special return value, -2 means that the tested timed element is waiting to begin
			   Assuming that the timed elements are sorted by begin order,
			   the next ones don't need to be checked */
			do_loop = 0;
			break;
		case -3:
			/* special case for animation elements which do not need to be notified anymore,
			   but which require a tree traversal */
			i--;
			active_count ++;
			gf_node_dirty_parent_graph(rti->timed_elt);
			break;
		case 1:
			active_count++;
			gf_node_dirty_parent_graph(rti->timed_elt);
			break;
		case 0:
		default:
			break;
		}
	}

	/* notify the timed elements which have been modified either since the previous frame (updates, scripts) or
	   because of the start/end/repeat of the previous notifications */
	while (gf_list_count(sg->modified_smil_timed_elements)) {
		/* first remove the modified smil timed element */
		rti = gf_list_get(sg->modified_smil_timed_elements, 0);
		gf_list_rem(sg->modified_smil_timed_elements, 0);

		/* then remove it in the list of non modified (if it was there) */
		gf_list_del_item(sg->smil_timed_elements, rti);

		/* then insert it at its right position (in the sorted list of timed elements) */
		gf_smil_timing_add_to_sg(sg, rti);

		/* finally again notify this timed element */
		rti->force_reevaluation = 1;
		ret = gf_smil_timing_notify_time(rti, gf_node_get_scene_time((GF_Node*)rti->timed_elt) );
		switch (ret) {
		case -1:
			break;
		case -2:
			break;
		case -3:
			active_count++;
			gf_node_dirty_parent_graph(rti->timed_elt);
			break;
		case 1:
			active_count++;
			gf_node_dirty_parent_graph(rti->timed_elt);
			break;
		case 0:
		default:
			break;
		}

	}
	return (active_count>0);
}

/* evaluation function for the discard element
   returns 1 if the discard was executed
           0 otherwise
*/
static Bool gf_smil_discard(SMIL_Timing_RTI *rti, Fixed scene_time)
{
	u32 nb_inst;
	SMIL_Time *begin;
	SVGTimedAnimBaseElement *tb = (SVGTimedAnimBaseElement *)rti->timed_elt;
	SMILTimingAttributesPointers *timingp = (SMILTimingAttributesPointers *)rti->timingp;
	GF_Node *target;

	if (!timingp) return 0;

	target = tb->xlinkp->href ? tb->xlinkp->href->target : NULL;

	begin = (timingp->begin ? (SMIL_Time *)gf_list_get(*timingp->begin, 0) : NULL);

	if (!begin) return 0;
	if (!GF_SMIL_TIME_IS_CLOCK(begin->type) ) return 0;

	if (begin->clock > scene_time) return 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SVG Composer] discarding element %s at time %f\n", target ? gf_node_get_log_name(target) : "None", scene_time));

	gf_smil_mark_modified(rti, 1);

	/*this takes care of cases where discard is a child of its target*/
	gf_node_register(rti->timed_elt, NULL);
	nb_inst = gf_node_get_num_instances(rti->timed_elt);
	if (target) gf_node_replace(target, NULL, 0);
	if (nb_inst == gf_node_get_num_instances(rti->timed_elt)) {
		gf_node_unregister(rti->timed_elt, NULL);
		/*after this the stack may be free'd*/
		gf_node_replace(rti->timed_elt, NULL, 0);
	} else {
		gf_node_unregister(rti->timed_elt, NULL);
	}
	return 1;
}

/* Animations are applied in their begin order first and then in document order.
   Whenever an animation (re)starts, it is placed at the end of the queue (potentially after frozen animations) */
static void gf_smil_reorder_anim(SMIL_Timing_RTI *rti)
{
	SMIL_Anim_RTI *rai = rti->rai;
	if (rai) {
		gf_list_del_item(rai->owner->anims, rai);
		gf_list_add(rai->owner->anims, rai);
		gf_smil_anim_reset_variables(rai);
	}
}

/* Notifies the scene time to a timed element, potentially changing its status and triggering its evaluation
   Returns:
	0 if no rendering traversal is required,
	1 if a rendering traversal is required,
   -1 if the time node is a discard which has been deleted during this notification,
   -2 means that the timed element is waiting to begin,
   -3 means that the timed element is active but does not need further notifications (set without dur)
             but still requires a rendering traversal */
s32 gf_smil_timing_notify_time(SMIL_Timing_RTI *rti, Double in_scene_time)
{
	s32 ret = 0;
	GF_DOM_Event evt;
	SMILTimingAttributesPointers *timingp = rti->timingp;
	Bool force_end = 0;

	if (!timingp) return 0;

	/* if the scene time is the same as it was during the previous notification, it means that the
	   animations are paused and we don't need to evaluate it again unless the force_reevaluation flag is set */
	if ((rti->scene_time == in_scene_time) && (rti->force_reevaluation == 0)) return 0;
	if (!rti->paused) rti->scene_time = in_scene_time;
	rti->force_reevaluation = 0;

	/* for fraction events, in all cases we indicate that the scene needs redraw */
	if (rti->evaluate_status == SMIL_TIMING_EVAL_FRACTION)
		return 1;

	if (rti->evaluate_status == SMIL_TIMING_EVAL_DISCARD) {
		/* TODO: FIX ME discarding should send a begin event ? */
		/* Since the discard can only be evaluated once, it unregisters itself
		   from the list of timed elements to be notified, so for this special case
		   we return -1 when the discard has actually been executed */
		if (gf_smil_discard(rti, FLT2FIX(rti->scene_time))) return -1;
		else return 0;
	}

	gf_node_register(rti->timed_elt, NULL);

waiting_to_begin:
	if (rti->status == SMIL_STATUS_WAITING_TO_BEGIN) {
		if (rti->current_interval->begin != -1 && rti->scene_time >= rti->current_interval->begin) {
			/* if there is a computed interval with a definite begin value
			   and if that value is lesser than the scene time, then the animation becomes active */
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Activating\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
			rti->status = SMIL_STATUS_ACTIVE;

			if (rti->timed_elt->sgprivate->tag==TAG_LSR_conditional) {
				SVG_Element *e = (SVG_Element *)rti->timed_elt;
				/*activate conditional*/
				if (e->children) gf_node_traverse(e->children->node, NULL);
				rti->status = SMIL_STATUS_DONE;
			} else {
				gf_smil_reorder_anim(rti);
			}

			memset(&evt, 0, sizeof(evt));
			evt.type = GF_EVENT_BEGIN_EVENT;
			evt.smil_event_time = rti->current_interval->begin;
			gf_dom_event_fire((GF_Node *)rti->timed_elt, &evt);
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Evaluating (Not starting)\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
			ret = -2;
			goto exit;
		}
	}

	if (rti->status == SMIL_STATUS_ACTIVE) {
		u32 cur_id;

		if (rti->current_interval->active_duration >= 0
		        && rti->scene_time >= (rti->current_interval->begin + rti->current_interval->active_duration)) {
force_end:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Stopping \n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));

			rti->normalized_simple_time = gf_smil_timing_get_normalized_simple_time(rti, rti->scene_time, NULL);
			ret = rti->postpone;

			if (timingp->fill && *timingp->fill == SMIL_FILL_FREEZE) {
				rti->status = SMIL_STATUS_FROZEN;
				rti->evaluate_status = SMIL_TIMING_EVAL_FREEZE;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Preparing to freeze\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
				if (!rti->postpone) {
					rti->evaluate(rti, rti->normalized_simple_time, rti->evaluate_status);
				}
			} else {
				rti->status = SMIL_STATUS_DONE;
				rti->evaluate_status = SMIL_TIMING_EVAL_REMOVE;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Preparing to remove\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
				if (!rti->postpone) {
					rti->evaluate(rti, rti->normalized_simple_time, rti->evaluate_status);
				}
			}

			memset(&evt, 0, sizeof(evt));
			evt.type = GF_EVENT_END_EVENT;
			/* WARNING: begin + active_duration may be greater than 'now' because of force_end cases */
			evt.smil_event_time = rti->current_interval->begin + rti->current_interval->active_duration;
			gf_dom_event_fire((GF_Node *)rti->timed_elt, &evt);

		} else { /* the animation is still active */

			if (!timingp->restart || *timingp->restart == SMIL_RESTART_ALWAYS) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Checking for restart (always)\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));

				if (rti->next_interval->begin != -1 && rti->next_interval->begin < rti->scene_time) {
					*rti->current_interval = *rti->next_interval;
					gf_smil_timing_get_next_interval(rti, 0, rti->next_interval, rti->scene_time);

					/* mark that this element has been modified and
					   need to be reinserted at its proper place in the list of timed elements in the scenegraph */
					gf_smil_mark_modified(rti, 0);

					/* if this is animation, reinserting the animation in the list of animations
					   that targets this attribute, so that it is the last one */
					gf_smil_reorder_anim(rti);

					memset(&evt, 0, sizeof(evt));
					evt.type = GF_EVENT_BEGIN_EVENT;
					evt.smil_event_time = rti->current_interval->begin;
					gf_dom_event_fire((GF_Node *)rti->timed_elt, &evt);
				}
			}

			ret = rti->postpone;

			cur_id = rti->current_interval->nb_iterations;
			rti->normalized_simple_time = gf_smil_timing_get_normalized_simple_time(rti, rti->scene_time, &force_end);
			if (force_end) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Forcing end (fill or remove)\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
				goto force_end;
			}
			if (cur_id < rti->current_interval->nb_iterations) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Preparing to repeat\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
				memset(&evt, 0, sizeof(evt));
				evt.type = GF_EVENT_REPEAT_EVENT;
				evt.smil_event_time = rti->current_interval->begin + rti->current_interval->nb_iterations*rti->current_interval->simple_duration;
				evt.detail = rti->current_interval->nb_iterations;
				gf_dom_event_fire((GF_Node *)rti->timed_elt, &evt);

				rti->evaluate_status = SMIL_TIMING_EVAL_REPEAT;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Preparing to update\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
				rti->evaluate_status = SMIL_TIMING_EVAL_UPDATE;
			}

			if (!rti->postpone) {
				rti->evaluate(rti, rti->normalized_simple_time, rti->evaluate_status);
			}

			/* special case for animations with unspecified simpleDur (not with media timed elements)
			   we need to indicate that this anim does not need to be notified anymore and that
			   it does not require tree traversal */
			if (gf_svg_is_animation_tag(rti->timed_elt->sgprivate->tag)
			        && (rti->current_interval->simple_duration==-1)
			        && (rti->current_interval->active_duration==-1)
			   ) {
				/*GF_SceneGraph * sg = rti->timed_elt->sgprivate->scenegraph;
				while (sg->parent_scene) sg = sg->parent_scene;
				gf_list_del_item(sg->smil_timed_elements, rti);
				ret = -3;*/
				ret = 1;
			}
		}
	}

	if ((rti->status == SMIL_STATUS_DONE) || (rti->status == SMIL_STATUS_FROZEN)) {
		if (!timingp->restart || *timingp->restart != SMIL_RESTART_NEVER) {
			/* Check changes in begin or end attributes */
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Checking for restart when not active\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
			if (rti->next_interval->begin != -1) {
				Bool restart_timing = 0;
				/*next interval is right now*/
				if (rti->next_interval->begin == rti->current_interval->begin+rti->current_interval->active_duration)
					restart_timing = 1;

				/*switch intervals*/
				if (rti->next_interval->begin >= rti->current_interval->begin+rti->current_interval->active_duration) {
					*rti->current_interval = *rti->next_interval;

					gf_smil_timing_print_interval(rti, 1, rti->current_interval);
					gf_smil_timing_get_next_interval(rti, 0, rti->next_interval, rti->scene_time);

					/* mark that this element has been modified and
					   need to be reinserted at its proper place in the list of timed elements in the scenegraph */
					gf_smil_mark_modified(rti, 0);
				} else {
					rti->next_interval->begin = -1;
				}

				/*if chaining to new interval, go to wait_for begin right now*/
				if (restart_timing) {
					rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
					rti->evaluate_status = SMIL_TIMING_EVAL_NONE;
					GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Returning to eval none status\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
					ret = 0;
					goto waiting_to_begin;
				}
				/*otherwise move state to waiting for begin for next smil_timing evaluation, but
				don't change evaluate status for next anim evaluation*/
				else {
					rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
				}
			} else {
				/*??? what is this ???*/
				//ret = 0;
			}
		} else if ((rti->status == SMIL_STATUS_DONE) &&
		           timingp->restart && (*timingp->restart == SMIL_RESTART_NEVER)) {
			/* the timed element is done and cannot restart, we don't need to evaluate it anymore */
			GF_SceneGraph * sg = rti->timed_elt->sgprivate->scenegraph;
			while (sg->parent_scene) sg = sg->parent_scene;
			gf_list_del_item(sg->smil_timed_elements, rti);
			ret = -1;
		}
	}

exit:
	gf_node_unregister(rti->timed_elt, NULL);
	return ret;
}

/* returns a fraction between 0 and 1 of the elapsed time in the simple duration */
/* WARNING: According to SMIL (http://www.w3.org/TR/2005/REC-SMIL2-20051213/animation.html#animationNS-Fill,
see "Illustration of animation combining a partial repeat and fill="freeze"")
When a element is frozen, its normalized simple time is not necessarily 1,
an animation can be frozen in the middle of a repeatition */
Fixed gf_smil_timing_get_normalized_simple_time(SMIL_Timing_RTI *rti, Double scene_time, Bool *force_end)
{
	Double activeTime;
	Double simpleTime;
	Fixed normalizedSimpleTime;

	if (rti->current_interval->begin == -1) return 0;

	/* we define the active time as the elapsed time from the current activation of the element */
	activeTime = scene_time - rti->current_interval->begin;

	/* Is the animation reaching the end of its active duration ? */
	if (rti->current_interval->active_duration != -1 && activeTime >= rti->current_interval->active_duration) {

		/* we clamp the active time to its maximum value */
		activeTime = rti->current_interval->active_duration;

		/* if the simple duration is defined, then we can take iterations into account */
		if (rti->current_interval->simple_duration>0) {

			if (activeTime == rti->current_interval->simple_duration*(rti->current_interval->nb_iterations+1)) {
				return FIX_ONE;
			} else {
				goto end;
			}
		} else {
			/* If the element does not define its simple duration, but it has an active duration,
			   and we are past this active duration, we assume it's blocked in final state
			   We should take into account fill behavior*/
			rti->current_interval->nb_iterations = 0;
			if (rti->timingp->fill && *(rti->timingp->fill) == SMIL_FILL_FREEZE) {
				if (rti->current_interval->repeat_duration == rti->current_interval->simple_duration) {
					return FIX_ONE;
				} else {
					return	rti->normalized_simple_time;
				}
			} else {
				return 0;
			}
		}
	}

end:
	/* if the simple duration is defined, then we can take iterations into account */
	if (rti->current_interval->simple_duration>0) {

		/* if we are active but frozen or done (animate-elem-65-t, animate-elem-222-t)
		(see The rule to apply to compute the active duration of an element with min or max specified) */
		if ((activeTime >= rti->current_interval->repeat_duration) && rti->current_interval->min_active) {
			/* freeze the normalized simple time */
			if (force_end) *force_end = 1;
			if (rti->timingp->fill && *(rti->timingp->fill) == SMIL_FILL_FREEZE) {
				if (rti->current_interval->repeat_duration == rti->current_interval->simple_duration) {
					return FIX_ONE;
				} else {
					return	rti->normalized_simple_time;
				}
			}
		}
		/* we update the number of iterations */
		rti->current_interval->nb_iterations = (u32)floor(activeTime / rti->current_interval->simple_duration);
	} else {
		/* If the element does not define its simple duration, we assume it's blocked in final state
		   Is this correct ? */
		rti->current_interval->nb_iterations = 0;
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Error Computing Normalized Simple Time while simple duration is indefinite\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
		return FIX_ONE;
	}

	/* We compute the simple time by removing time taken by previous iterations */
	simpleTime = activeTime - rti->current_interval->simple_duration * rti->current_interval->nb_iterations;

	/* Then we clamp the simple time to be sure it is between 0 and simple duration */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(rti->current_interval->simple_duration, simpleTime);

	/* Then we normalize to have a value between 0 and 1 */
	normalizedSimpleTime = FLT2FIX(simpleTime / rti->current_interval->simple_duration);

	return normalizedSimpleTime;
}

/* This function is called when a modification to the node has been made (scripts, updates or events ...) */
void gf_smil_timing_modified(GF_Node *node, GF_FieldInfo *field)
{
	SMIL_Timing_RTI *rti;
	SMILTimingAttributesPointers *timingp = ((SVGTimedAnimBaseElement *)node)->timingp;

	if (!timingp) return;
	rti = timingp->runtime;
	if (!rti) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Modification\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
	if (rti->current_interval->begin == -1) {
		gf_smil_timing_get_next_interval(rti, 1, rti->current_interval, gf_node_get_scene_time((GF_Node*)rti->timed_elt));
	} else {
		/* we don't have the right to modify the end of an element if it's not in unresolved state */
		if (rti->current_interval->end == -1) gf_smil_timing_get_interval_end(rti, rti->current_interval);
		if ((0) && rti->current_interval->end == -2) {
			/* TODO: check if the interval can be discarded if end = -2,
			   probably no, because the interval is currently running*/
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[SMIL Timing   ] Time %f - Timed element %s - Wrong Interval\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_log_name((GF_Node *)rti->timed_elt)));
			rti->current_interval->begin = -1;
			rti->current_interval->end = -1;
			return;
		}

		gf_smil_timing_compute_active_duration(rti, rti->current_interval);
		gf_smil_timing_print_interval(rti, 1, rti->current_interval);
	}
	gf_smil_timing_get_next_interval(rti, 0, rti->next_interval, gf_node_get_scene_time((GF_Node*)rti->timed_elt));

	/* mark that this element has been modified and
	   need to be reinserted at its proper place in the list of timed elements in the scenegraph */
	gf_smil_mark_modified(rti, 0);
}


/* Tries to resolve event-based or sync-based time values
   Used in parsing, to determine if a timed element can be initialized */
Bool gf_svg_resolve_smil_times(GF_Node *anim, void *event_base_element,
                               GF_List *smil_times, Bool is_end, const char *node_name)
{
	u32 i, done, count;

	done = 0;
	count = gf_list_count(smil_times);
	for (i=0; i<count; i++) {
		SMIL_Time *t = (SMIL_Time *)gf_list_get(smil_times, i);

		if (t->type != GF_SMIL_TIME_EVENT) {
			done++;
			continue;
		}
		if (!t->element_id) {
			if (!t->element) t->element = (GF_Node *)event_base_element;
			done++;
			continue;
		}
		/*commented out because it breaks regular anims (cf interact-pevents-07-t.svg)*/
//		if (node_name && strcmp(node_name, t->element_id)) continue;

		t->element = gf_sg_find_node_by_name(anim->sgprivate->scenegraph, t->element_id);
		if (t->element) {
			gf_free(t->element_id);
			t->element_id = NULL;
			done++;
		}
	}
	/*lacuna value of discard is 0*/
	if (!count && !is_end && (anim->sgprivate->tag==TAG_SVG_discard) ) {
		SMIL_Time *t;
		GF_SAFEALLOC(t, SMIL_Time);
		if (!t) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_COMPTIME, ("[SMIL Timing] Failed to alloc SMIL time for discard\n"));
			return 0;
		}
		t->clock = 0;
		t->type = GF_SMIL_TIME_CLOCK;
		gf_list_add(smil_times, t);
		return 1;
	}

	if (done!=count) return 0;
	return 1;
}

GF_EXPORT
void gf_smil_timing_insert_clock(GF_Node *elt, Bool is_end, Double clock)
{
	u32 i, count, found;
	SVGTimedAnimBaseElement *timed = (SVGTimedAnimBaseElement*)elt;
	SMIL_Time *begin;
	GF_List *l;
	GF_SAFEALLOC(begin, SMIL_Time);
	if (!begin) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPTIME, ("[SMIL Timing] Failed to alloc SMIL begin value\n"));
		return;
	}

	begin->type = GF_SMIL_TIME_EVENT_RESOLVED;
	begin->clock = clock;

	l = is_end ? *timed->timingp->end : *timed->timingp->begin;

	found = 0;
	count = gf_list_count(l);
	for (i=0; i<count; i++) {
		SMIL_Time *first = (SMIL_Time *)gf_list_get(l, i);
		/*remove past instanciations*/
		if ((first->type==GF_SMIL_TIME_EVENT_RESOLVED) && (first->clock < begin->clock)) {
			gf_list_rem(l, i);
			gf_free(first);
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

	/* call gf_smil_timing_modified */
	gf_node_changed(elt, NULL);
}

void gf_smil_timing_pause(GF_Node *node)
{
	if (node && ((SVGTimedAnimBaseElement *)node)->timingp  && ((SVGTimedAnimBaseElement *)node)->timingp->runtime) {
		SMIL_Timing_RTI *rti = ((SVGTimedAnimBaseElement *)node)->timingp->runtime;
		if (rti->status<=SMIL_STATUS_ACTIVE) rti->paused = 1;
	}
}

void gf_smil_timing_resume(GF_Node *node)
{
	if (node && ((SVGTimedAnimBaseElement *)node)->timingp  && ((SVGTimedAnimBaseElement *)node)->timingp->runtime) {
		SMIL_Timing_RTI *rti = ((SVGTimedAnimBaseElement *)node)->timingp->runtime;
		rti->paused = 0;
	}
}

void gf_smil_set_evaluation_callback(GF_Node *node, gf_sg_smil_evaluate smil_evaluate)
{
	if (node && ((SVGTimedAnimBaseElement *)node)->timingp  && ((SVGTimedAnimBaseElement *)node)->timingp->runtime) {
		SMIL_Timing_RTI *rti = ((SVGTimedAnimBaseElement *)node)->timingp->runtime;
		rti->evaluate = smil_evaluate;
	}
}

GF_Node *gf_smil_get_element(SMIL_Timing_RTI *rti)
{
	return rti->timed_elt;
}

Double gf_smil_get_media_duration(SMIL_Timing_RTI *rti)
{
	return rti ? rti->media_duration : 0.0;
}


#endif /*GPAC_DISABLE_SVG*/
