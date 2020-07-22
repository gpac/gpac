	/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2020
 *					All rights reserved
 *
 *  This file is part of GPAC / ATSC input filter
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
#include <gpac/atsc.h>

#ifndef GPAC_DISABLE_ATSC

typedef struct
{
	u32 sid;
	u32 tsi;
	GF_FilterPid *opid;
	Bool init_sent;
} TSI_Output;

typedef struct
{
	GF_FilterPid *opid;
	char *seg_name;
} SegInfo;

typedef struct
{
	//options
	char *src, *ifce, *odir;
	Bool gcache, kc, sr, reorder;
	u32 buffer, timeout, stats, max_segs, tsidbg, rtimeout;
	s32 tunein, stsi;
	
	//internal
	GF_Filter *filter;
	GF_DownloadManager *dm;

	char *clock_init_seg;
	GF_ATSCDmx *atsc_dmx;
	u32 tune_service_id;

	u32 sync_tsi, last_toi;

	u32 start_time, tune_time;
	GF_FilterPid *opid;
	GF_List *tsi_outs;

	u32 nb_stats;
	GF_List *received_seg_names;
} ATSCInCtx;


static GF_FilterProbeScore atscin_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "atsc://", 7)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}


static void atscin_finalize(GF_Filter *filter)
{
	ATSCInCtx *ctx = gf_filter_get_udta(filter);
	if (ctx->clock_init_seg) gf_free(ctx->clock_init_seg);
	if (ctx->atsc_dmx) gf_atsc3_dmx_del(ctx->atsc_dmx);

	if (ctx->tsi_outs) {
		while (gf_list_count(ctx->tsi_outs)) {
			TSI_Output *tsio = gf_list_pop_back(ctx->tsi_outs);
			gf_free(tsio);
		}
		gf_list_del(ctx->tsi_outs);
	}
	if (ctx->received_seg_names) {
		while (gf_list_count(ctx->received_seg_names)) {
			SegInfo *si = gf_list_pop_back(ctx->received_seg_names);
			gf_free(si->seg_name);
			gf_free(si);
		}
		gf_list_del(ctx->received_seg_names);
	}
}

