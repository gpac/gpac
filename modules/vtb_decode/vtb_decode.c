/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / codec pack module
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



#include <stdint.h>

#define Picture QuickdrawPicture
#include <VideoToolbox/VideoToolbox.h>
#undef Picture

#ifndef kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder
#  define kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder CFSTR("RequireHardwareAcceleratedVideoDecoder")
#endif

//do not include math.h we would have a conflict with Fixed ... we're lucky we don't need maths routines here
#define _GF_MATH_H_

#include <gpac/modules/codec.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>

#include "../../src/compositor/gl_inc.h"

#ifndef GPAC_DISABLE_AV_PARSERS

typedef struct
{
	Bool is_hardware;
	u32 width, height;
	u32 pixel_ar, pix_fmt;
	u32 out_size;
	Bool raw_frame_dispatch;
	
	GF_ESD *esd;
	GF_Err last_error;
	
	int vtb_type;
	VTDecompressionSessionRef vtb_session;
    CMFormatDescriptionRef fmt_desc;
	CVPixelBufferRef frame;

	u8 chroma_format, luma_bit_depth, chroma_bit_depth;
	Bool frame_size_changed;
	
	//MPEG-1/2 specific
	Bool init_mpeg12;
	
	//MPEG-4 specific
	Bool skip_mpeg4_vosh;
	char *vosh;
	u32 vosh_size;
	
	//NAL-based specific
	char *sps, *pps;
	u32 sps_size, pps_size;
	Bool is_annex_b;
	char *cached_annex_b;
	u32 cached_annex_b_size;
	u32 nalu_size_length;

	//openGL output
#ifdef GPAC_IPHONE
	Bool use_gl_textures;
	CVOpenGLESTextureCacheRef cache_texture;
#endif
	void *gl_context;
} VTBDec;


static void VTBDec_on_frame(void *opaque, void *sourceFrameRefCon, OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef image, CMTime pts, CMTime duration)
{
	VTBDec *ctx = (VTBDec *)opaque;
    if (ctx->frame) {
        CVPixelBufferRelease(ctx->frame);
        ctx->frame = NULL;
    }
	if (status) ctx->last_error = GF_NON_COMPLIANT_BITSTREAM;
	
    if (!image) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[VTB] No output buffer - status %d\n", status));
        return;
    }
    ctx->frame = CVPixelBufferRetain(image);
}

static CFDictionaryRef VTBDec_CreateBufferAttributes(VTBDec *ctx, OSType pix_fmt)
{
    CFMutableDictionaryRef buffer_attributes;
    CFMutableDictionaryRef surf_props;
    CFNumberRef w;
    CFNumberRef h;
    CFNumberRef pixel_fmt;

    w = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &ctx->width);
    h = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &ctx->height);
    pixel_fmt = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pix_fmt);

    buffer_attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    surf_props = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
    CFDictionarySetValue(buffer_attributes, kCVPixelBufferWidthKey, w);
    CFRelease(w);
    CFDictionarySetValue(buffer_attributes, kCVPixelBufferHeightKey, h);
    CFRelease(h);
    CFDictionarySetValue(buffer_attributes, kCVPixelBufferPixelFormatTypeKey, pixel_fmt);
    CFRelease(pixel_fmt);

#ifdef GPAC_IPHONE
	if (ctx->use_gl_textures)
		CFDictionarySetValue(buffer_attributes, kCVPixelBufferOpenGLESCompatibilityKey, kCFBooleanTrue);
#endif

    CFDictionarySetValue(buffer_attributes, kCVPixelBufferIOSurfacePropertiesKey, surf_props);
    CFRelease(surf_props);

    return buffer_attributes;
}

