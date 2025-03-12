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

//top boxes we look for in segments, by rough order of frequency
static const u32 top_codes[] = {
	GF_4CC('m', 'o', 'o', 'f'),
	GF_4CC('m', 'd', 'a', 't'),
	GF_4CC('p', 'r', 'f', 't'),
	GF_4CC('e', 'm', 's', 'g'),
	GF_4CC('s', 't', 'y', 'p'),
	GF_4CC('f', 'r', 'e', 'e'),
	GF_4CC('s', 'i', 'd', 'x'),
	GF_4CC('s', 's', 'i', 'x')
};
static u32 nb_top_codes = GF_ARRAY_LENGTH(top_codes);

static u32 next_top_level_box(GF_ROUTEEventFileInfo *finfo, u8 *data, u32 size, u32 *cur_pos, u32 *box_size)
{
    u32 pos = *cur_pos;
	u32 frag_start = 0;
	u32 frag_end = 0;
	u32 cur_frag = 0;
	while (cur_frag < finfo->nb_frags) {
		//in range, can go
		if ((finfo->frags[cur_frag].offset <= pos) && (finfo->frags[cur_frag].offset + finfo->frags[cur_frag].size > pos)) {
			frag_start = finfo->frags[cur_frag].offset;
			frag_end = frag_start + finfo->frags[cur_frag].size;
			break;
		}
		//before range, adjust pos
		if (finfo->frags[cur_frag].offset > pos) {
			frag_start = finfo->frags[cur_frag].offset;
			frag_end = frag_start + finfo->frags[cur_frag].size;
			pos = frag_start;
			break;
		}
		//after range, go to next
		cur_frag++;
		//current pos is outside last valid range, no more top-level boxes to parse
		if (cur_frag==finfo->nb_frags)
			return 0;
	}
	//we must have 4 valid bytes for size
	if (pos < frag_start + 4) pos = frag_start+4;

	//we cannot look outside of fragment
    while (pos + 8 < frag_end) {
        u32 i;
		u32 first_box = 0;
		u32 first_box_size = 0;

		//look for our top-level codes
		u32 box_code = GF_4CC(data[pos], data[pos+1], data[pos+2], data[pos+3]);
        for (i=0; i<nb_top_codes; i++) {
			if (box_code == top_codes[i]) {
				first_box = pos;
				break;
            }
        }
        //not found
        if (!first_box) {
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
        return box_code;
    }
    return 0;
}

#define SAFETY_ERASE_BYTES	32
//patch ISOBMFF file, replacing all top-level boxes overlaping a gap by free boxes
static void routein_repair_segment_isobmf_local(ROUTEInCtx *ctx, u32 service_id, GF_ROUTEEventFileInfo *finfo, Bool repair_start_only)
{
    u8 *data = finfo->blob->data;
    u32 size = finfo->blob->size;
    u32 pos = 0;
	u32 prev_moof_pos = 0;
	u32 prev_mdat_pos = 0;
	u32 last_box_size = 0;
	u32 nb_patches = 0;
	Bool was_partial = finfo->partial!=GF_LCTO_PARTIAL_NONE;
	u32 patch_first_range_size = 0;

	if (repair_start_only) {
		size = finfo->frags[0].offset + finfo->frags[0].size;
		patch_first_range_size = finfo->frags[0].offset;
	}

	/* walk through all possible top-level boxes in order
    - if box completely in a received byte range, keep as is
    - if incomplete mdat:
		- if strict mode, move to free and move previous moof to free as well
		- otherwise keep as is
    - if incomplete moof, move to free
    - if hole between two known boxes (some box headers where partially or totally lost), inject free box
    - when injecting a box, if no mdat was detected after the last moof, move the moof to free

	Whenever moving a moof to free and no mdat was detected after the moof; we memset to 0 (at most) SAFETY_ERASE_BYTES bytes following the box header
	This avoids GPAC libisomedia trying to recover a free box into a moof box, which is done by checking for mfhd presence
    */
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
			if (patch_first_range_size) {
				gf_route_dmx_patch_frag_info(ctx->route_dmx, service_id, finfo, 0, size);
				return;
			}
			nb_patches++;
			if (!finfo->total_size && prev_pos && last_box_size) {
				gf_route_dmx_patch_blob_size(ctx->route_dmx, service_id, finfo, prev_pos+last_box_size);
				size = prev_pos+last_box_size;
				data = finfo->blob->data;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching file size to %u\n", finfo->filename, size));
			}
			gf_assert(size > pos);

            u32 remain = size - pos;
            if (remain<8) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to patch end of corrupted segment, segment size not big enough to hold the final box header, something really corrupted in source data\n"));
				size -= remain;
				gf_route_dmx_patch_blob_size(ctx->route_dmx, service_id, finfo, size);
                if (finfo->blob)
					finfo->blob->flags |= GF_BLOB_CORRUPTED;
				goto exit;
            }
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s injecting last free box pos %u size %u\n", finfo->filename, pos, remain));
            data[pos] = (remain>>24) & 0xFF;
            data[pos+1] = (remain>>16) & 0xFF;
            data[pos+2] = (remain>>8) & 0xFF;
            data[pos+3] = (remain) & 0xFF;
            data[pos+4] = 'f';
            data[pos+5] = 'r';
            data[pos+6] = 'e';
            data[pos+7] = 'e';
            remain-=8;
            if (remain>SAFETY_ERASE_BYTES) remain = SAFETY_ERASE_BYTES;
            memset(data+pos+8, 0, remain);

			//we have a previous moof but no mdat header after, consider we completely lost the fragment
			//so reset moof to free and erase mvhd
			if (prev_moof_pos) {
				data[prev_moof_pos+4] = 'f';
				data[prev_moof_pos+5] = 'r';
				data[prev_moof_pos+6] = 'e';
				data[prev_moof_pos+7] = 'e';
				memset(data+prev_moof_pos+8, 0, 16);
				prev_moof_pos = 0;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching last moof (pos %u) to free and erase mvhd\n", finfo->filename, pos, prev_moof_pos));
			}
			goto exit;
        }
		last_box_size = box_size;

        //we missed a box header, insert one at previous pos, indicating a free box !!
        if (pos > prev_pos) {
            u32 missed_size = pos - prev_pos;

			//we might have detected a wrong box
			if (missed_size<8) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] File %s detected box %s at pos %u but not enough bytes since previous box end at pos %u - ignoring and looking further\n", finfo->filename, gf_4cc_to_str(type), pos, prev_pos));
				pos = prev_pos+8;
				continue;
			}
			nb_patches++;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s injecting mid-stream free box between pos %u and pos %u size %u\n", finfo->filename, prev_pos, pos, missed_size));

            data[prev_pos] = (missed_size>>24) & 0xFF;
            data[prev_pos+1] = (missed_size>>16) & 0xFF;
            data[prev_pos+2] = (missed_size>>8) & 0xFF;
            data[prev_pos+3] = (missed_size) & 0xFF;
            data[prev_pos+4] = 'f';
            data[prev_pos+5] = 'r';
            data[prev_pos+6] = 'e';
            data[prev_pos+7] = 'e';
            missed_size -= 8;
            if (missed_size>SAFETY_ERASE_BYTES) missed_size = SAFETY_ERASE_BYTES;
            memset(data+prev_pos+8, 0, missed_size);

			//we have a previous moof but no mdat header anywere, consider we completely lost the fragment
			//so reset moof to free and erase mvhd
			if (prev_moof_pos) {
				data[prev_moof_pos+4] = 'f';
				data[prev_moof_pos+5] = 'r';
				data[prev_moof_pos+6] = 'e';
				data[prev_moof_pos+7] = 'e';
				memset(data+prev_moof_pos+8, 0, 16);
				prev_moof_pos = 0;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching last moof (pos %u) to free and erase mvhd\n", finfo->filename, pos, prev_moof_pos));
			}
        }

        if (type == GF_4CC('f','r','e','e')) {
			//don't check / patch for free
            box_complete = GF_TRUE;
        } else if (type == GF_4CC('m','d','a','t')) {
			if (ctx->repair != ROUTEIN_REPAIR_STRICT) {
				box_complete = GF_TRUE;
			} else {
				is_mdat = GF_TRUE;
				prev_mdat_pos = pos;
			}
		} else if (type == GF_4CC('m','o','o','f')) {
			prev_moof_pos = pos;
		}

		//check if we are indeed in a recevied range
		if (!box_complete) {
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
			//mdat completely received, reset mdat pos and moof (we assume a single mdat per moof)
			if (is_mdat) {
				prev_mdat_pos = 0;
				prev_moof_pos = 0;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s complete box %s pos %u size %u\n", finfo->filename, gf_4cc_to_str(type), pos, box_size));
            pos += box_size;
            continue;
        }
		//incomplete mdat (strict mode), discard previous moof
		if (is_mdat) {
			if (prev_moof_pos) {
				data[prev_moof_pos+4] = 'f';
				data[prev_moof_pos+5] = 'r';
				data[prev_moof_pos+6] = 'e';
				data[prev_moof_pos+7] = 'e';
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching mdat (pos %u size %u) and last moof (pos %u) to free\n", finfo->filename, pos, box_size, prev_moof_pos));
				prev_moof_pos = 0;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching mdat (pos %u size %u) to free\n", finfo->filename, pos, box_size));
			}
			data[pos+4] = 'f';
			data[pos+5] = 'r';
			data[pos+6] = 'e';
			data[pos+7] = 'e';
			nb_patches++;
		} else {
			//incomplete box, move to free and erase begining of payload
			data[pos+4] = 'f';
			data[pos+5] = 'r';
			data[pos+6] = 'e';
			data[pos+7] = 'e';
			nb_patches++;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s erasing incomplete box %s payload - size %u pos %u\n", finfo->filename, gf_4cc_to_str(type), box_size, pos));

			u32 erase_size = box_size-8;
			if (erase_size>SAFETY_ERASE_BYTES) erase_size = SAFETY_ERASE_BYTES;
			memset(data+pos+8, 0, MIN(erase_size, size-pos-8));
		}
        pos += box_size;
    }
    //check if file had no known size and last fragment ends on our last box
    if (!finfo->total_size && (finfo->frags[finfo->nb_frags-1].offset + finfo->frags[finfo->nb_frags-1].size == pos)) {
		if (finfo->nb_frags==1) was_partial = GF_FALSE;
		finfo->total_size = pos;
	}
	//file size unknown, truncate blob to our last box end
	if (!finfo->total_size) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching file size to %u\n", finfo->filename, pos));
		gf_route_dmx_patch_blob_size(ctx->route_dmx, service_id, finfo, pos);
		data = finfo->blob->data;
		size = pos;
		//if prev mdat was not completly received, patch mdat & moof
		if (ctx->repair == ROUTEIN_REPAIR_STRICT) {

			if (prev_moof_pos) {
				data[prev_moof_pos+4] = 'f';
				data[prev_moof_pos+5] = 'r';
				data[prev_moof_pos+6] = 'e';
				data[prev_moof_pos+7] = 'e';
				nb_patches++;
				//moof without mdat, consider the fragment lost
				if (!prev_mdat_pos) {
					memset(data+prev_moof_pos+8, 0, 16);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching last moof (pos %u) to free and erasing\n", finfo->filename, pos, prev_moof_pos));
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching last moof (pos %u) to free\n", finfo->filename, pos, prev_moof_pos));
				}
			}

			if (prev_mdat_pos) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching mdat (pos %u) to free\n", finfo->filename, pos));
				data[prev_mdat_pos+4] = 'f';
				data[prev_mdat_pos+5] = 'r';
				data[prev_mdat_pos+6] = 'e';
				data[prev_mdat_pos+7] = 'e';
				nb_patches++;
			}
		}
	}
    assert(pos == finfo->total_size);

