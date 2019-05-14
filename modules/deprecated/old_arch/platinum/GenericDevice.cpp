/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2009-2012
 *			All rights reserved
 *
 *  This file is part of GPAC / Platinum UPnP module
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
 *
 *	----------------------------------------------------------------------------------
 *		PLATINUM IS LICENSED UNDER GPL or commercial agreement - cf platinum license
 *	----------------------------------------------------------------------------------
 *
 */


#include "GenericDevice.h"
#include "GPACPlatinum.h"
//#include "PltXmlHelper.h"

NPT_SET_LOCAL_LOGGER("gpac.genericdevice")

GPAC_ServiceItem::GPAC_ServiceItem(GPAC_DeviceItem *device, PLT_Service *service) : m_device(device), m_service(service)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	obj = NULL;
	on_event = JSVAL_NULL;
	m_StateListeners = gf_list_new();
	m_ArgListeners = gf_list_new();
	subscribed = GF_FALSE;
	vars=NULL;
#endif
}

GPAC_ServiceItem::~GPAC_ServiceItem()
{
#ifdef GPAC_HAS_SPIDERMONKEY
	DetachJS();
	gf_list_del(m_StateListeners);
	gf_list_del(m_ArgListeners);
#endif
}

#ifdef GPAC_HAS_SPIDERMONKEY
void GPAC_ServiceItem::DetachJS()
{
	if (obj) {
		gf_js_remove_root(js_ctx, &obj, GF_JSGC_OBJECT);
		SMJS_SET_PRIVATE(js_ctx, obj, NULL);
		obj = NULL;
	}
	if (!JSVAL_IS_NULL(on_event)) {
		gf_js_remove_root(js_ctx, &on_event, GF_JSGC_VAL);
		on_event = JSVAL_NULL;
	}
	while (gf_list_count(m_StateListeners)) {
		GPAC_StateVariableListener *svl = (GPAC_StateVariableListener *)gf_list_get(m_StateListeners, 0);
		gf_list_rem(m_StateListeners, 0);
		gf_js_remove_root(js_ctx, &svl->on_event, GF_JSGC_VAL);
		delete svl;
	}
	while (gf_list_count(m_ArgListeners)) {
		GPAC_ActionArgListener *argl = (GPAC_ActionArgListener *)gf_list_get(m_ArgListeners, 0);
		gf_list_rem(m_ArgListeners, 0);
		gf_js_remove_root(js_ctx, &argl->on_event, GF_JSGC_VAL);
		delete argl;
	}
}
#endif


GPAC_DeviceItem::GPAC_DeviceItem(PLT_DeviceDataReference device, NPT_String uuid)
	: m_device(device), m_UUID(uuid)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	obj = NULL;
	js_ctx = NULL;
	m_Services = gf_list_new();
#endif
}

GPAC_DeviceItem::~GPAC_DeviceItem()
{
#ifdef GPAC_HAS_SPIDERMONKEY
	DetachJS();
#endif
	gf_list_del(m_Services);
}

#ifdef GPAC_HAS_SPIDERMONKEY
void GPAC_DeviceItem::DetachJS() {
	if (obj) {
		gf_js_remove_root(js_ctx, &obj, GF_JSGC_OBJECT);
		SMJS_SET_PRIVATE(js_ctx, obj, NULL);
		obj = NULL;
	}
	while (gf_list_count(m_Services)) {
		GPAC_ServiceItem *item = (GPAC_ServiceItem*)gf_list_get(m_Services, 0);
		gf_list_rem(m_Services, 0);
		delete item;
	}
}
#endif

