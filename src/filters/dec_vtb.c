/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
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

#if !defined(GPAC_DISABLE_AV_PARSERS) && ( defined(GPAC_CONFIG_DARWIN) || defined(GPAC_CONFIG_IOS) )

#include <stdint.h>

#define Picture QuickdrawPicture
#include <VideoToolbox/VideoToolbox.h>
#undef Picture

#ifndef kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder
#  define kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder CFSTR("RequireHardwareAcceleratedVideoDecoder")
#endif

#include <gpac/maths.h>
#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include <gpac/filters.h>

#include "../../src/compositor/gl_inc.h"


#ifdef GPAC_CONFIG_IOS
#define VTB_GL_TEXTURE

#define GF_CVGLTextureREF CVOpenGLESTextureRef
#define GF_CVGLTextureCacheREF CVOpenGLESTextureCacheRef
#define GF_kCVPixelBufferOpenGLCompatibilityKey kCVPixelBufferOpenGLESCompatibilityKey
#define GF_CVOpenGLTextureCacheFlush CVOpenGLESTextureCacheFlush
#define GF_CVOpenGLTextureGetTarget CVOpenGLESTextureGetTarget
#define GF_CVOpenGLTextureGetName CVOpenGLESTextureGetName

#else

//not working yet, not sure why
//#define VTB_GL_TEXTURE

#include <CoreVideo/CVOpenGLTexture.h>

#define GF_CVGLTextureREF CVOpenGLTextureRef
#define GF_CVGLTextureCacheREF CVOpenGLTextureCacheRef
#define GF_kCVPixelBufferOpenGLCompatibilityKey kCVPixelBufferOpenGLCompatibilityKey
#define GF_CVOpenGLTextureCacheFlush CVOpenGLTextureCacheFlush
#define GF_CVOpenGLTextureGetTarget CVOpenGLTextureGetTarget
#define GF_CVOpenGLTextureGetName CVOpenGLTextureGetName


#endif

#ifndef GPAC_DISABLE_AV_PARSERS

typedef struct
{
	//opts
	u32 reorder, ofmt;
	Bool no_copy;
	Bool disable_hw;

	//internal
//	GF_FilterPid *ipid;
	GF_List *streams;
	GF_FilterPid *opid;
	u32 width, height;
	GF_Fraction pixel_ar;
	u32 pix_fmt;
	u32 out_size;
	u32 cfg_crc;
	u32 codecid;
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
	GF_BitStream *nal_bs;
	GF_BitStream *ps_bs;

	GF_BitStream *nalu_rewrite_bs;
	u8 *nalu_buffer;
	u32 nalu_buffer_alloc;

	Bool is_avc;
	Bool is_annex_b;

	u32 nalu_size_length;

	GF_List *SPSs, *PPSs, *VPSs;
	s32 active_sps, active_pps, active_vps;
	u32 active_sps_crc, active_pps_crc, active_vps_crc;

	AVCState avc;
	Bool check_h264_isma;

	HEVCState hevc;
	Bool is_hevc;

	//openGL output
#ifdef VTB_GL_TEXTURE
	Bool use_gl_textures;
	GF_CVGLTextureCacheREF cache_texture;
#endif
	void *gl_context;
} GF_VTBDecCtx;


typedef struct
{
	GF_FilterFrameInterface frame_ifce;

	Bool locked;
	CVPixelBufferRef frame;
	GF_VTBDecCtx *ctx;
	GF_FilterPacket *pck_src;
	//openGL mode
#ifdef VTB_GL_TEXTURE
	GF_CVGLTextureREF y, u, v;
#endif
} GF_VTBHWFrame;

static void vtbdec_delete_decoder(GF_VTBDecCtx *ctx);
static GF_Err vtbdec_flush_frame(GF_Filter *filter, GF_VTBDecCtx *ctx);

