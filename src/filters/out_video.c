/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018
 *					All rights reserved
 *
 *  This file is part of GPAC / video output filter
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


#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/modules/video_out.h>
#include <gpac/color.h>


//#define GPAC_DISABLE_3D

#ifndef GPAC_DISABLE_3D

#define GL_GLEXT_PROTOTYPES

#if defined (CONFIG_DARWIN_GL)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#if defined( _LP64 ) && defined(CONFIG_DARWIN_GL)
#define GF_SHADERID u64
#else
#define GF_SHADERID u32
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
#elif defined (GPAC_USE_GLES1X)
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


#ifdef WIN32
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
#endif

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

#define TEXTURE_TYPE GL_TEXTURE_2D

#ifdef WIN32
GLDECL_STATIC(glActiveTexture);
GLDECL_STATIC(glClientActiveTexture);
GLDECL_STATIC(glCreateProgram);
GLDECL_STATIC(glDeleteProgram);
GLDECL_STATIC(glLinkProgram);
GLDECL_STATIC(glUseProgram);
GLDECL_STATIC(glCreateShader);
GLDECL_STATIC(glDeleteShader);
GLDECL_STATIC(glShaderSource);
GLDECL_STATIC(glCompileShader);
GLDECL_STATIC(glAttachShader);
GLDECL_STATIC(glDetachShader);
GLDECL_STATIC(glGetShaderiv);
GLDECL_STATIC(glGetInfoLogARB);
GLDECL_STATIC(glGetUniformLocation);
GLDECL_STATIC(glUniform1f);
GLDECL_STATIC(glUniform1i);
GLDECL_STATIC(glGenBuffers);
GLDECL_STATIC(glDeleteBuffers);
GLDECL_STATIC(glBindBuffer);
GLDECL_STATIC(glBufferData);
GLDECL_STATIC(glBufferSubData);
GLDECL_STATIC(glMapBuffer);
GLDECL_STATIC(glUnmapBuffer);
#endif


static char *glsl_yuv_shader = "#version 120\n"\
	"uniform sampler2D y_plane;\
	uniform sampler2D u_plane;\
	uniform sampler2D v_plane;\
	const vec3 offset = vec3(-0.0625, -0.5, -0.5);\
	const vec3 R_mul = vec3(1.164,  0.000,  1.596);\
	const vec3 G_mul = vec3(1.164, -0.391, -0.813);\
	const vec3 B_mul = vec3(1.164,  2.018,  0.000);\
	void main(void)  \
	{\
		vec2 texc;\
		vec3 yuv, rgb;\
		texc = gl_TexCoord[0].st;\
		yuv.x = texture2D(y_plane, texc).r; \
		yuv.y = texture2D(u_plane, texc).r; \
		yuv.z = texture2D(v_plane, texc).r; \
		yuv += offset; \
	    rgb.r = dot(yuv, R_mul); \
	    rgb.g = dot(yuv, G_mul); \
	    rgb.b = dot(yuv, B_mul); \
		gl_FragColor = vec4(rgb, 1.0);\
	}";

static char *glsl_nv12_shader = "#version 120\n"\
	"uniform sampler2D y_plane;\
	uniform sampler2D u_plane;\
	const vec3 offset = vec3(-0.0625, -0.5, -0.5);\
	const vec3 R_mul = vec3(1.164,  0.000,  1.596);\
	const vec3 G_mul = vec3(1.164, -0.391, -0.813);\
	const vec3 B_mul = vec3(1.164,  2.018,  0.000);\
	void main(void)  \
	{\
		vec2 texc;\
		vec3 yuv, rgb;\
		texc = gl_TexCoord[0].st;\
		yuv.x = texture2D(y_plane, texc).r; \
		yuv.yz = texture2D(u_plane, texc).ra; \
		yuv += offset; \
	    rgb.r = dot(yuv, R_mul); \
	    rgb.g = dot(yuv, G_mul); \
	    rgb.b = dot(yuv, B_mul); \
		gl_FragColor = vec4(rgb, 1.0);\
	}";

static char *glsl_nv21_shader = "#version 120\n"\
	"uniform sampler2D y_plane;\
	uniform sampler2D u_plane;\
	const vec3 offset = vec3(-0.0625, -0.5, -0.5);\
	const vec3 R_mul = vec3(1.164,  0.000,  1.596);\
	const vec3 G_mul = vec3(1.164, -0.391, -0.813);\
	const vec3 B_mul = vec3(1.164,  2.018,  0.000);\
	void main(void)  \
	{\
		vec2 texc;\
		vec3 yuv, rgb;\
		texc = gl_TexCoord[0].st;\
		yuv.x = texture2D(y_plane, texc).r; \
		yuv.y = texture2D(u_plane, texc).a; \
		yuv.z = texture2D(u_plane, texc).r; \
		yuv += offset; \
	    rgb.r = dot(yuv, R_mul); \
	    rgb.g = dot(yuv, G_mul); \
	    rgb.b = dot(yuv, B_mul); \
		gl_FragColor = vec4(rgb, 1.0);\
	}";

static char *glsl_uyvy_shader = "#version 120\n"\
	"uniform sampler2D y_plane;\
	uniform float width;\
	const vec3 offset = vec3(-0.0625, -0.5, -0.5);\
	const vec3 R_mul = vec3(1.164,  0.000,  1.596);\
	const vec3 G_mul = vec3(1.164, -0.391, -0.813);\
	const vec3 B_mul = vec3(1.164,  2.018,  0.000);\
	void main(void)  \
	{\
		vec2 texc, t_texc;\
		vec3 yuv, rgb;\
		vec4 uyvy;\
		float tex_s;\
		texc = gl_TexCoord[0].st;\
		t_texc = texc * vec2(1, 1);\
		uyvy = texture2D(y_plane, t_texc); \
		tex_s = texc.x*width;\
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
		gl_FragColor = vec4(rgb, 1.0);\
	}";

