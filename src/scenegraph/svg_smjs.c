/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
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


#ifdef GPAC_HAS_SPIDERMONKEY

#ifdef GPAC_ANDROID
#ifndef XP_UNIX
#define XP_UNIX
#endif
#endif

#include <gpac/internal/smjs_api.h>

#define JSVAL_CHECK_STRING(_v) (JSVAL_IS_STRING(_v) || JSVAL_IS_NULL(_v))

static Bool svg_script_execute_handler(GF_Node *node, GF_DOM_Event *event, GF_Node *observer, char *utf8_script);


jsval dom_element_construct(JSContext *c, GF_Node *n);
#ifdef USE_FFDEV_15
void dom_element_finalize(JSFreeOp *fop, JSObject *obj);
void dom_document_finalize(JSFreeOp *fop, JSObject *obj);
#else
void dom_element_finalize(JSContext *c, JSObject *obj);
void dom_document_finalize(JSContext *c, JSObject *obj);
#endif

GF_Node *dom_get_element(JSContext *c, JSObject *obj);
GF_SceneGraph *dom_get_doc(JSContext *c, JSObject *obj);

void dom_node_changed(GF_Node *n, Bool child_modif, GF_FieldInfo *info);

JSBool dom_event_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool dom_event_remove_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

char *js_get_utf8(jsval val);

void dom_node_set_textContent(GF_Node *n, char *text);

jsval dom_node_get_sibling(JSContext *c, GF_Node *n, Bool is_prev, Bool elt_only);

void html_media_init_js_api(GF_SceneGraph *scene);

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

typedef struct
{
	u32 nb_inst;
	/*SVG uDOM classes*/
	GF_JSClass svgElement;
	GF_JSClass svgDocument;
	GF_JSClass globalClass;
	GF_JSClass connectionClass;
	GF_JSClass rgbClass;
	GF_JSClass rectClass;
	GF_JSClass pointClass;
	GF_JSClass pathClass;
	GF_JSClass matrixClass;

	GF_JSClass consoleClass;
} GF_SVGuDOM;
static GF_SVGuDOM *svg_rt = NULL;

static JSObject *svg_new_path_object(JSContext *c, SVG_PathData *d);



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

static JSBool SMJS_FUNCTION(svg_nav_to_location)
{
	GF_JSAPIParam par;
	GF_SceneGraph *sg;
	SMJS_OBJ
	SMJS_ARGS
	if ((argc!=1) || !GF_JS_InstanceOf(c, obj, &svg_rt->globalClass, NULL)) return JS_TRUE;
	sg = (GF_SceneGraph *)SMJS_GET_PRIVATE(c, obj);
	par.uri.url = SMJS_CHARS(c, argv[0]);
	par.uri.nb_params = 0;
	ScriptAction(sg, GF_JSAPI_OP_LOAD_URL, sg->RootNode, &par);
	SMJS_FREE(c, par.uri.url);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(svg_parse_xml)
{
	GF_SceneGraph *sg;
	JSObject *doc_obj;
	GF_Node *node;
	char *str;
	SMJS_ARGS
	GF_Node *gf_sm_load_svg_from_string(GF_SceneGraph *sg, char *svg_str);

	doc_obj = JSVAL_TO_OBJECT(argv[1]);
	if (!doc_obj) {
		dom_throw_exception(c, GF_DOM_EXC_WRONG_DOCUMENT_ERR);
		return JS_FALSE;
	}

	str = js_get_utf8(argv[0]);
	if (!str) return JS_TRUE;
	sg = dom_get_doc(c, doc_obj);

	node = gf_sm_load_svg_from_string(sg, str);
	gf_free(str);
	SMJS_SET_RVAL( dom_element_construct(c, node) );
	return JS_TRUE;
}

static void svg_script_error(JSContext *c, const char *msg, JSErrorReport *jserr)
{
	GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[JavaScript] %s in file %s:%d (%s)\n", msg, jserr->filename, jserr->lineno+1, jserr->linebuf));
}

static JSBool SMJS_FUNCTION(svg_echo)
{
	GF_SceneGraph *sg;
	SMJS_OBJ
	SMJS_ARGS
	if ((argc!=1) || (!GF_JS_InstanceOf(c, obj, &svg_rt->globalClass, NULL) && !GF_JS_InstanceOf(c, obj, &svg_rt->consoleClass, NULL))) return JS_TRUE;
	sg = (GF_SceneGraph *)SMJS_GET_PRIVATE(c, obj);
	if (!sg) return JS_TRUE;

	{
		char *str = SMJS_CHARS_FROM_STRING(c, JS_ValueToString(c, argv[0]) );
		_ScriptMessage(sg, str);
		SMJS_FREE(c, str);
	}
	return JS_TRUE;
}

static void svg_define_udom_exception(JSContext *c, JSObject *global)
{
	JSObject *obj = JS_DefineObject(c, global, "GlobalException", NULL, 0, 0);
	JS_DefineProperty(c, obj, "NOT_CONNECTED_ERR", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "ENCODING_ERR", INT_TO_JSVAL(2), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "DENIED_ERR", INT_TO_JSVAL(3), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "UNKNOWN_ERR", INT_TO_JSVAL(4), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	obj = JS_DefineObject(c, global, "SVGException", NULL, 0, 0);
	JS_DefineProperty(c, obj, "SVG_WRONG_TYPE_ERR", INT_TO_JSVAL(0), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "SVG_INVALID_VALUE_ERR", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "SVG_MATRIX_NOT_INVERTABLE", INT_TO_JSVAL(2), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	obj = JS_DefineObject(c, global, "SVGSVGElement", NULL, 0, 0);
	JS_DefineProperty(c, obj, "NAV_AUTO", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_NEXT", INT_TO_JSVAL(2), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_PREV", INT_TO_JSVAL(3), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_UP",		INT_TO_JSVAL(4), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_UP_RIGHT", INT_TO_JSVAL(5), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_RIGHT", INT_TO_JSVAL(6), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_DOWN_RIGHT", INT_TO_JSVAL(7), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_DOWN", INT_TO_JSVAL(8), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_DOWN_LEFT", INT_TO_JSVAL(9), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_LEFT", INT_TO_JSVAL(10), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "NAV_UP_LEFT", INT_TO_JSVAL(11), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
}

static SMJS_FUNC_PROP_GET( global_getProperty)

GF_SceneGraph *sg;
if (!GF_JS_InstanceOf(c, obj, &svg_rt->globalClass, NULL) )
	return JS_TRUE;

sg = (GF_SceneGraph *)SMJS_GET_PRIVATE(c, obj);
if (SMJS_ID_IS_INT(id)) {
	switch (SMJS_ID_TO_INT(id)) {
	/*namespaceURI*/
	case 0:
		return JS_TRUE;
	/*parent*/
	case 1:
		*vp = JSVAL_VOID;
		if (sg->parent_scene && sg->parent_scene->svg_js) *vp = OBJECT_TO_JSVAL(sg->parent_scene->svg_js->global);
		return JS_TRUE;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}

/*TODO - try to be more precise...*/
static JSBool SMJS_FUNCTION(dom_imp_has_feature)
{
	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_FALSE) );
	if (argc) {
		u32 len;
		char sep;
		SMJS_ARGS
		char *fname = SMJS_CHARS(c, argv[0]);
		if (!fname) return JS_TRUE;
		while (strchr(" \t\n\r", fname[0])) fname++;
		len = (u32) strlen(fname);
		while (len && strchr(" \t\n\r", fname[len-1])) len--;
		sep = fname[len];
		fname[len] = 0;
		if (!stricmp(fname, "xml")) SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );
		else if (!stricmp(fname, "core")) SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );
		else if (!stricmp(fname, "traversal")) SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );
		else if (!stricmp(fname, "uievents")) SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );
		else if (!stricmp(fname, "mouseevents")) SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );
		else if (!stricmp(fname, "mutationevents")) SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );
		else if (!stricmp(fname, "events")) SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );

		fname[len] = sep;
		SMJS_FREE(c, fname);
	}
	return JS_TRUE;
}

static GF_Node *get_corresponding_use(GF_Node *n)
{
	GF_Node *t;
	u32 i, count;
	if (!n || !n->sgprivate->scenegraph->use_stack) return NULL;

	/*find current node in the use stack - if found, return the use element*/
	count = gf_list_count(n->sgprivate->scenegraph->use_stack);
	for (i=count; i>0; i-=2) {
		t = (GF_Node *)gf_list_get(n->sgprivate->scenegraph->use_stack, i-2);
		if (t==n) {
			GF_Node *use = (GF_Node *)gf_list_get(n->sgprivate->scenegraph->use_stack, i-1);
			GF_Node *par_use = get_corresponding_use(use);
			return par_use ? par_use : use;
		}
	}
	/*otherwise recursively get up the tree*/
	return get_corresponding_use(gf_node_get_parent(n, 0));
}
static SMJS_FUNC_PROP_GET( svg_doc_getProperty)

u32 prop_id;
GF_SceneGraph *sg = dom_get_doc(c, obj);
if (!sg) return JS_TRUE;

if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
prop_id = SMJS_ID_TO_INT(id);
switch (prop_id) {
case 0:/*global*/
	*vp = OBJECT_TO_JSVAL( JS_GetGlobalObject(c) );
	return JS_TRUE;
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET(svg_element_getProperty)

u32 prop_id;
GF_JSAPIParam par;
JSString *s;
GF_Node *n = dom_get_element(c, obj);
if (!n) return JS_TRUE;

if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
prop_id = SMJS_ID_TO_INT(id);

switch (prop_id) {
case 0: /*id*/
{
	const char *node_name = gf_node_get_name((GF_Node*)n);
	if (node_name) {
		s = JS_NewStringCopyZ(c, node_name);
		*vp = STRING_TO_JSVAL( s );
		return JS_TRUE;
	}
	return JS_TRUE;
}
case 5:/*currentScale*/
	if (n->sgprivate->tag!=TAG_SVG_svg) return JS_TRUE;
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_SCALE, (GF_Node *)n, &par)) {
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT(par.val) );
		return JS_TRUE;
	}
	return JS_TRUE;
