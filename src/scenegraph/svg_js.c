/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Graph sub-project
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


#ifdef GPAC_HAS_QJS

#ifdef GPAC_CONFIG_ANDROID
#ifndef XP_UNIX
#define XP_UNIX
#endif
#endif

#include "qjs_common.h"

#define JSVAL_CHECK_STRING(_v) (JS_IsString(_v) || JS_IsNull(_v))


/*SVG uDOM classes*/
GF_JSClass svgElement;
GF_JSClass svgDocument;
GF_JSClass svg_globalClass;
GF_JSClass connectionClass;
GF_JSClass rgbClass;
GF_JSClass rectClass;
GF_JSClass pointClass;
GF_JSClass pathClass;
GF_JSClass matrixClass;

typedef struct
{
	JSValue proto;
	JSValue ctor;
} SVG_JSClass;


typedef struct __tag_svg_script_ctx
{
	Bool (*script_execute)(struct __tag_scene_graph *sg, char *utf8_script, GF_DOM_Event *event);
	Bool (*handler_execute)(GF_Node *n, GF_DOM_Event *event, GF_Node *observer, char *utf8_script);
	u32 nb_scripts;
	/*global script context for the scene*/
	JSContext *js_ctx;
	/*global object*/
	JSValue global;
	/*global event object - used to update the associated DOMEvent (JS private stack) when dispatching events*/
	JSValue event;

	Bool in_script;
	Bool force_gc;
	Bool use_strict;

	/*SVG uDOM classes*/
	SVG_JSClass svgElement;
	SVG_JSClass svgDocument;
	SVG_JSClass svg_globalClass;
	SVG_JSClass connectionClass;
	SVG_JSClass rgbClass;
	SVG_JSClass rectClass;
	SVG_JSClass pointClass;
	SVG_JSClass pathClass;
	SVG_JSClass matrixClass;
} GF_SVGJS;

void svg_mark_gc(struct __tag_svg_script_ctx *svg_js)
{
	if (svg_js)
		svg_js->force_gc = 1;
}

void svg_free_node_binding(struct __tag_svg_script_ctx *svg_js, GF_Node *node)
{
	struct _node_js_binding *js_binding = node->sgprivate->interact->js_binding;
	if (!JS_IsUndefined(js_binding->obj)) {
		JS_SetOpaque(js_binding->obj, NULL);
		JS_FreeValue(svg_js->js_ctx, js_binding->obj);
		js_binding->obj = JS_UNDEFINED;
		//unregister after destroying JS obj since this is a recursive call and we trigger the GC, we must make sure
		//all JS opaque is NULL before destroying the node
		gf_node_unregister(node, NULL);
	}

	if (svg_js->in_script)
		svg_js->force_gc = 1;
	else
		gf_js_call_gc(svg_js->js_ctx);
}

GF_Err svg_exec_script(struct __tag_svg_script_ctx *svg_js, GF_SceneGraph *sg, const char *com)
{
	Bool ret = sg->svg_js->script_execute(sg, (char *)com, NULL);
	return (ret == GF_TRUE ? GF_OK : GF_BAD_PARAM);
}

void svgjs_handler_execute(struct __tag_svg_script_ctx *svg_js, GF_Node *hdl, GF_DOM_Event *event, GF_Node *observer, const char *iri)
{
	if (svg_js->handler_execute(hdl, event, observer, (char *) iri)) {
		return;
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_INTERACT, ("[DOM Events] Error executing JavaScript event handler\n"));
}
static Bool svg_script_execute_handler(GF_Node *node, GF_DOM_Event *event, GF_Node *observer, char *utf8_script);

void dom_node_set_textContent(GF_Node *n, char *text);

JSValue dom_node_get_sibling(JSContext *c, GF_Node *n, Bool is_prev, Bool elt_only);

#ifdef GPAC_ENABLE_HTML5_MEDIA
void html_media_init_js_api(GF_SceneGraph *scene);
#endif

#define _ScriptMessage(_sg, _msg) {\
			GF_JSAPIParam par;	\
			par.info.e = GF_SCRIPT_INFO;			\
			par.info.msg = _msg;		\
			_sg->script_action(_sg->script_action_cbck, GF_JSAPI_OP_MESSAGE, NULL, &par);\
		}

static GFINLINE Bool ScriptAction(GF_SceneGraph *scene, u32 type, GF_Node *node, GF_JSAPIParam *param)
{
	if (scene->script_action)
		return scene->script_action(scene->script_action_cbck, type, node, param);
	return GF_FALSE;
}

static JSValue svg_new_path_object(JSContext *c, SVG_PathData *d);



/*note we are using float to avoid conversions fixed to/from JS native */
typedef struct
{
	u32 r, g, b;
} rgbCI;


typedef struct
{
	Float x, y, w, h;
	/*if set, this is the svg.viewport uDOM object, its values are updated upon query*/
	GF_SceneGraph *sg;
} rectCI;

typedef struct
{
	Float x, y;
	/*if set, this is the svg.currentTranslate uDOM object, its values are updated upon query*/
	GF_SceneGraph *sg;
} pointCI;

typedef struct
{
	Float x, y;
} ptCI;

typedef struct
{
	u32 nb_coms;
	u8 *tags;
	ptCI *pts;
} pathCI;

static JSValue svg_nav_to_location(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_JSAPIParam par;
	GF_SceneGraph *sg = JS_GetOpaque(obj, svg_globalClass.class_id);
	if ((argc!=1) || !sg)
		return GF_JS_EXCEPTION(c);

	par.uri.url = (char *) JS_ToCString(c, argv[0]);
	par.uri.nb_params = 0;
	if (par.uri.url) {
		ScriptAction(sg, GF_JSAPI_OP_LOAD_URL, sg->RootNode, &par);
		JS_FreeCString(c, par.uri.url);
	}
	return JS_UNDEFINED;
}

GF_Node *gf_sm_load_svg_from_string(GF_SceneGraph *sg, char *svg_str);
static JSValue svg_parse_xml(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_SceneGraph *sg;
	GF_Node *node;
	const char *str;

	if ((argc<2) || !JS_IsObject(argv[1])) {
		return js_throw_err(c, GF_DOM_EXC_WRONG_DOCUMENT_ERR);
	}

	sg = dom_get_doc(c, argv[1]);
	if (!sg) return js_throw_err(c, GF_DOM_EXC_WRONG_DOCUMENT_ERR);

	str = JS_ToCString(c, argv[0]);
	if (!str) return JS_TRUE;

	node = gf_sm_load_svg_from_string(sg, (char *) str);
	JS_FreeCString(c, str);
	return dom_element_construct(c, node);
}

static void svg_define_udom_exception(JSContext *c, JSValue global)
{
	JSValue obj = JS_NewObject(c);
	JS_SetPropertyStr(c, global, "GlobalException", obj);
#define DEFCONST(_name, _code)\
		JS_SetPropertyStr(c, obj, _name, JS_NewInt32(c, _code));

	DEFCONST("NOT_CONNECTED_ERR", 1)
	DEFCONST("ENCODING_ERR", 2)
	DEFCONST("DENIED_ERR", 3)
	DEFCONST("UNKNOWN_ERR", 4)

	obj = JS_NewObject(c);
	JS_SetPropertyStr(c, global, "SVGException", obj);
	DEFCONST("SVG_WRONG_TYPE_ERR", 0)
	DEFCONST("SVG_INVALID_VALUE_ERR", 1)
	DEFCONST("SVG_MATRIX_NOT_INVERTABLE", 2)

	obj = JS_NewObject(c);
	JS_SetPropertyStr(c, global, "SVGSVGElement", obj);
	DEFCONST("NAV_AUTO", 1)
	DEFCONST("NAV_NEXT", 2)
	DEFCONST("NAV_PREV", 3)
	DEFCONST("NAV_UP", 4)
	DEFCONST("NAV_UP_RIGHT", 5)
	DEFCONST("NAV_RIGHT", 6)
	DEFCONST("NAV_DOWN_RIGHT", 7)
	DEFCONST("NAV_DOWN", 8)
	DEFCONST("NAV_DOWN_LEFT", 9)
	DEFCONST("NAV_LEFT", 10)
	DEFCONST("NAV_UP_LEFT", 11)
}

static JSValue global_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_SceneGraph *sg = JS_GetOpaque(obj, svg_globalClass.class_id);
	if (!sg) return GF_JS_EXCEPTION(c);

	switch (magic) {
	/*namespaceURI*/
	case 0:
		return JS_NULL;
	/*parent*/
	case 1:
		if (sg->parent_scene && sg->parent_scene->svg_js)
			return JS_DupValue(c, sg->parent_scene->svg_js->global);
		return JS_NULL;
	default:
		return JS_UNDEFINED;
	}
}

/*TODO - try to be more precise...*/
static JSValue dom_imp_has_feature(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	JSValue ret = JS_FALSE;

	if (argc) {
		u32 len;
		char sep;
		char *fname = (char *) JS_ToCString(c, argv[0]);
		if (!fname) return JS_TRUE;
		while (strchr(" \t\n\r", fname[0])) fname++;
		len = (u32) strlen(fname);
		while (len && strchr(" \t\n\r", fname[len-1])) len--;
		sep = fname[len];
		fname[len] = 0;
		if (!stricmp(fname, "xml")) ret = JS_TRUE;
		else if (!stricmp(fname, "core")) ret = JS_TRUE;
		else if (!stricmp(fname, "traversal")) ret = JS_TRUE;
		else if (!stricmp(fname, "uievents")) ret = JS_TRUE;
		else if (!stricmp(fname, "mouseevents")) ret = JS_TRUE;
		else if (!stricmp(fname, "mutationevents")) ret = JS_TRUE;
		else if (!stricmp(fname, "events")) ret = JS_TRUE;

		fname[len] = sep;
		JS_FreeCString(c, fname);
	}
	return ret;
}

static GF_Node *get_corresponding_use(GF_Node *n)
{
	u32 i, count;
	if (!n || !n->sgprivate->scenegraph->use_stack) return NULL;

	/*find current node in the use stack - if found, return the use element*/
	count = gf_list_count(n->sgprivate->scenegraph->use_stack);
	for (i=count; i>0; i-=2) {
		GF_Node *t = (GF_Node *)gf_list_get(n->sgprivate->scenegraph->use_stack, i-2);
		if (t==n) {
			GF_Node *use = (GF_Node *)gf_list_get(n->sgprivate->scenegraph->use_stack, i-1);
			GF_Node *par_use = get_corresponding_use(use);
			return par_use ? par_use : use;
		}
	}
	/*otherwise recursively get up the tree*/
	return get_corresponding_use(gf_node_get_parent(n, 0));
}

static JSValue svg_doc_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return GF_JS_EXCEPTION(c);
	switch (magic) {
	case 0:/*global*/
		return JS_GetGlobalObject(c);
	}
	return JS_UNDEFINED;
}

static JSValue svg_element_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	switch (magic) {
	case 0: /*id*/
	{
		const char *node_name = gf_node_get_name((GF_Node*)n);
		if (node_name) {
			return JS_NewString(c, node_name);
		}
		return JS_NULL;
	}
	case 5:/*currentScale*/
		if (n->sgprivate->tag!=TAG_SVG_svg) return GF_JS_EXCEPTION(c);
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_SCALE, (GF_Node *)n, &par)) {
			return JS_NewFloat64(c, FIX2FLT(par.val) );
		}
		return GF_JS_EXCEPTION(c);
	case 6:/*currentRotate*/
		if (n->sgprivate->tag!=TAG_SVG_svg) return GF_JS_EXCEPTION(c);
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_ROTATION, (GF_Node *)n, &par)) {
			return JS_NewFloat64(c, FIX2FLT(par.val) );
		}
		return GF_JS_EXCEPTION(c);
	case 7:/*currentTranslate*/
		if (n->sgprivate->tag!=TAG_SVG_svg) return GF_JS_EXCEPTION(c);
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_TRANSLATE, (GF_Node *)n, &par)) {
			JSValue r = JS_NewObjectClass(c, pointClass.class_id);
			pointCI *rc = (pointCI *)gf_malloc(sizeof(pointCI));
			rc->x = FIX2FLT(par.pt.x);
			rc->y = FIX2FLT(par.pt.y);
			rc->sg = n->sgprivate->scenegraph;
			JS_SetOpaque(r, rc);
			return r;
		}
		return GF_JS_EXCEPTION(c);
	case 8:/*viewport*/
		if (n->sgprivate->tag!=TAG_SVG_svg) return GF_JS_EXCEPTION(c);
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_VIEWPORT, (GF_Node *)n, &par)) {
			JSValue r = JS_NewObjectClass(c, rectClass.class_id);
			rectCI *rc = (rectCI *)gf_malloc(sizeof(rectCI));
			rc->x = FIX2FLT(par.rc.x);
			rc->y = FIX2FLT(par.rc.y);
			rc->w = FIX2FLT(par.rc.width);
			rc->h = FIX2FLT(par.rc.height);
			rc->sg = n->sgprivate->scenegraph;
			JS_SetOpaque(r, rc);
			return r;
		}
		return GF_JS_EXCEPTION(c);
	case 9:/*currentTime*/
		return JS_NewFloat64(c, gf_node_get_scene_time((GF_Node *)n) );
	case 10:/*isPaused*/
		return JS_FALSE;
	case 11:/*ownerSVGElement*/
		while (1) {
			GF_Node *n_par = gf_node_get_parent(n, 0);
			if (!n_par) return JS_TRUE;
			if (n_par->sgprivate->tag==TAG_SVG_svg) {
				return dom_element_construct(c, n_par);
			}
			n = n_par;
		}
		return JS_NULL;
	case 12:/*correspondingElement*/
		/*if we can find a corresponding element for this node, then this is an SVGElementInstance*/
		if (get_corresponding_use(n)) {
			return dom_element_construct(c, n);
		} else {
			return dom_element_construct(c, NULL);
		}
		break;
	case 13:/*correspondingUseElement*/
		return dom_element_construct(c, get_corresponding_use(n));
	default:
		break;
	}
	return JS_UNDEFINED;
}

