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

//declaration of export functions is done first due to a linker bug on OSX ...
#include <gpac/modules/term_ext.h>

GF_TermExt *upnp_new();
void upnp_delete(GF_BaseInterface *ifce);

#ifdef __cplusplus
extern "C" {
#endif


GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
		GF_TERM_EXT_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_TERM_EXT_INTERFACE) return (GF_BaseInterface *)upnp_new();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_TERM_EXT_INTERFACE:
		upnp_delete(ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( platinum )

#ifdef __cplusplus
}
#endif


#include "GPACPlatinum.h"

#ifdef GPAC_HAS_SPIDERMONKEY

#if !defined(__GNUC__)
# if defined(_WIN32_WCE)
#  pragma comment(lib, "js32")
# elif defined (_WIN64)
#  pragma comment(lib, "js")
# elif defined (WIN32)
#  pragma comment(lib, "js")
# endif
#endif

#endif

GF_UPnP::GF_UPnP()
{
	m_pTerm = NULL;
	m_pPlatinum = NULL;
	m_pMediaRenderer = NULL;
	m_pMediaServer = NULL;
	m_pAVCtrlPoint = NULL;
	m_renderer_bound = GF_FALSE;
	m_pGenericController = NULL;

#ifdef GPAC_HAS_SPIDERMONKEY
	m_Devices = NULL;
	m_pJSCtx = NULL;
	m_nbJSInstances=0;
	last_time = 0;
#endif
}

GF_UPnP::~GF_UPnP()
{
#ifdef GPAC_HAS_SPIDERMONKEY
	if (m_Devices) gf_list_del(m_Devices);
#endif
}

#ifdef GPAC_HAS_SPIDERMONKEY
void GF_UPnP::LockJavascript(Bool do_lock)
{
	gf_sg_lock_javascript(m_pJSCtx, do_lock);
}
#endif

void GF_UPnP::OnStop(const char *src_url)
{
	if (m_renderer_bound) {
#ifdef GPAC_HAS_SPIDERMONKEY
		jsval funval, rval;
		if (!m_pJSCtx) return;
		LockJavascript(GF_TRUE);
		JS_LookupProperty(m_pJSCtx, m_pObj, "onMediaStop", &funval);
		if (JSVAL_IS_OBJECT(funval)) {
			jsval argv[1];
			argv[0] = GetUPnPDevice(src_url);
			JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 1, argv, &rval);
		}
		LockJavascript(GF_FALSE);
#endif
	} else {
//		gf_term_disconnect(m_pTerm);
		gf_term_play_from_time(m_pTerm, 0, 1);
	}
}

NPT_String GF_UPnP::OnMigrate()
{
	NPT_String res = "";
	if (m_renderer_bound) {
#ifdef GPAC_HAS_SPIDERMONKEY
		jsval funval, rval;
		if (!m_pJSCtx) return res;
		LockJavascript(GF_TRUE);
		JS_LookupProperty(m_pJSCtx, m_pObj, "onMigrate", &funval);
		if (JSVAL_IS_OBJECT(funval)) {
			JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 0, NULL, &rval);
			if (JSVAL_IS_STRING(rval)) {
				char *_res = SMJS_CHARS(m_pJSCtx, rval);
				res = _res;
				SMJS_FREE(m_pJSCtx, _res);
			}
		}
		LockJavascript(GF_FALSE);
#endif
	} else {
		GF_NetworkCommand com;

		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_SERVICE_MIGRATION_INFO;
		m_pTerm->root_scene->root_od->net_service->ifce->ServiceCommand(m_pTerm->root_scene->root_od->net_service->ifce, &com);
		if (com.migrate.data) {
			res = com.migrate.data;
		} else {
			res = m_pTerm->root_scene->root_od->net_service->url;
		}
	}
	return res;
}

#ifdef GPAC_HAS_SPIDERMONKEY
jsval GF_UPnP::GetUPnPDevice(const char *src_url)
{
	return src_url ? STRING_TO_JSVAL( JS_NewStringCopyZ(m_pJSCtx, src_url ) ) : JSVAL_NULL;
}
#endif

void GF_UPnP::OnConnect(const char *url, const char *src_url)
{
	if (m_renderer_bound) {
#ifdef GPAC_HAS_SPIDERMONKEY
		jsval funval, rval;
		if (!m_pJSCtx) return;

		LockJavascript(GF_TRUE);
		JS_LookupProperty(m_pJSCtx, m_pObj, "onMediaConnect", &funval);
		if (JSVAL_IS_OBJECT(funval)) {
			jsval argv[2];
			argv[0] = STRING_TO_JSVAL( JS_NewStringCopyZ(m_pJSCtx, url ) );
			argv[1] = GetUPnPDevice(src_url);
			JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 2, argv, &rval);
		}
		LockJavascript(GF_FALSE);
#endif
	} else {
		gf_term_navigate_to(m_pTerm, url);
	}
}
void GF_UPnP::OnPause(Bool do_resume, const char *src_url)
{
	if (m_renderer_bound) {
#ifdef GPAC_HAS_SPIDERMONKEY
		jsval funval, rval;
		if (!m_pJSCtx) return;
		LockJavascript(GF_TRUE);
		JS_LookupProperty(m_pJSCtx, m_pObj, do_resume ? "onMediaPlay" : "onMediaPause", &funval);
		if (JSVAL_IS_OBJECT(funval)) {
			jsval argv[1];
			argv[0] = GetUPnPDevice(src_url);
			JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 1, argv, &rval);
		}
		LockJavascript(GF_FALSE);
#endif
	} else {
		gf_term_set_option(m_pTerm, GF_OPT_PLAY_STATE, do_resume ? GF_STATE_PLAYING : GF_STATE_PAUSED);
	}
}

void GF_UPnP::OnSeek(Double time)
{
	if (m_renderer_bound) {
#ifdef GPAC_HAS_SPIDERMONKEY
		jsval funval, rval;
		if (!m_pJSCtx) return;
		LockJavascript(GF_TRUE);
		JS_LookupProperty(m_pJSCtx, m_pObj, "onMediaSeek", &funval);
		if (JSVAL_IS_OBJECT(funval)) {
			jsval argv[1];
			argv[0] = DOUBLE_TO_JSVAL( JS_NewDouble(m_pJSCtx, time) );
			JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 2, argv, &rval);
		}
		LockJavascript(GF_FALSE);
#endif
	} else {
		/* CanSeek and Duration set for each media by event_proc */
		if (!m_pTerm->root_scene || (m_pTerm->root_scene->root_od->flags & GF_ODM_NO_TIME_CTRL)
		        || (m_pTerm->root_scene->duration<2000)
		   ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[UPnP] Scene not seekable\n"));
		} else {
			gf_term_play_from_time(m_pTerm, (u64) (time * 1000), 0);
		}
	}
}

void GF_UPnP::ContainerChanged(PLT_DeviceDataReference& device, const char *item_id, const char *update_id)
{
}

void GF_UPnP::onTimeChanged(s32 renderer_idx, Double time)
{
	if (m_renderer_bound) {
#ifdef GPAC_HAS_SPIDERMONKEY
		jsval funval, rval;
		if (!m_pJSCtx) return;
		LockJavascript(GF_TRUE);
		JS_LookupProperty(m_pJSCtx, m_pObj, "onMediaTimeChanged", &funval);
		if (JSVAL_IS_OBJECT(funval)) {
			jsval argv[2];
			argv[0] = INT_TO_JSVAL( renderer_idx);
			argv[1] = DOUBLE_TO_JSVAL( JS_NewDouble(m_pJSCtx, time) );
			JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 2, argv, &rval);
		}
		LockJavascript(GF_FALSE);
#endif
	}
}

void GF_UPnP::onDurationChanged(s32 renderer_idx, Double dur)
{
	if (m_renderer_bound) {
#ifdef GPAC_HAS_SPIDERMONKEY
		jsval funval, rval;
		if (!m_pJSCtx) return;
		LockJavascript(GF_TRUE);
		JS_LookupProperty(m_pJSCtx, m_pObj, "onMediaDurationChanged", &funval);
		if (JSVAL_IS_OBJECT(funval)) {
			jsval argv[2];
			argv[0] = INT_TO_JSVAL( renderer_idx);
			argv[1] = DOUBLE_TO_JSVAL( JS_NewDouble(m_pJSCtx, dur) );
			JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 2, argv, &rval);
		}
		LockJavascript(GF_FALSE);
#endif
	}
}


