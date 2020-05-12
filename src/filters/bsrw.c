/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020
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
	s32 clrp, txchar, mxcoef;
	GF_Err (*rewrite_pid_config)(GF_BSRWCtx *ctx, BSRWPid *pctx);
	GF_Err (*rewrite_packet)(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck);

	s32 prev_clrp, prev_txchar, prev_mxcoef, prev_sar;

	u32 nalu_size_length;
};

struct _bsrw_ctx
{
	GF_Fraction sar;
	s32 m4vpl, prof, lev, pcomp, pidc, pspace, gpcflags;
	char *clrp, *txchar, *mxcoef;
	Bool remsei;


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

	if (ctx->sar.num && ctx->sar.num) {
		e = gf_m4v_rewrite_par(&dsi, &dsi_size, ctx->sar.num, ctx->sar.den);
		if (e) {
			gf_free(dsi);
			return e;
		}
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->sar) );
	}
	if (ctx->m4vpl>=0) {
		gf_m4v_rewrite_pl(&dsi, &dsi_size, (u32) ctx->m4vpl);
		gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_PROFILE_LEVEL, &PROP_UINT(ctx->m4vpl) );
	}

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	return GF_OK;
}

static GF_Err avc_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
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
		nal_type = data[size];
		nal_type = nal_type & 0x1F;
		if (nal_type != GF_AVC_NALU_SEI) {
			final_size += nal_size+pctx->nalu_size_length;
		}
		size += nal_size;
	}
	if (final_size == pck_size)
		return gf_filter_pck_forward(pck, pctx->opid);

	dst = gf_filter_pck_new_alloc(pctx->opid, final_size, &output);
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
		nal_type = data[size];
		nal_type = nal_type & 0x1F;
		if (nal_type == GF_AVC_NALU_SEI) {
			size += nal_size;
			continue;
		}
		memcpy(output, &data[size-pctx->nalu_size_length], pctx->nalu_size_length+nal_size);
		output += pctx->nalu_size_length+nal_size;
		size += nal_size;
	}
	return gf_filter_pck_send(dst);
}

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
	if (ctx->sar.num && ctx->sar.den) {
		e = gf_media_avc_change_par(avcc, ctx->sar.num, ctx->sar.den);
		if (!e)
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->sar) );
	}

	if ((ctx->lev>=0) || (ctx->prof>=0) || (ctx->pcomp>=0)) {
		u32 i, count = gf_list_count(avcc->sequenceParameterSets);
		for (i=0; i<count; i++) {
			GF_AVCConfigSlot *sps = gf_list_get(avcc->sequenceParameterSets, i);
			//first byte is nalu header, then profile_idc (8bits), prof_comp (8buts) a,d level_idc (8bits)
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

	if (ctx->remsei) {
		pctx->rewrite_packet = avc_rewrite_packet;
	} else {
		pctx->rewrite_packet = none_rewrite_packet;
	}
	return GF_OK;
}

static GF_Err hevc_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
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
		nal_type = data[size];
		nal_type = (nal_type & 0x7E) >> 1;
		if ((nal_type != GF_HEVC_NALU_SEI_PREFIX) && (nal_type != GF_HEVC_NALU_SEI_SUFFIX) ) {
			final_size += nal_size+pctx->nalu_size_length;
		}
		size += nal_size;
	}
	if (final_size == pck_size)
		return gf_filter_pck_forward(pck, pctx->opid);

	dst = gf_filter_pck_new_alloc(pctx->opid, final_size, &output);
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
		nal_type = data[size];
		nal_type = (nal_type & 0x7E) >> 1;
		if ((nal_type == GF_HEVC_NALU_SEI_PREFIX) || (nal_type == GF_HEVC_NALU_SEI_SUFFIX) ) {
			size += nal_size;
			continue;
		}
		memcpy(output, &data[size-pctx->nalu_size_length], pctx->nalu_size_length+nal_size);
		output += pctx->nalu_size_length+nal_size;
		size += nal_size;
	}
	return gf_filter_pck_send(dst);
}

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
	if (ctx->sar.num && ctx->sar.den) {
		e = gf_media_hevc_change_par(hvcc, ctx->sar.num, ctx->sar.den);
		if (!e)
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_SAR, &PROP_FRAC(ctx->sar) );
	}
	if (ctx->pidc>=0) hvcc->profile_idc = ctx->pidc;
	if (ctx->pspace>=0) hvcc->profile_space = ctx->pspace;
	if (ctx->gpcflags>=0) hvcc->general_profile_compatibility_flags = ctx->gpcflags;


	gf_odf_hevc_cfg_write(hvcc, &dsi, &dsi_size);
	pctx->nalu_size_length = hvcc->nal_unit_size;
	gf_odf_hevc_cfg_del(hvcc);
	if (e) return e;

	gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(dsi, dsi_size) );

	if (ctx->remsei) {
		pctx->rewrite_packet = hevc_rewrite_packet;
	} else {
		pctx->rewrite_packet = none_rewrite_packet;
	}
	return GF_OK;
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
	pctx->clrp = pctx->txchar = pctx->mxcoef = -1;
	pctx->prev_clrp = pctx->prev_txchar = pctx->prev_mxcoef = pctx->prev_sar = -1;

	if (ctx->clrp) {
		if (!stricmp(ctx->clrp, "BT.709") || !stricmp(ctx->clrp, "BT709")) pctx->clrp = 1;
		else if (!stricmp(ctx->clrp, "BT.601-625") || !stricmp(ctx->clrp, "BT601-625")) pctx->clrp = 5;
		else if (!stricmp(ctx->clrp, "BT.601-525") || !stricmp(ctx->clrp, "BT601-525")) pctx->clrp = 6;
		else if (!stricmp(ctx->clrp, "BT.2020") || !stricmp(ctx->clrp, "BT2020")) pctx->clrp = 9;
		else if (!stricmp(ctx->clrp, "P3")) pctx->clrp = 11;
		else if (!stricmp(ctx->clrp, "P3-D65")) pctx->clrp = 12;
		else {
			char szCoef[100];
			pctx->clrp = atoi(ctx->clrp);
			sprintf(szCoef, "%u", pctx->clrp);
			if (stricmp(szCoef, ctx->clrp))
				pctx->clrp = -1;
		}
	}
	if (ctx->txchar) {
		if (!strcmp(ctx->txchar, "BT-709") || !strcmp(ctx->txchar, "BT709")) pctx->txchar=1;
		else if (!strcmp(ctx->txchar, "ST-2084") || !strcmp(ctx->txchar, "ST2084")) pctx->txchar=16;
		else if (!strcmp(ctx->txchar, "STD-B67") || !strcmp(ctx->txchar, "STDB67")) pctx->txchar=18;
		else {
			char szTxChar[100];
			pctx->txchar = atoi(ctx->txchar);
			sprintf(szTxChar, "%u", pctx->txchar);
			if (stricmp(szTxChar, ctx->txchar))
				pctx->txchar = -1;
		}
	}
	if (ctx->mxcoef) {
		if (!strcmp(ctx->mxcoef, "BT.709") || !strcmp(ctx->mxcoef, "BT709")) pctx->mxcoef = 1;
		else if (!strcmp(ctx->mxcoef, "BT.601") || !strcmp(ctx->mxcoef, "BT601")) pctx->mxcoef = 6;
		else if (!strcmp(ctx->mxcoef, "BT.2020") || !strcmp(ctx->mxcoef, "BT2020")) pctx->mxcoef = 9;
		else {
			char szMx[100];
			pctx->mxcoef = atoi(ctx->mxcoef);
			sprintf(szMx, "%u", pctx->mxcoef);
			if (stricmp(szMx, ctx->mxcoef))
				pctx->mxcoef = -1;
		}
	}
	return GF_OK;
}


