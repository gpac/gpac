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

static void RT_LoadPrefs(GF_InputService *plug, RTPClient *rtp)
{
	const char *sOpt;

	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Streaming", "DefaultPort");
	if (sOpt) {
		rtp->default_port = atoi(sOpt);
	} else {
		rtp->default_port = 554;
	}
	if ((rtp->default_port == 80) || (rtp->default_port == 8080))
		gf_modules_set_option((GF_BaseInterface *)plug, "Streaming", "RTPoverRTSP", "yes");
	
	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Streaming", "RTPoverRTSP");
	if (sOpt && !stricmp(sOpt, "yes")) {
		rtp->rtp_mode = 1;
		/*HTTP tunnel*/
		if ((rtp->default_port==80) || (rtp->default_port==8080)) rtp->rtp_mode = 2;
	} else {
		rtp->rtp_mode = 0;
	}


	/*
		get heneral network config for UDP
	*/
	/*if UDP not available don't try it*/
	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Network", "UDPNotAvailable");
	if (!rtp->rtp_mode && sOpt && !stricmp(sOpt, "yes")) rtp->rtp_mode = 1;
	
	if (!rtp->rtp_mode) {
		sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Network", "UDPTimeout");
		if (sOpt ) {
			rtp->udp_time_out = atoi(sOpt);
		} else {
			rtp->udp_time_out = 10000;
		}
	}

	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Network", "BufferLength");
	if (sOpt) {
		rtp->buffer_len_ms = atoi(sOpt);
	} else {
		rtp->buffer_len_ms = 1000;
	}
	rtp->rebuffer_len_ms = 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Network", "RebufferLength");
	if (sOpt) rtp->rebuffer_len_ms = atoi(sOpt);
	
	rtp->reorder_size = 0;
	if (!rtp->rtp_mode) {
		sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Streaming", "ReorderSize");
		if (sOpt) rtp->reorder_size = atoi(sOpt);
	}
	if (!rtp->reorder_size) rtp->reorder_size = 10;

	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Streaming", "RTSPTimeout");
	if (sOpt ) {
		rtp->time_out = atoi(sOpt);
	} else {
		rtp->time_out = 30000;
	}

	/*packet drop emulation*/
	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Streaming", "FirstPacketDrop");
	if (sOpt) {
		rtp->first_packet_drop = atoi(sOpt);
	} else {
		rtp->first_packet_drop = 0;
	}
	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Streaming", "PacketDropFrequency");
	if (sOpt) {
		rtp->frequency_drop = atoi(sOpt);
	} else {
		rtp->frequency_drop = 0;
	}
	
	sOpt = gf_modules_get_option((GF_BaseInterface *)plug, "Streaming", "LogFile");
	if (sOpt && strlen(sOpt)) {
		if (!stricmp(sOpt, "stdout"))
			rtp->logs = stdout;
		else
			rtp->logs = fopen(sOpt, "wt");
	} else {
		rtp->logs = NULL;
	}

	rtp->handle_announce = 0;
	rtp->bandwidth = 10000000;
}

static void RP_close_service_thread(RTPClient *rtp)
{
	while (gf_list_count(rtp->channels)) {
		RTPStream *ch = gf_list_get(rtp->channels, 0);
		gf_list_rem(rtp->channels, 0);
		RP_DeleteStream(ch);
	}

	if (rtp->rtsp_session) 
		RP_RemoveSession(rtp->rtsp_session, 1);

	rtp->rtsp_session = NULL;
	
	if (rtp->session_desc) gf_odf_desc_del(rtp->session_desc);
	rtp->session_desc = NULL;

	if (rtp->sdp_temp) {
		free(rtp->sdp_temp->remote_url);
		free(rtp->sdp_temp);
	}
	rtp->sdp_temp = NULL;
	if (rtp->od_au) free(rtp->od_au);
	rtp->od_au = NULL;
	rtp->od_au_size = 0;

	if (rtp->do_exit==1) gf_term_on_disconnect(rtp->service, NULL, GF_OK);
	rtp->run_client = 0;
}

