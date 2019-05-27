/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 20019
 *					All rights reserved
 *
 *  This file is part of GPAC / rtp output filter
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

#include <gpac/filters.h>
#include <gpac/ietf.h>
#include <gpac/config_file.h>
#include <gpac/base_coding.h>
#include <gpac/rtp_streamer.h>

typedef struct
{
	GF_RTPStreamer *rtp;
	u16 port;

	/*scale from TimeStamps in media timescales to TimeStamps in microseconds*/
	Double microsec_ts_scale;

	u32 id, codecid;
	Bool is_encrypted;

	/*NALU size for H264/AVC parsing*/
	u32 avc_nalu_size;

	GF_FilterPid *pid;

	u32 timescale;
	u32 nb_aus;

	u32 depends_on;
	u32 cfg_crc;


	/*loaded AU info*/
	GF_FilterPacket *pck;
	u64 current_dts, current_cts, min_dts;
	u32 current_sap, current_duration;

	u32 pck_num;
	u32 sample_duration;
	u32 sample_desc_index;

	Bool inject_ps;

	/*normalized DTS in micro-sec*/
	u64 microsec_dts;

	/*offset of CTS/DTS in media timescale, used when looping the pid*/
	u64 ts_offset;
	/*offset of CTS/DTS in microseconds, used when looping the pid*/
	u64 microsec_ts_offset;

	GF_AVCConfig *avcc;
	GF_HEVCConfig *hvcc;
	u32 rtp_ts_offset;

	s32 ts_delay;
} GF_RTPOutStream;


typedef struct
{
	//options
	char *ip;
	u32 port;
	Bool loop, xps;
	Bool mpeg4;
	u32 mtu;
	u32 ttl;
	char *ifce;
	u32 payt, tt;
	s32 delay;
	char *info, *url, *email;
	s32 runfor, tso;

	/*timeline origin of our session (all tracks) in microseconds*/
	u64 sys_clock_at_init;

	/*list of streams in session*/
	GF_List *streams;

	/*base stream if this stream contains a media decoding dependency, 0 otherwise*/
	u32 base_pid_id;

	Bool first_RTCP_sent;

	GF_RTPOutStream *active_stream;
	u32 active_stream_idx;
	u64 active_min_ts_microsec;

	GF_FilterPid *opid;

	Bool wait_for_loop;
	u64 microsec_ts_init;
} GF_RTPOutCtx;


