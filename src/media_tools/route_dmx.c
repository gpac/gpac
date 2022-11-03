/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools ROUTE (ATSC3, DVB-I) demux sub-project
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

#include <gpac/route.h>

#if !defined(GPAC_DISABLE_ROUTE)

#include <gpac/network.h>
#include <gpac/bitstream.h>
#include <gpac/xml.h>
#include <gpac/thread.h>

#define GF_ROUTE_SOCK_SIZE	0x80000

typedef struct
{
	u8 codepoint;
	u8 format_id;
	u8 frag;
	u8 order;
	u32 src_fec_payload_id;
} GF_ROUTELCTReg;

typedef struct
{
	char *filename;
	u32 toi;
	u32 crc;
} GF_ROUTELCTFile;

typedef struct
{
	u32 tsi;
	char *toi_template;
	GF_List *static_files;

	GF_ROUTELCTReg CPs[8];
	u32 nb_cps;
	u32 last_dispatched_tsi, last_dispatched_toi;
	Bool tsi_init;
} GF_ROUTELCTChannel;

typedef enum
{
	GF_LCT_OBJ_INIT=0,
	GF_LCT_OBJ_RECEPTION,
	GF_LCT_OBJ_DONE_ERR,
	GF_LCT_OBJ_DONE,
	GF_LCT_OBJ_DISPATCHED,
} GF_LCTObjectStatus;

typedef struct
{
	u32 toi, tsi;
	u32 total_length;
	//fragment reaggregation
	char *payload;
	u32 nb_bytes, nb_recv_bytes, alloc_size;
	u32 nb_frags, nb_alloc_frags, nb_recv_frags;
	GF_LCTFragInfo *frags;
	GF_LCTObjectStatus status;
	u32 download_time_ms;
	u32 last_gather_time;
	u8 closed_flag;
	u8 force_keep;

	GF_ROUTELCTChannel *rlct;
	GF_ROUTELCTFile *rlct_file;

	u32 prev_start_offset;

    char solved_path[GF_MAX_PATH];
    
    GF_Blob blob;
	void *udta;
} GF_LCTObject;



typedef struct
{
	GF_Socket *sock;

	GF_List *channels;
} GF_ROUTESession;

typedef enum
{
	GF_ROUTE_TUNE_OFF=0,
	GF_ROUTE_TUNE_ON,
	GF_ROUTE_TUNE_SLS_ONLY,
} GF_ROUTETuneMode;

typedef struct
{
	u32 service_id;
	u32 protocol;
	u32 mpd_version, stsid_version;
	GF_Socket *sock;
	u32 secondary_sockets;
	GF_List *objects;
	GF_LCTObject *last_active_obj;

	u32 port;
	char *dst_ip;
	u32 last_dispatched_toi_on_tsi_zero;
	u32 stsid_crc;

	GF_List *route_sessions;
	GF_ROUTETuneMode tune_mode;
	void *udta;
} GF_ROUTEService;

struct __gf_routedmx {
	const char *ip_ifce;
	GF_Socket *atsc_sock;
	u8 *buffer;
	u32 buffer_size;
	u8 *unz_buffer;
	u32 unz_buffer_size;

	u32 reorder_timeout;
	Bool force_reorder;
    Bool progressive_dispatch;
    
	u32 slt_version, rrt_version, systime_version, aeat_version;
	GF_List *services;

	GF_List *object_reservoir;
	GF_BitStream *bs;

	GF_DOMParser *dom;
	u32 service_autotune;
	Bool tune_all_sls;

	GF_SockGroup *active_sockets;


	void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *info);
	void *udta;

	u32 debug_tsi;

	u64 nb_packets;
	u64 total_bytes_recv;
	u64 first_pck_time, last_pck_time;

    //for now use a single mutex for all blob access
    GF_Mutex *blob_mx;

};

static void gf_route_static_files_del(GF_List *files)
{
	while (gf_list_count(files)) {
		GF_ROUTELCTFile *rf = gf_list_pop_back(files);
		gf_free(rf->filename);
		gf_free(rf);
	}
	gf_list_del(files);
}

static void gf_route_route_session_del(GF_ROUTESession *rs)
{
	if (rs->sock) gf_sk_del(rs->sock);
	while (gf_list_count(rs->channels)) {
		GF_ROUTELCTChannel *lc = gf_list_pop_back(rs->channels);
		gf_route_static_files_del(lc->static_files);
		gf_free(lc->toi_template);
		gf_free(lc);
	}
	gf_list_del(rs->channels);
	gf_free(rs);
}

static void gf_route_lct_obj_del(GF_LCTObject *o)
{
	if (o->frags) gf_free(o->frags);
	if (o->payload) gf_free(o->payload);
	gf_free(o);
}

static void gf_route_service_del(GF_ROUTEDmx *routedmx, GF_ROUTEService *s)
{
	if (s->sock) {
		gf_sk_group_unregister(routedmx->active_sockets, s->sock);
		gf_sk_del(s->sock);
	}
	while (gf_list_count(s->route_sessions)) {
		GF_ROUTESession *rsess = gf_list_pop_back(s->route_sessions);
		gf_route_route_session_del(rsess);
	}
	gf_list_del(s->route_sessions);

	while (gf_list_count(s->objects)) {
		GF_LCTObject *o = gf_list_pop_back(s->objects);
		gf_route_lct_obj_del(o);
	}
	gf_list_del(s->objects);
	if (s->dst_ip) gf_free(s->dst_ip);
	gf_free(s);
}

GF_EXPORT
void gf_route_dmx_del(GF_ROUTEDmx *routedmx)
{
	if (!routedmx) return;

	if (routedmx->buffer) gf_free(routedmx->buffer);
	if (routedmx->unz_buffer) gf_free(routedmx->unz_buffer);
	if (routedmx->atsc_sock) gf_sk_del(routedmx->atsc_sock);
    if (routedmx->dom) gf_xml_dom_del(routedmx->dom);
    if (routedmx->blob_mx) gf_mx_del(routedmx->blob_mx);
	if (routedmx->services) {
		while (gf_list_count(routedmx->services)) {
			GF_ROUTEService *s = gf_list_pop_back(routedmx->services);
			gf_route_service_del(routedmx, s);
		}
		gf_list_del(routedmx->services);
	}
	if (routedmx->active_sockets) gf_sk_group_del(routedmx->active_sockets);
	if (routedmx->object_reservoir) {
		while (gf_list_count(routedmx->object_reservoir)) {
			GF_LCTObject *obj = gf_list_pop_back(routedmx->object_reservoir);
			gf_route_lct_obj_del(obj);
		}
		gf_list_del(routedmx->object_reservoir);
	}
	if (routedmx->bs) gf_bs_del(routedmx->bs);
	gf_free(routedmx);
}

static GF_ROUTEDmx *gf_route_dmx_new_internal(const char *ifce, u32 sock_buffer_size, Bool is_atsc,
							  void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *info),
							  void *udta)
{
	GF_ROUTEDmx *routedmx;
	GF_Err e;
	GF_SAFEALLOC(routedmx, GF_ROUTEDmx);
	if (!routedmx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to allocate ROUTE demuxer\n"));
		return NULL;
	}
	routedmx->ip_ifce = ifce;
	routedmx->dom = gf_xml_dom_new();
	if (!routedmx->dom) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to allocate DOM parser\n" ));
		gf_route_dmx_del(routedmx);
		return NULL;
	}
	routedmx->services = gf_list_new();
	if (!routedmx->services) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to allocate ROUTE service list\n" ));
		gf_route_dmx_del(routedmx);
		return NULL;
	}
	routedmx->object_reservoir = gf_list_new();
	if (!routedmx->object_reservoir) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to allocate ROUTE object reservoir\n" ));
		gf_route_dmx_del(routedmx);
		return NULL;
	}
    routedmx->blob_mx = gf_mx_new("ROUTEBlob");
    if (!routedmx->blob_mx) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to allocate ROUTE blob mutex\n" ));
        gf_route_dmx_del(routedmx);
        return NULL;
    }

	if (!sock_buffer_size) sock_buffer_size = GF_ROUTE_SOCK_SIZE;
	routedmx->unz_buffer_size = sock_buffer_size;
	//we store one UDP packet, or realloc to store LLS signaling so starting with 10k should be enough in most cases
	routedmx->buffer_size = 10000;
	routedmx->buffer = gf_malloc(routedmx->buffer_size);
	if (!routedmx->buffer) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to allocate socket buffer\n"));
		gf_route_dmx_del(routedmx);
		return NULL;
	}
	routedmx->unz_buffer = gf_malloc(routedmx->unz_buffer_size);
	if (!routedmx->unz_buffer) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to allocate socket buffer\n"));
		gf_route_dmx_del(routedmx);
		return NULL;
	}

	routedmx->active_sockets = gf_sk_group_new();
	if (!routedmx->active_sockets) {
		gf_route_dmx_del(routedmx);
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to create socket group\n"));
		return NULL;
	}
	//create static bs
	routedmx->bs = gf_bs_new((char*)&e, 1, GF_BITSTREAM_READ);

	routedmx->reorder_timeout = 5000;

	routedmx->on_event = on_event;
	routedmx->udta = udta;

	if (!is_atsc)
		return routedmx;

	routedmx->atsc_sock = gf_sk_new(GF_SOCK_TYPE_UDP);
	if (!routedmx->atsc_sock) {
		gf_route_dmx_del(routedmx);
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to create UDP socket\n"));
		return NULL;
	}

	gf_sk_set_usec_wait(routedmx->atsc_sock, 1);
	e = gf_sk_setup_multicast(routedmx->atsc_sock, GF_ATSC_MCAST_ADDR, GF_ATSC_MCAST_PORT, 1, GF_FALSE, (char *) ifce);
	if (e) {
		gf_route_dmx_del(routedmx);
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to bind to multicast address on interface %s\n", ifce ? ifce : "default"));
		return NULL;
	}
	gf_sk_set_buffer_size(routedmx->atsc_sock, GF_FALSE, sock_buffer_size);
	//gf_sk_set_block_mode(routedmx->sock, GF_TRUE);

	gf_sk_group_register(routedmx->active_sockets, routedmx->atsc_sock);
	return routedmx;
}

