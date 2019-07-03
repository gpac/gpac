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

void rtpin_stream_ack_connect(GF_RTPInStream *stream, GF_Err e)
{
	/*in case the channel has been disconnected while SETUP was issued&processed. We also could
	clean up the command stack*/
	if (!stream->opid) return;

	if (e != GF_OK || !stream->rtp_ch) return;
}

GF_Err rtpin_stream_init(GF_RTPInStream *stream, Bool ResetOnly)
{
	gf_rtp_depacketizer_reset(stream->depacketizer, !ResetOnly);

	if (!ResetOnly) {
		GF_Err e;
		const char *ip_ifce = NULL;
		if (!stream->rtpin->interleave) {
			ip_ifce = stream->rtpin->ifce;
		}
		if (stream->rtp_ch->rtp)
			gf_sk_group_unregister(stream->rtpin->sockgroup, stream->rtp_ch->rtp);
		if (stream->rtp_ch->rtcp)
			gf_sk_group_unregister(stream->rtpin->sockgroup, stream->rtp_ch->rtcp);

		e = gf_rtp_initialize(stream->rtp_ch, stream->rtpin->block_size, GF_FALSE, 0, stream->rtpin->reorder_len, stream->rtpin->reorder_delay, (char *)ip_ifce);
		if (e) return e;

		if (stream->rtp_ch->rtp)
			gf_sk_group_register(stream->rtpin->sockgroup, stream->rtp_ch->rtp);
		if (stream->rtp_ch->rtcp)
			gf_sk_group_register(stream->rtpin->sockgroup, stream->rtp_ch->rtcp);

	}
	//just reset the sockets
	gf_rtp_reset_buffers(stream->rtp_ch);
	return GF_OK;
}

void rtpin_stream_reset_queue(GF_RTPInStream *stream)
{
	if (!stream->pck_queue) return;
	while (gf_list_count(stream->pck_queue)) {
		GF_FilterPacket *pck = gf_list_pop_back(stream->pck_queue);
		gf_filter_pck_discard(pck);
	}
}

void rtpin_stream_del(GF_RTPInStream *stream)
{
	if (stream->rtsp) {
		if (stream->status == RTP_Running) {
			rtpin_rtsp_teardown(stream->rtsp, stream);
			stream->status = RTP_Disconnected;
		}
		rtpin_remove_stream(stream->rtpin, stream);
	} else {
		rtpin_find_stream(stream->rtpin, stream->opid, 0, NULL, GF_TRUE);
	}

	if (stream->depacketizer) gf_rtp_depacketizer_del(stream->depacketizer);
	if (stream->rtp_ch) gf_rtp_del(stream->rtp_ch);
	if (stream->control) gf_free(stream->control);
	if (stream->session_id) gf_free(stream->session_id);
	if (stream->buffer) gf_free(stream->buffer);
	if (stream->pck_queue) {
		rtpin_stream_reset_queue(stream);
		gf_list_del(stream->pck_queue);
	}
	gf_free(stream);
}

static void rtpin_stream_queue_pck(GF_RTPInStream *stream, GF_FilterPacket *pck, u64 cts)
{
	//get all packets in queue, dispatch all packets with a cts less than the cts of the packet being queued
	//if this is teh first packet in the RTP packet
	//This is only used for MPEG4, and we are sure we only have [AU(n),AU(n+i)..], [AU(n+j), AU(n+k) ...]
	//with i,j,k >0
	while (stream->first_in_rtp_pck) {
		u64 prev_cts;
		GF_FilterPacket *prev = gf_list_get(stream->pck_queue, 0);
		if (!prev) break;
		prev_cts = gf_filter_pck_get_cts(prev);
		if (prev_cts>cts) break;
		gf_list_rem(stream->pck_queue, 0);
		gf_filter_pck_send(prev);
	}
	stream->first_in_rtp_pck = GF_FALSE;
	gf_list_add(stream->pck_queue, pck);
}

