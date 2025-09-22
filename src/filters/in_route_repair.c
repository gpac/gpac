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
#include <gpac/mpegts.h>
#include <gpac/isomedia.h>
#include "../utils/downloader.h"

#ifndef GPAC_DISABLE_ROUTE

static void repair_session_dequeue(ROUTEInCtx *ctx, RepairSegmentInfo *rsi);

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

static u32 next_top_level_box(GF_ROUTEEventFileInfo *finfo, u8 *data, u32 size, Bool check_start, u32 *cur_pos, u32 *box_size)
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
			if (check_start) return 0;
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
	u32 partial_status = GF_LCTO_PARTIAL_NONE;
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

        u32 type = next_top_level_box(finfo, data, size, GF_FALSE, &pos, &box_size);
        //no more top-level found, patch from current pos until end of payload
        if (!type) {
			//first top-level not present in first range to repair, wa cannot patch now
			if (!pos && repair_start_only) {
				return;
			}
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

				gf_assert(finfo->total_size);
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
            //in case the whole file was lost
            if (!finfo->total_size) {
				gf_route_dmx_patch_blob_size(ctx->route_dmx, service_id, finfo, pos+remain);
				size = prev_pos+last_box_size;
				data = finfo->blob->data;
			}
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
			gf_assert(finfo->total_size);
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
			u32 max_s = MAX(size, finfo->total_size);
			if (pos+box_size>max_s) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s patching mdat size from %u to %u (truncated file)\n", finfo->filename, pos, box_size, max_s - pos));
				box_size = max_s - pos;
				data[pos] = (box_size>>24) & 0xFF;
				data[pos+1] = (box_size>>16) & 0xFF;
				data[pos+2] = (box_size>>8) & 0xFF;
				data[pos+3] = (box_size) & 0xFF;
			}
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

	if (patch_first_range_size) {
		return;
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
	if (pos>finfo->total_size) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] File %s is corrupted, invalid last top-level position %u vs size %u\n", finfo->filename, pos, finfo->total_size));
		partial_status = GF_LCTO_PARTIAL_ANY;
		was_partial = GF_FALSE; //suppress logs below
	} else {
		gf_assert(pos == finfo->total_size);
	}

exit:
	if ((ctx->repair == ROUTEIN_REPAIR_STRICT) && was_partial && !nb_patches) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] File %s was partially received but no modifications during fast-repair, please report to GPAC devs\n", finfo->filename));
	}
	if (was_partial) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] File %s fast-repair done (%d patches)\n", finfo->filename, nb_patches));
	}
	//remove corrupted flag
	finfo->partial = partial_status;

	//in gcache mode, we keep ranges in order to identify corrupted ISOBMF samples after repair - whether they are dispatched is governed by keepc option of isobmf demuxer
	//in other modes (dispatch to PID or write to file) we currently reset the info
	//WARNING: if removing this, asserts in routein_send_file will need to be removed
	if (!ctx->gcache) {
		gf_route_dmx_patch_frag_info(ctx->route_dmx, service_id, finfo, 0, finfo->total_size);
		gf_assert(finfo->nb_frags == 1);
	}
}

static RouteRepairRange *queue_repair_range(ROUTEInCtx *ctx, RepairSegmentInfo *rsi, u32 start_range, u32 end_range)
{
	RouteRepairRange *rr = gf_list_pop_back(ctx->seg_range_reservoir);
	if (!rr) {
		GF_SAFEALLOC(rr, RouteRepairRange);
		if (!rr) {
			rsi->nb_errors++;
			return NULL;
		}
	} else {
		memset(rr, 0, sizeof(RouteRepairRange));
	}
	rr->br_start = start_range;
	rr->br_end = end_range;
	if (end_range)
		gf_assert(rr->br_end >= rr->br_start);
	gf_list_add(rsi->ranges, rr);
	return rr;
}

static void route_repair_build_ranges_full(ROUTEInCtx *ctx, RepairSegmentInfo *rsi, GF_ROUTEEventFileInfo *finfo)
{
	u32 i, nb_bytes_ok=0;
	u32 bytes_overlap=0;
	RouteRepairRange *prev_br = NULL;

	//unused for now - goal is to adjust range priorities based on upper-chain buffer levels
#if 0
	//collect decoder stats, or if not found direct output
	if (ctx->opid && !rsi->tsio) {
		GF_FilterPidStatistics stats;
		GF_Err e = gf_filter_pid_get_statistics(ctx->opid, &stats, GF_STATS_DECODER_SINK);
		if (e) e = gf_filter_pid_get_statistics(ctx->opid, &stats, GF_STATS_SINK);
		if (!e) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] Repairing segment %s - buffer status: %d ms (%u for rebuffer %u max buffer)\n", finfo->filename, (u32) (stats.buffer_time/1000) , (u32) (stats.min_playout_time/1000), (u32) (stats.max_buffer_time/1000) ));
		}
	}
