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
#include <gpac/xml.h>

#ifndef GPAC_DISABLE_SVG

#include <gpac/scenegraph_svg.h>
#include <gpac/events.h>
#include <gpac/nodes_svg_sa.h>
#include <gpac/nodes_svg_sani.h>
#include <gpac/nodes_svg_da.h>

static void gf_smil_handle_event(GF_Node *anim, GF_FieldInfo *info, GF_DOM_Event *evt, Bool is_end);

GF_Err gf_dom_listener_add(GF_Node *node, GF_Node *listener)
{
	if (!node || !listener) return GF_BAD_PARAM;
	if ((listener->sgprivate->tag!=TAG_SVG_listener)
#ifdef GPAC_ENABLE_SVG_SANI
		&& (listener->sgprivate->tag!=TAG_SVG_SA_listener)
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		&& (listener->sgprivate->tag!=TAG_SVG_SANI_listener)
#endif
		) return GF_BAD_PARAM;

	if (!node->sgprivate->interact) GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);
	if (!node->sgprivate->interact->events) node->sgprivate->interact->events = gf_list_new();

	/*only one observer per listener*/
	if (listener->sgprivate->UserPrivate!=NULL) return GF_NOT_SUPPORTED;
	listener->sgprivate->UserPrivate = node;
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

GF_Node *gf_dom_listener_get(GF_Node *node, u32 i)
{
	if (!node || !node->sgprivate->interact) return 0;
	return (GF_Node *)gf_list_get(node->sgprivate->interact->events, i);
}

typedef struct
{
	GF_Node *obs;
	GF_Node *listener;
} DOMAddListener;

void gf_dom_listener_post_add(GF_Node *obs, GF_Node *listener)
{
	DOMAddListener *l;
	l = (DOMAddListener*)malloc(sizeof(DOMAddListener));
	l->listener = listener;
	l->obs = obs;
	gf_list_add(obs->sgprivate->scenegraph->listeners_to_add, l);
}

