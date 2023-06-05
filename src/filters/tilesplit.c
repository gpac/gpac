/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / tile splitting filter
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
#include <gpac/internal/media_dev.h>

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_TILESPLIT)

typedef struct
{
	GF_FilterPid *opid;
	u32 x, y, w, h;
	GF_BitStream *pck_bs;
	u8 *pck_buf;
	u32 pck_buf_alloc;
	Bool all_intra;
} TileSplitPid;


typedef struct
{
	//options
	GF_PropUIntList tiledrop;

	//internal
	GF_FilterPid *ipid;
	GF_FilterPid *base_opid;
	u32 base_id;

	u32 nb_tiles, nb_alloc_tiles;
	TileSplitPid *opids;

	HEVCState hevc;
	u32 nalu_size_length;

	Bool filter_disabled, passthrough;
	s32 cur_pps_idx;

	u32 width, height;

	GF_BitStream *pck_bs;
	u8 *pck_buf;
	u32 pck_buf_alloc;
} GF_TileSplitCtx;

static void tilesplit_update_pid_props(GF_TileSplitCtx *ctx, TileSplitPid *tinfo)
{
	gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_WIDTH, &PROP_UINT(tinfo->w) );
	gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(tinfo->h) );

#if 0
	GF_PropertyValue pval;
	pval.type = GF_PROP_VEC2I;
	pval.value.vec2i.x = ctx->width;
	pval.value.vec2i.y = ctx->height;
	gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_SRD_REF, &pval);

	pval.type = GF_PROP_VEC4I;
	pval.value.vec4i.x = tinfo->x;
	pval.value.vec4i.y = tinfo->y;
	pval.value.vec4i.z = tinfo->w;
	pval.value.vec4i.w = tinfo->h;
	gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_SRD, &pval);
#else

	gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(tinfo->x, tinfo->y) );

#endif
}

