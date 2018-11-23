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
#include <libopenhevc/openhevc.h>

#ifndef GPAC_DISABLE_AV_PARSERS

#define OPEN_SHVC

#if defined(WIN32) && !defined(_WIN32_WCE) && !defined(__GNUC__)
#  pragma comment(lib, "openhevc")

#if !defined _WIN64
void oh_select_view_layer(OHHandle codec, int val)
{
}
#endif

#endif
//
typedef struct
{
	u16 ES_ID;
	u32 width, stride, height, out_size, pixel_ar, layer, nb_threads, luma_bpp, chroma_bpp;

	Bool is_init;
	Bool had_pic;

	u32 hevc_nalu_size_length;
	u32 threading_type;
	Bool direct_output, has_pic;

	GF_ESD *esd;
	OHHandle codec;
	u32 nb_layers, cur_layer, prev_cur_layer;
	u32 output_cb_size;

	Bool decoder_started;

	u32 frame_idx;
	Bool pack_mode;
	Bool reset_dec;
	u32 dec_frames;
	u8  chroma_format_idc;

	u32 nb_views;
	Bool force_stereo, force_stereo_reset;

#ifdef  OHCONFIG_AVCBASE
	u32 avc_base_id;
	u32 avc_nalu_size_length;
	char *avc_base;
	u32 avc_base_size;
	u32 avc_base_pts;
#endif
	FILE *raw_out;
} HEVCDec;

static GF_Err HEVC_ConfigurationScalableStream(HEVCDec *ctx, GF_ESD *esd)
{
	GF_HEVCConfig *cfg = NULL;
	char *data;
	u32 data_len;
	GF_BitStream *bs;
	u32 i, j;

	if (!ctx->codec) return GF_NOT_SUPPORTED;
	if (! esd->has_scalable_layers) return GF_OK;

	if (!esd->decoderConfig->decoderSpecificInfo || !esd->decoderConfig->decoderSpecificInfo->data) {
		ctx->nb_layers++;
		ctx->prev_cur_layer = ctx->cur_layer;
		ctx->cur_layer++;
		oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
		oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
		return GF_OK;
	}

	if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_LHVC) {
		cfg = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_FALSE);
	} else {
		cfg = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_FALSE);
	}

	if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
	if (!ctx->hevc_nalu_size_length) ctx->hevc_nalu_size_length = cfg->nal_unit_size;
	else if (ctx->hevc_nalu_size_length != cfg->nal_unit_size)
		return GF_NON_COMPLIANT_BITSTREAM;

	ctx->nb_layers++;
	ctx->cur_layer++;
	oh_select_active_layer(ctx->codec, ctx->nb_layers-1);
	oh_select_view_layer(ctx->codec, ctx->nb_layers-1);

#ifdef  OHCONFIG_AVCBASE
	//LHVC mode with base AVC layer: set extradata for LHVC
	if (ctx->avc_base_id) {
		oh_extradata_cpy_lhvc(ctx->codec, NULL, (u8 *) esd->decoderConfig->decoderSpecificInfo->data, 0, esd->decoderConfig->decoderSpecificInfo->dataLength);
	} else
#endif
	//LHVC mode with base HEVC layer: decode the LHVC SPS/PPS/VPS
	{
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		for (i=0; i< gf_list_count(cfg->param_array); i++) {
			GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(cfg->param_array, i);
			for (j=0; j< gf_list_count(ar->nalus); j++) {
				GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j);
				gf_bs_write_int(bs, sl->size, 8*ctx->hevc_nalu_size_length);
				gf_bs_write_data(bs, sl->data, sl->size);
			}
		}

		gf_bs_get_content(bs, &data, &data_len);
		gf_bs_del(bs);
		//the decoder may not be already started
		if (!ctx->decoder_started) {
			oh_start(ctx->codec);
			ctx->decoder_started=1;
		}

		oh_decode(ctx->codec, (u8 *)data, data_len, 0);
		if (ctx->raw_out) fwrite((u8 *)data, 1, data_len, ctx->raw_out);

		gf_free(data);
	}
	gf_odf_hevc_cfg_del(cfg);
	return GF_OK;
}

