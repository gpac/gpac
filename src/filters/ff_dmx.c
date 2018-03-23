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

#include <gpac/filters.h>
#include <gpac/list.h>
#include <gpac/constants.h>
#include <gpac/network.h>
#include "ff_common.h"

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
	AVFormatContext *ctx;
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
	GF_FFDemuxCtx *ffd = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);
	if (ffd->pids)
		gf_free(ffd->pids);
	if (ffd->options)
		av_dict_free(&ffd->options);
	if (ffd->probe_times)
		gf_free(ffd->probe_times);
	return;
}

void ffdmx_shared_pck_release(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_FFDemuxCtx *ffd = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);
	if (ffd->raw_pck_out) {
		av_free_packet(&ffd->pkt);
		ffd->raw_pck_out = GF_FALSE;
		gf_filter_post_process_task(filter);
	}
}

static GF_Err ffdmx_process(GF_Filter *filter)
{
	GF_Err e;
	u32 i;
	u64 sample_time;
	char *data_dst;
	Bool copy = GF_TRUE;
	GF_FilterPacket *pck_dst;
	GF_FFDemuxCtx *ffd = (GF_FFDemuxCtx *) gf_filter_get_udta(filter);

	if (!ffd->nb_playing)
		return GF_EOS;

	if (ffd->raw_pck_out)
		return GF_EOS;

	sample_time = gf_sys_clock_high_res();
	av_init_packet(&ffd->pkt);
	ffd->pkt.stream_index = -1;
	/*EOF*/
	if (av_read_frame(ffd->ctx, &ffd->pkt) <0) {
		if (!ffd->raw_data) {
			for (i=0; i<ffd->ctx->nb_streams; i++) {
				if (ffd->pids[i]) gf_filter_pid_set_eos(ffd->pids[i]);
			}
			return GF_EOS;
		}
		return GF_OK;
	}

	assert(ffd->pkt.stream_index>=0);
	assert(ffd->pkt.stream_index<ffd->ctx->nb_streams);

	if (ffd->pkt.pts == AV_NOPTS_VALUE) {
		if (ffd->pkt.dts == AV_NOPTS_VALUE) {
			GF_LOG(GF_LOG_WARNING, ffd->log_class, ("[%s] No PTS for packet on stream %d\n", ffd->fname, ffd->pkt.stream_index ));
		} else {
			ffd->pkt.pts = ffd->pkt.dts;
		}
	}

	if (! ffd->pids[ffd->pkt.stream_index] ) {
		GF_LOG(GF_LOG_DEBUG, ffd->log_class, ("[%s] No PID defined for given stream %d\n", ffd->fname, ffd->pkt.stream_index ));
		av_free_packet(&ffd->pkt);
		return GF_OK;
	}

	if (ffd->raw_data && (ffd->probe_frames<ffd->probes) ) {
		if (ffd->pkt.stream_index==ffd->audio_idx) {
			av_free_packet(&ffd->pkt);
			return GF_OK;
		}
		
		ffd->probe_times[ffd->probe_frames] = ffd->sclock ? sample_time : ffd->pkt.pts;
		ffd->probe_frames++;
		if (ffd->probe_frames==ffd->probes) {
			u32 i;
			u32 best_diff=0, max_stat=0;
			for (i=0; i<ffd->probes; i++) {
				if (i) {
					u32 j, nb_stats=0;
					u32 diff = (u32) (ffd->probe_times[i]-ffd->probe_times[i-1]);
					for (j=1; j<ffd->probes; j++) {
						s32 sdiff = (s32) (ffd->probe_times[j]-ffd->probe_times[j-1]);
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
			GF_LOG(GF_LOG_INFO, ffd->log_class, ("[%s] Video probing done, frame diff is %d us (for %d frames out of %d)\n", ffd->fname, best_diff, max_stat, ffd->probes));
		} else {
			av_free_packet(&ffd->pkt);
			return GF_OK;
		}
	}

	if (ffd->raw_data) {
		if (ffd->pkt.stream_index==ffd->audio_idx) copy = ffd->copy_audio;
		else copy = ffd->copy_video;
	}

	if (ffd->raw_data && !copy) {
		//we don't use shared memory on demuxers since they are usually the ones performing all the buffering
		pck_dst = gf_filter_pck_new_shared(ffd->pids[ffd->pkt.stream_index], ffd->pkt.data, ffd->pkt.size, ffdmx_shared_pck_release);
		ffd->raw_pck_out = GF_TRUE;
	} else {
		//we don't use shared memory on demuxers since they are usually the ones performing all the buffering
		pck_dst = gf_filter_pck_new_alloc(ffd->pids[ffd->pkt.stream_index] , ffd->pkt.size, &data_dst);
		assert(pck_dst);
		memcpy(data_dst, ffd->pkt.data, ffd->pkt.size);
	}


	if (ffd->raw_data && ffd->sclock) {
		u64 ts;
		if (!ffd->first_sample_clock) {
			ffd->last_frame_ts = ffd->first_sample_clock = sample_time;
		}
		ts = sample_time - ffd->first_sample_clock;
		gf_filter_pck_set_cts(pck_dst, ts );
		ffd->last_frame_ts = ts;
	} else if (ffd->pkt.pts != AV_NOPTS_VALUE) {
		AVStream *stream = ffd->ctx->streams[ffd->pkt.stream_index];
		u64 ts = ffd->pkt.pts * stream->time_base.num;

		gf_filter_pck_set_cts(pck_dst, ts );

		if (!ffd->pkt.dts) ffd->pkt.dts = ffd->pkt.pts;

		if (ffd->pkt.dts != AV_NOPTS_VALUE) {
			ts = ffd->pkt.dts * stream->time_base.num;
			gf_filter_pck_set_dts(pck_dst, ts);
		}

		gf_filter_pck_set_duration(pck_dst, ffd->pkt.duration);
	}

	//fixme: try to identify SAP type 2 and more
	if (ffd->pkt.flags & AV_PKT_FLAG_KEY)
		gf_filter_pck_set_sap(pck_dst, GF_FILTER_SAP_1);

	if (ffd->pkt.flags & AV_PKT_FLAG_CORRUPT)
		gf_filter_pck_set_corrupted(pck_dst, GF_TRUE);

	gf_net_get_utc();

	if (ffd->raw_data) {
		u64 ntp = gf_net_get_ntp_ts();
		gf_filter_pck_set_property(pck_dst, GF_PROP_PCK_SENDER_NTP, &PROP_LONGUINT(ntp) );
	}
	e = gf_filter_pck_send(pck_dst);

	if (!ffd->raw_pck_out)
		av_free_packet(&ffd->pkt);
	return GF_OK;
}


static GF_Err ffdmx_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *arg_val)
{
	s32 res;
	GF_FFDemuxCtx *ffd = gf_filter_get_udta(filter);

	//initial parsing of arguments
	if (!ffd->initialized) {
		switch (arg_val->type) {
		case GF_PROP_STRING:
			res = av_dict_set(&ffd->options, arg_name, arg_val->value.string, 0);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] Failed to set option %s:%s\n", ffd->fname, arg_name, arg_val ));
			}
			break;
		default:
			GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] Failed to set option %s:%s, unrecognized type %d\n", ffd->fname, arg_name, arg_val, arg_val->type ));
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
	//updates of arguments, not supported for ffmpeg demuxers
	return GF_NOT_SUPPORTED;
}

GF_Err ffdmx_init_common(GF_Filter *filter, GF_FFDemuxCtx *ffd)
{
	u32 i;
	u32 nb_a, nb_v, nb_t;
	char szName[50];


	ffd->pids = gf_malloc(sizeof(GF_FilterPid *)*ffd->ctx->nb_streams);
	memset(ffd->pids, 0, sizeof(GF_FilterPid *)*ffd->ctx->nb_streams);

	nb_t = nb_a = nb_v = 0;
	for (i = 0; i < ffd->ctx->nb_streams; i++) {
		GF_FilterPid *pid=NULL;
		Bool expose_ffdec=GF_FALSE;
		AVStream *stream = ffd->ctx->streams[i];
		AVCodecContext *codec = stream->codec;
		u32 gpac_codec_id;

		switch(codec->codec_type) {
		case AVMEDIA_TYPE_AUDIO:
			pid = gf_filter_pid_new(filter);
			if (!pid) return GF_OUT_OF_MEM;
			gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO) );
			nb_a++;
			sprintf(szName, "audio%d", nb_a);
			if (ffd->audio_idx<0)
				ffd->audio_idx = i;
			break;

		case AVMEDIA_TYPE_VIDEO:
			pid = gf_filter_pid_new(filter);
			if (!pid) return GF_OUT_OF_MEM;
			gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_VISUAL) );
			nb_v++;
			sprintf(szName, "video%d", nb_v);
			if (ffd->video_idx<0)
				ffd->video_idx = i;
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
		ffd->pids[i] = pid;
		gf_filter_pid_set_udta(pid, stream);

		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(stream->id ? stream->id : i+1) );
		gf_filter_pid_set_name(pid, szName);

		if (ffd->raw_data && ffd->sclock) {
			gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(1000000) );
		} else {
			gf_filter_pid_set_property(pid, GF_PROP_PID_TIMESCALE, &PROP_UINT(stream->time_base.den) );
		}

		if (!ffd->raw_data && (stream->duration>=0))
			gf_filter_pid_set_property(pid, GF_PROP_PID_DURATION, &PROP_FRAC_INT(stream->duration, stream->time_base.den) );

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

		if (expose_ffdec) {
			gf_filter_pid_set_property(pid, GF_FFMPEG_DECODER_CONFIG, &PROP_POINTER( (void*)codec ) );
		} else if (codec->extradata_size) {
			//expose as const data
			gf_filter_pid_set_property(pid, GF_PROP_PID_DECODER_CONFIG, &PROP_CONST_DATA( (char *)codec->extradata, codec->extradata_size) );
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
		if (codec->framerate.num && codec->framerate.den)
			gf_filter_pid_set_property(pid, GF_PROP_PID_FPS, &PROP_FRAC_INT( codec->framerate.num, codec->framerate.den ) );

		if (codec->pix_fmt>0) {
			u32 pfmt = 0;
			switch (codec->pix_fmt) {
			case AV_PIX_FMT_YUV420P: pfmt = GF_PIXEL_YV12; break;
			case AV_PIX_FMT_YUV420P10LE: pfmt = GF_PIXEL_YV12_10; break;
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
				GF_LOG(GF_LOG_WARNING, ffd->log_class, ("[%s] Unsupported pixel format %d\n", ffd->fname, codec->pix_fmt));
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
				GF_LOG(GF_LOG_WARNING, ffd->log_class, ("[%s] Unsupported sample format %d\n", ffd->fname, codec->sample_fmt));
			}
			gf_filter_pid_set_property(pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT( sfmt) );
		}

		if (codec->bit_rate)
			gf_filter_pid_set_property(pid, GF_PROP_PID_BITRATE, &PROP_UINT( codec->bit_rate ) );
	}

	if (!nb_t && !nb_a && !nb_v)
		return GF_NOT_SUPPORTED;

	return GF_OK;
}


