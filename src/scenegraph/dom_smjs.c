/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2012
 *			All rights reserved
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

#include <gpac/setup.h>
#include <gpac/internal/scenegraph_dev.h>

/*base SVG type*/
#include <gpac/nodes_svg.h>
/*dom events*/
#include <gpac/events.h>
/*dom text event*/
#include <gpac/utf.h>

#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/xml.h>

#ifndef GPAC_DISABLE_SVG

#ifdef GPAC_HAS_SPIDERMONKEY

#ifdef GPAC_ANDROID
#ifndef XP_UNIX
#define XP_UNIX
#endif /* XP_UNIX */
#endif

#include <gpac/html5_media.h>
#include <gpac/internal/smjs_api.h>


static GFINLINE Bool ScriptAction(GF_SceneGraph *scene, u32 type, GF_Node *node, GF_JSAPIParam *param)
{
	if (scene->script_action)
		return scene->script_action(scene->script_action_cbck, type, node, param);
	return GF_FALSE;
}

static GFINLINE GF_SceneGraph *xml_get_scenegraph(JSContext *c)
{
	GF_SceneGraph *scene;
	JSObject *global = JS_GetGlobalObject(c);
	assert(global);
	scene = (GF_SceneGraph *)SMJS_GET_PRIVATE(c, global);
	assert(scene);
	return scene;
}

#define JSVAL_CHECK_STRING(_v) (JSVAL_IS_STRING(_v) || JSVAL_IS_NULL(_v))

char *js_get_utf8(JSContext *c, jsval val)
{
#if 0
	jschar *utf16;
	char *txt;
	u32 len;
	if (!JSVAL_CHECK_STRING(val) || JSVAL_IS_NULL(val)) return NULL;

	/*locate SVG sequence responsible for this code add-on in test suit*/
	utf16 = JS_GetStringChars(JSVAL_TO_STRING(val) );
	len = gf_utf8_wcslen(utf16)*2 + 1;
	txt = gf_malloc(sizeof(char)*len);
	len = gf_utf8_wcstombs(txt, len, (const u16**) &utf16);
	if ((s32)len<0) {
		gf_free(txt);
		return NULL;
	}
	txt[len]=0;
	return txt;
#elif (JS_VERSION>=185)
	if (!JSVAL_CHECK_STRING(val) || JSVAL_IS_NULL(val)) return NULL;
	return SMJS_CHARS(c, val);
#else
	if (!JSVAL_CHECK_STRING(val) || JSVAL_IS_NULL(val)) return NULL;
	return gf_strdup( SMJS_CHARS(c, val) );
#endif
}

/************************************************************
 *
 *				DOM3 core implementation
 *
 *	This is NOT a full dom implementation :)
 *
 *************************************************************/
typedef struct
{
	u32 nb_inst;
	GF_JSClass domDocumentClass;
	GF_JSClass domNodeClass;
	GF_JSClass domElementClass;
	GF_JSClass domTextClass;
	GF_JSClass domNodeListClass;
	GF_JSClass domEventClass;

	GF_JSClass xmlHTTPRequestClass;
	GF_JSClass DCCIClass;

	GF_JSClass storageClass;

    GF_JSClass htmlMediaElementClass;

    void *(*get_element_class)(GF_Node *n);
	void *(*get_document_class)(GF_SceneGraph *n);
	GF_List *handlers;
} GF_DOMRuntime;

static GF_DOMRuntime *dom_rt = NULL;


static JSBool SMJS_FUNCTION(xml_dom3_not_implemented)
{
	return JS_TRUE;
}


#define JS_DOM3_uDOM_FIRST_PROP		18

void dom_node_changed(GF_Node *n, Bool child_modif, GF_FieldInfo *info)
{
	if (info) {
		if (child_modif) {
			gf_node_dirty_set(n, GF_SG_CHILD_DIRTY, GF_FALSE);
		}
#ifndef GPAC_DISABLE_SVG
		else {
			u32 flag = gf_svg_get_modification_flags((SVG_Element *)n, info);
			gf_node_dirty_set(n, flag, GF_FALSE);
		}
#endif
	}
	gf_node_changed(n, info);
}

static void define_dom_exception(JSContext *c, JSObject *global)
{
#define DEF_EXC(_val)	\
	JS_DefineProperty(c, obj, #_val, INT_TO_JSVAL(GF_DOM_EXC_##_val), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	JSObject *obj = JS_DefineObject(c, global, "DOMException", NULL, 0, 0);

	DEF_EXC(INDEX_SIZE_ERR);
	DEF_EXC(DOMSTRING_SIZE_ERR);
	DEF_EXC(HIERARCHY_REQUEST_ERR);
	DEF_EXC(WRONG_DOCUMENT_ERR);
	DEF_EXC(INVALID_CHARACTER_ERR);
	DEF_EXC(NO_DATA_ALLOWED_ERR);
	DEF_EXC(NO_MODIFICATION_ALLOWED_ERR);
	DEF_EXC(NOT_FOUND_ERR);
	DEF_EXC(NOT_SUPPORTED_ERR);
	DEF_EXC(INUSE_ATTRIBUTE_ERR);
	DEF_EXC(INVALID_STATE_ERR);
	DEF_EXC(SYNTAX_ERR);
	DEF_EXC(INVALID_MODIFICATION_ERR);
	DEF_EXC(NAMESPACE_ERR);
	DEF_EXC(INVALID_ACCESS_ERR);
	DEF_EXC(VALIDATION_ERR);
	DEF_EXC(TYPE_MISMATCH_ERR);
	DEF_EXC(SECURITY_ERR);
	DEF_EXC(NETWORK_ERR);
	DEF_EXC(ABORT_ERR);
	DEF_EXC(URL_MISMATCH_ERR);
	DEF_EXC(QUOTA_EXCEEDED_ERR);
	DEF_EXC(TIMEOUT_ERR);
	DEF_EXC(INVALID_NODE_TYPE_ERR);
	DEF_EXC(DATA_CLONE_ERR);
	DEF_EXC(SECURITY_ERR);
#undef  DEF_EXC

	//JS_AliasProperty(c, global, "DOMException", "e");
	obj = JS_DefineObject(c, global, "EventException", NULL, 0, 0);
	JS_DefineProperty(c, obj, "UNSPECIFIED_EVENT_TYPE_ERR", INT_TO_JSVAL(0), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, obj, "DISPATCH_REQUEST_ERR", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
}


JSBool dom_throw_exception(JSContext *c, u32 code)
{
	JSObject *obj = JS_NewObject(c, NULL, 0, 0);
	JS_DefineProperty(c, obj, "code", INT_TO_JSVAL(code), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_SetPendingException(c, OBJECT_TO_JSVAL(obj));
	return JS_FALSE;
}


GF_Node *dom_get_node(JSContext *c, JSObject *obj)
{
	GF_Node *n = (obj ? (GF_Node *)SMJS_GET_PRIVATE(c, obj) : NULL);
	if (n && n->sgprivate) return n;
	return NULL;
}

GF_Node *dom_get_element(JSContext *c, JSObject *obj)
{
	GF_Node *n = (GF_Node *)SMJS_GET_PRIVATE(c, obj);
	if (!n || !n->sgprivate) return NULL;
	if (n->sgprivate->tag==TAG_DOMText) return NULL;
	return n;
}

GF_SceneGraph *dom_get_doc(JSContext *c, JSObject *obj)
{
	GF_SceneGraph *sg = (GF_SceneGraph *)SMJS_GET_PRIVATE(c, obj);
	if (sg && !sg->__reserved_null) return sg;
	return NULL;
}

static jsval dom_document_construct(JSContext *c, GF_SceneGraph *sg)
{
	GF_JSClass *jsclass;
	JSObject *new_obj;
	if (sg->document) return OBJECT_TO_JSVAL(sg->document);

	if (sg->reference_count)
		sg->reference_count++;
	gf_node_register(sg->RootNode, NULL);

	jsclass = NULL;
	if (dom_rt->get_document_class)
		jsclass = (GF_JSClass *) dom_rt->get_document_class(sg);

	if (!jsclass) jsclass = &dom_rt->domDocumentClass;

	new_obj = JS_NewObject(c, & jsclass->_class, 0, 0);
	SMJS_SET_PRIVATE(c, new_obj, sg);
	sg->document = new_obj;
	return OBJECT_TO_JSVAL(new_obj);
}

static jsval dom_base_node_construct(JSContext *c, GF_JSClass *_class, GF_Node *n)
{
	Bool set_rooted;
	GF_SceneGraph *sg;
	JSObject *new_obj;
	if (!n || !n->sgprivate->scenegraph) return JSVAL_VOID;
	if (n->sgprivate->tag<TAG_DOMText) return JSVAL_VOID;

	sg = n->sgprivate->scenegraph;
	/*if parent doc is not a scene (eg it is a doc being parsed/constructed from script, don't root objects*/
	set_rooted = sg->reference_count ? GF_FALSE : GF_TRUE;

	if (n->sgprivate->interact && n->sgprivate->interact->js_binding && n->sgprivate->interact->js_binding->node) {
		if (set_rooted && (gf_list_find(sg->objects, n->sgprivate->interact->js_binding->node)<0)) {
			const char *name = gf_node_get_name(n);
			if (name) {
				gf_js_add_named_root(c, &n->sgprivate->interact->js_binding->node, GF_JSGC_OBJECT, name);
			} else {
				gf_js_add_root(c, &n->sgprivate->interact->js_binding->node, GF_JSGC_OBJECT);
			}
			gf_list_add(sg->objects, n->sgprivate->interact->js_binding->node);
		}
		return OBJECT_TO_JSVAL(n->sgprivate->interact->js_binding->node);
	}

	if (n->sgprivate->scenegraph->reference_count)
		n->sgprivate->scenegraph->reference_count ++;

	gf_node_register(n, NULL);
	new_obj = JS_NewObject(c, & _class->_class, 0, 0);
	SMJS_SET_PRIVATE(c, new_obj, n);

    if (n->sgprivate->tag == TAG_SVG_video || n->sgprivate->tag == TAG_SVG_audio)
    {
        html_media_element_js_init(c, new_obj, n);
    }
	if (!n->sgprivate->interact) GF_SAFEALLOC(n->sgprivate->interact, struct _node_interactive_ext);
	if (!n->sgprivate->interact->js_binding) {
		GF_SAFEALLOC(n->sgprivate->interact->js_binding, struct _node_js_binding);
	}
	n->sgprivate->interact->js_binding->node = new_obj;

	/*don't root the node until inserted in the tree*/
	if (n->sgprivate->parents && set_rooted) {
		const char *name = gf_node_get_name(n);
		if (name) {
			gf_js_add_named_root(c, &n->sgprivate->interact->js_binding->node, GF_JSGC_OBJECT, name);
		} else {
			gf_js_add_root(c, &n->sgprivate->interact->js_binding->node, GF_JSGC_OBJECT);
		}
		gf_list_add(sg->objects, n->sgprivate->interact->js_binding->node);
	}
	return OBJECT_TO_JSVAL(new_obj);
}
static jsval dom_node_construct(JSContext *c, GF_Node *n)
{
	GF_JSClass *__class = NULL;

	if (!n) return JSVAL_NULL;
	if (n->sgprivate->scenegraph->dcci_doc)
		__class = &dom_rt->DCCIClass;
	else if (dom_rt->get_element_class)
		__class = (GF_JSClass *) dom_rt->get_element_class(n);

	if (!__class ) __class  = &dom_rt->domElementClass;

	/*in our implementation ONLY ELEMENTS are created, never attributes. We therefore always
	create Elements when asked to create a node !!*/
	return dom_base_node_construct(c, __class, n);
}
jsval dom_element_construct(JSContext *c, GF_Node *n)
{
	GF_JSClass *__class = NULL;

	if (!n) return JSVAL_NULL;
	if (n->sgprivate->scenegraph->dcci_doc)
		__class = &dom_rt->DCCIClass;
	else if (dom_rt->get_element_class)
		__class = (GF_JSClass *) dom_rt->get_element_class(n);

	if (!__class) __class = &dom_rt->domElementClass;

	return dom_base_node_construct(c, __class, n);
}

static jsval dom_text_construct(JSContext *c, GF_Node *n)
{
	return dom_base_node_construct(c, &dom_rt->domTextClass, n);
}

static void dom_unregister_node(GF_Node *n)
{
	GF_SceneGraph *sg = n->sgprivate->scenegraph;
	if (!sg) return;
	/*!! node is being deleted !! */
	if (!n->sgprivate->num_instances) return;

	gf_node_unregister(n, NULL);
	if (sg->reference_count) {
		sg->reference_count--;
		if (!sg->reference_count)
			gf_sg_del(sg);
	}
}

jsval dom_node_get_sibling(JSContext *c, GF_Node *n, Bool is_prev, Bool elt_only)
{
	GF_Node *val;
	GF_ChildNodeItem *child;
	s32 idx, cur;
	GF_ParentNode *par;
	if (!n) return JSVAL_NULL;
	par = (GF_ParentNode *)gf_node_get_parent(n, 0);
	if (!par) return JSVAL_NULL;

	idx = gf_node_list_find_child(par->children, n);
	if (idx<0) return JSVAL_NULL;

	if (!elt_only) {
		if (is_prev) {
			idx--;
			if (idx<0) return JSVAL_NULL;
		}
		else idx++;
		return dom_node_construct(c, gf_node_list_get_child(par->children, idx) );
	}
	cur = 0;
	val = NULL;
	child = par->children;
	while (child) {
		if ((idx!=cur) && (child->node->sgprivate->tag!=TAG_DOMText)) {
			val = child->node;
		}
		if (is_prev) {
			if (idx<=cur)
				break;
		} else {
			if (idx>=cur) val = NULL;
			if (val && idx<cur)
				break;
		}
		child = child->next;
		cur++;
	}
	return dom_node_construct(c, val);
}



/*dom3 NodeList/NamedNodeMap*/
typedef struct
{
	/*set if the object is a childList from an existing node*/
	GF_ParentNode *owner;
	/*child list*/
	GF_ChildNodeItem *child;
} DOMNodeList;


static jsval dom_nodelist_construct(JSContext *c, GF_ParentNode *n)
{
	DOMNodeList *nl;
	JSObject *new_obj;
	if (!n) return JSVAL_VOID;
	GF_SAFEALLOC(nl, DOMNodeList);
	nl->owner = n;
	if (n->sgprivate->scenegraph->reference_count)
		n->sgprivate->scenegraph->reference_count++;

	gf_node_register((GF_Node*)n, NULL);
	new_obj = JS_NewObject(c, &dom_rt->domNodeListClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, new_obj, nl);
	return OBJECT_TO_JSVAL(new_obj);
}

static DECL_FINALIZE(dom_nodelist_finalize)

	DOMNodeList *nl;
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->domNodeListClass, NULL) )
		return;

	nl = (DOMNodeList *) SMJS_GET_PRIVATE(c, obj);
	if (!nl) return;

	if (nl->owner) {
		dom_unregister_node((GF_Node*)nl->owner);
	} else {
		/*unregister all nodes for created lists*/
		while (nl->child) {
			GF_ChildNodeItem *child = nl->child;
			nl->child = child->next;
			dom_unregister_node(child->node);
			gf_free(child);
		}
	}
	gf_free(nl);
}

static JSBool SMJS_FUNCTION(dom_nodelist_item)
{
	GF_Node *n;
	u32 count;
	s32 idx;
	DOMNodeList *nl;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->domNodeListClass, NULL) )
		return JS_TRUE;
	if ((argc!=1) || !JSVAL_IS_INT(argv[0]))
		return JS_TRUE;

	nl = (DOMNodeList *)SMJS_GET_PRIVATE(c, obj);
	count = gf_node_list_get_count(nl->owner ? nl->owner->children : nl->child);
	idx = JSVAL_TO_INT(argv[0]);
	if ((idx<0) || ((u32) idx>=count)) {
		SMJS_SET_RVAL( JSVAL_VOID );
		return JS_TRUE;
	}
	n = gf_node_list_get_child(nl->owner ? nl->owner->children : nl->child, idx);
	SMJS_SET_RVAL( dom_node_construct(c, n) );
	return JS_TRUE;
}

static SMJS_FUNC_PROP_GET( dom_nodelist_getProperty)

	DOMNodeList *nl;
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->domNodeListClass, NULL)) {
		return JS_TRUE;
	}

	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;

	switch (SMJS_ID_TO_INT(id)) {
	case 0:
		nl = (DOMNodeList *) SMJS_GET_PRIVATE(c, obj);
		*vp = INT_TO_JSVAL( gf_node_list_get_count(nl->owner ? nl->owner->children : nl->child) );
		return JS_TRUE;
	}
	return JS_TRUE;
}
static SMJS_FUNC_PROP_SET_NOVP( dom_nodelist_setProperty)

	/*avoids gcc warning*/
	if (!obj) obj=NULL;
	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
	/*no write prop*/
	return JS_TRUE;
}


/*dom event listener*/

#define JS_DOM3_EVENT_TARGET_INTERFACE	\
	SMJS_FUNCTION_SPEC("addEventListenerNS", dom_event_add_listener, 4),	\
	SMJS_FUNCTION_SPEC("removeEventListenerNS", dom_event_remove_listener, 4),	\
	SMJS_FUNCTION_SPEC("addEventListener", dom_event_add_listener, 3),		\
	SMJS_FUNCTION_SPEC("removeEventListener", dom_event_remove_listener, 3),	\
	SMJS_FUNCTION_SPEC("dispatchEvent", xml_dom3_not_implemented, 1),


static void dom_handler_remove(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		SVG_handlerElement *handler = (SVG_handlerElement *)node;
		if (handler->js_context && handler->js_fun_val) {
			/*unprotect the function*/
			gf_js_remove_root((JSContext *)handler->js_context, &(handler->js_fun_val), GF_JSGC_VAL);
			handler->js_fun_val=0;
			gf_list_del_item(dom_rt->handlers, handler);
		}
	}
}

