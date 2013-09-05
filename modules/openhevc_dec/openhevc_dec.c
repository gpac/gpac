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
	u32 width, stride, height, out_size, pixel_ar, layer, nb_threads;

	Bool is_init;
	Bool state_found;

	u32 nalu_size_length;
	u32 restart_from;

	GF_ESD *esd;
	OpenHevc_Handle openHevcHandle;
} HEVCDec;

static GF_Err HEVC_ConfigureStream(HEVCDec *ctx, GF_ESD *esd)
{
	ctx->ES_ID = esd->ESID;
	ctx->width = ctx->height = ctx->out_size = 0;
	ctx->state_found = GF_FALSE;
	
	ctx->openHevcHandle = libOpenHevcInit(ctx->nb_threads);
    libOpenHevcSetDisableAU(ctx->openHevcHandle, 1);
	ctx->is_init = GF_TRUE;

	if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		u32 i, j;
		GF_HEVCConfig *cfg = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
		if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->nalu_size_length = cfg->nal_unit_size;

		for (i=0; i< gf_list_count(cfg->param_array); i++) {
			GF_HEVCParamArray *ar = gf_list_get(cfg->param_array, i);
			for (j=0; j< gf_list_count(ar->nalus); j++) {
				GF_AVCConfigSlot *sl = gf_list_get(ar->nalus, j);
				libOpenHevcDecode(ctx->openHevcHandle, sl->data, sl->size, 0);

				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					HEVCState hevc;
					s32 idx = gf_media_hevc_read_sps(sl->data, sl->size, &hevc);
					ctx->width = hevc.sps[idx].width;
					ctx->height = hevc.sps[idx].height;
				}
			}
		}
		ctx->state_found = GF_TRUE;
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
	if (gf_modules_get_option((GF_BaseInterface *)ifcg, "Systems", "DrawLateFrames")==NULL)
		gf_modules_set_option((GF_BaseInterface *)ifcg, "Systems", "DrawLateFrames", "yes");

	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "NumThreads");
	if (!sOpt) {
		gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "NumThreads", "1");
		ctx->nb_threads = 1;
	} else {
		ctx->nb_threads = atoi(sOpt);
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
		libOpenHevcClose(ctx->openHevcHandle);
		ctx->is_init = GF_FALSE;
	}
	fprintf(stderr, "closing hevc dec\n");
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
static GF_Err HEVC_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;
	switch (capability.CapCode) {
    case GF_CODEC_WAIT_RAP:
            libOpenHevcFlush(ctx->openHevcHandle);
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

static GF_Err HEVC_flush_picture(HEVCDec *ctx, char *outBuffer, u32 *outBufferLength )
{
	unsigned int a_w, a_h, a_stride;
    OpenHevc_Frame_cpy openHevcFrame;
	u8 *pY, *pU, *pV;

	libOpenHevcGetPictureInfo(ctx->openHevcHandle, &openHevcFrame.frameInfo);


	a_w      = openHevcFrame.frameInfo.nWidth;
    a_h      = openHevcFrame.frameInfo.nHeight;
    a_stride = openHevcFrame.frameInfo.nYPitch;

    if ((ctx->width != a_w) || (ctx->height!=a_h) || (ctx->stride != a_stride)) {
		ctx->width = a_w;
		ctx->stride = a_stride;
		ctx->height = a_h;
		ctx->out_size = ctx->stride * a_w * 3 / 2;
		/*always force layer resize*/
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

    pY = outBuffer;
    pU = outBuffer + ctx->stride * ctx->height;
    pV = outBuffer + 5*ctx->stride * ctx->height/4;
    openHevcFrame.pvY = (void*) pY;
    openHevcFrame.pvU = (void*) pU;
    openHevcFrame.pvV = (void*) pV;
    *outBufferLength = 0;
    if (libOpenHevcGetOutputCpy(ctx->openHevcHandle, 1, &openHevcFrame)) {
        *outBufferLength = ctx->out_size;
    }
    return GF_OK;
}


static GF_Err HEVC_ProcessData(GF_MediaDecoder *ifcg, 
		char *inBuffer, u32 inBufferLength,
		u16 ES_ID,
		char *outBuffer, u32 *outBufferLength,
		u8 PaddingBits, u32 mmlevel)
{
	GF_Err e;
	int got_pic;
    OpenHevc_Frame_cpy openHevcFrame;
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;
	u8 *pY, *pU, *pV, *ptr;
	u32 nb_pics=0;
	u32 input_data_consumed = 0;
	u32 sc_size, i, nalu_size;


	if (!inBuffer) {
		pY = outBuffer;
		pU = outBuffer + ctx->stride * ctx->height;
		pV = outBuffer + 5*ctx->stride * ctx->height/4;
	    openHevcFrame.pvY = (void*) pY;
	    openHevcFrame.pvU = (void*) pU;
	    openHevcFrame.pvV = (void*) pV;
	    *outBufferLength = 0;

		if ( libOpenHevcDecode(ctx->openHevcHandle, NULL, 0, 0) ) {
			if (libOpenHevcGetOutputCpy(ctx->openHevcHandle, 1, &openHevcFrame)) {
				*outBufferLength = ctx->out_size;
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

	nb_pics = 0;
	if (ctx->restart_from) {
		inBuffer += ctx->restart_from;
		inBufferLength -= ctx->restart_from;
		ctx->restart_from = 0;

		e = HEVC_flush_picture(ctx, outBuffer, outBufferLength);
		if (e) return e;
		nb_pics ++;
	}

	got_pic = 0;

	sc_size = 0;
	nalu_size = 0;
	ptr = inBuffer;
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

//		fprintf(stderr, "HEVC decode NAL type %d size %d\n", (ptr[0] & 0x7E) >> 1, nalu_size);

		if (!ctx->state_found) {
			u8 nal_type = (ptr[0] & 0x7E) >> 1;
			switch (nal_type) {
			case GF_HEVC_NALU_VID_PARAM:
			case GF_HEVC_NALU_SEQ_PARAM:
			case GF_HEVC_NALU_PIC_PARAM:
				ctx->state_found = GF_TRUE;
				break;
			}
		}

		if (ctx->state_found) {
			got_pic = libOpenHevcDecode(ctx->openHevcHandle, ptr, nalu_size, 0);
			if (got_pic>0) {
				nb_pics ++;
				e = HEVC_flush_picture(ctx, outBuffer, outBufferLength);
				if (e) {
					if (e==GF_BUFFER_TOO_SMALL) {
						if (ctx->nalu_size_length) {
							ctx->restart_from = nalu_size + ctx->nalu_size_length;
						} else {
							ctx->restart_from = input_data_consumed + sc_size;
						}
					}
					return e;
				}
				got_pic = 0;
			}
		}

		ptr += nalu_size;
		if (ctx->nalu_size_length) {
			if (inBufferLength < nalu_size + ctx->nalu_size_length) break;
			inBufferLength -= nalu_size + ctx->nalu_size_length;
			input_data_consumed += nalu_size + ctx->nalu_size_length;
		} else {
			if (!sc_size || (inBufferLength < nalu_size + sc_size)) break;
			inBufferLength -= nalu_size + sc_size;
			ptr += sc_size;
			input_data_consumed += sc_size;
		}
	}
	if (!nb_pics && (got_pic==0)) {
		*outBufferLength = 0;
		return GF_OK;
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
	HEVCDec *ctx = (HEVCDec*) dec->privateStack;
	return libOpenHevcVersion(ctx->openHevcHandle);
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