case 6:/*currentRotate*/
	if (n->sgprivate->tag!=TAG_SVG_svg) return JS_TRUE;
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_ROTATION, (GF_Node *)n, &par)) {
		*vp = JS_MAKE_DOUBLE(c, FIX2FLT(par.val) );
		return JS_TRUE;
	}
	return JS_TRUE;
case 7:/*currentTranslate*/
	if (n->sgprivate->tag!=TAG_SVG_svg) return JS_TRUE;
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_TRANSLATE, (GF_Node *)n, &par)) {
		JSObject *r = JS_NewObject(c, &svg_rt->pointClass._class, 0, 0);
		pointCI *rc = (pointCI *)gf_malloc(sizeof(pointCI));
		rc->x = FIX2FLT(par.pt.x);
		rc->y = FIX2FLT(par.pt.y);
		rc->sg = n->sgprivate->scenegraph;
		SMJS_SET_PRIVATE(c, r, rc);
		*vp = OBJECT_TO_JSVAL(r);
		return JS_TRUE;
	}
	return JS_TRUE;
case 8:/*viewport*/
	if (n->sgprivate->tag!=TAG_SVG_svg) return JS_TRUE;
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_VIEWPORT, (GF_Node *)n, &par)) {
		JSObject *r = JS_NewObject(c, &svg_rt->rectClass._class, 0, 0);
		rectCI *rc = (rectCI *)gf_malloc(sizeof(rectCI));
		rc->x = FIX2FLT(par.rc.x);
		rc->y = FIX2FLT(par.rc.y);
		rc->w = FIX2FLT(par.rc.width);
		rc->h = FIX2FLT(par.rc.height);
		rc->sg = n->sgprivate->scenegraph;
		SMJS_SET_PRIVATE(c, r, rc);
		*vp = OBJECT_TO_JSVAL(r);
		return JS_TRUE;
	}
	return JS_TRUE;
case 9:/*currentTime*/
	*vp = JS_MAKE_DOUBLE(c, gf_node_get_scene_time((GF_Node *)n) );
	return JS_TRUE;
case 10:/*isPaused*/
	*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
case 11:/*ownerSVGElement*/
	while (1) {
		GF_Node *par = gf_node_get_parent(n, 0);
		if (!par) return JS_TRUE;
		if (par->sgprivate->tag==TAG_SVG_svg) {
			*vp = dom_element_construct(c, par);
			return JS_TRUE;
		}
		n = par;
	}
	return JS_TRUE;
case 12:/*correspondingElement*/
	/*if we can find a corresponding element for this node, then this is an SVGElementInstance*/
	if (get_corresponding_use(n)) {
		*vp = dom_element_construct(c, n);
	} else {
		*vp = dom_element_construct(c, NULL);
	}
	return JS_TRUE;
case 13:/*correspondingUseElement*/
	*vp = dom_element_construct(c, get_corresponding_use(n));
	return JS_TRUE;
default:
	return JS_TRUE;
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_SET( svg_element_setProperty)

GF_JSAPIParam par;
jsdouble d;
u32 prop_id;
GF_Node *n = dom_get_element(c, obj);
if (!n) return JS_TRUE;

if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
prop_id = SMJS_ID_TO_INT(id);

switch (prop_id) {
case 0:/*id*/
	if (JSVAL_CHECK_STRING(*vp)) {
		char *id = SMJS_CHARS(c, *vp);
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
			SMJS_FREE(c, id);
		}
	}
	return JS_TRUE;
	/*currentScale*/
case 5:
	if (!JSVAL_IS_NUMBER(*vp) || (n->sgprivate->tag!=TAG_SVG_svg)) return JS_TRUE;
	JS_ValueToNumber(c, *vp, &d);
	par.val = FLT2FIX(d);
	if (!par.val) {
		return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);
	}
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_SCALE, (GF_Node *)n, &par)) {
		return JS_TRUE;
	}
	return JS_TRUE;
	/*currentRotate*/
case 6:
	if (!JSVAL_IS_NUMBER(*vp) || (n->sgprivate->tag!=TAG_SVG_svg)) return JS_TRUE;
	JS_ValueToNumber(c, *vp, &d);
	par.val = FLT2FIX(d);
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_ROTATION, (GF_Node *)n, &par)) {
		return JS_TRUE;
	}
	return JS_TRUE;
	/*currentTime*/
case 9:
	if (!JSVAL_IS_NUMBER(*vp) || (n->sgprivate->tag!=TAG_SVG_svg)) return JS_TRUE;
	JS_ValueToNumber(c, *vp, &d);
	par.time = d;
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_TIME, (GF_Node *)n, &par)) {
		return JS_TRUE;
	}
	return JS_TRUE;
default:
	return JS_TRUE;
}
return JS_TRUE;
}


static GF_Node *svg_udom_smil_check_instance(JSContext *c, JSObject *obj)
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
JSBool SMJS_FUNCTION_EXT(svg_udom_smil_time_insert, Bool is_end)
{
	GF_FieldInfo info;
	u32 i, count;
	Double offset;
	GF_List *times;
	SMIL_Time *newtime;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = svg_udom_smil_check_instance(c, obj);
	if (!n) return JS_TRUE;


	if (is_end) {
		info.far_ptr = ((SVGTimedAnimBaseElement *)n)->timingp->end;
	} else {
		info.far_ptr = ((SVGTimedAnimBaseElement *)n)->timingp->begin;
	}
	if (!info.far_ptr) {
		return JS_FALSE;
	}


	times = *((GF_List **)info.far_ptr);
	GF_SAFEALLOC(newtime, SMIL_Time);
	newtime->type = GF_SMIL_TIME_EVENT_RESOLVED;

	offset = 0;
	if (argc && JSVAL_IS_NUMBER(argv[0]) ) {
		jsdouble d;
		JS_ValueToNumber(c, argv[0], &d);
		offset = d;
	}
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

JSBool SMJS_FUNCTION(svg_udom_smil_begin)
{
	return svg_udom_smil_time_insert(SMJS_CALL_ARGS, GF_FALSE);
}

JSBool SMJS_FUNCTION(svg_udom_smil_end)
{
	return svg_udom_smil_time_insert(SMJS_CALL_ARGS, GF_TRUE);
}

/*TODO*/
JSBool SMJS_FUNCTION(svg_udom_smil_pause)
{
	u32 tag;
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);

	tag = gf_node_get_tag(n);
	if (gf_svg_is_animation_tag(tag)) {
		/* pausing an animation element (set, animate ...) should pause the main time line ? */
		gf_smil_timing_pause(n);
	} else if (gf_svg_is_timing_tag(tag)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_PAUSE_SVG, n, NULL);
	} else if ((tag==TAG_SVG_svg) && (n->sgprivate->scenegraph->RootNode==n)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_PAUSE_SVG, n, NULL);
	} else {
		return JS_TRUE;
	}
	return JS_TRUE;
}
/*TODO*/
JSBool SMJS_FUNCTION(svg_udom_smil_resume)
{
	u32 tag;
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);

	tag = gf_node_get_tag(n);
	if (gf_svg_is_animation_tag(tag)) {
		/* resuming an animation element (set, animate ...) should resume the main time line ? */
		gf_smil_timing_resume(n);
	} else if (gf_svg_is_timing_tag(tag)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_RESUME_SVG, n, NULL);
	} else if ((tag==TAG_SVG_svg) && (n->sgprivate->scenegraph->RootNode==n)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_RESUME_SVG, n, NULL);
	} else {
		return JS_TRUE;
	}
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_smil_restart)
{
	u32 tag;
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);

	tag = gf_node_get_tag(n);
	if ((tag==TAG_SVG_svg) && (n->sgprivate->scenegraph->RootNode==n)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_RESTART_SVG, n, NULL);
	} else {
		return JS_TRUE;
	}
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_smil_set_speed)
{
	u32 tag;
	Double speed = 1.0;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);

	if (argc && JSVAL_IS_NUMBER(argv[0]) ) {
		jsdouble d;
		JS_ValueToNumber(c, argv[0], &d);
		speed = d;
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
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_get_trait)
{
	char *attValue, *ns, *name;
	GF_Err e;
	GF_FieldInfo info;
	SMJS_ARGS
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	ns = name = NULL;
	if (! JSVAL_CHECK_STRING(argv[0]) ) return JS_TRUE;
	if (argc==2) {
		ns = SMJS_CHARS(c, argv[0]);
		name = SMJS_CHARS(c, argv[1]);
	} else if (argc==1) {
		name = SMJS_CHARS(c, argv[0]);
	} else return JS_TRUE;

	if (!name) {
		SMJS_FREE(c, ns);
		return JS_TRUE;
	}
	if (!strcmp(name, "#text")) {
		char *res = gf_dom_flatten_textContent(n);
		SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, res) ) );
		gf_free(res);
		SMJS_FREE(c, ns);
		SMJS_FREE(c, name);
		return JS_TRUE;
	}
	e = gf_node_get_field_by_name(n, name, &info);

	SMJS_FREE(c, ns);
	SMJS_FREE(c, name);

	if (e!=GF_OK) return JS_TRUE;
	SMJS_SET_RVAL( JSVAL_VOID );

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
	case SVG_ID_datatype:
	case SVG_GradientOffset_datatype:
		/*end of DOM string traits*/
		attValue = gf_svg_dump_attribute(n, &info);
		SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, attValue) ) );
		if (attValue) gf_free(attValue);
		return JS_TRUE;
		/*dump to trait*/
		break;

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
		return JS_TRUE;
