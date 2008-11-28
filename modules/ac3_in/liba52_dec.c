/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / AAC reader module
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

#ifdef GPAC_HAS_LIBA52

#include <gpac/modules/codec.h>
#include <gpac/constants.h>

#ifndef uint32_t
#define uint32_t u32
#endif
#ifndef uint8_t
#define uint8_t u8
#endif

#include <a52dec/mm_accel.h>
#include <a52dec/a52.h>


typedef struct
{
	a52_state_t *codec;
    sample_t* samples;

	u32 sample_rate, num_samples, out_size, flags;
	u8 num_channels;
	/*no support for scalability in FAAD yet*/
	u16 ES_ID;
	
	char ch_reorder[16];
} AC3Dec;


#define A52CTX() AC3Dec *ctx = (AC3Dec *) ifcg->privateStack

static GF_Err AC3_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	A52CTX();
	
	if (ctx->ES_ID && ctx->ES_ID!=esd->ESID) return GF_NOT_SUPPORTED;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[A52] Attaching stream %d\n", esd->ESID));

	if (ctx->codec) a52_free(ctx->codec);
	ctx->codec = a52_init(MM_ACCEL_DJBFFT);
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

	ctx->num_channels = 0;
	ctx->sample_rate = 0;
	ctx->out_size = 0;
	ctx->num_samples = 1536;
	ctx->ES_ID = esd->ESID;
	return GF_OK;
}

static GF_Err AC3_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	A52CTX();
	if (ES_ID != ctx->ES_ID) return GF_BAD_PARAM;
	if (ctx->codec) a52_free(ctx->codec);
	ctx->codec = NULL;
	ctx->ES_ID = 0;
	ctx->sample_rate = ctx->out_size = ctx->num_samples = 0;
	ctx->num_channels = 0;
	return GF_OK;
}
static GF_Err AC3_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	A52CTX();
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
		capability->cap.valueInt = 4;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = 12;
		break;
	/*by default AAC access unit lasts num_samples (timescale being sampleRate)*/
	case GF_CODEC_CU_DURATION:
		capability->cap.valueInt = ctx->num_samples;
		break;
	/*to refine, it seems that 4 bytes padding is not enough on all streams ?*/
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 8;
		break;
	case GF_CODEC_CHANNEL_CONFIG:
		capability->cap.valueInt = 0;
		switch (ctx->flags & A52_CHANNEL_MASK) {
		case A52_CHANNEL1:
		case A52_CHANNEL2:
		case A52_MONO:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_CENTER;
			break;
		case A52_CHANNEL:
		case A52_STEREO:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT;
			break;
		case A52_DOLBY:
			break;
		case A52_3F:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT; 
			break;
		case A52_2F1R:
			capability->cap.valueInt = GF_AUDIO_CH_BACK_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT; 
			break;
		case A52_3F1R:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_BACK_CENTER; 
			break;
		case A52_2F2R:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT; 
			break;
		case A52_3F2R:
			capability->cap.valueInt = GF_AUDIO_CH_FRONT_CENTER | GF_AUDIO_CH_FRONT_LEFT | GF_AUDIO_CH_FRONT_RIGHT | GF_AUDIO_CH_BACK_LEFT | GF_AUDIO_CH_BACK_RIGHT; 
			break;
		}
		if (ctx->flags & A52_LFE)
			capability->cap.valueInt |= GF_AUDIO_CH_LFE; 
		break;
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}
static GF_Err AC3_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like SR changing ...) */
	return GF_NOT_SUPPORTED;
}

