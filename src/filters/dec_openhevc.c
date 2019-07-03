/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / OpenHEVC decoder filter
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


#include <gpac/filters.h>
#include <gpac/avparse.h>
#include <gpac/constants.h>

#if defined(GPAC_HAS_OPENHEVC) && !defined(GPAC_DISABLE_AV_PARSERS)

#include <gpac/internal/media_dev.h>
#include <libopenhevc/openhevc.h>

#if defined(WIN32) && !defined(_WIN32_WCE) && !defined(__GNUC__)
#  pragma comment(lib, "openhevc")
#endif


#define HEVC_MAX_STREAMS 2

typedef struct
{
	GF_FilterPid *ipid;
	u32 cfg_crc;
	u32 id;
	u32 dep_id;
	u32 codec_id;
	Bool sublayer;

	u8 *inject_hdr;
	u32 inject_hdr_size;
} GF_HEVCStream;


typedef struct
{
	//options
	u32 threading;
	Bool no_copy;
	u32 nb_threads;
	Bool pack_hfr;
	Bool seek_reset;
	Bool force_stereo;
	Bool reset_switch;

	//internal
	GF_Filter *filter;
	GF_FilterPid *opid;
	GF_HEVCStream streams[HEVC_MAX_STREAMS];
	u32 nb_streams;
	Bool is_multiview;

	u32 width, stride, height, out_size, luma_bpp, chroma_bpp;
	GF_Fraction sar;
	GF_FilterPacket *packed_pck;
	u8 *packed_data;

	u32 hevc_nalu_size_length;
	Bool has_pic;

	OHHandle codec;
	u32 nb_layers, cur_layer;

	Bool decoder_started;
	
	u32 frame_idx;
	u32 dec_frames;
	u8  chroma_format_idc;

#ifdef  OPENHEVC_HAS_AVC_BASE
	u32 avc_base_id;
	u32 avc_nalu_size_length;
	char *avc_base;
	u32 avc_base_size;
	u32 avc_base_pts;
#endif

	Bool force_stereo_reset;

	GF_FilterFrameInterface frame_ifce;
	OHFrame frame_ptr;
	Bool frame_out;

	char *reaggregation_buffer;
	u32 reaggregation_alloc_size, reaggregation_size;

	char *inject_buffer;
	u32 inject_buffer_alloc_size, reaggregatioinject_buffer_size;

	Bool reconfig_pending;

	Bool signal_reconfig;

	GF_List *src_packets;
} GF_OHEVCDecCtx;

static GF_Err ohevcdec_configure_scalable_pid(GF_OHEVCDecCtx *ctx, GF_FilterPid *pid, u32 codecid)
{
	GF_HEVCConfig *cfg = NULL;
	u8 *data;
	u32 data_len;
	GF_BitStream *bs;
	Bool is_lhvc = GF_TRUE;
	u32 i, j;
	const GF_PropertyValue *dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);

	if (!dsi) {
		dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		is_lhvc = GF_FALSE;
	}

	if (!ctx->codec) return GF_NOT_SUPPORTED;

	if (!dsi || !dsi->value.data.size) {
		ctx->nb_layers++;
		ctx->cur_layer++;
		oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
		oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
		return GF_OK;
	}

	cfg = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, is_lhvc);

	if (!cfg) return GF_NON_COMPLIANT_BITSTREAM;
	if (!ctx->hevc_nalu_size_length) ctx->hevc_nalu_size_length = cfg->nal_unit_size;
	else if (ctx->hevc_nalu_size_length != cfg->nal_unit_size)
		return GF_NON_COMPLIANT_BITSTREAM;
	
	ctx->nb_layers++;
	ctx->cur_layer++;
	oh_select_active_layer(ctx->codec, ctx->nb_layers-1);
	oh_select_view_layer(ctx->codec, ctx->nb_layers-1);

#ifdef  OPENHEVC_HAS_AVC_BASE
	//LHVC mode with base AVC layer: set extradata for LHVC
	if (ctx->avc_base_id) {
		oh_extradata_cpy_lhvc(ctx->codec, NULL, (u8 *) dsi->value.data.ptr, 0, dsi->value.data.size);
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
		gf_free(data);
	}
	gf_odf_hevc_cfg_del(cfg);
	return GF_OK;
}


static void ohevcdec_write_ps_list(GF_BitStream *bs, GF_List *list, u32 nalu_size_length)
{
	u32 i, count = list ? gf_list_count(list) : 0;
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = gf_list_get(list, i);
		gf_bs_write_int(bs, sl->size, 8*nalu_size_length);
		gf_bs_write_data(bs, sl->data, sl->size);
	}
}

static void ohevcdec_create_inband(GF_HEVCStream *stream, u32 nal_unit_size, GF_List *sps, GF_List *pps, GF_List *vps)
{
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (vps) ohevcdec_write_ps_list(bs, vps, nal_unit_size);
	if (sps) ohevcdec_write_ps_list(bs, sps, nal_unit_size);
	if (pps) ohevcdec_write_ps_list(bs, pps, nal_unit_size);
	if (stream->inject_hdr) gf_free(stream->inject_hdr);
	gf_bs_get_content(bs, &stream->inject_hdr, &stream->inject_hdr_size);
	gf_bs_del(bs);
}


#if defined(OPENHEVC_HAS_AVC_BASE) && !defined(GPAC_DISABLE_LOG)
void openhevc_log_callback(void *udta, int l, const char*fmt, va_list vl)
{
	u32 level = GF_LOG_DEBUG;
	if (l <= OHEVC_LOG_ERROR) level = GF_LOG_ERROR;
	else if (l <= OHEVC_LOG_WARNING) level = GF_LOG_WARNING;
	else if (l <= OHEVC_LOG_INFO) level = GF_LOG_INFO;
//	else if (l >= OHEVC_LOG_VERBOSE) return;

	if (gf_log_tool_level_on(GF_LOG_CODEC, level)) {
		gf_log_va_list(level, GF_LOG_CODEC, fmt, vl);
	}
}
#endif

