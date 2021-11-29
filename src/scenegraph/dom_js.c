/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2019
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
#include "qjs_common.h"

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

#ifdef GPAC_HAS_QJS

#include <gpac/html5_mse.h>

#ifdef GPAC_CONFIG_ANDROID
#ifndef XP_UNIX
#define XP_UNIX
#endif /* XP_UNIX */
#endif

#include <gpac/html5_media.h>

/************************************************************
 *
 *				DOM3 core implementation
 *
 *	This is NOT a full dom implementation :)
 *
 *************************************************************/

GF_JSClass domDocumentClass;
GF_JSClass domNodeClass;
GF_JSClass domElementClass;
GF_JSClass domTextClass;
GF_JSClass domNodeListClass;
GF_JSClass domEventClass;
GF_JSClass htmlMediaElementClass;


typedef struct
{
	u32 nb_inst;

	GF_List *handlers;
} GF_DOMRuntime;

static GF_DOMRuntime *dom_rt = NULL;

typedef enum {
	NODE_JSPROPERTY_NODENAME				= -1,
	NODE_JSPROPERTY_NODEVALUE				= -2,
	NODE_JSPROPERTY_NODETYPE				= -3,
	NODE_JSPROPERTY_PARENTNODE				= -4,
	NODE_JSPROPERTY_CHILDNODES				= -5,
	NODE_JSPROPERTY_FIRSTCHILD				= -6,
	NODE_JSPROPERTY_LASTCHILD				= -7,
	NODE_JSPROPERTY_PREVIOUSSIBLING			= -8,
	NODE_JSPROPERTY_NEXTSIBLING				= -9,
	NODE_JSPROPERTY_ATTRIBUTES				= -10,
	NODE_JSPROPERTY_OWNERDOCUMENT			= -11,
	NODE_JSPROPERTY_NAMESPACEURI			= -12,
	NODE_JSPROPERTY_PREFIX					= -13,
	NODE_JSPROPERTY_LOCALNAME				= -14,
	NODE_JSPROPERTY_BASEURI					= -15,
	NODE_JSPROPERTY_TEXTCONTENT				= -16,
	NODE_JSPROPERTY_FIRSTELEMENTCHILD		= -17,
	NODE_JSPROPERTY_LASTELEMENTCHILD		= -18,
	NODE_JSPROPERTY_PREVIOUSELEMENTSIBLING	= -19,
	NODE_JSPROPERTY_NEXTELEMENTSIBLING		= -20
} GF_DOMNodeJSProperty;

typedef enum {
	DOCUMENT_JSPROPERTY_DOCTYPE				= -1,
	DOCUMENT_JSPROPERTY_IMPLEMENTATION		= -2,
	DOCUMENT_JSPROPERTY_DOCUMENTELEMENT		= -3,
	DOCUMENT_JSPROPERTY_INPUTENCODING		= -4,
	DOCUMENT_JSPROPERTY_XMLENCODING			= -5,
	DOCUMENT_JSPROPERTY_XMLSTANDALONE		= -6,
	DOCUMENT_JSPROPERTY_XMLVERSION			= -7,
	DOCUMENT_JSPROPERTY_STRICTERRORCHECKING	= -8,
	DOCUMENT_JSPROPERTY_DOCUMENTURI			= -9,
	DOCUMENT_JSPROPERTY_LOCATION			= -10,
	DOCUMENT_JSPROPERTY_DOMCONFIG			= -11,
	DOCUMENT_JSPROPERTY_GLOBAL				= -12
} GF_DOMDocumentJSProperty;

typedef enum {
	ELEMENT_JSPROPERTY_TAGNAME			= -1,
	ELEMENT_JSPROPERTY_SCHEMATYPEINFO	= -2,
} GF_DOMElementJSProperty;

typedef enum {
	DCCI_JSPROPERTY_VALUE						= -1,
	DCCI_JSPROPERTY_VALUETYPE					= -2,
	DCCI_JSPROPERTY_PROPERTYTYPE				= -3,
	DCCI_JSPROPERTY_READONLY					= -4,
	DCCI_JSPROPERTY_DCCIMETADATAINTERFACETYPE	= -5,
	DCCI_JSPROPERTY_DCCIMETADATAINTERFACE		= -6,
	DCCI_JSPROPERTY_VERSION						= -7,
} GF_DCCIJSProperty;

typedef enum {
	NODELIST_JSPROPERTY_LENGTH = -1,
} GF_DOMNodeListJSProperty;

typedef enum {
	EVENT_JSPROPERTY_TYPE						= -1,
	EVENT_JSPROPERTY_TARGET						= -2,
	EVENT_JSPROPERTY_CURRENTTARGET				= -3,
	EVENT_JSPROPERTY_EVENTPHASE					= -4,
	EVENT_JSPROPERTY_BUBBLES					= -5,
	EVENT_JSPROPERTY_CANCELABLE					= -6,
	EVENT_JSPROPERTY_TIMESTAMP					= -7,
	EVENT_JSPROPERTY_NAMESPACEURI				= -8,
	EVENT_JSPROPERTY_DEFAULTPREVENTED			= -9,
	EVENT_JSPROPERTY_DETAIL						= -10,
	EVENT_JSPROPERTY_DATA						= -11,
	EVENT_JSPROPERTY_SCREENX					= -12,
	EVENT_JSPROPERTY_SCREENY					= -13,
	EVENT_JSPROPERTY_CLIENTX					= -14,
	EVENT_JSPROPERTY_CLIENTY					= -15,
	EVENT_JSPROPERTY_BUTTON						= -16,
	EVENT_JSPROPERTY_RELATEDTARGET				= -17,
	EVENT_JSPROPERTY_WHEELDELTA					= -18,
	EVENT_JSPROPERTY_KEYIDENTIFIER				= -19,
	EVENT_JSPROPERTY_KEYCHAR					= -20,
	EVENT_JSPROPERTY_CHARCODE					= -21,
	EVENT_JSPROPERTY_LENGTHCOMPUTABLE			= -22,
	EVENT_JSPROPERTY_TYPEARG					= -23,
	EVENT_JSPROPERTY_LOADED						= -24,
	EVENT_JSPROPERTY_TOTAL						= -25,
	EVENT_JSPROPERTY_BUFFER_ON					= -26,
	EVENT_JSPROPERTY_BUFFERLEVEL				= -27,
	EVENT_JSPROPERTY_BUFFERREMAININGTIME		= -28,
	EVENT_JSPROPERTY_STATUS						= -29,
	EVENT_JSPROPERTY_WIDTH						= -30,
	EVENT_JSPROPERTY_HEIGHT						= -31,
	EVENT_JSPROPERTY_OFFSETX					= -32,
	EVENT_JSPROPERTY_OFFSETY					= -33,
	EVENT_JSPROPERTY_VPWIDTH					= -34,
	EVENT_JSPROPERTY_VPHEIGHT					= -35,
	EVENT_JSPROPERTY_TRANSLATIONX				= -36,
	EVENT_JSPROPERTY_TRANSLATIONY				= -37,
	EVENT_JSPROPERTY_TYPE3D						= -38,
	EVENT_JSPROPERTY_ERROR						= -39,
	EVENT_JSPROPERTY_DYNAMIC_SCENE				= -40,
	EVENT_JSPROPERTY_URL							= -41,
} GF_DOMEventJSProperty;

typedef enum {
	TEXT_JSPROPERTY_DATA						= -1,
	TEXT_JSPROPERTY_LENGTH						= -2,
	TEXT_JSPROPERTY_ISELEMENTCONTENTWHITESPACE	= -3,
	TEXT_JSPROPERTY_WHOLETEXT					= -4,
} GF_DOMTextJSProperty;



