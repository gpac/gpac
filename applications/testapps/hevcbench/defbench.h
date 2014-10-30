/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2012
 *					All rights reserved
 *
 *  This file is part of GPAC - sample DASH library usage
 *
 */

#ifndef __DEF_BENCH_H__
#define __DEF_BENCH_H__

#include <gpac/isomedia.h>
#include <openHevcWrapper.h>
#include <windows.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <gpac/maths.h>

#define GL_GLEXT_PROTOTYPES

#include <GL/GL.h>
#include <gpac/isomedia.h>




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



#define DEL_SHADER(_a) if (_a) { glDeleteShader(_a); _a = 0; }
#define DEL_PROGRAM(_a) if (_a) { glDeleteProgram(_a); _a = 0; }


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
GLDECL(void, glUniform1i, (GLint location, GLint v0) )
GLDECL(void, glActiveTexture, (GLenum texture) )
GLDECL(void, glClientActiveTexture, (GLenum texture) )
GLDECL(void, glGenBuffers, (GLsizei , GLuint *) )
GLDECL(void, glDeleteBuffers, (GLsizei , GLuint *) )
GLDECL(void, glBindBuffer, (GLenum, GLuint ) )
GLDECL(void, glBufferData, (GLenum, int, void *, GLenum) )
GLDECL(void, glBufferSubData, (GLenum, int, int, void *) )
GLDECL(void *, glMapBuffer, (GLenum, GLenum) )
GLDECL(void *, glUnmapBuffer, (GLenum) )


#define GL_TEXTURE_RECTANGLE_EXT 0x84F5

#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_PIXEL_UNPACK_BUFFER_ARB   0x88EC
#define GL_STREAM_DRAW_ARB   0x88E0
#define GL_WRITE_ONLY_ARB   0x88B9
#define GL_DYNAMIC_DRAW_ARB   0x88E8

#define 	GL_TEXTURE0   0x84C0
#define 	GL_TEXTURE1   0x84C1
#define 	GL_TEXTURE2   0x84C2


#endif