static char *glsl_yuyv_shader = "#version 120\n"\
	"uniform sampler2D y_plane;\
	uniform float width;\
	const vec3 offset = vec3(-0.0625, -0.5, -0.5);\
	const vec3 R_mul = vec3(1.164,  0.000,  1.596);\
	const vec3 G_mul = vec3(1.164, -0.391, -0.813);\
	const vec3 B_mul = vec3(1.164,  2.018,  0.000);\
	void main(void)  \
	{\
		vec2 texc, t_texc;\
		vec3 yuv, rgb;\
		vec4 yuyv;\
		float tex_s;\
		texc = gl_TexCoord[0].st;\
		t_texc = texc * vec2(1, 1);\
		yuyv = texture2D(y_plane, t_texc); \
		tex_s = texc.x*width;\
		if (tex_s - (2.0 * floor(tex_s/2.0)) == 1.0) {\
        	yuyv.r = yuyv.b; \
    	}\
    	yuv.r = yuyv.r;\
    	yuv.g = yuyv.g;\
    	yuv.b = yuyv.a;\
		yuv += offset; \
	    rgb.r = dot(yuv, R_mul); \
	    rgb.g = dot(yuv, G_mul); \
	    rgb.b = dot(yuv, B_mul); \
		gl_FragColor = vec4(rgb, 1.0);\
	}";

static char *glsl_rgb_shader = "#version 120\n"\
	"uniform sampler2D rgbtx;\
	uniform int rgb_mode;\
	void main(void)  \
	{\
		vec2 texc;\
		vec4 col;\
		texc = gl_TexCoord[0].st;\
		col = texture2D(rgbtx, texc); \
		if (rgb_mode==1) {\
			gl_FragColor.r = col.b;\
			gl_FragColor.g = col.g;\
			gl_FragColor.b = col.r;\
		} else if (rgb_mode==2) {\
			gl_FragColor.r = col.g;\
			gl_FragColor.g = col.b;\
			gl_FragColor.b = col.a;\
		} else if (rgb_mode==3) {\
			gl_FragColor.r = col.g;\
			gl_FragColor.g = col.a;\
			gl_FragColor.b = col.b;\
		} else {\
			gl_FragColor = col;\
		}\
	}";

static char *default_glsl_vertex = "\
	void main(void)\
	{\
		gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\
		gl_TexCoord[0] = gl_MultiTexCoord0;\
	}";


#endif

typedef enum
{
	MODE_GL,
	MODE_GL_PBO,
	MODE_2D,
	MODE_2D_SOFT,
} GF_VideoOutMode;

typedef struct
{
	//options
	char *drv;
	GF_VideoOutMode mode;
	Bool vsync, linear, fullscreen;
	GF_Fraction dur;
	Double speed, hold;
	u32 back;
	GF_PropVec2i size;
	GF_PropVec2i pos;


	GF_FilterPid *pid;
	u32 width, height, stride, pfmt, timescale;
	GF_Fraction fps;

	GF_User *user;
	GF_VideoOutput *video_out;

	u32 pck_offset;
	u64 first_cts;
	u64 clock_at_first_cts;
	Bool aborted;
	u32 display_width, display_height;
	Bool display_changed;
	Float dh, dw, oh, ow;
	Bool has_alpha;
	u32 nb_frames;
	Bool swap_uv;

#ifndef GPAC_DISABLE_3D
	GLint glsl_program;
	GF_SHADERID vertex_shader;
	GF_SHADERID fragment_shader;
	Bool first_tx_load;
	GLint txid[3];

	GLint pbo_Y;
	GLint pbo_U;
	GLint pbo_V;
	GLint memory_format;
	u32 num_textures;
	u32 bytes_per_pix;
	GLint pixel_format;
#endif // GPAC_DISABLE_3D

	u32 uv_w, uv_h, uv_stride, bit_depth;
	Bool is_yuv;
} GF_VideoOutCtx;


#ifndef GPAC_DISABLE_3D
static Bool vout_compile_shader(GF_SHADERID shader_id, const char *name, const char *source)
{
	GLint blen = 0;
	GLsizei slen = 0;
	u32 len;
	if (!source || !shader_id) return 0;
	len = (u32) strlen(source);
	glShaderSource(shader_id, 1, &source, &len);
	glCompileShader(shader_id);

	glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH , &blen);
	if (blen > 1) {
		char* compiler_log = (char*) gf_malloc(blen);
#ifdef CONFIG_DARWIN_GL
		glGetInfoLogARB((GLhandleARB) shader_id, blen, &slen, compiler_log);
#else
		glGetInfoLogARB(shader_id, blen, &slen, compiler_log);
#endif
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[GLSL] Failed to compile shader %s: %s\n", name, compiler_log));
		gf_free (compiler_log);
		return 0;
	}
	return 1;
}
#endif

static GF_Err vout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_Event evt;
	int rgb_mode=0;
	const GF_PropertyValue *p;
	u32 w, h, pfmt, stride, timescale;
	GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) gf_filter_get_udta(filter);

	if (is_remove) {
		assert(ctx->pid==pid);
		ctx->pid=NULL;
		return GF_OK;
	}
	assert(!ctx->pid || (ctx->pid==pid));
	gf_filter_pid_check_caps(pid);

	w = h = pfmt = timescale = stride = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) timescale = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) w = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) h = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
	if (p) pfmt = p->value.uint;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
	if (p) ctx->fps = p->value.frac;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE);
	if (p) stride = p->value.uint;

	if ((ctx->width==w) && (ctx->height == h) && (ctx->pfmt == pfmt) ) return GF_OK;

	if (!ctx->pid) {
		GF_FilterEvent fevt;
		GF_FEVT_INIT(fevt, GF_FEVT_PLAY, pid);
		gf_filter_pid_send_event(pid, &fevt);

		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_SET_CAPTION;
		evt.caption.caption = gf_filter_pid_original_args	(pid);
		ctx->video_out->ProcessEvent(ctx->video_out, &evt);
	}

	if ((ctx->width!=w) || (ctx->height != h) ) {
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.width = w;
		evt.setup.height = h;
		if ((ctx->size.x>0) && (ctx->size.y>0)) {
			evt.setup.width = ctx->size.x;
			evt.setup.height = ctx->size.y;
		}

#ifndef GPAC_DISABLE_3D
		if (ctx->mode<MODE_2D) {
			evt.setup.opengl_mode = 1;
		} else
#endif
		{
			evt.setup.back_buffer = 1;
		}
		evt.setup.disable_vsync = !ctx->vsync;
		ctx->video_out->ProcessEvent(ctx->video_out, &evt);
		ctx->display_width = evt.setup.width;
		ctx->display_height = evt.setup.height;
		ctx->display_changed = GF_TRUE;
	} else if ((ctx->size.x>0) && (ctx->size.y>0)) {
		ctx->display_width = ctx->size.x;
		ctx->display_height = ctx->size.y;
		ctx->display_changed = GF_TRUE;
	}
	if (ctx->fullscreen) {
		u32 nw=0, nh=0;
		ctx->video_out->SetFullScreen(ctx->video_out, GF_TRUE, &nw, &nh);
	} else if ((ctx->pos.x!=-1) && (ctx->pos.y!=-1)) {
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_MOVE;
		evt.move.relative = 0;
		evt.move.x = ctx->pos.x;
		evt.move.y = ctx->pos.y;
		ctx->video_out->ProcessEvent(ctx->video_out, &evt);
	}


	ctx->pid = pid;
	ctx->timescale = timescale;
	ctx->width = w;
	ctx->height = h;
	ctx->pfmt = pfmt;
	ctx->stride = stride ? stride : w;
	ctx->uv_w = ctx->uv_h = ctx->uv_stride = 0;
	ctx->bit_depth = 0;
	ctx->has_alpha = GF_FALSE;
	ctx->swap_uv = GF_FALSE;