exit:
	if ((ctx->repair == ROUTEIN_REPAIR_STRICT) && was_partial && !nb_patches) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] File %s was partially received but no modifications during fast-repair, please report to GPAC devs\n", finfo->filename));
	}
	if (was_partial) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] File %s fast-repair done (%d patches)\n", finfo->filename, pos));
	}
	//remove corrupted flag
	finfo->partial = GF_LCTO_PARTIAL_NONE;
	gf_route_dmx_patch_frag_info(ctx->route_dmx, service_id, finfo, 0, size);


}

static void route_repair_build_ranges_full(ROUTEInCtx *ctx, RepairSegmentInfo *rsi, GF_ROUTEEventFileInfo *finfo)
{
	u32 i, nb_bytes_ok=0;
	u32 bytes_overlap=0;
	RouteRepairRange *prev_br = NULL;
	//collect decoder stats, or if not found direct output
	if (ctx->opid && !rsi->tsio) {
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
		if (i<finfo->nb_frags)
			nb_bytes_ok += finfo->frags[i].size;

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
		else if (finfo->total_size) {
			br_start = finfo->frags[finfo->nb_frags-1].offset + finfo->frags[finfo->nb_frags-1].size;
			br_end = finfo->total_size;
		}

		//this was correctly received !
		if (br_end <= br_start) continue;
		//merge small byte ranges
		if (prev_br && (prev_br->br_end + ctx->range_merge > br_start)) {
			bytes_overlap += br_start - prev_br->br_end;
			prev_br->br_end = br_end;
			continue;
		}

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
		prev_br = rr;

		gf_list_add(rsi->ranges, rr);
	}
	//we missed the end !!
	if (finfo->total_size==0) {
		u32 br_start = finfo->frags[finfo->nb_frags-1].offset + finfo->frags[finfo->nb_frags-1].size;

		if (prev_br && (prev_br->br_end + ctx->range_merge > br_start)) {
			prev_br->br_end = 0;
			bytes_overlap += br_start - prev_br->br_end;
		} else {

			RouteRepairRange *rr = gf_list_pop_back(ctx->seg_range_reservoir);
			if (!rr) {
				GF_SAFEALLOC(rr, RouteRepairRange);
				if (!rr) {
					rsi->nb_errors++;
					return;
				}
			} else {
				memset(rr, 0, sizeof(RouteRepairRange));
			}
			rr->br_start = br_start;
			rr->br_end = 0;
			gf_list_add(rsi->ranges, rr);
		}
	}
	//no losses - should not happen, frags should be merged by routedmx
	if (!gf_list_count(rsi->ranges)) return;

	if (ctx->minrecv && finfo->total_size && (nb_bytes_ok * 100 < finfo->total_size * ctx->minrecv)) {
		RouteRepairRange *rr;
		while (gf_list_count(rsi->ranges)>1) {
			rr = gf_list_pop_back(rsi->ranges);
			gf_free(rr);
		}
		rr = gf_list_get(rsi->ranges, 0);
		rr->br_start = 0; //we could keep the start ?
		rr->br_end = finfo->total_size;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s to many bytes lost (%u %%), redownloading full file\n", rsi->finfo.filename, (u32) (nb_bytes_ok*100/finfo->total_size) ));
	}
	else if (bytes_overlap) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s merging repair ranges, downloading %u bytes already received\n", rsi->finfo.filename, bytes_overlap));
	}
}

