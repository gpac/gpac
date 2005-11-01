/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2004
 *					All rights reserved
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

#ifndef GPAC_DISABLE_SVG

#include <gpac/nodes_svg.h>

SVGElement *SVG_NewNode(GF_SceneGraph *inScene, u32 tag)
{
	SVGElement *node;
	if (!inScene) return NULL;
	node = SVG_CreateNode(tag);
	if (node) {
		node->sgprivate->scenegraph = inScene;
	}
	return (SVGElement *)node;
}

GF_Err gf_node_listener_add(GF_Node *node, GF_Node *listener)
{
	if (!node || !listener) return GF_BAD_PARAM;
	if (listener->sgprivate->tag!=TAG_SVG_listener) return GF_BAD_PARAM;

	if (!node->sgprivate->events) node->sgprivate->events = gf_list_new();
	return gf_list_add(node->sgprivate->events, listener);
}

GF_Err gf_node_listener_del(GF_Node *node, GF_Node *listener)
{
	if (!node || !node->sgprivate->events || !listener) return GF_BAD_PARAM;
	return (gf_list_del_item(node->sgprivate->events, listener)<0) ? GF_BAD_PARAM : GF_OK;
}

u32 gf_node_listener_count(GF_Node *node)
{
	if (!node || !node->sgprivate->events) return 0;
	return gf_list_count(node->sgprivate->events);
}

GF_Node *gf_node_listener_get(GF_Node *node, u32 i)
{
	if (!node || !node->sgprivate->events) return 0;
	return gf_list_get(node->sgprivate->events, i);
}

void gf_sg_handle_dom_event(SVGhandlerElement *hdl, GF_DOM_Event *event)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	if (hdl->sgprivate->scenegraph->svg_js) 
		if (hdl->sgprivate->scenegraph->svg_js->script_execute(hdl->sgprivate->scenegraph, hdl->textContent, event)) return;
#endif
	/*no clue what this is*/
	fprintf(stdout, "SVG: Unknown handler\n");
}

static void svg_process_event(SVGlistenerElement *listen, GF_DOM_Event *event)
{
	SVGhandlerElement *handler = (SVGhandlerElement *) listen->handler.target;
	if (!handler || !handler->handle_event) return;
	if (handler->ev_event != event->type) return;
	handler->handle_event(handler, event);
}

static Bool sg_fire_dom_event(GF_Node *node, GF_DOM_Event *event)
{
	if (!node) return 0;

	if (node->sgprivate->events) {
		u32 i, count;
		count = gf_list_count(node->sgprivate->events);
		for (i=0; i<count; i++) {
			SVGlistenerElement *listen = gf_list_get(node->sgprivate->events, i);
			if (listen->event<=SVG_DOM_EVT_MOUSEMOVE) event->has_ui_events=1;
			if (listen->event != event->type) continue;
			event->currentTarget = node;
			/*process event*/
			svg_process_event(listen, event);
			
			/*load event cannot bubble and can only be called once (on load :) ), remove it
			to release some resources*/
			if (event->type==SVG_DOM_EVT_LOAD) {
				gf_list_rem(node->sgprivate->events, i);
				count--;
				i--;
				gf_node_replace((GF_Node *) listen->handler.target, NULL, 0);
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
	if (node->sgprivate->events) gf_list_insert(stack, node, 0);
	gf_sg_dom_stack_parents(gf_node_get_parent(node, 0), stack);
}

Bool gf_sg_fire_dom_event(GF_Node *node, GF_DOM_Event *event)
{
	if (!node || !event) return 0;
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
			GF_Node *n = gf_list_get(parents, i);
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
		gf_sg_dom_event_bubble(gf_node_get_parent(node, 0), event);
	}

	return event->currentTarget ? 1 : 0;
}

Bool gf_sg_svg_node_init(GF_Node *node)
{
	GF_DOM_Event evt;
	memset(&evt, 0, sizeof(GF_DOM_Event));
	evt.type = SVG_DOM_EVT_LOAD;
	gf_sg_fire_dom_event(node, &evt);

	if ((node->sgprivate->tag==TAG_SVG_script) && node->sgprivate->scenegraph->script_load) {
		node->sgprivate->scenegraph->script_load(node);
		return 1;
	}
	else if (node->sgprivate->tag==TAG_SVG_handler) {
		if (node->sgprivate->scenegraph->js_ifce)
			((SVGhandlerElement *)node)->handle_event = gf_sg_handle_dom_event;
		return 1;
	}
	return 0;
}

#else
/*these ones are only needed for W32 libgpac_dll build in order not to modify export def file*/
u32 SVG_GetTagByName(const char *element_name)
{
	return 0;
}
u32 SVG_GetAttributeCount(GF_Node *n)
{
	return 0;
}
GF_Err SVG_GetAttributeInfo(GF_Node *node, GF_FieldInfo *info)
{
	return GF_NOT_SUPPORTED;
}

GF_Node *SVG_NewNode(GF_SceneGraph *inScene, u32 tag)
{
	return NULL;
}
GF_Node *SVG_CreateNode(GF_SceneGraph *inScene, u32 tag)
{
	return NULL;
}
const char *SVG_GetElementName(u32 tag)
{
	return "Unsupported";
}
Bool gf_sg_svg_node_init(GF_Node *node)
{
	return 0;
}


#endif	//GPAC_DISABLE_SVG
