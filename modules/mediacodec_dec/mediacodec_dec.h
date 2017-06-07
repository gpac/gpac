/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / codec pack module
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

#ifndef _MEDIACODEC_DEC_H_
#define _MEDIACODEC_DEC_H_
#include <gpac/modules/codec.h>
#include <gpac/thread.h>
#include <jni.h>
#include "../../src/compositor/gl_inc.h"

typedef struct {
	jobject oSurfaceTex;
	int texture_id;
} MC_SurfaceTexture;
	
GF_Err MCDec_CreateSurface (GLuint tex_id, ANativeWindow ** window, Bool * surface_rendering, MC_SurfaceTexture * surfaceTex);
GF_Err MCFrame_UpdateTexImage(MC_SurfaceTexture surfaceTex);
GF_Err MCFrame_GetTransformMatrix(GF_CodecMatrix * mx, MC_SurfaceTexture surfaceTex);
GF_Err MCDec_DeleteSurface(MC_SurfaceTexture surfaceTex);
u32 MCDec_BeforeExit(void * param);

#endif //_MEDIACODEC_DEC_H_

