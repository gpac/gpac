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
#include <gpac/internal/ietf_dev.h>

#ifndef GPAC_DISABLE_STREAMING

static Bool rtpin_stream_is_valid(GF_RTPIn *rtp, GF_RTPInStream *stream)
{
	u32 i=0;
	GF_RTPInStream *st;
	while ((st = (GF_RTPInStream *)gf_list_enum(rtp->streams, &i))) {
		if (st == stream) return GF_TRUE;
	}
	return GF_FALSE;
}

static void rtpin_stream_stop(GF_RTPInStream *stream)
{
	if (!stream || !stream->rtsp) return;

	stream->flags &= ~RTP_SKIP_NEXT_COM;
	//stream->status = RTP_Disconnected;
	//remove interleaved
	if (gf_rtp_is_interleaved(stream->rtp_ch)) {
		gf_rtsp_unregister_interleave(stream->rtsp->session, gf_rtp_get_low_interleave_id(stream->rtp_ch));
	}
}

/*this prevent sending teardown on session with running channels*/
static Bool rtpin_rtsp_is_active(GF_RTPInStream *stream)
{
	GF_RTPInStream *a_st;
	u32 i, count;
	i = count = 0;
	while ((a_st = (GF_RTPInStream *)gf_list_enum(stream->rtpin->streams, &i))) {
		if (a_st->rtsp != stream->rtsp) continue;
		/*count only active channels*/
		if (a_st->status == RTP_Running) count++;
	}
	return count ? GF_TRUE : GF_FALSE;
}

static void rtpin_rtsp_queue_command(GF_RTPInRTSP *sess, GF_RTPInStream *stream, GF_RTSPCommand *com, Bool needs_sess_id)
{
	if (needs_sess_id) {
		com->Session = sess->session_id;
	}
	gf_list_add(sess->rtsp_commands, com);
}



/*
 						channel setup functions
																*/

void rtpin_rtsp_setup_send(GF_RTPInStream *stream)
{
	GF_RTSPCommand *com;
	GF_RTSPTransport *trans;

	com = gf_rtsp_command_new();
	com->method = gf_strdup(GF_RTSP_SETUP);

	//setup ports if unicast non interleaved or multicast
	if (gf_rtp_is_unicast(stream->rtp_ch) && (stream->rtpin->interleave != 1) && !gf_rtp_is_interleaved(stream->rtp_ch) ) {
		gf_rtp_set_ports(stream->rtp_ch, stream->rtpin->firstport);
	} else if (stream->rtpin->force_mcast) {
		gf_rtp_set_ports(stream->rtp_ch, stream->rtpin->firstport);
	}

	trans = gf_rtsp_transport_clone(gf_rtp_get_transport(stream->rtp_ch));

	/*some servers get confused when trying to resetup on the same remote ports, so reset info*/
	trans->port_first = trans->port_last = 0;
	trans->SSRC = 0;

	/*override transport: */
	/*1: multicast forced*/
	if (stream->rtpin->force_mcast) {
		trans->IsUnicast = GF_FALSE;
		trans->destination = gf_strdup(stream->rtpin->force_mcast);
		trans->TTL = stream->rtpin->ttl;
		if (trans->Profile) gf_free(trans->Profile);
		trans->Profile = gf_strdup(GF_RTSP_PROFILE_RTP_AVP);
		if (!(stream->rtsp->flags & RTSP_DSS_SERVER) ) {
			trans->port_first = trans->client_port_first;
			trans->port_last = trans->client_port_last;
			/*this is correct but doesn't work with DSS: the server expects "client_port" to indicate
			the multicast port, not "port" - this will send both*/
			//trans->client_port_first = trans->client_port_last = 0;
		}
		gf_rtp_setup_transport(stream->rtp_ch, trans, NULL);
	}
	/*2: RTP over RTSP forced*/
	else if (stream->rtsp->flags & RTSP_FORCE_INTER) {
		if (trans->Profile) gf_free(trans->Profile);
		trans->Profile = gf_strdup(GF_RTSP_PROFILE_RTP_AVP_TCP);
		//some servers expect the interleaved to be set during the setup request
		trans->IsInterleaved = GF_TRUE;
		trans->rtpID = 2*gf_list_find(stream->rtpin->streams, stream);
		trans->rtcpID = trans->rtpID+1;
		gf_rtp_setup_transport(stream->rtp_ch, trans, NULL);
	}

	if (trans->source) {
		gf_free(trans->source);
		trans->source = NULL;
	}

	/*turn off interleaving in case of re-setup, some servers don't like it (we still signal it
	through RTP/AVP/TCP profile so it's OK)*/
//	trans->IsInterleaved = 0;
	gf_list_add(com->Transports, trans);
	if (strlen(stream->control)) com->ControlString = gf_strdup(stream->control);

	com->user_data = stream;
	stream->status = RTP_WaitingForAck;

	rtpin_rtsp_queue_command(stream->rtsp, stream, com, GF_TRUE);
}