static void vtbdec_on_frame(void *opaque, void *sourceFrameRefCon, OSStatus status, VTDecodeInfoFlags flags, CVImageBufferRef image, CMTime pts, CMTime duration)
{
	GF_VTBDecCtx *ctx = (GF_VTBDecCtx *)opaque;
	GF_VTBHWFrame *frame;
	u32 i, count, timescale;
	u64 cts, dts;
	assert(ctx->cur_pck);

    if (!image) {
		if (status != kCVReturnSuccess) {
			ctx->last_error = GF_NON_COMPLIANT_BITSTREAM;
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Decode error - status %d\n", status));
		}
        GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[VTB] No output buffer - status %d\n", status));
        return;
    }
	if (gf_filter_pck_get_seek_flag(ctx->cur_pck) ) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[VTB] Frame marked as seek, not dispatching - status %d\n", status));
		return;
	}

	frame = gf_list_pop_back(ctx->frames_res);
	if (!frame) {
		GF_SAFEALLOC(frame, GF_VTBHWFrame);
	} else {
		memset(frame, 0, sizeof(GF_VTBHWFrame));
	}

	assert( gf_filter_pck_get_seek_flag(ctx->cur_pck) == 0 );

	frame->frame_ifce.user_data = frame;
	frame->frame = CVPixelBufferRetain(image);
	frame->pck_src = ctx->cur_pck;
	gf_filter_pck_ref_props(&frame->pck_src);

	frame->ctx = ctx;
	cts = gf_filter_pck_get_cts(frame->pck_src);
	dts = gf_filter_pck_get_dts(frame->pck_src);
	timescale = gf_filter_pck_get_timescale(frame->pck_src);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[VTB] Decoded frame DTS "LLU" CTS "LLU" timescale %d\n", dts, cts, timescale));

	if (!ctx->last_timescale_out)
		ctx->last_timescale_out = gf_filter_pck_get_timescale(frame->pck_src);

	count = gf_list_count(ctx->frames);
	for (i=0; i<count; i++) {
		GF_VTBHWFrame *aframe = gf_list_get(ctx->frames, i);
		Bool insert = GF_FALSE;
		u64 acts, adts, atimescale;
		s64 diff;

		acts = gf_filter_pck_get_cts(aframe->pck_src);
		adts = gf_filter_pck_get_dts(aframe->pck_src);
		atimescale = gf_filter_pck_get_timescale(aframe->pck_src);

		if (adts > dts) {
			ctx->reorder_probe=0;
			ctx->reorder_detected=GF_FALSE;
			break;
		}
		if ((timescale == atimescale) && (ctx->last_timescale_out == timescale)) {
			diff = (s64) acts - (s64) cts;
			if ((diff>0) && (cts > ctx->last_cts_out) ) {
				insert = GF_TRUE;
			}
		} else {
			diff = (s64) (acts * timescale) - (s64) (cts * atimescale);
			if ((diff>0) && (ctx->last_timescale_out * cts > timescale * ctx->last_cts_out) ) {
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

#ifdef VTB_GL_TEXTURE
	if (ctx->use_gl_textures)
		CFDictionarySetValue(buffer_attributes, GF_kCVPixelBufferOpenGLCompatibilityKey, kCFBooleanTrue);
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
	u8 *dsi_data=NULL;
	u32 dsi_data_size=0;
	u32 w, h, stride;
	GF_FilterPid *pid;
	w = h = 0;
	
    dec_dsi = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	if (ctx->ofmt==1) {
		kColorSpace = kCVPixelFormatType_420YpCbCr8Planar;
		ctx->pix_fmt = GF_PIXEL_YUV;
	} else if (ctx->ofmt==2) {
		kColorSpace = kCVPixelFormatType_24RGB;
		ctx->pix_fmt = GF_PIXEL_RGB;
	} else {
		kColorSpace = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
		ctx->pix_fmt = GF_PIXEL_NV12;
	}

	ctx->reorder_probe = ctx->reorder;
	ctx->reorder_detected = GF_FALSE;
	pid = gf_list_get(ctx->streams, 0);
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) w = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) h = p->value.uint;


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);

	switch (ctx->codecid) {
    case GF_CODECID_AVC:
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

			s32 id = gf_media_avc_read_sps(sps->data, sps->size, &ctx->avc, 0, NULL);
			id = gf_media_avc_read_pps(pps->data, pps->size, &ctx->avc);


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
#ifndef GPAC_CONFIG_IOS
				//422 decoding doesn't seem supported ...
				if (ctx->luma_bit_depth>8) {
					kColorSpace = kCVPixelFormatType_422YpCbCr10;
					ctx->pix_fmt = GF_PIXEL_YUV422_10;
				} else
#endif
				{
					kColorSpace = kCVPixelFormatType_422YpCbCr8;
					ctx->pix_fmt = GF_PIXEL_YUV422;
				}
				break;
			case 3:
#ifndef GPAC_CONFIG_IOS
				if (ctx->luma_bit_depth>8) {
					kColorSpace = kCVPixelFormatType_444YpCbCr10;
					ctx->pix_fmt = GF_PIXEL_YUV444_10;
				} else
#endif
				{
					kColorSpace = kCVPixelFormatType_444YpCbCr8;
					ctx->pix_fmt = GF_PIXEL_YUV444;
				}
				break;
			default:
#if !defined(GPAC_CONFIG_IOS) && defined(AVAILABLE_MAC_OS_X_VERSION_10_13_AND_LATER)
				if (ctx->luma_bit_depth>8) {
					kColorSpace = kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
					ctx->pix_fmt = GF_PIXEL_NV12_10;
				}
#endif
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
				
			//we send only the active SPS and PPS, otherwise vtb complains !!
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

    case GF_CODECID_HEVC:
		if (gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs) && gf_list_count(ctx->VPSs)) {
			s32 idx;
			u32 i;
			GF_HEVCConfig *cfg;
			GF_HEVCParamArray *vpsa = NULL;
			GF_HEVCParamArray *spsa = NULL;
			GF_HEVCParamArray *ppsa = NULL;
			GF_AVCConfigSlot *vps = NULL;
			GF_AVCConfigSlot *sps = NULL;
			GF_AVCConfigSlot *pps = NULL;

			for (i=0; i<gf_list_count(ctx->VPSs); i++) {
				vps = gf_list_get(ctx->VPSs, i);
				if (ctx->active_vps<0) ctx->active_vps = vps->id;

				if (vps->id==ctx->active_vps) break;
				vps = NULL;
			}
			if (!vps) return GF_NON_COMPLIANT_BITSTREAM;

			for (i=0; i<gf_list_count(ctx->SPSs); i++) {
				sps = gf_list_get(ctx->SPSs, i);
				if (ctx->active_sps<0) ctx->active_sps = sps->id;

				if (sps->id==ctx->active_sps) break;
				sps = NULL;
			}
			if (!sps) return GF_NON_COMPLIANT_BITSTREAM;
			for (i=0; i<gf_list_count(ctx->PPSs); i++) {
				pps = gf_list_get(ctx->PPSs, i);
				if (ctx->active_pps<0) ctx->active_pps = pps->id;

				if (pps->id==ctx->active_pps) break;
				pps = NULL;
			}
			if (!pps) return GF_NON_COMPLIANT_BITSTREAM;
			ctx->reconfig_needed = GF_FALSE;

			ctx->vtb_type = kCMVideoCodecType_HEVC;

			idx = ctx->active_sps;
			ctx->width = ctx->hevc.sps[idx].width;
			ctx->height = ctx->hevc.sps[idx].height;
			if (ctx->hevc.sps[idx].aspect_ratio_info_present_flag && ctx->hevc.sps[idx].sar_width && ctx->hevc.sps[idx].sar_height) {
				ctx->pixel_ar.num = ctx->hevc.sps[idx].sar_width;
				ctx->pixel_ar.den = ctx->hevc.sps[idx].sar_height;
			}
			ctx->chroma_format = ctx->hevc.sps[idx].chroma_format_idc;
			ctx->luma_bit_depth = ctx->hevc.sps[idx].bit_depth_luma;
			ctx->chroma_bit_depth = ctx->hevc.sps[idx].bit_depth_chroma;

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
					ctx->pix_fmt = GF_PIXEL_NV12;
				}
				break;
			}
			//always rewrite with current sps and pps
			cfg = gf_odf_hevc_cfg_new();
			cfg->configurationVersion = 1;
			cfg->profile_space = ctx->hevc.sps[idx].ptl.profile_space;
			cfg->tier_flag = ctx->hevc.sps[idx].ptl.tier_flag;
			cfg->profile_idc = ctx->hevc.sps[idx].ptl.profile_idc;
			cfg->general_profile_compatibility_flags = ctx->hevc.sps[idx].ptl.profile_compatibility_flag;
			cfg->progressive_source_flag = ctx->hevc.sps[idx].ptl.general_progressive_source_flag;
			cfg->interlaced_source_flag = ctx->hevc.sps[idx].ptl.general_interlaced_source_flag;
			cfg->non_packed_constraint_flag = ctx->hevc.sps[idx].ptl.general_non_packed_constraint_flag;
			cfg->frame_only_constraint_flag = ctx->hevc.sps[idx].ptl.general_frame_only_constraint_flag;

			cfg->constraint_indicator_flags = ctx->hevc.sps[idx].ptl.general_reserved_44bits;
			cfg->level_idc = ctx->hevc.sps[idx].ptl.level_idc;

			cfg->luma_bit_depth = ctx->hevc.sps[idx].bit_depth_luma;
			cfg->chroma_bit_depth = ctx->hevc.sps[idx].bit_depth_chroma;
			cfg->chromaFormat = ctx->hevc.sps[idx].chroma_format_idc;
			cfg->complete_representation = GF_TRUE;

			cfg->nal_unit_size = 4;

			GF_SAFEALLOC(vpsa, GF_HEVCParamArray);
			vpsa->array_completeness = 1;
			vpsa->type = GF_HEVC_NALU_VID_PARAM;
			vpsa->nalus = gf_list_new();
			gf_list_add(vpsa->nalus, vps);
			gf_list_add(cfg->param_array, vpsa);

			GF_SAFEALLOC(spsa, GF_HEVCParamArray);
			spsa->array_completeness = 1;
			spsa->type = GF_HEVC_NALU_SEQ_PARAM;
			spsa->nalus = gf_list_new();
			gf_list_add(spsa->nalus, sps);
			gf_list_add(cfg->param_array, spsa);

			GF_SAFEALLOC(ppsa, GF_HEVCParamArray);
			ppsa->array_completeness = 1;
			ppsa->type = GF_HEVC_NALU_PIC_PARAM;
			//we send all PPS
			ppsa->nalus = ctx->PPSs;

			gf_list_add(cfg->param_array, ppsa);

			gf_odf_hevc_cfg_write(cfg, &dsi_data, &dsi_data_size);
			gf_list_reset(vpsa->nalus);
			gf_list_reset(spsa->nalus);
			ppsa->nalus = NULL;
			gf_odf_hevc_cfg_del(cfg);

			dsi = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			data = CFDataCreate(kCFAllocatorDefault, (const UInt8*)dsi_data, dsi_data_size);
			if (data) {
				CFDictionarySetValue(dsi, CFSTR("hvcC"), data);
				CFDictionarySetValue(dec_dsi, kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms, dsi);
				CFRelease(data);
			}
			CFRelease(dsi);

			gf_free(dsi_data);
		}
        break;

	case GF_CODECID_MPEG2_SIMPLE:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_SPATIAL:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_422:

        ctx->vtb_type = kCMVideoCodecType_MPEG2Video;
		if (!ctx->width || !ctx->height) {
			ctx->init_mpeg12 = GF_TRUE;
			return GF_OK;
		}
		ctx->init_mpeg12 = GF_FALSE;
		ctx->reconfig_needed = GF_FALSE;
        break;
		
	case GF_CODECID_MPEG1:
		ctx->vtb_type = kCMVideoCodecType_MPEG1Video;
		if (!ctx->width || !ctx->height) {
			ctx->init_mpeg12 = GF_TRUE;
			return GF_OK;
		}
		ctx->init_mpeg12 = GF_FALSE;
		ctx->reconfig_needed = GF_FALSE;
		break;
    case GF_CODECID_MPEG4_PART2 :
	{
		char *vosh = NULL;
		u32 vosh_size = 0;
		ctx->vtb_type = kCMVideoCodecType_MPEG4Video;

		if (!p || !p->value.data.ptr) {
			vosh = ctx->vosh;
			vosh_size = ctx->vosh_size;
		} else {
			vosh = p->value.data.ptr;
			vosh_size = p->value.data.size;
		}
		ctx->reconfig_needed = GF_FALSE;

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
	case GF_CODECID_H263:
	case GF_CODECID_S263:
		ctx->reorder_probe = 0;
		ctx->reconfig_needed = GF_FALSE;
		if (w && h) {
			ctx->width = w;
			ctx->height = h;
			ctx->vtb_type = kCMVideoCodecType_H263;
			break;
		}
		break;
		
	default :
		ctx->reconfig_needed = GF_FALSE;
		return GF_NOT_SUPPORTED;
    }
	//not yet ready
	if (! ctx->width || !ctx->height) return GF_OK;

    /*status = */CMVideoFormatDescriptionCreate(kCFAllocatorDefault, ctx->vtb_type, ctx->width, ctx->height, dec_dsi, &ctx->fmt_desc);

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
	stride = ctx->width;
	if (ctx->pix_fmt == GF_PIXEL_YUV422) {
		ctx->out_size = ctx->width*ctx->height*2;
	} else if (ctx->pix_fmt == GF_PIXEL_YUV444) {
		ctx->out_size = ctx->width*ctx->height*3;
	} else if (ctx->pix_fmt == GF_PIXEL_RGB) {
		ctx->out_size = ctx->width*ctx->height*3;
		stride *= 3;
	} else {
		// (ctx->pix_fmt == GF_PIXEL_YUV)
		ctx->out_size = ctx->width*ctx->height*3/2;
	}
	if (ctx->luma_bit_depth>8) {
		ctx->out_size *= 2;
	}
	ctx->frame_size_changed = GF_TRUE;

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(stride) );
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

