/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2006-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG2-TS sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the gf_free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *   
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the gf_free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 */


#ifndef _GF_DVB_MPE_H_
#define _GF_DVB_MPE_H_

#include <gpac/mpegts.h>
#include <string.h>

#ifndef GPAC_DISABLE_MPEG2TS

typedef struct tag_m2ts_section_mpe GF_M2TS_SECTION_MPE;
typedef struct _sock_entry GF_SOCK_ENTRY;

void gf_dvb_mpe_init(GF_M2TS_Demuxer *ts);
void gf_dvb_mpe_shutdown(GF_M2TS_Demuxer *ts);
GF_M2TS_ES *gf_dvb_mpe_section_new();
void gf_dvb_mpe_section_del(GF_M2TS_ES *es);
void gf_m2ts_print_mpe_info(GF_M2TS_Demuxer *ts);

#endif //GPAC_DISABLE_MPEG2TS

#endif	//_GF_DVB_MPE_H_
