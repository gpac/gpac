/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2021
 *			All rights reserved
 *
 *  This file is part of GPAC / JavaScript XmlHttpRequest bindings
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

/*
	ANY CHANGE TO THE API MUST BE REFLECTED IN THE DOCUMENTATION IN gpac/share/doc/idl/xhr.idl
	(no way to define inline JS doc with doxygen)
*/

#include <gpac/setup.h>

#ifdef GPAC_HAS_QJS

/*base SVG type*/
#include <gpac/nodes_svg.h>
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>
/*dom events*/
#include <gpac/events.h>

#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/options.h>
#include <gpac/xml.h>


#include <gpac/internal/scenegraph_dev.h>

#include "../quickjs/quickjs.h"
#include "../scenegraph/qjs_common.h"

typedef struct __xhr_context XMLHTTPContext;

/************************************************************
 *
 *	xmlHttpRequest implementation
 *
 *************************************************************/
typedef enum {
	XHR_ONABORT,
	XHR_ONERROR,
	XHR_ONLOAD,
	XHR_ONLOADEND,
	XHR_ONLOADSTART,
	XHR_ONPROGRESS,
	XHR_ONREADYSTATECHANGE,
	XHR_ONTIMEOUT,
	XHR_READYSTATE,
	XHR_RESPONSE,
	XHR_RESPONSETYPE,
	XHR_RESPONSETEXT,
	XHR_RESPONSEXML,
	XHR_STATUS,
	XHR_STATUSTEXT,
	XHR_TIMEOUT,
	XHR_UPLOAD,
	XHR_WITHCREDENTIALS,
	XHR_STATIC_UNSENT,
	XHR_STATIC_OPENED,
	XHR_STATIC_HEADERS_RECEIVED,
	XHR_STATIC_LOADING,
	XHR_STATIC_DONE,
	XHR_CACHE,
} XHR_JSProperty;

typedef enum {
	XHR_READYSTATE_UNSENT			= 0,
	XHR_READYSTATE_OPENED			= 1,
	XHR_READYSTATE_HEADERS_RECEIVED = 2,
	XHR_READYSTATE_LOADING			= 3,
	XHR_READYSTATE_DONE				= 4
} XHR_ReadyState;

typedef enum {
	XHR_RESPONSETYPE_NONE,
	XHR_RESPONSETYPE_ARRAYBUFFER,
	XHR_RESPONSETYPE_BLOB,
	XHR_RESPONSETYPE_DOCUMENT,
	XHR_RESPONSETYPE_JSON,
	XHR_RESPONSETYPE_TEXT,
	XHR_RESPONSETYPE_SAX,
	XHR_RESPONSETYPE_PUSH,
} XHR_ResponseType;

typedef enum {
	XHR_CACHETYPE_NORMAL,
	XHR_CACHETYPE_NONE,
	XHR_CACHETYPE_MEMORY,
} XHR_CacheType;

struct __xhr_context
{
	JSContext *c;
	JSValue _this;

	/* callback functions */
	JSValue onabort;
	JSValue onerror;
	JSValue onreadystatechange;
	JSValue onload;
	JSValue onloadstart;
	JSValue onloadend;
	JSValue onprogress;
	JSValue ontimeout;

	XHR_ReadyState readyState;
	Bool async;

	/* GPAC extension to control the caching of XHR-downloaded resources */
	XHR_CacheType  cache;

	/*header/header-val, terminated by NULL*/
	char **headers;
	u32 cur_header;
	char **recv_headers;

	char *method, *url;
	GF_DownloadSession *sess;
	char *data;
	u32 size;
	JSValue arraybuffer;
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

	GF_DOMEventTarget *event_target;

	/* dom graph in which the XHR is created */
	GF_SceneGraph *owning_graph;
	Bool local_graph;

#ifndef GPAC_DISABLE_SVG
	/* dom graph used to parse XML into */
	GF_SceneGraph *document;
#endif
	Bool js_dom_loaded;
};

GF_JSClass xhrClass;

#if 0 //unused
GF_SceneGraph *xml_http_get_scenegraph(XMLHTTPContext *ctx)
{
	return ctx->owning_graph;
}
#endif

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
		}
	}
	xml_http_append_recv_header(ctx, hdr, val);
}

static void xml_http_del_data(XMLHTTPContext *ctx)
{
	if (!JS_IsUndefined(ctx->arraybuffer)) {
		JS_FreeValue(ctx->c, ctx->arraybuffer);
		ctx->arraybuffer = JS_UNDEFINED;
	}
	//only free data if no arraybuffer was used to fetch it
	else if (ctx->data) {
		gf_free(ctx->data);
	}
	ctx->data = NULL;
	ctx->size = 0;
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
	if (ctx->method) {
		gf_free(ctx->method);
		ctx->method = NULL;
	}
	if (ctx->url) {
		gf_free(ctx->url);
		ctx->url = NULL;
	}

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
#ifndef GPAC_DISABLE_SVG
	if (ctx->document) {
		if (ctx->js_dom_loaded) {
			dom_js_unload();
			ctx->js_dom_loaded = GF_FALSE;
		}
		gf_node_unregister(ctx->document->RootNode, NULL);

		/*we're sure the graph is a "nomade" one since we initially put the refcount to 1 ourselves*/
		ctx->document->reference_count--;
		if (!ctx->document->reference_count) {
			gf_sg_js_dom_pre_destroy(JS_GetRuntime(ctx->c), ctx->document, NULL);
			gf_sg_del(ctx->document);
		}
	}
	ctx->document = NULL;
#endif
	ctx->size = 0;
	ctx->async = GF_FALSE;
	ctx->readyState = XHR_READYSTATE_UNSENT;
	ctx->ret_code = GF_OK;
}

static void xml_http_finalize(JSRuntime *rt, JSValue obj)
{
	XMLHTTPContext *ctx = (XMLHTTPContext *) JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx) return;
	JS_FreeValueRT(rt, ctx->onabort);
	JS_FreeValueRT(rt, ctx->onerror);
	JS_FreeValueRT(rt, ctx->onload);
	JS_FreeValueRT(rt, ctx->onloadend);
	JS_FreeValueRT(rt, ctx->onloadstart);
	JS_FreeValueRT(rt, ctx->onprogress);
	JS_FreeValueRT(rt, ctx->onreadystatechange);
	JS_FreeValueRT(rt, ctx->ontimeout);
	xml_http_reset(ctx);

