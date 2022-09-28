/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / compressed bitstream metadata rewrite filter
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
#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>
#include <gpac/mpeg4_odf.h>

typedef struct _bsrw_pid_ctx BSRWPid;
typedef struct _bsrw_ctx GF_BSRWCtx;

struct _bsrw_pid_ctx
{
	GF_FilterPid *ipid, *opid;
	u32 codec_id;
	Bool reconfigure;
	GF_Err (*rewrite_pid_config)(GF_BSRWCtx *ctx, BSRWPid *pctx);
	GF_Err (*rewrite_packet)(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck);

	s32 prev_cprim, prev_ctfc, prev_cmx, prev_sar;

	u32 nalu_size_length;

#ifndef GPAC_DISABLE_AV_PARSERS
	GF_VUIInfo vui;
#endif
	Bool rewrite_vui;
};

struct _bsrw_ctx
{
	GF_Fraction sar;
	s32 m4vpl, prof, lev, pcomp, pidc, pspace, gpcflags;
	s32 cprim, ctfc, cmx, vidfmt;
	Bool rmsei, fullrange, novsi, novuitiming;

	GF_List *pids;
	Bool reconfigure;
};

static GF_Err none_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	return gf_filter_pck_forward(pck, pctx->opid);
}

static GF_Err m4v_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	GF_Err e;
	u8 *dsi;
	u32 dsi_size;
	const GF_PropertyValue *prop;

	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_OK;

	pctx->reconfigure = GF_FALSE;


	dsi_size = prop->value.data.size;
	dsi = gf_malloc(sizeof(u8) * dsi_size);
	memcpy(dsi, prop->value.data.ptr, sizeof(u8) * dsi_size);

	if (ctx->sar.num && ctx->sar.den) {
#ifndef GPAC_DISABLE_AV_PARSERS
		e = gf_m4v_rewrite_par(&dsi, &dsi_size, ctx->sar.num, ctx->sar.den);
#else
		e = GF_NOT_SUPPORTED;
#endif
		if (e) {
			gf_free(dsi);
			return e;
		}
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->sar) );
	}
	if (ctx->m4vpl>=0) {
#ifndef GPAC_DISABLE_AV_PARSERS
		gf_m4v_rewrite_pl(&dsi, &dsi_size, (u32) ctx->m4vpl);
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_PROFILE_LEVEL, &PROP_UINT(ctx->m4vpl) );
#else
		return GF_NOT_SUPPORTED;
#endif
	}

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	return GF_OK;
}

static GF_Err nalu_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck, u32 codec_type)
{
	Bool is_sei;
	u32 size, pck_size, final_size;
	GF_FilterPacket *dst;
	u8 *output;
	const u8 *data = gf_filter_pck_get_data(pck, &pck_size);
	if (!data)
		return gf_filter_pck_forward(pck, pctx->opid);


	final_size = 0;
	size=0;
	while (size<pck_size) {
		u8 nal_type=0;
		u32 nal_hdr = pctx->nalu_size_length;
		u32 nal_size = 0;
		while (nal_hdr) {
			nal_size |= data[size];
			size++;
			nal_hdr--;
			if (!nal_hdr) break;
			nal_size<<=8;
		}
		is_sei = GF_FALSE;
		//AVC
		if (codec_type==0) {
			nal_type = data[size] & 0x1F;
			if (nal_type == GF_AVC_NALU_SEI) is_sei = GF_TRUE;
		}
		//HEVC
		else if (codec_type==1) {
			nal_type = (data[size] & 0x7E) >> 1;
			if ((nal_type == GF_HEVC_NALU_SEI_PREFIX) || (nal_type == GF_HEVC_NALU_SEI_SUFFIX))
				is_sei = GF_TRUE;
		}
		//VVC
		else if (codec_type==2) {
			nal_type = data[size+1] >> 3;
			if ((nal_type == GF_VVC_NALU_SEI_PREFIX) || (nal_type == GF_VVC_NALU_SEI_SUFFIX))
				is_sei = GF_TRUE;
		}

		if (!is_sei) {
			final_size += nal_size+pctx->nalu_size_length;
		}
		size += nal_size;
	}
	if (final_size == pck_size)
		return gf_filter_pck_forward(pck, pctx->opid);

	dst = gf_filter_pck_new_alloc(pctx->opid, final_size, &output);
	if (!dst) return GF_OUT_OF_MEM;

	gf_filter_pck_merge_properties(pck, dst);

	size=0;
	while (size<pck_size) {
		u8 nal_type=0;
		u32 nal_hdr = pctx->nalu_size_length;
		u32 nal_size = 0;
		while (nal_hdr) {
			nal_size |= data[size];
			size++;
			nal_hdr--;
			if (!nal_hdr) break;
			nal_size<<=8;
		}
		is_sei = GF_FALSE;

		//AVC
		if (codec_type==0) {
			nal_type = data[size] & 0x1F;
			if (nal_type == GF_AVC_NALU_SEI) is_sei = GF_TRUE;
		}
		//HEVC
		else if (codec_type==1) {
			nal_type = (data[size] & 0x7E) >> 1;
			if ((nal_type == GF_HEVC_NALU_SEI_PREFIX) || (nal_type == GF_HEVC_NALU_SEI_SUFFIX))
				is_sei = GF_TRUE;
		}
		//VVC
		else if (codec_type==2) {
			nal_type = data[size+1] >> 3;
			if ((nal_type == GF_VVC_NALU_SEI_PREFIX) || (nal_type == GF_VVC_NALU_SEI_SUFFIX))
				is_sei = GF_TRUE;
		}

		if (is_sei) {
			size += nal_size;
			continue;
		}
		memcpy(output, &data[size-pctx->nalu_size_length], pctx->nalu_size_length+nal_size);
		output += pctx->nalu_size_length+nal_size;
		size += nal_size;
	}
	return gf_filter_pck_send(dst);
}