static void rtp_sl_packet_cbk(void *udta, u8 *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	u64 cts, dts;
	s64 diff;
	GF_FilterPacket *pck;
	u8 *pck_data;
	GF_RTPInStream *stream = (GF_RTPInStream *)udta;


	if (!stream->rtcp_init) {
		if (!stream->rtcp_check_start) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP] No RTCP packet received yet, synchronization might be wrong for a while\n"));
			stream->rtcp_check_start = gf_sys_clock();
		}
		else if (gf_sys_clock() - stream->rtcp_check_start <= stream->rtpin->rtcp_timeout) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTP] No RTCP packet received yet, synchronization might be wrong for a while\n"));
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTP] Timeout for RTCP: no SR recevied after %d ms - sync may be broken\n", stream->rtpin->rtcp_timeout));
			stream->rtcp_init = GF_TRUE;
		}
	}

	if (stream->rtpin->first_packet_drop && (hdr->packetSequenceNumber >= stream->rtpin->first_packet_drop) ) {
		if (! ( (hdr->packetSequenceNumber - stream->rtpin->first_packet_drop) % stream->rtpin->frequency_drop) )
			return;
	}

	cts = hdr->compositionTimeStamp;
	dts = hdr->decodingTimeStamp;

	hdr->seekFlag = 0;
	hdr->compositionTimeStamp += stream->ts_adjust;
	if (stream->first_rtp_ts) {
		assert(hdr->compositionTimeStamp >= stream->first_rtp_ts - 1);
		hdr->compositionTimeStamp -= stream->first_rtp_ts - 1;
	}

	if (hdr->decodingTimeStamp) {
		hdr->decodingTimeStamp += stream->ts_adjust;
		if (stream->first_rtp_ts) {
			assert(hdr->decodingTimeStamp >= stream->first_rtp_ts - 1);
			hdr->decodingTimeStamp -= stream->first_rtp_ts - 1;
		}
	}

	pck = gf_filter_pck_new_alloc(stream->opid, size, &pck_data);
	memcpy(pck_data, payload, size);
	if (hdr->decodingTimeStampFlag)
		gf_filter_pck_set_dts(pck, hdr->decodingTimeStamp);

	gf_filter_pck_set_cts(pck, hdr->compositionTimeStamp);

	diff = (s64) hdr->compositionTimeStamp - (s64) stream->prev_cts;
	if ((stream->rtpin->max_sleep>0) && stream->prev_cts && diff) {
		if (diff<0) diff = -diff;
		if (!stream->min_dur_rtp || (diff < stream->min_dur_rtp)) {
			u64 dur = diff;
			dur *= 1000;
			dur /= stream->rtp_ch->TimeScale;
			stream->min_dur_rtp = (u32) dur;
			if (dur > stream->rtpin->max_sleep) dur = stream->rtpin->max_sleep;
			if (!stream->rtpin->min_frame_dur_ms || ( (u32) dur < stream->rtpin->min_frame_dur_ms)) {
				stream->rtpin->min_frame_dur_ms = (u32) dur;
			}
		}
	}
	stream->prev_cts = (u32) hdr->compositionTimeStamp;

	gf_filter_pck_set_sap(pck, hdr->randomAccessPointFlag ? GF_FILTER_SAP_1  :GF_FILTER_SAP_NONE);
	if (hdr->au_duration)
		gf_filter_pck_set_duration(pck, hdr->au_duration);

	if (hdr->samplerate && (hdr->samplerate != stream->sr)) {
		stream->sr = hdr->samplerate;
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(stream->sr));
	}
	if (hdr->channels && (hdr->channels != stream->nb_ch)) {
		stream->nb_ch = hdr->channels;
		gf_filter_pid_set_property(stream->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(stream->nb_ch));
	}
	gf_filter_pck_set_framing(pck, hdr->accessUnitStartFlag, hdr->accessUnitEndFlag);

	if (stream->rtp_ch->packet_loss)
		gf_filter_pck_set_corrupted(pck, 1);

	if (hdr->seekFlag)
		gf_filter_pck_set_seek_flag(pck, GF_TRUE);

	if (stream->depacketizer->sl_map.IndexDeltaLength) {
		rtpin_stream_queue_pck(stream, pck, hdr->compositionTimeStamp);
	} else {
		gf_filter_pck_send(pck);
	}

	hdr->compositionTimeStamp = cts;
	hdr->decodingTimeStamp = dts;
}