u32 RP_Thread(void *param)
{
	u32 i, nb_inter;
	GF_NetworkCommand com;
	RTPStream *ch;
	RTPClient *rtp = param;

	com.command_type = GF_NET_CHAN_BUFFER_QUERY;
	while (rtp->run_client) {
		gf_mx_p(rtp->mx);

		/*fecth data on udp*/
		nb_inter = 0;
		i=0;
		while ((ch = gf_list_enum(rtp->channels, &i))) {
			if ((ch->flags & CH_EOS) || (ch->status!=RTP_Running) ) continue;
			/*for interleaved channels don't read too fast, query the buffer occupancy*/
			if (ch->flags & CH_IsInterleaved) {
				com.base.on_channel = ch->channel;
				gf_term_on_command(rtp->service, &com, GF_OK);
				/*if no buffering, use a default value (3 sec of data should do it)*/
				if (!com.buffer.max) com.buffer.max = 3000;
				if (com.buffer.occupancy <= com.buffer.max) nb_inter++;
			} else {
				RP_ReadStream(ch);
			}
		}
		gf_mx_v(rtp->mx);
		
		/*and process commands / flush TCP*/
		if (rtp->rtsp_session) RP_ProcessCommands(rtp->rtsp_session, nb_inter);

		if (rtp->do_exit) RP_close_service_thread(rtp);

		gf_sleep(1);
	}

	if (rtp->dnload) gf_term_download_del(rtp->dnload);
	rtp->dnload = NULL;

	rtp->client_exit = 1;
	return 0;
}


static Bool RP_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt = strrchr(url, '.');

	if (sExt && gf_term_check_extension(plug, "application/sdp", "sdp", "OnDemand Media/Multicast Session", sExt)) return 1;

	/*local */
	if (strstr(url, "data:application/sdp")) return 1;
	/*embedded data*/
	if (strstr(url, "data:application/mpeg4-od-au;base64") || 
		strstr(url, "data:application/mpeg4-bifs-au;base64") ||
		strstr(url, "data:application/mpeg4-es-au;base64")) return 1;

	/*we need rtsp/tcp , rtsp/udp or direct RTP sender (no control)*/
	if (!strnicmp(url, "rtsp://", 7) || !strnicmp(url, "rtspu://", 8) || !strnicmp(url, "rtp://", 6)) return 1;
	/*we don't check extensions*/
	return 0;
}

static GF_Err RP_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	RTPSession *sess;
	RTPClient *priv = plug->priv;

	/*store user address*/
	priv->service = serv;

	if (priv->dnload) gf_term_download_del(priv->dnload);
	priv->dnload = NULL;


	/*load preferences*/
	RT_LoadPrefs(plug, priv);

	/*start thread*/
	priv->run_client = 1;
	gf_th_run(priv->th, RP_Thread, priv);

	/*local or remote SDP*/
	if (strstr(url, "data:application/sdp") || (strnicmp(url, "rtsp", 4) && strstr(url, ".sdp")) ) {
		RP_FetchSDP(plug, (char *) url, NULL);
	}
	/*rtsp and rtsp over udp*/
	else if (!strnicmp(url, "rtsp://", 7) || !strnicmp(url, "rtspu://", 8)) {
		char *the_url = strdup(url);
		char *the_ext = strrchr(the_url, '#');
		if (the_ext) {
			if (!stricmp(the_ext, "#audio")) priv->forced_type = GF_STREAM_AUDIO;
			else if (!stricmp(the_ext, "#video")) priv->forced_type = GF_STREAM_VISUAL;
			the_ext[0] = 0;
		}
		sess = RP_NewSession(priv, (char *) the_url);
		free(the_url);
		if (!sess) {
			gf_term_on_connect(serv, NULL, GF_NOT_SUPPORTED);
		} else {
			RP_Describe(sess, 0, NULL);
		}
	}
	/*direct RTP (no control) or embedded data - this means the service is attached to a single channel (no IOD)
	reply right away*/
	else {
		gf_term_on_connect(serv, NULL, GF_OK);
	}
	return GF_OK;
}

static GF_Err RP_CloseService(GF_InputService *plug)
{
	RTPClient *priv = plug->priv;
	priv->do_exit = 1;
	return GF_OK;
}

