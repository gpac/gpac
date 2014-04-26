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


Bool channel_is_valid(RTPClient *rtp, RTPStream *ch)
{
	u32 i=0;
	RTPStream *st;
	while ((st = (RTPStream *)gf_list_enum(rtp->channels, &i))) {
		if (st == ch) return 1;
	}
	return 0;
}

void RP_StopChannel(RTPStream *ch)
{
	if (!ch || !ch->rtsp) return;

	ch->flags &= ~RTP_SKIP_NEXT_COM;
	//ch->status = RTP_Disconnected;
	//remove interleaved
	if (gf_rtp_is_interleaved(ch->rtp_ch)) {
		gf_rtsp_unregister_interleave(ch->rtsp->session, gf_rtp_get_low_interleave_id(ch->rtp_ch));
	}
}

/*this prevent sending teardown on session with running channels*/
Bool RP_SessionActive(RTPStream *ch)
{
	RTPStream *ach;
	u32 i, count;
	i = count = 0;
	while ((ach = (RTPStream *)gf_list_enum(ch->owner->channels, &i))) {
		if (ach->rtsp != ch->rtsp) continue;
		/*count only active channels*/
		if (ach->status == RTP_Running) count++;
	}
	return count ? 1 : 0;
}

static void RP_QueueCommand(RTSPSession *sess, RTPStream *ch, GF_RTSPCommand *com, Bool needs_sess_id)
{
	if (needs_sess_id) {
		com->Session = sess->session_id;
	}
	if (gf_mx_try_lock(sess->owner->mx)) {
		gf_list_add(sess->rtsp_commands, com);
		gf_mx_v(sess->owner->mx);
	} else {
		gf_list_add(sess->rtsp_commands, com);
	}
}



/*
 						channel setup functions
																*/

void RP_Setup(RTPStream *ch)
{
	u16 def_first_port;
	const char *opt;
	GF_RTSPCommand *com;
	GF_RTSPTransport *trans;

	com = gf_rtsp_command_new();
	com->method = gf_strdup(GF_RTSP_SETUP);

	def_first_port = 0;
	opt = gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(ch->owner->service), "Streaming", "ForceFirstPort");
	if (opt) def_first_port = atoi(opt);

	opt = gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(ch->owner->service), "Streaming", "ForceMulticastIP");

	//setup ports if unicast non interleaved or multicast
	if (gf_rtp_is_unicast(ch->rtp_ch) && (ch->owner->transport_mode != 1) && !gf_rtp_is_interleaved(ch->rtp_ch) ) {
		gf_rtp_set_ports(ch->rtp_ch, def_first_port);
	} else if (opt) {
		gf_rtp_set_ports(ch->rtp_ch, def_first_port);
	}

	trans = gf_rtsp_transport_clone(gf_rtp_get_transport(ch->rtp_ch));

	/*some servers get confused when trying to resetup on the same remote ports, so reset info*/
	trans->port_first = trans->port_last = 0;
	trans->SSRC = 0;

	/*override transport: */
	/*1: multicast forced*/
	opt = gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(ch->owner->service), "Streaming", "ForceMulticastIP");
	if (opt) {
		trans->IsUnicast = 0;
		trans->destination = gf_strdup(opt);
		opt = gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(ch->owner->service), "Streaming", "ForceMulticastTTL");
		trans->TTL = opt ? atoi(opt) : 127;
		if (trans->Profile) gf_free(trans->Profile);
		trans->Profile = gf_strdup(GF_RTSP_PROFILE_RTP_AVP);
		if (!(ch->rtsp->flags & RTSP_DSS_SERVER) ) {
			trans->port_first = trans->client_port_first;
			trans->port_last = trans->client_port_last;
			/*this is correct but doesn't work with DSS: the server expects "client_port" to indicate
			the multicast port, not "port" - this will send both*/
			//trans->client_port_first = trans->client_port_last = 0;
		}
		gf_rtp_setup_transport(ch->rtp_ch, trans, NULL);
	}
	/*2: RTP over RTSP forced*/
	else if (ch->rtsp->flags & RTSP_FORCE_INTER) {
		if (trans->Profile) gf_free(trans->Profile);
		trans->Profile = gf_strdup(GF_RTSP_PROFILE_RTP_AVP_TCP);
		//some servers expect the interleaved to be set during the setup request
		trans->IsInterleaved = 1;
		trans->rtpID = gf_list_find(ch->owner->channels, ch);
		trans->rtcpID = trans->rtpID+1;
		gf_rtp_setup_transport(ch->rtp_ch, trans, NULL);
	}

	if (trans->source) {
		gf_free(trans->source);
		trans->source = NULL;
	}

	/*turn off interleaving in case of re-setup, some servers don't like it (we still signal it
	through RTP/AVP/TCP profile so it's OK)*/
