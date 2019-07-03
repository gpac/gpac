/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools ATSC demux sub-project
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

#include <gpac/atsc.h>

#ifndef GPAC_DISABLE_ATSC

#include <gpac/network.h>
#include <gpac/bitstream.h>
#include <gpac/xml.h>

#define GF_ATSC_MCAST_ADDR	"224.0.23.60"
#define GF_ATSC_MCAST_PORT	4937
#define GF_ATSC_SOCK_SIZE	0x80000

typedef struct
{
	u8 codepoint;
	u8 format_id;
	u8 frag;
	u8 order;
	u32 src_fec_payload_id;
} GF_ATSCLCTReg;

typedef struct
{
	u32 tsi;
	char *init_filename;
	u32 init_toi;
	char *toi_template;
	GF_ATSCLCTReg CPs[8];
	u32 nb_cps;
	u32 last_dispatched_tsi, last_dispatched_toi;

} GF_ATSCLCTChannel;


typedef struct
{
	u32 offset;
	u32 size;
} GF_LCTFragInfo;

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
	Bool closed_flag;
	u32 download_time_ms;

	GF_ATSCLCTChannel *rlct;

	u32 prev_start_offset;
} GF_LCTObject;



typedef struct
{
	GF_Socket *sock;

	GF_List *channels;
} GF_ATSCRouteSession;

typedef enum
{
	GF_ATSC_TUNE_OFF=0,
	GF_ATSC_TUNE_ON,
	GF_ATSC_TUNE_SLS_ONLY,
} GF_ATSCTuneMode;

typedef struct
{
	u32 service_id;
	u32 protocol;
	u32 mpd_version, stsid_version;
	GF_Socket *sock;
	u32 secondary_sockets;
	GF_List *objects;
	GF_LCTObject *last_active_obj;
	char *output_dir;
	u32 port;
	char *dst_ip;
	u32 last_dispatched_toi_on_tsi_zero;
	u32 stsid_crc;

	GF_List *route_sessions;
	GF_ATSCTuneMode tune_mode;
	void *udta;
} GF_ATSCService;

struct __gf_atscdmx {
	const char *ip_ifce;
	const char *base_dir;
	GF_Socket *sock;
	u8 *buffer;
	u32 buffer_size;
	u8 *unz_buffer;
	u32 unz_buffer_size;

	u32 max_seg_store;

	u32 slt_version, rrt_version, systime_version, aeat_version;
	GF_List *services;

	GF_List *object_reservoir;
	GF_BitStream *bs;

	GF_DOMParser *dom;
	u32 service_autotune;
	Bool tune_all_sls;

	GF_SockGroup *active_sockets;


	void (*on_event)(void *udta, GF_ATSCEventType evt, u32 evt_param, GF_ATSCEventFileInfo *info);
	void *udta;

	u32 debug_tsi;

	u64 nb_packets;
	u64 total_bytes_recv;
	u64 first_pck_time, last_pck_time;
};

static void gf_atsc3_route_session_del(GF_ATSCRouteSession *rs)
{
	if (rs->sock) gf_sk_del(rs->sock);
	while (gf_list_count(rs->channels)) {
		GF_ATSCLCTChannel *lc = gf_list_pop_back(rs->channels);
		if (lc->init_filename) gf_free(lc->init_filename);
		gf_free(lc->toi_template);
		gf_free(lc);
	}
	gf_list_del(rs->channels);
	gf_free(rs);
}

static void gf_atsc3_lct_obj_del(GF_LCTObject *o)
{
	if (o->frags) gf_free(o->frags);
	if (o->payload) gf_free(o->payload);
	gf_free(o);
}

static void gf_atsc3_service_del(GF_ATSCDmx *atscd, GF_ATSCService *s)
{
	if (s->sock) {
		gf_sk_group_unregister(atscd->active_sockets, s->sock);
		gf_sk_del(s->sock);
	}
	if (s->output_dir) gf_free(s->output_dir);
	while (gf_list_count(s->route_sessions)) {
		GF_ATSCRouteSession *rsess = gf_list_pop_back(s->route_sessions);
		gf_atsc3_route_session_del(rsess);
	}
	gf_list_del(s->route_sessions);

	while (gf_list_count(s->objects)) {
		GF_LCTObject *o = gf_list_pop_back(s->objects);
		gf_atsc3_lct_obj_del(o);
	}
	gf_list_del(s->objects);
	if (s->dst_ip) gf_free(s->dst_ip);
	gf_free(s);
}

GF_EXPORT
void gf_atsc3_dmx_del(GF_ATSCDmx *atscd)
{
	if (atscd->buffer) gf_free(atscd->buffer);
	if (atscd->unz_buffer) gf_free(atscd->unz_buffer);
	if (atscd->sock) gf_sk_del(atscd->sock);
	if (atscd->dom) gf_xml_dom_del(atscd->dom);
	if (atscd->services) {
		while (gf_list_count(atscd->services)) {
			GF_ATSCService *s = gf_list_pop_back(atscd->services);
			gf_atsc3_service_del(atscd, s);
		}
		gf_list_del(atscd->services);
	}
	if (atscd->active_sockets) gf_sk_group_del(atscd->active_sockets);
	if (atscd->object_reservoir) {
		while (gf_list_count(atscd->object_reservoir)) {
			GF_LCTObject *obj = gf_list_pop_back(atscd->object_reservoir);
			gf_atsc3_lct_obj_del(obj);
		}
		gf_list_del(atscd->object_reservoir);
	}
	if (atscd->bs) gf_bs_del(atscd->bs);
	gf_free(atscd);
}

GF_EXPORT
GF_ATSCDmx *gf_atsc3_dmx_new(const char *ifce, const char *dir, u32 sock_buffer_size)
{
	GF_ATSCDmx *atscd;
	GF_Err e;
	GF_SAFEALLOC(atscd, GF_ATSCDmx);
	if (!atscd) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ATSC] Failed to allocate ATSC demuxer\n"));
		return NULL;
	}
	atscd->ip_ifce = ifce;
	atscd->base_dir = dir;
	if (dir && !gf_dir_exists(dir)) {
		e = gf_mkdir(dir);
		if (e == GF_IO_ERR) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to create output dir %s - using memory mode\n", dir));
			atscd->base_dir = NULL;
		}
	}
	atscd->dom = gf_xml_dom_new();
	if (!atscd->dom) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to allocate DOM parser\n" ));
		gf_atsc3_dmx_del(atscd);
		return NULL;
	}
	atscd->services = gf_list_new();
	if (!atscd->services) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to allocate ATSC service list\n" ));
		gf_atsc3_dmx_del(atscd);
		return NULL;
	}
	atscd->object_reservoir = gf_list_new();
	if (!atscd->object_reservoir) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to allocate ATSC object reservoir\n" ));
		gf_atsc3_dmx_del(atscd);
		return NULL;
	}

	if (!sock_buffer_size) sock_buffer_size = GF_ATSC_SOCK_SIZE;
	atscd->unz_buffer_size = sock_buffer_size;
	//we store one UDP packet, or realloc to store LLS signaling so starting with 10k should be enough in most cases
	atscd->buffer_size = 10000;
	atscd->buffer = gf_malloc(atscd->buffer_size);
	if (!atscd->buffer) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ATSC] Failed to allocate socket buffer\n"));
		gf_atsc3_dmx_del(atscd);
		return NULL;
	}
	atscd->unz_buffer = gf_malloc(atscd->unz_buffer_size);
	if (!atscd->unz_buffer) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[ATSC] Failed to allocate socket buffer\n"));
		gf_atsc3_dmx_del(atscd);
		return NULL;
	}

	atscd->active_sockets = gf_sk_group_new();
	if (!atscd->active_sockets) {
		gf_atsc3_dmx_del(atscd);
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[ATSC] Failed to create socket group\n"));
		return NULL;
	}

	atscd->sock = gf_sk_new(GF_SOCK_TYPE_UDP);
	if (!atscd->sock) {
		gf_atsc3_dmx_del(atscd);
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[ATSC] Failed to create UDP socket\n"));
		return NULL;
	}
	gf_sk_group_register(atscd->active_sockets, atscd->sock);

	gf_sk_set_usec_wait(atscd->sock, 1);
	e = gf_sk_setup_multicast(atscd->sock, GF_ATSC_MCAST_ADDR, GF_ATSC_MCAST_PORT, 1, GF_FALSE, (char *) ifce);
	if (e) {
		gf_atsc3_dmx_del(atscd);
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[ATSC] Failed to bind to multicast address on interface %s\n", ifce ? ifce : "default"));
		return NULL;
	}
	gf_sk_set_buffer_size(atscd->sock, GF_FALSE, sock_buffer_size);
	//gf_sk_set_block_mode(atscd->sock, GF_TRUE);
	//create static bs
	atscd->bs = gf_bs_new((char*)&e, 1, GF_BITSTREAM_READ);

	return atscd;
}

