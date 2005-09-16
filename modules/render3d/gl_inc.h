/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / 3D rendering module
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

#ifndef GL_INC_H_
#define GL_INC_H_

#ifdef WIN32
#include <windows.h>
#endif

#ifdef GPAC_USE_OGL_ES
#ifndef GPAC_FIXED_POINT
#error "OpenGL ES defined without fixed-point support - unsupported."
#endif
#include "GLES/egl.h"
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

/*redefine all ext needed*/

/*BGRA pixel format*/
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

#ifndef GL_BGR_EXT
#define GL_BGR_EXT 0x80E0
#endif

/*rectangle texture (non-pow2) - GL_TEXTURE_RECTANGLE_NV*/
#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT 0x84F5
#endif

#ifndef GL_RESCALE_NORMAL
#define GL_RESCALE_NORMAL 0x803A
#endif

#endif