static GF_Err rtpout_setup_sdp(GF_RTPOutCtx *ctx)
{
	FILE *sdp_out;
	char sdpLine[20000];
	u32 i, count, fsize;
	const char *ip = ctx->ip;
	if (!ip) ip = "127.0.0.1";

	sdp_out = gf_temp_file_new(NULL);
	if (!sdp_out) return GF_IO_ERR;

	count = gf_list_count(ctx->streams);

	sprintf(sdpLine, "v=0");
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "o=gpac 3357474383 1148485440000 IN IP%d %s", gf_net_is_ipv6(ip) ? 6 : 4, ip);
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "s=livesession");
	fprintf(sdp_out, "%s\n", sdpLine);

	if (ctx->info) {
		sprintf(sdpLine, "i=%s", ctx->info);
	} else {
		GF_RTPOutStream *stream = gf_list_get(ctx->streams, 0);
		const char *src = gf_filter_pid_orig_src_args(stream->pid);
		if (!src) src = gf_filter_pid_get_source_filter_name(stream->pid);
		else {
			src = gf_file_basename(src);
		}
		if (src)
			sprintf(sdpLine, "i=%s", src);
	}
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "u=%s", ctx->url ? ctx->url : "http://gpac.io");
	fprintf(sdp_out, "%s\n", sdpLine);
	if (ctx->email) {
		sprintf(sdpLine, "e=%s", ctx->email);
		fprintf(sdp_out, "%s\n", sdpLine);
	}
	sprintf(sdpLine, "c=IN IP%d %s", gf_net_is_ipv6(ip) ? 6 : 4, ip);
	fprintf(sdp_out, "%s\n", sdpLine);
	sprintf(sdpLine, "t=0 0");
	fprintf(sdp_out, "%s\n", sdpLine);
	if (gf_sys_is_test_mode()) {
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC - http://gpac.io");
	} else {
		sprintf(sdpLine, "a=x-copyright: Streamed with GPAC %s - %s", gf_gpac_version(), gf_gpac_copyright() );
	}
	fprintf(sdp_out, "%s\n", sdpLine);

	if (ctx->base_pid_id)
	{
		sprintf(sdpLine, "a=group:DDP L%d", ctx->base_pid_id);
		fprintf(sdp_out, "%s", sdpLine);
		for (i = 0; i < count; i++) {
			GF_RTPOutStream *st = gf_list_get(ctx->streams, i);
			if (st->depends_on == ctx->base_pid_id) {
				sprintf(sdpLine, " L%d", i+1);
				fprintf(sdp_out, "%s", sdpLine);
			}
		}
		fprintf(sdp_out, "\n");
	}

	for (i=0; i<count; i++) {
		char *sdp_media=NULL;
		const char *KMS = NULL;
		char *dsi = NULL;
		char *dsi_enh = NULL;
		u32 w, h, tw, th;
		s32 tx, ty;
		s16 tl;
		u32 dsi_len = 0;
		u32 dsi_enh_len = 0;
        const GF_PropertyValue *p;
		GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
		if (!stream->rtp) continue;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DECODER_CONFIG);
		if (p && p->value.data.ptr) {
			dsi = p->value.data.ptr;
			dsi_len = p->value.data.size;
		}

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
		if (p && p->value.data.ptr) {
			dsi_enh = p->value.data.ptr;
			dsi_enh_len = p->value.data.size;
		}

		w = h = tw = th = 0;
		tx = ty = 0;
		tl = 0;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_WIDTH);
		if (p) w = p->value.uint;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_HEIGHT);
		if (p) h = p->value.uint;

		p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_PROTECTION_KMS_URI);
		if (p) KMS = p->value.string;

		if (stream->codecid == GF_CODECID_TX3G) {
			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_TRANS_X);
			if (p) tx = p->value.sint;
			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_TRANS_Y);
			if (p) ty = p->value.sint;
			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_ZORDER);
			if (p) tl = p->value.sint;
			tw = w;
			th = h;

			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_WIDTH_MAX);
			if (p) w = p->value.uint;

			p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_HEIGHT_MAX);
			if (p) h = p->value.uint;
		}

		gf_rtp_streamer_append_sdp_extended(stream->rtp, stream->id, dsi, dsi_len, dsi_enh, dsi_enh_len, (char *)KMS, w, h, tw, th, tx, ty, tl, GF_FALSE, &sdp_media);

		if (ctx->base_pid_id) {
			u32 size, j;
			char sdp[20000], sdpLine[10000];

			sprintf(sdp, "a=mid:L%d\n", i+1);
			sprintf(sdpLine, "a=depend:%d lay", gf_rtp_streamer_get_payload_type(stream->rtp) );
			strcat(sdp, sdpLine);

			for (j=0; j<count; j++) {
				GF_RTPOutStream *tk = gf_list_get(ctx->streams, j);
				if (tk == stream) continue;
				if (tk->depends_on == stream->id) {
					sprintf(sdpLine, " L%d:%d", j+1, gf_rtp_streamer_get_payload_type(tk->rtp) );
					strcat(sdp, sdpLine);
				}
			}
			strcat(sdp, "\n");

			size = (u32) strlen(sdp) + (sdp_media ? (u32) strlen(sdp_media) : 0) + 1;
			if ( ! sdp_media) {
				sdp_media = (char*)gf_malloc(sizeof(char)*size);
				if (! sdp_media) return GF_OUT_OF_MEM;
				strcpy(sdp_media, sdp);
			} else {
				sdp_media = (char*)gf_realloc(sdp_media, sizeof(char)*size);
				if (! sdp_media) return GF_OUT_OF_MEM;
				strcat(sdp_media, sdp);
			}
		}
		if (sdp_media) {
			fprintf(sdp_out, "%s", sdp_media);
			gf_free(sdp_media);
		}
	}
	fprintf(sdp_out, "\n");

	fsize = (u32) gf_ftell(sdp_out);
	char *output;
	GF_FilterPacket *pck = gf_filter_pck_new_alloc(ctx->opid, fsize, &output);
	if (pck) {
		gf_fseek(sdp_out, 0, SEEK_SET);
		u32 read = (u32) fread(output, 1, fsize, sdp_out);
		if (read != fsize) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Failed to read SDP from temp file, got %d bytes but expecting %d\n", read, fsize));
			gf_filter_pck_discard(pck);
		} else {
			char c = output[fsize-1];
			output[fsize-1] = 0;
			GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] SDP file generated: %s\n", output));
			output[fsize-1] = c;
			gf_filter_pck_set_framing(pck, GF_TRUE, GF_TRUE);
			gf_filter_pck_send(pck);
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Failed to send SDP file packet\n"));
	}

	gf_fclose(sdp_out);
	return GF_OK;
}


#if 0
static Double rtpout_get_current_time(GF_RTPOutCtx *ctx)
{
    Double res = (Double) (ctx->last_min_dts - ctx->sys_clock_at_init);
    res /= 1000000;
    return res;
}
#endif


