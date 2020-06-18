/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2020
 *					All rights reserved
 *
 *  This file is part of GPAC / OpenGL tools used by compositor filter, vout filter and WebGL bindings
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

#include <gpac/tools.h>
#include <gpac/filters.h>

#ifndef GPAC_DISABLE_3D

#include "../compositor/gl_inc.h"

#if (defined(WIN32) || defined(_WIN32_WCE)) && !defined(__GNUC__)
# if defined(GPAC_USE_TINYGL)
#  pragma comment(lib, "TinyGL")

# elif defined(GPAC_USE_GLES1X)

#  if 0
#   pragma message("Using OpenGL-ES Common Lite Profile")
#   pragma comment(lib, "libGLES_CL")
#	define GL_ES_CL_PROFILE
#  else
#   pragma message("Using OpenGL-ES Common Profile")
#   pragma comment(lib, "libGLES_CM")
#  endif

# elif defined(GPAC_USE_GLES2)
#  pragma comment(lib, "libGLESv2")

# else
#  pragma comment(lib, "opengl32")
# endif

#ifdef GPAC_HAS_GLU
#  pragma comment(lib, "glu32")
#endif

#endif

/*!! HORRIBLE HACK, but on my test devices, it seems that glClipPlanex is missing on the device but not in the SDK lib !!*/
#if defined(GL_MAX_CLIP_PLANES) && defined(__SYMBIAN32__)
#undef GL_MAX_CLIP_PLANES
#endif


#ifdef LOAD_GL_1_3

GLDECL_FUNC(glActiveTexture);
GLDECL_FUNC(glClientActiveTexture);
GLDECL_FUNC(glBlendEquation);

#endif //LOAD_GL_1_3

#ifdef LOAD_GL_1_4

GLDECL_FUNC(glPointParameterf);
GLDECL_FUNC(glPointParameterfv);

#endif //LOAD_GL_1_4


#ifdef LOAD_GL_1_5
GLDECL_FUNC(glGenBuffers);
GLDECL_FUNC(glDeleteBuffers);
GLDECL_FUNC(glBindBuffer);
GLDECL_FUNC(glBufferData);
GLDECL_FUNC(glBufferSubData);
GLDECL_FUNC(glMapBuffer);
GLDECL_FUNC(glUnmapBuffer);
#endif //LOAD_GL_1_5

#ifdef LOAD_GL_2_0

GLDECL_FUNC(glCreateProgram);
GLDECL_FUNC(glDeleteProgram);
GLDECL_FUNC(glLinkProgram);
GLDECL_FUNC(glUseProgram);
GLDECL_FUNC(glCreateShader);
GLDECL_FUNC(glDeleteShader);
GLDECL_FUNC(glShaderSource);
GLDECL_FUNC(glCompileShader);
GLDECL_FUNC(glAttachShader);
GLDECL_FUNC(glDetachShader);
GLDECL_FUNC(glGetShaderiv);
GLDECL_FUNC(glGetInfoLogARB);
GLDECL_FUNC(glGetUniformLocation);
GLDECL_FUNC(glGetUniformfv);
GLDECL_FUNC(glGetUniformiv);
GLDECL_FUNC(glUniform1f);
GLDECL_FUNC(glUniform2f);
GLDECL_FUNC(glUniform3f);
GLDECL_FUNC(glUniform4f);
GLDECL_FUNC(glUniform1i);
GLDECL_FUNC(glUniform2i);
GLDECL_FUNC(glUniform3i);
GLDECL_FUNC(glUniform4i);
GLDECL_FUNC(glUniform1fv);
GLDECL_FUNC(glUniform2fv);
GLDECL_FUNC(glUniform3fv);
GLDECL_FUNC(glUniform4fv);
GLDECL_FUNC(glUniform1iv);
GLDECL_FUNC(glUniform2iv);
GLDECL_FUNC(glUniform3iv);
GLDECL_FUNC(glUniform4iv);
GLDECL_FUNC(glUniformMatrix2fv);
GLDECL_FUNC(glUniformMatrix3fv);
GLDECL_FUNC(glUniformMatrix4fv);
GLDECL_FUNC(glUniformMatrix2x3fv);
GLDECL_FUNC(glUniformMatrix3x2fv);
GLDECL_FUNC(glUniformMatrix2x4fv);
GLDECL_FUNC(glUniformMatrix4x2fv);
GLDECL_FUNC(glUniformMatrix3x4fv);
GLDECL_FUNC(glUniformMatrix4x3fv);
GLDECL_FUNC(glGetProgramiv);
GLDECL_FUNC(glGetProgramInfoLog);
GLDECL_FUNC(glGetAttribLocation);
GLDECL_FUNC(glBindFramebuffer);
GLDECL_FUNC(glFramebufferTexture2D);
GLDECL_FUNC(glGenFramebuffers);
GLDECL_FUNC(glGenRenderbuffers);
GLDECL_FUNC(glBindRenderbuffer);
GLDECL_FUNC(glRenderbufferStorage);
GLDECL_FUNC(glFramebufferRenderbuffer);
GLDECL_FUNC(glDeleteFramebuffers);
GLDECL_FUNC(glDeleteRenderbuffers);
GLDECL_FUNC(glCheckFramebufferStatus);

GLDECL_FUNC(glBlendColor);
GLDECL_FUNC(glBlendEquationSeparate);
GLDECL_FUNC(glBlendFuncSeparate);
GLDECL_FUNC(glCompressedTexImage2D);
GLDECL_FUNC(glCompressedTexSubImage2D);
GLDECL_FUNC(glGenerateMipmap);
GLDECL_FUNC(glGetShaderInfoLog);
GLDECL_FUNC(glGetShaderSource);
GLDECL_FUNC(glGetActiveAttrib);
GLDECL_FUNC(glGetActiveUniform);
GLDECL_FUNC(glGetAttachedShaders);
GLDECL_FUNC(glBindAttribLocation);
GLDECL_FUNC(glIsBuffer);
GLDECL_FUNC(glIsFramebuffer);
GLDECL_FUNC(glIsProgram);
GLDECL_FUNC(glIsRenderbuffer);
GLDECL_FUNC(glIsShader);
GLDECL_FUNC(glSampleCoverage);
GLDECL_FUNC(glStencilFuncSeparate);
GLDECL_FUNC(glStencilOpSeparate);
GLDECL_FUNC(glStencilMaskSeparate);
GLDECL_FUNC(glValidateProgram);
GLDECL_FUNC(glVertexAttrib1fv);
GLDECL_FUNC(glVertexAttrib1f);
GLDECL_FUNC(glVertexAttrib2f);
GLDECL_FUNC(glVertexAttrib2fv);
GLDECL_FUNC(glVertexAttrib3f);
GLDECL_FUNC(glVertexAttrib3fv);
GLDECL_FUNC(glVertexAttrib4f);
GLDECL_FUNC(glVertexAttrib4fv);
GLDECL_FUNC(glGetBufferParameteriv);
GLDECL_FUNC(glGetFramebufferAttachmentParameteriv);
GLDECL_FUNC(glGetVertexAttribiv);
GLDECL_FUNC(glGetVertexAttribfv);
GLDECL_FUNC(glGetVertexAttribPointerv);
GLDECL_FUNC(glGetRenderbufferParameteriv);

