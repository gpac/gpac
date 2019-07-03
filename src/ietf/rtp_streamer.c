/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / IETF RTP/RTSP/SDP sub-project
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


#include <gpac/rtp_streamer.h>
#include <gpac/constants.h>
#include <gpac/base_coding.h>
#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/avparse.h>
#endif
#include <gpac/internal/ietf_dev.h>

#if !defined(GPAC_DISABLE_STREAMING) && !defined(GPAC_DISABLE_ISOM)

struct __rtp_streamer
{
	GP_RTPPacketizer *packetizer;
	GF_RTPChannel *channel;

	/* The current packet being formed */
	char *buffer;
	u32 payload_len, buffer_alloc;

	Double ts_scale;
};


/*callbacks from packetizer to channel*/

static void rtp_stream_on_new_packet(void *cbk, GF_RTPHeader *header)
{
}

static void rtp_stream_on_packet_done(void *cbk, GF_RTPHeader *header)
{
	GF_RTPStreamer *rtp = (GF_RTPStreamer*)cbk;
	GF_Err e = gf_rtp_send_packet(rtp->channel, header, rtp->buffer+12, rtp->payload_len, GF_TRUE);

#ifndef GPAC_DISABLE_LOG
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Error %s sending RTP packet\n", gf_error_to_string(e)));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("RTP SN %u - TS %u - M %u - Size %u\n", header->SequenceNumber, header->TimeStamp, header->Marker, rtp->payload_len + 12));
	}
#endif
	rtp->payload_len = 0;
}

static void rtp_stream_on_data(void *cbk, u8 *data, u32 data_size, Bool is_head)
{
	GF_RTPStreamer *rtp = (GF_RTPStreamer*)cbk;
	if (!data ||!data_size) return;

	if (rtp->payload_len+data_size+12 > rtp->buffer_alloc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP] Packet size %d bigger than MTU size %d - discarding\n", rtp->payload_len+data_size+12, rtp->buffer_alloc));
		rtp->payload_len += data_size;
		return;
	}
	if (!is_head) {
		memcpy(rtp->buffer + rtp->payload_len + 12, data, data_size);
	} else {
		memmove(rtp->buffer + data_size + 12, rtp->buffer + 12, rtp->payload_len);
		memcpy(rtp->buffer + 12, data, data_size);
	}
	rtp->payload_len += data_size;
}


GF_Err gf_rtp_streamer_init_rtsp(GF_RTPStreamer *rtp, u32 path_mtu, GF_RTSPTransport  *tr, const char *ifce_addr)
{
	GF_Err res;

	if (!rtp->channel) rtp->channel = gf_rtp_new();

	res = gf_rtp_setup_transport(rtp->channel, tr, tr->destination);
	if (res !=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Cannot setup RTP transport info: %s\n", gf_error_to_string(res) ));
		return res;
	}

	res = gf_rtp_initialize(rtp->channel, 0, GF_TRUE, path_mtu, 0, 0, (char *)ifce_addr);
	if (res !=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Cannot initialize RTP sockets: %s\n", gf_error_to_string(res) ));
		return res;
	}
	return GF_OK;
}
static GF_Err rtp_stream_init_channel(GF_RTPStreamer *rtp, u32 path_mtu, const char * dest, int port, int ttl, const char *ifce_addr)
{
	GF_RTSPTransport tr;
	GF_Err res;

	rtp->channel = gf_rtp_new();
	gf_rtp_set_ports(rtp->channel, 0);
	memset(&tr, 0, sizeof(GF_RTSPTransport));

	tr.IsUnicast = gf_sk_is_multicast_address(dest) ? GF_FALSE : GF_TRUE;
	tr.Profile="RTP/AVP";
	tr.destination = (char *)dest;
	tr.source = "0.0.0.0";
	tr.IsRecord = GF_FALSE;
	tr.Append = GF_FALSE;
	tr.SSRC = rand();
	tr.TTL = ttl;

	tr.port_first = port;
	tr.port_last = port+1;
	if (tr.IsUnicast) {
		tr.client_port_first = port;
		tr.client_port_last  = port+1;
	} else {
		tr.source = (char *)dest;
	}

	res = gf_rtp_setup_transport(rtp->channel, &tr, dest);
	if (res !=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Cannot setup RTP transport info: %s\n", gf_error_to_string(res) ));
		return res;
	}

	res = gf_rtp_initialize(rtp->channel, 0, GF_TRUE, path_mtu, 0, 0, (char *)ifce_addr);
	if (res !=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Cannot initialize RTP sockets: %s\n", gf_error_to_string(res) ));
		return res;
	}
	return GF_OK;
}

