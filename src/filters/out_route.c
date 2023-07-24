/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2020-2022
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
	LCT_SPLIT_NONE=0,
	LCT_SPLIT_TYPE,
	LCT_SPLIT_ALL,
};

typedef struct
{
	char *dst, *ext, *mime, *ifce, *ip;
	u32 carousel, first_port, bsid, mtu, splitlct, ttl, brinc, runfor;
	Bool korean, llmode, noreg;

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

	Bool done;
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
	char *manifest, *manifest_name, *manifest_mime, *manifest_server, *manifest_url;
	u32 manifest_version, manifest_crc;
	Bool stsid_changed;
	u32 stsid_version;
	u32 first_port;

	u8 *stsid_bundle;
	u32 stsid_bundle_size;
	u32 stsid_bundle_toi;

	u64 last_stsid_clock;
	u32 manifest_type;
	u32 creation_time;
} ROUTEService;

typedef struct
{
	GF_FilterPid *pid;

	ROUTEService *route;
	ROUTELCT *rlct;

	u32 tsi, bandwidth, stream_type;
	GF_Fraction dash_dur;
	//we cannot hold a ref to the init segment packet, as this may lock the input waiting for its realease to dispatch next packets
	u8 *init_seg_data;
	u32 init_seg_size;
	u32 init_seg_crc;
	Bool no_init;
	char *init_seg_name;

	//0: not manifest, 1: MPD, 2: HLS
	u32 manifest_type;

	Bool init_seg_sent;

	char *template;

	char *hld_child_pl, *hld_child_pl_name;
	u32 hld_child_pl_version, hld_child_pl_crc;
	u64 hls_ref_id;
	Bool update_hls_child_pl;

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
	Bool push_init, force_tol_send;
	u64 clock_at_frame_start, cts_us_at_frame_start, cts_at_frame_start;
	u32 pck_dur_at_frame_start;

	u32 bitrate;
} ROUTEPid;


ROUTELCT *route_create_lct_channel(GF_ROUTEOutCtx *ctx, const char *ip, u32 port, GF_Err *e)
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
		rlct->sock = gf_sk_new(GF_SOCK_TYPE_UDP);
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

