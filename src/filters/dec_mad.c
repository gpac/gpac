/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2005-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / codec pack module
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

#ifdef GPAC_HAS_MAD

#include <gpac/constants.h>

#if defined(_WIN32_WCE) || defined(_WIN64) || defined(__SYMBIAN32__)
#ifndef FPM_DEFAULT
#define FPM_DEFAULT
#endif
#endif

#include <mad.h>

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  pragma comment(lib, "libmad")
# endif
#endif


typedef struct
{
	GF_FilterPid *ipid, *opid;
	
	Bool configured;

	u32 sample_rate, num_samples, num_channels;
	u32 timescale;
	u64 last_cts;

	unsigned char *buffer;
	u32 len;

	struct mad_frame frame;
	struct mad_stream stream;
	struct mad_synth synth;

} GF_MADCtx;

static GF_Err maddec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 i, channel_mask=0;
	GF_MADCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) gf_filter_pid_remove(ctx->opid);
		ctx->opid = NULL;
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (ctx->configured) {
		mad_stream_finish(&ctx->stream);
		mad_frame_finish(&ctx->frame);
		mad_synth_finish(&ctx->synth);
	}
	mad_stream_init(&ctx->stream);
	mad_frame_init(&ctx->frame);
	mad_synth_init(&ctx->synth);
	ctx->configured = GF_TRUE;

	/*we need a frame to init, so use default values*/
	ctx->num_samples = 1152;
	ctx->num_channels = 0;
	ctx->sample_rate = 0;
	ctx->ipid = pid;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
		if (p) ctx->sample_rate = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
		if (p) ctx->num_channels = p->value.uint;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_OTI, &PROP_UINT(GPAC_OTI_RAW_MEDIA_STREAM) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16) );

		ctx->buffer = (unsigned char*)gf_malloc(sizeof(char) * 2*MAD_BUFFER_MDLEN);
	}

	if (ctx->sample_rate)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(ctx->sample_rate) );
	if (ctx->sample_rate) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(ctx->num_channels) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT((ctx->num_channels==1) ? GF_AUDIO_CH_FRONT_CENTER : GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT) );
	}

	gf_filter_set_name(filter, "dec_mad:MAD " MAD_VERSION);

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

static GF_Err maddec_finalize(GF_Filter *filter)
{
	GF_MADCtx *ctx = gf_filter_get_udta(filter);

	if (ctx->buffer) gf_free(ctx->buffer);

	if (ctx->configured) {
		mad_stream_finish(&ctx->stream);
		mad_frame_finish(&ctx->frame);
		mad_synth_finish(&ctx->synth);
	}
	return GF_OK;
}

/*from miniMad.c*/
#define MAD_SCALE(ret, s_chan)	\
	chan = s_chan;				\
	chan += (1L << (MAD_F_FRACBITS - 16));		\
	if (chan >= MAD_F_ONE)					\
		chan = MAD_F_ONE - 1;					\
	else if (chan < -MAD_F_ONE)				\
		chan = -MAD_F_ONE;				\
	ret = chan >> (MAD_F_FRACBITS + 1 - 16);		\
 
