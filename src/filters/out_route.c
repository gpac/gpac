/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / ROUTE output filter
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
#include <gpac/constants.h>
#include <gpac/xml.h>
#include <gpac/route.h>
#include <gpac/network.h>

#if !defined(GPAC_DISABLE_ROUTE)

enum
{
	DVB_CSUM_NO=0,
	DVB_CSUM_META,
	DVB_CSUM_ALL,
};

enum
{
	LCT_SPLIT_NONE=0,
	LCT_SPLIT_TYPE,
	LCT_SPLIT_ALL,
};

typedef struct
{
	//options
	char *dst, *ext, *mime, *ifce, *ip;
	u32 carousel, first_port, bsid, mtu, splitlct, ttl, brinc, runfor;
	Bool korean, llmode, noreg, nozip, furl, flute;
	u32 csum;

	//caps, overloaded at init
	GF_FilterCapability in_caps[2];
	char szExt[10];

	GF_List *services;
	//set to true if all services are done
	Bool done;

	//clock is sampled at each process() begin or before each LCT packet to be send
	u64 clock_init, clock;

	//preallocated buffer for LCT packet formating
	u8 *lct_buffer;
	GF_BitStream *lct_bs;

	u64 reschedule_us;
	//TOI for raw files
	u32 next_raw_file_toi;

	//stats reporting
	u64 clock_stats;
	Bool reporting_on;
	u64 total_size, total_bytes;
	Bool total_size_unknown;
	u32 nb_resources;
	u64 bytes_sent;

	const char *log_name;

	//ATSC3
	//main socket
	GF_Socket *sock_atsc_lls;
	//last LLS sent time
	u64 last_lls_clock;
	//time table
	u8 *lls_time_table;
	u32 lls_time_table_len;
	//service table
	u8 *lls_slt_table;
	u32 lls_slt_table_len;


	//DVB-MABR
	//set to true if using DVB-MABR
	Bool dvb_mabr;
	u32 flute_msize;
	//global service announcement
	GF_Socket *sock_dvb_mabr;
	u16 dvb_mabr_port;
	u32 dvb_mabr_tsi;

	u64 last_dvb_mabr_clock;
	//multicast gateway config
	u8 *dvb_mabr_config;
	u32 dvb_mabr_config_len;
	u32 dvb_mabr_config_toi;
	//FDT for mcast config + manifests + init segments
	u8 *dvb_mabr_fdt;
	u32 dvb_mabr_fdt_len;
	u32 dvb_mabr_fdt_instance_id;

	u32 next_toi_avail;
	Bool check_pending;
	u32 check_init_clock;
} GF_ROUTEOutCtx;

typedef struct
{
	char *ip;
	u32 port;

	//for now we use a single route session per service, differenciated by TSI
	GF_Socket *sock;

} ROUTELCT;

typedef struct
{
	u32 service_id;
	GF_List *pids;
	//for raw media mode in dash mode,
	u32 dash_mode;

	//LCT channels
	GF_List *rlcts;
	//pointer to the main LCT channel
	ROUTELCT *rlct_base;
	//next port to use when spliting components over LCT channels
	u32 next_port;
	Bool is_done;
	//service needs to check input data before being ready
	Bool wait_for_inputs;
	//service needs a reconfigure (mainest/init changed)
	//for route: a new STSID needs to be regenerated
	//for DVB MABR: a new global config needs to be regenerated
	Bool needs_reconfig;
	//set to FALSE if waiting for manifest of init segments, TRUE when everything is in place
	Bool service_ready;
	//time of service creation, used for setup timeout
	u32 creation_time;
	char *log_name;

	//0: no HAS, 1: DASH, 2: HLS
	u32 manifest_type;
	//storage for main manifest - all manifests (including HLS sub-playlists) are sent in the same PID
	//HLS sub-playlists are stored on their related PID to be pushed at each new seg
	char *manifest, *manifest_name, *manifest_mime, *manifest_server, *manifest_url;
	u32 manifest_version, manifest_crc;

	//for route
	u32 stsid_version;
	u8 *stsid_bundle;
	u32 stsid_bundle_size;
	u32 stsid_bundle_toi;
	u64 last_stsid_clock;

	//service name for DVB MABR
	char *service_name;
	//TOI for manifest in FLUTE mode
	u32 mani_toi;

	Bool use_flute;
} ROUTEService;

typedef struct
{
	GF_FilterPid *pid;

	ROUTEService *route;
	ROUTELCT *rlct;

	u32 tsi, bandwidth, stream_type;
	GF_Fraction dash_dur;
	//0: not manifest, 1: MPD, 2: HLS
	u32 manifest_type;
	//DASH template if any
	char *template;
	//raw file
	Bool raw_file;
	//template uses no '/'
	Bool use_basename;

	//set to true if no init seg for this format
	Bool no_init_seg;
	Bool init_seg_sent;
	//we cannot hold a ref to the init segment packet, as this may lock the input waiting for its release to dispatch next packets
	u8 *init_seg_data;
	u32 init_seg_size;
	u32 init_seg_crc;
	char *init_seg_name;

	//HLS variant playlist - we cannot hold a ref to the HLS packet, as this may lock the input waiting fot its release
	char *hld_child_pl, *hld_child_pl_name;
	u32 hld_child_pl_version, hld_child_pl_crc;
	u64 hls_ref_id;
	Bool update_hls_child_pl;

	//ROUTE code point
	u32 fmtp;

	//reference to current packet
	GF_FilterPacket *current_pck;
	//matches dash startNumber for ROUTE, otherwise in [1,0xFFFF] for flute
	u32 current_toi;

	const u8 *pck_data;
	u32 pck_size, pck_offset;
	char *seg_name;

	//cumulated segment size in RAW dash (for event signaling)
	u64 res_size;
	//byte offset at seg start, for RAW dash
	u64 offset_at_seg_start;

	//size of segment/file
	u32 full_frame_size;
	//cumulated size of chunks in segment for low latency mode
	u32 cumulated_frag_size;
	//byte offset of chunk in segment for low latency mode
	u32 frag_offset;
	//chunk index in segment
	u32 frag_idx;
	//set to TRUE when init sement must be pushed
	Bool push_init;
	//set to TRUE if last packet of segment - may trigger sending 0-bytes just to signal TOL in route or end of seg in flute
	Bool force_send_empty;

	//scheduling info
	u64 clock_at_frame_start, cts_us_at_frame_start, cts_at_frame_start;
	u32 pck_dur_at_frame_start;
	u32 timescale;
	u64 clock_at_first_pck;
	u64 cts_first_pck;
	u64 current_cts_us;
	u64 current_dur_us;
	u64 carousel_time_us;
	u64 clock_at_pck;
	//target send rate
	u32 bitrate;
	u64 last_init_push;
	Bool use_time_tpl;
	//for flute
	u32 init_toi, hls_child_toi;
	//set to true to force sending fragment name (flute LL mode)
	Bool push_frag_name;
	Bool init_cfg_done;
	u32 fdt_instance_id;
} ROUTEPid;


ROUTELCT *route_create_lct_channel(GF_Filter *filter, GF_ROUTEOutCtx *ctx, const char *ip, u32 port, GF_Err *e)
{
	*e = GF_OUT_OF_MEM;
	ROUTELCT *rlct;
	GF_SAFEALLOC(rlct, ROUTELCT);
	if (!rlct) return NULL;

	rlct->ip = gf_strdup(ip ? ip : ctx->ip);
	if (!rlct->ip) goto fail;
	if (port) {
		rlct->port = port;
	} else {
		rlct->port = ctx->first_port;
		ctx->first_port ++;
	}

	if (rlct->ip) {
		rlct->sock = gf_sk_new_ex(GF_SOCK_TYPE_UDP, gf_filter_get_netcap_id(filter) );
		if (rlct->sock) {
			*e = gf_sk_setup_multicast(rlct->sock, rlct->ip, rlct->port, ctx->ttl, GF_FALSE, ctx->ifce);
			if (*e) {
				gf_sk_del(rlct->sock);
				rlct->sock = NULL;
				goto fail;
			}
		}
	}
	*e = GF_OK;
	return rlct;
fail:
	if (rlct->ip) gf_free(rlct->ip);
	gf_free(rlct);
	return NULL;
}

ROUTEService *routeout_create_service(GF_Filter *filter, GF_ROUTEOutCtx *ctx, u32 service_id, const char *service_name, const char *ip, u32 port, GF_Err *e)
{
	ROUTEService *rserv;
	ROUTELCT *rlct = NULL;
	*e = GF_OUT_OF_MEM;
	GF_SAFEALLOC(rserv, ROUTEService);
	if (!rserv) return NULL;

	rlct = route_create_lct_channel(filter, ctx, ip, port, e);
	if (!rlct) {
		gf_free(rserv);
		return NULL;
	}
	if (port) {
		rserv->next_port = port+1;
	}
	if (ctx->dvb_mabr) rserv->use_flute = ctx->flute;

	rserv->pids = gf_list_new();
	if (!rserv->pids) goto fail;
	rserv->rlcts = gf_list_new();
	if (!rserv->rlcts) goto fail;

	gf_list_add(rserv->rlcts, rlct);
	rserv->rlct_base = rlct;

	rserv->service_id = service_id;
	if (service_name) {
		rserv->service_name = gf_strdup(service_name);
	} else {
		char szName[100];
		sprintf(szName, "gpac.io:dvb-mabr:service:%u", service_id);
		rserv->service_name = gf_strdup(szName);
	}

	gf_list_add(ctx->services, rserv);

	if (ctx->lls_slt_table) {
		gf_free(ctx->lls_slt_table);
		ctx->lls_slt_table = NULL;
		ctx->last_lls_clock = 0;
	}
	//reset MABR config
	if (ctx->dvb_mabr_config) {
		gf_free(ctx->dvb_mabr_config);
		ctx->dvb_mabr_config = NULL;
	}
	char logname[1024];
	sprintf(logname, "%s S%u", rserv->use_flute ? "DVB-FLUTE" : "ROUTE", service_id);
	rserv->log_name = gf_strdup(logname);
	*e = GF_OK;
	return rserv;

fail:
	*e = GF_OUT_OF_MEM;
	if (rlct->ip) gf_free(rlct->ip);
	gf_free(rlct);
	if (rserv->pids) gf_list_del(rserv->pids);
	if (rserv->rlcts) gf_list_del(rserv->rlcts);
	gf_free(rserv);
	return NULL;
}

void routeout_remove_pid(ROUTEPid *rpid, Bool is_rem)
{
	if (!is_rem) {
		gf_list_del_item(rpid->route->pids, rpid);
		rpid->route->needs_reconfig = GF_TRUE;
	}
	if (rpid->rlct != rpid->route->rlct_base) {
		gf_list_del_item(rpid->route->rlcts, rpid->rlct);
		gf_free(rpid->rlct->ip);
		gf_sk_del(rpid->rlct->sock);
		gf_free(rpid->rlct);
	}

	if (rpid->init_seg_data) gf_free(rpid->init_seg_data);
	if (rpid->init_seg_name) gf_free(rpid->init_seg_name);
	if (rpid->hld_child_pl) gf_free(rpid->hld_child_pl);
	if (rpid->hld_child_pl_name) gf_free(rpid->hld_child_pl_name);
	if (rpid->template) gf_free(rpid->template);
	if (rpid->seg_name) gf_free(rpid->seg_name);

	if (rpid->current_pck)
		gf_filter_pck_unref(rpid->current_pck);
	gf_free(rpid);
}

void routeout_delete_service(ROUTEService *serv)
{
	while (gf_list_count(serv->pids)) {
		ROUTEPid *rpid = gf_list_pop_back(serv->pids);
		routeout_remove_pid(rpid, GF_TRUE);
	}
	gf_list_del(serv->pids);

	while (gf_list_count(serv->rlcts)) {
		ROUTELCT *rlct = gf_list_pop_back(serv->rlcts);
		gf_sk_del(rlct->sock);
		gf_free(rlct->ip);
		gf_free(rlct);
	}
	gf_list_del(serv->rlcts);

	if (serv->manifest_mime) gf_free(serv->manifest_mime);
	if (serv->manifest_name) gf_free(serv->manifest_name);
	if (serv->manifest_server) gf_free(serv->manifest_server);
	if (serv->manifest_url) gf_free(serv->manifest_url);

	if (serv->manifest) gf_free(serv->manifest);
	if (serv->stsid_bundle) gf_free(serv->stsid_bundle);
	if (serv->service_name) gf_free(serv->service_name);
	if (serv->log_name) gf_free(serv->log_name);
	gf_free(serv);
}