//	trans->IsInterleaved = 0;
	gf_list_add(com->Transports, trans);
	if (strlen(ch->control)) com->ControlString = gf_strdup(ch->control);

	com->user_data = ch;
	ch->status = RTP_WaitingForAck;

	RP_QueueCommand(ch->rtsp, ch, com, 1);
}

/*filter setup if no session (rtp only)*/
GF_Err RP_SetupChannel(RTPStream *ch, ChannelDescribe *ch_desc)
{
	GF_Err resp;

	/*assign ES_ID of the channel*/
	if (ch_desc && !ch->ES_ID && ch_desc->ES_ID) ch->ES_ID = ch_desc->ES_ID;

	ch->status = RTP_Setup;

	/*assign channel handle if not done*/
	if (ch_desc && ch->channel) {
		assert(ch->channel == ch_desc->channel);
	} else if (!ch->channel) {
		assert(ch_desc);
		assert(ch_desc->channel);
		ch->channel = ch_desc->channel;
	}

	/*no session , setup for pure rtp*/
	if (!ch->rtsp) {
		ch->flags |= RTP_CONNECTED;
		/*init rtp*/
		resp = RP_InitStream(ch, 0),
		/*send confirmation to user*/
		RP_ConfirmChannelConnect(ch, resp);
	} else {
		RP_Setup(ch);
	}
	return GF_OK;
}

void RP_ProcessSetup(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e)
{
	RTPStream *ch;
	u32 i;
	GF_RTSPTransport *trans;

	ch = (RTPStream *)com->user_data;
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
	if (!ch) goto exit;

	/*assign session ID*/
	if (!sess->rtsp_rsp->Session) {
		e = GF_SERVICE_ERROR;
		goto exit;
	}
	if (!sess->session_id) sess->session_id = gf_strdup(sess->rtsp_rsp->Session);
	assert(!ch->session_id);

	/*transport setup: break at the first correct transport */
	i=0;
	while ((trans = (GF_RTSPTransport *)gf_list_enum(sess->rtsp_rsp->Transports, &i))) {
		/*copy over previous ports (hack for some servers overriding client ports)*/
		const char *opt = gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(ch->owner->service), "Streaming", "ForceClientPorts");
		if (opt && !stricmp(opt, "yes"))
			gf_rtp_get_ports(ch->rtp_ch, &trans->client_port_first, &trans->client_port_last);

		if (gf_rtp_is_interleaved(ch->rtp_ch) && !trans->IsInterleaved) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTSP] Requested interleaved RTP over RTSP but server did not setup interleave - cannot process command\n"));
			e = GF_REMOTE_SERVICE_ERROR;
			continue;
		}
		e = gf_rtp_setup_transport(ch->rtp_ch, trans, gf_rtsp_get_server_name(sess->session));
		if (!e) break;
	}
	if (e) goto exit;

	e = RP_InitStream(ch, 0);
	if (e) goto exit;
	ch->status = RTP_Connected;

	//in case this is TCP channel, setup callbacks
	ch->flags &= ~RTP_INTERLEAVED;
	if (gf_rtp_is_interleaved(ch->rtp_ch)) {
		ch->flags |= RTP_INTERLEAVED;
		gf_rtsp_set_interleave_callback(sess->session, RP_DataOnTCP);
	}