static void ohevcdec_set_codec_name(GF_Filter *filter)
{
	GF_OHEVCDecCtx *ctx = (GF_OHEVCDecCtx*) gf_filter_get_udta(filter);
#ifdef  OPENHEVC_HAS_AVC_BASE
	if (ctx->avc_base_id) {
		if (ctx->cur_layer==1) gf_filter_set_name(filter, "OpenHEVC-v"NV_VERSION"-AVC|H264");
		else gf_filter_set_name(filter, "OpenHEVC-v"NV_VERSION"-AVC|H264+LHVC");
	}
	if (ctx->cur_layer==1) gf_filter_set_name(filter, "OpenHEVC-v"NV_VERSION);
	else gf_filter_set_name(filter, "OpenHEVC-v"NV_VERSION"-LHVC");
#else
	return gf_filter_set_name(filter, libOpenHevcVersion(ctx->codec) );
#endif
}

static u32 ohevcdec_get_pixel_format( u32 luma_bpp, u8 chroma_format_idc)
{
	switch (chroma_format_idc) {
	case 1:
		return (luma_bpp==10) ? GF_PIXEL_YUV_10 : GF_PIXEL_YUV;
	case 2:
		return (luma_bpp==10) ? GF_PIXEL_YUV422_10 : GF_PIXEL_YUV422;
	case 3:
		return (luma_bpp==10) ? GF_PIXEL_YUV444_10 : GF_PIXEL_YUV444;
	default:
		return 0;
	}
	return 0;
}

static void ohevc_set_out_props(GF_OHEVCDecCtx *ctx)
{
	u32 pixfmt;

	gf_filter_pid_copy_properties(ctx->opid, ctx->streams[0].ipid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, NULL );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT, NULL );

	if (ctx->pack_hfr) {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(2*ctx->width) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(2*ctx->height) );
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(2*ctx->stride) );
	} else {
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->width) );
		if (ctx->force_stereo && ctx->is_multiview && ctx->cur_layer>1) {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(2*ctx->height) );
		} else {
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->height) );
		}

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STRIDE, &PROP_UINT(ctx->stride) );
	}
	pixfmt = ohevcdec_get_pixel_format(ctx->luma_bpp, ctx->chroma_format_idc);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_PIXFMT, &PROP_UINT(pixfmt) );
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->sar) );
}

static GF_Err ohevcdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 i, dep_id=0, id=0, cfg_crc=0, codecid;
	Bool has_scalable = GF_FALSE;
	Bool is_sublayer = GF_FALSE;
	u8 *patched_dsi=NULL;
	u32 patched_dsi_size=0;
	GF_HEVCStream *stream;
	const GF_PropertyValue *p, *dsi, *dsi_enh=NULL;
	GF_OHEVCDecCtx *ctx = (GF_OHEVCDecCtx*) gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->streams[0].ipid == pid) {
			memset(ctx->streams, 0, HEVC_MAX_STREAMS*sizeof(GF_HEVCStream));
			if (ctx->opid) gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
			ctx->nb_streams = 0;
			if (ctx->codec) oh_close(ctx->codec);
			ctx->codec = NULL;
			return GF_OK;
		} else {
			for (i=0; i<ctx->nb_streams; i++) {
				if (ctx->streams[i].ipid == pid) {
					ctx->streams[i].ipid = NULL;
					ctx->streams[i].cfg_crc = 0;
					memmove(&ctx->streams[i], &ctx->streams[i+1], sizeof(GF_HEVCStream)*(ctx->nb_streams-1));
					ctx->nb_streams--;
					return GF_OK;
				}
			}
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
	if (p) dep_id = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (!p) p = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	if (p) id = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	codecid = p ? p->value.uint : 0;
	if (!codecid) return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SCALABLE);
	if (p) has_scalable = p->value.boolean;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SUBLAYER);
	if (p) is_sublayer = p->value.boolean;

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	cfg_crc = 0;
	if (dsi && dsi->value.data.ptr && dsi->value.data.size) {
		cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
	}
	dsi_enh = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);

	stream = NULL;
	//check if this is an update
	for (i=0; i<ctx->nb_streams; i++) {
		if (ctx->streams[i].ipid == pid) {
			if (ctx->streams[i].cfg_crc == cfg_crc) return GF_OK;
			if (ctx->codec && (ctx->streams[i].codec_id != codecid)) {
				//we are already instantiated, flush all frames and reconfig
				ctx->reconfig_pending = GF_TRUE;
				return GF_OK;
			}
			ctx->streams[i].codec_id = codecid;
			ctx->streams[i].cfg_crc = cfg_crc;
			stream = &ctx->streams[i];
			break;
		}
	}

	if (!stream) {
		if (ctx->nb_streams==HEVC_MAX_STREAMS) {
			return GF_NOT_SUPPORTED;
		}
		//insert new pid in order of dependencies
		for (i=0; i<ctx->nb_streams; i++) {

			if (!dep_id && !ctx->streams[i].dep_id) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[SVC Decoder] Detected multiple independent base (%s and %s)\n", gf_filter_pid_get_name(pid), gf_filter_pid_get_name(ctx->streams[i].ipid)));
				return GF_REQUIRES_NEW_INSTANCE;
			}

			if (ctx->streams[i].id == dep_id) {
				if (ctx->nb_streams > i+2)
					memmove(&ctx->streams[i+1], &ctx->streams[i+2], sizeof(GF_HEVCStream) * (ctx->nb_streams-i-1));

				ctx->streams[i+1].ipid = pid;
				ctx->streams[i+1].cfg_crc = cfg_crc;
				ctx->streams[i+1].dep_id = dep_id;
				ctx->streams[i+1].id = id;
				ctx->streams[i+1].codec_id = codecid;
				ctx->streams[i+1].sublayer = is_sublayer;
				gf_filter_pid_set_framing_mode(pid, GF_TRUE);
				stream = &ctx->streams[i+1];
				break;
			}
			if (ctx->streams[i].dep_id == id) {
				if (ctx->nb_streams > i+1)
					memmove(&ctx->streams[i+1], &ctx->streams[i], sizeof(GF_HEVCStream) * (ctx->nb_streams-i));

				ctx->streams[i].ipid = pid;
				ctx->streams[i].cfg_crc = cfg_crc;
				ctx->streams[i].dep_id = dep_id;
				ctx->streams[i].id = id;
				ctx->streams[i].codec_id = codecid;
				ctx->streams[i].sublayer = is_sublayer;
				gf_filter_pid_set_framing_mode(pid, GF_TRUE);
				stream = &ctx->streams[i];
				break;
			}
		}
		if (!stream) {
			ctx->streams[ctx->nb_streams].ipid = pid;
			ctx->streams[ctx->nb_streams].cfg_crc = cfg_crc;
			ctx->streams[ctx->nb_streams].id = id;
			ctx->streams[ctx->nb_streams].dep_id = dep_id;
			ctx->streams[ctx->nb_streams].codec_id = codecid;
			ctx->streams[ctx->nb_streams].sublayer = is_sublayer;
			gf_filter_pid_set_framing_mode(pid, GF_TRUE);
			stream = &ctx->streams[ctx->nb_streams];
		}
		ctx->nb_streams++;
		ctx->signal_reconfig = GF_TRUE;
	}
	ctx->nb_layers = ctx->cur_layer = 1;

	//temporal sublayer stream setup, do nothing
	if (stream->sublayer)
		return GF_OK;

	//scalable stream setup
	if (stream->dep_id) {
		GF_Err e;
		e = ohevcdec_configure_scalable_pid(ctx, pid, codecid);
		ohevcdec_set_codec_name(filter);
		return e;
	}


	if (codecid == GF_CODECID_AVC)
