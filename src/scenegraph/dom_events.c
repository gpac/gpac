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
#include <gpac/nodes_svg.h>

static void gf_smil_handle_event(GF_Node *anim, GF_FieldInfo *info, GF_DOM_Event *evt, Bool is_end);


static void gf_dom_refresh_event_filter(GF_SceneGraph *sg)
{
	GF_SceneGraph *par;
	u32 prev_flags = sg->dom_evt_filter;

	sg->dom_evt_filter = 0;
	if (sg->nb_evts_mouse) sg->dom_evt_filter |= GF_DOM_EVENT_MOUSE;
	if (sg->nb_evts_focus) sg->dom_evt_filter |= GF_DOM_EVENT_FOCUS;
	if (sg->nb_evts_key) sg->dom_evt_filter |= GF_DOM_EVENT_KEY;
	if (sg->nb_evts_ui) sg->dom_evt_filter |= GF_DOM_EVENT_UI;
	if (sg->nb_evts_mutation) sg->dom_evt_filter |= GF_DOM_EVENT_MUTATION;
	if (sg->nb_evts_text) sg->dom_evt_filter |= GF_DOM_EVENT_TEXT;
	if (sg->nb_evts_smil) sg->dom_evt_filter |= GF_DOM_EVENT_SMIL;
	if (sg->nb_evts_laser) sg->dom_evt_filter |= GF_DOM_EVENT_LASER;
	if (sg->nb_evts_svg) sg->dom_evt_filter |= GF_DOM_EVENT_SVG;
	if (sg->nb_evts_mae) sg->dom_evt_filter |= GF_DOM_EVENT_MEDIA_ACCESS;

	/*for each graph until top, update event filter*/
	par = sg->parent_scene;
	while (par) {
		par->dom_evt_filter &= ~prev_flags;
		par->dom_evt_filter |= sg->dom_evt_filter;
		par = par->parent_scene;
	}
}

void gf_sg_unregister_event_type(GF_SceneGraph *sg, u32 type)
{
	if (sg->nb_evts_mouse && (type & GF_DOM_EVENT_MOUSE)) sg->nb_evts_mouse--;
	if (sg->nb_evts_focus && (type & GF_DOM_EVENT_FOCUS)) sg->nb_evts_focus--;
	if (sg->nb_evts_key && (type & GF_DOM_EVENT_KEY)) sg->nb_evts_key--;
	if (sg->nb_evts_ui && (type & GF_DOM_EVENT_UI)) sg->nb_evts_ui--;
	if (sg->nb_evts_mutation && (type & GF_DOM_EVENT_MUTATION)) sg->nb_evts_mutation--;
	if (sg->nb_evts_text && (type & GF_DOM_EVENT_TEXT)) sg->nb_evts_text--;
	if (sg->nb_evts_smil && (type & GF_DOM_EVENT_SMIL)) sg->nb_evts_smil--;
	if (sg->nb_evts_laser && (type & GF_DOM_EVENT_LASER)) sg->nb_evts_laser--;
	if (sg->nb_evts_text && (type & GF_DOM_EVENT_TEXT)) sg->nb_evts_text--;
	if (sg->nb_evts_svg && (type & GF_DOM_EVENT_SVG)) sg->nb_evts_svg--;
	if (sg->nb_evts_mae && (type & GF_DOM_EVENT_MEDIA_ACCESS)) sg->nb_evts_mae--;

	gf_dom_refresh_event_filter(sg);
}


void gf_sg_register_event_type(GF_SceneGraph *sg, u32 type)
{
	if (type & GF_DOM_EVENT_MOUSE) sg->nb_evts_mouse++;
	if (type & GF_DOM_EVENT_FOCUS) sg->nb_evts_focus++;
	if (type & GF_DOM_EVENT_KEY) sg->nb_evts_key++;
	if (type & GF_DOM_EVENT_UI) sg->nb_evts_ui++;
	if (type & GF_DOM_EVENT_MUTATION) sg->nb_evts_mutation++;
	if (type & GF_DOM_EVENT_TEXT) sg->nb_evts_text++;
	if (type & GF_DOM_EVENT_SMIL) sg->nb_evts_smil++;
	if (type & GF_DOM_EVENT_LASER) sg->nb_evts_laser++;
	if (type & GF_DOM_EVENT_SVG) sg->nb_evts_svg++;
	if (type & GF_DOM_EVENT_MEDIA_ACCESS) sg->nb_evts_mae++;

	gf_dom_refresh_event_filter(sg);
}

