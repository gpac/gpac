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
//Copyright (c) 2009.
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

JSBool widget_has_feature(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	const char *feat_name;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)JS_GetPrivate(c, obj);
	if (!wid) return JS_FALSE;

	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;

	feat_name = JS_GetStringBytes(JSVAL_TO_STRING( argv[0] ));
	*rval = BOOLEAN_TO_JSVAL( JS_FALSE );

	if (!strcmp(feat_name, "urn:mpeg:systems:mpeg-u:2009")) *rval = BOOLEAN_TO_JSVAL( JS_TRUE );

	return JS_TRUE;
}

JSBool widget_open_url(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	GF_Event evt;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)JS_GetPrivate(c, obj);
	if (!wid || !argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;

	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_NAVIGATE;
	evt.navigate.to_url = JS_GetStringBytes(JSVAL_TO_STRING( argv[0] ));
	return gf_term_send_event(wid->widget->wm->term, &evt);
	return JS_TRUE;
}

JSBool widget_get_attention(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	jsval fval;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)JS_GetPrivate(c, obj);
	if (!wid) return JS_TRUE;

	if ((JS_LookupProperty(c, wid->widget->wm->obj, "getAttention", &fval)==JS_TRUE) && JSVAL_IS_OBJECT(fval)) {
		jsval args[1];
		args[0] = OBJECT_TO_JSVAL(wid->obj);
		JS_CallFunctionValue(c, wid->widget->wm->obj, fval, 1, args, rval);
	}
	return JS_TRUE;
}

JSBool widget_show_notification(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	jsval fval;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)JS_GetPrivate(c, obj);
	if (!wid) return JS_TRUE;

	if ((JS_LookupProperty(c, wid->widget->wm->obj, "showNotification", &fval)==JS_TRUE) && JSVAL_IS_OBJECT(fval)) {
		jsval *vars;
		u32 i;
		vars = gf_malloc(sizeof(jsval)*(argc+1));
		vars[0] = OBJECT_TO_JSVAL(wid->obj);
		for (i=0; i<argc; i++) 
			vars[i+1] = argv[i];

		JS_CallFunctionValue(c, wid->widget->wm->obj, fval, argc+1, vars, rval);
	}
	return JS_TRUE;
}


static JSBool widget_call_message_reply_callback(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject *list;
	jsval *vals, fval;
	u32 i, count;
	GF_WidgetMessage *msg = JS_GetPrivate(c, obj);
	if (!msg || !argc || !JSVAL_IS_OBJECT(argv[0]) ) return JS_FALSE;

	if ((JS_LookupProperty(c, obj, "replyCallback", &fval)==JS_TRUE) && JSVAL_IS_OBJECT(fval)) {
		list = JSVAL_TO_OBJECT(argv[0]);
		JS_GetArrayLength(c, list, (jsuint*) &count);
		vals = gf_malloc(sizeof(jsval)*(count+1));
		vals[0] = OBJECT_TO_JSVAL(obj);
		for (i=0; i<count; i++) {
			JS_GetElement(c, list, (jsint) i, &vals[i+1]);
		}
		JS_CallFunctionValue(c, obj, fval, count, vals, rval);
		gf_free(vals);
	}
	return JS_TRUE;
}