GF_RTPInStream *rtpin_stream_new_satip(GF_RTPIn *rtp, const char *server_ip)
{
	GF_RTSPTransport trans;
	GF_RTPInStream *tmp;
	GF_SAFEALLOC(tmp, GF_RTPInStream);
	if (!tmp) return NULL;
	tmp->rtpin = rtp;
	tmp->buffer = gf_malloc(sizeof(char) * rtp->block_size);

	/*create an RTP channel*/
	tmp->rtp_ch = gf_rtp_new();
	tmp->control = gf_strdup("*");

	memset(&trans, 0, sizeof(GF_RTSPTransport));
	trans.Profile = "RTP/AVP";
	trans.source = gf_strdup(server_ip);
	trans.IsUnicast = GF_TRUE;
	trans.client_port_first = 0;
	trans.client_port_last = 0;
	trans.port_first = 0;
	trans.port_last = 0;

	if (gf_rtp_setup_transport(tmp->rtp_ch, &trans, NULL) != GF_OK) {
		rtpin_stream_del(tmp);
		return NULL;
	}

	gf_rtp_setup_payload(tmp->rtp_ch, 33, 90000);

	if (rtp->disable_rtcp) tmp->flags |= RTP_ENABLE_RTCP;

	/*setup NAT keep-alive*/
	gf_rtp_enable_nat_keepalive(tmp->rtp_ch, rtp->nat_keepalive ? rtp->nat_keepalive : 30000);

	tmp->range_start = 0;
	tmp->range_end = 0;

	return tmp;
}

GF_RTPInStream *rtpin_stream_new(GF_RTPIn *rtp, GF_SDPMedia *media, GF_SDPInfo *sdp, GF_RTPInStream *input_stream)
{
	GF_RTSPRange *range;
	GF_RTPInStream *tmp;
	GF_RTPMap *map;
	u32 i, ESID, ODID;
	Bool force_bcast = GF_FALSE;
	Double Start, End;
	u16 rvc_predef = 0;
	char *rvc_config_att = NULL;
	u32 s_port_first, s_port_last;
	GF_X_Attribute *att;
	char *ctrl;
	GF_SDPConnection *conn;
	GF_RTSPTransport trans;
	u32 mid, prev_stream, base_stream;

	//extract all relevant info from the GF_SDPMedia
	Start = 0.0;
	End = -1.0;
	ODID = 0;
	ESID = 0;
	ctrl = NULL;
	range = NULL;
	s_port_first = s_port_last = 0;
	mid = prev_stream = base_stream = 0;
	i=0;
	while ((att = (GF_X_Attribute*)gf_list_enum(media->Attributes, &i))) {
		if (!stricmp(att->Name, "control")) ctrl = att->Value;
		else if (!stricmp(att->Name, "gpac-broadcast")) force_bcast = GF_TRUE;
		else if (!stricmp(att->Name, "mpeg4-esid") && att->Value) ESID = atoi(att->Value);
		else if (!stricmp(att->Name, "mpeg4-odid") && att->Value) ODID = atoi(att->Value);
		else if (!stricmp(att->Name, "range") && !range) range = gf_rtsp_range_parse(att->Value);
		else if (!stricmp(att->Name, "rvc-config-predef")) {
			rvc_predef = atoi(att->Value);
		} else if (!stricmp(att->Name, "rvc-config")) {
			rvc_config_att = att->Value;
		} else if (!stricmp(att->Name, "mid")) {
			sscanf(att->Value, "L%d", &mid);
		} else if (!stricmp(att->Name, "depend")) {
			char buf[3000];
			memset(buf, 0, 3000);
			sscanf(att->Value, "%*d lay L%d %*s %s", &base_stream, buf);
			if (!strlen(buf))
				sscanf(att->Value, "%*d lay %s", buf);
			sscanf(buf, "L%d", &prev_stream);
		}
	}

	if (range) {
		Start = range->start;
		End = range->end;
		gf_rtsp_range_del(range);
	}

	/*check connection*/
	conn = sdp->c_connection;
	if (conn && (!conn->host || !strcmp(conn->host, "0.0.0.0"))) conn = NULL;

	if (!conn) conn = (GF_SDPConnection*)gf_list_get(media->Connections, 0);
	if (conn && (!conn->host || !strcmp(conn->host, "0.0.0.0"))) conn = NULL;

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
	if (gf_list_count(media->RTPMaps) > 1) return NULL;

	/*check payload type*/
	map = (GF_RTPMap*)gf_list_get(media->RTPMaps, 0);
	if (!map) {
		if (!media->fmt_list) return NULL;
		if (strchr(media->fmt_list, ' ')) return NULL;
	}

	/*this is an ESD-URL setup, we likely have namespace conflicts so overwrite given ES_ID
	by the app one (client side), but keep control (server side) if provided*/
	if (input_stream) {
		ESID = input_stream->ES_ID;
		if (!ctrl) ctrl = input_stream->control;
		tmp = input_stream;
	} else {
		tmp = rtpin_find_stream(rtp, NULL, ESID, NULL, GF_FALSE);
		if (tmp) return NULL;

		GF_SAFEALLOC(tmp, GF_RTPInStream);
		if (!tmp) return NULL;
		tmp->rtpin = rtp;
		tmp->buffer = gf_malloc(sizeof(char) * rtp->block_size);
	}

	/*create an RTP channel*/
	tmp->rtp_ch = gf_rtp_new();
	if (ctrl) tmp->control = gf_strdup(ctrl);
	tmp->ES_ID = ESID;
	tmp->OD_ID = ODID;
	tmp->mid = mid;
	tmp->prev_stream = prev_stream;
	tmp->base_stream = base_stream;

	memset(&trans, 0, sizeof(GF_RTSPTransport));
	trans.Profile = media->Profile;
	trans.source = conn ? conn->host : sdp->o_address;
	trans.IsUnicast = gf_sk_is_multicast_address(trans.source) ? GF_FALSE : GF_TRUE;
	if (!trans.IsUnicast) {
		trans.port_first = media->PortNumber;
		trans.port_last = media->PortNumber + 1;
		trans.TTL = conn ? conn->TTL : 0;
	} else {
		trans.client_port_first = media->PortNumber;
		trans.client_port_last = media->PortNumber + 1;
		trans.port_first = s_port_first ? s_port_first : trans.client_port_first;
		trans.port_last = s_port_last ? s_port_last : trans.client_port_last;
	}

	if (gf_rtp_setup_transport(tmp->rtp_ch, &trans, NULL) != GF_OK) {
		rtpin_stream_del(tmp);
		return NULL;
	}
	/*setup depacketizer*/
	tmp->depacketizer = gf_rtp_depacketizer_new(media, rtp_sl_packet_cbk, tmp);
	if (!tmp->depacketizer) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTP] Failed to create RTP depacketizer for payload type %d/%s - ignoring stream)\n", map ? map->PayloadType : 0, media->fmt_list ? media->fmt_list : ""));
		rtpin_stream_del(tmp);
		return NULL;
	}
	/*setup channel*/
	gf_rtp_setup_payload(tmp->rtp_ch, map ? map->PayloadType : tmp->depacketizer->payt, tmp->depacketizer->clock_rate);

	if (tmp->depacketizer->sl_map.IndexDeltaLength) {
		tmp->pck_queue = gf_list_new();
	}

