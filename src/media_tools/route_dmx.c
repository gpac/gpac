/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools ROUTE (ATSC3, DVB-MABR) and DVB-MABR Flute demux sub-project
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

Bool gf_sk_has_nrt_netcap(GF_Socket *sk);

typedef struct __route_service GF_ROUTEService;

typedef enum
{
	GF_SERVICE_UNDEFINED=0,
	GF_SERVICE_ROUTE=1,//as defined in ATSC SLS
	GF_SERVICE_MMTP=2,//as defined in ATSC SLS
	GF_SERVICE_DVB_FLUTE,
} GF_ServiceProtocolType;

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
	//set if flute, in which case this object is not tracked in the static_files of the LCT channel object
	u32 fdt_tsi;
	//for flute only, indicate the object is no longer advertized in FDT and can be removed
	Bool can_remove;
} GF_ROUTELCTFile;

typedef struct
{
	u32 tsi;
	char *toi_template;
	//for route services only, list of static files announced in STSID
	GF_List *static_files;
	u32 num_components;

	GF_ROUTELCTReg CPs[8];
	u32 nb_cps;
	u32 last_dispatched_tsi, last_dispatched_toi;
	Bool tsi_init;
	u32 flute_fdt_crc;

	GF_ROUTEService *flute_parent_service;

	Bool is_active;
	Bool first_seg_received;

	char *dash_period_id;
	s32 dash_as_id;
	char *dash_rep_id;
} GF_ROUTELCTChannel;

typedef enum
{
	GF_LCT_OBJ_INIT=0,
	GF_LCT_OBJ_RECEPTION,
	GF_LCT_OBJ_DONE_ERR,
	GF_LCT_OBJ_DONE,
} GF_LCTObjectStatus;

typedef enum
{
	GF_FLUTE_NONE = 0,
	GF_FLUTE_FDT,
	GF_FLUTE_DVB_MABR_CFG,
	GF_FLUTE_DASH_MANIFEST,
	GF_FLUTE_HLS_MANIFEST,
	GF_FLUTE_HLS_VARIANT,
	GF_FLUTE_OBJ,
} GF_FLUTEType;

typedef struct {
	u32 toi;
	u32 offset;
	u32 length;
	//no guarantee that flute symbol size will be the same for all chunks...
	u16 flute_symbol_size, flute_nb_symbols;
} GF_FLUTELLMapEntry;


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
	u32 start_time_ms, download_time_ms;
	u64 last_gather_time;
	u8 closed_flag;
	u8 force_keep;
	//flag set when the last chunk has been declared in ll_map
	u8 ll_map_last;
	u8 flute_type;
	u8 dispatched;

	GF_ROUTELCTChannel *rlct;
	GF_ROUTELCTFile *rlct_file;

	u32 prev_start_offset;

	u32 flute_symbol_size, flute_nb_symbols;

    char solved_path[GF_MAX_PATH];

	//for flute ll, we rebuild the complete segment so we need a map of chunk TOIs
	u32 ll_maps_count, ll_maps_alloc;
	GF_FLUTELLMapEntry *ll_map;

    GF_Blob blob;
	void *udta;
} GF_LCTObject;

typedef struct
{
	GF_Socket *sock;
	u32 nb_active;

	GF_List *channels;
	char *mcast_addr;
	u32 mcast_port;
} GF_ROUTESession;

typedef enum
{
	GF_ROUTE_TUNE_OFF=0,
	GF_ROUTE_TUNE_ON,
	GF_ROUTE_TUNE_SLS_ONLY,
} GF_ROUTETuneMode;

typedef GF_Err (*gf_service_process)(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_ROUTESession *route_sess);
struct __route_service
{
	u32 service_id;
	GF_ServiceProtocolType protocol;

	u32 mpd_version, stsid_version;
	GF_Socket *sock;
	u32 secondary_sockets;
	GF_List *objects;
	GF_LCTObject *last_active_obj;
	u32 nb_media_streams;
	//number of active session running on main socket
	u32 nb_active;

	gf_service_process process_service;

	u32 port;
	char *dst_ip;
	u32 last_dispatched_toi_on_tsi_zero;
	u32 stsid_crc;
	u32 flute_fdt_crc;
	u32 dvb_mabr_cfg_crc;

	GF_List *route_sessions;
	GF_ROUTETuneMode tune_mode;
	void *udta;

	char *service_identifier;
	char *log_name;
};

//maximum segs we keep in cache when playing from pcap in no realtime: this accounts for
//- init segment
//- oldest segment (used by player)
//- next segment being downloaded
//- following segment for packet reorder tests
#define MAX_SEG_IN_NRT	4

struct __gf_routedmx {
	const char *ip_ifce;
	const char *netcap_id;
	GF_Socket *atsc_sock;
	u8 *buffer;
	u32 buffer_size;
	u8 *unz_buffer;
	u32 unz_buffer_size;

	u64 reorder_timeout;
	Bool force_in_order;
	GF_RouteProgressiveDispatch dispatch_mode;
	u32 nrt_max_seg;

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

	Bool dvb_mabr;
};

static GF_Err dmx_process_service_route(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_ROUTESession *route_sess);
static GF_Err dmx_process_service_dvb_flute(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_ROUTESession *route_sess);


static void gf_route_static_files_del(GF_List *files)
{
	while (gf_list_count(files)) {
		GF_ROUTELCTFile *rf = gf_list_pop_back(files);
		gf_free(rf->filename);
		gf_free(rf);
	}
	gf_list_del(files);
}

static void gf_route_route_session_del(GF_ROUTEDmx *routedmx, GF_ROUTESession *rs)
{
	if (rs->sock) {
		gf_sk_group_unregister(routedmx->active_sockets, rs->sock);
		gf_sk_del(rs->sock);
	}
	while (gf_list_count(rs->channels)) {
		GF_ROUTELCTChannel *lc = gf_list_pop_back(rs->channels);
		gf_route_static_files_del(lc->static_files);
		if (lc->toi_template) gf_free(lc->toi_template);
		if (lc->dash_period_id) gf_free(lc->dash_period_id);
		if (lc->dash_rep_id) gf_free(lc->dash_rep_id);
		gf_free(lc);
	}
	if (rs->mcast_addr) gf_free(rs->mcast_addr);
	gf_list_del(rs->channels);
	gf_free(rs);
}

static void gf_route_lct_obj_del(GF_LCTObject *o)
{
	if (o->frags) gf_free(o->frags);
	if (o->payload) gf_free(o->payload);
	if (o->rlct_file && o->rlct_file->fdt_tsi) {
		if (o->rlct_file->filename) gf_free(o->rlct_file->filename);
		gf_free(o->rlct_file);
	}
	if (o->ll_map) gf_free(o->ll_map);
	gf_free(o);
}

static void gf_route_service_del(GF_ROUTEDmx *routedmx, GF_ROUTEService *s)
{
	if (s->sock) {
		gf_sk_group_unregister(routedmx->active_sockets, s->sock);
		gf_sk_del(s->sock);
	}
	while (gf_list_count(s->objects)) {
		GF_LCTObject *o = gf_list_pop_back(s->objects);
		gf_route_lct_obj_del(o);
	}
	gf_list_del(s->objects);

	while (gf_list_count(s->route_sessions)) {
		GF_ROUTESession *rsess = gf_list_pop_back(s->route_sessions);
		gf_route_route_session_del(routedmx, rsess);
	}
	gf_list_del(s->route_sessions);

	if (s->dst_ip) gf_free(s->dst_ip);
	if (s->log_name) gf_free(s->log_name);
	if (s->service_identifier) gf_free(s->service_identifier);
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

static GF_ROUTEDmx *gf_route_dmx_new_internal(const char *ifce, u32 sock_buffer_size, const char *netcap_id, Bool is_atsc,
							  void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *info),
							  void *udta, const char *log_name)
{
	GF_ROUTEDmx *routedmx;
	GF_Err e;
	GF_SAFEALLOC(routedmx, GF_ROUTEDmx);
	if (!routedmx) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate ROUTE demuxer\n", log_name));
		return NULL;
	}
	routedmx->ip_ifce = ifce;
	routedmx->netcap_id = netcap_id;
	routedmx->nrt_max_seg = 0;
	routedmx->dom = gf_xml_dom_new();
	if (!routedmx->dom) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate DOM parser\n", log_name));
		gf_route_dmx_del(routedmx);
		return NULL;
	}
	routedmx->services = gf_list_new();
	if (!routedmx->services) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate ROUTE service list\n", log_name));
		gf_route_dmx_del(routedmx);
		return NULL;
	}
	routedmx->object_reservoir = gf_list_new();
	if (!routedmx->object_reservoir) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate ROUTE object reservoir\n", log_name));
		gf_route_dmx_del(routedmx);
		return NULL;
	}
    routedmx->blob_mx = gf_mx_new("ROUTEBlob");
    if (!routedmx->blob_mx) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate ROUTE blob mutex\n", log_name));
        gf_route_dmx_del(routedmx);
        return NULL;
    }

	if (!sock_buffer_size) sock_buffer_size = GF_ROUTE_SOCK_SIZE;
	routedmx->unz_buffer_size = sock_buffer_size;
	//we store one UDP packet, or realloc to store LLS signaling so starting with 10k should be enough in most cases
	routedmx->buffer_size = 10000;
	routedmx->buffer = gf_malloc(routedmx->buffer_size);
	if (!routedmx->buffer) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate socket buffer\n", log_name));
		gf_route_dmx_del(routedmx);
		return NULL;
	}
	routedmx->unz_buffer = gf_malloc(routedmx->unz_buffer_size);
	if (!routedmx->unz_buffer) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate socket buffer\n", log_name));
		gf_route_dmx_del(routedmx);
		return NULL;
	}

	routedmx->active_sockets = gf_sk_group_new();
	if (!routedmx->active_sockets) {
		gf_route_dmx_del(routedmx);
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to create socket group\n", log_name));
		return NULL;
	}
	//create static bs
	routedmx->bs = gf_bs_new((char*)&e, 1, GF_BITSTREAM_READ);

	routedmx->reorder_timeout = 1000;

	routedmx->on_event = on_event;
	routedmx->udta = udta;

	if (!is_atsc)
		return routedmx;

	routedmx->atsc_sock = gf_sk_new_ex(GF_SOCK_TYPE_UDP, routedmx->netcap_id);
	if (!routedmx->atsc_sock) {
		gf_route_dmx_del(routedmx);
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to create UDP socket\n", log_name));
		return NULL;
	}
	if (gf_sk_has_nrt_netcap(routedmx->atsc_sock))
		routedmx->nrt_max_seg = MAX_SEG_IN_NRT;

	gf_sk_set_usec_wait(routedmx->atsc_sock, 1);
	e = gf_sk_setup_multicast(routedmx->atsc_sock, GF_ATSC_MCAST_ADDR, GF_ATSC_MCAST_PORT, 1, GF_FALSE, (char *) ifce);
	if (e) {
		gf_route_dmx_del(routedmx);
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to bind to multicast address on interface %s\n", log_name, ifce ? ifce : "default"));
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

static GF_ROUTEService *gf_route_create_service(GF_ROUTEDmx *routedmx, const char *dst_ip, u32 dst_port, u32 service_id, GF_ServiceProtocolType protocol_type)
{
	GF_ROUTEService *service;
	GF_Err e;
	char log_name[1024];
	if ((protocol_type == GF_SERVICE_DVB_FLUTE) && !service_id) {
		sprintf(log_name, "DVB-FLUTE RFDT");
	} else {
		sprintf(log_name, "%s S%u", (protocol_type==GF_SERVICE_ROUTE) ? "ROUTE" : "DVB-FLUTE", service_id);
	}

	switch (protocol_type) {
	case GF_SERVICE_ROUTE:
	case GF_SERVICE_DVB_FLUTE:
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Unsupported protocol type %d\n", log_name, protocol_type));
		return NULL;
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Setting up service %d destination IP %s port %d\n", log_name, service_id, dst_ip, dst_port));

	GF_SAFEALLOC(service, GF_ROUTEService);
	if (!service) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate service %d\n", log_name, service_id));
		return NULL;
	}
	service->service_id = service_id;
	service->protocol = protocol_type;
	if (protocol_type==GF_SERVICE_ROUTE) {
		service->process_service = dmx_process_service_route;
	} else {
		service->process_service = dmx_process_service_dvb_flute;
	}
	service->log_name = gf_strdup(log_name);

	service->sock = gf_sk_new_ex(GF_SOCK_TYPE_UDP, routedmx->netcap_id);
	if (gf_sk_has_nrt_netcap(service->sock))
		routedmx->nrt_max_seg = MAX_SEG_IN_NRT;

	gf_sk_set_usec_wait(service->sock, 1);
	e = gf_sk_setup_multicast(service->sock, dst_ip, dst_port, 0, GF_FALSE, (char*) routedmx->ip_ifce);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to setup multicast on %s:%d\n", service->log_name, dst_ip, dst_port));
		gf_route_service_del(routedmx, service);
		return NULL;
	}

	gf_sk_set_buffer_size(service->sock, GF_FALSE, routedmx->unz_buffer_size);
	//gf_sk_set_block_mode(service->sock, GF_TRUE);

	service->dst_ip = gf_strdup(dst_ip);
	service->port = dst_port;
	service->objects = gf_list_new();
	service->route_sessions = gf_list_new();

	gf_list_add(routedmx->services, service);

	//flute default service for root FDT carrying DVB MABR manifest + other static (depending on muxers)
	//always on
	if ((protocol_type == GF_SERVICE_DVB_FLUTE) && !service_id) {
		service->tune_mode = GF_ROUTE_TUNE_ON;
		gf_sk_group_register(routedmx->active_sockets, service->sock);
		return service;
	}

	if (routedmx->atsc_sock || routedmx->dvb_mabr) {
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
	if (routedmx->on_event)
		routedmx->on_event(routedmx->udta, GF_ROUTE_EVT_SERVICE_FOUND, service_id, NULL);

	return service;
}

GF_EXPORT
GF_ROUTEDmx *gf_route_atsc_dmx_new(const char *ifce, u32 sock_buffer_size,
								   void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *info),
								   void *udta)
{
	return gf_route_dmx_new_internal(ifce, sock_buffer_size, NULL, GF_TRUE, on_event, udta, "ROUTE");
}
GF_EXPORT
GF_ROUTEDmx *gf_route_dmx_new(const char *ip, u32 port, const char *ifce, u32 sock_buffer_size,
							  void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *info),
							  void *udta)
{
	GF_ROUTEDmx *routedmx = gf_route_dmx_new_internal(ifce, sock_buffer_size, NULL, GF_FALSE, on_event, udta, "ROUTE");
	if (!routedmx) return NULL;
	gf_route_create_service(routedmx, ip, port, 1, GF_SERVICE_ROUTE);
	return routedmx;
}