#ifndef GPAC_DISABLE_3D
	ctx->pixel_format = GL_LUMINANCE;
#endif
	switch (ctx->pfmt) {
	case GF_PIXEL_YUV444_10:
		ctx->bit_depth = 10;
	case GF_PIXEL_YUV444:
		ctx->uv_w = ctx->width;
		ctx->uv_h = ctx->height;
		ctx->uv_stride = ctx->stride;
		ctx->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_YUV422_10:
		ctx->bit_depth = 10;
	case GF_PIXEL_YUV422:
		ctx->uv_w = ctx->width;
		ctx->uv_h = ctx->height/2;
		ctx->uv_stride = ctx->stride;
		ctx->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_YV12_10:
		ctx->bit_depth = 10;
	case GF_PIXEL_YV12:
		ctx->uv_w = ctx->width/2;
		ctx->uv_h = ctx->height/2;
		ctx->uv_stride = ctx->stride/2;
		ctx->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
		ctx->uv_w = ctx->width/2;
		ctx->uv_h = ctx->height/2;
		ctx->uv_stride = ctx->stride/2;
		ctx->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_UYVY:
	case GF_PIXEL_YUYV:
		ctx->uv_w = ctx->width/2;
		ctx->uv_h = ctx->height;
		ctx->uv_stride = ctx->stride/2;
		ctx->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_GREYSCALE:
#ifndef GPAC_DISABLE_3D
		ctx->pixel_format = GL_LUMINANCE;
		ctx->bytes_per_pix = 1;
#endif
		break;
	case GF_PIXEL_ALPHAGREY:
#ifndef GPAC_DISABLE_3D
		ctx->pixel_format = GL_LUMINANCE_ALPHA;
		ctx->bytes_per_pix = 2;
#endif
		break;
	case GF_PIXEL_RGB:
#ifndef GPAC_DISABLE_3D
		ctx->pixel_format = GL_RGB;
		ctx->bytes_per_pix = 3;
#endif
		break;
	case GF_PIXEL_BGR:
#ifndef GPAC_DISABLE_3D
		ctx->pixel_format = GL_RGB;
		ctx->bytes_per_pix = 3;
		rgb_mode=1;
#endif
		break;
	case GF_PIXEL_BGRX:
#ifndef GPAC_DISABLE_3D
		ctx->pixel_format = GL_RGBA;
		ctx->bytes_per_pix = 4;
		rgb_mode=1;
#endif
		break;
	case GF_PIXEL_XRGB:
#ifndef GPAC_DISABLE_3D
		ctx->pixel_format = GL_RGBA;
		ctx->bytes_per_pix = 4;
		rgb_mode=2;
#endif
		break;
	case GF_PIXEL_XBGR:
#ifndef GPAC_DISABLE_3D
		ctx->pixel_format = GL_RGBA;
		ctx->bytes_per_pix = 4;
		rgb_mode=3;
#endif
		break;

	case GF_PIXEL_RGBA:
		ctx->has_alpha = GF_TRUE;
	case GF_PIXEL_RGBX:
#ifndef GPAC_DISABLE_3D
		ctx->pixel_format = GL_RGBA;
		ctx->bytes_per_pix = 4;
#endif
		break;
	case GF_PIXEL_ARGB:
		ctx->has_alpha = GF_TRUE;
#ifndef GPAC_DISABLE_3D
		ctx->pixel_format = GL_RGBA;
		ctx->bytes_per_pix = 4;
#endif
		break;
	case 0:
		//not yet set, happens with some decoders/stream settings - wait until first frame is available and PF is known
		return GF_OK;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[VideoOut] Pixel format %s unknown\n", gf_4cc_to_str(ctx->pfmt)));
		return GF_OK;
	}

