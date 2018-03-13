/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / AVI output filter
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
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_AVILIB

#include <gpac/internal/avilib.h>

typedef struct
{
	u32 width, height, pf;
	GF_Fraction fps;
	u32 sr, bps, nb_ch, wfmt, br;
	u32 codec_id;
	u32 format;
	u32 timescale;
	Bool is_video;
	u32 is_open;
	u32 tk_idx;
	GF_FilterPid *pid;
} AVIStream;

typedef struct
{
	//options
	char *dst;
	GF_Fraction fps;
	Bool noraw;


	avi_t *avi_out;
	u32 nb_write;
	GF_List *streams;
	Bool has_video;
	char comp_name[5];

	u64 last_video_time_ms;
	Bool video_is_eos;
} GF_AVIMuxCtx;

static GF_Err avimux_open_close(GF_AVIMuxCtx *ctx, const char *filename, const char *ext, u32 file_idx)
{
	Bool had_file = GF_FALSE;
	if (ctx->avi_out) {
		had_file = GF_TRUE;
		AVI_close(ctx->avi_out);
	}
	ctx->avi_out = NULL;
	if (!filename) return GF_OK;

	if (!strcmp(filename, "std") || !strcmp(filename, "stdout")) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIOut] Writing to stdout not yet implemented\n"));
		return GF_NOT_SUPPORTED;
	}

	ctx->avi_out = AVI_open_output_file((char *) filename);

	if (had_file && ctx->nb_write) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[AVIOut] re-opening in write mode output file %s, content overwrite\n", filename));
	}
	ctx->nb_write = 0;
	if (!ctx->avi_out) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIOut] cannot open output file %s\n", filename));
		return GF_IO_ERR;
	}
	return GF_OK;
}

static GF_Err avimux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	AVIStream *stream;
	GF_FilterEvent evt;
	const GF_PropertyValue *p;
	u32 w, h, sr, bps, nb_ch, pf, codec_id, type, br, timescale, wfmt;
	GF_Fraction fps;
	GF_AVIMuxCtx *ctx = (GF_AVIMuxCtx *) gf_filter_get_udta(filter);

	stream = gf_filter_pid_get_udta(pid);

	if (is_remove) {
		if (!stream) return GF_SERVICE_ERROR;
		gf_list_del_item(ctx->streams, stream);
		gf_free(stream);

		if (!gf_list_count(ctx->streams))
			avimux_open_close(ctx, NULL, NULL, 0);
		return GF_OK;
	}
	gf_filter_pid_check_caps(pid);

	w = h = sr = nb_ch = pf = codec_id = type = timescale = wfmt = 0;
	bps = 16;
	br=128000;
	fps.den = fps.num = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) type = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p) codec_id = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) timescale = p->value.uint;

	if (!codec_id) return GF_NOT_SUPPORTED;
	if ((type!=GF_STREAM_VISUAL) && (type!=GF_STREAM_AUDIO)) return GF_NOT_SUPPORTED;

	if (!stream && (type==GF_STREAM_VISUAL) && ctx->has_video) {
		return GF_REQUIRES_NEW_INSTANCE;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) w = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) h = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
	if (p) fps = p->value.frac;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
	if (p) pf = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) sr = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_BPS);
	if (p) bps = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p) nb_ch = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_BITRATE);
	if (p) br = p->value.uint;

	switch (codec_id) {
	case GF_CODECID_MPEG4_PART2:
		strcpy(ctx->comp_name, "XVID");
		break;
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		strcpy(ctx->comp_name, "h264");
		break;
//	case GF_CODECID_HEVC:
//	case GF_CODECID_LHVC:
//		strcpy(szComp, "hevc");
//		break;
	case GF_CODECID_MPEG_AUDIO:
	case GF_CODECID_MPEG2_PART3:
		wfmt = 0x55;
		break;
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
		wfmt = 0x0000706d;
		break;
	case GF_CODECID_RAW:
		wfmt = WAVE_FORMAT_PCM;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIOut] Unsupported codec %s for AVI  error\n", gf_codecid_name(codec_id) ));
		return GF_NOT_SUPPORTED;
	}

	if (stream) {
		if (!stream->is_open) {
			stream->codec_id = codec_id;
			stream->timescale = timescale;
			stream->width = w;
			stream->height = h;
			stream->pf = pf;
			stream->fps = fps;
			stream->sr = sr;
			stream->bps = bps;
			stream->nb_ch = nb_ch;
			stream->wfmt = wfmt;
			stream->br = br;
			return GF_OK;
		}
		if ((type==GF_STREAM_VISUAL) && (stream->width==w) && (stream->height==h) && (stream->format==pf) && (stream->codec_id==codec_id) && (stream->timescale==timescale) && (stream->fps.num*fps.den == stream->fps.den*fps.num)) {
			return GF_OK;
		}
		else if ((type==GF_STREAM_AUDIO) && (stream->sr==sr) && (stream->nb_ch==nb_ch) && (stream->bps==bps) && (stream->codec_id==codec_id) && (stream->timescale==timescale) ) {
			return GF_OK;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIOut] Stream configuration changed for codec %s, not supported in AVI\n", gf_codecid_name(codec_id) ));

		return GF_NOT_SUPPORTED;
	}
	GF_SAFEALLOC(stream, AVIStream);
	stream->pid = pid;
	gf_filter_pid_set_udta(pid, stream);
	gf_list_add(ctx->streams, stream);
	stream->codec_id = codec_id;
	stream->timescale = timescale;
	if (type==GF_STREAM_VISUAL) {
		ctx->has_video = GF_TRUE;
		stream->width = w;
		stream->height = h;
		stream->pf = pf;
		stream->fps = fps;
		stream->is_video = GF_TRUE;
	} else {
		stream->sr = sr;
		stream->bps = bps;
		stream->nb_ch = nb_ch;
		stream->wfmt = wfmt;
		stream->br = br;
	}
	GF_FEVT_INIT(evt, GF_FEVT_PLAY, pid);
	gf_filter_pid_send_event(pid, &evt);

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