static GF_Err routeout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	const GF_PropertyValue *p;
	u32 i;
	GF_FilterEvent evt;
	ROUTEService *rserv;
	u32 manifest_type;
	u32 service_id = 0;
	u32 pid_dash_mode = 0;
	GF_ROUTEOutCtx *ctx = (GF_ROUTEOutCtx *) gf_filter_get_udta(filter);
	ROUTEPid *rpid;

	rpid = gf_filter_pid_get_udta(pid);
	if (is_remove) {
		if (rpid) routeout_remove_pid(rpid, GF_FALSE);
		ctx->check_pending = GF_TRUE;
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_FILTER_NOT_SUPPORTED;

	//we currently ignore any reconfiguration of the pids
	if (rpid) {
		//any change to a raw file will require reconfiguring S-TSID
		if (rpid->raw_file) {
			rpid->route->needs_reconfig = GF_TRUE;
		}
		else if (!rpid->manifest_type) {
			p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_TEMPLATE);
			if (p && p->value.string) {
				char *sep1 = strstr(p->value.string, "$Number");
				char *sep2 = strstr(p->value.string, "$Time");
				if (sep1 && sep2) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] DASH Template is %s but ROUTE cannot use both Time and Number !\n", rpid->route->log_name, p->value.string));
					rpid->route->is_done = GF_TRUE;
					return GF_BAD_PARAM;
				}

				if (!rpid->template || strcmp(rpid->template, p->value.string)) {
					if (rpid->template) gf_free(rpid->template);
					rpid->template = gf_strdup(p->value.string);
					rpid->route->needs_reconfig = GF_TRUE;
					rpid->use_basename = (strchr(rpid->template, '/')==NULL) ? GF_TRUE : GF_FALSE;
				}
			} else if (rpid->template) {
				gf_free(rpid->template);
				rpid->template = NULL;
				rpid->route->needs_reconfig = GF_TRUE;
			}
		}

		return GF_OK;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DISABLE_PROGRESSIVE);
	if (p && p->value.uint) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Delivering files with progressive download disabled is not possible in ROUTE !\n", ctx->log_name));
		return GF_FILTER_NOT_SUPPORTED;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DASH_MODE);
	if (p) pid_dash_mode = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_ID);
	if (p) service_id = p->value.uint;

	rserv = NULL;
	for (i=0; i<gf_list_count(ctx->services); i++) {
		rserv = gf_list_get(ctx->services, i);
		if (service_id == rserv->service_id) break;
		rserv = NULL;
	}

	manifest_type = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_IS_MANIFEST);
	if (p) manifest_type = p->value.uint;

	if (manifest_type) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PREMUX_STREAM_TYPE);
		if (!p || (p->value.uint!=GF_STREAM_FILE)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Manifest file detected but no dashin filter, file will be uploaded as is !\n", ctx->log_name));
			manifest_type = 0;
		}
	}
	if (manifest_type) {
		if (manifest_type & 0x80000000) {
			manifest_type &= 0x7FFFFFFF;
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Manifest file describes a static session, clients tune-in will likely fail !\n", ctx->log_name));
		}
	}

	if (!rserv) {
		GF_Err e;
		u32 port = ctx->first_port;
		const char *service_ip = ctx->ip;

		//cannot have 2 manifest pids connecting in route mode
		if (!ctx->sock_atsc_lls && gf_list_count(ctx->services) && manifest_type) {
			if (strchr(ctx->dst, '$')) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Multiple services in route mode, creating a new output filter\n", ctx->log_name));
				return GF_REQUIRES_NEW_INSTANCE;
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("%s Multiple services in route mode and no URL templating, cannot create new output\n", ctx->log_name));
			return GF_FILTER_NOT_SUPPORTED;
		}

		if (ctx->sock_atsc_lls) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ROUTE_IP);
			if (p && p->value.string) service_ip = p->value.string;

			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ROUTE_PORT);
			if (p && p->value.uint) port = p->value.uint;
			else ctx->first_port++;
		}

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_NAME);
		rserv = routeout_create_service(filter, ctx, service_id, p ? p->value.string : NULL, service_ip, port, &e);
		if (!rserv) return e;
		rserv->dash_mode = pid_dash_mode;
	}

	if (!rserv->manifest_type)
		rserv->manifest_type = manifest_type;

	if (rserv->dash_mode != pid_dash_mode){
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Mix of raw and muxed files should never happen - please report bug !\n", rserv->log_name));
		return GF_SERVICE_ERROR;
	}

	GF_SAFEALLOC(rpid, ROUTEPid);
	if (!rpid) return GF_OUT_OF_MEM;
	rpid->route = rserv;
	rpid->pid = pid;
	rpid->fmtp = 128;
	rpid->tsi = 0;
	rpid->manifest_type = manifest_type;
	if (manifest_type) {
		rserv->creation_time = gf_sys_clock();
		gf_filter_pid_ignore_blocking(pid, GF_TRUE);
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NO_INIT);
		if (p && p->value.boolean) {
			rpid->no_init_seg = GF_TRUE;
		}
	}

	gf_list_add(rserv->pids, rpid);
	gf_filter_pid_set_udta(pid, rpid);

	rpid->rlct = rserv->rlct_base;

	rpid->bitrate = 0;
	rpid->bandwidth = 0;
	p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_BITRATE);
	if (p) {
		rpid->bitrate = p->value.uint * (100+ctx->brinc) / 100;
		rpid->bandwidth = p->value.uint;
	}

	rpid->stream_type = 0;
	p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_PREMUX_STREAM_TYPE);
	if (!p) {
		if (!rpid->manifest_type) {
			rpid->raw_file = GF_TRUE;
		}
	} else {
		rpid->stream_type = p->value.uint;
	}

	rpid->dash_dur.num = 1;
	rpid->dash_dur.den = 1;
	p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_DASH_DUR);
	if (p) rpid->dash_dur = p->value.frac;

	if (!rpid->manifest_type && !rpid->raw_file) {
		p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_TEMPLATE);
		if (p && p->value.string) {
			char *sep1 = strstr(p->value.string, "$Number");
			char *sep2 = strstr(p->value.string, "$Time");
			if (sep1 && sep2) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] DASH Template is %s but ROUTE cannot use both Time and Number !\n", rserv->log_name, p->value.string));
				gf_filter_abort(filter);
				return GF_BAD_PARAM;
			}
			rpid->template = gf_strdup(p->value.string);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Segment file PID detected but no template assigned, assuming raw file upload!\n", rserv->log_name));
			rpid->raw_file = GF_TRUE;
		}
	}

	if (rpid->raw_file) {
		rpid->tsi = 1;
		rpid->current_toi = ctx->next_raw_file_toi;
		ctx->next_raw_file_toi ++;
	}

	if (!rpid->manifest_type && !rpid->raw_file) {
		Bool do_split = GF_FALSE;

		p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_TIMESCALE);
		if (p) rpid->timescale = p->value.uint;

		p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PCK_HLS_REF);
		if (p) rpid->hls_ref_id = p->value.longuint;

		rpid->tsi = gf_list_find(rserv->pids, rpid) * 10;

		//do we put this on the main LCT of the service or do we split ?
		if (ctx->splitlct==LCT_SPLIT_ALL) {
			if (gf_list_count(rserv->pids)>1)
				do_split = GF_TRUE;
		}
		else if (ctx->splitlct && rpid->stream_type) {
			for (i=0; i<gf_list_count(rserv->pids); i++) {
				u32 astreamtype;
				ROUTEPid *apid = gf_list_get(rserv->pids, i);
				if (apid->manifest_type) continue;
				if (apid == rpid) continue;
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_PREMUX_STREAM_TYPE);
				if (!p) continue;
				astreamtype = p->value.uint;
				if (astreamtype==rpid->stream_type) {
					do_split = GF_TRUE;
					break;
				}
			}
		}
		if (do_split) {
			GF_Err e;
			rpid->rlct = route_create_lct_channel(filter, ctx, NULL, rserv->next_port, &e);
			if (e) return e;
			rserv->next_port++;
			ctx->first_port++;
			if (rpid->rlct) {
				gf_list_add(rserv->rlcts, rpid->rlct);
			}
		}
	}
	//a new pid has appeared, we need to reset MABR manifest
	if (ctx->dvb_mabr_config) {
		gf_free(ctx->dvb_mabr_config);
		ctx->dvb_mabr_config = NULL;
	}
	ctx->check_pending = GF_TRUE;

	gf_filter_pid_init_play_event(pid, &evt, 0, 1.0, "ROUTEOut");
	gf_filter_pid_send_event(pid, &evt);

	if (ctx->llmode && !rpid->raw_file) {
		rpid->carousel_time_us = ctx->carousel;
		p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_ROUTE_CAROUSEL);
		if (p) {
			//can be 0
			rpid->carousel_time_us = gf_timestamp_rescale(p->value.frac.num, p->value.frac.den, 1000000);
		}
	}

	rserv->wait_for_inputs = GF_TRUE;
	//low-latency mode, consume media segment files as they are received (don't wait for full segment reconstruction)
	if (ctx->llmode && !rpid->manifest_type && !rpid->raw_file) {
		gf_filter_pid_set_framing_mode(pid, GF_FALSE);
	} else {
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}
	return GF_OK;
}

static GF_Err routeout_initialize(GF_Filter *filter)
{
	char *base_name;
	Bool is_atsc = GF_TRUE;
	u32 proto_offset = 0;
	char *ext=NULL;
	GF_ROUTEOutCtx *ctx = (GF_ROUTEOutCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_BAD_PARAM;
	ctx->log_name = "ROUTE";
	if (!strnicmp(ctx->dst, "route://", 8)) {
		is_atsc = GF_FALSE;
		proto_offset = 8;
	} else if (!strnicmp(ctx->dst, "mabr://", 7)) {
		proto_offset = 7;
		is_atsc = GF_FALSE;
		ctx->dvb_mabr = GF_TRUE;
		ctx->log_name = "DVB-FLUTE";
	} else if (strnicmp(ctx->dst, "atsc://", 7))  {
		return GF_NOT_SUPPORTED;
	}

	if (ctx->ext) {
		ext = ctx->ext;
	} else {
		if (is_atsc) {
			base_name = gf_file_basename(ctx->dst);
		} else {
			char *sep = strchr(ctx->dst + proto_offset, '/');
			base_name = sep ? gf_file_basename(ctx->dst) : NULL;
		}
		ext = base_name ? gf_file_ext_start(base_name) : NULL;
		if (ext) ext++;
	}

#if 0
	if (!ext) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing destination manifest type, cannot infer format!\n", ctx->log_name));
		return GF_BAD_PARAM;
	}
#endif

	if (is_atsc) {
		if (!ctx->ip) return GF_BAD_PARAM;
	} else {
		char *sep, *root;
		sep = strrchr(ctx->dst+proto_offset, ':');
		if (sep) sep[0] = 0;
		root = sep ? strchr(sep+1, '/') : NULL;
		if (root) root[0] = 0;
		if (ctx->ip) gf_free(ctx->ip);
		ctx->ip = gf_strdup(ctx->dst + proto_offset);
		if (sep) {
			ctx->first_port = atoi(sep+1);
			sep[0] = ':';
		}
		if (root) root[0] = '/';
	}

	if (!gf_sk_is_multicast_address(ctx->ip)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] IP %s is not a multicast address\n", ctx->log_name, ctx->ip));
		return GF_BAD_PARAM;
	}

	if (ext || ctx->mime) {
		//static cap, streamtype = file
		ctx->in_caps[0].code = GF_PROP_PID_STREAM_TYPE;
		ctx->in_caps[0].val = PROP_UINT(GF_STREAM_FILE);
		ctx->in_caps[0].flags = GF_CAPS_INPUT_STATIC;

		if (ctx->mime) {
			ctx->in_caps[1].code = GF_PROP_PID_MIME;
			ctx->in_caps[1].val = PROP_NAME( ctx->mime );
			ctx->in_caps[1].flags = GF_CAPS_INPUT;
		} else {
			strncpy(ctx->szExt, ext, 9);
			ctx->szExt[9] = 0;
			strlwr(ctx->szExt);
			ctx->in_caps[1].code = GF_PROP_PID_FILE_EXT;
			ctx->in_caps[1].val = PROP_NAME( ctx->szExt );
			ctx->in_caps[1].flags = GF_CAPS_INPUT;
		}
		gf_filter_override_caps(filter, ctx->in_caps, 2);
	}


	/*this is an alias for our main filter, nothing to initialize*/
	if (gf_filter_is_alias(filter) || gf_filter_is_temporary(filter)) {
		return GF_OK;
	}

	ctx->services = gf_list_new();

	if (is_atsc) {
		ctx->sock_atsc_lls = gf_sk_new_ex(GF_SOCK_TYPE_UDP, gf_filter_get_netcap_id(filter) );
		gf_sk_setup_multicast(ctx->sock_atsc_lls, GF_ATSC_MCAST_ADDR, GF_ATSC_MCAST_PORT, 0, GF_FALSE, ctx->ifce);
	}
	if (ctx->dvb_mabr) {
		ctx->sock_dvb_mabr = gf_sk_new_ex(GF_SOCK_TYPE_UDP, gf_filter_get_netcap_id(filter) );
		gf_sk_setup_multicast(ctx->sock_dvb_mabr, ctx->ip, ctx->first_port, 0, GF_FALSE, ctx->ifce);
		ctx->dvb_mabr_port = ctx->first_port;
		ctx->first_port++;
		ctx->dvb_mabr_tsi = 1;
	}

	ctx->lct_buffer = gf_malloc(sizeof(u8) * ctx->mtu);
	ctx->clock_init = gf_sys_clock_high_res();
	ctx->clock_stats = ctx->clock_init;
	ctx->lct_bs = gf_bs_new(ctx->lct_buffer, ctx->mtu, GF_BITSTREAM_WRITE);
	ctx->flute_msize = ctx->mtu - 11*4; //max size of headers and extensins

	if (!ctx->carousel) ctx->carousel = 1000;
	//move to microseconds
	ctx->carousel *= 1000;
	ctx->next_raw_file_toi = ctx->dvb_mabr ? 0x7FFF : 1;
	ctx->next_toi_avail = 1;
	if (ctx->llmode && (ctx->csum == DVB_CSUM_ALL)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Low-latency mode is activated but `csum=all`set, using `csum=meta`\n", ctx->log_name));
		ctx->csum = DVB_CSUM_META;
	}
	ctx->check_pending = GF_TRUE;
	return GF_OK;
}

static void routeout_finalize(GF_Filter *filter)
{
	GF_ROUTEOutCtx *ctx;
	/*this is an alias for our main filter, nothing to finalize*/
	if (gf_filter_is_alias(filter))
		return;

	ctx = (GF_ROUTEOutCtx *) gf_filter_get_udta(filter);

	while (gf_list_count(ctx->services)) {
		routeout_delete_service(gf_list_pop_back(ctx->services));
	}
	gf_list_del(ctx->services);
	if (ctx->sock_atsc_lls)
		gf_sk_del(ctx->sock_atsc_lls);
	if (ctx->sock_dvb_mabr)
		gf_sk_del(ctx->sock_dvb_mabr);

	if (ctx->lct_buffer) gf_free(ctx->lct_buffer);
	if (ctx->lls_slt_table) gf_free(ctx->lls_slt_table);
	if (ctx->lls_time_table) gf_free(ctx->lls_time_table);
	if (ctx->dvb_mabr_config) gf_free(ctx->dvb_mabr_config);
	if (ctx->dvb_mabr_fdt) gf_free(ctx->dvb_mabr_fdt);
	if (ctx->lct_bs) gf_bs_del(ctx->lct_bs);
}

char *routeout_strip_base(ROUTEService *serv, char *url)
{
	u32 len;
	if (!url) return NULL;
	if (!serv->manifest_server && !serv->manifest_url)
		return gf_strdup(url);

	len = serv->manifest_server ? (u32) strlen(serv->manifest_server) : 0;
	if (!len || !strncmp(url, serv->manifest_server, len)) {
		url += len;
		char *sep = len ? strchr(url, '/') : url;
		if (sep) {
			if (len) sep += 1;
			len = (u32) strlen(serv->manifest_url);
			if (!strncmp(sep, serv->manifest_url, len)) {
				return gf_strdup(sep + len);
			}
		}
	}
	return gf_strdup(url);
}

static GF_Err routeout_update_stsid_bundle(GF_ROUTEOutCtx *ctx, ROUTEService *serv, Bool manifest_updated);
static GF_Err routeout_update_dvb_mabr_fdt(GF_ROUTEOutCtx *ctx, ROUTEService *serv, Bool manifest_updated);

static GF_Err routeout_check_service_updates(GF_ROUTEOutCtx *ctx, ROUTEService *serv)
{
	u32 i, count;
	Bool manifest_updated = GF_FALSE;
	u32 nb_media=0, nb_media_init=0, nb_raw_files=0;

	serv->service_ready = GF_FALSE;
	count = gf_list_count(serv->pids);
	//check no changes in init segment or in manifests
	for (i=0; i<count; i++) {
		const GF_PropertyValue *p;
		ROUTEPid *rpid = gf_list_get(serv->pids, i);

		//raw file, nothing to check
		if (rpid->raw_file) {
			nb_raw_files++;
			continue;
		}
		//media file, check for init segment and hls child manifest
		if (!rpid->manifest_type) {
			nb_media++;
			while (! rpid->no_init_seg) {
				GF_FilterPacket *pck = gf_filter_pid_get_packet(rpid->pid);
				if (!pck) break;

				p = gf_filter_pck_get_property(pck, GF_PROP_PCK_INIT);
				if (p && p->value.boolean) {
					const u8 *data;
					u32 len, crc;
					data = gf_filter_pck_get_data(pck, &len);
					crc = gf_crc_32(data, len);
					//whenever init seg changes, bump stsid version
					if (crc != rpid->init_seg_crc) {
						if (rpid->init_seg_data) gf_free(rpid->init_seg_data);
						rpid->init_seg_data = gf_malloc(len);
						memcpy(rpid->init_seg_data, data, len);
						rpid->init_seg_size = len;
						rpid->init_seg_crc = crc;
						rpid->init_seg_sent = GF_FALSE;
						//we need a new TOI since init seg changed
						rpid->init_toi = 0;
						serv->needs_reconfig = GF_TRUE;
						rpid->current_toi = 0;

						p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
						if (!p) p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PCK_FILENAME);
						if (rpid->init_seg_name) gf_free(rpid->init_seg_name);
						rpid->init_seg_name = p ? routeout_strip_base(rpid->route, p->value.string) : NULL;
					}
					gf_filter_pid_drop_packet(rpid->pid);
					continue;
				}

				break;
			}
			if (rpid->init_seg_data || rpid->no_init_seg) {
				nb_media_init ++;
				if (serv->manifest_type==2) {
					if (!rpid->hld_child_pl_name)
						nb_media_init --;
				}
			}
			continue;
		}
		//manifest pid, wait for manifest
		while (1) {
			char szLocManfest[100];
			u32 man_size, man_crc;
			const char *file_name=NULL, *proto;
			GF_FilterPacket *pck = gf_filter_pid_get_packet(rpid->pid);
			if (!pck) break;

			file_name = ctx->dst;
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
			if (p) {
				file_name = p->value.string;
			} else {
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_URL);
				if (p)
					file_name = gf_file_basename(p->value.string);
			}

			if (file_name) {
				proto = strstr(file_name, "://");
				if (proto) {
					if (ctx->sock_atsc_lls) {
						file_name = proto[3] ? proto+3 : NULL;
					} else {
						file_name = strchr(proto+3, '/');
						if (file_name) file_name ++;
					}
				}
			}

			if (!file_name) {
				snprintf(szLocManfest, 100, "manifest.%s", (serv->manifest_type==2) ? "m3u8" : "mpd");
				file_name = szLocManfest;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] No manifest name assigned, will use %s\n", serv->log_name, file_name));
			}

			//child subplaylist
			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_HLS_REF);
			if (p && p->value.uint) {
				u32 k;
				ROUTEPid *media_pid = NULL;
				for (k=0; k<count; k++) {
					media_pid = gf_list_get(serv->pids, k);
					if (media_pid->hls_ref_id == p->value.longuint)
						break;
					media_pid = NULL;
				}
				//not yet available - happens when loading an existing m3u8 session
				if (!media_pid)
					break;

				const u8 *data;
				u32 len, crc;
				data = gf_filter_pck_get_data(pck, &len);
				crc = gf_crc_32(data, len);
				if (crc != media_pid->hld_child_pl_crc) {
					if (media_pid->hld_child_pl) gf_free(media_pid->hld_child_pl);
					media_pid->hld_child_pl = gf_malloc(len+1);
					memcpy(media_pid->hld_child_pl, data, len);
					media_pid->hld_child_pl[len] = 0;
					media_pid->hld_child_pl_crc = crc;
					//we need a new TOI since manifest changed
					media_pid->hls_child_toi = 0;

					if (!media_pid->hld_child_pl_name || strcmp(media_pid->hld_child_pl_name, file_name)) {
						if (media_pid->hld_child_pl_name) gf_free(media_pid->init_seg_name);
						media_pid->hld_child_pl_name = routeout_strip_base(rpid->route, (char *)file_name);
						serv->needs_reconfig = GF_TRUE;
					}
					media_pid->update_hls_child_pl = GF_TRUE;
					manifest_updated = GF_TRUE;
				}
			} else {
				const u8 *man_data = gf_filter_pck_get_data(pck, &man_size);
				man_crc = gf_crc_32(man_data, man_size);
				if (man_crc != serv->manifest_crc) {
					serv->manifest_crc = man_crc;
					if (serv->manifest) gf_free(serv->manifest);
					serv->manifest = gf_malloc(man_size+1);
					memcpy(serv->manifest, man_data, man_size);
					serv->manifest[man_size] = 0;
					serv->manifest_version++;
					if (serv->manifest_name) {
						if (strcmp(serv->manifest_name, file_name)) serv->needs_reconfig = GF_TRUE;
						gf_free(serv->manifest_name);
					}
					if (file_name[0])
						serv->manifest_name = gf_strdup(file_name);
					else
						serv->manifest_name = gf_strdup((serv->manifest_type==2) ? "live.m3u8" : "live.mpd");
					manifest_updated = GF_TRUE;
					//we need a new TOI since manifest changed
					serv->mani_toi = 0;

					if (serv->manifest_server) gf_free(serv->manifest_server);
					if (serv->manifest_url) gf_free(serv->manifest_url);
					serv->manifest_server = serv->manifest_url = NULL;

					p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_URL);
					serv->manifest_url = p ? gf_strdup(p->value.string) : NULL;
					if (serv->manifest_url) {
						char *sep = strstr(serv->manifest_url, file_name);
						if (sep) sep[0] = 0;
						sep = strstr(serv->manifest_url, "://");
						if (sep) sep = strchr(sep + 3, '/');
						if (sep) {
							serv->manifest_server = serv->manifest_url;
							serv->manifest_url = gf_strdup(sep+1);
							sep[0] = 0;
							sep = strchr(serv->manifest_server + 8, ':');
							if (sep) sep[0] = 0;
						} else {
							gf_free(serv->manifest_url);
							serv->manifest_url = NULL;
						}
					}

					p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_MIME);
					if (p) {
						if (serv->manifest_mime) gf_free(serv->manifest_mime);
						serv->manifest_mime = gf_strdup(p->value.string);
					}
				}
			}
			gf_filter_pid_drop_packet(rpid->pid);
		}
	}
	//not ready, waiting for init
	if ((nb_media+nb_raw_files==0) || (nb_media_init<nb_media) || (serv->manifest && !nb_media)) {
		u32 now = gf_sys_clock() - serv->creation_time;
		if (now > 5000) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] No media PIDs found for HAS service after %d ms, aborting !\n", serv->log_name, now));
			serv->is_done = GF_TRUE;
			serv->service_ready = GF_TRUE;
			return GF_SERVICE_ERROR;
		}
		return GF_OK;
	}

	//not ready, waiting for manifest
	if (!serv->manifest && !nb_raw_files) {
		return GF_NOT_READY;
	}
	//already setup and no changes
	else if (!serv->wait_for_inputs && !manifest_updated && !serv->needs_reconfig) {
		serv->service_ready = GF_TRUE;
		return GF_OK;
	}
	serv->service_ready = GF_TRUE;
	if (serv->wait_for_inputs) {
		if (!serv->manifest) {
			gf_assert(nb_raw_files);
			serv->needs_reconfig = GF_TRUE;
		}
		serv->wait_for_inputs = GF_FALSE;
	}
	if (serv->use_flute) {
		if (manifest_updated) {
			if (ctx->dvb_mabr_fdt) gf_free(ctx->dvb_mabr_fdt);
			ctx->dvb_mabr_fdt = NULL;
			ctx->last_dvb_mabr_clock = 0;
		}
		return GF_OK;
	}
	return routeout_update_stsid_bundle(ctx, serv, manifest_updated);
}