static u16 rtpout_check_next_port(GF_RTPOutCtx *ctx, u16 first_port)
{
	u32 i, count = gf_list_count(ctx->streams);
	for (i=0;i<count; i++) {
		GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
		if (stream->port==first_port) {
			return rtpout_check_next_port(ctx, (u16) (first_port+2) );
		}
	}
	return first_port;
}

static void rtpout_del_stream(GF_RTPOutStream *st)
{
	if (st->rtp) gf_rtp_streamer_del(st->rtp);
	if (st->pck) gf_filter_pid_drop_packet(st->pid);
	if (st->avcc)
		gf_odf_avc_cfg_del(st->avcc);
	if (st->hvcc)
		gf_odf_hevc_cfg_del(st->hvcc);
	gf_free(st);
}

static GF_Err rtpout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_RTPOutCtx *ctx = (GF_RTPOutCtx *) gf_filter_get_udta(filter);
	GF_Err e = GF_OK;
	GF_RTPOutStream *stream;
	u16 first_port;
	u32 flags, average_size, max_size, max_tsdelta, codecid, const_dur, nb_ch, streamType, samplerate, max_cts_offset, bandwidth, IV_length, KI_length, dsi_len, max_ptime, au_sn_len, payt;
	char *dsi;
	Bool is_crypted;
	const GF_PropertyValue *p;

	max_ptime = au_sn_len = 0;

	first_port = ctx->port;

	if (is_remove) {
		GF_RTPOutStream *t =gf_filter_pid_get_udta(pid);
		if (t) {
			if (ctx->active_stream==t) ctx->active_stream = NULL;
			gf_list_del_item(ctx->streams, t);
			rtpout_del_stream(t);
		}
		if (!gf_list_count(ctx->streams)) {
			if (ctx->opid) gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	stream = gf_filter_pid_get_udta(pid);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	streamType = p ? p->value.uint : 0;

	switch (streamType) {
	case GF_STREAM_VISUAL:
	case GF_STREAM_AUDIO:
		break;
	case GF_STREAM_FILE:
	case GF_STREAM_UNKNOWN:
		if (stream) {
			if (ctx->active_stream==stream) ctx->active_stream = NULL;
			gf_list_del_item(ctx->streams, stream);
			rtpout_del_stream(stream);
		}
		return GF_FILTER_NOT_SUPPORTED;
		break;
	default:
		break;
	}
	if (!stream) {
		GF_SAFEALLOC(stream, GF_RTPOutStream);
		if (!stream) return GF_OUT_OF_MEM;
		gf_list_add(ctx->streams, stream);
		stream->pid = pid;
		stream->min_dts = GF_FILTER_NO_TS;
	 	gf_filter_pid_set_udta(pid, stream);
	}

	dsi_len = samplerate = nb_ch = IV_length = KI_length = 0;
	is_crypted = 0;
	dsi = NULL;

	flags = 0;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	codecid = p ? p->value.uint : 0;
	if (stream->codecid && (stream->codecid != codecid)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Dynamic change of codec in RTP session not supported !\n"));
		return GF_FILTER_NOT_SUPPORTED;
	}
	stream->codecid = codecid;

	stream->is_encrypted = GF_FALSE;
	if (streamType==GF_STREAM_ENCRYPTED) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_STREAM_TYPE);
		if (p) streamType = p->value.uint;
		stream->is_encrypted = GF_TRUE;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
		if (!p || (p->value.uint != GF_ISOM_ISMACRYP_SCHEME)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Protected track with scheme type %s, cannot stream (only ISMA over RTP is supported !\n", p ? gf_4cc_to_str(p->value.uint) : "unknwon" ));
			return GF_FILTER_NOT_SUPPORTED;
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	stream->timescale = p ? p->value.uint : 1000;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CONFIG_IDX);
	stream->sample_desc_index = p ? p->value.uint : 0;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, NULL );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("sdp") );
	gf_filter_pid_set_name(ctx->opid, "SDP");


	u32 cfg_crc=0;
	dsi = NULL;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (p) {
		dsi = p->value.data.ptr;
		dsi_len = p->value.data.size;
		cfg_crc = gf_crc_32(dsi, dsi_len);
	}
	if (stream->rtp && (cfg_crc==stream->cfg_crc))
		return GF_OK;

	if (ctx->xps)
		stream->inject_ps = GF_TRUE;
	else if (stream->cfg_crc)
		stream->inject_ps = GF_TRUE;
	stream->cfg_crc = cfg_crc;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
	stream->nb_aus = p ? p->value.uint : 0;

	stream->port = rtpout_check_next_port(ctx, first_port);
	first_port = stream->port+2;

	/*init packetizer*/
	if (ctx->mpeg4) flags = GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_FORCE_MPEG4;

	switch (streamType) {
	case GF_STREAM_OD:
	case GF_STREAM_SCENE:
		//todo, check if sync shadow is used
//		if (gf_isom_has_sync_shadows(ctx->isom, stream->track_num) || gf_isom_has_sample_dependency(ctx->isom, stream->track_num))
//			flags |= GP_RTP_PCK_SYSTEMS_CAROUSEL;
		break;
	case GF_STREAM_AUDIO:
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (p) samplerate = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		if (p) nb_ch = p->value.uint;
		break;
	case GF_STREAM_VISUAL:
		break;
	default:
		break;
	}

	if (stream->avcc) gf_odf_avc_cfg_del(stream->avcc);
	stream->avcc = NULL;
	if (stream->hvcc) gf_odf_hevc_cfg_del(stream->hvcc);
	stream->hvcc = NULL;

	stream->avc_nalu_size = 0;
	switch (codecid) {
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		if (dsi) {
			GF_AVCConfig *avcc = gf_odf_avc_cfg_read(dsi, dsi_len);
			if (avcc) {
				stream->avc_nalu_size = avcc->nal_unit_size;
				if (stream->inject_ps)
					stream->avcc = avcc;
				else
					gf_odf_avc_cfg_del(avcc);
			}
		}
		break;
	case GF_CODECID_HEVC:
	case GF_CODECID_LHVC:
		if (dsi) {
			GF_HEVCConfig *hvcc = gf_odf_hevc_cfg_read(dsi, dsi_len, (codecid==GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE );
			if (hvcc) {
				stream->avc_nalu_size = hvcc->nal_unit_size;
				if (stream->inject_ps) {
					u32 i, count = gf_list_count(hvcc->param_array);
					GF_HEVCParamArray *vpsa=NULL, *spsa=NULL;
					stream->hvcc = hvcc;
					for (i=0; i<count; i++) {
						GF_HEVCParamArray *pa = gf_list_get(hvcc->param_array, i);
						if (!vpsa && (pa->type == GF_HEVC_NALU_VID_PARAM)) {
							vpsa = pa;
							gf_list_rem(hvcc->param_array, i);
							count--;
						}
						else if (!spsa && (pa->type == GF_HEVC_NALU_SEQ_PARAM)) {
							spsa = pa;
							gf_list_rem(hvcc->param_array, i);
							count--;
						}
					}
					//insert SPS at begining
					gf_list_insert(hvcc->param_array, spsa, 0);
					//insert VPS at begining - we now have VPS, SPS and other (PPS, SEI...)
					gf_list_insert(hvcc->param_array, vpsa, 0);
				} else
					gf_odf_hevc_cfg_del(hvcc);
			}
		}
		break;
	}

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_ID);
	stream->id = p ? p->value.uint : 0;

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_BITRATE);
	bandwidth = p ? p->value.uint : 0;

	/*get sample info*/
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_MAX_FRAME_SIZE);
	max_size = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_AVG_FRAME_SIZE);
	average_size = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_MAX_TS_DELTA);
	max_tsdelta = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_MAX_CTS_OFFSET);
	max_cts_offset = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_CONSTANT_DURATION);
	const_dur = p ? p->value.uint : 0;

	if (is_crypted) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISMA_SELECTIVE_ENC);
		if (p->value.boolean) {
			flags |= GP_RTP_PCK_SELECTIVE_ENCRYPTION;
		}
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISMA_IV_LENGTH);
		IV_length = p ? p->value.uint : 0;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISMA_KI_LENGTH);
		KI_length = p ? p->value.uint : 0;
	}

	payt = ctx->payt + gf_list_find(ctx->streams, stream);

	stream->rtp = gf_rtp_streamer_new_extended(streamType, codecid, stream->timescale,
				 (char *) ctx->ip ? ctx->ip : "127.0.0.1", stream->port, ctx->mtu, ctx->ttl, ctx->ifce,
				 flags, dsi, dsi_len,
				 payt, samplerate, nb_ch,
				 is_crypted, IV_length, KI_length,
				 average_size, max_size, max_tsdelta, max_cts_offset, const_dur, bandwidth, max_ptime, au_sn_len, GF_FALSE);

	if (!stream->rtp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Could not initialize RTP for stream %s: %s\n", gf_filter_pid_get_name(pid), gf_error_to_string(e)));
		return GF_NOT_SUPPORTED;
	}

	p = gf_filter_pid_get_property(stream->pid, GF_PROP_PID_DELAY);
	stream->ts_delay = p ? p->value.sint : 0;

	payt++;
	stream->microsec_ts_scale = 1000000;
	stream->microsec_ts_scale /= stream->timescale;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) {
		ctx->base_pid_id = p->value.uint;
		gf_rtp_streamer_disable_auto_rtcp(stream->rtp);
	}

	if (ctx->loop) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAYBACK_MODE);
		if (!p || (p->value.uint<GF_PLAYBACK_MODE_SEEK)) {
			ctx->loop = GF_FALSE;
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] PID %s cannot be seek, disabling loop\n", gf_filter_pid_get_name(pid) ));
		}
	}
	return GF_OK;
}

