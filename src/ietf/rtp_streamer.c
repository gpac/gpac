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

static void rtp_stream_on_data(void *cbk, char *data, u32 data_size, Bool is_head)
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
GF_RTPStreamer *gf_rtp_streamer_new_extended(u32 streamType, u32 oti, u32 timeScale,
        const char *ip_dest, u16 port, u32 MTU, u8 TTL, const char *ifce_addr,
        u32 flags, char *dsi, u32 dsi_len,

        u32 PayloadType, u32 sample_rate, u32 nb_ch,
        Bool is_crypted, u32 IV_length, u32 KI_length,
        u32 MinSize, u32 MaxSize, u32 avgTS, u32 maxDTSDelta, u32 const_dur, u32 bandwidth, u32 max_ptime,
        u32 au_sn_len
                                            )
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
	case GF_STREAM_TEXT:
		if (oti!=GPAC_OTI_TEXT_MPEG4)
			return NULL;

		rtp_type = GF_RTP_PAYT_3GPP_TEXT;
		/*fixme - this works cos there's only one PL for text in mpeg4 at the current time*/
		PL_ID = 0x10;
		break;
	case GF_STREAM_AUDIO:
		required_rate = sample_rate;
		switch (oti) {
		/*AAC*/
		case GPAC_OTI_AUDIO_AAC_MPEG4:
		case GPAC_OTI_AUDIO_AAC_MPEG2_MP:
		case GPAC_OTI_AUDIO_AAC_MPEG2_LCP:
		case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP:
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
		case GPAC_OTI_AUDIO_MPEG2_PART3:
		case GPAC_OTI_AUDIO_MPEG1:
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

		/*QCELP audio*/
		case GPAC_OTI_AUDIO_13K_VOICE:
			rtp_type = GF_RTP_PAYT_QCELP;
			OfficialPayloadType = 12;
			required_rate = 8000;
			//nb_ch = 1;
			break;

		/*EVRC/SVM audio*/
		case GPAC_OTI_AUDIO_EVRC_VOICE:
		case GPAC_OTI_AUDIO_SMV_VOICE:
			rtp_type = GF_RTP_PAYT_EVRC_SMV;
			required_rate = 8000;
			//nb_ch = 1;
		}

		break;

	case GF_STREAM_VISUAL:
		rtp_type = GF_RTP_PAYT_MPEG4;
		required_rate = default_rtp_rate;
		if (is_crypted) {
			/*that's another pain with ISMACryp, even if no B-frames the DTS is signaled...*/
			if (oti==GPAC_OTI_VIDEO_MPEG4_PART2) force_dts_delta = 22;
			flags |= GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_SIGNAL_TS;
		}

		switch (oti) {
		/*ISO/IEC 14496-2*/
		case GPAC_OTI_VIDEO_MPEG4_PART2:
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
		case GPAC_OTI_VIDEO_MPEG1:
		case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
		case GPAC_OTI_VIDEO_MPEG2_MAIN:
		case GPAC_OTI_VIDEO_MPEG2_SNR:
		case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
		case GPAC_OTI_VIDEO_MPEG2_HIGH:
		case GPAC_OTI_VIDEO_MPEG2_422:
			if (!is_crypted) {
				rtp_type = GF_RTP_PAYT_MPEG12_VIDEO;
				OfficialPayloadType = 32;
			}
			break;
		/*AVC/H.264*/
		case GPAC_OTI_VIDEO_AVC:
			required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
			rtp_type = GF_RTP_PAYT_H264_AVC;
			PL_ID = 0x0F;
			break;
		/*H264-SVC*/
		case GPAC_OTI_VIDEO_SVC:
		case GPAC_OTI_VIDEO_MVC:
			required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
			rtp_type = GF_RTP_PAYT_H264_SVC;
			PL_ID = 0x0F;
			break;
		/*HEVC*/
		case GPAC_OTI_VIDEO_HEVC:
			required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
			rtp_type = GF_RTP_PAYT_HEVC;
			PL_ID = 0x0F;
			break;
		/*LHVC*/
		case GPAC_OTI_VIDEO_LHVC:
			required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
			rtp_type = GF_RTP_PAYT_LHVC;
			PL_ID = 0x0F;
			break;
		}
		break;

	case GF_STREAM_SCENE:
	case GF_STREAM_OD:
		if (oti == GPAC_OTI_SCENE_DIMS) {
			rtp_type = GF_RTP_PAYT_3GPP_DIMS;
			has_mpeg4_mapping = GF_FALSE;
		} else {
			rtp_type = GF_RTP_PAYT_MPEG4;
		}
		break;


	case GF_STREAM_4CC:
		switch (oti) {
		case GF_ISOM_SUBTYPE_3GP_H263:
			rtp_type = GF_RTP_PAYT_H263;
			required_rate = 90000;
			streamType = GF_STREAM_VISUAL;
			OfficialPayloadType = 34;
			/*not 100% compliant (short header is missing) but should still work*/
			oti = GPAC_OTI_VIDEO_MPEG4_PART2;
			PL_ID = 0x01;
			break;
		case GF_ISOM_SUBTYPE_3GP_AMR:
			required_rate = 8000;
			rtp_type = GF_RTP_PAYT_AMR;
			streamType = GF_STREAM_AUDIO;
			has_mpeg4_mapping = GF_FALSE;
//			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_3GP_AMR_WB:
			required_rate = 16000;
			rtp_type = GF_RTP_PAYT_AMR_WB;
			streamType = GF_STREAM_AUDIO;
			has_mpeg4_mapping = GF_FALSE;
//			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_AC3:
			rtp_type = GF_RTP_PAYT_AC3;
			streamType = GF_STREAM_AUDIO;
			has_mpeg4_mapping = GF_TRUE;
//			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_AVC_H264:
		case GF_ISOM_SUBTYPE_AVC2_H264:
		case GF_ISOM_SUBTYPE_AVC3_H264:
		case GF_ISOM_SUBTYPE_AVC4_H264:
		case GF_ISOM_SUBTYPE_SVC_H264:
		case GF_ISOM_SUBTYPE_MVC_H264:
		{
			required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
			rtp_type = GF_RTP_PAYT_H264_AVC;
			streamType = GF_STREAM_VISUAL;
			oti = GPAC_OTI_VIDEO_AVC;
			PL_ID = 0x0F;
		}
		break;
		case GF_ISOM_SUBTYPE_3GP_QCELP:
			required_rate = 8000;
			rtp_type = GF_RTP_PAYT_QCELP;
			streamType = GF_STREAM_AUDIO;
			oti = GPAC_OTI_AUDIO_13K_VOICE;
			OfficialPayloadType = 12;
//			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_3GP_EVRC:
		case GF_ISOM_SUBTYPE_3GP_SMV:
			required_rate = 8000;
			rtp_type = GF_RTP_PAYT_EVRC_SMV;
			streamType = GF_STREAM_AUDIO;
			oti = (oti==GF_ISOM_SUBTYPE_3GP_EVRC) ? GPAC_OTI_AUDIO_EVRC_VOICE : GPAC_OTI_AUDIO_SMV_VOICE;
//			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_MP3:
			rtp_type = GF_RTP_PAYT_MPEG12_AUDIO;
			/*use official RTP/AVP payload type*/
			OfficialPayloadType = 14;
			required_rate = 90000;
			break;

		case GF_ISOM_SUBTYPE_TEXT:
		case GF_ISOM_SUBTYPE_TX3G:
			rtp_type = GF_RTP_PAYT_3GPP_TEXT;
			/*fixme - this works cos there's only one PL for text in mpeg4 at the current time*/
			PL_ID = 0x10;
			break;

		}
		break;

	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP Packetizer] Unsupported stream type %x\n", streamType));
		return NULL;
	}

	/*not supported*/
	if (!rtp_type) return NULL;

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

	gf_rtp_builder_init(stream->packetizer, PayloadType, MTU, max_ptime,
	                    streamType, oti, PL_ID, MinSize, MaxSize, avgTS, maxDTSDelta, IV_length, KI_length, mpeg4mode);


	if (force_dts_delta) stream->packetizer->slMap.DTSDeltaLength = force_dts_delta;

	e = rtp_stream_init_channel(stream, MTU + 12, ip_dest, port, TTL, ifce_addr);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTP Packetizer] Failed to create RTP channel - error %s\n", gf_error_to_string(e) ));
		gf_free(stream);
		return NULL;
	}
	stream->ts_scale = slc.timestampResolution;
	stream->ts_scale /= timeScale;

	stream->buffer_alloc = MTU+12;
	stream->buffer = (char*)gf_malloc(sizeof(char) * stream->buffer_alloc);

	return stream;
}


