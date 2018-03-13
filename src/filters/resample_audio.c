/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / audio resample filter
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

	//internal
	GF_FilterPid *ipid, *opid;
	GF_AudioMixer *mixer;
	Bool cfg_forced;
	u32 freq, nb_ch, bps, ch_cfg;
	GF_AudioInterface input_ai;
	Bool passthrough;

	const char *data;
	u32 size;
} GF_ResampleCtx;


static char *resample_fetch_frame(void *callback, u32 *size, u32 audio_delay_ms)
{
	GF_ResampleCtx *ctx = (GF_ResampleCtx *) callback;
	assert(ctx->data);
	*size = ctx->size;
	return (char*)ctx->data;
}

static void resample_release_frame(void *callback, u32 nb_bytes)
{
	GF_ResampleCtx *ctx = (GF_ResampleCtx *) callback;
	assert(ctx->data);
	assert(ctx->size==nb_bytes);
//	ctx->size = 0;
//	ctx->data = NULL;
}

static Bool resample_get_config(struct _audiointerface *ai, Bool for_reconf)
{
	return GF_TRUE;
}
static Bool resample_is_muted(void *callback)
{
	return GF_FALSE;
}
static Fixed resample_get_speed(void *callback)
{
	return FIX_ONE;
}
static Bool resample_get_channel_volume(void *callback, Fixed *vol)
{
	u32 i;
	for (i=0; i<GF_AUDIO_MIXER_MAX_CHANNELS; i++) vol[i] = FIX_ONE;
	return GF_FALSE;
}

static GF_Err resample_initialize(GF_Filter *filter)
{
	GF_ResampleCtx *ctx = gf_filter_get_udta(filter);
	ctx->mixer = gf_mixer_new(NULL);
	if (!ctx->mixer) return GF_OUT_OF_MEM;

	ctx->input_ai.callback = ctx;
	ctx->input_ai.FetchFrame = resample_fetch_frame;
	ctx->input_ai.ReleaseFrame = resample_release_frame;
	ctx->input_ai.GetConfig = resample_get_config;
	ctx->input_ai.IsMuted = resample_is_muted;
	ctx->input_ai.GetSpeed = resample_get_speed;
	ctx->input_ai.GetChannelVolume = resample_get_channel_volume;
	return GF_OK;
}

static void resample_finalize(GF_Filter *filter)
{
	GF_ResampleCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->mixer) gf_mixer_del(ctx->mixer);
}


static GF_Err resample_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 sr, nb_ch, bps, ch_cfg;
	GF_ResampleCtx *ctx = gf_filter_get_udta(filter);
	if (is_remove) {
		if (ctx->opid) {
			gf_mixer_remove_input(ctx->mixer, &ctx->input_ai);
			gf_filter_pid_remove(ctx->opid);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_max_buffer(ctx->opid, gf_filter_pid_get_max_buffer(pid) );
	}
	if (!ctx->ipid) {
		ctx->ipid = pid;
		gf_mixer_add_input(ctx->mixer, &ctx->input_ai);
	}

	sr = ctx->freq;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) sr = p->value.uint;
	if (!sr) sr = 44100;

	nb_ch = ctx->nb_ch;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p) nb_ch = p->value.uint;
	if (!nb_ch) nb_ch = 1;

	bps = ctx->bps;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_BPS);
	if (p) bps = p->value.uint;
	if (!bps) bps = 16;

	ch_cfg = ctx->ch_cfg;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
	if (p) ch_cfg = p->value.uint;
	if (!ch_cfg) ch_cfg = (nb_ch==1) ? GF_AUDIO_CH_FRONT_CENTER : (GF_AUDIO_CH_FRONT_LEFT|GF_AUDIO_CH_FRONT_RIGHT);


	if ((sr==ctx->freq) && (nb_ch==ctx->nb_ch) && (bps==ctx->bps) && (ch_cfg==ctx->ch_cfg)) return GF_OK;
	ctx->input_ai.samplerate = sr;
	ctx->input_ai.bps = bps;
	ctx->input_ai.chan = nb_ch;
	ctx->input_ai.ch_cfg = ch_cfg;

	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
	ctx->bps = bps;
	ctx->freq = sr;
	ctx->nb_ch = nb_ch;
	ctx->ch_cfg = ch_cfg;
	gf_mixer_set_config(ctx->mixer, ctx->freq, ctx->nb_ch, ctx->bps, ctx->ch_cfg);
	ctx->passthrough = GF_FALSE;

	if ((ctx->input_ai.samplerate==ctx->freq) && (ctx->input_ai.chan==ctx->nb_ch) && (ctx->input_ai.bps==ctx->bps))
		ctx->passthrough = GF_TRUE;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BPS, &PROP_UINT(bps));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_ch));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT(ch_cfg));
	return GF_OK;
}


