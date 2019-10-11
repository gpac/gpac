/*
 *			GPAC - Multimedia Framework C SDK
 *
 *				Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2019
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

#ifndef SG_QJS_COMMONE
#define SG_QJS_COMMONE

#include <gpac/list.h>
#include <gpac/scenegraph.h>

#include "../quickjs/quickjs.h"
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


void js_do_loop(JSContext *ctx);
void js_dump_error(JSContext *ctx);

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

/*dom event listener interface, exported for XHR module*/
#define JS_DOM3_EVENT_TARGET_INTERFACE	\
	JS_CFUNC_DEF("addEventListenerNS", 4, dom_event_add_listener),	\
	JS_CFUNC_DEF("removeEventListenerNS", 4, dom_event_remove_listener),	\
	JS_CFUNC_DEF("addEventListener", 3, dom_event_add_listener),		\
	JS_CFUNC_DEF("removeEventListener", 3, dom_event_remove_listener),	\
	JS_CFUNC_DEF("dispatchEvent", 1, xml_dom3_not_implemented),

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


/*definitions of C modules in gpac*/
void qjs_module_init_scenejs(JSContext *ctx);
void qjs_module_init_xhr(JSContext *c, JSValue global);
void qjs_module_init_storage(JSContext *ctx);


#ifdef __cplusplus
}
#endif

#endif //SG_QJS_COMMONE