GF_EXPORT
GF_RTPStreamer *gf_rtp_streamer_new(u32 streamType, u32 codecid, u32 timeScale,
        const char *ip_dest, u16 port, u32 MTU, u8 TTL, const char *ifce_addr,
        u32 flags, u8 *dsi, u32 dsi_len,
        u32 PayloadType, u32 sample_rate, u32 nb_ch,
        Bool is_crypted, u32 IV_length, u32 KI_length,
        u32 MinSize, u32 MaxSize, u32 avgTS, u32 maxDTSDelta, u32 const_dur, u32 bandwidth, u32 max_ptime,
        u32 au_sn_len, Bool for_rtsp)
{
	GF_SLConfig slc;
	GF_RTPStreamer *stream;
	u32 rtp_type, default_rtp_rate;
	u8 OfficialPayloadType;
	u32 required_rate, force_dts_delta, PL_ID;
	char *mpeg4mode;
	Bool has_mpeg4_mapping;
	GF_Err e;

	if (!timeScale) timeScale = 1000;

	GF_SAFEALLOC(stream, GF_RTPStreamer);
	if (!stream) return NULL;


	/*by default NO PL signaled*/
	PL_ID = 0;
	OfficialPayloadType = 0;
	force_dts_delta = 0;
	mpeg4mode = NULL;
	required_rate = 0;
	has_mpeg4_mapping = GF_TRUE;
	rtp_type = 0;

	/*for max compatibility with QT*/
	default_rtp_rate = 90000;

	/*timed-text is a bit special, we support multiple stream descriptions & co*/
	switch (streamType) {
	case GF_STREAM_AUDIO:
		required_rate = sample_rate;
		break;
	case GF_STREAM_VISUAL:
		rtp_type = GF_RTP_PAYT_MPEG4;
		required_rate = default_rtp_rate;
		if (is_crypted) {
			/*that's another pain with ISMACryp, even if no B-frames the DTS is signaled...*/
			if (codecid==GF_CODECID_MPEG4_PART2) force_dts_delta = 22;
			flags |= GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_SIGNAL_TS;
		}
		break;
	case GF_STREAM_SCENE:
	case GF_STREAM_OD:
		if (codecid == GF_CODECID_DIMS) {
#if GPAC_ENABLE_3GPP_DIMS_RTP
			rtp_type = GF_RTP_PAYT_3GPP_DIMS;
			has_mpeg4_mapping = GF_FALSE;
#else
			gf_free(stream);
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP Packetizer] 3GPP DIMS over RTP disabled in build\n", streamType));
			return NULL;
#endif
		} else {
			rtp_type = GF_RTP_PAYT_MPEG4;
		}
		break;
	}

	switch (codecid) {
	/*AAC*/
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
		PL_ID = 0x01;
		mpeg4mode = "AAC";
		rtp_type = GF_RTP_PAYT_MPEG4;
		required_rate = sample_rate;

#ifndef GPAC_DISABLE_AV_PARSERS
		if (dsi) {
			GF_M4ADecSpecInfo a_cfg;
			gf_m4a_get_config(dsi, dsi_len, &a_cfg);
			//nb_ch = a_cfg.nb_chan;
			//sample_rate = a_cfg.base_sr;
			PL_ID = a_cfg.audioPL;
			switch (a_cfg.base_object_type) {
			case GF_M4A_AAC_MAIN:
			case GF_M4A_AAC_LC:
				if (flags & GP_RTP_PCK_USE_LATM_AAC) {
					rtp_type = GF_RTP_PAYT_LATM;
					break;
				}
			case GF_M4A_AAC_SBR:
			case GF_M4A_AAC_PS:
			case GF_M4A_AAC_LTP:
			case GF_M4A_AAC_SCALABLE:
			case GF_M4A_ER_AAC_LC:
			case GF_M4A_ER_AAC_LTP:
			case GF_M4A_ER_AAC_SCALABLE:
				mpeg4mode = "AAC";
				break;
			case GF_M4A_CELP:
			case GF_M4A_ER_CELP:
				mpeg4mode = "CELP";
				break;
			}
		}
#endif
		break;

	/*MPEG1/2 audio*/
	case GF_CODECID_MPEG2_PART3:
	case GF_CODECID_MPEG_AUDIO:
		if (!is_crypted) {
			rtp_type = GF_RTP_PAYT_MPEG12_AUDIO;
			/*use official RTP/AVP payload type*/
			OfficialPayloadType = 14;
			required_rate = 90000;
		}
		/*encrypted MP3 must be sent through MPEG-4 generic to signal all ISMACryp stuff*/
		else {
			rtp_type = GF_RTP_PAYT_MPEG4;
		}
		break;

	/*ISO/IEC 14496-2*/
	case GF_CODECID_MPEG4_PART2:
		PL_ID = 1;
#ifndef GPAC_DISABLE_AV_PARSERS
		if (dsi) {
			GF_M4VDecSpecInfo vhdr;
			gf_m4v_get_config(dsi, dsi_len, &vhdr);
			PL_ID = vhdr.VideoPL;
		}
#endif
		break;

	/*MPEG1/2 video*/
	case GF_CODECID_MPEG1:
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_SPATIAL:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_422:
		if (!is_crypted) {
			rtp_type = GF_RTP_PAYT_MPEG12_VIDEO;
			OfficialPayloadType = 32;
		}
		break;
	/*AVC/H.264*/
	case GF_CODECID_AVC:
		required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
		rtp_type = GF_RTP_PAYT_H264_AVC;
		PL_ID = 0x0F;
		break;
	/*H264-SVC*/
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
		rtp_type = GF_RTP_PAYT_H264_SVC;
		PL_ID = 0x0F;
		break;

	/*HEVC*/
	case GF_CODECID_HEVC:
		required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
		rtp_type = GF_RTP_PAYT_HEVC;
		PL_ID = 0x0F;
		break;
	/*LHVC*/
	case GF_CODECID_LHVC:
		required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
		rtp_type = GF_RTP_PAYT_LHVC;
		PL_ID = 0x0F;
		break;

	case GF_CODECID_H263:
		rtp_type = GF_RTP_PAYT_H263;
		required_rate = 90000;
		streamType = GF_STREAM_VISUAL;
		OfficialPayloadType = 34;
		/*not 100% compliant (short header is missing) but should still work*/
		codecid = GF_CODECID_MPEG4_PART2;
		PL_ID = 0x01;
		break;
	case GF_CODECID_AMR:
		required_rate = 8000;
		rtp_type = GF_RTP_PAYT_AMR;
		streamType = GF_STREAM_AUDIO;
		has_mpeg4_mapping = GF_FALSE;
		break;
	case GF_CODECID_AMR_WB:
		required_rate = 16000;
		rtp_type = GF_RTP_PAYT_AMR_WB;
		streamType = GF_STREAM_AUDIO;
		has_mpeg4_mapping = GF_FALSE;
		break;
	case GF_CODECID_AC3:
		rtp_type = GF_RTP_PAYT_AC3;
		streamType = GF_STREAM_AUDIO;
		has_mpeg4_mapping = GF_TRUE;
		break;

	case GF_CODECID_QCELP:
		required_rate = 8000;
		rtp_type = GF_RTP_PAYT_QCELP;
		streamType = GF_STREAM_AUDIO;
		codecid = GF_CODECID_QCELP;
		OfficialPayloadType = 12;
//			nb_ch = 1;
		break;
	case GF_CODECID_EVRC:
	case GF_CODECID_SMV:
		required_rate = 8000;
		rtp_type = GF_RTP_PAYT_EVRC_SMV;
		streamType = GF_STREAM_AUDIO;
		codecid = (codecid==GF_ISOM_SUBTYPE_3GP_EVRC) ? GF_CODECID_EVRC : GF_CODECID_SMV;
//			nb_ch = 1;
		break;
	case GF_CODECID_TX3G:
		rtp_type = GF_RTP_PAYT_3GPP_TEXT;
		/*fixme - this works cos there's only one PL for text in mpeg4 at the current time*/
		PL_ID = 0x10;
		break;
	case GF_CODECID_TEXT_MPEG4:
		rtp_type = GF_RTP_PAYT_3GPP_TEXT;
		/*fixme - this works cos there's only one PL for text in mpeg4 at the current time*/
		PL_ID = 0x10;
		break;

	default:
		if (!rtp_type) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP Packetizer] Unsupported stream type %x\n", streamType));
			return NULL;
		}
		break;
	}

	/*override hinter type if requested and possible*/
	if (has_mpeg4_mapping && (flags & GP_RTP_PCK_FORCE_MPEG4)) {
		rtp_type = GF_RTP_PAYT_MPEG4;
	}
	/*use static payload ID if enabled*/
	else if (OfficialPayloadType && (flags & GP_RTP_PCK_USE_STATIC_ID) ) {
		PayloadType = OfficialPayloadType;
	}

	/*systems carousel: we need at least IDX and RAP signaling*/
	if (flags & GP_RTP_PCK_SYSTEMS_CAROUSEL) {
		flags |= GP_RTP_PCK_SIGNAL_RAP;
	}

	/*update flags in MultiSL*/
	if (flags & GP_RTP_PCK_USE_MULTI) {
		if (MinSize != MaxSize) flags |= GP_RTP_PCK_SIGNAL_SIZE;
		if (!const_dur) flags |= GP_RTP_PCK_SIGNAL_TS;
	}

	/*default SL for RTP */
	memset(&slc, 0, sizeof(GF_SLConfig));
	slc.tag = GF_ODF_SLC_TAG;
	slc.useTimestampsFlag = 1;
	slc.timestampLength = 32;
	slc.timestampResolution = timeScale;

	/*override clockrate if set*/
	if (required_rate) {
		Double sc = required_rate;
		sc /= slc.timestampResolution;
		maxDTSDelta = (u32) (maxDTSDelta*sc);
		slc.timestampResolution = required_rate;
	}
	/*switch to RTP TS*/
	max_ptime = (u32) (max_ptime * slc.timestampResolution / 1000);

	slc.AUSeqNumLength = au_sn_len;
	slc.CUDuration = const_dur;

	if (flags & GP_RTP_PCK_SIGNAL_RAP) {
		slc.useRandomAccessPointFlag = 1;
	} else {
		slc.useRandomAccessPointFlag = 0;
		slc.hasRandomAccessUnitsOnlyFlag = 1;
	}

	stream->packetizer = gf_rtp_builder_new(rtp_type, &slc, flags,
	                                        stream,
	                                        rtp_stream_on_new_packet, rtp_stream_on_packet_done,
	                                        NULL, rtp_stream_on_data);

	if (!stream->packetizer) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP Packetizer] Failed to create packetizer\n"));
		gf_free(stream);
		return NULL;
	}

	gf_rtp_builder_init(stream->packetizer, (u8) PayloadType, MTU, max_ptime,
	                    streamType, codecid, PL_ID, MinSize, MaxSize, avgTS, maxDTSDelta, IV_length, KI_length, mpeg4mode);


	if (force_dts_delta) stream->packetizer->slMap.DTSDeltaLength = force_dts_delta;

	if (!for_rtsp) {
		e = rtp_stream_init_channel(stream, MTU + 12, ip_dest, port, TTL, ifce_addr);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP Packetizer] Failed to create RTP channel - error %s\n", gf_error_to_string(e) ));
			gf_free(stream);
			return NULL;
		}
	}

	stream->ts_scale = slc.timestampResolution;
	stream->ts_scale /= timeScale;

	stream->buffer_alloc = MTU+12;
	stream->buffer = (char*)gf_malloc(sizeof(char) * stream->buffer_alloc);

	return stream;
}


