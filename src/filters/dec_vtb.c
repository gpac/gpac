/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / VideoToolBox decoder filter
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

//do not include math.h we would have a conflict with Fixed ... we're lucky we don't need maths routines here
#define _GF_MATH_H_

#include <gpac/setup.h>

#if !defined(GPAC_DISABLE_AV_PARSERS) && ( defined(GPAC_CONFIG_DARWIN) || defined(GPAC_IPHONE) )

#include <stdint.h>

#define Picture QuickdrawPicture
#include <VideoToolbox/VideoToolbox.h>
#undef Picture

#ifndef kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder
#  define kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder CFSTR("RequireHardwareAcceleratedVideoDecoder")
#endif

#include <gpac/filters.h>
#include <gpac/maths.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>

#include "../../src/compositor/gl_inc.h"

#ifndef GPAC_DISABLE_AV_PARSERS

typedef struct
{
	//opts
	u32 reorder;
	Bool no_copy;
	Bool disable_hw;

	//internal
	GF_FilterPid *ipid, *opid;
	u32 width, height;
	GF_Fraction pixel_ar;
	u32 pix_fmt;
	u32 out_size;
	u32 cfg_crc;
	u32 oti;
	Bool is_hardware;

	GF_Err last_error;
	
	int vtb_type;
	VTDecompressionSessionRef vtb_session;
    CMFormatDescriptionRef fmt_desc;

    GF_List *frames, *frames_res;
    GF_FilterPacket *cur_pck;

	u8 chroma_format, luma_bit_depth, chroma_bit_depth;
	Bool frame_size_changed;
	Bool reorder_detected;

	u32 decoded_frames_pending;
	u32 reorder_probe;
	Bool reconfig_needed;
	u64 last_cts_out;
	u32 last_timescale_out;

	//MPEG-1/2 specific
	Bool init_mpeg12;
	
	//MPEG-4 specific
	Bool skip_mpeg4_vosh;
	char *vosh;
	u32 vosh_size;
	
	//NAL-based specific
	Bool is_avc;
	Bool is_annex_b;
	char *cached_annex_b;
	u32 cached_annex_b_size;
	u32 nalu_size_length;
	GF_List *SPSs, *PPSs;
	s32 active_sps, active_pps;
	u32 active_sps_crc, active_pps_crc;
	AVCState avc;
	Bool check_h264_isma;

	//openGL output
#ifdef GPAC_IPHONE
	Bool use_gl_textures;
	CVOpenGLESTextureCacheRef cache_texture;
#endif
	void *gl_context;
} GF_VTBDecCtx;


typedef struct
{
	GF_FilterHWFrame hw_frame;

	Bool locked;
	CVPixelBufferRef frame;
	GF_VTBDecCtx *ctx;
	u64 cts;
	u32 duration;
	u32 timescale;
	//openGL mode
#ifdef GPAC_IPHONE
	CVOpenGLESTextureRef y, u, v;
#endif
} GF_VTBHWFrame;

static void vtbdec_delete_decoder(GF_VTBDecCtx *ctx);

static void vtbdec_on_frame(void *opaque, void *sourceFrameRefCon, OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef image, CMTime pts, CMTime duration)
{
	GF_VTBDecCtx *ctx = (GF_VTBDecCtx *)opaque;
	GF_VTBHWFrame *frame;
	u32 i, count;
	assert(ctx->cur_pck);

	if (status != kCVReturnSuccess) {
		ctx->last_error = GF_NON_COMPLIANT_BITSTREAM;
        GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Decode error - status %d\n", status));
	}
    if (!image) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[VTB] No output buffer - status %d\n", status));
        return;
    }
	frame = gf_list_pop_back(ctx->frames_res);
	if (!frame) {
		GF_SAFEALLOC(frame, GF_VTBHWFrame);
	} else {
		memset(frame, 0, sizeof(GF_VTBHWFrame));
	}
	frame->hw_frame.user_data = frame;
	frame->frame = CVPixelBufferRetain(image);
	frame->cts = gf_filter_pck_get_cts(ctx->cur_pck);
	frame->duration = gf_filter_pck_get_duration(ctx->cur_pck);
	frame->timescale = gf_filter_pck_get_timescale(ctx->cur_pck);
	frame->ctx = ctx;

	count = gf_list_count(ctx->frames);
	for (i=0; i<count; i++) {
		GF_VTBHWFrame *aframe = gf_list_get(ctx->frames, i);
		Bool insert = GF_FALSE;
		s64 diff;
		if ((frame->timescale == aframe->timescale) && (ctx->last_timescale_out == frame->timescale)) {
			diff = (s64) aframe->cts - (s64) frame->cts;
			if ((diff>0) && (frame->cts > frame->timescale) ) {
				insert = GF_TRUE;
			}
		} else {
			diff = (s64) (aframe->cts * frame->timescale);
			diff = - (s64) (frame->cts * aframe->timescale);
			if ((diff>0) && (ctx->last_timescale_out * frame->cts > frame->timescale * ctx->last_cts_out) ) {
				insert = GF_TRUE;
			}
		}
		if (insert) {
			gf_list_insert(ctx->frames, frame, i);
			ctx->reorder_detected = GF_TRUE;
			return;
		}
	}
	gf_list_add(ctx->frames, frame);
}

