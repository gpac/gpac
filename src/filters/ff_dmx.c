/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2018
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

enum
{
	COPY_NO,
	COPY_A,
	COPY_V,
	COPY_AV
};

typedef struct
{
	//options
	const char *src;
	u32 buffer_size;
	u32 copy, probes;
	Bool sclock;
	const char *fmt, *dev;

	//internal data
	const char *fname;
	u32 log_class;

	Bool initialized;
	Bool raw_data;
	//input file
	AVFormatContext *demuxer;
	//demux options
	AVDictionary *options;

	GF_FilterPid **pids;

	Bool raw_pck_out;
	u32 nb_playing;

	u64 first_sample_clock, last_frame_ts;
	u32 probe_frames;

	AVPacket pkt;
	s32 audio_idx, video_idx;

	u64 *probe_times;

	Bool copy_audio, copy_video;
} GF_FFDemuxCtx;

static void ffdmx_finalize(GF_Filter *filter)
{
	GF_FFDemuxCtx *ctx = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);
	if (ctx->pids)
		gf_free(ctx->pids);
	if (ctx->options)
		av_dict_free(&ctx->options);
	if (ctx->probe_times)
		gf_free(ctx->probe_times);
	return;
}

void ffdmx_shared_pck_release(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_FFDemuxCtx *ctx = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);
	if (ctx->raw_pck_out) {
		av_free_packet(&ctx->pkt);
		ctx->raw_pck_out = GF_FALSE;
		gf_filter_post_process_task(filter);
	}
}

static GF_Err ffdmx_process(GF_Filter *filter)
{
	GF_Err e;
	u32 i;
	u64 sample_time;
	u8 *data_dst;
	Bool copy = GF_TRUE;
	GF_FilterPacket *pck_dst;
	GF_FFDemuxCtx *ctx = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);

	if (!ctx->nb_playing)
		return GF_EOS;

	if (ctx->raw_pck_out)
		return GF_EOS;

	sample_time = gf_sys_clock_high_res();
	av_init_packet(&ctx->pkt);
	ctx->pkt.stream_index = -1;
	/*EOF*/
	if (av_read_frame(ctx->demuxer, &ctx->pkt) <0) {
		if (!ctx->raw_data) {
			for (i=0; i<ctx->demuxer->nb_streams; i++) {
				if (ctx->pids[i]) gf_filter_pid_set_eos(ctx->pids[i]);
			}
			return GF_EOS;
		}
		return GF_OK;
	}

	assert(ctx->pkt.stream_index>=0);
	assert(ctx->pkt.stream_index < (s32) ctx->demuxer->nb_streams);

	if (ctx->pkt.pts == AV_NOPTS_VALUE) {
		if (ctx->pkt.dts == AV_NOPTS_VALUE) {
			GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] No PTS for packet on stream %d\n", ctx->fname, ctx->pkt.stream_index ));
		} else {
			ctx->pkt.pts = ctx->pkt.dts;
		}
	}

	if (! ctx->pids[ctx->pkt.stream_index] ) {
		GF_LOG(GF_LOG_DEBUG, ctx->log_class, ("[%s] No PID defined for given stream %d\n", ctx->fname, ctx->pkt.stream_index ));
		av_free_packet(&ctx->pkt);
		return GF_OK;
	}

	if (ctx->raw_data && (ctx->probe_frames<ctx->probes) ) {
		if (ctx->pkt.stream_index==ctx->audio_idx) {
			av_free_packet(&ctx->pkt);
			return GF_OK;
		}
		
		ctx->probe_times[ctx->probe_frames] = ctx->sclock ? sample_time : ctx->pkt.pts;
		ctx->probe_frames++;
		if (ctx->probe_frames==ctx->probes) {
			u32 i;
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
			av_free_packet(&ctx->pkt);
			return GF_OK;
		}
	}

	if (ctx->raw_data) {
		if (ctx->pkt.stream_index==ctx->audio_idx) copy = ctx->copy_audio;
		else copy = ctx->copy_video;
	}

	if (ctx->raw_data && !copy) {
		//we don't use shared memory on demuxers since they are usually the ones performing all the buffering
		pck_dst = gf_filter_pck_new_shared(ctx->pids[ctx->pkt.stream_index], ctx->pkt.data, ctx->pkt.size, ffdmx_shared_pck_release);
		ctx->raw_pck_out = GF_TRUE;
	} else {
		//we don't use shared memory on demuxers since they are usually the ones performing all the buffering
		pck_dst = gf_filter_pck_new_alloc(ctx->pids[ctx->pkt.stream_index] , ctx->pkt.size, &data_dst);
		assert(pck_dst);
		memcpy(data_dst, ctx->pkt.data, ctx->pkt.size);
	}


	if (ctx->raw_data && ctx->sclock) {
		u64 ts;
		if (!ctx->first_sample_clock) {
			ctx->last_frame_ts = ctx->first_sample_clock = sample_time;
		}
		ts = sample_time - ctx->first_sample_clock;
		gf_filter_pck_set_cts(pck_dst, ts );
		ctx->last_frame_ts = ts;
	} else if (ctx->pkt.pts != AV_NOPTS_VALUE) {
		AVStream *stream = ctx->demuxer->streams[ctx->pkt.stream_index];
		u64 ts = ctx->pkt.pts * stream->time_base.num;

		gf_filter_pck_set_cts(pck_dst, ts );

		if (ctx->pkt.dts != AV_NOPTS_VALUE) {
			ts = ctx->pkt.dts * stream->time_base.num;
			gf_filter_pck_set_dts(pck_dst, ts);
		}

		if (ctx->pkt.duration)
			gf_filter_pck_set_duration(pck_dst, (u32) ctx->pkt.duration);
	}

	//fixme: try to identify SAP type 2 and more
	if (ctx->pkt.flags & AV_PKT_FLAG_KEY)
		gf_filter_pck_set_sap(pck_dst, GF_FILTER_SAP_1);

	if (ctx->pkt.flags & AV_PKT_FLAG_CORRUPT)
		gf_filter_pck_set_corrupted(pck_dst, GF_TRUE);

	gf_net_get_utc();

	if (ctx->raw_data) {
		u64 ntp = gf_net_get_ntp_ts();
		gf_filter_pck_set_property(pck_dst, GF_PROP_PCK_SENDER_NTP, &PROP_LONGUINT(ntp) );
	}
	e = gf_filter_pck_send(pck_dst);

	if (!ctx->raw_pck_out)
		av_free_packet(&ctx->pkt);
	return e;
}