static GF_Err avc_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	return nalu_rewrite_packet(ctx, pctx, pck, 0);
}
static GF_Err hevc_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	return nalu_rewrite_packet(ctx, pctx, pck, 1);
}
static GF_Err vvc_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	return nalu_rewrite_packet(ctx, pctx, pck, 2);
}

#ifndef GPAC_DISABLE_AV_PARSERS
static void update_props(BSRWPid *pctx, GF_VUIInfo *vui)
{
	if ((vui->ar_num>0) && (vui->ar_den>0))
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_SAR, &PROP_FRAC_INT(vui->ar_num, vui->ar_den) );

	if (vui->fullrange>0)
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_RANGE, &PROP_BOOL(vui->fullrange) );

	if (!vui->remove_video_info) {
		if (vui->color_prim>=0)
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_PRIMARIES, &PROP_UINT(vui->color_prim) );
		if (vui->color_tfc>=0)
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_TRANSFER, &PROP_UINT(vui->color_tfc) );
		if (vui->color_matrix>=0)
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_MX, &PROP_UINT(vui->color_matrix) );
	}
}
#endif /*GPAC_DISABLE_AV_PARSERS*/

static GF_Err avc_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	GF_AVCConfig *avcc;
	GF_Err e = GF_OK;
	u8 *dsi;
	u32 dsi_size;
	const GF_PropertyValue *prop;

	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_OK;
	pctx->reconfigure = GF_FALSE;

	avcc = gf_odf_avc_cfg_read(prop->value.data.ptr, prop->value.data.size);

	if (pctx->rewrite_vui) {
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_VUIInfo tmp_vui = pctx->vui;
		tmp_vui.update = GF_TRUE;
		e = gf_avc_change_vui(avcc, &tmp_vui);
		if (!e) {
			update_props(pctx, &tmp_vui);
		}
#else
		return GF_NOT_SUPPORTED;
#endif /*GPAC_DISABLE_AV_PARSERS*/
	}

	if ((ctx->lev>=0) || (ctx->prof>=0) || (ctx->pcomp>=0)) {
		u32 i, count = gf_list_count(avcc->sequenceParameterSets);
		for (i=0; i<count; i++) {
			GF_NALUFFParam *sps = gf_list_get(avcc->sequenceParameterSets, i);
			//first byte is nalu header, then profile_idc (8bits), prof_comp (8bits), and level_idc (8bits)
			if (ctx->prof>=0) {
				sps->data[1] = (u8) ctx->prof;
			}
			if (ctx->pcomp>=0) {
				sps->data[2] = (u8) ctx->pcomp;
			}
			if (ctx->lev>=0) {
				sps->data[3] = (u8) ctx->lev;
			}
		}
		if (ctx->lev>=0) avcc->AVCLevelIndication = ctx->lev;
		if (ctx->prof>=0) avcc->AVCProfileIndication = ctx->prof;
		if (ctx->pcomp>=0) avcc->profile_compatibility = ctx->pcomp;
	}

	gf_odf_avc_cfg_write(avcc, &dsi, &dsi_size);
	pctx->nalu_size_length = avcc->nal_unit_size;

	gf_odf_avc_cfg_del(avcc);
	if (e) return e;

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	if (ctx->rmsei) {
		pctx->rewrite_packet = avc_rewrite_packet;
	} else {
		pctx->rewrite_packet = none_rewrite_packet;
	}
	return GF_OK;
}

