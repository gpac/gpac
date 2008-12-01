/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *				Copyright (c) 2005-200X ENST
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

#include <jsapi.h> 

#define JSVAL_CHECK_STRING(_v) (JSVAL_IS_STRING(_v) || JSVAL_IS_NULL(_v))
#define JSVAL_GET_STRING(_v) (JSVAL_IS_NULL(_v) ? NULL : JS_GetStringBytes(JSVAL_TO_STRING(_v)) )

static Bool svg_script_execute_handler(GF_Node *node, GF_DOM_Event *event, GF_Node *observer);


jsval dom_element_construct(JSContext *c, GF_Node *n);
void dom_element_finalize(JSContext *c, JSObject *obj);
void dom_document_finalize(JSContext *c, JSObject *obj);

GF_Node *dom_get_element(JSContext *c, JSObject *obj);
GF_SceneGraph *dom_get_doc(JSContext *c, JSObject *obj);

JSBool js_has_instance(JSContext *c, JSObject *obj, jsval val, JSBool *vp);
JSBool dom_event_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool dom_event_remove_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

char *js_get_utf8(jsval val);

void dom_node_set_textContent(GF_Node *n, char *text);
char *dom_node_flatten_text(GF_Node *n);

jsval dom_node_get_sibling(JSContext *c, GF_Node *n, Bool is_prev, Bool elt_only);




#define _ScriptMessage(_sg, _e, _msg) {\
			GF_JSAPIParam par;	\
			par.info.e = _e;			\
			par.info.msg = _msg;		\
			_sg->script_action(_sg->script_action_cbck, GF_JSAPI_OP_MESSAGE, NULL, &par);\
		}

static GFINLINE Bool ScriptAction(GF_SceneGraph *scene, u32 type, GF_Node *node, GF_JSAPIParam *param)
{
	if (scene->script_action) 
		return scene->script_action(scene->script_action_cbck, type, node, param);
	return 0;
}

typedef struct
{
	u32 nb_inst;
	/*SVG uDOM classes*/
	JSClass svgElement;
	JSClass svgDocument;
	JSClass globalClass;
	JSClass connectionClass;
	JSClass rgbClass;
	JSClass rectClass;
	JSClass pointClass;
	JSClass pathClass;
	JSClass matrixClass;
} GF_SVGuDOM;
static GF_SVGuDOM *svg_rt = NULL;

static JSObject *svg_new_path_object(JSContext *c, SVG_PathData *d);


static void svg_node_changed(GF_Node *n, GF_FieldInfo *info)
{
	if (!info) {
		gf_node_changed(n, NULL);
	} else {
		u32 flag = gf_svg_get_modification_flags((SVG_Element *)n, info);
		gf_node_dirty_set(n, flag, 0);
	}
	/*trigger rendering*/
	if (n->sgprivate->scenegraph->NodeCallback)
		n->sgprivate->scenegraph->NodeCallback(n->sgprivate->scenegraph->userpriv, GF_SG_CALLBACK_MODIFIED, n, info);
}


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

static JSBool svg_connection_create(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static JSBool svg_nav_to_location(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_SceneGraph *sg;
	if ((argc!=1) || !JS_InstanceOf(c, obj, &svg_rt->globalClass, NULL)) return JS_TRUE;
	sg = JS_GetContextPrivate(c);
	par.uri.url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	par.uri.nb_params = 0;
	ScriptAction(sg, GF_JSAPI_OP_LOAD_URL, sg->RootNode, &par);
	return JS_TRUE;
}

static JSBool svg_parse_xml(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_SceneGraph *sg;
	JSObject *doc_obj;
	GF_Node *node;
	char *str;
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
	free(str);
	*rval = dom_element_construct(c, node);

	return JS_TRUE;
}

static void svg_script_error(JSContext *c, const char *msg, JSErrorReport *jserr)
{
	GF_SceneGraph *sg = JS_GetContextPrivate(c);
	_ScriptMessage(sg, GF_SCRIPT_ERROR, msg);
}

static JSBool svg_echo(JSContext *c, JSObject *p, uintN argc, jsval *argv, jsval *rval)
{
	u32 i;
	char buf[5000];
	GF_SceneGraph *sg = JS_GetContextPrivate(c);
	if (!sg) return JS_TRUE;

	strcpy(buf, "");
	for (i = 0; i < argc; i++) {
		jschar*utf;
		JSString *str = JS_ValueToString(c, argv[i]);
		if (!str) return JS_TRUE;
		if (i) strcat(buf, " ");
		utf = JS_GetStringChars(str);
		strcat(buf, JS_GetStringBytes(str));
	}
	_ScriptMessage(sg, GF_SCRIPT_INFO, buf);
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

static JSBool global_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_SceneGraph *sg;
	if (!JS_InstanceOf(c, obj, &svg_rt->globalClass, NULL) ) 
		return JS_TRUE;

	sg = JS_GetContextPrivate(c);
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
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
static JSBool dom_imp_has_feature(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	if (argc) {
		u32 len;
		char sep;
		char *fname = JS_GetStringBytes(JS_ValueToString(c, argv[0]));
		if (!fname) return JS_TRUE;
		while (strchr(" \t\n\r", fname[0])) fname++;
		len = strlen(fname);
		while (len && strchr(" \t\n\r", fname[len-1])) len--;
		sep = fname[len];
		fname[len] = 0;
		if (!stricmp(fname, "xml")) *rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		else if (!stricmp(fname, "core")) *rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		else if (!stricmp(fname, "traversal")) *rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		else if (!stricmp(fname, "uievents")) *rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		else if (!stricmp(fname, "mouseevents")) *rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		else if (!stricmp(fname, "mutationevents")) *rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		else if (!stricmp(fname, "events")) *rval = BOOLEAN_TO_JSVAL(JS_TRUE);
		
		fname[len] = sep;
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
		t = gf_list_get(n->sgprivate->scenegraph->use_stack, i-2);
		if (t==n) {
			GF_Node *use = gf_list_get(n->sgprivate->scenegraph->use_stack, i-1);
			GF_Node *par_use = get_corresponding_use(use);
			return par_use ? par_use : use;
		}
	}
	/*otherwise recursively get up the tree*/
	return get_corresponding_use(gf_node_get_parent(n, 0));
}
static JSBool svg_doc_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_TRUE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);
	switch (prop_id) {
	case 0:/*global*/
		*vp = OBJECT_TO_JSVAL( JS_GetGlobalObject(c) ); 
		return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool svg_element_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	GF_JSAPIParam par;
	jsdouble *d;
	JSString *s;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);


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
	case 1:/*firstElementChild*/
		*vp = JSVAL_NULL;
		if (n->sgprivate->tag!=TAG_DOMText) {
			GF_ChildNodeItem *child = ((GF_ParentNode*)n)->children;
			while (child) {
				if (child->node->sgprivate->tag != TAG_DOMText) {
					*vp = dom_element_construct(c, child->node);
					break;
				}
				child = child->next;
			}
		}
		return JS_TRUE;
	case 2:/*lastElementChild*/
		*vp = JSVAL_NULL;
		if (n->sgprivate->tag!=TAG_DOMText) {
			GF_Node *last = NULL;
			GF_ChildNodeItem *child = ((GF_ParentNode*)n)->children;
			while (child) {
				if (child->node->sgprivate->tag != TAG_DOMText) {
					last = child->node;
				}
				child = child->next;
			}
			if (last) *vp = dom_element_construct(c, last);
		}
		return JS_TRUE;
	case 3:/*previousElementSibling*/
		*vp = dom_node_get_sibling(c, n, 1, 1);
		return JS_TRUE;
	case 4:/*nextElementSibling*/
		*vp = dom_node_get_sibling(c, n, 0, 1);
		return JS_TRUE;

	case 5:/*currentScale*/
		if (n->sgprivate->tag!=TAG_SVG_svg) return JS_TRUE;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_SCALE, (GF_Node *)n, &par)) {
			d = JS_NewDouble(c, FIX2FLT(par.val) );
			*vp = DOUBLE_TO_JSVAL(d);
			return JS_TRUE;
		}
		return JS_TRUE;
	case 6:/*currentRotate*/ 
		if (n->sgprivate->tag!=TAG_SVG_svg) return JS_TRUE;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_ROTATION, (GF_Node *)n, &par)) {
			d = JS_NewDouble(c, FIX2FLT(par.val) );
			*vp = DOUBLE_TO_JSVAL(d);
			return JS_TRUE;
		}
		return JS_TRUE;
	case 7:/*currentTranslate*/
		if (n->sgprivate->tag!=TAG_SVG_svg) return JS_TRUE;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_TRANSLATE, (GF_Node *)n, &par)) {
			JSObject *r = JS_NewObject(c, &svg_rt->pointClass, 0, 0);
			pointCI *rc = malloc(sizeof(pointCI));
			rc->x = FIX2FLT(par.pt.x);
			rc->y = FIX2FLT(par.pt.y);
			rc->sg = n->sgprivate->scenegraph;
			JS_SetPrivate(c, r, rc);
			*vp = OBJECT_TO_JSVAL(r);
			return JS_TRUE;
		}
		return JS_TRUE;
	case 8:/*viewport*/
		if (n->sgprivate->tag!=TAG_SVG_svg) return JS_TRUE;
		if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_VIEWPORT, (GF_Node *)n, &par)) {
			JSObject *r = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
			rectCI *rc = malloc(sizeof(rectCI));
			rc->x = FIX2FLT(par.rc.x);
			rc->y = FIX2FLT(par.rc.y);
			rc->w = FIX2FLT(par.rc.width);
			rc->h = FIX2FLT(par.rc.height);
			rc->sg = n->sgprivate->scenegraph;
			JS_SetPrivate(c, r, rc);
			*vp = OBJECT_TO_JSVAL(r);
			return JS_TRUE;
		}
		return JS_TRUE;
	case 9:/*currentTime*/ 
		d = JS_NewDouble(c, gf_node_get_scene_time((GF_Node *)n) );
		*vp = DOUBLE_TO_JSVAL(d);
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