Bool GF_UPnP::ProcessEvent(GF_Event *evt)
{
	if (!m_pMediaRenderer) return GF_FALSE;
	switch (evt->type) {
	case GF_EVENT_CONNECT:
		if (evt->connect.is_connected) {
			const char *url = gf_term_get_url(m_pTerm);
			if (url) {
				m_pMediaRenderer->SetConnected(url);
			}
		}
		break;

	case GF_EVENT_DURATION:
		m_pMediaRenderer->SetDuration(evt->duration.duration, evt->duration.can_seek);
	case GF_EVENT_METADATA:
		if (m_pTerm->root_scene) {
			char szName[1024];
			NetInfoCommand com;
			memset(&com, 0, sizeof(NetInfoCommand));

			/*get any service info*/
			if (gf_term_get_service_info(m_pTerm, m_pTerm->root_scene->root_od, &com) == GF_OK) {
				strcpy(szName, "");
				if (com.name) {
					strcat(szName, com.name);
					strcat(szName, " ");
				}
				if (com.album) {
					strcat(szName, "(");
					strcat(szName, com.album);
					strcat(szName, ")");
				}

				/*const char *artist = "Unknown";
				if (com.artist) artist = com.artist;
				else if (com.writer) artist = com.writer;
				else if (com.composer) artist = com.composer;

				MRSetMediaInfo(0, szName, com.artist ? com.artist : "Unknown");*/
			}
		}
		break;
	}
	return GF_FALSE;
}

Bool upnp_on_term_event(void *udta, GF_Event *evt, Bool consumed)
{
	GF_UPnP *upnp = (GF_UPnP *) udta;
	if (!consumed && upnp) return upnp->ProcessEvent(evt);
	return GF_FALSE;
}

void GF_UPnP::Load(GF_Terminal *term)
{
	u16 port = 0;
	Bool save_uuids=GF_FALSE;
	Bool ignore_local_devices=GF_FALSE;
	const char *uuid, *opt, *name;
	char hostname[100], friendly_name[1024];
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Starting UPnP\n"));

	m_pCtrlPoint = NULL;
	m_pTerm = term;

	m_pPlatinum = new PLT_UPnP();
	m_pPlatinum->SetIgnoreLocalUUIDs(false);

	gf_sk_get_host_name((char*)hostname);

	opt = gf_opts_get_key("UPnP", "AllowedDevicesIP");
	if (!opt) {
		gf_cfg_set_key(m_pTerm->user->config, "UPnP", "AllowedDevicesIP", "");
		opt="";
	}
	m_IPFilter = opt;

	opt = gf_opts_get_key("UPnP", "IgnoreCreatedDevices");
	if (!opt || !strcmp(opt, "yes")) {
		ignore_local_devices = GF_TRUE;
		if (!opt) gf_cfg_set_key(m_pTerm->user->config, "UPnP", "IgnoreCreatedDevices", "yes");
	}



	opt = gf_opts_get_key("UPnP", "IgnoreCreatedDevices");
	if (!opt || !strcmp(opt, "yes")) {
		ignore_local_devices = GF_TRUE;
		if (!opt) gf_cfg_set_key(m_pTerm->user->config, "UPnP", "IgnoreCreatedDevices", "yes");
	}


	opt = gf_opts_get_key("UPnP", "SaveUUIDs");
	if (opt && !strcmp(opt, "yes")) save_uuids = GF_TRUE;

	opt = gf_opts_get_key("UPnP", "MediaRendererEnabled");
	if (!opt || !strcmp(opt, "yes")) {
		if (!opt) gf_cfg_set_key(m_pTerm->user->config, "UPnP", "MediaRendererEnabled", "yes");

		name = gf_opts_get_key("UPnP", "MediaRendererName");
		if (!name) {
			sprintf(friendly_name, "GPAC @ %s", hostname);
			name = friendly_name;
		}

		port = 0;
		opt = gf_opts_get_key("UPnP", "MediaRendererPort");
		if (opt) port = atoi(opt);

		uuid = gf_opts_get_key("UPnP", "MediaRendererUUID");
		if (uuid) {
			m_pMediaRenderer = new GPAC_MediaRenderer(this, name, false, uuid, port);
		} else {
			m_pMediaRenderer = new GPAC_MediaRenderer(this, name, false, NULL, port);
			if (save_uuids) {
				uuid = m_pMediaRenderer->GetUUID();
				gf_cfg_set_key(m_pTerm->user->config, "UPnP", "MediaRendererUUID", uuid);
			}
		}

		PLT_DeviceHostReference device(m_pMediaRenderer );
		device->m_ModelDescription = "GPAC Media Renderer";
		device->m_ModelURL = "https://gpac.io";
		device->m_ModelNumber = gf_gpac_version();
		device->m_ModelName = "GPAC Media Renderer";
		device->m_Manufacturer = "Telecom ParisTech";
		device->m_ManufacturerURL = "http://www.telecom-paristech.fr/";
		m_pPlatinum->AddDevice(device);
	}

	opt = gf_opts_get_key("UPnP", "MediaServerEnabled");
	if (!opt || !strcmp(opt, "yes")) {
		char *media_root;
		if (!opt) gf_cfg_set_key(m_pTerm->user->config, "UPnP", "MediaServerEnabled", "yes");

		name = gf_opts_get_key("UPnP", "MediaServerName");
		if (!name) {
			sprintf(friendly_name, "GPAC @ %s", hostname);
			name = friendly_name;
		}

		port = 0;
		opt = gf_opts_get_key("UPnP", "MediaServerPort");
		if (opt) port = atoi(opt);

		uuid = gf_opts_get_key("UPnP", "MediaServerUUID");
		if (uuid) {
			m_pMediaServer = new GPAC_FileMediaServer(name, false, uuid, port);
		} else {
			m_pMediaServer = new GPAC_FileMediaServer(name, false, NULL, port);
			if (save_uuids) {
				uuid = m_pMediaServer->GetUUID();
				gf_cfg_set_key(m_pTerm->user->config, "UPnP", "MediaServerUUID", uuid);
			}
		}
		media_root = (char *) gf_opts_get_key("UPnP", "MediaServerRoot");
		if (!media_root) {
			gf_cfg_set_key(m_pTerm->user->config, "UPnP", "MediaServerRoot", "all:/");
			m_pMediaServer->AddSharedDirectory("/", "all");
		} else {
			while (media_root) {
				Bool is_hidden = GF_FALSE;
				char *sep1 = (char *)strchr(media_root, ':');
				if (!sep1) break;
				char *sep2 = (char *)strchr(sep1, ';');

				if (!strncmp(media_root, "(h)", 3)) {
					media_root+=3;
					is_hidden = GF_TRUE;
				}
				sep1[0] = 0;
				if (sep2) sep2[0] = 0;
				m_pMediaServer->AddSharedDirectory(sep1+1, media_root, is_hidden);
				sep1[0] = ':';
				if (sep2) sep2[0] = ';';
				else break;
				media_root = sep2+1;
			}
		}
		PLT_DeviceHostReference device(m_pMediaServer);
		device->m_ModelDescription = "GPAC Media Server";
		device->m_ModelURL = "https://gpac.io";
		device->m_ModelNumber = gf_gpac_version();
		device->m_ModelName = "GPAC Media Server";
		device->m_Manufacturer = "Telecom ParisTech";
		device->m_ManufacturerURL = "http://www.telecom-paristech.fr/";
		m_pPlatinum->AddDevice(device);
	}

	opt = gf_opts_get_key("UPnP", "GenericControllerEnabled");
	if (!opt || !strcmp(opt, "yes")) {
		if (!opt) gf_cfg_set_key(m_pTerm->user->config, "UPnP", "GenericControllerEnabled", "yes");
		/*create our generic control point*/
		if (!m_pCtrlPoint) {
			m_pCtrlPoint = new PLT_CtrlPoint();
			m_ctrlPtRef = PLT_CtrlPointReference(m_pCtrlPoint);
		}
		m_pGenericController = new GPAC_GenericController(m_ctrlPtRef, this);
	}

	opt = gf_opts_get_key("UPnP", "AVCPEnabled");
	if (!opt || !strcmp(opt, "yes")) {
		if (!opt) gf_cfg_set_key(m_pTerm->user->config, "UPnP", "AVCPEnabled", "yes");

		if (!m_pCtrlPoint) {
			m_pCtrlPoint = new PLT_CtrlPoint();
			m_ctrlPtRef = PLT_CtrlPointReference(m_pCtrlPoint);
		}
		m_pAVCtrlPoint = new GPAC_MediaController(m_ctrlPtRef, this);
	}

	// add control point to upnp engine
	if (m_pCtrlPoint) {
		if (ignore_local_devices) {
			if (m_pMediaServer) m_pCtrlPoint->IgnoreUUID(m_pMediaServer->GetUUID());
			if (m_pMediaRenderer) m_pCtrlPoint->IgnoreUUID(m_pMediaRenderer->GetUUID());
		}
		m_pPlatinum->AddCtrlPoint(m_ctrlPtRef);
	}



	gf_term_add_event_filter(term, &evt_filter);

	//start UPnP engine
	m_pPlatinum->Start();

	/*if we have a control point, force a rescan of the network servcies*/
	if (m_pCtrlPoint) {
		m_pCtrlPoint->Search();
	}
}