#ifndef GPAC_DISABLE_3D
	if (ctx->is_yuv) {
		ctx->bytes_per_pix = (ctx->bit_depth > 8) ? 2 : 1;
	}

	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_SET_GL;
	ctx->video_out->ProcessEvent(ctx->video_out, &evt);

	DEL_SHADER(ctx->vertex_shader);
	DEL_SHADER(ctx->fragment_shader);
	DEL_PROGRAM(ctx->glsl_program );
	if (ctx->pbo_Y) glDeleteBuffers(1, &ctx->pbo_Y);
	if (ctx->pbo_U) glDeleteBuffers(1, &ctx->pbo_U);
	if (ctx->pbo_V) glDeleteBuffers(1, &ctx->pbo_V);
	ctx->pbo_Y = ctx->pbo_U = ctx->pbo_V = 0;

	if (ctx->num_textures && ctx->txid[0])
		glDeleteTextures(ctx->num_textures, ctx->txid);
	ctx->num_textures = 0;

	if (ctx->mode<MODE_2D) {
		u32 i;
		GF_Matrix mx;
		Float hw, hh;

		ctx->memory_format = GL_UNSIGNED_BYTE;

		ctx->glsl_program = glCreateProgram();
		ctx->vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		vout_compile_shader(ctx->vertex_shader, "vertex", default_glsl_vertex);

		ctx->fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		if (ctx->pfmt==GF_PIXEL_NV21) {
			ctx->num_textures = 2;
			vout_compile_shader(ctx->fragment_shader, "fragment", glsl_nv21_shader);
		} else if (ctx->pfmt==GF_PIXEL_NV12) {
			ctx->num_textures = 2;
			vout_compile_shader(ctx->fragment_shader, "fragment", glsl_nv12_shader);
		} else if (ctx->pfmt==GF_PIXEL_UYVY) {
			ctx->num_textures = 1;
			vout_compile_shader(ctx->fragment_shader, "fragment", glsl_uyvy_shader);
		} else if (ctx->pfmt==GF_PIXEL_YUYV) {
			ctx->num_textures = 1;
			vout_compile_shader(ctx->fragment_shader, "fragment", glsl_yuyv_shader);
		} else if (ctx->is_yuv) {
			ctx->num_textures = 3;
			vout_compile_shader(ctx->fragment_shader, "fragment", glsl_yuv_shader);
		} else {
			ctx->num_textures = 1;
			vout_compile_shader(ctx->fragment_shader, "fragment", glsl_rgb_shader);
		}

		glAttachShader(ctx->glsl_program, ctx->vertex_shader);
		glAttachShader(ctx->glsl_program, ctx->fragment_shader);
		glLinkProgram(ctx->glsl_program);

		glGenTextures(ctx->num_textures, ctx->txid);
		for (i=0; i<ctx->num_textures; i++) {

			glEnable(TEXTURE_TYPE);
			glBindTexture(TEXTURE_TYPE, ctx->txid[i] );

			if (ctx->is_yuv && ctx->bit_depth>8) {
				glPixelTransferi(GL_RED_SCALE, 64);
				ctx->memory_format = GL_UNSIGNED_SHORT;
				glPixelStorei(GL_UNPACK_ALIGNMENT, 2);
			} else {
				ctx->memory_format = GL_UNSIGNED_BYTE;
				glPixelTransferi(GL_RED_SCALE, 1);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			}

			glTexParameteri(TEXTURE_TYPE, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(TEXTURE_TYPE, GL_TEXTURE_WRAP_T, GL_CLAMP);
			glTexParameteri(TEXTURE_TYPE, GL_TEXTURE_MAG_FILTER, ctx->linear ? GL_LINEAR : GL_NEAREST);
			glTexParameteri(TEXTURE_TYPE, GL_TEXTURE_MIN_FILTER, ctx->linear ? GL_LINEAR : GL_NEAREST);

			glDisable(TEXTURE_TYPE);
		}

		//sets uniforms: y, u, v textures point to texture slots 0, 1 and 2
		glUseProgram(ctx->glsl_program);
		for (i=0; i<ctx->num_textures; i++) {
			GLint loc;
			const char *txname = (i==0) ? "y_plane" : (i==1) ? "u_plane" : "v_plane";
			if (! ctx->is_yuv) txname = "rgbtx";
			loc = glGetUniformLocation(ctx->glsl_program, txname);
			if (loc == -1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Failed to locate texture %s in %s shader\n", txname, ctx->is_yuv ? "YUV" : "RGB"));
				continue;
			}
			glUniform1i(loc, i);
		}
		if (ctx->pfmt==GF_PIXEL_UYVY) {
			GLint loc;
			loc = glGetUniformLocation(ctx->glsl_program, "width");
			if (loc == -1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Failed to locate width uniform in shader\n"));
			} else {
				glUniform1f(loc, (GLfloat) ctx->width);
			}
		} else if (!ctx->is_yuv) {
			GLint loc;
			loc = glGetUniformLocation(ctx->glsl_program, "rgb_mode");
			if (loc == -1) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Failed to locate rgb_mode uniform in shader\n"));
			} else {
				glUniform1i(loc, rgb_mode);
			}
		}
		glUseProgram(0);

#ifdef WIN32
		if (glMapBuffer==NULL) ctx->pbo = GF_FALSE;
#endif

		ctx->first_tx_load = (ctx->mode==MODE_GL_PBO) ? GF_FALSE : GF_TRUE;
		if (ctx->is_yuv && (ctx->mode==MODE_GL_PBO)) {
			glGenBuffers(1, &ctx->pbo_Y);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_Y);

			if (ctx->num_textures>1) {
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->width*ctx->height, NULL, GL_DYNAMIC_DRAW_ARB);

				glGenBuffers(1, &ctx->pbo_U);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_U);
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->uv_w*ctx->uv_h, NULL, GL_DYNAMIC_DRAW_ARB);
			} else {
				//packed YUV
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, 4*ctx->width/2*ctx->height, NULL, GL_DYNAMIC_DRAW_ARB);

			}

			if (ctx->num_textures==3) {
				glGenBuffers(1, &ctx->pbo_V);
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_V);
				glBufferData(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->uv_w*ctx->uv_h, NULL, GL_DYNAMIC_DRAW_ARB);
			}

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
		}

		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, ctx->width, ctx->height);

		gf_mx_init(mx);
		hw = ((Float)ctx->width)/2;
		hh = ((Float)ctx->height)/2;
		gf_mx_ortho(&mx, -hw, hw, -hh, hh, 50, -50);
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(mx.m);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glClear(GL_DEPTH_BUFFER_BIT);
		glDisable(GL_NORMALIZE);
		glDisable(GL_DEPTH_TEST);
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		if (ctx->has_alpha) {
			glEnable(GL_BLEND);
		} else {
			glDisable(GL_BLEND);
		}
	}
#endif
	return GF_OK;
}

static Bool vout_on_event(void *cbk, GF_Event *evt)
{
	GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) cbk;
	if (evt->type==GF_EVENT_SIZE) {
		if ((ctx->display_width != evt->size.width) || (ctx->display_height != evt->size.height)) {
			ctx->display_width = evt->size.width;
			ctx->display_height = evt->size.height;
			ctx->display_changed = GF_TRUE;
		}
	}
	if (ctx->user->EventProc) return ctx->user->EventProc(ctx->user->opaque, evt);
	return GF_TRUE;
}

