/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato / Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / mp4 simple streamer application
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

#include <gpac/isomedia.h>
#include <gpac/ietf.h>
#include <gpac/config_file.h>
#include <gpac/base_coding.h>
#include <gpac/internal/media_dev.h>
#include <gpac/filestreamer.h>
#include <gpac/constants.h>
#include <gpac/math.h>

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING)

typedef struct __tag_rtp_pck 
{
	struct __tag_rtp_pck *next;

	GF_RTPHeader header;
	char *payload;
	u32 payload_len;
} RTP_Packet;

typedef struct __tag_rtp_stream
{
	struct __tag_rtp_stream *next;

	u32 current_au;
	u32 nb_aus;

	u32 port;
	GF_RTPChannel *channel; 
	GP_RTPPacketizer *packetizer;

	GF_ISOSample  *au; // the AU
	u32 track;
	u32 sample_duration;
	u32 sample_desc_index;
	/*normalized DTS in micro-sec*/
	u64 microsec_dts;

	/* The previous packet which could not be sent in previous burst*/
	RTP_Packet *pck_queue;
	/* The current packet being formed */
	RTP_Packet packet;
	u32 ts_offset, microsec_ts_offset;
	u32 next_ts;

	Double ts_scale, microsec_ts_scale;
	/*NALU size for H264/AVC*/
	u32 avc_nalu_size;

	struct __file_streamer *streamer;
} RTP_Stream;


struct __file_streamer
{
	u32 path_mtu;
	char *dest_ip;

	u32 looping; // 1: play the media in a loop, 0 play once
	char *filename;
	GF_ISOFile *mp4File;
	u32 timelineOrigin; // time when the first burts was sent. this time <=> (CTS=0)
	Bool force_mpeg4_generic;

	/*list of streams in session*/
	RTP_Stream *stream;

	/*to sync looping sessions with tracks of # length*/
	u32 duration;
} RTP_Session;



#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING)

