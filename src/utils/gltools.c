/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
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

#include <gpac/tools.h>

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

#else

void gf_opengl_init()
{
}

#endif //GPAC_DISABLE_3D