JSValue xml_dom3_not_implemented(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return js_throw_err(ctx, GF_DOM_EXC_NOT_SUPPORTED_ERR);
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

static void define_dom_exception(JSContext *c, JSValue global)
{
#define DEF_EXC(_val)	\
	JS_SetPropertyStr(c, obj, #_val, JS_NewInt32(c, GF_DOM_EXC_##_val));

	JSValue obj = JS_NewObject(c);
	JS_SetPropertyStr(c, global, "DOMException", obj);

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
	obj = JS_NewObject(c);
	JS_SetPropertyStr(c, global, "EventException", obj);
	JS_SetPropertyStr(c, obj, "UNSPECIFIED_EVENT_TYPE_ERR", JS_NewInt32(c, 0));
	JS_SetPropertyStr(c, obj, "DISPATCH_REQUEST_ERR", JS_NewInt32(c, 1));
}


GF_Node *dom_get_node(JSValue obj)
{
	GF_Node *n = (GF_Node *) JS_GetOpaque_Nocheck(obj);
	if (n && n->sgprivate) return n;
	return NULL;
}

GF_Node *dom_get_element(JSContext *c, JSValue obj)
{
	GF_Node *n = (GF_Node *) JS_GetOpaque_Nocheck(obj);
	if (!n || !n->sgprivate) return NULL;
	if (n->sgprivate->tag==TAG_DOMText) return NULL;
	return n;
}

GF_SceneGraph *dom_get_doc(JSContext *c, JSValue obj)
{
	GF_SceneGraph *sg = (GF_SceneGraph *) JS_GetOpaque_Nocheck(obj);
	if (sg && !sg->__reserved_null) return sg;
	return NULL;
}
typedef struct _dom_js_data
{
	JSValue document;
} GF_DOMJSData;

static void dom_js_define_document_ex(JSContext *c, JSValue global, GF_SceneGraph *doc, const char *name)
{
	JSClassID __class = 0;
	JSValue obj;
	GF_SceneGraph *par_sg;
	if (!doc || !doc->RootNode) return;

	if (doc->reference_count)
		doc->reference_count++;
	//no need to register root once more, it is already registered with graph
//	gf_node_register(doc->RootNode, NULL);
	par_sg = doc;
	while (par_sg && !par_sg->get_document_class)
		par_sg = par_sg->parent_scene;

	if (par_sg && par_sg->get_document_class)
		__class = par_sg->get_document_class(doc);

	if (!__class)
		__class = domDocumentClass.class_id;

	obj = JS_NewObjectClass(c, __class);
	JS_SetOpaque(obj, doc);
	GF_SAFEALLOC(doc->js_data, GF_DOMJSData);
	if (doc->js_data)
		doc->js_data->document = JS_DupValue(c, obj);

	JS_SetPropertyStr(c, global, name, obj);
}

void dom_js_define_document(JSContext *c, JSValue global, GF_SceneGraph *doc)
{
	dom_js_define_document_ex(c, global, doc, "document");
}

/* Constructs a new document based on the given context and scene graph, used for documents in XHR */
JSValue dom_document_construct(JSContext *c, GF_SceneGraph *sg)
{
	JSClassID __class=0;
	JSValue new_obj;
	GF_SceneGraph *par_sg = sg;
	if (!dom_rt) return GF_JS_EXCEPTION(c);
	if (sg->js_data) return JS_DupValue(c, sg->js_data->document);

	if (sg->reference_count)
		sg->reference_count++;
	//no need to register root once more, it is already registered with graph
//	gf_node_register(sg->RootNode, NULL);

	while (par_sg && !par_sg->get_element_class) {
		par_sg = par_sg->parent_scene;
	}

	if (par_sg && par_sg->get_document_class)
		__class = par_sg->get_document_class(sg);

	if (!__class)
		__class = domDocumentClass.class_id;

	new_obj = JS_NewObjectClass(c, __class);
	JS_SetOpaque(new_obj, sg);
	GF_SAFEALLOC(sg->js_data, GF_DOMJSData);
	if (sg->js_data)
		sg->js_data->document = JS_DupValue(c, new_obj);
	return new_obj;
}

JSValue dom_document_construct_external(JSContext *c, GF_SceneGraph *sg)
{
	return dom_document_construct(c, sg);

}

static JSValue dom_base_node_construct(JSContext *c, JSClassID class_id, GF_Node *n)
{
	GF_SceneGraph *sg;
	JSValue new_obj;
	if (!n || !n->sgprivate->scenegraph) return JS_NULL;
	if (n->sgprivate->tag<TAG_DOMText) return JS_NULL;

	sg = n->sgprivate->scenegraph;

	if (n->sgprivate->interact && n->sgprivate->interact->js_binding && !JS_IsUndefined(n->sgprivate->interact->js_binding->obj) ) {
		return JS_DupValue(c, n->sgprivate->interact->js_binding->obj);
	}

	if (n->sgprivate->scenegraph->reference_count)
		n->sgprivate->scenegraph->reference_count ++;

	gf_node_register(n, NULL);
	new_obj = JS_NewObjectClass(c, class_id);
	JS_SetOpaque(new_obj, n);

	if (n->sgprivate->tag == TAG_SVG_video || n->sgprivate->tag == TAG_SVG_audio)
	{
#ifdef GPAC_ENABLE_HTML5_MEDIA
		html_media_element_js_init(c, new_obj, n);
#endif

	}
	if (!n->sgprivate->interact) {
		GF_SAFEALLOC(n->sgprivate->interact, struct _node_interactive_ext);
		if (!n->sgprivate->interact) {
			return JS_NULL;
		}
	}
	if (!n->sgprivate->interact->js_binding) {
		GF_SAFEALLOC(n->sgprivate->interact->js_binding, struct _node_js_binding);
		if (!n->sgprivate->interact->js_binding) {
			return JS_NULL;
		}
	}
	n->sgprivate->interact->js_binding->obj = JS_DupValue(c, new_obj);
	gf_list_add(sg->objects, n->sgprivate->interact->js_binding);
	return new_obj;
}
static JSValue dom_node_construct(JSContext *c, GF_Node *n)
{
	JSClassID __class = 0;
	GF_SceneGraph *par_sg;
	if (!n) return JS_NULL;
	par_sg = n->sgprivate->scenegraph;
	while (par_sg && !par_sg->get_element_class)
		par_sg = par_sg->parent_scene;

	if (par_sg && par_sg->get_element_class)
		__class = par_sg->get_element_class(n);

	if (!__class)
		__class = domElementClass.class_id;

	/*in our implementation ONLY ELEMENTS are created, never attributes. We therefore always
	create Elements when asked to create a node !!*/
	return dom_base_node_construct(c, __class, n);
}

JSValue dom_element_construct(JSContext *c, GF_Node *n)
{
	JSClassID __class = 0;
	GF_SceneGraph *par_sg;
	if (!n) return JS_NULL;
	par_sg = n->sgprivate->scenegraph;
	while (par_sg && !par_sg->get_element_class)
		par_sg = par_sg->parent_scene;

	if (par_sg && par_sg->get_element_class)
		__class = par_sg->get_element_class(n);

	if (!__class)
		__class = domElementClass.class_id;

	return dom_base_node_construct(c, __class, n);
}

static JSValue dom_text_construct(JSContext *c, GF_Node *n)
{
	return dom_base_node_construct(c, domTextClass.class_id, n);
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

JSValue dom_node_get_sibling(JSContext *c, GF_Node *n, Bool is_prev, Bool elt_only)
{
	GF_Node *val;
	GF_ChildNodeItem *child;
	s32 idx, cur;
	GF_ParentNode *par;
	if (!n) return JS_NULL;
	par = (GF_ParentNode *)gf_node_get_parent(n, 0);
	if (!par) return JS_NULL;

	idx = gf_node_list_find_child(par->children, n);
	if (idx<0) return JS_NULL;

	if (!elt_only) {
		if (is_prev) {
			idx--;
			if (idx<0) return JS_NULL;
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


static JSValue dom_nodelist_construct(JSContext *c, GF_ParentNode *n)
{
	DOMNodeList *nl;
	JSValue new_obj;
	if (!n) return JS_NULL;
	GF_SAFEALLOC(nl, DOMNodeList);
	if (!nl) return GF_JS_EXCEPTION(c);
	
	nl->owner = n;
	if (n->sgprivate->scenegraph->reference_count)
		n->sgprivate->scenegraph->reference_count++;

	gf_node_register((GF_Node*)n, NULL);
	new_obj = JS_NewObjectClass(c, domNodeListClass.class_id);
	JS_SetOpaque(new_obj, nl);
	return new_obj;
}

static void dom_nodelist_finalize(JSRuntime *rt, JSValue obj)
{
	DOMNodeList *nl = JS_GetOpaque(obj, domNodeListClass.class_id);
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

static JSValue dom_nodelist_item(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Node *n;
	u32 count;
	s32 idx;
	DOMNodeList *nl;

	nl = (DOMNodeList *) JS_GetOpaque(obj, domNodeListClass.class_id);
	if (!nl || (argc!=1) || JS_ToInt32(c, &idx, argv[0]))
		return GF_JS_EXCEPTION(c);

	count = gf_node_list_get_count(nl->owner ? nl->owner->children : nl->child);
	if ((idx<0) || ((u32) idx>=count)) {
		return JS_NULL;
	}
	n = gf_node_list_get_child(nl->owner ? nl->owner->children : nl->child, idx);
	return dom_node_construct(c, n);
}

static JSValue dom_nodelist_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	DOMNodeList *nl;
	u32 count;
	nl = (DOMNodeList *) JS_GetOpaque(obj, domNodeListClass.class_id);
	if (!nl) return GF_JS_EXCEPTION(c);
	count = gf_node_list_get_count(nl->owner ? nl->owner->children : nl->child);

	switch (magic) {
	case NODELIST_JSPROPERTY_LENGTH:
		return JS_NewInt32(c, count);
	default:
		break;
	}
	return js_throw_err(c, GF_DOM_EXC_SYNTAX_ERR);
}




static void dom_handler_remove(GF_Node *node, void *rs, Bool is_destroy)
{
	if (is_destroy) {
		SVG_handlerElement *handler = (SVG_handlerElement *)node;
		if (handler->js_data) {
		 	if (handler->js_data->ctx && !JS_IsUndefined(handler->js_data->fun_val)) {
				/*unprotect the function*/
				JS_FreeValue(handler->js_data->ctx, handler->js_data->fun_val);
				gf_list_del_item(dom_rt->handlers, handler);
			}
			gf_free(handler->js_data);
			handler->js_data = NULL;
		}
	}
}

static void sg_js_get_event_target(JSContext *c, JSValue obj, GF_EventType evtType, GF_Node *vrml_node,
                                     GF_SceneGraph **sg, GF_DOMEventTarget **target, GF_Node **n)
{
	Bool is_svg_document_class(JSContext *c, JSValue obj);
	Bool is_svg_element_class(JSContext *c, JSValue obj);
	Bool gf_mse_is_mse_object(JSContext *c, JSValue obj);
	*target = NULL;
	*sg = NULL;
	*n = NULL;

#ifdef GPAC_ENABLE_HTML5_MEDIA
	if (gf_dom_event_get_category(evtType) == GF_DOM_EVENT_MEDIA) {
		void gf_html_media_get_event_target(JSContext *c, JSValue obj, GF_DOMEventTarget **target, GF_SceneGraph **sg);
		gf_html_media_get_event_target(c, obj, target, sg);
		if (*target && *sg) return;
	}

	if (gf_dom_event_get_category(evtType) == GF_DOM_EVENT_MEDIASOURCE) {
		void gf_mse_get_event_target(JSContext *c, JSValue obj, GF_DOMEventTarget **target, GF_SceneGraph **sg);
		gf_mse_get_event_target(c, obj, target, sg);
		if (*target && *sg) return;
	}
#endif


	if (JS_GetOpaque(obj, domDocumentClass.class_id) || is_svg_document_class(c, obj)) {
		/*document interface*/
		*sg = dom_get_doc(c, obj);
		if (*sg) {
#ifndef GPAC_DISABLE_SVG
			*target = (*sg)->dom_evt;
#else
			return;
#endif
		} else {
			return;
		}
	} else if (JS_GetOpaque(obj, domElementClass.class_id) || is_svg_element_class(c, obj) || vrml_node) {
		/*Element interface*/
		if (vrml_node) {
			*n = vrml_node;
		} else {
			*n = dom_get_element(c, obj);
		}
		if (*n) {
			*sg = (*n)->sgprivate->scenegraph;
			if (!(*n)->sgprivate->interact->dom_evt) {
				(*n)->sgprivate->interact->dom_evt = gf_dom_event_target_new(GF_DOM_EVENT_TARGET_NODE, *n);
			}
			*target = (*n)->sgprivate->interact->dom_evt;
		}
	} else {
		void xhr_get_event_target(JSContext *c, JSValue obj, GF_SceneGraph **sg, GF_DOMEventTarget **target);

		xhr_get_event_target(c, obj, sg, target);
	}
}

static GF_Err sg_js_parse_event_args(JSContext *c, JSValue obj, int argc, JSValue *argv,
                                     GF_EventType *evtType,
                                     char **callback, JSValue *funval, JSValue *evt_handler) {
	u32 offset = 0;
	const char *type = NULL;
	const char *inNS = NULL;

	*evtType = GF_EVENT_UNKNOWN;
	*callback = NULL;
	*funval = JS_NULL;
	if (evt_handler) *evt_handler = obj;

	/*NS version (4 args in DOM2, 5 in DOM3 for evt_group param)*/
	if (argc>=4) {
		if (!JS_CHECK_STRING(argv[0])) return GF_BAD_PARAM;
		inNS = JS_ToCString(c, argv[0]);
		offset = 1;
	}

	if (!JS_CHECK_STRING(argv[offset])) goto err_exit;
	type = JS_ToCString(c, argv[offset]);

	if (JS_CHECK_STRING(argv[offset+1])) {
		const char *str = JS_ToCString(c, argv[offset+1]);
		if (!str) goto err_exit;
		*callback = gf_strdup(str);
		JS_FreeCString(c, str);
	} else if (JS_IsFunction(c, argv[offset+1])) {
		*funval = argv[offset+1];

	} else if (JS_IsObject(argv[offset+1])) {
		JSValue evt_fun;
		if (evt_handler) {
			*evt_handler = JS_DupValue(c, argv[offset+1]);
		} else {
			goto err_exit;
		}
		evt_fun = JS_GetPropertyStr(c, *evt_handler, "handleEvent");
		if (!JS_IsFunction(c, evt_fun) ) goto err_exit;
		*funval = evt_fun;
	}

	*evtType = gf_dom_event_type_by_name(type);
	if (*evtType==GF_EVENT_UNKNOWN) goto err_exit;

	JS_FreeCString(c, type);
	JS_FreeCString(c, inNS);
	return GF_OK;

err_exit:
	if (type) JS_FreeCString(c, type);
	if (inNS) JS_FreeCString(c, inNS);
	if (callback) gf_free(*callback);
	*callback = NULL;
	*evtType = GF_EVENT_UNKNOWN;
	*funval = JS_NULL;
	if (evt_handler) *evt_handler = JS_UNDEFINED;
	return GF_BAD_PARAM;
}

void svg_execute_handler(GF_Node *node, GF_DOM_Event *event, GF_Node *observer);

static GF_Node *create_listener(GF_SceneGraph *sg, GF_EventType evtType, GF_Node *n, GF_Node *vrml_node,
                                JSContext *c, char *callback, JSValue funval, JSValue evt_handler)
{
	GF_FieldInfo info;
	GF_Node *listener;
	SVG_handlerElement *handler;

	listener = gf_node_new(sg, TAG_SVG_listener);
	/*we don't register the listener with the parent node , it will be registered
	on gf_dom_listener_add*/

	/*!!! create the handler in the scene owning the script context !!! */
	handler = (SVG_handlerElement *) gf_node_new(sg, TAG_SVG_handler);
	/*we register the handler with the listener node to avoid modifying the DOM*/
	gf_node_register((GF_Node *)handler, listener);
	gf_node_list_add_child(& ((GF_ParentNode *)listener)->children, (GF_Node*)handler);

	if (!callback) {
		GF_SAFEALLOC(handler->js_data, struct js_handler_context)
		if (handler->js_data) {
			handler->js_data->fun_val = funval;
			handler->js_data->ctx = c;
			if (JS_IsFunction(c, funval)) {
				/*protect the function - we don't know how it was passed to us, so prevent it from being GCed*/
				handler->js_data->fun_val = JS_DupValue(c, funval);
				handler->sgprivate->UserCallback = dom_handler_remove;
				gf_list_add(dom_rt->handlers, handler);
				handler->handle_event = svg_execute_handler;
			}
			handler->js_data->evt_listen_obj = evt_handler;
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
	if (handler->sgprivate->scenegraph->svg_js && !handler->handle_event)
		handler->handle_event = gf_sg_handle_dom_event;
#endif

	if (vrml_node) {
		assert(handler->js_data);
		handler->js_data->ctx = c;
#ifndef GPAC_DISABLE_VRML
		if (vrml_node->sgprivate->tag <= GF_NODE_RANGE_LAST_VRML)
			handler->handle_event = gf_sg_handle_dom_event_for_vrml;
#endif
	}

	return listener;
}

/*eventListeners routines used by document, element and XHR interfaces*/
JSValue gf_sg_js_event_add_listener(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, GF_Node *vrml_node)
{
	GF_DOMEventTarget *target = NULL;
	GF_Node *listener = NULL;
	GF_EventType evtType = GF_EVENT_UNKNOWN;
	GF_SceneGraph *sg = NULL;
	char *callback = NULL;
	JSValue funval = JS_UNDEFINED;
	GF_Node *n = NULL;
	JSValue evt_handler = JS_UNDEFINED;
	GF_Err e;

	/* Determine the event type and handler params */
	e = sg_js_parse_event_args(c, obj, argc, argv, &evtType, &callback, &funval, &evt_handler);
	if (e != GF_OK) goto err_exit;

	/* First retrieve the scenegraph and the GF_DOMEventTarget object */
	sg_js_get_event_target(c, obj, evtType, vrml_node, &sg, &target, &n);
	if (!sg || !target) goto err_exit;

	listener = create_listener(sg, evtType, n, vrml_node, c, callback, funval, evt_handler);
	if (!listener) goto err_exit;

	/*don't add listener directly, post it and wait for event processing*/
	if (n) {
		gf_sg_listener_post_add((GF_Node *) n, listener);
	} else {
		gf_sg_listener_associate(listener, target);
	}

err_exit:
	if (callback) gf_free(callback);
	if (e)
		return js_throw_err(c, GF_DOM_EXC_SYNTAX_ERR);
	return JS_UNDEFINED;
}


JSValue dom_event_add_listener(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return gf_sg_js_event_add_listener(c, obj, argc, argv, NULL);
}

JSValue gf_sg_js_event_remove_listener(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv, GF_Node *vrml_node)
{
#ifndef GPAC_DISABLE_SVG
	char *callback = NULL;
	GF_EventType evtType = GF_EVENT_UNKNOWN;
	u32 i, count;
	GF_Node *node = NULL;
	JSValue funval = JS_UNDEFINED;
	GF_SceneGraph *sg = NULL;
	GF_DOMEventTarget *target = NULL;
	GF_Err e;

	/* Determine the event type and handler params */
	e = sg_js_parse_event_args(c, obj, argc, argv, &evtType, &callback, &funval, NULL);
	if (e != GF_OK) goto err_exit;

	/* First retrieve the scenegraph and the GF_DOMEventTarget object */
	sg_js_get_event_target(c, obj, evtType, vrml_node, &sg, &target, &node);
	if (!sg || !target) return JS_TRUE;

	/*flush all pending add_listener*/
	gf_dom_listener_process_add(sg);

	count = gf_list_count(target->listeners);
	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		GF_DOMText *txt;
		SVG_handlerElement *hdl;
		GF_Node *el = (GF_Node *)gf_list_get(target->listeners, i);

		gf_node_get_attribute_by_tag(el, TAG_XMLEV_ATT_event, GF_FALSE, GF_FALSE, &info);
		if (!info.far_ptr) continue;
		if (((XMLEV_Event*)info.far_ptr)->type != evtType) continue;

		gf_node_get_attribute_by_tag(el, TAG_XMLEV_ATT_handler, GF_FALSE, GF_FALSE, &info);
		if (!info.far_ptr) continue;
		hdl = (SVG_handlerElement *) ((XMLRI*)info.far_ptr)->target;
		if (!hdl) continue;
		if (! JS_IsNull(funval) ) {
			Bool is_same = GF_FALSE;

			const char * f1 = JS_ToCString(c, funval);
			const char * f2 = JS_ToCString(c, funval);
			if (f1 && f2 && !strcmp(f1, f2)) is_same = GF_TRUE;
			JS_FreeCString(c, f1);
			JS_FreeCString(c, f2);
			if (!is_same) continue;
		} else if (hdl->children) {
			txt = (GF_DOMText *) hdl->children->node;
			if (txt->sgprivate->tag != TAG_DOMText) continue;
			if (!txt->textContent || !callback || strcmp(txt->textContent, callback)) continue;
		} else {
			continue;
		}

		/*this will destroy the listener and its child handler*/
		gf_dom_listener_del(el, target);
		break;
	}
#endif

err_exit:
	if (callback) gf_free(callback);
	return JS_UNDEFINED;
}

JSValue dom_event_remove_listener(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	return gf_sg_js_event_remove_listener(c, obj, argc, argv, NULL);
}

/*dom3 node*/
static void dom_node_finalize(JSRuntime *rt, JSValue obj)
{
	GF_Node *n = (GF_Node *) JS_GetOpaque_Nocheck(obj);
	/*the JS proto of the svgClass or a destroyed object*/
	if (!n) return;
	if (!n->sgprivate) return;

	JS_SetOpaque(obj, NULL);
	if (n->sgprivate->interact)
		gf_list_del_item(n->sgprivate->scenegraph->objects, n->sgprivate->interact->js_binding);
	gf_sg_js_dom_pre_destroy(rt, n->sgprivate->scenegraph, n);
	dom_unregister_node(n);
}

static Bool check_dom_parents(JSContext *c, GF_Node *n, GF_Node *parent)
{
	GF_ParentList *par = n->sgprivate->parents;
	if (n->sgprivate->scenegraph != parent->sgprivate->scenegraph) {
		js_throw_err(c, GF_DOM_EXC_WRONG_DOCUMENT_ERR);
		return GF_FALSE;
	}
	while (par) {
		if (par->node==parent) {
			js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
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

static JSValue xml_node_insert_before(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	s32 idx;
	u32 tag;
	GF_Node *n, *target, *new_node;
	GF_ParentNode *par;

	if (!argc || !JS_IsObject(argv[0]))
		return GF_JS_EXCEPTION(c);

	n = dom_get_node(obj);
	if (!n) {
		return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	}

	new_node = dom_get_node(argv[0]);
	if (!new_node)
		return js_throw_err(c, GF_DOM_EXC_SYNTAX_ERR);

	target = NULL;
	if ((argc==2) && JS_IsObject(argv[1]) && !JS_IsNull(argv[1])) {
		target = dom_get_node(argv[1]);
		if (!target) return JS_NULL;
	}
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText)
		return js_throw_err(c, GF_DOM_EXC_SYNTAX_ERR);

	if (!check_dom_parents(c, n, new_node))
		return js_throw_err(c, GF_DOM_EXC_VALIDATION_ERR);
	par = (GF_ParentNode*)n;
	idx = -1;
	if (target) {
		idx = gf_node_list_find_child(par->children, target);
		if (idx<0) {
			return js_throw_err(c, GF_DOM_EXC_NOT_FOUND_ERR);
		}
	}
	dom_node_inserted(c, new_node, n, idx);
	return JS_DupValue(c, argv[0] );
}

static JSValue xml_node_append_child(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 tag;
	GF_Node *n, *new_node;

	if (!argc || !JS_IsObject(argv[0])) {
		return GF_JS_EXCEPTION(c);
	}
	n = dom_get_node(obj);
	if (!n) {
		return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	}

	new_node = dom_get_node(argv[0]);
	if (!new_node) {
		return js_throw_err(c, GF_DOM_EXC_SYNTAX_ERR);
	}
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) {
		return js_throw_err(c, GF_DOM_EXC_SYNTAX_ERR);
	}

	if (!check_dom_parents(c, n, new_node)) {
		return js_throw_err(c, GF_DOM_EXC_VALIDATION_ERR);
	}

	dom_node_inserted(c, new_node, n, -1);
	return JS_DupValue(c, argv[0]);
}

void svg_mark_gc(struct __tag_svg_script_ctx *svg_js);

static JSValue xml_node_replace_child(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	s32 idx;
	u32 tag;
	GF_Node *n, *new_node, *old_node;
	GF_ParentNode *par;

	if ((argc!=2) || !JS_IsObject(argv[0]) || !JS_IsObject(argv[1])) return GF_JS_EXCEPTION(c);
	n = dom_get_node(obj);
	if (!n) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);

	new_node = dom_get_node(argv[0]);
	if (!new_node) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	old_node = dom_get_node(argv[1]);
	if (!old_node) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	par = (GF_ParentNode*)n;
	idx = gf_node_list_find_child(par->children, old_node);
	if (idx<0) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);

	gf_node_list_del_child(&par->children, old_node);
	gf_node_unregister(old_node, n);

	dom_node_inserted(c, new_node, n, -1);

	/*whenever we remove a node from the tree, call GC to cleanup the JS binding if any. Not doing so may screw up node IDs*/
	svg_mark_gc(n->sgprivate->scenegraph->svg_js);
	return JS_DupValue(c, argv[0]);
}

static JSValue xml_node_remove_child(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 tag;
	GF_Node *n, *old_node;
	GF_ParentNode *par;
	if (!argc || !JS_IsObject(argv[0])) return JS_TRUE;

	n = dom_get_node(obj);
	if (!n) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);

	old_node = dom_get_node(argv[0]);
	if (!old_node) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	par = (GF_ParentNode*)n;

	/*if node is present in parent, unregister*/
	if (gf_node_list_del_child(&par->children, old_node)) {
		gf_node_unregister(old_node, n);
	} else {
		return js_throw_err(c, GF_DOM_EXC_NOT_FOUND_ERR);
	}

	dom_node_changed(n, GF_TRUE, NULL);
	/*whenever we remove a node from the tree, call GC to cleanup the JS binding if any. Not doing so may screw up node IDs*/
	svg_mark_gc(n->sgprivate->scenegraph->svg_js);
	return JS_DupValue(c, argv[0]);
}

static JSValue xml_clone_node(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	Bool deep;
	GF_Node *n, *clone;

	n = dom_get_node(obj);
	if (!n) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	deep = argc ? (JS_ToBool(c, argv[0]) ? GF_TRUE : GF_FALSE) : GF_FALSE;

	clone = gf_node_clone(n->sgprivate->scenegraph, n, NULL, "", deep);
	return dom_node_construct(c, clone);
}

static JSValue xml_node_has_children(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Node *n;
	n = dom_get_node(obj);
	if (!n) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	return ((GF_ParentNode*)n)->children ? JS_TRUE : JS_FALSE;
}

static JSValue xml_node_has_attributes(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 tag;
	GF_Node *n;

	n = dom_get_node(obj);
	if (!n) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	tag = gf_node_get_tag(n);
	if (tag>=GF_NODE_FIRST_DOM_NODE_TAG) {
		return ((GF_DOMNode*)n)->attributes ? JS_TRUE : JS_FALSE;
	}
	/*not supported for other node types*/
	return JS_FALSE;
}

static JSValue xml_node_is_same_node(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Node *n, *a_node;

	if (!argc || !JS_IsObject(argv[0])) return JS_TRUE;
	n = dom_get_node(obj);
	if (!n) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);

	a_node = dom_get_node(argv[0]);
	if (!a_node) return js_throw_err(c, GF_DOM_EXC_HIERARCHY_REQUEST_ERR);
	return (a_node==n) ? JS_TRUE : JS_FALSE;
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
	if (node->sgprivate->tag < GF_NODE_FIRST_DOM_NODE_TAG) return NULL;

	att = ((SVG_Element*)node)->attributes;
	while (att) {
		if (att->tag==TAG_DOM_ATT_any) {
			GF_DOMFullAttribute *datt = (GF_DOMFullAttribute*)att;
			if (datt->name && !strncmp(datt->name, "xmlns", 5)) {
				char *xmlns = *(DOM_String *) datt->data;
				u32 crc = gf_crc_32(xmlns, (u32) strlen(xmlns));
				if (tag==crc)
					return xmlns;
				if (!tag && !strcmp(datt->name, "xmlns"))
					return xmlns;
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

static JSValue dom_node_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	u32 tag;
	GF_Node *n;
	GF_SceneGraph *sg = NULL;
	GF_ParentNode *par;

	n = dom_get_node(obj);
	if (!n) {
		sg = dom_get_doc(c, obj);
		if (!sg) return GF_JS_EXCEPTION(c);
	}
	tag = n ? gf_node_get_tag(n) : 0;
	par = (GF_ParentNode*)n;

	switch (magic) {
	case NODE_JSPROPERTY_NODENAME:
		if (sg) {
			return JS_NewString(c, "#document");
		}
		else if (tag==TAG_DOMText) {
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->type==GF_DOM_TEXT_CDATA) return JS_NewString(c, "#cdata-section");
			else return JS_NewString(c, "#text");
		}
		return JS_NewString(c, gf_node_get_class_name(n) );

	case NODE_JSPROPERTY_NODEVALUE:
		if (tag==TAG_DOMText) {
			GF_DOMText *txt = (GF_DOMText *)n;
			return JS_NewString(c, txt->textContent);
		}
		return JS_NULL;
	case NODE_JSPROPERTY_NODETYPE:
		if (sg) return JS_NewInt32(c, 9);
		else if (tag==TAG_DOMText) {
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->type==GF_DOM_TEXT_CDATA) return JS_NewInt32(c, 4);
			else return JS_NewInt32(c, 3);
		}
		return JS_NewInt32(c, 1);

	case NODE_JSPROPERTY_PARENTNODE:
		if (sg) {
			return JS_NULL;
		}
		/*if root node of the tree, the parentNode is the document*/
		else if (n->sgprivate->scenegraph->RootNode==n) {
			return dom_document_construct(c, n->sgprivate->scenegraph);
		} else {
			return dom_node_construct(c, gf_node_get_parent(n, 0) );
		}

	case NODE_JSPROPERTY_CHILDNODES:
		/*NOT SUPPORTED YET*/
		if (sg) return JS_UNDEFINED;
		else if (tag==TAG_DOMText) return JS_NULL;
		else return dom_nodelist_construct(c, par);

	case NODE_JSPROPERTY_FIRSTCHILD:
		if (sg) return dom_node_construct(c, sg->RootNode);
		else if (tag==TAG_DOMText) return JS_NULL;
		else if (!par->children) return JS_NULL;
		else return dom_node_construct(c, par->children->node);

	case NODE_JSPROPERTY_LASTCHILD:
		if (sg) return dom_node_construct(c, sg->RootNode);
		else if ((tag==TAG_DOMText) || !par->children) return JS_NULL;
		else return dom_node_construct(c, gf_node_list_get_child(par->children, -1) );

	case NODE_JSPROPERTY_PREVIOUSSIBLING:
		/*works for doc as well since n is NULL*/
		return dom_node_get_sibling(c, n, GF_TRUE, GF_FALSE);

	case NODE_JSPROPERTY_NEXTSIBLING:
		return dom_node_get_sibling(c, n, GF_FALSE, GF_FALSE);

	case NODE_JSPROPERTY_ATTRIBUTES:
		/*NOT SUPPORTED YET*/
		return JS_UNDEFINED;

	case NODE_JSPROPERTY_OWNERDOCUMENT:
		if (sg) return JS_NULL;
		else return dom_document_construct(c, n->sgprivate->scenegraph);

	case NODE_JSPROPERTY_NAMESPACEURI:
		if (!sg) {
			const char *xmlns;
			tag = gf_xml_get_element_namespace(n);
			xmlns = gf_sg_get_namespace(n->sgprivate->scenegraph, tag);
			if (!xmlns) xmlns = node_lookup_namespace_by_tag(n, tag);
			return xmlns ? JS_NewString(c, xmlns) : JS_NULL;
		}
		return JS_NULL;
	case NODE_JSPROPERTY_PREFIX:
		if (sg) tag = gf_sg_get_namespace_code(sg, NULL);
		else tag = gf_xml_get_element_namespace(n);

		if (tag) {
			char *xmlns = (char *)gf_sg_get_namespace_qname(sg ? sg : n->sgprivate->scenegraph, tag);
			if (xmlns) return JS_NewString(c, xmlns);
		}
		return JS_NULL;
	case NODE_JSPROPERTY_LOCALNAME:
		if (!sg && (tag!=TAG_DOMText)) {
			return JS_NewString(c, node_get_local_name(n) );
		}
		return JS_NULL;
	case NODE_JSPROPERTY_BASEURI:
		/*NOT SUPPORTED YET*/
		return JS_NULL;

	case NODE_JSPROPERTY_TEXTCONTENT:
		if (!sg)  {
			char *res = gf_dom_flatten_textContent(n);
			JSValue ret = JS_NewString(c, res ? res : "");
			if (res) gf_free(res);
			return ret;
		}
		return JS_NewString(c, "");

	case NODE_JSPROPERTY_FIRSTELEMENTCHILD:
		if (n->sgprivate->tag!=TAG_DOMText) {
			GF_ChildNodeItem *child = ((GF_ParentNode*)n)->children;
			while (child) {
				if (child->node->sgprivate->tag != TAG_DOMText) {
					return dom_element_construct(c, child->node);
				}
				child = child->next;
			}
		}
		return JS_NULL;

	case NODE_JSPROPERTY_LASTELEMENTCHILD:
		if (n->sgprivate->tag!=TAG_DOMText) {
			GF_Node *last = NULL;
			GF_ChildNodeItem *child = ((GF_ParentNode*)n)->children;
			while (child) {
				if (child->node->sgprivate->tag != TAG_DOMText) {
					last = child->node;
				}
				child = child->next;
			}
			if (last) return dom_element_construct(c, last);
		}
		return JS_NULL;
	case NODE_JSPROPERTY_PREVIOUSELEMENTSIBLING:
		return dom_node_get_sibling(c, n, GF_TRUE, GF_TRUE);

	case NODE_JSPROPERTY_NEXTELEMENTSIBLING:
		return dom_node_get_sibling(c, n, GF_FALSE, GF_TRUE);
	}
	return JS_UNDEFINED;
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

static JSValue dom_node_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	u32 tag;
	GF_Node *n;

	n = dom_get_node(obj);
	/*note an element - we don't support property setting on document yet*/
	if (!n) return GF_JS_EXCEPTION(c);

	tag = n ? gf_node_get_tag(n) : 0;
	switch (magic) {
	case NODE_JSPROPERTY_NODEVALUE:
		if ((tag==TAG_DOMText) && JS_CHECK_STRING(value)) {
			const char *str;
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->textContent) gf_free(txt->textContent);
			str = JS_ToCString(c, value);
			txt->textContent = str ? gf_strdup(str) : NULL;
			JS_FreeCString(c, str);
			dom_node_changed(n, GF_TRUE, NULL);
		}
		/*we only support element and sg in the Node interface, no set*/
		return JS_TRUE;
	case NODE_JSPROPERTY_PREFIX:
		/*NOT SUPPORTED YET*/
		return JS_TRUE;
	case NODE_JSPROPERTY_TEXTCONTENT:
	{
		const char *txt = JS_ToCString(c, value);
		dom_node_set_textContent(n, (char *) txt);
		if (txt) JS_FreeCString(c, txt);
	}
	return JS_TRUE;
	}
	/*not supported*/
	return JS_TRUE;
}


/*dom3 document*/

/*don't attempt to do anything with the scenegraph, it may be destroyed
fortunately a sg cannot be created like that...*/
void dom_document_finalize(JSRuntime *rt, JSValue obj)
{
	GF_SceneGraph *sg;
	sg = (GF_SceneGraph*) JS_GetOpaque_Nocheck(obj);
	/*the JS proto of the svgClass or a destroyed object*/
	if (!sg) return;

	if (sg->js_data) {
		JS_SetOpaque(sg->js_data->document, NULL);
		JS_FreeValueRT(rt, sg->js_data->document);
		gf_free(sg->js_data);
		sg->js_data = NULL;
	}

	//no need to unregister root, we did not registered it in the constructor
//	if (sg->RootNode) {
//		gf_node_unregister(sg->RootNode, NULL);
//	}

	if (sg->reference_count) {
		sg->reference_count--;
		if (!sg->reference_count)
			gf_sg_del(sg);
	}
}

static JSValue dom_document_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case DOCUMENT_JSPROPERTY_IMPLEMENTATION:
	{	/*FIXME, this is wrong, we should have our own implementation
		but at the current time we rely on the global object to provide it*/
		JSValue global = JS_GetGlobalObject(c);
		JSValue ret = JS_GetPropertyStr(c, global, "Window");
		JS_FreeValue(c, global);
		return ret;
	}

	case DOCUMENT_JSPROPERTY_DOCUMENTELEMENT:
		return dom_element_construct(c, sg->RootNode);

	case DOCUMENT_JSPROPERTY_GLOBAL:
		return JS_GetGlobalObject(c);

	case DOCUMENT_JSPROPERTY_DOCTYPE:
	case DOCUMENT_JSPROPERTY_INPUTENCODING:
	case DOCUMENT_JSPROPERTY_XMLENCODING:
	case DOCUMENT_JSPROPERTY_XMLSTANDALONE:
	case DOCUMENT_JSPROPERTY_XMLVERSION:
	case DOCUMENT_JSPROPERTY_STRICTERRORCHECKING:
	case DOCUMENT_JSPROPERTY_DOCUMENTURI:
	case DOCUMENT_JSPROPERTY_LOCATION:
	case DOCUMENT_JSPROPERTY_DOMCONFIG:
		return JS_NULL;
	}
	return JS_UNDEFINED;
}

static JSValue dom_document_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return GF_JS_EXCEPTION(c);
	return js_throw_err(c, GF_DOM_EXC_NOT_SUPPORTED_ERR);
}

static JSValue xml_document_create_element(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 tag, ns;
	JSValue ret = JS_NULL;
	const char *name;
	const char *xmlns;
	GF_SceneGraph *sg = dom_get_doc(c, obj);

	if (!sg || !argc || !JS_CHECK_STRING(argv[0]) )
		return GF_JS_EXCEPTION(c);

	name = NULL;
	/*NS version*/
	ns = 0;
	xmlns = NULL;
	if (argc==2) {
		if (!JS_CHECK_STRING(argv[1])) return GF_JS_EXCEPTION(c);
		xmlns = JS_ToCString(c, argv[0]);
		if (xmlns) ns = gf_sg_get_namespace_code_from_name(sg, (char *) xmlns);
		name = JS_ToCString(c, argv[1]);
	} else {
		name = JS_ToCString(c, argv[0]);
	}

	if (name) {
		GF_Node *n;
		/*browse all our supported DOM implementations*/
		tag = gf_xml_get_element_tag(name, ns);
		if (!tag) tag = TAG_DOMFullNode;
		n = gf_node_new(sg, tag);
		if (n && (tag == TAG_DOMFullNode)) {
			GF_DOMFullNode *elt = (GF_DOMFullNode *)n;
			elt->name = gf_strdup(name);
			if (xmlns) {
				gf_sg_add_namespace(sg, (char *) xmlns, NULL);
				elt->ns	= gf_sg_get_namespace_code_from_name(sg, (char *) xmlns);
			}
		}
		ret = dom_element_construct(c, n);
	}
	JS_FreeCString(c, name);
	JS_FreeCString(c, xmlns);
	return ret;
}

static JSValue xml_document_create_text(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Node *n;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return GF_JS_EXCEPTION(c);

	n = gf_node_new(sg, TAG_DOMText);
	if (argc) {
		const char *str = JS_ToCString(c, argv[0]);
		char *ntext = gf_strdup(str ? str : "");
		((GF_DOMText*)n)->textContent = ntext;
		JS_FreeCString(c, str);
	}
	return dom_text_construct(c, n);
}

static void xml_doc_gather_nodes(GF_ParentNode *node, char *name, DOMNodeList *nl)
{
	Bool bookmark = GF_TRUE;
	GF_ChildNodeItem *child;
	if (!node) return;
	if (name) {
		const char *node_name = gf_node_get_class_name((GF_Node*)node);
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

static JSValue xml_document_elements_by_tag(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	DOMNodeList *nl;
	JSValue new_obj;
	const char *name;
	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return GF_JS_EXCEPTION(c);

	if (!argc || !JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);

	/*NS version - TODO*/
	if (argc==2) {
		if (!JS_CHECK_STRING(argv[1])) return GF_JS_EXCEPTION(c);
		name = JS_ToCString(c, argv[1]);
	} else {
		name = JS_ToCString(c, argv[0]);
	}

	GF_SAFEALLOC(nl, DOMNodeList);
	if (!nl) return GF_JS_EXCEPTION(c);

	if (name && !strcmp(name, "*"))
		xml_doc_gather_nodes((GF_ParentNode*)sg->RootNode, NULL, nl);
	else
		xml_doc_gather_nodes((GF_ParentNode*)sg->RootNode, (char *) name, nl);
	new_obj = JS_NewObjectClass(c, domNodeListClass.class_id);
	JS_SetOpaque(new_obj, nl);
	JS_FreeCString(c, name);
	return new_obj;
}


static JSValue xml_document_element_by_id(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	NodeIDedItem *reg_node;
	GF_Node *n;
	const char *id;
	JSValue ret;

	GF_SceneGraph *sg = dom_get_doc(c, obj);
	if (!sg) return GF_JS_EXCEPTION(c);
	if (!argc || !JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);
	id = JS_ToCString(c, argv[0]);

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
	ret = dom_element_construct(c, n);
	JS_FreeCString(c, id);
	return ret;
}

/*dom3 element*/
void dom_element_finalize(JSRuntime *rt, JSValue obj)
{
	dom_node_finalize(rt, obj);
}

static JSValue dom_element_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_Node *n = dom_get_node(obj);
	if (!n) return JS_TRUE;

	switch (magic) {
	case ELEMENT_JSPROPERTY_TAGNAME:
		return JS_NewString(c, gf_node_get_class_name(n) );

	case ELEMENT_JSPROPERTY_SCHEMATYPEINFO:
		/*NOT SUPPORTED YET*/
		return JS_NULL;
	}
	return JS_UNDEFINED;
}

static JSValue xml_element_get_attribute(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *name;
	const char *ns;
	JSValue ret = JS_NULL;

	GF_Node *n = dom_get_node(obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if (!argc || !JS_CHECK_STRING(argv[0]))
		return JS_TRUE;
	ns = NULL;
	/*NS version*/
	if (argc==2) {
		if (!JS_CHECK_STRING(argv[1]))
			return JS_TRUE;
		ns = JS_ToCString(c, argv[0]);
		name = JS_ToCString(c, argv[1]);
	} else {
		name = JS_ToCString(c, argv[0]);
	}
	if (!name) goto exit;

	/*ugly ugly hack ...*/
	if (!strcmp(name, "id") || !strcmp(name, "xml:id") ) {
		char *sID = (char *) gf_node_get_name(n);
		if (sID) {
			ret = JS_NewString(c, sID);
			goto exit;
		}
	}

	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, name)) {
				ret = JS_NewString(c, *(char**)att->data );
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
			ns_code = gf_sg_get_namespace_code_from_name(n->sgprivate->scenegraph, (char *) ns);
			if (!ns_code) ns_code = gf_crc_32(ns, (u32) strlen(ns));
		}
		else {
			ns_code = gf_xml_get_element_namespace(n);
		}

		if (gf_node_get_attribute_by_name(n, (char *) name, ns_code, GF_FALSE, GF_FALSE, &info)==GF_OK) {
			char *szAtt = gf_svg_dump_attribute(n, &info);
			ret = JS_NewString(c, szAtt);
			if (szAtt) gf_free(szAtt);
			goto exit;
		}
	}
	ret = JS_NewString(c, "");
exit:
	JS_FreeCString(c, name);
	JS_FreeCString(c, ns);
	return ret;
}

