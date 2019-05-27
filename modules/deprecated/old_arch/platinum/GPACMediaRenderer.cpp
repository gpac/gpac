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

#include "GPACPlatinum.h"

NPT_SET_LOCAL_LOGGER("gpac.media.renderer")

extern NPT_UInt8 RDR_ConnectionManagerSCPD[];
extern NPT_UInt8 RDR_AVTransportSCPD[];
extern NPT_UInt8 RDR_RenderingControlSCPD[];


void format_time_string(char *str, Double dur)
{
	u32 h, m, s;
	h = (u32) (dur / 3600);
	m = (u32) ( (dur - h*3600) / 60);
	s = (u32) ((dur - h*3600 - m*60));
	sprintf(str, "%02d:%02d:%02d", h, m, s);
}

GPAC_MediaRenderer::GPAC_MediaRenderer(GF_UPnP *upnp, const char*  friendly_name,
                                       bool         show_ip,
                                       const char*  uuid,
                                       unsigned int port) :
	PLT_DeviceHost("/", uuid, "urn:schemas-upnp-org:device:MediaRenderer:1", friendly_name, show_ip, port)
{
	m_mediaHistoryList = gf_list_new();
	m_pUPnP = upnp;
	m_connected = GF_FALSE;
	m_Duration = m_Time = 0;
}

GPAC_MediaRenderer::~GPAC_MediaRenderer()
{
	if (m_mediaHistoryList) {
		/* empty mediaHistoryList */
		while (gf_list_count(m_mediaHistoryList) > 0) {
			char * last = (char*)gf_list_last(m_mediaHistoryList);
			gf_list_rem_last(m_mediaHistoryList);
			gf_free(last);
		}
		gf_list_del(m_mediaHistoryList);
	}
}

