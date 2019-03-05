/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include <gpac/maths.h>

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING)

#include <gpac/isomedia.h>
#include <gpac/ietf.h>
#include <gpac/config_file.h>
#include <gpac/base_coding.h>
#include <gpac/filestreamer.h>
#include <gpac/rtp_streamer.h>

typedef struct __tag_rtp_track
{
	struct __tag_rtp_track *next;

	GF_RTPStreamer *rtp;
	u16 port;

	/*scale from TimeStamps in media timescales to TimeStamps in microseconds*/
	Double microsec_ts_scale;

	/*NALU size for H264/AVC parsing*/
	u32 avc_nalu_size;

	/*track info*/
	u32 track_num;
	u32 timescale;
	u32 nb_aus;

	/*loaded AU info*/
	GF_ISOSample  *au;
	u32 current_au;
	u32 sample_duration;
	u32 sample_desc_index;
	/*normalized DTS in micro-sec*/
	u64 microsec_dts;

	/*offset of CTS/DTS in media timescale, used when looping the track*/
	u32 ts_offset;
	/*offset of CTS/DTS in microseconds, used when looping the track*/
	u32 microsec_ts_offset;
} GF_RTPTrack;


struct __isom_rtp_streamer
{
	GF_ISOFile *isom;
	char *dest_ip;
	Bool loop;
	Bool force_mpeg4_generic;
	/*timeline origin of our session (all tracks) in microseconds*/
	u64 timelineOrigin;
	/*list of streams in session*/
	GF_RTPTrack *stream;

	/*to sync looping sessions with tracks of # length*/
	u32 duration_ms;

	/*base track if this stream contains a media decoding dependancy, 0 otherwise*/
	u32 base_track;

	Bool first_RTCP_sent;
    u64 last_min_dts;
};



