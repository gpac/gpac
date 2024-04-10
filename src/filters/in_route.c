	/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2023
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

#include <gpac/filters.h>
#include <gpac/route.h>
#include <gpac/network.h>
#include <gpac/download.h>
#include <gpac/thread.h>

#ifndef GPAC_DISABLE_ROUTE

typedef struct
{
	u32 sid;
	u32 tsi;
	GF_FilterPid *opid;
} TSI_Output;

typedef struct
{
	GF_FilterPid *opid;
	char *seg_name;
} SegInfo;

enum
{
	ROUTEIN_REPAIR_NO = 0,
	ROUTEIN_REPAIR_SIMPLE,
	ROUTEIN_REPAIR_STRICT,
	ROUTEIN_REPAIR_FULL,
};

typedef struct
{
	//options
	char *src, *ifce, *odir, *repair_url;
	Bool gcache, kc, skipr, reorder, fullseg;
	u32 buffer, timeout, stats, max_segs, tsidbg, rtimeout, nbcached, repair;
	s32 tunein, stsi;
	
	//internal
	GF_Filter *filter;
	GF_DownloadManager *dm;

	char *clock_init_seg;
	GF_ROUTEDmx *route_dmx;
	u32 tune_service_id;

	u32 sync_tsi, last_toi;

	u32 start_time, tune_time, last_timeout;
	GF_FilterPid *opid;
	GF_List *tsi_outs;

	u32 nb_stats;
	GF_List *received_seg_names;

	u32 nb_playing;
	Bool initial_play_forced;
} ROUTEInCtx;


static Bool routein_repair_segment_ts(ROUTEInCtx *ctx, GF_ROUTEEventFileInfo *finfo)
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
	finfo->blob->flags = 0;
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

static void routein_repair_segment_isobmf(ROUTEInCtx *ctx, GF_ROUTEEventFileInfo *finfo)
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
			finfo->blob->flags = 0;
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
	finfo->blob->flags = 0;
}

static Bool routein_repair_segment(ROUTEInCtx *ctx, GF_ROUTEEventFileInfo *finfo)
{
	Bool drop_if_first = GF_FALSE;
	GF_Err e;

	if (ctx->repair==ROUTEIN_REPAIR_NO)
		return GF_FALSE;

	if (finfo->blob->mx)
		gf_mx_p(finfo->blob->mx);
	
	if(ctx->repair==ROUTEIN_REPAIR_FULL && ctx->repair_url) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Initiating repair procedure for object (TSI=%u, TOI=%u) using repair URL: %s \n", finfo->tsi, finfo->toi, ctx->repair_url));
		// create unicast connexion
		GF_DownloadManager *dm = gf_filter_get_download_manager(ctx->filter);
		char *url = gf_url_concatenate(ctx->repair_url, finfo->filename);
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] url = %s \n", url));

		GF_DownloadSession *dsess = gf_dm_sess_new(dm, url, GF_NETIO_SESSION_NOT_CACHED, 0, NULL, &e);
		
		
		
	} else {
		if (strstr(finfo->filename, ".ts") || strstr(finfo->filename, ".m2ts")) {
			drop_if_first = routein_repair_segment_ts(ctx, finfo);
		} else {
			routein_repair_segment_isobmf(ctx, finfo);
		}
	}

	if (finfo->blob->mx)
		gf_mx_v(finfo->blob->mx);

	return drop_if_first;
}


static GF_FilterProbeScore routein_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "atsc://", 7)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "route://", 8)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}


static void routein_finalize(GF_Filter *filter)
{
	ROUTEInCtx *ctx = gf_filter_get_udta(filter);

#ifdef GPAC_ENABLE_COVERAGE
    if (gf_sys_is_cov_mode())
        gf_route_dmx_purge_objects(ctx->route_dmx, 1);
#endif
    
    if (ctx->clock_init_seg) gf_free(ctx->clock_init_seg);
	if (ctx->route_dmx) gf_route_dmx_del(ctx->route_dmx);

	if (ctx->tsi_outs) {
		while (gf_list_count(ctx->tsi_outs)) {
			TSI_Output *tsio = gf_list_pop_back(ctx->tsi_outs);
			gf_free(tsio);
		}
		gf_list_del(ctx->tsi_outs);
	}
	if (ctx->received_seg_names) {
		while (gf_list_count(ctx->received_seg_names)) {
			if (ctx->odir) {
				char *filedel = gf_list_pop_back(ctx->received_seg_names);
				if (filedel) gf_free(filedel);
			} else {
				SegInfo *si = gf_list_pop_back(ctx->received_seg_names);
				gf_free(si->seg_name);
				gf_free(si);
			}
		}
		gf_list_del(ctx->received_seg_names);
	}
}