static void gf_atsc3_register_service_sockets(GF_ATSCDmx *atscd, GF_ATSCService *s, Bool do_register)
{
	u32 i;
	GF_ATSCRouteSession *rsess;
	if (do_register) gf_sk_group_register(atscd->active_sockets, s->sock);
	else gf_sk_group_unregister(atscd->active_sockets, s->sock);

	if (!s->secondary_sockets) return;

	i=0;
	while ((rsess = gf_list_enum(s->route_sessions, &i))) {
		if (! rsess->sock) continue;
		if (do_register) gf_sk_group_register(atscd->active_sockets, rsess->sock);
		else gf_sk_group_unregister(atscd->active_sockets, rsess->sock);
	}
}

GF_EXPORT
GF_Err gf_atsc3_tune_in(GF_ATSCDmx *atscd, u32 serviceID, Bool tune_all_sls)
{
	u32 i;
	GF_ATSCService *s;
	if (!atscd) return GF_BAD_PARAM;
	atscd->service_autotune = serviceID;
	atscd->tune_all_sls = tune_all_sls;
	i=0;
	while ((s = gf_list_enum(atscd->services, &i))) {
		GF_ATSCTuneMode prev_mode = s->tune_mode;
		if (s->service_id==serviceID) s->tune_mode = GF_ATSC_TUNE_ON;
		else if (serviceID==0xFFFFFFFF) s->tune_mode = GF_ATSC_TUNE_ON;
		else if ((s->tune_mode!=GF_ATSC_TUNE_ON) && (serviceID==0xFFFFFFFE)) {
			s->tune_mode = GF_ATSC_TUNE_ON;
			serviceID = s->service_id;
		} else {
			s->tune_mode = tune_all_sls ? GF_ATSC_TUNE_SLS_ONLY : GF_ATSC_TUNE_OFF;
		}
		//we were previously not tuned
		if (prev_mode == GF_ATSC_TUNE_OFF) {
			//we are now tuned, register sockets
			if (s->tune_mode != GF_ATSC_TUNE_OFF) {
				gf_atsc3_register_service_sockets(atscd, s, GF_TRUE);
			}
		}
		//we were previously tuned
		else {
			//we are now not tuned, unregister sockets
			if (s->tune_mode == GF_ATSC_TUNE_OFF) {
				gf_atsc3_register_service_sockets(atscd, s, GF_FALSE);
			}
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_atsc3_set_max_objects_store(GF_ATSCDmx *atscd, u32 max_segs)
{
	if (!atscd) return GF_BAD_PARAM;
	atscd->max_seg_store = max_segs;
	return GF_OK;
}

static GF_Err gf_atsc3_dmx_process_slt(GF_ATSCDmx *atscd, GF_XMLNode *root)
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
			GF_Err e;
			GF_ATSCService *service;
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] No service destination IP or port found for service %d - ignoring service\n", service_id));
				continue;
			}
			if ((protocol!=1) && (protocol!=2)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Unknown ATSC service signaling protocol %d for service %d - ignoring service\n", service_id, protocol));
				continue;
			}

			//todo - remove existing service ?

			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[ATSC] Found service %d destination IP %s port %d\n", service_id, dst_ip, dst_port));

			GF_SAFEALLOC(service, GF_ATSCService);
			if (!service) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to allocate service %d\n", service_id));
				continue;
			}
			service->service_id = service_id;
			service->protocol = protocol;

			service->sock = gf_sk_new(GF_SOCK_TYPE_UDP);
			gf_sk_set_usec_wait(service->sock, 1);
			e = gf_sk_setup_multicast(service->sock, dst_ip, dst_port, 0, GF_FALSE, (char*) atscd->ip_ifce);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to setup multicast on %s:%d for service %d\n", dst_ip, dst_port, service_id));
				gf_atsc3_service_del(atscd, service);
				continue;
			}
			gf_sk_set_buffer_size(service->sock, GF_FALSE, atscd->unz_buffer_size);
			//gf_sk_set_block_mode(service->sock, GF_TRUE);

			service->dst_ip = gf_strdup(dst_ip);
			service->port = dst_port;
			service->objects = gf_list_new();
			service->route_sessions = gf_list_new();

			if (atscd->base_dir) {
				u32 len = (u32) strlen(atscd->base_dir);
				service->output_dir = gf_malloc(sizeof(char) * (len + 20) );
				if ((atscd->base_dir[len-1]=='/') || (atscd->base_dir[len-1]=='\\')) {
					sprintf(service->output_dir, "%sservice%d", atscd->base_dir, service_id);
				} else {
					sprintf(service->output_dir, "%s/service%d", atscd->base_dir, service_id);
				}
				if (! gf_dir_exists(service->output_dir)) {
					e = gf_mkdir(service->output_dir);
					if (e==GF_IO_ERR) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to create output service dir %s, working in memory mode\n", service->output_dir));
						gf_free(service->output_dir);
						service->output_dir = NULL;
					}
				}
			}
			if (atscd->service_autotune==0xFFFFFFFF) service->tune_mode = GF_ATSC_TUNE_ON;
			else if (atscd->service_autotune==0xFFFFFFFE) {
				service->tune_mode = GF_ATSC_TUNE_ON;
				atscd->service_autotune -= 1;
			}
			else if (atscd->service_autotune==service_id) service->tune_mode = GF_ATSC_TUNE_ON;
			else if (atscd->tune_all_sls) service->tune_mode = GF_ATSC_TUNE_SLS_ONLY;

			//we are tuning, register socket
			if (service->tune_mode != GF_ATSC_TUNE_OFF)
				gf_sk_group_register(atscd->active_sockets, service->sock);

			gf_list_add(atscd->services, service);
			if (atscd->on_event) atscd->on_event(atscd->udta, GF_ATSC_EVT_SERVICE_FOUND, service_id, NULL);
		}
	}
	if (atscd->on_event) atscd->on_event(atscd->udta, GF_ATSC_EVT_SERVICE_SCAN, 0, NULL);
	return GF_OK;
}


static void gf_atsc3_obj_to_reservoir(GF_ATSCDmx *atscd, GF_ATSCService *s, GF_LCTObject *obj)
{
	//remove other objects
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d : moving object tsi %u toi %u to reservoir\n", s->service_id, obj->tsi, obj->toi));

#ifndef GPAC_DISABLE_LOG
	if (gf_log_tool_level_on(GF_LOG_CONTAINER, GF_LOG_DEBUG)){
		u32 i, count = gf_list_count(s->objects);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d : active objects TOIs for tsi %u: ", s->service_id, obj->tsi));
		for (i=0;i<count;i++) {
			GF_LCTObject *o = gf_list_get(s->objects, i);
			if (o==obj) continue;
			if (o->tsi != obj->tsi) continue;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, (" %u", o->toi));
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("\n"));
	}
#endif

	if (s->last_active_obj==obj) s->last_active_obj = NULL;
	obj->closed_flag = GF_FALSE;
	obj->nb_bytes = 0;
	obj->nb_frags = GF_FALSE;
	obj->nb_recv_frags = 0;
	obj->rlct = NULL;
	obj->toi = 0;
	obj->tsi = 0;
	obj->total_length = 0;
	obj->prev_start_offset = 0;
	obj->download_time_ms = 0;
	obj->status = GF_LCT_OBJ_INIT;
	gf_list_del_item(s->objects, obj);
	gf_list_add(atscd->object_reservoir, obj);

}