#define MULTIPART_BOUNDARY	"_GPAC_BOUNDARY_ROUTE_.67706163_"
#define ROUTE_INIT_TOI	0xFFFFFFFF
static GF_Err routeout_update_stsid_bundle(GF_ROUTEOutCtx *ctx, ROUTEService *serv, Bool manifest_updated)
{
	u32 i, j, nb_pids;
	char temp[1000];
	char *payload_text = NULL;

	nb_pids = gf_list_count(serv->pids);
	if (serv->needs_reconfig) {
		serv->stsid_version++;

		for (i=0; i<nb_pids; i++) {
			ROUTEPid *rpid = gf_list_get(serv->pids, i);
			if (rpid->manifest_type) continue;
			rpid->clock_at_first_pck = 0;
		}
	}

	//ATSC3: mbms enveloppe, service description, astcROUTE bundle
	if (ctx->sock_atsc_lls) {
		const GF_PropertyValue *p;
		char *service_name;
		ROUTEPid *rpid;
		u32 service_id = serv->service_id;
		if (!service_id) {
			service_id = 1;
		}
		gf_dynstrcat(&payload_text, "Content-Type: multipart/related; type=\"application/mbms-envelope+xml\"; boundary=\""MULTIPART_BOUNDARY"\"\r\n\r\n", NULL);

		gf_dynstrcat(&payload_text, "--"MULTIPART_BOUNDARY"\r\nContent-Type: application/mbms-envelope+xml\r\nContent-Location: envelope.xml\r\n\r\n", NULL);

		//dump usd first, then S-TSID then manifest
		gf_dynstrcat(&payload_text,
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<metadataEnvelope xmlns=\"urn:3gpp:metadata:2005:MBMS:envelope\">\n"
			" <item metadataURI=\"usbd.xml\" version=\"", NULL);
		snprintf(temp, 100, "%d", serv->stsid_version);
		gf_dynstrcat(&payload_text, temp, NULL);

		gf_dynstrcat(&payload_text, "\" contentType=\"", NULL);
		if (ctx->korean)
			gf_dynstrcat(&payload_text, "application/mbms-user-service-description+xml", NULL);
		else
			gf_dynstrcat(&payload_text, "application/route-usd+xml", NULL);
		gf_dynstrcat(&payload_text, "\"/>\n", NULL);

		gf_dynstrcat(&payload_text,
			" <item metadataURI=\"stsid.xml\" version=\"", NULL);
		snprintf(temp, 100, "%d", serv->stsid_version);
		gf_dynstrcat(&payload_text, temp, NULL);

		gf_dynstrcat(&payload_text, "\" contentType=\"", NULL);
		if (ctx->korean)
			gf_dynstrcat(&payload_text, "application/s-tsid", NULL);
		else
			gf_dynstrcat(&payload_text, "application/route-s-tsid+xml", NULL);
		gf_dynstrcat(&payload_text, "\"/>\n", NULL);


		if (serv->manifest) {
			gf_dynstrcat(&payload_text, " <item metadataURI=\"", NULL);
			gf_dynstrcat(&payload_text, serv->manifest_name, NULL);
			snprintf(temp, 1000, "\" version=\"%d\" contentType=\"", serv->manifest_version);
			gf_dynstrcat(&payload_text, temp, NULL);
			gf_dynstrcat(&payload_text, serv->manifest_mime, NULL);
			gf_dynstrcat(&payload_text, "\"/>\n", NULL);
		}

		gf_dynstrcat(&payload_text, "</metadataEnvelope>\n\r\n", NULL);

		gf_dynstrcat(&payload_text, "--"MULTIPART_BOUNDARY"\r\nContent-Type: ", NULL);
		if (ctx->korean)
			gf_dynstrcat(&payload_text, "application/mbms-user-service-description+xml", NULL);
		else
			gf_dynstrcat(&payload_text, "application/route-usd+xml", NULL);
		gf_dynstrcat(&payload_text, "\r\nContent-Location: usbd.xml\r\n\r\n", NULL);

		rpid = gf_list_get(serv->pids, 0);
		p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_SERVICE_NAME);
		if (p && p->value.string)
			service_name = p->value.string;
		else
			service_name = "GPAC TV";

		snprintf(temp, 1000, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<BundleDescriptionROUTE xmlns=\"tag:atsc.org,2016:XMLSchemas/ATSC3/Delivery/ROUTEUSD/1.0/\">\n"
				" <UserServiceDescription serviceId=\"%d\">\n"
				"  <Name lang=\"eng\">", service_id);
		gf_dynstrcat(&payload_text, temp, NULL);
		gf_dynstrcat(&payload_text, service_name, NULL);
		gf_dynstrcat(&payload_text, "</Name>\n"
				"  <DeliveryMethod>\n"
				"   <BroadcastAppService>\n", NULL);

		for (i=0;i<nb_pids; i++) {
			rpid = gf_list_get(serv->pids, i);
			if (rpid->manifest_type) continue;
			//set template
			p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_TEMPLATE);
			if (p) {
				char *tpl = gf_strdup(p->value.string);
				char *sep = strchr(tpl, '$');
				if (sep) sep[0] = 0;
				if (!strstr(payload_text, tpl)) {
					gf_dynstrcat(&payload_text, "    <BasePattern>", NULL);
					gf_dynstrcat(&payload_text, tpl, NULL);
					gf_dynstrcat(&payload_text, "</BasePattern>\n", NULL);
				}
				gf_free(tpl);
			}
		}

		gf_dynstrcat(&payload_text, "   </BroadcastAppService>\n"
				"  </DeliveryMethod>\n"
				" </UserServiceDescription>\n"
				"</BundleDescriptionROUTE>\n\r\n", NULL);
	}
	//ROUTE: only inject manifest and S-TSID
	else {
		gf_dynstrcat(&payload_text, "Content-Type: multipart/related; type=\"", NULL);
		if (serv->manifest) {
			gf_dynstrcat(&payload_text, serv->manifest_mime, NULL);
		}
		else if (ctx->korean)
			gf_dynstrcat(&payload_text, "application/s-tsid", NULL);
		else
			gf_dynstrcat(&payload_text, "application/route-s-tsid+xml", NULL);

		gf_dynstrcat(&payload_text, "\"; boundary=\""MULTIPART_BOUNDARY"\"\r\n\r\n", NULL);
	}

	if (serv->manifest) {
		gf_dynstrcat(&payload_text, "--"MULTIPART_BOUNDARY"\r\nContent-Type: ", NULL);
		gf_dynstrcat(&payload_text, serv->manifest_mime, NULL);
		gf_dynstrcat(&payload_text, "\r\nContent-Location: ", NULL);
		gf_dynstrcat(&payload_text, serv->manifest_name, NULL);
		gf_dynstrcat(&payload_text, "\r\n\r\n", NULL);
		gf_dynstrcat(&payload_text, serv->manifest, NULL);
		gf_dynstrcat(&payload_text, "\r\n\r\n", NULL);
	}

	gf_dynstrcat(&payload_text, "--"MULTIPART_BOUNDARY"\r\nContent-Type: ", NULL);
	if (ctx->korean)
		gf_dynstrcat(&payload_text, "application/s-tsid", NULL);
	else
		gf_dynstrcat(&payload_text, "application/route-s-tsid+xml", NULL);
	gf_dynstrcat(&payload_text, "\r\nContent-Location: stsid.xml\r\n\r\n", NULL);

	gf_dynstrcat(&payload_text, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<S-TSID xmlns=\"tag:atsc.org,2016:XMLSchemas/ATSC3/Delivery/S-TSID/1.0/\" xmlns:afdt=\"tag:atsc.org,2016:XMLSchemas/ATSC3/Delivery/ATSC-FDT/1.0/\" xmlns:fdt=\"urn:ietf:params:xml:ns:fdt\">\n", NULL);

	for (j=0; j<gf_list_count(serv->rlcts); j++) {
		ROUTELCT *rlct = gf_list_get(serv->rlcts, j);
		Bool has_rs_hdr=GF_FALSE;

		const char *src_ip;
		char szIP[GF_MAX_IP_NAME_LEN];
		src_ip = ctx->ifce;
		if (!src_ip) {
			if (gf_sk_get_local_ip(rlct->sock, szIP) != GF_OK)
				strcpy(szIP, "127.0.0.1");
			src_ip = szIP;
		}

		for (i=0; i<nb_pids; i++) {
			const GF_PropertyValue *p;
			ROUTEPid *rpid = gf_list_get(serv->pids, i);
			if (rpid->manifest_type) continue;
			if (rpid->rlct != rlct) continue;

			if (!has_rs_hdr) {
				gf_dynstrcat(&payload_text, " <RS dIpAddr=\"", NULL);
				gf_dynstrcat(&payload_text, rlct->ip, NULL);
				snprintf(temp, 1000, "\" dPort=\"%d\" sIpAddr=\"", rlct->port);
				gf_dynstrcat(&payload_text, temp, NULL);
				gf_dynstrcat(&payload_text, src_ip, NULL);
				gf_dynstrcat(&payload_text, "\">\n", NULL);
				has_rs_hdr = GF_TRUE;
			}

			if (rpid->bandwidth) {
				u32 kbps = rpid->bandwidth / 1000;
				kbps *= 110;
				kbps /= 100;
				snprintf(temp, 100, "  <LS tsi=\"%d\" bw=\"%d\">\n", rpid->tsi, kbps);
			} else {
				snprintf(temp, 100, "  <LS tsi=\"%d\">\n", rpid->tsi);
			}
			gf_dynstrcat(&payload_text, temp, NULL);

			if (serv->manifest) {
				gf_dynstrcat(&payload_text, "   <SrcFlow rt=\"true\">\n", NULL);
			} else {
				gf_dynstrcat(&payload_text, "   <SrcFlow rt=\"false\">\n", NULL);
			}

			if (ctx->korean) {
				gf_dynstrcat(&payload_text, "    <EFDT version=\"0\">\n", NULL);
			} else {
				gf_dynstrcat(&payload_text, "    <EFDT>\n", NULL);
			}

			p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_TEMPLATE);
			if (p) {
				char *sep, *sep2, *key = "$Number";
				strcpy(temp, p->value.string);
				sep = strstr(temp, "$Number");
				sep2 = strstr(temp, "$Time");
				if (sep && sep2) {
					gf_free(payload_text);
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] DASH Template is %s but ROUTE cannot use both Time and Number !\n", serv->log_name, p->value.string));
					serv->is_done = GF_TRUE;
					return GF_SERVICE_ERROR;
				}
				rpid->use_time_tpl = GF_FALSE;
				if (!sep) {
					sep = sep2;
					key = "$Time";
					rpid->use_time_tpl = GF_TRUE;
				}
				if (sep) {
					sep[0] = 0;
					strcat(temp, "$TOI");
					sep = strstr(p->value.string, key);
					strcat(temp, sep + strlen(key));
				}
			}

			if (ctx->korean) {
				if (p) {
					gf_dynstrcat(&payload_text, "     <FileTemplate>", NULL);
					gf_dynstrcat(&payload_text, temp, NULL);
					gf_dynstrcat(&payload_text, "</FileTemplate>\n", NULL);
				}
				gf_dynstrcat(&payload_text, "     <FDTParameters>\n", NULL);
			} else {
				u32 max_size = 0;
				gf_dynstrcat(&payload_text, "     <FDT-Instance afdt:efdtVersion=\"0\"", NULL);
				if (p) {
					gf_dynstrcat(&payload_text, " afdt:fileTemplate=\"", NULL);
					gf_dynstrcat(&payload_text, temp, NULL);
					gf_dynstrcat(&payload_text, "\"", NULL);
				}

				if (!rpid->bandwidth || !rpid->dash_dur.den || !rpid->dash_dur.num) {
					switch (rpid->stream_type) {
					case GF_STREAM_VISUAL: max_size = 5000000; break;
					case GF_STREAM_AUDIO: max_size = 1000000; break;
					default: max_size = 100000; break;
					}
				} else {
					max_size = rpid->bandwidth / 8;
					max_size *= rpid->dash_dur.num;
					max_size /= rpid->dash_dur.den;
					//use 2x avg rate announced as safety
					max_size *= 2;
				}

				snprintf(temp, 1000, " Expires=\"4000000000\" afdt:maxTransportSize=\"%d\">\n", max_size);
				gf_dynstrcat(&payload_text, temp, NULL);
			}

			if (rpid->init_seg_name) {
				snprintf(temp, 1000, "      <fdt:File Content-Location=\"%s\" TOI=\"%u\"/>\n", rpid->init_seg_name, ROUTE_INIT_TOI);
				gf_dynstrcat(&payload_text, temp, NULL);
			}
			if (rpid->hld_child_pl_name) {
				snprintf(temp, 1000, "      <fdt:File Content-Location=\"%s\" TOI=\"%u\"/>\n", rpid->hld_child_pl_name, ROUTE_INIT_TOI - 1);
				gf_dynstrcat(&payload_text, temp, NULL);
			}
			if (rpid->raw_file) {
				const char *mime, *url;
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_MIME);
				if (p && p->value.string && strcmp(p->value.string, "*"))
					mime = p->value.string;
				else {
					mime = "application/octet-string";
				}
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_ROUTE_NAME);
				if (p && p->value.string)
					url = p->value.string;
				else {
					p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_URL);
					url = (p && p->value.string) ? gf_file_basename(p->value.string) : "/dev/null";
				}
				snprintf(temp, 1000, "      <fdt:File Content-Location=\"%s\" Content-Type=\"%s\" TOI=\"%u\"/>\n", url, mime, rpid->current_toi);
				gf_dynstrcat(&payload_text, temp, NULL);
			}

			if (ctx->korean) {
				gf_dynstrcat(&payload_text, "     </FDTParameters>\n", NULL);
			} else {
                gf_dynstrcat(&payload_text, "     </FDT-Instance>\n", NULL);
			}
			gf_dynstrcat(&payload_text, "    </EFDT>\n", NULL);

			if (rpid->stream_type) {
				const char *rep_id;
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_REP_ID);
				if (p && p->value.string) {
					rep_id = p->value.string;
				} else {
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing representation ID on PID (broken input filter), using \"1\"\n", serv->log_name));
					rep_id = "1";
				}
				gf_dynstrcat(&payload_text, "    <ContentInfo>\n", NULL);
				gf_dynstrcat(&payload_text, "     <MediaInfo repId=\"", NULL);
				gf_dynstrcat(&payload_text, rep_id, NULL);
				gf_dynstrcat(&payload_text, "\"", NULL);
				switch (rpid->stream_type) {
				case GF_STREAM_VISUAL:
					gf_dynstrcat(&payload_text, " contentType=\"video\"", NULL);
					break;
				case GF_STREAM_AUDIO:
					gf_dynstrcat(&payload_text, " contentType=\"audio\"", NULL);
					break;
				case GF_STREAM_TEXT:
					gf_dynstrcat(&payload_text, " contentType=\"subtitles\"", NULL);
					break;
				//other stream types are not mapped in ATSC3, do not set content type
				}
				gf_dynstrcat(&payload_text, "/>\n", NULL);
				gf_dynstrcat(&payload_text, "    </ContentInfo>\n", NULL);
			}
			//setup payload format - we remove srcFecPayloadId=\"0\" as there is no FEC support yet
			//format is always 1 for the time being
			snprintf(temp, 1000,
					"    <Payload codePoint=\"%d\" formatId=\"1\" frag=\"0\" order=\"true\"/>\n"
					, rpid->fmtp);

			gf_dynstrcat(&payload_text, temp, NULL);

			gf_dynstrcat(&payload_text,
					"   </SrcFlow>\n"
					"  </LS>\n", NULL);
		}
		if (has_rs_hdr)
			gf_dynstrcat(&payload_text, " </RS>\n", NULL);
	}

	gf_dynstrcat(&payload_text, "</S-TSID>\n\r\n", NULL);

	gf_dynstrcat(&payload_text, "--"MULTIPART_BOUNDARY"--\n", NULL);


	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Updated Manifest+S-TSID bundle to:\n%s\n", serv->log_name, payload_text));

	if (serv->stsid_bundle) gf_free(serv->stsid_bundle);
	serv->stsid_bundle = (u8 *) payload_text;
	serv->stsid_bundle_size = 1 + (u32) strlen(payload_text);
	if(!ctx->nozip) {
		//compress and store as final payload
		gf_gz_compress_payload_ex(&serv->stsid_bundle, serv->stsid_bundle_size, &serv->stsid_bundle_size, 0, GF_FALSE, NULL, GF_TRUE);
		
		serv->stsid_bundle_toi = 0x80000000; //compressed
	}
	if (manifest_updated) serv->stsid_bundle_toi |= (1<<18);
	if (serv->needs_reconfig) {
		serv->stsid_bundle_toi |= (1<<17);
		serv->stsid_bundle_toi |= (serv->stsid_version & 0xFF);
	} else if (manifest_updated) {
		serv->stsid_bundle_toi |= (serv->manifest_version & 0xFF);
	}

	serv->needs_reconfig = GF_FALSE;

	//reset last sent time
	serv->last_stsid_clock = 0;
	return GF_OK;
}