static Bool routein_repair_local(ROUTEInCtx *ctx, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo, Bool start_only)
{
	Bool drop_if_first = GF_FALSE;
	Bool in_transfer = GF_FALSE;
	//remove corrupted flags and set in_transfer to avoid dispatch when using internal cache
	if (finfo->blob->mx) {
		gf_mx_p(finfo->blob->mx);
		finfo->blob->flags &= ~GF_BLOB_CORRUPTED;
		if (finfo->blob->flags & GF_BLOB_IN_TRANSFER) in_transfer = GF_TRUE;
		else finfo->blob->flags |= GF_BLOB_IN_TRANSFER;
	}
	//file received with no losses
	if ((finfo->nb_frags==1) && !finfo->frags[0].offset && (finfo->frags[0].size == finfo->total_size)) {
		if (finfo->blob->mx) {
			finfo->blob->flags &= ~GF_BLOB_IN_TRANSFER;
			gf_mx_v(finfo->blob->mx);
		}
		return GF_FALSE;
	}

	if (strstr(finfo->filename, ".ts") || strstr(finfo->filename, ".m2ts")) {
		drop_if_first = routein_repair_segment_ts_local(ctx, evt_param, finfo, start_only);
	} else {
		routein_repair_segment_isobmf_local(ctx, evt_param, finfo, start_only);
	}

	if (finfo->blob->mx) {
		finfo->blob->flags |= GF_BLOB_PARTIAL_REPAIR;
		if (!in_transfer)
			finfo->blob->flags &= ~GF_BLOB_IN_TRANSFER;
		gf_mx_v(finfo->blob->mx);
	}
	return drop_if_first;
}