//#define CHECK_ISOM

#ifdef CHECK_ISOM
#include <gpac/isomedia.h>
#endif

static GF_Err gf_atsc3_dmx_process_object(GF_ATSCDmx *atscd, GF_ATSCService *s, GF_LCTObject *obj)
{
	char szPath[GF_MAX_PATH], *sep;
	u32 i, count, nb_objs;
	Bool partial = GF_FALSE;
	FILE *out;

	if (!obj->rlct) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d : internal error, no LCT ROUTE channel defined for object TSI %u TOI %u\n", s->service_id, obj->tsi, obj->toi));
		return GF_SERVICE_ERROR;
	}
	assert(obj->status>GF_LCT_OBJ_RECEPTION);

	if (obj->status==GF_LCT_OBJ_DONE_ERR) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d : object TSI %u TOI %u partial received only\n", s->service_id, obj->tsi, obj->toi));
		partial = GF_TRUE;
	}
	if (obj->status == GF_LCT_OBJ_DISPATCHED) return GF_OK;
	obj->status = GF_LCT_OBJ_DISPATCHED;

	if (!atscd->base_dir) {
		Bool is_init = GF_FALSE;

		if (obj->rlct->init_toi == obj->toi) {
			sprintf(szPath, "%s", obj->rlct->init_filename ? obj->rlct->init_filename : "ghost-init.mp4");
			is_init = GF_TRUE;
		} else {
			sprintf(szPath, obj->rlct->toi_template, obj->toi);
		}
#ifndef GPAC_DISABLE_LOG
		if (partial || gf_log_tool_level_on(GF_LOG_CONTAINER, GF_LOG_DEBUG)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[ATSC] Service %d got file %s (TSI %u TOI %u) size %d in %d ms (%d bytes in %d fragments)\n", s->service_id, szPath, obj->tsi, obj->toi, obj->total_length, obj->download_time_ms, obj->nb_bytes, obj->nb_recv_frags));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[ATSC] Service %d got file %s (TSI %u TOI %u) size %d in %d ms\n", s->service_id, szPath, obj->tsi, obj->toi, obj->total_length, obj->download_time_ms));
		}
#endif

#ifdef CHECK_ISOM
		{
			char szBlob[1024];
			GF_Blob blob;
			blob.data = obj->payload;
			blob.size = obj->total_length;
			GF_ISOFile *file;
			sprintf(szBlob, "gmem://%p", &blob);
			file = gf_isom_open(szBlob, GF_ISOM_OPEN_READ_DUMP, NULL);
			if (file) {
				fprintf(stderr, "ISOBMF %s file OK\n", szPath);
				gf_isom_delete(file);
			} else {
				fprintf(stderr, "ISOBMF %s file invalid\n", szPath);
			}
		}
#endif

		if (atscd->on_event) {
			GF_ATSCEventFileInfo finfo;
			memset(&finfo, 0, sizeof(GF_ATSCEventFileInfo));
			finfo.filename = szPath;
			finfo.data = obj->payload;
			finfo.size = obj->total_length;
			finfo.tsi = obj->tsi;
			finfo.toi = obj->toi;
			finfo.corrupted = partial;
			finfo.download_ms = obj->download_time_ms;
			atscd->on_event(atscd->udta, is_init ? GF_ATSC_EVT_INIT_SEG : GF_ATSC_EVT_SEG, s->service_id, &finfo);
		}
		return GF_OK;
	}

	if (obj->rlct->init_toi == obj->toi) {
		sprintf(szPath, "%s/%s", s->output_dir, obj->rlct->init_filename ? obj->rlct->init_filename : "ghost-init.mp4");
	} else {
		char szFileName[1024];
		sprintf(szFileName, obj->rlct->toi_template, obj->toi);
		sprintf(szPath, "%s/%s", s->output_dir, szFileName);
	}


	sep = strrchr(szPath, '/');
	sep[0]=0;
	if (gf_dir_exists(szPath)==GF_FALSE) {
		GF_Err e = gf_mkdir(szPath);
		if (e==GF_IO_ERR) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d failed to create output dir %s\n", s->service_id, szPath ));
			return GF_IO_ERR;
		}
	}
	sep[0]='/';

	out = gf_fopen(szPath, "wb");
	if (!out) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d failed to create file %s\n", s->service_id, szPath ));
		return GF_IO_ERR;
	} else {
		u32 bytes;
		bytes = (u32) fwrite(obj->payload, 1, (size_t) obj->total_length, out);
		gf_fclose(out);
		if (bytes != obj->total_length) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d failed to write DASH resource file %d written for %d total\n", s->service_id, bytes, obj->total_length));
			return GF_IO_ERR;
		}

#ifdef CHECK_ISOM
		{
			GF_ISOFile *file = gf_isom_open(szPath, GF_ISOM_OPEN_READ_DUMP, NULL);
			if (file) {
				fprintf(stderr, "ISOBMF %s file OK\n", szPath);
				gf_isom_delete(file);
			} else {
				fprintf(stderr, "ISOBMF %s file invalid\n", szPath);
			}
		}
#endif

	}
#ifndef GPAC_DISABLE_LOG
	if (partial || gf_log_tool_level_on(GF_LOG_CONTAINER, GF_LOG_DEBUG)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[ATSC] Service %d got file %s (TSI %u TOI %u) size %d in %d ms (%d bytes in %d fragments)\n", s->service_id, szPath, obj->tsi, obj->toi, obj->total_length, obj->download_time_ms, obj->nb_bytes, obj->nb_recv_frags));
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[ATSC] Service %d got file %s (TSI %u TOI %u) size %d in %d ms\n", s->service_id, szPath, obj->tsi, obj->toi, obj->total_length, obj->download_time_ms));
	}
#endif
	//keep init segment active
	if (obj->toi==obj->rlct->init_toi) return GF_OK;
	//no limit on objects, move to reservoir
	if (!atscd->max_seg_store) {
		gf_atsc3_obj_to_reservoir(atscd, s, obj);
		return GF_OK;
	}

	//remove all pending objects except init segment
	count = gf_list_count(s->objects);
	nb_objs = 0;
	for (i=0; i<count; i++) {
		GF_LCTObject *o = gf_list_get(s->objects, i);
		if (o->tsi != obj->tsi) continue;
		if (obj->rlct && (o->toi==obj->rlct->init_toi)) continue;
		nb_objs++;
		if (o==obj) break;
	}
	if (atscd->max_seg_store >= nb_objs) return GF_OK;
	nb_objs -= atscd->max_seg_store;

	for (i=0; i<count; i++) {
		GF_LCTObject *o;
		if (!nb_objs) break;
		o = gf_list_get(s->objects, i);
		if (o->tsi != obj->tsi) continue;
		if (o==obj) break;
		if (o->toi==obj->rlct->init_toi) continue;
		if (nb_objs) {
			char szFileName[1024];
			sprintf(szFileName, o->rlct->toi_template, o->toi);
			sprintf(szPath, "%s/%s", s->output_dir, szFileName);
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[ATSC] Service %d deleting file %s (TSI %u TOI %u)\n", s->service_id, szPath, o->tsi, o->toi));
			gf_delete_file(szPath);
			i--;
			count--;
			gf_atsc3_obj_to_reservoir(atscd, s, o);
			nb_objs--;
		}
	}
	return GF_OK;
}