/*eventListeners routines used by document, element and connection interfaces*/
JSBool SMJS_FUNCTION_EXT(gf_sg_js_event_add_listener, GF_Node *on_node)
{
	GF_DOMEventTarget *target;
	GF_FieldInfo info;
	GF_Node *listener;
	SVG_handlerElement *handler;
	char *type, *callback;
	u32 of = 0;
	u32 evtType;
	char *inNS = NULL;
	GF_SceneGraph *sg = NULL;
	JSFunction*fun = NULL;
	GF_Node *n = NULL;
	jsval funval = JSVAL_NULL;
	JSObject *evt_handler;
	SMJS_OBJ
	SMJS_ARGS

	target = NULL;
	/*document interface*/
	sg = dom_get_doc(c, obj);
	if (sg) {
#ifndef GPAC_DISABLE_SVG
		target = &sg->dom_evt;
#else
		return JS_TRUE;
#endif
	} else {
		if (on_node) {
			n = on_node;
		} else {
			n = dom_get_element(c, obj);
		}
		if (n) sg = n->sgprivate->scenegraph;
	}

	/*FIXME - SVG uDOM connection not supported yet*/

	if (!sg) return JS_TRUE;

	/*NS version (4 args in DOM2, 5 in DOM3 for evt_group param)*/
	if (argc>=4) {
		if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
		inNS = js_get_utf8(c, argv[0]);
		of = 1;
	}
	evt_handler = obj;

	type = callback = NULL;
	if (!JSVAL_CHECK_STRING(argv[of])) goto err_exit;
	type = SMJS_CHARS(c, argv[of]);

	if (JSVAL_CHECK_STRING(argv[of+1])) {
		callback = SMJS_CHARS(c, argv[of+1]);
		if (!callback) goto err_exit;
	} else if (JSVAL_IS_OBJECT(argv[of+1])) {
		if (JS_ObjectIsFunction(c, JSVAL_TO_OBJECT(argv[of+1]))) {
			//fun = JS_ValueToFunction(c, argv[of+1]);
			funval = argv[of+1];
		} else {
			JSBool found;
			jsval evt_fun;
			evt_handler = JSVAL_TO_OBJECT(argv[of+1]);
			found = JS_GetProperty(c, evt_handler, "handleEvent", &evt_fun);
			if (!found || !JSVAL_IS_OBJECT(evt_fun) ) goto err_exit;
			if (!JS_ObjectIsFunction(c, JSVAL_TO_OBJECT(evt_fun)) ) goto err_exit;
			funval = evt_fun;
		}
	}

	evtType = gf_dom_event_type_by_name(type);
	if (evtType==GF_EVENT_UNKNOWN) goto err_exit;

	listener = gf_node_new(sg, TAG_SVG_listener);
	/*we don't register the listener with the parent node , it will be registered
	on gf_dom_listener__add*/

	/*!!! create the handler in the scene owning the script context !!! */
	{
		GF_SceneGraph *sg = xml_get_scenegraph(c);
		handler = (SVG_handlerElement *) gf_node_new(sg, TAG_SVG_handler);
		/*we register the handler with the listener node to avoid modifying the DOM*/
		gf_node_register((GF_Node *)handler, listener);
		gf_node_list_add_child(& ((GF_ParentNode *)listener)->children, (GF_Node*)handler);

		if (!callback) {
			handler->js_fun = fun;
			handler->js_fun_val = *(u64 *) &funval;
			if (handler->js_fun_val) {
				handler->js_context = c;
				/*protect the function - we don't know how it was passed to us, so prevent it from being GCed*/
				gf_js_add_root((JSContext *)handler->js_context, &handler->js_fun_val, GF_JSGC_VAL);
				handler->sgprivate->UserCallback = dom_handler_remove;
				gf_list_add(dom_rt->handlers, handler);
			}
			handler->evt_listen_obj = evt_handler;
		}
	}

	/*create attributes if needed*/
	gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_event, GF_TRUE, GF_FALSE, &info);
	((XMLEV_Event*)info.far_ptr)->type = evtType;
	gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_handler, GF_TRUE, GF_FALSE, &info);
	((XMLRI*)info.far_ptr)->target = (GF_Node*)handler;
	if (n) {
		gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_target, GF_TRUE, GF_FALSE, &info);
		((XMLRI*)info.far_ptr)->target = n;
	}

	gf_node_get_attribute_by_tag((GF_Node*)handler, TAG_XMLEV_ATT_event, GF_TRUE, GF_FALSE, &info);
	((XMLEV_Event*)info.far_ptr)->type = evtType;

	if (callback) gf_dom_add_text_node((GF_Node *)handler, gf_strdup(callback));

#ifndef GPAC_DISABLE_SVG
	if (handler->sgprivate->scenegraph->svg_js)
		handler->handle_event = gf_sg_handle_dom_event;
#endif

	if (on_node) {
		handler->js_context = c;
#ifndef GPAC_DISABLE_VRML
		if (on_node->sgprivate->tag <= GF_NODE_RANGE_LAST_VRML)
			handler->handle_event = gf_sg_handle_dom_event_for_vrml;
#endif
	}

	/*don't add listener directly, post it and wait for event processing*/
	if (n) {
		gf_sg_listener_post_add((GF_Node *) n, listener);
	} else {
		gf_sg_listener_add(listener, target);
	}

err_exit:
	if (inNS) gf_free(inNS);
	SMJS_FREE(c, type);
	SMJS_FREE(c, callback);
   return JS_TRUE;
}


JSBool SMJS_FUNCTION(dom_event_add_listener)
{
	return gf_sg_js_event_add_listener(SMJS_CALL_ARGS, NULL);
}

JSBool SMJS_FUNCTION_EXT(gf_sg_js_event_remove_listener, GF_Node *vrml_node)
{
#ifndef GPAC_DISABLE_SVG
	char *type, *callback;
	u32 of = 0;
	u32 evtType, i, count;
	char *inNS = NULL;
	GF_Node *node = NULL;
	jsval funval = JSVAL_NULL;
	GF_SceneGraph *sg = NULL;
	GF_DOMEventTarget *target = NULL;
	SMJS_OBJ
	SMJS_ARGS

	/*get the scenegraph first*/
	sg = dom_get_doc(c, obj);
	if (!sg) {
		if (vrml_node) {
			node = vrml_node;
		} else {
			node = dom_get_element(c, obj);
		}
		if (node) sg = node->sgprivate->scenegraph;
	}

	if (!sg) return JS_TRUE;
	/*flush all pending add_listener*/
	gf_dom_listener_process_add(sg);
	if (node) {
		if (node->sgprivate->interact) target = node->sgprivate->interact->dom_evt;
	}
	/*FIXME - SVG uDOM connection not supported yet*/
	else {
		target = &sg->dom_evt;
	}
	if (!target) return JS_TRUE;


	if (argc==4) {
		if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
		inNS = js_get_utf8(c, argv[0]);
		of = 1;
	}

	type = callback = NULL;
	if (!JSVAL_CHECK_STRING(argv[of])) goto err_exit;
	type = SMJS_CHARS(c, argv[of]);

	if (JSVAL_CHECK_STRING(argv[of+1])) {
		callback = SMJS_CHARS(c, argv[of+1]);
	} else if (JSVAL_IS_OBJECT(argv[of+1])) {
		if (JS_ObjectIsFunction(c, JSVAL_TO_OBJECT(argv[of+1]) )) {
			funval = argv[of+1];
		}
	}
	if (!callback && JSVAL_IS_NULL(funval) ) goto err_exit;

	evtType = gf_dom_event_type_by_name(type);
	if (evtType==GF_EVENT_UNKNOWN) goto err_exit;

	count = gf_list_count(target->evt_list);
	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		GF_DOMText *txt;
		SVG_handlerElement *hdl;
		GF_Node *el = (GF_Node *)gf_list_get(target->evt_list, i);

		gf_node_get_attribute_by_tag(el, TAG_XMLEV_ATT_event, GF_FALSE, GF_FALSE, &info);
		if (!info.far_ptr) continue;
		if (((XMLEV_Event*)info.far_ptr)->type != evtType) continue;

		gf_node_get_attribute_by_tag(el, TAG_XMLEV_ATT_handler, GF_FALSE, GF_FALSE, &info);
		if (!info.far_ptr) continue;
		hdl = (SVG_handlerElement *) ((XMLRI*)info.far_ptr)->target;
		if (!hdl) continue;
		if (! JSVAL_IS_NULL(funval) ) {
			if (*(u64 *) &funval != hdl->js_fun_val) continue;
		} else if (hdl->children) {
			txt = (GF_DOMText *) hdl->children->node;
			if (txt->sgprivate->tag != TAG_DOMText) continue;
			if (!txt->textContent || strcmp(txt->textContent, callback)) continue;
		} else {
			continue;
		}

		/*this will destroy the listener and its child handler*/
		gf_dom_listener_del(el, target);
		break;
	}
#endif

err_exit:
	if (inNS) gf_free(inNS);
	SMJS_FREE(c, type);
	SMJS_FREE(c, callback);
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(dom_event_remove_listener)
{
	return gf_sg_js_event_remove_listener(SMJS_CALL_ARGS, NULL);
}

/*dom3 node*/
static DECL_FINALIZE( dom_node_finalize)

	GF_Node *n = (GF_Node *) SMJS_GET_PRIVATE(c, obj);
	/*the JS proto of the svgClass or a destroyed object*/
	if (!n) return;
	if (!n->sgprivate) return;

	SMJS_SET_PRIVATE(c, obj, NULL);
	gf_list_del_item(n->sgprivate->scenegraph->objects, obj);

	dom_js_pre_destroy(c, n->sgprivate->scenegraph, n);
	dom_unregister_node(n);
}

static Bool check_dom_parents(JSContext *c, GF_Node *n, GF_Node *parent)
{
	GF_ParentList *par = n->sgprivate->parents;
	if (n->sgprivate->scenegraph != parent->sgprivate->scenegraph) {
		dom_throw_exception(c, GF_DOM_EXC_WRONG_DOCUMENT_ERR);
		return GF_FALSE;
	}
	while (par) {
		if (par->node==parent) {
			dom_throw_exception(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
			return GF_FALSE;
		}
		if (!check_dom_parents(c, par->node, parent))
			return GF_FALSE;
		par = par->next;
	}
	return GF_TRUE;
}

static void dom_node_inserted(JSContext *c, GF_Node *n, GF_Node *parent, s32 pos)
{
	GF_ParentNode *old_parent;
	Bool do_init = GF_FALSE;

	/*if node is already in graph, remove it from its parent*/
	old_parent = (GF_ParentNode*)gf_node_get_parent(n, 0);
	if (old_parent) {
		/*prevent destruction when removing node*/
		n->sgprivate->num_instances++;
		gf_node_list_del_child(&old_parent->children, n);
		gf_node_unregister(n, (GF_Node*)old_parent);
		n->sgprivate->num_instances--;
	} else {
		do_init = (n->sgprivate->UserCallback || n->sgprivate->UserPrivate) ? GF_FALSE : GF_TRUE;

	}
	/*node is being inserted - if node has a valid binding, re-root it if needed*/
	if (n->sgprivate->interact && n->sgprivate->interact->js_binding) {
		JSObject *nobj = (JSObject *)n->sgprivate->interact->js_binding->node;
		if (nobj && (gf_list_find(n->sgprivate->scenegraph->objects, nobj)<0) ) {
			const char *name = gf_node_get_name(n);
			if (name) {
				gf_js_add_named_root(c, &n->sgprivate->interact->js_binding->node, GF_JSGC_OBJECT, name);
			} else {
				gf_js_add_root(c, &n->sgprivate->interact->js_binding->node, GF_JSGC_OBJECT);
			}
			gf_list_add(n->sgprivate->scenegraph->objects, nobj);
		}
	}

	if (pos<0) gf_node_list_add_child( & ((GF_ParentNode *)parent)->children, n);
	else gf_node_list_insert_child( & ((GF_ParentNode *)parent)->children, n, (u32) pos);
	gf_node_register(n, parent);

	if (do_init) {
		/*node is a handler, create listener*/
		if (parent && (n->sgprivate->tag==TAG_SVG_handler)) {
			gf_dom_listener_build_ex(parent, 0, 0, n, NULL);
		}
		gf_node_init(n);
		

#ifndef GPAC_DISABLE_SVG
		if (n->sgprivate->interact && n->sgprivate->interact->dom_evt) {
			GF_DOM_Event evt;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.type = GF_EVENT_LOAD;
			gf_dom_event_fire(n, &evt);
		}
#endif
	}
	/*node is being re-inserted, activate it in case*/
	if (!old_parent) gf_node_activate(n);

	dom_node_changed(parent, GF_TRUE, NULL);
}

static JSBool SMJS_FUNCTION(xml_node_insert_before)
{
	s32 idx;
	u32 tag;
	GF_Node *n, *target, *new_node;
	GF_ParentNode *par;
	SMJS_OBJ
	SMJS_ARGS
	if (!argc || !JSVAL_IS_OBJECT(argv[0]))
		return JS_TRUE;

	n = dom_get_node(c, obj);
	if (!n) {
		return dom_throw_exception(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	}

	new_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]));
	if (!new_node) return JS_TRUE;

	target = NULL;
	if ((argc==2) && JSVAL_IS_OBJECT(argv[1]) && !JSVAL_IS_NULL(argv[1])) {
		target = dom_get_node(c, JSVAL_TO_OBJECT(argv[1]));
		if (!target) return JS_TRUE;
	}
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return JS_TRUE;

	if (!check_dom_parents(c, n, new_node))
		return JS_FALSE;

	par = (GF_ParentNode*)n;
	idx = -1;
	if (target) {
		idx = gf_node_list_find_child(par->children, target);
		if (idx<0) {
			return dom_throw_exception(c, GF_DOM_EXC_NOT_FOUND_ERR);
		}
	}
	dom_node_inserted(c, new_node, n, idx);
	SMJS_SET_RVAL( argv[0] );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_node_append_child)
{
	u32 tag;
	GF_Node *n, *new_node;
	SMJS_OBJ
	SMJS_ARGS
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;
	n = dom_get_node(c, obj);
	if (!n) {
		return dom_throw_exception(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	}

	new_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]));
	if (!new_node) return JS_TRUE;
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return JS_TRUE;

	if (!check_dom_parents(c, n, new_node))
		return JS_FALSE;

	dom_node_inserted(c, new_node, n, -1);

	SMJS_SET_RVAL( argv[0] );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_node_replace_child)
{
	s32 idx;
	u32 tag;
	GF_Node *n, *new_node, *old_node;
	GF_ParentNode *par;
	SMJS_OBJ
	SMJS_ARGS
	if ((argc!=2) || !JSVAL_IS_OBJECT(argv[0]) || !JSVAL_IS_OBJECT(argv[1])) return JS_TRUE;
	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	new_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]));
	if (!new_node) return JS_TRUE;
	old_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[1]));
	if (!old_node) return JS_TRUE;
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return JS_TRUE;
	par = (GF_ParentNode*)n;
	idx = gf_node_list_find_child(par->children, old_node);
	if (idx<0) return JS_TRUE;

	gf_node_list_del_child(&par->children, old_node);
	gf_node_unregister(old_node, n);

	dom_node_inserted(c, new_node, n, -1);

	/*whenever we remove a node from the tree, call GC to cleanup the JS binding if any. Not doing so may screw up node IDs*/
	n->sgprivate->scenegraph->svg_js->force_gc = GF_TRUE;

	SMJS_SET_RVAL( argv[0] );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_node_remove_child)
{
	u32 tag;
	GF_Node *n, *old_node;
	GF_ParentNode *par;
	SMJS_OBJ
	SMJS_ARGS
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;

	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	old_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]));
	if (!old_node) return JS_TRUE;
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return JS_TRUE;
	par = (GF_ParentNode*)n;
	
	/*if node is present in parent, unregister*/
	if (gf_node_list_del_child(&par->children, old_node)) {
		gf_node_unregister(old_node, n);
	} else {
		return dom_throw_exception(c, GF_DOM_EXC_NOT_FOUND_ERR);
	}
	SMJS_SET_RVAL( argv[0]);
	dom_node_changed(n, GF_TRUE, NULL);
	/*whenever we remove a node from the tree, call GC to cleanup the JS binding if any. Not doing so may screw up node IDs*/
	n->sgprivate->scenegraph->svg_js->force_gc = GF_TRUE;
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(xml_clone_node)
{
	Bool deep;
	GF_Node *n, *clone;
	SMJS_OBJ
	SMJS_ARGS
	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	deep = argc ? (JSVAL_TO_BOOLEAN(argv[0]) ? GF_TRUE : GF_FALSE) : GF_FALSE;

	clone = gf_node_clone(n->sgprivate->scenegraph, n, NULL, "", deep);
	SMJS_SET_RVAL( dom_node_construct(c, clone) );
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(xml_node_has_children)
{
	GF_Node *n;
	SMJS_OBJ
	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL( ((GF_ParentNode*)n)->children ? JS_TRUE : JS_FALSE) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_node_has_attributes)
{
	u32 tag;
	GF_Node *n;
	SMJS_OBJ
	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	tag = gf_node_get_tag(n);
	if (tag>=GF_NODE_FIRST_DOM_NODE_TAG) {
		SMJS_SET_RVAL( BOOLEAN_TO_JSVAL( ((GF_DOMNode*)n)->attributes ? JS_TRUE : JS_FALSE) );
	} else {
		/*not supported for other node types*/
		SMJS_SET_RVAL( BOOLEAN_TO_JSVAL( JS_FALSE) );
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_node_is_same_node)
{
	GF_Node *n, *a_node;
	SMJS_OBJ
	SMJS_ARGS
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;
	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	a_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]));
	if (!a_node) return JS_TRUE;
	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL( (a_node==n) ? JS_TRUE : JS_FALSE) );
	return JS_TRUE;
}

static const char *node_get_local_name(GF_Node *node)
{
	const char *res;
	GF_List *l;
	if (!node) return NULL;
	l = node->sgprivate->scenegraph->ns;
	node->sgprivate->scenegraph->ns = NULL;
	res = gf_node_get_class_name(node);
	node->sgprivate->scenegraph->ns = l;
	return res;
}

static const char *node_lookup_namespace_by_tag(GF_Node *node, u32 tag)
{
	/*browse attributes*/
	GF_DOMAttribute *att;
	if (!node) return NULL;
	att = ((SVG_Element*)node)->attributes;
	while (att) {
		if (att->tag==TAG_DOM_ATT_any) {
			GF_DOMFullAttribute *datt = (GF_DOMFullAttribute*)att;
			if (datt->name && !strncmp(datt->name, "xmlns", 5)) {
				char *xmlns = *(DOM_String *) datt->data;
				u32 crc = gf_crc_32(xmlns, (u32) strlen(xmlns));
				if (tag==crc) return xmlns;
			}
		}
		att = att->next;
	}
	/*browse for parent*/
	return node_lookup_namespace_by_tag(gf_node_get_parent(node, 0), tag);
}

#ifdef GPAC_UNUSED_FUNC
/**
 * FIXME : function is not used by anybody
 */
static u32 get_namespace_code_by_prefix(GF_Node *node, char *prefix)
{
	/*browse attributes*/
	GF_DOMAttribute *att;
	if (!node) return 0;
	att = ((SVG_Element*)node)->attributes;
	while (att) {
		if (att->tag==TAG_DOM_ATT_any) {
			GF_DOMFullAttribute *datt = (GF_DOMFullAttribute*)att;
			if (!prefix) {
				if (!strcmp(datt->name, "xmlns")) return datt->xmlns;
			} else if (!strncmp(datt->name, "xmlns:", 6)) {
				if (!strcmp(datt->name+6, prefix)) return datt->xmlns;
			}
		}
		att = att->next;
	}
	/*browse for parent*/
	return get_namespace_code_by_prefix(gf_node_get_parent(node, 0), prefix);
}
#endif /*GPAC_UNUSED_FUNC*/