static GF_Descriptor *RP_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	GF_Descriptor *desc;
	RTPClient *priv = plug->priv;

	if ((expect_type!=GF_MEDIA_OBJECT_UNDEF) && (expect_type!=GF_MEDIA_OBJECT_SCENE)) {
		/*ignore the SDP IOD and regenerate one*/
		if (priv->session_desc) gf_odf_desc_del(priv->session_desc);
		priv->session_desc = NULL;
		return RP_EmulateIOD(priv, expect_type, sub_url);
	}

	if (!priv->session_desc) 
		return RP_EmulateIOD(priv, expect_type, sub_url);

	gf_odf_desc_copy(priv->session_desc, &desc);
	return desc;
}

static GF_Err RP_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	u32 ESID;
	RTPStream *ch;
	RTPSession *sess;
	char *es_url;
	RTPClient *priv = plug->priv;
	if (upstream) return GF_NOT_SUPPORTED;


	ch = RP_FindChannel(priv, channel, 0, (char *) url, 0);
	if (ch && (ch->status != RTP_Disconnected) ) return GF_SERVICE_ERROR;

	es_url = NULL;
	sess = NULL;
	if (strstr(url, "ES_ID=")) {
		sscanf(url, "ES_ID=%d", &ESID);
		if (ESID==priv->od_es_id) {
			priv->od_ch = channel;
			gf_term_on_connect(priv->service, channel, GF_OK);
			return GF_OK;
		} else {
			/*first case: simple URL (same namespace)*/
			ch = RP_FindChannel(priv, NULL, ESID, NULL, 0);
			/*this should not happen, the sdp must describe all streams in the service*/
			if (!ch) return GF_SERVICE_ERROR;
			
			/*assign app channel*/
			ch->channel = channel;
			sess = ch->rtsp;
		}
	}
	/*rtsp url - create a session if needed*/
	else if (!strnicmp(url, "rtsp://", 7) || !strnicmp(url, "rtspu://", 8)) {
		sess = RP_CheckSession(priv, (char *) url);
		if (!sess) sess = RP_NewSession(priv, (char *) url);
		es_url = (char *) url;
	}
	/*data: url*/
	else if (strstr(url, "data:application/mpeg4-od-au;base64") 
		|| strstr(url, "data:application/mpeg4-bifs-au;base64")
		|| strstr(url, "data:application/mpeg4-es-au;base64")
		) {
		
		ch = malloc(sizeof(RTPStream));
		memset(ch, 0, sizeof(RTPStream));
		ch->control = strdup(url);
		ch->owner = priv;
		ch->channel = channel;
		ch->status = RTP_Connected;
		/*register*/
		gf_list_add(priv->channels, ch);
		RP_ConfirmChannelConnect(ch, GF_OK);

		return GF_OK;
	}
	
	/*send a DESCRIBE (not a setup) on the channel. If the channel is already created then the
	describe is skipped and a SETUP is sent directly, otherwise the channel is first described then setup*/
	if (sess) RP_Describe(sess, es_url, channel);
	/*otherwise confirm channel connection*/
	else RP_ConfirmChannelConnect(ch, GF_OK);

	return GF_OK;
}


static GF_Err RP_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	RTPStream *ch;
	RTPClient *priv = plug->priv;

	if (priv->od_ch==channel) {
		priv->od_ch = NULL;
		gf_term_on_disconnect(priv->service, channel, GF_OK);
		return GF_OK;
	}
	ch = RP_FindChannel(priv, channel, 0, NULL, 0);
	if (!ch) return GF_STREAM_NOT_FOUND;

	gf_mx_p(priv->mx);
	/*disconnect stream BUT DO NOT DELETE IT since we don't store SDP*/
	RP_DisconnectStream(ch);
	gf_mx_v(priv->mx);
	gf_term_on_disconnect(priv->service, channel, GF_OK);
	return GF_OK;
}