static JSBool svg_element_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_JSAPIParam par;
	jsdouble d;
	u32 prop_id;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);

	switch (prop_id) {
	case 0:/*id*/
		if (JSVAL_CHECK_STRING(*vp)) {
			char *id = JSVAL_GET_STRING(*vp);
			if (id) {
				GF_FieldInfo info;
				u32 nid = gf_node_get_id(n);
				if (!nid) nid = gf_sg_get_next_available_node_id(n->sgprivate->scenegraph);
				gf_node_set_id(n, nid, id);
				if (gf_node_get_attribute_by_tag(n, TAG_XML_ATT_id, 1, 0, &info)==GF_OK) {
					if (*(DOM_String *)info.far_ptr) free(*(DOM_String *)info.far_ptr);
					*(DOM_String *)info.far_ptr = strdup(id);
				}
				if (gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_id, 1, 0, &info)==GF_OK) {
					if (*(DOM_String *)info.far_ptr) free(*(DOM_String *)info.far_ptr);
					*(DOM_String *)info.far_ptr = strdup(id);
				}
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
	/*not sure about this one...*/
	case TAG_SVG_discard:
		return n;
	}
	return NULL;
}


/*TODO*/
JSBool svg_udom_smil_time_insert(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, Bool is_end)
{
	GF_FieldInfo info;
	u32 i, count;
	Double offset;
	GF_List *times;
	SMIL_Time *newtime;
	GF_Node *n = svg_udom_smil_check_instance(c, obj);
	if (!n) return JS_TRUE;
	

	if (is_end) {
		info.far_ptr = ((SVGTimedAnimBaseElement *)n)->timingp->end;
	} else {
		info.far_ptr = ((SVGTimedAnimBaseElement *)n)->timingp->begin;
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

JSBool svg_udom_smil_begin(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return svg_udom_smil_time_insert(c, obj, argc, argv, rval, 0);
}

JSBool svg_udom_smil_end(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return svg_udom_smil_time_insert(c, obj, argc, argv, rval, 1);
}

/*TODO*/
JSBool svg_udom_smil_pause(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag;
	GF_Node *n = dom_get_element(c, obj);

	tag = gf_node_get_tag(n);
	if (gf_svg_is_animation_tag(tag)) {
		/* pausing an animation element (set, animate ...) should pause the main time line ? */
		gf_smil_timing_pause(n);
	} else if (gf_svg_is_timing_tag(tag)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_PAUSE_SVG, n, NULL);
	} else {
		return JS_TRUE;
	}
	return JS_TRUE;
}
/*TODO*/
JSBool svg_udom_smil_resume(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag;
	GF_Node *n = dom_get_element(c, obj);

	tag = gf_node_get_tag(n);
	if (gf_svg_is_animation_tag(tag)) {
		/* resuming an animation element (set, animate ...) should resume the main time line ? */
		gf_smil_timing_resume(n);
	} else if (gf_svg_is_timing_tag(tag)) {
		ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_RESUME_SVG, n, NULL);
	} else {
		return JS_TRUE;
	}
	return JS_TRUE;
}

JSBool svg_udom_get_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char attValue[1024], *ns, *name;
	GF_Err e;
	GF_FieldInfo info;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	ns = name  =NULL;
	if (! JSVAL_CHECK_STRING(argv[0]) ) return JS_TRUE;
	if (argc==2) {
		ns = JSVAL_GET_STRING(argv[0]);
		name = JSVAL_GET_STRING(argv[1]);
	} else if (argc==1) {
		name = JSVAL_GET_STRING(argv[0]);
	} else return JS_TRUE;

	if (!name) return JS_TRUE;
	if (!strcmp(name, "#text")) {
		char *res = dom_node_flatten_text(n);
		*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, res) );
		free(res);
		return JS_TRUE;
	}
	e = gf_node_get_field_by_name(n, name, &info);

	if (e!=GF_OK) return JS_TRUE;
	*rval = JSVAL_VOID;

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
		gf_svg_dump_attribute(n, &info, attValue);
		*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, attValue) );
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

JSBool svg_udom_get_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	GF_FieldInfo info;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_attribute_by_name(n, szName, 0, 1, 1, &info) != GF_OK) return JS_TRUE;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_Number_datatype:
	case SVG_FontSize_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	{
		SVG_Number *l = (SVG_Number *)info.far_ptr;
		if (l->type==SVG_NUMBER_AUTO || l->type==SVG_NUMBER_INHERIT) return JS_TRUE;
		*rval = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(l->value) ) );
		return JS_TRUE;
	}
	case SVG_Clock_datatype:
		*rval = DOUBLE_TO_JSVAL( JS_NewDouble(c, *(SVG_Clock*)info.far_ptr ) );
		return JS_TRUE;
	default:
		return JS_TRUE;
	}
	return JS_TRUE;
}

JSBool svg_udom_get_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	JSObject *mO;
	GF_FieldInfo info;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_Transform_datatype) {
		GF_Matrix2D *mx = malloc(sizeof(GF_Matrix2D));
		mO = JS_NewObject(c, &svg_rt->matrixClass, 0, 0);
		gf_mx2d_init(*mx);
		gf_mx2d_copy(*mx, ((SVG_Transform*)info.far_ptr)->mat);

		JS_SetPrivate(c, mO, mx);
		*rval = OBJECT_TO_JSVAL(mO);
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool svg_udom_get_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *newObj;
	char *szName;
	GF_FieldInfo info;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_ViewBox_datatype) {
		rectCI *rc;
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		newObj = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
		GF_SAFEALLOC(rc, rectCI);
		rc->x = FIX2FLT(v->x);
		rc->y = FIX2FLT(v->y);
		rc->w = FIX2FLT(v->width);
		rc->h = FIX2FLT(v->height);
		JS_SetPrivate(c, newObj, rc);
		*rval = OBJECT_TO_JSVAL(newObj);
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool svg_udom_get_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	GF_FieldInfo info;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_PathData_datatype) {
		*rval = OBJECT_TO_JSVAL( svg_new_path_object(c, info.far_ptr) );
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool svg_udom_get_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	GF_FieldInfo info;
	rgbCI *rgb;
	JSObject *newObj;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=1) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_TRUE;

	switch (info.fieldType) {
	case SVG_Color_datatype:
	{
		SVG_Color *col = (SVG_Color *)info.far_ptr;
		if (col->type == SVG_COLOR_CURRENTCOLOR) return JS_TRUE;
		if (col->type == SVG_COLOR_INHERIT) return JS_TRUE;
		newObj = JS_NewObject(c, &svg_rt->rgbClass, 0, 0);
		GF_SAFEALLOC(rgb, rgbCI);
		rgb->r = (u8) (255*FIX2FLT(col->red)) ;
		rgb->g = (u8) (255*FIX2FLT(col->green)) ;
		rgb->b = (u8) (255*FIX2FLT(col->blue)) ;
		JS_SetPrivate(c, newObj, rgb);
		*rval = OBJECT_TO_JSVAL(newObj);
		return JS_TRUE;
	}
		break;
	case SVG_Paint_datatype:
	{
		SVG_Paint *paint = (SVG_Paint *)info.far_ptr;
		if (1 || paint->type==SVG_PAINT_COLOR) {
			newObj = JS_NewObject(c, &svg_rt->rgbClass, 0, 0);
			GF_SAFEALLOC(rgb, rgbCI);
			rgb->r = (u8) (255*FIX2FLT(paint->color.red) );
			rgb->g = (u8) (255*FIX2FLT(paint->color.green) );
			rgb->b = (u8) (255*FIX2FLT(paint->color.blue) );
			JS_SetPrivate(c, newObj, rgb);
			*rval = OBJECT_TO_JSVAL(newObj);
			return JS_TRUE;
		}
		return JS_TRUE;
	}
	}
	return JS_TRUE;
}

