/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean le Feuvre
 *			Copyright (c) 2007-200X ENST
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

#include <jsapi.h> 

JSBool js_has_instance(JSContext *c, JSObject *obj, jsval val, JSBool *vp)
{
	*vp = JS_FALSE;
	if (JSVAL_IS_OBJECT(val)) {
		JSObject *p;
		JSClass *js_class = JS_GET_CLASS(c, obj);
		p = JSVAL_TO_OBJECT(val);
		if (JS_InstanceOf(c, p, js_class, NULL) ) *vp = JS_TRUE;
	}
	return JS_TRUE;
}

static GFINLINE Bool ScriptAction(GF_SceneGraph *scene, u32 type, GF_Node *node, GF_JSAPIParam *param)
{
	if (scene->script_action) 
		return scene->script_action(scene->script_action_cbck, type, node, param);
	return 0;
}

static GFINLINE GF_SceneGraph *xml_get_scenegraph(JSContext *c)
{
	GF_SceneGraph *scene;
	JSObject *global = JS_GetGlobalObject(c);
	assert(global);
	scene = JS_GetPrivate(c, global);
	assert(scene);
	return scene;
}

#define JSVAL_CHECK_STRING(_v) (JSVAL_IS_STRING(_v) || JSVAL_IS_NULL(_v))
#define JSVAL_GET_STRING(_v) (JSVAL_IS_NULL(_v) ? NULL : JS_GetStringBytes(JSVAL_TO_STRING(_v)) )

char *js_get_utf8(jsval val)
{
	jschar *utf16;
	char *txt;
	u32 len;
	if (!JSVAL_CHECK_STRING(val) || JSVAL_IS_NULL(val)) return NULL;

	utf16 = JS_GetStringChars(JSVAL_TO_STRING(val) );
	len = gf_utf8_wcslen(utf16)*2 + 1;
	txt = malloc(sizeof(char)*len);
	len = gf_utf8_wcstombs(txt, len, &utf16);
	if ((s32)len<0) {
		free(txt);
		return NULL;
	}
	txt[len]=0;
	return txt;
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
	JSClass domDocumentClass;
	JSClass domNodeClass;
	JSClass domElementClass;
	JSClass domTextClass;
	JSClass domNodeListClass;
	JSClass domEventClass;

	JSClass xmlHTTPRequestClass;
	JSClass DCCIClass;

	JSObject *dom_node_proto;
	JSObject *dom_document_proto;
	JSObject *dom_element_proto;
	JSObject *dom_event_proto;

	void *(*get_element_class)(GF_Node *n);
	void *(*get_document_class)(GF_SceneGraph *n);
} GF_DOMRuntime;

static GF_DOMRuntime *dom_rt = NULL;


static JSBool xml_dom3_not_implemented(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{	
	return JS_TRUE;
}


#define JS_DOM3_uDOM_FIRST_PROP		18

static void dom_node_changed(GF_Node *n, Bool child_modif, GF_FieldInfo *info)
{
	if (info) {
		if (child_modif) {
			gf_node_dirty_set(n, GF_SG_CHILD_DIRTY, 0);
		} 
#ifndef GPAC_DISABLE_SVG
		else {
			u32 flag = gf_svg_get_modification_flags((SVG_Element *)n, info);
			gf_node_dirty_set(n, flag, 0);
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
#undef  DEF_EXC

	JS_AliasProperty(c, global, "DOMException", "e");
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
	GF_Node *n = obj ? JS_GetPrivate(c, obj) : NULL;
	if (n && n->sgprivate) return n;
	return NULL;
}

GF_Node *dom_get_element(JSContext *c, JSObject *obj)
{
	GF_Node *n = JS_GetPrivate(c, obj);
	if (!n || !n->sgprivate) return NULL;
	if (n->sgprivate->tag==TAG_DOMText) return NULL;
	return n;
}

GF_SceneGraph *dom_get_doc(JSContext *c, JSObject *obj)
{
	GF_SceneGraph *sg = JS_GetPrivate(c, obj);
	if (sg && !sg->__reserved_null) return sg;
	return NULL;
}

static jsval dom_document_construct(JSContext *c, GF_SceneGraph *sg)
{
	JSClass *jsclass;
	JSObject *new_obj;
	if (sg->document) return OBJECT_TO_JSVAL(sg->document);

	if (sg->reference_count)
		sg->reference_count++;
	gf_node_register(sg->RootNode, NULL);

	jsclass = &dom_rt->domDocumentClass;
	if (dom_rt->get_document_class)
		jsclass = (JSClass *) dom_rt->get_document_class(sg);

	new_obj = JS_NewObject(c, jsclass, 0, 0);
	JS_SetPrivate(c, new_obj, sg);
	sg->document = new_obj;
	return OBJECT_TO_JSVAL(new_obj);
}
static jsval dom_base_node_construct(JSContext *c, JSClass *_class, GF_Node *n)
{
	GF_SceneGraph *sg;
	u32 i, count;
	JSObject *new_obj;
	if (!n || !n->sgprivate->scenegraph) return JSVAL_VOID;
	if (n->sgprivate->tag<TAG_DOMText) return JSVAL_VOID;

	sg = n->sgprivate->scenegraph;
	count = gf_list_count(sg->objects);
	for (i=0; i<count; i++) {
		JSObject *obj = gf_list_get(sg->objects, i);
		GF_Node *a_node = JS_GetPrivate(c, obj);
		if (n==a_node) return OBJECT_TO_JSVAL(obj);
	}

	if (n->sgprivate->scenegraph->reference_count)
		n->sgprivate->scenegraph->reference_count ++;

	gf_node_register(n, NULL);
	new_obj = JS_NewObject(c, _class, 0, 0);
	JS_SetPrivate(c, new_obj, n);
	gf_list_add(sg->objects, new_obj);
	return OBJECT_TO_JSVAL(new_obj);
}
static jsval dom_node_construct(JSContext *c, GF_Node *n)
{
	JSClass *__class = &dom_rt->domElementClass;
	if (!n) return JSVAL_NULL;
	if (n->sgprivate->scenegraph->dcci_doc)
		__class = &dom_rt->DCCIClass;
	else if (dom_rt->get_element_class)
		__class = (JSClass *) dom_rt->get_element_class(n);

	/*in our implementation ONLY ELEMENTS are created, never attributes. We therefore always
	create Elements when asked to create a node !!*/
	return dom_base_node_construct(c, __class, n);
}
jsval dom_element_construct(JSContext *c, GF_Node *n)
{
	JSClass *__class = &dom_rt->domElementClass;
	if (!n) return JSVAL_NULL;
	if (n->sgprivate->scenegraph->dcci_doc)
		__class = &dom_rt->DCCIClass;
	else if (dom_rt->get_element_class)
		__class = (JSClass *) dom_rt->get_element_class(n);

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

char *dom_node_flatten_text(GF_Node *n)
{
	u32 len = 0;
	char *res = NULL;
	GF_ChildNodeItem *list;

	if ((n->sgprivate->tag==TAG_DOMText) && ((GF_DOMText*)n)->textContent) {
		if ( ((GF_DOMText*)n)->type == GF_DOM_TEXT_REGULAR) {
			res = strdup(((GF_DOMText*)n)->textContent);
			len = strlen(res);
		}
	}
	
	list = ((GF_ParentNode *)n)->children;
	while (list) {
		char *t = dom_node_flatten_text(list->node);
		if (t) {
			u32 sub_len = strlen(t);
			res = realloc(res, sizeof(char)*(len+sub_len+1));
			if (!len) res[0] = 0;
			len += sub_len;
			strcat(res, t);
			free(t);
		}
		list = list->next;
	}
	return res;
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
	new_obj = JS_NewObject(c, &dom_rt->domNodeListClass, 0, 0);
	JS_SetPrivate(c, new_obj, nl);
	return OBJECT_TO_JSVAL(new_obj);
}

static void dom_nodelist_finalize(JSContext *c, JSObject *obj)
{
	DOMNodeList *nl;
	if (!JS_InstanceOf(c, obj, &dom_rt->domNodeListClass, NULL) )
		return;

	nl = (DOMNodeList *) JS_GetPrivate(c, obj);
	if (!nl) return;

	if (nl->owner) {
		dom_unregister_node((GF_Node*)nl->owner);
	} else {
		/*unregister all nodes for created lists*/
		while (nl->child) {
			GF_ChildNodeItem *child = nl->child;
			nl->child = child->next;
			dom_unregister_node(child->node);
			free(child);
		}
	}
	free(nl);
}

static JSBool dom_nodelist_item(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n;
	u32 idx, count;
	DOMNodeList *nl;
	if (!JS_InstanceOf(c, obj, &dom_rt->domNodeListClass, NULL) ) 
		return JS_TRUE;
	if ((argc!=1) || !JSVAL_IS_INT(argv[0])) 
		return JS_TRUE;

	nl = (DOMNodeList *)JS_GetPrivate(c, obj);
	count = gf_node_list_get_count(nl->owner ? nl->owner->children : nl->child);
	idx = JSVAL_TO_INT(argv[0]);
	if ((idx<0) || (idx>=count)) {
		*rval = JSVAL_VOID;
		return JS_TRUE;
	}
	n = gf_node_list_get_child(nl->owner ? nl->owner->children : nl->child, idx);
	*rval = dom_node_construct(c, n);
	return JS_TRUE;
}



static JSBool dom_nodelist_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	DOMNodeList *nl;
	if (!JS_InstanceOf(c, obj, &dom_rt->domNodeListClass, NULL) 
	)
		return JS_TRUE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case 0:
		nl = (DOMNodeList *) JS_GetPrivate(c, obj);
		*vp = INT_TO_JSVAL( gf_node_list_get_count(nl->owner ? nl->owner->children : nl->child) );
		return JS_TRUE;
	}
	return JS_TRUE;
}
static JSBool dom_nodelist_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	/*no write prop*/
	return JS_TRUE;
}


/*dom event listener*/

#define JS_DOM3_EVEN_TARGET_INTERFACE	\
	{"addEventListenerNS", dom_event_add_listener, 4},	\
	{"removeEventListenerNS", dom_event_remove_listener, 4},	\
	{"addEventListener", dom_event_add_listener, 3},		\
	{"removeEventListener", dom_event_remove_listener, 3},	\
	{"dispatchEvent", xml_dom3_not_implemented, 1},

/*eventListeners routines used by document, element and connection interfaces*/
JSBool dom_event_add_listener_ex(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, GF_Node *vrml_node)
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
	jsval funval = 0;
	JSObject *evt_handler;

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
		if (vrml_node) {
			n = vrml_node;
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
		inNS = js_get_utf8(argv[0]);
		of = 1;
	}
	evt_handler = obj;

	if (!JSVAL_CHECK_STRING(argv[of])) goto err_exit;
	type = JSVAL_GET_STRING(argv[of]);
	callback = NULL;

	if (JSVAL_CHECK_STRING(argv[of+1])) {
		callback = JSVAL_GET_STRING(argv[of+1]);
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
			handler->js_fun_val = funval;
			handler->evt_listen_obj = evt_handler;
		}
	}

	/*create attributes if needed*/
	gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_event, 1, 0, &info);
	((XMLEV_Event*)info.far_ptr)->type = evtType;
	gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_handler, 1, 0, &info);
	((XMLRI*)info.far_ptr)->target = (GF_Node*)handler;
	if (n) {
		gf_node_get_attribute_by_tag(listener, TAG_XMLEV_ATT_target, 1, 0, &info);
		((XMLRI*)info.far_ptr)->target = n;
	}

	gf_node_get_attribute_by_tag((GF_Node*)handler, TAG_XMLEV_ATT_event, 1, 0, &info);
	((XMLEV_Event*)info.far_ptr)->type = evtType;

	if (callback) gf_dom_add_text_node((GF_Node *)handler, strdup(callback));