static JSValue svg_element_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	GF_JSAPIParam par;
	Double d;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case 0:/*id*/
		if (JSVAL_CHECK_STRING(value)) {
			const char *id = JS_ToCString(c, value);
			if (id) {
				GF_FieldInfo info;
				u32 nid = gf_node_get_id(n);
				if (!nid) nid = gf_sg_get_next_available_node_id(n->sgprivate->scenegraph);
				gf_node_set_id(n, nid, id);
				if (gf_node_get_attribute_by_tag(n, TAG_XML_ATT_id, GF_TRUE, GF_FALSE, &info)==GF_OK) {
					if (*(DOM_String *)info.far_ptr) gf_free(*(DOM_String *)info.far_ptr);
					*(DOM_String *)info.far_ptr = gf_strdup(id);
				}
				if (gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_id, GF_TRUE, GF_FALSE, &info)==GF_OK) {
					if (*(DOM_String *)info.far_ptr) gf_free(*(DOM_String *)info.far_ptr);
					*(DOM_String *)info.far_ptr = gf_strdup(id);
				}
				JS_FreeCString(c, id);
			}
		}
		return JS_TRUE;
		/*currentScale*/
	case 5:
		if ((n->sgprivate->tag!=TAG_SVG_svg) || JS_ToFloat64(c, &d, value))
			return GF_JS_EXCEPTION(c);

		par.val = FLT2FIX(d);
		if (!par.val) {
			return js_throw_err(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
		}
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_SCALE, (GF_Node *)n, &par)) {
			return JS_TRUE;
		}
		return JS_FALSE;
		/*currentRotate*/
	case 6:
		if ((n->sgprivate->tag!=TAG_SVG_svg) || JS_ToFloat64(c, &d, value))
			return GF_JS_EXCEPTION(c);

		par.val = FLT2FIX(d);
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_ROTATION, (GF_Node *)n, &par)) {
			return JS_TRUE;
		}
		return JS_FALSE;
		/*currentTime*/
	case 9:
		if ((n->sgprivate->tag!=TAG_SVG_svg) || JS_ToFloat64(c, &d, value))
			return GF_JS_EXCEPTION(c);

		par.time = d;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_TIME, (GF_Node *)n, &par)) {
			return JS_TRUE;
		}
		return JS_FALSE;
	default:
		break;
	}
	return JS_UNDEFINED;
}

static GF_Node *svg_udom_smil_check_instance(JSContext *c, JSValue obj)
{
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return NULL;
	switch (n->sgprivate->tag) {
	case TAG_SVG_animate:
	case TAG_SVG_animateColor:
	case TAG_SVG_animateMotion:
	case TAG_SVG_animateTransform:
	case TAG_SVG_animation:
	case TAG_SVG_audio:
	case TAG_SVG_video:
	case TAG_SVG_set:
	case TAG_LSR_updates:
	/*not sure about this one...*/
	case TAG_SVG_discard:
		return n;
	}
	return NULL;
}


/*TODO*/
static JSValue svg_udom_smil_time_insert(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool is_end)
{
	GF_FieldInfo info;
	u32 i, count;
	Double offset;
	GF_List *times;
	SMIL_Time *newtime;

	GF_Node *n = svg_udom_smil_check_instance(c, obj);
	if (!n) return JS_UNDEFINED;

	if (is_end) {
		info.far_ptr = ((SVGTimedAnimBaseElement *)n)->timingp->end;
	} else {
		info.far_ptr = ((SVGTimedAnimBaseElement *)n)->timingp->begin;
	}
	if (!info.far_ptr) {
		return GF_JS_EXCEPTION(c);
	}
	times = *((GF_List **)info.far_ptr);
	GF_SAFEALLOC(newtime, SMIL_Time);
	if (!newtime) {
		return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);
	}
	newtime->type = GF_SMIL_TIME_EVENT_RESOLVED;

	offset = 0;
	if (argc)
		JS_ToFloat64(c, &offset, argv[0]);

	newtime->clock = gf_node_get_scene_time(n) + offset;

	/*insert in sorted order*/
	count = gf_list_count(times);
	for (i=0; i<count; i++) {
		SMIL_Time*t = (SMIL_Time*)gf_list_get(times, i);
		if ( GF_SMIL_TIME_IS_CLOCK(t->type) ) {
			if (t->clock > newtime->clock) break;
		} else {
			break;
		}
	}
	gf_list_insert(times, newtime, i);

	info.fieldType = SMIL_Times_datatype;
	gf_node_changed(n, &info);
	return JS_TRUE;
}

static JSValue svg_udom_smil_begin(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return svg_udom_smil_time_insert(c, obj, argc, argv, GF_FALSE);
}
static JSValue svg_udom_smil_end(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return svg_udom_smil_time_insert(c, obj, argc, argv, GF_TRUE);
}

/*TODO*/
static JSValue svg_udom_smil_pause(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 tag;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	tag = gf_node_get_tag(n);
	if (gf_svg_is_animation_tag(tag)) {
		/* pausing an animation element (set, animate ...) should pause the main time line ? */
		gf_smil_timing_pause(n);
	} else if (gf_svg_is_timing_tag(tag)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_PAUSE_SVG, n, NULL);
	} else if ((tag==TAG_SVG_svg) && (n->sgprivate->scenegraph->RootNode==n)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_PAUSE_SVG, n, NULL);
	} else {
		return JS_FALSE;
	}
	return JS_TRUE;
}
/*TODO*/
static JSValue svg_udom_smil_resume(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 tag;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	tag = gf_node_get_tag(n);
	if (gf_svg_is_animation_tag(tag)) {
		/* resuming an animation element (set, animate ...) should resume the main time line ? */
		gf_smil_timing_resume(n);
	} else if (gf_svg_is_timing_tag(tag)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_RESUME_SVG, n, NULL);
	} else if ((tag==TAG_SVG_svg) && (n->sgprivate->scenegraph->RootNode==n)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_RESUME_SVG, n, NULL);
	} else {
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSValue svg_udom_smil_restart(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 tag;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	tag = gf_node_get_tag(n);
	if ((tag==TAG_SVG_svg) && (n->sgprivate->scenegraph->RootNode==n)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_RESTART_SVG, n, NULL);
	} else {
		return JS_FALSE;
	}
	return JS_TRUE;
}

static JSValue svg_udom_smil_set_speed(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 tag;
	Double speed = 1.0;
	GF_Node *n = dom_get_element(c, obj);

	if (!n || !argc || JS_ToFloat64(c, &speed, argv[0]) ) {
		return GF_JS_EXCEPTION(c);
	}
	tag = gf_node_get_tag(n);
	if ((tag==TAG_SVG_svg) && (n->sgprivate->scenegraph->RootNode==n)) {
		GF_JSAPIParam par;
		memset(&par, 0, sizeof(GF_JSAPIParam));
		par.val = FLT2FIX(speed);
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_SCENE_SPEED, n, &par);
	} else {
		return JS_TRUE;
	}
	return JS_UNDEFINED;
}

static JSValue svg_udom_get_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	char *attValue;
	const char *name;
	GF_Err e;
	JSValue ret;
	GF_FieldInfo info;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	//ns = NULL;
	name = NULL;
	if (! JSVAL_CHECK_STRING(argv[0]) ) return JS_TRUE;
	if (argc==2) {
		//ns = JS_ToCString(c, argv[0]);
		name = JS_ToCString(c, argv[1]);
	} else if (argc==1) {
		name = JS_ToCString(c, argv[0]);
	} else return GF_JS_EXCEPTION(c);

	if (!name) {
		//JS_FreeCString(c, ns);
		return GF_JS_EXCEPTION(c);
	}
	if (!strcmp(name, "#text")) {
		char *res = gf_dom_flatten_textContent(n);
		ret = JS_NewString(c, res);
		gf_free(res);
		//JS_FreeCString(c, ns);
		JS_FreeCString(c, name);
		return ret;
	}
	e = gf_node_get_field_by_name(n, (char *) name, &info);

	//JS_FreeCString(c, ns);
	JS_FreeCString(c, name);

	if (e!=GF_OK) return GF_JS_EXCEPTION(c);

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_FontSize_datatype:
	case SVG_Color_datatype:
	case SVG_Paint_datatype:
	/* inheritable float and unit */
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	/*Number*/
	case SVG_Number_datatype:

	/*all string traits*/
	case SVG_Boolean_datatype:
	case SVG_FillRule_datatype:
	case SVG_StrokeLineJoin_datatype:
	case SVG_StrokeLineCap_datatype:
	case SVG_FontStyle_datatype:
	case SVG_FontWeight_datatype:
	case SVG_FontVariant_datatype:
	case SVG_TextAnchor_datatype:
	case SVG_Display_datatype:
	case SVG_Visibility_datatype:
	case SVG_GradientUnit_datatype:
	case SVG_PreserveAspectRatio_datatype:
	case XML_Space_datatype:
	case XMLEV_Propagate_datatype:
	case XMLEV_DefaultAction_datatype:
	case XMLEV_Phase_datatype:
	case SMIL_SyncBehavior_datatype:
	case SMIL_SyncTolerance_datatype:
	case SMIL_AttributeType_datatype:
	case SMIL_CalcMode_datatype:
	case SMIL_Additive_datatype:
	case SMIL_Accumulate_datatype:
	case SMIL_Restart_datatype:
	case SMIL_Fill_datatype:
	case SVG_Overflow_datatype:
	case SVG_ZoomAndPan_datatype:
	case SVG_DisplayAlign_datatype:
	case SVG_TextAlign_datatype:
	case SVG_PointerEvents_datatype:
	case SVG_RenderingHint_datatype:
	case SVG_VectorEffect_datatype:
	case SVG_PlaybackOrder_datatype:
	case SVG_TimelineBegin_datatype:
	/*end of string traits*/
	/*DOM string traits*/
	case SVG_FontFamily_datatype:
	case XMLRI_datatype:
	case DOM_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
	case SVG_Focus_datatype:
	case SVG_ClipPath_datatype:
	case SVG_ID_datatype:
	case SVG_GradientOffset_datatype:
		/*end of DOM string traits*/
		attValue = gf_svg_dump_attribute(n, &info);
		ret = JS_NewString(c, attValue);
		if (attValue) gf_free(attValue);
		return ret;

#if 0
	/*SVGT 1.2 default traits*/
	case SMIL_KeyTimes_datatype:
	case SMIL_KeyPoints_datatype:
	case SMIL_KeySplines_datatype:
	case SVG_Coordinates_datatype:
	case SVG_StrokeDashArray_datatype:
	case SVG_Points_datatype:
	case SVG_Motion_datatype:
	/*end SVGT 1.2 default traits*/

	/*unimplemented/unnkown/FIXME traits*/
	case SMIL_SyncTolerance_datatype:
	case SVG_TransformType_datatype:
	case SVG_TransformList_datatype:
	case SMIL_AnimateValue_datatype:
	case SMIL_AnimateValues_datatype:
	case SMIL_AttributeName_datatype:
	case SMIL_Times_datatype:
	case SMIL_Duration_datatype:
	case SMIL_RepeatCount_datatype:
	default:
		/*end unimplemented/unnkown/FIXME traits*/
		return GF_JS_EXCEPTION(c);
