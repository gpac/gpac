	/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / ROUTE (ATSC3, DVB-I) input filter
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

#include "in_route.h"

#ifndef GPAC_DISABLE_ROUTE

static void update_first_frag(GF_ROUTEEventFileInfo *finfo)
{
	//first bytes of segment have been patched (first seg only), update frag size and offset
	//and update blob if needed
	gf_mx_p(finfo->blob->mx);
	finfo->frags[0].size += finfo->frags[0].offset;
	finfo->frags[0].offset = 0;
	finfo->blob->last_modification_time = gf_sys_clock_high_res();
	if (finfo->blob->size < finfo->frags[0].size)
		finfo->blob->size = finfo->frags[0].size;
	gf_mx_v(finfo->blob->mx);
}
//patch TS file, replacing all 188 bytes packets overlaping a gap by padding packets
static Bool routein_repair_segment_ts_local(ROUTEInCtx *ctx, u32 service_id, GF_ROUTEEventFileInfo *finfo, Bool repair_start_only)
{
    u32 i, pos;
    Bool drop_if_first = GF_FALSE;
    u8 *data = finfo->blob->data;
    u32 patch_first_range_size = 0;


	if (repair_start_only) {
		patch_first_range_size = finfo->frags[0].offset;
	}

    pos = 0;
    for (i=0; i<finfo->nb_frags; i++) {
        u32 start_range = finfo->frags[i].offset;
        u32 end_range = finfo->frags[i].size;

		//if we missed first 4 packets, we cannot rely on PAT/PMT being present in the rest of the segment
		//we could further check this at the demux level, but for now we drop the segment
        if (!i && (start_range>4*188))
			drop_if_first = GF_TRUE;

        end_range += start_range;
        //reset all missed byte ranges as padding packets
        start_range -= pos;
        while (start_range % 188) start_range++;
        while (pos<start_range) {
            data[pos] = 0x47;
            data[pos+1] = 0x1F;
            data[pos+2] = 0xFF;
            data[pos+3] = 0x10;
            pos += 188;
        }
        //end range not aligned with a packet start, rewind position to prev packet start
		while (end_range % 188) end_range--;
		pos = end_range;

		if (patch_first_range_size && (pos>=patch_first_range_size)) {
			update_first_frag(finfo);
			return GF_FALSE;
		}
    }
    //and patch all end packets
    while (pos<finfo->blob->size) {
        data[pos] = 0x47;
        data[pos+1] = 0x1F;
        data[pos+2] = 0xFF;
        data[pos+3] = 0x10;
        pos += 188;
    }
	//remove corrupted flag
	finfo->partial = GF_LCTO_PARTIAL_NONE;
	gf_route_dmx_patch_frag_info(ctx->route_dmx, service_id, finfo, 0, finfo->blob->size);
	return drop_if_first;
}

//top boxes we look for in segments
static const char *top_codes[] = {"styp", "emsg", "prft", "moof", "mdat", "free", "sidx", "ssix"};
static u32 nb_top_codes = GF_ARRAY_LENGTH(top_codes);

static u32 next_top_level_box(GF_ROUTEEventFileInfo *finfo, u8 *data, u32 size, u32 *cur_pos, u32 *box_size)
{
    u32 pos = *cur_pos;
	u32 cur_frag = 0;
	while (cur_frag < finfo->nb_frags) {
		//in range, can go
		if ((finfo->frags[cur_frag].offset <= pos) && (finfo->frags[cur_frag].offset + finfo->frags[cur_frag].size > pos)) {
			break;
		}
		//before range, adjust pos
		if (finfo->frags[cur_frag].offset > pos) {
			pos = finfo->frags[cur_frag].offset;
			break;
		}
		//after range, go to next
		cur_frag++;
		//current pos is outside last valid range, no more top-level boxes to parse
		if (cur_frag==finfo->nb_frags)
			return 0;
	}
		
    while (pos + 8 < size) {
        u32 i;
        u32 type_idx = 0;
		u32 first_box = 0;
		u32 first_box_size = 0;
        for (i=0; i<nb_top_codes; i++) {
			if ((data[pos]==top_codes[i][0]) && (data[pos+1]==top_codes[i][1]) && (data[pos+2]==top_codes[i][2]) && (data[pos+3]==top_codes[i][3])) {
				first_box = pos;
				type_idx = i;
				break;
            }
        }
        //we need at least 4 bytes size header
        if (first_box<4) {
            pos++;
            continue;
        }
		first_box_size = GF_4CC(data[first_box-4], data[first_box-3], data[first_box-2], data[first_box-1]);
		if (first_box_size<8) {
			pos++;
			continue;
		}
        *cur_pos = first_box-4;
        *box_size = first_box_size;
        return GF_4CC(top_codes[type_idx][0], top_codes[type_idx][1], top_codes[type_idx][2], top_codes[type_idx][3]);
    }
    return 0;
}