NPT_Result
GPAC_MediaRenderer::SetupServices()
{
	PLT_Service* service;

	{
		/* AVTransport */
		m_pAVService = new PLT_Service(
		    this,
		    "urn:schemas-upnp-org:service:AVTransport:1",
		    "urn:upnp-org:serviceId:AVTransport",
		    "AVTransport",
		    "urn:schemas-upnp-org:metadata-1-0/AVT/");

		NPT_CHECK_FATAL(m_pAVService->SetSCPDXML((const char*) RDR_AVTransportSCPD));
		NPT_CHECK_FATAL(AddService(m_pAVService));

		m_pAVService->SetStateVariableRate("LastChange", NPT_TimeInterval(0.2f));
		m_pAVService->SetStateVariable("A_ARG_TYPE_InstanceID", "0");

		// GetCurrentTransportActions
		m_pAVService->SetStateVariable("CurrentTransportActions", "Play,Pause,Stop,Seek,Next,Previous");

		// GetDeviceCapabilities
		m_pAVService->SetStateVariable("PossiblePlaybackStorageMedia", "NONE,NETWORK");
		m_pAVService->SetStateVariable("PossibleRecordStorageMedia", "NOT_IMPLEMENTED");
		m_pAVService->SetStateVariable("PossibleRecordQualityModes", "NOT_IMPLEMENTED");

		// GetMediaInfo
		m_pAVService->SetStateVariable("NumberOfTracks", "0");
		m_pAVService->SetStateVariable("CurrentMediaDuration", "00:00:00");
		m_pAVService->SetStateVariable("AVTransportURI", "");
		m_pAVService->SetStateVariable("AVTransportURIMetadata", "");
		m_pAVService->SetStateVariable("NextAVTransportURI", "NOT_IMPLEMENTED");
		m_pAVService->SetStateVariable("NextAVTransportURIMetadata", "NOT_IMPLEMENTED");
		m_pAVService->SetStateVariable("PlaybackStorageMedium", "NONE");
		m_pAVService->SetStateVariable("RecordStorageMedium", "NOT_IMPLEMENTED");
		m_pAVService->SetStateVariable("RecordMediumWriteStatus", "NOT_IMPLEMENTED");

		// GetPositionInfo
		m_pAVService->SetStateVariable("CurrentTrack", "0");
		m_pAVService->SetStateVariable("CurrentTrackDuration", "00:00:00");
		m_pAVService->SetStateVariable("CurrentTrackMetadata", "");
		m_pAVService->SetStateVariable("CurrentTrackURI", "");
		m_pAVService->SetStateVariable("RelativeTimePosition", "00:00:00");
		m_pAVService->SetStateVariable("AbsoluteTimePosition", "00:00:00");
		m_pAVService->SetStateVariable("RelativeCounterPosition", "2147483647"); // means NOT_IMPLEMENTED
		m_pAVService->SetStateVariable("AbsoluteCounterPosition", "2147483647"); // means NOT_IMPLEMENTED

		// disable indirect eventing for certain state variables
		//PLT_StateVariable* var;
		//var =
		m_pAVService->FindStateVariable("RelativeTimePosition");
		//if (var) var->DisableIndirectEventing();
		//var =
		m_pAVService->FindStateVariable("AbsoluteTimePosition");
		//if (var) var->DisableIndirectEventing();
		//var =
		m_pAVService->FindStateVariable("RelativeCounterPosition");
		//if (var) var->DisableIndirectEventing();
		//var =
		m_pAVService->FindStateVariable("AbsoluteCounterPosition");
		//if (var) var->DisableIndirectEventing();

		// GetTransportInfo
		m_pAVService->SetStateVariable("TransportState", "NO_MEDIA_PRESENT");
		m_pAVService->SetStateVariable("TransportStatus", "OK");
		m_pAVService->SetStateVariable("TransportPlaySpeed", "1");

		// GetTransportSettings
		m_pAVService->SetStateVariable("CurrentPlayMode", "NORMAL");
		m_pAVService->SetStateVariable("CurrentRecordQualityMode", "NOT_IMPLEMENTED");
	}

	{
		/* ConnectionManager */
		service = new PLT_Service(
		    this,
		    "urn:schemas-upnp-org:service:ConnectionManager:1",
		    "urn:upnp-org:serviceId:ConnectionManager",
		    "ConnectionManager");
		NPT_CHECK_FATAL(service->SetSCPDXML((const char*) RDR_ConnectionManagerSCPD));
		NPT_CHECK_FATAL(AddService(service));

		service->SetStateVariable("CurrentConnectionIDs", "0");

		// put all supported mime types here instead
		service->SetStateVariable("SinkProtocolInfo", "http-get:*:*:*, rtsp-rtp-udp:*:*:*");
		service->SetStateVariable("SourceProtocolInfo", "");
	}

	{
		/* RenderingControl */
		service = new PLT_Service(
		    this,
		    "urn:schemas-upnp-org:service:RenderingControl:1",
		    "urn:upnp-org:serviceId:RenderingControl",
		    "RenderingControl",
		    "urn:schemas-upnp-org:metadata-1-0/RCS/");
		NPT_CHECK_FATAL(service->SetSCPDXML((const char*) RDR_RenderingControlSCPD));
		NPT_CHECK_FATAL(AddService(service));

		service->SetStateVariableRate("LastChange", NPT_TimeInterval(0.2f));

		service->SetStateVariable("Mute", "0");
		service->SetStateVariable("Volume", "100");
	}

	{
		static NPT_UInt8 MIGRATION_SCPDXML[] = "<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">\
    <serviceStateTable>\
        <stateVariable>\
            <name>MigrationStatus</name>\
            <sendEventsAttribute>no</sendEventsAttribute>\
            <dataType>string</dataType>\
            <allowedValueList>\
                <allowedValue>OK</allowedValue>\
                <allowedValue>ERROR_OCCURRED</allowedValue>\
            </allowedValueList>\
        </stateVariable>\
        <stateVariable>\
            <name>MigrationMetaData</name>\
            <sendEventsAttribute>no</sendEventsAttribute>\
            <dataType>string</dataType>\
        </stateVariable>\
        <stateVariable>\
            <name>A_ARG_TYPE_InstanceID</name>\
            <sendEventsAttribute>no</sendEventsAttribute>\
            <dataType>ui4</dataType>\
        </stateVariable>\
    </serviceStateTable>\
    <actionList>\
        <action>\
            <name>StopForMigration</name>\
            <argumentList>\
                <argument>\
                    <name>InstanceID</name>\
                    <direction>in</direction>\
                    <relatedStateVariable>A_ARG_TYPE_InstanceID</relatedStateVariable>\
                </argument>\
                <argument>\
                    <name>MigrationStatus</name>\
                    <direction>out</direction>\
                    <relatedStateVariable>MigrationStatus</relatedStateVariable>\
                </argument>\
                <argument>\
                    <name>MigrationMetaData</name>\
                    <direction>out</direction>\
                    <relatedStateVariable>MigrationMetaData</relatedStateVariable>\
                </argument>\
            </argumentList>\
        </action>\
    </actionList>\
</scpd>";

		/* MigrationService */
		m_pMigrationService = new PLT_Service(this, "urn:intermedia:service:migration:1", "urn:intermedia:service:migration.001", "SessionMigration");

		NPT_CHECK_FATAL(m_pMigrationService->SetSCPDXML((const char*) MIGRATION_SCPDXML));
		NPT_CHECK_FATAL(AddService(m_pMigrationService));

		m_pMigrationService->SetStateVariable("MigrationStatus", "OK");
		m_pMigrationService->SetStateVariable("MigrationMetaData", "");
	}

	return NPT_SUCCESS;
}