static GF_Err gf_isom_streamer_setup_sdp(GF_ISOMRTPStreamer *streamer, char*sdpfilename, char **out_sdp_buffer)
{
	GF_RTPTrack *track;
	FILE *sdp_out;
	char filename[GF_MAX_PATH];
	char sdpLine[20000];
	u32 t, count;
	u8 *payload_type;

	strcpy(filename, sdpfilename ? sdpfilename : "videosession.sdp");
	sdp_out = gf_fopen(filename, "wt");
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
		sprintf(sdpLine, "u=http://gpac.io");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "e=admin@");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "c=IN IP%d %s", gf_net_is_ipv6(streamer->dest_ip) ? 6 : 4, streamer->dest_ip);
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "t=0 0");
		fprintf(sdp_out, "%s\n", sdpLine);
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC (C)2000-2016 - http://gpac.io");
		fprintf(sdp_out, "%s\n", sdpLine);
		if (streamer->base_track)
		{
			sprintf(sdpLine, "a=group:DDP L%d", streamer->base_track);
			fprintf(sdp_out, "%s", sdpLine);
			count = gf_isom_get_track_count(streamer->isom);
			for (t = 0; t < count; t++)
			{
				if (gf_isom_has_track_reference(streamer->isom, t+1, GF_ISOM_REF_BASE, gf_isom_get_track_id(streamer->isom, streamer->base_track)))
				{
					sprintf(sdpLine, " L%d", t+1);
					fprintf(sdp_out, "%s", sdpLine);
				}
			}
			fprintf(sdp_out, "\n");
		}
	}

	/*prepare array of payload type*/
	count = gf_isom_get_track_count(streamer->isom);
	payload_type = (u8 *)gf_malloc(count * sizeof(u8));
	track = streamer->stream;
	while (track) {
		payload_type[track->track_num-1] = gf_rtp_streamer_get_payload_type(track->rtp);
		track = track->next;
	}


	track = streamer->stream;
	while (track) {
		char *sdp_media=NULL;
		const char *KMS = NULL;
		char *dsi = NULL;
		u32 w, h;
		u32 dsi_len = 0;
		GF_DecoderConfig *dcd;
        u32 mtype;
        
		//use inspect mode so that we don't aggregate xPS from the base in the enhancement ESD
		gf_isom_set_nalu_extract_mode(streamer->isom, track->track_num, GF_ISOM_NALU_EXTRACT_INSPECT);
		dcd = gf_isom_get_decoder_config(streamer->isom, track->track_num, 1);

		if (dcd && dcd->decoderSpecificInfo) {
			dsi = dcd->decoderSpecificInfo->data;
			dsi_len = dcd->decoderSpecificInfo->dataLength;
		}
		w = h = 0;
        mtype = gf_isom_get_media_type(streamer->isom, track->track_num);
		if (gf_isom_is_video_subtype(mtype)) {
			gf_isom_get_visual_info(streamer->isom, track->track_num, 1, &w, &h);
		}

		gf_isom_get_ismacryp_info(streamer->isom, track->track_num, 1, NULL, NULL, NULL, NULL, &KMS, NULL, NULL, NULL);

		/*TODO retrieve DIMS content encoding from track to set the flags */
		gf_rtp_streamer_append_sdp_extended(track->rtp, gf_isom_get_track_id(streamer->isom, track->track_num), dsi, dsi_len, streamer->isom, track->track_num, (char *)KMS, w, h, &sdp_media);
		if (streamer->base_track)
			gf_rtp_streamer_append_sdp_decoding_dependency(streamer->isom, track->track_num, payload_type, &sdp_media);
		if (sdp_media) {
			fprintf(sdp_out, "%s", sdp_media);
			gf_free(sdp_media);
		}

		if (dcd) gf_odf_desc_del((GF_Descriptor *)dcd);

		track = track->next;
	}
	fprintf(sdp_out, "\n");
    GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[FileStreamer] SDP file generated\n"));
    
	gf_fclose(sdp_out);
	if (out_sdp_buffer) {
		u64 size;
		sdp_out = gf_fopen(filename, "r");
		gf_fseek(sdp_out, 0, SEEK_END);
		size = gf_ftell(sdp_out);
		gf_fseek(sdp_out, 0, SEEK_SET);
		if (*out_sdp_buffer) gf_free(*out_sdp_buffer);
		*out_sdp_buffer = gf_malloc(sizeof(char)*(size_t)(size+1));
		size = fread(*out_sdp_buffer, 1, (size_t)size, sdp_out);
		gf_fclose(sdp_out);
		(*out_sdp_buffer)[size]=0;
	}

	gf_free(payload_type);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_streamer_write_sdp(GF_ISOMRTPStreamer *streamer, char*sdpfilename)
{
	return gf_isom_streamer_setup_sdp(streamer, sdpfilename, NULL);
}

GF_EXPORT
GF_Err gf_isom_streamer_get_sdp(GF_ISOMRTPStreamer *streamer, char **out_sdp_buffer)
{
	return gf_isom_streamer_setup_sdp(streamer, NULL, out_sdp_buffer);
}

GF_EXPORT
void gf_isom_streamer_reset(GF_ISOMRTPStreamer *streamer, Bool is_loop)
{
	GF_RTPTrack *track;
	if (!streamer) return;
	track = streamer->stream;
	while (track) {
		if (is_loop) {
			Double scale = track->timescale/1000.0;
			track->ts_offset += (u32) (streamer->duration_ms * scale);
			track->microsec_ts_offset = (u32) (track->ts_offset*(1000000.0/track->timescale) + streamer->timelineOrigin);
		} else {
			track->ts_offset += 0;
			track->microsec_ts_offset = 0;
		}
		track->current_au = 0;
		track = track->next;
	}
	if (is_loop) streamer->timelineOrigin = 0;
}

