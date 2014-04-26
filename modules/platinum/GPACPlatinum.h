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


#ifndef _GPAC_PLATINUM_H_
#define _GPAC_PLATINUM_H_

#include "Platinum.h"
#include "PltUPnP.h"
#include "GPACFileMediaServer.h"
#include "GPACMediaRenderer.h"
#include "GPACMediaController.h"
#include "GenericDevice.h"

#include <gpac/modules/term_ext.h>
#include <gpac/term_info.h>
#include <gpac/thread.h>
#include <gpac/network.h>
#include <gpac/options.h>
#include <gpac/internal/terminal_dev.h>

#ifdef GPAC_HAS_SPIDERMONKEY

#include <gpac/internal/scenegraph_dev.h>
#include <gpac/internal/smjs_api.h>

#endif

class GPAC_DeviceItem;


class GF_UPnP
{
public:
	GF_UPnP();
	~GF_UPnP();

	/*load UPnP services*/
	void Load(GF_Terminal *term);
	/*unload UPnP services*/
	void Unload();

	/*GPAC event filter*/
	Bool ProcessEvent(GF_Event *evt);

	GF_TermExt *term_ext;
	/*GPAC's terminal*/
	GF_Terminal *m_pTerm;

	/*Platinum's UPnP stack*/
	PLT_UPnP *m_pPlatinum;

	/*GPAC UPnP/DLNA media renderer if loaded*/
	GPAC_MediaRenderer *m_pMediaRenderer;
	/*is renderer bound to the script ? If so, events are dispatched to the script's "UPnP" object*/
	Bool m_renderer_bound;
	NPT_String m_IPFilter;

	/*regular media file server from platinum*/
	GPAC_FileMediaServer *m_pMediaServer;

	/*GPAC's AVControlPoint*/
	GPAC_MediaController *m_pAVCtrlPoint;

	void LockJavascript(Bool do_lock);

	/*callback from GPAC MediaRenderer*/
	void OnConnect(const char *url, const char *src_url);
	void OnPause(Bool do_resume, const char *src_url);
	void OnStop(const char *src_url);
	void OnSeek(Double time);
	void OnSetPlayMode(const char *src_url);
	void onDurationChanged(s32 renderer_idx, Double dur);
	void onTimeChanged(s32 renderer_idx, Double time);
	void ContainerChanged(PLT_DeviceDataReference& device, const char *item_id, const char *update_id);
	NPT_String OnMigrate();

	GPAC_GenericController *m_pGenericController;

	PLT_CtrlPoint *m_pCtrlPoint;
	PLT_CtrlPointReference m_ctrlPtRef;

	GF_TermEventFilter evt_filter;
	/*JS bindings*/
#ifdef GPAC_HAS_SPIDERMONKEY
	Bool LoadJS(GF_TermExtJS *param);
	u32 m_nbJSInstances;
	JSContext *m_pJSCtx;
	JSObject *m_pObj;
	GF_JSClass upnpClass;
	GF_JSClass upnpDeviceClass;
	GF_JSClass upnpGenericDeviceClass;
	GF_JSClass upnpServiceClass;

	GF_List *m_Devices;
	u32 last_time, upnp_init_time;

	/*callback from AVControlPoint device discovery*/
	void OnMediaServerAdd(PLT_DeviceDataReference& device, int added);
	void OnMediaRendererAdd(PLT_DeviceDataReference& device, int added);

	void OnDeviceAdd(GPAC_DeviceItem *item, int added);


	jsval GetUPnPDevice(const char *src_url);

#else
	void OnMediaServerAdd(PLT_DeviceDataReference& device, int added) {}
	void OnMediaRendererAdd(PLT_DeviceDataReference& device, int added) {}
	void OnDeviceAdd(GPAC_DeviceItem *item, int added) {}
#endif

};

#ifdef GPAC_HAS_SPIDERMONKEY
SMJS_DECL_FUNC_PROP_GET( upnpservice_getProperty);

#ifdef USE_FFDEV_17
#define VPASSIGN(__b) __vp.set( __b )
#define VPGET() (jsval) __vp
#else
#define VPASSIGN(__b) *vp = __b
#define VPGET() *vp
#endif

#endif

void format_time_string(char *str, Double dur);


#endif	/*_GPAC_PLATINUM_H_*/