#endif
	}
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_get_float_trait)
{
	char *szName;
	GF_FieldInfo info;
	GF_Err e;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = SMJS_CHARS(c, argv[0]);
	if (!szName) return JS_FALSE;

	SMJS_SET_RVAL( JSVAL_VOID );

	e = gf_node_get_attribute_by_name(n, szName, 0, GF_TRUE, GF_TRUE, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_Number_datatype:
	case SVG_FontSize_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	{
		SVG_Number *l = (SVG_Number *)info.far_ptr;
		if (l->type==SVG_NUMBER_AUTO || l->type==SVG_NUMBER_INHERIT) return JS_TRUE;
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT(l->value) ) );
		return JS_TRUE;
	}
	case SVG_Clock_datatype:
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, *(SVG_Clock*)info.far_ptr ) );
		return JS_TRUE;
	default:
		return JS_TRUE;
	}
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_get_matrix_trait)
{
	char *szName;
	JSObject *mO;
	GF_FieldInfo info;
	GF_Err e;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = SMJS_CHARS(c, argv[0]);
	SMJS_SET_RVAL( JSVAL_VOID );
	e = gf_node_get_field_by_name(n, szName, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_Transform_datatype) {
		GF_Matrix2D *mx = (GF_Matrix2D *)gf_malloc(sizeof(GF_Matrix2D));
		mO = JS_NewObject(c, &svg_rt->matrixClass._class, 0, 0);
		gf_mx2d_init(*mx);
		gf_mx2d_copy(*mx, ((SVG_Transform*)info.far_ptr)->mat);

		SMJS_SET_PRIVATE(c, mO, mx);
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(mO) );
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_get_rect_trait)
{
	JSObject *newObj;
	char *szName;
	GF_FieldInfo info;
	GF_Err e;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = SMJS_CHARS(c, argv[0]);
	SMJS_SET_RVAL( JSVAL_VOID );
	e = gf_node_get_field_by_name(n, szName, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_ViewBox_datatype) {
		rectCI *rc;
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		newObj = JS_NewObject(c, &svg_rt->rectClass._class, 0, 0);
		GF_SAFEALLOC(rc, rectCI);
		rc->x = FIX2FLT(v->x);
		rc->y = FIX2FLT(v->y);
		rc->w = FIX2FLT(v->width);
		rc->h = FIX2FLT(v->height);
		SMJS_SET_PRIVATE(c, newObj, rc);
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(newObj) );
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_get_path_trait)
{
	char *szName;
	GF_FieldInfo info;
	GF_Err e;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = SMJS_CHARS(c, argv[0]);
	SMJS_SET_RVAL( JSVAL_VOID );
	e = gf_node_get_field_by_name(n, szName, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_PathData_datatype) {
		SMJS_SET_RVAL( OBJECT_TO_JSVAL( svg_new_path_object(c, (SVG_PathData *)info.far_ptr) ) );
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_get_rgb_color_trait)
{
	char *szName;
	GF_FieldInfo info;
	rgbCI *rgb;
	GF_Err e;
	JSObject *newObj;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = SMJS_CHARS(c, argv[0]);
	SMJS_SET_RVAL( JSVAL_VOID );
	e = gf_node_get_field_by_name(n, szName, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

	switch (info.fieldType) {
	case SVG_Color_datatype:
	{
		SVG_Color *col = (SVG_Color *)info.far_ptr;
		if (col->type == SVG_COLOR_CURRENTCOLOR) return JS_TRUE;
		if (col->type == SVG_COLOR_INHERIT) return JS_TRUE;
		newObj = JS_NewObject(c, &svg_rt->rgbClass._class, 0, 0);
		GF_SAFEALLOC(rgb, rgbCI);
		rgb->r = (u8) (255*FIX2FLT(col->red)) ;
		rgb->g = (u8) (255*FIX2FLT(col->green)) ;
		rgb->b = (u8) (255*FIX2FLT(col->blue)) ;
		SMJS_SET_PRIVATE(c, newObj, rgb);
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(newObj) );
		return JS_TRUE;
	}
	break;
	case SVG_Paint_datatype:
	{
		SVG_Paint *paint = (SVG_Paint *)info.far_ptr;
		if (1 || paint->type==SVG_PAINT_COLOR) {
			newObj = JS_NewObject(c, &svg_rt->rgbClass._class, 0, 0);
			GF_SAFEALLOC(rgb, rgbCI);
			rgb->r = (u8) (255*FIX2FLT(paint->color.red) );
			rgb->g = (u8) (255*FIX2FLT(paint->color.green) );
			rgb->b = (u8) (255*FIX2FLT(paint->color.blue) );
			SMJS_SET_PRIVATE(c, newObj, rgb);
			SMJS_SET_RVAL( OBJECT_TO_JSVAL(newObj) );
			return JS_TRUE;
		}
		return JS_TRUE;
	}
	}
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_set_trait)
{
	char *ns, *name, *val;
	GF_Err e;
	GF_FieldInfo info;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	val = ns = name = NULL;
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	if (argc==3) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		if (!JSVAL_CHECK_STRING(argv[2])) return JS_TRUE;
		ns = SMJS_CHARS(c, argv[0]);
		name = SMJS_CHARS(c, argv[1]);
		val = SMJS_CHARS(c, argv[2]);
	} else if (argc==2) {
		name = SMJS_CHARS(c, argv[0]);
		val = SMJS_CHARS(c, argv[1]);
	} else {
		return JS_TRUE;
	}
	if (!name) {
		SMJS_FREE(c, ns);
		SMJS_FREE(c, val);
		return JS_TRUE;
	}
	if (!strcmp(name, "#text")) {
		if (val) dom_node_set_textContent(n, val);
		SMJS_FREE(c, ns);
		SMJS_FREE(c, name);
		SMJS_FREE(c, val);
		return JS_TRUE;
	}
	e = gf_node_get_field_by_name(n, name, &info);
	SMJS_FREE(c, ns);
	SMJS_FREE(c, name);

	if (!val || (e!=GF_OK)) {
		SMJS_FREE(c, val);
		return JS_TRUE;
	}
	SMJS_SET_RVAL( JSVAL_VOID );

	e = gf_svg_parse_attribute(n, &info, val, 0);
	SMJS_FREE(c, val);

	if (e) return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);

	dom_node_changed(n, GF_FALSE, &info);

	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_set_float_trait)
{
	GF_FieldInfo info;
	jsdouble d;
	GF_Err e;
	char *szName;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_IS_NUMBER(argv[1])) return JS_TRUE;
	JS_ValueToNumber(c, argv[1], &d);
	SMJS_SET_RVAL( JSVAL_VOID );
	szName = SMJS_CHARS(c, argv[0]);
	e = gf_node_get_field_by_name(n, szName, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

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
			gf_free(val);
		}
		GF_SAFEALLOC(val, SVG_Coordinate);
		val->type=SVG_NUMBER_VALUE;
		val->value = FLT2FIX(d);
		gf_list_add(*l, val);
		break;
	}
	default:
		return JS_TRUE;
	}
	dom_node_changed(n, GF_FALSE, &info);
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_set_matrix_trait)
{
	JSObject *mO;
	char *szName;
	GF_FieldInfo info;
	GF_Matrix2D *mx;
	GF_Err e;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;

	if (JSVAL_IS_NULL(argv[1]) || !JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	mO = JSVAL_TO_OBJECT(argv[1]);
	if (!GF_JS_InstanceOf(c, mO, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx = SMJS_GET_PRIVATE(c, mO);
	if (!mx) return JS_TRUE;

	szName = SMJS_CHARS(c, argv[0]);
	e = gf_node_get_field_by_name(n, szName, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_Transform_datatype) {
		gf_mx2d_copy(((SVG_Transform*)info.far_ptr)->mat, *mx);
		dom_node_changed(n, GF_FALSE, NULL);
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_set_rect_trait)
{
	JSObject *rO;
	char *szName;
	GF_FieldInfo info;
	rectCI *rc;
	GF_Err e;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;

	if (JSVAL_IS_NULL(argv[1]) || !JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	rO = JSVAL_TO_OBJECT(argv[1]);
	if (!GF_JS_InstanceOf(c, rO, &svg_rt->rectClass, NULL) ) return JS_TRUE;
	rc = SMJS_GET_PRIVATE(c, rO);
	if (!rc) return JS_TRUE;

	szName = SMJS_CHARS(c, argv[0]);
	e = gf_node_get_field_by_name(n, szName, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_ViewBox_datatype) {
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		v->x = FLT2FIX(rc->x);
		v->y = FLT2FIX(rc->y);
		v->width = FLT2FIX(rc->w);
		v->height = FLT2FIX(rc->h);
		dom_node_changed(n, GF_FALSE, NULL);
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_set_path_trait)
{
	pathCI *path;
	GF_FieldInfo info;
	JSObject *pO;
	char *szName;
	GF_Err e;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	if (JSVAL_IS_NULL(argv[1]) || !JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	pO = JSVAL_TO_OBJECT(argv[1]);
	if (!GF_JS_InstanceOf(c, pO, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	path = SMJS_GET_PRIVATE(c, pO);
	if (!path) return JS_TRUE;

	szName = SMJS_CHARS(c, argv[0]);
	e = gf_node_get_field_by_name(n, szName, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

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
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_set_rgb_color_trait)
{
	GF_FieldInfo info;
	rgbCI *rgb;
	JSObject *colO;
	GF_Err e;
	char *szName;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	colO = JSVAL_TO_OBJECT(argv[1]);
	if (!colO) return JS_TRUE;
	if (!GF_JS_InstanceOf(c, colO, &svg_rt->rgbClass, NULL) ) return JS_TRUE;
	rgb = SMJS_GET_PRIVATE(c, colO);
	if (!rgb) return JS_TRUE;

	SMJS_SET_RVAL(JSVAL_VOID);
	szName = SMJS_CHARS(c, argv[0]);
	e = gf_node_get_field_by_name(n, szName, &info);
	SMJS_FREE(c, szName);
	if (e != GF_OK) return JS_TRUE;

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
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION_EXT(svg_get_bbox, Bool get_screen)
{
	GF_JSAPIParam par;
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	par.bbox.is_set = GF_FALSE;
	if (ScriptAction(n->sgprivate->scenegraph, get_screen ? GF_JSAPI_OP_GET_SCREEN_BBOX : GF_JSAPI_OP_GET_LOCAL_BBOX, (GF_Node *)n, &par) ) {
		if (par.bbox.is_set) {
			JSObject *rO = JS_NewObject(c, &svg_rt->rectClass._class, 0, 0);
			rectCI *rc = (rectCI *)gf_malloc(sizeof(rectCI));
			rc->sg = NULL;
			rc->x = FIX2FLT(par.bbox.min_edge.x);
			/*BBox is in 3D coord system style*/
			rc->y = FIX2FLT(par.bbox.min_edge.y);
			rc->w = FIX2FLT(par.bbox.max_edge.x - par.bbox.min_edge.x);
			rc->h = FIX2FLT(par.bbox.max_edge.y - par.bbox.min_edge.y);
			SMJS_SET_PRIVATE(c, rO, rc);
			SMJS_SET_RVAL( OBJECT_TO_JSVAL(rO) );
		} else {
			SMJS_SET_RVAL( JSVAL_VOID );
		}
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_get_local_bbox)
{
	return svg_get_bbox(SMJS_CALL_ARGS, GF_FALSE);
}
JSBool SMJS_FUNCTION(svg_udom_get_screen_bbox)
{
	return svg_get_bbox(SMJS_CALL_ARGS, GF_TRUE);
}

JSBool SMJS_FUNCTION(svg_udom_get_screen_ctm)
{
	GF_JSAPIParam par;
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_TRANSFORM, (GF_Node *)n, &par)) {
		JSObject *mO = JS_NewObject(c, &svg_rt->matrixClass._class, 0, 0);
		GF_Matrix2D *mx = (GF_Matrix2D *)gf_malloc(sizeof(GF_Matrix2D));
		gf_mx2d_from_mx(mx, &par.mx);
		SMJS_SET_PRIVATE(c, mO, mx);
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(mO) );
		return JS_TRUE;
	}
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_create_matrix_components)
{
	GF_Matrix2D *mx;
	JSObject *mat;
	jsdouble v;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;
	if (argc!=6) return JS_TRUE;

	GF_SAFEALLOC(mx, GF_Matrix2D)
	JS_ValueToNumber(c, argv[0], &v);
	mx->m[0] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[1], &v);
	mx->m[3] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[2], &v);
	mx->m[1] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[3], &v);
	mx->m[4] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[4], &v);
	mx->m[2] = FLT2FIX(v);
	JS_ValueToNumber(c, argv[5], &v);
	mx->m[5] = FLT2FIX(v);
	mat = JS_NewObject(c, &svg_rt->matrixClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, mat, mx);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(mat) );
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_create_rect)
{
	rectCI *rc;
	JSObject *r;
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	GF_SAFEALLOC(rc, rectCI);
	r = JS_NewObject(c, &svg_rt->rectClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, r, rc);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(r) );
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_create_point)
{
	pointCI *pt;
	JSObject *r;
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	GF_SAFEALLOC(pt, pointCI);
	r = JS_NewObject(c, &svg_rt->pointClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, r, pt);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(r) );
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_create_path)
{
	pathCI *path;
	JSObject *p;
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	GF_SAFEALLOC(path, pathCI);
	p = JS_NewObject(c, &svg_rt->pathClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, p, path);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(p) );
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_create_color)
{
	rgbCI *col;
	JSObject *p;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;
	if (argc!=3) return JS_TRUE;

	GF_SAFEALLOC(col, rgbCI);
	col->r = JSVAL_TO_INT(argv[0]);
	col->g = JSVAL_TO_INT(argv[1]);
	col->b = JSVAL_TO_INT(argv[2]);
	p = JS_NewObject(c, &svg_rt->rgbClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, p, col);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(p) );
	return JS_TRUE;
}