static void vtbdec_register_param_sets(GF_VTBDecCtx *ctx, char *data, u32 size, Bool is_sps, u8 hevc_nal_type)
{
	Bool add = GF_TRUE;
	u32 i, count;
	s32 ps_id;
	GF_List *dest = NULL;

	if (!ctx->ps_bs) ctx->ps_bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(ctx->ps_bs, data, size);

	if (hevc_nal_type) {
		if (hevc_nal_type==GF_HEVC_NALU_SEQ_PARAM) {
			dest = ctx->SPSs;
			ps_id = gf_media_hevc_read_sps_bs(ctx->ps_bs, &ctx->hevc);
			if (ps_id<0) return;
		}
		else if (hevc_nal_type==GF_HEVC_NALU_PIC_PARAM) {
			dest = ctx->PPSs;
			ps_id = gf_media_hevc_read_pps_bs(ctx->ps_bs, &ctx->hevc);
			if (ps_id<0) return;
		}
		else if (hevc_nal_type==GF_HEVC_NALU_VID_PARAM) {
			dest = ctx->VPSs;
			ps_id = gf_media_hevc_read_vps_bs(ctx->ps_bs, &ctx->hevc);
			if (ps_id<0) return;
		}

	} else {
		dest = is_sps ? ctx->SPSs : ctx->PPSs;

		if (is_sps) {
			ps_id = gf_media_avc_read_sps_bs(ctx->ps_bs, &ctx->avc, 0, NULL);
			if (ps_id<0) return;
		} else {
			ps_id = gf_media_avc_read_pps_bs(ctx->ps_bs, &ctx->avc);
			if (ps_id<0) return;
		}
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

static GF_Err vtbdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p, *dsi;
	u32 codecid, dsi_crc;
	GF_Err e;
	GF_FilterPid *base_pid = NULL;
	GF_VTBDecCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->opid) gf_filter_pid_remove(ctx->opid);
		ctx->opid = NULL;
		gf_list_del_item(ctx->streams, pid);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid)) {
		while (gf_list_count(ctx->frames)) {
			vtbdec_flush_frame(filter, ctx);
		}
		return GF_NOT_SUPPORTED;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTBDec] Missing codecid, cannot initialize\n"));
		return GF_NOT_SUPPORTED;
	}
	codecid = p->value.uint;

	base_pid = gf_list_get(ctx->streams, 0);
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
	if (!p && base_pid && (base_pid != pid)) return GF_REQUIRES_NEW_INSTANCE;
	else if (p) {
		u32 i;
		u32 base_idx_plus_one = 0;

		if (ctx->codecid != GF_CODECID_HEVC) return GF_REQUIRES_NEW_INSTANCE;

		for (i=0; i<gf_list_count(ctx->streams); i++) {
			GF_FilterPid *ipid = gf_list_get(ctx->streams, i);
			const GF_PropertyValue *p_dep;
			if (ipid==pid) continue;

			p_dep = gf_filter_pid_get_property(ipid, GF_PROP_PID_ID);
			if (p_dep && p_dep->value.uint == p->value.uint) {
				base_idx_plus_one = i+1;
				break;
			}
		}
		if (!base_idx_plus_one) return GF_REQUIRES_NEW_INSTANCE;

		//no support for L-HEVC
		if (codecid != GF_CODECID_HEVC) return GF_NOT_SUPPORTED;
		if (gf_list_find(ctx->streams, pid) < 0) {
			gf_list_insert(ctx->streams, pid, base_idx_plus_one);
		}
		//no configure for temporal enhancements
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (ctx->opid && p) gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FPS, p);
		return GF_OK;
	}


	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	dsi_crc = dsi ? gf_crc_32(dsi->value.data.ptr, dsi->value.data.size) : 0;
	if ((codecid==ctx->codecid) && (dsi_crc == ctx->cfg_crc) && ctx->width && ctx->height) return GF_OK;

	//need a reset !
	if (ctx->vtb_session) {
		//flush all pending frames and mark reconfigure as pending
		ctx->reconfig_needed = GF_TRUE;
		while (gf_list_count(ctx->frames)) {
			vtbdec_flush_frame(filter, ctx);
		}
	}
	if (gf_list_find(ctx->streams, pid) < 0) {
		gf_list_insert(ctx->streams, pid, 0);
	}
	ctx->cfg_crc = dsi_crc;
	ctx->codecid = codecid;
	gf_filter_sep_max_extra_input_pids(filter, (codecid==GF_CODECID_HEVC) ? 5 : 0);

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL);

	ctx->nalu_size_length = 0;
	ctx->is_annex_b = GF_FALSE;

	//check AVC config
	if (codecid==GF_CODECID_AVC) {

		if (ctx->SPSs) vtbdec_del_param_list(ctx->SPSs);
		ctx->SPSs = gf_list_new();
		if (ctx->PPSs) vtbdec_del_param_list(ctx->PPSs);
		ctx->PPSs = gf_list_new();

		ctx->is_avc = GF_TRUE;
		ctx->check_h264_isma = GF_TRUE;

		ctx->avc.sps_active_idx = ctx->avc.pps_active_idx = -1;
		ctx->active_sps = ctx->active_pps = -1;
		ctx->active_sps_crc = ctx->active_pps_crc = 0;

		if (!dsi || !dsi->value.data.ptr) {
			ctx->is_annex_b = GF_TRUE;
			ctx->width=ctx->height=128;
			ctx->out_size = ctx->width*ctx->height*3/2;
			ctx->pix_fmt = GF_PIXEL_YUV;
			return GF_OK;
		} else {
			u32 i;
			GF_AVCConfigSlot *slc;
			GF_AVCConfig *cfg = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
			for (i=0; i<gf_list_count(cfg->sequenceParameterSets); i++) {
				slc = gf_list_get(cfg->sequenceParameterSets, i);
				slc->id = -1;
				vtbdec_register_param_sets(ctx, slc->data, slc->size, GF_TRUE, 0);
			}

			for (i=0; i<gf_list_count(cfg->pictureParameterSets); i++) {
				slc = gf_list_get(cfg->pictureParameterSets, i);
				slc->id = -1;
				vtbdec_register_param_sets(ctx, slc->data, slc->size, GF_FALSE, 0);
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
			if (gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs) && !ctx->reconfig_needed ) {
				e = vtbdec_init_decoder(filter, ctx);
			} else {
				e = GF_OK;
			}
			gf_odf_avc_cfg_del(cfg);
			return e;
		}
	}

	//check HEVC config
	if (codecid==GF_CODECID_HEVC) {
		if (ctx->SPSs) vtbdec_del_param_list(ctx->SPSs);
		ctx->SPSs = gf_list_new();
		if (ctx->SPSs) vtbdec_del_param_list(ctx->PPSs);
		ctx->PPSs = gf_list_new();
		if (ctx->SPSs) vtbdec_del_param_list(ctx->VPSs);
		ctx->VPSs = gf_list_new();
		ctx->is_hevc = GF_TRUE;

		ctx->hevc.sps_active_idx = -1;
		ctx->active_sps = ctx->active_pps = ctx->active_vps = -1;

		if (!dsi || !dsi->value.data.ptr) {
			ctx->is_annex_b = GF_TRUE;
			ctx->width=ctx->height=128;
			ctx->out_size = ctx->width*ctx->height*3/2;
			ctx->pix_fmt = GF_PIXEL_YUV;
			return GF_OK;
		} else {
			u32 i, j;
			GF_AVCConfigSlot *slc;
			GF_HEVCConfig *cfg = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_FALSE);

			for (i=0; i<gf_list_count(cfg->param_array); i++) {
				GF_HEVCParamArray *pa = gf_list_get(cfg->param_array, i);


				for (j=0; j<gf_list_count(pa->nalus); j++) {
					slc = gf_list_get(pa->nalus, j);
					slc->id = -1;

					vtbdec_register_param_sets(ctx, slc->data, slc->size, GF_FALSE, pa->type);
				}
			}

			slc = gf_list_get(ctx->SPSs, 0);
			if (slc) ctx->active_sps = slc->id;

			slc = gf_list_get(ctx->PPSs, 0);
			if (slc) ctx->active_pps = slc->id;

			slc = gf_list_get(ctx->VPSs, 0);
			if (slc) ctx->active_vps = slc->id;

			ctx->nalu_size_length = cfg->nal_unit_size;
			if (gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs)  && gf_list_count(ctx->VPSs)  && !ctx->reconfig_needed) {
				e = vtbdec_init_decoder(filter, ctx);
			} else {
				e = GF_OK;
			}
			gf_odf_hevc_cfg_del(cfg);
			return e;
		}
	}

	if (ctx->vtb_session) {
		assert(ctx->reconfig_needed);
		return GF_OK;
	}

	//check VOSH config
	if (codecid==GF_CODECID_MPEG4_PART2) {
		if (!dsi || !dsi->value.data.ptr) {
			ctx->width=ctx->height=128;
			ctx->out_size = ctx->width*ctx->height*3/2;
			ctx->pix_fmt = GF_PIXEL_YUV;
		} else {
			return vtbdec_init_decoder(filter, ctx);
		}
	}

	return vtbdec_init_decoder(filter, ctx);
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
	vtbdec_del_param_list(ctx->VPSs);
	ctx->VPSs = NULL;
}