GP_RTPPacketizer *gf_rtp_packetizer_create_and_init_from_file(GF_ISOFile *file, 
															  u32 TrackNum,
															  void *cbk_obj, 
															  void (*OnNewPacket)(void *cbk, GF_RTPHeader *header),
															  void (*OnPacketDone)(void *cbk, GF_RTPHeader *header),
															  void (*OnDataReference)(void *cbk, u32 payload_size, u32 offset_from_orig),
															  void (*OnData)(void *cbk, char *data, u32 data_size, Bool is_head),
															  u32 Path_MTU, 
															  u32 max_ptime, 
															  u32 default_rtp_rate, 
															  u32 flags, 
															  u8 PayloadID, 
															  Bool copy_media, 
															  u32 InterleaveGroupID, 
															  u8 InterleaveGroupPriority)
{
	GF_SLConfig my_sl;
	u32 MinSize, MaxSize, avgTS, streamType, oti, const_dur, nb_ch, maxDTSDelta;
	u8 OfficialPayloadID;
	u32 TrackMediaSubType, TrackMediaType, hintType, required_rate, force_dts_delta, avc_nalu_size, PL_ID, bandwidth, IV_length, KI_length;
	const char *url, *urn;
	char *mpeg4mode;
	Bool is_crypted, has_mpeg4_mapping;
	GF_ESD *esd;
	GP_RTPPacketizer *rtp_packetizer = NULL;
	
	/*by default NO PL signaled*/
	PL_ID = 0;
	OfficialPayloadID = 0;
	force_dts_delta = 0;
	streamType = oti = 0;
	mpeg4mode = NULL;
	required_rate = 0;
	is_crypted = 0;
	IV_length = KI_length = 0;
	oti = 0;
	nb_ch = 0;
	avc_nalu_size = 0;
	has_mpeg4_mapping = 1;
	TrackMediaType = gf_isom_get_media_type(file, TrackNum);
	TrackMediaSubType = gf_isom_get_media_subtype(file, TrackNum, 1);
	
	/*for max compatibility with QT*/
	if (!default_rtp_rate) default_rtp_rate = 90000;

	/*timed-text is a bit special, we support multiple stream descriptions & co*/
	if ((TrackMediaType==GF_ISOM_MEDIA_TEXT) || (TrackMediaType==GF_ISOM_MEDIA_SUBT) ) {
		hintType = GF_RTP_PAYT_3GPP_TEXT;
		oti = 0x08;
		streamType = GF_STREAM_TEXT;
		/*fixme - this works cos there's only one PL for text in mpeg4 at the current time*/
		PL_ID = 0x10;
	} else {
		if (gf_isom_get_sample_description_count(file, TrackNum) > 1) return NULL;

		TrackMediaSubType = gf_isom_get_media_subtype(file, TrackNum, 1);
		switch (TrackMediaSubType) {
		case GF_ISOM_SUBTYPE_MPEG4_CRYP: 
			is_crypted = 1;
		case GF_ISOM_SUBTYPE_MPEG4:
			esd = gf_isom_get_esd(file, TrackNum, 1);
			hintType = GF_RTP_PAYT_MPEG4;
			if (esd) {
				streamType = esd->decoderConfig->streamType;
				oti = esd->decoderConfig->objectTypeIndication;
				if (esd->URLString) hintType = 0;
				/*AAC*/
				if ((streamType==GF_STREAM_AUDIO) && esd->decoderConfig->decoderSpecificInfo
				/*(nb: we use mpeg4 for MPEG-2 AAC)*/
				&& ((oti==0x40) || (oti==0x40) || (oti==0x66) || (oti==0x67) || (oti==0x68)) ) {

					u32 sample_rate;
#ifndef GPAC_DISABLE_AV_PARSERS
					GF_M4ADecSpecInfo a_cfg;
					gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg);
					nb_ch = a_cfg.nb_chan;
					sample_rate = a_cfg.base_sr;
					PL_ID = a_cfg.audioPL;
					switch (a_cfg.base_object_type) {
					case GF_M4A_AAC_MAIN:
					case GF_M4A_AAC_LC:
						if (flags & GP_RTP_PCK_USE_LATM_AAC) {
							hintType = GF_RTP_PAYT_LATM;
							break;
						}
					case GF_M4A_AAC_SBR:
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
#else
					gf_isom_get_audio_info(file, TrackNum, 1, &sample_rate, &nb_ch, NULL);
					PL_ID = 0x01;
					mpeg4mode = "AAC";
#endif
					required_rate = sample_rate;
				}
				/*MPEG1/2 audio*/
				else if ((streamType==GF_STREAM_AUDIO) && ((oti==0x69) || (oti==0x6B))) {
					u32 sample_rate;
					if (!is_crypted) {
#ifndef GPAC_DISABLE_AV_PARSERS
						GF_ISOSample *samp = gf_isom_get_sample(file, TrackNum, 1, NULL);
						u32 hdr = GF_4CC((u8)samp->data[0], (u8)samp->data[1], (u8)samp->data[2], (u8)samp->data[3]);
						nb_ch = gf_mp3_num_channels(hdr);
						sample_rate = gf_mp3_sampling_rate(hdr);
						gf_isom_sample_del(&samp);
#else
						gf_isom_get_audio_info(file, TrackNum, 1, &sample_rate, &nb_ch, NULL);
#endif
						hintType = GF_RTP_PAYT_MPEG12_AUDIO;
						/*use official RTP/AVP payload type*/
						OfficialPayloadID = 14;
						required_rate = 90000;
					}
					/*encrypted MP3 must be sent through MPEG-4 generic to signal all ISMACryp stuff*/
					else {
						u8 bps;
						gf_isom_get_audio_info(file, TrackNum, 1, &sample_rate, &nb_ch, &bps);
						required_rate = sample_rate;
					}
				}
				/*QCELP audio*/
				else if ((streamType==GF_STREAM_AUDIO) && (oti==0xE1)) {
					hintType = GF_RTP_PAYT_QCELP;
					OfficialPayloadID = 12;
					required_rate = 8000;
					streamType = GF_STREAM_AUDIO;
					nb_ch = 1;
				}
				/*EVRC/SVM audio*/
				else if ((streamType==GF_STREAM_AUDIO) && ((oti==0xA0) || (oti==0xA1)) ) {
					hintType = GF_RTP_PAYT_EVRC_SMV;
					required_rate = 8000;
					streamType = GF_STREAM_AUDIO;
					nb_ch = 1;
				}
				/*visual streams*/
				else if (streamType==GF_STREAM_VISUAL) {
					if (oti==0x20) {
#ifndef GPAC_DISABLE_AV_PARSERS
						GF_M4VDecSpecInfo dsi;
						gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
						PL_ID = dsi.VideoPL;
#else
						PL_ID = 1;
#endif
					}
					/*MPEG1/2 video*/
					if ( ((oti>=0x60) && (oti<=0x65)) || (oti==0x6A)) {
						if (!is_crypted) {
							hintType = GF_RTP_PAYT_MPEG12_VIDEO;
							OfficialPayloadID = 32;
						}
					}
					/*for ISMA*/
					if (is_crypted) {
						/*that's another pain with ISMACryp, even if no B-frames the DTS is signaled...*/
						if (oti==0x20) force_dts_delta = 22;
						flags |= GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_SIGNAL_TS;
					}

					required_rate = default_rtp_rate;
				}
				/*systems streams*/
				else if (gf_isom_has_sync_shadows(file, TrackNum) || gf_isom_has_sample_dependency(file, TrackNum)) {
					flags |= GP_RTP_PCK_AUTO_CAROUSEL;
				}
				gf_odf_desc_del((GF_Descriptor*)esd);
			}
			break;
		case GF_ISOM_SUBTYPE_3GP_H263:
			hintType = GF_RTP_PAYT_H263;
			required_rate = 90000;
			streamType = GF_STREAM_VISUAL;
			OfficialPayloadID = 34;
			/*not 100% compliant (short header is missing) but should still work*/
			oti = 0x20;
			PL_ID = 0x01;
			break;
		case GF_ISOM_SUBTYPE_3GP_AMR:
			required_rate = 8000;
			hintType = GF_RTP_PAYT_AMR;
			streamType = GF_STREAM_AUDIO;
			has_mpeg4_mapping = 0;
			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_3GP_AMR_WB:
			required_rate = 16000;
			hintType = GF_RTP_PAYT_AMR_WB;
			streamType = GF_STREAM_AUDIO;
			has_mpeg4_mapping = 0;
			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_AC3:
			hintType = GF_RTP_PAYT_AC3;
			streamType = GF_STREAM_AUDIO;
			has_mpeg4_mapping = 1;
			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_AVC_H264:
		case GF_ISOM_SUBTYPE_AVC2_H264:
		case GF_ISOM_SUBTYPE_SVC_H264:
		{
			GF_AVCConfig *avcc = gf_isom_avc_config_get(file, TrackNum, 1);
			required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
			hintType = GF_RTP_PAYT_H264_AVC;
			streamType = GF_STREAM_VISUAL;
			avc_nalu_size = avcc->nal_unit_size;
			oti = 0x21;
			PL_ID = 0x0F;
			gf_odf_avc_cfg_del(avcc);
		}
			break;
		case GF_ISOM_SUBTYPE_3GP_QCELP:
			required_rate = 8000;
			hintType = GF_RTP_PAYT_QCELP;
			streamType = GF_STREAM_AUDIO;
			oti = 0xE1;
			OfficialPayloadID = 12;
			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_3GP_EVRC:
		case GF_ISOM_SUBTYPE_3GP_SMV:
			required_rate = 8000;
			hintType = GF_RTP_PAYT_EVRC_SMV;
			streamType = GF_STREAM_AUDIO;
			oti = (TrackMediaSubType==GF_ISOM_SUBTYPE_3GP_EVRC) ? 0xA0 : 0xA1;
			nb_ch = 1;
			break;
		default:
			/*ERROR*/
			hintType = 0;
			break;
		}
	}

	/*not hintable*/
	if (!hintType) return NULL;
	/*we only support self-contained files for hinting*/
	gf_isom_get_data_reference(file, TrackNum, 1, &url, &urn);
	if (url || urn) return NULL;
	
	/*override hinter type if requested and possible*/
	if (has_mpeg4_mapping && (flags & GP_RTP_PCK_FORCE_MPEG4)) {
		hintType = GF_RTP_PAYT_MPEG4;
		avc_nalu_size = 0;
	}
	/*use static payload ID if enabled*/
	else if (OfficialPayloadID && (flags & GP_RTP_PCK_USE_STATIC_ID) ) {
		PayloadID = OfficialPayloadID;
	}

#if 0
	tmp->file = file;
	tmp->TrackNum = TrackNum;
	tmp->avc_nalu_size = avc_nalu_size;
	tmp->nb_chan = nb_ch;

	/*spatial scalability check*/
	tmp->has_ctts = gf_isom_has_time_offset(file, TrackNum);
#endif

	/*get sample info*/
	gf_media_get_sample_average_infos(file, TrackNum, &MinSize, &MaxSize, &avgTS, &maxDTSDelta, &const_dur, &bandwidth);

	/*systems carousel: we need at least IDX and RAP signaling*/
	if (flags & GP_RTP_PCK_AUTO_CAROUSEL) {
		flags |= GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_SIGNAL_AU_IDX;
	}

	/*update flags in MultiSL*/
	if (flags & GP_RTP_PCK_USE_MULTI) {
		if (MinSize != MaxSize) flags |= GP_RTP_PCK_SIGNAL_SIZE;
		if (!const_dur) flags |= GP_RTP_PCK_SIGNAL_TS;
	}
#if 0
	if (tmp->has_ctts) flags |= GP_RTP_PCK_SIGNAL_TS;
#endif

	/*default SL for RTP */
	memset(&my_sl, 0, sizeof(GF_SLConfig));
	my_sl.tag = GF_ODF_SLC_TAG;
	my_sl.useTimestampsFlag = 1;
	my_sl.timestampLength = 32;

	my_sl.timestampResolution = gf_isom_get_media_timescale(file, TrackNum);
	/*override clockrate if set*/
	if (required_rate) {
		Double sc = required_rate;
		sc /= my_sl.timestampResolution;
		maxDTSDelta = (u32) (maxDTSDelta*sc);
		my_sl.timestampResolution = required_rate;
	}
	/*switch to RTP TS*/
	max_ptime = (u32) (max_ptime * my_sl.timestampResolution / 1000);

	my_sl.AUSeqNumLength = gf_get_bit_size(gf_isom_get_sample_count(file, TrackNum));
	my_sl.CUDuration = const_dur;

	if (gf_isom_has_sync_points(file, TrackNum)) {
		my_sl.useRandomAccessPointFlag = 1;
	} else {
		my_sl.useRandomAccessPointFlag = 0;
		my_sl.hasRandomAccessUnitsOnlyFlag = 1;
	}

	if (is_crypted) {
		Bool use_sel_enc;
		gf_isom_get_ismacryp_info(file, TrackNum, 1, NULL, NULL, NULL, NULL, NULL, &use_sel_enc, &IV_length, &KI_length);
		if (use_sel_enc) flags |= GP_RTP_PCK_SELECTIVE_ENCRYPTION;
	}

	rtp_packetizer = gf_rtp_builder_new(hintType, &my_sl, flags, cbk_obj, 
								OnNewPacket, OnPacketDone, 
								/*if copy, no data ref*/
								copy_media ? NULL : OnDataReference, 
								OnData);
	
	gf_rtp_builder_init(rtp_packetizer, PayloadID, Path_MTU, max_ptime,
					   streamType, oti, PL_ID, MinSize, MaxSize, avgTS, maxDTSDelta, IV_length, KI_length, mpeg4mode);

	return rtp_packetizer;
}

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
		gf_isom_text_get_encoded_tx3g(file, track, i+1, SIDX_OFFSET_3GPP, &tx3g, &tx3g_len);
		len = gf_base64_encode(tx3g, tx3g_len, buffer, 2000);
		free(tx3g);
		buffer[len] = 0;
		if (i) strcat(sdpLine, ", ");
		strcat(sdpLine, buffer);
	}
}