#endif
	}
	return JS_NULL;
}

static JSValue svg_udom_get_float_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *szName;
	GF_FieldInfo info;
	GF_Err e;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if ((argc!=1) || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	szName = JS_ToCString(c, argv[0]);
	if (!szName) return GF_JS_EXCEPTION(c);

	e = gf_node_get_attribute_by_name(n, (char *) szName, 0, GF_TRUE, GF_TRUE, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_Number_datatype:
	case SVG_FontSize_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	{
		SVG_Number *l = (SVG_Number *)info.far_ptr;
		if (l->type==SVG_NUMBER_AUTO || l->type==SVG_NUMBER_INHERIT) return JS_TRUE;
		return JS_NewFloat64(c, FIX2FLT(l->value));
	}
	case SVG_Clock_datatype:
		return JS_NewFloat64(c, *(SVG_Clock*)info.far_ptr );
	default:
		return JS_NULL;
	}
	return JS_NULL;
}

static JSValue svg_udom_get_matrix_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *szName;
	GF_FieldInfo info;
	GF_Err e;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if ((argc!=1) || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	szName = JS_ToCString(c, argv[0]);

	e = gf_node_get_field_by_name(n, (char *) szName, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	if (info.fieldType==SVG_Transform_datatype) {
		GF_Matrix2D *mx = (GF_Matrix2D *)gf_malloc(sizeof(GF_Matrix2D));
		if (!mx) return GF_JS_EXCEPTION(c);
		JSValue mO = JS_NewObjectClass(c, matrixClass.class_id);
		gf_mx2d_init(*mx);
		gf_mx2d_copy(*mx, ((SVG_Transform*)info.far_ptr)->mat);

		JS_SetOpaque(mO, mx);
		return mO;
	}
	return JS_NULL;
}

static JSValue svg_udom_get_rect_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *szName;
	GF_FieldInfo info;
	GF_Err e;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if ((argc!=1) || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	szName = JS_ToCString(c, argv[0]);

	e = gf_node_get_field_by_name(n, (char *) szName, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	if (info.fieldType==SVG_ViewBox_datatype) {
		JSValue newObj;
		rectCI *rc;
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		GF_SAFEALLOC(rc, rectCI);
		if (!rc) {
			return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);
		}
		newObj = JS_NewObjectClass(c, rectClass.class_id);
		rc->x = FIX2FLT(v->x);
		rc->y = FIX2FLT(v->y);
		rc->w = FIX2FLT(v->width);
		rc->h = FIX2FLT(v->height);
		JS_SetOpaque(newObj, rc);
		return newObj;
	}
	return JS_NULL;
}

static JSValue svg_udom_get_path_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *szName;
	GF_FieldInfo info;
	GF_Err e;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if ((argc!=1) || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	szName = JS_ToCString(c, argv[0]);

	e = gf_node_get_field_by_name(n, (char *) szName, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	if (info.fieldType==SVG_PathData_datatype) {
		return svg_new_path_object(c, (SVG_PathData *)info.far_ptr);
	}
	return JS_NULL;
}

static JSValue svg_udom_get_rgb_color_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *szName;
	GF_FieldInfo info;
	rgbCI *rgb;
	GF_Err e;
	JSValue newObj;

	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if ((argc!=1) || !JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	szName = JS_ToCString(c, argv[0]);

	e = gf_node_get_field_by_name(n, (char *) szName, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	switch (info.fieldType) {
	case SVG_Color_datatype:
	{
		SVG_Color *col = (SVG_Color *)info.far_ptr;
		if (col->type == SVG_COLOR_CURRENTCOLOR) return JS_UNDEFINED;
		if (col->type == SVG_COLOR_INHERIT) return JS_UNDEFINED;

		GF_SAFEALLOC(rgb, rgbCI);
		if (!rgb) {
			return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);
		}
		newObj = JS_NewObjectClass(c, rgbClass.class_id);
		rgb->r = (u8) (255*FIX2FLT(col->red)) ;
		rgb->g = (u8) (255*FIX2FLT(col->green)) ;
		rgb->b = (u8) (255*FIX2FLT(col->blue)) ;
		JS_SetOpaque(newObj, rgb);
		return newObj;
	}
	break;
	case SVG_Paint_datatype:
	{
		SVG_Paint *paint = (SVG_Paint *)info.far_ptr;
		if ((1) || paint->type==SVG_PAINT_COLOR) {
			GF_SAFEALLOC(rgb, rgbCI);
			if (!rgb) {
				return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);
			}
			newObj = JS_NewObjectClass(c, rgbClass.class_id);
			rgb->r = (u8) (255*FIX2FLT(paint->color.red) );
			rgb->g = (u8) (255*FIX2FLT(paint->color.green) );
			rgb->b = (u8) (255*FIX2FLT(paint->color.blue) );
			JS_SetOpaque(newObj, rgb);
			return newObj;
		}
		return JS_TRUE;
	}
	}
	return JS_NULL;
}

static JSValue svg_udom_set_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *ns, *name, *val;
	GF_Err e;
	GF_FieldInfo info;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	val = ns = name = NULL;
	if (!JSVAL_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);
	if (argc==3) {
		if (!JSVAL_CHECK_STRING(argv[1])) return GF_JS_EXCEPTION(c);
		if (!JSVAL_CHECK_STRING(argv[2])) return GF_JS_EXCEPTION(c);
		ns = JS_ToCString(c, argv[0]);
		name = JS_ToCString(c, argv[1]);
		val = JS_ToCString(c, argv[2]);
	} else if (argc==2) {
		name = JS_ToCString(c, argv[0]);
		val = JS_ToCString(c, argv[1]);
	} else {
		return GF_JS_EXCEPTION(c);
	}
	if (!name) {
		JS_FreeCString(c, ns);
		JS_FreeCString(c, val);
		return GF_JS_EXCEPTION(c);
	}
	if (!strcmp(name, "#text")) {
		if (val) dom_node_set_textContent(n, (char *) val);
		JS_FreeCString(c, ns);
		JS_FreeCString(c, name);
		JS_FreeCString(c, val);
		return JS_UNDEFINED;
	}
	e = gf_node_get_field_by_name(n, (char *) name, &info);
	JS_FreeCString(c, ns);
	JS_FreeCString(c, name);

	if (!val || (e!=GF_OK)) {
		JS_FreeCString(c, val);
		return GF_JS_EXCEPTION(c);
	}
	e = gf_svg_parse_attribute(n, &info, (char *) val, 0);
	JS_FreeCString(c, val);

	if (e) return js_throw_err(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	dom_node_changed(n, GF_FALSE, &info);
	return JS_UNDEFINED;
}

static JSValue svg_udom_set_float_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_FieldInfo info;
	Double d;
	GF_Err e;
	const char *szName;

	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);
	if (argc!=2) return GF_JS_EXCEPTION(c);
	if (!JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	if (!JS_IsNumber(argv[1]) || JS_ToFloat64(c, &d, argv[1])) return GF_JS_EXCEPTION(c);

	szName = JS_ToCString(c, argv[0]);
	e = gf_node_get_field_by_name(n, (char *) szName, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_FontSize_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	case SVG_Number_datatype:
	{
		SVG_Number *l = (SVG_Number *)info.far_ptr;
		l->type=SVG_NUMBER_VALUE;
		l->value = FLT2FIX(d);
		break;
	}
	case SVG_Numbers_datatype:
	case SVG_Coordinates_datatype:
	{
		SVG_Number *val;
		SVG_Coordinates *l = (SVG_Coordinates *)info.far_ptr;
		while (gf_list_count(*l)) {
			val = (SVG_Number *)gf_list_get(*l, 0);
			gf_list_rem(*l, 0);
			if (val) gf_free(val);
		}
		GF_SAFEALLOC(val, SVG_Coordinate);
		if (!val) {
			return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);
		}
		val->type=SVG_NUMBER_VALUE;
		val->value = FLT2FIX(d);
		gf_list_add(*l, val);
		break;
	}
	default:
		return JS_FALSE;
	}
	dom_node_changed(n, GF_FALSE, &info);
	return JS_TRUE;
}

static JSValue svg_udom_set_matrix_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *szName;
	GF_FieldInfo info;
	GF_Matrix2D *mx;
	GF_Err e;

	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return GF_JS_EXCEPTION(c);
	if (!JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	if (JS_IsNull(argv[1]) || !JS_IsObject(argv[1])) return GF_JS_EXCEPTION(c);

	mx = JS_GetOpaque(argv[1], matrixClass.class_id);
	if (!mx) return GF_JS_EXCEPTION(c);

	szName = JS_ToCString(c, argv[0]);
	e = gf_node_get_field_by_name(n, (char *) szName, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	if (info.fieldType==SVG_Transform_datatype) {
		gf_mx2d_copy(((SVG_Transform*)info.far_ptr)->mat, *mx);
		dom_node_changed(n, GF_FALSE, NULL);
		return JS_TRUE;
	}
	return JS_FALSE;
}

static JSValue svg_udom_set_rect_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *szName;
	GF_FieldInfo info;
	rectCI *rc;
	GF_Err e;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if (argc!=2) return GF_JS_EXCEPTION(c);
	if (!JS_IsString(argv[0])) return JS_TRUE;
	if (JS_IsNull(argv[1]) || !JS_IsObject(argv[1])) return GF_JS_EXCEPTION(c);

	rc = JS_GetOpaque(argv[1], rectClass.class_id);
	if (!rc) return GF_JS_EXCEPTION(c);

	szName = JS_ToCString(c, argv[0]);
	e = gf_node_get_field_by_name(n, (char *) szName, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	if (info.fieldType==SVG_ViewBox_datatype) {
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		v->x = FLT2FIX(rc->x);
		v->y = FLT2FIX(rc->y);
		v->width = FLT2FIX(rc->w);
		v->height = FLT2FIX(rc->h);
		dom_node_changed(n, GF_FALSE, NULL);
		return JS_TRUE;
	}
	return JS_FALSE;
}

static JSValue svg_udom_set_path_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	pathCI *path;
	GF_FieldInfo info;
	const char *szName;
	GF_Err e;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if (argc!=2) return GF_JS_EXCEPTION(c);
	if (!JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	if (JS_IsNull(argv[1]) || !JS_IsObject(argv[1])) return GF_JS_EXCEPTION(c);
	path = JS_GetOpaque( argv[1], pathClass.class_id);
	if (!path) return GF_JS_EXCEPTION(c);

	szName = JS_ToCString(c, argv[0]);
	e = gf_node_get_field_by_name(n, (char *) szName, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	if (info.fieldType==SVG_PathData_datatype) {
#if USE_GF_PATH
#else
		u32 i;
		u32 nb_pts;
		SVG_PathData *d = (SVG_PathData *)info.far_ptr;
		while (gf_list_count(d->commands)) {
			u8 *t = gf_list_get(d->commands, 0);
			gf_list_rem(d->commands, 0);
			gf_free(t);
		}
		while (gf_list_count(d->points)) {
			SVG_Point *t = gf_list_get(d->points, 0);
			gf_list_rem(d->points, 0);
			gf_free(t);
		}
		nb_pts = 0;
		for (i=0; i<path->nb_coms; i++) {
			u8 *t = gf_malloc(sizeof(u8));
			*t = path->tags[i];
			gf_list_add(d->commands, t);
			switch (*t) {
			case 0:
			case 1:
				nb_pts++;
				break;
			case 2:
				nb_pts+=3;
				break;
			case 4:
				nb_pts+=2;
				break;
			}
		}
		for (i=0; i<nb_pts; i++) {
			SVG_Point *t = gf_malloc(sizeof(SVG_Point));
			t->x = FLT2FIX(path->pts[i].x);
			t->y = FLT2FIX(path->pts[i].y);
			gf_list_add(d->points, t);
		}
		dom_node_changed(n, 0, NULL);
		return JS_TRUE;
#endif
	}
	return JS_FALSE;
}

static JSValue svg_udom_set_rgb_color_trait(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_FieldInfo info;
	rgbCI *rgb;
	GF_Err e;
	const char *szName;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if (argc!=2) return GF_JS_EXCEPTION(c);
	if (!JS_IsString(argv[0])) return GF_JS_EXCEPTION(c);
	if (!JS_IsObject(argv[1])) return GF_JS_EXCEPTION(c);
	rgb = JS_GetOpaque(argv[1], rgbClass.class_id);
	if (!rgb) return GF_JS_EXCEPTION(c);

	szName = JS_ToCString(c, argv[0]);
	e = gf_node_get_field_by_name(n, (char *) szName, &info);
	JS_FreeCString(c, szName);
	if (e != GF_OK) return GF_JS_EXCEPTION(c);

	switch (info.fieldType) {
	case SVG_Color_datatype:
	{
		SVG_Color *col = (SVG_Color *)info.far_ptr;
		col->type = SVG_COLOR_RGBCOLOR;
		col->red = FLT2FIX(rgb->r / 255.0f);
		col->green = FLT2FIX(rgb->g / 255.0f);
		col->blue = FLT2FIX(rgb->b / 255.0f);
		dom_node_changed(n, GF_FALSE, &info);
		return JS_TRUE;
	}
	case SVG_Paint_datatype:
	{
		SVG_Paint *paint = (SVG_Paint *)info.far_ptr;
		paint->type = SVG_PAINT_COLOR;
		paint->color.type = SVG_COLOR_RGBCOLOR;
		paint->color.red = FLT2FIX(rgb->r / 255.0f);
		paint->color.green = FLT2FIX(rgb->g / 255.0f);
		paint->color.blue = FLT2FIX(rgb->b / 255.0f);
		dom_node_changed(n, GF_FALSE, &info);
		return JS_TRUE;
	}
	}
	return JS_FALSE;
}

static JSValue svg_get_bbox(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, Bool get_screen)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return GF_JS_EXCEPTION(c);

	par.bbox.is_set = GF_FALSE;
	if (ScriptAction(n->sgprivate->scenegraph, get_screen ? GF_JSAPI_OP_GET_SCREEN_BBOX : GF_JSAPI_OP_GET_LOCAL_BBOX, (GF_Node *)n, &par) ) {
		if (par.bbox.is_set) {
			rectCI *rc = (rectCI *)gf_malloc(sizeof(rectCI));
			if (!rc) return GF_JS_EXCEPTION(c);
			JSValue rO = JS_NewObjectClass(c, rectClass.class_id);
			rc->sg = NULL;
			rc->x = FIX2FLT(par.bbox.min_edge.x);
			/*BBox is in 3D coord system style*/
			rc->y = FIX2FLT(par.bbox.min_edge.y);
			rc->w = FIX2FLT(par.bbox.max_edge.x - par.bbox.min_edge.x);
			rc->h = FIX2FLT(par.bbox.max_edge.y - par.bbox.min_edge.y);
			JS_SetOpaque(rO, rc);
			return rO;
		} else {
			return JS_NULL;
		}
		return JS_TRUE;
	}
	return JS_FALSE;
}

static JSValue svg_udom_get_local_bbox(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return svg_get_bbox(c, obj, argc, argv, GF_FALSE);
}
static JSValue svg_udom_get_screen_bbox(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return svg_get_bbox(c, obj, argc, argv, GF_TRUE);
}

static JSValue svg_udom_get_screen_ctm(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return GF_JS_EXCEPTION(c);

	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_TRANSFORM, (GF_Node *)n, &par)) {
		GF_Matrix2D *mx = (GF_Matrix2D *)gf_malloc(sizeof(GF_Matrix2D));
		if (!mx) return GF_JS_EXCEPTION(c);
		JSValue mO = JS_NewObjectClass(c, matrixClass.class_id);
		gf_mx2d_from_mx(mx, &par.mx);
		JS_SetOpaque(mO, mx);
		return mO;
	}
	return GF_JS_EXCEPTION(c);
}