#include <gpac/base_coding.h>

static void inject_fdt_file_desc(GF_ROUTEOutCtx *ctx, char **payload, ROUTEService *serv, char *url, char *mime, const u8 *data, u32 size, u32 TOI, Bool use_full_url)
{
	char tmp[100];
	gf_dynstrcat(payload, "<File FEC-OTI-FEC-Encoding-ID=\"0\" FEC-OTI-Maximum-Source-Block-Length=\"65535\" Content-Length=\"", NULL);
	sprintf(tmp, "%u", size);
	gf_dynstrcat(payload, tmp, NULL);
	gf_dynstrcat(payload, "\" Transfer-Length=\"", NULL);
	gf_dynstrcat(payload, tmp, NULL);
	gf_dynstrcat(payload, "\" Content-Location=\"", NULL);
	if (use_full_url && serv && serv->manifest_server) {
		gf_dynstrcat(payload, serv->manifest_server, NULL);
		gf_dynstrcat(payload, serv->manifest_url, "/");
		gf_dynstrcat(payload, url, NULL);
	} else {
		gf_dynstrcat(payload, url, NULL);
	}
	gf_dynstrcat(payload, "\" Content-Type=\"", NULL);
	gf_dynstrcat(payload, mime, NULL);
	gf_dynstrcat(payload, "\" FEC-OTI-Encoding-Symbol-Length=\"", NULL);
	sprintf(tmp, "%u", ctx->flute_msize);
	gf_dynstrcat(payload, tmp, NULL);
	gf_dynstrcat(payload, "\" TOI=\"", NULL);
	sprintf(tmp, "%u", TOI);
	gf_dynstrcat(payload, tmp, NULL);
	gf_dynstrcat(payload, "\"", NULL);

	if (ctx->csum && data) {
		u8 digest[GF_MD5_DIGEST_SIZE];
		gf_md5_csum(data, size, digest);
		u32 db64_len = gf_base64_encode(digest, GF_MD5_DIGEST_SIZE, tmp, 100);
		tmp[db64_len]=0;
		gf_dynstrcat(payload, " Content-MD5=\"", NULL);
		gf_dynstrcat(payload, tmp, NULL);
		gf_dynstrcat(payload, "\"", NULL);
	}
	gf_dynstrcat(payload, "/>\n", NULL);
}

static void routeout_send_mabr_manifest(GF_ROUTEOutCtx *ctx);

static GF_Err routeout_update_dvb_mabr_fdt(GF_ROUTEOutCtx *ctx, ROUTEService *serv, Bool manifest_updated)
{
	u32 i, nb_serv;
	if (serv && ctx->dvb_mabr_fdt && !serv->needs_reconfig) return GF_OK;
	char *payload=NULL;
	gf_dynstrcat(&payload, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n", NULL);
	gf_dynstrcat(&payload, "<FDT-Instance Expires=\"3916741152\" xmlns=\"urn:IETF:metadata:2005:FLUTE:FDT\">\n", NULL);

	if (!ctx->dvb_mabr_config_toi) {
		ctx->dvb_mabr_config_toi = ctx->next_toi_avail;
		ctx->next_toi_avail++;
	}
	inject_fdt_file_desc(ctx, &payload, NULL, "dvb_mabr_config.xml", "application/xml+dvb-mabr-session-configuration", ctx->dvb_mabr_config, ctx->dvb_mabr_config_len, ctx->dvb_mabr_config_toi, GF_FALSE);

	nb_serv = gf_list_count(ctx->services);
	for (i=0; i<nb_serv; i++) {
		ROUTEService *serv = gf_list_get(ctx->services, i);
		if (!serv->use_flute) continue;
		//inject manifest
		if (serv->manifest && serv->manifest_type) {
			u32 len = (u32) strlen(serv->manifest);
			if (!serv->mani_toi) {
				serv->mani_toi = ctx->next_toi_avail;
				ctx->next_toi_avail++;
			}
			inject_fdt_file_desc(ctx, &payload, serv, serv->manifest_name,
				(serv->manifest_type==2) ? "application/vnd.apple.mpegURL" : "application/dash+xml",
					serv->manifest, len, serv->mani_toi, ctx->furl);
		}

		//inject init segs and HLS variant or RAW info
		u32 j, nb_pids = gf_list_count(serv->pids);
		for (j=0; j<nb_pids; j++) {
			ROUTEPid *pid = gf_list_get(serv->pids, j);
			if (pid->raw_file) {
				if (!pid->current_toi || !pid->full_frame_size)
					continue;

				char *mime;
				const GF_PropertyValue *p = gf_filter_pid_get_property(pid->pid, GF_PROP_PID_MIME);
				if (p && p->value.string && strcmp(p->value.string, "*")) {
					mime = p->value.string;
				} else {
					mime = "application/octet-string";
				}
				inject_fdt_file_desc(ctx, &payload, serv, pid->seg_name, mime, pid->pck_data, pid->full_frame_size, pid->current_toi, ctx->furl);
				continue;
			}

			if (pid->init_seg_data) {
				if (!pid->init_toi) {
					pid->init_toi = ctx->next_toi_avail;
					ctx->next_toi_avail++;
				}
				inject_fdt_file_desc(ctx, &payload, serv, pid->init_seg_name, "video/mp4", pid->init_seg_data, pid->init_seg_size, pid->init_toi, ctx->furl);
			}
			if (pid->hld_child_pl) {
				if (!pid->hls_child_toi) {
					pid->hls_child_toi = ctx->next_toi_avail;
					ctx->next_toi_avail++;
				}
				inject_fdt_file_desc(ctx, &payload, serv, pid->hld_child_pl_name, "application/vnd.apple.mpegURL", pid->hld_child_pl, (u32) strlen(pid->hld_child_pl), pid->hls_child_toi, ctx->furl);
			}
		}
	}


	gf_dynstrcat(&payload, "</FDT-Instance>", NULL);

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Updated Bootstrap FDT info to:\n%s\n", ctx->log_name, payload));
	if (ctx->dvb_mabr_fdt) gf_free(ctx->dvb_mabr_fdt);
	ctx->dvb_mabr_fdt = payload;
	ctx->dvb_mabr_fdt_len = (u32) strlen(payload);
	ctx->last_dvb_mabr_clock = 0;
	ctx->dvb_mabr_fdt_instance_id = (ctx->dvb_mabr_fdt_instance_id+1) % 0xFFFFF;
	return GF_OK;
}


u32 routeout_lct_send(GF_ROUTEOutCtx *ctx, GF_Socket *sock, u32 tsi, u32 toi, u32 codepoint, u8 *payload, u32 len, u32 offset, ROUTEService *serv, u32 total_size, u32 offset_in_frame, u32 fdt_instance_id, Bool is_flute)
{
	u32 max_size = ctx->mtu;
	u32 send_payl_size;
	u32 hdr_len = 4;
	u32 hpos;
	Bool short_h=GF_FALSE;
	GF_Err e;

	if (is_flute) {
		codepoint = 0;
		hdr_len = 2;
		if ((tsi<0xFFFF) && (toi<0xFFFF)) {
			hdr_len+=1;
			short_h = GF_TRUE;
		} else {
			hdr_len+=2;
		}
		//add ext for FDT
		if (!toi) {
			hdr_len+=5;
		}
	} else {
		hdr_len = 4;
		if (total_size) {
			//TOL extension
			if (total_size<=0xFFFFFF) hdr_len += 1;
			else hdr_len += 2;
		}
	}
	//start offset is not in header
	send_payl_size = 4 * (hdr_len+1) + len - offset;
	if (send_payl_size > max_size) {
		if (is_flute)
			send_payl_size = ctx->flute_msize;
		else
			send_payl_size = max_size - 4 * (hdr_len+1);
	} else {
		send_payl_size = len - offset;
	}
	ctx->lct_buffer[0] = 0x12; //V=b0001, C=b00, PSI=b10
	//S=b1|b0, O=b01|b00, h=b0|b1, res=b00, A=b0, B=X
	ctx->lct_buffer[1] = short_h ? 0x10 : 0xA0;
	//set close flag only if total_len is known
	if (total_size && (offset + send_payl_size == len))
		ctx->lct_buffer[1] |= 1;

	ctx->lct_buffer[2] = hdr_len;
	ctx->lct_buffer[3] = (u8) codepoint;
	hpos = 4;

#define PUT_U32(_val)\
	ctx->lct_buffer[hpos] = (_val>>24 & 0xFF);\
	ctx->lct_buffer[hpos+1] = (_val>>16 & 0xFF);\
	ctx->lct_buffer[hpos+2] = (_val>>8 & 0xFF);\
	ctx->lct_buffer[hpos+3] = (_val & 0xFF); \
	hpos+=4;

#define PUT_U16(_val)\
	ctx->lct_buffer[hpos] = (_val>>8 & 0xFF);\
	ctx->lct_buffer[hpos+1] = (_val & 0xFF);\
	hpos+=2;


	//CCI=0
	PUT_U32(0);
	if (short_h) {
		PUT_U16(tsi);
		PUT_U16(toi);
	} else {
		PUT_U32(tsi);
		PUT_U32(toi);
	}

	if (is_flute) {
		u32 ESI = offset_in_frame/ctx->flute_msize;
		if (toi==0) {
			gf_bs_reassign_buffer(ctx->lct_bs, ctx->lct_buffer+hpos, ctx->mtu);
			//set FDT
			gf_bs_write_u8(ctx->lct_bs, GF_LCT_EXT_FDT);
			gf_bs_write_int(ctx->lct_bs, 1, 4);
			gf_bs_write_int(ctx->lct_bs, fdt_instance_id, 20);
			hpos+=4; //32 bits

			//set FTI
			gf_bs_write_u8(ctx->lct_bs, GF_LCT_EXT_FTI); //TOH
			gf_bs_write_u8(ctx->lct_bs, 4); //TOL
			//48 bits of transfer length
			gf_bs_write_long_int(ctx->lct_bs, total_size, 48);
			//16bits of fec instanceID
			gf_bs_write_u16(ctx->lct_bs, 0);
			//16bits of symbol length
			gf_bs_write_u16(ctx->lct_bs, ctx->flute_msize);
			//32bits of max source block length
			gf_bs_write_u32(ctx->lct_bs, ctx->mtu);
			hpos+=16; //4 32bit words
		}
		PUT_U16(0)
		PUT_U16(ESI)
	} else {

		//total length
		if (total_size) {
			if (total_size<=0xFFFFFF) {
				ctx->lct_buffer[hpos] = GF_LCT_EXT_TOL24;
				ctx->lct_buffer[hpos+1] = total_size>>16 & 0xFF;
				ctx->lct_buffer[hpos+2] = total_size>>8 & 0xFF;
				ctx->lct_buffer[hpos+3] = total_size & 0xFF;
				hpos+=4;
			} else {
				ctx->lct_buffer[hpos] = GF_LCT_EXT_TOL48;
				ctx->lct_buffer[hpos+1] = 2; //2 x 32 bits for header ext
				ctx->lct_buffer[hpos+2] = 0;
				ctx->lct_buffer[hpos+3] = 0;
				hpos+=4;
				PUT_U32(total_size);
			}
		}
		//start_offset
		PUT_U32(offset_in_frame);
	}

	gf_assert(send_payl_size+hpos <= ctx->mtu);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] LCT TSI %u TOI %u size %u (frag %u total %u) offset %u (%u in obj)\n", serv ? serv->log_name : ctx->log_name, tsi, toi, send_payl_size, len, total_size, offset, offset_in_frame));

	memcpy(ctx->lct_buffer + hpos, payload + offset, send_payl_size);
	e = gf_sk_send(sock, ctx->lct_buffer, send_payl_size + hpos);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to send LCT object TSI %u TOI %u fragment: %s\n", serv ? serv->log_name : ctx->log_name, tsi, toi, gf_error_to_string(e) ));
	}
	//store what we actually sent including header for rate estimation
	ctx->bytes_sent += send_payl_size + hpos;
	//but return what we sent from the source
	return send_payl_size;
}

static void routeout_send_file(GF_ROUTEOutCtx *ctx, ROUTEService *serv, GF_Socket *sock, u32 tsi, u32 toi, u8 *payload, u32 size, u32 codepoint, u32 fdt_instance_id, Bool is_flute)
{
	u32 offset=0;
	while (offset<size) {
		offset += routeout_lct_send(ctx, sock, tsi, toi, codepoint, payload, size, offset, serv, size, offset, fdt_instance_id, is_flute);
	}
}

static GF_Err routeout_service_send_stsid_bundle(GF_ROUTEOutCtx *ctx, ROUTEService *serv)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Sent bundle (%d bytes)\n", serv->log_name, serv->stsid_bundle_size));
	/*send lct with codepoint "NRT - Unsigned Package Mode" (multipart):
	"In broadcast delivery of SLS for a DASH-formatted streaming Service delivered using ROUTE, since SLS fragments are NRT files in nature,
	their carriage over the ROUTE session/LCT channel assigned by the SLT shall be in accordance to the Unsigned Packaged Mode or
	the Signed Package Mode as described in Section A.3.3.4 and A.3.3.5, respectively"
	*/
	routeout_send_file(ctx, serv, serv->rlct_base->sock, 0, serv->stsid_bundle_toi, serv->stsid_bundle, serv->stsid_bundle_size, 3, 0, GF_FALSE);
	return GF_OK;
}

