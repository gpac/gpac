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


void RP_ConfirmChannelConnect(RTPStream *ch, GF_Err e)
{
	GF_NetworkCommand com;
	
	/*in case the channel has been disconnected while SETUP was issued&processed. We also could
	clean up the command stack*/
	if (!ch->channel) return;

	gf_term_on_connect(ch->owner->service, ch->channel, e);
	if (e != GF_OK || !ch->rtp_ch) return;

	/*success, overwrite SL config*/
	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.command_type = GF_NET_CHAN_RECONFIG;
	com.base.on_channel = ch->channel;

	gf_rtp_depacketizer_get_slconfig(ch->depacketizer, &com.cfg.sl_config);
	/*reconfig*/
	gf_term_on_command(ch->owner->service, &com, GF_OK);

	/*ISMACryp config*/
	if (ch->depacketizer->flags & GF_RTP_HAS_ISMACRYP) {
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.on_channel = ch->channel;
		com.command_type = GF_NET_CHAN_DRM_CFG;
		com.drm_cfg.scheme_type = ch->depacketizer->isma_scheme;
		com.drm_cfg.scheme_version = 1;
		/*not transported in SDP!!!*/
		com.drm_cfg.scheme_uri = NULL;
		com.drm_cfg.kms_uri = ch->depacketizer->key;
		gf_term_on_command(ch->owner->service, &com, GF_OK);
	}
}


GF_Err RP_InitStream(RTPStream *ch, Bool ResetOnly)
{
	gf_rtp_depacketizer_reset(ch->depacketizer, !ResetOnly);

	if (!ResetOnly) {
		const char *mcast_ifce = NULL;
		u32 reorder_size = 0;
		if (!ch->owner->transport_mode) {
			const char *sOpt = gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(ch->owner->service), "Streaming", "ReorderSize");
			if (sOpt) reorder_size = atoi(sOpt);
			else reorder_size = 10;

		
			mcast_ifce = gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(ch->owner->service), "Streaming", "DefaultMCastInterface");
		}
		return gf_rtp_initialize(ch->rtp_ch, RTP_BUFFER_SIZE, 0, 0, reorder_size, 200, (char *)mcast_ifce);
	}
	//just reset the sockets
	gf_rtp_reset_buffers(ch->rtp_ch);
	return GF_OK;
}

void RP_DisconnectStream(RTPStream *ch)
{
	/*no check for teardown, this is done at STOP stage*/
#if 0
	ch->status = RTP_Disconnected;
	ch->flags &= ~RTP_CONNECTED;
#endif
	ch->channel = NULL;
}

void RP_DeleteStream(RTPStream *ch)
{
	if (ch->rtsp) {
		if ((ch->status == RTP_Running)) {
			RP_Teardown(ch->rtsp, ch);
			ch->status = RTP_Disconnected;
		}
		RP_RemoveStream(ch->owner, ch);
	} else {
		RP_FindChannel(ch->owner, ch->channel, 0, NULL, 1);
	}

	if (ch->depacketizer) gf_rtp_depacketizer_del(ch->depacketizer);
	if (ch->rtp_ch) gf_rtp_del(ch->rtp_ch);
	if (ch->control) free(ch->control);
	if (ch->session_id) free(ch->session_id);
	free(ch);
}


static void rtp_sl_packet_cbk(void *udta, char *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	RTPStream *ch = (RTPStream *)udta;
	if (ch->owner->first_packet_drop && (hdr->packetSequenceNumber >= ch->owner->first_packet_drop) ) {
		if ( (hdr->packetSequenceNumber - ch->owner->first_packet_drop) % ch->owner->frequency_drop)
			gf_term_on_sl_packet(ch->owner->service, ch->channel, payload, size, hdr, e);
	} else {
		gf_term_on_sl_packet(ch->owner->service, ch->channel, payload, size, hdr, e);
	}
}


