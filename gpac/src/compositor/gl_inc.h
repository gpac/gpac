/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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
# include <windows.h>
#endif


#ifndef _GF_SETUP_H_
# error "Missing gpac/setup.h include"
#endif

#ifndef GPAC_DISABLE_3D

#ifdef GPAC_USE_GLES1X
# ifdef GPAC_CONFIG_ANDROID
#  include "GLES/gl.h"
# elif defined(GPAC_CONFIG_IOS)
#  include "OpenGLES/ES1/gl.h"
#  include "OpenGLES/ES1/glext.h"
#  include "glues.h"
# else
/*non standard include on linux/windows, usually not used*/
#  include "GLES/egl.h"
# endif

# if defined(GPAC_HAS_GLU) && !defined (GPAC_CONFIG_IOS)
/*WARNING - this is NOT a standard include, GLU is not supported by GLES*/
#  include <GLES/glu.h>
# endif

# ifndef EGL_VERSION_1_0
#  define EGL_VERSION_1_0		1
# endif

# ifndef EGL_VERSION_1_1
#  ifdef GL_OES_VERSION_1_1
#   define EGL_VERSION_1_1		1
#  endif
# endif

//end GPAC_USE_GLES1X

# elif defined(GPAC_USE_GLES2)
#  ifdef GPAC_CONFIG_IOS
#   include "OpenGLES/ES2/gl.h"
#   include "glues.h"
#  else
#   include <GLES2/gl2.h>
#   include <GLES2/gl2ext.h>
#  endif

//end GPAC_USE_GLES2

#elif defined (CONFIG_DARWIN_GL)

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

//end CONFIG_DARWIN_GL

#else
//generic windows/linux config

//in case the versions are defined, get the prototypes
# define GL_GLEXT_PROTOTYPES
# include <GL/gl.h>
# ifdef GPAC_HAS_GLU
#  include <GL/glu.h>
# endif

//end generic windows/linux config

#endif

#if 0
#define GL_CHECK_ERR()  {s32 res = glGetError(); if (res) GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("GL Error %d file %s line %d\n", res, __FILE__, __LINE__)); }
#else
#define GL_CHECK_ERR()
#endif

#ifdef GPAC_USE_TINYGL
# define GLTEXENV	glTexEnvi
# define GLTEXPARAM	glTexParameteri
# define TexEnvType u32
#elif defined (GPAC_USE_GLES1X)
# define GLTEXENV	glTexEnvx
# define GLTEXPARAM	glTexParameterx
# define TexEnvType Fixed
#else
# define GLTEXENV	glTexEnvf
# define GLTEXPARAM	glTexParameteri
# define TexEnvType Float
#endif


/*macros for GL proto and fun declaration*/
#ifdef _WIN32_WCE
#define GLAPICAST *
#elif defined(WIN32)
#define GLAPICAST APIENTRY *
#else
#define GLAPICAST *
#endif

#define GLDECL(ret, funname, args)	\
typedef ret (GLAPICAST proc_ ## funname)args;	\
extern proc_ ## funname funname;	\


#define GLDECL_FUNC(funname) proc_ ## funname funname = NULL
#define GLDECL_FUNC_STATIC(funname) static proc_ ## funname funname = NULL
#define GLDECL_EXTERN(funname) extern proc_ ## funname funname;

#if defined GPAC_USE_TINYGL
//no extensions with TinyGL
#elif defined (GPAC_USE_GLES1X)
//no extensions with OpenGL ES
#elif defined(WIN32) || defined (GPAC_CONFIG_WIN32)
#define LOAD_GL_FUNCS
#define GET_GLFUN(funname) funname = (proc_ ## funname) wglGetProcAddress(#funname)
#elif defined(CONFIG_DARWIN_GL)
//no extensions with OpenGL on OSX/IOS, glext.h is present
#else
#define LOAD_GL_FUNCS
extern void (*glXGetProcAddress(const GLubyte *procname))( void );
#define GET_GLFUN(funname) funname = (proc_ ## funname) glXGetProcAddress(#funname)
#endif

#ifndef YCBCR_MESA
#define YCBCR_MESA	0x8757
#endif

#ifndef YCBCR_422_APPLE
#define YCBCR_422_APPLE			0x85B9
#endif

#if defined(GPAC_USE_GLES2)
#define GL_UNPACK_ROW_LENGTH_EXT            0x0CF2
#define GL_UNPACK_SKIP_ROWS_EXT             0x0CF3
#define GL_UNPACK_SKIP_PIXELS_EXT           0x0CF4
#endif


#if defined(GPAC_USE_GLES1X) || defined(GPAC_USE_GLES2)
#  define GPAC_GL_NO_STRIDE
#endif

#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)

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

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
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


GLDECL(void, glBlendColor, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha))
GLDECL(void, glBlendEquation, (GLenum mode))
GLDECL(void, glBlendEquationSeparate, (GLenum modeRGB, GLenum modeAlpha))
GLDECL(void, glBlendFuncSeparate, (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) )

