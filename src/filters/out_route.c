/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020
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


enum
{
	LCT_SPLIT_NONE=0,
	LCT_SPLIT_TYPE,
	LCT_SPLIT_ALL,
};

typedef struct
{
	char *dst, *ext, *mime, *ifce, *ip;
	u32 carousel, first_port, bsid, mtu, splitlct, llmode, ttl;
	Bool korean;

	GF_FilterCapability in_caps[2];
	char szExt[10];

	GF_List *services;

	//ATSC3
	GF_Socket *sock_atsc_lls;

	u64 clock_init, clock, clock_stats;
	u64 last_lls_clock;

	u8 *lls_time_table;
	u32 lls_time_table_len;

	u8 *lls_slt_table;
	u32 lls_slt_table_len;

	u64 bytes_sent;
	u8 *lct_buffer;

	u64 reschedule_us;
	u32 next_raw_file_toi;

	Bool reporting_on;
	u64 total_size, total_bytes;
	Bool total_size_unknown;
	u32 nb_resources;
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
	u32 dash_mode;

	GF_List *rlcts;
	ROUTELCT *rlct_base;

	Bool is_done;
	Bool wait_for_inputs;

	//storage for main manifest - all manifests (including HLS sub-playlists) are sent in the same PID
	//HLS sub-playlists are stored on their related PID to be pushed at each new seg
	char *manifest, *manifest_name, *manifest_mime;
	u32 manifest_version, manifest_crc;
	Bool stsid_changed;
	u32 stsid_version;
	u32 first_port;

	u8 *stsid_bundle;
	u32 stsid_bundle_size;
	u32 stsid_bundle_toi;

	u64 last_stsid_clock;
	u32 manifest_type;
} ROUTEService;

typedef struct
{
	GF_FilterPid *pid;

	ROUTEService *route;
	ROUTELCT *rlct;

	u32 tsi, bandwidth;

	//we cannot hold a ref to the init segment packet, as this may lock the input waiting for its realease to dispatch next packets
	u8 *init_seg_data;
	u32 init_seg_size;
	u32 init_seg_crc;
	char *init_seg_name;

	//0: not manifest, 1: MPD, 2: HLS
	u32 manifest_type;

	char *template;

	char *hld_child_pl, *hld_child_pl_name;
	u32 hld_child_pl_version, hld_child_pl_crc;
	u64 hls_ref_id;

	u32 fmtp, mode;

	GF_FilterPacket *current_pck;
	u32 current_toi;

	const u8 *pck_data;
	u32 pck_size, pck_offset;
	u64 res_size, offset_at_seg_start;
	char *seg_name;

	u32 timescale;
	u64 clock_at_first_pck;
	u64 cts_first_pck;
	u64 current_cts_us;
	u64 current_dur_us;
	u64 carousel_time_us;
	u64 clock_at_pck;
	Bool raw_file, use_basename;

	u32 full_frame_size, cumulated_frag_size, frag_offset;
	u32 frag_idx;
	Bool push_init;
	u64 clock_at_frame_start, cts_us_at_frame_start;
} ROUTEPid;


ROUTELCT *route_create_lct_channel(GF_ROUTEOutCtx *ctx, const char *ip, u32 port)
{
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
		rlct->sock = gf_sk_new(GF_SOCK_TYPE_UDP);
		if (rlct->sock) {
			GF_Err e = gf_sk_setup_multicast(rlct->sock, rlct->ip, rlct->port, ctx->ttl, GF_FALSE, ctx->ifce);
			if (e) {
				gf_sk_del(rlct->sock);
				rlct->sock = NULL;
				goto fail;
			}
		}
	}
	return rlct;
fail:
	if (rlct->ip) gf_free(rlct->ip);
	gf_free(rlct);
	return NULL;
}