static void push_seg_info(ROUTEInCtx *ctx, GF_FilterPid *pid, GF_ROUTEEventFileInfo *finfo)
{
    if (ctx->received_seg_names) {
        SegInfo *si;
        GF_SAFEALLOC(si, SegInfo);
        if (!si) return;
        si->opid = pid;
        si->seg_name = gf_strdup(finfo->filename);
        gf_list_add(ctx->received_seg_names, si);
    }
    while (gf_list_count(ctx->received_seg_names) > ctx->max_segs) {
        GF_FilterEvent evt;
        SegInfo *si = gf_list_pop_front(ctx->received_seg_names);
        GF_FEVT_INIT(evt, GF_FEVT_FILE_DELETE, si->opid);
        evt.file_del.url = si->seg_name;
        gf_filter_pid_send_event(si->opid, &evt);
        gf_free(si->seg_name);
        gf_free(si);
    }
}

static void routein_send_file(ROUTEInCtx *ctx, u32 service_id, GF_ROUTEEventFileInfo *finfo, u32 evt_type)
{
	if (!ctx->kc || !(finfo->blob->flags & GF_BLOB_CORRUPTED)) {
		u8 *output;
		char *ext;
		GF_FilterPid *pid, **p_pid;
		GF_FilterPacket *pck;
		TSI_Output *tsio = NULL;

		p_pid = &ctx->opid;
		if (finfo->tsi && ctx->stsi) {
			u32 i, count = gf_list_count(ctx->tsi_outs);
			for (i=0; i<count; i++) {
				tsio = gf_list_get(ctx->tsi_outs, i);
				if ((tsio->sid==service_id) && (tsio->tsi==finfo->tsi)) {
					break;
				}
				tsio=NULL;
			}
			if (!tsio) {
				GF_SAFEALLOC(tsio, TSI_Output);
				if (!tsio) return;

				tsio->tsi = finfo->tsi;
				tsio->sid = service_id;
				gf_list_add(ctx->tsi_outs, tsio);
			}
			p_pid = &tsio->opid;

			if ((evt_type==GF_ROUTE_EVT_FILE) || (evt_type==GF_ROUTE_EVT_MPD)) {
				if (ctx->skipr && !finfo->updated) return;
			}
		}
		pid = *p_pid;

		if (!pid) {
			pid = gf_filter_pid_new(ctx->filter);
			(*p_pid) = pid;
			gf_filter_pid_set_property(pid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE));
		}
		gf_filter_pid_set_property(pid, GF_PROP_PID_ID, &PROP_UINT(tsio ? tsio->tsi : service_id));
		gf_filter_pid_set_property(pid, GF_PROP_PID_SERVICE_ID, &PROP_UINT(service_id));
		gf_filter_pid_set_property(pid, GF_PROP_PID_URL, &PROP_STRING(finfo->filename));
		ext = gf_file_ext_start(finfo->filename);
		gf_filter_pid_set_property(pid, GF_PROP_PID_FILE_EXT, &PROP_STRING(ext ? (ext+1) : "*" ));

		pck = gf_filter_pck_new_alloc(pid, finfo->blob->size, &output);
		if (pck) {
			memcpy(output, finfo->blob->data, finfo->blob->size);
			if (finfo->blob->flags & GF_BLOB_CORRUPTED) gf_filter_pck_set_corrupted(pck, GF_TRUE);
			gf_filter_pck_send(pck);
		}
		
        if (ctx->max_segs && (evt_type==GF_ROUTE_EVT_DYN_SEG))
            push_seg_info(ctx, pid, finfo);
	}

	while (gf_route_dmx_get_object_count(ctx->route_dmx, service_id)>1) {
		if (! gf_route_dmx_remove_first_object(ctx->route_dmx, service_id))
			break;
	}
}