static GF_Err vout_initialize(GF_Filter *filter)
{
	const char *sOpt;
	GF_Err e;
	GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) gf_filter_get_udta(filter);
	ctx->user = gf_fs_get_user( gf_filter_get_session(filter) );

	if (!ctx->user) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] No user/modules defined, cannot load video output\n"));
		return GF_IO_ERR;
	}
	if (ctx->drv) {
		ctx->video_out = (GF_VideoOutput *) gf_modules_load_interface_by_name(ctx->user->modules, ctx->drv, GF_VIDEO_OUTPUT_INTERFACE);
		if (!ctx->video_out->Flush || !ctx->video_out->Setup) {
			gf_modules_close_interface((GF_BaseInterface *)ctx->video_out);
			ctx->video_out = NULL;
		}
	}
	/*get a prefered compositor*/
	if (!ctx->video_out) {
		sOpt = gf_cfg_get_key(ctx->user->config, "Video", "DriverName");
		if (sOpt) {
			ctx->video_out = (GF_VideoOutput *) gf_modules_load_interface_by_name(ctx->user->modules, sOpt, GF_VIDEO_OUTPUT_INTERFACE);

			if (!ctx->video_out->Flush || !ctx->video_out->Setup) {
				gf_modules_close_interface((GF_BaseInterface *)ctx->video_out);
				ctx->video_out = NULL;
			}
		}
	}
	if (!ctx->video_out) {
		u32 i, count = gf_modules_get_count(ctx->user->modules);
		for (i=0; i<count; i++) {
			ctx->video_out = (GF_VideoOutput *) gf_modules_load_interface(ctx->user->modules, i, GF_VIDEO_OUTPUT_INTERFACE);
			if (!ctx->video_out) continue;

			//no more support for raw out, deprecated
			if (!stricmp(ctx->video_out->module_name, "Raw Video Output")
				|| !ctx->video_out->Flush || !ctx->video_out->Setup) {
				gf_modules_close_interface((GF_BaseInterface *)ctx->video_out);
				ctx->video_out = NULL;
				continue;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] Audio output module %s loaded\n", ctx->video_out->module_name));

			if (ctx->video_out->Flush) {
				/*remember the module we use*/
				gf_cfg_set_key(ctx->user->config, "Video", "DriverName", ctx->video_out->module_name);
				break;
			}
			gf_modules_close_interface((GF_BaseInterface *)ctx->video_out);
			ctx->video_out = NULL;
		}
	}

	/*if not init we run with a NULL audio compositor*/
	if (!ctx->video_out) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] No audio output modules found, cannot load audio output\n"));
		return GF_IO_ERR;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] Setting up video module %s\n", ctx->video_out->module_name));
	ctx->video_out->on_event = vout_on_event;
	ctx->video_out->evt_cbk_hdl = ctx;

	e = ctx->video_out->Setup(ctx->video_out, ctx->user->os_window_handler, ctx->user->os_display, ctx->user->init_flags);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("Failed to Setup Video Driver %s!\n", ctx->video_out->module_name));
		gf_modules_close_interface((GF_BaseInterface *)ctx->video_out);
		ctx->video_out = NULL;
		return e;
	}

	if ( !(ctx->video_out->hw_caps & GF_VIDEO_HW_OPENGL)
#ifdef GPAC_DISABLE_3D
	|| (1)
#endif
	) {
		if (ctx->mode < MODE_2D) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("No openGL support - using 2D rasterizer!\n", ctx->video_out->module_name));
			ctx->mode = MODE_2D;
		}
	}

#ifndef GPAC_DISABLE_3D

#ifdef WIN32
	GET_GLFUN(glActiveTexture);
	GET_GLFUN(glClientActiveTexture);
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
	GET_GLFUN(glUniform1f);
	GET_GLFUN(glUniform1i);
	GET_GLFUN(glGenBuffers);
	GET_GLFUN(glDeleteBuffers);
	GET_GLFUN(glBindBuffer);
	GET_GLFUN(glBufferData);
	GET_GLFUN(glBufferSubData);
	GET_GLFUN(glMapBuffer);
	GET_GLFUN(glUnmapBuffer);
#endif

#endif

	return GF_OK;
}

static void vout_finalize(GF_Filter *filter)
{
	GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) gf_filter_get_udta(filter);

	if (ctx->nb_frames==1) {
		gf_sleep(ctx->hold*1000);
	}

#ifndef GPAC_DISABLE_3D
	DEL_SHADER(ctx->vertex_shader);
	DEL_SHADER(ctx->fragment_shader);
	DEL_PROGRAM(ctx->glsl_program );
	if (ctx->pbo_Y) glDeleteBuffers(1, &ctx->pbo_Y);
	if (ctx->pbo_U) glDeleteBuffers(1, &ctx->pbo_U);
	if (ctx->pbo_V) glDeleteBuffers(1, &ctx->pbo_V);
	if (ctx->num_textures && ctx->txid[0])
		glDeleteTextures(ctx->num_textures, ctx->txid);
#endif //GPAC_DISABLE_3D

	/*stop and shutdown*/
	ctx->video_out->Shutdown(ctx->video_out);
	gf_modules_close_interface((GF_BaseInterface *)ctx->video_out);
	ctx->video_out = NULL;
}