ROUTEService *routeout_create_service(GF_ROUTEOutCtx *ctx, u32 service_id, const char *ip, u32 port)
{
	ROUTEService *rserv;
	ROUTELCT *rlct = NULL;
	GF_SAFEALLOC(rserv, ROUTEService);
	if (!rserv) return NULL;

	rlct = route_create_lct_channel(ctx, ip, port);
	if (!rlct) {
		gf_free(rserv);
		return NULL;
	}
	if (port) {
		rserv->first_port = port+1;
	}

	rserv->pids = gf_list_new();
	if (!rserv->pids) goto fail;
	rserv->rlcts = gf_list_new();
	if (!rserv->rlcts) goto fail;

	gf_list_add(rserv->rlcts, rlct);
	rserv->rlct_base = rlct;

	rserv->service_id = service_id;
	gf_list_add(ctx->services, rserv);

	if (ctx->lls_slt_table) {
		gf_free(ctx->lls_slt_table);
		ctx->lls_slt_table = NULL;
		ctx->last_lls_clock = 0;
	}
	return rserv;

fail:

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
		rpid->route->stsid_changed = GF_TRUE;
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
	if (serv->manifest) gf_free(serv->manifest);
	if (serv->stsid_bundle) gf_free(serv->stsid_bundle);
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
	u32 stream_type = 0;
	u32 pid_dash_mode = 0;
	GF_ROUTEOutCtx *ctx = (GF_ROUTEOutCtx *) gf_filter_get_udta(filter);
	ROUTEPid *rpid;

	rpid = gf_filter_pid_get_udta(pid);
	if (is_remove) {
		if (rpid) routeout_remove_pid(rpid, GF_FALSE);
		return GF_OK;
	}
	if (! gf_filter_pid_check_caps(pid))
		return GF_FILTER_NOT_SUPPORTED;

	//we currently ignore any reconfiguration of the pids
	if (rpid) {
		//any change to a raw file will require reconfiguring S-TSID
		if (rpid->raw_file) {
			rpid->route->stsid_changed = GF_TRUE;
		}
		else if (!rpid->manifest_type) {
			p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_TEMPLATE);
			if (p && p->value.string) {
				if (!rpid->template || strcmp(rpid->template, p->value.string)) {
					if (rpid->template) gf_free(rpid->template);
					rpid->template = gf_strdup(p->value.string);
					rpid->route->stsid_changed = GF_TRUE;
					rpid->use_basename = (strchr(rpid->template, '/')==NULL) ? GF_TRUE : GF_FALSE;
				}
			} else if (rpid->template) {
				gf_free(rpid->template);
				rpid->template = NULL;
				rpid->route->stsid_changed = GF_TRUE;
			}
		}

		return GF_OK;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DISABLE_PROGRESSIVE);
	if (p && p->value.uint) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Delivering files with progressive download disabled is not possible in ROUTE !\n"));
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
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_MIME);
	if (p && p->value.string) {
		if (strstr(p->value.string, "dash")) manifest_type = 1;
		else if (strstr(p->value.string, "mpd")) manifest_type = 1;
		else if (strstr(p->value.string, "mpegurl")) manifest_type = 2;
	}
	if (!manifest_type) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FILE_EXT);
		if (p && p->value.string) {
			if (strstr(p->value.string, "mpd")) manifest_type = 1;
			else if (strstr(p->value.string, "m3u8")) manifest_type = 2;
			else if (strstr(p->value.string, "3gm")) manifest_type = 1;
		}
	}
	if (manifest_type) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_STREAM_TYPE);
		if (!p || (p->value.uint!=GF_STREAM_FILE)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Manifest file detected but no dashin filter, file will be uploaded as is !\n"));
			manifest_type = 0;
		}
	}

	if (!rserv) {
		u32 port = ctx->first_port;
		const char *service_ip = ctx->ip;

		//cannot have 2 manifest pids connecting in route mode
		if (!ctx->sock_atsc_lls && gf_list_count(ctx->services) && manifest_type) {
			if (strchr(ctx->dst, '$')) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Multiple services in route mode, creating a new output filter\n"));
				return GF_REQUIRES_NEW_INSTANCE;
			}
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Multiple services in route mode and no URL templating, cannot create new output\n"));
			return GF_FILTER_NOT_SUPPORTED;
		}

		if (ctx->sock_atsc_lls) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ROUTE_IP);
			if (p && p->value.string) service_ip = p->value.string;

			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ROUTE_PORT);
			if (p && p->value.uint) port = p->value.uint;
			else ctx->first_port++;
		}

		rserv = routeout_create_service(ctx, service_id, service_ip, port);
		if (!rserv) return GF_OUT_OF_MEM;
		rserv->dash_mode = pid_dash_mode;
	}

	if (!rserv->manifest_type)
		rserv->manifest_type = manifest_type;

	if (rserv->dash_mode != pid_dash_mode){
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Mix of raw and muxed files should never happen - please report bug !\n"));
		return GF_SERVICE_ERROR;
	}

	GF_SAFEALLOC(rpid, ROUTEPid);
	if (!rpid) return GF_OUT_OF_MEM;
	rpid->route = rserv;
	rpid->pid = pid;
	rpid->fmtp = 128;
	rpid->mode = 1;
	rpid->tsi = 0;
	rpid->manifest_type = manifest_type;
	if (manifest_type)
		gf_filter_pid_ignore_blocking(pid, GF_TRUE);

	gf_list_add(rserv->pids, rpid);
	gf_filter_pid_set_udta(pid, rpid);

	rpid->rlct = rserv->rlct_base;
	stream_type = 0;
	p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_ORIG_STREAM_TYPE);
	if (!p) {
		if (!rpid->manifest_type) {
			rpid->raw_file = GF_TRUE;
		}
	} else {
		stream_type = p->value.uint;
	}


	if (!rpid->manifest_type && !rpid->raw_file) {
		p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_TEMPLATE);
		if (p && p->value.string) rpid->template = gf_strdup(p->value.string);

		else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Segment file PID detected but no template assigned, assuming raw file upload!\n"));
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
		else if (ctx->splitlct && stream_type) {
			for (i=0; i<gf_list_count(rserv->pids); i++) {
				u32 astreamtype;
				ROUTEPid *apid = gf_list_get(rserv->pids, i);
				if (apid->manifest_type) continue;
				if (apid == rpid) continue;
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_ORIG_STREAM_TYPE);
				if (!p) continue;
				astreamtype = p->value.uint;
				if (astreamtype==stream_type) {
					do_split = GF_TRUE;
					break;
				}
			}
		}
		if (do_split) {
			rserv->first_port++;
			ctx->first_port++;
			rpid->rlct = route_create_lct_channel(ctx, NULL, rserv->first_port);
			if (rpid->rlct) {
				gf_list_add(rserv->rlcts, rpid->rlct);
			}
		}
	}


	gf_filter_pid_init_play_event(pid, &evt, 0, 1.0, "ROUTEOut");
	gf_filter_pid_send_event(pid, &evt);

	rserv->wait_for_inputs = GF_TRUE;
	//no support for entity mode yet, we deliver full segments
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
	char *ext=NULL;
	GF_ROUTEOutCtx *ctx = (GF_ROUTEOutCtx *) gf_filter_get_udta(filter);

	if (!ctx || !ctx->dst) return GF_BAD_PARAM;

	if (!strnicmp(ctx->dst, "route://", 8)) {
		is_atsc = GF_FALSE;
	} else if (strnicmp(ctx->dst, "atsc://", 7))  {
		return GF_NOT_SUPPORTED;
	}

	if (ctx->ext) {
		ext = ctx->ext;
	} else {
		if (is_atsc) {
			base_name = gf_file_basename(ctx->dst);
		} else {
			char *sep = strchr(ctx->dst + 8, '/');
			base_name = sep ? gf_file_basename(ctx->dst) : NULL;
		}
		ext = base_name ? gf_file_ext_start(base_name) : NULL;
		if (ext) ext++;
	}

#if 0
	if (!ext) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Missing destination manifest type, cannot infer format!\n"));
		return GF_BAD_PARAM;
	}