static CFDictionaryRef vtbdec_create_buffer_attributes(GF_VTBDecCtx *ctx, OSType pix_fmt)
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

static GF_Err vtbdec_init_decoder(GF_Filter *filter, GF_VTBDecCtx *ctx)
{
	CFMutableDictionaryRef dec_dsi, dec_type;
	CFMutableDictionaryRef dsi;
	VTDecompressionOutputCallbackRecord cbacks;
    CFDictionaryRef buffer_attribs;
    OSStatus status;
	OSType kColorSpace;
	const GF_PropertyValue *p;
	CFDataRef data = NULL;
	char *dsi_data=NULL;
	u32 dsi_data_size=0;
	
    dec_dsi = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
//	kColorSpace = kCVPixelFormatType_420YpCbCr8Planar;
//	ctx->pix_fmt = GF_PIXEL_YV12;
	
	kColorSpace = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
	ctx->pix_fmt = GF_PIXEL_NV12;
	ctx->reorder_probe = ctx->reorder;
	ctx->reorder_detected = GF_FALSE;

	p = gf_filter_pid_get_property(ctx->ipid, GF_PROP_PID_DECODER_CONFIG);

	switch (ctx->oti) {
    case GPAC_OTI_VIDEO_AVC :
		if (gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs)) {
			s32 idx;
			u32 i;
			GF_AVCConfig *cfg;
			GF_AVCConfigSlot *sps = NULL;
			GF_AVCConfigSlot *pps = NULL;

			for (i=0; i<gf_list_count(ctx->SPSs); i++) {
				sps = gf_list_get(ctx->SPSs, i);
				if (ctx->active_sps<0) ctx->active_sps = sps->id;

				if (sps->id==ctx->active_sps) {
					ctx->active_sps_crc = gf_crc_32(sps->data, sps->size);
					break;
				}
				sps = NULL;
			}
			if (!sps) return GF_NON_COMPLIANT_BITSTREAM;
			for (i=0; i<gf_list_count(ctx->PPSs); i++) {
				pps = gf_list_get(ctx->PPSs, i);
				if (ctx->active_pps<0) ctx->active_pps = pps->id;

				if (pps->id==ctx->active_pps) {
					ctx->active_pps_crc = gf_crc_32(pps->data, pps->size);
					break;
				}
				pps = NULL;
			}
			if (!pps) return GF_NON_COMPLIANT_BITSTREAM;
			ctx->reconfig_needed = GF_FALSE;
			
			ctx->vtb_type = kCMVideoCodecType_H264;

			idx = ctx->active_sps;
			ctx->width = ctx->avc.sps[idx].width;
			ctx->height = ctx->avc.sps[idx].height;
			if (ctx->avc.sps[idx].vui.par_num && ctx->avc.sps[idx].vui.par_den) {
				ctx->pixel_ar.num = ctx->avc.sps[idx].vui.par_num;
				ctx->pixel_ar.den = ctx->avc.sps[idx].vui.par_den;
			}
			ctx->chroma_format = ctx->avc.sps[idx].chroma_format;
			ctx->luma_bit_depth = 8 + ctx->avc.sps[idx].luma_bit_depth_m8;
			ctx->chroma_bit_depth = 8 + ctx->avc.sps[idx].chroma_bit_depth_m8;
		
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
			//always rewrite with current sps and pps
			cfg = gf_odf_avc_cfg_new();
			cfg->configurationVersion = 1;
			cfg->profile_compatibility = ctx->avc.sps[idx].prof_compat;
			cfg->AVCProfileIndication = ctx->avc.sps[idx].profile_idc;
			cfg->AVCLevelIndication = ctx->avc.sps[idx].level_idc;
			cfg->chroma_format = ctx->avc.sps[idx].chroma_format;
			cfg->luma_bit_depth = 8 + ctx->avc.sps[idx].luma_bit_depth_m8;
			cfg->chroma_bit_depth = 8 + ctx->avc.sps[idx].chroma_bit_depth_m8;
			cfg->nal_unit_size = 4;
				
			gf_list_add(cfg->sequenceParameterSets, sps);
			gf_list_add(cfg->pictureParameterSets, pps);
				
			gf_odf_avc_cfg_write(cfg, &dsi_data, &dsi_data_size);
			gf_list_reset(cfg->sequenceParameterSets);
			gf_list_reset(cfg->pictureParameterSets);
			gf_odf_avc_cfg_del((cfg));
			
			dsi = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			data = CFDataCreate(kCFAllocatorDefault, (const UInt8*)dsi_data, dsi_data_size);
			if (data) {
				CFDictionarySetValue(dsi, CFSTR("avcC"), data);
				CFDictionarySetValue(dec_dsi, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, dsi);
				CFRelease(data);
			}
			CFRelease(dsi);
		
			gf_free(dsi_data);
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
		char *vosh = NULL;
		u32 vosh_size = 0;
		ctx->vtb_type = kCMVideoCodecType_MPEG4Video;

		if (!p || !p->value.data) {
			reset_dsi = GF_TRUE;
			vosh = ctx->vosh;
			vosh_size = ctx->vosh_size;
		} else {
			vosh = p->data_len;
			vosh_size = p->data_len;
		}
		
		if (vosh) {
			GF_M4VDecSpecInfo vcfg;
			GF_BitStream *bs;
			GF_ESD *esd;

			gf_m4v_get_config(vosh, vosh_size, &vcfg);
			ctx->width = vcfg.width;
			ctx->height = vcfg.height;
			esd = gf_odf_desc_esd_new(2);
			esd->decoderConfig->decoderSpecificInfo->data = vosh;
			esd->decoderConfig->decoderSpecificInfo->dataLength = vosh_size;
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(bs, 0);
			gf_odf_desc_write_bs((GF_Descriptor *) esd, bs);
			gf_bs_get_content(bs, &dsi_data, &dsi_data_size);
			gf_bs_del(bs);
			esd->decoderConfig->decoderSpecificInfo->data = NULL;
			esd->decoderConfig->decoderSpecificInfo->dataLength = 0;
			gf_odf_desc_del((GF_Descriptor*)esd);

			dsi = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			data = CFDataCreate(kCFAllocatorDefault, (const UInt8*) dsi_data, dsi_data_size);
			gf_free(dsi_data);
			
			if (data) {
				CFDictionarySetValue(dsi, CFSTR("esds"), data);
				CFDictionarySetValue(dec_dsi, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, dsi);
				CFRelease(data);
			}
			CFRelease(dsi);
			
			ctx->skip_mpeg4_vosh = GF_FALSE;
		} else {
			ctx->skip_mpeg4_vosh = GF_TRUE;
			return GF_OK;
		}
        break;
    }
	case GPAC_OTI_VIDEO_H263:
			ctx->reorder_probe = 0;
	case GPAC_OTI_MEDIA_GENERIC:
		if (p && p->value.data && p->data_len) {
			char *dsi = p->value.data;
			if (p->data_len<8) return GF_NON_COMPLIANT_BITSTREAM;
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
	buffer_attribs = vtbdec_create_buffer_attributes(ctx, kColorSpace);
	
	cbacks.decompressionOutputCallback = vtbdec_on_frame;
    cbacks.decompressionOutputRefCon   = ctx;

	status = 1;
	if (!ctx->disable_hw) {
		dec_type = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(dec_type, kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder, kCFBooleanTrue);
		ctx->is_hardware = GF_TRUE;

		status = VTDecompressionSessionCreate(NULL, ctx->fmt_desc, dec_type, buffer_attribs, &cbacks, &ctx->vtb_session);

		if (dec_type)
			CFRelease(dec_type);
	}

	//if HW decoder not available or disabled , try soft one
	if (status) {
		status = VTDecompressionSessionCreate(NULL, ctx->fmt_desc, NULL, buffer_attribs, &cbacks, &ctx->vtb_session);
		ctx->is_hardware = GF_FALSE;
	}
	
	if (dec_dsi)
		CFRelease(dec_dsi);
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

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->width) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PAR, &PROP_FRAC(ctx->pixel_ar) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->pix_fmt) );

	switch (ctx->vtb_type) {
	case kCMVideoCodecType_H264:
		gf_filter_set_name(filter, ctx->is_hardware ? "VTB:Hardware:AVC|H264" : "VTB:Software:AVC|H264");
		break;
	case kCMVideoCodecType_MPEG2Video:
		gf_filter_set_name(filter, ctx->is_hardware ? "VTB:Hardware:MPEG2" : "VTB:Software:MPEG2");
		break;
    case  kCMVideoCodecType_MPEG4Video:
		gf_filter_set_name(filter, ctx->is_hardware ? "VTB:Hardware:MPEG4P2" : "VTB:Software:MPEG4P2");
		break;
    case kCMVideoCodecType_H263:
		gf_filter_set_name(filter, ctx->is_hardware ? "VTB:Hardware:H263" : "VTB:Software:H263");
		break;
	case kCMVideoCodecType_MPEG1Video:
		gf_filter_set_name(filter, ctx->is_hardware ? "VTB:Hardware:MPEG1" : "VTB:Software:MPEG1");
		break;
	default:
		break;
	}
	return GF_OK;
}

