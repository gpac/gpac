/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#include <gpac/setup.h>

#ifdef GPAC_HAS_FAAD

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  pragma comment(lib, "libfaad")
# endif
#endif

#include <faad.h>

#include <gpac/modules/codec.h>
#include <gpac/constants.h>
#include <gpac/avparse.h>


typedef struct
{
	faacDecHandle codec;
	faacDecFrameInfo info;
	u32 sample_rate, out_size, num_samples;
	u8 num_channels;
	/*no support for scalability in FAAD yet*/
	u16 ES_ID;
	Bool signal_mc;
	Bool is_sbr;

	char ch_reorder[16];
	GF_ESD *esd;
} FAADDec;


#define FAADCTX() FAADDec *ctx = (FAADDec *) ifcg->privateStack

static GF_Err FAAD_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_Err e;
	GF_M4ADecSpecInfo a_cfg;
#endif
	FAADCTX();

	if (ctx->ES_ID && ctx->ES_ID!=esd->ESID) return GF_NOT_SUPPORTED;
	if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->dataLength) return GF_NON_COMPLIANT_BITSTREAM;

	if (!ctx->esd) {
		ctx->esd = esd;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[FAAD] Attaching stream %d\n", esd->ESID));
	}

	if (ctx->codec) faacDecClose(ctx->codec);
	ctx->codec = faacDecOpen();
	if (!ctx->codec) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FAAD] Error initializing decoder\n"));
		return GF_IO_ERR;
	}

#ifndef GPAC_DISABLE_AV_PARSERS
	e = gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg);
	if (e) return e;
#endif
	if (faacDecInit2(ctx->codec, (unsigned char *)esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, (unsigned long*)&ctx->sample_rate, (u8*)&ctx->num_channels) < 0)
	{
#ifndef GPAC_DISABLE_AV_PARSERS
		s8 res;
		char *dsi, *s_base_object_type;
		u32 dsi_len;
		switch (a_cfg.base_object_type) {
		case GF_M4A_AAC_MAIN:
			s_base_object_type = gf_stringizer(GF_M4A_AAC_MAIN);
			goto base_object_type_error;
		case GF_M4A_AAC_LC:
			s_base_object_type = gf_stringizer(GF_M4A_AAC_LC);
			goto base_object_type_error;
		case GF_M4A_AAC_SSR:
			s_base_object_type = gf_stringizer(GF_M4A_AAC_SSR);
			goto base_object_type_error;
		case GF_M4A_AAC_LTP:
			s_base_object_type = gf_stringizer(GF_M4A_AAC_LTP);
			goto base_object_type_error;
		case GF_M4A_AAC_SBR:
			s_base_object_type = gf_stringizer(GF_M4A_AAC_SBR);
			goto base_object_type_error;
		case GF_M4A_AAC_PS:
			s_base_object_type = gf_stringizer(GF_M4A_AAC_PS);
base_object_type_error: /*error case*/
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[FAAD] Error: unsupported %s format for stream %d - defaulting to AAC LC\n", s_base_object_type, esd->ESID));
		default:
			break;
		}
		a_cfg.base_object_type = GF_M4A_AAC_LC;
		a_cfg.has_sbr = 0;
		a_cfg.nb_chan = a_cfg.nb_chan > 2 ? 1 : a_cfg.nb_chan;

		gf_m4a_write_config(&a_cfg, &dsi, &dsi_len);
		res = faacDecInit2(ctx->codec, (unsigned char *) dsi, dsi_len, (unsigned long *) &ctx->sample_rate, (u8 *) &ctx->num_channels);
		gf_free(dsi);
		if (res < 0)
#endif
		{
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[FAAD] Error when initializing AAC decoder for stream %d\n", esd->ESID));
			return GF_NOT_SUPPORTED;
		}
	}

#ifndef GPAC_DISABLE_AV_PARSERS
	ctx->is_sbr = a_cfg.has_sbr;
