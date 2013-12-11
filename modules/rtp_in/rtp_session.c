/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / RTP input module
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

#include "rtp_in.h"

#ifndef GPAC_DISABLE_STREAMING


void RP_SendFailure(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e)
{
	char sMsg[1000];
	sprintf(sMsg, "Cannot send %s", com->method);
	gf_term_on_message(sess->owner->service, e, sMsg);
}

GF_Err RP_ProcessResponse(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e)
{
	if (!strcmp(com->method, GF_RTSP_DESCRIBE)) 
		return RP_ProcessDescribe(sess, com, e);
	else if (!strcmp(com->method, GF_RTSP_SETUP)) 
		RP_ProcessSetup(sess, com, e);
	else if (!strcmp(com->method, GF_RTSP_TEARDOWN)) 
		RP_ProcessTeardown(sess, com, e);
	else if (!strcmp(com->method, GF_RTSP_PLAY) || !strcmp(com->method, GF_RTSP_PAUSE)) 
		RP_ProcessUserCommand(sess, com, e);
	return GF_OK;
}

/*access to command list is protected bymutex, BUT ONLY ACCESS - this way we're sure that command queueing
from app will not deadlock if we're waiting for the app to release any mutex (don't forget play request may 
come on stream N while we're processing stream P setup)*/
static GF_RTSPCommand *RP_GetCommand(RTSPSession *sess)
{
	GF_RTSPCommand *com;
	gf_mx_p(sess->owner->mx);
	com = (GF_RTSPCommand *)gf_list_get(sess->rtsp_commands, 0);
	gf_mx_v(sess->owner->mx);
	return com;
}

static void RP_RemoveCommand(RTSPSession *sess)
{
	gf_mx_p(sess->owner->mx);
	gf_list_rem(sess->rtsp_commands, 0);
	gf_mx_v(sess->owner->mx);
}

void RP_ProcessCommands(RTSPSession *sess)
{
	GF_RTSPCommand *com;
	GF_Err e;
	u32 time;

	com = RP_GetCommand(sess);

	/*if asked or command to send, flushout TCP - TODO: check what's going on with ANNOUNCE*/
	if ((com && !(sess->flags & RTSP_WAIT_REPLY) ) || (sess->flags & RTSP_TCP_FLUSH) ) {
		while (1) {
			e = gf_rtsp_session_read(sess->session);
			if (e) break;
		}
		sess->flags &= ~RTSP_TCP_FLUSH;
	}

	/*handle response or announce*/
	if ( (com && (sess->flags & RTSP_WAIT_REPLY) ) /*|| (!com && sess->owner->handle_announce)*/) {
		e = gf_rtsp_get_response(sess->session, sess->rtsp_rsp);
		if (e!= GF_IP_NETWORK_EMPTY) {
			e = RP_ProcessResponse(sess, com, e);
			/*this is a service connect error -> plugin may be discarded */
			if (e!=GF_OK) {
				RP_RemoveCommand(sess);
				gf_rtsp_command_del(com);
				gf_term_on_connect(sess->owner->service, NULL, e);
				return;
			}

			RP_RemoveCommand(sess);
			gf_rtsp_command_del(com);
			sess->flags &= ~RTSP_WAIT_REPLY;
			sess->command_time = 0;
		} else {
			/*evaluate timeout*/
			time = gf_sys_clock() - sess->command_time;
			/*don't waste time waiting for teardown ACK, half a sec is enough. If server is not replying
			in time it is likely to never reply (happens with RTP over RTSP) -> kill session 
			and create new one*/
			if (!strcmp(com->method, GF_RTSP_TEARDOWN) && (time>=500) ) time = sess->owner->time_out;
			//signal what's going on
			if (time >= sess->owner->time_out) {
				if (!strcmp(com->method, GF_RTSP_TEARDOWN)) gf_rtsp_session_reset(sess->session, 1);

				RP_ProcessResponse(sess, com, GF_IP_NETWORK_FAILURE);
				RP_RemoveCommand(sess);
				gf_rtsp_command_del(com);
				sess->flags &= ~RTSP_WAIT_REPLY;
				sess->command_time = 0;
				gf_rtsp_reset_aggregation(sess->session);
			}
		}
		return;
	}

	if (!com) return;

	/*send command - check RTSP session state first*/
	switch (gf_rtsp_get_session_state(sess->session)) {
	case GF_RTSP_STATE_WAITING:
	case GF_RTSP_STATE_WAIT_FOR_CONTROL:
		return;
	case GF_RTSP_STATE_INVALIDATED:
		RP_SendFailure(sess, com, GF_IP_NETWORK_FAILURE);
		RP_RemoveCommand(sess);
		gf_rtsp_command_del(com);
		sess->flags &= ~RTSP_WAIT_REPLY;
		sess->command_time = 0;
		return;
	}
	/*process*/
	com->User_Agent = (char*)gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(sess->owner->service), "Downloader", "UserAgent");
	if (!com->User_Agent) com->User_Agent = "GPAC " GPAC_VERSION " RTSP Client";
	com->Accept_Language = RTSP_LANGUAGE;
	/*if no session assigned and a session ID is valid, use it*/
	if (sess->session_id && !com->Session)
		com->Session = sess->session_id;

	e = GF_OK;
	/*preprocess describe before sending (always the ESD url thing)*/
	if (!strcmp(com->method, GF_RTSP_DESCRIBE)) {
		com->Session = NULL;
		if (!RP_PreprocessDescribe(sess, com)) {
			e = GF_BAD_PARAM;
			goto exit;
		}
	}
	/*preprocess play/stop/pause before sending (aggregation)*/
	if (!strcmp(com->method, GF_RTSP_PLAY) 
		|| !strcmp(com->method, GF_RTSP_PAUSE)
		|| !strcmp(com->method, GF_RTSP_TEARDOWN)) {
		//command is skipped
		if (!RP_PreprocessUserCom(sess, com)) {
			e = GF_BAD_PARAM;
			goto exit;
		}
	}
	e = gf_rtsp_send_command(sess->session, com);
	if (e) {
		RP_SendFailure(sess, com, e);
		RP_ProcessResponse(sess, com, e);
	} else {
		sess->command_time = gf_sys_clock();
		sess->flags |= RTSP_WAIT_REPLY;
	}