static void gf_route_register_service_sockets(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, Bool do_register)
{
    u32 i;
    GF_ROUTESession *rsess;
    if (do_register) gf_sk_group_register(routedmx->active_sockets, s->sock);
    else gf_sk_group_unregister(routedmx->active_sockets, s->sock);

    if (!s->secondary_sockets) return;

    i=0;
    while ((rsess = gf_list_enum(s->route_sessions, &i))) {
        if (! rsess->sock) continue;
        if (do_register) gf_sk_group_register(routedmx->active_sockets, rsess->sock);
        else gf_sk_group_unregister(routedmx->active_sockets, rsess->sock);
    }
}

static void gf_route_create_service(GF_ROUTEDmx *routedmx, const char *dst_ip, u32 dst_port, u32 service_id, u32 protocol)
{
	GF_ROUTEService *service;
	GF_Err e;

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Setting up service %d destination IP %s port %d\n", service_id, dst_ip, dst_port));

	GF_SAFEALLOC(service, GF_ROUTEService);
	if (!service) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to allocate service %d\n", service_id));
		return;
	}
	service->service_id = service_id;
	service->protocol = protocol;

	service->sock = gf_sk_new(GF_SOCK_TYPE_UDP);
	gf_sk_set_usec_wait(service->sock, 1);
	e = gf_sk_setup_multicast(service->sock, dst_ip, dst_port, 0, GF_FALSE, (char*) routedmx->ip_ifce);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to setup multicast on %s:%d for service %d\n", dst_ip, dst_port, service_id));
		gf_route_service_del(routedmx, service);
		return;
	}
	gf_sk_set_buffer_size(service->sock, GF_FALSE, routedmx->unz_buffer_size);
	//gf_sk_set_block_mode(service->sock, GF_TRUE);

	service->dst_ip = gf_strdup(dst_ip);
	service->port = dst_port;
	service->objects = gf_list_new();
	service->route_sessions = gf_list_new();

	gf_list_add(routedmx->services, service);

	if (routedmx->atsc_sock) {
		if (routedmx->service_autotune==0xFFFFFFFF) service->tune_mode = GF_ROUTE_TUNE_ON;
		else if (routedmx->service_autotune==0xFFFFFFFE) {
			service->tune_mode = GF_ROUTE_TUNE_ON;
			routedmx->service_autotune -= 1;
		}
		else if (routedmx->service_autotune==service_id) service->tune_mode = GF_ROUTE_TUNE_ON;
		else if (routedmx->tune_all_sls) service->tune_mode = GF_ROUTE_TUNE_SLS_ONLY;

		//we are tuning, register socket
        if (service->tune_mode != GF_ROUTE_TUNE_OFF) {
			//call gf_route_register_service_sockets rather than gf_sk_group_register for coverage purpose only
            gf_route_register_service_sockets(routedmx, service, GF_TRUE);
        }
	} else {
		service->tune_mode = GF_ROUTE_TUNE_ON;
		routedmx->service_autotune = service_id;
		gf_sk_group_register(routedmx->active_sockets, service->sock);
	}

	if (routedmx->on_event) routedmx->on_event(routedmx->udta, GF_ROUTE_EVT_SERVICE_FOUND, service_id, NULL);
}

GF_EXPORT
GF_ROUTEDmx *gf_route_atsc_dmx_new(const char *ifce, u32 sock_buffer_size,
								   void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *info),
								   void *udta)
{
	return gf_route_dmx_new_internal(ifce, sock_buffer_size, GF_TRUE, on_event, udta);

}
GF_EXPORT
GF_ROUTEDmx *gf_route_dmx_new(const char *ip, u32 port, const char *ifce, u32 sock_buffer_size,
							  void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *info),
							  void *udta)
{
	GF_ROUTEDmx *routedmx = gf_route_dmx_new_internal(ifce, sock_buffer_size, GF_FALSE, on_event, udta);
	if (!routedmx) return NULL;
	gf_route_create_service(routedmx, ip, port, 1, 1);
	return routedmx;
}

GF_EXPORT
GF_Err gf_route_atsc3_tune_in(GF_ROUTEDmx *routedmx, u32 serviceID, Bool tune_all_sls)
{
	u32 i;
	GF_ROUTEService *s;
	if (!routedmx) return GF_BAD_PARAM;
	routedmx->service_autotune = serviceID;
	routedmx->tune_all_sls = tune_all_sls;
	i=0;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		GF_ROUTETuneMode prev_mode = s->tune_mode;
		if (s->service_id==serviceID) s->tune_mode = GF_ROUTE_TUNE_ON;
		else if (serviceID==0xFFFFFFFF) s->tune_mode = GF_ROUTE_TUNE_ON;
		else if ((s->tune_mode!=GF_ROUTE_TUNE_ON) && (serviceID==0xFFFFFFFE)) {
			s->tune_mode = GF_ROUTE_TUNE_ON;
			serviceID = s->service_id;
		} else {
			s->tune_mode = tune_all_sls ? GF_ROUTE_TUNE_SLS_ONLY : GF_ROUTE_TUNE_OFF;
		}
		//we were previously not tuned
		if (prev_mode == GF_ROUTE_TUNE_OFF) {
			//we are now tuned, register sockets
			if (s->tune_mode != GF_ROUTE_TUNE_OFF) {
				gf_route_register_service_sockets(routedmx, s, GF_TRUE);
			}
		}
		//we were previously tuned
		else {
			//we are now not tuned, unregister sockets
			if (s->tune_mode == GF_ROUTE_TUNE_OFF) {
				gf_route_register_service_sockets(routedmx, s, GF_FALSE);
			}
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_route_set_reorder(GF_ROUTEDmx *routedmx, Bool force_reorder, u32 timeout_ms)
{
	if (!routedmx) return GF_BAD_PARAM;
	routedmx->reorder_timeout = timeout_ms;
	routedmx->force_reorder = force_reorder;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_route_set_allow_progressive_dispatch(GF_ROUTEDmx *routedmx, Bool allow_progressive)
{
    if (!routedmx) return GF_BAD_PARAM;
    routedmx->progressive_dispatch = allow_progressive;
    return GF_OK;
}

static GF_Err gf_route_dmx_process_slt(GF_ROUTEDmx *routedmx, GF_XMLNode *root)
{
	GF_XMLNode *n;
	u32 i=0;

	while ( ( n = gf_list_enum(root->content, &i)) ) {
		if (n->type != GF_XML_NODE_TYPE) continue;
		//setup service
		if (!strcmp(n->name, "Service")) {
			GF_XMLAttribute *att;
			GF_XMLNode *m;
			u32 j=0;
			const char *dst_ip=NULL;
			u32 dst_port = 0;
			u32 protocol = 0;
			u32 service_id=0;
			while ( ( att = gf_list_enum(n->attributes, &j)) ) {
				if (!strcmp(att->name, "serviceId")) sscanf(att->value, "%u", &service_id);
			}

			j=0;
			while ( ( m = gf_list_enum(n->content, &j)) ) {
				if (m->type != GF_XML_NODE_TYPE) continue;
				if (!strcmp(m->name, "BroadcastSvcSignaling")) {
					u32 k=0;
					while ( ( att = gf_list_enum(m->attributes, &k)) ) {
						if (!strcmp(att->name, "slsProtocol")) protocol = atoi(att->value);
						if (!strcmp(att->name, "slsDestinationIpAddress")) dst_ip = att->value;
						else if (!strcmp(att->name, "slsDestinationUdpPort")) dst_port = atoi(att->value);
						//don't care about the rest
					}
				}
			}

			if (!dst_ip || !dst_port) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] No service destination IP or port found for service %d - ignoring service\n", service_id));
				continue;
			}
			if (protocol==2) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] ATSC service %d using MMTP protocol is not supported - ignoring\n", service_id));
				continue;
			}
			if (protocol!=1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Unknown ATSC signaling protocol %d for service %d - ignoring\n", protocol, service_id));
				continue;
			}

			//todo - remove existing service ?
			gf_route_create_service(routedmx, dst_ip, dst_port, service_id, protocol);

		}
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Done scaning all services\n"));
	if (routedmx->on_event) routedmx->on_event(routedmx->udta, GF_ROUTE_EVT_SERVICE_SCAN, 0, NULL);
	return GF_OK;
}

#ifndef GPAC_DISABLE_LOG
static const char *get_lct_obj_status_name(GF_LCTObjectStatus status)
{
	switch (status) {
	case GF_LCT_OBJ_INIT: return "init";
	case GF_LCT_OBJ_RECEPTION: return "reception";
	case GF_LCT_OBJ_DONE_ERR: return "done_error";
	case GF_LCT_OBJ_DONE: return "done";
	case GF_LCT_OBJ_DISPATCHED: return "dispatched";
	}
	return "unknown";
}
#endif // GPAC_DISABLE_LOG

