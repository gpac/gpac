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
#include <gpac/nodes_svg.h>


#ifdef GPAC_HAS_SPIDERMONKEY

#define uDOM_SETUP_CLASS(the_class, cname, flag, getp, setp, fin)	\
	memset(&the_class, 0, sizeof(the_class));	\
	the_class.name = cname;	\
	the_class.flags = flag;	\
	the_class.addProperty = JS_PropertyStub;	\
	the_class.delProperty = JS_PropertyStub;	\
	the_class.getProperty = getp;	\
	the_class.setProperty = setp;	\
	the_class.enumerate = JS_EnumerateStub;	\
	the_class.resolve = JS_ResolveStub;		\
	the_class.convert = JS_ConvertStub;		\
	the_class.finalize = fin;	


static JSClass globalClass;
static JSClass docClass;
static JSClass svgClass;
static JSClass connectionClass;

static JSObject *svg_new_path_object(JSContext *c, SVG_PathData *d);


static GF_SVGJS *get_svg_js(JSContext *c)
{
	GF_SceneGraph *sg = JS_GetContextPrivate(c);
	return sg->svg_js;
}

static void svg_node_changed(GF_Node *n, GF_FieldInfo *info)
{
	if (!info) {
		gf_node_changed(n, NULL);
	} else {
		switch (info->fieldType) {
		case SVG_Color_datatype:
		case SVG_Paint_datatype:
		case SVG_FillRule_datatype:
		case SVG_Opacity_datatype:
		case SVG_StrokeLineJoin_datatype:
		case SVG_StrokeLineCap_datatype:
		case SVG_StrokeMiterLimit_datatype:
		case SVG_StrokeDashOffset_datatype:
		case SVG_StrokeWidth_datatype:
		case SVG_StrokeDashArray_datatype:
			gf_node_dirty_set(n, GF_SG_SVG_APPEARANCE_DIRTY, 0);
			break;

		case SVG_FontStyle_datatype:
		case SVG_FontWeight_datatype:
		case SVG_FontSize_datatype:
		case SVG_TextAnchor_datatype:
		case SVG_Length_datatype:
		case SVG_PathData_datatype:
		case SVG_FontFamily_datatype:
			gf_node_dirty_set(n, 0, 0);
			break;
		default:
			break;
		}
	}
	/*trigger rendering*/
	if (n->sgprivate->scenegraph->NodeModified)
		n->sgprivate->scenegraph->NodeModified(n->sgprivate->scenegraph->ModifCallback, NULL);
}


/*note we are using float to avoid conversions fixed to/from JS native */
typedef struct
{
	u32 r, g, b;
} rgbCI;
static JSClass rgbClass;
typedef struct
{
	Float x, y, w, h;
	/*if set, this is the svg.viewport uDOM object, its values are updated upon query*/
	GF_SceneGraph *sg;
} rectCI;
static JSClass rectClass;
typedef struct
{
	Float x, y;
	/*if set, this is the svg.currentTranslate uDOM object, its values are updated upon query*/
	GF_SceneGraph *sg;
} pointCI;
static JSClass pointClass;
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
static JSClass pathClass;
static JSClass matrixClass;

static JSBool svg_connection_create(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static JSBool svg_nav_to_location(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_SceneGraph *sg;
	if ((argc!=1) || !JS_InstanceOf(c, obj, &globalClass, NULL)) return JS_FALSE;
	sg = JS_GetContextPrivate(c);
	par.uri.url = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	par.uri.nb_params = 0;
	sg->js_ifce->ScriptAction(sg->js_ifce->callback, GF_JSAPI_OP_LOAD_URL, sg->RootNode, &par);
	return JS_FALSE;
}

static void svg_script_error(JSContext *c, const char *msg, JSErrorReport *jserr)
{
	GF_SceneGraph *sg = JS_GetContextPrivate(c);
	sg->js_ifce->ScriptMessage(sg->js_ifce->callback, GF_SCRIPT_ERROR, msg);
}

static JSBool svg_echo(JSContext *c, JSObject *p, uintN argc, jsval *argv, jsval *rval)
{
	u32 i;
	char buf[5000];
	GF_SceneGraph *sg = JS_GetContextPrivate(c);
	if (!sg) return JS_FALSE;

	strcpy(buf, "");
	for (i = 0; i < argc; i++) {
		JSString *str = JS_ValueToString(c, argv[i]);
		if (!str) return JS_FALSE;
		if (i) strcat(buf, " ");
		strcat(buf, JS_GetStringBytes(str));
	}
	sg->js_ifce->ScriptMessage(sg->js_ifce->callback, GF_SCRIPT_INFO, buf);
	return JS_TRUE;
}
static JSBool global_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_SceneGraph *sg;
	if (!JS_InstanceOf(c, obj, &globalClass, NULL) ) return JS_FALSE;
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
			return JS_FALSE;
		}
	}
	return JS_TRUE;
}

/*creates a new SVGelement wrapper for the given node if needed, and stores it in node bank*/
static JSObject *svg_elt_construct(JSContext *c, GF_Node *n, Bool search_bank)
{
	u32 i, count;
	GF_Node *m;
	JSObject *obj;
	GF_SVGJS *svg_js = get_svg_js(c);

	if (search_bank) {
		count = gf_list_count(svg_js->node_bank);
		for (i=0; i<count; i++) {
			obj = gf_list_get(svg_js->node_bank, i);
			m = JS_GetPrivate(c, obj);
			if (m==n) return obj;
		}
	}
	obj = JS_NewObject(c, &svgClass, 0, 0);
	JS_SetPrivate(c, obj, n);
	gf_list_add(svg_js->node_bank, obj);
	return obj;
}


/*TODO - try to be more precise...*/
static JSBool dom_imp_has_feature(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	*rval = BOOLEAN_TO_JSVAL(1);
	return JS_TRUE;
}

static JSPropertySpec globalClassProps[] = {
	{"connected",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"parent",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};
static JSFunctionSpec globalClassFuncs[] = {
	{"createConnection", svg_connection_create, 0},
	{"gotoLocation", svg_nav_to_location, 1},
    {"alert",           svg_echo,          0},
	/*technically, this is part of Implementation interface, not global, but let's try not to complicate things too much*/
	{"hasFeature", dom_imp_has_feature, 2},
	{0}
};

void gf_sg_handle_dom_event(struct _tagSVGhandlerElement *hdl, GF_DOM_Event *event);

/*eventListeners routines used by document, element anbd connection interfaces*/
static JSBool udom_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SVGlistenerElement *listener;
	SVGhandlerElement *handler;
	char *type, *callback;
	u32 of = 0;
	u32 evtType;
	char *inNS = NULL;
	GF_Node *node = NULL;
	if (JS_InstanceOf(c, obj, &svgClass, NULL) ) {
		node = JS_GetPrivate(c, obj);
	}
	else if (JS_InstanceOf(c, obj, &docClass, NULL) ) {
		GF_SceneGraph *sg = JS_GetPrivate(c, obj);
		node = sg->RootNode;
	} else if (JS_InstanceOf(c, obj, &connectionClass, NULL) ) {
	}
	if (!node) return JS_FALSE;

	if (argc==4) {
		if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
		inNS = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		of = 1;
	}

	if (!JSVAL_IS_STRING(argv[of])) return JS_FALSE;
	type = JS_GetStringBytes(JSVAL_TO_STRING(argv[of]));
	callback = NULL;
	if (JSVAL_IS_STRING(argv[of+1])) {
		callback = JS_GetStringBytes(JSVAL_TO_STRING(argv[of+1]));
	} else if (JSVAL_IS_OBJECT(argv[of+1])) {
		JSFunction *fun = JS_ValueToFunction(c, argv[of+1]);
		if (!fun) return JS_FALSE;
		callback = (char *) JS_GetFunctionName(fun);
	}
	if(!callback) return JS_FALSE;

	evtType = gf_dom_event_type_by_name(type);
	if (evtType==SVG_DOM_EVT_UNKNOWN) return JS_FALSE;

	/*emulate a listener for onClick event*/
	listener = (SVGlistenerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_listener);
	handler = (SVGhandlerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_handler);
	gf_node_register((GF_Node *)listener, node);
	gf_list_add( ((GF_ParentNode *)node)->children, listener);
	gf_node_register((GF_Node *)handler, node);
	gf_list_add(((GF_ParentNode *)node)->children, handler);
	handler->ev_event.type = listener->event.type = evtType;
	listener->handler.target = (SVGElement *) handler;
	listener->target.target = (SVGElement *)node;
	handler->textContent = strdup(callback);
	handler->handle_event = gf_sg_handle_dom_event;
	gf_dom_listener_add((GF_Node *) node, (GF_Node *) listener);
	return JS_TRUE;
}
static JSBool udom_remove_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *type, *callback;
	u32 of = 0;
	u32 evtType, i, count;
	char *inNS = NULL;
	GF_Node *node = NULL;
	if (JS_InstanceOf(c, obj, &svgClass, NULL) ) {
		node = JS_GetPrivate(c, obj);
	}
	else if (JS_InstanceOf(c, obj, &docClass, NULL) ) {
		GF_SceneGraph *sg = JS_GetPrivate(c, obj);
		node = sg->RootNode;
	} else if (JS_InstanceOf(c, obj, &connectionClass, NULL) ) {
	}
	if (!node || !node->sgprivate->events) return JS_FALSE;

	if (argc==4) {
		if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
		inNS = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		of = 1;
	}

	if (!JSVAL_IS_STRING(argv[of])) return JS_FALSE;
	type = JS_GetStringBytes(JSVAL_TO_STRING(argv[of]));
	callback = NULL;
	if (JSVAL_IS_STRING(argv[of+1])) {
		callback = JS_GetStringBytes(JSVAL_TO_STRING(argv[of+1]));
	} else if (JSVAL_IS_OBJECT(argv[of+1])) {
		JSFunction *fun = JS_ValueToFunction(c, argv[of+1]);
		if (!fun) return JS_FALSE;
		callback = (char *) JS_GetFunctionName(fun);
	}
	if(!callback) return JS_FALSE;

	evtType = gf_dom_event_type_by_name(type);
	if (evtType==SVG_DOM_EVT_UNKNOWN) return JS_FALSE;
	
	count = gf_list_count(node->sgprivate->events);
	for (i=0; i<count; i++) {
		SVGhandlerElement *hdl;
		SVGlistenerElement *el = (SVGlistenerElement*)gf_dom_listener_get(node, i);
		if ((el->event.type != evtType) || !el->handler.target) continue;
		hdl = (SVGhandlerElement *)el->handler.target;
		if (!hdl->textContent || strcmp(hdl->textContent, callback)) continue;
		gf_dom_listener_del(node, (GF_Node *) el);
		gf_list_del_item( ((GF_ParentNode *)node)->children, (GF_Node *) hdl);
		gf_node_unregister((GF_Node *) hdl, node);
		gf_list_del_item( ((GF_ParentNode *)node)->children, (GF_Node *) el);
		gf_node_unregister((GF_Node *) el, node);
		return JS_TRUE;
	}
	return JS_FALSE;
}


