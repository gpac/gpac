/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / mediacodec decoder filter
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


#include <gpac/internal/media_dev.h>
#include <gpac/constants.h>
#include "dec_mediacodec.h"

typedef struct
{
	//opts
	Bool disable_gl;


	GF_List *streams;
	GF_List *frames_res;
	GF_Filter *filter;
	GF_FilterPid *opid;
	u32 codecid;
	u32 cfg_crc;

	AMediaCodec *codec;
	AMediaFormat *format;
	ANativeWindow *window;

	Bool use_gl_textures;
	u32 dequeue_timeout;
	u32 width, height, stride, out_size;
	u32 pix_fmt;
	GF_Fraction sar;

	u32 crop_left, crop_right, crop_top, crop_bottom;

	const char *mime;
	u8 chroma_format, luma_bit_depth, chroma_bit_depth;

	Bool is_adaptive;
	Bool surface_rendering;
	Bool frame_size_changed;
	Bool inputEOS, outputEOS;

	GF_MCDecSurfaceTexture surfaceTex;

	//NAL-based specific
	GF_List *SPSs, *PPSs, *VPSs;
	s32 active_sps, active_pps, active_vps;
	u32 nalu_size_length;
	Bool inject_xps;

	AVCState avc;
	HEVCState hevc;

	u32 decoded_frames_pending;
	Bool reconfig_needed;
	Bool before_exit_registered;

	//hevc
	u32 luma_bpp, chroma_bpp;
	u8 chroma_format_idc;

	ssize_t outIndex;
	GLuint tex_id;
} GF_MCDecCtx;

typedef struct {
	GF_FilterFrameInterface frame_ifce;
	u8 * frame;
	GF_MCDecCtx * ctx;
	ssize_t outIndex;
	Bool flushed;
} GF_MCDecFrame;

enum {
	MCDEC_PPS = 0,
	MCDEC_SPS,
	MCDEC_VPS,
};

#if 0
u8 sdkInt()
{
    char sdk_str[3] = "0";
    //__system_property_get("ro.build.version.sdk", sdk_str, "0");
    return atoi(sdk_str);
}
#endif


void mcdec_init_media_format(GF_MCDecCtx *ctx, AMediaFormat *format)
{
    AMediaFormat_setString(ctx->format, AMEDIAFORMAT_KEY_MIME, ctx->mime);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_WIDTH, ctx->width);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_HEIGHT, ctx->height);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_STRIDE, ctx->stride);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_MAX_INPUT_SIZE, ctx->width * ctx->height);
}


static void mcdec_reset_ps_list(GF_List *list)
{
	while (list && gf_list_count(list)) {
		GF_AVCConfigSlot *slc = gf_list_get(list, 0);
		gf_free(slc->data);
		gf_free(slc);
		gf_list_rem(list, 0);
	}
}


GF_Err mcdec_init_avc_dec(GF_MCDecCtx *ctx)
{
	s32 idx;
	u32 i;
	GF_AVCConfigSlot *sps = NULL;
	GF_AVCConfigSlot *pps = NULL;

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
	ctx->inject_xps = GF_TRUE;

	idx = ctx->active_sps;
	ctx->width = ctx->avc.sps[idx].width;
	ctx->height = ctx->avc.sps[idx].height;
	ctx->crop_left = ctx->avc.sps[idx].crop.left;
	ctx->crop_right = ctx->avc.sps[idx].crop.right;
	ctx->crop_top = ctx->avc.sps[idx].crop.top;
	ctx->crop_bottom = ctx->avc.sps[idx].crop.bottom;
	ctx->crop_left = ctx->avc.sps[idx].crop.left;
	ctx->width +=  ctx->crop_left + ctx->crop_right;
	ctx->height += ctx->crop_top + ctx->crop_bottom;
	ctx->out_size = ctx->height * ctx->width * 3/2 ;

	if (ctx->avc.sps[idx].vui.par_num && ctx->avc.sps[idx].vui.par_den) {
		ctx->sar.num = ctx->avc.sps[idx].vui.par_num;
		ctx->sar.den = ctx->avc.sps[idx].vui.par_den;
	}
	ctx->chroma_format = ctx->avc.sps[idx].chroma_format;
	ctx->luma_bit_depth = 8 + ctx->avc.sps[idx].luma_bit_depth_m8;
	ctx->chroma_bit_depth = 8 + ctx->avc.sps[idx].chroma_bit_depth_m8;
	switch (ctx->chroma_format) {
	case 2:
		if (ctx->luma_bit_depth>8) {
			ctx->pix_fmt = GF_PIXEL_YUV422_10;
		} else {
			ctx->pix_fmt = GF_PIXEL_YUV422;
		}
		break;
	case 3:
		if (ctx->luma_bit_depth>8) {
			ctx->pix_fmt = GF_PIXEL_YUV444_10;
		} else {
			ctx->pix_fmt = GF_PIXEL_YUV444;
		}
		break;
	default:
		if (ctx->luma_bit_depth>8) {
			ctx->pix_fmt = GF_PIXEL_YUV_10;
		}
		break;
	}
	ctx->frame_size_changed = GF_TRUE;
    return GF_OK;
}