//patch ISOBMFF file, replacing all top-level boxes overlaping a gap by free boxes
static void routein_repair_segment_isobmf_local(ROUTEInCtx *ctx, u32 service_id, GF_ROUTEEventFileInfo *finfo, Bool repair_start_only)
{
    u8 *data = finfo->blob->data;
    u32 size = finfo->blob->size;
    u32 pos = 0;
	u32 prev_moof_pos = 0;
	u32 patch_first_range_size = GF_FALSE;

	if (repair_start_only) {
		size = finfo->frags[0].offset + finfo->frags[0].size;
		patch_first_range_size = finfo->frags[0].offset;
	}
	//walk through all possible top-level boxes in order
    //if box completely in a received byte range, keep as is
    //if mdat or free box, keep as is
    //otherwise change box type to free
    while ((u64)pos + 8 < size) {
        u32 i;
		Bool is_mdat = GF_FALSE;
        Bool box_complete = GF_FALSE;
        u32 prev_pos = pos;
        u32 box_size = 0;

		if (patch_first_range_size && (pos+8 >= patch_first_range_size)) {
			update_first_frag(finfo);
			return;
		}

        u32 type = next_top_level_box(finfo, data, size, &pos, &box_size);
        //no more top-level found, patch from current pos until end of payload
        if (!type) {
			if (patch_first_range_size)
				return;

            u32 remain = size - pos;
            gf_assert(remain);
            if (remain<8) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to patch end of corrupted segment, segment size not big enough to hold the final box header, something really corrupted in source data\n"));
                return;
            }
            data[pos] = (remain>>24) & 0xFF;
            data[pos+1] = (remain>>16) & 0xFF;
            data[pos+2] = (remain>>8) & 0xFF;
            data[pos+3] = (remain) & 0xFF;
            data[pos+4] = 'f';
            data[pos+5] = 'r';
            data[pos+6] = 'e';
            data[pos+7] = 'e';
			//remove corrupted flag
			finfo->partial = GF_LCTO_PARTIAL_NONE;
            return;
        }
        //we missed a box header, insert one at previous pos, indicating a free box !!
        if (pos > prev_pos) {
            u32 missed_size = pos - prev_pos;
            data[prev_pos] = (missed_size>>24) & 0xFF;
            data[prev_pos+1] = (missed_size>>16) & 0xFF;
            data[prev_pos+2] = (missed_size>>8) & 0xFF;
            data[prev_pos+3] = (missed_size) & 0xFF;
            data[prev_pos+4] = 'f';
            data[prev_pos+5] = 'r';
            data[prev_pos+6] = 'e';
            data[prev_pos+7] = 'e';
        }
        if (type == GF_4CC('f','r','e','e')) {
            box_complete = GF_TRUE;
        } else if (type == GF_4CC('m','d','a','t')) {
			if (ctx->repair != ROUTEIN_REPAIR_STRICT) {
				box_complete = GF_TRUE;
			} else {
				is_mdat = GF_TRUE;
			}
		} else if (type == GF_4CC('m','o','o','f')) {
			prev_moof_pos = pos;
		}

        if (!box_complete) {
            //box is only partially received
            for (i=0; i<finfo->nb_frags; i++) {
                if (pos + box_size < finfo->frags[i].offset)
                    break;
                if ((pos >= finfo->frags[i].offset) && (pos+box_size<=finfo->frags[i].offset + finfo->frags[i].size)) {
                    box_complete = GF_TRUE;
                    break;
                }
            }
        }
        if (box_complete) {
            pos += box_size;
            continue;
        }
		//incomplete mdat (strict mode), discard previous moof
		if (is_mdat) {
			data[prev_moof_pos+4] = 'f';
			data[prev_moof_pos+5] = 'r';
			data[prev_moof_pos+6] = 'e';
			data[prev_moof_pos+7] = 'e';
		}
        //incomplete box, move to free (not changing size)
        data[pos+4] = 'f';
        data[pos+5] = 'r';
        data[pos+6] = 'e';
        data[pos+7] = 'e';
        pos += box_size;
    }
	//remove corrupted flag
	finfo->partial = GF_LCTO_PARTIAL_NONE;
	gf_route_dmx_patch_frag_info(ctx->route_dmx, service_id, finfo, 0, size);
}

