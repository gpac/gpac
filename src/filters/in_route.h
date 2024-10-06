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

#include <gpac/filters.h>
#include <gpac/route.h>
#include <gpac/network.h>
#include <gpac/download.h>
#include <gpac/thread.h>

#ifndef IN_ROUTE_H
#define IN_ROUTE_H

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
	ROUTEIN_REPAIR_ISOBMF,
};

typedef struct _route_repair_seg_info RepairSegmentInfo;

typedef struct
{
	u32 br_start;
	u32 br_end;
	u32 done;
	u32 priority;
	//id of simple in the list of simples; -1 if not used
	s32 sample_id;
} RouteRepairRange;

typedef struct 
{
	char *url;
	Bool accept_ranges, is_up, support_h2;
	u32 nb_req_success, nb_bytes, latency;
} RouteRepairServer;

typedef struct
{
	GF_DownloadSession *dld;
	RepairSegmentInfo *current_si;

	RouteRepairRange *range;
	RouteRepairServer *server;
} RouteRepairSession;

typedef struct
{
	//options
	char *src, *ifce, *odir;
	Bool gcache, kc, skipr, reorder, fullseg, cloop, llmode, dynsel;
	u32 buffer, timeout, stats, max_segs, tsidbg, rtimeout, nbcached, repair;
	u32 max_sess;
	s32 tunein, stsi;
	GF_PropStringList repair_urls;
	
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
	Bool evt_interrupt;

	RouteRepairSession *http_repair_sessions;

	GF_List *seg_repair_queue;
	GF_List *seg_repair_reservoir;
	GF_List *seg_range_reservoir;
	GF_List *repair_servers;

	const char *log_name;
} ROUTEInCtx;


typedef struct __sample_dep
{
	//byte offset in file for the start of the range
	u32 offset;
	//size of the range
	u32 size;
	//ID of the range
	u32 id;
	//ID of the ranges required to process the range
	//if nb_deps<0, dependency is unknown
	//if nb_deps==0, sample is random access
	s32 nb_deps;
	u32 *dep_ids;
	
	s32 nb_rev_deps;
	u32 *rev_dep_ids;
	//0: unknown
	//1: random access
	//2: leaf temporal level, discardable right away
	u8 type;
	//repair costs in terms of number of bytes and network requests
	u32 bytes_cost;
	u32 reqs_cost;

	u32 total_bytes_cost;
	u32 total_reqs_cost;
} SampleRangeDependency;

struct _route_repair_seg_info
{
	//copy of finfo event, valid until associated object is removed
	GF_ROUTEEventFileInfo finfo;
	//copy of filename which is not guaranteed to be kept outside the event callback
	char *filename;
	GF_ROUTEEventType evt;
	u32 service_id;
	Bool removed;
	u32 pending;
	GF_List *ranges;
	u32 nb_errors;

	u32 state; //doing top-level boxes (except mdat; header only) or repairing level 0, level 1, 
	SampleRangeDependency* srd; // valid once all top headers moof are ok (array of frames)
	u32 nb_ranges; // number of sample ranges in srd; 0 if srd is not valid

	u32 last_pos_repair_top_level; 
};


void routein_repair_mark_file(ROUTEInCtx *ctx, u32 service_id, const char *filename, Bool is_delete);
void routein_queue_repair(ROUTEInCtx *ctx, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo);

void routein_on_event_file(ROUTEInCtx *ctx, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo, Bool is_defer_repair, Bool drop_if_first);

//return GF_EOS if nothing active, GF_OK otherwise
GF_Err routein_do_repair(ROUTEInCtx *ctx);

#endif /* GPAC_DISABLE_ROUTE */

#endif //#define IN_ROUTE_H