#ifndef GPAC_DISABLE_SVG
	if (handler->sgprivate->scenegraph->svg_js)
		handler->handle_event = gf_sg_handle_dom_event;
#endif
	
	if (!handler->handle_event) {
		handler->handle_event = gf_sg_handle_dom_event_for_vrml;
		handler->js_context = c;
	}

	/*don't add listener directly, post it and wait for event processing*/
	if (n) {
		gf_dom_listener_post_add((GF_Node *) n, listener);
	} else {
		gf_dom_listener_add(listener, target);
	}

err_exit:
	if (inNS) free(inNS);
	return JS_TRUE;
}
JSBool dom_event_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return dom_event_add_listener_ex(c, obj, argc, argv, rval, NULL);
}

JSBool dom_event_remove_listener_ex(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, GF_Node *vrml_node)
{
#ifndef GPAC_DISABLE_SVG
	char *type, *callback;
	u32 of = 0;
	u32 evtType, i, count;
	char *inNS = NULL;
	GF_Node *node = NULL;
	jsval funval = 0;
	GF_SceneGraph *sg = NULL;
	GF_DOMEventTarget *target = NULL;

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
		inNS = js_get_utf8(argv[0]);
		of = 1;
	}

	if (!JSVAL_CHECK_STRING(argv[of])) goto err_exit;
	type = JSVAL_GET_STRING(argv[of]);
	callback = NULL;
	if (JSVAL_CHECK_STRING(argv[of+1])) {
		callback = JSVAL_GET_STRING(argv[of+1]);
	} else if (JSVAL_IS_OBJECT(argv[of+1])) {
		if (JS_ObjectIsFunction(c, JSVAL_TO_OBJECT(argv[of+1]) )) {
			funval = argv[of+1];
		}
	}
	if (!callback && !funval) goto err_exit;

	evtType = gf_dom_event_type_by_name(type);
	if (evtType==GF_EVENT_UNKNOWN) goto err_exit;
	
	count = gf_list_count(target->evt_list);
	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		GF_DOMText *txt;
		SVG_handlerElement *hdl;
		GF_Node *el = gf_list_get(target->evt_list, i);

		gf_node_get_attribute_by_tag(el, TAG_XMLEV_ATT_event, 0, 0, &info);
		if (!info.far_ptr) continue;
		if (((XMLEV_Event*)info.far_ptr)->type != evtType) continue;

		gf_node_get_attribute_by_tag(el, TAG_XMLEV_ATT_handler, 0, 0, &info);
		if (!info.far_ptr) continue;
		hdl = (SVG_handlerElement *) ((XMLRI*)info.far_ptr)->target;
		if (!hdl) continue;
		if (funval) {
			if (funval != hdl->js_fun_val) continue;
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
	if (inNS) free(inNS);
	return JS_TRUE;
}

JSBool dom_event_remove_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return dom_event_remove_listener_ex(c, obj, argc, argv, rval, NULL);
}

/*dom3 node*/
static void dom_node_finalize(JSContext *c, JSObject *obj)
{
	GF_Node *n = (GF_Node *) JS_GetPrivate(c, obj);
	/*the JS proto of the svgClass or a destroyed object*/
	if (!n) return;
	if (!n->sgprivate) return;
	gf_list_del_item(n->sgprivate->scenegraph->objects, obj);
	dom_unregister_node(n);
}

static Bool check_dom_parents(JSContext *c, GF_Node *n, GF_Node *parent)
{
	GF_ParentList *par = n->sgprivate->parents;
	if (n->sgprivate->scenegraph != parent->sgprivate->scenegraph) {
		dom_throw_exception(c, GF_DOM_EXC_WRONG_DOCUMENT_ERR);
		return 0;
	}
	while (par) {
		if (par->node==parent) {
			dom_throw_exception(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
			return 0;
		}
		if (!check_dom_parents(c, par->node, parent))
			return 0;
		par = par->next;
	}
	return 1;
}

static void dom_node_inserted(GF_Node *par, GF_Node *n, s32 pos)
{
	GF_ParentNode *old_par;
	Bool do_init = 0;

	/*if node is already in graph, remove it from its parent*/
	old_par = (GF_ParentNode*)gf_node_get_parent(n, 0);
	if (old_par) {
		/*prevent destruction when removing node*/
		n->sgprivate->num_instances++;
		gf_node_list_del_child(&old_par->children, n);
		gf_node_unregister(n, (GF_Node*)old_par);
		n->sgprivate->num_instances--;
	} else {
		do_init = (n->sgprivate->UserCallback || n->sgprivate->UserPrivate) ? 0 : 1;
	}

	if (pos<0) gf_node_list_add_child( & ((GF_ParentNode *)par)->children, n);
	else gf_node_list_insert_child( & ((GF_ParentNode *)par)->children, n, (u32) pos);
	gf_node_register(n, par);

	if (do_init) {
		/*node is a handler, create listener*/
		if (par && (n->sgprivate->tag==TAG_SVG_handler)) {
			gf_dom_listener_build_ex(par, 0, 0, n, NULL);
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
	if (!old_par) gf_node_activate(n);

	dom_node_changed(par, 1, NULL);
}

static JSBool xml_node_insert_before(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	s32 idx;
	u32 tag;
	GF_Node *n, *target, *new_node;
	GF_ParentNode *par;
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
	dom_node_inserted(n, new_node, idx);
	*rval = argv[0];
	return JS_TRUE;
}

static JSBool xml_node_append_child(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag;
	GF_Node *n, *new_node;
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

	dom_node_inserted(n, new_node, -1);

	*rval = argv[0];
	return JS_TRUE;
}

static JSBool xml_node_replace_child(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	s32 idx;
	u32 tag;
	GF_Node *n, *new_node, *old_node;
	GF_ParentNode *par;
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

	dom_node_inserted(n, new_node, -1);
	
	*rval = argv[0];
	return JS_TRUE;
}

static JSBool xml_node_remove_child(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag;
	GF_Node *n, *old_node;
	GF_ParentNode *par;
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
	*rval = argv[0];
	dom_node_changed(n, 1, NULL);
	return JS_TRUE;
}


static JSBool xml_clone_node(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool deep;
	GF_Node *n, *clone;
	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	deep = argc ? JSVAL_TO_BOOLEAN(argv[0]) : 0;
	
	clone = gf_node_clone(n->sgprivate->scenegraph, n, NULL, "", deep);
	*rval = dom_node_construct(c, clone);
	return JS_TRUE;
}


static JSBool xml_node_has_children(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n;
	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	*rval = BOOLEAN_TO_JSVAL( ((GF_ParentNode*)n)->children ? JS_TRUE : JS_FALSE);
	return JS_TRUE;
}

static JSBool xml_node_has_attributes(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag;
	GF_Node *n;
	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	tag = gf_node_get_tag(n);
	if (tag>=GF_NODE_FIRST_DOM_NODE_TAG) {
		*rval = BOOLEAN_TO_JSVAL( ((GF_DOMNode*)n)->attributes ? JS_TRUE : JS_FALSE);
	} else {
		/*not supported for other node types*/
		*rval = BOOLEAN_TO_JSVAL( JS_FALSE);
	}
	return JS_TRUE;
}

static JSBool xml_node_is_same_node(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n, *a_node;
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_TRUE;
	n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	a_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]));
	if (!a_node) return JS_TRUE;
	*rval = BOOLEAN_TO_JSVAL( (a_node==n) ? JS_TRUE : JS_FALSE);
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
				u32 crc = gf_crc_32(xmlns, strlen(xmlns));
				if (tag==crc) return xmlns;
			}
		}
		att = att->next;
	}
	/*browse for parent*/
	return node_lookup_namespace_by_tag(gf_node_get_parent(node, 0), tag);
}

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

static JSBool dom_node_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
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
	if (!JSVAL_IS_INT(id)) return JS_TRUE;

	tag = n ? gf_node_get_tag(n) : 0;
	par = (GF_ParentNode*)n;

	switch (JSVAL_TO_INT(id)) {
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
		*vp = dom_node_get_sibling(c, n, 1, 0);
		return JS_TRUE;
	/*"nextSibling"*/
	case 8:
		*vp = dom_node_get_sibling(c, n, 0, 0);
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
			char *res = dom_node_flatten_text(n);
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, res) );
			free(res);
		}
		return JS_TRUE;
	}
	/*not supported*/
	return JS_TRUE;
}