JSBool svg_udom_set_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *ns, *name, *val;
	GF_Err e;
	GF_FieldInfo info;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	val = ns = name = NULL;
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	if (argc==3) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		if (!JSVAL_CHECK_STRING(argv[2])) return JS_TRUE;
		ns = JSVAL_GET_STRING(argv[0]);
		name = JSVAL_GET_STRING(argv[1]);
		val = JS_GetStringBytes(JSVAL_TO_STRING(argv[2]));
	} else if (argc==2) {
		name = JSVAL_GET_STRING(argv[0]);
		val = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	} else 
		return JS_TRUE;
	if (!name)  return JS_TRUE;
	if (!strcmp(name, "#text")) {
		if (!JSVAL_IS_STRING(argv[1])) return JS_TRUE;
		dom_node_set_textContent(n, val);
		return JS_TRUE;
	}
	e = gf_node_get_field_by_name(n, name, &info);


	if (!val || (e!=GF_OK)) return JS_TRUE;
	*rval = JSVAL_VOID;

	e = gf_svg_parse_attribute(n, &info, val, 0);
	if (e) return dom_throw_exception(c, GF_DOM_EXC_INVALID_ACCESS_ERR);

	svg_node_changed(n, &info);

	return JS_TRUE;
}

JSBool svg_udom_set_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_FieldInfo info;
	jsdouble d;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_IS_NUMBER(argv[1])) return JS_TRUE;
	JS_ValueToNumber(c, argv[1], &d);
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info) != GF_OK) return JS_TRUE;

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
			val = gf_list_get(*l, 0);
			gf_list_rem(*l, 0);
			free(val);
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
	svg_node_changed(n, &info);
	return JS_TRUE;
}
JSBool svg_udom_set_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *mO;
	char *szName;
	GF_FieldInfo info;
	GF_Matrix2D *mx;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (JSVAL_IS_NULL(argv[1]) || !JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	mO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, mO, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx = JS_GetPrivate(c, mO);
	if (!mx) return JS_TRUE;

	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_Transform_datatype) {
		gf_mx2d_copy(((SVG_Transform*)info.far_ptr)->mat, *mx);
		svg_node_changed(n, NULL);
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool svg_udom_set_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *rO;
	char *szName;
	GF_FieldInfo info;
	rectCI *rc;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (JSVAL_IS_NULL(argv[1]) || !JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	rO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, rO, &svg_rt->rectClass, NULL) ) return JS_TRUE;
	rc = JS_GetPrivate(c, rO);
	if (!rc) return JS_TRUE;

	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_ViewBox_datatype) {
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		v->x = FLT2FIX(rc->x);
		v->y = FLT2FIX(rc->y);
		v->width = FLT2FIX(rc->w);
		v->height = FLT2FIX(rc->h);
		svg_node_changed(n, NULL);
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool svg_udom_set_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *path;
	GF_FieldInfo info;
	JSObject *pO;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	if (JSVAL_IS_NULL(argv[1]) || !JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	pO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, pO, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	path = JS_GetPrivate(c, pO);
	if (!path) return JS_TRUE;
	if (gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info) != GF_OK) return JS_TRUE;

	if (info.fieldType==SVG_PathData_datatype) {
#if USE_GF_PATH
#else
		u32 i;
		u32 nb_pts;
		SVG_PathData *d = (SVG_PathData *)info.far_ptr;
		while (gf_list_count(d->commands)) {
			u8 *t = gf_list_get(d->commands, 0);
			gf_list_rem(d->commands, 0);
			free(t);
		}
		while (gf_list_count(d->points)) {
			SVG_Point *t = gf_list_get(d->points, 0);
			gf_list_rem(d->points, 0);
			free(t);
		}
		nb_pts = 0;
		for (i=0; i<path->nb_coms; i++) {
			u8 *t = malloc(sizeof(u8));
			*t = path->tags[i];
			gf_list_add(d->commands, t);
			switch (*t) {
			case 0:
			case 1: nb_pts++; break;
			case 2: nb_pts+=3; break;
			case 4: nb_pts+=2; break;
			}
		}
		for (i=0; i<nb_pts; i++) {
			SVG_Point *t = malloc(sizeof(SVG_Point));
			t->x = FLT2FIX(path->pts[i].x);
			t->y = FLT2FIX(path->pts[i].y);
			gf_list_add(d->points, t);
		}
		svg_node_changed(n, NULL);
		return JS_TRUE;
#endif
	}
	return JS_TRUE;
}

JSBool svg_udom_set_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_FieldInfo info;
	rgbCI *rgb;
	JSObject *colO;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	if (argc!=2) return JS_TRUE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	colO = JSVAL_TO_OBJECT(argv[1]);
	if (!colO) return JS_TRUE;
	if (!JS_InstanceOf(c, colO, &svg_rt->rgbClass, NULL) ) return JS_TRUE;
	rgb = JS_GetPrivate(c, colO);
	if (!rgb) return JS_TRUE;

	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info) != GF_OK) return JS_TRUE;

	switch (info.fieldType) {
	case SVG_Color_datatype:
	{
		SVG_Color *col = (SVG_Color *)info.far_ptr;
		col->type = SVG_COLOR_RGBCOLOR;
		col->red = FLT2FIX(rgb->r / 255.0f);
		col->green = FLT2FIX(rgb->g / 255.0f);
		col->blue = FLT2FIX(rgb->b / 255.0f);
		svg_node_changed(n, &info);
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
		svg_node_changed(n, &info);
		return JS_TRUE;
	}
	}
	return JS_TRUE;
}

static JSBool svg_get_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, Bool get_screen)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	par.bbox.is_set = 0;
	if (ScriptAction(n->sgprivate->scenegraph, get_screen ? GF_JSAPI_OP_GET_SCREEN_BBOX : GF_JSAPI_OP_GET_LOCAL_BBOX, (GF_Node *)n, &par) ) {
		if (par.bbox.is_set) {
			JSObject *rO = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
			rectCI *rc = malloc(sizeof(rectCI));
			rc->sg = NULL;
			rc->x = FIX2FLT(par.bbox.min_edge.x);
			/*BBox is in 3D coord system style*/
			rc->y = FIX2FLT(par.bbox.min_edge.y);
			rc->w = FIX2FLT(par.bbox.max_edge.x - par.bbox.min_edge.x);
			rc->h = FIX2FLT(par.bbox.max_edge.y - par.bbox.min_edge.y);
			JS_SetPrivate(c, rO, rc);
			*rval = OBJECT_TO_JSVAL(rO);
		} else {
			*rval = JSVAL_VOID;
		}
		return JS_TRUE;
	}
	return JS_TRUE;
}
JSBool svg_udom_get_local_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return svg_get_bbox(c, obj, argc, argv, rval, 0);
}
JSBool svg_udom_get_screen_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return svg_get_bbox(c, obj, argc, argv, rval, 1);
}

JSBool svg_udom_get_screen_ctm(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_TRANSFORM, (GF_Node *)n, &par)) {
		JSObject *mO = JS_NewObject(c, &svg_rt->matrixClass, 0, 0);
		GF_Matrix2D *mx = malloc(sizeof(GF_Matrix2D));
		gf_mx2d_from_mx(mx, &par.mx);
		JS_SetPrivate(c, mO, mx);
		*rval = OBJECT_TO_JSVAL(mO);
		return JS_TRUE;
	}
	return JS_TRUE;
}