static JSValue xml_element_has_attribute(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *name;
	const char *ns;
	JSValue ret = JS_NULL;

	GF_Node *n = dom_get_node(obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if (!argc || !JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);
	ns = NULL;
	/*NS version*/
	if (argc==2) {
		if (!JS_CHECK_STRING(argv[1])) return GF_JS_EXCEPTION(c);
		ns = JS_ToCString(c, argv[0]);
		name = JS_ToCString(c, argv[1]);
	} else {
		name = JS_ToCString(c, argv[0]);
	}
	if (!name) goto exit;

	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, name)) {
				ret = JS_TRUE;
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
		if (ns) ns_code = gf_sg_get_namespace_code_from_name(n->sgprivate->scenegraph, (char *) ns);
		else ns_code = gf_sg_get_namespace_code(n->sgprivate->scenegraph, NULL);

		if (gf_node_get_attribute_by_name(n, (char *) name, ns_code, GF_FALSE, GF_FALSE, &info)==GF_OK) {
			ret = JS_TRUE;
			goto exit;
		}
	}
	ret = JS_FALSE;
exit:
	JS_FreeCString(c, name);
	JS_FreeCString(c, ns);
	return ret;
}



static JSValue xml_element_remove_attribute(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 tag;
	GF_DOMFullNode *node;
	GF_DOMFullAttribute *prev, *att;
	const char *name, *ns;

	GF_Node *n = dom_get_node(obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if (!argc || !JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);
	ns = NULL;
	/*NS version*/
	if (argc==2) {
		if (!JS_CHECK_STRING(argv[1])) return GF_JS_EXCEPTION(c);
		ns = JS_ToCString(c, argv[0]);
		name = JS_ToCString(c, argv[1]);
	} else {
		name = JS_ToCString(c, argv[0]);
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
		if (ns) ns_code = gf_sg_get_namespace_code_from_name(n->sgprivate->scenegraph, (char *) ns);
		else ns_code = gf_sg_get_namespace_code(n->sgprivate->scenegraph, NULL);

		tag = gf_xml_get_attribute_tag(n, (char *)name, ns_code);
	}

	while (att) {
		if ((att->tag==TAG_DOM_ATT_any) && !strcmp(att->name, name)) {
			DOM_String *s;
			if (prev) prev->next = att->next;
			else node->attributes = att->next;
			s = att->data;
			if (*s) gf_free(*s);
			gf_free(s);
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
	JS_FreeCString(c, name);
	JS_FreeCString(c, ns);
	return JS_TRUE;
}

static void gf_dom_add_handler_listener(GF_Node *n, u32 evtType, char *handlerCode)
{
	/*check if we're modifying an existing listener*/
	SVG_handlerElement *handler;
	u32 i, count = gf_dom_listener_count(n);
	for (i=0; i<count; i++) {
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
			DOM_String *s;
			assert(att->data_type == DOM_String_datatype);
			assert(att->data);
			s = (DOM_String *) att->data;
			if ( *s ) gf_free( *s);
			*s = gf_strdup(attribute_content);
			dom_node_changed((GF_Node *)node, GF_FALSE, NULL);
			return;
		}
		prev = att;
		att = (GF_DOMFullAttribute *) att->next;
	}
	/*create new att*/
	GF_SAFEALLOC(att, GF_DOMFullAttribute);
	if (!att) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DOMJS] Failed to allocate DOM attribute\n"));
		return;
	}
	att->tag = TAG_DOM_ATT_any;
	att->name = gf_strdup(attribute_name);
	att->data_type = (u16) DOM_String_datatype;
	att->data = gf_svg_create_attribute_value(att->data_type);
	*((char **)att->data) = gf_strdup(attribute_content);

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
				char *a_name = attname->name;
				sep = strchr(a_name, ':');
				if (sep) {
					sep[0] = 0;
					attname->type = gf_sg_get_namespace_code(n->sgprivate->scenegraph, a_name);
					sep[0] = ':';
					a_name = gf_strdup(sep+1);
					gf_free(attname->name);
					attname->name = a_name;
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
		case SVG_TRANSFORM_TRANSLATE:
			anim_value_type = SVG_Transform_Translate_datatype;
			break;
		case SVG_TRANSFORM_SCALE:
			anim_value_type = SVG_Transform_Scale_datatype;
			break;
		case SVG_TRANSFORM_ROTATE:
			anim_value_type = SVG_Transform_Rotate_datatype;
			break;
		case SVG_TRANSFORM_SKEWX:
			anim_value_type = SVG_Transform_SkewX_datatype;
			break;
		case SVG_TRANSFORM_SKEWY:
			anim_value_type = SVG_Transform_SkewY_datatype;
			break;
		case SVG_TRANSFORM_MATRIX:
			anim_value_type = SVG_Transform_datatype;
			break;
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
					GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DOM] Cannot find target of the animation to parse attribute %s\n", attname->name));
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

static JSValue xml_element_set_attribute(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 idx;
	const char *name, *_val, *val, *ns;
	char szVal[100];

	GF_Node *n = dom_get_node(obj);
	if (!n) return GF_JS_EXCEPTION(c);
	if ((argc < 2)) return GF_JS_EXCEPTION(c);

	if (!JS_CHECK_STRING(argv[0]))
		return GF_JS_EXCEPTION(c);

	idx = 1;
	name = _val = val = NULL;
	ns = NULL;
	/*NS version*/
	if (argc==3) {
		char *sep;
		if (!JS_CHECK_STRING(argv[1]))
			return GF_JS_EXCEPTION(c);
		ns = JS_ToCString(c, argv[0]);
		gf_sg_add_namespace(n->sgprivate->scenegraph, (char *) ns, NULL);
		name = JS_ToCString(c, argv[1]);
		idx = 2;

		sep = strchr(name, ':');
		if (sep) name = sep+1;

	} else {
		name = JS_ToCString(c, argv[0]);
	}

	val = NULL;
	if (JS_CHECK_STRING(argv[idx])) {
		val = _val = JS_ToCString(c, argv[idx]);
	} else if (JS_IsBool(argv[idx])) {
		sprintf(szVal, "%s", JS_ToBool(c, argv[idx]) ? "true" : "false");
		val = szVal;
	} else if (JS_IsNumber(argv[idx])) {
		Double d;
		JS_ToFloat64(c, &d, argv[idx]);
		sprintf(szVal, "%g", d);
		val = szVal;
	} else if (JS_IsInteger(argv[idx])) {
		u32 i;
		JS_ToInt32(c, &i, argv[idx]);
		sprintf(szVal, "%d", i);
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
			gf_dom_add_handler_listener(n, evtType, (char *) val);
			goto exit;
		}
	}

	if (n->sgprivate->tag==TAG_DOMFullNode) {
		gf_dom_full_set_attribute((GF_DOMFullNode*)n, (char *) name, (char *) val);
		goto exit;
	}

	if (n->sgprivate->tag==TAG_DOMText) {
		goto exit;
	}

	if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) {
		gf_svg_set_attribute(n, (char *) ns, (char *) name, (char *) val);
	}