RTPStream *RP_NewStream(RTPClient *rtp, GF_SDPMedia *media, GF_SDPInfo *sdp, RTPStream *input_stream)
{
	GF_RTSPRange *range;
	RTPStream *tmp;
	GF_RTPMap *map;
	u32 i, ESID;
	Bool force_bcast = 0;
	Double Start, End;
	GF_X_Attribute *att;
	char *ctrl;
	GF_SDPConnection *conn;
	GF_RTSPTransport trans;

	//extract all relevant info from the GF_SDPMedia
	Start = 0.0;
	End = -1.0;
	ESID = 0;
	ctrl = NULL;
	range = NULL;
	i=0;
	while ((att = (GF_X_Attribute*)gf_list_enum(media->Attributes, &i))) {
		if (!stricmp(att->Name, "control")) ctrl = att->Value;
		else if (!stricmp(att->Name, "gpac-broadcast")) force_bcast = 1;
		else if (!stricmp(att->Name, "mpeg4-esid") && att->Value) ESID = atoi(att->Value);
		else if (!stricmp(att->Name, "range") && !range) range = gf_rtsp_range_parse(att->Value);
	}

	if (range) {
		Start = range->start;
		End = range->end;
		gf_rtsp_range_del(range);
	}

	/*check connection*/
	conn = sdp->c_connection;
	if (!conn) conn = (GF_SDPConnection*)gf_list_get(media->Connections, 0);

	if (!conn) {
		/*RTSP RFC recommends an empty "c= " line but some server don't send it. Use session info (o=)*/
		if (!sdp->o_net_type || !sdp->o_add_type || strcmp(sdp->o_net_type, "IN")) return NULL;
		if (strcmp(sdp->o_add_type, "IP4") && strcmp(sdp->o_add_type, "IP6")) return NULL;
	} else {
		if (strcmp(conn->net_type, "IN")) return NULL;
		if (strcmp(conn->add_type, "IP4") && strcmp(conn->add_type, "IP6")) return NULL;
	}
	/*do we support transport*/
	if (strcmp(media->Profile, "RTP/AVP") && strcmp(media->Profile, "RTP/AVP/TCP")
		&& strcmp(media->Profile, "RTP/SAVP") && strcmp(media->Profile, "RTP/SAVP/TCP")
		) return NULL; 

	/*check RTP map. For now we only support 1 RTPMap*/
	if (media->fmt_list || (gf_list_count(media->RTPMaps) > 1)) return NULL;

	/*check payload type*/
	map = (GF_RTPMap*)gf_list_get(media->RTPMaps, 0);

	/*this is an ESD-URL setup, we likely have namespace conflicts so overwrite given ES_ID
	by the app one (client side), but keep control (server side) if provided*/
	if (input_stream) {
		ESID = input_stream->ES_ID;
		if (!ctrl) ctrl = input_stream->control;
		tmp = input_stream;
	} else {
		tmp = RP_FindChannel(rtp, NULL, ESID, NULL, 0);
		if (tmp) return NULL;

		GF_SAFEALLOC(tmp, RTPStream);
		tmp->owner = rtp;
	}

	/*create an RTP channel*/
	tmp->rtp_ch = gf_rtp_new();
	if (ctrl) tmp->control = strdup(ctrl);
	tmp->ES_ID = ESID;

	memset(&trans, 0, sizeof(GF_RTSPTransport));
	trans.Profile = media->Profile;
	trans.source = conn ? conn->host : sdp->o_address;
	trans.IsUnicast = gf_sk_is_multicast_address(trans.source) ? 0 : 1;
	if (!trans.IsUnicast) {
		trans.port_first = media->PortNumber;
		trans.port_last = media->PortNumber + 1;
		trans.TTL = conn ? conn->TTL : 0;
	} else {
		trans.client_port_first = media->PortNumber;
		trans.client_port_last = media->PortNumber + 1;
	}

	if (gf_rtp_setup_transport(tmp->rtp_ch, &trans, NULL) != GF_OK) {
		RP_DeleteStream(tmp);
		return NULL;
	}
	/*setup depacketizer*/
	tmp->depacketizer = gf_rtp_depacketizer_new(media, rtp_sl_packet_cbk, tmp);
	if (!tmp->depacketizer) {
		RP_DeleteStream(tmp);
		return NULL;
	}
	/*setup channel*/
	gf_rtp_setup_payload(tmp->rtp_ch, map);

//	tmp->status = NM_Disconnected;

	ctrl = (char *) gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(rtp->service), "Streaming", "DisableRTCP");
	if (!ctrl || stricmp(ctrl, "yes")) tmp->flags |= RTP_ENABLE_RTCP;

	/*setup NAT keep-alive*/
	ctrl = (char *) gf_modules_get_option((GF_BaseInterface *) gf_term_get_service_interface(rtp->service), "Streaming", "NATKeepAlive");
	if (ctrl) gf_rtp_enable_nat_keepalive(tmp->rtp_ch, atoi(ctrl));
	
	tmp->range_start = Start;
	tmp->range_end = End;
	if (End != -1.0) tmp->flags |= RTP_HAS_RANGE;

	if (force_bcast) tmp->flags |= RTP_FORCE_BROADCAST;
	return tmp;
}