static GF_Err tilesplit_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_PropertyValue pval;
	const GF_PropertyValue *p;
	GF_HEVCConfig *hvcc;
	GF_HEVCConfig tile_cfg;
	HEVC_PPS *pps;
	HEVC_SPS *sps;
	u8 *dsi;
	u32 dsi_size, bitrate;
	u32 i, j, count, nb_tiles, PicWidthInCtbsY, PicHeightInCtbsY, tile_y, active_tiles;
	s32 pps_idx=-1, sps_idx=-1;
	GF_TileSplitCtx *ctx = (GF_TileSplitCtx *) gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->ipid == pid) {
			for (i=0; i<ctx->nb_tiles; i++) {
				if (ctx->opids[i].opid) {
					gf_filter_pid_remove(ctx->opids[i].opid);
					ctx->opids[i].opid = NULL;
				}
			}
			ctx->nb_tiles = 0;
			ctx->ipid = NULL;
		}
		return GF_OK;
	}

	if (!ctx->ipid) {
		ctx->ipid = pid;
		ctx->base_opid = gf_filter_pid_new(filter);
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}
	gf_filter_pid_copy_properties(ctx->base_opid, pid);
	//set SABT to true by default for link resolution
	gf_filter_pid_set_property(ctx->base_opid, GF_PROP_PID_TILE_BASE, &PROP_BOOL(GF_TRUE) );

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!p) return GF_OK;

	hvcc = gf_odf_hevc_cfg_read(p->value.data.ptr, p->value.data.size, GF_FALSE);
	if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;

	ctx->nalu_size_length = hvcc->nal_unit_size;
	memset(&ctx->hevc, 0, sizeof(HEVCState));

	count = gf_list_count(hvcc->param_array);
	for (i=0; i<count; i++) {
		GF_NALUFFParamArray *ar = gf_list_get(hvcc->param_array, i);
		for (j=0; j < gf_list_count(ar->nalus); j++) {
			GF_NALUFFParam *sl = gf_list_get(ar->nalus, j);
			if (!sl) continue;
			switch (ar->type) {
			case GF_HEVC_NALU_PIC_PARAM:
				pps_idx = gf_hevc_read_pps(sl->data, sl->size, &ctx->hevc);
				break;
			case GF_HEVC_NALU_SEQ_PARAM:
				sps_idx = gf_hevc_read_sps(sl->data, sl->size, &ctx->hevc);
				break;
			case GF_HEVC_NALU_VID_PARAM:
				gf_hevc_read_vps(sl->data, sl->size, &ctx->hevc);
				break;
			}
		}
	}
	if (pps_idx==-1) return GF_NON_COMPLIANT_BITSTREAM;
	if (sps_idx==-1) return GF_NON_COMPLIANT_BITSTREAM;

	if (ctx->hevc.pps[pps_idx].loop_filter_across_tiles_enabled_flag)
		ctx->filter_disabled = GF_FALSE;
	else
		ctx->filter_disabled = GF_TRUE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (p) ctx->base_id = p->value.uint;

	if (! ctx->hevc.pps[pps_idx].tiles_enabled_flag) {
		gf_odf_hevc_cfg_del(hvcc);
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[TileSplit] Tiles not enabled, using passthrough\n"));
		gf_filter_pid_set_property(ctx->base_opid, GF_PROP_PID_TILE_BASE, NULL);
		ctx->passthrough = GF_TRUE;
		return GF_OK;
	}
	ctx->passthrough = GF_FALSE;
	nb_tiles = ctx->hevc.pps[pps_idx].num_tile_columns * ctx->hevc.pps[pps_idx].num_tile_rows;

	while (nb_tiles < ctx->nb_tiles) {
		ctx->nb_tiles--;
		if (ctx->opids[ctx->nb_tiles].opid) {
			gf_filter_pid_remove(ctx->opids[ctx->nb_tiles].opid);
			ctx->opids[ctx->nb_tiles].opid = NULL;
		}
	}

	if (nb_tiles>ctx->nb_alloc_tiles) {
		ctx->opids = gf_realloc(ctx->opids, sizeof(TileSplitPid) * nb_tiles);
		memset(&ctx->opids[ctx->nb_alloc_tiles], 0, sizeof(TileSplitPid) * (nb_tiles-ctx->nb_alloc_tiles) );
		ctx->nb_alloc_tiles = nb_tiles;
	}

	/*setup tile info*/
	ctx->cur_pps_idx = pps_idx;
	pps = &ctx->hevc.pps[pps_idx];
	sps = &ctx->hevc.sps[sps_idx];

	ctx->width = sps->width;
	ctx->height = sps->height;

	PicWidthInCtbsY = sps->width / sps->max_CU_width;
	if (PicWidthInCtbsY * sps->max_CU_width < sps->width) PicWidthInCtbsY++;
	PicHeightInCtbsY = sps->height / sps->max_CU_width;
	if (PicHeightInCtbsY * sps->max_CU_width < sps->height) PicHeightInCtbsY++;

	//setup grid info before sending data
	//we fill the grid in order of the tiling
	tile_y = 0;
	for (i=0; i<pps->num_tile_rows; i++) {
		u32 tile_x = 0;
		u32 tile_height;
		if (pps->uniform_spacing_flag) {
			tile_height = (i+1)*PicHeightInCtbsY / pps->num_tile_rows - i * PicHeightInCtbsY / pps->num_tile_rows;
		} else {
			if (i < pps->num_tile_rows-1) {
				tile_height = pps->row_height[i];
			} else if (i) {
				tile_height = (PicHeightInCtbsY - pps->row_height[i-1]);
			} else {
				tile_height = PicHeightInCtbsY;
			}
		}

		for (j=0; j < pps->num_tile_columns; j++) {
			TileSplitPid *tinfo;
			u32 tile_width;

			if (pps->uniform_spacing_flag) {
				tile_width = (j+1) * PicWidthInCtbsY / pps->num_tile_columns - j * PicWidthInCtbsY / pps->num_tile_columns;
			} else {
				if (j<pps->num_tile_columns-1) {
					tile_width = pps->column_width[j];
				} else {
					tile_width = (PicWidthInCtbsY - pps->column_width[j-1]);
				}
			}

			tinfo = &ctx->opids[i * pps->num_tile_columns + j];
			tinfo->x = tile_x * sps->max_CU_width;
			tinfo->w = tile_width * sps->max_CU_width;
			tinfo->y = tile_y * sps->max_CU_width;
			tinfo->h = tile_height * sps->max_CU_width;
			tinfo->all_intra = GF_TRUE;

			if (tinfo->x + tinfo->w > sps->width)
				tinfo->w = sps->width - tinfo->x;
			if (tinfo->y + tinfo->h > sps->height)
				tinfo->h = sps->height - tinfo->y;

			tile_x += tile_width;
		}
		tile_y += tile_height;
	}

	pval.type = GF_PROP_VEC2I;
	pval.value.vec2i.x = ctx->width;
	pval.value.vec2i.y = ctx->height;

	memcpy(&tile_cfg, hvcc, sizeof(GF_HEVCConfig));
	tile_cfg.param_array = NULL;
	gf_odf_hevc_cfg_write(&tile_cfg, &dsi, &dsi_size);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_BITRATE);
	bitrate = 0;
	if (p && (p->value.uint>10000)) {
		bitrate = p->value.uint - 10000;
		bitrate /= nb_tiles-ctx->tiledrop.nb_items;
	}

	const char *pname = gf_filter_pid_get_name(ctx->ipid);
	if (pname) pname = gf_file_basename(pname);
	if (!pname) pname = "video";


	for (i=ctx->nb_tiles; i<nb_tiles; i++) {
		char szName[GF_MAX_PATH];
		TileSplitPid *tinfo = &ctx->opids[i];
		if (!tinfo->opid) {
			Bool drop = GF_FALSE;
			for (j=0; j<ctx->tiledrop.nb_items; j++) {
				if (ctx->tiledrop.vals[j]==i) {
					drop = GF_TRUE;
					break;
				}

			}
			if (!drop)
				tinfo->opid = gf_filter_pid_new(filter);
		}

		if (!tinfo->opid) continue;

		gf_filter_pid_copy_properties(tinfo->opid, pid);
		gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_HEVC_TILES) );
		if (dsi)
			gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA(dsi, dsi_size) );
		else
			gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_DECODER_CONFIG, NULL);

		gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_BITRATE, bitrate ? &PROP_UINT(bitrate) : NULL);

		gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_ID, &PROP_UINT(ctx->base_id + i + 1 ) );
		gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_ESID, NULL);
		gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_TILE_ID, &PROP_UINT(i + 1 ) );
		gf_filter_pid_set_property(tinfo->opid, GF_PROP_PID_DEPENDENCY_ID, &PROP_UINT(ctx->base_id) );
		tilesplit_update_pid_props(ctx, tinfo);

		sprintf(szName, "%s_tile%d", pname, i+1);
		gf_filter_pid_set_name(tinfo->opid, szName);
	}
	if (dsi) gf_free(dsi);
	ctx->nb_tiles = nb_tiles;
	active_tiles = 0;
	//setup tbas track ref
	for (i=0; i<nb_tiles; i++) {
		if (!ctx->opids[i].opid)
			continue;
		pval.type = GF_PROP_4CC_LIST;
		pval.value.uint_list.nb_items = 1;
		pval.value.uint_list.vals = &ctx->base_id;
		active_tiles ++;
		gf_filter_pid_set_property_str(ctx->opids[i].opid, "isom:tbas", &pval);
	}
	//setup sabt track ref
	pval.type = GF_PROP_4CC_LIST;
	pval.value.uint_list.nb_items = active_tiles;
	pval.value.uint_list.vals = gf_malloc(sizeof(u32) * active_tiles);
	active_tiles = 0;
	for (i=0; i<nb_tiles; i++) {
		if (!ctx->opids[i].opid)
			continue;
		pval.value.uint_list.vals[active_tiles] = ctx->base_id + i + 1;
		active_tiles++;
	}
	gf_filter_pid_set_property_str(ctx->base_opid, "isom:sabt", &pval);
	gf_free(pval.value.uint_list.vals);

	gf_filter_pid_set_property(ctx->base_opid, GF_PROP_PID_ORIG_SIZE, &PROP_VEC2I_INT(ctx->width, ctx->height) );
	gf_filter_pid_set_property(ctx->base_opid, GF_PROP_PID_BITRATE, bitrate ? &PROP_UINT(10000) : NULL);

	gf_odf_hevc_cfg_del(hvcc);
	return GF_OK;
}

