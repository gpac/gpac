/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / image format module
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

#include "img_in.h"
#include <gpac/avparse.h>

typedef struct
{
	/*no support for scalability with JPEG (progressive JPEG to test)*/
	u16 ES_ID;
	u32 BPP, width, height, out_size, pixel_format;
} JPEGDec;

#define JPEGCTX()	JPEGDec *ctx = (JPEGDec *) ((IMGDec *)ifcg->privateStack)->opaque

static GF_Err JPEG_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	JPEGCTX();
	if (ctx->ES_ID && ctx->ES_ID!=esd->ESID) return GF_NOT_SUPPORTED;
	ctx->ES_ID = esd->ESID;
	ctx->BPP = 3;
	return GF_OK;
}
static GF_Err JPEG_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	JPEGCTX();
	if (ctx->ES_ID != ES_ID) return GF_BAD_PARAM;
	ctx->ES_ID = ES_ID;
	return GF_OK;
}
static GF_Err JPEG_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	JPEGCTX();
	switch (capability->CapCode) {
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ctx->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ctx->height;
		break;
	case GF_CODEC_STRIDE:
		capability->cap.valueInt = ctx->width * ctx->BPP;
		break;
	case GF_CODEC_FPS:
		capability->cap.valueFloat = 0;
		break;
	case GF_CODEC_PIXEL_FORMAT:
		capability->cap.valueInt = ctx->pixel_format;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = 	ctx->out_size;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = IMG_CM_SIZE;
		break;
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 4;
		break;
	case GF_CODEC_PAR:
		capability->cap.valueInt = 0;
		break;
	default:
		capability->cap.valueInt = 0;
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}
static GF_Err JPEG_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;
}

static GF_Err JPEG_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
	GF_Err e;
	JPEGCTX();

	e = gf_img_jpeg_dec(inBuffer, inBufferLength, &ctx->width, &ctx->height, &ctx->pixel_format, outBuffer, outBufferLength, ctx->BPP);
	switch (ctx->pixel_format) {
	case GF_PIXEL_GREYSCALE: ctx->BPP = 1; break;
	case GF_PIXEL_RGB_24: ctx->BPP = 3; break;
	}
	ctx->out_size = *outBufferLength;
	return e;
}

static const char *JPEG_GetCodecName(GF_BaseDecoder *dec)
{
	return "JPEG 6b IJG";
}


Bool NewJPEGDec(GF_BaseDecoder *ifcd)
{
	IMGDec *wrap = (IMGDec *) ifcd->privateStack;
	JPEGDec *dec = (JPEGDec *) malloc(sizeof(JPEGDec));
	memset(dec, 0, sizeof(JPEGDec));
	wrap->opaque = dec;
	wrap->type = DEC_JPEG;

	/*setup our own interface*/	
	ifcd->AttachStream = JPEG_AttachStream;
	ifcd->DetachStream = JPEG_DetachStream;
	ifcd->GetCapabilities = JPEG_GetCapabilities;
	ifcd->SetCapabilities = JPEG_SetCapabilities;
	ifcd->GetName = JPEG_GetCodecName;
	((GF_MediaDecoder *)ifcd)->ProcessData = JPEG_ProcessData;
	return 1;
}

void DeleteJPEGDec(GF_BaseDecoder *ifcg)
{
	JPEGCTX();
	free(ctx);
}