#endif

	//we missed the whole file !
	if (!finfo->nb_frags) {
		queue_repair_range(ctx, rsi, 0, 0);
		return;
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

		RouteRepairRange *rr = queue_repair_range(ctx, rsi, br_start, br_end);
		if (!rr) continue;
		prev_br = rr;
	}
	//we missed the end !!
	if (finfo->total_size==0) {
		u32 br_start = finfo->frags[finfo->nb_frags-1].offset + finfo->frags[finfo->nb_frags-1].size;

		if (prev_br && (prev_br->br_end + ctx->range_merge > br_start)) {
			prev_br->br_end = 0;
			bytes_overlap += br_start - prev_br->br_end;
		} else {
			RouteRepairRange *rr = queue_repair_range(ctx, rsi, br_start, 0);
			if (!rr) return;
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
		memset(rr, 0, sizeof(RouteRepairRange));
		rr->br_start = 0; //we could keep the start ?
		rr->br_end = finfo->total_size;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s to many bytes lost (%u %%), redownloading full file\n", rsi->finfo.filename, (u32) (nb_bytes_ok*100/finfo->total_size) ));
	}
	else if (bytes_overlap) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] File %s merging repair ranges, downloading %u bytes already received\n", rsi->finfo.filename, bytes_overlap));
	}
}

static u32 get_dependent_samples(GF_List *sample_deps, SampleDepInfo *for_sample, SampleDepInfo *ref_sample, GF_List *done)
{
	u32 nb_deps, i, j, count = gf_list_count(sample_deps);

	if (gf_list_find(done, for_sample)>=0) return 0;
	gf_list_add(done, for_sample);
	if (ref_sample==for_sample)
		ref_sample->num_direct_dependencies = 0;

	nb_deps = 0;
	for (i=0;i<count;i++) {
		SampleDepInfo *sd = gf_list_get(sample_deps, i);
		if (sd == for_sample) continue;
		if (sd->drop) continue;
		if (sd->gop_id != ref_sample->gop_id) continue;
		for (j=0; j<sd->nb_refs; j++) {
			if (sd->refs[j] == for_sample->sample_id) {
				if (ref_sample==for_sample)
					ref_sample->num_direct_dependencies ++;
				if (gf_list_find(done, sd)>=0) continue;
				nb_deps++;
				nb_deps += get_dependent_samples(sample_deps, sd, ref_sample, done);
			}
		}
	}
	return nb_deps;
}

static void update_num_dependent_samples(GF_List *sample_deps, SampleDepInfo *for_sample)
{
	GF_List *done = gf_list_new();
	for_sample->num_indirect_dependencies = get_dependent_samples(sample_deps, for_sample, for_sample, done);
	gf_list_del(done);
}

static void routein_repair_get_isobmf_deps(ROUTEInCtx *ctx, RepairSegmentInfo *rsi)
{
	GF_ISOFile *file;
	u64 BytesMissing;
	u32 i;
	char szBlobPath[100];

	GF_Err e = gf_isom_open_progressive_ex("isobmff://4cc=none", 0, 0, 0, &file, &BytesMissing, NULL);
	if (e) return;

	sprintf(szBlobPath, "gmem://%p", rsi->finfo.blob);
	e = gf_isom_open_segment(file, szBlobPath, 0, 0, 0);
	if (e) {
		gf_isom_delete(file);
		return;
	}
	u32 ID, nb_refs, count;
	const u32 *refs;
	count = gf_isom_get_sample_count(file, 1);
	if (gf_isom_get_sample_references(file, 1, 1, &ID, &nb_refs, &refs)!=GF_OK) {
		gf_isom_delete(file);
		return;
	}

	if (!rsi->sample_deps) rsi->sample_deps = gf_list_new();

	GF_ISOSample static_sample;
	u32 gop_id=0;
	rsi->max_dep_per_sample = 0;

	for (i=0; i<count; i++) {
		u64 offset;
		SampleDepInfo *r = gf_list_pop_back(ctx->sample_deps_reservoir);
		if (!r) {
			GF_SAFEALLOC(r, SampleDepInfo);
			if (!r) continue;
		} else {
			u32 *refs = r->refs;
			u32 alloc_refs = r->nb_refs_alloc;
			memset(r, 0, sizeof(SampleDepInfo));
			r->nb_refs_alloc = alloc_refs;
			r->refs = refs;
		}
		gf_list_add(rsi->sample_deps, r);

		memset(&static_sample, 0, sizeof(static_sample));
		GF_ISOSample *samp = gf_isom_get_sample_info_ex(file, 1, i+1, NULL, &offset, &static_sample);
		if (!samp) break;
		r->start_range = offset;
		r->end_range = offset + samp->dataLength;
		r->dts = samp->DTS+i;

		gf_isom_get_sample_references(file, 1, i+1, &r->sample_id, (u32 *) &r->nb_refs, &refs);
		if (!r->nb_refs) gop_id ++;
		r->gop_id = gop_id;
		if (r->sample_id==0xFFFFFFFF) {
			r->nb_refs = -1;
			continue;
		}
		if (refs && (r->nb_refs>0)) {
			if (r->nb_refs_alloc < r->nb_refs) {
				r->refs = gf_realloc(r->refs, sizeof(u32)*r->nb_refs);
				r->nb_refs_alloc = r->nb_refs;
				if (!r->refs) {
					r->nb_refs = -1;
					continue;
				}
			}
			memcpy(r->refs, refs, sizeof(u32)*r->nb_refs);
		}
	}
	gf_isom_delete(file);

	for (i=0; i<count; i++) {
		SampleDepInfo *r = gf_list_get(rsi->sample_deps, i);
		update_num_dependent_samples(rsi->sample_deps, r);
		if (r->num_direct_dependencies > rsi->max_dep_per_sample)
			rsi->max_dep_per_sample = r->num_direct_dependencies;
	}
	//sort by decreasing number of indirect dependencies - the first items will be the most important ones to fix
	GF_List *res = gf_list_new();
	for (i=0; i<count; i++) {
		SampleDepInfo *r = gf_list_get(rsi->sample_deps, i);
		u32 j, scount = gf_list_count(res);
		for (j=0; j<scount; j++) {
			SampleDepInfo *elt = gf_list_get(res, j);
			//note that we currently don't sort by gops, so if 2 gops in the segment the intra from both GOPs will be patched first
			//we could further optimize this especially in playback modes
			if (elt->nb_refs > r->nb_refs) {
				gf_list_insert(res, r, j);
				r = NULL;
				break;
			}
			if (elt->num_direct_dependencies < r->num_direct_dependencies) {
				gf_list_insert(res, r, j);
				r = NULL;
				break;
			}
		}
		if (r)
			gf_list_add(res, r);
	}
	gf_list_del(rsi->sample_deps);
	rsi->sample_deps = res;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] Sample dependencies:\n"));
	for (i=0; i<count; i++) {
		SampleDepInfo *r = gf_list_get(rsi->sample_deps, i);
		u32 j;
		for (j=0; j<rsi->finfo.nb_frags; j++) {
			GF_LCTFragInfo *frag = &rsi->finfo.frags[j];
			if ((r->start_range>=frag->offset) && (r->end_range<=frag->offset+frag->size)) {
				r->valid = 1;
				break;
			}
			if (r->end_range<frag->offset) break;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] Sample %d (GID %u) has %u DD %u ID (%u refs) - complete %d\n", r->sample_id, r->gop_id, r->num_direct_dependencies, r->num_indirect_dependencies, r->nb_refs, r->valid ));
	}
	return;
}

