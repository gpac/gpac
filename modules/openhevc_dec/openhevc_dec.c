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

#include "openHevcWrapper.h"


#if defined(WIN32) && !defined(_WIN32_WCE) && !defined(__GNUC__)
#  pragma comment(lib, "libLibOpenHevcWrapper")
#endif

typedef struct
{
	u16 ES_ID;
	u32 width, stride, height, out_size, pixel_ar, layer;

	Bool is_init;
	Bool state_found;

	u32 nalu_size_length;
	u32 had_pic;

	Bool reset_dec_on_seek;

	GF_ESD *esd;
} HEVCDec;

static GF_Err HEVC_ConfigureStream(HEVCDec *ctx, GF_ESD *esd)
{
	ctx->ES_ID = esd->ESID;
	ctx->width = ctx->height = ctx->out_size = 0;
	ctx->state_found = 0;
	
	libOpenHevcInit(3);
	ctx->is_init = 1;

	if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		u32 i, j;
		GF_HEVCConfig *cfg = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
		if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->nalu_size_length = cfg->nal_unit_size;

		for (i=0; i< gf_list_count(cfg->param_array); i++) {
			GF_HEVCParamArray *ar = gf_list_get(cfg->param_array, i);
			for (j=0; j< gf_list_count(ar->nalus); j++) {
				GF_AVCConfigSlot *sl = gf_list_get(ar->nalus, j);
				libOpenHevcDecode(sl->data, sl->size);

				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					HEVCState hevc;
					s32 idx = gf_media_hevc_read_sps(sl->data, sl->size, &hevc);
					ctx->width = hevc.sps[idx].width;
					ctx->height = hevc.sps[idx].height;
				}
			}
		}
		ctx->state_found = 1;
		gf_odf_hevc_cfg_del(cfg);
	} else {
		ctx->nalu_size_length = 0;
	}
	ctx->stride = ctx->width;
	ctx->out_size = ctx->stride * ctx->height * 3 / 2;
	return GF_OK;
}


static GF_Err HEVC_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	const char *sOpt;
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	/*that's a bit crude ...*/
	gf_modules_set_option((GF_BaseInterface *)ifcg, "Systems", "DrawLateFrames", "yes");

	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "DestroyDecoderUponSeek");
	if (!sOpt) {
		gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "DestroyDecoderUponSeek", "yes");
		ctx->reset_dec_on_seek = 1;
	} else if (!strcmp(sOpt, "yes")) {
		ctx->reset_dec_on_seek = 1;
	}

	/*not supported in this version*/
	if (esd->dependsOnESID) return GF_NOT_SUPPORTED;

	ctx->esd = esd;
	return HEVC_ConfigureStream(ctx, esd);
}


static GF_Err HEVC_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	if (ctx->is_init) {
		libOpenHevcClose();
		ctx->is_init = 0;
	}
	ctx->width = ctx->height = ctx->out_size = 0;
	return GF_OK;
}

static GF_Err HEVC_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
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
		capability->cap.valueInt = 3;
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
static GF_Err HEVC_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	switch (capability.CapCode) {
    case GF_CODEC_WAIT_RAP:
            libOpenHevcFlush();
        return GF_OK;
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

static GF_Err HEVC_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
	unsigned int got_pic, a_w, a_h, a_stride;
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;
	u8 *pY, *pU, *pV;


	if (!inBuffer) {
		/*because of some seeking bugs, we destroy the context and reinit when we see a flush ... */
		if (ctx->reset_dec_on_seek) {
			*outBufferLength = 0;
			libOpenHevcClose();
			return HEVC_ConfigureStream(ctx, ctx->esd);
		} else {
			int flushed;
		
			pY = outBuffer;
			pU = outBuffer + ctx->stride * ctx->height;
			pV = outBuffer + 5*ctx->stride * ctx->height/4;

			flushed = libOpenHevcGetOutputCpy(1, pY, pU, pV);


			if (flushed) {
				*outBufferLength = ctx->out_size;
				*outBufferLength = 0;
			}
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

	got_pic = 0;
	if (ctx->had_pic) {
		got_pic = 1;
		ctx->had_pic = 0;
	} else {
		u32 sc_size = 0;
		u32 i, nalu_size = 0;
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

			if (!ctx->state_found) {
				u8 nal_type = (ptr[0] & 0x7E) >> 1;
				switch (nal_type) {
				case GF_HEVC_NALU_VID_PARAM:
				case GF_HEVC_NALU_SEQ_PARAM:
				case GF_HEVC_NALU_PIC_PARAM:
					ctx->state_found = 1;
					break;
				}
			}

			if (ctx->state_found) {
				if (!got_pic) {
					got_pic = libOpenHevcDecode(ptr, nalu_size);
				} else {
//					libOpenHevcDecode(ptr, nalu_size);
//					printf("%d bytes left over from frame - nal type %d\n", nalu_size, (ptr[0] & 0x7E) >> 1 );
				}
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
		if (got_pic==0) {
			*outBufferLength = 0;
			return GF_OK;
		}
	}

	a_w = a_h = a_stride = 0;
	libOpenHevcGetPictureSize(&a_w, &a_h, &a_stride);

	if ((ctx->width != a_w) || (ctx->height!=a_h) || (ctx->stride != a_stride)) {
		ctx->width = a_w;
		ctx->stride = a_stride;
		ctx->height = a_h;
		ctx->out_size = ctx->stride * a_w * 3 / 2;
		ctx->had_pic = 1;
		/*always force layer resize*/
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}
	*outBufferLength = 0;

	pY = outBuffer;
	pU = outBuffer + ctx->stride * ctx->height;
	pV = outBuffer + 5*ctx->stride * ctx->height/4;

	if (libOpenHevcGetOutputCpy(1, pY, pU, pV)) {
		*outBufferLength = ctx->out_size;
	}

	return GF_OK;
}

static u32 HEVC_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
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

static const char *HEVC_GetCodecName(GF_BaseDecoder *dec)
{
	return libOpenHevcVersion();
}

GF_BaseDecoder *NewHEVCDec()
{
	GF_MediaDecoder *ifcd;
	HEVCDec *dec;
	
	GF_SAFEALLOC(ifcd, GF_MediaDecoder);
	GF_SAFEALLOC(dec, HEVCDec);
	GF_REGISTER_MODULE_INTERFACE(ifcd, GF_MEDIA_DECODER_INTERFACE, "HEVC Decoder", "gpac distribution")

	ifcd->privateStack = dec;

	/*setup our own interface*/	
	ifcd->AttachStream = HEVC_AttachStream;
	ifcd->DetachStream = HEVC_DetachStream;
	ifcd->GetCapabilities = HEVC_GetCapabilities;
	ifcd->SetCapabilities = HEVC_SetCapabilities;
	ifcd->GetName = HEVC_GetCodecName;
	ifcd->CanHandleStream = HEVC_CanHandleStream;
	ifcd->ProcessData = HEVC_ProcessData;
	return (GF_BaseDecoder *) ifcd;
}

void DeleteHEVCDec(GF_BaseDecoder *ifcg)
{
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;
	gf_free(ctx);
	gf_free(ifcg);
}

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
#ifndef GPAC_DISABLE_AV_PARSERS
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewHEVCDec();
#endif
	return NULL;
}

GPAC_MODULE_EXPORT
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

GPAC_MODULE_STATIC_DELARATION( openhevc )