#if defined(OHCONFIG_AVCBASE) && !defined(GPAC_DISABLE_LOG)
void openhevc_log_callback(void *udta, int l, const char*fmt, va_list vl)
{
	u32 level = GF_LOG_DEBUG;
	if (l <= OHEVC_LOG_ERROR) l = GF_LOG_ERROR;
	else if (l <= OHEVC_LOG_WARNING) l = GF_LOG_WARNING;
	else if (l <= OHEVC_LOG_INFO) l = GF_LOG_INFO;
	else if (l >= OHEVC_LOG_VERBOSE) return;

	if (gf_log_tool_level_on(GF_LOG_CODEC, level)) {
		gf_log_va_list(level, GF_LOG_CODEC, fmt, vl);
	}
}
#endif


static GF_Err HEVC_ConfigureStream(HEVCDec *ctx, GF_ESD *esd)
{
	u32 i, j, stride_mul=1;
	ctx->ES_ID = esd->ESID;
	ctx->width = ctx->height = ctx->out_size = ctx->luma_bpp = ctx->chroma_bpp = ctx->chroma_format_idc = 0;

	ctx->nb_layers = 1;
	ctx->cur_layer = 1;

	if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC)
#ifdef  OHCONFIG_AVCBASE
		ctx->avc_base_id=esd->ESID;
#else
	return GF_NOT_SUPPORTED;
#endif

	if (esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
#ifdef  OHCONFIG_AVCBASE
		if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) {
			GF_AVCConfig *avcc = NULL;
			AVCState avc;
			memset(&avc, 0, sizeof(AVCState));

			avcc = gf_odf_avc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
			if (!avcc) return GF_NON_COMPLIANT_BITSTREAM;
			ctx->avc_nalu_size_length = avcc->nal_unit_size;

			for (i=0; i< gf_list_count(avcc->sequenceParameterSets); i++) {
				GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(avcc->sequenceParameterSets, i);
				s32 idx = gf_media_avc_read_sps(sl->data, sl->size, &avc, 0, NULL);
				ctx->width = MAX(avc.sps[idx].width, ctx->width);
				ctx->height = MAX(avc.sps[idx].height, ctx->height);
				ctx->luma_bpp = avcc->luma_bit_depth;
				ctx->chroma_bpp = avcc->chroma_bit_depth;
				ctx->chroma_format_idc = avcc->chroma_format;
			}
			gf_odf_avc_cfg_del(avcc);
		} else

#endif
	{
		GF_HEVCConfig *hvcc = NULL;
		HEVCState hevc;
		memset(&hevc, 0, sizeof(HEVCState));

		hvcc = gf_odf_hevc_cfg_read(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, GF_FALSE);
		if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
		ctx->hevc_nalu_size_length = hvcc->nal_unit_size;

		for (i=0; i< gf_list_count(hvcc->param_array); i++) {
			GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(hvcc->param_array, i);
			for (j=0; j< gf_list_count(ar->nalus); j++) {
				GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j);
				s32 idx;
				u16 hdr = sl->data[0] << 8 | sl->data[1];

				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					idx = gf_media_hevc_read_sps(sl->data, sl->size, &hevc);
					ctx->width = MAX(hevc.sps[idx].width, ctx->width);
					ctx->height = MAX(hevc.sps[idx].height, ctx->height);
					ctx->luma_bpp = MAX(hevc.sps[idx].bit_depth_luma, ctx->luma_bpp);
					ctx->chroma_bpp = MAX(hevc.sps[idx].bit_depth_chroma, ctx->chroma_bpp);
					ctx->chroma_format_idc  = hevc.sps[idx].chroma_format_idc;

					if (hdr & 0x1f8) {
						ctx->nb_layers ++;
					}
				}
				else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
					gf_media_hevc_read_vps(sl->data, sl->size, &hevc);
				}
				else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
					gf_media_hevc_read_pps(sl->data, sl->size, &hevc);
				}
			}
		}
		gf_odf_hevc_cfg_del(hvcc);
	}

	}

#ifdef  OHCONFIG_AVCBASE
	if (ctx->avc_base_id) {
		ctx->codec = oh_init_lhvc(ctx->nb_threads, ctx->threading_type);
	} else
#endif
	{
		ctx->codec = oh_init(ctx->nb_threads, ctx->threading_type);
	}