exit:
	JS_FreeCString(c, name);
	JS_FreeCString(c, ns);
	JS_FreeCString(c, _val);
	return JS_TRUE;
}


static JSValue xml_element_elements_by_tag(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	DOMNodeList *nl;
	JSValue new_obj;
	const char *name;

	GF_Node *n = dom_get_node(obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if (!argc || !JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);

	/*NS version*/
	if (argc==2) {
		if (!JS_CHECK_STRING(argv[1])) return GF_JS_EXCEPTION(c);
		name = JS_ToCString(c, argv[1]);
	} else {
		name = JS_ToCString(c, argv[0]);
	}
	GF_SAFEALLOC(nl, DOMNodeList);
	if (!nl) return GF_JS_EXCEPTION(c);

	if (name && !strcmp(name, "*")) {
		JS_FreeCString(c, name);
		name = NULL;
	}
	xml_doc_gather_nodes((GF_ParentNode*)n, (char *)name, nl);
	new_obj = JS_NewObjectClass(c, domNodeListClass.class_id);
	JS_SetOpaque(new_obj, nl);
	JS_FreeCString(c, name);
	return new_obj;
}

static JSValue xml_element_set_id(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 node_id;
	const char *name;
	Bool is_id;

	GF_Node *n = dom_get_node(obj);
	if (!n) return GF_JS_EXCEPTION(c);

	if ((argc<2) || !JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);

	/*NS version*/
	if (argc==3) {
		if (!JS_CHECK_STRING(argv[1])) return GF_JS_EXCEPTION(c);
		name = JS_ToCString(c, argv[1]);
		is_id = JS_ToBool(c, argv[2]) ? GF_TRUE : GF_FALSE;
	} else {
		name = JS_ToCString(c, argv[0]);
		is_id = JS_ToBool(c, argv[1]) ? GF_TRUE : GF_FALSE;
	}
	gf_node_get_name_and_id(n, &node_id);
	if (node_id && is_id) {
		/*we only support ONE ID per node*/
		JS_FreeCString(c, name);
		return GF_JS_EXCEPTION(c);
	}
	if (is_id) {
		if (!name) return GF_JS_EXCEPTION(c);
		gf_node_set_id(n, gf_sg_get_max_node_id(n->sgprivate->scenegraph) + 1, name);
	} else if (node_id) {
		gf_node_remove_id(n);
	}
	JS_FreeCString(c, name);
	return JS_TRUE;
}
static void gather_text(GF_ParentNode *n, char **out_str)
{
	if (n->sgprivate->tag==TAG_DOMText) {
		GF_DOMText *dom_text = (GF_DOMText *)n;
		if (dom_text->textContent) gf_dynstrcat(out_str, dom_text->textContent, NULL);
	} else {
		GF_ChildNodeItem *child = ((GF_ParentNode *) n)->children;
		while (child) {
			gather_text((GF_ParentNode *)child->node, out_str);
			child = child->next;
		}
	}
}
static JSValue xml_element_to_string(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	JSValue ret;
	char *out_str = NULL;
	GF_Node *n = dom_get_node(obj);
	if (!n) return GF_JS_EXCEPTION(c);

	GF_ChildNodeItem *child;
	child = ((GF_ParentNode *) n)->children;
	while (child) {
		gather_text((GF_ParentNode *)child->node, &out_str);
		child = child->next;
	}
	if (!out_str) {
		const char *node_name = gf_node_get_class_name(n);
		if (node_name)
			return JS_NewString(c, node_name);
		return JS_NULL;
	}
	ret = JS_NewString(c, out_str);
	gf_free(out_str);
	return ret;
}

/*dom3 character/text/comment*/
static JSValue dom_text_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_DOMText *txt = (GF_DOMText*)dom_get_node(obj);
	if (!txt || (txt->sgprivate->tag != TAG_DOMText)) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case TEXT_JSPROPERTY_DATA:
		if (txt->textContent) return JS_NewString(c, txt->textContent);
		else return JS_NewString(c, "");

	case TEXT_JSPROPERTY_LENGTH:
		return JS_NewInt32(c, txt->textContent ? (u32) strlen(txt->textContent) : 0);

	case TEXT_JSPROPERTY_ISELEMENTCONTENTWHITESPACE:
		return JS_FALSE;

	case TEXT_JSPROPERTY_WHOLETEXT:
		/*FIXME - this is wrong we should serialize adjacent text strings as well*/
		if (txt->textContent) return JS_NewString(c, txt->textContent);
		else return JS_NewString(c, "");
	}
	return JS_UNDEFINED;
}