static GF_Err maddec_process(GF_Filter *filter)
{
	mad_fixed_t *left_ch, *right_ch, chan;
	char *ptr;
	char *data;
	u32 num, in_size, outSize;
	GF_MADCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *dst_pck;
	GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->ipid);

	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid))
			gf_filter_pid_set_eos(ctx->opid);
		return GF_OK;
	}
	data = gf_filter_pck_get_data(pck, &in_size);

	if (ctx->len + in_size > 2*MAD_BUFFER_MDLEN) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[MAD] MAD buffer overflow, truncating\n"));
		in_size = 2*MAD_BUFFER_MDLEN - ctx->len;
	}

	memcpy(ctx->buffer + ctx->len, data, in_size);
	ctx->len += in_size;
	mad_stream_buffer(&ctx->stream, ctx->buffer, ctx->len);

	if (mad_frame_decode(&ctx->frame, &ctx->stream) == -1) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	mad_synth_frame(&ctx->synth, &ctx->frame);

	if ((ctx->sample_rate != ctx->synth.pcm.samplerate) || (ctx->num_channels != ctx->synth.pcm.channels) || (ctx->num_samples != ctx->synth.pcm.length)) {
		ctx->sample_rate = ctx->synth.pcm.samplerate;
		ctx->num_channels = (u8) ctx->synth.pcm.channels;
		ctx->num_samples = ctx->synth.pcm.length;

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(ctx->sample_rate) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(ctx->num_channels) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_UINT((ctx->num_channels==1) ? GF_AUDIO_CH_FRONT_CENTER : GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT) );
	}

	if (ctx->stream.next_frame) {
		ctx->len = (u32) (&ctx->buffer[ctx->len] - ctx->stream.next_frame);
		memmove(ctx->buffer, ctx->stream.next_frame, ctx->len);
	}

	num = ctx->synth.pcm.length;
	left_ch = ctx->synth.pcm.samples[0];
	right_ch = ctx->synth.pcm.samples[1];

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, num*2*ctx->num_channels, &ptr);

	if (pck) {
		ctx->last_cts = gf_filter_pck_get_cts(pck);
		ctx->timescale = gf_filter_pck_get_timescale(pck);
		gf_filter_pck_set_sap(dst_pck, gf_filter_pck_get_sap(pck));

		gf_filter_pid_drop_packet(ctx->ipid);
	}
	gf_filter_pck_set_cts(dst_pck, ctx->last_cts);
	if (ctx->timescale != ctx->sample_rate) {
		u64 dur = num * ctx->timescale;
		dur /= ctx->sample_rate;
		gf_filter_pck_set_duration(dst_pck, dur);
		ctx->last_cts += dur;
	} else {
		gf_filter_pck_set_duration(dst_pck, num);
		ctx->last_cts += num;
	}


	while (num--) {
		s32 rs;
		MAD_SCALE(rs, (*left_ch++) );

		*ptr = (rs >> 0) & 0xff;
		ptr++;
		*ptr = (rs >> 8) & 0xff;
		ptr++;
		outSize += 2;

		if (ctx->num_channels == 2) {
			MAD_SCALE(rs, (*right_ch++) );
			*ptr = (rs >> 0) & 0xff;
			ptr++;
			*ptr = (rs >> 8) & 0xff;
			ptr++;
			outSize += 2;
		}
	}
	gf_filter_pck_send(dst_pck);
	return GF_OK;
}


static const GF_FilterCapability MADInputs[] =
{
	{.code=GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO), .start=GF_TRUE},
	{.code=GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_AUDIO_MPEG2_PART3)},
	{.code=GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO), .start=GF_TRUE},
	{.code=GF_PROP_PID_OTI, PROP_UINT(GPAC_OTI_AUDIO_MPEG1)},

	{}
};

static const GF_FilterCapability MADOutputs[] =
{
	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_AUDIO)},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_RAW_MEDIA_STREAM )},

	{.code= GF_PROP_PID_STREAM_TYPE, PROP_UINT(GF_STREAM_VISUAL), .start=GF_TRUE},
	{.code= GF_PROP_PID_OTI, PROP_UINT( GPAC_OTI_RAW_MEDIA_STREAM )},

	{}
};

GF_FilterRegister MADRegister = {
	.name = "mad",
	.description = "MAD decoder",
	.private_size = sizeof(GF_MADCtx),
	.input_caps = MADInputs,
	.output_caps = MADOutputs,
	.finalize = maddec_finalize,
	.configure_pid = maddec_configure_pid,
	.process = maddec_process,
};

#endif

const GF_FilterRegister *maddec_register(GF_FilterSession *session)
{
#ifdef GPAC_HAS_FAAD
	return &MADRegister;
#else
	return NULL;
#endif
}