GF_Err mcdec_init_m4vp2_dec(GF_MCDecCtx *ctx)
{
	GF_ESD *esd;
	GF_M4VDecSpecInfo vcfg;
	GF_BitStream *bs;
	u8 *dsi_data;
	u32 dsi_size;
	GF_FilterPid *ipid = gf_list_get(ctx->streams, 0);
	const GF_PropertyValue *dcd = gf_filter_pid_get_property(ipid, GF_PROP_PID_DECODER_CONFIG);

	if (!dcd) return GF_NON_COMPLIANT_BITSTREAM;


	gf_m4v_get_config(dcd->value.data.ptr, dcd->value.data.size, &vcfg);
	ctx->width = vcfg.width;
	ctx->height = vcfg.height;

	esd = gf_odf_desc_esd_new(2);
	esd->decoderConfig->decoderSpecificInfo->data = dcd->value.data.ptr;
	esd->decoderConfig->decoderSpecificInfo->dataLength = dcd->value.data.size;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u32(bs, 1);
	gf_odf_desc_write_bs((GF_Descriptor *) esd, bs);
	gf_bs_get_content(bs, &dsi_data, &dsi_size);
	gf_bs_del(bs);

	ctx->mime = "video/mp4v-es";
	AMediaFormat_setBuffer(ctx->format, "csd-0", dsi_data, dsi_size);

	gf_free(dsi_data);
	gf_odf_desc_del((GF_Descriptor*)esd);
	return GF_OK;
}


GF_Err mcdec_init_hevc_dec(GF_MCDecCtx *ctx)
{

	s32 idx;

	u32 i;
	GF_AVCConfigSlot *sps = NULL;
	GF_AVCConfigSlot *pps = NULL;
	GF_AVCConfigSlot *vps = NULL;

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
	for (i=0; i<gf_list_count(ctx->VPSs); i++) {
		vps = gf_list_get(ctx->VPSs, i);
		if (ctx->active_vps<0) ctx->active_vps = vps->id;

		if (vps->id==ctx->active_vps) break;
		vps = NULL;
	}
	if (!vps) return GF_NON_COMPLIANT_BITSTREAM;
	ctx->reconfig_needed = GF_FALSE;
	ctx->inject_xps = GF_TRUE;

	idx = ctx->active_sps;
	ctx->width = ctx->hevc.sps[idx].width;
	ctx->height = ctx->hevc.sps[idx].height;
	ctx->luma_bpp = ctx->hevc.sps[idx].bit_depth_luma;
	ctx->chroma_bpp = ctx->hevc.sps[idx].bit_depth_chroma;
	ctx->chroma_format_idc  = ctx->hevc.sps[idx].chroma_format_idc;

	ctx->stride = ((ctx->luma_bpp==8) && (ctx->chroma_bpp==8)) ? ctx->width : ctx->width * 2;
	if ( ctx->chroma_format_idc  == 1) { // 4:2:0
		ctx->out_size = ctx->stride * ctx->height * 3 / 2;
	}
	else if ( ctx->chroma_format_idc  == 2) { // 4:2:2
		ctx->out_size = ctx->stride * ctx->height * 2 ;
	}
	else if ( ctx->chroma_format_idc  == 3) { // 4:4:4
		ctx->out_size = ctx->stride * ctx->height * 3;
	}
	else {
		return GF_NOT_SUPPORTED;
	}

	switch (ctx->chroma_format) {
	case 2:
		if (ctx->luma_bit_depth>8) {
			ctx->pix_fmt = GF_PIXEL_YUV422_10;
		} else {
			ctx->pix_fmt = GF_PIXEL_YUV422;
		}
		break;
	case 3:
		if (ctx->luma_bit_depth>8) {
			ctx->pix_fmt = GF_PIXEL_YUV444_10;
		} else {
			ctx->pix_fmt = GF_PIXEL_YUV444;
		}
		break;
	default:
		if (ctx->luma_bit_depth>8) {
			ctx->pix_fmt = GF_PIXEL_NV12;
		}
		break;
	}

	ctx->frame_size_changed = GF_TRUE;
    return GF_OK;
}


