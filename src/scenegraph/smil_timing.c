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

static void gf_smil_timing_print_interval(SMIL_Timing_RTI *rti, SMIL_Interval *interval)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - ", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("Interval - "));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("begin: %.2f", interval->begin));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, (" - end: %.2f", interval->end));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, (" - simple dur: %.2f - active dur: %.2f\n",interval->simple_duration, interval->active_duration));
}

/* Computes the active duration for the given interval,
   assumes that the values of begin and end have been set (>0 for real duration or -1 if infinite)
   and that begin is defined (i.e. a positive value, not infinite)*/
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

/* This should be called when the dur attribute is set to media and when the media duration is known.
   The function recomputes the active duration of the current interval according to the given media duration */
GF_EXPORT
void gf_smil_set_media_duration(SMIL_Timing_RTI *rti, Double media_duration)
{
	rti->media_duration = media_duration;
	gf_smil_timing_compute_active_duration(rti, rti->current_interval);
}

#ifdef NO_INTERVAL_LIST

/* the end value of this interval needs to be initialized before computing the active duration
   the begin value must be >= 0 */ 
static void gf_smil_timing_get_interval_end(SMIL_Timing_RTI *rti, SMIL_Interval *interval)
{
	u32 end_count, j;
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
	if (rti->current_interval->begin == -1 && count == 0) {
		/* except for LASeR Conditional element*/
		if (rti->timed_elt->sgprivate->tag != TAG_SVG_conditional) {
			rti->current_interval->begin = 0;
		} else {
			return;
		}
	}
		
	gf_smil_timing_get_interval_end(rti, rti->current_interval);

	gf_smil_timing_compute_active_duration(rti, rti->current_interval);
		
	gf_smil_timing_print_interval(rti, rti->current_interval);
}

static Bool gf_smil_timing_get_next_interval(SMIL_Timing_RTI *rti, SMIL_Interval *interval, Double scene_time)
{
	u32 i, count;

	memset(interval, 0, sizeof(SMIL_Interval));
	interval->begin = -1;
	
	count = (rti->timingp->begin ? gf_list_count(*rti->timingp->begin) : 0);
	for (i = 0; i < count; i ++) {
		SMIL_Time *begin = (SMIL_Time*)gf_list_get(*rti->timingp->begin, i);
		if (GF_SMIL_TIME_IS_CLOCK(begin->type)) {
			if (rti->current_interval->begin != -1 && begin->clock <= rti->current_interval->begin) continue;
			if (rti->current_interval->begin == -1 || begin->clock <= scene_time) {
				interval->begin = begin->clock;
				break;
			}
		}
	}
	if (interval->begin != -1) {
		gf_smil_timing_get_interval_end(rti, interval);
		gf_smil_timing_compute_active_duration(rti, interval);
		gf_smil_timing_print_interval(rti, interval);
		return 1;
	} else {
		return 0;
	}
}


#else

/* Finds in the list of intervals, the first interval after the current one (if any), 
   for which the begin value is less than the given scene time. Since the list is sorted 
   according to the begin value, the returned interval is the one that has the smallest begin.*/
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

/*
	The function either adds a new interval to the current list of intervals if the given interval 
	parameter is NULL, otherwise it modifies the given interval. 
	It also sets the begin value of this (new or not) interval with the given begin value, 
	and the end value according to the attributes specified in the timed element.
	The index parameter helps improving the insertion by giving the index of the previous end attribute value 
	for which an interval was created. In case of doubt, use 0.
*/
static void gf_smil_timing_add_new_interval(SMIL_Timing_RTI *rti, SMIL_Interval *interval, Double begin, u32 index)
{
	u32 j, end_count;
	SMIL_Time *end;
	SMILTimingAttributesPointers *timingp = rti->timingp;

	if (!timingp) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - nb intervals %d\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt),gf_list_count(rti->intervals)));

	if (!interval) {
		GF_SAFEALLOC(interval, SMIL_Interval)
		gf_list_add(rti->intervals, interval);
	}
	interval->begin = begin;


	/* the end value of this interval needs to be initialized before computing the active duration */ 
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

	gf_smil_timing_print_interval(rti, interval);
}