GF_EXPORT
GF_ROUTEDmx *gf_route_atsc_dmx_new_ex(const char *ifce, u32 sock_buffer_size, const char *netcap_id,
								   void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *info),
								   void *udta)
{
	return gf_route_dmx_new_internal(ifce, sock_buffer_size, netcap_id, GF_TRUE, on_event, udta, "ROUTE");
}
GF_EXPORT
GF_ROUTEDmx *gf_route_dmx_new_ex(const char *ip, u32 port, const char *ifce, u32 sock_buffer_size, const char *netcap_id,
							  void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *info),
							  void *udta)
{
	GF_ROUTEDmx *routedmx = gf_route_dmx_new_internal(ifce, sock_buffer_size, netcap_id, GF_FALSE, on_event, udta, "ROUTE");
	if (!routedmx) return NULL;
	gf_route_create_service(routedmx, ip, port, 1, GF_SERVICE_ROUTE);
	return routedmx;
}

GF_ROUTEDmx *gf_dvb_mabr_dmx_new(const char *ip, u32 port, const char *ifce, u32 sock_buffer_size, const char *netcap_id, void (*on_event)(void *udta, GF_ROUTEEventType evt, u32 evt_param, GF_ROUTEEventFileInfo *finfo), void *udta)
{
	GF_ROUTEDmx *routedmx = gf_route_dmx_new_internal(ifce, sock_buffer_size, netcap_id, GF_FALSE, on_event, udta, "DVB-FLUTE");
	if (!routedmx) return NULL;
	routedmx->dvb_mabr = GF_TRUE;
	gf_route_create_service(routedmx, ip, port, 0, GF_SERVICE_DVB_FLUTE);
	return routedmx;
}

GF_EXPORT
GF_Err gf_route_atsc3_tune_in(GF_ROUTEDmx *routedmx, u32 serviceID, Bool tune_all_sls)
{
	u32 i;
	GF_ROUTEService *s;
	if (!routedmx) return GF_BAD_PARAM;
	routedmx->service_autotune = serviceID;
	if (routedmx->dvb_mabr) tune_all_sls = GF_FALSE;
	routedmx->tune_all_sls = tune_all_sls;
	i=0;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (routedmx->dvb_mabr && !s->service_id) continue;

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
GF_Err gf_route_dmx_set_reorder(GF_ROUTEDmx *routedmx, Bool force_reorder, u32 timeout_ms)
{
	if (!routedmx) return GF_BAD_PARAM;
	routedmx->reorder_timeout = timeout_ms;
	routedmx->force_in_order = !force_reorder;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_route_set_dispatch_mode(GF_ROUTEDmx *routedmx, GF_RouteProgressiveDispatch dispatch_mode)
{
    if (!routedmx) return GF_BAD_PARAM;
    routedmx->dispatch_mode = dispatch_mode;
    return GF_OK;
}

static GF_BlobRangeStatus routedmx_check_blob_range(GF_Blob *blob, u64 start_offset, u32 *io_size)
{
	GF_LCTObject *obj = blob->range_udta;
	if (!obj) return GF_BLOB_RANGE_CORRUPTED;
	if (obj->status==GF_LCT_OBJ_INIT)
		return GF_BLOB_RANGE_CORRUPTED;
	u32 i, size = *io_size;
	*io_size = 0;
	gf_mx_p(blob->mx);
	for (i=0; i<obj->nb_frags; i++) {
		GF_LCTFragInfo *frag = &obj->frags[i];
		if ((frag->offset<=start_offset) && (start_offset+size <= frag->offset + frag->size)) {
			gf_mx_v(blob->mx);
			*io_size = size;
			return GF_BLOB_RANGE_VALID;
		}
		//start is in fragment but exceeds it
		if ((frag->offset <= start_offset) && (start_offset <= frag->offset + frag->size)) {
			*io_size = frag->offset + frag->size - start_offset;
			break;
		}
	}
	gf_mx_v(blob->mx);
	if (blob->flags & GF_BLOB_IN_TRANSFER)
		return GF_BLOB_RANGE_IN_TRANSFER;
	return GF_BLOB_RANGE_CORRUPTED;
}


static GF_Err gf_route_dmx_process_slt(GF_ROUTEDmx *routedmx, GF_XMLNode *root)
{
	GF_XMLNode *n;
	u32 i=0;
	GF_List *old_services = gf_list_clone(routedmx->services);

	while ( ( n = gf_list_enum(root->content, &i)) ) {
		if (n->type != GF_XML_NODE_TYPE) continue;
		//setup service
		if (strcmp(n->name, "Service")) continue;

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
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ATSC] No service destination IP or port found for service %d - ignoring service\n", service_id));
			continue;
		}
		if (protocol==2) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ATSC] ATSC service %d using MMTP protocol is not supported - ignoring\n", service_id));
			continue;
		}
		if (protocol!=1) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ATSC] Unknown ATSC signaling protocol %d for service %d - ignoring\n", protocol, service_id));
			continue;
		}
		protocol = GF_SERVICE_ROUTE;

		GF_ROUTEService *orig_serv=NULL;
		for (j=0; j<gf_list_count(routedmx->services); j++) {
			orig_serv = gf_list_get(routedmx->services, j);
			if (orig_serv->service_id==service_id) break;
			orig_serv=NULL;
		}
		if (orig_serv) {
			gf_list_del_item(old_services, orig_serv);
			if (!strcmp(dst_ip, orig_serv->dst_ip) && (orig_serv->port==dst_port) && (orig_serv->protocol==protocol)) {
				continue;
			}
			//remove service
			gf_list_del_item(routedmx->services, orig_serv);
			gf_route_service_del(routedmx, orig_serv);
		}
		gf_route_create_service(routedmx, dst_ip, dst_port, service_id, protocol);
	}
	//remove all non redeclared services
	while (gf_list_count(old_services)) {
		GF_ROUTEService *serv = gf_list_pop_back(old_services);
		gf_route_service_del(routedmx, serv);
	}
	gf_list_del(old_services);

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[ATSC] Done scaning all services\n"));
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
	}
	return "unknown";
}
#endif // GPAC_DISABLE_LOG

static void gf_route_obj_to_reservoir(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_LCTObject *obj)
{
	assert (obj->status != GF_LCT_OBJ_RECEPTION);

    if (routedmx->on_event && (obj->solved_path[0] || (obj->rlct_file && obj->rlct_file->filename))) {
        GF_ROUTEEventFileInfo finfo;
        memset(&finfo, 0, sizeof(GF_ROUTEEventFileInfo));
		finfo.filename = obj->solved_path[0] ? obj->solved_path : obj->rlct_file->filename;
		finfo.udta = obj->udta;
        routedmx->on_event(routedmx->udta, GF_ROUTE_EVT_FILE_DELETE, s->service_id, &finfo);
    }

	//remove other objects
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Moving object tsi %u toi %u to reservoir (status %s)\n", s->log_name, obj->tsi, obj->toi, get_lct_obj_status_name(obj->status) ));

	if (s->last_active_obj==obj) s->last_active_obj = NULL;
	obj->closed_flag = 0;
	obj->force_keep = 0;
	obj->nb_bytes = 0;
	obj->nb_frags = GF_FALSE;
	obj->nb_recv_frags = 0;
	obj->ll_maps_count = 0;
	obj->ll_map_last = 0;
	obj->flute_type = 0;

	obj->rlct = NULL;
	//flute rlct file, delete
	if (obj->rlct_file && (obj->rlct_file->fdt_tsi || obj->rlct_file->can_remove)) {
		if (obj->rlct_file->filename) gf_free(obj->rlct_file->filename);
		gf_free(obj->rlct_file);
	}
	obj->rlct_file = NULL;
	obj->toi = 0;
    obj->tsi = 0;
	obj->udta = NULL;
    obj->solved_path[0] = 0;
	obj->total_length = 0;
	obj->prev_start_offset = 0;
	obj->download_time_ms = obj->start_time_ms = 0;
	obj->last_gather_time = 0;
	obj->dispatched = 0;
	gf_mx_p(obj->blob.mx);
	obj->blob.data = NULL;
	obj->blob.size = 0;
	obj->blob.flags = GF_BLOB_CORRUPTED;
	gf_mx_v(obj->blob.mx);
	obj->status = GF_LCT_OBJ_INIT;
	gf_list_del_item(s->objects, obj);
	gf_list_add(routedmx->object_reservoir, obj);

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_ROUTE, GF_LOG_DEBUG)){
		u32 i, count = gf_list_count(s->objects);
		if (!count) return;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Active objects (tsi/toi) for service: ", s->log_name));
		for (i=0;i<count;i++) {
			GF_LCTObject *o = gf_list_get(s->objects, i);
			if (o==obj) continue;
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, (" %u/%u", o->tsi, o->toi));
			if (o->rlct_file)
				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("(%s)", o->rlct_file->filename));
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("\n"));
	}
#endif

}

static void gf_route_lct_removed(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_ROUTELCTChannel *lc)
{
	u32 i, count = gf_list_count(s->objects);
	for (i=0; i<count; i++) {
		GF_LCTObject *o = gf_list_get(s->objects, i);
		if (o->rlct == lc) {
			o->rlct = NULL;
			gf_route_obj_to_reservoir(routedmx, s, o);
		}
	}
	count = gf_list_count(s->route_sessions);
	for (i=0; i<count; i++) {
		GF_ROUTESession *rsess = gf_list_get(s->route_sessions, i);
		gf_list_del_item(rsess->channels, lc);
	}
	gf_route_static_files_del(lc->static_files);
	if (lc->toi_template) gf_free(lc->toi_template);
	if (lc->dash_period_id) gf_free(lc->dash_period_id);
	if (lc->dash_rep_id) gf_free(lc->dash_rep_id);
	gf_free(lc);
}

static GF_Err gf_route_dmx_push_object(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_LCTObject *obj, Bool final_push)
{
    char *filepath;
    GF_LCTObjectPartial partial;
	Bool updated = GF_FALSE;
    Bool is_init = GF_FALSE;
    u64 bytes_done=0;

	if (final_push && (obj->status==GF_LCT_OBJ_DONE)) {
		partial = GF_LCTO_PARTIAL_NONE;
	} else {
		gf_assert(obj->nb_frags);
		//push=begin, notified size is the start range starting at 0
		partial = GF_LCTO_PARTIAL_BEGIN;
		bytes_done = obj->frags[0].size;

		GF_LCTFragInfo *f = &obj->frags[obj->nb_frags-1];
		//push=any, notified size is the max received size
		if ((obj->nb_frags>1) || obj->frags[0].offset) {
			partial = GF_LCTO_PARTIAL_ANY;
			bytes_done = f->offset+f->size;
		}
	}

    if (obj->rlct_file) {
        filepath = obj->rlct_file->filename ? obj->rlct_file->filename : "ghost-init.mp4";
        is_init = GF_TRUE;
		if ((s->protocol==GF_SERVICE_DVB_FLUTE) && obj->rlct) {
			is_init = GF_FALSE;
		} else {
			gf_assert(final_push);
		}
		if (final_push) {
			u32 crc = gf_crc_32(obj->payload, obj->total_length);
			if (crc != obj->rlct_file->crc) {
				obj->rlct_file->crc = crc;
				updated = GF_TRUE;
			} else {
				updated = GF_FALSE;
			}
		}
    } else {
        if (!obj->solved_path[0]) {
			if (!obj->rlct->toi_template) {
				if (obj->status != GF_LCT_OBJ_RECEPTION)
					gf_route_obj_to_reservoir(routedmx, s, obj);
				return GF_OK;
			}
            sprintf(obj->solved_path, obj->rlct->toi_template, obj->toi);
		}
        filepath = obj->solved_path;
    }
#ifndef GPAC_DISABLE_LOG
    if (final_push) {
        GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Got file %s (TSI %u TOI %u) size %d in %d ms\n", s->log_name, filepath, obj->tsi, obj->toi, obj->total_length, obj->download_time_ms));
    } else {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] File %s (TSI %u TOI %u) in progress - size %d in %d ms (%d bytes in %d fragments)\n", s->log_name, filepath, obj->tsi, obj->toi, obj->total_length, obj->download_time_ms, obj->nb_bytes, obj->nb_recv_frags));
    }
#endif

    if (routedmx->on_event) {
        GF_ROUTEEventType evt_type;
        GF_ROUTEEventFileInfo finfo;
        memset(&finfo, 0,sizeof(GF_ROUTEEventFileInfo));
        finfo.filename = filepath;
        gf_mx_p(obj->blob.mx);
		obj->blob.data = obj->payload;
		if (final_push) {
			if (!obj->total_length)
				obj->total_length = obj->alloc_size;
			obj->blob.size = (u32) obj->total_length;
		} else {
			obj->blob.size = (u32) bytes_done;
		}
        gf_mx_v(obj->blob.mx);
		finfo.blob = &obj->blob;
        finfo.total_size = obj->total_length;
        finfo.tsi = obj->tsi;
        finfo.toi = obj->toi;
		finfo.updated = updated;
		finfo.partial = partial;
		finfo.udta = obj->udta;
        finfo.download_ms = obj->download_time_ms;
		finfo.nb_frags = obj->nb_frags;
		finfo.frags = obj->frags;
		if (obj->rlct && !is_init) {
			finfo.first_toi_received = obj->rlct->first_seg_received;
		}

        if (final_push) {
            evt_type = is_init ? GF_ROUTE_EVT_FILE : GF_ROUTE_EVT_DYN_SEG;
			gf_assert(obj->total_length <= obj->alloc_size);
			if (obj->rlct && !is_init) {
				obj->rlct->first_seg_received = GF_TRUE;
			}
        }
        else
            evt_type = GF_ROUTE_EVT_DYN_SEG_FRAG;

		routedmx->on_event(routedmx->udta, evt_type, s->service_id, &finfo);
		//store udta cookie
		obj->udta = finfo.udta;
	} else if (final_push) {
		//keep static files active, move other to reservoir
		if (!obj->rlct_file || obj->rlct_file->can_remove)
			gf_route_obj_to_reservoir(routedmx, s, obj);
	}
    return GF_OK;
}

#define HAS_MIMES "application/dash+xml|video/vnd.3gpp.mpd|audio/vnd.3gpp.mpd|video/vnd.mpeg.dash.mpd|audio/vnd.mpeg.dash.mpd|audio/mpegurl|video/mpegurl|application/vnd.ms-sstr+xml|application/x-mpegURL|application/vnd.apple.mpegURL"