exit:
	/*reset static strings*/
	com->User_Agent = NULL;
	com->Accept_Language = NULL;
	com->Session = NULL;
	/*remove command*/
	if (e) {
		RP_RemoveCommand(sess);
		gf_rtsp_command_del(com);
		sess->flags &= ~RTSP_WAIT_REPLY;
		sess->command_time = 0;
	}
}


/*locate channel - if requested remove from session*/
RTPStream *RP_FindChannel(RTPClient *rtp, LPNETCHANNEL ch, u32 ES_ID, char *es_control, Bool remove_stream)
{
	u32 i=0;
	RTPStream *st;

	while ((st = (RTPStream *)gf_list_enum(rtp->channels, &i))) {
		if (ch && (st->channel==ch)) goto found;
		if (ES_ID && (st->ES_ID==ES_ID)) goto found;
		if (es_control && st->control) {
			char *ctrl_start = strstr(es_control, st->control);
			if (ctrl_start && !strcmp(ctrl_start, st->control)) goto found;
		}
	}
	return NULL;

found:
	if (remove_stream) gf_list_rem(rtp->channels, i-1);
	return st;
}

/*locate session by control*/
RTSPSession *RP_CheckSession(RTPClient *rtp, char *control)
{
	u32 i;
	RTSPSession *sess;
	if (!control) return NULL;

	if (!strcmp(control, "*")) control = (char *) gf_term_get_service_url(rtp->service);

	i=0;
	while ( (sess = (RTSPSession *)gf_list_enum(rtp->sessions, &i)) ) {
		if (gf_rtsp_is_my_session(sess->session, control)) return sess;
	}
	return NULL;
}

RTSPSession *RP_NewSession(RTPClient *rtp, char *session_control)
{
	char *szCtrl, *szExt;
	RTSPSession *tmp;
	GF_RTSPSession *rtsp;

	if (!session_control) return NULL;

	/*little fix: some servers don't understand DESCRIBE URL/trackID=, so remove the trackID...*/
	szCtrl = gf_strdup(session_control);
	szExt = szCtrl ? strrchr(szCtrl, '.') : NULL;
	if (szExt) {
		szExt = strchr(szExt, '/');
		if (szExt) {
			if (!strnicmp(szExt+1, "trackID=", 8) || !strnicmp(szExt+1, "ESID=", 5) || !strnicmp(szExt+1, "ES_ID=", 6)) szExt[0] = 0;
		}
	}

	rtsp = gf_rtsp_session_new(szCtrl, rtp->default_port);
	gf_free(szCtrl);

	if (!rtsp) return NULL;

	GF_SAFEALLOC(tmp, RTSPSession);
	tmp->owner = rtp;
	tmp->session = rtsp;


	szCtrl = (char *)gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(rtp->service), "Network", "MobileIPEnabled");
	if (szCtrl && !strcmp(szCtrl, "yes")) {
		char *ip = (char *)gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(rtp->service), "Network", "MobileIP");
		gf_rtsp_set_mobile_ip(rtsp, ip);
	}

	if (rtp->transport_mode) {
		gf_rtsp_set_buffer_size(rtsp, RTSP_TCP_BUFFER_SIZE);
	} else {
		gf_rtsp_set_buffer_size(rtsp, RTSP_BUFFER_SIZE);
	}
	tmp->rtsp_commands = gf_list_new();
	tmp->rtsp_rsp = gf_rtsp_response_new();	

	gf_list_add(rtp->sessions, tmp);

	return tmp;
}

