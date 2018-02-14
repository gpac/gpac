/*
 * Copyright (c) TELECOM ParisTech 2011
 */

#ifndef _GF_ATSC_H_
#define _GF_ATSC_H_

#include <gpac/tools.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *	\file <gpac/atsc.h>
 *	Specific extensions for ATSC ROUTE demux
 */

typedef struct __gf_atscdmx GF_ATSCDmx;

GF_Err gf_atsc_dmx_process(GF_ATSCDmx *atscd);

void gf_atsc_dmx_del(GF_ATSCDmx *atscd);

GF_ATSCDmx *gf_atsc_dmx_new(const char *ifce, const char *dir, u32 sock_buffer_size);

GF_Err gf_atsc_tune_in(GF_ATSCDmx *atscd, u32 serviceID);

GF_Err gf_atsc_set_max_objects_store(GF_ATSCDmx *atscd, u32 max_segs);

GF_Err gf_atsc_dmx_process_services(GF_ATSCDmx *atscd);

typedef enum
{
	//new service detected, ID is in evt_param
	GF_ATSC_EVT_SERVICE_FOUND = 0,
	//service scan completed
	GF_ATSC_EVT_SERVICE_SCAN,
	//new MPD available for service,  service ID is in evt_param
	GF_ATSC_EVT_MPD,
	//init segment update,  service ID is in evt_param
	GF_ATSC_EVT_INIT_SEG,
	//segment reception,  service ID is in evt_param
	GF_ATSC_EVT_SEG,
} GF_ATSCEventType;

GF_Err gf_atsc_set_callback(GF_ATSCDmx *atscd, void (*on_event)(void *udta, GF_ATSCEventType evt, u32 evt_param, const char *filename, char *data, u32 size, u32 tsi, u32 toi), void *udta);

u32 gf_atsc_dmx_get_object_count(GF_ATSCDmx *atscd, u32 service_id);

void gf_atsc_dmx_remove_object_by_name(GF_ATSCDmx *atscd, u32 service_id, char *fileName, Bool purge_previous);

void gf_atsc_dmx_remove_first_object(GF_ATSCDmx *atscd, u32 service_id);

Bool gf_atsc_dmx_find_service(GF_ATSCDmx *atscd, u32 service_id);

#ifdef __cplusplus
}
#endif

#endif	//_GF_ATSC_H_

