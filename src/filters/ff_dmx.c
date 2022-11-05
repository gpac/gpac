/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / ffmpeg demux filter
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

#include <gpac/setup.h>

#ifdef GPAC_HAS_FFMPEG

#include "ff_common.h"

//for NTP clock
#include <gpac/network.h>
#include <gpac/bitstream.h>
#include <libavutil/mastering_display_metadata.h>

enum
{
	COPY_NO,
	COPY_A,
	COPY_V,
	COPY_AV
};

typedef struct
{
	GF_FilterPid *pid;
	u64 ts_offset;
	Bool mkv_webvtt;
} PidCtx;

typedef struct
{
	//options
	const char *src;
	u32 block_size;
	u32 copy, probes;
	Bool sclock;
	const char *fmt, *dev;

	//internal data
	const char *fname;
	u32 log_class;

	Bool raw_data;
	//input file
	AVFormatContext *demuxer;
	//demux options
	AVDictionary *options;

	PidCtx *pids_ctx;

	Bool raw_pck_out;
	u32 nb_streams;
	u32 nb_playing, nb_stop_pending;
	Bool stop_seen;
	u64 first_sample_clock, last_frame_ts;
	u32 probe_frames;
    u32 nb_pck_sent;
	Double last_play_start_range;

#if (LIBAVFORMAT_VERSION_MAJOR < 59)
	AVPacket pkt;
#else
	AVPacket *pkt;
#endif

	s32 audio_idx, video_idx;

	u64 *probe_times;

	Bool copy_audio, copy_video;

	u8 *avio_ctx_buffer;
	AVIOContext *avio_ctx;
	FILE *gfio;
	GF_Fraction fps_forced;
} GF_FFDemuxCtx;

static void ffdmx_finalize(GF_Filter *filter)
{
	GF_FFDemuxCtx *ctx = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);
	if (ctx->pids_ctx)
		gf_free(ctx->pids_ctx);
	if (ctx->options)
		av_dict_free(&ctx->options);
	if (ctx->probe_times)
		gf_free(ctx->probe_times);
	if (ctx->demuxer) {
		avformat_close_input(&ctx->demuxer);
		avformat_free_context(ctx->demuxer);
	}
	if (ctx->avio_ctx) {
		if (ctx->avio_ctx->buffer) av_freep(&ctx->avio_ctx->buffer);
		av_freep(&ctx->avio_ctx);
	}
	if (ctx->gfio) gf_fclose(ctx->gfio);
	return;
}

void ffdmx_shared_pck_release(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_FFDemuxCtx *ctx = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);
	if (ctx->raw_pck_out) {
#if (LIBAVFORMAT_VERSION_MAJOR < 59)
		av_free_packet(&ctx->pkt);
#else
		av_packet_unref(ctx->pkt);
#endif
		ctx->raw_pck_out = GF_FALSE;
		gf_filter_post_process_task(filter);
	}
}

static void ffdmx_set_decoder_config(GF_FilterPid *pid, const u8 *exdata, u32 exdata_size, u32 gpac_codec_id)
{
	u8 *dsi;
	u32 dsi_size;
	GF_Err e = ffmpeg_extradata_to_gpac(gpac_codec_id, exdata, exdata_size, &dsi, &dsi_size);
	if (!e)
		gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY( dsi, dsi_size) );
}

static inline u32 rescale_mdcv(AVRational q, int b)
{
    return (u32) av_rescale(q.num, b, q.den);
}

//dovi_meta.h not exported in old releases, just redefine
typedef struct {
    u8 dv_version_major;
    u8 dv_version_minor;
    u8 dv_profile;
    u8 dv_level;
    u8 rpu_present_flag;
    u8 el_present_flag;
    u8 bl_present_flag;
    u8 dv_bl_signal_compatibility_id;
} Ref_FFAVDoviRecord;

static void ffdmx_parse_side_data(const AVPacketSideData *sd, GF_FilterPid *pid)
{
	switch (sd->type) {
	case AV_PKT_DATA_PALETTE:
		break;
	case AV_PKT_DATA_NEW_EXTRADATA:
		break;
	case AV_PKT_DATA_PARAM_CHANGE:
		break;
	case AV_PKT_DATA_H263_MB_INFO:
		break;
	case AV_PKT_DATA_REPLAYGAIN:
		break;
	case AV_PKT_DATA_STEREO3D:
		break;
	case AV_PKT_DATA_AUDIO_SERVICE_TYPE:
		break;
	case AV_PKT_DATA_QUALITY_STATS:
		break;
	case AV_PKT_DATA_CPB_PROPERTIES:
		break;
	case AV_PKT_DATA_DISPLAYMATRIX:
	{
		GF_PropertyValue p;
		memset(&p, 0, sizeof(GF_PropertyValue));
		p.type = GF_PROP_SINT_LIST;
		p.value.uint_list.nb_items = 9;
		p.value.uint_list.vals = (u32 *)sd->data;
		gf_filter_pid_set_property(pid, GF_PROP_PID_ISOM_TRACK_MATRIX, &p);

	}
		break;
	case AV_PKT_DATA_SPHERICAL:
		break;

#if (LIBAVCODEC_VERSION_MAJOR<=58) && (LIBAVCODEC_VERSION_MINOR<75)
#else
	case AV_PKT_DATA_MASTERING_DISPLAY_METADATA:
	{
		u8 mdcv[24];
		const int chroma_den = 50000;
		const int luma_den = 10000;
		memset(mdcv, 0, sizeof(u8)*24);
		const AVMasteringDisplayMetadata *metadata = (const AVMasteringDisplayMetadata *)sd->data;
		GF_BitStream *bs = gf_bs_new(mdcv, 24, GF_BITSTREAM_WRITE);
		gf_bs_write_u16(bs, rescale_mdcv(metadata->display_primaries[1][0], chroma_den));
		gf_bs_write_u16(bs, rescale_mdcv(metadata->display_primaries[1][1], chroma_den));
    	gf_bs_write_u16(bs, rescale_mdcv(metadata->display_primaries[2][0], chroma_den));
    	gf_bs_write_u16(bs, rescale_mdcv(metadata->display_primaries[2][1], chroma_den));
    	gf_bs_write_u16(bs, rescale_mdcv(metadata->display_primaries[0][0], chroma_den));
    	gf_bs_write_u16(bs, rescale_mdcv(metadata->display_primaries[0][1], chroma_den));
    	gf_bs_write_u16(bs, rescale_mdcv(metadata->white_point[0], chroma_den));
    	gf_bs_write_u16(bs, rescale_mdcv(metadata->white_point[1], chroma_den));
    	gf_bs_write_u32(bs, rescale_mdcv(metadata->max_luminance, luma_den));
    	gf_bs_write_u32(bs, rescale_mdcv(metadata->min_luminance, luma_den));
    	gf_bs_del(bs);
		gf_filter_pid_set_property(pid, GF_PROP_PID_MASTER_DISPLAY_COLOUR, &PROP_DATA(mdcv, 24));
	}
		break;

	case AV_PKT_DATA_CONTENT_LIGHT_LEVEL:
	{
		u8 clli[4];
		memset(clli, 0, sizeof(u8)*4);
		const AVContentLightMetadata *metadata = (const AVContentLightMetadata *)sd->data;
		GF_BitStream *bs = gf_bs_new(clli, 4, GF_BITSTREAM_WRITE);
		gf_bs_write_u16(bs, metadata->MaxCLL);
		gf_bs_write_u16(bs, metadata->MaxFALL);
    	gf_bs_del(bs);
		gf_filter_pid_set_property(pid, GF_PROP_PID_CONTENT_LIGHT_LEVEL, &PROP_DATA(clli, 4));
	}
		break;
	case AV_PKT_DATA_ICC_PROFILE:
		gf_filter_pid_set_property(pid, GF_PROP_PID_ICC_PROFILE, &PROP_DATA(sd->data, (u32) sd->size));
		break;
	case AV_PKT_DATA_DOVI_CONF:
	{
		u8 dv_cfg[24];
		const Ref_FFAVDoviRecord *dovi = (const Ref_FFAVDoviRecord *)sd->data;
		memset(dv_cfg, 0, sizeof(u8)*24);
		GF_BitStream *bs = gf_bs_new(dv_cfg, 24, GF_BITSTREAM_WRITE);
		gf_bs_write_u8(bs, dovi->dv_version_major);
		gf_bs_write_u8(bs, dovi->dv_version_minor);
		gf_bs_write_int(bs, dovi->dv_profile, 7);
		gf_bs_write_int(bs, dovi->dv_level, 6);
		gf_bs_write_int(bs, dovi->rpu_present_flag, 1);
		gf_bs_write_int(bs, dovi->el_present_flag, 1);
		gf_bs_write_int(bs, dovi->bl_present_flag, 1);
		gf_bs_write_int(bs, dovi->dv_bl_signal_compatibility_id, 4);
		//the rest is zero-reserved
		gf_bs_write_int(bs, 0, 28);
		gf_bs_write_u32(bs, 0);
		gf_bs_write_u32(bs, 0);
		gf_bs_write_u32(bs, 0);
		gf_bs_write_u32(bs, 0);
		gf_bs_del(bs);
		gf_filter_pid_set_property(pid, GF_PROP_PID_DOLBY_VISION, &PROP_DATA(dv_cfg, 24));
	}
		break;
	case AV_PKT_DATA_S12M_TIMECODE:
		break;
#endif
	default:
		break;
	}
}

