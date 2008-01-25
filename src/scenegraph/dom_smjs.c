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
#include <gpac/nodes_svg_da.h>
/*dom events*/
#include <gpac/events.h>

#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/xml.h>


#ifdef GPAC_HAS_SPIDERMONKEY

#define DOM_CORE_SETUP_CLASS(the_class, cname, flag, getp, setp, fin)	\
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
} GF_DOMRuntime;

static GF_DOMRuntime *dom_rt = NULL;

/*SVG uDOM declarations*/
JSBool svg_udom_set_property(JSContext *c, GF_Node *n, u32 svg_prop_id, jsval *vp);
JSBool svg_udom_get_property(JSContext *c, GF_Node *_n, u32 svg_prop_id, jsval *vp);
JSBool svg_udom_smil_begin(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_smil_end(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_smil_pause(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_smil_resume(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_set_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_set_float_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_set_matrix_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_set_rect_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_set_path_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_set_rgb_color_trait(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_local_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_screen_bbox(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_screen_ctm(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_create_matrix_components(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_create_rect(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_create_path(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_create_color(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_move_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_set_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
JSBool svg_udom_get_focus(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
jsval svg_udom_new_rect(JSContext *c, Fixed x, Fixed y, Fixed width, Fixed h);
jsval svg_udom_new_point(JSContext *c, Fixed x, Fixed y);


static JSBool xml_dom3_not_implemented(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{	
	return JS_FALSE;
}

/*DOM Node functions*/
#define JS_DOM3_NODE_FUNCS	\
	{"insertBefore",			xml_node_insert_before, 2},	\
	{"replaceChild",			xml_node_replace_child, 2},	\
	{"removeChild",				xml_node_remove_child, 1},	\
	{"appendChild",				xml_node_append_child, 1},	\
	{"hasChildNodes",			xml_node_has_children, 0},	\
	{"cloneNode",				xml_dom3_not_implemented, 1},	\
	{"normalize",				xml_dom3_not_implemented, 0},	\
	{"isSupported",				xml_dom3_not_implemented, 2},	\
	{"hasAttributes",			xml_node_has_attributes, 0},	\
	{"compareDocumentPosition", xml_dom3_not_implemented, 1},	\
	{"isSameNode",				xml_node_is_same_node, 1},	\
	{"lookupPrefix",			xml_dom3_not_implemented, 1},	\
	{"isDefaultNamespace",		xml_dom3_not_implemented, 1},	\
	{"lookupNamespaceURI",		xml_dom3_not_implemented, 1},	\
	/*we don't support full node compare*/	\
	{"isEqualNode",				xml_node_is_same_node, 1},	\
	{"getFeature",				xml_dom3_not_implemented, 2},	\
	{"setUserData",				xml_dom3_not_implemented, 3},	\
	{"getUserData",				xml_dom3_not_implemented, 1},

/*DOM Node properties*/
#define JS_DOM3_NODE_PROPS	\
	{"nodeName",		0,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"nodeValue",		1,       JSPROP_ENUMERATE | JSPROP_PERMANENT},	\
	{"nodeType",		2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"parentNode",		3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"childNodes",		4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"firstChild",		5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"lastChild",		6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"previousSibling",	7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"nextSibling",		8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"attributes",		9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"ownerDocument",	10,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"namespaceURI",	11,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"prefix",			12,       JSPROP_ENUMERATE | JSPROP_PERMANENT},						\
	{"localName",		13,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"baseURI",			14,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},	\
	{"textContent",		15,       JSPROP_ENUMERATE | JSPROP_PERMANENT},			

#define JS_DOM3_NODE_LAST_PROP		15
#define JS_DOM3_uDOM_FIRST_PROP		18

static void dom_node_changed(GF_Node *n, Bool child_modif, GF_FieldInfo *info)
{
	if (!info) {
		gf_node_changed(n, NULL);
	} else if (child_modif) {
		gf_node_dirty_set(n, GF_SG_CHILD_DIRTY, 0);
	} else {
		u32 flag = gf_svg_get_modification_flags((SVG_Element *)n, info);
		gf_node_dirty_set(n, flag, 0);
	}
	/*trigger rendering*/
	if (n->sgprivate->scenegraph->NodeCallback)
		n->sgprivate->scenegraph->NodeCallback(n->sgprivate->scenegraph->userpriv, GF_SG_CALLBACK_MODIFIED, n, info);
}

GF_Node *dom_get_node(JSContext *c, JSObject *obj, Bool *is_doc)
{
	if (is_doc) *is_doc = 0;
	if (JS_InstanceOf(c, obj, &dom_rt->domDocumentClass, NULL)) {
		GF_SceneGraph *sg = (GF_SceneGraph *)JS_GetPrivate(c, obj);
		if (!sg) return NULL;
		if (is_doc) *is_doc = 1;
		return sg->RootNode;
	}
	if (JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) 
		|| JS_InstanceOf(c, obj, &dom_rt->domTextClass, NULL)
		|| JS_InstanceOf(c, obj, &dom_rt->domNodeClass, NULL) 
	)
		return (GF_Node *) JS_GetPrivate(c, obj);

	return NULL;
}

static jsval dom_document_construct(JSContext *c, GF_SceneGraph *sg)
{
	JSObject *new_obj;
	if (sg->document) return OBJECT_TO_JSVAL(sg->document);

	if (sg->reference_count)
		sg->reference_count++;
	gf_node_register(sg->RootNode, NULL);
	new_obj = JS_NewObject(c, &dom_rt->domDocumentClass, 0, 0);
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
	/*in our implementation ONLY ELEMENTS are created, never attributes. We therefore always
	create Elements when asked to create a node !!*/
	return dom_base_node_construct(c, &dom_rt->domElementClass, n);
}
jsval dom_element_construct(JSContext *c, GF_Node *n)
{
	/*in our implementation ONLY ELEMENTS are created, never attributes. We therefore always
	create Elements when asked to create a node !!*/
	return dom_base_node_construct(c, &dom_rt->domElementClass, n);
}
static jsval dom_text_construct(JSContext *c, GF_Node *n)
{
	return dom_base_node_construct(c, &dom_rt->domTextClass, n);
}
Bool dom_is_element(JSContext *c, JSObject *obj)
{
	if (JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL)) return 1;
	return 0;
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

static jsval dom_node_get_sibling(JSContext *c, GF_Node *n, Bool is_prev)
{
	s32 idx;
	GF_ParentNode *par;
	if (!n) return JSVAL_VOID;
	par = (GF_ParentNode *)gf_node_get_parent(n, 0);
	if (!par) return JSVAL_VOID;

	idx = gf_node_list_find_child(par->children, n);
	if (idx<0) return JSVAL_VOID;
	if (is_prev) {
		idx--;
		if (idx<0) return JSVAL_VOID;
	}
	else idx++;
	return dom_node_construct(c, gf_node_list_get_child(par->children, idx) );
}

static char *dom_flatten_text(GF_Node *n)
{
	u32 len = 0;
	char *res = NULL;
	GF_ChildNodeItem *list;
	if (n->sgprivate->tag==TAG_DOMText) return strdup(((GF_DOMText*)n)->textContent);
	list = ((GF_ParentNode *)n)->children;
	while (list) {
		char *t = dom_flatten_text(list->node);
		if (t) {
			u32 sub_len = strlen(t);
			res = realloc(res, sizeof(char)*(len+sub_len+1));
			if (!len) res[0] = 0;
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
	if (!JS_InstanceOf(c, obj, &dom_rt->domNodeListClass, NULL) ) return JS_FALSE;
	if ((argc!=1) || !JSVAL_IS_INT(argv[0])) return JS_FALSE;

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
		return JS_FALSE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;

	switch (JSVAL_TO_INT(id)) {
	case 0:
		nl = (DOMNodeList *) JS_GetPrivate(c, obj);
		*vp = INT_TO_JSVAL( gf_node_list_get_count(nl->owner ? nl->owner->children : nl->child) );
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool dom_nodelist_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	/*no write prop*/
	return JS_FALSE;
}


/*dom event listener*/

#define JS_DOM3_EVEN_TARGET_FUNC	\
	{"addEventListenerNS", dom_event_add_listener, 4},	\
	{"removeEventListenerNS", dom_event_remove_listener, 4},	\
	{"addEventListener", dom_event_add_listener, 3},		\
	{"removeEventListener", dom_event_remove_listener, 3},	\
	{"dispatchEvent", xml_dom3_not_implemented, 1},

/*eventListeners routines used by document, element and connection interfaces*/
JSBool dom_event_add_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_FieldInfo info;
	GF_Node *listener;
	SVG_handlerElement *handler;
	char *type, *callback;
	u32 of = 0;
	u32 evtType;
	char *inNS = NULL;
	GF_Node *node = NULL;

	if (JS_InstanceOf(c, obj, &dom_rt->domDocumentClass, NULL) ) {
		GF_SceneGraph *sg = JS_GetPrivate(c, obj);
		node = sg->RootNode;
	}
	else if (JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) {
		node = JS_GetPrivate(c, obj);
	}
	/*FIXME - SVG uDOM conection not supported yet*/
	else {
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
	if (evtType==GF_EVENT_UNKNOWN) return JS_FALSE;

	listener = gf_node_new(node->sgprivate->scenegraph, TAG_SVG_listener);
	handler = (SVG_handlerElement *) gf_node_new(node->sgprivate->scenegraph, TAG_SVG_handler);
	gf_node_register(listener, node);
	gf_node_list_add_child(& ((GF_ParentNode *)node)->children, listener);
	gf_node_register((GF_Node *)handler, node);
	gf_node_list_add_child(& ((GF_ParentNode *)node)->children, (GF_Node*)handler);

	/*create attributes if needed*/
	gf_svg_get_attribute_by_tag(listener, TAG_SVG_ATT_event, 1, 0, &info);
	((XMLEV_Event*)info.far_ptr)->type = evtType;
	gf_svg_get_attribute_by_tag(listener, TAG_SVG_ATT_handler, 1, 0, &info);
	((XMLRI*)info.far_ptr)->target = (GF_Node*)handler;
	gf_svg_get_attribute_by_tag(listener, TAG_SVG_ATT_listener_target, 1, 0, &info);
	((XMLRI*)info.far_ptr)->target = node;

	gf_svg_get_attribute_by_tag((GF_Node*)handler, TAG_SVG_ATT_ev_event, 1, 0, &info);
	((XMLEV_Event*)info.far_ptr)->type = evtType;

	gf_dom_add_text_node((GF_Node *)handler, strdup(callback));
	handler->handle_event = gf_sg_handle_dom_event;
	/*don't add listener directly, post it and wait for event processing*/
	gf_dom_listener_post_add((GF_Node *) node, listener);
	return JS_TRUE;
}
JSBool dom_event_remove_listener(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *type, *callback;
	u32 of = 0;
	u32 evtType, i, count;
	char *inNS = NULL;
	GF_Node *node = NULL;

	if (JS_InstanceOf(c, obj, &dom_rt->domDocumentClass, NULL) ) {
		GF_SceneGraph *sg = JS_GetPrivate(c, obj);
		node = sg->RootNode;
	}
	else if (JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) {
		node = JS_GetPrivate(c, obj);
	} 
	/*FIXME - SVG uDOM conection not supported yet*/
	else  {
	}
	if (!node || !node->sgprivate->interact || !node->sgprivate->interact->events) return JS_FALSE;

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
	if (evtType==GF_EVENT_UNKNOWN) return JS_FALSE;
	
	count = gf_list_count(node->sgprivate->interact->events);
	for (i=0; i<count; i++) {
		GF_FieldInfo info;
		GF_DOMText *txt;
		SVG_handlerElement *hdl;
		GF_Node *el = gf_dom_listener_get(node, i);
		gf_svg_get_attribute_by_tag(el, TAG_SVG_ATT_event, 0, 0, &info);
		if (!info.far_ptr) continue;
		if (((XMLEV_Event*)info.far_ptr)->type != evtType) continue;

		gf_svg_get_attribute_by_tag(el, TAG_SVG_ATT_handler, 0, 0, &info);
		if (!info.far_ptr) continue;
		hdl = (SVG_handlerElement *) ((XMLRI*)info.far_ptr)->target;
		if (!hdl) continue;
		txt = (GF_DOMText *) hdl->children->node;
		if (txt->sgprivate->tag != TAG_DOMText) continue;
		if (!txt->textContent || strcmp(txt->textContent, callback)) continue;

		gf_dom_listener_del(node, el);
		gf_node_list_del_child( & ((GF_ParentNode *)node)->children, (GF_Node *) hdl);
		gf_node_unregister((GF_Node *) hdl, node);
		gf_node_list_del_child( & ((GF_ParentNode *)node)->children, el);
		gf_node_unregister(el, node);
		return JS_TRUE;
	}
	return JS_FALSE;
}


/*dom3 node*/
static void dom_node_finalize(JSContext *c, JSObject *obj)
{
	if (JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) 
		|| JS_InstanceOf(c, obj, &dom_rt->domTextClass, NULL)
		|| JS_InstanceOf(c, obj, &dom_rt->domNodeClass, NULL)
		) {
		GF_Node *n = (GF_Node *) JS_GetPrivate(c, obj);
		/*the JS proto of the svgClass or a destroyed object*/
		if (!n) return;
		gf_list_del_item(n->sgprivate->scenegraph->objects, obj);
		dom_unregister_node(n);
	}
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
		do_init = n->sgprivate->UserCallback ? 0 : 1;
	}

	if (pos<0) gf_node_list_add_child( & ((GF_ParentNode *)par)->children, n);
	else gf_node_list_insert_child( & ((GF_ParentNode *)par)->children, n, (u32) pos);
	gf_node_register(n, par);

	if (do_init) {
		gf_node_init(n);

		if (n->sgprivate->interact && n->sgprivate->interact->events) {
			GF_DOM_Event evt;
			memset(&evt, 0, sizeof(GF_DOM_Event));
			evt.type = GF_EVENT_LOAD;
			gf_dom_event_fire(n, NULL, &evt);
		}
	}
	/*node is being re-inserted, activate it in case*/
	if (!old_par) gf_node_activate(n);

	dom_node_changed(par, 1, NULL);
}

static JSBool xml_node_insert_before(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool is_doc;
	s32 idx;
	u32 tag;
	GF_Node *n, *target, *new_node;
	GF_ParentNode *par;
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	n = dom_get_node(c, obj, &is_doc);
	if (!n || is_doc) return JS_FALSE;

	new_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]), &is_doc);
	if (!new_node || is_doc) return JS_FALSE;

	target = NULL;
	if ((argc==2) && JSVAL_IS_OBJECT(argv[1])) {
		target = dom_get_node(c, JSVAL_TO_OBJECT(argv[1]), &is_doc);
		if (!target || is_doc) return JS_FALSE;
	}
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return JS_FALSE;
	par = (GF_ParentNode*)n;
	idx = -1;
	if (target) idx = gf_node_list_find_child(par->children, target);
	dom_node_inserted(n, new_node, idx);
	*rval = argv[0];
	return JS_TRUE;
}

static JSBool xml_node_append_child(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool is_doc;
	u32 tag;
	GF_Node *n, *new_node;
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	n = dom_get_node(c, obj, &is_doc);
	if (!n || is_doc) return JS_FALSE;

	new_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]), &is_doc);
	if (!new_node || is_doc) return JS_FALSE;
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return JS_FALSE;

	dom_node_inserted(n, new_node, -1);

	*rval = argv[0];
	return JS_TRUE;
}

static JSBool xml_node_replace_child(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool is_doc;
	s32 idx;
	u32 tag;
	GF_Node *n, *new_node, *old_node;
	GF_ParentNode *par;
	if ((argc!=2) || !JSVAL_IS_OBJECT(argv[0]) || !JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;
	n = dom_get_node(c, obj, &is_doc);
	if (!n || is_doc) return JS_FALSE;

	new_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]), &is_doc);
	if (!new_node || is_doc) return JS_FALSE;
	old_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[1]), &is_doc);
	if (!old_node || is_doc) return JS_FALSE;
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return JS_FALSE;
	par = (GF_ParentNode*)n;
	idx = gf_node_list_find_child(par->children, old_node);
	if (idx<0) return JS_FALSE;

	gf_node_list_del_child(&par->children, old_node);
	gf_node_unregister(old_node, n);

	dom_node_inserted(n, new_node, -1);
	
	*rval = argv[0];
	return JS_TRUE;
}

static JSBool xml_node_remove_child(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool is_doc;
	u32 tag;
	GF_Node *n, *old_node;
	GF_ParentNode *par;
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	n = dom_get_node(c, obj, &is_doc);
	if (!n || is_doc) return JS_FALSE;

	old_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]), &is_doc);
	if (!old_node || is_doc) return JS_FALSE;
	tag = gf_node_get_tag(n);
	if (tag==TAG_DOMText) return JS_FALSE;
	par = (GF_ParentNode*)n;

	/*if node is present in parent, unregister*/
	if (gf_node_list_del_child(&par->children, old_node)) {
		gf_node_unregister(old_node, n);
	}
	
	/*deactivate node sub-tree*/
	gf_node_deactivate(old_node);

	*rval = argv[0];
	dom_node_changed(n, 1, NULL);
	return JS_TRUE;
}


