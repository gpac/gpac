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


#include <gpac/modules/codec.h>
#include <gpac/avparse.h>
#include <gpac/constants.h>

#include "xvid.h"

#ifndef XVID_DEC_FRAME
#define XVID_DEC_FRAME xvid_dec_frame_t
#define XVID_DEC_PARAM xvid_dec_create_t
#else
#define XVID_USE_OLD_API
#endif

#undef XVID_USE_OLD_API

static Bool xvid_is_init = 0;

typedef struct
{
	void *codec;
	/*no support for scalability in XVID yet*/
	u16 ES_ID;
	u32 width, height, out_size, pixel_ar;
	Bool first_frame;
	s32 base_filters;
	Float FPS;
} XVIDDec;

#define XVIDCTX()	XVIDDec *ctx = (XVIDDec *) ifcg->privateStack


static GF_Err XVID_AttachStream(GF_BaseDecoder *ifcg, u16 ES_ID, unsigned char *decSpecInfo, u32 decSpecInfoSize, u16 DependsOnES_ID, u32 objectTypeIndication, Bool UpStream)
{
	GF_M4VDecSpecInfo dsi;
	GF_Err e;
#ifdef XVID_USE_OLD_API
	XVID_DEC_FRAME frame;
	XVID_DEC_PARAM par;
#else
	xvid_dec_frame_t frame;
	xvid_dec_create_t par;
#endif
	
	XVIDCTX();

	if (ctx->ES_ID && ctx->ES_ID!=ES_ID) return GF_NOT_SUPPORTED;
	if (!decSpecInfoSize || !decSpecInfo) return GF_NON_COMPLIANT_BITSTREAM;
	if (ctx->codec) xvid_decore(ctx->codec, XVID_DEC_DESTROY, NULL, NULL);

	/*decode DSI*/
	e = gf_m4v_get_config(decSpecInfo, decSpecInfoSize, &dsi);
	if (e) return e;
	if (!dsi.width || !dsi.height) return GF_NON_COMPLIANT_BITSTREAM;

	memset(&par, 0, sizeof(par));
	par.width = dsi.width;
	par.height = dsi.height;
	/*note that this may be irrelevant when used through systems (FPS is driven by systems CTS)*/
	ctx->FPS = dsi.clock_rate;
	ctx->FPS /= 1000;
	if (!ctx->FPS) ctx->FPS = 30.0f;
	ctx->pixel_ar = (dsi.par_num<<16) | dsi.par_den;

#ifndef XVID_USE_OLD_API
	par.version = XVID_VERSION;
#endif

	if (xvid_decore(NULL, XVID_DEC_CREATE, &par, NULL) < 0) return GF_NON_COMPLIANT_BITSTREAM;

	ctx->width = par.width;
	ctx->height = par.height;
	ctx->codec = par.handle;

	/*init decoder*/
	memset(&frame, 0, sizeof(frame));
	frame.bitstream = (void *) decSpecInfo;
	frame.length = decSpecInfoSize;
#ifndef XVID_USE_OLD_API
	frame.version = XVID_VERSION;
	xvid_decore(ctx->codec, XVID_DEC_DECODE, &frame, NULL);
#else
	/*don't perform error check, XviD doesn't like DSI only frame ...*/
	xvid_decore(ctx->codec, XVID_DEC_DECODE, &frame, NULL);
#endif

	ctx->ES_ID = ES_ID;
	ctx->first_frame = 1;
	/*output in YV12 only - let the player handle conversion*/
	ctx->out_size = ctx->width * ctx->height * 3 / 2;
	return GF_OK;
}
static GF_Err XVID_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	XVIDCTX();
	if (ctx->ES_ID != ES_ID) return GF_BAD_PARAM;
	if (ctx->codec) xvid_decore(ctx->codec, XVID_DEC_DESTROY, NULL, NULL);
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
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = 4;
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
		unsigned char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		unsigned char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
#ifdef XVID_USE_OLD_API
	XVID_DEC_FRAME frame;
#else
	xvid_dec_frame_t frame;
#endif
	s32 postproc;
	XVIDCTX();

	/*check not using scalabilty*/
	if (ES_ID != ctx->ES_ID) return GF_BAD_PARAM;

	if (*outBufferLength < ctx->out_size) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	memset(&frame, 0, sizeof(frame));
	frame.bitstream = (void *) inBuffer;
	frame.length = inBufferLength;


#ifdef XVID_USE_OLD_API
	frame.colorspace = XVID_CSP_I420;
	frame.stride = ctx->width;
	frame.image = (void *) outBuffer;
#else
	frame.version = XVID_VERSION;
	frame.output.csp = XVID_CSP_I420;
	frame.output.stride[0] = ctx->width;
	frame.output.plane[0] = (void *) outBuffer;
