/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
	u16 ES_ID;
	u32 BPP, width, height, out_size, pixel_format;
	u32 aux_type;
} PNGDec;

#define PNGCTX()	PNGDec *ctx = (PNGDec *) ((IMGDec *)ifcg->privateStack)->opaque


static GF_Err PNG_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	u32 i = 0;
	GF_Descriptor *d = NULL;
	PNGCTX();
	if (ctx->ES_ID && ctx->ES_ID!=esd->ESID) return GF_NOT_SUPPORTED;
	ctx->ES_ID = esd->ESID;

	while ((d = gf_list_enum(esd->extensionDescriptors, &i))) {
		if (d->tag == GF_ODF_AUX_VIDEO_DATA) {
			ctx->aux_type = ((GF_AuxVideoDescriptor*)d)->aux_video_type;
			break;
		}
	}
	return GF_OK;
}
static GF_Err PNG_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	PNGCTX();
	if (ctx->ES_ID != ES_ID) return GF_BAD_PARAM;
	ctx->ES_ID = ES_ID;
	return GF_OK;
}
static GF_Err PNG_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	PNGCTX();
	switch (capability->CapCode) {
	case GF_CODEC_WIDTH:
		if (ctx->aux_type==3) {
			capability->cap.valueInt = ctx->width/2;
		} else {
			capability->cap.valueInt = ctx->width;
		}
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
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_PAR:
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 0;
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}
static GF_Err PNG_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;
}


static GF_Err PNG_ProcessData(GF_MediaDecoder *ifcg,
                              char *inBuffer, u32 inBufferLength,
                              u16 ES_ID, u32 *CTS,
                              char *outBuffer, u32 *outBufferLength,
                              u8 PaddingBits, u32 mmlevel)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_Err e;
	PNGCTX();

	e = gf_img_png_dec(inBuffer, inBufferLength, &ctx->width, &ctx->height, &ctx->pixel_format, outBuffer, outBufferLength);

	switch (ctx->pixel_format) {
	case GF_PIXEL_GREYSCALE:
		ctx->BPP = 1;
		break;
	case GF_PIXEL_ALPHAGREY:
		ctx->BPP = 2;
		break;
	case GF_PIXEL_RGB_24:
		ctx->BPP = 3;
		if (ctx->aux_type==3) ctx->pixel_format = GF_PIXEL_RGBS;
		break;
	case GF_PIXEL_RGBA:
	case GF_PIXEL_RGBD:
		ctx->BPP = 4;
		if (ctx->aux_type==1) ctx->pixel_format = GF_PIXEL_RGBD;
		else if (ctx->aux_type==2) ctx->pixel_format = GF_PIXEL_RGBDS;
		else if (ctx->aux_type==3) ctx->pixel_format = GF_PIXEL_RGBAS;
		break;
	}
	ctx->out_size = *outBufferLength;
	return e;
#else
	return GF_NOT_SUPPORTED;
#endif //GPAC_DISABLE_AV_PARSERS
}

static const char *PNG_GetCodecName(GF_BaseDecoder *dec)
{
	return "LibPNG";
}

Bool NewPNGDec(GF_BaseDecoder *ifcd)
{
	IMGDec *wrap = (IMGDec *) ifcd->privateStack;
	PNGDec *dec = (PNGDec *) gf_malloc(sizeof(PNGDec));
	memset(dec, 0, sizeof(PNGDec));
	wrap->opaque = dec;
	wrap->type = DEC_PNG;

	/*setup our own interface*/
	ifcd->AttachStream = PNG_AttachStream;
	ifcd->DetachStream = PNG_DetachStream;
	ifcd->GetCapabilities = PNG_GetCapabilities;
	ifcd->SetCapabilities = PNG_SetCapabilities;
	ifcd->GetName = PNG_GetCodecName;
	((GF_MediaDecoder *)ifcd)->ProcessData = PNG_ProcessData;
	return 1;
}

void DeletePNGDec(GF_BaseDecoder *ifcg)
{
	PNGCTX();
	gf_free(ctx);
}