JSBool SMJS_FUNCTION(svg_path_get_total_length)
{
	Double length = 0;
	GF_FieldInfo info;
	SMJS_OBJ

	GF_Node *n = (GF_Node *)dom_get_element(c, obj);
	if (!n) return JS_TRUE;
	if (n->sgprivate->tag != TAG_SVG_path) return JS_TRUE;

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
	SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, length ) );
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_move_focus)
{
	GF_JSAPIParam par;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;
	if ((argc!=1) || !JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;

	par.opt = JSVAL_TO_INT(argv[1]);
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_FOCUS, (GF_Node *)n, &par))
		return JS_TRUE;
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_set_focus)
{
	GF_JSAPIParam par;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;
	if ((argc!=1) || !JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;

	par.node = dom_get_element(c, JSVAL_TO_OBJECT(argv[0]));

	/*NOT IN THE GRAPH*/
	if (!par.node || !par.node->sgprivate->num_instances) return JS_TRUE;
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_FOCUS, (GF_Node *)n, &par))
		return JS_TRUE;
	return JS_TRUE;
}
JSBool SMJS_FUNCTION(svg_udom_get_focus)
{
	GF_JSAPIParam par;
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	SMJS_SET_RVAL( JSVAL_VOID );
	if (!ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_FOCUS, (GF_Node *)n, &par))
		return JS_TRUE;

	if (par.node) {
		SMJS_SET_RVAL( dom_element_construct(c, par.node) );
	}
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(svg_udom_get_time)
{
	SMJS_OBJ
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, gf_node_get_scene_time(n)) );
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(svg_connection_create)
{
	return JS_TRUE;
}

#ifdef GPAC_UNUSED_FUNC
/**
 * FIXME : those 5 funcs and two variables are not used anywhere...
 */
static JSBool svg_connection_set_encoding(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_TRUE;
}
static JSBool svg_connection_connect(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_TRUE;
}
static JSBool svg_connection_send(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_TRUE;
}
static JSBool svg_connection_close(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_TRUE;
}

static JSPropertySpec connectionProps[] = {
	SMJS_PROPERTY_SPEC("connected",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
	SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
};
static JSFunctionSpec connectionFuncs[] = {
	/*eventTarget interface*/
	{"addEventListenerNS", dom_event_add_listener, 4, 0, 0},
	{"removeEventListenerNS", dom_event_remove_listener, 4, 0, 0},
	{"addEventListener", dom_event_add_listener, 3, 0, 0},
	{"removeEventListener", dom_event_remove_listener, 3, 0, 0},
	/*connection interface*/
	{"setEncoding", svg_connection_set_encoding, 1, 0, 0},
	{"connect", svg_connection_connect, 1, 0, 0},
	{"send", svg_connection_send, 1, 0, 0},
	{"close", svg_connection_close, 0, 0, 0},
	{0, 0, 0, 0, 0}
};
#endif /*GPAC_UNUSED_FUNC*/

static DECL_FINALIZE( baseCI_finalize)

/*avoids GCC warning*/
void *data = SMJS_GET_PRIVATE(c, obj);
if (!c) c=NULL;
if (data) gf_free(data);
}

static SMJS_FUNC_PROP_GET(rgb_getProperty)

if (!GF_JS_InstanceOf(c, obj, &svg_rt->rgbClass, NULL) ) return JS_TRUE;
if (SMJS_ID_IS_INT(id)) {
	rgbCI *col = (rgbCI *)SMJS_GET_PRIVATE(c, obj);
	if (!col) return JS_TRUE;
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		*vp = INT_TO_JSVAL(col->r);
		return JS_TRUE;
	case 1:
		*vp = INT_TO_JSVAL(col->g);
		return JS_TRUE;
	case 2:
		*vp = INT_TO_JSVAL(col->b);
		return JS_TRUE;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}
static SMJS_FUNC_PROP_SET( rgb_setProperty)

if (!GF_JS_InstanceOf(c, obj, &svg_rt->rgbClass, NULL) ) return JS_TRUE;
if (SMJS_ID_IS_INT(id)) {
	rgbCI *col = (rgbCI *)SMJS_GET_PRIVATE(c, obj);
	if (!col) return JS_TRUE;
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		col->r = JSVAL_TO_INT(*vp);
		return JS_TRUE;
	case 1:
		col->g = JSVAL_TO_INT(*vp);
		return JS_TRUE;
	case 2:
		col->b = JSVAL_TO_INT(*vp);
		return JS_TRUE;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}


static SMJS_FUNC_PROP_GET(rect_getProperty)

if (!GF_JS_InstanceOf(c, obj, &svg_rt->rectClass, NULL) ) return JS_TRUE;
if (SMJS_ID_IS_INT(id)) {
	rectCI *rc = (rectCI *)SMJS_GET_PRIVATE(c, obj);
	if (!rc) return JS_TRUE;
	if (rc->sg) {
		GF_JSAPIParam par;
		ScriptAction(rc->sg, GF_JSAPI_OP_GET_VIEWPORT, rc->sg->RootNode, &par);
		rc->x = FIX2FLT(par.rc.x);
		rc->y = FIX2FLT(par.rc.y);
		rc->w = FIX2FLT(par.rc.width);
		rc->h = FIX2FLT(par.rc.height);
	}
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		*vp = JS_MAKE_DOUBLE(c, rc->x);
		return JS_TRUE;
	case 1:
		*vp = JS_MAKE_DOUBLE(c, rc->y);
		return JS_TRUE;
	case 2:
		*vp = JS_MAKE_DOUBLE(c, rc->w);
		return JS_TRUE;
	case 3:
		*vp = JS_MAKE_DOUBLE(c, rc->h);
		return JS_TRUE;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}
static SMJS_FUNC_PROP_SET( rect_setProperty)

if (!GF_JS_InstanceOf(c, obj, &svg_rt->rectClass, NULL) ) return JS_TRUE;
if (SMJS_ID_IS_INT(id)) {
	jsdouble d;
	rectCI *rc = (rectCI *)SMJS_GET_PRIVATE(c, obj);
	if (!rc) return JS_TRUE;
	JS_ValueToNumber(c, *vp, &d);
	switch (SMJS_ID_TO_INT(id)) {
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
		return JS_TRUE;
	}
}
return JS_TRUE;
}

static SMJS_FUNC_PROP_GET( point_getProperty)

if (!GF_JS_InstanceOf(c, obj, &svg_rt->pointClass, NULL) ) return JS_TRUE;
if (SMJS_ID_IS_INT(id)) {
	pointCI *pt = (pointCI *)SMJS_GET_PRIVATE(c, obj);
	if (!pt) return JS_TRUE;
	if (pt->sg) {
		GF_JSAPIParam par;
		ScriptAction(pt->sg, GF_JSAPI_OP_GET_TRANSLATE, pt->sg->RootNode, &par);
		pt->x = FIX2FLT(par.pt.x);
		pt->y = FIX2FLT(par.pt.y);
	}
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		*vp = JS_MAKE_DOUBLE(c, pt->x);
		return JS_TRUE;
	case 1:
		*vp = JS_MAKE_DOUBLE(c, pt->y);
		return JS_TRUE;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}