#ifndef GPAC_CONFIG_ANDROID
GLDECL_FUNC(glEnableVertexAttribArray);
GLDECL_FUNC(glDisableVertexAttribArray);
GLDECL_FUNC(glVertexAttribPointer);
GLDECL_FUNC(glVertexAttribIPointer);
#endif

#endif //LOAD_GL_2_0

static Bool gl_fun_loaded = GF_FALSE;
void gf_opengl_init()
{
	if (gl_fun_loaded) return;
	gl_fun_loaded = GF_TRUE;
	
	if (gf_opts_get_bool("core", "rmt-ogl")) {
		rmt_BindOpenGL();
	}

#ifndef GPAC_USE_TINYGL

#ifdef LOAD_GL_1_3
	GET_GLFUN(glActiveTexture);
	GET_GLFUN(glClientActiveTexture);
	GET_GLFUN(glBlendEquation);
#endif

#ifdef LOAD_GL_1_4
	GET_GLFUN(glPointParameterf);
	GET_GLFUN(glPointParameterfv);
#endif

#ifdef LOAD_GL_1_5
	GET_GLFUN(glGenBuffers);
	GET_GLFUN(glDeleteBuffers);
	GET_GLFUN(glBindBuffer);
	GET_GLFUN(glBufferData);
	GET_GLFUN(glBufferSubData);

	GET_GLFUN(glMapBuffer);
	GET_GLFUN(glUnmapBuffer);
#endif


#ifdef LOAD_GL_2_0
	GET_GLFUN(glCreateProgram);

	GET_GLFUN(glDeleteProgram);
	GET_GLFUN(glLinkProgram);
	GET_GLFUN(glUseProgram);
	GET_GLFUN(glCreateShader);
	GET_GLFUN(glDeleteShader);
	GET_GLFUN(glShaderSource);
	GET_GLFUN(glCompileShader);
	GET_GLFUN(glAttachShader);
	GET_GLFUN(glDetachShader);
	GET_GLFUN(glGetShaderiv);
	GET_GLFUN(glGetInfoLogARB);
	GET_GLFUN(glGetUniformLocation);
	GET_GLFUN(glGetUniformfv);
	GET_GLFUN(glGetUniformiv);
	GET_GLFUN(glUniform1f);
	GET_GLFUN(glUniform2f);
	GET_GLFUN(glUniform3f);
	GET_GLFUN(glUniform4f);
	GET_GLFUN(glUniform1i);
	GET_GLFUN(glUniform2i);
	GET_GLFUN(glUniform3i);
	GET_GLFUN(glUniform4i);
	GET_GLFUN(glUniform1fv);
	GET_GLFUN(glUniform2fv);
	GET_GLFUN(glUniform3fv);
	GET_GLFUN(glUniform4fv);
	GET_GLFUN(glUniform1iv);
	GET_GLFUN(glUniform2iv);
	GET_GLFUN(glUniform3iv);
	GET_GLFUN(glUniform4iv);
	GET_GLFUN(glUniformMatrix2fv);
	GET_GLFUN(glUniformMatrix3fv);
	GET_GLFUN(glUniformMatrix4fv);
	GET_GLFUN(glUniformMatrix2x3fv);
	GET_GLFUN(glUniformMatrix3x2fv);
	GET_GLFUN(glUniformMatrix2x4fv);
	GET_GLFUN(glUniformMatrix4x2fv);
	GET_GLFUN(glUniformMatrix3x4fv);
	GET_GLFUN(glUniformMatrix4x3fv);
	GET_GLFUN(glGetProgramiv);
	GET_GLFUN(glGetProgramInfoLog);
	GET_GLFUN(glBlendColor);
	GET_GLFUN(glBlendEquationSeparate);
	GET_GLFUN(glBlendFuncSeparate);
	GET_GLFUN(glCompressedTexImage2D);
	GET_GLFUN(glCompressedTexSubImage2D);
	GET_GLFUN(glGenerateMipmap);
	GET_GLFUN(glGetShaderInfoLog);
	GET_GLFUN(glGetShaderSource);
	GET_GLFUN(glGetActiveAttrib);
	GET_GLFUN(glGetActiveUniform);
	GET_GLFUN(glGetAttachedShaders);
	GET_GLFUN(glBindAttribLocation);
	GET_GLFUN(glIsBuffer);
	GET_GLFUN(glIsFramebuffer);
	GET_GLFUN(glIsProgram);
	GET_GLFUN(glIsRenderbuffer);
	GET_GLFUN(glIsShader);
	GET_GLFUN(glSampleCoverage);
	GET_GLFUN(glStencilFuncSeparate);
	GET_GLFUN(glStencilOpSeparate);
	GET_GLFUN(glStencilMaskSeparate);
	GET_GLFUN(glValidateProgram);
	GET_GLFUN(glVertexAttrib1fv);
	GET_GLFUN(glVertexAttrib1f);
	GET_GLFUN(glVertexAttrib2f);
	GET_GLFUN(glVertexAttrib2fv);
	GET_GLFUN(glVertexAttrib3f);
	GET_GLFUN(glVertexAttrib3fv);
	GET_GLFUN(glVertexAttrib4f);
	GET_GLFUN(glVertexAttrib4fv);
	GET_GLFUN(glGetBufferParameteriv);
	GET_GLFUN(glGetFramebufferAttachmentParameteriv);
	GET_GLFUN(glGetVertexAttribiv);
	GET_GLFUN(glGetVertexAttribfv);
	GET_GLFUN(glGetVertexAttribPointerv);
	GET_GLFUN(glGetRenderbufferParameteriv);

#ifndef GPAC_CONFIG_ANDROID
	GET_GLFUN(glEnableVertexAttribArray);
	GET_GLFUN(glDisableVertexAttribArray);
	GET_GLFUN(glVertexAttribPointer);
	GET_GLFUN(glVertexAttribIPointer);
	GET_GLFUN(glGetAttribLocation);

	GET_GLFUN(glBindFramebuffer);
	GET_GLFUN(glFramebufferTexture2D);
	GET_GLFUN(glGenFramebuffers);
	GET_GLFUN(glGenRenderbuffers);
	GET_GLFUN(glBindRenderbuffer);
	GET_GLFUN(glRenderbufferStorage);
	GET_GLFUN(glFramebufferRenderbuffer);
	GET_GLFUN(glDeleteFramebuffers);
	GET_GLFUN(glDeleteRenderbuffers);
	GET_GLFUN(glCheckFramebufferStatus);

#endif

#endif //LOAD_GL_2_0

#endif //GPAC_USE_TINYGL

}