static GF_Err rtpout_initialize(GF_Filter *filter)
{
	GF_RTPOutCtx *ctx = (GF_RTPOutCtx *) gf_filter_get_udta(filter);
	if (!ctx->payt) ctx->payt = 96;
	if (!ctx->port) ctx->port = 7000;
	if (!ctx->mtu) ctx->mtu = 1450;
	if (ctx->payt<96) ctx->payt = 96;
	if (ctx->payt>127) ctx->payt = 127;
	ctx->streams = gf_list_new();
	return GF_OK;
}

static void rtpout_finalize(GF_Filter *filter)
{
	GF_RTPOutCtx *ctx = (GF_RTPOutCtx *) gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		GF_RTPOutStream *tmp = gf_list_pop_back(ctx->streams);
		rtpout_del_stream(tmp);
	}
	gf_list_del(ctx->streams);

}

static GF_Err rtpout_send_xps(GF_RTPOutStream *stream, GF_List *pslist, Bool *au_start, u32 pck_size, u32 cts, u32 dts, u32 duration)
{
	GF_Err e;
	u32 i, count = gf_list_count(pslist);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(pslist, i);
		e = gf_rtp_streamer_send_data(stream->rtp, (char *) sl->data, sl->size, pck_size, cts, dts, stream->current_sap ? 1 : 0, *au_start, GF_FALSE, stream->pck_num, duration, stream->sample_desc_index);
		if (e) return e;
		*au_start = GF_FALSE;
	}
	return GF_OK;
}