#ifdef GPAC_HAS_SPIDERMONKEY
static JSBool SMJS_FUNCTION(upnp_service_set_listener)
{
	GPAC_StateVariableListener *svl = NULL;
	char *name=NULL;
	u32 i;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_ServiceItem *service = (GPAC_ServiceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!service || !argc || !JSVAL_IS_OBJECT(argv[0])) return JS_FALSE;

	if (argc<1) {
		if (!JSVAL_IS_NULL(service->on_event))
			gf_js_remove_root(c, &service->on_event, GF_JSGC_VAL);
		service->on_event = JSVAL_NULL;
		if (!JSVAL_IS_NULL(argv[0])) {
			service->on_event = argv[0];
			gf_js_add_root(c, &service->on_event, GF_JSGC_VAL);
			if (!service->subscribed) {
				service->m_device->m_pUPnP->m_pGenericController->m_CtrlPoint->Subscribe(service->m_service);
				service->subscribed = GF_TRUE;
			}
		}
		return JS_TRUE;
	}
	if (!JSVAL_IS_STRING(argv[1])) return JS_FALSE;
	name = SMJS_CHARS(c, argv[1]);
	if (!name) return JS_FALSE;

	/*variable state listener*/
	i=0;
	while ((svl = (GPAC_StateVariableListener *)gf_list_enum(service->m_StateListeners, &i))) {
		if (svl->name == (const char *) name) break;
	}
	if (!svl) {
		svl = new GPAC_StateVariableListener();
		svl->name = name;
		svl->var = service->m_service->FindStateVariable(name);
		gf_list_add(service->m_StateListeners, svl);
	}
	if (!JSVAL_IS_NULL(svl->on_event))
		gf_js_remove_root(c, &svl->on_event, GF_JSGC_VAL);
	if (JSVAL_IS_NULL(argv[0])) {
		gf_list_del_item(service->m_StateListeners, svl);
		delete svl;
	}
	svl->on_event = argv[0];
	gf_js_add_root(c, &svl->on_event, GF_JSGC_VAL);
	if (!service->subscribed) {
		service->m_device->m_pUPnP->m_pGenericController->m_CtrlPoint->Subscribe(service->m_service);
		service->subscribed = GF_TRUE;
	}
	SMJS_FREE(c, name);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_service_set_action_listener)
{
	PLT_ActionDesc *action;
	PLT_ArgumentDesc *desc;
	GPAC_ActionArgListener *argl  = NULL;
	char *name;
	Bool script_callback = GF_FALSE;
	u32 i;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_ServiceItem *service = (GPAC_ServiceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!service || (argc<2) || !JSVAL_IS_STRING(argv[0]) || !JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;

	name = SMJS_CHARS(c, argv[0]);
	if (!name) return JS_FALSE;

	action = service->m_service->FindActionDesc(name);
	SMJS_FREE(c, name);

	if (!action) return JS_FALSE;

	desc = NULL;
	if (argc==3) {
		if (JSVAL_IS_BOOLEAN(argv[2])) {
			script_callback = GF_TRUE;
		} else {
			if (!JSVAL_IS_STRING(argv[2]) ) return JS_FALSE;
			name = SMJS_CHARS(c, argv[2]);
			if (!name) return JS_FALSE;
			desc = action->GetArgumentDesc(name);
			SMJS_FREE(c, name);
			if (!desc) return JS_FALSE;
		}
	}


	/*action listener*/
	i=0;
	while ((argl = (GPAC_ActionArgListener *)gf_list_enum(service->m_ArgListeners, &i))) {
		if (argl->arg == desc) break;
		argl = NULL;
	}
	if (!argl) {
		argl = new GPAC_ActionArgListener();
		argl->arg = desc;
		gf_list_add(service->m_ArgListeners, argl);
	}
	argl->action = action;
	if (!JSVAL_IS_NULL(argl->on_event))
		gf_js_remove_root(c, &argl->on_event, GF_JSGC_VAL);
	if (JSVAL_IS_NULL(argv[1])) {
		gf_list_del_item(service->m_ArgListeners, argl);
		delete argl;
	}
	argl->on_event = argv[1];
	argl->is_script = script_callback;
	gf_js_add_root(c, &argl->on_event, GF_JSGC_VAL);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_service_get_scpd)
{
	NPT_String name;
	SMJS_OBJ
	GPAC_ServiceItem *service = (GPAC_ServiceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!service )
		return JS_FALSE;

	service->m_service->GetSCPDXML(name);
	SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, name) ) );
	return JS_TRUE;
}

