/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
#include <gpac/internal/compositor_dev.h>

#ifdef GPAC_USE_OGL_ES

#ifndef GPAC_FIXED_POINT
#error "OpenGL ES defined without fixed-point support - unsupported."
#endif

#ifdef GPAC_ANDROID
#include "GLES/gl.h"
#else
#ifdef GPAC_IPHONE
#include "OpenGLES/ES1/gl.h"
#include "OpenGLES/ES1/glext.h"
#include "glues.h"
#else
#include "GLES/egl.h"
#endif
#endif

#if defined(GPAC_HAS_GLU) && !defined (GPAC_IPHONE)
/*WARNING - this is NOT a standard include, GLU is not supported by GLES*/
#include <GLES/glu.h>
#endif


#ifndef EGL_VERSION_1_0
#define EGL_VERSION_1_0		1
#endif

#ifndef EGL_VERSION_1_1
#ifdef GL_OES_VERSION_1_1
#define EGL_VERSION_1_1		1
#endif
#endif

#elif defined (CONFIG_DARWIN_GL)

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#else


#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#ifdef GPAC_HAS_GLU
#include <GL/glu.h>
#endif

#endif


#define GL_CHECK_ERR  {s32 res = glGetError(); if (res) GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("GL Error %d file %s line %d\n", res, __FILE__, __LINE__)); }

/*macros for GL proto and fun declaration*/
#ifdef _WIN32_WCE
#define GLAPICAST *
#elif defined(WIN32)
#include <windows.h>
#define GLAPICAST APIENTRY *
#else
#define GLAPICAST *
#endif