static GF_Err VTBDec_InitDecoder(VTBDec *ctx, Bool force_dsi_rewrite)
{
	CFMutableDictionaryRef dec_dsi, dec_type;
	CFMutableDictionaryRef dsi;
	VTDecompressionOutputCallbackRecord cbacks;
    CFDictionaryRef buffer_attribs;
    OSStatus status;
	OSType kColorSpace;
	
	CFDataRef data = NULL;
	char *dsi_data=NULL;
	u32 dsi_data_size=0;
	
    dec_dsi = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
//	kColorSpace = kCVPixelFormatType_420YpCbCr8Planar;
//	ctx->pix_fmt = GF_PIXEL_YV12;
	
	kColorSpace = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
	ctx->pix_fmt = GF_PIXEL_YPVU;
	
	switch (ctx->esd->decoderConfig->objectTypeIndication) {
    case GPAC_OTI_VIDEO_AVC :
		if (ctx->sps && ctx->pps) {
			AVCState avc;
			s32 idx;
			memset(&avc, 0, sizeof(AVCState));
			avc.sps_active_idx = -1;

			idx = gf_media_avc_read_sps(ctx->sps, ctx->sps_size, &avc, 0, NULL);

			ctx->vtb_type = kCMVideoCodecType_H264;
			assert(ctx->sps);
			ctx->width = avc.sps[idx].width;
			ctx->height = avc.sps[idx].height;
			if (avc.sps[idx].vui.par_num && avc.sps[idx].vui.par_den) {
				ctx->pixel_ar = avc.sps[idx].vui.par_num;
				ctx->pixel_ar <<= 16;
				ctx->pixel_ar |= avc.sps[idx].vui.par_den;
			}
			ctx->chroma_format = avc.sps[idx].chroma_format;
			ctx->luma_bit_depth = 8 + avc.sps[idx].luma_bit_depth_m8;
			ctx->chroma_bit_depth = 8 + avc.sps[idx].chroma_bit_depth_m8;
		
			switch (ctx->chroma_format) {
			case 2:
				//422 decoding doesn't seem supported ...
				if (ctx->luma_bit_depth>8) {
					kColorSpace = kCVPixelFormatType_422YpCbCr10;
					ctx->pix_fmt = GF_PIXEL_YUV422_10;
				} else {
					kColorSpace = kCVPixelFormatType_422YpCbCr8;
					ctx->pix_fmt = GF_PIXEL_YUV422;
				}
				break;
			case 3:
				if (ctx->luma_bit_depth>8) {
					kColorSpace = kCVPixelFormatType_444YpCbCr10;
					ctx->pix_fmt = GF_PIXEL_YUV444_10;
				} else {
					kColorSpace = kCVPixelFormatType_444YpCbCr8;
					ctx->pix_fmt = GF_PIXEL_YUV444;
				}
				break;
			default:
				if (ctx->luma_bit_depth>8) {
					kColorSpace = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
					ctx->pix_fmt = GF_PIXEL_YV12_10;
				}
				break;
			}
		
			if (!ctx->esd->decoderConfig->decoderSpecificInfo || force_dsi_rewrite || !ctx->esd->decoderConfig->decoderSpecificInfo->data) {
				GF_AVCConfigSlot *slc_s, *slc_p;
				GF_AVCConfig *cfg = gf_odf_avc_cfg_new();
				cfg->configurationVersion = 1;
				cfg->profile_compatibility = avc.sps[idx].prof_compat;
				cfg->AVCProfileIndication = avc.sps[idx].profile_idc;
				cfg->AVCLevelIndication = avc.sps[idx].level_idc;
				cfg->chroma_format = avc.sps[idx].chroma_format;
				cfg->luma_bit_depth = 8 + avc.sps[idx].luma_bit_depth_m8;
				cfg->chroma_bit_depth = 8 + avc.sps[idx].chroma_bit_depth_m8;
				cfg->nal_unit_size = 4;
				
				GF_SAFEALLOC(slc_s, GF_AVCConfigSlot);
				slc_s->data = ctx->sps;
				slc_s->size = ctx->sps_size;
				gf_list_add(cfg->sequenceParameterSets, slc_s);
				
				GF_SAFEALLOC(slc_p, GF_AVCConfigSlot);
				slc_p->data = ctx->pps;
				slc_p->size = ctx->pps_size;
				gf_list_add(cfg->pictureParameterSets , slc_p);
				
				gf_odf_avc_cfg_write(cfg, &dsi_data, &dsi_data_size);
				slc_s->data = slc_p->data = NULL;
				gf_odf_avc_cfg_del((cfg));
			} else {
				dsi_data = ctx->esd->decoderConfig->decoderSpecificInfo->data;
				dsi_data_size = ctx->esd->decoderConfig->decoderSpecificInfo->dataLength;
			}
			
			dsi = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			data = CFDataCreate(kCFAllocatorDefault, dsi_data, dsi_data_size);
			if (data) {
				CFDictionarySetValue(dsi, CFSTR("avcC"), data);
				CFDictionarySetValue(dec_dsi, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, dsi);
				CFRelease(data);
			}
			CFRelease(dsi);
		
			if (!ctx->esd->decoderConfig->decoderSpecificInfo || !ctx->esd->decoderConfig->decoderSpecificInfo->data) {
				gf_free(ctx->sps);
				ctx->sps = NULL;
				gf_free(ctx->pps);
				ctx->pps = NULL;
				gf_free(dsi_data);
			}
		}
        break;
	case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
	case GPAC_OTI_VIDEO_MPEG2_MAIN:
	case GPAC_OTI_VIDEO_MPEG2_SNR:
	case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
	case GPAC_OTI_VIDEO_MPEG2_HIGH:
	case GPAC_OTI_VIDEO_MPEG2_422:
        ctx->vtb_type = kCMVideoCodecType_MPEG2Video;
		if (!ctx->width || !ctx->height) {
			ctx->init_mpeg12 = GF_TRUE;
			return GF_OK;
		}
		ctx->init_mpeg12 = GF_FALSE;
        break;
		
	case GPAC_OTI_VIDEO_MPEG1:
		ctx->vtb_type = kCMVideoCodecType_MPEG1Video;
		if (!ctx->width || !ctx->height) {
			ctx->init_mpeg12 = GF_TRUE;
			return GF_OK;
		}
		ctx->init_mpeg12 = GF_FALSE;
		break;
    case GPAC_OTI_VIDEO_MPEG4_PART2 :
	{
		Bool reset_dsi = GF_FALSE;
		ctx->vtb_type = kCMVideoCodecType_MPEG4Video;
		if (!ctx->esd->decoderConfig->decoderSpecificInfo) {
			ctx->esd->decoderConfig->decoderSpecificInfo = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
		}

		if (!ctx->esd->decoderConfig->decoderSpecificInfo->data) {
			reset_dsi = GF_TRUE;
			ctx->esd->decoderConfig->decoderSpecificInfo->data = ctx->vosh;
			ctx->esd->decoderConfig->decoderSpecificInfo->dataLength = ctx->vosh_size;
		}
		
		if (ctx->esd->decoderConfig->decoderSpecificInfo->data) {
			GF_M4VDecSpecInfo vcfg;
			GF_BitStream *bs;
			
			gf_m4v_get_config(ctx->esd->decoderConfig->decoderSpecificInfo->data, ctx->esd->decoderConfig->decoderSpecificInfo->dataLength, &vcfg);
			ctx->width = vcfg.width;
			ctx->height = vcfg.height;
			if (ctx->esd->slConfig) {
				ctx->esd->slConfig->predefined  = 2;
			}
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(bs, 0);
			gf_odf_desc_write_bs((GF_Descriptor *) ctx->esd, bs);
			gf_bs_get_content(bs, &dsi_data, &dsi_data_size);
			gf_bs_del(bs);
			
			dsi = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			data = CFDataCreate(kCFAllocatorDefault, dsi_data, dsi_data_size);
			gf_free(dsi_data);
			
			if (data) {
				CFDictionarySetValue(dsi, CFSTR("esds"), data);
				CFDictionarySetValue(dec_dsi, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, dsi);
				CFRelease(data);
			}
			CFRelease(dsi);
			
			if (reset_dsi) {
				ctx->esd->decoderConfig->decoderSpecificInfo->data = NULL;
				ctx->esd->decoderConfig->decoderSpecificInfo->dataLength = 0;
			}
			ctx->skip_mpeg4_vosh = GF_FALSE;
		} else {
			ctx->skip_mpeg4_vosh = GF_TRUE;
			return GF_OK;
		}
        break;
    }
	case GPAC_OTI_MEDIA_GENERIC:
		if (ctx->esd->decoderConfig->decoderSpecificInfo && ctx->esd->decoderConfig->decoderSpecificInfo->dataLength) {
			char *dsi = ctx->esd->decoderConfig->decoderSpecificInfo->data;
			if (ctx->esd->decoderConfig->decoderSpecificInfo->dataLength<8) return GF_NON_COMPLIANT_BITSTREAM;
			if (strnicmp(dsi, "s263", 4)) return GF_NOT_SUPPORTED;
			
			ctx->width = ((u8) dsi[4]); ctx->width<<=8; ctx->width |= ((u8) dsi[5]);
			ctx->height = ((u8) dsi[6]); ctx->height<<=8; ctx->height |= ((u8) dsi[7]);
			ctx->vtb_type = kCMVideoCodecType_H263;
		}
		break;
		
	default :
		return GF_NOT_SUPPORTED;
    }

	if (! ctx->width || !ctx->height) return GF_NOT_SUPPORTED;

    status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault, ctx->vtb_type, ctx->width, ctx->height, dec_dsi, &ctx->fmt_desc);

    if (!ctx->fmt_desc) {
		if (dec_dsi) CFRelease(dec_dsi);
        return GF_NON_COMPLIANT_BITSTREAM;
    }
	buffer_attribs = VTBDec_CreateBufferAttributes(ctx, kColorSpace);
	
	cbacks.decompressionOutputCallback = VTBDec_on_frame;
    cbacks.decompressionOutputRefCon   = ctx;

    dec_type = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dec_type, kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder, kCFBooleanTrue);
	ctx->is_hardware = GF_TRUE;

    status = VTDecompressionSessionCreate(NULL, ctx->fmt_desc, dec_type, buffer_attribs, &cbacks, &ctx->vtb_session);
	//if HW decoder not available, try soft one
	if (status) {
		status = VTDecompressionSessionCreate(NULL, ctx->fmt_desc, NULL, buffer_attribs, &cbacks, &ctx->vtb_session);
		ctx->is_hardware = GF_FALSE;
	}
	
	if (dec_dsi)
		CFRelease(dec_dsi);
	if (dec_type)
		CFRelease(dec_type);
    if (buffer_attribs)
        CFRelease(buffer_attribs);

    switch (status) {
    case kVTVideoDecoderNotAvailableNowErr:
    case kVTVideoDecoderUnsupportedDataFormatErr:
        return GF_NOT_SUPPORTED;
    case kVTVideoDecoderMalfunctionErr:
        return GF_IO_ERR;
    case kVTVideoDecoderBadDataErr :
        return GF_BAD_PARAM;

	case kVTPixelTransferNotSupportedErr:
	case kVTCouldNotFindVideoDecoderErr:
		return GF_NOT_SUPPORTED;
    case 0:
        break;
    default:
        return GF_SERVICE_ERROR;
    }
	
	//good to go !
	if (ctx->pix_fmt == GF_PIXEL_YUV422) {
		ctx->out_size = ctx->width*ctx->height*2;
	} else if (ctx->pix_fmt == GF_PIXEL_YUV444) {
		ctx->out_size = ctx->width*ctx->height*3;
	} else {
		// (ctx->pix_fmt == GF_PIXEL_YV12)
		ctx->out_size = ctx->width*ctx->height*3/2;
	}
	if (ctx->luma_bit_depth>8) {
		ctx->out_size *= 2;
	}
	ctx->frame_size_changed = GF_TRUE;
	return GF_OK;
}

