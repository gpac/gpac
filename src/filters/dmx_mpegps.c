/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG Program Stream demuxer filter
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
#include <gpac/thread.h>
#include <gpac/list.h>
#include <gpac/bitstream.h>

#ifndef GPAC_DISABLE_MPEG2PS
#include "../media_tools/mpeg2_ps.h"
#include <gpac/media_tools.h>

typedef struct
{
	GF_FilterPid *opid;
	u32 stream_type;
	u32 stream_num;
	Bool in_use;
	u64 last_dts;
} M2PSStream;


typedef struct
{
	GF_FilterPid *ipid;

	const char *src_url;
	mpeg2ps_t *ps;

	Double start_range;
	u64 first_dts;

	u32 nb_playing;
	GF_Fraction duration;
	Bool in_seek;

	GF_List *streams;
	Bool initial_play_done;
} GF_M2PSDmxCtx;

static void m2psdmx_setup(GF_Filter *filter, GF_M2PSDmxCtx *ctx)
{
	u32 i, nb_streams;
	u32 sync_id = 0;
	Double fps;
	GF_Fraction dur;

	dur.den = 1000;
	dur.num = (s32) mpeg2ps_get_max_time_msec(ctx->ps);

	ctx->first_dts = mpeg2ps_get_first_cts(ctx->ps);

	nb_streams = mpeg2ps_get_video_stream_count(ctx->ps);
	for (i=0; i<nb_streams; i++) {
		u32 par;
		u32 id;
		GF_Fraction frac;
		M2PSStream *st = NULL;
		u32 j, count = gf_list_count(ctx->streams);
		for (j=0; j<count; j++) {
			st = gf_list_get(ctx->streams, j);
			if ((st->stream_type==GF_STREAM_VISUAL) && !st->in_use) break;
			st = NULL;
		}
		if (!st) {
			GF_SAFEALLOC(st, M2PSStream);
			st->opid = gf_filter_pid_new(filter);
			st->stream_type = GF_STREAM_VISUAL;
			gf_list_add(ctx->streams, st);
		}
		st->in_use = GF_TRUE;
		st->stream_num = i;
		id = 0x100 | mpeg2ps_get_video_stream_id(ctx->ps, st->stream_num);
		if (!sync_id) sync_id = id;

		gf_filter_pid_set_property(st->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(st->stream_type) );
		switch (mpeg2ps_get_video_stream_type(ctx->ps, st->stream_num)) {
		case MPEG_VIDEO_MPEG1:
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_MPEG1) );
			break;
		case MPEG_VIDEO_MPEG2:
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_MPEG2_MAIN) );
			break;
		default:
			break;
		}
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(90000) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_ID, &PROP_UINT( id) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT( sync_id ) );

		fps = mpeg2ps_get_video_stream_framerate(ctx->ps, i);
		if (fps) {
			gf_media_get_video_timing(fps, &frac.num, &frac.den);
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_FPS, &PROP_FRAC( frac ) );
		}
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_WIDTH, &PROP_UINT( mpeg2ps_get_video_stream_width(ctx->ps, i) ) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_HEIGHT, &PROP_UINT( mpeg2ps_get_video_stream_height(ctx->ps, i) ) );
		par = mpeg2ps_get_video_stream_aspect_ratio(ctx->ps, i);
		if (par) {
			frac.num = par>>16;
			frac.den = (par&0xffff);
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_SAR, &PROP_FRAC( frac ) );
		}
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_DURATION, &PROP_FRAC( dur ) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL( GF_TRUE ) );
		gf_filter_pid_set_property_str(st->opid, "nocts", &PROP_BOOL(GF_TRUE ));

		gf_filter_pid_set_property(st->opid, GF_PROP_PID_BITRATE, &PROP_UINT((u32) mpeg2ps_get_video_stream_bitrate(ctx->ps, i) ) );

		gf_filter_pid_set_property(st->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_SEEK ) );
	}

	nb_streams = mpeg2ps_get_audio_stream_count(ctx->ps);
	for (i=0; i<nb_streams; i++) {
		u32 id;
		M2PSStream *st = NULL;
		u32 j, count = gf_list_count(ctx->streams);

		if (mpeg2ps_get_audio_stream_type(ctx->ps, i) == MPEG_AUDIO_UNKNOWN) {
			continue;
		}

		for (j=0; j<count; j++) {
			st = gf_list_get(ctx->streams, j);
			if ((st->stream_type==GF_STREAM_AUDIO) && !st->in_use) break;
			st = NULL;
		}
		if (!st) {
			GF_SAFEALLOC(st, M2PSStream);
			st->opid = gf_filter_pid_new(filter);
			st->stream_type = GF_STREAM_AUDIO;
			gf_list_add(ctx->streams, st);
		}
		st->in_use = GF_TRUE;
		st->stream_num = i;
		id = 0x100 | mpeg2ps_get_audio_stream_id(ctx->ps, st->stream_num);
		if (!sync_id) sync_id = id;

		gf_filter_pid_set_property(st->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(st->stream_type) );
		switch (mpeg2ps_get_audio_stream_type(ctx->ps, st->stream_num)) {
		case MPEG_AUDIO_MPEG:
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_CODECID, &PROP_UINT( GF_CODECID_MPEG_AUDIO) );
			break;
		case MPEG_AUDIO_AC3:
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_CODECID, &PROP_UINT( GF_CODECID_AC3) );
			break;
		case MPEG_AUDIO_LPCM:
			gf_filter_pid_set_property(st->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_4CC('L','P','C','M') ) );
			break;
		default:
			break;
		}
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT( mpeg2ps_get_audio_stream_sample_freq(ctx->ps, i) ) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT( mpeg2ps_get_audio_stream_channels(ctx->ps, i) ) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_BITRATE, &PROP_UINT( mpeg2ps_get_audio_stream_bitrate(ctx->ps, i) ) );

		gf_filter_pid_set_property(st->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(90000) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_ID, &PROP_UINT( id) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_CLOCK_ID, &PROP_UINT( sync_id ) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_DURATION, &PROP_FRAC( dur ) );
		gf_filter_pid_set_property(st->opid, GF_PROP_PID_UNFRAMED, &PROP_BOOL( GF_TRUE ) );
		gf_filter_pid_set_property_str(st->opid, "nocts", &PROP_BOOL(GF_TRUE ));

		gf_filter_pid_set_property(st->opid, GF_PROP_PID_PLAYBACK_MODE, &PROP_UINT(GF_PLAYBACK_MODE_SEEK ) );
	}
}


