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
#include <openHevcWrapper.h>

//#define OPEN_SHVC

#if defined(WIN32) && !defined(_WIN32_WCE) && !defined(__GNUC__)
#  pragma comment(lib, "libLibOpenHevcWrapper")
#endif

typedef struct
{
	u16 ES_ID;
	u32 width, stride, height, out_size, pixel_ar, layer, nb_threads, luma_bpp, chroma_bpp;

	Bool is_init;
	Bool had_pic;

	u32 nalu_size_length;
	u32 threading_type;
	Bool direct_output, has_pic;

	GF_ESD *esd;
	OpenHevc_Handle openHevcHandle;
#ifdef OPEN_SHVC
    u32 nb_layers;
    Bool base_only;
#endif
} HEVCDec;



static GF_Err HEVC_ConfigureStream(HEVCDec *ctx, GF_ESD *esd)
{
    u32 i, j;
    GF_HEVCConfig *cfg = NULL;
	ctx->ES_ID = esd->ESID;
	ctx->width = ctx->height = ctx->out_size = ctx->luma_bpp = ctx->chroma_bpp = 0;
	
#ifdef OPEN_SHVC
    ctx->nb_layers = 1;
    ctx->base_only = GF_FALSE;
#endif
    
    
	if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
        cfg = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, 0);
 		if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
 		ctx->nalu_size_length = cfg->nal_unit_size;
        
 		for (i=0; i< gf_list_count(cfg->param_array); i++) {
 			GF_HEVCParamArray *ar = gf_list_get(cfg->param_array, i);
            if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
                for (j=0; j< gf_list_count(ar->nalus); j++) {
                    GF_AVCConfigSlot *sl = gf_list_get(ar->nalus, j);
					HEVCState hevc;
                    s32 idx;
					u16 hdr = sl->data[0] << 8 | sl->data[1]; 
					
					idx = gf_media_hevc_read_sps(sl->data, sl->size, &hevc);
					ctx->width = MAX(hevc.sps[idx].width, ctx->width);
					ctx->height = MAX(hevc.sps[idx].height, ctx->height);
					ctx->luma_bpp = MAX(hevc.sps[idx].bit_depth_luma, ctx->luma_bpp);
					ctx->chroma_bpp = MAX(hevc.sps[idx].bit_depth_chroma, ctx->chroma_bpp);

					if (hdr & 0x1f8) {
#ifdef OPEN_SHVC
                        ctx->nb_layers ++;
#endif
                    }
                }
            }
        }
		gf_odf_hevc_cfg_del(cfg);
    } else {
		ctx->nalu_size_length = 0;
	}

#ifdef OPEN_SHVC
    ctx->openHevcHandle = libOpenHevcInit(ctx->nb_threads, ctx->nb_layers, 0);
#else
    ctx->openHevcHandle = libOpenHevcInit(ctx->nb_threads, ctx->threading_type);
#endif

	if (esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		libOpenHevcCopyExtraData(ctx->openHevcHandle, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength+8);
	}

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_CODEC, GF_LOG_DEBUG) ) {	
		libOpenHevcSetDebugMode(ctx->openHevcHandle, 1);
	}
#endif
    libOpenHevcStartDecoder(ctx->openHevcHandle);

	
	ctx->stride = ((ctx->luma_bpp==8) && (ctx->chroma_bpp==8)) ? ctx->width : ctx->width * 2;
	ctx->out_size = ctx->stride * ctx->height * 3 / 2;
	return GF_OK;
}

