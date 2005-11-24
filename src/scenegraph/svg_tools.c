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

SVGElement *gf_svg_new_node(GF_SceneGraph *inScene, u32 tag)
{
	SVGElement *node;
	if (!inScene) return NULL;
	node = gf_svg_create_node(tag);
	if (node) {
		node->sgprivate->scenegraph = inScene;
	}
	return (SVGElement *)node;
}

GF_Err gf_node_animation_add(GF_Node *node, void *animation)
{
	if (!node || !animation) return GF_BAD_PARAM;
	if (!node->sgprivate->animations) node->sgprivate->animations = gf_list_new();
	return gf_list_add(node->sgprivate->animations, animation);
}

GF_Err gf_node_animation_del(GF_Node *node)
{
	if (!node || !node->sgprivate->animations) return GF_BAD_PARAM;
	gf_list_del(node->sgprivate->animations);
	return GF_OK;
}

u32 gf_node_animation_count(GF_Node *node)
{
	if (!node || !node->sgprivate->animations) return 0;
	return gf_list_count(node->sgprivate->animations);
}

void *gf_node_animation_get(GF_Node *node, u32 i)
{
	if (!node || !node->sgprivate->animations) return 0;
	return gf_list_get(node->sgprivate->animations, i);
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

SVGhandlerElement *gf_sg_dom_create_listener(GF_Node *node, u32 eventType)
{
	SVGlistenerElement *listener;
	SVGhandlerElement *handler;

	/*emulate a listener for onClick event*/
	listener = (SVGlistenerElement *) gf_svg_new_node(node->sgprivate->scenegraph, TAG_SVG_listener);
	handler = (SVGhandlerElement *) gf_svg_new_node(node->sgprivate->scenegraph, TAG_SVG_handler);
	gf_node_register((GF_Node *)listener, node);
	gf_list_add( ((GF_ParentNode *)node)->children, listener);
	gf_node_register((GF_Node *)handler, node);
	gf_list_add(((GF_ParentNode *)node)->children, handler);
	handler->ev_event = listener->event = eventType;
	listener->handler.target = (SVGElement *) handler;
	listener->target.target = (SVGElement *)node;
	gf_node_listener_add((GF_Node *) node, (GF_Node *) listener);
	/*set default handler*/
	handler->handle_event = gf_sg_handle_dom_event;
	return handler;
}


static void gf_smil_handle_event(GF_Node *anim, GF_List *l, GF_DOM_Event *evt)
{
	SMIL_Time *resolved, *proto;
	u32 found = 0;
	u32 i, j, count = gf_list_count(l);

	/*remove all previously instantiated times?? TODO move this chack to SMIL anim module*/
	for (i=0; i<count; i++) {
		proto = gf_list_get(l, i);
		if (proto->dynamic_type == 2) {
			free(proto);
			gf_list_rem(l, i);
			i--;
			count--;
		}
	}
	for (i=0; i<count; i++) {
		proto = gf_list_get(l, i);
		if (proto->dynamic_type!=1) continue;
		if (proto->event!=evt->type) continue;
		if ((evt->type!=SVG_DOM_EVT_KEYPRESS) || (proto->parameter==evt->detail)) {
			/*solve*/
			GF_SAFEALLOC(resolved, sizeof(SMIL_Time));
			resolved->type = SMIL_TIME_CLOCK;
			resolved->clock = gf_node_get_scene_time(evt->target) + proto->clock;
			resolved->dynamic_type=2;
			for (j=0; j<count; j++) {
				proto = gf_list_get(l, j);
				if (proto->dynamic_type==1) continue;
				if (proto->clock > resolved->clock) break;
			}
			gf_list_insert(l, resolved, j);
			count++;
			found++;
		}
	}
	if (found) gf_node_changed(anim, NULL);
}

static void gf_smil_handle_event_begin(SVGhandlerElement *hdl, GF_DOM_Event *evt)
{
	GF_FieldInfo info;
	GF_Node *anim = gf_node_get_private((GF_Node *)hdl);
	gf_node_get_field_by_name(anim, "begin", &info);
	gf_smil_handle_event(anim, *(GF_List **)info.far_ptr, evt);
}

static void gf_smil_handle_event_end(SVGhandlerElement *hdl, GF_DOM_Event *evt)
{
	GF_FieldInfo info;
	GF_Node *anim = gf_node_get_private((GF_Node *)hdl);
	gf_node_get_field_by_name(anim, "end", &info);
	gf_smil_handle_event(anim, *(GF_List **)info.far_ptr, evt);
}

static void gf_smil_setup_event_list(GF_Node *node, GF_List *l, Bool is_begin)
{
	SVGhandlerElement *hdl;
	u32 i, count;
	count = gf_list_count(l);
	for (i=0; i<count; i++) {
		SMIL_Time *t = gf_list_get(l, i);
		if (t->type != SMIL_TIME_EVENT) continue;
		/*already setup*/
		if (t->dynamic_type) continue;
		/*not resolved yet*/
		if (!t->element && t->element_id) continue;
		hdl = gf_sg_dom_create_listener(t->element, t->event);
		hdl->handle_event = is_begin ? gf_smil_handle_event_begin : gf_smil_handle_event_end;
		gf_node_set_private((GF_Node *)hdl, node);
		t->element = NULL;
		t->dynamic_type = 1;
	}
}

static void gf_smil_setup_events(GF_Node *node)
{
	GF_FieldInfo info;
	if (gf_node_get_field_by_name(node, "begin", &info) != GF_OK) return;
	gf_smil_setup_event_list(node, * (GF_List **)info.far_ptr, 1);
	if (gf_node_get_field_by_name(node, "end", &info) != GF_OK) return;
	gf_smil_setup_event_list(node, * (GF_List **)info.far_ptr, 0);
}

Bool gf_sg_svg_node_init(GF_Node *node)
{
	switch (node->sgprivate->tag) {
	case TAG_SVG_script:
		if (node->sgprivate->scenegraph->script_load) 
			node->sgprivate->scenegraph->script_load(node);
		return 1;
	case TAG_SVG_handler:
		if (node->sgprivate->scenegraph->js_ifce)
			((SVGhandlerElement *)node)->handle_event = gf_sg_handle_dom_event;
		return 1;
	case TAG_SVG_animateMotion:
	case TAG_SVG_set: 
	case TAG_SVG_animate: 
	case TAG_SVG_animateColor: 
	case TAG_SVG_animateTransform: 
		gf_smil_anim_init_node(node);
	case TAG_SVG_audio: 
	case TAG_SVG_video: 
		gf_smil_setup_events(node);
		/*we may get called several times depending on xlink:href resoling for events*/
		return (node->sgprivate->privateStack || node->sgprivate->RenderNode) ? 1 : 0;
	default:
		return 0;
	}
	return 0;
}

#include <gpac/base_coding.h>
Bool svg_store_embedded_data(SVG_IRI *iri, const char *iri_data, const char *cache_dir, const char *base_filename)
{
	char szFile[GF_MAX_PATH], *sep, buf[20], *data;
	u32 data_size;

	if (!cache_dir || !base_filename || !iri || !!strncmp(iri_data, "data:", 5)) return 0;

	/*handle "data:" scheme when cache is specified*/
	strcpy(szFile, cache_dir);
	data_size = strlen(szFile);
	if (szFile[data_size-1] != GF_PATH_SEPARATOR) {
		szFile[data_size] = GF_PATH_SEPARATOR;
		szFile[data_size+1] = 0;
	}
	if (base_filename) {
		sep = strrchr(base_filename, GF_PATH_SEPARATOR);
#ifdef WIN32
		if (!sep) sep = strrchr(base_filename, '/');
#endif
		if (!sep) sep = (char *) base_filename;
		else sep += 1;
		strcat(szFile, sep);
	}
	sep = strrchr(szFile, '.');
	if (sep) sep[0] = 0;
	sprintf(buf, "_img_%08X", (u32) iri);
	strcat(szFile, buf);
	/*get mime type*/
	sep = (char *)iri_data + 5;
	if (!strncmp(sep, "image/jpg", 9) || !strncmp(sep, "image/jpeg", 10)) strcat(szFile, ".jpg");
	else if (!strncmp(sep, "image/png", 9)) strcat(szFile, ".png");

	data = NULL;
	sep = strchr(iri_data, ';');
	if (!strncmp(sep, ";base64,", 8)) {
		sep += 8;
		data_size = 2*strlen(sep);
		data = malloc(sizeof(char)*data_size);
		data_size = gf_base64_decode(sep, strlen(sep), data, data_size);
	}
	else if (!strncmp(sep, ";base16,", 8)) {
		data_size = 2*strlen(sep);
		data = malloc(sizeof(char)*data_size);
		sep += 8;
		data_size = gf_base16_decode(sep, strlen(sep), data, data_size);
	}
	iri->type = SVG_IRI_IRI;
	if (data) {
		FILE *f = fopen(szFile, "wb");
		fwrite(data, data_size, 1, f);
		fclose(f);
		if (iri->iri) free(iri->iri);
		iri->iri = strdup(szFile);
		free(data);
	} 
	return 1;
}

#else
/*these ones are only needed for W32 libgpac_dll build in order not to modify export def file*/
u32 gf_svg_get_tag_by_name(const char *element_name)
{
	return 0;
}
u32 gf_svg_get_attribute_count(GF_Node *n)
{
	return 0;
}
GF_Err gf_svg_get_attribute_info(GF_Node *node, GF_FieldInfo *info)
{
	return GF_NOT_SUPPORTED;
}

GF_Node *gf_svg_new_node(GF_SceneGraph *inScene, u32 tag)
{
	return NULL;
}
GF_Node *gf_svg_create_node(GF_SceneGraph *inScene, u32 tag)
{
	return NULL;
}
const char *gf_svg_get_element_name(u32 tag)
{
	return "Unsupported";
}
Bool gf_sg_svg_node_init(GF_Node *node)
{
	return 0;
}


#endif	//GPAC_DISABLE_SVG