JSBool svg_udom_create_matrix_components(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Matrix2D *mx;
	JSObject *mat;
	jsdouble v;
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
	mat = JS_NewObject(c, &svg_rt->matrixClass, 0, 0);
	JS_SetPrivate(c, mat, mx);
	*rval = OBJECT_TO_JSVAL(mat);
	return JS_TRUE;
}
JSBool svg_udom_create_rect(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rectCI *rc;
	JSObject *r;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	GF_SAFEALLOC(rc, rectCI);
	r = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
	JS_SetPrivate(c, r, rc);
	*rval = OBJECT_TO_JSVAL(r);
	return JS_TRUE;
}
JSBool svg_udom_create_point(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pointCI *pt;
	JSObject *r;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	GF_SAFEALLOC(pt, pointCI);
	r = JS_NewObject(c, &svg_rt->pointClass, 0, 0);
	JS_SetPrivate(c, r, pt);
	*rval = OBJECT_TO_JSVAL(r);
	return JS_TRUE;
}
JSBool svg_udom_create_path(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *path;
	JSObject *p;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;

	GF_SAFEALLOC(path, pathCI);
	p = JS_NewObject(c, &svg_rt->pathClass, 0, 0);
	JS_SetPrivate(c, p, path);
	*rval = OBJECT_TO_JSVAL(p);
	return JS_TRUE;
}
JSBool svg_udom_create_color(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rgbCI *col;
	JSObject *p;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;
	if (argc!=3) return JS_TRUE;

	GF_SAFEALLOC(col, rgbCI);
	col->r = JSVAL_TO_INT(argv[0]);
	col->g = JSVAL_TO_INT(argv[1]);
	col->b = JSVAL_TO_INT(argv[2]);
	p = JS_NewObject(c, &svg_rt->rgbClass, 0, 0);
	JS_SetPrivate(c, p, col);
	*rval = OBJECT_TO_JSVAL(p);
	return JS_TRUE;
}

JSBool svg_udom_move_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;
	if ((argc!=1) || !JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;

	par.opt = JSVAL_TO_INT(argv[1]);
	if (ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_SET_FOCUS, (GF_Node *)n, &par)) 
		return JS_TRUE;
	return JS_TRUE;
}
JSBool svg_udom_set_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
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
JSBool svg_udom_get_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_Node *n = dom_get_element(c, obj);
	if (!n || argc) return JS_TRUE;
	
	*rval = JSVAL_VOID;
	if (!ScriptAction(n->sgprivate->scenegraph, GF_JSAPI_OP_GET_FOCUS, (GF_Node *)n, &par)) 
		return JS_TRUE;

	if (par.node) {
		*rval = dom_element_construct(c, par.node);
	}
	return JS_TRUE;
}

JSBool svg_udom_get_time(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	jsdouble *d;
	GF_Node *n = dom_get_element(c, obj);
	if (!n) return JS_TRUE;

	d = JS_NewDouble(c, gf_node_get_scene_time(n));
	*rval = DOUBLE_TO_JSVAL(d);
	return JS_TRUE;
}



static JSBool svg_connection_set_encoding(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_TRUE;
}
static JSBool svg_connection_create(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
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
	{"connected",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};
static JSFunctionSpec connectionFuncs[] = {
	/*eventTarget interface*/
	{"addEventListenerNS", dom_event_add_listener, 4},
	{"removeEventListenerNS", dom_event_remove_listener, 4},
	{"addEventListenerNS", dom_event_add_listener, 3},
	{"removeEventListenerNS", dom_event_remove_listener, 3},
	/*connection interface*/
	{"setEncoding", svg_connection_set_encoding, 1},
	{"connect", svg_connection_connect, 1},
	{"send", svg_connection_send, 1},
	{"close", svg_connection_close, 0},
	{0}
};

static void baseCI_finalize(JSContext *c, JSObject *obj)
{
	void *data = JS_GetPrivate(c, obj);
	if (data) free(data);
}

static JSBool rgb_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->rgbClass, NULL) ) return JS_TRUE;
	if (JSVAL_IS_INT(id)) {
		rgbCI *col = JS_GetPrivate(c, obj);
		if (!col) return JS_TRUE;
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = INT_TO_JSVAL(col->r); return JS_TRUE;
		case 1: *vp = INT_TO_JSVAL(col->g); return JS_TRUE;
		case 2: *vp = INT_TO_JSVAL(col->b); return JS_TRUE;
		default:
			return JS_TRUE;
		}
	}
	return JS_TRUE;
}
static JSBool rgb_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->rgbClass, NULL) ) return JS_TRUE;
	if (JSVAL_IS_INT(id)) {
		rgbCI *col = JS_GetPrivate(c, obj);
		if (!col) return JS_TRUE;
		switch (JSVAL_TO_INT(id)) {
		case 0: col->r = JSVAL_TO_INT(*vp); return JS_TRUE;
		case 1: col->g = JSVAL_TO_INT(*vp); return JS_TRUE;
		case 2: col->b = JSVAL_TO_INT(*vp); return JS_TRUE;
		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}


static JSBool rect_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->rectClass, NULL) ) return JS_TRUE;
	if (JSVAL_IS_INT(id)) {
		jsdouble *d;
		rectCI *rc = JS_GetPrivate(c, obj);
		if (!rc) return JS_TRUE;
		if (rc->sg) {
			GF_JSAPIParam par;
			ScriptAction(rc->sg, GF_JSAPI_OP_GET_VIEWPORT, rc->sg->RootNode, &par);
			rc->x = FIX2FLT(par.rc.x);
			rc->y = FIX2FLT(par.rc.y);
			rc->w = FIX2FLT(par.rc.width);
			rc->h = FIX2FLT(par.rc.height);
		}
		switch (JSVAL_TO_INT(id)) {
		case 0: d = JS_NewDouble(c, rc->x); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		case 1: d = JS_NewDouble(c, rc->y); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		case 2: d = JS_NewDouble(c, rc->w); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		case 3: d = JS_NewDouble(c, rc->h); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}
static JSBool rect_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->rectClass, NULL) ) return JS_TRUE;
	if (JSVAL_IS_INT(id)) {
		jsdouble d;
		rectCI *rc = JS_GetPrivate(c, obj);
		if (!rc) return JS_TRUE;
		JS_ValueToNumber(c, *vp, &d);
		switch (JSVAL_TO_INT(id)) {
		case 0: rc->x = (Float) d; return JS_TRUE;
		case 1: rc->y = (Float) d; return JS_TRUE;
		case 2: rc->w = (Float) d; return JS_TRUE;
		case 3: rc->h = (Float) d; return JS_TRUE;
		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}

static JSBool point_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->pointClass, NULL) ) return JS_TRUE;
	if (JSVAL_IS_INT(id)) {
		jsdouble *d;
		pointCI *pt = JS_GetPrivate(c, obj);
		if (!pt) return JS_TRUE;
		if (pt->sg) {
			GF_JSAPIParam par;
			ScriptAction(pt->sg, GF_JSAPI_OP_GET_TRANSLATE, pt->sg->RootNode, &par);
			pt->x = FIX2FLT(par.pt.x);
			pt->y = FIX2FLT(par.pt.y);
		}
		switch (JSVAL_TO_INT(id)) {
		case 0: d = JS_NewDouble(c, pt->x); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		case 1: d = JS_NewDouble(c, pt->y); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}
static JSBool point_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->pointClass, NULL) ) return JS_TRUE;
	if (JSVAL_IS_INT(id)) {
		jsdouble d;
		pointCI *pt = JS_GetPrivate(c, obj);
		if (!pt) return JS_TRUE;
		JS_ValueToNumber(c, *vp, &d);
		switch (JSVAL_TO_INT(id)) {
		case 0: pt->x = (Float) d; break;
		case 1: pt->y = (Float) d; break;
		default: return JS_TRUE;
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
		p->tags = malloc(sizeof(u8) * p->nb_coms);
		for (i=0; i<p->nb_coms; i++) p->tags[i] = * (u8 *) gf_list_get(d->commands, i);
		count = gf_list_count(d->points);
		p->pts = malloc(sizeof(pointCI) * count);
		for (i=0; i<count; i++) {
			GF_Point2D *pt = gf_list_get(d->commands, i);
			p->pts[i].x = FIX2FLT(pt->x);
			p->pts[i].y = FIX2FLT(pt->y);
		}
	}
	obj = JS_NewObject(c, &svg_rt->pathClass, 0, 0);
	JS_SetPrivate(c, obj, p);
	return obj;
#endif
}

