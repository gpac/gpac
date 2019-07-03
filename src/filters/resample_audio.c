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
	u32 ch, sr, fmt;

	//internal
	GF_FilterPid *ipid, *opid;
	GF_AudioMixer *mixer;
	Bool cfg_forced;
	//output config
	u32 freq, nb_ch, afmt, ch_cfg;
	//source is planar
	Bool src_is_planar;
	GF_AudioInterface input_ai;
	Bool passthrough;

	const char *data;
	u32 size, bytes_consumed;
	Fixed speed;
	GF_FilterPacket *in_pck;
} GF_ResampleCtx;


static u8 *resample_fetch_frame(void *callback, u32 *size, u32 *planar_stride, u32 audio_delay_ms)
{
	u32 sample_offset;
	GF_ResampleCtx *ctx = (GF_ResampleCtx *) callback;
	if (!ctx->data) {
		*size = 0;
		return NULL;
	}
	assert(ctx->data);
	*size = ctx->size - ctx->bytes_consumed;
	sample_offset = ctx->bytes_consumed;
	//planar mode, bytes consummed correspond to all channels, so move frame pointer
	//to first sample non consumed = bytes_consumed/nb_channels
	if (ctx->src_is_planar) {
		*planar_stride = ctx->size / ctx->nb_ch;
		sample_offset /= ctx->nb_ch;
	}
	return (char*)ctx->data + sample_offset;
}

static void resample_release_frame(void *callback, u32 nb_bytes)
{
	GF_ResampleCtx *ctx = (GF_ResampleCtx *) callback;
	ctx->bytes_consumed += nb_bytes;
	assert(ctx->bytes_consumed<=ctx->size);
	if (ctx->bytes_consumed==ctx->size) {
		//trash packet and get a new one
		gf_filter_pid_drop_packet(ctx->ipid);
		ctx->data = NULL;
		ctx->size = ctx->bytes_consumed = 0;
		ctx->in_pck = gf_filter_pid_get_packet(ctx->ipid);
		if (!ctx->in_pck) {
			return;
		}
		ctx->data = gf_filter_pck_get_data(ctx->in_pck, &ctx->size);
		ctx->bytes_consumed = 0;
	}
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
	GF_ResampleCtx *ctx = (GF_ResampleCtx *) callback;
	return ctx->speed;
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
	ctx->speed = FIX_ONE;
	return GF_OK;
}

static void resample_finalize(GF_Filter *filter)
{
	GF_ResampleCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->mixer) gf_mixer_del(ctx->mixer);
	if (ctx->in_pck && ctx->ipid) gf_filter_pid_drop_packet(ctx->ipid);
}


static GF_Err resample_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 sr, nb_ch, ch_cfg, afmt;
	GF_ResampleCtx *ctx = gf_filter_get_udta(filter);
	if (is_remove) {
		if (ctx->opid) {
			gf_mixer_remove_input(ctx->mixer, &ctx->input_ai);
			gf_filter_pid_remove(ctx->opid);
		}
		if (ctx->in_pck) gf_filter_pid_drop_packet(ctx->ipid);
		ctx->in_pck = NULL;
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

	afmt = ctx->afmt;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
	if (p) afmt = p->value.uint;

	ch_cfg = ctx->ch_cfg;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
	if (p) ch_cfg = p->value.uint;
	if (!ch_cfg) ch_cfg = (nb_ch==1) ? GF_AUDIO_CH_FRONT_CENTER : (GF_AUDIO_CH_FRONT_LEFT|GF_AUDIO_CH_FRONT_RIGHT);

	//initial config
	if (!ctx->freq || !ctx->nb_ch || !ctx->afmt) {
		ctx->afmt = ctx->fmt ? ctx->fmt : afmt;
		ctx->freq = ctx->sr ? ctx->sr : sr;
		ctx->nb_ch = ctx->ch ? ctx->ch : nb_ch;
		ctx->ch_cfg = ch_cfg;

		gf_mixer_set_config(ctx->mixer, ctx->freq, ctx->nb_ch, afmt, ctx->ch_cfg);
	}
	//input reconfig
	if ((sr != ctx->input_ai.samplerate) || (nb_ch != ctx->input_ai.chan)
		|| (afmt != ctx->input_ai.afmt) || (ch_cfg != ctx->input_ai.ch_cfg)
		|| (ctx->src_is_planar != gf_audio_fmt_is_planar(afmt))
	) {
		ctx->input_ai.samplerate = sr;
		ctx->input_ai.afmt = afmt;
		ctx->input_ai.chan = nb_ch;
		ctx->input_ai.ch_cfg = ch_cfg;
		ctx->src_is_planar = gf_audio_fmt_is_planar(afmt);
	}

	ctx->passthrough = GF_FALSE;
	gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);

	if ((ctx->input_ai.samplerate==ctx->freq) && (ctx->input_ai.chan==ctx->nb_ch) && (ctx->input_ai.afmt==ctx->afmt) && (ctx->speed==FIX_ONE))
		ctx->passthrough = GF_TRUE;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(ctx->freq));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(ctx->afmt));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(ctx->nb_ch));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT(ctx->ch_cfg));
	return GF_OK;
}