static GF_Err gf_atsc3_service_flush_object(GF_ATSCService *s, GF_LCTObject *obj)
{
	u32 i;
	u64 start_offset = 0;
	obj->status = GF_LCT_OBJ_DONE;
	for (i=0; i<obj->nb_frags; i++) {
		if (start_offset != obj->frags[0].offset) {
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

static GF_Err gf_atsc3_service_gather_object(GF_ATSCDmx *atscd, GF_ATSCService *s, u32 tsi, u32 toi, u32 start_offset, char *data, u32 size, u32 total_len, Bool close_flag, Bool in_order, GF_ATSCLCTChannel *rlct, GF_LCTObject **gather_obj)
{
	Bool inserted, done;
	u32 i, count;
	GF_LCTObject *obj = s->last_active_obj;

	//in case last packet(s) are duplicated after we sent the object, skip them
	if (rlct) {
		if ((tsi==rlct->last_dispatched_tsi) && (toi==rlct->last_dispatched_toi)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d TSI %u TOI %u LCT fragment on already dispatched object, skipping\n", s->service_id, tsi, toi));
			return GF_OK;
		}
	} else {
		if (!tsi && (toi==s->last_dispatched_toi_on_tsi_zero)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d TSI %u TOI %u LCT fragment on already dispatched object, skipping\n", s->service_id, tsi, toi));
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
		obj = gf_list_pop_back(atscd->object_reservoir);
		if (!obj) {
			GF_SAFEALLOC(obj, GF_LCTObject);
			if (!obj) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d failed to allocate LCT object TSI %u TOI %u\n", s->service_id, toi, tsi ));
				return GF_OUT_OF_MEM;
			}
			obj->nb_alloc_frags = 10;
			obj->frags = gf_malloc(sizeof(GF_LCTFragInfo)*obj->nb_alloc_frags);
		}
		obj->toi = toi;
		obj->tsi = tsi;
		obj->status = GF_LCT_OBJ_INIT;
		obj->total_length = total_len;
		if (tsi && rlct) obj->rlct = rlct;

		if (!total_len) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d object TSI %u TOI %u started without total-length assigned !\n", s->service_id, tsi, toi ));
		} else {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d starting object TSI %u TOI %u total-length %d\n", s->service_id, tsi, toi, total_len));

		}
		obj->download_time_ms = gf_sys_clock();
		gf_list_add(s->objects, obj);
	} else if (!obj->total_length && total_len) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d object TSI %u TOI %u was started without total-length assigned, assigning to %u\n", s->service_id, tsi, toi, total_len));
		obj->total_length = total_len;
	} else if (total_len && (obj->total_length != total_len)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d object TSI %u TOI %u mismatch in total-length %u  assigned, %u redeclared\n", s->service_id, tsi, toi, obj->total_length, total_len));
	}
	if (s->last_active_obj != obj) {
		//last object had EOS and not completed
		if (s->last_active_obj && s->last_active_obj->closed_flag && (s->last_active_obj->status<GF_LCT_OBJ_DONE_ERR)) {
			GF_LCTObject *o = s->last_active_obj;
		 	if (o->tsi) {
		 		gf_atsc3_service_flush_object(s, o);
				gf_atsc3_dmx_process_object(atscd, s, o);
			} else {
				gf_atsc3_obj_to_reservoir(atscd, s, o);
			}
 			s->last_active_obj = obj;
		} else if (in_order) {
			u32 count = gf_list_count(s->objects);
 			s->last_active_obj = obj;
			for (i=0; i<count; i++) {
				u32 new_count;
				GF_LCTObject *o = gf_list_get(s->objects, i);
				if (o==obj) break;
				//we can only detect losses if a new TOI on the same TSI is found
				if (o->tsi != obj->tsi) continue;
				if (o->status>=GF_LCT_OBJ_DONE_ERR) continue;

				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d object TSI %u TOI %u not completely received but in-order delivery signaled and new TOI %u - forcing dispatch\n", s->service_id, o->tsi, o->toi, toi ));
				if (o->tsi) {
		 			gf_atsc3_service_flush_object(s, o);
					gf_atsc3_dmx_process_object(atscd, s, o);
				} else {
					gf_atsc3_obj_to_reservoir(atscd, s, o);
				}
				new_count = gf_list_count(s->objects);
				//objects purged
				if (new_count<count) {
					i=-1;
					count = new_count;
				}
			}
		}
	}
	*gather_obj = obj;
	assert(obj->toi == toi);
	assert(obj->tsi == tsi);

	//keep receiving if we are done with errors
	if (obj->status >= GF_LCT_OBJ_DONE) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d object TSI %u TOI %u already received - skipping\n", s->service_id, tsi, toi ));
		return GF_EOS;
	}
	obj->nb_recv_bytes += size;
	inserted = GF_FALSE;
	for (i=0; i<obj->nb_frags; i++) {
		if ((obj->frags[i].offset <= start_offset) && (obj->frags[i].offset + obj->frags[i].size >= start_offset + size) ) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d LCT fragment already received\n", s->service_id));
			return GF_OK;
		}

		//insert fragment
		if (obj->frags[i].offset > start_offset) {
			//check overlap (not sure if this is legal)
			if (start_offset + size > obj->frags[i].offset) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d Overlapping LCT fragment, not supported\n", s->service_id));
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
			break;
		}
		//expand fragment
		if (obj->frags[i].offset + obj->frags[i].size == start_offset) {
			obj->frags[i].size += size;
			obj->nb_bytes += size;
			inserted = GF_TRUE;
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
	}
	obj->nb_recv_frags++;

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
		obj->payload = gf_realloc(obj->payload, obj->alloc_size+1);
		obj->payload[obj->alloc_size] = 0;
	}
	assert(obj->alloc_size >= start_offset + size);

	memcpy(obj->payload + start_offset, data, size);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d TSI %u TOI %u append LCT fragment, offset %d total size %d recv bytes %d - offset diff since last %d\n", s->service_id, obj->tsi, obj->toi, start_offset, obj->total_length, obj->nb_bytes, (s32) start_offset - (s32) obj->prev_start_offset));

	obj->prev_start_offset = start_offset;
	assert(obj->toi == toi);
	assert(obj->tsi == tsi);

	//check if we are done
	done = GF_FALSE;
	if (obj->total_length) {
		if (obj->nb_bytes >= obj->total_length) {
			done = GF_TRUE;
		}
		else if (close_flag) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[ATSC] Service %d object TSI %u TOI %u closed flag found but not yet completed\n", s->service_id, tsi, toi ));
		}
	} else {
		if (close_flag) obj->closed_flag = GF_TRUE;
	}
	if (!done) return GF_OK;

	s->last_active_obj = NULL;
	if (obj->rlct) {
		obj->rlct->last_dispatched_tsi = obj->tsi;
		obj->rlct->last_dispatched_toi = obj->toi;
	} else {
		s->last_dispatched_toi_on_tsi_zero = obj->toi;
	}
	return gf_atsc3_service_flush_object(s, obj);
}

static GF_Err gf_atsc3_service_setup_dash(GF_ATSCDmx *atscd, GF_ATSCService *s, char *content, char *content_location)
{
	u32 len = (u32) strlen(content);

	if (s->tune_mode==GF_ATSC_TUNE_SLS_ONLY) {
		s->tune_mode = GF_ATSC_TUNE_OFF;
		//unregister sockets
		gf_atsc3_register_service_sockets(atscd, s, GF_FALSE);
	}

	if (s->output_dir) {
		FILE *out;
		char szPath[GF_MAX_PATH];
		sprintf(szPath, "%s/%s", s->output_dir, content_location);
		out = gf_fopen(szPath, "wb");
		if (!out) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d failed to create MPD file %s\n", s->service_id, szPath ));
			return GF_IO_ERR;
		} else {
			u32 bytes = (u32) fwrite(content, 1, (size_t) len, out);
			gf_fclose(out);
			if (bytes != len) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d failed to write MPD file %d written for %d total\n", s->service_id, bytes, len));
				return GF_IO_ERR;
			}
		}
		return GF_OK;
	}

	if (atscd->on_event) {
		GF_ATSCEventFileInfo finfo;
		memset(&finfo, 0, sizeof(GF_ATSCEventFileInfo));
		finfo.data = content;
		finfo.size = len;
		finfo.filename = content_location;
		atscd->on_event(atscd->udta, GF_ATSC_EVT_MPD, s->service_id, &finfo);
	}
	else {
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[ATSC] Service %d received MPD file %s content:\n%s\n", s->service_id, content_location, content ));
	}
	return GF_OK;
}