static JSValue dom_text_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	GF_DOMText *txt = (GF_DOMText*)dom_get_node(obj);
	if (!txt || (txt->sgprivate->tag != TAG_DOMText)) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case TEXT_JSPROPERTY_DATA:
		if (txt->textContent) gf_free(txt->textContent);
		txt->textContent = NULL;
		if (JS_CHECK_STRING(value)) {
			const char *str = JS_ToCString(c, value);
			txt->textContent = gf_strdup(str ? str : "");
			JS_FreeCString(c, str);
		}
		dom_node_changed((GF_Node*)txt, GF_FALSE, NULL);
		return JS_TRUE;
		/*the rest is read-only*/
	}
	return JS_UNDEFINED;
}

static JSValue event_stop_propagation(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_DOM_Event *evt = JS_GetOpaque(obj, domEventClass.class_id);
	if (!evt) return GF_JS_EXCEPTION(c);
	evt->event_phase |= GF_DOM_EVENT_PHASE_CANCEL;
	return JS_TRUE;
}
static JSValue event_stop_immediate_propagation(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_DOM_Event *evt = JS_GetOpaque(obj, domEventClass.class_id);
	if (!evt) return GF_JS_EXCEPTION(c);
	evt->event_phase |= GF_DOM_EVENT_PHASE_CANCEL_ALL;
	return JS_TRUE;
}
static JSValue event_prevent_default(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_DOM_Event *evt = JS_GetOpaque(obj, domEventClass.class_id);
	if (!evt) return GF_JS_EXCEPTION(c);
	evt->event_phase |= GF_DOM_EVENT_PHASE_PREVENT;
	return JS_TRUE;
}