GF_EXPORT
void gf_rtp_streamer_del(GF_RTPStreamer *streamer)
{
	if (streamer) {
		if (streamer->channel) gf_rtp_del(streamer->channel);
		if (streamer->packetizer) gf_rtp_builder_del(streamer->packetizer);
		if (streamer->buffer) gf_free(streamer->buffer);
		gf_free(streamer);
	}
}

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING)

void gf_media_format_ttxt_sdp(GP_RTPPacketizer *builder, char *payload_name, char *sdpLine, u32 w, u32 h, s32 tx, s32 ty, s16 l, u32 max_w, u32 max_h, char *tx3g_base64)
{
	char buffer[2000];
	sprintf(sdpLine, "a=fmtp:%d sver=60; ", builder->PayloadType);

	sprintf(buffer, "width=%d; height=%d; tx=%d; ty=%d; layer=%d; ", w, h, tx, ty, l);
	strcat(sdpLine, buffer);

	sprintf(buffer, "max-w=%d; max-h=%d", max_w, max_h);
	strcat(sdpLine, buffer);

	if (tx3g_base64) {
		strcat(sdpLine, "; tx3g=");
		strcat(sdpLine, tx3g_base64);
	}
}

#endif /*!defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING)*/


GF_EXPORT
GF_Err gf_rtp_streamer_append_sdp_extended(GF_RTPStreamer *rtp, u16 ESID, u8 *dsi, u32 dsi_len, u8 *dsi_enh, u32 dsi_enh_len, char *KMS_URI, u32 width, u32 height, u32 tw, u32 th, s32 tx, s32 ty, s16 tl, Bool for_rtsp, char **out_sdp_buffer)
{
	u32 size;
	u16 port=0;
	char mediaName[30], payloadName[30];
	char sdp[20000], sdpLine[10000];

	if (!out_sdp_buffer) return GF_BAD_PARAM;

	gf_rtp_builder_get_payload_name(rtp->packetizer, payloadName, mediaName);
	if (!for_rtsp)
		gf_rtp_get_ports(rtp->channel, &port, NULL);

	sprintf(sdp, "m=%s %d RTP/%s %d\n", mediaName, for_rtsp ? 0 : port, rtp->packetizer->slMap.IV_length ? "SAVP" : "AVP", rtp->packetizer->PayloadType);
	sprintf(sdpLine, "a=rtpmap:%d %s/%d\n", rtp->packetizer->PayloadType, payloadName, rtp->packetizer->sl_config.timestampResolution);
	strcat(sdp, sdpLine);

	if (ESID
#if GPAC_ENABLE_3GPP_DIMS_RTP
		&& (rtp->packetizer->rtp_payt != GF_RTP_PAYT_3GPP_DIMS)
#endif
	 ) {
		sprintf(sdpLine, "a=mpeg4-esid:%d\n", ESID);
		strcat(sdp, sdpLine);
	}

	if (width && height) {
		if (rtp->packetizer->rtp_payt == GF_RTP_PAYT_H263) {
			sprintf(sdpLine, "a=cliprect:0,0,%d,%d\n", height, width);
			strcat(sdp, sdpLine);
		}
		/*extensions for some mobile phones*/
		sprintf(sdpLine, "a=framesize:%d %d-%d\n", rtp->packetizer->PayloadType, width, height);
		strcat(sdp, sdpLine);
	}

	strcpy(sdpLine, "");

	/*AMR*/
	if ((rtp->packetizer->rtp_payt == GF_RTP_PAYT_AMR) || (rtp->packetizer->rtp_payt == GF_RTP_PAYT_AMR_WB)) {
		sprintf(sdpLine, "a=fmtp:%d octet-align=1\n", rtp->packetizer->PayloadType);
	}
	/*Text*/
	else if (rtp->packetizer->rtp_payt == GF_RTP_PAYT_3GPP_TEXT) {
		gf_media_format_ttxt_sdp(rtp->packetizer, payloadName, sdpLine, tw, th, tx, ty, tl, width, height, dsi_enh);
		strcat(sdpLine, "\n");
	}
	/*EVRC/SMV in non header-free mode*/
	else if ((rtp->packetizer->rtp_payt == GF_RTP_PAYT_EVRC_SMV) && (rtp->packetizer->auh_size>1)) {
		sprintf(sdpLine, "a=fmtp:%d maxptime=%d\n", rtp->packetizer->PayloadType, rtp->packetizer->auh_size*20);
	}
	/*H264/AVC*/
	else if ((rtp->packetizer->rtp_payt == GF_RTP_PAYT_H264_AVC) || (rtp->packetizer->rtp_payt == GF_RTP_PAYT_H264_SVC)) {
		GF_AVCConfig *avcc = dsi ? gf_odf_avc_cfg_read(dsi, dsi_len) : NULL;

		if (avcc) {
			sprintf(sdpLine, "a=fmtp:%d profile-level-id=%02X%02X%02X; packetization-mode=1", rtp->packetizer->PayloadType, avcc->AVCProfileIndication, avcc->profile_compatibility, avcc->AVCLevelIndication);
			if (gf_list_count(avcc->pictureParameterSets) || gf_list_count(avcc->sequenceParameterSets)) {
				u32 i, count, b64s;
				char b64[200];
				strcat(sdpLine, "; sprop-parameter-sets=");
				count = gf_list_count(avcc->sequenceParameterSets);
				for (i=0; i<count; i++) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(avcc->sequenceParameterSets, i);
					b64s = gf_base64_encode(sl->data, sl->size, b64, 200);
					b64[b64s]=0;
					strcat(sdpLine, b64);
					if (i+1<count) strcat(sdpLine, ",");
				}
				if (i) strcat(sdpLine, ",");
				count = gf_list_count(avcc->pictureParameterSets);
				for (i=0; i<count; i++) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(avcc->pictureParameterSets, i);
					b64s = gf_base64_encode(sl->data, sl->size, b64, 200);
					b64[b64s]=0;
					strcat(sdpLine, b64);
					if (i+1<count) strcat(sdpLine, ",");
				}
			}
			gf_odf_avc_cfg_del(avcc);
			strcat(sdpLine, "\n");
		}
	}
	else if ((rtp->packetizer->rtp_payt == GF_RTP_PAYT_HEVC) || (rtp->packetizer->rtp_payt == GF_RTP_PAYT_LHVC)) {
#ifndef GPAC_DISABLE_HEVC
		GF_HEVCConfig *hevcc = dsi ? gf_odf_hevc_cfg_read(dsi, dsi_len, GF_FALSE) : NULL;
		if (hevcc) {
			u32 count, i, j, b64s;
			char b64[200];
			sprintf(sdpLine, "a=fmtp:%d", rtp->packetizer->PayloadType);
			count = gf_list_count(hevcc->param_array);
			for (i = 0; i < count; i++) {
				GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(hevcc->param_array, i);
				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					strcat(sdpLine, "; sprop-sps=");
				} else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
					strcat(sdpLine, "; sprop-pps=");
				} else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
					strcat(sdpLine, "; sprop-vps=");
				}
				for (j = 0; j < gf_list_count(ar->nalus); j++) {
					GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j);
					b64s = gf_base64_encode(sl->data, sl->size, b64, 200);
					b64[b64s]=0;
					if (j) strcat(sdpLine, ", ");
					strcat(sdpLine, b64);
				}
			}
			gf_odf_hevc_cfg_del(hevcc);
			strcat(sdpLine, "\n");
		}