static GF_Err ffdmx_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	s32 res;
	GF_FFDemuxCtx *ctx = gf_filter_get_udta(filter);

	//initial parsing of arguments
	if (!ctx->initialized) {
		switch (arg_val->type) {
		case GF_PROP_STRING:
			res = av_dict_set(&ctx->options, arg_name, arg_val->value.string, 0);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Failed to set option %s:%s\n", ctx->fname, arg_name, arg_val ));
			}
			break;
		default:
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Failed to set option %s:%s, unrecognized type %d\n", ctx->fname, arg_name, arg_val, arg_val->type ));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
	//updates of arguments, not supported for ffmpeg demuxers
	return GF_NOT_SUPPORTED;
}

GF_Err ffdmx_init_common(GF_Filter *filter, GF_FFDemuxCtx *ctx)
{
	u32 i;
	u32 nb_a, nb_v, nb_t;
	char szName[50];


	ctx->pids = gf_malloc(sizeof(GF_FilterPid *)*ctx->demuxer->nb_streams);
	memset(ctx->pids, 0, sizeof(GF_FilterPid *)*ctx->demuxer->nb_streams);

	nb_t = nb_a = nb_v = 0;
	for (i = 0; i < ctx->demuxer->nb_streams; i++) {
		GF_FilterPid *pid=NULL;
		Bool force_reframer = GF_FALSE;
		Bool expose_ffdec=GF_FALSE;
		AVStream *stream = ctx->demuxer->streams[i];
		AVCodecContext *codec = stream->codec;
		u32 gpac_codec_id;

		switch(codec->codec_type) {
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
#ifdef FF_SUB_SUPPORT
		case AVMEDIA_TYPE_SUBTITLE:
			pid = gf_filter_pid_new(filter);
			if (!pid) return GF_OUT_OF_MEM;
			gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_TEXT) );
			nb_t++;
			sprintf(szName, "text%d", nb_t);
			break;
#endif
		default:
			sprintf(szName, "ffdmx%d", i+1);
			break;
		}
		if (!pid) continue;
		ctx->pids[i] = pid;
		gf_filter_pid_set_udta(pid, stream);

		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(stream->id ? stream->id : i+1) );
		gf_filter_pid_set_name(pid, szName);

		if (ctx->raw_data && ctx->sclock) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000000) );
		} else {
			gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(stream->time_base.den) );
		}

		if (!ctx->raw_data && (stream->duration>=0))
			gf_filter_pid_set_property(pid, GF_PROP_PID_DURATION, &PROP_FRAC_INT((u32) stream->duration, stream->time_base.den) );

		if (stream->sample_aspect_ratio.num && stream->sample_aspect_ratio.den)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAR, &PROP_FRAC_INT( stream->sample_aspect_ratio.num, stream->sample_aspect_ratio.den ) );

		if (stream->metadata) {
			AVDictionaryEntry *ent=NULL;
			while (1) {
				ent = av_dict_get(stream->metadata, "", ent, AV_DICT_IGNORE_SUFFIX);
				if (!ent) break;

				//we use the same syntax as ffmpeg here
				gf_filter_pid_set_property_str(pid, ent->key, &PROP_STRING(ent->value) );
			}
		}

		gpac_codec_id = ffmpeg_codecid_to_gpac(codec->codec_id);
		if (!gpac_codec_id) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_FFMPEG) );
			expose_ffdec=GF_TRUE;
		} else {
			gf_filter_pid_set_property(pid, GF_PROP_PID_CODECID, &PROP_UINT(gpac_codec_id) );
		}

		//force reframer for the following formats if no DSI is found
		if (!codec->extradata_size) {
			switch (gpac_codec_id) {
			case GF_CODECID_AC3:
			case GF_CODECID_AAC_MPEG4:
			case GF_CODECID_AAC_MPEG2_MP:
			case GF_CODECID_AAC_MPEG2_LCP:
			case GF_CODECID_AAC_MPEG2_SSRP:
			case GF_CODECID_AVC:
			case GF_CODECID_HEVC:
			case GF_CODECID_AV1:
				force_reframer = GF_TRUE;
				break;
			}
		}
		if (expose_ffdec) {
			const char *cname = avcodec_get_name(codec->codec_id);
			gf_filter_pid_set_property(pid, GF_FFMPEG_DECODER_CONFIG, &PROP_POINTER( (void*)codec ) );

			if (cname)
				gf_filter_pid_set_property_str(pid, "ffmpeg:codec", &PROP_STRING(cname ) );
		} else if (codec->extradata_size) {

			//avc/hevc read by ffmpeg is still in annex B format
			if (!strcmp(ctx->demuxer->iformat->name, "h264") || !strcmp(ctx->demuxer->iformat->name, "hevc")) {
				force_reframer = GF_TRUE;
			}
			
			if (!force_reframer) {
				//expose as const data
				gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_CONST_DATA( (char *)codec->extradata, codec->extradata_size) );
			}
		}

		if (force_reframer) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_UNFRAMED, &PROP_BOOL(GF_TRUE) );
		}


		if (codec->sample_rate)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT( codec->sample_rate ) );
		if (codec->frame_size)
			gf_filter_pid_set_property(pid, GF_PROP_PID_SAMPLES_PER_FRAME, &PROP_UINT( codec->frame_size ) );
		if (codec->channels)
			gf_filter_pid_set_property(pid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT( codec->channels ) );

		if (codec->width)
			gf_filter_pid_set_property(pid, GF_PROP_PID_WIDTH, &PROP_UINT( codec->width ) );
		if (codec->height)
			gf_filter_pid_set_property(pid, GF_PROP_PID_HEIGHT, &PROP_UINT( codec->height ) );