static JSValue event_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	GF_DOM_Event *evt = JS_GetOpaque(obj, domEventClass.class_id);
	if (evt==NULL) return JS_TRUE;

	switch (magic) {
	case EVENT_JSPROPERTY_TYPE:
		return JS_NewString(c, gf_dom_event_get_name(evt->type) );

	case EVENT_JSPROPERTY_TARGET:
		if (evt->is_vrml) return JS_TRUE;
		switch (evt->target_type) {
		case GF_DOM_EVENT_TARGET_NODE:
			return dom_element_construct(c, (GF_Node*) evt->target);

		case GF_DOM_EVENT_TARGET_DOCUMENT:
			return dom_document_construct(c, (GF_SceneGraph *) evt->target);

#ifdef GPAC_ENABLE_HTML5_MEDIA
		case GF_DOM_EVENT_TARGET_MSE_MEDIASOURCE:
			return OBJECT_TO_JSValue(((GF_HTML_MediaSource *)evt->target)->_this);
		case GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFER:
			return OBJECT_TO_JSValue(((GF_HTML_SourceBuffer *)evt->target)->_this);
		case GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFERLIST:
			return OBJECT_TO_JSValue(((GF_HTML_SourceBufferList *)evt->target)->_this);
#endif
		default:
			break;
		}
		return JS_TRUE;
	case EVENT_JSPROPERTY_CURRENTTARGET:
		if (evt->is_vrml) return JS_NULL;
		switch (evt->currentTarget->ptr_type) {
		case GF_DOM_EVENT_TARGET_NODE:
			return dom_element_construct(c, (GF_Node*) evt->currentTarget->ptr);
		case GF_DOM_EVENT_TARGET_DOCUMENT:
			return dom_document_construct(c, (GF_SceneGraph *) evt->currentTarget->ptr);
#ifdef GPAC_ENABLE_HTML5_MEDIA
		case GF_DOM_EVENT_TARGET_MSE_MEDIASOURCE:
			return OBJECT_TO_JSValue(((GF_HTML_MediaSource *)evt->target)->_this);
		case GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFER:
			return OBJECT_TO_JSValue(((GF_HTML_SourceBuffer *)evt->target)->_this);
			break;
		case GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFERLIST:
			return OBJECT_TO_JSValue(((GF_HTML_SourceBufferList *)evt->target)->_this);
#endif
		default:
			break;
		}
		return JS_TRUE;
	case EVENT_JSPROPERTY_EVENTPHASE:
		return JS_NewInt32(c, (evt->event_phase & 0x3) );

	case EVENT_JSPROPERTY_BUBBLES:
		return evt->bubbles ? JS_TRUE : JS_FALSE;

	case EVENT_JSPROPERTY_CANCELABLE:
		return evt->cancelable ? JS_TRUE : JS_FALSE;
	case EVENT_JSPROPERTY_NAMESPACEURI:
		return JS_NULL;

	case EVENT_JSPROPERTY_TIMESTAMP:
		return JS_NULL;

	case EVENT_JSPROPERTY_DEFAULTPREVENTED:
		return (evt->event_phase & GF_DOM_EVENT_PHASE_PREVENT) ? JS_TRUE : JS_FALSE;

	case EVENT_JSPROPERTY_DETAIL:
		return JS_NewInt32(c, evt->detail);

	case EVENT_JSPROPERTY_DATA:
	{
		u32 len;
		s16 txt[2];
		const u16 *srcp;
		char szData[5];
		txt[0] = evt->detail;
		txt[1] = 0;
		srcp = (const u16 *) txt;
		len = (u32) gf_utf8_wcstombs(szData, 5, &srcp);
		szData[len] = 0;
		return JS_NewString(c, szData);
	}

	case EVENT_JSPROPERTY_SCREENX:
		return JS_NewInt32(c, evt->screenX);
	case EVENT_JSPROPERTY_SCREENY:
		return JS_NewInt32(c, evt->screenY);
	case EVENT_JSPROPERTY_CLIENTX:
		return JS_NewInt32(c, evt->clientX);
	case EVENT_JSPROPERTY_CLIENTY:
		return JS_NewInt32(c, evt->clientY);
	case EVENT_JSPROPERTY_BUTTON:
		return JS_NewInt32(c, evt->button);
	case EVENT_JSPROPERTY_RELATEDTARGET:
		if (evt->is_vrml) return JS_NULL;
		return dom_element_construct(c, evt->relatedTarget);
	case EVENT_JSPROPERTY_WHEELDELTA:
		return JS_NewInt32(c, FIX2INT(evt->new_scale) );
	case EVENT_JSPROPERTY_KEYIDENTIFIER:
		return JS_NewString(c, gf_dom_get_key_name(evt->detail) );
	/*Mozilla keyChar, charCode: wrap up to same value*/
	case EVENT_JSPROPERTY_KEYCHAR:
	case EVENT_JSPROPERTY_CHARCODE:
		return JS_NewInt32(c, evt->detail);
	case EVENT_JSPROPERTY_LOADED:
		return JS_NewInt64(c, evt->media_event.loaded_size);
	case EVENT_JSPROPERTY_TOTAL:
		return JS_NewInt64(c, evt->media_event.total_size);
	case EVENT_JSPROPERTY_BUFFER_ON:
		return evt->media_event.bufferValid ? JS_TRUE : JS_FALSE;
	case EVENT_JSPROPERTY_BUFFERLEVEL:
		return JS_NewInt32(c, evt->media_event.level);
	case EVENT_JSPROPERTY_BUFFERREMAININGTIME:
		return JS_NewFloat64(c, evt->media_event.remaining_time);
	case EVENT_JSPROPERTY_STATUS:
		return JS_NewInt32(c, evt->media_event.status);

	/*VRML ones*/
	case EVENT_JSPROPERTY_WIDTH:
		return JS_NewFloat64(c, FIX2FLT(evt->screen_rect.width) );
	case EVENT_JSPROPERTY_HEIGHT:
		return JS_NewFloat64(c, FIX2FLT(evt->screen_rect.height) );
	case EVENT_JSPROPERTY_OFFSETX:
		return JS_NewFloat64(c, FIX2FLT(evt->screen_rect.x) );
	case EVENT_JSPROPERTY_OFFSETY:
		return JS_NewFloat64(c, FIX2FLT(evt->screen_rect.y) );
	case EVENT_JSPROPERTY_VPWIDTH:
		return JS_NewFloat64(c, FIX2FLT(evt->prev_translate.x) );
	case EVENT_JSPROPERTY_VPHEIGHT:
		return JS_NewFloat64(c, FIX2FLT(evt->prev_translate.y) );
	case EVENT_JSPROPERTY_TRANSLATIONX:
		return JS_NewFloat64(c, FIX2FLT(evt->new_translate.x) );
	case EVENT_JSPROPERTY_TRANSLATIONY:
		return JS_NewFloat64(c, FIX2FLT(evt->new_translate.y) );
	case EVENT_JSPROPERTY_TYPE3D:
		return JS_NewInt32(c, evt->detail);
	case EVENT_JSPROPERTY_ERROR:
		return JS_NewInt32(c, evt->error_state);
	case EVENT_JSPROPERTY_DYNAMIC_SCENE:
		return JS_NewInt32(c, evt->key_flags);
	case EVENT_JSPROPERTY_URL:
		return JS_NewString(c, evt->addon_url ? evt->addon_url : "");

	default:
		break;
	}
	return JS_UNDEFINED;
}


#define SETUP_JSCLASS(_class, _name, _proto_funcs, _construct, _finalize, _proto_class_id) \
	if (! _class.class_id) {\
		JS_NewClassID(&(_class.class_id)); \
		_class.class.class_name = _name; \
		_class.class.finalizer = _finalize;\
		JS_NewClass(jsrt, _class.class_id, &(_class.class));\
	}\
	proto = JS_NewObjectClass(c, _proto_class_id ? _proto_class_id : _class.class_id);\
    	JS_SetPropertyFunctionList(c, proto, _proto_funcs, countof(_proto_funcs));\
    	JS_SetClassProto(c, _class.class_id, proto);\
    	if (_construct) {\
		JSValue ctor = JS_NewCFunction2(c, _construct, _name, 1, JS_CFUNC_constructor, 0);\
    		JS_SetPropertyStr(c, global, _name, ctor);\
	}\