#endif
	}
	/*MPEG-4 decoder config*/
	else if (rtp->packetizer->rtp_payt==GF_RTP_PAYT_MPEG4) {
		gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, dsi, dsi_len);
		strcat(sdpLine, "\n");

		if (rtp->packetizer->slMap.IV_length && KMS_URI) {
			if (!strnicmp(KMS_URI, "(key)", 5) || !strnicmp(KMS_URI, "(ipmp)", 6) || !strnicmp(KMS_URI, "(uri)", 5)) {
				strcat(sdpLine, "; ISMACrypKey=");
			} else {
				strcat(sdpLine, "; ISMACrypKey=(uri)");
			}
			strcat(sdpLine, KMS_URI);
			strcat(sdpLine, "\n");
		}
	}
#if GPAC_ENABLE_3GPP_DIMS_RTP
	/*DIMS decoder config*/
	else if (rtp->packetizer->rtp_payt==GF_RTP_PAYT_3GPP_DIMS) {
		sprintf(sdpLine, "a=fmtp:%d Version-profile=%d", rtp->packetizer->PayloadType, 10);
		if (rtp->packetizer->flags & GP_RTP_DIMS_COMPRESSED) {
			strcat(sdpLine, ";content-coding=deflate");
		}
		strcat(sdpLine, "\n");
	}