static JSBool widget_message_handler_factory(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char *msg_name;
	u32 i, count;
	GF_WidgetInstance *wid = NULL;
	GF_WidgetInterfaceInstance *bifce = (GF_WidgetInterfaceInstance *)JS_GetPrivate(c, obj);
	if (!bifce) return JS_FALSE;

	if (!argc) return JS_FALSE;
	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;

	msg_name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));
	if (!msg_name ) return JS_FALSE;

	*rval = JSVAL_NULL;
	count = gf_list_count(bifce->ifce->messages);
	for (i=0; i<count; i++) {
		GF_WidgetMessage *msg = gf_list_get(bifce->ifce->messages, i);
		if (!strcmp(msg->name, msg_name)) {
			JSObject *an_obj = JS_NewObject(c, &bifce->wid->widget->wm->widgetAnyClass, 0, 0);
			JS_SetPrivate(c, an_obj, msg);
			JS_DefineProperty(c, an_obj, "msgName", STRING_TO_JSVAL( JS_NewStringCopyZ(c, msg->name) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineProperty(c, an_obj, "interfaceHandler", OBJECT_TO_JSVAL( obj ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			JS_DefineFunction(c, an_obj, "onInvokeReply", widget_call_message_reply_callback, 1, 0);

			if ((argc==2) && JSVAL_IS_OBJECT(argv[1]) && !JSVAL_IS_NULL(argv[1]))
				JS_DefineProperty(c, an_obj, "replyCallback", argv[1], 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
			
			*rval = OBJECT_TO_JSVAL(an_obj);
		}
	}
	return JS_TRUE;
}
static JSBool widget_invoke_message(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	jsval oval;
	GF_WidgetMessage *msg = NULL;
	GF_WidgetInterfaceInstance *bifce = (GF_WidgetInterfaceInstance *)JS_GetPrivate(c, obj);
	if (!bifce) return JS_FALSE;

	*rval = JSVAL_NULL;

	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	if (JSVAL_IS_NULL(argv[0])) return JS_FALSE;
	msg = (GF_WidgetMessage *)JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0]) );
	if (!msg) return JS_FALSE;

	/*look for JS Callback "invoke" in the widget manager script*/
	if (JS_LookupProperty(c, bifce->ifce->obj, "invoke", &oval)==JS_TRUE) {
		if (JSVAL_IS_OBJECT(oval)) {
			JS_CallFunctionValue(bifce->wid->widget->wm->ctx, bifce->ifce->obj, oval, argc, argv, rval);
		}
	}

	return JS_TRUE;
}

static JSBool widget_invoke_message_reply(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	jsval oval;
	GF_WidgetMessage *msg = NULL;
	GF_WidgetInterfaceInstance *bifce = (GF_WidgetInterfaceInstance *)JS_GetPrivate(c, obj);
	if (!bifce) return JS_FALSE;

	*rval = JSVAL_NULL;

	if (!JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;
	if (JSVAL_IS_NULL(argv[0])) return JS_FALSE;
	msg = (GF_WidgetMessage *)JS_GetPrivate(c, JSVAL_TO_OBJECT(argv[0]) );
	if (!msg) return JS_FALSE;

	/*look for JS Callback "invokeReply" in the widget manager script*/
	if (JS_LookupProperty(c, bifce->ifce->obj, "invokeReply", &oval)==JS_TRUE) {
		if (JSVAL_IS_OBJECT(oval)) {
			JS_CallFunctionValue(bifce->wid->widget->wm->ctx, bifce->ifce->obj, oval, argc, argv, rval);
		}
	}
	return JS_TRUE;
}

static void widget_interface_js_bind(JSContext *c, GF_WidgetInterfaceInstance *ifce)
{
	if (!ifce->obj) {
		ifce->obj = JS_NewObject(c, &ifce->wid->widget->wm->widgetAnyClass, 0, 0);
		JS_SetPrivate(c, ifce->obj, ifce);
		gf_js_add_root(c, &ifce->obj);
		JS_DefineProperty(c, ifce->obj, "type", STRING_TO_JSVAL( JS_NewStringCopyZ(c, ifce->ifce->type) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineProperty(c, ifce->obj, "bound", STRING_TO_JSVAL( JS_NewStringCopyZ(c, ifce->hostname) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineFunction(c, ifce->obj, "invoke", widget_invoke_message, 1, 0);
		JS_DefineFunction(c, ifce->obj, "msgHandlerFactory", widget_message_handler_factory, 1, 0);
		JS_DefineFunction(c, ifce->obj, "invokeReply", widget_invoke_message_reply, 1, 0);
	}
}

static JSBool widget_get_interfaces(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	u32 i, count;
	char *ifce_name;
	JSObject *list;
	jsuint idx;
	jsval v;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)JS_GetPrivate(c, obj);
	if (!wid) return JS_FALSE;

	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	ifce_name = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

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
	*rval = OBJECT_TO_JSVAL(list);
	return JS_TRUE;
}

static JSBool widget_activate_component(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval, Bool is_deactivate)
{
	u32 i, count;
	char *comp_id;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)JS_GetPrivate(c, obj);
	if (!wid) return JS_FALSE;

	if (!argc || !JSVAL_IS_STRING(argv[0])) return JS_TRUE;
	comp_id = JS_GetStringBytes(JSVAL_TO_STRING(argv[0]));

	count = gf_list_count(wid->widget->main->components);
	for (i=0; i<count; i++) {
		GF_WidgetComponent *comp = gf_list_get(wid->widget->main->components, i);
		if (!comp->id  || strcmp(comp->id, comp_id)) continue;
		
		if (is_deactivate) {
			wm_deactivate_component(c, wid, comp, NULL);
		} else {
			wm_activate_component(c, wid, comp, 0);
		}
		return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool widget_activate_widget(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return widget_activate_component(c, obj, argc, argv, rval, 0);
}

static JSBool widget_deactivate_widget(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	return widget_activate_component(c, obj, argc, argv, rval, 1);
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

JSBool widget_getProperty(JSContext *c, JSObject *obj, jsval id, jsval *rval)
{
	const char *opt;
	char *prop_name;
	GF_WidgetInstance *wid = (GF_WidgetInstance *)JS_GetPrivate(c, obj);
	if (!wid) return JS_FALSE;

	if (!JSVAL_IS_STRING(id)) return JS_TRUE;
	prop_name = JS_GetStringBytes(JSVAL_TO_STRING(id));
	if (!prop_name) return JS_FALSE;

	if (!strcmp(prop_name, "viewMode")) {
		*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, "floating") );
	}
	else if (!strcmp(prop_name, "locale")) {
		opt = gf_cfg_get_key(wid->widget->wm->term->user->config, "Systems", "Language2CC");
		if (!opt) opt = "und";
		*rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, opt) );
	}
	else if (!strcmp(prop_name, "identifier")) {
		if (wid->widget->identifier) *rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->identifier) );
	}
	else if (!strcmp(prop_name, "authorName")) {
		if (wid->widget->authorName) *rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->authorName) );
	}
	else if (!strcmp(prop_name, "authorEmail")) {
		if (wid->widget->authorEmail) *rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->authorEmail) );
	}
	else if (!strcmp(prop_name, "authorHref")) {
		if (wid->widget->authorHref) *rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->authorHref) );
	}
	else if (!strcmp(prop_name, "name")) {
		if (wid->widget->name) *rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->name) );
	}
	else if (!strcmp(prop_name, "version")) {
		if (wid->widget->version) *rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->version) );
	}
	else if (!strcmp(prop_name, "description")) {
		if (wid->widget->description) *rval = STRING_TO_JSVAL( JS_NewStringCopyZ(c, wid->widget->description) );
	}
	else if (!strcmp(prop_name, "width")) {
		opt = gf_cfg_get_key(wid->widget->wm->term->user->config, wid->secname, "width");
		*rval = INT_TO_JSVAL( JS_NewDouble(c, opt ? atoi(opt) : 0) );
	}
	else if (!strcmp(prop_name, "height")) {
		opt = gf_cfg_get_key(wid->widget->wm->term->user->config, wid->secname, "height");
		*rval = INT_TO_JSVAL( JS_NewDouble(c, opt ? atoi(opt) : 0) );
	}
	else if (!strcmp(prop_name, "preferences")) {
	}
	return JS_TRUE;
}

