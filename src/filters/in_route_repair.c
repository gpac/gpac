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

#include <gpac/isomedia.h>
#include "in_route.h"

#ifndef GPAC_DISABLE_ROUTE


static void routein_repair_get_inverted_isobmf_deps(SampleRangeDependency *out_ranges, u32 nb_ranges) {
	u32 i, j;

	u32 indexes[nb_ranges];
	memset(indexes, 0, nb_ranges* sizeof(int));

	for(i=0; i < nb_ranges; i++) {
		for(j=0; j < out_ranges[i].nb_deps; j++) {
			out_ranges[out_ranges[i].dep_ids[j]].nb_rev_deps++;
		}
	}
	
	for(i=0; i < nb_ranges; i++) {
		out_ranges[i].rev_dep_ids = gf_malloc(sizeof(u32) * out_ranges[i].nb_rev_deps);
	}

	for(i=0; i < nb_ranges; i++) {
		for(j=0; j < out_ranges[i].nb_deps; j++) {
			out_ranges[out_ranges[i].dep_ids[j]].rev_dep_ids[indexes[out_ranges[i].dep_ids[j]]] = i;
			indexes[out_ranges[i].dep_ids[j]]++;
		}
	}
}

//simple dependency extraction from isobmff:
//if error returns GF_FALSE, otherwise GF_TRUE
//if all samples are random access point, ranges is set to NULL and nb_ranges set to 0
//otherwise ranges is allocated with the dependency list
//the function is very basic and will need further rework, listng all direct reference samples
static Bool routein_repair_get_isobmf_deps(const char *seg_name, GF_Blob *blob, SampleRangeDependency **out_ranges, u32 *nb_ranges)
{
	GF_ISOFile *file;
	u64 BytesMissing;
	u32 i;
	SampleRangeDependency *ranges;
	char szBlobPath[100];
	if ((!blob && !seg_name) || !out_ranges || !nb_ranges) return GF_FALSE;
	*out_ranges = NULL;
	*nb_ranges = 0;
	GF_Err e = gf_isom_open_progressive_ex("isobmff://", 0, 0, 0, &file, &BytesMissing, NULL);
	if (e) return GF_FALSE;
	if (blob) {
		sprintf(szBlobPath, "gmem://%p", blob);
		e = gf_isom_open_segment(file, szBlobPath, 0, 0, 0);
	} else {
		e = gf_isom_open_segment(file, seg_name, 0, 0, 0);
	}
	if (e) {
		gf_isom_delete(file);
		return GF_FALSE;
	}
	s32 max_cts_o = (s32) gf_isom_get_max_sample_cts_offset(file, 1);
	s32 min_cts_o = gf_isom_get_min_negative_cts_offset(file, 1, GF_ISOM_MIN_NEGCTTS_ANY);
	u32 ID, nb_refs, count;
	const u32 *refs;

	count = gf_isom_get_sample_count(file, 1);
	if (gf_isom_get_sample_references(file, 1, 1, &ID, &nb_refs, &refs)==GF_OK) {
		blob->use_sref = GF_TRUE;
	} else {
		blob->use_sref = GF_FALSE;
		if (!max_cts_o || !count) {
			gf_isom_delete(file);
			return GF_TRUE;
		}
	}

	ranges = gf_malloc(sizeof(SampleRangeDependency)*count);
	memset(ranges, 0, sizeof(SampleRangeDependency)*count);
	*out_ranges = ranges;
	*nb_ranges = count;

	GF_ISOSample static_sample;
	s32 max_ctso = -1;
	u32 cur_id_sap = 0;
	u32 cur_id_p = 0;
	u32 nb_levels=0;

	for (i=0; i<count; i++) {
		SampleRangeDependency *r = &ranges[i];
		u64 offset;
		s32 cts_offset;
		memset(&static_sample, 0, sizeof(static_sample));
		GF_ISOSample *samp = gf_isom_get_sample_info_ex(file, 1, i+1, NULL, &offset, &static_sample);
		if (!samp) break;
		r->size = samp->dataLength;
		r->offset = (u32) offset;

		//initialize costs
		r->bytes_cost = 0;
		r->reqs_cost = 0;

		if (blob->use_sref) {
			gf_isom_get_sample_references(file, 1, i+1, &r->id, &r->nb_deps, &refs);
			if (r->id==0xFFFFFFFF) {
				r->nb_deps = -1;
				continue;
			}
			if (refs && r->nb_deps) {
				r->dep_ids = gf_malloc(sizeof(u32)*r->nb_deps);
				memcpy(r->dep_ids, refs, sizeof(u32)*r->nb_deps);
			}
			if (samp->IsRAP) {
				r->type = 1;
			}
			continue;
		}

		cts_offset = samp->CTS_Offset;
		//translate to unsigned cts offset
		if (min_cts_o<0) cts_offset -= cts_offset;

		if (samp->IsRAP) {
			max_ctso = -1;
			cur_id_sap += cur_id_p + nb_levels+1;
			r->id = cur_id_sap;
			r->nb_deps = 0;
			cur_id_p = cur_id_sap;
			nb_levels = 0;
			r->type = 1;
			continue;
		}
		if ((max_ctso<0) || (cts_offset==max_ctso)) {
			max_ctso = cts_offset;
			cur_id_p += nb_levels+1;
			nb_levels = 1;
			r->id = cur_id_p;
			r->nb_deps = 1;
			r->dep_ids = gf_malloc(sizeof(u32));
			r->dep_ids[0] = cur_id_sap;
			continue;
		}
		if (cts_offset) {
			nb_levels = 2;
			r->id = cur_id_p+1;
			r->nb_deps = 2;
			r->dep_ids = gf_malloc(r->nb_deps * sizeof(u32));
			r->dep_ids[0] = cur_id_sap;
			r->dep_ids[1] = cur_id_p;
			continue;
		}
		if (nb_levels == 2) nb_levels = 3;
		else if (nb_levels==1) nb_levels = 2;

		r->id = cur_id_p + nb_levels-1;
		r->nb_deps = 2;
		r->dep_ids = gf_malloc(r->nb_deps * sizeof(u32));
		r->dep_ids[0] = cur_id_p;
		r->dep_ids[1] = cur_id_p + nb_levels-2;
		r->type = 2;
	}
	gf_isom_delete(file);

	routein_repair_get_inverted_isobmf_deps(*out_ranges, *nb_ranges);

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_ROUTE, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Range dependency/inverted dependency for file %s\n", seg_name ? seg_name : szBlobPath));
		for (i=0; i<count; i++) {
			u32 j;
			SampleRangeDependency *r = &ranges[i];
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("\t#%3d size %10u offset %10u ID %2u depends on range IDs", i+1, r->size, r->offset, r->id));
			for (j=0;j<r->nb_deps;j++) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, (" %2u", r->dep_ids[j]));
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("; dependant frames:"));
			for (j=0;j<r->nb_rev_deps;j++) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, (" %2u", r->rev_dep_ids[j]));
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("\n"));
		}
	}