u32 gf_sg_get_dom_event_filter(GF_SceneGraph *sg)
{
	return sg->dom_evt_filter;
}
u32 gf_node_get_dom_event_filter(GF_Node *node)
{
	return node->sgprivate->scenegraph->dom_evt_filter;
}

GF_Err gf_dom_listener_add(GF_Node *listener, GF_DOMEventTarget *evt_target)
{
	GF_FieldInfo info;
	if (!evt_target || !listener) return GF_BAD_PARAM;
	if (listener->sgprivate->tag!=TAG_SVG_listener)
		return GF_BAD_PARAM;


	/*only one observer per listener*/
	if (listener->sgprivate->UserPrivate!=NULL) return GF_NOT_SUPPORTED;
	listener->sgprivate->UserPrivate = evt_target;

	/*register with NULL parent*/
	gf_node_register((GF_Node *)listener, NULL);

	if (gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_event, 0, 0, &info) == GF_OK) {
		u32 type = ((XMLEV_Event *)info.far_ptr)->type;
		gf_sg_register_event_type(listener->sgprivate->scenegraph, gf_dom_event_get_category(type));
	}

	return gf_list_add(evt_target->evt_list, listener);
}

GF_Err gf_node_dom_listener_add(GF_Node *node, GF_Node *listener)
{
	if (!node || !listener) return GF_BAD_PARAM;
	if (listener->sgprivate->tag!=TAG_SVG_listener)
		return GF_BAD_PARAM;
	
	if (!node->sgprivate->interact) 
		GF_SAFEALLOC(node->sgprivate->interact, struct _node_interactive_ext);

	if (!node->sgprivate->interact->dom_evt) {
		GF_SAFEALLOC(node->sgprivate->interact->dom_evt, GF_DOMEventTarget);
		node->sgprivate->interact->dom_evt->ptr = node;
		node->sgprivate->interact->dom_evt->ptr_type = GF_DOM_EVENT_NODE;
		node->sgprivate->interact->dom_evt->evt_list = gf_list_new();
	}
	return gf_dom_listener_add(listener, node->sgprivate->interact->dom_evt);
}

GF_Err gf_dom_listener_del(GF_Node *listener, GF_DOMEventTarget *target)
{
	GF_FieldInfo info;
	if (!listener || !target) return GF_BAD_PARAM;

	if (gf_list_del_item(target->evt_list, listener)<0) return GF_BAD_PARAM;

	if (gf_node_get_attribute_by_tag((GF_Node *)listener, TAG_XMLEV_ATT_event, 0, 0, &info) == GF_OK) {
		u32 type = ((XMLEV_Event *)info.far_ptr)->type;
		gf_sg_unregister_event_type(listener->sgprivate->scenegraph, gf_dom_event_get_category(type));
	}
	listener->sgprivate->UserPrivate = NULL;
	gf_node_unregister((GF_Node *)listener, NULL);
	return GF_OK;
}

GF_EXPORT
u32 gf_dom_listener_count(GF_Node *node)
{
	if (!node || !node->sgprivate->interact || !node->sgprivate->interact->dom_evt) return 0;
	return gf_list_count(node->sgprivate->interact->dom_evt->evt_list);
}

GF_EXPORT
GF_Node *gf_dom_listener_get(GF_Node *node, u32 i)
{
	if (!node || !node->sgprivate->interact || !node->sgprivate->interact->dom_evt) return 0;
	return (GF_Node *)gf_list_get(node->sgprivate->interact->dom_evt->evt_list, i);
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
		gf_node_dom_listener_add(l->obs, l->listener);
		free(l);
	}
	gf_list_reset(sg->listeners_to_add);
}

void gf_sg_handle_dom_event(GF_Node *hdl, GF_DOM_Event *event, GF_Node *observer)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	if (hdl->sgprivate->scenegraph->svg_js) 
		if (hdl->sgprivate->scenegraph->svg_js->handler_execute(hdl, event, observer)) return;
#endif
	/*no clue what this is*/
	GF_LOG(GF_LOG_WARNING, GF_LOG_INTERACT, ("[DOM Events    ] Unknown event handler\n"));
}