static GF_Err vtbdec_parse_nal_units(GF_Filter *filter, GF_VTBDecCtx *ctx, char *inBuffer, u32 inBufferLength, char **out_buffer, u32 *out_size)
{
	u32 i, sc_size=0;
	char *ptr = inBuffer;
	u32 nal_size;
	GF_Err e = GF_OK;
	Bool reassign_bs = GF_TRUE;
	Bool check_reconfig = GF_FALSE;

	if (out_buffer) {
		*out_buffer = NULL;
		*out_size = 0;
	}
	
	if (!ctx->nalu_size_length) {
		nal_size = gf_media_nalu_next_start_code((u8 *) inBuffer, inBufferLength, &sc_size);
		if (!sc_size) return GF_NON_COMPLIANT_BITSTREAM;
		ptr += nal_size + sc_size;
		assert(inBufferLength >= nal_size + sc_size);
		inBufferLength -= nal_size + sc_size;
	}
	
	while (inBufferLength) {
		Bool add_nal = GF_TRUE;
		u8 nal_type, nal_hdr;

		if (ctx->nalu_size_length) {
			nal_size = 0;
			for (i=0; i<ctx->nalu_size_length; i++) {
				nal_size = (nal_size<<8) + ((u8) ptr[i]);
			}

			if (nal_size > inBufferLength) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error parsing NAL: size indicated %d but %d bytes only in payload\n", nal_size, inBufferLength));
				break;
			}
			ptr += ctx->nalu_size_length;
		} else {
			nal_size = gf_media_nalu_next_start_code((const u8 *) ptr, inBufferLength, &sc_size);
		}

        if (nal_size==0) {
            GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error parsing NAL: size 0 shall never happen\n", nal_size));

            if (ctx->nalu_size_length) {
                if (inBufferLength < ctx->nalu_size_length) break;
                inBufferLength -= ctx->nalu_size_length;
            } else {
                if (!sc_size || (inBufferLength < sc_size)) break;
                inBufferLength -= sc_size;
                ptr += sc_size;
            }
            continue;
        }
        
		if (ctx->is_avc) {
			if (!ctx->nal_bs) ctx->nal_bs = gf_bs_new(ptr, nal_size, GF_BITSTREAM_READ);
			else gf_bs_reassign_buffer(ctx->nal_bs, ptr, nal_size);

			nal_hdr = ptr[0];
			nal_type = nal_hdr & 0x1F;
			switch (nal_type) {
			case GF_AVC_NALU_SEQ_PARAM:
				vtbdec_register_param_sets(ctx, ptr, nal_size, GF_TRUE, 0);
				add_nal = GF_FALSE;
				break;
			case GF_AVC_NALU_PIC_PARAM:
				vtbdec_register_param_sets(ctx, ptr, nal_size, GF_FALSE, 0);
				add_nal = GF_FALSE;
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

			gf_media_avc_parse_nalu(ctx->nal_bs, &ctx->avc);

			if ((nal_type<=GF_AVC_NALU_IDR_SLICE) && ctx->avc.s_info.sps) {
				if (ctx->avc.sps_active_idx != ctx->active_sps) {
					ctx->reconfig_needed = 1;
					ctx->active_sps = ctx->avc.sps_active_idx;
					ctx->active_pps = ctx->avc.s_info.pps->id;
					return GF_OK;
				}
			}
		} else if (ctx->is_hevc) {
			u8 temporal_id, ayer_id;

			if (!ctx->nal_bs) ctx->nal_bs = gf_bs_new(ptr, nal_size, GF_BITSTREAM_READ);
			else gf_bs_reassign_buffer(ctx->nal_bs, ptr, nal_size);

			s32 res = gf_media_hevc_parse_nalu_bs(ctx->nal_bs, &ctx->hevc, &nal_type, &temporal_id, &ayer_id);
			if (res>=0) {
				switch (nal_type) {
				case GF_HEVC_NALU_VID_PARAM:
				case GF_HEVC_NALU_SEQ_PARAM:
				case GF_HEVC_NALU_PIC_PARAM:
					vtbdec_register_param_sets(ctx, ptr, nal_size, GF_FALSE, nal_type);
					add_nal = GF_FALSE;
					break;
				case GF_HEVC_NALU_ACCESS_UNIT:
				case GF_HEVC_NALU_END_OF_SEQ:
				case GF_HEVC_NALU_END_OF_STREAM:
				case GF_HEVC_NALU_FILLER_DATA:
					add_nal = GF_FALSE;
					break;
				default:
					break;
				}

				if ((nal_type<=GF_HEVC_NALU_SLICE_CRA) && ctx->hevc.s_info.sps) {
					if (ctx->hevc.sps_active_idx != ctx->active_sps) {
						ctx->reconfig_needed = 1;
						ctx->active_sps = ctx->hevc.sps_active_idx;
						ctx->active_pps = ctx->hevc.s_info.pps->id;
						ctx->active_vps = ctx->hevc.s_info.sps->vps_id;
						return GF_OK;
					}
				}
			}
		}
		
		//if sps and pps are ready, init decoder
		if (!ctx->vtb_session && gf_list_count(ctx->SPSs) && gf_list_count(ctx->PPSs) ) {
			e = vtbdec_init_decoder(filter, ctx);
			if (e) {
				return e;
			}
		}
		
		if (!out_buffer) add_nal = GF_FALSE;
		else if (add_nal && !ctx->vtb_session) add_nal = GF_FALSE;

		if (add_nal) {
			if (reassign_bs) {
				if (!ctx->nalu_rewrite_bs) ctx->nalu_rewrite_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
				else {
					gf_bs_reassign_buffer(ctx->nalu_rewrite_bs, ctx->nalu_buffer, ctx->nalu_buffer_alloc);
					//detach from context until we get the output of the bistream
					ctx->nalu_buffer = NULL;
					ctx->nalu_buffer_alloc = 0;
				}
				reassign_bs = GF_FALSE;
			}
			
			gf_bs_write_u32(ctx->nalu_rewrite_bs, nal_size);
			gf_bs_write_data(ctx->nalu_rewrite_bs, ptr, nal_size);
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

	if (!reassign_bs) {
		//get output without truncating the allocated buffer, repass the buffer at the next AU
		gf_bs_get_content_no_truncate(ctx->nalu_rewrite_bs, &ctx->nalu_buffer, out_size, &ctx->nalu_buffer_alloc);
		*out_buffer = ctx->nalu_buffer;
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[VTB] Outputing frame DTS "LLU" CTS "LLU" timescale %d\n", gf_filter_pck_get_dts(vtbframe->pck_src), gf_filter_pck_get_cts(vtbframe->pck_src), gf_filter_pck_get_timescale(vtbframe->pck_src)));


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
		u8 *dst;
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
					case GF_PIXEL_YUV:
						w /= 2;
						break;
					}
				}
				if (stride != w) {
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

		gf_filter_pck_merge_properties(vtbframe->pck_src, dst_pck);
		ctx->last_cts_out = gf_filter_pck_get_cts(vtbframe->pck_src);
		ctx->last_timescale_out = gf_filter_pck_get_timescale(vtbframe->pck_src);
		gf_filter_pck_unref(vtbframe->pck_src);
		vtbframe->pck_src = NULL;
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
	char *in_data=NULL;
	u32 in_data_size;
	char *in_buffer;
	u32 in_buffer_size, frames_count, i, count, nb_eos;
	u64 min_dts;
	GF_Err e;
	GF_VTBDecCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	GF_FilterPid *ref_pid = NULL;

	//figure out min DTS
	count = gf_list_count(ctx->streams);
	nb_eos = 0;
	min_dts = 0;
	for (i=0; i<count; i++) {
		u64 dts;
		GF_FilterPid *pid = gf_list_get(ctx->streams, i);

		pck = gf_filter_pid_get_packet(pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(pid)) {
				nb_eos++;
				continue;
			} else {
				return GF_OK;
			}
		}
		dts = gf_filter_pck_get_dts(pck);
		dts *= 1000;
		dts /= gf_filter_pck_get_timescale(pck);
		if (!min_dts || (min_dts>dts)) {
			min_dts = dts;
			ref_pid = pid;
		}
	}

	if (nb_eos==count) {
		while (gf_list_count(ctx->frames)) {
			vtbdec_flush_frame(filter, ctx);
		}
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}
	assert(ref_pid);
	pck = gf_filter_pid_get_packet(ref_pid);
	assert(pck);

	in_buffer = (char *) gf_filter_pck_get_data(pck, &in_buffer_size);

	//discard empty packets
	if (!in_buffer || !in_buffer_size) {
		gf_filter_pid_drop_packet(ref_pid);
		//if inbuffer is null this is a hardware frame, should never happen
		return in_buffer ? GF_OK : GF_NOT_SUPPORTED;
	}

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
					gf_filter_pid_drop_packet(ref_pid);
					return e;
				}

				//enfoce removal for all frames
				ctx->skip_mpeg4_vosh = GF_TRUE;
			}
			ctx->vosh_size = dsi.next_object_start;
		} else if (!ctx->vtb_session) {
			gf_filter_pid_drop_packet(ref_pid);
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
				gf_filter_pid_drop_packet(ref_pid);
				return e;
			}
		}

		if (!ctx->vtb_session) {
			gf_filter_pid_drop_packet(ref_pid);
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

		e = vtbdec_parse_nal_units(filter, ctx, in_buffer, in_buffer_size, &in_data, &in_data_size);
		if (e) {
			gf_filter_pid_drop_packet(ref_pid);
			return e;
		}
	} else if (ctx->vosh_size) {
		in_data = in_buffer + ctx->vosh_size;
		in_data_size = in_buffer_size - ctx->vosh_size;
		ctx->vosh_size = 0;
	} else {
		in_data = in_buffer;
		in_data_size = in_buffer_size;
	}

	if (ctx->reconfig_needed) {
		//flush all pending frames
		while (gf_list_count(ctx->frames)) {
			vtbdec_flush_frame(filter, ctx);
		}
		//waiting for last frame to be discarded - this needs checking with the new arch (compositor might not release the last frame)
		if (ctx->no_copy && ctx->decoded_frames_pending) {
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
		return GF_OK;
	}
	if (!ctx->vtb_session) {
		gf_filter_pid_drop_packet(ref_pid);
		return GF_OK;
	}
	

	status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, in_data, in_data_size, kCFAllocatorNull, NULL, 0, in_data_size, 0, &block_buffer);

	if (status ||  (block_buffer == NULL) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Failed to allocate block buffer of %d bytes\n", in_data_size));
		gf_filter_pid_drop_packet(ref_pid);
		return GF_IO_ERR;
	}

	status = CMSampleBufferCreate(kCFAllocatorDefault, block_buffer, TRUE, NULL, NULL, ctx->fmt_desc, 1, 0, NULL, 0, NULL, &sample);

    if (status || (sample==NULL)) {
		if (block_buffer)
			CFRelease(block_buffer);

		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Failed to create sample buffer for %d bytes\n", in_data_size));
		gf_filter_pid_drop_packet(ref_pid);
		return GF_IO_ERR;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[VTB] Decoding frame DTS "LLU" ms\n", min_dts));
	ctx->cur_pck = pck;
	ctx->last_error = GF_OK;
    status = VTDecompressionSessionDecodeFrame(ctx->vtb_session, sample, 0, NULL, 0);
    if (!status)
		status = VTDecompressionSessionWaitForAsynchronousFrames(ctx->vtb_session);
	

	CFRelease(block_buffer);
	CFRelease(sample);

	gf_filter_pid_drop_packet(ref_pid);
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
	GF_FilterFrameInterface *frame = gf_filter_pck_get_frame_interface(pck);
	GF_VTBHWFrame *f = (GF_VTBHWFrame *)frame->user_data;
	if (f->locked) {
		CVPixelBufferUnlockBaseAddress(f->frame, kCVPixelBufferLock_ReadOnly);
	}
#ifdef VTB_GL_TEXTURE
	if (f->y) CVBufferRelease(f->y);
	if (f->u) CVBufferRelease(f->u);
	if (f->v) CVBufferRelease(f->v);
	if (f->ctx->cache_texture)
		GF_CVOpenGLTextureCacheFlush(f->ctx->cache_texture, 0);
#endif
	
	if (f->frame) {
        CVPixelBufferRelease(f->frame);
    }
	f->ctx->decoded_frames_pending--;
	gf_list_add(f->ctx->frames_res, f);
}