#endif
	return GF_TRUE;
}


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
	u32 was_partial = finfo->partial==GF_LCTO_PARTIAL_NONE ? GF_FALSE : GF_TRUE;
	u32 patch_first_range_size = GF_FALSE;

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
			memset(data+pos+8, 0, erase_size);
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

static Bool route_repair_is_range_completely_received(GF_LCTFragInfo *frags, u32 nb_frags, u32 start, u32 size) {
	//check if intervale [start, start+size[ belongs to frags list of intervales
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

static void route_repair_build_ranges_isobmf(ROUTEInCtx *ctx, RepairSegmentInfo *rsi, GF_ROUTEEventFileInfo *finfo, int position) {
	u32 pos = position;
	u32 min_size = 1024;
	u32 box_size, box_type;

	while(pos+8 <= finfo->total_size) {
		if(!route_repair_is_range_completely_received(finfo->frags, finfo->nb_frags, pos, 8)) {
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

			rr->br_start = pos;
			rr->br_end = MIN(finfo->total_size, pos + min_size);
			rr->sample_id = -1;

			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repair top_level boxes: adding range [%u, %u[ for repair\n", rr->br_start, rr->br_end));

			gf_list_add(rsi->ranges, rr);
			rsi->last_pos_repair_top_level = rr->br_start;
			return;
		} else {
			box_size = GF_4CC(finfo->blob->data[pos], finfo->blob->data[pos+1], finfo->blob->data[pos+2], finfo->blob->data[pos+3]);
			box_type = GF_4CC(finfo->blob->data[pos+4], finfo->blob->data[pos+5], finfo->blob->data[pos+6], finfo->blob->data[pos+7]);
			if(pos + box_size > finfo->total_size) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] Corrupted data: top-level box range [%u, %u[ exceeds segment size %u \n", pos, pos+box_size, finfo->total_size));
				return;
			}
			if ((box_type != GF_4CC('m', 'd', 'a', 't')) && (box_type != GF_4CC('i', 'd', 'a', 't'))) {
				if(!route_repair_is_range_completely_received(finfo->frags, finfo->nb_frags, pos, box_size)) {
					RouteRepairRange *rr = gf_list_pop_back(ctx->seg_range_reservoir);
					if (!rr) {
						GF_SAFEALLOC(rr, RouteRepairRange);
						if (!rr) {
							rsi->nb_errors++;
							//check next box
							pos += box_size;
							continue;
						}
					} else {
						memset(rr, 0, sizeof(RouteRepairRange));
					}

					rr->br_start = pos;
					rr->br_end = MIN(finfo->total_size, MAX(pos + box_size, pos + min_size));
					rr->sample_id = -1;

					GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repair top_level boxes: adding range [%u, %u[ for repair\n", rr->br_start, rr->br_end));

					gf_list_add(rsi->ranges, rr);
					rsi->last_pos_repair_top_level = rr->br_start;
				}
			}
			pos += box_size;
		}
	}
}


static void routein_repair_get_costs(RepairSegmentInfo *rsi, SampleRangeDependency *r, u32 threshold, u32 *nb_bytes, u32 *nb_requests) {
	*nb_requests = 0;
	*nb_bytes = 0;
	u32 last_br_end = 0, last_br_start = 0;
	u32 i;

	for (i=0; i<=rsi->finfo.nb_frags; i++) {
		u32 br_start = 0, br_end = 0;
		// first range
		if (!i) {
			br_end = rsi->finfo.frags[i].offset;
		}
		//middle ranges
		else if (i < rsi->finfo.nb_frags) {
			br_start = rsi->finfo.frags[i-1].offset + rsi->finfo.frags[i-1].size;
			br_end = rsi->finfo.frags[i].offset;
		}
		//last range
		else {
			br_start = rsi->finfo.frags[rsi->finfo.nb_frags-1].offset + rsi->finfo.frags[rsi->finfo.nb_frags-1].size;
			br_end = rsi->finfo.total_size;
		}

		if (br_end <= br_start) continue;
		if (br_end <= r->offset) continue;
		if (br_end > r->offset && br_start < r->offset+r->size) {

			if(last_br_start < last_br_end && br_start - last_br_end < threshold) {
				last_br_end = br_end;
			} else {
				*nb_bytes += last_br_end - last_br_start;

				(*nb_requests)++;
				last_br_start = MAX(r->offset, br_start);
				last_br_end = MIN(r->offset + r->size, br_end);
			}
		}
		if (br_start >= r->offset + r->size) {
			break;
		}
	}
	*nb_bytes += last_br_end - last_br_start;
}

static void routein_repair_propagate_costs_rec(RepairSegmentInfo *rsi, s32 bytes_cost, s32 reqs_cost, u32 index, Bool visited[]) {
	u32 i;
	if(visited[index]) return;

	rsi->srd[index].total_bytes_cost += bytes_cost;
	rsi->srd[index].total_reqs_cost += reqs_cost; 
	visited[index] = GF_TRUE;

	for(i=0; i < rsi->srd[index].nb_rev_deps; i++) {
		routein_repair_propagate_costs_rec(rsi, bytes_cost, reqs_cost, rsi->srd[index].rev_dep_ids[i], visited);
	}
}

static void routein_repair_propagate_costs(RepairSegmentInfo *rsi, s32 bytes_cost, s32 reqs_cost, u32 index) {
	Bool visited[rsi->nb_ranges];
	memset(visited, GF_FALSE, rsi->nb_ranges*sizeof(Bool));

	routein_repair_propagate_costs_rec(rsi, bytes_cost, reqs_cost, index, visited);
}

static void routein_repair_compute_entire_all_costs(RepairSegmentInfo *rsi, u32 threshold) {
	u32 i, nb_bytes=0, nb_reqs=0;
	Bool visited[rsi->nb_ranges];

	//use the principle of propagation to compute the real costs !
	for(i=0; i < rsi->nb_ranges; i++) {
		routein_repair_get_costs(rsi, &rsi->srd[i], threshold, &nb_bytes, &nb_reqs);

		rsi->srd[i].bytes_cost = nb_bytes;
		rsi->srd[i].reqs_cost = nb_reqs;

		if(nb_bytes > 0 && nb_reqs > 0) {
			memset(visited, GF_FALSE, rsi->nb_ranges*sizeof(Bool));
			routein_repair_propagate_costs(rsi, nb_bytes, nb_reqs, i);
		}
	}
}

static u32 routein_repair_isobmf_frames(ROUTEInCtx *ctx, RepairSegmentInfo *rsi, u32 index, u32 threshold) {
	//add byte ranges to "rsi->ranges" for repair
	u32 nb_rr = 0;
	u32 i;
	RouteRepairRange *rr = NULL;
	SampleRangeDependency *r = &rsi->srd[index];

	for (i=0; i<=rsi->finfo.nb_frags; i++) {
		u32 br_start = 0, br_end = 0;
		// first range
		if (!i) {
			br_end = rsi->finfo.frags[i].offset;
		}
		//middle ranges
		else if (i < rsi->finfo.nb_frags) {
			br_start = rsi->finfo.frags[i-1].offset + rsi->finfo.frags[i-1].size;
			br_end = rsi->finfo.frags[i].offset;
		}
		//last range
		else {
			br_start = rsi->finfo.frags[rsi->finfo.nb_frags-1].offset + rsi->finfo.frags[rsi->finfo.nb_frags-1].size;
			br_end = rsi->finfo.total_size;
		}

		//this was correctly received !
		if (br_end <= br_start) continue;
		//byte range is before sample range
		if (br_end <= r->offset) continue;

		if (br_end > r->offset && br_start < r->offset+r->size) {

			if(rr && br_start - rr->br_end < threshold) {
				rr->br_end = br_end;
			} else {
				if(rr) {
					nb_rr++;
					gf_list_add(rsi->ranges, rr);
					GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repair (TSI=%u, TOI=%u) frame ID #%3u: adding range [%u, %u[ for repair\n", rsi->finfo.tsi, rsi->finfo.toi, r->id, rr->br_start, rr->br_end));
				}

				rr = gf_list_pop_back(ctx->seg_range_reservoir);
				if (!rr) {
					GF_SAFEALLOC(rr, RouteRepairRange);
					if (!rr) {
						rsi->nb_errors++;
						continue;
					}
				} else {
					memset(rr, 0, sizeof(RouteRepairRange));
				}
				rr->br_start = MAX(r->offset, br_start);
				rr->br_end = MIN(r->offset + r->size, br_end);
				rr->sample_id = index;
			}
		}
		//byte range is after sample range
		if (br_start >= r->offset + r->size) {
			break;
		}
	}

	if(rr) {
		nb_rr++;
		gf_list_add(rsi->ranges, rr);
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Repair (TSI=%u, TOI=%u) frame ID #%3u: adding range [%u, %u[ for repair\n", rsi->finfo.tsi, rsi->finfo.toi, r->id, rr->br_start, rr->br_end));
	}

#ifndef GPAC_DISABLE_LOG
	u32 bytes=0, reqs=0;
	routein_repair_get_costs(rsi, r, threshold, &bytes, &reqs);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[REPAIR] Repair (TSI=%u, TOI=%u) Frame #%3u - Costs in number of bytes: %10u; number of reqs: %4u \n", rsi->finfo.tsi, rsi->finfo.toi, r->id, bytes, reqs));
#endif

	return nb_rr;
}

static void route_repair_topological_sort_samples(SampleRangeDependency srd[], u32 nb_ranges, SampleRangeDependency* sorted_samples[]) {
	s32 i;
	u32 j;
	s32 sort_i = 0;
	int incoming[nb_ranges];

	for(i=0; i < nb_ranges; i++) {
		incoming[i] = srd[i].nb_deps;
	}

	//Kahn's algorithm for toological sort
	for(i=0; i < nb_ranges; i++) {
		if(incoming[i] == 0) {
			sorted_samples[sort_i] = &srd[i];
			sort_i++;
		}
	}
	
	for(i=0; i < nb_ranges; i++) {
		assert(sort_i > i); 
		SampleRangeDependency* sample = sorted_samples[i];

		for(j=0; j < sample->nb_rev_deps; j++) {
			u32 sample_i = sample->rev_dep_ids[j];

			incoming[sample_i]--;
			if(incoming[sample_i] == 0) {
				sorted_samples[sort_i] = &srd[sample_i];
				sort_i++;
			}
		}
	}
}

static void routein_delete_range_deps(SampleRangeDependency *srd, u32 nb_ranges)
{
	u32 i;
	for (i=0; i<nb_ranges; i++) {
		if (srd[i].dep_ids) gf_free(srd[i].dep_ids);
		if (srd[i].rev_dep_ids) gf_free(srd[i].rev_dep_ids);
	}
	gf_free(srd);
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

		//first range
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
		rr->sample_id = -1;
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
	if (finfo->blob->mx) {
		gf_mx_p(finfo->blob->mx);
		finfo->blob->flags &= ~GF_BLOB_CORRUPTED;
		if (finfo->blob->flags & GF_BLOB_IN_TRANSFER) in_transfer = GF_TRUE;
		else finfo->blob->flags |= GF_BLOB_IN_TRANSFER;
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

static void route_repair_isobmf_mdat_box(ROUTEInCtx *ctx, RepairSegmentInfo *rsi) {
	u32 threshold = 1024, i;
	
	routein_repair_get_isobmf_deps(rsi->finfo.filename, rsi->finfo.blob, &rsi->srd, &rsi->nb_ranges);
	if(! rsi->nb_ranges) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] All samples have the same dependency level in \"%s\", switching to full repair \n", rsi->finfo.filename));
		route_repair_build_ranges_full(ctx, rsi, &rsi->finfo);
		return;
	}

	if(! rsi->finfo.blob->use_sref) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[REPAIR] Dependency graph cannot be constructed reliably and accurately; switching to full repair. \n"));
		route_repair_build_ranges_full(ctx, rsi, &rsi->finfo);
		return;
	}

#ifndef GPAC_DISABLE_LOG
	//compute costs
	routein_repair_compute_entire_all_costs(rsi, threshold);
	for(i=0; i < rsi->nb_ranges; i++) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Frame ID #%3u: %6d bytes & %3d requests | %6u total bytes; %3d total requests \n", rsi->srd[i].id, rsi->srd[i].bytes_cost, rsi->srd[i].reqs_cost, rsi->srd[i].total_bytes_cost, rsi->srd[i].total_reqs_cost));
	}
#endif

	SampleRangeDependency* sorted_samples[rsi->nb_ranges];

	route_repair_topological_sort_samples(rsi->srd, rsi->nb_ranges, sorted_samples);
	for(i=0; i<rsi->nb_ranges; i++) {
		routein_repair_isobmf_frames(ctx, rsi, i, threshold);
	}
}

void routein_queue_repair(ROUTEInCtx *ctx, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo)
{
	u32 i, count;
	u32 fast_repair = 0;
	//TODO handle late data - keep log = warning as reminder
	RepairSegmentInfo *rsi;
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
			fprintf(stderr, "direct dispatch1 for file %s TOI %u\n", finfo->filename, finfo->toi);
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
			fprintf(stderr, "direct dispatch2 for file %s TOI %u\n", finfo->filename, finfo->toi);
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
	rsi = gf_list_pop_back(ctx->seg_repair_reservoir);
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

	if(finfo->partial) {
		if(ctx->repair == ROUTEIN_REPAIR_FULL) {
			route_repair_build_ranges_full(ctx, rsi, finfo);
		} else if(ctx->repair == ROUTEIN_REPAIR_ISOBMF) {
			rsi->state = 0;
			route_repair_build_ranges_isobmf(ctx, rsi, finfo, 0);

			if(gf_list_count(rsi->ranges) == 0) {
				rsi->state = 1;
				route_repair_isobmf_mdat_box(ctx, rsi);
			}
		}
	}

	if(gf_list_count(rsi->ranges) == 0) {
		if (rsi->filename) gf_free(rsi->filename);
		gf_list_transfer(ctx->seg_range_reservoir, rsi->ranges);
		gf_list_add(ctx->seg_range_reservoir, rsi->ranges);
		gf_list_add(ctx->seg_range_reservoir, rsi);
		return;
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

void repair_session_unqueue(ROUTEInCtx *ctx, RouteRepairSession *rsess, RepairSegmentInfo *rsi)
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
	//get next in list, if existing and done or if removed, unqueue
	rsi = gf_list_get(tsio->pending_repairs, 0);
	if (rsi && (rsi->done || (rsi->removed && !rsi->pending)) )
		goto restart;
}

void repair_session_done(ROUTEInCtx *ctx, RouteRepairSession *rsess, GF_Err res_code)
{
	RepairSegmentInfo *rsi = rsess->current_si;
	if (!rsi) return;

	if(rsess->range) {
		//notify routedmx we have received a byte range
		if(rsess->range->done) {
			if (!rsi->removed) {
				gf_route_dmx_patch_frag_info(ctx->route_dmx, rsi->service_id, &rsi->finfo, rsess->range->br_start, rsess->range->br_start + rsess->range->done);
				s32 index = rsess->range->sample_id;
				u32 req = 0;

				if(rsess->range->br_end == rsess->range->br_start + rsess->range->done) {
					GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Successfully repaired data interval [%u, %u[ of object (TSI=%u, TOI=%u) \n", rsess->range->br_start, rsess->range->br_end, rsi->finfo.tsi, rsi->finfo.toi));
					req = 1;
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Failed to repair entire data interval [%u, %u[ of object (TSI=%u, TOI=%u). Only sub-interval [%u, %u[ was received.. \n", rsess->range->br_start, rsess->range->br_end, rsi->finfo.tsi, rsi->finfo.toi, rsess->range->br_start, rsess->range->br_start+rsess->range->done));
				}

				if(index >= 0) {
					rsi->srd[index].reqs_cost -= req;
					rsi->srd[index].bytes_cost -= rsess->range->done;
					routein_repair_propagate_costs(rsi, - (rsess->range->done), - req, index);
				}
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[REPAIR] Failed to repair data interval - Nothing has been received for interval [%u, %u[ of object (TSI=%u, TOI=%u). \n", rsess->range->br_start, rsess->range->br_end, rsi->finfo.tsi, rsi->finfo.toi));
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

	if(ctx->repair == ROUTEIN_REPAIR_ISOBMF && !rsi->state) {
		route_repair_build_ranges_isobmf(ctx, rsi, &(rsi->finfo), rsi->last_pos_repair_top_level);
		if (gf_list_count(rsi->ranges)) return;

		rsi->state = 1;
		route_repair_isobmf_mdat_box(ctx, rsi);
		if (gf_list_count(rsi->ranges)) return;
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

	//make sure we unqueue in order
	if (rsi->tsio) {
		s32 idx = gf_list_find(rsi->tsio->pending_repairs, rsi);
		//not first in list, do not unqueue now
		//this happens if multiple repair sessions are activated, they will likely not finish at the same time
		if (idx > 0) {
			rsi->done = GF_TRUE;
			return;
		}
	}
	routein_delete_range_deps(rsi->srd, rsi->nb_ranges);
	//repair_session_unqueue(ctx, rsess, rsi);
}

void repair_session_run(ROUTEInCtx *ctx, RouteRepairSession *rsess)
{
	GF_Err e;
	RepairSegmentInfo *rsi;
	u32 offset, nb_read, j;

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
			u32 nb_ranges;
			rsi = gf_list_get(ctx->seg_repair_queue, i);
			//over or no longer active
			if (rsi->done || rsi->removed) continue;

			nb_ranges = gf_list_count(rsi->ranges);
			//no more ranges, done with session
			//this happens when enqued repair had no losses when using progressive dispatch
			if (!nb_ranges) {
				rsess->current_si = rsi;
				rsi->pending++;
				repair_session_done(ctx, rsess, GF_OK);
				rsess->current_si = NULL;
				goto restart;
			}
			//if TSIO, always unqueue in order
			if (rsi->tsio) {
				rr = gf_list_get(rsi->ranges, 0);
			} else {
				for (j=0; j<nb_ranges; j++) {
					rr = gf_list_get(rsi->ranges, j);
					//todo check priority
					if (rr) break;
				}
			}
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