static GF_Node *dom_evt_get_handler(GF_Node *n)
{
	XMLRI *iri;
	GF_FieldInfo info;

	if (!n || (n->sgprivate->tag!=TAG_SVG_handler)) return n;

	if (!n || (gf_node_get_attribute_by_tag(n, TAG_XLINK_ATT_href, 0, 0, &info) != GF_OK)) {
		return n;
	}
	iri = (XMLRI *)info.far_ptr;
	if (!iri->target && iri->string) {
		iri->target = gf_sg_find_node_by_name(n->sgprivate->scenegraph, iri->string+1);
	}
	return dom_evt_get_handler(iri->target);
}

static void dom_event_process(GF_Node *listen, GF_DOM_Event *event, GF_Node *observer)
{
	GF_Node *hdl_node;

	switch (listen->sgprivate->tag) {
	case TAG_SVG_listener:
	{
		GF_FieldInfo info;
		if (gf_node_get_attribute_by_tag(listen, TAG_XMLEV_ATT_handler, 0, 0, &info) == GF_OK) {
			XMLRI *iri = (XMLRI *)info.far_ptr;
			if (!iri->target && iri->string) {
				iri->target = gf_sg_find_node_by_name(listen->sgprivate->scenegraph, iri->string+1);
			}
			hdl_node = dom_evt_get_handler(iri->target);
		} else {
			return;
		}
	}
		break;
	default:
		return;
	}
	if (!hdl_node) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[DOM Events    ] Time %f - Processing event type: %s\n", gf_node_get_scene_time((GF_Node *)listen), gf_dom_event_get_name(event->type)));

	switch (hdl_node->sgprivate->tag) {
	case TAG_SVG_handler:
	{
		//GF_FieldInfo info;
		SVG_handlerElement *handler = (SVG_handlerElement *) hdl_node;
		if (!handler->handle_event) return;

		/*spec is not clear regarding event filtering*/
#if 0
		if (gf_node_get_attribute_by_tag(hdl_node, TAG_XMLEV_ATT_event, 0, 0, &info) == GF_OK) {
			XMLEV_Event *ev_event = (XMLEV_Event *)info.far_ptr;
			if (ev_event->type != event->type) return;
			handler->handle_event(hdl_node, event, observer);
		} else {
			handler->handle_event(hdl_node, event, observer);
		}
#else
		handler->handle_event(hdl_node, event, observer);
#endif
	}
		break;
	case TAG_LSR_conditional:
		if ( ((SVG_Element*)hdl_node)->children)
			gf_node_traverse(((SVG_Element*)hdl_node)->children->node, NULL);
		break;
	case TAG_SVG_a:
	{
		GF_DOM_Event act;
		memset(&act, 0, sizeof(GF_DOM_Event));
		act.type = GF_EVENT_ACTIVATE;
		gf_dom_event_fire((GF_Node *)hdl_node, &act);
	}
		break;
	default:
		return;
	}
}