static GF_Err resample_process(GF_Filter *filter)
{
	u8 *output;
	u32 osize, written;
	GF_FilterPacket *dstpck;
	GF_ResampleCtx *ctx = gf_filter_get_udta(filter);
	u32 bps;
	if (!ctx->ipid) return GF_OK;

	bps = gf_audio_fmt_bit_depth(ctx->afmt);

	while (1) {
		if (!ctx->in_pck) {
			ctx->in_pck = gf_filter_pid_get_packet(ctx->ipid);

			if (!ctx->in_pck) {
				if (gf_filter_pid_is_eos(ctx->ipid)) {
					gf_filter_pid_set_eos(ctx->opid);
					return GF_EOS;
				}
				return GF_OK;
			}
			ctx->data = gf_filter_pck_get_data(ctx->in_pck, &ctx->size);
		}

		if (ctx->passthrough) {
			gf_filter_pck_forward(ctx->in_pck, ctx->opid);
			gf_filter_pid_drop_packet(ctx->ipid);
			ctx->in_pck = NULL;
			continue;
		}

		osize = ctx->size * ctx->nb_ch * bps;
		osize /= ctx->input_ai.chan * gf_audio_fmt_bit_depth(ctx->input_ai.afmt);

		dstpck = gf_filter_pck_new_alloc(ctx->opid, osize, &output);
		if (!dstpck) return GF_OK;
		gf_filter_pck_merge_properties(ctx->in_pck, dstpck);

		written = gf_mixer_get_output(ctx->mixer, output, osize, 0);
		if (written != osize) {
			gf_filter_pck_truncate(dstpck, written);
		}
		gf_filter_pck_send(dstpck);

		//still some bytes to use from packet, do not discard
		if (ctx->bytes_consumed<ctx->size) {
			continue;
		}
		//done with this packet
		if (ctx->in_pck) {
			ctx->in_pck = NULL;
			gf_filter_pid_drop_packet(ctx->ipid);
		}
	}
	return GF_OK;
}

static GF_Err resample_reconfigure_output(GF_Filter *filter, GF_FilterPid *pid)
{
	u32 sr, nb_ch, afmt, ch_cfg;
	const GF_PropertyValue *p;
	GF_ResampleCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->opid != pid) return GF_BAD_PARAM;
		
	sr = ctx->freq;
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) sr = p->value.uint;

	nb_ch = ctx->nb_ch;
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p) nb_ch = p->value.uint;

	afmt = ctx->afmt;
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_AUDIO_FORMAT);
	if (p) afmt = p->value.uint;

	ch_cfg = ctx->ch_cfg;
	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_CHANNEL_LAYOUT);
	if (p) ch_cfg = p->value.uint;

	p = gf_filter_pid_caps_query(pid, GF_PROP_PID_AUDIO_SPEED);
	if (p) {
		ctx->speed = FLT2FIX((Float) p->value.number);
		if (ctx->speed<0) ctx->speed = -ctx->speed;
	} else {
		ctx->speed = FIX_ONE;
	}

	if ((sr==ctx->freq) && (nb_ch==ctx->nb_ch) && (afmt==ctx->afmt) && (ch_cfg==ctx->ch_cfg) && (ctx->speed == FIX_ONE) ) return GF_OK;

	ctx->afmt = afmt;
	ctx->freq = sr;
	ctx->nb_ch = nb_ch;
	ctx->ch_cfg = ch_cfg;

	gf_mixer_set_config(ctx->mixer, ctx->freq, ctx->nb_ch, ctx->afmt, ctx->ch_cfg);
	ctx->passthrough = GF_FALSE;

	if ((ctx->input_ai.samplerate==ctx->freq) && (ctx->input_ai.chan==ctx->nb_ch) && (ctx->input_ai.afmt==afmt) && (ctx->speed == FIX_ONE))
		ctx->passthrough = GF_TRUE;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(sr));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(afmt));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(nb_ch));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT(ch_cfg));

	if (ctx->speed > FIX_ONE) {
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_BUFFER_REQ, ctx->ipid);
		evt.buffer_req.max_buffer_us = FIX2INT( ctx->speed * 100000 );
		gf_filter_pid_send_event(ctx->ipid, &evt);
	}

	return GF_OK;
}

static const GF_FilterCapability ResamplerCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_ResampleCtx, _n)
static const GF_FilterArgs ResamplerArgs[] =
{
	{ OFFS(ch), "desired number of output audio channels - 0 for auto", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(sr), "desired sample rate of output audio - 0 for auto", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(fmt), "desired format of output audio - none for auto", GF_PROP_PCMFMT, "none", NULL, 0},
	{0}
};

GF_FilterRegister ResamplerRegister = {
	.name = "resample",
	GF_FS_SET_DESCRIPTION("Audio resampler")
	GF_FS_SET_HELP("This filter resamples raw audio to a target sample rate, number of channels or audio format.")
	.private_size = sizeof(GF_ResampleCtx),
	.initialize = resample_initialize,
	.finalize = resample_finalize,
	.args = ResamplerArgs,
	SETCAPS(ResamplerCaps),
	.configure_pid = resample_configure_pid,
	.process = resample_process,
	.reconfigure_output = resample_reconfigure_output,
};


const GF_FilterRegister *resample_register(GF_FilterSession *session)
{
	return &ResamplerRegister;
}