static GF_Err routeout_fetch_packet(GF_ROUTEOutCtx *ctx, ROUTEPid *rpid)
{
	const GF_PropertyValue *p;
	Bool start, end;
	u64 pck_dur;
	u64 ts;
	Bool has_ts;
	Bool pck_dur_for_segment;

retry:

	rpid->current_pck = gf_filter_pid_get_packet(rpid->pid);
	if (!rpid->current_pck) {
		if (gf_filter_pid_is_eos(rpid->pid) && !gf_filter_pid_is_flush_eos(rpid->pid) ) {
#if 0
			if (gf_filter_reporting_enabled(filter)) {
				char szStatus[1024];
				snprintf(szStatus, 1024, "%s: done - wrote "LLU" bytes", gf_file_basename(ctx->szFileName), ctx->nb_write);
				gf_filter_update_status(filter, 10000, szStatus);
			}
#endif

			if (rpid->route->dash_mode && rpid->res_size) {
				GF_FilterEvent evt;
				GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, rpid->pid);
				evt.seg_size.seg_url = NULL;

				if (rpid->route->dash_mode==1) {
					evt.seg_size.is_init = 1;
					rpid->route->dash_mode = 2;
					evt.seg_size.media_range_start = 0;
					evt.seg_size.media_range_end = 0;
					gf_filter_pid_send_event(rpid->pid, &evt);
				} else {
					evt.seg_size.is_init = 0;
					evt.seg_size.media_range_start = rpid->offset_at_seg_start;
					//end range excludes last byte, except if 0 size (some text segments)
					evt.seg_size.media_range_end = rpid->res_size ? (rpid->res_size - 1) : 0;
					gf_filter_pid_send_event(rpid->pid, &evt);
				}
			}
			//done
			rpid->res_size = 0;
			return GF_OK;
		}
		return GF_OK;
	}
	//skip eods packets
	if (rpid->route->dash_mode && gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_EODS)) {
		gf_filter_pid_drop_packet(rpid->pid);
		rpid->current_pck = NULL;
		goto retry;
	}

	gf_filter_pck_ref(&rpid->current_pck);
	gf_filter_pid_drop_packet(rpid->pid);

	gf_filter_pck_get_framing(rpid->current_pck, &start, &end);

	//trash redundant info for raw files
	if (rpid->raw_file) {
		u32 dep_flags = gf_filter_pck_get_dependency_flags(rpid->current_pck);
		//redundant packet, do not store
		if ((dep_flags & 0x3) == 1) {
			gf_filter_pck_unref(rpid->current_pck);
			rpid->current_pck = NULL;
			goto retry;
		}
	}

	rpid->pck_data = gf_filter_pck_get_data(rpid->current_pck, &rpid->pck_size);
	rpid->pck_offset = 0;

	p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_INIT);
	if (p && p->value.boolean) {
		u32 crc;
		gf_assert(start && end);
		crc = gf_crc_32(rpid->pck_data, rpid->pck_size);
		//whenever init seg changes, reconfig service
		if (crc != rpid->init_seg_crc) {
			rpid->init_seg_crc = crc;
			rpid->route->needs_reconfig = GF_TRUE;
			rpid->current_toi = 0;
			if (rpid->init_seg_data) gf_free(rpid->init_seg_data);
			rpid->init_seg_data = gf_malloc(rpid->pck_size);
			memcpy(rpid->init_seg_data, rpid->pck_data, rpid->pck_size);
			rpid->init_seg_size = rpid->pck_size;
			rpid->init_seg_sent = GF_FALSE;
			p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_FILENAME);
			if (rpid->init_seg_name) gf_free(rpid->init_seg_name);
			rpid->init_seg_name = p ? routeout_strip_base(rpid->route, p->value.string) : NULL;
		}
		gf_filter_pck_unref(rpid->current_pck);
		rpid->current_pck = NULL;
		goto retry;
	}

	p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PID_HLS_PLAYLIST);
	if (p && p->value.string) {
		u32 crc;
		gf_assert(start && end);
		crc = gf_crc_32(rpid->pck_data, rpid->pck_size);
		//whenever init seg changes, bump stsid version
		if (crc != rpid->hld_child_pl_crc) {
			rpid->hld_child_pl_crc = crc;
			if (rpid->hld_child_pl) gf_free(rpid->hld_child_pl);
			rpid->hld_child_pl = gf_malloc(rpid->pck_size+1);
			memcpy(rpid->hld_child_pl, rpid->pck_data, rpid->pck_size);
			rpid->hld_child_pl[rpid->pck_size] = 0;

			if (!rpid->hld_child_pl_name || strcmp(rpid->hld_child_pl_name, p->value.string)) {
				rpid->route->needs_reconfig = GF_TRUE;
				if (rpid->hld_child_pl_name) gf_free(rpid->hld_child_pl_name);
				rpid->hld_child_pl_name = routeout_strip_base(rpid->route, p->value.string);
			}
		}
		gf_filter_pck_unref(rpid->current_pck);
		rpid->current_pck = NULL;
		goto retry;
	}
	if (rpid->route->dash_mode) {
		p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_FILENUM);
		if (p) {
			GF_FilterEvent evt;

			GF_FEVT_INIT(evt, GF_FEVT_SEGMENT_SIZE, rpid->pid);
			evt.seg_size.seg_url = NULL;

			if (rpid->route->dash_mode==1) {
				evt.seg_size.is_init = 1;
				rpid->route->dash_mode = 2;
				evt.seg_size.media_range_start = 0;
				evt.seg_size.media_range_end = 0;
				gf_filter_pid_send_event(rpid->pid, &evt);
			} else {
				evt.seg_size.is_init = 0;
				evt.seg_size.media_range_start = rpid->offset_at_seg_start;
				//end range excludes last byte, except if 0 size (some text segments)
				evt.seg_size.media_range_end = rpid->res_size ? (rpid->res_size - 1) : 0;
				rpid->offset_at_seg_start = evt.seg_size.media_range_end;
				gf_filter_pid_send_event(rpid->pid, &evt);
			}
			if ( gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_FILENAME))
				start = GF_TRUE;
		}
	}

	if (rpid->raw_file) {
		gf_assert(start && end);

		if (rpid->seg_name) gf_free(rpid->seg_name);
		rpid->seg_name = "unknown";
		p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_ROUTE_NAME);
		if (p)
			rpid->seg_name = p->value.string;
		else {
			p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_URL);
			if (p) rpid->seg_name = gf_file_basename(p->value.string);
		}
		rpid->seg_name = gf_strdup(rpid->seg_name);

		//setup carousel period
		rpid->carousel_time_us = ctx->carousel;
		p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_ROUTE_CAROUSEL);
		if (p) {
			rpid->carousel_time_us = gf_timestamp_rescale(p->value.frac.num, p->value.frac.den, 1000000);
		}
		//setup upload time
		rpid->current_dur_us = ctx->carousel;
		p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_ROUTE_SENDTIME);
		if (p) {
			rpid->current_dur_us = p->value.frac.num;
			rpid->current_dur_us *= 1000000;
			rpid->current_dur_us /= p->value.frac.den;
		}
		if (rpid->carousel_time_us && (rpid->current_dur_us>rpid->carousel_time_us)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Requested upload time of file "LLU" is greater than its carousel time "LLU", adjusting carousel\n", rpid->route->log_name, rpid->current_dur_us, rpid->carousel_time_us));
			rpid->carousel_time_us = rpid->current_dur_us;
		}
		rpid->clock_at_pck = rpid->current_cts_us = rpid->cts_us_at_frame_start = ctx->clock;
		rpid->full_frame_size = rpid->pck_size;
		return GF_OK;
	}

	if (start) {
		p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_FILENAME);
		if (rpid->seg_name) gf_free(rpid->seg_name);
		if (p) {
			if (rpid->use_basename)
				rpid->seg_name = gf_strdup(gf_file_basename(p->value.string));
			else
				rpid->seg_name = routeout_strip_base(rpid->route, p->value.string);
		} else {
			rpid->seg_name = gf_strdup("unknown");
		}

		//file num increased per packet, open new file
		Bool in_error = GF_FALSE;
		p = rpid->use_time_tpl ? gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_MPD_SEGSTART) : NULL;
		if (rpid->use_time_tpl && !p) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing MPD Time on segment, cannot map to TOI - aborting\n", rpid->route->log_name));
			in_error = GF_TRUE;
		}
		if (ctx->dvb_mabr) {
			//keep using only 16bits but no TOI =0 (reserved for FDT)
			rpid->current_toi = (rpid->current_toi+1) % 0xFFFF;
			if (!rpid->current_toi) rpid->current_toi = 1;
		} else if (p || in_error) {
			if (p && p->value.lfrac.num >= ROUTE_INIT_TOI) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] MPD Time "LLU" greater than 32 bits, cannot map on TOI - aborting\n", rpid->route->log_name, p->value.lfrac.num));
				in_error = GF_TRUE;
			}
			if (in_error) {
				u32 i;
				GF_FilterEvent evt;
				GF_FEVT_INIT(evt, GF_FEVT_STOP, NULL);
				rpid->route->is_done = GF_TRUE;
				for (i=0; i<gf_list_count(rpid->route->pids); i++) {
					ROUTEPid *r_pid = gf_list_get(rpid->route->pids, i);
					gf_filter_pid_set_discard(r_pid->pid, GF_TRUE);
					evt.base.on_pid = r_pid->pid;
					gf_filter_pid_send_event(r_pid->pid, &evt);
				}
				return GF_SERVICE_ERROR;
			}
			rpid->current_toi = (u32) p->value.lfrac.num;
		} else {
			p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_FILENUM);
			if (p) {
				rpid->current_toi = p->value.uint;
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing filenum on segment %s, something is wrong in source chain - assuming +1 increase\n", rpid->route->log_name, rpid->seg_name));
				rpid->current_toi ++;
			}
		}
		rpid->frag_idx = 0;
		rpid->full_frame_size = end ? rpid->pck_size : 0;
		rpid->cumulated_frag_size = rpid->pck_size;
		rpid->push_init = GF_TRUE;
		rpid->frag_offset = 0;
	} else {
		rpid->frag_idx++;
		if (ctx->dvb_mabr) {
			//one object per fragment in flute
			//keep using only 16bits but no TOI =0 (reserved for FDT)
			rpid->current_toi = (rpid->current_toi+1) % 0xFFFF;
			if (!rpid->current_toi) rpid->current_toi = 1;
		}
		rpid->cumulated_frag_size += rpid->pck_size;
		rpid->push_frag_name = (ctx->dvb_mabr && ctx->llmode) ? GF_TRUE : GF_FALSE;
		if (end) {
			rpid->full_frame_size = rpid->cumulated_frag_size;
			rpid->force_send_empty = GF_TRUE;
		}
	}


	pck_dur = gf_filter_pck_get_duration(rpid->current_pck);
	//check if duration is for the entire segment or this fragment (cf forward=file in dmx_dash.c)
	pck_dur_for_segment = GF_FALSE;
	if (start) {
		rpid->pck_dur_at_frame_start = 0;
		if (gf_filter_pck_get_carousel_version(rpid->current_pck))
			pck_dur_for_segment = GF_TRUE;
	}

	ts = gf_filter_pck_get_cts(rpid->current_pck);
	has_ts = (ts==GF_FILTER_NO_TS) ? GF_FALSE : GF_TRUE;

	if (
		//no TS and not initial fragment, recompute timing and dur
		(!start && !has_ts && rpid->bitrate && rpid->pck_dur_at_frame_start)
		//has TS, initial fragment and duration for the entire segment, recompute dur only
		|| (has_ts && start && !end && pck_dur && pck_dur_for_segment)
	) {
		u64 frag_time, tot_est_size;

		//fragment start, store packed duration
		if (has_ts) {
			rpid->pck_dur_at_frame_start = (u32) pck_dur;
		}
		//compute estimated file size based on segment duration and rate, use 10% overhead
		tot_est_size = gf_timestamp_rescale(rpid->bitrate, rpid->timescale, rpid->pck_dur_at_frame_start);
		tot_est_size /= 8;

		//our estimate was too small...
		if (tot_est_size < rpid->cumulated_frag_size)
			tot_est_size = rpid->cumulated_frag_size;

		//compute timing proportional to packet duration, with ratio of current size / tot_est_size
		pck_dur = rpid->pck_size * rpid->pck_dur_at_frame_start / tot_est_size;
		if (!pck_dur) pck_dur = 1;

		if (!has_ts) {
			frag_time = (rpid->cumulated_frag_size-rpid->pck_size) *  rpid->pck_dur_at_frame_start / tot_est_size;
			ts = rpid->cts_at_frame_start + frag_time;
		} else {
			frag_time = 0;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Missing timing for fragment %d of segment %s - timing estimated from bitrate: TS "LLU" ("LLU" in segment) dur "LLU"\n", rpid->route->log_name, rpid->frag_idx, rpid->seg_name, ts, frag_time, pck_dur));
	}

	if (ts!=GF_FILTER_NO_TS) {
		u64 diff;
		if (!rpid->clock_at_first_pck) {
			rpid->clock_at_first_pck = ctx->clock;
			rpid->cts_first_pck = ts;
		}
		//move to microsecs
		diff = gf_timestamp_rescale(ts - rpid->cts_first_pck, rpid->timescale, 1000000);

		rpid->current_cts_us = rpid->clock_at_first_pck + diff;
		rpid->clock_at_pck = ctx->clock;

		if (start) {
			rpid->clock_at_frame_start = ctx->clock;
			rpid->cts_us_at_frame_start = rpid->current_cts_us;
			rpid->cts_at_frame_start = ts;
		}

		rpid->current_dur_us = pck_dur;
		if (!rpid->current_dur_us) {
			rpid->current_dur_us = rpid->timescale;
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing duration on segment %s, something is wrong in source chain, will not be able to regulate correctly\n", rpid->route->log_name, rpid->seg_name));
		}
		rpid->current_dur_us = gf_timestamp_rescale(rpid->current_dur_us, rpid->timescale, 1000000);
	} else if (start && end) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing timing on segment %s, using previous fragment timing CTS "LLU" duration "LLU" us\nSomething could be wrong in demux chain, will not be able to regulate correctly\n", rpid->route->log_name, rpid->seg_name, rpid->current_cts_us-rpid->clock_at_first_pck, rpid->current_dur_us));
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing timing on fragment %d of segment %s, using previous fragment timing CTS "LLU" duration "LLU" us\nSomething could be wrong in demux chain, will not be able to regulate correctly\n", rpid->route->log_name, rpid->frag_idx, rpid->seg_name, rpid->current_cts_us-rpid->clock_at_first_pck, rpid->current_dur_us));
	}

	rpid->res_size += rpid->pck_size;
	return GF_OK;
}

void routeout_send_fdt(GF_ROUTEOutCtx *ctx, ROUTEService *serv, ROUTEPid *rpid)
{
	char *payload = NULL;
	char szName[GF_MAX_PATH], *seg_name;

	gf_dynstrcat(&payload, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n", NULL);
	gf_dynstrcat(&payload, "<FDT-Instance Expires=\"3916741152\" xmlns=\"urn:IETF:metadata:2005:FLUTE:FDT\">\n", NULL);
	//todo: inject manifest, init seg and child playlist ?

	//cannot use TOI 0 for anything else than FDT
	if (!rpid->current_toi)
		rpid->current_toi = 1;

	const u8 *pck_data;
	if (!rpid->frag_idx && (ctx->csum==DVB_CSUM_ALL))
		pck_data = rpid->pck_data;
	else
		pck_data = NULL;

	if (ctx->llmode && (rpid->pck_size!=rpid->full_frame_size)) {
		if (rpid->full_frame_size)
			sprintf(szName, "%s?isLast#%u", rpid->seg_name, rpid->frag_offset);
		else
			sprintf(szName, "%s#%u", rpid->seg_name, rpid->frag_offset);
		seg_name = szName;
	} else {
		seg_name = rpid->seg_name;
	}
	inject_fdt_file_desc(ctx, &payload, serv, seg_name, "video/mp4", pck_data, rpid->pck_size, rpid->current_toi, ctx->furl);
	gf_dynstrcat(&payload, "</FDT-Instance>", NULL);

	if (payload) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Sending FDT for segment %s:\n%s\n", serv->log_name, seg_name, payload));
		routeout_send_file(ctx, serv, rpid->rlct->sock, rpid->tsi, 0, payload, (u32) strlen(payload), 0, rpid->fdt_instance_id, GF_TRUE);
		gf_free(payload);
		rpid->fdt_instance_id = (rpid->fdt_instance_id+1) % 0xFFFFF;
	}
}

