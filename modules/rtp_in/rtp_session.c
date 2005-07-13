/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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

void RP_SendFailure(RTPSession *sess, GF_RTSPCommand *com, GF_Err e)
{
	char sMsg[1000];
	sprintf(sMsg, "Cannot send %s", com->method);
	gf_term_on_message(sess->owner->service, e, sMsg);
}

void RP_ProcessResponse(RTPSession *sess, GF_RTSPCommand *com, GF_Err e)
{
	if (!strcmp(com->method, GF_RTSP_DESCRIBE)) 
		RP_ProcessDescribe(sess, com, e);
	else if (!strcmp(com->method, GF_RTSP_SETUP)) 
		RP_ProcessSetup(sess, com, e);
	else if (!strcmp(com->method, GF_RTSP_TEARDOWN)) 
		RP_ProcessTeardown(sess, com, e);
	else if (!strcmp(com->method, GF_RTSP_PLAY) || !strcmp(com->method, GF_RTSP_PAUSE)) 
		RP_ProcessUserCommand(sess, com, e);
}

/*access to command list is protected bymutex, BUT ONLY ACCESS - this way we're sure that command queueing
from app will not deadlock if we're waiting for the app to release any mutex (don't forget play request may 
come on stream N while we're processing stream P setup)*/
static GF_RTSPCommand *RP_GetCommand(RTPSession *sess)
{
	GF_RTSPCommand *com;
	gf_mx_p(sess->owner->mx);
	com = gf_list_get(sess->rtsp_commands, 0);
	gf_mx_v(sess->owner->mx);
	return com;
}

static void RP_RemoveCommand(RTPSession *sess)
{
	gf_mx_p(sess->owner->mx);
	gf_list_rem(sess->rtsp_commands, 0);
	gf_mx_v(sess->owner->mx);
}

void RP_ProcessCommands(RTPSession *sess, Bool read_tcp)
{
	GF_RTSPCommand *com;
	GF_Err e;
	u32 time;

	com = RP_GetCommand(sess);

	/*if asked or command to send, flushout TCP - TODO: check what's going on with ANNOUNCE*/
	if ((com && !sess->wait_for_reply) || read_tcp) {
		while (1) {
			e = gf_rtsp_session_read(sess->session);
			if (e) break;
		}
	}

	/*handle response or announce*/
	if ( (com && sess->wait_for_reply) || (!com && sess->owner->handle_announce)) {
		e = gf_rtsp_get_response(sess->session, sess->rtsp_rsp);
		if (e!= GF_IP_NETWORK_EMPTY) {
			RP_ProcessResponse(sess, com, e);
			RP_RemoveCommand(sess);
			gf_rtsp_command_del(com);
			sess->wait_for_reply = 0;
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
				sess->wait_for_reply = 0;
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
		sess->wait_for_reply = 0;
		sess->command_time = 0;
		return;
	}
	/*process*/
	com->User_Agent = RTSP_CLIENTNAME;
	com->Accept_Language = RTSP_LANGUAGE;
	com->Session = gf_rtsp_get_session_id(sess->session);

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
		sess->wait_for_reply = 1;
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
		sess->wait_for_reply = 0;
		sess->command_time = 0;
	}
}


/*locate channel - if requested remove from session*/
RTPStream *RP_FindChannel(RTPClient *rtp, LPNETCHANNEL ch, u32 ES_ID, char *es_control, Bool remove_stream)
{
	u32 i;
	RTPStream *st;

	for (i=0; i<gf_list_count(rtp->channels); i++) {
		st = gf_list_get(rtp->channels, i);
		if (ch && (st->channel==ch)) goto found;
		if (ES_ID && (st->ES_ID==ES_ID)) goto found;
		if (es_control && st->control) {
			char *ctrl_start = strstr(es_control, st->control);
			if (ctrl_start && !strcmp(ctrl_start, st->control)) goto found;
		}
	}
	return NULL;

found:
	if (remove_stream) gf_list_rem(rtp->channels, i);
	return st;
}

/*locate session by control*/
RTPSession *RP_CheckSession(RTPClient *rtp, char *control)
{
	if (!control || !rtp->rtsp_session) return NULL;
	if (gf_rtsp_is_my_session(rtp->rtsp_session->session, control)) return rtp->rtsp_session;
	return NULL;
}