GLDECL(void, glCompressedTexImage2D, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data) )
GLDECL(void, glCompressedTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data) )
GLDECL(void, glGenerateMipmap, (GLenum target ) )

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

#endif //GL_VERSION_1_4



#ifndef GL_VERSION_1_5

#ifdef LOAD_GL_FUNCS
#define LOAD_GL_1_5
#endif

#define GL_ARRAY_BUFFER	0x8892
#define GL_ELEMENT_ARRAY_BUFFER	0x8893
#define GL_STREAM_DRAW	0x88E0
#define GL_STATIC_DRAW	0x88E4
#define GL_DYNAMIC_DRAW 0x88E8

GLDECL(void, glGenBuffers, (GLsizei , GLuint *) )
GLDECL(void, glDeleteBuffers, (GLsizei , GLuint *) )
GLDECL(void, glBindBuffer, (GLenum, GLuint ) )
GLDECL(void, glBufferData, (GLenum, int, void *, GLenum) )
GLDECL(void, glBufferSubData, (GLenum, size_t, int, void *) )
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

#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_DEPTH_COMPONENT32              0x81A7
#define GL_TEXTURE_DEPTH_SIZE             0x884A
#define GL_DEPTH_TEXTURE_MODE             0x884B

#define GL_TEXTURE_COMPARE_MODE           0x884C
#define GL_TEXTURE_COMPARE_FUNC           0x884D
#define GL_COMPARE_R_TO_TEXTURE           0x884E