/*SVGMatrix class*/
static const JSCFunctionListEntry node_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("nodeName", dom_node_getProperty, NULL, NODE_JSPROPERTY_NODENAME),
	JS_CGETSET_MAGIC_DEF("nodeValue", dom_node_getProperty, dom_node_setProperty, NODE_JSPROPERTY_NODEVALUE),

	JS_CGETSET_MAGIC_DEF("nodeType", dom_node_getProperty, NULL, NODE_JSPROPERTY_NODETYPE),
	JS_CGETSET_MAGIC_DEF("parentNode", dom_node_getProperty, NULL, NODE_JSPROPERTY_PARENTNODE),
	JS_CGETSET_MAGIC_DEF("childNodes", dom_node_getProperty, NULL, NODE_JSPROPERTY_CHILDNODES),
	JS_CGETSET_MAGIC_DEF("firstChild", dom_node_getProperty, NULL, NODE_JSPROPERTY_FIRSTCHILD),
	JS_CGETSET_MAGIC_DEF("lastChild", dom_node_getProperty, NULL, NODE_JSPROPERTY_LASTCHILD),
	JS_CGETSET_MAGIC_DEF("previousSibling", dom_node_getProperty, NULL, NODE_JSPROPERTY_PREVIOUSSIBLING),
	JS_CGETSET_MAGIC_DEF("nextSibling", dom_node_getProperty, NULL, NODE_JSPROPERTY_NEXTSIBLING),
	JS_CGETSET_MAGIC_DEF("attributes", dom_node_getProperty, NULL, NODE_JSPROPERTY_ATTRIBUTES),
	JS_CGETSET_MAGIC_DEF("ownerDocument", dom_node_getProperty, NULL, NODE_JSPROPERTY_OWNERDOCUMENT),
	JS_CGETSET_MAGIC_DEF("namespaceURI", dom_node_getProperty, NULL, NODE_JSPROPERTY_NAMESPACEURI),
	JS_CGETSET_MAGIC_DEF("prefix", dom_node_getProperty, dom_node_setProperty, NODE_JSPROPERTY_PREFIX),
	JS_CGETSET_MAGIC_DEF("localName", dom_node_getProperty, NULL, NODE_JSPROPERTY_LOCALNAME),
	JS_CGETSET_MAGIC_DEF("baseURI", dom_node_getProperty, NULL, NODE_JSPROPERTY_BASEURI),
	JS_CGETSET_MAGIC_DEF("textContent", dom_node_getProperty, dom_node_setProperty, NODE_JSPROPERTY_TEXTCONTENT),
	/*elementTraversal interface*/
	JS_CGETSET_MAGIC_DEF("firstElementChild", dom_node_getProperty, NULL, NODE_JSPROPERTY_FIRSTELEMENTCHILD),
	JS_CGETSET_MAGIC_DEF("lastElementChild", dom_node_getProperty, NULL, NODE_JSPROPERTY_LASTELEMENTCHILD),
	JS_CGETSET_MAGIC_DEF("previousElementSibling", dom_node_getProperty, NULL, NODE_JSPROPERTY_PREVIOUSELEMENTSIBLING),
	JS_CGETSET_MAGIC_DEF("nextElementSibling", dom_node_getProperty, NULL, NODE_JSPROPERTY_NEXTELEMENTSIBLING),

	JS_CFUNC_DEF("insertBefore", 2, xml_node_insert_before),
	JS_CFUNC_DEF("replaceChild", 2, xml_node_replace_child),
	JS_CFUNC_DEF("removeChild",	1, xml_node_remove_child),
	JS_CFUNC_DEF("appendChild", 1, xml_node_append_child),
	JS_CFUNC_DEF("hasChildNodes", 0, xml_node_has_children),
	JS_CFUNC_DEF("cloneNode", 1, xml_clone_node),
	JS_CFUNC_DEF("normalize", 0, xml_dom3_not_implemented),
	JS_CFUNC_DEF("isSupported", 2, xml_dom3_not_implemented),
	JS_CFUNC_DEF("hasAttributes", 0, xml_node_has_attributes),
	JS_CFUNC_DEF("compareDocumentPosition", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("isSameNode", 1, xml_node_is_same_node),
	JS_CFUNC_DEF("lookupPrefix", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("isDefaultNamespace", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("lookupNamespaceURI", 1, xml_dom3_not_implemented),
	/*we don't support full node compare*/
	JS_CFUNC_DEF("isEqualNode", 1, xml_node_is_same_node),
	JS_CFUNC_DEF("getFeature", 2, xml_dom3_not_implemented),
	JS_CFUNC_DEF("setUserData", 3, xml_dom3_not_implemented),
	JS_CFUNC_DEF("getUserData",	1, xml_dom3_not_implemented),
};


static const JSCFunctionListEntry document_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("doctype", dom_document_getProperty, NULL, DOCUMENT_JSPROPERTY_DOCTYPE),
	JS_CGETSET_MAGIC_DEF("implementation", dom_document_getProperty, NULL, DOCUMENT_JSPROPERTY_IMPLEMENTATION),
	JS_CGETSET_MAGIC_DEF("documentElement", dom_document_getProperty, NULL, DOCUMENT_JSPROPERTY_DOCUMENTELEMENT),
	JS_CGETSET_MAGIC_DEF("inputEncoding", dom_document_getProperty, NULL, DOCUMENT_JSPROPERTY_INPUTENCODING),
	JS_CGETSET_MAGIC_DEF("xmlEncoding", dom_document_getProperty, NULL, DOCUMENT_JSPROPERTY_XMLENCODING),
	JS_CGETSET_MAGIC_DEF("xmlStandalone", dom_document_getProperty, dom_document_setProperty, DOCUMENT_JSPROPERTY_XMLSTANDALONE),
	JS_CGETSET_MAGIC_DEF("xmlVersion", dom_document_getProperty, dom_document_setProperty, DOCUMENT_JSPROPERTY_XMLVERSION),
	JS_CGETSET_MAGIC_DEF("strictErrorChecking", dom_document_getProperty, dom_document_setProperty, DOCUMENT_JSPROPERTY_STRICTERRORCHECKING),
	JS_CGETSET_MAGIC_DEF("documentURI", dom_document_getProperty, dom_document_setProperty, DOCUMENT_JSPROPERTY_DOCUMENTURI),
	JS_CGETSET_MAGIC_DEF("location", dom_document_getProperty, dom_document_setProperty, DOCUMENT_JSPROPERTY_LOCATION),
	JS_CGETSET_MAGIC_DEF("domConfig", dom_document_getProperty, dom_document_setProperty, DOCUMENT_JSPROPERTY_DOMCONFIG),
	JS_CGETSET_MAGIC_DEF("global", dom_document_getProperty, dom_document_setProperty, DOCUMENT_JSPROPERTY_GLOBAL),

	JS_CFUNC_DEF("createElement", 1, xml_document_create_element),
	JS_CFUNC_DEF("createDocumentFragment", 0, xml_dom3_not_implemented),
	JS_CFUNC_DEF("createTextNode", 1, xml_document_create_text),
	JS_CFUNC_DEF("createComment", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("createCDATASection", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("createProcessingInstruction", 2, xml_dom3_not_implemented),
	JS_CFUNC_DEF("createAttribute", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("createEntityReference", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("getElementsByTagName", 1, xml_document_elements_by_tag),
	JS_CFUNC_DEF("importNode", 2, xml_dom3_not_implemented),
	JS_CFUNC_DEF("createElementNS", 2, xml_document_create_element),
	JS_CFUNC_DEF("createAttributeNS", 2, xml_dom3_not_implemented),
	JS_CFUNC_DEF("getElementsByTagNameNS", 2, xml_document_elements_by_tag),
	JS_CFUNC_DEF("getElementById", 1, xml_document_element_by_id),
	JS_CFUNC_DEF("adoptNode", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("normalizeDocument", 0, xml_dom3_not_implemented),
	JS_CFUNC_DEF("renameNode", 3, xml_dom3_not_implemented),
	/*eventTarget interface*/
	JS_DOM3_EVENT_TARGET_INTERFACE
	/*DocumentEvent interface*/
	JS_CFUNC_DEF("createEvent", 1, xml_dom3_not_implemented),
};

static const JSCFunctionListEntry element_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("tagName", dom_element_getProperty, NULL, ELEMENT_JSPROPERTY_TAGNAME),
	JS_CGETSET_MAGIC_DEF("schemaTypeInfo", dom_element_getProperty, NULL, ELEMENT_JSPROPERTY_SCHEMATYPEINFO),

	JS_CFUNC_DEF("getAttribute", 1, xml_element_get_attribute),
	JS_CFUNC_DEF("setAttribute", 2, xml_element_set_attribute),
	JS_CFUNC_DEF("removeAttribute", 1, xml_element_remove_attribute),
	JS_CFUNC_DEF("getAttributeNS", 2, xml_element_get_attribute),
	JS_CFUNC_DEF("setAttributeNS", 3, xml_element_set_attribute),
	JS_CFUNC_DEF("removeAttributeNS", 2, xml_element_remove_attribute),
	JS_CFUNC_DEF("hasAttribute", 1, xml_element_has_attribute),
	JS_CFUNC_DEF("hasAttributeNS", 2, xml_element_has_attribute),
	JS_CFUNC_DEF("getElementsByTagName", 1, xml_element_elements_by_tag),
	JS_CFUNC_DEF("getElementsByTagNameNS", 2, xml_element_elements_by_tag),
	JS_CFUNC_DEF("setIdAttribute", 2, xml_element_set_id),
	JS_CFUNC_DEF("setIdAttributeNS", 3, xml_element_set_id),
	JS_CFUNC_DEF("toString", 0, xml_element_to_string),
	JS_CFUNC_DEF("getAttributeNode", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("setAttributeNode", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("removeAttributeNode", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("getAttributeNodeNS", 2, xml_dom3_not_implemented),
	JS_CFUNC_DEF("setAttributeNodeNS", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("setIdAttributeNode", 2, xml_dom3_not_implemented),
	/*eventTarget interface*/
	JS_DOM3_EVENT_TARGET_INTERFACE
};

static const JSCFunctionListEntry text_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("data", dom_text_getProperty, dom_text_setProperty, TEXT_JSPROPERTY_DATA),
	JS_CGETSET_MAGIC_DEF("length", dom_text_getProperty, NULL, TEXT_JSPROPERTY_LENGTH),
	JS_CGETSET_MAGIC_DEF("isElementContentWhitespace", dom_text_getProperty, NULL, TEXT_JSPROPERTY_ISELEMENTCONTENTWHITESPACE),
	JS_CGETSET_MAGIC_DEF("wholeText", dom_text_getProperty, NULL, TEXT_JSPROPERTY_WHOLETEXT),
#if 0
	JS_CFUNC_DEF("substringData", 2, xml_dom3_not_implemented),
	JS_CFUNC_DEF("appendData", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("insertData", 2, xml_dom3_not_implemented),
	JS_CFUNC_DEF("deleteData", 2, xml_dom3_not_implemented),
	JS_CFUNC_DEF("replaceData", 3, xml_dom3_not_implemented),
	JS_CFUNC_DEF("splitText", 1, xml_dom3_not_implemented),
	JS_CFUNC_DEF("replaceWholeText", 1, xml_dom3_not_implemented),
#endif
};

static const JSCFunctionListEntry event_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("type", event_getProperty, NULL, EVENT_JSPROPERTY_TYPE),
	JS_CGETSET_MAGIC_DEF("target", event_getProperty, NULL, EVENT_JSPROPERTY_TARGET),
	JS_CGETSET_MAGIC_DEF("currentTarget", event_getProperty, NULL, EVENT_JSPROPERTY_CURRENTTARGET),
	JS_CGETSET_MAGIC_DEF("eventPhase", event_getProperty, NULL, EVENT_JSPROPERTY_EVENTPHASE),
	JS_CGETSET_MAGIC_DEF("bubbles", event_getProperty, NULL, EVENT_JSPROPERTY_BUBBLES),
	JS_CGETSET_MAGIC_DEF("cancelable", event_getProperty, NULL, EVENT_JSPROPERTY_CANCELABLE),
	JS_CGETSET_MAGIC_DEF("timeStamp", event_getProperty, NULL, EVENT_JSPROPERTY_TIMESTAMP),
	JS_CGETSET_MAGIC_DEF("namespaceURI", event_getProperty, NULL, EVENT_JSPROPERTY_NAMESPACEURI),
	JS_CGETSET_MAGIC_DEF("defaultPrevented", event_getProperty, NULL, EVENT_JSPROPERTY_DEFAULTPREVENTED),
	/*UIEvent*/
	JS_CGETSET_MAGIC_DEF("detail", event_getProperty, NULL, EVENT_JSPROPERTY_DETAIL),
	/*text, connectionEvent*/
	JS_CGETSET_MAGIC_DEF("data", event_getProperty, NULL, EVENT_JSPROPERTY_DATA),
	/*MouseEvent*/
	JS_CGETSET_MAGIC_DEF("screenX", event_getProperty, NULL, EVENT_JSPROPERTY_SCREENX),
	JS_CGETSET_MAGIC_DEF("screenY", event_getProperty, NULL, EVENT_JSPROPERTY_SCREENY),
	JS_CGETSET_MAGIC_DEF("clientX", event_getProperty, NULL, EVENT_JSPROPERTY_CLIENTX),
	JS_CGETSET_MAGIC_DEF("clientY", event_getProperty, NULL, EVENT_JSPROPERTY_CLIENTY),
	JS_CGETSET_MAGIC_DEF("button", event_getProperty, NULL, EVENT_JSPROPERTY_BUTTON),
	JS_CGETSET_MAGIC_DEF("relatedTarget", event_getProperty, NULL, EVENT_JSPROPERTY_RELATEDTARGET),
	/*wheelEvent*/
	JS_CGETSET_MAGIC_DEF("wheelDelta", event_getProperty, NULL, EVENT_JSPROPERTY_WHEELDELTA),
	/*keyboard*/
	JS_CGETSET_MAGIC_DEF("keyIdentifier", event_getProperty, NULL, EVENT_JSPROPERTY_KEYIDENTIFIER),
	JS_CGETSET_MAGIC_DEF("keyChar", event_getProperty, NULL, EVENT_JSPROPERTY_KEYCHAR),
	JS_CGETSET_MAGIC_DEF("charCode", event_getProperty, NULL, EVENT_JSPROPERTY_CHARCODE),
	/*progress*/
	JS_CGETSET_MAGIC_DEF("lengthComputable", event_getProperty, NULL, EVENT_JSPROPERTY_LENGTHCOMPUTABLE),
	JS_CGETSET_MAGIC_DEF("typeArg", event_getProperty, NULL, EVENT_JSPROPERTY_TYPEARG),
	JS_CGETSET_MAGIC_DEF("loaded", event_getProperty, NULL, EVENT_JSPROPERTY_LOADED),
	JS_CGETSET_MAGIC_DEF("total", event_getProperty, NULL, EVENT_JSPROPERTY_TOTAL),
	JS_CGETSET_MAGIC_DEF("buffering", event_getProperty, NULL, EVENT_JSPROPERTY_BUFFER_ON),
	JS_CGETSET_MAGIC_DEF("bufferLevel", event_getProperty, NULL, EVENT_JSPROPERTY_BUFFERLEVEL),
	JS_CGETSET_MAGIC_DEF("bufferRemainingTime", event_getProperty, NULL, EVENT_JSPROPERTY_BUFFERREMAININGTIME),
	JS_CGETSET_MAGIC_DEF("status", event_getProperty, NULL, EVENT_JSPROPERTY_STATUS),
	/*used by vrml*/
	JS_CGETSET_MAGIC_DEF("width", event_getProperty, NULL, EVENT_JSPROPERTY_WIDTH),
	JS_CGETSET_MAGIC_DEF("height", event_getProperty, NULL, EVENT_JSPROPERTY_HEIGHT),
	JS_CGETSET_MAGIC_DEF("offset_x", event_getProperty, NULL, EVENT_JSPROPERTY_OFFSETX),
	JS_CGETSET_MAGIC_DEF("offset_y", event_getProperty, NULL, EVENT_JSPROPERTY_OFFSETY),
	JS_CGETSET_MAGIC_DEF("vp_width", event_getProperty, NULL, EVENT_JSPROPERTY_VPWIDTH),
	JS_CGETSET_MAGIC_DEF("vp_height", event_getProperty, NULL, EVENT_JSPROPERTY_VPHEIGHT),
	JS_CGETSET_MAGIC_DEF("translation_x", event_getProperty, NULL, EVENT_JSPROPERTY_TRANSLATIONX),
	JS_CGETSET_MAGIC_DEF("translation_y", event_getProperty, NULL, EVENT_JSPROPERTY_TRANSLATIONY),
	JS_CGETSET_MAGIC_DEF("type3d", event_getProperty, NULL, EVENT_JSPROPERTY_TYPE3D),
	JS_CGETSET_MAGIC_DEF("error", event_getProperty, NULL, EVENT_JSPROPERTY_ERROR),
	JS_CGETSET_MAGIC_DEF("dynamic_scene", event_getProperty, NULL, EVENT_JSPROPERTY_DYNAMIC_SCENE),
	JS_CGETSET_MAGIC_DEF("url", event_getProperty, NULL, EVENT_JSPROPERTY_URL),

	JS_CFUNC_DEF("stopPropagation", 0, event_stop_propagation),
	JS_CFUNC_DEF("stopImmediatePropagation", 0, event_stop_immediate_propagation),
	JS_CFUNC_DEF("preventDefault", 0, event_prevent_default),
#if 0
	JS_CFUNC_DEF("initEvent", 3, event_prevent_default),
	JS_CFUNC_DEF("initEventNS", 4, event_prevent_default),
#endif
};

static const JSCFunctionListEntry nodeList_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("length", dom_nodelist_getProperty, NULL, NODELIST_JSPROPERTY_LENGTH),
	JS_CFUNC_DEF("item", 1, dom_nodelist_item),
};


void domDocument_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func)
{
	GF_SceneGraph *sg;
	sg = (GF_SceneGraph*) JS_GetOpaque_Nocheck(obj);
	/*the JS proto of the svgClass or a destroyed object*/
	if (!sg || !sg->js_data) return;
	if (!JS_IsUndefined(sg->js_data->document))
		JS_MarkValue(rt, sg->js_data->document, mark_func);
}
void domElement_gc_mark(JSRuntime *rt, JSValueConst obj, JS_MarkFunc *mark_func)
{
	GF_Node *n = (GF_Node *) JS_GetOpaque_Nocheck(obj);
	/*the JS proto of the svgClass or a destroyed object*/
	if (!n) return;
	if (!n->sgprivate || !n->sgprivate->interact || !n->sgprivate->interact->js_binding) return;

	if (!JS_IsUndefined(n->sgprivate->interact->js_binding->obj))
		JS_MarkValue(rt, n->sgprivate->interact->js_binding->obj, mark_func);
}

void dom_js_load(GF_SceneGraph *scene, JSContext *c)
{
	JSValue proto;
	JSValue global = JS_GetGlobalObject(c);
	JSRuntime *jsrt = JS_GetRuntime(c);

	if (!dom_rt) {
		GF_SAFEALLOC(dom_rt, GF_DOMRuntime);
		if (!dom_rt) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DOMJS] Failed to allocate DOM runtime\n"));
			return;
		}
		dom_rt->handlers = gf_list_new();
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] dom run-time allocated\n"));
	}
	dom_rt->nb_inst++;

	define_dom_exception(c, global);

	domNodeClass.class.gc_mark = domElement_gc_mark;
	SETUP_JSCLASS(domNodeClass, "Node", node_Funcs, NULL, dom_node_finalize, 0);
	JS_SetPropertyStr(c, proto, "ELEMENT_NODE", JS_NewInt32(c, 1) );
	JS_SetPropertyStr(c, proto, "TEXT_NODE", JS_NewInt32(c, 3));
	JS_SetPropertyStr(c, proto, "CDATA_SECTION_NODE", JS_NewInt32(c, 4));
	JS_SetPropertyStr(c, proto, "DOCUMENT_NODE", JS_NewInt32(c, 9));

	domDocumentClass.class.gc_mark = domDocument_gc_mark;
	SETUP_JSCLASS(domDocumentClass, "Document", document_Funcs, NULL, dom_document_finalize, domNodeClass.class_id);
	domElementClass.class.gc_mark = domElement_gc_mark;
	SETUP_JSCLASS(domElementClass, "Element", element_Funcs, NULL, dom_node_finalize, domNodeClass.class_id);
	domTextClass.class.gc_mark = domElement_gc_mark;
	SETUP_JSCLASS(domTextClass, "Text", text_Funcs, NULL, dom_node_finalize, domNodeClass.class_id);

	SETUP_JSCLASS(domEventClass, "Event", event_Funcs, NULL, NULL, 0);
	JS_SetPropertyStr(c, proto, "CAPTURING_PHASE", JS_NewInt32(c, 1));
	JS_SetPropertyStr(c, proto, "AT_TARGET", JS_NewInt32(c, 2));
	JS_SetPropertyStr(c, proto, "BUBBLING_PHASE", JS_NewInt32(c, 3));
	JS_SetPropertyStr(c, proto, "DOM_KEY_LOCATION_STANDARD ", JS_NewInt32(c, 0));
	JS_SetPropertyStr(c, proto, "DOM_KEY_LOCATION_LEFT", JS_NewInt32(c, 1));
	JS_SetPropertyStr(c, proto, "DOM_KEY_LOCATION_RIGHT", JS_NewInt32(c, 2));
	JS_SetPropertyStr(c, proto, "DOM_KEY_LOCATION_NUMPAD", JS_NewInt32(c, 3));


	SETUP_JSCLASS(domNodeListClass, "NodeList", nodeList_Funcs, NULL, dom_nodelist_finalize, 0);

	JS_FreeValue(c, global);
}

#ifdef GPAC_ENABLE_HTML5_MEDIA
void html_media_element_js_finalize(JSContext *c, GF_Node *n);
#endif

GF_EXPORT
void gf_sg_js_dom_pre_destroy(JSRuntime *rt, GF_SceneGraph *sg, GF_Node *n)
{
	u32 i, count;
	if (n) {
		if (n->sgprivate->interact && n->sgprivate->interact->js_binding && !JS_IsUndefined(n->sgprivate->interact->js_binding->obj) ) {
			JS_SetOpaque(n->sgprivate->interact->js_binding->obj, NULL);
			if (gf_list_del_item(sg->objects, n->sgprivate->interact->js_binding)>=0) {
				JS_SetOpaque(n->sgprivate->interact->js_binding->obj, NULL);
				JS_FreeValueRT(rt, n->sgprivate->interact->js_binding->obj);
			}
			n->sgprivate->interact->js_binding->obj = JS_UNDEFINED;
		}
		return;
	}

	/*force cleanup of scripts/handlers not destroyed - this usually happens when a script/handler node has been created in a script
	but not inserted in the graph*/
	while (gf_list_count(sg->objects)) {
		GF_Node *js_node;
		struct _node_js_binding *js_bind = (struct _node_js_binding *)gf_list_get(sg->objects, 0);
		js_node = dom_get_node(js_bind->obj);
		if (js_node) {
			if (js_node->sgprivate->tag == TAG_SVG_video || js_node->sgprivate->tag == TAG_SVG_audio) {
#ifdef GPAC_ENABLE_HTML5_MEDIA
				html_media_element_js_finalize(c, js_node);
#endif
			}
			JS_SetOpaque(js_bind->obj, NULL);
			JS_FreeValueRT(rt, js_node->sgprivate->interact->js_binding->obj);
			js_node->sgprivate->interact->js_binding->obj=JS_UNDEFINED;
			gf_node_unregister(js_node, NULL);
		}
		gf_list_rem(sg->objects, 0);
	}

	count = dom_rt ? gf_list_count(dom_rt->handlers) : 0;
	for (i=0; i<count; i++) {
		SVG_handlerElement *handler = (SVG_handlerElement *)gf_list_get(dom_rt->handlers, i);
		if (!handler->js_data) continue;

		/*if same context and same document, discard handler*/
		if (!sg || (handler->sgprivate->scenegraph==sg)) {
			/*unprotect the function*/
			JS_FreeValueRT(rt, handler->js_data->fun_val);
			gf_free(handler->js_data);
			handler->js_data = NULL;
			gf_list_rem(dom_rt->handlers, i);
			i--;
			count--;
		}
	}
	if (sg->js_data) {
		if (!JS_IsUndefined(sg->js_data->document)) {
			//detach sg from object and free it
			JS_SetOpaque(sg->js_data->document, NULL);
			JS_FreeValueRT(rt, sg->js_data->document);
		}
		gf_free(sg->js_data);
		sg->js_data = NULL;
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

JSValue dom_js_define_event(JSContext *c)
{
	JSValue global = JS_GetGlobalObject(c);

	JSValue obj = JS_NewObjectClass(c, domEventClass.class_id);
	JS_SetPropertyStr(c, global, "evt", obj);
//	JS_DefinePropertyStr(c, global, "event", obj);
	JS_FreeValue(c, global);
	return obj;
}
GF_DOM_Event *dom_get_evt_private(JSValue v)
{
	return JS_GetOpaque(v, domEventClass.class_id);
}

JSValue gf_dom_new_event(JSContext *c)
{
	return JS_NewObjectClass(c, domEventClass.class_id);
}

JSClassID dom_js_get_element_proto(JSContext *c)
{
	return domElementClass.class_id;
}
JSClassID dom_js_get_document_proto(JSContext *c)
{
	return domDocumentClass.class_id;
}
#endif	/*GPAC_HAS_QJS*/



#if 0 //unused
/*! parses the given XML document and returns a scene graph composed of GF_DOMFullNode
\param src the source file
\param scene set to the new scene graph
\return error if any
*/
GF_Err gf_sg_new_from_xml_doc(const char *src, GF_SceneGraph **scene)
{
#ifdef GPAC_HAS_QJS
	GF_Err e;
	XMLHTTPContext *ctx;
	GF_SceneGraph *sg;

	GF_SAFEALLOC(ctx, XMLHTTPContext);
	if (!ctx) return GF_OUT_OF_MEM;

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
#endif



#if 0 //unused

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
				if (!att) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[DOMJS] Failed to allocate DOM attribute\n"));
					continue;
				}
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
		evt.relatedNode = (GF_Node*) ( (evt.type == GF_EVENT_NODE_INSERTED) ? par : node);
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

GF_Err gf_sg_reload_xml_doc(const char *src, GF_SceneGraph *scene)
{
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
}
#endif


#endif	/*GPAC_DISABLE_SVG*/