static void atscin_send_file(ATSCInCtx *ctx, u32 service_id, GF_ATSCEventFileInfo *finfo, u32 evt_type)
{
	if (!ctx->kc || !finfo->corrupted) {
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

			if ((evt_type==GF_ATSC_EVT_INIT_SEG) || (evt_type==GF_ATSC_EVT_MPD)) {
				if (ctx->sr && tsio->init_sent) return;
				tsio->init_sent = GF_TRUE;
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

		pck = gf_filter_pck_new_alloc(pid, finfo->size, &output);
		memcpy(output, finfo->data, finfo->size);
		if (finfo->corrupted) gf_filter_pck_set_corrupted(pck, GF_TRUE);
		gf_filter_pck_send(pck);

		if (ctx->received_seg_names && (evt_type==GF_ATSC_EVT_SEG)) {
			SegInfo *si;
			GF_SAFEALLOC(si, SegInfo);
			if (!si) return;
			si->opid = pid;
			si->seg_name = gf_strdup(finfo->filename);
			gf_list_add(ctx->received_seg_names, si);
		}
	}

	while (gf_atsc3_dmx_get_object_count(ctx->atsc_dmx, service_id)>1) {
		if (! gf_atsc3_dmx_remove_first_object(ctx->atsc_dmx, service_id))
			break;
	}

	if (ctx->max_segs) {
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
}

void atscin_on_event(void *udta, GF_ATSCEventType evt, u32 evt_param, GF_ATSCEventFileInfo *finfo)
{
	char szPath[GF_MAX_PATH];
	ATSCInCtx *ctx = (ATSCInCtx *)udta;
	u32 nb_obj;
	Bool is_init = GF_TRUE;
	Bool is_loop = GF_FALSE;
	DownloadedCacheEntry cache_entry;

	switch (evt) {
	case GF_ATSC_EVT_SERVICE_FOUND:
		if (!ctx->tune_time) ctx->tune_time = gf_sys_clock();

		break;
	case GF_ATSC_EVT_SERVICE_SCAN:
		if (ctx->tune_service_id && !gf_atsc3_dmx_find_service(ctx->atsc_dmx, ctx->tune_service_id)) {

			GF_LOG(GF_LOG_ERROR, GF_LOG_ATSC, ("[ATSCDmx] Asked to tune to service %d but no such service, tuning to first one\n", ctx->tune_service_id));

			ctx->tune_service_id = 0;
			gf_atsc3_tune_in(ctx->atsc_dmx, (u32) -2, GF_TRUE);
		}
		break;
	case GF_ATSC_EVT_MPD:

		if (!ctx->gcache) {
			atscin_send_file(ctx, evt_param, finfo, evt);
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

		sprintf(szPath, "http://gpatsc/service%d/%s", evt_param, finfo->filename);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_REDIRECT_URL, &PROP_STRING(szPath));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_URL, &PROP_STRING(szPath));

		cache_entry = gf_dm_add_cache_entry(ctx->dm, szPath, finfo->data, finfo->size, 0, 0, "application/dash+xml", GF_TRUE, 0);

		sprintf(szPath, "x-dash-atsc: %d\r\n", evt_param);
		gf_dm_force_headers(ctx->dm, cache_entry, szPath);
		gf_atsc3_dmx_set_service_udta(ctx->atsc_dmx, evt_param, cache_entry);

		ctx->sync_tsi = 0;
		ctx->last_toi = 0;
		if (ctx->clock_init_seg) gf_free(ctx->clock_init_seg);
		ctx->clock_init_seg = NULL;
		ctx->tune_service_id = evt_param;
		break;
	case GF_ATSC_EVT_SEG:

		if (!ctx->gcache) {
			atscin_send_file(ctx, evt_param, finfo, evt);
			break;
		}
		if (finfo->corrupted && !ctx->kc) return;
		cache_entry = gf_atsc3_dmx_get_service_udta(ctx->atsc_dmx, evt_param);
		if (cache_entry) {
			if (!ctx->clock_init_seg) ctx->clock_init_seg = gf_strdup(finfo->filename);
			sprintf(szPath, "x-dash-atsc: %d\r\nx-dash-first-seg: %s\r\n", evt_param, ctx->clock_init_seg);
			gf_dm_force_headers(ctx->dm, cache_entry, szPath);
		}
		is_init = GF_FALSE;
		if (!ctx->sync_tsi) {
			ctx->sync_tsi = finfo->tsi;
			ctx->last_toi = finfo->toi;
		} else if (ctx->sync_tsi == finfo->tsi) {
			if (ctx->last_toi > finfo->toi) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ATSC, ("[ATSCDmx] Loop detected on service %d for TSI %u: prev TOI %u this toi %u\n", ctx->tune_service_id, finfo->tsi, ctx->last_toi, finfo->toi));

				gf_atsc3_dmx_purge_objects(ctx->atsc_dmx, evt_param);
				is_loop = GF_TRUE;
				if (cache_entry) {
					if (ctx->clock_init_seg) gf_free(ctx->clock_init_seg);
					ctx->clock_init_seg = gf_strdup(finfo->filename);
					sprintf(szPath, "x-dash-atsc: %d\r\nx-dash-first-seg: %s\r\nx-atsc-loop: yes\r\n", evt_param, ctx->clock_init_seg);
					gf_dm_force_headers(ctx->dm, cache_entry, szPath);
				}
			}
			ctx->last_toi = finfo->toi;
		}
		//fallthrough

	case GF_ATSC_EVT_INIT_SEG:
		if (!ctx->gcache) {
			atscin_send_file(ctx, evt_param, finfo, evt);
			break;
		}

		if (finfo->corrupted && !ctx->kc) return;

		sprintf(szPath, "http://gpatsc/service%d/%s", evt_param, finfo->filename);
		//we copy over the init segment, but only share the data pointer for segments

		cache_entry = gf_dm_add_cache_entry(ctx->dm, szPath, finfo->data, finfo->size, 0, 0, "video/mp4", is_init ? GF_TRUE : GF_FALSE, finfo->download_ms);

		if (cache_entry) gf_dm_force_headers(ctx->dm, cache_entry, "x-atsc: yes\r\n");

		GF_LOG(GF_LOG_INFO, GF_LOG_ATSC, ("[ATSCDmx] Pushing file %s to cache\n", szPath));

		if (is_loop) break;

		nb_obj = gf_atsc3_dmx_get_object_count(ctx->atsc_dmx, evt_param);
		if (nb_obj>10) {
			while (nb_obj>10) {
				gf_atsc3_dmx_remove_first_object(ctx->atsc_dmx, evt_param);
				nb_obj = gf_atsc3_dmx_get_object_count(ctx->atsc_dmx, evt_param);
			}
		}
		break;
	default:
		break;
	}
}