#define GL_INVALID_FRAMEBUFFER_OPERATION                     0x0506
#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING             0x8210
#define GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE             0x8211
#define GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE                   0x8212
#define GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE                 0x8213
#define GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE                  0x8214
#define GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE                 0x8215
#define GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE                 0x8216
#define GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE               0x8217
#define GL_FRAMEBUFFER_DEFAULT                               0x8218
#define GL_FRAMEBUFFER_UNDEFINED                             0x8219
#define GL_DEPTH_STENCIL_ATTACHMENT                          0x821A
#define GL_MAX_RENDERBUFFER_SIZE                             0x84E8
#define GL_DEPTH_STENCIL                                     0x84F9
#define GL_UNSIGNED_INT_24_8                                 0x84FA
#define GL_DEPTH24_STENCIL8                                  0x88F0
#define GL_TEXTURE_STENCIL_SIZE                              0x88F1
#define GL_TEXTURE_RED_TYPE                                  0x8C10
#define GL_TEXTURE_GREEN_TYPE                                0x8C11
#define GL_TEXTURE_BLUE_TYPE                                 0x8C12
#define GL_TEXTURE_ALPHA_TYPE                                0x8C13
#define GL_TEXTURE_DEPTH_TYPE                                0x8C16
#define GL_UNSIGNED_NORMALIZED                               0x8C17
#define GL_FRAMEBUFFER_BINDING                               0x8CA6
#define GL_DRAW_FRAMEBUFFER_BINDING                          GL_FRAMEBUFFER_BINDING
#define GL_RENDERBUFFER_BINDING                              0x8CA7
#define GL_READ_FRAMEBUFFER                                  0x8CA8
#define GL_DRAW_FRAMEBUFFER                                  0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING                          0x8CAA
#define GL_RENDERBUFFER_SAMPLES                              0x8CAB
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE                0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME                0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL              0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE      0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER              0x8CD4
#define GL_FRAMEBUFFER_COMPLETE                              0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT                 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT         0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER                0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER                0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED                           0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS                             0x8CDF
#define GL_COLOR_ATTACHMENT0                                 0x8CE0
#define GL_COLOR_ATTACHMENT1                                 0x8CE1
#define GL_COLOR_ATTACHMENT2                                 0x8CE2
#define GL_COLOR_ATTACHMENT3                                 0x8CE3
#define GL_COLOR_ATTACHMENT4                                 0x8CE4
#define GL_COLOR_ATTACHMENT5                                 0x8CE5
#define GL_COLOR_ATTACHMENT6                                 0x8CE6
#define GL_COLOR_ATTACHMENT7                                 0x8CE7
#define GL_COLOR_ATTACHMENT8                                 0x8CE8
#define GL_COLOR_ATTACHMENT9                                 0x8CE9
#define GL_COLOR_ATTACHMENT10                                0x8CEA
#define GL_COLOR_ATTACHMENT11                                0x8CEB
#define GL_COLOR_ATTACHMENT12                                0x8CEC
#define GL_COLOR_ATTACHMENT13                                0x8CED
#define GL_COLOR_ATTACHMENT14                                0x8CEE
#define GL_COLOR_ATTACHMENT15                                0x8CEF
#define GL_DEPTH_ATTACHMENT                                  0x8D00
#define GL_STENCIL_ATTACHMENT                                0x8D20
#define GL_FRAMEBUFFER                                       0x8D40
#define GL_RENDERBUFFER                                      0x8D41
#define GL_RENDERBUFFER_WIDTH                                0x8D42
#define GL_RENDERBUFFER_HEIGHT                               0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT                      0x8D44
#define GL_STENCIL_INDEX1                                    0x8D46
#define GL_STENCIL_INDEX4                                    0x8D47
#define GL_STENCIL_INDEX8                                    0x8D48
#define GL_STENCIL_INDEX16                                   0x8D49
#define GL_RENDERBUFFER_RED_SIZE                             0x8D50
#define GL_RENDERBUFFER_GREEN_SIZE                           0x8D51
#define GL_RENDERBUFFER_BLUE_SIZE                            0x8D52
#define GL_RENDERBUFFER_ALPHA_SIZE                           0x8D53
#define GL_RENDERBUFFER_DEPTH_SIZE                           0x8D54
#define GL_RENDERBUFFER_STENCIL_SIZE                         0x8D55
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE                0x8D56
#define GL_MAX_SAMPLES                                       0x8D57

#define GL_SMOOTH_POINT_SIZE_RANGE        0x0B12
#define GL_SMOOTH_POINT_SIZE_GRANULARITY  0x0B13
#define GL_SMOOTH_LINE_WIDTH_RANGE        0x0B22
#define GL_SMOOTH_LINE_WIDTH_GRANULARITY  0x0B23
#define GL_ALIASED_POINT_SIZE_RANGE       0x846D
#define GL_ALIASED_LINE_WIDTH_RANGE       0x846E
#define GL_ARRAY_BUFFER_BINDING                        0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING                0x8895
#define GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING          0x889F
#define GL_BLEND_COLOR                    0x8005
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS 0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS     0x86A3
#define GL_TEXTURE_BINDING_CUBE_MAP       0x8514

#define GL_SAMPLE_ALPHA_TO_COVERAGE       0x809E
#define GL_SAMPLE_ALPHA_TO_ONE            0x809F
#define GL_SAMPLE_COVERAGE                0x80A0
#define GL_SAMPLE_COVERAGE_VALUE          0x80AA
#define GL_SAMPLE_COVERAGE_INVERT         0x80AB

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
GLDECL(GLint, glGetUniformfv, (GLuint program, GLint location, GLfloat *params))
GLDECL(GLint, glGetUniformiv, (GLuint program, GLint location, GLint *params))

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
GLDECL(void, glGetProgramiv, (GLuint program, GLenum pname, GLint *params) )
GLDECL(void, glGetProgramInfoLog, (GLuint program,  GLsizei maxLength,  GLsizei *length,  char *infoLog) )
GLDECL(void, glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei *length, char *infoLog) )
GLDECL(void, glGetShaderSource, (GLuint shader, GLsizei bufSize, GLsizei *length, char *source) )

