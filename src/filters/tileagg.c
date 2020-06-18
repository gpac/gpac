/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2019
 *					All rights reserved
 *
 *  This file is part of GPAC / tile aggregrator filter
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
#include <gpac/media_tools.h>


typedef struct
{
	//options
	GF_PropUIntList tiledrop;

	//internal
	GF_FilterPid *opid;
	GF_FilterPid *base_ipid;
	u32 nalu_size_length;
	u32 base_id;

	GF_BitStream *bs_r;
} GF_TileAggCtx;


static GF_Err tileagg_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 codec_id=0;
	const GF_PropertyValue *p;
	GF_HEVCConfig *hvcc;
	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->base_ipid == pid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) return GF_NOT_SUPPORTED;
	codec_id = p->value.uint;


	if ((codec_id==GF_CODECID_HEVC) && ctx->base_ipid && (ctx->base_ipid != pid)) return GF_REQUIRES_NEW_INSTANCE;

	if ((codec_id==GF_CODECID_HEVC_TILES) && !ctx->base_ipid) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[TileAggr] Base HEVC PID not found for tiled HEVC PID %s\n", gf_filter_pid_get_name(pid) ));
		return GF_NOT_SUPPORTED;
	}

	if (!ctx->opid) ctx->opid = gf_filter_pid_new(filter);

	if (!ctx->base_ipid) {
		ctx->base_ipid = pid;
	}
	if (ctx->base_ipid == pid) {
		gf_filter_pid_copy_properties(ctx->opid, ctx->base_ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TILE_BASE, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SRD, NULL);
		//we keep the SRDREF property to indicate this was a tiled HEVC reassembled
		gf_filter_pid_set_property_str(ctx->opid, "isom:sabt", NULL);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		if (!p) return GF_NOT_SUPPORTED;
		hvcc = gf_odf_hevc_cfg_read(p->value.data.ptr, p->value.data.size, GF_FALSE);
		ctx->nalu_size_length = hvcc ? hvcc->nal_unit_size : 4;
		if (hvcc) gf_odf_hevc_cfg_del(hvcc);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (!p) return GF_NOT_SUPPORTED;
		ctx->base_id = p->value.uint;
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
		if (!p) return GF_NOT_SUPPORTED;
		if (ctx->base_id != p->value.uint) return GF_NOT_SUPPORTED;
	}

	return GF_OK;
}