static JSBool xml_node_has_children(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool is_doc;
	GF_Node *n;
	n = dom_get_node(c, obj, &is_doc);
	if (!n || is_doc) return JS_FALSE;
	*rval = BOOLEAN_TO_JSVAL( ((GF_ParentNode*)n)->children ? JS_TRUE : JS_FALSE );
	return JS_TRUE;
}

static JSBool xml_node_has_attributes(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool is_doc;
	u32 tag;
	GF_Node *n;
	n = dom_get_node(c, obj, &is_doc);
	if (!n || is_doc) return JS_FALSE;
	tag = gf_node_get_tag(n);
	if (tag>=GF_NODE_FIRST_DOM_NODE_TAG) {
		*rval = BOOLEAN_TO_JSVAL( ((GF_DOMNode*)n)->attributes ? JS_TRUE : JS_FALSE );
	} else {
		/*not supported for other node types*/
		*rval = BOOLEAN_TO_JSVAL( JS_FALSE );
	}
	return JS_TRUE;
}

static JSBool xml_node_is_same_node(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	Bool is_doc;
	GF_Node *n, *a_node;
	if (!argc || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	n = dom_get_node(c, obj, &is_doc);
	if (!n) return JS_FALSE;
	a_node = dom_get_node(c, JSVAL_TO_OBJECT(argv[0]), &is_doc);
	if (!a_node) return JS_FALSE;
	*rval = BOOLEAN_TO_JSVAL( (a_node==n) ? JS_TRUE : JS_FALSE);
	return JS_TRUE;
}

static JSBool dom_node_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 tag;
	Bool is_doc;
	GF_Node *n;
	GF_ParentNode *par;
	
	n = dom_get_node(c, obj, &is_doc);
	if (!n) return JS_FALSE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;

	tag = gf_node_get_tag(n);
	par = (GF_ParentNode*)n;

	switch (JSVAL_TO_INT(id)) {
	/*"nodeName"*/
	case 0:
		if (tag==TAG_DOMText) {
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->is_cdata) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "#cdata-section") );
			else *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "#text") );
		}
		else if (is_doc) {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "#document") );
		}
		else {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, gf_node_get_class_name(n) ) );
		}
		return JS_TRUE;
	/*"nodeValue"*/
	case 1:
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"nodeType"*/
	case 2:
		if (tag==TAG_DOMText) {
			GF_DOMText *txt = (GF_DOMText *)n;
			if (txt->is_cdata) *vp = INT_TO_JSVAL(4);
			else *vp = INT_TO_JSVAL(3);
		}
		else if (is_doc) *vp = INT_TO_JSVAL(9);
		else *vp = INT_TO_JSVAL(1);
		return JS_TRUE;
	/*"parentNode"*/
	case 3:
		if (is_doc) {
			*vp = JSVAL_VOID;
		} else {
			*vp = dom_node_construct(c, gf_node_get_parent(n, 0) );
		}
		return JS_TRUE;
	/*"childNodes"*/
	case 4:
		if (tag==TAG_DOMText) *vp = JSVAL_VOID;
		else *vp = dom_nodelist_construct(c, par);
		return JS_TRUE;
	/*"firstChild"*/
	case 5:
		if (tag==TAG_DOMText) *vp = JSVAL_VOID;
		else if (!par->children) {
			/*our imp always remove empty text node, create one*/
		/*
			GF_DOMText *txt = gf_dom_add_text_node(n, NULL);
			*vp = dom_node_construct(c, (GF_Node*)txt);
		*/
			/*svg uDOM*/
			*vp = JSVAL_VOID;
		}
		else *vp = dom_node_construct(c, par->children->node);
		return JS_TRUE;
	/*"lastChild"*/
	case 6:
		if ((tag==TAG_DOMText) || !par->children) *vp = JSVAL_VOID;
		else *vp = dom_node_construct(c, gf_node_list_get_child(par->children, -1) );
		return JS_TRUE;
	/*"previousSibling"*/
	case 7:
		*vp = dom_node_get_sibling(c, n, 1);
		return JS_TRUE;
	/*"nextSibling"*/
	case 8:
		*vp = dom_node_get_sibling(c, n, 0);
		return JS_TRUE;
	/*"attributes"*/
	case 9:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"ownerDocument"*/
	case 10:
		*vp = dom_document_construct(c, n->sgprivate->scenegraph);
		return JS_TRUE;
	/*"namespaceURI"*/
	case 11:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"prefix"*/
	case 12:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"localName"*/
	case 13:
		if (tag!=TAG_DOMText) {
			*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, gf_node_get_class_name(n) ) );
		} else {
			*vp = JSVAL_VOID;
		}
		return JS_TRUE;
	/*"baseURI"*/
	case 14:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"textContent"*/
	case 15:
	{
		char *res = dom_flatten_text(n);
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, res) );
		free(res);
	}
		return JS_TRUE;
	}
	/*not supported*/
	return JS_FALSE;
}