static GF_Err RP_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	RTPStream *ch;
	RTPClient *priv = plug->priv;

	/*ignore commands other than channels one*/
	if (!com->base.on_channel) {
		if (com->command_type==GF_NET_IS_CACHABLE) return GF_OK;
		return GF_NOT_SUPPORTED;
	}

	if (priv->od_ch==com->base.on_channel) {
		switch (com->command_type) {
		case GF_NET_CHAN_SET_PULL: return GF_OK;
		case GF_NET_CHAN_BUFFER: com->buffer.max = com->buffer.min = 0; return GF_OK;
		case GF_NET_CHAN_DURATION: com->duration.duration = 0; return GF_OK;
		case GF_NET_CHAN_PLAY:
			priv->od_fetched = 0;
			priv->od_start_time = (u32) (com->play.start_range*1000);
			return GF_OK;
		case GF_NET_CHAN_STOP:
			priv->od_start_time = 0;
			break;
		}
		return GF_OK;
	}

	ch = RP_FindChannel(priv, com->base.on_channel, 0, NULL, 0);
	if (!ch) return GF_STREAM_NOT_FOUND;

	switch (com->command_type) {
	case GF_NET_CHAN_SET_PULL:
		if (ch->rtp_ch || ch->rtsp || !ch->control) return GF_NOT_SUPPORTED;
		/*embedded channels work in pull mode*/
		if (strstr(ch->control, "data:application/")) return GF_OK;
		return GF_NOT_SUPPORTED;
	case GF_NET_CHAN_INTERACTIVE:
		/*looks like pure RTP / multicast etc, not interactive*/
		if (!ch->control) return GF_NOT_SUPPORTED;
		return GF_OK;
	case GF_NET_CHAN_BUFFER:
		if (!(ch->rtp_ch || ch->rtsp || !ch->control)) {
		  com->buffer.max = com->buffer.min = 0;
		} else {
		  com->buffer.max = priv->buffer_len_ms;
		  com->buffer.min = priv->rebuffer_len_ms;
		  if (priv->rebuffer_len_ms>=priv->buffer_len_ms) com->buffer.min = 0;
		}
		return GF_OK;
	case GF_NET_CHAN_DURATION:
		com->duration.duration = (ch->flags & CH_HasRange) ? (ch->range_end - ch->range_start) : 0;
		return GF_OK;
	/*RTP channel config is done upon connection, once the complete SL mapping is known
	however we must store some info not carried in SDP*/
	case GF_NET_CHAN_CONFIG:
		ch->clock_rate = com->cfg.sample_rate ? com->cfg.sample_rate : com->cfg.sl_config.timestampResolution;
		if (!ch->clock_rate) ch->clock_rate = 1000;
		if (com->cfg.frame_duration) ch->unit_duration = com->cfg.frame_duration;
		return GF_OK;

	case GF_NET_CHAN_PLAY:
		/*is this RTSP or direct RTP?*/
		ch->flags &= ~CH_EOS;
		if (ch->rtsp) {
			RP_UserCommand(ch->rtsp, ch, com);
		} else {
			ch->status = RTP_Running;
			if (ch->rtp_ch) {
				ch->check_rtp_time = 1;
				RP_InitStream(ch, 0);
				gf_rtp_set_info_rtp(ch->rtp_ch, 0, 0, 0);
			} else {
				/*direct channel, store current start*/
				ch->current_start = com->play.start_range;
				ch->flags |= CH_NewAU;
			}
		}
		return GF_OK;
	case GF_NET_CHAN_STOP:
		/*is this RTSP or direct RTP?*/
		if (ch->rtsp) {
			RP_UserCommand(ch->rtsp, ch, com);
		} else {
			ch->status = RTP_Connected;
		}
		return GF_OK;
	case GF_NET_CHAN_SET_SPEED:
	case GF_NET_CHAN_PAUSE:
	case GF_NET_CHAN_RESUME:
		assert(ch->rtsp);
		RP_UserCommand(ch->rtsp, ch, com);
		return GF_OK;

	case GF_NET_CHAN_GET_DSI:
		if (ch->sl_map.configSize) {
			com->get_dsi.dsi_len = ch->sl_map.configSize;
			com->get_dsi.dsi = malloc(sizeof(char)*com->get_dsi.dsi_len);
			memcpy(com->get_dsi.dsi, ch->sl_map.config, sizeof(char)*com->get_dsi.dsi_len);
		} else {
			com->get_dsi.dsi = NULL;
			com->get_dsi.dsi_len = 0;
		}
		return GF_OK;

	
	case GF_NET_GET_STATS:
		memset(&com->net_stats, 0, sizeof(GF_NetComStats));
		if (ch->rtp_ch) {
			u32 time;
			Float bps;
			com->net_stats.pck_loss_percentage = gf_rtp_get_loss(ch->rtp_ch);
			if (ch->flags & CH_IsInterleaved) {
				com->net_stats.multiplex_port = gf_rtsp_get_session_port(ch->rtsp->session);
				com->net_stats.port = gf_rtp_get_low_interleave_id(ch->rtp_ch);
				com->net_stats.ctrl_port = gf_rtp_get_hight_interleave_id(ch->rtp_ch);
			} else {
				com->net_stats.multiplex_port = 0;
				gf_rtp_get_ports(ch->rtp_ch, &com->net_stats.port, &com->net_stats.ctrl_port);
			}
			if (ch->stat_stop_time) {
				time = ch->stat_stop_time - ch->stat_start_time;
			} else {
				time = gf_sys_clock() - ch->stat_start_time;
			}
			bps = 8.0f * ch->rtp_bytes; bps *= 1000; bps /= time; com->net_stats.bw_down = (u32) bps;
			bps = 8.0f * ch->rtcp_bytes; bps *= 1000; bps /= time; com->net_stats.ctrl_bw_down = (u32) bps;
			bps = 8.0f * gf_rtp_get_tcp_bytes_sent(ch->rtp_ch); bps *= 1000; bps /= time; com->net_stats.ctrl_bw_up = (u32) bps;
		}
		return GF_OK;
	}
	return GF_NOT_SUPPORTED;
}