SMJS_DECL_FUNC_PROP_GET( upnpservice_getProperty)
{
#ifdef USE_FFDEV_15
	JSObject *obj = (JSObject *) __hobj;
	jsid id = (jsid) __hid;
#endif
	char *prop_name;
	GPAC_ServiceItem *service = (GPAC_ServiceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!service) return JS_FALSE;

	if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
	prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));
	if (!prop_name) return JS_FALSE;

	if (!strcmp(prop_name, "Device")) {
		VPASSIGN( OBJECT_TO_JSVAL(service->m_device->obj) );
	}
	else if (!strcmp(prop_name, "ModifiedStateVariablesCount")) {
		VPASSIGN( INT_TO_JSVAL(service->vars ? service->vars->GetItemCount() : 0) );
	}

	SMJS_FREE(c, prop_name);
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(upnp_service_has_var)
{
	char *name = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_ServiceItem *service = (GPAC_ServiceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!service || !argc || !JSVAL_IS_STRING(argv[0]) )
		return JS_FALSE;

	name = SMJS_CHARS(c, argv[0]);
	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL( (service->m_service->FindStateVariable(name)==NULL) ? JS_FALSE : JS_TRUE ) );
	SMJS_FREE(c, name);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_service_has_action)
{
	PLT_ActionDesc *action;
	char *name = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_ServiceItem *service = (GPAC_ServiceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!service || !argc || !JSVAL_IS_STRING(argv[0]) )
		return JS_FALSE;

	name = SMJS_CHARS(c, argv[0]);
	action = service->m_service->FindActionDesc(name);
	SMJS_FREE(c, name);

	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(action ? JS_TRUE : JS_FALSE) );
	if (!action) return JS_TRUE;

	if ((argc==2) && JSVAL_IS_STRING(argv[1])) {
		name = SMJS_CHARS(c, argv[1]);
		if (action->GetArgumentDesc(name) == NULL) SMJS_SET_RVAL( BOOLEAN_TO_JSVAL(JS_FALSE) );
		SMJS_FREE(c, name);
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_service_call_action)
{
	GPAC_ActionUDTA *act_udta = NULL;
	char *action_name = NULL;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_ServiceItem *service = (GPAC_ServiceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!service || !argc || !JSVAL_IS_STRING(argv[0]) ) return JS_FALSE;

	action_name = SMJS_CHARS(c, argv[0]);
	PLT_ActionDesc* action_desc = service->m_service->FindActionDesc(action_name);
	SMJS_FREE(c, action_name);

	if (action_desc == NULL) return JS_FALSE;
	PLT_ActionReference action;

	NPT_CHECK_SEVERE(
	    service->m_device->m_pUPnP->m_pGenericController->m_CtrlPoint->CreateAction(
	        service->m_device->m_device,
	        service->m_service->GetServiceType(),
	        action_name,
	        action)
	);


	if ((argc>=2) && JSVAL_IS_OBJECT(argv[1])) {
		JSObject *list = JSVAL_TO_OBJECT(argv[1]);
		u32 i, count;
		JS_GetArrayLength(c, list, (jsuint*) &count);

		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Calling %s(", action_name));
		i=0;
		while (i+2<=count) {
			NPT_Result res;
			jsval an_arg;
			char *param_val, *_param_val = NULL;
			char szParamVal[1024];
			JS_GetElement(c, list, (jsint) i, &an_arg);
			char *param_name = SMJS_CHARS(c, an_arg);

			JS_GetElement(c, list, (jsint) i+1, &an_arg);

			param_val = (char*)"";
			if (JSVAL_IS_STRING(an_arg)) {
				param_val = _param_val = SMJS_CHARS(c, an_arg);
			} else if (JSVAL_IS_BOOLEAN(an_arg)) {
				param_val = (char *) ((JSVAL_TO_BOOLEAN(an_arg) == JS_TRUE) ? "true" : "false");
			}
			else if (JSVAL_IS_INT(argv[1])) {
				sprintf(szParamVal, "%d",  JSVAL_TO_INT(an_arg));
				param_val = szParamVal;
			}
			else if (JSVAL_IS_NUMBER(an_arg)) {
				jsdouble v;
				JS_ValueToNumber(c, an_arg, &v);
				sprintf(szParamVal, "%g", v);
				param_val = szParamVal;
			}

			if (!param_name || !param_val) res = NPT_FAILURE;
			else {
				GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, (" %s(%s)", param_name, param_val));
				res = action->SetArgumentValue(param_name, param_val);
			}
			SMJS_FREE(c, param_name);
			SMJS_FREE(c, _param_val);

			if (res != NPT_SUCCESS) return JS_FALSE;

			i+=2;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, (" )\n"));
	}

	if ((argc==3) && JSVAL_IS_OBJECT(argv[2])) {
		act_udta = new 	GPAC_ActionUDTA();
		act_udta->udta = argv[2];
		gf_js_add_root(c, &act_udta->udta, GF_JSGC_VAL);
	}
	service->m_device->m_pUPnP->m_pGenericController->m_CtrlPoint->InvokeAction(action, act_udta);
	return JS_TRUE;
}

#endif

void GPAC_DeviceItem::RefreshServiceList()
{
	u32 i;
	NPT_Array<PLT_Service*> services = m_device->GetServices();

	for (i=0; i<services.GetItemCount(); i++) {
		PLT_Service *serv = services[i];
		FindService(serv->GetServiceType());
	}
}