static void gf_route_obj_to_reservoir(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_LCTObject *obj)
{

	assert (obj->status != GF_LCT_OBJ_RECEPTION);

    if (routedmx->on_event && obj->solved_path[0]) {
        GF_ROUTEEventFileInfo finfo;
        memset(&finfo, 0, sizeof(GF_ROUTEEventFileInfo));
		finfo.filename = obj->solved_path;
		finfo.udta = obj->udta;
        routedmx->on_event(routedmx->udta, GF_ROUTE_EVT_FILE_DELETE, s->service_id, &finfo);
    }

	//remove other objects
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d : moving object tsi %u toi %u to reservoir (status %s)\n", s->service_id, obj->tsi, obj->toi, get_lct_obj_status_name(obj->status) ));

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_ROUTE, GF_LOG_DEBUG)){
		u32 i, count = gf_list_count(s->objects);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d : active objects TOIs for tsi %u: ", s->service_id, obj->tsi));
		for (i=0;i<count;i++) {
			GF_LCTObject *o = gf_list_get(s->objects, i);
			if (o==obj) continue;
			if (o->tsi != obj->tsi) continue;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, (" %u", o->toi));
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("\n"));
	}
#endif

	if (s->last_active_obj==obj) s->last_active_obj = NULL;
	obj->closed_flag = 0;
	obj->force_keep = 0;
	obj->nb_bytes = 0;
	obj->nb_frags = GF_FALSE;
	obj->nb_recv_frags = 0;
	obj->rlct = NULL;
	obj->rlct_file = NULL;
	obj->toi = 0;
    obj->tsi = 0;
	obj->udta = NULL;
    obj->solved_path[0] = 0;
	obj->total_length = 0;
	obj->prev_start_offset = 0;
	obj->download_time_ms = 0;
	obj->last_gather_time = 0;
	obj->status = GF_LCT_OBJ_INIT;
	gf_list_del_item(s->objects, obj);
	gf_list_add(routedmx->object_reservoir, obj);

}

static GF_Err gf_route_dmx_push_object(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_LCTObject *obj, Bool final_push, Bool partial, Bool updated, u64 bytes_done)
{
    char *filepath;
    Bool is_init = GF_FALSE;

    if (obj->rlct_file) {
        filepath = obj->rlct_file->filename ? obj->rlct_file->filename : "ghost-init.mp4";
        is_init = GF_TRUE;
        assert(final_push);
    } else {
        if (!obj->solved_path[0])
            sprintf(obj->solved_path, obj->rlct->toi_template, obj->toi);
        filepath = obj->solved_path;
    }
#ifndef GPAC_DISABLE_LOG
    if (partial) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d got file %s (TSI %u TOI %u) size %d in %d ms (%d bytes in %d fragments)\n", s->service_id, filepath, obj->tsi, obj->toi, obj->total_length, obj->download_time_ms, obj->nb_bytes, obj->nb_recv_frags));
    } else {
        GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Service %d got file %s (TSI %u TOI %u) size %d in %d ms\n", s->service_id, filepath, obj->tsi, obj->toi, obj->total_length, obj->download_time_ms));
    }
#endif

    if (routedmx->on_event) {
        GF_ROUTEEventType evt_type;
        GF_ROUTEEventFileInfo finfo;
        memset(&finfo, 0, sizeof(GF_ROUTEEventFileInfo));
        finfo.filename = filepath;
		obj->blob.data = obj->payload;
		obj->blob.flags = 0;
		if (final_push) {
			if (!obj->total_length)
				obj->total_length = obj->alloc_size;
			if (partial)
				obj->blob.flags |= GF_BLOB_CORRUPTED;
			obj->blob.size = (u32) obj->total_length;
		} else {
			obj->blob.flags = GF_BLOB_IN_TRANSFER;
			obj->blob.size = (u32) bytes_done;
		}
		finfo.blob = &obj->blob;		
        finfo.total_size = obj->total_length;
        finfo.tsi = obj->tsi;
        finfo.toi = obj->toi;
		finfo.updated = updated;
		finfo.udta = obj->udta;
        finfo.download_ms = obj->download_time_ms;
        if (is_init)
            evt_type = GF_ROUTE_EVT_FILE;
        else if (final_push) {
            evt_type = GF_ROUTE_EVT_DYN_SEG;
            finfo.nb_frags = obj->nb_frags;
            finfo.frags = obj->frags;
			assert(obj->total_length <= obj->alloc_size);
        }
        else
            evt_type = GF_ROUTE_EVT_DYN_SEG_FRAG;

		routedmx->on_event(routedmx->udta, evt_type, s->service_id, &finfo);
		//store udta cookie
		obj->udta = finfo.udta;
	} else if (final_push) {
		//keep static files active, move other to reservoir
		if (!obj->rlct_file)
			gf_route_obj_to_reservoir(routedmx, s, obj);
	}
    return GF_OK;
}

static GF_Err gf_route_dmx_process_object(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_LCTObject *obj)
{
	Bool partial = GF_FALSE;
	Bool updated = GF_TRUE;

	if (!obj->rlct) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d : internal error, no LCT ROUTE channel defined for object TSI %u TOI %u\n", s->service_id, obj->tsi, obj->toi));
		return GF_SERVICE_ERROR;
	}
	assert(obj->status>GF_LCT_OBJ_RECEPTION);

	if (obj->status==GF_LCT_OBJ_DONE_ERR) {
		if (obj->rlct->tsi_init) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d : object TSI %u TOI %u partial received only\n", s->service_id, obj->tsi, obj->toi));
		}
		partial = GF_TRUE;
	}
	obj->rlct->tsi_init = GF_TRUE;
	if (obj->status == GF_LCT_OBJ_DISPATCHED) return GF_OK;
	obj->status = GF_LCT_OBJ_DISPATCHED;

	if (obj->rlct_file) {
		u32 crc = gf_crc_32(obj->payload, obj->total_length);
		if (crc != obj->rlct_file->crc) {
			obj->rlct_file->crc = crc;
			updated = GF_TRUE;
		} else {
			updated = GF_FALSE;
		}
	}
	return gf_route_dmx_push_object(routedmx, s, obj, GF_TRUE, partial, updated, 0);
}

static GF_Err gf_route_service_flush_object(GF_ROUTEService *s, GF_LCTObject *obj)
{
	u32 i;
	u64 start_offset = 0;
	obj->status = GF_LCT_OBJ_DONE;
	for (i=0; i<obj->nb_frags; i++) {
		if (start_offset != obj->frags[i].offset) {
			obj->status = GF_LCT_OBJ_DONE_ERR;
			break;
		}
		start_offset += obj->frags[i].size;
	}
	if (start_offset != obj->total_length) {
		obj->status = GF_LCT_OBJ_DONE_ERR;
	}
	obj->download_time_ms = gf_sys_clock() - obj->download_time_ms;
	return GF_EOS;
}