static GF_Err ffdmx_process(GF_Filter *filter)
{
	GF_Err e;
	u32 i;
	u64 sample_time;
	u8 *data_dst;
	Bool copy = GF_TRUE;
	Bool check_webvtt = GF_FALSE;
	GF_FilterPacket *pck_dst;
	AVPacket *pkt;
	PidCtx *pctx;
	GF_FFDemuxCtx *ctx = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);

	if (!ctx->nb_playing)
		return GF_EOS;

	if (ctx->raw_pck_out)
		return GF_EOS;

	u32 would_block=0, pids=0;
	for (i=0; i<ctx->demuxer->nb_streams; i++) {
		if (!ctx->pids_ctx[i].pid) continue;
		pids++;
		if (!gf_filter_pid_is_playing(ctx->pids_ctx[i].pid))
			would_block++;
		else if (gf_filter_pid_would_block(ctx->pids_ctx[i].pid))
			would_block++;
	}
	if (would_block == pids) {
		gf_filter_ask_rt_reschedule(filter, 0);
		return GF_OK;
	}
	
	sample_time = gf_sys_clock_high_res();

	FF_INIT_PCK(ctx, pkt)
	pkt->side_data = NULL;
	pkt->side_data_elems = 0;

	pkt->stream_index = -1;

	/*EOF*/
	if (av_read_frame(ctx->demuxer, pkt) <0) {
		FF_FREE_PCK(pkt);
		if (!ctx->raw_data) {
			for (i=0; i<ctx->nb_streams; i++) {
				if (ctx->pids_ctx[i].pid) gf_filter_pid_set_eos(ctx->pids_ctx[i].pid);
			}
			return GF_EOS;
		}
		return GF_OK;
	}
	assert(pkt->stream_index>=0);
	assert(pkt->stream_index < (s32) ctx->demuxer->nb_streams);

	if (pkt->stream_index >= (s32) ctx->nb_streams) {
		GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] More streams (%d) than initialy declared (%d), buggy source demux or not supported, ignoring packet in stream %d\n", ctx->fname, ctx->demuxer->nb_streams, ctx->nb_streams, pkt->stream_index+1 ));
		FF_FREE_PCK(pkt);
		return GF_OK;
	}

	if (pkt->pts == AV_NOPTS_VALUE) {
		if (pkt->dts == AV_NOPTS_VALUE) {
			GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] No PTS for packet on stream %d\n", ctx->fname, pkt->stream_index ));
		} else {
			pkt->pts = pkt->dts;
		}
	}

	pctx = &ctx->pids_ctx[pkt->stream_index];
	if (! pctx->pid ) {
		GF_LOG(GF_LOG_DEBUG, ctx->log_class, ("[%s] No PID defined for given stream %d\n", ctx->fname, pkt->stream_index ));
		FF_FREE_PCK(pkt);
		return GF_OK;
	}
    if (ctx->stop_seen && ! gf_filter_pid_is_playing( pctx->pid ) ) {
		FF_FREE_PCK(pkt);
        return GF_OK;
    }
	if (ctx->raw_data && (ctx->probe_frames<ctx->probes) ) {
		if (pkt->stream_index==ctx->audio_idx) {
			FF_FREE_PCK(pkt);
			return GF_OK;
		}

		ctx->probe_times[ctx->probe_frames] = ctx->sclock ? sample_time : pkt->pts;
		ctx->probe_frames++;
		if (ctx->probe_frames==ctx->probes) {
			u32 best_diff=0, max_stat=0;
			for (i=0; i<ctx->probes; i++) {
				if (i) {
					u32 j, nb_stats=0;
					u32 diff = (u32) (ctx->probe_times[i]-ctx->probe_times[i-1]);
					for (j=1; j<ctx->probes; j++) {
						s32 sdiff = (s32) (ctx->probe_times[j]-ctx->probe_times[j-1]);
						sdiff -= (s32) diff;
						if (sdiff<0) sdiff = -sdiff;
						if (sdiff<2000) nb_stats++;
					}
					if (max_stat<nb_stats) {
						max_stat = nb_stats;
						best_diff = diff;
					}
				}
			}
			GF_LOG(GF_LOG_INFO, ctx->log_class, ("[%s] Video probing done, frame diff is %d us (for %d frames out of %d)\n", ctx->fname, best_diff, max_stat, ctx->probes));
		} else {
			FF_FREE_PCK(pkt);
			return GF_OK;
		}
	}

	if (pkt->side_data_elems) {
		for (i=0; i < (u32) pkt->side_data_elems; i++) {
			AVPacketSideData *sd = &pkt->side_data[i];
			if (sd->type == AV_PKT_DATA_NEW_EXTRADATA) {
				if (sd->data) {
					u32 cid = 0;
					const GF_PropertyValue *p = gf_filter_pid_get_property(pctx->pid, GF_PROP_PID_CODECID);
					if (p) cid = p->value.uint;
					ffdmx_set_decoder_config(pctx->pid, sd->data, (u32) sd->size, cid);
				}
			}
			else if (sd->type == AV_PKT_DATA_PARAM_CHANGE) {
				GF_BitStream *bs = gf_bs_new(sd->data, sd->size, GF_BITSTREAM_READ);

				u32 flags = gf_bs_read_u32_le(bs);
				if (flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_COUNT) {
					u32 new_ch = gf_bs_read_u32_le(bs);
					gf_filter_pid_set_property(pctx->pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(new_ch) );
				}
				if (flags & AV_SIDE_DATA_PARAM_CHANGE_CHANNEL_LAYOUT) {
					u64 new_lay = gf_bs_read_u64_le(bs);
					new_lay = ffmpeg_channel_layout_to_gpac(new_lay);
					gf_filter_pid_set_property(pctx->pid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_LONGUINT(new_lay) );
				}
				if (flags & AV_SIDE_DATA_PARAM_CHANGE_SAMPLE_RATE) {
					u32 new_sr = gf_bs_read_u32_le(bs);
					gf_filter_pid_set_property(pctx->pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(new_sr) );
				}
				if (flags & AV_SIDE_DATA_PARAM_CHANGE_DIMENSIONS) {
					u32 new_w = gf_bs_read_u32_le(bs);
					u32 new_h = gf_bs_read_u32_le(bs);
					gf_filter_pid_set_property(pctx->pid, GF_PROP_PID_WIDTH, &PROP_UINT(new_w) );
					gf_filter_pid_set_property(pctx->pid, GF_PROP_PID_HEIGHT, &PROP_UINT(new_h) );
				}
				gf_bs_del(bs);
			}
			else if ((sd->type == AV_PKT_DATA_WEBVTT_IDENTIFIER) || (sd->type == AV_PKT_DATA_WEBVTT_SETTINGS)
				|| (sd->type == AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL)
			) {
				check_webvtt = pctx->mkv_webvtt;
			}
			//todo, map the rest ?
			else {
				ffdmx_parse_side_data(sd, pctx->pid);
			}
		}
	}

	if (ctx->raw_data) {
		if (pkt->stream_index==ctx->audio_idx) copy = ctx->copy_audio;
		else copy = ctx->copy_video;
	}

	if (ctx->raw_data && !copy) {
		//we don't use shared memory on demuxers since they are usually the ones performing all the buffering
		pck_dst = gf_filter_pck_new_shared(pctx->pid, pkt->data, pkt->size, ffdmx_shared_pck_release);
		if (!pck_dst) return GF_OUT_OF_MEM;
		ctx->raw_pck_out = GF_TRUE;
	} else {
		//we don't use shared memory on demuxers since they are usually the ones performing all the buffering
		pck_dst = gf_filter_pck_new_alloc(pctx->pid, pkt->size, &data_dst);
		if (!pck_dst) return GF_OUT_OF_MEM;
		assert(pck_dst);
		memcpy(data_dst, pkt->data, pkt->size);
	}

	if (ctx->raw_data && ctx->sclock) {
		u64 ts;
		if (!ctx->first_sample_clock) {
			ctx->last_frame_ts = ctx->first_sample_clock = sample_time;
		}
		ts = sample_time - ctx->first_sample_clock;
		gf_filter_pck_set_cts(pck_dst, ts );
		ctx->last_frame_ts = ts;
	} else if (pkt->pts != AV_NOPTS_VALUE) {
		AVStream *stream = ctx->demuxer->streams[pkt->stream_index];
		u64 ts;

		//initial delay setup - we only dispatch dts or cts >=0
		if (!pctx->ts_offset) {
			//if first dts is <0, offset timeline and set offset
			if ((pkt->dts != AV_NOPTS_VALUE) && (pkt->dts<0) && pctx->ts_offset) {
				pctx->ts_offset = -pkt->dts + 1;
				gf_filter_pid_set_property(pctx->pid, GF_PROP_PID_DELAY, &PROP_LONGSINT( pkt->dts) );
			}
			//otherwise reset any potential delay set previously
			else {
				pctx->ts_offset = 1;
				gf_filter_pid_set_property(pctx->pid, GF_PROP_PID_DELAY, NULL);
			}
		}

		ts = (pkt->pts + pctx->ts_offset-1) * stream->time_base.num;
		gf_filter_pck_set_cts(pck_dst, ts );

		if (pkt->dts != AV_NOPTS_VALUE) {
			ts = (pkt->dts + pctx->ts_offset-1) * stream->time_base.num;
			gf_filter_pck_set_dts(pck_dst, ts);
		}

		if (pkt->duration)
			gf_filter_pck_set_duration(pck_dst, (u32) pkt->duration);
	}

	//fixme: try to identify SAP type 2 and more
	if (pkt->flags & AV_PKT_FLAG_KEY)
		gf_filter_pck_set_sap(pck_dst, GF_FILTER_SAP_1);

	if (pkt->flags & AV_PKT_FLAG_CORRUPT)
		gf_filter_pck_set_corrupted(pck_dst, GF_TRUE);

	if (ctx->raw_data) {
		u64 ntp = gf_net_get_ntp_ts();
		gf_filter_pck_set_property(pck_dst, GF_PROP_PCK_SENDER_NTP, &PROP_LONGUINT(ntp) );
	}
	if (check_webvtt) {
		for (i=0; i < (u32) pkt->side_data_elems; i++) {
			AVPacketSideData *sd = &pkt->side_data[i];
			if (!sd->data) continue;
			if ((sd->type == AV_PKT_DATA_WEBVTT_IDENTIFIER) || (sd->type == AV_PKT_DATA_WEBVTT_SETTINGS)) {
				u8 *d = gf_malloc(sd->size+1);
				if (d) {
					memcpy(d, sd->data, sd->size);
					d[sd->size]=0;
					if (sd->type == AV_PKT_DATA_WEBVTT_SETTINGS)
						gf_filter_pck_set_property_str(pck_dst, "vtt_settings", &PROP_STRING_NO_COPY(d) );
					else
						gf_filter_pck_set_property_str(pck_dst, "vtt_cueid", &PROP_STRING_NO_COPY(d) );
				}
			} else if ((sd->type == AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL) && (sd->size>8)) {
				u8 *d = gf_malloc(sd->size-7);
				if (d) {
					memcpy(d, sd->data+8, sd->size-8);
					d[sd->size-8]=0;
					gf_filter_pck_set_property_str(pck_dst, "vtt_pre", &PROP_STRING_NO_COPY(d) );
				}
			}
		}
	}

	e = gf_filter_pck_send(pck_dst);
    ctx->nb_pck_sent++;
	ctx->nb_stop_pending=0;
	if (!ctx->raw_pck_out) {
		FF_FREE_PCK(pkt);
	}
	return e;
}