/*svgDocument object*/
static JSBool doc_element_op_forbidden(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{	
	return JS_FALSE;
}
static JSBool doc_create_element(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *inNS, *eltName;
	u32 tag;
	GF_SceneGraph *sg;
	GF_Node *n;
	if (!JS_InstanceOf(c, obj, &docClass, NULL) ) return JS_FALSE;
	if (argc!=2) return JS_FALSE;
	sg = JS_GetPrivate(c, obj);
	if (!sg) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
	
	inNS = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	eltName = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));

	tag = gf_node_svg_type_by_class_name(eltName);
	if (!tag) return JS_FALSE;
	n = (GF_Node *) gf_node_new(sg, tag);
	*rval = OBJECT_TO_JSVAL(svg_elt_construct(c, n, 0));
	return JS_TRUE;
}
static JSBool doc_get_element_by_id(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_SceneGraph *sg;
	GF_Node *n;
	char *name;
	if ((argc!=1) || !JS_InstanceOf(c, obj, &docClass, NULL) ) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	sg = JS_GetPrivate(c, obj);
	if (!sg) return JS_FALSE;
	name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (!name) return JS_FALSE;
	n = gf_sg_find_node_by_name(sg, name);
	if (n) {
		*rval = OBJECT_TO_JSVAL(svg_elt_construct(c, n, 1));
		return JS_TRUE;
	}
	return JS_FALSE;
}

/*svgDocument (: node, : document, : eventTarget)*/
static JSFunctionSpec docClassFuncs[] = {
	/*node interface*/
	{"appendChild", doc_element_op_forbidden, 1},
	{"insertBefore", doc_element_op_forbidden, 2},
	{"removeChild", doc_element_op_forbidden, 1},
	/*doc interface*/
    {"createElementNS",  doc_create_element,    2},
    {"getElementById",   doc_get_element_by_id,  1},
	/*eventTarget interface*/
	{"addEventListenerNS", udom_add_listener, 4},
	{"removeEventListenerNS", udom_remove_listener, 4},
	{"addEventListener", udom_add_listener, 3},
	{"removeEventListener", udom_remove_listener, 3},
	{0}
};
static JSPropertySpec docClassProps[] = {
	/*node interface*/
	{"namespaceURI",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"localName",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"parentNode",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"ownerDocument",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"textContent",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
	/*DOM implementation*/
	{"implementation",	5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*document interface*/
	{"documentElement", 6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*svgDocument interface*/
	{"global",			7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};



static JSBool svg_doc_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &docClass, NULL) ) return JS_FALSE;

	if (JSVAL_IS_INT(id)) {
		JSString *s;
		GF_JSAPIParam par;
		GF_SceneGraph *sg = JS_GetPrivate(c, obj);
		if (!sg) return JS_FALSE;

		switch (JSVAL_TO_INT(id)) {
		/*namespaceURI*/
		case 0: 
			s = JS_NewStringCopyZ(c, "http://www.w3.org/2000/svg");
			*vp = STRING_TO_JSVAL( s );
			return JS_TRUE;
		/*localName*/
		case 1:
			sg->js_ifce->ScriptAction(sg->js_ifce, GF_JSAPI_OP_GET_SCENE_URI, sg->RootNode, &par);
			s = JS_NewStringCopyZ(c, par.uri.url);
			*vp = STRING_TO_JSVAL(s);
			return JS_TRUE;
		/*parentNode is NULL*/
		case 2: *vp = JSVAL_VOID; return JS_TRUE;
		/*ownerDocument is self*/
		case 3: *vp = OBJECT_TO_JSVAL(obj); return JS_TRUE;
		/*textContent - TODO*/
		case 4: 
			return JS_TRUE;
		/*dom implementation object is handled through our global object*/
		case 5: *vp = OBJECT_TO_JSVAL(sg->svg_js->global); return JS_TRUE;
		/*documentElement*/
		case 6: 
			if (sg->RootNode) *vp = OBJECT_TO_JSVAL(svg_elt_construct(c, sg->RootNode, 1) );
			return JS_TRUE;
		/*global*/
		case 7: 
			*vp = OBJECT_TO_JSVAL(get_svg_js(c)->global);
			return JS_TRUE;
		default: 
			return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSBool svg_doc_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &docClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		GF_SceneGraph *sg = JS_GetPrivate(c, obj);
		if (!sg) return JS_FALSE;

		switch (JSVAL_TO_INT(id)) {
		/*textContent - TODO*/
		case 0: 
			return JS_TRUE;
		/*the rest is read-only*/
		default:
			return JS_FALSE;
		}
	}
	return JS_FALSE;
}

static void svg_elt_finalize(JSContext *c, JSObject *obj)
{
	s32 i;
	GF_Node *n;
	GF_SVGJS *svg_js = get_svg_js(c);
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return;
	n = (GF_Node *) JS_GetPrivate(c, obj);
	/*the JS proto of the svgClass or a destroyed object*/
	if (!n) return;

	/*remove from node bank*/
	i = gf_list_del_item(svg_js->node_bank, obj);
	assert(i>=0);
	/*node is not inserted in graph, destroy it*/
	if (!n->sgprivate->num_instances) {
		n->sgprivate->num_instances = 1;
		gf_node_unregister(n, NULL);
	}
}

static void svg_elt_add(JSContext *c, GF_Node *par, GF_Node *n, s32 pos) 
{
	Bool do_init = n->sgprivate->num_instances ? 0 : 1;
	if (pos<0) gf_list_add( ((GF_ParentNode *)par)->children, n);
	else gf_list_insert( ((GF_ParentNode *)par)->children, n, (u32) pos);
	gf_node_register(n, par);

	if (do_init && !n->sgprivate->PreDestroyNode && !n->sgprivate->RenderNode) {
		gf_node_init(n);

		if (n->sgprivate->events) {
			GF_DOM_Event evt;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.type = SVG_DOM_EVT_LOAD;
			gf_dom_event_fire(n, NULL, &evt);
		}
	}
	svg_node_changed(par, NULL);
}

/*SVG element object*/
static JSBool svg_elt_append(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *par, *n;
	JSObject *newObj;
	if (!argc || !JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	par = JS_GetPrivate(c, obj);
	if (!par) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	newObj = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(c, newObj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, newObj);
	if (!n) return JS_FALSE;
	svg_elt_add(c, par, n, -1);
	return JS_TRUE;
}
static JSBool svg_elt_insert(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *par, *n, *next;
	s32 pos;
	JSObject *newObj;
	if ((argc!=2) || !JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	par = JS_GetPrivate(c, obj);
	if (!par) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	newObj = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(c, newObj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, newObj);
	if (!n) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	newObj = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, newObj, &svgClass, NULL) ) return JS_FALSE;
	next = JS_GetPrivate(c, newObj);
	if (!next) return JS_FALSE;
	pos = gf_list_find(((GF_ParentNode *)par)->children, next);
	if (pos<0) return JS_FALSE;
	svg_elt_add(c, par, n, pos);
	return JS_TRUE;
}
static JSBool svg_elt_remove(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *par, *n;
	JSObject *newObj;
	s32 i;
	if (!argc || !JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	par = JS_GetPrivate(c, obj);
	if (!par) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	newObj = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(c, newObj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, newObj);
	if (!n) return JS_FALSE;

	i = gf_list_del_item( ((GF_ParentNode *)par)->children, n);
	if (i<0) return JS_FALSE;
	if (n->sgprivate->num_instances==1) {
		gf_node_register(n, NULL);
		gf_node_unregister(n, par);
		/*force numInstance to 0 to signale the object is no longer in the tree*/
		n->sgprivate->num_instances = 0;
	} else {
		gf_node_unregister(n, par);
	}
	svg_node_changed(par, NULL);
	return JS_TRUE;
}

/*TODO*/
static JSBool svg_elt_clone(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

static JSBool svg_elt_get_attr(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 offset = 0;
	JSString *s;
	GF_FieldInfo info;
	char *attName, *inNS;
	char attValue[4096];
	SVGElement *elt = (SVGElement *)JS_GetPrivate(c, obj);
	if (!elt || (argc>2)) return JS_FALSE;
	inNS = NULL;
	if (argc==2) {
		if (JSVAL_IS_STRING(argv[0])) 
			inNS = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

		offset = 1;
	}
	if (!JSVAL_IS_STRING(argv[offset]) ) return JS_FALSE;
	attName = JS_GetStringBytes(JSVAL_TO_STRING(argv[offset]));
	if (gf_node_get_field_by_name((GF_Node *)elt, attName, &info) != GF_OK) return JS_FALSE;

	gf_svg_dump_attribute(elt, &info, attValue);
	s = JS_NewStringCopyZ(c, attValue);
	*rval = STRING_TO_JSVAL(s);
	return JS_TRUE;
}

static JSBool svg_elt_set_attr(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 i, count, offset = 0;
	XMLEV_Event evt;
	SVGhandlerElement *handler;
	GF_FieldInfo info;
	char *attName, *attValue, *inNS;
	SVGElement *elt = (SVGElement *)JS_GetPrivate(c, obj);
	if (!elt || (argc>3)) return JS_FALSE;

	inNS = NULL;
	if (argc==3) {
		if (JSVAL_IS_STRING(argv[0])) {
			inNS = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		}
		offset = 1;
	}
	if (!JSVAL_IS_STRING(argv[offset])) return JS_FALSE;
	attName = JS_GetStringBytes(JSVAL_TO_STRING(argv[offset]));

	if (!JSVAL_IS_STRING(argv[offset+1])) {
		/*non-uDOM*/
		attValue = JS_GetStringBytes(JS_ValueToString(c, argv[offset+1]));
	} else {
		attValue = JS_GetStringBytes(JSVAL_TO_STRING(argv[offset+1]));
	}

	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name((GF_Node *)elt, attName, &info) == GF_OK) {
		gf_svg_parse_attribute(elt, &info, attValue, 0, 0);
		svg_node_changed((GF_Node *)elt, &info);
		return JS_TRUE;
	}
	/*check for old adressing of events*/
	evt.parameter = 0;
	evt.type = gf_dom_event_type_by_name(attName + 2);
	if (evt.type == SVG_DOM_EVT_UNKNOWN) return JS_FALSE;

	/*check if we're modifying an existing listener*/
	count = gf_list_count(elt->sgprivate->events);
	for (i=0;i<count; i++) {
		SVGhandlerElement *handler;
		SVGlistenerElement *listen = gf_list_get(elt->sgprivate->events, i);
		if (listen->event.type != evt.type) continue;
		assert(listen->handler.target);
		handler = (SVGhandlerElement *)listen->handler.target;
		if (handler->textContent) free(handler->textContent);
		handler->textContent = strdup(attValue);
		return JS_TRUE;
	}
	/*nope, create a listener*/
	handler = gf_dom_listener_build((GF_Node *) elt, evt);
	handler->textContent = strdup(attValue);
	return JS_TRUE;
}


static Bool svg_smil_check_instance(JSContext *c, JSObject *obj)
{
	GF_Node *n;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return 0;
	n = JS_GetPrivate(c, obj);
	if (!n) return 0;
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
		return 1;
	}
	return 0;
}

/*TODO*/
static JSBool svg_smil_begin(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	if (!svg_smil_check_instance(c, obj) ) return JS_FALSE;
	return JS_FALSE;
}
/*TODO*/
static JSBool svg_smil_end(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	if (!svg_smil_check_instance(c, obj) ) return JS_FALSE;
	return JS_FALSE;
}
/*TODO*/
static JSBool svg_smil_pause(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	if (!svg_smil_check_instance(c, obj) ) return JS_FALSE;
	return JS_FALSE;
}
/*TODO*/
static JSBool svg_smil_resume(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	if (!svg_smil_check_instance(c, obj) ) return JS_FALSE;
	return JS_FALSE;
}


static JSBool udom_get_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char attValue[1024];
	GF_Err e;
	GF_FieldInfo info;
	GF_Node *n;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (argc==2) {
		char fullName[1024];
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;

		strcpy(fullName, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])) );
		strcat(fullName, ":");
		strcat(fullName, JS_GetStringBytes(JSVAL_TO_STRING(argv[1])) );
		e = gf_node_get_field_by_name(n, fullName, &info);
	} else if (argc==2) {
		e = gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info);
	} else return JS_FALSE;

	if (e!=GF_OK) return JS_FALSE;
	*rval = JSVAL_VOID;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_Opacity_datatype:
	case SVG_StrokeMiterLimit_datatype:
	case SVG_FontSize_datatype:
	case SVG_StrokeDashOffset_datatype:
	case SVG_AudioLevel_datatype:
	case SVG_LineIncrement_datatype:
	case SVG_Color_datatype:
	case SVG_Paint_datatype:
	/* inheritable float and unit */
	case SVG_StrokeWidth_datatype:
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
	case SVG_PointerEvents_datatype:
	case SVG_RenderingHint_datatype:
	case SVG_VectorEffect_datatype:
	case SVG_PlaybackOrder_datatype:
	case SVG_TimelineBegin_datatype:
/*end of string traits*/
/*DOM string traits*/
	case SVG_FontFamily_datatype:
	case SVG_IRI_datatype:
	case SVG_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
	case SVG_Focus_datatype:
	case SVG_ID_datatype:
	case SVG_GradientOffset_datatype:
/*end of DOM string traits*/
		gf_svg_dump_attribute((SVGElement *) n, &info, attValue);
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
		return JS_FALSE;
#endif
	}
	return JS_FALSE;
}
static JSBool udom_get_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	GF_FieldInfo info;
	GF_Node *n;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_Opacity_datatype:
	case SVG_StrokeMiterLimit_datatype:
	case SVG_FontSize_datatype:
	case SVG_StrokeDashOffset_datatype:
	case SVG_AudioLevel_datatype:
	case SVG_LineIncrement_datatype:
	case SVG_StrokeWidth_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	{
		SVG_Number *l = (SVG_Number *)info.far_ptr;
		if (l->type==SVG_NUMBER_AUTO || l->type==SVG_NUMBER_INHERIT) return JS_FALSE;
		*rval = DOUBLE_TO_JSVAL( JS_NewDouble(c, FIX2FLT(l->value) ) );
		return JS_TRUE;
	}
	default:
		return JS_FALSE;
	}
	return JS_FALSE;
}
static JSBool udom_get_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	JSObject *mO;
	GF_FieldInfo info;
	GF_Node *n;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_Matrix_datatype) {
		GF_Matrix2D *mx = malloc(sizeof(GF_Matrix2D));
		mO = JS_NewObject(c, &matrixClass, 0, 0);
		gf_mx2d_init(*mx);
		gf_mx2d_copy(*mx, *(SVG_Matrix*)info.far_ptr);

		JS_SetPrivate(c, mO, mx);
		*rval = OBJECT_TO_JSVAL(mO);
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool udom_get_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *newObj;
	char *szName;
	GF_FieldInfo info;
	GF_Node *n;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_ViewBox_datatype) {
		rectCI *rc;
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		newObj = JS_NewObject(c, &rectClass, 0, 0);
		GF_SAFEALLOC(rc, sizeof(rectCI));
		rc->x = FIX2FLT(v->x);
		rc->y = FIX2FLT(v->y);
		rc->w = FIX2FLT(v->width);
		rc->h = FIX2FLT(v->height);
		JS_SetPrivate(c, newObj, rc);
		*rval = OBJECT_TO_JSVAL(newObj);
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool udom_get_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	GF_FieldInfo info;
	GF_Node *n;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_PathData_datatype) {
		*rval = OBJECT_TO_JSVAL( svg_new_path_object(c, info.far_ptr) );
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool udom_get_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *szName;
	GF_FieldInfo info;
	GF_Node *n;
	rgbCI *rgb;
	JSObject *newObj;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=1) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	switch (info.fieldType) {
	case SVG_Color_datatype:
	{
		SVG_Color *col = (SVG_Color *)info.far_ptr;
		if (col->type == SVG_COLOR_CURRENTCOLOR) return JS_FALSE;
		if (col->type == SVG_COLOR_INHERIT) return JS_FALSE;
		newObj = JS_NewObject(c, &rgbClass, 0, 0);
		GF_SAFEALLOC(rgb, sizeof(rgbCI));
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
		if (paint->type==SVG_PAINT_COLOR) {
			newObj = JS_NewObject(c, &rgbClass, 0, 0);
			GF_SAFEALLOC(rgb, sizeof(rgbCI));
			rgb->r = (u8) (255*FIX2FLT(paint->color.red) );
			rgb->g = (u8) (255*FIX2FLT(paint->color.green) );
			rgb->b = (u8) (255*FIX2FLT(paint->color.blue) );
			JS_SetPrivate(c, newObj, rgb);
			*rval = OBJECT_TO_JSVAL(newObj);
			return JS_TRUE;
		}
		return JS_FALSE;
	}
	}
	return JS_FALSE;
}

static JSBool udom_set_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Err e;
	GF_FieldInfo info;
	GF_Node *n;
	char *val = NULL;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (argc==3) {
		char fullName[1024];
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;

		strcpy(fullName, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])) );
		strcat(fullName, ":");
		strcat(fullName, JS_GetStringBytes(JSVAL_TO_STRING(argv[1])) );
		e = gf_node_get_field_by_name(n, fullName, &info);
		val = JS_GetStringBytes(JSVAL_TO_STRING(argv[2]));
	} else if (argc==2) {
		e = gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info);
		val = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	} else e = GF_BAD_PARAM;
	if (!val || (e!=GF_OK)) return JS_FALSE;
	*rval = JSVAL_VOID;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_Opacity_datatype:
	case SVG_StrokeMiterLimit_datatype:
	case SVG_FontSize_datatype:
	case SVG_StrokeDashOffset_datatype:
	case SVG_AudioLevel_datatype:
	case SVG_LineIncrement_datatype:
	case SVG_Color_datatype:
	case SVG_Paint_datatype:
	/* inheritable float and unit */
	case SVG_StrokeWidth_datatype:
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
	case SMIL_AttributeType_datatype:
	case SMIL_CalcMode_datatype:
	case SMIL_Additive_datatype:
	case SMIL_Accumulate_datatype:
	case SMIL_Restart_datatype:
	case SMIL_Fill_datatype:
	case SVG_GradientUnit_datatype:
	case SVG_PreserveAspectRatio_datatype:
/*end of string traits*/
/*DOM string traits*/
	case SVG_FontFamily_datatype:
	case SVG_IRI_datatype:
	case SVG_String_datatype:
	case SVG_ContentType_datatype:
	case SVG_LanguageID_datatype:
/*end of DOM string traits*/
		gf_svg_parse_attribute((SVGElement *) n, &info, val, 0, 0);
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
		return JS_FALSE;
#endif
	}
	return JS_FALSE;
}

static JSBool udom_set_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_FieldInfo info;
	GF_Node *n;
	jsdouble d;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_NUMBER(argv[1])) return JS_FALSE;
	JS_ValueToNumber(c, argv[1], &d);
	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info) != GF_OK) return JS_FALSE;

	switch (info.fieldType) {
	/* inheritable floats */
	case SVG_Opacity_datatype:
	case SVG_StrokeMiterLimit_datatype:
	case SVG_FontSize_datatype:
	case SVG_StrokeDashOffset_datatype:
	case SVG_AudioLevel_datatype:
	case SVG_LineIncrement_datatype:
	case SVG_StrokeWidth_datatype:
	case SVG_Length_datatype:
	case SVG_Coordinate_datatype:
	{
		SVG_Number *l = (SVG_Number *)info.far_ptr;
		l->type=SVG_NUMBER_VALUE;
		l->value = FLT2FIX(d);
		break;
	}
	default:
		return JS_FALSE;
	}
	svg_node_changed(n, &info);
	return JS_TRUE;
}
static JSBool udom_set_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *mO;
	char *szName;
	GF_FieldInfo info;
	GF_Node *n;
	GF_Matrix2D *mx;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	mO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, mO, &matrixClass, NULL) ) return JS_FALSE;
	mx = JS_GetPrivate(c, mO);
	if (!mx) return JS_FALSE;

	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_Matrix_datatype) {
		gf_mx2d_copy(*(SVG_Matrix*)info.far_ptr, *mx);
		svg_node_changed(n, NULL);
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool udom_set_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *rO;
	char *szName;
	GF_FieldInfo info;
	GF_Node *n;
	rectCI *rc;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	szName = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	rO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, rO, &rectClass, NULL) ) return JS_FALSE;
	rc = JS_GetPrivate(c, rO);
	if (!rc) return JS_FALSE;

	if (gf_node_get_field_by_name(n, szName, &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_ViewBox_datatype) {
		SVG_ViewBox *v = (SVG_ViewBox *)info.far_ptr;
		v->x = FLT2FIX(rc->x);
		v->y = FLT2FIX(rc->y);
		v->width = FLT2FIX(rc->w);
		v->height = FLT2FIX(rc->h);
		svg_node_changed(n, NULL);
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool udom_set_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *path;
	GF_FieldInfo info;
	JSObject *pO;
	GF_Node *n;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	pO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, pO, &pathClass, NULL) ) return JS_FALSE;
	path = JS_GetPrivate(c, pO);
	if (!path) return JS_FALSE;
	if (gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info) != GF_OK) return JS_FALSE;

	if (info.fieldType==SVG_PathData_datatype) {
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
	}
	return JS_FALSE;
}

static JSBool udom_set_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_FieldInfo info;
	GF_Node *n;
	rgbCI *rgb;
	JSObject *colO;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;

	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	colO = JSVAL_TO_OBJECT(argv[1]);
	if (!JS_InstanceOf(c, colO, &rgbClass, NULL) ) return JS_FALSE;
	rgb = JS_GetPrivate(c, colO);
	if (!rgb) return JS_FALSE;

	*rval = JSVAL_VOID;
	if (gf_node_get_field_by_name(n, JS_GetStringBytes(JSVAL_TO_STRING(argv[0])), &info) != GF_OK) return JS_FALSE;

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
	return JS_FALSE;
}

static JSBool svg_get_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, Bool get_screen)
{
	GF_Node *n;
	GF_JSAPIParam par;
	GF_JSInterface *ifce;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n || argc) return JS_FALSE;
	ifce = n->sgprivate->scenegraph->js_ifce;
	par.bbox.is_set = 0;
	if (ifce->ScriptAction(ifce->callback, get_screen ? GF_JSAPI_OP_GET_SCREEN_BBOX : GF_JSAPI_OP_GET_LOCAL_BBOX, (GF_Node *)n, &par) && par.bbox.is_set) {
		JSObject *rO = JS_NewObject(c, &rectClass, 0, 0);
		rectCI *rc = malloc(sizeof(rectCI));
		rc->sg = NULL;
		rc->x = FIX2FLT(par.bbox.min_edge.x);
		/*BBox is in 3D coord system style*/
		rc->y = FIX2FLT(par.bbox.min_edge.y);
		rc->w = FIX2FLT(par.bbox.max_edge.x - par.bbox.min_edge.x);
		rc->h = FIX2FLT(par.bbox.max_edge.y - par.bbox.min_edge.y);
		JS_SetPrivate(c, rO, rc);
		*rval = OBJECT_TO_JSVAL(rO);
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool svg_get_local_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return svg_get_bbox(c, obj, argc, argv, rval, 0);
}
static JSBool svg_get_screen_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return svg_get_bbox(c, obj, argc, argv, rval, 1);
}

static JSBool svg_get_screen_ctm(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n;
	GF_JSAPIParam par;
	GF_JSInterface *ifce;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n || argc) return JS_FALSE;
	ifce = n->sgprivate->scenegraph->js_ifce;
	if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_GET_TRANSFORM, (GF_Node *)n, &par)) {
		JSObject *mO = JS_NewObject(c, &matrixClass, 0, 0);
		GF_Matrix2D *mx = malloc(sizeof(GF_Matrix2D));
		gf_mx2d_from_mx(mx, &par.mx);
		JS_SetPrivate(c, mO, mx);
		*rval = OBJECT_TO_JSVAL(mO);
		return JS_TRUE;
	}
	return JS_FALSE;
}

static JSBool svg_create_svg_matrix_components(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Matrix2D *mx;
	JSObject *mat;
	jsdouble v;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	if (argc!=6) return JS_FALSE;
	GF_SAFEALLOC(mx, sizeof(GF_Matrix2D));
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
	mat = JS_NewObject(c, &matrixClass, 0, 0);
	JS_SetPrivate(c, mat, mx);
	*rval = OBJECT_TO_JSVAL(mat);
	return JS_TRUE;
}
static JSBool svg_create_svg_rect(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rectCI *rc;
	JSObject *r;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	if (argc) return JS_FALSE;
	GF_SAFEALLOC(rc, sizeof(rectCI));
	r = JS_NewObject(c, &rectClass, 0, 0);
	JS_SetPrivate(c, r, rc);
	*rval = OBJECT_TO_JSVAL(r);
	return JS_TRUE;
}
static JSBool svg_create_svg_path(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *path;
	JSObject *p;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	if (argc) return JS_FALSE;
	GF_SAFEALLOC(path, sizeof(pathCI));
	p = JS_NewObject(c, &pathClass, 0, 0);
	JS_SetPrivate(c, p, path);
	*rval = OBJECT_TO_JSVAL(p);
	return JS_TRUE;
}
static JSBool svg_create_svg_color(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rgbCI *col;
	JSObject *p;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	if (argc!=3) return JS_FALSE;
	GF_SAFEALLOC(col, sizeof(rgbCI));
	col->r = JSVAL_TO_INT(argv[0]);
	col->g = JSVAL_TO_INT(argv[1]);
	col->b = JSVAL_TO_INT(argv[2]);
	p = JS_NewObject(c, &rgbClass, 0, 0);
	JS_SetPrivate(c, p, col);
	*rval = OBJECT_TO_JSVAL(p);
	return JS_TRUE;
}
static JSBool svg_move_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n;
	GF_JSAPIParam par;
	GF_JSInterface *ifce;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	if ((argc!=1) || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;
	ifce = n->sgprivate->scenegraph->js_ifce;
	par.opt = JSVAL_TO_INT(argv[1]);
	if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_SET_FOCUS, (GF_Node *)n, &par)) 
		return JS_TRUE;
	return JS_FALSE;
}
static JSBool svg_set_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *foc;
	GF_Node *n;
	GF_JSAPIParam par;
	GF_JSInterface *ifce;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	if ((argc!=1) || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n) return JS_FALSE;
	foc = JSVAL_TO_OBJECT(argv[0]);
	if (!foc || !JS_InstanceOf(c, foc, &svgClass, NULL) ) return JS_FALSE;

	ifce = n->sgprivate->scenegraph->js_ifce;
	par.focused = JS_GetPrivate(c, foc);
	/*NOT IN THE GRAPH*/
	if (!par.focused->sgprivate->num_instances) return JS_FALSE;
	if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_SET_FOCUS, (GF_Node *)n, &par)) 
		return JS_TRUE;
	return JS_FALSE;
}
static JSBool svg_get_focused_object(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n;
	GF_JSAPIParam par;
	GF_JSInterface *ifce;
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	n = JS_GetPrivate(c, obj);
	if (!n || argc) return JS_FALSE;
	
	*rval = JSVAL_VOID;
	ifce = n->sgprivate->scenegraph->js_ifce;
	if (!ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_GET_FOCUS, (GF_Node *)n, &par)) 
		return JS_FALSE;

	if (par.focused) {
		*rval = OBJECT_TO_JSVAL(svg_elt_construct(c, par.focused, 1));
	}
	return JS_TRUE;
}