static GF_Err gf_route_service_gather_object(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, u32 tsi, u32 toi, u32 start_offset, char *data, u32 size, u32 total_len, Bool close_flag, Bool in_order, GF_ROUTELCTChannel *rlct, GF_LCTObject **gather_obj)
{
	Bool inserted, done;
	u32 i, count;
    Bool do_push = GF_FALSE;
	GF_LCTObject *obj = s->last_active_obj;

	if (routedmx->force_reorder)
		in_order = GF_FALSE;

	//in case last packet(s) are duplicated after we sent the object, skip them
	if (rlct) {
		if ((tsi==rlct->last_dispatched_tsi) && (toi==rlct->last_dispatched_toi)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d TSI %u TOI %u LCT fragment on already dispatched object, skipping\n", s->service_id, tsi, toi));
			return GF_OK;
		}
	} else {
		if (!tsi && (toi==s->last_dispatched_toi_on_tsi_zero)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d TSI %u TOI %u LCT fragment on already dispatched object, skipping\n", s->service_id, tsi, toi));
			return GF_OK;
		}
	}

	if (!obj || (obj->tsi!=tsi) || (obj->toi!=toi)) {
		count = gf_list_count(s->objects);
		for (i=0; i<count; i++) {
			obj = gf_list_get(s->objects, i);
			if ((obj->toi == toi) && (obj->tsi==tsi)) break;

			if (!tsi && !obj->tsi && ((obj->toi&0xFFFFFF00) == (toi&0xFFFFFF00)) ) {
				//change in version of bundle but same other flags: reuse this one
				obj->nb_frags = obj->nb_recv_frags = 0;
				obj->nb_bytes = obj->nb_recv_bytes = 0;
				obj->total_length = total_len;
				obj->toi = toi;
				obj->status = GF_LCT_OBJ_INIT;
				break;
			}
			obj = NULL;
		}
	}
	if (!obj) {
		obj = gf_list_pop_back(routedmx->object_reservoir);
		if (!obj) {
			GF_SAFEALLOC(obj, GF_LCTObject);
			if (!obj) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d failed to allocate LCT object TSI %u TOI %u\n", s->service_id, toi, tsi ));
				return GF_OUT_OF_MEM;
			}
			obj->nb_alloc_frags = 10;
			obj->frags = gf_malloc(sizeof(GF_LCTFragInfo)*obj->nb_alloc_frags);
            obj->blob.mx = routedmx->blob_mx;
		}
		obj->toi = toi;
		obj->tsi = tsi;
		obj->status = GF_LCT_OBJ_INIT;
		obj->total_length = total_len;
		if (tsi && rlct) {
			count = gf_list_count(rlct->static_files);
			obj->rlct = rlct;
			obj->rlct_file = NULL;
			for (i=0; i<count; i++) {
				GF_ROUTELCTFile *rf = gf_list_get(rlct->static_files, i);
				if (rf->toi == toi) {
					obj->rlct_file = rf;
					break;
				}
			}
		}

		if (!total_len) {
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Service %d object TSI %u TOI %u started without total-length assigned !\n", s->service_id, tsi, toi ));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d starting object TSI %u TOI %u total-length %d\n", s->service_id, tsi, toi, total_len));

		}
		obj->download_time_ms = gf_sys_clock();
		gf_list_add(s->objects, obj);
	} else if (!obj->total_length && total_len) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Service %d object TSI %u TOI %u was started without total-length assigned, assigning to %u\n", s->service_id, tsi, toi, total_len));
        
        if (obj->alloc_size < total_len) {
            gf_mx_p(routedmx->blob_mx);
            obj->payload = gf_realloc(obj->payload, total_len);
            obj->alloc_size = total_len;
            obj->blob.size = total_len;
            obj->blob.data = obj->payload;
            gf_mx_v(routedmx->blob_mx);
        }
		obj->total_length = total_len;
	} else if (total_len && (obj->total_length != total_len)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d object TSI %u TOI %u mismatch in total-length %u  assigned, %u redeclared\n", s->service_id, tsi, toi, obj->total_length, total_len));
	}
	if (s->last_active_obj != obj) {
		//last object had EOS and not completed
		if (s->last_active_obj && s->last_active_obj->closed_flag && (s->last_active_obj->status<GF_LCT_OBJ_DONE_ERR)) {
			GF_LCTObject *o = s->last_active_obj;
		 	if (o->tsi) {
		 		gf_route_service_flush_object(s, o);
				gf_route_dmx_process_object(routedmx, s, o);
			} else {
				gf_route_obj_to_reservoir(routedmx, s, o);
			}
		}
		//note that if not in order and no timeout, we wait forever !
		else if (in_order || routedmx->reorder_timeout) {
			count = gf_list_count(s->objects);
			for (i=0; i<count; i++) {
				u32 new_count;
				GF_LCTObject *o = gf_list_get(s->objects, i);
				if (o==obj) break;
				//we can only detect losses if a new TOI on the same TSI is found
				if (o->tsi != obj->tsi) continue;
				if (o->status>=GF_LCT_OBJ_DONE_ERR) continue;

				if (!in_order) {
					u32 elapsed = gf_sys_clock() - o->last_gather_time;
					if (elapsed < routedmx->reorder_timeout)
						continue;

					GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d object TSI %u TOI %u timeout after %d ms - forcing dispatch\n", s->service_id, o->tsi, o->toi, elapsed ));
				} else if (o->rlct && !o->rlct->tsi_init) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d object TSI %u TOI %u incomplete (tune-in) - forcing dispatch\n", s->service_id, o->tsi, o->toi, toi ));
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d object TSI %u TOI %u not completely received but in-order delivery signaled and new TOI %u - forcing dispatch\n", s->service_id, o->tsi, o->toi, toi ));
				}

				if (o->tsi && o->nb_frags) {
		 			gf_route_service_flush_object(s, o);
					gf_route_dmx_process_object(routedmx, s, o);
				} else {
					gf_route_obj_to_reservoir(routedmx, s, o);
				}
				new_count = gf_list_count(s->objects);
				//objects purged
				if (new_count<count) {
					i=-1;
					count = new_count;
				}
			}
		}
		s->last_active_obj = obj;
	}
	*gather_obj = obj;
	assert(obj->toi == toi);
	assert(obj->tsi == tsi);

	//keep receiving if we are done with errors
	if (obj->status >= GF_LCT_OBJ_DONE) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d object TSI %u TOI %u already received - skipping\n", s->service_id, tsi, toi ));
		return GF_EOS;
	}
	obj->last_gather_time = gf_sys_clock();

    if (!size) {
        goto check_done;
    }
	obj->nb_recv_bytes += size;

	inserted = GF_FALSE;
	for (i=0; i<obj->nb_frags; i++) {
		if ((obj->frags[i].offset <= start_offset) && (obj->frags[i].offset + obj->frags[i].size >= start_offset + size) ) {
            //data already received
			goto check_done;
		}

		//insert fragment
		if (obj->frags[i].offset > start_offset) {
			//check overlap (not sure if this is legal)
			if (start_offset + size > obj->frags[i].offset) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d Overlapping LCT fragment, not supported\n", s->service_id));
				return GF_NOT_SUPPORTED;
			}
			if (obj->nb_frags==obj->nb_alloc_frags) {
				obj->nb_alloc_frags *= 2;
				obj->frags = gf_realloc(obj->frags, sizeof(GF_LCTFragInfo)*obj->nb_alloc_frags);
			}
			memmove(&obj->frags[i+1], &obj->frags[i], sizeof(GF_LCTFragInfo) * (obj->nb_frags - i)  );
			obj->frags[i].offset = start_offset;
			obj->frags[i].size = size;
			obj->nb_bytes += size;
			obj->nb_frags++;
			inserted = GF_TRUE;
            if (!i)
                do_push = start_offset ? GF_FALSE : routedmx->progressive_dispatch;
			break;
		}
		//expand fragment
		if (obj->frags[i].offset + obj->frags[i].size == start_offset) {
			obj->frags[i].size += size;
			obj->nb_bytes += size;
			inserted = GF_TRUE;
            if (!i)
                do_push = obj->frags[0].offset ? GF_FALSE : routedmx->progressive_dispatch;
			break;
		}
	}

	if (!inserted) {
		if (obj->nb_frags==obj->nb_alloc_frags) {
			obj->nb_alloc_frags *= 2;
			obj->frags = gf_realloc(obj->frags, sizeof(GF_LCTFragInfo)*obj->nb_alloc_frags);
		}
		obj->frags[obj->nb_frags].offset = start_offset;
		obj->frags[obj->nb_frags].size = size;
		obj->nb_frags++;
		obj->nb_bytes += size;
        if (obj->nb_frags==1)
            do_push = start_offset ? GF_FALSE : routedmx->progressive_dispatch;
	}
	obj->nb_recv_frags++;
	obj->status = GF_LCT_OBJ_RECEPTION;

	assert(obj->toi == toi);
	assert(obj->tsi == tsi);
	if (start_offset + size > obj->alloc_size) {
		obj->alloc_size = start_offset + size;
		//use total size if available
		if (obj->alloc_size < obj->total_length)
			obj->alloc_size = obj->total_length;
		//for signaling objects, we set byte after last to 0 to use string functions
		if (!tsi)
			obj->alloc_size++;
        gf_mx_p(routedmx->blob_mx);
		obj->payload = gf_realloc(obj->payload, obj->alloc_size+1);
		obj->payload[obj->alloc_size] = 0;
        obj->blob.data = obj->payload;
        gf_mx_v(routedmx->blob_mx);
    }
	assert(obj->alloc_size >= start_offset + size);

	memcpy(obj->payload + start_offset, data, size);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d TSI %u TOI %u append LCT fragment, offset %d total size %d recv bytes %d - offset diff since last %d\n", s->service_id, obj->tsi, obj->toi, start_offset, obj->total_length, obj->nb_bytes, (s32) start_offset - (s32) obj->prev_start_offset));

	obj->prev_start_offset = start_offset;
	assert(obj->toi == toi);
	assert(obj->tsi == tsi);
    
    //not a file (uses templates->segment) and can push
    if (do_push && !obj->rlct_file && obj->rlct) {
        gf_route_dmx_push_object(routedmx, s, obj, GF_FALSE, GF_TRUE, GF_FALSE, obj->frags[0].size);
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d TSI %u TOI %u: %d bytes inserted on non-first fragment (%d totals), cannot push\n", s->service_id, obj->tsi, obj->toi, size, obj->nb_frags));
    }

check_done:
	//check if we are done
	done = GF_FALSE;
	if (obj->total_length) {
		if (obj->nb_bytes >= obj->total_length) {
			done = GF_TRUE;
		}
		else if (close_flag) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d object TSI %u TOI %u closed flag found (object not yet completed)\n", s->service_id, tsi, toi ));
		}
	} else {
		if (close_flag) obj->closed_flag = 1;
	}
	if (!done) return GF_OK;

	s->last_active_obj = NULL;
	if (obj->rlct) {
		obj->rlct->last_dispatched_tsi = obj->tsi;
		obj->rlct->last_dispatched_toi = obj->toi;
	} else {
		s->last_dispatched_toi_on_tsi_zero = obj->toi;
	}
	return gf_route_service_flush_object(s, obj);
}