static GF_Err gf_atsc3_service_parse_mbms_enveloppe(GF_ATSCDmx *atscd, GF_ATSCService *s, char *content, char *content_location, u32 *stsid_version, u32 *mpd_version)
{
	u32 i, j;
	GF_Err e;
	GF_XMLAttribute *att;
	GF_XMLNode *it, *root;

	e = gf_xml_dom_parse_string(atscd->dom, content);
	root = gf_xml_dom_get_root(atscd->dom);
	if (e || !root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d failed to parse S-TSID: %s\n", s->service_id, gf_error_to_string(e) ));
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
		if (!strcmp(content_type, "application/s-tsid")) *stsid_version = version;
		else if (!strcmp(content_type, "application/dash+xml")) *mpd_version = version;
	}
	return GF_OK;
}

static GF_Err gf_atsc3_service_setup_stsid(GF_ATSCDmx *atscd, GF_ATSCService *s, char *content, char *content_location)
{
	GF_Err e;
	GF_XMLAttribute *att;
	GF_XMLNode *rs, *ls, *srcf, *efdt, *node, *root;
	u32 i, j, k, crc, nb_lct_channels=0;

	crc = gf_crc_32(content, (u32) strlen(content) );
	if (!s->stsid_crc) {
		s->stsid_crc = crc;
	} else if (s->stsid_crc != crc) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d update of S-TSID not yet supported, skipping\n", s->service_id));
		return GF_NOT_SUPPORTED;
	} else {
		return GF_OK;
	}

	e = gf_xml_dom_parse_string(atscd->dom, content);
	root = gf_xml_dom_get_root(atscd->dom);
	if (e || !root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d failed to parse S-TSID: %s\n", s->service_id, gf_error_to_string(e) ));
		return e;
	}
	i=0;
	while ((rs = gf_list_enum(root->content, &i))) {
		char *dst_ip = s->dst_ip;
		u32 dst_port = s->port;
		char *file_template = NULL;
		char *init_file_name = NULL;
		GF_ATSCRouteSession *rsess;
		GF_ATSCLCTChannel *rlct;
		u32 init_file_toi=0;
		u32 tsi = 0;
		if (rs->type != GF_XML_NODE_TYPE) continue;
		if (strcmp(rs->name, "RS")) continue;

		j=0;
		while ((att = gf_list_enum(rs->attributes, &j))) {
			if (!stricmp(att->name, "dIpAddr")) dst_ip = att->value;
			else if (!stricmp(att->name, "dPort")) dst_port = atoi(att->value);
		}

		GF_SAFEALLOC(rsess, GF_ATSCRouteSession);
		rsess->channels = gf_list_new();

		//need a new socket for the session
		if ((strcmp(s->dst_ip, dst_ip)) || (s->port != dst_port) ) {
			rsess->sock = gf_sk_new(GF_SOCK_TYPE_UDP);
			gf_sk_set_usec_wait(rsess->sock, 1);
			e = gf_sk_setup_multicast(rsess->sock, dst_ip, dst_port, 0, GF_FALSE, (char *) atscd->ip_ifce);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d  failed to setup mcast for route session on %s:%d\n", s->service_id, dst_ip, dst_port));
				return e;
			}
			gf_sk_set_buffer_size(rsess->sock, GF_FALSE, atscd->unz_buffer_size);
			//gf_sk_set_block_mode(rsess->sock, GF_TRUE);
			s->secondary_sockets++;
			if (s->tune_mode == GF_ATSC_TUNE_ON) gf_sk_group_register(atscd->active_sockets, rsess->sock);
		}
		gf_list_add(s->route_sessions, rsess);

		j=0;
		while ((ls = gf_list_enum(rs->content, &j))) {
			char *sep;
			if (ls->type != GF_XML_NODE_TYPE) continue;
			if (strcmp(ls->name, "LS")) continue;

			//extract TSI
			k=0;
			while ((att = gf_list_enum(ls->attributes, &k))) {
				if (!strcmp(att->name, "tsi")) sscanf(att->value, "%u", &tsi);
			}
			if (!tsi) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d missing TSI in LS/ROUTE session\n", s->service_id));
				return GF_NON_COMPLIANT_BITSTREAM;
			}
			k=0;
			srcf = NULL;
			while ((srcf = gf_list_enum(ls->content, &k))) {
				if ((srcf->type == GF_XML_NODE_TYPE) && !strcmp(srcf->name, "SrcFlow")) break;
				srcf = NULL;
			}
			if (!srcf) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d missing srcFlow in LS/ROUTE session\n", s->service_id));
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
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d missing EFDT element in LS/ROUTE session, not supported\n", s->service_id));
				return GF_NOT_SUPPORTED;
			}

			k=0;
			while ((node = gf_list_enum(efdt->content, &k))) {
				if (node->type != GF_XML_NODE_TYPE) continue;
				//Korean version
				if (!strcmp(node->name, "FileTemplate")) {
					GF_XMLNode *content = gf_list_get(node->content, 0);
					if (content->type==GF_XML_TEXT_TYPE) file_template = content->name;
				}
				else if (!strcmp(node->name, "FDTParameters")) {
					u32 l=0;
					GF_XMLNode *fdt = NULL;
					while ((fdt = gf_list_enum(node->content, &l))) {
						if ((fdt->type == GF_XML_NODE_TYPE) && (strstr(fdt->name, "File")!=NULL)) break;
						fdt = NULL;
					}
					if (fdt) {
						l=0;
						while ((att = gf_list_enum(fdt->attributes, &l))) {
							if (!strcmp(att->name, "Content-Location")) init_file_name = att->value;
							else if (!strcmp(att->name, "TOI")) sscanf(att->value, "%u", &init_file_toi);
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
						if ((fdt->type == GF_XML_NODE_TYPE) && (strstr(fdt->name, "File")!=NULL)) break;
						fdt = NULL;
					}
					if (fdt) {
						l=0;
						while ((att = gf_list_enum(fdt->attributes, &l))) {
							if (!strcmp(att->name, "Content-Location")) init_file_name = att->value;
							else if (!strcmp(att->name, "TOI")) sscanf(att->value, "%u", &init_file_toi);
						}
					}
				}
			}

			if (!init_file_name) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d missing init file name in LS/ROUTE session, could be problematic - will consider any TOI %u (-1) present as a ghost init segment\n", s->service_id, (u32)-1));
				//force an init at -1, some streams still have the init not declared but send on TOI -1
				// interpreting it as a regular segment would break clock setup
				init_file_toi = (u32) -1;
			}
			if (!file_template) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d missing file TOI template in LS/ROUTE session, not supported - skipping stream\n", s->service_id));
				continue;
			}
			sep = strstr(file_template, "$TOI$");
			if (!sep) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d wrong TOI template %s in LS/ROUTE session\n", s->service_id, file_template));
				return GF_NOT_SUPPORTED;
			}
			nb_lct_channels++;

			//OK setup LCT channel for route
			GF_SAFEALLOC(rlct, GF_ATSCLCTChannel);
			rlct->init_toi = init_file_toi;
			rlct->tsi = tsi;
			rlct->init_filename = init_file_name ? gf_strdup(init_file_name) : NULL;
			rlct->toi_template = gf_strdup(file_template);
			sep = strstr(rlct->toi_template, "$TOI$");
			sep[0] = 0;
			strcat(rlct->toi_template, "%d");
			sep = strstr(file_template, "$TOI$");
			strcat(rlct->toi_template, sep+5);

			//fill in payloads
			k=0;
			efdt = NULL;
			while ((node = gf_list_enum(srcf->content, &k))) {
				if (node->type != GF_XML_NODE_TYPE) continue;
				if (!strcmp(node->name, "Payload")) {
					u32 l=0;
					GF_ATSCLCTReg *lreg;
					lreg = &rlct->CPs[rlct->nb_cps];
					lreg->order = 1; //default
					while ((att = gf_list_enum(node->attributes, &l))) {
						if (!strcmp(att->name, "codePoint")) lreg->codepoint = (u8) atoi(att->value);
						else if (!strcmp(att->name, "formatID")) lreg->format_id = (u8) atoi(att->value);
						else if (!strcmp(att->name, "frag")) lreg->frag = (u8) atoi(att->value);
						else if (!strcmp(att->name, "order")) lreg->order = (u8) atoi(att->value);
						else if (!strcmp(att->name, "srcFecPayloadID")) lreg->src_fec_payload_id = (u8) atoi(att->value);
					}
					if (lreg->src_fec_payload_id) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d payload format indicates srcFecPayloadId %d (reserved), assuming 0\n", s->service_id, lreg->src_fec_payload_id));

					}
					if (lreg->format_id != 1) {
						if (lreg->format_id && (lreg->format_id<5)) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d payload formatID %d not supported\n", s->service_id, lreg->format_id));
						} else {
							GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d payload formatID %d reserved, assuming 1\n", s->service_id, lreg->format_id));
						}
					}
					rlct->nb_cps++;
					if (rlct->nb_cps==8) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d more payload formats than supported (8 max)\n", s->service_id));
						break;
					}
				}
			}

			gf_list_add(rsess->channels, rlct);
		}
	}

	if (!nb_lct_channels) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d does not have any supported LCT channels\n", s->service_id));
		return GF_NOT_SUPPORTED;
	}
	return GF_OK;
}