static JSBool pathCI_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *p;
	GF_SAFEALLOC(p, pathCI);
	JS_SetPrivate(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static void pathCI_finalize(JSContext *c, JSObject *obj)
{
	pathCI *p = JS_GetPrivate(c, obj);
	if (p) {
		if (p->pts) free(p->pts);
		if (p->tags) free(p->tags);
		free(p);
	}
}

static JSBool path_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	if (JSVAL_IS_INT(id)) {
		pathCI *p = JS_GetPrivate(c, obj);
		if (!p) return JS_TRUE;
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = INT_TO_JSVAL(p->nb_coms); return JS_TRUE;
		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}
static JSBool svg_path_get_segment(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	u32 idx;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=1) || !JSVAL_IS_INT(argv[0])) return JS_TRUE;
	idx = JSVAL_TO_INT(argv[0]);
	if (idx>=p->nb_coms) return JS_TRUE;
	switch (p->tags[idx]) {
	case 0: *vp = INT_TO_JSVAL(77); return JS_TRUE;	/* Move To */
	case 1: *vp = INT_TO_JSVAL(76); return JS_TRUE;	/* Line To */
	case 2:/* Curve To */
	case 3:/* next Curve To */
		*vp = INT_TO_JSVAL(67); return JS_TRUE;	
	case 4:/* Quad To */
	case 5:/* next Quad To */
		*vp = INT_TO_JSVAL(81); return JS_TRUE;	
	case 6: *vp = INT_TO_JSVAL(90); return JS_TRUE;	/* Close */
	}
	return JS_TRUE;
}

static JSBool svg_path_get_segment_param(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble *d;
	pathCI *p;
	ptCI *pt;
	u32 i, idx, param_idx, pt_idx;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=2) || !JSVAL_IS_INT(argv[0]) || !JSVAL_IS_INT(argv[1])) return JS_TRUE;
	idx = JSVAL_TO_INT(argv[0]);
	param_idx = JSVAL_TO_INT(argv[1]);
	if (idx>=p->nb_coms) return JS_TRUE;
	pt_idx = 0;
	for (i=0; i<idx; i++) {
		switch (p->tags[i]) {
		case 0: pt_idx++; break;
		case 1: pt_idx++; break;
		case 2: pt_idx+=3; break;
		case 3: pt_idx+=2; break;
		case 4: pt_idx+=2; break;
		case 5: pt_idx+=1; break;
		}
	}
	switch (p->tags[idx]) {
	case 0: 
	case 1: 
		if (param_idx>1) return JS_TRUE;
		pt = &p->pts[pt_idx];
		d = JS_NewDouble(c, param_idx ? pt->y : pt->x); 
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 2:/* Curve To */
		if (param_idx>5) return JS_TRUE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		d = JS_NewDouble(c, (param_idx%2) ? pt->y : pt->x);
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 3:/* Next Curve To */
		if (param_idx>5) return JS_TRUE;
		if (param_idx<2) {
			pt = &p->pts[pt_idx - 1];
			d = JS_NewDouble(c, param_idx ? pt->y : pt->x);
		} else {
			param_idx-=2;
			pt = &p->pts[pt_idx + (param_idx/2)];
			d = JS_NewDouble(c, (param_idx%2) ? pt->y : pt->x);
		}
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 4:/* Quad To */
		if (param_idx>3) return JS_TRUE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		d = JS_NewDouble(c, (param_idx%2) ? pt->y : pt->x);
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 5:/* Next Quad To */
		if (param_idx>3) return JS_TRUE;
		if (param_idx<2) {
			pt = &p->pts[pt_idx - 1];
			d = JS_NewDouble(c, param_idx ? pt->y : pt->x);
		} else {
			param_idx-=2;
			pt = &p->pts[pt_idx];
			d = JS_NewDouble(c, param_idx ? pt->y : pt->x);
		}
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
		/*spec is quite obscur here*/
	case 6:
		d = JS_NewDouble(c, 0); 
		*vp = DOUBLE_TO_JSVAL(d);  
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
		case 0: orig_pts++; break;
		case 1: orig_pts++; break;
		case 2: orig_pts+=3; break;
		case 3: orig_pts+=2; break;
		case 4: orig_pts+=2; break;
		case 5: orig_pts+=1; break;
		}
	}
	p->pts = realloc(p->pts, sizeof(ptCI)*(nb_pts+orig_pts));
	return orig_pts;
}
static JSBool svg_path_move_to(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	jsdouble x, y;
	u32 nb_pts;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=2) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);
	nb_pts = svg_path_realloc_pts(p, 1);
	p->pts[nb_pts].x = (Float) x;
	p->pts[nb_pts].y = (Float) y;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 0;
	p->nb_coms++;
	return JS_TRUE;
}
static JSBool svg_path_line_to(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	jsdouble x, y;
	u32 nb_pts;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=2) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);
	nb_pts = svg_path_realloc_pts(p, 1);
	p->pts[nb_pts].x = (Float) x;
	p->pts[nb_pts].y = (Float) y;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 1;
	p->nb_coms++;
	return JS_TRUE;
}

static JSBool svg_path_quad_to(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	jsdouble x1, y1, x2, y2;
	u32 nb_pts;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=4) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1]) || !JSVAL_IS_NUMBER(argv[2]) || !JSVAL_IS_NUMBER(argv[3])) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x1);
	JS_ValueToNumber(c, argv[1], &y1);
	JS_ValueToNumber(c, argv[2], &x2);
	JS_ValueToNumber(c, argv[3], &y2);
	nb_pts = svg_path_realloc_pts(p, 2);
	p->pts[nb_pts].x = (Float) x1; p->pts[nb_pts].y = (Float) y1;
	p->pts[nb_pts+1].x = (Float) x2; p->pts[nb_pts+1].y = (Float) y2;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 4;
	p->nb_coms++;
	return JS_TRUE;
}
static JSBool svg_path_curve_to(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	jsdouble x1, y1, x2, y2, x, y;
	u32 nb_pts;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_TRUE;
	if ((argc!=6) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1]) || !JSVAL_IS_NUMBER(argv[2]) || !JSVAL_IS_NUMBER(argv[3]) || !JSVAL_IS_NUMBER(argv[4]) || !JSVAL_IS_NUMBER(argv[5])) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x1);
	JS_ValueToNumber(c, argv[1], &y1);
	JS_ValueToNumber(c, argv[2], &x2);
	JS_ValueToNumber(c, argv[3], &y2);
	JS_ValueToNumber(c, argv[4], &x);
	JS_ValueToNumber(c, argv[5], &y);
	nb_pts = svg_path_realloc_pts(p, 3);
	p->pts[nb_pts].x = (Float) x1; p->pts[nb_pts].y = (Float) y1;
	p->pts[nb_pts+1].x = (Float) x2; p->pts[nb_pts+1].y = (Float) y2;
	p->pts[nb_pts+2].x = (Float) x; p->pts[nb_pts+2].y = (Float) y;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 2;
	p->nb_coms++;
	return JS_TRUE;
}
static JSBool svg_path_close(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	if (!JS_InstanceOf(c, obj, &svg_rt->pathClass, NULL) ) return JS_TRUE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_TRUE;
	if (argc) return JS_TRUE;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 6;
	p->nb_coms++;
	return JS_TRUE;
}