#ifndef GPAC_DISABLE_HEVC

static GF_Err hevc_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	GF_HEVCConfig *hvcc;
	GF_Err e = GF_OK;
	u8 *dsi;
	u32 dsi_size;
	const GF_PropertyValue *prop;

	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_OK;
	pctx->reconfigure = GF_FALSE;

	hvcc = gf_odf_hevc_cfg_read(prop->value.data.ptr, prop->value.data.size, (pctx->codec_id==GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);
	if (pctx->rewrite_vui) {
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_VUIInfo tmp_vui = pctx->vui;
		tmp_vui.update = GF_TRUE;
		e = gf_hevc_change_vui(hvcc, &tmp_vui);
		if (!e) {
			update_props(pctx, &tmp_vui);
		}
#else
		return GF_NOT_SUPPORTED;
#endif /*GPAC_DISABLE_AV_PARSERS*/
	}

	if (ctx->pidc>=0) hvcc->profile_idc = ctx->pidc;
	if (ctx->pspace>=0) hvcc->profile_space = ctx->pspace;
	if (ctx->gpcflags>=0) hvcc->general_profile_compatibility_flags = ctx->gpcflags;


	gf_odf_hevc_cfg_write(hvcc, &dsi, &dsi_size);
	pctx->nalu_size_length = hvcc->nal_unit_size;
	gf_odf_hevc_cfg_del(hvcc);
	if (e) return e;

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	if (ctx->rmsei) {
		pctx->rewrite_packet = hevc_rewrite_packet;
	} else {
		pctx->rewrite_packet = none_rewrite_packet;
	}
	return GF_OK;
}
#endif // GPAC_DISABLE_HEVC


static GF_Err vvc_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_VVCConfig *vvcc;
	GF_Err e = GF_OK;
	u8 *dsi;
	u32 dsi_size;
	const GF_PropertyValue *prop;

	prop = gf_filter_pid_get_property(pctx->ipid, GF_PROP_PID_DECODER_CONFIG);
	if (!prop) return GF_OK;
	pctx->reconfigure = GF_FALSE;

	vvcc = gf_odf_vvc_cfg_read(prop->value.data.ptr, prop->value.data.size);
	if (pctx->rewrite_vui) {
		GF_VUIInfo tmp_vui = pctx->vui;
		tmp_vui.update = GF_TRUE;
		e = gf_vvc_change_vui(vvcc, &tmp_vui);
		if (!e) {
			update_props(pctx, &tmp_vui);
		}
	}

	if (ctx->pidc>=0) vvcc->general_profile_idc = ctx->pidc;
	if (ctx->lev>=0) vvcc->general_level_idc = ctx->pidc;


	gf_odf_vvc_cfg_write(vvcc, &dsi, &dsi_size);
	pctx->nalu_size_length = vvcc->nal_unit_size;
	gf_odf_vvc_cfg_del(vvcc);
	if (e) return e;

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	if (ctx->rmsei) {
		pctx->rewrite_packet = vvc_rewrite_packet;
	} else {
		pctx->rewrite_packet = none_rewrite_packet;
	}
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif /*GPAC_DISABLE_AV_PARSERS*/
}

static GF_Err none_rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	pctx->reconfigure = GF_FALSE;
	return GF_OK;
}

static GF_Err rewrite_pid_config(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
	GF_Err e = pctx->rewrite_pid_config(ctx, pctx);
	if (e) return e;
	pctx->prev_cprim = pctx->prev_ctfc = pctx->prev_cmx = pctx->prev_sar = -1;
	return GF_OK;
}