GF_EXPORT
GF_Err gf_isom_streamer_send_next_packet(GF_ISOMRTPStreamer *streamer, s32 send_ahead_delay, s32 max_sleep_time)
{
	GF_Err e = GF_OK;
	GF_RTPTrack *track, *to_send;
	u32 duration;
	s32 diff;
	u64 min_ts, dts, cts, clock;

	if (!streamer) return GF_BAD_PARAM;

	/*browse all sessions and locate most mature stream*/
	to_send = NULL;
	min_ts = (u64) -1;

	clock = gf_sys_clock_high_res();

	/*init session timeline - all sessions are sync'ed for packet scheduling purposes*/
	if (!streamer->timelineOrigin) {
		streamer->timelineOrigin = clock;
		GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[FileStreamer] RTP session %s initialized - time origin set to "LLU"\n", gf_isom_get_filename(streamer->isom), clock));
	}

	track = streamer->stream;
	while (track) {
		/*load next AU*/
		gf_isom_set_nalu_extract_mode(streamer->isom, track->track_num, GF_ISOM_NALU_EXTRACT_LAYER_ONLY);
		if (!track->au) {
			if (track->current_au >= track->nb_aus) {
				Double scale;
				if (!streamer->loop) {
					track = track->next;
					continue;
				}
				/*increment ts offset*/
				scale = track->timescale/1000.0;
				track->ts_offset += (u32) (streamer->duration_ms * scale);
				track->microsec_ts_offset = (u32) (track->ts_offset*(1000000.0/track->timescale) + streamer->timelineOrigin);
				track->current_au = 0;
			}

			track->au = gf_isom_get_sample(streamer->isom, track->track_num, track->current_au + 1, &track->sample_desc_index);
			track->current_au ++;
			if (track->au) {
				track->microsec_dts = (u64) (track->microsec_ts_scale * (s64) (track->au->DTS) + track->microsec_ts_offset + streamer->timelineOrigin);
			}
		}

		/*check timing*/
		if (track->au) {
			if (min_ts > track->microsec_dts) {
				min_ts = track->microsec_dts;
				to_send = track;
			}
		}

		track = track->next;
	}

	/*no input data ...*/
	if( !to_send) return GF_EOS;

    streamer->last_min_dts = min_ts;

	/*we are about to send scalable base: trigger RTCP reports with the same NTP. This avoids
	NTP drift due to system clock precision which could break sync decoding*/
	if (!streamer->first_RTCP_sent || (streamer->base_track && streamer->base_track==to_send->track_num)) {
		u32 ntp_sec, ntp_frac;
		/*force sending RTCP SR every RAP ? - not really compliant but we cannot perform scalable tuning otherwise*/
		u32 ntp_type = to_send->au->IsRAP ? 2 : 1;
		gf_net_get_ntp(&ntp_sec, &ntp_frac);
		track = streamer->stream;
		while (track && track->au) {
			u32 ts = (u32) (track->au->DTS + track->au->CTS_Offset + track->ts_offset);
			gf_rtp_streamer_send_rtcp(track->rtp, GF_TRUE, ts, ntp_type, ntp_sec, ntp_frac);
			track = track->next;
		}

		streamer->first_RTCP_sent = 1;
	}

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
			if (diff<10) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("WARNING: RTP session %s stream %d - sending packet %d ms too late\n", gf_isom_get_filename(streamer->isom), to_send->track_num, -diff));
			}
			break;
		}
	}

	/*send packets*/
	dts = to_send->au->DTS + to_send->ts_offset;
	cts = to_send->au->DTS + to_send->au->CTS_Offset + to_send->ts_offset;
	duration = gf_isom_get_sample_duration(streamer->isom, to_send->track_num, to_send->current_au);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[FileStreamer] Sending RTP packets for track %d AU %d/%d DTS "LLU" - CTS "LLU" - RTP TS "LLU" - size %d - RAP %d\n", to_send->track_num, to_send->current_au, to_send->nb_aus, to_send->au->DTS, to_send->au->DTS+to_send->au->CTS_Offset, cts, to_send->au->dataLength, to_send->au->IsRAP ) );

	/*unpack nal units*/
	if (to_send->avc_nalu_size) {
		Bool au_start, au_end;
		u32 v, size;
		u32 remain = to_send->au->dataLength;
		char *ptr = to_send->au->data;

		au_start = 1;
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
			if (remain < size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[rtp hinter] Broken AVC nalu encapsulation: NALU size is %d but only %d bytes left in sample %d\n", size, remain, to_send->current_au));
				break;
			}
			remain -= size;
			au_end = remain ? 0 : 1;

			e = gf_rtp_streamer_send_data(to_send->rtp, ptr, size, to_send->au->dataLength, cts, dts, (to_send->au->IsRAP==RAP) ? 1 : 0, au_start, au_end, to_send->current_au, duration, to_send->sample_desc_index);
			ptr += size;
			au_start = 0;
		}
	} else {
		e = gf_rtp_streamer_send_data(to_send->rtp, to_send->au->data, to_send->au->dataLength, to_send->au->dataLength, cts, dts, (to_send->au->IsRAP==RAP) ? 1 : 0, 1, 1, to_send->current_au, duration, to_send->sample_desc_index);
	}
	/*delete sample*/
	gf_isom_sample_del(&to_send->au);

	return e;
}