#ifdef  OPENHEVC_HAS_AVC_BASE
		ctx->avc_base_id = id;
#else
	return GF_NOT_SUPPORTED;
#endif
	
	if (dsi && dsi->value.data.size) {
#ifdef  OPENHEVC_HAS_AVC_BASE
		if (codecid==GF_CODECID_AVC) {
			GF_AVCConfig *avcc = NULL;
			AVCState avc;
			memset(&avc, 0, sizeof(AVCState));

			avcc = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
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

			if (ctx->codec && ctx->decoder_started) {
				//this seems to be broken in openhevc, we need to reconfigure the decoder ...
				if (ctx->reset_switch) {
					ctx->reconfig_pending = GF_TRUE;
					stream->cfg_crc = 0;
					gf_odf_avc_cfg_del(avcc);
					return GF_OK;
				} else {
					ohevcdec_create_inband(stream, avcc->nal_unit_size, avcc->sequenceParameterSets, avcc->pictureParameterSets, NULL);
				}
			}
			gf_odf_avc_cfg_del(avcc);
		} else

#endif
		{
			GF_HEVCConfig *hvcc = NULL;
			GF_HEVCConfig *hvcc_enh = NULL;
			HEVCState hevc;
			u32 j;
			GF_List *SPSs=NULL, *PPSs=NULL, *VPSs=NULL;
			memset(&hevc, 0, sizeof(HEVCState));

			hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_FALSE);
			if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;

			ctx->hevc_nalu_size_length = hvcc->nal_unit_size;

			if (dsi_enh) {
				hvcc_enh = gf_odf_hevc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size, GF_TRUE);
			}

			for (i=0; i< gf_list_count(hvcc->param_array); i++) {
				GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(hvcc->param_array, i);

				if (hvcc_enh) {
					for (j=0; j< gf_list_count(hvcc_enh->param_array); j++) {
						GF_HEVCParamArray *ar_enh = (GF_HEVCParamArray *)gf_list_get(hvcc_enh->param_array, j);
						if (ar->type==ar_enh->type)
							gf_list_transfer(ar->nalus, ar_enh->nalus);
					}
				}
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
						SPSs = ar->nalus;
					}
					else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
						s32 vps_id = gf_media_hevc_read_vps(sl->data, sl->size, &hevc);
						//multiview
						if ((vps_id>=0) && (hevc.vps[vps_id].scalability_mask[1])) {
							ctx->is_multiview = GF_TRUE;
						}
						VPSs = ar->nalus;
					}
					else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
						gf_media_hevc_read_pps(sl->data, sl->size, &hevc);
						PPSs = ar->nalus;
					}
				}
			}
			if (ctx->codec && ctx->decoder_started) {
				if (ctx->reset_switch) {
					ctx->reconfig_pending = GF_TRUE;
					stream->cfg_crc = 0;
					gf_odf_hevc_cfg_del(hvcc);
					return GF_OK;
				} else {
					ohevcdec_create_inband(stream, hvcc->nal_unit_size, SPSs, PPSs, VPSs);
				}
			}
			if (hvcc_enh) {
				gf_odf_hevc_cfg_del(hvcc_enh);
				gf_odf_hevc_cfg_write(hvcc, &patched_dsi, &patched_dsi_size);
			}
			gf_odf_hevc_cfg_del(hvcc);
		}
	}

	if (!ctx->codec) {
#ifdef  OPENHEVC_HAS_AVC_BASE
		if (ctx->avc_base_id) {
			ctx->codec = oh_init_lhvc(ctx->nb_threads, ctx->threading);
		} else
#endif
		{
			ctx->codec = oh_init(ctx->nb_threads, ctx->threading);
		}
	}