exit:
	/*confirm only on first connect, otherwise this is a re-SETUP of the rtsp session, not the channel*/
	if (! (ch->flags & RTP_CONNECTED) ) {
		if (!e)
			ch->flags |= RTP_CONNECTED;
		RP_ConfirmChannelConnect(ch, e);
	}
	com->user_data = NULL;
}



/*
 						session/channel describe functions
																*/
/*filter describe commands in case of ESD URLs*/
Bool RP_PreprocessDescribe(RTSPSession *sess, GF_RTSPCommand *com)
{
	RTPStream *ch;
	ChannelDescribe *ch_desc;
	/*not a channel describe*/
	if (!com->user_data) {
		gf_term_on_message(sess->owner->service, GF_OK, "Connecting...");
		return 1;
	}

	ch_desc = (ChannelDescribe *)com->user_data;
	ch = RP_FindChannel(sess->owner, NULL, ch_desc->ES_ID, ch_desc->esd_url, 0);
	if (!ch) return 1;

	/*channel has been described already, skip describe and send setup directly*/
	RP_SetupChannel(ch, ch_desc);

	if (ch_desc->esd_url) gf_free(ch_desc->esd_url);
	gf_free(ch_desc);
	return 0;
}

/*process describe reply*/
GF_Err RP_ProcessDescribe(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e)
{
	RTPStream *ch;
	ChannelDescribe *ch_desc;

	ch = NULL;
	ch_desc = (ChannelDescribe *)com->user_data;
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

	ch = NULL;
	if (ch_desc) {
		ch = RP_FindChannel(sess->owner, ch_desc->channel, ch_desc->ES_ID, ch_desc->esd_url, 0);
	} else {
		gf_term_on_message(sess->owner->service, GF_OK, "Connected");
	}

	/*error on loading SDP is done internally*/
	RP_LoadSDP(sess->owner, sess->rtsp_rsp->body, sess->rtsp_rsp->Content_Length, ch);

	if (!ch_desc) goto exit;
	if (!ch) {
		e = GF_STREAM_NOT_FOUND;
		goto exit;
	}
	e = RP_SetupChannel(ch, ch_desc);

exit:
	com->user_data = NULL;
	if (e) {
		if (!ch_desc) {
			sess->connect_error = e;
			return e;
		} else if (ch) {
			RP_ConfirmChannelConnect(ch, e);
		} else {
			gf_term_on_connect(sess->owner->service, ch_desc->channel, e);
		}
	}
	if (ch_desc) gf_free(ch_desc);
	return GF_OK;
}

/*send describe*/
void RP_Describe(RTSPSession *sess, char *esd_url, LPNETCHANNEL channel)
{
	const char *opt;
	RTPStream *ch;
	ChannelDescribe *ch_desc;
	GF_RTSPCommand *com;

	/*locate the channel by URL - if we have one, this means the channel is already described
	this happens when 2 ESD with URL use the same RTSP service - skip describe and send setup*/
	if (esd_url || channel) {
		ch = RP_FindChannel(sess->owner, channel, 0, esd_url, 0);
		if (ch) {
			if (!ch->channel) ch->channel = channel;
			switch (ch->status) {
			case RTP_Connected:
			case RTP_Running:
				RP_ConfirmChannelConnect(ch, GF_OK);
				return;
			default:
				break;
			}
			ch_desc = (ChannelDescribe *)gf_malloc(sizeof(ChannelDescribe));
			ch_desc->esd_url = esd_url ? gf_strdup(esd_url) : NULL;
			ch_desc->channel = channel;
			RP_SetupChannel(ch, ch_desc);

			if (esd_url) gf_free(ch_desc->esd_url);
			gf_free(ch_desc);
			return;
		}
		/*channel not found, send describe on service*/
	}

	/*send describe*/
	com = gf_rtsp_command_new();
	com->method = gf_strdup(GF_RTSP_DESCRIBE);

	if (channel || esd_url) {
		com->Accept = gf_strdup("application/sdp");
		com->ControlString = esd_url ? gf_strdup(esd_url) : NULL;

		ch_desc = (ChannelDescribe *)gf_malloc(sizeof(ChannelDescribe));
		ch_desc->esd_url = esd_url ? gf_strdup(esd_url) : NULL;
		ch_desc->channel = channel;

		com->user_data = ch_desc;
	} else {
		//always accept both SDP and IOD
		com->Accept = gf_strdup("application/sdp, application/mpeg4-iod");
//		com->Accept = gf_strdup("application/sdp");
	}

	/*need better tuning ...*/
	opt = (char *) gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(sess->owner->service), "Network", "Bandwidth");
	if (opt && !stricmp(opt, "yes")) com->Bandwidth = atoi(opt);

	RP_QueueCommand(sess, NULL, com, 0);
}


