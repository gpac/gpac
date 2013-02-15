/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2010-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / OpenSVC Decoder module
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
#include <gpac/internal/media_dev.h>
#include "wrapper.h"


#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
#  pragma comment(lib, "libhevcdecoder")
#endif

typedef struct
{
	u16 ES_ID;
	u32 width, stride, height, out_size, pixel_ar, layer;

	Bool is_init;

	u32 nalu_size_length;
	u32 had_pic;
} HEVCDec;

static GF_Err OSVC_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	/*not supported in this version*/
	if (esd->dependsOnESID) return GF_NOT_SUPPORTED;
	
	ctx->ES_ID = esd->ESID;
	ctx->width = ctx->height = ctx->out_size = 0;

	libDecoderInit();
	ctx->is_init = 1;

	if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		unsigned int temp_id;
		u32 i, j;
		GF_HEVCConfig *cfg = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
		if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->nalu_size_length = cfg->nal_unit_size;

		for (i=0; i< gf_list_count(cfg->param_array); i++) {
			GF_HEVCParamArray *ar = gf_list_get(cfg->param_array, i);
			for (j=0; j< gf_list_count(ar->nalus); j++) {
				GF_AVCConfigSlot *sl = gf_list_get(ar->nalus, j);
				libDecoderDecode(sl->data, sl->size, &temp_id);

				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					HEVCState hevc;
					s32 idx = gf_media_hevc_read_sps(sl->data, sl->size, &hevc);
					ctx->width = hevc.sps[idx].width;
					ctx->height = hevc.sps[idx].height;
				}
			}
		}
		gf_odf_hevc_cfg_del(cfg);
	} else {
		ctx->nalu_size_length = 0;
	}
	ctx->stride = ctx->width;
	ctx->out_size = ctx->stride * ctx->height * 3 / 2;
	return GF_OK;
}

static GF_Err OSVC_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	if (ctx->is_init) {
		libDecoderClose();
		ctx->is_init = 0;
	}
	ctx->width = ctx->height = ctx->out_size = 0;
	return GF_OK;
}

static GF_Err OSVC_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	switch (capability->CapCode) {
	case GF_CODEC_RESILIENT:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_WIDTH:
		capability->cap.valueInt = ctx->width;
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ctx->height;
		break;
	case GF_CODEC_STRIDE:
		capability->cap.valueInt = ctx->stride;
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
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 32;
		break;
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 1;
		break;
	/*not known at our level...*/
	case GF_CODEC_CU_DURATION:
	default:
		capability->cap.valueInt = 0;
		break;
	}
	return GF_OK;
}
static GF_Err OSVC_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;
	switch (capability.CapCode) {
	case GF_CODEC_MEDIA_SWITCH_QUALITY:
		/*todo - update temporal filtering*/
		if (capability.cap.valueInt) {
		} else {
		}
		return GF_OK;
	}
	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;

}