static GF_Err ffdmx_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	GF_FFDemuxCtx *ctx = gf_filter_get_udta(filter);
	return ffmpeg_update_arg(ctx->fname, ctx->demuxer, &ctx->options, arg_name, arg_val);
}

#include <gpac/mpeg4_odf.h>
#include <gpac/avparse.h>
/* probe DSI syntax - if valid according to our own representation, we're good to go otherwise we'll need a reframer

This is needed because libavformat does not always expose the same dsi syntax depending on mux types (!)
*/
static u32 ffdmx_valid_should_reframe(u32 gpac_codec_id, u8 *dsi, u32 dsi_size)
{
	GF_AC3Config ac3;
	GF_M4ADecSpecInfo aaccfg;
	GF_AVCConfig *avcc;
	GF_HEVCConfig *hvcc;
	GF_VVCConfig *vvcc;
	GF_AV1Config *av1c;
	GF_VPConfig *vpxc;

	if (!dsi_size) dsi = NULL;

	switch (gpac_codec_id) {
	//force reframer for the following formats if no DSI is found
	case GF_CODECID_AC3:
	case GF_CODECID_EAC3:
		if (dsi && (gf_odf_ac3_config_parse(dsi, dsi_size, (gpac_codec_id==GF_CODECID_EAC3) ? GF_TRUE : GF_FALSE, &ac3) == GF_OK))
			return 0;
		return 1;

	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
		if (dsi && (gf_m4a_get_config(dsi, dsi_size, &aaccfg) == GF_OK))
			return 0;
		return 1;

	case GF_CODECID_FLAC:
		if (!dsi) return 1;
		break;
	case GF_CODECID_AVC:
		avcc = dsi ? gf_odf_avc_cfg_read(dsi, dsi_size) : NULL;
		if (avcc) {
			gf_odf_avc_cfg_del(avcc);
			return 0;
		}
		return 1;
	case GF_CODECID_HEVC:
		hvcc = dsi ? gf_odf_hevc_cfg_read(dsi, dsi_size, GF_FALSE) : NULL;
		if (hvcc) {
			gf_odf_hevc_cfg_del(hvcc);
			return 0;
		}
		return 1;
	case GF_CODECID_VVC:
		vvcc = dsi ? gf_odf_vvc_cfg_read(dsi, dsi_size) : NULL;
		if (vvcc) {
			gf_odf_vvc_cfg_del(vvcc);
			return 0;
		}
		return 1;
	case GF_CODECID_AV1:
		av1c = dsi ? gf_odf_av1_cfg_read(dsi, dsi_size) : NULL;
		if (av1c) {
			gf_odf_av1_cfg_del(av1c);
			return 0;
		}
		return 1;
	case GF_CODECID_VP8:
	case GF_CODECID_VP9:
		vpxc = dsi ? gf_odf_vp_cfg_read(dsi, dsi_size) : NULL;
		if (vpxc) {
			gf_odf_vp_cfg_del(vpxc);
			return 0;
		}
		return 1;
	//force reframer for the following formats regardless of DSI and drop it
	case GF_CODECID_MPEG1:
	case GF_CODECID_MPEG2_422:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_SPATIAL:
	case GF_CODECID_MPEG4_PART2:
		return 1;

	//SRT or other subs: sample data is the raw text but timing is at packet level, force a reframer to parse styles and other
	//keep dsi if any (for webvtt in mkv)
	case GF_CODECID_SUBS_TEXT:
	case GF_CODECID_WEBVTT:
	case GF_CODECID_SUBS_SSA:
		return 2;

	//all other codecs are reframed
	default:
		break;
	}
	return 0;
}