static char *gl_shader_vars_yuv = \
"uniform sampler2D _gf_%s_1;\n\
uniform sampler2D _gf_%s_2;\n\
uniform sampler2D _gf_%s_3;\n\
const vec3 offset = vec3(-0.0625, -0.5, -0.5);\n\
const vec3 R_mul = vec3(1.164,  0.000,  1.596);\n\
const vec3 G_mul = vec3(1.164, -0.391, -0.813);\n\
const vec3 B_mul = vec3(1.164,  2.018,  0.000);\n\
";

static char *gl_shader_fun_yuv = \
"vec2 texc;\n\
vec3 yuv, rgb;\n\
texc = _gpacTexCoord.st;\n\
yuv.x = texture2D(_gf_%s_1, texc).r;\n\
yuv.y = texture2D(_gf_%s_2, texc).r;\n\
yuv.z = texture2D(_gf_%s_3, texc).r;\n\
yuv += offset;\n\
rgb.r = dot(yuv, R_mul);\n\
rgb.g = dot(yuv, G_mul);\n\
rgb.b = dot(yuv, B_mul);\n\
return vec4(rgb, 1.0);\n\
";

static char *gl_shader_vars_nv12 = \
"uniform sampler2D _gf_%s_1;\n\
uniform sampler2D _gf_%s_2;\n\
const vec3 offset = vec3(-0.0625, -0.5, -0.5);\n\
const vec3 R_mul = vec3(1.164,  0.000,  1.596);\n\
const vec3 G_mul = vec3(1.164, -0.391, -0.813);\n\
const vec3 B_mul = vec3(1.164,  2.018,  0.000);\n\
";

static char *gl_shader_fun_nv12 = \
"vec2 texc;\n\
vec3 yuv, rgb;\n\
texc = _gpacTexCoord.st;\n\
yuv.x = texture2D(_gf_%s_1, texc).r;\n\
yuv.yz = texture2D(_gf_%s_2, texc).ra;\n\
yuv += offset;\n\
rgb.r = dot(yuv, R_mul);\n\
rgb.g = dot(yuv, G_mul);\n\
rgb.b = dot(yuv, B_mul);\n\
return vec4(rgb, 1.0);\n\
";


static char *gl_shader_vars_nv21 = \
"uniform sampler2D _gf_%s_1;\n\
uniform sampler2D _gf_%s_2;\n\
const vec3 offset = vec3(-0.0625, -0.5, -0.5);\n\
const vec3 R_mul = vec3(1.164,  0.000,  1.596);\n\
const vec3 G_mul = vec3(1.164, -0.391, -0.813);\n\
const vec3 B_mul = vec3(1.164,  2.018,  0.000);\n\
";

static char *gl_shader_fun_nv21 = \
"vec2 texc;\n\
vec3 yuv, rgb;\n\
texc = _gpacTexCoord.st;\n\
yuv.x = texture2D(_gf_%s_1, texc).r;\n\
yuv.yz = texture2D(_gf_%s_2, texc).ar;\n\
yuv += offset;\n\
rgb.r = dot(yuv, R_mul);\n\
rgb.g = dot(yuv, G_mul);\n\
rgb.b = dot(yuv, B_mul);\n\
return vec4(rgb, 1.0);\n\
";


static char *gl_shader_vars_uyvu = \
	"uniform sampler2D _gf_%s_1;\
	uniform float _gf_%s_width;\
	const vec3 offset = vec3(-0.0625, -0.5, -0.5);\
	const vec3 R_mul = vec3(1.164,  0.000,  1.596);\
	const vec3 G_mul = vec3(1.164, -0.391, -0.813);\
	const vec3 B_mul = vec3(1.164,  2.018,  0.000);\
	";

static char *gl_shader_fun_uyvy = \
"vec2 texc, t_texc;\
vec3 yuv, rgb;\
vec4 uyvy;\
float tex_s;\
texc = _gpacTexCoord.st;\
t_texc = texc * vec2(1, 1);\
uyvy = texture2D(_gf_%s_1, t_texc); \
tex_s = texc.x*_gf_%s_width;\
if (tex_s - (2.0 * floor(tex_s/2.0)) == 1.0) {\
	uyvy.g = uyvy.a; \
}\
yuv.r = uyvy.g;\
yuv.g = uyvy.r;\
yuv.b = uyvy.b;\
yuv += offset; \
rgb.r = dot(yuv, R_mul); \
rgb.g = dot(yuv, G_mul); \
rgb.b = dot(yuv, B_mul); \
return vec4(rgb, 1.0);\
";


static char *gl_shader_fun_vyuy = \
"vec2 texc, t_texc;\
vec3 yuv, rgb;\
vec4 vyuy;\
float tex_s;\
texc = _gpacTexCoord.st;\
t_texc = texc * vec2(1, 1);\
vyuy = texture2D(_gf_%s_1, t_texc); \
tex_s = texc.x*_gf_%s_width;\
if (tex_s - (2.0 * floor(tex_s/2.0)) == 1.0) {\
	vyuy.g = vyuy.a; \
}\
yuv.r = vyuy.g;\
yuv.g = vyuy.b;\
yuv.b = vyuy.r;\
yuv += offset; \
rgb.r = dot(yuv, R_mul); \
rgb.g = dot(yuv, G_mul); \
rgb.b = dot(yuv, B_mul); \
return vec4(rgb, 1.0);\
";


