/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2021
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

	u32 flush_packets;
	const GF_PropertyValue *sabt;

	Bool check_connections;

} GF_TileAggCtx;


static GF_Err tileagg_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 codec_id=0;
	const GF_PropertyValue *p;
	GF_HEVCConfig *hvcc;

	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);

	if (is_remove) {
		if (ctx->base_ipid == pid) {
			if (ctx->opid) {
				gf_filter_pid_remove(ctx->opid);
				ctx->opid = NULL;
			}
		}
		return GF_OK;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p)
		return GF_NOT_SUPPORTED;
	codec_id = p->value.uint;

	//a single HEVC base is allowed per instance
	if ((codec_id==GF_CODECID_HEVC) && ctx->base_ipid && (ctx->base_ipid != pid))
		return GF_REQUIRES_NEW_INSTANCE;

	//a tile pid connected before our base, check we have the same base ID, otherwise we need a new instance
	if ((codec_id==GF_CODECID_HEVC) && !ctx->base_ipid && ctx->base_id) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (!p)
			return GF_NOT_SUPPORTED;

		if (ctx->base_id != p->value.uint)
			return GF_REQUIRES_NEW_INSTANCE;

		ctx->base_ipid = pid;
	}
	//tile pid connecting after another tile pid,  we share the same base
	if ((codec_id==GF_CODECID_HEVC_TILES) && ctx->base_id) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
		if (!p) return GF_NOT_SUPPORTED;
		if (ctx->base_id != p->value.uint)
			return GF_REQUIRES_NEW_INSTANCE;
	}


	if (!ctx->base_ipid && (codec_id==GF_CODECID_HEVC) ) {
		ctx->base_ipid = pid;
		if (!ctx->opid) {
			ctx->opid = gf_filter_pid_new(filter);
		}
	}
	ctx->check_connections = GF_TRUE;

	if (ctx->base_ipid == pid) {
		gf_filter_pid_copy_properties(ctx->opid, ctx->base_ipid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TILE_BASE, NULL);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SRD, NULL);
		//we keep the SRDREF property to indicate this was a tiled HEVC reassembled
		gf_filter_pid_set_property_str(ctx->opid, "isom:sabt", NULL);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		//not ready yet
		if (!p) return GF_OK;
		hvcc = gf_odf_hevc_cfg_read(p->value.data.ptr, p->value.data.size, GF_FALSE);
		ctx->nalu_size_length = hvcc ? hvcc->nal_unit_size : 4;
		if (hvcc) gf_odf_hevc_cfg_del(hvcc);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (!p)
			return GF_NOT_SUPPORTED;
		ctx->base_id = p->value.uint;

		ctx->sabt = gf_filter_pid_get_property_str(pid, "isom:sabt");
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
		if (!p) return GF_NOT_SUPPORTED;
		if (!ctx->base_ipid) {
			ctx->base_id = p->value.uint;
		}
		//we already checked the same base ID is used
	}

	return GF_OK;
}

static GF_Err tileagg_set_eos(GF_Filter *filter, GF_TileAggCtx *ctx)
{
	u32 i, count = gf_filter_get_ipid_count(filter);
	for (i=0; i<count; i++) {
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		gf_filter_pid_set_discard(pid, GF_TRUE);
	}

	gf_filter_pid_set_eos(ctx->opid);
	return GF_EOS;
}