#ifndef GPAC_DISABLE_SVG
	if (ctx->event_target) {
		if (ctx->local_graph) {
			while (gf_list_count(ctx->event_target->listeners)) {
				GF_Node *listener = gf_list_get(ctx->event_target->listeners, 0);
				gf_dom_listener_del(listener, ctx->event_target);
			}
		}
		gf_dom_event_target_del(ctx->event_target);
	}
#endif

	if (ctx->local_graph) {
		dom_js_unload();
		gf_sg_del(ctx->owning_graph);
	}

	gf_free(ctx);
}

static GFINLINE GF_SceneGraph *xml_get_scenegraph(JSContext *c)
{
	GF_SceneGraph *scene;
	JSValue global = JS_GetGlobalObject(c);
	scene = (GF_SceneGraph *) JS_GetOpaque_Nocheck(global);
	JS_FreeValue(c, global);
	return scene;
}

#ifndef GPAC_DISABLE_SVG
void xhr_get_event_target(JSContext *c, JSValue obj, GF_SceneGraph **sg, GF_DOMEventTarget **target)
{
	if (c) {
		/*XHR interface*/
		XMLHTTPContext *ctx = JS_GetOpaque(obj, xhrClass.class_id);
		if (!ctx) return;

		if (ctx->local_graph)
			*sg = ctx->owning_graph;
		else
			*sg = xml_get_scenegraph(c);
		*target = ctx->event_target;
	}
}
#endif

static JSValue xml_http_constructor(JSContext *c, JSValueConst new_target, int argc, JSValueConst *argv)
{
	XMLHTTPContext *p;
	JSValue obj;

	GF_SAFEALLOC(p, XMLHTTPContext);
	if (!p) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[WHR] Failed to allocate XHR object\n"));
		return GF_JS_EXCEPTION(c);
	}
	obj = JS_NewObjectClass(c, xhrClass.class_id);
	p->c = c;
	p->_this = obj;
	p->owning_graph = xml_get_scenegraph(c);
	if (!p->owning_graph) {
		p->local_graph = GF_TRUE;
		p->owning_graph = gf_sg_new();
		dom_js_load(p->owning_graph, c);
	}

#ifndef GPAC_DISABLE_SVG
	if (p->owning_graph)
		p->event_target = gf_dom_event_target_new(GF_DOM_EVENT_TARGET_XHR, p);
#endif

	p->onabort = JS_NULL;
	p->onerror = JS_NULL;
	p->onreadystatechange = JS_NULL;
	p->onload = JS_NULL;
	p->onloadstart = JS_NULL;
	p->onloadend = JS_NULL;
	p->onprogress = JS_NULL;
	p->ontimeout = JS_NULL;

	JS_SetOpaque(obj, p);
	return obj;
}

static void xml_http_fire_event(XMLHTTPContext *ctx, GF_EventType evtType)
{
#ifndef GPAC_DISABLE_SVG
	GF_DOM_Event xhr_evt;
	if (!ctx->event_target)
		return;

	memset(&xhr_evt, 0, sizeof(GF_DOM_Event));
	xhr_evt.type = evtType;
	xhr_evt.target = ctx->event_target->ptr;
	xhr_evt.target_type = ctx->event_target->ptr_type;
	gf_sg_fire_dom_event(ctx->event_target, &xhr_evt, ctx->owning_graph, NULL);
#endif
}


static void xml_http_state_change(XMLHTTPContext *ctx)
{
#ifndef GPAC_DISABLE_VRML
	GF_SceneGraph *scene;
	GF_Node *n;
#endif

	gf_js_lock(ctx->c, GF_TRUE);
	if (! JS_IsNull(ctx->onreadystatechange)) {
		JSValue ret = JS_Call(ctx->c, ctx->onreadystatechange, ctx->_this, 0, NULL);
		if (JS_IsException(ret))
			js_dump_error(ctx->c);
		JS_FreeValue(ctx->c, ret);
	}

	js_std_loop(ctx->c);
	gf_js_lock(ctx->c, GF_FALSE);

	if (! ctx->owning_graph) return;
	if (ctx->local_graph) return;

	/*Flush BIFS eventOut events*/
#ifndef GPAC_DISABLE_VRML
	scene = (GF_SceneGraph *)JS_GetContextOpaque(ctx->c);
	/*this is a scene, we look for a node (if scene is used, this is DOM-based scripting not VRML*/
	if (scene->__reserved_null == 0) return;
	n = (GF_Node *)JS_GetContextOpaque(ctx->c);
	gf_js_vrml_flush_event_out(n, (GF_ScriptPriv *)n->sgprivate->UserPrivate);
#endif
}


static JSValue xml_http_open(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *val;
	GF_JSAPIParam par;
	XMLHTTPContext *ctx;
	GF_SceneGraph *scene;

	ctx = (XMLHTTPContext *) JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx) return GF_JS_EXCEPTION(c);

	/*reset*/
	if (ctx->readyState) xml_http_reset(ctx);

	if (argc<2) return GF_JS_EXCEPTION(c);
	/*method is a string*/
	if (!JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);
	/*url is a string*/
	if (!JS_CHECK_STRING(argv[1])) return GF_JS_EXCEPTION(c);

	xml_http_reset(ctx);
	val = JS_ToCString(c, argv[0]);
	if (strcmp(val, "GET") && strcmp(val, "POST") && strcmp(val, "HEAD")
	        && strcmp(val, "PUT") && strcmp(val, "DELETE") && strcmp(val, "OPTIONS") ) {
		JS_FreeCString(c, val);
		return GF_JS_EXCEPTION(c);
	}

	ctx->method = gf_strdup(val);
	JS_FreeCString(c, val);

	/*concatenate URL*/
	scene = xml_get_scenegraph(c);
#ifndef GPAC_DISABLE_VRML
	while (scene && scene->pOwningProto && scene->parent_scene) scene = scene->parent_scene;