/* Initialization of the list of intervals associated with this timed element
   This fonction assumes that the list of time values in the begin attribute is sorted */
static void gf_smil_timing_init_interval_list(SMIL_Timing_RTI *rti)
{
	u32 i, count;
	SMIL_Time *begin;
	SMILTimingAttributesPointers *timingp = rti->timingp;

	if (!timingp) return;

	count = (timingp->begin ? gf_list_count(*timingp->begin) : 0);
	if (count) {
		/* For each numeric begin value, we create a new interval,
		   we do not reuse the same end value for the new interval (is it correct ? TODO: check)
		*/
		for (i = 0; i < count; i ++) {
			begin = (SMIL_Time*)gf_list_get(*timingp->begin, i);
			if (GF_SMIL_TIME_IS_CLOCK(begin->type) ) {
				/* we create an acceptable only if begin is defined (clock or wallclock) */
				gf_smil_timing_add_new_interval(rti, NULL, begin->clock, i);
			} else {
				/* this is not an acceptable value for a begin */
			}
		} 
	} else {
		/* In SVG, if no begin is specified, the default timing of the time container is 
		equivalent to an offset value of 0, so a new interval should be created but in LASeR, 
		the conditional element has a default begin value of indefinite, in which case no interval is created */
		if (rti->timed_elt->sgprivate->tag != TAG_SVG_conditional) {
			gf_smil_timing_add_new_interval(rti, NULL, 0, 0);
		}
	}
}

#endif


/* To reduce the process of notifying the time to each timed element, we add to the scene graph 
   only the timed elements which have a resolved current interval, other timed elements will be 
   added at runtime when an event leads to the creation of a new interval.
   We also insert the new timed element in the order of the current_interval begin value, to stop 
   the notification of time when not necessary */