static GF_Err gf_atsc3_dmx_process_service_signaling(GF_ATSCDmx *atscd, GF_ATSCService *s, GF_LCTObject *object, u8 cc, u32 stsid_version, u32 mpd_version)
{
	char *payload, *boundary=NULL, *sep;
	char szContentType[100], szContentLocation[1024];
	u32 payload_size;
	GF_Err e;

	//uncompress bundle
	if (object->toi & 0x80000000 /*(1<<31)*/ ) {
		u32 raw_size;
		if (object->total_length > atscd->buffer_size) {
			atscd->buffer_size = object->total_length;
			atscd->buffer = gf_realloc(atscd->buffer, object->total_length);
			if (!atscd->buffer) return GF_OUT_OF_MEM;
		}
		memcpy(atscd->buffer, object->payload, object->total_length);
		e = gf_gz_decompress_payload(atscd->buffer, object->total_length, &atscd->unz_buffer, &raw_size);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d failed to decompress signaling bundle: %s\n", s->service_id, gf_error_to_string(e) ));
			return e;
		}
		if (raw_size > atscd->unz_buffer_size) atscd->unz_buffer_size = raw_size;
		payload = atscd->unz_buffer;
		payload_size = raw_size;
	} else {
		payload = object->payload;
		payload_size = object->total_length;
	}
	payload[payload_size] = 0;

	//check for multipart
	if (!strncmp(payload, "Content-Type: multipart/", 24)) {
		sep = strstr(payload, "boundary=\"");
		if (!sep) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d cannot find multipart boundary in package:\n%s\n", s->service_id, payload ));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		if (sep) payload = sep + 10;
		sep = strstr(payload, "\"");
		if (!sep) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d multipart boundary not properly formatted in package:\n%s\n", s->service_id, payload ));
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
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d unrecognized header entity in package:\n%s\n", s->service_id, payload ));
			}
		}
		payload += 4;
		content = boundary ? strstr(payload, "\r\n--") : strstr(payload, "\r\n\r\n");
		if (content) {
			content[0] = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d package type %s location %s content:\n%s\n", s->service_id, szContentType, szContentLocation, payload ));
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d %s not properly formatted in package:\n%s\n", s->service_id, boundary ? "multipart boundary" : "entity", payload ));
			if (sep) sep[0] = boundary[0];
			gf_free(boundary);
			return GF_NON_COMPLIANT_BITSTREAM;
		}

		if (!strcmp(szContentType, "application/mbms-envelope+xml")) {
			e = gf_atsc3_service_parse_mbms_enveloppe(atscd, s, payload, szContentLocation, &stsid_version, &mpd_version);

			if (e || !stsid_version || !mpd_version) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d MBMS envelope error %s, S-TSID version %d MPD version %d\n",s->service_id, gf_error_to_string(e), stsid_version, mpd_version ));
				gf_free(boundary);
				return e ? e : GF_SERVICE_ERROR;
			}
		} else if (!strcmp(szContentType, "application/mbms-user-service-description+xml")) {
		} else if (!strcmp(szContentType, "application/dash+xml")) {
			if (mpd_version && (mpd_version+1 != s->mpd_version)) {
				s->mpd_version = mpd_version+1;
				gf_atsc3_service_setup_dash(atscd, s, payload, szContentLocation);
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d same MPD version, ignoring\n",s->service_id));
			}
		}
		//Korean and US version have different mime types
		else if (!strcmp(szContentType, "application/s-tsid") || !strcmp(szContentType, "application/route-s-tsid+xml")) {
			if (stsid_version && (stsid_version+1 != s->stsid_version)) {
				s->stsid_version = stsid_version+1;
				gf_atsc3_service_setup_stsid(atscd, s, payload, szContentLocation);
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d same S-TSID version, ignoring\n",s->service_id));
			}
		}
		if (!sep) break;
		sep[0] = boundary[0];
		payload = sep;
	}

	gf_free(boundary);
	return GF_OK;
}

enum
{
	/*No-operation extension header*/
	GF_LCT_EXT_NOP = 0,
	/*Authentication extension header*/
	GF_LCT_EXT_AUTH = 1,
	/*Time extension header*/
	GF_LCT_EXT_TIME = 2,
	/*FEC object transmission information extension header*/
	GF_LCT_EXT_FTI = 64,
	/*Extension header for FDT - FLUTE*/
	GF_LCT_EXT_FDT = 192,
	/*Extension header for FDT content encoding - FLUTE*/
	GF_LCT_EXT_CENC = 193,
	/*TOL extension header - ATSC - 24 bit payload*/
	GF_LCT_EXT_TOL24 = 194,
	/*TOL extension header - ATSC - HEL + 28 bit payload*/
	GF_LCT_EXT_TOL48 = 67,
};