/*filter setup if no session (rtp only)*/
GF_Err rtpin_stream_setup(GF_RTPInStream *stream, RTPIn_StreamDescribe *ch_desc)
{
	GF_Err resp;

	/*assign ES_ID of the channel*/
	if (ch_desc && !stream->ES_ID && ch_desc->ES_ID) stream->ES_ID = ch_desc->ES_ID;

	stream->status = RTP_Setup;

	/*assign channel handle if not done*/
	if (ch_desc && stream->opid) {
		assert(stream->opid == ch_desc->opid);
	} else if (!stream->opid && stream->rtsp && !stream->rtsp->satip) {
		assert(ch_desc);
		assert(ch_desc->opid);
		stream->opid = ch_desc->opid;
	}

	/*no session , setup for pure rtp*/
	if (!stream->rtsp) {
		stream->flags |= RTP_CONNECTED;
		/*init rtp*/
		resp = rtpin_stream_init(stream, GF_FALSE);
		/*send confirmation to user*/
		rtpin_stream_ack_connect(stream, resp);
	} else {
		rtpin_rtsp_setup_send(stream);
	}
	return GF_OK;
}

static GF_Err rtpin_rtsp_tcp_send_report(void *par, void *par2, Bool is_rtcp, u8 *pck, u32 pck_size)
{
	return GF_OK;
}

void rtpin_rtsp_setup_process(GF_RTPInRTSP *sess, GF_RTSPCommand *com, GF_Err e)
{
	GF_RTPInStream *stream;
	u32 i;
	GF_RTSPTransport *trans;

	stream = (GF_RTPInStream *)com->user_data;
	if (e) goto exit;

	switch (sess->rtsp_rsp->ResponseCode) {
	case NC_RTSP_OK:
		break;
	case NC_RTSP_Not_Found:
		e = GF_STREAM_NOT_FOUND;
		goto exit;
	default:
		e = GF_SERVICE_ERROR;
		goto exit;
	}
	e = GF_SERVICE_ERROR;
	if (!stream) goto exit;

	/*assign session ID*/
	if (!sess->rtsp_rsp->Session) {
		e = GF_SERVICE_ERROR;
		goto exit;
	}
	if (!sess->session_id) sess->session_id = gf_strdup(sess->rtsp_rsp->Session);
	assert(!stream->session_id);

	/*transport setup: break at the first correct transport */
	i=0;
	while ((trans = (GF_RTSPTransport *)gf_list_enum(sess->rtsp_rsp->Transports, &i))) {
		/*copy over previous ports (hack for some servers overriding client ports)*/
		if (stream->rtpin->use_client_ports)
			gf_rtp_get_ports(stream->rtp_ch, &trans->client_port_first, &trans->client_port_last);

		if (gf_rtp_is_interleaved(stream->rtp_ch) && !trans->IsInterleaved) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSP] Requested interleaved RTP over RTSP but server did not setup interleave - cannot process command\n"));
			e = GF_REMOTE_SERVICE_ERROR;
			continue;
		}

		e = gf_rtp_setup_transport(stream->rtp_ch, trans, gf_rtsp_get_server_name(sess->session));
		if (!e) break;
	}
	if (e) goto exit;

	e = rtpin_stream_init(stream, GF_FALSE);
	if (e) goto exit;
	stream->status = RTP_Connected;

	//in case this is TCP channel, setup callbacks
	stream->flags &= ~RTP_INTERLEAVED;
	if (gf_rtp_is_interleaved(stream->rtp_ch)) {
		stream->flags |= RTP_INTERLEAVED;
		gf_rtsp_set_interleave_callback(sess->session, rtpin_rtsp_data_cbk);

		gf_rtp_set_interleave_callbacks(stream->rtp_ch, rtpin_rtsp_tcp_send_report, stream, stream);
		sess->flags |= RTSP_TCP_FLUSH;
	}

	if (sess->satip) {
		RTPIn_StreamControl *ch_ctrl = NULL;
		GF_RTSPCommand *com = gf_rtsp_command_new();
		com->method = gf_strdup(GF_RTSP_PLAY);
		GF_SAFEALLOC(ch_ctrl, RTPIn_StreamControl);
		ch_ctrl->stream = stream;
		com->user_data = ch_ctrl;
		rtpin_rtsp_queue_command(sess, stream, com, GF_TRUE);
	}