ROUTEService *routeout_create_service(GF_ROUTEOutCtx *ctx, u32 service_id, const char *ip, u32 port, GF_Err *e)
{
	ROUTEService *rserv;
	ROUTELCT *rlct = NULL;
	*e = GF_OUT_OF_MEM;
	GF_SAFEALLOC(rserv, ROUTEService);
	if (!rserv) return NULL;

	rlct = route_create_lct_channel(ctx, ip, port, e);
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
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_IS_MANIFEST);
        if (p)
                manifest_type = p->value.uint;

	if (manifest_type) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_STREAM_TYPE);
		if (!p || (p->value.uint!=GF_STREAM_FILE)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Manifest file detected but no dashin filter, file will be uploaded as is !\n"));
			manifest_type = 0;
		}
	}

	if (!rserv) {
		GF_Err e;
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

		rserv = routeout_create_service(ctx, service_id, service_ip, port, &e);
		if (!rserv) return e;
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
	if (manifest_type) {
		rserv->creation_time = gf_sys_clock();
		gf_filter_pid_ignore_blocking(pid, GF_TRUE);
	} else {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_NO_INIT);
		if (p && p->value.boolean) {
			rpid->no_init = GF_TRUE;
		}
	}

	gf_list_add(rserv->pids, rpid);
	gf_filter_pid_set_udta(pid, rpid);

	rpid->rlct = rserv->rlct_base;

	p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_BITRATE);
	if (p) rpid->bitrate = p->value.uint * (100+ctx->brinc) / 100;

	rpid->stream_type = 0;
	p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_ORIG_STREAM_TYPE);
	if (!p) {
		if (!rpid->manifest_type) {
			rpid->raw_file = GF_TRUE;
		}
	} else {
		rpid->stream_type = p->value.uint;
	}

	rpid->bandwidth = 0;
	p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_BITRATE);
	if (p) rpid->bandwidth = p->value.uint;

	rpid->dash_dur.num = 1;
	rpid->dash_dur.den = 1;
	p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_DASH_DUR);
	if (p) rpid->dash_dur = p->value.frac;

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
		else if (ctx->splitlct && rpid->stream_type) {
			for (i=0; i<gf_list_count(rserv->pids); i++) {
				u32 astreamtype;
				ROUTEPid *apid = gf_list_get(rserv->pids, i);
				if (apid->manifest_type) continue;
				if (apid == rpid) continue;
				p = gf_filter_pid_get_property(rpid->pid, GF_PROP_PID_ORIG_STREAM_TYPE);
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
			rserv->first_port++;
			ctx->first_port++;
			rpid->rlct = route_create_lct_channel(ctx, NULL, rserv->first_port, &e);
			if (e) return e;
			if (rpid->rlct) {
				gf_list_add(rserv->rlcts, rpid->rlct);
			}
		}
	}


	gf_filter_pid_init_play_event(pid, &evt, 0, 1.0, "ROUTEOut");
	gf_filter_pid_send_event(pid, &evt);

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
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] IP %s is not a multicast address\n", ctx->ip));
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
			while (! rpid->no_init) {
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
						serv->stsid_changed = GF_TRUE;
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
			if (rpid->init_seg_data || rpid->no_init) {
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
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] No manifest name assigned, will use %s\n", file_name));
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
						media_pid->hld_child_pl_name = routeout_strip_base(rpid->route, (char *)file_name);
						serv->stsid_changed = GF_TRUE;
					}
					media_pid->update_hls_child_pl = GF_TRUE;
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
						if (strcmp(serv->manifest_name, file_name)) serv->stsid_changed = GF_TRUE;
						gf_free(serv->manifest_name);
					}
					serv->manifest_name = gf_strdup(file_name);
					manifest_updated = GF_TRUE;

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
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] No media PIDs found for HAS service after %d ms, aborting !\n", now));
			serv->is_done = GF_TRUE;
			return GF_SERVICE_ERROR;
		}
		return GF_OK;
	}

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

		for (i=0;i<count; i++) {
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

		const char *src_ip;
		char szIP[GF_MAX_IP_NAME_LEN];
		src_ip = ctx->ifce;
		if (!src_ip) {
			gf_sk_get_local_ip(rlct->sock, szIP);
			src_ip = szIP;
		}

		gf_dynstrcat(&payload_text, " <RS dIpAddr=\"", NULL);
		gf_dynstrcat(&payload_text, rlct->ip, NULL);
		snprintf(temp, 1000, "\" dPort=\"%d\" sIpAddr=\"", rlct->port);
		gf_dynstrcat(&payload_text, temp, NULL);
		gf_dynstrcat(&payload_text, src_ip, NULL);
		gf_dynstrcat(&payload_text, "\">\n", NULL);

		for (i=0; i<count; i++) {
			const GF_PropertyValue *p;
			ROUTEPid *rpid = gf_list_get(serv->pids, i);
			if (rpid->manifest_type) continue;
			if (rpid->rlct != rlct) continue;

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
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Missing representation ID on PID (broken input filter), using \"1\"\n"));
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
			snprintf(temp, 1000,
					"    <Payload codePoint=\"%d\" formatId=\"%d\" frag=\"0\" order=\"true\"/>\n"
					, rpid->fmtp, rpid->mode);

			gf_dynstrcat(&payload_text, temp, NULL);

			gf_dynstrcat(&payload_text,
					"   </SrcFlow>\n"
					"  </LS>\n", NULL);
		}
		gf_dynstrcat(&payload_text, " </RS>\n", NULL);
	}

	gf_dynstrcat(&payload_text, "</S-TSID>\n\r\n", NULL);

	gf_dynstrcat(&payload_text, "--"MULTIPART_BOUNDARY"--\n", NULL);


	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Updated Manifest+S-TSID bundle to:\n%s\n", payload_text));

	//compress and store as final payload
	if (serv->stsid_bundle) gf_free(serv->stsid_bundle);
	serv->stsid_bundle = (u8 *) payload_text;
	serv->stsid_bundle_size = 1 + (u32) strlen(payload_text);
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
		/*send lct with codepoint "NRT - Unsigned Package Mode" (multipart):
		"In broadcast delivery of SLS for a DASH-formatted streaming Service delivered using ROUTE, since SLS fragments are NRT files in nature,
		their carriage over the ROUTE session/LCT channel assigned by the SLT shall be in accordance to the Unsigned Packaged Mode or
		the Signed Package Mode as described in Section A.3.3.4 and A.3.3.5, respectively"
		*/
		offset += routeout_lct_send(ctx, serv->rlct_base->sock, 0, serv->stsid_bundle_toi, 3, serv->stsid_bundle, serv->stsid_bundle_size, offset, serv->service_id, serv->stsid_bundle_size, offset);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Sent service %d bundle (%d bytes)\n", serv->service_id, serv->stsid_bundle_size));
	return GF_OK;
}

