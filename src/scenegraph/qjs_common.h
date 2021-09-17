/*
 *			GPAC - Multimedia Framework C SDK
 *
 *				Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019-2021
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Management sub-project
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

#ifndef _SG_QJS_COMMON_
#define _SG_QJS_COMMON_

#include <gpac/list.h>
#include <gpac/scenegraph.h>

#include "../quickjs/quickjs.h"

/********************************************

	Common JS tools

*********************************************/

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define JS_CHECK_STRING(_v) (JS_IsString(_v) || JS_IsNull(_v))

struct JSContext *gf_js_create_context();
void gf_js_delete_context(struct JSContext *);
#ifdef GPAC_HAS_QJS
void gf_js_lock(struct JSContext *c, Bool LockIt);
Bool gf_js_try_lock(struct JSContext *c);
void gf_js_call_gc(struct JSContext *c);
#endif /* GPAC_HAS_QJS */

/* throws an error with integer property 'code' set to err*/
JSValue js_throw_err(JSContext *ctx, s32 err);
/* throws an error with integer property 'code' set to err and string property 'message' set to the formatted string*/
JSValue js_throw_err_msg(JSContext *ctx, s32 err, const char *fmt, ...);

void js_std_loop(JSContext *ctx);
void js_dump_error(JSContext *ctx);
void js_dump_error_exc(JSContext *ctx, const JSValue exception_val);

JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

void js_load_constants(JSContext *ctx, JSValue global_obj);

/********************************************

	SceneGraph JS tools

Note that the generic DOM API is also potentially used by XHR in JSFilter, not only by scenegraph

*********************************************/

#ifndef GPAC_DISABLE_SVG

typedef struct __tag_html_media_script_ctx
{
	/*global script context for the scene*/
	struct JSContext *js_ctx;
	/*global object*/
	struct JSObject *global;
	/*global event object - used to update the associated DOMEvent (JS private stack) when dispatching events*/
	struct JSObject *event;

	Bool force_gc;
} GF_HTMLMediaJS;

#endif	/*GPAC_DISABLE_SVG*/

typedef struct
{
	JSClassDef class;
	JSClassID class_id;
} GF_JSClass;


struct js_handler_context
{
	/* JavaScript context in which the listener is applicable */
	void *ctx;
	/*function value for handler */
	JSValue fun_val;
	/*target EventListener object (this) */
	JSValue evt_listen_obj;
};

struct _node_js_binding
{
	JSValue obj;
	struct __gf_js_field *pf;
	GF_List *fields;
};


void dom_node_changed(GF_Node *n, Bool child_modif, GF_FieldInfo *info);
void dom_element_finalize(JSRuntime *rt, JSValue obj);
void dom_document_finalize(JSRuntime *rt, JSValue obj);
GF_Node *dom_get_element(JSContext *c, JSValue obj);
GF_SceneGraph *dom_get_doc(JSContext *c, JSValue obj);

#ifndef GPAC_DISABLE_SVG
JSValue dom_element_construct(JSContext *c, GF_Node *n);
#endif

/*initialize DOM Core (subset) + xmlHTTPRequest API. The global object MUST have private data storage
and its private data MUST be a scenegraph. This scenegraph is only used to create new documents
and setup the callback pointers*/
void dom_js_load(GF_SceneGraph *scene, struct JSContext *c);
/*unloads the DOM core support (to be called upon destruction only, once the JSContext has been destroyed
to releases all resources used by DOM JS)*/
void dom_js_unload();
/*unloads DOM core before the JSContext is being destroyed */
void gf_sg_js_dom_pre_destroy(struct JSRuntime *rt, GF_SceneGraph *sg, GF_Node *script_or_handler_node);