#if defined(OHCONFIG_AVCBASE) && !defined(GPAC_DISABLE_LOG)

	if (gf_log_tool_level_on(GF_LOG_CODEC, GF_LOG_DEBUG) ) {
		oh_set_log_level(ctx->codec, OHEVC_LOG_DEBUG);
	} else if (gf_log_tool_level_on(GF_LOG_CODEC, GF_LOG_INFO) ) {
		oh_set_log_level(ctx->codec, OHEVC_LOG_INFO);
	} else if (gf_log_tool_level_on(GF_LOG_CODEC, GF_LOG_WARNING) ) {
		oh_set_log_level(ctx->codec, OHEVC_LOG_WARNING);
	} else {
		oh_set_log_level(ctx->codec, OHEVC_LOG_ERROR);
	}
	oh_set_log_callback(ctx->codec, openhevc_log_callback);
#endif

	if (esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
		if (esd->has_scalable_layers) {
			ctx->cur_layer = ctx->nb_layers;
			oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
			oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
		} else {
			oh_select_active_layer(ctx->codec, 1);
			oh_select_view_layer(ctx->codec, 0);
		}

#ifdef  OHCONFIG_AVCBASE
		if (ctx->avc_base_id) {
			oh_extradata_cpy_lhvc(ctx->codec, (u8 *) esd->decoderConfig->decoderSpecificInfo->data, NULL, esd->decoderConfig->decoderSpecificInfo->dataLength, 0);
		} else
#endif
			oh_extradata_cpy(ctx->codec, (u8 *) esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
	}else{
		//decode and display layer 0 by default - will be changed when attaching enhancement layers

		//has_scalable_layers is set, the esd describes a set of HEVC stream but we don't know how many - for now only two decoders so easy,
		//but should be fixed in the future
		if (esd->has_scalable_layers) {
			ctx->nb_layers = 2;
			ctx->cur_layer = 2;
		}
		oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
		oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
	}

	//in case we don't have a config record
	if (!ctx->chroma_format_idc) ctx->chroma_format_idc = 1;

	//we start decoder on the first frame

	//FIXME - we need to get the views info from the decoder ...
	if (ctx->nb_views>1) stride_mul = ctx->nb_layers;
	else if (ctx->force_stereo && ctx->nb_layers>1)
		stride_mul = ctx->nb_layers;

	ctx->stride = ((ctx->luma_bpp==8) && (ctx->chroma_bpp==8)) ? ctx->width : ctx->width * 2;
	if ( ctx->chroma_format_idc  == 1) { // 4:2:0
		ctx->out_size = stride_mul * ctx->stride * ctx->height * 3 / 2;
	}
	else if ( ctx->chroma_format_idc  == 2) { // 4:2:2
		ctx->out_size = stride_mul * ctx->stride * ctx->height * 2 ;
	}
	else if ( ctx->chroma_format_idc  == 3) { // 4:4:4
		ctx->out_size = stride_mul * ctx->stride * ctx->height * 3;
	}
	else {
		return GF_NOT_SUPPORTED;
	}


	ctx->dec_frames = 0;
	return GF_OK;
}

static GF_Err HEVC_AttachStream(GF_BaseDecoder *ifcg, GF_ESD *esd)
{
	GF_SystemRTInfo rti;
	const char *sOpt;
	u32 nb_threads = 1;
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	/*scalable layer, configure enhancement only*/
	if (esd->dependsOnESID) {
		HEVC_ConfigurationScalableStream(ctx, esd);
		return GF_OK;
	}

	if (gf_sys_get_rti(0, &rti, 0) ) {
		nb_threads = (rti.nb_cores>1) ? rti.nb_cores-1 : 1;
	}

	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "Compositor", "NumViews");
	ctx->nb_views = atoi(sOpt);
	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "NumThreads");
	if (!sOpt) {
		char szO[100];
		sprintf(szO, "%d", nb_threads);
		gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "NumThreads", szO);
		ctx->nb_threads = nb_threads;
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[OpenHEVC] Initializing with %d threads\n", ctx->nb_threads));
	} else {
		ctx->nb_threads = atoi(sOpt);
		if (ctx->nb_threads > nb_threads) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[OpenHEVC] Initializing with %d threads but only %d available cores detected on the system\n", ctx->nb_threads, rti.nb_cores));
		}
	}
	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "ForceStereo");
	if (sOpt && !strcmp(sOpt, "yes")) ctx->force_stereo = GF_TRUE;

	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "ThreadingType");
	if (sOpt && !strcmp(sOpt, "wpp")) ctx->threading_type = 2;
	else if (sOpt && !strcmp(sOpt, "frame+wpp")) ctx->threading_type = 4;
	else {
		ctx->threading_type = 1;
		if (!sOpt) gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "ThreadingType", "frame");
	}

	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "CBUnits");
	if (!sOpt) gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "CBUnits", "4");
	if (sOpt) ctx->output_cb_size = atoi(sOpt);
	if (!ctx->output_cb_size) ctx->output_cb_size = 4;

	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "PackHFR");
	if (sOpt && !strcmp(sOpt, "yes") && !ctx->direct_output ) ctx->pack_mode = GF_TRUE;
	else if (!sOpt) gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "PackHFR", "no");

	sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "ResetAtReinit");
	if (sOpt && !strcmp(sOpt, "yes") ) ctx->reset_dec = GF_TRUE;
	else if (!sOpt) gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "ResetAtReinit", "no");

	if (!ctx->raw_out) {
		sOpt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "InputRipFile");
		if (sOpt) ctx->raw_out = fopen(sOpt, "wb");
	}

	ctx->esd = esd;
	return HEVC_ConfigureStream(ctx, esd);
}