static GF_Err gf_route_dmx_process_dvb_flute_signaling(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_LCTObject *fdt_obj)
{
    u32 crc = gf_crc_32(fdt_obj->payload, fdt_obj->total_length);
    if (fdt_obj->rlct) {
		if (crc == fdt_obj->rlct->flute_fdt_crc)
			return GF_OK;
		fdt_obj->rlct->flute_fdt_crc = crc;
	} else {
		if (crc == s->flute_fdt_crc) return GF_OK;
		s->flute_fdt_crc = crc;
	}

    // Verifying that the payload is not erroneously treated as plaintext
    if (!isprint(fdt_obj->payload[0])) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Package appears to be compressed but is being treated as plaintext:\n%s\n", s->log_name, fdt_obj->payload));
    }
    fdt_obj->payload[fdt_obj->total_length]=0;

	GF_Err e = gf_xml_dom_parse_string(routedmx->dom, fdt_obj->payload);
	GF_XMLNode *root = gf_xml_dom_get_root(routedmx->dom);
	if (!root || strcmp(root->name, "FDT-Instance") || e) {
        // Error: Couldn't find start or end tags
        if (!e) e = GF_NON_COMPLIANT_BITSTREAM;
        GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Unable to extract XML content from package: %s - %s\n%s\n", s->log_name, gf_error_to_string(e), gf_xml_dom_get_error(routedmx->dom), fdt_obj->payload));
        return e;
	}

    GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] TSI %u got FDT config package:\n%s\n", s->log_name, fdt_obj->tsi, fdt_obj->payload));

	//mark all objects previously declared by this fdt as removable
	u32 i, count=gf_list_count(s->objects);
	for (i=0; i<count; i++) {
		GF_LCTObject *old_o = gf_list_get(s->objects, i);
		if (old_o->rlct_file && (old_o->rlct_file->fdt_tsi==fdt_obj->tsi)) {
			old_o->rlct_file->can_remove = GF_TRUE;
		}
	}

	GF_XMLNode *fdt;
	u32 fdt_idx=0;
	while ( (fdt = gf_list_enum(root->content, &fdt_idx)) ) {
		GF_XMLAttribute *att;
		if (!fdt || strcmp(fdt->name, "File"))
			continue;

		u32 toi=0;
		u32 tsi = fdt_obj->tsi;
		const char *content_location=NULL;
		u32 content_length=0;
		char *content_type=NULL;
		u32 flute_symbol_size = 0;
		u32 a_idx=0;

		while ( (att = gf_list_enum(fdt->attributes, &a_idx)) ) {
			if (!att->name) continue;
			if (!strcmp(att->name, "Content-Location")) content_location = att->value;
			else if (!strcmp(att->name, "Content-Type")) content_type = att->value;
			else if (!strcmp(att->name, "Transfer-Length")) content_length = atoi(att->value);
			else if (!strcmp(att->name, "FEC-OTI-Encoding-Symbol-Length")) flute_symbol_size = atoi(att->value);
			else if (!strcmp(att->name, "TOI")) toi = atoi(att->value);
		}
		if (!toi) continue;

		GF_LCTObject *obj=NULL;
		GF_ROUTELCTChannel *prev_rlct=NULL;
		u32 prev_flute_type = 0;
		u32 prev_flute_crc = 0;
		u32 i;
		for (i=0; i<gf_list_count(s->objects); i++) {
			obj = gf_list_get(s->objects, i);
			if ((obj->toi==toi) && (obj->tsi==tsi)) break;
			if ((obj->tsi==tsi) && obj->rlct_file && !strcmp(obj->rlct_file->filename, content_location)) {
				obj->toi = toi;
				break;
			}
			obj=NULL;
		}
		u32 flute_nb_symbols = content_length / flute_symbol_size;
		if (flute_nb_symbols * flute_symbol_size < content_length)
			flute_nb_symbols++;

		//found, we assume the same content if same size
		if (obj && (obj->total_length==content_length)) {
			if (obj->rlct_file) obj->rlct_file->can_remove = GF_FALSE;
			continue;
		}
		if (obj && !obj->ll_maps_count) {
			gf_list_del_item(s->objects, obj);
			prev_rlct = obj->rlct;
			prev_flute_type = obj->flute_type;
			if (obj->rlct_file)
				prev_flute_crc = obj->rlct_file->crc;
			gf_route_obj_to_reservoir(routedmx, s, obj);
			obj=NULL;
		}

		char *frag_sep = strrchr(content_location, '#');
		char *query_sep = NULL;
		u32 ll_offset = 0;
		Bool ll_is_last = 0;
		if (frag_sep) {
			ll_offset = atoi(frag_sep+1);
			query_sep = strrchr(content_location, '?');
			if (query_sep && strstr(query_sep, "isLast"))
				ll_is_last = GF_TRUE;

			if (query_sep) query_sep[0] = 0;
			else frag_sep[0] = 0;
			//look for obj
			for (i=0; i<gf_list_count(s->objects); i++) {
				obj = gf_list_get(s->objects, i);
				if ((obj->tsi==tsi) && obj->rlct_file && !strcmp(obj->rlct_file->filename, content_location))
					break;
				obj = NULL;
			}

			if (obj) {
				if (obj->ll_maps_alloc<=obj->ll_maps_count) {
					obj->ll_maps_alloc ++;
					obj->ll_map = gf_realloc(obj->ll_map, sizeof(GF_FLUTELLMapEntry)*obj->ll_maps_alloc);
				}
				GF_FLUTELLMapEntry *ll_map = &obj->ll_map[obj->ll_maps_count];
				obj->ll_maps_count++;
				if (obj->rlct_file) obj->rlct_file->can_remove = GF_FALSE;
				ll_map->toi = toi;
				ll_map->offset = ll_offset;
				ll_map->length = content_length;
				ll_map->flute_symbol_size = flute_symbol_size;
				ll_map->flute_nb_symbols = flute_nb_symbols;
				if (ll_is_last) obj->ll_map_last = 1;

				if (obj->total_length < ll_offset+content_length) {
					gf_mx_p(routedmx->blob_mx);
					obj->total_length = ll_offset+content_length;
					obj->blob.size = obj->total_length;
					if (obj->total_length > obj->alloc_size) {
						obj->payload = gf_realloc(obj->payload, obj->total_length+1);
						obj->alloc_size = obj->total_length;
						obj->blob.data = obj->payload;
					}
					gf_mx_v(routedmx->blob_mx);
				}
				if (query_sep) query_sep[0] = 0;
				else if (frag_sep) frag_sep[0] = 0;
				if (!content_length)
					obj->ll_map_last = 1;
				continue;
			}
		}

		//gathering for flute LL, try to find a preallocated obj with an allocated ll map
		if (frag_sep) {
			for (i=0; i<gf_list_count(routedmx->object_reservoir); i++) {
				obj = gf_list_get(routedmx->object_reservoir, i);
				if (obj->ll_maps_alloc) {
					gf_list_rem(routedmx->object_reservoir, i);
					break;
				}
				obj = NULL;
			}
		}

		if (!obj)
			obj = gf_list_pop_back(routedmx->object_reservoir);

		if (!obj) {
			GF_SAFEALLOC(obj, GF_LCTObject);
			if (!obj) {
				if (query_sep) query_sep[0] = 0;
				else if (frag_sep) frag_sep[0] = 0;
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate LCT object TSI %u TOI %u\n", s->log_name, toi, tsi ));
				return GF_OUT_OF_MEM;
			}
			obj->nb_alloc_frags = 10;
			obj->frags = gf_malloc(sizeof(GF_LCTFragInfo)*obj->nb_alloc_frags);
			obj->blob.mx = routedmx->blob_mx;
		}
		obj->toi = toi;
		obj->tsi = tsi;
		obj->blob.flags = 0;
		obj->blob.range_valid = routedmx_check_blob_range;
		obj->blob.range_udta = obj;

		if (ll_is_last) content_length += ll_offset;

		obj->flute_symbol_size = flute_symbol_size;
		obj->flute_nb_symbols = flute_nb_symbols;

		obj->rlct = fdt_obj->rlct;
		obj->flute_type = GF_FLUTE_OBJ;
		if (content_type) {
			if (!strcmp(content_type, "application/xml+dvb-mabr-session-configuration")) {
				obj->flute_type = GF_FLUTE_DVB_MABR_CFG;
			} else if (strstr(HAS_MIMES, content_type)) {
				if (prev_flute_type)
					obj->flute_type = prev_flute_type;
				else if (strstr(content_type, "mpd") || strstr(content_type, "dash"))
					obj->flute_type = GF_FLUTE_DASH_MANIFEST;
				else
					obj->flute_type = GF_FLUTE_HLS_MANIFEST;
				obj->rlct = prev_rlct;
			}
		}

		if (obj->total_length!=content_length) {
			obj->status = GF_LCT_OBJ_INIT;
			obj->total_length = content_length;
			obj->nb_frags = obj->nb_recv_frags = 0;
			obj->nb_bytes = obj->nb_recv_bytes = 0;

			if (obj->alloc_size < content_length) {
				gf_mx_p(routedmx->blob_mx);
				obj->payload = gf_realloc(obj->payload, obj->total_length+1);
				obj->alloc_size = obj->total_length;
				obj->blob.size = obj->total_length;
				obj->blob.data = obj->payload;
				gf_mx_v(routedmx->blob_mx);
			}
			obj->payload[obj->total_length] = 0;
		}
		if (!obj->rlct_file) {
			GF_SAFEALLOC(obj->rlct_file, GF_ROUTELCTFile);
			obj->rlct_file->filename = gf_strdup(content_location);
			obj->rlct_file->toi = toi;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Added object %s TSI %u TOI %u size %u\n", s->log_name, content_location, tsi, toi, obj->total_length));
			obj->rlct_file->fdt_tsi = fdt_obj->tsi;
			obj->rlct_file->crc = prev_flute_crc;

			//if last frag and no offset, obj is a single chunk, otherwise build map
			if (frag_sep && (!ll_is_last || ll_offset)) {
				if (!obj->ll_maps_alloc) {
					obj->ll_maps_alloc = 10;
					obj->ll_map = gf_malloc(sizeof(GF_FLUTELLMapEntry)*obj->ll_maps_alloc);
				}
				obj->ll_maps_count = 1;
				GF_FLUTELLMapEntry *ll_map = &obj->ll_map[0];
				memset(ll_map, 0, sizeof(GF_FLUTELLMapEntry));
				ll_map->toi = toi;
				ll_map->offset = ll_offset;
				ll_map->length = content_length;
				ll_map->flute_symbol_size = flute_symbol_size;
				ll_map->flute_nb_symbols = flute_nb_symbols;
				obj->flute_symbol_size = 0;
				obj->flute_nb_symbols = 0;
			}
		}
		if (query_sep) query_sep[0] = 0;
		else if (frag_sep) frag_sep[0] = 0;
		gf_list_add(s->objects, obj);
	}
	return GF_OK;
}

static GF_Err gf_route_service_setup_dash(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, char *content, char *content_location, u32 file_type);

static const char *_xml_get_attr(const GF_XMLNode *n, const char *att_name)
{
	u32 i=0;
	GF_XMLAttribute *att;
	if (!n) return NULL;
	while ((att = gf_list_enum(n->attributes, &i))) {
		if (att->name && !strcmp(att->name, att_name)) return att->value;
	}
	return NULL;
}