#endif

	par.uri.nb_params = 0;
	val = JS_ToCString(c, argv[1]);
	par.uri.url = (char *) val;
	ctx->url = NULL;
	if (scene && scene->script_action) {
		scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_RESOLVE_URI, scene->RootNode, &par);
		ctx->url = par.uri.url;
	} else {
		const char *par_url = jsf_get_script_filename(c);
		ctx->url = gf_url_concatenate(par_url, val);
		if (!ctx->url)
			ctx->url = gf_strdup(val);
	}
	JS_FreeCString(c, val);

	/*async defaults to true*/
	ctx->async = GF_TRUE;
	if (argc>2) {
		val = NULL;
		ctx->async = JS_ToBool(c, argv[2]) ? GF_TRUE : GF_FALSE;
		if (argc>3) {
			if (!JS_CHECK_STRING(argv[3])) return GF_JS_EXCEPTION(c);
			/*TODO*/
			if (argc>4) {
				if (!JS_CHECK_STRING(argv[4])) return GF_JS_EXCEPTION(c);
				val = JS_ToCString(c, argv[4]);
				/*TODO*/
			} else {
				val = JS_ToCString(c, argv[3]);
			}
		}
		JS_FreeCString(c, val);
	}
	/*OPEN success*/
	ctx->readyState = XHR_READYSTATE_OPENED;
	xml_http_state_change(ctx);
	xml_http_fire_event(ctx, GF_EVENT_MEDIA_LOAD_START);
	if (JS_IsFunction(c, ctx->onloadstart) ) {
		return JS_Call(ctx->c, ctx->onloadstart, ctx->_this, 0, NULL);
	}
	return JS_TRUE;
}

static JSValue xml_http_set_header(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *hdr, *val;
	XMLHTTPContext *ctx = (XMLHTTPContext *) JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx) return GF_JS_EXCEPTION(c);

	if (ctx->readyState!=XHR_READYSTATE_OPENED) return GF_JS_EXCEPTION(c);
	if (argc!=2) return GF_JS_EXCEPTION(c);
	if (!JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);
	if (!JS_CHECK_STRING(argv[1])) return GF_JS_EXCEPTION(c);

	hdr = JS_ToCString(c, argv[0]);
	val = JS_ToCString(c, argv[1]);
	xml_http_append_send_header(ctx, (char *)hdr, (char *)val);
	JS_FreeCString(c, hdr);
	JS_FreeCString(c, val);
	return JS_TRUE;
}

#ifndef GPAC_DISABLE_SVG

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
			if (!att) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[XHR] Fail to allocate DOM attribute\n"));
				continue;
			}
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
#endif // GPAC_DISABLE_SVG

static void xml_http_terminate(XMLHTTPContext *ctx, GF_Err error)
{
	/*if we get here, destroy downloader*/
	if (ctx->sess) {
		gf_dm_sess_del(ctx->sess);
		ctx->sess = NULL;
	}

	/*but stay in loaded mode*/
	ctx->readyState = XHR_READYSTATE_DONE;
	xml_http_state_change(ctx);
	xml_http_fire_event(ctx, GF_EVENT_LOAD);
	xml_http_fire_event(ctx, GF_EVENT_MEDIA_LOAD_DONE);
	if (JS_IsFunction(ctx->c, ctx->onload)) {
		JSValue rval = JS_Call(ctx->c, ctx->onload, ctx->_this, 0, NULL);
		if (JS_IsException(rval)) js_dump_error(ctx->c);
		JS_FreeValue(ctx->c, rval);
	}
	if (JS_IsFunction(ctx->c, ctx->onloadend)) {
		JSValue rval = JS_Call(ctx->c, ctx->onloadend, ctx->_this, 0, NULL);
		if (JS_IsException(rval)) js_dump_error(ctx->c);
		JS_FreeValue(ctx->c, rval);
	}
}