void GF_UPnP::Unload()
{
	m_pPlatinum->Stop();
	if (m_pGenericController) delete m_pGenericController;
	if (m_pAVCtrlPoint) delete m_pAVCtrlPoint;
	/*this will delete all UPnP devices*/
	delete m_pPlatinum;

	/*final cleanup of UPnP lib*/
	NPT_AutomaticCleaner::Shutdown();
}


#ifdef GPAC_HAS_SPIDERMONKEY

void GF_UPnP::OnMediaRendererAdd(PLT_DeviceDataReference& device, int added)
{
	jsval funval, rval;
	if (!m_pJSCtx) return;

	if (m_IPFilter.GetLength() && (strstr((const char*)m_IPFilter, (const char*)device->GetURLBase().GetHost()) == NULL) ) return;

	LockJavascript(GF_TRUE);

	JS_LookupProperty(m_pJSCtx, m_pObj, "onMediaRendererAdd", &funval);
	if (JSVAL_IS_OBJECT(funval)) {
		jsval argv[3];
		argv[0] = STRING_TO_JSVAL( JS_NewStringCopyZ(m_pJSCtx, device->GetFriendlyName() ) );
		argv[1] = STRING_TO_JSVAL( JS_NewStringCopyZ(m_pJSCtx, device->GetUUID() ) );
		argv[2] = BOOLEAN_TO_JSVAL( added ? JS_TRUE : JS_FALSE);

		JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 3, argv, &rval);
	}
	LockJavascript(GF_FALSE);
}


void GF_UPnP::OnMediaServerAdd(PLT_DeviceDataReference& device, int added)
{
	jsval funval, rval;
	if (!m_pJSCtx) return;

	if (m_IPFilter.GetLength() && (strstr((const char*)m_IPFilter, (const char*)device->GetURLBase().GetHost()) == NULL) ) return;

	LockJavascript(GF_TRUE);
	JS_LookupProperty(m_pJSCtx, m_pObj, "onMediaServerAdd", &funval);
	if (JSVAL_IS_OBJECT(funval)) {
		jsval argv[3];
		argv[0] = STRING_TO_JSVAL( JS_NewStringCopyZ(m_pJSCtx, device->GetFriendlyName() ) );
		argv[1] = STRING_TO_JSVAL( JS_NewStringCopyZ(m_pJSCtx, device->GetUUID() ) );
		argv[2] = BOOLEAN_TO_JSVAL( added ? JS_TRUE : JS_FALSE);

		JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 3, argv, &rval);
	}
	LockJavascript(GF_FALSE);
}

static SMJS_DECL_FUNC_PROP_GET(  upnpdevice_getProperty)
{
#ifdef USE_FFDEV_15
	JSObject *obj = (JSObject *) __hobj;
	jsid id = (jsid) __hid;
#endif

	char *prop_name;
	GPAC_DeviceItem *dev = (GPAC_DeviceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!dev) return JS_FALSE;

	if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
	prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));
	if (!prop_name) return JS_FALSE;

	if (!strcmp(prop_name, "Name")) {
		VPASSIGN( STRING_TO_JSVAL( JS_NewStringCopyZ(c, dev->m_device->GetFriendlyName()) ) );
	}
	else if (!strcmp(prop_name, "UUID")) {
		VPASSIGN( STRING_TO_JSVAL( JS_NewStringCopyZ(c, dev->m_device->GetUUID()) ) );
	}
	else if (!strcmp(prop_name, "PresentationURL")) {
		VPASSIGN( STRING_TO_JSVAL( JS_NewStringCopyZ(c, dev->m_device->m_PresentationURL) )  );
	}
	else if (!strcmp(prop_name, "ServicesCount")) {
		u32 count = gf_list_count(dev->m_Services);
		if (!count) {
			dev->RefreshServiceList();
			count = gf_list_count(dev->m_Services);
		}
		VPASSIGN( INT_TO_JSVAL(count)  );
	}
	SMJS_FREE(c, prop_name);
	return JS_TRUE;
}

#ifdef GPAC_UNUSED_FUNC
static JSBool upnp_device_subscribe(JSContext *c, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	PLT_Service* service;
	char *service_uuid;
	GPAC_DeviceItem *item = (GPAC_DeviceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!item || (argc!=2) ) return JS_FALSE;

	if (!JSVAL_IS_STRING(argv[0])) return JS_FALSE;
	if (!JSVAL_IS_OBJECT(argv[1])) return JS_FALSE;

	service_uuid = SMJS_CHARS(c, argv[0]);
	if (item->m_device->FindServiceByType(service_uuid, service) == NPT_SUCCESS) {
		item->m_pUPnP->m_pGenericController->m_CtrlPoint->Subscribe(service);
	}
	SMJS_FREE(c, service_uuid);
	return JS_TRUE;
}
#endif

static JSBool SMJS_FUNCTION(upnp_device_find_service)
{
	char *service_uuid;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_DeviceItem *item = (GPAC_DeviceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!item || !argc) return JS_FALSE;
	service_uuid = SMJS_CHARS(c, argv[0]);

	GPAC_ServiceItem *serv = item->FindService(service_uuid);
	SMJS_FREE(c, service_uuid);
	if (!serv) {
		SMJS_SET_RVAL( JSVAL_NULL );
		return JS_TRUE;
	}
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(serv->obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_device_get_service)
{
	u32 service_index;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_DeviceItem *item = (GPAC_DeviceItem *)SMJS_GET_PRIVATE(c, obj);
	if (!item || !argc || !JSVAL_IS_INT(argv[0])) return JS_FALSE;
	service_index = JSVAL_TO_INT(argv[0]);

	GPAC_ServiceItem *serv = (GPAC_ServiceItem*)gf_list_get(item->m_Services, service_index);
	if (!serv) {
		SMJS_SET_RVAL( JSVAL_NULL );
		return JS_TRUE;
	}
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(serv->obj) );
	return JS_TRUE;
}


void GF_UPnP::OnDeviceAdd(GPAC_DeviceItem *item, int added)
{
	jsval funval, rval;
	if (!m_pJSCtx) return;

	if (m_IPFilter.GetLength() && (strstr((const char*)m_IPFilter, (const char*)item->m_device->GetURLBase().GetHost()) == NULL) ) return;

	LockJavascript(GF_TRUE);

	if (added) {
		item->js_ctx = m_pJSCtx;
		item->obj = JS_NewObject(m_pJSCtx, &upnpGenericDeviceClass._class, 0, 0);
		item->m_pUPnP = this;
		gf_js_add_root(m_pJSCtx, &item->obj, GF_JSGC_OBJECT);
		SMJS_SET_PRIVATE(item->js_ctx, item->obj, item);
	}

	JS_LookupProperty(m_pJSCtx, m_pObj, "onDeviceAdd", &funval);
	if (JSVAL_IS_OBJECT(funval)) {
		jsval argv[2];
		argv[0] = OBJECT_TO_JSVAL( item->obj );
		argv[1] = BOOLEAN_TO_JSVAL( added ? JS_TRUE : JS_FALSE);
		JS_CallFunctionValue(m_pJSCtx, m_pObj, funval, 3, argv, &rval);
	}
	LockJavascript(GF_FALSE);
}