GF_Err ffdmx_init_common(GF_Filter *filter, GF_FFDemuxCtx *ctx, u32 grab_type)
{
	u32 i;
	u32 nb_a, nb_v, nb_t, clock_id;
	char szName[50];

#if (LIBAVCODEC_VERSION_MAJOR >= 59)
	ctx->pkt = av_packet_alloc();
#endif

	ctx->pids_ctx = gf_malloc(sizeof(PidCtx)*ctx->demuxer->nb_streams);
	memset(ctx->pids_ctx, 0, sizeof(PidCtx)*ctx->demuxer->nb_streams);
	ctx->nb_streams = ctx->demuxer->nb_streams;

	clock_id = 0;
	for (i = 0; i < ctx->demuxer->nb_streams; i++) {
		AVStream *stream = ctx->demuxer->streams[i];
#if (LIBAVFORMAT_VERSION_MAJOR < 59)
		AVCodecContext *codec = stream->codec;
		u32 codec_type = codec->codec_type;
#else
		u32 codec_type = stream->codecpar->codec_type;
#endif
		switch(codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			if (!clock_id) clock_id = stream->id ? stream->id : i+1;
			break;
		case AVMEDIA_TYPE_VIDEO:
			clock_id = stream->id ? stream->id : i+1;
			break;
		}
	}

	nb_a = nb_v = nb_t = 0;
	for (i = 0; i < ctx->demuxer->nb_streams; i++) {
		GF_FilterPid *pid=NULL;
		u32 j;
		u32 force_reframer = 0;
		Bool expose_ffdec=GF_FALSE;
		u32 gpac_codec_id;
		AVStream *stream = ctx->demuxer->streams[i];
#if (LIBAVFORMAT_VERSION_MAJOR < 59)
		AVCodecContext *codec = stream->codec;
		u32 codec_type = codec->codec_type;
		u32 codec_id = codec->codec_id;
		const uint8_t *exdata = codec->extradata;
		u32 exdata_size = codec->extradata_size;
		u32 codec_sample_rate = codec->sample_rate;
		u32 codec_frame_size = codec->frame_size;
		u32 codec_channels = codec->channels;
		u32 codec_width = codec->width;
		u32 codec_height = codec->height;
		u32 codec_field_order = codec->field_order;
		u32 codec_tag = codec->codec_tag;
		u32 codec_pixfmt = codec->pix_fmt;
		u32 codec_blockalign = 0;
		AVRational codec_framerate = {0, 0};
#if LIBAVCODEC_VERSION_MAJOR >= 58
		codec_framerate = codec->framerate;
#endif
		s32 codec_sample_fmt = codec->sample_fmt;
		u32 codec_bitrate = (u32) codec->bit_rate;

#else
		u32 codec_type = stream->codecpar->codec_type;
		u32 codec_id = stream->codecpar->codec_id;
		const uint8_t *exdata = stream->codecpar->extradata;
		u32 exdata_size = stream->codecpar->extradata_size;
		u32 codec_sample_rate = stream->codecpar->sample_rate;
		u32 codec_frame_size = stream->codecpar->frame_size;
		u32 codec_channels = stream->codecpar->channels;
		u32 codec_width = stream->codecpar->width;
		u32 codec_height = stream->codecpar->height;
		u32 codec_field_order = stream->codecpar->field_order;
		u32 codec_tag = stream->codecpar->codec_tag;
		u32 codec_pixfmt = (codec_type==AVMEDIA_TYPE_VIDEO) ? stream->codecpar->format : 0;
		s32 codec_sample_fmt = (codec_type==AVMEDIA_TYPE_AUDIO) ? stream->codecpar->format : 0;
		u32 codec_bitrate = (u32) stream->codecpar->bit_rate;
		u32 codec_blockalign = (u32) stream->codecpar->block_align;
		AVRational codec_framerate = stream->r_frame_rate;
		if (!stream->r_frame_rate.num || !stream->r_frame_rate.den)
			codec_framerate = stream->avg_frame_rate;
#endif

		//if fps was detected by ffavin, use it (r_frame_rate is unreliable, just a guess)
		if (ctx->fps_forced.num) {
			codec_framerate.num = ctx->fps_forced.num;
			codec_framerate.den = ctx->fps_forced.den;
		}
		switch(codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			pid = gf_filter_pid_new(filter);
			if (!pid) return GF_OUT_OF_MEM;
			gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO) );
			nb_a++;
			sprintf(szName, "audio%d", nb_a);
			if (ctx->audio_idx<0)
				ctx->audio_idx = i;
			break;

		case AVMEDIA_TYPE_VIDEO:
			pid = gf_filter_pid_new(filter);
			if (!pid) return GF_OUT_OF_MEM;
			gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL) );
			nb_v++;
			sprintf(szName, "video%d", nb_v);
			if (ctx->video_idx<0)
				ctx->video_idx = i;
			break;
		case AVMEDIA_TYPE_SUBTITLE:
			pid = gf_filter_pid_new(filter);
			if (!pid) return GF_OUT_OF_MEM;
			gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
			nb_t++;
			sprintf(szName, "text%d", nb_t);
			break;
		default:
			sprintf(szName, "ffdmx%d", i+1);
			break;
		}
		if (!pid) continue;
		ctx->pids_ctx[i].pid = pid;
		ctx->pids_ctx[i].ts_offset = 0;
		gf_filter_pid_set_udta(pid, stream);

		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT( (stream->id ? stream->id : i+1)) );
		gf_filter_pid_set_name(pid, szName);

		if (ctx->raw_data && ctx->sclock) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000000) );
		} else {
			gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(stream->time_base.den) );
		}
		if (clock_id)
			gf_filter_pid_set_property(pid, GF_PROP_PID_CLOCK_ID, &PROP_UINT(clock_id) );

		if (!ctx->raw_data) {
			if (stream->duration>=0)
				gf_filter_pid_set_property(pid, GF_PROP_PID_DURATION, &PROP_FRAC64_INT(stream->duration, stream->time_base.den) );
			else if (ctx->demuxer->duration>=0)
				gf_filter_pid_set_property(pid, GF_PROP_PID_DURATION, &PROP_FRAC64_INT(ctx->demuxer->duration, AV_TIME_BASE) );
		}

		if (stream->sample_aspect_ratio.num && stream->sample_aspect_ratio.den)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAR, &PROP_FRAC_INT( stream->sample_aspect_ratio.num, stream->sample_aspect_ratio.den ) );

		ffmpeg_tags_to_gpac(stream->metadata, pid);

		gpac_codec_id = ffmpeg_codecid_to_gpac(codec_id);
		if (!gpac_codec_id) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_FFMPEG) );
			expose_ffdec = GF_TRUE;
		} else {
			gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(gpac_codec_id) );
		}

		if (grab_type)
			gf_filter_pid_set_property(pid, GF_PROP_PID_RAWGRAB, &PROP_UINT(grab_type) );
		else if (ctx->demuxer->iformat) {
			if ((ctx->demuxer->iformat->flags & AVFMT_SEEK_TO_PTS) || ctx->demuxer->iformat->read_seek)
				gf_filter_pid_set_property(pid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_FASTFORWARD ) );
		}


		u32 lt = gf_log_get_tool_level(GF_LOG_CODING);
		gf_log_set_tool_level(GF_LOG_CODING, GF_LOG_QUIET);
		force_reframer = ffdmx_valid_should_reframe(gpac_codec_id, (u8 *) exdata, exdata_size);
		gf_log_set_tool_level(GF_LOG_CODING, lt);

		if (expose_ffdec) {
			const char *cname = avcodec_get_name(codec_id);
#if (LIBAVFORMAT_VERSION_MAJOR < 59)
			gf_filter_pid_set_property(pid, GF_PROP_PID_FFMPEG_CODEC_ID, &PROP_POINTER( (void*)codec ) );
#else
			gf_filter_pid_set_property(pid, GF_PROP_PID_FFMPEG_CODEC_ID, &PROP_UINT( codec_id ) );
			if (exdata) {
				//expose as const data
				gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_CONST_DATA( (char *)exdata, exdata_size) );
			}