static JSValue svg_udom_create_matrix_components(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Matrix2D *mx;
	JSValue mat;
	Double v;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);
	if (argc!=6) return GF_JS_EXCEPTION(c);

	GF_SAFEALLOC(mx, GF_Matrix2D)
	if (!mx) return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);

	JS_ToFloat64(c, &v, argv[0]);
	mx->m[0] = FLT2FIX(v);
	JS_ToFloat64(c, &v, argv[1]);
	mx->m[3] = FLT2FIX(v);
	JS_ToFloat64(c, &v, argv[2]);
	mx->m[1] = FLT2FIX(v);
	JS_ToFloat64(c, &v, argv[3]);
	mx->m[4] = FLT2FIX(v);
	JS_ToFloat64(c, &v, argv[4]);
	mx->m[2] = FLT2FIX(v);
	JS_ToFloat64(c, &v, argv[5]);
	mx->m[5] = FLT2FIX(v);
	mat = JS_NewObjectClass(c, matrixClass.class_id);
	JS_SetOpaque(mat, mx);
	return mat;
}

static JSValue svg_udom_create_rect(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	rectCI *rc;
	JSValue r;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return GF_JS_EXCEPTION(c);

	GF_SAFEALLOC(rc, rectCI);
	if (!rc) return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);
	r = JS_NewObjectClass(c, rectClass.class_id);
	JS_SetOpaque(r, rc);
	return r;
}

static JSValue svg_udom_create_point(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	pointCI *pt;
	JSValue r;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return GF_JS_EXCEPTION(c);

	GF_SAFEALLOC(pt, pointCI);
	if (!pt) return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);
	r = JS_NewObjectClass(c, pointClass.class_id);
	JS_SetOpaque(r, pt);
	return r;
}

static JSValue svg_udom_create_path(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	pathCI *path;
	JSValue p;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return GF_JS_EXCEPTION(c);

	GF_SAFEALLOC(path, pathCI);
	if (!path) return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);
	p = JS_NewObjectClass(c, pathClass.class_id);
	JS_SetOpaque(p, path);
	return p;
}

static JSValue svg_udom_create_color(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	rgbCI *col;
	JSValue p;
	GF_Node *n = dom_get_element(c, obj);
	if (!n|| (argc!=3)) return GF_JS_EXCEPTION(c);

	GF_SAFEALLOC(col, rgbCI);
	if (!col) return js_throw_err(c, GF_DOM_EXC_DATA_CLONE_ERR);

	JS_ToInt32(c, &col->r, argv[0]);
	JS_ToInt32(c, &col->g, argv[1]);
	JS_ToInt32(c, &col->b, argv[2]);
	p = JS_NewObjectClass(c, rgbClass.class_id);
	JS_SetOpaque(p, col);
	return p;
}

static JSValue svg_path_get_total_length(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double length = 0;
	GF_FieldInfo info;

	GF_Node *n = (GF_Node *)dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);
	if (n->sgprivate->tag != TAG_SVG_path) return GF_JS_EXCEPTION(c);

	gf_node_get_field_by_name(n, "d", &info);
	if (info.fieldType == SVG_PathData_datatype) {
#if USE_GF_PATH
		GF_Path *p = (GF_Path *)info.far_ptr;
		GF_PathIterator *path_it = gf_path_iterator_new(p);
		if (path_it) {
			Fixed len = gf_path_iterator_get_length(path_it);
			length = FIX2FLT(len);
			gf_path_iterator_del(path_it);
		}
#endif
	}
	return JS_NewFloat64(c, length);
}

static JSValue svg_udom_move_focus(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);
	if ((argc!=1) || !JS_IsObject(argv[0])) return GF_JS_EXCEPTION(c);

	JS_ToInt32(c, &par.opt, argv[1]);
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_FOCUS, (GF_Node *)n, &par))
		return JS_TRUE;
	return JS_FALSE;
}

static JSValue svg_udom_set_focus(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);
	if ((argc!=1) || !JS_IsObject(argv[0])) return GF_JS_EXCEPTION(c);

	par.node = dom_get_element(c, argv[0]);
	/*NOT IN THE GRAPH*/
	if (!par.node || !par.node->sgprivate->num_instances) return GF_JS_EXCEPTION(c);
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_FOCUS, (GF_Node *)n, &par))
		return JS_TRUE;
	return JS_TRUE;
}

static JSValue svg_udom_get_focus(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return GF_JS_EXCEPTION(c);

	if (!ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_FOCUS, (GF_Node *)n, &par))
		return GF_JS_EXCEPTION(c);

	if (par.node) {
		return dom_element_construct(c, par.node);
	}
	return JS_NULL;
}

static JSValue svg_udom_get_time(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return GF_JS_EXCEPTION(c);

	return JS_NewFloat64(c, gf_node_get_scene_time(n) );
}


static JSValue svg_connection_create(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return js_throw_err(c, GF_DOM_EXC_NOT_SUPPORTED_ERR);
}

static void baseCI_finalize(JSRuntime *rt, JSValue obj)
{
	/*avoids GCC warning*/
	void *data = JS_GetOpaque_Nocheck(obj);
	if (data) gf_free(data);
}

static JSValue rgb_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	rgbCI *col = (rgbCI *) JS_GetOpaque(obj, rgbClass.class_id);
	if (!col) return GF_JS_EXCEPTION(c);
	switch (magic) {
	case 0: return JS_NewInt32(c, col->r);
	case 1: return JS_NewInt32(c, col->g);
	case 2: return JS_NewInt32(c, col->b);
	default:
		return GF_JS_EXCEPTION(c);
	}
}
static JSValue rgb_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	rgbCI *col = (rgbCI *) JS_GetOpaque(obj, rgbClass.class_id);
	if (!col) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case 0: return JS_ToInt32(c, &col->r, value) ? GF_JS_EXCEPTION(c) : JS_TRUE;
	case 1: return JS_ToInt32(c, &col->g, value) ? GF_JS_EXCEPTION(c) : JS_TRUE;
	case 2: return JS_ToInt32(c, &col->b, value) ? GF_JS_EXCEPTION(c) : JS_TRUE;
	default:
		return GF_JS_EXCEPTION(c);
	}
}

static JSValue rect_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	rectCI *rc = (rectCI *) JS_GetOpaque(obj, rectClass.class_id);
	if (!rc) return GF_JS_EXCEPTION(c);
	if (rc->sg) {
		GF_JSAPIParam par;
		if (!ScriptAction(rc->sg, GF_JSAPI_OP_GET_VIEWPORT, rc->sg->RootNode, &par)) {
			return GF_JS_EXCEPTION(c);
		}
		rc->x = FIX2FLT(par.rc.x);
		rc->y = FIX2FLT(par.rc.y);
		rc->w = FIX2FLT(par.rc.width);
		rc->h = FIX2FLT(par.rc.height);
	}
	switch (magic) {
	case 0: return JS_NewFloat64(c, rc->x);
	case 1: return JS_NewFloat64(c, rc->y);
	case 2: return JS_NewFloat64(c, rc->w);
	case 3: return JS_NewFloat64(c, rc->h);
	default:
		return GF_JS_EXCEPTION(c);
	}
}

static JSValue rect_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	Double d;
	rectCI *rc = (rectCI *) JS_GetOpaque(obj, rectClass.class_id);
	if (!rc) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &d, value)) return GF_JS_EXCEPTION(c);
	switch (magic) {
	case 0:
		rc->x = (Float) d;
		return JS_TRUE;
	case 1:
		rc->y = (Float) d;
		return JS_TRUE;
	case 2:
		rc->w = (Float) d;
		return JS_TRUE;
	case 3:
		rc->h = (Float) d;
		return JS_TRUE;
	default:
		return GF_JS_EXCEPTION(c);
	}
}

static JSValue point_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	pointCI *pt = (pointCI *) JS_GetOpaque(obj, pointClass.class_id);
	if (!pt) return GF_JS_EXCEPTION(c);

	if (pt->sg) {
		GF_JSAPIParam par;
		if (!ScriptAction(pt->sg, GF_JSAPI_OP_GET_TRANSLATE, pt->sg->RootNode, &par)) {
			return GF_JS_EXCEPTION(c);
		}
		pt->x = FIX2FLT(par.pt.x);
		pt->y = FIX2FLT(par.pt.y);
	}
	switch (magic) {
	case 0: return JS_NewFloat64(c, pt->x);
	case 1: return JS_NewFloat64(c, pt->y);
	default: return GF_JS_EXCEPTION(c);
	}
}

static JSValue point_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	pointCI *pt = (pointCI *) JS_GetOpaque(obj, pointClass.class_id);
	if (!pt) return GF_JS_EXCEPTION(c);

	Double d;
	if (JS_ToFloat64(c, &d, value)) return GF_JS_EXCEPTION(c);
	switch (magic) {
	case 0:
		pt->x = (Float) d;
		break;
	case 1:
		pt->y = (Float) d;
		break;
	default:
		return GF_JS_EXCEPTION(c);
	}
	if (pt->sg) {
		GF_JSAPIParam par;
		par.pt.x = FLT2FIX(pt->x);
		par.pt.y = FLT2FIX(pt->y);
		ScriptAction(pt->sg, GF_JSAPI_OP_SET_TRANSLATE, pt->sg->RootNode, &par);
	}
	return JS_UNDEFINED;
}