static void xml_http_on_data(void *usr_cbk, GF_NETIO_Parameter *parameter)
{
	Bool locked = GF_FALSE;
	XMLHTTPContext *ctx = (XMLHTTPContext *)usr_cbk;

	/*make sure we can grab JS and the session is not destroyed*/
	while (ctx->sess) {
		if (gf_js_try_lock(ctx->c) ) {
			locked = GF_TRUE;
			break;
		}
		gf_sleep(1);
	}
	/*if session not set, we've had an abort*/
	if (!ctx->sess && !ctx->isFile) {
		if (locked)
			gf_js_lock(ctx->c, GF_FALSE);
		return;
	}

	if (!ctx->isFile) {
		assert( locked );
		gf_js_lock(ctx->c, GF_FALSE);
		locked = GF_FALSE;
	}
	switch (parameter->msg_type) {
	case GF_NETIO_SETUP:
		/*nothing to do*/
		goto exit;
	case GF_NETIO_CONNECTED:
		/*nothing to do*/
		goto exit;
	case GF_NETIO_WAIT_FOR_REPLY:
		/*reset send() state (data, current header) and prepare recv headers*/
		xml_http_reset_partial(ctx);
		ctx->readyState = XHR_READYSTATE_HEADERS_RECEIVED;
		xml_http_state_change(ctx);
		xml_http_fire_event(ctx, GF_EVENT_MEDIA_PROGRESS);
		if (JS_IsFunction(ctx->c, ctx->onprogress) ) {
			JSValue rval = JS_Call(ctx->c, ctx->onprogress, ctx->_this, 0, NULL);
			if (JS_IsException(rval)) js_dump_error(ctx->c);
			JS_FreeValue(ctx->c, rval);
		}
		goto exit;
	/*this is signaled sent AFTER headers*/
	case GF_NETIO_PARSE_REPLY:
		ctx->html_status = parameter->reply;
		if (parameter->value) {
			ctx->statusText = gf_strdup(parameter->value);
		}
		ctx->readyState = XHR_READYSTATE_LOADING;
		xml_http_state_change(ctx);
		ctx->readyState = XHR_READYSTATE_HEADERS_RECEIVED;
		xml_http_state_change(ctx);
		xml_http_fire_event(ctx, GF_EVENT_MEDIA_PROGRESS);
		if (JS_IsFunction(ctx->c, ctx->onprogress) ) {
			JSValue rval = JS_Call(ctx->c, ctx->onprogress, ctx->_this, 0, NULL);
			if (JS_IsException(rval)) js_dump_error(ctx->c);
			JS_FreeValue(ctx->c, rval);
		}
		goto exit;

	case GF_NETIO_GET_METHOD:
		parameter->name = ctx->method;
		goto exit;
	case GF_NETIO_GET_HEADER:
		if (ctx->headers && ctx->headers[2*ctx->cur_header]) {
			parameter->name = ctx->headers[2*ctx->cur_header];
			parameter->value = ctx->headers[2*ctx->cur_header+1];
			ctx->cur_header++;
		}
		goto exit;
	case GF_NETIO_GET_CONTENT:
		if (ctx->data) {
			parameter->data = ctx->data;
			parameter->size = ctx->size;
		}
		goto exit;
	case GF_NETIO_PARSE_HEADER:
		xml_http_append_recv_header(ctx, parameter->name, parameter->value);
		/*prepare SAX parser*/
		if (ctx->responseType != XHR_RESPONSETYPE_SAX) goto exit;
		if (strcmp(parameter->name, "Content-Type")) goto exit;

#ifndef GPAC_DISABLE_SVG
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
		}
#endif

		goto exit;
	case GF_NETIO_DATA_EXCHANGE:
		if (parameter->data && parameter->size) {
			if (ctx->sax) {
				GF_Err e;
				if (!ctx->size) e = gf_xml_sax_init(ctx->sax, (unsigned char *) parameter->data);
				else e = gf_xml_sax_parse(ctx->sax, parameter->data);
				if (e) {
					gf_xml_sax_del(ctx->sax);
					ctx->sax = NULL;
				}
				goto exit;
			}

			/*detach arraybuffer if any*/
			if (!JS_IsUndefined(ctx->arraybuffer)) {
				JS_FreeValue(ctx->c, ctx->arraybuffer);
				ctx->arraybuffer = JS_UNDEFINED;
			}

			if (ctx->responseType!=XHR_RESPONSETYPE_PUSH) {
				ctx->data = (char *)gf_realloc(ctx->data, sizeof(char)*(ctx->size+parameter->size+1));
				memcpy(ctx->data + ctx->size, parameter->data, sizeof(char)*parameter->size);
				ctx->size += parameter->size;
				ctx->data[ctx->size] = 0;
			}

			if (JS_IsFunction(ctx->c, ctx->onprogress) ) {
				JSValue prog_evt = JS_NewObject(ctx->c);
				JSValue buffer_ab=JS_UNDEFINED;
				u32 bps=0;
				u64 tot_size=0;
				gf_dm_sess_get_stats(ctx->sess, NULL, NULL, &tot_size, NULL, &bps, NULL);
				JS_SetPropertyStr(ctx->c, prog_evt, "lengthComputable", JS_NewBool(ctx->c, tot_size ? 1 : 0));
				JS_SetPropertyStr(ctx->c, prog_evt, "loaded", JS_NewInt64(ctx->c, ctx->size));
				JS_SetPropertyStr(ctx->c, prog_evt, "total", JS_NewInt64(ctx->c, tot_size));
				JS_SetPropertyStr(ctx->c, prog_evt, "bps", JS_NewInt64(ctx->c, bps*8));
				if (ctx->responseType==XHR_RESPONSETYPE_PUSH) {
					buffer_ab = JS_NewArrayBuffer(ctx->c, (u8 *) parameter->data, parameter->size, NULL, ctx, 0/*1*/);
					JS_SetPropertyStr(ctx->c, prog_evt, "buffer", buffer_ab);
				}

				JSValue rval = JS_Call(ctx->c, ctx->onprogress, ctx->_this, 1, &prog_evt);
				if (JS_IsException(rval)) js_dump_error(ctx->c);
				JS_FreeValue(ctx->c, rval);
				if (ctx->responseType==XHR_RESPONSETYPE_PUSH) {
					JS_DetachArrayBuffer(ctx->c, buffer_ab);
				}
				JS_FreeValue(ctx->c, prog_evt);
			}
		}
		goto exit;
	case GF_NETIO_DATA_TRANSFERED:
		/* No return, go till the end of the function */
		break;
	case GF_NETIO_DISCONNECTED:
		goto exit;
	case GF_NETIO_STATE_ERROR:
		ctx->ret_code = parameter->error;
		/* No return, go till the end of the function */
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[XmlHttpRequest] Download error: %s\n", gf_error_to_string(parameter->error)));
		break;
	case GF_NETIO_REQUEST_SESSION:
	case GF_NETIO_CANCEL_STREAM:
		parameter->error = GF_NOT_SUPPORTED;
		return;
	}
	if (ctx->async) {
		xml_http_terminate(ctx, parameter->error);
	}

exit:
	if (locked) {
		gf_js_lock(ctx->c, GF_FALSE);
	}
}

