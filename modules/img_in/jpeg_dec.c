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

#ifdef GPAC_HAS_JPEG

#include <setjmp.h>

#include "jpeglib.h"

typedef struct
{
	/*no support for scalability with JPEG (progressive JPEG to test)*/
	u16 ES_ID;
	u32 BPP, width, height, out_size, pixel_format;
} JPEGDec;

#define JPEGCTX()	JPEGDec *ctx = (JPEGDec *) ((IMGDec *)ifcg->privateStack)->opaque

/*JPG context while decoding*/
typedef struct
{
	struct jpeg_error_mgr pub;
	jmp_buf jmpbuf;
} JPGErr;

typedef struct
{
	/*io manager*/
    struct jpeg_source_mgr src;

	s32 skip;
	struct jpeg_decompress_struct cinfo;
} JPGCtx;


static GF_Err JPEG_AttachStream(GF_BaseDecoder *ifcg, u16 ES_ID, unsigned char *decSpecInfo, u32 decSpecInfoSize, u16 DependsOnES_ID, u32 objectTypeIndication, Bool UpStream)
{
	JPEGCTX();
	if (ctx->ES_ID && ctx->ES_ID!=ES_ID) return GF_NOT_SUPPORTED;
	ctx->ES_ID = ES_ID;
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
	default:
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}
static GF_Err JPEG_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;
}

void _nonfatal_error(j_common_ptr cinfo) { }
void _nonfatal_error2(j_common_ptr cinfo, int lev) {}

void _fatal_error(j_common_ptr cinfo) 
{
	JPGErr *err = (JPGErr *) cinfo->err;
	longjmp(err->jmpbuf, 1);
}

void stub(j_decompress_ptr cinfo) {}

/*a JPEG is always carried in a complete, single MPEG4 AU so no refill*/
boolean fill_input_buffer(j_decompress_ptr cinfo) { return 0; }

void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	JPGCtx *jpx = (JPGCtx *) cinfo->src;
	if (num_bytes > (long) jpx->src.bytes_in_buffer) {
		jpx->skip = num_bytes - jpx->src.bytes_in_buffer;
		jpx->src.next_input_byte += jpx->src.bytes_in_buffer;
		jpx->src.bytes_in_buffer = 0;
	} else {
		jpx->src.bytes_in_buffer -= num_bytes;
		jpx->src.next_input_byte += num_bytes;
		jpx->skip = 0;
	}
}


#define JPEG_MAX_SCAN_BLOCK_HEIGHT		16

static GF_Err JPEG_ProcessData(GF_MediaDecoder *ifcg, 
		unsigned char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		unsigned char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
	s32 i, j, scans, k;
	char *scan_line, *ptr, *dst;
	char *lines[JPEG_MAX_SCAN_BLOCK_HEIGHT];


	JPGErr jper;
	JPGCtx jpx;
	JPEGCTX();

	jpx.cinfo.err = jpeg_std_error(&(jper.pub));
	jper.pub.error_exit = _fatal_error;
	jper.pub.output_message = _nonfatal_error;
	jper.pub.emit_message = _nonfatal_error2;
	if (setjmp(jper.jmpbuf)) {
		jpeg_destroy_decompress(&jpx.cinfo);
		*outBufferLength = 0;
		return GF_IO_ERR;
	}

	/*create decompress struct*/
	jpeg_create_decompress(&jpx.cinfo);

	/*prepare IO*/
	jpx.src.init_source = stub;
	jpx.src.fill_input_buffer = fill_input_buffer;
	jpx.src.skip_input_data = skip_input_data;
	jpx.src.resync_to_restart = jpeg_resync_to_restart;
	jpx.src.term_source = stub;
	jpx.skip = 0;
    jpx.src.next_input_byte = inBuffer;
    jpx.src.bytes_in_buffer = inBufferLength;
	jpx.cinfo.src = (void *) &jpx.src;


	/*read header*/
	do {
		i = jpeg_read_header(&jpx.cinfo, TRUE);
	} while (i == JPEG_HEADER_TABLES_ONLY);
	/*we're supposed to have the full image in the buffer, wrong stream*/
	if (i == JPEG_SUSPENDED) {
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if ( (ctx->width != jpx.cinfo.image_width) || 
		(ctx->height != jpx.cinfo.image_height) ||
		(ctx->BPP != (u32) jpx.cinfo.num_components)  ) 
	{
		ctx->width = jpx.cinfo.image_width;
		ctx->height = jpx.cinfo.image_height;
		ctx->BPP = jpx.cinfo.num_components;
		ctx->out_size = ctx->BPP * ctx->height * ctx->width;
		switch (ctx->BPP) {
		case 1:
			ctx->pixel_format = GF_PIXEL_GREYSCALE;
			break;
		case 3:
			ctx->pixel_format = GF_PIXEL_RGB_24;
			break;
		default:
			jpeg_destroy_decompress(&jpx.cinfo);
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		*outBufferLength = ctx->out_size;
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_BUFFER_TOO_SMALL;
	}
	if (*outBufferLength < ctx->out_size) {
		*outBufferLength = ctx->out_size;
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_BUFFER_TOO_SMALL;
	}

	/*decode*/
	jpx.cinfo.do_fancy_upsampling = FALSE;
	jpx.cinfo.do_block_smoothing = FALSE;
	if (!jpeg_start_decompress(&jpx.cinfo)) {
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (jpx.cinfo.rec_outbuf_height>JPEG_MAX_SCAN_BLOCK_HEIGHT) {
		jpeg_destroy_decompress(&jpx.cinfo);
		return GF_IO_ERR;
	}

	/*read scanlines (the scan is not one line by one line so alloc a placeholder for block scaning) */
	scan_line = malloc(sizeof(char) * ctx->width * ctx->BPP * jpx.cinfo.rec_outbuf_height);
	for (i = 0; i<jpx.cinfo.rec_outbuf_height;i++) {
		lines[i] = scan_line + i * ctx->width * ctx->BPP;
	}
	dst = outBuffer;
	for (j=0; j< (s32) ctx->height; j += jpx.cinfo.rec_outbuf_height) {
		jpeg_read_scanlines(&jpx.cinfo, (unsigned char **) lines, jpx.cinfo.rec_outbuf_height);
        scans = jpx.cinfo.rec_outbuf_height;
        if (( (s32) ctx->height - j) < scans) scans = ctx->height - j;
        ptr = scan_line;
		/*for each line in the scan*/
		for (k = 0; k < scans; k++) {
			memcpy(dst, ptr, sizeof(char) * ctx->width * ctx->BPP);
			ptr += ctx->width * ctx->BPP;
			dst += ctx->width * ctx->BPP;
		}
	}

	/*done*/
	jpeg_finish_decompress(&jpx.cinfo);
	jpeg_destroy_decompress(&jpx.cinfo);

	free(scan_line);
	*outBufferLength = ctx->out_size;
	return GF_OK;
}

static const char *JPEG_GetCodecName(GF_BaseDecoder *dec)
{
	return "JPEG 6b IJG";
}


u32 NewJPEGDec(GF_BaseDecoder *ifcd)
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

#endif
