/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / AMR decoder module
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



/*decoder Interface*/
#include <gpac/modules/codec.h>
#include <gpac/modules/service.h>
#include <gpac/constants.h>

#if (defined(WIN32) || defined (_WIN32_WCE)) && defined(GPAC_HAS_AMR_FT)
/*AMR NB*/
#include "amr_api.h"

#ifndef GPAC_HAS_AMR_FT_WB
#define GPAC_HAS_AMR_FT_WB
#endif

#if !defined(__GNUC__)
#pragma comment(lib, "libamrfloat")
#endif

#else

#ifdef GPAC_HAS_AMR_FT
/*AMR WB*/
#include "amr_nb_ft/interf_dec.h"
#endif

#ifdef GPAC_HAS_AMR_FT_WB
/*AMR WB*/
#include "amr_wb_ft/dec_if.h"
#endif

#endif

/*default size in CU of composition memory for audio*/
#define DEFAULT_AUDIO_CM_SIZE			12
/*default critical size in CU of composition memory for audio*/
#define DEFAULT_AUDIO_CM_TRIGGER		4

typedef struct
{
	Bool is_amr_wb;
	u32 sample_rate, out_size, num_samples;
	u8 num_channels;

	/*AMR NB state vars*/
	int *nb_destate;
    void *wb_destate;
} AMRFTDec;

#define AMRFTCTX() AMRFTDec *ctx = (AMRFTDec *) ifcg->privateStack


static GF_Err AMR_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	GF_BitStream *bs;
	u32 packed;
	AMRFTCTX();
	if (esd->dependsOnESID || !esd->decoderConfig->decoderSpecificInfo) return GF_NOT_SUPPORTED;	

	/*AMRWB dec is another module*/
	if (!strnicmp(esd->decoderConfig->decoderSpecificInfo->data, "sawb", 4)) ctx->is_amr_wb = 1;
	else if (!strnicmp(esd->decoderConfig->decoderSpecificInfo->data, "samr", 4) || !strnicmp(esd->decoderConfig->decoderSpecificInfo->data, "amr ", 4)) ctx->is_amr_wb = 0;
	else return GF_NOT_SUPPORTED;


	bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
	gf_bs_read_u32(bs);
	gf_bs_read_u32(bs);
	ctx->num_channels = gf_bs_read_u16(bs);
	gf_bs_read_u16(bs);
	gf_bs_read_u8(bs);
	packed = gf_bs_read_u8(bs);
	gf_bs_del(bs);
	/*max possible frames in a sample are seen in MP4, that's 15*/
	if (!packed) packed = 15;

	if (ctx->is_amr_wb) {
#ifdef GPAC_HAS_AMR_FT_WB
		ctx->wb_destate = D_IF_init();
		if (!ctx->wb_destate) return GF_IO_ERR;
#else
		return GF_NOT_SUPPORTED;
#endif
		ctx->num_samples = 320;
		ctx->sample_rate = 16000;
	} else {
#ifdef GPAC_HAS_AMR_FT
		ctx->nb_destate = Decoder_Interface_init();
		if (!ctx->nb_destate) return GF_IO_ERR;
#else
		return GF_NOT_SUPPORTED;
#endif
		ctx->num_samples = 160;
		ctx->sample_rate = 8000;
	}

	ctx->out_size = packed * 2 * ctx->num_samples * ctx->num_channels;
	return GF_OK;
}

static GF_Err AMR_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	AMRFTCTX();

#ifdef GPAC_HAS_AMR_FT
	if (ctx->nb_destate) Decoder_Interface_exit(ctx->nb_destate);
#endif
	ctx->nb_destate = NULL;
#ifdef GPAC_HAS_AMR_FT_WB
	if (ctx->wb_destate) D_IF_exit(ctx->wb_destate);
#endif
	ctx->wb_destate = NULL;
	ctx->sample_rate = ctx->out_size = ctx->num_samples = 0;
	ctx->num_channels = 0;
	return GF_OK;
}
static GF_Err AMR_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	AMRFTCTX();
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
		capability->cap.valueInt = DEFAULT_AUDIO_CM_TRIGGER;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = DEFAULT_AUDIO_CM_SIZE;
		break;
	/*FIXME: get exact sampling window*/
	case GF_CODEC_CU_DURATION:
		capability->cap.valueInt = ctx->num_samples;
		break;
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 4;
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

static GF_Err AMR_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like SR changing ...) */
	return GF_NOT_SUPPORTED;
}