static void route_repair_build_ranges_full(ROUTEInCtx *ctx, RepairSegmentInfo *rsi, GF_ROUTEEventFileInfo *finfo)
{
	u32 i;

	//collect decoder stats, or if not found direct output
	if (ctx->opid) {
		GF_FilterPidStatistics stats;
		GF_Err e = gf_filter_pid_get_statistics(ctx->opid, &stats, GF_STATS_DECODER_SINK);
		if (e) e = gf_filter_pid_get_statistics(ctx->opid, &stats, GF_STATS_SINK);
		if (!e) {
			//log is set as warning for now as this is work in progress
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] Repairing segment %s - buffer status: %d ms (%u for rebuffer %u max buffer)\n", finfo->filename, (u32) (stats.buffer_time/1000) , (u32) (stats.min_playout_time/1000), (u32) (stats.max_buffer_time/1000) ));
		}
	}

	//compute byte range - max ranges to repair: if N interval received, at max N+1 interval losts
	//TODO, select byte range priorities & co, check if we want multiple byte ranges??
	for (i=0; i<=finfo->nb_frags; i++) {
		u32 br_start = 0, br_end = 0;
		// first range
		if (!i) {
			br_end = finfo->frags[i].offset;
		}
		//middle ranges
		else if (i < finfo->nb_frags) {
			br_start = finfo->frags[i-1].offset + rsi->finfo.frags[i-1].size;
			br_end  = finfo->frags[i].offset;
		}
		//last range
		else {
			br_start = finfo->frags[finfo->nb_frags-1].offset + finfo->frags[finfo->nb_frags-1].size;
			br_end = finfo->total_size;
		}

		//this was correctly received !
		if (br_end <= br_start) continue;

		RouteRepairRange *rr = gf_list_pop_back(ctx->seg_range_reservoir);
		if (!rr) {
			GF_SAFEALLOC(rr, RouteRepairRange);
			if (!rr) {
				rsi->nb_errors++;
				continue;
			}
		} else {
			memset(rr, 0, sizeof(RouteRepairRange));
		}
		rr->br_start = br_start;
		rr->br_end = br_end;

		gf_list_add(rsi->ranges, rr);
	}

}