#endif

			if (cname)
				gf_filter_pid_set_property_str(pid, "ffmpeg:codec", &PROP_STRING(cname ) );
		} else if (exdata_size) {

			//avc/hevc read by ffmpeg is still in annex B format
			if (ctx->demuxer->iformat) {
				if (!strcmp(ctx->demuxer->iformat->name, "h264")
					|| !strcmp(ctx->demuxer->iformat->name, "hevc")
					|| !strcmp(ctx->demuxer->iformat->name, "vvc")
				) {
					force_reframer = 1;
				}
			}

			//set extra data if desired
			if (force_reframer!=1) {
				ffdmx_set_decoder_config(pid, exdata, exdata_size, gpac_codec_id);
			}
		}

		if (force_reframer) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
		}


		if (codec_sample_rate)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT( codec_sample_rate ) );
		if (codec_frame_size)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLES_PER_FRAME, &PROP_UINT( codec_frame_size ) );
		if (codec_channels)
			gf_filter_pid_set_property(pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT( codec_channels ) );

		if (codec_width)
			gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT( codec_width ) );
		if (codec_height)
			gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT( codec_height ) );

		if (codec_width && codec_height) {
			if (codec_framerate.num && codec_framerate.den)
				gf_filter_pid_set_property(pid, GF_PROP_PID_FPS, &PROP_FRAC_INT( codec_framerate.num, codec_framerate.den ) );
			else {
				GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] Unknown frame rate, will use 25 fps - use `:#FPS=VAL` to force frame rate signaling\n", ctx->fname));
				gf_filter_pid_set_property(pid, GF_PROP_PID_FPS, &PROP_FRAC_INT( 25, 1 ) );
			}
		}
		
		if (codec_field_order>AV_FIELD_PROGRESSIVE)
			gf_filter_pid_set_property(pid, GF_PROP_PID_INTERLACED, &PROP_BOOL(GF_TRUE) );

		if ((codec_type==AVMEDIA_TYPE_VIDEO)
			&& (codec_pixfmt || ((codec_id==AV_CODEC_ID_RAWVIDEO) && codec_tag))
		) {
			Bool is_full_range = GF_FALSE;
			u32 pfmt = 0;

			if (codec_pixfmt) {
				pfmt = ffmpeg_pixfmt_to_gpac(codec_pixfmt, GF_FALSE);
				is_full_range = ffmpeg_pixfmt_is_fullrange(codec_pixfmt);
			} else if (codec_tag) {
				pfmt = ffmpeg_pixfmt_from_codec_tag(codec_tag, &is_full_range);
			}

			if (!pfmt) {
				GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] Unsupported pixel format %d\n", ctx->fname, codec_pixfmt));
			} else {
				gf_filter_pid_set_property(pid, GF_PROP_PID_PIXFMT, &PROP_UINT( pfmt) );
				if (is_full_range)
					gf_filter_pid_set_property(pid, GF_PROP_PID_COLR_RANGE, &PROP_BOOL( GF_TRUE ) );
			}
		}

		if (codec_type==AVMEDIA_TYPE_SUBTITLE) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_SUBTYPE, &PROP_UINT(GF_4CC('s','b','t','l')));
		}

		ctx->pids_ctx[i].mkv_webvtt = GF_FALSE;
		if ((gpac_codec_id==GF_CODECID_WEBVTT) && strstr(ctx->demuxer->iformat->name, "matroska"))
			ctx->pids_ctx[i].mkv_webvtt = GF_TRUE;

		if (codec_sample_fmt>0) {
			u32 sfmt = 0;
			switch (codec_sample_fmt) {
			case AV_SAMPLE_FMT_U8: sfmt = GF_AUDIO_FMT_U8; break;
			case AV_SAMPLE_FMT_S16: sfmt = GF_AUDIO_FMT_S16; break;
			case AV_SAMPLE_FMT_S32: sfmt = GF_AUDIO_FMT_S32; break;
			case AV_SAMPLE_FMT_FLT: sfmt = GF_AUDIO_FMT_FLT; break;
			case AV_SAMPLE_FMT_DBL: sfmt = GF_AUDIO_FMT_DBL; break;
			case AV_SAMPLE_FMT_U8P: sfmt = GF_AUDIO_FMT_U8P; break;
			case AV_SAMPLE_FMT_S16P: sfmt = GF_AUDIO_FMT_S16P; break;
			case AV_SAMPLE_FMT_S32P: sfmt = GF_AUDIO_FMT_S32P; break;
			case AV_SAMPLE_FMT_FLTP: sfmt = GF_AUDIO_FMT_FLTP; break;
			case AV_SAMPLE_FMT_DBLP: sfmt = GF_AUDIO_FMT_DBLP; break;
			default:
				GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] Unsupported sample format %d\n", ctx->fname, codec_sample_fmt));
			}
			gf_filter_pid_set_property(pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT( sfmt) );
		}

		if (codec_bitrate)
			gf_filter_pid_set_property(pid, GF_PROP_PID_BITRATE, &PROP_UINT( (u32) codec_bitrate ) );

		gf_filter_pid_set_property(pid, GF_PROP_PID_URL, &PROP_STRING( AVFMT_URL(ctx->demuxer) ));

		if (gf_file_exists(ctx->src)) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_FILE_CACHED, &PROP_BOOL(GF_TRUE));
		}

		if (codec_blockalign)
			gf_filter_pid_set_property_str(pid, "ffdmx:blockalign", &PROP_UINT(codec_blockalign));


		for (j=0; j<(u32) stream->nb_side_data; j++) {
			ffdmx_parse_side_data(&stream->side_data[i], pid);
		}
	}

	if (!nb_a && !nb_v && !nb_t)
		return GF_NOT_SUPPORTED;

	return GF_OK;
}

static int ffavio_read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	GF_FFDemuxCtx *ctx = (GF_FFDemuxCtx *)opaque;
	int res = (int) gf_fread(buf, buf_size, ctx->gfio);
	if (!res && gf_feof(ctx->gfio)) {
		return AVERROR_EOF;
	}
	return res;
}

static int64_t ffavio_seek(void *opaque, int64_t offset, int whence)
{
	GF_FFDemuxCtx *ctx = (GF_FFDemuxCtx *)opaque;
	if (whence==AVSEEK_SIZE) {
		u64 pos = gf_ftell(ctx->gfio);
		u64 size = gf_fsize(ctx->gfio);
		gf_fseek(ctx->gfio, pos, SEEK_SET);
		return size;
	}
	return (int64_t) gf_fseek(ctx->gfio, offset, whence);
}

static GF_Err ffdmx_initialize(GF_Filter *filter)
{
	GF_FFDemuxCtx *ctx = gf_filter_get_udta(filter);
	GF_Err e;
	s32 res;
	char *ext;
	const char *url;
	const AVInputFormat *av_in = NULL;
	ctx->fname = "FFDmx";
	ctx->log_class = GF_LOG_CONTAINER;

	ffmpeg_setup_logs(ctx->log_class);

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		ffdmx_update_arg(filter, "foo", &PROP_STRING_NO_COPY("bar"));
		ffmpeg_pixfmt_from_codec_tag(0, NULL);
	}
#endif
	if (!ctx->src) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Missing file name, cannot open\n", ctx->fname));
		return GF_SERVICE_ERROR;
	}
	/*some extensions not supported by ffmpeg, overload input format*/
	ext = strrchr(ctx->src, '.');
	if (ext) {
		if (!stricmp(ext+1, "cmp")) av_in = av_find_input_format("m4v");
	}

	GF_LOG(GF_LOG_DEBUG, ctx->log_class, ("[%s] opening file %s - av_in %08x\n", ctx->fname, ctx->src, av_in));

	ctx->demuxer = avformat_alloc_context();
	ffmpeg_set_mx_dmx_flags(ctx->options, ctx->demuxer);

	url = ctx->src;
	if (!strncmp(ctx->src, "gfio://", 7)) {
		ctx->gfio = gf_fopen(ctx->src, "rb");
		if (!ctx->gfio) {
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Failed to open %s\n", ctx->fname, ctx->src));
			return GF_URL_ERROR;
		}
		ctx->avio_ctx_buffer = av_malloc(ctx->block_size);
		if (!ctx->avio_ctx_buffer) {
			return GF_OUT_OF_MEM;
		}
		ctx->avio_ctx = avio_alloc_context(ctx->avio_ctx_buffer, ctx->block_size,
									  0, ctx, &ffavio_read_packet, NULL, &ffavio_seek);

		if (!ctx->avio_ctx) {
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Failed to create AVIO context for %s\n", ctx->fname, ctx->src));
			return GF_OUT_OF_MEM;
		}
		ctx->demuxer->pb = ctx->avio_ctx;
		url = gf_fileio_translate_url(ctx->src);
	}

	AVDictionary *options = NULL;
	av_dict_copy(&options, ctx->options, 0);

	res = avformat_open_input(&ctx->demuxer, url, FF_IFMT_CAST av_in, &options);

	switch (res) {
	case 0:
		e = GF_OK;
		break;
	case AVERROR_INVALIDDATA:
		e = GF_NON_COMPLIANT_BITSTREAM;
		break;
	case AVERROR_DEMUXER_NOT_FOUND:
	case -ENOENT:
		e = GF_URL_ERROR;
		break;
	case AVERROR_EOF:
		e = GF_EOS;
		break;
	default:
		e = GF_NOT_SUPPORTED;
		break;
	}
	if (e) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Fail to open %s - error %s\n", ctx->fname, ctx->src, av_err2str(res) ));
		avformat_close_input(&ctx->demuxer);
		avformat_free_context(ctx->demuxer);
		if (options) av_dict_free(&options);
		return e;
	}

	res = avformat_find_stream_info(ctx->demuxer, ctx->options ? &ctx->options : NULL);
	if (res <0) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] cannot locate streams - error %s\n", ctx->fname, av_err2str(res)));
		e = GF_NOT_SUPPORTED;
		avformat_close_input(&ctx->demuxer);
		avformat_free_context(ctx->demuxer);
		if (options) av_dict_free(&options);
		return e;
	}
	GF_LOG(GF_LOG_DEBUG, ctx->log_class, ("[%s] file %s opened - %d streams\n", ctx->fname, ctx->src, ctx->demuxer->nb_streams));

	ffmpeg_report_options(filter, options, ctx->options);
	return ffdmx_init_common(filter, ctx, 0);
}