void routein_queue_repair(ROUTEInCtx *ctx, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo)
{
	u32 i, count;
	u32 fast_repair = 0;

	//TODO handle late data - keep log = warning as reminder
	if (evt==GF_ROUTE_EVT_LATE_DATA) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Late data patching not yet implemented !\n"));
		return;
	}

	if (ctx->repair==ROUTEIN_REPAIR_NO) {
		//no repair, we only dispatch full files
		if (evt==GF_ROUTE_EVT_DYN_SEG_FRAG) return;

		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		return;
	}
	//not using gcache nor split stsi, we must dispatch only full files
	if (!ctx->gcache && !ctx->stsi && (evt==GF_ROUTE_EVT_DYN_SEG_FRAG))
		return;

	//figure out if we have queued repair on this TSI
	TSI_Output *tsio = routein_get_tsio(ctx, evt_param, finfo);
	Bool can_flush_fragment = ctx->gcache ? GF_TRUE : GF_FALSE;

	if (tsio) {
		if (tsio->delete_first) {
			if (evt==GF_ROUTE_EVT_DYN_SEG_FRAG) {
				return;
			} else if (evt==GF_ROUTE_EVT_DYN_SEG) {
				tsio->delete_first = GF_FALSE;
				return;
			}
		}
		if (!tsio->flags_progress && !gf_list_count(tsio->pending_repairs)) {
			//if not same TOI, do not fast flush fragment (progressive)
			if (!tsio->current_toi || (tsio->current_toi==finfo->toi))
				can_flush_fragment = GF_TRUE;
		}
	}
	if (evt<GF_ROUTE_EVT_DYN_SEG) {
		fast_repair = 0;
	}
	//patch start of file only at tune-in for LL mode, otherwise use regular route repair
	else if (ctx->llmode && !finfo->first_toi_received && (evt==GF_ROUTE_EVT_DYN_SEG_FRAG)
		&& (finfo->partial==GF_LCTO_PARTIAL_ANY)
		&& finfo->nb_frags
		&& finfo->frags[0].offset
		&& !tsio
	) {
		fast_repair = 2;
	}
	//for non-http base repair, check if we can push
	else if (ctx->repair<ROUTEIN_REPAIR_FULL) {
		if (evt==GF_ROUTE_EVT_DYN_SEG_FRAG) {
			//we use cache, we can push
			if (!ctx->stsi || !tsio) {
				routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
				return;
			}
			//TSIO, must dispatch in order
			if (!can_flush_fragment || finfo->frags[0].offset) {
				//remember we have a file in progress so that we don't dispatch packets from following file
				tsio->flags_progress |= TSIO_FILE_PROGRESS;
				return;
			}

			u32 true_size = finfo->frags[0].size;
			//figureout largest safest size from start
			if (strstr(finfo->filename, ".ts") || strstr(finfo->filename, ".m2ts")) {
				finfo->frags[0].size /= 188;
				finfo->frags[0].size *= 188;
			} else {
				u32 btype, bsize, cur_pos=0;
				u32 valid_bytes=0;
				while (1) {
					btype = next_top_level_box(finfo, finfo->blob->data, true_size, &cur_pos, &bsize);
					if (!btype) break;
					if (cur_pos + bsize>true_size) {
						//strict repair, never send a box until completed
						if (ctx->repair==ROUTEIN_REPAIR_STRICT) break;
						if (btype == GF_4CC('m','d','a','t')) {
							valid_bytes = true_size;
						}
						break;
					}
					cur_pos+=bsize;
					if (ctx->repair==ROUTEIN_REPAIR_STRICT) {
						//in strict repair break at moof - if we had a moof+mdat valid, we'll stop at the mdat end
						if (btype != GF_4CC('m','o','o','f')) {
							valid_bytes = cur_pos;
						}
					} else {
						valid_bytes = cur_pos;
					}
				}
				finfo->frags[0].size = valid_bytes;
			}
			routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
			finfo->frags[0].size = true_size;
			//remember we have a file in progress so that we don't dispatch packets from following file
			tsio->flags_progress |= TSIO_FILE_PROGRESS;
			return;
		}
		fast_repair = 1;
	}

	//nothing pending
	if (!finfo->partial && can_flush_fragment) {
		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		return;
	}
	//no tsio and no errors, we can forward
	else if (!tsio && !finfo->partial) {
		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		return;
	}
	//fast repair and we can flush
	if (fast_repair && can_flush_fragment) {
		Bool drop_if_first = routein_repair_local(ctx, evt, evt_param, finfo, (fast_repair==2) ? GF_TRUE : GF_FALSE);
		routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, drop_if_first);
		return;
	}

	//for now we only trigger file repair
	if (evt==GF_ROUTE_EVT_DYN_SEG_FRAG) {
		if (!tsio)
			routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		//in stsi mode no cache with http repair (other modes are tested above):
		// push fragment if no pending and data is contiguous with prev
		//this ensures we push data asap in the output pids
		else if ((finfo->partial!=GF_LCTO_PARTIAL_ANY) && can_flush_fragment) {
			routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		} else {
			//remember we have a file in progress so that we don't dispatch packets from following file
			tsio->flags_progress |= TSIO_FILE_PROGRESS;
		}
		return;
	}

	//check if not already queued (for when we will allow repair mid-segments)
	count = gf_list_count(ctx->seg_repair_queue);
	for (i=0; i<count; i++) {
		RepairSegmentInfo *rsi = gf_list_get(ctx->seg_repair_queue, i);
		if (!rsi->done && (rsi->finfo.tsi==finfo->tsi) && (rsi->finfo.toi==finfo->toi)) {
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Failed to allocate repair entry %s (TSI=%u, TOI=%u)\n", finfo->filename, finfo->tsi, finfo->toi));
		if (tsio) tsio->flags_progress &= ~ TSIO_FILE_PROGRESS;

		//do a local file repair
		if (can_flush_fragment && (evt==GF_ROUTE_EVT_DYN_SEG)) {
			u32 repair = ctx->repair;
			ctx->repair = ROUTEIN_REPAIR_STRICT;
			routein_repair_local(ctx, evt, evt_param, finfo, GF_FALSE);
			ctx->repair = repair;

			routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
		}
		return;
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Queue repair for object %s (TSI=%u, TOI=%u)\n", finfo->filename, finfo->tsi, finfo->toi));

	rsi->evt = evt;
	rsi->service_id = evt_param;
	rsi->finfo = *finfo;
	rsi->filename = gf_strdup(finfo->filename);
	rsi->finfo.filename = rsi->filename;
	rsi->tsio = tsio;
	rsi->local_repair = fast_repair;

	if (finfo->partial && !fast_repair)
		route_repair_build_ranges_full(ctx, rsi, finfo);

	if (rsi->tsio && (rsi->tsio->current_toi==finfo->toi)) {
		gf_assert(finfo->frags[0].offset==0 || !rsi->tsio->bytes_sent);
		gf_assert(rsi->tsio->bytes_sent<=finfo->frags[0].size);
	}

	//mark blob as in transfer and corrupted - this will be unmarked when doing the final dispatch
	gf_mx_p(finfo->blob->mx);
	finfo->blob->flags |= GF_BLOB_IN_TRANSFER|GF_BLOB_CORRUPTED;
	gf_mx_v(finfo->blob->mx);
	if (rsi->tsio) {
		if (rsi->tsio->current_toi==finfo->toi) {
			gf_assert(finfo->frags[0].offset==0 || !rsi->tsio->bytes_sent);
			gf_assert(rsi->tsio->bytes_sent<=finfo->frags[0].size);
		}
		rsi->tsio->flags_progress &= ~ TSIO_FILE_PROGRESS;
		rsi->tsio->flags_progress |= TSIO_REPAIR_SCHEDULED;

		gf_route_dmx_force_keep_object(ctx->route_dmx, rsi->service_id, finfo->tsi, finfo->toi, GF_TRUE);
		//inject by start time
		Bool found=GF_FALSE;
		count = gf_list_count(rsi->tsio->pending_repairs);
		for (i=0; i<count; i++) {
			RepairSegmentInfo *a_rsi = gf_list_get(rsi->tsio->pending_repairs, i);
			if (finfo->start_time < a_rsi->finfo.start_time) {
				gf_list_insert(rsi->tsio->pending_repairs, rsi, i);
				found = GF_TRUE;
				break;
			}
		}
		if (!found)
			gf_list_add(rsi->tsio->pending_repairs, rsi);
	}

	if (!ctx->seg_repair_queue)
		ctx->seg_repair_queue = gf_list_new();

	//inject by start time
	count = gf_list_count(ctx->seg_repair_queue);
	for (i=0; i<count; i++) {
		RepairSegmentInfo *a_rsi = gf_list_get(ctx->seg_repair_queue, i);
		if (finfo->start_time < a_rsi->finfo.start_time) {
			gf_list_insert(ctx->seg_repair_queue, rsi, i);
			return;
		}
	}
	gf_list_add(ctx->seg_repair_queue, rsi);
}


static void repair_session_dequeue(ROUTEInCtx *ctx, RouteRepairSession *rsess, RepairSegmentInfo *rsi)
{
	Bool unprotect;
	TSI_Output *tsio;

restart:
	unprotect=GF_FALSE;
	tsio = rsi->tsio;

	//done with this repair, remove force_keep flag from object
	if (tsio) {
		s32 idx = gf_list_find(tsio->pending_repairs, rsi);
		assert(idx == 0);
		assert (!tsio->current_toi || (tsio->current_toi == rsi->finfo.toi));
		unprotect = GF_TRUE;
	}
	//remove before calling route_on_event, as it may trigger a delete
	gf_list_del_item(ctx->seg_repair_queue, rsi);


	if (!rsi->removed) {
		//do a local file repair if we still have errors ?
		if (rsi->nb_errors && (rsi->evt==GF_ROUTE_EVT_DYN_SEG)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] File %s still has error, doing a final local repair\n", rsi->finfo.filename));
			u32 repair = ctx->repair;
			ctx->repair = ROUTEIN_REPAIR_STRICT;
			routein_repair_local(ctx, rsi->evt, rsi->service_id, &rsi->finfo, GF_FALSE);
			ctx->repair = repair;
			rsi->nb_errors = 0;
		}

		//flush
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repair done for object %s (TSI=%u, TOI=%u)%s\n", rsi->finfo.filename, rsi->finfo.tsi, rsi->finfo.toi, rsi->nb_errors ? " - errors remain" : ""));

		if (rsi->evt==GF_ROUTE_EVT_DYN_SEG) {
			gf_assert(rsi->finfo.nb_frags == 1);
			gf_assert(rsi->finfo.frags[0].offset == 0);
			gf_assert(rsi->finfo.frags[0].size == rsi->finfo.blob->size);
		}
		routein_on_event_file(ctx, rsi->evt, rsi->service_id, &rsi->finfo, GF_TRUE, GF_FALSE);
	}

	//done with this repair, remove force_keep flag from object
	if (unprotect) {
		gf_route_dmx_force_keep_object(ctx->route_dmx, rsi->service_id, rsi->finfo.tsi, rsi->finfo.toi, GF_FALSE);
		gf_list_del_item(tsio->pending_repairs, rsi);
		if (!gf_list_count(tsio->pending_repairs))
			tsio->flags_progress &= ~TSIO_REPAIR_SCHEDULED;
	}

	GF_List *bck = rsi->ranges;
	if (rsi->filename) gf_free(rsi->filename);
	memset(rsi, 0, sizeof(RepairSegmentInfo));
	rsi->ranges = bck;
	gf_list_add(ctx->seg_repair_reservoir, rsi);


	if (!tsio) return;
	//get next in list, if existing and done or if removed, dequeue
	rsi = gf_list_get(tsio->pending_repairs, 0);
	if (rsi && (rsi->done || (rsi->removed && !rsi->pending)))
		goto restart;
}