GF_Err RP_AddStream(RTPClient *rtp, RTPStream *stream, char *session_control)
{
	Bool has_aggregated_control;
	char *service_name, *ctrl;
	RTSPSession *in_session = RP_CheckSession(rtp, session_control);

	has_aggregated_control = 0;
	if (session_control) {
		//if (!strcmp(session_control, "*")) session_control = NULL;
		if (session_control) has_aggregated_control = 1;
	}

	/*regular setup in an established session (RTSP DESCRIBE)*/
	if (in_session) {
		in_session->flags |= RTSP_AGG_CONTROL;
		stream->rtsp = in_session;
		gf_list_add(rtp->channels, stream);
		return GF_OK;
	}

	/*setup through SDP with control - assume this is RTSP and try to create a session*/
	if (stream->control) {
		/*stream control is relative to main session*/
		if (strnicmp(stream->control, "rtsp://", 7) && strnicmp(stream->control, "rtspu://", 7)) {
			/*locate session by control - if no control was provided for the session, use default
			session*/
			if (!in_session) in_session = RP_CheckSession(rtp, session_control ? session_control : "*");
			/*none found, try to create one*/
			if (!in_session) in_session = RP_NewSession(rtp, session_control);
			/*cannot add an RTSP session for this channel, check if multicast*/
//			if (!in_session && gf_rtp_is_unicast(stream->rtp_ch) ) return GF_SERVICE_ERROR;
		}
		/*stream control is absolute*/
		else {
			in_session = RP_CheckSession(rtp, stream->control);
			if (!in_session) in_session = RP_CheckSession(rtp, session_control);
			if (!in_session) {
				if (session_control && strstr(stream->control, session_control))
					in_session = RP_NewSession(rtp, session_control);
				else
					in_session = RP_NewSession(rtp, stream->control);
				if (!in_session) return GF_SERVICE_ERROR;
			}
			/*remove session control part from channel control*/
			service_name = gf_rtsp_get_service_name(in_session->session);
			ctrl = strstr(stream->control, service_name);
			if (ctrl && (strlen(ctrl) != strlen(service_name)) ) {
				ctrl += strlen(service_name) + 1;
				service_name = gf_strdup(ctrl);
				gf_free(stream->control);
				stream->control = service_name;
			}
		}
	}
	/*no control specified, assume this is multicast*/
	else {
		in_session = NULL;
	}

	if (in_session) {
		if (has_aggregated_control)
			in_session->flags |= RTSP_AGG_CONTROL;
	} else if (stream->control) {
		gf_free(stream->control);
		stream->control = NULL;
	}
	stream->rtsp = in_session;
	gf_list_add(rtp->channels, stream);
	return GF_OK;
}


void RP_RemoveStream(RTPClient *rtp, RTPStream *ch)
{
	u32 i=0;
	RTPStream *st;
	gf_mx_p(rtp->mx);
	while ((st = (RTPStream *)gf_list_enum(rtp->channels, &i))) {
		if (st == ch) {
			gf_list_rem(rtp->channels, i-1);
			break;
		}
	}
	gf_mx_v(rtp->mx);
}

void RP_ResetSession(RTSPSession *sess, GF_Err e)
{
	GF_RTSPCommand *com;
	//u32 first = 1;

	//destroy command list
	while (gf_list_count(sess->rtsp_commands)) {
		com = (GF_RTSPCommand *)gf_list_get(sess->rtsp_commands, 0);
		gf_list_rem(sess->rtsp_commands, 0);
		//this destroys stacks if any
//		RP_SendFailure(sess, com, first ? e : GF_OK);
		gf_rtsp_command_del(com);
		//first = 0;
	}
	/*reset session state*/	
	gf_rtsp_session_reset(sess->session, 1);
	sess->flags &= ~RTSP_WAIT_REPLY;
}


void RP_DelSession(RTSPSession *sess)
{
	RP_ResetSession(sess, GF_OK);
	gf_list_del(sess->rtsp_commands);
	gf_rtsp_response_del(sess->rtsp_rsp);
	gf_rtsp_session_del(sess->session);
	if (sess->control) gf_free(sess->control);
	if (sess->session_id) gf_free(sess->session_id);
	gf_free(sess);
}


#endif /*GPAC_DISABLE_STREAMING*/