static SMJS_FUNC_PROP_GET( dom_node_getProperty)

	u32 tag;
	GF_Node *n;
	GF_SceneGraph *sg = NULL;
	GF_ParentNode *par;

	n = dom_get_node(c, obj);
	if (!n) {
		sg = dom_get_doc(c, obj);
		if (!sg) return JS_TRUE;
	}

	/*not supported*/
	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;

	tag = n ? gf_node_get_tag(n) : 0;
	par = (GF_ParentNode*)n;

	switch (SMJS_ID_TO_INT(id)) {
	/*"nodeName"*/
	case 0:
		if (sg) {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "#document") );
		}
		else if (tag==TAG_DOMText) {
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->type==GF_DOM_TEXT_CDATA) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "#cdata-section") );
			else *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "#text") );
		}
		else {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, gf_node_get_class_name(n) ) );
		}
		return JS_TRUE;
	/*"nodeValue"*/
	case 1:
		*vp = JSVAL_VOID;
		if (tag==TAG_DOMText) {
			GF_DOMText *txt = (GF_DOMText *)n;
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, txt->textContent) );
		}
		return JS_TRUE;
	/*"nodeType"*/
	case 2:
		if (sg) *vp = INT_TO_JSVAL(9);
		else if (tag==TAG_DOMText) {
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->type==GF_DOM_TEXT_CDATA) *vp = INT_TO_JSVAL(4);
			else *vp = INT_TO_JSVAL(3);
		}
		else *vp = INT_TO_JSVAL(1);
		return JS_TRUE;
	/*"parentNode"*/
	case 3:
		if (sg) {
			*vp = JSVAL_NULL;
		}
		/*if root node of the tree, the parentNode is the document*/
		else if (n->sgprivate->scenegraph->RootNode==n) {
			*vp = dom_document_construct(c, n->sgprivate->scenegraph);
		} else {
			*vp = dom_node_construct(c, gf_node_get_parent(n, 0) );
		}
		return JS_TRUE;
	/*"childNodes"*/
	case 4:
		/*NOT SUPPORTED YET*/
		if (sg) *vp = JSVAL_VOID;
		else if (tag==TAG_DOMText) *vp = JSVAL_NULL;
		else *vp = dom_nodelist_construct(c, par);
		return JS_TRUE;
	/*"firstChild"*/
	case 5:
		if (sg) *vp = dom_node_construct(c, sg->RootNode);
		else if (tag==TAG_DOMText) *vp = JSVAL_NULL;
		else if (!par->children) {
			*vp = JSVAL_NULL;
		}
		else *vp = dom_node_construct(c, par->children->node);
		return JS_TRUE;
	/*"lastChild"*/
	case 6:
		if (sg) *vp = dom_node_construct(c, sg->RootNode);
		else if ((tag==TAG_DOMText) || !par->children) *vp = JSVAL_VOID;
		else *vp = dom_node_construct(c, gf_node_list_get_child(par->children, -1) );
		return JS_TRUE;
	/*"previousSibling"*/
	case 7:
		/*works for doc as well since n is NULL*/
		*vp = dom_node_get_sibling(c, n, GF_TRUE, GF_FALSE);
		return JS_TRUE;
	/*"nextSibling"*/
	case 8:
		*vp = dom_node_get_sibling(c, n, GF_FALSE, GF_FALSE);
		return JS_TRUE;
	/*"attributes"*/
	case 9:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"ownerDocument"*/
	case 10:
		if (sg) *vp = JSVAL_NULL;
		else *vp = dom_document_construct(c, n->sgprivate->scenegraph);
		return JS_TRUE;
	/*"namespaceURI"*/
	case 11:
		*vp = JSVAL_NULL;
		if (!sg) {
			tag = gf_xml_get_element_namespace(n);
			if (tag) {
				const char *xmlns = gf_sg_get_namespace(n->sgprivate->scenegraph, tag);
				if (!xmlns) xmlns = node_lookup_namespace_by_tag(n, tag);
				*vp = xmlns ? STRING_TO_JSVAL( JS_NewStringCopyZ(c, xmlns) ) : JSVAL_VOID;
			}
		}
		return JS_TRUE;
	/*"prefix"*/
	case 12:
		if (sg) tag = gf_sg_get_namespace_code(sg, NULL);
		else tag = gf_xml_get_element_namespace(n);
		*vp = JSVAL_NULL;
		if (tag) {
			char *xmlns = (char *)gf_sg_get_namespace_qname(sg ? sg : n->sgprivate->scenegraph, tag);
			if (xmlns) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, xmlns) );
		}
		return JS_TRUE;
	/*"localName"*/
	case 13:
		*vp = JSVAL_NULL;
		if (!sg && (tag!=TAG_DOMText)) {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, node_get_local_name(n) ) );
		}
		return JS_TRUE;
	/*"baseURI"*/
	case 14:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_NULL;
		return JS_TRUE;
	/*"textContent"*/
	case 15:
		*vp = JSVAL_VOID;
		if (!sg)  {
			char *res = gf_dom_flatten_textContent(n);
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, res) );
			gf_free(res);
		}
		return JS_TRUE;

	case 16:/*firstElementChild*/
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
	case 17:/*lastElementChild*/
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
	case 18:/*previousElementSibling*/
		*vp = dom_node_get_sibling(c, n, GF_TRUE, GF_TRUE);
		return JS_TRUE;
	case 19:/*nextElementSibling*/
		*vp = dom_node_get_sibling(c, n, GF_FALSE, GF_TRUE);
		return JS_TRUE;

	}
	/*not supported*/
	return JS_TRUE;
}

void dom_node_set_textContent(GF_Node *n, char *text)
{
	GF_FieldInfo info;
	gf_dom_set_textContent(n, text);

	gf_node_dirty_set(n, GF_SG_CHILD_DIRTY, GF_FALSE);
	memset(&info, 0, sizeof(GF_FieldInfo));
	info.fieldIndex = (u32) -1;
	gf_node_changed(n, &info);
}

static SMJS_FUNC_PROP_SET( dom_node_setProperty)

	u32 tag;
	GF_Node *n;

	n = dom_get_node(c, obj);
	/*note an element - we don't support property setting on document yet*/
	if (!n) return JS_TRUE;

	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;

	tag = n ? gf_node_get_tag(n) : 0;

	switch (SMJS_ID_TO_INT(id)) {
	/*"nodeValue"*/
	case 1:
		if ((tag==TAG_DOMText) && JSVAL_CHECK_STRING(*vp)) {
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->textContent) gf_free(txt->textContent);
			txt->textContent = js_get_utf8(c, *vp);
			dom_node_changed(n, GF_TRUE, NULL);
		}
		/*we only support element and sg in the Node interface, no set*/
		return JS_TRUE;
	/*"prefix"*/
	case 12:
		/*NOT SUPPORTED YET*/
		return JS_TRUE;
	/*"textContent"*/
	case 15:
	{
		char *txt;
		txt = js_get_utf8(c, *vp);
		dom_node_set_textContent(n, txt);
		if (txt) gf_free(txt);
	}
		return JS_TRUE;
	}
	/*not supported*/
	return JS_TRUE;
}


/*dom3 document*/

/*don't attempt to do anything with the scenegraph, it may be destroyed
fortunately a sg cannot be created like that...*/
DECL_FINALIZE(dom_document_finalize)

	GF_SceneGraph *sg = dom_get_doc(c, obj);

	sg = (GF_SceneGraph*) SMJS_GET_PRIVATE(c, obj);
	/*the JS proto of the svgClass or a destroyed object*/
	if (!sg) return;

	SMJS_SET_PRIVATE(c, sg->document, NULL);
	sg->document = NULL;
	if (sg->RootNode) {
		gf_node_unregister(sg->RootNode, NULL);
		if (sg->reference_count) {
			sg->reference_count--;
			if (!sg->reference_count)
				gf_sg_del(sg);
		}
	}
}

static SMJS_FUNC_PROP_GET( dom_document_getProperty )

	u32 prop_id;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_TRUE;

	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
	prop_id = SMJS_ID_TO_INT(id);

	switch (prop_id) {
	case 2:/*implementation*/
		/*FIXME, this is wrong, we should have our own implementation
		but at the current time we rely on the global object to provide it*/
		*vp = OBJECT_TO_JSVAL( JS_GetGlobalObject(c) );
		return JS_TRUE;
	case 3: /*"documentElement"*/
		*vp = dom_element_construct(c, sg->RootNode);
		return JS_TRUE;

	case 11:/*global*/
		*vp = OBJECT_TO_JSVAL( JS_GetGlobalObject(c) );
		return JS_TRUE;


	/*NOT SUPPORTED YET*/

	case 1: /*"doctype"*/
	case 4: /*"inputEncoding"*/
	case 5: /*"xmlEncoding"*/
	case 6: /*"xmlStandalone"*/
	case 7: /*"xmlVersion"*/
	case 8: /*"strictErrorChecking"*/
	case 9: /*"documentURI"*/
	case 10: /*"domConfig"*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	}
	return JS_TRUE;
}

static SMJS_FUNC_PROP_SET_NOVP(dom_document_setProperty)

	u32 prop_id;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_TRUE;

	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
	prop_id = SMJS_ID_TO_INT(id);

	switch (prop_id) {
	case 6:/*"xmlStandalone"*/
		break;
	case 7:/*"xmlVersion"*/
		break;
	case 8:/*"strictErrorChecking"*/
		break;
	case 9:/*"documentURI"*/
		break;
	case 10:/*"domConfig"*/
		break;

	/*the rest is read-only*/
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_document_create_element)
{
	u32 tag, ns;
	GF_Node *n;
	char *name, *xmlns;
	SMJS_OBJ
	SMJS_ARGS
	GF_SceneGraph *sg = dom_get_doc(c, obj);

	if (!sg || !argc || !JSVAL_CHECK_STRING(argv[0]) )
		return JS_TRUE;

	name = NULL;
	/*NS version*/
	ns = 0;
	xmlns = NULL;
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		xmlns = js_get_utf8(c, argv[0]);
		if (xmlns) ns = gf_sg_get_namespace_code_from_name(sg, xmlns);
		name = SMJS_CHARS(c, argv[1]);
	} else {
		name = SMJS_CHARS(c, argv[0]);
	}

	if (name) {
		/*browse all our supported DOM implementations*/
		tag = gf_xml_get_element_tag(name, ns);
		if (!tag) tag = TAG_DOMFullNode;
		n = gf_node_new(sg, tag);
		if (n && (tag == TAG_DOMFullNode)) {
			GF_DOMFullNode *elt = (GF_DOMFullNode *)n;
			elt->name = gf_strdup(name);
			if (xmlns) {
				gf_sg_add_namespace(sg, xmlns, NULL);
				elt->ns	= gf_sg_get_namespace_code_from_name(sg, xmlns);
			}
		}
		SMJS_SET_RVAL( dom_element_construct(c, n) );
	}
	if (xmlns) gf_free(xmlns);
	SMJS_FREE(c, name);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_document_create_text)
{
	GF_Node *n;
	SMJS_OBJ
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_TRUE;

	n = gf_node_new(sg, TAG_DOMText);
	if (argc) {
		SMJS_ARGS
		char *str = js_get_utf8(c, argv[0]);
		if (!str) str = gf_strdup("");
		((GF_DOMText*)n)->textContent = str;
	}
	SMJS_SET_RVAL( dom_text_construct(c, n) );
	return JS_TRUE;
}

static void xml_doc_gather_nodes(GF_ParentNode *node, char *name, DOMNodeList *nl)
{
	Bool bookmark = GF_TRUE;
	GF_ChildNodeItem *child;
	const char *node_name;
	if (!node) return;
	if (name) {
		node_name = gf_node_get_class_name((GF_Node*)node);
		if (strcmp(node_name, name)) bookmark = GF_FALSE;
	}
	if (bookmark) {
		gf_node_register((GF_Node*)node, NULL);
		if (node->sgprivate->scenegraph->reference_count)
			node->sgprivate->scenegraph->reference_count++;
		gf_node_list_add_child(&nl->child, (GF_Node*)node);
	}
	if (node->sgprivate->tag<GF_NODE_FIRST_PARENT_NODE_TAG) return;
	child = node->children;
	while (child) {
		xml_doc_gather_nodes((GF_ParentNode*)child->node, name, nl);
		child = child->next;
	}
}

static JSBool SMJS_FUNCTION(xml_document_elements_by_tag)
{
	DOMNodeList *nl;
	JSObject *new_obj;
	char *name;
	SMJS_OBJ
	SMJS_ARGS
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;

	/*NS version - TODO*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		name = SMJS_CHARS(c, argv[1]);
	} else {
		name = SMJS_CHARS(c, argv[0]);
	}

	GF_SAFEALLOC(nl, DOMNodeList);
	if (name && !strcmp(name, "*")) name = NULL;
	xml_doc_gather_nodes((GF_ParentNode*)sg->RootNode, name, nl);
	new_obj = JS_NewObject(c, &dom_rt->domNodeListClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, new_obj, nl);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(new_obj));
	SMJS_FREE(c, name);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_document_element_by_id)
{
	NodeIDedItem *reg_node;
	GF_Node *n;
	char *id;
	SMJS_OBJ
	SMJS_ARGS
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_FALSE;
	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	id = SMJS_CHARS(c, argv[0]);

	/*we don't use the regular gf_sg_find_node_by_name because we may have nodes defined with the
	same ID and we need to locate the first one which is inserted in the tree*/
	n = NULL;
	reg_node = sg->id_node;
	while (reg_node) {
		if (reg_node->NodeName && !strcmp(reg_node->NodeName, id)) {
			n = reg_node->node;
			/*element is not inserted - fixme, we should check all parents*/
			if (n && (n->sgprivate->scenegraph->RootNode!=n) && !n->sgprivate->parents) n = NULL;
			else break;
		}
		reg_node = reg_node->next;
	}
	SMJS_SET_RVAL( dom_element_construct(c, n));
	SMJS_FREE(c, id);
	return JS_TRUE;
}

/*dom3 element*/
DECL_FINALIZE( dom_element_finalize)

	dom_node_finalize(c, obj);
}

static SMJS_FUNC_PROP_GET( dom_element_getProperty) 

	u32 prop_id;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
	prop_id = SMJS_ID_TO_INT(id);
	switch (prop_id) {
	case 1: /*"tagName"*/
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, gf_node_get_class_name(n) ) );
		return JS_TRUE;
	case 2: /*"schemaTypeInfo"*/
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_element_get_attribute)
{
	char *name, *ns;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0]))
		return JS_TRUE;
	name = ns = NULL;
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1]))
			return JS_TRUE;
		ns = js_get_utf8(c, argv[0]);
		name = SMJS_CHARS(c, argv[1]);
	} else {
		name = SMJS_CHARS(c, argv[0]);
	}
	if (!name) goto exit;

	/*ugly ugly hack ...*/
	if (!strcmp(name, "id") || !strcmp(name, "xml:id") ) {
		char *sID = (char *) gf_node_get_name(n);
		if (sID) {
			SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, sID ? sID : "") ) );
			goto exit;
		}
	}

	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, name)) {
				SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, *(char**)att->data ) ));
				goto exit;
			}
			att = (GF_DOMFullAttribute *) att->next;
		}
	}
	else if (n->sgprivate->tag==TAG_DOMText) {

	} else if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) {
		GF_FieldInfo info;
		u32 ns_code = 0;
		if (ns) {
			ns_code = gf_sg_get_namespace_code_from_name(n->sgprivate->scenegraph, ns);
			if (!ns_code) ns_code = gf_crc_32(ns, (u32) strlen(ns));
		}
		else {
			ns_code = gf_xml_get_element_namespace(n);
		}

		if (gf_node_get_attribute_by_name(n, name, ns_code, GF_FALSE, GF_FALSE, &info)==GF_OK) {
			char *szAtt = gf_svg_dump_attribute(n, &info);
			SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, szAtt) ) );
			if (szAtt) gf_free(szAtt);
			goto exit;
		}
	}
	SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, "") ));
exit:
	if (ns) gf_free(ns);
	SMJS_FREE(c, name);
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(xml_element_has_attribute)
{
	char *name, *ns;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_node(c, obj);
	if (!n)
		return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	name = ns = NULL;
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		ns = js_get_utf8(c, argv[0]);
		name = SMJS_CHARS(c, argv[1]);
	} else {
		name = SMJS_CHARS(c, argv[0]);
	}
	if (!name) goto exit;

	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, name)) {
				SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE));
				goto exit;
			}
			att = (GF_DOMFullAttribute *) att->next;
		}
	}
	else if (n->sgprivate->tag==TAG_DOMText) {
	}
	else if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) {
		GF_FieldInfo info;
		u32 ns_code = 0;
		if (ns) ns_code = gf_sg_get_namespace_code_from_name(n->sgprivate->scenegraph, ns);
		else ns_code = gf_sg_get_namespace_code(n->sgprivate->scenegraph, NULL);

		if (gf_node_get_attribute_by_name(n, name, ns_code, GF_FALSE, GF_FALSE, &info)==GF_OK) {
			SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_TRUE) );
			goto exit;
		}
	}
	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_FALSE) );
exit:
	if (ns) gf_free(ns);
	SMJS_FREE(c, name);
	return JS_TRUE;
}



static JSBool SMJS_FUNCTION(xml_element_remove_attribute)
{
	u32 tag;
	GF_DOMFullNode *node;
	GF_DOMFullAttribute *prev, *att;
	char *name, *ns;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	name = ns = NULL;
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		ns = js_get_utf8(c, argv[0]);
		name = SMJS_CHARS(c, argv[1]);
	} else {
		name = SMJS_CHARS(c, argv[0]);
	}
	if (!name) goto exit;

	tag = TAG_DOM_ATT_any;
	node = (GF_DOMFullNode*)n;
	prev = NULL;
	att = (GF_DOMFullAttribute*)node->attributes;

	if (n->sgprivate->tag==TAG_DOMFullNode) tag = TAG_DOM_ATT_any;
	else if (n->sgprivate->tag==TAG_DOMText) {
		goto exit;
	} else if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) {
		u32 ns_code = 0;
		if (ns) ns_code = gf_sg_get_namespace_code_from_name(n->sgprivate->scenegraph, ns);
		else ns_code = gf_sg_get_namespace_code(n->sgprivate->scenegraph, NULL);

		tag = gf_xml_get_attribute_tag(n, name, ns_code);
	}

	while (att) {
		if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, name)) {
			if (prev) prev->next = att->next;
			else node->attributes = att->next;
			if (att->data) gf_free(att->data);
			gf_free(att->name);
			gf_free(att);
			dom_node_changed(n, GF_FALSE, NULL);
			goto exit;
		} else if (tag==att->tag) {
			if (prev) prev->next = att->next;
			else node->attributes = att->next;
			gf_svg_delete_attribute_value(att->data_type, att->data, n->sgprivate->scenegraph);
			gf_free(att);
			dom_node_changed(n, GF_FALSE, NULL);
			goto exit;
		}
		prev = att;
		att = (GF_DOMFullAttribute *) att->next;
	}
exit:
	if (ns) gf_free(ns);
	SMJS_FREE(c, name);
	return JS_TRUE;
}

static void gf_dom_add_handler_listener(GF_Node *n, u32 evtType, char *handlerCode)
{
	/*check if we're modifying an existing listener*/
	SVG_handlerElement *handler;
	u32 i, count = gf_dom_listener_count(n);
	for (i=0;i<count; i++) {
		GF_FieldInfo info;
		GF_DOMText *text;
		GF_Node *listen = gf_dom_listener_get(n, i);

		gf_node_get_attribute_by_tag(listen, TAG_XMLEV_ATT_event, GF_FALSE, GF_FALSE, &info);
		if (!info.far_ptr || (((XMLEV_Event*)info.far_ptr)->type != evtType)) continue;

		/* found a listener for this event, override the handler 
		TODO: FIX this, there may be a listener/handler already set with JS, why overriding ? */
		gf_node_get_attribute_by_tag(listen, TAG_XMLEV_ATT_handler, GF_FALSE, GF_FALSE, &info);
		assert(info.far_ptr);
		handler = (SVG_handlerElement *) ((XMLRI*)info.far_ptr)->target;
		text = (GF_DOMText*)handler->children->node;
		if (text->sgprivate->tag==TAG_DOMText) {
			if (text->textContent) gf_free(text->textContent);
			text->textContent = gf_strdup(handlerCode);
		}
		return;
	}
	/*nope, create a listener*/
	handler = gf_dom_listener_build(n, evtType, 0);
	gf_dom_add_text_node((GF_Node*)handler, gf_strdup(handlerCode));
	return;
}