exit:
	/*confirm only on first connect, otherwise this is a re-SETUP of the rtsp session, not the channel*/
	if (stream && ! (stream->flags & RTP_CONNECTED) ) {
		if (!e)
			stream->flags |= RTP_CONNECTED;
		rtpin_stream_ack_connect(stream, e);
	}
	com->user_data = NULL;
}


/*
 						session/channel describe functions
																*/
/*filter describe commands in case of ESD URLs*/
Bool rtpin_rtsp_describe_preprocess(GF_RTPInRTSP *sess, GF_RTSPCommand *com)
{
	GF_RTPInStream *stream;
	RTPIn_StreamDescribe *ch_desc;
	/*not a channel describe*/
	if (!com->user_data) {
		rtpin_send_message(sess->rtpin, GF_OK, "Connecting...");
		return GF_TRUE;
	}

	ch_desc = (RTPIn_StreamDescribe *)com->user_data;
	stream = rtpin_find_stream(sess->rtpin, NULL, ch_desc->ES_ID, ch_desc->esd_url, GF_FALSE);
	if (!stream) return GF_TRUE;

	/*channel has been described already, skip describe and send setup directly*/
	rtpin_stream_setup(stream, ch_desc);

	if (ch_desc->esd_url) gf_free(ch_desc->esd_url);
	gf_free(ch_desc);
	return GF_FALSE;
}

/*process describe reply*/
GF_Err rtpin_rtsp_describe_process(GF_RTPInRTSP *sess, GF_RTSPCommand *com, GF_Err e)
{
	GF_RTPInStream *stream;
	RTPIn_StreamDescribe *ch_desc;

	stream = NULL;
	ch_desc = (RTPIn_StreamDescribe *)com->user_data;
	if (e) goto exit;

	switch (sess->rtsp_rsp->ResponseCode) {
	//TODO handle all 3xx codes  (redirections)
	case NC_RTSP_Multiple_Choice:
		e = ch_desc ? GF_STREAM_NOT_FOUND : GF_URL_ERROR;
		goto exit;
	case NC_RTSP_Not_Found:
		e = GF_URL_ERROR;
		goto exit;
	case NC_RTSP_OK:
		break;
	default:
		//we should have a basic error code mapping here
		e = GF_SERVICE_ERROR;
		goto exit;
	}

	stream = NULL;
	if (ch_desc) {
		stream = rtpin_find_stream(sess->rtpin, ch_desc->opid, ch_desc->ES_ID, ch_desc->esd_url, GF_FALSE);
	} else {
		rtpin_send_message(sess->rtpin, GF_OK, "Connected");
	}

	/*error on loading SDP is done internally*/
	rtpin_load_sdp(sess->rtpin, sess->rtsp_rsp->body, sess->rtsp_rsp->Content_Length, stream);

	if (!ch_desc) goto exit;
	if (!stream) {
		e = GF_STREAM_NOT_FOUND;
		goto exit;
	}
	e = rtpin_stream_setup(stream, ch_desc);

exit:
	com->user_data = NULL;
	if (e) {
		if (!ch_desc) {
			sess->connect_error = e;
			return e;
		} else if (stream) {
			rtpin_stream_ack_connect(stream, e);
		} else {
			//TODO - check if this is correct
			GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTPIn] code not tested file %s line %d !!\n", __FILE__, __LINE__));
			gf_filter_setup_failure(sess->rtpin->filter, e);
		}
	}
	if (ch_desc) gf_free(ch_desc);
	return GF_OK;
}