static JSBool dom_node_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 tag;
	Bool is_doc;
	GF_Node *n;
	GF_ParentNode *par;

	n = dom_get_node(c, obj, &is_doc);
	if (!n) return JS_FALSE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;

	tag = gf_node_get_tag(n);
	par = (GF_ParentNode*)n;

	switch (JSVAL_TO_INT(id)) {
	/*"nodeValue"*/
	case 1:
		/*we only support element and sg in the Node interface, no set*/
		return JS_FALSE;
	/*"prefix"*/
	case 12:
		/*NOT SUPPORTED YET*/
		return JS_TRUE;
	/*"textContent"*/
	case 15:
		if (!JSVAL_IS_STRING(*vp)) return JS_FALSE;
		/*reset all children*/
		gf_node_unregister_children(n, par->children);
		par->children = NULL;
		gf_dom_add_text_node(n, strdup( JS_GetStringBytes(JSVAL_TO_STRING(*vp)) ) );
		dom_node_changed(n, 1, NULL);
		return JS_TRUE;
	}
	
	/*not supported*/
	return JS_FALSE;
}


/*dom3 document*/

/*don't attempt to do anything with the scenegraph, it may be destroyed
fortunately a sg cannot be created like that...*/
static void dom_document_finalize(JSContext *c, JSObject *obj)
{
	GF_SceneGraph *sg;
	if (!JS_InstanceOf(c, obj, &dom_rt->domDocumentClass, NULL) ) 
		return;

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
	Bool is_doc;
	u32 prop_id;
	GF_Node *n = dom_get_node(c, obj, &is_doc);
	if (!n || !is_doc) return JS_FALSE;

	*vp = JSVAL_VOID;
	if (!JSVAL_IS_INT(id)) return JS_FALSE;
	prop_id = JSVAL_TO_INT(id);
	if (prop_id<=JS_DOM3_NODE_LAST_PROP) return dom_node_getProperty(c, obj, id, vp);

	switch (prop_id) {
	/*"doctype"*/
	case JS_DOM3_NODE_LAST_PROP+1:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;

	/*implementation - FIXME, this is wrong, we should have our own implementation
		but at the current time we rely on the global object to provide it*/
	case JS_DOM3_NODE_LAST_PROP+2:
		*vp = OBJECT_TO_JSVAL( JS_GetGlobalObject(c) ); 
		return JS_TRUE;

	/*"documentElement"*/
	case JS_DOM3_NODE_LAST_PROP+3:
		*vp = dom_element_construct(c, n);
		return JS_TRUE;
	/*"inputEncoding"*/
	case JS_DOM3_NODE_LAST_PROP+4:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"xmlEncoding"*/
	case JS_DOM3_NODE_LAST_PROP+5:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"xmlStandalone"*/
	case JS_DOM3_NODE_LAST_PROP+6:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"xmlVersion"*/
	case JS_DOM3_NODE_LAST_PROP+7:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"strictErrorChecking"*/
	case JS_DOM3_NODE_LAST_PROP+8:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"documentURI"*/
	case JS_DOM3_NODE_LAST_PROP+9:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*"domConfig"*/
	case JS_DOM3_NODE_LAST_PROP+10:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	/*global*/
	case JS_DOM3_NODE_LAST_PROP+11:
		*vp = OBJECT_TO_JSVAL( JS_GetGlobalObject(c) ); 
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool dom_document_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	Bool is_doc;
	GF_Node *n = dom_get_node(c, obj, &is_doc);
//	if (!n || !is_doc) return JS_FALSE;
	if (!n || !is_doc) return JS_TRUE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);
	if (id<=JS_DOM3_NODE_LAST_PROP) return dom_node_setProperty(c, obj, id, vp);

	switch (prop_id) {
	/*"xmlStandalone"*/
	case JS_DOM3_NODE_LAST_PROP+6:
		break;
	/*"xmlVersion"*/
	case JS_DOM3_NODE_LAST_PROP+7:
		break;
	/*"strictErrorChecking"*/
	case JS_DOM3_NODE_LAST_PROP+8:
		break;
	/*"documentURI"*/
	case JS_DOM3_NODE_LAST_PROP+9:
		break;
	/*"domConfig"*/
	case JS_DOM3_NODE_LAST_PROP+10:
		break;
	/*the rest is read-only*/
	}
	return JS_FALSE;
}

