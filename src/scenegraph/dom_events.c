/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
 *
 *  This file is part of GPAC / DOM 3 Events sub-project
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

#ifndef GPAC_DISABLE_SVG

#include <gpac/events.h>
#include <gpac/nodes_svg.h>

static void gf_smil_handle_event(GF_Node *anim, GF_FieldInfo *info, GF_DOM_Event *evt, Bool is_end);

GF_Err gf_dom_listener_add(GF_Node *node, GF_Node *listener)
{
	if (!node || !listener) return GF_BAD_PARAM;
	if (listener->sgprivate->tag!=TAG_SVG_listener) return GF_BAD_PARAM;

	if (!node->sgprivate->interact) GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
	if (!node->sgprivate->interact->events) node->sgprivate->interact->events = gf_list_new();
	return gf_list_add(node->sgprivate->interact->events, listener);
}

GF_Err gf_dom_listener_del(GF_Node *node, GF_Node *listener)
{
	if (!node || !node->sgprivate->interact || !node->sgprivate->interact->events || !listener) return GF_BAD_PARAM;
	return (gf_list_del_item(node->sgprivate->interact->events, listener)<0) ? GF_BAD_PARAM : GF_OK;
}

GF_EXPORT
u32 gf_dom_listener_count(GF_Node *node)
{
	if (!node || !node->sgprivate->interact || !node->sgprivate->interact->events) return 0;
	return gf_list_count(node->sgprivate->interact->events);
}

SVGlistenerElement *gf_dom_listener_get(GF_Node *node, u32 i)
{
	if (!node || !node->sgprivate->interact) return 0;
	return (SVGlistenerElement *)gf_list_get(node->sgprivate->interact->events, i);
}

void gf_sg_handle_dom_event(GF_Node *hdl, GF_DOM_Event *event)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	if (hdl->sgprivate->scenegraph->svg_js) 
		if (hdl->sgprivate->scenegraph->svg_js->handler_execute(hdl, event)) return;
#endif
	/*no clue what this is*/
	GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[DOM Events] Unknown event handler\n"));
}

static void svg_process_event(SVGlistenerElement *listen, GF_DOM_Event *event)
{
	SVGhandlerElement *handler = (SVGhandlerElement *) listen->handler.target;
	if (!handler) return;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[DOM Events] Time %f - Processing event type: %s\n", gf_node_get_scene_time((GF_Node *)listen), gf_dom_event_get_name(event->type)));

	if (is_svg_animation_tag(handler->sgprivate->tag) && handler->anim && handler->timing->runtime) {
		if (event->type == GF_EVENT_BATTERY) {
			handler->timing->runtime->fraction = gf_divfix(INT2FIX(event->batteryLevel), INT2FIX(100));
		} else if (event->type == GF_EVENT_CPU) {
			handler->timing->runtime->fraction = gf_divfix(INT2FIX(event->cpu_percentage), INT2FIX(100));
		}
		switch (handler->timing->runtime->evaluate_status) {
		case SMIL_TIMING_EVAL_NONE:
			handler->timing->runtime->evaluate_status = SMIL_TIMING_EVAL_FRACTION;
		case SMIL_TIMING_EVAL_FRACTION:
			handler->timing->runtime->fraction = (handler->timing->runtime->fraction>FIX_ONE?FIX_ONE:handler->timing->runtime->fraction);
			handler->timing->runtime->fraction = (handler->timing->runtime->fraction<0?0:handler->timing->runtime->fraction);
			handler->timing->runtime->evaluate_status = SMIL_TIMING_EVAL_FRACTION;
			break;
		}
//		printf("event handled %f\n",FLT2FIX(handler->timing->runtime->fraction));
		return;
	}
#if 0
	/*laser-like handling*/
	if (handler->timing) {
		u32 i, count;
		GF_List *times;
		SMIL_Time *resolved;
		GF_FieldInfo info;

		if (listen->lsr_timeAttribute==LASeR_TIMEATTRIBUTE_END) times = handler->timing->end;
		else times = handler->timing->begin;

		/*solve*/
		GF_SAFEALLOC(resolved, SMIL_Time);
		resolved->type = SMIL_TIME_CLOCK;
		resolved->clock = gf_node_get_scene_time((GF_Node *)handler) + listen->lsr_delay;
		resolved->dynamic_type=2;
		count = gf_list_count(times);
		for (i=0; i<count; i++) {
			SMIL_Time *proto = gf_list_get(times, i);
			if ( (proto->type == SMIL_TIME_INDEFINITE) 
				|| ( (proto->type == SMIL_TIME_CLOCK) && (proto->clock > resolved->clock) ) 
				) 
				break;
		}
		gf_list_insert(times, resolved, i);
		memset(&info , 0, sizeof(GF_FieldInfo));
		info.fieldType = SMIL_Times_datatype;
		info.far_ptr = &times;
		gf_node_changed((GF_Node*)handler, &info);
		return;
	}
#endif
	if (handler->sgprivate->tag != TAG_SVG_handler) return;
	if (!handler->handle_event) return;
	if (handler->ev_event.type != event->type) return;
	handler->handle_event(handler, event);
}