static SMJS_DECL_FUNC_PROP_GET(  upnp_getProperty)
{
#ifdef USE_FFDEV_15
	JSObject *obj = (JSObject *) __hobj;
	jsid id = (jsid) __hid;
#endif


	char *prop_name;
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp) return JS_FALSE;

	if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
	prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));
	if (!prop_name) return JS_FALSE;

	if (!strcmp(prop_name, "MediaRendererEnabled")) {
		VPASSIGN( BOOLEAN_TO_JSVAL( upnp->m_pMediaRenderer ? JS_TRUE : JS_FALSE ) );
	}
	else if (!strcmp(prop_name, "MediaServerEnabled")) {
		VPASSIGN( BOOLEAN_TO_JSVAL( upnp->m_pMediaServer ? JS_TRUE : JS_FALSE)	);
	}
	else if (!strcmp(prop_name, "MediaControlEnabled")) {
		VPASSIGN( BOOLEAN_TO_JSVAL( upnp->m_pAVCtrlPoint ? JS_TRUE : JS_FALSE)	);
	}
	else if (!strcmp(prop_name, "MediaServersCount")) {
		VPASSIGN( INT_TO_JSVAL( upnp->m_pAVCtrlPoint ? gf_list_count(upnp->m_pAVCtrlPoint->m_MediaServers) : 0)	);
	}
	else if (!strcmp(prop_name, "MediaRenderersCount")) {
		VPASSIGN( INT_TO_JSVAL( upnp->m_pAVCtrlPoint ? gf_list_count(upnp->m_pAVCtrlPoint->m_MediaRenderers) : 0) );
	}
	else if (!strcmp(prop_name, "DevicesCount")) {
		VPASSIGN( INT_TO_JSVAL( upnp->m_pGenericController ? gf_list_count(upnp->m_pGenericController->m_Devices) : 0) );
	}
	SMJS_FREE(c, prop_name);
	return JS_TRUE;
}

static SMJS_DECL_FUNC_PROP_SET(upnp_setProperty)
{
#ifdef USE_FFDEV_15
	JSObject *obj = (JSObject *) __hobj;
	jsid id = (jsid) __hid;
#endif
	char *prop_name;
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp) return JS_FALSE;

	if (!SMJS_ID_IS_STRING(id)) return JS_TRUE;
	prop_name = SMJS_CHARS_FROM_STRING(c, SMJS_ID_TO_STRING(id));
	if (!prop_name) return JS_FALSE;

	if (upnp->m_pMediaRenderer ) {
		if (!strcmp(prop_name, "MovieDuration") && JSVAL_IS_DOUBLE( VPGET() )) {
			jsdouble d;
			JS_ValueToNumber(c, VPGET(), &d);
			upnp->m_pMediaRenderer->SetDuration(d, GF_TRUE);
		}
		else if (!strcmp(prop_name, "MovieTime") && JSVAL_IS_DOUBLE( VPGET() )) {
			jsdouble d;
			JS_ValueToNumber(c, VPGET(), &d);
			upnp->m_pMediaRenderer->SetTime(d);
		}
		else if (!strcmp(prop_name, "MovieURL") && JSVAL_IS_STRING( VPGET() ) ) {
			char *url = SMJS_CHARS(c, VPGET() );
			if (url) upnp->m_pMediaRenderer->SetConnected(url);
			SMJS_FREE(c, url);
		}
	}
	SMJS_FREE(c, prop_name);
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(upnp_get_device)
{
	u32 idx;
	GPAC_DeviceItem *device;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || !argc || !JSVAL_IS_INT(argv[0]) ) return JS_FALSE;

	idx = JSVAL_TO_INT(argv[0]);
	if (!upnp->m_pGenericController) return JS_FALSE;

	device = (GPAC_DeviceItem *)gf_list_get(upnp->m_pGenericController->m_Devices, idx);
	if (!device) return JS_FALSE;
	if (!device->obj) {
		device->js_ctx = upnp->m_pJSCtx;
		device->obj = JS_NewObject(upnp->m_pJSCtx, &upnp->upnpGenericDeviceClass._class, 0, 0);
		device->m_pUPnP = upnp;
		gf_js_add_root(upnp->m_pJSCtx, &device->obj, GF_JSGC_OBJECT);
		SMJS_SET_PRIVATE(device->js_ctx, device->obj, device);
	}
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(device->obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_find_service)
{
	char *dev_ip;
	char *serv_name;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || (argc!=2) || !JSVAL_IS_STRING(argv[0]) || !JSVAL_IS_STRING(argv[1]) ) return JS_FALSE;

	dev_ip = SMJS_CHARS(c, argv[0]);
	serv_name = SMJS_CHARS(c, argv[1]);
	SMJS_SET_RVAL(JSVAL_NULL);
	if (!dev_ip || !serv_name || !upnp->m_pGenericController) {
		SMJS_FREE(c, dev_ip);
		SMJS_FREE(c, serv_name);
		return JS_TRUE;
	}

	u32 i, count = gf_list_count(upnp->m_pGenericController->m_Devices);
	for (i=0; i<count; i++) {
		GPAC_DeviceItem *item = (GPAC_DeviceItem *) gf_list_get(upnp->m_pGenericController->m_Devices, i);
		if (item->m_device->GetURLBase().GetHost() == (const char *)dev_ip) {
			GPAC_ServiceItem *serv = item->FindService(serv_name);
			if (serv) {
				SMJS_SET_RVAL( OBJECT_TO_JSVAL(serv->obj) );
				break;
			}
		}
	}
	SMJS_FREE(c, dev_ip);
	SMJS_FREE(c, serv_name);
	return JS_TRUE;
}

static GPAC_MediaRendererItem *upnp_renderer_get_device(GF_UPnP *upnp , JSContext *c, JSObject *obj)
{
	char *uuid;
	jsval val;
	u32 i, count;
	GPAC_MediaRendererItem *render;
	if (!JS_LookupProperty(c, obj, "UUID", &val) || JSVAL_IS_NULL(val) || JSVAL_IS_VOID(val) )
		return NULL;
	uuid = SMJS_CHARS(c, val);
	if (!uuid) return NULL;

	count = gf_list_count(upnp->m_pAVCtrlPoint->m_MediaRenderers);
	for (i=0; i<count; i++) {
		render = (GPAC_MediaRendererItem *)gf_list_get(upnp->m_pAVCtrlPoint->m_MediaRenderers, i);
		if (render->m_UUID==(const char *)uuid) {
			SMJS_FREE(c, uuid);
			return render;
		}
	}
	SMJS_FREE(c, uuid);
	return NULL;
}

static GPAC_MediaServerItem *upnp_server_get_device(GF_UPnP *upnp , JSContext *c, JSObject *obj)
{
	char *uuid;
	jsval val;
	u32 i, count;
	GPAC_MediaServerItem *server;
	if (!JS_LookupProperty(c, obj, "UUID", &val) || JSVAL_IS_NULL(val) || JSVAL_IS_VOID(val) )
		return NULL;
	uuid = SMJS_CHARS(c, val);
	if (!uuid) return NULL;

	count = gf_list_count(upnp->m_pAVCtrlPoint->m_MediaServers);
	for (i=0; i<count; i++) {
		server = (GPAC_MediaServerItem *)gf_list_get(upnp->m_pAVCtrlPoint->m_MediaServers, i);
		if (server->m_UUID==(const char *)uuid) {
			SMJS_FREE(c, uuid);
			return server;
		}
	}
	SMJS_FREE(c, uuid);
	return NULL;
}