static void init_vui(GF_BSRWCtx *ctx, BSRWPid *pctx)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	pctx->vui.ar_num = ctx->sar.num;
	pctx->vui.ar_den = ctx->sar.den;
	pctx->vui.color_matrix = ctx->cmx;
	pctx->vui.color_prim = ctx->cprim;
	pctx->vui.fullrange = ctx->fullrange;
	pctx->vui.remove_video_info = ctx->novsi;
	pctx->vui.remove_vui_timing_info = ctx->novuitiming;
	pctx->vui.color_tfc = ctx->ctfc;
	pctx->vui.video_format = ctx->vidfmt;
#endif /*GPAC_DISABLE_AV_PARSERS*/

	pctx->rewrite_vui = GF_TRUE;
	if (ctx->sar.num>=0) return;
	if ((s32) ctx->sar.den>=0) return;
	if (ctx->cmx>-1) return;
	if (ctx->cprim>-1) return;
	if (ctx->fullrange) return;
	if (ctx->novsi) return;
	if (ctx->novuitiming) return;
	if (ctx->ctfc>-1) return;
	if (ctx->vidfmt>-1) return;
	//all default
	pctx->rewrite_vui = GF_FALSE;
}

static GF_Err prores_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	u8 *output;
	GF_FilterPacket *dst_pck = gf_filter_pck_new_clone(pctx->opid, pck, &output);
	if (!dst_pck) return GF_OUT_OF_MEM;
	
	//starting at offset 20 in frame:
	/*
	prores_frame->chroma_format = gf_bs_read_int(bs, 2);
	gf_bs_read_int(bs, 2);
	prores_frame->interlaced_mode = gf_bs_read_int(bs, 2);
	gf_bs_read_int(bs, 2);
	prores_frame->aspect_ratio_information = gf_bs_read_int(bs, 4);
	prores_frame->framerate_code = gf_bs_read_int(bs, 4);
	prores_frame->color_primaries = gf_bs_read_u8(bs);
	prores_frame->transfer_characteristics = gf_bs_read_u8(bs);
	prores_frame->matrix_coefficients = gf_bs_read_u8(bs);
	gf_bs_read_int(bs, 4);
	prores_frame->alpha_channel_type = gf_bs_read_int(bs, 4);
	*/

	if (ctx->sar.num && ctx->sar.den) {
		u32 framerate_code = output[21] & 0xF;
		u32 new_ar = 0;
		if (ctx->sar.num==ctx->sar.den) new_ar = 1;
		else if (ctx->sar.num * 3 == ctx->sar.den * 4) new_ar = 2;
		else if (ctx->sar.num * 9 == ctx->sar.den * 16) new_ar = 3;
		else {
			if (pctx->prev_sar != new_ar) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[BSRW] Aspect ratio %d/%d not registered in ProRes, using 0 (unknown)\n", ctx->sar.num, ctx->sar.den));
			}
		}
		new_ar <<= 4;
		framerate_code |= new_ar;
		output[21] = (u8) framerate_code;
		if (pctx->prev_sar != new_ar) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->sar) );
			pctx->prev_sar = new_ar;
		}
	}
	if (ctx->cprim>=0) {
		output[22] = (u8) ctx->cprim;
		if (pctx->prev_cprim != ctx->cprim) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_PRIMARIES, &PROP_UINT(ctx->cprim) );
			pctx->prev_cprim = ctx->cprim;
		}
	}
	if (ctx->ctfc>=0) {
		output[23] = (u8) ctx->ctfc;
		if (ctx->ctfc != pctx->prev_ctfc) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_TRANSFER, &PROP_UINT(ctx->ctfc) );
			pctx->prev_ctfc = ctx->ctfc;
		}
	}
	if (ctx->cmx>=0) {
		output[24] = (u8) ctx->cmx;
		if (pctx->prev_cmx != ctx->cmx) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_MX, &PROP_UINT(ctx->cmx) );
			pctx->prev_cmx = ctx->cmx;
		}
	}
	return gf_filter_pck_send(dst_pck);
}