static void avimux_finalize(GF_Filter *filter)
{
	GF_AVIMuxCtx *ctx = (GF_AVIMuxCtx *) gf_filter_get_udta(filter);
	avimux_open_close(ctx, NULL, NULL, 0);
	while (gf_list_count(ctx->streams)) {
		AVIStream *st = gf_list_pop_back(ctx->streams);
		gf_free(st);
	}
	gf_list_del(ctx->streams);
}

static GF_Err avimux_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	s32 res=0;
	u32 i, count, nb_eos;
	AVIStream *video_st=NULL;
	const char *pck_data;
	u32 pck_size;
	GF_AVIMuxCtx *ctx = (GF_AVIMuxCtx *) gf_filter_get_udta(filter);

	//no video in mux, force writing 100ms of audio
	//hack in raw mode, avoids flushing frames after video TS for video dump
	//this should be fixed
	if (!ctx->has_video || (ctx->noraw && ctx->video_is_eos)) ctx->last_video_time_ms += 100;


	//flush all audio
	nb_eos = 0;
	count = gf_list_count(ctx->streams);
	for (i=0; i<count; i++) {
		u64 cts;
		AVIStream *st = gf_list_get(ctx->streams, i);

		if (st->is_video) {
			video_st=st;
			continue;
		}
		if (!ctx->last_video_time_ms) continue;

		while (1) {
			pck = gf_filter_pid_get_packet(st->pid);
			if (!pck) {
				if (gf_filter_pid_is_eos(st->pid))
					nb_eos++;
				break;
			}
			cts = gf_filter_pck_get_cts(pck);
			if (cts==GF_FILTER_NO_TS) {

			} else {
				cts *= 1000;
				cts /= st->timescale;
				if (cts > ctx->last_video_time_ms) break;
			}

			pck_data = gf_filter_pck_get_data(pck, &pck_size);
			if (!st->is_open) {
				st->is_open = 1;
				AVI_set_audio(ctx->avi_out, st->nb_ch, st->sr, st->bps, st->wfmt, st->br);
				st->tk_idx = AVI_get_audio_track(ctx->avi_out);
			} else {
				AVI_set_audio_track(ctx->avi_out, st->tk_idx);
			}
			res = AVI_write_audio(ctx->avi_out, (char *) pck_data, pck_size);
			gf_filter_pid_drop_packet(st->pid);
			if (res<0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIOut] Audio write error %d\n", res));
			}
			ctx->nb_write++;
		}
	}
	//write video
	if (!video_st) return GF_OK;

	pck = gf_filter_pid_get_packet(video_st->pid);

	if (!video_st->is_open) {
		Double fps;
		u32 duration;
		if (!pck) return GF_OK;

		duration = gf_filter_pck_get_duration(pck);
		if (duration) {
			fps = video_st->timescale;
			fps /= duration;
		}
		else if (video_st->fps.num && video_st->fps.den) {
			fps = video_st->fps.num;
			fps /= video_st->fps.den;
		} else {
			fps = ctx->fps.num;
			fps /= ctx->fps.den;
		}
		AVI_set_video(ctx->avi_out, video_st->width, video_st->height, fps, ctx->comp_name);
		video_st->is_open = GF_TRUE;
	}

	if (!pck) {
		if (gf_filter_pid_is_eos(video_st->pid)) {
			nb_eos++;
			ctx->video_is_eos=GF_TRUE;
		}
	} else {
		u64 cts = gf_filter_pck_get_cts(pck);
		int is_rap = gf_filter_pck_get_sap(pck) ? 1 : 0;
		pck_data = gf_filter_pck_get_data(pck, &pck_size);
		if (pck_data) {
			res = AVI_write_frame(ctx->avi_out, (char *) pck_data, pck_size, is_rap);
		} else {
			GF_FilterHWFrame *hwframe = gf_filter_pck_get_hw_frame(pck);
			if (hwframe && hwframe->get_plane) {
				u32 out_stride;
				GF_Err e = hwframe->get_plane(hwframe, 0, (const u8 **) &pck_data, &out_stride);
				if (e==GF_OK) {
					res = AVI_write_frame(ctx->avi_out, (char *) pck_data, video_st->height*out_stride, is_rap);
				}
			}
		}

		if (cts!=GF_FILTER_NO_TS) {
			u32 dur = gf_filter_pck_get_duration(pck);
			cts += dur;
			cts *= 1000;
			cts /= video_st->timescale;
			ctx->last_video_time_ms = cts + 1;
		} else {
			ctx->last_video_time_ms ++;
		}
		gf_filter_pid_drop_packet(video_st->pid);
		if (res<0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIOut] Video write error %d\n", res));
		}
		ctx->nb_write++;
	}

	if (nb_eos==count) {
		if (ctx->avi_out)
			avimux_open_close(ctx, NULL, NULL, 0);
		return GF_EOS;
	}

	return GF_OK;
}

