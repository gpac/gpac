/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / AC3 liba52 decoder filter
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

#ifdef GPAC_HAS_LIBA52

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  pragma comment(lib, "liba52")
# endif
#endif


#ifndef uint32_t
#define uint32_t u32
#endif
#ifndef uint8_t
#define uint8_t u8
#endif

#include <a52dec/mm_accel.h>
#include <a52dec/a52.h>

#define AC3_FRAME_SIZE 1536

typedef struct
{
	GF_FilterPid *ipid, *opid;
	
	a52_state_t *codec;
	sample_t* samples;

	u32 sample_rate, flags, bit_rate;
	u8 num_channels;
	u32 channel_mask;
	u64 last_cts;
	u32 timescale;
} GF_A52DecCtx;


static GF_Err a52dec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_A52DecCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) gf_filter_pid_remove(ctx->opid);
		ctx->opid = NULL;
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);
	if (!ctx->opid) {
		u32 flags = MM_ACCEL_DJBFFT;
#if !defined (GPAC_CONFIG_IOS) && !defined (GPAC_CONFIG_ANDROID)
		flags |= MM_ACCEL_X86_MMX | MM_ACCEL_X86_3DNOW | MM_ACCEL_X86_MMXEXT;
#endif
		ctx->codec = a52_init(flags);
		if (!ctx->codec) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[A52] Error initializing decoder\n"));
			return GF_IO_ERR;
		}

		ctx->samples = a52_samples(ctx->codec);
		if (!ctx->samples) {
			a52_free(ctx->codec);
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[A52] Error initializing decoder\n"));
			return GF_IO_ERR;
		}

		ctx->opid = gf_filter_pid_new(filter);
	}
	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLES_PER_FRAME, &PROP_UINT(AC3_FRAME_SIZE) );
	return GF_OK;
}

static void a52dec_finalize(GF_Filter *filter)
{
	GF_A52DecCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->codec) a52_free(ctx->codec);
}


static void a52dec_check_mc_config(GF_A52DecCtx *ctx)
{
	u32 channel_mask = 0;

	switch (ctx->flags & A52_CHANNEL_MASK) {
	case A52_CHANNEL1:
	case A52_CHANNEL2:
	case A52_MONO:
		channel_mask = GF_AUDIO_CH_FRONT_CENTER;
		break;
	case A52_CHANNEL:
	case A52_STEREO:
		channel_mask = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
		break;
	case A52_DOLBY:
		break;
	case A52_3F:
		channel_mask = GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
		break;
	case A52_2F1R:
		channel_mask = GF_AUDIO_CH_BACK_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
		break;
	case A52_3F1R:
		channel_mask = GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_BACK_CENTER;
		break;
	case A52_2F2R:
		channel_mask = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT;
		break;
	case A52_3F2R:
		channel_mask = GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT;
		break;
	}
	if (ctx->flags & A52_LFE)
		channel_mask |= GF_AUDIO_CH_LFE;

	if (ctx->channel_mask != channel_mask) {
		ctx->channel_mask = channel_mask;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT(channel_mask) );
	}
}


/**** the following two functions comes from a52dec */
static GFINLINE s32 blah (s32 i)
{
	if (i > 0x43c07fff)
		return 32767;
	else if (i < 0x43bf8000)
		return -32768;
	return i - 0x43c00000;
}

static GFINLINE void float_to_int (float * _f, s16 *samps, int nchannels)
{
	int i, j, c;
	s32 * f = (s32 *) _f;       // XXX assumes IEEE float format

	j = 0;
	nchannels *= 256;
	for (i = 0; i < 256; i++) {
		for (c = 0; c < nchannels; c += 256)
			samps[j++] = blah (f[i + c]);
	}
}

/**** end */

static const int ac3_channels[8] = {
	2, 1, 2, 3, 3, 4, 4, 5
};

static GF_Err a52dec_process(GF_Filter *filter)
{
	short *out_samples;
	int i, len, bit_rate;
	u32 size;
	const char *data;
	u8 *buffer;
	u32 sample_rate, flags;
	u8 num_channels;
	sample_t level;
	GF_A52DecCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *dst_pck;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);

	if (!pck) {
		if ( gf_filter_pid_is_eos(ctx->ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[A52] Decoding AU\n"));

	data = gf_filter_pck_get_data(pck, &size);
	len = a52_syncinfo((u8 *) data, &flags, &sample_rate, &bit_rate);
	if (!len) return GF_NON_COMPLIANT_BITSTREAM;

	num_channels = ac3_channels[flags & 7];
	if (flags & A52_LFE) num_channels++;
	flags |= A52_ADJUST_LEVEL;
	if ((sample_rate != ctx->sample_rate) || (num_channels != ctx->num_channels) || (ctx->flags != flags) ) {
		ctx->num_channels = num_channels;
		ctx->sample_rate = sample_rate;
		ctx->flags = flags;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(ctx->sample_rate) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(ctx->num_channels) );
		a52dec_check_mc_config(ctx);
	}
	if (ctx->bit_rate != bit_rate) {
		ctx->bit_rate = bit_rate;
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, &PROP_UINT(ctx->bit_rate) );
	}
	level = 1;
	if ( a52_frame(ctx->codec, (u8 *) data, &ctx->flags, &level, 384)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[A52] Error decoding AU\n" ));
		if (pck) gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->num_channels * sizeof(short) * AC3_FRAME_SIZE, &buffer);

	if (pck) {
		ctx->last_cts = gf_filter_pck_get_cts(pck);
		ctx->timescale = gf_filter_pck_get_timescale(pck);
		gf_filter_pck_merge_properties(pck, dst_pck);

		gf_filter_pid_drop_packet(ctx->ipid);
	}
	gf_filter_pck_set_cts(dst_pck, ctx->last_cts);
	if (ctx->timescale != ctx->sample_rate) {
		u64 dur = AC3_FRAME_SIZE * ctx->timescale;
		dur /= ctx->sample_rate;
		gf_filter_pck_set_duration(dst_pck, (u32) dur);
		ctx->last_cts += dur;
	} else {
		gf_filter_pck_set_duration(dst_pck, AC3_FRAME_SIZE);
		ctx->last_cts += AC3_FRAME_SIZE;
	}

	out_samples = (short*)buffer;
	for (i=0; i<6; i++) {
		if (a52_block(ctx->codec))
			return GF_NON_COMPLIANT_BITSTREAM;

		float_to_int(ctx->samples, out_samples + i * 256 * ctx->num_channels, ctx->num_channels);
	}
	gf_filter_pck_send(dst_pck);

	return GF_OK;
}



static const GF_FilterCapability A52DecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AC3),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister A52DecRegister = {
	.name = "a52dec",
	GF_FS_SET_DESCRIPTION("A52 decoder")
	GF_FS_SET_HELP("This filter decodes AC3 streams through a52dec library.")
	.private_size = sizeof(GF_A52DecCtx),
	.priority = 1,
	SETCAPS(A52DecCaps),
	.configure_pid = a52dec_configure_pid,
	.process = a52dec_process,
	.finalize = a52dec_finalize
};

#endif

const GF_FilterRegister *a52dec_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_LIBA52
	return &A52DecRegister;
#else
	return NULL;
#endif
}