static GF_Err gf_route_service_setup_dash(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, char *content, char *content_location)
{
	u32 len = (u32) strlen(content);

	if (s->tune_mode==GF_ROUTE_TUNE_SLS_ONLY) {
		s->tune_mode = GF_ROUTE_TUNE_OFF;
		//unregister sockets
		gf_route_register_service_sockets(routedmx, s, GF_FALSE);
	}

	if (routedmx->on_event) {
		GF_ROUTEEventFileInfo finfo;
        GF_Blob blob;
		memset(&finfo, 0, sizeof(GF_ROUTEEventFileInfo));
        memset(&blob, 0, sizeof(GF_Blob));
        blob.data = content;
        blob.size = len;
        finfo.blob = &blob;
        finfo.total_size = len;
		finfo.filename = content_location;
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Service %d received MPD file %s\n", s->service_id, content_location ));
		routedmx->on_event(routedmx->udta, GF_ROUTE_EVT_MPD, s->service_id, &finfo);
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Service %d received MPD file %s content:\n%s\n", s->service_id, content_location, content ));
	}
	return GF_OK;
}

static GF_Err gf_route_service_parse_mbms_enveloppe(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, char *content, char *content_location, u32 *stsid_version, u32 *mpd_version)
{
	u32 i, j;
	GF_Err e;
	GF_XMLAttribute *att;
	GF_XMLNode *it, *root;

	e = gf_xml_dom_parse_string(routedmx->dom, content);
	root = gf_xml_dom_get_root(routedmx->dom);
	if (e || !root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d failed to parse S-TSID: %s\n", s->service_id, gf_error_to_string(e) ));
		return e;
	}

	i=0;
	while ((it = gf_list_enum(root->content, &i))) {
		const char *content_type = NULL;
		/*const char *uri = NULL;*/
		u32 version = 0;

		if (strcmp(it->name, "item")) continue;

		j=0;
		while ((att = gf_list_enum(it->attributes, &j))) {
			if (!stricmp(att->name, "contentType")) content_type = att->value;
			/*else if (!stricmp(att->name, "metadataURI")) uri = att->value;*/
			else if (!stricmp(att->name, "version")) version = atoi(att->value);
		}
		if (!content_type) continue;
        if (!strcmp(content_type, "application/s-tsid") || !strcmp(content_type, "application/route-s-tsid+xml"))
            *stsid_version = version;
		else if (!strcmp(content_type, "application/dash+xml"))
            *mpd_version = version;
	}
	return GF_OK;
}

static GF_Err gf_route_service_setup_stsid(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, char *content, char *content_location)
{
	GF_Err e;
	GF_XMLAttribute *att;
	GF_XMLNode *rs, *ls, *srcf, *efdt, *node, *root;
	u32 i, j, k, crc, nb_lct_channels=0;

	crc = gf_crc_32(content, (u32) strlen(content) );
	if (!s->stsid_crc) {
		s->stsid_crc = crc;
	} else if (s->stsid_crc != crc) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d update of S-TSID not yet supported, skipping\n", s->service_id));
		return GF_NOT_SUPPORTED;
	} else {
		return GF_OK;
	}

	e = gf_xml_dom_parse_string(routedmx->dom, content);
	root = gf_xml_dom_get_root(routedmx->dom);
	if (e || !root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d failed to parse S-TSID: %s\n", s->service_id, gf_error_to_string(e) ));
		return e;
	}
	i=0;
	while ((rs = gf_list_enum(root->content, &i))) {
		char *dst_ip = s->dst_ip;
		u32 dst_port = s->port;
		char *file_template = NULL;
		GF_ROUTESession *rsess;
		GF_ROUTELCTChannel *rlct;
		u32 tsi = 0;
		if (rs->type != GF_XML_NODE_TYPE) continue;
		if (strcmp(rs->name, "RS")) continue;

		j=0;
		while ((att = gf_list_enum(rs->attributes, &j))) {
			if (!stricmp(att->name, "dIpAddr")) dst_ip = att->value;
			else if (!stricmp(att->name, "dPort")) dst_port = atoi(att->value);
		}

		GF_SAFEALLOC(rsess, GF_ROUTESession);
		if (!rsess) return GF_OUT_OF_MEM;

		rsess->channels = gf_list_new();

		//need a new socket for the session
		if ((strcmp(s->dst_ip, dst_ip)) || (s->port != dst_port) ) {
			rsess->sock = gf_sk_new(GF_SOCK_TYPE_UDP);
			gf_sk_set_usec_wait(rsess->sock, 1);
			e = gf_sk_setup_multicast(rsess->sock, dst_ip, dst_port, 0, GF_FALSE, (char *) routedmx->ip_ifce);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d  failed to setup mcast for route session on %s:%d\n", s->service_id, dst_ip, dst_port));
				return e;
			}
			gf_sk_set_buffer_size(rsess->sock, GF_FALSE, routedmx->unz_buffer_size);
			//gf_sk_set_block_mode(rsess->sock, GF_TRUE);
			s->secondary_sockets++;
			if (s->tune_mode == GF_ROUTE_TUNE_ON) gf_sk_group_register(routedmx->active_sockets, rsess->sock);
		}
		gf_list_add(s->route_sessions, rsess);

		j=0;
		while ((ls = gf_list_enum(rs->content, &j))) {
			GF_List *static_files;
			char *sep;
			if (ls->type != GF_XML_NODE_TYPE) continue;
			if (strcmp(ls->name, "LS")) continue;

			//extract TSI
			k=0;
			while ((att = gf_list_enum(ls->attributes, &k))) {
				if (!strcmp(att->name, "tsi")) sscanf(att->value, "%u", &tsi);
			}
			if (!tsi) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d missing TSI in LS/ROUTE session\n", s->service_id));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			k=0;
			srcf = NULL;
			while ((srcf = gf_list_enum(ls->content, &k))) {
				if ((srcf->type == GF_XML_NODE_TYPE) && !strcmp(srcf->name, "SrcFlow")) break;
				srcf = NULL;
			}
			if (!srcf) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d missing srcFlow in LS/ROUTE session\n", s->service_id));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			//enum srcf for efdt
			k=0;
			efdt = NULL;
			while ((node = gf_list_enum(srcf->content, &k))) {
				if (node->type != GF_XML_NODE_TYPE) continue;
				if (!strcmp(node->name, "EFDT")) efdt = node;
			}
			if (!efdt) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d missing EFDT element in LS/ROUTE session, not supported\n", s->service_id));
				return GF_NOT_SUPPORTED;
			}

			static_files = gf_list_new();

			k=0;
			while ((node = gf_list_enum(efdt->content, &k))) {
				if (node->type != GF_XML_NODE_TYPE) continue;
				//Korean version
				if (!strcmp(node->name, "FileTemplate")) {
					GF_XMLNode *cnode = gf_list_get(node->content, 0);
					if (cnode->type==GF_XML_TEXT_TYPE) file_template = cnode->name;
				}
				else if (!strcmp(node->name, "FDTParameters")) {
					u32 l=0;
					GF_XMLNode *fdt = NULL;
					while ((fdt = gf_list_enum(node->content, &l))) {
						GF_ROUTELCTFile *rf;
						if (fdt->type != GF_XML_NODE_TYPE) continue;
						if (strstr(fdt->name, "File")==NULL) continue;

						GF_SAFEALLOC(rf, GF_ROUTELCTFile)
						if (rf) {
							u32 n=0;
							while ((att = gf_list_enum(fdt->attributes, &n))) {
								if (!strcmp(att->name, "Content-Location")) rf->filename = gf_strdup(att->value);
								else if (!strcmp(att->name, "TOI")) sscanf(att->value, "%u", &rf->toi);
							}
							if (!rf->filename) {
								gf_free(rf);
							} else {
								gf_list_add(static_files, rf);
							}
						}
					}
				}
				//US version
				else if (!strcmp(node->name, "FDT-Instance")) {
					u32 l=0;
					GF_XMLNode *fdt = NULL;
					while ((att = gf_list_enum(node->attributes, &l))) {
						if (strstr(att->name, "fileTemplate")) file_template = att->value;
					}
					l=0;
					while ((fdt = gf_list_enum(node->content, &l))) {
						GF_ROUTELCTFile *rf;
						if (fdt->type != GF_XML_NODE_TYPE) continue;
						if (strstr(fdt->name, "File")==NULL) continue;

						GF_SAFEALLOC(rf, GF_ROUTELCTFile)
						if (rf) {
							u32 n=0;
							while ((att = gf_list_enum(fdt->attributes, &n))) {
								if (!strcmp(att->name, "Content-Location")) rf->filename = gf_strdup(att->value);
								else if (!strcmp(att->name, "TOI")) sscanf(att->value, "%u", &rf->toi);
							}
							if (!rf->filename) {
								gf_free(rf);
							} else {
								gf_list_add(static_files, rf);
							}
						}
					}
				}
			}

			if (!gf_list_count(static_files)) {
				GF_ROUTELCTFile *rf;
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d missing init file name in LS/ROUTE session, could be problematic - will consider any TOI %u (-1) present as a ghost init segment\n", s->service_id, (u32)-1));
				//force an init at -1, some streams still have the init not declared but send on TOI -1
				// interpreting it as a regular segment would break clock setup
				GF_SAFEALLOC(rf, GF_ROUTELCTFile)
				rf->toi = (u32) -1;
				gf_list_add(static_files, rf);
			}
			if (!file_template) {
				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Service %d missing file TOI template in LS/ROUTE session, static content only\n", s->service_id));
			} else {
				sep = strstr(file_template, "$TOI");
				if (sep) sep = strchr(sep+3, '$');

				if (!sep) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d wrong TOI template %s in LS/ROUTE session\n", s->service_id, file_template));
					gf_route_static_files_del(static_files);
					return GF_NOT_SUPPORTED;
				}
			}
			nb_lct_channels++;

			//OK setup LCT channel for route
			GF_SAFEALLOC(rlct, GF_ROUTELCTChannel);
			if (!rlct) {
				gf_route_static_files_del(static_files);
				return GF_OUT_OF_MEM;
			}
			rlct->static_files = static_files;
			rlct->tsi = tsi;
			rlct->toi_template = NULL;
			if (file_template) {
				sep = strstr(file_template, "$TOI");
				sep[0] = 0;
				gf_dynstrcat(&rlct->toi_template, file_template, NULL);
				sep[0] = '$';

				if (sep[4]=='$') {
					gf_dynstrcat(&rlct->toi_template, "%d", NULL);
					sep += 5;
				} else {
					char *sep_end = strchr(sep+3, '$');
					sep_end[0] = 0;
					gf_dynstrcat(&rlct->toi_template, sep+4, NULL);
					sep_end[0] = '$';
					sep = sep_end + 1;
				}
				gf_dynstrcat(&rlct->toi_template, sep, NULL);
			}

			//fill in payloads
			k=0;
			efdt = NULL;
			while ((node = gf_list_enum(srcf->content, &k))) {
				if (node->type != GF_XML_NODE_TYPE) continue;
				if (!strcmp(node->name, "Payload")) {
					u32 l=0;
					GF_ROUTELCTReg *lreg;
					lreg = &rlct->CPs[rlct->nb_cps];
					lreg->order = 1; //default
					while ((att = gf_list_enum(node->attributes, &l))) {
						if (!strcmp(att->name, "codePoint")) lreg->codepoint = (u8) atoi(att->value);
						else if (!strcmp(att->name, "formatId")) lreg->format_id = (u8) atoi(att->value);
						else if (!strcmp(att->name, "frag")) lreg->frag = (u8) atoi(att->value);
						else if (!strcmp(att->name, "order")) {
							if (!strcmp(att->value, "true")) lreg->order = 1;
							else lreg->order = 0;
						}
						else if (!strcmp(att->name, "srcFecPayloadId")) lreg->src_fec_payload_id = (u8) atoi(att->value);
					}
					if (lreg->src_fec_payload_id) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d payload format indicates srcFecPayloadId %d (reserved), assuming 0\n", s->service_id, lreg->src_fec_payload_id));

					}
					if (lreg->format_id != 1) {
						if (lreg->format_id && (lreg->format_id<5)) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d payload formatId %d not supported\n", s->service_id, lreg->format_id));
						} else {
							GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d payload formatId %d reserved, assuming 1\n", s->service_id, lreg->format_id));
						}
					}
					rlct->nb_cps++;
					if (rlct->nb_cps==8) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d more payload formats than supported (8 max)\n", s->service_id));
						break;
					}
				}
			}

			gf_list_add(rsess->channels, rlct);
		}
	}

	if (!nb_lct_channels) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d does not have any supported LCT channels\n", s->service_id));
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}