static const GF_XMLNode *_xml_get_child(const GF_XMLNode *n, const char *child_name)
{
	u32 i=0;
	GF_XMLNode *c;
	if (!n) return NULL;
	while ((c = gf_list_enum(n->content, &i))) {
		if (!c->type && !strcmp(c->name, child_name)) return c;
	}
	return NULL;
}
static const char *_xml_get_child_text(const GF_XMLNode *n, const char *child_name)
{
	u32 i=0;
	GF_XMLNode *c;
	if (!n) return NULL;
	while ((c = gf_list_enum(n->content, &i))) {
		if (!c->type && !strcmp(c->name, child_name)) {
			c = gf_list_get(c->content, 0);
			return c->name;
		}
	}
	return NULL;
}
static u32 _xml_get_child_count(const GF_XMLNode *n, const char *child_name)
{
	u32 i=0, nb_children=0;
	GF_XMLNode *c;
	if (!n) return 0;
	while ((c = gf_list_enum(n->content, &i))) {
		if (!c->type && !strcmp(c->name, child_name)) nb_children++;
	}
	return nb_children;
}
static GF_Err gf_route_dmx_process_dvb_mcast_signaling(GF_ROUTEDmx *routedmx, GF_ROUTEService *parent_s, GF_LCTObject *object)
{
    u32 crc = gf_crc_32(object->payload, object->total_length);
    if (crc == parent_s->dvb_mabr_cfg_crc) return GF_OK;
	parent_s->dvb_mabr_cfg_crc = crc;


    // Verifying that the payload is not erroneously treated as plaintext
    if (!isprint(object->payload[0])) {
        GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Package appears to be compressed but is being treated as plaintext:\n%s\n",
			parent_s->log_name, object->payload));
    }
    object->payload[object->total_length]=0;

	GF_Err e = gf_xml_dom_parse_string(routedmx->dom, object->payload);
	GF_XMLNode *root = gf_xml_dom_get_root(routedmx->dom);
	if (!root || strcmp(root->name, "MulticastGatewayConfiguration") || e) {
        // Error: Couldn't find start or end tags
        if (!e) e = GF_NON_COMPLIANT_BITSTREAM;
        GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Unable to extract XML content from package: %s - %s\n%s\n", parent_s->log_name, gf_error_to_string(e), gf_xml_dom_get_error(routedmx->dom), object->payload));
        return e;
	}
    GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Got TSI %d MulticastGateway configuration:\n%s\n", parent_s->log_name, object->tsi, object->payload));
    GF_List *old_services = gf_list_clone(routedmx->services);
    gf_list_del_item(old_services, parent_s);

    GF_XMLNode *mcast_sess;
	u32 s_idx=0;
	Bool has_stsid_session=GF_FALSE;
	while ( (mcast_sess = gf_list_enum(root->content, &s_idx)) ) {
		Bool is_cfg_session=GF_FALSE;
		if (!mcast_sess || !mcast_sess->name) continue;
		if (!strcmp(mcast_sess->name, "MulticastGatewayConfigurationTransportSession")) {
			is_cfg_session = GF_TRUE;
		} else if (strcmp(mcast_sess->name, "MulticastSession"))
			continue;

		GF_ROUTEService *new_s = NULL;
		GF_List *old_sessions = NULL;
		GF_List *old_channels = NULL;
		GF_LCTObject *mani_obj = NULL;
		GF_XMLNode *tr_sess;
		GF_ServiceProtocolType proto_id = 0;
		const char *service_id_uri = _xml_get_attr(mcast_sess, "serviceIdentifier");
		if (!service_id_uri) service_id_uri = "unknown";
		u32 trs_idx=0;
		while ( (tr_sess = gf_list_enum(mcast_sess->content, &trs_idx)) ) {
			u32 j;
			if (!tr_sess || !tr_sess->name) continue;
			if (!strcmp(tr_sess->name, "PresentationManifestLocator") && !is_cfg_session) {
				tr_sess = gf_list_get(tr_sess->content, 0);
				if (!tr_sess || !tr_sess->name) continue;
				u32 i, count=gf_list_count(parent_s->objects);
				for (i=0;i<count; i++) {
					GF_LCTObject *obj = gf_list_get(parent_s->objects, i);
					if (obj->rlct_file && !strcmp(obj->rlct_file->filename, tr_sess->name)) {
						mani_obj=obj;
						break;
					}
				}
				continue;
			}
			else if (is_cfg_session && !strcmp(tr_sess->name, "TransportProtocol")) {
				tr_sess = mcast_sess;
			} else if (strcmp(tr_sess->name, "MulticastTransportSession")) {
				continue;
			}

			const GF_XMLNode *trp = _xml_get_child(tr_sess, "TransportProtocol");
			const char *mode = _xml_get_attr(trp, "protocolIdentifier");
			if (mode && !strcmp(mode, "urn:dvb:metadata:cs:MulticastTransportProtocolCS:2019:FLUTE")) {
				proto_id = GF_SERVICE_DVB_FLUTE;
			} else if (mode && !strcmp(mode, "urn:dvb:metadata:cs:MulticastTransportProtocolCS:2019:ROUTE")) {
				proto_id = GF_SERVICE_ROUTE;
				//spec is really bad here, there is no way to match a ROUTE config session with the declared channels in the MulticastConfig
				//we assume that if the config session is present, setup will be done using stsid
				if (!is_cfg_session && has_stsid_session) continue;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Unrecognized protocol type %s, ignoring\n", parent_s->log_name, mode ? mode : "UNSPECIFIED"));
				continue;
			}
			trp = _xml_get_child(tr_sess, "EndpointAddress");
			if (!trp) continue;

			const char *dst_add = _xml_get_child_text(trp, "NetworkDestinationGroupAddress");
			const char *dst_port_str = _xml_get_child_text(trp, "TransportDestinationPort");
			const char *dst_tsi = _xml_get_child_text(trp, "MediaTransportSessionIdentifier");
			if (!dst_add || !dst_port_str || !dst_tsi) continue;
			u16 dst_port = atoi(dst_port_str);

			if (!new_s) {
				//config session same as our bootstrap adress, do not process
				if (!strcmp(dst_add, parent_s->dst_ip) && (parent_s->port == dst_port)) {
					gf_list_del_item(old_services, parent_s);
					if (is_cfg_session) continue;
					new_s = parent_s;
					old_sessions = gf_list_clone(new_s->route_sessions);
				} else {
					GF_ROUTEService *existing = NULL;
					for (j=0; j<gf_list_count(routedmx->services); j++) {
						existing = gf_list_get(routedmx->services, j);
						if (existing->service_identifier
							&& !strcmp(existing->service_identifier, service_id_uri)
							&& (existing->port==dst_port)
							&& !strcmp(existing->dst_ip, dst_add)
						)
							break;
						existing=NULL;
					}
					if (!existing) {
						u32 service_id = gf_list_count(routedmx->services);
						new_s = gf_route_create_service(routedmx, dst_add, dst_port, service_id, proto_id);
						if (!new_s) continue;
						new_s->service_identifier = gf_strdup(service_id_uri);
					} else {
						gf_list_del_item(old_services, existing);
						new_s = existing;
						old_sessions = gf_list_clone(new_s->route_sessions);
						old_channels = gf_list_new();
						for (j=0; j<gf_list_count(new_s->route_sessions); j++) {
							u32 k;
							GF_ROUTESession *s = gf_list_get(new_s->route_sessions, j);
							for (k=0; k<gf_list_count(s->channels); k++) {
								GF_ROUTELCTChannel *ch = gf_list_get(s->channels, k);
								gf_list_add(old_channels, ch);
							}
						}
					}
				}
			}
			if (proto_id==GF_SERVICE_ROUTE) {
				//setup done through stsid
				has_stsid_session = GF_TRUE;
				break;
			}

			GF_ROUTESession *rsess=NULL;
			for (j=0; j<gf_list_count(new_s->route_sessions); j++) {
				rsess = gf_list_get(new_s->route_sessions, j);
				if (rsess->mcast_addr && !strcmp(rsess->mcast_addr, dst_add) && (rsess->mcast_port==dst_port))
					break;
				if (!rsess->mcast_addr && !strcmp(new_s->dst_ip, dst_add) && (new_s->port==dst_port))
					break;
				rsess = NULL;
			}
			GF_ROUTELCTChannel *rlct=NULL;
			if (rsess) {
				gf_list_del_item(old_sessions, rsess);
				for (j=0; j<gf_list_count(rsess->channels); j++) {
					rlct = gf_list_get(rsess->channels, j);
					if (rlct->tsi == atoi(dst_tsi)) break;
					rlct = NULL;
				}
				if (rlct) gf_list_del_item(old_channels, rlct);
			} else {
				GF_SAFEALLOC(rsess, GF_ROUTESession);
				if (!rsess) continue;
				rsess->channels = gf_list_new();

				//need a new socket for the session
				if ((strcmp(new_s->dst_ip, dst_add)) || (new_s->port != dst_port) ) {
					rsess->sock = gf_sk_new_ex(GF_SOCK_TYPE_UDP, routedmx->netcap_id);
					if (gf_sk_has_nrt_netcap(rsess->sock))
						routedmx->nrt_max_seg = MAX_SEG_IN_NRT;

					gf_sk_set_usec_wait(rsess->sock, 1);
					e = gf_sk_setup_multicast(rsess->sock, dst_add, dst_port, 0, GF_FALSE, (char *) routedmx->ip_ifce);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to setup mcast for route session on %s:%d\n", new_s->log_name, dst_add, dst_port));
						gf_list_del(rsess->channels);
						gf_free(rsess);
						gf_list_del(old_sessions);
						goto exit;
					}
					gf_sk_set_buffer_size(rsess->sock, GF_FALSE, routedmx->unz_buffer_size);
					//gf_sk_set_block_mode(rsess->sock, GF_TRUE);
					new_s->secondary_sockets++;
					if (new_s->tune_mode == GF_ROUTE_TUNE_ON)
						gf_sk_group_register(routedmx->active_sockets, rsess->sock);

					rsess->mcast_addr = gf_strdup(dst_add);
					rsess->mcast_port = dst_port;
				} else {
					new_s->nb_active++;
				}
				gf_list_add(new_s->route_sessions, rsess);
			}

			if (!rlct) {
				//OK setup LCT channel for route
				GF_SAFEALLOC(rlct, GF_ROUTELCTChannel);
				if (!rlct) {
					continue;
				}
				gf_list_add(rsess->channels, rlct);
				rlct->is_active = GF_TRUE;
				rsess->nb_active ++;
				if (new_s->protocol==GF_SERVICE_ROUTE)
					rlct->static_files = gf_list_new();
			}

			new_s->nb_media_streams -= rlct->num_components;
			rlct->num_components = _xml_get_child_count(tr_sess, "ServiceComponentIdentifier");
			new_s->nb_media_streams += rlct->num_components;

			rlct->flute_parent_service = new_s;
			rlct->tsi = atoi(dst_tsi);
			rlct->toi_template = NULL;
			GF_ROUTELCTReg *lreg = &rlct->CPs[0];
			rlct->nb_cps=1;
			lreg->order = 1; //default
			lreg->codepoint = 0;
			lreg->format_id = 1;

			//associate manifest object with first channel we use
			if (mani_obj && !mani_obj->rlct) {
				mani_obj->rlct = rlct;
				if ((mani_obj->status>=GF_LCT_OBJ_RECEPTION) && (new_s->tune_mode==GF_ROUTE_TUNE_ON))
					gf_route_service_setup_dash(routedmx, new_s, mani_obj->payload, mani_obj->rlct_file->filename, mani_obj->flute_type);
			}

			trp = _xml_get_child(tr_sess, "ServiceComponentIdentifier");
			const char *trp_attr = _xml_get_attr(trp, "mediaPlaylistLocator");
			if (trp_attr) {
				if (rlct->dash_rep_id) gf_free(rlct->dash_rep_id);
				rlct->dash_rep_id = gf_strdup(trp_attr);
				u32 i, count=gf_list_count(parent_s->objects);
				for (i=0;i<count; i++) {
					GF_LCTObject *obj = gf_list_get(parent_s->objects, i);
					if (obj->rlct_file && !strcmp(obj->rlct_file->filename, trp_attr)) {
						obj->rlct = rlct;
						obj->flute_type = GF_FLUTE_HLS_VARIANT;
						break;
					}
				}
			} else {
				if (rlct->dash_period_id) gf_free(rlct->dash_period_id);
				trp_attr = _xml_get_attr(trp, "periodIdentifier");
				rlct->dash_period_id = trp_attr ? gf_strdup(trp_attr) : NULL;

				trp_attr = _xml_get_attr(trp, "adaptationSetIdentifier");
				rlct->dash_as_id = trp_attr ? atoi(trp_attr) : 1;

				if (rlct->dash_rep_id) gf_free(rlct->dash_rep_id);
				trp_attr = _xml_get_attr(trp, "representationIdentifier");
				rlct->dash_rep_id = trp_attr ? gf_strdup(trp_attr) : NULL;
			}
		}
		//purge old LCT channels
		while (gf_list_count(old_channels)) {
			GF_ROUTELCTChannel *lc = gf_list_pop_back(old_channels);
			gf_route_lct_removed(routedmx, new_s, lc);
		}
		gf_list_del(old_channels);

		//purge old LCT sessions
		while (gf_list_count(old_sessions)) {
			GF_ROUTESession *rsess = gf_list_pop_back(old_sessions);
			gf_route_route_session_del(routedmx, rsess);
		}
		gf_list_del(old_sessions);

		if (new_s && (new_s->tune_mode != GF_ROUTE_TUNE_ON))
			gf_route_register_service_sockets(routedmx, new_s, GF_FALSE);
	}

exit:
	//purge old services sessions
	while (gf_list_count(old_services)) {
		GF_ROUTEService *serv = gf_list_pop_back(old_services);
		gf_route_service_del(routedmx, serv);
	}
	gf_list_del(old_services);
	return e;

}

static GF_Err gf_route_dmx_process_object(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_LCTObject *obj)
{
	u32 crc;
	switch (obj->flute_type) {
	// if this flute and its an FDT packet parse the fdt and associate the object with the TOI
	case GF_FLUTE_FDT:
		if (obj->status<GF_LCT_OBJ_RECEPTION)
			return GF_SERVICE_ERROR;
		GF_Err e = gf_route_dmx_process_dvb_flute_signaling(routedmx, s, obj);
		//FDTs are always pushed to reservoir since they may have different content with same TOI
		if (obj->rlct) obj->rlct->last_dispatched_toi = obj->rlct->last_dispatched_tsi = 0;
		gf_route_obj_to_reservoir(routedmx, s, obj);
		return e;
	case GF_FLUTE_DVB_MABR_CFG:
		if (obj->status<GF_LCT_OBJ_RECEPTION)
			return GF_SERVICE_ERROR;
		return gf_route_dmx_process_dvb_mcast_signaling(routedmx, s, obj);
	case GF_FLUTE_DASH_MANIFEST:
	case GF_FLUTE_HLS_MANIFEST:
	case GF_FLUTE_HLS_VARIANT:
		if (!obj->rlct || !obj->rlct->flute_parent_service) return GF_OK;
		if (obj->rlct->flute_parent_service->tune_mode!=GF_ROUTE_TUNE_ON) return GF_OK;
		crc = gf_crc_32(obj->payload, obj->total_length);
		if (crc != obj->rlct_file->crc) {
			obj->rlct_file->crc = crc;
			return gf_route_service_setup_dash(routedmx, obj->rlct->flute_parent_service, obj->payload, obj->rlct_file->filename, obj->flute_type);
		}
		return GF_OK;
	default:
		break;
	}

	if (!obj->flute_type && !obj->rlct) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Internal error, no LCT ROUTE channel defined for object TSI %u TOI %u\n", s->log_name, obj->tsi, obj->toi));
		return GF_SERVICE_ERROR;
	}
	if (obj->status<GF_LCT_OBJ_RECEPTION)
		return GF_SERVICE_ERROR;

	if (obj->status==GF_LCT_OBJ_DONE_ERR) {
		if (obj->rlct && obj->rlct->tsi_init) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u partial received only\n", s->log_name, obj->tsi, obj->toi));
		}
	}
	if (obj->rlct)
		obj->rlct->tsi_init = GF_TRUE;

	if (obj->dispatched) return GF_OK;
	obj->dispatched = 1;

	return gf_route_dmx_push_object(routedmx, s, obj, GF_TRUE);
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
	obj->download_time_ms = gf_sys_clock() - obj->start_time_ms;
	return GF_EOS;
}