static void vtbdec_register_param_sets(GF_VTBDecCtx *ctx, char *data, u32 size, Bool is_sps)
{
	Bool add = GF_TRUE;
	u32 i, count;
	s32 ps_id;
	GF_List *dest = is_sps ? ctx->SPSs : ctx->PPSs;
	
	if (is_sps) {
		ps_id = gf_media_avc_read_sps(data, size, &ctx->avc, 0, NULL);
		if (ps_id<0) return;
	} else {
		ps_id = gf_media_avc_read_pps(data, size, &ctx->avc);
		if (ps_id<0) return;
	}
	
	count = gf_list_count(dest);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *a_slc = gf_list_get(dest, i);
		if (a_slc->id != ps_id) continue;
		//not same size or different content but same ID, remove old xPS
		if ((a_slc->size != size) || memcmp(a_slc->data, data, size) ) {
			gf_free(a_slc->data);
			gf_free(a_slc);
			gf_list_rem(dest, i);
			break;
		} else {
			add = GF_FALSE;
		}
		break;
	}
	if (add) {
		GF_AVCConfigSlot *slc;
		GF_SAFEALLOC(slc, GF_AVCConfigSlot);
		slc->data = gf_malloc(size);
		memcpy(slc->data, data, size);
		slc->size = size;
		slc->id = ps_id;
		slc->crc = gf_crc_32(data, size);
		gf_list_add(dest, slc);
	}
}