static void drop_sample_ref(GF_List *sample_deps, SampleDepInfo *ref, Bool primary)
{
	u32 i, j, count;
	if (ref->drop) return;

	count = gf_list_count(sample_deps);
	ref->drop |= SAMPLE_DROP;
	if (!primary) ref->drop |= SAMPLE_DROP_DEP;
	for (i=0; i<count; i++) {
		SampleDepInfo *sdi = gf_list_get(sample_deps, i);
		if (sdi->gop_id != ref->gop_id) continue;
		for (j=0; j<sdi->nb_refs; j++) {
			if (sdi->refs[j] == ref->sample_id) {
				drop_sample_ref(sample_deps, sdi, GF_FALSE);
				break;
			}
		}
	}
}

GF_Err gf_route_dmx_add_frag_hole(GF_ROUTEDmx *routedmx, u32 service_id, GF_ROUTEEventFileInfo *finfo, u32 br_start, u32 br_size);

static void route_repair_build_ranges_isobmf(ROUTEInCtx *ctx, RepairSegmentInfo *rsi, GF_ROUTEEventFileInfo *finfo)
{
	u32 i, count;
	Bool use_repair = (ctx->repair==ROUTEIN_REPAIR_FULL) ? GF_TRUE : GF_FALSE;

	if ((finfo->nb_frags==1) && !finfo->frags[0].offset && (finfo->frags[0].size==finfo->total_size)) {
		rsi->isox_state = REPAIR_ISO_STATUS_DONE;
		return;
	}
	//we failed at repairing something, abort
	if (rsi->nb_errors) {
		rsi->isox_state = REPAIR_ISO_STATUS_DONE;
		return;
	}

	if (!finfo->nb_frags) {
		if (use_repair && finfo->total_size) {
			queue_repair_range(ctx, rsi, 0, finfo->total_size);
		} else {
			rsi->nb_errors++;
		}
		rsi->isox_state = REPAIR_ISO_STATUS_DONE;
		return;
	}

	if ((rsi->isox_state==REPAIR_ISO_STATUS_INIT) && ctx->minrecv && finfo->total_size && use_repair) {
		u32 nb_bytes_ok=0;
		for (i=0; i<finfo->nb_frags; i++) {
			nb_bytes_ok += finfo->frags[i].size;
		}
		if (nb_bytes_ok * 100 < finfo->total_size * ctx->minrecv) {
			u32 brstart = (finfo->frags[0].offset==0) ? finfo->frags[0].size : 0;
			queue_repair_range(ctx, rsi, brstart, finfo->total_size);
			rsi->isox_state = REPAIR_ISO_STATUS_DONE;
			return;
		}
	}
	if (rsi->isox_state <= REPAIR_ISO_STATUS_PATCH_TOP_LEVEL) {
		u32 patch_start=0;
		u32 patch_box_size=0;
		u32 patch_box_type=0;
		u32 pos=0;
		u32 prev_bpos=0;
		u32 prev_btype=0;
		u32 prev_bsize=0;
		u8 *data = finfo->blob->data;
		u32 size = finfo->blob->size;

		while (pos < size) {
			u32 bsize, j;
			u32 btype = next_top_level_box(finfo, data, size, GF_TRUE, &pos, &bsize);
			//no valid bytes to read box header, need to patch previous box end or iniial range
			if (!btype) {
				if (!pos) {
					gf_assert(finfo->frags[0].offset || (finfo->frags[0].size<8));
					patch_box_size = finfo->frags[0].offset;
					if (patch_box_size>500) patch_box_size = 500;
					break;
				}
				gf_assert(prev_btype);
				gf_assert(prev_bsize);
				patch_start = prev_bpos;
				patch_box_size = prev_bsize;
				patch_box_type = prev_btype;
				break;
			}
			prev_btype = btype;
			prev_bpos = pos;
			prev_bsize = bsize;
			//box is mdat/idat, go to next
			if ((btype==GF_4CC('m','d','a','t')) || (btype==GF_4CC('i','d','a','t'))) {
				pos += bsize;
				continue;
			}
			//check we have the full box
			Bool box_complete = GF_FALSE;
			for (j=0; j<finfo->nb_frags; j++) {
				GF_LCTFragInfo *frag = &finfo->frags[j];

/*
				if (patch_start && (patch_start+patch_box_size>=frag->offset) && (patch_start+patch_box_size+8<=frag->offset+frag->size)) {
					next_box_valid = GF_TRUE;
					break;
				}
*/
				//not in a valid range
				if (pos<frag->offset) continue;
				//box is in a received range
				if ((pos >= frag->offset) && (pos + bsize <= frag->offset + frag->size)) {
					box_complete = GF_TRUE;
					break;
				}
				if (pos>frag->offset+frag->size) continue;

				//remember patch start (end of fragment)
				if (pos >= frag->offset) {
					patch_start = pos;
					patch_box_size = bsize;
					patch_box_type = btype;
					//break;
				}
			}
			if (!box_complete) break;
			pos += bsize;
			continue;
		}
		if (patch_box_size) {
			u32 patch_end = 0;
			//mdat, patch end of box only
			if ((patch_box_type==GF_4CC('m','d','a','t')) || (patch_box_type==GF_4CC('i','d','a','t'))) {
				patch_start += patch_box_size;
				patch_end = patch_start + 500;
			} else {
				patch_end = patch_start + patch_box_size + 16;
			}
			//issue a single byte range
			for (i=0; i<finfo->nb_frags; i++) {
				GF_LCTFragInfo *frag = &finfo->frags[i];
				if ((patch_start>=frag->offset) && (patch_start<frag->offset+frag->size))
					patch_start = frag->offset+frag->size;

				if ((patch_start>=frag->offset+frag->size) && (patch_start<frag->offset+frag->size+1500))
					patch_start = frag->offset+frag->size;

				if ((patch_end>=frag->offset) && (patch_end<frag->offset+frag->size)) {
					patch_end = frag->offset;
					break;
				}
				if ((patch_end<frag->offset) && (patch_end+1500>=frag->offset)) {
					patch_end = frag->offset;
					break;
				}
			}
			//we missed a top-level box, cannot go any further if no repair
			if (!use_repair) {
				rsi->nb_errors++;
				rsi->isox_state = REPAIR_ISO_STATUS_DONE;
				return;
			}

			RouteRepairRange *rr = queue_repair_range(ctx, rsi, patch_start, patch_end);
			if (!rr) {
				rsi->isox_state = REPAIR_ISO_STATUS_DONE;
				return;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] Patching box %s patch start %u end %u\n", gf_4cc_to_str(patch_box_type), patch_start, patch_end ));
			rsi->isox_state = REPAIR_ISO_STATUS_PATCH_TOP_LEVEL;
			return;
		}
		if (!finfo->total_size) {
			if (!use_repair) {
				rsi->nb_errors++;
				rsi->isox_state = REPAIR_ISO_STATUS_DONE;
				return;
			}
			gf_assert(!rsi->total_size);
			RouteRepairRange *rr = queue_repair_range(ctx, rsi, size, 0);
			if (!rr) {
				rsi->isox_state = REPAIR_ISO_STATUS_DONE;
				return;
			}
			rsi->isox_state = REPAIR_ISO_STATUS_PATCH_TOP_LEVEL;
			return;
		}
		if (rsi->isox_state == REPAIR_ISO_STATUS_PATCH_TOP_LEVEL) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] All top-level boxes patched\n"));
		}
		//simple mode, patch mdats in order
		if (ctx->riso==REPAIR_ISO_SIMPLE) {
			if (use_repair)
				route_repair_build_ranges_full(ctx, rsi, finfo);
			else
				rsi->nb_errors++;

			rsi->isox_state = REPAIR_ISO_STATUS_DONE;
			return;
		}
		//partial mode, we are done
		if (ctx->riso==REPAIR_ISO_PARTIAL) {
			//not done yet, signal a final patch
			if (!finfo->total_size || (finfo->nb_frags>1))
				rsi->nb_errors = 1;
			rsi->isox_state = REPAIR_ISO_STATUS_DONE;
			return;
		}
		rsi->isox_state = REPAIR_ISO_STATUS_PATCH_MDAT;
		routein_repair_get_isobmf_deps(ctx, rsi);
		if (!gf_list_count(rsi->sample_deps)) {
			//no sample deps, repair everything
			if (use_repair)
				route_repair_build_ranges_full(ctx, rsi, finfo);
			else
				rsi->nb_errors++;
			rsi->isox_state = REPAIR_ISO_STATUS_DONE;
			return;
		}
		//fallthrough
	}
	gf_assert(rsi->sample_deps);
	//get first sample dep
	SampleDepInfo *sdi = NULL;
	count = gf_list_count(rsi->sample_deps);
	for (i=0; i<count; i++) {
		sdi = gf_list_get(rsi->sample_deps, i);
		if (sdi->valid || sdi->drop) {
			sdi = NULL;
			continue;
		}
		//no repair, drop sample
		if (!use_repair) {
			drop_sample_ref(rsi->sample_deps, sdi, GF_TRUE);
			sdi = NULL;
			continue;
		}

		//drop rules, to refine
		if (
			//if 0 or 1 indirect deps, drop
			(sdi->num_indirect_dependencies<=1)
			//if not "as important" as other refs, drop
//			|| (sdi->num_direct_dependencies + 1 < rsi->max_dep_per_sample)
		) {
			drop_sample_ref(rsi->sample_deps, sdi, GF_TRUE);
			sdi = NULL;
			continue;
		}
		break;
	}
	if (sdi) {
		Bool has_patch=GF_FALSE;
		sdi->valid = 2;
		u32 prev_frag_end=0;
		//get range(s)
		for (i=0;i<rsi->finfo.nb_frags; i++) {
			GF_LCTFragInfo *frag = &rsi->finfo.frags[i];
			u32 start_patch = sdi->start_range;
			u32 end_patch = sdi->end_range;
			//part before a hole
			if (start_patch<frag->offset) {
				if (start_patch<prev_frag_end)
					start_patch = prev_frag_end;

				if (end_patch>frag->offset)
					end_patch = frag->offset;
			} else {
				start_patch = 0;
				end_patch = 0;
			}
			prev_frag_end = frag->offset + frag->size;
			if (!start_patch && !end_patch) continue;

			RouteRepairRange *rr = queue_repair_range(ctx, rsi, start_patch, end_patch);
			if (!rr) {
				rsi->isox_state = REPAIR_ISO_STATUS_DONE;
				return;
			}
			has_patch = GF_TRUE;
			if (end_patch <= frag->offset+frag->size)
				break;
		}
		//final patch - sample exceeds end of last range
		if (sdi->end_range>prev_frag_end) {
			u32 start_patch = sdi->start_range;
			if (start_patch<prev_frag_end)
				start_patch = prev_frag_end;
			else if (sdi->start_range - prev_frag_end < 1500)
				start_patch = prev_frag_end;
			u32 end_patch = finfo->total_size ? sdi->end_range : 0;

			RouteRepairRange *rr = queue_repair_range(ctx, rsi, start_patch, end_patch);
			if (!rr) {
				rsi->isox_state = REPAIR_ISO_STATUS_DONE;
				return;
			}
			has_patch = GF_TRUE;
		}
		if (has_patch) return;
	}
	gf_assert(sdi==NULL);

	//we are done patching mdat
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] MDAT patching done:\n"));

	//reset the frag info to complete file and add holes for removed samples
	gf_assert(rsi->finfo.total_size);
	gf_route_dmx_patch_frag_info(ctx->route_dmx, rsi->service_id, &rsi->finfo, 0, rsi->finfo.total_size);

	for (i=0; i<count; i++) {
		sdi = gf_list_get(rsi->sample_deps, i);
		u8 drop = sdi->drop;
		gf_assert(!drop || (rsi->tsio->bytes_sent <= sdi->start_range));

		if (drop) {
			//reset data
			u32 s_size = sdi->end_range-sdi->start_range;
			u8 *s_data = rsi->finfo.blob->data+sdi->start_range;
			memset(s_data, 0, s_size);

			//add a fake NAL size length on 4 bytes (crude, to refine)
			s_size-=4;
			s_data[0] = s_size>>24;
			s_data[1] = s_size>>16;
			s_data[2] = s_size>>8;
			s_data[3] = s_size&0xFF;

			//in depx mode, don't hide moof when patching mdat - usually breaks decoders
			if (ctx->riso == REPAIR_ISO_DEPS) {
				//add hole and trigger final local repair to hide moofs
				gf_route_dmx_add_frag_hole(ctx->route_dmx, rsi->service_id, &rsi->finfo, sdi->start_range, sdi->end_range-sdi->start_range);
				rsi->nb_errors++;
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("\tSample %d (GID %u): DTS "LLU": valid %u - dropped: %u\n", sdi->sample_id, sdi->gop_id, sdi->dts, sdi->valid, sdi->drop));
	}
	rsi->isox_state = REPAIR_ISO_STATUS_DONE;
	gf_mx_p(rsi->finfo.blob->mx);
	rsi->finfo.blob->flags &= ~GF_BLOB_RANGE_IN_TRANSFER;
	gf_mx_v(rsi->finfo.blob->mx);
}