static JSBool svg_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		u32 count;
		GF_JSAPIParam par;
		jsdouble *d;
		JSString *s;
		GF_Node *a_node;
		GF_JSInterface *ifce;
		SVGElement *n = JS_GetPrivate(c, obj);
		if (!n) return JS_FALSE;
		ifce = n->sgprivate->scenegraph->js_ifce;

		switch (JSVAL_TO_INT(id)) {
		/*namespaceURI*/
		case 0: 
			s = JS_NewStringCopyZ(c, "http://www.w3.org/2000/svg");
			*vp = STRING_TO_JSVAL( s );
			return JS_TRUE;
		/*localName*/
		case 1:
			s = JS_NewStringCopyZ(c, gf_node_get_class_name((GF_Node *)n) );
			*vp = STRING_TO_JSVAL( s );
			return JS_TRUE;
		/*parentNode*/
		case 2:
			a_node = gf_node_get_parent((GF_Node *) n, 0);
			if (a_node) {
				*vp = OBJECT_TO_JSVAL(svg_elt_construct(c, a_node, 1) );
			}
			return JS_TRUE;
		/*ownerDocument*/
		case 3: 
			*vp = OBJECT_TO_JSVAL(get_svg_js(c)->document);
			return JS_TRUE;
		/*textContent*/
		case 4: 
			if (n->textContent) {
				s = JS_NewStringCopyZ(c, n->textContent);
				*vp = STRING_TO_JSVAL( s );
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
		/*isPaused*/
		case 5: 
			/*need to modify all SMIL-based elements for that*/
			return JS_TRUE;
		/*firstElementChild*/
		case 6: 
			a_node = gf_list_get(n->children, 0);
			if (a_node) {
				*vp = OBJECT_TO_JSVAL(svg_elt_construct(c, a_node, 1));
			}
			return JS_TRUE;
		/*lastElementChild*/
		case 7:
			count = gf_list_count(n->children);
			a_node = count ? gf_list_get(n->children, count-1) : NULL;
			if (a_node) {
				*vp = OBJECT_TO_JSVAL(svg_elt_construct(c, a_node, 1) );
			}
			return JS_TRUE;
		/*nextElementSibling - NOT SUPPORTED*/
		case 8: return JS_TRUE;
		/*previousElementSibling - NOT SUPPORTED*/
		case 9: return JS_TRUE;
		/*id*/
		case 10: 
			if (n->sgprivate->NodeID) {
				s = JS_NewStringCopyZ(c, n->sgprivate->NodeName);
				*vp = STRING_TO_JSVAL( s );
			}
			return JS_TRUE;
		/*currentScale*/
		case 11:
			if (n->sgprivate->tag!=TAG_SVG_svg) return JS_FALSE;
			if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_GET_SCALE, (GF_Node *)n, &par)) {
				d = JS_NewDouble(c, FIX2FLT(par.val) );
				*vp = DOUBLE_TO_JSVAL(d);
				return JS_TRUE;
			}
			return JS_FALSE;
		/*currentRotate*/
		case 12: 
			if (n->sgprivate->tag!=TAG_SVG_svg) return JS_FALSE;
			if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_GET_ROTATION, (GF_Node *)n, &par)) {
				d = JS_NewDouble(c, FIX2FLT(par.val) );
				*vp = DOUBLE_TO_JSVAL(d);
				return JS_TRUE;
			}
			return JS_FALSE;
		/*currentTranslate*/
		case 13:
			if (n->sgprivate->tag!=TAG_SVG_svg) return JS_FALSE;
			if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_GET_TRANSLATE, (GF_Node *)n, &par)) {
				JSObject *r = JS_NewObject(c, &pointClass, 0, 0);
				pointCI *rc = malloc(sizeof(pointCI));
				rc->x = FIX2FLT(par.pt.x);
				rc->y = FIX2FLT(par.pt.y);
				rc->sg = n->sgprivate->scenegraph;
				JS_SetPrivate(c, r, rc);
				*vp = OBJECT_TO_JSVAL(r);
				return JS_TRUE;
			}
			return JS_FALSE;
		/*viewport*/
		case 14:
			if (n->sgprivate->tag!=TAG_SVG_svg) return JS_FALSE;
			if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_GET_VIEWPORT, (GF_Node *)n, &par)) {
				JSObject *r = JS_NewObject(c, &rectClass, 0, 0);
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
			return JS_FALSE;
		/*currentTime*/
		case 15: 
			d = JS_NewDouble(c, gf_node_get_scene_time((GF_Node *)n) );
			*vp = DOUBLE_TO_JSVAL(d);
			return JS_TRUE;
		default: 
			return JS_FALSE;
		}
	} else if (JSVAL_IS_STRING(id)) {
		SVGElement *n = JS_GetPrivate(c, obj);
		GF_FieldInfo info;
		char *name = JS_GetStringBytes(JSVAL_TO_STRING(id));
		if ( gf_node_get_field_by_name((GF_Node *)n, name, &info) != GF_OK) return JS_TRUE;

		switch (info.fieldType) {
		case SVG_Opacity_datatype:
		case SVG_AudioLevel_datatype:
		case SVG_Length_datatype:
		case SVG_Coordinate_datatype: 
		case SVG_StrokeWidth_datatype:
		case SVG_NumberOrPercentage_datatype:
		case SVG_StrokeMiterLimit_datatype:
		case SVG_FontSize_datatype:
		case SVG_StrokeDashOffset_datatype:
		case SVG_LineIncrement_datatype:
		case SVG_Rotate_datatype:
		case SVG_Number_datatype:
			if ( ((SVG_Number*)info.far_ptr)->type==SVG_NUMBER_INHERIT) {
				JSString *s = JS_NewStringCopyZ(c, "inherit");
				*vp = STRING_TO_JSVAL(s);
			} else {
				jsdouble *d = JS_NewDouble(c, FIX2FLT(((SVG_Number*)info.far_ptr)->value) );
				*vp = DOUBLE_TO_JSVAL(d);
			}
			return JS_TRUE;
		}
	}
	return JS_TRUE;
}
static JSBool svg_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &svgClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		GF_JSInterface *ifce;
		jsdouble d;
		GF_JSAPIParam par;
		SVGElement *n = JS_GetPrivate(c, obj);
		if (!n) return JS_FALSE;
		ifce = n->sgprivate->scenegraph->js_ifce;
		/*only read-only ones*/
		switch (JSVAL_TO_INT(id)) {
		/*textContent*/
		case 4:
			if (!JSVAL_IS_STRING(*vp)) return JS_FALSE;
			if (n->textContent) free(n->textContent);
			n->textContent = strdup(JS_GetStringBytes(JSVAL_TO_STRING(*vp)));
			svg_node_changed((GF_Node *) n, NULL);
			return JS_TRUE;
		/*id*/
		case 10: return JS_TRUE;
		/*currentScale*/
		case 11: 
			if (!JSVAL_IS_NUMBER(*vp) || (n->sgprivate->tag!=TAG_SVG_svg)) return JS_FALSE;
			JS_ValueToNumber(c, *vp, &d);
			par.val = FLT2FIX(d);
			if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_SET_SCALE, (GF_Node *)n, &par)) {
				return JS_TRUE;
			}
			return JS_FALSE;
		/*currentRotate*/
		case 12: 
			if (!JSVAL_IS_NUMBER(*vp) || (n->sgprivate->tag!=TAG_SVG_svg)) return JS_FALSE;
			JS_ValueToNumber(c, *vp, &d);
			par.val = FLT2FIX(d);
			if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_SET_ROTATION, (GF_Node *)n, &par)) {
				return JS_TRUE;
			}
			return JS_FALSE;
		/*currentTime*/
		case 15:
			if (!JSVAL_IS_NUMBER(*vp) || (n->sgprivate->tag!=TAG_SVG_svg)) return JS_FALSE;
			JS_ValueToNumber(c, *vp, &d);
			par.time = d;
			if (ifce->ScriptAction(ifce->callback, GF_JSAPI_OP_SET_TIME, (GF_Node *)n, &par)) {
				return JS_TRUE;
			}
			return JS_FALSE;
		default: 
			return JS_FALSE;
		}
	} else if (JSVAL_IS_STRING(id)) {
		SVGElement *n = JS_GetPrivate(c, obj);
		GF_FieldInfo info;
		char *name = JS_GetStringBytes(JSVAL_TO_STRING(id));
		if ( gf_node_get_field_by_name((GF_Node *)n, name, &info) != GF_OK) return JS_TRUE;

		if (JSVAL_IS_STRING(*vp)) {
			char *str = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
			gf_svg_parse_attribute(n, &info, str, 0, 0);
		} else {
			switch (info.fieldType) {
			case SVG_Opacity_datatype:
			case SVG_AudioLevel_datatype:
			case SVG_Length_datatype:
			case SVG_Coordinate_datatype: 
			case SVG_StrokeWidth_datatype:
			case SVG_NumberOrPercentage_datatype:
			case SVG_StrokeMiterLimit_datatype:
			case SVG_FontSize_datatype:
			case SVG_StrokeDashOffset_datatype:
			case SVG_LineIncrement_datatype:
			case SVG_Rotate_datatype:
			case SVG_Number_datatype:
				if (JSVAL_IS_NUMBER(*vp)) {
					jsdouble d;
					JS_ValueToNumber(c, *vp, &d);
					((SVG_Number*)info.far_ptr)->type = SVG_NUMBER_VALUE;
					((SVG_Number*)info.far_ptr)->value = FLT2FIX(d);
				} else {
					return JS_FALSE;
				}
				break;
			default:
				return JS_FALSE;
			}
			svg_node_changed((GF_Node *)n, &info);
			return JS_TRUE;
		}
	}
	return JS_TRUE;
}