static JSBool xml_document_create_element(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag;
	GF_Node *n;
	GF_SceneGraph *sg;
	char *name;
	if (!JS_InstanceOf(c, obj, &dom_rt->domDocumentClass, NULL) || !argc || !JSVAL_IS_STRING(argv[0]) ) return JS_FALSE;
	sg = (GF_SceneGraph *) JS_GetPrivate(c, obj);
	if (!sg) return JS_FALSE;
	name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (!name) return JS_FALSE;
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
		name = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	}
	if (!name) return JS_FALSE;
	/*browse all our supported DOM implementations*/
	tag = gf_svg_get_element_tag(name);
	if (!tag) tag = TAG_DOMFullNode;
	n = gf_node_new(sg, tag);
	*rval = dom_element_construct(c, n);
	return JS_TRUE;
}

static JSBool xml_document_create_text(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n;
	GF_SceneGraph *sg;
	if (!JS_InstanceOf(c, obj, &dom_rt->domDocumentClass, NULL) ) return JS_FALSE;
	sg = (GF_SceneGraph *) JS_GetPrivate(c, obj);
	if (!sg) return JS_FALSE;

	n = gf_node_new(sg, TAG_DOMText);
	if (argc) {
		((GF_DOMText*)n)->textContent = strdup( JS_GetStringBytes(JS_ValueToString(c, argv[0])) );
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
	GF_SceneGraph *sg;
	JSObject *new_obj;
	char *name;
	if (!JS_InstanceOf(c, obj, &dom_rt->domDocumentClass, NULL) ) return JS_FALSE;
	sg = (GF_SceneGraph *) JS_GetPrivate(c, obj);
	if (!sg) return JS_FALSE;

	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
		name = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
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
	GF_Node *n;
	GF_SceneGraph *sg;
	char *id;
	if (!JS_InstanceOf(c, obj, &dom_rt->domDocumentClass, NULL) ) return JS_FALSE;
	sg = (GF_SceneGraph *) JS_GetPrivate(c, obj);
	if (!sg) return JS_FALSE;

	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	id = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	n = gf_sg_find_node_by_name(sg, id);
	*rval = dom_element_construct(c, n);
	return JS_TRUE;
}



/*dom3 element*/

static JSBool dom_element_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	Bool is_doc;
	GF_Node *n;
	if (!JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) return JS_FALSE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);
	if (prop_id<=JS_DOM3_NODE_LAST_PROP) return dom_node_getProperty(c, obj, id, vp);

	n = dom_get_node(c, obj, &is_doc);

	switch (prop_id) {
	/*"tagName"*/
	case JS_DOM3_NODE_LAST_PROP+1:
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, gf_node_get_class_name(n) ) );
		return JS_TRUE;
	/*"schemaTypeInfo"*/
	case JS_DOM3_NODE_LAST_PROP+2:
		/*NOT SUPPORTED YET*/
		*vp = JSVAL_VOID;
		return JS_TRUE;
	case JS_DOM3_NODE_LAST_PROP+3:
	case JS_DOM3_NODE_LAST_PROP+4:
	case JS_DOM3_NODE_LAST_PROP+5:
	case JS_DOM3_NODE_LAST_PROP+6:
	case JS_DOM3_NODE_LAST_PROP+7:
	case JS_DOM3_NODE_LAST_PROP+8:
	case JS_DOM3_NODE_LAST_PROP+9:
	case JS_DOM3_NODE_LAST_PROP+10:
		return svg_udom_get_property(c, n, prop_id-(JS_DOM3_NODE_LAST_PROP+3), vp);
	}
	return JS_FALSE;
}
static JSBool dom_element_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	u32 prop_id;
	if (!JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) return JS_FALSE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);
	if (prop_id<=JS_DOM3_NODE_LAST_PROP) return dom_node_setProperty(c, obj, id, vp);

	switch (prop_id) {
	case JS_DOM3_NODE_LAST_PROP+3:
	case JS_DOM3_NODE_LAST_PROP+4:
	case JS_DOM3_NODE_LAST_PROP+5:
	case JS_DOM3_NODE_LAST_PROP+6:
	case JS_DOM3_NODE_LAST_PROP+7:
	case JS_DOM3_NODE_LAST_PROP+8:
	case JS_DOM3_NODE_LAST_PROP+9:
		return svg_udom_set_property(c, dom_get_node(c, obj, NULL), prop_id-(JS_DOM3_NODE_LAST_PROP+3), vp);
	default:
		/*rest is read only*/
		return JS_FALSE;
	}
}