static void gf_dom_full_set_attribute(GF_DOMFullNode *node, char *attribute_name, char *attribute_content)
{
	GF_DOMFullAttribute *prev = NULL;
	GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
	while (att) {
		if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, attribute_name)) {
			if (att->data) gf_free(att->data);
			att->data = gf_strdup(attribute_content);
			dom_node_changed((GF_Node *)node, GF_FALSE, NULL);
			return;
		}
		prev = att;
		att = (GF_DOMFullAttribute *) att->next;
	}
	/*create new att*/
	GF_SAFEALLOC(att, GF_DOMFullAttribute);
	att->name = gf_strdup(attribute_name);
	att->data = gf_strdup(attribute_content);
	if (prev) prev->next = (GF_DOMAttribute*) att;
	else node->attributes = (GF_DOMAttribute*) att;
	return;
}

void gf_svg_set_attributeNS(GF_Node *n, u32 ns_code, char *name, char *val)
{
	GF_FieldInfo info;
	u32 anim_value_type = 0;

	if (!strcmp(name, "attributeName")) {
		if (gf_node_get_attribute_by_tag(n, TAG_SVG_ATT_attributeName, GF_FALSE, GF_FALSE, &info) == GF_OK) {
			SMIL_AttributeName *attname = (SMIL_AttributeName *)info.far_ptr;

			/*parse the attribute name even if the target is not found, because a namespace could be specified and 
			only valid for the current node*/
			if (!attname->type) {
				char *sep;
				char *name = attname->name;
				sep = strchr(name, ':');
				if (sep) {
					sep[0] = 0;
					attname->type = gf_sg_get_namespace_code(n->sgprivate->scenegraph, name);
					sep[0] = ':';
					name = gf_strdup(sep+1);
					gf_free(attname->name);
					attname->name = name;
				}
			}
		}
	}

	if ((n->sgprivate->tag == TAG_SVG_animateTransform) && (strstr(name, "from") || strstr(name, "to")) ) {
		if (gf_node_get_attribute_by_tag((GF_Node *)n, TAG_SVG_ATT_transform_type, GF_TRUE, GF_FALSE, &info) != GF_OK) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_SCRIPT, ("Cannot retrieve attribute 'type' from animateTransform\n"));
			return;
		}

		switch(*(SVG_TransformType *) info.far_ptr) {
		case SVG_TRANSFORM_TRANSLATE: anim_value_type = SVG_Transform_Translate_datatype; break;
		case SVG_TRANSFORM_SCALE: anim_value_type = SVG_Transform_Scale_datatype; break;
		case SVG_TRANSFORM_ROTATE: anim_value_type = SVG_Transform_Rotate_datatype; break;
		case SVG_TRANSFORM_SKEWX: anim_value_type = SVG_Transform_SkewX_datatype; break;
		case SVG_TRANSFORM_SKEWY: anim_value_type = SVG_Transform_SkewY_datatype; break;
		case SVG_TRANSFORM_MATRIX: anim_value_type = SVG_Transform_datatype; break;
		default:
			return;
		}
	}

	if (gf_node_get_attribute_by_name(n, name, ns_code,  GF_TRUE, GF_TRUE, &info)==GF_OK) {
		GF_Err e;
		if (!strcmp(name, "from") || !strcmp(name, "to") || !strcmp(name, "values") ) {
			GF_FieldInfo attType;
			SMIL_AttributeName *attname;
			if (gf_node_get_attribute_by_tag((GF_Node *)n, TAG_SVG_ATT_attributeName, GF_FALSE, GF_FALSE, &attType) != GF_OK) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_SCRIPT, ("Cannot retrieve attribute 'attributeName'\n"));
				return;
			}
		
			attname = (SMIL_AttributeName *)attType.far_ptr;
			if (!attname->type && attname->name) {
				GF_Node *anim_target = gf_smil_anim_get_target(n);
				if (anim_target) {
					gf_node_get_attribute_by_name((GF_Node *)anim_target, attname->name, attname->type, GF_FALSE, GF_FALSE, &attType);
					attname->type = attType.fieldType;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DOM] Cannot find attribute 'type' on <%s> element and cannot find target of the animation to parse attribute %s\n", gf_node_get_class_name(n), attname->name));
				}
			}

			anim_value_type = attname->type;
		}
		e = gf_svg_parse_attribute(n, &info, val, anim_value_type);
		if (e != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DOM] Error parsing attribute\n"));
		}

		if (info.fieldType==SVG_ID_datatype) {
			char *idname = *(SVG_String*)info.far_ptr;
			gf_svg_parse_element_id(n, idname, GF_FALSE);
		}
		if (info.fieldType==XMLRI_datatype) {
			gf_node_dirty_set(n, GF_SG_SVG_XLINK_HREF_DIRTY, GF_FALSE);
		}
		dom_node_changed(n, GF_FALSE, &info);
		return;
	}
}

void gf_svg_set_attribute(GF_Node *n, char * ns, char *name, char *val)
{
	u32 ns_code = 0;
	if (ns) {
		ns_code = gf_sg_get_namespace_code_from_name(n->sgprivate->scenegraph, ns);
	} else {
		ns_code = gf_xml_get_element_namespace(n);
	}
	gf_svg_set_attributeNS(n, ns_code, name, val);
}

JSBool SMJS_FUNCTION(xml_element_set_attribute)
{
	u32 idx;
	char *name, *val, *ns, *_val;
	char szVal[100];
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	if ((argc < 2)) return JS_TRUE;

	if (!JSVAL_CHECK_STRING(argv[0])) 
		return JS_TRUE;

	idx = 1;
	_val = name = ns = NULL;
	/*NS version*/
	if (argc==3) {
		char *sep;
		if (!JSVAL_CHECK_STRING(argv[1])) 
			return JS_TRUE;
		ns = js_get_utf8(c, argv[0]);
		gf_sg_add_namespace(n->sgprivate->scenegraph, ns, NULL);
		name = SMJS_CHARS(c, argv[1]);
		idx = 2;

		sep = strchr(name, ':');
		if (sep) name = sep+1;

	} else {
		name = SMJS_CHARS(c, argv[0]);
	}

	val = NULL;
	if (JSVAL_CHECK_STRING(argv[idx])) {
		val = _val = SMJS_CHARS(c, argv[idx]);
	} else if (JSVAL_IS_INT(argv[idx])) {
		sprintf(szVal, "%d", JSVAL_TO_INT(argv[idx]));
		val = szVal;
	} else if (JSVAL_IS_NUMBER(argv[idx])) {
		jsdouble d;
		JS_ValueToNumber(c, argv[idx], &d);
		sprintf(szVal, "%g", d);
		val = szVal;
	} else if (JSVAL_IS_BOOLEAN(argv[idx])) {
		sprintf(szVal, "%s", JSVAL_TO_BOOLEAN(argv[idx]) ? "true" : "false");
		val = szVal;
	} else {
		goto exit;
	}
	if (!name || !val) 
		goto exit;


	/* For on* attribute (e.g. onclick), we create a couple listener/handler elements on setting the attribute */
	if ((name[0]=='o') && (name[1]=='n')) {
		u32 evtType = gf_dom_event_type_by_name(name + 2);
		if (evtType != GF_EVENT_UNKNOWN) {
			gf_dom_add_handler_listener(n, evtType, val);
			goto exit;
		}
	}

	if (n->sgprivate->tag==TAG_DOMFullNode) {
		gf_dom_full_set_attribute((GF_DOMFullNode*)n, name, val);
		goto exit;
	}

	if (n->sgprivate->tag==TAG_DOMText) {
		goto exit;
	}

	if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) {
		gf_svg_set_attribute(n, ns, name, val);
	}
exit:
	if (ns) gf_free(ns);
	SMJS_FREE(c, name);
	SMJS_FREE(c, _val);
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(xml_element_elements_by_tag)
{
	DOMNodeList *nl;
	JSObject *new_obj;
	char *name;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;

	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		name = SMJS_CHARS(c, argv[1]);
	} else {
		name = SMJS_CHARS(c, argv[0]);
	}
	GF_SAFEALLOC(nl, DOMNodeList);
	if (name && !strcmp(name, "*")) {
		SMJS_FREE(c, name);
		name = NULL;
	}
	xml_doc_gather_nodes((GF_ParentNode*)n, name, nl);
	new_obj = JS_NewObject(c, &dom_rt->domNodeListClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, new_obj, nl);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(new_obj) );
	
	SMJS_FREE(c, name);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_element_set_id)
{
	u32 node_id;
	char *name;
	Bool is_id;
	SMJS_OBJ
	SMJS_ARGS
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if ((argc<2) || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;

	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		name = SMJS_CHARS(c, argv[1]);
		is_id = (JSVAL_TO_BOOLEAN(argv[2])==JS_TRUE) ? GF_TRUE : GF_FALSE;
	} else {
		name = SMJS_CHARS(c, argv[0]);
		is_id = (JSVAL_TO_BOOLEAN(argv[1])==JS_TRUE) ? GF_TRUE : GF_FALSE;
	}
	gf_node_get_name_and_id(n, &node_id);
	if (node_id && is_id) {
		/*we only support ONE ID per node*/
		SMJS_FREE(c, name);
		return JS_TRUE;
	}
	if (is_id) {
		if (!name) return JS_TRUE;
		gf_node_set_id(n, gf_sg_get_max_node_id(n->sgprivate->scenegraph) + 1, gf_strdup(name) );
	} else if (node_id) {
		gf_node_remove_id(n);
	}
	SMJS_FREE(c, name);
	return JS_TRUE;
}


/*dom3 character/text/comment*/

static SMJS_FUNC_PROP_GET( dom_text_getProperty) 

	u32 prop_id;
	GF_DOMText *txt = (GF_DOMText*)dom_get_node(c, obj);
	if (!txt || (txt->sgprivate->tag != TAG_DOMText)) return JS_TRUE;
	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
	prop_id = SMJS_ID_TO_INT(id);

	switch (prop_id) {
	case 1: /*"data"*/
		if (txt->textContent) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, txt->textContent ) );
		else *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "") );
		return JS_TRUE;
	case 2:/*"length"*/
		*vp = INT_TO_JSVAL(txt->textContent ? strlen(txt->textContent) : 0);
		return JS_TRUE;
	case 3:/*"isElementContentWhitespace"*/
		*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	case 4:/*"wholeText"*/
		/*FIXME - this is wrong*/
		*vp = INT_TO_JSVAL(txt->textContent ? strlen(txt->textContent) : 0);
		return JS_TRUE;
	}
	return JS_TRUE;
}
static SMJS_FUNC_PROP_SET( dom_text_setProperty)

	u32 prop_id;
	GF_DOMText *txt = (GF_DOMText*)dom_get_node(c, obj);
	if (!txt || (txt->sgprivate->tag != TAG_DOMText)) return JS_TRUE;
	if (!SMJS_ID_IS_INT(id)) return JS_TRUE;
	prop_id = SMJS_ID_TO_INT(id);

	switch (prop_id) {
	case 1: /*"data"*/
		if (txt->textContent) gf_free(txt->textContent);
		txt->textContent = NULL;
		if (JSVAL_CHECK_STRING(*vp)) {
			char *str = js_get_utf8(c, *vp);
			txt->textContent = str ? str : gf_strdup("" );
		}
		dom_node_changed((GF_Node*)txt, GF_FALSE, NULL);
		return JS_TRUE;
	/*the rest is read-only*/
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(event_stop_propagation)
{
	SMJS_OBJ
	GF_DOM_Event *evt = (GF_DOM_Event *)SMJS_GET_PRIVATE(c, obj);
	if (!evt) return JS_TRUE;
	evt->event_phase |= GF_DOM_EVENT_PHASE_CANCEL;
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(event_stop_immediate_propagation)
{
	SMJS_OBJ
	GF_DOM_Event *evt = (GF_DOM_Event *)SMJS_GET_PRIVATE(c, obj);
	if (!evt) return JS_TRUE;
	evt->event_phase |= GF_DOM_EVENT_PHASE_CANCEL_ALL;
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(event_prevent_default)
{
	SMJS_OBJ
	GF_DOM_Event *evt = (GF_DOM_Event *)SMJS_GET_PRIVATE(c, obj);
	if (!evt) return JS_TRUE;
	evt->event_phase |= GF_DOM_EVENT_PHASE_PREVENT;
	return JS_TRUE;
}

static SMJS_FUNC_PROP_GET( event_getProperty)

	JSString *s;
	GF_DOM_Event *evt = (GF_DOM_Event *)SMJS_GET_PRIVATE(c, obj);
	if (evt==NULL) return JS_TRUE;
	if (SMJS_ID_IS_INT(id)) {
		switch (SMJS_ID_TO_INT(id)) {
		case 0: /*type*/
			s = JS_NewStringCopyZ(c, gf_dom_event_get_name(evt->type) );
			*vp = STRING_TO_JSVAL( s );
			break;
		case 1: /*target*/
			if (evt->is_vrml) return JS_TRUE;
			switch (evt->target_type) {
			case GF_DOM_EVENT_NODE:
				*vp = dom_element_construct(c, (GF_Node*) evt->target);
				break;
			case GF_DOM_EVENT_DOCUMENT:
				*vp = dom_document_construct(c, (GF_SceneGraph *) evt->target);
				break;
			}
			return JS_TRUE;
		case 2:	/*currentTarget*/
			if (evt->is_vrml) return JS_TRUE;
			switch (evt->currentTarget->ptr_type) {
			case GF_DOM_EVENT_NODE:
				*vp = dom_element_construct(c, (GF_Node*) evt->currentTarget->ptr);
				break;
			case GF_DOM_EVENT_DOCUMENT:
				*vp = dom_document_construct(c, (GF_SceneGraph *) evt->currentTarget->ptr);
				break;
            default:
                break;
			}
			return JS_TRUE;
		/*eventPhase */
		case 3:
			*vp = INT_TO_JSVAL( (evt->event_phase & 0x3) ); return JS_TRUE;
		case 4: /*bubbles*/
			*vp = BOOLEAN_TO_JSVAL(evt->bubbles ? JS_TRUE : JS_FALSE); return JS_TRUE;
		case 5: /*cancelable*/
			*vp = BOOLEAN_TO_JSVAL(evt->cancelable ? JS_TRUE : JS_FALSE); return JS_TRUE;
		case 6: /*namespaceURI*/
			*vp = JSVAL_NULL;
			return JS_TRUE;
		/*timeStamp */
		case 7:
			*vp = JSVAL_VOID;
			return JS_TRUE;
		/*defaultPrevented */
		case 8:
			*vp = BOOLEAN_TO_JSVAL((evt->event_phase & GF_DOM_EVENT_PHASE_PREVENT) ? JS_TRUE : JS_FALSE); return JS_TRUE;


		case 20:/*detail*/
			*vp = INT_TO_JSVAL(evt->detail); return JS_TRUE;

		case 25: /*data*/
		{
			u32 len;
			s16 txt[2];
			const u16 *srcp;
			char szData[5];;
			txt[0] = evt->detail;
			txt[1] = 0;
			srcp = (const u16 *) txt;
			len = (u32) gf_utf8_wcstombs(szData, 5, &srcp);
			szData[len] = 0;
			s = JS_NewStringCopyZ(c, szData);
			*vp = STRING_TO_JSVAL( s );
		}
			return JS_TRUE;

		case 30:/*screenX*/
			*vp = INT_TO_JSVAL(evt->screenX); return JS_TRUE;
		case 31: /*screenY*/
			*vp = INT_TO_JSVAL(evt->screenY); return JS_TRUE;
		case 32: /*clientX*/
			*vp = INT_TO_JSVAL(evt->clientX); return JS_TRUE;
		case 33: /*clientY*/
			*vp = INT_TO_JSVAL(evt->clientY); return JS_TRUE;
		case 34:/*button*/
			*vp = INT_TO_JSVAL(evt->button); return JS_TRUE;
		case 35:/*relatedTarget*/
			if (evt->is_vrml) return JS_TRUE;
			*vp = dom_element_construct(c, evt->relatedTarget);
			return JS_TRUE;
		case 36:/*wheelDelta*/
			*vp = INT_TO_JSVAL(FIX2INT(evt->new_scale) ); return JS_TRUE;


		/*DOM3 event keyIndentifier*/
		case 40:
			s = JS_NewStringCopyZ(c, gf_dom_get_key_name(evt->detail) );
			*vp = STRING_TO_JSVAL( s );
			 return JS_TRUE;
		/*Mozilla keyChar, charCode: wrap up to same value*/
		case 41:
		case 42:
			*vp = INT_TO_JSVAL(evt->detail); return JS_TRUE;

		case 52:/*loaded*/
			if (!evt->media_event) return JS_TRUE;
			*vp = INT_TO_JSVAL( evt->media_event->loaded_size);
			return JS_TRUE;
		case 53:/*total*/
			if (!evt->media_event) return JS_TRUE;
			*vp = INT_TO_JSVAL( evt->media_event->total_size);
			return JS_TRUE;
		case 54:/*bufferLevelValid*/
			if (!evt->media_event) return JS_TRUE;
			*vp = BOOLEAN_TO_JSVAL( evt->media_event->bufferValid ? JS_TRUE : JS_FALSE);
			return JS_TRUE;
		case 55:/*bufferLevel*/
			if (!evt->media_event) return JS_TRUE;
			*vp = INT_TO_JSVAL( evt->media_event->level);
			return JS_TRUE;
		case 56:/*bufferRemainingTime*/
			if (!evt->media_event) return JS_TRUE;
			*vp = JS_MAKE_DOUBLE(c, evt->media_event->remaining_time);
			return JS_TRUE;
		case 57:/*status*/
			if (!evt->media_event) return JS_TRUE;
			*vp = INT_TO_JSVAL( evt->media_event->status);
			return JS_TRUE;

		/*VRML ones*/
		case 60:/*width*/
			*vp = JS_MAKE_DOUBLE(c, FIX2FLT(evt->screen_rect.width) );
			return JS_TRUE;
		case 61:/*height*/
			*vp = JS_MAKE_DOUBLE(c, FIX2FLT(evt->screen_rect.height) );
			return JS_TRUE;
		case 62:/*offset_x*/
			*vp = JS_MAKE_DOUBLE(c, FIX2FLT(evt->screen_rect.x) );
			return JS_TRUE;
		case 63:/*offset_x*/
			*vp = JS_MAKE_DOUBLE(c, FIX2FLT(evt->screen_rect.y) );
			return JS_TRUE;
		case 64:/*vp_width*/
			*vp = JS_MAKE_DOUBLE(c, FIX2FLT(evt->prev_translate.x) );
			return JS_TRUE;
		case 65:/*vp_height*/
			*vp = JS_MAKE_DOUBLE(c, FIX2FLT(evt->prev_translate.y) );
			return JS_TRUE;
		case 66:/*translation_x*/
			*vp = JS_MAKE_DOUBLE(c, FIX2FLT(evt->new_translate.x) );
			return JS_TRUE;
		case 67:/*translation_y*/
			*vp = JS_MAKE_DOUBLE(c, FIX2FLT(evt->new_translate.y) );
			return JS_TRUE;
		case 68:/*type3d*/
			*vp = INT_TO_JSVAL(evt->detail); return JS_TRUE;
		case 69:/*error*/
			*vp = INT_TO_JSVAL(evt->error_state); return JS_TRUE;

		default: return JS_TRUE;
		}
	}
	return JS_TRUE;
}

/************************************************************
 *
 *	xmlHttpRequest implementation
 *
 *************************************************************/
typedef enum {
	XHR_READYSTATE_UNSENT,
	XHR_READYSTATE_OPENED,
	XHR_READYSTATE_HEADERS_RECEIVED,
	XHR_READYSTATE_LOADING,
	XHR_READYSTATE_DONE
} XHR_ReadyState;

typedef enum {
	XHR_RESPONSETYPE_NONE,
	XHR_RESPONSETYPE_ARRAYBUFFER,
	XHR_RESPONSETYPE_BLOB,
	XHR_RESPONSETYPE_DOCUMENT,
	XHR_RESPONSETYPE_JSON,
	XHR_RESPONSETYPE_TEXT,
	XHR_RESPONSETYPE_STREAM
} XHR_ResponseType;

typedef struct __xhr_context
{
	JSContext *c;
	JSObject *_this;
	JSFunction *onreadystatechange;

	XHR_ReadyState readyState;
	Bool async;
	/*header/header-val, terminated by NULL*/
	char **headers;
	u32 cur_header;
	char **recv_headers;

	char *method, *url;
	GF_DownloadSession *sess;
	char *data;
	u32 size;
	JSObject *arraybuffer; 
	GF_Err ret_code;
	u32 html_status;
	char *statusText;
	char *mime;
	u32 timeout;
	XHR_ResponseType responseType;
	Bool withCredentials;
	Bool isFile;

	GF_SAXParser *sax;
	GF_List *node_stack;
	/*dom graph*/
	GF_SceneGraph *document;
} XMLHTTPContext;

static void xml_http_reset_recv_hdr(XMLHTTPContext *ctx)
{
	u32 nb_hdr = 0;
	if (ctx->recv_headers) {
		while (ctx->recv_headers[nb_hdr]) {
			gf_free(ctx->recv_headers[nb_hdr]);
			gf_free(ctx->recv_headers[nb_hdr+1]);
			nb_hdr+=2;
		}
		gf_free(ctx->recv_headers);
		ctx->recv_headers = NULL;
	}
}

static void xml_http_append_recv_header(XMLHTTPContext *ctx, const char *hdr, const char *val)
{
	u32 nb_hdr = 0;
	if (ctx->recv_headers) {
		while (ctx->recv_headers[nb_hdr]) nb_hdr+=2;
	}
	ctx->recv_headers = (char **)gf_realloc(ctx->recv_headers, sizeof(char*)*(nb_hdr+3));
	ctx->recv_headers[nb_hdr] = gf_strdup(hdr);
	ctx->recv_headers[nb_hdr+1] = gf_strdup(val ? val : "");
	ctx->recv_headers[nb_hdr+2] = NULL;
}

static void xml_http_append_send_header(XMLHTTPContext *ctx, char *hdr, char *val)
{
	if (!hdr) return;

	if (ctx->headers) {
		u32 nb_hdr = 0;
		while (ctx->headers && ctx->headers[nb_hdr]) {
			if (stricmp(ctx->headers[nb_hdr], hdr)) {
				nb_hdr+=2;
				continue;
			}
			/*ignore these ones*/
			if (!stricmp(hdr, "Accept-Charset")
				|| !stricmp(hdr, "Accept-Encoding")
				|| !stricmp(hdr, "Content-Length")
				|| !stricmp(hdr, "Expect")
				|| !stricmp(hdr, "Date")
				|| !stricmp(hdr, "Host")
				|| !stricmp(hdr, "Keep-Alive")
				|| !stricmp(hdr, "Referer")
				|| !stricmp(hdr, "TE")
				|| !stricmp(hdr, "Trailer")
				|| !stricmp(hdr, "Transfer-Encoding")
				|| !stricmp(hdr, "Upgrade")
				) {
				return;
			}

			/*replace content for these ones*/
			if (!stricmp(hdr, "Authorization")
				|| !stricmp(hdr, "Content-Base")
				|| !stricmp(hdr, "Content-Location")
				|| !stricmp(hdr, "Content-MD5")
				|| !stricmp(hdr, "Content-Range")
				|| !stricmp(hdr, "Content-Type")
				|| !stricmp(hdr, "Content-Version")
				|| !stricmp(hdr, "Delta-Base")
				|| !stricmp(hdr, "Depth")
				|| !stricmp(hdr, "Destination")
				|| !stricmp(hdr, "ETag")
				|| !stricmp(hdr, "From")
				|| !stricmp(hdr, "If-Modified-Since")
				|| !stricmp(hdr, "If-Range")
				|| !stricmp(hdr, "If-Unmodified-Since")
				|| !stricmp(hdr, "Max-Forwards")
				|| !stricmp(hdr, "MIME-Version")
				|| !stricmp(hdr, "Overwrite")
				|| !stricmp(hdr, "Proxy-Authorization")
				|| !stricmp(hdr, "SOAPAction")
				|| !stricmp(hdr, "Timeout") ) {
				gf_free(ctx->headers[nb_hdr+1]);
				ctx->headers[nb_hdr+1] = gf_strdup(val);
				return;
			}
			/*append value*/
			else {
				char *new_val = (char *)gf_malloc(sizeof(char) * (strlen(ctx->headers[nb_hdr+1])+strlen(val)+3));
				sprintf(new_val, "%s, %s", ctx->headers[nb_hdr+1], val);
				gf_free(ctx->headers[nb_hdr+1]);
				ctx->headers[nb_hdr+1] = new_val;
				return;
			}
			nb_hdr+=2;
		}
	}
	xml_http_append_recv_header(ctx, hdr, val);
}

void xhr_del_array_buffer(void *udta)
{
	if (udta)
		((XMLHTTPContext *)udta)->arraybuffer = NULL;;
}

static void xml_http_del_data(XMLHTTPContext *ctx)
{
	if (ctx->data) {
		if (ctx->arraybuffer) {
			/* if there is an arraybuffer holding a point to that data, we need to release it */
			if (ctx->arraybuffer) {
//				GF_HTML_ArrayBuffer *html_array = (GF_HTML_ArrayBuffer *)SMJS_GET_PRIVATE(ctx->c, ctx->arraybuffer);
				//detach the ArrayBuffer from this object
				JS_SetParent(ctx->c, ctx->arraybuffer, NULL);
				ctx->arraybuffer = NULL;
			}
		} else {
			gf_free(ctx->data);
		}
		ctx->data = NULL;
		ctx->size = 0;
	}
}

static void xml_http_reset_partial(XMLHTTPContext *ctx)
{
	xml_http_reset_recv_hdr(ctx);
	xml_http_del_data(ctx);
	if (ctx->mime) {
		gf_free(ctx->mime);
		ctx->mime = NULL;
	}
	if (ctx->statusText) {
		gf_free(ctx->statusText);
		ctx->statusText = NULL;
	}
	ctx->cur_header = 0;
	ctx->html_status = 0;
}

static void xml_http_reset(XMLHTTPContext *ctx)
{
	if (ctx->method) { gf_free(ctx->method); ctx->method = NULL; }
	if (ctx->url) { gf_free(ctx->url); ctx->url = NULL; }

	xml_http_reset_partial(ctx);

	if (ctx->sess) {
		GF_DownloadSession *tmp = ctx->sess;
		ctx->sess = NULL;
		gf_dm_sess_abort(tmp);
		gf_dm_sess_del(tmp);
	}

	if (ctx->url) {
		gf_free(ctx->url);
		ctx->url = NULL;
	}
	if (ctx->sax) {
		gf_xml_sax_del(ctx->sax);
		ctx->sax = NULL;
	}
	if (ctx->node_stack) {
		gf_list_del(ctx->node_stack);
		ctx->node_stack = NULL;
	}
	if (ctx->document) {
		gf_node_unregister(ctx->document->RootNode, NULL);
		/*we're sure the graph is a "nomade" one since we initially put the refcount to 1 ourselves*/
		ctx->document->reference_count--;
		if (!ctx->document->reference_count) {
			dom_js_pre_destroy(ctx->c, ctx->document, NULL);
			gf_sg_del(ctx->document);
		}
	}
	ctx->document = NULL;
	ctx->size = 0;
	ctx->async = GF_FALSE;
	ctx->readyState = XHR_READYSTATE_UNSENT;
	ctx->ret_code = GF_OK;
}

static DECL_FINALIZE( xml_http_finalize)

	XMLHTTPContext *ctx;
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (ctx) {
		if (ctx->onreadystatechange) gf_js_remove_root(c, &(ctx->onreadystatechange), GF_JSGC_VAL);
		xml_http_reset(ctx);
		gf_free(ctx);
	}
}
static JSBool SMJS_FUNCTION(xml_http_constructor)
{
	XMLHTTPContext *p;
	SMJS_OBJ_CONSTRUCTOR(&dom_rt->xmlHTTPRequestClass)

	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	GF_SAFEALLOC(p, XMLHTTPContext);
	p->c = c;
	p->_this = obj;
	SMJS_SET_PRIVATE(c, obj, p);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(obj) );
	return JS_TRUE;
}