void routein_queue_repair(ROUTEInCtx *ctx, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo)
{
	u32 fast_repair = 0;
	//TODO handle late data
	if (evt==GF_ROUTE_EVT_LATE_DATA) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Late data patching not yet implemented !\n"));
		return;
	}

	if (ctx->repair==ROUTEIN_REPAIR_NO) {
		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		return;
	}

	//patch start of file only at tune-in for LL mode, otherwise use regular route repair
	if (ctx->llmode && !finfo->first_toi_received && (evt==GF_ROUTE_EVT_DYN_SEG_FRAG)
		&& (finfo->partial==GF_LCTO_PARTIAL_ANY)
		&& finfo->nb_frags
		&& finfo->frags[0].offset
	) {
		fast_repair = 2;
	}
	else if (ctx->repair<ROUTEIN_REPAIR_FULL) {
		if (evt==GF_ROUTE_EVT_DYN_SEG_FRAG) {
			routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
			return;
		}
		fast_repair = 1;
	}

	if (fast_repair) {
		Bool drop_if_first = GF_FALSE;
		if (finfo->blob->mx)
			gf_mx_p(finfo->blob->mx);

		if (strstr(finfo->filename, ".ts") || strstr(finfo->filename, ".m2ts")) {
			drop_if_first = routein_repair_segment_ts_local(ctx, evt_param, finfo, (fast_repair==2) ? GF_TRUE : GF_FALSE);
		} else {
			routein_repair_segment_isobmf_local(ctx, evt_param, finfo, (fast_repair==2) ? GF_TRUE : GF_FALSE);
		}

		if (finfo->blob->mx)
			gf_mx_v(finfo->blob->mx);
		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, drop_if_first);
		return;
	}
	//TODO, pass any repair URL info coming from broadcast
	if (!ctx->repair_url) {
		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		return;
	}

	//for now we only trigger file repair
	if (evt==GF_ROUTE_EVT_DYN_SEG_FRAG) {
		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		return;
	}

	//check if not already queued (for when we will allow repair mid-segments)
	u32 i, count = gf_list_count(ctx->seg_repair_queue);
	for (i=0; i<count; i++) {
		RepairSegmentInfo *rsi = gf_list_get(ctx->seg_repair_queue, i);
		if ((rsi->finfo.tsi==finfo->tsi) && (rsi->finfo.toi==finfo->toi)) {
			//remember event type (fragment or segment)
			rsi->evt = evt;
			return;
		}
	}

	//queue up our repair
	RepairSegmentInfo *rsi = gf_list_pop_back(ctx->seg_repair_reservoir);
	if (!rsi) {
		GF_SAFEALLOC(rsi, RepairSegmentInfo);
		rsi->ranges = gf_list_new();
	}

	if (!rsi) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Failed to allocate repair entry %s (TSI=%u, TOI=%u)\n", finfo->filename, finfo->tsi, finfo->toi));
		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		return;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Queing repair for object %s (TSI=%u, TOI=%u)\n", finfo->filename, finfo->tsi, finfo->toi));

	rsi->evt = evt;
	rsi->service_id = evt_param;
	rsi->finfo = *finfo;
	rsi->filename = gf_strdup(finfo->filename);
	rsi->finfo.filename = rsi->filename;

	route_repair_build_ranges_full(ctx, rsi, finfo);

	//mark blob as in transfer and corrupted - this will be unmarked when doing the final dispatch
	gf_mx_p(finfo->blob->mx);
	finfo->blob->flags |= GF_BLOB_IN_TRANSFER|GF_BLOB_CORRUPTED;
	gf_mx_v(finfo->blob->mx);

	gf_list_add(ctx->seg_repair_queue, rsi);
}



static void repair_session_done(ROUTEInCtx *ctx, RouteRepairSession *rsess, GF_Err res_code)
{
	RepairSegmentInfo *rsi = rsess->current_si;
	if (!rsi) return;

	//notify routedmx we have received a byte range
	if (!rsi->removed)
		gf_route_dmx_patch_frag_info(ctx->route_dmx, rsi->service_id, &rsi->finfo, rsess->range->br_start, rsess->range->br_end);

	rsess->current_si = NULL;
	gf_list_add(ctx->seg_range_reservoir, rsess->range);
	rsess->range = NULL;
	rsi->pending--;
	if (res_code<0) rsi->nb_errors++;
	if (rsi->pending) return;
	if (rsi->removed) {
		gf_list_transfer(ctx->seg_range_reservoir, rsi->ranges);
	}

	if (gf_list_count(rsi->ranges)) return;

	gf_mx_p(rsi->finfo.blob->mx);
	if (!rsi->nb_errors) {
		rsi->finfo.partial = GF_LCTO_PARTIAL_NONE;
		rsi->finfo.blob->flags &= ~GF_BLOB_CORRUPTED;
	}
	//not a fragment, remove in-transfer flag
	if (rsi->evt!=GF_ROUTE_EVT_DYN_SEG_FRAG)
		rsi->finfo.blob->flags &= ~GF_BLOB_IN_TRANSFER;
	gf_mx_v(rsi->finfo.blob->mx);

	if (!rsi->removed) {
		//flush
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repair done for object %s (TSI=%u, TOI=%u)%s\n", rsi->finfo.filename, rsi->finfo.tsi, rsi->finfo.toi, rsi->nb_errors ? " - errors remain" : ""));

		routein_on_event_file(ctx, rsi->evt, rsi->service_id, &rsi->finfo, GF_TRUE, GF_FALSE);
	}

	gf_list_del_item(ctx->seg_repair_queue, rsi);
	GF_List *bck = rsi->ranges;
	if (rsi->filename) gf_free(rsi->filename);
	memset(rsi, 0, sizeof(RepairSegmentInfo));
	rsi->ranges = bck;
	gf_list_add(ctx->seg_repair_reservoir, rsi);
}