//	tmp->status = NM_Disconnected;

	if (!rtp->disable_rtcp) tmp->flags |= RTP_ENABLE_RTCP;

	/*setup NAT keep-alive*/
	if (rtp->nat_keepalive) gf_rtp_enable_nat_keepalive(tmp->rtp_ch, rtp->nat_keepalive);

	tmp->range_start = Start;
	tmp->range_end = End;
	if (End != -1.0) tmp->flags |= RTP_HAS_RANGE;

	if (force_bcast) tmp->flags |= RTP_FORCE_BROADCAST;

	if (rvc_predef) {
		tmp->depacketizer->sl_map.rvc_predef = rvc_predef ;
	} else if (rvc_config_att) {
		char *rvc_data=NULL;
		u32 rvc_size;
		Bool is_gz = GF_FALSE;
		if (!strncmp(rvc_config_att, "data:application/rvc-config+xml", 32) && strstr(rvc_config_att, "base64") ) {
			char *data = strchr(rvc_config_att, ',');
			if (data) {
				rvc_size = (u32) strlen(data) * 3 / 4 + 1;
				rvc_data = (char*)gf_malloc(sizeof(char) * rvc_size);
				rvc_size = gf_base64_decode(data, (u32) strlen(data), rvc_data, rvc_size);
				rvc_data[rvc_size] = 0;
			}
			if (!strncmp(rvc_config_att, "data:application/rvc-config+xml+gz", 35)) is_gz = GF_TRUE;
		} else if (!strnicmp(rvc_config_att, "http://", 7) || !strnicmp(rvc_config_att, "https://", 8) ) {
			return NULL;
		}
		if (rvc_data) {
			if (is_gz) {
#ifdef GPAC_DISABLE_ZLIB
				fprintf(stderr, "Error: no zlib support - RVC not supported in RTP\n");
				return NULL;
#endif
				gf_gz_decompress_payload(rvc_data, rvc_size, &tmp->depacketizer->sl_map.rvc_config, &tmp->depacketizer->sl_map.rvc_config_size);
				gf_free(rvc_data);
			} else {
				tmp->depacketizer->sl_map.rvc_config = rvc_data;
				tmp->depacketizer->sl_map.rvc_config_size = rvc_size;
			}
		}
	}

	return tmp;
}