static JSBool xml_element_get_attribute(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n;
	char *name;
	if (!JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) return JS_FALSE;
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	n = dom_get_node(c, obj, NULL);
	name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
		name = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	}
	if (!name) return JS_FALSE;
	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATTRIBUTE_FULL) && !strcmp(att->name, name)) {
				*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, (char*)att->data ) );
				return JS_TRUE;
			}
			att = (GF_DOMFullAttribute *) att->next;
		}
	}
	else if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) {
		GF_FieldInfo info;
		if (gf_svg_get_attribute_by_name(n, name, 0, 0, &info)==GF_OK) {
			char szAtt[4096];
			gf_svg_dump_attribute(n, &info, szAtt);
			*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, szAtt) );
			return JS_TRUE;
		}
	}
	*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "") );
	return JS_TRUE;
}


static JSBool xml_element_has_attribute(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Node *n;
	char *name;
	if (!JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) return JS_FALSE;
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	n = dom_get_node(c, obj, NULL);
	name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
		name = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	}
	if (!name) return JS_FALSE;
	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATTRIBUTE_FULL) && !strcmp(att->name, name)) {
				*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
				return JS_TRUE;
			}
			att = (GF_DOMFullAttribute *) att->next;
		}
	}
	else if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) {
		GF_FieldInfo info;
		if (gf_svg_get_attribute_by_name(n, name, 0, 0, &info)==GF_OK) {
			*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
			return JS_TRUE;
		}
	}
	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
	return JS_TRUE;
}



static JSBool xml_element_remove_attribute(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 tag;
	GF_DOMFullNode *node;
	GF_DOMFullAttribute *prev, *att;
	GF_Node *n;
	char *name;
	if (!JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) return JS_FALSE;
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	n = dom_get_node(c, obj, NULL);
	name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
		name = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	}
	if (!name) return JS_FALSE;

	tag = TAG_DOM_ATTRIBUTE_FULL;
	node = (GF_DOMFullNode*)n;
	prev = NULL;
	att = (GF_DOMFullAttribute*)node->attributes;

	if (n->sgprivate->tag==TAG_DOMFullNode) tag = TAG_DOM_ATTRIBUTE_FULL;
	else if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) tag = gf_svg_get_attribute_tag(n->sgprivate->tag, name);

	while (att) {
		if ((att->tag==TAG_DOM_ATTRIBUTE_FULL) && !strcmp(att->name, name)) {
			if (prev) prev->next = att->next;
			else node->attributes = att->next;
			if (att->data) free(att->data);
			free(att->name);
			free(att);
			dom_node_changed(n, 0, NULL);
			return JS_TRUE;
		} else if (tag==att->tag) {
			if (prev) prev->next = att->next;
			else node->attributes = att->next;
			gf_svg_delete_attribute_value(att->data_type, att->data, n->sgprivate->scenegraph);
			free(att);
			dom_node_changed(n, 0, NULL);
			return JS_TRUE;
		}
		prev = att;
		att = (GF_DOMFullAttribute *) att->next;
	}
	return JS_TRUE;
}