static GF_Err HEVC_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	GF_SystemRTInfo rti;
	const char *sOpt;
	u32 nb_threads = 1;
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	if (gf_sys_get_rti(100, &rti, 0) ) {
		nb_threads = (rti.nb_cores>1) ? rti.nb_cores-1 : 1;
		//checkme I have perf using too many threads
		if (nb_threads > 6)
			nb_threads = 6;
	}

	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "NumThreads");
	if (!sOpt) {
		char szO[100];
		sprintf(szO, "%d", nb_threads);
		gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "NumThreads", szO);
		ctx->nb_threads = nb_threads;
		GF_LOG(GF_LOG_CODEC, GF_LOG_INFO, ("[OpenHEVC] Initializing with %d threads\n", ctx->nb_threads));
	} else {
		ctx->nb_threads = atoi(sOpt);
		if (ctx->nb_threads > nb_threads) {
			GF_LOG(GF_LOG_CODEC, GF_LOG_WARNING, ("[OpenHEVC] Initializing with %d threads but only %d available cores detected on the system\n", ctx->nb_threads, rti.nb_cores));
		}
	}

	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "ThreadingType");
	if (sOpt && !strcmp(sOpt, "wpp")) ctx->threading_type = 2;
	else if (sOpt && !strcmp(sOpt, "frame+wpp")) ctx->threading_type = 3;
	else {
		ctx->threading_type = 1;
		if (!sOpt) gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "ThreadingType", "frame+wpp");
	}

	/*once base layer is configured, nothing to do on enhancement*/
	if (esd->dependsOnESID) return GF_OK;

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
		capability->cap.valueInt = (ctx->luma_bpp==8) ? GF_PIXEL_YV12 : GF_PIXEL_YV12_10;
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
	case GF_CODEC_DIRECT_OUTPUT:
		capability->cap.valueBool = 1;
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
#ifdef OPEN_SHVC
	case GF_CODEC_MEDIA_SWITCH_QUALITY:
		/*switch up*/
		if (capability.cap.valueInt) {
            ctx->base_only = GF_FALSE;
		} else {
            ctx->base_only = GF_TRUE;
		}
		return GF_OK;
#endif
	case GF_CODEC_DIRECT_OUTPUT:
		ctx->direct_output = GF_TRUE;
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
	if ((ctx->luma_bpp>8) || (ctx->chroma_bpp>8)) a_stride *= 2;

    if ((ctx->width != a_w) || (ctx->height!=a_h) || (ctx->stride != a_stride)) {
		ctx->width = a_w;
		ctx->stride = a_stride;
		ctx->height = a_h;
		ctx->out_size = ctx->stride * a_w * 3 / 2;
		ctx->had_pic = GF_TRUE;
		/*always force layer resize*/
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}
	if (ctx->direct_output) {
	    OpenHevc_Frame HVCFrame;
		libOpenHevcGetOutput(ctx->openHevcHandle, 1, &HVCFrame);
		*outBufferLength = ctx->out_size;
		ctx->has_pic = GF_TRUE;
	} else {

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
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	if (!inBuffer) {
		if ( libOpenHevcDecode(ctx->openHevcHandle, NULL, 0, 0) ) {
			return HEVC_flush_picture(ctx, outBuffer, outBufferLength);
		}
 		return GF_OK;
	}


	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("HEVC: Decoding AU %d bytes\n", inBufferLength));

	if (!ES_ID || (ES_ID!=ctx->ES_ID) ) {
		*outBufferLength = 0;
		return GF_OK;
	}
	if (*outBufferLength < ctx->out_size) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	*outBufferLength = 0;

	if (ctx->had_pic) {
		ctx->had_pic = 0;
		return HEVC_flush_picture(ctx, outBuffer, outBufferLength);
	}
	
	got_pic = libOpenHevcDecode(ctx->openHevcHandle, inBuffer, inBufferLength, 0);
	if (got_pic>0) {
		e = HEVC_flush_picture(ctx, outBuffer, outBufferLength);
		if (e) return e;
		got_pic = 0;
	}
	return GF_OK;
}


static GF_Err HEVC_GetOutputBuffer(GF_MediaDecoder *ifcg, u16 ESID, u8 **pY_or_RGB, u8 **pU, u8 **pV)
{
	s32 res;
    OpenHevc_Frame openHevcFrame;
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;
	if (!ctx->has_pic) return GF_BAD_PARAM;
	ctx->has_pic = GF_FALSE;

	res = libOpenHevcGetOutput(ctx->openHevcHandle, 1, &openHevcFrame);
	if ((res<=0) || !openHevcFrame.pvY || !openHevcFrame.pvU || !openHevcFrame.pvV)
		return GF_SERVICE_ERROR;

	*pY_or_RGB = (u8 *) openHevcFrame.pvY;
	*pU = (u8 *) openHevcFrame.pvU;
	*pV = (u8 *) openHevcFrame.pvV;
	return GF_OK;
}



static u32 HEVC_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;

	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_VIDEO_HEVC:
	case GPAC_OTI_VIDEO_SHVC:
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
	ifcd->GetOutputBuffer = HEVC_GetOutputBuffer;
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