static GF_Err HEVC_DetachStream(GF_BaseDecoder *ifcg, u16 ES_ID)
{
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	if (ctx->codec) {
		oh_close(ctx->codec);
		ctx->codec = NULL;
	}
	ctx->is_init = GF_FALSE;
	ctx->width = ctx->height = ctx->out_size = 0;
	return GF_OK;
}

static u32 HEVC_GetPixelFormat( u32 luma_bpp, u8 chroma_format_idc)
{
	u32 ret = 0;
	if (chroma_format_idc == 1)
	{
		ret =  (luma_bpp==10) ? GF_PIXEL_YV12_10 : GF_PIXEL_YV12;
	}
	else if (chroma_format_idc == 2)
	{
		ret = (luma_bpp==10) ? GF_PIXEL_YUV422_10 : GF_PIXEL_YUV422;
	}
	else if (chroma_format_idc == 3)
	{
		ret = (luma_bpp==10) ? GF_PIXEL_YUV444_10 : GF_PIXEL_YUV444;
	}

	return ret;

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
		if (ctx->pack_mode) {
			capability->cap.valueInt *= 2;
		}
		break;
	case GF_CODEC_HEIGHT:
		capability->cap.valueInt = ctx->height;
		if (ctx->pack_mode) {
			capability->cap.valueInt *= 2;
		} else if (ctx->force_stereo && ctx->cur_layer>1) {
			capability->cap.valueInt *= 2;
		}

		break;
	case GF_CODEC_STRIDE:
		capability->cap.valueInt = ctx->stride;
		if (ctx->pack_mode) {
			capability->cap.valueInt *= 2;
		}
		break;
	case GF_CODEC_PAR:
		capability->cap.valueInt = ctx->pixel_ar;
		break;
	case GF_CODEC_OUTPUT_SIZE:
		capability->cap.valueInt = ctx->out_size;
		if (ctx->pack_mode) {
			capability->cap.valueInt *= 4;
		}
		break;
	case GF_CODEC_NBVIEWS:
		capability->cap.valueInt = ctx->force_stereo ? 1 : ctx->nb_views;
		break;
	case GF_CODEC_NBLAYERS:
		capability->cap.valueInt = ctx->force_stereo ? 1 : ctx->nb_layers;
		break;
	case GF_CODEC_PIXEL_FORMAT:
		capability->cap.valueInt = HEVC_GetPixelFormat(ctx->luma_bpp, ctx->chroma_format_idc);
		break;
	case GF_CODEC_BUFFER_MIN:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_BUFFER_MAX:
		capability->cap.valueInt = ctx->output_cb_size;
		break;
	case GF_CODEC_WANTS_THREAD:
		capability->cap.valueBool= GF_TRUE;
		break;
	case GF_CODEC_PADDING_BYTES:
		capability->cap.valueInt = 32;
		break;
	case GF_CODEC_REORDER:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_TRUSTED_CTS:
		capability->cap.valueInt = 1;
		break;
	case GF_CODEC_RAW_MEMORY:
		capability->cap.valueBool = GF_TRUE;
		break;
	case GF_CODEC_FORCE_ANNEXB:
	{
		const char *opt = gf_modules_get_option((GF_BaseInterface *)ifcg, "OpenHEVC", "ForceAnnexB");
		if (!opt)
			gf_modules_set_option((GF_BaseInterface *)ifcg, "OpenHEVC", "ForceAnnexB", "no");
		else if ( !strcmp(opt, "yes"))
			capability->cap.valueBool = GF_TRUE;
	}
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
		if (ctx->reset_dec && ctx->dec_frames) {
			u32 cl = ctx->cur_layer;
			u32 nl = ctx->nb_layers;

			//quick hack, we have an issue with openHEVC resuming after being flushed ...
			ctx->had_pic = GF_FALSE;
			oh_close(ctx->codec);
			ctx->codec = NULL;
			ctx->decoder_started = GF_FALSE;
			ctx->is_init = GF_FALSE;
			HEVC_ConfigureStream(ctx, ctx->esd);
			ctx->cur_layer = cl;
			ctx->nb_layers = nl;
			if (ctx->codec) {
				oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
				oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
			}
		}
		return GF_OK;
	case GF_CODEC_MEDIA_LAYER_DETACH:
		if (ctx->cur_layer<=1) return GF_OK;
		ctx->cur_layer--;
		if (ctx->nb_layers>1) ctx->nb_layers--;
		oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
		oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
		ctx->force_stereo_reset = ctx->force_stereo;
		return GF_OK;
	case GF_CODEC_MEDIA_SWITCH_QUALITY:
		if (ctx->nb_layers==1) return GF_OK;
		if (ctx->nb_layers==1 || ctx->nb_views!=1) return GF_OK;
		/*switch up*/
		if (capability.cap.valueInt > 0) {
			if (ctx->cur_layer>=ctx->nb_layers) return GF_OK;
			ctx->cur_layer++;
		} else {
			if (ctx->cur_layer<=1) return GF_OK;
			ctx->cur_layer--;
		}
		oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
		oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
		ctx->force_stereo_reset = ctx->force_stereo;
		return GF_OK;
	case GF_CODEC_RAW_MEMORY:
		ctx->direct_output = GF_TRUE;
		ctx->pack_mode = GF_FALSE;
		return GF_OK;
	case GF_CODEC_CAN_INIT:
		if (!ctx->decoder_started && ctx->codec) {
			ctx->decoder_started=1;
			oh_start(ctx->codec);
		}
		return GF_OK;
	}
	/*return unsupported to avoid confusion by the player (like color space changing ...) */
	return GF_NOT_SUPPORTED;

}