static void routein_write_to_disk(ROUTEInCtx *ctx, u32 service_id, GF_ROUTEEventFileInfo *finfo, u32 evt_type)
{
	char szPath[GF_MAX_PATH];
	FILE *out;
	if (!finfo->blob)
		return;

	if ((finfo->blob->flags & GF_BLOB_CORRUPTED) && !ctx->kc)
		return;
	
	sprintf(szPath, "%s/service%d/%s", ctx->odir, service_id, finfo->filename);

	out = gf_fopen(szPath, "wb");
	if (!out) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d failed to create MPD file %s\n", service_id, szPath ));
	} else {
		u32 bytes = (u32) gf_fwrite(finfo->blob->data, finfo->blob->size, out);
		gf_fclose(out);
		if (bytes != finfo->blob->size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d failed to write file %s:  %d written for %d total\n", service_id, finfo->filename, bytes, finfo->blob->size));
		}
	}
	while (gf_route_dmx_get_object_count(ctx->route_dmx, service_id)>1) {
		if (! gf_route_dmx_remove_first_object(ctx->route_dmx, service_id))
			break;
	}

	if (ctx->max_segs && (evt_type==GF_ROUTE_EVT_DYN_SEG)) {
		gf_list_add(ctx->received_seg_names, gf_strdup(szPath));

		while (gf_list_count(ctx->received_seg_names) > ctx->max_segs) {
			char *filedel = gf_list_pop_front(ctx->received_seg_names);
			if (filedel) {
				gf_file_delete(filedel);
				gf_free(filedel);
			}
		}
	}
}



void routein_on_event(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo)
{
	char szPath[GF_MAX_PATH];
	ROUTEInCtx *ctx = (ROUTEInCtx *)udta;
	u32 nb_obj;
	Bool is_init = GF_TRUE;
	Bool drop_if_first = GF_FALSE;
	Bool is_loop = GF_FALSE;
	DownloadedCacheEntry cache_entry;

	//events without finfo
	if (evt==GF_ROUTE_EVT_SERVICE_FOUND) {
		if (!ctx->tune_time) ctx->tune_time = gf_sys_clock();
		return;
	}
	if (evt==GF_ROUTE_EVT_SERVICE_SCAN) {
		if (ctx->tune_service_id && !gf_route_dmx_find_atsc3_service(ctx->route_dmx, ctx->tune_service_id)) {

			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Asked to tune to service %d but no such service, tuning to first one\n", ctx->tune_service_id));

			ctx->tune_service_id = 0;
			gf_route_atsc3_tune_in(ctx->route_dmx, (u32) -2, GF_TRUE);
		}
		return;
	}
	if (!finfo)
		return;

	//events without finfo->blob
	if (evt==GF_ROUTE_EVT_FILE_DELETE) {
		if (ctx->gcache) {
			sprintf(szPath, "http://groute/service%d/%s", evt_param, finfo->filename);
			gf_dm_add_cache_entry(ctx->dm, szPath, NULL, 0, 0, "video/mp4", GF_FALSE, 0);
		}
		return;
	}
	
	if (!finfo->blob)
		return;

	cache_entry = finfo->udta;
	szPath[0] = 0;
	switch (evt) {
	case GF_ROUTE_EVT_MPD:
		if (!ctx->tune_time) ctx->tune_time = gf_sys_clock();
			
		if (ctx->odir) {
			routein_write_to_disk(ctx, evt_param, finfo, evt);
			break;
		}
		if (!ctx->gcache) {
			routein_send_file(ctx, evt_param, finfo, evt);
			break;
		}

		if (!ctx->opid) {
			ctx->opid = gf_filter_pid_new(ctx->filter);
			gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_FILE));
		}
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ID, &PROP_UINT(evt_param));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SERVICE_ID, &PROP_UINT(evt_param));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_FILE_EXT, &PROP_STRING("mpd"));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_MIME, &PROP_STRING("application/dash+xml"));

		sprintf(szPath, "http://groute/service%d/%s", evt_param, finfo->filename);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_REDIRECT_URL, &PROP_STRING(szPath));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_URL, &PROP_STRING(szPath));

		cache_entry = gf_dm_add_cache_entry(ctx->dm, szPath, finfo->blob, 0, 0, "application/dash+xml", GF_TRUE, 0);

		sprintf(szPath, "x-route: %d\r\n", evt_param);
		gf_dm_force_headers(ctx->dm, cache_entry, szPath);
		gf_route_dmx_set_service_udta(ctx->route_dmx, evt_param, cache_entry);

		ctx->sync_tsi = 0;
		ctx->last_toi = 0;
		if (ctx->clock_init_seg) gf_free(ctx->clock_init_seg);
		ctx->clock_init_seg = NULL;
		ctx->tune_service_id = evt_param;
		break;
	case GF_ROUTE_EVT_DYN_SEG:

		//corrupted file, try to repair
		if (finfo->blob->flags & GF_BLOB_CORRUPTED) {
			drop_if_first = routein_repair_segment(ctx, finfo);
		}
			
		if (ctx->odir) {
			routein_write_to_disk(ctx, evt_param, finfo, evt);
			break;
		}
		if (!ctx->gcache) {
			routein_send_file(ctx, evt_param, finfo, evt);
			break;
		}
		//fallthrough

    case GF_ROUTE_EVT_DYN_SEG_FRAG:
        //for now we only push complete files
        if (!ctx->gcache) {
            break;
        }

