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



#ifndef _GPAC_MEDIA_RENDER_H_
#define _GPAC_MEDIA_RENDER_H_

#include "Neptune.h"
#include "PltMediaRenderer.h"
#include "PltService.h"

class GF_UPnP;

class GPAC_MediaRenderer  : public PLT_DeviceHost,
	public PLT_MediaRendererDelegate
{
public:
	GPAC_MediaRenderer (GF_UPnP *upnp, const char*          friendly_name,
	                    bool                 show_ip = false,
	                    const char*          uuid = NULL,
	                    unsigned int         port = 0);

	// PLT_DeviceHost methods
	virtual NPT_Result SetupServices();
	virtual NPT_Result OnAction(PLT_ActionReference &action, const PLT_HttpRequestContext& context);

	/*these are called when event filtering is used (no JS)*/
	void SetDuration(Double duration, Bool can_seek);
	void SetTime(Double time);
	void SetConnected(const char *url);

protected:
	virtual ~GPAC_MediaRenderer();

	// PLT_MediaRendererInterface methods
	// ConnectionManager
	virtual NPT_Result OnGetCurrentConnectionInfo(PLT_ActionReference& action);

	// AVTransport
	virtual NPT_Result OnNext(PLT_ActionReference& action);
	virtual NPT_Result OnPause(PLT_ActionReference& action);
	virtual NPT_Result OnPlay(PLT_ActionReference& action);
	virtual NPT_Result OnPrevious(PLT_ActionReference& action);
	virtual NPT_Result OnSeek(PLT_ActionReference& action);
	virtual NPT_Result OnStop(PLT_ActionReference& action);
	virtual NPT_Result OnSetAVTransportURI(PLT_ActionReference& action);
	virtual NPT_Result OnSetPlayMode(PLT_ActionReference& action);

	// RenderingControl
	//virtual NPT_Result OnGetVolume(PLT_ActionReference& action);
	virtual NPT_Result OnSetVolume(PLT_ActionReference& action);
	virtual NPT_Result OnSetVolumeDB(PLT_ActionReference& action);
	virtual NPT_Result OnSetMute(PLT_ActionReference& action);
	virtual NPT_Result OnGetVolumeDBRange(PLT_ActionReference &action);


private:
	GF_UPnP *m_pUPnP;

	Bool m_connected;

	/*pointer to the AV service for further StateVariable modifications*/
	PLT_Service *m_pAVService;
	PLT_Service *m_pMigrationService;

	GF_List *m_mediaHistoryList;
	u32 m_track_pos;
	u32 m_volume;
	Bool m_muted, m_l_muted, m_r_muted;
	NPT_String m_ip_src;
	Double m_Duration, m_Time;
};

#endif	/*_GPAC_MEDIA_RENDER_H_*/