static char *gl_shader_fun_yuyv = \
"vec2 texc, t_texc;\
vec3 yuv, rgb;\
vec4 yvyu;\
float tex_s;\
texc = _gpacTexCoord.st;\
t_texc = texc * vec2(1, 1);\
yvyu = texture2D(_gf_%s_1, t_texc); \
tex_s = texc.x*_gf_%s_width;\
if (tex_s - (2.0 * floor(tex_s/2.0)) == 1.0) {\
	yvyu.r = yvyu.b; \
}\
yuv.r = yvyu.r;\
yuv.g = yvyu.g;\
yuv.b = yvyu.a;\
yuv += offset; \
rgb.r = dot(yuv, R_mul); \
rgb.g = dot(yuv, G_mul); \
rgb.b = dot(yuv, B_mul); \
return vec4(rgb, 1.0);\
";

static char *gl_shader_fun_yvyu = \
"vec2 texc, t_texc;\
vec3 yuv, rgb;\
vec4 yuyv;\
float tex_s;\
texc = _gpacTexCoord.st;\
t_texc = texc * vec2(1, 1);\
yuyv = texture2D(_gf_%s_1, t_texc); \
tex_s = texc.x*_gf_%s_width;\
if (tex_s - (2.0 * floor(tex_s/2.0)) == 1.0) {\
	yuyv.r = yuyv.b; \
}\
yuv.r = yuyv.r;\
yuv.g = yuyv.a;\
yuv.b = yuyv.g;\
yuv += offset; \
rgb.r = dot(yuv, R_mul); \
rgb.g = dot(yuv, G_mul); \
rgb.b = dot(yuv, B_mul); \
return vec4(rgb, 1.0);\
";

static char *gl_shader_vars_rgb = \
"uniform sampler2D _gf_%s_1;\n\
";

static char *gl_shader_fun_alphagrey = \
"vec4 col, ocol;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
ocol.r = ocol.g = ocol.b = col.a;\n\
ocol.a = col.r;\n\
return ocol;\n\
";


static char *gl_shader_fun_rgb444 = \
"vec4 col, ocol;\n\
float res, col_g, col_b;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
res = floor( 255.0 * col.a );\n\
col_g = floor(res / 16.0);\n\
col_b = floor(res - col_g*16.0);\n\
ocol.r = col.r * 17.0;\n\
ocol.g = col_g / 15.0;\n\
ocol.b = col_b / 15.0;\n\
ocol.a = 1.0;\n\
return ocol;\
";


static char *gl_shader_fun_rgb555 = \
"vec4 col, ocol;\n\
float res, col_r, col_g1, col_g, col_b;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
res = floor(255.0 * col.r);\n\
col_r = floor(res / 4.0);\n\
col_g1 = floor(res - col_r*4.0);\n\
res = floor(255.0 * col.a);\n\
col_g = floor(res / 32);\n\
col_b = floor(res - col_g*32.0);\n\
col_g += col_g1 * 8;\n\
ocol.r = col_r / 31.0;\n\
ocol.g = col_g / 31.0;\n\
ocol.b = col_b / 31.0;\n\
ocol.a = 1.0:\n\
return ocol;\
";

static char *gl_shader_fun_rgb565 = \
"vec4 col, ocol;\n\
float res, col_r, col_g1, col_g, col_b;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
res = floor(255.0 * col.r);\n\
col_r = floor(res / 8.0);\n\
col_g1 = floor(res - col_r*8.0);\n\
res = floor(255.0 * col.a);\n\
col_g = floor(res / 32);\n\
col_b = floor(res - col_g*32.0);\n\
col_g += col_g1 * 8;\n\
ocol.r = col_r / 31.0;\n\
ocol.g = col_g / 63.0;\n\
ocol.b = col_b / 31.0;\n\
ocol.a = 1.0;\n\
return ocol;\
";


static char *gl_shader_fun_argb = \
"vec4 col, ocol;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
ocol.r = col.g;\n\
ocol.g = col.b;\n\
ocol.b = col.a;\n\
ocol.a = col.r;\n\
return ocol;\
";

static char *gl_shader_fun_abgr = \
"vec4 col, ocol;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
ocol.r = col.a;\n\
ocol.g = col.b;\n\
ocol.b = col.g;\n\
ocol.a = col.r;\n\
return ocol;\
";


static char *gl_shader_fun_bgra = \
"vec4 col, ocol;\n\
col = texture2D(_gf_%s_1, _gpacTexCoord.st);\n\
ocol.r = col.b;\n\
ocol.g = col.g;\n\
ocol.b = col.r;\n\
ocol.a = col.a;\n\
return ocol;\
";


static char *gl_shader_fun_rgb = \
"return texture2D(_gf_%s_1, _gpacTexCoord.st);\
";

static char *gl_shader_vars_externalOES = \
"uniform samplerExternalOES _gf_%s_1;\n\
";