/*send describe*/
void rtpin_rtsp_describe_send(GF_RTPInRTSP *sess, char *esd_url, GF_FilterPid *opid)
{
	GF_RTPInStream *stream;
	RTPIn_StreamDescribe *ch_desc;
	GF_RTSPCommand *com;

	/*locate the channel by URL - if we have one, this means the channel is already described
	this happens when 2 ESD with URL use the same RTSP service - skip describe and send setup*/
	if (esd_url || opid) {
		stream = rtpin_find_stream(sess->rtpin, opid, 0, esd_url, GF_FALSE);
		if (stream) {
			if (!stream->opid) stream->opid = opid;
			switch (stream->status) {
			case RTP_Connected:
			case RTP_Running:
				rtpin_stream_ack_connect(stream, GF_OK);
				return;
			default:
				break;
			}
			ch_desc = (RTPIn_StreamDescribe *)gf_malloc(sizeof(RTPIn_StreamDescribe));
			ch_desc->esd_url = esd_url ? gf_strdup(esd_url) : NULL;
			ch_desc->opid = opid;
			rtpin_stream_setup(stream, ch_desc);

			if (esd_url) gf_free(ch_desc->esd_url);
			gf_free(ch_desc);
			return;
		}
		/*channel not found, send describe on service*/
	}

	/*send describe*/
	com = gf_rtsp_command_new();
	if (!sess->satip) {
		com->method = gf_strdup(GF_RTSP_DESCRIBE);
	} else {
		GF_Err e;
		GF_RTSPTransport *trans;
		GF_RTPInStream *stream = NULL;

		com->method = gf_strdup(GF_RTSP_SETUP);

		/*setup transport ports*/
		GF_SAFEALLOC(trans, GF_RTSPTransport);
		trans->IsUnicast = GF_TRUE;
		trans->client_port_first = sess->rtpin->satip_port;
		trans->client_port_last = sess->rtpin->satip_port+1;
		trans->Profile = gf_strdup(GF_RTSP_PROFILE_RTP_AVP);
		gf_list_add(com->Transports, trans);

		/*hardcoded channel*/
		stream = rtpin_stream_new_satip(sess->rtpin, sess->satip_server);
		if (!stream) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("SAT>IP: couldn't create the RTP stream.\n"));
			return;
		}
		e = rtpin_add_stream(sess->rtpin, stream, "*");
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("SAT>IP: couldn't add the RTP stream.\n"));
			return;
		}
		com->user_data = stream;
	}

	if (opid || esd_url) {
		com->Accept = gf_strdup("application/sdp");
		com->ControlString = esd_url ? gf_strdup(esd_url) : NULL;

		ch_desc = (RTPIn_StreamDescribe *)gf_malloc(sizeof(RTPIn_StreamDescribe));
		ch_desc->esd_url = esd_url ? gf_strdup(esd_url) : NULL;
		ch_desc->opid = opid;

		com->user_data = ch_desc;
	} else {
		//always accept both SDP and IOD
		com->Accept = gf_strdup("application/sdp, application/mpeg4-iod");
//		com->Accept = gf_strdup("application/sdp");
	}

	/*need better tuning ...*/
	if (sess->rtpin->bandwidth)
		com->Bandwidth = sess->rtpin->bandwidth;

	rtpin_rtsp_queue_command(sess, NULL, com, GF_FALSE);
}