static GF_Err VTBDec_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	GF_Err e;
	VTBDec *ctx = (VTBDec *)ifcg->privateStack;
	ctx->esd = esd;

	//check AVC config
	if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) {
		if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
			ctx->is_annex_b = GF_TRUE;
			ctx->width=ctx->height=128;
			ctx->out_size = ctx->width*ctx->height*3/2;
			ctx->pix_fmt = GF_PIXEL_YV12;
			return GF_OK;
		} else {
			GF_AVCConfigSlot *slc;
			GF_AVCConfig *cfg = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			slc = gf_list_get(cfg->sequenceParameterSets, 0);
			if (slc) {
				ctx->sps = slc->data;
				ctx->sps_size = slc->size;
			}
			slc = gf_list_get(cfg->pictureParameterSets, 0);
			if (slc) {
				ctx->pps = slc->data;
				ctx->pps_size = slc->size;
			}
			
			if (ctx->sps && ctx->pps) {
				e = VTBDec_InitDecoder(ctx, GF_FALSE);
			} else {
				ctx->nalu_size_length = cfg->nal_unit_size;
				e = GF_OK;
			}
			gf_odf_avc_cfg_del(cfg);
			return e;
		}
	}

	//check VOSH config
	if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) {
		if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
			ctx->width=ctx->height=128;
			ctx->out_size = ctx->width*ctx->height*3/2;
			ctx->pix_fmt = GF_PIXEL_YV12;
		} else {
			return VTBDec_InitDecoder(ctx, GF_FALSE);
		}
	}

	return VTBDec_InitDecoder(ctx, GF_FALSE);
}

