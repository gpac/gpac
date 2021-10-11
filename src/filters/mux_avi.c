/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2019
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
	u32 width, height, pf, stride;
	GF_Fraction fps;
	u32 sr, bps, nb_ch, wfmt, br;
	u32 codec_id;
	u32 format;
	u32 timescale;
	Bool is_video, is_raw_vid;
	u32 is_open;
	u32 tk_idx;
	GF_FilterPid *pid;
	Bool suspended;
} AVIStream;

typedef struct
{
	//options
	char *dst;
	GF_Fraction fps;
	Bool noraw;
	u64 opendml_size;


	avi_t *avi_out;
	u32 nb_write;
	GF_List *streams;
	Bool has_video;
	char comp_name[5];

	u64 last_video_time_ms;
	Bool video_is_eos;
	char *buf_tmp;
	u32 buf_alloc;
	Bool in_error;

	u32 cur_file_idx_plus_one;
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

	ctx->avi_out = AVI_open_output_file((char *) filename, ctx->opendml_size);

	if (had_file && ctx->nb_write) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[AVIOut] re-opening in write mode output file %s, content overwrite\n", filename));
	}
	ctx->nb_write = 0;
	if (!ctx->avi_out) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIOut] cannot open output file %s\n", filename));
		ctx->in_error = GF_TRUE;
		return GF_IO_ERR;
	}
	return GF_OK;
}

static GF_Err avimux_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	AVIStream *stream;
	GF_FilterEvent evt;
	const GF_PropertyValue *p;
	u32 w, h, sr, stride, bps, nb_ch, pf, codec_id, type, br, timescale, wfmt;
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

	w = h = sr = nb_ch = pf = codec_id = type = timescale = wfmt = stride = 0;
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
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
	if (p) stride = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) h = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
	if (p) fps = p->value.frac;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
	if (p) pf = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) sr = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
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
	case GF_CODECID_MPEG_AUDIO_L1:
		wfmt = 0x55;
		break;
	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
		wfmt = 0x0000706d;
		break;
	case GF_CODECID_RAW:

		if (type==GF_STREAM_VISUAL) {
			if (pf != GF_PIXEL_BGR) {
				gf_filter_pid_negociate_property(pid, GF_PROP_PID_PIXFMT, &PROP_UINT(GF_PIXEL_BGR));
				return GF_OK;
			}
		} else if (type==GF_STREAM_AUDIO) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
			if (!p || (p->value.uint != GF_AUDIO_FMT_S16) ) {
				gf_filter_pid_negociate_property(pid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16));
				return GF_OK;
			}
			wfmt = WAVE_FORMAT_PCM;
		}
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIOut] Unsupported codec %s for AVI  error\n", gf_codecid_name(codec_id) ));
		return GF_NOT_SUPPORTED;
	}

	if (!stream) {
		GF_SAFEALLOC(stream, AVIStream);
		if (!stream) return GF_OUT_OF_MEM;
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
	} else {
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
			stream->stride = stride;
			stream->is_raw_vid = (codec_id==GF_CODECID_RAW) ? GF_TRUE : GF_FALSE;
			goto check_mx;
		}
		if ((type==GF_STREAM_VISUAL) && (stream->width==w) && (stream->height==h) && (stream->format==pf) && (stream->codec_id==codec_id) && (stream->timescale==timescale) && (stream->fps.num*fps.den == stream->fps.den*fps.num))
		{
			goto check_mx;
		}
		else if ((type==GF_STREAM_AUDIO) && (stream->sr==sr) && (stream->nb_ch==nb_ch) && (stream->bps==bps) && (stream->codec_id==codec_id) && (stream->timescale==timescale) )
		{
			goto check_mx;
		}
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AVIOut] Stream configuration changed for codec %s, not supported in AVI\n", gf_codecid_name(codec_id) ));

		return GF_NOT_SUPPORTED;
	}

check_mx:
	if (!ctx->avi_out) {
		char szPath[GF_MAX_PATH];
		const char *cur_file_suffix=NULL;
		GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(pid)) return GF_EOS;
			return GF_OK;
		}
		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
		ctx->cur_file_idx_plus_one = p ? (p->value.uint + 1) : 1;
		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILESUF);
		if (p && p->value.string) cur_file_suffix = p->value.string;

		gf_filter_pid_resolve_file_template(pid, ctx->dst, szPath, ctx->cur_file_idx_plus_one-1, cur_file_suffix);
		avimux_open_close(ctx, szPath, NULL, 0);
	}

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
	if (ctx->buf_tmp) gf_free(ctx->buf_tmp);
}