static GF_Err gf_route_service_gather_object(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, u32 tsi, u32 toi, u32 start_offset, char *data, u32 size, u32 total_len, Bool close_flag, Bool in_order, GF_ROUTELCTChannel *rlct, GF_LCTObject **gather_obj, s32 flute_esi, u32 fdt_symbol_length)
{
	Bool done;
	u32 i, j, count;
    Bool do_push = GF_FALSE;
	GF_LCTObject *obj = s->last_active_obj;
	GF_FLUTELLMapEntry *ll_map = NULL;

	//not on a broadcast channel, ignore in_order flag
	if (!routedmx->force_in_order)
		in_order = GF_FALSE;

	if (fdt_symbol_length) {
		u32 fdt_symbols = total_len / fdt_symbol_length;
		if (fdt_symbols * fdt_symbol_length < total_len)
			fdt_symbols++;

		if (fdt_symbols <= flute_esi) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] TSI %u TOI %u invalid ESI %u for block size %u and content size %u\n", s->log_name, tsi, toi, fdt_symbols, fdt_symbol_length, total_len));
			return GF_NOT_SUPPORTED;
		}
		start_offset = flute_esi * fdt_symbol_length;
	}

	if (rlct && !rlct->is_active) {
		return GF_OK;
	}

	if((rlct && (tsi==rlct->last_dispatched_tsi) && (toi==rlct->last_dispatched_toi)) || (!rlct && !tsi && (toi==s->last_dispatched_toi_on_tsi_zero))) {
		if(routedmx->on_event) {
			// Sending event about the delayed data received.
			GF_ROUTEEventFileInfo finfo;
			GF_Blob blob;
			memset(&finfo, 0, sizeof(GF_ROUTEEventFileInfo));
			memset(&blob, 0, sizeof(GF_Blob));
			blob.data = data;
			blob.size = size;
			finfo.blob = &blob;
			finfo.total_size = size;
			finfo.tsi = tsi;
			finfo.toi = toi;
			finfo.late_fragment_offset = start_offset;
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u received delayed data [%u, %u] - event sent\n", s->log_name, tsi, toi, start_offset, start_offset+size-1));
			routedmx->on_event(routedmx->udta, GF_ROUTE_EVT_LATE_DATA, s->service_id, &finfo);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u received delayed data [%u, %u] - ignoring\n", s->log_name, tsi, toi, start_offset, start_offset+size-1));
		}
		return GF_OK;
	}

	if((u64)start_offset + size > GF_UINT_MAX) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] TSI %u TOI %u Not supported: Offset (%u) + Size (%u) exceeds the maximum supported value (%u), skipping\n", s->log_name, tsi, toi, start_offset, size, GF_UINT_MAX));
		return GF_NOT_SUPPORTED;
	}
	if(total_len && (start_offset + size > total_len)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] TSI %u TOI %u Corrupted data: Offset (%u) + Size (%u) exceeds Total Size of the object (%u), skipping\n", s->log_name, tsi, toi, start_offset, size, total_len));
		return GF_NOT_SUPPORTED;
	}

	if (!obj || (obj->tsi!=tsi) || (obj->toi!=toi) || obj->ll_maps_count) {
		count = gf_list_count(s->objects);
		for (i=0; i<count; i++) {
			obj = gf_list_get(s->objects, i);

			if (obj->ll_maps_count && (obj->tsi==tsi)) {
				for (j=0;j<obj->ll_maps_count;j++) {
					if (obj->ll_map[j].toi == toi) {
						ll_map = &obj->ll_map[j];
						break;
					}
				}
				if (ll_map) break;
			}
			if ((obj->toi == toi) && (obj->tsi==tsi)) break;

			if (!tsi && !obj->tsi && ((obj->toi&0xFFFFFF00) == (toi&0xFFFFFF00)) ) {
				//change in version of bundle but same other flags: reuse this one
				obj->nb_frags = obj->nb_recv_frags = 0;
				obj->nb_bytes = obj->nb_recv_bytes = 0;
				obj->total_length = total_len;
				if (obj->total_length>obj->alloc_size) {
					gf_mx_p(routedmx->blob_mx);
					obj->payload = gf_realloc(obj->payload, obj->total_length+1);
					obj->alloc_size = obj->total_length;
					obj->blob.size = obj->total_length;
					obj->blob.data = obj->payload;
					gf_mx_v(routedmx->blob_mx);
				}
				obj->toi = toi;
				obj->rlct = rlct;
				obj->status = GF_LCT_OBJ_INIT;
				break;
			}
			obj = NULL;
		}
	}
	if ((s->protocol==GF_SERVICE_DVB_FLUTE) && !fdt_symbol_length) {
		if (!obj) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] TSI %u TOI %u unknown, skipping\n", s->log_name, tsi, toi));
			return GF_NOT_FOUND;
		}
		u32 flute_nb_symbols = ll_map ? ll_map->flute_nb_symbols : obj->flute_nb_symbols;
		u32 flute_symbol_size = ll_map ? ll_map->flute_symbol_size : obj->flute_symbol_size;
		if (flute_nb_symbols) {
			if (flute_nb_symbols <= flute_esi) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] TSI %u TOI %u invalid ESI %u for number of symbols %u (content size %u)\n", s->log_name, tsi, toi, flute_esi, flute_nb_symbols, obj->total_length));
				return GF_NOT_SUPPORTED;
			}
		}
		//consider 0-length objects as closed
		else if (ll_map && obj->ll_map_last) {
			close_flag = GF_TRUE;
		}
		//recompute start offset
		start_offset = flute_esi * flute_symbol_size;
		if (ll_map) start_offset += ll_map->offset;

		total_len = obj->total_length;
	}

	if (!obj) {
		obj = gf_list_pop_back(routedmx->object_reservoir);
		if (!obj) {
			GF_SAFEALLOC(obj, GF_LCTObject);
			if (!obj) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to allocate LCT object TSI %u TOI %u\n", s->log_name, toi, tsi ));
				return GF_OUT_OF_MEM;
			}
			obj->nb_alloc_frags = 10;
			obj->frags = gf_malloc(sizeof(GF_LCTFragInfo)*obj->nb_alloc_frags);
            obj->blob.mx = routedmx->blob_mx;
		}
		obj->toi = toi;
		obj->tsi = tsi;
		obj->blob.range_valid = routedmx_check_blob_range;
		obj->blob.range_udta = obj;
		obj->status = GF_LCT_OBJ_INIT;
		obj->total_length = total_len;
		if (fdt_symbol_length) obj->flute_type = GF_FLUTE_FDT;

		if (obj->alloc_size < total_len) {
            gf_mx_p(routedmx->blob_mx);
            obj->payload = gf_realloc(obj->payload, total_len+1);
            obj->alloc_size = total_len;
            obj->blob.size = total_len;
            obj->blob.data = obj->payload;
            gf_mx_v(routedmx->blob_mx);
        }
		if (obj->payload)
			obj->payload[total_len] = 0;
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
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u started without total-length assigned !\n", s->log_name, tsi, toi ));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Starting object TSI %u TOI %u total-length %d\n", s->log_name, tsi, toi, total_len));

		}
		obj->start_time_ms = gf_sys_clock();
		gf_list_add(s->objects, obj);
	} else if (!obj->total_length && total_len) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u was started without total-length assigned, assigning to %u\n", s->log_name, tsi, toi, total_len));
		// Check if there are no fragments in the object that extend beyond the total length
		for (i=0; i < obj->nb_frags; i++) {
			if((u64) obj->frags[i].offset + obj->frags[i].size > total_len) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u: TOL (%u) doesn't cover previously received fragment [%u, %u[, purging object \n", s->log_name, tsi, toi, total_len, obj->frags[i].offset, obj->frags[i].offset+obj->frags[i].size));
				obj->nb_frags = obj->nb_recv_frags = 0;
				obj->nb_bytes = obj->nb_recv_bytes = 0;
			}
		}
        if (obj->alloc_size < total_len) {
            gf_mx_p(routedmx->blob_mx);
            obj->payload = gf_realloc(obj->payload, total_len+1);
            obj->alloc_size = total_len;
            obj->blob.size = total_len;
            obj->blob.data = obj->payload;
            gf_mx_v(routedmx->blob_mx);
        }
		obj->total_length = total_len;
		if (obj->payload)
			obj->payload[total_len] = 0;
	} else if (total_len && (obj->total_length != total_len) && (obj->status < GF_LCT_OBJ_DONE)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u mismatch in total-length %u assigned, %u redeclared, purging objet\n", s->log_name, tsi, toi, obj->total_length, total_len));
		obj->nb_frags = obj->nb_recv_frags = 0;
		obj->nb_bytes = obj->nb_recv_bytes = 0;
		obj->total_length = total_len;

		if (obj->alloc_size < total_len) {
			gf_mx_p(routedmx->blob_mx);
			obj->payload = gf_realloc(obj->payload, obj->total_length+1);
			obj->alloc_size = obj->total_length;
			obj->blob.size = obj->total_length;
			obj->blob.data = obj->payload;
			gf_mx_v(routedmx->blob_mx);
		}

		if (obj->payload)
			obj->payload[total_len] = 0;
		obj->status = GF_LCT_OBJ_INIT;
	}
	if (s->last_active_obj != obj) {
		//last object had EOS and not completed
		if (s->last_active_obj && s->last_active_obj->closed_flag && (s->last_active_obj->status<GF_LCT_OBJ_DONE_ERR)) {
			GF_LCTObject *o = s->last_active_obj;
		 	if (o->tsi) {
		 		gf_route_service_flush_object(s, o);
				gf_route_dmx_process_object(routedmx, s, o);
			} else {
				o->status = GF_LCT_OBJ_DONE_ERR;
				gf_route_obj_to_reservoir(routedmx, s, o);
			}
		} else {
			count = gf_list_count(s->objects);
			for (i=0; i<count; i++) {
				u32 new_count;
				GF_LCTObject *o = gf_list_get(s->objects, i);
				if (o==obj) break;
				//we can only detect losses if a new TOI on the same TSI is found
				if (o->tsi != obj->tsi) continue;
				if (o->status>=GF_LCT_OBJ_DONE_ERR) continue;

				//FDT repeat in middle of object, keep alive
				if (!toi && (s->protocol==GF_SERVICE_DVB_FLUTE) ) {
					continue;
				}
				//object pushed by flute FDT with no bytes received or not last frag, keep alive
				else if (o->rlct_file && !o->rlct_file->can_remove
					&& (!o->nb_bytes || (o->ll_maps_count && !o->ll_map_last))
				) {
					continue;
				}
				//packets not in order and timeout used
				else if (!in_order && routedmx->reorder_timeout) {
					u64 elapsed = gf_sys_clock_high_res() - o->last_gather_time;
					if (elapsed < routedmx->reorder_timeout)
						continue;

					GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u timeout after %d us - forcing dispatch\n", s->log_name, o->tsi, o->toi, elapsed ));
				} else if (o->rlct && !o->rlct->tsi_init) {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u incomplete (tune-in) - forcing dispatch\n", s->log_name, o->tsi, o->toi, toi ));
				}
				//do not warn if we received a last frag in seg - todo try to flush earlier
				else if (!o->ll_map_last) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] object TSI %u TOI %u not completely received but in-order delivery signaled and new TOI %u - forcing dispatch\n", s->log_name, o->tsi, o->toi, toi ));
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
	gf_assert((ll_map ? ll_map->toi : obj->toi) == toi);
	gf_assert(obj->tsi == tsi);

	//ignore if we are done with errors
	if (obj->status == GF_LCT_OBJ_DONE) {
		return GF_EOS;
	}
	//keep receiving if we are done with errors
	if (obj->status == GF_LCT_OBJ_DONE_ERR) {
		if (routedmx->on_event) {
			// Sending event about the delayed data received.
			GF_ROUTEEventFileInfo finfo;
			GF_Blob blob;
			memset(&finfo, 0, sizeof(GF_ROUTEEventFileInfo));
			memset(&blob, 0, sizeof(GF_Blob));
			blob.data = data;
			blob.size = size;
			finfo.blob = &blob;
			finfo.total_size = size;
			finfo.tsi = tsi;
			finfo.toi = toi;
			finfo.late_fragment_offset = start_offset;
			GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u received delayed data [%u, %u] - event sent\n", s->log_name, tsi, toi, start_offset, start_offset+size-1));
			routedmx->on_event(routedmx->udta, GF_ROUTE_EVT_LATE_DATA, s->service_id, &finfo);
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u received delayed data [%u, %u] - ignoring\n", s->log_name, tsi, toi, start_offset, start_offset+size-1));
		}
		return GF_EOS;
	}
	obj->last_gather_time = gf_sys_clock_high_res();
	obj->blob.last_modification_time = obj->last_gather_time;

    if (!size) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Empty LCT packet TSI %u TOI %u\n", s->log_name, tsi, toi));
        goto check_done;
    }
	obj->nb_recv_bytes += size;

	int start_frag = -1;
	int end_frag = -1;
	for (i=0; (i < obj->nb_frags) && (start_frag==-1 || end_frag==-1); i++) {
		if((start_frag == -1) && (start_offset <= obj->frags[i].offset + obj->frags[i].size)) {
			start_frag = i;
		}

		if((end_frag == -1) && (start_offset + size < obj->frags[i].offset)) {
			end_frag = i;
		}
	}
	if(start_frag == -1) {
		start_frag = obj->nb_frags;
	}
	if(end_frag == -1) {
		end_frag = obj->nb_frags;
	}


	//only push on first packet of object
	if (!start_frag) {
		if (routedmx->dispatch_mode==GF_ROUTE_DISPATCH_OUT_OF_ORDER) {
			do_push = GF_TRUE;
		} else if (!start_offset && (routedmx->dispatch_mode==GF_ROUTE_DISPATCH_PROGRESSIVE)) {
			do_push = GF_TRUE;
		}
	}

	if (start_frag == end_frag) {
		// insert new fragment between two already received fragments or at the end
		if (obj->nb_frags==obj->nb_alloc_frags) {
			obj->nb_alloc_frags *= 2;
			obj->frags = gf_realloc(obj->frags, sizeof(GF_LCTFragInfo)*obj->nb_alloc_frags);
		}
		memmove(&obj->frags[start_frag+1], &obj->frags[start_frag], sizeof(GF_LCTFragInfo) * (obj->nb_frags - start_frag));
		obj->frags[start_frag].offset = start_offset;
		obj->frags[start_frag].size = size;
		obj->nb_bytes += size;
		obj->nb_frags++;
	} else {
		int old_size = obj->frags[start_frag].size;

		int end = MAX(start_offset+size, obj->frags[end_frag-1].offset+obj->frags[end_frag-1].size);
		obj->frags[start_frag].offset = MIN(obj->frags[start_frag].offset, start_offset);
		obj->frags[start_frag].size = end - obj->frags[start_frag].offset;

		if(end_frag == start_frag + 1) {
			// received data extends fragment of index start_frag
			if(obj->frags[start_frag].size < old_size + size) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Overlapping or already received LCT fragment [%u, %u]\n", s->log_name, start_offset, start_offset+size-1));
			}
			obj->nb_bytes += obj->frags[start_frag].size - old_size;
			//adding bytes in first frag, we can push
			if (!start_frag && !obj->frags[0].offset)
				do_push = GF_TRUE;

		} else if(end_frag > start_frag + 1) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Merging LCT fragment\n", s->log_name));
			memmove(&obj->frags[start_frag+1], &obj->frags[end_frag], sizeof(GF_LCTFragInfo) * (obj->nb_frags - end_frag));
			obj->nb_frags += start_frag - end_frag + 1;

			obj->nb_bytes = 0;
			u32 i;
			for(i=0; i < obj->nb_frags; i++) {
				obj->nb_bytes += obj->frags[i].size;
			}
		}
	}

	obj->nb_recv_frags++;
	obj->status = GF_LCT_OBJ_RECEPTION;

	gf_assert((ll_map ? ll_map->toi : obj->toi) == toi);
	gf_assert(obj->tsi == tsi);
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
        obj->blob.size = obj->alloc_size;
        gf_mx_v(routedmx->blob_mx);
    }
	gf_assert(obj->alloc_size >= start_offset + size);

	memcpy(obj->payload + start_offset, data, size);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] TSI %u TOI %u append LCT fragment (%d/%d), offset %u total size %u recv bytes %u - offset diff since last %d\n", s->log_name, obj->tsi, toi, start_frag, obj->nb_frags, start_offset, obj->total_length, obj->nb_bytes, (s32) start_offset - (s32) obj->prev_start_offset));

	obj->prev_start_offset = start_offset;
	gf_assert((ll_map ? ll_map->toi : obj->toi) == toi);
	gf_assert(obj->tsi == tsi);

    //media file (uses templates->segment or is FLUTE obj), push if we can
    if (do_push && obj->rlct && ((!obj->rlct_file && !obj->flute_type) || (obj->flute_type==GF_FLUTE_OBJ))) {
        gf_route_dmx_push_object(routedmx, s, obj, GF_FALSE);
    }
	//if no TOL specified, update blob size - no need to lock the mutex as we only increase the size but do not change the data pointer
    if (!obj->total_length && (start_offset+size > obj->blob.size))
		obj->blob.size = start_offset+size;