#if 0
			//couldn't repair or this is a fragment
        if ((finfo->blob->flags & GF_BLOB_CORRUPTED) && !ctx->kc) {
			
            //force updating the cache entry since we may have reallocated the data buffer
            sprintf(szPath, "http://groute/service%d/%s", evt_param, finfo->filename);
            if (evt==GF_ROUTE_EVT_DYN_SEG_FRAG) {
                cache_entry = gf_dm_add_cache_entry(ctx->dm, szPath, finfo->blob, 0, 0, "video/mp4", GF_FALSE, finfo->download_ms);
            } else {
                if (ctx->fullseg)
                    break;
                cache_entry = gf_dm_add_cache_entry(ctx->dm, szPath, finfo->blob, 0, 0, "video/mp4", GF_FALSE, finfo->download_ms);
            }
            //don't break yet, we want to signal the clock
        }
#endif
			
		if (!ctx->clock_init_seg
			//if full seg push of previsously advertized init, reset x-route-ll header
			|| ((evt==GF_ROUTE_EVT_DYN_SEG) && !strcmp(ctx->clock_init_seg, finfo->filename))
		) {
			DownloadedCacheEntry mpd_cache_entry = gf_route_dmx_get_service_udta(ctx->route_dmx, evt_param);
			if (mpd_cache_entry) {
				if (!ctx->clock_init_seg)
					ctx->clock_init_seg = gf_strdup(finfo->filename);
				sprintf(szPath, "x-route: %d\r\nx-route-first-seg: %s\r\n", evt_param, ctx->clock_init_seg);
				if (evt==GF_ROUTE_EVT_DYN_SEG_FRAG)
					strcat(szPath, "x-route-ll: yes\r\n");
				gf_dm_force_headers(ctx->dm, mpd_cache_entry, szPath);
				szPath[0] = 0;
			}
		}

		if ((finfo->blob->flags & GF_BLOB_CORRUPTED) && !ctx->kc)
            break;
            
		is_init = GF_FALSE;
		if (!ctx->sync_tsi) {
			ctx->sync_tsi = finfo->tsi;
			ctx->last_toi = finfo->toi;
			if (drop_if_first) {
				break;
			}
		} else if (ctx->sync_tsi == finfo->tsi) {
			if (ctx->last_toi > finfo->toi) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Loop detected on service %d for TSI %u: prev TOI %u this toi %u\n", ctx->tune_service_id, finfo->tsi, ctx->last_toi, finfo->toi));

				gf_route_dmx_purge_objects(ctx->route_dmx, evt_param);
				is_loop = GF_TRUE;
				if (cache_entry) {
					if (ctx->clock_init_seg) gf_free(ctx->clock_init_seg);
					ctx->clock_init_seg = gf_strdup(finfo->filename);
					sprintf(szPath, "x-route: %d\r\nx-route-first-seg: %s\r\nx-route-loop: yes\r\n", evt_param, ctx->clock_init_seg);
					gf_dm_force_headers(ctx->dm, cache_entry, szPath);
					szPath[0] = 0;
				}
			}
			ctx->last_toi = finfo->toi;
		}
		//fallthrough

	case GF_ROUTE_EVT_FILE:

		if (ctx->odir) {
			routein_write_to_disk(ctx, evt_param, finfo, evt);
			break;
		}
		if (!ctx->gcache) {
			routein_send_file(ctx, evt_param, finfo, evt);
			break;
		}

		if ((finfo->blob->flags & GF_BLOB_CORRUPTED) && !ctx->kc) return;


		if (!cache_entry) {
			sprintf(szPath, "http://groute/service%d/%s", evt_param, finfo->filename);
			
			//we copy over the init segment, but only share the data pointer for segments
			cache_entry = gf_dm_add_cache_entry(ctx->dm, szPath, finfo->blob, 0, 0, "video/mp4", is_init ? GF_TRUE : GF_FALSE, finfo->download_ms);
			if (cache_entry) {
				gf_dm_force_headers(ctx->dm, cache_entry, "x-route: yes\r\n");
				finfo->udta = cache_entry;
			}
		}
			
        if (evt==GF_ROUTE_EVT_DYN_SEG_FRAG) {
            GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Pushing fragment from file %s to cache\n", finfo->filename));
			break;
        }
			
			
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Pushing file %s to cache\n", finfo->filename));
		if (ctx->max_segs && (evt==GF_ROUTE_EVT_DYN_SEG))
			push_seg_info(ctx, ctx->opid, finfo);

		if (is_loop) break;

		//cf routein_local_cache_probe
		gf_filter_lock(ctx->filter, GF_TRUE);
		nb_obj = gf_route_dmx_get_object_count(ctx->route_dmx, evt_param);
		while (nb_obj > ctx->nbcached) {
			if (!gf_route_dmx_remove_first_object(ctx->route_dmx, evt_param))
				break;
			nb_obj = gf_route_dmx_get_object_count(ctx->route_dmx, evt_param);
		}
		gf_filter_lock(ctx->filter, GF_FALSE);
		break;
	default:
		break;
	}
}