static GF_Err mcdec_init_decoder(GF_MCDecCtx *ctx) {

    GF_Err err;

    ctx->format = AMediaFormat_new();

    if(!ctx->format) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] AMediaFormat_new failed"));
        return GF_FILTER_NOT_FOUND;
    }

	ctx->sar.num = ctx->sar.den = 0;
    ctx->pix_fmt = GF_PIXEL_NV12;

    switch (ctx->codecid) {
	case GF_CODECID_AVC :
		err = mcdec_init_avc_dec(ctx);
		break;
	case GF_CODECID_MPEG4_PART2 :
		err = mcdec_init_m4vp2_dec(ctx);
		break;
	case GF_CODECID_HEVC:
		err = mcdec_init_hevc_dec(ctx);
		break;
	default:
		return GF_NOT_SUPPORTED;
    }

    if (err != GF_OK) {
        return err;
    }

    ctx->dequeue_timeout = 5000;
    ctx->stride = ctx->width;
    mcdec_init_media_format(ctx, ctx->format);

    if (!ctx->codec) {
		char *decoder_name = mcdec_find_decoder(ctx->mime, ctx->width, ctx->height, &ctx->is_adaptive);
		if(!decoder_name) return GF_PROFILE_NOT_SUPPORTED;

		ctx->codec = AMediaCodec_createCodecByName(decoder_name);
		gf_free(decoder_name);
	}

    if (!ctx->codec) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] AMediaCodec_createDecoderByType failed"));
        return GF_FILTER_NOT_FOUND;
    }

    if (ctx->disable_gl) {
    	ctx->surface_rendering = 0;
	} else if (!ctx->window) {
		if(mcdec_create_surface(ctx->tex_id, &ctx->window, &ctx->surface_rendering, &ctx->surfaceTex) != GF_OK)
			return GF_BAD_PARAM;
	}

	//TODO add support for crypto
	if( AMediaCodec_configure(ctx->codec, ctx->format, (ctx->surface_rendering) ? ctx->window : NULL, NULL, 0) != AMEDIA_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] AMediaCodec_configure failed"));
        return GF_BAD_PARAM;
    }

    if( AMediaCodec_start(ctx->codec) != AMEDIA_OK){
         GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC , ("[MCDec] AMediaCodec_start failed"));
        return GF_BAD_PARAM;
    }

	ctx->inputEOS = GF_FALSE;
    ctx->outputEOS = GF_FALSE;
	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[MCDec] Video size: %d x %d", ctx->width, ctx->height));


	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->stride) );
	if (ctx->sar.den)
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->sar) );

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(ctx->pix_fmt) );
	switch (ctx->codecid) {
	case GF_CODECID_AVC:
		gf_filter_set_name(ctx->filter, "MediaCodec hardware AVC|H264");
		break;
	case GF_CODECID_HEVC:
		gf_filter_set_name(ctx->filter, "MediaCodec hardware HEVC|H265");
		break;
	case GF_CODECID_MPEG4_PART2:
		gf_filter_set_name(ctx->filter, "MediaCodec hardware MPEG-4 Part2");
		break;
	default:
		gf_filter_set_name(ctx->filter, "MediaCodec unsupported");
		break;
	}
	return GF_OK;
}

