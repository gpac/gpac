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

#ifdef GPAC_HAS_JP2

#if !defined(__GNUC__)
# if defined(_WIN32_WCE) || defined (WIN32)
#  if defined(_DEBUG)
#   pragma comment(lib, "LibOpenJPEGd")
#  else
#   pragma comment(lib, "LibOpenJPEG")
#  endif
# endif
#endif


#include <openjpeg.h>

typedef struct
{
	/*no support for scalability with JPEG (progressive JPEG to test)*/
	u32 bpp, nb_comp, width, height, out_size, pixel_format, dsi_size;
	char *dsi;
	opj_image_t *image;
} JP2Dec;

#define JP2CTX()	JP2Dec *ctx = (JP2Dec *) ((IMGDec *)ifcg->privateStack)->opaque


static GF_Err JP2_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	GF_BitStream *bs;
	JP2CTX();
	if (esd->dependsOnESID || esd->decoderConfig->upstream) return GF_NOT_SUPPORTED;

	if (!esd->decoderConfig->decoderSpecificInfo) return GF_OK;

	if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_IMAGE_JPEG_2000) {
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		ctx->height = gf_bs_read_u32(bs);
		ctx->width = gf_bs_read_u32(bs);
		ctx->nb_comp = gf_bs_read_u16(bs);
		ctx->bpp = 1 + gf_bs_read_u8(bs);
		ctx->out_size = ctx->width * ctx->height * ctx->nb_comp /* * ctx->bpp / 8 */;
		gf_bs_del(bs);

		switch (ctx->nb_comp) {
		case 1: ctx->pixel_format = GF_PIXEL_GREYSCALE; break;
		case 2: ctx->pixel_format = GF_PIXEL_ALPHAGREY; break;
		case 3: ctx->pixel_format = GF_PIXEL_RGB_24; break;
		case 4: ctx->pixel_format = GF_PIXEL_RGBA; break;
		default: return GF_NOT_SUPPORTED;
		}
	} else {
		bs = gf_bs_new(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_BITSTREAM_READ);
		gf_bs_read_u32(bs);
		ctx->width = gf_bs_read_u16(bs);
		ctx->height = gf_bs_read_u16(bs);
		gf_bs_del(bs);

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u32(bs, 12);
		gf_bs_write_u32(bs, GF_4CC('j','P',' ',' ') );
		gf_bs_write_u32(bs, 0x0D0A870A);
		gf_bs_write_u32(bs, 20);
		gf_bs_write_u32(bs, GF_4CC('f','t','y','p') );
		gf_bs_write_u32(bs, GF_4CC('j','p','2',' ') );
		gf_bs_write_u32(bs, 0);
		gf_bs_write_u32(bs, GF_4CC('j','p','2',' ') );

		gf_bs_write_data(bs, esd->decoderConfig->decoderSpecificInfo->data+8, esd->decoderConfig->decoderSpecificInfo->dataLength-8);
		gf_bs_get_content(bs, &ctx->dsi, &ctx->dsi_size);
		gf_bs_del(bs);

		ctx->nb_comp = 3;
		ctx->out_size = 3*ctx->width*ctx->height/2;
		ctx->pixel_format = GF_PIXEL_YV12; 
	}

	return GF_OK;
}
static GF_Err JP2_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	JP2CTX();
	if (ctx->image) {
		opj_image_destroy(ctx->image);
		ctx->image = NULL;
	}
	return GF_OK;
}
static GF_Err JP2_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	JP2CTX();
	switch (capability->CapCode) {
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ctx->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ctx->height;
		break;
	case GF_CODEC_STRIDE:
		if (ctx->pixel_format == GF_PIXEL_YV12) {
			capability->cap.valueInt = ctx->width;
		} else {
			capability->cap.valueInt = ctx->width * ctx->nb_comp;
		}
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
		capability->cap.valueInt = (ctx->pixel_format == GF_PIXEL_YV12) ? 4 : 1;
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
static GF_Err JP2_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;
}

/**
sample error callback expecting a FILE* client object
*/
void error_callback(const char *msg, void *client_data) 
{
	GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[OpenJPEG] Error %s", msg));
}
/**
sample warning callback expecting a FILE* client object
*/
void warning_callback(const char *msg, void *client_data) 
{
	GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[OpenJPEG] Warning %s", msg));
}
/**
sample debug callback expecting no client object
*/
void info_callback(const char *msg, void *client_data) 
{
	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[OpenJPEG] Info %s", msg));
}

/*
* Divide an integer by a power of 2 and round upwards.
*
* a divided by 2^b
*/
static int int_ceildivpow2(int a, int b) {
	return (a + (1 << b) - 1) >> b;
}