static GF_Err RP_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	char *data;
	RTPStream *ch;
	RTPClient *priv = plug->priv;

	if (priv->od_ch==channel) {
		*sl_compressed = 0;
		memset(out_sl_hdr, 0, sizeof(GF_SLHeader));
		out_sl_hdr->accessUnitEndFlag = 1;
		out_sl_hdr->accessUnitStartFlag = 1;
		out_sl_hdr->compositionTimeStamp = priv->od_start_time;
		out_sl_hdr->compositionTimeStampFlag = 1;
		out_sl_hdr->randomAccessPointFlag = 1;
		if (priv->od_fetched) {
			*out_reception_status = GF_EOS;
		} else {
			*out_reception_status = GF_OK;
			*is_new_data = 1;
			*out_data_ptr = priv->od_au;
			*out_data_size = priv->od_au_size;
		}
		return GF_OK;
	}

	ch = RP_FindChannel(priv, channel, 0, NULL, 0);
	if (!ch) return GF_STREAM_NOT_FOUND;
	if (ch->rtp_ch || ch->rtsp || !ch->control) return GF_SERVICE_ERROR;
	if (ch->status != RTP_Running) return GF_SERVICE_ERROR;
	data = strstr(ch->control, ";base64");
	if (!data) return GF_SERVICE_ERROR;

	if (ch->current_start>=0) {
		*sl_compressed = 0;
		memset(out_sl_hdr, 0, sizeof(GF_SLHeader));
		out_sl_hdr->accessUnitEndFlag = 1;
		out_sl_hdr->accessUnitStartFlag = 1;
		out_sl_hdr->compositionTimeStamp = (u64) (ch->current_start * ch->clock_rate);
		out_sl_hdr->compositionTimeStampFlag = 1;
		out_sl_hdr->randomAccessPointFlag = 1;
		*out_reception_status = GF_OK;
		*is_new_data = (ch->flags & CH_NewAU) ? 1 : 0;

		/*decode data*/
		data = strstr(data, ",");
		data += 1;
		*out_data_size = gf_base64_decode(data, strlen(data), ch->buffer, RTP_BUFFER_SIZE);
		/*FIXME - currently only support for empty SL header*/
		*out_data_ptr = ch->buffer;
		ch->flags &= ~CH_NewAU;
	} else {
		*out_data_ptr = NULL;
		*out_data_size = 0;
		*out_reception_status = GF_EOS;
		ch->flags |= CH_EOS;
	}
	return GF_OK;
}