static GF_Err resample_process(GF_Filter *filter)
{
	char *output;
	u32 osize, written;
	GF_FilterPacket *pck, *dstpck;
	GF_ResampleCtx *ctx = gf_filter_get_udta(filter);
	if (!ctx->ipid) return GF_OK;

	while (1) {
		pck = gf_filter_pid_get_packet(ctx->ipid);
		if (!pck) {
			if (gf_filter_pid_is_eos(ctx->ipid)) {
				gf_filter_pid_set_eos(ctx->opid);
				return GF_EOS;
			}
			return GF_OK;
		}
		if (ctx->passthrough) {
			gf_filter_pck_forward(pck, ctx->opid);
			gf_filter_pid_drop_packet(ctx->ipid);
			continue;
		}
		ctx->data = gf_filter_pck_get_data(pck, &ctx->size);
		osize = 8 * ctx->size / ctx->input_ai.chan / ctx->input_ai.bps;
		osize *= ctx->nb_ch * ctx->bps / 8;

		dstpck = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
		if (!dstpck) return GF_OK;

		written = gf_mixer_get_output(ctx->mixer, output, osize, 0);
		assert(written==osize);
		gf_filter_pck_merge_properties(pck, dstpck);
		gf_filter_pck_send(dstpck);
		ctx->data = NULL;
		ctx->size = 0;
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	return GF_OK;
}

static GF_Err resample_reconfigure_output(GF_Filter *filter, GF_FilterPid *pid)
{
	u32 sr, nb_ch, bps, ch_cfg;
	const GF_PropertyValue *p;
	GF_ResampleCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->opid != pid) return GF_BAD_PARAM;
		
	sr = ctx->freq;
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) sr = p->value.uint;

	nb_ch = ctx->nb_ch;
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p) nb_ch = p->value.uint;

	bps = ctx->bps;
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_BPS);
	if (p) bps = p->value.uint;

	ch_cfg = ctx->ch_cfg;
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_CHANNEL_LAYOUT);
	if (p) ch_cfg = p->value.uint;

	if ((sr==ctx->freq) && (nb_ch==ctx->nb_ch) && (bps==ctx->bps) && (ch_cfg==ctx->ch_cfg)) return GF_OK;

	ctx->bps = bps;
	ctx->freq = sr;
	ctx->nb_ch = nb_ch;
	ctx->ch_cfg = ch_cfg;
	gf_mixer_set_config(ctx->mixer, ctx->freq, ctx->nb_ch, ctx->bps, ctx->ch_cfg);
	ctx->passthrough = GF_FALSE;

	if ((ctx->input_ai.samplerate==ctx->freq) && (ctx->input_ai.chan==ctx->nb_ch) && (ctx->input_ai.bps==ctx->bps))
		ctx->passthrough = GF_TRUE;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BPS, &PROP_UINT(bps));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_ch));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT(ch_cfg));

	return GF_OK;
}

static const GF_FilterCapability ResamplerInputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_RAW),
};


static const GF_FilterCapability ResamplerOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_ResampleCtx, _n)
static const GF_FilterArgs ResamplerArgs[] =
{
	{}
};

GF_FilterRegister ResamplerRegister = {
	.name = "resample",
	.description = "Audio resampling filter",
	.private_size = sizeof(GF_ResampleCtx),
	.initialize = resample_initialize,
	.finalize = resample_finalize,
	.args = ResamplerArgs,
	INCAPS(ResamplerInputs),
	OUTCAPS(ResamplerOutputs),
	.configure_pid = resample_configure_pid,
	.process = resample_process,
	.reconfigure_output = resample_reconfigure_output,
};


const GF_FilterRegister *resample_register(GF_FilterSession *session)
{
	return &ResamplerRegister;
}