Bool gf_gl_txw_insert_fragment_shader(u32 pix_fmt, const char *tx_name, char **f_source)
{
	char szCode[4000];
	const char *shader_vars = NULL;
	const char *shader_fun = NULL;

	switch (pix_fmt) {
	case GF_PIXEL_NV21:
	case GF_PIXEL_NV21_10:
		shader_vars = gl_shader_vars_nv21;
		shader_fun = gl_shader_fun_nv21;
		break;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV12_10:
		shader_vars = gl_shader_vars_nv12;
		shader_fun = gl_shader_fun_nv12;
		break;
	case GF_PIXEL_UYVY:
		shader_vars = gl_shader_vars_uyvu;
		shader_fun = gl_shader_fun_uyvy;
		break;
	case GF_PIXEL_YUYV:
		shader_vars = gl_shader_vars_uyvu; //same as uyvy
		shader_fun = gl_shader_fun_yuyv;
		break;
	case GF_PIXEL_VYUY:
		shader_vars = gl_shader_vars_uyvu; //same as uyvy
		shader_fun = gl_shader_fun_vyuy;
		break;
	case GF_PIXEL_YVYU:
		shader_vars = gl_shader_vars_uyvu; //same as uyvy
		shader_fun = gl_shader_fun_yvyu;
		break;
	case GF_PIXEL_ALPHAGREY:
		shader_vars = gl_shader_vars_rgb;
		shader_fun = gl_shader_fun_alphagrey;
		break;
	case GF_PIXEL_RGB_444:
		shader_vars = gl_shader_vars_rgb;
		shader_fun = gl_shader_fun_rgb444;
		break;
	case GF_PIXEL_RGB_555:
		shader_vars = gl_shader_vars_rgb;
		shader_fun = gl_shader_fun_rgb555;
		break;
	case GF_PIXEL_RGB_565:
		shader_vars = gl_shader_vars_rgb;
		shader_fun = gl_shader_fun_rgb565;
		break;
	case GF_PIXEL_ARGB:
	case GF_PIXEL_XRGB:
		shader_vars = gl_shader_vars_rgb;
		shader_fun = gl_shader_fun_argb;
		break;
	case GF_PIXEL_ABGR:
	case GF_PIXEL_XBGR:
		shader_vars = gl_shader_vars_rgb;
		shader_fun = gl_shader_fun_abgr;
		break;
	case GF_PIXEL_BGR:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_BGRX:
		shader_vars = gl_shader_vars_rgb;
		shader_fun = gl_shader_fun_bgra;
		break;

	case GF_PIXEL_YUV:
	case GF_PIXEL_YUV_10:
	case GF_PIXEL_YUVA:
	case GF_PIXEL_YUVD:
	case GF_PIXEL_YUV422:
	case GF_PIXEL_YUV422_10:
	case GF_PIXEL_YUVA444:
	case GF_PIXEL_YUV444:
	case GF_PIXEL_YUV444_10:
		shader_vars = gl_shader_vars_yuv;
		shader_fun = gl_shader_fun_yuv;
		break;
	case GF_PIXEL_GL_EXTERNAL:
		shader_vars = gl_shader_vars_externalOES;
		shader_fun = gl_shader_fun_rgb;
		break;
	default:
		shader_vars = gl_shader_vars_rgb;
		shader_fun = gl_shader_fun_rgb;
		break;
	}

	if (!shader_vars || !shader_fun) return GF_FALSE;
	/*format with max 3 tx_name (for yuv)*/
	sprintf(szCode, shader_vars, tx_name, tx_name, tx_name);
	gf_dynstrcat(f_source, szCode, NULL);

	gf_dynstrcat(f_source, "\nvec4 ", NULL);
	gf_dynstrcat(f_source, tx_name, NULL);
	gf_dynstrcat(f_source, "_sample(vec2 _gpacTexCoord) {\n", NULL);
	/*format with max 3 tx_name (for yuv)*/
	sprintf(szCode, shader_fun, tx_name, tx_name, tx_name);

	gf_dynstrcat(f_source, szCode, NULL);
	gf_dynstrcat(f_source, "\n}\n", NULL);

	return GF_TRUE;
}


Bool gf_gl_txw_setup(GF_GLTextureWrapper *tx, u32 pix_fmt, u32 width, u32 height, u32 stride, u32 uv_stride, Bool linear_interp, GF_FilterFrameInterface *frame_ifce)
{
	u32 i;

	tx->width = width;
	tx->height = height;
	tx->pix_fmt = pix_fmt;
	tx->stride = stride;
	tx->uv_stride = uv_stride;
	tx->internal_textures = GF_TRUE;
	tx->nb_textures = 1;
	tx->is_yuv = GF_FALSE;
	tx->bit_depth = 8;
	tx->gl_format = GL_LUMINANCE;
	tx->bytes_per_pix = 1;

	switch (tx->pix_fmt) {
	case GF_PIXEL_YUV444_10:
		tx->bit_depth = 10;
	case GF_PIXEL_YUV444:
		tx->uv_w = tx->width;
		tx->uv_h = tx->height;
		if (!tx->uv_stride)
			tx->uv_stride = tx->stride;
		tx->is_yuv = GF_TRUE;
		tx->nb_textures = 3;
		break;
	case GF_PIXEL_YUV422_10:
		tx->bit_depth = 10;
	case GF_PIXEL_YUV422:
		tx->uv_w = tx->width/2;
		tx->uv_h = tx->height;
		if (!tx->uv_stride)
			tx->uv_stride = tx->stride/2;
		tx->is_yuv = GF_TRUE;
		tx->nb_textures = 3;
		break;
	case GF_PIXEL_YUV_10:
		tx->bit_depth = 10;
	case GF_PIXEL_YUV:
		tx->uv_w = tx->width/2;
		if (tx->width % 2) tx->uv_w++;
		tx->uv_h = tx->height/2;
		if (tx->height % 2) tx->uv_h++;
		if (!tx->uv_stride) {
			tx->uv_stride = tx->stride/2;
			if (tx->stride%2) tx->uv_stride ++;
		}
		tx->is_yuv = GF_TRUE;
		tx->nb_textures = 3;
		break;
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
		tx->bit_depth = 10;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
		tx->uv_w = tx->width/2;
		tx->uv_h = tx->height/2;
		if (!tx->uv_stride)
			tx->uv_stride = tx->stride;
		tx->is_yuv = GF_TRUE;
		tx->nb_textures = 2;
		break;
	case GF_PIXEL_UYVY:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_VYUY:
		tx->uv_w = tx->width/2;
		tx->uv_h = tx->height;
		if (!tx->uv_stride) {
			tx->uv_stride = tx->stride/2;
			if (tx->stride%2) tx->uv_stride ++;
		}
		tx->is_yuv = GF_TRUE;
		tx->nb_textures = 1;
		break;
	case GF_PIXEL_GREYSCALE:
		tx->gl_format = GL_LUMINANCE;
		tx->bytes_per_pix = 1;
		break;
	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
		tx->gl_format = GL_LUMINANCE_ALPHA;
		tx->bytes_per_pix = 2;
		break;
	case GF_PIXEL_RGB:
		tx->gl_format = GL_RGB;
		tx->bytes_per_pix = 3;
		break;
	case GF_PIXEL_BGR:
		tx->gl_format = GL_RGB;
		tx->bytes_per_pix = 3;
		break;
	case GF_PIXEL_BGRA:
		tx->has_alpha = GF_TRUE;
	case GF_PIXEL_BGRX:
		tx->gl_format = GL_RGBA;
		tx->bytes_per_pix = 4;
		break;
	case GF_PIXEL_ARGB:
		tx->has_alpha = GF_TRUE;
	case GF_PIXEL_XRGB:
		tx->gl_format = GL_RGBA;
		tx->bytes_per_pix = 4;
		break;
	case GF_PIXEL_ABGR:
		tx->has_alpha = GF_TRUE;
	case GF_PIXEL_XBGR:
		tx->gl_format = GL_RGBA;
		tx->bytes_per_pix = 4;
		break;
	case GF_PIXEL_RGBA:
		tx->has_alpha = GF_TRUE;
	case GF_PIXEL_RGBX:
		tx->gl_format = GL_RGBA;
		tx->bytes_per_pix = 4;
		break;
	case GF_PIXEL_RGB_444:
		tx->gl_format = GL_LUMINANCE_ALPHA;
		tx->bytes_per_pix = 2;
		break;
	case GF_PIXEL_RGB_555:
		tx->gl_format = GL_LUMINANCE_ALPHA;
		tx->bytes_per_pix = 2;
		break;
	case GF_PIXEL_RGB_565:
		tx->gl_format = GL_LUMINANCE_ALPHA;
		tx->bytes_per_pix = 2;
		break;
	case GF_PIXEL_GL_EXTERNAL:
		tx->internal_textures = GF_FALSE;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Pixel format %s unknown, cannot setup texture wrapper\n", gf_4cc_to_str(tx->pix_fmt)));
		return GF_FALSE;
	}

	if (tx->bit_depth>8)
		tx->bytes_per_pix = 2;

	if (frame_ifce && frame_ifce->get_gl_texture) {
		tx->internal_textures = GF_FALSE;
	}

	/*create textures*/
	if (tx->internal_textures) {
#if !defined(GPAC_USE_GLES1X)
		u32 glmode = GL_NEAREST;
		if ((tx->nb_textures==1) && linear_interp && !tx->is_yuv)
			glmode = GL_LINEAR;
#endif
		tx->first_tx_load = GF_TRUE;
		tx->scale_10bit = 0;

		tx->memory_format = GL_UNSIGNED_BYTE;
		if (tx->is_yuv && tx->bit_depth>8) {
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
			//we use x bits but GL will normalise using 16 bits, so we need to multiply the normalized result by 2^(16-x)
			u32 nb_bits = (16 - tx->bit_depth);
			u32 scale = 1;
			while (nb_bits) {
				scale*=2;
				nb_bits--;
			}
			tx->scale_10bit = scale;
#endif
			tx->memory_format = GL_UNSIGNED_SHORT;
		}


		GL_CHECK_ERR()
		glGenTextures(tx->nb_textures, tx->textures);
		GL_CHECK_ERR()

#ifndef GPAC_USE_GLES2
		glEnable(GL_TEXTURE_2D);
		GL_CHECK_ERR()
#endif

		for (i=0; i<tx->nb_textures; i++) {
#if !defined(GPAC_USE_GLES1X)
			glBindTexture(GL_TEXTURE_2D, tx->textures[i] );
			GL_CHECK_ERR()
#if defined(GPAC_USE_GLES2)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#endif
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glmode);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glmode);
#endif
		}