static Bool routein_local_cache_probe(void *par, char *url, Bool is_destroy)
{
	ROUTEInCtx *ctx = (ROUTEInCtx *)par;
	u32 sid=0;
	char *subr;
	if (strncmp(url, "http://groute/service", 21)) return GF_FALSE;

	subr = strchr(url+21, '/');
	subr[0] = 0;
	sid = atoi(url+21);
	subr[0] = '/';
	//this is not a thread-safe callback (typically called from httpin filter)
	gf_filter_lock(ctx->filter, GF_TRUE);
	if (is_destroy) {
		gf_route_dmx_remove_object_by_name(ctx->route_dmx, sid, subr+1, GF_TRUE);
	} else if (sid && (sid != ctx->tune_service_id)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Request on service %d but tuned on service %d, retuning\n", sid, ctx->tune_service_id));
		ctx->tune_service_id = sid;
		ctx->sync_tsi = 0;
		ctx->last_toi = 0;
		if (ctx->clock_init_seg) gf_free(ctx->clock_init_seg);
		ctx->clock_init_seg = NULL;
        gf_route_atsc3_tune_in(ctx->route_dmx, sid, GF_TRUE);
	} else {
		//mark object as in-use to prevent it from being discarded
		gf_route_dmx_force_keep_object_by_name(ctx->route_dmx, sid, subr+1);
	}
	gf_filter_lock(ctx->filter, GF_FALSE);
	return GF_TRUE;
}

static void routein_set_eos(GF_Filter *filter)
{
	u32 i, nb_out = gf_filter_get_opid_count(filter);
	for (i=0; i<nb_out; i++) {
		GF_FilterPid *opid = gf_filter_get_opid(filter, i);
		if (opid) gf_filter_pid_set_eos(opid);
	}
}