static void rtpin_rtsp_skip_command(GF_RTPInStream *stream)
{
	u32 i;
	GF_RTPInStream *a_st;
	if (!stream || (stream->flags & RTP_SKIP_NEXT_COM) || !(stream->rtsp->flags & RTSP_AGG_CONTROL) ) return;
	i=0;
	while ((a_st = (GF_RTPInStream *)gf_list_enum(stream->rtpin->streams, &i))) {
		if ((stream == a_st) || (a_st->rtsp != stream->rtsp) ) continue;
		if (a_st->status>=RTP_Connected)
			a_st->flags |= RTP_SKIP_NEXT_COM;
	}
}


/*
 						channel control functions
																*/
/*remove command if session is using aggregated control*/
Bool rtpin_rtsp_usercom_preprocess(GF_RTPInRTSP *sess, GF_RTSPCommand *com)
{
	RTPIn_StreamControl *ch_ctrl;
	GF_RTPInStream *stream;
	GF_Err e;
	Bool skip_it;

	ch_ctrl = NULL;
	if (strcmp(com->method, GF_RTSP_TEARDOWN)) ch_ctrl = (RTPIn_StreamControl *)com->user_data;
	if (!ch_ctrl || !ch_ctrl->stream) return GF_TRUE;
	stream = ch_ctrl->stream;

	if (!sess->satip) {
		if (!stream->opid || !rtpin_stream_is_valid(sess->rtpin, stream)) {
			gf_free(ch_ctrl);
			com->user_data = NULL;
			return GF_FALSE;
		}

		assert(stream->rtsp == sess);
		assert(stream->opid == ch_ctrl->evt.base.on_pid);
	}

	skip_it = GF_FALSE;
	if (!com->Session) {
		/*re-SETUP failed*/
		if (!strcmp(com->method, GF_RTSP_PLAY) || !strcmp(com->method, GF_RTSP_PAUSE)) {
			e = GF_SERVICE_ERROR;
			goto err_exit;
		}
		/*this is a stop, no need for SessionID just skip*/
		skip_it = GF_TRUE;
	} else {
		rtpin_rtsp_skip_command(stream);
	}

	/*check if aggregation discards this command*/
	if (skip_it || ( (sess->flags & RTSP_AGG_CONTROL) && (stream->flags & RTP_SKIP_NEXT_COM) )) {
		stream->flags &= ~RTP_SKIP_NEXT_COM;
		gf_free(ch_ctrl);
		com->user_data = NULL;
		return GF_FALSE;
	}
	return GF_TRUE;

err_exit:
	gf_rtsp_reset_aggregation(stream->rtsp->session);
	stream->status = RTP_Disconnected;
	stream->check_rtp_time = RTP_SET_TIME_NONE;

	GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSP] Error processing event %s: %s\n", gf_filter_event_name(ch_ctrl->evt.base.type), gf_error_to_string(e) ));

	gf_free(ch_ctrl);
	com->user_data = NULL;
	return GF_FALSE;
}