static GF_Err bsrw_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *prop;
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);
	BSRWPid *pctx = gf_filter_pid_get_udta(pid);

	//disconnect of src pid (not yet supported)
	if (is_remove) {
		if (pctx) {
			if (pctx->opid) {
				gf_filter_pid_remove(pctx->opid);
				pctx->opid = NULL;
			}
			gf_filter_pid_set_udta(pid, NULL);
			gf_list_del_item(ctx->pids, pctx);
 			gf_free(pctx);
		}
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	if (!pctx) {
		GF_SAFEALLOC(pctx, BSRWPid);
		if (!pctx) return GF_OUT_OF_MEM;
		pctx->ipid = pid;
		gf_filter_pid_set_udta(pid, pctx);
		pctx->rewrite_pid_config = none_rewrite_pid_config;
		pctx->rewrite_packet = none_rewrite_packet;
		gf_list_add(ctx->pids, pctx);
		pctx->opid = gf_filter_pid_new(filter);
		if (!pctx->opid) return GF_OUT_OF_MEM;
		init_vui(ctx, pctx);
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	assert(prop);
	switch (prop->value.uint) {
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		pctx->rewrite_pid_config = avc_rewrite_pid_config;
		break;
#ifndef GPAC_DISABLE_HEVC
	case GF_CODECID_HEVC:
	case GF_CODECID_HEVC_TILES:
	case GF_CODECID_LHVC:
		pctx->rewrite_pid_config = hevc_rewrite_pid_config;
		break;
#endif
	case GF_CODECID_VVC:
	case GF_CODECID_VVC_SUBPIC:
		pctx->rewrite_pid_config = vvc_rewrite_pid_config;
		break;
	case GF_CODECID_MPEG4_PART2:
		pctx->rewrite_pid_config = m4v_rewrite_pid_config;
		break;
	case GF_CODECID_AP4H:
	case GF_CODECID_AP4X:
	case GF_CODECID_APCH:
	case GF_CODECID_APCN:
	case GF_CODECID_APCO:
	case GF_CODECID_APCS:
		pctx->rewrite_pid_config = none_rewrite_pid_config;
		pctx->rewrite_packet = prores_rewrite_packet;
		break;

	default:
		pctx->rewrite_pid_config = none_rewrite_pid_config;
		pctx->rewrite_packet = none_rewrite_packet;
		break;
	}

	gf_filter_pid_copy_properties(pctx->opid, pctx->ipid);
	pctx->codec_id = prop->value.uint;
	pctx->reconfigure = GF_FALSE;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	//rewrite asap - waiting for first packet could lead to issues further down the chain, especially for movie fragments
	//were the init segment could have already been flushed at the time we will dispatch the first packet
	rewrite_pid_config(ctx, pctx);
	return GF_OK;
}


static GF_Err bsrw_process(GF_Filter *filter)
{
	u32 i, count;
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);

	count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		BSRWPid *pctx;
		GF_FilterPacket *pck;
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		if (!pid) break;
		pctx = gf_filter_pid_get_udta(pid);
		if (!pctx) break;
		if (ctx->reconfigure)
			pctx->reconfigure = GF_TRUE;

		if (pctx->reconfigure) {
			init_vui(ctx, pctx);
			GF_Err e = rewrite_pid_config(ctx, pctx);
			if (e) return e;
		}
		pck = gf_filter_pid_get_packet(pid);
		if (!pck) {
			if (gf_filter_pid_is_eos(pctx->ipid))
				gf_filter_pid_set_eos(pctx->opid);
			continue;
		}
		pctx->rewrite_packet(ctx, pctx, pck);
		gf_filter_pid_drop_packet(pid);
	}
	ctx->reconfigure = GF_FALSE;
	return GF_OK;
}

static GF_Err bsrw_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val)
{
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);
	ctx->reconfigure = GF_TRUE;
	return GF_OK;
}

static GF_Err bsrw_initialize(GF_Filter *filter)
{
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);
	ctx->pids = gf_list_new();

#ifdef GPAC_ENABLE_COVERAGE
	bsrw_update_arg(filter, NULL, NULL);
#endif
	return GF_OK;
}
static void bsrw_finalize(GF_Filter *filter)
{
	GF_BSRWCtx *ctx = (GF_BSRWCtx *) gf_filter_get_udta(filter);
	while (gf_list_count(ctx->pids)) {
		BSRWPid *pctx = gf_list_pop_back(ctx->pids);
		gf_free(pctx);
	}
	gf_list_del(ctx->pids);
}

