/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

#ifdef GPAC_HAS_MAD

#include <gpac/modules/codec.h>
#include <gpac/constants.h>

#if defined(_WIN32_WCE) || defined(__SYMBIAN32__)
#ifndef FPM_DEFAULT 
#define FPM_DEFAULT 
#endif
#endif

#include "mad.h"

typedef struct
{
	Bool configured;

	u32 sample_rate, out_size, num_samples;
	u8 num_channels;
	/*no support for scalability in FAAD yet*/
	u16 ES_ID;
	u32 cb_size, cb_trig;

	unsigned char *buffer;
	u32 len;
	Bool first;


	struct mad_frame frame;
	struct mad_stream stream;
	struct mad_synth synth;

} MADDec;


#define MADCTX() MADDec *ctx = (MADDec *) ifcg->privateStack


static GF_Err MAD_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	MADCTX();
	if (ctx->ES_ID && ctx->ES_ID!=esd->ESID) return GF_NOT_SUPPORTED;

	if (ctx->configured) {
		mad_stream_finish(&ctx->stream);
		mad_frame_finish(&ctx->frame);
		mad_synth_finish(&ctx->synth);
	}
	mad_stream_init(&ctx->stream);
	mad_frame_init(&ctx->frame);
	mad_synth_init(&ctx->synth);
	ctx->configured = 1;
	
	ctx->buffer = malloc(sizeof(char) * 2*MAD_BUFFER_MDLEN);
	
	/*we need a frame to init, so use default values*/
	ctx->num_samples = 1152;
	ctx->num_channels = 0;
	ctx->sample_rate = 0;
	ctx->out_size = 2 * ctx->num_samples * ctx->num_channels;
	ctx->ES_ID = esd->ESID;
	ctx->first = 1;
	return GF_OK;
}

static GF_Err MAD_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	MADCTX();
	if (ES_ID != ctx->ES_ID) return GF_BAD_PARAM;
	ctx->ES_ID = 0;
	if (ctx->buffer) free(ctx->buffer);
	ctx->buffer = NULL;
	ctx->sample_rate = ctx->out_size = ctx->num_samples = 0;
	ctx->num_channels = 0;
	if (ctx->configured) {
		mad_stream_finish(&ctx->stream);
		mad_frame_finish(&ctx->frame);
		mad_synth_finish(&ctx->synth);
	}
	ctx->configured = 0;
	return GF_OK;
}
static GF_Err MAD_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	MADCTX();
	switch (capability->CapCode) {
	/*not tested yet*/
	case GF_CODEC_RESILIENT:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = ctx->out_size;
		break;
	case GF_CODEC_SAMPLERATE:
		capability->cap.valueInt = ctx->sample_rate;
		break;
	case GF_CODEC_NB_CHAN:
		capability->cap.valueInt = ctx->num_channels;
		break;
	case GF_CODEC_BITS_PER_SAMPLE:
		capability->cap.valueInt = 16;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = ctx->cb_trig;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = ctx->cb_size;
		break;
	/*FIXME: get exact sampling window*/
	case GF_CODEC_CU_DURATION:
		capability->cap.valueInt = ctx->num_samples;
		break;
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_CHANNEL_CONFIG:
		if (ctx->num_channels==1) {
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_CENTER;
		} else {
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
		}
		break;
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}

static GF_Err MAD_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	MADCTX();
	switch (capability.CapCode) {
	/*reset storage buffer*/
	case GF_CODEC_WAIT_RAP:
		ctx->first = 1;
		ctx->len = 0;
		if (ctx->configured) {
			mad_stream_finish(&ctx->stream);
			mad_frame_finish(&ctx->frame);
			mad_synth_finish(&ctx->synth);

			mad_stream_finish(&ctx->stream);
			mad_frame_finish(&ctx->frame);
			mad_synth_finish(&ctx->synth);
		}
		return GF_OK;
	default:
		/*return unsupported to avoid confusion by the player (like SR changing ...) */
		return GF_NOT_SUPPORTED;
	}
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