static void mcdec_register_avc_param_set(GF_MCDecCtx *ctx, u8 *data, u32 size, u8 xps)
{
	Bool add = GF_TRUE;
	u32 i, count;
	s32 ps_id;
	GF_List *dest = (xps == MCDEC_SPS) ? ctx->SPSs : ctx->PPSs;

	if (xps == MCDEC_SPS) {
		ps_id = gf_media_avc_read_sps(data, size, &ctx->avc, 0, NULL);
		if (ps_id<0) return;
	}
	else {
		ps_id = gf_media_avc_read_pps(data, size, &ctx->avc);
		if (ps_id<0) return;
	}
	count = gf_list_count(dest);
	for (i = 0; i<count; i++) {
		GF_AVCConfigSlot *a_slc = gf_list_get(dest, i);
		if (a_slc->id != ps_id) continue;
		//not same size or different content but same ID, remove old xPS
		if ((a_slc->size != size) || memcmp(a_slc->data, data, size)) {
			gf_free(a_slc->data);
			gf_free(a_slc);
			gf_list_rem(dest, i);
			break;
		}
		else {
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
		gf_list_add(dest, slc);

		//force re-activation of sps/pps
		if (xps == MCDEC_SPS) ctx->active_sps = -1;
		else ctx->active_pps = -1;
	}
}

static void mcdec_register_hevc_param_set(GF_MCDecCtx *ctx, u8 *data, u32 size, u8 xps)
{
	Bool add = GF_TRUE;
	u32 i, count;
	s32 ps_id;
	GF_List *dest = NULL; 
	
	switch(xps) {
		case MCDEC_SPS:
			dest = ctx->SPSs;
			ps_id = gf_media_hevc_read_sps(data, size, &ctx->hevc);
			if (ps_id<0) return;
			break;
		case MCDEC_PPS:
			dest = ctx->PPSs;
			ps_id = gf_media_hevc_read_pps(data, size, &ctx->hevc);
			if (ps_id<0) return;
			break;
		case MCDEC_VPS:
			dest = ctx->VPSs;
			ps_id = gf_media_hevc_read_vps(data, size, &ctx->hevc);
			if (ps_id<0) return;
			break;
		default:
			break;
	}
	
	count = gf_list_count(dest);
	for (i = 0; i<count; i++) {
		GF_AVCConfigSlot *a_slc = gf_list_get(dest, i);
		if (a_slc->id != ps_id) continue;
		//not same size or different content but same ID, remove old xPS
		if ((a_slc->size != size) || memcmp(a_slc->data, data, size)) {
			gf_free(a_slc->data);
			gf_free(a_slc);
			gf_list_rem(dest, i);
			break;
		}
		else {
			add = GF_FALSE;
		}
		break;
	}
	if (add && dest) {
		GF_AVCConfigSlot *slc;
		GF_SAFEALLOC(slc, GF_AVCConfigSlot);
		slc->data = gf_malloc(size);
		memcpy(slc->data, data, size);
		slc->size = size;
		slc->id = ps_id;
		gf_list_add(dest, slc);
		
		//force re-activation of sps/pps/vps
		if (xps == MCDEC_SPS) ctx->active_sps = -1;
		else if (xps == MCDEC_PPS) ctx->active_pps = -1;
		else  ctx->active_vps = -1;
	}
}

static GF_Err mcdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p, *dsi;
	const GF_PropertyValue *dcd;
	u32 codecid, dsi_crc;
	Bool do_reset=GF_FALSE;
	u32 i;
	GF_FilterPid *base_pid = NULL;
    GF_MCDecCtx *ctx = gf_filter_get_udta(filter);

	if (is_remove) {
		gf_list_del_item(ctx->streams, pid);
		if (!gf_list_count(ctx->streams)) {
			if (ctx->codec) AMediaCodec_stop(ctx->codec);
			if (ctx->opid) gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid)) {
		return GF_NOT_SUPPORTED;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] Missing codecid, cannot initialize\n"));
		return GF_NOT_SUPPORTED;
	}
	codecid = p->value.uint;

	base_pid = gf_list_get(ctx->streams, 0);
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
	if (!p && base_pid && (base_pid != pid)) return GF_REQUIRES_NEW_INSTANCE;
	else if (p) {
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
	if ((codecid==ctx->codecid) && (dsi_crc == ctx->cfg_crc)) return GF_OK;

	if (gf_list_find(ctx->streams, pid) < 0) {
		gf_list_insert(ctx->streams, pid, 0);
	}
	ctx->cfg_crc = dsi_crc;

	gf_filter_sep_max_extra_input_pids(filter, (codecid==GF_CODECID_HEVC) ? 5 : 0);

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}

	//copy properties at init or reconfig
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );

	dcd = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	//not ready yet
	if (!dcd) return GF_OK;

	if (codecid!=ctx->codecid) do_reset = GF_TRUE;
	else if (!ctx->is_adaptive) do_reset = GF_TRUE;

	ctx->codecid = codecid;


    if (do_reset && ctx->codec) {
    	AMediaCodec_delete(ctx->codec);
    	ctx->codec = NULL;
    	mcdec_reset_ps_list(ctx->SPSs);
    	mcdec_reset_ps_list(ctx->VPSs);
    	mcdec_reset_ps_list(ctx->PPSs);

    	if (ctx->format) AMediaFormat_delete(ctx->format);
    	ctx->format = NULL;
	}

    if (!ctx->tex_id)
       glGenTextures(1, &ctx->tex_id);

	//check AVC config
    if (ctx->codecid == GF_CODECID_AVC) {
		GF_AVCConfig *cfg;
		GF_AVCConfigSlot *slc;

		ctx->mime = "video/avc";
		ctx->avc.sps_active_idx = -1;
		ctx->active_sps = ctx->active_pps = -1;

		cfg = gf_odf_avc_cfg_read(dcd->value.data.ptr, dcd->value.data.size);
		for (i = 0; i<gf_list_count(cfg->sequenceParameterSets); i++) {
			slc = gf_list_get(cfg->sequenceParameterSets, i);
			slc->id = -1;
			mcdec_register_avc_param_set(ctx, slc->data, slc->size, MCDEC_SPS);
		}

		for (i = 0; i<gf_list_count(cfg->pictureParameterSets); i++) {
			slc = gf_list_get(cfg->pictureParameterSets, i);
			slc->id = -1;
			mcdec_register_avc_param_set(ctx, slc->data, slc->size, MCDEC_PPS);
		}
		//activate first SPS/PPS by default
		slc = gf_list_get(ctx->SPSs, 0);
		if (slc) ctx->active_sps = slc->id;

		slc = gf_list_get(ctx->PPSs, 0);
		if (slc) ctx->active_pps = slc->id;

		ctx->nalu_size_length = cfg->nal_unit_size;
		gf_odf_avc_cfg_del(cfg);
    }
	else if (codecid == GF_CODECID_MPEG4_PART2) {
		GF_M4VDecSpecInfo vcfg;
		gf_m4v_get_config(dcd->value.data.ptr, dcd->value.data.size, &vcfg);
		ctx->width = vcfg.width;
		ctx->height = vcfg.height;
		ctx->out_size = ctx->width*ctx->height*3/2;
		ctx->pix_fmt = GF_PIXEL_NV12;
    }
	else if (codecid == GF_CODECID_HEVC) {
		ctx->mime = "video/hevc";
        GF_HEVCConfig *hvcc;
	    GF_AVCConfigSlot *sl;
	    HEVCState hevc;
	    u32 i,j;

	    memset(&hevc, 0, sizeof(HEVCState));

	    hvcc = gf_odf_hevc_cfg_read(dcd->value.data.ptr, dcd->value.data.size, GF_FALSE);
	    if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
	    ctx->nalu_size_length = hvcc->nal_unit_size;

	    for (i=0; i< gf_list_count(hvcc->param_array); i++) {
			GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(hvcc->param_array, i);
			for (j=0; j< gf_list_count(ar->nalus); j++) {
				GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j);

				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					mcdec_register_hevc_param_set(ctx, sl->data, sl->size, MCDEC_SPS);
				} else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
					mcdec_register_hevc_param_set(ctx, sl->data, sl->size, MCDEC_VPS);
				} else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
					mcdec_register_hevc_param_set(ctx, sl->data, sl->size, MCDEC_PPS);
				}
			}
	    }
		//activate first VPS/SPS/PPS by default
	    sl = gf_list_get(ctx->VPSs, 0);
	    if (sl) ctx->active_vps = sl->id;

	    sl = gf_list_get(ctx->SPSs, 0);
	    if (sl) ctx->active_sps = sl->id;

	    sl = gf_list_get(ctx->PPSs, 0);
	    if (sl) ctx->active_pps = sl->id;

	    gf_odf_hevc_cfg_del(hvcc);
    }
	if (ctx->codec) return GF_OK;
	return mcdec_init_decoder(ctx);
}