void gf_dom_listener_process_add(GF_SceneGraph *sg)
{
	u32 i, count;
	count = gf_list_count(sg->listeners_to_add);
	for (i=0; i<count; i++) {
		DOMAddListener *l = (DOMAddListener *)gf_list_get(sg->listeners_to_add, i);
		gf_dom_listener_add(l->obs, l->listener);
		free(l);
	}
	gf_list_reset(sg->listeners_to_add);
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

static void svg_process_event(GF_Node *listen, GF_DOM_Event *event)
{
	GF_Node *hdl_node;

	switch (listen->sgprivate->tag) {
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_listener:
		hdl_node = ((SVG_SA_listenerElement *)listen)->handler.target;
		break;
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_listener:
		hdl_node = ((SVG_SANI_listenerElement *)listen)->handler.target;
		break;
#endif
	case TAG_SVG_listener:
	{
		GF_FieldInfo info;
		if (gf_svg_get_attribute_by_tag(listen, TAG_SVG_ATT_handler, 0, 0, &info) == GF_OK) {
			XMLRI *iri = (XMLRI *)info.far_ptr;
			if (!iri->target && iri->string) {
				iri->target = gf_sg_find_node_by_name(listen->sgprivate->scenegraph, iri->string+1);
			}
			hdl_node = iri->target;
		} else {
			return;
		}
	}
		break;
	default:
		return;
	}
	if (!hdl_node) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[DOM Events] Time %f - Processing event type: %s\n", gf_node_get_scene_time((GF_Node *)listen), gf_dom_event_get_name(event->type)));

	switch (hdl_node->sgprivate->tag) {
#ifdef GPAC_ENABLE_SVG_SA_BASE
#ifdef GPAC_ENABLE_SVG_SA
	case TAG_SVG_SA_handler:
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	case TAG_SVG_SANI_handler:
#endif
	{
		SVG_SA_handlerElement *handler = (SVG_SA_handlerElement *) hdl_node;
		if (gf_svg_sa_is_animation_tag(handler->sgprivate->tag) && handler->anim && handler->timing->runtime) {
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
		if (!handler->handle_event) return;
		if (handler->ev_event.type != event->type) return;
		handler->handle_event((GF_Node*)handler, event);

	} 
		break;
#endif	/*GPAC_ENABLE_SVG_SA_BASE*/
	case TAG_SVG_handler:
	{
		GF_FieldInfo info;
		SVG_handlerElement *handler = (SVG_handlerElement *) hdl_node;
		if (!handler->handle_event) return;

		if (gf_svg_get_attribute_by_tag(hdl_node, TAG_SVG_ATT_ev_event, 0, 0, &info) == GF_OK) {
			XMLEV_Event *ev_event = (XMLEV_Event *)info.far_ptr;
			if (ev_event->type != event->type) return;
			handler->handle_event(hdl_node, event);
		} else {
			handler->handle_event(hdl_node, event);
		}
	}
		break;
	case TAG_SVG_conditional:
		if ( ((SVG_Element*)hdl_node)->children)
			gf_node_render(((SVG_Element*)hdl_node)->children->node, NULL);
		break;
	default:
		return;
	}
}

static Bool sg_fire_dom_event(GF_Node *node, GF_DOM_Event *event)
{
	if (!node) return 0;

	if (node->sgprivate->interact && node->sgprivate->interact->events) {
		u32 i, count, post_count;
		count = gf_list_count(node->sgprivate->interact->events);
		for (i=0; i<count; i++) {
			GF_Node *handler = NULL;
			XMLEV_Event *listened_event;
			GF_Node *listen = (GF_Node *)gf_list_get(node->sgprivate->interact->events, i);

			switch (listen->sgprivate->tag) {
#ifdef GPAC_ENABLE_SVG_SA_BASE
#ifdef GPAC_ENABLE_SVG_SA
			case TAG_SVG_SA_listener:
#endif
#ifdef GPAC_ENABLE_SVG_SANI
			case TAG_SVG_SANI_listener:
#endif
				listened_event = &((SVG_SA_listenerElement *)listen)->event;
				handler = ((SVG_SA_listenerElement *)listen)->handler.target;
				break;
#endif /*GPAC_ENABLE_SVG_SA_BASE*/
			case TAG_SVG_listener:
			{
				SVGAllAttributes atts;
				gf_svg_flatten_attributes((SVG_Element*)listen, &atts);
				listened_event = atts.event;
				if (atts.handler) handler = atts.handler->target;
			}
				break;
			default:
				continue;
			}
			if (listened_event->type <= GF_EVENT_MOUSEMOVE) event->has_ui_events=1;
			if (listened_event->type != event->type) continue;
			if (listened_event->parameter && (listened_event->parameter != event->detail)) continue;
			event->currentTarget = node;
			
			/*load event cannot bubble and can only be called once (on load :) ), remove it
			to release some resources*/
			if (event->type==GF_EVENT_LOAD) {
				svg_process_event(listen, event);

				gf_list_rem(node->sgprivate->interact->events, i);
				count--;
				i--;
				if (handler) gf_node_replace(handler, NULL, 0);
				gf_node_replace((GF_Node *) listen, NULL, 0);
			} else {
				assert(node->sgprivate->num_instances);
				/*protect node*/
				node->sgprivate->num_instances++;
				/*exec event*/
				svg_process_event(listen, event);
				/*the event has destroyed ourselves, abort propagation
				THIS IS NOT DOM compliant, the event should propagate on the original target+ancestors path*/
				if (node->sgprivate->num_instances==1) {
					/*unprotect node event*/
					gf_node_unregister(node, NULL);
					return 0;
				}
				node->sgprivate->num_instances--;
			}
			/*canceled*/
			if (event->event_phase==4) {
				gf_dom_listener_process_add(node->sgprivate->scenegraph);
				return 0;
			}

			/*if listeners have been removed, update count*/
			post_count = gf_list_count(node->sgprivate->interact->events);
			if (post_count < count) {
				s32 pos = gf_list_find(node->sgprivate->interact->events, listen);
				if (pos>=0) i = pos;
				/*FIXME this is not going to work in all cases...*/
				else i--;
				count = post_count;
			}
		}
		/*propagation stoped*/
		if (event->event_phase>=3) {
			gf_dom_listener_process_add(node->sgprivate->scenegraph);
			return 0;
		}
	}
	gf_dom_listener_process_add(node->sgprivate->scenegraph);
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

	/*flush any pending add_listener*/
	gf_dom_listener_process_add(node->sgprivate->scenegraph);

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
GF_DOMHandler *gf_dom_listener_build(GF_Node *node, u32 event_type, u32 event_parameter)
{
	u32 tag;
	GF_ChildNodeItem *last = NULL;

	tag = gf_node_get_tag(node);
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		SVG_Element *listener;
		SVG_handlerElement *handler;
		GF_FieldInfo info;

		listener = (SVG_Element *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_listener);
		handler = (SVG_handlerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_handler);
		gf_node_register((GF_Node *)listener, node);
		gf_node_list_add_child_last( & ((GF_ParentNode *)node)->children, (GF_Node*)listener, &last);
		gf_node_register((GF_Node *)handler, node);
		gf_node_list_add_child_last(& ((GF_ParentNode *)node)->children, (GF_Node*)handler, &last);

		gf_svg_get_attribute_by_tag((GF_Node*)handler, TAG_SVG_ATT_ev_event, 1, 0, &info);
		((XMLEV_Event *)info.far_ptr)->type = event_type;
		((XMLEV_Event *)info.far_ptr)->parameter = event_parameter;
		
		gf_svg_get_attribute_by_tag((GF_Node*)listener, TAG_SVG_ATT_event, 1, 0, &info);
		((XMLEV_Event *)info.far_ptr)->type = event_type;
		((XMLEV_Event *)info.far_ptr)->parameter = event_parameter;

		gf_svg_get_attribute_by_tag((GF_Node*)listener, TAG_SVG_ATT_handler, 1, 0, &info);
		((XMLRI *)info.far_ptr)->target = handler;

		gf_svg_get_attribute_by_tag((GF_Node*)listener, TAG_SVG_ATT_listener_target, 1, 0, &info);
		((XMLRI *)info.far_ptr)->target = node;
		
		gf_dom_listener_add((GF_Node *) node, (GF_Node *) listener);

		/*set default handler*/
		handler->handle_event = gf_sg_handle_dom_event;
		return handler;
	} 
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
		SVG_SA_listenerElement *listener;
		SVG_SA_handlerElement *handler;
		/*emulate a listener for onClick event*/
		listener = (SVG_SA_listenerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_SA_listener);
		handler = (SVG_SA_handlerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_SA_handler);
		gf_node_register((GF_Node *)listener, node);
		gf_node_list_add_child_last( & ((GF_ParentNode *)node)->children, (GF_Node*)listener, &last);
		gf_node_register((GF_Node *)handler, node);
		gf_node_list_add_child_last(& ((GF_ParentNode *)node)->children, (GF_Node*)handler, &last);
		listener->event.type = event_type;
		listener->event.parameter = event_parameter;
		handler->ev_event = listener->event;
		listener->handler.target = (SVG_SA_Element *) handler;
		listener->target.target = (SVG_SA_Element *)node;
		gf_dom_listener_add((GF_Node *) node, (GF_Node *) listener);
		/*set default handler*/
		handler->handle_event = gf_sg_handle_dom_event;
		return handler;
	} 
#endif /*GPAC_ENABLE_SVG_SA*/
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		SVG_SA_listenerElement *listener;
		SVG_SA_handlerElement *handler;
		/*emulate a listener for onClick event*/
		listener = (SVG_SA_listenerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_SANI_listener);
		handler = (SVG_SA_handlerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_SANI_handler);
		gf_node_register((GF_Node *)listener, node);
		gf_node_list_add_child_last( & ((GF_ParentNode *)node)->children, (GF_Node*)listener, &last);
		gf_node_register((GF_Node *)handler, node);
		gf_node_list_add_child_last(& ((GF_ParentNode *)node)->children, (GF_Node*)handler, &last);
		listener->event.type = event_type;
		listener->event.parameter = event_parameter;
		handler->ev_event = listener->event;
		listener->handler.target = (SVG_SA_Element *) handler;
		listener->target.target = (SVG_SA_Element *)node;
		gf_dom_listener_add((GF_Node *) node, (GF_Node *) listener);
		/*set default handler*/
		handler->handle_event = gf_sg_handle_dom_event;
		return handler;
	} 
#endif /*GPAC_ENABLE_SVG_SANI*/
	else {
		return NULL;
	}
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
		if ((proto->type == GF_SMIL_TIME_EVENT_RESOLVED) && (proto->clock<scene_time) ) {
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
		if ((evt->type == GF_EVENT_KEYDOWN) || (evt->type == GF_EVENT_REPEAT_EVENT)) {
			if (proto->event.parameter!=evt->detail) continue;
		}
		/*solve*/
		GF_SAFEALLOC(resolved, SMIL_Time);
		resolved->type = GF_SMIL_TIME_EVENT_RESOLVED;

		if (proto->is_absolute_event) {
			resolved->clock = evt->smil_event_time + proto->clock;
		} else {
			resolved->clock = scene_time + proto->clock;
		}

		/*insert in sorted order*/
		for (j=0; j<count; j++) {
			proto = (SMIL_Time*)gf_list_get(times, j);

			if ( GF_SMIL_TIME_IS_CLOCK(proto->type) ) {
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
	GF_Node *timed_elt = (GF_Node *)gf_node_get_private(hdl);
	u32 tag = timed_elt->sgprivate->tag;
	memset(&info, 0, sizeof(GF_FieldInfo));
	info.name = "begin";
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		info.far_ptr = ((SVGTimedAnimBaseElement *)timed_elt)->timingp->begin;
	}
#ifdef GPAC_ENABLE_SVG_SA
	else if ( (tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA) ) {
		info.far_ptr = &((SVG_SA_Element *)timed_elt)->timing->begin;
	}
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ( (tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI) ) {
		info.far_ptr = &((SVG_SA_Element *)timed_elt)->timing->begin;
	}
#endif
	else {
		return; 
	}
	info.fieldType = SMIL_Times_datatype;
	gf_smil_handle_event(timed_elt, &info, evt, 0);
}

static void gf_smil_handle_event_end(GF_Node *hdl, GF_DOM_Event *evt)
{
	GF_FieldInfo info;
	GF_Node *timed_elt = (GF_Node *)gf_node_get_private(hdl);
	u32 tag = timed_elt->sgprivate->tag;
	memset(&info, 0, sizeof(GF_FieldInfo));
	info.name = "end";
	if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
		info.far_ptr = ((SVGTimedAnimBaseElement *)timed_elt)->timingp->end;
	} 
#ifdef GPAC_ENABLE_SVG_SA
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA) ) {
		info.far_ptr = &((SVG_SA_Element *)timed_elt)->timing->end;
	} 