check_done:
	//check if we are done
	done = GF_FALSE;
	if (obj->total_length && (!ll_map || obj->ll_map_last)) {
		if (obj->nb_bytes >= obj->total_length) {
			done = GF_TRUE;
		}
		else if (close_flag) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Object TSI %u TOI %u closed flag found (object not yet completed)\n", s->log_name, tsi, toi ));
			done = GF_TRUE;
		}
	} else {
		if (close_flag && (!ll_map || obj->ll_map_last)) obj->closed_flag = 1;
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

static GF_Err gf_route_service_setup_dash(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, char *content, char *content_location, u32 file_type)
{
	u32 len = (u32) strlen(content);

	if (file_type!=GF_FLUTE_HLS_VARIANT) {
		if (s->tune_mode==GF_ROUTE_TUNE_SLS_ONLY) {
			s->tune_mode = GF_ROUTE_TUNE_OFF;
			//unregister sockets
			gf_route_register_service_sockets(routedmx, s, GF_FALSE);
		}
	}

	if (routedmx->on_event) {
		u32 evt_type = GF_ROUTE_EVT_MPD;
		GF_ROUTEEventFileInfo finfo;
        GF_Blob blob;
		memset(&finfo, 0, sizeof(GF_ROUTEEventFileInfo));
        memset(&blob, 0, sizeof(GF_Blob));
        blob.data = content;
        blob.size = len;
        finfo.blob = &blob;
        finfo.total_size = len;
		finfo.filename = content_location;
		switch (file_type) {
		case GF_FLUTE_HLS_VARIANT:
			finfo.mime = "application/vnd.apple.mpegURL";
			evt_type = GF_ROUTE_EVT_HLS_VARIANT;
			break;
		case GF_FLUTE_HLS_MANIFEST:
			finfo.mime = "application/vnd.apple.mpegURL";
			evt_type = GF_ROUTE_EVT_MPD;
			break;
		case GF_FLUTE_DASH_MANIFEST:
			evt_type = GF_ROUTE_EVT_MPD;
			finfo.mime = "application/dash+xml";
		default:
			break;
		}

		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Received MPD file %s\n", s->log_name, content_location ));
		routedmx->on_event(routedmx->udta, evt_type, s->service_id, &finfo);
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Received MPD file %s content:\n%s\n", s->log_name, content_location, content ));
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to parse S-TSID: %s - %s\n", s->log_name, gf_error_to_string(e), gf_xml_dom_get_error(routedmx->dom) ));
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
	GF_List *remove_sessions = NULL;
	GF_List *remove_channels = NULL;

	crc = gf_crc_32(content, (u32) strlen(content) );
	if (!s->stsid_crc) {
		s->stsid_crc = crc;
	} else if (s->stsid_crc != crc) {
		GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Update of S-TSID\n", s->log_name));
		//collect old network sessions
		remove_sessions = gf_list_clone(s->route_sessions);
		//collect old LCT channels
		remove_channels = gf_list_new();
		for (i=0; i<gf_list_count(s->route_sessions); i++) {
			GF_ROUTESession *rsess = gf_list_get(s->route_sessions, i);
			for (j=0; j<gf_list_count(rsess->channels); j++) {
				gf_list_add(remove_channels, gf_list_get(rsess->channels, j));
			}
		}
	} else {
		return GF_OK;
	}

	e = gf_xml_dom_parse_string(routedmx->dom, content);
	root = gf_xml_dom_get_root(routedmx->dom);
	if (e || !root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to parse S-TSID: %s - %s\n", s->log_name, gf_error_to_string(e), gf_xml_dom_get_error(routedmx->dom)));
		gf_list_del(remove_sessions);
		gf_list_del(remove_channels);
		return GF_CORRUPTED_DATA;
	}
	i=0;
	while ((rs = gf_list_enum(root->content, &i))) {
		char *dst_ip = s->dst_ip;
		u32 dst_port = s->port;
		GF_ROUTESession *rsess;
		GF_ROUTELCTChannel *rlct;
		u32 tsi = 0;
		if (rs->type != GF_XML_NODE_TYPE) continue;
		if (strcmp(rs->name, "RS")) continue;
		if (!_xml_get_child_count(rs, "LS")) continue;

		j=0;
		while ((att = gf_list_enum(rs->attributes, &j))) {
			if (!stricmp(att->name, "dIpAddr")) dst_ip = att->value;
			else if (!stricmp(att->name, "dPort")) {
				if(! gf_strict_atoui(att->value, &dst_port)) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong dPort value (%s), it should be numeric \n", s->log_name, att->value));
					gf_list_del(remove_sessions);
					gf_list_del(remove_channels);
					return GF_CORRUPTED_DATA;
				} else if(dst_port >= 65536) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong dPort value (%s), it should belong to the interval [0, 65535] \n", s->log_name, att->value));
					gf_list_del(remove_sessions);
					gf_list_del(remove_channels);
					return GF_CORRUPTED_DATA;
				}
			}
		}

		//locate existing session
		rsess = NULL;
		for (j=0; j< gf_list_count(s->route_sessions); j++) {
			rsess = gf_list_get(s->route_sessions, j);
			if (rsess->mcast_addr) {
				if (!strcmp(rsess->mcast_addr, dst_ip) && rsess->mcast_port==dst_port) {
					gf_list_del_item(remove_sessions, rsess);
					break;
				}
			} else {
				if (!strcmp(s->dst_ip, dst_ip) && s->port==dst_port) {
					gf_list_del_item(remove_sessions, rsess);
					break;
				}
			}
			rsess=NULL;
		}

		if (!rsess) {
			GF_SAFEALLOC(rsess, GF_ROUTESession);
			if (rsess) rsess->channels = gf_list_new();
			if (!rsess || !rsess->channels) {
				if (rsess) gf_free(rsess);
				gf_list_del(remove_sessions);
				gf_list_del(remove_channels);
				return GF_OUT_OF_MEM;
			}
			gf_list_add(s->route_sessions, rsess);

			//need a new socket for the session
			if ((strcmp(s->dst_ip, dst_ip)) || (s->port != dst_port) ) {
				rsess->sock = gf_sk_new_ex(GF_SOCK_TYPE_UDP, routedmx->netcap_id);
				if (gf_sk_has_nrt_netcap(rsess->sock))
					routedmx->nrt_max_seg = MAX_SEG_IN_NRT;

				gf_sk_set_usec_wait(rsess->sock, 1);
				e = gf_sk_setup_multicast(rsess->sock, dst_ip, dst_port, 0, GF_FALSE, (char *) routedmx->ip_ifce);
				if (e) {
					gf_sk_del(rsess->sock);
					gf_list_del(rsess->channels);
					gf_list_del_item(s->route_sessions, rsess);
					gf_list_del(remove_sessions);
					gf_list_del(remove_channels);
					gf_free(rsess);
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to setup mcast for route session on %s:%d\n", s->log_name, dst_ip, dst_port));
					return e;
				}
				gf_sk_set_buffer_size(rsess->sock, GF_FALSE, routedmx->unz_buffer_size);
				//gf_sk_set_block_mode(rsess->sock, GF_TRUE);
				s->secondary_sockets++;
				if (s->tune_mode == GF_ROUTE_TUNE_ON) gf_sk_group_register(routedmx->active_sockets, rsess->sock);

				rsess->mcast_addr = gf_strdup(dst_ip);
				rsess->mcast_port = dst_port;
			}
		}

		u32 nb_media_streams=0;
		j=0;
		while ((ls = gf_list_enum(rs->content, &j))) {
			char *file_template = NULL;
			char *sep;
			if (ls->type != GF_XML_NODE_TYPE) continue;
			if (strcmp(ls->name, "LS")) continue;

			//extract TSI
			k=0;
			while ((att = gf_list_enum(ls->attributes, &k))) {
				if (!strcmp(att->name, "tsi")) {
					if(! gf_strict_atoui(att->value, &tsi)) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong TSI value (%s), it should be numeric \n", s->log_name, att->value));
						gf_list_del(remove_sessions);
						gf_list_del(remove_channels);
						return GF_CORRUPTED_DATA;
					}
				}
			}
			if (!tsi) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing TSI in LS/ROUTE session\n", s->log_name));
				gf_list_del(remove_sessions);
				gf_list_del(remove_channels);
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			k=0;
			srcf = NULL;
			while ((srcf = gf_list_enum(ls->content, &k))) {
				if ((srcf->type == GF_XML_NODE_TYPE) && !strcmp(srcf->name, "SrcFlow")) break;
				srcf = NULL;
			}
			if (!srcf) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing srcFlow in LS/ROUTE session\n", s->log_name));
				gf_list_del(remove_sessions);
				gf_list_del(remove_channels);
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Missing EFDT element in LS/ROUTE session, not supported\n", s->log_name));
				gf_list_del(remove_sessions);
				gf_list_del(remove_channels);
				return GF_NOT_SUPPORTED;
			}

			//collect TOI template and all FDT files
			GF_List *fdt_files = gf_list_new();

			k=0;
			while ((node = gf_list_enum(efdt->content, &k))) {
				if (node->type != GF_XML_NODE_TYPE) continue;

				//Korean version
				if (!strcmp(node->name, "FileTemplate")) {
					GF_XMLNode *cnode = gf_list_get(node->content, 0);
					if (cnode->type==GF_XML_TEXT_TYPE) file_template = cnode->name;
					nb_media_streams++;
				}
				else if (!strcmp(node->name, "FDTParameters")) {
					u32 l=0;
					GF_XMLNode *fdt = NULL;
					while ((fdt = gf_list_enum(node->content, &l))) {
						if (fdt->type != GF_XML_NODE_TYPE) continue;
						if (strstr(fdt->name, "File")==NULL) continue;

						char *fdt_location = NULL;
						u32 fdt_toi = 0;
						u32 n=0;
						while ((att = gf_list_enum(fdt->attributes, &n))) {
							if (!strcmp(att->name, "Content-Location")) fdt_location = gf_strdup(att->value);
							else if (!strcmp(att->name, "TOI")) {
								if (! gf_strict_atoui(att->value, &fdt_toi)) fdt_toi=0;
							}
						}
						if (!fdt_toi || !fdt_location) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Corrupted FDT instance : location/TOI missing\n", s->log_name));
							continue;
						}
						gf_list_add(fdt_files, fdt);
					}
				}
				//US version
				else if (!strcmp(node->name, "FDT-Instance")) {
					u32 l=0;
					GF_XMLNode *fdt = NULL;
					while ((att = gf_list_enum(node->attributes, &l))) {
						if (strstr(att->name, "fileTemplate")) {
							file_template = att->value;
							nb_media_streams++;
						}
					}
					l=0;
					while ((fdt = gf_list_enum(node->content, &l))) {
						if (fdt->type != GF_XML_NODE_TYPE) continue;
						if (strstr(fdt->name, "File")==NULL) continue;

						u32 n=0;
						char *fdt_location = NULL;
						u32 fdt_toi = 0;
						while ((att = gf_list_enum(fdt->attributes, &n))) {
							if (!strcmp(att->name, "Content-Location")) fdt_location = att->value;
							else if (!strcmp(att->name, "TOI")) {
								if(! gf_strict_atoui(att->value, &fdt_toi)) fdt_toi = 0;
							}
						}
						if (!fdt_toi || !fdt_location) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Corrupted FDT instance : location/TOI missing\n", s->log_name));
							continue;
						}
						gf_list_add(fdt_files, fdt);
					}
				}
			}


			if (!file_template) {
				GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Missing file TOI template in LS/ROUTE session, static content only\n", s->log_name));
			} else {
				sep = strstr(file_template, "$TOI");
				if (sep) sep = strchr(sep+3, '$');

				if (!sep) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong TOI template %s in LS/ROUTE session\n", s->log_name, file_template));
					gf_list_del(fdt_files);
					return GF_NOT_SUPPORTED;
				}
			}
			nb_lct_channels++;

			rlct = NULL;
			for (k=0; k<gf_list_count(rsess->channels); k++) {
				rlct = gf_list_get(rsess->channels, k);
				if (rlct->tsi == tsi) break;
				rlct = NULL;
			}
			if (rlct) {
				gf_list_del_item(remove_channels, rlct);
			} else {
				//OK setup LCT channel for route
				GF_SAFEALLOC(rlct, GF_ROUTELCTChannel);
				if (!rlct) {
					gf_list_del(fdt_files);
					gf_list_del(remove_sessions);
					gf_list_del(remove_channels);
					return GF_OUT_OF_MEM;
				}
				rlct->static_files = gf_list_new();
				rlct->tsi = tsi;
				rlct->is_active = GF_TRUE;
				rsess->nb_active ++;
				gf_list_add(rsess->channels, rlct);
			}
			GF_List *purge_rlct = gf_list_clone(rlct->static_files);
			for (k=0; k<gf_list_count(fdt_files); k++) {
				u32 l;
				GF_XMLNode *fdt = gf_list_get(fdt_files, k);
				u32 toi = atoi( _xml_get_attr(fdt, "TOI") );
				const char *location = _xml_get_attr(fdt, "Content-Location");
				GF_ROUTELCTFile *fdt_file = NULL;
				for (l=0; l<gf_list_count(purge_rlct); l++) {
					fdt_file = gf_list_get(purge_rlct, l);
					if ((fdt_file->toi==toi) && fdt_file->filename && !strcmp(fdt_file->filename, location)) {
						gf_list_rem(purge_rlct, l);
						break;
					}
					fdt_file = NULL;
				}
				if (fdt_file) continue;
				GF_SAFEALLOC(fdt_file,GF_ROUTELCTFile);
				if (!fdt_file) continue;
				fdt_file->filename = gf_strdup(location);
				fdt_file->toi = toi;
				if (strstr(location, ".m3u8")) {
					if (rlct->dash_rep_id) gf_free(rlct->dash_rep_id);
					rlct->dash_rep_id = gf_strdup(location);
				}
				gf_list_add(rlct->static_files, fdt_file);
			}
			gf_list_del(fdt_files);
			//trash all objects pending on files removed
			while (gf_list_count(purge_rlct)) {
				GF_ROUTELCTFile *old_fdt = gf_list_pop_back(purge_rlct);
				//remove from static file list
				gf_list_del_item(rlct->static_files, old_fdt);
				old_fdt->can_remove = GF_TRUE;
			}
			gf_list_del(purge_rlct);

			if (!gf_list_count(rlct->static_files)) {
				GF_ROUTELCTFile *rf;
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Missing init file name in LS/ROUTE session, could be problematic - will consider any TOI %u (-1) present as a ghost init segment\n", s->log_name, (u32)-1));
				//force an init at -1, some streams still have the init not declared but send on TOI -1
				// interpreting it as a regular segment would break clock setup
				GF_SAFEALLOC(rf, GF_ROUTELCTFile)
				rf->toi = (u32) -1;
				gf_list_add(rlct->static_files, rf);
			}


			if (rlct->toi_template) gf_free(rlct->toi_template);
			rlct->toi_template = NULL;
			if (file_template) {
				if (rlct->toi_template) gf_free(rlct->toi_template);
				rlct->toi_template = NULL;
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

			s->nb_media_streams -= rlct->num_components;
			rlct->num_components = nb_media_streams;
			s->nb_media_streams += rlct->num_components;

			//fill in payloads
			rlct->nb_cps = 0;
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
						GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Payload format indicates srcFecPayloadId %d (reserved), assuming 0\n", s->log_name, lreg->src_fec_payload_id));

					}
					if (lreg->format_id != 1) {
						if (lreg->format_id && (lreg->format_id<5)) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Payload formatId %d not supported\n", s->log_name, lreg->format_id));
						} else {
							GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] payload formatId %d reserved, assuming 1\n", s->log_name, lreg->format_id));
						}
					}
					rlct->nb_cps++;
					if (rlct->nb_cps==8) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] More payload formats than supported (8 max)\n", s->log_name));
						break;
					}
				}
				else if (!strcmp(node->name, "ContentInfo") && !rlct->dash_rep_id) {
					const GF_XMLNode *n = _xml_get_child(node, "MediaInfo");
					if (n) {
						const char *rep = _xml_get_attr(n, "repId");
						if (rep) {
							if (rlct->dash_rep_id) gf_free(rlct->dash_rep_id);
							rlct->dash_rep_id = gf_strdup(rep);
						}
					}
				}
			}
		}
	}

	while (gf_list_count(remove_channels)) {
		GF_ROUTELCTChannel *lc = gf_list_pop_back(remove_channels);
		gf_route_lct_removed(routedmx, s, lc);
	}
	gf_list_del(remove_channels);

	while (gf_list_count(remove_sessions)) {
		GF_ROUTESession *rsess = gf_list_pop_back(remove_sessions);
		gf_list_del_item(s->route_sessions, rsess);
		gf_route_route_session_del(routedmx, rsess);
	}
	gf_list_del(remove_sessions);

	if (!nb_lct_channels) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] No supported LCT channels\n", s->log_name));
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
		e = gf_gz_decompress_payload_ex(routedmx->buffer, object->total_length, &routedmx->unz_buffer, &raw_size, GF_TRUE);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to decompress signaling bundle: %s\n", s->log_name, gf_error_to_string(e) ));
			return e;
		}
		if (raw_size > routedmx->unz_buffer_size) routedmx->unz_buffer_size = raw_size;
		payload = routedmx->unz_buffer;
		payload_size = raw_size;
		payload[payload_size] = 0; //gf_gz_decompress_payload_ex adds one extra byte at end
	} else {
		payload = object->payload;
		payload_size = object->total_length;
		payload[payload_size] = 0; //object->payload is allocated with one extra byte
		// Verifying that the payload is not erroneously treated as plaintext
		if(!isprint(payload[0])) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Package appears to be compressed but is being treated as plaintext:\n%s\n", s->log_name, payload));
		}
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_ROUTE, ("[%s] Got TSI 0 config package:\n%s\n", s->log_name, payload ));

	//check for multipart
	if (!strncmp(payload, "Content-Type: multipart/", 24)) {
		sep = strstr(payload, "boundary=\"");
		if (!sep) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Cannot find multipart boundary in package:\n%s\n", s->log_name, payload ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		payload = sep + 10;
		sep = strstr(payload, "\"");
		if (!sep) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Multipart boundary not properly formatted in package:\n%s\n", s->log_name, payload ));
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
		while (payload[0] && strncmp(payload, "\r\n\r\n", 4)) {
			u32 i=0;
			while (payload[0] && strchr("\n\r", payload[0]) != NULL) payload++;
			while (payload[i] && strchr("\r\n", payload[i]) == NULL) i++;

			if (!strnicmp(payload, "Content-Type: ", 14)) {
				u32 copy = MIN(i-14, 100);
				strncpy(szContentType, payload+14, copy);
				szContentType[copy]=0;
			}
			else if (!strnicmp(payload, "Content-Location: ", 18)) {
				u32 copy = MIN(i-18, 1024);
				strncpy(szContentLocation, payload+18, copy);
				szContentLocation[copy]=0;
			} else {
				char tmp = payload[i];
				payload[i] = 0;
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Unrecognized header entity in package:\n%s\n", s->log_name, payload));
				payload[i] = tmp;
			}
			payload += i;
		}
		if(!payload[0]) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] End of package has been prematurely reached\n", s->log_name));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		payload += 4;
		content = boundary ? strstr(payload, "\r\n--") : strstr(payload, "\r\n\r\n");
		if (content) {
			content[0] = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Package type %s location %s content:\n%s\n", s->log_name, szContentType, szContentLocation, payload ));
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] %s not properly formatted in package:\n%s\n", s->log_name, boundary ? "multipart boundary" : "entity", payload ));
			if (sep && boundary) sep[0] = boundary[0];
			gf_free(boundary);
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		if (!strcmp(szContentType, "application/mbms-envelope+xml")) {
			e = gf_route_service_parse_mbms_enveloppe(routedmx, s, payload, szContentLocation, &stsid_version, &mpd_version);

			if (e || !stsid_version || !mpd_version) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] MBMS envelope error %s, S-TSID version %d MPD version %d\n", s->log_name, gf_error_to_string(e), stsid_version, mpd_version ));
				gf_free(boundary);
				return e ? e : GF_SERVICE_ERROR;
			}
		} else if (!strcmp(szContentType, "application/mbms-user-service-description+xml")) {
		} else if (!strcmp(szContentType, "application/dash+xml")
			|| !strcmp(szContentType, "video/vnd.3gpp.mpd")
			|| !strcmp(szContentType, "audio/mpegurl")
			|| !strcmp(szContentType, "video/mpegurl")
			|| !strcmp(szContentType, "application/vnd.apple.mpegURL")
		) {
			if (!s->mpd_version || (mpd_version && (mpd_version+1 != s->mpd_version))) {
				u32 ftype = GF_FLUTE_HLS_MANIFEST;
				if (strstr(szContentType, "dash") || strstr(szContentType, "mpd")) ftype = GF_FLUTE_DASH_MANIFEST;
				s->mpd_version = mpd_version+1;
				gf_route_service_setup_dash(routedmx, s, payload, szContentLocation, ftype);
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Same MPD version, ignoring\n", s->log_name));
			}
		}
		//Korean and US version have different mime types
		else if (!strcmp(szContentType, "application/s-tsid") || !strcmp(szContentType, "application/route-s-tsid+xml")) {
			if (!s->stsid_version || (stsid_version && (stsid_version+1 != s->stsid_version))) {
				s->stsid_version = stsid_version+1;
				e = gf_route_service_setup_stsid(routedmx, s, payload, szContentLocation);
				if (e) {
					gf_free(boundary);
					return e;
				}
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Same S-TSID version, ignoring\n", s->log_name));
			}
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Unsupported content type (%s), parsing payload is skipped\n", s->log_name, szContentType));
		}
		if (!sep) break;
		sep[0] = boundary[0];
		payload = sep;
	}

	gf_free(boundary);
	payload_size = (u32) strlen(payload);
	if(payload_size > 1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Unable to process %d remaining characters in the payload due to data corruption\n", s->log_name, payload_size));
		return GF_CORRUPTED_DATA;
	} else {
		GF_ROUTESession *rsess;
		u32 i=0;
		u32 nb_channels=0;
		while ((rsess = gf_list_enum(s->route_sessions, &i))) {
			nb_channels += gf_list_count(rsess->channels);
		}
		if(nb_channels == 0) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] No session found, dropping manifest\n", s->log_name));
			return GF_INVALID_CONFIGURATION;
		}
		return GF_OK;
	}
}