static void SkipCommandOnSession(RTPStream *ch)
{
	u32 i;
	RTPStream *a_ch;
	if (!ch || (ch->flags & RTP_SKIP_NEXT_COM) || !(ch->rtsp->flags & RTSP_AGG_CONTROL) ) return;
	i=0;
	while ((a_ch = (RTPStream *)gf_list_enum(ch->owner->channels, &i))) {
		if ((ch == a_ch) || (a_ch->rtsp != ch->rtsp) ) continue;
		if (a_ch->status>=RTP_Connected)
			a_ch->flags |= RTP_SKIP_NEXT_COM;
	}
}


/*
 						channel control functions
																*/
/*remove command if session is using aggregated control*/
Bool RP_PreprocessUserCom(RTSPSession *sess, GF_RTSPCommand *com)
{
	ChannelControl *ch_ctrl;
	RTPStream *ch;
	GF_Err e;
	Bool skip_it;

	ch_ctrl = NULL;
	if (strcmp(com->method, GF_RTSP_TEARDOWN)) ch_ctrl = (ChannelControl *)com->user_data;
	if (!ch_ctrl || !ch_ctrl->ch) return 1;
	ch = ch_ctrl->ch;

	if (!ch->channel || !channel_is_valid(sess->owner, ch)) {
		gf_free(ch_ctrl);
		com->user_data = NULL;
		return 0;
	}

	assert(ch->rtsp == sess);
	assert(ch->channel==ch_ctrl->com.base.on_channel);

	skip_it = 0;
	if (!com->Session) {
		/*re-SETUP failed*/
		if (!strcmp(com->method, GF_RTSP_PLAY) || !strcmp(com->method, GF_RTSP_PAUSE)) {
			e = GF_SERVICE_ERROR;
			goto err_exit;
		}
		/*this is a stop, no need for SessionID just skip*/
		skip_it = 1;
	} else {
		SkipCommandOnSession(ch);
	}

	/*check if aggregation discards this command*/
	if (skip_it || ( (sess->flags & RTSP_AGG_CONTROL) && (ch->flags & RTP_SKIP_NEXT_COM) )) {
		ch->flags &= ~RTP_SKIP_NEXT_COM;
		gf_term_on_command(sess->owner->service, &ch_ctrl->com, GF_OK);
		gf_free(ch_ctrl);
		com->user_data = NULL;
		return 0;
	}
	return 1;

err_exit:
	gf_rtsp_reset_aggregation(ch->rtsp->session);
	ch->status = RTP_Disconnected;
	ch->check_rtp_time = RTP_SET_TIME_NONE;
	gf_term_on_command(sess->owner->service, &ch_ctrl->com, e);
	gf_free(ch_ctrl);
	com->user_data = NULL;
	return 0;
}

