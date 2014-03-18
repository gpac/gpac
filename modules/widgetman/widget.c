//This software module was originally developed by TelecomParisTech in the
//course of the development of MPEG-U Widgets (ISO/IEC 23007-1) standard.
//
//This software module is an implementation of a part of one or 
//more MPEG-U Widgets (ISO/IEC 23007-1) tools as specified by the MPEG-U Widgets
//(ISO/IEC 23007-1) standard. ISO/IEC gives users of the MPEG-U Widgets
//(ISO/IEC 23007-1) free license to this software module or modifications
//thereof for use in hardware or software products claiming conformance to
//the MPEG-U Widgets (ISO/IEC 23007-1). Those intending to use this software
//module in hardware or software products are advised that its use may
//infringe existing patents.
//The original developer of this software module and his/her company, the
//subsequent editors and their companies, and ISO/IEC have no liability 
//for use of this software module or modifications thereof in an implementation. 
//Copyright is not released for non MPEG-U Widgets (ISO/IEC 23007-1) conforming 
//products. 
//Telecom ParisTech retains full right to use the code for his/her own purpose, 
//assign or donate the code to a third party and to inhibit third parties from 
//using the code for non MPEG-U Widgets (ISO/IEC 23007-1) conforming products. 
//
//This copyright notice must be included in all copies or derivative works.
//
//Copyright (c) 2009 Telecom ParisTech.
//
// Alternatively, this software module may be redistributed and/or modified
//  it under the terms of the GNU Lesser General Public License as published by
//  the Free Software Foundation; either version 2, or (at your option)
//  any later version.
//
/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
//
//	Authors:	
//					Jean Le Feuvre, Telecom ParisTech
//
/////////////////////////////////////////////////////////////////////////////////


#include "widgetman.h"

#ifdef GPAC_HAS_SPIDERMONKEY

JSBool SMJS_FUNCTION(widget_has_feature)
{
	char *feat_name;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return JS_FALSE;

	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;

	feat_name = SMJS_CHARS(c, argv[0]);
	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL( JS_FALSE ) );

	if (!strcmp(feat_name, "urn:mpeg:systems:mpeg-u:2009")) SMJS_SET_RVAL( BOOLEAN_TO_JSVAL( JS_TRUE ));
	SMJS_FREE(c, feat_name);
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(widget_open_url)
{
	GF_Event evt;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid || !argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;

	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_NAVIGATE;
	evt.navigate.to_url = SMJS_CHARS(c, argv[0]);
	gf_term_send_event(wid->widget->wm->term, &evt);
	SMJS_FREE(c, (char *)evt.navigate.to_url);

	return JS_TRUE;
}

JSBool SMJS_FUNCTION(widget_get_attention)
{
	jsval fval;
	SMJS_OBJ
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return JS_TRUE;

	if ((JS_LookupProperty(c, wid->widget->wm->obj, "getAttention", &fval)==JS_TRUE) && JSVAL_IS_OBJECT(fval)) {
		jsval args[1];
		args[0] = OBJECT_TO_JSVAL(wid->obj);
		JS_CallFunctionValue(c, wid->widget->wm->obj, fval, 1, args, SMJS_GET_RVAL);
	}
	return JS_TRUE;
}