static GF_Err VTBDec_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	VTBDec *ctx = (VTBDec *)ifcg->privateStack;
	if (ctx->fmt_desc) {
		CFRelease(ctx->fmt_desc);
		ctx->fmt_desc = NULL;
	}
	if (ctx->vtb_session) {
		VTDecompressionSessionInvalidate(ctx->vtb_session);
		ctx->vtb_session=NULL;
	}
	return GF_OK;
}
static GF_Err VTBDec_GetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability *capability)
{
	VTBDec *ctx = (VTBDec *)ifcg->privateStack;
	
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
		capability->cap.valueInt = ctx->width;
		break;
	case GF_CODEC_FPS:
		capability->cap.valueFloat = 30.0;
		break;
	case GF_CODEC_PAR:
		capability->cap.valueInt = ctx->pixel_ar;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = ctx->out_size;
		break;
	case GF_CODEC_PIXEL_FORMAT:
		capability->cap.valueInt = ctx->pix_fmt;
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_BUFFER_MAX:
		//since we do the temporal de-interleaving ask for more CUs to avoid displaying refs before reordered frames
		capability->cap.valueInt = 6;
		break;
	/*by default we use 4 bytes padding (otherwise it happens that XviD crashes on some videos...)*/
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 0;
		break;
	/*reorder is up to us*/
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_WANTS_THREAD:
		capability->cap.valueInt = 0;
		break;
	case GF_CODEC_FRAME_OUTPUT:
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
static GF_Err VTBDec_SetCapabilities(GF_BaseDecoder *ifcg, GF_CodecCapability capability)
{
	VTBDec *ctx = (VTBDec *)ifcg->privateStack;
	
	switch (capability.CapCode) {
	case GF_CODEC_FRAME_OUTPUT:
		ctx->raw_frame_dispatch = capability.cap.valueInt ? GF_TRUE : GF_FALSE;
#ifdef GPAC_IPHONE
		if (ctx->raw_frame_dispatch && (capability.cap.valueInt==2))
			ctx->use_gl_textures = GF_TRUE;
#endif
		return GF_OK;
	}

	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;
}

static GF_Err VTB_RewriteNALs(VTBDec *ctx, char *inBuffer, u32 inBufferLength, char **out_buffer, u32 *out_size)
{
	u32 i, sc_size;
	char *ptr = inBuffer;
	u32 nal_size;
	GF_Err e = GF_OK;
	GF_BitStream *bs = NULL;
	
	*out_buffer = NULL;
	*out_size = 0;
	
	if (!ctx->nalu_size_length) {
		nal_size = gf_media_nalu_next_start_code((u8 *) inBuffer, inBufferLength, &sc_size);
		if (!sc_size) return GF_NON_COMPLIANT_BITSTREAM;
		ptr += nal_size + sc_size;
		assert(inBufferLength >= nal_size + sc_size);
		inBufferLength -= nal_size + sc_size;
	}
	
	while (inBufferLength) {
		Bool add_nal = GF_TRUE;
		u8 nal_type;
		
		if (ctx->nalu_size_length) {
			for (i=0; i<ctx->nalu_size_length; i++) {
				nal_size = (nal_size<<8) + ptr[i];
			}
			ptr += ctx->nalu_size_length;
		} else {
			nal_size = gf_media_nalu_next_start_code(ptr, inBufferLength, &sc_size);
		}
		
		nal_type = ptr[0] & 0x1F;
		switch (nal_type) {
		case GF_AVC_NALU_SEQ_PARAM:
			if (!ctx->vtb_session) {
				ctx->sps = gf_malloc(sizeof(char)*nal_size);
				memcpy(ctx->sps, ptr, sizeof(char)*nal_size);
				ctx->sps_size=nal_size;
				add_nal = GF_FALSE;
			}
			break;
		case GF_AVC_NALU_PIC_PARAM:
			if (!ctx->vtb_session) {
				ctx->pps = gf_malloc(sizeof(char)*nal_size);
				memcpy(ctx->pps, ptr, sizeof(char)*nal_size);
				ctx->pps_size=nal_size;
				add_nal = GF_FALSE;
			}
			break;
		case GF_AVC_NALU_ACCESS_UNIT:
		case GF_AVC_NALU_END_OF_SEQ:
		case GF_AVC_NALU_END_OF_STREAM:
		case GF_AVC_NALU_FILLER_DATA:
			add_nal = GF_FALSE;
			break;
		default:
			break;
		}
		
		//if sps and pps are ready, init decoder
		if (!ctx->vtb_session && ctx->sps && ctx->pps) {
			e = VTBDec_InitDecoder(ctx, GF_TRUE);
			if (e) return e;
		}
		
		if (add_nal && !ctx->vtb_session) add_nal = GF_FALSE;

		if (add_nal) {
			if (!bs) bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			
			gf_bs_write_u32(bs, nal_size);
			gf_bs_write_data(bs, ptr, nal_size);
		}
		
		ptr += nal_size;
		if (ctx->nalu_size_length) {
			if (inBufferLength < nal_size + ctx->nalu_size_length) break;
			inBufferLength -= nal_size + ctx->nalu_size_length;
		} else {
			if (!sc_size || (inBufferLength < nal_size + sc_size)) break;
			inBufferLength -= nal_size + sc_size;
			ptr += sc_size;
		}
	}
	
	if (bs) {
		gf_bs_get_content(bs, out_buffer, out_size);
		gf_bs_del(bs);
	}
	return e;
}

static GF_Err VTBDec_ProcessData(GF_MediaDecoder *ifcg,
                               char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 *CTS,
                               char *outBuffer, u32 *outBufferLength,
                               u8 PaddingBits, u32 mmlevel)
{
    OSStatus status;
    CMSampleBufferRef sample = NULL;
    CMBlockBufferRef block_buffer = NULL;
	OSType type;
	char *in_data;
	u32 in_data_size;
	
	GF_Err e;
	VTBDec *ctx = (VTBDec *)ifcg->privateStack;
	
	if (ctx->skip_mpeg4_vosh) {
		GF_M4VDecSpecInfo dsi;
		dsi.width = dsi.height = 0;
		e = gf_m4v_get_config(inBuffer, inBufferLength, &dsi);
		//found a vosh - remove it from payload, init decoder if needed
		if ((e==GF_OK) && dsi.width && dsi.height) {
			if (!ctx->vtb_session) {
				ctx->vosh = inBuffer;
				ctx->vosh_size = dsi.next_object_start;
				e = VTBDec_InitDecoder(ctx, GF_FALSE);
				if (e) return e;

				//enfoce removal for all frames
				ctx->skip_mpeg4_vosh = GF_TRUE;
				
				if (!ctx->raw_frame_dispatch && (ctx->out_size != *outBufferLength)) {
					*outBufferLength = ctx->out_size;
					return GF_BUFFER_TOO_SMALL;
				}
			}
			ctx->vosh_size = dsi.next_object_start;
		} else if (!ctx->vtb_session) {
			*outBufferLength=0;
			return GF_OK;
		}
	}

	if (ctx->init_mpeg12) {
		GF_M4VDecSpecInfo dsi;
		dsi.width = dsi.height = 0;
		
		e = gf_mpegv12_get_config(inBuffer, inBufferLength, &dsi);
		if ((e==GF_OK) && dsi.width && dsi.height) {
			ctx->width = dsi.width;
			ctx->height = dsi.height;
			ctx->pixel_ar = dsi.par_num;
			ctx->pixel_ar <<= 16;
			ctx->pixel_ar |= dsi.par_den;
			
			e = VTBDec_InitDecoder(ctx, GF_FALSE);
			if (e) return e;

			if (!ctx->raw_frame_dispatch && (ctx->out_size != *outBufferLength)) {
				*outBufferLength = ctx->out_size;
				return GF_BUFFER_TOO_SMALL;
			}
		}

		if (!ctx->vtb_session) {
			*outBufferLength=0;
			return GF_OK;
		}
	}

	if (ctx->is_annex_b || (!ctx->vtb_session && ctx->nalu_size_length) ) {
		if (ctx->cached_annex_b) {
			in_data = ctx->cached_annex_b;
			in_data_size = ctx->cached_annex_b_size;
			ctx->cached_annex_b = NULL;
		} else {
			e = VTB_RewriteNALs(ctx, inBuffer, inBufferLength, &in_data, &in_data_size);
			if (e) return e;
		}
		
		if (!ctx->raw_frame_dispatch && (ctx->out_size != *outBufferLength)) {
			*outBufferLength = ctx->out_size;
			ctx->cached_annex_b = in_data;
			ctx->cached_annex_b_size = in_data_size;

			return GF_BUFFER_TOO_SMALL;
		}
	} else if (ctx->vosh_size) {
		in_data = inBuffer + ctx->vosh_size;
		in_data_size = inBufferLength - ctx->vosh_size;
		ctx->vosh_size = 0;
	} else {
		in_data = inBuffer;
		in_data_size = inBufferLength;
	}
	
	if (!ctx->vtb_session) {
		*outBufferLength=0;
		return GF_OK;
	}
	

	status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, in_data, in_data_size, kCFAllocatorNull, NULL, 0, in_data_size, 0, &block_buffer);

	if (status) {
		return GF_IO_ERR;
	}

	*outBufferLength=0;
	if (block_buffer == NULL)
		return GF_OK;
		
	
	status = CMSampleBufferCreate(kCFAllocatorDefault, block_buffer, TRUE, NULL, NULL, ctx->fmt_desc, 1, 0, NULL, 0, NULL, &sample);

    if (status || (sample==NULL)) {
		if (block_buffer)
			CFRelease(block_buffer);
		return GF_IO_ERR;
	}
	ctx->last_error = GF_OK;
    status = VTDecompressionSessionDecodeFrame(ctx->vtb_session, sample, 0, NULL, 0);
    if (!status)
		status = VTDecompressionSessionWaitForAsynchronousFrames(ctx->vtb_session);
	

	CFRelease(block_buffer);
	CFRelease(sample);
	if (ctx->cached_annex_b)
		gf_free(in_data);
	
	if (ctx->last_error) return ctx->last_error;
	if (status) return GF_NON_COMPLIANT_BITSTREAM;
	
	if (!ctx->frame) {
		*outBufferLength=0;
		return ctx->last_error;
	}
	
	*outBufferLength = ctx->out_size;
	if (ctx->raw_frame_dispatch) return GF_OK;

	status = CVPixelBufferLockBaseAddress(ctx->frame, kCVPixelBufferLock_ReadOnly);
    if (status != kCVReturnSuccess) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error locking frame data\n"));
        return GF_IO_ERR;
    }

	type = CVPixelBufferGetPixelFormatType(ctx->frame);
	
	if ((type==kCVPixelFormatType_420YpCbCr8Planar)
		|| (type==kCVPixelFormatType_420YpCbCr8PlanarFullRange)
		|| (type==kCVPixelFormatType_422YpCbCr8_yuvs)
		|| (type==kCVPixelFormatType_444YpCbCr8)
		|| (type=='444v')
		|| (type==kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
		|| (type==kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
	) {
        u32 i, j, nb_planes = (u32) CVPixelBufferGetPlaneCount(ctx->frame);
		char *dst = outBuffer;
		Bool needs_stride=GF_FALSE;
		u32 stride = (u32) CVPixelBufferGetBytesPerRowOfPlane(ctx->frame, 0);
			
		//TOCHECK - for now the 3 planes are consecutive in VideoToolbox
		if (stride==ctx->width) {
			char *data = CVPixelBufferGetBaseAddressOfPlane(ctx->frame, 0);
			memcpy(dst, data, sizeof(char)*ctx->out_size);
		} else {
			for (i=0; i<nb_planes; i++) {
				char *data = CVPixelBufferGetBaseAddressOfPlane(ctx->frame, i);
				u32 stride = (u32) CVPixelBufferGetBytesPerRowOfPlane(ctx->frame, i);
				u32 w, h = (u32) CVPixelBufferGetHeightOfPlane(ctx->frame, i);
				w = ctx->width;
				if (i) {
					switch (ctx->pix_fmt) {
					case GF_PIXEL_YUV444:
						break;
					case GF_PIXEL_YUV422:
					case GF_PIXEL_YV12:
						w /= 2;
						break;
					}
				}
				if (stride != w) {
					needs_stride=GF_TRUE;
					for (j=0; j<h; j++) {
						memcpy(dst, data, sizeof(char)*w);
						dst += w;
						data += stride;
					}
				} else {
					memcpy(dst, data, sizeof(char)*h*stride);
					dst += sizeof(char)*h*stride;
				}
			}
		}
	}
    CVPixelBufferUnlockBaseAddress(ctx->frame, kCVPixelBufferLock_ReadOnly);

	return GF_OK;
}

typedef struct
{
	Bool locked;
	CVPixelBufferRef frame;
	VTBDec *ctx;
	
	//openGL mode
#ifdef GPAC_IPHONE
	CVOpenGLESTextureRef y, u, v;
#endif
} VTB_Frame;

void VTBFrame_Release(GF_MediaDecoderFrame *frame)
{
	VTB_Frame *f = (VTB_Frame *)frame->user_data;
	if (f->locked) {
		CVPixelBufferUnlockBaseAddress(f->frame, kCVPixelBufferLock_ReadOnly);
	}
#ifdef GPAC_IPHONE
	if (f->y) CVBufferRelease(f->y);
	if (f->u) CVBufferRelease(f->u);
	if (f->v) CVBufferRelease(f->v);
	if (f->ctx->cache_texture)
		CVOpenGLESTextureCacheFlush(f->ctx->cache_texture, 0);
#endif
	
	if (f->frame) {
        CVPixelBufferRelease(f->frame);
    }
	
	gf_free(f);
	gf_free(frame);
}

GF_Err VTBFrame_GetPlane(GF_MediaDecoderFrame *frame, u32 plane_idx, const char **outPlane, u32 *outStride)
{
    OSStatus status;
	GF_Err e;
	VTB_Frame *f = (VTB_Frame *)frame->user_data;
	if (! outPlane || !outStride) return GF_BAD_PARAM;
	*outPlane = NULL;
	*outStride = 0;

//	if (!f->locked) {
		status = CVPixelBufferLockBaseAddress(f->frame, kCVPixelBufferLock_ReadOnly);
		if (status != kCVReturnSuccess) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error locking frame data\n"));
			return GF_IO_ERR;
		}
//		f->locked = GF_TRUE;
//	}
	e = GF_OK;
	
    if (CVPixelBufferIsPlanar(f->frame)) {
		*outStride = (u32) CVPixelBufferGetBytesPerRowOfPlane(f->frame, plane_idx);
		*outPlane = CVPixelBufferGetBaseAddressOfPlane(f->frame, plane_idx);
	} else if (plane_idx==0) {
		*outStride = (u32) CVPixelBufferGetBytesPerRow(f->frame);
		*outPlane = CVPixelBufferGetBaseAddress(f->frame);
	} else {
		e = GF_BAD_PARAM;
	}
		CVPixelBufferUnlockBaseAddress(f->frame, kCVPixelBufferLock_ReadOnly);

	return e;
}