static GF_Err gf_atsc3_dmx_process_service(GF_ATSCDmx *atscd, GF_ATSCService *s, GF_ATSCRouteSession *route_sess)
{
	GF_Err e;
	u32 nb_read, v, C, psi, S, O, H, /*Res, A,*/ B, hdr_len, cp, cc, tsi, toi, pos;
	u32 /*a_G=0, a_U=0,*/ a_S=0, a_M=0/*, a_A=0, a_H=0, a_D=0*/;
	u64 tol_size=0;
	Bool in_order = GF_TRUE;
	u32 start_offset;
	GF_ATSCLCTChannel *rlct=NULL;
	GF_LCTObject *gather_object=NULL;

	if (route_sess) {
		e = gf_sk_receive_no_select(route_sess->sock, atscd->buffer, atscd->buffer_size, &nb_read);
	} else {
		e = gf_sk_receive_no_select(s->sock, atscd->buffer, atscd->buffer_size, &nb_read);
	}

	if (e != GF_OK) return e;
	assert(nb_read);

	atscd->nb_packets++;
	atscd->total_bytes_recv += nb_read;
	atscd->last_pck_time = gf_sys_clock_high_res();
	if (!atscd->first_pck_time) atscd->first_pck_time = atscd->last_pck_time;

	e = gf_bs_reassign_buffer(atscd->bs, atscd->buffer, nb_read);
	if (e != GF_OK) return e;

	//parse LCT header
	v = gf_bs_read_int(atscd->bs, 4);
	C = gf_bs_read_int(atscd->bs, 2);
	psi = gf_bs_read_int(atscd->bs, 2);
	S = gf_bs_read_int(atscd->bs, 1);
	O = gf_bs_read_int(atscd->bs, 2);
	H = gf_bs_read_int(atscd->bs, 1);
	/*Res = */gf_bs_read_int(atscd->bs, 2);
	/*A = */gf_bs_read_int(atscd->bs, 1);
	B = gf_bs_read_int(atscd->bs, 1);
	hdr_len = gf_bs_read_int(atscd->bs, 8);
	cp = gf_bs_read_int(atscd->bs, 8);

	if (v!=1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d : wrong LCT header version %d\n", s->service_id, v));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (C!=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d : wrong ROUTE LCT header C %d, expecting 0\n", s->service_id, C));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if ((psi!=0) && (psi!=2) ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d : wrong ROUTE LCT header PSI %d, expecting b00 or b10\n", s->service_id, psi));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (S!=1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d : wrong ROUTE LCT header S, shall be 1\n", s->service_id));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (O!=1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d : wrong ROUTE LCT header S, shall be b01\n", s->service_id));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	else if (H!=0) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d : wrong ROUTE LCT header H, shall be 0\n", s->service_id));
		return GF_NON_COMPLIANT_BITSTREAM;
	}
	if (hdr_len<4) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Service %d : wrong ROUTE LCT header len %d, shall be at least 4 0\n", s->service_id, hdr_len));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (psi==0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d : FEC ROUTE not implemented\n", s->service_id));
		return GF_NOT_SUPPORTED;
	}

	cc = gf_bs_read_u32(atscd->bs);
	tsi = gf_bs_read_u32(atscd->bs);
	toi = gf_bs_read_u32(atscd->bs);
	hdr_len-=4;

	//filter TSI if not 0 (service TSI) and debug mode set
	if (atscd->debug_tsi && tsi && (tsi!=atscd->debug_tsi)) return GF_OK;

	//look for TSI 0 first
	if (tsi!=0) {
		u32 i=0;
		Bool in_session = GF_FALSE;
		if (s->tune_mode==GF_ATSC_TUNE_SLS_ONLY) return GF_OK;

		if (s->last_active_obj && (s->last_active_obj->tsi==tsi)) {
			in_session = GF_TRUE;
			rlct = s->last_active_obj->rlct;
		} else {
			u32 i=0;
			GF_ATSCRouteSession *rsess;
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
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d : no session with TSI %u defined, skipping packet\n", s->service_id, tsi));
			return GF_OK;
		}
		for (i=0; rlct && i<rlct->nb_cps; i++) {
			if (rlct->CPs[i].codepoint==cp) {
				in_order = rlct->CPs[i].order;
				break;
			}
		}
	} else {
		//check TOI for TSI 0
		//a_G = toi & 0x80000000 /*(1<<31)*/ ? 1 : 0;
		//a_U = toi & (1<<16) ? 1 : 0;
		a_S = toi & (1<<17) ? 1 : 0;
		a_M = toi & (1<<18) ? 1 : 0;
		/*a_A = toi & (1<<19) ? 1 : 0;
		a_H = toi & (1<<22) ? 1 : 0;
		a_D = toi & (1<<23) ? 1 : 0;*/
		v = toi & 0xFF;
		//skip known version
		if (a_M && (s->mpd_version == v+1)) a_M = 0;
		if (a_S && (s->stsid_version == v+1)) a_S = 0;


		//for now we only care about S and M
		if (!a_S && ! a_M) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d : SLT bundle without MPD or S-TSID, skipping packet\n", s->service_id));
			return GF_OK;
		}
	}

	//parse extensions
	while (hdr_len) {
		u8 hel=0, het = gf_bs_read_u8(atscd->bs);
		if (het<128) hel = gf_bs_read_u8(atscd->bs);

		switch (het) {
		case GF_LCT_EXT_FDT:
			/*flute_version = */gf_bs_read_int(atscd->bs, 4);
			/*fdt_instance_id = */gf_bs_read_int(atscd->bs, 20);
			break;

		case GF_LCT_EXT_CENC:
			/*content_encodind = */gf_bs_read_int(atscd->bs, 8);
			/*reserved = */gf_bs_read_int(atscd->bs, 16);
			break;

		case GF_LCT_EXT_TOL24:
			tol_size = gf_bs_read_int(atscd->bs, 24);
			break;

		case GF_LCT_EXT_TOL48:
			if (hel!=2) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d : wrong HEL %d for TOL48 LCT extension, expecing 2\n", s->service_id, hel));
				continue;
			}
			tol_size = gf_bs_read_long_int(atscd->bs, 48);
			break;

		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d : unsupported header extension HEL %d HET %d\n", s->service_id, hel, het));
			break;
		}
		if (hdr_len<hel) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Service %d : wrong HEL %d for LCT extension %d, remainign header size %d\n", s->service_id, hel, het, hdr_len));
			continue;
		}
		if (hel) hdr_len -= hel;
		else hdr_len -= 1;
	}

	start_offset = gf_bs_read_u32(atscd->bs);
	pos = (u32) gf_bs_get_position(atscd->bs);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSC] Service %d : LCT packet TSI %u TOI %u size %d startOffset %u TOL "LLU"\n", s->service_id, tsi, toi, nb_read-pos, start_offset, tol_size));

	e = gf_atsc3_service_gather_object(atscd, s, tsi, toi, start_offset, atscd->buffer + pos, nb_read-pos, (u32) tol_size, B, in_order, rlct, &gather_object);

	if (e==GF_EOS) {
		if (!tsi) {
			if (gather_object->status==GF_LCT_OBJ_DONE_ERR) {
				gf_atsc3_obj_to_reservoir(atscd, s, gather_object);
				return GF_OK;
			}
			//we don't assign version here, we use mbms envelope for that since the bundle may have several
			//packages

			gf_atsc3_dmx_process_service_signaling(atscd, s, gather_object, cc, a_S ? v : 0, a_M ? v : 0);
			//we don't release the LCT object, so that we can disard future versions
		} else {
			gf_atsc3_dmx_process_object(atscd, s, gather_object);
		}
	}

	return GF_OK;
}