static Bool ffdmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_FFDemuxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (evt->play.initial_broadcast_play==2)
			return GF_TRUE;

		ctx->nb_playing++;
		if (ctx->nb_playing>1) {
			Bool skip_com = GF_TRUE;
			//PLAY/STOP may arrive at different times depending on the length of filter chains on each PID
			//we stack number of STOP received and trigger seek when we have the same amount of play
			if (ctx->nb_stop_pending==ctx->nb_playing) {
				skip_com = GF_FALSE;
				ctx->last_play_start_range = 0;
			}
			if (skip_com) {
				return GF_TRUE;
			}
			ctx->nb_playing--;
		}

		//change in play range
		if (!ctx->raw_data && (ctx->last_play_start_range != evt->play.start_range)) {
			u32 i;
			int res = av_seek_frame(ctx->demuxer, -1, (s64) (AV_TIME_BASE*evt->play.start_range), AVSEEK_FLAG_BACKWARD);
			if (res<0) {
				GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] Fail to seek %s to %g - error %s\n", ctx->fname, ctx->src, evt->play.start_range, av_err2str(res) ));
			}
			//reset initial delay compute
			for (i=0; i<ctx->demuxer->nb_streams; i++) {
				ctx->pids_ctx[i].ts_offset = 0;
			}
			ctx->last_play_start_range = evt->play.start_range;
		}
		ctx->stop_seen = GF_FALSE;
		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		if (evt->play.initial_broadcast_play==2)
			return GF_TRUE;
		ctx->nb_stop_pending++;
		if (ctx->nb_playing) {
			ctx->nb_playing--;
			ctx->stop_seen = GF_TRUE;
		}
		//cancel event
		return GF_TRUE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

static GF_FilterProbeScore ffdmx_probe_url(const char *url, const char *mime)
{
	if (!strncmp(url, "video://", 8)) return GF_FPROBE_NOT_SUPPORTED;
	if (!strncmp(url, "audio://", 8)) return GF_FPROBE_NOT_SUPPORTED;
	if (!strncmp(url, "av://", 5)) return GF_FPROBE_NOT_SUPPORTED;
	if (!strncmp(url, "pipe://", 7)) return GF_FPROBE_NOT_SUPPORTED;

	const char *ext = gf_file_ext_start(url);
	if (ext) {
		const AVInputFormat *f = av_find_input_format(ext+1);
		if (!f)
			return GF_FPROBE_MAYBE_NOT_SUPPORTED;
	}
	return GF_FPROBE_MAYBE_SUPPORTED;
}


static const char *ffdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	int ffscore;
	const AVInputFormat *probe_fmt;
	AVProbeData pb;

#if (LIBAVFORMAT_VERSION_MAJOR < 59)
	av_register_all();
#endif

	memset(&pb, 0, sizeof(AVProbeData));
	//not setting this crashes some probers in ffmpeg
	pb.filename = "";
	if (size <= AVPROBE_PADDING_SIZE) {
		pb.buf = gf_malloc(sizeof(char) * (size+AVPROBE_PADDING_SIZE) );
		memcpy(pb.buf, data, sizeof(char)*size);
		memset(pb.buf+size, 0, sizeof(char)*AVPROBE_PADDING_SIZE);
		pb.buf_size = size;
		probe_fmt = av_probe_input_format3(&pb, GF_TRUE, &ffscore);
		if (ffscore<=AVPROBE_SCORE_RETRY/2) probe_fmt=NULL;
		if (!probe_fmt) probe_fmt = av_probe_input_format3(&pb, GF_FALSE, &ffscore);
		if (ffscore<=AVPROBE_SCORE_RETRY/2) probe_fmt=NULL;
		gf_free(pb.buf);
	} else {
		pb.buf =  (char *) data;
		pb.buf_size = size - AVPROBE_PADDING_SIZE;
		probe_fmt = av_probe_input_format3(&pb, GF_TRUE, &ffscore);
		if (ffscore<=AVPROBE_SCORE_RETRY/2) probe_fmt=NULL;
		if (!probe_fmt) probe_fmt = av_probe_input_format3(&pb, GF_FALSE, &ffscore);
		if (ffscore<=AVPROBE_SCORE_RETRY/2) probe_fmt=NULL;
	}

	if (!probe_fmt) return NULL;
	if (probe_fmt->mime_type) {
		//TODO try to refine based on ffprobe score
		*score = GF_FPROBE_MAYBE_SUPPORTED;
		return probe_fmt->mime_type;
	}
	*score = GF_FPROBE_MAYBE_NOT_SUPPORTED;
	return "video/x-ffmpeg";
}

#define OFFS(_n)	#_n, offsetof(GF_FFDemuxCtx, _n)

static const GF_FilterCapability FFDmxCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
};


GF_FilterRegister FFDemuxRegister = {
	.name = "ffdmx",
	.version=LIBAVFORMAT_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG demultiplexer")
	GF_FS_SET_HELP("This filter demultiplexes an input file or open a source protocol using FFMPEG.\n"
	"See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more details.\n"
	"To list all supported demultiplexers for your GPAC build, use `gpac -h ffdmx:*`.\n"
	"This will list both supported input formats and protocols.\n"
	"Input protocols are listed with `Description: Input protocol`, and the subclass name identifies the protocol scheme.\n"
	"For example, if `ffdmx:rtmp` is listed as input protocol, this means `rtmp://` source URLs are supported.\n"
	)
	.private_size = sizeof(GF_FFDemuxCtx),
	SETCAPS(FFDmxCaps),
	.initialize = ffdmx_initialize,
	.finalize = ffdmx_finalize,
	.process = ffdmx_process,
	.update_arg = ffdmx_update_arg,
	.probe_url = ffdmx_probe_url,
	.probe_data = ffdmx_probe_data,
	.process_event = ffdmx_process_event,
	.flags = GF_FS_REG_META,
};


static const GF_FilterArgs FFDemuxArgs[] =
{
	{ OFFS(src), "URL of source content", GF_PROP_NAME, NULL, NULL, 0},
	{ "*", -1, "any possible options defined for AVFormatContext and sub-classes. See `gpac -hx ffdmx` and `gpac -hx ffdmx:*`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};


const GF_FilterRegister *ffdmx_register(GF_FilterSession *session)
{
	ffmpeg_build_register(session, &FFDemuxRegister, FFDemuxArgs, 2, FF_REG_TYPE_DEMUX);
	return &FFDemuxRegister;
}



static GF_Err ffavin_initialize(GF_Filter *filter)
{
	s32 res, i, dev_idx=-1;
	Bool has_a, has_v;
	char szPatchedName[1024];
	const char *dev_name=NULL;
	const char *default_fmt=NULL;
	const AVInputFormat *dev_fmt=NULL;
	GF_FFDemuxCtx *ctx = gf_filter_get_udta(filter);
	Bool wants_audio = GF_FALSE;
	Bool wants_video = GF_FALSE;
	ctx->fname = "FFAVIn";
	ctx->log_class = GF_LOG_MMIO;

	ffmpeg_setup_logs(ctx->log_class);

	avdevice_register_all();

	if (!ctx->src) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] No source URL specified, expecting video://, audio:/ or av://\n", ctx->fname));
		return GF_SERVICE_ERROR;
	}
	default_fmt = ctx->fmt;
	if (!default_fmt) {
#ifdef WIN32
		default_fmt = "dshow";
#elif defined(__DARWIN) || defined(__APPLE__)
		default_fmt = "avfoundation";
#elif defined(GPAC_CONFIG_ANDROID)
		default_fmt = "android_camera";
#else
		default_fmt = "video4linux2";
#endif
	}

	if (default_fmt) {
		dev_fmt = av_find_input_format(default_fmt);
		if (dev_fmt == NULL) {
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Cannot find the input format %s\n", ctx->fname, default_fmt));
		}
#if LIBAVCODEC_VERSION_MAJOR >= 58
		else if (dev_fmt->priv_class->category != AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT && dev_fmt->priv_class->category != AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT) {
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s]] %s is neither a video nor an audio input device\n", ctx->fname, default_fmt));
			dev_fmt = NULL;
		}
#else
		//not supported for old FFMPEG versions
#endif
	}
#if (LIBAVCODEC_VERSION_MAJOR >= 58) && (LIBAVCODEC_VERSION_MINOR>=20)
	if (!dev_fmt) {
		while (1) {
			dev_fmt = av_input_video_device_next(FF_IFMT_CAST dev_fmt);
			if (!dev_fmt) break;
			if (!dev_fmt || !dev_fmt->priv_class) continue;
			if ((dev_fmt->priv_class->category != AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT) && (dev_fmt->priv_class->category != AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT))
				continue;

			//listing devices is on its way of implementation in ffmpeg ... for now break at first provider
			break;
		}
	}
