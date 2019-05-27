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


#ifndef _GPAC_GENERIC_DEVICE_H_
#define _GPAC_GENERIC_DEVICE_H_

#include "Neptune.h"
#include "PltCtrlPoint.h"
#include "PltDeviceHost.h"
#include "PltService.h"

#include <gpac/thread.h>
#include <gpac/list.h>

#ifdef GPAC_HAS_SPIDERMONKEY
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/internal/smjs_api.h>
#endif

class GF_UPnP;
class GPAC_ServiceItem;

class GPAC_DeviceItem
{
public:
	GPAC_DeviceItem(PLT_DeviceDataReference device, NPT_String uuid);
	~GPAC_DeviceItem();

	PLT_DeviceDataReference m_device;
	NPT_String m_UUID;
	GF_UPnP *m_pUPnP;

	GPAC_ServiceItem *FindService(const char *type);
	void RefreshServiceList();

	GF_List *m_Services;
#ifdef GPAC_HAS_SPIDERMONKEY
	void DetachJS();
	JSObject *obj;
	JSContext *js_ctx;
#endif
};

#ifdef GPAC_HAS_SPIDERMONKEY
class GPAC_StateVariableListener
{
public:
	GPAC_StateVariableListener() {
		on_event = JSVAL_NULL;
		name = "";
		var = NULL;
	}
	NPT_String name;
	jsval on_event;
	PLT_StateVariable *var;
};


class GPAC_ActionArgListener
{
public:
	GPAC_ActionArgListener() {
		on_event = JSVAL_NULL;
		arg = NULL;
		action = NULL;
	}
	jsval on_event;
	PLT_ActionDesc *action;
	PLT_ArgumentDesc *arg;
	Bool is_script;
};

class GPAC_ActionUDTA
{
public:
	GPAC_ActionUDTA() {
		m_Reserved = NULL;
		udta = JSVAL_NULL;
	}
	/*members*/

	/*this is used to differentiate browse request from JS (using BrowseDataReference)
	as used in GPAC_MediaController and actionResponse used by the generic controller*/
	void *m_Reserved;
	jsval udta;
};

#endif


class GPAC_ServiceItem
{
public:
	GPAC_ServiceItem(GPAC_DeviceItem *device, PLT_Service *service);
	~GPAC_ServiceItem();


	GPAC_DeviceItem *m_device;
	PLT_Service *m_service;

	NPT_List<PLT_StateVariable*>*vars;

#ifdef GPAC_HAS_SPIDERMONKEY
	void DetachJS();
	JSObject *obj;
	jsval on_event;
	JSContext *js_ctx;
	GF_List *m_StateListeners;
	GF_List *m_ArgListeners;
	Bool subscribed;
#endif

};


class GPAC_GenericController :  public PLT_CtrlPointListener
{
public:
	GPAC_GenericController(PLT_CtrlPointReference& ctrlPoint, GF_UPnP *upnp);
	~GPAC_GenericController();

	/*any device control point*/
	virtual NPT_Result OnDeviceAdded(PLT_DeviceDataReference& device);
	virtual NPT_Result OnDeviceRemoved(PLT_DeviceDataReference& device);
	virtual NPT_Result OnActionResponse(NPT_Result res, PLT_ActionReference& action, void* userdata);
	virtual NPT_Result OnEventNotify(PLT_Service* service, NPT_List<PLT_StateVariable*>* vars);

	GF_List *m_Devices;
	GF_UPnP *m_pUPnP;
	PLT_CtrlPointReference m_CtrlPoint;
	GF_Mutex *m_ControlPointLock;
};


class GPAC_GenericDevice : public PLT_DeviceHost
{
public:
	GPAC_GenericDevice(const char* FriendlyName, const char *device_id);
	virtual ~GPAC_GenericDevice();

protected:
	// PLT_DeviceHost methods
	virtual NPT_Result SetupServices();
	virtual NPT_Result OnAction(PLT_ActionReference&          action,
	                            const PLT_HttpRequestContext& context);
public:
	GF_UPnP *m_pUPnP;
	GF_List *m_pServices;

#ifdef GPAC_HAS_SPIDERMONKEY
	void DetachJS(JSContext *c);
	jsval run_proc;
	jsval act_proc;
	JSObject *obj;
	NPT_String js_source;
	PLT_ActionReference act_ref;
	GF_Semaphore *m_pSema;
	GF_Mutex *m_pMutex;
#endif
};

class GPAC_Service : public PLT_Service
{
public:
	GPAC_Service(PLT_DeviceData* device, const char*     type = NULL,  const char*     id = NULL, const char*     name = NULL, const char*     last_change_namespace = NULL);
	~GPAC_Service();

#ifdef GPAC_HAS_SPIDERMONKEY
	void SetupJS(JSContext *c, GF_UPnP *upnp, JSObject *parent);
	JSObject *m_pObj;
	JSContext *m_pCtx;
	jsval on_action;
#endif

};



#endif //_GPAC_GENERIC_DEVICE_H_