void RP_ProcessRTP(RTPStream *ch, char *pck, u32 size)
{
	GF_NetworkCommand com;
	GF_Err e;
	GF_RTPHeader hdr;
	u32 PayloadStart;
	ch->rtp_bytes += size;

	/*first decode RTP*/
	e = gf_rtp_decode_rtp(ch->rtp_ch, pck, size, &hdr, &PayloadStart);

	/*corrupted or NULL data*/
	if (e || (PayloadStart >= size)) {
		//gf_term_on_sl_packet(ch->owner->service, ch->channel, NULL, 0, NULL, GF_CORRUPTED_DATA);
		return;
	}

	/*if we must notify some timing, do it now. If the channel has no range, this should NEVER be called*/
	if (ch->check_rtp_time /*&& gf_rtp_is_active(ch->rtp_ch)*/) {
		Double ch_time = gf_rtp_get_current_time(ch->rtp_ch);

		/*this is the first packet on the channel (no PAUSE)*/
		if (ch->check_rtp_time == 1) {
			/*Note: in a SEEK with RTSP, the rtp-info time given by the server is 
			the rtp time of the desired range. But the server may (and should) send from
			the previous I frame on video, so the time of the first rtp packet after
			a SEEK can actually be less than CurrentStart. We don't drop these
			packets in order to see the maximum video. We could drop it, this would mean 
			wait for next RAP...*/

			memset(&com, 0, sizeof(com));
			com.command_type = GF_NET_CHAN_MAP_TIME;
			com.base.on_channel = ch->channel;
			com.map_time.media_time = ch->current_start + ch_time;
			com.map_time.timestamp = hdr.TimeStamp;
			com.map_time.reset_buffers = 1;
			gf_term_on_command(ch->owner->service, &com, GF_OK);

			if (ch->depacketizer->payt==GF_RTP_PAYT_H264_AVC) ch->depacketizer->flags |= GF_RTP_AVC_WAIT_RAP;
		}
		/*this is RESUME on channel, filter packet based on time (darwin seems to send
		couple of packet before)
		do not fetch if we're below 10 ms or <0, because this means we already have
		this packet - as the PAUSE is issued with the RTP currentTime*/
		else if (ch_time <= 0.021) {
			return;
		}
		ch->check_rtp_time = 0;
	}
	
	gf_rtp_depacketizer_process(ch->depacketizer, &hdr, pck + PayloadStart, size - PayloadStart);

	/*last check: signal EOS if we're close to end range in case the server do not send RTCP BYE*/
	if ((ch->flags & RTP_HAS_RANGE) && !(ch->flags & RTP_EOS) ) {
		/*also check last CTS*/
		Double ts = (Double) ((u32) ch->depacketizer->sl_hdr.compositionTimeStamp - hdr.TimeStamp);
		ts /= gf_rtp_get_clockrate(ch->rtp_ch);
		if (ABSDIFF(ch->range_end, (ts + ch->current_start + gf_rtp_get_current_time(ch->rtp_ch)) ) < 0.2) {
			ch->flags |= RTP_EOS;
			ch->stat_stop_time = gf_sys_clock();
			gf_term_on_sl_packet(ch->owner->service, ch->channel, NULL, 0, NULL, GF_EOS);
		}
	}
}

void RP_ProcessRTCP(RTPStream *ch, char *pck, u32 size)
{
	GF_Err e;

	if (ch->status == RTP_Connected) return;

	ch->rtcp_bytes += size;

	e = gf_rtp_decode_rtcp(ch->rtp_ch, pck, size);
	
	if (e == GF_EOS) {
		ch->flags |= RTP_EOS;
		ch->stat_stop_time = gf_sys_clock();
		gf_term_on_sl_packet(ch->owner->service, ch->channel, NULL, 0, NULL, GF_EOS);
	}
}

GF_Err RP_DataOnTCP(GF_RTSPSession *sess, void *cbk, char *buffer, u32 bufferSize, Bool IsRTCP)
{
	RTPStream *ch = (RTPStream *) cbk;
	if (!ch) return GF_OK;
	if (IsRTCP) {
		RP_ProcessRTCP(ch, buffer, bufferSize);
	} else {
		RP_ProcessRTP(ch, buffer, bufferSize);
	}
	return GF_OK;
}


static GF_Err SendTCPData(void *par, char *pck, u32 pck_size)
{
	return GF_OK;
}


void RP_ReadStream(RTPStream *ch)
{
	u32 size, tot_size;

	if (!ch->rtp_ch) return;

	/*NOTE: A weird bug on windows wrt to select(): if both RTP and RTCP are in the same loop
	there is a hudge packet drop on RTP. We therefore split RTP and RTCP reading, this is not a big
	deal as the RTCP traffic is far less than RTP, and we should never have more than one RTCP
	packet reading per RTP reading loop
	NOTE2: a better implementation would be to use select() to get woken up...
	*/

	tot_size = 0;
	while (1) {
		size = gf_rtp_read_rtp(ch->rtp_ch, ch->buffer, RTP_BUFFER_SIZE);
		if (!size) break;
		tot_size += size;
		RP_ProcessRTP(ch, ch->buffer, size);
	}

	while (1) {
		size = gf_rtp_read_rtcp(ch->rtp_ch, ch->buffer, RTP_BUFFER_SIZE);
		if (!size) break;
		tot_size += size;
		RP_ProcessRTCP(ch, ch->buffer, size);
	}

	/*and send the report*/
	if (ch->flags & RTP_ENABLE_RTCP) gf_rtp_send_rtcp_report(ch->rtp_ch, SendTCPData, ch);
	
	if (tot_size) ch->owner->udp_time_out = 0;

	/*detect timeout*/
	if (ch->owner->udp_time_out) {
		if (!ch->last_udp_time) {
			ch->last_udp_time = gf_sys_clock();
		} else {
			u32 diff = gf_sys_clock() - ch->last_udp_time;
			if (diff >= ch->owner->udp_time_out) {
				char szMessage[1024];
				sprintf(szMessage, "No data recieved in %d ms", diff);
				gf_term_on_message(ch->owner->service, GF_IP_UDP_TIMEOUT, szMessage);
				ch->status = RTP_Unavailable;
			}
		}
	}
}

