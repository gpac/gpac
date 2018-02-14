/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / ATSC input module
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
#include <gpac/thread.h>
#include <gpac/modules/service.h>

#include <time.h>

typedef struct
{
	GF_ClientService *service;
	GF_InputService *input;

	GF_DownloadManager *dm;

	GF_Thread *th;
	u32 state;
	char *clock_init_seg;
	GF_ATSCDmx *atsc_dmx;
	DownloadedCacheEntry mpd_cache_entry;
	u32 tune_service_id;

	u32 sync_tsi, last_toi;
} ATSCIn;


static Bool ATSCIn_CanHandleURL(GF_InputService *plug, const char *url)
{
	if (!strnicmp(url, "atsc://", 7)) return GF_TRUE;
	return GF_FALSE;
}


static void ATSCIn_del(ATSCIn *atscd)
{
	if (atscd->th) gf_th_del(atscd->th);
	if (atscd->clock_init_seg) gf_free(atscd->clock_init_seg);
	if (atscd->atsc_dmx) gf_atsc_dmx_del(atscd->atsc_dmx);

	gf_free(atscd);
}

static ATSCIn * ATSCIn_new()
{
	ATSCIn *atscd;
	GF_SAFEALLOC(atscd, ATSCIn);
	atscd->th = gf_th_new("ATSCDmx");
	return atscd;
}

const DownloadedCacheEntry gf_dm_add_cache_entry(GF_DownloadManager *dm, const char *szURL, char *data, u64 size, u64 start_range, u64 end_range,  const char *mime, Bool clone_memory);

GF_Err gf_dm_force_headers(GF_DownloadManager *dm, const DownloadedCacheEntry entry, const char *headers);

void ATSCIn_on_event(void *udta, GF_ATSCEventType evt, u32 evt_param, const char *filename, char *data, u32 size, u32 tsi, u32 toi)
{
	char szPath[GF_MAX_PATH];
	ATSCIn *atscd = (ATSCIn *)udta;
	u32 nb_obj;
	Bool is_init = GF_TRUE;

	switch (evt) {
	case GF_ATSC_EVT_SERVICE_FOUND:
		if (!atscd->tune_service_id) {
			atscd->tune_service_id = evt_param;
			gf_atsc_tune_in(atscd->atsc_dmx, atscd->tune_service_id);
		}
		break;
	case GF_ATSC_EVT_SERVICE_SCAN:
		if (atscd->tune_service_id && !gf_atsc_dmx_find_service(atscd->atsc_dmx, atscd->tune_service_id)) {

			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ATSCDmx] Asked to tune to service %d but no such service, tuning to first one\n", atscd->tune_service_id));

			atscd->tune_service_id = 0;
			gf_atsc_tune_in(atscd->atsc_dmx, (u32) -2);
		}
		break;
	case GF_ATSC_EVT_MPD:
	{
		GF_ObjectDescriptor *od = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
		od->ServiceID = evt_param;

		sprintf(szPath, "http://gpatsc/service%d/%s", evt_param, filename);
		od->URLString = gf_strdup(szPath);
		atscd->mpd_cache_entry = gf_dm_add_cache_entry(atscd->dm, szPath, data, size, 0, 0, "application/dash+xml", GF_FALSE);

		sprintf(szPath, "x-dash-atsc: %d\r\n", evt_param);
		gf_dm_force_headers(atscd->dm, atscd->mpd_cache_entry, szPath);

		gf_service_declare_media(atscd->service, (GF_Descriptor *) od, GF_TRUE);

		atscd->sync_tsi = 0;
		atscd->last_toi = 0;
		if (atscd->clock_init_seg) gf_free(atscd->clock_init_seg);
		atscd->clock_init_seg = NULL;
	}
		break;
	case GF_ATSC_EVT_SEG:
		if (atscd->mpd_cache_entry) {
			if (!atscd->clock_init_seg) atscd->clock_init_seg = gf_strdup(filename);
			sprintf(szPath, "x-dash-atsc: %d\r\nx-dash-first-seg: %s\r\n", evt_param, atscd->clock_init_seg);
			gf_dm_force_headers(atscd->dm, atscd->mpd_cache_entry, szPath);
		}
		is_init = GF_FALSE;
		if (!atscd->sync_tsi) {
			atscd->sync_tsi = tsi;
			atscd->last_toi = toi;
		} else if (atscd->sync_tsi == tsi) {
			if (atscd->last_toi > toi) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ATSCDmx] Loop detected on service %d for TSI %u: prev TOI %u this toi %u\n", atscd->tune_service_id, tsi, atscd->last_toi, toi));
				if (atscd->mpd_cache_entry) {
					if (atscd->clock_init_seg) gf_free(atscd->clock_init_seg);
					atscd->clock_init_seg = gf_strdup(filename);
					sprintf(szPath, "x-dash-atsc: %d\r\nx-dash-first-seg: %s\r\nx-atsc-loop: yes\r\n", evt_param, atscd->clock_init_seg);
					gf_dm_force_headers(atscd->dm, atscd->mpd_cache_entry, szPath);
				}
			}
			atscd->last_toi = toi;
		}
		//fallthrough

	case GF_ATSC_EVT_INIT_SEG:

		sprintf(szPath, "http://gpatsc/service%d/%s", evt_param, filename);
		//we copy over the init segment, but only share the data pointer for segments

		gf_dm_add_cache_entry(atscd->dm, szPath, data, size, 0, 0, "video/mp4", is_init ? GF_TRUE : GF_FALSE);

		nb_obj = gf_atsc_dmx_get_object_count(atscd->atsc_dmx, evt_param);
		if (nb_obj>10) {
			while (nb_obj>10) {
				gf_atsc_dmx_remove_first_object(atscd->atsc_dmx, evt_param);
				nb_obj = gf_atsc_dmx_get_object_count(atscd->atsc_dmx, evt_param);
			}
		}
		break;
	default:
		break;
	}
}