static Bool gf_smil_timing_add_to_sg(GF_SceneGraph *sg, SMIL_Timing_RTI *rti)
{
#ifdef NO_INTERVAL_LIST
	if (rti->current_interval->begin != -1) {
#else
	if (rti->current_interval) {
#endif
		SMIL_Timing_RTI *cur_rti = NULL;
		u32 i, count;

		count = gf_list_count(sg->smil_timed_elements);
		for (i = 0; i < count; i++) {
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
static void gf_smil_reorder_timing(SMIL_Timing_RTI *rti)
{
	GF_SceneGraph * sg = rti->timed_elt->sgprivate->scenegraph;
	while (sg->parent_scene) sg = sg->parent_scene;
	gf_list_del_item(sg->smil_timed_elements, rti);
	gf_smil_timing_add_to_sg(sg, rti);
}

/* Attributes from the timed elements are not easy to use during runtime, the runtime info is a set of easy to use 
   structures. This function initializes them (intervals, status ...). */
GF_EXPORT
void gf_smil_timing_init_runtime_info(GF_Node *timed_elt)
{
#ifndef NO_INTERVAL_LIST
	s32 interval_index;
#endif
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

		if (tag == TAG_SVG_audio || tag == TAG_SVG_video) {
			/* if the dur attribute is not set, then it should be set to media 
			   as this is the default for media elements see 
			   http://www.w3.org/TR/2005/REC-SMIL2-20051213/smil-timing.html#Timing-DurValueSemantics
			   "For simple media elements that specify continuous media 
			   (i.e. media with an inherent notion of time), the implicit duration is 
			   the intrinsic duration of the media itself - e.g. video and audio files 
			   have a defined duration."
			Check if this should work with the animation element */
			if (!e->timingp->dur) {
				SVGAttribute *att = gf_svg_create_attribute((GF_Node *)e, TAG_SVG_ATT_dur);
				e->timingp->dur = (SMIL_Duration *)att->data;
				e->timingp->dur->type = SMIL_DURATION_MEDIA;
			}
		}
	
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
	timingp->runtime = rti;
	rti->timed_elt = timed_elt;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Initialization\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));

	rti->timingp = timingp;
	rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
	rti->evaluate_status = SMIL_TIMING_EVAL_NONE;	
	rti->evaluate = gf_smil_timing_null_timed_function;
	rti->scene_time = -1;
	rti->force_reevaluation = 0;
	rti->media_duration = -1;

#ifndef NO_INTERVAL_LIST
	rti->intervals = gf_list_new();
	rti->current_interval = NULL;
	gf_smil_timing_init_interval_list(rti);
	interval_index = gf_smil_timing_find_interval_index(rti, GF_MAX_DOUBLE);
	if (interval_index >= 0) {
		rti->current_interval_index = interval_index;
		rti->current_interval = (SMIL_Interval*)gf_list_get(rti->intervals, rti->current_interval_index);
	} 
#else
	GF_SAFEALLOC(rti->current_interval, SMIL_Interval);
	gf_smil_timing_get_first_interval(rti);
#endif
	/* Now that the runtime info for this timed element are set, we can tell the scene graph that it can start
	   notifying the scene time to this element. Because of the 'animation' element, we can have many scene graphs
	   sharing the same scene time, we therefore add this timed element to the rootmost scene graph. */
	sg = timed_elt->sgprivate->scenegraph;
	while (sg->parent_scene) sg = sg->parent_scene;
	gf_smil_timing_add_to_sg(sg, rti);
}

void gf_smil_timing_delete_runtime_info(GF_Node *timed_elt, SMIL_Timing_RTI *rti)
{
	GF_SceneGraph *sg;
#ifndef NO_INTERVAL_LIST
	u32 i;
#endif

	if (!rti || !timed_elt) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Destruction\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
#ifndef NO_INTERVAL_LIST
	for (i = 0; i<gf_list_count(rti->intervals); i++) {
		SMIL_Interval *interval = (SMIL_Interval *)gf_list_get(rti->intervals, i);
		free(interval);
	}
	gf_list_del(rti->intervals);
#else
	free(rti->current_interval);
#endif

	/* we inform the rootmost scene graph that this node will not need notification of the scene time anymore */
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

#ifndef NO_INTERVAL_LIST

/* After a modification is made to the (timing? TODO check) attributes of a timed element, 
   we need to recreate the list of intervals. As for the initialization of intervals, 
   we assume that the list of time values is sorted */
static void gf_smil_timing_refresh_interval_list(SMIL_Timing_RTI *rti)
{
	u32 i, count, j, count2;
	SMIL_Time *begin;
	SMILTimingAttributesPointers *timingp = rti->timingp;

	if (!timingp) return;

	count = (timingp->begin ? gf_list_count(*timingp->begin) : 0);
	for (i = 0; i < count; i ++) {
		SMIL_Interval *existing_interval = NULL;

		begin = (SMIL_Time*)gf_list_get(*timingp->begin, i);
		
		if (! GF_SMIL_TIME_IS_CLOCK(begin->type) ) continue;

		count2 = gf_list_count(rti->intervals);
		for (j=0; j<count2; j++) {
			existing_interval = (SMIL_Interval *)gf_list_get(rti->intervals, j);
			/*if (
				(existing_interval->active_duration != -1 && existing_interval->begin + existing_interval->active_duration < rti->scene_time) ||
				(existing_interval->end != -1 && existing_interval->end < rti->scene_time)				
				) {
				/*
				gf_list_rem(rti->intervals, j);
				j--;
				count2--;
				free(existing_interval);
				
				break;
			} else*/ if (existing_interval->begin==begin->clock) break;
			existing_interval = NULL;
		}
		/* we create an acceptable only if begin is unspecified or defined (clock or wallclock) */
		gf_smil_timing_add_new_interval(rti, existing_interval, begin->clock, i);
	}
}
#endif

/* This function notifies the scene time to all the timed elements from the list in the given scene graph.
   It returns the number of active timed elements. If no timed element is active, this means that from the timing
   point of view, the scene has not changed and no rendering refresh is needed, even if the time has changed.
   TODO: merge the two steps in a do/while.
*/
Bool gf_smil_notify_timed_elements(GF_SceneGraph *sg)
{
	SMIL_Timing_RTI *rti;
	u32 active_count = 0, i = 0;
	s32 ret;
	if (!sg) return 0;

	sg->update_smil_timing = 0;
	while((rti = (SMIL_Timing_RTI *)gf_list_enum(sg->smil_timed_elements, &i))) {
		//scene_time = rti->timed_elt->sgprivate->scenegraph->GetSceneTime(rti->timed_elt->sgprivate->scenegraph->userpriv);
		ret = gf_smil_timing_notify_time(rti, gf_node_get_scene_time((GF_Node*)rti->timed_elt));
		if (ret == -1) {
			/* special case for discard element
			   when a discard element is executed, it automatically removes itself from the list of timed element 
			   in the scene graph, we need to fix the index i.
			*/
			i--;
		} else if (ret == -2) {
			/* special return value, -2 means that the tested timed element is waiting to begin
			   Assuming that the timed elements are sorted by begin order, 
			   the next ones don't need to be checked */
			break;
		} else {
			active_count += ret;
		}
	}

	/* In case a timed element triggers another one previously inactive, sg->update_smil_timing has been updated
	   TODO FIXME: it would be much better to stack anim as active/inactive*/
	while (sg->update_smil_timing) {
		sg->update_smil_timing = 0;
		i = 0;
		while((rti = (SMIL_Timing_RTI *)gf_list_enum(sg->smil_timed_elements, &i))) {
			/*this means the timed element has been modified, re-evaluate it*/
			if (rti->force_reevaluation) {
				ret = gf_smil_timing_notify_time(rti, gf_node_get_scene_time((GF_Node*)rti->timed_elt) );
				if (ret == -1) {
					/* special case for discard element */
					i--;
				} else if (ret == -2) {
					/* special return value, -2 means that the tested timed element is waiting to begin
					   Assuming that the timed elements are sorted by begin order, 
					   the next ones don't need to be checked */
					break;
				} else {
					active_count += ret;
				}
			}
		}
	}
	return (active_count>0);
}

/* evaluation function for the discard element */
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

/* Animations are applied in their begin order first and then in document order. 
   Whenever an animation (re)starts, it is placed at the end of the queue (potentially after frozen animations) */
static void gf_smil_reorder_anim(SMIL_Timing_RTI *rti)
{
	SMIL_Anim_RTI *rai = (SMIL_Anim_RTI *) gf_smil_anim_get_anim_runtime_from_timing(rti);
	if (rai) {
		gf_list_del_item(rai->owner->anims, rai);
		gf_list_add(rai->owner->anims, rai);
		gf_smil_anim_reset_variables(rai);
	}
}

/* Notifies the scene time to a timed element, potentially changing its status and triggering its evaluation
   Returns:
	0 if no rendering traversal is required, 
	1 if a rendering traversal is required!!!,
   -1 if the time node is a discard which has been deleted during this notification!! 
   -2 means that the timed element is waiting to begin */
s32 gf_smil_timing_notify_time(SMIL_Timing_RTI *rti, Double scene_time)
{
	Fixed simple_time;
	s32 ret = 0;
	GF_DOM_Event evt;
	SMILTimingAttributesPointers *timingp = rti->timingp;

	if (!timingp) return 0;
	
	if ((rti->scene_time == scene_time) && (rti->force_reevaluation == 0)) return 0;
	rti->scene_time = scene_time;
	rti->force_reevaluation = 0;
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

	gf_node_register(rti->timed_elt, NULL);

waiting_to_begin:
	if (rti->status == SMIL_STATUS_WAITING_TO_BEGIN) {
		if (
#ifndef NO_INTERVAL_LIST
			rti->current_interval
#else
			(rti->current_interval->begin != -1)
#endif
			&& scene_time >= rti->current_interval->begin) {			
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Activating\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
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
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Evaluating (Not starting)\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
			ret = -2;
			goto exit;
		}
	}

	if (rti->status == SMIL_STATUS_ACTIVE) {
		u32 cur_id;

		if (rti->current_interval->active_duration >= 0 
			&& scene_time >= (rti->current_interval->begin + rti->current_interval->active_duration)) {

			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Stopping \n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
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
		/*special case for unspecified simpleDur with animations (not with media timed elements)*/
		else if (0 && rti->postpone 
			&& (rti->current_interval->simple_duration==-1) 
			&& (rti->current_interval->active_duration==-1) 
		) {
			ret = 1;
			rti->status = SMIL_STATUS_FROZEN;
			rti->first_frozen = rti->cycle_number;
			rti->evaluate_status = SMIL_TIMING_EVAL_FREEZE;
		} else { // the animation is still active 
			if (!timingp->restart || *timingp->restart == SMIL_RESTART_ALWAYS) {
#ifdef NO_INTERVAL_LIST
				SMIL_Interval tmp_interval;
#else
				s32 interval_index;
#endif				
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Checking for restart when active\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
			
#ifndef NO_INTERVAL_LIST
				interval_index = gf_smil_timing_find_interval_index(rti, scene_time);
				
				if (interval_index >= 0 &&
					interval_index != rti->current_interval_index) {
					/* intervals are different, use the new one */
					rti->current_interval_index = interval_index;
					rti->current_interval = (SMIL_Interval*)gf_list_get(rti->intervals, rti->current_interval_index);
#else
				if (gf_smil_timing_get_next_interval(rti, &tmp_interval, scene_time)) {
					*rti->current_interval = tmp_interval;

#endif
					/* reinserting the new timed elements at its proper place in the list
					  of timed elements in the scenegraph */
					gf_smil_reorder_timing(rti);

					/* if this is animation, reinserting the animation in the list of animations 
				       that targets this attribute, so that it is the last one */
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

				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Repeating\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
				rti->evaluate_status = SMIL_TIMING_EVAL_REPEAT;		
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Updating\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
				rti->evaluate_status = SMIL_TIMING_EVAL_UPDATE;
			}

			if (!rti->postpone) {
				rti->evaluate(rti, simple_time, rti->evaluate_status);
			}	
		}
	}

	if ((rti->status == SMIL_STATUS_DONE) || (rti->status == SMIL_STATUS_FROZEN)) {
		if (!timingp->restart || *timingp->restart != SMIL_RESTART_NEVER) { 
			/* Check changes in begin or end attributes */
#ifdef NO_INTERVAL_LIST
			SMIL_Interval tmp_interval;
#else
			s32 interval_index;
#endif				

			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Checking for restart when not active\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
#ifndef NO_INTERVAL_LIST
			interval_index = gf_smil_timing_find_interval_index(rti, scene_time);
			if (interval_index >= 0 && interval_index != rti->current_interval_index) {
				/* intervals are different, use the new one */
				rti->current_interval_index = interval_index;
				rti->current_interval = (SMIL_Interval*)gf_list_get(rti->intervals, rti->current_interval_index);
#else
			if (gf_smil_timing_get_next_interval(rti, &tmp_interval, scene_time)) {
				*rti->current_interval = tmp_interval;
#endif

				/* reinserting the new timed elements at its proper place in the list
				  of timed elements in the scenegraph */
				gf_smil_reorder_timing(rti);

				rti->status = SMIL_STATUS_WAITING_TO_BEGIN;
				rti->evaluate_status = SMIL_TIMING_EVAL_NONE;
				goto waiting_to_begin;
			} 
		} else if ((rti->status == SMIL_STATUS_DONE) && 
			        timingp->restart && (*timingp->restart == SMIL_RESTART_NEVER)) {
			/* the timed element is done and cannot restart, we don't need to evaluate it anymore */
			GF_SceneGraph * sg = rti->timed_elt->sgprivate->scenegraph;
			while (sg->parent_scene) sg = sg->parent_scene;
			gf_list_del_item(sg->smil_timed_elements, rti);
		}
	}

exit:
	gf_node_unregister(rti->timed_elt, NULL);
	return ret;
}

/* returns a fraction between 0 and 1 of the elapsed time in the simple duration */
Fixed gf_smil_timing_get_normalized_simple_time(SMIL_Timing_RTI *rti, Double scene_time) 
{
	Double activeTime;
	Double simpleTime;
	Fixed normalizedSimpleTime;

#ifdef NO_INTERVAL_LIST
	if (rti->current_interval->begin == -1) return 0;
#else
	if (!rti->current_interval) return 0;
#endif

	activeTime = scene_time - rti->current_interval->begin;
	if (rti->current_interval->active_duration != -1 && activeTime > rti->current_interval->active_duration) return FIX_ONE;

	if (rti->current_interval->simple_duration>0) {
		rti->current_interval->nb_iterations = (u32)floor(activeTime / rti->current_interval->simple_duration);
	} else {
		rti->current_interval->nb_iterations = 0;
	}
	
	simpleTime = activeTime - rti->current_interval->simple_duration * rti->current_interval->nb_iterations;

	/* to be sure clamp simpleTime */
	simpleTime = MAX(0, simpleTime);
	simpleTime = MIN(rti->current_interval->simple_duration, simpleTime);
	normalizedSimpleTime = FLT2FIX(simpleTime / rti->current_interval->simple_duration);

	return normalizedSimpleTime;
}

/* This function is called when a modification to the node has been made (scripts, updates ...) */
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
	
	if (!timingp) return;
	rti = timingp->runtime;
	if (!rti) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing   ] Time %f - Timed element %s - Modification\n", gf_node_get_scene_time((GF_Node *)rti->timed_elt), gf_node_get_name((GF_Node *)rti->timed_elt)));
#ifdef NO_INTERVAL_LIST
	if (rti->current_interval->begin == -1) {
		SMIL_Interval tmp_interval;
		if (gf_smil_timing_get_next_interval(rti, &tmp_interval, gf_node_get_scene_time((GF_Node*)rti->timed_elt))) {
			*rti->current_interval = tmp_interval;
		}
	}
#else
	/* We need to recompute the list of intervals, preserving the current interval if any */
	gf_smil_timing_refresh_interval_list(rti);
	if (!rti->current_interval) {
		s32 interval_index;
		interval_index = gf_smil_timing_find_interval_index(rti, GF_MAX_DOUBLE);
		if (interval_index >= 0) {
			rti->current_interval_index = interval_index;
			rti->current_interval = (SMIL_Interval*)gf_list_get(rti->intervals, rti->current_interval_index);
		} 
	}
#endif

	sg = rti->timed_elt->sgprivate->scenegraph;
	while (sg->parent_scene) sg = sg->parent_scene;
	/* TODO When an element is modified it should probably be re-inserted at the end of 
	   the list using gf_smil_reorder_timing(rti); but this does not seem to work, which 
	   means that it has at this point already been removed from the list ??? */
	if (gf_smil_timing_add_to_sg(sg, rti)) {
		sg->update_smil_timing = 1;
		/* we need to force reevaluation of this timed element */
		rti->force_reevaluation = 1;
	}
}


/* Tries to resolve event-based or sync-based time values
   Used in parsing, to determine if a timed element can be initialized */
Bool gf_svg_resolve_smil_times(GF_SceneGraph *sg, void *event_base_element, 
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

/* Inserts a new resolved time instant in the begin or end attribute. 
   The insertion preserves the sorting and removes the previous insertions
    which have become obsolete
   WARNING: Only used for inserting time when an <a> element, whose target is a timed element, is activated. */
GF_EXPORT
void gf_smil_timing_insert_clock(GF_Node *elt, Bool is_end, Double clock)
{
	u32 i, count, found;
	SVGTimedAnimBaseElement *timed = (SVGTimedAnimBaseElement*)elt;
	SMIL_Time *begin;
	GF_List *l;
	GF_SAFEALLOC(begin, SMIL_Time);

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