void dom_node_set_textContent(GF_Node *n, char *text)
{
	GF_ParentNode *par = (GF_ParentNode *)n;
	gf_node_unregister_children(n, par->children);
	par->children = NULL;
	gf_dom_add_text_node(n, strdup( text) );
	dom_node_changed(n, 1, NULL);
}

static JSBool dom_node_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 tag;
	GF_Node *n;
	GF_ParentNode *par;

	n = dom_get_node(c, obj);
	/*note an element - we don't support property setting on document yet*/
	if (!n) return JS_TRUE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;

	tag = n ? gf_node_get_tag(n) : 0;
	par = (GF_ParentNode*)n;

	switch (JSVAL_TO_INT(id)) {
	/*"nodeValue"*/
	case 1:
		if ((tag==TAG_DOMText) && JSVAL_CHECK_STRING(*vp)) {
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->textContent) free(txt->textContent);
			txt->textContent = js_get_utf8(*vp);
			dom_node_changed(n, 1, NULL);
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
		txt = js_get_utf8(*vp);
		dom_node_set_textContent(n, txt);
		if (txt) free(txt);
	}
		return JS_TRUE;
	}
	/*not supported*/
	return JS_TRUE;
}


/*dom3 document*/

/*don't attempt to do anything with the scenegraph, it may be destroyed
fortunately a sg cannot be created like that...*/
void dom_document_finalize(JSContext *c, JSObject *obj)
{
	GF_SceneGraph *sg = dom_get_doc(c, obj);

	sg = (GF_SceneGraph*) JS_GetPrivate(c, obj);
	/*the JS proto of the svgClass or a destroyed object*/
	if (!sg) return;

	sg->document = NULL;
	gf_node_unregister(sg->RootNode, NULL);
	if (sg->reference_count) {
		sg->reference_count--;
		if (!sg->reference_count) 
			gf_sg_del(sg);
	}
}

static JSBool dom_document_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_TRUE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);

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

static JSBool dom_document_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_TRUE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);
	
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

static JSBool xml_document_create_element(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag, ns;
	GF_Node *n;
	char *name, *xmlns;
	GF_SceneGraph *sg = dom_get_doc(c, obj);

	if (!sg || !argc || !JSVAL_CHECK_STRING(argv[0]) ) 
		return JS_TRUE;

	name = NULL;
	/*NS version*/
	ns = 0;
	xmlns = NULL;
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		xmlns = js_get_utf8(argv[0]);
		if (xmlns) ns = gf_sg_get_namespace_code_from_name(sg, xmlns);
		name = JSVAL_GET_STRING(argv[1]);
	} else {
		name = JSVAL_GET_STRING(argv[0]);
	}

	if (name) {
		/*browse all our supported DOM implementations*/
		tag = gf_xml_get_element_tag(name, ns);
		if (!tag) tag = TAG_DOMFullNode;
		n = gf_node_new(sg, tag);
		if (n && (tag == TAG_DOMFullNode)) {
			GF_DOMFullNode *elt = (GF_DOMFullNode *)n;
			elt->name = strdup(name);
			if (xmlns) {
				gf_sg_add_namespace(sg, xmlns, NULL);
				elt->ns	= gf_sg_get_namespace_code_from_name(sg, xmlns);
			}
		}
		*rval = dom_element_construct(c, n);
	}
	if (xmlns) free(xmlns);
	return JS_TRUE;
}

static JSBool xml_document_create_text(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_TRUE;

	n = gf_node_new(sg, TAG_DOMText);
	if (argc) {
		char *str = js_get_utf8(argv[0]);
		if (!str) str = strdup("");
		((GF_DOMText*)n)->textContent = str;
	}
	*rval = dom_text_construct(c, n);
	return JS_TRUE;
}

static void xml_doc_gather_nodes(GF_ParentNode *node, char *name, DOMNodeList *nl)
{
	Bool bookmark = 1;
	GF_ChildNodeItem *child;
	const char *node_name;
	if (!node) return;
	if (name) {
		node_name = gf_node_get_class_name((GF_Node*)node);
		if (strcmp(node_name, name)) bookmark = 0;
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

static JSBool xml_document_elements_by_tag(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	DOMNodeList *nl;
	JSObject *new_obj;
	char *name;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	name = JSVAL_GET_STRING(argv[0]);
	/*NS version - TODO*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		name = JSVAL_GET_STRING(argv[1]);
	}

	GF_SAFEALLOC(nl, DOMNodeList);
	if (name && !strcmp(name, "*")) name = NULL;
	xml_doc_gather_nodes((GF_ParentNode*)sg->RootNode, name, nl);
	new_obj = JS_NewObject(c, &dom_rt->domNodeListClass, 0, 0);
	JS_SetPrivate(c, new_obj, nl);
	*rval = OBJECT_TO_JSVAL(new_obj);
	return JS_TRUE;
}

static JSBool xml_document_element_by_id(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	NodeIDedItem *reg_node;
	GF_Node *n;
	char *id;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return JS_FALSE;
	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	id = JSVAL_GET_STRING(argv[0]);

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
	*rval = dom_element_construct(c, n);
	return JS_TRUE;
}

/*dom3 element*/
void dom_element_finalize(JSContext *c, JSObject *obj)
{
	dom_node_finalize(c, obj);
}

static JSBool dom_element_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);
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

static JSBool xml_element_get_attribute(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *name, *ns;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) 
		return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0])) 
		return JS_TRUE;
	name = JSVAL_GET_STRING(argv[0]);
	ns = NULL;
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) 
			return JS_TRUE;
		ns = js_get_utf8(argv[0]);
		name = JSVAL_GET_STRING(argv[1]);
	}
	if (!name) goto exit;

	/*ugly ugly hack ...*/
	if (!strcmp(name, "id") || !strcmp(name, "xml:id") ) {
		char *sID = (char *) gf_node_get_name(n);
		*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, sID ? sID : "") );
		goto exit;
	}

	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, name)) {
				*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, (char*)att->data ) );
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
			if (!ns_code) ns_code = gf_crc_32(ns, strlen(ns));
		}
		else {
			ns_code = gf_xml_get_element_namespace(n);
		}

		if (gf_node_get_attribute_by_name(n, name, ns_code, 0, 0, &info)==GF_OK) {
			char szAtt[4096];
			gf_svg_dump_attribute(n, &info, szAtt);
			*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, szAtt) );
			goto exit;
		} 
	}
	*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "") );
exit:
	if (ns) free(ns);
	return JS_TRUE;
}


static JSBool xml_element_has_attribute(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *name, *ns;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) 
		return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	name = JSVAL_GET_STRING(argv[0]);
	ns = NULL;
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		ns = js_get_utf8(argv[0]);
		name = JSVAL_GET_STRING(argv[1]);
	}
	if (!name) goto exit;
	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, name)) {
				*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
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

		if (gf_node_get_attribute_by_name(n, name, ns_code, 0, 0, &info)==GF_OK) {
			*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
			goto exit;
		}
	}
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
exit:
	if (ns) free(ns);
	return JS_TRUE;
}



static JSBool xml_element_remove_attribute(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag;
	GF_DOMFullNode *node;
	GF_DOMFullAttribute *prev, *att;
	char *name, *ns;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	name = JSVAL_GET_STRING(argv[0]);
	ns = NULL;
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		ns = js_get_utf8(argv[0]);
		name = JSVAL_GET_STRING(argv[1]);
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
			if (att->data) free(att->data);
			free(att->name);
			free(att);
			dom_node_changed(n, 0, NULL);
			goto exit;
		} else if (tag==att->tag) {
			if (prev) prev->next = att->next;
			else node->attributes = att->next;
			gf_svg_delete_attribute_value(att->data_type, att->data, n->sgprivate->scenegraph);
			free(att);
			dom_node_changed(n, 0, NULL);
			goto exit;
		}
		prev = att;
		att = (GF_DOMFullAttribute *) att->next;
	}
exit:
	if (ns) free(ns);
	return JS_TRUE;
}


static JSBool xml_element_set_attribute(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 evtType, idx;
	char *name, *val, *ns;
	char szVal[100];
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	if ((argc < 2)) return JS_TRUE;

	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	name = JSVAL_GET_STRING(argv[0]);

	idx = 1;
	ns = NULL;
	/*NS version*/
	if (argc==3) {
		char *sep;
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		ns = js_get_utf8(argv[0]);
		gf_sg_add_namespace(n->sgprivate->scenegraph, ns, NULL);
		name = JSVAL_GET_STRING(argv[1]);
		idx = 2;

		sep = strchr(name, ':');
		if (sep) name = sep+1;

	}

	val = NULL;
	if (JSVAL_CHECK_STRING(argv[idx])) {
		val = JSVAL_GET_STRING(argv[idx]);
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
	if (!name || !val) goto exit;


	if ((name[0]=='o') && (name[1]=='n')) {
		evtType = gf_dom_event_type_by_name(name + 2);
		if (evtType != GF_EVENT_UNKNOWN) {
			/*check if we're modifying an existing listener*/
			SVG_handlerElement *handler;
			u32 i, count = gf_dom_listener_count(n);
			for (i=0;i<count; i++) {
				GF_FieldInfo info;
				GF_DOMText *text;
				GF_Node *listen = gf_dom_listener_get(n, i);

				gf_node_get_attribute_by_tag(listen, TAG_XMLEV_ATT_event, 0, 0, &info);
				if (!info.far_ptr || (((XMLEV_Event*)info.far_ptr)->type != evtType)) continue;
				gf_node_get_attribute_by_tag(listen, TAG_XMLEV_ATT_handler, 0, 0, &info);
				assert(info.far_ptr);
				handler = (SVG_handlerElement *) ((XMLRI*)info.far_ptr)->target;
				text = (GF_DOMText*)handler->children->node;
				if (text->sgprivate->tag==TAG_DOMText) {
					if (text->textContent) free(text->textContent);
					text->textContent = strdup(val);
				}
				goto exit;
			}
			/*nope, create a listener*/
			handler = gf_dom_listener_build(n, evtType, 0);
			gf_dom_add_text_node((GF_Node*)handler, strdup(val) );
			goto exit;
		}
	}

	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullAttribute *prev = NULL;
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, name)) {
				if (att->data) free(att->data);
				att->data = strdup(val);
				dom_node_changed(n, 0, NULL);
				goto exit;
			}
			prev = att;
			att = (GF_DOMFullAttribute *) att->next;
		}
		/*create new att*/
		GF_SAFEALLOC(att, GF_DOMFullAttribute);
		att->name = strdup(name);
		att->data = strdup(val);
		if (prev) prev->next = (GF_DOMAttribute*) att;
		else node->attributes = (GF_DOMAttribute*) att;
		goto exit;
	}
	
	if (n->sgprivate->tag==TAG_DOMText) {
		goto exit;
	}

	if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) {
		GF_FieldInfo info;
		u32 ns_code = 0;
		if (ns) {
			ns_code = gf_sg_get_namespace_code_from_name(n->sgprivate->scenegraph, ns);
		} else {
			ns_code = gf_xml_get_element_namespace(n);
		}
		if (gf_node_get_attribute_by_name(n, name, ns_code,  1, 1, &info)==GF_OK) {
			gf_svg_parse_attribute(n, &info, val, 0);

			if (info.fieldType==SVG_ID_datatype) {
				gf_svg_parse_element_id(n, *(SVG_String*)info.far_ptr, 0);
			}
			dom_node_changed(n, 0, &info);
			goto exit;
		}
	}