#endif

	if (is_atsc) {
		if (!ctx->ip) return GF_BAD_PARAM;
	} else {
		char *sep, *root;
		sep = strrchr(ctx->dst+8, ':');
		if (sep) sep[0] = 0;
		root = sep ? strchr(sep+1, '/') : NULL;
		if (root) root[0] = 0;
		if (ctx->ip) gf_free(ctx->ip);
		ctx->ip = gf_strdup(ctx->dst+8);
		if (sep) {
			ctx->first_port = atoi(sep+1);
			sep[0] = ':';
		}
		if (root) root[0] = '/';
	}

	if (!gf_sk_is_multicast_address(ctx->ip)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] IP %s is not a multicast adress\n", ctx->ip));
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
	if (gf_filter_is_alias(filter)) {
		return GF_OK;
	}

	ctx->services = gf_list_new();

	if (!ctx->ifce) {
		ctx->ifce = gf_strdup("127.0.0.1");
	}
	if (is_atsc) {
		ctx->sock_atsc_lls = gf_sk_new(GF_SOCK_TYPE_UDP);
		gf_sk_setup_multicast(ctx->sock_atsc_lls, GF_ATSC_MCAST_ADDR, GF_ATSC_MCAST_PORT, 0, GF_FALSE, ctx->ifce);
	}

	ctx->lct_buffer = gf_malloc(sizeof(u8) * ctx->mtu);
	ctx->clock_init = gf_sys_clock_high_res();
	ctx->clock_stats = ctx->clock_init;

	if (!ctx->carousel) ctx->carousel = 1000;
	//move to microseconds
	ctx->carousel *= 1000;
	ctx->next_raw_file_toi = 1;
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

	if (ctx->lct_buffer) gf_free(ctx->lct_buffer);
	if (ctx->lls_slt_table) gf_free(ctx->lls_slt_table);
	if (ctx->lls_time_table) gf_free(ctx->lls_time_table);
}