static void repair_session_done(ROUTEInCtx *ctx, RouteRepairSession *rsess, GF_Err res_code)
{
	RepairSegmentInfo *rsi = rsess->current_si;
	if (!rsi) return;

	if (rsess->range) {
		//notify routedmx we have received a byte range
		if (!rsi->removed) {
			gf_route_dmx_patch_frag_info(ctx->route_dmx, rsi->service_id, &rsi->finfo, rsess->range->br_start, rsess->range->br_end);
		}

		rsess->server->nb_bytes += rsess->range->done;
		rsess->server->nb_req_success += (rsess->range->done==rsess->range->br_end-rsess->range->br_start)?1:0;

		gf_list_add(ctx->seg_range_reservoir, rsess->range);

		rsess->server->accept_ranges = RANGE_SUPPORT_YES;
	}

	rsess->initial_retry = 0;
	//always reset even if we have pending byte ranges, so that each repair session fetches the most urgent download
	rsess->current_si = NULL;
	rsess->range = NULL;
	if (res_code<0) rsi->nb_errors++;

	gf_assert(rsi->pending);
	rsi->pending--;
	if (rsi->pending) return;

	if (rsi->removed) {
		gf_list_transfer(ctx->seg_range_reservoir, rsi->ranges);
	}

	if (gf_list_count(rsi->ranges)) {
		//if TSIO notify ranges when received
		if (rsi->tsio && !rsi->nb_errors && !rsi->finfo.frags[0].offset &&
			((rsi->evt==GF_ROUTE_EVT_DYN_SEG_FRAG) || (rsi->evt==GF_ROUTE_EVT_DYN_SEG))
		) {
			gf_mx_p(rsi->finfo.blob->mx);
			GF_LCTObjectPartial partial = rsi->finfo.partial;
			u32 blob_flags = rsi->finfo.blob->flags;
			rsi->finfo.partial = GF_LCTO_PARTIAL_NONE;
			rsi->finfo.blob->flags = GF_BLOB_IN_TRANSFER;
			//notify as DYN_SEG_FRAG not as DYN_SEG
			routein_on_event_file(ctx, GF_ROUTE_EVT_DYN_SEG_FRAG, rsi->service_id, &rsi->finfo, GF_TRUE, GF_FALSE);

			rsi->finfo.partial = partial;
			rsi->finfo.blob->flags = blob_flags;
			gf_mx_v(rsi->finfo.blob->mx);
		}
		return;
	}
	gf_mx_p(rsi->finfo.blob->mx);
	if (!rsi->nb_errors) {
		rsi->finfo.partial = GF_LCTO_PARTIAL_NONE;
		rsi->finfo.blob->flags &= ~GF_BLOB_CORRUPTED;
	}
	//not a fragment, remove in-transfer flag
	if (rsi->evt!=GF_ROUTE_EVT_DYN_SEG_FRAG)
		rsi->finfo.blob->flags &= ~GF_BLOB_IN_TRANSFER;
	gf_mx_v(rsi->finfo.blob->mx);

	if (rsi->local_repair) {
		routein_repair_local(ctx, rsi->evt, rsi->service_id, &rsi->finfo, GF_FALSE);
		rsi->local_repair = GF_FALSE;
	}

	//make sure we dequeue in order
	if (rsi->tsio) {
		s32 idx = gf_list_find(rsi->tsio->pending_repairs, rsi);
		//not first in list, do not dequeue now
		//this happens if multiple repair sessions are activated, they will likely not finish at the same time
		if (idx > 0) {
			rsi->done = GF_TRUE;
			return;
		}
	}
	repair_session_dequeue(ctx, rsess, rsi);
}