#ifdef GPAC_IPHONE

void *myGetGLContext();

GF_Err VTBFrame_GetGLTexture(GF_MediaDecoderFrame *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id)
{
    OSStatus status;
	GLenum target_fmt;
	u32 w, h;
	CVOpenGLESTextureRef *outTexture;
	VTB_Frame *f = (VTB_Frame *)frame->user_data;
	if (! gl_tex_format || !gl_tex_id) return GF_BAD_PARAM;
	*gl_tex_format = 0;
	*gl_tex_id = 0;

	if (!f->ctx->gl_context) {
		f->ctx->gl_context = myGetGLContext();
		if (!f->ctx->gl_context) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error locating current GL context\n"));
			return GF_IO_ERR;
		}
	}
	
	if (!f->ctx->cache_texture) {
		status = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, f->ctx->gl_context, NULL, &f->ctx->cache_texture);
		if (status != kCVReturnSuccess) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error creating cache texture\n"));
			return GF_IO_ERR;
		}
	}
	
	if (!f->locked) {
		status = CVPixelBufferLockBaseAddress(f->frame, kCVPixelBufferLock_ReadOnly);
		if (status != kCVReturnSuccess) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error locking frame data\n"));
			return GF_IO_ERR;
		}
		f->locked = GF_TRUE;
	}
	
	if (plane_idx >= (u32) CVPixelBufferGetPlaneCount(f->frame)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Wrong plane index\n"));
		return GF_BAD_PARAM;
	}

	target_fmt = GL_LUMINANCE;
	w = f->ctx->width;
	h = f->ctx->height;
	if (plane_idx) {
		w /= 2;
		h /= 2;
		target_fmt = GL_LUMINANCE_ALPHA;
	}
	if (plane_idx==0) {
		outTexture = &f->y;
	}
	else if (plane_idx==1) {
		outTexture = &f->u;
	}
	status = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault, f->ctx->cache_texture, f->frame, NULL, GL_TEXTURE_2D, target_fmt, w, h, target_fmt, GL_UNSIGNED_BYTE, plane_idx, outTexture);
	
	if (status != kCVReturnSuccess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error creating cache texture for plane %d\n", plane_idx));
		return GF_IO_ERR;
	}
	*gl_tex_format = CVOpenGLESTextureGetTarget(*outTexture);
	*gl_tex_id = CVOpenGLESTextureGetName(*outTexture);

	return GF_OK;
}
#endif