exit:
	if (ns) free(ns);
	return JS_TRUE;
}


static JSBool xml_element_elements_by_tag(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	DOMNodeList *nl;
	JSObject *new_obj;
	char *name;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if (!argc || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;

	name = JSVAL_GET_STRING(argv[0]);
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		name = JSVAL_GET_STRING(argv[1]);
	}
	GF_SAFEALLOC(nl, DOMNodeList);
	if (name && !strcmp(name, "*")) name = NULL;
	xml_doc_gather_nodes((GF_ParentNode*)n, name, nl);
	new_obj = JS_NewObject(c, &dom_rt->domNodeListClass, 0, 0);
	JS_SetPrivate(c, new_obj, nl);
	*rval = OBJECT_TO_JSVAL(new_obj);
	return JS_TRUE;
}

static JSBool xml_element_set_id(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	const char *node_name;
	u32 node_id;
	char *name;
	Bool is_id;
	GF_Node *n = dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if ((argc<2) || !JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	name = JSVAL_GET_STRING(argv[0]);

	/*NS version*/
	if (argc==2) {
		if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
		name = JSVAL_GET_STRING(argv[1]);
		is_id = (JSVAL_TO_BOOLEAN(argv[2])==JS_TRUE) ? 1 : 0;
	} else {
		is_id = (JSVAL_TO_BOOLEAN(argv[1])==JS_TRUE) ? 1 : 0;
	}
	node_name = gf_node_get_name_and_id(n, &node_id);
	if (node_id && is_id) {
		/*we only support ONE ID per node*/
		return JS_TRUE;
	}
	if (is_id) {
		if (!name) return JS_TRUE;
		gf_node_set_id(n, gf_sg_get_max_node_id(n->sgprivate->scenegraph) + 1, strdup(name) );
	} else if (node_id) {
		gf_node_remove_id(n);
	}
	return JS_TRUE;
}


/*dom3 character/text/comment*/

static JSBool dom_text_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	GF_DOMText *txt = (GF_DOMText*)dom_get_node(c, obj);
	if (!txt || (txt->sgprivate->tag != TAG_DOMText)) return JS_TRUE;
	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);

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
static JSBool dom_text_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	GF_DOMText *txt = (GF_DOMText*)dom_get_node(c, obj);
	if (!txt || (txt->sgprivate->tag != TAG_DOMText)) return JS_TRUE;
	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);

	switch (prop_id) {
	case 1: /*"data"*/
		if (txt->textContent) free(txt->textContent);
		txt->textContent = NULL;
		if (JSVAL_CHECK_STRING(*vp)) {
			char *str = js_get_utf8(*vp);
			txt->textContent = str ? str : strdup("" );
		}
		dom_node_changed((GF_Node*)txt, 0, NULL);
		return JS_TRUE;
	/*the rest is read-only*/
	}
	return JS_TRUE;
}

static JSBool event_stop_propagation(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_DOM_Event *evt = JS_GetPrivate(c, obj);
	if (!evt) return JS_TRUE;
	evt->event_phase |= GF_DOM_EVENT_PHASE_CANCEL;
	return JS_TRUE;
}
static JSBool event_stop_immediate_propagation(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_DOM_Event *evt = JS_GetPrivate(c, obj);
	if (!evt) return JS_TRUE;
	evt->event_phase |= GF_DOM_EVENT_PHASE_CANCEL_ALL;
	return JS_TRUE;
}
static JSBool event_prevent_default(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_DOM_Event *evt = JS_GetPrivate(c, obj);
	if (!evt) return JS_TRUE;
	evt->event_phase |= GF_DOM_EVENT_PHASE_PREVENT;
	return JS_TRUE;
}

static JSBool event_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	JSString *s;
	GF_DOM_Event *evt = JS_GetPrivate(c, obj);
	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
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
			srcp = txt;
			len = gf_utf8_wcstombs(szData, 5, &srcp);
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

		/*MAE*/
		case 54:/*bufferLevelValid*/
			if (!evt->mae) return JS_TRUE;
			*vp = BOOLEAN_TO_JSVAL( evt->mae->bufferValid ? JS_TRUE : JS_FALSE);
			return JS_TRUE;
		case 55:/*bufferLevel*/
			if (!evt->mae) return JS_TRUE;
			*vp = INT_TO_JSVAL( evt->mae->level);
			return JS_TRUE;
		case 56:/*bufferRemainingTime*/
			if (!evt->mae) return JS_TRUE;
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, evt->mae->remaining_time) );
			return JS_TRUE;
		case 57:/*status*/
			if (!evt->mae) return JS_TRUE;
			*vp = INT_TO_JSVAL( evt->mae->status);
			return JS_TRUE;

		/*VRML ones*/
		case 60:/*width*/
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, evt->screen_rect.width) );
			return JS_TRUE;
		case 61:/*height*/
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, evt->screen_rect.height) );
			return JS_TRUE;
		case 62:/*h_translation*/
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, evt->new_translate.x) );
			return JS_TRUE;
		case 63:/*v_translation*/
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, evt->new_translate.y) );
			return JS_TRUE;


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

typedef struct
{
	JSContext *c;
	JSObject *_this;
	JSFunction *onreadystatechange;

	u32 readyState;
	Bool async;
	/*header/header-val, terminated by NULL*/
	char **headers;
	u32 cur_header;
	char **recv_headers;

	char *method, *url;
	GF_DownloadSession *sess;
	char *data;
	u32 size;
	GF_Err ret_code;
	u32 html_status;
	char *statusText;

	GF_SAXParser *sax;
	GF_List *node_stack;
	/*dom graph*/
	GF_SceneGraph *document;
} XMLHTTPContext;

static void xml_http_append_send_header(XMLHTTPContext *ctx, char *hdr, char *val)
{
	u32 nb_hdr = 0;
	if (!hdr) return;

	if (ctx->headers) {
		nb_hdr = 0;
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
				free(ctx->headers[nb_hdr+1]);
				ctx->headers[nb_hdr+1] = strdup(val);
				return;
			}
			/*append value*/
			else {
				char *new_val = malloc(sizeof(char) * (strlen(ctx->headers[nb_hdr+1])+strlen(val)+3));
				sprintf(new_val, "%s, %s", ctx->headers[nb_hdr+1], val);
				free(ctx->headers[nb_hdr+1]);
				ctx->headers[nb_hdr+1] = new_val;
				return;
			}
			nb_hdr+=2;
		}
	}
	nb_hdr = 0;
	if (ctx->headers) {
		while (ctx->headers[nb_hdr]) nb_hdr+=2;
	}
	ctx->headers = realloc(ctx->headers, sizeof(char*)*(nb_hdr+3));
	ctx->headers[nb_hdr] = strdup(hdr);
	ctx->headers[nb_hdr+1] = strdup(val ? val : "");
	ctx->headers[nb_hdr+2] = NULL;
}

static void xml_http_reset_recv_hdr(XMLHTTPContext *ctx)
{
	u32 nb_hdr = 0;
	if (ctx->recv_headers) {
		while (ctx->recv_headers[nb_hdr]) {
			free(ctx->recv_headers[nb_hdr]);
			free(ctx->recv_headers[nb_hdr+1]);
			nb_hdr+=2;
		}
		free(ctx->recv_headers);
		ctx->recv_headers = NULL;
	}
}
static void xml_http_append_recv_header(XMLHTTPContext *ctx, char *hdr, char *val)
{
	u32 nb_hdr = 0;
	if (ctx->recv_headers) {
		while (ctx->recv_headers[nb_hdr]) nb_hdr+=2;
	}
	ctx->recv_headers = realloc(ctx->recv_headers, sizeof(char*)*(nb_hdr+3));
	ctx->recv_headers[nb_hdr] = strdup(hdr);
	ctx->recv_headers[nb_hdr+1] = strdup(val ? val : "");
	ctx->recv_headers[nb_hdr+2] = NULL;
}