static GF_Err JP2_ProcessData(GF_MediaDecoder *ifcg, 
							  char *inBuffer, u32 inBufferLength,
							  u16 ES_ID, u32 *CTS,
							  char *outBuffer, u32 *outBufferLength,
							  u8 PaddingBits, u32 mmlevel)
{
	u32 i, w, wr, h, hr, wh;
	opj_dparameters_t parameters;	/* decompression parameters */
	opj_event_mgr_t event_mgr;		/* event manager */
	opj_dinfo_t* dinfo = NULL;	/* handle to a decompressor */
	opj_cio_t *cio = NULL;
	opj_codestream_info_t cinfo;

	JP2CTX();

#if 1
	switch (mmlevel) {
	case GF_CODEC_LEVEL_SEEK:
	case GF_CODEC_LEVEL_DROP:
		*outBufferLength = 0;
		return GF_OK;
	}
#endif

	if (!ctx->image) {
		/* configure the event callbacks (not required) */
		memset(&event_mgr, 0, sizeof(opj_event_mgr_t));
		event_mgr.error_handler = error_callback;
		event_mgr.warning_handler = warning_callback;
		event_mgr.info_handler = info_callback;

		/* set decoding parameters to default values */
		opj_set_default_decoder_parameters(&parameters);

		/* get a decoder handle */
		dinfo = opj_create_decompress(CODEC_JP2);

		/* catch events using our callbacks and give a local context */
		opj_set_event_mgr((opj_common_ptr)dinfo, &event_mgr, stderr);

		/* setup the decoder decoding parameters using the current image and user parameters */
		opj_setup_decoder(dinfo, &parameters);


		/* open a byte stream */
		if (ctx->dsi) {
			char *data;

			data = gf_malloc(sizeof(char) * (ctx->dsi_size+inBufferLength));
			memcpy(data, ctx->dsi, ctx->dsi_size);
			memcpy(data+ctx->dsi_size, inBuffer, inBufferLength);
			cio = opj_cio_open((opj_common_ptr)dinfo, data, ctx->dsi_size+inBufferLength);
			/* decode the stream and fill the image structure */
			ctx->image = opj_decode(dinfo, cio);
			gf_free(data);
		} else {
			cio = opj_cio_open((opj_common_ptr)dinfo, inBuffer, inBufferLength);
			/* decode the stream and fill the image structure */
			ctx->image = opj_decode_with_info(dinfo, cio, &cinfo);
		}

		//Fill the ctx info because dsi was not present
		if (ctx->image) {
			ctx->nb_comp = cinfo.numcomps;
			ctx->width = cinfo.image_w;
			ctx->height = cinfo.image_h;
			ctx->bpp = ctx->nb_comp * 8;
			ctx->out_size = ctx->width * ctx->height * ctx->nb_comp /* * ctx->bpp / 8 */;

			switch (ctx->nb_comp) {
			case 1: ctx->pixel_format = GF_PIXEL_GREYSCALE; break;
			case 2: ctx->pixel_format = GF_PIXEL_ALPHAGREY; break;
			case 3: ctx->pixel_format = GF_PIXEL_RGB_24; break;
			case 4: ctx->pixel_format = GF_PIXEL_RGBA; break;
			default: return GF_NOT_SUPPORTED;
			}

			if ( *outBufferLength < ctx->out_size ) {
				*outBufferLength = ctx->out_size;
				opj_destroy_decompress(dinfo);
				opj_cio_close(cio);
				return GF_BUFFER_TOO_SMALL;
			}
		}

		if(!ctx->image) {
			opj_destroy_decompress(dinfo);
			opj_cio_close(cio);
			return GF_IO_ERR;
		}

		/* close the byte stream */
		opj_cio_close(cio);
		cio = NULL;

		/* gf_free( remaining structures */
		if(dinfo) {
			opj_destroy_decompress(dinfo);
			dinfo = NULL;
		}
	}

	w = ctx->image->comps[0].w;
	wr = int_ceildivpow2(ctx->image->comps[0].w, ctx->image->comps[0].factor);
	h = ctx->image->comps[0].h;
	hr = int_ceildivpow2(ctx->image->comps[0].h, ctx->image->comps[0].factor);
	wh = wr*hr;

	if (ctx->nb_comp==1) {
		if ((w==wr) && (h==hr)) {
			for (i=0; i<wh; i++) {
				outBuffer[i] = ctx->image->comps[0].data[i];
			}
		} else {
			for (i=0; i<wh; i++) {
				outBuffer[i] = ctx->image->comps[0].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
			}
		}
	}
	else if (ctx->nb_comp==3) {

		if ((ctx->image->comps[0].w==2*ctx->image->comps[1].w) && (ctx->image->comps[1].w==ctx->image->comps[2].w)
			&& (ctx->image->comps[0].h==2*ctx->image->comps[1].h) && (ctx->image->comps[1].h==ctx->image->comps[2].h)) {

				if (ctx->pixel_format != GF_PIXEL_YV12) {
					ctx->pixel_format = GF_PIXEL_YV12;
					ctx->out_size = 3*ctx->width*ctx->height/2;
					*outBufferLength = ctx->out_size;
					return GF_BUFFER_TOO_SMALL;
				}

				if ((w==wr) && (h==hr)) {
					for (i=0; i<wh; i++) {
						*outBuffer = ctx->image->comps[0].data[i];
						outBuffer++;
					}
					w = ctx->image->comps[1].w;
					wr = int_ceildivpow2(ctx->image->comps[1].w, ctx->image->comps[1].factor);
					h = ctx->image->comps[1].h;
					hr = int_ceildivpow2(ctx->image->comps[1].h, ctx->image->comps[1].factor);
					wh = wr*hr;
					for (i=0; i<wh; i++) {
						*outBuffer = ctx->image->comps[1].data[i];
						outBuffer++;
					}
					for (i=0; i<wh; i++) {
						*outBuffer = ctx->image->comps[2].data[i];
						outBuffer++;
					}
				} else {
					for (i=0; i<wh; i++) {
						*outBuffer = ctx->image->comps[0].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					}
					w = ctx->image->comps[1].w;
					wr = int_ceildivpow2(ctx->image->comps[1].w, ctx->image->comps[1].factor);
					h = ctx->image->comps[1].h;
					hr = int_ceildivpow2(ctx->image->comps[1].h, ctx->image->comps[1].factor);
					wh = wr*hr;
					for (i=0; i<wh; i++) {
						*outBuffer = ctx->image->comps[1].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					}
					for (i=0; i<wh; i++) {
						*outBuffer = ctx->image->comps[2].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					}
				}


		} else if ((ctx->image->comps[0].w==ctx->image->comps[1].w) && (ctx->image->comps[1].w==ctx->image->comps[2].w)
			&& (ctx->image->comps[0].h==ctx->image->comps[1].h) && (ctx->image->comps[1].h==ctx->image->comps[2].h)) {

				if ((w==wr) && (h==hr)) {
					for (i=0; i<wh; i++) {
						u32 idx = 3*i;
						outBuffer[idx] = ctx->image->comps[0].data[i];
						outBuffer[idx+1] = ctx->image->comps[1].data[i];
						outBuffer[idx+2] = ctx->image->comps[2].data[i];
					}
				} else {
					for (i=0; i<wh; i++) {
						u32 idx = 3*i;
						outBuffer[idx] = ctx->image->comps[0].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
						outBuffer[idx+1] = ctx->image->comps[1].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
						outBuffer[idx+2] = ctx->image->comps[2].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					}
				}
		}
	}
	else if (ctx->nb_comp==4) {
		if ((ctx->image->comps[0].w==ctx->image->comps[1].w) && (ctx->image->comps[1].w==ctx->image->comps[2].w) && (ctx->image->comps[2].w==ctx->image->comps[3].w)
			&& (ctx->image->comps[0].h==ctx->image->comps[1].h) && (ctx->image->comps[1].h==ctx->image->comps[2].h) && (ctx->image->comps[2].h==ctx->image->comps[3].h)) {

				if ((w==wr) && (h==hr)) {
					for (i=0; i<wh; i++) {
						u32 idx = 4*i;
						outBuffer[idx] = ctx->image->comps[0].data[i];
						outBuffer[idx+1] = ctx->image->comps[1].data[i];
						outBuffer[idx+2] = ctx->image->comps[2].data[i];
						outBuffer[idx+3] = ctx->image->comps[3].data[i];
					}
				} else {
					for (i=0; i<wh; i++) {
						u32 idx = 4*i;
						outBuffer[idx] = ctx->image->comps[0].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
						outBuffer[idx+1] = ctx->image->comps[1].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
						outBuffer[idx+2] = ctx->image->comps[2].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
						outBuffer[idx+3] = ctx->image->comps[3].data[w * hr - ((i) / (wr) + 1) * w + (i) % (wr)];
					}
				}
		}
	}	

	/* gf_free( image data structure */
	if (ctx->image) {
		opj_image_destroy(ctx->image);
		ctx->image = NULL;
	}

	*outBufferLength = ctx->out_size;
	return GF_OK;
}

static const char *JP2_GetCodecName(GF_BaseDecoder *dec)
{
#ifdef OPENJPEG_VERSION
	return "OpenJPEG "OPENJPEG_VERSION ;
#else
	return "OpenJPEG" ;
#endif
}


Bool NewJP2Dec(GF_BaseDecoder *ifcd)
{
	IMGDec *wrap = (IMGDec *) ifcd->privateStack;
	JP2Dec *dec = (JP2Dec *) gf_malloc(sizeof(JP2Dec));
	memset(dec, 0, sizeof(JP2Dec));
	wrap->opaque = dec;
	wrap->type = DEC_JPEG;

	/*setup our own interface*/	
	ifcd->AttachStream = JP2_AttachStream;
	ifcd->DetachStream = JP2_DetachStream;
	ifcd->GetCapabilities = JP2_GetCapabilities;
	ifcd->SetCapabilities = JP2_SetCapabilities;
	ifcd->GetName = JP2_GetCodecName;
	((GF_MediaDecoder *)ifcd)->ProcessData = JP2_ProcessData;
	return 1;
}

void DeleteJP2Dec(GF_BaseDecoder *ifcg)
{
	JP2CTX();
	gf_free(ctx);
}

#endif