#define GLDECL(ret, funname, args)	\
typedef ret (GLAPICAST proc_ ## funname)args;	\
extern proc_ ## funname funname;	\
 
#define GLDECL_STATIC(funname) proc_ ## funname funname = NULL

#if defined GPAC_USE_TINYGL
//no extensions with TinyGL
#elif defined (GPAC_USE_OGL_ES)
//no extensions with OpenGL ES
#elif defined(WIN32) || defined (GPAC_CONFIG_WIN32)
#define LOAD_GL_FUNCS
#define GET_GLFUN(funname) funname = (proc_ ## funname) wglGetProcAddress(#funname)
#elif defined(CONFIG_DARWIN_GL)
extern void (*glutGetProcAddress(const GLubyte *procname))( void );
#define GET_GLFUN(funname) funname = (proc_ ## funname) glutGetProcAddress(#funname)
#else
#define LOAD_GL_FUNCS
extern void (*glXGetProcAddress(const GLubyte *procname))( void );
#define GET_GLFUN(funname) funname = (proc_ ## funname) glXGetProcAddress(#funname)
#endif


#if !defined(GPAC_USE_OGL_ES)

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

/* for triscope - not conformant*/
#ifndef GL_RGBDS
#define GL_RGBDS 0x1910
#endif

#ifndef GL_TEXTURE_RECTANGLE_NV
#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE_NV 0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE_NV 0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE_NV 0x84F8
#endif

#ifndef GL_POINT_SIZE_MIN_EXT
#define GL_POINT_SIZE_MIN_EXT               0x8126
#define GL_POINT_SIZE_MAX_EXT               0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_EXT    0x8128
#define GL_DISTANCE_ATTENUATION_EXT         0x8129
#endif


#ifndef GL_VERSION_1_3

#ifdef LOAD_GL_FUNCS
#define LOAD_GL_1_3
#endif

#define 	GL_TEXTURE0   0x84C0
#define 	GL_TEXTURE1   0x84C1
#define 	GL_TEXTURE2   0x84C2
#define 	GL_TEXTURE3   0x84C3
#define 	GL_TEXTURE4   0x84C4
#define 	GL_TEXTURE5   0x84C5
#define 	GL_TEXTURE6   0x84C6
#define 	GL_TEXTURE7   0x84C7
#define 	GL_TEXTURE8   0x84C8
#define 	GL_TEXTURE9   0x84C9
#define 	GL_TEXTURE10   0x84CA
#define 	GL_TEXTURE11   0x84CB
#define 	GL_TEXTURE12   0x84CC
#define 	GL_TEXTURE13   0x84CD
#define 	GL_TEXTURE14   0x84CE
#define 	GL_TEXTURE15   0x84CF
#define 	GL_TEXTURE16   0x84D0
#define 	GL_TEXTURE17   0x84D1
#define 	GL_TEXTURE18   0x84D2
#define 	GL_TEXTURE19   0x84D3
#define 	GL_TEXTURE20   0x84D4
#define 	GL_TEXTURE21   0x84D5
#define 	GL_TEXTURE22   0x84D6
#define 	GL_TEXTURE23   0x84D7
#define 	GL_TEXTURE24   0x84D8
#define 	GL_TEXTURE25   0x84D9
#define 	GL_TEXTURE26   0x84DA
#define 	GL_TEXTURE27   0x84DB
#define 	GL_TEXTURE28   0x84DC
#define 	GL_TEXTURE29   0x84DD
#define 	GL_TEXTURE30   0x84DE
#define 	GL_TEXTURE31   0x84DF
#define 	GL_ACTIVE_TEXTURE   0x84E0
#define 	GL_CLIENT_ACTIVE_TEXTURE   0x84E1
#define 	GL_MAX_TEXTURE_UNITS   0x84E2
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

#ifndef GL_LOGIC_OP
#define 	GL_LOGIC_OP   GL_INDEX_LOGIC_OP
#endif

#ifndef GL_TEXTURE_COMPONENTS
#define 	GL_TEXTURE_COMPONENTS   GL_TEXTURE_INTERNAL_FORMAT
#endif


#define GL_COMBINE_RGB                                 0x8571
#define GL_COMBINE_ALPHA                               0x8572
#define GL_INTERPOLATE                                 0x8575
#define GL_COMBINE                                     0x8570
#define GL_SOURCE0_RGB                                 0x8580
#define GL_SOURCE1_RGB                                 0x8581
#define GL_SOURCE2_RGB                                 0x8582
#define GL_INTERPOLATE                              0x8575
#define GL_OPERAND0_RGB                             0x8590
#define GL_OPERAND1_RGB                             0x8591
#define GL_OPERAND2_RGB                             0x8592
#define GL_OPERAND0_ALPHA                           0x8598
#define GL_OPERAND1_ALPHA                           0x8599
#define GL_ADD_SIGNED  								0x8574
#define GL_SUBTRACT  								0x84E7
#define GL_SOURCE0_ALPHA  							0x8588
#define GL_SOURCE1_ALPHA						 	0x8589
#define GL_SOURCE2_ALPHA						 	0x858A


GLDECL(void, glActiveTexture, (GLenum texture) )
GLDECL(void, glClientActiveTexture, (GLenum texture) )


GLDECL(void, glBlendEquation, (GLint mode) )
#endif	//GL_VERSION_1_3

#ifndef GL_VERSION_1_4

#ifdef LOAD_GL_FUNCS
#define LOAD_GL_1_4
#endif

#define FUNC_ADD_EXT	0x8006
#define MIN_EXT	0x8007
#define MAX_EXT	0x8008
#define BLEND_EQUATION_EXT	0x8009
#define GL_BLEND_EQUATION_RGB GL_BLEND_EQUATION


GLDECL(void, glPointParameterf, (GLenum , GLfloat) )
GLDECL(void, glPointParameterfv, (GLenum, const GLfloat *) )

#endif



#ifndef GL_VERSION_1_5

#ifdef LOAD_GL_FUNCS
#define LOAD_GL_1_5
#endif

#define GL_ARRAY_BUFFER	0x8892
#define GL_STREAM_DRAW	0x88E0
#define GL_STATIC_DRAW	0x88E4
#define GL_DYNAMIC_DRAW 0x88E8

GLDECL(void, glGenBuffers, (GLsizei , GLuint *) )
GLDECL(void, glDeleteBuffers, (GLsizei , GLuint *) )
GLDECL(void, glBindBuffer, (GLenum, GLuint ) )
GLDECL(void, glBufferData, (GLenum, int, void *, GLenum) )
GLDECL(void, glBufferSubData, (GLenum, int, int, void *) )
GLDECL(void *, glMapBuffer, (GLenum, GLenum) )
GLDECL(void *, glUnmapBuffer, (GLenum) )

#endif	//GL_VERSION_1_5


#ifndef GL_VERSION_2_0

#ifdef LOAD_GL_FUNCS
#define LOAD_GL_2_0
#endif

#define GL_VERTEX_ATTRIB_ARRAY_ENABLED 0x8622
#define GL_VERTEX_ATTRIB_ARRAY_SIZE 0x8623
#define GL_VERTEX_ATTRIB_ARRAY_STRIDE 0x8624
#define GL_VERTEX_ATTRIB_ARRAY_TYPE 0x8625
#define GL_CURRENT_VERTEX_ATTRIB 0x8626
#define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
#define GL_VERTEX_PROGRAM_TWO_SIDE 0x8643
#define GL_VERTEX_ATTRIB_ARRAY_POINTER 0x8645
#define GL_STENCIL_BACK_FUNC 0x8800
#define GL_STENCIL_BACK_FAIL 0x8801
#define GL_STENCIL_BACK_PASS_DEPTH_FAIL 0x8802
#define GL_STENCIL_BACK_PASS_DEPTH_PASS 0x8803
#define GL_MAX_DRAW_BUFFERS 0x8824
#define GL_DRAW_BUFFER0 0x8825
#define GL_DRAW_BUFFER1 0x8826
#define GL_DRAW_BUFFER2 0x8827
#define GL_DRAW_BUFFER3 0x8828
#define GL_DRAW_BUFFER4 0x8829
#define GL_DRAW_BUFFER5 0x882A
#define GL_DRAW_BUFFER6 0x882B
#define GL_DRAW_BUFFER7 0x882C
#define GL_DRAW_BUFFER8 0x882D
#define GL_DRAW_BUFFER9 0x882E
#define GL_DRAW_BUFFER10 0x882F
#define GL_DRAW_BUFFER11 0x8830
#define GL_DRAW_BUFFER12 0x8831
#define GL_DRAW_BUFFER13 0x8832
#define GL_DRAW_BUFFER14 0x8833
#define GL_DRAW_BUFFER15 0x8834
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_POINT_SPRITE 0x8861
#define GL_COORD_REPLACE 0x8862
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_VERTEX_ATTRIB_ARRAY_NORMALIZED 0x886A
#define GL_MAX_TEXTURE_COORDS 0x8871
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS 0x8B49
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS 0x8B4A
#define GL_MAX_VARYING_FLOATS 0x8B4B
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 0x8B4D
#define GL_SHADER_TYPE 0x8B4F
#define GL_FLOAT_VEC2 0x8B50
#define GL_FLOAT_VEC3 0x8B51
#define GL_FLOAT_VEC4 0x8B52
#define GL_INT_VEC2 0x8B53
#define GL_INT_VEC3 0x8B54
#define GL_INT_VEC4 0x8B55
#define GL_BOOL 0x8B56
#define GL_BOOL_VEC2 0x8B57
#define GL_BOOL_VEC3 0x8B58
#define GL_BOOL_VEC4 0x8B59
#define GL_FLOAT_MAT2 0x8B5A
#define GL_FLOAT_MAT3 0x8B5B
#define GL_FLOAT_MAT4 0x8B5C
#define GL_SAMPLER_1D 0x8B5D
#define GL_SAMPLER_2D 0x8B5E
#define GL_SAMPLER_3D 0x8B5F
#define GL_SAMPLER_CUBE 0x8B60
#define GL_SAMPLER_1D_SHADOW 0x8B61
#define GL_SAMPLER_2D_SHADOW 0x8B62
#define GL_DELETE_STATUS 0x8B80
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VALIDATE_STATUS 0x8B83
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ATTACHED_SHADERS 0x8B85
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_UNIFORM_MAX_LENGTH 0x8B87
#define GL_SHADER_SOURCE_LENGTH 0x8B88
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_ACTIVE_ATTRIBUTE_MAX_LENGTH 0x8B8A
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT 0x8B8B
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_POINT_SPRITE_COORD_ORIGIN 0x8CA0
#define GL_LOWER_LEFT 0x8CA1
#define GL_UPPER_LEFT 0x8CA2
#define GL_STENCIL_BACK_REF 0x8CA3
#define GL_STENCIL_BACK_VALUE_MASK 0x8CA4
#define GL_STENCIL_BACK_WRITEMASK 0x8CA5
#define GL_PIXEL_UNPACK_BUFFER_ARB   0x88EC
#define GL_STREAM_DRAW_ARB   0x88E0
#define GL_WRITE_ONLY_ARB   0x88B9
#define GL_DYNAMIC_DRAW_ARB   0x88E8


GLDECL(GLuint, glCreateProgram, (void) )
GLDECL(void, glDeleteProgram, (GLuint ) )
GLDECL(void, glLinkProgram, (GLuint program) )
GLDECL(void, glUseProgram, (GLuint program) )
GLDECL(GLuint, glCreateShader, (GLenum shaderType) )
GLDECL(void, glDeleteShader, (GLuint shader) )
GLDECL(void, glShaderSource, (GLuint shader, GLsizei count, const char **string, const GLint *length) )
GLDECL(void, glCompileShader, (GLuint shader) )
GLDECL(void, glAttachShader, (GLuint program, GLuint shader) )
GLDECL(void, glDetachShader, (GLuint program, GLuint shader) )
GLDECL(void, glGetShaderiv, (GLuint shader, GLenum type, GLint *res) )
GLDECL(void, glGetInfoLogARB, (GLuint shader, GLint size, GLsizei *rsize, const char *logs) )
GLDECL(GLint, glGetUniformLocation, (GLuint prog, const char *name) )
GLDECL(void, glUniform1f, (GLint location, GLfloat v0) )
GLDECL(void, glUniform2f, (GLint location, GLfloat v0, GLfloat v1) )
GLDECL(void, glUniform3f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2) )
GLDECL(void, glUniform4f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) )
GLDECL(void, glUniform1i, (GLint location, GLint v0) )
GLDECL(void, glUniform2i, (GLint location, GLint v0, GLint v1) )
GLDECL(void, glUniform3i, (GLint location, GLint v0, GLint v1, GLint v2) )
GLDECL(void, glUniform4i, (GLint location, GLint v0, GLint v1, GLint v2, GLint v3) )
GLDECL(void, glUniform1fv, (GLint location, GLsizei count, const GLfloat *value) )
GLDECL(void, glUniform2fv, (GLint location, GLsizei count, const GLfloat *value) )
GLDECL(void, glUniform3fv, (GLint location, GLsizei count, const GLfloat *value) )
GLDECL(void, glUniform4fv, (GLint location, GLsizei count, const GLfloat *value) )
GLDECL(void, glUniform1iv, (GLint location, GLsizei count, const GLint *value) )
GLDECL(void, glUniform2iv, (GLint location, GLsizei count, const GLint *value) )
GLDECL(void, glUniform3iv, (GLint location, GLsizei count, const GLint *value) )
GLDECL(void, glUniform4iv, (GLint location, GLsizei count, const GLint *value) )
GLDECL(void, glUniformMatrix2fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) )
GLDECL(void, glUniformMatrix3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) )
GLDECL(void, glUniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) )
GLDECL(void, glUniformMatrix2x3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat * value) )
GLDECL(void, glUniformMatrix3x2fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) )
GLDECL(void, glUniformMatrix2x4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) )
GLDECL(void, glUniformMatrix4x2fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) )
GLDECL(void, glUniformMatrix3x4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) )
GLDECL(void, glUniformMatrix4x3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) )



#endif //GL_VERSION_2_0

#endif //GPAC_USE_OGL_ES 

#endif	/*GPAC_DISABLE_3D*/

#endif	/*_GL_INC_H_*/