static GF_Err xml_http_process_local(XMLHTTPContext *ctx)
{
	/* For XML Http Requests to files, we fake the processing by calling the HTTP callbacks */
	GF_NETIO_Parameter par;
	u64 fsize;
	char contentLengthHeader[256];
	FILE *responseFile;

	/*opera-style local host*/
	if (!strnicmp(ctx->url, "file://localhost", 16)) responseFile = gf_fopen(ctx->url+16, "rb");
	/*regular-style local host*/
	else if (!strnicmp(ctx->url, "file://", 7)) responseFile = gf_fopen(ctx->url+7, "rb");
	/* other types: e.g. "C:\" */
	else responseFile = gf_fopen(ctx->url, "rb");

	if (!responseFile) {
		ctx->html_status = 404;
		GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[XmlHttpRequest] cannot open local file %s\n", ctx->url));
		xml_http_fire_event(ctx, GF_EVENT_ERROR);
		if (JS_IsFunction(ctx->c, ctx->onerror) ) {
			JSValue rval = JS_Call(ctx->c, ctx->onerror, ctx->_this, 0, NULL);
			if (JS_IsException(rval)) js_dump_error(ctx->c);
			JS_FreeValue(ctx->c, rval);
		}
		return GF_BAD_PARAM;
	}
	ctx->isFile = GF_TRUE;

	par.msg_type = GF_NETIO_WAIT_FOR_REPLY;
	xml_http_on_data(ctx, &par);

	fsize = gf_fsize(responseFile);

	ctx->html_status = 200;

	ctx->data = (char *)gf_malloc(sizeof(char)*(size_t)(fsize+1));
	fsize = gf_fread(ctx->data, (size_t)fsize, responseFile);
	gf_fclose(responseFile);
	ctx->data[fsize] = 0;
	ctx->size = (u32)fsize;

	memset(&par, 0, sizeof(GF_NETIO_Parameter));
	par.msg_type = GF_NETIO_PARSE_HEADER;
	par.name = "Content-Type";
	if (ctx->responseType == XHR_RESPONSETYPE_SAX) {
		par.value = "application/xml";
	} else if (ctx->responseType == XHR_RESPONSETYPE_DOCUMENT) {
		par.value = "application/xml";
	} else {
		par.value = "application/octet-stream";
	}
	xml_http_on_data(ctx, &par);

	memset(&par, 0, sizeof(GF_NETIO_Parameter));
	par.msg_type = GF_NETIO_PARSE_HEADER;
	par.name = "Content-Length";
	sprintf(contentLengthHeader, "%d", ctx->size);
	par.value = contentLengthHeader;
	xml_http_on_data(ctx, &par);


	if (ctx->sax) {
		memset(&par, 0, sizeof(GF_NETIO_Parameter));
		par.msg_type = GF_NETIO_DATA_EXCHANGE;
		par.data = ctx->data;
		par.size = ctx->size;
		ctx->size = 0;
		xml_http_on_data(ctx, &par);
		ctx->size = par.size;
	}

	memset(&par, 0, sizeof(GF_NETIO_Parameter));
	par.msg_type = GF_NETIO_DATA_TRANSFERED;
	xml_http_on_data(ctx, &par);

	if (!ctx->async) {
		xml_http_terminate(ctx, GF_OK);
	}
	return GF_OK;
}

static JSValue xml_http_send(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_Err e;
	GF_JSAPIParam par;
	GF_SceneGraph *scene;
	const char *data = NULL;
	u32 data_size = 0;
	XMLHTTPContext *ctx = (XMLHTTPContext *) JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx) return GF_JS_EXCEPTION(c);

	if (ctx->readyState!=XHR_READYSTATE_OPENED) return GF_JS_EXCEPTION(c);
	if (ctx->sess) return GF_JS_EXCEPTION(c);

	scene = xml_get_scenegraph(c);
	if (scene) {
		par.dnld_man = NULL;
		if (scene->script_action)
			scene->script_action(scene->script_action_cbck, GF_JSAPI_OP_GET_DOWNLOAD_MANAGER, NULL, &par);
	} else {
		par.dnld_man = jsf_get_download_manager(c);
	}
	if (!par.dnld_man) return GF_JS_EXCEPTION(c);

	if (argc) {
		if (JS_IsNull(argv[0])) {
		} else if (JS_IsArrayBuffer(c, argv[0])) {
			size_t asize;
			data = JS_GetArrayBuffer(c, &asize, argv[0] );
			if (data) data_size = (u32) asize;
		} else if (JS_IsObject(argv[0])) {
			/*NOT SUPPORTED YET, we must serialize the sg*/
			return GF_JS_EXCEPTION(c);
		} else {
			if (!JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);
			data = JS_ToCString(c, argv[0]);
			data_size = (u32) strlen(data);
		}
	}

	/*reset previous text*/
	xml_http_del_data(ctx);
	ctx->data = data ? gf_strdup(data) : NULL;
	ctx->size = data_size;

	JS_FreeCString(c, data);

	if (!strncmp(ctx->url, "http://", 7)) {
		u32 flags = GF_NETIO_SESSION_NOTIFY_DATA;
		if (!ctx->async)
			flags |= GF_NETIO_SESSION_NOT_THREADED;

		if (ctx->cache != XHR_CACHETYPE_NORMAL) {
			if (ctx->cache == XHR_CACHETYPE_NONE) {
				flags |= GF_NETIO_SESSION_NOT_CACHED;
			}
			if (ctx->cache == XHR_CACHETYPE_MEMORY) {
				flags |= GF_NETIO_SESSION_MEMORY_CACHE;
			}
		}
		ctx->sess = gf_dm_sess_new(par.dnld_man, ctx->url, flags, xml_http_on_data, ctx, &e);
		if (!ctx->sess) return GF_JS_EXCEPTION(c);

		/*start our download (whether the session is threaded or not)*/
		e = gf_dm_sess_process(ctx->sess);
		if (e!=GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[XmlHttpRequest] Error processing %s: %s\n", ctx->url, gf_error_to_string(e) ));
		}
		if (!ctx->async) {
			xml_http_terminate(ctx, e);
		}
	} else {
		e = xml_http_process_local(ctx);
		if (e!=GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_SCRIPT, ("[XmlHttpRequest] Error processing %s: %s\n", ctx->url, gf_error_to_string(e) ));
		}
	}

	return JS_TRUE;
}

static JSValue xml_http_abort(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	GF_DownloadSession *sess;
	XMLHTTPContext *ctx = JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx) return GF_JS_EXCEPTION(c);

	sess = ctx->sess;
	ctx->sess = NULL;
	if (sess) {
		//abort first, so that on HTTP/2 this results in RST_STREAM
		gf_dm_sess_abort(sess);
		gf_dm_sess_del(sess);
	}

	xml_http_fire_event(ctx, GF_EVENT_ABORT);
	xml_http_reset(ctx);
	if (JS_IsFunction(c, ctx->onabort)) {
		return JS_Call(ctx->c, ctx->onabort, ctx->_this, 0, NULL);
	}
	return JS_TRUE;
}

