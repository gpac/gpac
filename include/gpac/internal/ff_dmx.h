/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Deniz Ugur
 *			Copyright (c) Motion Spell 2025
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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
#ifndef _GF_FF_DMX_H_
#define _GF_FF_DMX_H_

#include <gpac/setup.h>

#ifdef GPAC_HAS_FFMPEG

#include <gpac/tools.h>
#include <gpac/filters.h>

#include <libavformat/avformat.h>

typedef enum
{
	GF_FFDMX_EOS = -2,
	GF_FFDMX_ERR = -1,
	GF_FFDMX_OK = 0,
	GF_FFDMX_HAS_MORE = 1
} GF_FFDemuxCallbackRet;

typedef GF_FFDemuxCallbackRet (*GF_FFDemuxCallbackFn)(void *udta, AVPacket **pkt);

GF_Err gf_filter_bind_ffdmx_callbacks(GF_Filter *filter, void *udta, GF_FFDemuxCallbackFn on_pkt);

#endif // GPAC_HAS_FFMPEG

#endif // _GF_FF_DMX_H_