GF_EXPORT
GF_RTPStreamer *gf_rtp_streamer_new(u32 streamType, u32 oti, u32 timeScale,
                                    const char *ip_dest, u16 port, u32 MTU, u8 TTL, const char *ifce_addr,
                                    u32 flags, char *dsi, u32 dsi_len)
{
	return gf_rtp_streamer_new_extended(streamType, oti, timeScale, ip_dest, port, MTU, TTL, ifce_addr, flags, dsi, dsi_len,

	                                    96, 0, 0, GF_FALSE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

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

void gf_media_format_ttxt_sdp(GP_RTPPacketizer *builder, char *payload_name, char *sdpLine, GF_ISOFile *file, u32 track)
{
	char buffer[2000];
	u32 w, h, i, m_w, m_h;
	s32 tx, ty;
	s16 l;
	sprintf(sdpLine, "a=fmtp:%d sver=60; ", builder->PayloadType);
	gf_isom_get_track_layout_info(file, track, &w, &h, &tx, &ty, &l);
	sprintf(buffer, "width=%d; height=%d; tx=%d; ty=%d; layer=%d; ", w, h, tx, ty, l);
	strcat(sdpLine, buffer);
	m_w = w;
	m_h = h;
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		switch (gf_isom_get_media_type(file, i+1)) {
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_VISUAL:
        case GF_ISOM_MEDIA_AUXV:
        case GF_ISOM_MEDIA_PICT:
			gf_isom_get_track_layout_info(file, i+1, &w, &h, &tx, &ty, &l);
			if (w>m_w) m_w = w;
			if (h>m_h) m_h = h;
			break;
		default:
			break;
		}
	}
	sprintf(buffer, "max-w=%d; max-h=%d", m_w, m_h);
	strcat(sdpLine, buffer);

	strcat(sdpLine, "; tx3g=");
	for (i=0; i<gf_isom_get_sample_description_count(file, track); i++) {
		char *tx3g;
		u32 tx3g_len, len;
		gf_isom_text_get_encoded_tx3g(file, track, i+1, GF_RTP_TX3G_SIDX_OFFSET, &tx3g, &tx3g_len);
		len = gf_base64_encode(tx3g, tx3g_len, buffer, 2000);
		gf_free(tx3g);
		buffer[len] = 0;
		if (i) strcat(sdpLine, ", ");
		strcat(sdpLine, buffer);
	}
}

#endif /*!defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING)*/


GF_EXPORT
GF_Err gf_rtp_streamer_append_sdp_extended(GF_RTPStreamer *rtp, u16 ESID, char *dsi, u32 dsi_len, GF_ISOFile *isofile, u32 isotrack, char *KMS_URI, u32 width, u32 height, char **out_sdp_buffer)
{
	u32 size;
	u16 port;
	char mediaName[30], payloadName[30];
	char sdp[20000], sdpLine[10000];

	if (!out_sdp_buffer) return GF_BAD_PARAM;

	gf_rtp_builder_get_payload_name(rtp->packetizer, payloadName, mediaName);
	gf_rtp_get_ports(rtp->channel, &port, NULL);

	sprintf(sdp, "m=%s %d RTP/%s %d\n", mediaName, port, rtp->packetizer->slMap.IV_length ? "SAVP" : "AVP", rtp->packetizer->PayloadType);
	sprintf(sdpLine, "a=rtpmap:%d %s/%d\n", rtp->packetizer->PayloadType, payloadName, rtp->packetizer->sl_config.timestampResolution);
	strcat(sdp, sdpLine);
	if (ESID && (rtp->packetizer->rtp_payt != GF_RTP_PAYT_3GPP_DIMS)) {
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
		gf_media_format_ttxt_sdp(rtp->packetizer, payloadName, sdpLine, isofile, isotrack);
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
	/*DIMS decoder config*/
	else if (rtp->packetizer->rtp_payt==GF_RTP_PAYT_3GPP_DIMS) {
		sprintf(sdpLine, "a=fmtp:%d Version-profile=%d", rtp->packetizer->PayloadType, 10);
		if (rtp->packetizer->flags & GP_RTP_DIMS_COMPRESSED) {
			strcat(sdpLine, ";content-coding=deflate");
		}
		strcat(sdpLine, "\n");
	}
	/*MPEG-4 Audio LATM*/
	else if (rtp->packetizer->rtp_payt==GF_RTP_PAYT_LATM) {
		GF_BitStream *bs;
		char *config_bytes;
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
GF_Err gf_rtp_streamer_append_sdp_decoding_dependency(GF_ISOFile *isofile, u32 isotrack, u8 *payload_type, char **out_sdp_buffer)
{
	u32 size, i, ref_track;
	s32 count;
	char sdp[20000], sdpLine[10000];

	sprintf(sdp, "a=mid:L%d\n", isotrack);

	count = gf_isom_get_reference_count(isofile, isotrack, GF_ISOM_REF_SCAL);
	if (count > 0)
	{
		sprintf(sdpLine, "a=depend:%d lay", payload_type[isotrack-1]);
		strcat(sdp, sdpLine);
		for (i = 0; i < (u32) count; i++)
		{
			gf_isom_get_reference(isofile, isotrack, GF_ISOM_REF_SCAL, i+1, &ref_track);
			sprintf(sdpLine, " L%d:%d", ref_track, payload_type[ref_track-1]);
			strcat(sdp, sdpLine);
		}
		strcat(sdp, "\n");
	}

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
GF_Err gf_rtp_streamer_append_sdp(GF_RTPStreamer *rtp, u16 ESID, char *dsi, u32 dsi_len, char *KMS_URI, char **out_sdp_buffer)
{
	return gf_rtp_streamer_append_sdp_extended(rtp, ESID, dsi, dsi_len, NULL, 0, KMS_URI, 0, 0, out_sdp_buffer);
}

GF_EXPORT
GF_Err gf_rtp_streamer_send_data(GF_RTPStreamer *rtp, char *data, u32 size, u32 fullsize, u64 cts, u64 dts, Bool is_rap, Bool au_start, Bool au_end, u32 au_sn, u32 sampleDuration, u32 sampleDescIndex)
{
	rtp->packetizer->sl_header.compositionTimeStamp = (u64) (cts*rtp->ts_scale);
	rtp->packetizer->sl_header.decodingTimeStamp = (u64) (dts*rtp->ts_scale);
	rtp->packetizer->sl_header.randomAccessPointFlag = is_rap;
	rtp->packetizer->sl_header.accessUnitStartFlag = au_start;
	rtp->packetizer->sl_header.accessUnitEndFlag = au_end;
	rtp->packetizer->sl_header.randomAccessPointFlag = is_rap;
	rtp->packetizer->sl_header.AU_sequenceNumber = au_sn;
	sampleDuration = (u32) (sampleDuration * rtp->ts_scale);

	return gf_rtp_builder_process(rtp->packetizer, data, size, (u8) au_end, fullsize, sampleDuration, sampleDescIndex);
}

GF_EXPORT
GF_Err gf_rtp_streamer_send_au(GF_RTPStreamer *rtp, char *data, u32 size, u64 cts, u64 dts, Bool is_rap)
{
	return gf_rtp_streamer_send_data(rtp, data, size, size, cts, dts, is_rap, GF_TRUE, GF_TRUE, 0, 0, 0);
}

GF_EXPORT
GF_Err gf_rtp_streamer_send_au_with_sn(GF_RTPStreamer *rtp, char *data, u32 size, u64 cts, u64 dts, Bool is_rap, u32 inc_au_sn)
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
	return gf_rtp_send_rtcp_report(streamer->channel, NULL, NULL);
}

GF_EXPORT
u8 gf_rtp_streamer_get_payload_type(GF_RTPStreamer *streamer)
{
	return streamer->packetizer->PayloadType;
}

#endif /*GPAC_DISABLE_STREAMING && GPAC_DISABLE_ISOM*/