#define GF_ROUTE_MAX_SIZE 0x40000000

static GF_Err dmx_process_service_route(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_ROUTESession *route_sess)
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
	gf_assert(nb_read);

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
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header version %d, expecting 1\n", s->log_name, v));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (C!=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header C %d, expecting 0\n", s->log_name, C));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if ((psi!=0) && (psi!=2) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header PSI %d, expecting b00 or b10\n", s->log_name, psi));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (S!=1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header S, should be 1\n", s->log_name));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (O!=1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header O, should be b01\n", s->log_name));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (H!=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header H, should be 0\n", s->log_name));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (hdr_len<4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header len %d, should be at least 4\n", s->log_name, hdr_len));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (psi==0) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] FEC not implemented\n", s->log_name));
		return GF_OK;
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
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] No session with TSI %u defined, skipping packet (TOI %u)\n", s->log_name, tsi, toi));
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
				GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Unsupported code point %d, skipping packet (TOI %u)\n", s->log_name, cp, toi));
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
		if (!a_S && !a_M) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] SLT bundle without MPD or S-TSID, skipping packet\n", s->log_name));
			return GF_OK;
		}
	}

	//parse extensions
	while (hdr_len) {
		u32 h_pos = gf_bs_get_position(routedmx->bs);
		u8 het = gf_bs_read_u8(routedmx->bs);
		u8 hel =0 ;

		if (het<=127) hel = gf_bs_read_u8(routedmx->bs);
		else hel=1;

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
			if(! tol_size) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Wrong TOL=%u value \n", s->log_name, tol_size));
			}
			break;

		case GF_LCT_EXT_TOL48:
			if (hel!=2) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Wrong HEL %d for TOL48 LCT extension, expecting 2\n", s->log_name, hel));
				continue;
			}
			tol_size = gf_bs_read_long_int(routedmx->bs, 48);
			if(! tol_size) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Wrong TOL=%u value \n", s->log_name, tol_size));
			}
			break;

		default:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Unsupported header extension HEL %d HET %d, ignoring\n", s->log_name, hel, het));
			break;
		}
		if (hdr_len<hel) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Wrong HEL %d for LCT extension %d, remaining header size %d\n", s->log_name, hel, het, hdr_len));
			continue;
		}
		h_pos = gf_bs_get_position(routedmx->bs) - h_pos;
		while (hel*4 > h_pos) {
			h_pos++;
			gf_bs_read_u8(routedmx->bs);
		}
		if (hel) hdr_len -= hel;
		else hdr_len -= 1;
	}

	start_offset = gf_bs_read_u32(routedmx->bs);
	if (start_offset>=GF_ROUTE_MAX_SIZE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Invalid start offset %u\n", s->log_name, start_offset));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (tol_size>=GF_ROUTE_MAX_SIZE) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Invalid object size %u\n", s->log_name, tol_size));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	pos = (u32) gf_bs_get_position(routedmx->bs);

	e = gf_route_service_gather_object(routedmx, s, tsi, toi, start_offset, routedmx->buffer + pos, nb_read-pos, (u32) tol_size, B, in_order, rlct, &gather_object, -1, 0);

	if (e==GF_EOS) {
		//in case we were pushed a NULL object
		if (!gather_object->nb_frags) {
			gf_route_obj_to_reservoir(routedmx, s, gather_object);
		}
		else if (!tsi) {
			if (gather_object->status==GF_LCT_OBJ_DONE_ERR) {
				s->last_dispatched_toi_on_tsi_zero=0;
				gf_route_obj_to_reservoir(routedmx, s, gather_object);
				return GF_OK;
			}
			//we don't assign version here, we use mbms envelope for that since the bundle may have several
			//packages

			e = gf_route_dmx_process_service_signaling(routedmx, s, gather_object, cc, a_S ? v : 0, a_M ? v : 0);
			//we don't release the LCT object, so that we can discard future versions
			if(e) {
				//ignore this object in order to be able to accept future versions
				s->last_dispatched_toi_on_tsi_zero=0;
				s->stsid_version = 0;
				s->stsid_crc = 0;
				s->mpd_version = 0;
				gf_route_obj_to_reservoir(routedmx, s, gather_object);
			}
		} else {
			gf_route_dmx_process_object(routedmx, s, gather_object);
		}
	}

	return GF_OK;
}