#if defined(OPENHEVC_HAS_AVC_BASE) && !defined(GPAC_DISABLE_LOG)
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

	if (dsi) {
		if (has_scalable) {
			ctx->cur_layer = ctx->nb_layers;
			oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
			oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
		} else {
			//there is a bug with select active layer on win32 with avc base
#ifdef WIN32
			if (!ctx->avc_base_id) {
#endif
				oh_select_active_layer(ctx->codec, 1);
				if (!ctx->avc_base_id)
					oh_select_view_layer(ctx->codec, 0);
#ifdef WIN32
			}
#endif
			}

		if (!ctx->decoder_started) {
#ifdef  OPENHEVC_HAS_AVC_BASE
			if (ctx->avc_base_id) {
				oh_extradata_cpy_lhvc(ctx->codec, (u8 *) dsi->value.data.ptr, NULL, dsi->value.data.size, 0);
			} else
#endif
			{
				if (patched_dsi) {
					oh_extradata_cpy(ctx->codec, (u8 *) patched_dsi, patched_dsi_size);
				} else {
					oh_extradata_cpy(ctx->codec, (u8 *) dsi->value.data.ptr, dsi->value.data.size);
				}
			}
		}
	} else {
		//decode and display layer 0 by default - will be changed when attaching enhancement layers

		//has_scalable_layers is set, the esd describes a set of HEVC stream but we don't know how many - for now only two decoders so easy,
		//but should be fixed in the future
		if (has_scalable) {
			ctx->nb_layers = 2;
			ctx->cur_layer = 2;
		}
		oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
		oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
	}

	if (patched_dsi) gf_free(patched_dsi);

	//in case we don't have a config record
	if (!ctx->chroma_format_idc) ctx->chroma_format_idc = 1;

	//we start decoder on the first frame
	ctx->dec_frames = 0;
	ohevcdec_set_codec_name(filter);

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
	}

	//copy properties at init or reconfig
	if (ctx->signal_reconfig) {
		ohevc_set_out_props(ctx);
		ctx->signal_reconfig = GF_FALSE;
	} else {
		ctx->signal_reconfig = GF_TRUE;
	}
	return GF_OK;
}



static Bool ohevcdec_process_event(GF_Filter *filter, const GF_FilterEvent *fevt)
{
	GF_OHEVCDecCtx *ctx = (GF_OHEVCDecCtx*) gf_filter_get_udta(filter);

	if (fevt->base.type == GF_FEVT_QUALITY_SWITCH) {

		if (ctx->nb_layers==1) return GF_FALSE;
		/*switch up*/
		if (fevt->quality_switch.up > 0) {
			if (ctx->cur_layer>=ctx->nb_layers) return GF_FALSE;
			ctx->cur_layer++;
		} else {
			if (ctx->cur_layer<=1) return GF_FALSE;
			ctx->cur_layer--;
		}
		oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
		oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
		if (ctx->is_multiview)
			ctx->force_stereo_reset = ctx->force_stereo;

		//todo: we should get the set of pids active and trigger the switch up/down based on that
		//rather than not canceling the event
		return GF_FALSE;
	} else if (fevt->base.type == GF_FEVT_STOP) {
		if (ctx->seek_reset && ctx->dec_frames) {
			u32 i;
			u32 cl = ctx->cur_layer;
			u32 nl = ctx->nb_layers;

			//quick hack, we have an issue with openHEVC resuming after being flushed ...
			oh_close(ctx->codec);
			ctx->codec = NULL;
			ctx->decoder_started = GF_FALSE;
			for (i=0; i<ctx->nb_streams; i++) {
				ohevcdec_configure_pid(filter, ctx->streams[i].ipid, GF_FALSE);
			}
			ctx->cur_layer = cl;
			ctx->nb_layers = nl;
			if (ctx->codec) {
				oh_select_active_layer(ctx->codec, ctx->cur_layer-1);
				oh_select_view_layer(ctx->codec, ctx->cur_layer-1);
			}
		}
	}
	return GF_FALSE;
}


void ohevcframe_release(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_OHEVCDecCtx *ctx = (GF_OHEVCDecCtx *) gf_filter_get_udta(filter);
	ctx->frame_out = GF_FALSE;
	gf_filter_post_process_task(ctx->filter);
}

GF_Err ohevcframe_get_plane(GF_FilterFrameInterface *frame, u32 plane_idx, const u8 **outPlane, u32 *outStride)
{
	GF_OHEVCDecCtx *ctx = (GF_OHEVCDecCtx *)frame->user_data;
	if (! outPlane || !outStride) return GF_BAD_PARAM;
	*outPlane = NULL;
	*outStride = 0;

	if (plane_idx==0) {
		*outPlane = (const u8 *) ctx->frame_ptr.data_y_p;
		*outStride = ctx->frame_ptr.frame_par.linesize_y;
	} else if (plane_idx==1) {
		*outPlane = (const u8 *)  ctx->frame_ptr.data_cb_p;
		*outStride = ctx->frame_ptr.frame_par.linesize_cb;
	} else if (plane_idx==2) {
		*outPlane = (const u8 *)  ctx->frame_ptr.data_cr_p;
		*outStride = ctx->frame_ptr.frame_par.linesize_cr;
	} else
		return GF_BAD_PARAM;

	return GF_OK;
}