static GF_Err ffdmx_initialize(GF_Filter *filter)
{
	GF_FFDemuxCtx *ffd = gf_filter_get_udta(filter);
	GF_Err e;
	s32 res;
	char *ext;
	AVInputFormat *av_in = NULL;
	ffd->fname = "FFDmx";
	ffd->log_class = GF_LOG_CONTAINER;

//	av_log_set_level(AV_LOG_DEBUG);

	ffd->initialized = GF_TRUE;
	if (!ffd->src) return GF_SERVICE_ERROR;

	/*some extensions not supported by ffmpeg, overload input format*/
	ext = strrchr(ffd->src, '.');
	if (ext) {
		if (!stricmp(ext+1, "cmp")) av_in = av_find_input_format("m4v");
	}

	GF_LOG(GF_LOG_DEBUG, ffd->log_class, ("[%s] opening file %s - av_in %08x\n", ffd->fname, ffd->src, av_in));

	res = avformat_open_input(&ffd->ctx, ffd->src, av_in, ffd->options ? &ffd->options : NULL);
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
		GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] Fail to open %s - error %s\n", ffd->fname, ffd->src, av_err2str(res) ));
		return e;
	}

	res = avformat_find_stream_info(ffd->ctx, ffd->options ? &ffd->options : NULL);

	if (res <0) {
		GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] cannot locate streams - error %s\n", ffd->fname, av_err2str(res)));
		e = GF_NOT_SUPPORTED;
		return e;
	}
	GF_LOG(GF_LOG_DEBUG, ffd->log_class, ("[%s] file %s opened - %d streams\n", ffd->fname, ffd->src, ffd->ctx->nb_streams));

	return ffdmx_init_common(filter, ffd);
}