enum {
	ROUTE_FTYPE_UNKNOWN=0,
	ROUTE_FTYPE_M2TS,
	ROUTE_FTYPE_ISOBMF,
};

static u32 guess_file_type(GF_ROUTEEventFileInfo *finfo, u8 *data, u32 size)
{
	const char *ext = gf_file_ext_start(finfo->filename);
	if (ext) {
		if (!stricmp(ext, ".ts")) return ROUTE_FTYPE_M2TS;
		if (!stricmp(ext, ".m2ts")) return ROUTE_FTYPE_M2TS;
		if (!stricmp(ext, ".mp4")) return ROUTE_FTYPE_ISOBMF;
		if (!stricmp(ext, ".m4s")) return ROUTE_FTYPE_ISOBMF;
		if (!stricmp(ext, ".mp4s")) return ROUTE_FTYPE_ISOBMF;
	}
	if (gf_m2ts_probe_data(data, size)) return ROUTE_FTYPE_M2TS;
	u32 pos=0, bsize;
	u32 btype = next_top_level_box(finfo, data, size, GF_TRUE, &pos, &bsize);
	if (btype) return ROUTE_FTYPE_ISOBMF;
	return ROUTE_FTYPE_UNKNOWN;
}

void routein_check_type(ROUTEInCtx *ctx, GF_ROUTEEventFileInfo *finfo, u32 service_id)
{
	finfo->channel_hint = guess_file_type(finfo, finfo->blob->data, finfo->blob->size);
	if (finfo->channel_hint) {
		gf_route_dmx_set_object_hint(ctx->route_dmx, service_id, finfo->tsi, finfo->toi, finfo->channel_hint);
	}
}