static GF_Err prores_rewrite_packet(GF_BSRWCtx *ctx, BSRWPid *pctx, GF_FilterPacket *pck)
{
	u8 *output;
	GF_FilterPacket *dst_pck = gf_filter_pck_new_clone(pctx->opid, pck, &output);

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
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[BSRW] Aspect ratio %d/%d not registered in ProRes, using 0 (unknown)\n", ctx->sar.num, ctx->sar.den));
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
	if (pctx->clrp>=0) {
		output[22] = (u8) pctx->clrp;
		if (pctx->prev_clrp != pctx->clrp) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_PRIMARIES, &PROP_UINT(pctx->clrp) );
			pctx->prev_clrp = pctx->clrp;
		}
	}
	if (pctx->txchar>=0) {
		output[23] = (u8) pctx->txchar;
		if (pctx->prev_txchar != pctx->txchar) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_TRANSFER, &PROP_UINT(pctx->txchar) );
			pctx->prev_txchar = pctx->txchar;
		}
	}
	if (pctx->mxcoef>=0) {
		output[24] = (u8) pctx->mxcoef;
		if (pctx->prev_mxcoef != pctx->mxcoef) {
			gf_filter_pid_set_property(pctx->opid, GF_PROP_PID_COLR_MX, &PROP_UINT(pctx->mxcoef) );
			pctx->prev_mxcoef = pctx->mxcoef;
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
			//one in one out, this is simple
			gf_filter_pid_remove(pctx->opid);
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
		pctx->ipid = pid;
		pctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_udta(pid, pctx);
		pctx->rewrite_pid_config = none_rewrite_pid_config;
		pctx->rewrite_packet = none_rewrite_packet;
		gf_list_add(ctx->pids, pctx);
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	assert(prop);
	switch (prop->value.uint) {
	case GF_CODECID_AVC:
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		pctx->rewrite_pid_config = avc_rewrite_pid_config;
		break;
	case GF_CODECID_HEVC:
	case GF_CODECID_HEVC_TILES:
	case GF_CODECID_LHVC:
		pctx->rewrite_pid_config = hevc_rewrite_pid_config;
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
	pctx->reconfigure = GF_TRUE;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
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
static const GF_FilterArgs BSRWArgs[] =
{
	{ OFFS(sar), "aspect ratio to rewrite", GF_PROP_FRACTION, "0/0", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(m4vpl), "set ProfileLevel for MPEG-4 video part two", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(clrp), "color primaries according to ISO/IEC 23001-8 / 23091-2. Value can be the integer value or (case insensitive) `BT709`, `BT601-625`, `BT601-525`, `BT2020`, `P3` or `P3-D65`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_UPDATE},
	{ OFFS(txchar), "transfer characteristics according to ISO/IEC 23001-8 / 23091-2. Value can be the integer value or (case insensitive) `BT709`, `ST2084` or `STDB67`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_UPDATE},
	{ OFFS(mxcoef), "matrix coeficients according to ISO/IEC 23001-8 / 23091-2. Value can be the integer value or (case insensitive) `BT709`, `BT601` or `BT2020`", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_UPDATE},

	{ OFFS(prof), "profile indication for AVC|H264", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(lev), "level indication for AVC|H264", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(pcomp), "profile compatibility for AVC|H264", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(pidc), "profile IDC for HEVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(pspace), "profile space for HEVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(gpcflags), "general compatibility flags for HEVC", GF_PROP_SINT, "-1", NULL, GF_FS_ARG_UPDATE},

	{ OFFS(remsei), "remove SEI messages from bitstream", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},

	{0}
};

static const GF_FilterCapability BSRWCaps[] =
{
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
};

GF_FilterRegister BSRWRegister = {
	.name = "bsrw",
	GF_FS_SET_DESCRIPTION("Compressed bitstream rewriter")
	GF_FS_SET_HELP("This filter rewrites some metadata of various bitstream formats.\n"
	"The filter can currently modify the following properties in the bitstream:\n"
	"- MPEG-4 Visual: aspect ratio and profile/level\n"
	"- AVC|H264: aspect ratio, profile, level, profile compatibility\n"
	"- HEVC: aspect ratio\n"
	"- ProRes: aspect ratio, color primaries, transfer characteristics and matrix coefficients\n"
	"  \n"
	"The filter can currently modify the following properties in the stream configuration but not in the bitstream:\n"
	"- HEVC: profile IDC, profile space, general compatibility flags\n"
	)
	.private_size = sizeof(GF_BSRWCtx),
	.max_extra_pids = 0xFFFFFFFF,
	.flags = GF_FS_REG_EXPLICIT_ONLY,
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
	return &BSRWRegister;
}