static GF_Err routein_process(GF_Filter *filter)
{
	ROUTEInCtx *ctx = gf_filter_get_udta(filter);

	if (!ctx->nb_playing)
		return GF_EOS;

	while (1) {
		GF_Err e = gf_route_dmx_process(ctx->route_dmx);
		if (e == GF_IP_NETWORK_EMPTY) {
			if (ctx->tune_time) {
				if (!ctx->last_timeout) ctx->last_timeout = gf_sys_clock();
				else {
					u32 diff = gf_sys_clock() - ctx->last_timeout;
					if (diff > ctx->timeout) {
						GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] No data for %d ms, aborting\n", diff));
						routein_set_eos(filter);
						return GF_EOS;
					}
				}
			}
			gf_filter_ask_rt_reschedule(filter, 1000);
			break;
		} else if (!e) {
			ctx->last_timeout = 0;
		} else if (e==GF_EOS) {
			routein_set_eos(filter);
			return e;
		} else {
			break;
		}
	}
	if (!ctx->tune_time) {
	 	u32 diff = gf_sys_clock() - ctx->start_time;
	 	if (diff>ctx->timeout) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] No data for %d ms, aborting\n", diff));
			gf_filter_setup_failure(filter, GF_SERVICE_ERROR);
			routein_set_eos(filter);
			return GF_EOS;
		}
	}

	if (ctx->stats) {
		u32 now = gf_sys_clock() - ctx->start_time;
		if (now >= ctx->nb_stats*ctx->stats) {
			ctx->nb_stats+=1;
			if (gf_filter_reporting_enabled(filter)) {
				Double rate=0.0;
				char szRpt[1024];

				u64 st = gf_route_dmx_get_first_packet_time(ctx->route_dmx);
				u64 et = gf_route_dmx_get_last_packet_time(ctx->route_dmx);
				u64 nb_pck = gf_route_dmx_get_nb_packets(ctx->route_dmx);
				u64 nb_bytes = gf_route_dmx_get_recv_bytes(ctx->route_dmx);

				et -= st;
				if (et) {
					rate = (Double)nb_bytes*8;
					rate /= et;
				}
				sprintf(szRpt, "[%us] "LLU" bytes "LLU" packets in "LLU" ms rate %.02f mbps", now/1000, nb_bytes, nb_pck, et/1000, rate);
				gf_filter_update_status(filter, 0, szRpt);
			}
		}
	}

	return GF_OK;
}


static GF_Err routein_initialize(GF_Filter *filter)
{
	Bool is_atsc = GF_TRUE;
	ROUTEInCtx *ctx = gf_filter_get_udta(filter);
	ctx->filter = filter;

	if (!ctx->src) return GF_BAD_PARAM;
	if (!strncmp(ctx->src, "route://", 8)) {
		is_atsc = GF_FALSE;
	} else if (strcmp(ctx->src, "atsc://"))
		return GF_BAD_PARAM;

	if (ctx->odir) {
		ctx->gcache = GF_FALSE;
	}

	if (ctx->gcache) {
		ctx->dm = gf_filter_get_download_manager(filter);
		if (!ctx->dm) return GF_SERVICE_ERROR;
		gf_dm_set_localcache_provider(ctx->dm, routein_local_cache_probe, ctx);
	} else {
		//for now progressive dispatch is only possible when populating cache
		ctx->fullseg = GF_TRUE;
	}
	if (!ctx->nbcached)
		ctx->nbcached = 1;

	if (is_atsc) {
		ctx->route_dmx = gf_route_atsc_dmx_new_ex(ctx->ifce, ctx->buffer, gf_filter_get_netcap_id(filter), routein_on_event, ctx);
	} else {
		char *sep, *root;
		u32 port;
		sep = strrchr(ctx->src+8, ':');
		if (!sep) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Missing port number\n"));
			return GF_BAD_PARAM;
		}
		sep[0] = 0;
		root = strchr(sep+1, '/');
		if (root) root[0] = 0;
		port = atoi(sep+1);
		if (root) root[0] = '/';

		if (!gf_sk_is_multicast_address(ctx->src+8)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] %s is not a multicast address\n", ctx->src));
			sep[0] = ':';
			return GF_BAD_PARAM;
		}
		ctx->route_dmx = gf_route_dmx_new_ex(ctx->src+8, port, ctx->ifce, ctx->buffer, gf_filter_get_netcap_id(filter), routein_on_event, ctx);
		sep[0] = ':';
	}
	if (!ctx->route_dmx) return GF_SERVICE_ERROR;
	
	gf_route_set_allow_progressive_dispatch(ctx->route_dmx, !ctx->fullseg);

	gf_route_set_reorder(ctx->route_dmx, ctx->reorder, ctx->rtimeout);

	if (ctx->tsidbg) {
		gf_route_dmx_debug_tsi(ctx->route_dmx, ctx->tsidbg);
	}

	if (ctx->tunein>0) ctx->tune_service_id = ctx->tunein;

	if (is_atsc) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] ATSC 3.0 Tunein started\n"));
		if (ctx->tune_service_id)
            gf_route_atsc3_tune_in(ctx->route_dmx, ctx->tune_service_id, GF_FALSE);
		else
            gf_route_atsc3_tune_in(ctx->route_dmx, (u32) ctx->tunein, GF_TRUE);
	}

	ctx->start_time = gf_sys_clock();

	if (ctx->stsi) ctx->tsi_outs = gf_list_new();
	if (ctx->max_segs)
		ctx->received_seg_names = gf_list_new();

	ctx->nb_playing = 1;
	ctx->initial_play_forced = GF_TRUE;
	return GF_OK;
}