static void rtpin_stream_update_stats(GF_RTPInStream *stream)
{
	u32 time;
	Float bps;
	if (!stream->rtp_ch) return;

	gf_filter_pid_set_info_str(stream->opid, "nets:loss", &PROP_FLOAT( gf_rtp_get_loss(stream->rtp_ch) ) );
	if (stream->rtsp && (stream->flags & RTP_INTERLEAVED)) {
		gf_filter_pid_set_info_str(stream->opid, "nets:interleaved", &PROP_UINT( gf_rtsp_get_session_port(stream->rtsp->session) ) );
		gf_filter_pid_set_info_str(stream->opid, "nets:rtpid", &PROP_UINT( gf_rtp_get_low_interleave_id(stream->rtp_ch) ) );
		gf_filter_pid_set_info_str(stream->opid, "nets:rctpid", &PROP_UINT( gf_rtp_get_hight_interleave_id(stream->rtp_ch) ) );

	} else {
		gf_filter_pid_set_info_str(stream->opid, "nets:rtpp", &PROP_UINT( stream->rtp_ch->net_info.client_port_first ) );
		gf_filter_pid_set_info_str(stream->opid, "nets:rtcpp", &PROP_UINT( stream->rtp_ch->net_info.client_port_last ) );
	}
	if (stream->stat_stop_time) {
		time = stream->stat_stop_time - stream->stat_start_time;
	} else {
		time = gf_sys_clock() - stream->stat_start_time;
	}
	if (!time) return;

	bps = 8.0f * stream->rtp_bytes;
	bps *= 1000;
	bps /= time;
	gf_filter_pid_set_info_str(stream->opid, "nets:bw_down", &PROP_UINT( (u32) bps ) );

	bps = 8.0f * stream->rtcp_bytes;
	bps *= 1000;
	bps /= time;
	gf_filter_pid_set_info_str(stream->opid, "nets:ctrl_bw_down", &PROP_UINT( (u32) bps ) );

	bps = 8.0f * gf_rtp_get_tcp_bytes_sent(stream->rtp_ch);
	bps *= 1000;
	bps /= time;
	gf_filter_pid_set_info_str(stream->opid, "nets:ctrl_bw_up", &PROP_UINT( (u32) bps ) );
}