static Bool rtpout_init_clock(GF_RTPOutCtx *ctx)
{
	GF_Err e;
	u64 min_dts = GF_FILTER_NO_TS;
	u32 i, count = gf_list_count(ctx->streams);

	for (i=0; i<count; i++) {
		u64 dts;
		GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(stream->pid);
		if (!pck) return GF_FALSE;

		dts = gf_filter_pck_get_dts(pck);
		if (dts==GF_FILTER_NO_TS)
			dts = gf_filter_pck_get_cts(pck);

		if (dts==GF_FILTER_NO_TS) dts=0;

		dts *= 1000000;
		dts /= stream->timescale;
		if (min_dts > dts)
			min_dts = dts;

		if (ctx->tso>0) {
			u64 offset = ctx->tso;
			offset *= stream->timescale;
			offset /= 1000000;
			stream->rtp_ts_offset = (u32) offset;
		}
	}
	ctx->sys_clock_at_init = gf_sys_clock_high_res();
	ctx->microsec_ts_init = min_dts;
	GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] RTP clock initialized - time origin set to "LLU" us (sys clock) / "LLU" us (media clock)\n", ctx->sys_clock_at_init, ctx->microsec_ts_init));
	if (ctx->tso<0) {
		gf_rand_init(0);
		for (i=0; i<count; i++) {
			GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
			stream->rtp_ts_offset = gf_rand();
			GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] RTP stream %d initial RTP TS set to %d\n", i+1, stream->rtp_ts_offset));
		}
	}

	e = rtpout_setup_sdp(ctx);
	if (e) return e;

	if (ctx->runfor==0) {
		for (i=0; i<count; i++) {
			GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
			gf_filter_pid_set_discard(stream->pid, GF_TRUE);
		}
	}

	return GF_TRUE;
}