#define OFFS(_n)	#_n, offsetof(GF_BSRWCtx, _n)
static GF_FilterArgs BSRWArgs[] =
{
	///do not change order of the first 3
	{ OFFS(cprim), "color primaries according to ISO/IEC 23001-8 / 23091-2", GF_PROP_CICP_COL_PRIM, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(ctfc), "color transfer characteristics according to ISO/IEC 23001-8 / 23091-2", GF_PROP_CICP_COL_TFC, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(cmx), "color matrix coeficients according to ISO/IEC 23001-8 / 23091-2", GF_PROP_CICP_COL_MX, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(sar), "aspect ratio to rewrite", GF_PROP_FRACTION, "-1/-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(m4vpl), "set ProfileLevel for MPEG-4 video part two", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(fullrange), "video full range flag", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(novsi), "remove video_signal_type from VUI in AVC|H264 and HEVC", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(novuitiming), "remove timing_info from VUI in AVC|H264 and HEVC", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(prof), "profile indication for AVC|H264", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(lev), "level indication for AVC|H264, level_idc for VVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(pcomp), "profile compatibility for AVC|H264", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(pidc), "profile IDC for HEVC and VVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(pspace), "profile space for HEVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(gpcflags), "general compatibility flags for HEVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(rmsei), "remove SEI messages from bitstream for AVC|H264, HEVC and VVC", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(vidfmt), "video format for AVC|H264, HEVC and VVC", GF_PROP_UINT, "-1", "component|pal|ntsc|secam|mac|undef", GF_FS_ARG_UPDATE},
	{0}
};

static const GF_FilterCapability BSRWCaps[] =
{
	//this is correct but we want the filter to act as passthrough for other media
#if 0
	CAP_UINT(GF_CAPS_INPUT_STATIC ,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED|GF_CAPFLAG_STATIC , GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_SVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_MVC),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC_TILES),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_LHVC),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_MPEG4_PART2),
	{0},
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AP4H),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_AP4X),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_APCH),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_APCN),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_APCO),
	CAP_UINT(GF_CAPS_INPUT_OUTPUT,GF_PROP_PID_CODECID, GF_CODECID_APCS),
#else
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_OUTPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
#endif

};

GF_FilterRegister BSRWRegister = {
	.name = "bsrw",
	GF_FS_SET_DESCRIPTION("Compressed bitstream rewriter")
	GF_FS_SET_HELP("This filter rewrites some metadata of various bitstream formats.\n"
	"The filter can currently modify the following properties in video bitstreams:\n"
	"- MPEG-4 Visual:\n"
	"  - sample aspect ratio\n"
	"  - profile and level\n"
	"- AVC|H264, HEVC and VVC:\n"
	"  - sample aspect ratio\n"
	"  - profile, level, profile compatibility\n"
	"  - video format, video fullrange\n"
	"  - color primaries, transfer characteristics and matrix coefficients (or remove all info)\n"
	"- ProRes:\n"
	"  - sample aspect ratio\n"
	"  - color primaries, transfer characteristics and matrix coefficients\n"
	"  \n"
	"Values are by default initialized to -1, implying to keep the related info (present or not) in the bitstream.\n"
	"A [-sar]() value of `0/0` will remove sample aspect ratio info from bitstream if possible.\n"
	"  \n"
	"The filter can currently modify the following properties in the stream configuration but not in the bitstream:\n"
	"- HEVC: profile IDC, profile space, general compatibility flags\n"
	"- VVC: profile IDC, general profile and level indication\n"
	"  \n"
	"The filter will work in passthrough mode for all other codecs and media types.\n"
	)
	.private_size = sizeof(GF_BSRWCtx),
	.max_extra_pids = 0xFFFFFFFF,
	.flags = GF_FS_REG_EXPLICIT_ONLY|GF_FS_REG_ALLOW_CYCLIC,
	.args = BSRWArgs,
	SETCAPS(BSRWCaps),
	.initialize = bsrw_initialize,
	.finalize = bsrw_finalize,
	.configure_pid = bsrw_configure_pid,
	.process = bsrw_process,
	.update_arg = bsrw_update_arg
};

const GF_FilterRegister *bsrw_register(GF_FilterSession *session)
{
	//assign runtime caps on first load
	if (gf_opts_get_bool("temp", "helponly")) {
		BSRWArgs[0].min_max_enum = gf_cicp_color_primaries_all_names();
		BSRWArgs[1].min_max_enum = gf_cicp_color_transfer_all_names();
		BSRWArgs[2].min_max_enum = gf_cicp_color_matrix_all_names();
	}
	return (const GF_FilterRegister *) &BSRWRegister;
}
