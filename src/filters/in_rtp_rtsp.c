/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / RTP/RTSP input filter
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

#include "in_rtp.h"

#ifndef GPAC_DISABLE_STREAMING

static GF_Err rtpin_rtsp_process_response(GF_RTPInRTSP *sess, GF_RTSPCommand *com, GF_Err e)
{
	if (!strcmp(com->method, GF_RTSP_DESCRIBE))
		return rtpin_rtsp_describe_process(sess, com, e);
	else if (!strcmp(com->method, GF_RTSP_SETUP))
		rtpin_rtsp_setup_process(sess, com, e);
	else if (!strcmp(com->method, GF_RTSP_TEARDOWN))
		rtpin_rtsp_teardown_process(sess, com, e);
	else if (!strcmp(com->method, GF_RTSP_PLAY) || !strcmp(com->method, GF_RTSP_PAUSE))
		rtpin_rtsp_usercom_process(sess, com, e);
	return GF_OK;
}

void rtpin_rtsp_process_commands(GF_RTPInRTSP *sess)
{
	GF_RTSPCommand *com;
	GF_Err e;
	u32 time;
	char sMsg[1000];

	com = (GF_RTSPCommand *)gf_list_get(sess->rtsp_commands, 0);

	/*if asked or command to send, flushout TCP - TODO: check what's going on with ANNOUNCE*/
	if ((com && !(sess->flags & RTSP_WAIT_REPLY) ) || (sess->flags & RTSP_TCP_FLUSH) ) {
		while (1) {
			e = gf_rtsp_session_read(sess->session);
			if (e) break;
		}
	}

	/*handle response or announce*/
	if ( (com && (sess->flags & RTSP_WAIT_REPLY) ) /*|| (!com && sess->rtpin->handle_announce)*/) {
		e = gf_rtsp_get_response(sess->session, sess->rtsp_rsp);
		if (e!= GF_IP_NETWORK_EMPTY) {
			e = rtpin_rtsp_process_response(sess, com, e);
			/*this is a service connect error -> plugin may be discarded */
			if (e!=GF_OK) {
				gf_list_rem(sess->rtsp_commands, 0);
				gf_rtsp_command_del(com);
				gf_filter_setup_failure(sess->rtpin->filter, e);
				return;
			}

			gf_list_rem(sess->rtsp_commands, 0);
			gf_rtsp_command_del(com);
			sess->flags &= ~RTSP_WAIT_REPLY;
			sess->command_time = 0;
		} else {
			u32 time_out = sess->rtpin->rtsp_timeout;
			/*evaluate timeout*/
			time = gf_sys_clock() - sess->command_time;

			if (!strcmp(com->method, GF_RTSP_DESCRIBE) && (time_out < 10000) ) time_out = 10000;
			/*don't waste time waiting for teardown ACK, half a sec is enough. If server is not replying
			in time it is likely to never reply (happens with RTP over RTSP) -> kill session
			and create new one*/
			else if (!strcmp(com->method, GF_RTSP_TEARDOWN) && (time>=500) ) time = time_out;

			//signal what's going on
			if (time >= time_out) {
				if (!strcmp(com->method, GF_RTSP_TEARDOWN)) {
					gf_rtsp_session_reset(sess->session, GF_TRUE);
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTP] Request Timeout for command %s after %d ms\n", com->method, time));
				}

				rtpin_rtsp_process_response(sess, com, GF_IP_NETWORK_FAILURE);
				gf_list_rem(sess->rtsp_commands, 0);
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
		sprintf(sMsg, "Cannot send %s", com->method);
		rtpin_send_message(sess->rtpin, GF_IP_NETWORK_FAILURE, sMsg);
		gf_list_rem(sess->rtsp_commands, 0);
		gf_rtsp_command_del(com);
		sess->flags &= ~RTSP_WAIT_REPLY;
		sess->command_time = 0;
		return;
	}
	/*process*/
	if (!com->User_Agent) com->User_Agent = (char *) sess->rtpin->user_agent;
	com->Accept_Language = (char *) sess->rtpin->languages;
	/*if no session assigned and a session ID is valid, use it*/
	if (sess->session_id && !com->Session)
		com->Session = sess->session_id;

	/*preprocess describe before sending (always the ESD url thing)*/
	if (!strcmp(com->method, GF_RTSP_DESCRIBE)) {
		com->Session = NULL;
		if (!rtpin_rtsp_describe_preprocess(sess, com)) {
			e = GF_BAD_PARAM;
			goto exit;
		}
	}
	/*preprocess play/stop/pause before sending (aggregation)*/
	if (!strcmp(com->method, GF_RTSP_PLAY)
	        || !strcmp(com->method, GF_RTSP_PAUSE)
	        || !strcmp(com->method, GF_RTSP_TEARDOWN)) {
		//command is skipped
		if (!rtpin_rtsp_usercom_preprocess(sess, com)) {
			e = GF_BAD_PARAM;
			goto exit;
		}
	}
	e = gf_rtsp_send_command(sess->session, com);
	if (e) {
		sprintf(sMsg, "Cannot send %s", com->method);
		rtpin_send_message(sess->rtpin, e, sMsg);
		rtpin_rtsp_process_response(sess, com, e);
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
		gf_list_rem(sess->rtsp_commands, 0);
		gf_rtsp_command_del(com);
		sess->flags &= ~RTSP_WAIT_REPLY;
		sess->command_time = 0;
	}
}


/*locate channel - if requested remove from session*/
GF_RTPInStream *rtpin_find_stream(GF_RTPIn *rtp, GF_FilterPid *opid, u32 ES_ID, char *es_control, Bool remove_stream)
{
	u32 i=0;
	GF_RTPInStream *st;

	while ((st = (GF_RTPInStream *)gf_list_enum(rtp->streams, &i))) {
		if (opid && (st->opid==opid)) goto found;
		if (ES_ID && (st->ES_ID==ES_ID)) goto found;
		if (es_control && st->control) {
			char *ctrl_start = strstr(es_control, st->control);
			if (ctrl_start && !strcmp(ctrl_start, st->control)) goto found;
		}
	}
	return NULL;

found:
	if (remove_stream) gf_list_rem(rtp->streams, i-1);
	return st;
}

/*locate session by control*/
GF_RTPInRTSP *rtpin_rtsp_check(GF_RTPIn *rtp, char *control)
{
	if (!control) return NULL;

	if (!strcmp(control, "*")) control = (char *) rtp->src;
	if (gf_rtsp_is_my_session(rtp->session->session, control)) return rtp->session;
	return NULL;
}

GF_RTPInRTSP *rtpin_rtsp_new(GF_RTPIn *rtp, char *session_control)
{
	char *szCtrl, *szExt;
	GF_RTPInRTSP *tmp;
	GF_RTSPSession *rtsp;

	if (!session_control) return NULL;

	if (rtp->session) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP] Request more than one RTSP session for more URL, old code to patch !!\n"));
		return rtp->session;
	}

	/*little fix: some servers don't understand DESCRIBE URL/trackID=, so remove the trackID...*/
	szCtrl = gf_strdup(session_control);
	szExt = szCtrl ? gf_file_ext_start(szCtrl) : NULL;
	if (szExt) {
		szExt = strchr(szExt, '/');
		if (szExt) {
			if (!strnicmp(szExt+1, "trackID=", 8) || !strnicmp(szExt+1, "ESID=", 5) || !strnicmp(szExt+1, "ES_ID=", 6)) szExt[0] = 0;
		}
	}

	rtsp = gf_rtsp_session_new(szCtrl, rtp->default_port);
	gf_free(szCtrl);

	if (!rtsp) return NULL;

	GF_SAFEALLOC(tmp, GF_RTPInRTSP);
	if (!tmp) return NULL;
	tmp->rtpin = rtp;
	tmp->session = rtsp;

	if (rtp->interleave) {
		gf_rtsp_set_buffer_size(rtsp, rtp->block_size);
	} else {
		gf_rtsp_set_buffer_size(rtsp, RTSP_BUFFER_SIZE);
	}
	tmp->rtsp_commands = gf_list_new();
	tmp->rtsp_rsp = gf_rtsp_response_new();

	rtp->session = tmp;
	return tmp;
}