static GF_Err tilesplit_set_eos(GF_Filter *filter, GF_TileSplitCtx *ctx)
{
	u32 i;
	for (i=0; i<ctx->nb_tiles; i++) {
		if (ctx->opids[i].opid)
			gf_filter_pid_set_eos(ctx->opids[i].opid);
	}
	gf_filter_pid_set_eos(ctx->base_opid);
	return GF_EOS;
}

#if !defined(GPAC_DISABLE_AV_PARSERS)
u32 hevc_get_tile_id(HEVCState *hevc, u32 *tile_x, u32 *tile_y, u32 *tile_width, u32 *tile_height);
#endif
static GF_Err tilesplit_process(GF_Filter *filter)
{
	GF_TileSplitCtx *ctx = (GF_TileSplitCtx *) gf_filter_get_udta(filter);
	GF_FilterPacket *in_pck, *opck;
	u32 pck_size, i;
	GF_Err e = GF_OK;
	const u8 *in_data;
	u8 *output;

	in_pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!in_pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			return tilesplit_set_eos(filter, ctx);
		}
		return GF_OK;
	}

	if (ctx->passthrough) {
		gf_filter_pck_forward(in_pck, ctx->base_opid);
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	in_data = gf_filter_pck_get_data(in_pck, &pck_size);
	if (!in_data) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}

	if (!ctx->pck_bs) ctx->pck_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->pck_bs, ctx->pck_buf, ctx->pck_buf_alloc);

	for (i=0; i<ctx->nb_tiles; i++) {
		TileSplitPid *tinfo = &ctx->opids[i];
		if (!tinfo->opid)
			continue;
		if (!tinfo->pck_bs) tinfo->pck_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		else gf_bs_reassign_buffer(tinfo->pck_bs, tinfo->pck_buf, tinfo->pck_buf_alloc);
	}

	while (pck_size) {
		TileSplitPid *tinfo;
		u8 temporal_id, layer_id;
		u8 nal_type = 0;
		u32 nalu_size = 0;
		s32 ret;
		u32 cur_tile;
		u32 tx, ty, tw, th;
		for (i=0; i<ctx->nalu_size_length; i++) {
			nalu_size = (nalu_size<<8) + in_data[i];
		}

		if (pck_size < nalu_size + ctx->nalu_size_length) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[TileSplit] Corrupted NAL size, %d indicated but %d remaining\n", nalu_size + ctx->nalu_size_length, pck_size));
			break;
		}

		ret = gf_hevc_parse_nalu((u8 *) in_data + ctx->nalu_size_length, nalu_size, &ctx->hevc, &nal_type, &temporal_id, &layer_id);

		//error parsing NAL, set nal to fallback to regular import
		if (ret<0) nal_type = GF_HEVC_NALU_VID_PARAM;

		switch (nal_type) {
		case GF_HEVC_NALU_SLICE_TRAIL_N:
		case GF_HEVC_NALU_SLICE_TRAIL_R:
		case GF_HEVC_NALU_SLICE_TSA_N:
		case GF_HEVC_NALU_SLICE_TSA_R:
		case GF_HEVC_NALU_SLICE_STSA_N:
		case GF_HEVC_NALU_SLICE_STSA_R:
		case GF_HEVC_NALU_SLICE_BLA_W_LP:
		case GF_HEVC_NALU_SLICE_BLA_W_DLP:
		case GF_HEVC_NALU_SLICE_BLA_N_LP:
		case GF_HEVC_NALU_SLICE_IDR_W_DLP:
		case GF_HEVC_NALU_SLICE_IDR_N_LP:
		case GF_HEVC_NALU_SLICE_CRA:
		case GF_HEVC_NALU_SLICE_RADL_R:
		case GF_HEVC_NALU_SLICE_RADL_N:
		case GF_HEVC_NALU_SLICE_RASL_R:
		case GF_HEVC_NALU_SLICE_RASL_N:
			tx = ty = tw = th = 0;
			cur_tile = hevc_get_tile_id(&ctx->hevc, &tx, &ty, &tw, &th);
			if (cur_tile >= ctx->nb_tiles) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[TileSplit] Tile index %d is greater than number of tiles %d in PPS\n", cur_tile, ctx->nb_tiles));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto err_exit;
			}
			tinfo = &ctx->opids[cur_tile];
			if (!tinfo->opid)
				break;

			if ((tinfo->x != tx) || (tinfo->w != tw) || (tinfo->y != ty) || (tinfo->h != th)) {
				tinfo->x = tx;
				tinfo->y = ty;
				tinfo->w = tw;
				tinfo->h = th;

				tilesplit_update_pid_props(ctx, tinfo);
			}

			if (ctx->hevc.s_info.slice_type != GF_HEVC_SLICE_TYPE_I) {
				tinfo->all_intra = GF_FALSE;
			}

			gf_bs_write_data(tinfo->pck_bs, (u8 *) in_data, nalu_size + ctx->nalu_size_length);
			break;
		//note we don't need to care about sps or pps here, they are not in the bitstream
		default:
			gf_bs_write_data(ctx->pck_bs, (u8 *) in_data, nalu_size + ctx->nalu_size_length);
			break;
		}

		in_data += nalu_size + ctx->nalu_size_length;
		pck_size -= nalu_size + ctx->nalu_size_length;
	}

