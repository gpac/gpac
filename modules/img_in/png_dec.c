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

#ifdef GPAC_HAS_PNG

#include "png.h"


typedef struct
{
	/*io part for PNG lib*/
	void *in_data;
	u32 in_length, current_pos;

	/*no support for scalability with PNG yet*/
	u16 ES_ID;
	u32 BPP, width, height, out_size, pixel_format;
} PNGDec;
	
#define PNGCTX()	PNGDec *ctx = (PNGDec *) ((IMGDec *)ifcg->privateStack)->opaque


static GF_Err PNG_AttachStream(GF_BaseDecoder *ifcg, u16 ES_ID, char *decSpecInfo, u32 decSpecInfoSize, u16 DependsOnES_ID, u32 objectTypeIndication, Bool UpStream)
{
	PNGCTX();
	if (ctx->ES_ID && ctx->ES_ID!=ES_ID) return GF_NOT_SUPPORTED;
	ctx->ES_ID = ES_ID;
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

static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
	PNGDec *ctx = (PNGDec *)png_ptr->io_ptr;

	if (ctx->current_pos + length > ctx->in_length) {
		png_error(png_ptr, "Read Error");
	} else {
		memcpy(data, (char*) ctx->in_data + ctx->current_pos, length);
		ctx->current_pos += length;
	}
}
static void user_error_fn(png_structp png_ptr,png_const_charp error_msg)
{
	longjmp(png_ptr->jmpbuf, 1);
}

static GF_Err PNG_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
	png_struct *png_ptr;
	png_info *info_ptr;
	png_byte **rows;
	u32 i, stride;

	PNGCTX();
	if ((inBufferLength<8) || png_sig_cmp(inBuffer, 0, 8) ) return GF_NON_COMPLIANT_BITSTREAM;

	ctx->in_data = inBuffer;
	ctx->in_length = inBufferLength;
	ctx->current_pos = 0;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp) ctx, NULL, NULL);
	if (!png_ptr) return GF_IO_ERR;
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return GF_IO_ERR;
	}
	if (setjmp(png_ptr->jmpbuf)) {
		png_destroy_info_struct(png_ptr,(png_infopp) & info_ptr);
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return GF_IO_ERR;
	}
    png_set_read_fn(png_ptr, ctx, (png_rw_ptr) user_read_data);
	png_set_error_fn(png_ptr, ctx, (png_error_ptr) user_error_fn, NULL);

	png_read_info(png_ptr, info_ptr);

	/*unpaletize*/
	if (info_ptr->color_type==PNG_COLOR_TYPE_PALETTE) {
		png_set_expand(png_ptr);
		png_read_update_info(png_ptr, info_ptr);
	}
	if (info_ptr->num_trans) {
		png_set_tRNS_to_alpha(png_ptr);
		png_read_update_info(png_ptr, info_ptr);
	}

	ctx->BPP = info_ptr->pixel_depth / 8;
	ctx->width = info_ptr->width;
	ctx->height = info_ptr->height;

	switch (ctx->BPP) {
	case 1:
		ctx->pixel_format = GF_PIXEL_GREYSCALE;
		break;
	case 2:
		ctx->pixel_format = GF_PIXEL_ALPHAGREY;
		break;
	case 3:
		ctx->pixel_format = GF_PIXEL_RGB_24;
		break;
	case 4:
		ctx->pixel_format = GF_PIXEL_RGBA;
		break;
	}

	/*new cfg, reset*/
	if (ctx->out_size != ctx->width * ctx->height * ctx->BPP) {
		ctx->out_size = ctx->width * ctx->height * ctx->BPP;
		*outBufferLength = ctx->out_size;
		png_destroy_info_struct(png_ptr,(png_infopp) & info_ptr);
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return GF_BUFFER_TOO_SMALL;
	}

	/*read*/
	stride = png_get_rowbytes(png_ptr, info_ptr);
	rows = (png_bytepp) malloc(sizeof(png_bytep) * ctx->height);
	for (i=0; i<ctx->height; i++) {
		rows[i] = outBuffer + i*stride;
	}
	png_read_image(png_ptr, rows);
	png_read_end(png_ptr, NULL);
	free(rows);

	png_destroy_info_struct(png_ptr,(png_infopp) & info_ptr);
	png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
	*outBufferLength = ctx->out_size;

	
	return GF_OK;
}

static const char *PNG_GetCodecName(GF_BaseDecoder *dec)
{
	return "LibPNG " PNG_LIBPNG_VER_STRING;
}

u32 NewPNGDec(GF_BaseDecoder *ifcd)
{
	IMGDec *wrap = (IMGDec *) ifcd->privateStack;
	PNGDec *dec = (PNGDec *) malloc(sizeof(PNGDec));
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
	free(ctx);
}

#endif