#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
		if (tx->is_yuv && tx->pbo_state) {
			tx->first_tx_load = GF_FALSE;
			glGenBuffers(1, &tx->PBOs[0]);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[0]);

			if (tx->nb_textures>1) {
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, tx->bytes_per_pix * tx->width*tx->height, NULL, GL_DYNAMIC_DRAW_ARB);

				glGenBuffers(1, &tx->PBOs[1]);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[1]);
			} else {
				//packed YUV
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, tx->bytes_per_pix * 4*tx->width/2*tx->height, NULL, GL_DYNAMIC_DRAW_ARB);
			}

			if (tx->nb_textures==3) {
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, tx->bytes_per_pix * tx->uv_w*tx->uv_h, NULL, GL_DYNAMIC_DRAW_ARB);

				glGenBuffers(1, &tx->PBOs[2]);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[2]);
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, tx->bytes_per_pix * tx->uv_w*tx->uv_h, NULL, GL_DYNAMIC_DRAW_ARB);
			}
			//nv12/21
			else {
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, tx->bytes_per_pix * 2*tx->uv_w*tx->uv_h, NULL, GL_DYNAMIC_DRAW_ARB);
			}
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		}
#endif

#ifndef GPAC_USE_GLES2
		glDisable(GL_TEXTURE_2D);
		GL_CHECK_ERR()
#endif
	}
	return GF_TRUE;
}

Bool gf_gl_txw_upload(GF_GLTextureWrapper *tx, const u8 *data, GF_FilterFrameInterface *frame_ifce)
{
	Bool use_stride = GF_FALSE;
	u32 stride_luma, stride_chroma;
	const u8 *pY=NULL, *pU=NULL, *pV=NULL, *pA=NULL;


	tx->frame_ifce = NULL;

	if (!frame_ifce && !data) return GF_FALSE;

	if (!tx->internal_textures) {
		if (frame_ifce && frame_ifce->get_gl_texture) {
			tx->frame_ifce = frame_ifce;
			return GF_TRUE;
		}
		return GF_FALSE;
	}

	stride_luma = tx->stride;
	stride_chroma = tx->uv_stride;
	if (!frame_ifce) {
		if (tx->is_yuv) {
			if (tx->nb_textures==2) {
				pU = data + tx->stride * tx->height;
			} else if (tx->nb_textures==3) {
				pU = data + tx->stride * tx->height;
				pV = pU + tx->uv_stride * tx->uv_h;
			}
		}
	} else {
		u32 st_o;
		if (tx->nb_textures)
			frame_ifce->get_plane(frame_ifce, 0, &data, &stride_luma);
		if (tx->nb_textures>1)
			frame_ifce->get_plane(frame_ifce, 1, &pU, &stride_chroma);
		//todo we need to cleanup alpha frame fetch, how do we differentiate between NV12+alpha (3 planes) and YUV420 ?
		if (tx->nb_textures>2)
			frame_ifce->get_plane(frame_ifce, 2, &pV, &st_o);
		if (tx->nb_textures>3)
			frame_ifce->get_plane(frame_ifce, 3, &pA, &st_o);
	}

	pY = data;

	if (tx->is_yuv && (stride_luma != tx->width)) {
		use_stride = GF_TRUE; //whether >8bits or real stride
	}
	GL_CHECK_ERR()

	//push data
#ifndef GPAC_USE_GLES2
	glEnable(GL_TEXTURE_2D);
	GL_CHECK_ERR()
#endif


	if (tx->scale_10bit) {
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
		glPixelTransferi(GL_RED_SCALE, tx->scale_10bit);
		if (tx->nb_textures==2)
			glPixelTransferi(GL_ALPHA_SCALE, tx->scale_10bit);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
#endif
	} else {
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
		glPixelTransferi(GL_RED_SCALE, 1);
		glPixelTransferi(GL_ALPHA_SCALE, 1);
		glPixelStorei(GL_UNPACK_LSB_FIRST, 0);
#endif
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}
	GL_CHECK_ERR()


	if (!tx->is_yuv) {
		glBindTexture(GL_TEXTURE_2D, tx->textures[0] );
		GL_CHECK_ERR()
		if (use_stride) {
#if !defined(GPAC_GL_NO_STRIDE)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, tx->stride / tx->bytes_per_pix);
#endif
		}
		if (tx->first_tx_load) {
			glTexImage2D(GL_TEXTURE_2D, 0, tx->gl_format, tx->width, tx->height, 0, tx->gl_format, GL_UNSIGNED_BYTE, data);
			tx->first_tx_load = GF_FALSE;
		} else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tx->width, tx->height, tx->gl_format, tx->memory_format, data);
		}
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

		GL_CHECK_ERR()
	}