static u32 vtbdec_purge_param_sets(GF_VTBDecCtx *ctx, Bool is_sps, s32 idx)
{
	u32 i, j, count, crc_res = 0;
	GF_List *dest = is_sps ? ctx->SPSs : ctx->PPSs;

	//remove all xPS sharing the same ID, use only the last occurence
	count = gf_list_count(dest);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *slc = gf_list_get(dest, i);
		if (slc->id != idx) continue;
		crc_res = slc->crc;

		for (j=i+1; j<count; j++) {
			GF_AVCConfigSlot *a_slc = gf_list_get(dest, j);
			if (a_slc->id != slc->id) continue;
			//not same size or different content but same ID, remove old xPS
			if ((slc->size != a_slc->size) || memcmp(a_slc->data, slc->data, a_slc->size) ) {
				crc_res = a_slc->crc;
				gf_free(slc->data);
				gf_free(slc);
				gf_list_rem(dest, i);
				i--;
				count--;
				break;
			}
		}
	}
	return crc_res;
}

static GF_Err vtbdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p, *dsi;
	u32 oti, dsi_crc;
	GF_Err e;
	GF_VTBDecCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) gf_filter_pid_remove(ctx->opid);
		ctx->opid = NULL;
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_OTI);
	if (!p) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTBDec] Missing OTI, cannot initialize\n"));
		return GF_NOT_SUPPORTED;
	}
	oti = p->value.uint;

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	dsi_crc = dsi ? gf_crc_32(dsi->value.data, dsi->data_len) : 0;
	if ((oti==ctx->oti) && (dsi_crc == ctx->cfg_crc)) return GF_OK;
	//need a reset !
	if (ctx->vtb_session) {
		vtbdec_delete_decoder(ctx);
	}

	ctx->ipid = pid;
	ctx->cfg_crc = dsi_crc;
	ctx->oti = oti;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_framing_mode(ctx->ipid, GF_TRUE);

		gf_filter_pid_copy_properties(ctx->opid, ctx->ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_OTI, &PROP_UINT(GPAC_OTI_RAW_MEDIA_STREAM) );
	}

	//check AVC config
	if (oti==GPAC_OTI_VIDEO_AVC) {
		ctx->SPSs = gf_list_new();
		ctx->PPSs = gf_list_new();
		ctx->is_avc = GF_TRUE;
		ctx->check_h264_isma = GF_TRUE;

		ctx->avc.sps_active_idx = ctx->avc.pps_active_idx = -1;
		ctx->active_sps = ctx->active_pps = -1;
		ctx->active_sps_crc = ctx->active_pps_crc = 0;

		if (!dsi || !dsi->value.data) {
			ctx->is_annex_b = GF_TRUE;
			ctx->width=ctx->height=128;
			ctx->out_size = ctx->width*ctx->height*3/2;
			ctx->pix_fmt = GF_PIXEL_YV12;
			return GF_OK;
		} else {
			u32 i;
			GF_AVCConfigSlot *slc;
			GF_AVCConfig *cfg = gf_odf_avc_cfg_read(dsi->value.data, dsi->data_len);
			for (i=0; i<gf_list_count(cfg->sequenceParameterSets); i++) {
				slc = gf_list_get(cfg->sequenceParameterSets, i);
				slc->id = -1;
				vtbdec_register_param_sets(ctx, slc->data, slc->size, GF_TRUE);
			}

			for (i=0; i<gf_list_count(cfg->pictureParameterSets); i++) {
				slc = gf_list_get(cfg->pictureParameterSets, i);
				slc->id = -1;
				vtbdec_register_param_sets(ctx, slc->data, slc->size, GF_FALSE);
			}

			slc = gf_list_get(ctx->SPSs, 0);
			if (slc) {
				ctx->active_sps = slc->id;
				ctx->active_sps_crc = gf_crc_32(slc->data, slc->size);
			}

			slc = gf_list_get(ctx->PPSs, 0);
			if (slc) {
				ctx->active_pps = slc->id;
				ctx->active_pps_crc = gf_crc_32(slc->data, slc->size);
			}

			ctx->nalu_size_length = cfg->nal_unit_size;
			if (gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs) ) {
				e = vtbdec_init_decoder(filter, ctx);
			} else {
				e = GF_OK;
			}
			gf_odf_avc_cfg_del(cfg);
			return e;
		}
	}

	//check VOSH config
	if (oti==GPAC_OTI_VIDEO_MPEG4_PART2) {
		if (!dsi || !dsi->value.data) {
			ctx->width=ctx->height=128;
			ctx->out_size = ctx->width*ctx->height*3/2;
			ctx->pix_fmt = GF_PIXEL_YV12;
		} else {
			return vtbdec_init_decoder(filter, ctx);
		}
	}

	return vtbdec_init_decoder(filter, ctx);
}