static void mcdec_write_ps(GF_BitStream *bs, GF_List *ps, s32 active_ps)
{
	u32 i, count;
	if (active_ps<0) return;
	count = gf_list_count(ps);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(ps, i);
		if (sl->id==active_ps) {
			gf_bs_write_u32(bs, 1);
			gf_bs_write_data(bs, sl->data, sl->size);
			return;
		}
	}
}


static GF_Err mcdec_rewrite_annex_b(GF_MCDecCtx *ctx, u8 *inBuffer, u32 inBufferLength, u8 **out_buffer, u32 *out_size)
{
	u32 i;
	u8 *ptr = inBuffer;
	u32 nal_size;
	GF_Err e = GF_OK;
	GF_BitStream *bs = NULL;

	*out_buffer = NULL;
	*out_size = 0;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!bs) return GF_OUT_OF_MEM;

	if (ctx->inject_xps) {
		ctx->inject_xps = GF_FALSE;
		mcdec_write_ps(bs, ctx->VPSs, ctx->active_vps);
		mcdec_write_ps(bs, ctx->SPSs, ctx->active_sps);
		mcdec_write_ps(bs, ctx->PPSs, ctx->active_pps);
	}

	while (inBufferLength) {
		nal_size = 0;
		for (i = 0; i<ctx->nalu_size_length; i++) {
			nal_size = (nal_size << 8) + ((u8)ptr[i]);
		}
		ptr += ctx->nalu_size_length;

		gf_bs_write_u32(bs, 1);
		gf_bs_write_data(bs, ptr, nal_size);


		ptr += nal_size;

		if (inBufferLength < nal_size + ctx->nalu_size_length) break;
		inBufferLength -= nal_size + ctx->nalu_size_length;
	}

	gf_bs_get_content(bs, out_buffer, out_size);
	gf_bs_del(bs);
	return e;
}