GLDECL(void, glGetActiveAttrib, (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, const char *name))
GLDECL(void, glGetActiveUniform, (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, const char *name))
GLDECL(void, glGetAttachedShaders, (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders))
GLDECL(void, glBindAttribLocation, (GLuint program, GLuint index, const char *name))

GLDECL(GLboolean, glIsBuffer, (GLuint buffer) )
GLDECL(GLboolean, glIsFramebuffer, (GLuint framebuffer))
GLDECL(GLboolean, glIsProgram, (GLuint program))
GLDECL(GLboolean, glIsRenderbuffer, (GLuint rbuffer))
GLDECL(GLboolean, glIsShader, (GLuint shader))
GLDECL(void, glSampleCoverage, (GLclampf value, GLboolean invert) )
GLDECL(void, glStencilFuncSeparate, (GLenum face, GLenum func, GLint ref, GLuint mask) )
GLDECL(void, glStencilOpSeparate, (GLenum face, GLenum fail, GLenum zfail, GLenum zpass) )
GLDECL(void, glStencilMaskSeparate, (GLenum face, GLuint mask) )
GLDECL(void, glValidateProgram, (GLuint program) )

GLDECL(void, glVertexAttrib1fv, (GLuint index, const GLfloat *v) )
GLDECL(void, glVertexAttrib1f, (GLuint index, GLfloat x))
GLDECL(void, glVertexAttrib2f, (GLuint index, GLfloat x, GLfloat y))
GLDECL(void, glVertexAttrib2fv, (GLuint index, const GLfloat *v))
GLDECL(void, glVertexAttrib3f, (GLuint index, GLfloat x, GLfloat y, GLfloat z))
GLDECL(void, glVertexAttrib3fv, (GLuint index, const GLfloat *v))
GLDECL(void, glVertexAttrib4f, (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w))
GLDECL(void, glVertexAttrib4fv, (GLuint index, const GLfloat *v))

GLDECL(void, glGetBufferParameteriv, (GLenum target, GLenum pname, GLint *params) )
GLDECL(void, glGetFramebufferAttachmentParameteriv, (GLenum target, GLenum attachment, GLenum pname, GLint *params) )

GLDECL(void, glGetVertexAttribiv, (GLuint index, GLenum pname, GLint *params) )
GLDECL(void, glGetVertexAttribfv, (GLuint index, GLenum pname, GLfloat *params) )
GLDECL(void, glGetVertexAttribPointerv, (GLuint index, GLenum pname, GLvoid **pointer) )

#ifndef GPAC_CONFIG_ANDROID
GLDECL(void, glEnableVertexAttribArray, (GLuint index) )
GLDECL(void, glDisableVertexAttribArray, (GLuint index) )
GLDECL(void, glVertexAttribPointer, (GLuint  index,  GLint  size,  GLenum  type,  GLboolean  normalized,  GLsizei  stride,  const GLvoid *  pointer) )
GLDECL(void, glVertexAttribIPointer, (GLuint  index,  GLint  size,  GLenum  type,  GLsizei  stride,  const GLvoid *  pointer) )
GLDECL(GLint, glGetAttribLocation, (GLuint prog, const char *name) )

GLDECL(void, glBindFramebuffer, (GLenum target, GLuint framebuffer))
GLDECL(void, glFramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level))
GLDECL(void, glGenFramebuffers, (GLsizei n, GLuint *ids))
GLDECL(void, glGenRenderbuffers, (GLsizei n, GLuint *renderbuffers))
GLDECL(void, glBindRenderbuffer, (GLenum target, GLuint renderbuffer))
GLDECL(void, glRenderbufferStorage, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height))
GLDECL(void, glFramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer))
GLDECL(void, glDeleteFramebuffers, (GLsizei n, const GLuint * framebuffers))
GLDECL(void, glDeleteRenderbuffers, (GLsizei n, const GLuint * renderbuffers))
GLDECL(void, glGetRenderbufferParameteriv, (GLenum target, GLenum pname, GLint *params) )

GLDECL(GLenum, glCheckFramebufferStatus, (GLenum target))


#endif //GPAC_CONFIG_ANDROID


#endif //GL_VERSION_2_0

#endif //GPAC_USE_GLES1X || GPAC_USE_GLES2

#endif	/*GPAC_DISABLE_3D*/

#endif	/*_GL_INC_H_*/