static JSValue svg_new_path_object(JSContext *c, SVG_PathData *d)
{
#if USE_GF_PATH
	return JS_NULL;
#else
	JSValue obj;
	pathCI *p;
	GF_SAFEALLOC(p, pathCI);
	if (!p) return GF_JS_EXCEPTION(c);
	if (d) {
		u32 i, count;
		p->nb_coms = gf_list_count(d->commands);
		p->tags = gf_malloc(sizeof(u8) * p->nb_coms);
		for (i=0; i<p->nb_coms; i++) p->tags[i] = * (u8 *) gf_list_get(d->commands, i);
		count = gf_list_count(d->points);
		p->pts = gf_malloc(sizeof(pointCI) * count);
		for (i=0; i<count; i++) {
			GF_Point2D *pt = gf_list_get(d->commands, i);
			p->pts[i].x = FIX2FLT(pt->x);
			p->pts[i].y = FIX2FLT(pt->y);
		}
	}
	obj = JS_NewObjectClass(c, pathClass.class_id);
	JS_SetOpaque(obj, p);
	return obj;
#endif
}

static void pathCI_finalize(JSRuntime *rt, JSValue obj)
{
	pathCI *p = (pathCI *) JS_GetOpaque(obj, pathClass.class_id);
	if (p) {
		if (p->pts) gf_free(p->pts);
		if (p->tags) gf_free(p->tags);
		gf_free(p);
	}
}

static JSValue path_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	pathCI *p = (pathCI *) JS_GetOpaque(obj, pathClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	switch (magic) {
	case 0: return JS_NewInt32(c, p->nb_coms);
	default:
		return GF_JS_EXCEPTION(c);
	}
}

static JSValue svg_path_get_segment(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 idx;
	pathCI *p = (pathCI *) JS_GetOpaque(obj, pathClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);
	if ((argc!=1) || JS_ToInt32(c, &idx, argv[0])) return GF_JS_EXCEPTION(c);
	if (idx>=p->nb_coms) return JS_TRUE;
	switch (p->tags[idx]) {
	case 0: return JS_NewInt32(c, 77);/* Move To */
	case 1: return JS_NewInt32(c, 76);/* Line To */
	case 2:/* Curve To */
	case 3:/* next Curve To */
		return JS_NewInt32(c, 67);
	case 4:/* Quad To */
	case 5:/* next Quad To */
		return JS_NewInt32(c, 81);
	case 6:
		return JS_NewInt32(c, 90);/* Close */
	}
	return GF_JS_EXCEPTION(c);
}

static JSValue svg_path_get_segment_param(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 i, idx, param_idx, pt_idx;
	ptCI *pt;
	pathCI *p = (pathCI *) JS_GetOpaque(obj, pathClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);

	if ((argc!=2) || !JS_IsInteger(argv[0]) || !JS_IsInteger(argv[1])) return GF_JS_EXCEPTION(c);
	if (JS_ToInt32(c, &idx, argv[0])) return GF_JS_EXCEPTION(c);
	if (JS_ToInt32(c, &param_idx, argv[1])) return GF_JS_EXCEPTION(c);
	if (idx>=p->nb_coms) return JS_TRUE;
	pt_idx = 0;
	for (i=0; i<idx; i++) {
		switch (p->tags[i]) {
		case 0:
			pt_idx++;
			break;
		case 1:
			pt_idx++;
			break;
		case 2:
			pt_idx+=3;
			break;
		case 3:
			pt_idx+=2;
			break;
		case 4:
			pt_idx+=2;
			break;
		case 5:
			pt_idx+=1;
			break;
		}
	}
	switch (p->tags[idx]) {
	case 0:
	case 1:
		if (param_idx>1) return JS_TRUE;
		pt = &p->pts[pt_idx];
		return JS_NewFloat64(c, param_idx ? pt->y : pt->x);
	case 2:/* Curve To */
		if (param_idx>5) return JS_TRUE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		return JS_NewFloat64(c, (param_idx%2) ? pt->y : pt->x);
	case 3:/* Next Curve To */
		if (param_idx>5) return JS_TRUE;
		if (param_idx<2) {
			pt = &p->pts[pt_idx - 1];
			return JS_NewFloat64(c, param_idx ? pt->y : pt->x);
		}
		param_idx-=2;
		pt = &p->pts[pt_idx + (param_idx/2)];
		return JS_NewFloat64(c, (param_idx%2) ? pt->y : pt->x);

	case 4:/* Quad To */
		if (param_idx>3) return JS_TRUE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		return JS_NewFloat64(c, (param_idx%2) ? pt->y : pt->x);

	case 5:/* Next Quad To */
		if (param_idx>3) return JS_TRUE;
		if (param_idx<2) {
			pt = &p->pts[pt_idx - 1];
			return JS_NewFloat64(c, param_idx ? pt->y : pt->x);
		} else {
			param_idx-=2;
			pt = &p->pts[pt_idx];
			return JS_NewFloat64(c, param_idx ? pt->y : pt->x);
		}
		return JS_TRUE;
	/*spec is quite obscur here*/
	case 6:
		return JS_NewFloat64(c, 0);
	}
	return GF_JS_EXCEPTION(c);
}

static u32 svg_path_realloc_pts(pathCI *p, u32 nb_pts)
{
	u32 i, orig_pts;
	orig_pts = 0;
	for (i=0; i<p->nb_coms; i++) {
		switch (p->tags[i]) {
		case 0:
			orig_pts++;
			break;
		case 1:
			orig_pts++;
			break;
		case 2:
			orig_pts+=3;
			break;
		case 3:
			orig_pts+=2;
			break;
		case 4:
			orig_pts+=2;
			break;
		case 5:
			orig_pts+=1;
			break;
		}
	}
	p->pts = (ptCI *)gf_realloc(p->pts, sizeof(ptCI)*(nb_pts+orig_pts));
	return orig_pts;
}

static JSValue svg_path_move_to(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	pathCI *p;
	Double x, y;
	u32 nb_pts;

	p = (pathCI *) JS_GetOpaque(obj, pathClass.class_id);
	if (!p || (argc!=2)) return GF_JS_EXCEPTION(c);

	if (JS_ToFloat64(c, &x, argv[0])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &y, argv[1])) return GF_JS_EXCEPTION(c);
	nb_pts = svg_path_realloc_pts(p, 1);
	p->pts[nb_pts].x = (Float) x;
	p->pts[nb_pts].y = (Float) y;
	p->tags = (u8 *)gf_realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 0;
	p->nb_coms++;
	return JS_TRUE;
}

static JSValue svg_path_line_to(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	pathCI *p;
	Double x, y;
	u32 nb_pts;
	p = (pathCI *) JS_GetOpaque(obj, pathClass.class_id);
	if (!p || (argc!=2)) return GF_JS_EXCEPTION(c);

	if (JS_ToFloat64(c, &x, argv[0])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &y, argv[1])) return GF_JS_EXCEPTION(c);

	nb_pts = svg_path_realloc_pts(p, 1);
	p->pts[nb_pts].x = (Float) x;
	p->pts[nb_pts].y = (Float) y;
	p->tags = (u8 *)gf_realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 1;
	p->nb_coms++;
	return JS_TRUE;
}

static JSValue svg_path_quad_to(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	pathCI *p;
	Double x1, y1, x2, y2;
	u32 nb_pts;

	p = (pathCI *) JS_GetOpaque(obj, pathClass.class_id);
	if (!p || (argc!=4)) return GF_JS_EXCEPTION(c);

	if (JS_ToFloat64(c, &x1, argv[0])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &y1, argv[1])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &x2, argv[2])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &y2, argv[3])) return GF_JS_EXCEPTION(c);
	nb_pts = svg_path_realloc_pts(p, 2);
	p->pts[nb_pts].x = (Float) x1;
	p->pts[nb_pts].y = (Float) y1;
	p->pts[nb_pts+1].x = (Float) x2;
	p->pts[nb_pts+1].y = (Float) y2;
	p->tags = (u8 *)gf_realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 4;
	p->nb_coms++;
	return JS_TRUE;
}

static JSValue svg_path_curve_to(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	pathCI *p;
	Double x1, y1, x2, y2, x, y;
	u32 nb_pts;

	p = (pathCI *) JS_GetOpaque(obj, pathClass.class_id);
	if (!p || (argc!=6)) return GF_JS_EXCEPTION(c);

	if (JS_ToFloat64(c, &x1, argv[0])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &y1, argv[1])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &x2, argv[2])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &y2, argv[3])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &x, argv[4])) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &y, argv[5])) return GF_JS_EXCEPTION(c);
	nb_pts = svg_path_realloc_pts(p, 3);
	p->pts[nb_pts].x = (Float) x1;
	p->pts[nb_pts].y = (Float) y1;
	p->pts[nb_pts+1].x = (Float) x2;
	p->pts[nb_pts+1].y = (Float) y2;
	p->pts[nb_pts+2].x = (Float) x;
	p->pts[nb_pts+2].y = (Float) y;
	p->tags = (u8 *)gf_realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 2;
	p->nb_coms++;
	return JS_TRUE;
}

static JSValue svg_path_close(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	pathCI *p = (pathCI *) JS_GetOpaque(obj, pathClass.class_id);
	if (!p) return GF_JS_EXCEPTION(c);

	p->tags = (u8 *)gf_realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 6;
	p->nb_coms++;
	return JS_TRUE;
}


static JSValue matrix_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_Matrix2D *mx = (GF_Matrix2D *) JS_GetOpaque(obj, matrixClass.class_id);
	if (!mx) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case 0: return JS_NewFloat64(c, FIX2FLT(mx->m[0]));
	case 1: return JS_NewFloat64(c, FIX2FLT(mx->m[3]));
	case 2: return JS_NewFloat64(c, FIX2FLT(mx->m[1]));
	case 3: return JS_NewFloat64(c, FIX2FLT(mx->m[4]));
	case 4: return JS_NewFloat64(c, FIX2FLT(mx->m[2]));
	case 5: return JS_NewFloat64(c, FIX2FLT(mx->m[5]));
	default:
		return GF_JS_EXCEPTION(c);
	}
}

static JSValue matrix_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	Double d;
	GF_Matrix2D *mx = (GF_Matrix2D *) JS_GetOpaque(obj, matrixClass.class_id);
	if (!mx) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &d, value)) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case 0:
		mx->m[0] = FLT2FIX(d);
		return JS_UNDEFINED;
	case 1:
		mx->m[3] = FLT2FIX(d);
		return JS_UNDEFINED;
	case 2:
		mx->m[1] = FLT2FIX(d);
		return JS_UNDEFINED;
	case 3:
		mx->m[4] = FLT2FIX(d);
		return JS_UNDEFINED;
	case 4:
		mx->m[2] = FLT2FIX(d);
		return JS_UNDEFINED;
	case 5:
		mx->m[5] = FLT2FIX(d);
		return JS_UNDEFINED;
	default:
		break;
	}
	return GF_JS_EXCEPTION(c);
}

static JSValue svg_mx2d_get_component(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 comp;
	GF_Matrix2D *mx = (GF_Matrix2D *) JS_GetOpaque(obj, matrixClass.class_id);
	if (!mx || (argc!=1)) return GF_JS_EXCEPTION(c);
	if (JS_ToInt32(c, &comp, argv[0])) return GF_JS_EXCEPTION(c);

	switch (comp) {
	case 0: return JS_NewFloat64(c, FIX2FLT(mx->m[0]));
	case 1: return JS_NewFloat64(c, FIX2FLT(mx->m[3]));
	case 2: return JS_NewFloat64(c, FIX2FLT(mx->m[1]));
	case 3: return JS_NewFloat64(c, FIX2FLT(mx->m[4]));
	case 4: return JS_NewFloat64(c, FIX2FLT(mx->m[2]));
	case 5: return JS_NewFloat64(c, FIX2FLT(mx->m[5]));
	}
	return GF_JS_EXCEPTION(c);
}

static JSValue svg_mx2d_multiply(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Matrix2D *mx1, *mx2;
	mx1 = (GF_Matrix2D *) JS_GetOpaque(obj, matrixClass.class_id);
	if (!mx1 || (argc!=1)) return GF_JS_EXCEPTION(c);
	mx2 = (GF_Matrix2D *) JS_GetOpaque(argv[0], matrixClass.class_id);
	if (!mx2) return GF_JS_EXCEPTION(c);

	gf_mx2d_add_matrix(mx1, mx2);
	return JS_DupValue(c, obj);
}

static JSValue svg_mx2d_inverse(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Matrix2D *mx = (GF_Matrix2D *) JS_GetOpaque(obj, matrixClass.class_id);
	if (!mx) return GF_JS_EXCEPTION(c);
	gf_mx2d_inverse(mx);
	return JS_DupValue(c, obj);
}