#endif
#ifdef GPAC_ENABLE_SVG_SANI
	else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
		info.far_ptr = &((SVG_SA_Element *)timed_elt)->timing->end;
	} 
#endif
	else {
		return; 
	}
	info.fieldType = SMIL_Times_datatype;
	gf_smil_handle_event((GF_Node *)timed_elt, &info, evt, 1);
}

static void gf_smil_setup_event_list(GF_Node *node, GF_List *l, Bool is_begin)
{
	void *hdl;
	u32 i, count;
	u32 tag = node->sgprivate->tag;
	count = gf_list_count(l);
	for (i=0; i<count; i++) {
		SMIL_Time *t = (SMIL_Time*)gf_list_get(l, i);
		if (t->type != GF_SMIL_TIME_EVENT) continue;
		/*not resolved yet*/
		if (!t->element && t->element_id) continue;
		if (t->event.type==GF_EVENT_BEGIN) {
			t->event.type=GF_EVENT_BEGIN_EVENT;
			t->is_absolute_event = 1;
		} else if (t->event.type==GF_EVENT_END) {
			t->event.type=GF_EVENT_END_EVENT;
			t->is_absolute_event = 1;
		} else if (t->event.type==GF_EVENT_REPEAT) {
			t->event.type=GF_EVENT_REPEAT_EVENT;
			t->is_absolute_event = 1;
		} 
		hdl = gf_dom_listener_build(t->element, t->event.type, t->event.parameter);
		
		if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
			((SVG_handlerElement *)hdl)->handle_event = is_begin ? gf_smil_handle_event_begin : gf_smil_handle_event_end;
		}
#ifdef GPAC_ENABLE_SVG_SA
		else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SA) && (tag<=GF_NODE_RANGE_LAST_SVG_SA)) {
			((SVG_SA_handlerElement *)hdl)->handle_event = is_begin ? gf_smil_handle_event_begin : gf_smil_handle_event_end;
		}