GF_Err vtbframe_get_plane(GF_FilterFrameInterface *frame, u32 plane_idx, const u8 **outPlane, u32 *outStride)
{
    OSStatus status;
	GF_Err e;
	GF_VTBHWFrame *f = (GF_VTBHWFrame *)frame->user_data;
	if (! outPlane || !outStride) return GF_BAD_PARAM;
	*outPlane = NULL;

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
		*outPlane = CVPixelBufferGetBaseAddressOfPlane(f->frame, plane_idx);
		if (*outPlane)
			*outStride = (u32) CVPixelBufferGetBytesPerRowOfPlane(f->frame, plane_idx);
	} else if (plane_idx==0) {
		*outStride = (u32) CVPixelBufferGetBytesPerRow(f->frame);
		*outPlane = CVPixelBufferGetBaseAddress(f->frame);
	} else {
		e = GF_BAD_PARAM;
	}
	return e;
}

#ifdef VTB_GL_TEXTURE

/*Define codec matrix*/
typedef struct __matrix GF_CodecMatrix;

#ifdef GPAC_CONFIG_IOS
void *myGetGLContext();
#else

#include <OpenGL/CGLCurrent.h>
void *myGetGLContext()
{
	return CGLGetCurrentContext();
}
#endif


GF_Err vtbframe_get_gl_texture(GF_FilterFrameInterface *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_CodecMatrix * texcoordmatrix)
{
    OSStatus status;
	GLenum target_fmt;
	u32 w, h;
	GF_CVGLTextureREF *outTexture=NULL;
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
#ifdef GPAC_CONFIG_IOS
		status = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, f->ctx->gl_context, NULL, &f->ctx->cache_texture);