static GF_Err avimux_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	s32 res=0;
	const GF_PropertyValue *p;
	u32 i, count, nb_eos, nb_suspended;
	AVIStream *video_st=NULL;
	const char *pck_data;
	u32 pck_size;
	GF_AVIMuxCtx *ctx = (GF_AVIMuxCtx *) gf_filter_get_udta(filter);

	if (ctx->in_error)
		return GF_IO_ERR;

	count = gf_list_count(ctx->streams);
	if (!ctx->avi_out) {
		for (i=0; i<count; i++) {
			AVIStream *st = gf_list_get(ctx->streams, i);
			GF_Err e = avimux_configure_pid(filter, st->pid, GF_FALSE);
			if (e) return e;
		}
		if (!ctx->avi_out)
			return GF_OK;
	}

	//no video in mux, force writing 100ms of audio
	//hack in raw mode, avoids flushing frames after video TS for video dump
	//this should be fixed
	if (!ctx->has_video || (ctx->noraw && ctx->video_is_eos)) ctx->last_video_time_ms += 100;


	//flush all audio
	nb_eos = 0;
	nb_suspended = 0;
	for (i=0; i<count; i++) {
		u64 cts;
		AVIStream *st = gf_list_get(ctx->streams, i);

		if (st->is_video) {
			video_st=st;
			continue;
		}
		if (!ctx->last_video_time_ms) continue;
		if (st->suspended) {
			nb_suspended++;
			continue;
		}

		while (1) {
			pck = gf_filter_pid_get_packet(st->pid);
			if (!pck) {
				if (gf_filter_pid_is_eos(st->pid)) {
					nb_eos++;
					if (st->suspended) {
						nb_suspended++;
					}
				}
				break;
			}

			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
			if (p) {
				if (ctx->cur_file_idx_plus_one == p->value.uint+1) {
				} else if (!st->suspended) {
					st->suspended = GF_TRUE;
					nb_suspended++;
					break;
				}
			}

			cts = gf_filter_pck_get_cts(pck);
			if (cts==GF_FILTER_NO_TS) {

			} else {
				cts = gf_timestamp_rescale(cts, st->timescale, 1000);
				if (!ctx->video_is_eos && (cts > ctx->last_video_time_ms))
				 	break;
			}


			pck_data = gf_filter_pck_get_data(pck, &pck_size);
			if (!st->is_open) {
				st->is_open = 1;
				AVI_set_audio(ctx->avi_out, st->nb_ch, st->sr, 8*st->bps, st->wfmt, st->br);
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
		if (!pck) return GF_OK;

		fps = ctx->fps.num;
		fps /= ctx->fps.den;

		if (video_st->fps.num && video_st->fps.den) {
			fps = video_st->fps.num;
			fps /= video_st->fps.den;
		} else {
			u32 duration = gf_filter_pck_get_duration(pck);
			if (duration) {
				fps = video_st->timescale;
				fps /= duration;
			}
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
		p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENUM);
		if (p) {
			if (ctx->cur_file_idx_plus_one == p->value.uint+1) {
			} else if (!video_st->suspended) {
				video_st->suspended = GF_TRUE;
				nb_suspended++;
				pck = NULL;
			}
		}
	}

	if (pck) {
		u32 out_stride = video_st->stride;
		u64 cts = gf_filter_pck_get_cts(pck);
		int is_rap = gf_filter_pck_get_sap(pck) ? 1 : 0;
		pck_data = gf_filter_pck_get_data(pck, &pck_size);
		if (!pck_data) {
			GF_FilterFrameInterface *frame_ifce = gf_filter_pck_get_frame_interface(pck);
			if (frame_ifce && frame_ifce->get_plane) {
				GF_Err e = frame_ifce->get_plane(frame_ifce, 0, (const u8 **) &pck_data, &out_stride);
				if (e!=GF_OK) {
					pck_data = NULL;
				} else {
					gf_pixel_get_size_info(video_st->pf, video_st->width, video_st->height, &pck_size, &out_stride, NULL, NULL, NULL);
				}
			}
		}
		if (pck_data) {
			//raw RGB, flip
			if (video_st->is_raw_vid) {
				if (ctx->buf_alloc<pck_size) {
					ctx->buf_alloc = pck_size;
					ctx->buf_tmp = gf_realloc(ctx->buf_tmp, pck_size);
				}
				for (i=0; i<video_st->height; i++) {
					memcpy(ctx->buf_tmp + i*out_stride, pck_data + (video_st->height-i-1)*out_stride, out_stride);
				}
				res = AVI_write_frame(ctx->avi_out, (char *) ctx->buf_tmp, pck_size, is_rap);
			} else {
				res = AVI_write_frame(ctx->avi_out, (char *) pck_data, pck_size, is_rap);
			}
		}

		if (cts!=GF_FILTER_NO_TS) {
			u32 dur = gf_filter_pck_get_duration(pck);
			cts += dur;

			cts = gf_timestamp_rescale(cts, video_st->timescale, 1000);
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

	if (nb_suspended && (nb_suspended==count)) {
		avimux_open_close(ctx, NULL, NULL, 0);
		for (i=0; i<count; i++) {
			AVIStream *st = gf_list_get(ctx->streams, i);
			st->is_open = GF_FALSE;
			st->suspended = GF_FALSE;
		}
		ctx->avi_out = NULL;
		return GF_OK;
	}

	if (nb_eos==count) {
		if (ctx->avi_out)
			avimux_open_close(ctx, NULL, NULL, 0);
		return GF_EOS;
	}

	return GF_OK;
}

static GF_FilterProbeScore avimux_probe_url(const char *url, const char *mime)
{
	char *fext = gf_file_ext_start(url);
	if (fext && !stricmp(fext, ".avi")) return GF_FPROBE_FORCE;
	if (mime) {
		if (!stricmp(mime, "video/avi")) return GF_FPROBE_FORCE;
		if (!stricmp(mime, "video/x-avi")) return GF_FPROBE_FORCE;
	}
	return GF_FPROBE_NOT_SUPPORTED;
}

static GF_Err avimux_initialize(GF_Filter *filter);


#define OFFS(_n)	#_n, offsetof(GF_AVIMuxCtx, _n)

static const GF_FilterArgs AVIMuxArgs[] =
{
	{ OFFS(dst), "location of destination file", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(fps), "default framerate if none indicated in stream", GF_PROP_FRACTION, "25/1", NULL, 0},
	{ OFFS(noraw), "disable raw output in AVI, only compressed ones allowed", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(opendml_size), "force opendml format when chunks are larger than this amount (0 means 1.9Gb max size in each riff chunk)", GF_PROP_LUINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability AVIMuxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO_L1),
};

static const GF_FilterCapability AVIMuxCapsNoRAW[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG4),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_MP),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_LCP),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AAC_MPEG2_SSRP),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_PART3),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG_AUDIO_L1),
};