#endif
	if (!dev_fmt) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] No input format specified\n", ctx->fname, ctx->fmt));
		return GF_BAD_PARAM;
	}

	dev_name = ctx->dev;

	if (!strncmp(ctx->src, "video://", 8)) wants_video = GF_TRUE;
	else if (!strncmp(ctx->src, "audio://", 8)) wants_audio = GF_TRUE;
	else if (!strncmp(ctx->src, "av://", 5)) wants_video = wants_audio = GF_TRUE;

	if (sscanf(dev_name, "%d", &dev_idx)==1) {
		sprintf(szPatchedName, "%d", dev_idx);
		if (strcmp(szPatchedName, dev_name)) 
			dev_idx = -1;
	} else {
		dev_idx = -1;
	}

	szPatchedName[0]=0;

#if defined(__DARWIN) || defined(__APPLE__)
	if (!strncmp(dev_name, "screen", 6)) {
		strcpy(szPatchedName, "Capture screen ");
		strcat(szPatchedName, dev_name+6);
		dev_name = (char *) szPatchedName;
	}
#endif
	if (!strncmp(dev_fmt->priv_class->class_name, "V4L2", 4) && (dev_idx>=0) ) {
		if (wants_audio) {
			sprintf(szPatchedName, "/dev/video%d:hw:%d", dev_idx, dev_idx);
		} else {
			sprintf(szPatchedName, "/dev/video%d", dev_idx);
		}
		dev_name = (char *) szPatchedName;
	}

	else if (wants_video && wants_audio && (dev_idx>=0)) {
		sprintf(szPatchedName, "%d:%d", dev_idx, dev_idx);
		dev_name = (char *) szPatchedName;
	}
#if defined(__APPLE__) && !defined(GPAC_CONFIG_IOS)
	else if (!strncmp(dev_fmt->priv_class->class_name, "AVFoundation", 12) && wants_audio && !wants_video) {
		//for avfoundation if no video, we must use ":audio_dev_idx"
		if (ctx->dev[0] != ':') {
			strcpy(szPatchedName, ":");
			strcat(szPatchedName, ctx->dev);
			dev_name = (char *) szPatchedName;
		}
	}
#endif

	/* Open video */
	ctx->demuxer = avformat_alloc_context();
	ffmpeg_set_mx_dmx_flags(ctx->options, ctx->demuxer);

	AVDictionary *options = NULL;
	av_dict_copy(&options, ctx->options, 0);

	res = avformat_open_input(&ctx->demuxer, dev_name, FF_IFMT_CAST dev_fmt, &options);
	if ( (res < 0) && !stricmp(ctx->dev, "screen-capture-recorder") ) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Buggy screen capture input (open failed with code %d), retrying without specifying resolution\n", ctx->fname, res));
		av_dict_free(&options);
		options = NULL;
		av_dict_copy(&options, ctx->options, 0);
		av_dict_set(&options, "video_size", NULL, 0);
		res = avformat_open_input(&ctx->demuxer, ctx->dev, FF_IFMT_CAST dev_fmt, &options);
	}

	if (res < 0) {
		av_dict_free(&options);
		options = NULL;
		av_dict_copy(&options, ctx->options, 0);
		av_dict_set(&options, "framerate", "30", 0);
		ctx->fps_forced.num = 30;
		ctx->fps_forced.den = 1;
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Error %d opening input - retrying with 30 fps\n", ctx->fname, res));
		res = avformat_open_input(&ctx->demuxer, dev_name, FF_IFMT_CAST dev_fmt, &options);
		if (res < 0) {
			av_dict_free(&options);
			options = NULL;
			av_dict_copy(&options, ctx->options, 0);
			av_dict_set(&options, "framerate", "25", 0);
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Error %d opening input - retrying with 25 fps\n", ctx->fname, res));
			ctx->fps_forced.num = 25;
			res = avformat_open_input(&ctx->demuxer, dev_name, FF_IFMT_CAST dev_fmt, &options);

			if ((res<0) && options) {
				av_dict_free(&options);
				options = NULL;
				ctx->fps_forced.num = 0;
				GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Error %d opening input - retrying without options\n", ctx->fname, res));
				res = avformat_open_input(&ctx->demuxer, dev_name, FF_IFMT_CAST dev_fmt, NULL);
			}
		}
	} else {
		AVDictionaryEntry *key = NULL;
		while (1) {
			key = av_dict_get(ctx->options, "", key, AV_DICT_IGNORE_SUFFIX);
			if (!key) break;
			if (!strcmp(key->key, "framerate")) {
				char *fps = key->value;
				if (strchr(fps, '.')) {
					ctx->fps_forced.num = (u32) (atoi(fps)*1000);
					ctx->fps_forced.den = 1000;
				} else {
					ctx->fps_forced.num = atoi(fps);
					ctx->fps_forced.den = 1;
				}
			}
		}
	}

	if (res < 0) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Cannot open device %s:%s\n", ctx->fname, dev_fmt->priv_class->class_name, ctx->dev));
		if (options) av_dict_free(&options);
		return -1;
	}

	av_dump_format(ctx->demuxer, 0, ctx->dev, 0);
	ctx->raw_data = GF_TRUE;
	ctx->audio_idx = ctx->video_idx = -1;
	u32 grab_type = 1;
#if defined(GPAC_CONFIG_IOS) || defined(GPAC_CONFIG_ANDROID)
	if (!strcmp(ctx->dev, "1") || strstr(ctx->dev, "Front") || strstr(ctx->dev, "front") )
		grab_type = 2;
#endif

	res = avformat_find_stream_info(ctx->demuxer, ctx->options ? &ctx->options : NULL);

	ffmpeg_report_options(filter, options, ctx->options);

	if (res <0) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] cannot locate streams - error %s\n", ctx->fname,  av_err2str(res)));
		return GF_NOT_SUPPORTED;
	}

	//check we have the stream we want
	has_a = has_v = GF_FALSE;
	for (i = 0; (u32) i < ctx->demuxer->nb_streams; i++) {
#if (LIBAVFORMAT_VERSION_MAJOR < 59)
		u32 codec_type = ctx->demuxer->streams[i]->codec->codec_type;
#else
		u32 codec_type = ctx->demuxer->streams[i]->codecpar->codec_type;
#endif

		switch (codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			has_a = GF_TRUE;
			break;
		case AVMEDIA_TYPE_VIDEO:
			has_v = GF_TRUE;
			break;
		default:
			break;
		}
	}
	if (wants_audio && !has_a) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] No audio stream in input device\n", ctx->fname));
		return GF_NOT_SUPPORTED;
	}
	if (wants_video && !has_v) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] No video stream in input device\n", ctx->fname));
		return GF_NOT_SUPPORTED;
	}
	ctx->probe_frames = ctx->probes;
	if (has_v && ctx->probes) {
		ctx->probe_times = gf_malloc(sizeof(u64) * ctx->probes);
		memset(ctx->probe_times, 0, sizeof(u64) * ctx->probes);
		//we probe timestamps in either modes because timestamps of first frames are sometimes off
		ctx->probe_frames = 0;
	}

	GF_LOG(GF_LOG_INFO, ctx->log_class, ("[%s] device %s:%s opened - %d streams\n", ctx->fname, dev_fmt->priv_class->class_name, ctx->dev, ctx->demuxer->nb_streams));

	ctx->copy_audio = ctx->copy_video = GF_FALSE;
	switch (ctx->copy) {
	case COPY_NO:
		break;
	case COPY_A:
		ctx->copy_audio = GF_TRUE;
		break;
	case COPY_V:
		ctx->copy_video = GF_TRUE;
		break;
	default:
		ctx->copy_audio = ctx->copy_video = GF_TRUE;
		break;
	}
	if (wants_audio && wants_video) {
		if (!ctx->copy_audio || !ctx->copy_video) {
			GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] using muxed capture av:// without copy on %s, this might introduce packet losses due to blocking modes or delayed consumption of the frames. If experiencing problems, either set [-copy]() to `AV` or consider using two filters video:// and audio://\n", ctx->fname, (!ctx->copy_audio && !ctx->copy_video) ? "audio and video streams" : ctx->copy_video ? "audio stream" : "video stream"));
		}
	}
	return ffdmx_init_common(filter, ctx, grab_type);
}

static GF_FilterProbeScore ffavin_probe_url(const char *url, const char *mime)
{
	if (!strncmp(url, "video://", 8)) return GF_FPROBE_MAYBE_SUPPORTED;
	if (!strncmp(url, "audio://", 8)) return GF_FPROBE_MAYBE_SUPPORTED;
	if (!strncmp(url, "av://", 5)) return GF_FPROBE_MAYBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

static const GF_FilterCapability FFAVInCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL)
	//do not expose a specific codec ID (eg raw) as some grabbers might give us mjpeg
};