static Bool ATSCIn_LocalCacheCbk(void *par, char *url, Bool is_destroy)
{
	ATSCIn *atscd = (ATSCIn *)par;
	if (strncmp(url, "http://gpatsc/service", 21)) return GF_FALSE;
	if (is_destroy) {
		u32 sid=0;
		char *subr = strchr(url+21, '/');
		subr[0] = 0;
		sid = atoi(url+21);
		subr[0] = '/';
		gf_atsc_dmx_remove_object_by_name(atscd->atsc_dmx, sid, subr+1, GF_TRUE);
	}
	return GF_TRUE;
}

static u32 ATSCIn_Run(void *par)
{
	ATSCIn *atscd = (ATSCIn *)par;

	gf_service_connect_ack(atscd->service, NULL, GF_OK);
	gf_atsc_set_callback(atscd->atsc_dmx, ATSCIn_on_event, atscd);
	if (atscd->tune_service_id)
		gf_atsc_tune_in(atscd->atsc_dmx, atscd->tune_service_id);

	while (atscd->state==1) {
		Bool is_empty = GF_TRUE;
		GF_Err e = gf_atsc_dmx_process(atscd->atsc_dmx);
		if (e != GF_IP_NETWORK_EMPTY) is_empty = GF_FALSE;
		gf_atsc_dmx_process_services(atscd->atsc_dmx);
		if (e != GF_IP_NETWORK_EMPTY) is_empty = GF_FALSE;

		if (is_empty) gf_sleep(1);
	}
	atscd->state = 3;
	return 0;
}

GF_DownloadManager *gf_term_service_get_dm(GF_ClientService *ns);

GF_Err gf_dm_set_localcache_provider(GF_DownloadManager *dm, Bool (*local_cache_url_provider_cbk)(void *udta, char *url, Bool is_cache_destroy), void *lc_udta);


static GF_Err ATSCIn_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char *opts;
	ATSCIn *atscd = plug->priv;
	atscd->service = serv;
	if (!atscd->atsc_dmx) {
		const char *ifce = gf_modules_get_option((GF_BaseInterface*)plug, "Network", "DefaultMCastInterface");
		atscd->atsc_dmx = gf_atsc_dmx_new(ifce, NULL, 0);

		atscd->dm = gf_term_service_get_dm(serv);
		if (!atscd->dm) return GF_SERVICE_ERROR;
		gf_dm_set_localcache_provider(atscd->dm, ATSCIn_LocalCacheCbk, atscd);
	}
	opts = (char *)url + 7;
	if (opts[0]==0) opts = NULL;
	while (opts) {
		char *sep = strchr(opts, ':');
		if (sep) sep[0] = 0;
		if (!strncmp(opts, "service=", 8)) atscd->tune_service_id = atoi(opts+8);
		if (!sep) break;
		sep[0] = ':';
		opts = sep+1;
	}
	atscd->state = 1;
	gf_th_run(atscd->th, ATSCIn_Run, atscd);
	return GF_OK;
}

static GF_Err ATSCIn_CloseService(GF_InputService *plug)
{
	ATSCIn *atscd = plug->priv;
	if (atscd->state<2) atscd->state = 2;
	while (atscd->state==2) gf_sleep(1);
	gf_service_disconnect_ack(atscd->service, NULL, GF_OK);
	return GF_OK;
}

static GF_Descriptor *ATSCIn_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	return NULL;
}

static GF_Err ATSCIn_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	return GF_SERVICE_ERROR;
}

static GF_Err ATSCIn_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	return GF_SERVICE_ERROR;
}

static GF_Err ATSCIn_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	return GF_SERVICE_ERROR;
}


GF_InputService *ATSCIn_Load()
{

	GF_InputService *plug = gf_malloc(sizeof(GF_InputService));
	memset(plug, 0, sizeof(GF_InputService));
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC ATSC3 Input", "gpac distribution")

	plug->CanHandleURL = ATSCIn_CanHandleURL;
	plug->ConnectService = ATSCIn_ConnectService;
	plug->CloseService = ATSCIn_CloseService;
	plug->GetServiceDescriptor = ATSCIn_GetServiceDesc;
	plug->ConnectChannel = ATSCIn_ConnectChannel;
	plug->DisconnectChannel = ATSCIn_DisconnectChannel;
	plug->ServiceCommand = ATSCIn_ServiceCommand;

	plug->priv = ATSCIn_new();
	return plug;
}

void ATSCIn_Delete(void *ifce)
{
	ATSCIn *read;
	GF_InputService *plug = (GF_InputService *) ifce;
	if (!plug)
		return;
	read = plug->priv;
	if (read) {
		ATSCIn_del(read);
		plug->priv = NULL;
	}
	gf_free(plug);
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
	static u32 si [] = {
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_NET_CLIENT_INTERFACE,
#endif
		0
	};

	return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	if (InterfaceType == GF_NET_CLIENT_INTERFACE) return (GF_BaseInterface *)ATSCIn_Load();
#endif
	return NULL;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *ifce)
{
	switch (ifce->InterfaceType) {
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_NET_CLIENT_INTERFACE:
		ATSCIn_Delete(ifce);
		break;
#endif
	}
}

GPAC_MODULE_STATIC_DECLARATION( atsc_in )