static Bool sg_fire_dom_event(GF_DOMEventTarget *et, GF_DOM_Event *event, GF_SceneGraph *sg, GF_Node *n)
{
	if (et) {
		GF_Node *observer = (et->ptr_type==GF_DOM_EVENT_NODE) ? et->ptr : NULL;
		u32 i, count, post_count;
		count = gf_list_count(et->evt_list);
		for (i=0; i<count; i++) {
			XMLEV_Event *listened_event;
			GF_Node *listen = (GF_Node *)gf_list_get(et->evt_list, i);

			switch (listen->sgprivate->tag) {
			case TAG_SVG_listener:
			{
				SVGAllAttributes atts;
				gf_svg_flatten_attributes((SVG_Element*)listen, &atts);
				listened_event = atts.event;
				if (!listened_event) continue;

				if (atts.propagate && (*atts.propagate==XMLEVENT_PROPAGATE_STOP))
					event->event_phase |= GF_DOM_EVENT_PHASE_CANCEL;
				if (atts.defaultAction && (*atts.defaultAction==XMLEVENT_DEFAULTACTION_CANCEL))
					event->event_phase |= GF_DOM_EVENT_PHASE_PREVENT;
			}
				break;
			default:
				continue;
			}
			if (listened_event->type <= GF_EVENT_MOUSEMOVE) event->has_ui_events=1;
			if (listened_event->type != event->type) continue;
			if (listened_event->parameter && (listened_event->parameter != event->detail)) continue;
			event->currentTarget = et;
			event->consumed	++;

			/*load event cannot bubble and can only be called once (on load :) ), remove it
			to release some resources*/
			if (event->type==GF_EVENT_LOAD) {
				dom_event_process(listen, event, observer);
				/*delete listener*/
				gf_dom_listener_del(listen, et);
			} else if (n) {
				assert(n->sgprivate->num_instances);
				/*protect node*/
				n->sgprivate->num_instances++;
				/*exec event*/
				dom_event_process(listen, event, observer);
				/*the event has destroyed ourselves, abort propagation
				THIS IS NOT DOM compliant, the event should propagate on the original target+ancestors path*/
				if (n->sgprivate->num_instances==1) {
					/*unprotect node event*/
					gf_node_unregister(n, NULL);
					return 0;
				}
				n->sgprivate->num_instances--;
			} else {
				dom_event_process(listen, event, observer);
			}
			/*canceled*/
			if (event->event_phase & GF_DOM_EVENT_PHASE_CANCEL_ALL) {
				gf_dom_listener_process_add(sg);
				return 0;
			}

			/*if listeners have been removed, update count*/
			post_count = gf_list_count(et->evt_list);
			if (post_count < count) {
				s32 pos = gf_list_find(et->evt_list, listen);
				if (pos>=0) i = pos;
				/*FIXME this is not going to work in all cases...*/
				else i--;
				count = post_count;
			}
		}
		/*propagation stoped*/
		if (event->event_phase & (GF_DOM_EVENT_PHASE_CANCEL|GF_DOM_EVENT_PHASE_CANCEL_ALL) ) {
			gf_dom_listener_process_add(sg);
			return 0;
		}
	}
	gf_dom_listener_process_add(sg);
	/*if the current target is a node, we can bubble*/
	return n ? 1 : 0;
}

static void gf_sg_dom_event_bubble(GF_Node *node, GF_DOM_Event *event, GF_List *use_stack, u32 cur_par_idx)
{
	GF_Node *parent;

	if (!node) return;

	/*get the node's parent*/
	parent = gf_node_get_parent(node, 0);

	if (!parent) {
	/*top of the graph, use Document*/
		if (node->sgprivate->scenegraph->RootNode==node)
			sg_fire_dom_event(&node->sgprivate->scenegraph->dom_evt, event, node->sgprivate->scenegraph, NULL);
		return;
	}
	if (cur_par_idx) {
		GF_Node *used_node = gf_list_get(use_stack, cur_par_idx-1);
		/*if the node is a used one, switch to the <use> subtree*/
		if (used_node==node) {
			parent = gf_list_get(use_stack, cur_par_idx);
			if (cur_par_idx>1) cur_par_idx-=2;
			else cur_par_idx = 0;
			node->sgprivate->scenegraph->use_stack = use_stack;
			/*if no events attached,bubble by default*/
			if (parent->sgprivate->interact && !sg_fire_dom_event(parent->sgprivate->interact->dom_evt, event, node->sgprivate->scenegraph, parent)) return;
			gf_sg_dom_event_bubble(parent, event, use_stack, cur_par_idx);
			node->sgprivate->scenegraph->use_stack = use_stack;
			return;
		}
	}
	/*if no events attached,bubble by default*/
	if (parent->sgprivate->interact && !sg_fire_dom_event(parent->sgprivate->interact->dom_evt, event, node->sgprivate->scenegraph, parent)) return;
	gf_sg_dom_event_bubble(parent, event, use_stack, cur_par_idx);
}


void gf_sg_dom_stack_parents(GF_Node *node, GF_List *stack)
{
	if (!node) return;
	if (node->sgprivate->interact && node->sgprivate->interact->dom_evt) gf_list_insert(stack, node, 0);
	gf_sg_dom_stack_parents(gf_node_get_parent(node, 0), stack);
}