GF_FilterRegister AVIMuxRegister = {
	.name = "avimx",
	GF_FS_SET_DESCRIPTION("AVI muxer")
	GF_FS_SET_HELP("This filter multiplexes raw or compressed audio and video to produce an AVI output.\n"
		"\n"
		"Unlike other multiplexing filters in GPAC, this filter is a sink filter and does not produce any PID to be redirected in the graph.\n"
		"The filter can however use template names for its output, using the first input PID to resolve the final name.\n"
		"The filter watches the property `FileNumber` on incoming packets to create new files.\n"
	)
	.private_size = sizeof(GF_AVIMuxCtx),
	.max_extra_pids = -1,
	.args = AVIMuxArgs,
	SETCAPS(AVIMuxCaps),
	.probe_url = avimux_probe_url,
	.initialize = avimux_initialize,
	.finalize = avimux_finalize,
	.configure_pid = avimux_configure_pid,
	.process = avimux_process
};

static GF_Err avimux_initialize(GF_Filter *filter)
{
	const char *sep;
	GF_AVIMuxCtx *ctx = (GF_AVIMuxCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_OK;
	ctx->streams = gf_list_new();
	if (strnicmp(ctx->dst, "file:/", 6) && strstr(ctx->dst, "://"))  {
		gf_filter_setup_failure(filter, GF_NOT_SUPPORTED);
		return GF_NOT_SUPPORTED;
	}
	if (ctx->noraw) {
		gf_filter_override_caps(filter, AVIMuxCapsNoRAW,  sizeof(AVIMuxCapsNoRAW)/sizeof(GF_FilterCapability) );
	}
	sep = strchr(ctx->dst, '$');
	if (sep && strchr(sep+1, '$')) {
		//using templates, cannot solve final name
	} else {
		avimux_open_close(ctx, ctx->dst, NULL, 0);
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