GF_FilterRegister FFAVInRegister = {
	.name = "ffavin",
	.version = LIBAVDEVICE_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG AV Capture")
	GF_FS_SET_HELP("Reads from audio/video capture devices using FFMPEG.\n"
	"See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more details.\n"
	"To list all supported grabbers for your GPAC build, use `gpac -h ffavin:*`.\n"
	"\n"
	"# Device identification\n"
	"Typical classes are `dshow` on windows, `avfoundation` on OSX, `video4linux2` or `x11grab` on linux\n"
	"\n"
	"Typical device name can be the webcam name:\n"
	"- `FaceTime HD Camera` on OSX, device name on windows, `/dev/video0` on linux\n"
	"- `screen-capture-recorder`, see http://screencapturer.sf.net/ on windows\n"
	"- `Capture screen 0` on OSX (0=first screen), or `screenN` for short\n"
	"- X display name (e.g. `:0.0`) on linux\n"
	"\n"
	"The general mapping from ffmpeg command line is:\n"
	"- ffmpeg `-f` maps to [-fmt]() option\n"
	"- ffmpeg `-i` maps to [-dev]() option\n"
	"\n"
	"EX ffmpeg -f libndi_newtek -i MY_NDI_TEST ...\n"
	"EX gpac -i av://:fmt=libndi_newtek:dev=MY_NDI_TEST ...\n"
	"\n"
	"You may need to escape the [-dev]() option if the format uses ':' as separator, as is the case for AVFoundation:\n"
	"EX gpac -i av://::dev=0:1 ...\n"
	)
	.private_size = sizeof(GF_FFDemuxCtx),
	SETCAPS(FFAVInCaps),
	.initialize = ffavin_initialize,
	.finalize = ffdmx_finalize,
	.process = ffdmx_process,
	.update_arg = ffdmx_update_arg,
	.probe_url = ffavin_probe_url,
	.process_event = ffdmx_process_event,
	.flags = GF_FS_REG_META,
};


static const GF_FilterArgs FFAVInArgs[] =
{
	{ OFFS(src), "url of device, `video://`, `audio://` or `av://`", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(fmt), "name of device class. If not set, defaults to first device class", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(dev), "name of device or index of device", GF_PROP_STRING, "0", NULL, 0},
	{ OFFS(copy), "set copy mode of raw frames\n"
		"- N: frames are only forwarded (shared memory, no copy)\n"
		"- A: audio frames are copied, video frames are forwarded\n"
		"- V: video frames are copied, audio frames are forwarded\n"
		"- AV: all frames are copied"
		"", GF_PROP_UINT, "A", "N|A|V|AV", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sclock), "use system clock (us) instead of device timestamp (for buggy devices)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(probes), "probe a given number of video frames before emitting (this usually helps with bad timing of the first frames)", GF_PROP_UINT, "10", "0-100", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(block_size), "block size used to read file when using avio context", GF_PROP_UINT, "4096", NULL, GF_FS_ARG_HINT_EXPERT},
	{ "*", -1, "any possible options defined for AVInputFormat and AVFormatContext (see `gpac -hx ffavin` and `gpac -hx ffavin:*`)", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};


//number of arguments defined above
const int FFAVIN_STATIC_ARGS = (sizeof (FFAVInArgs) / sizeof (GF_FilterArgs)) - 1;


#if (LIBAVCODEC_VERSION_MAJOR >= 58) && (LIBAVCODEC_VERSION_MINOR>=18) && !defined(GPAC_DISABLE_DOC)
#define FF_PROBE_DEVICES
#endif

#ifdef FF_PROBE_DEVICES
char *dev_desc = NULL;

static void ffavin_enum_devices(const char *dev_name, Bool is_audio)
{
	const AVInputFormat *fmt;
	AVFormatContext *ctx;

    if (!dev_name) return;
    fmt = av_find_input_format(dev_name);
    if (!fmt) return;

    if (!fmt || !fmt->priv_class || !AV_IS_INPUT_DEVICE(fmt->priv_class->category)) {
		return;
	}
    ctx = avformat_alloc_context();
    if (!ctx) return;
    ctx->iformat = (AVInputFormat *)fmt;
    if (ctx->iformat->priv_data_size > 0) {
        ctx->priv_data = av_mallocz(ctx->iformat->priv_data_size);
        if (!ctx->priv_data) {
			avformat_free_context(ctx);
            return;
        }
        if (ctx->iformat->priv_class) {
            *(const AVClass**)ctx->priv_data = ctx->iformat->priv_class;
            av_opt_set_defaults(ctx->priv_data);
        }
    } else {
        ctx->priv_data = NULL;
	}

	AVDeviceInfoList *dev_list = NULL;

    AVDictionary *tmp = NULL;
	av_dict_set(&tmp, "list_devices", "1", 0);
    av_opt_set_dict2(ctx, &tmp, AV_OPT_SEARCH_CHILDREN);

	int res = avdevice_list_devices(ctx, &dev_list);
	if (res<0) {
		//device doesn't implement avdevice_list_devices, try loading the context using "list_devices=1" option
		if (-res == ENOSYS) {
			AVDictionary *opts = NULL;
			av_dict_set(&opts, "list_devices", "1", 0);
			res = avformat_open_input(&ctx, "dummy", FF_IFMT_CAST fmt, &opts);
		}
	} else if (!res && dev_list->nb_devices) {
		if (!dev_desc) {
			gf_dynstrcat(&dev_desc, "# Detected devices\n", NULL);
		}
		gf_dynstrcat(&dev_desc, dev_name, NULL);
		gf_dynstrcat(&dev_desc, is_audio ? " audio" : " video", NULL);
		gf_dynstrcat(&dev_desc, " devices\n", NULL);
		for (int i=0; i<dev_list->nb_devices; i++) {
			char szFmt[20];
			sprintf(szFmt, "[%d] ", i);
			gf_dynstrcat(&dev_desc, dev_list->devices[i]->device_name, szFmt);
			gf_dynstrcat(&dev_desc, dev_list->devices[i]->device_description, ": ");
			gf_dynstrcat(&dev_desc, "\n", NULL);
		}
	}

	if (dev_list) avdevice_free_list_devices(&dev_list);
	avformat_free_context(ctx);
}

static void ffavin_log_none(void *avcl, int level, const char *fmt, va_list vl)
{
	if (level == AV_LOG_INFO) {
		char szLogBuf[2049];
		vsnprintf(szLogBuf, 2048, fmt, vl);
		szLogBuf[2048]=0;

		if (!dev_desc) {
			gf_dynstrcat(&dev_desc, "# Detected devices\n", NULL);
		}
		gf_dynstrcat(&dev_desc, szLogBuf, NULL);
	}
}
#endif

const GF_FilterRegister *ffavin_register(GF_FilterSession *session)
{
	ffmpeg_build_register(session, &FFAVInRegister, FFAVInArgs, FFAVIN_STATIC_ARGS, FF_REG_TYPE_DEV_IN);

	if (gf_opts_get_bool("temp", "get_proto_schemes")) {
		gf_opts_set_key("temp_in_proto", FFAVInRegister.name, "video,audio,av");
	}
	if (!gf_opts_get_bool("temp", "helponly") || gf_opts_get_bool("temp", "gendoc"))
		return &FFAVInRegister;
	
#ifdef FF_PROBE_DEVICES
	Bool audio_pass=GF_FALSE;
	av_log_set_callback(ffavin_log_none);
	const AVInputFormat *fmt = NULL;
	while (1) {
		if (audio_pass) {
			fmt = av_input_audio_device_next(FF_IFMT_CAST fmt);
		} else {
			fmt = av_input_video_device_next(FF_IFMT_CAST fmt);
		}
		if (!fmt) {
			if (audio_pass) break;
			audio_pass = GF_TRUE;
			continue;
		}
		if (!fmt->priv_class) continue;
		if (audio_pass && (fmt->priv_class->category!=AV_CLASS_CATEGORY_DEVICE_AUDIO_INPUT)) continue;
		else if (!audio_pass && (fmt->priv_class->category!=AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT)) continue;
		ffavin_enum_devices(fmt->name, audio_pass);
	}
	av_log_set_callback(av_log_default_callback);
	if (dev_desc) {
		char *out_doc = NULL;
		gf_dynstrcat(&out_doc, FFAVInRegister.help, NULL);
		gf_dynstrcat(&out_doc, dev_desc, "\n");
		gf_free(dev_desc);
		FFAVInRegister.help = out_doc;
		ffmpeg_register_set_dyn_help(&FFAVInRegister);
	}
#endif
	return &FFAVInRegister;
}

#else

#include <gpac/filters.h>

const GF_FilterRegister *ffdmx_register(GF_FilterSession *session)
{
	return NULL;
}

const GF_FilterRegister *ffavin_register(GF_FilterSession *session)
{
	return NULL;
}
#endif