RTPSession *RP_NewSession(RTPClient *rtp, char *session_control)
{
	char *szCtrl, *szExt;
	RTPSession *tmp;
	GF_RTSPSession *rtsp;

	if (rtp->rtsp_session) return NULL;

	/*little fix: some servers don't understand DESCRIBE URL/trackID=, so remove the trackID...*/
	szCtrl = strdup(session_control);
	szExt = strrchr(szCtrl, '.');
	if (szExt) {
		szExt = strchr(szExt, '/');
		if (szExt) {
			if (!strnicmp(szExt+1, "trackID=", 8) || !strnicmp(szExt+1, "ESID=", 5) || !strnicmp(szExt+1, "ES_ID=", 6)) szExt[0] = 0;
		}
	}

	rtsp = gf_rtsp_session_new(szCtrl, rtp->default_port);
	free(szCtrl);

	if (!rtsp) return NULL;

	gf_rtsp_set_logs(rtsp, rtp->logs);

	tmp = malloc(sizeof(RTPSession));
	memset(tmp, 0, sizeof(RTPSession));
	tmp->owner = rtp;
	tmp->session = rtsp;

	if (rtp->rtp_mode) {
		gf_rtsp_set_buffer_size(rtsp, RTSP_TCP_BUFFER_SIZE);
	} else {
		gf_rtsp_set_buffer_size(rtsp, RTSP_BUFFER_SIZE);
	}
	rtp->rtsp_session = tmp;

	tmp->rtsp_commands = gf_list_new();

	tmp->rtsp_rsp = gf_rtsp_response_new();	

	return tmp;
}

GF_Err RP_AddStream(RTPClient *rtp, RTPStream *stream, char *session_control)
{
	Bool has_aggregated_control;
	char *service_name, *ctrl;
	RTPSession *in_session = rtp->rtsp_session;

	has_aggregated_control = 0;
	if (session_control) {
		//if (!strcmp(session_control, "*")) session_control = NULL;
		if (session_control) has_aggregated_control = 1;
	}

	/*regular setup in an established session (RTSP DESCRIBE)*/
	if (in_session) {
		in_session->has_aggregated_control = has_aggregated_control;
		stream->rtsp = in_session;
		gf_list_add(rtp->channels, stream);
		return GF_OK;
	}

	/*setup through SDP with control - assume this is RTSP and try to create a session*/
	if (stream->control) {
		/*stream control is relative to main session*/
		if (strnicmp(stream->control, "rtsp://", 7) && strnicmp(stream->control, "rtspu://", 7)) {
			/*we need session control*/
			if (!session_control) return GF_SERVICE_ERROR;
			/*locate session by control*/
			if (!in_session) in_session = RP_CheckSession(rtp, session_control);
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
				service_name = strdup(ctrl);
				free(stream->control);
				stream->control = service_name;
			}
		}
	}
	/*no control specified, assume this is multicast*/
	else {
		in_session = NULL;
	}

	if (in_session) {
		in_session->has_aggregated_control = has_aggregated_control;
	} else if (stream->control) {
		free(stream->control);
		stream->control = NULL;
	}
	stream->rtsp = in_session;
	gf_list_add(rtp->channels, stream);
	return GF_OK;
}


void RP_RemoveStream(RTPClient *rtp, RTPStream *ch)
{
	u32 i;
	gf_mx_p(rtp->mx);
	for (i=0; i<gf_list_count(rtp->channels); i++) {
		if (gf_list_get(rtp->channels, i) == ch) {
			gf_list_rem(rtp->channels, i);
			break;
		}
	}
	gf_mx_v(rtp->mx);
}

void RP_ResetSession(RTPSession *sess, GF_Err e)
{
	GF_RTSPCommand *com;
	u32 first = 1;

	//destroy command list
	while (gf_list_count(sess->rtsp_commands)) {
		com = gf_list_get(sess->rtsp_commands, 0);
		gf_list_rem(sess->rtsp_commands, 0);
		//this destroys stacks if any
//		RP_SendFailure(sess, com, first ? e : GF_OK);
		gf_rtsp_command_del(com);
		first = 0;
	}
	/*reset session state*/	
	gf_rtsp_session_reset(sess->session, 1);
	sess->wait_for_reply = 0;
}


void RP_RemoveSession(RTPSession *sess, Bool immediate_shutdown)
{
	/*shutdown session*/
	RP_Teardown(sess, NULL);
	/*wait for ack*/
	if (!immediate_shutdown ) {
		while (gf_list_count(sess->rtsp_commands)) 
			gf_sleep(10);
	}

	RP_ResetSession(sess, GF_OK);

	gf_list_del(sess->rtsp_commands);
	gf_rtsp_response_del(sess->rtsp_rsp);
	gf_rtsp_session_del(sess->session);
	if (sess->control) free(sess->control);
	free(sess);
}