#define CHECK_ISOBMF

#ifdef CHECK_ISOBMF
static void routein_check_isobmf(ROUTEInCtx *ctx, GF_ROUTEEventFileInfo *finfo)
{
	u32 pos = 0;
	u8 *data = finfo->blob->data;
	while (pos < finfo->total_size) {
		u32 bsize = GF_4CC(data[pos], data[pos+1], data[pos+2], data[pos+3]);
		u32 btype = GF_4CC(data[pos+4], data[pos+5], data[pos+6], data[pos+7]);
		switch (btype) {
		case GF_4CC('f','t','y','p'):
		case GF_4CC('m','o','o','v'):
		case GF_4CC('m','e','t','a'):
		case GF_4CC('s','t','y','p'):
		case GF_4CC('m','o','o','f'):
		case GF_4CC('f','r','e','e'):
		case GF_4CC('m','d','a','t'):
			break;
		default:
			fprintf(stderr, "Unknown top-level %s\n", gf_4cc_to_str(btype));
		}
		pos += bsize;
		if (!bsize) {
			pos = finfo->total_size;
			break;
		}
	}
	if (!finfo->nb_frags) {
		fprintf(stderr, "File lost\n");
	}
	else if (pos!=finfo->total_size) {
		fprintf(stderr, "Invalid top-level box size\n");
	} else {
		fprintf(stderr, "Recovered file OK: %s size %u !\n", finfo->filename, finfo->total_size);
		gf_assert(finfo->nb_frags == 1);
		gf_assert(finfo->frags[0].offset == 0);
		gf_assert(finfo->frags[0].size == finfo->total_size);
	}
}
#endif


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

	if (!start_only) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] File %s (TSI=%u, TOI=%u) corrupted, patching\n", finfo->filename, finfo->tsi, finfo->toi));
	}

	if (!finfo->channel_hint) {
		routein_check_type(ctx, finfo, evt_param);
	}
	switch (finfo->channel_hint) {
	case ROUTE_FTYPE_M2TS:
		drop_if_first = routein_repair_segment_ts_local(ctx, evt_param, finfo, start_only);
		break;
	case ROUTE_FTYPE_ISOBMF:
		routein_repair_segment_isobmf_local(ctx, evt_param, finfo, start_only);
#ifdef CHECK_ISOBMF
		routein_check_isobmf(ctx, finfo);
#endif
		break;
	default:
		break;
	}

	if (finfo->blob->mx) {
		finfo->blob->flags |= GF_BLOB_PARTIAL_REPAIR;
		if (!in_transfer)
			finfo->blob->flags &= ~GF_BLOB_IN_TRANSFER;
		gf_mx_v(finfo->blob->mx);
	}
	return drop_if_first;
}