static void xml_http_state_change(XMLHTTPContext *ctx)
{
	GF_SceneGraph *scene;
	GF_Node *n;
	jsval rval;
	
	gf_sg_lock_javascript(ctx->c, GF_TRUE);
	if (ctx->onreadystatechange)
		JS_CallFunction(ctx->c, ctx->_this, ctx->onreadystatechange, 0, NULL, &rval);

	/*todo - fire XHR events*/


	gf_sg_lock_javascript(ctx->c, GF_FALSE);

	/*Flush BIFS eventOut events*/
#ifndef GPAC_DISABLE_VRML
	scene = (GF_SceneGraph *)JS_GetContextPrivate(ctx->c);
	/*this is a scene, we look for a node (if scene is used, this is DOM-based scripting not VRML*/
	if (scene->__reserved_null == 0) return;
	n = (GF_Node *)JS_GetContextPrivate(ctx->c);
	gf_js_vrml_flush_event_out(n, (GF_ScriptPriv *)n->sgprivate->UserPrivate);
#endif
}


static JSBool SMJS_FUNCTION(xml_http_open)
{
	char *val;
	GF_JSAPIParam par;
	XMLHTTPContext *ctx;
	GF_SceneGraph *scene;
	SMJS_OBJ
	SMJS_ARGS

	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	/*reset*/
	if (ctx->readyState) xml_http_reset(ctx);

	if (argc<2) return JS_TRUE;
	/*method is a string*/
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	/*url is a string*/
	if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;

	xml_http_reset(ctx);
	val = SMJS_CHARS(c, argv[0]);
	if (strcmp(val, "GET") && strcmp(val, "POST") && strcmp(val, "HEAD")
	&& strcmp(val, "PUT") && strcmp(val, "DELETE") && strcmp(val, "OPTIONS") ) {
		SMJS_FREE(c, val);
		return JS_TRUE;
	}

	ctx->method = gf_strdup(val);
	
	SMJS_FREE(c, val);

	/*concatenate URL*/
	scene = xml_get_scenegraph(c);
#ifndef GPAC_DISABLE_VRML
	while (scene->pOwningProto && scene->parent_scene) scene = scene->parent_scene;
#endif

	par.uri.nb_params = 0;
	val = SMJS_CHARS(c, argv[1]);
	par.uri.url = val;
	ScriptAction(scene, GF_JSAPI_OP_RESOLVE_URI, scene->RootNode, &par);
	ctx->url = par.uri.url;
	SMJS_FREE(c, val);

	/*async defaults to true*/
	ctx->async = GF_TRUE;
	if (argc>2) {
		val = NULL;
		ctx->async = (JSVAL_TO_BOOLEAN(argv[2])==JS_TRUE) ? GF_TRUE : GF_FALSE;
		if (argc>3) {
			if (!JSVAL_CHECK_STRING(argv[3])) return JS_TRUE;
			/*TODO*/
			if (argc>4) {
				if (!JSVAL_CHECK_STRING(argv[4])) return JS_TRUE;
				val = SMJS_CHARS(c, argv[4]);
				/*TODO*/
			} else {
				val = SMJS_CHARS(c, argv[3]);
			}
		}
		SMJS_FREE(c, val);
	}
	/*OPEN success*/
	ctx->readyState = XHR_READYSTATE_OPENED;
	xml_http_state_change(ctx);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_http_set_header)
{
	char *hdr, *val;
	XMLHTTPContext *ctx;
	SMJS_OBJ
	SMJS_ARGS

	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	if (ctx->readyState!=XHR_READYSTATE_OPENED) return JS_TRUE;
	if (argc!=2) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;

	hdr = SMJS_CHARS(c, argv[0]);
	val = SMJS_CHARS(c, argv[1]);
	xml_http_append_send_header(ctx, hdr, val);
	SMJS_FREE(c, hdr);
	SMJS_FREE(c, val);
	return JS_TRUE;
}
static void xml_http_sax_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	u32 i;
	GF_DOMFullAttribute *prev = NULL;
	GF_DOMFullNode *par;
	XMLHTTPContext *ctx = (XMLHTTPContext *)sax_cbck;
	GF_DOMFullNode *node = (GF_DOMFullNode *) gf_node_new(ctx->document, TAG_DOMFullNode);

	node->name = gf_strdup(node_name);
	for (i=0; i<nb_attributes; i++) {
		/* special case for 'xml:id' to be parsed as an ID
		NOTE: we do not test for the 'id' attribute because without DTD we are not sure that it's an ID */
		if (!stricmp(attributes[i].name, "xml:id")) {
			u32 id = gf_sg_get_max_node_id(ctx->document) + 1;
			gf_node_set_id((GF_Node *)node, id, attributes[i].value);
		} else {
			GF_DOMFullAttribute *att;
			GF_SAFEALLOC(att, GF_DOMFullAttribute);
			att->tag = TAG_DOM_ATT_any;
			att->name = gf_strdup(attributes[i].name);
			att->data_type = (u16) DOM_String_datatype;
			att->data = gf_svg_create_attribute_value(att->data_type);
			*((char **)att->data) = gf_strdup(attributes[i].value);
			if (prev) prev->next = (GF_DOMAttribute*)att;
			else node->attributes = (GF_DOMAttribute*)att;
			prev = att;
		}
	}
	par = (GF_DOMFullNode *)gf_list_last(ctx->node_stack);
	gf_node_register((GF_Node*)node, (GF_Node*)par);
	if (par) {
		gf_node_list_add_child(&par->children, (GF_Node*)node);
	} else {
		assert(!ctx->document->RootNode);
		ctx->document->RootNode = (GF_Node*)node;
	}
	gf_list_add(ctx->node_stack, node);
}

static void xml_http_sax_end(void *sax_cbck, const char *node_name, const char *name_space)
{
	XMLHTTPContext *ctx = (XMLHTTPContext *)sax_cbck;
	GF_DOMFullNode *par = (GF_DOMFullNode *)gf_list_last(ctx->node_stack);
	if (par) {
		/*depth mismatch*/
		if (strcmp(par->name, node_name)) return;
		gf_list_rem_last(ctx->node_stack);
	}
}
static void xml_http_sax_text(void *sax_cbck, const char *content, Bool is_cdata)
{
	XMLHTTPContext *ctx = (XMLHTTPContext *)sax_cbck;
	GF_DOMFullNode *par = (GF_DOMFullNode *)gf_list_last(ctx->node_stack);
	if (par) {
		u32 i, len;
		GF_DOMText *txt;
		/*basic check, remove all empty text nodes*/
		len = (u32) strlen(content);
		for (i=0; i<len; i++) {
			if (!strchr(" \n\r\t", content[i])) break;
		}
		if (i==len) return;

		txt = gf_dom_add_text_node((GF_Node *)par, gf_strdup(content) );
		txt->type = is_cdata ? GF_DOM_TEXT_CDATA : GF_DOM_TEXT_REGULAR;
	}
}

#define USE_PROGRESSIVE_SAX	0

static void xml_http_on_data(void *usr_cbk, GF_NETIO_Parameter *parameter)
{
	Bool locked = GF_FALSE;
	XMLHTTPContext *ctx = (XMLHTTPContext *)usr_cbk;

	/*make sure we can grab JS and the session is not destroyed*/
	while (ctx->sess) {
		if (gf_sg_try_lock_javascript(ctx->c) ){
			locked = GF_TRUE;
			break;
		}
		gf_sleep(1);
	}
	/*if session not set, we've had an abort*/
	if (!ctx->sess && !ctx->isFile){
		if (locked)
			gf_sg_lock_javascript(ctx->c, GF_FALSE);
		return;
	}

	if (!ctx->isFile) {
		assert( locked );
		gf_sg_lock_javascript(ctx->c, GF_FALSE);
	}
	switch (parameter->msg_type) {
	case GF_NETIO_SETUP:
		/*nothing to do*/
		return;
	case GF_NETIO_CONNECTED:
		/*nothing to do*/
		return;
	case GF_NETIO_WAIT_FOR_REPLY:
		/*reset send() state (data, current header) and prepare recv headers*/
		xml_http_reset_partial(ctx);
		ctx->readyState = XHR_READYSTATE_HEADERS_RECEIVED;
		xml_http_state_change(ctx);
		return;
	/*this is signaled sent AFTER headers*/
	case GF_NETIO_PARSE_REPLY:
		ctx->html_status = parameter->reply;
		if (parameter->value) {
			ctx->statusText = gf_strdup(parameter->value);
		}
		ctx->readyState = XHR_READYSTATE_LOADING;
		xml_http_state_change(ctx);
		return;

	case GF_NETIO_GET_METHOD:
		parameter->name = ctx->method;
		return;
	case GF_NETIO_GET_HEADER:
		if (ctx->headers && ctx->headers[2*ctx->cur_header]) {
			parameter->name = ctx->headers[2*ctx->cur_header];
			parameter->value = ctx->headers[2*ctx->cur_header+1];
			ctx->cur_header++;
		}
		return;
	case GF_NETIO_GET_CONTENT:
		if (ctx->data) {
			parameter->data = ctx->data;
			parameter->size = (u32) strlen(ctx->data);
		}
		return;
	case GF_NETIO_PARSE_HEADER:
		xml_http_append_recv_header(ctx, parameter->name, parameter->value);
		/*prepare DOM*/
		if (strcmp(parameter->name, "Content-Type")) return;
		if (!strncmp(parameter->value, "application/xml", 15)
			|| !strncmp(parameter->value, "text/xml", 8)
			|| strstr(parameter->value, "+xml")
			|| strstr(parameter->value, "/xml")
//			|| !strncmp(parameter->value, "text/plain", 10)
		) {
			assert(!ctx->sax);
			ctx->sax = gf_xml_sax_new(xml_http_sax_start, xml_http_sax_end, xml_http_sax_text, ctx);
			ctx->node_stack = gf_list_new();
			ctx->document = gf_sg_new();
			/*mark this doc as "nomade", and let it leave until all references to it are destroyed*/
			ctx->document->reference_count = 1;
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_SCRIPT, ("[XmlHttpRequest] content type %s - ResponseXML not supported\n", parameter->value));
		}
		return;
	case GF_NETIO_DATA_EXCHANGE:
		if (parameter->data && parameter->size) {
#if USE_PROGRESSIVE_SAX
			if (ctx->sax) {
				GF_Err e;
				if (!ctx->size) e = gf_xml_sax_init(ctx->sax, parameter->data);
				else e = gf_xml_sax_parse(ctx->sax, parameter->data);
				if (e) {
					gf_xml_sax_del(ctx->sax);
					ctx->sax = NULL;
				}
			}
#endif
			ctx->data = (char *)gf_realloc(ctx->data, sizeof(char)*(ctx->size+parameter->size+1));
			memcpy(ctx->data + ctx->size, parameter->data, sizeof(char)*parameter->size);
			ctx->size += parameter->size;
			ctx->data[ctx->size] = 0;
		}
		return;
	case GF_NETIO_DATA_TRANSFERED:
		if (ctx->sax) {
#if !USE_PROGRESSIVE_SAX
			gf_xml_sax_init(ctx->sax, (unsigned char *) ctx->data);
#endif
		}
		/* No return, go till the end of the function */
		break;
	case GF_NETIO_DISCONNECTED:
		return;
	case GF_NETIO_STATE_ERROR:
		ctx->ret_code = parameter->error;
		/* No return, go till the end of the function */
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[XmlHttpRequest] Download error: %s\n", gf_error_to_string(parameter->error)));
		break;
	}

	/*if we get here, destroy downloader - FIXME we'll need a mutex here for sync case...*/
	if (ctx->sess) {
		gf_dm_sess_del(ctx->sess);
		ctx->sess = NULL;
	}

	/*error, complete reset*/
	if (parameter->error) {
		xml_http_reset(ctx);
	} else {
		ctx->html_status = 200;
	}
	/*but stay in loaded mode*/
	ctx->readyState = XHR_READYSTATE_DONE;
	xml_http_state_change(ctx);
}