void rtpin_stream_on_rtp_pck(GF_RTPInStream *stream, char *pck, u32 size)
{
	GF_Err e;
	GF_RTPHeader hdr;
	u32 PayloadStart;
	stream->rtp_bytes += size;
	stream->first_in_rtp_pck = GF_TRUE;

	/*first decode RTP*/
	e = gf_rtp_decode_rtp(stream->rtp_ch, pck, size, &hdr, &PayloadStart);

	/*corrupted or NULL data*/
	if (e || (PayloadStart >= size)) {
		//gf_service_send_packet(stream->rtpin->service, stream->opid, NULL, 0, NULL, GF_CORRUPTED_DATA);
		return;
	}
	/*first we only work with one payload type...*/
	if (hdr.PayloadType != stream->rtp_ch->PayloadType) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTP] Mismatched in packet payload type %d vs channel payload type %d, only one payload type per channel is currently supported\n", hdr.PayloadType, stream->rtp_ch->PayloadType));
	}

	//pure RTP, remap all streams to 0, adjust sync later
	if (!stream->rtsp) {
		if (!stream->first_rtp_ts || (hdr.TimeStamp < stream->first_rtp_ts-1) )
			stream->first_rtp_ts = 1 + hdr.TimeStamp;
	}

	/*if we must notify some timing, do it now. */
	if (stream->check_rtp_time) {
		Double ch_time;

		/*it may happen that we still receive packets from a previous "play" request. If this is the case,
		filter until we reach the indicated rtptime*/
		if (stream->rtp_ch->rtp_time
		        && (stream->rtp_ch->rtp_first_SN > hdr.SequenceNumber)
		        && (stream->rtp_ch->rtp_time > hdr.TimeStamp)
		   ) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTP] Rejecting too early packet (TS %d vs signaled rtp time %d - diff %d ms)\n",
			                                    hdr.TimeStamp, stream->rtp_ch->rtp_time, ((hdr.TimeStamp - stream->rtp_ch->rtp_time)*1000) / stream->rtp_ch->TimeScale));
			return;
		}

		ch_time = gf_rtp_get_current_time(stream->rtp_ch);

		/*this is the first packet on the channel (no PAUSE)*/
		if (stream->check_rtp_time == RTP_SET_TIME_RTP) {
			Double media_time = 0;
			if (stream->rtsp) {
				media_time = stream->current_start + ch_time;
			}
			gf_filter_pid_set_property_str(stream->opid, "time:timestamp", &PROP_LONGUINT(hdr.TimeStamp) );
			gf_filter_pid_set_property_str(stream->opid, "time:media", &PROP_DOUBLE(media_time) );

			GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTP] Mapping RTP Time seq %d TS %d Media Time %g - rtp info seq %d TS %d\n",
			                                 hdr.SequenceNumber, hdr.TimeStamp, stream->current_start + ch_time, stream->rtp_ch->rtp_first_SN, stream->rtp_ch->rtp_time
			                                ));

			/*skip RTCP clock init when RTSP is used*/
			if (stream->rtsp) stream->rtcp_init = GF_TRUE;

			if (stream->depacketizer->payt==GF_RTP_PAYT_H264_AVC) stream->depacketizer->flags |= GF_RTP_AVC_WAIT_RAP;
		}
		/*this is RESUME on channel, filter packet based on time (darwin seems to send
		couple of packet before)
		do not fetch if we're below 10 ms or <0, because this means we already have
		this packet - as the PAUSE is issued with the RTP currentTime*/
		else if (ch_time <= 0.021) {
			return;
		}
		stream->check_rtp_time = RTP_SET_TIME_NONE;
	}

	gf_rtp_depacketizer_process(stream->depacketizer, &hdr, pck + PayloadStart, size - PayloadStart);

	/*last check: signal EOS if we're close to end range in case the server do not send RTCP BYE*/
	if ((stream->flags & RTP_HAS_RANGE) && !(stream->flags & RTP_EOS) ) {
		/*also check last CTS*/
		Double ts = (Double) ((u32) stream->depacketizer->sl_hdr.compositionTimeStamp - hdr.TimeStamp);
		ts /= gf_rtp_get_clockrate(stream->rtp_ch);
		if (ABSDIFF(stream->range_end, (ts + stream->current_start + gf_rtp_get_current_time(stream->rtp_ch)) ) < 0.2) {
			stream->flags |= RTP_EOS;
			stream->stat_stop_time = gf_sys_clock();
			gf_filter_pid_set_eos(stream->opid);
		}
	}

	if (stream->rtpin->stats) {
		u32 now = gf_sys_clock();
		if (stream->last_stats_time + stream->rtpin->stats < now) {
			stream->last_stats_time = now;
			rtpin_stream_update_stats(stream);
		}
	}
}

static void rtpin_adjust_sync(GF_RTPIn *ctx)
{
	u32 i, count = gf_list_count(ctx->streams);
	u64 max_ntp_us = 0;

	for (i=0; i<count; i++) {
		GF_RTPInStream *stream = gf_list_get(ctx->streams, i);
		if (!stream->rtcp_init) return;

		if (max_ntp_us < stream->init_ntp_us) {
			max_ntp_us = stream->init_ntp_us;
		}
	}

	for (i=0; i<count; i++) {
		s64 ntp_diff_us;
		GF_RTPInStream *stream = gf_list_get(ctx->streams, i);

		ntp_diff_us = max_ntp_us;
		ntp_diff_us -= stream->init_ntp_us;
		ntp_diff_us *= stream->rtp_ch->TimeScale;
		ntp_diff_us /= 1000000;
		stream->ts_adjust = ntp_diff_us;
	}
}