static JSBool matrix_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	jsdouble *d;
	GF_Matrix2D *mx;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx = JS_GetPrivate(c, obj);
	if (!JSVAL_IS_INT(id)) return JS_TRUE;

	if (!mx) return JS_TRUE;
	switch (JSVAL_TO_INT(id)) {
	case 0: d = JS_NewDouble(c, FIX2FLT(mx->m[0])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 1: d = JS_NewDouble(c, FIX2FLT(mx->m[3])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 2: d = JS_NewDouble(c, FIX2FLT(mx->m[1])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 3: d = JS_NewDouble(c, FIX2FLT(mx->m[4])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 4: d = JS_NewDouble(c, FIX2FLT(mx->m[2])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 5: d = JS_NewDouble(c, FIX2FLT(mx->m[5])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	default: return JS_TRUE;
	}
	return JS_TRUE;
}
static JSBool matrix_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	jsdouble d;
	GF_Matrix2D *mx;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx = JS_GetPrivate(c, obj);
	if (!JSVAL_IS_INT(id)) return JS_TRUE;

	JS_ValueToNumber(c, *vp, &d);
	switch (JSVAL_TO_INT(id)) {
	case 0: mx->m[0] = FLT2FIX(d); break;
	case 1: mx->m[3] = FLT2FIX(d); break;
	case 2: mx->m[1] = FLT2FIX(d); break;
	case 3: mx->m[4] = FLT2FIX(d); break;
	case 4: mx->m[2] = FLT2FIX(d); break;
	case 5: mx->m[5] = FLT2FIX(d); break;
	default: return JS_TRUE;
	}
	return JS_TRUE;
}
static JSBool svg_mx2d_get_component(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble *d;
	GF_Matrix2D *mx;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx = JS_GetPrivate(c, obj);
	if (!mx || (argc!=1)) return JS_TRUE;
	if (!JSVAL_IS_INT(argv[0])) return JS_TRUE;
	switch (JSVAL_TO_INT(argv[0])) {
	case 0: d = JS_NewDouble(c, FIX2FLT(mx->m[0])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 1: d = JS_NewDouble(c, FIX2FLT(mx->m[3])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 2: d = JS_NewDouble(c, FIX2FLT(mx->m[1])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 3: d = JS_NewDouble(c, FIX2FLT(mx->m[4])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 4: d = JS_NewDouble(c, FIX2FLT(mx->m[2])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 5: d = JS_NewDouble(c, FIX2FLT(mx->m[5])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool svg_mx2d_multiply(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	JSObject *mat;
	GF_Matrix2D *mx1, *mx2;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=1)) return JS_TRUE;
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;
	mat = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(c, mat, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx2 = JS_GetPrivate(c, mat);
	if (!mx2) return JS_TRUE;
	gf_mx2d_add_matrix(mx1, mx2);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_inverse(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	GF_Matrix2D *mx1;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1) return JS_TRUE;
	gf_mx2d_inverse(mx1);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_translate(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble x, y;
	GF_Matrix2D *mx1, mx2;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=2)) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);

	gf_mx2d_init(mx2);
	mx2.m[2] = FLT2FIX(x);
	mx2.m[5] = FLT2FIX(y);
	gf_mx2d_pre_multiply(mx1, &mx2);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_scale(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble scale;
	GF_Matrix2D *mx1, mx2;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=1)) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &scale);
	gf_mx2d_init(mx2);
	mx2.m[0] = mx2.m[4] = FLT2FIX(scale);
	gf_mx2d_pre_multiply(mx1, &mx2);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_rotate(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble angle;
	GF_Matrix2D *mx1, mx2;
	if (!JS_InstanceOf(c, obj, &svg_rt->matrixClass, NULL) ) return JS_TRUE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=1)) return JS_TRUE;
	JS_ValueToNumber(c, argv[0], &angle);
	gf_mx2d_init(mx2);
	gf_mx2d_add_rotation(&mx2, 0, 0, gf_mulfix(FLT2FIX(angle/180), GF_PI));
	gf_mx2d_pre_multiply(mx1, &mx2);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

jsval svg_udom_new_rect(JSContext *c, Fixed x, Fixed y, Fixed width, Fixed height)
{
	JSObject *r = JS_NewObject(c, &svg_rt->rectClass, 0, 0);
	rectCI *rc = malloc(sizeof(rectCI));
	rc->x = FIX2FLT(x);
	rc->y = FIX2FLT(y);
	rc->w = FIX2FLT(width);
	rc->h = FIX2FLT(height);
	rc->sg = NULL;
	JS_SetPrivate(c, r, rc);
	return OBJECT_TO_JSVAL(r);
}

jsval svg_udom_new_point(JSContext *c, Fixed x, Fixed y)
{
	JSObject *p = JS_NewObject(c, &svg_rt->pointClass, 0, 0);
	pointCI *pt = malloc(sizeof(pointCI));
	pt->x = FIX2FLT(x);
	pt->y = FIX2FLT(y);
	pt->sg = NULL;
	JS_SetPrivate(c, p, pt);
	return OBJECT_TO_JSVAL(p);
}

void *svg_get_element_class(GF_Node *n)
{
	return &svg_rt->svgElement;
}
void *svg_get_document_class(GF_SceneGraph *n)
{
	return &svg_rt->svgDocument;
}


static void svg_init_js_api(GF_SceneGraph *scene)
{
	JSObject *proto;
	JS_SetContextPrivate(scene->svg_js->js_ctx, scene);
	JS_SetErrorReporter(scene->svg_js->js_ctx, svg_script_error);

	/*init global object*/
	scene->svg_js->global = JS_NewObject(scene->svg_js->js_ctx, &svg_rt->globalClass, 0, 0 );
	JS_InitStandardClasses(scene->svg_js->js_ctx, scene->svg_js->global);
	/*remember pointer to scene graph!!*/
	JS_SetPrivate(scene->svg_js->js_ctx, scene->svg_js->global, scene);
	{
		JSPropertySpec globalClassProps[] = {
			{"connected",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"parent",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		JSFunctionSpec globalClassFuncs[] = {
			{"createConnection", svg_connection_create, 0},
			{"gotoLocation", svg_nav_to_location, 1},
			{"alert",           svg_echo,          0},
			/*technically, this is part of Implementation interface, not global, but let's try not to complicate things too much*/
			{"hasFeature", dom_imp_has_feature, 2},
			{"parseXML",   svg_parse_xml,          0},
			{0}
		};
		JS_DefineFunctions(scene->svg_js->js_ctx, scene->svg_js->global, globalClassFuncs);
		JS_DefineProperties(scene->svg_js->js_ctx, scene->svg_js->global, globalClassProps);
	}

	/*initialize DOM core */
	dom_js_load(scene, scene->svg_js->js_ctx, scene->svg_js->global);

	/*user-defined extensions*/
	gf_sg_load_script_extensions(scene, scene->svg_js->js_ctx, scene->svg_js->global, 0);

	svg_define_udom_exception(scene->svg_js->js_ctx, scene->svg_js->global);

	/*SVGDocument class*/
	{

		JSPropertySpec svgDocumentProps[] = {
			{"global",				0,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*in our implementation, defaultView is just an alias to the global object*/
			{"defaultView",			0,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
				
			{0}
		};
		JSObject *doc_proto = dom_js_get_document_proto(scene->svg_js->js_ctx);
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, doc_proto, &svg_rt->svgDocument, 0, 0, svgDocumentProps, 0, 0, 0);
	}

	/*SVGElement class*/
	{
		JSFunctionSpec svgElementFuncs[] = {
			/*trait access interface*/
			{"getTrait", svg_udom_get_trait, 1},
			{"getTraitNS", svg_udom_get_trait, 2},
			{"getFloatTrait", svg_udom_get_float_trait, 1},
			{"getMatrixTrait", svg_udom_get_matrix_trait, 1},
			{"getRectTrait", svg_udom_get_rect_trait, 1},
			{"getPathTrait", svg_udom_get_path_trait, 1},
			{"getRGBColorTrait", svg_udom_get_rgb_color_trait, 1},
			/*FALLBACK TO BASE-VALUE FOR NOW - WILL NEED EITHER DOM TREE-CLONING OR A FAKE RENDER
			PASS FOR EACH PRESENTATION VALUE ACCESS*/
			{"getPresentationTrait", svg_udom_get_trait, 1},
			{"getPresentationTraitNS", svg_udom_get_trait, 2},
			{"getFloatPresentationTrait", svg_udom_get_float_trait, 1},
			{"getMatrixPresentationTrait", svg_udom_get_matrix_trait, 1},
			{"getRectPresentationTrait", svg_udom_get_rect_trait, 1},
			{"getPathPresentationTrait", svg_udom_get_path_trait, 1},
			{"getRGBColorPresentationTrait", svg_udom_get_rgb_color_trait, 1},
			{"setTrait", svg_udom_set_trait, 2},
			{"setTraitNS", svg_udom_set_trait, 3},
			{"setFloatTrait", svg_udom_set_float_trait, 2},
			{"setMatrixTrait", svg_udom_set_matrix_trait, 2},
			{"setRectTrait", svg_udom_set_rect_trait, 2},
			{"setPathTrait", svg_udom_set_path_trait, 2},
			{"setRGBColorTrait", svg_udom_set_rgb_color_trait, 2},
			/*locatable interface*/
			{"getBBox", svg_udom_get_local_bbox, 0},
			{"getScreenCTM", svg_udom_get_screen_ctm, 0},
			{"getScreenBBox", svg_udom_get_screen_bbox, 0},
			/*svgSVGElement interface*/
			{"createSVGMatrixComponents", svg_udom_create_matrix_components, 0},
			{"createSVGRect", svg_udom_create_rect, 0},
			{"createSVGPath", svg_udom_create_path, 0},
			{"createSVGRGBColor", svg_udom_create_color, 0},
			{"createSVGPoint", svg_udom_create_point, 0},

			{"moveFocus", svg_udom_move_focus, 0},
			{"setFocus", svg_udom_set_focus, 0},
			{"getCurrentFocusedObject", svg_udom_get_focus, 0},
			{"getCurrentTime", svg_udom_get_time, 0},
			
			/*timeControl interface*/
			{"beginElementAt", svg_udom_smil_begin, 1},
			{"beginElement", svg_udom_smil_begin, 0},
			{"endElementAt", svg_udom_smil_end, 1},
			{"endElement", svg_udom_smil_end, 0},
			{"pauseElement", svg_udom_smil_pause, 0},
			{"resumeElement", svg_udom_smil_resume, 0},
			{0}
		};

		JSPropertySpec svgElementProps[] = {
			/*svgElement interface*/
			{"id",						0,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			/*elementTraversal interface - all SVGElement implement this*/
			{"firstElementChild",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"lastElementChild",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"previousElementSibling",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"nextElementSibling",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*svgSVGElement interface*/
			{"currentScale",			5,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"currentRotate",			6,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"currentTranslate",		7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"viewport",				8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"currentTime",				9,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			/*timeControl interface*/
			{"isPaused",				10,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*old SVG1.1 stuff*/
			{"ownerSVGElement",			11,		JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*SVGElementInstance*/
			{"correspondingElement",	12, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"correspondingUseElement",	13, JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		JSObject *elt_proto = dom_js_get_element_proto(scene->svg_js->js_ctx);
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, elt_proto, &svg_rt->svgElement, 0, 0, svgElementProps, svgElementFuncs, 0, 0);

	}

	/*RGBColor class*/
	{
		JSPropertySpec rgbClassProps[] = {
			{"red",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"green",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"blue",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{0}
		};
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->rgbClass, 0, 0, rgbClassProps, 0, 0, 0);
	}
	/*SVGRect class*/
	{
		JSPropertySpec rectClassProps[] = {
			{"x",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"y",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"width",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"height",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{0}
		};
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->rectClass, 0, 0, rectClassProps, 0, 0, 0);
	}
	/*SVGPoint class*/
	{
		JSPropertySpec pointClassProps[] = {
			{"x",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"y",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{0}
		};
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->pointClass, 0, 0, pointClassProps, 0, 0, 0);
	}
	/*SVGMatrix class*/
	{
		JSFunctionSpec matrixClassFuncs[] = {
			{"getComponent", svg_mx2d_get_component, 1},
			{"mMultiply", svg_mx2d_multiply, 1},
			{"inverse", svg_mx2d_inverse, 0},
			{"mTranslate", svg_mx2d_translate, 2},
			{"mScale", svg_mx2d_scale, 1},
			{"mRotate", svg_mx2d_rotate, 1},
			{0}
		};
		JSPropertySpec matrixClassProps[] = {
			{"a",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"b",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"c",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"d",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"e",	4,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{"f",	5,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
			{0}
		};
		JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->matrixClass, 0, 0, matrixClassProps, matrixClassFuncs, 0, 0);
	}
	/*SVGPath class*/
	{
		JSFunctionSpec pathClassFuncs[] = {
			{"getSegment", svg_path_get_segment, 1},
			{"getSegmentParam", svg_path_get_segment_param, 2},
			{"moveTo", svg_path_move_to, 2},
			{"lineTo", svg_path_line_to, 2},
			{"quadTo", svg_path_quad_to, 4},
			{"curveTo", svg_path_curve_to, 6},
			{"close", svg_path_close, 0},
			{0}
		};
		JSPropertySpec pathClassProps[] = {
			{"numberOfSegments",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		proto = JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svg_rt->pathClass, 0, 0, pathClassProps, pathClassFuncs, 0, 0);
		JS_DefineProperty(scene->svg_js->js_ctx, proto, "MOVE_TO", INT_TO_JSVAL(77), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(scene->svg_js->js_ctx, proto, "LINE_TO", INT_TO_JSVAL(76), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(scene->svg_js->js_ctx, proto, "CURVE_TO", INT_TO_JSVAL(67), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(scene->svg_js->js_ctx, proto, "QUAD_TO", INT_TO_JSVAL(81), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(scene->svg_js->js_ctx, proto, "CLOSE", INT_TO_JSVAL(90), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

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
	prev_event = JS_GetPrivate(sg->svg_js->js_ctx, sg->svg_js->event);
	JS_SetPrivate(sg->svg_js->js_ctx, sg->svg_js->event, event);
	ret = JS_EvaluateScript(sg->svg_js->js_ctx, sg->svg_js->global, utf8_script, strlen(utf8_script), 0, 0, &rval);
	JS_SetPrivate(sg->svg_js->js_ctx, sg->svg_js->event, prev_event);

	/*clean-up*/
	JS_GC(sg->svg_js->js_ctx);

	if (ret==JS_FALSE) {
		char *sep = strchr(utf8_script, '(');
		if (!sep) return 0;
		sep[0] = 0;
		JS_LookupProperty(sg->svg_js->js_ctx, sg->svg_js->global, utf8_script, &rval);
		sep[0] = '(';
		if (JSVAL_IS_VOID(rval)) return 0;
	}
	return 1;
}

static void svg_script_predestroy(GF_Node *n, void *eff, Bool is_destroy)
{
	if (is_destroy) {
		GF_SVGJS *svg_js = n->sgprivate->scenegraph->svg_js;
		/*unregister script from parent scene (cf base_scenegraph::sg_reset) */
		gf_list_del_item(n->sgprivate->scenegraph->scripts, n);

		if (svg_js->nb_scripts) {
			svg_js->nb_scripts--;
			if (!svg_js->nb_scripts) {
				/*user-defined extensions*/
				gf_sg_load_script_extensions(n->sgprivate->scenegraph, svg_js->js_ctx, svg_js->global, 1);
				gf_sg_ecmascript_del(svg_js->js_ctx);
				dom_js_unload(svg_js->js_ctx, svg_js->global);
				free(svg_js);
				n->sgprivate->scenegraph->svg_js = NULL;
				assert(svg_rt);
				svg_rt->nb_inst--;
				if (!svg_rt->nb_inst) {
					free(svg_rt);
					svg_rt = NULL;
				}
			} else {
				u32 i, count;
				/*detach this script from our object cache*/
				count = gf_list_count(n->sgprivate->scenegraph->objects);
				for (i=0; i<count; i++) {
					JSObject *obj = gf_list_get(n->sgprivate->scenegraph->objects, i);
					GF_Node *a_node = JS_GetPrivate(svg_js->js_ctx, obj);
					if (n==a_node) 
						JS_SetPrivate(svg_js->js_ctx, obj, NULL);
				}

			}
		}
	}
}

static GF_Err JSScript_CreateSVGContext(GF_SceneGraph *sg)
{
	GF_SVGJS *svg_js;
	GF_SAFEALLOC(svg_js, GF_SVGJS);
	/*create new ecmascript context*/
	svg_js->js_ctx = gf_sg_ecmascript_new(sg);
	if (!svg_js->js_ctx) {
		free(svg_js);
		return GF_SCRIPT_ERROR;
	}
	if (!svg_rt) {
		GF_SAFEALLOC(svg_rt, GF_SVGuDOM);
		JS_SETUP_CLASS(svg_rt->svgElement, "SVGElement", JSCLASS_HAS_PRIVATE, svg_element_getProperty, svg_element_setProperty, dom_element_finalize);
		JS_SETUP_CLASS(svg_rt->svgDocument, "SVGDocument", JSCLASS_HAS_PRIVATE, svg_doc_getProperty, JS_PropertyStub, dom_document_finalize);
		JS_SETUP_CLASS(svg_rt->globalClass, "global", JSCLASS_HAS_PRIVATE, global_getProperty, JS_PropertyStub, JS_FinalizeStub);
		JS_SETUP_CLASS(svg_rt->rgbClass, "SVGRGBColor", JSCLASS_HAS_PRIVATE, rgb_getProperty, rgb_setProperty, baseCI_finalize);
		JS_SETUP_CLASS(svg_rt->rectClass, "SVGRect", JSCLASS_HAS_PRIVATE, rect_getProperty, rect_setProperty, baseCI_finalize);
		JS_SETUP_CLASS(svg_rt->pointClass, "SVGPoint", JSCLASS_HAS_PRIVATE, point_getProperty, point_setProperty, baseCI_finalize);
		JS_SETUP_CLASS(svg_rt->matrixClass, "SVGMatrix", JSCLASS_HAS_PRIVATE, matrix_getProperty, matrix_setProperty, baseCI_finalize);
		JS_SETUP_CLASS(svg_rt->pathClass, "SVGPath", JSCLASS_HAS_PRIVATE, path_getProperty, JS_PropertyStub, pathCI_finalize);
	}
	svg_rt->nb_inst++;

	sg->svg_js = svg_js;
	/*load SVG & DOM APIs*/
	svg_init_js_api(sg);

	svg_js->script_execute = svg_script_execute;
	svg_js->handler_execute = svg_script_execute_handler;
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
	Bool success = 1;
	JSBool ret;
	jsval rval;
	GF_SVGJS *svg_js;

	svg_js = script->sgprivate->scenegraph->svg_js;
	jsf = fopen(file, "rb");
	if (!jsf) {
		GF_JSAPIParam par;
		GF_SceneGraph *scene = script->sgprivate->scenegraph;
		char *abs_url = NULL;
		par.uri.url = file;
		if (scene->script_action && scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_RESOLVE_XLINK, script, &par))
			abs_url = (char *) par.uri.url;

		if (abs_url) {
			jsf = fopen(abs_url, "rb");
			free(abs_url);
		}
	}
	if (!jsf) return 0;

	fseek(jsf, 0, SEEK_END);
	fsize = ftell(jsf);
	fseek(jsf, 0, SEEK_SET);
	jsscript = malloc(sizeof(char)*(fsize+1));
	fread(jsscript, sizeof(char)*fsize, 1, jsf);
	fclose(jsf);
	jsscript[fsize] = 0;

	/*for handler, only load code*/
	if (script->sgprivate->tag==TAG_SVG_handler) {
		GF_DOMText *txt = gf_dom_add_text_node(script, jsscript);
		txt->type = GF_DOM_TEXT_INSERTED;
		return 1;
	}

	ret = JS_EvaluateScript(svg_js->js_ctx, svg_js->global, jsscript, sizeof(char)*fsize, 0, 0, &rval);
	if (ret==JS_FALSE) success = 0;
	gf_dom_listener_process_add(script->sgprivate->scenegraph);

	free(jsscript);
	return success;
}

#include <gpac/download.h>
#include <gpac/network.h>

/* Download is performed asynchronously, so the script loading must be performed in this function */
static void JS_SVG_NetIO(void *cbck, GF_NETIO_Parameter *param)
{
	GF_Err e;
	JSFileDownload *jsdnload = (JSFileDownload *)cbck;
	GF_Node *node = jsdnload->node;

	e = param->error;
	if (param->msg_type==GF_NETIO_DATA_TRANSFERED) {
		const char *szCache = gf_dm_sess_get_cache_name(jsdnload->sess);
		if (!svg_js_load_script(jsdnload->node, (char *) szCache))
			e = GF_SCRIPT_ERROR;
		else
			e = GF_OK;

	} else {
		if (!e) {
			/* the download is going on */
			return;
		} else {
			/* there was an error during the download */
		}
	}

	/*destroy current download session (ie, destroy ourselves)*/
	gf_dm_sess_del(jsdnload->sess);
	free(jsdnload);

	if (e) {
		GF_JSAPIParam par;
		par.info.e = e;
		par.info.msg = "Cannot fetch script";
		ScriptAction(node->sgprivate->scenegraph, GF_JSAPI_OP_MESSAGE, NULL, &par);
	}
}

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
	if (gf_node_get_attribute_by_tag(node, TAG_XLINK_ATT_href, 0, 0, &href_info) == GF_OK) {
		GF_JSAPIParam par;
		char *url;
		GF_Err e;
		JSFileDownload *jsdnload;
		XMLRI *xmlri = (XMLRI *)href_info.far_ptr;
	
		/* getting the base uri of the scene and concatenating */
		ScriptAction(node->sgprivate->scenegraph, GF_JSAPI_OP_GET_SCENE_URI, node, &par);
		url = NULL;
		if (par.uri.url) url = gf_url_concatenate(par.uri.url, xmlri->string);
		if (!url) url = strdup(xmlri->string);

		/* if the file is local, we don't need to download it */
		if (!strstr(url, "://") || !strnicmp(url, "file://", 7)) {
			svg_js_load_script(node, url);
		} else {

			/* getting a download manager */
			par.dnld_man = NULL;
			ScriptAction(node->sgprivate->scenegraph, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
			if (par.dnld_man) {

				GF_SAFEALLOC(jsdnload, JSFileDownload);
				jsdnload->node = node;
				jsdnload->sess = gf_dm_sess_new(par.dnld_man, url, 0, JS_SVG_NetIO, jsdnload, &e);
				if (!jsdnload->sess) {
					free(jsdnload);
				}
			}
		}
		free(url);
	} 
	/*for scripts only, execute*/
	else if (node->sgprivate->tag == TAG_SVG_script) {
		txt = svg_get_text_child(node);
		if (!txt) return;
		ret = JS_EvaluateScript(svg_js->js_ctx, svg_js->global, txt->textContent, strlen(txt->textContent), 0, 0, &rval);
		if (ret==JS_FALSE) {
			_ScriptMessage(node->sgprivate->scenegraph, GF_SCRIPT_ERROR, "SVG: Invalid script");
		}
		gf_dom_listener_process_add(node->sgprivate->scenegraph);
	}
}

static Bool svg_script_execute_handler(GF_Node *node, GF_DOM_Event *event, GF_Node *observer)
{
	GF_DOMText *txt = NULL;
	GF_SVGJS *svg_js;
	JSObject *__this;
	JSBool ret = JS_TRUE;
	GF_DOM_Event *prev_event = NULL;
	GF_DOMHandler *hdl = (GF_DOMHandler *)node;
	jsval fval, rval;

	if (!hdl->js_fun && !hdl->js_fun_val && !hdl->evt_listen_obj) {
		txt = svg_get_text_child(node);
		if (!txt) return 0;
	}
	/*not sure about this (cf test struct-use-205-t.svg)*/
	if (!node->sgprivate->parents) return 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_INTERACT, ("[DOM Events    ] Executing script code from handler\n"));

	svg_js = node->sgprivate->scenegraph->svg_js;

	prev_event = JS_GetPrivate(svg_js->js_ctx, svg_js->event);
	/*break loops*/
	if (prev_event && (prev_event->type==event->type) && (prev_event->target==event->target)) 
		return 0;
	JS_SetPrivate(svg_js->js_ctx, svg_js->event, event);

	/*if an observer is being specified, use it*/
	if (hdl->evt_listen_obj) __this = hdl->evt_listen_obj;
	/*compile the jsfun if any - 'this' is the associated observer*/
	else __this = observer ? JSVAL_TO_OBJECT( dom_element_construct(svg_js->js_ctx, observer) ) : svg_js->global;
	if (txt && !hdl->js_fun) {
		char *argn = "evt";
		hdl->js_fun = JS_CompileFunction(svg_js->js_ctx, __this, NULL, 1, &argn, txt->textContent, strlen(txt->textContent), NULL, 0);
	}

	if (hdl->js_fun || hdl->js_fun_val || hdl->evt_listen_obj) {
		JSObject *evt;
		jsval argv[1];
		evt = gf_dom_new_event(svg_js->js_ctx);
		JS_SetPrivate(svg_js->js_ctx, evt, event);
		argv[0] = OBJECT_TO_JSVAL(evt);

		if (hdl->js_fun) {
			ret = JS_CallFunction(svg_js->js_ctx, __this, (JSFunction *)hdl->js_fun, 1, argv, &rval);
		}
		else if (hdl->js_fun_val) {
			jsval funval = (JSWord) hdl->js_fun_val;
			ret = JS_CallFunctionValue(svg_js->js_ctx, __this, funval, 1, argv, &rval);
		} else {
			ret = JS_CallFunctionName(svg_js->js_ctx, hdl->evt_listen_obj, "handleEvent", 1, argv, &rval);
		}
	} 
	else if (JS_LookupProperty(svg_js->js_ctx, svg_js->global, txt->textContent, &fval) && !JSVAL_IS_VOID(fval) ) {
		if (svg_script_execute(node->sgprivate->scenegraph, txt->textContent, event)) 
			ret = JS_FALSE;
	} 
	else {
		ret = JS_EvaluateScript(svg_js->js_ctx, __this, txt->textContent, strlen(txt->textContent), 0, 0, &rval);
	}
	JS_SetPrivate(svg_js->js_ctx, svg_js->event, prev_event);

	if (ret==JS_FALSE) {
		_ScriptMessage(node->sgprivate->scenegraph, GF_SCRIPT_ERROR, "SVG: Invalid handler textContent");
		return 0;
	}
	return 1;
}

#endif

#endif