GF_Err VTBDec_GetOutputFrame(GF_MediaDecoder *dec, u16 ES_ID, GF_MediaDecoderFrame **frame, Bool *needs_resize)
{
	GF_MediaDecoderFrame *a_frame;
	VTB_Frame *vtb_frame;
	VTBDec *ctx = (VTBDec *)dec->privateStack;
	
	*needs_resize = GF_FALSE;
	
	if (!ctx->frame) return GF_BAD_PARAM;
	
	GF_SAFEALLOC(a_frame, GF_MediaDecoderFrame);
	if (!a_frame) return GF_OUT_OF_MEM;
	GF_SAFEALLOC(vtb_frame, VTB_Frame);
	if (!vtb_frame) {
		gf_free(a_frame);
		return GF_OUT_OF_MEM;
	}
	a_frame->user_data = vtb_frame;
	vtb_frame->ctx = ctx;
	vtb_frame->frame = ctx->frame;
	ctx->frame = NULL;
	a_frame->Release = VTBFrame_Release;
	a_frame->GetPlane = VTBFrame_GetPlane;
#ifdef GPAC_IPHONE
	if (ctx->use_gl_textures)
		a_frame->GetGLTexture = VTBFrame_GetGLTexture;
#endif

	*frame = a_frame;
	if (ctx->frame_size_changed) {
		ctx->frame_size_changed = GF_FALSE;
		*needs_resize = GF_TRUE;
	}
	return GF_OK;
}