static GF_Err gf_route_dmx_process_service_signaling(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_LCTObject *object, u8 cc, u32 stsid_version, u32 mpd_version)
{
	char *payload, *boundary=NULL, *sep;
	char szContentType[100], szContentLocation[1024];
	u32 payload_size;
	GF_Err e;

	//uncompress bundle
	if (object->toi & 0x80000000 /*(1<<31)*/ ) {
		u32 raw_size;
		if (object->total_length > routedmx->buffer_size) {
			routedmx->buffer_size = object->total_length;
			routedmx->buffer = gf_realloc(routedmx->buffer, object->total_length);
			if (!routedmx->buffer) return GF_OUT_OF_MEM;
		}
		memcpy(routedmx->buffer, object->payload, object->total_length);
		raw_size = routedmx->unz_buffer_size;
		e = gf_gz_decompress_payload(routedmx->buffer, object->total_length, &routedmx->unz_buffer, &raw_size);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d failed to decompress signaling bundle: %s\n", s->service_id, gf_error_to_string(e) ));
			return e;
		}
		if (raw_size > routedmx->unz_buffer_size) routedmx->unz_buffer_size = raw_size;
		payload = routedmx->unz_buffer;
		payload_size = raw_size;
	} else {
		payload = object->payload;
		payload_size = object->total_length;
	}
	payload[payload_size] = 0;

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ROUTE] Service %d got TSI 0 config package:\n%s\n", s->service_id, payload ));

	//check for multipart
	if (!strncmp(payload, "Content-Type: multipart/", 24)) {
		sep = strstr(payload, "boundary=\"");
		if (!sep) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d cannot find multipart boundary in package:\n%s\n", s->service_id, payload ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		payload = sep + 10;
		sep = strstr(payload, "\"");
		if (!sep) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d multipart boundary not properly formatted in package:\n%s\n", s->service_id, payload ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		sep[0] = 0;
		boundary = gf_strdup(payload);
		sep[0] = '\"';
		payload = sep+1;
	}

	//extract all content
	while (1) {
		char *content;
		//multipart, check for start and end
		if (boundary) {
			sep = strstr(payload, boundary);
			if (!sep) break;
			payload = sep + strlen(boundary) + 2;
			sep = strstr(payload, boundary);
			if (!sep) break;
			sep[0] = 0;
		} else {
			sep = NULL;
		}

		//extract headers
		while (strncmp(payload, "\r\n\r\n", 4)) {
			u32 i=0;
			while (strchr("\n\r", payload[0]) != NULL) payload++;
			while (strchr("\r\n", payload[i]) == NULL) i++;

			if (!strnicmp(payload, "Content-Type: ", 14)) {
				u32 copy = MIN(i-14, 100);
				strncpy(szContentType, payload+14, copy);
				szContentType[copy]=0;
				payload += i;
			}
			else if (!strnicmp(payload, "Content-Location: ", 18)) {
				u32 copy = MIN(i-18, 1024);
				strncpy(szContentLocation, payload+18, copy);
				szContentLocation[copy]=0;
				payload += i;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d unrecognized header entity in package:\n%s\n", s->service_id, payload ));
			}
		}
		payload += 4;
		content = boundary ? strstr(payload, "\r\n--") : strstr(payload, "\r\n\r\n");
		if (content) {
			content[0] = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d package type %s location %s content:\n%s\n", s->service_id, szContentType, szContentLocation, payload ));
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d %s not properly formatted in package:\n%s\n", s->service_id, boundary ? "multipart boundary" : "entity", payload ));
			if (sep && boundary) sep[0] = boundary[0];
			gf_free(boundary);
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		if (!strcmp(szContentType, "application/mbms-envelope+xml")) {
			e = gf_route_service_parse_mbms_enveloppe(routedmx, s, payload, szContentLocation, &stsid_version, &mpd_version);

			if (e || !stsid_version || !mpd_version) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d MBMS envelope error %s, S-TSID version %d MPD version %d\n",s->service_id, gf_error_to_string(e), stsid_version, mpd_version ));
				gf_free(boundary);
				return e ? e : GF_SERVICE_ERROR;
			}
		} else if (!strcmp(szContentType, "application/mbms-user-service-description+xml")) {
		} else if (!strcmp(szContentType, "application/dash+xml")
			|| !strcmp(szContentType, "video/vnd.3gpp.mpd")
			|| !strcmp(szContentType, "audio/mpegurl")
			|| !strcmp(szContentType, "video/mpegurl")
		) {
			if (!s->mpd_version || (mpd_version && (mpd_version+1 != s->mpd_version))) {
				s->mpd_version = mpd_version+1;
				gf_route_service_setup_dash(routedmx, s, payload, szContentLocation);
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d same MPD version, ignoring\n",s->service_id));
			}
		}
		//Korean and US version have different mime types
		else if (!strcmp(szContentType, "application/s-tsid") || !strcmp(szContentType, "application/route-s-tsid+xml")) {
			if (!s->stsid_version || (stsid_version && (stsid_version+1 != s->stsid_version))) {
				s->stsid_version = stsid_version+1;
				gf_route_service_setup_stsid(routedmx, s, payload, szContentLocation);
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d same S-TSID version, ignoring\n",s->service_id));
			}
		}
		if (!sep) break;
		sep[0] = boundary[0];
		payload = sep;
	}

	gf_free(boundary);
	return GF_OK;
}