GF_EXPORT
Bool gf_dom_event_fire_ex(GF_Node *node, GF_DOM_Event *event, GF_List *use_stack)
{
	GF_DOMEventTarget cur_target;
	u32 cur_par_idx;
	if (!node || !event) return 0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[DOM Events    ] Time %f - Firing event  %s.%s\n", gf_node_get_scene_time(node), gf_node_get_log_name(node), gf_dom_event_get_name(event->type)));

	/*flush any pending add_listener
	see "determine the current target's candidate event listeners" in http://www.w3.org/TR/DOM-Level-3-Events/events.html*/
	gf_dom_listener_process_add(node->sgprivate->scenegraph);

	event->consumed = 0;
	event->target = node;
	event->target_type = GF_DOM_EVENT_NODE;
	if (node->sgprivate->interact && node->sgprivate->interact->dom_evt) {
		event->currentTarget = node->sgprivate->interact->dom_evt;
	} else {
		cur_target.ptr_type = GF_DOM_EVENT_NODE;
		cur_target.ptr = node;
		cur_target.evt_list = NULL;
		event->currentTarget = &cur_target;
	}

	/*capture phase - not 100% sure, the actual capture phase should be determined by the std using the DOM events
	SVGT doesn't use this phase, so we don't add it for now.*/
	if (0) {
		Bool aborted = 0;
		u32 i, count;
		GF_List *parents;
		event->event_phase = GF_DOM_EVENT_PHASE_CAPTURE;
		parents = gf_list_new();
		/*get all parents to top*/
		gf_sg_dom_stack_parents(gf_node_get_parent(node, 0), parents);
		count = gf_list_count(parents);
		for (i=0; i<count; i++) {
			GF_Node *n = (GF_Node *)gf_list_get(parents, i);
			if (n->sgprivate->interact)
				sg_fire_dom_event(n->sgprivate->interact->dom_evt, event, node->sgprivate->scenegraph, n);

			/*event has been canceled*/
			if (event->event_phase & (GF_DOM_EVENT_PHASE_CANCEL|GF_DOM_EVENT_PHASE_CANCEL_ALL) ) {
				aborted = 1;
				break;
			}
		}
		gf_list_del(parents);
		if (aborted) return 1;
	}
	/*target phase*/
	event->event_phase = GF_DOM_EVENT_PHASE_AT_TARGET;
	cur_par_idx = 0;
	if (use_stack) {
		cur_par_idx = gf_list_count(use_stack);
		if (cur_par_idx) cur_par_idx--;
	}
	if ( (!node->sgprivate->interact || sg_fire_dom_event(node->sgprivate->interact->dom_evt, event, node->sgprivate->scenegraph, node)) 
		&& event->bubbles) {
		/*bubbling phase*/
		event->event_phase = GF_DOM_EVENT_PHASE_BUBBLE;
		gf_sg_dom_event_bubble(node, event, use_stack, cur_par_idx);
	}

	return event->consumed ? 1 : 0;
}

GF_EXPORT
Bool gf_dom_event_fire(GF_Node *node, GF_DOM_Event *event)
{
	return gf_dom_event_fire_ex(node, event, NULL);
}

GF_DOMHandler *gf_dom_listener_build_ex(GF_Node *node, u32 event_type, u32 event_parameter, GF_Node *handler, GF_Node **out_listener)
{
	u32 tag;
	SVG_Element *listener;
	GF_FieldInfo info;
	GF_ChildNodeItem *last = NULL;

	tag = gf_node_get_tag(node);
	listener = (SVG_Element *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_listener);
	/*don't register the listener, this will be done when adding to the node events list*/

	if (handler) {
		if (gf_node_get_attribute_by_tag(handler, TAG_XMLEV_ATT_event, 0, 0, &info)==GF_OK) {
			event_type = ((XMLEV_Event *)info.far_ptr)->type;
			event_parameter = ((XMLEV_Event *)info.far_ptr)->parameter;
		}
	} else {
		handler = gf_node_new(node->sgprivate->scenegraph, TAG_SVG_handler);
		gf_node_get_attribute_by_tag(handler, TAG_XMLEV_ATT_event, 1, 0, &info);
		((XMLEV_Event *)info.far_ptr)->type = event_type;
		((XMLEV_Event *)info.far_ptr)->parameter = event_parameter;

		/*register the handler as a child of the listener*/
		gf_node_register((GF_Node *)handler, (GF_Node *) listener);
		gf_node_list_add_child_last(& ((GF_ParentNode *)listener)->children, (GF_Node*)handler, &last);
	}
	
	gf_node_get_attribute_by_tag((GF_Node*)listener, TAG_XMLEV_ATT_event, 1, 0, &info);
	((XMLEV_Event *)info.far_ptr)->type = event_type;
	((XMLEV_Event *)info.far_ptr)->parameter = event_parameter;

	gf_node_get_attribute_by_tag((GF_Node*)listener, TAG_XMLEV_ATT_handler, 1, 0, &info);
	((XMLRI *)info.far_ptr)->target = handler;

	gf_node_get_attribute_by_tag((GF_Node*)listener, TAG_XMLEV_ATT_target, 1, 0, &info);
	((XMLRI *)info.far_ptr)->target = node;
	
	gf_node_dom_listener_add((GF_Node *) node, (GF_Node *) listener);

	if (out_listener) *out_listener = (GF_Node *) listener;

	/*set default handler*/
	((SVG_handlerElement *) handler)->handle_event = gf_sg_handle_dom_event;
	return (SVG_handlerElement *) handler;
}