static u32 get_max_dispatch_len(GF_ROUTEEventFileInfo *finfo)
{
	u32 max_len = 0;
	//figure out largest safest size from start
	if (finfo->channel_hint==ROUTE_FTYPE_M2TS) {
		max_len = finfo->frags[0].size / 188;
		max_len *= 188;
	} else if (finfo->channel_hint==ROUTE_FTYPE_ISOBMF) {
		u32 btype, bsize, cur_pos=0;
		u32 valid_bytes=0;
		u32 true_size = finfo->frags[0].size;
		while (1) {
			btype = next_top_level_box(finfo, finfo->blob->data, true_size, GF_TRUE, &cur_pos, &bsize);
			if (!btype) break;
			//never send a box until completed
			if (cur_pos + bsize>true_size)
				break;

			cur_pos+=bsize;
			//always break at moof - if we had a moof+mdat valid, we'll stop at the mdat end
			if (btype != GF_4CC('m','o','o','f')) {
				valid_bytes = cur_pos;
			}
		}
		max_len = valid_bytes;
	}
	return max_len;
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
			finfo->frags[0].size = get_max_dispatch_len(finfo);
			if (finfo->frags[0].size > tsio->bytes_sent)
				routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);
			finfo->frags[0].size = true_size;

			//remember we have a file in progress so that we don't dispatch packets from following file
			tsio->flags_progress |= TSIO_FILE_PROGRESS;
			gf_assert(tsio->bytes_sent<=finfo->frags[0].size);
			return;
		}
		if (ctx->riso >= REPAIR_ISO_SIMPLE)
			fast_repair = 0;
		else
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
			u32 true_size = finfo->frags[0].size;
			finfo->frags[0].size = get_max_dispatch_len(finfo);
			if (finfo->frags[0].size > tsio->bytes_sent)
				routein_on_event_file(ctx, evt, evt_param, finfo, GF_FALSE, GF_FALSE);

			finfo->frags[0].size = true_size;
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
			if (ctx->riso!=REPAIR_ISO_NO)
				rsi->isox_state = REPAIR_ISO_STATUS_INIT;
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

	if (finfo->partial) {
		//HTTP repair, build ranges
		if (!fast_repair) {
			if (ctx->riso!=REPAIR_ISO_NO) {
				rsi->isox_state = REPAIR_ISO_STATUS_INIT;
				route_repair_build_ranges_isobmf(ctx, rsi, finfo);
				if (ctx->repair<ROUTEIN_REPAIR_FULL) {
					gf_list_add(ctx->seg_repair_queue, rsi);
					if (tsio) gf_list_add(tsio->pending_repairs, rsi);
					repair_session_dequeue(ctx, rsi);
					return;
				}
			} else {
				route_repair_build_ranges_full(ctx, rsi, finfo);
			}
		}
		//direct repair, still queue but mark as in error
		else {
			rsi->nb_errors = 1;
		}
	}

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