#define MULTIPART_BOUNDARY	"_GPAC_BOUNDARY_ROUTE_.67706163_"
#define ROUTE_INIT_TOI	0xFFFFFFFF
static GF_Err routeout_check_service_updates(GF_ROUTEOutCtx *ctx, ROUTEService *serv)
{
	u32 i, j, count;
	char temp[1000];
	char *payload_text = NULL;
	Bool manifest_updated = GF_FALSE;
	u32 nb_media=0, nb_media_init=0, nb_raw_files=0;

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
			while (1) {
				GF_FilterPacket *pck = gf_filter_pid_get_packet(rpid->pid);
				if (!pck) break;

				const GF_PropertyValue *p = gf_filter_pck_get_property(pck, GF_PROP_PCK_INIT);
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
						serv->stsid_changed = GF_TRUE;
						rpid->current_toi = 0;

						p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
						if (!p) p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PCK_FILENAME);
						if (rpid->init_seg_name) gf_free(rpid->init_seg_name);
						rpid->init_seg_name = p ? gf_strdup(p->value.string) : NULL;
					}
					gf_filter_pid_drop_packet(rpid->pid);
					continue;
				}

				break;
			}
			if (rpid->init_seg_data) {
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
			const u8 *man_data;
			u32 man_size, man_crc;
			const char *file_name=NULL, *proto;
			GF_FilterPacket *pck = gf_filter_pid_get_packet(rpid->pid);
			if (!pck) break;

			p = gf_filter_pck_get_property(pck, GF_PROP_PCK_FILENAME);
			if (!p) p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_URL);

			if (p)
				file_name = p->value.string;
			else
				file_name = ctx->dst;

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
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Cannot guess manifest name, assuming %s\n", file_name));
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

					if (!media_pid->hld_child_pl_name || strcmp(media_pid->hld_child_pl_name, file_name)) {
						if (media_pid->hld_child_pl_name) gf_free(media_pid->init_seg_name);
						media_pid->hld_child_pl_name = gf_strdup(file_name);
						serv->stsid_changed = GF_TRUE;
					}
				}
			} else {
				man_data = gf_filter_pck_get_data(pck, &man_size);
				man_crc = gf_crc_32(man_data, man_size);
				if (man_crc != serv->manifest_crc) {
					serv->manifest_crc = man_crc;
					if (serv->manifest) gf_free(serv->manifest);
					serv->manifest = gf_malloc(man_size+1);
					memcpy(serv->manifest, man_data, man_size);
					serv->manifest[man_size] = 0;
					serv->manifest_version++;
					if (serv->manifest_name) {
						if (strcmp(serv->manifest_name, file_name)) serv->stsid_changed = GF_TRUE;
						gf_free(serv->manifest_name);
					}
					serv->manifest_name = gf_strdup(file_name);
					manifest_updated = GF_TRUE;

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
	if ((nb_media+nb_raw_files==0) || (nb_media_init<nb_media) || (serv->manifest && !nb_media))
		return GF_OK;

	//not ready, waiting for manifest
	if (!serv->manifest && !nb_raw_files) {
		return GF_OK;
	}
	//already setup and no changes
	else if (!serv->wait_for_inputs) {
		if (!manifest_updated && !serv->stsid_changed)
			return GF_OK;
	}
	if (serv->wait_for_inputs) {
		if (!serv->manifest) {
			assert(nb_raw_files);
			serv->stsid_changed = GF_TRUE;
		}
		serv->wait_for_inputs = GF_FALSE;
	}

	if (serv->stsid_changed) {
		serv->stsid_version++;

		for (i=0; i<count; i++) {
			ROUTEPid *rpid = gf_list_get(serv->pids, i);
			if (rpid->manifest_type) continue;
			rpid->clock_at_first_pck = 0;
		}
	}

	//ATSC3: mbms enveloppe, service description, astcROUTE bundle
	if (ctx->sock_atsc_lls) {
		u32 service_id = serv->service_id;
		if (!service_id) {
			service_id = 1;
		}
		gf_dynstrcat(&payload_text, "Content-Type: multipart/related; type=\"application/mbms-envelope+xml\"; boundary=\""MULTIPART_BOUNDARY"\"\r\n\r\n", NULL);

		gf_dynstrcat(&payload_text, "--"MULTIPART_BOUNDARY"\r\nContent-Type: application/mbms-envelope+xml\r\nContent-Location: envelope.xml\r\n\r\n", NULL);
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


		if (serv->manifest) {
			snprintf(temp, 1000, " <item metadataURI=\"%s\" version=\"%d\" contentType=\"%s\"/>\n", serv->manifest_name, serv->manifest_version, serv->manifest_mime);
			gf_dynstrcat(&payload_text, temp, NULL);
		}

		gf_dynstrcat(&payload_text,
			" <item metadataURI=\"stsid.xml\" version=\"", NULL);
		snprintf(temp, 100, "%d", serv->stsid_version);
		gf_dynstrcat(&payload_text, temp, NULL);

		gf_dynstrcat(&payload_text, "\" contentType=\"", NULL);
		if (ctx->korean)
			gf_dynstrcat(&payload_text, "application/s-tsid", NULL);
		else
			gf_dynstrcat(&payload_text, "route-s-tsid+xml", NULL);
		gf_dynstrcat(&payload_text, "\"/>\n", NULL);

		gf_dynstrcat(&payload_text, "</metadataEnvelope>\n\r\n", NULL);

		gf_dynstrcat(&payload_text, "--"MULTIPART_BOUNDARY"\r\nContent-Type: ", NULL);
		if (ctx->korean)
			gf_dynstrcat(&payload_text, "application/mbms-user-service-description+xml", NULL);
		else
			gf_dynstrcat(&payload_text, "application/route-usd+xml", NULL);
		gf_dynstrcat(&payload_text, "\r\nContent-Location: usbd.xml\r\n\r\n", NULL);

		snprintf(temp, 1000, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<bundleDescriptionROUTE xmlns=\"tag:atsc.org,2016:XMLSchemas/ATSC3/Delivery/ROUTEUSD/1.0/\">\n"
				" <userServiceDescription serviceId=\"%d\" globalServiceID=\"gpac://atsc30/us/%d/%d\" sTSIDUri=\"stsid.xml\">\n"
				"  <deliveryMethod>\n"
				"   <broadcastAppService>\n", service_id, ctx->bsid, service_id);
		gf_dynstrcat(&payload_text, temp, NULL);

		for (i=0;i<count; i++) {
			const GF_PropertyValue *p;
			ROUTEPid *rpid = gf_list_get(serv->pids, i);
			if (rpid->manifest_type) continue;
			//set template
			p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_TEMPLATE);
			if (p) {
				char *tpl = gf_strdup(p->value.string);
				char *sep = strchr(tpl, '$');
				if (sep) sep[0] = 0;
				if (!strstr(payload_text, tpl)) {
					gf_dynstrcat(&payload_text, "    <basePattern>", NULL);
					gf_dynstrcat(&payload_text, tpl, NULL);
					gf_dynstrcat(&payload_text, "</basePattern>\n", NULL);
				}
				gf_free(tpl);
			}
		}

		gf_dynstrcat(&payload_text, "   </broadcastAppService>\n"
				"  </deliveryMethod>\n"
				" </userServiceDescription>\n"
				"</bundleDescriptionROUTE>\n\r\n", NULL);
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

		snprintf(temp, 1000, " <RS dIpAddr=\"%s\" dport=\"%d\">\n", rlct->ip, rlct->port);
		gf_dynstrcat(&payload_text, temp, NULL);

		for (i=0; i<count; i++) {
			const GF_PropertyValue *p;
			ROUTEPid *rpid = gf_list_get(serv->pids, i);
			if (rpid->manifest_type) continue;
			if (rpid->rlct != rlct) continue;

			snprintf(temp, 100, "  <LS tsi=\"%d\" bw=\"%d\">\n", rpid->tsi, rpid->bandwidth);
			gf_dynstrcat(&payload_text, temp, NULL);

			if (serv->manifest) {
				gf_dynstrcat(&payload_text, "   <SrcFlow rt=\"true\">\n", NULL);
			} else {
				gf_dynstrcat(&payload_text, "   <SrcFlow rt=\"false\">\n", NULL);
			}
			gf_dynstrcat(&payload_text, "    <EFDT version=\"0\">\n", NULL);

			p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_TEMPLATE);
			if (p) {
				char *sep;
				strcpy(temp, p->value.string);
				sep = strstr(temp, "$Number");
				if (sep) {
					sep[0] = 0;
					strcat(temp, "$TOI");
					sep = strstr(p->value.string, "$Number");
					strcat(temp, sep + 7);
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
				gf_dynstrcat(&payload_text, "     <FDT-Instance afdt:efdtVersion=\"0\"", NULL);
				if (p) {
					gf_dynstrcat(&payload_text, " afdt:fileTemplate=\"", NULL);
					gf_dynstrcat(&payload_text, temp, NULL);
					gf_dynstrcat(&payload_text, "\"", NULL);
				}
				gf_dynstrcat(&payload_text, ">\n", NULL);
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

			//we always operate in file mode for now
			snprintf(temp, 1000,
					"    <Payload codePoint=\"%d\" formatID=\"%d\" frag=\"0\" order=\"1\" srcFecPayloadID=\"0\"/>\n"
					, rpid->fmtp, rpid->mode);

			gf_dynstrcat(&payload_text, temp, NULL);

			gf_dynstrcat(&payload_text,
					"   </SrcFlow>\n"
					"  </LS>\n", NULL);
		}
		gf_dynstrcat(&payload_text, " </RS>\n", NULL);
	}

	gf_dynstrcat(&payload_text, "</S-TSID>\n\r\n", NULL);

	gf_dynstrcat(&payload_text, "--"MULTIPART_BOUNDARY"--", NULL);


	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Updated Manifest+S-TSID bundle to:\n%s\n", payload_text));

	//compress and store as final payload
	if (serv->stsid_bundle) gf_free(serv->stsid_bundle);
	serv->stsid_bundle = (u8 *) payload_text;
	serv->stsid_bundle_size = 1+strlen(payload_text);
	gf_gz_compress_payload(&serv->stsid_bundle, serv->stsid_bundle_size, &serv->stsid_bundle_size);

	serv->stsid_bundle_toi = 0x80000000; //compressed
	if (manifest_updated) serv->stsid_bundle_toi |= (1<<18);
	if (serv->stsid_changed) {
		serv->stsid_bundle_toi |= (1<<17);
		serv->stsid_bundle_toi |= (serv->stsid_version & 0xFF);
	} else if (manifest_updated) {
		serv->stsid_bundle_toi |= (serv->manifest_version & 0xFF);
	}

	serv->stsid_changed = GF_FALSE;

	//reset last sent time
	serv->last_stsid_clock = 0;
	return GF_OK;
}


u32 routeout_lct_send(GF_ROUTEOutCtx *ctx, GF_Socket *sock, u32 tsi, u32 toi, u32 codepoint, u8 *payload, u32 len, u32 offset, u32 service_id, u32 total_size, u32 offset_in_frame)
{
	u32 max_size = ctx->mtu;
	u32 send_payl_size;
	u32 hdr_len = 4;
	u32 hpos;
	GF_Err e;

	if (total_size) {
		//TOL extension
		if (total_size<=0xFFFFFF) hdr_len += 1;
		else hdr_len += 2;
	}

	//start offset is not in header
	send_payl_size = 4 * (hdr_len+1) + len - offset;
	if (send_payl_size > max_size) {
		send_payl_size = max_size - 4 * (hdr_len+1);
	} else {
		send_payl_size = len - offset;
	}
	ctx->lct_buffer[0] = 0x12; //V=b0001, C=b00, PSI=b10
	ctx->lct_buffer[1] = 0xA0; //S=b1, 0=b01, h=b0, res=b00, A=b0, B=X
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

	//CCI=0
	PUT_U32(0);
	PUT_U32(tsi);
	PUT_U32(toi);

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

	assert(send_payl_size+hpos <= ctx->mtu);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] LCT SID %u TSI %u TOI %u size %u (frag %u total %u) offset %u (%u in obj)\n", service_id, tsi, toi, send_payl_size, len, total_size, offset, offset_in_frame));

	memcpy(ctx->lct_buffer + hpos, payload + offset, send_payl_size);
	e = gf_sk_send(sock, ctx->lct_buffer, send_payl_size + hpos);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to send LCT object TSI %u TOI %u fragment: %s\n", tsi, toi, gf_error_to_string(e) ));
	}
	//store what we actually sent including header for rate estimation
	ctx->bytes_sent += send_payl_size + hpos;
	//but return what we sent from the source
	return send_payl_size;
}