static JSValue xml_http_get_all_headers(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 nb_hdr;
	char *szVal = NULL;
	JSValue res;
	XMLHTTPContext *ctx = JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx) return GF_JS_EXCEPTION(c);

	/*must be received or loaded*/
	if (ctx->readyState<XHR_READYSTATE_LOADING) return GF_JS_EXCEPTION(c);
	nb_hdr = 0;
	if (ctx->recv_headers) {
		while (ctx->recv_headers[nb_hdr]) {
			if (szVal) gf_dynstrcat(&szVal, "\r\n", NULL);
			gf_dynstrcat(&szVal, ctx->recv_headers[nb_hdr], NULL);
			gf_dynstrcat(&szVal, ctx->recv_headers[nb_hdr+1], ": ");
			nb_hdr+=2;
		}
	}

	if (!szVal) {
		return JS_NULL;
	}
	res = JS_NewString(c, szVal);
	gf_free(szVal);
	return res;
}

static JSValue xml_http_get_header(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	u32 nb_hdr;
	const char *hdr;
	char *szVal = NULL;
	XMLHTTPContext *ctx = JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx) return GF_JS_EXCEPTION(c);

	if (!JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);
	/*must be received or loaded*/
	if (ctx->readyState<XHR_READYSTATE_LOADING) return GF_JS_EXCEPTION(c);
	hdr = JS_ToCString(c, argv[0]);

	nb_hdr = 0;
	if (ctx->recv_headers) {
		while (ctx->recv_headers[nb_hdr]) {
			if (!strcmp(ctx->recv_headers[nb_hdr], hdr)) {
				gf_dynstrcat(&szVal, ctx->recv_headers[nb_hdr+1], ", ");
			}
			nb_hdr+=2;
		}
	}
	JS_FreeCString(c, hdr);
	if (!szVal) {
		return JS_NULL;
	}
	JSValue res = JS_NewString(c, szVal);
	gf_free(szVal);
	return res;
}

#ifndef GPAC_DISABLE_SVG
static GF_Err xml_http_load_dom(XMLHTTPContext *ctx)
{
	GF_Err e;
	GF_DOMParser *parser = gf_xml_dom_new();
	e = gf_xml_dom_parse_string(parser, ctx->data);
	if (!e) {
		e = gf_sg_init_from_xml_node(ctx->document, gf_xml_dom_get_root(parser));
	}
	gf_xml_dom_del(parser);
	return e;
}
#endif //GPAC_DISABLE_SVG


static JSValue xml_http_overrideMimeType(JSContext *c, JSValueConst obj, int argc, JSValueConst *argv)
{
	const char *mime;
	XMLHTTPContext *ctx = JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx || !argc) return GF_JS_EXCEPTION(c);

	if (!JS_CHECK_STRING(argv[0])) return GF_JS_EXCEPTION(c);
	mime = JS_ToCString(c, argv[0]);
	if (ctx->mime) gf_free(ctx->mime);
	ctx->mime = gf_strdup(mime);
	JS_FreeCString(c, mime);
	return JS_TRUE;
}

static void xml_http_array_buffer_free(JSRuntime *rt, void *opaque, void *ptr)
{
	//might already been destroyed !
//	XMLHTTPContext *ctx = (XMLHTTPContext *)opaque;
	gf_free(ptr);
}

static JSValue xml_http_getProperty(JSContext *c, JSValueConst obj, int magic)
{
	XMLHTTPContext *ctx = JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx) return GF_JS_EXCEPTION(c);

	switch (magic) {
	case XHR_ONABORT: return JS_DupValue(c, ctx->onabort);
	case XHR_ONERROR: return JS_DupValue(c, ctx->onerror);
	case XHR_ONLOAD: return JS_DupValue(c, ctx->onload);
	case XHR_ONLOADSTART: return JS_DupValue(c, ctx->onloadstart);
	case XHR_ONLOADEND: return JS_DupValue(c, ctx->onloadend);
	case XHR_ONPROGRESS: return JS_DupValue(c, ctx->onprogress);
	case XHR_ONREADYSTATECHANGE: return JS_DupValue(c, ctx->onreadystatechange);
	case XHR_ONTIMEOUT: return JS_DupValue(c, ctx->ontimeout);
	case XHR_READYSTATE: return JS_NewInt32(c, ctx->readyState);
	case XHR_RESPONSETEXT:
		if (ctx->readyState<XHR_READYSTATE_LOADING) return JS_NULL;
		if (ctx->data) {
			return JS_NewString(c, ctx->data);
		} else {
			return JS_NULL;
		}

	case XHR_RESPONSEXML:
		if (ctx->readyState<XHR_READYSTATE_LOADING) return JS_NULL;
#ifndef GPAC_DISABLE_SVG
		if (ctx->data) {
			if (!ctx->document) {
				ctx->document = gf_sg_new();
				/*mark this doc as "nomade", and let it leave until all references to it are destroyed*/
				ctx->document->reference_count = 1;
				xml_http_load_dom(ctx);
			}
			if (!ctx->js_dom_loaded) {
				dom_js_load(ctx->document, c);
				ctx->js_dom_loaded = GF_TRUE;
			}
			return dom_document_construct_external(c, ctx->document);
		} else {
			return JS_NULL;
		}
#else
		return js_throw_err_msg(c, GF_NOT_SUPPORTED, "DOM support not included in buil");
#endif

	case XHR_RESPONSE:
		if (ctx->readyState<XHR_READYSTATE_LOADING) return JS_NULL;

		if (ctx->data) {
			switch(ctx->responseType)
			{
			case XHR_RESPONSETYPE_NONE:
			case XHR_RESPONSETYPE_TEXT:
				return JS_NewString(c, ctx->data);

			case XHR_RESPONSETYPE_ARRAYBUFFER:
				if (JS_IsUndefined(ctx->arraybuffer)) {
					ctx->arraybuffer = JS_NewArrayBuffer(c, ctx->data, ctx->size, xml_http_array_buffer_free, ctx, 0/*1*/);
				}
				return JS_DupValue(c, ctx->arraybuffer);

			case XHR_RESPONSETYPE_DOCUMENT:
#ifndef GPAC_DISABLE_SVG
				if (ctx->data) {
					if (!ctx->document) {
						ctx->document = gf_sg_new();
						/*mark this doc as "nomade", and let it leave until all references to it are destroyed*/
						ctx->document->reference_count = 1;
						xml_http_load_dom(ctx);
					}
					if (!ctx->js_dom_loaded) {
						dom_js_load(ctx->document, c);
						ctx->js_dom_loaded = GF_TRUE;
					}
					return dom_document_construct_external(c, ctx->document);
				}
				return JS_NULL;
#else
				return js_throw_err_msg(c, GF_NOT_SUPPORTED, "DOM support not included in buil");
#endif
			case XHR_RESPONSETYPE_JSON:
				return JS_ParseJSON(c, ctx->data, ctx->size, "responseJSON");
			case XHR_RESPONSETYPE_PUSH:
				return GF_JS_EXCEPTION(c);
			default:
				/*other	types not supported	*/
				break;
			}
		}
		return JS_NULL;

	case XHR_STATUS:
		return JS_NewInt32(c, ctx->html_status);

	case XHR_STATUSTEXT:
		if (ctx->statusText) {
			return JS_NewString(c, ctx->statusText);
		}
		return JS_NULL;
	case XHR_TIMEOUT:
		return JS_NewInt32(c, ctx->timeout);
	case XHR_WITHCREDENTIALS:
		return ctx->withCredentials ? JS_TRUE : JS_FALSE;
	case XHR_UPLOAD:
		/* TODO */
		return GF_JS_EXCEPTION(c);
	case XHR_RESPONSETYPE:
		switch (ctx->responseType) {
		case XHR_RESPONSETYPE_NONE: return JS_NewString(c, "");
		case XHR_RESPONSETYPE_ARRAYBUFFER: return JS_NewString(c, "arraybuffer");
		case XHR_RESPONSETYPE_BLOB: return JS_NewString(c, "blob");
		case XHR_RESPONSETYPE_DOCUMENT: return JS_NewString(c, "document");
		case XHR_RESPONSETYPE_SAX: return JS_NewString(c, "sax");
		case XHR_RESPONSETYPE_JSON: return JS_NewString(c, "json");
		case XHR_RESPONSETYPE_TEXT: return JS_NewString(c, "text");
		case XHR_RESPONSETYPE_PUSH: return JS_NewString(c, "push");
		}
		return JS_NULL;
	case XHR_STATIC_UNSENT:
		return JS_NewInt32(c, XHR_READYSTATE_UNSENT);
	case XHR_STATIC_OPENED:
		return JS_NewInt32(c, XHR_READYSTATE_OPENED);
	case XHR_STATIC_HEADERS_RECEIVED:
		return JS_NewInt32(c, XHR_READYSTATE_HEADERS_RECEIVED);
	case XHR_STATIC_LOADING:
		return JS_NewInt32(c, XHR_READYSTATE_LOADING);
	case XHR_STATIC_DONE:
		return JS_NewInt32(c, XHR_READYSTATE_DONE);
	case XHR_CACHE:
		switch (ctx->cache) {
		case XHR_CACHETYPE_NORMAL: return JS_NewString(c, "normal");
		case XHR_CACHETYPE_NONE:  return JS_NewString(c, "none");
		case XHR_CACHETYPE_MEMORY: return JS_NewString(c, "memory");
		}
		return JS_NULL;
	default:
		break;
	}
	return JS_UNDEFINED;
}

