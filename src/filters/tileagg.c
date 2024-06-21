/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2023
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

#ifndef GPAC_DISABLE_TILEAGG

typedef struct
{
	GF_FilterPid *pid;
	u32 id;
} GF_TileAggInput;

typedef struct
{
	//options
	GF_PropUIntList tiledrop;
	u32 ttimeout;

	//internal
	GF_FilterPid *opid;
	GF_FilterPid *base_ipid;
	u32 nalu_size_length;
	u32 base_id, dash_grp_id;
	GF_List *ipids;

	GF_BitStream *bs_r;

	u32 flush_packets;
	const GF_PropertyValue *sabt;

	Bool check_connections;
	GF_Err in_error;
	u32 wait_start, wait_pid, force_flush;
	Bool is_playing;
	GF_FEVT_Play play_evt;

} GF_TileAggCtx;

#define TILEAGG_CFG_ERR(_msg) {\
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[TileAgg] Error configuring pid %s: %s\n", gf_filter_pid_get_name(pid), _msg ));\
		goto config_error;\
	}


static GF_Err tileagg_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 codec_id=0;
	u32 dash_grp_id=0;
	const GF_PropertyValue *p;
	GF_TileAggInput *pctx;

	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);
	if (ctx->in_error) return GF_SERVICE_ERROR;

	pctx = gf_filter_pid_get_udta(pid);
	if (is_remove) {
		if (pctx) {
			gf_list_del_item(ctx->ipids, pctx);
			gf_free(pctx);
		}
		if (ctx->base_ipid == pid) {
			if (ctx->opid) {
				gf_filter_pid_remove(ctx->opid);
				ctx->opid = NULL;
			}
		}
		return GF_OK;
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) TILEAGG_CFG_ERR("missing CodecID")
	codec_id = p->value.uint;

	Bool is_base_codec_type = GF_FALSE;
	if (codec_id==GF_CODECID_HEVC) is_base_codec_type = GF_TRUE;
	else if (codec_id==GF_CODECID_VVC) is_base_codec_type = GF_TRUE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_DEP_GROUP);
	if (p) dash_grp_id = p->value.uint;

	//a single base is allowed per instance
	if (is_base_codec_type && ctx->base_ipid && (ctx->base_ipid != pid))
		return GF_REQUIRES_NEW_INSTANCE;

	//a tile pid connected before our base, check we have the same base ID, otherwise we need a new instance
	if (is_base_codec_type && !ctx->base_ipid && ctx->base_id) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (!p) TILEAGG_CFG_ERR("missing PID ID")

		if (ctx->base_id != p->value.uint)
			return GF_REQUIRES_NEW_INSTANCE;

		if (ctx->dash_grp_id != dash_grp_id)
			return GF_REQUIRES_NEW_INSTANCE;

		ctx->base_ipid = pid;
		if (!ctx->opid) {
			ctx->opid = gf_filter_pid_new(filter);
		}
	}
	//tile pid connecting after another tile pid,  we share the same base
	if (!is_base_codec_type && ctx->base_id) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
		if (!p) TILEAGG_CFG_ERR("missing PID DependencyID")

		if (ctx->base_id != p->value.uint)
			return GF_REQUIRES_NEW_INSTANCE;

		if (ctx->dash_grp_id != dash_grp_id)
			return GF_REQUIRES_NEW_INSTANCE;
	}

	if (!pctx) {
		GF_SAFEALLOC(pctx, GF_TileAggInput);
		pctx->pid = pid;
		gf_filter_pid_set_udta(pid, pctx);
		gf_list_add(ctx->ipids, pctx);
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	pctx->id = p ? p->value.uint : 0;

	if (!ctx->base_ipid && is_base_codec_type ) {
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

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
		//not ready yet
		if (!p) return GF_OK;

		if (codec_id==GF_CODECID_HEVC) {
			GF_HEVCConfig *hvcc = gf_odf_hevc_cfg_read(p->value.data.ptr, p->value.data.size, GF_FALSE);
			ctx->nalu_size_length = hvcc ? hvcc->nal_unit_size : 4;
			if (hvcc) gf_odf_hevc_cfg_del(hvcc);
			gf_filter_pid_set_property_str(ctx->opid, "isom:sabt", NULL);
			ctx->sabt = gf_filter_pid_get_property_str(pid, "isom:sabt");
		} else {
			GF_VVCConfig *vvcc = gf_odf_vvc_cfg_read(p->value.data.ptr, p->value.data.size);
			ctx->nalu_size_length = vvcc ? vvcc->nal_unit_size : 4;
			if (vvcc) gf_odf_vvc_cfg_del(vvcc);
			gf_filter_pid_set_property_str(ctx->opid, "isom:subp", NULL);
			//todo, need to handle spor override, for now we only support all subpic in order
//			ctx->sabt = gf_filter_pid_get_property_str(pid, "isom:subp");
		}

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (!p) TILEAGG_CFG_ERR("missing PID ID, base PID assigned")
		ctx->base_id = p->value.uint;
		ctx->dash_grp_id = dash_grp_id;

	} else {
		u32 base_id;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_DEPENDENCY_ID);
		if (!p) TILEAGG_CFG_ERR("missing PID DependencyID, base PID not assigned")

		base_id = p->value.uint;
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
		if (!p || (base_id == p->value.uint)) TILEAGG_CFG_ERR("missing PID ID, base PID not assigned")

		if (!ctx->base_ipid) {
			ctx->base_id = base_id;
			ctx->dash_grp_id = dash_grp_id;
		}
		//we already checked the same base ID is used
	}

	//it may happen that we are already running when we get this pid connection, typically with very large number of tiles
	//post a play event on pid in this case
	if (ctx->is_playing) {
		GF_FilterEvent fevt;
		fevt.play = ctx->play_evt;
		fevt.base.on_pid = pid;
		gf_filter_pid_send_event(pid, &fevt);
	}
	return GF_OK;