GPAC_ServiceItem *GPAC_DeviceItem::FindService(const char *type)
{
	u32 i, count;
	GPAC_ServiceItem *serv;

	count = gf_list_count(m_Services);
	for (i=0; i<count; i++) {
		serv = (GPAC_ServiceItem*)gf_list_get(m_Services, i);
		if (serv->m_service->GetServiceType() == type)
			return serv;
	}

	PLT_Service *service;
	if (m_device->FindServiceByType(type, service) != NPT_SUCCESS) return NULL;

	serv = new GPAC_ServiceItem(this, service);

#ifdef GPAC_HAS_SPIDERMONKEY
	serv->js_ctx = js_ctx;
	serv->obj = JS_NewObject(serv->js_ctx, &m_pUPnP->upnpServiceClass._class, 0, obj);
	gf_js_add_root(serv->js_ctx, &serv->obj, GF_JSGC_OBJECT);
	SMJS_SET_PRIVATE(serv->js_ctx, serv->obj, serv);
	JS_DefineProperty(serv->js_ctx, serv->obj, "Name", STRING_TO_JSVAL( JS_NewStringCopyZ(serv->js_ctx, service->GetServiceID()) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(serv->js_ctx, serv->obj, "Type", STRING_TO_JSVAL( JS_NewStringCopyZ(serv->js_ctx, service->GetServiceType()) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(serv->js_ctx, serv->obj, "Hostname", STRING_TO_JSVAL( JS_NewStringCopyZ(serv->js_ctx, m_device->GetURLBase().GetHost() ) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunction(serv->js_ctx, serv->obj, "SetStateVariableListener", upnp_service_set_listener, 1, 0);
	JS_DefineFunction(serv->js_ctx, serv->obj, "HasStateVariable", upnp_service_has_var, 1, 0);
	JS_DefineFunction(serv->js_ctx, serv->obj, "HasAction", upnp_service_has_action, 2, 0);
	JS_DefineFunction(serv->js_ctx, serv->obj, "CallAction", upnp_service_call_action, 2, 0);
	JS_DefineFunction(serv->js_ctx, serv->obj, "SetActionListener", upnp_service_set_action_listener, 2, 0);
	JS_DefineFunction(serv->js_ctx, serv->obj, "GetSCPD", upnp_service_get_scpd, 1, 0);

#endif

	gf_list_add(m_Services, serv);
	return serv;
}


GPAC_GenericController::GPAC_GenericController(PLT_CtrlPointReference& ctrlPoint, GF_UPnP *upnp)
{
	m_pUPnP = upnp;
	m_CtrlPoint = ctrlPoint;
	m_CtrlPoint->AddListener(this);
	m_ControlPointLock = gf_mx_new("GenericController");
	m_Devices = gf_list_new();
}

GPAC_GenericController::~GPAC_GenericController()
{
	m_CtrlPoint->RemoveListener(this);
	gf_mx_del(m_ControlPointLock);
	while (gf_list_count(m_Devices)) {
		GPAC_DeviceItem*ms = (GPAC_DeviceItem*)gf_list_get(m_Devices, 0);
		gf_list_rem(m_Devices, 0);
		delete ms;
	}
	gf_list_del(m_Devices);
}



NPT_Result GPAC_GenericController::OnDeviceAdded(PLT_DeviceDataReference& device)
{
	GPAC_DeviceItem *item;
	NPT_String uuid = device->GetUUID();
	gf_mx_p(m_ControlPointLock);

	u32 i, count = gf_list_count(m_Devices);
	for (i=0; i<count; i++) {
		item = (GPAC_DeviceItem *) gf_list_get(m_Devices, i);
		if (item->m_UUID == uuid ) {
			gf_mx_v(m_ControlPointLock);
			return NPT_SUCCESS;
		}
	}
	item = new GPAC_DeviceItem(device, device->GetUUID() );
	gf_list_add(m_Devices, item );
	m_pUPnP->OnDeviceAdd(item, 1);
	gf_mx_v(m_ControlPointLock);
	return NPT_SUCCESS;
}
NPT_Result GPAC_GenericController::OnDeviceRemoved(PLT_DeviceDataReference& device)
{
	u32 i, count;
	GPAC_DeviceItem *item = NULL;
	NPT_String uuid = device->GetUUID();
	gf_mx_p(m_ControlPointLock);
	count = gf_list_count(m_Devices);
	for (i=0; i<count; i++) {
		item = (GPAC_DeviceItem *) gf_list_get(m_Devices, i);
		if (item->m_UUID == uuid ) {
			gf_list_rem(m_Devices, i);
			break;
		}
		item = NULL;
	}
	if (item) {
		m_pUPnP->OnDeviceAdd(item, 0);
		delete item;
	}
	gf_mx_v(m_ControlPointLock);
	return NPT_SUCCESS;
}


#ifdef GPAC_HAS_SPIDERMONKEY

static JSBool SMJS_FUNCTION(upnp_action_get_argument_value)
{
	NPT_String res;
	char *arg_name = NULL;
	SMJS_OBJ
	SMJS_ARGS
	PLT_Action *action = (PLT_Action *) SMJS_GET_PRIVATE(c, obj);
	if (!action || !argc || !JSVAL_IS_STRING(argv[0])) return JS_FALSE;

	arg_name = SMJS_CHARS(c, argv[0]);
	if (!arg_name) return JS_FALSE;

	action->GetArgumentValue(arg_name, res);
	SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, res) ));
	SMJS_FREE(c, arg_name);
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(upnp_action_get_error_code)
{
	NPT_String res;
	SMJS_OBJ
	PLT_Action *action = (PLT_Action *) SMJS_GET_PRIVATE(c, obj);
	if (!action ) return JS_FALSE;
	SMJS_SET_RVAL( INT_TO_JSVAL( action->GetErrorCode() ));
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_action_get_error)
{
	NPT_String res;
	unsigned int code;
	SMJS_OBJ
	PLT_Action *action = (PLT_Action *) SMJS_GET_PRIVATE(c, obj);
	if (!action ) return JS_FALSE;
	SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, action->GetError( &code ) ) ) );
	return JS_TRUE;
}

#endif


NPT_Result GPAC_GenericController::OnActionResponse(NPT_Result res, PLT_ActionReference& action, void* userdata)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	u32 i, count;
	GPAC_DeviceItem *item = NULL;
	GPAC_ServiceItem *serv = NULL;
	GPAC_ActionArgListener *argl, *act_l;
	PLT_Service* service = action->GetActionDesc().GetService();
	NPT_String uuid;
	GPAC_ActionUDTA *act_udta = (GPAC_ActionUDTA *)userdata;

	/*this is NOT an actionResponse to an action triggered on a generic device*/
	if (act_udta && act_udta->m_Reserved) act_udta = NULL;

	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Receive %s Response - error code %d\n", (char *) action->GetActionDesc().GetName(), res));

	gf_mx_p(m_ControlPointLock);

	/*get our device*/
	uuid = service->GetDevice()->GetUUID();
	count = gf_list_count(m_Devices);
	for (i=0; i<count; i++) {
		item = (GPAC_DeviceItem *) gf_list_get(m_Devices, i);
		if (item->m_UUID == uuid ) {
			break;
		}
		item = NULL;
	}
	gf_mx_v(m_ControlPointLock);

	if (!item) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[UPnP] Receive %s Response on unknown device (uuid %s)\n", (char *) action->GetActionDesc().GetName(), (char *) uuid));
		goto exit;
	}
	/*get our service*/
	count = gf_list_count(item->m_Services);
	for (i=0; i<count; i++) {
		serv = (GPAC_ServiceItem *)gf_list_get(item->m_Services, i);
		if (serv->m_service == service) break;
	}
	if (!serv) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[UPnP] Receive %s Response on unknown service %s\n", (char *) action->GetActionDesc().GetName(), (char *) service->GetServiceType()));
		goto exit;
	}

	/*locate our listeners*/
	act_l = NULL;
	i=0;
	while ((argl = (GPAC_ActionArgListener *)gf_list_enum(serv->m_ArgListeners, &i))) {
		NPT_String value;
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] checking argument %s\n", (char *) argl->action->GetName() ));
		if (argl->action->GetName() != action->GetActionDesc().GetName() ) continue;

		/*global action listener*/
		if (argl->arg==NULL) {
			act_l = argl;
			continue;
		}
		/*if error don't trigger listeners*/
		if (res != NPT_SUCCESS) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[UPnP] Receive %s Response: error on remote device %d\n", (char *) action->GetActionDesc().GetName(), res));
			continue;
		}
		/*action arg listener*/
		if (action->GetArgumentValue(argl->arg->GetName(), value) == NPT_SUCCESS) {
			jsval argv[1], rval;

			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Calling handler for response %s argument %s\n", (char *) action->GetActionDesc().GetName(), (char *) argl->arg->GetName() ));
			m_pUPnP->LockJavascript(GF_TRUE);
			argv[0] = STRING_TO_JSVAL( JS_NewStringCopyZ(serv->js_ctx, value) );
			JS_CallFunctionValue(serv->js_ctx, serv->obj, argl->on_event, 1, argv, &rval);
			m_pUPnP->LockJavascript(GF_FALSE);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[UPnP] %s Response: couldn't get argument %s value\n", (char *) action->GetActionDesc().GetName(), (char *) argl->arg->GetName() ));
		}
	}

	if (act_l) {
		jsval rval;
		m_pUPnP->LockJavascript(GF_TRUE);
		if (act_l->is_script) {
			JSObject *act_obj;
			jsval argv[2];

			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Calling handler for response %s\n", (char *) action->GetActionDesc().GetName()));

			act_obj = JS_NewObject(serv->js_ctx, &item->m_pUPnP->upnpDeviceClass._class, 0, item->obj);
			SMJS_SET_PRIVATE(serv->js_ctx, act_obj, (void *)action.AsPointer() );
			JS_DefineFunction(serv->js_ctx, act_obj, "GetArgumentValue", upnp_action_get_argument_value, 1, 0);
			JS_DefineFunction(serv->js_ctx, act_obj, "GetErrorCode", upnp_action_get_error_code, 1, 0);
			JS_DefineFunction(serv->js_ctx, act_obj, "GetError", upnp_action_get_error, 1, 0);

			gf_js_add_root(serv->js_ctx, &act_obj, GF_JSGC_OBJECT);
			argv[0] = OBJECT_TO_JSVAL(act_obj);
			if (act_udta) {
				argv[1] = act_udta->udta;
				JS_CallFunctionValue(serv->js_ctx, serv->obj, act_l->on_event, 2, argv, &rval);
			} else {
				JS_CallFunctionValue(serv->js_ctx, serv->obj, act_l->on_event, 1, argv, &rval);
			}
			gf_js_remove_root(serv->js_ctx, &act_obj, GF_JSGC_OBJECT);
		}
		/*if error don't trigger listeners*/
		else if (res == NPT_SUCCESS) {
			GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Calling handler for response %s\n", (char *) action->GetActionDesc().GetName()));
			JS_CallFunctionValue(serv->js_ctx, serv->obj, act_l->on_event, 0, 0, &rval);
		}
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[UPnP] response %s has error %d\n", (char *) action->GetActionDesc().GetName(), res ));
		}
		m_pUPnP->LockJavascript(GF_FALSE);
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Done processing response %s\n", (char *) action->GetActionDesc().GetName()));