#else
		status = CVOpenGLTextureCacheCreate(kCFAllocatorDefault, NULL, f->ctx->gl_context, CGLGetPixelFormat(f->ctx->gl_context), NULL, &f->ctx->cache_texture);
#endif
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

    if (CVPixelBufferIsPlanar(f->frame)) {
		w = (u32) CVPixelBufferGetPlaneCount(f->frame);
		if (plane_idx >= (u32) CVPixelBufferGetPlaneCount(f->frame)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Wrong plane index\n"));
			return GF_BAD_PARAM;
		}
	} else if (plane_idx!=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Wrong plane index %d on interleaved format\n", plane_idx));
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
#ifdef GPAC_CONFIG_IOS
		status = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault, f->ctx->cache_texture, f->frame, NULL, GL_TEXTURE_2D, target_fmt, w, h, target_fmt, GL_UNSIGNED_BYTE, plane_idx, outTexture);
#else
		status = CVOpenGLTextureCacheCreateTextureFromImage(kCFAllocatorDefault, f->ctx->cache_texture, f->frame, NULL, outTexture);
#endif

		if (status != kCVReturnSuccess) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[VTB] Error creating cache texture for plane %d\n", plane_idx));
			return GF_IO_ERR;
		}
	}
	*gl_tex_format = GF_CVOpenGLTextureGetTarget(*outTexture);
	*gl_tex_id = GF_CVOpenGLTextureGetName(*outTexture);

	return GF_OK;
}
#endif