static Bool ffdmx_process_event(GF_Filter *filter, const GF_FilterEvent *com)
{
	GF_FFDemuxCtx *ffd = gf_filter_get_udta(filter);

	switch (com->base.type) {
	case GF_FEVT_PLAY:
		if (!ffd->nb_playing && !ffd->raw_data) {
			int res = av_seek_frame(ffd->ctx, -1, (AV_TIME_BASE*com->play.start_range), AVSEEK_FLAG_BACKWARD);
			if (res<0) {
				GF_LOG(GF_LOG_WARNING, ffd->log_class, ("[%s] Fail to seek %s to %g - error %s\n", ffd->fname, ffd->src, com->play.start_range, av_err2str(res) ));
			}
		}
		ffd->nb_playing++;

		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		if (ffd->nb_playing) ffd->nb_playing--;
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
	return GF_FPROBE_MAYBE_SUPPORTED;
}

#define OFFS(_n)	#_n, offsetof(GF_FFDemuxCtx, _n)


GF_FilterRegister FFDemuxRegister = {
	.name = "ffdmx",
	.description = "FFMPEG demuxer "LIBAVFORMAT_IDENT,
	.private_size = sizeof(GF_FFDemuxCtx),
	.initialize = ffdmx_initialize,
	.finalize = ffdmx_finalize,
	.process = ffdmx_process,
	.update_arg = ffdmx_update_arg,
	.probe_url = ffdmx_probe_url,
	.process_event = ffdmx_process_event
};


void ffdmx_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	ffmpeg_registry_free(session, reg, 1);
}