static JSValue svg_mx2d_translate(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double x, y;
	GF_Matrix2D *mx1, mx2;
	mx1 = (GF_Matrix2D *) JS_GetOpaque(obj, matrixClass.class_id);
	if (!mx1 || (argc!=2)) return GF_JS_EXCEPTION(c);

	if (JS_ToFloat64(c, &x, argv[0]) || JS_ToFloat64(c, &y, argv[1])) return GF_JS_EXCEPTION(c);

	gf_mx2d_init(mx2);
	mx2.m[2] = FLT2FIX(x);
	mx2.m[5] = FLT2FIX(y);
	gf_mx2d_pre_multiply(mx1, &mx2);
	return JS_DupValue(c, obj);
}

static JSValue svg_mx2d_scale(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double scale;
	GF_Matrix2D *mx1, mx2;
	mx1 = (GF_Matrix2D *) JS_GetOpaque(obj, matrixClass.class_id);
	if (!mx1 || (argc!=2)) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &scale, argv[0])) return GF_JS_EXCEPTION(c);

	gf_mx2d_init(mx2);
	mx2.m[0] = mx2.m[4] = FLT2FIX(scale);
	gf_mx2d_pre_multiply(mx1, &mx2);
	return JS_DupValue(c, obj);
}

static JSValue svg_mx2d_rotate(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Double angle;
	GF_Matrix2D *mx1, mx2;

	mx1 = (GF_Matrix2D *) JS_GetOpaque(obj, matrixClass.class_id);
	if (!mx1 || (argc!=1)) return GF_JS_EXCEPTION(c);
	if (JS_ToFloat64(c, &angle, argv[0])) return GF_JS_EXCEPTION(c);

	gf_mx2d_init(mx2);
	gf_mx2d_add_rotation(&mx2, 0, 0, gf_mulfix(FLT2FIX(angle/180), GF_PI));
	gf_mx2d_pre_multiply(mx1, &mx2);
	return JS_DupValue(c, obj);
}


#ifdef GPAC_ENABLE_HTML5_MEDIA
void *html_get_element_class(GF_Node *n);
#endif

JSClassID svg_get_element_class(GF_Node *n)
{
	if (!n) return 0;
	if ((n->sgprivate->tag>=GF_NODE_RANGE_FIRST_SVG) && (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG)) {
#ifdef GPAC_ENABLE_HTML5_MEDIA
		if (n->sgprivate->tag == TAG_SVG_video || n->sgprivate->tag == TAG_SVG_audio) {
			assert(0);
			return html_get_element_class(n);
		}
#endif
		return svgElement.class_id;
	}
	return 0;
}
JSClassID svg_get_document_class(GF_SceneGraph *sg)
{
	GF_Node *n = sg->RootNode;
	if (!n) return 0;
	if ((n->sgprivate->tag>=GF_NODE_RANGE_FIRST_SVG) && (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG))
		return svgDocument.class_id;
	return 0;
}

Bool is_svg_document_class(JSContext *c, JSValue obj)
{
	void *ptr = JS_GetOpaque(obj, svgDocument.class_id);
	if (ptr) return GF_TRUE;
	return GF_FALSE;
}

Bool is_svg_element_class(JSContext *c, JSValue obj)
{
	void *ptr = JS_GetOpaque(obj, svgElement.class_id);
	if (ptr) return GF_TRUE;
	return GF_FALSE;
}

#define SETUP_JSCLASS(_class, _name, _proto_funcs, _construct, _finalize, _proto_class_id) \
	if (!_class.class_id) {\
		JS_NewClassID(&(_class.class_id)); \
		_class.class.class_name = _name; \
		_class.class.finalizer = _finalize;\
		JS_NewClass(jsrt, _class.class_id, &(_class.class));\
	}\
	scene->svg_js->_class.proto = JS_NewObjectClass(c, _proto_class_id ? _proto_class_id : _class.class_id);\
    	JS_SetPropertyFunctionList(c, scene->svg_js->_class.proto, _proto_funcs, countof(_proto_funcs));\
    	JS_SetClassProto(c, _class.class_id, scene->svg_js->_class.proto);\
    	if (_construct) {\
		scene->svg_js->_class.ctor = JS_NewCFunction2(c, _construct, _name, 1, JS_CFUNC_constructor, 0);\
    		JS_SetPropertyStr(c, global, _name, scene->svg_js->_class.ctor);\
	}\


static const JSCFunctionListEntry globalFuncs[] =
{
    JS_CGETSET_MAGIC_DEF("connected", global_getProperty, NULL, 0),
    JS_CGETSET_MAGIC_DEF("parent", global_getProperty, NULL, 1),
	JS_CFUNC_DEF("createConnection", 0, svg_connection_create),
	JS_CFUNC_DEF("gotoLocation", 1, svg_nav_to_location),
	JS_CFUNC_DEF("alert", 0, js_print),
	JS_CFUNC_DEF("print", 0, js_print),
	JS_CFUNC_DEF("hasFeature", 2, dom_imp_has_feature),
	JS_CFUNC_DEF("parseXML", 0, svg_parse_xml),
};

static const JSCFunctionListEntry documentFuncs[] =
{
	JS_CGETSET_MAGIC_DEF("defaultView",	svg_doc_getProperty, NULL, 0),
};

static const JSCFunctionListEntry svg_elementFuncs[] =
{
	JS_CGETSET_MAGIC_DEF("id",	svg_element_getProperty, svg_element_setProperty, 0),
	/*svgSVGElement interface*/
	JS_CGETSET_MAGIC_DEF("currentScale",	svg_element_getProperty, svg_element_setProperty, 5),
	JS_CGETSET_MAGIC_DEF("currentRotate",	svg_element_getProperty, svg_element_setProperty, 6),
	JS_CGETSET_MAGIC_DEF("currentTranslate",	svg_element_getProperty, svg_element_setProperty, 7),
	JS_CGETSET_MAGIC_DEF("viewport",	svg_element_getProperty, NULL, 8),
	JS_CGETSET_MAGIC_DEF("currentTime",	svg_element_getProperty, svg_element_setProperty, 9),
	/*timeControl interface*/
	JS_CGETSET_MAGIC_DEF("isPaused",	svg_element_getProperty, NULL, 10),
	/*old SVG1.1 stuff*/
	JS_CGETSET_MAGIC_DEF("ownerSVGElement",	svg_element_getProperty, NULL, 11),
	/*SVGElementInstance*/
	JS_CGETSET_MAGIC_DEF("correspondingElement",	svg_element_getProperty, NULL, 12),
	JS_CGETSET_MAGIC_DEF("correspondingUseElement",	svg_element_getProperty, NULL, 13),

	/*trait access interface*/
	JS_CFUNC_DEF("getTrait", 1, svg_udom_get_trait),
	JS_CFUNC_DEF("getTraitNS", 2, svg_udom_get_trait),
	JS_CFUNC_DEF("getFloatTrait", 1, svg_udom_get_float_trait),
	JS_CFUNC_DEF("getMatrixTrait", 1, svg_udom_get_matrix_trait),
	JS_CFUNC_DEF("getRectTrait", 1, svg_udom_get_rect_trait),
	JS_CFUNC_DEF("getPathTrait", 1, svg_udom_get_path_trait),
	JS_CFUNC_DEF("getRGBColorTrait", 1, svg_udom_get_rgb_color_trait),
	/*FALLBACK TO BASE-VALUE FOR NOW - WILL NEED EITHER DOM TREE-CLONING OR A FAKE RENDER
	PASS FOR EACH PRESENTATION VALUE ACCESS*/
	JS_CFUNC_DEF("getPresentationTrait", 1, svg_udom_get_trait),
	JS_CFUNC_DEF("getPresentationTraitNS", 2, svg_udom_get_trait),
	JS_CFUNC_DEF("getFloatPresentationTrait", 1, svg_udom_get_float_trait),
	JS_CFUNC_DEF("getMatrixPresentationTrait", 1, svg_udom_get_matrix_trait),
	JS_CFUNC_DEF("getRectPresentationTrait", 1, svg_udom_get_rect_trait),
	JS_CFUNC_DEF("getPathPresentationTrait", 1, svg_udom_get_path_trait),
	JS_CFUNC_DEF("getRGBColorPresentationTrait", 1, svg_udom_get_rgb_color_trait),
	JS_CFUNC_DEF("setTrait", 2, svg_udom_set_trait),
	JS_CFUNC_DEF("setTraitNS", 3, svg_udom_set_trait),
	JS_CFUNC_DEF("setFloatTrait", 2, svg_udom_set_float_trait),
	JS_CFUNC_DEF("setMatrixTrait", 2, svg_udom_set_matrix_trait),
	JS_CFUNC_DEF("setRectTrait", 2, svg_udom_set_rect_trait),
	JS_CFUNC_DEF("setPathTrait", 2, svg_udom_set_path_trait),
	JS_CFUNC_DEF("setRGBColorTrait", 2, svg_udom_set_rgb_color_trait),
	/*locatable interface*/
	JS_CFUNC_DEF("getBBox", 0, svg_udom_get_local_bbox),
	JS_CFUNC_DEF("getScreenCTM", 0, svg_udom_get_screen_ctm),
	JS_CFUNC_DEF("getScreenBBox", 0, svg_udom_get_screen_bbox),
	/*svgSVGElement interface*/
	JS_CFUNC_DEF("createSVGMatrixComponents", 0, svg_udom_create_matrix_components),
	JS_CFUNC_DEF("createSVGRect", 0, svg_udom_create_rect),
	JS_CFUNC_DEF("createSVGPath", 0, svg_udom_create_path),
	JS_CFUNC_DEF("createSVGRGBColor", 0, svg_udom_create_color),
	JS_CFUNC_DEF("createSVGPoint", 0, svg_udom_create_point),
	JS_CFUNC_DEF("moveFocus", 0, svg_udom_move_focus),
	JS_CFUNC_DEF("setFocus", 0, svg_udom_set_focus),
	JS_CFUNC_DEF("getCurrentFocusedObject", 0, svg_udom_get_focus),
	JS_CFUNC_DEF("getCurrentTime", 0, svg_udom_get_time),

	/*timeControl interface*/
	JS_CFUNC_DEF("beginElementAt", 1, svg_udom_smil_begin),
	JS_CFUNC_DEF("beginElement", 0, svg_udom_smil_begin),
	JS_CFUNC_DEF("endElementAt", 1, svg_udom_smil_end),
	JS_CFUNC_DEF("endElement", 0, svg_udom_smil_end),
	JS_CFUNC_DEF("pauseElement", 0, svg_udom_smil_pause),
	JS_CFUNC_DEF("resumeElement", 0, svg_udom_smil_resume),
	JS_CFUNC_DEF("restartElement", 0, svg_udom_smil_restart),
	JS_CFUNC_DEF("setSpeed", 0, svg_udom_smil_set_speed),
	JS_CFUNC_DEF("getTotalLength", 0, svg_path_get_total_length)
};

/*RGBColor class*/
static const JSCFunctionListEntry rgb_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("red", rgb_getProperty, rgb_setProperty, 0),
	JS_CGETSET_MAGIC_DEF("green", rgb_getProperty, rgb_setProperty, 1),
	JS_CGETSET_MAGIC_DEF("blue", rgb_getProperty, rgb_setProperty, 2),
};

	/*SVGRect class*/
static const JSCFunctionListEntry rect_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("x", rect_getProperty, rect_setProperty, 0),
	JS_CGETSET_MAGIC_DEF("y", rect_getProperty, rect_setProperty, 1),
	JS_CGETSET_MAGIC_DEF("width", rect_getProperty, rect_setProperty, 2),
	JS_CGETSET_MAGIC_DEF("height", rect_getProperty, rect_setProperty, 3),
};

/*SVGPoint class*/
static const JSCFunctionListEntry point_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("x", point_getProperty, point_setProperty, 0),
	JS_CGETSET_MAGIC_DEF("y", point_getProperty, point_setProperty, 1)
};

/*SVGMatrix class*/
static const JSCFunctionListEntry matrix_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("a", matrix_getProperty, matrix_setProperty, 0),
	JS_CGETSET_MAGIC_DEF("b", matrix_getProperty, matrix_setProperty, 1),
	JS_CGETSET_MAGIC_DEF("c", matrix_getProperty, matrix_setProperty, 2),
	JS_CGETSET_MAGIC_DEF("d", matrix_getProperty, matrix_setProperty, 3),
	JS_CGETSET_MAGIC_DEF("e", matrix_getProperty, matrix_setProperty, 4),
	JS_CGETSET_MAGIC_DEF("f", matrix_getProperty, matrix_setProperty, 5),

	JS_CFUNC_DEF("getComponent", 1, svg_mx2d_get_component),
	JS_CFUNC_DEF("mMultiply", 1, svg_mx2d_multiply),
	JS_CFUNC_DEF("inverse", 0, svg_mx2d_inverse),
	JS_CFUNC_DEF("mTranslate", 2, svg_mx2d_translate),
	JS_CFUNC_DEF("mScale", 1, svg_mx2d_scale),
	JS_CFUNC_DEF("mRotate", 1, svg_mx2d_rotate),
};