static Bool routein_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	ROUTEInCtx *ctx = gf_filter_get_udta(filter);
	if (evt->base.type==GF_FEVT_PLAY) {
		if (!ctx->initial_play_forced)
			ctx->nb_playing++;
		ctx->initial_play_forced = GF_FALSE;
	} else {
		ctx->nb_playing--;
	}
	return GF_TRUE;
}

#define OFFS(_n)	#_n, offsetof(ROUTEInCtx, _n)
static const GF_FilterArgs ROUTEInArgs[] =
{
	{ OFFS(src), "URL of source content", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(ifce), "default interface to use for multicast. If NULL, the default system interface will be used", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(gcache), "indicate the files should populate GPAC HTTP cache", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(tunein), "service ID to bootstrap on for ATSC 3.0 mode (0 means tune to no service, -1 tune all services -2 means tune on first service found)", GF_PROP_SINT, "-2", NULL, 0},
	{ OFFS(buffer), "receive buffer size to use in bytes", GF_PROP_UINT, "0x80000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timeout), "timeout in ms after which tunein fails", GF_PROP_UINT, "5000", NULL, 0},
    { OFFS(nbcached), "number of segments to keep in cache per service", GF_PROP_UINT, "8", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(kc), "keep corrupted file", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(skipr), "skip repeated files (ignored in cache mode)", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(stsi), "define one output PID per tsi/serviceID (ignored in cache mode)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(stats), "log statistics at the given rate in ms (0 disables stats)", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(tsidbg), "gather only objects with given TSI (debug)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(max_segs), "maximum number of segments to keep on disk", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(odir), "output directory for standalone mode", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(reorder), "ignore order flag in ROUTE/LCT packets, avoiding considering object done when TOI changes", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(rtimeout), "default timeout in ms to wait when gathering out-of-order packets", GF_PROP_UINT, "5000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(fullseg), "only dispatch full segments in cache mode (always true for other modes)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(repair), "repair mode for corrupted files\n"
		"- no: no repair is performed\n"
		"- simple: simple repair is performed (incomplete `mdat` boxes will be kept)\n"
		"- strict: incomplete mdat boxes will be lost as well as preceding `moof` boxes\n"
		"- full: HTTP-based repair, not yet implemented"
		, GF_PROP_UINT, "simple", "no|simple|strict|full", GF_FS_ARG_HINT_EXPERT},
	{ OFFS(repair_url), "repair url", GF_PROP_NAME, NULL, NULL, 0},

	{0}
};

static const GF_FilterCapability ROUTEInCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};