#if !defined(GPAC_USE_GLES1X) && !defined(GPAC_USE_GLES2)
	else if (tx->pbo_state && tx->PBOs[0]) {
		u32 i, linesize, count, p_stride;
		u8 *ptr;

		//packed YUV
		if (tx->nb_textures==1) {
			if (tx->pbo_state!=GF_GL_PBO_TEXIMG) {
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[0]);
				ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

				linesize = tx->width/2 * tx->bytes_per_pix * 4;
				p_stride = stride_luma;
				count = tx->height;

				for (i=0; i<count; i++) {
					memcpy(ptr, pY, linesize);
					pY += p_stride;
					ptr += linesize;
				}

				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
			}
			if (tx->pbo_state!=GF_GL_PBO_PUSH) {
				glBindTexture(GL_TEXTURE_2D, tx->textures[0] );
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[0]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tx->width/2, tx->height, 0, GL_RGBA, tx->memory_format, NULL);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			}
		} else {
			if (tx->pbo_state!=GF_GL_PBO_TEXIMG) {
				glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[0]);
				ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

				linesize = tx->width*tx->bytes_per_pix;
				p_stride = stride_luma;
				count = tx->height;

				for (i=0; i<count; i++) {
					memcpy(ptr, pY, linesize);
					pY += p_stride;
					ptr += linesize;
				}

				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

				if (pU) {
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[1]);
					ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

					linesize = tx->uv_w * tx->bytes_per_pix;
					p_stride = stride_chroma;
					count = tx->uv_h;
					//NV12 and  NV21
					if (!pV) {
						linesize *= 2;
					}

					for (i=0; i<count; i++) {
						memcpy(ptr, pU, linesize);
						pU += p_stride;
						ptr += linesize;
					}

					glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
				}

				
				if (pV) {
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[2]);
					ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

					for (i=0; i<count; i++) {
						memcpy(ptr, pV, linesize);
						pV += p_stride;
						ptr += linesize;
					}

					glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
				}
			}

			if (tx->pbo_state!=GF_GL_PBO_PUSH) {
				glBindTexture(GL_TEXTURE_2D, tx->textures[0] );
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[0]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tx->width, tx->height, 0, tx->gl_format, tx->memory_format, NULL);

				glBindTexture(GL_TEXTURE_2D, tx->textures[1] );
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[1]);
				glTexImage2D(GL_TEXTURE_2D, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, tx->uv_w, tx->uv_h, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, tx->memory_format, NULL);

				if (pV) {
					glBindTexture(GL_TEXTURE_2D, tx->textures[2] );
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, tx->PBOs[2]);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tx->uv_w, tx->uv_h, 0, tx->gl_format, tx->memory_format, NULL);
				}

				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			}
		}
	}
#endif
	else if ((tx->pix_fmt==GF_PIXEL_UYVY) || (tx->pix_fmt==GF_PIXEL_YUYV) || (tx->pix_fmt==GF_PIXEL_VYUY) || (tx->pix_fmt==GF_PIXEL_YVYU)) {
		u32 uv_stride = 0;
		glBindTexture(GL_TEXTURE_2D, tx->textures[0] );

		use_stride = GF_FALSE;
		if (stride_luma > 2*tx->width) {
			//stride is given in bytes for packed formats, so divide by 2 to get the number of pixels
			//for YUYV, and we upload as a texture with half the wsize so rdevide again by two
			//since GL_UNPACK_ROW_LENGTH counts in component and we moved the set 2 bytes per comp on 10 bits
			//no need to further divide
			uv_stride = stride_luma/4;
			use_stride = GF_TRUE;
		}
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, uv_stride);
#endif
		if (tx->first_tx_load) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tx->width/2, tx->height, 0, GL_RGBA, tx->memory_format, pY);
			tx->first_tx_load = GF_FALSE;
		} else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tx->width/2, tx->height, GL_RGBA, tx->memory_format, pY);

		}

#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

	}
	else if (tx->first_tx_load) {
		glBindTexture(GL_TEXTURE_2D, tx->textures[0] );
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_luma/tx->bytes_per_pix);
#endif
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tx->width, tx->height, 0, tx->gl_format, tx->memory_format, pY);

		glBindTexture(GL_TEXTURE_2D, tx->textures[1] );
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, pV ? stride_chroma/tx->bytes_per_pix : stride_chroma/tx->bytes_per_pix/2);
#endif
		glTexImage2D(GL_TEXTURE_2D, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, tx->uv_w, tx->uv_h, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, tx->memory_format, pU);

		if (pV) {
			glBindTexture(GL_TEXTURE_2D, tx->textures[2] );
#if !defined(GPAC_GL_NO_STRIDE)
			if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_chroma/tx->bytes_per_pix);
#endif
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tx->uv_w, tx->uv_h, 0, tx->gl_format, tx->memory_format, pV);
		}