static void vtbdec_del_param_list(GF_List *list)
{
	while (gf_list_count(list)) {
		GF_AVCConfigSlot *slc = gf_list_get(list, 0);
		gf_free(slc->data);
		gf_free(slc);
		gf_list_rem(list, 0);
	}
	gf_list_del(list);
}

static void vtbdec_delete_decoder(GF_VTBDecCtx *ctx)
{
	if (ctx->fmt_desc) {
		CFRelease(ctx->fmt_desc);
		ctx->fmt_desc = NULL;
	}
	if (ctx->vtb_session) {
		VTDecompressionSessionInvalidate(ctx->vtb_session);
		ctx->vtb_session=NULL;
	}
	vtbdec_del_param_list(ctx->SPSs);
	ctx->SPSs = NULL;
	vtbdec_del_param_list(ctx->PPSs);
	ctx->PPSs = NULL;
}

static GF_Err vtbdec_parse_nal_units(GF_Filter *filter, GF_VTBDecCtx *ctx, char *inBuffer, u32 inBufferLength, char **out_buffer, u32 *out_size)
{
	u32 i, sc_size;
	char *ptr = inBuffer;
	u32 nal_size;
	GF_Err e = GF_OK;
	GF_BitStream *bs = NULL;
	Bool check_reconfig = GF_FALSE;

	if (out_buffer) {
		*out_buffer = NULL;
		*out_size = 0;
	}
	
	if (!ctx->nalu_size_length) {
		sc_size=0;
		nal_size = gf_media_nalu_next_start_code((u8 *) inBuffer, inBufferLength, &sc_size);
		if (!sc_size) return GF_NON_COMPLIANT_BITSTREAM;
		ptr += nal_size + sc_size;
		assert(inBufferLength >= nal_size + sc_size);
		inBufferLength -= nal_size + sc_size;
	}
	
	while (inBufferLength) {
		Bool add_nal = GF_TRUE;
		u8 nal_type, nal_hdr;
		GF_BitStream *nal_bs=NULL;
		
		if (ctx->nalu_size_length) {
			nal_size = 0;
			for (i=0; i<ctx->nalu_size_length; i++) {
				nal_size = (nal_size<<8) + ((u8) ptr[i]);
			}
			ptr += ctx->nalu_size_length;
		} else {
			nal_size = gf_media_nalu_next_start_code((const u8 *) ptr, inBufferLength, &sc_size);
		}
		nal_bs = gf_bs_new(ptr, nal_size, GF_BITSTREAM_READ);
		nal_hdr = gf_bs_read_u8(nal_bs);
		nal_type = nal_hdr & 0x1F;
		switch (nal_type) {
		case GF_AVC_NALU_SEQ_PARAM:
			vtbdec_register_param_sets(ctx, ptr, nal_size, GF_TRUE);
			add_nal = GF_FALSE;
			check_reconfig = GF_TRUE;
			break;
		case GF_AVC_NALU_PIC_PARAM:
			vtbdec_register_param_sets(ctx, ptr, nal_size, GF_FALSE);
			add_nal = GF_FALSE;
			check_reconfig = GF_TRUE;
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
		
		gf_media_avc_parse_nalu(nal_bs, nal_hdr, &ctx->avc);

		gf_bs_del(nal_bs);

		//if sps and pps are ready, init decoder
		if (!ctx->vtb_session && gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs) ) {
			e = vtbdec_init_decoder(filter, ctx);
			if (e) {
				gf_bs_del(bs);
				return e;
			}
		}
		
		if (!out_buffer) add_nal = GF_FALSE;
		else if (add_nal && !ctx->vtb_session) add_nal = GF_FALSE;

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

	if (check_reconfig && ctx->avc.s_info.pps ) {
		u32 sps_crc, pps_crc;
		sps_crc = vtbdec_purge_param_sets(ctx, GF_TRUE, ctx->avc.s_info.pps->sps_id);
		pps_crc = vtbdec_purge_param_sets(ctx, GF_FALSE, ctx->avc.s_info.pps->id);

		if ((sps_crc != ctx->active_sps_crc) || (pps_crc != ctx->active_pps_crc) ) {
			ctx->reconfig_needed = 1;
			ctx->active_sps = ctx->avc.s_info.pps->sps_id;
			ctx->active_pps = ctx->avc.s_info.pps->id;
			ctx->active_sps_crc = sps_crc;
			ctx->active_pps_crc = pps_crc;
		}
	}

	if (bs) {
		gf_bs_get_content(bs, out_buffer, out_size);
		gf_bs_del(bs);
	}
	return e;
}


static GF_Err vtbdec_send_output_frame(GF_Filter *filter, GF_VTBDecCtx *ctx);

static GF_Err vtbdec_flush_frame(GF_Filter *filter, GF_VTBDecCtx *ctx)
{
	GF_VTBHWFrame *vtbframe;
    OSStatus status;
	OSType type;
	if (ctx->no_copy) return vtbdec_send_output_frame(filter, ctx);

	vtbframe = gf_list_pop_front(ctx->frames);

	status = CVPixelBufferLockBaseAddress(vtbframe->frame, kCVPixelBufferLock_ReadOnly);
    if (status != kCVReturnSuccess) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error locking frame data\n"));
		gf_list_add(ctx->frames_res, vtbframe);
        return GF_IO_ERR;
    }

	type = CVPixelBufferGetPixelFormatType(vtbframe->frame);

	if ((type==kCVPixelFormatType_420YpCbCr8Planar)
		|| (type==kCVPixelFormatType_420YpCbCr8PlanarFullRange)
		|| (type==kCVPixelFormatType_422YpCbCr8_yuvs)
		|| (type==kCVPixelFormatType_444YpCbCr8)
		|| (type=='444v')
		|| (type==kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
		|| (type==kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
	) {
        u32 i, j, nb_planes = (u32) CVPixelBufferGetPlaneCount(vtbframe->frame);
		char *dst;
		Bool needs_stride=GF_FALSE;
		u32 stride = (u32) CVPixelBufferGetBytesPerRowOfPlane(vtbframe->frame, 0);

		GF_FilterPacket *dst_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &dst);

		//TOCHECK - for now the 3 planes are consecutive in VideoToolbox
		if (stride==ctx->width) {
			char *data = CVPixelBufferGetBaseAddressOfPlane(vtbframe->frame, 0);
			memcpy(dst, data, sizeof(char)*ctx->out_size);
		} else {
			for (i=0; i<nb_planes; i++) {
				char *data = CVPixelBufferGetBaseAddressOfPlane(vtbframe->frame, i);
				u32 stride = (u32) CVPixelBufferGetBytesPerRowOfPlane(vtbframe->frame, i);
				u32 w, h = (u32) CVPixelBufferGetHeightOfPlane(vtbframe->frame, i);
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

		gf_filter_pck_set_cts(dst_pck, vtbframe->cts);
		ctx->last_cts_out = vtbframe->cts;
		ctx->last_timescale_out = vtbframe->timescale;

		gf_filter_pck_send(dst_pck);
	}
    CVPixelBufferUnlockBaseAddress(vtbframe->frame, kCVPixelBufferLock_ReadOnly);
	gf_list_add(ctx->frames_res, vtbframe);
	return GF_OK;
}
static GF_Err vtbdec_process(GF_Filter *filter)
{
    OSStatus status;
    CMSampleBufferRef sample = NULL;
    CMBlockBufferRef block_buffer = NULL;
	char *in_data;
	u32 in_data_size;
	char *in_buffer;
	u32 in_buffer_size, frames_count;
	Bool do_free=GF_FALSE;
	GF_Err e;
	GF_VTBDecCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			while (gf_list_count(ctx->frames)) {
				vtbdec_flush_frame(filter, ctx);
			}
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	in_buffer = (char *) gf_filter_pck_get_data(pck, &in_buffer_size);

	if (ctx->skip_mpeg4_vosh) {
		GF_M4VDecSpecInfo dsi;
		dsi.width = dsi.height = 0;
		e = gf_m4v_get_config(in_buffer, in_buffer_size, &dsi);
		//found a vosh - remove it from payload, init decoder if needed
		if ((e==GF_OK) && dsi.width && dsi.height) {
			if (!ctx->vtb_session) {
				ctx->vosh = in_buffer;
				ctx->vosh_size = dsi.next_object_start;
				e = vtbdec_init_decoder(filter, ctx);
				if (e) {
					gf_filter_pid_drop_packet(ctx->ipid);
					return e;
				}

				//enfoce removal for all frames
				ctx->skip_mpeg4_vosh = GF_TRUE;
			}
			ctx->vosh_size = dsi.next_object_start;
		} else if (!ctx->vtb_session) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
	}

	if (ctx->init_mpeg12) {
		GF_M4VDecSpecInfo dsi;
		dsi.width = dsi.height = 0;
		
		e = gf_mpegv12_get_config(in_buffer, in_buffer_size, &dsi);
		if ((e==GF_OK) && dsi.width && dsi.height) {
			ctx->width = dsi.width;
			ctx->height = dsi.height;
			ctx->pixel_ar.num = dsi.par_num;
			ctx->pixel_ar.den = dsi.par_den;
			
			e = vtbdec_init_decoder(filter, ctx);
			if (e) {
				gf_filter_pid_drop_packet(ctx->ipid);
				return e;
			}
		}

		if (!ctx->vtb_session) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}
	}
	
	if (ctx->check_h264_isma) {
		if (in_buffer && !in_buffer[0] && !in_buffer[1] && !in_buffer[2] && (in_buffer[3]==0x01)) {
			ctx->check_h264_isma=GF_FALSE;
			ctx->nalu_size_length=0;
			ctx->is_annex_b=GF_TRUE;
		}
	}

	//Always parse AVC data , remove SPS/PPS/... and reconfig if needed
	if (ctx->is_annex_b || ctx->nalu_size_length) {
		do_free=GF_TRUE;
		if (ctx->vtb_session && ctx->cached_annex_b) {
			in_data = ctx->cached_annex_b;
			in_data_size = ctx->cached_annex_b_size;
			ctx->cached_annex_b = NULL;
		} else {
			e = vtbdec_parse_nal_units(filter, ctx, in_buffer, in_buffer_size, &in_data, &in_data_size);
			if (e) return e;
		}
		if (ctx->reconfig_needed) {
			//flush alll pending frames
			while (gf_list_count(ctx->frames)) {
				vtbdec_flush_frame(filter, ctx);
			}
			//waiting for last frame to be discarded - this needs checking with the new arch (compositor might not release the last frame)
			if (ctx->no_copy && ctx->decoded_frames_pending) {
				gf_free(in_data);
				ctx->cached_annex_b = NULL;
				return GF_OK;
			}
			if (ctx->fmt_desc) {
				CFRelease(ctx->fmt_desc);
				ctx->fmt_desc = NULL;
			}
			if (ctx->vtb_session) {
				VTDecompressionSessionInvalidate(ctx->vtb_session);
				ctx->vtb_session=NULL;
			}
			vtbdec_init_decoder(filter, ctx);
		}

	} else if (ctx->vosh_size) {
		in_data = in_buffer + ctx->vosh_size;
		in_data_size = in_buffer_size - ctx->vosh_size;
		ctx->vosh_size = 0;
	} else {
		in_data = in_buffer;
		in_data_size = in_buffer_size;
	}
	
	if (!ctx->vtb_session) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}
	

	status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, in_data, in_data_size, kCFAllocatorNull, NULL, 0, in_data_size, 0, &block_buffer);

	if (status) {
		return GF_IO_ERR;
	}

	if (block_buffer == NULL)
		return GF_OK;
		
	
	status = CMSampleBufferCreate(kCFAllocatorDefault, block_buffer, TRUE, NULL, NULL, ctx->fmt_desc, 1, 0, NULL, 0, NULL, &sample);

    if (status || (sample==NULL)) {
		if (block_buffer)
			CFRelease(block_buffer);

		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_IO_ERR;
	}
	ctx->cur_pck = pck;
	ctx->last_error = GF_OK;
    status = VTDecompressionSessionDecodeFrame(ctx->vtb_session, sample, 0, NULL, 0);
    if (!status)
		status = VTDecompressionSessionWaitForAsynchronousFrames(ctx->vtb_session);
	

	CFRelease(block_buffer);
	CFRelease(sample);
	if (do_free)
		gf_free(in_data);

	gf_filter_pid_drop_packet(ctx->ipid);
	ctx->cur_pck = NULL;

	if (ctx->last_error) return ctx->last_error;
	if (status)
		return GF_NON_COMPLIANT_BITSTREAM;

	frames_count = gf_list_count(ctx->frames);
	if (!frames_count) {
		return ctx->last_error;
	}
	//probing for reordering, or reordering is on but not enough frames: wait before we dispatch
	if (ctx->reorder_probe) {
		ctx->reorder_probe--;
		return GF_OK;
	}

	if (ctx->reorder_detected && (frames_count<ctx->reorder) )
		return GF_OK;

	return vtbdec_flush_frame(filter, ctx);
}