void RP_ProcessUserCommand(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e)
{
	ChannelControl *ch_ctrl;
	RTPStream *ch, *agg_ch;
	u32 i, count;
	GF_RTPInfo *info;


	ch_ctrl = (ChannelControl *)com->user_data;
	ch = ch_ctrl->ch;
	if (ch) {
		if (!ch->channel || !channel_is_valid(sess->owner, ch)) {
			gf_free(ch_ctrl);
			com->user_data = NULL;
			return;
		}

		assert(ch->channel==ch_ctrl->com.base.on_channel);
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

	gf_term_on_command(sess->owner->service, &ch_ctrl->com, GF_OK);

	if ( (ch_ctrl->com.command_type==GF_NET_CHAN_PLAY)
	        || (ch_ctrl->com.command_type==GF_NET_CHAN_SET_SPEED)
	        || (ch_ctrl->com.command_type==GF_NET_CHAN_RESUME) ) {

		//auto-detect any aggregated control if not done yet
		if (gf_list_count(sess->rtsp_rsp->RTP_Infos) > 1) {
			sess->flags |= RTSP_AGG_CONTROL;
		}

		//process all RTP infos
		count = gf_list_count(sess->rtsp_rsp->RTP_Infos);
		for (i=0; i<count; i++) {
			info = (GF_RTPInfo*)gf_list_get(sess->rtsp_rsp->RTP_Infos, i);
			agg_ch = RP_FindChannel(sess->owner, NULL, 0, info->url, 0);

			if (!agg_ch || (agg_ch->rtsp != sess) ) continue;
			/*channel is already playing*/
			if (agg_ch->status == RTP_Running) {
				gf_rtp_set_info_rtp(agg_ch->rtp_ch, info->seq, info->rtp_time, info->ssrc);
				agg_ch->check_rtp_time = RTP_SET_TIME_RTP;
				continue;
			}

			/*if play/seeking we must send update RTP/NPT link*/
			if (ch_ctrl->com.command_type != GF_NET_CHAN_RESUME) {
				agg_ch->check_rtp_time = RTP_SET_TIME_RTP;
			}
			/*this is used to discard RTP packets re-sent on resume*/
			else {
				agg_ch->check_rtp_time = RTP_SET_TIME_RTP_SEEK;
			}
			/* reset the buffers */
			RP_InitStream(agg_ch, 1);

			gf_rtp_set_info_rtp(agg_ch->rtp_ch, info->seq, info->rtp_time, info->ssrc);
			agg_ch->status = RTP_Running;

			/*skip next play command on this channel if aggregated control*/
			if (ch!=agg_ch && (ch->rtsp->flags & RTSP_AGG_CONTROL) ) agg_ch->flags |= RTP_SKIP_NEXT_COM;


			if (gf_rtp_is_interleaved(agg_ch->rtp_ch)) {
				gf_rtsp_register_interleave(sess->session,
				                            agg_ch,
				                            gf_rtp_get_low_interleave_id(agg_ch->rtp_ch),
				                            gf_rtp_get_hight_interleave_id(agg_ch->rtp_ch));
			}
		}
		/*no rtp info (just in case), no time mapped - set to 0 and specify we're not interactive*/
		if (!i) {
			ch->current_start = 0.0;
			ch->check_rtp_time = RTP_SET_TIME_RTP;
			RP_InitStream(ch, 1);
			ch->status = RTP_Running;
			if (gf_rtp_is_interleaved(ch->rtp_ch)) {
				gf_rtsp_register_interleave(sess->session,
				                            ch, gf_rtp_get_low_interleave_id(ch->rtp_ch), gf_rtp_get_hight_interleave_id(ch->rtp_ch));
			}
		}
		ch->flags &= ~RTP_SKIP_NEXT_COM;
	} else if (ch_ctrl->com.command_type == GF_NET_CHAN_PAUSE) {
		if (ch) {
			SkipCommandOnSession(ch);
			ch->flags &= ~RTP_SKIP_NEXT_COM;

		}
	} else if (ch_ctrl->com.command_type == GF_NET_CHAN_STOP) {
	}
	gf_free(ch_ctrl);
	com->user_data = NULL;
	return;


err_exit:
	gf_term_on_command(sess->owner->service, &ch_ctrl->com, e);
	if (ch) {
		ch->status = RTP_Disconnected;
		gf_rtsp_reset_aggregation(ch->rtsp->session);
		ch->check_rtp_time = RTP_SET_TIME_NONE;
	}
	gf_free(ch_ctrl);
	com->user_data = NULL;
}

#if 0
static void RP_FlushAndTearDown(RTSPSession *sess)
{
	GF_RTSPCommand *com;
	gf_mx_p(sess->owner->mx);

	while (gf_list_count(sess->rtsp_commands)) {
		com = (GF_RTSPCommand *)gf_list_get(sess->rtsp_commands, 0);
		gf_list_rem(sess->rtsp_commands, 0);
		gf_rtsp_command_del(com);
	}
	if (sess->flags & RTSP_WAIT_REPLY) {
		GF_Err e;
		while (1) {
			e = gf_rtsp_get_response(sess->session, sess->rtsp_rsp);
			if (e!= GF_IP_NETWORK_EMPTY) break;
		}
		sess->flags &= ~RTSP_WAIT_REPLY;
	}
	gf_mx_v(sess->owner->mx);


	/*no private stack on teardown - shutdown now*/
	com	= gf_rtsp_command_new();
	com->method = gf_strdup(GF_RTSP_TEARDOWN);
	RP_QueueCommand(sess, NULL, com, 1);
}
#endif


void RP_UserCommand(RTSPSession *sess, RTPStream *ch, GF_NetworkCommand *command)
{
	RTPStream *a_ch;
	ChannelControl *ch_ctrl;
	u32 i;
	Bool needs_setup = 0;
	GF_RTSPCommand *com;
	GF_RTSPRange *range;

	switch (command->command_type) {
	case GF_NET_CHAN_PLAY:
	case GF_NET_CHAN_RESUME:
		needs_setup = 1;
		break;
	case GF_NET_CHAN_PAUSE:
	case GF_NET_CHAN_STOP:
		break;
	default:
		gf_term_on_command(sess->owner->service, command, GF_NOT_SUPPORTED);
		return;
	}


	/*we may need to re-setup stream/session*/
	if (needs_setup) {
		if (ch->status == RTP_Disconnected) {
			if (sess->flags & RTSP_AGG_CONTROL) {
				i=0;
				while ((a_ch = (RTPStream *)gf_list_enum(sess->owner->channels, &i))) {
					if (a_ch->rtsp != sess) continue;
					if (a_ch->status == RTP_Disconnected)
						RP_Setup(a_ch);
				}
			} else {
				RP_Setup(ch);
			}
		}
	}

	com	= gf_rtsp_command_new();
	range = NULL;

	if ( (command->command_type==GF_NET_CHAN_PLAY) || (command->command_type==GF_NET_CHAN_RESUME) ) {

		range = gf_rtsp_range_new();
		range->start = ch->range_start;
		range->end = ch->range_end;

		com->method = gf_strdup(GF_RTSP_PLAY);

		ch->paused=0;

		/*specify pause range on resume - this is not mandatory but most servers need it*/
		if (command->command_type==GF_NET_CHAN_RESUME) {
			range->start = ch->current_start;

			ch->stat_start_time -= ch->stat_stop_time;
			ch->stat_start_time += gf_sys_clock();
			ch->stat_stop_time = 0;
		} else {
			range->start = ch->range_start;
			if (command->play.start_range>=0) range->start += command->play.start_range;
			range->end = ch->range_start;
			if (command->play.end_range >=0) {
				range->end += command->play.end_range;
				if (range->end > ch->range_end) range->end = ch->range_end;
			}

			ch->stat_start_time = gf_sys_clock();
			ch->stat_stop_time = 0;
		}
		/*if aggregated the command is sent once, so store info at session level*/
		if (ch->flags & RTP_SKIP_NEXT_COM) {
			ch->current_start = ch->rtsp->last_range;
		} else {
			ch->rtsp->last_range = range->start;
			ch->current_start = range->start;
		}
		/*some RTSP servers don't accept Range=npt:0.0- (for ex, broadcast only...), so skip it if:
		- a range was given in initial describe
		- the command is not a RESUME
		*/
		if (!(ch->flags & RTP_HAS_RANGE) && (command->command_type != GF_NET_CHAN_RESUME) ) {
			gf_rtsp_range_del(range);
			com->Range = NULL;
		} else {
			com->Range = range;
		}

		if (sess->flags & RTSP_AGG_CONTROL)
			SkipCommandOnSession(ch);
		else if (strlen(ch->control))
			com->ControlString = gf_strdup(ch->control);

		if (RP_SessionActive(ch)) {
			if (!com->ControlString && ch->control) com->ControlString = gf_strdup(ch->control);
		} else {
			if (com->ControlString) {
				gf_free(com->ControlString);
				com->ControlString=NULL;
			}
		}

	} else if (command->command_type==GF_NET_CHAN_PAUSE) {
		com->method = gf_strdup(GF_RTSP_PAUSE);
		if (ch) {
			range = gf_rtsp_range_new();
			/*update current time*/
			ch->current_start += gf_rtp_get_current_time(ch->rtp_ch);
			ch->stat_stop_time = gf_sys_clock();
			range->start = ch->current_start;
			range->end = -1.0;
			com->Range = range;

			if (sess->flags & RTSP_AGG_CONTROL)
				SkipCommandOnSession(ch);
			else if (strlen(ch->control))
				com->ControlString = gf_strdup(ch->control);

			ch->paused=1;
		}
	}
	else if (command->command_type==GF_NET_CHAN_STOP) {
		ch->current_start = 0;
		ch->stat_stop_time = gf_sys_clock();

		ch->status = RTP_Connected;
		RP_InitStream(ch, 1);

		/*if server only support aggregation on pause, skip the command or issue
		a teardown if last active stream*/
		if (ch->rtsp->flags & RTSP_AGG_ONLY) {
			RP_StopChannel(ch);
			if (com) gf_rtsp_command_del(com);
			if (!RP_SessionActive(ch))
				RP_Teardown(sess, ch);
			return;
		}
		/* otherwise send a PAUSE on the stream */
		else {
			if (ch->paused) {
				if (com) gf_rtsp_command_del(com);
				return;
			}
			range = gf_rtsp_range_new();
			range->start = 0;
			range->end = -1;
			com->method = gf_strdup(GF_RTSP_PAUSE);
			com->Range = range;
			/*only pause the specified stream*/
			if (ch->control) com->ControlString = gf_strdup(ch->control);
		}
	} else {
		gf_term_on_command(sess->owner->service, command, GF_NOT_SUPPORTED);
		gf_rtsp_command_del(com);
		return;
	}

	ch_ctrl = (ChannelControl *)gf_malloc(sizeof(ChannelControl));
	ch_ctrl->ch = ch;
	memcpy(&ch_ctrl->com, command, sizeof(GF_NetworkCommand));
	com->user_data = ch_ctrl;

	RP_QueueCommand(sess, ch, com, 1);
	return;
}


/*
 						session/channel teardown functions
																*/
void RP_ProcessTeardown(RTSPSession *sess, GF_RTSPCommand *com, GF_Err e)
{
	RTPStream *ch = (RTPStream *)com->user_data;
	if (ch) {
		if (ch->session_id) gf_free(ch->session_id);
		ch->session_id = NULL;
	} else {
		if (sess->session_id) gf_free(sess->session_id);
		sess->session_id = NULL;
	}
}

void RP_Teardown(RTSPSession *sess, RTPStream *ch)
{
	GF_RTSPCommand *com;
	if (sess->owner->session_migration) return;
	/*we need a session id*/
	if (!sess->session_id) return;
	/*ignore teardown on channels*/
	if ((sess->flags & RTSP_AGG_CONTROL) && ch) return;

	com = gf_rtsp_command_new();
	com->method = gf_strdup(GF_RTSP_TEARDOWN);
	/*this only works in RTSP2*/
	if (ch && ch->control) {
		com->ControlString = gf_strdup(ch->control);
		com->user_data = ch;
	}

	RP_QueueCommand(sess, ch, com, 1);
}

#endif /*GPAC_DISABLE_STREAMING*/