static void repair_session_run(ROUTEInCtx *ctx, RouteRepairSession *rsess)
{
	GF_Err e;
	RepairSegmentInfo *rsi;
	u32 offset, nb_read;

restart:
	rsi = rsess->current_si;
	if (!rsi) {
		RouteRepairRange *rr = NULL;
		u32 i, count;
		RouteRepairServer* repair_server = NULL;
		char *url = NULL;

		if (rsess->retry_in && (gf_sys_clock()<rsess->retry_in)) return;
		rsess->retry_in = 0;

		count = gf_list_count(ctx->seg_repair_queue);
		for (i=0; i<count;i++) {
			u32 j, nb_ranges;
			rsi = gf_list_get(ctx->seg_repair_queue, i);
			//over or no longer active
			if (rsi->done || rsi->removed) continue;

			nb_ranges = gf_list_count(rsi->ranges);
			//no more ranges, done with session
			//this happens when enqueued repair had no losses when using progressive dispatch
			if (!nb_ranges) {
				rsess->current_si = rsi;
				rsi->pending++;
				repair_session_done(ctx, rsess, GF_OK);
				rsess->current_si = NULL;
				goto restart;
			}
			//if TSIO, always dequeue in order
			if (rsi->tsio) {
				rr = gf_list_get(rsi->ranges, 0);
			} else {
				for (j=0; j<nb_ranges; j++) {
					rr = gf_list_get(rsi->ranges, j);
					//todo check priority
					if (rr) break;
				}
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

		for (i=0; i< gf_list_count(ctx->repair_servers); i++) {
			repair_server = gf_list_get(ctx->repair_servers, i);
			if (repair_server->url && repair_server->accept_ranges && repair_server->is_up) {
				url = gf_url_concatenate(repair_server->url, rsi->finfo.filename);
				break;
			}
		}

		if (!url) {
			//TODO: do a full patch using the entire file ?
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] Failed to find an adequate repair server for %s - Repair abort \n", rsi->finfo.filename));
			rsi->nb_errors++;
			repair_session_done(ctx, rsess, e);
			return;
		}

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
		rsess->server = repair_server;
		if (rsess->range->br_end) {
			gf_dm_sess_set_range(rsess->dld, rsess->range->br_start, rsess->range->br_end-1, GF_TRUE);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] Queue request for %s byte range %u-%u\n", url, rsess->range->br_start, rsess->range->br_end-1));
		} else {
			gf_dm_sess_set_range(rsess->dld, rsess->range->br_start, 0, GF_TRUE);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] Queue request for %s byte range %u-\n", url, rsess->range->br_start));
		}
		gf_free(url);
		ctx->has_data = GF_TRUE;
	}