static GF_Err routeout_process_service(GF_ROUTEOutCtx *ctx, ROUTEService *serv)
{
	u32 i, count, nb_done;
	Bool manifest_sent=GF_FALSE;
	if (!serv->service_ready) return GF_OK;

	//carousel STSID bundle
	if (serv->stsid_bundle) {
		u64 diff = ctx->clock - serv->last_stsid_clock;
		if (!serv->last_stsid_clock || (diff >= ctx->carousel)) {
			routeout_service_send_stsid_bundle(ctx, serv);
			serv->last_stsid_clock = ctx->clock;
		} else {
			u64 next_sched = ctx->carousel - diff;
			if (next_sched < ctx->reschedule_us)
				ctx->reschedule_us = next_sched;
		}
	}

	nb_done = 0;
	count = gf_list_count(serv->pids);
	for (i=0; i<count; i++) {
		ROUTEPid *rpid = gf_list_get(serv->pids, i);
		Bool send_hls_child = rpid->update_hls_child_pl;
		if (rpid->manifest_type) {
			nb_done++;
			continue;
		}

		if (ctx->reporting_on) {
			if (rpid->full_frame_size) {
				ctx->total_size += rpid->full_frame_size;
			} else {
				ctx->total_size += rpid->pck_size;
				ctx->total_size_unknown = GF_TRUE;
			}
			ctx->total_bytes += rpid->pck_offset;
			ctx->nb_resources++;
		}

next_packet:

		if (!rpid->current_pck) {
			Bool push_init;
			GF_Err e = routeout_fetch_packet(ctx, rpid);
			if (e) return e;

			if (!rpid->current_pck) {
				if (gf_filter_pid_is_eos(rpid->pid) && !gf_filter_pid_is_flush_eos(rpid->pid))
					nb_done++;
				continue;
			}

			if (ctx->reporting_on) {
				if (rpid->full_frame_size) {
					ctx->total_size += rpid->full_frame_size;
				} else {
					ctx->total_size += rpid->pck_size;
					ctx->total_size_unknown = GF_TRUE;
				}
				ctx->nb_resources++;
			}

			//in low-latency mode, push manifest and child playlists
			push_init = rpid->push_init;
			if (ctx->llmode && !push_init
				&& rpid->carousel_time_us
				&& (gf_sys_clock_high_res() - rpid->last_init_push > rpid->carousel_time_us)
			) {
				push_init = GF_TRUE;
				if (rpid->hld_child_pl)
					send_hls_child = GF_TRUE;
			}

			if (push_init) {
				GF_Socket *init_sock = rpid->rlct->sock;
				u32 init_tsi = rpid->tsi;
				u32 init_toi = ROUTE_INIT_TOI;

				rpid->push_init = GF_FALSE;
				send_hls_child = GF_TRUE;
				if (serv->use_flute) {
					//update fdt
					routeout_send_fdt(ctx, serv, rpid);
					rpid->push_frag_name = GF_FALSE;

					if (!manifest_sent) {
						GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Sending Manifest %s\n", serv->log_name, serv->manifest_name));
						manifest_sent = GF_TRUE;
						routeout_send_file(ctx, serv, ctx->sock_dvb_mabr, ctx->dvb_mabr_tsi, serv->mani_toi, serv->manifest, (u32) strlen(serv->manifest), 0, 0, GF_TRUE);
					}
					init_sock = ctx->sock_dvb_mabr;
					init_tsi = ctx->dvb_mabr_tsi;
					init_toi = rpid->init_toi;
				}

				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Sending init segment %s\n", serv->log_name, rpid->init_seg_name));
				u32 codepoint;
				if (rpid->init_seg_sent) {
					codepoint = 7;
				} else {
					rpid->init_seg_sent = GF_FALSE;
					codepoint = 5;
				}
				//send init asap
				routeout_send_file(ctx, serv, init_sock, init_tsi, init_toi, (u8 *) rpid->init_seg_data, rpid->init_seg_size, codepoint, 0, serv->use_flute);

				if (ctx->reporting_on) {
					ctx->total_size += rpid->init_seg_size;
					ctx->total_bytes = rpid->init_seg_size;
					ctx->nb_resources++;
				}
				if (ctx->llmode)
					rpid->last_init_push = gf_sys_clock_high_res();
			}
		}
		//send child m3u8 asap
		if (send_hls_child && rpid->hld_child_pl) {
			u32 hls_len = (u32) strlen(rpid->hld_child_pl);
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Sending HLS sub playlist %s\n", rpid->route->log_name, rpid->hld_child_pl_name));

			if (serv->use_flute) {
				routeout_send_file(ctx, serv, ctx->sock_dvb_mabr, ctx->dvb_mabr_tsi, rpid->hls_child_toi, rpid->hld_child_pl, hls_len, 0, 0, GF_TRUE);
			} else {
				//we use codepoint 1 (NRT - file mode) for subplaylists
				routeout_send_file(ctx, serv, rpid->rlct->sock, rpid->tsi, ROUTE_INIT_TOI-1, rpid->hld_child_pl, hls_len, 1, 0, GF_FALSE);
			}

			if (ctx->reporting_on) {
				ctx->total_size += hls_len;
				ctx->total_bytes = hls_len;
				ctx->nb_resources++;
			}
			rpid->update_hls_child_pl = GF_FALSE;
		}
		//update fdt
		if (rpid->push_frag_name) {
			routeout_send_fdt(ctx, serv, rpid);
			rpid->push_frag_name = GF_FALSE;
		}

		while ((rpid->pck_offset < rpid->pck_size) || rpid->force_send_empty) {
			u32 sent, codepoint;
			//we have timing info, let's regulate
			if (rpid->current_cts_us!=GF_FILTER_NO_TS) {
				u64 cur_time_us;
				ctx->clock = gf_sys_clock_high_res();
				//carousel, not yet ready
				if (ctx->clock < rpid->clock_at_frame_start)
					break;

				//send delay proportionnal to send progress - ultimately we should follow the frame timing, not the segment timing
				//but for the time being we only push complete segments (no LL)
				//it may happen that pck_size is 0 (last_chunk=0) when we force sending a TOL
				if (!ctx->noreg && rpid->pck_size) {
					cur_time_us = rpid->pck_offset;
					cur_time_us *= rpid->current_dur_us;
					cur_time_us /= rpid->pck_size;
					cur_time_us += rpid->current_cts_us;

					if (cur_time_us > ctx->clock ) {
						u64 next_sched = (cur_time_us - ctx->clock) / 2;
						if (next_sched < ctx->reschedule_us)
							ctx->reschedule_us = next_sched;
						break;
					}
				}
			}
			if (ctx->dvb_mabr && !rpid->pck_offset && rpid->raw_file) {
				routeout_send_fdt(ctx, serv, rpid);
			}
			//we use codepoint 8 (media segment, file mode) for media segments, otherwise as listed in S-TSID
			codepoint = rpid->raw_file ? rpid->fmtp : 8;
			//ll mode in flute, each packet is sent as an object so use packet offset instead of file offset
			if (ctx->dvb_mabr && ctx->llmode) {
				sent = routeout_lct_send(ctx, rpid->rlct->sock, rpid->tsi, rpid->current_toi, codepoint, (u8 *) rpid->pck_data, rpid->pck_size, rpid->pck_offset, serv, rpid->pck_size, rpid->pck_offset, 0, serv->use_flute);
			} else {
				sent = routeout_lct_send(ctx, rpid->rlct->sock, rpid->tsi, rpid->current_toi, codepoint, (u8 *) rpid->pck_data, rpid->pck_size, rpid->pck_offset, serv, rpid->full_frame_size, rpid->pck_offset + rpid->frag_offset, 0, serv->use_flute);
			}
			rpid->pck_offset += sent;
			if (ctx->reporting_on) {
				ctx->total_bytes += sent;
			}
			rpid->force_send_empty = GF_FALSE;
		}
		assert (rpid->pck_offset <= rpid->pck_size);

		if (rpid->pck_offset == rpid->pck_size) {
			//print fragment push info except if single fragment
			if (rpid->frag_idx || !rpid->full_frame_size) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] pushed fragment %s#%d (%d bytes) in "LLU" us - target push "LLU" us\n", rpid->route->log_name, rpid->seg_name, rpid->frag_idx+1, rpid->pck_size, ctx->clock - rpid->clock_at_pck, rpid->current_dur_us));
			}
#ifndef GPAC_DISABLE_LOG
			//print full object push info
			if (rpid->full_frame_size && (gf_log_get_tool_level(GF_LOG_ROUTE)>=GF_LOG_INFO)) {
				char szFInfo[1000], szSID[31];
				u64 seg_clock, target_push_dur;

				if (rpid->pck_dur_at_frame_start) {
					target_push_dur = gf_timestamp_rescale(rpid->pck_dur_at_frame_start, rpid->timescale, 1000000);
				} else {
					target_push_dur = rpid->current_dur_us + rpid->current_cts_us - rpid->cts_us_at_frame_start;
				}
				if (ctx->sock_atsc_lls) {
					snprintf(szSID, 20, "Service%d ", serv->service_id);
					szSID[30] = 0;
				}
				else
					szSID[0] = 0;

				if (rpid->frag_idx)
					snprintf(szFInfo, 100, "%s%s (%d frags %d bytes)", szSID, rpid->seg_name, rpid->frag_idx+1, rpid->full_frame_size);
				else
					snprintf(szFInfo, 100, "%s%s (%d bytes)", szSID, rpid->seg_name, rpid->full_frame_size);

				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Pushed %s in "LLU" us - target push "LLU" us\n", rpid->route->log_name, szFInfo, ctx->clock - rpid->clock_at_frame_start, target_push_dur));

				//real-time stream, check we are not out of sync
				if (!rpid->raw_file) {
					//clock time is the clock at first pck of first seg + cts_diff(cur_seg, first_seg)
					seg_clock = rpid->cts_us_at_frame_start + target_push_dur;

					//if segment clock time is greater than clock, we've been pushing too fast
					if (seg_clock > ctx->clock) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Segment %s pushed early by "LLU" us\n", rpid->route->log_name, rpid->seg_name, seg_clock - ctx->clock));
					}
					//otherwise we've been pushing too slowly
					else if (ctx->clock > 1000 + seg_clock) {
						GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Segment %s pushed too late by "LLU" us\n", rpid->route->log_name, rpid->seg_name, ctx->clock - seg_clock));
					}

					if (rpid->pck_dur_at_frame_start && ctx->llmode) {
						u64 seg_rate = gf_timestamp_rescale(rpid->full_frame_size * 8, rpid->pck_dur_at_frame_start, rpid->timescale);

						if (seg_rate > rpid->bitrate) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Segment %s rate "LLU" but stream rate "LLU", updating bitrate\n", rpid->route->log_name, rpid->seg_name, seg_rate, rpid->bitrate));
							rpid->bitrate = (u32) seg_rate;
						}
					}
				}
			}
#endif
			//raw file, keep on sending data if carousel period is set and no new packet
			if (rpid->raw_file && rpid->carousel_time_us) {
				GF_FilterPacket *pck_next = gf_filter_pid_get_packet(rpid->pid);
				if (!pck_next) {
					rpid->pck_offset = 0;
					rpid->current_cts_us += rpid->carousel_time_us;
					rpid->clock_at_pck = rpid->current_cts_us;
					rpid->clock_at_frame_start += rpid->carousel_time_us;
					rpid->cts_us_at_frame_start = rpid->current_cts_us;
					//exit sending for this pid
					continue;
				}
			}
			gf_filter_pck_unref(rpid->current_pck);
			rpid->current_pck = NULL;
			rpid->frag_offset = rpid->cumulated_frag_size;
			goto next_packet;
		}
	}
	serv->is_done = (nb_done==count) ? GF_TRUE : GF_FALSE;
	return GF_OK;
}

#define GF_TAI_UTC_OFFSET	37
static void routeout_send_lls(GF_ROUTEOutCtx *ctx)
{
	char *payload_text = NULL;
	u8 *payload = NULL, *pay_start;
	char tmp[2000];
	u32 i, count, len, comp_size;
	s32 timezone, h, m;
	u64 diff = ctx->clock - ctx->last_lls_clock;
	if (diff < ctx->carousel) {
		u64 next_sched = ctx->carousel - diff;
		if (next_sched < ctx->reschedule_us)
			ctx->reschedule_us = next_sched;
		return;
	}
	ctx->last_lls_clock = ctx->clock;

	if (!ctx->bytes_sent) ctx->clock_stats = ctx->clock;

	//ATSC3 we send 2 LLS tables, SysTime and SLT

	//ATSC3 SysTime
	if (!ctx->lls_time_table) {
		gf_dynstrcat(&payload_text, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<SystemTime currentUtcOffset=\"", NULL);
		sprintf(tmp, "%d", GF_TAI_UTC_OFFSET);
		gf_dynstrcat(&payload_text, tmp, NULL);
		gf_dynstrcat(&payload_text, "\" utcLocalOffset=\"", NULL);
		timezone = -gf_net_get_timezone() / 60;
		h = timezone / 60;
		m = timezone - h*60;
		if (m)
			sprintf(tmp, "%sPT%dH%dM", (h<0) ? "-" : "", ABS(h), m);
		else
			sprintf(tmp, "%sPT%dH", (h<0) ? "-" : "", ABS(h));
		gf_dynstrcat(&payload_text, tmp, NULL);
		gf_dynstrcat(&payload_text, "\" dsStatus=\"", NULL);
		gf_dynstrcat(&payload_text, gf_net_time_is_dst() ? "true" : "false", NULL);
		gf_dynstrcat(&payload_text, "\"/>\n", NULL);

		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Updating ATSC3 LLS.SysTime:\n%s\n", ctx->log_name, payload_text));
		len = (u32) strlen(payload_text);
		comp_size = 2*len;
		payload = gf_malloc(sizeof(char)*(comp_size+4));
		pay_start = payload + 4;
		gf_gz_compress_payload_ex((u8 **) &payload_text, len, &comp_size, 0, GF_FALSE, &pay_start, GF_TRUE);
		gf_free(payload_text);
		payload_text = NULL;

		comp_size += 4;
		//format header
		payload[0] = 3; //tableID
		payload[1] = 0; //groupid
		payload[2] = 0; //lls_group_count_minus_one
		payload[3] = 1; //lls_table_version

		ctx->lls_time_table = payload;
		ctx->lls_time_table_len = comp_size;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Sending ATSC3 LLS.SysTime\n", ctx->log_name));
	gf_sk_send(ctx->sock_atsc_lls, ctx->lls_time_table, ctx->lls_time_table_len);
	ctx->bytes_sent += ctx->lls_time_table_len;

	//ATSC3 SLT
	if (!ctx->lls_slt_table) {
		count = gf_list_count(ctx->services);
		snprintf(tmp, 1000, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<SLT xmlns=\"tag:atsc.org,2016:XMLSchemas/ATSC3/Delivery/SLT/1.0/\" bsid=\"%d\">\n", ctx->bsid);
		gf_dynstrcat(&payload_text, tmp, NULL);
		for (i=0; i<count; i++) {
			const GF_PropertyValue *p;
			const char *src_ip, *service_name, *hidden, *hideInESG, *configuration;
			char szIP[GF_MAX_IP_NAME_LEN];
			ROUTEPid *rpid;
			ROUTEService *serv = gf_list_get(ctx->services, i);

			u32 sid = serv->service_id;
			if (!sid) sid = 1;

			rpid = gf_list_get(serv->pids, 0);

			p = gf_filter_pid_get_property_str(rpid->pid, "ATSC3ShortServiceName");
			if (!p)
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_SERVICE_NAME);
			service_name = (p && p->value.string) ? p->value.string : "GPAC";
			len = (u32) strlen(service_name);
			if (len>7) len = 7;
			strncpy(szIP, service_name, len);
			szIP[len] = 0;

			// ATSC 3.0 major channel number starts at 2. This really should be set rather than using the default.
			u32 major = 2;
			p = gf_filter_pid_get_property_str(rpid->pid, "ATSC3MajorChannel");
			if (p && p->value.string) major = atoi(p->value.string);

			// ATSC 3.0 minor channel number starts at 1.
			u32 minor = 1;
			p = gf_filter_pid_get_property_str(rpid->pid, "ATSC3MinorChannel");
			if (p && p->value.string) minor = atoi(p->value.string);

			u32 service_cat = 1;
			p = gf_filter_pid_get_property_str(rpid->pid, "ATSC3ServiceCat");
			if (p && p->value.string) service_cat = atoi(p->value.string);

			p = gf_filter_pid_get_property_str(rpid->pid, "ATSC3hidden");
			hidden = (p && p->value.string) ? p->value.string : "false";

			p = gf_filter_pid_get_property_str(rpid->pid, "ATSC3hideInGuide");
			hideInESG = (p && p->value.string) ? p->value.string : "false";

			p = gf_filter_pid_get_property_str(rpid->pid, "ATSC3configuration");
			configuration = (p && p->value.string) ? p->value.string : "Broadcast";

			snprintf(tmp, 2000,
				" <Service serviceId=\"%d\" globalServiceID=\"urn:atsc:gpac:%d:%d\" sltSvcSeqNum=\"0\" protected=\"false\" majorChannelNo=\"%d\" minorChannelNo=\"%d\" serviceCategory=\"%d\" shortServiceName=\"%s\" hidden=\"%s\" hideInGuide=\"%s\" broadbandAccessRequired=\"false\" configuration=\"%s\"> \n", sid, ctx->bsid, sid, major, minor, service_cat, szIP, hidden, hideInESG, configuration);
			gf_dynstrcat(&payload_text, tmp, NULL);

			src_ip = ctx->ifce;
			if (!src_ip) {
				if (gf_sk_get_local_ip(serv->rlct_base->sock, szIP)!=GF_OK)
					strcpy(szIP, "127.0.0.1");
				src_ip = szIP;
			}
			int res = snprintf(tmp, 1000, "  <BroadcastSvcSignaling slsProtocol=\"1\" slsDestinationIpAddress=\"%s\" slsDestinationUdpPort=\"%d\" slsSourceIpAddress=\"%s\"/>\n"
				" </Service>\n", serv->rlct_base->ip, serv->rlct_base->port, src_ip);
			if (res<0) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] String truncated will trying to write: <BroadcastSvcSignaling slsProtocol=\"1\" slsDestinationIpAddress=\"%s\" slsDestinationUdpPort=\"%d\" slsSourceIpAddress=\"%s\"/>\n", serv->log_name, serv->rlct_base->ip, serv->rlct_base->port, src_ip));
			}
			gf_dynstrcat(&payload_text, tmp, NULL);
		}
		gf_dynstrcat(&payload_text, "</SLT>\n", NULL);

		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Updating ATSC3 LLS.SLT:\n%s\n", ctx->log_name, payload_text));

		len = (u32) strlen(payload_text);
		comp_size = 2*len;
		payload = gf_malloc(sizeof(char)*(comp_size+4));
		pay_start = payload + 4;
		gf_gz_compress_payload_ex((u8 **) &payload_text, len, &comp_size, 0, GF_FALSE, &pay_start, GF_TRUE);
		gf_free(payload_text);
		payload_text = NULL;

		comp_size += 4;
		//format header
		payload[0] = 1; //tableID
		payload[1] = 0; //groupid
		payload[2] = 0; //lls_group_count_minus_one
		payload[3] = 1; //lls_table_version

		ctx->lls_slt_table = payload;
		ctx->lls_slt_table_len = comp_size;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Sending ATSC3 LLS SysTime and SLT\n", ctx->log_name));
	gf_sk_send(ctx->sock_atsc_lls, ctx->lls_slt_table, ctx->lls_slt_table_len);
	ctx->bytes_sent += ctx->lls_slt_table_len;
}