static void xml_http_reset(XMLHTTPContext *ctx)
{
	u32 nb_hdr = 0;
	if (ctx->method) { free(ctx->method); ctx->method = NULL; }
	if (ctx->url) { free(ctx->url); ctx->url = NULL; }

	xml_http_reset_recv_hdr(ctx);
	if (ctx->headers) {
		while (ctx->headers[nb_hdr]) {
			free(ctx->headers[nb_hdr]);
			free(ctx->headers[nb_hdr+1]);
			nb_hdr+=2;
		}
		free(ctx->headers);
		ctx->headers = NULL;
	}
	if (ctx->sess) {
		gf_dm_sess_del(ctx->sess);
		ctx->sess = NULL;
	}
	if (ctx->data) {
		free(ctx->data);
		ctx->data = NULL;
	}
	if (ctx->statusText) {
		free(ctx->statusText);
		ctx->statusText = NULL;
	}
	if (ctx->url) {
		free(ctx->url);
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
			gf_sg_del(ctx->document);
		}
	}
	ctx->document = NULL;
	ctx->size = 0;
	ctx->async = 0;
	ctx->readyState = 0;
	ctx->cur_header = 0;
	ctx->ret_code = 0;
	ctx->html_status = 0;
}

static void xml_http_finalize(JSContext *c, JSObject *obj)
{
	XMLHTTPContext *ctx;
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (ctx) {
		xml_http_reset(ctx);
		free(ctx);
	}
}
static JSBool xml_http_constructor(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	XMLHTTPContext *p;
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	GF_SAFEALLOC(p, XMLHTTPContext);
	p->c = c;
	p->_this = obj;
	JS_SetPrivate(c, obj, p);
	*rval = OBJECT_TO_JSVAL(obj);
	return JS_TRUE;
}

static void xml_http_state_change(XMLHTTPContext *ctx)
{
	jsval rval;
	//GF_SceneGraph *scene = (GF_SceneGraph *) xml_get_scenegraph(ctx->c);
	if (ctx->onreadystatechange)
		JS_CallFunction(ctx->c, ctx->_this, ctx->onreadystatechange, 0, NULL, &rval);

	/*todo - fire event*/
}


static JSBool xml_http_open(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *val;
	GF_JSAPIParam par;
	XMLHTTPContext *ctx;
	GF_SceneGraph *scene;

	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_TRUE;

	/*reset*/
	if (ctx->readyState) xml_http_reset(ctx);

	if (argc<2) return JS_TRUE;
	/*method is a string*/
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	/*url is a string*/
	if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;

	xml_http_reset(ctx);
	val = JSVAL_GET_STRING(argv[0]);
	if (strcmp(val, "GET") && strcmp(val, "POST") && strcmp(val, "HEAD")
	&& strcmp(val, "PUT") && strcmp(val, "DELETE") && strcmp(val, "OPTIONS") )
		return JS_TRUE;

	ctx->method = strdup(val);

	val = JSVAL_GET_STRING(argv[1]);
	/*concatenate URL*/
	scene = xml_get_scenegraph(c);
	while (scene->pOwningProto && scene->parent_scene) scene = scene->parent_scene;
	ScriptAction(scene, GF_JSAPI_OP_GET_SCENE_URI, scene->RootNode, &par);

	if (par.uri.url) ctx->url = gf_url_concatenate(par.uri.url, val);
	if (!ctx->url) ctx->url = strdup(val);

	/*async defaults to true*/
	ctx->async = 1;
	if (argc>2) {
		ctx->async = (JSVAL_TO_BOOLEAN(argv[2])==JS_TRUE) ? 1 : 0;
		if (argc>3) {
			if (!JSVAL_CHECK_STRING(argv[3])) return JS_TRUE;
			val = JSVAL_GET_STRING(argv[3]);
			/*TODO*/
			if (argc>4) {
				if (!JSVAL_CHECK_STRING(argv[3])) return JS_TRUE;
				val = JSVAL_GET_STRING(argv[3]);
				/*TODO*/
			}
		}
	}
	/*OPEN success*/
	ctx->readyState = 1;
	xml_http_state_change(ctx);
	return JS_TRUE;
}

static JSBool xml_http_set_header(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *hdr, *val;
	XMLHTTPContext *ctx;
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_TRUE;

	if (ctx->readyState!=1) return JS_TRUE;
	if (argc!=2) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;

	hdr = JSVAL_GET_STRING(argv[0]);
	val = JSVAL_GET_STRING(argv[1]);
	xml_http_append_send_header(ctx, hdr, val);
	return JS_TRUE;
}
static void xml_http_sax_start(void *sax_cbck, const char *node_name, const char *name_space, const GF_XMLAttribute *attributes, u32 nb_attributes)
{
	u32 i;
	GF_DOMFullAttribute *prev = NULL;
	GF_DOMFullNode *par;
	XMLHTTPContext *ctx = (XMLHTTPContext *)sax_cbck;
	GF_DOMFullNode *node = (GF_DOMFullNode *) gf_node_new(ctx->document, TAG_DOMFullNode);

	node->name = strdup(node_name);
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
			att->name = strdup(attributes[i].name);
			att->data = strdup(attributes[i].value);
			if (prev) prev->next = (GF_DOMAttribute*)att;
			else node->attributes = (GF_DOMAttribute*)att;
			prev = att;
		}
	}
	par = gf_list_last(ctx->node_stack);
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
	GF_DOMFullNode *par = gf_list_last(ctx->node_stack);
	if (par) {
		/*depth mismatch*/
		if (strcmp(par->name, node_name)) return;
		gf_list_rem_last(ctx->node_stack);
	}
}
static void xml_http_sax_text(void *sax_cbck, const char *content, Bool is_cdata)
{
	XMLHTTPContext *ctx = (XMLHTTPContext *)sax_cbck;
	GF_DOMFullNode *par = gf_list_last(ctx->node_stack);
	if (par) {
		u32 i, len;
		GF_DOMText *txt;
		/*basic check, remove all empty text nodes*/
		len = strlen(content);
		for (i=0; i<len; i++) {
			if (!strchr(" \n\r\t", content[i])) break;
		}
		if (i==len) return;

		txt = gf_dom_add_text_node((GF_Node *)par, strdup(content) );
		txt->type = is_cdata ? GF_DOM_TEXT_CDATA : GF_DOM_TEXT_REGULAR;
	}
}

static void xml_http_on_data(void *usr_cbk, GF_NETIO_Parameter *parameter)
{
	XMLHTTPContext *ctx = (XMLHTTPContext *)usr_cbk;

	/*if session not set, we've had an abort*/
	if (!ctx->sess) return;

	switch (parameter->msg_type) {
	case GF_NETIO_SETUP:
		/*nothing to do*/
		return;
	case GF_NETIO_CONNECTED:
		/*nothing to do*/
		return;
	case GF_NETIO_WAIT_FOR_REPLY:
		/*reset send() state (data, current header) and prepare recv headers*/
		if (ctx->data) free(ctx->data);
		ctx->data = NULL;
		ctx->size = 0;
		ctx->cur_header = 0;
		ctx->html_status = 0;
		if (ctx->statusText) {
			free(ctx->statusText);
			ctx->statusText = NULL;
		}
		xml_http_reset_recv_hdr(ctx);
		ctx->readyState = 2;
		xml_http_state_change(ctx);
		return;
	/*this is signaled sent AFTER headers*/
	case GF_NETIO_PARSE_REPLY:
		ctx->html_status = parameter->reply;
		if (parameter->value) {
			ctx->statusText = strdup(parameter->value);
		}
		ctx->readyState = 3;
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
			parameter->size = strlen(ctx->data);
		}
		return;
	case GF_NETIO_PARSE_HEADER:
		xml_http_append_recv_header(ctx, parameter->name, parameter->value);
		/*prepare DOM*/
		if (strcmp(parameter->name, "Content-Type")) return;
		if (!strncmp(parameter->value, "application/xml", 15)
			|| !strncmp(parameter->value, "text/xml", 8)
			|| strstr(parameter->value, "+xml")
			|| !strncmp(parameter->value, "text/plain", 10)
		) {
			assert(!ctx->sax);
			ctx->sax = gf_xml_sax_new(xml_http_sax_start, xml_http_sax_end, xml_http_sax_text, ctx);
			ctx->node_stack = gf_list_new();
			ctx->document = gf_sg_new();
			/*mark this doc as "nomade", and let it leave until all references to it are destroyed*/
			ctx->document->reference_count = 1;
		}
		return;
	case GF_NETIO_DATA_EXCHANGE:
		if (parameter->data && parameter->size) {
			if (ctx->sax) {
				GF_Err e;
				if (!ctx->size) e = gf_xml_sax_init(ctx->sax, parameter->data);
				else e = gf_xml_sax_parse(ctx->sax, parameter->data);
				if (e) {
					gf_xml_sax_del(ctx->sax);
					ctx->sax = NULL;
				}
			}
			ctx->data = realloc(ctx->data, sizeof(char)*(ctx->size+parameter->size+1));
			memcpy(ctx->data + ctx->size, parameter->data, sizeof(char)*parameter->size);
			ctx->size += parameter->size;
			ctx->data[ctx->size] = 0;
		}
		return;
	case GF_NETIO_DATA_TRANSFERED:
		break;
	case GF_NETIO_DISCONNECTED:
		return;
	case GF_NETIO_STATE_ERROR:
		ctx->ret_code = parameter->error;
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
	}
	/*but stay in loaded mode*/
	ctx->readyState = 4;
	xml_http_state_change(ctx);
}