static GF_Err ohevcdec_send_output_frame(GF_OHEVCDecCtx *ctx)
{
	GF_FilterPacket *dst_pck, *src_pck;
	u32 i, count;

	ctx->frame_ifce.user_data = ctx;
	ctx->frame_ifce.get_plane = ohevcframe_get_plane;
	//we only keep one frame out, force releasing it
	ctx->frame_ifce.blocking = GF_TRUE;
	oh_output_update(ctx->codec, 1, &ctx->frame_ptr);

	dst_pck = gf_filter_pck_new_frame_interface(ctx->opid, &ctx->frame_ifce, ohevcframe_release);

	src_pck = NULL;
	count = gf_list_count(ctx->src_packets);
	for (i=0;i<count; i++) {
		src_pck = gf_list_get(ctx->src_packets, i);
		if (gf_filter_pck_get_cts(src_pck) == ctx->frame_ptr.frame_par.pts) {
			u8 car_v = gf_filter_pck_get_carousel_version(src_pck);
			gf_filter_pck_set_carousel_version(src_pck, 0);

			gf_filter_pck_merge_properties(src_pck, dst_pck);
			gf_list_rem(ctx->src_packets, i);
			gf_filter_pck_unref(src_pck);

			if (car_v)
				ohevc_set_out_props(ctx);

			break;
		}
		src_pck = NULL;
	}
	if (!src_pck)
		gf_filter_pck_set_cts(dst_pck, ctx->frame_ptr.frame_par.pts);

	ctx->frame_out = GF_TRUE;
	gf_filter_pck_send(dst_pck);
	return GF_OK;
}
static GF_Err ohevcdec_flush_picture(GF_OHEVCDecCtx *ctx)
{
	GF_FilterPacket *pck, *src_pck;
	u8 *data;
	u32 a_w, a_h, a_stride, bit_depth, i, count;
	u64 cts;
	OHFrame_cpy openHevcFrame_FL, openHevcFrame_SL;
	int chromat_format;

	if (ctx->no_copy && !ctx->pack_hfr) {
		oh_frameinfo_update(ctx->codec, &openHevcFrame_FL.frame_par);
	} else {
		oh_cropped_frameinfo(ctx->codec, &openHevcFrame_FL.frame_par);
		if (ctx->nb_layers == 2) oh_cropped_frameinfo_from_layer(ctx->codec, &openHevcFrame_SL.frame_par, 1);
	}

	a_w = openHevcFrame_FL.frame_par.width;
	a_h = openHevcFrame_FL.frame_par.height;
	a_stride = openHevcFrame_FL.frame_par.linesize_y;
	bit_depth = openHevcFrame_FL.frame_par.bitdepth;
	chromat_format = openHevcFrame_FL.frame_par.chromat_format;
	cts = (u32) openHevcFrame_FL.frame_par.pts;
	if (!openHevcFrame_FL.frame_par.sample_aspect_ratio.den || !openHevcFrame_FL.frame_par.sample_aspect_ratio.num)
		openHevcFrame_FL.frame_par.sample_aspect_ratio.den = openHevcFrame_FL.frame_par.sample_aspect_ratio.num = 1;
		
	src_pck = NULL;
	count = gf_list_count(ctx->src_packets);
	for (i=0;i<count; i++) {
		src_pck = gf_list_get(ctx->src_packets, i);
		if (gf_filter_pck_get_cts(src_pck) == cts) break;
		src_pck = NULL;
	}

	if (src_pck && gf_filter_pck_get_seek_flag(src_pck)) {
		gf_list_del_item(ctx->src_packets, src_pck);
		gf_filter_pck_unref(src_pck);
		return GF_OK;
	}

	if (ctx->force_stereo_reset || !ctx->out_size || (ctx->width != a_w) || (ctx->height!=a_h) || (ctx->stride != a_stride)
		|| (ctx->luma_bpp!= bit_depth)  || (ctx->chroma_bpp != bit_depth) || (ctx->chroma_format_idc != (chromat_format + 1))
		|| (ctx->sar.num*openHevcFrame_FL.frame_par.sample_aspect_ratio.den != ctx->sar.den*openHevcFrame_FL.frame_par.sample_aspect_ratio.num)
	 ) {
		ctx->width = a_w;
		ctx->stride = a_stride;
		ctx->height = a_h;
		if( chromat_format == OH_YUV420 ) {
			ctx->out_size = ctx->stride * ctx->height * 3 / 2;
		} else if  ( chromat_format == OH_YUV422 ) {
			ctx->out_size = ctx->stride * ctx->height * 2;
		} else if ( chromat_format == OH_YUV444 ) {
			ctx->out_size = ctx->stride * ctx->height * 3;
		} 
		//force top/bottom output of left and right frame, double height
		if (ctx->pack_hfr) {
			ctx->out_size *= 4;
		} else if ((ctx->cur_layer==2) && (ctx->is_multiview || ctx->force_stereo) ){
			ctx->out_size *= 2;
		}

		ctx->luma_bpp = ctx->chroma_bpp = bit_depth;
		ctx->chroma_format_idc = chromat_format + 1;
		ctx->sar.num = openHevcFrame_FL.frame_par.sample_aspect_ratio.num;
		ctx->sar.den = openHevcFrame_FL.frame_par.sample_aspect_ratio.den;
		if (!ctx->sar.num) ctx->sar.num = ctx->sar.den = 1;

		ohevc_set_out_props(ctx);
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[HEVC Decoder] Sending ouput frame CTS "LLU"\n", openHevcFrame_FL.frame_par.pts ));

	if (ctx->no_copy && !ctx->pack_hfr) {
		return ohevcdec_send_output_frame(ctx);
	}

	if (ctx->pack_hfr) {
		OHFrame openHFrame;
		u8 *pY, *pU, *pV;
		u32 idx_w, idx_h;

		idx_w = ((ctx->frame_idx==0) || (ctx->frame_idx==2)) ? 0 : ctx->stride;
		idx_h = ((ctx->frame_idx==0) || (ctx->frame_idx==1)) ? 0 : ctx->height*2*ctx->stride;

		if (!ctx->packed_pck) {
			ctx->packed_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &ctx->packed_data);
			if (src_pck) gf_filter_pck_merge_properties(src_pck, ctx->packed_pck);
			else gf_filter_pck_set_cts(ctx->packed_pck, cts);
		}
		if (src_pck) {
			gf_list_del_item(ctx->src_packets, src_pck);
			gf_filter_pck_unref(src_pck);
		}

		pY = (u8*) (ctx->packed_data + idx_h + idx_w );

		if (chromat_format == OH_YUV422) {
			pU = (u8*)(ctx->packed_data + 4 * ctx->stride  * ctx->height + idx_w / 2 + idx_h / 2);
			pV = (u8*)(ctx->packed_data + 4 * (3 * ctx->stride * ctx->height /2)  + idx_w / 2 + idx_h / 2);
		} else if (chromat_format == OH_YUV444) {
			pU = (u8*)(ctx->packed_data + 4 * ctx->stride * ctx->height + idx_w + idx_h);
			pV = (u8*)(ctx->packed_data + 4 * ( 2 * ctx->stride * ctx->height) + idx_w + idx_h);
		} else {
			pU = (u8*)(ctx->packed_data + 2 * ctx->stride * 2 * ctx->height + idx_w / 2 + idx_h / 4);
			pV = (u8*)(ctx->packed_data + 4 * ( 5 *ctx->stride  * ctx->height  / 4) + idx_w / 2 + idx_h / 4);
		
		}

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
			} else if (chromat_format == OH_YUV444) {
				for (i = 0; i < ctx->height; i++) {
					memcpy(pY, (u8 *)openHFrame.data_y_p + i*s_stride, d_stride);
					pY += dd_stride;

					memcpy(pU, (u8 *)openHFrame.data_cb_p + i*s_stride, d_stride);
					pU += dd_stride;

					memcpy(pV, (u8 *)openHFrame.data_cr_p + i*s_stride, d_stride);
					pV += dd_stride;
				}
			} else {
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
				gf_filter_pck_send(ctx->packed_pck);
				ctx->packed_pck = NULL;
				ctx->frame_idx = 0;
			}
		}
		return GF_OK;
	}

	pck = gf_filter_pck_new_alloc(ctx->opid, ctx->out_size, &data);

	openHevcFrame_FL.data_y = (void*) data;

	if (ctx->nb_layers==2 && ctx->is_multiview && !ctx->no_copy){
		int out1, out2;
		if( chromat_format == OH_YUV420){
			openHevcFrame_SL.data_y = (void*) (data +  ctx->stride * ctx->height);
			openHevcFrame_FL.data_cb = (void*) (data + 2*ctx->stride * ctx->height);
			openHevcFrame_SL.data_cb = (void*) (data +  9*ctx->stride * ctx->height/4);
			openHevcFrame_FL.data_cr = (void*) (data + 5*ctx->stride * ctx->height/2);
			openHevcFrame_SL.data_cr = (void*) (data + 11*ctx->stride * ctx->height/4);
		}

		out1 = oh_output_cropped_cpy_from_layer(ctx->codec, &openHevcFrame_FL, 0);
		out2 = oh_output_cropped_cpy_from_layer(ctx->codec, &openHevcFrame_SL, 1);

		if (out1 && out2) {
			gf_filter_pck_set_cts(pck, cts);
			gf_filter_pck_send(pck);
		} else {
			gf_filter_pck_discard(pck);
		}
	} else {
		openHevcFrame_FL.data_cb = (void*) (data + ctx->stride * ctx->height);
		if( chromat_format == OH_YUV420) {
			openHevcFrame_FL.data_cr = (void*) (data + 5*ctx->stride * ctx->height/4);
		} else if (chromat_format == OH_YUV422) {
			openHevcFrame_FL.data_cr = (void*) (data + 3*ctx->stride * ctx->height/2);
		} else if ( chromat_format == OH_YUV444) {
			openHevcFrame_FL.data_cr = (void*) (data + 2*ctx->stride * ctx->height);
		}

		if (oh_output_cropped_cpy(ctx->codec, &openHevcFrame_FL)) {
			if (src_pck) {
				u8 car_v = gf_filter_pck_get_carousel_version(src_pck);
				gf_filter_pck_set_carousel_version(src_pck, 0);

				gf_filter_pck_merge_properties(src_pck, pck);
				gf_list_del_item(ctx->src_packets, src_pck);
				gf_filter_pck_unref(src_pck);

				if (car_v)
					ohevc_set_out_props(ctx);
			} else {
				gf_filter_pck_set_cts(pck, cts);
			}
			gf_filter_pck_send(pck);
		} else
			gf_filter_pck_discard(pck);
	}
	return GF_OK;
}