static GF_Err gf_atsc3_dmx_process_lls(GF_ATSCDmx *atscd)
{
	u32 read;
	GF_Err e;
	const char *name=NULL;
	u32 lls_table_id, lls_group_id, lls_group_count, lls_table_version;
	u32 raw_size = atscd->unz_buffer_size;
	GF_XMLNode *root;

	e = gf_sk_receive_no_select(atscd->sock, atscd->buffer, atscd->buffer_size, &read);
	if (e)
		return e;

	atscd->nb_packets++;
	atscd->total_bytes_recv += read;
	atscd->last_pck_time = gf_sys_clock_high_res();
	if (!atscd->first_pck_time) atscd->first_pck_time = atscd->last_pck_time;

	lls_table_id = atscd->buffer[0];
	lls_group_id = atscd->buffer[1];
	lls_group_count = 1 + atscd->buffer[2];
	lls_table_version = atscd->buffer[3];

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSCDmx] LLSTable size %d version %d: ID %d (%s) - group ID %d - group count %d\n", read, lls_table_id, lls_table_version, (lls_table_id==1) ? "SLT" : (lls_table_id==2) ? "RRT" : (lls_table_id==3) ? "SystemTime" : (lls_table_id==4) ? "AEAT" : "Reserved", lls_group_id, lls_group_count));
	switch (lls_table_id) {
	case 1:
		if (atscd->slt_version== 1+lls_table_version) return GF_OK;
		atscd->slt_version = 1+lls_table_version;
		name="SLT";
		break;
	case 2:
		if (atscd->rrt_version== 1+lls_table_version) return GF_OK;
		atscd->rrt_version = 1+lls_table_version;
		name="RRT";
    	break;
	case 3:
		if (atscd->systime_version== 1+lls_table_version) return GF_OK;
		atscd->systime_version = 1+lls_table_version;
		name="SysTime";
    	break;
	case 4:
		if (atscd->aeat_version== 1+lls_table_version) return GF_OK;
		atscd->aeat_version = 1+lls_table_version;
		name="AEAT";
    	break;
	default:
		return GF_OK;
	}

	e = gf_gz_decompress_payload(&atscd->buffer[4], read-4, &atscd->unz_buffer, &raw_size);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to decompress %s table: %s\n", name, gf_error_to_string(e) ));
		return e;
	}
	//realloc happened
	if (atscd->unz_buffer_size<raw_size) atscd->unz_buffer_size = raw_size;
	atscd->unz_buffer[raw_size]=0;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[ATSCDmx] SLT table - payload:\n%s\n", atscd->unz_buffer));


	e = gf_xml_dom_parse_string(atscd->dom, atscd->unz_buffer);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to parse SLT XML: %s\n", gf_error_to_string(e) ));
		return e;
	}

	root = gf_xml_dom_get_root(atscd->dom);
	if (!root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSC] Failed to get XML root for %s table\n", name ));
		return e;
	}

	switch (lls_table_id) {
	case 1:
		return gf_atsc3_dmx_process_slt(atscd, root);
	case 2:
	case 3:
	case 4:
	default:
		break;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_atsc3_dmx_process(GF_ATSCDmx *atscd)
{
	u32 i, count;
	GF_Err e;

	//check all active sockets
	e = gf_sk_group_select(atscd->active_sockets, 10);
	if (e) return e;

	if (gf_sk_group_sock_is_set(atscd->active_sockets, atscd->sock)) {
		e = gf_atsc3_dmx_process_lls(atscd);
		if (e) return e;
	}

	count = gf_list_count(atscd->services);
	for (i=0; i<count; i++) {
		u32 j;
		GF_Err e;
		GF_ATSCRouteSession *rsess;
		GF_ATSCService *s = (GF_ATSCService *)gf_list_get(atscd->services, i);
		if (s->tune_mode==GF_ATSC_TUNE_OFF) continue;

		if (gf_sk_group_sock_is_set(atscd->active_sockets, s->sock)) {
			e = gf_atsc3_dmx_process_service(atscd, s, NULL);
			if (e) return e;
		}
		if (s->tune_mode!=GF_ATSC_TUNE_ON) continue;
		if (!s->secondary_sockets) continue;

		j=0;
		while ((rsess = (GF_ATSCRouteSession *)gf_list_enum(s->route_sessions, &j) )) {
			if (gf_sk_group_sock_is_set(atscd->active_sockets, rsess->sock)) {
				e = gf_atsc3_dmx_process_service(atscd, s, rsess);
				if (e) return e;
			}
		}

	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_atsc3_set_callback(GF_ATSCDmx *atscd, void (*on_event)(void *udta, GF_ATSCEventType evt, u32 evt_param, GF_ATSCEventFileInfo *info), void *udta)
{
	if (!atscd) return GF_BAD_PARAM;
	atscd->udta = udta;
	atscd->on_event = on_event;
	return GF_OK;
}

GF_EXPORT
Bool gf_atsc3_dmx_find_service(GF_ATSCDmx *atscd, u32 service_id)
{
	u32 i=0;
	GF_ATSCService *s;
	while ((s = gf_list_enum(atscd->services, &i))) {
		if (s->service_id != service_id) continue;
		return GF_TRUE;
	}
	return GF_FALSE;
}


GF_EXPORT
u32 gf_atsc3_dmx_get_object_count(GF_ATSCDmx *atscd, u32 service_id)
{
	u32 i=0;
	GF_ATSCService *s;
	while ((s = gf_list_enum(atscd->services, &i))) {
		if (s->service_id != service_id) continue;
		return gf_list_count(s->objects);
	}
	return 0;
}

GF_EXPORT
void gf_atsc3_dmx_remove_object_by_name(GF_ATSCDmx *atscd, u32 service_id, char *fileName, Bool purge_previous)
{
	u32 i=0;
	GF_ATSCService *s=NULL;
	GF_LCTObject *obj = NULL;
	while ((s = gf_list_enum(atscd->services, &i))) {
		if (s->service_id == service_id) break;
		s = NULL;
	}
	if (!s) return;
	i=0;
	while ((obj = gf_list_enum(s->objects, &i))) {
		u32 toi;
		if (obj->rlct && (sscanf(fileName, obj->rlct->toi_template, &toi) == 1)) {
			if (toi == obj->toi) {
				GF_ATSCLCTChannel *rlct = obj->rlct;
				//we likely have a loop here
				if (obj == s->last_active_obj) break;

				gf_atsc3_obj_to_reservoir(atscd, s, obj);
				if (purge_previous) {
					i=0;
					while ((obj = gf_list_enum(s->objects, &i))) {
						if (obj->rlct != rlct) continue;
						if (obj->toi==rlct->init_toi) continue;
						if (obj->toi<toi) {
							i--;
							//we likely have a loop here
							if (obj == s->last_active_obj) return;
							gf_atsc3_obj_to_reservoir(atscd, s, obj);
						}
					}
				}
				return;
			}
		}
		else if (obj->rlct && obj->rlct->init_filename && !strcmp(fileName, obj->rlct->init_filename)) {
			gf_atsc3_obj_to_reservoir(atscd, s, obj);
			return;
		}
	}
	GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSC] Failed to remove object %s from service, object not found\n", fileName));
}


GF_EXPORT
Bool gf_atsc3_dmx_remove_first_object(GF_ATSCDmx *atscd, u32 service_id)
{
	u32 i=0;
	GF_ATSCService *s=NULL;
	GF_LCTObject *obj = NULL;
	while ((s = gf_list_enum(atscd->services, &i))) {
		if (s->service_id == service_id) break;
		s = NULL;
	}
	if (!s) return GF_FALSE;

	obj = gf_list_get(s->objects, 0);
	if (obj) {
		if (obj != s->last_active_obj) {
			gf_atsc3_obj_to_reservoir(atscd, s, obj);
			return GF_TRUE;
		}
	}
	return GF_FALSE;
}

GF_EXPORT
void gf_atsc3_dmx_purge_objects(GF_ATSCDmx *atscd, u32 service_id)
{
	u32 i=0;
	GF_ATSCService *s=NULL;
	GF_LCTObject *obj = NULL;
	while ((s = gf_list_enum(atscd->services, &i))) {
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
		//if object is init segment keep it - this may need refinement in case we had init segment updates
		if (obj->rlct && (obj->rlct->init_toi==obj->toi)) continue;
		//trash
		gf_atsc3_obj_to_reservoir(atscd, s, obj);
	}
}

GF_EXPORT
void gf_atsc3_dmx_set_service_udta(GF_ATSCDmx *atscd, u32 service_id, void *udta)
{
	u32 i=0;
	GF_ATSCService *s=NULL;
	while ((s = gf_list_enum(atscd->services, &i))) {
		if (s->service_id == service_id) {
			s->udta = udta;
			return;
		}
	}
}

GF_EXPORT
void *gf_atsc3_dmx_get_service_udta(GF_ATSCDmx *atscd, u32 service_id)
{
	u32 i=0;
	GF_ATSCService *s=NULL;
	while ((s = gf_list_enum(atscd->services, &i))) {
		if (s->service_id == service_id) {
			return s->udta;
		}
	}
	return NULL;
}

GF_EXPORT
u64 gf_atsc3_dmx_get_first_packet_time(GF_ATSCDmx *atscd)
{
	return atscd ? atscd->first_pck_time : 0;
}

GF_EXPORT
u64 gf_atsc3_dmx_get_last_packet_time(GF_ATSCDmx *atscd)
{
	return atscd ? atscd->last_pck_time : 0;
}

GF_EXPORT
u64 gf_atsc3_dmx_get_nb_packets(GF_ATSCDmx *atscd)
{
	return atscd ? atscd->nb_packets : 0;
}

GF_EXPORT
u64 gf_atsc3_dmx_get_recv_bytes(GF_ATSCDmx *atscd)
{
	return atscd ? atscd->total_bytes_recv : 0;
}

GF_EXPORT
void gf_atsc3_dmx_debug_tsi(GF_ATSCDmx *atscd, u32 tsi)
{
	if (atscd) atscd->debug_tsi = tsi;
}

#endif /* GPAC_DISABLE_ATSC */