static GF_Err mcdec_check_ps_state(GF_MCDecCtx *ctx, u8 *inBuffer, u32 inBufferLength)
{
	u32 nal_size;
	GF_Err e = GF_OK;
	GF_BitStream *bs = gf_bs_new(inBuffer, inBufferLength, GF_BITSTREAM_READ);


	while (gf_bs_available(bs)) {
		u8 nal_type;
		u32 pos;
		//turn off epb removal when reading nal size
		gf_bs_enable_emulation_byte_removal(bs, GF_FALSE);
		nal_size = gf_bs_read_int(bs, ctx->nalu_size_length * 8);
		pos = gf_bs_get_position(bs);

		if (ctx->codecid==GF_CODECID_AVC) {
			//this will turn it back on
			gf_media_avc_parse_nalu (bs, &ctx->avc);
			gf_bs_seek(bs, pos + nal_size);

			nal_type = ctx->avc.last_nal_type_parsed;
			switch (nal_type) {
			case GF_AVC_NALU_SEQ_PARAM:
			case GF_AVC_NALU_PIC_PARAM:
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] AVC Parameter sets still present in packet payload !\n"));
				break;
			default:
				if ((nal_type <= GF_AVC_NALU_IDR_SLICE) && ctx->avc.s_info.sps) {
					if (ctx->avc.sps_active_idx != ctx->active_sps) {
						ctx->reconfig_needed = 1;
						ctx->active_sps = ctx->avc.sps_active_idx;
						ctx->active_pps = ctx->avc.s_info.pps->id;
						gf_bs_del(bs);
						return GF_OK;
					}
				}
				break;
			}
		} else {
			u8 nal_type, quality_id, temporal_id;
			gf_media_hevc_parse_nalu_bs (bs, &ctx->hevc, &nal_type, &temporal_id, &quality_id);
			gf_bs_seek(bs, pos + nal_size);

			switch(nal_type) {
			case GF_HEVC_NALU_SEQ_PARAM:
			case GF_HEVC_NALU_PIC_PARAM:
			case GF_HEVC_NALU_VID_PARAM:
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] HEVC Parameter sets still present in packet payload !\n"));
				break;
			default:
				if ((nal_type <= GF_HEVC_NALU_SLICE_CRA) && ctx->hevc.s_info.sps) {
					if ((ctx->hevc.sps_active_idx != ctx->active_sps) || (ctx->hevc.sps[ctx->active_sps].vps_id != ctx->active_vps)) {
						ctx->reconfig_needed = 1;
						ctx->active_sps = ctx->hevc.sps_active_idx;
						ctx->active_pps = ctx->hevc.s_info.pps->id;
						ctx->active_vps = ctx->hevc.sps[ctx->active_sps].vps_id;
						return GF_OK;
					}
				}
				break;
			}
		}
	}
	gf_bs_del(bs);
	return e;
}

static GF_Err mcdec_send_frame(GF_MCDecCtx *ctx, u8 *frame_buffer, u64 cts);

static GF_Err mcdec_process(GF_Filter *filter)
{
	u64 cts=0;
	u8 *in_buffer;
	u32 in_buffer_size, i, count, nb_eos;
	u64 min_dts;
	GF_Err e;
	GF_MCDecCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck;
	GF_FilterPid *ref_pid = NULL;
	u8 *dec_frame=NULL;

	Bool mcdec_buffer_available = GF_FALSE;

#if FILTER_FIXME
	if (!ctx->before_exit_registered) {
		ctx->before_exit_registered = GF_TRUE;
		if (gf_register_before_exit_function(gf_th_current(), &mcdec_exit_callback) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] Failed to register exit function for the decoder thread %p, try to continue anyway...\n", gf_th_current()));
		}
	}