void vtbframe_release(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_FilterHWFrame *frame = gf_filter_pck_get_hw_frame(pck);
	GF_VTBHWFrame *f = (GF_VTBHWFrame *)frame->user_data;
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
	f->ctx->decoded_frames_pending--;
	gf_list_add(f->ctx->frames_res, f);
}

GF_Err vtbframe_get_plane(GF_FilterHWFrame *frame, u32 plane_idx, const u8 **outPlane, u32 *outStride)
{
    OSStatus status;
	GF_Err e;
	GF_VTBHWFrame *f = (GF_VTBHWFrame *)frame->user_data;
	if (! outPlane || !outStride) return GF_BAD_PARAM;
	*outPlane = NULL;
	*outStride = 0;

	if (!f->locked) {
		status = CVPixelBufferLockBaseAddress(f->frame, kCVPixelBufferLock_ReadOnly);
		if (status != kCVReturnSuccess) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error locking frame data\n"));
			return GF_IO_ERR;
		}
		f->locked = GF_TRUE;
	}
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
	return e;
}

#ifdef GPAC_IPHONE

void *myGetGLContext();

GF_Err vtbframe_get_gl_texture(GF_FilterHWFrame *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_CodecMatrix * texcoordmatrix)
{
    OSStatus status;
	GLenum target_fmt;
	u32 w, h;
	CVOpenGLESTextureRef *outTexture=NULL;
	GF_VTBHWFrame *f = (GF_VTBHWFrame *)frame->user_data;
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
	if (! f->ctx->decoded_frames_pending) return GF_IO_ERR;
	
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
	//don't create texture if already done !
	if ( *outTexture == NULL) {
		status = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault, f->ctx->cache_texture, f->frame, NULL, GL_TEXTURE_2D, target_fmt, w, h, target_fmt, GL_UNSIGNED_BYTE, plane_idx, outTexture);
	
		if (status != kCVReturnSuccess) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error creating cache texture for plane %d\n", plane_idx));
			return GF_IO_ERR;
		}
	}
	*gl_tex_format = CVOpenGLESTextureGetTarget(*outTexture);
	*gl_tex_id = CVOpenGLESTextureGetName(*outTexture);

	return GF_OK;
}
#endif