/*SVGPath class*/
static const JSCFunctionListEntry path_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("numberOfSegments", path_getProperty, NULL, 0),
	JS_CFUNC_DEF("getSegment", 1, svg_path_get_segment),
	JS_CFUNC_DEF("getSegmentParam", 2, svg_path_get_segment_param),
	JS_CFUNC_DEF("moveTo", 2, svg_path_move_to),
	JS_CFUNC_DEF("lineTo", 2, svg_path_line_to),
	JS_CFUNC_DEF("quadTo", 4, svg_path_quad_to),
	JS_CFUNC_DEF("curveTo", 6, svg_path_curve_to),
	JS_CFUNC_DEF("close", 0, svg_path_close),
};

JSClassID dom_js_get_element_proto(struct JSContext *c);
JSClassID dom_js_get_document_proto(JSContext *c);

/*defines a new global object "document" of type Document*/
void dom_js_define_document(struct JSContext *c, JSValue global, GF_SceneGraph *doc);

void domDocument_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func);
void domElement_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func);

static void svg_init_js_api(GF_SceneGraph *scene)
{
	JSContext *c = scene->svg_js->js_ctx;
	JSRuntime *jsrt = JS_GetRuntime(c);
	JSValue global = JS_GetGlobalObject(scene->svg_js->js_ctx);

	SETUP_JSCLASS(svg_globalClass, "Window", globalFuncs, NULL, NULL, 0);
	scene->svg_js->global = JS_NewObjectClass(c, svg_globalClass.class_id);
	JS_SetOpaque(scene->svg_js->global, scene);
	JS_SetPropertyStr(c, global, "Window", scene->svg_js->global);

 	JS_SetPropertyStr(c, global, "alert", JS_NewCFunction(c, js_print, "alert", 1));
 	
	/*initialize DOM core */
	dom_js_load(scene, scene->svg_js->js_ctx);

#ifdef GPAC_USE_DOWNLOADER
	qjs_module_init_xhr_global(c, global);
#endif

	svg_define_udom_exception(scene->svg_js->js_ctx, scene->svg_js->global);
	JSValue console = JS_NewObject(c);
 	JS_SetPropertyStr(c, console, "log", JS_NewCFunction(c, js_print, "print", 1));
	JS_SetPropertyStr(c, global, "console", console);

	svgDocument.class.gc_mark = domDocument_gc_mark;
	SETUP_JSCLASS(svgDocument, "SVGDocument", documentFuncs, NULL, dom_document_finalize, dom_js_get_document_proto(c));
	svgElement.class.gc_mark = domElement_gc_mark;
	SETUP_JSCLASS(svgElement, "SVGElement", svg_elementFuncs, NULL, dom_element_finalize, dom_js_get_element_proto(c));

	SETUP_JSCLASS(rgbClass, "SVGRGBColor", rgb_Funcs, NULL, baseCI_finalize, 0);
	SETUP_JSCLASS(rectClass, "SVGRect", rect_Funcs, NULL, baseCI_finalize, 0);
	SETUP_JSCLASS(pointClass, "SVGPoint", point_Funcs, NULL, baseCI_finalize, 0);
	SETUP_JSCLASS(matrixClass, "SVGMatrix", matrix_Funcs, NULL, baseCI_finalize, 0);

	SETUP_JSCLASS(pathClass, "SVGPath", path_Funcs, NULL, pathCI_finalize, 0);


	JS_SetPropertyStr(c, scene->svg_js->pathClass.proto, "MOVE_TO", JS_NewInt32(c, 77));
	JS_SetPropertyStr(c, scene->svg_js->pathClass.proto, "LINE_TO", JS_NewInt32(c, 76));
	JS_SetPropertyStr(c, scene->svg_js->pathClass.proto, "CURVE_TO", JS_NewInt32(c, 67));
	JS_SetPropertyStr(c, scene->svg_js->pathClass.proto, "QUAD_TO", JS_NewInt32(c, 81));
	JS_SetPropertyStr(c, scene->svg_js->pathClass.proto, "CLOSE", JS_NewInt32(c, 90));

	/*we have our own constructors*/
	scene->get_element_class = svg_get_element_class;
	scene->get_document_class = svg_get_document_class;

	/*create document object*/
	dom_js_define_document(scene->svg_js->js_ctx, global, scene);
	/*create event object, and remember it*/
	scene->svg_js->event = dom_js_define_event(scene->svg_js->js_ctx);

	JS_FreeValue(c, global);
}

GF_DOM_Event *dom_get_evt_private(JSValue v);

JSContext *svg_script_get_context(GF_SceneGraph *sg)
{
	return sg->svg_js ? sg->svg_js->js_ctx : NULL;
}

Bool svg_script_execute(GF_SceneGraph *sg, char *utf8_script, GF_DOM_Event *event)
{
	char szFuncName[1024];
	JSValue ret;
	Bool ok=GF_TRUE;
	GF_DOM_Event *prev_event = NULL;
	char *sep = strchr(utf8_script, '(');

	if (!sep) {
		strcpy(szFuncName, utf8_script);
		strcat(szFuncName, "(evt)");
		utf8_script = szFuncName;
	}

	gf_js_lock(sg->svg_js->js_ctx, GF_TRUE);

	prev_event = dom_get_evt_private(sg->svg_js->event);
	JS_SetOpaque(sg->svg_js->event, event);
	ret = JS_Eval(sg->svg_js->js_ctx, utf8_script, (u32) strlen(utf8_script), "inline script", sg->svg_js->use_strict ? JS_EVAL_TYPE_MODULE : JS_EVAL_TYPE_GLOBAL);
	JS_SetOpaque(sg->svg_js->event, prev_event);

#if 0
	//to check, what was the purpose of this ?
	if (ret==JS_FALSE) {
		char *sep = strchr(utf8_script, '(');
		if (sep) {
			sep[0] = 0;
			ret = JS_LookupProperty(sg->svg_js->js_ctx, sg->svg_js->global, utf8_script, &rval);
			sep[0] = '(';
		}
	}
#endif

	if (JS_IsException(ret)) {
		js_dump_error(sg->svg_js->js_ctx);
		ok=GF_FALSE;
	}
	JS_FreeValue(sg->svg_js->js_ctx, ret);

	if (sg->svg_js->force_gc) {
		gf_js_call_gc(sg->svg_js->js_ctx);
		sg->svg_js->force_gc = GF_FALSE;
	}
	js_std_loop(sg->svg_js->js_ctx);
	gf_js_lock(sg->svg_js->js_ctx, GF_FALSE);

	return ok;
}

#ifdef GPAC_ENABLE_HTML5_MEDIA
void html_media_js_api_del();
#endif

void gf_svg_script_context_del(GF_SVGJS *svg_js, GF_SceneGraph *scenegraph)
{
	gf_sg_js_dom_pre_destroy(JS_GetRuntime(svg_js->js_ctx), scenegraph, NULL);
	gf_js_delete_context(svg_js->js_ctx);
	dom_js_unload();
#ifdef GPAC_ENABLE_HTML5_MEDIA
	/* HTML */
	html_media_js_api_del();
#endif

	gf_free(svg_js);
	scenegraph->svg_js = NULL;

}

static void svg_script_predestroy(GF_Node *n, void *eff, Bool is_destroy)
{
	if (is_destroy) {
		GF_SVGJS *svg_js = n->sgprivate->scenegraph->svg_js;
		/*unregister script from parent scene (cf base_scenegraph::sg_reset) */
		gf_list_del_item(n->sgprivate->scenegraph->scripts, n);

		if (svg_js->nb_scripts) {
			svg_js->nb_scripts--;

			/*detach this script from our object cache*/
			gf_sg_js_dom_pre_destroy(JS_GetRuntime(svg_js->js_ctx), n->sgprivate->scenegraph, n);

			if (!svg_js->nb_scripts) {
				gf_svg_script_context_del(svg_js, n->sgprivate->scenegraph);
			}
		}
	}
}

GF_Err JSScript_CreateSVGContext(GF_SceneGraph *sg)
{
	GF_SVGJS *svg_js;

	if (sg->svg_js) {
		/* the JS/SVG context is already created, no need to do anything  */
		return GF_OK;
	}

	GF_SAFEALLOC(svg_js, GF_SVGJS);
	if (!svg_js) {
		return GF_OUT_OF_MEM;
	}
	/*create new ecmascript context*/
	svg_js->js_ctx = gf_js_create_context();
	if (!svg_js->js_ctx) {
		gf_free(svg_js);
		return GF_SCRIPT_ERROR;
	}

	gf_js_lock(svg_js->js_ctx, GF_TRUE);

	sg->svg_js = svg_js;
	/*load SVG & DOM APIs*/
	svg_init_js_api(sg);

#ifdef GPAC_ENABLE_HTML5_MEDIA
	/* HTML */
	html_media_init_js_api(sg);
#endif


	svg_js->script_execute = svg_script_execute;
	svg_js->handler_execute = svg_script_execute_handler;

	gf_js_lock(svg_js->js_ctx, GF_FALSE);

	return GF_OK;
}

GF_DOMText *svg_get_text_child(GF_Node *node)
{
	GF_ChildNodeItem *child;
	GF_DOMText *txt;
	txt = NULL;
	child = ((SVG_Element*)node)->children;
	if (! child) return NULL;
	while (child) {
		txt = (GF_DOMText*)child->node;
		if ((txt->sgprivate->tag==TAG_DOMText) && txt->textContent) return txt;
		txt = NULL;
		child = child->next;
	}
	return NULL;
}

static Bool svg_js_load_script(GF_Node *script, char *file)
{
	GF_Err e;
	u8 *jsscript;
	u32 fsize;
	Bool success = GF_TRUE;
	JSValue ret;
	GF_SVGJS *svg_js;
	u32 flags = JS_EVAL_TYPE_GLOBAL;
	char *abs_url = NULL;

	svg_js = script->sgprivate->scenegraph->svg_js;
	if (!strnicmp(file, "file://", 7)) file += 7;

	if (!gf_file_exists(file)) {
		GF_JSAPIParam par;
		GF_SceneGraph *scene = script->sgprivate->scenegraph;
		par.uri.url = file;
		if (scene->script_action && scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_RESOLVE_XLINK, script, &par))
			abs_url = (char *) par.uri.url;

		if (abs_url || !gf_file_exists(abs_url)) {
			if (abs_url) gf_free(abs_url);
			return GF_FALSE;
		}
	}

	e = gf_file_load_data(abs_url ? abs_url : file, &jsscript, &fsize);
	if (abs_url) gf_free(abs_url);

	if (e!=GF_OK) return GF_FALSE;

	/*for handler, only load code*/
	if (script->sgprivate->tag==TAG_SVG_handler) {
		GF_DOMText *txt = gf_dom_add_text_node(script, jsscript);
		txt->type = GF_DOM_TEXT_INSERTED;
		return GF_TRUE;
	}

	gf_js_lock(svg_js->js_ctx, GF_TRUE);

 	if (!gf_opts_get_bool("core", "no-js-mods") && JS_DetectModule((const char *) jsscript, fsize )) {
		flags = JS_EVAL_TYPE_MODULE;
		svg_js->use_strict = GF_TRUE;
	}

	ret = JS_Eval(svg_js->js_ctx, jsscript, sizeof(char)*fsize, file, flags);
	if (JS_IsException(ret)) {
		js_dump_error(svg_js->js_ctx);
		success=GF_FALSE;
	}
	JS_FreeValue(svg_js->js_ctx, ret);
	if (svg_js->force_gc) {
		gf_js_call_gc(svg_js->js_ctx);
		svg_js->force_gc = GF_FALSE;
	}
	gf_js_lock(svg_js->js_ctx, GF_FALSE);

	gf_dom_listener_process_add(script->sgprivate->scenegraph);

	gf_free(jsscript);
	return success;
}

#include <gpac/download.h>
#include <gpac/network.h>