static SMJS_FUNC_PROP_SET( point_setProperty)

if (!GF_JS_InstanceOf(c, obj, &svg_rt->pointClass, NULL) ) return JS_TRUE;
if (SMJS_ID_IS_INT(id)) {
	jsdouble d;
	pointCI *pt = (pointCI *)SMJS_GET_PRIVATE(c, obj);
	if (!pt) return JS_TRUE;
	JS_ValueToNumber(c, *vp, &d);
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		pt->x = (Float) d;
		break;
	case 1:
		pt->y = (Float) d;
		break;
	default:
		return JS_TRUE;
	}
	if (pt->sg) {
		GF_JSAPIParam par;
		par.pt.x = FLT2FIX(pt->x);
		par.pt.y = FLT2FIX(pt->y);
		ScriptAction(pt->sg, GF_JSAPI_OP_SET_TRANSLATE, pt->sg->RootNode, &par);
	}
	return JS_TRUE;
}
return JS_TRUE;
}

static JSObject *svg_new_path_object(JSContext *c, SVG_PathData *d)
{
#if USE_GF_PATH
	return NULL;
#else
	JSObject *obj;
	pathCI *p;
	GF_SAFEALLOC(p, pathCI);
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
	obj = JS_NewObject(c, &svg_rt->pathClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, obj, p);
	return obj;
#endif
}

#ifdef GPAC_UNUSED_FUNC
static JSBool pathCI_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *p;
	GF_SAFEALLOC(p, pathCI);
	SMJS_SET_PRIVATE(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}
#endif /*GPAC_UNUSED_FUNC*/

static void pathCI_finalize(JSContext *c, JSObject *obj)
{
	pathCI *p = (pathCI *)SMJS_GET_PRIVATE(c, obj);
	if (p) {
		if (p->pts) gf_free(p->pts);
		if (p->tags) gf_free(p->tags);
		gf_free(p);
	}
}

static SMJS_FUNC_PROP_GET( path_getProperty)

if (!GF_JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
if (SMJS_ID_IS_INT(id)) {
	pathCI *p = (pathCI *)SMJS_GET_PRIVATE(c, obj);
	if (!p) return JS_TRUE;
	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		*vp = INT_TO_JSVAL(p->nb_coms);
		return JS_TRUE;
	default:
		return JS_TRUE;
	}
}
return JS_TRUE;
}
static JSBool SMJS_FUNCTION(svg_path_get_segment)
{
	pathCI *p;
	u32 idx;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = (pathCI *)SMJS_GET_PRIVATE(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=1) || !JSVAL_IS_INT(argv[0])) return JS_TRUE;
	idx = JSVAL_TO_INT(argv[0]);
	if (idx>=p->nb_coms) return JS_TRUE;
	switch (p->tags[idx]) {
	case 0:
		SMJS_SET_RVAL( INT_TO_JSVAL(77));
		return JS_TRUE;	/* Move To */
	case 1:
		SMJS_SET_RVAL( INT_TO_JSVAL(76));
		return JS_TRUE;	/* Line To */
	case 2:/* Curve To */
	case 3:/* next Curve To */
		SMJS_SET_RVAL( INT_TO_JSVAL(67));
		return JS_TRUE;
	case 4:/* Quad To */
	case 5:/* next Quad To */
		SMJS_SET_RVAL( INT_TO_JSVAL(81));
		return JS_TRUE;
	case 6:
		SMJS_SET_RVAL( INT_TO_JSVAL(90));
		return JS_TRUE;	/* Close */
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(svg_path_get_segment_param)
{
	pathCI *p;
	ptCI *pt;
	SMJS_OBJ
	SMJS_ARGS
	u32 i, idx, param_idx, pt_idx;
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = SMJS_GET_PRIVATE(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=2) || !JSVAL_IS_INT(argv[0]) || !JSVAL_IS_INT(argv[1])) return JS_TRUE;
	idx = JSVAL_TO_INT(argv[0]);
	param_idx = JSVAL_TO_INT(argv[1]);
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
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, param_idx ? pt->y : pt->x));
		return JS_TRUE;
	case 2:/* Curve To */
		if (param_idx>5) return JS_TRUE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, (param_idx%2) ? pt->y : pt->x));
		return JS_TRUE;
	case 3:/* Next Curve To */
		if (param_idx>5) return JS_TRUE;
		if (param_idx<2) {
			pt = &p->pts[pt_idx - 1];
			SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, param_idx ? pt->y : pt->x));
		} else {
			param_idx-=2;
			pt = &p->pts[pt_idx + (param_idx/2)];
			SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, (param_idx%2) ? pt->y : pt->x));
		}
		return JS_TRUE;
	case 4:/* Quad To */
		if (param_idx>3) return JS_TRUE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, (param_idx%2) ? pt->y : pt->x));
		return JS_TRUE;
	case 5:/* Next Quad To */
		if (param_idx>3) return JS_TRUE;
		if (param_idx<2) {
			pt = &p->pts[pt_idx - 1];
			SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, param_idx ? pt->y : pt->x));
		} else {
			param_idx-=2;
			pt = &p->pts[pt_idx];
			SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, param_idx ? pt->y : pt->x));
		}
		return JS_TRUE;
	/*spec is quite obscur here*/
	case 6:
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, 0));
		return JS_TRUE;
	}
	return JS_TRUE;
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
static JSBool SMJS_FUNCTION(svg_path_move_to)
{
	pathCI *p;
	jsdouble x, y;
	u32 nb_pts;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = (pathCI *)SMJS_GET_PRIVATE(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=2) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);
	nb_pts = svg_path_realloc_pts(p, 1);
	p->pts[nb_pts].x = (Float) x;
	p->pts[nb_pts].y = (Float) y;
	p->tags = (u8 *)gf_realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 0;
	p->nb_coms++;
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(svg_path_line_to)
{
	pathCI *p;
	jsdouble x, y;
	u32 nb_pts;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = (pathCI *)SMJS_GET_PRIVATE(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=2) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);
	nb_pts = svg_path_realloc_pts(p, 1);
	p->pts[nb_pts].x = (Float) x;
	p->pts[nb_pts].y = (Float) y;
	p->tags = (u8 *)gf_realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 1;
	p->nb_coms++;
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(svg_path_quad_to)
{
	pathCI *p;
	jsdouble x1, y1, x2, y2;
	u32 nb_pts;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = (pathCI *)SMJS_GET_PRIVATE(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=4) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1]) || !JSVAL_IS_NUMBER(argv[2]) || !JSVAL_IS_NUMBER(argv[3])) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x1);
	JS_ValueToNumber(c, argv[1], &y1);
	JS_ValueToNumber(c, argv[2], &x2);
	JS_ValueToNumber(c, argv[3], &y2);
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
static JSBool SMJS_FUNCTION(svg_path_curve_to)
{
	pathCI *p;
	jsdouble x1, y1, x2, y2, x, y;
	u32 nb_pts;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = (pathCI *)SMJS_GET_PRIVATE(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=6) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1]) || !JSVAL_IS_NUMBER(argv[2]) || !JSVAL_IS_NUMBER(argv[3]) || !JSVAL_IS_NUMBER(argv[4]) || !JSVAL_IS_NUMBER(argv[5])) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x1);
	JS_ValueToNumber(c, argv[1], &y1);
	JS_ValueToNumber(c, argv[2], &x2);
	JS_ValueToNumber(c, argv[3], &y2);
	JS_ValueToNumber(c, argv[4], &x);
	JS_ValueToNumber(c, argv[5], &y);
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
static JSBool SMJS_FUNCTION(svg_path_close)
{
	pathCI *p;
	SMJS_OBJ
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = (pathCI *)SMJS_GET_PRIVATE(c, obj);
	if (!p) return JS_TRUE;
	if (argc) return JS_TRUE;
	p->tags = (u8 *)gf_realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 6;
	p->nb_coms++;
	return JS_TRUE;
}

static SMJS_FUNC_PROP_GET( matrix_getProperty)

GF_Matrix2D *mx;
if (!GF_JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
mx = (GF_Matrix2D *)SMJS_GET_PRIVATE(c, obj);
if (!SMJS_ID_IS_INT(id)) return JS_TRUE;

if (!mx) return JS_TRUE;
switch (SMJS_ID_TO_INT(id)) {
case 0:
	*vp = JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[0]));
	return JS_TRUE;
case 1:
	*vp = JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[3]));
	return JS_TRUE;
case 2:
	*vp = JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[1]));
	return JS_TRUE;
case 3:
	*vp = JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[4]));
	return JS_TRUE;
case 4:
	*vp = JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[2]));
	return JS_TRUE;
case 5:
	*vp = JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[5]));
	return JS_TRUE;
default:
	return JS_TRUE;
}
return JS_TRUE;
}
static SMJS_FUNC_PROP_SET( matrix_setProperty)