#ifndef GPAC_DISABLE_3D
static void vout_draw_gl(GF_VideoOutCtx *ctx, GF_FilterPacket *pck)
{
	u32 needs_stride = 0;
	Float dw, dh;
	char *data;
	GF_Event evt;
	u32 size, stride_luma, stride_chroma;
	char *pY=NULL, *pU=NULL, *pV=NULL;

	if (!ctx->glsl_program) return;

	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_SET_GL;
	ctx->video_out->ProcessEvent(ctx->video_out, &evt);

	if (ctx->display_changed) {
		GF_Matrix mx;
		Float hw, hh;
		glViewport(0, 0, ctx->display_width, ctx->display_height);

		gf_mx_init(mx);
		hw = ((Float)ctx->display_width)/2;
		hh = ((Float)ctx->display_height)/2;
		gf_mx_ortho(&mx, -hw, hw, -hh, hh, 10, -5);
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(mx.m);

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		//if we fill width to display width and height is outside
		if (ctx->display_width * ctx->height / ctx->width > ctx->display_height) {
			ctx->dw = ctx->display_height * ctx->width / ctx->height;
			ctx->dh = ctx->display_height;
			ctx->oh = 0;
			ctx->ow = (ctx->display_width - ctx->dw ) / 2;
		} else {
			ctx->dh = ctx->display_width * ctx->height / ctx->width;
			ctx->dw = ctx->display_width;
			ctx->ow = 0;
			ctx->oh = (ctx->display_height - ctx->dh ) / 2;
		}
		ctx->display_changed = GF_FALSE;
	}
	if (ctx->has_alpha) {
		Float r, g, b;
		r = GF_COL_R(ctx->back); r /= 255;
		g = GF_COL_G(ctx->back); g /= 255;
		b = GF_COL_B(ctx->back); b /= 255;
		glClearColor(r, g, b, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	stride_luma = ctx->stride;
	stride_chroma = ctx->uv_stride;

	data = (char*) gf_filter_pck_get_data(pck, &size);
	if (!data) {
		GF_Err e;
		GF_FilterHWFrame *hw_frame = gf_filter_pck_get_hw_frame(pck);
		if (! hw_frame->get_plane) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Hardware GL texture blit not yet implemented\n"));
			return;
		}
		e = hw_frame->get_plane(hw_frame, 0, (const u8 **) &pY, &stride_luma);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error fetching chroma plane from hardware frame\n"));
			return;
		}
		data = pY;
		pU = pV = NULL;
		if (ctx->num_textures>1) {
			e = hw_frame->get_plane(hw_frame, 1, (const u8 **) &pU, &stride_chroma);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error fetching luma U plane from hardware frame\n"));
				return;
			}
		}
		if (ctx->num_textures>2) {
			e = hw_frame->get_plane(hw_frame, 2, (const u8 **) &pV, &stride_chroma);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error fetching luma V plane from hardware frame\n"));
				return;
			}
		}
	} else {
		if (ctx->is_yuv) {
			pY = data;
			if (ctx->num_textures>1) {
				pU = pY + ctx->stride*ctx->height;
				if (ctx->num_textures>2) {
					pV = pU + ctx->uv_stride * ctx->uv_h;
				}
			}
		}
	}

	if (stride_luma != ctx->width) {
		if (ctx->bit_depth==10) {
			if (ctx->stride != 2*ctx->width) {
				needs_stride = ctx->stride / 2;
			}
		} else {
			needs_stride = ctx->stride;
		}
	}
	if (ctx->swap_uv) {
		char *tmp = pU;
		pU = pV;
		pV = tmp;
	}

	glEnable(TEXTURE_TYPE);

	if (!ctx->is_yuv) {
		glBindTexture(TEXTURE_TYPE, ctx->txid[0] );
		if (ctx->first_tx_load) {
			glTexImage2D(TEXTURE_TYPE, 0, ctx->pixel_format, ctx->width, ctx->height, 0, ctx->pixel_format, GL_UNSIGNED_BYTE, data);
			ctx->first_tx_load = GF_FALSE;
		} else {
			glTexSubImage2D(TEXTURE_TYPE, 0, 0, 0, ctx->width, ctx->height, ctx->pixel_format, ctx->memory_format, data);
		}
	}
	else if (ctx->mode==MODE_GL_PBO) {
		u32 i, linesize, count, p_stride;
		u8 *ptr;

		//packed YUV
		if (ctx->num_textures==1) {

			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_Y);
			ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

			linesize = ctx->width/2 * ctx->bytes_per_pix * 4;
			p_stride = ctx->stride/2 * ctx->bytes_per_pix * 4;
			count = ctx->height;

			for (i=0; i<count; i++) {
				memcpy(ptr, pY, linesize);
				pY += p_stride;
				ptr += linesize;
			}

			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

			glBindTexture(TEXTURE_TYPE, ctx->txid[0] );
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_Y);
			if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride);
			glTexImage2D(TEXTURE_TYPE, 0, GL_RGBA, ctx->width/2, ctx->height, 0, GL_RGBA, ctx->memory_format, NULL);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		} else {
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_Y);
			ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

			linesize = ctx->width*ctx->bytes_per_pix;
			p_stride = ctx->stride;
			count = ctx->height;

			for (i=0; i<count; i++) {
				memcpy(ptr, pY, linesize);
				pY += p_stride;
				ptr += linesize;
			}

			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_U);
			ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

			linesize = ctx->uv_w * ctx->bytes_per_pix;
			p_stride = ctx->uv_stride;
			count = ctx->uv_h;
			//NV12 and  NV21
			if (!pV) {
				linesize *= 2;
				p_stride *= 2;
			}

			for (i=0; i<count; i++) {
				memcpy(ptr, pU, linesize);
				pU += p_stride;
				ptr += linesize;
			}

			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);

			if (pV) {
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_V);
				ptr =(u8 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, GL_WRITE_ONLY_ARB);

				for (i=0; i<count; i++) {
					memcpy(ptr, pV, linesize);
					pV += p_stride;
					ptr += linesize;
				}

				glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER_ARB);
			}

			needs_stride=0;

			glBindTexture(TEXTURE_TYPE, ctx->txid[0] );
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_Y);
			if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride);
			glTexImage2D(TEXTURE_TYPE, 0, GL_LUMINANCE, ctx->width, ctx->height, 0, ctx->pixel_format, ctx->memory_format, NULL);

			glBindTexture(TEXTURE_TYPE, ctx->txid[1] );
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_U);
			if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
			glTexImage2D(TEXTURE_TYPE, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, ctx->uv_w, ctx->uv_h, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, ctx->memory_format, NULL);

			if (pV) {
				glBindTexture(TEXTURE_TYPE, ctx->txid[2] );
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, ctx->pbo_V);
				if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
				glTexImage2D(TEXTURE_TYPE, 0, GL_LUMINANCE, ctx->uv_w, ctx->uv_h, 0, ctx->pixel_format, ctx->memory_format, NULL);
			}

			glBindBuffer(GL_PIXEL_UNPACK_BUFFER_ARB, 0);
			if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		}
	}
	else if ((ctx->pfmt==GF_PIXEL_UYVY) || (ctx->pfmt==GF_PIXEL_YUYV)) {
		glBindTexture(TEXTURE_TYPE, ctx->txid[0] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride);
		if (ctx->first_tx_load) {
			glTexImage2D(TEXTURE_TYPE, 0, GL_RGBA, ctx->width/2, ctx->height, 0, GL_RGBA, ctx->memory_format, pY);
			ctx->first_tx_load = GF_FALSE;
		} else {
			glTexSubImage2D(TEXTURE_TYPE, 0, 0, 0, ctx->width/2, ctx->height, GL_RGBA, ctx->memory_format, pY);

		}
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
	else if (ctx->first_tx_load) {
		glBindTexture(TEXTURE_TYPE, ctx->txid[0] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride);
		glTexImage2D(TEXTURE_TYPE, 0, GL_LUMINANCE, ctx->width, ctx->height, 0, ctx->pixel_format, ctx->memory_format, pY);

		glBindTexture(TEXTURE_TYPE, ctx->txid[1] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, ctx->uv_stride);
		glTexImage2D(TEXTURE_TYPE, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, ctx->uv_w, ctx->uv_h, 0, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, ctx->memory_format, pU);

		if (pV) {
			glBindTexture(TEXTURE_TYPE, ctx->txid[2] );
			if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, ctx->uv_stride);
			glTexImage2D(TEXTURE_TYPE, 0, GL_LUMINANCE, ctx->uv_w, ctx->uv_h, 0, ctx->pixel_format, ctx->memory_format, pV);
		}

		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		ctx->first_tx_load = GF_FALSE;
	}
	else {
		glBindTexture(TEXTURE_TYPE, ctx->txid[0] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride);
		glTexSubImage2D(TEXTURE_TYPE, 0, 0, 0, ctx->width, ctx->height, ctx->pixel_format, ctx->memory_format, pY);
		glBindTexture(TEXTURE_TYPE, 0);

		glBindTexture(TEXTURE_TYPE, ctx->txid[1] );
		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
		glTexSubImage2D(TEXTURE_TYPE, 0, 0, 0, ctx->uv_w, ctx->uv_h, pV ? GL_LUMINANCE : GL_LUMINANCE_ALPHA, ctx->memory_format, pU);
		glBindTexture(TEXTURE_TYPE, 0);

		if (pV) {
			glBindTexture(TEXTURE_TYPE, ctx->txid[2] );
			if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, needs_stride/2);
			glTexSubImage2D(TEXTURE_TYPE, 0, 0, 0, ctx->uv_w, ctx->uv_h, ctx->pixel_format, ctx->memory_format, pV);
			glBindTexture(TEXTURE_TYPE, 0);
		}

		if (needs_stride) glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}

	glUseProgram(ctx->glsl_program);

	if (ctx->num_textures>2) {
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(TEXTURE_TYPE, ctx->txid[2]);
	}
	if (ctx->num_textures>1) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(TEXTURE_TYPE, ctx->txid[1]);
	}
	if (ctx->num_textures) {
		glActiveTexture(GL_TEXTURE0 );
		glBindTexture(TEXTURE_TYPE, ctx->txid[0]);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glClientActiveTexture(GL_TEXTURE0);
	}

	dw = ((Float)ctx->dw)/2;
	dh = ((Float)ctx->dh)/2;

	glBegin(GL_QUADS);

	glTexCoord2f(1, 0);
	glVertex3f(dw, dh, 0);

	glTexCoord2f(1, 1);
	glVertex3f(dw, -dh, 0);

	glTexCoord2f(0, 1);
	glVertex3f(-dw, -dh, 0);

	glTexCoord2f(0, 0);
	glVertex3f(-dw, dh, 0);

	glEnd();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(TEXTURE_TYPE, 0);
	glDisable(TEXTURE_TYPE);
	glUseProgram(0);

	ctx->video_out->Flush(ctx->video_out, NULL);
}
#endif