static GF_Err rtpout_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	GF_RTPOutStream *stream;
	u32 duration, i, count;
	s64 diff;
	u64 clock;
	const char *pck_data;
	u32 pck_size;
	u32 dts, cts;
	GF_RTPOutCtx *ctx = gf_filter_get_udta(filter);

	clock = gf_sys_clock_high_res();

	/*init session timeline - all sessions are sync'ed for packet scheduling purposes*/
	if (!ctx->sys_clock_at_init) {
		if (!rtpout_init_clock(ctx)) return GF_OK;
	}

	if (ctx->runfor>0) {
		s64 diff = clock;
		diff -= ctx->sys_clock_at_init;
		diff /= 1000;
		if ((s32) diff > ctx->runfor) {
			count = gf_list_count(ctx->streams);
			for (i=0; i<count; i++) {
				stream = gf_list_get(ctx->streams, i);
				gf_filter_pid_set_discard(stream->pid, GF_TRUE);
				stream->pck = NULL;
			}
			if (ctx->opid) gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
	}

	/*browse all inputs and locate most mature stream*/
	if (!ctx->active_stream) {
		u32 nb_eos = 0;
		count = gf_list_count(ctx->streams);

		ctx->active_min_ts_microsec = (u64) -1;
		for (i=0; i<count; i++) {
			stream = gf_list_get(ctx->streams, i);
			if (!stream->rtp) continue;

			/*load next AU*/
			if (!stream->pck) {
				stream->pck = gf_filter_pid_get_packet(stream->pid);

				if (!stream->pck) {
					if (gf_filter_pid_is_eos(stream->pid)) {
						//flush stream
						gf_rtp_streamer_send_au (stream->rtp, NULL, 0, 0, 0, GF_FALSE);
						nb_eos++;
					}
					continue;
				}
				stream->current_dts = gf_filter_pck_get_dts(stream->pck);
				stream->current_cts = gf_filter_pck_get_cts(stream->pck);
				stream->current_sap = gf_filter_pck_get_sap(stream->pck);
				duration = gf_filter_pck_get_duration(stream->pck);
				if (duration) stream->current_duration = duration;
				if (stream->current_dts==GF_FILTER_NO_TS)
					stream->current_dts = stream->current_cts;

				if (stream->min_dts==GF_FILTER_NO_TS) {
					stream->min_dts = stream->current_dts;
				}

				stream->microsec_dts = (u64) (stream->microsec_ts_scale * (s64) (stream->current_dts) + stream->microsec_ts_offset + ctx->sys_clock_at_init);
				if (stream->microsec_dts < ctx->microsec_ts_init) {
					stream->microsec_dts = 0;
					GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTPOut] next RTP packet (stream %d) has a timestamp "LLU" less than initial timestamp "LLU" - forcing to 0\n", i+1, stream->microsec_dts, ctx->microsec_ts_init));
				} else {
					stream->microsec_dts -= ctx->microsec_ts_init;
				}

				if (stream->current_sap>GF_FILTER_SAP_3) stream->current_sap = 0;
				stream->pck_num++;
				ctx->wait_for_loop = GF_FALSE;
			}

			/*check timing*/
			if (stream->pck) {
				if (ctx->active_min_ts_microsec > stream->microsec_dts) {
					ctx->active_min_ts_microsec = stream->microsec_dts;
					ctx->active_stream = stream;
					ctx->active_stream_idx = i+1;
				}
			}
		}

		/*no input data ...*/
		if (!ctx->active_stream) {
			if (nb_eos==count) {
				u64 max_dur = 0;
				if (!ctx->loop)
					return GF_EOS;

				for (i=0; i<count; i++) {
					GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);
					u64 dur = stream->current_dts + stream->current_duration - stream->min_dts;

					dur *= 1000000;
					dur /= stream->timescale;

					if (max_dur < dur) {
						max_dur = dur;
					}
				}
				if (ctx->wait_for_loop)
					return GF_OK;

				GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] RTP session done, looping source\n"));
				ctx->wait_for_loop = GF_TRUE;
				for (i=0; i<count; i++) {
					GF_FilterEvent evt;
					u64 new_ts;
					GF_RTPOutStream *stream = gf_list_get(ctx->streams, i);

					new_ts = max_dur;
					new_ts *= stream->timescale;
					new_ts /= 1000000;
					stream->ts_offset += new_ts;
					stream->microsec_ts_offset = (u64) (stream->ts_offset*(1000000.0/stream->timescale) + ctx->sys_clock_at_init);

					//loop pid: stop and play
					GF_FEVT_INIT(evt, GF_FEVT_STOP, stream->pid);
					gf_filter_pid_send_event(stream->pid, &evt);

					GF_FEVT_INIT(evt, GF_FEVT_PLAY, stream->pid);
					gf_filter_pid_send_event(stream->pid, &evt);
				}
			}
			return GF_OK;
		}
	}

	stream = ctx->active_stream;

	clock = gf_sys_clock_high_res();
	diff = (s64) ctx->active_min_ts_microsec;
	diff += (s64) ctx->delay * 1000;
	diff -= (s64) clock;

	if (diff > 1000) {
		u64 repost_in;
		//if more than 11 secs ahead of time, ask for delay minus one second, otherwise ask for half the delay
		if (diff<=11000) repost_in = diff/2;
		else repost_in = diff - 10000;
		gf_filter_ask_rt_reschedule(filter, (u32) repost_in);
//		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTPOut] next RTP packet (stream %d) scheduled in "LLU" us, requesting filter reschedule in "LLU" us - clock "LLU" us\n", ctx->active_stream_idx, diff, repost_in, clock));
		return GF_OK;
	} else if (diff<=-1000) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[RTPOut] RTP session stream %d - sending packet %d too late by %d us - clock "LLU" us\n", ctx->active_stream_idx, stream->pck_num, -diff, clock));
	} else if (diff>0){
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTPOut] RTP session stream %d - sending packet %d ahead of %d us - clock "LLU" us\n", ctx->active_stream_idx, stream->pck_num, diff, clock));
	}

	/*send packets*/
	pck_data = gf_filter_pck_get_data(stream->pck, &pck_size);
	if (!pck_size) {
		gf_filter_pid_drop_packet(stream->pid);
		stream->pck = NULL;
		ctx->active_stream = NULL;
		return GF_OK;
	}

	dts = (u32) (stream->current_dts + stream->ts_offset);
	cts = (u32) (stream->current_cts + stream->ts_offset);
	duration = stream->current_duration;

	dts += stream->rtp_ts_offset;
	cts += stream->rtp_ts_offset;
	if (stream->ts_delay>=0) {
		dts += stream->ts_delay;
		cts += stream->ts_delay;
	} else {
		if ((s32) dts >= -stream->ts_delay)
			dts += stream->ts_delay;
		else
			dts = 0;

		if ((s32) cts >= -stream->ts_delay )
			cts += stream->ts_delay;
		else
			cts = 0;
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_RTP, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_RTP, ("[RTPOut] Sending RTP packets for stream %d pck %d/%d DTS "LLU" - CTS "LLU" - RTP TS "LLU" - size %d - SAP %d - clock "LLU" us\n", ctx->active_stream_idx, stream->pck_num, stream->nb_aus, stream->current_dts, stream->current_dts, cts, pck_size, stream->current_sap, clock) );
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_RTP, ("[RTPOut] Runtime %08u ms send stream %d pck %08u/%08u DTS %08u CTS %08u RTP-TS %08u size %08u SAP %d\r", (u32) (clock - ctx->sys_clock_at_init)/1000, ctx->active_stream_idx, stream->pck_num, stream->nb_aus, (u32) stream->current_dts, (u32) stream->current_dts, (u32) cts, pck_size, stream->current_sap) );
	}