static GF_Err ohevcdec_process(GF_Filter *filter)
{
	s32 got_pic;
	u64 min_dts = GF_FILTER_NO_TS;
	u64 min_cts = GF_FILTER_NO_TS;
	u32 idx, nb_eos=0;
	u32 data_size, nbpck;
	char *data;
	Bool has_pic = GF_FALSE;
	GF_FilterPacket *pck_ref = NULL;
	GF_OHEVCDecCtx *ctx = (GF_OHEVCDecCtx*) gf_filter_get_udta(filter);

	if (ctx->frame_out) return GF_EOS;


	if (ctx->reconfig_pending) {
		//wait for each input pid to be ready - this will force reconfig on pids if needed
		for (idx=0; idx<ctx->nb_streams; idx++) {
			GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->streams[idx].ipid);
			if (!pck) return GF_OK;
		}
		oh_flush(ctx->codec);
		//flush
#ifdef  OPENHEVC_HAS_AVC_BASE
		if (ctx->avc_base_id) {
			got_pic = oh_decode_lhvc(ctx->codec, NULL, NULL, 0, 0, 0, 0);
		} else
#endif
			got_pic = oh_decode(ctx->codec, NULL, 0, 0);

		if ( got_pic ) {
			return ohevcdec_flush_picture(ctx);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[HEVC Decoder] Closing decoder for reconfigure\n"));
		//good to go: no more pics in decoder, no frame out
		ctx->reconfig_pending = GF_FALSE;
		oh_close(ctx->codec);
		ctx->codec = NULL;
		ctx->decoder_started = GF_FALSE;
		for (idx=0; idx<ctx->nb_streams; idx++) {
			ohevcdec_configure_pid(filter, ctx->streams[idx].ipid, GF_FALSE);
		}
	}
	if (!ctx->codec) return GF_SERVICE_ERROR;

	//probe all streams
	for (idx=0; idx<ctx->nb_streams; idx++) {
		u64 dts, cts;
		GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->streams[idx].ipid);

		if (ctx->reconfig_pending) return GF_OK;

		if (!pck) {
			if (gf_filter_pid_is_eos(ctx->streams[idx].ipid))
				nb_eos++;
			//make sure we do have a packet on the enhancement
			else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[OpenHEVC] no input packets on running pid %s - postponing decode\n", gf_filter_pid_get_name(ctx->streams[idx].ipid) ) );
				return GF_OK;
			}
			continue;
		}

		dts = gf_filter_pck_get_dts(pck);
		cts = gf_filter_pck_get_cts(pck);

		data = (char *) gf_filter_pck_get_data(pck, &data_size);
		//TODO: this is a clock signaling, for now just trash ..
		if (!data) {
			gf_filter_pid_drop_packet(ctx->streams[idx].ipid);
			idx--;
			continue;
		}
		if (dts==GF_FILTER_NO_TS) dts = cts;
		//get packet with min dts (either a timestamp or a decode order number)
		if (min_dts > dts) {
			min_dts = dts;
			if (cts == GF_FILTER_NO_TS) min_cts = min_dts;
			else min_cts = cts;
			pck_ref = pck;
		}
	}

	//start decoder
	if (!ctx->decoder_started) {
		oh_start(ctx->codec);
		ctx->decoder_started = GF_TRUE;
	}

	if (nb_eos == ctx->nb_streams) {
		while (1) {
#ifdef  OPENHEVC_HAS_AVC_BASE
			if (ctx->avc_base_id)
				got_pic = oh_decode_lhvc(ctx->codec, NULL, NULL, 0, 0, 0, 0);
			else
#endif
				got_pic = oh_decode(ctx->codec, NULL, 0, 0);

			if (got_pic) {
				ohevcdec_flush_picture(ctx);
			}
			else
				break;
		}
		gf_filter_pid_set_eos(ctx->opid);
		while (gf_list_count(ctx->src_packets)) {
			GF_FilterPacket *pck = gf_list_pop_back(ctx->src_packets);
			gf_filter_pck_unref(pck);
		}
		return GF_EOS;
	}

	if (min_cts == GF_FILTER_NO_TS)
		return GF_OK;

	//queue reference to source packet props
	gf_filter_pck_ref_props(&pck_ref);
	gf_list_add(ctx->src_packets, pck_ref);
	gf_filter_pck_set_carousel_version(pck_ref, ctx->signal_reconfig ? 1 : 0);
	ctx->signal_reconfig = GF_FALSE;

	ctx->dec_frames++;
	got_pic = 0;
	ctx->reaggregation_size = 0;
	nbpck = 0;

	for (idx=0; idx<ctx->nb_streams; idx++) {
		u64 dts, cts;

		GF_FilterPacket *pck = gf_filter_pid_get_packet(ctx->streams[idx].ipid);
		if (!pck) continue;

		dts = gf_filter_pck_get_dts(pck);
		cts = gf_filter_pck_get_cts(pck);
		if (dts==GF_FILTER_NO_TS) dts = cts;

		if (min_dts != GF_FILTER_NO_TS) {
			if (min_dts != dts) continue;
		} else if (min_cts != cts) {
			continue;
		}

		data = (char *) gf_filter_pck_get_data(pck, &data_size);

		if (ctx->streams[idx].inject_hdr) {
			if (ctx->inject_buffer_alloc_size < ctx->streams[idx].inject_hdr_size + data_size) {
				ctx->inject_buffer_alloc_size = ctx->streams[idx].inject_hdr_size + data_size;
				ctx->inject_buffer = gf_realloc(ctx->inject_buffer, ctx->inject_buffer_alloc_size);
			}
			memcpy(ctx->inject_buffer, ctx->streams[idx].inject_hdr, ctx->streams[idx].inject_hdr_size);
			memcpy(ctx->inject_buffer+ctx->streams[idx].inject_hdr_size, data, data_size);
			data = ctx->inject_buffer;
			data_size += ctx->streams[idx].inject_hdr_size;

			gf_free(ctx->streams[idx].inject_hdr);
			ctx->streams[idx].inject_hdr=NULL;
			ctx->streams[idx].inject_hdr_size=0;
			
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[HEVC Decoder] Config changed, injecting param sets inband at DTS "LLU" CTS "LLU"\n", dts, cts));
		}

#ifdef  OPENHEVC_HAS_AVC_BASE
		if (ctx->avc_base_id) {
			if (ctx->avc_base_id == ctx->streams[idx].id) {
				got_pic = oh_decode_lhvc(ctx->codec, (u8 *) data, NULL, data_size, 0, cts, 0);
			} else if (ctx->cur_layer>1) {
				got_pic = oh_decode_lhvc(ctx->codec, (u8*)NULL, (u8 *) data, 0, data_size, 0, cts);
			}
		} else
#endif
		{
			if (ctx->nb_streams>1) {
				if (ctx->reaggregation_alloc_size < ctx->reaggregation_size + data_size) {
					ctx->reaggregation_alloc_size = ctx->reaggregation_size + data_size;
					ctx->reaggregation_buffer = gf_realloc(ctx->reaggregation_buffer, sizeof(char)*ctx->reaggregation_alloc_size);
				}
				memcpy(ctx->reaggregation_buffer + ctx->reaggregation_size, data, sizeof(char)*data_size);
				ctx->reaggregation_size += data_size;
			} else {
				got_pic = oh_decode(ctx->codec, (u8 *) data, data_size, cts);
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[HEVC Decoder] PID %s Decode CTS %d - size %d - got pic %d\n", gf_filter_pid_get_name(ctx->streams[idx].ipid), min_cts, data_size, got_pic));

		if (got_pic>0)
			has_pic = GF_TRUE;

		gf_filter_pid_drop_packet(ctx->streams[idx].ipid);
		nbpck++;
	}

	if (ctx->reaggregation_size) {
		got_pic = oh_decode(ctx->codec, (u8 *) ctx->reaggregation_buffer, ctx->reaggregation_size, min_cts);
		ctx->reaggregation_size = 0;
		if (got_pic)
			has_pic = GF_TRUE;
	}


	if (!has_pic) return GF_OK;

	return ohevcdec_flush_picture(ctx);
}

static GF_Err ohevcdec_initialize(GF_Filter *filter)
{
	GF_SystemRTInfo rti;
	GF_OHEVCDecCtx *ctx = (GF_OHEVCDecCtx *) gf_filter_get_udta(filter);
	ctx->filter = filter;
	if (!ctx->nb_threads) {
		if (gf_sys_get_rti(0, &rti, 0) ) {
			ctx->nb_threads = (rti.nb_cores>1) ? rti.nb_cores-1 : 1;
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[OpenHEVCDec] Initializing with %d threads\n", ctx->nb_threads));
		}
	}
	ctx->src_packets = gf_list_new();
	return GF_OK;
}

static void ohevcdec_finalize(GF_Filter *filter)
{
	GF_OHEVCDecCtx *ctx = (GF_OHEVCDecCtx *) gf_filter_get_udta(filter);
	if (ctx->codec) {
		if (!ctx->decoder_started) oh_start(ctx->codec);
		oh_close(ctx->codec);
	}
	if (ctx->reaggregation_buffer) gf_free(ctx->reaggregation_buffer);
	if (ctx->inject_buffer) gf_free(ctx->inject_buffer);

	while (gf_list_count(ctx->src_packets)) {
		GF_FilterPacket *pck = gf_list_pop_back(ctx->src_packets);
		gf_filter_pck_unref(pck);
	}
	gf_list_del(ctx->src_packets);
}

static const GF_FilterCapability OHEVCDecCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,GF_PROP_PID_TILE_BASE, GF_TRUE),