GF_Err m2psdmx_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 i;
	const GF_PropertyValue *p;
	GF_M2PSDmxCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		ctx->ipid = NULL;
		while (gf_list_count(ctx->streams) ) {
			M2PSStream *st = gf_list_pop_back(ctx->streams);
			gf_filter_pid_remove(st->opid);
			gf_free(st);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->ipid) {
		GF_FilterEvent fevt;
		ctx->ipid = pid;

		//we work with full file only, send a play event on source to indicate that
		GF_FEVT_INIT(fevt, GF_FEVT_PLAY, pid);
		fevt.play.start_range = 0;
		fevt.base.on_pid = ctx->ipid;
		fevt.play.full_file_only = GF_TRUE;
		gf_filter_pid_send_event(ctx->ipid, &fevt);
	}

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_FILEPATH);
	if (!p) return GF_NOT_SUPPORTED;

	if (ctx->src_url && !strcmp(ctx->src_url, p->value.string)) return GF_OK;

	if (ctx->ps) {
		mpeg2ps_close(ctx->ps);
		for (i=0; i<gf_list_count(ctx->streams); i++) {
			M2PSStream *st = gf_list_get(ctx->streams, i);
			st->in_use = GF_FALSE;
		}
	}
	ctx->ps = NULL;

	ctx->src_url = p->value.string;

	return GF_OK;
}