exit:
	if (act_udta) {
		gf_js_remove_root(serv->js_ctx, &act_udta->udta, GF_JSGC_VAL);
		delete act_udta;
	}

	return NPT_SUCCESS;
#else
	return NPT_SUCCESS;
#endif
}

NPT_Result GPAC_GenericController::OnEventNotify(PLT_Service* service, NPT_List<PLT_StateVariable*>* vars)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	u32 i, count;
	GPAC_DeviceItem *item = NULL;
	GPAC_ServiceItem *serv = NULL;
	GPAC_StateVariableListener *svl;

	gf_mx_p(m_ControlPointLock);

	NPT_String uuid = service->GetDevice()->GetUUID();
	count = gf_list_count(m_Devices);
	for (i=0; i<count; i++) {
		item = (GPAC_DeviceItem *) gf_list_get(m_Devices, i);
		if (item->m_UUID == uuid ) {
			break;
		}
		item = NULL;
	}
	gf_mx_v(m_ControlPointLock);

	if (!item) return NPT_SUCCESS;

	count = gf_list_count(item->m_Services);
	for (i=0; i<count; i++) {
		serv = (GPAC_ServiceItem *)gf_list_get(item->m_Services, i);
		if (serv->m_service == service) break;
	}
	if (!serv) return NPT_SUCCESS;

	if (!JSVAL_IS_NULL(serv->on_event)) {
		jsval rval;
		m_pUPnP->LockJavascript(GF_TRUE);
		serv->vars = vars;
		JS_CallFunctionValue(serv->js_ctx, serv->obj, serv->on_event, 0, 0, &rval);
		m_pUPnP->LockJavascript(GF_FALSE);
		serv->vars = NULL;
	}

	i=0;
	while ((svl = (GPAC_StateVariableListener *)gf_list_enum(serv->m_StateListeners, &i))) {
		/*check if we can find our var in this list*/
		if (vars->Contains(svl->var)) {
			jsval argv[1], rval;
			m_pUPnP->LockJavascript(GF_TRUE);
			argv[0] = STRING_TO_JSVAL( JS_NewStringCopyZ(serv->js_ctx, svl->var->GetValue() ) );
			JS_CallFunctionValue(serv->js_ctx, serv->obj, svl->on_event, 1, argv, &rval);
			m_pUPnP->LockJavascript(GF_FALSE);
		}

	}