static JSBool SMJS_FUNCTION(upnp_renderer_open)
{
	JSObject *sobj, *fobj;
	jsval val;
	GPAC_MediaRendererItem *render;
	GPAC_MediaServerItem *server;
	char *item, *resource_url;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || (argc<1) ) return JS_FALSE;

	render = upnp_renderer_get_device(upnp, c, obj);
	if (!render) return JS_TRUE;

	PLT_Service* service;
	server = NULL;
	if (argc==2) {
		sobj = JSVAL_IS_NULL(argv[1]) ? NULL : JSVAL_TO_OBJECT(argv[1]);
		server = sobj ? upnp_server_get_device(upnp, c, sobj) : NULL;
		if (NPT_FAILED(server->m_device->FindServiceByType("urn:schemas-upnp-org:service:ContentDirectory:1", service))) {
			server = NULL;
		}
		if (!server) return JS_TRUE;
	}
	item = NULL;
	resource_url = NULL;
	if (JSVAL_IS_OBJECT(argv[0])) {
		fobj = JSVAL_TO_OBJECT(argv[0]);
		if (!JS_LookupProperty(c, fobj, "ObjectID", &val) || JSVAL_IS_NULL(val) || !JSVAL_IS_STRING(val)) return JS_TRUE;
		item = SMJS_CHARS(c, val);
	}
	else if (JSVAL_IS_STRING(argv[0]))
		resource_url = SMJS_CHARS(c, argv[0]);

	if (!item && !resource_url) {
		SMJS_FREE(c, item);
		SMJS_FREE(c, resource_url);
		return JS_TRUE;
	}
	if (item && !server) {
		SMJS_FREE(c, item);
		SMJS_FREE(c, resource_url);
		return JS_TRUE;
	}

	if (NPT_SUCCEEDED(render->m_device->FindServiceByType("urn:schemas-upnp-org:service:AVTransport:1", service))) {
		if (resource_url) {
			upnp->m_pAVCtrlPoint->m_MediaController->SetAVTransportURI(render->m_device, 0, resource_url, NULL, NULL);
			upnp->m_pAVCtrlPoint->m_MediaController->Play(render->m_device, 0, "1", NULL);
		} else {
			NPT_String objID = item;

			// look back for the PLT_MediaItem in the results
			NPT_List<PLT_MediaObject*>::Iterator item = server->m_BrowseResults->GetFirstItem();
			while (item) {
				if ((*item)->m_ObjectID == objID) {
					if ((*item)->m_Resources.GetItemCount()) {
						upnp->m_pAVCtrlPoint->m_MediaController->SetAVTransportURI(render->m_device, 0, (*item)->m_Resources[0].m_Uri, (*item)->m_Didl, NULL);
						upnp->m_pAVCtrlPoint->m_MediaController->Play(render->m_device, 0, "1", NULL);
					}
					break;
				}
				++item;
			}
		}
	}
	SMJS_FREE(c, item);
	SMJS_FREE(c, resource_url);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION_EXT(upnp_renderer_playback, u32 act_type)
{
	char szSpeed[20];
	GPAC_MediaRendererItem *render;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp) return JS_FALSE;

	render = upnp_renderer_get_device(upnp, c, obj);
	if (!render) return JS_TRUE;

	switch (act_type) {
	/*play/setspeed*/
	case 0:
		strcpy(szSpeed, "1");
		if (argc && JSVAL_IS_NUMBER(argv[0]) ) {
			jsdouble d;
			JS_ValueToNumber(c, argv[0], &d);
			sprintf(szSpeed, "%2.2f", d);
		}
		upnp->m_pAVCtrlPoint->m_MediaController->Play(render->m_device, 0, szSpeed, NULL);
		break;
	/*pause*/
	case 1:
		upnp->m_pAVCtrlPoint->m_MediaController->Pause(render->m_device, 0, NULL);
		break;
	/*stop*/
	case 2:
		upnp->m_pAVCtrlPoint->m_MediaController->Stop(render->m_device, 0, NULL);
		break;
	/*seek*/
	case 3:
		if (argc && JSVAL_IS_NUMBER(argv[0]) ) {
			char szVal[100];
			jsdouble d;
			JS_ValueToNumber(c, argv[0], &d);
			format_time_string(szVal, d);
			upnp->m_pAVCtrlPoint->m_MediaController->Seek(render->m_device, 0, "ABS_TIME", szVal, NULL);
		}
		break;
	}
	return JS_TRUE;
}
static JSBool SMJS_FUNCTION(upnp_renderer_play)
{
	return upnp_renderer_playback(SMJS_CALL_ARGS, 0);
}
static JSBool SMJS_FUNCTION(upnp_renderer_pause)
{
	return upnp_renderer_playback(SMJS_CALL_ARGS, 1);
}
static JSBool SMJS_FUNCTION(upnp_renderer_stop)
{
	return upnp_renderer_playback(SMJS_CALL_ARGS, 2);
}
static JSBool SMJS_FUNCTION(upnp_renderer_seek)
{
	return upnp_renderer_playback(SMJS_CALL_ARGS, 3);
}