static GF_Err xml_http_process_local(XMLHTTPContext *ctx)
{
	/* For XML Http Requests to files, we fake the processing by calling the HTTP callbacks */
	GF_NETIO_Parameter par;
	u64 fsize;
	FILE *responseFile;
		
	/*opera-style local host*/
	if (!strnicmp(ctx->url, "file://localhost", 16)) responseFile = gf_f64_open(ctx->url+16, "rb");
	/*regular-style local host*/
	else if (!strnicmp(ctx->url, "file://", 7)) responseFile = gf_f64_open(ctx->url+7, "rb");
	/* other types: e.g. "C:\" */
	else responseFile = gf_f64_open(ctx->url, "rb");

	if (!responseFile) {
		ctx->html_status = 404;
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[XmlHttpRequest] cannot open local file %s\n", ctx->url));
		return GF_BAD_PARAM;
	}
	ctx->isFile = GF_TRUE;

	par.msg_type = GF_NETIO_WAIT_FOR_REPLY;
	xml_http_on_data(ctx, &par);
		
	gf_f64_seek(responseFile, 0, SEEK_END);
	fsize = gf_f64_tell(responseFile);
	gf_f64_seek(responseFile, 0, SEEK_SET);
		
	ctx->data = (char *)gf_malloc(sizeof(char)*(size_t)(fsize+1));
	fsize = fread(ctx->data, sizeof(char), (size_t)fsize, responseFile);
	fclose(responseFile);
	ctx->data[fsize] = 0;
	ctx->size = (u32)fsize;
		
	memset(&par, 0, sizeof(GF_NETIO_Parameter));
	par.msg_type = GF_NETIO_PARSE_HEADER;
	par.name = "Content-Type";
	if (ctx->responseType == XHR_RESPONSETYPE_DOCUMENT) {
		par.value = "application/xml";
	} else {
		par.value = "application/octet-stream";
	}
	xml_http_on_data(ctx, &par);

	memset(&par, 0, sizeof(GF_NETIO_Parameter));
	par.msg_type = GF_NETIO_DATA_TRANSFERED;
	xml_http_on_data(ctx, &par);			

	return GF_OK;
}

static JSBool SMJS_FUNCTION(xml_http_send)
{
	GF_Err e;
	GF_JSAPIParam par;
	GF_SceneGraph *scene;
	char *data = NULL;
	XMLHTTPContext *ctx;
	SMJS_OBJ
	SMJS_ARGS

	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	if (ctx->readyState!=XHR_READYSTATE_OPENED) return JS_TRUE;
	if (ctx->sess) return JS_TRUE;

	scene = xml_get_scenegraph(c);
	par.dnld_man = NULL;
	ScriptAction(scene, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
	if (!par.dnld_man) return JS_TRUE;

	if (argc) {
		if (JSVAL_IS_NULL(argv[0])) {
		} else  if (JSVAL_IS_OBJECT(argv[0])) {
//			if (!GF_JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &documentClass, NULL) ) return JS_TRUE;

			/*NOT SUPPORTED YET, we must serialize the sg*/
			return JS_TRUE;
		} else {
			if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
			data = SMJS_CHARS(c, argv[0]);
		}
	}

	/*reset previous text*/
	xml_http_del_data(ctx);
	ctx->data = data ? gf_strdup(data) : NULL;
	SMJS_FREE(c, data);

	if (!strncmp(ctx->url, "http://", 7)) {

		ctx->sess = gf_dm_sess_new(par.dnld_man, ctx->url, ctx->async ? 0 : GF_NETIO_SESSION_NOT_THREADED, xml_http_on_data, ctx, &e);			
		if (!ctx->sess) return JS_TRUE;

		/*start our download (whether the session is threaded or not)*/
		e = gf_dm_sess_process(ctx->sess);
		if (e!=GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[XmlHttpRequest] Error processing %s: %s\n", ctx->url, gf_error_to_string(e) ));
		}
		/**/
		if (!ctx->async && ctx->sess) gf_dm_sess_del(ctx->sess);
	} else {
		e = xml_http_process_local(ctx);
		if (e!=GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[XmlHttpRequest] Error processing %s: %s\n", ctx->url, gf_error_to_string(e) ));
		}
	}

	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_http_abort)
{
	GF_DownloadSession *sess;
	XMLHTTPContext *ctx;
	SMJS_OBJ

	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	sess = ctx->sess;
	ctx->sess = NULL;
	if (sess) gf_dm_sess_del(sess);

	xml_http_reset(ctx);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_http_wait)
{
	XMLHTTPContext *ctx;
	SMJS_OBJ

	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	gf_sleep(2000);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_http_get_all_headers)
{
	u32 nb_hdr;
	char szVal[4096];
	XMLHTTPContext *ctx;
	SMJS_OBJ

	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	/*must be received or loaded*/
	if (ctx->readyState<XHR_READYSTATE_LOADING) return JS_TRUE;
	szVal[0] = 0;
	nb_hdr = 0;
	if (ctx->recv_headers) {
		while (ctx->recv_headers[nb_hdr]) {
			if (szVal[0]) strcat(szVal, "\r\n");
			strcat(szVal, ctx->recv_headers[nb_hdr]);
			strcat(szVal, ": ");
			strcat(szVal, ctx->recv_headers[nb_hdr+1]);
			nb_hdr+=2;
		}
	}
	if (!szVal[0]) {
		SMJS_SET_RVAL( JSVAL_VOID );
	} else {
		SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, szVal) ) );
	}

	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_http_get_header)
{
	u32 nb_hdr;
	char *hdr;
	char szVal[2048];
	XMLHTTPContext *ctx;
	SMJS_OBJ
	SMJS_ARGS
	if (!argc || !GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	/*must be received or loaded*/
	if (ctx->readyState<XHR_READYSTATE_LOADING) return JS_TRUE;
	hdr = SMJS_CHARS(c, argv[0]);

	szVal[0] = 0;
	nb_hdr = 0;
	if (ctx->recv_headers) {
		while (ctx->recv_headers[nb_hdr]) {
			if (!strcmp(ctx->recv_headers[nb_hdr], hdr)) {
				if (szVal[0]) strcat(szVal, ", ");
				strcat(szVal, ctx->recv_headers[nb_hdr+1]);
			}
			nb_hdr+=2;
		}
	}
	if (!szVal[0]) {
		SMJS_SET_RVAL( JSVAL_VOID );
	} else {
		SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, szVal)) );
	}
	SMJS_FREE(c, hdr);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(xml_http_overrideMimeType)
{
	char *mime;
	XMLHTTPContext *ctx;
	SMJS_OBJ
	SMJS_ARGS
	if (!argc || !GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	mime = SMJS_CHARS(c, argv[0]);
	ctx->mime = gf_strdup(mime);
	SMJS_FREE(c, mime);
	return JS_TRUE;
}

static SMJS_FUNC_PROP_GET( xml_http_getProperty)

	JSString *s;
	XMLHTTPContext *ctx;
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	s = NULL;
	if (SMJS_ID_IS_INT(id)) {
		switch (SMJS_ID_TO_INT(id)) {
		/*onreadystatechange*/
		case 0:
			if (ctx->onreadystatechange) {
				*vp = OBJECT_TO_JSVAL((JSObject*)ctx->onreadystatechange);
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
		/*readyState*/
		case 1:
			*vp = INT_TO_JSVAL(ctx->readyState); 
			return JS_TRUE;
		/*responseText*/
		case 2:
			if (ctx->readyState<XHR_READYSTATE_LOADING) return JS_TRUE;
			if (ctx->data) {
				s = JS_NewStringCopyZ(c, ctx->data);
				*vp = STRING_TO_JSVAL( s );
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
		/*responseXML*/
		case 3:
			if (ctx->readyState<XHR_READYSTATE_LOADING) return JS_TRUE;
			if (ctx->data && ctx->document) {
				*vp = dom_document_construct(c, ctx->document);
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
		/*response*/
		case 10:
			if (ctx->readyState<XHR_READYSTATE_LOADING) return JS_TRUE;
			if (ctx->data) {
				switch(ctx->responseType)
				{
				case XHR_RESPONSETYPE_NONE:
				case XHR_RESPONSETYPE_TEXT:
					s =	JS_NewStringCopyZ(c, ctx->data);
					*vp	= STRING_TO_JSVAL( s );
					break;
				case XHR_RESPONSETYPE_ARRAYBUFFER:
					if (!ctx->arraybuffer) {
						/* always return the same ArrayBuffer, is this correct? Probably not in chunked/loading mode */
						ctx->arraybuffer = gf_arraybuffer_js_new(c, ctx->data, ctx->size, obj);
					}
					*vp = OBJECT_TO_JSVAL( ctx->arraybuffer );
					break;
				case XHR_RESPONSETYPE_DOCUMENT:
					if (ctx->data && ctx->document) {
						*vp = dom_document_construct(c, ctx->document);
					} else {
						*vp = JSVAL_VOID;
					}
					break;
				default:
					/*other	types not supported	*/
					*vp = JSVAL_VOID;
				}
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
		/*status*/
		case 4:
			*vp = INT_TO_JSVAL(ctx->html_status); 
			return JS_TRUE;
		/*statusText*/
		case 5:
			if (ctx->statusText) {
				s = JS_NewStringCopyZ(c, ctx->statusText);
				*vp = STRING_TO_JSVAL( s );
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
        /*timeout*/
		case 6:
			*vp = INT_TO_JSVAL(ctx->timeout); 
			return JS_TRUE;
		/* withCredentials */
        case 7:
			*vp = BOOLEAN_TO_JSVAL(ctx->withCredentials ? JS_TRUE : JS_FALSE); 
			return JS_TRUE;
        /* upload */
		case 8:
			/* TODO */
			return JS_TRUE;
        /* responseType */
		case 9:
			switch (ctx->responseType)
            {
            case XHR_RESPONSETYPE_NONE:
				s = JS_NewStringCopyZ(c, "");
                break;
            case XHR_RESPONSETYPE_ARRAYBUFFER:
				s = JS_NewStringCopyZ(c, "arraybuffer");
                break;
            case XHR_RESPONSETYPE_BLOB:
				s = JS_NewStringCopyZ(c, "blob");
                break;
            case XHR_RESPONSETYPE_DOCUMENT:
				s = JS_NewStringCopyZ(c, "document");
                break;
            case XHR_RESPONSETYPE_JSON:
				s = JS_NewStringCopyZ(c, "json");
                break;
            case XHR_RESPONSETYPE_TEXT:
				s = JS_NewStringCopyZ(c, "text");
                break;
            case XHR_RESPONSETYPE_STREAM:
				s = JS_NewStringCopyZ(c, "stream");
                break;
            }
			*vp = STRING_TO_JSVAL( s );
			return JS_TRUE;
		case 100:
			*vp = INT_TO_JSVAL(0);
			return JS_TRUE;
		case 101:
			*vp = INT_TO_JSVAL(1);
			return JS_TRUE;
		case 102:
			*vp = INT_TO_JSVAL(2);
			return JS_TRUE;
		case 103:
			*vp = INT_TO_JSVAL(3);
			return JS_TRUE;
		case 104:
			*vp = INT_TO_JSVAL(4);
			return JS_TRUE;
		default:
			return JS_TRUE;
		}
	}
	return SMJS_CALL_PROP_STUB();
}

static SMJS_FUNC_PROP_SET( xml_http_setProperty)

	XMLHTTPContext *ctx;
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)SMJS_GET_PRIVATE(c, obj);
	if (!ctx) return JS_TRUE;

	if (SMJS_ID_IS_INT(id)) {
		switch (SMJS_ID_TO_INT(id)) {
		/*onreadystatechange*/
		case 0:
		    if (ctx->onreadystatechange) gf_js_remove_root(c, &(ctx->onreadystatechange), GF_JSGC_VAL);
		    ctx->onreadystatechange = NULL;

		    if (JSVAL_IS_VOID(*vp)) {
			    return JS_TRUE;
		    }
		    else if (JSVAL_CHECK_STRING(*vp)) {
			    jsval fval;
			    char *callback = SMJS_CHARS(c, *vp);
			    if (! JS_LookupProperty(c, JS_GetGlobalObject(c), callback, &fval)) return JS_TRUE;
			    ctx->onreadystatechange = JS_ValueToFunction(c, fval);
			    SMJS_FREE(c, callback);
		    } else if (JSVAL_IS_OBJECT(*vp)) {
			    ctx->onreadystatechange = JS_ValueToFunction(c, *vp);
		    }
		    if (ctx->onreadystatechange) gf_js_add_root(c, &(ctx->onreadystatechange), GF_JSGC_VAL);
		    return JS_TRUE;
			break;
        /*timeout*/
		case 6:
			ctx->timeout = JSVAL_TO_INT(*vp);
			return JS_TRUE;
		/* withCredentials */
        case 7:
			ctx->withCredentials = (JSVAL_TO_BOOLEAN(*vp) == JS_TRUE ? GF_TRUE : GF_FALSE);
			return JS_TRUE;
        /* responseType */
		case 9:
            { 
                char *str = SMJS_CHARS(c, *vp);
	            if (!str) return JS_TRUE;
                if (!strcmp(str, "")) {
                    ctx->responseType = XHR_RESPONSETYPE_NONE;
                } else if (!strcmp(str, "arraybuffer")) {
                    ctx->responseType = XHR_RESPONSETYPE_ARRAYBUFFER;
                } else if (!strcmp(str, "blob")) {
                    ctx->responseType = XHR_RESPONSETYPE_BLOB;
                } else if (!strcmp(str, "document")) {
                    ctx->responseType = XHR_RESPONSETYPE_DOCUMENT;
                } else if (!strcmp(str, "json")) {
                    ctx->responseType = XHR_RESPONSETYPE_JSON;
                } else if (!strcmp(str, "text")) {
                    ctx->responseType = XHR_RESPONSETYPE_TEXT;
                } else if (!strcmp(str, "stream")) {
                    ctx->responseType = XHR_RESPONSETYPE_STREAM;
                }
                break;
			    return JS_TRUE;
            }
		/*all other properties are read-only*/
		default:
			return JS_TRUE;
		}
	}
	return JS_TRUE;
}


static SMJS_FUNC_PROP_GET(dcci_getProperty)

	GF_DOMFullAttribute *att;
	GF_ChildNodeItem *child;
	GF_DOMFullNode *n;
	char *value;
	JSString *s;
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->DCCIClass, NULL) ) return JS_TRUE;
	n = (GF_DOMFullNode*) dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if (SMJS_ID_IS_INT(id)) {
		switch (SMJS_ID_TO_INT(id)) {
		/*value*/
		case 0:
			child = n->children;
			while (child) {
				if (child->node && (child->node->sgprivate->tag==TAG_DOMText)) {
					GF_DOMText *txt = (GF_DOMText *)child->node;
					if (txt->type==GF_DOM_TEXT_REGULAR) {
						s = JS_NewStringCopyZ(c, txt->textContent);
						*vp = STRING_TO_JSVAL( s );
						return JS_TRUE;
					}
				}
				child = child->next;
			}
			*vp = JSVAL_NULL;
			return JS_TRUE;
		/*valueType*/
		case 1:
			value = "DOMString";
			att = (GF_DOMFullAttribute*) n->attributes;
			while (att) {
				if (att->name && !strcmp(att->name, "valueType") && att->data) {
					value = (char *)att->data;
					break;
				}
				att = (GF_DOMFullAttribute*) att->next;
			}
			s = JS_NewStringCopyZ(c, value);
			*vp = STRING_TO_JSVAL( s );
			return JS_TRUE;
		/*propertyType*/
		case 2:
			value = "DOMString";
			att = (GF_DOMFullAttribute*) n->attributes;
			while (att) {
				if (att->name && !strcmp(att->name, "propertyType") && att->data) {
					value = (char *)att->data;
					break;
				}
				att = (GF_DOMFullAttribute*) att->next;
			}
			s = JS_NewStringCopyZ(c, value);
			*vp = STRING_TO_JSVAL( s );
			return JS_TRUE;
		/*readOnly*/
		case 3:
			att = (GF_DOMFullAttribute*) n->attributes;
			while (att) {
				if (att->name && !strcmp(att->name, "readOnly") && att->data && !strcmp((const char *)att->data, "true")) {
					*vp = BOOLEAN_TO_JSVAL(JS_TRUE);
					return JS_TRUE;
				}
				att = (GF_DOMFullAttribute*) att->next;
			}
			*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
			return JS_TRUE;
		/*DCCIMetadataInterfaceType*/
		case 4:
			*vp = JSVAL_NULL;
			return JS_TRUE;
		/*DCCIMetadataInterface*/
		case 5:
			*vp = JSVAL_NULL;
			return JS_TRUE;
		/*version*/
		case 6:
			s = JS_NewStringCopyZ(c, "1.0");
			*vp = STRING_TO_JSVAL( s );
			return JS_TRUE;
		default:
			return JS_TRUE;
		}
	}
	return SMJS_CALL_PROP_STUB();
}

static SMJS_FUNC_PROP_SET( dcci_setProperty)

	GF_ChildNodeItem *child;
	GF_DOMFullNode *n;
	GF_DOMFullAttribute*att;
	GF_DOM_Event evt;
	char *str;
	Bool readonly;
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->DCCIClass, NULL) ) return JS_TRUE;
	n = (GF_DOMFullNode*) dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	str = SMJS_CHARS(c, *vp);
	if (!str) return JS_TRUE;

	if (SMJS_ID_IS_INT(id)) {
		switch (SMJS_ID_TO_INT(id)) {
		/*value*/
		case 0:
			readonly = GF_FALSE;
			att = (GF_DOMFullAttribute*) n->attributes;
			while (att) {
				if (att->name && !strcmp(att->name, "readOnly") && att->data && !strcmp((const char *)att->data, "true")) {
					readonly = GF_TRUE;
					break;
				}
				att = (GF_DOMFullAttribute*) att->next;
			}
			if (readonly) break;
			child = n->children;
			while (child) {
				if (child->node && (child->node->sgprivate->tag==TAG_DOMText)) {
					GF_DOMText *txt = (GF_DOMText *)child->node;
					if (txt->type==GF_DOM_TEXT_REGULAR) {
						if (txt->textContent) gf_free(txt->textContent);
						txt->textContent = gf_strdup(str);
						break;

					}
				}
				child = child->next;
			}
			if (!child)
				gf_dom_add_text_node((GF_Node*)n, gf_strdup(str) );

			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.type = GF_EVENT_DCCI_PROP_CHANGE;
			evt.bubbles = 1;
			evt.relatedNode = (GF_Node*)n;
			gf_dom_event_fire((GF_Node*)n, &evt);
			n->sgprivate->scenegraph->modified = GF_TRUE;
			break;
		/*propertyType*/
		case 2:
			break;

		/*all other properties are read-only*/
		default:
			break;
		}
	}
	SMJS_FREE(c, str);
	return JS_TRUE;
}