#endif
	return NPT_SUCCESS;
}


GPAC_Service::GPAC_Service(PLT_DeviceData* device, const char *type,  const char *id, const char *name, const char *last_change_namespace)
	: PLT_Service(device, type,  id, name, last_change_namespace)
{
#ifdef GPAC_HAS_SPIDERMONKEY
	m_pObj = NULL;
	m_pCtx = NULL;
#endif
}

GPAC_Service::~GPAC_Service()
{
#ifdef GPAC_HAS_SPIDERMONKEY
	if (m_pObj) gf_js_remove_root(m_pCtx, &m_pObj, GF_JSGC_OBJECT);
#endif
}

#ifdef GPAC_HAS_SPIDERMONKEY


static JSBool SMJS_FUNCTION(upnp_service_set_state_variable)
{
	char *name, *val;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_Service* service = (GPAC_Service*) SMJS_GET_PRIVATE(c, obj);
	if (!service) return JS_FALSE;

	name = SMJS_CHARS(c, argv[0]);
	if (!name) return JS_FALSE;

	val = SMJS_CHARS(c, argv[1]);
	if (!val) {
		SMJS_FREE(c, name);
		return JS_FALSE;
	}

	service->SetStateVariable(name, val);
	SMJS_FREE(c, name);
	SMJS_FREE(c, val);
	return JS_TRUE;
}