static const GF_FilterArgs FFDemuxArgs[] =
{
	{ OFFS(src), "location of source content", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ "*", -1, "Any possible args defined for AVFormatContext and sub-classes", GF_PROP_UINT, NULL, NULL, GF_FALSE, GF_TRUE},
	{}
};

const GF_FilterRegister *ffdmx_register(GF_FilterSession *session)
{
	GF_FilterArgs *args;
	u32 i=0;
	const struct AVOption *opt;
	AVFormatContext *ctx;
	Bool load_meta_filters = session ? GF_TRUE : GF_FALSE;
	ffmpeg_initialize();

	if (!load_meta_filters) {
		FFDemuxRegister.args = FFDemuxArgs;
		return &FFDemuxRegister;
	}

	FFDemuxRegister.registry_free = ffdmx_regfree;
	ctx = avformat_alloc_context();

	while (ctx->av_class->option) {
		opt = &ctx->av_class->option[i];
		if (!opt || !opt->name) break;
		i++;
	}
	i+=2;

	args = gf_malloc(sizeof(GF_FilterArgs)*(i+1));
	memset(args, 0, sizeof(GF_FilterArgs)*(i+1));
	FFDemuxRegister.args = args;
	args[0] = (GF_FilterArgs){ OFFS(src), "Source URL", GF_PROP_STRING, NULL, NULL, GF_FALSE} ;
	i=0;
	while (ctx->av_class->option) {
		opt = &ctx->av_class->option[i];
		if (!opt || !opt->name) break;
		args[i+1] = ffmpeg_arg_translate(opt);
		i++;
	}
	args[i+1] = (GF_FilterArgs) { "*", -1, "Options depend on input type, check individual filter syntax", GF_PROP_STRING, NULL, NULL, GF_FALSE};

	avformat_free_context(ctx);

	ffmpeg_expand_registry(session, &FFDemuxRegister, 2);

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
	GF_FFDemuxCtx *ffd = gf_filter_get_udta(filter);
	Bool wants_audio = GF_FALSE;
	Bool wants_video = GF_FALSE;
	ffd->fname = "FFAVIn";
	ffd->log_class = GF_LOG_MMIO;

//	av_log_set_level(AV_LOG_DEBUG);

	avdevice_register_all();
	ffd->initialized = GF_TRUE;
	if (!ffd->src) return GF_SERVICE_ERROR;

	default_fmt = ffd->fmt;
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
			GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] Cannot find the input format %s\n", ffd->fname, ffd->fmt));
		} else if (dev_fmt->priv_class->category!=AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT) {
			GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s]] %s is not a video input device\n", ffd->fname, ffd->fmt));
			dev_fmt = NULL;
		}
	}
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

	dev_name = ffd->dev;

	if (!strncmp(ffd->src, "video://", 8)) wants_video = GF_TRUE;
	else if (!strncmp(ffd->src, "audio://", 8)) wants_audio = GF_TRUE;
	else if (!strncmp(ffd->src, "av://", 5)) wants_video = wants_audio = GF_TRUE;

	if (wants_video && wants_audio && !strcmp(dev_name, "0")) {
		strcpy(szPatchedName, "0:0");
		dev_name = (char *) szPatchedName;
	}