Bool dcci_prop_lookup(GF_DOMFullNode *n, char *ns, char *name, Bool deep, Bool first)
{
	Bool ok = GF_TRUE;
	GF_ChildNodeItem *child = n->children;
	/*ns mismatch*/
	if (strcmp(ns, "*") && n->ns && strcmp(ns, gf_sg_get_namespace(n->sgprivate->scenegraph, n->ns) ))
		ok = GF_FALSE;
	/*name mismatch*/
	if (strcmp(name, "*") && n->name && strcmp(name, n->name))
		ok = GF_FALSE;

	/*"Some DCCIProperty nodes may not have a value"*/
	if (ok) return GF_TRUE;

	/*not found*/
	if (!first && !deep) return GF_FALSE;

	while (child) {
		if (child->node && (child->node->sgprivate->tag==TAG_DOMFullNode)) {
			ok = dcci_prop_lookup((GF_DOMFullNode *)child->node, ns, name, deep, GF_FALSE);
			if (ok) return GF_TRUE;
		}
		child = child->next;
	}
	return GF_FALSE;
}

static JSBool SMJS_FUNCTION(dcci_has_property)
{
	GF_DOMFullNode *n;
	Bool deep;
	char *ns, *name;
	SMJS_OBJ
	SMJS_ARGS
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->DCCIClass, NULL) ) return JS_TRUE;
	n = (GF_DOMFullNode*) dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	if (argc!=3) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
	ns = SMJS_CHARS(c, argv[0]);
	name = SMJS_CHARS(c, argv[1]);

	deep = JSVAL_TO_BOOLEAN(argv[2]) ? GF_TRUE : GF_FALSE;
	deep = dcci_prop_lookup(n, ns, name, deep, GF_TRUE);
	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(deep ? JS_TRUE : JS_FALSE) );
	SMJS_FREE(c, ns);
	SMJS_FREE(c, name);
	return JS_TRUE;
}

void dcci_prop_collect(DOMNodeList *nl, GF_DOMFullNode *n, char *ns, char *name, Bool deep, Bool first)
{
	Bool ok = GF_TRUE;
	GF_ChildNodeItem *child = n->children;
	/*ns mismatch*/
	if (strcmp(ns, "*") && n->ns && strcmp(ns, gf_sg_get_namespace(n->sgprivate->scenegraph, n->ns) ))
		ok = GF_FALSE;
	/*name mismatch*/
	if (strcmp(name, "*") && n->name && strcmp(name, n->name))
		ok = GF_FALSE;

	/*"Some DCCIProperty nodes may not have a value"*/
	if (ok) {
		gf_node_register((GF_Node*)n, NULL);
		if (n->sgprivate->scenegraph->reference_count)
			n->sgprivate->scenegraph->reference_count++;
		gf_node_list_add_child(&nl->child, (GF_Node*)n);
	}

	/*not found*/
	if (!first && !deep) return;

	while (child) {
		if (child->node && (child->node->sgprivate->tag==TAG_DOMFullNode)) {
			dcci_prop_collect(nl, (GF_DOMFullNode *)child->node, ns, name, GF_TRUE, GF_FALSE);
		}
		child = child->next;
	}
}

static JSBool SMJS_FUNCTION(dcci_search_property)
{
	JSObject *new_obj;
	DOMNodeList *nl;
	GF_DOMFullNode *n;
	Bool deep;
	char *ns, *name;
	SMJS_OBJ
	SMJS_ARGS

	if (!GF_JS_InstanceOf(c, obj, &dom_rt->DCCIClass, NULL) ) return JS_TRUE;
	n = (GF_DOMFullNode*) dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	if (argc!=4) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
	ns = SMJS_CHARS(c, argv[0]);
	name = SMJS_CHARS(c, argv[1]);
	/*todo - DCCI prop filter*/
	deep = JSVAL_TO_BOOLEAN(argv[3]) ? GF_TRUE : GF_FALSE;

	GF_SAFEALLOC(nl, DOMNodeList);
	dcci_prop_collect(nl, n, ns, name, deep, GF_TRUE);
	new_obj = JS_NewObject(c, &dom_rt->domNodeListClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, new_obj, nl);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(new_obj) );
	SMJS_FREE(c, ns);
	SMJS_FREE(c, name);
	return JS_TRUE;
}

static SMJS_FUNC_PROP_GET( storage_getProperty)

	/*avoids gcc warning*/
	if (!id) id=0;
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->storageClass, NULL) ) return JS_TRUE;
	*vp = JSVAL_VOID;
	return JS_TRUE;
}

static SMJS_FUNC_PROP_SET_NOVP( storage_setProperty)

	/*avoids gcc warning*/
	if (!id) id=0;
	if (!GF_JS_InstanceOf(c, obj, &dom_rt->storageClass, NULL) ) return JS_TRUE;
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(storage_constructor)
{
	SMJS_OBJ_CONSTRUCTOR(&dom_rt->storageClass)

	if (!GF_JS_InstanceOf(c, obj, &dom_rt->storageClass, NULL) ) return JS_TRUE;
	return JS_TRUE;
}

static DECL_FINALIZE( storage_finalize)

	/*avoids GCC warning*/
	if (!c) c=NULL;
}

void dom_js_define_storage(JSContext *c, JSObject *parent_obj, const char *name)
{
    JS_DefineObject(c, parent_obj, name, &dom_rt->storageClass._class, 0, 0 );
}