#endif

	/*we are about to send scalable base: trigger RTCP reports with the same NTP. This avoids
	NTP drift due to system clock precision which could break sync decoding*/
	if (!ctx->first_RTCP_sent || (ctx->base_pid_id && ctx->base_pid_id== stream->id)) {
		u32 ntp_sec, ntp_frac;
		/*force sending RTCP SR every RAP ? - not really compliant but we cannot perform scalable tuning otherwise*/
		u32 ntp_type = stream->current_sap ? 2 : 1;
		gf_net_get_ntp(&ntp_sec, &ntp_frac);
		count = gf_list_count(ctx->streams);

		for (i=0; i<count; i++) {
			GF_RTPOutStream *astream = gf_list_get(ctx->streams, i);
			if (!astream->pck) break;

			u32 ts = (u32) (astream->current_cts + astream->ts_offset + astream->rtp_ts_offset);
			gf_rtp_streamer_send_rtcp(stream->rtp, GF_TRUE, ts, ntp_type, ntp_sec, ntp_frac);
		}
		ctx->first_RTCP_sent = 1;
	}

	/*unpack nal units*/
	if (stream->avc_nalu_size) {
		Bool au_start, au_end;
		u32 v, size;
		u32 remain = pck_size;
		const char *ptr = pck_data;

		au_start = 1;

		if (stream->avcc && stream->current_sap) {
			e = rtpout_send_xps(stream, stream->avcc->sequenceParameterSets, &au_start, pck_size, cts, dts, duration);
			e = rtpout_send_xps(stream, stream->avcc->sequenceParameterSetExtensions, &au_start, pck_size, cts, dts, duration);
			e = rtpout_send_xps(stream, stream->avcc->pictureParameterSets, &au_start, pck_size, cts, dts, duration);
		}
		else if (stream->hvcc && stream->current_sap) {
			u32 nbps = gf_list_count(stream->hvcc->param_array);
			for (i=0; i<nbps; i++) {
				GF_HEVCParamArray *pa = gf_list_get(stream->hvcc->param_array, i);
				e = rtpout_send_xps(stream, pa->nalus, &au_start, pck_size, cts, dts, duration);
			}
		}

		while (remain) {
			size = 0;
			v = ctx->active_stream->avc_nalu_size;
			while (v) {
				size |= (u8) *ptr;
				ptr++;
				remain--;
				v-=1;
				if (v) size<<=8;
			}
			if (remain < size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Broken AVC nalu encapsulation: NALU size is %d but only %d bytes left in sample %d\n", size, remain, ctx->active_stream->pck_num));
				break;
			}
			remain -= size;
			au_end = remain ? 0 : 1;

			e = gf_rtp_streamer_send_data(stream->rtp, (char *) ptr, size, pck_size, cts, dts, stream->current_sap ? 1 : 0, au_start, au_end, stream->pck_num, duration, stream->sample_desc_index);
			ptr += size;
			au_start = 0;
		}
	} else {
		e = gf_rtp_streamer_send_data(stream->rtp, (char *) pck_data, pck_size, pck_size, cts, dts, stream->current_sap ? 1 : 0, 1, 1, stream->pck_num, duration, stream->sample_desc_index);
	}
	gf_filter_pid_drop_packet(stream->pid);
	stream->pck = NULL;

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[RTPOut] Error sending RTP packet %d: %s\n", stream->pck_num, gf_error_to_string(e) ));
	}

	ctx->active_stream = NULL;
	return e;
}

