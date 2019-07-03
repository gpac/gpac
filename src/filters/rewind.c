/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / media rewinder filter
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

#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/filters.h>
#include <gpac/internal/compositor_dev.h>

typedef struct
{
	//opts
	u32 rbuffer;

	//internal
	GF_FilterPid *ipid, *opid;
	u32 type;

	Bool passthrough;

	GF_List *frames;

	u32 nb_ch;
	u32 bytes_per_sample;
	Bool is_planar;

	Bool wait_for_next_sap;

} GF_RewindCtx;


static GF_Err rewind_initialize(GF_Filter *filter)
{
	GF_RewindCtx *ctx = gf_filter_get_udta(filter);
	ctx->frames = gf_list_new();
	return GF_OK;
}

static void rewind_finalize(GF_Filter *filter)
{
	GF_RewindCtx *ctx = gf_filter_get_udta(filter);
	gf_list_del(ctx->frames);
}


static GF_Err rewind_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 afmt;
	GF_RewindCtx *ctx = gf_filter_get_udta(filter);
	if (is_remove) {
		if (ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!p) return GF_NOT_SUPPORTED;
	ctx->type = p->value.uint;

	if (ctx->type==GF_STREAM_AUDIO) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		if (p) ctx->nb_ch = p->value.uint;
		if (!ctx->nb_ch) ctx->nb_ch = 1;

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
		if (!p) return GF_NOT_SUPPORTED;

		afmt = p->value.uint;
		ctx->bytes_per_sample = gf_audio_fmt_bit_depth(afmt) * ctx->nb_ch / 8;
		ctx->is_planar = gf_audio_fmt_is_planar(afmt);
	}

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_max_buffer(ctx->opid, gf_filter_pid_get_max_buffer(pid) );
	}
	if (!ctx->ipid) {
		ctx->ipid = pid;
	}
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	return GF_OK;
}

static GF_Err rewind_process_video(GF_RewindCtx *ctx, GF_FilterPacket *pck)
{
	Bool do_flush = GF_FALSE;
	if (pck) {
		//keep a ref on this packet
		gf_filter_pck_ref(&pck);
		//and drop input
		gf_filter_pid_drop_packet(ctx->ipid);
		if (gf_filter_pck_get_sap(pck)) {
			do_flush = GF_TRUE;
			ctx->wait_for_next_sap = GF_FALSE;
		}
		else if (gf_list_count(ctx->frames)>ctx->rbuffer) {
			do_flush = GF_TRUE;
			ctx->wait_for_next_sap = GF_TRUE;
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Rewind] Too many frames in GOP, %d vs %d max allowed, flushing until next SAP\n", gf_list_count(ctx->frames), ctx->rbuffer));
		}
	} else {
		do_flush = GF_TRUE;
	}
	//frame was a SAP, flush all previous frames in reverse order
	if (do_flush) {
		while (1) {
			GF_FilterPacket *frame = gf_list_pop_back(ctx->frames);
			if (!frame) break;
			gf_filter_pck_forward(frame, ctx->opid);
			gf_filter_pck_unref(frame);
		}
	}
	if (pck) {
		//rewind buffer exceeded
		if (ctx->wait_for_next_sap) {
			gf_filter_pck_forward(pck, ctx->opid);
			gf_filter_pck_unref(pck);
		} else {
			gf_list_add(ctx->frames, pck);
		}
	}
	return GF_OK;
}

static GF_Err rewind_process(GF_Filter *filter)
{
	u8 *output;
	const u8 *data;
	u32 size;
	GF_FilterPacket *pck, *dstpck;
	GF_RewindCtx *ctx = gf_filter_get_udta(filter);

	if (!ctx->ipid) return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);

	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->passthrough && (ctx->type == GF_STREAM_VISUAL)) {
				return rewind_process_video(ctx, NULL);
			}
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	if (ctx->passthrough) {
		gf_filter_pck_forward(pck, ctx->opid);
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	if (ctx->type == GF_STREAM_VISUAL) {
		return rewind_process_video(ctx, pck);
	}
	data = gf_filter_pck_get_data(pck, &size);

	dstpck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	if (!dstpck) return GF_OK;
	gf_filter_pck_merge_properties(pck, dstpck);

	if (ctx->is_planar) {
		u32 i, j, nb_samples, planesize, bytes_per_samp;
		nb_samples = size / ctx->bytes_per_sample;
		planesize = nb_samples * ctx->bytes_per_sample / ctx->nb_ch;

		bytes_per_samp = ctx->bytes_per_sample / ctx->nb_ch;
		for (j=0; j<ctx->nb_ch; j++) {
			char *dst = output + j * planesize;
			char *src = (char *) data + j * planesize;

			for (i=0; i<nb_samples; i++) {
				memcpy(dst + i*bytes_per_samp, src + (nb_samples - i - 1)*bytes_per_samp, bytes_per_samp);
			}
		}
	} else {
		u32 i, nb_samples;
		nb_samples = size / ctx->bytes_per_sample;

		for (i=0; i<nb_samples; i++) {
			memcpy(output + i*ctx->bytes_per_sample, data + (nb_samples - i - 1)*ctx->bytes_per_sample, ctx->bytes_per_sample);
		}
	}
	gf_filter_pck_send(dstpck);
	gf_filter_pid_drop_packet(ctx->ipid);
	return GF_OK;
}

static Bool rewind_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_RewindCtx *ctx = gf_filter_get_udta(filter);
	if (evt->base.type==GF_FEVT_PLAY) {
		if (evt->play.speed>0) ctx->passthrough = GF_TRUE;
		else ctx->passthrough = GF_FALSE;
	}
	return GF_FALSE;
}

static const GF_FilterCapability RewinderCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_RewindCtx, _n)
static const GF_FilterArgs RewinderArgs[] =
{
	{ OFFS(rbuffer), "size of video rewind buffer in frames. If more frames than this, flush is performed", GF_PROP_UINT, "100", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister RewinderRegister = {
	.name = "rewind",
	GF_FS_SET_DESCRIPTION("Audio/Video rewinder")
	GF_FS_SET_HELP("This filter reverses audio and video frames in negative playback spped.\nThe filter is in passthrough if speed is positive. Otherwise, it reverts decoded GOPs for video, or revert samples in decoded frame for audio (not really nice for most codecs).")
	.private_size = sizeof(GF_RewindCtx),
	//rewind shall be explicetely loaded
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.initialize = rewind_initialize,
	.finalize = rewind_finalize,
	.args = RewinderArgs,
	SETCAPS(RewinderCaps),
	.configure_pid = rewind_configure_pid,
	.process = rewind_process,
	.process_event = rewind_process_event
};


const GF_FilterRegister *rewind_register(GF_FilterSession *session)
{
	return &RewinderRegister;
}