static JSValue xml_http_setProperty(JSContext *c, JSValueConst obj, JSValueConst value, int magic)
{
	XMLHTTPContext *ctx = JS_GetOpaque(obj, xhrClass.class_id);
	if (!ctx) return GF_JS_EXCEPTION(c);

#define SET_CBK(_sym) \
		if (JS_IsFunction(c, value) || JS_IsUndefined(value) || JS_IsNull(value)) {\
			JS_FreeValue(c, ctx->_sym);\
			ctx->_sym = JS_DupValue(c, value);\
			return JS_TRUE;\
		}\
		return GF_JS_EXCEPTION(c);\

	switch (magic) {
	case XHR_ONERROR:
		SET_CBK(onerror)

	case XHR_ONABORT:
		SET_CBK(onabort)

	case XHR_ONLOAD:
		SET_CBK(onload)

	case XHR_ONLOADSTART:
		SET_CBK(onloadstart)

	case XHR_ONLOADEND:
		SET_CBK(onloadend)

	case XHR_ONPROGRESS:
		SET_CBK(onprogress)

	case XHR_ONREADYSTATECHANGE:
		SET_CBK(onreadystatechange)

	case XHR_ONTIMEOUT:
		SET_CBK(ontimeout)

	case XHR_TIMEOUT:
		if (JS_ToInt32(c, &ctx->timeout, value)) return GF_JS_EXCEPTION(c);
		return JS_TRUE;

	case XHR_WITHCREDENTIALS:
		ctx->withCredentials = JS_ToBool(c, value) ? GF_TRUE : GF_FALSE;
		return JS_TRUE;
	case XHR_RESPONSETYPE:
	{
		const char *str = JS_ToCString(c, value);
		if (!str || !strcmp(str, "")) {
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
		} else if (!strcmp(str, "sax")) {
			ctx->responseType = XHR_RESPONSETYPE_SAX;
		} else if (!strcmp(str, "push")) {
			ctx->responseType = XHR_RESPONSETYPE_PUSH;
		}
		JS_FreeCString(c, str);
		return JS_TRUE;
	}
	case XHR_CACHE:
	{
		const char *str = JS_ToCString(c, value);
		if (!str) return GF_JS_EXCEPTION(c);
		if (!strcmp(str, "normal")) {
			ctx->cache = XHR_CACHETYPE_NORMAL;
		} else if (!strcmp(str, "none")) {
			ctx->cache = XHR_CACHETYPE_NONE;
		} else if (!strcmp(str, "memory")) {
			ctx->cache = XHR_CACHETYPE_MEMORY;
		}
		JS_FreeCString(c, str);
		return JS_TRUE;
	}
	/*all other properties are read-only*/
	default:
		break;
	}
	return JS_FALSE;
}

