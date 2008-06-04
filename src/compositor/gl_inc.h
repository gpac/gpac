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


#ifndef GL_TEXTURE0_ARB

#define 	GL_TEXTURE0_ARB   0x84C0
#define 	GL_TEXTURE1_ARB   0x84C1
#define 	GL_TEXTURE2_ARB   0x84C2
#define 	GL_TEXTURE3_ARB   0x84C3
#define 	GL_TEXTURE4_ARB   0x84C4
#define 	GL_TEXTURE5_ARB   0x84C5
#define 	GL_TEXTURE6_ARB   0x84C6
#define 	GL_TEXTURE7_ARB   0x84C7
#define 	GL_TEXTURE8_ARB   0x84C8
#define 	GL_TEXTURE9_ARB   0x84C9
#define 	GL_TEXTURE10_ARB   0x84CA
#define 	GL_TEXTURE11_ARB   0x84CB
#define 	GL_TEXTURE12_ARB   0x84CC
#define 	GL_TEXTURE13_ARB   0x84CD
#define 	GL_TEXTURE14_ARB   0x84CE
#define 	GL_TEXTURE15_ARB   0x84CF
#define 	GL_TEXTURE16_ARB   0x84D0
#define 	GL_TEXTURE17_ARB   0x84D1
#define 	GL_TEXTURE18_ARB   0x84D2
#define 	GL_TEXTURE19_ARB   0x84D3
#define 	GL_TEXTURE20_ARB   0x84D4
#define 	GL_TEXTURE21_ARB   0x84D5
#define 	GL_TEXTURE22_ARB   0x84D6
#define 	GL_TEXTURE23_ARB   0x84D7
#define 	GL_TEXTURE24_ARB   0x84D8
#define 	GL_TEXTURE25_ARB   0x84D9
#define 	GL_TEXTURE26_ARB   0x84DA
#define 	GL_TEXTURE27_ARB   0x84DB
#define 	GL_TEXTURE28_ARB   0x84DC
#define 	GL_TEXTURE29_ARB   0x84DD
#define 	GL_TEXTURE30_ARB   0x84DE
#define 	GL_TEXTURE31_ARB   0x84DF
#define 	GL_ACTIVE_TEXTURE_ARB   0x84E0
#define 	GL_CLIENT_ACTIVE_TEXTURE_ARB   0x84E1
#define 	GL_MAX_TEXTURE_UNITS_ARB   0x84E2
#define 	GL_EXT_abgr   1
#define 	GL_EXT_blend_color   1
#define 	GL_EXT_blend_minmax   1
#define 	GL_EXT_blend_subtract   1
#define 	GL_EXT_texture_env_combine   1
#define 	GL_EXT_texture_env_add   1
#define 	GL_ABGR_EXT   0x8000
#define 	GL_CONSTANT_COLOR_EXT   0x8001
#define 	GL_ONE_MINUS_CONSTANT_COLOR_EXT   0x8002
#define 	GL_CONSTANT_ALPHA_EXT   0x8003
#define 	GL_ONE_MINUS_CONSTANT_ALPHA_EXT   0x8004
#define 	GL_BLEND_COLOR_EXT   0x8005
#define 	GL_FUNC_ADD_EXT   0x8006
#define 	GL_MIN_EXT   0x8007
#define 	GL_MAX_EXT   0x8008
#define 	GL_BLEND_EQUATION_EXT   0x8009
#define 	GL_FUNC_SUBTRACT_EXT   0x800A
#define 	GL_FUNC_REVERSE_SUBTRACT_EXT   0x800B
#define 	GL_COMBINE_EXT   0x8570
#define 	GL_COMBINE_RGB_EXT   0x8571
#define 	GL_COMBINE_ALPHA_EXT   0x8572
#define 	GL_RGB_SCALE_EXT   0x8573
#define 	GL_ADD_SIGNED_EXT   0x8574
#define 	GL_INTERPOLATE_EXT   0x8575
#define 	GL_CONSTANT_EXT   0x8576
#define 	GL_PRIMARY_COLOR_EXT   0x8577
#define 	GL_PREVIOUS_EXT   0x8578
#define 	GL_SOURCE0_RGB_EXT   0x8580
#define 	GL_SOURCE1_RGB_EXT   0x8581
#define 	GL_SOURCE2_RGB_EXT   0x8582
#define 	GL_SOURCE0_ALPHA_EXT   0x8588
#define 	GL_SOURCE1_ALPHA_EXT   0x8589
#define 	GL_SOURCE2_ALPHA_EXT   0x858A
#define 	GL_OPERAND0_RGB_EXT   0x8590
#define 	GL_OPERAND1_RGB_EXT   0x8591
#define 	GL_OPERAND2_RGB_EXT   0x8592
#define 	GL_OPERAND0_ALPHA_EXT   0x8598
#define 	GL_OPERAND1_ALPHA_EXT   0x8599
#define 	GL_OPERAND2_ALPHA_EXT   0x859A
#define 	GL_LOGIC_OP   GL_INDEX_LOGIC_OP
#define 	GL_TEXTURE_COMPONENTS   GL_TEXTURE_INTERNAL_FORMAT

#endif

#ifndef COMBINE_RGB_ARB
#define COMBINE_RGB_ARB                                 0x8571
#define COMBINE_ALPHA_ARB                               0x8572
#define INTERPOLATE_ARB                                 0x8575
#define COMBINE_ARB                                     0x8570
#define SOURCE0_RGB_ARB                                 0x8580
#define SOURCE1_RGB_ARB                                 0x8581
#define SOURCE2_RGB_ARB                                 0x8582
#define GL_INTERPOLATE_ARB                              0x8575
#define GL_OPERAND0_RGB_ARB                             0x8590
#define GL_OPERAND1_RGB_ARB                             0x8591
#define GL_OPERAND2_RGB_ARB                             0x8592
#define GL_OPERAND0_ALPHA_ARB                           0x8598
#define GL_OPERAND1_ALPHA_ARB                           0x8599
#define GL_ADD_SIGNED_ARB  								0x8574
#define GL_SUBTRACT_ARB  								0x84E7
#define GL_SOURCE0_ALPHA_ARB  							0x8588
#define GL_SOURCE1_ALPHA_ARB 	0x8589 	
#define GL_SOURCE2_ALPHA_ARB 	0x858A 	
#endif



#endif	/*GPAC_DISABLE_3D*/

#endif	/*_GL_INC_H_*/