void rtpin_rtsp_usercom_process(GF_RTPInRTSP *sess, GF_RTSPCommand *com, GF_Err e)
{
	RTPIn_StreamControl *ch_ctrl;
	GF_RTPInStream *stream, *agg_st;
	u32 i, count;
	GF_RTPInfo *info;

	ch_ctrl = (RTPIn_StreamControl *)com->user_data;
	stream = ch_ctrl->stream;

	if (stream) {
		if (!stream->opid || !rtpin_stream_is_valid(sess->rtpin, stream)) {
			gf_free(ch_ctrl);
			com->user_data = NULL;
			return;
		}

		assert(stream->opid==ch_ctrl->evt.base.on_pid);
	}

	/*some consistency checking: on interleaved sessions, some servers do NOT reply to the
	teardown. If our command is STOP just skip the error notif*/
	if (e) {
		if (!strcmp(com->method, GF_RTSP_TEARDOWN)) {
			goto process_reply;
		} else {
			/*spec is not really clear about what happens if the server doesn't support
			non aggregated operations. Since this happen only on pause/play, we consider
			that no error occured and wait for next play*/
			if (sess->rtsp_rsp->ResponseCode == NC_RTSP_Only_Aggregate_Operation_Allowed) {
				sess->flags |= RTSP_AGG_ONLY;
				sess->rtsp_rsp->ResponseCode = NC_RTSP_OK;
			} else {
				goto err_exit;
			}
		}
	}

	switch (sess->rtsp_rsp->ResponseCode) {
	//handle all 3xx codes  (redirections)
	case NC_RTSP_Method_Not_Allowed:
		e = GF_NOT_SUPPORTED;
		goto err_exit;
	case NC_RTSP_OK:
		break;
	default:
		//we should have a basic error code mapping here
		e = GF_SERVICE_ERROR;
		goto err_exit;
	}

process_reply:

	if ( (ch_ctrl->evt.base.type==GF_FEVT_PLAY)
	        || (ch_ctrl->evt.base.type==GF_FEVT_SET_SPEED)
	        || (ch_ctrl->evt.base.type==GF_FEVT_RESUME) ) {

		//auto-detect any aggregated control if not done yet
		if (gf_list_count(sess->rtsp_rsp->RTP_Infos) > 1) {
			sess->flags |= RTSP_AGG_CONTROL;
		}

		//process all RTP infos
		count = gf_list_count(sess->rtsp_rsp->RTP_Infos);
		for (i=0; i<count; i++) {
			info = (GF_RTPInfo*)gf_list_get(sess->rtsp_rsp->RTP_Infos, i);
			agg_st = rtpin_find_stream(sess->rtpin, NULL, 0, info->url, GF_FALSE);

			if (!agg_st || (agg_st->rtsp != sess) ) continue;
			/*channel is already playing*/
			if (agg_st->status == RTP_Running) {
				gf_rtp_set_info_rtp(agg_st->rtp_ch, info->seq, info->rtp_time, info->ssrc);
				agg_st->check_rtp_time = RTP_SET_TIME_RTP;
				continue;
			}

			/*if play/seeking we must send update RTP/NPT link*/
			if (ch_ctrl->evt.base.type != GF_FEVT_RESUME) {
				agg_st->check_rtp_time = RTP_SET_TIME_RTP;
			}
			/*this is used to discard RTP packets re-sent on resume*/
			else {
				agg_st->check_rtp_time = RTP_SET_TIME_RTP_SEEK;
			}
			/* reset the buffers */
			rtpin_stream_init(agg_st, GF_TRUE);

			gf_rtp_set_info_rtp(agg_st->rtp_ch, info->seq, info->rtp_time, info->ssrc);
			agg_st->status = RTP_Running;

			/*skip next play command on this channel if aggregated control*/
			if ((stream != agg_st) && stream && (stream->rtsp->flags & RTSP_AGG_CONTROL) ) agg_st->flags |= RTP_SKIP_NEXT_COM;


			if (gf_rtp_is_interleaved(agg_st->rtp_ch)) {
				gf_rtsp_register_interleave(sess->session,
				                            agg_st,
				                            gf_rtp_get_low_interleave_id(agg_st->rtp_ch),
				                            gf_rtp_get_hight_interleave_id(agg_st->rtp_ch));
			}
		}
		/*no rtp info (just in case), no time mapped - set to 0 and specify we're not interactive*/
		if (stream && !i) {
			stream->current_start = 0.0;
			stream->check_rtp_time = RTP_SET_TIME_RTP;
			rtpin_stream_init(stream, GF_TRUE);
			stream->status = RTP_Running;
			if (gf_rtp_is_interleaved(stream->rtp_ch)) {
				gf_rtsp_register_interleave(sess->session,
				                            stream, gf_rtp_get_low_interleave_id(stream->rtp_ch), gf_rtp_get_hight_interleave_id(stream->rtp_ch));
			}
		}
		if (stream) stream->flags &= ~RTP_SKIP_NEXT_COM;
	} else if (ch_ctrl->evt.base.type == GF_FEVT_PAUSE) {
		if (stream) {
			rtpin_rtsp_skip_command(stream);
			stream->flags &= ~RTP_SKIP_NEXT_COM;

		}
	} else if (ch_ctrl->evt.base.type == GF_FEVT_STOP) {
		sess->rtpin->eos_probe_start = gf_sys_clock();
		if (stream)
			stream->flags |= RTP_EOS;
	}
	gf_free(ch_ctrl);
	com->user_data = NULL;
	return;


err_exit:
	if (stream) {
		stream->status = RTP_Disconnected;
		stream->flags |= RTP_EOS;
		gf_rtsp_reset_aggregation(stream->rtsp->session);
		stream->check_rtp_time = RTP_SET_TIME_NONE;
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSP] Error processing user command %s\n", gf_error_to_string(e) ));
	gf_free(ch_ctrl);
	com->user_data = NULL;
	if (sess->flags & RTSP_TCP_FLUSH) {
		sess->rtpin->eos_probe_start = gf_sys_clock();
	}
}