refetch:
	offset = rsess->range->br_start + rsess->range->done;
	nb_read=0;
	if (rsi->removed) {
		e = GF_URL_REMOVED;
		gf_dm_sess_abort(rsess->dld);
	} else {
		e = gf_dm_sess_fetch_data(rsess->dld, rsess->http_buf, REPAIR_BUF_SIZE, &nb_read);
		if (e==GF_IP_NETWORK_EMPTY) return;
		if (e==GF_IO_BYTE_RANGE_NOT_SUPPORTED) {
			u32 now = gf_sys_clock();
			u32 error_type = 0;
			//it is quite possible that the server replied 200 on an open end-range request when the segment size is unknown
			if (!rsess->range->br_end) {
				u32 filesize = gf_dm_sess_get_resource_size(rsess->dld);
				//in case we lost a FDT with content-length=0
				if (filesize==rsess->range->br_start) {
					error_type = 1;
				} else {
					//200 instead of 206 on server supporting byte-ranges: the content size is not yet known, cancel and postpone
					//note: we're either in probe mode or know the server accepts ranges so we can't figure out if the server doesn't accept
					//by postponing we should get an answer soon...
					error_type = 2;
				}
			} else if (rsess->server->accept_ranges==RANGE_SUPPORT_PROBE) {
				rsess->server->accept_ranges = RANGE_SUPPORT_NO;
			} else {
				//200 instead of 206 on server supporting byte-ranges: the content size is not yet known, cancel and postpone
				error_type = 2;
			}
			if (error_type==2) {
				if (! rsess->initial_retry) rsess->initial_retry = now;
				else if (rsess->initial_retry + 1000 < now) {
					error_type = 3;
				}
			}

			if (e) {
				if (!error_type) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] Server \"%s\" does not support byte range requests: Server is blacklisted for partial repair \n", rsess->server->url));
				}
				gf_dm_sess_abort(rsess->dld);
				gf_dm_sess_del(rsess->dld);
				rsess->dld = NULL;
				rsi->pending--;
				rsess->current_si = NULL;
				if ((error_type==1) || (error_type==3)) {
					gf_list_add(ctx->seg_range_reservoir, rsess->range);
					if (error_type==3) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[REPAIR] Server \"%s\" failed to give byte range after %u ms, aborting repair\n", rsess->server->url, now - rsess->initial_retry));
						rsess->current_si->nb_errors++;
						rsess->initial_retry = 0;
					}
				} else {
					//try with another server if there is one
					gf_list_add(rsi->ranges, rsess->range);
				}
				rsess->range = NULL;
				if (error_type==2) {
					rsess->retry_in = gf_sys_clock() + 100;
					return;
				}
				goto restart;
			}
		}
	}

	//open end range
	if (nb_read && (e>=GF_OK) && !rsess->range->br_end) {
		rsess->range->br_end = gf_dm_sess_get_resource_size(rsess->dld);
		if (!rsess->range->br_end || (rsess->range->br_end<rsess->current_si->finfo.blob->size)) {
			e = GF_REMOTE_SERVICE_ERROR;
		} else {
			e = gf_route_dmx_patch_blob_size(ctx->route_dmx, rsess->current_si->service_id, &rsess->current_si->finfo, rsess->range->br_end);
		}
	}
	if (offset + nb_read > rsess->range->br_end)
		e = GF_REMOTE_SERVICE_ERROR;

	if (nb_read && (e>=GF_OK)) {
		gf_mx_p(rsi->finfo.blob->mx);
		memcpy(rsi->finfo.blob->data + offset, rsess->http_buf, nb_read);
		gf_mx_v(rsi->finfo.blob->mx);
		rsess->range->done += nb_read;
		ctx->has_data = GF_TRUE;
		//for now, fragment info is updated once the session is done
		//flush buffer until network empty
		if (e==GF_OK) goto refetch;
	}
	if (e==GF_OK) return;

	repair_session_done(ctx, rsess, e);
	if (e<0) return;

	goto restart;

}