#if defined(__APPLE__) && !defined(GPAC_IPHONE)
	else if (!strncmp(dev_fmt->priv_class->class_name, "AVFoundation", 12) && wants_audio) {
		if (ffd->dev[0] != ':') {
			strcpy(szPatchedName, ":");
			strcat(szPatchedName, ffd->dev);
			dev_name = (char *) szPatchedName;
		}
	}
#endif

	/* Open video */
	res = avformat_open_input(&ffd->ctx, dev_name, dev_fmt, &ffd->options);
	if ( (res < 0) && !stricmp(ffd->dev, "screen-capture-recorder") ) {
		GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] Buggy screen capture input (open failed with code %d), retrying without specifying resolution\n", ffd->fname, res));
		av_dict_set(&ffd->options, "video_size", NULL, 0);
		res = avformat_open_input(&ffd->ctx, ffd->dev, dev_fmt, &ffd->options);
	}

	if (res < 0) {
		if (ffd->options) {
			GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] Error %d opening input - retrying without options\n", ffd->fname, res));
			res = avformat_open_input(&ffd->ctx, dev_name, dev_fmt, NULL);
		} else {
			av_dict_set(&ffd->options, "framerate", "30", 0);
			GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] Error %d opening input - retrying with 30 fps\n", ffd->fname, res));
			res = avformat_open_input(&ffd->ctx, dev_name, dev_fmt, &ffd->options);
			if (res < 0) {
				av_dict_set(&ffd->options, "framerate", "25", 0);
				GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] Error %d opening input - retrying with 25 fps\n", ffd->fname, res));
				res = avformat_open_input(&ffd->ctx, dev_name, dev_fmt, &ffd->options);
			}
		}
	}

	if (res < 0) {
		GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] Cannot open device %s:%s\n", ffd->fname, dev_fmt->priv_class->class_name, ffd->dev));
		return -1;
	}
	av_dump_format(ffd->ctx, 0, ffd->dev, 0);
	ffd->raw_data = GF_TRUE;
	ffd->audio_idx = ffd->video_idx = -1;

	res = avformat_find_stream_info(ffd->ctx, ffd->options ? &ffd->options : NULL);

	if (res <0) {
		GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] cannot locate streams - error %s\n", ffd->fname,  av_err2str(res)));
		return GF_NOT_SUPPORTED;
	}

	//check we have the stream we want
	has_a = has_v = GF_FALSE;
	for (i = 0; i < ffd->ctx->nb_streams; i++) {
		switch(ffd->ctx->streams[i]->codec->codec_type) {
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
		GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] No audio stream in input device\n", ffd->fname));
		return GF_NOT_SUPPORTED;
	}
	if (wants_video && !has_v) {
		GF_LOG(GF_LOG_ERROR, ffd->log_class, ("[%s] No video stream in input device\n", ffd->fname));
		return GF_NOT_SUPPORTED;
	}
	ffd->probe_frames = ffd->probes;
	if (has_v && ffd->probes) {
		ffd->probe_times = gf_malloc(sizeof(u64) * ffd->probes);
		memset(ffd->probe_times, 0, sizeof(u64) * ffd->probes);
		//we probe timestamps in either modes because timestamps of first frames are sometimes off
		ffd->probe_frames = 0;
	}

	GF_LOG(GF_LOG_INFO, ffd->log_class, ("[%s] device %s:%s opened - %d streams\n", ffd->fname, dev_fmt->priv_class->class_name, ffd->dev, ffd->ctx->nb_streams));

	ffd->copy_audio = ffd->copy_video = GF_FALSE;
	switch (ffd->copy) {
	case COPY_NO:
		break;
	case COPY_A:
		ffd->copy_audio = GF_TRUE;
		break;
	case COPY_V:
		ffd->copy_video = GF_TRUE;
		break;
	default:
		ffd->copy_audio = ffd->copy_video = GF_TRUE;
		break;
	}
	if (wants_audio && wants_video) {
		if (!ffd->copy_audio || !ffd->copy_video) {
			GF_LOG(GF_LOG_WARNING, ffd->log_class, ("[%s] using muxed capture av:// without copy on %s, this might introduce packet losses due to blocking modes or delayed consumption of the frames. If experiencing problems, either use copy=AV option or consider using two filters video:// and audio://\n", ffd->fname, (!ffd->copy_audio && !ffd->copy_video) ? "audio and video streams" : ffd->copy_video ? "audio stream" : "video stream"));
		}
	}
	return ffdmx_init_common(filter, ffd);
}