#if 0
static s8 AC3_GetChannelPos(AC3Dec *ffd, u32 ch_cfg)
{
	u32 i;
	for (i=0; i<ffd->info.channels; i++) {
		switch (ffd->info.channel_position[i]) {
		case FRONT_CHANNEL_CENTER: if (ch_cfg==GF_AUDIO_CH_FRONT_CENTER) return i; break;
		case FRONT_CHANNEL_LEFT: if (ch_cfg==GF_AUDIO_CH_FRONT_LEFT) return i; break;
		case FRONT_CHANNEL_RIGHT: if (ch_cfg==GF_AUDIO_CH_FRONT_RIGHT) return i; break;
		case SIDE_CHANNEL_LEFT: if (ch_cfg==GF_AUDIO_CH_SIDE_LEFT) return i; break;
		case SIDE_CHANNEL_RIGHT: if (ch_cfg==GF_AUDIO_CH_SIDE_RIGHT) return i; break;
		case BACK_CHANNEL_LEFT: if (ch_cfg==GF_AUDIO_CH_BACK_LEFT) return i; break;
		case BACK_CHANNEL_RIGHT: if (ch_cfg==GF_AUDIO_CH_BACK_RIGHT) return i; break;
		case BACK_CHANNEL_CENTER: if (ch_cfg==GF_AUDIO_CH_BACK_CENTER) return i; break;
		case LFE_CHANNEL: if (ch_cfg==GF_AUDIO_CH_LFE) return i; break;
		}
	}
	return -1;
}
#endif

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

static GF_Err AC3_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
    short *out_samples;
	int i, len, bit_rate;
	sample_t level;
	A52CTX();

	/*check not using scalabilty*/
	if (ctx->ES_ID != ES_ID) return GF_BAD_PARAM;

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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[A52] Decoding AU\n"));

	len = a52_syncinfo(inBuffer, &ctx->flags, &ctx->sample_rate, &bit_rate);
	if (!len) return GF_NON_COMPLIANT_BITSTREAM;

	/*init decoder*/
	if (!ctx->out_size) {
		ctx->num_channels = ac3_channels[ctx->flags & 7];
		if (ctx->flags & A52_LFE) ctx->num_channels++;
		ctx->flags |= A52_ADJUST_LEVEL;

		ctx->out_size = ctx->num_channels * sizeof(short) * 1536;
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	level = 1;
	if ( a52_frame(ctx->codec, inBuffer, &ctx->flags, &level, 384)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[A52] Error decoding AU\n" ));
		*outBufferLength = 0;
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	out_samples = (short*)outBuffer;
	for (i=0; i<6; i++) {
		if (a52_block(ctx->codec))
			return GF_NON_COMPLIANT_BITSTREAM;

		float_to_int(ctx->samples, out_samples + i * 256 * ctx->num_channels, ctx->num_channels);
	}

	*outBufferLength = 6 * ctx->num_channels * 256 * sizeof(short);

	return GF_OK;
}

static const char *AC3_GetCodecName(GF_BaseDecoder *ifcg)
{
	return "LIBA52 AC3 Decoder";
}

static Bool AC3_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, u32 ObjectType, char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	/*audio decs*/	
	if (StreamType != GF_STREAM_AUDIO) return 0;
	switch (ObjectType) {
	case 0xA5:
	case 0:
		return 1;
	}
	return 0;
}

GF_BaseDecoder *NewAC3Dec()
{
	GF_MediaDecoder *ifce;
	AC3Dec *dec;

	GF_SAFEALLOC(ifce, GF_MediaDecoder);
	GF_SAFEALLOC(dec, AC3Dec);
	GF_REGISTER_MODULE_INTERFACE(ifce, GF_MEDIA_DECODER_INTERFACE, "LIBA52 AC3 Decoder", "gpac distribution")

	ifce->privateStack = dec;

	/*setup our own interface*/	
	ifce->AttachStream = AC3_AttachStream;
	ifce->DetachStream = AC3_DetachStream;
	ifce->GetCapabilities = AC3_GetCapabilities;
	ifce->SetCapabilities = AC3_SetCapabilities;
	ifce->ProcessData = AC3_ProcessData;
	ifce->CanHandleStream = AC3_CanHandleStream;
	ifce->GetName = AC3_GetCodecName;
	return (GF_BaseDecoder *) ifce;
}

void DeleteAC3Dec(GF_BaseDecoder *ifcg)
{
	A52CTX();
	if (ctx->codec) a52_free(ctx->codec);
	free(ctx);
	free(ifcg);
}


#endif