static GF_Err vtbdec_send_output_frame(GF_Filter *filter, GF_VTBDecCtx *ctx)
{
	GF_VTBHWFrame *vtb_frame;
	GF_FilterPacket *dst_pck;

	vtb_frame = gf_list_pop_front(ctx->frames);
	if (!vtb_frame) return GF_BAD_PARAM;

	vtb_frame->hw_frame.user_data = vtb_frame;
	vtb_frame->hw_frame.get_plane = vtbframe_get_plane;
#ifdef GPAC_IPHONE
	if (ctx->use_gl_textures)
		vtb_frame->hw_frame.get_gl_texture = vtbframe_get_gl_texture;
#endif

	if (!gf_list_count(ctx->frames) && ctx->reconfig_needed)
		vtb_frame->hw_frame.hardware_reset_pending = GF_TRUE;

	ctx->decoded_frames_pending++;

	dst_pck = gf_filter_pck_new_hw_frame(ctx->opid, &vtb_frame->hw_frame, vtbframe_release);
	gf_filter_pck_set_cts(dst_pck, vtb_frame->cts);
	ctx->last_cts_out = vtb_frame->cts;
	ctx->last_timescale_out = vtb_frame->timescale;

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}

#endif

static GF_Err vtbdec_initialize(GF_Filter *filter)
{
	GF_VTBDecCtx *ctx = (GF_VTBDecCtx *) gf_filter_get_udta(filter);

#ifdef GPAC_IPHONE
	if (ctx->no_copy)
		ctx->use_gl_textures = GF_TRUE;
#endif

	ctx->frames_res = gf_list_new();
	ctx->frames = gf_list_new();
	return GF_OK;
}