#if LIBAVCODEC_VERSION_MAJOR >= 58
		if (codec->framerate.num && codec->framerate.den)
			gf_filter_pid_set_property(pid, GF_PROP_PID_FPS, &PROP_FRAC_INT( codec->framerate.num, codec->framerate.den ) );
#endif

		if (codec->pix_fmt>0) {
			u32 pfmt = 0;
			switch (codec->pix_fmt) {
			case AV_PIX_FMT_YUV420P: pfmt = GF_PIXEL_YUV; break;
			case AV_PIX_FMT_YUV420P10LE: pfmt = GF_PIXEL_YUV_10; break;
			case AV_PIX_FMT_YUV422P: pfmt = GF_PIXEL_YUV422; break;
			case AV_PIX_FMT_YUV422P10LE: pfmt = GF_PIXEL_YUV422_10; break;
			case AV_PIX_FMT_YUV444P: pfmt = GF_PIXEL_YUV444; break;
			case AV_PIX_FMT_YUV444P10LE: pfmt = GF_PIXEL_YUV444_10; break;
			case AV_PIX_FMT_RGBA: pfmt = GF_PIXEL_RGBA; break;
			case AV_PIX_FMT_RGB24: pfmt = GF_PIXEL_RGB; break;
			case AV_PIX_FMT_BGR24: pfmt = GF_PIXEL_BGR; break;
			case AV_PIX_FMT_UYVY422: pfmt = GF_PIXEL_UYVY; break;
			case AV_PIX_FMT_YUYV422: pfmt = GF_PIXEL_YUYV; break;
			case AV_PIX_FMT_NV12: pfmt = GF_PIXEL_NV12; break;
			case AV_PIX_FMT_NV21: pfmt = GF_PIXEL_NV21; break;
			case AV_PIX_FMT_0RGB: pfmt = GF_PIXEL_XRGB; break;
			case AV_PIX_FMT_RGB0: pfmt = GF_PIXEL_RGBX; break;
			case AV_PIX_FMT_0BGR: pfmt = GF_PIXEL_XBGR; break;
			case AV_PIX_FMT_BGR0: pfmt = GF_PIXEL_BGRX; break;

			default:
				GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] Unsupported pixel format %d\n", ctx->fname, codec->pix_fmt));
			}
			gf_filter_pid_set_property(pid, GF_PROP_PID_PIXFMT, &PROP_UINT( pfmt) );
		}
		if (codec->sample_fmt>0) {
			u32 sfmt = 0;
			switch (codec->sample_fmt) {
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
				GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] Unsupported sample format %d\n", ctx->fname, codec->sample_fmt));
			}
			gf_filter_pid_set_property(pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT( sfmt) );
		}

		if (codec->bit_rate)
			gf_filter_pid_set_property(pid, GF_PROP_PID_BITRATE, &PROP_UINT( (u32) codec->bit_rate ) );

		gf_filter_pid_set_property(pid, GF_PROP_PID_URL, &PROP_STRING(ctx->demuxer->filename));
	}

	if (!nb_t && !nb_a && !nb_v)
		return GF_NOT_SUPPORTED;

	return GF_OK;
}