GF_EXPORT
Double gf_isom_streamer_get_current_time(GF_ISOMRTPStreamer *streamer)
{
    Double res = (Double) (streamer->last_min_dts - streamer->timelineOrigin);
    res /= 1000000;
    return res;
}


static u16 check_next_port(GF_ISOMRTPStreamer *streamer, u16 first_port)
{
	GF_RTPTrack *track = streamer->stream;
	while (track) {
		if (track->port==first_port) {
			return check_next_port(streamer, (u16) (first_port+2) );
		}
		track = track->next;
	}
	return first_port;
}

GF_EXPORT
GF_ISOMRTPStreamer *gf_isom_streamer_new(const char *file_name, const char *ip_dest, u16 port, Bool loop, Bool force_mpeg4, u32 path_mtu, u32 ttl, char *ifce_addr)
{
	GF_ISOMRTPStreamer *streamer;
	GF_Err e = GF_OK;
	const char *opt = NULL;
	/*GF_Config *configFile = NULL;	*/
	u32 i, max_ptime, au_sn_len;
	u8 payt;
	GF_ISOFile *file;
	GF_RTPTrack *track, *prev_track;
	u16 first_port;
	u32 nb_tracks;
	u32 sess_data_size;
	u32 base_track;

	if (!ip_dest) ip_dest = "127.0.0.1";
	if (!port) port = 7000;
	if (!path_mtu) path_mtu = 1450;

	GF_SAFEALLOC(streamer, GF_ISOMRTPStreamer);
	if (!streamer) return NULL;
	streamer->dest_ip = gf_strdup(ip_dest);

	payt = 96;
	max_ptime = au_sn_len = 0;

	file = gf_isom_open(file_name, GF_ISOM_OPEN_READ, NULL);
	if (!file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Error opening file %s: %s\n", opt, gf_error_to_string(gf_isom_last_error(NULL))));
		return NULL;
	}

	streamer->isom = file;
	streamer->loop = loop;
	streamer->force_mpeg4_generic = force_mpeg4;
	first_port = port;

	sess_data_size = 0;
	prev_track = NULL;

	nb_tracks = gf_isom_get_track_count(streamer->isom);
	for (i=0; i<nb_tracks; i++) {
		u32 mediaSize, mediaDuration, flags, MinSize, MaxSize, avgTS, streamType, oti, const_dur, nb_ch, samplerate, maxDTSDelta, TrackMediaSubType, TrackMediaType, bandwidth, IV_length, KI_length, dsi_len;
		const char *url, *urn;
		char *dsi;
		Bool is_crypted;

		dsi_len = samplerate = streamType = oti = nb_ch = IV_length = KI_length = 0;
		is_crypted = 0;
		dsi = NULL;

		flags = 0;

		/*we only support self-contained files for hinting*/
		gf_isom_get_data_reference(streamer->isom, i+1, 1, &url, &urn);
		if (url || urn) continue;

		TrackMediaType = gf_isom_get_media_type(streamer->isom, i+1);
		TrackMediaSubType = gf_isom_get_media_subtype(streamer->isom, i+1, 1);

		switch (TrackMediaType) {
		case GF_ISOM_MEDIA_TEXT:
			break;
		case GF_ISOM_MEDIA_VISUAL:
        case GF_ISOM_MEDIA_AUXV:
        case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_AUDIO:
		case GF_ISOM_MEDIA_SUBT:
		case GF_ISOM_MEDIA_OD:
		case GF_ISOM_MEDIA_SCENE:
			if (gf_isom_get_sample_description_count(streamer->isom, i+1) > 1) continue;
			break;
		default:
			continue;
		}

		GF_SAFEALLOC(track, GF_RTPTrack);
		if (!track) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Could not allocate file streamer track\n"));
			continue;
		}
		if (prev_track) prev_track->next = track;
		else streamer->stream = track;
		prev_track = track;

		track->track_num = i+1;

		track->nb_aus = gf_isom_get_sample_count(streamer->isom, track->track_num);
		track->timescale = gf_isom_get_media_timescale(streamer->isom, track->track_num);
		mediaDuration = (u32)(gf_isom_get_media_duration(streamer->isom, track->track_num)*1000/track->timescale); // ms
		mediaSize = (u32)gf_isom_get_media_data_size(streamer->isom, track->track_num);

		sess_data_size += mediaSize;
		if (mediaDuration > streamer->duration_ms) streamer->duration_ms = mediaDuration;

		track->port = check_next_port(streamer, first_port);
		first_port = track->port+2;

		/*init packetizer*/
		if (streamer->force_mpeg4_generic) flags = GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_FORCE_MPEG4;


		switch (TrackMediaSubType) {
		case GF_ISOM_SUBTYPE_MPEG4_CRYP:
			is_crypted = 1;
		case GF_ISOM_SUBTYPE_MPEG4:
		{
			GF_ESD *esd = gf_isom_get_esd(streamer->isom, track->track_num, 1);
			if (esd) {
				streamType = esd->decoderConfig->streamType;
				oti = esd->decoderConfig->objectTypeIndication;

				/*systems streams*/
				if (streamType==GF_STREAM_AUDIO) {
					gf_isom_get_audio_info(streamer->isom, track->track_num, 1, &samplerate, &nb_ch, NULL);
				}
				/*systems streams*/
				else if (streamType==GF_STREAM_SCENE) {
					if (gf_isom_has_sync_shadows(streamer->isom, track->track_num) || gf_isom_has_sample_dependency(streamer->isom, track->track_num))
						flags |= GP_RTP_PCK_SYSTEMS_CAROUSEL;
				}

				if (esd->decoderConfig->decoderSpecificInfo) {
					dsi = esd->decoderConfig->decoderSpecificInfo->data;
					dsi_len = esd->decoderConfig->decoderSpecificInfo->dataLength;
					esd->decoderConfig->decoderSpecificInfo->data = NULL;
					esd->decoderConfig->decoderSpecificInfo->dataLength = 0;
				}
				gf_odf_desc_del((GF_Descriptor*)esd);
			}
		}
		break;
		case GF_ISOM_SUBTYPE_AVC_H264:
		case GF_ISOM_SUBTYPE_AVC2_H264:
		case GF_ISOM_SUBTYPE_AVC3_H264:
		case GF_ISOM_SUBTYPE_AVC4_H264:
		case GF_ISOM_SUBTYPE_SVC_H264:
		case GF_ISOM_SUBTYPE_MVC_H264:
		{
			GF_AVCConfig *avcc, *svcc, *mvcc;
			avcc = gf_isom_avc_config_get(streamer->isom, track->track_num, 1);
			if (avcc)
			{
				track->avc_nalu_size = avcc->nal_unit_size;
				gf_odf_avc_cfg_del(avcc);
				streamType = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_AVC;
			}
			svcc = gf_isom_svc_config_get(streamer->isom, track->track_num, 1);
			if (svcc)
			{
				track->avc_nalu_size = svcc->nal_unit_size;
				gf_odf_avc_cfg_del(svcc);
				streamType = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_SVC;
			}
			mvcc = gf_isom_mvc_config_get(streamer->isom, track->track_num, 1);
			if (mvcc)
			{
				track->avc_nalu_size = mvcc->nal_unit_size;
				gf_odf_avc_cfg_del(mvcc);
				streamType = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_MVC;
			}
			break;
		}
		break;
		case GF_ISOM_SUBTYPE_HVC1:
		case GF_ISOM_SUBTYPE_HEV1:
		case GF_ISOM_SUBTYPE_HVC2:
		case GF_ISOM_SUBTYPE_HEV2:
		case GF_ISOM_SUBTYPE_LHV1:
		{
			GF_HEVCConfig *hevcc = NULL, *lhvcc = NULL;
			hevcc = gf_isom_hevc_config_get(streamer->isom, track->track_num, 1);
			if (hevcc) {
				track->avc_nalu_size = hevcc->nal_unit_size;
				gf_odf_hevc_cfg_del(hevcc);
				streamType = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_HEVC;
			}
			lhvcc = gf_isom_lhvc_config_get(streamer->isom, track->track_num, 1);
			if (lhvcc) {
				track->avc_nalu_size = lhvcc->nal_unit_size;
				gf_odf_hevc_cfg_del(lhvcc);
				streamType = GF_STREAM_VISUAL;
				oti = GPAC_OTI_VIDEO_LHVC;
			}
			flags |= GP_RTP_PCK_USE_MULTI;
			break;
		}
		break;
		default:
			streamType = GF_STREAM_4CC;
			oti = TrackMediaSubType;
			break;
		}

		/*get sample info*/
		gf_media_get_sample_average_infos(streamer->isom, track->track_num, &MinSize, &MaxSize, &avgTS, &maxDTSDelta, &const_dur, &bandwidth);

		if (is_crypted) {
			Bool use_sel_enc;
			gf_isom_get_ismacryp_info(streamer->isom, track->track_num, 1, NULL, NULL, NULL, NULL, NULL, &use_sel_enc, &IV_length, &KI_length);
			if (use_sel_enc) flags |= GP_RTP_PCK_SELECTIVE_ENCRYPTION;
		}

		track->rtp = gf_rtp_streamer_new_extended(streamType, oti, track->timescale,
		             (char *) streamer->dest_ip, track->port, path_mtu, ttl, ifce_addr,
		             flags, dsi, dsi_len,
		             payt, samplerate, nb_ch,
		             is_crypted, IV_length, KI_length,
		             MinSize, MaxSize, avgTS, maxDTSDelta, const_dur, bandwidth, max_ptime, au_sn_len);

		if (!track->rtp) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("Could not initialize RTP streamer: %s\n", gf_error_to_string(e)));
			goto exit;
		}

		payt++;
		track->microsec_ts_scale = 1000000;
		track->microsec_ts_scale /= gf_isom_get_media_timescale(streamer->isom, track->track_num);

		/*does this stream have the decoding dependency ?*/
		gf_isom_get_reference(streamer->isom, track->track_num, GF_ISOM_REF_BASE, 1, &base_track);
		if (base_track)
			streamer->base_track = base_track;
	}

	/*if scalable coding is found, disable auto RTCP reports and send them ourselves*/
	if (streamer->base_track) {
		GF_RTPTrack *track = streamer->stream;
		while (track) {
			gf_rtp_streamer_disable_auto_rtcp(track->rtp);
			track = track->next;
		}
	}
	return streamer;

exit:
	gf_free(streamer);
	return NULL;
}

GF_EXPORT
void gf_isom_streamer_del(GF_ISOMRTPStreamer *streamer)
{
	GF_RTPTrack *track = streamer->stream;
	while (track) {
		GF_RTPTrack *tmp = track;
		if (track->au) gf_isom_sample_del(&track->au);
		if (track->rtp) gf_rtp_streamer_del(track->rtp);
		track = track->next;
		gf_free(tmp);
	}
	if (streamer->isom) gf_isom_close(streamer->isom);
	gf_free(streamer->dest_ip);
	gf_free(streamer);
}

#endif /* !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING) */
