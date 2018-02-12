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

GF_Err gf_atsc_dmx_process_services(GF_ATSCDmx *atscd);

#ifdef __cplusplus
}
#endif

#endif	//_GF_ATSC_H_