static u32 VTBDec_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;
	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_VIDEO_AVC:
		if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
			GF_AVCConfig *cfg = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			Bool cp_ok = GF_TRUE;
			if (!cfg->chroma_format) {
				GF_AVCConfigSlot *s = gf_list_get(cfg->sequenceParameterSets, 0);
				if (s) {
					AVCState avc;
					s32 idx;
					memset(&avc, 0, sizeof(AVCState));
					avc.sps_active_idx = -1;
					idx = gf_media_avc_read_sps(s->data, s->size, &avc, 0, NULL);
					cfg->chroma_format = avc.sps[idx].chroma_format;
					cfg->luma_bit_depth = 8 + avc.sps[idx].luma_bit_depth_m8;
					cfg->chroma_bit_depth = 8 + avc.sps[idx].chroma_bit_depth_m8;
				}
			}
			if ((cfg->chroma_bit_depth>8) || (cfg->luma_bit_depth > 8) || (cfg->chroma_format>1)) {
				cp_ok = GF_FALSE;
			}
			gf_odf_avc_cfg_del(cfg);
			if (!cp_ok) return GF_CODEC_PROFILE_NOT_SUPPORTED;
		}
		return GF_CODEC_SUPPORTED * 2;

	case GPAC_OTI_VIDEO_MPEG4_PART2:
		return GF_CODEC_SUPPORTED * 2;

	case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
	case GPAC_OTI_VIDEO_MPEG2_MAIN:
	case GPAC_OTI_VIDEO_MPEG2_SNR:
	case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
	case GPAC_OTI_VIDEO_MPEG2_HIGH:
	case GPAC_OTI_VIDEO_MPEG2_422:
		//not supported on iphone