#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
		tx->first_tx_load = GF_FALSE;
	}
	else {
		glBindTexture(GL_TEXTURE_2D, tx->textures[0] );
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_luma/tx->bytes_per_pix);
#endif
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tx->width, tx->height, tx->gl_format, tx->memory_format, pY);
		glBindTexture(GL_TEXTURE_2D, 0);

		glBindTexture(GL_TEXTURE_2D, tx->textures[1] );
#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, pV ? stride_chroma/tx->bytes_per_pix : stride_chroma/tx->bytes_per_pix/2);
#endif
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tx->uv_w, tx->uv_h, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, tx->memory_format, pU);
		glBindTexture(GL_TEXTURE_2D, 0);

		if (pV) {
			glBindTexture(GL_TEXTURE_2D, tx->textures[2] );
#if !defined(GPAC_GL_NO_STRIDE)
			if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, stride_chroma/tx->bytes_per_pix);
#endif
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tx->uv_w, tx->uv_h, tx->gl_format, tx->memory_format, pV);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

#if !defined(GPAC_GL_NO_STRIDE)
		if (use_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
	}
	return GF_TRUE;
}

Bool gf_gl_txw_bind(GF_GLTextureWrapper *tx, const char *tx_name, u32 gl_program, u32 texture_unit)
{
	if (!texture_unit)
		texture_unit = GL_TEXTURE0;

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
	if (!tx->uniform_setup && gl_program) {
		char szName[100];
		u32 i;
		u32 start_idx = texture_unit - GL_TEXTURE0;
		s32 loc;
		for (i=0; i<tx->nb_textures; i++) {
			sprintf(szName, "_gf_%s_%d", tx_name, i+1);
			loc = glGetUniformLocation(gl_program, szName);
			GL_CHECK_ERR()
			if (loc == -1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[GL] Failed to locate texture %s in shader\n", szName));
				return GF_FALSE;
			}
			glUniform1i(loc, start_idx + i);
			GL_CHECK_ERR()
		}
		GL_CHECK_ERR()
		switch (tx->pix_fmt) {
		case GF_PIXEL_UYVY:
		case GF_PIXEL_YUYV:
		case GF_PIXEL_VYUY:
		case GF_PIXEL_YVYU:
			sprintf(szName, "_gf_%s_width", tx_name);
			loc = glGetUniformLocation(gl_program, szName);
			if (loc == -1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[GL] Failed to locate uniform %s in shader\n", szName));
				return GF_FALSE;
			}
			glUniform1f(loc, (GLfloat) tx->width);
			break;
		default:
			break;
		}
		tx->uniform_setup = GF_TRUE;
	}
#endif // !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)

	GL_CHECK_ERR()

	if (!tx->internal_textures) {
		u32 i;
		GF_Matrix txmx;

		gf_mx_init(txmx);
		for (i=0; i<tx->nb_textures; i++) {
			u32 gl_format = GL_TEXTURE_2D;
			if (tx->frame_ifce && tx->frame_ifce->get_gl_texture(tx->frame_ifce, i, &gl_format, &tx->textures[i], &txmx) != GF_OK) {
				if (!i) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[GL] Failed to get frame interface OpenGL texture ID for plane %d\n", i));
					return GF_FALSE;
				}
				break;
			}
#ifndef GPAC_USE_GLES2
			if (!gl_program)
				glEnable(gl_format);
#endif
			if (!tx->textures[i])
				break;
			glActiveTexture(texture_unit + i);
			glBindTexture(gl_format, tx->textures[i]);
			glTexParameteri(gl_format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(gl_format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(gl_format, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(gl_format, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			/*todo pass matrix to shader !!*/
		}
		tx->flip = (txmx.m[5]<0) ? GF_TRUE : GF_FALSE;

		if (tx->nb_textures) {
#ifndef GPAC_USE_GLES2
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			glClientActiveTexture(texture_unit);
#endif
		}
		return GF_TRUE;
	}
	if (tx->nb_textures>2) {
		glActiveTexture(texture_unit + 2);
		glBindTexture(GL_TEXTURE_2D, tx->textures[2]);
	}
	if (tx->nb_textures>1) {
		glActiveTexture(texture_unit + 1);
		glBindTexture(GL_TEXTURE_2D, tx->textures[1]);
	}
	if (tx->nb_textures) {
		glActiveTexture(texture_unit);
		glBindTexture(GL_TEXTURE_2D, tx->textures[0]);

#ifndef GPAC_USE_GLES2
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glClientActiveTexture(texture_unit);
#endif
	}

#ifndef GPAC_USE_GLES2
	glEnable(GL_TEXTURE_2D);
#endif

	GL_CHECK_ERR()
	return GF_TRUE;
}

void gf_gl_txw_reset(GF_GLTextureWrapper *tx)
{
	if (tx->nb_textures) {
		if (tx->internal_textures) {
			glDeleteTextures(tx->nb_textures, tx->textures);
			if (tx->pbo_state && tx->PBOs[0]) {
				glDeleteBuffers(tx->nb_textures, tx->PBOs);
			}
		}
		tx->nb_textures = 0;
	}
	tx->width = 0;
	tx->height = 0;
	tx->pix_fmt = 0;
	tx->stride = 0;
	tx->uv_stride = 0;
	tx->internal_textures = GF_FALSE;
	tx->uniform_setup = GF_FALSE;
}

#else

void gf_opengl_init()
{
}

Bool gf_gl_txw_insert_fragment_shader(u32 pix_fmt, const char *tx_name, char **f_source)
{
	return GF_FALSE;
}
Bool gf_gl_txw_setup(GF_GLTextureWrapper *tx, u32 pix_fmt, u32 width, u32 height, u32 stride, u32 uv_stride, Bool linear_interp, GF_FilterFrameInterface *frame_ifce)
{
	return GF_FALSE;
}

Bool gf_gl_txw_upload(GF_GLTextureWrapper *tx, const u8 *data, GF_FilterFrameInterface *frame_ifce)
{
	return GF_FALSE;
}

Bool gf_gl_txw_bind(GF_GLTextureWrapper *tx, const char *tx_name, u32 gl_program, u32 texture_unit)
{
	return GF_FALSE;
}
void gf_gl_txw_reset(GF_GLTextureWrapper *tx)
{
}

#endif //GPAC_DISABLE_3D