static GF_Err OSVC_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
	unsigned int got_pic, a_w, a_h, temporal_id;
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;
	u8 *pY, *pU, *pV;


	if (!inBuffer) {
		int flushed;
		
		pY = outBuffer;
		pU = outBuffer + ctx->stride * ctx->height;
		pV = outBuffer + 5*ctx->stride * ctx->height/4;
		return GF_OK;

		flushed = libDecoderGetOuptut(0, pY, pU, pV, 1);
		if (flushed) {
			*outBufferLength = ctx->out_size;
		}
		return GF_OK;
	}

	if (!ES_ID || (ES_ID!=ctx->ES_ID) ) {
		*outBufferLength = 0;
		return GF_OK;
	}
	if (*outBufferLength < ctx->out_size) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	temporal_id = 0;
	got_pic = 0;
	if (ctx->had_pic) {
		got_pic = 1;
		temporal_id = ctx->had_pic;
		ctx->had_pic = 0;
	} else {
		u32 sc_size = 0;
		u32 temp_id, i, nalu_size = 0;
		u32 total_size = inBufferLength;
		u8 *ptr = inBuffer;

		if (!ctx->nalu_size_length) {
			u32 size = gf_media_nalu_next_start_code(inBuffer, inBufferLength, &sc_size);
			if (sc_size) {
				ptr += size+sc_size;
				inBufferLength-=size+sc_size;
			}
		}

		while (inBufferLength) {
						
			if (ctx->nalu_size_length) {
				for (i=0; i<ctx->nalu_size_length; i++) {
					nalu_size = (nalu_size<<8) + ptr[i];
				}
				ptr += ctx->nalu_size_length;
			} else {
				nalu_size = gf_media_nalu_next_start_code(ptr, inBufferLength, &sc_size);
			}

			if (!got_pic) {
				got_pic = libDecoderDecode(ptr, nalu_size, &temp_id);

				/*the HM is a weird beast: 
				    "... design fault in the decoder, whereby
			 * the process of reading a new slice that is the first slice of a new frame
			 * requires the TDecTop::decode() method to be called again with the same
			 * nal unit. "

				*/
				if (got_pic) {
					temporal_id = temp_id;

					libDecoderDecode(ptr, nalu_size, &temp_id);
				}

			} else {
				libDecoderDecode(ptr, nalu_size, &temp_id);
			}

			ptr += nalu_size;
			if (ctx->nalu_size_length) {
				if (inBufferLength < nalu_size + ctx->nalu_size_length) break;
				inBufferLength -= nalu_size + ctx->nalu_size_length;
			} else {
				if (!sc_size || (inBufferLength < nalu_size + sc_size)) break;
				inBufferLength -= nalu_size + sc_size;
				ptr += sc_size;
			}
		}
		if (got_pic!=1) return GF_OK;
	}

	a_w = a_h = 0;
	libDecoderGetPictureSize(&a_w, &a_h);
	if ((ctx->width != a_w) || (ctx->height!=a_h)) {
		ctx->width = a_w;
		ctx->stride = a_w;
		ctx->height = a_h;
		ctx->out_size = ctx->stride * a_w * 3 / 2;
		ctx->had_pic = 1;
		/*always force layer resize*/
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}
	*outBufferLength = ctx->out_size;

	pY = outBuffer;
	pU = outBuffer + ctx->stride * ctx->height;
	pV = outBuffer + 5*ctx->stride * ctx->height/4;

	if (libDecoderGetOuptut(temporal_id, pY, pU, pV, 0)) {
		*outBufferLength = ctx->out_size;
	}
	return GF_OK;
}

static u32 OSVC_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;

	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_VIDEO_HEVC:
		return GF_CODEC_SUPPORTED;
	}
	return GF_CODEC_NOT_SUPPORTED;
}

static const char *OSVC_GetCodecName(GF_BaseDecoder *dec)
{
	return libDecoderVersion();;
}

GF_BaseDecoder *NewHEVCDec()
{
	GF_MediaDecoder *ifcd;
	HEVCDec *dec;
	
	GF_SAFEALLOC(ifcd, GF_MediaDecoder);
	GF_SAFEALLOC(dec, HEVCDec);
	GF_REGISTER_MODULE_INTERFACE(ifcd, GF_MEDIA_DECODER_INTERFACE, "OpenSVC Decoder", "gpac distribution")

	ifcd->privateStack = dec;

	/*setup our own interface*/	
	ifcd->AttachStream = OSVC_AttachStream;
	ifcd->DetachStream = OSVC_DetachStream;
	ifcd->GetCapabilities = OSVC_GetCapabilities;
	ifcd->SetCapabilities = OSVC_SetCapabilities;
	ifcd->GetName = OSVC_GetCodecName;
	ifcd->CanHandleStream = OSVC_CanHandleStream;
	ifcd->ProcessData = OSVC_ProcessData;
	return (GF_BaseDecoder *) ifcd;
}

void DeleteHEVCDec(GF_BaseDecoder *ifcg)
{
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;
	gf_free(ctx);
	gf_free(ifcg);
}

GF_EXPORT
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

GF_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType) 
{
#ifndef GPAC_DISABLE_AV_PARSERS
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewHEVCDec();
#endif
	return NULL;
}

GF_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_MEDIA_DECODER_INTERFACE: 
		DeleteHEVCDec((GF_BaseDecoder*)ifce);
		break;
#endif
	}
}