GF_Err rtpin_add_stream(GF_RTPIn *rtp, GF_RTPInStream *stream, char *session_control)
{
	Bool has_aggregated_control;
	char *service_name;
	GF_RTPInRTSP *in_session = rtpin_rtsp_check(rtp, session_control);

	has_aggregated_control = GF_FALSE;
	if (session_control) {
		//if (!strcmp(session_control, "*")) session_control = NULL;
		has_aggregated_control = GF_TRUE;
	}

	/*regular setup in an established session (RTSP DESCRIBE)*/
	if (in_session) {
		in_session->flags |= RTSP_AGG_CONTROL;
		stream->rtsp = in_session;
		gf_list_add(rtp->streams, stream);
		return GF_OK;
	}

	/*setup through SDP with control - assume this is RTSP and try to create a session*/
	if (stream->control) {
		/*stream control is relative to main session*/
		if (strnicmp(stream->control, "rtsp://", 7) && strnicmp(stream->control, "rtspu://", 8) && strnicmp(stream->control, "satip://", 8)) {
			/*locate session by control - if no control was provided for the session, use default
			session*/
			if (!in_session) in_session = rtpin_rtsp_check(rtp, session_control ?: "*");
			/*none found, try to create one*/
			if (!in_session) in_session = rtpin_rtsp_new(rtp, session_control);
			/*cannot add an RTSP session for this channel, check if multicast*/
//			if (!in_session && gf_rtp_is_unicast(stream->rtp_ch) ) return GF_SERVICE_ERROR;
		}
		/*stream control is absolute*/
		else {
			char *ctrl;
			in_session = rtpin_rtsp_check(rtp, stream->control);
			if (!in_session) in_session = rtpin_rtsp_check(rtp, session_control);
			if (!in_session) {
				if (session_control && strstr(stream->control, session_control))
					in_session = rtpin_rtsp_new(rtp, session_control);
				else
					in_session = rtpin_rtsp_new(rtp, stream->control);
				if (!in_session) return GF_SERVICE_ERROR;
			}
			/*remove session control part from channel control*/
			service_name = in_session->session->Service;
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
	gf_list_add(rtp->streams, stream);
	return GF_OK;
}


void rtpin_remove_stream(GF_RTPIn *rtp, GF_RTPInStream *stream)
{
	u32 i=0;
	GF_RTPInStream *st;
	while ((st = (GF_RTPInStream *)gf_list_enum(rtp->streams, &i))) {
		if (st == stream) {
			gf_list_rem(rtp->streams, i-1);
			break;
		}
	}
}

static void rtpin_rtsp_reset(GF_RTPInRTSP *sess, GF_Err e)
{
	if (!sess) return;

	//destroy command list
	while (gf_list_count(sess->rtsp_commands)) {
		GF_RTSPCommand *com = (GF_RTSPCommand *)gf_list_get(sess->rtsp_commands, 0);
		gf_list_rem(sess->rtsp_commands, 0);

		/*reset static strings*/
		com->User_Agent = NULL;
		com->Accept_Language = NULL;
		com->Session = NULL;

		gf_rtsp_command_del(com);
		//first = 0;
	}
	/*reset session state*/
	gf_rtsp_session_reset(sess->session, GF_TRUE);
	sess->flags &= ~RTSP_WAIT_REPLY;
}


void rtpin_rtsp_del(GF_RTPInRTSP *sess)
{
	if (!sess) return;
	rtpin_rtsp_reset(sess, GF_OK);
	gf_list_del(sess->rtsp_commands);
	gf_rtsp_response_del(sess->rtsp_rsp);
	gf_rtsp_session_del(sess->session);
	if (sess->control) gf_free(sess->control);
	if (sess->session_id) gf_free(sess->session_id);
	if (sess->satip_server) gf_free(sess->satip_server);
	gf_free(sess);
}

void rtpin_send_message(GF_RTPIn *ctx, GF_Err e, const char *message)
{
/*	GF_NetworkCommand com;
	memset(&com, 0, sizeof(com));
	com.command_type = GF_NET_SERVICE_EVENT;
	com.send_event.evt.type = GF_EVENT_MESSAGE;
	com.send_event.evt.message.message = message;
	com.send_event.evt.message.error = e;
	gf_service_command(service, &com, GF_OK);
*/

}

#endif /*GPAC_DISABLE_STREAMING*/