static void routeout_fetch_packet(GF_ROUTEOutCtx *ctx, ROUTEPid *rpid)
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
			return;
		}
		return;
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
			rpid->force_tol_send = GF_TRUE;
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Missing timing for fragment %d of segment %s - timing estimated from bitrate: TS "LLU" ("LLU" in segment) dur "LLU"\n", rpid->frag_idx, rpid->seg_name, ts, frag_time, pck_dur));
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
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Missing duration on segment %s, something is wrong in demux chain, will not be able to regulate correctly\n", rpid->seg_name));
		}
		rpid->current_dur_us = gf_timestamp_rescale(rpid->current_dur_us, rpid->timescale, 1000000);
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
	GF_Err e;

	e = routeout_check_service_updates(ctx, serv);

	if (serv->stsid_bundle) {
		u64 diff = ctx->clock - serv->last_stsid_clock;
		if (diff >= ctx->carousel) {
			routeout_service_send_bundle(ctx, serv);
			serv->last_stsid_clock = ctx->clock;
		} else {
			u64 next_sched = ctx->carousel - diff;
			if (next_sched < ctx->reschedule_us)
				ctx->reschedule_us = next_sched;
		}
	} else {
		//not ready
		return e;
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
			u32 offset;
			routeout_fetch_packet(ctx, rpid);
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

			if (rpid->push_init) {
				rpid->push_init = GF_FALSE;
				send_hls_child = GF_TRUE;
				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Sending init segment %s\n", rpid->init_seg_name));

				//send init asap (may be empty)
				offset = 0;
				while (offset < rpid->init_seg_size) {
					//we use codepoint 5 (new IS) or 7 (repeated IS)
					u32 codepoint;
					if (rpid->init_seg_sent) {
						codepoint = 7;
					} else {
						rpid->init_seg_sent = GF_FALSE;
						codepoint = 5;
					}
					offset += routeout_lct_send(ctx, rpid->rlct->sock, rpid->tsi, ROUTE_INIT_TOI, codepoint, (u8 *) rpid->init_seg_data, rpid->init_seg_size, offset, serv->service_id, rpid->init_seg_size, offset);
				}
				if (ctx->reporting_on) {
					ctx->total_size += rpid->init_seg_size;
					ctx->total_bytes = rpid->init_seg_size;
					ctx->nb_resources++;
				}
			}
		}
		//send child m3u8 asap
		if (send_hls_child && rpid->hld_child_pl) {
			u32 hls_len = (u32) strlen(rpid->hld_child_pl);
			u32 offset = 0;
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Sending HLS sub playlist %s: \n%s\n", rpid->hld_child_pl_name, rpid->hld_child_pl));

			while (offset < hls_len) {
				//we use codepoint 1 (NRT - file mode) for subplaylists
				offset += routeout_lct_send(ctx, rpid->rlct->sock, rpid->tsi, ROUTE_INIT_TOI-1, 1, (u8 *) rpid->hld_child_pl, hls_len, offset, serv->service_id, hls_len, offset);
			}
			if (ctx->reporting_on) {
				ctx->total_size += hls_len;
				ctx->total_bytes = hls_len;
				ctx->nb_resources++;
			}
			rpid->update_hls_child_pl = GF_FALSE;
		}

		while ((rpid->pck_offset < rpid->pck_size) || rpid->force_tol_send) {
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
			//we use codepoint 8 (media segment, file mode) for media segments, otherwise as listed in S-TSID
			codepoint = rpid->raw_file ? rpid->fmtp : 8;
			sent = routeout_lct_send(ctx, rpid->rlct->sock, rpid->tsi, rpid->current_toi, codepoint, (u8 *) rpid->pck_data, rpid->pck_size, rpid->pck_offset, serv->service_id, rpid->full_frame_size, rpid->pck_offset + rpid->frag_offset);
			rpid->pck_offset += sent;
			if (ctx->reporting_on) {
				ctx->total_bytes += sent;
			}
			rpid->force_tol_send = GF_FALSE;
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

				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Pushed %s in "LLU" us - target push "LLU" us\n", szFInfo, ctx->clock - rpid->clock_at_frame_start, target_push_dur));

				//real-time stream, check we are not out of sync
				if (!rpid->raw_file) {
					//clock time is the clock at first pck of first seg + cts_diff(cur_seg, first_seg)
					seg_clock = rpid->cts_us_at_frame_start + target_push_dur;

					//if segment clock time is greater than clock, we've been pushing too fast
					if (seg_clock > ctx->clock) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Segment %s pushed early by "LLU" us\n", rpid->seg_name, seg_clock - ctx->clock));
					}
					//otherwise we've been pushing too slowly
					else if (ctx->clock > 1000 + seg_clock) {
						GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Segment %s pushed too late by "LLU" us\n", rpid->seg_name, ctx->clock - seg_clock));
					}

					if (rpid->pck_dur_at_frame_start && ctx->llmode) {
						u64 seg_rate = gf_timestamp_rescale(rpid->full_frame_size * 8, rpid->pck_dur_at_frame_start, rpid->timescale);

						if (seg_rate > rpid->bitrate) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Segment %s rate "LLU" but stream rate "LLU", updating bitrate\n", rpid->seg_name, seg_rate, rpid->bitrate));
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

		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Updating ATSC3 LLS.SysTime:\n%s\n", payload_text));
		len = (u32) strlen(payload_text);
		comp_size = 2*len;
		payload = gf_malloc(sizeof(char)*(comp_size+4));
		pay_start = payload + 4;
		gf_gz_compress_payload_ex((u8 **) &payload_text, len, &comp_size, 0, GF_FALSE, &pay_start, GF_FALSE);
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
				gf_sk_get_local_ip(serv->rlct_base->sock, szIP);
				src_ip = szIP;
			}
			int res = snprintf(tmp, 1000, "  <BroadcastSvcSignaling slsProtocol=\"1\" slsDestinationIpAddress=\"%s\" slsDestinationUdpPort=\"%d\" slsSourceIpAddress=\"%s\"/>\n"
				" </Service>\n", serv->rlct_base->ip, serv->rlct_base->port, src_ip);
			if (res<0) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] String truncated will trying to write: <BroadcastSvcSignaling slsProtocol=\"1\" slsDestinationIpAddress=\"%s\" slsDestinationUdpPort=\"%d\" slsSourceIpAddress=\"%s\"/>\n", serv->rlct_base->ip, serv->rlct_base->port, src_ip));
			}
			gf_dynstrcat(&payload_text, tmp, NULL);
		}
		gf_dynstrcat(&payload_text, "</SLT>\n", NULL);

		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Updating ATSC3 LLS.SLT:\n%s\n", payload_text));

		len = (u32) strlen(payload_text);
		comp_size = 2*len;
		payload = gf_malloc(sizeof(char)*(comp_size+4));
		pay_start = payload + 4;
		gf_gz_compress_payload_ex((u8 **) &payload_text, len, &comp_size, 0, GF_FALSE, &pay_start, GF_FALSE);
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

	if (ctx->sock_atsc_lls)
		routeout_send_lls(ctx);

	count = gf_list_count(ctx->services);
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
	gf_filter_ask_rt_reschedule(filter, (u32) ctx->reschedule_us);
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
		"When DASHing for ROUTE or single service ATSC, a file extension, either in [-dst]() or in [-ext](), may be used to identify the HAS session type (DASH or HLS).\n"
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
		"By default, all streams in a service are assigned to a single route session, and differentiated by ROUTE TSI (see [-splitlct]()).\n"
		"TSI are assigned as follows:\n"
		"- signaling TSI is always 0\n"
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
		"# Low latency mode\n"
		"When using low-latency mode, the input media segments are not re-assembled in a single packet but are instead sent as they are received.\n"
		"In order for the real-time scheduling of data chunks to work, each fragment of the segment should have a CTS and timestamp describing its timing.\n"
		"If this is not the case (typically when used with an existing DASH session in file mode), the scheduler will estimate CTS and duration based on the stream bitrate and segment duration. The indicated bitrate is increased by [-brinc]() percent for safety.\n"
		"If this fails, the filter will trigger warnings and send as fast as possible.\n"
		"Note: The LCT objects are sent with no length (TOL header) assigned until the final segment size is known, potentially leading to a final 0-size LCT fragment signaling only the final size.\n"
		"\n"
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
		gf_opts_set_key("temp_out_proto", ROUTEOutRegister.name, "atsc,route");
	}
	return &ROUTEOutRegister;
}
#else
const GF_FilterRegister *routeout_register(GF_FilterSession *session)
{
	return NULL;
}
#endif