void rtpin_rtsp_usercom_send(GF_RTPInRTSP *sess, GF_RTPInStream *stream, const GF_FilterEvent *evt)
{
	GF_RTPInStream *a_st;
	RTPIn_StreamControl *ch_ctrl;
	u32 i;
	Bool needs_setup = GF_FALSE;
	GF_RTSPCommand *com;
	GF_RTSPRange *range;

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
	case GF_FEVT_RESUME:
		needs_setup = GF_TRUE;
		break;
	case GF_FEVT_PAUSE:
	case GF_FEVT_STOP:
		break;
	//TODO
	case GF_FEVT_SET_SPEED:
	default:
		return;
	}


	/*we may need to re-setup stream/session*/
	if (needs_setup) {
		if (stream->status == RTP_Disconnected) {
			if (sess->flags & RTSP_AGG_CONTROL) {
				i=0;
				while ((a_st = (GF_RTPInStream *)gf_list_enum(sess->rtpin->streams, &i))) {
					if (a_st->rtsp != sess) continue;
					if (a_st->status == RTP_Disconnected)
						rtpin_rtsp_setup_send(a_st);
				}
			} else {
				rtpin_rtsp_setup_send(stream);
			}
		}
	}

	com	= gf_rtsp_command_new();
	range = NULL;

	if ( (evt->base.type == GF_FEVT_PLAY) || (evt->base.type == GF_FEVT_RESUME) ) {

		range = gf_rtsp_range_new();
		range->start = stream->range_start;
		range->end = stream->range_end;

		com->method = gf_strdup(GF_RTSP_PLAY);

		stream->paused = GF_FALSE;

		/*specify pause range on resume - this is not mandatory but most servers need it*/
		if (evt->base.type == GF_FEVT_RESUME) {
			range->start = stream->current_start;

			stream->stat_start_time -= stream->stat_stop_time;
			stream->stat_start_time += gf_sys_clock();
			stream->stat_stop_time = 0;
		} else {
			range->start = stream->range_start;
			if (evt->play.start_range>=0) range->start += evt->play.start_range;
			range->end = stream->range_start;
			if (evt->play.end_range >=0) {
				range->end += evt->play.end_range;
				if (range->end > stream->range_end) range->end = stream->range_end;
			}

			stream->stat_start_time = gf_sys_clock();
			stream->stat_stop_time = 0;
		}
		/*if aggregated the command is sent once, so store info at session level*/
		if (stream->flags & RTP_SKIP_NEXT_COM) {
			stream->current_start = stream->rtsp->last_range;
		} else {
			stream->rtsp->last_range = range->start;
			stream->current_start = range->start;
		}
		/*some RTSP servers don't accept Range=npt:0.0- (for ex, broadcast only...), so skip it if:
		- a range was given in initial describe
		- the command is not a RESUME
		*/
		if (!(stream->flags & RTP_HAS_RANGE) && (evt->base.type != GF_FEVT_RESUME) ) {
			gf_rtsp_range_del(range);
			com->Range = NULL;
		} else {
			com->Range = range;
		}

		if (sess->flags & RTSP_AGG_CONTROL)
			rtpin_rtsp_skip_command(stream);
		else if (strlen(stream->control))
			com->ControlString = gf_strdup(stream->control);

		if (rtpin_rtsp_is_active(stream)) {
			if (!com->ControlString && stream->control) com->ControlString = gf_strdup(stream->control);
		} else {
			if (com->ControlString) {
				gf_free(com->ControlString);
				com->ControlString=NULL;
			}
		}

	} else if (evt->base.type == GF_FEVT_PAUSE) {
		com->method = gf_strdup(GF_RTSP_PAUSE);
		if (stream) {
			range = gf_rtsp_range_new();
			/*update current time*/
			stream->current_start += gf_rtp_get_current_time(stream->rtp_ch);
			stream->stat_stop_time = gf_sys_clock();
			range->start = stream->current_start;
			range->end = -1.0;
			com->Range = range;

			if (sess->flags & RTSP_AGG_CONTROL)
				rtpin_rtsp_skip_command(stream);
			else if (strlen(stream->control))
				com->ControlString = gf_strdup(stream->control);

			stream->paused = GF_TRUE;
		}
	}
	else if (evt->base.type == GF_FEVT_STOP) {
		stream->current_start = 0;
		stream->stat_stop_time = gf_sys_clock();

		stream->status = RTP_Connected;
		rtpin_stream_init(stream, GF_TRUE);

		/*if server only support aggregation on pause, skip the command or issue
		a teardown if last active stream*/
		if (stream->rtsp->flags & RTSP_AGG_ONLY) {
			rtpin_stream_stop(stream);
			if (com) gf_rtsp_command_del(com);
			if (!rtpin_rtsp_is_active(stream))
				rtpin_rtsp_teardown(sess, stream);
			return;
		}
		/* otherwise send a PAUSE on the stream */
		else {
			if (stream->paused) {
				if (com) gf_rtsp_command_del(com);
				return;
			}
			range = gf_rtsp_range_new();
			range->start = 0;
			range->end = -1;
			com->method = gf_strdup(GF_RTSP_PAUSE);
			com->Range = range;
			/*only pause the specified stream*/
			if (stream->control) com->ControlString = gf_strdup(stream->control);
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSP] Unsupported command %s\n", gf_filter_event_name(evt->base.type) ));
		gf_rtsp_command_del(com);
		return;
	}

	ch_ctrl = (RTPIn_StreamControl *)gf_malloc(sizeof(RTPIn_StreamControl));
	ch_ctrl->stream = stream;
	memcpy(&ch_ctrl->evt, evt, sizeof(GF_FilterEvent));
	com->user_data = ch_ctrl;

	rtpin_rtsp_queue_command(sess, stream, com, GF_TRUE);
	return;
}


/*
 						session/channel teardown functions
																*/
void rtpin_rtsp_teardown_process(GF_RTPInRTSP *sess, GF_RTSPCommand *com, GF_Err e)
{
	GF_RTPInStream *stream = (GF_RTPInStream *)com->user_data;
	if (stream) {
		if (stream->session_id) gf_free(stream->session_id);
		stream->session_id = NULL;
	} else {
		if (sess->session_id) gf_free(sess->session_id);
		sess->session_id = NULL;
	}
}

void rtpin_rtsp_teardown(GF_RTPInRTSP *sess, GF_RTPInStream *stream)
{
	GF_RTSPCommand *com;

	/*we need a session id*/
	if (!sess->session_id) return;
	/*ignore teardown on channels*/
	if ((sess->flags & RTSP_AGG_CONTROL) && stream) return;

	com = gf_rtsp_command_new();
	com->method = gf_strdup(GF_RTSP_TEARDOWN);
	/*this only works in RTSP2*/
	if (stream && stream->control) {
		com->ControlString = gf_strdup(stream->control);
		com->user_data = stream;
	}

	rtpin_rtsp_queue_command(sess, stream, com, GF_TRUE);
}

#endif /*GPAC_DISABLE_STREAMING*/