void GPAC_Service::SetupJS(JSContext *c, GF_UPnP *upnp, JSObject *parent)
{
	m_pCtx = c;
	m_pObj = JS_NewObject(c, &upnp->upnpDeviceClass._class, 0, parent);
	gf_js_add_root(m_pCtx, &m_pObj, GF_JSGC_OBJECT);
	SMJS_SET_PRIVATE(c, m_pObj, this);
	JS_DefineFunction(c, m_pObj, "SetStateVariable", upnp_service_set_state_variable, 2, 0);

}
#endif


GPAC_GenericDevice::GPAC_GenericDevice(const char* FriendlyName, const char *device_id)
	: PLT_DeviceHost("/", "", device_id ? device_id : "urn:schemas-upnp-org:device:GenericDevice:1", FriendlyName)
{
	m_pServices = gf_list_new();

#ifdef GPAC_HAS_SPIDERMONKEY
	run_proc = JSVAL_NULL;
	act_proc = JSVAL_NULL;
	obj = NULL;
	js_source = "";
	act_ref = NULL;
	m_pSema = NULL;
	m_pMutex = gf_mx_new("UPnP Generic Device");
#endif
}

GPAC_GenericDevice::~GPAC_GenericDevice()
{
	gf_list_del(m_pServices);
#ifdef GPAC_HAS_SPIDERMONKEY
	if (m_pSema) gf_sema_del(m_pSema);
	m_pSema = NULL;
	gf_mx_del(m_pMutex);
#endif
}

#ifdef GPAC_HAS_SPIDERMONKEY
void GPAC_GenericDevice::DetachJS(JSContext *c)
{
	u32 i, count;
	if (obj)
		gf_js_remove_root(c, &obj, GF_JSGC_OBJECT);
	obj = NULL;
	if (!JSVAL_IS_NULL(run_proc))
		gf_js_remove_root(c, &run_proc, GF_JSGC_VAL);
	run_proc = JSVAL_NULL;
	if (!JSVAL_IS_NULL(act_proc))
		gf_js_remove_root(c, &act_proc, GF_JSGC_VAL);
	act_proc = JSVAL_NULL;

	count = gf_list_count(m_pServices);
	for (i=0; i<count; i++) {
		GPAC_Service *service = (GPAC_Service*)gf_list_get(m_pServices, i);
		if (service->m_pObj) {
			gf_js_remove_root(c, &service->m_pObj, GF_JSGC_OBJECT);
			service->m_pObj = NULL;
		}
	}
}
#endif

NPT_Result
GPAC_GenericDevice::SetupServices()
{
	u32 i, count;
	count = gf_list_count(m_pServices);
	for (i=0; i<count; i++) {
		GPAC_Service *service = (GPAC_Service *)gf_list_get(m_pServices, i);
		AddService(service);
	}
	return NPT_SUCCESS;
}