static GF_Err ffdmx_initialize(GF_Filter *filter)
{
	GF_FFDemuxCtx *ctx = gf_filter_get_udta(filter);
	GF_Err e;
	s32 res;
	char *ext;
	AVInputFormat *av_in = NULL;
	ctx->fname = "FFDmx";
	ctx->log_class = GF_LOG_CONTAINER;

	ffmpeg_setup_logs(ctx->log_class);

	ctx->initialized = GF_TRUE;
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

	res = avformat_open_input(&ctx->demuxer, ctx->src, av_in, ctx->options ? &ctx->options : NULL);

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
	default:
		e = GF_NOT_SUPPORTED;
		break;
	}
	if (e) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Fail to open %s - error %s\n", ctx->fname, ctx->src, av_err2str(res) ));
		return e;
	}

	AVDictionaryEntry *prev_e = NULL;
	while (1) {
		prev_e = av_dict_get(ctx->options, "", prev_e, AV_DICT_IGNORE_SUFFIX);
		if (!prev_e) break;
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[%s] meta-filter option %s=%s set but not used, see gpac -h ffdmx and gpac -h ffdmx:%s for allowed options\n", ctx->fname, prev_e->key, prev_e->value, ctx->demuxer->iformat->name));
		gf_filter_report_unused_meta_option(filter, prev_e->key);
	}

	res = avformat_find_stream_info(ctx->demuxer, ctx->options ? &ctx->options : NULL);

	if (res <0) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] cannot locate streams - error %s\n", ctx->fname, av_err2str(res)));
		e = GF_NOT_SUPPORTED;
		return e;
	}
	GF_LOG(GF_LOG_DEBUG, ctx->log_class, ("[%s] file %s opened - %d streams\n", ctx->fname, ctx->src, ctx->demuxer->nb_streams));

	return ffdmx_init_common(filter, ctx);
}