#endif


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
		in_buffer = NULL;
		in_buffer_size = 0;
	} else {
		assert(ref_pid);
		pck = gf_filter_pid_get_packet(ref_pid);
		assert(pck);

		in_buffer = (u8 *) gf_filter_pck_get_data(pck, &in_buffer_size);
		cts = gf_filter_pck_get_cts(pck);

		//no reconfig but no adaptive, parse NALUs to figure out active ps
		if (!ctx->reconfig_needed && !ctx->is_adaptive) {
			mcdec_check_ps_state(ctx, in_buffer, in_buffer_size);
		}

		if (ctx->reconfig_needed) {
			if (ctx->codec) {
				AMediaCodec_flush(ctx->codec);
				AMediaCodec_stop(ctx->codec);
			}
			e = mcdec_init_decoder(ctx);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] Failed to reconfigure decoder: %s\n", gf_error_to_string(e) ));
				return e;
			}
		}
	}

	if (!ctx->inputEOS) {
		ssize_t inIndex = AMediaCodec_dequeueInputBuffer(ctx->codec, ctx->dequeue_timeout);

		if (inIndex >= 0) {
            size_t inSize;
            u32 flags = 0;
            Bool do_free = GF_TRUE;
			u8 *in_data=NULL;
			u32 in_data_size=0;
            u8 *buffer = (u8 *)AMediaCodec_getInputBuffer(ctx->codec, inIndex, &inSize);

            //rewrite input from isobmf to annexB - this will write the param sets if needed
            if (in_buffer) {
            	mcdec_rewrite_annex_b(ctx, in_buffer, in_buffer_size, &in_data, &in_data_size);

				if (!in_data) {
					in_data = in_buffer;
					in_data_size = in_buffer_size;
					do_free = GF_FALSE;
				}
			}

            if (in_data_size > inSize)  {
                 GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[MCDec] The returned buffer is too small"));
                 if (do_free) gf_free(in_data);
                 return GF_OK;
            }

            if (in_data_size) {
				memcpy(buffer, in_data, in_data_size);
				gf_filter_pid_drop_packet(ref_pid);
                 if (do_free) gf_free(in_data);
			} else {
                 GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[MCDec] AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM input"));
				 ctx->inputEOS = GF_TRUE;
				 flags = AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM;
            }

			if (AMediaCodec_queueInputBuffer(ctx->codec, inIndex, 0, in_data_size, cts, flags) != AMEDIA_OK) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] AMediaCodec_queueInputBuffer failed"));
				return GF_BAD_PARAM;
            }
            mcdec_buffer_available = GF_TRUE;
        }
    }

    if (!ctx->outputEOS) {
		u32 width, height, stride;
		AMediaCodecBufferInfo info;

        ctx->outIndex = AMediaCodec_dequeueOutputBuffer(ctx->codec, &info, ctx->dequeue_timeout);

        switch(ctx->outIndex) {
		case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED:
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[MCDec] AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED"));
			ctx->format = AMediaCodec_getOutputFormat(ctx->codec);
			AMediaFormat_getInt32(ctx->format, AMEDIAFORMAT_KEY_WIDTH, &width);
			AMediaFormat_getInt32(ctx->format, AMEDIAFORMAT_KEY_HEIGHT, &height);
			AMediaFormat_getInt32(ctx->format, AMEDIAFORMAT_KEY_STRIDE, &stride);
			if((ctx->width != width) || (ctx->height != height) || (ctx->stride != stride)){
				ctx->out_size = ctx->stride * ctx->height * 3 / 2;
				ctx->frame_size_changed = GF_TRUE;
			}
			break;

		case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[MCDec] AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED"));
			break;

		case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[MCDec] AMEDIACODEC_INFO_TRY_AGAIN_LATER"));
			break;

		default:
			if (ctx->outIndex < 0)
			 	break;

			dec_frame = NULL;

			if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[MCDec] AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM output"));
				ctx->outputEOS = GF_TRUE;
			}

			if (!ctx->surface_rendering) {
				size_t outSize;
				uint8_t * buffer = AMediaCodec_getOutputBuffer(ctx->codec, ctx->outIndex, &outSize);
				dec_frame = buffer + info.offset;

				if (!dec_frame) {
					 GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] AMediaCodec_getOutputBuffer failed"));
					 break;
				}
			}
			e = mcdec_send_frame(ctx, dec_frame, info.presentationTimeUs);
			if (e) return e;
			break;
		}
    }

	if (ctx->outputEOS) gf_filter_pid_set_eos(ctx->opid);
	return GF_OK;
}

static void mcdec_hw_del(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_FilterFrameInterface *frame_ifce = (GF_FilterFrameInterface *) gf_filter_pck_get_frame_interface(pck);
	GF_MCDecFrame *f = frame_ifce ? frame_ifce->user_data : NULL;
	if (!f) return;

	if (f->ctx->codec && !f->flushed)  {
		if ( AMediaCodec_releaseOutputBuffer(f->ctx->codec, f->outIndex, GF_FALSE) != AMEDIA_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] NOT Release Output Buffer Index: %d", f->outIndex));
		}
	}
	f->ctx->decoded_frames_pending--;
	gf_list_add(f->ctx->frames_res, f);
}

GF_Err mcdec_hw_get_plane(GF_FilterFrameInterface *frame, u32 plane_idx, const u8 **outPlane, u32 *outStride)
{
	GF_MCDecFrame *f = (GF_MCDecFrame *)frame->user_data;
	if (! outPlane || !outStride) return GF_BAD_PARAM;
	*outPlane = NULL;
	*outStride = 0;
	f->ctx->format = AMediaCodec_getOutputFormat(f->ctx->codec);

	AMediaFormat_getInt32(f->ctx->format, AMEDIAFORMAT_KEY_STRIDE, outStride);

	switch(plane_idx) {
	case 0 :
		*outPlane = f->frame ;
		break;
	case 1 :
		*outPlane = f->frame + *outStride * f->ctx->height ;
		*outStride/=2;
		break;
	case 2:
		*outPlane = f->frame + 5 * *outStride * f->ctx->height/4 ;
		*outStride/=2;
		break;
	default :
		return GF_BAD_PARAM;
	}

	return GF_OK;
}

GF_Err mcdec_hw_get_gl_texture(GF_FilterFrameInterface *frame, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_Matrix * texcoordmatrix)
{
	GF_MCDecFrame *f = (GF_MCDecFrame *)frame->user_data;
   	if (!gl_tex_format || !gl_tex_id) return GF_BAD_PARAM;
#ifndef MEDIACODEC_EMUL_API
	*gl_tex_format = GL_TEXTURE_EXTERNAL_OES;
#endif

	*gl_tex_id = f->ctx->tex_id;
	
	if(!f->flushed && f->ctx->codec) {
		if (AMediaCodec_releaseOutputBuffer(f->ctx->codec, f->outIndex, GF_TRUE) != AMEDIA_OK) {
			 GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] NOT Release Output Buffer Index: %d to surface", f->outIndex));
			 return GF_OK;
		}
		if(mcdec_update_surface(f->ctx->surfaceTex) != GF_OK) return GF_BAD_PARAM;
		if(mcdec_get_transform_matrix(texcoordmatrix, f->ctx->surfaceTex) != GF_OK) return GF_BAD_PARAM;
		
		f->flushed = GF_TRUE;
	}
	return GF_OK;
}