JSBool SMJS_FUNCTION(widget_show_notification)
{
	jsval fval;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return JS_TRUE;

	if ((JS_LookupProperty(c, wid->widget->wm->obj, "showNotification", &fval)==JS_TRUE) && JSVAL_IS_OBJECT(fval)) {
		jsval *vars;
		u32 i;
		vars = gf_malloc(sizeof(jsval)*(argc+1));
		vars[0] = OBJECT_TO_JSVAL(wid->obj);
		for (i=0; i<argc; i++) 
			vars[i+1] = argv[i];

		JS_CallFunctionValue(c, wid->widget->wm->obj, fval, argc+1, vars, SMJS_GET_RVAL);
	}
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(widget_call_message_reply_callback)
{
	JSObject *list;
	jsval *vals, fval;
	u32 i, count;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetMessage *msg = SMJS_GET_PRIVATE(c, obj);
	if (!msg || !argc || !JSVAL_IS_OBJECT(argv[0]) ) return JS_FALSE;

	if ((JS_LookupProperty(c, obj, "replyCallback", &fval)==JS_TRUE) && JSVAL_IS_OBJECT(fval)) {
		list = JSVAL_TO_OBJECT(argv[0]);
		JS_GetArrayLength(c, list, (jsuint*) &count);
		vals = gf_malloc(sizeof(jsval)*(count+1));
		vals[0] = OBJECT_TO_JSVAL(obj);
		for (i=0; i<count; i++) {
			JS_GetElement(c, list, (jsint) i, &vals[i+1]);
		}
		JS_CallFunctionValue(c, obj, fval, count, vals, SMJS_GET_RVAL);
		gf_free(vals);
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(widget_message_handler_factory)
{
	char *msg_name;
	u32 i, count;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInterfaceInstance *bifce = (GF_WidgetInterfaceInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!bifce) return JS_FALSE;

	if (!argc) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;

	msg_name = SMJS_CHARS(c, argv[0]);
	if (!msg_name ) return JS_FALSE;

	SMJS_SET_RVAL( JSVAL_NULL );
	count = gf_list_count(bifce->ifce->messages);
	for (i=0; i<count; i++) {
		GF_WidgetMessage *msg = gf_list_get(bifce->ifce->messages, i);
		if (!strcmp(msg->name, msg_name)) {
			JSObject *an_obj = JS_NewObject(c, &bifce->wid->widget->wm->widgetAnyClass._class, 0, 0);
			SMJS_SET_PRIVATE(c, an_obj, msg);
			JS_DefineProperty(c, an_obj, "msgName", STRING_TO_JSVAL( JS_NewStringCopyZ(c, msg->name) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, an_obj, "interfaceHandler", OBJECT_TO_JSVAL( obj ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineFunction(c, an_obj, "onInvokeReply", widget_call_message_reply_callback, 1, 0);

			if ((argc==2) && JSVAL_IS_OBJECT(argv[1]) && !JSVAL_IS_NULL(argv[1]))
				JS_DefineProperty(c, an_obj, "replyCallback", argv[1], 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			
			SMJS_SET_RVAL( OBJECT_TO_JSVAL(an_obj) );
		}
	}
	SMJS_FREE(c, msg_name);
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(widget_invoke_message)
{
	jsval oval;
	GF_WidgetMessage *msg = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInterfaceInstance *bifce = (GF_WidgetInterfaceInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!bifce) return JS_FALSE;

	SMJS_SET_RVAL( JSVAL_NULL );

	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	if (JSVAL_IS_NULL(argv[0])) return JS_FALSE;
	msg = (GF_WidgetMessage *)SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
	if (!msg) return JS_FALSE;

	/*look for JS Callback "invoke" in the widget manager script*/
	if (JS_LookupProperty(c, bifce->ifce->obj, "invoke", &oval)==JS_TRUE) {
		if (JSVAL_IS_OBJECT(oval)) {
			JS_CallFunctionValue(bifce->wid->widget->wm->ctx, bifce->ifce->obj, oval, argc, argv, SMJS_GET_RVAL );
		}
	}

	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(widget_invoke_message_reply)
{
	jsval oval;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetMessage *msg = NULL;
	GF_WidgetInterfaceInstance *bifce = (GF_WidgetInterfaceInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!bifce) return JS_FALSE;

	SMJS_SET_RVAL( JSVAL_NULL );

	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	if (JSVAL_IS_NULL(argv[0])) return JS_FALSE;
	msg = (GF_WidgetMessage *)SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
	if (!msg) return JS_FALSE;

	/*look for JS Callback "invokeReply" in the widget manager script*/
	if (JS_LookupProperty(c, bifce->ifce->obj, "invokeReply", &oval)==JS_TRUE) {
		if (JSVAL_IS_OBJECT(oval)) {
			JS_CallFunctionValue(bifce->wid->widget->wm->ctx, bifce->ifce->obj, oval, argc, argv, SMJS_GET_RVAL);
		}
	}
	return JS_TRUE;
}

static void widget_interface_js_bind(JSContext *c, GF_WidgetInterfaceInstance *ifce)
{
	if (!ifce->obj) {
		ifce->obj = JS_NewObject(c, &ifce->wid->widget->wm->widgetAnyClass._class, 0, 0);
		SMJS_SET_PRIVATE(c, ifce->obj, ifce);
		gf_js_add_root(c, &ifce->obj, GF_JSGC_OBJECT);
		JS_DefineProperty(c, ifce->obj, "type", STRING_TO_JSVAL( JS_NewStringCopyZ(c, ifce->ifce->type) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, ifce->obj, "bound", STRING_TO_JSVAL( JS_NewStringCopyZ(c, ifce->hostname) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineFunction(c, ifce->obj, "invoke", widget_invoke_message, 1, 0);
		JS_DefineFunction(c, ifce->obj, "msgHandlerFactory", widget_message_handler_factory, 1, 0);
		JS_DefineFunction(c, ifce->obj, "invokeReply", widget_invoke_message_reply, 1, 0);
	}
}

static JSBool SMJS_FUNCTION(widget_get_interfaces)
{
	u32 i, count;
	char *ifce_name;
	JSObject *list;
	jsuint idx;
	jsval v;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return JS_FALSE;

	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	ifce_name = SMJS_CHARS(c, argv[0]);

	list = JS_NewArrayObject(c, 0, 0);

	count = gf_list_count(wid->bound_ifces);
	for (i=0; i<count; i++) {
		GF_WidgetInterfaceInstance *ifce = gf_list_get(wid->bound_ifces, i);
		if (strcmp(ifce->ifce->type, ifce_name)) continue;

		widget_interface_js_bind(c, ifce);

		JS_GetArrayLength(c, list, &idx);
		v = OBJECT_TO_JSVAL(ifce->obj);
		JS_SetElement(c, list, idx, &v);
	}
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(list) );
	SMJS_FREE(c, ifce_name);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION_EXT(widget_activate_component, Bool is_deactivate)
{
	u32 i, count;
	char *comp_id;
	SMJS_OBJ
	SMJS_ARGS
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return JS_FALSE;

	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	comp_id = SMJS_CHARS(c, argv[0]);

	count = gf_list_count(wid->widget->main->components);
	for (i=0; i<count; i++) {
		GF_WidgetComponent *comp = gf_list_get(wid->widget->main->components, i);
		if (!comp->id  || strcmp(comp->id, comp_id)) continue;
		
		if (is_deactivate) {
			wm_deactivate_component(c, wid, comp, NULL);
		} else {
			wm_activate_component(c, wid, comp, 0);
		}
		break;
	}
	SMJS_FREE(c, comp_id);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(widget_activate_widget)
{
	return widget_activate_component(SMJS_CALL_ARGS, 0);
}

static JSBool SMJS_FUNCTION(widget_deactivate_widget)
{
	return widget_activate_component(SMJS_CALL_ARGS, 1);
}

void widget_on_interface_bind(GF_WidgetInterfaceInstance *ifce, Bool unbind)
{
	jsval funval, rval, argv[1];

	const char *fun_name = unbind ? "onInterfaceUnbind" : "onInterfaceBind";
	if (!ifce || !ifce->wid || !ifce->wid->scene_context) return;

	/*look for JS Callback "invoke" in the widget manager script*/
	if (JS_LookupProperty(ifce->wid->scene_context, ifce->wid->scene_obj, fun_name, &funval)!=JS_TRUE) 
		return;
	if (!JSVAL_IS_OBJECT(funval)) return;

	widget_interface_js_bind(ifce->wid->widget->wm->ctx, ifce);
	argv[0] = OBJECT_TO_JSVAL(ifce->obj);
	JS_CallFunctionValue(ifce->wid->widget->wm->ctx, ifce->ifce->obj, funval, 1, argv, &rval);

}

SMJS_FUNC_PROP_GET(widget_getProperty)

	const char *opt;
	char *prop_name;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)SMJS_GET_PRIVATE(c, obj);
	if (!wid) return JS_FALSE;

	if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
	prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));
	if (!prop_name) return JS_FALSE;

	if (!strcmp(prop_name, "viewMode")) {
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "floating") );
	}
	else if (!strcmp(prop_name, "locale")) {
		opt = gf_cfg_get_key(wid->widget->wm->term->user->config, "Systems", "Language2CC");
		if (!opt) opt = "und";
		*vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, opt) );
	}
	else if (!strcmp(prop_name, "identifier")) {
		if (wid->widget->identifier) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->identifier) );
	}
	else if (!strcmp(prop_name, "authorName")) {
		if (wid->widget->authorName) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->authorName) );
	}
	else if (!strcmp(prop_name, "authorEmail")) {
		if (wid->widget->authorEmail) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->authorEmail) );
	}
	else if (!strcmp(prop_name, "authorHref")) {
		if (wid->widget->authorHref) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->authorHref) );
	}
	else if (!strcmp(prop_name, "name")) {
		if (wid->widget->name) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->name) );
	}
	else if (!strcmp(prop_name, "version")) {
		if (wid->widget->version) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->version) );
	}
	else if (!strcmp(prop_name, "description")) {
		if (wid->widget->description) *vp = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->description) );
	}
	else if (!strcmp(prop_name, "width")) {
		opt = gf_cfg_get_key(wid->widget->wm->term->user->config, (const char *) wid->secname, "width");
		*vp = INT_TO_JSVAL( (opt ? atoi(opt) : 0) );
	}
	else if (!strcmp(prop_name, "height")) {
		opt = gf_cfg_get_key(wid->widget->wm->term->user->config, (const char *) wid->secname, "height");
		*vp = INT_TO_JSVAL( (opt ? atoi(opt) : 0) );
	}
	else if (!strcmp(prop_name, "preferences")) {
	}
	SMJS_FREE(c, prop_name);
	return JS_TRUE;
}

