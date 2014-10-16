/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#include <gpac/modules/codec.h>
#include <gpac/avparse.h>
#include <gpac/constants.h>

#include "xvid_wce/xvid.h"

typedef struct
{
	void *codec;
	/*no support for scalability in XVID yet*/
	u16 ES_ID;
	u32 width, height, out_size, pixel_ar;
	u32 cb_size, cb_trig;
	Bool first_frame;
	s32 base_filters;
	Float FPS;
} XVIDDec;

#define XVIDCTX()	XVIDDec *ctx = (XVIDDec *) (ifcg->privateStack)


static GF_Err XVID_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	GF_M4VDecSpecInfo dsi;
	GF_Err e;
	unsigned char *ptr;
	unsigned long pitch;

	XVIDCTX();

	if (ctx->ES_ID && ctx->ES_ID!=esd->ESID) return GF_NOT_SUPPORTED;
	if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) return GF_NON_COMPLIANT_BITSTREAM;

	/*decode DSI*/
	e = gf_m4v_get_config((char *) esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
	if (e) return e;
	if (!dsi.width || !dsi.height) return GF_NON_COMPLIANT_BITSTREAM;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[XviD] Attaching Stream %d - framesize %d x %d\n", esd->ESID, dsi.width, dsi.height ));

	ctx->codec =  InitCodec(dsi.width, dsi.height, GF_4CC('x', 'v', 'i', 'd'));
	if (!ctx->codec) return GF_OUT_OF_MEM;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[XviD] Decoding DecoderSpecificInfo\n"));

	DecodeFrame(ctx->codec, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, ptr, ptr, ptr, pitch);

	/*note that this may be irrelevant when used through systems (FPS is driven by systems CTS)*/
	ctx->FPS = dsi.clock_rate;
	ctx->FPS /= 1000;
	if (!ctx->FPS) ctx->FPS = 30.0f;
	ctx->width = dsi.width;
	ctx->height = dsi.height;
	ctx->pixel_ar = (dsi.par_num<<16) | dsi.par_den;
	ctx->pixel_ar = 0;
	ctx->ES_ID = esd->ESID;
	ctx->first_frame = 1;
	/*output in YV12 only - let the player handle conversion*/
	ctx->out_size = 3 * ctx->width * ctx->height / 2;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[XviD] Decoder setup - output size %d\n", ctx->out_size ));
	return GF_OK;
}
static GF_Err XVID_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	XVIDCTX();
	if (ctx->ES_ID != ES_ID) return GF_BAD_PARAM;
	if (ctx->codec) CloseCodec(ctx->codec);
	ctx->codec = NULL;
	ctx->ES_ID = 0;
	ctx->width = ctx->height = ctx->out_size = 0;
	return GF_OK;
}
static GF_Err XVID_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	XVIDCTX();

	switch (capability->CapCode) {
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ctx->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ctx->height;
		break;
	case GF_CODEC_STRIDE:
		capability->cap.valueInt = ctx->width;
		break;
	case GF_CODEC_FPS:
		capability->cap.valueFloat = ctx->FPS;
		break;
	case GF_CODEC_PAR:
		capability->cap.valueInt = ctx->pixel_ar;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = ctx->out_size;
		break;
	case GF_CODEC_PIXEL_FORMAT:
		capability->cap.valueInt = GF_PIXEL_YV12;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = ctx->cb_trig;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = ctx->cb_size;
		break;
	/*by default we use 4 bytes padding (otherwise it happens that XviD crashes on some videos...)*/
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 32;
		break;
	/*XviD performs frame reordering internally*/
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_WANTS_THREAD:
	{
		const char *sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "XviD", "Threaded");
		capability->cap.valueInt = (sOpt && stricmp(sOpt, "yes")) ? 1 : 0;
	}
	break;
	/*not known at our level...*/
	case GF_CODEC_CU_DURATION:
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}
static GF_Err XVID_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;
}
static GF_Err XVID_ProcessData(GF_MediaDecoder *ifcg,
                               char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 *CTS,
                               char *outBuffer, u32 *outBufferLength,
                               u8 PaddingBits, u32 mmlevel)
{
	unsigned char *pY, *pU, *pV;
	u32 i, uv_w, half_h;
	unsigned long pitch;
	XVIDCTX();

	/*check not using scalabilty*/
	if (ES_ID != ctx->ES_ID) return GF_BAD_PARAM;

	if (*outBufferLength < ctx->out_size) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	if (!DecodeFrame(ctx->codec, inBuffer, inBufferLength, pY, pU, pV, pitch)) {
		*outBufferLength = 0;
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*dispatch nothing if seeking or droping*/
	switch (mmlevel) {
	case GF_CODEC_LEVEL_SEEK:
	case GF_CODEC_LEVEL_DROP:
		*outBufferLength = 0;
		return GF_OK;
	default:
		break;
	}
	*outBufferLength = ctx->out_size;
	for (i=0; i<ctx->height; i++) {
		unsigned char *src = pY + pitch*i;
		char *dst = outBuffer + ctx->width*i;
		memcpy(dst, src, sizeof(char) * ctx->width);
	}
	outBuffer += ctx->width * ctx->height;
	half_h = ctx->height/2;
	uv_w = ctx->width/2;
	for (i=0; i<half_h; i++) {
		unsigned char *src = pU + pitch/2*i;
		char *dst = outBuffer + i*uv_w;
		memcpy(dst, src, sizeof(char) * uv_w);
	}
	outBuffer += ctx->width * ctx->height / 4;
	for (i=0; i<half_h; i++) {
		unsigned char *src = pV + pitch/2*i;
		char *dst = outBuffer + i*uv_w;
		memcpy(dst, src, sizeof(char) * uv_w);
	}

	return GF_OK;
}

static const char *XVID_GetCodecName(GF_BaseDecoder *dec)
{
	return "XviD 1.0 for WinCE";
}

static u32 XVID_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;
	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;
	if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) return GF_CODEC_STREAM_TYPE_SUPPORTED;
	return GF_CODEC_NOT_SUPPORTED;
}