static GF_Err AMR_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID, u32 *CTS,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
    u32 offset;
    u8 toc, ft;
	AMRFTCTX();

	/*if late or seeking don't decode (each frame is a RAP)*/
	/*	switch (mmlevel) {
	case GF_CODEC_LEVEL_SEEK:
	case GF_CODEC_LEVEL_DROP:
		*outBufferLength = 0;
		return GF_OK;
	default:
		break;
	}
	*/
	if (ctx->out_size > *outBufferLength) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}
	
	*outBufferLength = 0;

    while (inBufferLength) {
		toc = inBuffer[0];
		ft = (toc >> 3) & 0x0F;

		offset = 0;
		if (ctx->is_amr_wb) {
#ifdef GPAC_HAS_AMR_FT_WB
			D_IF_decode(ctx->wb_destate, inBuffer, (s16 *) outBuffer, 0);
			*outBufferLength += 320*2;
			outBuffer += 320*2;
			offset = (u32)GF_AMR_WB_FRAME_SIZE[ft] + 1;
#endif
		} else {
#ifdef GPAC_HAS_AMR_FT
			Decoder_Interface_Decode(ctx->nb_destate, inBuffer, (s16 *) outBuffer, 0);
			*outBufferLength += 160*2;
			outBuffer += 160*2;
			offset = (u32)GF_AMR_FRAME_SIZE[ft] + 1;
#endif
		}
		/*don't complain but...*/
		if (inBufferLength<offset) return GF_OK;
		inBuffer += offset;
		inBufferLength -= offset;
	}
	return GF_OK;
}


static u32 AMR_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	char *dsi;
	/*we handle audio only*/
	if (StreamType!=GF_STREAM_AUDIO) return GF_CODEC_NOT_SUPPORTED;
	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	if (esd->decoderConfig->objectTypeIndication != GPAC_OTI_MEDIA_GENERIC) return GF_CODEC_NOT_SUPPORTED;
	dsi = esd->decoderConfig->decoderSpecificInfo ? esd->decoderConfig->decoderSpecificInfo->data : NULL;
	if (!dsi) return GF_CODEC_NOT_SUPPORTED;
	if (esd->decoderConfig->decoderSpecificInfo->dataLength < 4) return GF_CODEC_NOT_SUPPORTED;

#ifdef GPAC_HAS_AMR_FT
	if (!strnicmp(dsi, "samr", 4) || !strnicmp(dsi, "amr ", 4)) return GF_CODEC_SUPPORTED;
#endif
#ifdef GPAC_HAS_AMR_FT_WB
	if (!strnicmp(dsi, "sawb", 4)) return GF_CODEC_SUPPORTED;
#endif
	return GF_CODEC_NOT_SUPPORTED;
}


static const char *AMR_GetCodecName(GF_BaseDecoder *ifcg)
{
	AMRFTCTX();
	if (ctx->is_amr_wb) return "3GPP Floating-point AMR Wideband";
	return "3GPP Floating-point AMR";
}

GF_MediaDecoder *NewAMRFTDecoder()
{
	AMRFTDec *dec;
	GF_MediaDecoder *ifce;
	GF_SAFEALLOC(ifce , GF_MediaDecoder);
	dec = gf_malloc(sizeof(AMRFTDec));
	memset(dec, 0, sizeof(AMRFTDec));
	ifce->privateStack = dec;
	ifce->CanHandleStream = AMR_CanHandleStream;

	/*setup our own interface*/	
	ifce->AttachStream = AMR_AttachStream;
	ifce->DetachStream = AMR_DetachStream;
	ifce->GetCapabilities = AMR_GetCapabilities;
	ifce->SetCapabilities = AMR_SetCapabilities;
	ifce->ProcessData = AMR_ProcessData;
	ifce->GetName = AMR_GetCodecName;

	GF_REGISTER_MODULE_INTERFACE(ifce, GF_MEDIA_DECODER_INTERFACE, "AMR-FT 3GPP decoder", "gpac distribution");

	return ifce;
}

void DeleteAMRFTDecoder(GF_BaseDecoder *ifcg)
{
	AMRFTCTX();
	gf_free(ctx);
	gf_free(ifcg);
}

/*re-include AMR reader (we coul make it an independant module...)*/
GF_InputService *NewAESReader();
void DeleteAESReader(void *ifce);

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces() 
{
	static u32 si [] = {
		GF_NET_CLIENT_INTERFACE,
		GF_MEDIA_DECODER_INTERFACE,
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	switch (InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE: return (GF_BaseInterface *)NewAMRFTDecoder();
	case GF_NET_CLIENT_INTERFACE: return (GF_BaseInterface *)NewAESReader();
	default: return NULL;
	}
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE: DeleteAMRFTDecoder((GF_BaseDecoder *)ifce); break;
	case GF_NET_CLIENT_INTERFACE:  DeleteAESReader(ifce); break;
	}
}

GPAC_MODULE_STATIC_DELARATION( amr_float )