static Bool m2psdmx_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	u32 i;
	GF_M2PSDmxCtx *ctx = gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		if (ctx->nb_playing && (ctx->start_range ==  evt->play.start_range)) {
			ctx->nb_playing++;
			return GF_TRUE;
		}
		ctx->nb_playing++;
		ctx->start_range = evt->play.start_range;
		if (!ctx->initial_play_done) {
			ctx->initial_play_done = GF_TRUE;
			//seek will not change the current source state, don't send a seek
			if (ctx->start_range<0.5)
				return GF_TRUE;
		}

		for (i=0; i<gf_list_count(ctx->streams); i++) {
			M2PSStream *pss = gf_list_get(ctx->streams, i);
			if (!pss->in_use) continue;

			if (pss->stream_type==GF_STREAM_VISUAL) {
				mpeg2ps_seek_video_frame(ctx->ps, pss->stream_num, (u64) (ctx->start_range*1000));
			} else {
				mpeg2ps_seek_audio_frame(ctx->ps, pss->stream_num, (u64) (ctx->start_range*1000) );
			}
		}
		//cancel event
		return GF_TRUE;

	case GF_FEVT_STOP:
		ctx->nb_playing--;
		//cancel event if not last stream playing
		if (ctx->nb_playing) return GF_TRUE;

		return GF_FALSE;

	case GF_FEVT_SET_SPEED:
		//cancel event
		return GF_TRUE;
	default:
		break;
	}
	//by default don't cancel event - to rework once we have downloading in place
	return GF_FALSE;
}

GF_Err m2psdmx_process(GF_Filter *filter)
{
	GF_M2PSDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	Bool start, end;
	u32 i, count, nb_done;
	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		return GF_OK;
	}
	gf_filter_pck_get_framing(pck, &start, &end);
	if (!end) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	if (!ctx->ps) {
		ctx->ps = mpeg2ps_init(ctx->src_url);
		if (!ctx->ps) {
			GF_Err e = GF_NON_COMPLIANT_BITSTREAM;
			if (! gf_file_exists(ctx->src_url)) e = GF_URL_ERROR;
			gf_filter_setup_failure(filter, e);
			return GF_NOT_SUPPORTED;
		}
		m2psdmx_setup(filter, ctx);
	}
	if (!ctx->nb_playing) return GF_OK;


	nb_done = 0;
	count = gf_list_count(ctx->streams);

	if (ctx->in_seek) {
		u64 seek_to = (u64) (1000*ctx->start_range);
		for (i=0; i<count;i++) {
			M2PSStream *st = gf_list_get(ctx->streams, i);
			if (!st->in_use) continue;
			if (st->stream_type==GF_STREAM_VISUAL) {
				mpeg2ps_seek_video_frame(ctx->ps, st->stream_num, seek_to);
			} else {
				mpeg2ps_seek_audio_frame(ctx->ps, st->stream_num, seek_to);
			}
		}
		ctx->in_seek = GF_FALSE;
	}


	for (i=0; i<count;i++) {
		u8 *buf;
		u32 buf_len;
		u8 *pck_data;
		GF_FilterPacket *dst_pck;
		M2PSStream *st = gf_list_get(ctx->streams, i);
		if (!st->in_use) {
			nb_done++;
			continue;
		}

		if (gf_filter_pid_would_block(st->opid)) continue;

		if (st->stream_type==GF_STREAM_VISUAL) {
			u8 ftype;
			u64 dts, cts;
			u32 res = mpeg2ps_get_video_frame(ctx->ps, st->stream_num, (u8 **) &buf, &buf_len, &ftype, TS_90000, &dts, &cts);
			if (!res) {
				nb_done++;
				continue;
			}

			//bug in some streams, make sure we don't dispatch twice the same ts
			if (st->last_dts == dts) dts++;;
			st->last_dts = dts;

			if ((buf[buf_len - 4] == 0) && (buf[buf_len - 3] == 0) && (buf[buf_len - 2] == 1)) buf_len -= 4;
			dst_pck = gf_filter_pck_new_alloc(st->opid, buf_len, &pck_data);
			memcpy(pck_data, buf, buf_len);
			if (ftype==1) gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);

			if (cts != GF_FILTER_NO_TS) {
				if (dts == GF_FILTER_NO_TS) dts = cts;
				dts -= ctx->first_dts;
				cts -= ctx->first_dts;
				gf_filter_pck_set_dts(dst_pck, dts);
				gf_filter_pck_set_cts(dst_pck, cts);
				gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_FALSE);
			} else {
				gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
			}
			gf_filter_pck_send(dst_pck);
		} else {
			u64 cts;
			u32 res = mpeg2ps_get_audio_frame(ctx->ps, st->stream_num, (u8**)&buf, &buf_len, TS_90000, NULL, &cts);
			if (!res) {
				nb_done++;
				continue;
			}
			dst_pck = gf_filter_pck_new_alloc(st->opid, buf_len, &pck_data);
			memcpy(pck_data, buf, buf_len);
			if (cts != GF_FILTER_NO_TS) {
				cts -= ctx->first_dts;
				gf_filter_pck_set_cts(dst_pck, cts);
				gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_FALSE);
			} else {
				gf_filter_pck_set_framing(dst_pck, GF_FALSE, GF_FALSE);
			}
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);

			gf_filter_pck_send(dst_pck);
		}
	}

	if (nb_done==count) {
		for (i=0; i<count;i++) {
			M2PSStream *st = gf_list_get(ctx->streams, i);
			gf_filter_pid_set_eos(st->opid);
		}
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_EOS;
	}
	return GF_OK;
}