static void repair_session_dequeue(ROUTEInCtx *ctx, RepairSegmentInfo *rsi)
{
	Bool unprotect;
	TSI_Output *tsio;

restart:
	unprotect=GF_FALSE;
	tsio = rsi->tsio;

	//done with this repair, remove force_keep flag from object
	if (tsio) {
		gf_assert(gf_list_find(tsio->pending_repairs, rsi) == 0);
		unprotect = GF_TRUE;
	}
	//remove before calling route_on_event, as it may trigger a delete
	gf_list_del_item(ctx->seg_repair_queue, rsi);


	if (!rsi->removed) {
		//do a local file repair if we still have errors ?
		if (rsi->nb_errors) {
			switch (rsi->evt) {
			case GF_ROUTE_EVT_DYN_SEG:
				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] File %s still has error, doing a final local repair\n", rsi->finfo.filename));
				u32 repair = ctx->repair;
				ctx->repair = ROUTEIN_REPAIR_STRICT;
				routein_repair_local(ctx, rsi->evt, rsi->service_id, &rsi->finfo, GF_FALSE);
				ctx->repair = repair;
				rsi->nb_errors = 0;
				break;
			case GF_ROUTE_EVT_FILE:
			case GF_ROUTE_EVT_MPD:
			case GF_ROUTE_EVT_HLS_VARIANT:
				rsi->finfo.partial = GF_LCTO_PARTIAL_ANY;
				break;
			default:
				break;
			}
#ifdef CHECK_ISOBMF
		} else {
			routein_check_isobmf(ctx, &rsi->finfo);
#endif
		}
		//flush
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repair done for object %s (TSI=%u, TOI=%u)%s\n", rsi->finfo.filename, rsi->finfo.tsi, rsi->finfo.toi, rsi->nb_errors ? " - errors remain" : ""));

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
	if (rsi->sample_deps) {
		if (!ctx->sample_deps_reservoir) ctx->sample_deps_reservoir = gf_list_new();
		while (gf_list_count(rsi->sample_deps)) {
			gf_list_add(ctx->sample_deps_reservoir, gf_list_pop_back(rsi->sample_deps));
		}
		gf_list_del(rsi->sample_deps);
	}
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
		if (!rsi->removed && rsess->range->bytes_recv) {
			u64 patch_end = rsess->range->br_start + rsess->range->bytes_recv;
			if (patch_end > rsess->range->br_end)
				patch_end = rsess->range->br_end;

			else if (patch_end < rsess->range->br_end) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] Incomplete byte range in file %s: end offset %u but last byte received %u received byte range end %u\n", rsi->finfo.filename, rsess->range->br_end, patch_end));
			}
			gf_route_dmx_patch_frag_info(ctx->route_dmx, rsi->service_id, &rsi->finfo, rsess->range->br_start, (u32) patch_end);
		}

		gf_list_add(ctx->seg_range_reservoir, rsess->range);
		if (!res_code && rsess->server && (rsess->server->accept_ranges == RANGE_SUPPORT_PROBE))
			rsess->server->accept_ranges = RANGE_SUPPORT_YES;
	}

	rsess->initial_retry = 0;
	//always reset even if we have pending byte ranges, so that each repair session fetches the most urgent download
	rsess->current_si = NULL;
	rsess->range = NULL;
	if (res_code<0)
		rsi->nb_errors++;

	gf_assert(rsi->pending);
	rsi->pending--;
	if (rsi->pending) return;

	if (rsi->isox_state>=REPAIR_ISO_STATUS_PATCH_TOP_LEVEL) {
		route_repair_build_ranges_isobmf(ctx, rsi, &rsi->finfo);
		return;
	}

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
		gf_assert(rsi->finfo.nb_frags == 1);
		rsi->finfo.partial = GF_LCTO_PARTIAL_NONE;
		rsi->finfo.blob->flags &= ~GF_BLOB_CORRUPTED;
	}
	//not a fragment, remove in-transfer flag
	if (rsi->evt!=GF_ROUTE_EVT_DYN_SEG_FRAG)
		rsi->finfo.blob->flags &= ~GF_BLOB_IN_TRANSFER;
	gf_mx_v(rsi->finfo.blob->mx);

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
	repair_session_dequeue(ctx, rsi);
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

		const char *repair_base_uri, *repair_server_url;
		gf_route_dmx_get_repair_info(ctx->route_dmx, rsi->service_id, &repair_base_uri, &repair_server_url);
		u32 repair_base_uri_len = repair_base_uri ? (u32) strlen(repair_base_uri) : 0;
		Bool has_repair_server = GF_FALSE;

		for (i=0; i< gf_list_count(ctx->repair_servers); i++) {
			repair_server = gf_list_get(ctx->repair_servers, i);
			if (repair_server->url && repair_server_url && !strcmp(repair_server->url, repair_server_url))
				has_repair_server = GF_TRUE;
			if (!repair_server->url || !repair_server->accept_ranges) {
				repair_server = NULL;
				continue;
			}
			break;
		}
		if (!repair_server && !has_repair_server && repair_server_url) {
			repair_server = routein_push_repair_server(ctx, repair_server_url, rsi->service_id);
			if (repair_server && !repair_server->accept_ranges) repair_server = NULL;
		}

		if (repair_server) {
			const char *filename_start = NULL;
			if (repair_base_uri && !strncmp(rsi->finfo.filename, repair_base_uri, repair_base_uri_len)) {
				filename_start = rsi->finfo.filename + repair_base_uri_len+1;
			}
			if (!filename_start) {
				char *sep = strstr(rsi->finfo.filename, "://");
				if (sep) {
					filename_start = rsi->finfo.filename;
				} else {
					sep = strchr(rsi->finfo.filename, '/');
					filename_start = sep ? sep : rsi->finfo.filename;
				}
			}
			url = gf_url_concatenate(repair_server->url, filename_start);
		}


		if (!url) {
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Failed to find an adequate repair server for %s - Repair abort \n", rsi->finfo.filename));
			rsi->nb_errors++;
			repair_session_done(ctx, rsess, e);
			return;
		}

		if (!rsess->dld) {
			GF_DownloadManager *dm = gf_filter_get_download_manager(ctx->filter);
			rsess->dld = gf_dm_sess_new(dm, url, GF_NETIO_SESSION_NOT_CACHED | GF_NETIO_SESSION_NOT_THREADED | GF_NETIO_SESSION_PERSISTENT, NULL, NULL, &e);
			if (rsess->dld) {
				gf_dm_sess_set_netcap_id(rsess->dld, "__ignore");
				gf_dm_sess_set_timeout(rsess->dld, 1);
			}
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
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] Queue request for %s byte range %u-%u\n", url, rsess->range->br_start, rsess->range->br_end-1));
		} else {
			u32 start_r = rr->br_start;
			if (!rr->br_start) {
				rr->is_open = 0;
			} else {
				//add one more byte, so that if the resource is complete (we missed last 0-byte chunk) we issue
				//a request falling into the file range
				start_r -= 1;
				rr->is_open = 1;
			}
			gf_dm_sess_set_range(rsess->dld, start_r, 0, GF_TRUE);
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] Queue request for %s open byte range %u-\n", url, start_r));
		}
		gf_free(url);
		ctx->has_data = GF_TRUE;
	}