static GF_Err RP_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	RTPStream *ch;
	RTPClient *priv = plug->priv;
	if (priv->od_ch==channel) {
		priv->od_fetched = 1;
		return GF_OK;
	}
	ch = RP_FindChannel(priv, channel, 0, NULL, 0);
	if (!ch) return GF_STREAM_NOT_FOUND;
	if (ch->rtp_ch || ch->rtsp || !ch->control) return GF_SERVICE_ERROR;
	if (ch->status != RTP_Running) return GF_SERVICE_ERROR;

	/*this will trigger EOS at next fetch*/
	ch->current_start = -1.0;
	return GF_OK;
}

static Bool RP_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	RTPSession *sess;
	RTPClient *priv = plug->priv;

	if (strstr(url, "data:application/mpeg4-od-au;base64") 
		|| strstr(url, "data:application/mpeg4-bifs-au;base64")
		|| strstr(url, "data:application/mpeg4-es-au;base64")
		) return 1;

	if (!RP_CanHandleURL(plug, url)) return 0;
	/*if this URL is part of a running session then ok*/
	sess = RP_CheckSession(priv, (char *) url);
	if (sess) return 1;
	return 0;
}


GF_InputService *RTP_Load()
{
	RTPClient *priv;
	GF_InputService *plug = malloc(sizeof(GF_InputService));
	memset(plug, 0, sizeof(GF_InputService));
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC RTP/RTSP Client", "gpac distribution")

	plug->CanHandleURL = RP_CanHandleURL;
	plug->CanHandleURLInService = RP_CanHandleURLInService;
	plug->ConnectService = RP_ConnectService;
	plug->CloseService = RP_CloseService;
	plug->GetServiceDescriptor = RP_GetServiceDesc;
	plug->ConnectChannel = RP_ConnectChannel;
	plug->DisconnectChannel = RP_DisconnectChannel;
	plug->ServiceCommand = RP_ServiceCommand;

	/*PULL mode for embedded streams*/
	plug->ChannelGetSLP = RP_ChannelGetSLP;
	plug->ChannelReleaseSLP = RP_ChannelReleaseSLP;

	priv = malloc(sizeof(RTPClient));
	memset(priv, 0, sizeof(RTPClient));
	priv->channels = gf_list_new();

	plug->priv = priv;
	
	priv->time_out = 30000;
	priv->buffer_len_ms = 5000;
	priv->rebuffer_len_ms = 500;

	priv->mx = gf_mx_new();
	priv->th = gf_th_new();
	
	return plug;
}

void RTP_Delete(GF_BaseInterface *bi)
{
	RTPClient *priv;
	u32 retry;
	GF_InputService *plug = (GF_InputService *) bi;
	priv = plug->priv;
	if (priv->run_client) {
		if (!priv->do_exit) priv->do_exit = 2;
		retry = 20;
		while (!priv->client_exit && retry) {
			gf_sleep(10);
			retry--;
		}
		assert(retry);
	}

	/*delete all streams*/
	while (gf_list_count(priv->channels)) {
		RTPStream *ch = gf_list_get(priv->channels, 0);
		gf_list_rem(priv->channels, 0);
		RP_DeleteStream(ch);
	}

	if (priv->rtsp_session) RP_RemoveSession(priv->rtsp_session, 1);
	if (priv->session_desc) gf_odf_desc_del(priv->session_desc);
	priv->session_desc = NULL;
	if (priv->sdp_temp) {
		free(priv->sdp_temp->remote_url);
		free(priv->sdp_temp);
	}

	if (priv->logs && (priv->logs != stdout) ) fclose(priv->logs);

	gf_list_del(priv->channels);
	gf_th_del(priv->th);
	gf_mx_del(priv->mx);
	free(priv);
	free(bi);
}

Bool QueryInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return 1;
	return 0;
}

GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return (GF_BaseInterface *)RTP_Load();
	return NULL;
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_NET_CLIENT_INTERFACE:
		RTP_Delete(ifce);
		break;
	}
}