static JSBool SMJS_FUNCTION(upnp_get_renderer)
{
	JSObject *s_obj;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || !upnp->m_pAVCtrlPoint || (argc!=1) ) return JS_FALSE;

	GPAC_MediaRendererItem *mr = NULL;
	if (JSVAL_IS_INT(argv[0])) {
		u32 id = JSVAL_TO_INT(argv[0]);
		mr = (GPAC_MediaRendererItem *) gf_list_get(upnp->m_pAVCtrlPoint->m_MediaRenderers, id);
	}
	else if (JSVAL_IS_STRING(argv[0])) {
		u32 i=0;
		char *uuid = SMJS_CHARS(c, argv[0]);
		while ((mr = (GPAC_MediaRendererItem *) gf_list_enum(upnp->m_pAVCtrlPoint->m_MediaRenderers, &i))) {
			if (mr->m_UUID==(const char *)uuid) break;
		}
		SMJS_FREE(c, uuid);
	}
	if (!mr) return JS_FALSE;

	s_obj = JS_NewObject(c, &upnp->upnpDeviceClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, s_obj, upnp);

	JS_DefineProperty(c, s_obj, "Name", STRING_TO_JSVAL( JS_NewStringCopyZ(c, mr->m_device->GetFriendlyName()) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, s_obj, "UUID", STRING_TO_JSVAL( JS_NewStringCopyZ(c, mr->m_UUID ) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, s_obj, "HostName", STRING_TO_JSVAL( JS_NewStringCopyZ(c, mr->m_device->GetURLBase().GetHost() ) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunction(c, s_obj, "Open", upnp_renderer_open, 2, 0);
	JS_DefineFunction(c, s_obj, "Play", upnp_renderer_play, 1, 0);
	JS_DefineFunction(c, s_obj, "Pause", upnp_renderer_pause, 0, 0);
	JS_DefineFunction(c, s_obj, "Stop", upnp_renderer_stop, 0, 0);
	JS_DefineFunction(c, s_obj, "Seek", upnp_renderer_seek, 0, 0);

	SMJS_SET_RVAL( OBJECT_TO_JSVAL(s_obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_server_browse)
{
	NPT_String parent;
	GPAC_MediaServerItem *server;
	char *dir, *filter, *_dir, *_filter;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || (argc!=2) ) return JS_FALSE;

	server = upnp_server_get_device(upnp, c, obj);
	if (!server) return JS_FALSE;

	_dir = _filter = NULL;

	dir = _dir = SMJS_CHARS(c, argv[0]);
	if (!dir) dir = (char*)"0";
	filter = _filter = SMJS_CHARS(c, argv[1]);
	if (!filter) filter = (char*)"*";

	PLT_Service* service;
	if (NPT_SUCCEEDED(server->m_device->FindServiceByType("urn:schemas-upnp-org:service:ContentDirectory:1", service))) {
		if (!strcmp(dir, "0") || !strcmp(dir, "\\") || !strcmp(dir, "/")) {
			server->m_ParentDirectories.Clear();
		}
		if (!strcmp(dir, "..")) {
			if (!server->m_ParentDirectories.GetItemCount()) {
				SMJS_FREE(c, _dir);
				SMJS_FREE(c, _filter);
				return JS_FALSE;
			}
			server->m_ParentDirectories.Pop(parent);
			server->m_ParentDirectories.Peek(parent);
			dir=parent;

			if (server->m_ParentDirectories.GetItemCount()==1)
				server->m_ParentDirectories.Clear();

		} else {
			server->m_ParentDirectories.Push(dir);
		}
		upnp->m_pAVCtrlPoint->Browse(server, dir, filter);


		jsval aval = INT_TO_JSVAL(0);
		if (!server->m_BrowseResults.IsNull()) {
			aval = INT_TO_JSVAL(server->m_BrowseResults->GetItemCount());
		}
		JS_SetProperty(c, obj, "FilesCount", &aval);
	}
	SMJS_FREE(c, _dir);
	SMJS_FREE(c, _filter);
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(upnp_server_has_parent_dir)
{
	GPAC_MediaServerItem *server;
	SMJS_OBJ
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp) return JS_FALSE;

	server = upnp_server_get_device(upnp, c, obj);
	if (!server) return JS_TRUE;
	SMJS_SET_RVAL( BOOLEAN_TO_JSVAL( server->m_ParentDirectories.GetItemCount() ? JS_TRUE : JS_FALSE));
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_server_get_resource_uri)
{
	u32 idx;
	SMJS_OBJ
	SMJS_ARGS
	PLT_MediaObject *mo = (PLT_MediaObject *)SMJS_GET_PRIVATE(c, obj);
	if (!mo || (argc!=1) || !JSVAL_IS_INT(argv[0]) ) return JS_FALSE;
	idx = JSVAL_TO_INT(argv[0]);
	if (idx<mo->m_Resources.GetItemCount()) {
		SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, mo->m_Resources[idx].m_Uri)));
	} else {
		SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, "")));
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_server_get_file)
{
	GPAC_MediaServerItem *server;
	u32 id;
	JSObject *f_obj;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || (argc!=1) || !JSVAL_IS_INT(argv[0]) ) return JS_FALSE;

	server = upnp_server_get_device(upnp, c, obj);
	if (!server) return JS_TRUE;
	if (server->m_BrowseResults.IsNull()) return JS_TRUE;

	id = JSVAL_TO_INT(argv[0]);
	PLT_MediaObject *mo;
	server->m_BrowseResults->Get(id, mo);
	if (!mo) return JS_TRUE;

	f_obj = JS_NewObject(c, &upnp->upnpDeviceClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, f_obj, mo);

	JS_DefineProperty(c, f_obj, "ObjectID", STRING_TO_JSVAL( JS_NewStringCopyZ(c, mo->m_ObjectID)), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, f_obj, "Name", STRING_TO_JSVAL( JS_NewStringCopyZ(c, mo->m_Title)), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, f_obj, "ParentID", STRING_TO_JSVAL( JS_NewStringCopyZ(c, mo->m_ParentID)), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, f_obj, "Directory", BOOLEAN_TO_JSVAL( mo->IsContainer() ? JS_TRUE : JS_FALSE), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	if (!mo->IsContainer()) {
		JS_DefineProperty(c, f_obj, "ResourceCount", INT_TO_JSVAL(mo->m_Resources.GetItemCount()), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
		JS_DefineFunction(c, f_obj, "GetResourceURI", upnp_server_get_resource_uri, 1, 0);
	}
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(f_obj));
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_server_get_file_uri)
{
	GPAC_MediaServerItem *server;
	u32 id;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || (argc!=1) || !JSVAL_IS_INT(argv[0]) ) return JS_FALSE;

	server = upnp_server_get_device(upnp, c, obj);
	if (!server) return JS_TRUE;
	if (server->m_BrowseResults.IsNull()) return JS_TRUE;

	id = JSVAL_TO_INT(argv[0]);
	PLT_MediaObject *mo;
	server->m_BrowseResults->Get(id, mo);
	if (!mo) return JS_TRUE;

	if (mo->m_Resources.GetItemCount()) {
		SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(c, mo->m_Resources[0].m_Uri) ) );
	}
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_get_server)
{
	JSObject *s_obj;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || !upnp->m_pAVCtrlPoint || (argc!=1)) return JS_FALSE;


	GPAC_MediaServerItem *ms = NULL;
	if (JSVAL_IS_INT(argv[0])) {
		u32 id = JSVAL_TO_INT(argv[0]);
		ms = (GPAC_MediaServerItem *) gf_list_get(upnp->m_pAVCtrlPoint->m_MediaServers, id);
	}
	else if (JSVAL_IS_STRING(argv[0])) {
		u32 i=0;
		char *uuid = SMJS_CHARS(c, argv[0]);
		while ((ms = (GPAC_MediaServerItem *) gf_list_enum(upnp->m_pAVCtrlPoint->m_MediaServers, &i))) {
			if (ms->m_UUID==(const char *)uuid) break;
		}
		SMJS_FREE(c, uuid);
	}
	if (!ms) return JS_FALSE;
	s_obj = JS_NewObject(c, &upnp->upnpDeviceClass._class, 0, 0);
	SMJS_SET_PRIVATE(c, s_obj, upnp);

	JS_DefineProperty(c, s_obj, "Name", STRING_TO_JSVAL( JS_NewStringCopyZ(c, ms->m_device->GetFriendlyName()) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, s_obj, "UUID", STRING_TO_JSVAL( JS_NewStringCopyZ(c, ms->m_UUID ) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(c, s_obj, "FilesCount", INT_TO_JSVAL(10), 0, 0, 0  | JSPROP_PERMANENT);
	JS_DefineFunction(c, s_obj, "Browse", upnp_server_browse, 2, 0);
	JS_DefineFunction(c, s_obj, "GetFile", upnp_server_get_file, 1, 0);
	JS_DefineFunction(c, s_obj, "GetFileURI", upnp_server_get_file_uri, 1, 0);
	JS_DefineFunction(c, s_obj, "HasParentDirectory", upnp_server_has_parent_dir, 0, 0);


	SMJS_SET_RVAL( OBJECT_TO_JSVAL(s_obj) );
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_bind_renderer)
{
	SMJS_OBJ
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp) return JS_TRUE;
	upnp->m_renderer_bound = GF_TRUE;

	/*remove ourselves from the event filters since we will only be called through JS*/
	gf_term_remove_event_filter(upnp->m_pTerm, &upnp->evt_filter);

	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_share_resource)
{
	char *url, *host;
	NPT_String resourceURI;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || !upnp->m_pMediaServer || !argc || !JSVAL_IS_STRING(argv[0]) ) return JS_TRUE;
	url = SMJS_CHARS(c, argv[0]);
	if (!url) return JS_TRUE;

	host = NULL;
	if (argc && JSVAL_IS_STRING(argv[1]) ) {
		host = SMJS_CHARS(c, argv[1]);
	}

	resourceURI = upnp->m_pMediaServer->GetResourceURI(url, host);
	SMJS_SET_RVAL( STRING_TO_JSVAL( JS_NewStringCopyZ(upnp->m_pJSCtx, resourceURI ) ));

	SMJS_FREE(c, url);
	SMJS_FREE(c, host);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_share_virtual_resource)
{
	Bool temp = GF_FALSE;
	char *res_url, *res_val, *mime;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || !upnp->m_pMediaServer || (argc<2) || !JSVAL_IS_STRING(argv[0]) || !JSVAL_IS_STRING(argv[1]) ) return JS_TRUE;
	res_url = SMJS_CHARS(c, argv[0]);
	if (!res_url) return JS_TRUE;
	res_val = SMJS_CHARS(c, argv[1]);
	if (!res_val) {
		SMJS_FREE(c, res_url);
		return JS_TRUE;
	}
	mime = NULL;
	if (argc==3) mime = SMJS_CHARS(c, argv[2]);
	if ((argc==4) && JSVAL_IS_BOOLEAN(argv[3]) && (JSVAL_TO_BOOLEAN(argv[3])==JS_TRUE) ) temp = GF_TRUE;

	upnp->m_pMediaServer->ShareVirtualResource(res_url, res_val, mime ? mime : "application/octet-stream", temp);
	SMJS_FREE(c, res_url);
	SMJS_FREE(c, res_val);
	SMJS_FREE(c, mime);
	return JS_TRUE;
}


static NPT_UInt8 GENERIC_SCPDXML[] = "<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\"><specVersion>  <major>1</major>   <minor>0</minor> </specVersion> <actionList>  <action>  <name>GetStatus</name>    <argumentList>    <argument>     <name>ResultStatus</name>     <direction>out</direction>      <relatedStateVariable>Status</relatedStateVariable>     </argument>   </argumentList>  </action> </actionList>  <serviceStateTable>  <stateVariable sendEvents=\"yes\">   <name>Status</name>    <dataType>boolean</dataType>   </stateVariable></serviceStateTable> </scpd>";


static JSBool SMJS_FUNCTION(upnp_device_setup_service)
{
	char *name, *type, *id, *scpd_xml;
	NPT_Result res;
	SMJS_OBJ
	SMJS_ARGS
	GPAC_GenericDevice *device = (GPAC_GenericDevice *)SMJS_GET_PRIVATE(c, obj);
	if (!device) return JS_FALSE;
	if (argc<3) return JS_FALSE;

	name = SMJS_CHARS(c, argv[0]);
	type = SMJS_CHARS(c, argv[1]);
	id = SMJS_CHARS(c, argv[2]);

	if (!name || !type || !id) {
		SMJS_FREE(c, name);
		SMJS_FREE(c, type);
		SMJS_FREE(c, id);
		return JS_FALSE;
	}

	scpd_xml = NULL;
	if ((argc>3) && JSVAL_IS_STRING(argv[3])) scpd_xml = SMJS_CHARS(c, argv[3]);

	GPAC_Service* service = new GPAC_Service(device, type, id, name);
	res = service->SetSCPDXML((const char*) scpd_xml ? scpd_xml : (char *)GENERIC_SCPDXML);

	SMJS_FREE(c, name);
	SMJS_FREE(c, type);
	SMJS_FREE(c, id);

	if (res != NPT_SUCCESS) {
		delete service;
		return JS_FALSE;
	}

	gf_list_add(device->m_pServices, service);

	service->SetupJS(c, device->m_pUPnP, device->obj);
	SMJS_SET_RVAL( OBJECT_TO_JSVAL(service->m_pObj) );
	return JS_TRUE;
}