#endif /*!defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING)*/


static void rtp_flush_channel(RTP_Stream *rtp)
{
	RTP_Packet *pck = rtp->pck_queue;
	while (pck) {
		RTP_Packet *tmp = pck;

		gf_rtp_send_packet(rtp->channel, &pck->header, 0, 0, pck->payload, pck->payload_len);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("RTP SN %u - TS %u - M %u - Size %u\n", pck->header.SequenceNumber, pck->header.TimeStamp, pck->header.Marker, pck->payload_len + 12));

		pck = pck->next;
		free(tmp->payload);
		free(tmp);
	}
	rtp->pck_queue = NULL;
}


/*
 * The RTP packetiser has added data to the current RTP packet
 */
static void on_pck_data(void *cbk, char *data, u32 data_size, Bool is_head) 
{
	RTP_Stream *rtp = cbk;
	if (!data ||!data_size) return;

	if (!rtp->packet.payload_len) {
		rtp->packet.payload = malloc(data_size);
		memcpy(rtp->packet.payload, data, data_size);
		rtp->packet.payload_len = data_size;
	} else {
		rtp->packet.payload = realloc(rtp->packet.payload, rtp->packet.payload_len + data_size);
		if (!is_head) {
			memcpy(rtp->packet.payload+rtp->packet.payload_len, data, data_size);
		} else {
			memmove(rtp->packet.payload+data_size, rtp->packet.payload, rtp->packet.payload_len);
			memcpy(rtp->packet.payload, data, data_size);
		}
		rtp->packet.payload_len += data_size;
	}	

} /* OnData */