static GF_Err gf_route_dmx_process_service(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_ROUTESession *route_sess)
{
	GF_Err e;
	u32 nb_read, v, C, psi, S, O, H, /*Res, A,*/ B, hdr_len, cp, cc, tsi, toi, pos;
	u32 /*a_G=0, a_U=0,*/ a_S=0, a_M=0/*, a_A=0, a_H=0, a_D=0*/;
	u64 tol_size=0;
	Bool in_order = GF_TRUE;
	u32 start_offset;
	GF_ROUTELCTChannel *rlct=NULL;
	GF_LCTObject *gather_object=NULL;

	if (route_sess) {
		e = gf_sk_receive_no_select(route_sess->sock, routedmx->buffer, routedmx->buffer_size, &nb_read);
	} else {
		e = gf_sk_receive_no_select(s->sock, routedmx->buffer, routedmx->buffer_size, &nb_read);
	}

	if (e != GF_OK) return e;
	assert(nb_read);

	routedmx->nb_packets++;
	routedmx->total_bytes_recv += nb_read;
	routedmx->last_pck_time = gf_sys_clock_high_res();
	if (!routedmx->first_pck_time) routedmx->first_pck_time = routedmx->last_pck_time;

	e = gf_bs_reassign_buffer(routedmx->bs, routedmx->buffer, nb_read);
	if (e != GF_OK) return e;

	//parse LCT header
	v = gf_bs_read_int(routedmx->bs, 4);
	C = gf_bs_read_int(routedmx->bs, 2);
	psi = gf_bs_read_int(routedmx->bs, 2);
	S = gf_bs_read_int(routedmx->bs, 1);
	O = gf_bs_read_int(routedmx->bs, 2);
	H = gf_bs_read_int(routedmx->bs, 1);
	/*Res = */gf_bs_read_int(routedmx->bs, 2);
	/*A = */gf_bs_read_int(routedmx->bs, 1);
	B = gf_bs_read_int(routedmx->bs, 1);
	hdr_len = gf_bs_read_int(routedmx->bs, 8);
	cp = gf_bs_read_int(routedmx->bs, 8);

	if (v!=1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d : wrong LCT header version %d\n", s->service_id, v));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (C!=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d : wrong ROUTE LCT header C %d, expecting 0\n", s->service_id, C));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if ((psi!=0) && (psi!=2) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d : wrong ROUTE LCT header PSI %d, expecting b00 or b10\n", s->service_id, psi));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (S!=1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d : wrong ROUTE LCT header S, shall be 1\n", s->service_id));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (O!=1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d : wrong ROUTE LCT header 0, shall be b01\n", s->service_id));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (H!=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d : wrong ROUTE LCT header H, shall be 0\n", s->service_id));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (hdr_len<4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Service %d : wrong ROUTE LCT header len %d, shall be at least 4 0\n", s->service_id, hdr_len));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (psi==0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d : FEC ROUTE not implemented\n", s->service_id));
		return GF_NOT_SUPPORTED;
	}

	cc = gf_bs_read_u32(routedmx->bs);
	tsi = gf_bs_read_u32(routedmx->bs);
	toi = gf_bs_read_u32(routedmx->bs);
	hdr_len-=4;

	//filter TSI if not 0 (service TSI) and debug mode set
	if (routedmx->debug_tsi && tsi && (tsi!=routedmx->debug_tsi)) return GF_OK;

	//look for TSI 0 first
	if (tsi!=0) {
		Bool cp_found = GF_FALSE;
		u32 i=0;
		Bool in_session = GF_FALSE;
		if (s->tune_mode==GF_ROUTE_TUNE_SLS_ONLY) return GF_OK;

		if (s->last_active_obj && (s->last_active_obj->tsi==tsi)) {
			in_session = GF_TRUE;
			rlct = s->last_active_obj->rlct;
		} else {
			GF_ROUTESession *rsess;
			i=0;
			while ((rsess = gf_list_enum(s->route_sessions, &i))) {
				u32 j=0;
				while ((rlct = gf_list_enum(rsess->channels, &j))) {
					if (rlct->tsi == tsi) {
						in_session = GF_TRUE;
						break;
					}
					rlct = NULL;
				}
				if (in_session) break;
			}
		}
		if (!in_session) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d : no session with TSI %u defined, skipping packet (TOI %u)\n", s->service_id, tsi, toi));
			return GF_OK;
		}
		for (i=0; rlct && i<rlct->nb_cps; i++) {
			if (rlct->CPs[i].codepoint==cp) {
				in_order = rlct->CPs[i].order;
				cp_found = GF_TRUE;
				break;
			}
		}
		if (!cp_found) {
			if ((cp==0) || (cp==2) || (cp>=9) ) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d : unsupported code point %d, skipping packet (TOI %u)\n", s->service_id, cp, toi));
				return GF_OK;
			}
		}
	} else {
		//check TOI for TSI 0
		//a_G = (toi & 0x80000000) /*(1<<31)*/ ? 1 : 0;
		//a_U = (toi & (1<<16)) ? 1 : 0;
		a_S = (toi & (1<<17)) ? 1 : 0;
		a_M = (toi & (1<<18)) ? 1 : 0;
		/*a_A = (toi & (1<<19)) ? 1 : 0;
		a_H = (toi & (1<<22)) ? 1 : 0;
		a_D = (toi & (1<<23)) ? 1 : 0;*/
		v = toi & 0xFF;
		//skip known version
		if (a_M && (s->mpd_version == v+1)) a_M = 0;
		if (a_S && (s->stsid_version == v+1)) a_S = 0;


		//for now we only care about S and M
		if (!a_S && ! a_M) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d : SLT bundle without MPD or S-TSID, skipping packet\n", s->service_id));
			return GF_OK;
		}
	}

	//parse extensions
	while (hdr_len) {
		u8 hel=0, het = gf_bs_read_u8(routedmx->bs);
		if (het<128) hel = gf_bs_read_u8(routedmx->bs);

		switch (het) {
		case GF_LCT_EXT_FDT:
			/*flute_version = */gf_bs_read_int(routedmx->bs, 4);
			/*fdt_instance_id = */gf_bs_read_int(routedmx->bs, 20);
			break;

		case GF_LCT_EXT_CENC:
			/*content_encodind = */gf_bs_read_int(routedmx->bs, 8);
			/*reserved = */gf_bs_read_int(routedmx->bs, 16);
			break;

		case GF_LCT_EXT_TOL24:
			tol_size = gf_bs_read_int(routedmx->bs, 24);
			break;

		case GF_LCT_EXT_TOL48:
			if (hel!=2) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d : wrong HEL %d for TOL48 LCT extension, expecting 2\n", s->service_id, hel));
				continue;
			}
			tol_size = gf_bs_read_long_int(routedmx->bs, 48);
			break;

		default:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d : unsupported header extension HEL %d HET %d, ignoring\n", s->service_id, hel, het));
			break;
		}
		if (hdr_len<hel) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Service %d : wrong HEL %d for LCT extension %d, remaining header size %d\n", s->service_id, hel, het, hdr_len));
			continue;
		}
		if (hel) hdr_len -= hel;
		else hdr_len -= 1;
	}

	start_offset = gf_bs_read_u32(routedmx->bs);
	pos = (u32) gf_bs_get_position(routedmx->bs);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] Service %d : LCT packet TSI %u TOI %u size %d startOffset %u TOL "LLU"\n", s->service_id, tsi, toi, nb_read-pos, start_offset, tol_size));

	e = gf_route_service_gather_object(routedmx, s, tsi, toi, start_offset, routedmx->buffer + pos, nb_read-pos, (u32) tol_size, B, in_order, rlct, &gather_object);

	if (e==GF_EOS) {
		if (!tsi) {
			if (gather_object->status==GF_LCT_OBJ_DONE_ERR) {
				gf_route_obj_to_reservoir(routedmx, s, gather_object);
				return GF_OK;
			}
			//we don't assign version here, we use mbms envelope for that since the bundle may have several
			//packages

			gf_route_dmx_process_service_signaling(routedmx, s, gather_object, cc, a_S ? v : 0, a_M ? v : 0);
			//we don't release the LCT object, so that we can disard future versions
		} else {
			gf_route_dmx_process_object(routedmx, s, gather_object);
		}
	}

	return GF_OK;
}