static void routeout_update_mabr_manifest(GF_ROUTEOutCtx *ctx)
{
	char *payload_text = NULL;
	char tmp[2000];
	u32 i, count;
	if (ctx->dvb_mabr_config) return;

	const char *src_ip;
	char szIP[GF_MAX_IP_NAME_LEN];
	src_ip = ctx->ifce;
	if (!src_ip) {
		if (gf_sk_get_local_ip(ctx->sock_dvb_mabr, szIP) != GF_OK)
			strcpy(szIP, "127.0.0.1");
		src_ip = szIP;
	}

	count = gf_list_count(ctx->services);
	snprintf(tmp, 1000, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<MulticastGatewayConfiguration xmlns=\"urn:dvb:metadata:MulticastSessionConfiguration:2023\"");
	gf_dynstrcat(&payload_text, tmp, NULL);
	//todo, validity ? : snprintf(tmp, 1000, " validUntil=\"2030-01-01T00:00:00:000F1000\"";

	gf_dynstrcat(&payload_text, ">\n", NULL);

	for (i=0; i<count; i++) {
		ROUTEService *serv = gf_list_get(ctx->services, i);
		//not ready
		if (!serv->manifest_mime && serv->dash_mode) {
			gf_free(payload_text);
			return;
		}

		gf_dynstrcat(&payload_text, "<MulticastGatewayConfigurationTransportSession transportSecurity=\"", NULL);
		gf_dynstrcat(&payload_text, ctx->csum ? "integrity\">\n" : "none\">\n", NULL);
		gf_dynstrcat(&payload_text, "<TransportProtocol protocolIdentifier=\"urn:dvb:metadata:cs:MulticastTransportProtocolCS:2019:", NULL);
		gf_dynstrcat(&payload_text, serv->use_flute ? "FLUTE" : "ROUTE", NULL);
		gf_dynstrcat(&payload_text, "\" protocolVersion=\"1\"/>\n", NULL);
		gf_dynstrcat(&payload_text, "<EndpointAddress>\n<NetworkSourceAddress>", NULL);

		gf_dynstrcat(&payload_text, src_ip, NULL);
		gf_dynstrcat(&payload_text, "</NetworkSourceAddress>\n<NetworkDestinationGroupAddress>", NULL);
		//- for FLUTE we use the main port to deliver FDT, manifests and init - we could split by service as done for ROUTE
		//- for ROUTE we must send the STSID, on the session port
		gf_dynstrcat(&payload_text, serv->use_flute ? ctx->ip : serv->rlct_base->ip, NULL);
		gf_dynstrcat(&payload_text, "</NetworkDestinationGroupAddress>\n<TransportDestinationPort>", NULL);
		sprintf(tmp, "%u", serv->use_flute ? ctx->dvb_mabr_port : serv->rlct_base->port);
		gf_dynstrcat(&payload_text, tmp, NULL);
		gf_dynstrcat(&payload_text, "</TransportDestinationPort>\n<MediaTransportSessionIdentifier>", NULL);
		sprintf(tmp, "%u", ctx->dvb_mabr_tsi);
		gf_dynstrcat(&payload_text, tmp, NULL);
		gf_dynstrcat(&payload_text, "</MediaTransportSessionIdentifier>\n</EndpointAddress>\n", NULL);

		u32 carousel_size=0;

		if (serv->manifest) carousel_size += (u32) strlen(serv->manifest);
		u32 j, nb_pids=gf_list_count(serv->pids);
		for (j=0;j<nb_pids; j++) {
			ROUTEPid *rpid = gf_list_get(serv->pids, j);
			carousel_size += rpid->init_seg_size;
			if (rpid->hld_child_pl)
				carousel_size += (u32) strlen(rpid->hld_child_pl);
			if (rpid->raw_file)
				carousel_size += rpid->pck_size;
		}
		//carousel info
		gf_dynstrcat(&payload_text, "<ObjectCarousel aggregateTransportSize=\"", NULL);
		sprintf(tmp, "%u", carousel_size);
		gf_dynstrcat(&payload_text, tmp, NULL);
		gf_dynstrcat(&payload_text, "\">\n", NULL);

		if (serv->dash_mode) {
			//announce manifest and init segs
			gf_dynstrcat(&payload_text, "<PresentationManifests serviceIdRef=\"", NULL);
			gf_dynstrcat(&payload_text, serv->service_name, NULL);
			gf_dynstrcat(&payload_text, "\" targetAcquisitionLatency=\"", NULL);
			sprintf(tmp, "%u", (u32) ctx->carousel/100000);
			gf_dynstrcat(&payload_text, tmp, NULL);
			gf_dynstrcat(&payload_text, "\"/>\n", NULL);

			gf_dynstrcat(&payload_text, "<InitSegments serviceIdRef=\"", NULL);
			gf_dynstrcat(&payload_text, serv->service_name, NULL);
			gf_dynstrcat(&payload_text, " targetAcquisitionLatency=\"", NULL);
			gf_dynstrcat(&payload_text, tmp, NULL);
			gf_dynstrcat(&payload_text, "\"/>\n", NULL);
		}
		gf_dynstrcat(&payload_text, "</ObjectCarousel>\n</MulticastGatewayConfigurationTransportSession>\n", NULL);
	}


	for (i=0; i<count; i++) {
		const GF_PropertyValue *p;
		ROUTEPid *rpid;
		ROUTEService *serv = gf_list_get(ctx->services, i);
		if (!serv->manifest_mime && serv->dash_mode) continue;

		u32 sid = serv->service_id;
		if (!sid) sid = 1;

		rpid = gf_list_get(serv->pids, 0);
		gf_dynstrcat(&payload_text, "<MulticastSession serviceIdentifier=\"", NULL);
		gf_dynstrcat(&payload_text, serv->service_name, NULL);
		gf_dynstrcat(&payload_text, "\"", NULL);

		p = gf_filter_pid_get_property_str(rpid->pid, "DVBPlaybackOffset");
		sprintf(tmp, " contentPlaybackAvailabilityOffset=\"PT%dS\"", p ? p->value.uint : 4);
		gf_dynstrcat(&payload_text, tmp, NULL);
		gf_dynstrcat(&payload_text, ">\n", NULL);

		gf_dynstrcat(&payload_text, "<PresentationManifestLocator manifestId=\"", NULL);
		sprintf(tmp, "gpac_mani_serv_%u", serv->service_id);
		gf_dynstrcat(&payload_text, tmp, NULL);
		gf_dynstrcat(&payload_text, "\" contentType=\"", NULL);
		gf_dynstrcat(&payload_text, serv->manifest_mime, NULL);
		gf_dynstrcat(&payload_text, "\">", NULL);
		if (ctx->furl && serv->manifest_server) {
			gf_dynstrcat(&payload_text, serv->manifest_server, NULL);
			gf_dynstrcat(&payload_text, serv->manifest_url, "/");
			gf_dynstrcat(&payload_text, serv->manifest_name, NULL);
		} else {
			gf_dynstrcat(&payload_text, serv->manifest_name, NULL);
		}
		gf_dynstrcat(&payload_text, "</PresentationManifestLocator>\n", NULL);

		u32 j, nb_pids = gf_list_count(serv->pids);
		for (j=0; j<nb_pids; j++) {
			ROUTEPid *rpid = gf_list_get(serv->pids, j);
			if (rpid->manifest_type) continue;

			gf_dynstrcat(&payload_text, "<MulticastTransportSession sessionIdleTimeout=\"60000\" transportSecurity=\"", NULL);
			gf_dynstrcat(&payload_text, (ctx->csum==DVB_CSUM_ALL) ? "integrity\"" : "none\"", NULL);
			sprintf(tmp, " id=\"%u\"", rpid->tsi);
			gf_dynstrcat(&payload_text, tmp, NULL);
			//todo start, duration

			//if set to chunked for Low latency
			gf_dynstrcat(&payload_text, " transmissionMode=\"", NULL);
			gf_dynstrcat(&payload_text, ctx->llmode ? "chunked" : "resource", NULL);
			gf_dynstrcat(&payload_text, "\"", NULL);
			gf_dynstrcat(&payload_text, ">\n", NULL);
			if (serv->use_flute) {
				gf_dynstrcat(&payload_text, "<TransportProtocol protocolIdentifier=\"urn:dvb:metadata:cs:MulticastTransportProtocolCS:2019:FLUTE\" protocolVersion=\"1\"/>\n", NULL);
			} else {
				gf_dynstrcat(&payload_text, "<TransportProtocol protocolIdentifier=\"urn:dvb:metadata:cs:MulticastTransportProtocolCS:2019:ROUTE\" protocolVersion=\"1\"/>\n", NULL);
			}
			gf_dynstrcat(&payload_text, "<EndpointAddress>\n<NetworkSourceAddress>", NULL);

			gf_dynstrcat(&payload_text, src_ip, NULL);
			gf_dynstrcat(&payload_text, "</NetworkSourceAddress>\n<NetworkDestinationGroupAddress>", NULL);
			gf_dynstrcat(&payload_text, rpid->rlct->ip, NULL);
			gf_dynstrcat(&payload_text, "</NetworkDestinationGroupAddress>\n<TransportDestinationPort>", NULL);
			sprintf(tmp, "%u", rpid->rlct->port);
			gf_dynstrcat(&payload_text, tmp, NULL);
			gf_dynstrcat(&payload_text, "</TransportDestinationPort>\n<MediaTransportSessionIdentifier>", NULL);
			sprintf(tmp, "%u", rpid->tsi);
			gf_dynstrcat(&payload_text, tmp, NULL);
			gf_dynstrcat(&payload_text, "</MediaTransportSessionIdentifier>\n</EndpointAddress>\n", NULL);
			sprintf(tmp, "<BitRate average=\"%u\" maximum=\"%u\"/>\n", rpid->bitrate, rpid->bitrate);
			gf_dynstrcat(&payload_text, tmp, NULL);

			gf_dynstrcat(&payload_text, "<ServiceComponentIdentifier xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" manifestIdRef=\"", NULL);
			sprintf(tmp, "gpac_mani_serv_%u", serv->service_id);
			gf_dynstrcat(&payload_text, tmp, NULL);
			gf_dynstrcat(&payload_text, "\"", NULL);

			if (serv->manifest_type==2) {
				gf_dynstrcat(&payload_text, " xsi:type=\"HLSComponentIdentifierType\" mediaPlaylistLocator=\"", NULL);
				gf_dynstrcat(&payload_text, rpid->hld_child_pl_name, NULL);
				gf_dynstrcat(&payload_text, "\"", NULL);
			} else {
				const char *id;
				gf_dynstrcat(&payload_text, " xsi:type=\"DASHComponentIdentifierType\" periodIdentifier=\"", NULL);
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_PERIOD_ID);
				if (p && p->value.string) {
					id = p->value.string;
				} else {
					if (!rpid->init_cfg_done) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing Period ID on PID using \"1\"\n", serv->log_name));
					}
					id = "1";
				}
				gf_dynstrcat(&payload_text, id, NULL);
				gf_dynstrcat(&payload_text, "\" adaptationSetIdentifier=\"", NULL);

				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_AS_ID);
				if (p) {
					sprintf(tmp, "%u", p->value.uint);
					id = tmp;
				} else {
					if (!rpid->init_cfg_done) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing AS ID on PID, using \"1\"\n", serv->log_name));
					}
					id = "1";
				}
				gf_dynstrcat(&payload_text, id, NULL);
				gf_dynstrcat(&payload_text, "\" representationIdentifier=\"", NULL);
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_REP_ID);
				if (p && p->value.string) {
					id = p->value.string;
				} else {
					if (!rpid->init_cfg_done) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing representation ID on PID (broken input filter), using \"1\"\n", serv->log_name));
					}
					id = "1";
				}
				gf_dynstrcat(&payload_text, id, NULL);
				gf_dynstrcat(&payload_text, "\"", NULL);
			}
			gf_dynstrcat(&payload_text, "/>\n</MulticastTransportSession>\n", NULL);
			rpid->init_cfg_done = GF_TRUE;
		}
		gf_dynstrcat(&payload_text, "</MulticastSession>\n", NULL);
	}
	gf_dynstrcat(&payload_text, "</MulticastGatewayConfiguration>\n", NULL);

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Updated MulticastGatewayConfiguration to:\n%s\n", ctx->log_name, payload_text));

	ctx->dvb_mabr_config = payload_text;
	ctx->dvb_mabr_config_len = (u32) strlen(payload_text);
	//we need a new TOI since config changed
	ctx->dvb_mabr_config_toi = 0;

	//reset FDT
	if (ctx->dvb_mabr_fdt) {
		gf_free(ctx->dvb_mabr_fdt);
		ctx->dvb_mabr_fdt = NULL;
	}
	//force sending asap
	ctx->last_dvb_mabr_clock = 0;
}

static void routeout_send_mabr_manifest(GF_ROUTEOutCtx *ctx)
{
	//update multicast gateway config - this may trigger an FDT recompute
	if (!ctx->dvb_mabr_config) routeout_update_mabr_manifest(ctx);
	//update fdt if needed
	if (!ctx->dvb_mabr_fdt) routeout_update_dvb_mabr_fdt(ctx, NULL, GF_FALSE);
	//not ready
	if (!ctx->dvb_mabr_config || !ctx->dvb_mabr_fdt) return;
	if (ctx->dvb_mabr_config && ctx->last_dvb_mabr_clock) {
		u64 diff = ctx->clock - ctx->last_dvb_mabr_clock;
		if (diff < ctx->carousel) {
			u64 next_sched = ctx->carousel - diff;
			if (next_sched < ctx->reschedule_us)
				ctx->reschedule_us = next_sched;
			return;
		}
	}

	if (!ctx->bytes_sent) ctx->clock_stats = ctx->clock;
	ctx->last_dvb_mabr_clock = ctx->clock;

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Sending Bootstrap info\n", ctx->log_name));

	//send fdt
	routeout_send_file(ctx, NULL, ctx->sock_dvb_mabr, ctx->dvb_mabr_tsi, 0, ctx->dvb_mabr_fdt, ctx->dvb_mabr_fdt_len, 0, ctx->dvb_mabr_fdt_instance_id, GF_TRUE);
	//send mcast gateway config
	routeout_send_file(ctx, NULL, ctx->sock_dvb_mabr, ctx->dvb_mabr_tsi, ctx->dvb_mabr_config_toi, ctx->dvb_mabr_config, ctx->dvb_mabr_config_len, 0, 0, GF_TRUE);
}