static GF_Err avimux_initialize(GF_Filter *filter);


#define OFFS(_n)	#_n, offsetof(GF_AVIMuxCtx, _n)

static const GF_FilterArgs AVIMuxArgs[] =
{
	{ OFFS(dst), "location of destination file", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(fps), "default framerate if none indicated in stream", GF_PROP_FRACTION, "25/1", NULL, GF_FALSE},
	{ OFFS(noraw), "disables raw output in AVI, only compressed ones allowed", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{}
};

static const GF_FilterCapability AVIMuxInputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_INC_UINT(GF_PROP_PID_PIXFMT, GF_PIXEL_RGB_24),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_INC_UINT(GF_PROP_PID_AUDIO_FORMAT, GF_AUDIO_FMT_S16),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_INC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_INC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
};

static const GF_FilterCapability AVIMuxInputsNoRAW[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_INC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_INC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE),
	{},
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
};


GF_FilterRegister AVIMuxRegister = {
	.name = "avimx",
	.description = "AVI Muxer",
	.private_size = sizeof(GF_AVIMuxCtx),
	.max_extra_pids = -1,
	.args = AVIMuxArgs,
	INCAPS(AVIMuxInputs),
	.initialize = avimux_initialize,
	.finalize = avimux_finalize,
	.configure_pid = avimux_configure_pid,
	.process = avimux_process
};

static GF_Err avimux_initialize(GF_Filter *filter)
{
	GF_AVIMuxCtx *ctx = (GF_AVIMuxCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_OK;
	ctx->streams = gf_list_new();
	if (strnicmp(ctx->dst, "file:/", 6) && strstr(ctx->dst, "://"))  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}
	avimux_open_close(ctx, ctx->dst, NULL, 0);

	if (ctx->noraw) {
		AVIMuxRegister.input_caps = AVIMuxInputsNoRAW;
		AVIMuxRegister.nb_input_caps = sizeof(AVIMuxInputsNoRAW)/sizeof(GF_FilterCapability);
	}
	return GF_OK;
}

#endif

const GF_FilterRegister *avimux_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_AVILIB
	return &AVIMuxRegister;
#else
	return NULL;
#endif

}