#ifdef GPAC_HAS_SPIDERMONKEY
static JSBool SMJS_FUNCTION(upnp_action_get_argument)
{
	char *act_name;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_GenericDevice *device = (GPAC_GenericDevice *)SMJS_GET_PRIVATE(c, obj);
	if (!device || !argc || !JSVAL_IS_STRING(argv[0]) ) return JS_FALSE;

	act_name = SMJS_CHARS(c, argv[0]);
	NPT_String value;

	if (device->act_ref->GetArgumentValue(act_name, value) != NPT_SUCCESS) return JS_FALSE;
	SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, value) ));
	SMJS_FREE(c, act_name);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_action_send_reply)
{
	SMJS_OBJ
	SMJS_ARGS
	GPAC_GenericDevice *device = (GPAC_GenericDevice *)SMJS_GET_PRIVATE(c, obj);
	if (!device) return JS_FALSE;

	if (argc && JSVAL_IS_OBJECT(argv[0]) ) {
		JSObject *list = JSVAL_TO_OBJECT(argv[0]);
		u32 i, count;
		JS_GetArrayLength(c, list, (jsuint*) &count);

		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Calling response %s(", (char *) device->act_ref->GetActionDesc().GetName()));
		i=0;
		while (i+2<=count) {
			jsval an_arg;
			NPT_Result res;
			char *param_val, *_param_val = NULL;
			char szParamVal[1024];
			JS_GetElement(c, list, (jsint) i, &an_arg);
			char *param_name = SMJS_CHARS(c, an_arg);

			JS_GetElement(c, list, (jsint) i+1, &an_arg);

			param_val = (char*)"";
			if (JSVAL_IS_STRING(an_arg)) {
				param_val = _param_val = SMJS_CHARS(c, an_arg);
			} else if (JSVAL_IS_BOOLEAN(an_arg)) {
				param_val = (char *) ((JSVAL_TO_BOOLEAN(an_arg) == JS_TRUE) ? "true" : "false");
			}
			else if (JSVAL_IS_INT(argv[1])) {
				sprintf(szParamVal, "%d",  JSVAL_TO_INT(an_arg));
				param_val = szParamVal;
			}
			else if (JSVAL_IS_NUMBER(an_arg)) {
				jsdouble v;
				JS_ValueToNumber(c, an_arg, &v);
				sprintf(szParamVal, "%g", v);
				param_val = szParamVal;
			}

			if (!param_name || !param_val) res = NPT_FAILURE;
			else {
				GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, (" %s(%s)", param_name, param_val));

				res = device->act_ref->SetArgumentValue(param_name, param_val);
			}
			SMJS_FREE(c, param_name);
			SMJS_FREE(c, _param_val);
			if (res != NPT_SUCCESS) return JS_FALSE;
			i+=2;
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, (" )\n"));
	}

	//notify we are ready
	if (device->m_pSema) {
		gf_sema_notify(device->m_pSema, 1);
	}
	return JS_TRUE;
}
#endif


NPT_Result
GPAC_GenericDevice::OnAction(PLT_ActionReference&          action,
                             const PLT_HttpRequestContext& context)
{
	NPT_COMPILER_UNUSED(context);

#ifdef GPAC_HAS_SPIDERMONKEY
	gf_mx_p(m_pMutex);
#endif
	PLT_ActionDesc &act_desc = action->GetActionDesc();
	NPT_String name = act_desc.GetName();
#ifdef GPAC_HAS_SPIDERMONKEY
	assert(!m_pSema);
#endif
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Action %s called (thread %d)\n", (char *) name, gf_th_id() ));

#ifdef GPAC_HAS_SPIDERMONKEY
	if (JSVAL_IS_NULL(act_proc)) {
		gf_mx_v(m_pMutex);
		return NPT_SUCCESS;
	}

	jsval argv[2];

	m_pUPnP->LockJavascript(GF_TRUE);

	JSObject *js_action = JS_NewObject(m_pUPnP->m_pJSCtx, &m_pUPnP->upnpDeviceClass._class, 0, 0);
	argv[0] = OBJECT_TO_JSVAL(js_action);
	SMJS_SET_PRIVATE(m_pUPnP->m_pJSCtx, js_action, this);

	act_ref = action;

	JS_DefineProperty(m_pUPnP->m_pJSCtx, js_action, "Name", STRING_TO_JSVAL( JS_NewStringCopyZ(m_pUPnP->m_pJSCtx, name) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	GPAC_Service *service = (GPAC_Service *) act_desc.GetService();
	JS_DefineProperty(m_pUPnP->m_pJSCtx, js_action, "Service", service->m_pObj ? OBJECT_TO_JSVAL( service->m_pObj) : JSVAL_NULL, 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunction(m_pUPnP->m_pJSCtx, js_action, "GetArgument", upnp_action_get_argument, 1, 0);
	JS_DefineFunction(m_pUPnP->m_pJSCtx, js_action, "SendReply", upnp_action_send_reply, 1, 0);

	/*create a semaphore*/
	m_pSema = gf_sema_new(1, 0);

	jsval rval;
	JS_CallFunctionValue(m_pUPnP->m_pJSCtx, obj, act_proc, 1, argv, &rval);
	SMJS_SET_PRIVATE(m_pUPnP->m_pJSCtx, js_action, NULL);
	m_pUPnP->LockJavascript(GF_FALSE);

	if (JSVAL_IS_INT(rval) && (JSVAL_TO_INT(rval) != 0)) {
		action->SetError(JSVAL_TO_INT(rval), "Action Failed");
	}
	/*wait on the semaphore*/
	if (!gf_sema_wait_for(m_pSema, 10000)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[UPnP] Reply processing to action %s timeout - sending incomplete reply)\n", (char *) name));
	}
	gf_sema_del(m_pSema);
	m_pSema = NULL;

	gf_mx_v(m_pMutex);
#endif
	return NPT_SUCCESS;
}