config_error:
	if (ctx->opid) gf_filter_pid_set_eos(ctx->opid);
	if (ctx->base_id) ctx->in_error = GF_NON_COMPLIANT_BITSTREAM;
	return GF_SERVICE_ERROR;

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
	u32 i, j;
	GF_FilterPacket *dst_pck, *base_pck;
	u32 count;
	u64 min_cts = GF_FILTER_NO_TS;
	u32 pck_size, final_size, size = 0;
	u32 pos, nb_ready=0;
	u32 sabt_idx;
	Bool has_sei_suffix = GF_FALSE;
	const char *data;
	u8 *output;

	if (ctx->in_error) {
		return ctx->in_error;
	}
	if (!ctx->base_ipid) return GF_EOS;

	if (ctx->check_connections) {
		if (gf_filter_connections_pending(filter)) {
			return GF_OK;
		}
		ctx->check_connections = GF_FALSE;
	}

restart:

	base_pck = gf_filter_pid_get_packet(ctx->base_ipid);
	if (!base_pck) {
		ctx->flush_packets = 0;
		if (gf_filter_pid_is_eos(ctx->base_ipid)) {
			return tileagg_set_eos(filter, ctx);
		}

		if (ctx->wait_pid != ctx->base_id) {
			ctx->wait_start = gf_sys_clock();
			ctx->wait_pid = ctx->base_id;
			return GF_OK;
		} else if (!ctx->ttimeout || (!ctx->force_flush && (gf_sys_clock() - ctx->wait_start < ctx->ttimeout))) {
			//if not playing don't reschedule
			if (ctx->is_playing)
				gf_filter_ask_rt_reschedule(filter, 1000);
			return GF_OK;
		}
		ctx->wait_pid = 0;
		ctx->wait_start = 0;
		ctx->force_flush = GF_FALSE;
		count = gf_list_count(ctx->ipids);
		u32 nb_drop=0;
		for (i=0; i<count; i++) {
			GF_TileAggInput *pctx = gf_list_get(ctx->ipids, i);
			if (pctx->pid==ctx->base_ipid) continue;
			while (1) {
				GF_FilterPacket *pck = gf_filter_pid_get_packet(pctx->pid);
				if (!pck) break;
				gf_filter_pid_drop_packet(pctx->pid);
				nb_drop++;
			}
		}
		//we may have nothing to drop here when the flush_seg event is received
		if (nb_drop) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[TileAgg] No frames on tile base pid %s after %d ms, discarding all tiles\n", gf_filter_pid_get_name(ctx->base_ipid), gf_sys_clock() - ctx->wait_start ));
		}
		return GF_OK;
	}

	min_cts = gf_filter_pck_get_cts(base_pck);
	gf_filter_pck_get_data(base_pck, &pck_size);
	size = pck_size;

	count = gf_list_count(ctx->ipids);
	for (i=0; i<count; i++) {
		GF_FilterPacket *pck;
		u64 cts;
		Bool do_drop=GF_FALSE;
		GF_TileAggInput *pctx = gf_list_get(ctx->ipids, i);
		if (pctx->pid==ctx->base_ipid) continue;
		while (1) {
			pck = gf_filter_pid_get_packet(pctx->pid);
			if (!pck) {
				if (gf_filter_pid_is_eos(pctx->pid)) {
					return tileagg_set_eos(filter, ctx);
				}
				//if we are flushing a segment (ctx->flush_packets>0), consider the PID discarded if no packet
				//otherwise wait for packet
				if (! ctx->flush_packets) {
					if (ctx->wait_pid != pctx->id) {
						if (!ctx->wait_pid)
							ctx->wait_start = gf_sys_clock();
						ctx->wait_pid = pctx->id;
						return GF_OK;
					} else if (!ctx->ttimeout || (!ctx->force_flush && (gf_sys_clock() - ctx->wait_start < ctx->ttimeout))) {
						gf_filter_ask_rt_reschedule(filter, 1000);
						return GF_OK;
					} else {
						GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[TileAgg] No frames on tiled pid %s after %d ms, reaggregating with lost tiles\n", gf_filter_pid_get_name(pctx->pid), gf_sys_clock() - ctx->wait_start ));
						break;
					}
				}
				break;
			}

			cts = gf_filter_pck_get_cts(pck);
			if (cts < min_cts) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[TileAgg] Tiled pid %s with cts "LLU" less than base tile pid cts "LLU" - discarding packet\n", gf_filter_pid_get_name(pctx->pid), cts, min_cts ));
				gf_filter_pid_drop_packet(pctx->pid);
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
			gf_filter_pid_drop_packet(pctx->pid);
			continue;
		}

		gf_filter_pck_get_data(pck, &pck_size);
		size += pck_size;
		nb_ready++;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[TileAgg] reaggregating CTS "LLU" %d ready %d pids (nb flush pck %d)\n", min_cts, nb_ready+1, count, ctx->flush_packets));
	if (ctx->flush_packets) {
		ctx->flush_packets--;
		if (!ctx->flush_packets) ctx->force_flush = GF_FALSE;
	}
	if (ctx->wait_pid) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[TileAgg] reaggregating CTS "LLU" ready after %d ms wait\n", min_cts, gf_sys_clock()-ctx->wait_start));
		ctx->wait_pid = 0;
	}

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
	if (!dst_pck) return GF_OUT_OF_MEM;
	
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
			GF_TileAggInput *pctx = gf_list_get(ctx->ipids, i);
			if (pctx->pid==ctx->base_ipid) continue;
			if (pid_id && (pctx->id != pid_id)) {
				continue;
			}
			pck = gf_filter_pid_get_packet(pctx->pid);
			//can happen if we drop one tile
			if (!pck) continue;

			cts = gf_filter_pck_get_cts(pck);
			if (cts != min_cts) continue;

			data = gf_filter_pck_get_data(pck, &pck_size);
			memcpy(output+size, data, pck_size);
			size += pck_size;

			gf_filter_pid_drop_packet(pctx->pid);
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

	//flush asap, avoid recursion
	goto restart;

	return GF_OK;
}