static Bool sg_fire_dom_event(GF_Node *node, GF_DOM_Event *event)
{
	if (!node) return 0;

	if (node->sgprivate->interact && node->sgprivate->interact->events) {
		u32 i, count;
		count = gf_list_count(node->sgprivate->interact->events);
		for (i=0; i<count; i++) {
			SVGlistenerElement *listen = (SVGlistenerElement *)gf_list_get(node->sgprivate->interact->events, i);
			if (listen->event.type <= GF_EVENT_MOUSEMOVE) event->has_ui_events=1;
			if (listen->event.type != event->type) continue;
			event->currentTarget = node;
			/*process event*/
			svg_process_event(listen, event);
			
			/*load event cannot bubble and can only be called once (on load :) ), remove it
			to release some resources*/
			if (event->type==GF_EVENT_LOAD) {
				gf_list_rem(node->sgprivate->interact->events, i);
				count--;
				i--;
				if (listen->handler.target) gf_node_replace((GF_Node *) listen->handler.target, NULL, 0);
				gf_node_replace((GF_Node *) listen, NULL, 0);
			}
			/*canceled*/
			if (event->event_phase==4) return 0;
		}
		/*propagation stoped*/
		if (event->event_phase>=3) return 0;
	}
	return 1;
}

static void gf_sg_dom_event_bubble(GF_Node *node, GF_DOM_Event *event)
{
	if (sg_fire_dom_event(node, event)) 
		gf_sg_dom_event_bubble(gf_node_get_parent(node, 0), event);
}

void gf_sg_dom_stack_parents(GF_Node *node, GF_List *stack)
{
	if (!node) return;
	if (node->sgprivate->interact && node->sgprivate->interact->events) gf_list_insert(stack, node, 0);
	gf_sg_dom_stack_parents(gf_node_get_parent(node, 0), stack);
}

GF_EXPORT
Bool gf_dom_event_fire(GF_Node *node, GF_Node *parent_use, GF_DOM_Event *event)
{
	if (!node || !event) return 0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[DOM Events] Time %f - Firing event %s.%s\n", gf_node_get_scene_time(node), gf_node_get_name(node), gf_dom_event_get_name(event->type)));

	event->target = node;
	event->currentTarget = NULL;
	/*capture phase - not 100% sure, the actual capture phase should be determined by the std using the DOM events
	SVGT doesn't use this phase, so we don't add it for now.*/
	if (0) {
		u32 i, count;
		GF_List *parents;
		event->event_phase = 0;
		parents = gf_list_new();
		/*get all parents to top*/
		gf_sg_dom_stack_parents(gf_node_get_parent(node, 0), parents);
		count = gf_list_count(parents);
		for (i=0; i<count; i++) {
			GF_Node *n = (GF_Node *)gf_list_get(parents, i);
			sg_fire_dom_event(n, event);
			/*event has been canceled*/
			if (event->event_phase==4) break;
		}
		gf_list_del(parents);
		if (event->event_phase>=3) return 1;
	}
	/*target + bubbling phase*/
	event->event_phase = 0;
	if (sg_fire_dom_event(node, event) && event->bubbles) {
		event->event_phase = 1;
		if (!parent_use) parent_use = gf_node_get_parent(node, 0);
		/*TODO check in the specs*/
		else event->target = parent_use;
		gf_sg_dom_event_bubble(parent_use, event);
	}

	return event->currentTarget ? 1 : 0;
}

GF_EXPORT
void *gf_dom_listener_build(GF_Node *node, XMLEV_Event event)
{
	GF_ChildNodeItem *last = NULL;
	SVGlistenerElement *listener;
	SVGhandlerElement *handler;

	/*emulate a listener for onClick event*/
	listener = (SVGlistenerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_listener);
	handler = (SVGhandlerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_handler);
	gf_node_register((GF_Node *)listener, node);
	gf_node_list_add_child_last( & ((GF_ParentNode *)node)->children, (GF_Node*)listener, &last);
	gf_node_register((GF_Node *)handler, node);
	gf_node_list_add_child_last(& ((GF_ParentNode *)node)->children, (GF_Node*)handler, &last);
	handler->ev_event = listener->event = event;
	listener->handler.target = (SVGElement *) handler;
	listener->target.target = (SVGElement *)node;
	gf_dom_listener_add((GF_Node *) node, (GF_Node *) listener);
	/*set default handler*/
	handler->handle_event = gf_sg_handle_dom_event;
	return handler;
}