err_exit:
	//and flush

	gf_bs_get_content_no_truncate(ctx->pck_bs, &ctx->pck_buf, &pck_size, &ctx->pck_buf_alloc);
	opck = gf_filter_pck_new_alloc(ctx->base_opid, pck_size, &output);
	if (opck) {
		memcpy(output, ctx->pck_buf, pck_size);
		gf_filter_pck_merge_properties(in_pck, opck);
		gf_filter_pck_send(opck);
	} else {
		e = GF_OUT_OF_MEM;
	}

	for (i=0; i<ctx->nb_tiles; i++) {
		TileSplitPid *tinfo = &ctx->opids[i];
		if (!tinfo->opid) continue;

		gf_bs_get_content_no_truncate(tinfo->pck_bs, &tinfo->pck_buf, &pck_size, &tinfo->pck_buf_alloc);
		opck = gf_filter_pck_new_alloc(tinfo->opid, pck_size, &output);
		if (opck) {
			memcpy(output, tinfo->pck_buf, pck_size);
			gf_filter_pck_merge_properties(in_pck, opck);
			gf_filter_pck_send(opck);
		} else {
			e = GF_OUT_OF_MEM;
		}
	}

	gf_filter_pid_drop_packet(ctx->ipid);
	return e;
}

static GF_Err tilesplit_initialize(GF_Filter *filter)
{
//	GF_TileSplitCtx *ctx = (GF_TileSplitCtx *) gf_filter_get_udta(filter);

	return GF_OK;
}