SMJS_FUNC_PROP_SET( widget_setProperty) 

	/*avoids GCC warning*/
	if (!obj) obj = NULL;
#ifndef GPAC_CONFIG_DARWIN
	if (!id) id=0;
#endif
    if (!vp) vp=0;
	return JS_TRUE;
}

void widget_load(GF_WidgetManager *wm, GF_SceneGraph *scene, JSContext *c, JSObject *global, Bool unload)
{
	u32 i, count;
	GF_WidgetInstance *wi;

	/*Is this scenegraph a widget or not ? To find out, browse all widget instances*/
	i=0;
	while ((wi = gf_list_enum(wm->widget_instances, &i))) {
		if (!wi->scene || (wi->scene != scene)) continue;
		break;
	}
	if (!wi) return;

	/*OK we found our widget*/

	if (unload) {
		/*detach all bound interfaces from this script*/
		count = gf_list_count(wi->bound_ifces);
		for (i=0; i<count; i++) {
			GF_WidgetInterfaceInstance *ifce = gf_list_get(wi->bound_ifces, i);
			if (ifce->obj) {
				SMJS_SET_PRIVATE(c, ifce->obj, NULL);
				gf_js_remove_root(c, &ifce->obj, GF_JSGC_OBJECT);
				ifce->obj = NULL;
			}
		}
		return;
	} else {

		JSPropertySpec widgetClassProps[] = {
			SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
		};
		JSFunctionSpec widgetClassFuncs[] = {
			/*W3C*/
			SMJS_FUNCTION_SPEC("has_feature", widget_has_feature, 1),
			SMJS_FUNCTION_SPEC("openURL", widget_open_url, 1),
			SMJS_FUNCTION_SPEC("getAttention", widget_get_attention, 0),
			SMJS_FUNCTION_SPEC("showNotification", widget_show_notification, 0),
			/*MPEG*/
			SMJS_FUNCTION_SPEC("getInterfaceHandlersByType", widget_get_interfaces, 1),
			SMJS_FUNCTION_SPEC("activateComponentWidget", widget_activate_widget, 1),
			SMJS_FUNCTION_SPEC("deactivateComponentWidget", widget_deactivate_widget, 1),
			SMJS_FUNCTION_SPEC(0, 0, 0)
		};

		GF_JS_InitClass(c, global, 0, &wm->widgetClass, 0, 0,widgetClassProps, widgetClassFuncs, 0, 0);

		
		wi->scene_obj = JS_DefineObject(c, global, "widget", &wm->widgetClass._class, 0, 0);
		//JS_AliasProperty(c, global, "widget", "MPEGWidget");
		SMJS_SET_PRIVATE(c, wi->scene_obj, wi);
		/*and remember the script*/
		wi->scene_context = c;
		wi->scene_global = global;
	}
}

#endif