static GF_Err tileagg_process(GF_Filter *filter)
{
	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);
	u32 i, j, count = gf_filter_get_ipid_count(filter);
	GF_FilterPacket *dst_pck, *base_pck;
	u64 min_cts = GF_FILTER_NO_TS;
	u32 pck_size, final_size, size = 0;
	u32 pos, nb_ready=0;
	u32 sabt_idx;
	Bool has_sei_suffix = GF_FALSE;
	const char *data;
	u8 *output;
	if (!ctx->base_ipid) return GF_EOS;

	if (ctx->check_connections) {
		if (gf_filter_connections_pending(filter))
			return GF_OK;
		ctx->check_connections = GF_FALSE;
	}

	base_pck = gf_filter_pid_get_packet(ctx->base_ipid);
	if (!base_pck) {
		ctx->flush_packets = 0;
		if (gf_filter_pid_is_eos(ctx->base_ipid)) {
			return tileagg_set_eos(filter, ctx);
		}
		return GF_OK;
	}
	min_cts = gf_filter_pck_get_cts(base_pck);
	gf_filter_pck_get_data(base_pck, &pck_size);
	size = pck_size;

	for (i=0; i<count; i++) {
		GF_FilterPacket *pck;
		u64 cts;
		Bool do_drop=GF_FALSE;
		GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
		if (pid==ctx->base_ipid) continue;
		while (1) {
			pck = gf_filter_pid_get_packet(pid);
			if (!pck) {
				if (gf_filter_pid_is_eos(pid)) {
					return tileagg_set_eos(filter, ctx);
				}
				//if we are flushing a segment, consider the PID discarded if no packet
				//otherwise wait for packet
				if (! ctx->flush_packets)
					return GF_OK;
				break;
			}

			cts = gf_filter_pck_get_cts(pck);
			if (cts < min_cts) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TileAgg] Tiled pid %s with cts "LLU" less than base tile pid cts "LLU" - discarding packet\n", gf_filter_pid_get_name(pid), cts, min_cts ));
				gf_filter_pid_drop_packet(pid);
			} else {
				break;
			}
		}

		if (!pck) continue;

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
		nb_ready++;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[TileAgg] reaggregating CTS "LLU" %d ready %d pids (nb flush pck %d)\n", min_cts, nb_ready+1, count, ctx->flush_packets));
	if (ctx->flush_packets)
		ctx->flush_packets--;

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	final_size = size;

	gf_filter_pck_merge_properties(base_pck, dst_pck);
	data = gf_filter_pck_get_data(base_pck, &pck_size);

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

	sabt_idx = 0;
	while (1) {
		u32 pid_id = 0;
		if (ctx->sabt) {
			if (sabt_idx >= ctx->sabt->value.uint_list.nb_items)
				break;
			pid_id = ctx->sabt->value.uint_list.vals[sabt_idx];
			sabt_idx++;
		}
		for (i=0; i<count; i++) {
			u64 cts;
			GF_FilterPacket *pck;
			GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
			if (pid==ctx->base_ipid) continue;
			if (pid_id) {
				const GF_PropertyValue *p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
				if (!p || (p->value.uint != pid_id))
					continue;
			}
			pck = gf_filter_pid_get_packet(pid);
			//can happen if we drop one tile
			if (!pck) continue;

			cts = gf_filter_pck_get_cts(pck);
			if (cts != min_cts) continue;

			data = gf_filter_pck_get_data(pck, &pck_size);
			memcpy(output+size, data, pck_size);
			size += pck_size;

			gf_filter_pid_drop_packet(pid);
			if (pid_id)
				break;
		}
		if (!pid_id)
			break;
	}

	//append all SEI suffixes
	if (has_sei_suffix) {
		data = gf_filter_pck_get_data(base_pck, &pck_size);
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

	if (size < 	final_size)
		gf_filter_pck_truncate(dst_pck, size);
		
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

static Bool tileagg_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);
	if (evt->base.type != GF_FEVT_PLAY_HINT) return GF_FALSE;
	if (evt->play.forced_dash_segment_switch) {
		//this assumes the dashin module performs regulation of output in case of losses
		//otherwise it may dispatch more than one segment in the input buffer
		if (!ctx->flush_packets)
			gf_filter_pid_get_buffer_occupancy(ctx->base_ipid, NULL, &ctx->flush_packets, NULL, NULL);
		else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[TileAgg] Something is wrong in demuxer, received segment flush event but previous segment is not yet flushed !\n" ));
		}
	}
	return GF_TRUE;
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
	.process_event = tileagg_process_event,
	.max_extra_pids = (u32) (-1),
};

const GF_FilterRegister *tileagg_register(GF_FilterSession *session)
{
	return &TileAggRegister;
}