static void tilesplit_finalize(GF_Filter *filter)
{
	u32 i;
	GF_TileSplitCtx *ctx = (GF_TileSplitCtx *) gf_filter_get_udta(filter);

	for (i=0; i<ctx->nb_alloc_tiles; i++) {
		TileSplitPid *tinfo = &ctx->opids[i];
		if (tinfo->pck_buf) gf_free(tinfo->pck_buf);
		if (tinfo->pck_bs) gf_bs_del(tinfo->pck_bs);
	}
	if (ctx->opids)
		gf_free(ctx->opids);

	if (ctx->pck_bs) gf_bs_del(ctx->pck_bs);
	if (ctx->pck_buf) gf_free(ctx->pck_buf);
}


static const GF_FilterCapability TileSplitCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_TILE_BASE, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_OUTPUT, GF_PROP_PID_TILE_BASE, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_TILE_BASE, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC_TILES)
};

#define OFFS(_n)	#_n, offsetof(GF_TileSplitCtx, _n)

static const GF_FilterArgs TileSplitArgs[] =
{
	{ OFFS(tiledrop), "specify indexes of tiles to drop (0-based, in tile raster scan order)", GF_PROP_UINT_LIST, NULL, NULL, GF_FS_ARG_UPDATE},
	{0}
};

GF_FilterRegister TileSplitRegister = {
	.name = "tilesplit",
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	GF_FS_SET_DESCRIPTION("HEVC tile bitstream splitter")
	GF_FS_SET_HELP("This filter splits an HEVC tiled stream into tiled HEVC streams (`hvt1` or `hvt2` in ISOBMFF)."
	"\n"
	"The filter will move to passthrough mode if the bitstream is not tiled.\n"
	"If the `Bitrate` property is set on the input PID, the output tile PIDs will have a bitrate set to `(Bitrate - 10k)/nb_opids`, 10 kbps being reserved for the base.\n"
	"\n"
	"Each tile PID will be assigned the following properties:\n"
	"- `ID`: equal to the base PID ID (same as input) plus the 1-based index of the tile in raster scan order.\n"
	"- `TileID`: equal to the 1-based index of the tile in raster scan order.\n"
	"\n"
	"Warning: The filter does not check if tiles are independently-coded (MCTS) !\n"
	"\n"
	"Warning: Support for dynamic changes of tiling grid has not been tested !\n"
	)
	.private_size = sizeof(GF_TileSplitCtx),
	SETCAPS(TileSplitCaps),
#if !defined(GPAC_DISABLE_AV_PARSERS)
	.initialize = tilesplit_initialize,
	.finalize = tilesplit_finalize,
	.args = TileSplitArgs,
	.configure_pid = tilesplit_configure_pid,
#endif
	.process = tilesplit_process,
};


const GF_FilterRegister *tilesplit_register(GF_FilterSession *session)
{
	return &TileSplitRegister;
}
#else
const GF_FilterRegister *tilesplit_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // #if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_TILESPLIT)