static JSBool xml_http_send(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_JSAPIParam par;
	GF_SceneGraph *scene;
	char *data = NULL;
	XMLHTTPContext *ctx;
	GF_Err e;

	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_TRUE;

	if (ctx->readyState!=1) return JS_TRUE;
	if (ctx->sess) return JS_TRUE;

	if (argc) {
		if (JSVAL_IS_NULL(argv[0])) {
		} else  if (JSVAL_IS_OBJECT(argv[0])) {
//			if (!JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &documentClass, NULL) ) return JS_TRUE;
			
			/*NOT SUPPORTED YET, we must serialize the sg*/
			return JS_TRUE;
		} else {
			if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
			data = JSVAL_GET_STRING(argv[0]);
		}
	}

	scene = xml_get_scenegraph(c);
	par.dnld_man = NULL;
	ScriptAction(scene, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
	if (!par.dnld_man) return JS_TRUE;

	/*reset previous text*/
	if (ctx->data) free(ctx->data);

	ctx->data = data ? strdup(data) : NULL;
	ctx->sess = gf_dm_sess_new(par.dnld_man, ctx->url, GF_NETIO_SESSION_NOT_CACHED, xml_http_on_data, ctx, &e);
	if (!ctx->sess) return JS_TRUE;

	/*just wait for destruction*/
	if (!ctx->async) {
		while (ctx->sess) {
			gf_sleep(20);
		}
	}
	return JS_TRUE;
}

static JSBool xml_http_abort(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_DownloadSession *sess;
	XMLHTTPContext *ctx;
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_TRUE;

	sess = ctx->sess;
	ctx->sess = NULL;
	if (sess) gf_dm_sess_del(sess);

	xml_http_reset(ctx);
	return JS_TRUE;
}

static JSBool xml_http_get_all_headers(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 nb_hdr;
	char szVal[4096];
	XMLHTTPContext *ctx;
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_TRUE;

	/*must be received or loaded*/
	if (ctx->readyState<3) return JS_TRUE;
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
		*rval = JSVAL_VOID;
	} else {
		JSString *s;
		s = JS_NewStringCopyZ(c, szVal);
		*rval = STRING_TO_JSVAL( s );
	}

	return JS_TRUE;
}

static JSBool xml_http_get_header(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 nb_hdr;
	char *hdr;
	char szVal[2048];
	XMLHTTPContext *ctx;
	if (!argc || !JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_TRUE;

	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	hdr = JSVAL_GET_STRING(argv[0]);
	/*must be received or loaded*/
	if (ctx->readyState<3) return JS_TRUE;
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
		*rval = JSVAL_VOID;
	} else {
		JSString *s;
		s = JS_NewStringCopyZ(c, szVal);
		*rval = STRING_TO_JSVAL( s );
	}

	return JS_TRUE;
}

static JSBool xml_http_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	JSString *s;
	XMLHTTPContext *ctx;
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_TRUE;

	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		/*onreadystatechange*/
		case 0: 
			if (ctx->onreadystatechange) {
				*vp = OBJECT_TO_JSVAL(ctx->onreadystatechange);
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
		/*readyState*/
		case 1: 
			*vp = INT_TO_JSVAL(ctx->readyState); return JS_TRUE;
		/*responseText*/
		case 2: 
			if (ctx->readyState<3) return JS_TRUE;
			if (ctx->data) {
				s = JS_NewStringCopyZ(c, ctx->data);
				*vp = STRING_TO_JSVAL( s );
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
		/*responseXML*/
		case 3: 
			if (ctx->readyState<3) return JS_TRUE;
			*vp = dom_document_construct(c, ctx->document);
			return JS_TRUE;
		/*status*/
		case 4: 
			*vp = INT_TO_JSVAL(ctx->html_status); return JS_TRUE;
		/*statusText*/
		case 5: 
			if (ctx->statusText) {
				s = JS_NewStringCopyZ(c, ctx->statusText);
				*vp = STRING_TO_JSVAL( s );
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
		default:
			return JS_TRUE;
		}
	}
	return JS_PropertyStub(c, obj, id, vp);
	return JS_TRUE;
}

static JSBool xml_http_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	XMLHTTPContext *ctx;
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_TRUE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_TRUE;

	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		/*onreadystatechange*/
		case 0: 
			break;
		/*all other properties are read-only*/
		default:
			return JS_TRUE;
		}
		if (JSVAL_IS_VOID(*vp)) {
			ctx->onreadystatechange = NULL;
			return JS_TRUE;
		}
		else if (JSVAL_CHECK_STRING(*vp)) {
			jsval fval;
			char *callback = JSVAL_GET_STRING(*vp);
			if (! JS_LookupProperty(c, JS_GetGlobalObject(c), callback, &fval)) return JS_TRUE;
			ctx->onreadystatechange = JS_ValueToFunction(c, fval);
			if (ctx->onreadystatechange) return JS_TRUE;
		} else if (JSVAL_IS_OBJECT(*vp)) {
			ctx->onreadystatechange = JS_ValueToFunction(c, *vp);
			if (ctx->onreadystatechange) return JS_TRUE;
		}
		return JS_TRUE;
	}
	return JS_TRUE;
}


static JSBool dcci_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_DOMFullAttribute *att;
	GF_ChildNodeItem *child;
	GF_DOMFullNode *n;
	char *value;
	JSString *s;
	if (!JS_InstanceOf(c, obj, &dom_rt->DCCIClass, NULL) ) return JS_TRUE;
	n = (GF_DOMFullNode*) dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
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
				if (att->name && !strcmp(att->name, "readOnly") && att->data && !strcmp(att->data, "true")) {
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
	return JS_PropertyStub(c, obj, id, vp);
}

static JSBool dcci_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_ChildNodeItem *child;
	GF_DOMFullNode *n;
	GF_DOM_Event evt;
	char *str;
	jsval readonly;
	if (!JS_InstanceOf(c, obj, &dom_rt->DCCIClass, NULL) ) return JS_TRUE;
	n = (GF_DOMFullNode*) dom_get_node(c, obj);
	if (!n) return JS_TRUE;

	str = JSVAL_GET_STRING( *vp);
	if (!str) return JS_TRUE;

	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		/*value*/
		case 0: 
			dcci_getProperty(c, obj, INT_TO_JSVAL(3), &readonly);
			if (JSVAL_TO_BOOLEAN(readonly) == JS_TRUE) return JS_TRUE;
			child = n->children;
			while (child) {
				if (child->node && (child->node->sgprivate->tag==TAG_DOMText)) {
					GF_DOMText *txt = (GF_DOMText *)child->node;
					if (txt->type==GF_DOM_TEXT_REGULAR) {
						if (txt->textContent) free(txt->textContent);
						txt->textContent = strdup(str);
						break;

					}
				}
				child = child->next;
			}
			if (!child)
				gf_dom_add_text_node((GF_Node*)n, strdup(str) );

			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.type = GF_EVENT_DCCI_PROP_CHANGE;
			evt.bubbles = 1;
			evt.relatedNode = (GF_Node*)n;
			gf_dom_event_fire((GF_Node*)n, &evt);
			n->sgprivate->scenegraph->modified = 1;
			return JS_TRUE;
		/*propertyType*/
		case 2: 
			return JS_TRUE;

		/*all other properties are read-only*/
		default:
			return JS_TRUE;
		}
		return JS_TRUE;
	}
	return JS_TRUE;
}

Bool dcci_prop_lookup(GF_DOMFullNode *n, char *ns, char *name, Bool deep, Bool first)
{
	Bool ok = 1;
	GF_ChildNodeItem *child = n->children;
	/*ns mismatch*/
	if (strcmp(ns, "*") && n->ns && strcmp(ns, gf_sg_get_namespace(n->sgprivate->scenegraph, n->ns) ))
		ok = 0;
	/*name mismatch*/
	if (strcmp(name, "*") && n->name && strcmp(name, n->name))
		ok = 0;
	
	/*"Some DCCIProperty nodes may not have a value"*/
	if (ok) return 1;

	/*not found*/
	if (!first && !deep) return 0;

	while (child) {
		if (child->node && (child->node->sgprivate->tag==TAG_DOMFullNode)) {
			ok = dcci_prop_lookup((GF_DOMFullNode *)child->node, ns, name, deep, 0);
			if (ok) return 1;
		}
		child = child->next;
	}
	return 0;
}

static JSBool dcci_has_property(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_DOMFullNode *n;
	Bool deep;
	char *ns, *name;
	if (!JS_InstanceOf(c, obj, &dom_rt->DCCIClass, NULL) ) return JS_TRUE;
	n = (GF_DOMFullNode*) dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	if (argc!=3) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
	ns = JSVAL_GET_STRING(argv[0]);
	name = JSVAL_GET_STRING(argv[1]);

	deep = JSVAL_TO_BOOLEAN(argv[2]) ? 1 : 0;
	deep = dcci_prop_lookup(n, ns, name, deep, 1);
	*rval = BOOLEAN_TO_JSVAL(deep ? JS_TRUE : JS_FALSE);

	return JS_TRUE;
}

void dcci_prop_collect(DOMNodeList *nl, GF_DOMFullNode *n, char *ns, char *name, Bool deep, Bool first)
{
	Bool ok = 1;
	GF_ChildNodeItem *child = n->children;
	/*ns mismatch*/
	if (strcmp(ns, "*") && n->ns && strcmp(ns, gf_sg_get_namespace(n->sgprivate->scenegraph, n->ns) ))
		ok = 0;
	/*name mismatch*/
	if (strcmp(name, "*") && n->name && strcmp(name, n->name))
		ok = 0;
	
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
			dcci_prop_collect(nl, (GF_DOMFullNode *)child->node, ns, name, 1, 0);
		}
		child = child->next;
	}
}

static JSBool dcci_search_property(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *new_obj;
	DOMNodeList *nl;
	GF_DOMFullNode *n;
	Bool deep;
	char *ns, *name;
	if (!JS_InstanceOf(c, obj, &dom_rt->DCCIClass, NULL) ) return JS_TRUE;
	n = (GF_DOMFullNode*) dom_get_node(c, obj);
	if (!n) return JS_TRUE;
	if (argc!=4) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[0])) return JS_TRUE;
	if (!JSVAL_CHECK_STRING(argv[1])) return JS_TRUE;
	ns = JSVAL_GET_STRING(argv[0]);
	name = JSVAL_GET_STRING(argv[1]);
	/*todo - DCCI prop filter*/
	deep = JSVAL_TO_BOOLEAN(argv[3]) ? 1 : 0;

	GF_SAFEALLOC(nl, DOMNodeList);
	dcci_prop_collect(nl, n, ns, name, deep, 1);
	new_obj = JS_NewObject(c, &dom_rt->domNodeListClass, 0, 0);
	JS_SetPrivate(c, new_obj, nl);
	*rval = OBJECT_TO_JSVAL(new_obj);
	return JS_TRUE;
}

