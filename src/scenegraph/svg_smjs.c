/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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


/*these are all static variables, because scripts should be shared in the doc */
static JSContext *js_svg_ctx = NULL;
static struct JSObject *js_svg_glob = NULL;
static struct JSObject *js_svg_doc = NULL;
static u32 nb_scripts = 0;

static JSClass globalClass;
static JSClass docClass;
static JSClass svgAnyClass;


static void svg_script_error(JSContext *c, const char *msg, JSErrorReport *jserr)
{
	GF_SceneGraph *sg = JS_GetContextPrivate(c);
	sg->js_ifce->Error(sg->js_ifce->callback, msg);
}


static JSBool js_print(JSContext *c, JSObject *p, uintN argc, jsval *argv, jsval *rval)
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
	sg->js_ifce->Print(sg->js_ifce->callback, buf);
	return JS_TRUE;
}

static JSFunctionSpec globalFunctions[] = {
    {"print",           js_print,          0},
    {"Print",           js_print,          0},
    { 0 }
};

static JSBool udom_doc_create_element(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_doc_get_element_by_id(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

static JSBool udom_element_append(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_element_insert(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_element_remove(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_element_get_attr(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	fprintf(stdout, "HeHooo!!\n");
	return JS_FALSE;
}
static JSBool udom_element_set_attr(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	fprintf(stdout, "HeHiii!!\n");
	return JS_FALSE;
}
static JSBool udom_element_get_attr_ns(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_element_set_attr_ns(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_element_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_element_rem_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

static JSBool udom_listener_handle_event(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

static JSBool udom_smil_begin(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_smil_end(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_smil_pause(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_smil_resume(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

static JSBool udom_get_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_get_trait_ns(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_get_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_get_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_get_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_get_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_get_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

static JSBool udom_set_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_set_trait_ns(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_set_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_set_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_set_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_set_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_set_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

static JSBool udom_any_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	fprintf(stdout, "getting props\n");
	if (JSVAL_IS_INT(id)) {
		u32 theid = JSVAL_TO_INT(id);
	} else if (JSVAL_IS_STRING(id)) {
		char *str = JS_GetStringBytes(JSVAL_TO_STRING(id));
	}
	return JS_FALSE;
}
static JSBool udom_any_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	fprintf(stdout, "setting props\n");
	return JS_FALSE;
}


static JSBool udom_doc_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	return JS_FALSE;
}
static JSBool udom_doc_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	return JS_FALSE;
}

#define uDOM_PROPS_node \
	{"__dummy",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT},						\
	{"namespaceURI",	1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"localName",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"parentNode",		3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"ownerDocument",	4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"textContent",		5,       JSPROP_ENUMERATE | JSPROP_PERMANENT},	\

#define	uDOM_FUNCS_node \
	{"appendChild", udom_element_append, 1},	\
	{"insertBefore", udom_element_insert, 2},	\
	{"removeChild", udom_element_remove, 1},	\

static JSPropertySpec nodeProps[] = {
	uDOM_PROPS_node
	{0}
};

static JSFunctionSpec nodeFuncs[] = {
	uDOM_FUNCS_node
	{0}
};

#define	uDOM_FUNCS_element	\
	uDOM_FUNCS_node			\
	{"getAttributeNS", udom_element_get_attr_ns, 2},	\
	{"setAttributeNS", udom_element_set_attr_ns, 3},	\
	{"getAttribute", udom_element_get_attr, 1},	\
	{"setAttribute", udom_element_set_attr, 2},	\

#define	uDOM_PROPS_element	uDOM_PROPS_node

static JSFunctionSpec elementFuncs[] = {
	uDOM_FUNCS_element
	{0}
};
static JSPropertySpec elementProps[] = {
	uDOM_PROPS_element
	{0}
};

#define uDOM_FUNCS_document		\
	uDOM_FUNCS_node		\
    {"createElementNS",  udom_doc_create_element,    2},	\
    {"getElementById",   udom_doc_get_element_by_id,  1},	\

#define uDOM_PROPS_document\
	uDOM_PROPS_node	\
	{"documentElement",  6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},

static JSFunctionSpec docFuncs[] = {
	uDOM_FUNCS_document
    { 0 }
};
static JSPropertySpec docProps[] = {
	uDOM_PROPS_document
	{0}
};

#define uDOM_FUNCS_eventTarget	\
	{"addEventListenerNS", udom_element_add_listener, 4},	\
	{"removeEventListenerNS", udom_element_rem_listener, 4},	\

static JSFunctionSpec evtTargetFuncs[] = {
	uDOM_FUNCS_eventTarget
	{0}
};

#define uDOM_FUNCS_eventListener	\
	{"handleEvent", udom_listener_handle_event, 1},	\

static JSFunctionSpec evtListenerFuncs[] = {
	uDOM_FUNCS_eventListener
	{0}
};


#define uDOM_FUNCS_timeControl \
	{"beginElementAt", udom_smil_begin, 1},	\
	{"beginElement", udom_smil_begin, 0},	\
	{"endElementAt", udom_smil_end, 1},	\
	{"endElement", udom_smil_end, 0},	\
	{"pauseElement", udom_smil_pause, 0},	\
	{"resumeElement", udom_smil_resume, 0},	\

static JSFunctionSpec timeCtrlFuncs[] = {
	uDOM_FUNCS_timeControl
	{0}
};
#define uDOM_PROPS_timeControl	\
	{"isPaused",	7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\

static JSPropertySpec timeCtrlProps[] = {
	uDOM_PROPS_timeControl
	{0}
};

static JSClass eventClass;
static JSPropertySpec eventProps[] = {
	{"__dummy",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"target",			1,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"currentTarget",	2,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"type",			3,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"detail",			4,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	/*mouse*/
	{"screenX",			5,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"screenY",			6,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"clientX",			7,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"clientY",			8,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"button",			9,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	/*text, connectionEvent*/
	{"data",			10,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	/*keyboard*/
	{"keyIdentifier",	11,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	/*wheelEvent*/
	{"wheelDelta",		12,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	/*progress*/
	{"lengthComputable",13,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"typeArg",			14,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"loaded",			15,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"total",			16,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{0}
};static JSBool udom_get_target(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	fprintf(stdout, "whesh\n");
	return JS_FALSE;
}
static JSFunctionSpec eventFuncs[] = {
	{"getTarget", udom_get_target, 0},
	{0,0,0,0,0},
};

static JSBool event_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	JSObject *res;
	GF_DOM_Event *evt = JS_GetPrivate(c, obj);
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		case 0:
			res = JS_NewObject(c, &svgAnyClass, 0, 0 );
			JS_SetPrivate(js_svg_ctx, res, evt->target);
			*vp = OBJECT_TO_JSVAL(res);
			return JS_TRUE;
		default: 
			break;
		}
	} else if (JSVAL_IS_STRING(id)) {
		char *str = JS_GetStringBytes(JSVAL_TO_STRING(id));
	}
	return JS_FALSE;
}
static JSBool event_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	return JS_FALSE;
}

static JSPropertySpec originalEventProps[] = {
	{"originalEvent",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};


#define	uDOM_FUNCS_traitAccess	\
	{"getTrait", udom_get_trait, 1},	\
	{"getTraitNS", udom_get_trait_ns, 2},	\
	{"getFloatTrait", udom_get_float_trait, 1},	\
	{"getMatrixTrait", udom_get_matrix_trait, 1},	\
	{"getRectTrait", udom_get_rect_trait, 1},	\
	{"getPathTrait", udom_get_path_trait, 1},	\
	{"getRGBColorTrait", udom_get_rgb_color_trait, 1},	\
	{"getPresentationTrait", udom_get_trait, 1},	\
	{"getPresentationTraitNS", udom_get_trait_ns, 2},	\
	{"getFloatPresentationTrait", udom_get_float_trait, 1},	\
	{"getMatrixPresentationTrait", udom_get_matrix_trait, 1},	\
	{"getRectPresentationTrait", udom_get_rect_trait, 1},	\
	{"getPathPresentationTrait", udom_get_path_trait, 1},	\
	{"getRGBColorPresentationTrait", udom_get_rgb_color_trait, 1},	\
	{"setTrait", udom_set_trait, 2},	\
	{"setTraitNS", udom_set_trait_ns, 3},	\
	{"setFloatTrait", udom_set_float_trait, 2},	\
	{"setMatrixTrait", udom_set_matrix_trait, 2},	\
	{"setRectTrait", udom_set_rect_trait, 2},	\
	{"setPathTrait", udom_set_path_trait, 2},	\
	{"setRGBColorTrait", udom_set_rgb_color_trait, 2},	\

static JSBool udom_connection_create(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_connection_connect(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_connection_send(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_connection_close(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

static JSPropertySpec connectionProps[] = {
	{"connected",			0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};
static JSFunctionSpec connectionFuncs[] = {
	uDOM_FUNCS_eventTarget
	{"connect", udom_connection_connect, 1},
	{"send", udom_connection_send, 1},
	{"close", udom_connection_close, 0},
	{0}
};

static JSBool udom_nav_to_location(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSPropertySpec svgGlobalProps[] = {
	{"connected",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"parent",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{0}
};
static JSFunctionSpec svgGlobalFuncs[] = {
	{"createConnection", udom_connection_create, 0},
	{"gotoLocation", udom_nav_to_location, 1},
	{0}
};

static JSPropertySpec svgDocumentProps[] = {
	uDOM_PROPS_document
	{0}
};
static JSFunctionSpec svgDocumentFuncs[] = {
	uDOM_FUNCS_document
	uDOM_FUNCS_eventTarget
	{0}
};

#define uDOM_PROPS_elementTraversal		\
	{"firstElementChild",		8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"lastElementChild",		9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"nextElementSibling",		10,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"previousElementSibling",	11,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\


static JSPropertySpec svgElementTraversalProps[] = {
	uDOM_PROPS_elementTraversal
};


#define uDOM_PROPS_svgElement	\
	uDOM_PROPS_element			\
	uDOM_PROPS_elementTraversal	\
	{"id",	12,       JSPROP_ENUMERATE | JSPROP_PERMANENT},	\

#define uDOM_FUNCS_svgElement	\
	uDOM_FUNCS_element	\
	uDOM_FUNCS_eventTarget	\
	uDOM_FUNCS_traitAccess	\

static JSPropertySpec svgElementProps[] = {
	uDOM_PROPS_svgElement
	{0}
};
static JSFunctionSpec svgElementFuncs[] = {
	uDOM_FUNCS_svgElement
	{0}
};

#define uDOM_PROPS_animation	\
	uDOM_FUNCS_svgElement	\
	uDOM_FUNCS_timeControl \

static JSFunctionSpec svgAnimationFuncs[] = {
	uDOM_PROPS_animation
	{0}
};

static JSBool udom_get_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_get_screen_ctm(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_get_screen_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
#define	uDOM_FUNCS_locatable	\
	{"getBBox", udom_get_bbox, 0},	\
	{"getScreenCTM", udom_get_screen_ctm, 0},	\
	{"getScreenBBox", udom_get_screen_bbox, 0},	\

#define uDOM_PROPS_locatable

static JSFunctionSpec svgLocatableFuncs[] = {
	uDOM_FUNCS_locatable
	{0}
};
static JSFunctionSpec svgLocatableElementFuncs[] = {
	uDOM_FUNCS_svgElement
	uDOM_FUNCS_locatable
	{0}
};
static JSPropertySpec svgLocatableElementProps[] = {
	uDOM_PROPS_svgElement
	{0}
};

static JSBool udom_create_svg_matrix_components(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_create_svg_rect(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_create_svg_path(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_create_svg_color(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_move_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_set_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}
static JSBool udom_get_focused_object(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return JS_FALSE;
}

#define uDOM_FUNCS_svgSVG		\
	{"createSVGMatrixComponents", udom_create_svg_matrix_components, 0},	\
	{"createSVGRect", udom_create_svg_rect, 0},	\
	{"createSVGPath", udom_create_svg_path, 0},	\
	{"createSVGRGBColor", udom_create_svg_color, 0},	\
	{"moveFocus", udom_move_focus, 0},	\
	{"setFocus", udom_set_focus, 0},	\
	{"getCurrentFocusedObject", udom_get_focused_object, 0},	\

#define uDOM_PROPS_svgSVG		\
	{"currentScale",	13,       JSPROP_ENUMERATE | JSPROP_PERMANENT},		\
	{"currentRotate",	14,       JSPROP_ENUMERATE | JSPROP_PERMANENT},		\
	{"currentTranslate",15,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"viewport",		16,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"currentTime",		17,       JSPROP_ENUMERATE | JSPROP_PERMANENT},						\

static JSFunctionSpec svgSVGElementFuncs[] = {
	uDOM_FUNCS_svgElement
	uDOM_FUNCS_locatable
	uDOM_FUNCS_timeControl
	uDOM_FUNCS_svgSVG
	{0}
};

static JSPropertySpec svgSVGElementProps[] = {
	uDOM_PROPS_svgElement
	uDOM_PROPS_locatable
	uDOM_PROPS_timeControl
	uDOM_PROPS_svgSVG
};


static JSFunctionSpec svgAnyElementFuncs[] = {
	uDOM_FUNCS_svgElement
/*	uDOM_FUNCS_locatable
	uDOM_FUNCS_timeControl
	uDOM_FUNCS_svgSVG
*/	{0}
};

static JSPropertySpec svgAnyElementProps[] = {
	{"__dummy",       0,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
/*	uDOM_PROPS_svgElement
	uDOM_PROPS_locatable
	uDOM_PROPS_timeControl
	uDOM_PROPS_svgSVG
*/	{0}
};

static JSPropertySpec svgColorProps[] = {
	{"red",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"green",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
	{"blue",0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
};

static JSPropertySpec svgRectProps[] = {
	{"x",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"y",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"width",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"height",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
};

static JSPropertySpec svgPointProps[] = {
	{"x",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
	{"y",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT },
};
/*
static JSFunctionSpec svgPathFuncs[] = {
	{"getSegment", udom_path_get_segment, 1},
	{"getSegmentParam", udom_path_get_segment_param, 2},
	{"moveTo", udom_path_move_to, 2},
	{"lineTo", udom_path_line_to, 2},
	{"quadTo", udom_path_quad_to, 4},
	{"curveTo", udom_path_quad_to, 6},
	{"close", udom_path_close, 0},
};
*/
static JSPropertySpec svgPathProps[] = {
	{"numberOfSegments",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
};
/*
static JSFunctionSpec svgMatrixFuncs[] = {
	{"getComponent", udom_mx2d_get_component, 1},
	{"mMultiply", udom_mx2d_multiply, 1},
	{"inverse", udom_mx2d_inverse, 0},
	{"mTranslate", udom_mx2d_translate, 1},
	{"mTranslate", udom_mx2d_translate, 2},
	{"mScale", udom_mx2d_scale, 1},
	{"mRotate", udom_mx2d_rotate, 1},
};
*/
static JSBool svgAnyConstructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}
static void svg_init_js_api(GF_SceneGraph *scene)
{
	JSObject *obj;
	JS_SetContextPrivate(js_svg_ctx, scene);
	JS_SetErrorReporter(js_svg_ctx, svg_script_error);

	/*init global object (browser or whatever we will define as our extensions*/
	uDOM_SETUP_CLASS(globalClass, "global", 0, JS_PropertyStub, JS_PropertyStub, JS_FinalizeStub);

	js_svg_glob = JS_NewObject(js_svg_ctx, &globalClass, 0, 0 );
	JS_InitStandardClasses(js_svg_ctx, js_svg_glob);
	JS_DefineFunctions(js_svg_ctx, js_svg_glob, globalFunctions );
#if 0
	/*init document object (single instance)*/
	uDOM_SETUP_CLASS(docClass , "document", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub, JS_FinalizeStub);
	js_svg_doc = JS_DefineObject(js_svg_ctx, js_svg_glob, "document", &docClass, 0, 0 );
	JS_SetPrivate(js_svg_ctx, js_svg_doc, scene);
	JS_DefineProperties(js_svg_ctx, js_svg_doc, docProps);
	JS_DefineFunctions(js_svg_ctx, js_svg_doc, docFuncs);
#endif

	uDOM_SETUP_CLASS(eventClass , "Event", JSCLASS_HAS_PRIVATE, event_getProperty, event_setProperty, JS_FinalizeStub);
	obj = JS_InitClass(js_svg_ctx, js_svg_glob, 0, &eventClass, 0, 0, eventProps, eventFuncs, 0, 0);

	JS_DefineProperties(js_svg_ctx, obj, eventProps);
	JS_DefineFunctions(js_svg_ctx, obj, eventFuncs);

//	uDOM_SETUP_CLASS(svgAnyClass, "SVGAnyElement", JSCLASS_HAS_PRIVATE, udom_any_getProperty, udom_any_setProperty, JS_FinalizeStub);
//	obj = JS_InitClass(js_svg_ctx, js_svg_glob, 0, &svgAnyClass, svgAnyConstructor, 0, svgAnyElementProps, svgAnyElementFuncs, 0, 0);
}

Bool svg_script_execute(char *utf8_script, GF_DOM_Event *event)
{
	JSBool ret;
	jsval rval;
	JSObject *js_evt;

	js_evt = JS_DefineObject(js_svg_ctx, js_svg_glob, "evt", &eventClass, 0, 0);
	JS_SetPrivate(js_svg_ctx, js_evt, event);
	JS_AddRoot(js_svg_ctx, js_evt);

	JS_CallFunctionName(js_svg_ctx, js_svg_glob, "testIt", 0, NULL, NULL);
	ret = JS_EvaluateScript(js_svg_ctx, js_svg_glob, utf8_script, strlen(utf8_script), 0, 0, &rval);
	JS_RemoveRoot(js_svg_ctx, js_evt);

	if (ret==JS_FALSE) {
		char *sep = strchr(utf8_script, '(');
		if (!sep) return 0;
		sep[0] = 0;
		JS_LookupProperty(js_svg_ctx, js_svg_glob, utf8_script, &rval);
		sep[0] = '(';
		if (JSVAL_IS_VOID(rval)) return 0;
	}
	return 1;
}

static void svg_script_predestroy(GF_Node *n)
{

	if (nb_scripts) {
		nb_scripts--;
		if (!nb_scripts) {
			gf_sg_ecmascript_del(js_svg_ctx);
			js_svg_ctx = NULL;
		}
	}
}

void JSScript_LoadSVG(GF_Node *node)
{
	JSBool ret;
	jsval rval;
	SVGscriptElement *script = (SVGscriptElement *)node;
	if (!script->type || strcmp(script->type, "text/ecmascript")) return;

	if (!js_svg_ctx) {
		/*create new ecmascript context*/
		js_svg_ctx = gf_sg_ecmascript_new();
		if (!js_svg_ctx) return;
		/*load SVG & DOM APIs*/
		svg_init_js_api(node->sgprivate->scenegraph);
	}
	nb_scripts++;
	node->sgprivate->PreDestroyNode = svg_script_predestroy;

	ret = JS_EvaluateScript(js_svg_ctx, js_svg_glob, script->textContent, strlen(script->textContent), 0, 0, &rval);
	if (ret==JS_FALSE) {
		node->sgprivate->scenegraph->js_ifce->Error(node->sgprivate->scenegraph->js_ifce->callback, "SVG: Invalid script");
		return;
	}

	return;
}


#endif

#endif