static GF_Err gf_route_dmx_process_lls(GF_ROUTEDmx *routedmx)
{
	u32 read;
	GF_Err e;
	const char *name=NULL;
	u32 lls_table_id, lls_group_id, lls_group_count, lls_table_version;
	u32 raw_size = routedmx->unz_buffer_size;
	GF_XMLNode *root;

	e = gf_sk_receive_no_select(routedmx->atsc_sock, routedmx->buffer, routedmx->buffer_size, &read);
	if (e)
		return e;

	routedmx->nb_packets++;
	routedmx->total_bytes_recv += read;
	routedmx->last_pck_time = gf_sys_clock_high_res();
	if (!routedmx->first_pck_time) routedmx->first_pck_time = routedmx->last_pck_time;

	lls_table_id = routedmx->buffer[0];
	lls_group_id = routedmx->buffer[1];
	lls_group_count = 1 + routedmx->buffer[2];
	lls_table_version = routedmx->buffer[3];

	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] LLSTable size %d version %d: ID %d (%s) - group ID %d - group count %d\n", read, lls_table_id, lls_table_version, (lls_table_id==1) ? "SLT" : (lls_table_id==2) ? "RRT" : (lls_table_id==3) ? "SystemTime" : (lls_table_id==4) ? "AEAT" : "Reserved", lls_group_id, lls_group_count));
	switch (lls_table_id) {
	case 1:
		if (routedmx->slt_version== 1+lls_table_version) return GF_OK;
		routedmx->slt_version = 1+lls_table_version;
		name="SLT";
		break;
	case 2:
		if (routedmx->rrt_version== 1+lls_table_version) return GF_OK;
		routedmx->rrt_version = 1+lls_table_version;
		name="RRT";
    	break;
	case 3:
		if (routedmx->systime_version== 1+lls_table_version) return GF_OK;
		routedmx->systime_version = 1+lls_table_version;
		name="SysTime";
    	break;
	case 4:
		if (routedmx->aeat_version== 1+lls_table_version) return GF_OK;
		routedmx->aeat_version = 1+lls_table_version;
		name="AEAT";
    	break;
	default:
		return GF_OK;
	}

	e = gf_gz_decompress_payload(&routedmx->buffer[4], read-4, &routedmx->unz_buffer, &raw_size);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to decompress %s table: %s\n", name, gf_error_to_string(e) ));
		return e;
	}
	//realloc happened
	if (routedmx->unz_buffer_size<raw_size) routedmx->unz_buffer_size = raw_size;
	routedmx->unz_buffer[raw_size]=0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ROUTE] %s table - payload:\n%s\n", name, routedmx->unz_buffer));


	e = gf_xml_dom_parse_string(routedmx->dom, routedmx->unz_buffer);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to parse SLT XML: %s\n", gf_error_to_string(e) ));
		return e;
	}

	root = gf_xml_dom_get_root(routedmx->dom);
	if (!root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ROUTE] Failed to get XML root for %s table\n", name ));
		return e;
	}

	switch (lls_table_id) {
	case 1:
		return gf_route_dmx_process_slt(routedmx, root);
	case 2:
	case 3:
	case 4:
	default:
		break;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_route_dmx_process(GF_ROUTEDmx *routedmx)
{
	u32 i, count;
	GF_Err e;

	//check all active sockets
	e = gf_sk_group_select(routedmx->active_sockets, 10, GF_SK_SELECT_READ);
	if (e) return e;

	if (routedmx->atsc_sock) {
		if (gf_sk_group_sock_is_set(routedmx->active_sockets, routedmx->atsc_sock, GF_SK_SELECT_READ)) {
			e = gf_route_dmx_process_lls(routedmx);
			if (e) return e;
		}
	}

	count = gf_list_count(routedmx->services);
	for (i=0; i<count; i++) {
		u32 j;
		GF_ROUTESession *rsess;
		GF_ROUTEService *s = (GF_ROUTEService *)gf_list_get(routedmx->services, i);
		if (s->tune_mode==GF_ROUTE_TUNE_OFF) continue;

		if (gf_sk_group_sock_is_set(routedmx->active_sockets, s->sock, GF_SK_SELECT_READ)) {
			e = gf_route_dmx_process_service(routedmx, s, NULL);
			if (e) return e;
		}
		if (s->tune_mode!=GF_ROUTE_TUNE_ON) continue;
		if (!s->secondary_sockets) continue;

		j=0;
		while ((rsess = (GF_ROUTESession *)gf_list_enum(s->route_sessions, &j) )) {
			if (gf_sk_group_sock_is_set(routedmx->active_sockets, rsess->sock, GF_SK_SELECT_READ)) {
				e = gf_route_dmx_process_service(routedmx, s, rsess);
				if (e) return e;
			}
		}

	}
	return GF_OK;
}

GF_EXPORT
Bool gf_route_dmx_find_atsc3_service(GF_ROUTEDmx *routedmx, u32 service_id)
{
	u32 i=0;
	GF_ROUTEService *s;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id != service_id) continue;
		return GF_TRUE;
	}
	return GF_FALSE;
}


GF_EXPORT
u32 gf_route_dmx_get_object_count(GF_ROUTEDmx *routedmx, u32 service_id)
{
	u32 i=0;
	GF_ROUTEService *s;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id != service_id) continue;
		return gf_list_count(s->objects);
	}
	return 0;
}

#if 0
void gf_route_dmx_print_objects(GF_ROUTEDmx *routedmx, u32 service_id)
{
	u32 i=0;
	GF_ROUTEService *s=NULL;
	GF_LCTObject *obj = NULL;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id == service_id) break;
		s = NULL;
	}
	if (!s) return;
	i=0;
	while ((obj = gf_list_enum(s->objects, &i))) {
		fprintf(stderr, "#%d TSI %d TOI %d size %d (/%d) status %d\n", i, obj->tsi, obj->toi, obj->nb_bytes, obj->total_length, obj->status);
	}
}
#endif


static GF_Err gf_route_dmx_keep_or_remove_object_by_name(GF_ROUTEDmx *routedmx, u32 service_id, char *fileName, Bool purge_previous, Bool is_remove)
{
	u32 i=0;
	GF_ROUTEService *s=NULL;
	GF_LCTObject *obj = NULL;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id == service_id) break;
		s = NULL;
	}
	if (!s) return GF_BAD_PARAM;
	i=0;
	while ((obj = gf_list_enum(s->objects, &i))) {
		u32 toi;
		if (obj->rlct && (sscanf(fileName, obj->rlct->toi_template, &toi) == 1)) {
			u32 tsi;
			if (toi == obj->toi) {
				GF_ROUTELCTChannel *rlct = obj->rlct;

				if (!is_remove) {
					obj->force_keep = 1;
					return GF_OK;
				}
				obj->force_keep = 0;

				//we likely have a loop here
				if (obj == s->last_active_obj) break;
				//obj being received do not destroy
				if (obj->status == GF_LCT_OBJ_RECEPTION) break;

				tsi = obj->tsi;
				gf_route_obj_to_reservoir(routedmx, s, obj);
				if (purge_previous) {
					i=0;
					while ((obj = gf_list_enum(s->objects, &i))) {
						if (obj->rlct != rlct) continue;
						if (obj->rlct_file) continue;
						if (obj->tsi != tsi) continue;
						if (obj->toi < toi) {
							i--;
							//we likely have a loop here
							if (obj == s->last_active_obj) return GF_OK;
							gf_route_obj_to_reservoir(routedmx, s, obj);
						}
					}
				}
				return GF_OK;
			}
		}
		else if (obj->rlct && obj->rlct_file && obj->rlct_file->filename && !strcmp(fileName, obj->rlct_file->filename)) {
			if (!is_remove) {
				obj->force_keep = 1;
			} else {
				gf_route_obj_to_reservoir(routedmx, s, obj);
			}
			return GF_OK;
		}
	}
	if (is_remove) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[ROUTE] Failed to remove object %s from service, object not found\n", fileName));
		return GF_NOT_FOUND;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_route_dmx_force_keep_object_by_name(GF_ROUTEDmx *routedmx, u32 service_id, char *fileName)
{
	return gf_route_dmx_keep_or_remove_object_by_name(routedmx, service_id, fileName, GF_FALSE, GF_FALSE);
}

GF_EXPORT
GF_Err gf_route_dmx_remove_object_by_name(GF_ROUTEDmx *routedmx, u32 service_id, char *fileName, Bool purge_previous)
{
	return gf_route_dmx_keep_or_remove_object_by_name(routedmx, service_id, fileName, purge_previous, GF_TRUE);
}

GF_EXPORT
Bool gf_route_dmx_remove_first_object(GF_ROUTEDmx *routedmx, u32 service_id)
{
	u32 i=0;
	GF_ROUTEService *s=NULL;
	GF_LCTObject *obj = NULL;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id == service_id) break;
		s = NULL;
	}
	if (!s) return GF_FALSE;

	i=0;
	while ( (obj = gf_list_enum(s->objects, &i))) {
		if (obj == s->last_active_obj) continue;
		//object is active, abort
		if (obj->status<=GF_LCT_OBJ_RECEPTION) break;

		if (obj->force_keep)
			return GF_FALSE;

		//keep static files active
		if (obj->rlct_file)
			continue;

		gf_route_obj_to_reservoir(routedmx, s, obj);
		return GF_TRUE;
	}
	return GF_FALSE;
}

GF_EXPORT
void gf_route_dmx_purge_objects(GF_ROUTEDmx *routedmx, u32 service_id)
{
	u32 i=0;
	GF_ROUTEService *s=NULL;
	GF_LCTObject *obj = NULL;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id == service_id) break;
		s = NULL;
	}
	if (!s) return;

	i=0;
	while ((obj = gf_list_enum(s->objects, &i))) {
		//only purge non signaling objects
		if (!obj->tsi) continue;
		//if object is being received keep it
		if (s->last_active_obj == obj) continue;
		//if object is static file keep it - this may need refinement in case we had init segment updates
		if (obj->rlct_file) continue;
		//obj being received do not destroy
		if (obj->status == GF_LCT_OBJ_RECEPTION) continue;
		//trash
		gf_route_obj_to_reservoir(routedmx, s, obj);
	}
}

GF_EXPORT
void gf_route_dmx_set_service_udta(GF_ROUTEDmx *routedmx, u32 service_id, void *udta)
{
	u32 i=0;
	GF_ROUTEService *s=NULL;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id == service_id) {
			s->udta = udta;
			return;
		}
	}
}

GF_EXPORT
void *gf_route_dmx_get_service_udta(GF_ROUTEDmx *routedmx, u32 service_id)
{
	u32 i=0;
	GF_ROUTEService *s=NULL;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id == service_id) {
			return s->udta;
		}
	}
	return NULL;
}

GF_EXPORT
u64 gf_route_dmx_get_first_packet_time(GF_ROUTEDmx *routedmx)
{
	return routedmx ? routedmx->first_pck_time : 0;
}

GF_EXPORT
u64 gf_route_dmx_get_last_packet_time(GF_ROUTEDmx *routedmx)
{
	return routedmx ? routedmx->last_pck_time : 0;
}

GF_EXPORT
u64 gf_route_dmx_get_nb_packets(GF_ROUTEDmx *routedmx)
{
	return routedmx ? routedmx->nb_packets : 0;
}

GF_EXPORT
u64 gf_route_dmx_get_recv_bytes(GF_ROUTEDmx *routedmx)
{
	return routedmx ? routedmx->total_bytes_recv : 0;
}

GF_EXPORT
void gf_route_dmx_debug_tsi(GF_ROUTEDmx *routedmx, u32 tsi)
{
	if (routedmx) routedmx->debug_tsi = tsi;
}

#endif /* !GPAC_DISABLE_ROUTE */