#endif
	ctx->num_samples = 1024;
	ctx->out_size = 2 * ctx->num_samples * ctx->num_channels;
	ctx->ES_ID = esd->ESID;
	ctx->signal_mc = ctx->num_channels>2 ? 1 : 0;
	return GF_OK;
}

static GF_Err FAAD_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	FAADCTX();
	if (ES_ID != ctx->ES_ID) return GF_BAD_PARAM;
	if (ctx->codec) faacDecClose(ctx->codec);
	ctx->codec = NULL;
	ctx->ES_ID = 0;
	ctx->sample_rate = ctx->out_size = ctx->num_samples = 0;
	ctx->num_channels = 0;
	return GF_OK;
}
static GF_Err FAAD_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	u32 i;
	FAADCTX();
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
		for (i=0; i<ctx->num_channels; i++) {
			switch (ctx->info.channel_position[i]) {
			case FRONT_CHANNEL_CENTER:
				capability->cap.valueInt |= GF_AUDIO_CH_FRONT_CENTER;
				break;
			case FRONT_CHANNEL_LEFT:
				capability->cap.valueInt |= GF_AUDIO_CH_FRONT_LEFT;
				break;
			case FRONT_CHANNEL_RIGHT:
				capability->cap.valueInt |= GF_AUDIO_CH_FRONT_RIGHT;
				break;
			case SIDE_CHANNEL_LEFT:
				capability->cap.valueInt |= GF_AUDIO_CH_SIDE_LEFT;
				break;
			case SIDE_CHANNEL_RIGHT:
				capability->cap.valueInt |= GF_AUDIO_CH_SIDE_RIGHT;
				break;
			case BACK_CHANNEL_LEFT:
				capability->cap.valueInt |= GF_AUDIO_CH_BACK_LEFT;
				break;
			case BACK_CHANNEL_RIGHT:
				capability->cap.valueInt |= GF_AUDIO_CH_BACK_RIGHT;
				break;
			case BACK_CHANNEL_CENTER:
				capability->cap.valueInt |= GF_AUDIO_CH_BACK_CENTER;
				break;
			case LFE_CHANNEL:
				capability->cap.valueInt |= GF_AUDIO_CH_LFE;
				break;
			default:
				break;
			}
		}
		break;
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}
static GF_Err FAAD_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like SR changing ...) */
	return GF_NOT_SUPPORTED;
}

static s8 FAAD_GetChannelPos(FAADDec *ffd, u32 ch_cfg)
{
	u32 i;
	for (i=0; i<ffd->info.channels; i++) {
		switch (ffd->info.channel_position[i]) {
		case FRONT_CHANNEL_CENTER:
			if (ch_cfg==GF_AUDIO_CH_FRONT_CENTER) return i;
			break;
		case FRONT_CHANNEL_LEFT:
			if (ch_cfg==GF_AUDIO_CH_FRONT_LEFT) return i;
			break;
		case FRONT_CHANNEL_RIGHT:
			if (ch_cfg==GF_AUDIO_CH_FRONT_RIGHT) return i;
			break;
		case SIDE_CHANNEL_LEFT:
			if (ch_cfg==GF_AUDIO_CH_SIDE_LEFT) return i;
			break;
		case SIDE_CHANNEL_RIGHT:
			if (ch_cfg==GF_AUDIO_CH_SIDE_RIGHT) return i;
			break;
		case BACK_CHANNEL_LEFT:
			if (ch_cfg==GF_AUDIO_CH_BACK_LEFT) return i;
			break;
		case BACK_CHANNEL_RIGHT:
			if (ch_cfg==GF_AUDIO_CH_BACK_RIGHT) return i;
			break;
		case BACK_CHANNEL_CENTER:
			if (ch_cfg==GF_AUDIO_CH_BACK_CENTER) return i;
			break;
		case LFE_CHANNEL:
			if (ch_cfg==GF_AUDIO_CH_LFE) return i;
			break;
		}
	}
	return -1;
}