void vout_draw_2d(GF_VideoOutCtx *ctx, GF_FilterPacket *pck)
{
	char *data;
	u32 size;
	GF_Err e;
	GF_VideoSurface src_surf;
	GF_Window dst_wnd;

	memset(&src_surf, 0, sizeof(GF_VideoSurface));
	src_surf.width = ctx->width;
	src_surf.height = ctx->height;
	src_surf.pitch_x = 0;
	src_surf.pitch_y = ctx->stride;
	src_surf.pixel_format = ctx->pfmt;
	src_surf.global_alpha = 0xFF;

	data = (char *) gf_filter_pck_get_data(pck, &size);
	if (!data) {
		GF_Err e;
		u32 stride_luma, stride_chroma;
		GF_FilterHWFrame *hw_frame = gf_filter_pck_get_hw_frame(pck);
		if (! hw_frame->get_plane) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Hardware GL texture blit not supported with non-GL blitter\n"));
			return;
		}
		e = hw_frame->get_plane(hw_frame, 0, (const u8 **) &src_surf.video_buffer, &stride_luma);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error fetching chroma plane from hardware frame\n"));
			return;
		}
		if (ctx->is_yuv) {
			e = hw_frame->get_plane(hw_frame, 1, (const u8 **) &src_surf.u_ptr, &stride_chroma);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error fetching luma U plane from hardware frame\n"));
				return;
			}
		}
		if ((ctx->pfmt!= GF_PIXEL_NV12) && (ctx->pfmt!= GF_PIXEL_NV21)) {
			e = hw_frame->get_plane(hw_frame, 2, (const u8 **) &src_surf.v_ptr, &stride_chroma);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error fetching luma V plane from hardware frame\n"));
				return;
			}
		}
	} else {
		src_surf.video_buffer = data;
	}


	if (ctx->display_changed) {
		GF_Event evt;

		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.back_buffer = 1;
		evt.setup.width = ctx->display_width;
		evt.setup.height = ctx->display_height;
		e = ctx->video_out->ProcessEvent(ctx->video_out, &evt);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error resizing 2D backbuffer %s\n", gf_error_to_string(e) ));
		}
		//if we fill width to display width and height is outside
		if (ctx->display_width * ctx->height / ctx->width > ctx->display_height) {
			ctx->dw = ctx->display_height * ctx->width / ctx->height;
			ctx->dh = ctx->display_height;
			ctx->oh = 0;
			ctx->ow = (ctx->display_width - ctx->dw ) / 2;
		} else {
			ctx->dh = ctx->display_width * ctx->height / ctx->width;
			ctx->dw = ctx->display_width;
			ctx->ow = 0;
			ctx->oh = (ctx->display_height - ctx->dh ) / 2;
		}
		ctx->display_changed = GF_FALSE;
	}

	dst_wnd.x = (u32) ctx->ow;
	dst_wnd.y = (u32) ctx->oh;
	dst_wnd.w = (u32) ctx->dw;
	dst_wnd.h = (u32) ctx->dh;
	e = GF_EOS;

	if ((ctx->mode!=MODE_2D_SOFT) && ctx->video_out->Blit) {
		e = ctx->video_out->Blit(ctx->video_out, &src_surf, NULL, &dst_wnd, 0);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error bliting surface %s - retrying in software mode\n", gf_error_to_string(e) ));
		}
	}
	if (e) {
		GF_VideoSurface backbuffer;
		GF_Window src_wnd;
		src_wnd.x = src_wnd.y = 0;
		src_wnd.w = ctx->width;
		src_wnd.h = ctx->height;


		e = ctx->video_out->LockBackBuffer(ctx->video_out, &backbuffer, GF_TRUE);
		if (!e) {
			gf_stretch_bits(&backbuffer, &src_surf, &dst_wnd, &src_wnd, 0xFF, GF_FALSE, NULL, NULL);
			ctx->video_out->LockBackBuffer(ctx->video_out, &backbuffer, GF_FALSE);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Cannot lock back buffer - Error %s\n", gf_error_to_string(e) ));
		}
	}

	if (e==GF_OK)
		ctx->video_out->Flush(ctx->video_out, NULL);

	return;
}

