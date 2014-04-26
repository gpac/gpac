/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / XIPH.org module
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

#ifndef OGG_IN_H
#define OGG_IN_H

#include <gpac/modules/service.h>
#include <gpac/modules/codec.h>
#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/thread.h>

GF_InputService *OGG_LoadDemux();
void OGG_DeleteDemux(void *ifce);


enum
{
	OGG_VORBIS = 1,
	OGG_SPEEX,
	OGG_FLAC,
	OGG_THEORA
};

typedef struct
{
	u32 type;
	void *opaque;
} OGGWraper;

#ifdef GPAC_HAS_VORBIS
u32 NewVorbisDecoder(GF_BaseDecoder *ifcd);
void DeleteVorbisDecoder(GF_BaseDecoder *ifcg);
#endif

#ifdef GPAC_HAS_THEORA
u32 NewTheoraDecoder(GF_BaseDecoder *ifcd);
void DeleteTheoraDecoder(GF_BaseDecoder *ifcg);
#endif

#endif