static GF_Err tileagg_process(GF_Filter *filter)
{
	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);
	u32 i, j, count = gf_filter_get_ipid_count(filter);
	GF_FilterPacket *pck, *dst_pck;
	u64 min_cts = GF_FILTER_NO_TS;
	u32 pck_size, size = 0;
	u32 pos;
	Bool has_sei_suffix = GF_FALSE;
	const char *data;
	u8 *output;
	if (!ctx->base_ipid) return GF_EOS;

	pck = gf_filter_pid_get_packet(ctx->base_ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->base_ipid)) {
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	min_cts = gf_filter_pck_get_cts(pck);
	gf_filter_pck_get_data(pck, &pck_size);
	size = pck_size;

	for (i=0; i<count; i++) {
		u64 cts;
		Bool do_drop=GF_FALSE;
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		if (pid==ctx->base_ipid) continue;
		while (1) {
			pck = gf_filter_pid_get_packet(pid);
			if (!pck) return GF_OK;

			cts = gf_filter_pck_get_cts(pck);
			if (cts < min_cts) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[TileAggr] Tiled pid %s with cts "LLU" less than base tile pid cts "LLU" - discarding packet\n", gf_filter_pid_get_name(pid), cts, min_cts ));
				gf_filter_pid_drop_packet(pid);
			} else {
				break;
			}
		}
		assert(pck);
		if (cts > min_cts) continue;

		for (j=0; j<ctx->tiledrop.nb_items; j++) {
			if (ctx->tiledrop.vals[j] == i)
				do_drop=GF_TRUE;
		}
		if (do_drop) {
			gf_filter_pid_drop_packet(pid);
			continue;
		}

		gf_filter_pck_get_data(pck, &pck_size);
		size += pck_size;
	}
	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);

	pck = gf_filter_pid_get_packet(ctx->base_ipid);
	gf_filter_pck_merge_properties(pck, dst_pck);
	data = gf_filter_pck_get_data(pck, &pck_size);

	//copy all NAL from base except SEI suffixes
	gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);
	pos = 0;
	size = 0;
	while (pos<pck_size) {
		u32 nal_size = gf_bs_read_int(ctx->bs_r, 8*ctx->nalu_size_length);
		u8 nal_type;
		gf_bs_read_int(ctx->bs_r, 1);
		nal_type = gf_bs_read_int(ctx->bs_r, 6);
		gf_bs_read_int(ctx->bs_r, 1);
		gf_bs_skip_bytes(ctx->bs_r, nal_size-1);
		if (nal_type == GF_HEVC_NALU_SEI_SUFFIX) {
			has_sei_suffix = GF_TRUE;
		} else {
			memcpy(output+size, data+pos, nal_size + ctx->nalu_size_length);
			size += nal_size + ctx->nalu_size_length;
		}
		pos += nal_size + ctx->nalu_size_length;
	}

	for (i=0; i<count; i++) {
		u64 cts;
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		if (pid==ctx->base_ipid) continue;
		pck = gf_filter_pid_get_packet(pid);
		//can happen if we drop one tile
		if (!pck) continue;

		cts = gf_filter_pck_get_cts(pck);
		if (cts != min_cts) continue;

		data = gf_filter_pck_get_data(pck, &pck_size);
		memcpy(output+size, data, pck_size);
		size += pck_size;

		gf_filter_pid_drop_packet(pid);
	}

	//append all SEI suffixes
	if (has_sei_suffix) {
		pck = gf_filter_pid_get_packet(ctx->base_ipid);
		data = gf_filter_pck_get_data(pck, &pck_size);
		gf_bs_reassign_buffer(ctx->bs_r, data, pck_size);

		pos = 0;
		while (pos<pck_size) {
			u32 nal_size = gf_bs_read_int(ctx->bs_r, 8*ctx->nalu_size_length);
			u8 nal_type;
			gf_bs_read_int(ctx->bs_r, 1);
			nal_type = gf_bs_read_int(ctx->bs_r, 6);
			gf_bs_read_int(ctx->bs_r, 1);
			gf_bs_skip_bytes(ctx->bs_r, nal_size-1);
			if (nal_type == GF_HEVC_NALU_SEI_SUFFIX) {
				memcpy(output+size, data+pos, nal_size + ctx->nalu_size_length);
				size += nal_size + ctx->nalu_size_length;
			}
			pos += nal_size + ctx->nalu_size_length;
		}
	}

	gf_filter_pid_drop_packet(ctx->base_ipid);

	gf_filter_pck_send(dst_pck);
	return GF_OK;
}

static GF_Err tileagg_initialize(GF_Filter *filter)
{
	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);
	ctx->bs_r = gf_bs_new((char *)ctx, 1, GF_BITSTREAM_READ);

	return GF_OK;
}

static void tileagg_finalize(GF_Filter *filter)
{
	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);
	gf_bs_del(ctx->bs_r);
}

static const GF_FilterCapability TileAggCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_TILE_BASE, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT_STATIC, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC_TILES),
};

#define OFFS(_n)	#_n, offsetof(GF_TileAggCtx, _n)

static const GF_FilterArgs TileAggArgs[] =
{
	{ OFFS(tiledrop), "specify indexes of tiles to drop", GF_PROP_UINT_LIST, "", NULL, GF_FS_ARG_UPDATE},
	{0}
};

GF_FilterRegister TileAggRegister = {
	.name = "tileagg",
	GF_FS_SET_DESCRIPTION("HEVC tile aggregator")
	GF_FS_SET_HELP("This filter reaggregates a set of split tiled HEVC streams (`hvt1` or `hvt2` in isobmff) into a single HEVC stream.")
	.private_size = sizeof(GF_TileAggCtx),
	SETCAPS(TileAggCaps),
	.initialize = tileagg_initialize,
	.finalize = tileagg_finalize,
	.args = TileAggArgs,
	.configure_pid = tileagg_configure_pid,
	.process = tileagg_process,
	.max_extra_pids = (u32) (-1),
};

const GF_FilterRegister *tileagg_register(GF_FilterSession *session)
{
	return &TileAggRegister;
}