/*
 * The RTP packetizer is starting a new RTP packet and is giving the header
 */
static void on_pck_new(void *cbk, GF_RTPHeader *header)
{
	RTP_Stream *rtp = cbk;
	if (!header) return;
	memcpy(&rtp->packet.header, header, sizeof(GF_RTPHeader));
} 

/*
 * The RTP packetiser is done with the current RTP packet
 * the header may have changed since the beginning of the packet (OnNewPacket)
 * 
 */
static void on_pck_done(void *cbk, GF_RTPHeader *header) 
{
	RTP_Stream *rtp = cbk;
	GF_Err e = gf_rtp_send_packet(rtp->channel, header, 0, 0, rtp->packet.payload, rtp->packet.payload_len);
	
#ifndef GPAC_DISABLE_LOG
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Error %s sending RTP packet\n", gf_error_to_string(e)));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("RTP SN %u - TS %u - M %u - Size %u\n", rtp->packet.header.SequenceNumber, rtp->packet.header.TimeStamp, rtp->packet.header.Marker, rtp->packet.payload_len + 12));
	}
#endif
	free(rtp->packet.payload);				
	rtp->packet.payload = NULL;
	rtp->packet.payload_len = 0;
}


static GF_Err gf_streamer_setup_sdp(GF_FileStreamer *streamer, char*sdpfilename, char **out_sdp_buffer) 
{	
	RTP_Stream *rtp;
	FILE *sdp_out;
	char filename[GF_MAX_PATH];
	char mediaName[30], payloadName[30];
	char sdpLine[20000];

	strcpy(filename, sdpfilename ? sdpfilename : "videosession.sdp");
	sdp_out = fopen(filename, "wt");
	if (!sdp_out) return GF_IO_ERR;

	if (!out_sdp_buffer) {
		sprintf(sdpLine, "v=0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "o=MP4Streamer 3357474383 1148485440000 IN IP%d %s", gf_net_is_ipv6(streamer->dest_ip) ? 6 : 4, streamer->dest_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "s=livesession");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "i=This is an MP4 time-sliced Streaming demo");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "u=http://gpac.sourceforge.net");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "e=admin@");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "c=IN IP%d %s", gf_net_is_ipv6(streamer->dest_ip) ? 6 : 4, streamer->dest_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "t=0 0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC (C)2000-200X - http://gpac.sourceforge.net\n");
		fprintf(sdp_out, "%s\n", sdpLine);
	}

	rtp = streamer->stream;
	while (rtp) {
		gf_rtp_builder_get_payload_name(rtp->packetizer, payloadName, mediaName);
		
		sprintf(sdpLine, "m=%s %d RTP/%s %d", mediaName, rtp->port, rtp->packetizer->slMap.IV_length ? "SAVP" : "AVP", rtp->packetizer->PayloadType);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=rtpmap:%d %s/%d", rtp->packetizer->PayloadType, payloadName, rtp->packetizer->sl_config.timestampResolution);
		fprintf(sdp_out, "%s\n", sdpLine);
		
		if (gf_isom_get_media_type(streamer->mp4File, rtp->track) == GF_ISOM_MEDIA_VISUAL) {
			u32 w, h;
			w = h = 0;
			gf_isom_get_visual_info(streamer->mp4File, rtp->track, 1, &w, &h);
			if (rtp->packetizer->rtp_payt == GF_RTP_PAYT_H263) {
				sprintf(sdpLine, "a=cliprect:0,0,%d,%d", h, w);
			}
			/*extensions for some mobile phones*/
			sprintf(sdpLine, "a=framesize:%d %d-%d", rtp->packetizer->PayloadType, w, h);
		}
		/*AMR*/
		if ((rtp->packetizer->rtp_payt == GF_RTP_PAYT_AMR) || (rtp->packetizer->rtp_payt == GF_RTP_PAYT_AMR_WB)) {
			sprintf(sdpLine, "a=fmtp:%d octet-align", rtp->packetizer->PayloadType);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*Text*/
		else if (rtp->packetizer->rtp_payt == GF_RTP_PAYT_3GPP_TEXT) {
			gf_media_format_ttxt_sdp(rtp->packetizer, payloadName, sdpLine, streamer->mp4File, rtp->track);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*EVRC/SMV in non header-free mode*/
		else if ((rtp->packetizer->rtp_payt == GF_RTP_PAYT_EVRC_SMV) && (rtp->packetizer->auh_size>1)) {
			sprintf(sdpLine, "a=fmtp:%d maxptime=%d", rtp->packetizer->PayloadType, rtp->packetizer->auh_size*20);
			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*H264/AVC*/
		else if (rtp->packetizer->rtp_payt == GF_RTP_PAYT_H264_AVC) {
			GF_AVCConfig *avcc = gf_isom_avc_config_get(streamer->mp4File, rtp->track, 1);
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
			fprintf(sdp_out, "%s\n", sdpLine);
			rtp->avc_nalu_size = avcc->nal_unit_size;
			gf_odf_avc_cfg_del(avcc);
		}
		/*MPEG-4 decoder config*/
		else if (rtp->packetizer->rtp_payt==GF_RTP_PAYT_MPEG4) {
			GF_DecoderConfig *dcd;
			dcd = gf_isom_get_decoder_config(streamer->mp4File, rtp->track, 1);

			if (dcd && dcd->decoderSpecificInfo && dcd->decoderSpecificInfo->data) {
				gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, dcd->decoderSpecificInfo->data, dcd->decoderSpecificInfo->dataLength);
			} else {
				gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, NULL, 0);
			}
			if (dcd) gf_odf_desc_del((GF_Descriptor *)dcd);

			if (rtp->packetizer->slMap.IV_length) {
				const char *kms;
				gf_isom_get_ismacryp_info(streamer->mp4File, rtp->track, 1, NULL, NULL, NULL, NULL, &kms, NULL, NULL, NULL);
				if (!strnicmp(kms, "(key)", 5) || !strnicmp(kms, "(ipmp)", 6) || !strnicmp(kms, "(uri)", 5)) {
					strcat(sdpLine, "; ISMACrypKey=");
				} else {
					strcat(sdpLine, "; ISMACrypKey=(uri)");
				}
				strcat(sdpLine, kms);
			}

			fprintf(sdp_out, "%s\n", sdpLine);
		}
		/*MPEG-4 Audio LATM*/
		else if (rtp->packetizer->rtp_payt==GF_RTP_PAYT_LATM) { 
			GF_DecoderConfig *dcd;
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

			/* audio-specific config */ 
			dcd = gf_isom_get_decoder_config(streamer->mp4File, rtp->track, 1); 
			if (dcd) { 
				gf_bs_write_data(bs, dcd->decoderSpecificInfo->data, dcd->decoderSpecificInfo->dataLength); 
				gf_odf_desc_del((GF_Descriptor *)dcd); 
			} 

			/* other data */ 
			gf_bs_write_int(bs, 0, 3); /* frameLengthType */ 
			gf_bs_write_int(bs, 0xff, 8); /* latmBufferFullness */ 
			gf_bs_write_int(bs, 0, 1); /* otherDataPresent */ 
			gf_bs_write_int(bs, 0, 1); /* crcCheckPresent */ 
			gf_bs_get_content(bs, &config_bytes, &config_size); 
			gf_bs_del(bs); 

			gf_rtp_builder_format_sdp(rtp->packetizer, payloadName, sdpLine, config_bytes, config_size); 
			fprintf(sdp_out, "%s\n", sdpLine);
			free(config_bytes); 
		}
		rtp = rtp->next;
	}
	fprintf(sdp_out, "\n");

	fclose(sdp_out);
	if (out_sdp_buffer) {
		u32 size;
		sdp_out = fopen(filename, "r");
		fseek(sdp_out, 0, SEEK_END);
		size = ftell(sdp_out);
		fseek(sdp_out, 0, SEEK_SET);
		if (*out_sdp_buffer) free(*out_sdp_buffer);
		*out_sdp_buffer = malloc(sizeof(char)*(size+1));
		size = fread(*out_sdp_buffer, 1, size, sdp_out);
		fclose(sdp_out);
		(*out_sdp_buffer)[size]=0;
	}

	return GF_OK;
} 

GF_Err gf_streamer_write_sdp(GF_FileStreamer *streamer, char*sdpfilename) 
{
	return gf_streamer_setup_sdp(streamer, sdpfilename, NULL);
}

GF_Err gf_streamer_get_sdp(GF_FileStreamer *streamer, char **out_sdp_buffer) 
{
	return gf_streamer_setup_sdp(streamer, NULL, out_sdp_buffer);
}

static GF_Err rtp_init_channel(RTP_Stream *rtp, u32 path_mtu, char * dest, int port, int ttl, char *ifce_addr)
{
	GF_RTSPTransport tr;
	GF_Err res;

	rtp->channel = gf_rtp_new();
	gf_rtp_set_ports(rtp->channel, 0);

	memset(&tr, 0, sizeof(GF_RTSPTransport));

	tr.IsUnicast = gf_sk_is_multicast_address(dest) ? 0 : 1;
	tr.Profile="RTP/AVP";
	tr.destination = dest;
	tr.source = "0.0.0.0";
	tr.IsRecord = 0;
	tr.Append = 0;
	tr.SSRC = rand();
	tr.TTL = ttl;

	tr.port_first = port;
	tr.port_last = port+1;
	if (tr.IsUnicast) {
		tr.client_port_first = port;
		tr.client_port_last  = port+1;
	} else {
		tr.source = dest;
	}

	res = gf_rtp_setup_transport(rtp->channel, &tr, dest);
	if (res !=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Cannot setup RTP transport info: %s\n", gf_error_to_string(res) ));
		return res;
	}

	res = gf_rtp_initialize(rtp->channel, 0, 1, path_mtu, 0, 0, ifce_addr);
	if (res !=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Cannot initialize RTP sockets: %s\n", gf_error_to_string(res) ));
		return res;
	}
	return GF_OK;
} 

void gf_streamer_reset(GF_FileStreamer *streamer, Bool is_loop)
{
	RTP_Stream *rtp;
	if (!streamer) return;
	rtp = streamer->stream;
	while (rtp) {
		if (is_loop) {
			Double scale = rtp->packetizer->sl_config.timestampResolution/1000.0;
			rtp->ts_offset += (u32) (streamer->duration * scale);
			rtp->microsec_ts_offset = (u32) (rtp->ts_offset*(1000000.0/rtp->packetizer->sl_config.timestampResolution)) + streamer->timelineOrigin;
		} else {
			rtp->ts_offset += 0;
			rtp->microsec_ts_offset = 0;
		}
		rtp->current_au = 0;
		rtp = rtp->next;
	}
	if (is_loop) streamer->timelineOrigin = 0;
}

GF_Err gf_streamer_send_next_packet(GF_FileStreamer *streamer, s32 send_ahead_delay, s32 max_sleep_time) 
{
	GF_Err e = GF_OK;
	RTP_Stream *rtp, *to_send;
	u32 time;
	s32 diff;
	u64 min_ts;

	if (!streamer) return GF_BAD_PARAM;
	
	/*browse all sessions and locate most mature stream*/
	to_send = NULL;
	min_ts = (u64) -1;

	time = gf_sys_clock();

	/*init session timeline - all sessions are sync'ed for packet scheduling purposes*/
	if (!streamer->timelineOrigin)
		streamer->timelineOrigin = time*1000;

	rtp = streamer->stream;
	while (rtp) {
		/*load next AU*/
		if (!rtp->au) {
			if (rtp->current_au >= rtp->nb_aus) {
				Double scale;
				if (!streamer->looping) {
					rtp = rtp->next;
					continue;
				}
				/*increment ts offset*/
				scale = rtp->packetizer->sl_config.timestampResolution/1000.0;
				rtp->ts_offset += (u32) (streamer->duration * scale);
				rtp->microsec_ts_offset = (u32) (rtp->ts_offset*(1000000.0/rtp->packetizer->sl_config.timestampResolution)) + streamer->timelineOrigin;
				rtp->current_au = 0;
			}

			rtp->au = gf_isom_get_sample(streamer->mp4File, rtp->track, rtp->current_au + 1, &rtp->sample_desc_index);
			rtp->current_au ++;
			if (rtp->au) {
				u64 ts;
				rtp->sample_duration = gf_isom_get_sample_duration(streamer->mp4File, rtp->track, rtp->current_au);
				rtp->sample_duration = (u32)(rtp->sample_duration*rtp->ts_scale);

				rtp->microsec_dts = (u64) (rtp->microsec_ts_scale * (s64) (rtp->au->DTS)) + rtp->microsec_ts_offset + streamer->timelineOrigin;

				ts = (u64) (rtp->ts_scale * (s64) (rtp->au->DTS));
				rtp->packetizer->sl_header.decodingTimeStamp = ts + rtp->ts_offset;
										
				ts = (u64) (rtp->ts_scale * (s64) (rtp->au->DTS+rtp->au->CTS_Offset));
				rtp->packetizer->sl_header.compositionTimeStamp = ts + rtp->ts_offset;
					
				rtp->packetizer->sl_header.randomAccessPointFlag = rtp->au->IsRAP;
			}
		}

		/*check timing*/
		if (rtp->au) {
			if (min_ts > rtp->microsec_dts) {
				min_ts = rtp->microsec_dts;
				to_send = rtp;
			}
		}

		rtp = rtp->next;
	}

	/*no input data ...*/
	if( !to_send) return GF_EOS;
	min_ts /= 1000;

	if (max_sleep_time) {
		diff = ((u32) min_ts) - gf_sys_clock();	
		if (diff>max_sleep_time) 
			return GF_OK;
	}

	/*sleep until TS is mature*/
	while (1) {
		diff = ((u32) min_ts) - gf_sys_clock();
		
		if (diff > send_ahead_delay) {
			gf_sleep(1);
		} else {
			if (diff<0) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("WARNING: RTP session %s stream %d - sending packet %d ms too late\n", streamer->filename, to_send->track, -diff));
			}
			break;
		}
	}

	/*send packets*/

	/*unpack nal units*/
	if (to_send->avc_nalu_size) {
		u32 v, size;
		u32 remain = to_send->au->dataLength;
		char *ptr = to_send->au->data;

		to_send->packetizer->sl_header.accessUnitStartFlag = 1;
		to_send->packetizer->sl_header.accessUnitEndFlag = 0;
		while (remain) {
			size = 0;
			v = to_send->avc_nalu_size;
			while (v) {
				size |= (u8) *ptr;
				ptr++;
				remain--;
				v-=1;
				if (v) size<<=8;
			}
			remain -= size;
			to_send->packetizer->sl_header.accessUnitEndFlag = remain ? 0 : 1;
			e = gf_rtp_builder_process(to_send->packetizer, ptr, size, (u8) !remain, to_send->au->dataLength, to_send->sample_duration, (u8) to_send->sample_desc_index );
			ptr += size;
			to_send->packetizer->sl_header.accessUnitStartFlag = 0;
		}
	} else {
		e = gf_rtp_builder_process(to_send->packetizer, to_send->au->data, to_send->au->dataLength, (u8) 1, to_send->au->dataLength, to_send->sample_duration, (u8) to_send->sample_desc_index);
	}
	to_send->packetizer->sl_header.AU_sequenceNumber += 1;
	/*delete sample*/
	gf_isom_sample_del(&to_send->au);

	return e;
}