static void vtbdec_finalize(GF_Filter *filter)
{
	GF_VTBDecCtx *ctx = (GF_VTBDecCtx *) gf_filter_get_udta(filter);
	vtbdec_delete_decoder(ctx);

#ifdef GPAC_IPHONE
	if (ctx->cache_texture) {
		CFRelease(ctx->cache_texture);
    }
#endif

	if (ctx->frames) {
		while (gf_list_count(ctx->frames) ) {
			GF_VTBHWFrame *f = gf_list_pop_back(ctx->frames);
			gf_free(f);
		}
		gf_list_del(ctx->frames);
	}

	if (ctx->frames_res) {
		while (gf_list_count(ctx->frames_res) ) {
			GF_VTBHWFrame *f = gf_list_pop_back(ctx->frames_res);
			gf_free(f);
		}
		gf_list_del(ctx->frames_res);
	}
}


static const GF_FilterCapability VTBDecInputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_EXC_BOOL(GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG4_PART2),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_AVC),

#ifndef GPAC_IPHONE
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_SIMPLE),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_MAIN),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_SNR),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_SPATIAL),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_HIGH),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_MPEG2_422),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_VIDEO_H263),
#endif
};

static const GF_FilterCapability VTBDecOutputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_OTI, GPAC_OTI_RAW_MEDIA_STREAM),
};

#define OFFS(_n)	#_n, offsetof(GF_VTBDecCtx, _n)

static const GF_FilterArgs VTBDecArgs[] =
{
	{ OFFS(reorder), "number of frames to wait for temporal re-ordering", GF_PROP_UINT, "6", NULL, GF_TRUE},
	{ OFFS(no_copy), "dispatch VTB frames into filter chain (no copy)", GF_PROP_BOOL, "true", NULL, GF_TRUE},
	{ OFFS(disable_hw), "Disables hardware decoding", GF_PROP_BOOL, "false", NULL, GF_TRUE},
	{}
};

GF_FilterRegister GF_VTBDecCtxRegister = {
	.name = "vtbdec",
	.description = "VideoToolBox decoder",
	.private_size = sizeof(GF_VTBDecCtx),
	.args = VTBDecArgs,
	INCAPS(VTBDecInputs),
	OUTCAPS(VTBDecOutputs),
	.initialize = vtbdec_initialize,
	.finalize = vtbdec_finalize,
	.configure_pid = vtbdec_configure_pid,
	.process = vtbdec_process,
};

#endif // !defined(GPAC_DISABLE_AV_PARSERS) && ( defined(GPAC_CONFIG_DARWIN) || defined(GPAC_IPHONE) )

const GF_FilterRegister *vtbdec_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && ( defined(GPAC_CONFIG_DARWIN) || defined(GPAC_IPHONE) )
	return &GF_VTBDecCtxRegister;
#else
	return NULL;
#endif
}