JSValue dom_event_add_listener(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv);
JSValue dom_event_remove_listener(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv);
JSValue xml_dom3_not_implemented(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#ifndef GPAC_DISABLE_SVG

/*dom event listener interface, exported for XHR module*/
#define JS_DOM3_EVENT_TARGET_INTERFACE	\
	JS_CFUNC_DEF("addEventListenerNS", 3, dom_event_add_listener),	\
	JS_CFUNC_DEF("removeEventListenerNS", 3, dom_event_remove_listener),	\
	JS_CFUNC_DEF("addEventListener", 2, dom_event_add_listener),		\
	JS_CFUNC_DEF("removeEventListener", 2, dom_event_remove_listener),	\
	JS_CFUNC_DEF("dispatchEvent", 1, xml_dom3_not_implemented),

#else

#define JS_DOM3_EVENT_TARGET_INTERFACE

#endif

/*defines a new global object "evt" of type Event*/
JSValue dom_js_define_event(struct JSContext *c);

JSValue gf_dom_new_event(struct JSContext *c);

JSValue dom_document_construct_external(JSContext *c, GF_SceneGraph *sg);

/*
		Script node
*/
struct _gf_vrml_script_priv
{
	//extra script fields
	GF_List *fields;

	//BIFS coding stuff
	u32 numIn, numDef, numOut;

	void (*JS_PreDestroy)(GF_Node *node);
	void (*JS_EventIn)(GF_Node *node, GF_FieldInfo *in_field);

	Bool is_loaded;

#ifdef GPAC_HAS_QJS
	struct JSContext *js_ctx;
	JSValue js_obj;
	/*all attached JS fields with associated JS objects (eg, not created by the script) are stored here so that we don't
	allocate them again and again when getting properties. Garbage collection is performed (if needed)
	on these objects after each eventIn execution*/
	GF_List *jsf_cache;
	//Event object, whose private is the pointer to current event being executed
	JSValue the_event;
	Bool use_strict;

	/*VRML constructors*/
	JSValue SFNodeClass;
#ifndef GPAC_DISABLE_VRML
	JSValue globalClass;
	JSValue browserClass;
	JSValue SFVec2fClass;
	JSValue SFVec3fClass;
	JSValue SFRotationClass;
	JSValue SFColorClass;
	JSValue SFImageClass;
	JSValue MFInt32Class;
	JSValue MFBoolClass;
	JSValue MFFloatClass;
	JSValue MFTimeClass;
	JSValue MFVec2fClass;
	JSValue MFVec3fClass;
	JSValue MFRotationClass;
	JSValue MFColorClass;
	JSValue MFStringClass;
	JSValue MFUrlClass;
	JSValue MFNodeClass;
	JSValue AnyClass;

	JSValue node_toString_fun;
	JSValue node_getTime_fun;
#endif //GPAC_DISABLE_VRML

#ifndef GPAC_DISABLE_SVG
	JSValue node_addEventListener_fun;
	JSValue node_removeEventListener_fun;
#endif //GPAC_DISABLE_SVG

#endif //GPAC_HAS_QJS
};

/********************************************

	JSFilter tools

*********************************************/

/*provided by JSFilter only*/
struct __gf_download_manager *jsf_get_download_manager(JSContext *c);
struct _gf_ft_mgr *jsf_get_font_manager(JSContext *c);
GF_Err jsf_request_opengl(JSContext *c);
GF_Err jsf_set_gl_active(JSContext *c);
GF_Err jsf_get_filter_packet_planes(JSContext *c, JSValue obj, u32 *width, u32 *height, u32 *pf, u32 *stride, u32 *stride_uv, const u8 **data, const u8 **p_u, const u8 **p_v, const u8 **p_a);

Bool jsf_is_packet(JSContext *c, JSValue obj);
const char *jsf_get_script_filename(JSContext *c);


/*special init of XHR as builtin of DOM rather than standalone module*/
void qjs_module_init_xhr_global(JSContext *c, JSValue global);

void qjs_init_all_modules(JSContext *ctx, Bool no_webgl, Bool for_vrml);

Bool gs_js_context_is_valid(JSContext *ctx);
JSRuntime *gf_js_get_rt();

const char *jsf_get_script_filename(JSContext *c);


#define GF_JS_EXCEPTION(_ctx) \
	js_throw_err_msg(_ctx, GF_BAD_PARAM, "Invalid value in function %s (%s@%d)", __func__, strrchr(__FILE__, GF_PATH_SEPARATOR)+1, __LINE__)


#ifdef __cplusplus
}
#endif

#endif //_SG_QJS_COMMON_