static JSBool SMJS_FUNCTION(upnp_device_start)
{
	jsval sval;
	char *str;
	SMJS_OBJ
	GPAC_GenericDevice *device = (GPAC_GenericDevice *)SMJS_GET_PRIVATE(c, obj);
	if (!device) return JS_FALSE;

	if (JS_LookupProperty(device->m_pUPnP->m_pJSCtx, obj, "PresentationURL", &sval) && JSVAL_IS_STRING(sval)) {
		str = SMJS_CHARS(c, sval);
		char *url = gf_url_concatenate(device->js_source, str);
		SMJS_FREE(c, str);

		/*we will use our media server to exchange the URL if file based
			!!! THIS IS BROKEN IF MULTIPLE INTERFACES EXIST ON THE DEVICE !!!
		*/
		if (device->m_pUPnP->m_pMediaServer) {
			device->m_PresentationURL = device->m_pUPnP->m_pMediaServer->GetResourceURI(url, NULL);
		}
		/*otherwise we can only use absolute URLs */
		else if (strstr(url, "://") && !strstr(url, "file://")) {
			device->m_PresentationURL = url;
		}
		gf_free(url);
	}

	str = NULL;
	if (JS_LookupProperty(device->m_pUPnP->m_pJSCtx, obj, "ModelDescription", &sval) && JSVAL_IS_STRING(sval))
		str = SMJS_CHARS(c, sval);

	device->m_ModelDescription = str ? str : "GPAC Generic Device";
	SMJS_FREE(c, str);

	str = NULL;
	if (JS_LookupProperty(device->m_pUPnP->m_pJSCtx, obj, "ModelURL", &sval) && JSVAL_IS_STRING(sval))
		str = SMJS_CHARS(c, sval);
	device->m_ModelURL = str ? str : "https://gpac.io";
	SMJS_FREE(c, str);

	str = NULL;
	if (JS_LookupProperty(device->m_pUPnP->m_pJSCtx, obj, "ModelNumber", &sval) && JSVAL_IS_STRING(sval))
		str = SMJS_CHARS(c, sval);
	device->m_ModelNumber = str ? str : gf_gpac_version();
	SMJS_FREE(c, str);

	str = NULL;
	if (JS_LookupProperty(device->m_pUPnP->m_pJSCtx, obj, "ModelName", &sval) && JSVAL_IS_STRING(sval))
		str = SMJS_CHARS(c, sval);
	device->m_ModelName = str ? str : "GPAC Generic Device";
	SMJS_FREE(c, str);

	device->m_Manufacturer = "Telecom ParisTech";
	device->m_ManufacturerURL = "http://www.telecom-paristech.fr/";

	if (device->m_pUPnP->m_pGenericController) {
		const char *opt = gf_opts_get_key("UPnP", "IgnoreCreatedDevices");
		if (!opt || !strcmp(opt, "yes")) {
			device->m_pUPnP->m_pGenericController->m_CtrlPoint->IgnoreUUID(device->GetUUID());
		}
	}
	if (JS_LookupProperty(device->m_pUPnP->m_pJSCtx, obj, "Run", &sval) && JSVAL_IS_OBJECT(sval)) {
		device->obj = obj;
		device->run_proc = sval;
		gf_js_add_root(device->m_pUPnP->m_pJSCtx, &device->run_proc, GF_JSGC_VAL);
	}
	if (JS_LookupProperty(device->m_pUPnP->m_pJSCtx, obj, "OnAction", &sval) && JSVAL_IS_OBJECT(sval)) {
		device->obj = obj;
		device->act_proc = sval;
		gf_js_add_root(device->m_pUPnP->m_pJSCtx, &device->act_proc, GF_JSGC_VAL);
	}
	PLT_DeviceHostReference devRef(device);
	device->m_pUPnP->m_pPlatinum->AddDevice(devRef);

	return JS_TRUE;
}

#ifdef GPAC_UNUSED_FUNC
static JSBool SMJS_FUNCTION(upnp_device_stop)
{
	SMJS_OBJ
	GPAC_GenericDevice *device = (GPAC_GenericDevice *)SMJS_GET_PRIVATE(c, obj);
	if (!device) return JS_FALSE;

	PLT_DeviceHostReference devRef(device);
	device->m_pUPnP->m_pPlatinum->RemoveDevice(devRef);

	return JS_TRUE;
}
#endif