#ifdef  OPENHEVC_HAS_AVC_BASE
	CAP_UINT_PRIORITY(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC, 255),
#endif

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW)
};

#define OFFS(_n)	#_n, offsetof(GF_OHEVCDecCtx, _n)

static const GF_FilterArgs OHEVCDecArgs[] =
{
	{ OFFS(threading), "set threading mode\n"
	"- frameslice: parallel decoding of both frames and slices\n"
	"- frame: parallel decoding of frames\n"
	"- slice: parallel decoding of slices", GF_PROP_UINT, "frame", "frameslice|frame|slice", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(nb_threads), "set number of threads. If 0, uses number of cores minus one", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(no_copy), "directly dispatch internal decoded frame without copy", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(pack_hfr), "pack 4 consecutive frames in a single output", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(seek_reset), "reset decoder when seeking", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(force_stereo), "force stereo output for multiview (top-bottom only)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(reset_switch), "reset decoder at config change", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

GF_FilterRegister OHEVCDecRegister = {
	.name = "ohevcdec",
	GF_FS_SET_DESCRIPTION("OpenHEVC decoder")
	GF_FS_SET_HELP("This filter decodes HEVC and LHVC (HEVC scalable extensions) from one or more PIDs through the OpenHEVC library")
	.private_size = sizeof(GF_OHEVCDecCtx),
	SETCAPS(OHEVCDecCaps),
	.initialize = ohevcdec_initialize,
	.finalize = ohevcdec_finalize,
	.args = OHEVCDecArgs,
	.configure_pid = ohevcdec_configure_pid,
	.process = ohevcdec_process,
	.process_event = ohevcdec_process_event,
	.max_extra_pids = (HEVC_MAX_STREAMS-1),
	//by default take over FFMPEG
	.priority = 100
};

#endif // defined(GPAC_HAS_OPENHEVC) && !defined(GPAC_DISABLE_AV_PARSERS)


#ifndef GPAC_OPENHEVC_STATIC

GPAC_MODULE_EXPORT
GF_FilterRegister *RegisterFilter(GF_FilterSession *session)
#else
const GF_FilterRegister *ohevcdec_register(GF_FilterSession *session)
#endif

{
#if defined(GPAC_HAS_OPENHEVC) && !defined(GPAC_DISABLE_AV_PARSERS)
	return &OHEVCDecRegister;
#else
	return NULL;
#endif
}