GF_Err m2psdmx_initialize(GF_Filter *filter)
{
	GF_M2PSDmxCtx *ctx = gf_filter_get_udta(filter);
	ctx->streams = gf_list_new();
	return GF_OK;
}

void m2psdmx_finalize(GF_Filter *filter)
{
	GF_M2PSDmxCtx *ctx = gf_filter_get_udta(filter);

	while (gf_list_count(ctx->streams)) {
		M2PSStream *st = gf_list_pop_back(ctx->streams);
		gf_free(st);
	}
	gf_list_del(ctx->streams);
	if (ctx->ps) mpeg2ps_close(ctx->ps);
}

static const char *m2psdmx_probe_data(const u8 *data, u32 size, GF_FilterProbeScore *score)
{
	*score = GF_FPROBE_EXT_MATCH;
	return "mpg|mpeg|vob";
}

static const GF_FilterCapability M2PSDmxCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "video/mpeg|audio/mpeg"),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "mpg|mpeg|vob"),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};



GF_FilterRegister M2PSDmxRegister = {
	.name = "m2psdmx",
	GF_FS_SET_DESCRIPTION("MPEG PS demuxer")
	GF_FS_SET_HELP("This filter demultiplexes MPEG-2 program stream files/data to produce media PIDs and frames.")
	.private_size = sizeof(GF_M2PSDmxCtx),
	.initialize = m2psdmx_initialize,
	.finalize = m2psdmx_finalize,
	SETCAPS(M2PSDmxCaps),
	.configure_pid = m2psdmx_configure_pid,
	.process = m2psdmx_process,
	.process_event = m2psdmx_process_event,
	.probe_data = m2psdmx_probe_data,
	//this filter is not very reliable, prefer ffmpeg when available
	.priority = 255
};

#endif // GPAC_DISABLE_MPEG2PS

const GF_FilterRegister *m2psdmx_register(GF_FilterSession *session)
{
#ifndef GPAC_DISABLE_MPEG2PS
	return &M2PSDmxRegister;
#else
	return NULL;
#endif
}