static JSBool xml_element_set_attribute(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 evtType;
	GF_Node *n;
	char *name, *val;
	if (!JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) return JS_FALSE;
	if ((argc < 2) || !JSVAL_IS_STRING(argv[0]) ) return JS_FALSE;
	n = dom_get_node(c, obj, NULL);
	name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	/*may not be a string in DOM*/
	val = JS_GetStringBytes(JS_ValueToString(c, argv[1]));
	/*NS version*/
	if (argc==3) {
		name = val;
		val = JS_GetStringBytes(JS_ValueToString(c, argv[2]));
	}
	if (!name || !val) return JS_FALSE;

	if ((name[0]=='o') && (name[1]=='n')) {
		evtType = gf_dom_event_type_by_name(name + 2);
		if (evtType != GF_EVENT_UNKNOWN) {
			/*check if we're modifying an existing listener*/
			SVG_handlerElement *handler;
			u32 i, count = (n->sgprivate->interact && n->sgprivate->interact->events) ? gf_list_count(n->sgprivate->interact->events) : 0;
			for (i=0;i<count; i++) {
				GF_FieldInfo info;
				GF_DOMText *text;
				GF_Node *listen = gf_list_get(n->sgprivate->interact->events, i);

				gf_svg_get_attribute_by_tag(listen, TAG_SVG_ATT_event, 0, 0, &info);
				if (!info.far_ptr || (((XMLEV_Event*)info.far_ptr)->type != evtType)) continue;
				gf_svg_get_attribute_by_tag(listen, TAG_SVG_ATT_handler, 0, 0, &info);
				assert(info.far_ptr);
				handler = (SVG_handlerElement *) ((XMLRI*)info.far_ptr)->target;
				text = (GF_DOMText*)handler->children->node;
				if (text->sgprivate->tag==TAG_DOMText) {
					if (text->textContent) free(text->textContent);
					text->textContent = strdup(val);
				}
				return JS_TRUE;
			}
			/*nope, create a listener*/
			handler = gf_dom_listener_build(n, evtType, 0, NULL);
			gf_dom_add_text_node((GF_Node*)handler, strdup(val) );
			return JS_TRUE;
		}
	}

	if (n->sgprivate->tag==TAG_DOMFullNode) {
		GF_DOMFullAttribute *prev = NULL;
		GF_DOMFullNode *node = (GF_DOMFullNode*)n;
		GF_DOMFullAttribute *att = (GF_DOMFullAttribute*)node->attributes;
		while (att) {
			if ((att->tag==TAG_DOM_ATTRIBUTE_FULL) && !strcmp(att->name, name)) {
				if (att->data) free(att->data);
				att->data = strdup(val);
				dom_node_changed(n, 0, NULL);
				return JS_TRUE;
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
		return JS_TRUE;
	}
	
	if (n->sgprivate->tag<=GF_NODE_RANGE_LAST_SVG) {
		GF_FieldInfo info;
		if (gf_svg_get_attribute_by_name(n, name, 1, 1, &info)==GF_OK) {
			gf_svg_parse_attribute(n, &info, val, 0);
			dom_node_changed(n, 0, &info);
			return JS_TRUE;
		}
	}
	return JS_TRUE;
}


static JSBool xml_element_elements_by_tag(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	DOMNodeList *nl;
	GF_Node *n;
	JSObject *new_obj;
	char *name;
	if (!JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) return JS_FALSE;
	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	n = dom_get_node(c, obj, NULL);
	if (!n) return JS_FALSE;

	name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	/*NS version*/
	if (argc==2) {
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
		name = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
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
	GF_Node *n;
	const char *node_name;
	u32 node_id;
	char *name;
	Bool is_id;
	if (!JS_InstanceOf(c, obj, &dom_rt->domElementClass, NULL) ) return JS_FALSE;
	if ((argc<2) || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	n = dom_get_node(c, obj, NULL);
	if (!n) return JS_FALSE;

	name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

	/*NS version*/
	if (argc==2) {
		if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
		name = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
		is_id = (JSVAL_TO_BOOLEAN(argv[2])==JS_TRUE) ? 1 : 0;
	} else {
		is_id = (JSVAL_TO_BOOLEAN(argv[1])==JS_TRUE) ? 1 : 0;
	}
	node_name = gf_node_get_name_and_id(n, &node_id);
	if (node_id && is_id) {
		/*we only support ONE ID per node*/
		return JS_FALSE;
	}
	if (is_id) {
		if (!name) return JS_FALSE;
		gf_node_set_id(n, gf_sg_get_max_node_id(n->sgprivate->scenegraph) + 1, strdup(name) );
	} else if (node_id) {
		gf_node_remove_id(n);
	}
	return JS_TRUE;
}


/*dom3 character/text/comment*/

static JSBool dom_text_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_DOMText *txt;
	u32 prop_id;

	if (!JS_InstanceOf(c, obj, &dom_rt->domTextClass, NULL))
		return JS_FALSE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);
	if (prop_id<=JS_DOM3_NODE_LAST_PROP) return dom_node_getProperty(c, obj, id, vp);

	txt = (GF_DOMText*)dom_get_node(c, obj, NULL);
	if (!txt) return JS_FALSE;

	switch (prop_id) {
	/*"data"*/
	case JS_DOM3_NODE_LAST_PROP+1:
		if (txt->textContent) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, txt->textContent ) );
		else *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "") );
		return JS_TRUE;
	/*"length"*/
	case JS_DOM3_NODE_LAST_PROP+2:
		*vp = INT_TO_JSVAL(txt->textContent ? strlen(txt->textContent) : 0);
		return JS_TRUE;
	/*"isElementContentWhitespace"*/
	case JS_DOM3_NODE_LAST_PROP+3:
		*vp = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	/*"wholeText"*/
	case JS_DOM3_NODE_LAST_PROP+4:
		/*FIXME - this is wrong*/
		*vp = INT_TO_JSVAL(txt->textContent ? strlen(txt->textContent) : 0);
		return JS_TRUE;
	}
	return JS_FALSE;
}
static JSBool dom_text_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	GF_DOMText *txt;
	u32 prop_id;
	if (!JS_InstanceOf(c, obj, &dom_rt->domTextClass, NULL))
		return JS_FALSE;

	if (!JSVAL_IS_INT(id)) return JS_TRUE;
	prop_id = JSVAL_TO_INT(id);
	if (prop_id<=JS_DOM3_NODE_LAST_PROP) return dom_node_setProperty(c, obj, id, vp);

	txt = (GF_DOMText*)dom_get_node(c, obj, NULL);
	if (!txt) return JS_FALSE;

	switch (prop_id) {
	/*"data"*/
	case JS_DOM3_NODE_LAST_PROP+1:
		if (txt->textContent) free(txt->textContent);
		txt->textContent = NULL;
		if (JSVAL_IS_STRING(*vp)) {
			txt->textContent = strdup( JS_GetStringBytes(JSVAL_TO_STRING(*vp)) );
		}
		dom_node_changed((GF_Node*)txt, 0, NULL);
		return JS_TRUE;
	/*the rest is read-only*/
	}
	return JS_FALSE;
}

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
		case 1: 
			*vp = dom_element_construct(c, evt->target); 
			return JS_TRUE;
		/*currentTarget*/
		case 2: *vp = dom_element_construct(c, evt->currentTarget); return JS_TRUE;
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
		case 12: *vp = dom_element_construct(c, evt->relatedTarget); return JS_TRUE;
		/*DOM3 event keyIndentifier*/
		case 14: 
			s = JS_NewStringCopyZ(c, gf_dom_get_key_name(evt->detail) );
			*vp = STRING_TO_JSVAL( s );
			 return JS_TRUE;
		/*Mozilla keyChar, charCode: wrap up to same value*/
		case 15: *vp = INT_TO_JSVAL(evt->detail); return JS_TRUE;

		/*zoomRectScreen*/
		case 21:
			*vp = svg_udom_new_rect(c, evt->screen_rect.x, evt->screen_rect.y, evt->screen_rect.width, evt->screen_rect.height);
			return JS_TRUE;
		/*previousScale*/
		case 22:
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, evt->prev_scale) );
			return JS_TRUE;
		/*previousTranslate*/
		case 23:
			*vp = svg_udom_new_point(c, evt->prev_translate.x, evt->prev_translate.y);
			return JS_TRUE;
		/*newScale*/
		case 24:
			*vp = DOUBLE_TO_JSVAL( JS_NewDouble(c, evt->new_scale) );
			return JS_TRUE;
		/*newTranslate*/
		case 25:
			*vp = svg_udom_new_point(c, evt->new_translate.x, evt->new_translate.y);
			return JS_TRUE;
		default: return JS_FALSE;
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
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_FALSE;
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

	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_FALSE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_FALSE;

	/*reset*/
	if (ctx->readyState) xml_http_reset(ctx);

	if (argc<2) return JS_FALSE;
	/*method is a string*/
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	/*url is a string*/
	if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;

	xml_http_reset(ctx);
	val = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (strcmp(val, "GET") && strcmp(val, "POST") && strcmp(val, "HEAD")
	&& strcmp(val, "PUT") && strcmp(val, "DELETE") && strcmp(val, "OPTIONS") )
		return JS_FALSE;

	ctx->method = strdup(val);

	val = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
	/*concatenate URL*/
	scene = xml_get_scenegraph(c);
	ScriptAction(scene, GF_JSAPI_OP_GET_SCENE_URI, scene->RootNode, &par);

	if (par.uri.url) ctx->url = gf_url_concatenate(par.uri.url, val);
	if (!ctx->url) ctx->url = strdup(val);

	/*async defaults to true*/
	ctx->async = 1;
	if (argc>2) {
		ctx->async = (JSVAL_TO_BOOLEAN(argv[2])==JS_TRUE) ? 1 : 0;
		if (argc>3) {
			if (!JSVAL_IS_STRING(argv[3])) return JS_FALSE;
			val = JS_GetStringBytes(JSVAL_TO_STRING(argv[3]));
			/*TODO*/
			if (argc>4) {
				if (!JSVAL_IS_STRING(argv[3])) return JS_FALSE;
				val = JS_GetStringBytes(JSVAL_TO_STRING(argv[3]));
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
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_FALSE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_FALSE;

	if (ctx->readyState!=1) return JS_FALSE;
	if (argc!=2) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;

	hdr = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	val = JS_GetStringBytes(JSVAL_TO_STRING(argv[1]));
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
		GF_DOMFullAttribute *att;
		GF_SAFEALLOC(att, GF_DOMFullAttribute);
		att->tag = TAG_DOM_ATTRIBUTE_FULL;
		att->name = strdup(attributes[i].name);
		att->data = strdup(attributes[i].value);
		if (prev) prev->next = (GF_DOMAttribute*)att;
		else node->attributes = (GF_DOMAttribute*)att;
		prev = att;
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
		txt->is_cdata = is_cdata;
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
		if (!strcmp(parameter->value, "application/xml")
			|| !strcmp(parameter->value, "text/xml")
			|| strstr(parameter->value, "+xml")
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

	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_FALSE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_FALSE;

	if (ctx->readyState!=1) return JS_FALSE;
	if (ctx->sess) return JS_FALSE;

	if (argc) {
		if (JSVAL_IS_NULL(argv[0])) {
		} else  if (JSVAL_IS_OBJECT(argv[0])) {
//			if (!JS_InstanceOf(c, JSVAL_TO_OBJECT(argv[0]), &documentClass, NULL) ) return JS_FALSE;
			
			/*NOT SUPPORTED YET, we must serialize the sg*/
			return JS_FALSE;
		} else {
			if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
			data = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
		}
	}

	scene = xml_get_scenegraph(c);
	par.dnld_man = NULL;
	ScriptAction(scene, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
	if (!par.dnld_man) return JS_FALSE;

	/*reset previous text*/
	if (ctx->data) free(ctx->data);

	ctx->data = data ? strdup(data) : NULL;
	ctx->sess = gf_dm_sess_new(par.dnld_man, ctx->url, GF_NETIO_SESSION_NOT_CACHED, xml_http_on_data, ctx, &e);
	if (!ctx->sess) return JS_FALSE;

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
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_FALSE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_FALSE;

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
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_FALSE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_FALSE;

	/*must be received or loaded*/
	if (ctx->readyState<3) return JS_FALSE;
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
	if (!argc || !JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_FALSE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	hdr = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	/*must be received or loaded*/
	if (ctx->readyState<3) return JS_FALSE;
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
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_FALSE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_FALSE;

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
			if (ctx->readyState<3) return JS_FALSE;
			if (ctx->data) {
				s = JS_NewStringCopyZ(c, ctx->data);
				*vp = STRING_TO_JSVAL( s );
			} else {
				*vp = JSVAL_VOID;
			}
			return JS_TRUE;
		/*responseXML*/
		case 3: 
			if (ctx->readyState<3) return JS_FALSE;
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
			return JS_FALSE;
		}
	}
	return JS_PropertyStub(c, obj, id, vp);
	return JS_TRUE;
}

static JSBool xml_http_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
	XMLHTTPContext *ctx;
	if (!JS_InstanceOf(c, obj, &dom_rt->xmlHTTPRequestClass, NULL) ) return JS_FALSE;
	ctx = (XMLHTTPContext *)JS_GetPrivate(c, obj);
	if (!ctx) return JS_FALSE;

	if (JSVAL_IS_INT(id)) {
		switch (JSVAL_TO_INT(id)) {
		/*onreadystatechange*/
		case 0: 
			break;
		/*all other properties are read-only*/
		default:
			return JS_FALSE;
		}
		if (JSVAL_IS_VOID(*vp)) {
			ctx->onreadystatechange = NULL;
			return JS_TRUE;
		}
		else if (JSVAL_IS_STRING(*vp)) {
			jsval fval;
			char *callback = JS_GetStringBytes(JSVAL_TO_STRING(*vp));
			if (! JS_LookupProperty(c, JS_GetGlobalObject(c), callback, &fval)) return JS_FALSE;
			ctx->onreadystatechange = JS_ValueToFunction(c, fval);
			if (ctx->onreadystatechange) return JS_TRUE;
		} else if (JSVAL_IS_OBJECT(*vp)) {
			ctx->onreadystatechange = JS_ValueToFunction(c, *vp);
			if (ctx->onreadystatechange) return JS_TRUE;
		}
		return JS_FALSE;
	}
	return JS_TRUE;
}


void dom_js_load(JSContext *c, JSObject *global)
{
	if (!dom_rt) {
		GF_SAFEALLOC(dom_rt, GF_DOMRuntime);
		DOM_CORE_SETUP_CLASS(dom_rt->domNodeClass, "Node", JSCLASS_HAS_PRIVATE, dom_node_getProperty, dom_node_setProperty, dom_node_finalize);
		DOM_CORE_SETUP_CLASS(dom_rt->domDocumentClass, "Document", JSCLASS_HAS_PRIVATE, dom_document_getProperty, dom_document_setProperty, dom_document_finalize);
		/*Element uses the same destructor as node*/
		DOM_CORE_SETUP_CLASS(dom_rt->domElementClass, "Element", JSCLASS_HAS_PRIVATE, dom_element_getProperty, dom_element_setProperty, dom_node_finalize);
		/*Text uses the same destructor as node*/
		DOM_CORE_SETUP_CLASS(dom_rt->domTextClass, "Text", JSCLASS_HAS_PRIVATE, dom_text_getProperty, dom_text_setProperty, dom_node_finalize);
		/*Event class*/
		DOM_CORE_SETUP_CLASS(dom_rt->domEventClass , "Event", JSCLASS_HAS_PRIVATE, event_getProperty, JS_PropertyStub, JS_FinalizeStub);
		DOM_CORE_SETUP_CLASS(dom_rt->domNodeListClass, "NodeList", JSCLASS_HAS_PRIVATE, dom_nodelist_getProperty, dom_nodelist_setProperty, dom_nodelist_finalize);
		DOM_CORE_SETUP_CLASS(dom_rt->xmlHTTPRequestClass, "XMLHttpRequest", JSCLASS_HAS_PRIVATE, xml_http_getProperty, xml_http_setProperty, xml_http_finalize);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] dom run-time allocated\n"));
	}
	dom_rt->nb_inst++;

	{
		JSFunctionSpec nodeFuncs[] = {
			JS_DOM3_NODE_FUNCS
			{0}
		};
		JSPropertySpec nodeProps[] = {
			JS_DOM3_NODE_PROPS
			{0}
		};
		JS_InitClass(c, global, 0, &dom_rt->domNodeClass, 0, 0, nodeProps, nodeFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] node class initialized\n"));
	}
	{
		JSFunctionSpec documentFuncs[] = {
			JS_DOM3_NODE_FUNCS
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
			JS_DOM3_EVEN_TARGET_FUNC
			/*DocumentEvent interface*/
			{"createEvent", xml_dom3_not_implemented, 1},
			{0}
		};

		JSPropertySpec documentProps[] = {
			JS_DOM3_NODE_PROPS
			{"doctype",				JS_DOM3_NODE_LAST_PROP+1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"implementation",		JS_DOM3_NODE_LAST_PROP+2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"documentElement",		JS_DOM3_NODE_LAST_PROP+3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"inputEncoding",		JS_DOM3_NODE_LAST_PROP+4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"xmlEncoding",			JS_DOM3_NODE_LAST_PROP+5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"xmlStandalone",		JS_DOM3_NODE_LAST_PROP+6,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"xmlVersion",			JS_DOM3_NODE_LAST_PROP+7,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"strictErrorChecking",	JS_DOM3_NODE_LAST_PROP+8,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"documentURI",			JS_DOM3_NODE_LAST_PROP+9,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"domConfig",			JS_DOM3_NODE_LAST_PROP+10,      JSPROP_ENUMERATE | JSPROP_PERMANENT},
			/*svgDocument interface*/
			{"global",				JS_DOM3_NODE_LAST_PROP+11,      JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};

		JS_InitClass(c, global, 0, &dom_rt->domDocumentClass, 0, 0, documentProps, documentFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] document class initialized\n"));
	}
	
	{
		JSFunctionSpec elementFuncs[] = {
			JS_DOM3_NODE_FUNCS
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
			JS_DOM3_EVEN_TARGET_FUNC	

			/*SVG uDOM extensions*/

			/*timeControl interface*/
			{"beginElementAt", svg_udom_smil_begin, 1},
			{"beginElement", svg_udom_smil_begin, 0},
			{"endElementAt", svg_udom_smil_end, 1},
			{"endElement", svg_udom_smil_end, 0},
			{"pauseElement", svg_udom_smil_pause, 0},
			{"resumeElement", svg_udom_smil_resume, 0},
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
			{"moveFocus", svg_udom_move_focus, 0},
			{"setFocus", svg_udom_set_focus, 0},
			{"getCurrentFocusedObject", svg_udom_get_focus, 0},
			{0}
		};

		JSPropertySpec elementProps[] = {
			JS_DOM3_NODE_PROPS
			{"tagName",				JS_DOM3_NODE_LAST_PROP+1,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"schemaTypeInfo",		JS_DOM3_NODE_LAST_PROP+2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},

			/*SVG uDOM extensions*/

			/*elementTraversal interface - mapped to node interface*/
			/*= firstChild (id=5)*/
			{"firstElementChild",		5,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*= lastChild (id=6)*/
			{"lastElementChild",		6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*= previousSibling (id=7)*/
			{"previousElementSibling",	7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*= nextSibling (id=8)*/
			{"nextElementSibling",		8,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*svgElement interface*/
			{"id",						JS_DOM3_NODE_LAST_PROP+3,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			/*svgSVGElement interface*/
			{"currentScale",			JS_DOM3_NODE_LAST_PROP+4,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"currentRotate",			JS_DOM3_NODE_LAST_PROP+5,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"currentTranslate",		JS_DOM3_NODE_LAST_PROP+6,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"viewport",				JS_DOM3_NODE_LAST_PROP+7,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"currentTime",				JS_DOM3_NODE_LAST_PROP+8,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			/*timeControl interface*/
			{"isPaused",				JS_DOM3_NODE_LAST_PROP+9,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*old SVG1.1 stuff*/
			{"ownerSVGElement",			JS_DOM3_NODE_LAST_PROP+10, 0},
			{0}
		};
		JS_InitClass(c, global, 0, &dom_rt->domElementClass, 0, 0, elementProps, elementFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] element class initialized\n"));
	}

	{
		JSFunctionSpec textFuncs[] = {
			JS_DOM3_NODE_FUNCS
			{"substringData",			xml_dom3_not_implemented, 2},
			{"appendData",				xml_dom3_not_implemented, 1},
			{"insertData",				xml_dom3_not_implemented, 2},
			{"deleteData",				xml_dom3_not_implemented, 2},
			{"replaceData",				xml_dom3_not_implemented, 3},
			/*text*/
			{"splitText",				xml_dom3_not_implemented, 1},
			{"replaceWholeText",		xml_dom3_not_implemented, 1},
			{0}
		};
		JSPropertySpec textProps[] = {
			JS_DOM3_NODE_PROPS
			{"data",						JS_DOM3_NODE_LAST_PROP+1,       JSPROP_ENUMERATE | JSPROP_PERMANENT},
			{"length",						JS_DOM3_NODE_LAST_PROP+2,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*text*/
			{"isElementContentWhitespace",	JS_DOM3_NODE_LAST_PROP+3,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"wholeText",					JS_DOM3_NODE_LAST_PROP+4,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		JS_InitClass(c, global, 0, &dom_rt->domTextClass, 0, 0, textProps, textFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] text class initialized\n"));
	}
	
	{
		JSFunctionSpec eventFuncs[] = {
			{"stopPropagation", event_stop_propagation, 0},
			{"preventDefault", event_prevent_default, 0},
			{"isDefaultPrevented", event_is_default_prevented, 0},
			/*TODO - all event initXXX methods*/
			{0},
		};
		JSPropertySpec eventProps[] = {
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
			{"keyChar",			15,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"charCode",		15,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*wheelEvent*/
			{"wheelDelta",		16,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*progress*/
			{"lengthComputable",17,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"typeArg",			18,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"loaded",			19,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"total",			20,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			/*zoom*/
			{"zoomRectScreen",	21,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"previousScale",	22,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"previousTranslate",	23,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"newScale",		24,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{"newTranslate",	25,       JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY},
			{0}
		};
		JS_InitClass(c, global, 0, &dom_rt->domEventClass, 0, 0, eventProps, eventFuncs, 0, 0);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_SCRIPT, ("[DOMCore] event class initialized\n"));
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
	JSObject *obj;
	if (!doc || !doc->RootNode) return;

	if (doc->reference_count)
		doc->reference_count++;

	obj = JS_DefineObject(c, global, name, &dom_rt->domDocumentClass, 0, 0 );
	gf_node_register(doc->RootNode, NULL);
	JS_SetPrivate(c, obj, doc);
	doc->document = obj;
}

void dom_js_define_document(JSContext *c, JSObject *global, GF_SceneGraph *doc)
{
	dom_js_define_document_ex(c, global, doc, "document");
}
void dom_js_define_svg_document(JSContext *c, JSObject *global, GF_SceneGraph *doc)
{
	dom_js_define_document_ex(c, global, doc, "SVGDocument");
}

JSObject *dom_js_define_event(JSContext *c, JSObject *global)
{
	return JS_DefineObject(c, global, "evt", &dom_rt->domEventClass, 0, 0 );
}

#endif