static GF_Err HEVC_flush_picture(HEVCDec *ctx, char *outBuffer, u32 *outBufferLength, u32 *CTS)
{
	unsigned int a_w, a_h, a_stride, bit_depth;
	OHFrame_cpy openHevcFrame_FL, openHevcFrame_SL;
	int        chromat_format;

	if (ctx->direct_output){
		oh_frameinfo_update(ctx->codec, &openHevcFrame_FL.frame_par);
	}else{
		oh_cropped_frameinfo(ctx->codec, &openHevcFrame_FL.frame_par);
		if (ctx->nb_layers == 2) {
			int res = oh_cropped_frameinfo_from_layer(ctx->codec, &openHevcFrame_SL.frame_par, 1);
			if ( (res==1) && (ctx->prev_cur_layer != ctx->cur_layer)) {
				ctx->prev_cur_layer = ctx->cur_layer;
				ctx->force_stereo_reset = GF_TRUE;
			}
		}
	}

	a_w      = openHevcFrame_FL.frame_par.width;
	a_h      = openHevcFrame_FL.frame_par.height;
	a_stride = openHevcFrame_FL.frame_par.linesize_y;
	bit_depth = openHevcFrame_FL.frame_par.bitdepth;
	chromat_format = openHevcFrame_FL.frame_par.chromat_format;
	*CTS = (u32) openHevcFrame_FL.frame_par.pts;

	if (ctx->force_stereo_reset || !ctx->out_size || (ctx->width != a_w) || (ctx->height!=a_h) || (ctx->stride != a_stride) || (ctx->luma_bpp!= bit_depth)  || (ctx->chroma_bpp != bit_depth) || (ctx->chroma_format_idc != (chromat_format + 1)) )
	{
		/*u32 stride_mul = 1;*/
		ctx->width = a_w;
		ctx->stride = a_stride;
		ctx->height = a_h;
		ctx->force_stereo_reset = GF_FALSE;

		/*if (ctx->nb_views>1) stride_mul = ctx->cur_layer;
		else if (ctx->force_stereo && ctx->cur_layer>1)
			stride_mul = ctx->cur_layer;*/

		if( chromat_format == OH_YUV420 ) {
			ctx->out_size = ctx->stride * ctx->height * 3 / 2;
		} else if  ( chromat_format == OH_YUV422 ) {
			ctx->out_size = ctx->stride * ctx->height * 2;
		} else if ( chromat_format == OH_YUV444 ) {
			ctx->out_size = ctx->stride * ctx->height * 3;
		}
		//force top/bottom output of left and right frame, double height
		if ((ctx->cur_layer==2)  && !ctx->direct_output && (ctx->nb_views>1 || ctx->force_stereo) ){
			ctx->out_size *= 2;
		}

		ctx->had_pic = GF_TRUE;
		ctx->luma_bpp = ctx->chroma_bpp = bit_depth;
		ctx->chroma_format_idc = chromat_format + 1;
		/*always force layer resize*/
		*outBufferLength = ctx->out_size;

		return GF_BUFFER_TOO_SMALL;
	}
	if (ctx->direct_output) {
		*outBufferLength = ctx->out_size;
		ctx->has_pic = GF_TRUE;
		return GF_OK;
	}


	if (ctx->pack_mode) {
		OHFrame openHFrame;
		u8 *pY, *pU, *pV;

		u32 idx_w, idx_h;
		idx_w = ((ctx->frame_idx==0) || (ctx->frame_idx==2)) ? 0 : ctx->stride;
		idx_h = ((ctx->frame_idx==0) || (ctx->frame_idx==1)) ? 0 : ctx->height*2*ctx->stride;

		pY = (u8*) (outBuffer + idx_h + idx_w );


		if (chromat_format == OH_YUV422) {
			pU = (u8*)(outBuffer + 4 * ctx->stride  * ctx->height + idx_w / 2 + idx_h / 2);
			pV = (u8*)(outBuffer + 4 * (3 * ctx->stride * ctx->height /2)  + idx_w / 2 + idx_h / 2);
		} else if (chromat_format == OH_YUV444) {
			pU = (u8*)(outBuffer + 4 * ctx->stride * ctx->height + idx_w + idx_h);
			pV = (u8*)(outBuffer + 4 * ( 2 * ctx->stride * ctx->height) + idx_w + idx_h);
		} else {
			pU = (u8*)(outBuffer + 2 * ctx->stride * 2 * ctx->height + idx_w / 2 + idx_h / 4);
			pV = (u8*)(outBuffer + 4 * ( 5 *ctx->stride  * ctx->height  / 4) + idx_w / 2 + idx_h / 4);

		}

		*outBufferLength = 0;
		if (oh_output_update(ctx->codec, 1, &openHFrame)) {
			u32 i, s_stride, hs_stride, qs_stride, d_stride, dd_stride, hd_stride;

			s_stride = openHFrame.frame_par.linesize_y;
			qs_stride = s_stride / 4;
			hs_stride = s_stride / 2;

			d_stride = ctx->stride;
			dd_stride = 2*ctx->stride;
			hd_stride = ctx->stride/2;

			if (chromat_format == OH_YUV422) {

				for (i = 0; i < ctx->height; i++) {

					memcpy(pY, (u8 *)openHFrame.data_y_p + i*s_stride, d_stride);
					pY += dd_stride;

					memcpy(pU, (u8 *)openHFrame.data_cb_p + i*hs_stride, hd_stride);
					pU += d_stride;

					memcpy(pV, (u8 *)openHFrame.data_cr_p + i*hs_stride, hd_stride);
					pV += d_stride;
				}
			}
			else if (chromat_format == OH_YUV444) {

				for (i = 0; i < ctx->height; i++) {

					memcpy(pY, (u8 *)openHFrame.data_y_p + i*s_stride, d_stride);
					pY += dd_stride;

					memcpy(pU, (u8 *)openHFrame.data_cb_p + i*s_stride, d_stride);
					pU += dd_stride;

					memcpy(pV, (u8 *)openHFrame.data_cr_p + i*s_stride, d_stride);
					pV += dd_stride;
				}
			}
			else {
				for (i = 0; i<ctx->height; i++) {
					memcpy(pY, (u8 *)openHFrame.data_y_p + i*s_stride, d_stride);
					pY += dd_stride;

					if (!(i % 2)) {
						memcpy(pU, (u8 *)openHFrame.data_cb_p + i*qs_stride, hd_stride);
						pU += d_stride;

						memcpy(pV, (u8 *)openHFrame.data_cr_p + i*qs_stride, hd_stride);
						pV += d_stride;
					}
				}
			}

			ctx->frame_idx++;
			if (ctx->frame_idx==4) {
				*outBufferLength = 4 * ctx->out_size;
				ctx->frame_idx = 0;
			}
		}
		return GF_OK;
	}


	*outBufferLength = 0;
	openHevcFrame_FL.data_y = (void*) outBuffer;
	if ((ctx->cur_layer==2)  && !ctx->direct_output && (ctx->nb_views>1 || ctx->force_stereo) ){
		int out1, out2;
		if( chromat_format == OH_YUV420){
			openHevcFrame_SL.data_y = (void*) (outBuffer +  ctx->stride * ctx->height);
			openHevcFrame_FL.data_cb = (void*) (outBuffer + 2*ctx->stride * ctx->height);
			openHevcFrame_SL.data_cb = (void*) (outBuffer +  9*ctx->stride * ctx->height/4);
			openHevcFrame_FL.data_cr = (void*) (outBuffer + 5*ctx->stride * ctx->height/2);
			openHevcFrame_SL.data_cr = (void*) (outBuffer + 11*ctx->stride * ctx->height/4);
		}

		out1 = oh_output_cropped_cpy_from_layer(ctx->codec, &openHevcFrame_FL, 0);
		out2 = oh_output_cropped_cpy_from_layer(ctx->codec, &openHevcFrame_SL, 1);

		if (out1 && out2) *outBufferLength = ctx->out_size*2;

	}else{
		openHevcFrame_FL.data_cb = (void*) (outBuffer + ctx->stride * ctx->height);
		if( chromat_format == OH_YUV420) {
			openHevcFrame_FL.data_cr = (void*) (outBuffer + 5*ctx->stride * ctx->height/4);
		} else if (chromat_format == OH_YUV422) {
			openHevcFrame_FL.data_cr = (void*) (outBuffer + 3*ctx->stride * ctx->height/2);
		} else if ( chromat_format == OH_YUV444) {
			openHevcFrame_FL.data_cr = (void*) (outBuffer + 2*ctx->stride * ctx->height);
		}

		if (oh_output_cropped_cpy(ctx->codec, &openHevcFrame_FL))
			*outBufferLength = ctx->out_size;
	}
	return GF_OK;
}