static Bool ffdmx_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	GF_FFDemuxCtx *ctx = gf_filter_get_udta(filter);

	switch (com->base.type) {
	case GF_FEVT_PLAY:
		if (!ctx->nb_playing && !ctx->raw_data && (com->play.start_range>0) ) {
			int res = av_seek_frame(ctx->demuxer, -1, (s64) (AV_TIME_BASE*com->play.start_range), AVSEEK_FLAG_BACKWARD);
			if (res<0) {
				GF_LOG(GF_LOG_WARNING, ctx->log_class, ("[%s] Fail to seek %s to %g - error %s\n", ctx->fname, ctx->src, com->play.start_range, av_err2str(res) ));
			}
		}
		ctx->nb_playing++;

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		if (ctx->nb_playing) ctx->nb_playing--;
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
	return GF_FPROBE_MAYBE_SUPPORTED;
}


static const char *ffdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	int ffscore;
	AVInputFormat *probe_fmt;
	AVProbeData pb;

	av_register_all();


	memset(&pb, 0, sizeof(AVProbeData));
	//not setting this crashes some probers in ffmpeg
	pb.filename = "";
	if (size <= AVPROBE_PADDING_SIZE) {
		pb.buf = gf_malloc(sizeof(char)*(size+AVPROBE_PADDING_SIZE) );
		memcpy(pb.buf, data, sizeof(char)*size);
		pb.buf_size = size;
		probe_fmt = av_probe_input_format3(&pb, GF_FALSE, &ffscore);
		gf_free(pb.buf);
	} else {
		pb.buf =  (char *) data;
		pb.buf_size = size - AVPROBE_PADDING_SIZE;
		probe_fmt = av_probe_input_format3(&pb, GF_FALSE, &ffscore);
	}

	if (!probe_fmt) return NULL;
	if (probe_fmt->mime_type) {
		//TODO try to refine based on ffprobe score
		*score = GF_FPROBE_MAYBE_SUPPORTED;
		return probe_fmt->mime_type;
	}
	return NULL;
}

#define OFFS(_n)	#_n, offsetof(GF_FFDemuxCtx, _n)

static const GF_FilterCapability FFDmxCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
};


GF_FilterRegister FFDemuxRegister = {
	.name = "ffdmx",
	.version=LIBAVFORMAT_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG demuxer")
	GF_FS_SET_HELP("See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more detailed info on demuxers options")
	.private_size = sizeof(GF_FFDemuxCtx),
	SETCAPS(FFDmxCaps),
	.initialize = ffdmx_initialize,
	.finalize = ffdmx_finalize,
	.process = ffdmx_process,
	.update_arg = ffdmx_update_arg,
	.probe_url = ffdmx_probe_url,
	.probe_data = ffdmx_probe_data,
	.process_event = ffdmx_process_event
};


void ffdmx_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	ffmpeg_register_free(session, reg, 1);
}

static const GF_FilterArgs FFDemuxArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, 0},
	{ "*", -1, "any possible args defined for AVFormatContext and sub-classes. See `gpac -hx ffdmx` and `gpac -hx ffdmx:*`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};

const GF_FilterRegister *ffdmx_register(GF_FilterSession *session)
{
	GF_FilterArgs *args;
	u32 i=0;
	const struct AVOption *opt;
	AVFormatContext *dmx_ctx;
	Bool load_meta_filters = session ? GF_TRUE : GF_FALSE;
	ffmpeg_initialize();

	if (!load_meta_filters) {
		FFDemuxRegister.args = FFDemuxArgs;
		FFDemuxRegister.register_free = NULL;
		return &FFDemuxRegister;
	}

	FFDemuxRegister.register_free = ffdmx_regfree;
	dmx_ctx = avformat_alloc_context();

	while (dmx_ctx->av_class->option) {
		opt = &dmx_ctx->av_class->option[i];
		if (!opt || !opt->name) break;
		i++;
	}
	i+=2;

	args = gf_malloc(sizeof(GF_FilterArgs)*(i+1));
	memset(args, 0, sizeof(GF_FilterArgs)*(i+1));
	FFDemuxRegister.args = args;
	args[0] = (GF_FilterArgs){ OFFS(src), "location of source content", GF_PROP_STRING, NULL, NULL, 0} ;
	i=0;
	while (dmx_ctx->av_class->option) {
		opt = &dmx_ctx->av_class->option[i];
		if (!opt || !opt->name) break;
		args[i+1] = ffmpeg_arg_translate(opt);
		i++;
	}
	args[i+1] = (GF_FilterArgs) { "*", -1, "meta options depend on input type, check individual filter syntax", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_EXPERT};

	avformat_free_context(dmx_ctx);

	ffmpeg_expand_register(session, &FFDemuxRegister, FF_REG_TYPE_DEMUX);

	return &FFDemuxRegister;
}



static GF_Err ffavin_initialize(GF_Filter *filter)
{
	s32 res, i;
	Bool has_a, has_v;
	char szPatchedName[1024];
	const char *dev_name=NULL;
	const char *default_fmt=NULL;
	AVInputFormat *dev_fmt=NULL;
	GF_FFDemuxCtx *ctx = gf_filter_get_udta(filter);
	Bool wants_audio = GF_FALSE;
	Bool wants_video = GF_FALSE;
	ctx->fname = "FFAVIn";
	ctx->log_class = GF_LOG_MMIO;

	ffmpeg_setup_logs(ctx->log_class);

	avdevice_register_all();
	ctx->initialized = GF_TRUE;
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
#else
		default_fmt = "video4linux2";
#endif
	}

	if (default_fmt) {
		dev_fmt = av_find_input_format(default_fmt);
		if (dev_fmt == NULL) {
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Cannot find the input format %s\n", ctx->fname, ctx->fmt));
		}
#if LIBAVCODEC_VERSION_MAJOR >= 58
		else if (dev_fmt->priv_class->category!=AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT) {
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s]] %s is not a video input device\n", ctx->fname, ctx->fmt));
			dev_fmt = NULL;
		}