jsdouble d;
GF_Matrix2D *mx;
if (!GF_JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
mx = (GF_Matrix2D *)SMJS_GET_PRIVATE(c, obj);
if (!SMJS_ID_IS_INT(id)) return JS_TRUE;

JS_ValueToNumber(c, *vp, &d);
switch (SMJS_ID_TO_INT(id)) {
case 0:
	mx->m[0] = FLT2FIX(d);
	break;
case 1:
	mx->m[3] = FLT2FIX(d);
	break;
case 2:
	mx->m[1] = FLT2FIX(d);
	break;
case 3:
	mx->m[4] = FLT2FIX(d);
	break;
case 4:
	mx->m[2] = FLT2FIX(d);
	break;
case 5:
	mx->m[5] = FLT2FIX(d);
	break;
default:
	return JS_TRUE;
}
return JS_TRUE;
}
static JSBool SMJS_FUNCTION(svg_mx2d_get_component)
{
	GF_Matrix2D *mx;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx = (GF_Matrix2D *)SMJS_GET_PRIVATE(c, obj);
	if (!mx || (argc!=1)) return JS_TRUE;
	if (!JSVAL_IS_INT(argv[0])) return JS_TRUE;
	switch (JSVAL_TO_INT(argv[0])) {
	case 0:
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[0])));
		return JS_TRUE;
	case 1:
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[3])));
		return JS_TRUE;
	case 2:
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[1])));
		return JS_TRUE;
	case 3:
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[4])));
		return JS_TRUE;
	case 4:
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[2])));
		return JS_TRUE;
	case 5:
		SMJS_SET_RVAL( JS_MAKE_DOUBLE(c, FIX2FLT(mx->m[5])));
		return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(svg_mx2d_multiply)
{
	JSObject *mat;
	GF_Matrix2D *mx1, *mx2;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = (GF_Matrix2D *)SMJS_GET_PRIVATE(c, obj);
	if (!mx1 || (argc!=1)) return JS_TRUE;
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;
	mat = JSVAL_TO_OBJECT(argv[0]);
	if (!GF_JS_InstanceOf(c, mat, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx2 = (GF_Matrix2D *)SMJS_GET_PRIVATE(c, mat);
	if (!mx2) return JS_TRUE;
	gf_mx2d_add_matrix(mx1, mx2);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(svg_mx2d_inverse)
{
	GF_Matrix2D *mx1;
	SMJS_OBJ
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = (GF_Matrix2D *)SMJS_GET_PRIVATE(c, obj);
	if (!mx1) return JS_TRUE;
	gf_mx2d_inverse(mx1);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(svg_mx2d_translate)
{
	jsdouble x, y;
	GF_Matrix2D *mx1, mx2;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = (GF_Matrix2D *)SMJS_GET_PRIVATE(c, obj);
	if (!mx1 || (argc!=2)) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);

	gf_mx2d_init(mx2);
	mx2.m[2] = FLT2FIX(x);
	mx2.m[5] = FLT2FIX(y);
	gf_mx2d_pre_multiply(mx1, &mx2);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(svg_mx2d_scale)
{
	jsdouble scale;
	GF_Matrix2D *mx1, mx2;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = (GF_Matrix2D *)SMJS_GET_PRIVATE(c, obj);
	if (!mx1 || (argc!=1)) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &scale);
	gf_mx2d_init(mx2);
	mx2.m[0] = mx2.m[4] = FLT2FIX(scale);
	gf_mx2d_pre_multiply(mx1, &mx2);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(svg_mx2d_rotate)
{
	jsdouble angle;
	GF_Matrix2D *mx1, mx2;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = (GF_Matrix2D *)SMJS_GET_PRIVATE(c, obj);
	if (!mx1 || (argc!=1)) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &angle);
	gf_mx2d_init(mx2);
	gf_mx2d_add_rotation(&mx2, 0, 0, gf_mulfix(FLT2FIX(angle/180), GF_PI));
	gf_mx2d_pre_multiply(mx1, &mx2);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj ));
	return JS_TRUE;
}

jsval svg_udom_new_rect(JSContext *c, Fixed x, Fixed y, Fixed width, Fixed height)
{
	JSObject *r = JS_NewObject(c, &svg_rt->rectClass._class, 0, 0);
	rectCI *rc = (rectCI *)gf_malloc(sizeof(rectCI));
	rc->x = FIX2FLT(x);
	rc->y = FIX2FLT(y);
	rc->w = FIX2FLT(width);
	rc->h = FIX2FLT(height);
	rc->sg = NULL;
	SMJS_SET_PRIVATE(c, r, rc);
	return OBJECT_TO_JSVAL(r);
}

jsval svg_udom_new_point(JSContext *c, Fixed x, Fixed y)
{
	JSObject *p = JS_NewObject(c, &svg_rt->pointClass._class, 0, 0);
	pointCI *pt = (pointCI *)gf_malloc(sizeof(pointCI));
	pt->x = FIX2FLT(x);
	pt->y = FIX2FLT(y);
	pt->sg = NULL;
	SMJS_SET_PRIVATE(c, p, pt);
	return OBJECT_TO_JSVAL(p);
}

void *html_get_element_class(GF_Node *n);

void *svg_get_element_class(GF_Node *n)
{
	if (!n) return NULL;
	if ((n->sgprivate->tag>=GF_NODE_RANGE_FIRST_SVG) && (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG)) {
		if (n->sgprivate->tag == TAG_SVG_video || n->sgprivate->tag == TAG_SVG_audio) {
			return html_get_element_class(n);
		}
		return &svg_rt->svgElement;
	}
	return NULL;
}
void *svg_get_document_class(GF_SceneGraph *sg)
{
	GF_Node *n = sg->RootNode;
	if (!n) return NULL;
	if ((n->sgprivate->tag>=GF_NODE_RANGE_FIRST_SVG) && (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG))
		return &svg_rt->svgDocument;
	return NULL;
}

Bool is_svg_document_class(JSContext *c, JSObject *obj)
{
	if (!obj) return GF_FALSE;
	if (GF_JS_InstanceOf(c, obj, &svg_rt->svgDocument, NULL))
		return GF_TRUE;
	return GF_FALSE;
}

Bool is_svg_element_class(JSContext *c, JSObject *obj)
{
	if (!obj) return GF_FALSE;
	if (GF_JS_InstanceOf(c, obj, &svg_rt->svgElement, NULL))
		return GF_TRUE;
	return GF_FALSE;
}

static void svg_init_js_api(GF_SceneGraph *scene)
{
	JS_SetContextPrivate(scene->svg_js->js_ctx, scene);
	JS_SetErrorReporter(scene->svg_js->js_ctx, svg_script_error);

	/*init global object*/
	scene->svg_js->global = gf_sg_js_global_object(scene->svg_js->js_ctx, &svg_rt->globalClass);

	JS_InitStandardClasses(scene->svg_js->js_ctx, scene->svg_js->global);
	/*remember pointer to scene graph!!*/
	SMJS_SET_PRIVATE(scene->svg_js->js_ctx, scene->svg_js->global, scene);
	{
		JSPropertySpec globalClassProps[] = {
			SMJS_PROPERTY_SPEC("connected",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("parent",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec globalClassFuncs[] = {
			SMJS_FUNCTION_SPEC("createConnection", svg_connection_create, 0),
			SMJS_FUNCTION_SPEC("gotoLocation", svg_nav_to_location, 1),
			SMJS_FUNCTION_SPEC("alert",           svg_echo,          0),
			SMJS_FUNCTION_SPEC("print",           svg_echo,          0),
			/*technically, this is part of Implementation interface, not global, but let's try not to complicate things too much*/
			SMJS_FUNCTION_SPEC("hasFeature", dom_imp_has_feature, 2),
			SMJS_FUNCTION_SPEC("parseXML",   svg_parse_xml,          0),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JS_DefineFunctions(scene->svg_js->js_ctx, scene->svg_js->global, globalClassFuncs);
		JS_DefineProperties(scene->svg_js->js_ctx, scene->svg_js->global, globalClassProps);
	}

	/*initialize DOM core */
	dom_js_load(scene, scene->svg_js->js_ctx, scene->svg_js->global);

	/*user-defined extensions*/
	gf_sg_load_script_extensions(scene, scene->svg_js->js_ctx, scene->svg_js->global, GF_FALSE);

	svg_define_udom_exception(scene->svg_js->js_ctx, scene->svg_js->global);

	/*Console class*/
	{

		JSPropertySpec consoleProps[] = {
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec consoleFuncs[] = {
			/*trait access interface*/
			SMJS_FUNCTION_SPEC("log", svg_echo, 1),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->consoleClass, 0, 0, consoleProps, 0, 0, consoleFuncs);
	}

	/*SVGDocument class*/
	{

		JSPropertySpec svgDocumentProps[] = {
			/*in our implementation, defaultView is just an alias to the global Window object*/
			SMJS_PROPERTY_SPEC("defaultView",			0,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSObject *doc_proto = dom_js_get_document_proto(scene->svg_js->js_ctx);
		GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, doc_proto, &svg_rt->svgDocument, 0, 0, svgDocumentProps, 0, 0, 0);
	}

	/*SVGElement class*/
	{
		JSFunctionSpec svgElementFuncs[] = {
			/*trait access interface*/
			SMJS_FUNCTION_SPEC("getTrait", svg_udom_get_trait, 1),
			SMJS_FUNCTION_SPEC("getTraitNS", svg_udom_get_trait, 2),
			SMJS_FUNCTION_SPEC("getFloatTrait", svg_udom_get_float_trait, 1),
			SMJS_FUNCTION_SPEC("getMatrixTrait", svg_udom_get_matrix_trait, 1),
			SMJS_FUNCTION_SPEC("getRectTrait", svg_udom_get_rect_trait, 1),
			SMJS_FUNCTION_SPEC("getPathTrait", svg_udom_get_path_trait, 1),
			SMJS_FUNCTION_SPEC("getRGBColorTrait", svg_udom_get_rgb_color_trait, 1),
			/*FALLBACK TO BASE-VALUE FOR NOW - WILL NEED EITHER DOM TREE-CLONING OR A FAKE RENDER
			PASS FOR EACH PRESENTATION VALUE ACCESS*/
			SMJS_FUNCTION_SPEC("getPresentationTrait", svg_udom_get_trait, 1),
			SMJS_FUNCTION_SPEC("getPresentationTraitNS", svg_udom_get_trait, 2),
			SMJS_FUNCTION_SPEC("getFloatPresentationTrait", svg_udom_get_float_trait, 1),
			SMJS_FUNCTION_SPEC("getMatrixPresentationTrait", svg_udom_get_matrix_trait, 1),
			SMJS_FUNCTION_SPEC("getRectPresentationTrait", svg_udom_get_rect_trait, 1),
			SMJS_FUNCTION_SPEC("getPathPresentationTrait", svg_udom_get_path_trait, 1),
			SMJS_FUNCTION_SPEC("getRGBColorPresentationTrait", svg_udom_get_rgb_color_trait, 1),
			SMJS_FUNCTION_SPEC("setTrait", svg_udom_set_trait, 2),
			SMJS_FUNCTION_SPEC("setTraitNS", svg_udom_set_trait, 3),
			SMJS_FUNCTION_SPEC("setFloatTrait", svg_udom_set_float_trait, 2),
			SMJS_FUNCTION_SPEC("setMatrixTrait", svg_udom_set_matrix_trait, 2),
			SMJS_FUNCTION_SPEC("setRectTrait", svg_udom_set_rect_trait, 2),
			SMJS_FUNCTION_SPEC("setPathTrait", svg_udom_set_path_trait, 2),
			SMJS_FUNCTION_SPEC("setRGBColorTrait", svg_udom_set_rgb_color_trait, 2),
			/*locatable interface*/
			SMJS_FUNCTION_SPEC("getBBox", svg_udom_get_local_bbox, 0),
			SMJS_FUNCTION_SPEC("getScreenCTM", svg_udom_get_screen_ctm, 0),
			SMJS_FUNCTION_SPEC("getScreenBBox", svg_udom_get_screen_bbox, 0),
			/*svgSVGElement interface*/
			SMJS_FUNCTION_SPEC("createSVGMatrixComponents", svg_udom_create_matrix_components, 0),
			SMJS_FUNCTION_SPEC("createSVGRect", svg_udom_create_rect, 0),
			SMJS_FUNCTION_SPEC("createSVGPath", svg_udom_create_path, 0),
			SMJS_FUNCTION_SPEC("createSVGRGBColor", svg_udom_create_color, 0),
			SMJS_FUNCTION_SPEC("createSVGPoint", svg_udom_create_point, 0),

			SMJS_FUNCTION_SPEC("moveFocus", svg_udom_move_focus, 0),
			SMJS_FUNCTION_SPEC("setFocus", svg_udom_set_focus, 0),
			SMJS_FUNCTION_SPEC("getCurrentFocusedObject", svg_udom_get_focus, 0),
			SMJS_FUNCTION_SPEC("getCurrentTime", svg_udom_get_time, 0),

			/*timeControl interface*/
			SMJS_FUNCTION_SPEC("beginElementAt", svg_udom_smil_begin, 1),
			SMJS_FUNCTION_SPEC("beginElement", svg_udom_smil_begin, 0),
			SMJS_FUNCTION_SPEC("endElementAt", svg_udom_smil_end, 1),
			SMJS_FUNCTION_SPEC("endElement", svg_udom_smil_end, 0),
			SMJS_FUNCTION_SPEC("pauseElement", svg_udom_smil_pause, 0),
			SMJS_FUNCTION_SPEC("resumeElement", svg_udom_smil_resume, 0),
			SMJS_FUNCTION_SPEC("restartElement", svg_udom_smil_restart, 0),
			SMJS_FUNCTION_SPEC("setSpeed", svg_udom_smil_set_speed, 0),

			SMJS_FUNCTION_SPEC("getTotalLength", svg_path_get_total_length, 0),

			SMJS_FUNCTION_SPEC(0, 0, 0)
		};

		JSPropertySpec svgElementProps[] = {
			/*svgElement interface*/
			SMJS_PROPERTY_SPEC("id",						0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			/*svgSVGElement interface*/
			SMJS_PROPERTY_SPEC("currentScale",			5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("currentRotate",			6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("currentTranslate",		7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("viewport",				8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("currentTime",				9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			/*timeControl interface*/
			SMJS_PROPERTY_SPEC("isPaused",				10,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			/*old SVG1.1 stuff*/
			SMJS_PROPERTY_SPEC("ownerSVGElement",			11,		JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			/*SVGElementInstance*/
			SMJS_PROPERTY_SPEC("correspondingElement",	12, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("correspondingUseElement",	13, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSObject *elt_proto = dom_js_get_element_proto(scene->svg_js->js_ctx);
		GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, elt_proto, &svg_rt->svgElement, 0, 0, svgElementProps, svgElementFuncs, 0, 0);

	}

	/*RGBColor class*/
	{
		JSPropertySpec rgbClassProps[] = {
			SMJS_PROPERTY_SPEC("red",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("green",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("blue",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->rgbClass, 0, 0, rgbClassProps, 0, 0, 0);
	}
	/*SVGRect class*/
	{
		JSPropertySpec rectClassProps[] = {
			SMJS_PROPERTY_SPEC("x",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("y",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("width",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("height",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->rectClass, 0, 0, rectClassProps, 0, 0, 0);
	}
	/*SVGPoint class*/
	{
		JSPropertySpec pointClassProps[] = {
			SMJS_PROPERTY_SPEC("x",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("y",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->pointClass, 0, 0, pointClassProps, 0, 0, 0);
	}
	/*SVGMatrix class*/
	{
		JSFunctionSpec matrixClassFuncs[] = {
			SMJS_FUNCTION_SPEC("getComponent", svg_mx2d_get_component, 1),
			SMJS_FUNCTION_SPEC("mMultiply", svg_mx2d_multiply, 1),
			SMJS_FUNCTION_SPEC("inverse", svg_mx2d_inverse, 0),
			SMJS_FUNCTION_SPEC("mTranslate", svg_mx2d_translate, 2),
			SMJS_FUNCTION_SPEC("mScale", svg_mx2d_scale, 1),
			SMJS_FUNCTION_SPEC("mRotate", svg_mx2d_rotate, 1),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JSPropertySpec matrixClassProps[] = {
			SMJS_PROPERTY_SPEC("a",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("b",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("c",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("d",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("e",	4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("f",	5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->matrixClass, 0, 0, matrixClassProps, matrixClassFuncs, 0, 0);
	}
	/*SVGPath class*/
	{
		JSFunctionSpec pathClassFuncs[] = {
			SMJS_FUNCTION_SPEC("getSegment", svg_path_get_segment, 1),
			SMJS_FUNCTION_SPEC("getSegmentParam", svg_path_get_segment_param, 2),
			SMJS_FUNCTION_SPEC("moveTo", svg_path_move_to, 2),
			SMJS_FUNCTION_SPEC("lineTo", svg_path_line_to, 2),
			SMJS_FUNCTION_SPEC("quadTo", svg_path_quad_to, 4),
			SMJS_FUNCTION_SPEC("curveTo", svg_path_curve_to, 6),
			SMJS_FUNCTION_SPEC("close", svg_path_close, 0),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JSPropertySpec pathClassProps[] = {
			SMJS_PROPERTY_SPEC("numberOfSegments",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		GF_JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->pathClass, 0, 0, pathClassProps, pathClassFuncs, 0, 0);
		JS_DefineProperty(scene->svg_js->js_ctx, svg_rt->pathClass._proto, "MOVE_TO", INT_TO_JSVAL(77), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(scene->svg_js->js_ctx, svg_rt->pathClass._proto, "LINE_TO", INT_TO_JSVAL(76), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(scene->svg_js->js_ctx, svg_rt->pathClass._proto, "CURVE_TO", INT_TO_JSVAL(67), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(scene->svg_js->js_ctx, svg_rt->pathClass._proto, "QUAD_TO", INT_TO_JSVAL(81), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(scene->svg_js->js_ctx, svg_rt->pathClass._proto, "CLOSE", INT_TO_JSVAL(90), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	}
	/*we have our own constructors*/
	dom_set_class_selector(scene->svg_js->js_ctx, svg_get_element_class, svg_get_document_class);

	/*create document object*/
	dom_js_define_document(scene->svg_js->js_ctx, scene->svg_js->global, scene);
	/*create event object, and remember it*/
	scene->svg_js->event = dom_js_define_event(scene->svg_js->js_ctx, scene->svg_js->global);
}

Bool svg_script_execute(GF_SceneGraph *sg, char *utf8_script, GF_DOM_Event *event)
{
	char szFuncName[1024];
	JSBool ret;
	jsval rval;
	GF_DOM_Event *prev_event = NULL;
	char *sep = strchr(utf8_script, '(');

	if (!sep) {
		strcpy(szFuncName, utf8_script);
		strcat(szFuncName, "(evt)");
		utf8_script = szFuncName;
	}

	gf_sg_lock_javascript(sg->svg_js->js_ctx, GF_TRUE);

	prev_event = (GF_DOM_Event *)SMJS_GET_PRIVATE(sg->svg_js->js_ctx, sg->svg_js->event);
	SMJS_SET_PRIVATE(sg->svg_js->js_ctx, sg->svg_js->event, event);
	ret = JS_EvaluateScript(sg->svg_js->js_ctx, sg->svg_js->global, utf8_script, (u32) strlen(utf8_script), 0, 0, &rval);
	SMJS_SET_PRIVATE(sg->svg_js->js_ctx, sg->svg_js->event, prev_event);

	if (ret==JS_FALSE) {
		char *sep = strchr(utf8_script, '(');
		if (sep) {
			sep[0] = 0;
			ret = JS_LookupProperty(sg->svg_js->js_ctx, sg->svg_js->global, utf8_script, &rval);
			sep[0] = '(';
		}
	}

	if (sg->svg_js->force_gc) {
		gf_sg_js_call_gc(sg->svg_js->js_ctx);
		sg->svg_js->force_gc = GF_FALSE;
	}
	gf_sg_lock_javascript(sg->svg_js->js_ctx, GF_FALSE);

	return (ret==JS_FALSE) ? GF_FALSE : GF_TRUE;
}

void html_media_js_api_del();

void gf_svg_script_context_del(GF_SVGJS *svg_js, GF_SceneGraph *scenegraph)
{
	dom_js_pre_destroy(svg_js->js_ctx, scenegraph, NULL);
	/*user-defined extensions*/
	gf_sg_load_script_extensions(scenegraph, svg_js->js_ctx, svg_js->global, GF_TRUE);
	gf_sg_ecmascript_del(svg_js->js_ctx);
	dom_js_unload();
	gf_free(svg_js);
	scenegraph->svg_js = NULL;
	assert(svg_rt);
	svg_rt->nb_inst--;
	if (!svg_rt->nb_inst) {
		/* HTML */
		html_media_js_api_del();
		gf_free(svg_rt);
		svg_rt = NULL;
	}
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
			dom_js_pre_destroy(svg_js->js_ctx, n->sgprivate->scenegraph, n);

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
	/*create new ecmascript context*/
	svg_js->js_ctx = gf_sg_ecmascript_new(sg);
	if (!svg_js->js_ctx) {
		gf_free(svg_js);
		return GF_SCRIPT_ERROR;
	}

	gf_sg_lock_javascript(svg_js->js_ctx, GF_TRUE);

	if (!svg_rt) {
		GF_SAFEALLOC(svg_rt, GF_SVGuDOM);
		JS_SETUP_CLASS(svg_rt->svgElement, "SVGElement", JSCLASS_HAS_PRIVATE, svg_element_getProperty, svg_element_setProperty, dom_element_finalize);
		JS_SETUP_CLASS(svg_rt->svgDocument, "SVGDocument", JSCLASS_HAS_PRIVATE, svg_doc_getProperty, JS_PropertyStub_forSetter, dom_document_finalize);
		JS_SETUP_CLASS(svg_rt->globalClass, "Window", JSCLASS_HAS_PRIVATE, global_getProperty, JS_PropertyStub_forSetter, JS_FinalizeStub);
		JS_SETUP_CLASS(svg_rt->rgbClass, "SVGRGBColor", JSCLASS_HAS_PRIVATE, rgb_getProperty, rgb_setProperty, baseCI_finalize);
		JS_SETUP_CLASS(svg_rt->rectClass, "SVGRect", JSCLASS_HAS_PRIVATE, rect_getProperty, rect_setProperty, baseCI_finalize);
		JS_SETUP_CLASS(svg_rt->pointClass, "SVGPoint", JSCLASS_HAS_PRIVATE, point_getProperty, point_setProperty, baseCI_finalize);
		JS_SETUP_CLASS(svg_rt->matrixClass, "SVGMatrix", JSCLASS_HAS_PRIVATE, matrix_getProperty, matrix_setProperty, baseCI_finalize);
		JS_SETUP_CLASS(svg_rt->pathClass, "SVGPath", JSCLASS_HAS_PRIVATE, path_getProperty, JS_PropertyStub_forSetter, pathCI_finalize);
		JS_SETUP_CLASS(svg_rt->consoleClass, "console", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_PropertyStub);
	}
	svg_rt->nb_inst++;

	sg->svg_js = svg_js;
	/*load SVG & DOM APIs*/
	svg_init_js_api(sg);

	/* HTML */
	html_media_init_js_api(sg);

	svg_js->script_execute = svg_script_execute;
	svg_js->handler_execute = svg_script_execute_handler;

	gf_sg_lock_javascript(svg_js->js_ctx, GF_FALSE);

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
	FILE *jsf;
	char *jsscript;
	u32 fsize;
	Bool success = GF_TRUE;
	JSBool ret;
	jsval rval;
	GF_SVGJS *svg_js;

	svg_js = script->sgprivate->scenegraph->svg_js;
	jsf = gf_f64_open(file, "rb");
	if (!jsf) {
		GF_JSAPIParam par;
		GF_SceneGraph *scene = script->sgprivate->scenegraph;
		char *abs_url = NULL;
		par.uri.url = file;
		if (scene->script_action && scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_RESOLVE_XLINK, script, &par))
			abs_url = (char *) par.uri.url;

		if (abs_url) {
			jsf = gf_f64_open(abs_url, "rb");
			gf_free(abs_url);
		}
	}
	if (!jsf) return GF_FALSE;

	gf_f64_seek(jsf, 0, SEEK_END);
	fsize = (u32) gf_f64_tell(jsf);
	gf_f64_seek(jsf, 0, SEEK_SET);
	jsscript = (char *)gf_malloc(sizeof(char)*(size_t)(fsize+1));
	fsize = (u32) fread(jsscript, sizeof(char), (size_t)fsize, jsf);
	fclose(jsf);
	jsscript[fsize] = 0;

	/*for handler, only load code*/
	if (script->sgprivate->tag==TAG_SVG_handler) {
		GF_DOMText *txt = gf_dom_add_text_node(script, jsscript);
		txt->type = GF_DOM_TEXT_INSERTED;
		return GF_TRUE;
	}

	gf_sg_lock_javascript(svg_js->js_ctx, GF_TRUE);

	ret = JS_EvaluateScript(svg_js->js_ctx, svg_js->global, jsscript, sizeof(char)*fsize, file, 0, &rval);

	if (svg_js->force_gc) {
		gf_sg_js_call_gc(svg_js->js_ctx);
		svg_js->force_gc = GF_FALSE;
	}
	gf_sg_lock_javascript(svg_js->js_ctx, GF_FALSE);

	if (ret==JS_FALSE) success = GF_FALSE;
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
	JSBool ret;
	jsval rval;
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
		GF_Err e;
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
		}
		gf_free(url);
	}
	/*for scripts only, execute*/
	else if (node->sgprivate->tag == TAG_SVG_script) {
		txt = svg_get_text_child(node);
		if (!txt) return;
		ret = JS_EvaluateScript(svg_js->js_ctx, svg_js->global, txt->textContent, (u32) strlen(txt->textContent), 0, 0, &rval);
		if (ret==JS_FALSE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("SVG: Invalid script\n") );
		}
		gf_dom_listener_process_add(node->sgprivate->scenegraph);
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
	JSObject *__this;
	JSBool ret = JS_TRUE;
	GF_DOM_Event *prev_event = NULL;
	GF_DOMHandler *hdl = (GF_DOMHandler *)node;
	jsval fval, rval;

	/*LASeR hack for encoding scripts without handler - node is a listener in this case, not a handler*/
	if (utf8_script) {
		if (!node) return GF_FALSE;
		hdl = NULL;
	} else {
		if (!hdl->js_fun && !hdl->js_fun_val && !hdl->evt_listen_obj) {
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
		} else if (hdl->js_fun_val) {
			JSString *s = JS_DecompileFunction(svg_js->js_ctx, JS_ValueToFunction(svg_js->js_ctx, (jsval) hdl->js_fun_val), 0);
			content = _content = SMJS_CHARS_FROM_STRING(svg_js->js_ctx, s);
		} else if (hdl->js_fun) {
			JSString *s=JS_DecompileFunction(svg_js->js_ctx, (JSFunction *)hdl->js_fun, 0);
			content = _content = SMJS_CHARS_FROM_STRING(svg_js->js_ctx, s);
		} else if (txt) {
			content = txt->textContent;
		} else {
			content = "unknown";
		}
		gf_log_lt(GF_LOG_DEBUG, GF_LOG_SCRIPT);
		gf_log("[DOM Events    ] Executing script code from handler: %s\n", content);
		SMJS_FREE(svg_js->js_ctx, _content);
	}
#endif

	gf_sg_lock_javascript(svg_js->js_ctx, GF_TRUE);
	prev_event = (GF_DOM_Event *)SMJS_GET_PRIVATE(svg_js->js_ctx, svg_js->event);
	/*break loops*/
	if (prev_event && (prev_event->type==event->type) && (prev_event->target==event->target)) {
		gf_sg_lock_javascript(svg_js->js_ctx, GF_FALSE);
		return GF_FALSE;
	}
	SMJS_SET_PRIVATE(svg_js->js_ctx, svg_js->event, event);

	svg_js->in_script = GF_TRUE;

	/*if an observer is being specified, use it*/
	if (hdl && hdl->evt_listen_obj) __this = (JSObject *)hdl->evt_listen_obj;
	/*compile the jsfun if any - 'this' is the associated observer*/
	else __this = observer ? JSVAL_TO_OBJECT( dom_element_construct(svg_js->js_ctx, observer) ) : svg_js->global;
	if (txt && hdl && !hdl->js_fun) {
		const char *argn = "evt";
		hdl->js_fun = JS_CompileFunction(svg_js->js_ctx, __this, NULL, 1, &argn, txt->textContent, strlen(txt->textContent), NULL, 0);
	}

	if (utf8_script) {
		ret = JS_EvaluateScript(svg_js->js_ctx, __this, utf8_script, (u32) strlen(utf8_script), 0, 0, &rval);
	}
	else if (hdl->js_fun || hdl->js_fun_val || hdl->evt_listen_obj) {
		JSObject *evt;
		jsval argv[1];
		evt = gf_dom_new_event(svg_js->js_ctx);
		SMJS_SET_PRIVATE(svg_js->js_ctx, evt, event);
		argv[0] = OBJECT_TO_JSVAL(evt);

		if (hdl->js_fun) {
			ret = JS_CallFunction(svg_js->js_ctx, __this, (JSFunction *)hdl->js_fun, 1, argv, &rval);
		}
		else if (hdl->js_fun_val) {
			jsval funval = (jsval) hdl->js_fun_val;
			ret = JS_CallFunctionValue(svg_js->js_ctx, __this, funval, 1, argv, &rval);
		} else {
			ret = JS_CallFunctionName(svg_js->js_ctx, (JSObject *)hdl->evt_listen_obj, "handleEvent", 1, argv, &rval);
		}
	}
	else if (JS_LookupProperty(svg_js->js_ctx, svg_js->global, txt->textContent, &fval) && !JSVAL_IS_VOID(fval) ) {
		if (svg_script_execute(node->sgprivate->scenegraph, txt->textContent, event))
			ret = JS_FALSE;
	}
	else {
		ret = JS_EvaluateScript(svg_js->js_ctx, __this, txt->textContent, (u32) strlen(txt->textContent), 0, 0, &rval);
	}

	/*check any pending exception if outer-most event*/
	if (!prev_event && JS_IsExceptionPending(svg_js->js_ctx)) {
		JS_ReportPendingException(svg_js->js_ctx);
		JS_ClearPendingException(svg_js->js_ctx);
	}

	SMJS_SET_PRIVATE(svg_js->js_ctx, svg_js->event, prev_event);
	if (txt && hdl) hdl->js_fun=0;

	while (svg_js->force_gc) {
		svg_js->force_gc = GF_FALSE;
		gf_sg_js_call_gc(svg_js->js_ctx);
	}
	svg_js->in_script = GF_FALSE;

	gf_sg_lock_javascript(svg_js->js_ctx, GF_FALSE);

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

	if (ret==JS_FALSE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("SVG: Invalid event handler script\n" ));
		return GF_FALSE;
	}
	return GF_TRUE;
}

#endif

#endif
