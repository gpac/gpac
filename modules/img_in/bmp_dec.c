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

#if defined(WIN32) || defined(_WIN32_WCE) || defined(__SYMBIAN32__)
#else
#include <netinet/in.h>
#endif

typedef struct
{
	u16 ES_ID;
	u32 width, height, out_size, pixel_format;
} BMPDec;

#define BMPCTX()	BMPDec *ctx = (BMPDec *) ((IMGDec *)ifcg->privateStack)->opaque


static GF_Err BMP_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	BMPCTX();
	if (ctx->ES_ID && ctx->ES_ID!=esd->ESID) return GF_NOT_SUPPORTED;
	if (esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;
	ctx->ES_ID = esd->ESID;
	return GF_OK;
}
static GF_Err BMP_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	BMPCTX();
	if (ctx->ES_ID != ES_ID) return GF_BAD_PARAM;
	ctx->ES_ID = ES_ID;
	return GF_OK;
}
static GF_Err BMP_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	BMPCTX();
	switch (capability->CapCode) {
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ctx->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ctx->height;
		break;
	case GF_CODEC_STRIDE:
		capability->cap.valueInt = (ctx->pixel_format == GF_PIXEL_RGB_24) ? 3 : 4;
		capability->cap.valueInt *= ctx->width;
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
static GF_Err BMP_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;
}

static GF_Err BMP_ProcessData(GF_MediaDecoder *ifcg,
                              char *inBuffer, u32 inBufferLength,
                              u16 ES_ID, u32 *CTS,
                              char *outBuffer, u32 *outBufferLength,
                              u8 PaddingBits, u32 mmlevel)
{
	char *pix;
	u32 i, j, irow, in_stride, out_stride, BPP;
	GF_BitStream *bs;
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER fi;

	BMPCTX();
	if (inBufferLength<54) return GF_NON_COMPLIANT_BITSTREAM;
	bs = gf_bs_new(inBuffer, inBufferLength, GF_BITSTREAM_READ);

#if defined(WIN32) || defined(_WIN32_WCE) || defined(__SYMBIAN32__)
	gf_bs_read_data(bs, (char *) &fh, 14);
#else
	fh.bfType = gf_bs_read_u16(bs);
	fh.bfSize = gf_bs_read_u32(bs);
	fh.bfReserved1 = gf_bs_read_u16(bs);
	fh.bfReserved2 = gf_bs_read_u16(bs);
	fh.bfOffBits = gf_bs_read_u32(bs);
	fh.bfOffBits = ntohl(fh.bfOffBits);
#endif
	gf_bs_read_data(bs, (char *) &fi, 40);
	gf_bs_del(bs);

	if ((fi.biCompression != BI_RGB) || (fi.biPlanes!=1)) return GF_NOT_SUPPORTED;
	if ((fi.biBitCount!=24) && (fi.biBitCount!=32)) return GF_NOT_SUPPORTED;

	BPP = (fi.biBitCount==24) ? 3 : 4;
	ctx->width = fi.biWidth;
	ctx->height = fi.biHeight;
	ctx->pixel_format = (fi.biBitCount==24) ? GF_PIXEL_RGB_24 : GF_PIXEL_RGBA;

	/*new cfg, reset*/
	if (ctx->out_size != ctx->width * ctx->height * BPP) {
		ctx->out_size = ctx->width * ctx->height * BPP;
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}
	out_stride = ctx->width*BPP;
	in_stride = out_stride;
	while (in_stride % 4) in_stride++;

	/*read*/
	if (fi.biBitCount==24) {
		for (i=0; i<ctx->height; i++) {
			irow = (ctx->height-1-i)*out_stride;
			pix = inBuffer + fh.bfOffBits + i*in_stride;
			for (j=0; j<out_stride; j+=3) {
				outBuffer[j + irow] = pix[2];
				outBuffer[j+1 + irow] = pix[1];
				outBuffer[j+2 + irow] = pix[0];
				pix += 3;
			}
		}
	} else {
		for (i=0; i<ctx->height; i++) {
			irow = (ctx->height-1-i)*out_stride;
			pix = inBuffer + fh.bfOffBits + i*in_stride;
			for (j=0; j<out_stride; j+=4) {
				outBuffer[j + irow] = pix[2];
				outBuffer[j+1 + irow] = pix[1];
				outBuffer[j+2 + irow] = pix[0];
				outBuffer[j+3 + irow] = pix[3];
				pix += 4;
			}
		}
	}
	*outBufferLength = ctx->out_size;
	return GF_OK;
}

static const char *BMP_GetCodecName(GF_BaseDecoder *dec)
{
	return "BMP Decoder";
}

Bool NewBMPDec(GF_BaseDecoder *ifcd)
{
	IMGDec *wrap = (IMGDec *) ifcd->privateStack;
	BMPDec *dec = (BMPDec *) gf_malloc(sizeof(BMPDec));
	memset(dec, 0, sizeof(BMPDec));
	wrap->opaque = dec;
	wrap->type = DEC_BMP;

	/*setup our own interface*/
	ifcd->AttachStream = BMP_AttachStream;
	ifcd->DetachStream = BMP_DetachStream;
	ifcd->GetCapabilities = BMP_GetCapabilities;
	ifcd->SetCapabilities = BMP_SetCapabilities;
	ifcd->GetName = BMP_GetCodecName;
	((GF_MediaDecoder *)ifcd)->ProcessData = BMP_ProcessData;
	return 1;
}

void DeleteBMPDec(GF_BaseDecoder *ifcg)
{
	BMPCTX();
	gf_free(ctx);
}