static JSPropertySpec svgClassProps[] = {
	/*node interface*/
	{"namespaceURI",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"localName",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"parentNode",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"ownerDocument",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"textContent",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
	/*timeControl interface*/
	{"isPaused",		5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*elementTraversal interface*/
	{"firstElementChild",		6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"lastElementChild",		7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"nextElementSibling",		8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"previousElementSibling",	9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*svgElement interface*/
	{"id",						10,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
	/*svgSVGElement interface*/
	{"currentScale",			11,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
	{"currentRotate",			12,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
	{"currentTranslate",		13,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"viewport",				14,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"currentTime",				15,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
	{0}
};

static JSFunctionSpec svgClassFuncs[] = {
	/*node interface*/
	{"appendChild", svg_elt_append, 1},
	{"insertBefore", svg_elt_insert, 2},
	{"removeChild", svg_elt_remove, 1},
	{"cloneNode", svg_elt_clone, 1},
	/*element interface*/
	{"getAttributeNS", svg_elt_get_attr, 2},
	{"setAttributeNS", svg_elt_set_attr, 3},
	{"getAttribute", svg_elt_get_attr, 1},
	{"setAttribute", svg_elt_set_attr, 2},
	/*eventTarget interface*/
	{"addEventListenerNS", udom_add_listener, 4},
	{"removeEventListenerNS", udom_remove_listener, 4},
	{"addEventListener", udom_add_listener, 3},
	{"removeEventListener", udom_remove_listener, 3},
	/*timeControl interface*/
	{"beginElementAt", svg_smil_begin, 1},
	{"beginElement", svg_smil_begin, 0},
	{"endElementAt", svg_smil_end, 1},
	{"endElement", svg_smil_end, 0},
	{"pauseElement", svg_smil_pause, 0},
	{"resumeElement", svg_smil_resume, 0},
	/*trait access interface*/
	{"getTrait", udom_get_trait, 1},
	{"getTraitNS", udom_get_trait, 2},
	{"getFloatTrait", udom_get_float_trait, 1},
	{"getMatrixTrait", udom_get_matrix_trait, 1},
	{"getRectTrait", udom_get_rect_trait, 1},
	{"getPathTrait", udom_get_path_trait, 1},
	{"getRGBColorTrait", udom_get_rgb_color_trait, 1},
	/*FALLBACK TO BASE-VALUE FOR NOW - WILL NEED EITHER DOM TREE-CLONING OR A FAKE RENDER
	PASS FOR EACH PRESENTATION VALUE ACCESS*/
	{"getPresentationTrait", udom_get_trait, 1},
	{"getPresentationTraitNS", udom_get_trait, 2},
	{"getFloatPresentationTrait", udom_get_float_trait, 1},
	{"getMatrixPresentationTrait", udom_get_matrix_trait, 1},
	{"getRectPresentationTrait", udom_get_rect_trait, 1},
	{"getPathPresentationTrait", udom_get_path_trait, 1},
	{"getRGBColorPresentationTrait", udom_get_rgb_color_trait, 1},
	{"setTrait", udom_set_trait, 2},
	{"setTraitNS", udom_set_trait, 3},
	{"setFloatTrait", udom_set_float_trait, 2},
	{"setMatrixTrait", udom_set_matrix_trait, 2},
	{"setRectTrait", udom_set_rect_trait, 2},
	{"setPathTrait", udom_set_path_trait, 2},
	{"setRGBColorTrait", udom_set_rgb_color_trait, 2},
	/*locatable interface*/
	{"getBBox", svg_get_local_bbox, 0},
	{"getScreenCTM", svg_get_screen_ctm, 0},
	{"getScreenBBox", svg_get_screen_bbox, 0},
	/*svgSVGElement interface*/
	{"createSVGMatrixComponents", svg_create_svg_matrix_components, 0},
	{"createSVGRect", svg_create_svg_rect, 0},
	{"createSVGPath", svg_create_svg_path, 0},
	{"createSVGRGBColor", svg_create_svg_color, 0},
	{"moveFocus", svg_move_focus, 0},
	{"setFocus", svg_set_focus, 0},
	{"getCurrentFocusedObject", svg_get_focused_object, 0},
	{0}
};


static JSClass eventClass;
static JSPropertySpec eventProps[] = {
	{"type",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"target",			1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"currentTarget",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"namespaceURI",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"bubbles",			4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"cancelable",		5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"detail",			6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*mouse*/
	{"screenX",			7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"screenY",			8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"clientX",			9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"clientY",			10,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"button",			11,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"relatedTarget",	12,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*text, connectionEvent*/
	{"data",			13,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*keyboard*/
	{"keyIdentifier",	14,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"keyChar",			14,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"charCode",		14,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*wheelEvent*/
	{"wheelDelta",		15,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*progress*/
	{"lengthComputable",16,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"typeArg",			17,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"loaded",			18,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"total",			19,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	/*zoom*/
	{"zoomRectScreen",	20,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"previousScale",	21,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"previousTranslate",	22,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"newScale",		23,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"newTranslate",	24,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};

static JSBool event_stop_propagation(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_DOM_Event *evt = JS_GetPrivate(c, obj);
	if (!evt) return JS_FALSE;
	evt->event_phase = 3;
	return JS_TRUE;
}
static JSBool event_prevent_default(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_DOM_Event *evt = JS_GetPrivate(c, obj);
	if (!evt) return JS_FALSE;
	evt->event_phase = 4;
	return JS_TRUE;
}
static JSBool event_is_default_prevented(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_DOM_Event *evt = JS_GetPrivate(c, obj);
	if (!evt) return JS_FALSE;
	*rval = (evt->event_phase == 4) ? JS_TRUE : JS_FALSE;
	return JS_TRUE;
}

static JSFunctionSpec eventFuncs[] = {
	{"stopPropagation", event_stop_propagation, 0},
	{"preventDefault", event_prevent_default, 0},
	{"isDefaultPrevented", event_is_default_prevented, 0},
	/*TODO - all event initXXX methods*/
	{0},
};

static JSBool event_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	JSString *s;
	GF_DOM_Event *evt = JS_GetPrivate(c, obj);
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		/*type*/
		case 0:
			s = JS_NewStringCopyZ(c, gf_dom_event_get_name(evt->type) );
			*vp = STRING_TO_JSVAL( s );
			break;
		/*target*/
		case 1: *vp = OBJECT_TO_JSVAL(svg_elt_construct(c, evt->target, 1) ); return JS_TRUE;
		/*currentTarget*/
		case 2: *vp = OBJECT_TO_JSVAL(svg_elt_construct(c, evt->currentTarget, 1) ); return JS_TRUE;
		/*namespaceURI*/
		case 3: return JS_FALSE;
		/*bubbles*/
		case 4: *vp = evt->bubbles ? JS_TRUE : JS_FALSE; return JS_TRUE;
		/*cancelable*/
		case 5: *vp = evt->cancelable ? JS_TRUE : JS_FALSE; return JS_TRUE;
		/*screenX*/
		case 7: *vp = INT_TO_JSVAL(evt->screenX); return JS_TRUE;
		/*screenY*/
		case 8: *vp = INT_TO_JSVAL(evt->screenY); return JS_TRUE;
		/*clientX*/
		case 9: *vp = INT_TO_JSVAL(evt->clientX); return JS_TRUE;
		/*clientY*/
		case 10: *vp = INT_TO_JSVAL(evt->clientY); return JS_TRUE;
		/*button*/
		case 11: *vp = INT_TO_JSVAL(evt->detail); return JS_TRUE;
		/*relatedTarget*/
		case 12: *vp = OBJECT_TO_JSVAL(svg_elt_construct(c, evt->relatedTarget, 1) ); return JS_TRUE;
		/*keyIndentifier, keyChar, charCode: wrap up to same value*/
		case 14: *vp = INT_TO_JSVAL(evt->detail); return JS_TRUE;

		/*zoomRectScreen*/
		case 20:
		{
			JSObject *r = JS_NewObject(c, &rectClass, 0, 0);
			rectCI *rc = malloc(sizeof(rectCI));
			rc->x = FIX2FLT(evt->screen_rect.x);
			rc->y = FIX2FLT(evt->screen_rect.y);
			rc->w = FIX2FLT(evt->screen_rect.width);
			rc->h = FIX2FLT(evt->screen_rect.height);
			rc->sg = NULL;
			JS_SetPrivate(c, r, rc);
			*vp = OBJECT_TO_JSVAL(r);
			return JS_TRUE;
		}
		/*previousScale*/
		case 21:
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, evt->prev_scale) );
			return JS_TRUE;
		/*previousTranslate*/
		case 22:
		{
			JSObject *p = JS_NewObject(c, &pointClass, 0, 0);
			pointCI *pt = malloc(sizeof(pointCI));
			pt->x = FIX2FLT(evt->prev_translate.x);
			pt->y = FIX2FLT(evt->prev_translate.y);
			pt->sg = NULL;
			JS_SetPrivate(c, p, pt);
			*vp = OBJECT_TO_JSVAL(p);
			return JS_TRUE;
		}
		/*newScale*/
		case 23:
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, evt->new_scale) );
			return JS_TRUE;
		/*newTranslate*/
		case 24:
		{
			JSObject *p = JS_NewObject(c, &pointClass, 0, 0);
			pointCI *pt = malloc(sizeof(pointCI));
			pt->x = FIX2FLT(evt->new_translate.x);
			pt->y = FIX2FLT(evt->new_translate.y);
			pt->sg = NULL;
			JS_SetPrivate(c, p, pt);
			*vp = OBJECT_TO_JSVAL(p);
			return JS_TRUE;
		}

		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}

static JSPropertySpec originalEventProps[] = {
	{"originalEvent",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};

static JSBool svg_connection_set_encoding(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool svg_connection_create(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool svg_connection_connect(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool svg_connection_send(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool svg_connection_close(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSPropertySpec connectionProps[] = {
	{"connected",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};
static JSFunctionSpec connectionFuncs[] = {
	/*eventTarget interface*/
	{"addEventListenerNS", udom_add_listener, 4},
	{"removeEventListenerNS", udom_remove_listener, 4},
	{"addEventListenerNS", udom_add_listener, 3},
	{"removeEventListenerNS", udom_remove_listener, 3},
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
static JSBool rgb_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rgbCI *p;
	GF_SAFEALLOC(p, sizeof(rgbCI));
	JS_SetPrivate(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool rgb_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &rgbClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		rgbCI *col = JS_GetPrivate(c, obj);
		if (!col) return JS_FALSE;
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = INT_TO_JSVAL(col->r); return JS_TRUE;
		case 1: *vp = INT_TO_JSVAL(col->g); return JS_TRUE;
		case 2: *vp = INT_TO_JSVAL(col->b); return JS_TRUE;
		default:
			return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSBool rgb_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &rgbClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		rgbCI *col = JS_GetPrivate(c, obj);
		if (!col) return JS_FALSE;
		switch (JSVAL_TO_INT(id)) {
		case 0: col->r = JSVAL_TO_INT(*vp); return JS_TRUE;
		case 1: col->g = JSVAL_TO_INT(*vp); return JS_TRUE;
		case 2: col->b = JSVAL_TO_INT(*vp); return JS_TRUE;
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSPropertySpec rgbClassProps[] = {
	{"red",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"green",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"blue",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{0}
};

static JSBool rect_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	rectCI *p;
	GF_SAFEALLOC(p, sizeof(rectCI));
	JS_SetPrivate(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool rect_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &rectClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		jsdouble *d;
		rectCI *rc = JS_GetPrivate(c, obj);
		if (!rc) return JS_FALSE;
		if (rc->sg) {
			GF_JSAPIParam par;
			rc->sg->js_ifce->ScriptAction(rc->sg->js_ifce->callback, GF_JSAPI_OP_GET_VIEWPORT, rc->sg->RootNode, &par);
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
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSBool rect_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &rectClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		jsdouble d;
		rectCI *rc = JS_GetPrivate(c, obj);
		if (!rc) return JS_FALSE;
		JS_ValueToNumber(c, *vp, &d);
		switch (JSVAL_TO_INT(id)) {
		case 0: rc->x = (Float) d; return JS_TRUE;
		case 1: rc->y = (Float) d; return JS_TRUE;
		case 2: rc->w = (Float) d; return JS_TRUE;
		case 3: rc->h = (Float) d; return JS_TRUE;
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}

static JSPropertySpec rectClassProps[] = {
	{"x",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"y",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"width",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"height",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{0}
};

static JSBool point_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pointCI *p;
	GF_SAFEALLOC(p, sizeof(pointCI));
	JS_SetPrivate(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool point_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &pointClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		jsdouble *d;
		pointCI *pt = JS_GetPrivate(c, obj);
		if (!pt) return JS_FALSE;
		if (pt->sg) {
			GF_JSAPIParam par;
			pt->sg->js_ifce->ScriptAction(pt->sg->js_ifce->callback, GF_JSAPI_OP_GET_TRANSLATE, pt->sg->RootNode, &par);
			pt->x = FIX2FLT(par.pt.x);
			pt->y = FIX2FLT(par.pt.y);
		}
		switch (JSVAL_TO_INT(id)) {
		case 0: d = JS_NewDouble(c, pt->x); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		case 1: d = JS_NewDouble(c, pt->y); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSBool point_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JS_InstanceOf(c, obj, &pointClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		jsdouble d;
		pointCI *pt = JS_GetPrivate(c, obj);
		if (!pt) return JS_FALSE;
		JS_ValueToNumber(c, *vp, &d);
		switch (JSVAL_TO_INT(id)) {
		case 0: pt->x = (Float) d; break;
		case 1: pt->y = (Float) d; break;
		default: return JS_FALSE;
		}
		if (pt->sg) {
			GF_JSAPIParam par;
			par.pt.x = FLT2FIX(pt->x);
			par.pt.y = FLT2FIX(pt->y);
			pt->sg->js_ifce->ScriptAction(pt->sg->js_ifce->callback, GF_JSAPI_OP_SET_TRANSLATE, pt->sg->RootNode, &par);
		}
		return JS_TRUE;
	}
	return JS_TRUE;
}
static JSPropertySpec pointClassProps[] = {
	{"x",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"y",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{0}
};

static JSObject *svg_new_path_object(JSContext *c, SVG_PathData *d)
{
	JSObject *obj;
	pathCI *p;
	GF_SAFEALLOC(p, sizeof(pathCI));
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
	obj = JS_NewObject(c, &pathClass, 0, 0);
	JS_SetPrivate(c, obj, p);
	return obj;
}

static JSBool pathCI_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	pathCI *p;
	GF_SAFEALLOC(p, sizeof(pathCI));
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
	if (!JS_InstanceOf(c, obj, &pathClass, NULL) ) return JS_FALSE;
	if (JSVAL_IS_INT(id)) {
		pathCI *p = JS_GetPrivate(c, obj);
		if (!p) return JS_FALSE;
		switch (JSVAL_TO_INT(id)) {
		case 0: *vp = INT_TO_JSVAL(p->nb_coms); return JS_TRUE;
		default: return JS_FALSE;
		}
	}
	return JS_TRUE;
}
static JSBool svg_path_get_segment(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	pathCI *p;
	u32 idx;
	if (!JS_InstanceOf(c, obj, &pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=1) || !JSVAL_IS_INT(argv[0])) return JS_FALSE;
	idx = JSVAL_TO_INT(argv[0]);
	if (idx>=p->nb_coms) return JS_FALSE;
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
	return JS_FALSE;
}

static JSBool svg_path_get_segment_param(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble *d;
	pathCI *p;
	ptCI *pt;
	u32 i, idx, param_idx, pt_idx;
	if (!JS_InstanceOf(c, obj, &pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=2) || !JSVAL_IS_INT(argv[0]) || !JSVAL_IS_INT(argv[1])) return JS_FALSE;
	idx = JSVAL_TO_INT(argv[0]);
	param_idx = JSVAL_TO_INT(argv[1]);
	if (idx>=p->nb_coms) return JS_FALSE;
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
		if (param_idx>1) return JS_FALSE;
		pt = &p->pts[pt_idx];
		d = JS_NewDouble(c, param_idx ? pt->y : pt->x); 
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 2:/* Curve To */
		if (param_idx>5) return JS_FALSE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		d = JS_NewDouble(c, (param_idx%2) ? pt->y : pt->x);
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 3:/* Next Curve To */
		if (param_idx>5) return JS_FALSE;
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
		if (param_idx>3) return JS_FALSE;
		pt = &p->pts[pt_idx + (param_idx/2) ];
		d = JS_NewDouble(c, (param_idx%2) ? pt->y : pt->x);
		*vp = DOUBLE_TO_JSVAL(d); 
		return JS_TRUE;
	case 5:/* Next Quad To */
		if (param_idx>3) return JS_FALSE;
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
	return JS_FALSE;
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
	if (!JS_InstanceOf(c, obj, &pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=2) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) return JS_FALSE;
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
	if (!JS_InstanceOf(c, obj, &pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=2) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1])) return JS_FALSE;
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
	if (!JS_InstanceOf(c, obj, &pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=4) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1]) || !JSVAL_IS_NUMBER(argv[2]) || !JSVAL_IS_NUMBER(argv[3])) return JS_FALSE;
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
	if (!JS_InstanceOf(c, obj, &pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if ((argc!=6) || !JSVAL_IS_NUMBER(argv[0]) || !JSVAL_IS_NUMBER(argv[1]) || !JSVAL_IS_NUMBER(argv[2]) || !JSVAL_IS_NUMBER(argv[3]) || !JSVAL_IS_NUMBER(argv[4]) || !JSVAL_IS_NUMBER(argv[5])) return JS_FALSE;
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
	if (!JS_InstanceOf(c, obj, &pathClass, NULL) ) return JS_FALSE;
	p = JS_GetPrivate(c, obj);
	if (!p) return JS_FALSE;
	if (argc) return JS_FALSE;
	p->tags = realloc(p->tags, sizeof(u8)*(p->nb_coms+1) );
	p->tags[p->nb_coms] = 6;
	p->nb_coms++;
	return JS_TRUE;
}

static JSFunctionSpec pathClassFuncs[] = {
	{"getSegment", svg_path_get_segment, 1},
	{"getSegmentParam", svg_path_get_segment_param, 2},
	{"moveTo", svg_path_move_to, 2},
	{"lineTo", svg_path_line_to, 2},
	{"quadTo", svg_path_quad_to, 4},
	{"curveTo", svg_path_curve_to, 6},
	{"close", svg_path_close, 0},
	{0}
};
static JSPropertySpec pathClassProps[] = {
	{"numberOfSegments",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};


static JSBool svg_mx2d_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Matrix2D *mx = malloc(sizeof(GF_Matrix2D));
	gf_mx2d_init(*mx);
	JS_SetPrivate(c, obj, mx);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_get_component(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble *d;
	GF_Matrix2D *mx;
	if (!JS_InstanceOf(c, obj, &matrixClass, NULL) ) return JS_FALSE;
	mx = JS_GetPrivate(c, obj);
	if (!mx || (argc!=1)) return JS_FALSE;
	if (!JSVAL_IS_INT(argv[0])) return JS_FALSE;
	switch (JSVAL_TO_INT(argv[0])) {
	case 0: d = JS_NewDouble(c, FIX2FLT(mx->m[0])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 1: d = JS_NewDouble(c, FIX2FLT(mx->m[3])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 2: d = JS_NewDouble(c, FIX2FLT(mx->m[1])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 3: d = JS_NewDouble(c, FIX2FLT(mx->m[4])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 4: d = JS_NewDouble(c, FIX2FLT(mx->m[2])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	case 5: d = JS_NewDouble(c, FIX2FLT(mx->m[5])); *vp = DOUBLE_TO_JSVAL(d); return JS_TRUE;
	}
	return JS_FALSE;
}

static JSBool svg_mx2d_multiply(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	JSObject *mat;
	GF_Matrix2D *mx1, *mx2;
	if (!JS_InstanceOf(c, obj, &matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=1)) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	mat = JSVAL_TO_OBJECT(argv[0]);
	if (!JS_InstanceOf(c, mat, &matrixClass, NULL) ) return JS_FALSE;
	mx2 = JS_GetPrivate(c, mat);
	if (!mx2) return JS_FALSE;
	gf_mx2d_add_matrix(mx1, mx2);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_inverse(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	GF_Matrix2D *mx1;
	if (!JS_InstanceOf(c, obj, &matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1) return JS_FALSE;
	gf_mx2d_inverse(mx1);
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_translate(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble x, y;
	GF_Matrix2D *mx1;
	if (!JS_InstanceOf(c, obj, &matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=2)) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &x);
	JS_ValueToNumber(c, argv[1], &y);
	gf_mx2d_add_translation(mx1, FLT2FIX(x), FLT2FIX(y));
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_scale(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble scale;
	GF_Matrix2D *mx1;
	if (!JS_InstanceOf(c, obj, &matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=2)) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &scale);
	gf_mx2d_add_scale(mx1, FLT2FIX(scale), FLT2FIX(scale));
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSBool svg_mx2d_rotate(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *vp)
{
	jsdouble angle;
	GF_Matrix2D *mx1;
	if (!JS_InstanceOf(c, obj, &matrixClass, NULL) ) return JS_FALSE;
	mx1 = JS_GetPrivate(c, obj);
	if (!mx1 || (argc!=2)) return JS_FALSE;
	JS_ValueToNumber(c, argv[0], &angle);
	gf_mx2d_add_rotation(mx1, 0, 0, FLT2FIX(angle));
	*vp = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static JSFunctionSpec matrixClassFuncs[] = {
	{"getComponent", svg_mx2d_get_component, 1},
	{"mMultiply", svg_mx2d_multiply, 1},
	{"inverse", svg_mx2d_inverse, 0},
	{"mTranslate", svg_mx2d_translate, 2},
	{"mScale", svg_mx2d_scale, 1},
	{"mRotate", svg_mx2d_rotate, 1},
	{0}
};


static void svg_init_js_api(GF_SceneGraph *scene)
{
	scene->svg_js->node_bank = gf_list_new();

	JS_SetContextPrivate(scene->svg_js->js_ctx, scene);
	JS_SetErrorReporter(scene->svg_js->js_ctx, svg_script_error);

	/*init global object*/
	uDOM_SETUP_CLASS(globalClass, "Global", 0, global_getProperty, JS_PropertyStub, JS_FinalizeStub);
	scene->svg_js->global = JS_NewObject(scene->svg_js->js_ctx, &globalClass, 0, 0 );
	JS_InitStandardClasses(scene->svg_js->js_ctx, scene->svg_js->global);
	JS_DefineFunctions(scene->svg_js->js_ctx, scene->svg_js->global, globalClassFuncs);
	JS_DefineProperties(scene->svg_js->js_ctx, scene->svg_js->global, globalClassProps);


	/*document class*/
	uDOM_SETUP_CLASS(docClass, "SVGDocument", JSCLASS_HAS_PRIVATE, svg_doc_getProperty, svg_doc_setProperty, JS_FinalizeStub);
	JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &docClass, 0, 0, docClassProps, docClassFuncs, 0, 0);
	scene->svg_js->document = JS_DefineObject(scene->svg_js->js_ctx, scene->svg_js->global, "document", &docClass, 0, 0 );
	JS_SetPrivate(scene->svg_js->js_ctx, scene->svg_js->document, scene);

	/*Event class*/
	uDOM_SETUP_CLASS(eventClass , "Event", JSCLASS_HAS_PRIVATE, event_getProperty, JS_PropertyStub, JS_FinalizeStub);
	JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &eventClass, 0, 0, eventProps, eventFuncs, 0, 0);
	scene->svg_js->event = JS_DefineObject(scene->svg_js->js_ctx, scene->svg_js->global, "evt", &eventClass, 0, 0);

	/*element class*/
	uDOM_SETUP_CLASS(svgClass, "SVGElement", JSCLASS_HAS_PRIVATE, svg_getProperty, svg_setProperty, svg_elt_finalize);
	JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &svgClass, 0, 0, svgClassProps, svgClassFuncs, 0, 0);

	/*RGBColor class*/
	uDOM_SETUP_CLASS(rgbClass, "SVGRGBColor", JSCLASS_HAS_PRIVATE, rgb_getProperty, rgb_setProperty, baseCI_finalize);
	JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &rgbClass, rgb_constructor, 0, rgbClassProps, 0, 0, 0);
	/*SVGRect class*/
	uDOM_SETUP_CLASS(rectClass, "SVGRect", JSCLASS_HAS_PRIVATE, rect_getProperty, rect_setProperty, baseCI_finalize);
	JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &rectClass, rect_constructor, 0, rectClassProps, 0, 0, 0);
	/*SVGPoint class*/
	uDOM_SETUP_CLASS(pointClass, "SVGPoint", JSCLASS_HAS_PRIVATE, point_getProperty, point_setProperty, baseCI_finalize);
	JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &pointClass, point_constructor, 0, pointClassProps, 0, 0, 0);
	/*SVGMatrix class*/
	uDOM_SETUP_CLASS(matrixClass, "SVGMatrix", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, baseCI_finalize);
	JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &matrixClass, svg_mx2d_constructor, 0, 0, matrixClassFuncs, 0, 0);
	/*SVGPath class*/
	uDOM_SETUP_CLASS(pathClass, "SVGPath", JSCLASS_HAS_PRIVATE, path_getProperty, JS_PropertyStub, pathCI_finalize);
	JS_InitClass(scene->svg_js->js_ctx, scene->svg_js->global, 0, &pathClass, pathCI_constructor, 0, pathClassProps, pathClassFuncs, 0, 0);

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

static void svg_script_predestroy(GF_Node *n)
{
	GF_SVGJS *svg_js = n->sgprivate->scenegraph->svg_js;
	if (svg_js->nb_scripts) {
		svg_js->nb_scripts--;
		if (!svg_js->nb_scripts) {
			gf_sg_ecmascript_del(svg_js->js_ctx);
			gf_list_del(svg_js->node_bank);
			free(svg_js);
			n->sgprivate->scenegraph->svg_js = NULL;
		}
	}
}

static void svg_node_destroy(GF_SceneGraph *sg, GF_Node *n)
{
	u32 i, count;
	count = gf_list_count(sg->svg_js->node_bank);
	for (i=0; i<count; i++) {
		JSObject *obj = gf_list_get(sg->svg_js->node_bank, i);
		if (JS_GetPrivate(sg->svg_js->js_ctx, obj)==n) {
			JS_SetPrivate(sg->svg_js->js_ctx, obj, NULL);
			gf_list_rem(sg->svg_js->node_bank, i);
			return;
		}
	}

}

void JSScript_LoadSVG(GF_Node *node)
{
	GF_SVGJS *svg_js;
	JSBool ret;
	jsval rval;
	SVGscriptElement *script = (SVGscriptElement *)node;
	if (/*!script->xlink->type || strcmp(script->xlink->type, "text/ecmascript") || */ !script->textContent) return;

	if (!node->sgprivate->scenegraph->svg_js) {
		GF_SVGJS *svg_js;
		GF_SAFEALLOC(svg_js, sizeof(GF_SVGJS));
		/*create new ecmascript context*/
		svg_js->js_ctx = gf_sg_ecmascript_new();
		if (!svg_js->js_ctx) {
			free(svg_js);
			return;
		}
		svg_js->on_node_destroy = svg_node_destroy;
		svg_js->script_execute = svg_script_execute;
		node->sgprivate->scenegraph->svg_js = svg_js;
		/*load SVG & DOM APIs*/
		svg_init_js_api(node->sgprivate->scenegraph);
	}
	svg_js = node->sgprivate->scenegraph->svg_js;
	if (!node->sgprivate->PreDestroyNode ) {
		svg_js->nb_scripts++;
		node->sgprivate->PreDestroyNode = svg_script_predestroy;
	}

	ret = JS_EvaluateScript(svg_js->js_ctx, svg_js->global, script->textContent, strlen(script->textContent), 0, 0, &rval);
	if (ret==JS_FALSE) {
		node->sgprivate->scenegraph->js_ifce->ScriptMessage(node->sgprivate->scenegraph->js_ifce->callback, GF_SCRIPT_ERROR, "SVG: Invalid script");
		return;
	}
	return;
}


#endif

#endif