#endif
#ifdef GPAC_ENABLE_SVG_SANI
		else if ((tag>=GF_NODE_RANGE_FIRST_SVG_SANI) && (tag<=GF_NODE_RANGE_LAST_SVG_SANI)) {
			((SVG_SANI_handlerElement *)hdl)->handle_event = is_begin ? gf_smil_handle_event_begin : gf_smil_handle_event_end;
		}
#endif
		else {
			continue;
		}
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


GF_DOMText *gf_dom_add_text_node(GF_Node *parent, char *text_data)
{
	GF_DOMText *text;
	GF_SAFEALLOC(text, GF_DOMText);

	gf_node_setup((GF_Node *)text, TAG_DOMText);
	text->sgprivate->scenegraph = parent->sgprivate->scenegraph;
	text->textContent = text_data;
	gf_node_register((GF_Node *)text, parent);
	gf_node_list_add_child_last(&((GF_ParentNode *)parent)->children, (GF_Node*)text, NULL);
	return text;
}
GF_DOMText *gf_dom_new_text_node(GF_SceneGraph *sg)
{
	GF_DOMText *text;
	GF_SAFEALLOC(text, GF_DOMText);
	gf_node_setup((GF_Node *)text, TAG_DOMText);
	text->sgprivate->scenegraph = sg;
	return text;
}

GF_DOMUpdates *gf_dom_add_updates_node(GF_Node *parent)
{
	GF_DOMUpdates *text;
	GF_SAFEALLOC(text, GF_DOMUpdates);

	gf_node_setup((GF_Node *)text, TAG_DOMUpdates);
	text->sgprivate->scenegraph = parent->sgprivate->scenegraph;
	text->updates = gf_list_new();
	gf_node_register((GF_Node *)text, parent);
	gf_node_list_add_child_last(&((GF_ParentNode *)parent)->children, (GF_Node*)text, NULL);
	return text;
}


GF_DOMUpdates *gf_dom_add_update_node(GF_Node *parent)
{
	GF_DOMUpdates *update;
	GF_SAFEALLOC(update, GF_DOMUpdates);

	gf_node_setup((GF_Node *)update, TAG_DOMUpdates);
	update->sgprivate->scenegraph = parent->sgprivate->scenegraph;
	update->updates = gf_list_new();
	gf_node_register((GF_Node *)update, parent);
	gf_node_list_add_child_last(&((GF_ParentNode *)parent)->children, (GF_Node*)update, NULL);
	return update;
}


#endif	//GPAC_DISABLE_SVG