#define REPAIR_BUF_SIZE	5000
static void repair_session_run(ROUTEInCtx *ctx, RouteRepairSession *rsess)
{
	GF_Err e;
	RepairSegmentInfo *rsi;
	char http_buf[REPAIR_BUF_SIZE];

restart:
	rsi = rsess->current_si;
	if (!rsi) {
		RouteRepairRange *rr = NULL;
		u32 i, count;
		count = gf_list_count(ctx->seg_repair_queue);
		for (i=0; i<count;i++) {
			u32 j, nb_ranges;
			rsi = gf_list_get(ctx->seg_repair_queue, i);
			nb_ranges = gf_list_count(rsi->ranges);
			for (j=0; j<nb_ranges; j++) {
				rr = gf_list_get(rsi->ranges, j);
				//todo check priotiry
			}
			if (rr) break;
			rsi = NULL;
		}
		if (!rsi) return;
		rsess->current_si = rsi;
		rsi->pending++;
		rsess->range = rr;
		gf_list_del_item(rsi->ranges, rr);
gf_assert(rsi->finfo.filename);
gf_assert(rsi->finfo.filename[0]);

		char *url = gf_url_concatenate(ctx->repair_url, rsi->finfo.filename);

		if (!rsess->dld) {
			GF_DownloadManager *dm = gf_filter_get_download_manager(ctx->filter);
			rsess->dld = gf_dm_sess_new(dm, url, GF_NETIO_SESSION_NOT_CACHED | GF_NETIO_SESSION_NOT_THREADED | GF_NETIO_SESSION_PERSISTENT, NULL, NULL, &e);
			if (rsess->dld)
				gf_dm_sess_set_netcap_id(rsess->dld, "__ignore");
		} else {
			e = gf_dm_sess_setup_from_url(rsess->dld, url, GF_FALSE);
		}
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Failed to setup download session for %s: %s\n", rsi->finfo.filename, gf_error_to_string(e)));
			gf_free(url);
			repair_session_done(ctx, rsess, e);
			return;
		}
		gf_dm_sess_set_range(rsess->dld, rsess->range->br_start, rsess->range->br_end-1, GF_TRUE);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] Queue request for %s byte range %u-%u\n", url, rsess->range->br_start, rsess->range->br_end-1));
		gf_free(url);
	}

	u32 offset = rsess->range->br_start + rsess->range->done;
	u32 nb_read=0;
	if (rsi->removed) {
		e = GF_URL_REMOVED;
		gf_dm_sess_abort(rsess->dld);
	} else {
		e = gf_dm_sess_fetch_data(rsess->dld, http_buf, REPAIR_BUF_SIZE, &nb_read);
		if (e==GF_IP_NETWORK_EMPTY) return;
	}

	if (offset + nb_read > rsess->range->br_end)
		e = GF_REMOTE_SERVICE_ERROR;

	if (nb_read && (e>=GF_OK)) {
		gf_mx_p(rsi->finfo.blob->mx);
		memcpy(rsi->finfo.blob->data + offset, http_buf, nb_read);
		gf_mx_v(rsi->finfo.blob->mx);
		rsess->range->done += nb_read;
		//TODO, update frag info
	}
	if (e==GF_OK) return;

	repair_session_done(ctx, rsess, e);
	if (e<0) return;

	goto restart;

}

GF_Err routein_do_repair(ROUTEInCtx *ctx)
{
	u32 i, nb_active=0;

	for (i=0; i<ctx->max_sess; i++) {
		RouteRepairSession *rsess = &ctx->http_repair_sessions[i];
		repair_session_run(ctx, rsess);
		if (rsess->current_si) nb_active++;
	}
	return nb_active ? GF_OK : GF_EOS;
}


void routein_repair_mark_file(ROUTEInCtx *ctx, u32 service_id, const char *filename, Bool is_delete)
{
	u32 i, count = gf_list_count(ctx->seg_repair_queue);
	for (i=0; i<count; i++) {
		RepairSegmentInfo *rsi = gf_list_get(ctx->seg_repair_queue, i);
		if ((rsi->service_id==service_id) && !strcmp(rsi->finfo.filename, filename)) {
			//we don't cancel sessions now, this should be done in session_done
			if (is_delete) {
				rsi->removed = GF_TRUE;
			} else {
				//TODO: decide if we need to be more agressive ?
			}
			return;
		}
	}
}


#endif /* GPAC_DISABLE_ROUTE */