static const GF_FilterCapability RTPOutCaps[] =
{
	//anything else (not file and framed) result in manifest PID
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_OUTPUT, GF_PROP_PID_FILE_EXT, "sdp"),
	{0},
	//anything else (not file and framed) result in media pids not file
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED | GF_CAPFLAG_LOADED_FILTER,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED | GF_CAPFLAG_LOADED_FILTER, GF_PROP_PID_UNFRAMED, GF_TRUE),
};


#define OFFS(_n)	#_n, offsetof(GF_RTPOutCtx, _n)
static const GF_FilterArgs RTPOutArgs[] =
{
	{ OFFS(ip), "destination IP adress (NULL is 127.0.0.1)", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(port), "port for first stream in session", GF_PROP_UINT, "7000", NULL, 0},
	{ OFFS(loop), "loops all streams in session (not always possible depending on source type)", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(mpeg4), "sends all streams using MPEG-4 generic payload format", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(mtu), "RTP MTU size in bytes", GF_PROP_UINT, "1460", NULL, 0},
	{ OFFS(ttl), "time-to-live for muticast packets", GF_PROP_UINT, "1", NULL, 0},
	{ OFFS(ifce), "IP adress of network inteface to use", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(payt), "RTP payload type to use for dynamic configs.", GF_PROP_UINT, "96", "96-127", 0},
	{ OFFS(delay), "send delay for packet (negative means send earlier)", GF_PROP_SINT, "0", NULL, 0},
	{ OFFS(tt), "time tolerance in microseconds. Whenever schedule time minus realtime is below this value, the packet is sent right away", GF_PROP_UINT, "1000", NULL, 0},
	{ OFFS(runfor), "run for the given time in ms. Negative value means run for ever (if loop) or source duration, 0 only outputs the sdp", GF_PROP_SINT, "-1", NULL, 0},
	{ OFFS(tso), "sets timestamp offset in microsecs. Negative value means random initial timestamp", GF_PROP_SINT, "-1", NULL, 0},
	{ OFFS(xps), "force parameter set injection at each SAP. If not set, only inject if different from SDP ones", GF_PROP_BOOL, "false", NULL, 0},
	{0}
};


GF_FilterRegister RTPOutRegister = {
	.name = "rtpout",
	GF_FS_SET_DESCRIPTION("RTP Streamer")
	GF_FS_SET_HELP("The RTP streamer outputs an SDP on a file pid and streams RTP packets over UDP, starting from the indicated port.\n"\
	"The RTP packets produced have a maximum payload set by the mtu option (IP packet will be mtu + 40 bytes of IP+UDP+RTP headers)\n."
	"The real-time scheduling algorithm first initializes the clock by computing the smallest timestamp for all input pids and mapping this media time to the system clock\n"\
	"It then determines the earliest packet to send next on each input pid, potentially adding delay if set in the filter options.\n"\
	"It then compares the packet mapped timestamp TS to the system clock SC. When TS-SC is less than tt, the RTP packets for the source packet are sent\n."
	)
	.private_size = sizeof(GF_RTPOutCtx),
	.max_extra_pids = -1,
	.args = RTPOutArgs,
	.initialize = rtpout_initialize,
	.finalize = rtpout_finalize,
	SETCAPS(RTPOutCaps),
	.configure_pid = rtpout_configure_pid,
	.process = rtpout_process
};


const GF_FilterRegister *rtpout_register(GF_FilterSession *session)
{
	return &RTPOutRegister;
}

#else

const GF_FilterRegister *rtpout_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /* !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING) */