static GF_Err dmx_process_service_dvb_flute(GF_ROUTEDmx *routedmx, GF_ROUTEService *s, GF_ROUTESession *route_sess)
{
	GF_Err e;
	u32 fdt_symbol_length=0;
	u32 nb_read, cp , v, C, psi, S, O, H, /*Res, A,*/ B, hdr_len, tsi, toi, pos;
	u64 transfert_length=0;
	u32 start_offset=0;
	GF_ROUTELCTChannel *rlct=NULL;
	GF_LCTObject *gather_object=NULL;
	u32 /*SBN,*/ESI; //Source Block Length  | Encoding Symbol  

	if (route_sess) {
		e = gf_sk_receive_no_select(route_sess->sock, routedmx->buffer, routedmx->buffer_size, &nb_read);
	} else {
		e = gf_sk_receive_no_select(s->sock, routedmx->buffer, routedmx->buffer_size, &nb_read);
	}

	if (e != GF_OK) return e;
	gf_assert(nb_read);

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
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header version %d, expecting 1\n", s->log_name, v));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (C!=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header C %d, expecting 0\n", s->log_name, C));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if ((psi!=0) && (psi!=2) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header PSI %d, expecting b00 or b10\n", s->log_name, psi));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (cp>1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header CP %d but only Compact No-Code FEC and raptor FEC are allowed\n", s->log_name, cp));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (S && H) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header S, only 16 and 32 bits supported\n", s->log_name));
		return GF_NOT_SUPPORTED;
	}
	else if ((O>1) || (O && H)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header O, only 16 and 32 bits supported\n", s->log_name));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (hdr_len < (H ? 3 : 4)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Wrong LCT header len %d, should be at least %u\n", s->log_name, hdr_len, (H ? 3 : 4)));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	/*cc = */gf_bs_read_u32(routedmx->bs);
	if (H) {
		tsi = gf_bs_read_u16(routedmx->bs);
		toi = gf_bs_read_u16(routedmx->bs);
		hdr_len -= 3;
	} else {
		tsi = gf_bs_read_u32(routedmx->bs);
		toi = gf_bs_read_u32(routedmx->bs);
		hdr_len -= 4;
	}

	//parse extensions
	while (hdr_len) {
		u32 h_pos = gf_bs_get_position(routedmx->bs);
		u8 het = gf_bs_read_u8(routedmx->bs);
		u8 hel =0 ;

		if (het<=127) hel = gf_bs_read_u8(routedmx->bs);
		else hel=1;

		switch (het) {
		case GF_LCT_EXT_FDT:
			/*u8 flute_version = */gf_bs_read_int(routedmx->bs, 4); // TODO: add version verification, if different than 1
			/*u16 fdt_instance_id = */gf_bs_read_int(routedmx->bs, 20);
			break;

		case GF_LCT_EXT_FTI:
		{
			transfert_length = gf_bs_read_int(routedmx->bs, 48);
			/*u16 Fec_instance_ID = */gf_bs_read_int(routedmx->bs, 16);
			fdt_symbol_length = gf_bs_read_int(routedmx->bs, 16);
            /*u32 Maximum_source_block_length = */gf_bs_read_int(routedmx->bs, 32);
		}
			break;

		default:
			GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[%s] Unsupported header extension HEL %d HET %d, ignoring\n", s->log_name, hel, het));
			break;
		}
		if (hdr_len<hel) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Wrong HEL %d for LCT extension %d, remaining header size %d\n", s->log_name, hel, het, hdr_len));
			continue;
		}
		h_pos = gf_bs_get_position(routedmx->bs) - h_pos;
		while (hel*4 > h_pos) {
			h_pos++;
			gf_bs_read_u8(routedmx->bs);
		}
		if (hel) hdr_len -= hel;
		else hdr_len -= 1;
	}

	//both no-code and raptor use 16 bits for each SBN and ESI
	/*SBN =(u32) */gf_bs_read_u16(routedmx->bs);
	ESI = (u32) gf_bs_read_u16(routedmx->bs);
	pos = (u32) gf_bs_get_position(routedmx->bs);

	if (s->last_active_obj
		&& (s->last_active_obj->tsi==tsi)
		//watchout for manifest and init segment objects which set RLCT to point to the segment delivery service
		&& s->last_active_obj->rlct
		&& (s->last_active_obj->rlct->tsi==tsi)
	) {
		rlct = s->last_active_obj->rlct;
	} else {
		Bool in_session=GF_FALSE;
		GF_ROUTESession *rsess;
		u32 i=0;
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

	e = gf_route_service_gather_object(routedmx, s, tsi, toi, start_offset, routedmx->buffer + pos, nb_read-pos, (u32) transfert_length, B, GF_FALSE, rlct, &gather_object, ESI, fdt_symbol_length);

	start_offset += (nb_read ) * ESI; 
	
	if (e==GF_EOS) {
		gf_route_dmx_process_object(routedmx, s, gather_object);
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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ATSC] LLSTable size %d version %d: ID %d (%s) - group ID %d - group count %d\n", read, lls_table_id, lls_table_version, (lls_table_id==1) ? "SLT" : (lls_table_id==2) ? "RRT" : (lls_table_id==3) ? "SystemTime" : (lls_table_id==4) ? "AEAT" : "Reserved", lls_group_id, lls_group_count));
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

	e = gf_gz_decompress_payload_ex(&routedmx->buffer[4], read-4, &routedmx->unz_buffer, &raw_size, GF_TRUE);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ATSC] Failed to decompress %s table: %s\n", name, gf_error_to_string(e) ));
		return e;
	}
	//realloc happened
	if (routedmx->unz_buffer_size<raw_size) routedmx->unz_buffer_size = raw_size;
	routedmx->unz_buffer[raw_size]=0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_ROUTE, ("[ATSC] %s table - payload:\n%s\n", name, routedmx->unz_buffer));


	e = gf_xml_dom_parse_string(routedmx->dom, routedmx->unz_buffer);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ATSC] Failed to parse SLT XML: %s - %s\n", gf_error_to_string(e), gf_xml_dom_get_error(routedmx->dom) ));
		return e;
	}

	root = gf_xml_dom_get_root(routedmx->dom);
	if (!root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[ATSC] Failed to get XML root for %s table\n", name ));
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
	u32 i, j, count, nb_obj=0;
	GF_Err e;

	//check all active sockets
	e = gf_sk_group_select(routedmx->active_sockets, 10, GF_SK_SELECT_READ);
	if (e) {
		//this only happens when netcap is used, flush all pending objects
		if (e==GF_EOS) {
			count = gf_list_count(routedmx->services);
			for (i=0; i<count; i++) {
				GF_ROUTEService *s = (GF_ROUTEService *)gf_list_get(routedmx->services, i);
				j=0;
				GF_LCTObject *obj;
				while ((obj=gf_list_enum(s->objects, &j))) {
					if (obj->status==GF_LCT_OBJ_RECEPTION) {
						obj->status = GF_LCT_OBJ_DONE_ERR;
						if (s->tune_mode!=GF_ROUTE_TUNE_OFF)
							gf_route_dmx_process_object(routedmx, s, obj);
					}
					obj->blob.flags &= ~GF_BLOB_IN_TRANSFER;
				}
			}
		}
		return e;
	}
	if (routedmx->atsc_sock) {
		if (gf_sk_group_sock_is_set(routedmx->active_sockets, routedmx->atsc_sock, GF_SK_SELECT_READ)) {
			e = gf_route_dmx_process_lls(routedmx);
			if (e) return e;
		}
	}

	count = gf_list_count(routedmx->services);
	for (i=0; i<count; i++) {
		u32 nb_obj_service;
		GF_ROUTESession *rsess;
		GF_ROUTEService *s = (GF_ROUTEService *)gf_list_get(routedmx->services, i);
		if (s->tune_mode==GF_ROUTE_TUNE_OFF) continue;
		nb_obj_service = gf_list_count(s->objects);
		if (s->nb_media_streams) nb_obj_service /= s->nb_media_streams;
		//except for flute
		if (s->service_id) {
			if (nb_obj<nb_obj_service) nb_obj = nb_obj_service;
			if (routedmx->nrt_max_seg && (nb_obj_service > routedmx->nrt_max_seg))
				continue;
		}
		if (gf_sk_group_sock_is_set(routedmx->active_sockets, s->sock, GF_SK_SELECT_READ)) {
			e = s->process_service(routedmx, s, NULL);
			if (e) return e;
		}
		if (s->tune_mode!=GF_ROUTE_TUNE_ON) continue;

		if (!s->secondary_sockets) continue;

		j=0;
		while ((rsess = (GF_ROUTESession *)gf_list_enum(s->route_sessions, &j) )) {
			if (gf_sk_group_sock_is_set(routedmx->active_sockets, rsess->sock, GF_SK_SELECT_READ)) {
				e = s->process_service(routedmx, s, rsess);
				if (e) return e;
			}
		}
	}
	if (routedmx->nrt_max_seg && (nb_obj>routedmx->nrt_max_seg))
		return GF_IP_NETWORK_EMPTY;
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
		u32 nb_obj = gf_list_count(s->objects);
		if (s->nb_media_streams) nb_obj /= s->nb_media_streams;
		return nb_obj;
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
		if (obj->rlct && obj->rlct->toi_template && (sscanf(fileName, obj->rlct->toi_template, &toi) == 1)) {
			u32 tsi;
			if (toi == obj->toi) {
				u32 obj_start_time;
				//GF_ROUTELCTChannel *rlct = obj->rlct;

				if (!is_remove) {
					obj->force_keep = 1;
					return GF_OK;
				}
				obj->force_keep = 0;

				//we likely have a loop here
				if (obj == s->last_active_obj) break;
				//obj being received do not destroy
				if (obj->status == GF_LCT_OBJ_RECEPTION) break;

				obj_start_time = obj->start_time_ms;
				tsi = obj->tsi;
				gf_route_obj_to_reservoir(routedmx, s, obj);
				if (purge_previous) {
					i=0;
					while ((obj = gf_list_enum(s->objects, &i))) {
						//static file (ROUTE) or file still advertized in FDT (FLUTE)
						if (obj->rlct_file && !obj->rlct_file->can_remove) continue;
						if (obj->status <= GF_LCT_OBJ_RECEPTION) continue;

						//crude hack as we currently don't know which media si playing so we need to purge all other ones...
						//- don't check LCT channel
						//- if not same same tsi, prune if received a few segments ago
						//if (obj->rlct != rlct) continue;
						if (obj->tsi != tsi) {
							if (obj->start_time_ms + 2*obj->download_time_ms >= obj_start_time) {
								continue;
							}
						}
						//do NOT purge based on TOI, won't work for flute
						if (obj->start_time_ms+obj->download_time_ms < obj_start_time) {
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
		else if (obj->rlct_file && obj->rlct_file->filename && !strcmp(fileName, obj->rlct_file->filename)) {
			if (!is_remove) {
				obj->force_keep = 1;
			} else if (!obj->rlct_file->fdt_tsi || obj->rlct_file->can_remove) {
				gf_route_obj_to_reservoir(routedmx, s, obj);
			}
			return GF_OK;
		}
	}
	//we are flute, check root service
	if (routedmx->dvb_mabr && service_id) {
		return gf_route_dmx_keep_or_remove_object_by_name(routedmx, 0, fileName, purge_previous, is_remove);
	}
	if (is_remove) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_ROUTE, ("[%s] Failed to remove object %s from service, object not found\n", s->log_name, fileName));
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
		if (obj->rlct_file && !obj->rlct_file->can_remove)
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
		if (obj->rlct_file && !obj->rlct_file->can_remove) continue;
		//obj being received do not destroy
		if (obj->status <= GF_LCT_OBJ_RECEPTION) continue;
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

GF_EXPORT
GF_Err gf_route_dmx_patch_frag_info(GF_ROUTEDmx *routedmx, u32 service_id, GF_ROUTEEventFileInfo *finfo, u32 br_start, u32 br_end)
{
	u32 i=0;
	Bool is_patched=GF_FALSE;
	if (!routedmx) return GF_BAD_PARAM;
	GF_ROUTEService *s=NULL;
	GF_LCTObject *obj = NULL;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id == service_id) break;
		s = NULL;
	}
	if (!s) return GF_BAD_PARAM;
	i=0;
	while ((obj = gf_list_enum(s->objects, &i))) {
		if ((obj->tsi == finfo->tsi) && (obj->toi == finfo->toi))
			break;
	}
	if (!obj) return GF_BAD_PARAM;
	gf_mx_p(obj->blob.mx);
	if (!br_start && (br_end==obj->total_length)) {
		obj->nb_frags = 1;
		obj->frags[0].offset = 0;
		obj->frags[0].size = obj->total_length;
		finfo->nb_frags = obj->nb_frags;
		finfo->frags = obj->frags;
		gf_mx_v(obj->blob.mx);
		return GF_OK;
	}

	for (i=0; i<obj->nb_frags; i++) {
		if (br_start < obj->frags[i].offset) {
			//we patched until begining of this fragment, merge
			if (br_end >= obj->frags[i].offset) {
				u32 last_end = i ? (obj->frags[i-1].offset+obj->frags[i-1].size) : 0;
				obj->frags[i].offset = (last_end > br_start) ? last_end : br_start;
				is_patched = GF_TRUE;
				break;
			}
			//we need a new fragment
			if (obj->nb_frags+1>obj->nb_alloc_frags) {
				obj->nb_alloc_frags = obj->nb_frags+1;
				obj->frags = gf_realloc(obj->frags, sizeof(GF_LCTFragInfo)*obj->nb_alloc_frags);
				if (!obj->frags) {
					finfo->nb_frags = obj->nb_frags = 0;
					finfo->frags = obj->frags;
					gf_mx_v(obj->blob.mx);
					return GF_OUT_OF_MEM;
				}
			}
			memmove(&obj->frags[i+1], &obj->frags[i], sizeof(GF_LCTFragInfo) * (obj->nb_frags - i));
			obj->frags[i].offset = br_start;
			obj->frags[i].size = br_end - br_start;
			obj->nb_frags++;
			is_patched = GF_TRUE;
			break;
		}
	}
	if (!is_patched) {
		if (obj->nb_frags+1>obj->nb_alloc_frags) {
			obj->nb_alloc_frags = obj->nb_frags+1;
			obj->frags = gf_realloc(obj->frags, sizeof(GF_LCTFragInfo)*obj->nb_alloc_frags);
			if (!obj->frags) {
				finfo->nb_frags = obj->nb_frags = 0;
				finfo->frags = obj->frags;
				gf_mx_v(obj->blob.mx);
				return GF_OUT_OF_MEM;
			}
		}
		obj->frags[obj->nb_frags].offset = br_start;
		obj->frags[obj->nb_frags].size = br_end - br_start;
		obj->nb_frags++;
	}
	for (i=1; i<obj->nb_frags; i++) {
		if (obj->frags[i-1].offset+obj->frags[i-1].size == obj->frags[i].offset) {
			obj->frags[i-1].size += obj->frags[i].size;
			if (i+1<obj->nb_frags) {
				memmove(&obj->frags[i], &obj->frags[i+1], sizeof(GF_LCTFragInfo) * (obj->nb_frags - i - 1));
			}
			obj->nb_frags--;
		}
	}
	finfo->nb_frags = obj->nb_frags;
	finfo->frags = obj->frags;

	gf_mx_v(obj->blob.mx);
	return GF_OK;
}

GF_Err gf_route_dmx_mark_active_quality(GF_ROUTEDmx *routedmx, u32 service_id, const char *period_id, s32 as_id, const char *rep_id, Bool is_selected)
{
	u32 count, i=0;
	if (!routedmx || !rep_id) return GF_BAD_PARAM;
	GF_ROUTEService *s=NULL;
	while ((s = gf_list_enum(routedmx->services, &i))) {
		if (s->service_id == service_id) break;
		s = NULL;
	}
	if (!s) return GF_BAD_PARAM;

	GF_ROUTESession *mcast_sess=NULL;
	GF_ROUTELCTChannel *rlct=NULL;
	count = gf_list_count(s->route_sessions);
	for (i=0; i<count; i++) {
		GF_ROUTESession *rsess = gf_list_get(s->route_sessions, i);
		u32 j, nb_chan = gf_list_count(rsess->channels);
		for (j=0; j<nb_chan; j++) {
			rlct = gf_list_get(rsess->channels, j);
			//if periodID is set, amke sure they match
			if (period_id && rlct->dash_period_id && strcmp(period_id, rlct->dash_period_id))
				continue;
			if (rlct->dash_as_id && (rlct->dash_as_id!=as_id))
				continue;

			if (rep_id && rlct->dash_rep_id && !strcmp(rep_id, rlct->dash_rep_id)) {
				mcast_sess = rsess;
				break;
			}
		}
		if (mcast_sess) break;
	}
	if (!mcast_sess) return GF_OK;

	GF_Socket **sock = mcast_sess->mcast_addr ? &mcast_sess->sock : &s->sock;
	u32 *nb_active = mcast_sess->mcast_addr ? &mcast_sess->nb_active : &s->nb_active;

	if (is_selected) {
		if (rlct->is_active) return GF_OK;

		rlct->is_active = GF_TRUE;
		if (!mcast_sess->nb_active && !(*sock)) {
			*sock = gf_sk_new_ex(GF_SOCK_TYPE_UDP, routedmx->netcap_id);

			gf_sk_set_usec_wait(*sock, 1);
			const char *dst_add = mcast_sess->mcast_addr ? mcast_sess->mcast_addr : s->dst_ip;
			u32 dst_port = mcast_sess->mcast_addr ? mcast_sess->mcast_port : s->port;
			GF_Err e = gf_sk_setup_multicast(*sock, dst_add, dst_port, 0, GF_FALSE, (char *) routedmx->ip_ifce);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_ROUTE, ("[%s] Failed to resetup multicast for route session on %s:%d\n", s->log_name, dst_add, dst_port));
				return e;
			}
			gf_sk_set_buffer_size(*sock, GF_FALSE, routedmx->unz_buffer_size);
			gf_sk_group_register(routedmx->active_sockets, *sock);
			if (mcast_sess->mcast_addr)
				s->secondary_sockets++;
		}

		(*nb_active) ++;
	} else {
		if (!rlct->is_active) return GF_OK;
		rlct->is_active = 0;
		if (s->last_active_obj && (s->last_active_obj->rlct==rlct))
			s->last_active_obj = NULL;

		(*nb_active) --;
		//we cannot deactivate service socket in ROUTE, we need to get MPD and STSID updates
		//for mabr (flute or route) we can
		if (! (*nb_active) && (mcast_sess->mcast_addr || routedmx->dvb_mabr) ) {
			gf_sk_group_unregister(routedmx->active_sockets, *sock);
			gf_sk_del(*sock);
			*sock = NULL;
			if (mcast_sess->mcast_addr)
				s->secondary_sockets--;
		}
	}
	return GF_OK;
}

#endif /* !GPAC_DISABLE_ROUTE */