#endif
	/*MPEG-4 Audio LATM*/
	else if (rtp->packetizer->rtp_payt==GF_RTP_PAYT_LATM) {
		GF_BitStream *bs;
		u8 *config_bytes;
		u32 config_size;

		/* form config string */
		bs = gf_bs_new(NULL, 32, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, 0, 1); /* AudioMuxVersion */
		gf_bs_write_int(bs, 1, 1); /* all streams same time */
		gf_bs_write_int(bs, 0, 6); /* numSubFrames */
		gf_bs_write_int(bs, 0, 4); /* numPrograms */
		gf_bs_write_int(bs, 0, 3); /* numLayer */

		/* audio-specific config  - PacketVideo patch: don't signal SBR and PS stuff, not allowed in LATM with audioMuxVersion=0*/
		if (dsi) gf_bs_write_data(bs, dsi, MIN(dsi_len, 2) );

		/* other data */
		gf_bs_write_int(bs, 0, 3); /* frameLengthType */
		gf_bs_write_int(bs, 0xff, 8); /* latmBufferFullness */
		gf_bs_write_int(bs, 0, 1); /* otherDataPresent */
		gf_bs_write_int(bs, 0, 1); /* crcCheckPresent */
		gf_bs_get_content(bs, &config_bytes, &config_size);
		gf_bs_del(bs);

		gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, config_bytes, config_size);
		gf_free(config_bytes);
		strcat(sdpLine, "\n");
	}

	strcat(sdp, sdpLine);

	size = (u32) strlen(sdp) + (*out_sdp_buffer ? (u32) strlen(*out_sdp_buffer) : 0) + 1;
	if ( !*out_sdp_buffer) {
		*out_sdp_buffer = (char*)gf_malloc(sizeof(char)*size);
		if (! *out_sdp_buffer) return GF_OUT_OF_MEM;
		strcpy(*out_sdp_buffer, sdp);
	} else {
		*out_sdp_buffer = (char*)gf_realloc(*out_sdp_buffer, sizeof(char)*size);
		if (! *out_sdp_buffer) return GF_OUT_OF_MEM;
		strcat(*out_sdp_buffer, sdp);
	}
	return GF_OK;
}