static GPAC_GenericDevice *upnp_create_generic_device(GF_UPnP *upnp, JSObject*global, const char *id, const char *name)
{
	GPAC_GenericDevice *device;
	device = new GPAC_GenericDevice(name, id);
	device->m_pUPnP = upnp;
	device->js_source = "";

	device->obj = JS_NewObject(upnp->m_pJSCtx, &upnp->upnpDeviceClass._class, 0, global);
	gf_js_add_root(upnp->m_pJSCtx, &device->obj, GF_JSGC_OBJECT);

	JS_DefineProperty(upnp->m_pJSCtx, device->obj, "Name", STRING_TO_JSVAL( JS_NewStringCopyZ(upnp->m_pJSCtx, name) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(upnp->m_pJSCtx, device->obj, "ID", STRING_TO_JSVAL( JS_NewStringCopyZ(upnp->m_pJSCtx, id) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineProperty(upnp->m_pJSCtx, device->obj, "UUID", STRING_TO_JSVAL( JS_NewStringCopyZ(upnp->m_pJSCtx, device->GetUUID()) ), 0, 0, JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunction(upnp->m_pJSCtx, device->obj, "SetupService", upnp_device_setup_service, 0, 0);
	JS_DefineFunction(upnp->m_pJSCtx, device->obj, "Start", upnp_device_start, 0, 0);
//	JS_DefineFunction(upnp->m_pJSCtx, device->obj, "Stop", upnp_device_stop, 0, 0);
	SMJS_SET_PRIVATE(upnp->m_pJSCtx, device->obj, device);
	if (!upnp->m_Devices) upnp->m_Devices = gf_list_new();
	gf_list_add(upnp->m_Devices, device);

	return device;
}

static JSBool SMJS_FUNCTION(upnp_create_device)
{
	GPAC_GenericDevice *device;
	char *id, *name;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || (argc != 2)) return JS_FALSE;

	id = SMJS_CHARS(c, argv[0]);
	name = SMJS_CHARS(c, argv[1]);
	if (!id || !name) {
		SMJS_FREE(c, name);
		SMJS_FREE(c, id);
		return JS_FALSE;
	}

	device = upnp_create_generic_device(upnp, NULL, id, name);
	if (device)
		SMJS_SET_RVAL( OBJECT_TO_JSVAL(device->obj) );

	SMJS_FREE(c, name);
	SMJS_FREE(c, id);
	return JS_TRUE;
}

static JSBool SMJS_FUNCTION(upnp_delete_device)
{
	GPAC_GenericDevice *device;
	SMJS_OBJ
	SMJS_ARGS
	GF_UPnP *upnp = (GF_UPnP *)SMJS_GET_PRIVATE(c, obj);
	if (!upnp || (argc != 1)) return JS_FALSE;

	device = (GPAC_GenericDevice *) SMJS_GET_PRIVATE(c, JSVAL_TO_OBJECT(argv[0]) );
	if (!device) return JS_FALSE;

	gf_list_del_item(upnp->m_Devices, device);
	device->DetachJS(c);
	PLT_DeviceHostReference devRef = PLT_DeviceHostReference(device);
	upnp->m_pPlatinum->RemoveDevice(devRef);
	devRef.Detach();
	return JS_TRUE;
}

Bool GF_UPnP::LoadJS(GF_TermExtJS *param)
{
	u32 i, count;
	JSPropertySpec upnpClassProps[] = {
		SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
	};
	JSFunctionSpec upnpClassFuncs[] = {
		SMJS_FUNCTION_SPEC("BindRenderer", upnp_bind_renderer, 0),
		SMJS_FUNCTION_SPEC("GetMediaServer", upnp_get_server, 1),
		SMJS_FUNCTION_SPEC("GetMediaRenderer", upnp_get_renderer, 1),
		SMJS_FUNCTION_SPEC("ShareResource", upnp_share_resource, 1),
		SMJS_FUNCTION_SPEC("ShareVirtualResource", upnp_share_virtual_resource, 2),
		SMJS_FUNCTION_SPEC("GetDevice", upnp_get_device, 1),
		SMJS_FUNCTION_SPEC("FindService", upnp_find_service, 1),
		SMJS_FUNCTION_SPEC("CreateDevice", upnp_create_device, 2),
		SMJS_FUNCTION_SPEC("DeleteDevice", upnp_delete_device, 1),
		SMJS_FUNCTION_SPEC(0, 0, 0)
	};

	if (param->unload) {
		if (m_nbJSInstances) {
			m_nbJSInstances--;
			if (m_pJSCtx==param->ctx) m_nbJSInstances = 0;

			if (!m_nbJSInstances) {
				if (m_pGenericController) {
					u32 i, count;
					count = gf_list_count(m_pGenericController->m_Devices);
					for (i=0; i<count; i++) {
						GPAC_DeviceItem *item = (GPAC_DeviceItem *)gf_list_get(m_pGenericController->m_Devices, i);
						item->DetachJS();
						item->js_ctx = NULL;
					}
				}
				if (m_Devices) {
					while (gf_list_count(m_Devices)) {
						GPAC_GenericDevice *device = (GPAC_GenericDevice*)gf_list_get(m_Devices, 0);
						gf_list_rem(m_Devices, 0);
						device->DetachJS(m_pJSCtx);
					}
					gf_list_del(m_Devices);
					m_Devices = NULL;
				}
				m_pJSCtx = NULL;
			}
		}
		return GF_FALSE;
	}
	if (m_nbJSInstances) {
		/*FIXME - this was possible in previous version of SpiderMonkey, don't know how to fix that for new ones*/
#if (JS_VERSION>=185)
		m_nbJSInstances++;
		return GF_FALSE;
#else
		JS_DefineProperty((JSContext*)param->ctx, (JSObject*)param->global, "UPnP", OBJECT_TO_JSVAL(m_pObj), 0, 0, 0);
		m_nbJSInstances++;
#endif
		return GF_FALSE;
	}

	m_pJSCtx = (JSContext*)param->ctx;
	/*setup JS bindings*/
	JS_SETUP_CLASS(upnpClass, "UPNPMANAGER", JSCLASS_HAS_PRIVATE, upnp_getProperty, upnp_setProperty, JS_FinalizeStub);

	GF_JS_InitClass(m_pJSCtx, (JSObject*)param->global, 0, &upnpClass, 0, 0, upnpClassProps, upnpClassFuncs, 0, 0);
	m_pObj = JS_DefineObject(m_pJSCtx, (JSObject*)param->global, "UPnP", &upnpClass._class, 0, 0);
	SMJS_SET_PRIVATE(m_pJSCtx, m_pObj, this);

	JS_SETUP_CLASS(upnpDeviceClass, "UPNPAVDEVICE", JSCLASS_HAS_PRIVATE, JS_PropertyStub, JS_PropertyStub_forSetter, JS_FinalizeStub);

	/*setup JS bindings*/
	JSPropertySpec upnpDeviceClassProps[] = {
		SMJS_PROPERTY_SPEC(0, 0, 0, 0, 0)
	};
	JSFunctionSpec upnpDeviceClassFuncs[] = {
		SMJS_FUNCTION_SPEC("FindService", upnp_device_find_service, 0),
		SMJS_FUNCTION_SPEC("GetService", upnp_device_get_service, 0),
		SMJS_FUNCTION_SPEC(0, 0, 0)
	};
	JS_SETUP_CLASS(upnpGenericDeviceClass, "UPNPDEVICE", JSCLASS_HAS_PRIVATE, upnpdevice_getProperty, JS_PropertyStub_forSetter, JS_FinalizeStub);
	GF_JS_InitClass(m_pJSCtx, (JSObject*)param->global, 0, &upnpGenericDeviceClass, 0, 0, upnpDeviceClassProps, upnpDeviceClassFuncs, 0, 0);

	JS_SETUP_CLASS(upnpServiceClass, "UPNPSERVICEDEVICE", JSCLASS_HAS_PRIVATE, upnpservice_getProperty, JS_PropertyStub_forSetter, JS_FinalizeStub);
	GF_JS_InitClass(m_pJSCtx, (JSObject*)param->global, 0, &upnpServiceClass, 0, 0, 0, 0, 0, 0);

	m_nbJSInstances=1;

	upnp_init_time = gf_sys_clock();

	count = gf_opts_get_key_count("UPnPDevices");
	for (i=0; i<count; i++) {
		char szFriendlyName[1024], szFile[1024], *sep;
		const char *device_id = gf_opts_get_key_name("UPnPDevices", i);
		const char *dev = gf_opts_get_key("UPnPDevices", device_id);

		if (!strncmp(dev, "off;", 4)) continue;

		if (!strncmp(dev, "on;", 3)) dev += 3;

		sep = (char*)strchr(dev, ';');
		if (!sep) continue;
		sep[0] = 0;
		strcpy(szFile, dev);
		sep[0] = ';';

		if (!sep[1]) continue;
		strcpy(szFriendlyName, sep+1);

		FILE *f = gf_fopen(szFile, "rt");
		if (!f) continue;


		GPAC_GenericDevice *device = upnp_create_generic_device(this, (JSObject*)param->global, device_id, szFriendlyName);
		device->js_source = szFile;

		jsval aval;
		gf_fseek(f, 0, SEEK_END);
		u32 size = (u32) gf_ftell(f);
		gf_fseek(f, 0, SEEK_SET);
		char *buf = (char*)gf_malloc(sizeof(char)*(size+1));
		size = (u32) fread(buf, 1, size, f);
		buf[size]=0;
		/*evaluate the script on the object only*/
		if (JS_EvaluateScript(m_pJSCtx, device->obj, buf, size, 0, 0, &aval) != JS_TRUE) {
			gf_js_remove_root(m_pJSCtx, &device->obj, GF_JSGC_OBJECT);
			GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[UPnP] Unable to load device %s: script error in %s\n", szFriendlyName, szFile));
			gf_list_del_item(m_Devices, device);
			delete device;
		}
		gf_fclose(f);
		gf_free(buf);
	}
	return GF_TRUE;
}

#endif

static Bool upnp_process(GF_TermExt *termext, u32 action, void *param)
{
	const char *opt;
	GF_UPnP *upnp = (GF_UPnP *) termext->udta;

	switch (action) {
	case GF_TERM_EXT_START:
		opt = gf_opts_get_key("UPnP", "Enabled");
		if (!opt) {
			//UPnP is disabled by default on all platforms until we have a more stable state on load and exit
			opt = "no";
			GF_LOG(GF_LOG_WARNING, GF_LOG_NETWORK, ("[UPnP] Disabling UPnP - to enable it, modify section [UPnP] key \"Enabled\" in GPAC config file\n"));
			gf_opts_set_key("UPnP", "Enabled", opt);
		}
		if (!strcmp(opt, "yes")) {
			upnp->Load((GF_Terminal *)param);
			return GF_TRUE;
		}
		return GF_FALSE;

	case GF_TERM_EXT_STOP:
		upnp->Unload();
		break;

	case GF_TERM_EXT_PROCESS:
#ifdef GPAC_HAS_SPIDERMONKEY
		if (upnp->m_Devices) {
			u32 now;
			now = gf_sys_clock() - upnp->upnp_init_time;
			if (now - upnp->last_time > 200) {
				u32 i, count, arg_set;
				jsval argv[1], rval;
				upnp->LockJavascript(GF_TRUE);
				arg_set = 0;
				count = gf_list_count(upnp->m_Devices);
				for (i=0; i<count; i++) {
					GPAC_GenericDevice *device = (GPAC_GenericDevice *)gf_list_get(upnp->m_Devices, i);
					if (!JSVAL_IS_NULL(device->run_proc)) {
						if (!arg_set) {
							argv[0] = DOUBLE_TO_JSVAL( JS_NewDouble(upnp->m_pJSCtx, (Double)now / 1000.0) );
							arg_set = 1;
						}
						JS_CallFunctionValue(upnp->m_pJSCtx, device->obj, device->run_proc, 1, argv, &rval);
					}
				}
				upnp->LockJavascript(GF_FALSE);
				upnp->last_time = now;
			}
		}
#endif
		break;

#ifdef GPAC_HAS_SPIDERMONKEY
	case GF_TERM_EXT_JSBIND:
		return upnp->LoadJS((GF_TermExtJS*)param);
#endif
	}
	return GF_FALSE;
}


GF_TermExt *upnp_new()
{
	GF_TermExt *dr;
	GF_UPnP *ext;
	dr = (GF_TermExt *) gf_malloc(sizeof(GF_TermExt));
	memset(dr, 0, sizeof(GF_TermExt));
	GF_REGISTER_MODULE_INTERFACE(dr, GF_TERM_EXT_INTERFACE, "GPAC UPnP Platinum", "gpac distribution");

	dr->process = upnp_process;
	ext = new GF_UPnP();
	dr->udta = ext;
	ext->term_ext = dr;
	ext->evt_filter.on_event = upnp_on_term_event;
	ext->evt_filter.udta = ext;

	dr->caps = GF_TERM_EXTENSION_NOT_THREADED;
#ifdef GPAC_HAS_SPIDERMONKEY
	dr->caps |= GF_TERM_EXTENSION_JS;
#endif
	return dr;
}


void upnp_delete(GF_BaseInterface *ifce)
{
	GF_TermExt *dr = (GF_TermExt *) ifce;
	GF_UPnP *ext = (GF_UPnP *) dr->udta;
	delete ext;
	gf_free(dr);
}