static GF_Err FAAD_ProcessData(GF_MediaDecoder *ifcg,
                               char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 *CTS,
                               char *outBuffer, u32 *outBufferLength,
                               u8 PaddingBits, u32 mmlevel)
{
	void *buffer;
	unsigned short *conv_in, *conv_out;
	u32 i, j;
	FAADCTX();

	/*check not using scalabilty*/
	if (ctx->ES_ID != ES_ID) return GF_BAD_PARAM;

	/*if late or seeking don't decode*/
	switch (mmlevel) {
	case GF_CODEC_LEVEL_SEEK:
//	case GF_CODEC_LEVEL_DROP:
		*outBufferLength = 0;
		return GF_OK;
	default:
		break;
	}

	if (ctx->out_size > *outBufferLength) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[FAAD] Decoding AU\n"));
	buffer = faacDecDecode(ctx->codec, &ctx->info, (unsigned char *) inBuffer, inBufferLength);
	if (ctx->info.error>0) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[FAAD] Error decoding AU %s\n", faacDecGetErrorMessage(ctx->info.error) ));
		*outBufferLength = 0;
		//reinit if error
		FAAD_AttachStream((GF_BaseDecoder *)ifcg, ctx->esd);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (!ctx->info.samples || !buffer || !ctx->info.bytesconsumed) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[FAAD] empty/non complete AU\n"));
		*outBufferLength = 0;
		return GF_OK;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[FAAD] AU decoded\n"));

	/*FAAD froces us to decode a frame to get channel cfg*/
	if (ctx->signal_mc) {
		s32 ch, idx;
		ctx->signal_mc = 0;
		idx = 0;
		/*NOW WATCH OUT!! FAAD may very well decide to output more channels than indicated!!!*/
		ctx->num_channels = ctx->info.channels;

		/*get cfg*/
		ch = FAAD_GetChannelPos(ctx, GF_AUDIO_CH_FRONT_LEFT);
		if (ch>=0) {
			ctx->ch_reorder[idx] = ch;
			idx++;
		}
		ch = FAAD_GetChannelPos(ctx, GF_AUDIO_CH_FRONT_RIGHT);
		if (ch>=0) {
			ctx->ch_reorder[idx] = ch;
			idx++;
		}
		ch = FAAD_GetChannelPos(ctx, GF_AUDIO_CH_FRONT_CENTER);
		if (ch>=0) {
			ctx->ch_reorder[idx] = ch;
			idx++;
		}
		ch = FAAD_GetChannelPos(ctx, GF_AUDIO_CH_LFE);
		if (ch>=0) {
			ctx->ch_reorder[idx] = ch;
			idx++;
		}
		ch = FAAD_GetChannelPos(ctx, GF_AUDIO_CH_BACK_LEFT);
		if (ch>=0) {
			ctx->ch_reorder[idx] = ch;
			idx++;
		}
		ch = FAAD_GetChannelPos(ctx, GF_AUDIO_CH_BACK_RIGHT);
		if (ch>=0) {
			ctx->ch_reorder[idx] = ch;
			idx++;
		}
		ch = FAAD_GetChannelPos(ctx, GF_AUDIO_CH_BACK_CENTER);
		if (ch>=0) {
			ctx->ch_reorder[idx] = ch;
			idx++;
		}
		ch = FAAD_GetChannelPos(ctx, GF_AUDIO_CH_SIDE_LEFT);
		if (ch>=0) {
			ctx->ch_reorder[idx] = ch;
			idx++;
		}
		ch = FAAD_GetChannelPos(ctx, GF_AUDIO_CH_SIDE_RIGHT);
		if (ch>=0) {
			ctx->ch_reorder[idx] = ch;
			idx++;
		}

		*outBufferLength = ctx->out_size;
		if (sizeof(short) * ctx->info.samples > *outBufferLength) {
			*outBufferLength = ctx->out_size = sizeof(short)*ctx->info.samples;
		}
		return GF_BUFFER_TOO_SMALL;
	}
	if (sizeof(short) * ctx->info.samples > *outBufferLength) {
		*outBufferLength = sizeof(short)*ctx->info.samples;
		return GF_BUFFER_TOO_SMALL;
	}

	/*we assume left/right order*/
	if (ctx->num_channels<=2) {
		memcpy(outBuffer, buffer, sizeof(short)* ctx->info.samples);
		*outBufferLength = sizeof(short)*ctx->info.samples;
		return GF_OK;
	}
	conv_in = (unsigned short *) buffer;
	conv_out = (unsigned short *) outBuffer;
	for (i=0; i<ctx->info.samples; i+=ctx->info.channels) {
		for (j=0; j<ctx->info.channels; j++) {
			conv_out[i + j] = conv_in[i + ctx->ch_reorder[j]];
		}
	}
	*outBufferLength = sizeof(short)*ctx->info.samples;
	return GF_OK;
}