NPT_Result
GPAC_MediaRenderer::OnAction(PLT_ActionReference&          action,
                             const PLT_HttpRequestContext& context)
{
	NPT_COMPILER_UNUSED(context);

	/* parse the action name */
	NPT_String name = action->GetActionDesc().GetName();

	m_ip_src = context.GetRemoteAddress().GetIpAddress().ToString();

	/* Is it a ConnectionManager Service Action ? */
	if (name.Compare("GetCurrentConnectionIDs", true) == 0) {
		if (NPT_FAILED(action->SetArgumentsOutFromStateVariable())) {
			return NPT_FAILURE;
		}
		return NPT_SUCCESS;
	}
	if (name.Compare("GetProtocolInfo", true) == 0) {
		if (NPT_FAILED(action->SetArgumentsOutFromStateVariable())) {
			return NPT_FAILURE;
		}
		return NPT_SUCCESS;
	}
	if (name.Compare("GetCurrentConnectionInfo", true) == 0) {
		return OnGetCurrentConnectionInfo(action);
	}
	if (name.Compare("StopForMigration", true) == 0) {
		NPT_String res = m_pUPnP->OnMigrate();
		m_pMigrationService->SetStateVariable("MigrationStatus", "OK");
		m_pMigrationService->SetStateVariable("MigrationMetaData", res);

		if (NPT_FAILED(action->SetArgumentsOutFromStateVariable())) {
			return NPT_FAILURE;
		}
		return NPT_SUCCESS;
	}

	/* Is it a AVTransport Service Action ? */

	// since all actions take an instance ID and we only support 1 instance
	// verify that the Instance ID is 0 and return an error here now if not
	NPT_String serviceType = action->GetActionDesc().GetService()->GetServiceType();
	if (serviceType.Compare("urn:schemas-upnp-org:service:AVTransport:1", true) == 0) {
		if (NPT_FAILED(action->VerifyArgumentValue("InstanceID", "0"))) {
			action->SetError(802,"Not valid InstanceID.");
			return NPT_FAILURE;
		}
	}

	if (name.Compare("GetCurrentTransportActions", true) == 0) {
		if (NPT_FAILED(action->SetArgumentsOutFromStateVariable())) {
			return NPT_FAILURE;
		}
		return NPT_SUCCESS;
	}
	if (name.Compare("GetDeviceCapabilities", true) == 0) {
		if (NPT_FAILED(action->SetArgumentsOutFromStateVariable())) {
			return NPT_FAILURE;
		}
		return NPT_SUCCESS;
	}
	if (name.Compare("GetMediaInfo", true) == 0) {
		if (NPT_FAILED(action->SetArgumentsOutFromStateVariable())) {
			return NPT_FAILURE;
		}
		return NPT_SUCCESS;
	}
	if (name.Compare("GetPositionInfo", true) == 0) {
		if (m_pUPnP->m_pTerm->root_scene) {
			char szVal[100];

			m_pAVService->SetStateVariable("CurrentTrack", "0");
			format_time_string(szVal, m_Duration);
			m_pAVService->SetStateVariable("CurrentTrackDuration", szVal);

			m_pAVService->SetStateVariable("CurrentTrackMetadata", "");
			m_pAVService->SetStateVariable("CurrentTrackURI", "");
			format_time_string(szVal, m_Time);
			m_pAVService->SetStateVariable("RelativeTimePosition", szVal);
			m_pAVService->SetStateVariable("AbsoluteTimePosition", szVal);
			m_pAVService->SetStateVariable("RelativeCounterPosition", "2147483647"); // means NOT_IMPLEMENTED
			m_pAVService->SetStateVariable("AbsoluteCounterPosition", "2147483647"); // means NOT_IMPLEMENTED
		} else {
			if (NPT_FAILED(action->SetArgumentsOutFromStateVariable())) {
				return NPT_FAILURE;
			}
		}
		return NPT_SUCCESS;
	}
	if (name.Compare("GetTransportInfo", true) == 0) {
		if (NPT_FAILED(action->SetArgumentsOutFromStateVariable())) {
			return NPT_FAILURE;
		}
		return NPT_SUCCESS;
	}
	if (name.Compare("GetTransportSettings", true) == 0) {
		if (NPT_FAILED(action->SetArgumentsOutFromStateVariable())) {
			return NPT_FAILURE;
		}
		return NPT_SUCCESS;
	}
	if (name.Compare("Next", true) == 0) {
		return OnNext(action);
	}
	if (name.Compare("Pause", true) == 0) {
		return OnPause(action);
	}
	if (name.Compare("Play", true) == 0) {
		return OnPlay(action);
	}
	if (name.Compare("Previous", true) == 0) {
		return OnPrevious(action);
	}
	if (name.Compare("Seek", true) == 0) {
		return OnSeek(action);
	}
	if (name.Compare("Stop", true) == 0) {
		return OnStop(action);
	}
	if (name.Compare("SetAVTransportURI", true) == 0) {
		return OnSetAVTransportURI(action);
	}
	if (name.Compare("SetPlayMode", true) == 0) {
		return OnSetPlayMode(action);
	}

	/* Is it a RendererControl Service Action ? */
	if (serviceType.Compare("urn:schemas-upnp-org:service:RenderingControl:1", true) == 0) {
		/* we only support master channel */
		if (NPT_FAILED(action->VerifyArgumentValue("Channel", "Master"))) {
			action->SetError(402,"Invalid Args.");
			return NPT_FAILURE;
		}
	}

	if ((name.Compare("GetVolume", true) == 0) || (name.Compare("GetVolumeRangeDB", true) == 0) ) {
		NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());
		return NPT_SUCCESS;
	}

	if (name.Compare("GetMute", true) == 0) {
		NPT_CHECK_SEVERE(action->SetArgumentsOutFromStateVariable());
		return NPT_SUCCESS;
	}

	if (name.Compare("SetVolume", true) == 0) {
		return OnSetVolume(action);
	}
	if (name.Compare("SetVolumeDB", true) == 0) {
		return OnSetVolumeDB(action);
	}

	if (name.Compare("SetMute", true) == 0) {
		return OnSetMute(action);
	}

	action->SetError(401,"No Such Action.");
	return NPT_FAILURE;
}