GF_EXPORT
char *gf_rtp_streamer_format_sdp_header(char *app_name, char *ip_dest, char *session_name, char *iod64)
{
	u64 size;
	char *sdp, *tmp_fn = NULL;
	FILE *tmp = gf_temp_file_new(&tmp_fn);
	if (!tmp) return NULL;

	/* write SDP header*/
	fprintf(tmp, "v=0\n");
	fprintf(tmp, "o=%s 3326096807 1117107880000 IN IP%d %s\n", app_name, gf_net_is_ipv6(ip_dest) ? 6 : 4, ip_dest);
	fprintf(tmp, "s=%s\n", (session_name ? session_name : "GPAC Scene Streaming Session"));
	fprintf(tmp, "c=IN IP%d %s\n", gf_net_is_ipv6(ip_dest) ? 6 : 4, ip_dest);
	fprintf(tmp, "t=0 0\n");

	if (iod64) fprintf(tmp, "a=mpeg4-iod:\"data:application/mpeg4-iod;base64,%s\"\n", iod64);

	gf_fseek(tmp, 0, SEEK_END);
	size = gf_ftell(tmp);
	gf_fseek(tmp, 0, SEEK_SET);
	sdp = (char*)gf_malloc(sizeof(char) * (size_t)(size+1));
	size = fread(sdp, 1, (size_t)size, tmp);
	sdp[size] = 0;
	gf_fclose(tmp);
	gf_delete_file(tmp_fn);
	gf_free(tmp_fn);
	return sdp;
}