static GF_Err HEVC_ProcessData(GF_MediaDecoder *ifcg,
                               char *inBuffer, u32 inBufferLength,
                               u16 ES_ID, u32 *CTS,
                               char *outBuffer, u32 *outBufferLength,
                               u8 PaddingBits, u32 mmlevel)
{
	GF_Err e;
	int got_pic;
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;

	if (!ctx->decoder_started) {
		oh_start(ctx->codec);
		ctx->decoder_started=1;
	}

	if (!inBuffer) {
#ifdef  OHCONFIG_AVCBASE
		if (ctx->avc_base_id)
			got_pic = oh_decode_lhvc(ctx->codec, NULL, NULL, 0, 0, 0, 0);
		else
#endif
			got_pic = oh_decode(ctx->codec, NULL, 0, 0);

		if ( got_pic ) {
			return HEVC_flush_picture(ctx, outBuffer, outBufferLength, CTS);
		}
		return GF_OK;
	}

	if (!ES_ID) {
		*outBufferLength = 0;
		return GF_OK;
	}

	if (*outBufferLength < ctx->out_size) {
		*outBufferLength = ctx->out_size;
		return GF_BUFFER_TOO_SMALL;
	}

	*outBufferLength = 0;

	if (ctx->had_pic) {
		ctx->had_pic = GF_FALSE;
		return HEVC_flush_picture(ctx, outBuffer, outBufferLength, CTS);
	}
	ctx->dec_frames++;
	got_pic = 0;

#ifdef  OHCONFIG_AVCBASE
	if (ctx->avc_base_id) {
		if (ctx->avc_base_id == ES_ID) {
			if (ctx->cur_layer<=1) {
				got_pic = oh_decode_lhvc(ctx->codec, (u8 *) inBuffer, NULL, inBufferLength, 0, *CTS, 0);
				if (ctx->avc_base) {
					gf_free(ctx->avc_base);
					ctx->avc_base=NULL;
					ctx->avc_base_size=0;
					ctx->avc_base_pts=0;
				}
			} else {
				ctx->avc_base = gf_realloc(ctx->avc_base, inBufferLength);
				memcpy(ctx->avc_base, inBuffer, inBufferLength);
				ctx->avc_base_size = inBufferLength;
				ctx->avc_base_pts = *CTS;
			}
		} else if (ctx->cur_layer>1) {
			got_pic = oh_decode_lhvc(ctx->codec, (u8*)ctx->avc_base, (u8 *) inBuffer, ctx->avc_base_size, inBufferLength, ctx->avc_base_pts, *CTS);
			if (ctx->avc_base) {
				gf_free(ctx->avc_base);
				ctx->avc_base = NULL;
				ctx->avc_base_size = 0;
			}
		}
	}
	else
#endif
		got_pic = oh_decode(ctx->codec, (u8 *) inBuffer, inBufferLength, *CTS);

	if (ctx->raw_out) fwrite((u8 *) inBuffer, 1, inBufferLength, ctx->raw_out);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[HEVC Decoder] Decode CTS %d - size %d - got pic %d\n", *CTS, inBufferLength, got_pic));
	if (got_pic>0) {
		e = HEVC_flush_picture(ctx, outBuffer, outBufferLength, CTS);
		if (e) return e;
	}

	return GF_OK;
}