GF_BaseDecoder *NewXVIDDec()
{
	GF_MediaDecoder *ifcd;
	XVIDDec *dec;

	ifcd = (GF_MediaDecoder*) gf_malloc(sizeof(GF_MediaDecoder));
	memset(ifcd, 0, sizeof(GF_MediaDecoder));

	dec = (XVIDDec*) gf_malloc(sizeof(XVIDDec));
	memset(dec, 0, sizeof(XVIDDec));

	dec->cb_size = 4;
	dec->cb_trig = 1;
	/*setup our own interface*/
	ifcd->AttachStream = XVID_AttachStream;
	ifcd->DetachStream = XVID_DetachStream;
	ifcd->GetCapabilities = XVID_GetCapabilities;
	ifcd->SetCapabilities = XVID_SetCapabilities;
	ifcd->GetName = XVID_GetCodecName;
	ifcd->ProcessData = XVID_ProcessData;
	ifcd->CanHandleStream = XVID_CanHandleStream;
	ifcd->privateStack = dec;
	GF_REGISTER_MODULE_INTERFACE(ifcd, GF_MEDIA_DECODER_INTERFACE, "XviD for CE Decoder", "gpac distribution")
	return (GF_BaseDecoder *) ifcd;
}

void DeleteXVIDDec(GF_BaseDecoder *ifcg)
{
	XVIDCTX();
	if (ctx->codec) CloseCodec(ctx->codec);
	gf_free(ctx);
	gf_free(ifcg);
}


#ifdef __cplusplus
extern "C" {
#endif

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_MEDIA_DECODER_INTERFACE,
#endif
		0
	};
	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *) NewXVIDDec();
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE:
		DeleteXVIDDec((GF_BaseDecoder *)ifce);
		break;
	}
}

GPAC_MODULE_STATIC_DECLARATION( xvid_dec_wce )

#ifdef __cplusplus
}
#endif