static Bool atscin_local_cache_probe(void *par, char *url, Bool is_destroy)
{
	ATSCInCtx *ctx = (ATSCInCtx *)par;
	u32 sid=0;
	char *subr;
	if (strncmp(url, "http://gpatsc/service", 21)) return GF_FALSE;

	subr = strchr(url+21, '/');
	subr[0] = 0;
	sid = atoi(url+21);
	subr[0] = '/';
	if (is_destroy) {
		gf_atsc3_dmx_remove_object_by_name(ctx->atsc_dmx, sid, subr+1, GF_TRUE);
	} else if (sid && (sid != ctx->tune_service_id)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ATSC, ("[ATSCDmx] Request on service %d but tuned on service %d, retuning\n", sid, ctx->tune_service_id));
		ctx->tune_service_id = sid;
		ctx->sync_tsi = 0;
		ctx->last_toi = 0;
		if (ctx->clock_init_seg) gf_free(ctx->clock_init_seg);
		ctx->clock_init_seg = NULL;
		gf_atsc3_tune_in(ctx->atsc_dmx, sid, GF_TRUE);
	}
	return GF_TRUE;
}

static GF_Err atscin_process(GF_Filter *filter)
{
	ATSCInCtx *ctx = gf_filter_get_udta(filter);

	while (1) {
		GF_Err e = gf_atsc3_dmx_process(ctx->atsc_dmx);
		if (e == GF_IP_NETWORK_EMPTY) {
			gf_filter_ask_rt_reschedule(filter, 1000);
			break;
		}
	}
	if (!ctx->tune_time) {
	 	u32 diff = gf_sys_clock() - ctx->start_time;
	 	if (diff>ctx->timeout) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ATSC, ("[ATSCDmx] No data for %d ms, aborting\n", diff));
			gf_filter_setup_failure(filter, GF_SERVICE_ERROR);
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

				u64 st = gf_atsc3_dmx_get_first_packet_time(ctx->atsc_dmx);
				u64 et = gf_atsc3_dmx_get_last_packet_time(ctx->atsc_dmx);
				u64 nb_pck = gf_atsc3_dmx_get_nb_packets(ctx->atsc_dmx);
				u64 nb_bytes = gf_atsc3_dmx_get_recv_bytes(ctx->atsc_dmx);

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


static GF_Err atscin_initialize(GF_Filter *filter)
{
	ATSCInCtx *ctx = gf_filter_get_udta(filter);
	ctx->filter = filter;

	if (!ctx->src) return GF_BAD_PARAM;
	if (strcmp(ctx->src, "atsc://")) return GF_BAD_PARAM;

	if (ctx->odir)
		ctx->gcache = GF_FALSE;

	if (ctx->gcache) {
		ctx->dm = gf_filter_get_download_manager(filter);
		if (!ctx->dm) return GF_SERVICE_ERROR;
		gf_dm_set_localcache_provider(ctx->dm, atscin_local_cache_probe, ctx);
	}

	ctx->atsc_dmx = gf_atsc3_dmx_new(ctx->ifce, ctx->odir, ctx->buffer);
	if (ctx->odir && ctx->max_segs) {
		gf_atsc3_set_max_objects_store(ctx->atsc_dmx, (u32) ctx->max_segs);
	}

	gf_atsc3_set_reorder(ctx->atsc_dmx, ctx->reorder, ctx->rtimeout);

	if (ctx->tsidbg) {
		gf_atsc3_dmx_debug_tsi(ctx->atsc_dmx, ctx->tsidbg);
	}

	gf_atsc3_set_callback(ctx->atsc_dmx, atscin_on_event, ctx);
	if (ctx->tunein>0) ctx->tune_service_id = ctx->tunein;

	if (ctx->tune_service_id)
		gf_atsc3_tune_in(ctx->atsc_dmx, ctx->tune_service_id, GF_FALSE);
	else
		gf_atsc3_tune_in(ctx->atsc_dmx, (u32) ctx->tunein, GF_TRUE);

	ctx->start_time = gf_sys_clock();

	if (ctx->stsi) ctx->tsi_outs = gf_list_new();
	if (ctx->max_segs)
		ctx->received_seg_names = gf_list_new();

	return GF_OK;
}

#define OFFS(_n)	#_n, offsetof(ATSCInCtx, _n)
static const GF_FilterArgs ATSCInArgs[] =
{
	{ OFFS(src), "location of source content - see filter help", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(ifce), "default interface to use for multicast. If NULL, the default system interface will be used", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(gcache), "indicate the files should populate GPAC HTTP cache - see filter help", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(tunein), "service ID to bootstrap on. 0 means tune to no service, -1 tune all services -2 means tune on first service found", GF_PROP_SINT, "-2", NULL, 0},
	{ OFFS(buffer), "receive buffer size to use in bytes", GF_PROP_UINT, "0x80000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(timeout), "timeout in ms after which tunein fails", GF_PROP_UINT, "5000", NULL, 0},
	{ OFFS(kc), "keep corrupted file", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(sr), "skip repeated files - ignored in cache mode", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(stsi), "define one output pid per tsi/serviceID - ignored in cache mode, see filter help", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(stats), "log statistics at the given rate in ms (0 disables stats)", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(tsidbg), "gather only objects with given TSI (debug)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(max_segs), "maximum number of segments to keep - ignored in cache mode", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(odir), "output directory for stand-alone mode - see filter help", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(reorder), "ignore order flag in ROUTE/LCT packets, avoiding considering object done when TOI changes", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(rtimeout), "default timeout in ms to wait when gathering out-of-order packets", GF_PROP_UINT, "5000", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability ATSCInCaps[] =
{
	CAP_UINT(GF_CAPS_OUTPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
};

GF_FilterRegister ATSCInRegister = {
	.name = "atscin",
	GF_FS_SET_DESCRIPTION("ATSC input")
#ifndef GPAC_DISABLE_DOC
	.help = "This filter is a receiver for ATSC 3.0 ROUTE sessions. Source is identified using the string `atsc://`.\n"
	"The filter can work in cached mode, source mode or standalone mode.\n"
	"# Cached mode\n"
	"The cached mode is the default filter behaviour. It populates GPAC HTTP Cache with the recieved files, using `http://gpatsc/serviceN/` as service root, N being the ATSC service ID.\n"
	"In cached mode, repeated files are always send.\n"
	"  \n"
	"The cached MPD is assigned the following headers:\n"
	"- x-dash-atsc: integer value, indicates the ATSC service ID.\n"
	"- x-dash-first-seg: string value, indicates the name of the first segment completely retrieved from the broadcast.\n"
	"- x-atsc-loop: boolean value, if yes indicates a loop in the service has been detected (usually pcap replay loop).\n"
	"  \n"
	"The cached files are assigned the following headers:\n"
	"- x-atsc: boolean value, if yes indicates the file comes from an ATSC session.\n"
	"\n"
	"# Source mode\n"
	"In source mode, the filter outputs files on a single output pid of type `file`. "
	"The files are dispatched once fully received, the output pid carries a sequence of complete files. Repeated files are not sent unless requested.\n"
	"If needed, one pid per TSI can be used rather than a single pid. This avoids mixing files of different mime types on the same pid (e.g. mpd and isobmff).\n"
	"EX gpac -i atsc://cache=false -o $ServiceID$/$File$:dynext\n"
	"This will grab the files and forward them as output PIDs, consumed by the [fout](fout) filter.\n"
	"\n"
	"# Standalone mode\n"
	"In standalone mode, the filter does not produce any output pid and writes received files to the [-odir]() directory.\n"
	"EX gpac -i atsc://odir=output\n"
	"This will grab the files and write them to `output` directory.\n"
	"\n"
	"# Interface setup\n"
	"On some systems (OSX), when using VM packet replay, you may need to force multicast routing on your local interface.\n"
	"You will have to do this for the base ATSC3 multicast (224.0.23.60):\n"
	"EX route add -net 224.0.23.60/32 -interface vboxnet0\n"
	"and on each service multicast:\n"
	"EX route add -net 239.255.1.4/32 -interface vboxnet0\n"
	"",
#endif //GPAC_DISABLE_DOC
	.private_size = sizeof(ATSCInCtx),
	.args = ATSCInArgs,
	.initialize = atscin_initialize,
	.finalize = atscin_finalize,
	SETCAPS(ATSCInCaps),
	.process = atscin_process,
	.probe_url = atscin_probe_url
};

const GF_FilterRegister *atscin_register(GF_FilterSession *session)
{
	return &ATSCInRegister;
}

#else

const GF_FilterRegister *atscin_register(GF_FilterSession *session)
{
	return NULL;
}

#endif /* GPAC_DISABLE_ATSC */