static GF_Err routeout_service_send_bundle(GF_ROUTEOutCtx *ctx, ROUTEService *serv)
{
	u32 offset = 0;

	//we don't regulate
	while (offset < serv->stsid_bundle_size) {
		//send lct with codepoint "unsigned package" (multipart)
		offset += routeout_lct_send(ctx, serv->rlct_base->sock, 0, serv->stsid_bundle_toi, 3, serv->stsid_bundle, serv->stsid_bundle_size, offset, serv->service_id, serv->stsid_bundle_size, offset);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Sent service %d bundle\n", serv->service_id));
	return GF_OK;
}

static void routeout_fetch_packet(GF_ROUTEOutCtx *ctx, ROUTEPid *rpid)
{
	const GF_PropertyValue *p;
	Bool start, end;
	u64 ts;

retry:

	rpid->current_pck = gf_filter_pid_get_packet(rpid->pid);
	if (!rpid->current_pck) {
		if (gf_filter_pid_is_eos(rpid->pid)) {
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
					evt.seg_size.is_init = GF_TRUE;
					rpid->route->dash_mode = 2;
					evt.seg_size.media_range_start = 0;
					evt.seg_size.media_range_end = 0;
					gf_filter_pid_send_event(rpid->pid, &evt);
				} else {
					evt.seg_size.is_init = GF_FALSE;
					evt.seg_size.media_range_start = rpid->offset_at_seg_start;
					evt.seg_size.media_range_end = rpid->res_size - 1;
					gf_filter_pid_send_event(rpid->pid, &evt);
				}
			}
			//done
			rpid->res_size = 0;
			return;
		}
		return;
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
		assert(start && end);
		crc = gf_crc_32(rpid->pck_data, rpid->pck_size);
		//whenever init seg changes, bump stsid version
		if (crc != rpid->init_seg_crc) {
			rpid->init_seg_crc = crc;
			rpid->route->stsid_changed = GF_TRUE;
			rpid->current_toi = 0;
			if (rpid->init_seg_data) gf_free(rpid->init_seg_data);
			rpid->init_seg_data = gf_malloc(rpid->pck_size);
			memcpy(rpid->init_seg_data, rpid->pck_data, rpid->pck_size);
			rpid->init_seg_size = rpid->pck_size;
			p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_FILENAME);
			if (rpid->init_seg_name) gf_free(rpid->init_seg_name);
			rpid->init_seg_name = p ? gf_strdup(p->value.string) : NULL;
		}
		gf_filter_pck_unref(rpid->current_pck);
		rpid->current_pck = NULL;
		goto retry;
	}

	p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PID_HLS_PLAYLIST);
	if (p && p->value.string) {
		u32 crc;
		assert(start && end);
		crc = gf_crc_32(rpid->pck_data, rpid->pck_size);
		//whenever init seg changes, bump stsid version
		if (crc != rpid->hld_child_pl_crc) {
			rpid->hld_child_pl_crc = crc;
			if (rpid->hld_child_pl) gf_free(rpid->hld_child_pl);
			rpid->hld_child_pl = gf_malloc(rpid->pck_size+1);
			memcpy(rpid->hld_child_pl, rpid->pck_data, rpid->pck_size);
			rpid->hld_child_pl[rpid->pck_size] = 0;

			if (!rpid->hld_child_pl_name || strcmp(rpid->hld_child_pl_name, p->value.string)) {
				rpid->route->stsid_changed = GF_TRUE;
				if (rpid->hld_child_pl_name) gf_free(rpid->hld_child_pl_name);
				rpid->hld_child_pl_name = gf_strdup(p->value.string);
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
				evt.seg_size.is_init = GF_TRUE;
				rpid->route->dash_mode = 2;
				evt.seg_size.media_range_start = 0;
				evt.seg_size.media_range_end = 0;
				gf_filter_pid_send_event(rpid->pid, &evt);
			} else {
				evt.seg_size.is_init = GF_FALSE;
				evt.seg_size.media_range_start = rpid->offset_at_seg_start;
				evt.seg_size.media_range_end = rpid->res_size - 1;
				rpid->offset_at_seg_start = evt.seg_size.media_range_end;
				gf_filter_pid_send_event(rpid->pid, &evt);
			}
			if ( gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_FILENAME))
				start = GF_TRUE;
		}
	}

	if (rpid->raw_file) {
		assert(start && end);

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
			rpid->carousel_time_us = p->value.frac.num;
			rpid->carousel_time_us *= 1000000;
			rpid->carousel_time_us /= p->value.frac.den;
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
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Requested upload time of file "LLU" is greater than its carousel time "LLU", adjusting carousel\n", rpid->current_dur_us, rpid->carousel_time_us));
			rpid->carousel_time_us = rpid->current_dur_us;
		}
		rpid->clock_at_pck = rpid->current_cts_us = rpid->cts_us_at_frame_start = ctx->clock;
		rpid->full_frame_size = rpid->pck_size;
		return;
	}

	if (start) {
		p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_FILENAME);
		if (rpid->seg_name) gf_free(rpid->seg_name);
		if (p) {
			rpid->seg_name = rpid->use_basename ? gf_file_basename(p->value.string) : p->value.string;
		} else {
			rpid->seg_name = "unknown";
		}
		rpid->seg_name = gf_strdup(rpid->seg_name);

		//file num increased per packet, open new file
		p = gf_filter_pck_get_property(rpid->current_pck, GF_PROP_PCK_FILENUM);
		if (p)
			rpid->current_toi = p->value.uint;
		else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Missing filenum on segment %s, something is wrong in demux chain - assuming +1 increase\n", rpid->seg_name));
			rpid->current_toi ++;
		}
		rpid->frag_idx = 0;
		rpid->full_frame_size = end ? rpid->pck_size : 0;
		rpid->cumulated_frag_size = rpid->pck_size;
		rpid->push_init = GF_TRUE;
		rpid->frag_offset = 0;
	} else {
		rpid->frag_idx++;
		rpid->cumulated_frag_size += rpid->pck_size;
		if (end) {
			rpid->full_frame_size = rpid->cumulated_frag_size;
		}
	}


	ts = gf_filter_pck_get_cts(rpid->current_pck);
	if (ts!=GF_FILTER_NO_TS) {
		u64 diff;
		if (!rpid->clock_at_first_pck) {
			rpid->clock_at_first_pck = ctx->clock;
			rpid->cts_first_pck = ts;
		}
		//move to microsecs
		diff = ts - rpid->cts_first_pck;
		diff *= 1000000;
		diff /= rpid->timescale;

		rpid->current_cts_us = rpid->clock_at_first_pck + diff;
		rpid->clock_at_pck = ctx->clock;

		if (start) {
			rpid->clock_at_frame_start = ctx->clock;
			rpid->cts_us_at_frame_start = rpid->current_cts_us;
		}

		rpid->current_dur_us = gf_filter_pck_get_duration(rpid->current_pck);
		if (!rpid->current_dur_us) {
			rpid->current_dur_us = rpid->timescale;
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Missing duration on segment %s, something is wrong in demux chain, will not be able to regulate correctly\n", rpid->seg_name));
		}
		rpid->current_dur_us *= 1000000;
		rpid->current_dur_us /= rpid->timescale;
	} else if (start && end) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Missing timing on segment %s, using previous fragment timing CTS "LLU" duration "LLU" us\nSomething could be wrong in demux chain, will not be able to regulate correctly\n", rpid->seg_name, rpid->current_cts_us-rpid->clock_at_first_pck, rpid->current_dur_us));
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Missing timing on fragment %d of segment %s, using previous fragment timing CTS "LLU" duration "LLU" us\nSomething could be wrong in demux chain, will not be able to regulate correctly\n", rpid->frag_idx, rpid->seg_name, rpid->current_cts_us-rpid->clock_at_first_pck, rpid->current_dur_us));
	}

	rpid->res_size += rpid->pck_size;
}