GF_EXPORT
GF_Err gf_rtp_streamer_append_sdp(GF_RTPStreamer *rtp, u16 ESID, u8 *dsi, u32 dsi_len, char *KMS_URI, char **out_sdp_buffer)
{
	return gf_rtp_streamer_append_sdp_extended(rtp, ESID, dsi, dsi_len, NULL, 0, KMS_URI, 0, 0, 0, 0, 0, 0, 0, GF_FALSE, out_sdp_buffer);
}

GF_EXPORT
GF_Err gf_rtp_streamer_send_data(GF_RTPStreamer *rtp, u8 *data, u32 size, u32 fullsize, u64 cts, u64 dts, Bool is_rap, Bool au_start, Bool au_end, u32 au_sn, u32 sampleDuration, u32 sampleDescIndex)
{
	rtp->packetizer->sl_header.compositionTimeStamp = (u64) (cts*rtp->ts_scale);
	rtp->packetizer->sl_header.decodingTimeStamp = (u64) (dts*rtp->ts_scale);
	rtp->packetizer->sl_header.randomAccessPointFlag = is_rap;
	rtp->packetizer->sl_header.accessUnitStartFlag = au_start;
	rtp->packetizer->sl_header.accessUnitEndFlag = au_end;
	rtp->packetizer->sl_header.randomAccessPointFlag = is_rap;
	rtp->packetizer->sl_header.AU_sequenceNumber = au_sn;
	sampleDuration = (u32) (sampleDuration * rtp->ts_scale);
	if (au_start && size) rtp->packetizer->nb_aus++;

	return gf_rtp_builder_process(rtp->packetizer, data, size, (u8) au_end, fullsize, sampleDuration, sampleDescIndex);
}