static const char *FAAD_GetCodecName(GF_BaseDecoder *ifcg)
{
	FAADCTX();
	if (ctx->is_sbr) return "FAAD2 " FAAD2_VERSION " SBR mode";
	return "FAAD2 " FAAD2_VERSION;
}

static u32 FAAD_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_M4ADecSpecInfo a_cfg;
#endif

	/*audio decs*/
	if (StreamType != GF_STREAM_AUDIO) return GF_CODEC_NOT_SUPPORTED;
	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	switch (esd->decoderConfig->objectTypeIndication) {
	/*MPEG2 aac*/
	case GPAC_OTI_AUDIO_AAC_MPEG2_MP:
	case GPAC_OTI_AUDIO_AAC_MPEG2_LCP:
	case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP:
	/*MPEG4 aac*/
	case GPAC_OTI_AUDIO_AAC_MPEG4:
		break;
	default:
		return GF_CODEC_NOT_SUPPORTED;
	}
	/*this codec currently requires AAC config in ESD*/
	if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) return GF_CODEC_NOT_SUPPORTED;
#ifndef GPAC_DISABLE_AV_PARSERS
	if (gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg) != GF_OK) return GF_CODEC_NOT_SUPPORTED;

	switch (a_cfg.base_object_type) {
	case GF_M4A_AAC_MAIN:
	case GF_M4A_AAC_LC:
	case GF_M4A_AAC_SSR:
	case GF_M4A_AAC_LTP:
	case GF_M4A_AAC_SBR:
		return GF_CODEC_SUPPORTED;
	case GF_M4A_ER_AAC_LC:
	case GF_M4A_ER_AAC_LTP:
	case GF_M4A_ER_AAC_SCALABLE:
	case GF_M4A_ER_AAC_LD:
	case GF_M4A_AAC_PS:
		return GF_CODEC_MAYBE_SUPPORTED;
	default:
		return GF_CODEC_NOT_SUPPORTED;
	}
#endif

	return GF_CODEC_SUPPORTED;
}

GF_BaseDecoder *NewFAADDec()
{
	GF_MediaDecoder *ifce;
	FAADDec *dec;

	GF_SAFEALLOC(ifce, GF_MediaDecoder);
	GF_SAFEALLOC(dec, FAADDec);
	GF_REGISTER_MODULE_INTERFACE(ifce, GF_MEDIA_DECODER_INTERFACE, "FAAD2 Decoder", "gpac distribution")

	ifce->privateStack = dec;

	/*setup our own interface*/
	ifce->AttachStream = FAAD_AttachStream;
	ifce->DetachStream = FAAD_DetachStream;
	ifce->GetCapabilities = FAAD_GetCapabilities;
	ifce->SetCapabilities = FAAD_SetCapabilities;
	ifce->ProcessData = FAAD_ProcessData;
	ifce->CanHandleStream = FAAD_CanHandleStream;
	ifce->GetName = FAAD_GetCodecName;
	return (GF_BaseDecoder *) ifce;
}

void DeleteFAADDec(GF_BaseDecoder *ifcg)
{
	FAADCTX();
	if (ctx->codec) faacDecClose(ctx->codec);
	gf_free(ctx);
	gf_free(ifcg);
}


#endif