#else
		//not supported for old FFMPE versions
#endif
	}
#if (LIBAVCODEC_VERSION_MAJOR >= 58) && (LIBAVCODEC_VERSION_MINOR>=20)
	if (!dev_fmt) {
		while (1) {
			dev_fmt = av_input_video_device_next(dev_fmt);
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

#if defined(__DARWIN) || defined(__APPLE__)
	if (!strncmp(dev_name, "screen", 6)) {
		strcpy(szPatchedName, "Capture screen ");
		strcat(szPatchedName, dev_name+6);
		dev_name = (char *) szPatchedName;
	}
#endif

	if (wants_video && wants_audio && !strcmp(dev_name, "0")) {
		strcpy(szPatchedName, "0:0");
		dev_name = (char *) szPatchedName;
	}
#if defined(__APPLE__) && !defined(GPAC_CONFIG_IOS)
	else if (!strncmp(dev_fmt->priv_class->class_name, "AVFoundation", 12) && wants_audio) {
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

	res = avformat_open_input(&ctx->demuxer, dev_name, dev_fmt, &ctx->options);
	if ( (res < 0) && !stricmp(ctx->dev, "screen-capture-recorder") ) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Buggy screen capture input (open failed with code %d), retrying without specifying resolution\n", ctx->fname, res));
		av_dict_set(&ctx->options, "video_size", NULL, 0);
		res = avformat_open_input(&ctx->demuxer, ctx->dev, dev_fmt, &ctx->options);
	}

	if (res < 0) {
		if (ctx->options) {
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Error %d opening input - retrying without options\n", ctx->fname, res));
			res = avformat_open_input(&ctx->demuxer, dev_name, dev_fmt, NULL);
		} else {
			av_dict_set(&ctx->options, "framerate", "30", 0);
			GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Error %d opening input - retrying with 30 fps\n", ctx->fname, res));
			res = avformat_open_input(&ctx->demuxer, dev_name, dev_fmt, &ctx->options);
			if (res < 0) {
				av_dict_set(&ctx->options, "framerate", "25", 0);
				GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Error %d opening input - retrying with 25 fps\n", ctx->fname, res));
				res = avformat_open_input(&ctx->demuxer, dev_name, dev_fmt, &ctx->options);
			}
		}
	}

	if (res < 0) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] Cannot open device %s:%s\n", ctx->fname, dev_fmt->priv_class->class_name, ctx->dev));
		return -1;
	}

	AVDictionaryEntry *prev_e = NULL;
	while (1) {
		prev_e = av_dict_get(ctx->options, "", prev_e, AV_DICT_IGNORE_SUFFIX);
		if (!prev_e) break;
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[%s] meta-filter option %s=%s set but not used, see gpac -h ffavin and gpac -h ffavin:%s for allowed options\n", ctx->fname, prev_e->key, prev_e->value, ctx->demuxer->iformat->name));
		gf_filter_report_unused_meta_option(filter, prev_e->key);
	}

	av_dump_format(ctx->demuxer, 0, ctx->dev, 0);
	ctx->raw_data = GF_TRUE;
	ctx->audio_idx = ctx->video_idx = -1;

	res = avformat_find_stream_info(ctx->demuxer, ctx->options ? &ctx->options : NULL);

	if (res <0) {
		GF_LOG(GF_LOG_ERROR, ctx->log_class, ("[%s] cannot locate streams - error %s\n", ctx->fname,  av_err2str(res)));
		return GF_NOT_SUPPORTED;
	}

	//check we have the stream we want
	has_a = has_v = GF_FALSE;
	for (i = 0; (u32) i < ctx->demuxer->nb_streams; i++) {
		switch(ctx->demuxer->streams[i]->codec->codec_type) {
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
	return ffdmx_init_common(filter, ctx);
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
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister FFAVInRegister = {
	.name = "ffavin",
	.version = LIBAVDEVICE_IDENT,
	GF_FS_SET_DESCRIPTION("FFMPEG AV Capture")
	GF_FS_SET_HELP("Typical classes are `dshow` on windows, `avfoundation` on OSX, `video4linux2` or `x11grab` on linux\n\n"\
	"Typical device name can be the webcam name:\n"\
		"`FaceTime HD Camera` on OSX, device name on windows, `/dev/video0` on linux\n"\
		"`screen-capture-recorder`, see http://screencapturer.sf.net/ on windows\n"\
		"`Capture screen 0` on OSX (0=first screen), or `screenN` for short\n"\
		"X display name (eg `:0.0`) on linux"\
		"\n"\
		"See FFMPEG documentation (https://ffmpeg.org/documentation.html) for more detailed info on capture devices options")
	.private_size = sizeof(GF_FFDemuxCtx),
	SETCAPS(FFAVInCaps),
	.initialize = ffavin_initialize,
	.finalize = ffdmx_finalize,
	.process = ffdmx_process,
	.update_arg = ffdmx_update_arg,
	.probe_url = ffavin_probe_url,
	.process_event = ffdmx_process_event
};


static const GF_FilterArgs FFAVInArgs[] =
{
	{ OFFS(src), "url of device, `video://`, `audio://` or `av://`", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(fmt), "name of device class - see filter help. If not set, defaults to first device class", GF_PROP_STRING, NULL, NULL, 0},
	{ OFFS(dev), "name of device or index of device - see filter help", GF_PROP_STRING, "0", NULL, 0},
	{ OFFS(copy), "set copy mode of raw frames\n"
	"- N: frames are only forwarded (shared memory, no copy)\n"
	"- A: audio frames are copied, video frames are forwarded\n"
	"- V: video frames are copied, audio frames are forwarded\n"
	"- AV: all frames are copied"
	"", GF_PROP_UINT, "A", "N|A|V|AV", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sclock), "use system clock (us) instead of device timestamp (for buggy devices)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(probes), "probe a given number of video frames before emitting - this usually helps with bad timing of the first frames", GF_PROP_UINT, "10", "0-100", GF_FS_ARG_HINT_EXPERT},


	{ "*", -1, "any possible args defined for AVInputFormat and AVFormatContext. See `gpac -hx ffavin` and `gpac -hx ffavin:*`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_META},
	{0}
};


//number of arguments defined above
const int FFAVIN_STATIC_ARGS = (sizeof (FFAVInArgs) / sizeof (GF_FilterArgs)) - 1;

void ffavin_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	ffmpeg_register_free(session, reg, FFAVIN_STATIC_ARGS-1);
}

const GF_FilterRegister *ffavin_register(GF_FilterSession *session)
{
	GF_FilterArgs *args;
	u32 i;
	Bool load_meta_filters = session ? GF_TRUE : GF_FALSE;
	ffmpeg_initialize();

	if (!load_meta_filters) {
		FFAVInRegister.args = FFAVInArgs;
		FFAVInRegister.register_free = NULL;
		return &FFAVInRegister;
	}

	FFAVInRegister.register_free = ffavin_regfree;
	args = gf_malloc(sizeof(GF_FilterArgs)*(FFAVIN_STATIC_ARGS+1));
	memset(args, 0, sizeof(GF_FilterArgs)*(FFAVIN_STATIC_ARGS+1));
	for (i=0; (s32) i<FFAVIN_STATIC_ARGS; i++)
		args[i] = FFAVInArgs[i];

	FFAVInRegister.args = args;

	ffmpeg_expand_register(session, &FFAVInRegister, FF_REG_TYPE_DEV_IN);
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