static GF_Err mcdec_send_frame(GF_MCDecCtx *ctx, u8 *frame_buffer, u64 cts)
{
	GF_MCDecFrame *mc_frame;
	GF_FilterPacket *dst_pck;

	if(ctx->outIndex < 0 || (!frame_buffer && !ctx->surface_rendering))  return GF_BAD_PARAM;

	mc_frame = gf_list_pop_front(ctx->frames_res);
	if (!mc_frame) {
		GF_SAFEALLOC(mc_frame, GF_MCDecFrame);
		if (!mc_frame) return GF_OUT_OF_MEM;
	}
	memset(&mc_frame->frame_ifce, 0, sizeof(GF_FilterFrameInterface));
	mc_frame->frame_ifce.user_data = mc_frame;
	mc_frame->ctx = ctx;
	mc_frame->frame = frame_buffer;
	mc_frame->outIndex = ctx->outIndex;
	mc_frame->flushed = GF_FALSE;

	if (ctx->surface_rendering) {
		mc_frame->frame_ifce.get_gl_texture = mcdec_hw_get_gl_texture;
	} else {
		mc_frame->frame_ifce.get_plane = mcdec_hw_get_plane;
	}

	if (ctx->frame_size_changed) {
		mc_frame->frame_ifce.blocking = GF_TRUE;
	}
	ctx->decoded_frames_pending++;


	dst_pck = gf_filter_pck_new_frame_interface(ctx->opid, &mc_frame->frame_ifce, mcdec_hw_del);
	if (!dst_pck) return GF_OUT_OF_MEM;

	if (dst_pck) {
		gf_filter_pck_set_cts(dst_pck, cts);
		//TODO, copy properties from source packet !

		gf_filter_pck_send(dst_pck);
	}

	return GF_OK;
}

GF_Err mcdec_initialize(GF_Filter *filter)
{
    GF_MCDecCtx *ctx = gf_filter_get_udta(filter);
    ctx->filter = filter;

	ctx->SPSs = gf_list_new();
	ctx->PPSs = gf_list_new();
	ctx->VPSs = gf_list_new();
	ctx->frames_res = gf_list_new();

	ctx->active_vps = -1;
	ctx->active_sps = -1;
	ctx->active_pps = -1;

    return GF_OK;
}

void mcdec_finalize(GF_Filter *filter)
{
    GF_MCDecCtx *ctx = gf_filter_get_udta(filter);

    if (ctx->format && AMediaFormat_delete(ctx->format) != AMEDIA_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] AMediaFormat_delete failed"));
    }

    if (ctx->codec && AMediaCodec_delete(ctx->codec) != AMEDIA_OK) {
         GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[MCDec] AMediaCodec_delete failed"));
    }

    if(ctx->window) {
		ANativeWindow_release(ctx->window);
		ctx->window = NULL;
    }

    if (ctx->surfaceTex.texture_id)
		mcdec_delete_surface(ctx->surfaceTex);
	
    if(ctx->tex_id)
    	glDeleteTextures (1, &ctx->tex_id);

    mcdec_reset_ps_list(ctx->SPSs);
	gf_list_del(ctx->SPSs);
    mcdec_reset_ps_list(ctx->PPSs);
	gf_list_del(ctx->PPSs);
    mcdec_reset_ps_list(ctx->VPSs);
	gf_list_del(ctx->VPSs);

	while (gf_list_count(ctx->frames_res) ) {
		GF_MCDecFrame *f = gf_list_pop_back(ctx->frames_res);
		gf_free(f);
	}
	gf_list_del(ctx->frames_res);
}



static const GF_FilterCapability MCDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,GF_PROP_PID_TILE_BASE, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

#define OFFS(_n)	#_n, offsetof(GF_MCDecCtx, _n)

static const GF_FilterArgs MCDecArgs[] =
{
	{ OFFS(disable_gl), "Disables OpenGL texture transfer", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(disable_gl), "Disables OpenGL texture transfer", GF_PROP_BOOL, "false", NULL, 0},
	{}
};

GF_FilterRegister GF_MCDecCtxRegister = {
	.name = "mcdec",
	GF_FS_SET_DESCRIPTION("MediaCodec decoder")
	.private_size = sizeof(GF_MCDecCtx),
	.args = MCDecArgs,
	SETCAPS(MCDecCaps),
	.initialize = mcdec_initialize,
	.finalize = mcdec_finalize,
	.configure_pid = mcdec_configure_pid,
	.process = mcdec_process,
};


const GF_FilterRegister *mcdec_register(GF_FilterSession *session)
{
	return &GF_MCDecCtxRegister;
}
