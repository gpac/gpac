/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Compositor sub-project
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

#ifndef _GL_INC_H_
#define _GL_INC_H_

#ifdef WIN32
#include <windows.h>
#endif

#ifndef GPAC_DISABLE_3D

#ifdef GPAC_USE_OGL_ES

#ifndef GPAC_FIXED_POINT
#error "OpenGL ES defined without fixed-point support - unsupported."
#endif
#include "GLES/egl.h"

#else


#ifndef GPAC_USE_TINYGL
#include <GL/gl.h>
#else
#include "../../../TinyGL/include/GL/gl.h"
#endif


#ifdef GPAC_HAS_GLU
#include <GL/glu.h>
#endif

#endif

/*redefine all ext needed*/

/*BGRA pixel format*/
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif

#ifndef GL_BGR_EXT
#define GL_BGR_EXT 0x80E0
#endif

/*rectangle texture (non-pow2) - same as GL_TEXTURE_RECTANGLE_NV*/
#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT 0x84F5
#endif

#ifndef GL_RESCALE_NORMAL
#define GL_RESCALE_NORMAL 0x803A
#endif

#ifndef YCBCR_MESA
#define YCBCR_MESA	0x8757
#endif

#ifndef YCBCR_422_APPLE
#define YCBCR_422_APPLE			0x85B9
#endif

#ifndef UNSIGNED_SHORT_8_8_MESA
#define UNSIGNED_SHORT_8_8_MESA      0x85BA /* same as Apple's */
#define UNSIGNED_SHORT_8_8_REV_MESA  0x85BB /* same as Apple's */
#endif


#endif	/*GPAC_DISABLE_3D*/

#endif	/*_GL_INC_H_*/