void JSScript_LoadSVG(GF_Node *node)
{
	GF_DOMText *txt;
	GF_SVGJS *svg_js;
	GF_FieldInfo href_info;

	if (!node->sgprivate->scenegraph->svg_js) {
		if (JSScript_CreateSVGContext(node->sgprivate->scenegraph) != GF_OK) return;
	}

	/*register script with parent scene (cf base_scenegraph::sg_reset) */
	gf_list_add(node->sgprivate->scenegraph->scripts, node);

	svg_js = node->sgprivate->scenegraph->svg_js;
	if (!node->sgprivate->UserCallback) {
		svg_js->nb_scripts++;
		node->sgprivate->UserCallback = svg_script_predestroy;
	}
	/*if href download the script file*/
	if (gf_node_get_attribute_by_tag(node, TAG_XLINK_ATT_href, GF_FALSE, GF_FALSE, &href_info) == GF_OK) {
		GF_DownloadManager *dnld_man;
		GF_JSAPIParam par;
		char *url;
		XMLRI *xmlri = (XMLRI *)href_info.far_ptr;

		/* getting a download manager */
		par.dnld_man = NULL;
		ScriptAction(node->sgprivate->scenegraph, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
		dnld_man = par.dnld_man;


		/* resolve the uri of the script*/
		par.uri.nb_params = 0;
		par.uri.url = xmlri->string;
		ScriptAction(node->sgprivate->scenegraph, GF_JSAPI_OP_RESOLVE_URI, node, &par);
		url = (char *)par.uri.url;

		/* if the file is local, we don't need to download it */
		if (!strstr(url, "://") || !strnicmp(url, "file://", 7)) {
			svg_js_load_script(node, url);
		} else if (dnld_man) {
#ifdef GPAC_USE_DOWNLOADER
			GF_Err e;
			/*fetch the remote script synchronously and load it - cf section on script processing in SVG specs*/
			GF_DownloadSession *sess = gf_dm_sess_new(dnld_man, url, GF_NETIO_SESSION_NOT_THREADED, NULL, NULL, &e);
			if (sess) {
				e = gf_dm_sess_process(sess);
				if (e==GF_OK) {
					const char *szCache = gf_dm_sess_get_cache_name(sess);
					if (!svg_js_load_script(node, (char *) szCache))
						e = GF_SCRIPT_ERROR;
				}
				gf_dm_sess_del(sess);
			}
			if (e) {
				par.info.e = e;
				par.info.msg = "Cannot fetch script";
				ScriptAction(node->sgprivate->scenegraph, GF_JSAPI_OP_MESSAGE, NULL, &par);
			}
#else
		//todo
#endif
		}
		gf_free(url);
	}
	/*for scripts only, execute*/
	else if (node->sgprivate->tag == TAG_SVG_script) {
		JSValue ret;
		u32 txtlen;
		u32 flags = JS_EVAL_TYPE_GLOBAL;

		txt = svg_get_text_child(node);
		if (!txt) return;
		txtlen = (u32) strlen(txt->textContent);

		if (!gf_opts_get_bool("core", "no-js-mods") && JS_DetectModule((const char *) txt, txtlen )) {
			flags = JS_EVAL_TYPE_MODULE;
			svg_js->use_strict = GF_TRUE;
		}

		ret = JS_Eval(svg_js->js_ctx, txt->textContent, (u32) strlen(txt->textContent), "inline_script", flags);
		if (JS_IsException(ret)) {
			js_dump_error(svg_js->js_ctx);
		}
		JS_FreeValue(svg_js->js_ctx, ret);
		gf_dom_listener_process_add(node->sgprivate->scenegraph);
		js_std_loop(svg_js->js_ctx);
	}
}


#ifdef _DEBUG
//#define DUMP_DEF_AND_ROOT
#endif

#ifdef DUMP_DEF_AND_ROOT
void dump_root(const char *name, void *rp, void *data)
{
	if (name[0]=='_') fprintf(stderr, "\t%s\n", name);
}
#endif

/* Executes JavaScript code in response to an event being triggered
  The code to be executed (stored in a GF_DOMHandler struct) is either:
  - text content not yet passed to the JS engine
     - text contained in a node's text content not yet parsed (node)
	 - text, outside of a node, obtained by some external means (XHR, ...) - utf8_script
  - already in the JS engine, in the form of:
     - an anonymous function (js_fun_val)
	 - a named function (js_fun)
*/
static Bool svg_script_execute_handler(GF_Node *node, GF_DOM_Event *event, GF_Node *observer, char *utf8_script)
{
	GF_DOMText *txt = NULL;
	GF_SVGJS *svg_js;
	JSValue __this;
	JSValue ret;
	Bool free_this = GF_FALSE;
	u32 flags = JS_EVAL_TYPE_GLOBAL;
	Bool success=GF_TRUE;
	GF_DOM_Event *prev_event = NULL;
	GF_DOMHandler *hdl = (GF_DOMHandler *)node;

	/*LASeR hack for encoding scripts without handler - node is a listener in this case, not a handler*/
	if (utf8_script) {
		if (!node) return GF_FALSE;
		hdl = NULL;
	} else {
		if (!hdl->js_data || (JS_IsUndefined(hdl->js_data->fun_val) && JS_IsUndefined(hdl->js_data->evt_listen_obj))) {
			txt = svg_get_text_child(node);
			if (!txt) return GF_FALSE;
		}
		/*not sure about this (cf test struct-use-205-t.svg)*/
		//if (!node->sgprivate->parents) return GF_FALSE;
	}

	svg_js = node->sgprivate->scenegraph->svg_js;

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_SCRIPT, GF_LOG_DEBUG)) {
		char *content, *_content = NULL;
		if (utf8_script) {
			content = utf8_script;
		} else if (!JS_IsUndefined(hdl->js_data->fun_val)) {
			content = _content = (char *) JS_ToCString(svg_js->js_ctx, hdl->js_data->fun_val);
		} else if (txt) {
			content = txt->textContent;
		} else {
			content = "unknown";
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOM Events    ] Executing script code from handler: %s\n", content) );
		JS_FreeCString(svg_js->js_ctx, _content);
	}
#endif

	gf_js_lock(svg_js->js_ctx, GF_TRUE);
	prev_event = dom_get_evt_private(svg_js->event);
	/*break loops*/
	if (prev_event && (prev_event->type==event->type) && (prev_event->target==event->target)) {
		gf_js_lock(svg_js->js_ctx, GF_FALSE);
		return GF_FALSE;
	}
	JS_SetOpaque(svg_js->event, event);

	svg_js->in_script = GF_TRUE;
	if (svg_js->use_strict)
		flags = JS_EVAL_TYPE_MODULE;

	/*if an observer is being specified, use it*/
	if (hdl && hdl->js_data && !JS_IsUndefined(hdl->js_data->evt_listen_obj))
		__this = hdl->js_data->evt_listen_obj;
	/*compile the jsfun if any - 'this' is the associated observer*/
	else if (observer) {
		__this = dom_element_construct(svg_js->js_ctx, observer);
		free_this = GF_TRUE;
	} else {
		__this = svg_js->global;
	}

	if (txt && hdl && hdl->js_data && !JS_IsUndefined(hdl->js_data->fun_val)) {
		hdl->js_data->fun_val = JS_EvalThis(svg_js->js_ctx, __this, txt->textContent, strlen(txt->textContent), "handler", flags|JS_EVAL_FLAG_COMPILE_ONLY);
	}

	if (utf8_script) {
		ret = JS_EvalThis(svg_js->js_ctx, __this, utf8_script, (u32) strlen(utf8_script), "inline script", flags);
	}
	else if (hdl && hdl->js_data
		&& (!JS_IsUndefined(hdl->js_data->fun_val) || !JS_IsUndefined(hdl->js_data->evt_listen_obj) )
	) {
		JSValue evt;
		JSValue argv[1];
		evt = gf_dom_new_event(svg_js->js_ctx);
		JS_SetOpaque(evt, event);
		argv[0] = evt;

		if (!JS_IsUndefined(hdl->js_data->fun_val) ) {
			ret = JS_Call(svg_js->js_ctx, hdl->js_data->fun_val, __this, 1, argv);
		} else {
			JSValue fun = JS_GetPropertyStr(svg_js->js_ctx, hdl->js_data->evt_listen_obj, "hanldeEvent");
			ret = JS_Call(svg_js->js_ctx, fun, hdl->js_data->evt_listen_obj, 1, argv);
			JS_FreeValue(svg_js->js_ctx, fun);
		}
		JS_FreeValue(svg_js->js_ctx, evt);
	} else if (txt) {
		JSValue fun = JS_GetPropertyStr(svg_js->js_ctx, svg_js->global, txt->textContent);
		if (JS_IsUndefined(fun)) {
			JSValue glob = JS_GetGlobalObject(svg_js->js_ctx);
			fun = JS_GetPropertyStr(svg_js->js_ctx, glob, txt->textContent);
			JS_FreeValue(svg_js->js_ctx, glob);
		}

		if (!JS_IsUndefined(fun)) {
			ret = JS_NULL;
			if (! svg_script_execute(node->sgprivate->scenegraph, txt->textContent, event)) {
				success = GF_FALSE;
			}
		}
		else {
			ret = JS_EvalThis(svg_js->js_ctx, __this, txt->textContent, (u32) strlen(txt->textContent), "internal", flags);
		}
		JS_FreeValue(svg_js->js_ctx, fun);
	} else {
		ret = JS_NULL;
	}
	if (JS_IsException(ret)) {
		js_dump_error(svg_js->js_ctx);
		success = GF_FALSE;
	}
	JS_FreeValue(svg_js->js_ctx, ret);

	if (free_this)
		JS_FreeValue(svg_js->js_ctx, __this);

	JS_SetOpaque(svg_js->event, prev_event);
	if (txt && hdl && hdl->js_data) hdl->js_data->fun_val = JS_UNDEFINED;

	while (svg_js->force_gc) {
		svg_js->force_gc = GF_FALSE;
		gf_js_call_gc(svg_js->js_ctx);
	}
	svg_js->in_script = GF_FALSE;

	/*check any pending exception if outer-most event*/
	if (!prev_event) {
		js_std_loop(svg_js->js_ctx);
	}

	gf_js_lock(svg_js->js_ctx, GF_FALSE);

#ifdef DUMP_DEF_AND_ROOT
	if ((event->type==GF_EVENT_CLICK) || (event->type==GF_EVENT_MOUSEOVER)) {
		NodeIDedItem *reg_node;
		fprintf(stderr, "Node registry\n");
		reg_node = node->sgprivate->scenegraph->id_node;
		while (reg_node) {
			fprintf(stderr, "\t%s\n", reg_node->NodeName);
			reg_node = reg_node->next;
		}

		fprintf(stderr, "\n\nNamed roots:\n");
		JS_DumpNamedRoots(JS_GetRuntime(svg_js->js_ctx), dump_root, NULL);
	}
#endif

	if (!success) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("SVG: Invalid event handler script\n" ));
		return GF_FALSE;
	}
	return GF_TRUE;
}

void svg_execute_handler(GF_Node *node, GF_DOM_Event *event, GF_Node *observer)
{
	JSValue evt;
	JSValue argv[1];
	JSValue __this;
	JSValue ret;
	Bool success=GF_TRUE;
	GF_DOMHandler *hdl = (GF_DOMHandler *)node;
	JSContext *ctx;

	if (!hdl || !hdl->js_data || !hdl->js_data->ctx
		|| (JS_IsUndefined(hdl->js_data->fun_val) && JS_IsUndefined(hdl->js_data->evt_listen_obj))
	) {
		return;
	}
	ctx = hdl->js_data->ctx;

	/*if an observer is being specified, use it*/
	if (hdl && hdl->js_data && !JS_IsUndefined(hdl->js_data->evt_listen_obj))
		__this = hdl->js_data->evt_listen_obj;
	else
		return;
	/*compile the jsfun if any - 'this' is the associated observer*/
//	else
//		__this = observer ? dom_element_construct(ctx, observer) : svg_js->global;

	gf_js_lock(ctx, GF_TRUE);


	evt = gf_dom_new_event(ctx);
	JS_SetOpaque(evt, event);
	argv[0] = evt;

	if (!JS_IsUndefined(hdl->js_data->fun_val) ) {
		ret = JS_Call(ctx, hdl->js_data->fun_val, __this, 1, argv);
	} else {
		JSValue fun = JS_GetPropertyStr(ctx, hdl->js_data->evt_listen_obj, "hanldeEvent");
		ret = JS_Call(ctx, fun, hdl->js_data->evt_listen_obj, 1, argv);
		JS_FreeValue(ctx, fun);
	}

	if (JS_IsException(ret)) {
		js_dump_error(ctx);
		success = GF_FALSE;
	}
	JS_FreeValue(ctx, ret);
	JS_FreeValue(ctx, evt);

	gf_js_lock(ctx, GF_FALSE);

	if (!success) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("SVG: Invalid event handler script\n" ));
		return;
	}
}

#endif

#endif