#ifdef GPAC_IPHONE
		return GF_CODEC_NOT_SUPPORTED;
#else
		return GF_CODEC_SUPPORTED * 2;
#endif

	//cannot make it work on ios and OSX version seems buggy (wrong frame output order)
//	case GPAC_OTI_VIDEO_MPEG1:
//		return GF_CODEC_SUPPORTED * 2;

	case GPAC_OTI_MEDIA_GENERIC:
		if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->dataLength) {
			char *dsi = esd->decoderConfig->decoderSpecificInfo->data;
			if (!strnicmp(dsi, "s263", 4)) return GF_CODEC_SUPPORTED*2;
		}
	}
	return GF_CODEC_NOT_SUPPORTED;
}

static const char *VTBDec_GetCodecName(GF_BaseDecoder *dec)
{
	VTBDec *ctx = (VTBDec *)dec->privateStack;
	switch (ctx->vtb_type) {
	case kCMVideoCodecType_H264:
		return ctx->is_hardware ? "VTB hardware AVC|H264" : "VTB software AVC|H264";
	case kCMVideoCodecType_MPEG2Video:
		return ctx->is_hardware ? "VTB hardware MPEG-2" : "VTB software MPEG-2";
    case  kCMVideoCodecType_MPEG4Video:
		return ctx->is_hardware ? "VTB hardware MPEG-4 Part2" : "VTB software MPEG-4 Part2";
    case kCMVideoCodecType_H263:
		return ctx->is_hardware ? "VTB hardware H263" : "VTB software H263";
	case kCMVideoCodecType_MPEG1Video:
		return ctx->is_hardware ? "VTB hardware MPEG-1" : "VTB software MPEG-1";
	default:
		return "VideoToolbox unknown Decoder";
	}
}

GF_BaseDecoder *NewVTBDec()
{
	GF_MediaDecoder *ifcd;
	VTBDec *dec;

	GF_SAFEALLOC(ifcd, GF_MediaDecoder);
	if (!ifcd) return NULL;
	GF_SAFEALLOC(dec, VTBDec);
	if (!dec) {
		gf_free(ifcd);
		return NULL;
	}
	GF_REGISTER_MODULE_INTERFACE(ifcd, GF_MEDIA_DECODER_INTERFACE, "VideoToolbox Decoder", "gpac distribution")

	ifcd->privateStack = dec;

	/*setup our own interface*/
	ifcd->AttachStream = VTBDec_AttachStream;
	ifcd->DetachStream = VTBDec_DetachStream;
	ifcd->GetCapabilities = VTBDec_GetCapabilities;
	ifcd->SetCapabilities = VTBDec_SetCapabilities;
	ifcd->GetName = VTBDec_GetCodecName;
	ifcd->CanHandleStream = VTBDec_CanHandleStream;
	ifcd->ProcessData = VTBDec_ProcessData;
	ifcd->GetOutputFrame = VTBDec_GetOutputFrame;
	return (GF_BaseDecoder *) ifcd;
}

void DeleteVTBDec(GF_BaseDecoder *ifcg)
{
	VTBDec *ctx = (VTBDec *)ifcg->privateStack;

#ifdef GPAC_IPHONE
	if (ctx->cache_texture) {
		CFRelease(ctx->cache_texture);
    }
#endif
	gf_free(ctx);
	gf_free(ifcg);
}
#endif


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
	if (InterfaceType == GF_MEDIA_DECODER_INTERFACE) return (GF_BaseInterface *)NewVTBDec();
#endif
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_MEDIA_DECODER_INTERFACE:
		DeleteVTBDec((GF_BaseDecoder*)ifce);
		break;
#endif
	}
}

GPAC_MODULE_STATIC_DECLARATION( vtb )