JSBool widget_setProperty(JSContext *c, JSObject *obj, jsval id, jsval *vp)
{
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
				JS_SetPrivate(c, ifce->obj, NULL);
				gf_js_remove_root(c, &ifce->obj);
				ifce->obj = NULL;
			}
		}
		return;
	} else {

		JSPropertySpec widgetClassProps[] = {
			{0, 0, 0, 0, 0}
		};
		JSFunctionSpec widgetClassFuncs[] = {
			/*W3C*/
			{"has_feature", widget_has_feature, 1, 0, 0},
			{"openURL", widget_open_url, 1, 0, 0},
			{"getAttention", widget_get_attention, 0, 0, 0},
			{"showNotification", widget_show_notification, 0, 0, 0},
			/*MPEG*/
			{"getInterfaceHandlersByType", widget_get_interfaces, 1, 0, 0},
			{"activateComponentWidget", widget_activate_widget, 1, 0, 0},
			{"deactivateComponentWidget", widget_deactivate_widget, 1, 0, 0},
			{0, 0, 0, 0, 0}
		};

		JS_InitClass(c, global, 0, &wm->widgetClass, 0, 0,widgetClassProps, widgetClassFuncs, 0, 0);

		
		wi->scene_obj = JS_DefineObject(c, global, "widget", &wm->widgetClass, 0, 0);
		JS_AliasProperty(c, global, "widget", "MPEGWidget");
		JS_SetPrivate(c, wi->scene_obj, wi);
		/*and remember the script*/
		wi->scene_context = c;
		wi->scene_global = global;
	}
}

#endif