GF_EXPORT
GF_DOMHandler *gf_dom_listener_build(GF_Node *node, u32 event_type, u32 event_parameter)
{
	return gf_dom_listener_build_ex(node, event_type, event_parameter, NULL, NULL);
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
		/*only handle event if coming from the watched element*/
		if (proto->element) {
			if ((evt->currentTarget->ptr_type!=GF_DOM_EVENT_NODE) || (proto->element!= (GF_Node*)evt->currentTarget->ptr)) 
				continue;
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SMIL, ("[SMIL Timing   ] Time %f - Timed element %s - Inserting new time in %s: %f\n", gf_node_get_scene_time(timed_elt), gf_node_get_log_name(timed_elt), (is_end?"end":"begin"), resolved->clock));
	}
	/* calling indirectly gf_smil_timing_modified */
	if (found) gf_node_changed(timed_elt, info);
}

static void gf_smil_handle_event_begin(GF_Node *hdl, GF_DOM_Event *evt, GF_Node *observer)
{
	GF_FieldInfo info;
	GF_Node *timed_elt = (GF_Node *)gf_node_get_private(hdl);
	memset(&info, 0, sizeof(GF_FieldInfo));
	info.name = "begin";
	info.far_ptr = ((SVGTimedAnimBaseElement *)timed_elt)->timingp->begin;
	info.fieldType = SMIL_Times_datatype;
	gf_smil_handle_event(timed_elt, &info, evt, 0);
}

static void gf_smil_handle_event_end(GF_Node *hdl, GF_DOM_Event *evt, GF_Node *observer)
{
	GF_FieldInfo info;
	GF_Node *timed_elt = (GF_Node *)gf_node_get_private(hdl);
	memset(&info, 0, sizeof(GF_FieldInfo));
	info.name = "end";
	info.far_ptr = ((SVGTimedAnimBaseElement *)timed_elt)->timingp->end;
	info.fieldType = SMIL_Times_datatype;
	gf_smil_handle_event((GF_Node *)timed_elt, &info, evt, 1);
}

static void gf_smil_setup_event_list(GF_Node *node, GF_List *l, Bool is_begin)
{
	void *hdl;
	u32 i, count;
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

		/*create a new listener*/
		hdl = gf_dom_listener_build_ex(t->element, t->event.type, t->event.parameter, NULL, &t->listener);

		/*register the listener so that we can handle cyclic references: 
			- If the anim node gets destroyed, the listener is removed through the SMIL_Time reference
			- If the target gets destroyed, the listener is removed through the regular way
		*/
		if (t->listener)
			gf_node_register(t->listener, NULL);
		
		if (hdl) {
			((SVG_handlerElement *)hdl)->handle_event = is_begin ? gf_smil_handle_event_begin : gf_smil_handle_event_end;
		}
		else {
			continue;
		}
		gf_node_set_private((GF_Node *)hdl, node);
		/*we keep the t->element pointer in order to discard the source of identical events (begin of # elements, ...)*/
	}
}

void gf_smil_setup_events(GF_Node *node)
{
	GF_FieldInfo info;
	if (gf_node_get_attribute_by_tag(node, TAG_SVG_ATT_begin, 0, 0, &info) == GF_OK)
		gf_smil_setup_event_list(node, * (GF_List **)info.far_ptr, 1);
	if (gf_node_get_attribute_by_tag(node, TAG_SVG_ATT_end, 0, 0, &info) == GF_OK)
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