GF_EXPORT
GF_Err gf_rtp_streamer_send_au(GF_RTPStreamer *rtp, u8 *data, u32 size, u64 cts, u64 dts, Bool is_rap)
{
	return gf_rtp_streamer_send_data(rtp, data, size, size, cts, dts, is_rap, GF_TRUE, GF_TRUE, 0, 0, 0);
}

GF_EXPORT
GF_Err gf_rtp_streamer_send_au_with_sn(GF_RTPStreamer *rtp, u8 *data, u32 size, u64 cts, u64 dts, Bool is_rap, u32 inc_au_sn)
{
	if (inc_au_sn) rtp->packetizer->sl_header.AU_sequenceNumber += inc_au_sn;
	return gf_rtp_streamer_send_data(rtp, data, size, size, cts, dts, is_rap, GF_TRUE, GF_TRUE, rtp->packetizer->sl_header.AU_sequenceNumber, 0, 0);
}

GF_EXPORT
void gf_rtp_streamer_disable_auto_rtcp(GF_RTPStreamer *streamer)
{
	streamer->channel->no_auto_rtcp = GF_TRUE;
}

GF_EXPORT
GF_Err gf_rtp_streamer_send_rtcp(GF_RTPStreamer *streamer, Bool force_ts, u32 rtp_ts, u32 force_ntp_type, u32 ntp_sec, u32 ntp_frac)
{
	if (force_ts) streamer->channel->last_pck_ts = rtp_ts;
	streamer->channel->forced_ntp_sec = force_ntp_type ? ntp_sec : 0;
	streamer->channel->forced_ntp_frac = force_ntp_type ? ntp_frac : 0;
	if (force_ntp_type==2)
		streamer->channel->next_report_time = 0;
	return gf_rtp_send_rtcp_report(streamer->channel);
}

GF_EXPORT
GF_Err gf_rtp_streamer_send_bye(GF_RTPStreamer *streamer)
{
	return gf_rtp_send_bye(streamer->channel);
}

GF_EXPORT
u8 gf_rtp_streamer_get_payload_type(GF_RTPStreamer *streamer)
{
	return streamer ? streamer->packetizer->PayloadType : 0;
}

GF_EXPORT
u16 gf_rtp_streamer_get_next_rtp_sn(GF_RTPStreamer *streamer)
{
	return streamer->packetizer->rtp_header.SequenceNumber+1;
}

GF_EXPORT
GF_Err gf_rtp_streamer_set_interleave_callbacks(GF_RTPStreamer *streamer, GF_Err (*RTP_TCPCallback)(void *cbk1, void *cbk2, Bool is_rtcp, u8 *pck, u32 pck_size), void *cbk1, void *cbk2)
{

 	return gf_rtp_set_interleave_callbacks(streamer->channel, RTP_TCPCallback, cbk1, cbk2);
}

#endif /*GPAC_DISABLE_STREAMING && GPAC_DISABLE_ISOM*/