static GF_Err routeout_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	u32 i, count;
	Bool all_serv_done = GF_TRUE;
	GF_ROUTEOutCtx *ctx = (GF_ROUTEOutCtx *) gf_filter_get_udta(filter);

	ctx->clock = gf_sys_clock_high_res();
	ctx->reschedule_us = MIN(ctx->carousel, 50000);
	ctx->reporting_on = gf_filter_reporting_enabled(filter);
	if (ctx->reporting_on) {
		ctx->total_size_unknown = GF_FALSE;
		ctx->total_size = 0;
		ctx->total_bytes = GF_FALSE;
		ctx->nb_resources = 0;
	}

	if (ctx->runfor) {
		if (ctx->clock - ctx->clock_init > ctx->runfor*1000) {
			if (!ctx->done) {
				ctx->done = GF_TRUE;
				count = gf_filter_get_ipid_count(filter);
				for (i=0; i<count; i++) {
					GF_FilterEvent evt;
					GF_FilterPid *pid = gf_filter_get_ipid(filter, i);
					GF_FEVT_INIT(evt, GF_FEVT_STOP, pid);
					gf_filter_pid_send_event(pid, &evt);
					gf_filter_pid_set_discard(pid, GF_TRUE);
				}
			}
			return GF_EOS;
		}
	}

	if (ctx->check_pending) {
		if (gf_filter_connections_pending(filter)) {
			if (!ctx->check_init_clock) ctx->check_init_clock = gf_sys_clock();
			else if (gf_sys_clock() - ctx->check_init_clock > 2000) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] PIDs still pending after 2s, starting broadcast but will need to update signaling\n", ctx->log_name));
				ctx->check_pending = GF_FALSE;
			}
			if (ctx->check_pending) return GF_OK;
		}
		ctx->check_pending = GF_FALSE;
		ctx->check_init_clock = 0;
	}

	if (ctx->sock_atsc_lls)
		routeout_send_lls(ctx);

	//check all our services are ready
	count = gf_list_count(ctx->services);
	for (i=0; i<count; i++) {
		ROUTEService *serv = gf_list_get(ctx->services, i);
		if (serv->is_done) continue;
		e = routeout_check_service_updates(ctx, serv);
		if (!serv->service_ready || (e==GF_NOT_READY)) return GF_OK;
	}
	if (ctx->sock_dvb_mabr) {
		routeout_send_mabr_manifest(ctx);
	}

	for (i=0; i<count; i++) {
		ROUTEService *serv = gf_list_get(ctx->services, i);
		if (!serv->is_done) {
			e |= routeout_process_service(ctx, serv);
			if (!serv->is_done)
				all_serv_done = GF_FALSE;
		}
	}

	if (all_serv_done) {
		return e ? e : GF_EOS;
	}

	if (ctx->clock - ctx->clock_stats >= 1000000) {
		u64 rate = ctx->bytes_sent * 8 * 1000 / (ctx->clock - ctx->clock_stats);
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Mux rate "LLU" kbps\n", ctx->log_name, rate));
		if (ctx->reporting_on) {
			u32 progress = 0;
			char szStatus[200];
			if (!ctx->total_size_unknown && ctx->total_bytes)
				progress = (u32) (10000*ctx->total_bytes / ctx->total_size);

			if (ctx->sock_atsc_lls) {
				snprintf(szStatus, 200, "Mux rate "LLU" kbps - %d services - %d active resources %.02f %% done", rate, count, ctx->nb_resources, ((Double)progress) / 100);
			} else {
				snprintf(szStatus, 200, "Mux rate "LLU" kbps - %d active resources %.02f %% done", rate, ctx->nb_resources, ((Double)progress) / 100);
			}
			gf_filter_update_status(filter, 0, szStatus);
		}
		ctx->bytes_sent = 0;
		ctx->clock_stats = ctx->clock;
	}
	ctx->reschedule_us++;
	gf_filter_ask_rt_reschedule(filter, (u32) ctx->reschedule_us);
	return e;
}

static GF_FilterProbeScore routeout_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "atsc://", 7)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "route://", 8)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "mabr://", 7)) return GF_FPROBE_SUPPORTED;
	return GF_FPROBE_NOT_SUPPORTED;
}
static Bool routeout_use_alias(GF_Filter *filter, const char *url, const char *mime)
{
	const char *sep;
	u32 len;
	GF_ROUTEOutCtx *ctx = (GF_ROUTEOutCtx *) gf_filter_get_udta(filter);

	//atsc, do not analyze IP and port
	if (ctx->sock_atsc_lls) {
		if (!strncmp(url, "atsc://", 7)) return GF_TRUE;
		return GF_FALSE;
	}

	//check we have same hostname. If so, accept this destination as a source for our filter
	//- if atsc://, single instance of the filter
	//- if route://IP:PORT, same instance if same IP:PORT
	sep = strstr(url, "://");
	if (!sep) return GF_FALSE;
	sep += 3;
	sep = strchr(sep, '/');
	if (!sep) {
		if (!strcmp(ctx->dst, url)) return GF_TRUE;
		return GF_FALSE;
	}
	len = (u32) (sep - url);
	if (!strncmp(ctx->dst, url, len)) return GF_TRUE;
	return GF_FALSE;
}


#define OFFS(_n)	#_n, offsetof(GF_ROUTEOutCtx, _n)

static const GF_FilterArgs ROUTEOutArgs[] =
{
	{ OFFS(dst), "destination URL", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(ext), "set extension for graph resolution, regardless of file extension", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mime), "set mime type for graph resolution", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ifce), "default interface to use for multicast. If NULL, the default system interface will be used", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(carousel), "carousel period in ms for repeating signaling and raw file data", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(first_port), "port number of first ROUTE session in ATSC mode", GF_PROP_UINT, "6000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ip), "multicast IP address for ROUTE session in ATSC mode", GF_PROP_STRING, "225.1.1.0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ttl), "time-to-live for multicast packets", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(bsid), "ID for ATSC broadcast stream", GF_PROP_UINT, "800", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mtu), "size of LCT MTU in bytes", GF_PROP_UINT, "1472", NULL, 0},
	{ OFFS(splitlct), "split mode for LCT channels\n"
		"- off: all streams are in the same LCT channel\n"
		"- type: each new stream type results in a new LCT channel\n"
		"- all: all streams are in dedicated LCT channel, the first stream being used for STSID signaling"
		, GF_PROP_UINT, "off", "off|type|all", 0},
	{ OFFS(korean), "use Korean version of ATSC 3.0 spec instead of US", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(llmode), "use low-latency mode", GF_PROP_BOOL, "false", NULL, GF_ARG_HINT_EXPERT},
	{ OFFS(brinc), "bitrate increase in percent when estimating timing in low latency mode", GF_PROP_UINT, "10", NULL, GF_ARG_HINT_EXPERT},
	{ OFFS(noreg), "disable rate regulation for media segments, pushing them as fast as received", GF_PROP_BOOL, "false", NULL, GF_ARG_HINT_EXPERT},

	{ OFFS(runfor), "run for the given time in ms", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(nozip), "do not zip signaling package (STSID+manifest)", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(furl), "inject full URLs of source service in the signaling instead of stripped server path", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(flute), "use flute for DVB-MABR object delivery", GF_PROP_BOOL, "true", NULL, 0},
	{ OFFS(csum), "send MD5 checksum for DVB flute\n"
		"- no: do not send checksum\n"
		"- meta: only send checksum for configuration files, manifests and init segments\n"
		"- all: send checksum for everything", GF_PROP_UINT, "meta", "no|meta|all", 0},
	{0}
};

static const GF_FilterCapability ROUTEOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_STRING(GF_CAPS_INPUT,GF_PROP_PID_FILE_EXT, "*"),
};


GF_FilterRegister ROUTEOutRegister = {
	.name = "routeout",
	GF_FS_SET_DESCRIPTION("ROUTE output")
	GF_FS_SET_HELP("The ROUTE output filter is used to distribute a live file-based session using ROUTE or DVB-MABR.\n"
		"The filter supports DASH and HLS inputs, ATSC3.0 signaling and generic ROUTE or DVB-MABR signaling.\n"
		"\n"
		"The filter is identified using the following URL schemes:\n"
		"- `atsc://`: session is a full ATSC 3.0 session\n"
		"- `route://IP:port`: session is a ROUTE session running on given multicast IP and port\n"
		"- `mabr://IP:port`: session is a DVB-MABR session using FLUTE running on given multicast IP and port\n"
		"\n"
		"The filter only accepts input PIDs of type `FILE`.\n"
		"- HAS Manifests files are detected by file extension and/or MIME types, and sent as part of the signaling bundle or as LCT object files for HLS child playlists.\n"
		"- HAS Media segments are detected using the `OrigStreamType` property, and send as LCT object files using the DASH template string.\n"
		"- A PID without `OrigStreamType` property set is delivered as a regular LCT object file (called `raw` hereafter).\n"
		"  \n"
		"For `raw` file PIDs, the filter will look for the following properties:\n"
		"- `ROUTEName`: set resource name. If not found, uses basename of URL\n"
		"- `ROUTECarousel`: set repeat period. If not found, uses [-carousel](). If 0, the file is only sent once\n"
		"- `ROUTEUpload`: set resource upload time. If not found, uses [-carousel](). If 0, the file will be sent as fast as possible.\n"
		"\n"
		"When DASHing for ROUTE, DVB-MABR or single service ATSC, a file extension, either in [-dst]() or in [-ext](), may be used to identify the HAS session type (DASH or HLS).\n"
		"EX \"route://IP:PORT/manifest.mpd\", \"route://IP:PORT/:ext=mpd\"\n"
		"\n"
		"When DASHing for multi-service ATSC, forcing an extension will force all service to use the same formats.\n"
		"EX \"atsc://:ext=mpd\", \"route://IP:PORT/manifest.mpd\"\n"
		"If multiple services with different formats are needed, you will need to explicit your filters:\n"
		"EX gpac -i DASH_URL:#ServiceID=1 dashin:forward=file:FID=1 -i HLS_URL:#ServiceID=2 dashin:forward=file:FID=2 -o atsc://:SID=1,2\n"
		"EX gpac -i MOVIE1:#ServiceID=1 dasher:FID=1:mname=manifest.mpd -i MOVIE2:#ServiceID=2 dasher:FID=2:mname=manifest.m3u8 -o atsc://:SID=1,2\n"
		"\n"
		"Warning: When forwarding an existing DASH/HLS session, do NOT set any extension or manifest name.\n"
		"\n"
		"By default, all streams in a service are assigned to a single multicast session, and differentiated by TSI (see [-splitlct]()).\n"
		"TSI are assigned as follows:\n"
		"- signaling TSI is always 0 for ROUTE, 1 for DVB+Flute\n"
		"- raw files are assigned TSI 1 and increasing number of TOI\n"
		"- otherwise, the first PID found is assigned TSI 10, the second TSI 20 etc ...\n"
		"\n"
		"Init segments and HLS child playlists are sent before each new segment, independently of [-carousel]().\n"
		"# ATSC 3.0 mode\n"
		"In this mode, the filter allows multiple service multiplexing, identified through the `ServiceID` property.\n"
		"By default, a single multicast IP is used for route sessions, each service will be assigned a different port.\n"
		"The filter will look for `ROUTEIP` and `ROUTEPort` properties on the incoming PID. If not found, the default [-ip]() and [-port]() will be used.\n"
		"\n"
		"ATSC 3.0 attributes set by using the following PID properties:\n"
		"- ATSC3ShortServiceName: set the short service name, maxiumu of 7 characters.  If not found, `ServiceName` is checked, otherwise default to `GPAC`.\n"
		"- ATSC3MajorChannel: set major channel number of service. Default to 2.  This really should be set and should not use the default.\n"
		"- ATSC3MinorChannel: set minor channel number of service. Default of 1.\n"
		"- ATSC3ServiceCat: set service category, default to 1 if not found. 1=Linear a/v service. 2=Linear audio only service. 3=App-based service. 4=ESg service. 5=EA service. 6=DRM service.\n"
		"- ATSC3hidden: set if service is hidden.  Boolean true or false. Default of false.\n"
		"- ATSC3hideInGuide: set if service is hidden in ESG.  Boolean true or false. Default of false.\n"
		"- ATSC3configuration: set service configuration.  Choices are Broadcast or Broadband.  Default of Broadcast\n"
		"\n"
		"# ROUTE mode\n"
		"In this mode, only a single service can be distributed by the ROUTE session.\n"
		"Note: [-ip]() is ignored, and [-first_port]() is used if no port is specified in [-dst]().\n"
		"The ROUTE session will include a multi-part MIME unsigned package containing manifest and S-TSID, sent on TSI=0.\n"
		"\n"
		"# DVB-MABR mode\n"
		"In this mode, the filter allows multiple service multiplexing, identified through the `ServiceID` and `ServiceName` properties.\n"
		"Note: [-ip]() and [-first_port]() are used to send the multicast gateway configuration, init segments and manifests. [-first_port]() is used only if no port is specified in [-dst]().\n"
		"\n"
		"The session will carry DVB-MABR gateway configuration, maifests and init segments on `TSI=1`\n"
		"The filter will look for `ROUTEIP` and `ROUTEPort` properties on the incoming PID to setup multicast of each service. If not found, the default [-ip]() and port will be used, with port incremented by one for each new multicast stream.\n"
		"\n"
		"The FLUTE session always uses a symbol length of [-mtu]() minus 44 bytes.\n"
		"\n"
		"# Low latency mode\n"
		"When using low-latency mode (-llmode)(), the input media segments are not re-assembled in a single packet but are instead sent as they are received.\n"
		"In order for the real-time scheduling of data chunks to work, each fragment of the segment should have a CTS and timestamp describing its timing.\n"
		"If this is not the case (typically when used with an existing DASH session in file mode), the scheduler will estimate CTS and duration based on the stream bitrate and segment duration. The indicated bitrate is increased by [-brinc]() percent for safety.\n"
		"If this fails, the filter will trigger warnings and send as fast as possible.\n"
		"Note: The LCT objects are sent with no length (TOL header) assigned until the final segment size is known, potentially leading to a final 0-size LCT fragment signaling only the final size.\n"
		"\n"
		"In this mode, init segments and manifests are sent at the frequency given by property `ROUTECarousel` of the source PID if set or by (-carousel)[] option.\n"
		"Indicating `ROUTECarousel=0` will disable mid-segment repeating of manifests and init segments.\n"
		"# Examples\n"
		"Since the ROUTE filter only consumes files, it is required to insert:\n"
		"- the dash demultiplexer in file forwarding mode when loading a DASH session\n"
		"- the dash multiplexer when creating a DASH session\n"
		"\n"
		"Multiplexing an existing DASH session in route:\n"
		"EX gpac -i source.mpd dashin:forward=file -o route://225.1.1.0:6000/\n"
		"Multiplexing an existing DASH session in atsc:\n"
		"EX gpac -i source.mpd dashin:forward=file -o atsc://\n"
		"Dashing and multiplexing in route:\n"
		"EX gpac -i source.mp4 dasher:profile=live -o route://225.1.1.0:6000/manifest.mpd\n"
		"Dashing and multiplexing in route Low Latency:\n"
		"EX gpac -i source.mp4 dasher -o route://225.1.1.0:6000/manifest.mpd:profile=live:cdur=0.2:llmode\n"
		"\n"
		"Sending a single file in ROUTE using half a second upload time, 2 seconds carousel:\n"
		"EX gpac -i URL:#ROUTEUpload=0.5:#ROUTECarousel=2 -o route://225.1.1.0:6000/\n"
		"\n"
		"Common mistakes:\n"
		"EX gpac -i source.mpd -o route://225.1.1.0:6000/\n"
		"This will only send the manifest file as a regular object and will not load the dash session.\n"
		"EX gpac -i source.mpd dashin:forward=file -o route://225.1.1.0:6000/manifest.mpd\n"
		"This will force the ROUTE multiplexer to only accept .mpd files, and will drop all segment files (same if [-ext]() is used).\n"
		"EX gpac -i source.mpd dasher -o route://225.1.1.0:6000/\n"
		"EX gpac -i source.mpd dasher -o route://225.1.1.0:6000/manifest.mpd\n"
		"These will demultiplex the input, re-dash it and send the output of the dasher to ROUTE\n"
	)
	.private_size = sizeof(GF_ROUTEOutCtx),
	.max_extra_pids = -1,
	.args = ROUTEOutArgs,
	SETCAPS(ROUTEOutCaps),
	.probe_url = routeout_probe_url,
	.initialize = routeout_initialize,
	.finalize = routeout_finalize,
	.configure_pid = routeout_configure_pid,
	.process = routeout_process,
	.use_alias = routeout_use_alias,
	.flags = GF_FS_REG_TEMP_INIT
};

const GF_FilterRegister *routeout_register(GF_FilterSession *session)
{
	if (gf_opts_get_bool("temp", "get_proto_schemes")) {
		gf_opts_set_key("temp_out_proto", ROUTEOutRegister.name, "atsc,route,mabr");
	}
	return &ROUTEOutRegister;
}
#else
const GF_FilterRegister *routeout_register(GF_FilterSession *session)
{
	return NULL;
}
#endif