static const JSCFunctionListEntry xhr_Funcs[] =
{
	JS_CGETSET_MAGIC_DEF("onabort", xml_http_getProperty, xml_http_setProperty, XHR_ONABORT),
	JS_CGETSET_MAGIC_DEF("onerror", xml_http_getProperty, xml_http_setProperty, XHR_ONERROR),
	JS_CGETSET_MAGIC_DEF("onload", xml_http_getProperty, xml_http_setProperty, XHR_ONLOAD),
	JS_CGETSET_MAGIC_DEF("onloadend", xml_http_getProperty, xml_http_setProperty, XHR_ONLOADEND),
	JS_CGETSET_MAGIC_DEF("onloadstart", xml_http_getProperty, xml_http_setProperty, XHR_ONLOADSTART),
	JS_CGETSET_MAGIC_DEF("onprogress", xml_http_getProperty, xml_http_setProperty, XHR_ONPROGRESS),
	JS_CGETSET_MAGIC_DEF("onreadystatechange", xml_http_getProperty, xml_http_setProperty, XHR_ONREADYSTATECHANGE),
	JS_CGETSET_MAGIC_DEF("ontimeout", xml_http_getProperty, xml_http_setProperty, XHR_ONTIMEOUT),
	JS_CGETSET_MAGIC_DEF("readyState", xml_http_getProperty, NULL, XHR_READYSTATE),
	JS_CGETSET_MAGIC_DEF("response", xml_http_getProperty, NULL, XHR_RESPONSE),
	JS_CGETSET_MAGIC_DEF("responseType", xml_http_getProperty, xml_http_setProperty, XHR_RESPONSETYPE),
	JS_CGETSET_MAGIC_DEF("responseText", xml_http_getProperty, NULL, XHR_RESPONSETEXT),
	JS_CGETSET_MAGIC_DEF("responseXML", xml_http_getProperty, NULL, XHR_RESPONSEXML),
	JS_CGETSET_MAGIC_DEF("status", xml_http_getProperty, NULL, XHR_STATUS),
	JS_CGETSET_MAGIC_DEF("statusText", xml_http_getProperty, NULL, XHR_STATUSTEXT),
	JS_CGETSET_MAGIC_DEF("timeout", xml_http_getProperty, xml_http_setProperty, XHR_TIMEOUT),
	JS_CGETSET_MAGIC_DEF("upload", xml_http_getProperty, NULL, XHR_UPLOAD),
	JS_CGETSET_MAGIC_DEF("withCredentials", xml_http_getProperty, xml_http_setProperty, XHR_WITHCREDENTIALS),
	JS_CGETSET_MAGIC_DEF("cache", xml_http_getProperty, xml_http_setProperty, XHR_CACHE),

	JS_CFUNC_DEF("abort", 0, xml_http_abort),
	JS_CFUNC_DEF("getAllResponseHeaders", 0, xml_http_get_all_headers),
	JS_CFUNC_DEF("getResponseHeader", 1, xml_http_get_header),
	JS_CFUNC_DEF("open", 2, xml_http_open),
	JS_CFUNC_DEF("overrideMimeType", 1, xml_http_overrideMimeType),
	JS_CFUNC_DEF("send", 0, xml_http_send),
	JS_CFUNC_DEF("setRequestHeader", 2, xml_http_set_header),

	/*eventTarget interface*/
	JS_DOM3_EVENT_TARGET_INTERFACE
};

static void xml_http_gc_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func)
{
    XMLHTTPContext *ctx = JS_GetOpaque(val, xhrClass.class_id);
    if (!ctx) return;

	JS_MarkValue(rt, ctx->onabort, mark_func);
	JS_MarkValue(rt, ctx->onerror, mark_func);
	JS_MarkValue(rt, ctx->onload, mark_func);
	JS_MarkValue(rt, ctx->onloadend, mark_func);
	JS_MarkValue(rt, ctx->onloadstart, mark_func);
	JS_MarkValue(rt, ctx->onprogress, mark_func);
	JS_MarkValue(rt, ctx->onreadystatechange, mark_func);
	JS_MarkValue(rt, ctx->ontimeout, mark_func);
}

static JSValue xhr_load_class(JSContext *c)
{
	if (! xhrClass.class_id) {
		JS_NewClassID(&xhrClass.class_id);
		xhrClass.class.class_name = "XMLHttpRequest";
		xhrClass.class.finalizer = xml_http_finalize;
		xhrClass.class.gc_mark = xml_http_gc_mark;
		JS_NewClass(JS_GetRuntime(c), xhrClass.class_id, &xhrClass.class);
	}
	JSValue proto = JS_NewObjectClass(c, xhrClass.class_id);
	JS_SetPropertyFunctionList(c, proto, xhr_Funcs, countof(xhr_Funcs));
	JS_SetClassProto(c, xhrClass.class_id, proto);
	JS_SetPropertyStr(c, proto, "UNSENT",	JS_NewInt32(c, XHR_STATIC_UNSENT));
	JS_SetPropertyStr(c, proto, "OPENED",	JS_NewInt32(c, XHR_STATIC_OPENED));
	JS_SetPropertyStr(c, proto, "HEADERS_RECEIVED",	JS_NewInt32(c, XHR_STATIC_HEADERS_RECEIVED));
	JS_SetPropertyStr(c, proto, "LOADING",	JS_NewInt32(c, XHR_STATIC_LOADING));
	JS_SetPropertyStr(c, proto, "DONE",	JS_NewInt32(c, XHR_STATIC_DONE));

	return JS_NewCFunction2(c, xml_http_constructor, "XMLHttpRequest", 1, JS_CFUNC_constructor, 0);
}

void qjs_module_init_xhr_global(JSContext *c, JSValue global)
{
	if (c) {
		JSValue ctor = xhr_load_class(c);
		JS_SetPropertyStr(c, global, "XMLHttpRequest", ctor);
	}
}

static int js_xhr_load_module(JSContext *c, JSModuleDef *m)
{
	JSValue global = JS_GetGlobalObject(c);
	JSValue ctor = xhr_load_class(c);
	JS_FreeValue(c, global);
    JS_SetModuleExport(c, m, "XMLHttpRequest", ctor);
	return 0;
}
void qjs_module_init_xhr(JSContext *ctx)
{
	JSModuleDef *m;
	m = JS_NewCModule(ctx, "xhr", js_xhr_load_module);
	if (!m) return;

	JS_AddModuleExport(ctx, m, "XMLHttpRequest");

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		qjs_module_init_xhr_global(NULL, JS_TRUE);
		xhr_get_event_target(NULL, JS_TRUE, NULL, NULL);
	}
#endif
	return;
}


#endif