static GF_Err HEVC_GetOutputBuffer(GF_MediaDecoder *ifcg, u16 ESID, u8 **pY_or_RGB, u8 **pU, u8 **pV)
{
	s32 res;
	OHFrame openHevcFrame;
	HEVCDec *ctx = (HEVCDec*) ifcg->privateStack;
	if (!ctx->has_pic) return GF_BAD_PARAM;
	ctx->has_pic = GF_FALSE;

	res = oh_output_update(ctx->codec, 1, &openHevcFrame);
	if ((res<=0) || !openHevcFrame.data_y_p || !openHevcFrame.data_cb_p || !openHevcFrame.data_cr_p)
		return GF_SERVICE_ERROR;

	*pY_or_RGB = (u8 *) openHevcFrame.data_y_p;
	*pU = (u8 *) openHevcFrame.data_cb_p;
	*pV = (u8 *) openHevcFrame.data_cr_p;
	return GF_OK;
}



static u32 HEVC_CanHandleStream(GF_BaseDecoder *dec, u32 StreamType, GF_ESD *esd, u8 PL)
{
	if (StreamType != GF_STREAM_VISUAL) return GF_CODEC_NOT_SUPPORTED;

	/*media type query*/
	if (!esd) return GF_CODEC_STREAM_TYPE_SUPPORTED;

	switch (esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_VIDEO_HEVC:
	case GPAC_OTI_VIDEO_LHVC:
		return GF_CODEC_SUPPORTED;
	case GPAC_OTI_VIDEO_AVC:
#ifdef  OHCONFIG_AVCBASE
		return GF_CODEC_MAYBE_SUPPORTED;
#else
		return GF_CODEC_NOT_SUPPORTED;
#endif
	}
	return GF_CODEC_NOT_SUPPORTED;
}

static const char *HEVC_GetCodecName(GF_BaseDecoder *dec)
{
	HEVCDec *ctx = (HEVCDec*) dec->privateStack;
#ifdef  OHCONFIG_AVCBASE
	if (ctx->avc_base_id) {
		if (ctx->cur_layer==1) return "OpenHEVC v"NV_VERSION" - H264";
		else return "OpenHEVC v"NV_VERSION" - H264+LHVC";
	}
	if (ctx->cur_layer==1) return "OpenHEVC v"NV_VERSION;
	else return "OpenHEVC v"NV_VERSION" - LHVC";
#else
	return openhevc_ident();
#endif
}

GF_BaseDecoder *NewHEVCDec()
{
	GF_MediaDecoder *ifcd;
	HEVCDec *dec;

	GF_SAFEALLOC(ifcd, GF_MediaDecoder);
	if (!ifcd) return NULL;
	GF_SAFEALLOC(dec, HEVCDec);
	if (!dec) {
		gf_free(ifcd);
		return NULL;
	}
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
	if (ctx->raw_out) fclose(ctx->raw_out);

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

GPAC_MODULE_STATIC_DECLARATION( openhevc )