refetch:
	offset = rsess->range->br_start + rsess->range->bytes_recv;
	//we requested one more byte, adjust offset
	if (rsess->range->is_open) offset -= 1;
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
			u32 filesize = gf_dm_sess_get_resource_size(rsess->dld);
			//it is quite possible that the server replied 200 on an open end-range request when the segment size is unknown
			if (!rsess->range->br_end) {
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

	//open end range - we can get 0 bytes as a 0-size chunk can be used in flute and it could be lost!
	if (!rsess->range->br_end) {
		if (e>=GF_OK) {
			rsess->range->br_end = gf_dm_sess_get_resource_size(rsess->dld);
			if (!rsess->range->br_end || (rsess->range->br_end<rsess->current_si->finfo.blob->size)) {
				e = GF_REMOTE_SERVICE_ERROR;
			} else {
				e = gf_route_dmx_patch_blob_size(ctx->route_dmx, rsess->current_si->service_id, &rsess->current_si->finfo, rsess->range->br_end);
			}
		} else {
			//if we have an error and the content start was the first byte after the known file size, consider we got the final range
			u64 res_size = gf_dm_sess_get_resource_size(rsess->dld);
			if (res_size && (res_size == rsess->range->br_start)) {
				rsess->range->br_end = rsess->range->br_start;
				e = GF_EOS;
			}
		}
	}
	if (offset + nb_read > rsess->range->br_end)
		e = GF_REMOTE_SERVICE_ERROR;

	if (nb_read && (e>=GF_OK)) {
		gf_mx_p(rsi->finfo.blob->mx);
		memcpy(rsi->finfo.blob->data + offset, rsess->http_buf, nb_read);
		gf_mx_v(rsi->finfo.blob->mx);
		rsess->range->bytes_recv += nb_read;
		ctx->has_data = GF_TRUE;
		//for now, fragment info is updated once the session is done
		//flush buffer until network empty
		if (e==GF_OK) goto refetch;
	}
	if (e==GF_OK) return;

	//figure out total size if indicated by server - otherwise it is 0
	if (!rsi->finfo.total_size) {
		rsi->total_size = gf_dm_sess_get_resource_size(rsess->dld);
		if (rsi->total_size)
			gf_route_dmx_patch_blob_size(ctx->route_dmx, rsi->service_id, &rsi->finfo, rsi->total_size);
	}
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