static GF_Err vtbdec_send_output_frame(GF_Filter *filter, GF_VTBDecCtx *ctx)
{
	GF_VTBHWFrame *vtb_frame;
	GF_FilterPacket *dst_pck;

	vtb_frame = gf_list_pop_front(ctx->frames);
	if (!vtb_frame) return GF_BAD_PARAM;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[VTB] Outputing frame DTS "LLU" CTS "LLU" timescale %d\n", gf_filter_pck_get_dts(vtb_frame->pck_src), gf_filter_pck_get_cts(vtb_frame->pck_src), gf_filter_pck_get_timescale(vtb_frame->pck_src)));

	vtb_frame->frame_ifce.user_data = vtb_frame;
	vtb_frame->frame_ifce.get_plane = vtbframe_get_plane;
#ifdef VTB_GL_TEXTURE
	if (ctx->use_gl_textures)
		vtb_frame->frame_ifce.get_gl_texture = vtbframe_get_gl_texture;
#endif

	if (!gf_list_count(ctx->frames) && ctx->reconfig_needed)
		vtb_frame->frame_ifce.blocking = GF_TRUE;

	ctx->decoded_frames_pending++;

	dst_pck = gf_filter_pck_new_frame_interface(ctx->opid, &vtb_frame->frame_ifce, vtbframe_release);

	gf_filter_pck_merge_properties(vtb_frame->pck_src, dst_pck);
	ctx->last_cts_out = gf_filter_pck_get_cts(vtb_frame->pck_src);
	ctx->last_timescale_out = gf_filter_pck_get_timescale(vtb_frame->pck_src);
	gf_filter_pck_unref(vtb_frame->pck_src);
	vtb_frame->pck_src = NULL;

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}