static GF_Err vout_process(GF_Filter *filter)
{
	GF_FilterPacket *pck;
	GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) gf_filter_get_udta(filter);

	ctx->video_out->ProcessEvent(ctx->video_out, NULL);

	pck = gf_filter_pid_get_packet(ctx->pid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->pid)) {
			if (!ctx->aborted) {
				GF_FilterEvent evt;
				GF_FEVT_INIT(evt, GF_FEVT_STOP, ctx->pid);
				gf_filter_pid_send_event(ctx->pid, &evt);

				ctx->aborted = GF_TRUE;
			}
			return GF_EOS;
		}
		return GF_OK;
	}

	if (ctx->vsync) {
		u64 cts = gf_filter_pck_get_cts(pck);
		u64 clock_us, now = gf_sys_clock_high_res();
		Double media_ts;

		//check if we have a clock hint from an audio output
		gf_filter_get_clock_hint(filter, &clock_us, &media_ts);
		if (clock_us) {
			//ref frame TS in video stream timescale
			s64 ref_ts = media_ts * ctx->timescale;
			//compute time ellapsed since last clock ref in timescale
			s64 diff = now;
			diff -= (s64) clock_us;
			if (ctx->timescale!=1000000) {
				diff *= ctx->timescale;
				diff /= 1000000;
			}
			assert(diff>=0);
			//ref stream hypothetical timestamp at now
			ref_ts += diff;
			if (cts > ref_ts) {
				//the clock is not updated continuously, only when audio sound card writes. We therefore
				//cannot know if the sampling was recent or old, so ask for a short reschedule time
				gf_filter_ask_rt_reschedule(filter, 3);
				//not ready yet
				return GF_OK;
			}
		} else if (!ctx->first_cts) {
			//init clock on second frame, first frame likely will have large rendering time
			//due to GPU config. While this is not important for recorded media, it may impact
			//monitoring of live source (webcams) by consuming frame really late which may
			//block frame sampling when blocking mode is enabled
			if (!ctx->clock_at_first_cts) {
				ctx->clock_at_first_cts = 1;
			} else {
				ctx->first_cts = 1 + cts;
				ctx->clock_at_first_cts = now;
			}
		} else {
			s64 diff = (s64) ((now - ctx->clock_at_first_cts) * ctx->speed);
			if (ctx->timescale != 1000000)
				diff -= (s64) ( (cts-ctx->first_cts+1) * 1000000  / ctx->timescale);
			else
				diff -= (s64) (cts-ctx->first_cts+1);

			if (diff<0) {
					gf_filter_ask_rt_reschedule(filter, -diff);
				//not ready yet
				return GF_OK;
			}
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] At %d ms display frame "LLU" ms\n", gf_sys_clock(), (1000*cts)/ctx->timescale));
	}

	if (ctx->pfmt) {
#ifndef GPAC_DISABLE_3D
		if (ctx->mode < MODE_2D) {
			vout_draw_gl(ctx, pck);
		} else
#endif
		{
			vout_draw_2d(ctx, pck);
		}
	}
	gf_filter_pid_drop_packet(ctx->pid);
	ctx->nb_frames++;
	return GF_OK;
}

static Bool vout_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	//cancel
	return GF_TRUE;
}

#define OFFS(_n)	#_n, offsetof(GF_VideoOutCtx, _n)

static const GF_FilterArgs VideoOutArgs[] =
{
	{ OFFS(drv), "video driver name", GF_PROP_NAME, NULL, NULL, GF_FALSE},
	{ OFFS(vsync), "enables video screen sync", GF_PROP_BOOL, "true", NULL, GF_FALSE},
	{ OFFS(mode), "Display mode, gl: OpenGL, pbo: OpenGL with PBO, blit: 2D HW blit, soft: software blit", GF_PROP_UINT, "gl", "gl|pbo|blit|soft", GF_FALSE},
	{ OFFS(dur), "only plays the specified duration", GF_PROP_FRACTION, "0", NULL, GF_FALSE},
	{ OFFS(speed), "sets playback speed when vsync is on", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{ OFFS(hold), "specifies the number of seconds to hold display for single-frame streams", GF_PROP_DOUBLE, "1.0", NULL, GF_FALSE},
	{ OFFS(linear), "uses linear filtering instead of nearest pixel for GL mode", GF_PROP_BOOL, "false", NULL, GF_FALSE},
	{ OFFS(back), "specifies back color for transparent images", GF_PROP_UINT, "0x808080", NULL, GF_FALSE},
	{ OFFS(size), "Default init size", GF_PROP_VEC2I, "-1x-1", NULL, GF_FALSE},
	{ OFFS(pos), "Default position (0,0 top-left)", GF_PROP_VEC2I, "-1x-1", NULL, GF_FALSE},
	{ OFFS(fullscreen), "Use fullcreen", GF_PROP_BOOL, "false", NULL, GF_FALSE},

	{}
};

static const GF_FilterCapability VideoOutInputs[] =
{
	CAP_INC_UINT(GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_INC_UINT(GF_PROP_PID_CODECID, GF_CODECID_RAW)
};


GF_FilterRegister VideoOutRegister = {
	.name = "vout",
	.description = "Video Output",
	.private_size = sizeof(GF_VideoOutCtx),
	.args = VideoOutArgs,
	INCAPS(VideoOutInputs),
	.initialize = vout_initialize,
	.finalize = vout_finalize,
	.configure_pid = vout_configure_pid,
	.process = vout_process,
	.process_event = vout_process_event
};

const GF_FilterRegister *vout_register(GF_FilterSession *session)
{
	return &VideoOutRegister;
}