static u16 check_next_port(GF_FileStreamer *streamer, u16 first_port)
{
	RTP_Stream *rtp = streamer->stream;
	while (rtp) {
		if (rtp->port==first_port) {
			return check_next_port(streamer, (u16) (first_port+2) );
		}
		rtp = rtp->next;
	}
	return first_port;
}

GF_FileStreamer *gf_streamer_new(const char *file_name, const char *ip_dest, u16 port, Bool loop, Bool force_mpeg4, u32 path_mtu, u32 ttl, char *ifce_addr)
{
	GF_FileStreamer *streamer;
	GF_Err e = GF_OK;
	const char *opt = NULL;
	const char *dest_ip;
	GF_Config *configFile = NULL;	
	u32 i;	
	u8 payt;			
	GF_ISOFile *file;
	RTP_Stream *rtp, *prev_stream;
	u16 first_port;
	u32 nb_tracks;
	u32 sess_data_size;

	if (!ip_dest) ip_dest = "127.0.0.1";
	if (!port) port = 7000;
	if (!path_mtu) path_mtu = 1450;

	GF_SAFEALLOC(streamer, GF_FileStreamer);
	streamer->path_mtu = path_mtu;
	streamer->dest_ip = strdup(ip_dest);

	dest_ip = ip_dest;
	payt = 96;


	file = gf_isom_open(file_name, GF_ISOM_OPEN_READ, NULL);
	if (!file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Error opening file %s: %s\n", opt, gf_error_to_string(gf_isom_last_error(NULL))));
		return NULL;
	}

	streamer->mp4File = file;
	streamer->looping = loop;
	streamer->force_mpeg4_generic = force_mpeg4;
	first_port = port;

	sess_data_size = 0;
	prev_stream = NULL;
	
	nb_tracks = gf_isom_get_track_count(streamer->mp4File);
	for (i=0; i<nb_tracks; i++) {
		u32 mediaTimescale; 
		u32 mediaSize;
		u32 mediaDuration;
		u32 flags = 0;

		switch (gf_isom_get_media_type(streamer->mp4File, i+1)) {
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_SCENE:
			break;
		default:
			continue;
		}

		GF_SAFEALLOC(rtp, RTP_Stream);
		if (prev_stream) prev_stream->next = rtp;
		else streamer->stream = rtp;
		prev_stream = rtp;

		rtp->streamer = streamer;
		rtp->track = i+1;

		rtp->nb_aus = gf_isom_get_sample_count(streamer->mp4File, rtp->track);
		mediaTimescale = gf_isom_get_media_timescale(streamer->mp4File, rtp->track);
		mediaDuration = (u32)(gf_isom_get_media_duration(streamer->mp4File, rtp->track)*1000/mediaTimescale); // ms
		mediaSize = (u32)gf_isom_get_media_data_size(streamer->mp4File, rtp->track);

		sess_data_size += mediaSize;
		if (mediaDuration > streamer->duration) streamer->duration = mediaDuration;

		rtp->port = check_next_port(streamer, first_port);
		first_port = rtp->port+2;

		e = rtp_init_channel(rtp, streamer->path_mtu + 12, (char *) dest_ip, rtp->port, ttl, ifce_addr);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Could not initialize RTP Channel: %s\n", gf_error_to_string(e)));
			goto exit;
		}		
		
		/*init packetizer*/
		if (streamer->force_mpeg4_generic) flags = GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_FORCE_MPEG4;

		rtp->packetizer = gf_rtp_packetizer_create_and_init_from_file(streamer->mp4File, rtp->track, rtp,
													on_pck_new, on_pck_done, NULL, on_pck_data,
													streamer->path_mtu, 0, 0, flags, 
													payt, 0, 0, 0);
		if (!rtp->packetizer) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Could not initialize Packetizer\n"));
			e = GF_SERVICE_ERROR;
			goto exit;
		}
		payt++;

		rtp->ts_scale = rtp->packetizer->sl_config.timestampResolution;
		rtp->ts_scale /= gf_isom_get_media_timescale(streamer->mp4File, rtp->track);

		rtp->microsec_ts_scale = 1000000;
		rtp->microsec_ts_scale /= gf_isom_get_media_timescale(streamer->mp4File, rtp->track);
	}
	return streamer;

exit:
	free(streamer);
	return NULL;
} 

void gf_streamer_del(GF_FileStreamer *streamer) 
{
	RTP_Stream *rtp = streamer->stream;
	while (rtp) {
		RTP_Stream *tmp = rtp;
		rtp_flush_channel(rtp);
		if (rtp->au) gf_isom_sample_del(&rtp->au);
		if (rtp->channel) gf_rtp_del(rtp->channel);
		if (rtp->packetizer) gf_rtp_builder_del(rtp->packetizer);
		if (rtp->packet.payload) free(rtp->packet.payload);	
		rtp = rtp->next;
		free(tmp);
	}
	if (streamer->mp4File) gf_isom_close(streamer->mp4File);
	free(streamer->dest_ip);
	free(streamer);
}

#endif /* !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING) */