NPT_Result
GPAC_MediaRenderer::OnGetCurrentConnectionInfo(PLT_ActionReference& action)
{
	if (NPT_FAILED(action->VerifyArgumentValue("ConnectionID", "0"))) {
		action->SetError(706,"No Such Connection.");
		return NPT_FAILURE;
	}

	if (NPT_FAILED(action->SetArgumentValue("RcsID", "0"))) {
		return NPT_FAILURE;
	}
	if (NPT_FAILED(action->SetArgumentValue("AVTransportID", "0"))) {
		return NPT_FAILURE;
	}
	if (NPT_FAILED(action->SetArgumentValue("ProtocolInfo", "http-get:*:*:*"))) {
		return NPT_FAILURE;
	}
	if (NPT_FAILED(action->SetArgumentValue("PeerConnectionManager", "/"))) {
		return NPT_FAILURE;
	}
	if (NPT_FAILED(action->SetArgumentValue("PeerConnectionID", "-1"))) {
		return NPT_FAILURE;
	}
	if (NPT_FAILED(action->SetArgumentValue("Direction", "Input"))) {
		return NPT_FAILURE;
	}
	if (NPT_FAILED(action->SetArgumentValue("Status", "Unknown"))) {
		return NPT_FAILURE;
	}

	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaRenderer::OnSetAVTransportURI(PLT_ActionReference& action)
{
	char the_url[4096], szVal[100];
	NPT_String url_id;
	const char *MediaUri;
	if (NPT_FAILED(action->GetArgumentValue("CurrentURI", url_id))) {
		return NPT_FAILURE;
	}
	MediaUri = url_id;
	if (!MediaUri) return NPT_FAILURE;

	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Request: change media\n"));

	if (m_connected) {
		m_connected = GF_FALSE;
		m_pUPnP->OnStop(m_ip_src);
	}
	const char *ext = strrchr(MediaUri, '.');
	if (ext && !stricmp(ext, ".m3u")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[UPnP] M3U playlists not supported yet\n"));
		return NPT_SUCCESS;
	}

	/* Load and add to mediaHistoryList */
	strcpy(the_url, MediaUri);
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Adding media to the list : %s\n", MediaUri));
	gf_list_add(m_mediaHistoryList, gf_strdup(MediaUri));
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Opening URL %s\n", the_url));
	m_track_pos = gf_list_count(m_mediaHistoryList);

	m_connected = GF_TRUE;
	m_pUPnP->OnConnect(the_url, m_ip_src);
	/* Set UPnP datas */
	m_pAVService->SetStateVariable("TransportState", "PLAYING");
	m_pAVService->SetStateVariable("AVTransportURI", the_url);

	sprintf(szVal, "%d", gf_list_count(m_mediaHistoryList));
	m_pAVService->SetStateVariable("NumberOfTracks", szVal);
	sprintf(szVal, "%d", m_track_pos);
	m_pAVService->SetStateVariable("CurrentTrack", szVal);
	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaRenderer::OnPause(PLT_ActionReference& action)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Request: change state : PAUSE\n"));
	m_pAVService->SetStateVariable("TransportState", "PAUSED_PLAYBACK");
	m_pUPnP->OnPause(GF_FALSE, m_ip_src);
	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaRenderer::OnPlay(PLT_ActionReference& action)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Request: change state : PLAY\n"));

	/* if nothing playing, connect to first media of mediaHistoryList */
	if (m_connected) {
		m_pAVService->SetStateVariable("TransportState", "PLAYING");
		m_pUPnP->OnPause(GF_TRUE, m_ip_src);
	} else if (gf_list_count(m_mediaHistoryList) >= 1) {
		char *track = (char *) gf_list_get(m_mediaHistoryList, 0);
		m_track_pos = 1;

		GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Reading first media : %s\n", track));
		m_pAVService->SetStateVariable("TransportState", "PLAYING");
		m_connected = GF_TRUE;
		m_pUPnP->OnConnect(track, m_ip_src);
		//MRSetTrack(track, upnph->TrackPosition);
	}
	return NPT_SUCCESS;
}


NPT_Result GPAC_MediaRenderer::OnStop(PLT_ActionReference& action)
{
	GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[UPnP] Request: change state : STOP\n"));
	if (m_pUPnP->m_pTerm->root_scene) {
		m_pAVService->SetStateVariable("TransportState", "STOPPED");
		m_pUPnP->OnStop(m_ip_src);
	}
	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaRenderer::OnPrevious(PLT_ActionReference& action)
{
	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaRenderer::OnNext(PLT_ActionReference& action)
{
	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaRenderer::OnSeek(PLT_ActionReference& action)
{
	u32 h, m, s;
	Double time;
	NPT_String unit, target;
	if (NPT_FAILED(action->GetArgumentValue("Unit", unit))) {
		return NPT_FAILURE;
	}
	if (NPT_FAILED(action->GetArgumentValue("Target", target))) {
		return NPT_FAILURE;
	}
	if ((unit!="ABS_TIME") && (unit!="REL_TIME")) {
		action->SetError(710,"Seek mode not supported");
		return NPT_FAILURE;
	}
	sscanf(target, "%d:%d:%d", &h, &m, &s);
	time = h*3600.0 + m*60.0 + s;
	m_pUPnP->OnSeek(time);
	return NPT_SUCCESS;
}


NPT_Result GPAC_MediaRenderer::OnSetPlayMode(PLT_ActionReference& action)
{
	return NPT_SUCCESS;
}


NPT_Result GPAC_MediaRenderer::OnSetVolume(PLT_ActionReference& action)
{
	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaRenderer::OnSetVolumeDB(PLT_ActionReference& action)
{
	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaRenderer::OnSetMute(PLT_ActionReference& action)
{
	return NPT_SUCCESS;
}

NPT_Result GPAC_MediaRenderer::OnGetVolumeDBRange(PLT_ActionReference& action )
{
	return NPT_SUCCESS;
}

void GPAC_MediaRenderer::SetDuration(Double duration, Bool can_seek)
{
	char szVal[100];
	format_time_string(szVal, duration);
	m_Duration = duration;
	m_pAVService->SetStateVariable("CurrentTrackDuration", szVal);
}

void GPAC_MediaRenderer::SetTime(Double time)
{
	char szVal[100];
	format_time_string(szVal, time);
	m_Time = time;
	m_pAVService->SetStateVariable("RelativeTimePosition", szVal);
	m_pAVService->SetStateVariable("AbsoluteTimePosition", szVal);
}

void GPAC_MediaRenderer::SetConnected(const char *url)
{
	m_pAVService->SetStateVariable("AVTransportURI", url);
	m_pAVService->SetStateVariable("CurrentTrackURI", url);
	m_pAVService->SetStateVariable("TransportState", "PLAYING");
	m_connected = url ? GF_TRUE : GF_FALSE;
}