#endif

static Bool vtbdec_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_VTBDecCtx *ctx = (GF_VTBDecCtx *) gf_filter_get_udta(filter);
	if (evt->base.type==GF_FEVT_PLAY) {
		while (gf_list_count(ctx->frames) ) {
			GF_VTBHWFrame *f = gf_list_pop_back(ctx->frames);
			gf_list_add(ctx->frames_res, f);
		}
	}
	return GF_FALSE;
}

static GF_Err vtbdec_initialize(GF_Filter *filter)
{
	GF_VTBDecCtx *ctx = (GF_VTBDecCtx *) gf_filter_get_udta(filter);
#ifdef VTB_GL_TEXTURE
	if (ctx->no_copy)
		ctx->use_gl_textures = GF_TRUE;
#endif

	ctx->frames_res = gf_list_new();
	ctx->frames = gf_list_new();
	ctx->streams = gf_list_new();
	return GF_OK;
}

static void vtbdec_finalize(GF_Filter *filter)
{
	GF_VTBDecCtx *ctx = (GF_VTBDecCtx *) gf_filter_get_udta(filter);
	vtbdec_delete_decoder(ctx);

#ifdef VTB_GL_TEXTURE
	if (ctx->cache_texture) {
		CFRelease(ctx->cache_texture);
    }
#endif

	if (ctx->frames) {
		while (gf_list_count(ctx->frames) ) {
			GF_VTBHWFrame *f = gf_list_pop_back(ctx->frames);
			if (f->pck_src) gf_filter_pck_unref(f->pck_src);
			gf_free(f);
		}
		gf_list_del(ctx->frames);
	}

	if (ctx->frames_res) {
		while (gf_list_count(ctx->frames_res) ) {
			GF_VTBHWFrame *f = gf_list_pop_back(ctx->frames_res);
			if (f->pck_src) gf_filter_pck_unref(f->pck_src);
			gf_free(f);
		}
		gf_list_del(ctx->frames_res);
	}
	gf_list_del(ctx->streams);

	if (ctx->nal_bs) gf_bs_del(ctx->nal_bs);
	if (ctx->ps_bs) gf_bs_del(ctx->ps_bs);
	if (ctx->nalu_rewrite_bs) gf_bs_del(ctx->nalu_rewrite_bs);
	if (ctx->nalu_buffer) gf_free(ctx->nalu_buffer);
}


static const GF_FilterCapability VTBDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,GF_PROP_PID_TILE_BASE, GF_TRUE),

#ifndef GPAC_CONFIG_IOS
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SIMPLE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_MAIN),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SNR),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_SPATIAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_HIGH),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG2_422),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_H263),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_S263),
#endif
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_VTBDecCtx, _n)

static const GF_FilterArgs VTBDecArgs[] =
{
	{ OFFS(reorder), "number of frames to wait for temporal re-ordering", GF_PROP_UINT, "6", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(no_copy), "dispatch VTB frames into filter chain (no copy)", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(ofmt), "set default pixel format for decoded video. If not matched default to nv12", GF_PROP_PIXFMT, "nv12", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(disable_hw), "disable hardware decoding", GF_PROP_BOOL, "false", NULL, 0},
	{}
};

GF_FilterRegister GF_VTBDecCtxRegister = {
	.name = "vtbdec",
	GF_FS_SET_DESCRIPTION("VideoToolBox decoder")
	GF_FS_SET_HELP("This filter decodes MPEG-2, H263, AVC|H264 and HEVC streams through VideoToolBox. It allows GPU frame dispatch or direct frame copy.")
	.private_size = sizeof(GF_VTBDecCtx),
	.args = VTBDecArgs,
	.priority = 1,
	SETCAPS(VTBDecCaps),
	.initialize = vtbdec_initialize,
	.finalize = vtbdec_finalize,
	.configure_pid = vtbdec_configure_pid,
	.process = vtbdec_process,
	.max_extra_pids = 5,
	.process_event = vtbdec_process_event,
};

#else
#undef _GF_MATH_H_
#include <gpac/maths.h>
#include <gpac/filters.h>

#endif // !defined(GPAC_DISABLE_AV_PARSERS) && ( defined(GPAC_CONFIG_DARWIN) || defined(GPAC_CONFIG_IOS) )

const GF_FilterRegister *vtbdec_register(GF_FilterSession *session)
{
#if !defined(GPAC_DISABLE_AV_PARSERS) && ( defined(GPAC_CONFIG_DARWIN) || defined(GPAC_CONFIG_IOS) )
	return &GF_VTBDecCtxRegister;
#else
	return NULL;
#endif
}