static GF_Err tileagg_initialize(GF_Filter *filter)
{
	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);
	ctx->bs_r = gf_bs_new((char *)ctx, 1, GF_BITSTREAM_READ);
	ctx->ipids = gf_list_new();
	return GF_OK;
}

static void tileagg_finalize(GF_Filter *filter)
{
	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);
	gf_bs_del(ctx->bs_r);
	while (gf_list_count(ctx->ipids)) {
		GF_TileAggInput *pctx = gf_list_pop_back(ctx->ipids);
		gf_free(pctx);
	}
	gf_list_del(ctx->ipids);
}

static Bool tileagg_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_TileAggCtx *ctx = (GF_TileAggCtx *) gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		ctx->play_evt = evt->play;
		return GF_FALSE;
	case GF_FEVT_STOP:
	{
		ctx->is_playing = GF_FALSE;
		if (evt->play.initial_broadcast_play==2)
			return GF_FALSE;

		u32 i, count = gf_filter_get_ipid_count(filter);
		for (i=0; i<count; i++) {
			GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
			gf_filter_pid_set_discard(pid, GF_FALSE);
		}
	}
		return GF_FALSE;
	case GF_FEVT_PLAY_HINT:
		if (evt->play.forced_dash_segment_switch) {
			//this assumes the dashin module performs regulation of output in case of losses
			//otherwise it may dispatch more than one segment in the input buffer
			if (!ctx->flush_packets) {
				gf_filter_pid_get_buffer_occupancy(ctx->base_ipid, NULL, &ctx->flush_packets, NULL, NULL);
				//we already flushed everything, don't force flush
				if (!ctx->flush_packets) {
					ctx->wait_pid = 0;
					ctx->force_flush = GF_FALSE;
					return GF_TRUE;
				}
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[TileAgg] Something is wrong in demuxer, received segment flush event but previous segment is not yet flushed !\n" ));
			}
			ctx->force_flush = GF_TRUE;
		}
		return GF_TRUE;
	default:
		break;
	}
	return GF_FALSE;
}