GF_FilterRegister ROUTEInRegister = {
	.name = "routein",
	GF_FS_SET_DESCRIPTION("ROUTE input")
#ifndef GPAC_DISABLE_DOC
	.help = "This filter is a receiver for ROUTE sessions (ATSC 3.0 and generic ROUTE).\n"
	"- ATSC 3.0 mode is identified by the URL `atsc://`.\n"
	"- Generic ROUTE mode is identified by the URL `route://IP:PORT`.\n"
	"\n"
	"The filter can work in cached mode, source mode or standalone mode.\n"
	"# Cached mode\n"
	"The cached mode is the default filter behavior. It populates GPAC HTTP Cache with the received files, using `http://groute/serviceN/` as service root, `N being the ROUTE service ID.\n"
	"In cached mode, repeated files are always pushed to cache.\n"
	"The maximum number of media segment objects in cache per service is defined by [-nbcached](); this is a safety used to force object removal in case DASH client timing is wrong and some files are never requested at cache level.\n"
	"  \n"
	"The cached MPD is assigned the following headers:\n"
	"- `x-route`: integer value, indicates the ROUTE service ID.\n"
	"- `x-route-first-seg`: string value, indicates the name of the first segment (completely or currently being) retrieved from the broadcast.\n"
    "- `x-route-ll`: boolean value, if yes indicates that the indicated first segment is currently being received (low latency signaling).\n"
    "- `x-route-loop`: boolean value, if yes indicates a loop in the service has been detected (usually pcap replay loop).\n"
	"  \n"
	"The cached files are assigned the following headers:\n"
	"- `x-route`: boolean value, if yes indicates the file comes from an ROUTE session.\n"
	"\n"
	"If [-max_segs]() is set, file deletion event will be triggered in the filter chain.\n"
	"\n"
	"# Source mode\n"
	"In source mode, the filter outputs files on a single output PID of type `file`. "
	"The files are dispatched once fully received, the output PID carries a sequence of complete files. Repeated files are not sent unless requested.\n"
	"If needed, one PID per TSI can be used rather than a single PID. This avoids mixing files of different mime types on the same PID (e.g. HAS manifest and ISOBMFF).\n"
	"EX gpac -i atsc://gcache=false -o $ServiceID$/$File$:dynext\n"
	"This will grab the files and forward them as output PIDs, consumed by the [fout](fout) filter.\n"
	"\n"
	"If [-max_segs]() is set, file deletion event will be triggered in the filter chain.\n"
	"\n"
	"# Standalone mode\n"
	"In standalone mode, the filter does not produce any output PID and writes received files to the [-odir]() directory.\n"
	"EX gpac -i atsc://:odir=output\n"
	"This will grab the files and write them to `output` directory.\n"
	"\n"
	"If [-max_segs]() is set, old files will be deleted.\n"
	"\n"
	"# File Repair\n"
	"In case of losses or incomplete segment reception (during tune-in), the files are patched as follows:\n"
	"- MPEG-2 TS: all lost ranges are adjusted to 188-bytes boundaries, and transformed into NULL TS packets.\n"
	"- ISOBMFF: all top-level boxes are scanned, and incomplete boxes are transformed in `free` boxes, except mdat kept as is if [-repair]() is set to simple.\n"
	"\n"
	"If [-kc]() option is set, corrupted files will be kept. If [-fullseg]() is not set and files are only partially received, they will be kept.\n"
	"\n"
	"# Interface setup\n"
	"On some systems (OSX), when using VM packet replay, you may need to force multicast routing on your local interface.\n"
	"For ATSC, you will have to do this for the base signaling multicast (224.0.23.60):\n"
	"EX route add -net 224.0.23.60/32 -interface vboxnet0\n"
	"Then for each ROUTE service in the multicast:\n"
	"EX route add -net 239.255.1.4/32 -interface vboxnet0\n"
	"",
#endif //GPAC_DISABLE_DOC
	.private_size = sizeof(ROUTEInCtx),
	.args = ROUTEInArgs,
	.initialize = routein_initialize,
	.finalize = routein_finalize,
	SETCAPS(ROUTEInCaps),
	.process = routein_process,
	.process_event = routein_process_event,
	.probe_url = routein_probe_url
};

const GF_FilterRegister *routein_register(GF_FilterSession *session)
{
	if (gf_opts_get_bool("temp", "get_proto_schemes")) {
		gf_opts_set_key("temp_in_proto", ROUTEInRegister.name, "atsc,route");
	}
	return &ROUTEInRegister;
}

#else

const GF_FilterRegister *routein_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /* GPAC_DISABLE_ROUTE */