static GF_FilterProbeScore ffavin_probe_url(const char *url, const char *mime)
{
	if (!strncmp(url, "video://", 8)) return GF_FPROBE_MAYBE_SUPPORTED;
	if (!strncmp(url, "audio://", 8)) return GF_FPROBE_MAYBE_SUPPORTED;
	if (!strncmp(url, "av://", 5)) return GF_FPROBE_MAYBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}

GF_FilterRegister FFAVInRegister = {
	.name = "ffavin",
	.description = "FFMPEG Audio Video Capture "LIBAVDEVICE_IDENT,
	.private_size = sizeof(GF_FFDemuxCtx),
	.initialize = ffavin_initialize,
	.finalize = ffdmx_finalize,
	.process = ffdmx_process,
	.update_arg = ffdmx_update_arg,
	.probe_url = ffavin_probe_url,
	.process_event = ffdmx_process_event
};


static const GF_FilterArgs FFAVInArgs[] =
{
	{ OFFS(src), "url of device, video:// audio:// or av://", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(fmt), "name of device class. If not set, defaults to first device class. Typical classes ar 'dshow' on windows, 'avfoundation' on OSX, 'video4linux2' or 'x11grab' on linux", GF_PROP_STRING, NULL, NULL, GF_FALSE},
	{ OFFS(dev), "name of device or index of device. Typical names can be the webcam name ('FaceTime HD Camera' on OSX, device name on windows, '/dev/video0' on linux), 'screen-capture-recorder' (see http://screencapturer.sf.net/) on windows, 'Capture screen 0' on OSX (0=first screen), or X display name (eg ':0.0') on linux", GF_PROP_STRING, "0", NULL, GF_FALSE},
	{ OFFS(copy), "Copy raw frames rather instead of sharing them", GF_PROP_UINT, "A", "NO|A|V|AV", GF_FALSE},
	{ OFFS(sclock), "Use system clock (us) instead of device timestamp (for buggy devices)", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(probes), "Probes a given number of video frames before emitting - this usually helps with bad timing of the first frames", GF_PROP_UINT, "10", "0-100", GF_FALSE},


	{ "*", -1, "Any possible args defined for AVInputFormat and AVFormatContext", GF_PROP_UINT, NULL, NULL, GF_FALSE, GF_TRUE},
	{}
};


//number of arguments defined above
const int FFAVIN_STATIC_ARGS = (sizeof (FFAVInArgs) / sizeof (GF_FilterArgs)) - 1;

void ffavin_regfree(GF_FilterSession *session, GF_FilterRegister *reg)
{
	ffmpeg_registry_free(session, reg, FFAVIN_STATIC_ARGS-1);
}

const GF_FilterRegister *ffavin_register(GF_FilterSession *session)
{
	GF_FilterArgs *args;
	u32 i;
	Bool load_meta_filters = session ? GF_TRUE : GF_FALSE;
	ffmpeg_initialize();

	if (!load_meta_filters) {
		FFAVInRegister.args = FFAVInArgs;
		return &FFAVInRegister;
	}

	FFAVInRegister.registry_free = ffavin_regfree;
	args = gf_malloc(sizeof(GF_FilterArgs)*(FFAVIN_STATIC_ARGS+1));
	memset(args, 0, sizeof(GF_FilterArgs)*(FFAVIN_STATIC_ARGS+1));
	for (i=0; i<FFAVIN_STATIC_ARGS; i++)
		args[i] = FFAVInArgs[i];

	FFAVInRegister.args = args;

	ffmpeg_expand_registry(session, &FFAVInRegister, 2);

	return &FFAVInRegister;
}