void dom_js_load(GF_SceneGraph *scene, JSContext *c, JSObject *global)
{
	if (!dom_rt) {
		GF_SAFEALLOC(dom_rt, GF_DOMRuntime);
		JS_SETUP_CLASS(dom_rt->domNodeClass, "Node", JSCLASS_HAS_PRIVATE, dom_node_getProperty, dom_node_setProperty, dom_node_finalize);
		JS_SETUP_CLASS(dom_rt->domDocumentClass, "Document", JSCLASS_HAS_PRIVATE, dom_document_getProperty, dom_document_setProperty, dom_document_finalize);
		/*Element uses the same destructor as node*/
		JS_SETUP_CLASS(dom_rt->domElementClass, "Element", JSCLASS_HAS_PRIVATE, dom_element_getProperty, JS_PropertyStub, dom_node_finalize);
		/*Text uses the same destructor as node*/
		JS_SETUP_CLASS(dom_rt->domTextClass, "Text", JSCLASS_HAS_PRIVATE, dom_text_getProperty, dom_text_setProperty, dom_node_finalize);
		/*Event class*/
		JS_SETUP_CLASS(dom_rt->domEventClass , "Event", JSCLASS_HAS_PRIVATE, event_getProperty, JS_PropertyStub, JS_FinalizeStub);


		JS_SETUP_CLASS(dom_rt->domNodeListClass, "NodeList", JSCLASS_HAS_PRIVATE, dom_nodelist_getProperty, dom_nodelist_setProperty, dom_nodelist_finalize);
		JS_SETUP_CLASS(dom_rt->xmlHTTPRequestClass, "XMLHttpRequest", JSCLASS_HAS_PRIVATE, xml_http_getProperty, xml_http_setProperty, xml_http_finalize);

		JS_SETUP_CLASS(dom_rt->DCCIClass, "DCCI", JSCLASS_HAS_PRIVATE, dcci_getProperty, dcci_setProperty, dom_node_finalize);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] dom run-time allocated\n"));
	}
	dom_rt->nb_inst++;
	
	define_dom_exception(c, global);

	{
		JSFunctionSpec nodeFuncs[] = {
			{"insertBefore",			xml_node_insert_before, 2},
			{"replaceChild",			xml_node_replace_child, 2},
			{"removeChild",				xml_node_remove_child, 1},
			{"appendChild",				xml_node_append_child, 1},
			{"hasChildNodes",			xml_node_has_children, 0},
			{"cloneNode",				xml_clone_node, 1},
			{"normalize",				xml_dom3_not_implemented, 0},
			{"isSupported",				xml_dom3_not_implemented, 2},
			{"hasAttributes",			xml_node_has_attributes, 0},
			{"compareDocumentPosition", xml_dom3_not_implemented, 1},
			{"isSameNode",				xml_node_is_same_node, 1},
			{"lookupPrefix",			xml_dom3_not_implemented, 1},
			{"isDefaultNamespace",		xml_dom3_not_implemented, 1},
			{"lookupNamespaceURI",		xml_dom3_not_implemented, 1},
			/*we don't support full node compare*/
			{"isEqualNode",				xml_node_is_same_node, 1},
			{"getFeature",				xml_dom3_not_implemented, 2},
			{"setUserData",				xml_dom3_not_implemented, 3},
			{"getUserData",				xml_dom3_not_implemented, 1},
			{0}
		};
		JSPropertySpec nodeProps[] = {
			{"nodeName",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"nodeValue",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"nodeType",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"parentNode",		3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"childNodes",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"firstChild",		5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"lastChild",		6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"previousSibling",	7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"nextSibling",		8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"attributes",		9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"ownerDocument",	10,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"namespaceURI",	11,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"prefix",			12,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"localName",		13,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"baseURI",			14,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"textContent",		15,       JSPROP_ENUMERATE | JSPROP_PERMANENT},	
			{0}
		};
		dom_rt->dom_node_proto = JS_InitClass(c, global, 0, &dom_rt->domNodeClass, 0, 0, nodeProps, nodeFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] node class initialized\n"));

		JS_DefineProperty(c, dom_rt->dom_node_proto, "ELEMENT_NODE", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->dom_node_proto, "TEXT_NODE", INT_TO_JSVAL(3), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->dom_node_proto, "CDATA_SECTION_NODE", INT_TO_JSVAL(4), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->dom_node_proto, "DOCUMENT_NODE", INT_TO_JSVAL(9), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

	}
	{
		JSFunctionSpec documentFuncs[] = {
			{"createElement",				xml_document_create_element, 1},
			{"createDocumentFragment",		xml_dom3_not_implemented, 0},
			{"createTextNode",				xml_document_create_text, 1},
			{"createComment",				xml_dom3_not_implemented, 1},
			{"createCDATASection",			xml_dom3_not_implemented, 1},
			{"createProcessingInstruction",	xml_dom3_not_implemented, 2},
			{"createAttribute",				xml_dom3_not_implemented, 1},
			{"createEntityReference",		xml_dom3_not_implemented, 1},
			{"getElementsByTagName",		xml_document_elements_by_tag, 1},
			{"importNode",					xml_dom3_not_implemented, 2},
			{"createElementNS",				xml_document_create_element, 2},
			{"createAttributeNS",			xml_dom3_not_implemented, 2},
			{"getElementsByTagNameNS",		xml_document_elements_by_tag, 2},
			{"getElementById",				xml_document_element_by_id, 1},	
			{"adoptNode",					xml_dom3_not_implemented, 1},
			{"normalizeDocument",			xml_dom3_not_implemented, 0},
			{"renameNode",					xml_dom3_not_implemented, 3},
			/*eventTarget interface*/
			JS_DOM3_EVEN_TARGET_INTERFACE
			/*DocumentEvent interface*/
			{"createEvent",					xml_dom3_not_implemented, 1},
			{0}
		};

		JSPropertySpec documentProps[] = {
			{"doctype",				1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"implementation",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"documentElement",		3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"inputEncoding",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"xmlEncoding",			5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"xmlStandalone",		6,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"xmlVersion",			7,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"strictErrorChecking",	8,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"documentURI",			9,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"domConfig",			10,      JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{0}
		};

		dom_rt->dom_document_proto = JS_InitClass(c, global, dom_rt->dom_node_proto, &dom_rt->domDocumentClass, 0, 0, documentProps, documentFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] document class initialized\n"));
	}
	
	{
		JSFunctionSpec elementFuncs[] = {
			{"getAttribute",				xml_element_get_attribute, 1},
			{"setAttribute",				xml_element_set_attribute, 2},
			{"removeAttribute",				xml_element_remove_attribute, 1},
			{"getAttributeNS",				xml_element_get_attribute, 2},
			{"setAttributeNS",				xml_element_set_attribute, 3},
			{"removeAttributeNS",			xml_element_remove_attribute, 2},
			{"hasAttribute",				xml_element_has_attribute, 1},
			{"hasAttributeNS",				xml_element_has_attribute, 2},
			{"getElementsByTagName",		xml_element_elements_by_tag, 1},
			{"getElementsByTagNameNS",		xml_element_elements_by_tag, 2},
			{"setIdAttribute",				xml_element_set_id, 2},
			{"setIdAttributeNS",			xml_element_set_id, 3},
			{"getAttributeNode",			xml_dom3_not_implemented, 1},
			{"setAttributeNode",			xml_dom3_not_implemented, 1},
			{"removeAttributeNode",			xml_dom3_not_implemented, 1},
			{"getAttributeNodeNS",			xml_dom3_not_implemented, 2},
			{"setAttributeNodeNS",			xml_dom3_not_implemented, 1},
			{"setIdAttributeNode",			xml_dom3_not_implemented, 2},
			/*eventTarget interface*/
			JS_DOM3_EVEN_TARGET_INTERFACE
			{0}
		};

		JSPropertySpec elementProps[] = {
			{"tagName",				1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"schemaTypeInfo",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		dom_rt->dom_element_proto = JS_InitClass(c, global, dom_rt->dom_node_proto, &dom_rt->domElementClass, 0, 0, elementProps, elementFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] element class initialized\n"));
	}

	{
		JSFunctionSpec textFuncs[] = {
#if 0
			{"substringData",			xml_dom3_not_implemented, 2},
			{"appendData",				xml_dom3_not_implemented, 1},
			{"insertData",				xml_dom3_not_implemented, 2},
			{"deleteData",				xml_dom3_not_implemented, 2},
			{"replaceData",				xml_dom3_not_implemented, 3},
			/*text*/
			{"splitText",				xml_dom3_not_implemented, 1},
			{"replaceWholeText",		xml_dom3_not_implemented, 1},
#endif
			{0}
		};
		JSPropertySpec textProps[] = {
			{"data",						1,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"length",						2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*text*/
			{"isElementContentWhitespace",	3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"wholeText",					4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		JS_InitClass(c, global, dom_rt->dom_node_proto, &dom_rt->domTextClass, 0, 0, textProps, textFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] text class initialized\n"));
	}
	
	/*event API*/
	{
		JSFunctionSpec eventFuncs[] = {
			{"stopPropagation", event_stop_propagation, 0},
			{"stopImmediatePropagation", event_stop_immediate_propagation, 0},
			{"preventDefault", event_prevent_default, 0},
#if 0
			{"initEvent", event_prevent_default, 3},
			{"initEventNS", event_prevent_default, 4},
#endif
			{0},
		};
		JSPropertySpec eventProps[] = {
			{"type",			 0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"target",			 1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"currentTarget",	 2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"eventPhase",		 3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"bubbles",			 4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"cancelable",		 5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"timeStamp",		 6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"namespaceURI",	 7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"defaultPrevented", 8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},

			/*UIEvent*/
			{"detail",			20,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*text, connectionEvent*/
			{"data",			25,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*MouseEvent*/
			{"screenX",			30,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"screenY",			31,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"clientX",			32,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"clientY",			33,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"button",			34,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"relatedTarget",	35,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*wheelEvent*/
			{"wheelDelta",		36,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},

			/*keyboard*/
			{"keyIdentifier",	40,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"keyChar",			41,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"charCode",		42,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
		
			/*progress*/
			{"lengthComputable",50,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"typeArg",			51,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"loaded",			52,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"total",			53,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"bufferLevelValid",	54,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"bufferLevel",			55,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"bufferRemainingTime",	56,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"status",				57,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},

			/*used by vrml*/
			{"width",			60,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"height",			61,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"translation_x",	62,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"translation_y",	63,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},

			
			{0},
		};
		dom_rt->dom_event_proto = JS_InitClass(c, global, 0, &dom_rt->domEventClass, 0, 0, eventProps, eventFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] Event class initialized\n"));

		JS_DefineProperty(c, dom_rt->dom_event_proto, "CAPTURING_PHASE", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->dom_event_proto, "AT_TARGET", INT_TO_JSVAL(2), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->dom_event_proto, "BUBBLING_PHASE", INT_TO_JSVAL(3), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);

		JS_DefineProperty(c, dom_rt->dom_event_proto, "DOM_KEY_LOCATION_STANDARD ", INT_TO_JSVAL(0), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->dom_event_proto, "DOM_KEY_LOCATION_LEFT", INT_TO_JSVAL(1), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->dom_event_proto, "DOM_KEY_LOCATION_RIGHT", INT_TO_JSVAL(2), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, dom_rt->dom_event_proto, "DOM_KEY_LOCATION_NUMPAD", INT_TO_JSVAL(3), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	}


	{
		JSFunctionSpec nodeListFuncs[] = {
			{"item",					dom_nodelist_item, 1},
			{0}
		};
		JSPropertySpec nodeListProps[] = {
			{"length",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		JS_InitClass(c, global, 0, &dom_rt->domNodeListClass, 0, 0, nodeListProps, nodeListFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] nodeList class initialized\n"));
	}
	
	{
		JSPropertySpec xmlHTTPRequestClassProps[] = {
			{"onreadystatechange",	0,       JSPROP_ENUMERATE | JSPROP_PERMANENT  },
			{"readyState",			1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"responseText",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"responseXML",			3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"status",				4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"statusText",			5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		JSFunctionSpec xmlHTTPRequestClassFuncs[] = {
			{"open",					xml_http_open, 2},
			{"setRequestHeader",		xml_http_set_header, 2},
			{"send",					xml_http_send,          0},
			{"abort",					xml_http_abort, 0},
			{"getAllResponseHeaders",	xml_http_get_all_headers, 0},
			{"getResponseHeader",		xml_http_get_header, 1},
			/*todo - addEventListener and removeEventListener*/
			{0}
		};
		JS_InitClass(c, global, 0, &dom_rt->xmlHTTPRequestClass, xml_http_constructor, 0, xmlHTTPRequestClassProps, xmlHTTPRequestClassFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] XMLHttpRequest class initialized\n"));
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
				{"value",							0,       JSPROP_ENUMERATE | JSPROP_PERMANENT  },
				{"valueType",						1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
				{"propertyType",					2,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
				{"readOnly",						3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
				{"DCCIMetadataInterfaceType",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
				{"DCCIMetadataInterface",			5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
				{"version",							6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
				{0}
			};
			JSFunctionSpec DCCIClassFuncs[] = {
				{"hasProperty",			dcci_has_property, 3},
				{"searchProperty",		dcci_search_property, 4},
				{0}
			};

			JS_InitClass(c, global, dom_rt->dom_element_proto, &dom_rt->DCCIClass, 0, 0, DCCIClassProps, DCCIClassFuncs, 0, 0);
			dcci_root = dom_base_node_construct(c, &dom_rt->DCCIClass, dcci->RootNode);
			JS_DefineProperty(c, global, "DCCIRoot", dcci_root, 0, 0, JSPROP_READONLY | JSPROP_PERMANENT );

			dcci->dcci_doc = 1;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] DCCI class initialized\n"));
		}
	}
}

void dom_js_unload()
{
	if (!dom_rt) return;
	dom_rt->nb_inst--;
	if (!dom_rt->nb_inst) {
		free(dom_rt);
		dom_rt = NULL;
	}
}

static void dom_js_define_document_ex(JSContext *c, JSObject *global, GF_SceneGraph *doc, const char *name)
{
	JSClass *__class;
	JSObject *obj;
	if (!doc || !doc->RootNode) return;

	if (doc->reference_count)
		doc->reference_count++;

	__class = &dom_rt->domDocumentClass;
	if (dom_rt->get_document_class)
		__class = dom_rt->get_document_class(doc);

	obj = JS_DefineObject(c, global, name, __class, 0, 0 );
	gf_node_register(doc->RootNode, NULL);
	JS_SetPrivate(c, obj, doc);
	doc->document = obj;
}

void dom_js_define_document(JSContext *c, JSObject *global, GF_SceneGraph *doc)
{
	dom_js_define_document_ex(c, global, doc, "document");
}

JSObject *dom_js_define_event(JSContext *c, JSObject *global)
{
	JSObject *obj = JS_DefineObject(c, global, "evt", &dom_rt->domEventClass, 0, 0 );
	JS_AliasProperty(c, global, "evt", "event");
	return obj;
}

JSObject *gf_dom_new_event(JSContext *c)
{
	return JS_NewObject(c, &dom_rt->domEventClass, 0, 0);
}
JSObject *dom_js_get_node_proto(JSContext *c) { return dom_rt->dom_node_proto; }
JSObject *dom_js_get_element_proto(JSContext *c) { return dom_rt->dom_element_proto; }
JSObject *dom_js_get_document_proto(JSContext *c) { return dom_rt->dom_document_proto; }
JSObject *dom_js_get_event_proto(JSContext *c) { return dom_rt->dom_event_proto; }

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
	free(ctx);

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
	Bool is_root = 0;
	Bool modified = 0;
	Bool atts_modified = 0;
	Bool new_node = 0;
	u32 i;
	GF_DOMFullAttribute *prev = NULL;
	GF_DOMFullNode *par = NULL;
	XMLReloadContext *ctx = (XMLReloadContext *)sax_cbck;
	GF_DOMFullNode *node;
	
	node = gf_list_last(ctx->node_stack);
	if (!node) {
		is_root = 1;
		node = (GF_DOMFullNode *) ctx->document->RootNode;
	}

	if (is_root) {
		Bool same_node = 1;
		if (strcmp(node->name, node_name)) same_node = 0;
		if (node->ns && name_space && strcmp(gf_sg_get_namespace(node->sgprivate->scenegraph, node->ns), name_space)) same_node = 0;

		if (!same_node) {
			if (node->name) free(node->name);
			node->name = strdup(node_name);
			node->ns = 0;
			if (name_space) {
				gf_sg_add_namespace(node->sgprivate->scenegraph, (char *) name_space, NULL);
				node->ns = gf_sg_get_namespace_code_from_name(node->sgprivate->scenegraph, (char*)name_space);
			}
			modified = 1;
		}
	} else {
		GF_ChildNodeItem *child = node->children;
		node = NULL;
		/*locate the node in the children*/
		while (child) {
			Bool same_node = 1;
			node = (GF_DOMFullNode*)child->node;
			if (strcmp(node->name, node_name)) same_node = 0;
			if (node->ns && name_space && strcmp(gf_sg_get_namespace(node->sgprivate->scenegraph, node->ns), name_space)) same_node = 0;
			if (same_node) {
				break;
			}
			node = NULL;
			child = child->next;
		}
		if (!node) {
			modified = 1;

			/*create the new node*/
			node = (GF_DOMFullNode *) gf_node_new(ctx->document, TAG_DOMFullNode);
			node->name = strdup(node_name);
			if (name_space) {
				gf_sg_add_namespace(node->sgprivate->scenegraph, (char *)name_space, NULL);
				node->ns = gf_sg_get_namespace_code(node->sgprivate->scenegraph, (char *)name_space);
			}

			par = gf_list_last(ctx->node_stack);
			gf_node_register((GF_Node*)node, (GF_Node*)par);
			gf_node_list_add_child(&par->children, (GF_Node*)node);
			new_node = 1;
		}
	}
	gf_list_add(ctx->node_stack, node);

	if (!modified) {
		u32 count = 0;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute *)node->attributes;
		while (att) {
			Bool found = 0;
			for (i=0; i<nb_attributes; i++) {
				if (!stricmp(attributes[i].name, "xml:id")) {
					const char *id = gf_node_get_name((GF_Node*)node);
					if (!id || strcmp(id, attributes[i].value))
						modified = 1;
				} else if (!strcmp(att->name, attributes[i].name)) {
					found = 1;
					if (strcmp(att->data, attributes[i].value)) {
						atts_modified = 1;
						free(att->data);
						att->data = strdup(attributes[i].value);
					}
				}
			}
			if (!found) {
				modified = 1;
				break;
			}
			count++;
			att = (GF_DOMFullAttribute *)att->next;
		}
		if (count != nb_attributes)
			modified = 1;
	}


	if (modified) {
		GF_DOMFullAttribute *tmp, *att;
		att = (GF_DOMFullAttribute *)node->attributes;
		while (att) {
			if (att->name) free(att->name);
			if (att->data) free(att->data);
			tmp = att;
			att = (GF_DOMFullAttribute *)att->next;
			free(tmp);
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
				att->name = strdup(attributes[i].name);
				att->data = strdup(attributes[i].value);
				if (prev) prev->next = (GF_DOMAttribute*)att;
				else node->attributes = (GF_DOMAttribute*)att;
				prev = att;
			}
		}
		atts_modified = 1;
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
	GF_DOMFullNode *par = gf_list_last(ctx->node_stack);
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
	GF_DOMFullNode *par = gf_list_last(ctx->node_stack);
	if (!par) return;

	/*basic check, remove all empty text nodes*/
	len = strlen(content);
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
			if (txt->textContent) free(txt->textContent);
			txt->textContent = strdup(content);
			txt->type = is_cdata ? GF_DOM_TEXT_CDATA : GF_DOM_TEXT_REGULAR;
			break;
		}
		child = child->next;
	}
	if (!txt) {
		txt = gf_dom_add_text_node((GF_Node *)par, strdup(content) );
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