#endif

	/*to check, not convinced yet by results...*/
	postproc = ctx->base_filters;

	switch (mmlevel) {
	case GF_CODEC_LEVEL_SEEK:
	case GF_CODEC_LEVEL_DROP:
		/*turn off all post-proc*/
#ifdef XVID_USE_OLD_API
		postproc &= ~XVID_DEC_DEBLOCKY;
		postproc &= ~XVID_DEC_DEBLOCKUV;
#else
		postproc &= ~XVID_DEBLOCKY;
		postproc &= ~XVID_DEBLOCKUV;
		postproc &= ~XVID_FILMEFFECT;
#endif
		break;
	case GF_CODEC_LEVEL_VERY_LATE:
		/*turn off post-proc*/
#ifdef XVID_USE_OLD_API
		postproc &= ~XVID_DEC_DEBLOCKY;
#else
		postproc &= ~XVID_FILMEFFECT;
		postproc &= ~XVID_DEBLOCKY;
#endif
		break;
	case GF_CODEC_LEVEL_LATE:
#ifdef XVID_USE_OLD_API
		postproc &= ~XVID_DEC_DEBLOCKUV;
#else
		postproc &= ~XVID_DEBLOCKUV;
		postproc &= ~XVID_FILMEFFECT;
#endif
		break;
	}
	postproc = 0;

	/*xvid is a real pain here, it may keep the first I frame and force a 1-frame delay, so we simply
	trick it*/
	if (ctx->first_frame) { outBuffer[0] = 'v'; outBuffer[1] = 'o'; outBuffer[2] = 'i'; outBuffer[3] = 'd'; }

	if (xvid_decore(ctx->codec, XVID_DEC_DECODE, &frame, NULL) < 0) {
		*outBufferLength = 0;
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*dispatch nothing if seeking or droping*/
	switch (mmlevel) {
	case GF_CODEC_LEVEL_SEEK:
	case GF_CODEC_LEVEL_DROP:
		*outBufferLength = 0;
		break;
	default:
		*outBufferLength = ctx->out_size;
		if (ctx->first_frame) {
			ctx->first_frame = 0;
			if ((outBuffer[0] == 'v') && (outBuffer[1] == 'o') && (outBuffer[2] == 'i') && (outBuffer[3] == 'd'))
				*outBufferLength = 0;
		}
		break;
	}

	return GF_OK;
}

static Bool XVID_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, u32 ObjectType, unsigned char *decSpecInfo, u32 decSpecInfoSize, u32 PL)
{
	if (StreamType != GF_STREAM_VISUAL) return 0;
	switch (ObjectType) {
	case 0x20:
		return 1;
	/*cap query*/
	case 0:
		return 1;
	}
	return 0;
}

static const char *XVID_GetCodecName(GF_BaseDecoder *dec)
{
#ifdef XVID_USE_OLD_API
	return "XviD Dev Version";
#else
	return "XviD 1.0";
#endif
}

GF_BaseDecoder *NewXVIDDec()
{
	const char *sOpt;
	GF_MediaDecoder *ifcd;
	XVIDDec *dec;
	
	GF_SAFEALLOC(ifcd, GF_MediaDecoder);
	GF_SAFEALLOC(dec, XVIDDec);
	GF_REGISTER_MODULE_INTERFACE(ifcd, GF_MEDIA_DECODER_INTERFACE, "XviD Decoder", "gpac distribution")

	ifcd->privateStack = dec;

	if (!xvid_is_init) {
#ifdef XVID_USE_OLD_API
		XVID_INIT_PARAM init;
		init.api_version = 0;
		init.core_build = 0;
		/*get info*/
		init.cpu_flags = XVID_CPU_CHKONLY;
		xvid_init(NULL, 0, &init, NULL);
		/*then init*/
		xvid_init(NULL, 0, &init, NULL);
#else
		xvid_gbl_init_t init;
		init.debug = 0;
		init.version = XVID_VERSION;
		init.cpu_flags = 0; /*autodetect*/
		xvid_global(NULL, 0, &init, NULL);
#endif
		xvid_is_init = 1;
	}

	/*get config*/
	dec->base_filters = 0;
	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcd, "XviD", "PostProc");
	if (sOpt) {
#ifndef XVID_USE_OLD_API
		if (strstr(sOpt, "FilmEffect")) dec->base_filters |= XVID_FILMEFFECT;
#endif
		if (strstr(sOpt, "Deblock_Y")) {
#ifdef XVID_USE_OLD_API
			dec->base_filters |= XVID_DEC_DEBLOCKY;
#else
			dec->base_filters |= XVID_DEBLOCKY;
#endif
		}
		if (strstr(sOpt, "Deblock_UV")) {
#ifdef XVID_USE_OLD_API
			dec->base_filters |= XVID_DEC_DEBLOCKUV;
#else
			dec->base_filters |= XVID_DEBLOCKUV;
#endif
		}
	}

	/*setup our own interface*/	
	ifcd->AttachStream = XVID_AttachStream;
	ifcd->DetachStream = XVID_DetachStream;
	ifcd->GetCapabilities = XVID_GetCapabilities;
	ifcd->SetCapabilities = XVID_SetCapabilities;
	ifcd->GetName = XVID_GetCodecName;
	ifcd->CanHandleStream = XVID_CanHandleStream;
	ifcd->ProcessData = XVID_ProcessData;
	return (GF_BaseDecoder *) ifcd;
}

void DeleteXVIDDec(GF_BaseDecoder *ifcg)
{
	XVIDCTX();
	if (ctx->codec) xvid_decore(ctx->codec, XVID_DEC_DESTROY, NULL, NULL);
	free(ctx);
	free(ifcg);
}


Bool QueryInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return 1;
	return 0;
}

GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewXVIDDec();
	return NULL;
}

void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
	case GF_MEDIA_DECODER_INTERFACE: 
		DeleteXVIDDec((GF_BaseDecoder*)ifce);
		break;
	}
}
