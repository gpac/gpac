/*
*			GPAC - Multimedia Framework C SDK
*
*			Authors: Romain Bouqueau, Jean Le Feuvre
*			Copyright (c) 2014-2016 GPAC Licensing
*			Copyright (c) 2016-2020 Telecom Paris
*					All rights reserved
*
*  This file is part of GPAC / Dektec SDI video output filter
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

#ifndef _GF_DEKTECVID_H_
#define _GF_DEKTECVID_H_

#include <gpac/filters.h>


//#define FAKE_DT_API

#if defined(GPAC_HAS_DTAPI) && !defined(FAKE_DT_API)
#include <DTAPI.h>
#endif

#include <gpac/constants.h>
#include <gpac/color.h>
#include <gpac/thread.h>

#if defined(GPAC_HAS_DTAPI) && !defined(FAKE_DT_API)

#if defined(WIN32) && !defined(__GNUC__)
# include <intrin.h>
#else
#  include <emmintrin.h>
#endif

#endif


typedef struct {
	struct _dtout_ctx *ctx;
	GF_FilterPid *video; //null if audio callback
	GF_FilterPid *audio; //null if video callback
} DtCbkCtx;

typedef struct _dtout_ctx
{
	//opts
	s32 bus, slot;
	GF_Fraction fps;
	Bool clip;
	u32 port;
	Double start;

	u32 width, height, pix_fmt, stride, stride_uv, uv_height, nb_planes;
	GF_Fraction framerate;
	Bool is_sending, is_configured, is_10b, is_eos;

#if defined(GPAC_HAS_DTAPI) && !defined(FAKE_DT_API)
	DtMxProcess *dt_matrix;
	DtDevice *dt_device;
	DtCbkCtx audio_cbk, video_cbk;
#endif

	s64 frameNum;
	u64 init_clock, last_frame_time;

	u32 frame_dur, frame_scale;
	Bool needs_reconfigure;
} GF_DTOutCtx;


#if defined(GPAC_HAS_DTAPI)
GF_Err dtout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove);
GF_Err dtout_initialize(GF_Filter *filter);
void dtout_finalize(GF_Filter *filter);
GF_Err dtout_process(GF_Filter *filter);
#endif


#endif //_GF_DEKTECVID_H_

 