static GF_Err MAD_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
	mad_fixed_t *left_ch, *right_ch, chan;
	char *ptr;
	u32 num, outSize;
	MADCTX();

	/*check not using scalabilty*/
	assert(ctx->ES_ID == ES_ID);

	if (ctx->ES_ID != ES_ID) 
		return GF_BAD_PARAM;

	/*if late or seeking don't decode*/
	switch (mmlevel) {
	case GF_CODEC_LEVEL_SEEK:
	case GF_CODEC_LEVEL_DROP:
		*outBufferLength = 0;
		return GF_OK;
	default:
		break;
	}

	if (ctx->out_size > *outBufferLength) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	if (ctx->first) {
		ctx->first = 0;
		memcpy(ctx->buffer, inBuffer, inBufferLength);
		ctx->len = inBufferLength;
		*outBufferLength = 0;
		return GF_OK;
	}
	memcpy(ctx->buffer + ctx->len, inBuffer, inBufferLength);
	ctx->len += inBufferLength;
	mad_stream_buffer(&ctx->stream, ctx->buffer, ctx->len);

	if (mad_frame_decode(&ctx->frame, &ctx->stream) == -1) {
		memcpy(ctx->buffer, inBuffer, inBufferLength);
		ctx->len = inBufferLength;
		*outBufferLength = 0;
		return GF_NON_COMPLIANT_BITSTREAM;
	}


	/*first cfg, reconfig composition buffer*/
	if (!ctx->sample_rate) {
		mad_synth_frame(&ctx->synth, &ctx->frame);
		ctx->len -= inBufferLength;
		ctx->sample_rate = ctx->synth.pcm.samplerate;
		ctx->num_channels = (u8) ctx->synth.pcm.channels;
		ctx->num_samples = ctx->synth.pcm.length;
		ctx->out_size = 2 * ctx->num_samples * ctx->num_channels;
		*outBufferLength = ctx->out_size;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[MAD] decoder initialized - MP3 sample rate %d - %d channel(s)", ctx->sample_rate, ctx->num_channels));
		return GF_BUFFER_TOO_SMALL;
	}
	
	if (ctx->stream.next_frame) {
		ctx->len = &ctx->buffer[ctx->len] - ctx->stream.next_frame;
	    memmove(ctx->buffer, ctx->stream.next_frame, ctx->len);
	}
	

	mad_synth_frame(&ctx->synth, &ctx->frame);
	num = ctx->synth.pcm.length;
	ptr = (char *) outBuffer;
	left_ch = ctx->synth.pcm.samples[0];
	right_ch = ctx->synth.pcm.samples[1];
	outSize = 0;

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
	*outBufferLength = outSize;
	return GF_OK;
}

static const char *MAD_GetCodecName(GF_BaseDecoder *dec)
{
	return "MAD " \
		MAD_VERSION;
}

static Bool MAD_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, u32 ObjectType, char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	/*audio decs*/	
	if (StreamType != GF_STREAM_AUDIO) return 0;

	switch (ObjectType) {
	/*MPEG1 audio*/
	case 0x69:
	/*MPEG2 audio*/
	case 0x6B:
		return 1;
	/*cap query*/
	case 0:
		return 1;
	}
	return 0;
}

GF_BaseDecoder *NewMADDec()
{
	GF_MediaDecoder *ifce;
	MADDec *dec;
	
	GF_SAFEALLOC(ifce, GF_MediaDecoder);
	GF_SAFEALLOC(dec, MADDec);
	GF_REGISTER_MODULE_INTERFACE(ifce, GF_MEDIA_DECODER_INTERFACE, "MAD Decoder", "gpac distribution")
	ifce->privateStack = dec;

	dec->cb_size = 12;
	dec->cb_trig = 4;

	/*setup our own interface*/	
	ifce->AttachStream = MAD_AttachStream;
	ifce->DetachStream = MAD_DetachStream;
	ifce->GetCapabilities = MAD_GetCapabilities;
	ifce->SetCapabilities = MAD_SetCapabilities;
	ifce->GetName = MAD_GetCodecName;
	ifce->ProcessData = MAD_ProcessData;
	ifce->CanHandleStream = MAD_CanHandleStream;
	return (GF_BaseDecoder *)ifce;
}

void DeleteMADDec(GF_MediaDecoder *ifcg)
{
	MADCTX();
	if (ctx->configured) {
		mad_stream_finish(&ctx->stream);
		mad_frame_finish(&ctx->frame);
		mad_synth_finish(&ctx->synth);
	}
	free(ctx);
	free(ifcg);
}

#endif
