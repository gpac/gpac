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

//patch TS file, replacing all 188 bytes packets overlaping a gap by padding packets
static Bool routein_repair_segment_ts_local(ROUTEInCtx *ctx, GF_ROUTEEventFileInfo *finfo)
{
    u32 i, pos;
    Bool drop_if_first = GF_FALSE;
    u8 *data = finfo->blob->data;


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
	finfo->blob->flags &= ~GF_BLOB_CORRUPTED;
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
static void routein_repair_segment_isobmf_local(ROUTEInCtx *ctx, GF_ROUTEEventFileInfo *finfo)
{
    u8 *data = finfo->blob->data;
    u32 size = finfo->blob->size;
    u32 pos = 0;
	u32 prev_moof_pos = 0;
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
        u32 type = next_top_level_box(finfo, data, size, &pos, &box_size);
        //no more top-level found, patch from current pos until end of payload
        if (!type) {
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
			finfo->blob->flags &= ~GF_BLOB_CORRUPTED;
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
	finfo->blob->flags &= ~GF_BLOB_CORRUPTED;
}

#if 0
static GF_Err repair_intervale(GF_LCTFragInfo **frags, u32 *nb_frags, u32 *nb_alloc_frags, u32 start, u32 size, ROUTEInCtx *ctx, GF_ROUTEEventFileInfo *finfo) {
	//insert intervalle into frags
	GF_Err e;
	u32 nb_loss = size;
	u32 pos = start;
	u32 nb_inserted = 0, i;
	GF_DownloadManager *dm = gf_filter_get_download_manager(ctx->filter);
	GF_DownloadSession *dsess = NULL;

	gf_assert(finfo->blob->size == finfo->total_size);
	gf_assert(*nb_frags <= *nb_alloc_frags);

	if(start + size > finfo->total_size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Repair procedure failed for [%u, %u]: trying to get data beyond segment intervale \n", start, start+size-1));
		return GF_BAD_PARAM;
	}
	char *url = gf_url_concatenate(ctx->repair_url, finfo->filename);
	if (!dsess) {
		dsess = gf_dm_sess_new(dm, url, GF_NETIO_SESSION_NOT_CACHED | GF_NETIO_SESSION_NOT_THREADED | GF_NETIO_SESSION_PERSISTENT, NULL, NULL, &e);
		gf_dm_sess_set_netcap_id(dsess, "__ignore");
	} else {
		e = gf_dm_sess_setup_from_url(dsess, url, GF_FALSE);
	}
	gf_dm_sess_set_range(dsess, start, start+size-1, GF_TRUE);

	while (nb_loss) {
		u8 data[1000];
		u32 nb_read=0;
		e = gf_dm_sess_fetch_data(dsess, data, 1000, &nb_read);
		if (e==GF_IP_NETWORK_EMPTY) continue;
		if (e<0) break;

		nb_loss -= nb_read;
		memcpy(finfo->blob->data + pos, data, nb_read);
		pos += nb_read;
		if (e) break;
	}

	if (nb_loss) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Repair procedure failed for [%u, %u]: %u bytes were not received \n", start, start+size-1, nb_loss));
		e = GF_IP_NETWORK_FAILURE;
	} else {
		e = GF_OK;
	}
	nb_inserted = size - nb_loss;
	if(!nb_inserted) {
		gf_free(url);
		return e;
	}

	//update frags

	int start_frag = -1;
	int end_frag = -1;
	for (i=0; (i < *nb_frags) && (start_frag==-1 || end_frag==-1); i++) {
		if((start_frag == -1) && (start <= (*frags)[i].offset + (*frags)[i].size)) {
			start_frag = i;
		}

		if((end_frag == -1) && (start + nb_inserted < (*frags)[i].offset)) {
			end_frag = i;
		} 
	}
	if(start_frag == -1) {
		start_frag = *nb_frags;
	}
	if(end_frag == -1) {
		end_frag = *nb_frags;
	}

	if(start_frag == end_frag) {
		// insert new fragment between two already received fragments or at the end
		if (*nb_frags==*nb_alloc_frags) {
			*nb_alloc_frags *= 2;
			*frags = gf_realloc(*frags, sizeof(GF_LCTFragInfo) * *nb_alloc_frags);
		}
		memmove(&(*frags)[start_frag+1], &(*frags)[start_frag], sizeof(GF_LCTFragInfo) * (*nb_frags - start_frag));
		(*frags)[start_frag].offset = start;
		(*frags)[start_frag].size = nb_inserted;
		(*nb_frags)++;
	} else {
		int end = MAX(start + nb_inserted, (*frags)[end_frag-1].offset + (*frags)[end_frag-1].size);
		(*frags)[start_frag].offset = MIN((*frags)[start_frag].offset, start);
		(*frags)[start_frag].size = end - (*frags)[start_frag].offset;

		if(end_frag > start_frag + 1) {
			memmove(&(*frags)[start_frag+1], &(*frags)[end_frag], sizeof(GF_LCTFragInfo) * (*nb_frags - end_frag));
			*nb_frags += start_frag - end_frag + 1;
		}
	}
	gf_free(url);
	return e;
}

static Bool does_belong(GF_LCTFragInfo *frags, u32 nb_frags, u32 start, u32 size) {
	// check if intervale [start, start+size[ belongs to frags list of intervales
	u32 i;
	if(start > GF_UINT_MAX - size) return GF_FALSE;
	for(i=0; i < nb_frags; i++) {
		if(start < frags[i].offset) return GF_FALSE;
		if(start < frags[i].offset + frags[i].size) {
			return start + size <= frags[i].offset + frags[i].size;
		}
	}
	return GF_FALSE;
}

static GF_Err routein_repair_segment_isobmf_new(ROUTEInCtx *ctx, GF_ROUTEEventFileInfo *finfo) {
	int pos = 0;
	u32 box_size, type, i=0, size;
	u32 threshold = 1000; // seuil ! 

	u32 nb_frags = finfo->nb_frags;
	u32 nb_alloc_frags = nb_frags;
	Bool size_checked_belong = GF_FALSE, type_checked_belong = GF_FALSE;
	GF_LCTFragInfo *frags = gf_calloc(nb_alloc_frags, sizeof(GF_LCTFragInfo));
	GF_Err e;

	memcpy(frags, finfo->frags, nb_frags * sizeof(GF_LCTFragInfo));

	while(pos+8 <= finfo->total_size) {
		if(size_checked_belong || does_belong(frags, nb_frags, pos, 4)) {
			box_size = GF_4CC(finfo->blob->data[pos], finfo->blob->data[pos+1], finfo->blob->data[pos+2], finfo->blob->data[pos+3]);
			if(box_size == 0) {
				box_size = finfo->total_size - pos;
			}
			size_checked_belong = GF_FALSE;

			if(!type_checked_belong && !does_belong(frags, nb_frags, pos+4, 4)) {
				if(box_size <= threshold) {
					size = (pos + box_size + 8 > finfo->total_size)? box_size-4: box_size+4;
					GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Top-level box size below thresholed %u, repairing top-level box n°%u and header of next one\n", threshold, i));
					e = repair_intervale(&frags, &nb_frags, &nb_alloc_frags, pos+4, size, ctx, finfo); // whole box + size and type of next box if there is one
					if(e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Something went wrong while repairing  \n"));
						return (e == GF_IP_NETWORK_FAILURE)? e: GF_NON_COMPLIANT_BITSTREAM;
					}
					pos += box_size;
					size_checked_belong = (pos + box_size + 8 <= finfo->total_size);
					type_checked_belong = (pos + box_size + 8 <= finfo->total_size);
					i++;
					continue;
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repairing type header of top-level box n°%u\n", i));
					e = repair_intervale(&frags, &nb_frags, &nb_alloc_frags, pos+4, 4, ctx, finfo); // type!
					if(e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Something went wrong while repairing  \n"));
						return (e == GF_IP_NETWORK_FAILURE)? e: GF_NON_COMPLIANT_BITSTREAM;
					}
				}
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repairing header of top-level box n°%u\n", i));
			e = repair_intervale(&frags, &nb_frags, &nb_alloc_frags, pos, 8, ctx, finfo); // size + type
			if(e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Something went wrong while repairing header of top-level box\n"));
				return (e == GF_IP_NETWORK_FAILURE)? e: GF_NON_COMPLIANT_BITSTREAM;
			}
			box_size = GF_4CC(finfo->blob->data[pos], finfo->blob->data[pos+1], finfo->blob->data[pos+2], finfo->blob->data[pos+3]);
			if(box_size == 0) {
				box_size = finfo->total_size - pos;
			}
			size_checked_belong = GF_FALSE;
		}

		type = GF_4CC(finfo->blob->data[pos+4], finfo->blob->data[pos+5], finfo->blob->data[pos+6], finfo->blob->data[pos+7]);
		type_checked_belong = GF_FALSE;

		// check type
		if(type == GF_4CC('m', 'o', 'o', 'f')) {
			if(! does_belong(frags, nb_frags, pos, box_size)) {
				size = (pos + box_size + 8 > finfo->total_size)? box_size-8: box_size;
				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repairing top-level box n°%u (type 'moof')\n", i));
				e = repair_intervale(&frags, &nb_frags, &nb_alloc_frags, pos+8, size, ctx, finfo);
				if(e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Something went wrong while repairing moof top-level box\n"));
					return (e == GF_IP_NETWORK_FAILURE)? e: GF_NON_COMPLIANT_BITSTREAM;
				}
				size_checked_belong = (pos + box_size + 8 <= finfo->total_size);
				type_checked_belong = (pos + box_size + 8 <= finfo->total_size);
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Ignoring top-level box n°%u of type '%c%c%c%c'\n", i, finfo->blob->data[pos+4], finfo->blob->data[pos+5], finfo->blob->data[pos+6], finfo->blob->data[pos+7]));
		}
		pos += box_size;
		i++;
	}
	return GF_OK;
}

#endif

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
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[RPUTE] Repairing segment - buffer status: %d ms (%u for rebuffer %u max buffer)\n", (u32) (stats.buffer_time/1000) , (u32) (stats.min_playout_time/1000), (u32) (stats.max_buffer_time/1000) ));
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
	//TODO handle late data
	if (evt==GF_ROUTE_EVT_LATE_DATA) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Late data patching not yet implemented !\n"));
		return;
	}
	if (ctx->repair==ROUTEIN_REPAIR_NO) {
		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		return;
	}
	if (ctx->repair<ROUTEIN_REPAIR_FULL) {
		Bool drop_if_first = GF_FALSE;
		if (finfo->blob->mx)
			gf_mx_p(finfo->blob->mx);

		if (strstr(finfo->filename, ".ts") || strstr(finfo->filename, ".m2ts")) {
			drop_if_first = routein_repair_segment_ts_local(ctx, finfo);
		} else {
			routein_repair_segment_isobmf_local(ctx, finfo);
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

	route_repair_build_ranges_full(ctx, rsi, finfo);

	gf_mx_p(finfo->blob->mx);
	if (finfo->blob->flags & GF_BLOB_IN_TRANSFER)
		rsi->was_partial = GF_TRUE;
	finfo->blob->flags |= GF_BLOB_IN_TRANSFER;
	gf_mx_v(finfo->blob->mx);

	gf_list_add(ctx->seg_repair_queue, rsi);
}



static void repair_session_done(ROUTEInCtx *ctx, RouteRepairSession *rsess, GF_Err res_code)
{
	RepairSegmentInfo *rsi = rsess->current_si;
	if (!rsi) return;

	//notify routedmx we have received a byte range
	gf_routedmx_patch_frag_info(ctx->route_dmx, rsi->service_id, &rsi->finfo, rsess->range->br_start, rsess->range->br_end);

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

	if (!rsi->removed) {
		//flush
		gf_mx_p(rsi->finfo.blob->mx);
		if (!rsi->nb_errors) rsi->finfo.blob->flags &= ~GF_BLOB_CORRUPTED;
		if (!rsi->was_partial) rsi->finfo.blob->flags &= ~GF_BLOB_IN_TRANSFER;
		gf_mx_v(rsi->finfo.blob->mx);

		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repair done for object %s (TSI=%u, TOI=%u)%s\n", rsi->finfo.filename, rsi->finfo.tsi, rsi->finfo.toi, rsi->nb_errors ? " - errors remain" : ""));

		routein_on_event_file(ctx, rsi->evt, rsi->service_id, &rsi->finfo, GF_TRUE, GF_FALSE);
	}

	gf_list_del_item(ctx->seg_repair_queue, rsi);
	GF_List *bck = rsi->ranges;
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

		char *url = gf_url_concatenate(ctx->repair_url, rsi->finfo.filename);

		if (!rsess->dld) {
			GF_DownloadManager *dm = gf_filter_get_download_manager(ctx->filter);
			rsess->dld = gf_dm_sess_new(dm, url, GF_NETIO_SESSION_NOT_CACHED | GF_NETIO_SESSION_NOT_THREADED | GF_NETIO_SESSION_PERSISTENT, NULL, NULL, &e);
			if (rsess->dld)
				gf_dm_sess_set_netcap_id(rsess->dld, "__ignore");
		} else {
			e = gf_dm_sess_setup_from_url(rsess->dld, url, GF_FALSE);
		}
		gf_free(url);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Failed to setup download session for %s: %s\n", rsi->finfo.filename, gf_error_to_string(e)));
			repair_session_done(ctx, rsess, e);
			return;
		}
		gf_dm_sess_set_range(rsess->dld, rsess->range->br_start, rsess->range->br_end-1, GF_TRUE);
	}

	u32 offset = rsess->range->br_start + rsess->range->done;
	u32 nb_read=0;
	e = gf_dm_sess_fetch_data(rsess->dld, http_buf, REPAIR_BUF_SIZE, &nb_read);
	if (e==GF_IP_NETWORK_EMPTY) return;

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
	for (i=0; i<ctx->nb_http_repair; i++) {
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