static void gf_smil_handle_event(GF_Node *timed_elt, GF_FieldInfo *info, GF_DOM_Event *evt, Bool is_end)
{
	SMIL_Time *resolved, *proto;
	Double scene_time = gf_node_get_scene_time(evt->target);
	GF_List *times = *(GF_List **)info->far_ptr;
	u32 found = 0;
	u32 i, j, count = gf_list_count(times);

	/*remove all previously instantiated times that are in the past
	TODO FIXME: this is not 100% correct, a begin val in the past can be interpreted!!*/
	for (i=0; i<count; i++) {
		proto = (SMIL_Time*)gf_list_get(times, i);
		if ((proto->type == GF_SMIL_TIME_EVENT_RESLOVED) && (proto->clock<scene_time) ) {
			free(proto);
			gf_list_rem(times, i);
			i--;
			count--;
		}
	}

	for (i=0; i<count; i++) {
		proto = (SMIL_Time*)gf_list_get(times, i);
		if (proto->type != GF_SMIL_TIME_EVENT) continue;
		if (proto->event.type != evt->type) continue;
		if ((evt->type == GF_EVENT_KEYDOWN) || (evt->type == GF_EVENT_REPEAT)) {
			if (proto->event.parameter!=evt->detail) continue;
		}
		/*solve*/
		GF_SAFEALLOC(resolved, SMIL_Time);
		resolved->type = GF_SMIL_TIME_EVENT_RESLOVED;
		resolved->clock = scene_time + proto->clock;

		/*insert in sorted order*/
		for (j=0; j<count; j++) {
			proto = (SMIL_Time*)gf_list_get(times, j);

			if ( GF_SMIL_TIME_IS_SPECIFIED_CLOCK(proto->type) ) {
				if (proto->clock > resolved->clock) break;
			} else {
				break;
			}
		}
		gf_list_insert(times, resolved, j);
		if (j!=count) i++;
		count++;
		found++;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[SMIL Timing] Inserting new time in %s.%s: %f\n", gf_node_get_name(timed_elt), (is_end?"end":"begin"), resolved->clock));
	}
	if (found) gf_node_changed(timed_elt, info);
}

static void gf_smil_handle_event_begin(GF_Node *hdl, GF_DOM_Event *evt)
{
	GF_FieldInfo info;
	SVGElement *timed_elt = (SVGElement *)gf_node_get_private(hdl);
	memset(&info, 0, sizeof(GF_FieldInfo));
	info.name = "begin";
	info.far_ptr = &timed_elt->timing->begin;
	info.fieldType = SMIL_Times_datatype;
	gf_smil_handle_event((GF_Node*)timed_elt, &info, evt, 0);
}

static void gf_smil_handle_event_end(GF_Node *hdl, GF_DOM_Event *evt)
{
	GF_FieldInfo info;
	SVGElement *timed_elt = (SVGElement *)gf_node_get_private(hdl);
	memset(&info, 0, sizeof(GF_FieldInfo));
	info.name = "end";
	info.far_ptr = &timed_elt->timing->end;
	info.fieldType = SMIL_Times_datatype;
	gf_smil_handle_event((GF_Node *)timed_elt, &info, evt, 1);
}

static void gf_smil_setup_event_list(GF_Node *node, GF_List *l, Bool is_begin)
{
	SVGhandlerElement *hdl;
	u32 i, count;
	count = gf_list_count(l);
	for (i=0; i<count; i++) {
		SMIL_Time *t = (SMIL_Time*)gf_list_get(l, i);
		if (t->type != GF_SMIL_TIME_EVENT) continue;
		/*not resolved yet*/
		if (!t->element && t->element_id) continue;
		hdl = gf_dom_listener_build(t->element, t->event);
		hdl->handle_event = is_begin ? gf_smil_handle_event_begin : gf_smil_handle_event_end;
		gf_node_set_private((GF_Node *)hdl, node);
		t->element = NULL;
	}
}

void gf_smil_setup_events(GF_Node *node)
{
	GF_FieldInfo info;
	if (gf_node_get_field_by_name(node, "begin", &info) != GF_OK) return;
	gf_smil_setup_event_list(node, * (GF_List **)info.far_ptr, 1);
	if (gf_node_get_field_by_name(node, "end", &info) != GF_OK) return;
	gf_smil_setup_event_list(node, * (GF_List **)info.far_ptr, 0);
}

#endif	//GPAC_DISABLE_SVG