static void rtpin_stream_on_rtcp_pck(GF_RTPInStream *stream, char *pck, u32 size)
{
	Bool has_sr;
	GF_Err e;

	if (stream->status == RTP_Connected) return;

	stream->rtcp_bytes += size;

	e = gf_rtp_decode_rtcp(stream->rtp_ch, pck, size, &has_sr);
	if (e<0) return;

	/*update sync if on pure RTP*/
	if (!stream->rtcp_init && has_sr) {
		u64 ntp_clock_us, rtp_diff_us;

		ntp_clock_us = stream->rtp_ch->last_SR_NTP_frac;
		ntp_clock_us *= 1000000;
		ntp_clock_us /= 0xFFFFFFFF;
		ntp_clock_us += 1000000 * (u64) stream->rtp_ch->last_SR_NTP_sec;

		//ntp time at origin: ntp(now) - ntp(first_pck) = rtpts(now) - rtpts(orig) -> ntp_first = ntp - rtp_diff
		rtp_diff_us = stream->rtp_ch->last_SR_rtp_time - (stream->first_rtp_ts - 1);
		rtp_diff_us *= 1000000;
		rtp_diff_us /= stream->rtp_ch->TimeScale;

		stream->init_ntp_us = ntp_clock_us - rtp_diff_us;


		GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTCP] At %d Using Sender Report to map RTP TS %d to NTP clock "LLU" us - NTP origin "LLU"\n", gf_sys_clock(), stream->rtp_ch->last_SR_rtp_time, ntp_clock_us, stream->init_ntp_us));

		stream->rtcp_init = GF_TRUE;

		if (stream->rtpin->rtcpsync)
			rtpin_adjust_sync(stream->rtpin);
	}

	if (e == GF_EOS) {
		stream->flags |= RTP_EOS;
	}
}

GF_Err rtpin_rtsp_data_cbk(GF_RTSPSession *sess, void *cbk, u8 *buffer, u32 bufferSize, Bool IsRTCP)
{
	GF_RTPInStream *stream = (GF_RTPInStream *) cbk;
	if (!stream || stream->rtpin->done) return GF_OK;
	if (IsRTCP) {
		rtpin_stream_on_rtcp_pck(stream, buffer, bufferSize);
	} else {
		rtpin_stream_on_rtp_pck(stream, buffer, bufferSize);
	}
	return GF_OK;
}



u32 rtpin_stream_read(GF_RTPInStream *stream)
{
	u32 size, tot_size = 0;

	if (!stream->rtp_ch) return 0;
	if (gf_sk_group_sock_is_set(stream->rtpin->sockgroup, stream->rtp_ch->rtcp)) {
		size = gf_rtp_read_rtcp(stream->rtp_ch, stream->buffer, stream->rtpin->block_size);
		if (size) {
			tot_size += size;
			rtpin_stream_on_rtcp_pck(stream, stream->buffer, size);
		}
	}

	if (gf_sk_group_sock_is_set(stream->rtpin->sockgroup, stream->rtp_ch->rtp)) {
		size = gf_rtp_read_rtp(stream->rtp_ch, stream->buffer, stream->rtpin->block_size);
		if (size) {
			tot_size += size;
			stream->rtpin->udp_timeout = 0;
			rtpin_stream_on_rtp_pck(stream, stream->buffer, size);
		}
	}
	if (!tot_size) return 0;

	/*and send the report*/
	if (stream->flags & RTP_ENABLE_RTCP) gf_rtp_send_rtcp_report(stream->rtp_ch);

	/*detect timeout*/
	if (stream->rtpin->udp_timeout) {
		if (!stream->last_udp_time) {
			stream->last_udp_time = gf_sys_clock();
		} else if (stream->rtp_ch->net_info.IsUnicast) {
			u32 diff = gf_sys_clock() - stream->last_udp_time;
			if (diff >= stream->rtpin->udp_timeout) {
				char szMessage[1024];
				GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTP] UDP Timeout after %d ms\n", diff));
				sprintf(szMessage, "No data received in %d ms%s", diff, stream->rtpin->autortsp ? ", retrying using TCP" : "");
				rtpin_send_message(stream->rtpin, GF_IP_UDP_TIMEOUT, szMessage);
				if (stream->rtpin->autortsp)
					stream->rtpin->retry_tcp = GF_TRUE;
			}
		}
	}
	return tot_size;
}

#endif /*GPAC_DISABLE_STREAMING*/