GF_Err routein_do_repair(ROUTEInCtx *ctx)
{
	u32 i, nb_active=0;
	ctx->has_data = GF_FALSE;
	for (i=0; i<ctx->max_sess; i++) {
		RouteRepairSession *rsess = &ctx->http_repair_sessions[i];
		repair_session_run(ctx, rsess);
		if (rsess->current_si) nb_active++;
	}
	if (!nb_active) return GF_EOS;
	if (ctx->has_data) return GF_OK;
	return GF_IP_NETWORK_EMPTY;
}


void routein_repair_mark_file(ROUTEInCtx *ctx, u32 service_id, const char *filename, Bool is_delete)
{
	u32 i, count = gf_list_count(ctx->seg_repair_queue);
	for (i=0; i<count; i++) {
		RepairSegmentInfo *rsi = gf_list_get(ctx->seg_repair_queue, i);
		if (!rsi->done && (rsi->service_id==service_id) && !strcmp(rsi->finfo.filename, filename)) {
			//we don't cancel sessions now, this should be done in session_done
			if (is_delete) {
				//log is set as warning for now as this is work in progress
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] Repair canceled for object %s (TSI=%u, TOI=%u)\n", rsi->finfo.filename, rsi->finfo.tsi, rsi->finfo.toi));
				rsi->removed = GF_TRUE;
			} else {
				//TODO: decide if we need to be more agressive ?
			}
			return;
		}
	}
}


#endif /* GPAC_DISABLE_ROUTE */