void dom_js_load(GF_SceneGraph *scene, JSContext *c, JSObject *global)
{
	if (!dom_rt) {
		GF_SAFEALLOC(dom_rt, GF_DOMRuntime);
		dom_rt->handlers = gf_list_new();
		JS_SETUP_CLASS(dom_rt->domNodeClass, "Node", JSCLASS_HAS_PRIVATE, dom_node_getProperty, dom_node_setProperty, dom_node_finalize);
		JS_SETUP_CLASS(dom_rt->domDocumentClass, "Document", JSCLASS_HAS_PRIVATE, dom_document_getProperty, dom_document_setProperty, dom_document_finalize);
		/*Element uses the same destructor as node*/
		JS_SETUP_CLASS(dom_rt->domElementClass, "Element", JSCLASS_HAS_PRIVATE, dom_element_getProperty, JS_PropertyStub_forSetter, dom_node_finalize);
		/*Text uses the same destructor as node*/
		JS_SETUP_CLASS(dom_rt->domTextClass, "Text", JSCLASS_HAS_PRIVATE, dom_text_getProperty, dom_text_setProperty, dom_node_finalize);
		/*Event class*/
		JS_SETUP_CLASS(dom_rt->domEventClass , "Event", JSCLASS_HAS_PRIVATE, event_getProperty, JS_PropertyStub_forSetter, JS_FinalizeStub);


		JS_SETUP_CLASS(dom_rt->domNodeListClass, "NodeList", JSCLASS_HAS_PRIVATE, dom_nodelist_getProperty, dom_nodelist_setProperty, dom_nodelist_finalize);
		JS_SETUP_CLASS(dom_rt->xmlHTTPRequestClass, "XMLHttpRequest", JSCLASS_HAS_PRIVATE, xml_http_getProperty, xml_http_setProperty, xml_http_finalize);
		JS_SETUP_CLASS(dom_rt->storageClass, "Storage", JSCLASS_HAS_PRIVATE, storage_getProperty, storage_setProperty, storage_finalize);

		JS_SETUP_CLASS(dom_rt->DCCIClass, "DCCI", JSCLASS_HAS_PRIVATE, dcci_getProperty, dcci_setProperty, dom_node_finalize);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] dom run-time allocated\n"));
	}
	dom_rt->nb_inst++;

	define_dom_exception(c, global);

	{
		JSFunctionSpec nodeFuncs[] = {
			SMJS_FUNCTION_SPEC("insertBefore",			xml_node_insert_before, 2),
			SMJS_FUNCTION_SPEC("replaceChild",			xml_node_replace_child, 2),
			SMJS_FUNCTION_SPEC("removeChild",				xml_node_remove_child, 1),
			SMJS_FUNCTION_SPEC("appendChild",				xml_node_append_child, 1),
			SMJS_FUNCTION_SPEC("hasChildNodes",			xml_node_has_children, 0),
			SMJS_FUNCTION_SPEC("cloneNode",				xml_clone_node, 1),
			SMJS_FUNCTION_SPEC("normalize",				xml_dom3_not_implemented, 0),
			SMJS_FUNCTION_SPEC("isSupported",				xml_dom3_not_implemented, 2),
			SMJS_FUNCTION_SPEC("hasAttributes",			xml_node_has_attributes, 0),
			SMJS_FUNCTION_SPEC("compareDocumentPosition", xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("isSameNode",				xml_node_is_same_node, 1),
			SMJS_FUNCTION_SPEC("lookupPrefix",			xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("isDefaultNamespace",		xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("lookupNamespaceURI",		xml_dom3_not_implemented, 1),
			/*we don't support full node compare*/
			SMJS_FUNCTION_SPEC("isEqualNode",				xml_node_is_same_node, 1),
			SMJS_FUNCTION_SPEC("getFeature",				xml_dom3_not_implemented, 2),
			SMJS_FUNCTION_SPEC("setUserData",				xml_dom3_not_implemented, 3),
			SMJS_FUNCTION_SPEC("getUserData",				xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JSPropertySpec nodeProps[] = {
			SMJS_PROPERTY_SPEC("nodeName",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("nodeValue",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("nodeType",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("parentNode",		3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("childNodes",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("firstChild",		5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("lastChild",		6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("previousSibling",	7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("nextSibling",		8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("attributes",		9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("ownerDocument",	10,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("namespaceURI",	11,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("prefix",			12,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("localName",		13,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("baseURI",			14,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("textContent",		15,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			/*elementTraversal interface*/
			SMJS_PROPERTY_SPEC("firstElementChild",		16,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("lastElementChild",		17,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("previousElementSibling",	18,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("nextElementSibling",		19,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),

			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		GF_JS_InitClass(c, global, 0, &dom_rt->domNodeClass, 0, 0, nodeProps, nodeFuncs, 0, 0);
		if (!dom_rt->domNodeClass._proto) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DOMCore] Not enough memory to initialize JS node class\n"));
			return;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] node class initialized\n"));

		JS_DefineProperty(c, dom_rt->domNodeClass._proto, "ELEMENT_NODE", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->domNodeClass._proto, "TEXT_NODE", INT_TO_JSVAL(3), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->domNodeClass._proto, "CDATA_SECTION_NODE", INT_TO_JSVAL(4), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->domNodeClass._proto, "DOCUMENT_NODE", INT_TO_JSVAL(9), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	}
	{
		JSFunctionSpec documentFuncs[] = {
			SMJS_FUNCTION_SPEC("createElement",				xml_document_create_element, 1),
			SMJS_FUNCTION_SPEC("createDocumentFragment",		xml_dom3_not_implemented, 0),
			SMJS_FUNCTION_SPEC("createTextNode",				xml_document_create_text, 1),
			SMJS_FUNCTION_SPEC("createComment",				xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("createCDATASection",			xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("createProcessingInstruction",	xml_dom3_not_implemented, 2),
			SMJS_FUNCTION_SPEC("createAttribute",				xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("createEntityReference",		xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("getElementsByTagName",		xml_document_elements_by_tag, 1),
			SMJS_FUNCTION_SPEC("importNode",					xml_dom3_not_implemented, 2),
			SMJS_FUNCTION_SPEC("createElementNS",				xml_document_create_element, 2),
			SMJS_FUNCTION_SPEC("createAttributeNS",			xml_dom3_not_implemented, 2),
			SMJS_FUNCTION_SPEC("getElementsByTagNameNS",		xml_document_elements_by_tag, 2),
			SMJS_FUNCTION_SPEC("getElementById",				xml_document_element_by_id, 1),
			SMJS_FUNCTION_SPEC("adoptNode",					xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("normalizeDocument",			xml_dom3_not_implemented, 0),
			SMJS_FUNCTION_SPEC("renameNode",					xml_dom3_not_implemented, 3),
			/*eventTarget interface*/
			JS_DOM3_EVENT_TARGET_INTERFACE
			/*DocumentEvent interface*/
			SMJS_FUNCTION_SPEC("createEvent",					xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};

		JSPropertySpec documentProps[] = {
			SMJS_PROPERTY_SPEC("doctype",				1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("implementation",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("documentElement",		3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("inputEncoding",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("xmlEncoding",			5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("xmlStandalone",		6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("xmlVersion",			7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("strictErrorChecking",	8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("documentURI",			9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("domConfig",			10,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0),
		};

		GF_JS_InitClass(c, global, dom_rt->domNodeClass._proto, &dom_rt->domDocumentClass, 0, 0, documentProps, documentFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] document class initialized\n"));
	}

	{
		JSFunctionSpec elementFuncs[] = {
			SMJS_FUNCTION_SPEC("getAttribute",				xml_element_get_attribute, 1),
			SMJS_FUNCTION_SPEC("setAttribute",				xml_element_set_attribute, 2),
			SMJS_FUNCTION_SPEC("removeAttribute",				xml_element_remove_attribute, 1),
			SMJS_FUNCTION_SPEC("getAttributeNS",				xml_element_get_attribute, 2),
			SMJS_FUNCTION_SPEC("setAttributeNS",				xml_element_set_attribute, 3),
			SMJS_FUNCTION_SPEC("removeAttributeNS",			xml_element_remove_attribute, 2),
			SMJS_FUNCTION_SPEC("hasAttribute",				xml_element_has_attribute, 1),
			SMJS_FUNCTION_SPEC("hasAttributeNS",				xml_element_has_attribute, 2),
			SMJS_FUNCTION_SPEC("getElementsByTagName",		xml_element_elements_by_tag, 1),
			SMJS_FUNCTION_SPEC("getElementsByTagNameNS",		xml_element_elements_by_tag, 2),
			SMJS_FUNCTION_SPEC("setIdAttribute",				xml_element_set_id, 2),
			SMJS_FUNCTION_SPEC("setIdAttributeNS",			xml_element_set_id, 3),
			SMJS_FUNCTION_SPEC("getAttributeNode",			xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("setAttributeNode",			xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("removeAttributeNode",			xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("getAttributeNodeNS",			xml_dom3_not_implemented, 2),
			SMJS_FUNCTION_SPEC("setAttributeNodeNS",			xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("setIdAttributeNode",			xml_dom3_not_implemented, 2),
			/*eventTarget interface*/
			JS_DOM3_EVENT_TARGET_INTERFACE
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};

		JSPropertySpec elementProps[] = {
			SMJS_PROPERTY_SPEC("tagName",				1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("schemaTypeInfo",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0),
		};
		GF_JS_InitClass(c, global, dom_rt->domNodeClass._proto, &dom_rt->domElementClass, 0, 0, elementProps, elementFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] element class initialized\n"));
	}

	{
		JSFunctionSpec textFuncs[] = {
#if 0
			SMJS_FUNCTION_SPEC("substringData",			xml_dom3_not_implemented, 2),
			SMJS_FUNCTION_SPEC("appendData",				xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("insertData",				xml_dom3_not_implemented, 2),
			SMJS_FUNCTION_SPEC("deleteData",				xml_dom3_not_implemented, 2),
			SMJS_FUNCTION_SPEC("replaceData",				xml_dom3_not_implemented, 3),
			/*text*/
			SMJS_FUNCTION_SPEC("splitText",				xml_dom3_not_implemented, 1),
			SMJS_FUNCTION_SPEC("replaceWholeText",		xml_dom3_not_implemented, 1),
#endif
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JSPropertySpec textProps[] = {
			SMJS_PROPERTY_SPEC("data",						1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("length",						2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			/*text*/
			SMJS_PROPERTY_SPEC("isElementContentWhitespace",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("wholeText",					4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0),
		};
		GF_JS_InitClass(c, global, dom_rt->domNodeClass._proto, &dom_rt->domTextClass, 0, 0, textProps, textFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] text class initialized\n"));
	}

	/*event API*/
	{
		JSFunctionSpec eventFuncs[] = {
			SMJS_FUNCTION_SPEC("stopPropagation", event_stop_propagation, 0),
			SMJS_FUNCTION_SPEC("stopImmediatePropagation", event_stop_immediate_propagation, 0),
			SMJS_FUNCTION_SPEC("preventDefault", event_prevent_default, 0),
#if 0
			SMJS_FUNCTION_SPEC("initEvent", event_prevent_default, 3),
			SMJS_FUNCTION_SPEC("initEventNS", event_prevent_default, 4),
#endif
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JSPropertySpec eventProps[] = {
			SMJS_PROPERTY_SPEC("type",			 0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("target",			 1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("currentTarget",	 2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("eventPhase",		 3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("bubbles",			 4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("cancelable",		 5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("timeStamp",		 6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("namespaceURI",	 7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("defaultPrevented", 8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),

			/*UIEvent*/
			SMJS_PROPERTY_SPEC("detail",			20,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			/*text, connectionEvent*/
			SMJS_PROPERTY_SPEC("data",			25,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			/*MouseEvent*/
			SMJS_PROPERTY_SPEC("screenX",			30,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("screenY",			31,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("clientX",			32,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("clientY",			33,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("button",			34,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("relatedTarget",	35,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			/*wheelEvent*/
			SMJS_PROPERTY_SPEC("wheelDelta",		36,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),

			/*keyboard*/
			SMJS_PROPERTY_SPEC("keyIdentifier",	40,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("keyChar",			41,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("charCode",		42,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),

			/*progress*/
			SMJS_PROPERTY_SPEC("lengthComputable",50,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("typeArg",			51,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("loaded",			52,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("total",			53,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("bufferLevelValid",	54,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("bufferLevel",			55,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("bufferRemainingTime",	56,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("status",				57,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),

			/*used by vrml*/
			SMJS_PROPERTY_SPEC("width",			60,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("height",			61,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("offset_x",		62,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("offset_y",		63,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("vp_width",		64,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("vp_height",		65,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("translation_x",	66,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("translation_y",	67,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("type3d",			68,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("error",			69,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),

			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0),
		};
		GF_JS_InitClass(c, global, 0, &dom_rt->domEventClass, 0, 0, eventProps, eventFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] Event class initialized\n"));

		JS_DefineProperty(c, dom_rt->domEventClass._proto, "CAPTURING_PHASE", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->domEventClass._proto, "AT_TARGET", INT_TO_JSVAL(2), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->domEventClass._proto, "BUBBLING_PHASE", INT_TO_JSVAL(3), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

		JS_DefineProperty(c, dom_rt->domEventClass._proto, "DOM_KEY_LOCATION_STANDARD ", INT_TO_JSVAL(0), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->domEventClass._proto, "DOM_KEY_LOCATION_LEFT", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->domEventClass._proto, "DOM_KEY_LOCATION_RIGHT", INT_TO_JSVAL(2), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->domEventClass._proto, "DOM_KEY_LOCATION_NUMPAD", INT_TO_JSVAL(3), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	}


	{
		JSFunctionSpec nodeListFuncs[] = {
			SMJS_FUNCTION_SPEC("item", dom_nodelist_item, 1),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		JSPropertySpec nodeListProps[] = {
			SMJS_PROPERTY_SPEC("length",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		GF_JS_InitClass(c, global, 0, &dom_rt->domNodeListClass, 0, 0, nodeListProps, nodeListFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] nodeList class initialized\n"));
	}

	{
		JSPropertySpec xmlHTTPRequestClassProps[] = {
			SMJS_PROPERTY_SPEC("onreadystatechange",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
			SMJS_PROPERTY_SPEC("readyState",			1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("responseText",			2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("responseXML",			3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("status",				4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("statusText",			5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("timeout",				6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0),
			SMJS_PROPERTY_SPEC("withCredentials",		7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0),
			SMJS_PROPERTY_SPEC("upload",				8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("responseType",			9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED , 0, 0),
			SMJS_PROPERTY_SPEC("response",				10,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("UNSENT",			100,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("OPENED",			101,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("HEADERS_RECEIVED",	102,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("LOADING",			103,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC("DONE",				104,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSPropertySpec xmlHTTPRequestStaticClassProps[] = {
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec xmlHTTPRequestClassFuncs[] = {
			SMJS_FUNCTION_SPEC("open",					xml_http_open, 2),
			SMJS_FUNCTION_SPEC("setRequestHeader",		xml_http_set_header, 2),
			SMJS_FUNCTION_SPEC("send",					xml_http_send, 0),
			SMJS_FUNCTION_SPEC("abort",					xml_http_abort, 0),
			SMJS_FUNCTION_SPEC("wait",					xml_http_wait, 0),
			SMJS_FUNCTION_SPEC("getAllResponseHeaders",	xml_http_get_all_headers, 0),
			SMJS_FUNCTION_SPEC("getResponseHeader",		xml_http_get_header, 1),
			SMJS_FUNCTION_SPEC("overrideMimeType",		xml_http_overrideMimeType, 1),
			/*todo - addEventListener and removeEventListener*/
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		GF_JS_InitClass(c, global, 0, &dom_rt->xmlHTTPRequestClass, xml_http_constructor, 0, xmlHTTPRequestClassProps, xmlHTTPRequestClassFuncs, xmlHTTPRequestStaticClassProps, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] XMLHttpRequest class initialized\n"));
	}

	{
		JSPropertySpec storageClassProps[] = {
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec storageClassFuncs[] = {
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};
		GF_JS_InitClass(c, global, 0, &dom_rt->storageClass, storage_constructor, 0, storageClassProps, storageClassFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] Storage class initialized\n"));
	}

	{
		GF_SceneGraph *dcci;
		jsval dcci_root;

		dcci = NULL;
		if (scene->script_action) {
			GF_JSAPIParam par;
			par.node = NULL;
			scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_GET_DCCI, NULL, &par);
			dcci = par.scene;
		}
		if (dcci && dcci->RootNode) {

			JSPropertySpec DCCIClassProps[] = {
				SMJS_PROPERTY_SPEC("value",							0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
				SMJS_PROPERTY_SPEC("valueType",						1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
				SMJS_PROPERTY_SPEC("propertyType",					2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED, 0, 0),
				SMJS_PROPERTY_SPEC("readOnly",						3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
				SMJS_PROPERTY_SPEC("DCCIMetadataInterfaceType",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
				SMJS_PROPERTY_SPEC("DCCIMetadataInterface",			5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
				SMJS_PROPERTY_SPEC("version",							6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_READONLY, 0, 0),
				SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
			};
			JSFunctionSpec DCCIClassFuncs[] = {
				SMJS_FUNCTION_SPEC("hasProperty",			dcci_has_property, 3),
				SMJS_FUNCTION_SPEC("searchProperty",		dcci_search_property, 4),
				SMJS_FUNCTION_SPEC(0, 0, 0)
			};

			GF_JS_InitClass(c, global, dom_rt->domElementClass._proto, &dom_rt->DCCIClass, 0, 0, DCCIClassProps, DCCIClassFuncs, 0, 0);
			dcci_root = dom_base_node_construct(c, &dom_rt->DCCIClass, dcci->RootNode);
			JS_DefineProperty(c, global, "DCCIRoot", dcci_root, 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );

			dcci->dcci_doc = GF_TRUE;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] DCCI class initialized\n"));
		}
	}
}

void html_media_element_js_finalize(JSContext *c, GF_Node *n);

void dom_js_pre_destroy(JSContext *c, GF_SceneGraph *sg, GF_Node *n)
{
	u32 i, count;
	if (n) {
        if (n->sgprivate->tag == TAG_SVG_video || n->sgprivate->tag == TAG_SVG_audio) {
            html_media_element_js_finalize(c, n);
        }
		if (n->sgprivate->interact && n->sgprivate->interact->js_binding && n->sgprivate->interact->js_binding->node) {
			JSObject *obj = (JSObject *)n->sgprivate->interact->js_binding->node;
			SMJS_SET_PRIVATE(c, obj, NULL);
			n->sgprivate->interact->js_binding->node=NULL;
			if (gf_list_del_item(sg->objects, obj)>=0) {
				gf_js_remove_root(c, &(n->sgprivate->interact->js_binding->node), GF_JSGC_OBJECT);
			}
		}
		return;
	}

	/*force cleanup of scripts/handlers not destroyed - this usually happens when a script/handler node has been created in a script
	but not inserted in the graph*/
	while (gf_list_count(sg->objects)) {
		GF_Node *n;
		JSObject *obj = (JSObject *)gf_list_get(sg->objects, 0);
		n = dom_get_node(c, obj);
		if (n) {
			if (n->sgprivate->tag == TAG_SVG_video || n->sgprivate->tag == TAG_SVG_audio) {
				html_media_element_js_finalize(c, n);
			}
			SMJS_SET_PRIVATE(c, obj, NULL);
			n->sgprivate->interact->js_binding->node=NULL;
			gf_node_unregister(n, NULL);
			gf_js_remove_root(c, &(n->sgprivate->interact->js_binding->node), GF_JSGC_OBJECT);
		}
		gf_list_rem(sg->objects, 0);
	}

	count = gf_list_count(dom_rt->handlers);
	for (i=0; i<count; i++) {
		SVG_handlerElement *handler = (SVG_handlerElement *)gf_list_get(dom_rt->handlers, i);
		/*if same context and same document, discard handler*/
		if ( (handler->js_context==c) && (!sg || (handler->sgprivate->scenegraph==sg)) ) {
			/*unprotect the function*/
			gf_js_remove_root((JSContext *)handler->js_context, &(handler->js_fun_val), GF_JSGC_VAL);
			handler->js_fun_val=0;
			handler->js_context=0;
			gf_list_rem(dom_rt->handlers, i);
			i--;
			count--;
		}
	}
	if (sg->document) {
#ifdef USE_FFDEV_15
		dom_document_finalize(NULL, sg->document);
#else
		dom_document_finalize(c, sg->document);
#endif
	}
}

void dom_js_unload()
{
	if (!dom_rt) return;
	dom_rt->nb_inst--;
	if (!dom_rt->nb_inst) {
		gf_list_del(dom_rt->handlers);
		gf_free(dom_rt);
		dom_rt = NULL;
	}
}

static void dom_js_define_document_ex(JSContext *c, JSObject *global, GF_SceneGraph *doc, const char *name)
{
	GF_JSClass *__class;
	JSObject *obj;
	if (!doc || !doc->RootNode) return;

	if (doc->reference_count)
		doc->reference_count++;

	__class = NULL;
	if (dom_rt->get_document_class)
		__class = (GF_JSClass *)dom_rt->get_document_class(doc);
	if (!__class) __class = &dom_rt->domDocumentClass;

	obj = JS_DefineObject(c, global, name, & __class->_class, 0, 0 );
	gf_node_register(doc->RootNode, NULL);
	SMJS_SET_PRIVATE(c, obj, doc);
	doc->document = obj;
}

void dom_js_define_document(JSContext *c, JSObject *global, GF_SceneGraph *doc)
{
	dom_js_define_document_ex(c, global, doc, "document");
}

JSObject *dom_js_define_event(JSContext *c, JSObject *global)
{
	JSObject *obj = JS_DefineObject(c, global, "evt", &dom_rt->domEventClass._class, 0, 0 );
	//JS_AliasProperty(c, global, "evt", "event");
	return obj;
}

JSObject *gf_dom_new_event(JSContext *c)
{
	return JS_NewObject(c, &dom_rt->domEventClass._class, 0, 0);
}
JSObject *dom_js_get_node_proto(JSContext *c) { return dom_rt->domNodeClass._proto; }
JSObject *dom_js_get_element_proto(JSContext *c) { return dom_rt->domElementClass._proto; }
JSObject *dom_js_get_document_proto(JSContext *c) { return dom_rt->domDocumentClass._proto; }
JSObject *dom_js_get_event_proto(JSContext *c) { return dom_rt->domEventClass._proto; }

void dom_set_class_selector(JSContext *c, void *(*get_element_class)(GF_Node *n), void *(*get_document_class)(GF_SceneGraph *n) )
{
	if (dom_rt) {
		dom_rt->get_document_class = get_document_class;
		dom_rt->get_element_class = get_element_class;
	}
}

#endif	/*GPAC_HAS_SPIDERMONKEY*/



GF_Err gf_sg_new_from_xml_doc(const char *src, GF_SceneGraph **scene)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	GF_Err e;
	XMLHTTPContext *ctx;
	GF_SceneGraph *sg;

	GF_SAFEALLOC(ctx, XMLHTTPContext);
	ctx->sax = gf_xml_sax_new(xml_http_sax_start, xml_http_sax_end, xml_http_sax_text, ctx);
	ctx->node_stack = gf_list_new();
	sg = gf_sg_new();
	ctx->document = sg;
	e = gf_xml_sax_parse_file(ctx->sax, src, NULL);
	gf_xml_sax_del(ctx->sax);
	gf_list_del(ctx->node_stack);
	gf_free(ctx);

	*scene = NULL;
	if (e<0) {
		gf_sg_del(sg);
		return e;
	}
	*scene = sg;
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

#ifdef GPAC_HAS_SPIDERMONKEY

typedef struct
{
	GF_SAXParser *sax;
	GF_List *node_stack;
	/*dom graph*/
	GF_SceneGraph *document;
} XMLReloadContext;

static void xml_reload_node_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	GF_DOM_Event evt;
	Bool is_root = GF_FALSE;
	Bool modified = GF_FALSE;
	Bool atts_modified = GF_FALSE;
	Bool new_node = GF_FALSE;
	u32 i;
	GF_DOMFullAttribute *prev = NULL;
	GF_DOMFullNode *par = NULL;
	XMLReloadContext *ctx = (XMLReloadContext *)sax_cbck;
	GF_DOMFullNode *node;

	node = (GF_DOMFullNode *)gf_list_last(ctx->node_stack);
	if (!node) {
		is_root = GF_TRUE;
		node = (GF_DOMFullNode *) ctx->document->RootNode;
	}

	if (is_root) {
		Bool same_node = GF_TRUE;
		if (strcmp(node->name, node_name)) same_node = GF_FALSE;
		if (node->ns && name_space && strcmp(gf_sg_get_namespace(node->sgprivate->scenegraph, node->ns), name_space)) same_node = GF_FALSE;

		if (!same_node) {
			if (node->name) gf_free(node->name);
			node->name = gf_strdup(node_name);
			node->ns = 0;
			if (name_space) {
				gf_sg_add_namespace(node->sgprivate->scenegraph, (char *) name_space, NULL);
				node->ns = gf_sg_get_namespace_code_from_name(node->sgprivate->scenegraph, (char*)name_space);
			}
			modified = GF_TRUE;
		}
	} else {
		GF_ChildNodeItem *child = node->children;
		node = NULL;
		/*locate the node in the children*/
		while (child) {
			Bool same_node = GF_TRUE;
			node = (GF_DOMFullNode*)child->node;
			if (strcmp(node->name, node_name)) same_node = GF_FALSE;
			if (node->ns && name_space && strcmp(gf_sg_get_namespace(node->sgprivate->scenegraph, node->ns), name_space)) same_node = GF_FALSE;
			if (same_node) {
				break;
			}
			node = NULL;
			child = child->next;
		}
		if (!node) {
			modified = GF_TRUE;

			/*create the new node*/
			node = (GF_DOMFullNode *) gf_node_new(ctx->document, TAG_DOMFullNode);
			node->name = gf_strdup(node_name);
			if (name_space) {
				gf_sg_add_namespace(node->sgprivate->scenegraph, (char *)name_space, NULL);
				node->ns = gf_sg_get_namespace_code(node->sgprivate->scenegraph, (char *)name_space);
			}

			par = (GF_DOMFullNode *)gf_list_last(ctx->node_stack);
			gf_node_register((GF_Node*)node, (GF_Node*)par);
			gf_node_list_add_child(&par->children, (GF_Node*)node);
			new_node = GF_TRUE;
		}
	}
	gf_list_add(ctx->node_stack, node);

	if (!modified) {
		u32 count = 0;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute *)node->attributes;
		while (att) {
			Bool found = GF_FALSE;
			for (i=0; i<nb_attributes; i++) {
				if (!stricmp(attributes[i].name, "xml:id")) {
					const char *id = gf_node_get_name((GF_Node*)node);
					if (!id || strcmp(id, attributes[i].value))
						modified = GF_TRUE;
				} else if (!strcmp(att->name, attributes[i].name)) {
					found = GF_TRUE;
					if (strcmp((const char *)att->data, attributes[i].value)) {
						atts_modified = GF_TRUE;
						gf_free(att->data);
						att->data = gf_strdup(attributes[i].value);
					}
				}
			}
			if (!found) {
				modified = GF_TRUE;
				break;
			}
			count++;
			att = (GF_DOMFullAttribute *)att->next;
		}
		if (count != nb_attributes)
			modified = GF_TRUE;
	}


	if (modified) {
		GF_DOMFullAttribute *tmp, *att;
		att = (GF_DOMFullAttribute *)node->attributes;
		while (att) {
			if (att->name) gf_free(att->name);
			if (att->data) gf_free(att->data);
			tmp = att;
			att = (GF_DOMFullAttribute *)att->next;
			gf_free(tmp);
		}

		/*parse all atts*/
		for (i=0; i<nb_attributes; i++) {
			if (!stricmp(attributes[i].name, "xml:id")) {
				u32 id = gf_sg_get_max_node_id(ctx->document) + 1;
				gf_node_set_id((GF_Node *)node, id, attributes[i].value);
			} else {
				GF_DOMFullAttribute *att;
				GF_SAFEALLOC(att, GF_DOMFullAttribute);
				att->tag = TAG_DOM_ATT_any;
				att->name = gf_strdup(attributes[i].name);
				att->data = gf_strdup(attributes[i].value);
				if (prev) prev->next = (GF_DOMAttribute*)att;
				else node->attributes = (GF_DOMAttribute*)att;
				prev = att;
			}
		}
		atts_modified = GF_TRUE;
	}

	if (atts_modified || new_node) {
		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = new_node ? GF_EVENT_NODE_INSERTED : GF_EVENT_ATTR_MODIFIED;
		evt.bubbles = 1;
		evt.relatedNode = (GF_Node*) (GF_EVENT_NODE_INSERTED ? par : node);
		gf_dom_event_fire((GF_Node*)node, &evt);
	}
}

static void xml_reload_node_end(void *sax_cbck, const char *node_name, const char *name_space)
{
	XMLReloadContext *ctx = (XMLReloadContext *)sax_cbck;
	GF_DOMFullNode *par = (GF_DOMFullNode *)gf_list_last(ctx->node_stack);
	if (par) {
		/*depth mismatch*/
		if (strcmp(par->name, node_name)) return;
		gf_list_rem_last(ctx->node_stack);
	}
}
static void xml_reload_text_content(void *sax_cbck, const char *content, Bool is_cdata)
{
	GF_DOM_Event evt;
	u32 i, len;
	GF_ChildNodeItem *child;
	GF_DOMText *txt = NULL;
	XMLReloadContext *ctx = (XMLReloadContext *)sax_cbck;
	GF_DOMFullNode *par = (GF_DOMFullNode *)gf_list_last(ctx->node_stack);
	if (!par) return;

	/*basic check, remove all empty text nodes*/
	len = (u32) strlen(content);
	for (i=0; i<len; i++) {
		if (!strchr(" \n\r\t", content[i])) break;
	}
	if (i==len) return;

	child = par->children;
	while (child) {
		if (child->node->sgprivate->tag == TAG_DOMText) {
			txt = (GF_DOMText *)child->node;
			if (!strcmp(txt->textContent, content) && ((txt->type==GF_DOM_TEXT_REGULAR) || is_cdata))
				return;
			if (txt->textContent) gf_free(txt->textContent);
			txt->textContent = gf_strdup(content);
			txt->type = is_cdata ? GF_DOM_TEXT_CDATA : GF_DOM_TEXT_REGULAR;
			break;
		}
		child = child->next;
	}
	if (!txt) {
		txt = gf_dom_add_text_node((GF_Node *)par, gf_strdup(content) );
		txt->type = is_cdata ? GF_DOM_TEXT_CDATA : GF_DOM_TEXT_REGULAR;

		memset(&evt, 0, sizeof(GF_DOM_Event));
		evt.type = GF_EVENT_NODE_INSERTED;
		evt.bubbles = 1;
		evt.relatedNode = (GF_Node*) par;
		gf_dom_event_fire((GF_Node*)txt, &evt);
	}

	memset(&evt, 0, sizeof(GF_DOM_Event));
	evt.type = GF_EVENT_CHAR_DATA_MODIFIED;
	evt.bubbles = 1;
	evt.relatedNode = (GF_Node*)par;
	gf_dom_event_fire((GF_Node*)par, &evt);

	memset(&evt, 0, sizeof(GF_DOM_Event));
	evt.type = GF_EVENT_DCCI_PROP_CHANGE;
	evt.bubbles = 1;
	evt.relatedNode = (GF_Node*)par;
	gf_dom_event_fire((GF_Node*)par, &evt);
}
#endif

GF_Err gf_sg_reload_xml_doc(const char *src, GF_SceneGraph *scene)
{
#ifndef GPAC_HAS_SPIDERMONKEY
	return GF_NOT_SUPPORTED;
#else
	GF_Err e;
	XMLReloadContext ctx;

	if (!src || !scene) return GF_BAD_PARAM;
	memset(&ctx, 0, sizeof(XMLReloadContext));
	ctx.document = scene;
	ctx.node_stack = gf_list_new();

	ctx.sax = gf_xml_sax_new(xml_reload_node_start, xml_reload_node_end, xml_reload_text_content, &ctx);
	e = gf_xml_sax_parse_file(ctx.sax, src, NULL);
	gf_list_del(ctx.node_stack);
	gf_xml_sax_del(ctx.sax);
	if (e<0) return e;
	return GF_OK;
#endif
}

#endif	/*GPAC_DISABLE_SVG*/