static const GF_FilterCapability TileAggCaps[] =
{
	//HEVC tiles
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_TILE_BASE, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC_TILES),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	{0},
	//VVC subpic
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VVC),
	CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_TILE_BASE, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_VVC),
	{0},
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_VVC_SUBPIC),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_VVC),
};

#define OFFS(_n)	#_n, offsetof(GF_TileAggCtx, _n)

static const GF_FilterArgs TileAggArgs[] =
{
	{ OFFS(tiledrop), "specify indexes of tiles to drop", GF_PROP_UINT_LIST, NULL, NULL, GF_FS_ARG_UPDATE},
	{ OFFS(ttimeout), "number of milliseconds to wait until considering a tile packet lost, 0 waits forever", GF_PROP_UINT, "10000", NULL, GF_FS_ARG_UPDATE},
	{0}
};

GF_FilterRegister TileAggRegister = {
	.name = "tileagg",
	GF_FS_SET_DESCRIPTION("HEVC tile aggregator")
	GF_FS_SET_HELP("This filter aggregates a set of split tiled HEVC streams (`hvt1` or `hvt2` in ISOBMFF) into a single HEVC stream.")
	.private_size = sizeof(GF_TileAggCtx),
	.flags = GF_FS_REG_DYNAMIC_REUSE,
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
#else
const GF_FilterRegister *tileagg_register(GF_FilterSession *session)
{
	return NULL;
}
#endif //#ifndef GPAC_DISABLE_TILEAGG