static GF_Err routeout_process_service(GF_ROUTEOutCtx *ctx, ROUTEService *serv)
{
	u32 i, count, nb_done;
#if 0
	Bool stsid_sent = GF_FALSE;
#endif
	routeout_check_service_updates(ctx, serv);

	if (serv->stsid_bundle) {
		u64 diff = ctx->clock - serv->last_stsid_clock;
		if (diff >= ctx->carousel) {
			routeout_service_send_bundle(ctx, serv);
			serv->last_stsid_clock = ctx->clock;
#if 0
			stsid_sent = GF_TRUE;
#endif
		} else {
			u64 next_sched = ctx->carousel - diff;
			if (next_sched < ctx->reschedule_us)
				ctx->reschedule_us = next_sched;
		}
	} else {
		//not ready
		return GF_OK;
	}

	nb_done = 0;
	count = gf_list_count(serv->pids);
	for (i=0; i<count; i++) {
		ROUTEPid *rpid = gf_list_get(serv->pids, i);
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
			u32 offset;
			routeout_fetch_packet(ctx, rpid);
			if (!rpid->current_pck) {
				if (gf_filter_pid_is_eos(rpid->pid))
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

			if (rpid->push_init) {
				rpid->push_init = GF_FALSE;

#if 0
				//force sending stsid at each new seg
				if (!rpid->raw_file && !stsid_sent) {
					routeout_service_send_bundle(ctx, serv);
					serv->last_stsid_clock = ctx->clock;
					stsid_sent = GF_TRUE;
				}
#endif
				//send init asap
				offset = 0;
				while (offset < rpid->init_seg_size) {
					offset += routeout_lct_send(ctx, rpid->rlct->sock, rpid->tsi, ROUTE_INIT_TOI, rpid->fmtp, (u8 *) rpid->init_seg_data, rpid->init_seg_size, offset, serv->service_id, rpid->init_seg_size, offset);
				}
				if (ctx->reporting_on) {
					ctx->total_size += rpid->init_seg_size;
					ctx->total_bytes = rpid->init_seg_size;
					ctx->nb_resources++;
				}

				//send child m3u8 asap
				if (rpid->hld_child_pl) {
					u32 hls_len = strlen(rpid->hld_child_pl);
					offset = 0;
					GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] SendingHLS sub playlist %s: \n%s\n", rpid->hld_child_pl_name, rpid->hld_child_pl));

					while (offset < hls_len) {
						offset += routeout_lct_send(ctx, rpid->rlct->sock, rpid->tsi, ROUTE_INIT_TOI-1, rpid->fmtp, (u8 *) rpid->hld_child_pl, hls_len, offset, serv->service_id, hls_len, offset);
					}
					if (ctx->reporting_on) {
						ctx->total_size += hls_len;
						ctx->total_bytes = hls_len;
						ctx->nb_resources++;
					}
				}
			}
		}

		while (rpid->pck_offset < rpid->pck_size) {
			u32 sent;
			//we have timing info, let's regulate
			if (rpid->current_cts_us!=GF_FILTER_NO_TS) {
				u64 cur_time_us;
				ctx->clock = gf_sys_clock_high_res();
				//carousel, not yet ready
				if (ctx->clock < rpid->clock_at_frame_start)
					break;

				//send delay proportionnal to send progress - ultimately we should follow the frame timing, not the segment timing
				//but for the time being we only push complete segments (no LL)
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
			sent = routeout_lct_send(ctx, rpid->rlct->sock, rpid->tsi, rpid->current_toi, rpid->fmtp, (u8 *) rpid->pck_data, rpid->pck_size, rpid->pck_offset, serv->service_id, rpid->full_frame_size, rpid->pck_offset + rpid->frag_offset);
			rpid->pck_offset += sent;
			if (ctx->reporting_on) {
				ctx->total_bytes += sent;
			}
		}
		assert (rpid->pck_offset <= rpid->pck_size);

		if (rpid->pck_offset == rpid->pck_size) {
			//print fragment push info except if single fragment
			if (rpid->frag_idx || !rpid->full_frame_size) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] pushed fragment %s#%d (%d bytes) in "LLU" us - target push "LLU" us\n", rpid->seg_name, rpid->frag_idx+1, rpid->pck_size, ctx->clock - rpid->clock_at_pck, rpid->current_dur_us));
			}
#ifndef GPAC_DISABLE_LOG
			//print full object push info
			if (rpid->full_frame_size && (gf_log_get_tool_level(GF_LOG_ROUTE)>=GF_LOG_INFO)) {
				char szFInfo[1000], szSID[31];
				u64 seg_clock;
				u64 target_push_dur = rpid->current_dur_us + rpid->current_cts_us - rpid->cts_us_at_frame_start;

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

				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Pushed %s in "LLU" us - target push "LLU" us\n", szFInfo, ctx->clock - rpid->clock_at_frame_start, target_push_dur));

				//real-time stream, check we are not out of sync
				if (!rpid->raw_file) {
					seg_clock = rpid->cts_us_at_frame_start + target_push_dur;

					if (seg_clock > ctx->clock) {
						GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Segment %s pushed too late by "LLU" us\n", rpid->seg_name, seg_clock - ctx->clock));
					} else if (ctx->clock > 1000 + seg_clock) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Segment %s pushed early by "LLU" us\n", rpid->seg_name, ctx->clock - seg_clock));
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
	char tmp[1000];
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

	//we send 2 LLS tables, SysTime and SLT

	//SysTime
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

		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Updating ATSC3 LLS.SysTime:\n%s\n", payload_text));
		len = strlen(payload_text);
		comp_size = 2*len;
		payload = gf_malloc(sizeof(char)*(comp_size+4));
		pay_start = payload + 4;
		gf_gz_compress_payload_ex((u8 **) &payload_text, len, &comp_size, 0, GF_FALSE, &pay_start);
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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Sending ATSC3 LLS.SysTime\n"));
	gf_sk_send(ctx->sock_atsc_lls, ctx->lls_time_table, ctx->lls_time_table_len);
	ctx->bytes_sent += ctx->lls_time_table_len;
	
	//SLT
	if (!ctx->lls_slt_table) {
		count = gf_list_count(ctx->services);
		snprintf(tmp, 1000, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<SLT bsid=\"%d\">\n", ctx->bsid);
		gf_dynstrcat(&payload_text, tmp, NULL);
		for (i=0; i<count; i++) {
			const GF_PropertyValue *p;
			const char *src_ip;
			char szIP[GF_MAX_IP_NAME_LEN];
			ROUTEPid *rpid;
			ROUTEService *serv = gf_list_get(ctx->services, i);
			u32 sid = serv->service_id;
			if (!sid) sid = 1;
			snprintf(tmp, 1000,
				" <Service serviceId=\"%d\" sltSvcSeqNum=\"1\" serviceCategory=\"1\" globalServiceId=\"gpac://atsc30/us/%d/%d\" majorChannelNo=\"666\" minorChannelNo=\"666\" shortServiceName=\"", sid, ctx->bsid, sid);
			gf_dynstrcat(&payload_text, tmp, NULL);
			rpid = gf_list_get(serv->pids, 0);
			p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_SERVICE_NAME);
			gf_dynstrcat(&payload_text, p ? p->value.string : "GPAC", NULL);

			src_ip = ctx->ifce;
			if (!src_ip) {
				gf_sk_get_local_ip(serv->rlct_base->sock, szIP);
				src_ip = szIP;
			}
			snprintf(tmp, 1000, "\">\n"
				"  <BroadcastSvcSignaling slsProtocol=\"1\" slsDestinationIpAddress=\"%s\" slsDestinationUdpPort=\"%d\" slsSourceIpAddress=\"%s\"/>\n"
				" </Service>\n", serv->rlct_base->ip, serv->rlct_base->port, src_ip);
			gf_dynstrcat(&payload_text, tmp, NULL);
		}
		gf_dynstrcat(&payload_text, "</SLT>\n", NULL);

		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Updating ATSC3 LLS.SLT:\n%s\n", payload_text));

		len = strlen(payload_text);
		comp_size = 2*len;
		payload = gf_malloc(sizeof(char)*(comp_size+4));
		pay_start = payload + 4;
		gf_gz_compress_payload_ex((u8 **) &payload_text, len, &comp_size, 0, GF_FALSE, &pay_start);
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

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Sending ATSC3 LLS SysTime and SLT\n"));
	gf_sk_send(ctx->sock_atsc_lls, ctx->lls_slt_table, ctx->lls_slt_table_len);
	ctx->bytes_sent += ctx->lls_slt_table_len;
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

	if (ctx->sock_atsc_lls)
		routeout_send_lls(ctx);

	count = gf_list_count(ctx->services);
	for (i=0; i<count; i++) {
		ROUTEService *serv = gf_list_get(ctx->services, i);
		e |= routeout_process_service(ctx, serv);
		if (!serv->is_done)
			all_serv_done = GF_FALSE;
	}

	if (all_serv_done) {
		return GF_EOS;
	}

	if (ctx->clock - ctx->clock_stats >= 1000000) {
		u64 rate = ctx->bytes_sent * 8 * 1000 / (ctx->clock - ctx->clock_stats);
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Mux rate "LLU" kbps\n", rate));
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
	gf_filter_ask_rt_reschedule(filter, ctx->reschedule_us);
	return e;
}

static GF_FilterProbeScore routeout_probe_url(const char *url, const char *mime)
{
	if (!strnicmp(url, "atsc://", 7)) return GF_FPROBE_SUPPORTED;
	if (!strnicmp(url, "route://", 8)) return GF_FPROBE_SUPPORTED;
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
	{ OFFS(dst), "location of destination file - see filter help ", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(ext), "set extension for graph resolution, regardless of file extension", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(mime), "set mime type for graph resolution", GF_PROP_NAME, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ifce), "default interface to use for multicast. If NULL, the default system interface will be used", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(carousel), "carousel period in ms for repeating signaling and raw file data - see filter help", GF_PROP_UINT, "1000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(first_port), "port number of first ROUTE session in ATSC mode", GF_PROP_UINT, "6000", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ip), "mulicast IP address for ROUTE session in ATSC mode", GF_PROP_STRING, "225.1.1.0", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(ttl), "time-to-live for multicast packets", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(bsid), "ID for ATSC broadcast stream", GF_PROP_UINT, "800", NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(mtu), "size of LCT MTU in bytes", GF_PROP_UINT, "1472", NULL, 0},
	{ OFFS(splitlct), "split mode for LCT channels\n"
		"- off: all streams are in the same LCT channel\n"
		"- type: each new stream type results in a new LCT channel\n"
		"- all: all streams are in dedicated LCT channel, the first stream being used for STSID signaling"
		, GF_PROP_UINT, "off", "off|type|all", 0},
	{ OFFS(korean), "use korean version of ATSC 3.0 spec instead of US", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(llmode), "use low-latency mode (experimental, will soon be removed)", GF_PROP_BOOL, "false", NULL, 0},
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
	GF_FS_SET_HELP("The ROUTE output filter is used to distribute a live file-based session using ROUTE.\n"
		"The filter supports DASH and HLS inputs, ATSC3.0 signaling and generic ROUTE signaling.\n"
		"\n"
		"The filter is identified using the following URL schemes:\n"
		"- `atsc://`: session is a full ATSC 3.0 session\n"
		"- `route://IP:port`: session is a ROUTE session running on given multicast IP and port\n"
		"\n"
		"The filter only accepts input PIDs of type `FILE`. A PID without `OrigStreamType` property set is delievered as a raw file.\n"
		"For such PIDs, the filter will look for the following properties:\n"
		"- `ROUTEName`: set resource name. If not found, uses basename of URL\n"
		"- `ROUTECarousel`: set repeat period. If not found, uses [-carousel](). If 0, the file is only sent once\n"
		"- `ROUTEUpload`: set resource upload time. If not found, uses [-carousel](). If 0, the file will be sent as fast as possible.\n"
		"\n"
		"For ROUTE or single service ATSC, a file extension, either in [-dst]() or in [-ext](), may be used to identify the HAS session type (DASH or HLS).\n"
		"EX \"route://IP:PORT/manifest.mpd\", \"route://IP:PORT/:ext=mpd\"\n"
		"\n"
		"For multi-service ATSC, forcing an extension will force all service to use the same formats\n"
		"EX \"atsc://ext=mpd\", \"route://IP:PORT/manifest.mpd\"\n"
		"If multiple services with different formats are needed, you will need to explicit your filters:\n"
		"EX gpac -i DASH_URL:#ServiceID=1 @ dashin:filemode:FID=1 -i HLS_URL:#ServiceID=2 @ dashin:filemode:FID=2 -o atsc://:SID=1,2\n"
		"EX gpac -i MOVIE1:#ServiceID=1 @ dasher:FID=1:mname=manifest.mpd -i MOVIE2:#ServiceID=2 @ dasher:FID=2:mname=manifest.m3u8 -o atsc://:SID=1,2\n"
		"\n"
		"By default, all streams in a service are assigned to a single route session, and differentiated by ROUTE TSI (see [-splitltc]()).\n"
		"TSI are assigned as follows:\n"
		"- signaling TSI is always 0\n"
		"- raw files are assigned TSI 1 and increasing number of TOI\n"
		"- otherwise, the first PID found is assigned TSI 10, the second TSI 20 etc ...\n"
		"\n"
		"Init segments and HLS subplaylists are sent before each new segment, independently of [-carousel]().\n"
		"# ATSC 3.0 mode\n"
		"In this mode, the filter allows multiple service multiplexing.\n"
		"By default, a single multicast IP is used for route sessions, each service will be assigned a different port.\n"
		"The filter will look for `ROUTEIP` and `ROUTEPort` properties on the incoming PID. If not found, the default [-ip]() and [-port]() will be used.\n"
		"\n"
		"# ROUTE mode\n"
		"In this mode, only a single service can be distributed by the ROUTE session.\n"
		"Note: [-ip]() is ignored, and [-first_port]() is used if no port is specified in [-dst]().\n"
		"The ROUTE session will include a multi-part MIME unsigned package containing manifest and S-TSID, sent on TSI=0.\n"
		"\n"
		"# Examples (to be refined!)\n"
		"Since the ROUTE filter only consummes files, it is needed to insert:\n"
		"- the dash demuxer in filemode when loading a DASH session\n"
		"- the dash muxer when creating a DASH session\n"
		"\n"
		"Muxing an existing DASH session in route:\n"
		"EX gpac -i source.mpd dashin:filemode @ -o route://225.1.1.0:6000/:ext=mpd\n"
		"Muxing an existing DASH session in atsc:\n"
		"EX gpac -i source.mpd dashin:filemode @ -o atsc://:ext=mpd\n"
		"Dashing and muxing in route:\n"
		"EX gpac -i source.mp4 dasher @ -o route://225.1.1.0:6000/manifest.mpd:profile=live\n"
		"Dashing and muxing in route Low Latency (experimental):\n"
		"EX gpac -i source.mp4 dasher @ -o route://225.1.1.0:6000/manifest.mpd:profile=live:cdur=0.2:llmode\n"
		"\n"
		"Sending a single file in ROUTE using half a second upload time, 2 seconds carousel:\n"
		"EX gpac -i URL:#ROUTEUpload=0.5:#ROUTECarousel=2 -o route://225.1.1.0:6000/\n"
		"\n"
		"Common mistakes\n"
		"EX gpac -i source.mpd -o route://225.1.1.0:6000/\n"
		"This will only send the manifest file as a regular object and will not load the dash session.\n"
		"EX gpac -i source.mpd dasher @ -o route://225.1.1.0:6000/\n"
		"This will load the dash session, and instantiate a muxer of the same format as the dash input to create a single file sent to ROUTE\n"
		"EX gpac -i source.mpd dasher @ -o route://225.1.1.0:6000/manifest.mpd\n"
		"This will load the dash session, and instantiate a new dasher filter (hence a new DASH manifest), sending the output of the DASHER to ROUTE\n"
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
	.use_alias = routeout_use_alias
};


const GF_FilterRegister *routeout_register(GF_FilterSession *session)
{
	return &ROUTEOutRegister;
}

