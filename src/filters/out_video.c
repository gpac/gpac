/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2018-2020
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

#ifndef GPAC_DISABLE_PLAYER

//#define GPAC_DISABLE_3D

#if !defined(GPAC_DISABLE_3D) && !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
#define VOUT_USE_OPENGL

//include openGL
#include "../compositor/gl_inc.h"

#define DEL_SHADER(_a) if (_a) { glDeleteShader(_a); _a = 0; }
#define DEL_PROGRAM(_a) if (_a) { glDeleteProgram(_a); _a = 0; }


#define TEXTURE_TYPE GL_TEXTURE_2D


#if defined( _LP64 ) && defined(CONFIG_DARWIN_GL)
#define GF_SHADERID u64
#else
#define GF_SHADERID u32
#endif


static char *default_glsl_vertex = "\
	attribute vec4 gfVertex;\
	attribute vec2 gfTexCoord;\
	varying vec2 TexCoord;\
	void main(void)\
	{\
		gl_Position = gl_ModelViewProjectionMatrix * gfVertex;\
		TexCoord = gfTexCoord;\
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
	GF_VideoOutMode disp;
	Bool vsync, linear, fullscreen, drop, hide;
	GF_Fraction64 dur;
	Double speed, hold;
	u32 back;
	GF_PropVec2i wsize;
	GF_PropVec2i wpos;
	Double start;
	u32 buffer;
	GF_Fraction vdelay;
	const char *out;
	GF_PropUIntList dumpframes;

	GF_Filter *filter;
	GF_FilterPid *pid;
	u32 width, height, stride, pfmt, timescale;
	GF_Fraction fps;

	GF_VideoOutput *video_out;

	u32 pck_offset;
	u64 first_cts_plus_one;
	u64 clock_at_first_cts, last_frame_clock, clock_at_first_frame;
	u32 last_pck_dur_us;
	Bool aborted;
	u32 display_width, display_height;
	Bool display_changed;
	Float dh, dw, oh, ow;
	Bool has_alpha;
	u32 nb_frames;

	//if source is raw live grab (webcam/etc), we don't trust cts and always draw the frame
	//this is needed for cases where we have a sudden jump in timestamps as is the case with ffmpeg: not doing so would
	//hold the frame until its CTS is reached, triggering drops at capture time
	Bool raw_grab;

#ifdef VOUT_USE_OPENGL
	GLint glsl_program;
	GF_SHADERID vertex_shader;
	GF_SHADERID fragment_shader;

	GF_GLTextureWrapper tx;
#endif // VOUT_USE_OPENGL

	u32 num_textures;

	u32 uv_w, uv_h, uv_stride, bit_depth;
	Bool is_yuv, in_fullscreen;
	u32 nb_drawn;

	GF_Fraction sar;

	Bool force_release;
	GF_FilterPacket *last_pck;

	s32 pid_delay;
	Bool buffer_done;
	Bool no_buffering;
	Bool dump_done;

	u32 dump_f_idx;
	char *dump_buffer;
} GF_VideoOutCtx;

static GF_Err vout_draw_frame(GF_VideoOutCtx *ctx);

#ifdef VOUT_USE_OPENGL
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
#if defined(GPAC_USE_GLES2)
		glGetShaderInfoLog(shader_id, blen, &slen, compiler_log);
#elif defined(CONFIG_DARWIN_GL)
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
#endif // VOUT_USE_OPENGL


static void vout_set_caption(GF_VideoOutCtx *ctx)
{
	GF_Event evt;
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_SET_CAPTION;
	evt.caption.caption = gf_filter_pid_orig_src_args(ctx->pid);
	if (!evt.caption.caption) evt.caption.caption = gf_filter_pid_get_source_filter_name(ctx->pid);
	if (evt.caption.caption) {
		if (!strncmp(evt.caption.caption, "src=", 4)) evt.caption.caption += 4;
		if (!strncmp(evt.caption.caption, "./", 2)) evt.caption.caption += 2;
		ctx->video_out->ProcessEvent(ctx->video_out, &evt);
	}
}

static GF_Err vout_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_Event evt;
	const GF_PropertyValue *p;
	u32 w, h, pfmt, stride, stride_uv, timescale, dw, dh, hw, hh;
	GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) gf_filter_get_udta(filter);

	//if we have a pending packet, draw it now
	if (ctx->last_pck) {
		vout_draw_frame(ctx);
		gf_filter_pck_unref(ctx->last_pck);
		ctx->last_pck = NULL;
	}


	if (is_remove) {
		assert(ctx->pid==pid);
		ctx->pid=NULL;
		return GF_OK;
	}
	assert(!ctx->pid || (ctx->pid==pid));
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

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
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STRIDE_UV);
	stride_uv = p ? p->value.uint : 0;

	ctx->sar.num = ctx->sar.den = 1;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
	if (p && p->value.frac.den && p->value.frac.num) ctx->sar = p->value.frac;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	ctx->pid_delay = p ? p->value.sint : 0;

	p = gf_filter_pid_get_property_str(pid, "BufferLength");
	ctx->no_buffering = (p && !p->value.sint) ? GF_TRUE : GF_FALSE;
	if (ctx->no_buffering) ctx->buffer_done = GF_TRUE;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_RAWGRAB);
	ctx->raw_grab = (p && p->value.boolean) ? GF_TRUE : GF_FALSE;

	if (!ctx->pid) {
		GF_FilterEvent fevt;

		GF_FEVT_INIT(fevt, GF_FEVT_BUFFER_REQ, pid);
		fevt.buffer_req.max_buffer_us = ctx->buffer * 1000;
//		if (!fevt.buffer_req.max_buffer_us) fevt.buffer_req.max_buffer_us = 100000;
		gf_filter_pid_send_event(pid, &fevt);

		gf_filter_pid_init_play_event(pid, &fevt, ctx->start, ctx->speed, "VideoOut");
		if (ctx->dur.num && ctx->dur.den) {
			fevt.play.end_range = ((Double)ctx->dur.num) / ctx->dur.den;
			fevt.play.end_range += fevt.play.start_range;
		}
		gf_filter_pid_send_event(pid, &fevt);

		ctx->start = fevt.play.start_range;
		if ((ctx->speed<0) && (fevt.play.speed>=0))
			ctx->speed = fevt.play.speed;

		ctx->pid = pid;
	}
	vout_set_caption(ctx);

	if (ctx->first_cts_plus_one && ctx->timescale && (ctx->timescale != timescale) ) {
		ctx->first_cts_plus_one-=1;
		ctx->first_cts_plus_one *= timescale;
		ctx->first_cts_plus_one /= ctx->timescale;
		ctx->first_cts_plus_one+=1;
	}
	if (!timescale) timescale = 1;
	ctx->timescale = timescale;

	//pid not yet ready
	if (!pfmt || !w || !h) return GF_OK;

	if ((ctx->width==w) && (ctx->height == h) && (ctx->pfmt == pfmt) ) return GF_OK;

	dw = w;
	dh = h;
	if (ctx->sar.den != ctx->sar.num) {
		dw = dw * ctx->sar.num / ctx->sar.den;
	}
	if (ctx->wsize.x==0) ctx->wsize.x = w;
	if (ctx->wsize.y==0) ctx->wsize.y = h;
	if ((ctx->wsize.x>0) && (ctx->wsize.y>0)) {
		dw = ctx->wsize.x;
		dh = ctx->wsize.y;
	}
	//in 2D mode we need to send a setup event to resize backbuffer to the video source wsize
	if (ctx->disp>=MODE_2D) {
		dw = w;
		dh = h;
		if (ctx->sar.den != ctx->sar.num) {
			dw = dw * ctx->sar.num / ctx->sar.den;
		}
	}

	if ((dw != ctx->display_width) || (dh != ctx->display_height) ) {

		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.width = dw;
		evt.setup.height = dh;

#ifdef VOUT_USE_OPENGL
		if (ctx->disp<MODE_2D) {
			evt.setup.use_opengl = GF_TRUE;
			//always double buffer
			evt.setup.back_buffer = gf_opts_get_bool("core", "gl-doublebuf");
		} else
#endif
		{
			evt.setup.back_buffer = 1;
		}
		evt.setup.disable_vsync = !ctx->vsync;
		ctx->video_out->ProcessEvent(ctx->video_out, &evt);
		if (evt.setup.use_opengl) {
			gf_opengl_init();
		}
		if (!ctx->in_fullscreen) {
			ctx->display_width = evt.setup.width;
			ctx->display_height = evt.setup.height;
		}
		ctx->display_changed = GF_TRUE;

#if defined(VOUT_USE_OPENGL) && defined(WIN32)
		if (evt.setup.use_opengl)
			gf_opengl_init();

		if ((ctx->disp<MODE_2D) && (glCompileShader == NULL)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[VideoOut] Failed to load openGL, fallback to 2D blit\n"));
			evt.setup.use_opengl = GF_FALSE;
			evt.setup.back_buffer = 1;
			ctx->disp = MODE_2D;
			ctx->video_out->ProcessEvent(ctx->video_out, &evt);
		}
#endif

		if (!ctx->in_fullscreen) {
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_SIZE;
			evt.size.width = dw;
			evt.size.height = dh;
			ctx->video_out->ProcessEvent(ctx->video_out, &evt);
		}
	} else if ((ctx->wsize.x>0) && (ctx->wsize.y>0)) {
		ctx->display_width = ctx->wsize.x;
		ctx->display_height = ctx->wsize.y;
		ctx->display_changed = GF_TRUE;
	}
	if (ctx->fullscreen) {
		u32 nw=ctx->display_width, nh=ctx->display_height;
		ctx->video_out->SetFullScreen(ctx->video_out, GF_TRUE, &nw, &nh);
		ctx->in_fullscreen = GF_TRUE;
	} else if ((ctx->wpos.x!=-1) && (ctx->wpos.y!=-1)) {
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_MOVE;
		evt.move.relative = 0;
		evt.move.x = ctx->wpos.x;
		evt.move.y = ctx->wpos.y;
		ctx->video_out->ProcessEvent(ctx->video_out, &evt);
	}

	ctx->timescale = timescale;
	ctx->width = w;
	ctx->height = h;
	ctx->pfmt = pfmt;
	ctx->uv_w = ctx->uv_h = 0;
	ctx->uv_stride = stride_uv;
	ctx->bit_depth = 0;
	ctx->has_alpha = GF_FALSE;

	if (!stride) {
		gf_pixel_get_size_info(ctx->pfmt, w, h, NULL, &stride, &ctx->uv_stride, NULL, &ctx->uv_h);
	}
	ctx->stride = stride ? stride : w;
	if (ctx->dump_buffer) {
		gf_free(ctx->dump_buffer);
		ctx->dump_buffer = NULL;
	}

	hw = ctx->width/2;
	if (ctx->width %2) hw++;
	hh = ctx->height/2;
	if (ctx->height %2) hh++;

	ctx->is_yuv = GF_FALSE;
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
		ctx->uv_w = ctx->width/2;
		ctx->uv_h = ctx->height;
		ctx->uv_stride = ctx->stride/2;
		ctx->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_YUV_10:
		ctx->bit_depth = 10;
	case GF_PIXEL_YUV:
		ctx->uv_w = ctx->width/2;
		if (ctx->width % 2) ctx->uv_w++;
		ctx->uv_h = ctx->height/2;
		if (ctx->height % 2) ctx->uv_h++;
		if (!ctx->uv_stride) {
			ctx->uv_stride = ctx->stride/2;
			if (ctx->stride%2) ctx->uv_stride ++;
		}
		ctx->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_NV12_10:
	case GF_PIXEL_NV21_10:
		ctx->bit_depth = 10;
	case GF_PIXEL_NV12:
	case GF_PIXEL_NV21:
		ctx->uv_w = ctx->width/2;
		ctx->uv_h = ctx->height/2;
		ctx->uv_stride = ctx->stride;
		ctx->is_yuv = GF_TRUE;
		break;
	case GF_PIXEL_UYVY:
	case GF_PIXEL_YUYV:
	case GF_PIXEL_YVYU:
	case GF_PIXEL_VYUY:
		ctx->uv_w = ctx->width/2;
		ctx->uv_h = ctx->height;
		if (!ctx->uv_stride) {
			ctx->uv_stride = ctx->stride/2;
			if (ctx->stride%2) ctx->uv_stride ++;
		}
		ctx->is_yuv = GF_TRUE;
		break;

	case GF_PIXEL_ALPHAGREY:
	case GF_PIXEL_GREYALPHA:
	case GF_PIXEL_BGRA:
	case GF_PIXEL_ARGB:
	case GF_PIXEL_ABGR:
	case GF_PIXEL_RGBA:
		ctx->has_alpha = GF_TRUE;
		break;
	case GF_PIXEL_GREYSCALE:
	case GF_PIXEL_RGB:
	case GF_PIXEL_BGR:
	case GF_PIXEL_BGRX:
	case GF_PIXEL_XRGB:
	case GF_PIXEL_XBGR:
	case GF_PIXEL_RGBX:
	case GF_PIXEL_RGB_444:
	case GF_PIXEL_RGB_555:
	case GF_PIXEL_RGB_565:
		break;

	case 0:
		//not yet set, happens with some decoders/stream settings - wait until first frame is available and PF is known
		return GF_OK;
	default:
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[VideoOut] Pixel format %s unknown\n", gf_4cc_to_str(ctx->pfmt)));
		return GF_OK;
	}

#ifdef VOUT_USE_OPENGL
	if (ctx->disp<MODE_2D) {
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_SET_GL;
		ctx->video_out->ProcessEvent(ctx->video_out, &evt);
	}
	
	DEL_SHADER(ctx->vertex_shader);
	DEL_SHADER(ctx->fragment_shader);
	DEL_PROGRAM(ctx->glsl_program );
	gf_gl_txw_reset(&ctx->tx);

	if (ctx->disp<MODE_2D) {
		char *frag_shader_src = NULL;
		GF_Matrix mx;
		Float ft_hw, ft_hh;

		ctx->glsl_program = glCreateProgram();
		ctx->vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		vout_compile_shader(ctx->vertex_shader, "vertex", default_glsl_vertex);

		ctx->fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		gf_gl_txw_setup(&ctx->tx, ctx->pfmt, ctx->width, ctx->height, ctx->stride, ctx->uv_stride, ctx->linear, NULL);

		gf_dynstrcat(&frag_shader_src, "#version 120\n", NULL);

		gf_gl_txw_insert_fragment_shader(ctx->tx.pix_fmt, "maintx", &frag_shader_src);
		gf_dynstrcat(&frag_shader_src, "varying vec2 TexCoord;\n"
										"void main(void) {\n"
										"gl_FragColor = maintx_sample(TexCoord.st);\n"
										"}"
								, NULL);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] Fragment shader is %s\n", frag_shader_src));

		vout_compile_shader(ctx->fragment_shader, "fragment", frag_shader_src);
		gf_free(frag_shader_src);

		glAttachShader(ctx->glsl_program, ctx->vertex_shader);
		glAttachShader(ctx->glsl_program, ctx->fragment_shader);
		glLinkProgram(ctx->glsl_program);

#ifdef WIN32
		if (glMapBuffer == NULL) {
			if (ctx->disp == MODE_GL_PBO) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("[VideoOut] GL PixelBufferObject extensions not found, fallback to regular GL\n"));
				ctx->disp = MODE_GL;
			}
		}
#endif

		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		glViewport(0, 0, ctx->width, ctx->height);

		gf_mx_init(mx);
		ft_hw = ((Float)ctx->width)/2;
		ft_hh = ((Float)ctx->height)/2;
		gf_mx_ortho(&mx, -ft_hw, ft_hw, -ft_hh, ft_hh, 50, -50);
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
	} else
#endif //VOUT_USE_OPENGL
	{
		switch (ctx->pfmt) {
		case GF_PIXEL_NV12:
		case GF_PIXEL_NV21:
		case GF_PIXEL_NV12_10:
		case GF_PIXEL_NV21_10:
			ctx->num_textures = 2;
			break;
		case GF_PIXEL_UYVY:
		case GF_PIXEL_VYUY:
		case GF_PIXEL_YVYU:
		case GF_PIXEL_YUYV:
			ctx->num_textures = 1;
			break;
		default:
			if (ctx->is_yuv) {
				ctx->num_textures = 3;
			} else {
				ctx->num_textures = 1;
			}
			break;
		}
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[VideoOut] Reconfig input wsize %d x %d, %d textures\n", ctx->width, ctx->height, ctx->num_textures));
	return GF_OK;
}

static Bool vout_on_event(void *cbk, GF_Event *evt)
{
	GF_FilterEvent fevt;
	GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) cbk;

	fevt.base.type = 0;
	switch (evt->type) {
	case GF_EVENT_SIZE:
		if ((ctx->display_width != evt->size.width) || (ctx->display_height != evt->size.height)) {
			ctx->display_width = evt->size.width;
			ctx->display_height = evt->size.height;
			ctx->display_changed = GF_TRUE;
		}
		break;
	case GF_EVENT_CLICK:
	case GF_EVENT_MOUSEUP:
	case GF_EVENT_MOUSEDOWN:
	case GF_EVENT_MOUSEOVER:
	case GF_EVENT_MOUSEOUT:
	case GF_EVENT_MOUSEMOVE:
	case GF_EVENT_MOUSEWHEEL:
	case GF_EVENT_KEYUP:
	case GF_EVENT_KEYDOWN:
	case GF_EVENT_LONGKEYPRESS:
	case GF_EVENT_TEXTINPUT:

		GF_FEVT_INIT(fevt, GF_FEVT_USER, ctx->pid);
		fevt.user_event.event = *evt;
		break;
	}
	if (gf_filter_ui_event(ctx->filter, evt))
		return GF_TRUE;

	if (fevt.base.type)
	 	gf_filter_pid_send_event(ctx->pid, &fevt);

	 return GF_TRUE;
}

GF_VideoOutput *gf_filter_claim_opengl_provider(GF_Filter *filter);
Bool gf_filter_unclaim_opengl_provider(GF_Filter *filter, GF_VideoOutput *vout);

static GF_Err vout_initialize(GF_Filter *filter)
{
	const char *sOpt;
	void *os_wnd_handler, *os_disp_handler;
	u32 init_flags;
	GF_Err e;
	GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) gf_filter_get_udta(filter);

	ctx->filter = filter;

#ifdef VOUT_USE_OPENGL
	if (ctx->disp <= MODE_GL_PBO) {
		ctx->video_out = (GF_VideoOutput *) gf_filter_claim_opengl_provider(filter);
	}
#endif

	if (!ctx->video_out)
		ctx->video_out = (GF_VideoOutput *) gf_module_load(GF_VIDEO_OUTPUT_INTERFACE, ctx->drv);


	if (ctx->dumpframes.nb_items) {
		ctx->hide = GF_TRUE;
		ctx->vsync = GF_FALSE;
	}

	/*if not init we run with a NULL audio compositor*/
	if (!ctx->video_out) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] No output modules found, cannot load video output\n"));
		return GF_IO_ERR;
	}
	if (!gf_opts_get_key("core", "video-output")) {
		gf_opts_set_key("core", "video-output", ctx->video_out->module_name);
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] Setting up video module %s\n", ctx->video_out->module_name));
	ctx->video_out->on_event = vout_on_event;
	ctx->video_out->evt_cbk_hdl = ctx;

	os_wnd_handler = os_disp_handler = NULL;
	init_flags = 0;
	sOpt = gf_opts_get_key("Temp", "OSWnd");
	if (sOpt) sscanf(sOpt, "%p", &os_wnd_handler);
	sOpt = gf_opts_get_key("Temp", "OSDisp");
	if (sOpt) sscanf(sOpt, "%p", &os_disp_handler);
	sOpt = gf_opts_get_key("Temp", "InitFlags");
	if (sOpt) sscanf(sOpt, "%d", &init_flags);

	if (ctx->hide)
		init_flags |= GF_TERM_INIT_HIDE;

	e = ctx->video_out->Setup(ctx->video_out, os_wnd_handler, os_disp_handler, init_flags);
	if (e!=GF_OK) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("Failed to Setup Video Driver %s!\n", ctx->video_out->module_name));
		gf_modules_close_interface((GF_BaseInterface *)ctx->video_out);
		ctx->video_out = NULL;
		return e;
	}

#ifdef VOUT_USE_OPENGL
	if ( !(ctx->video_out->hw_caps & GF_VIDEO_HW_OPENGL) )
#endif
	{
		if (ctx->disp < MODE_2D) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MMIO, ("No openGL support - using 2D rasterizer!\n", ctx->video_out->module_name));
			ctx->disp = MODE_2D;
		}
	}
#ifdef VOUT_USE_OPENGL
	if (ctx->disp <= MODE_GL_PBO) {
		GF_Event evt;
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_VIDEO_SETUP;
		evt.setup.width = 320;
		evt.setup.height = 240;
		evt.setup.use_opengl = GF_TRUE;
		evt.setup.back_buffer = 1;
		evt.setup.disable_vsync = !ctx->vsync;
		ctx->video_out->ProcessEvent(ctx->video_out, &evt);

		if (evt.setup.use_opengl) {
			gf_opengl_init();
		}
		gf_filter_register_opengl_provider(filter, GF_TRUE);
	}
#endif

	gf_filter_set_event_target(filter, GF_TRUE);
	return GF_OK;
}

static void vout_finalize(GF_Filter *filter)
{
	GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) gf_filter_get_udta(filter);

	if (ctx->last_pck) {
		gf_filter_pck_unref(ctx->last_pck);
		ctx->last_pck = NULL;
	}

	if ((ctx->nb_frames==1) || (ctx->hold<0)) {
		u32 holdms = (u32) (((ctx->hold>0) ? ctx->hold : -ctx->hold) * 1000);
		gf_sleep(holdms);
	}

#ifdef VOUT_USE_OPENGL
	DEL_SHADER(ctx->vertex_shader);
	DEL_SHADER(ctx->fragment_shader);
	DEL_PROGRAM(ctx->glsl_program );
	gf_gl_txw_reset(&ctx->tx);
#endif //VOUT_USE_OPENGL

	/*stop and shutdown*/
	if (ctx->video_out) {
		if (! gf_filter_unclaim_opengl_provider(filter, ctx->video_out)) {
			ctx->video_out->Shutdown(ctx->video_out);
			gf_modules_close_interface((GF_BaseInterface *)ctx->video_out);
		}
		ctx->video_out = NULL;
	}
	if (ctx->dump_buffer) gf_free(ctx->dump_buffer);

}

#ifdef VOUT_USE_OPENGL

static void vout_draw_gl_quad(GF_VideoOutCtx *ctx, Bool flip_texture)
{
	Float dw, dh;

	gf_gl_txw_bind(&ctx->tx, "maintx", ctx->glsl_program, 0);

	dw = ((Float)ctx->dw) / 2;
	dh = ((Float)ctx->dh) / 2;


	GLfloat squareVertices[] = {
		dw, dh,
		dw, -dh,
		-dw,  -dh,
		-dw,  dh,
	};

	GLfloat textureVertices[] = {
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f,  1.0f,
		0.0f,  0.0f,
	};

	if (flip_texture) {
		textureVertices[1] = textureVertices[7] = 1.0f;
		textureVertices[3] = textureVertices[5] = 0.0f;
	}

	int loc = glGetAttribLocation(ctx->glsl_program, "gfVertex");
	if (loc >= 0) {
		glVertexAttribPointer(loc, 2, GL_FLOAT, 0, 0, squareVertices);
		glEnableVertexAttribArray(loc);

		//setup texcoord location
		loc = glGetAttribLocation(ctx->glsl_program, "gfTexCoord");
		if (loc >= 0) {
			glVertexAttribPointer(loc, 2, GL_FLOAT, 0, 0, textureVertices);
			glEnableVertexAttribArray(loc);

			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Failed to gfTexCoord uniform in shader\n"));
		}
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Failed to gfVertex uniform in shader\n"));
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(TEXTURE_TYPE, 0);
	glDisable(TEXTURE_TYPE);

	glUseProgram(0);

	if (ctx->dump_f_idx) {
		FILE *fout;
		char szFileName[1024];
		if (strchr(gf_file_basename(ctx->out), '.')) {
			snprintf(szFileName, 1024, "%s", ctx->out);
		} else {
			snprintf(szFileName, 1024, "%s_%d.rgb", ctx->out, ctx->dump_f_idx);
		}
		if (!ctx->dump_buffer)
			ctx->dump_buffer = gf_malloc(sizeof(char)*ctx->display_width*ctx->display_height*3);

		glReadPixels(0, 0, ctx->display_width, ctx->display_height, GL_RGB, GL_UNSIGNED_BYTE, ctx->dump_buffer);
		GL_CHECK_ERR()
		fout = gf_fopen(szFileName, "wb");
		if (fout) {
			gf_fwrite(ctx->dump_buffer, ctx->display_width*ctx->display_height*3, fout);
			gf_fclose(fout);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error writing frame %d buffer to %s\n", ctx->nb_frames, szFileName));
		}
	}

	ctx->video_out->Flush(ctx->video_out, NULL);
}

static void vout_draw_gl_hw_textures(GF_VideoOutCtx *ctx, GF_FilterFrameInterface *hwf)
{
	if (ctx->tx.internal_textures) {
		glDeleteTextures(ctx->tx.nb_textures, ctx->tx.textures);
		ctx->tx.internal_textures = GF_FALSE;
	}
	ctx->tx.frame_ifce = hwf;
	gf_gl_txw_bind(&ctx->tx, "maintx", ctx->glsl_program, 0);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//and draw
	vout_draw_gl_quad(ctx, ctx->tx.flip ? GF_TRUE : GF_FALSE);
	GL_CHECK_ERR()
}

static void vout_draw_gl(GF_VideoOutCtx *ctx, GF_FilterPacket *pck)
{
	char *data;
	GF_Event evt;
	GF_Matrix mx;
	Float hw, hh;
	u32 wsize;
	GF_FilterFrameInterface *frame_ifce;

	if (!ctx->glsl_program) return;

	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_SET_GL;
	ctx->video_out->ProcessEvent(ctx->video_out, &evt);

	if (ctx->display_changed) {
		//if we fill width to display width and height is outside
		if (ctx->display_width * ctx->height / ctx->width > ctx->display_height) {
			ctx->dw = (Float) (ctx->display_height * ctx->width / ctx->height);
			ctx->dw *= ctx->sar.num;
			ctx->dw /= ctx->sar.den;
			ctx->dh = (Float) ctx->display_height;
			ctx->oh = (Float) 0;
			ctx->ow = (Float) (ctx->display_width - ctx->dw ) / 2;
		} else {
			ctx->dh = (Float) (ctx->display_width * ctx->height / ctx->width);
			ctx->dh *= ctx->sar.den;
			ctx->dh /= ctx->sar.num;
			ctx->dw = (Float) ctx->display_width;
			ctx->ow = (Float) 0;
			ctx->oh = (Float) (ctx->display_height - ctx->dh ) / 2;
		}
		if (ctx->dump_buffer) {
			gf_free(ctx->dump_buffer);
			ctx->dump_buffer = NULL;
		}

		ctx->display_changed = GF_FALSE;
	}


	frame_ifce = gf_filter_pck_get_frame_interface(pck);
	if (frame_ifce && (frame_ifce->flags & GF_FRAME_IFCE_MAIN_GLFB)) {
		ctx->video_out->Flush(ctx->video_out, NULL);
		return;
	}

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

	if (ctx->has_alpha) {
		Float r, g, b;
		r = GF_COL_R(ctx->back); r /= 255;
		g = GF_COL_G(ctx->back); g /= 255;
		b = GF_COL_B(ctx->back); b /= 255;
		glClearColor(r, g, b, 1.0);
	} else {
		glClearColor(0, 0, 0, 1.0);
	}
	glClear(GL_COLOR_BUFFER_BIT);

	//we are not sure if we are the only ones using the gl context, reset our defaults
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	if (ctx->has_alpha) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	} else {
		glDisable(GL_BLEND);
	}
	if (ctx->has_alpha) {
		Float r, g, b;
		r = GF_COL_R(ctx->back); r /= 255;
		g = GF_COL_G(ctx->back); g /= 255;
		b = GF_COL_B(ctx->back); b /= 255;
		glClearColor(r, g, b, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	glUseProgram(ctx->glsl_program);

	if (frame_ifce && frame_ifce->get_gl_texture) {
		vout_draw_gl_hw_textures(ctx, frame_ifce);
		return;
	}

	data = (char*) gf_filter_pck_get_data(pck, &wsize);

	//upload texture
	gf_gl_txw_upload(&ctx->tx, data, frame_ifce);

	//and draw
	vout_draw_gl_quad(ctx, GF_FALSE);
}
#endif

void vout_draw_2d(GF_VideoOutCtx *ctx, GF_FilterPacket *pck)
{
	char *data;
	u32 wsize;
	GF_Err e;
	GF_VideoSurface src_surf;
	GF_Window dst_wnd, src_wnd;

	memset(&src_surf, 0, sizeof(GF_VideoSurface));
	src_surf.width = ctx->width;
	src_surf.height = ctx->height;
	src_surf.pitch_x = 0;
	src_surf.pitch_y = ctx->stride;
	src_surf.pixel_format = ctx->pfmt;
	src_surf.global_alpha = 0xFF;

	data = (char *) gf_filter_pck_get_data(pck, &wsize);
	if (!data) {
		u32 stride_luma;
		u32 stride_chroma;
		GF_FilterFrameInterface *frame_ifce = gf_filter_pck_get_frame_interface(pck);
		if (frame_ifce->flags & GF_FRAME_IFCE_BLOCKING)
			ctx->force_release = GF_TRUE;
		if (! frame_ifce->get_plane) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Hardware GL texture blit not supported with non-GL blitter\n"));
			return;
		}
		e = frame_ifce->get_plane(frame_ifce, 0, (const u8 **) &src_surf.video_buffer, &stride_luma);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error fetching chroma plane from hardware frame\n"));
			return;
		}
		if (ctx->is_yuv && (ctx->num_textures>1)) {
			e = frame_ifce->get_plane(frame_ifce, 1, (const u8 **) &src_surf.u_ptr, &stride_chroma);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error fetching luma U plane from hardware frame\n"));
				return;
			}
			if ((ctx->pfmt!= GF_PIXEL_NV12) && (ctx->pfmt!= GF_PIXEL_NV21)) {
				e = frame_ifce->get_plane(frame_ifce, 2, (const u8 **) &src_surf.v_ptr, &stride_chroma);
				if (e) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error fetching luma V plane from hardware frame\n"));
					return;
				}
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
			ctx->dw = (Float) (ctx->display_height * ctx->width * ctx->sar.num / ctx->height / ctx->sar.den);
			ctx->dh = (Float) ctx->display_height;
			ctx->oh = (Float) 0;
			ctx->ow = (Float) (ctx->display_width - ctx->dw ) / 2;
		} else {
			ctx->dh = (Float) (ctx->display_width * ctx->height *ctx->sar.den / ctx->width / ctx->sar.num);
			ctx->dw = (Float) ctx->display_width;
			ctx->ow = (Float) 0;
			ctx->oh = (Float) (ctx->display_height - ctx->dh ) / 2;
		}
		ctx->display_changed = GF_FALSE;
	}

	dst_wnd.x = (u32) ctx->ow;
	dst_wnd.y = (u32) ctx->oh;
	dst_wnd.w = (u32) ctx->dw;
	dst_wnd.h = (u32) ctx->dh;
	e = GF_EOS;

	src_wnd.x = src_wnd.y = 0;
	src_wnd.w = ctx->width;
	src_wnd.h = ctx->height;

	if ((ctx->disp!=MODE_2D_SOFT) && ctx->video_out->Blit) {
		e = ctx->video_out->Blit(ctx->video_out, &src_surf, &src_wnd, &dst_wnd, 0);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error bliting surface %s - retrying in software mode\n", gf_error_to_string(e) ));
		}
	}
	if (e) {
		GF_VideoSurface backbuffer;

		e = ctx->video_out->LockBackBuffer(ctx->video_out, &backbuffer, GF_TRUE);
		if (!e) {
			gf_stretch_bits(&backbuffer, &src_surf, &dst_wnd, &src_wnd, 0xFF, GF_FALSE, NULL, NULL);
			ctx->video_out->LockBackBuffer(ctx->video_out, &backbuffer, GF_FALSE);
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Cannot lock back buffer - Error %s\n", gf_error_to_string(e) ));
		}
	}

	if (e != GF_OK)
		return;


	if (ctx->dump_f_idx) {
		char szFileName[1024];
		GF_VideoSurface backbuffer;

		e = ctx->video_out->LockBackBuffer(ctx->video_out, &backbuffer, GF_TRUE);
		if (!e) {
			FILE *fout;
			const char *src_ext = strchr(gf_file_basename(ctx->out), '.');
			const char *ext = gf_pixel_fmt_sname(backbuffer.pixel_format);
			if (!ext) ext = "rgb";

			if (src_ext && !strcmp(ext, src_ext+1)) {
				snprintf(szFileName, 1024, "%s", ctx->out);
			} else {
				snprintf(szFileName, 1024, "%s_%d.%s", ctx->out, ctx->dump_f_idx, ext);
			}
			fout = gf_fopen(szFileName, "wb");
			if (fout) {
				u32 i;
				for (i=0; i<backbuffer.height; i++) {
					gf_fwrite(backbuffer.video_buffer + i*backbuffer.pitch_y, backbuffer.pitch_y, fout);
				}
				gf_fclose(fout);
			} else {
				e = GF_IO_ERR;
			}
			ctx->video_out->LockBackBuffer(ctx->video_out, &backbuffer, GF_FALSE);
		}

		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error writing frame %d buffer to %s: %s\n", ctx->nb_frames, szFileName, gf_error_to_string(e) ));
		}
	}

	dst_wnd.x = 0;
	dst_wnd.y = 0;
	dst_wnd.w = ctx->display_width;
	dst_wnd.h = ctx->display_height;
	e = ctx->video_out->Flush(ctx->video_out, &dst_wnd);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] Error flushing vido output %s%s\n", gf_error_to_string(e)));
		return;
	}
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
		}
		if (ctx->aborted) {
			if (ctx->last_pck) {
				gf_filter_pck_unref(ctx->last_pck);
				ctx->last_pck = NULL;
			}
			if ((ctx->nb_frames>1) && ctx->last_pck_dur_us) {
				gf_filter_ask_rt_reschedule(filter, ctx->last_pck_dur_us);
				ctx->last_pck_dur_us = 0;
				return GF_OK;
			}
			//fallthrough
		}
		//check if all sinks are done - if not keep requesting a process to pump window event loop
		if (!gf_filter_all_sinks_done(filter)) {
			gf_filter_ask_rt_reschedule(filter, 100000);
			if (ctx->display_changed) goto draw_frame;
			return GF_OK;
		}
		return ctx->aborted ? GF_EOS : GF_OK;
	}
	
	if (!ctx->width || !ctx->height) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MMIO, ("[VideoOut] pid display wsize unknown, discarding packet\n"));
		gf_filter_pid_drop_packet(ctx->pid);
		return GF_OK;
	}

	if (ctx->dumpframes.nb_items) {
		u32 i;

		ctx->nb_frames++;
		ctx->dump_f_idx = 0;

		for (i=0; i<ctx->dumpframes.nb_items; i++) {
			if (ctx->dumpframes.vals[i] == 0) {
				ctx->dump_f_idx = ctx->nb_frames;
				break;
			}
			if (ctx->dumpframes.vals[i] == ctx->nb_frames) {
				ctx->dump_f_idx = ctx->dumpframes.vals[i];
				break;
			}
			if (ctx->dumpframes.vals[i] > ctx->nb_frames) {
				break;
			}
		}

		if (ctx->dump_f_idx) {
			ctx->last_pck = pck;
			vout_draw_frame(ctx);
			ctx->last_pck = NULL;
		}
		gf_filter_pid_drop_packet(ctx->pid);

		if (ctx->dumpframes.vals[ctx->dumpframes.nb_items-1] <= ctx->nb_frames) {
			gf_filter_pid_set_discard(ctx->pid, GF_TRUE);
		}
		return GF_OK;
	}

	if (ctx->buffer) {
		if (gf_filter_pck_is_blocking_ref(pck)) {
			ctx->buffer_done = GF_TRUE;
			ctx->buffer = 0;
		} else {
			//query full buffer duration in us
			u64 dur = gf_filter_pid_query_buffer_duration(ctx->pid, GF_FALSE);

			GF_LOG(GF_LOG_INFO, GF_LOG_MMIO, ("[VideoOut] buffer %d / %d ms\r", (u32) (dur/1000), ctx->buffer));
			if (!ctx->buffer_done) {
				if ((dur < ctx->buffer * 1000) && !gf_filter_pid_has_seen_eos(ctx->pid)) {
					gf_filter_ask_rt_reschedule(filter, 100000);

					if (gf_filter_reporting_enabled(filter)) {
						char szStatus[1024];
						sprintf(szStatus, "buffering %d / %d ms", (u32) (dur/1000), ctx->buffer);
						gf_filter_update_status(filter, -1, szStatus);
					}
					return GF_OK;
				}
				ctx->buffer_done = GF_TRUE;
			}
		}
	}

	if (ctx->vsync || ctx->drop) {
		u64 ref_clock = 0;
		u64 cts = gf_filter_pck_get_cts(pck);
		u64 clock_us, now = gf_sys_clock_high_res();
		Bool check_clock = GF_FALSE;
		GF_Fraction64 media_ts;
		s64 delay;

		if (ctx->dur.num) {
			if ((cts - ctx->first_cts_plus_one + 1) * ctx->dur.den > (u64) (ctx->dur.num * ctx->timescale)) {
				GF_FilterEvent evt;
				if (ctx->last_pck) {
					gf_filter_pck_unref(ctx->last_pck);
					ctx->last_pck = NULL;
				}
				ctx->aborted = GF_TRUE;
				GF_FEVT_INIT(evt, GF_FEVT_STOP, ctx->pid);
				gf_filter_pid_send_event(ctx->pid, &evt);
				gf_filter_pid_set_discard(ctx->pid, GF_TRUE);
				return GF_EOS;
			}
		}

		delay = ctx->pid_delay;
		if (ctx->vdelay.den)
			delay += ctx->vdelay.num * (s32)ctx->timescale / (s32)ctx->vdelay.den;

		if (delay>=0) {
			cts += delay;
		} else if (cts < (u64) (-delay) ) {
			if (ctx->vsync) {
				if (ctx->last_pck) {
					gf_filter_pck_unref(ctx->last_pck);
					ctx->last_pck = NULL;
				}
				gf_filter_pid_drop_packet(ctx->pid);
				return GF_OK;
			}
			cts = 0;
		} else {
			cts -= (u64) -delay;
		}

		//check if we have a clock hint from an audio output
		gf_filter_get_clock_hint(filter, &clock_us, &media_ts);
		if (clock_us && media_ts.den) {
			u32 safety;
			//ref frame TS in video stream timescale
			s64 ref_ts = (s64) (media_ts.num * ctx->timescale);
			ref_ts /= media_ts.den;
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
			ctx->first_cts_plus_one = cts + 1;

			//allow 10ms video advance
			#define DEF_VIDEO_AUDIO_ADVANCE_MS	15
			safety = DEF_VIDEO_AUDIO_ADVANCE_MS * ctx->timescale / 1000;
			if (!ctx->raw_grab && ((s64) cts > ref_ts + safety)) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] At %d ms display frame CTS "LLU" CTS greater than reference clock CTS "LLU" (%g sec), waiting\n", gf_sys_clock(), cts, ref_ts, ((Double)media_ts.num)/media_ts.den));
				//the clock is not updated continuously, only when audio sound card writes. We therefore
				//cannot know if the sampling was recent or old, so ask for a short reschedule time
				gf_filter_ask_rt_reschedule(filter, (u32) ((cts-ref_ts - safety) * 1000000 / ctx->timescale) );
				//not ready yet
				return GF_OK;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] At %d ms frame CTS "LLU" latest reference clock CTS "LLU" (%g sec)\n", gf_sys_clock(), cts, ref_ts, ((Double)media_ts.num)/media_ts.den));
			ref_clock = ref_ts;
			check_clock = GF_TRUE;
		} else if (!ctx->first_cts_plus_one) {
			//init clock on second frame, first frame likely will have large rendering time
			//due to GPU config. While this is not important for recorded media, it may impact
			//monitoring of live source (webcams) by consuming frame really late which may
			//block source sampling when blocking mode is enabled

			//store first frame TS
			if (!ctx->clock_at_first_cts) {
				ctx->clock_at_first_cts = 1 + cts;
			} else {
				//comute CTS diff in ms
				u64 diff = cts - ctx->clock_at_first_cts + 1;
				diff *= 1000;
				diff /= ctx->timescale;
				//diff less than 100ms (eg 10fps or more), init on the second frame
				if (diff<100) {
					ctx->first_cts_plus_one = 1 + cts;
					ctx->clock_at_first_cts = now;
				}
				//otherwise (low frame rate), init on the first frame
				else {
					ctx->first_cts_plus_one = ctx->clock_at_first_cts /* - 1 + 1*/;
					ctx->clock_at_first_cts = ctx->last_frame_clock;
					check_clock = GF_TRUE;
				}
			}
			ref_clock = cts;
		} else {
			check_clock = GF_TRUE;
		}

		if (check_clock) {
			s64 diff;
			if (ctx->speed>=0) {
				if (cts < ctx->first_cts_plus_one) cts = ctx->first_cts_plus_one;
				diff = (s64) ((now - ctx->clock_at_first_cts) * ctx->speed);

				if (ctx->timescale != 1000000)
					diff -= (s64) ( (cts - ctx->first_cts_plus_one + 1) * 1000000  / ctx->timescale);
				else
					diff -= (s64) (cts - ctx->first_cts_plus_one + 1);

			} else {
				diff = (s64) ((now - ctx->clock_at_first_cts) * -ctx->speed);

				if (ctx->timescale != 1000000)
					diff -= (s64) ( (ctx->first_cts_plus_one-1 - cts) * 1000000  / ctx->timescale);
				else
					diff -= (s64) (ctx->first_cts_plus_one-1 - cts);
			}

			if (!ctx->raw_grab && (diff < -2000)) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] At %d ms frame cts "LLU"/%d "LLU" us too early, waiting\n", gf_sys_clock(), cts, ctx->timescale, -diff));
				if (diff<-1000000) diff = -1000000;
				gf_filter_ask_rt_reschedule(filter, (u32) (-diff));

				if (ctx->display_changed) goto draw_frame;
				//not ready yet
				return GF_OK;
			}
			if (ABS(diff)>2000) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] At %d ms frame cts "LLU"/%d is "LLD" us too %s\n", gf_sys_clock(), cts, ctx->timescale, diff<0 ? -diff : diff, diff<0 ? "early" : "late"));
			} else {
				diff = 0;
			}

			if (ctx->timescale != 1000000)
				ref_clock = diff * ctx->timescale / 1000000 + cts;
			else
				ref_clock = diff + cts;

			ctx->last_pck_dur_us = gf_filter_pck_get_duration(pck);
			ctx->last_pck_dur_us *= 1000000;
			ctx->last_pck_dur_us /= ctx->timescale;
		}
		//detach packet from pid, so that we can query next cts
		gf_filter_pck_ref(&pck);
		gf_filter_pid_drop_packet(ctx->pid);

		if (ctx->drop) {
			u64 next_ts;
			Bool do_drop = GF_FALSE;
			//peeking next packet CTS might fail if no packet in buffer, in which case we don't drop
			if (gf_filter_pid_get_first_packet_cts(ctx->pid, &next_ts)) {
				if (next_ts<ref_clock) {
					do_drop = GF_TRUE;
				}
			} else if (ctx->speed > 2){
				u32 speed = (u32) ABS(ctx->speed);

				ctx->nb_drawn++;
				if (ctx->nb_drawn % speed) {
					do_drop = GF_TRUE;
				} else {
					ctx->nb_drawn = 0;
				}
			}
			if (do_drop) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] At %d ms drop frame "LLU" ms reference clock "LLU" ms\n", gf_sys_clock(), (1000*cts)/ctx->timescale, (1000*ref_clock)/ctx->timescale));

				gf_filter_pck_unref(pck);
				return GF_OK;
			}
		}

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] At %d ms display frame cts "LLU"/%d  "LLU" ms\n", gf_sys_clock(), cts, ctx->timescale, (1000*cts)/ctx->timescale));
	} else {
		gf_filter_pck_ref(&pck);
		gf_filter_pid_drop_packet(ctx->pid);

#ifndef GPAC_DISABLE_LOG
#ifdef VOUT_USE_OPENGL
		u64 cts = gf_filter_pck_get_cts(pck);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MMIO, ("[VideoOut] At %d ms display frame cts "LLU"/%d  "LLU" ms\n", gf_sys_clock(), cts, ctx->timescale, (1000*cts)/ctx->timescale));
#endif
#endif

	}

	if (ctx->last_pck)
		gf_filter_pck_unref(ctx->last_pck);
	ctx->last_pck = pck;
	ctx->nb_frames++;


draw_frame:

	if (ctx->last_pck && gf_filter_reporting_enabled(filter)) {
		char szStatus[1024];
		u64 dur = gf_filter_pid_query_buffer_duration(ctx->pid, GF_FALSE);
		Double fps = 0;
		if (ctx->clock_at_first_frame) {
			fps = ctx->nb_frames;
			fps *= 1000000;
			fps /= (1 + gf_sys_clock_high_res() - ctx->clock_at_first_cts);
		} else {
			ctx->clock_at_first_frame = gf_sys_clock_high_res();
		}
		sprintf(szStatus, "%dx%d->%dx%d %s frame TS "LLU"/%d buffer %d / %d ms %02.02f FPS", ctx->width, ctx->height, ctx->display_width, ctx->display_height,
		 	gf_pixel_fmt_name(ctx->pfmt), gf_filter_pck_get_cts(ctx->last_pck), ctx->timescale, (u32) (dur/1000), ctx->buffer, fps);
		gf_filter_update_status(filter, -1, szStatus);
	}

	return vout_draw_frame(ctx);
}

static GF_Err vout_draw_frame(GF_VideoOutCtx *ctx)
{
	ctx->force_release = GF_TRUE;
	if (ctx->pfmt && ctx->last_pck) {
#ifdef VOUT_USE_OPENGL
		if (ctx->disp < MODE_2D) {
			gf_rmt_begin_gl(vout_draw_gl);
			glGetError();
			vout_draw_gl(ctx, ctx->last_pck);
			gf_rmt_end_gl();
			glGetError();
		} else
#endif
		{
			vout_draw_2d(ctx, ctx->last_pck);
		}
	}
	if (ctx->no_buffering)
		ctx->force_release = GF_TRUE;
	else if (ctx->last_pck && gf_filter_pck_is_blocking_ref(ctx->last_pck) )
		ctx->force_release = GF_TRUE;

	if (ctx->force_release && ctx->last_pck && !ctx->dump_f_idx) {
		gf_filter_pck_unref(ctx->last_pck);
		ctx->last_pck = NULL;
	}
	//remember the clock right after the flush (in case of vsync)
	ctx->last_frame_clock = gf_sys_clock_high_res();

	//note we don't ask RT reschedule in frame duration, we take the decision on postponing the next frame in the next process call
	return GF_OK;
}

static Bool vout_process_event(GF_Filter *filter, const GF_FilterEvent *fevt)
{
	if (fevt->base.type==GF_FEVT_INFO_UPDATE) {
		GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) gf_filter_get_udta(filter);
		vout_set_caption(ctx);
		return GF_TRUE;
	}
	if (!fevt->base.on_pid && (fevt->base.type==GF_FEVT_USER)) {
		GF_VideoOutCtx *ctx = (GF_VideoOutCtx *) gf_filter_get_udta(filter);
		GF_Err e;
		if (!ctx->video_out) return GF_FALSE;
		e = ctx->video_out->ProcessEvent(ctx->video_out, (GF_Event *) &fevt->user_event.event);
		if (e) return GF_FALSE;
		return GF_TRUE;
	}
	//cancel
	return GF_TRUE;
}

#define OFFS(_n)	#_n, offsetof(GF_VideoOutCtx, _n)

static const GF_FilterArgs VideoOutArgs[] =
{
	{ OFFS(drv), "video driver name", GF_PROP_NAME, NULL, NULL, 0},
	{ OFFS(vsync), "enable video screen sync", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(drop), "enable droping late frames", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(disp), "display mode\n"
	"- gl: OpenGL\n"
	"- pbo: OpenGL with PBO\n"
	"- blit: 2D hardware blit\n"
	"- soft: software blit", GF_PROP_UINT, "gl", "gl|pbo|blit|soft", GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(start), "set playback start offset. Negative value means percent of media dur with -1 <=> dur", GF_PROP_DOUBLE, "0.0", NULL, 0},
	{ OFFS(dur), "only play the specified duration", GF_PROP_FRACTION64, "0", NULL, 0},
	{ OFFS(speed), "set playback speed when vsync is on. If speed is negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(hold), "number of seconds to hold display for single-frame streams. A negative value force a hold on last frame for single or multi-frames streams", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(linear), "use linear filtering instead of nearest pixel for GL mode", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(back), "back color for transparent images", GF_PROP_UINT, "0x808080", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(wsize), "default init window size. 0x0 holds the window size of the first frame. Negative values indicate video media size", GF_PROP_VEC2I, "-1x-1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(wpos), "default position (0,0 top-left)", GF_PROP_VEC2I, "-1x-1", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(vdelay), "set delay in sec, positive value displays after audio clock", GF_PROP_FRACTION, "0", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(hide), "hide output window", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(fullscreen), "use fullcreen", GF_PROP_BOOL, "false", NULL, 0},
	{ OFFS(buffer), "set buffer in ms", GF_PROP_UINT, "100", NULL, 0},
	{ OFFS(dumpframes), "ordered list of frames to dump, 1 being first frame - see filter help. Special value 0 means dump all frames", GF_PROP_UINT_LIST, NULL, NULL, GF_FS_ARG_HINT_EXPERT},
	{ OFFS(out), "radical of dump frame filenames. If no extension is provided, frames are exported as $OUT_%d.PFMT", GF_PROP_STRING, "dump", NULL, GF_FS_ARG_HINT_EXPERT},
	{0}
};

static const GF_FilterCapability VideoOutCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_RAW)
};


GF_FilterRegister VideoOutRegister = {
	.name = "vout",
	GF_FS_SET_DESCRIPTION("Video output")
	GF_FS_SET_HELP("This filter displays a single visual pid in a window.\n"\
	"The window is created unless a window handle (HWND, xWindow, etc) is indicated in the config file ( [Temp]OSWnd=ptr).\n"\
	"The output uses GPAC video output module indicated in [-drv]() option or in the config file (see GPAC core help).\n"\
	"The video output module can be further configured (see GPAC core help).\n"\
	"The filter can use openGL or 2D blitter of the graphics card, depending on the OS support.\n"\
	"The filter can be used do dump frames as written on the grapics card.\n"\
	"In this case, the window is not visible and only the listed frames are drawn to the GPU.\n"\
	"The pixel format of the dumped frame is always RGB in OpenGL and matches the video backbuffer format in 2D mode.\n"\
	)
	.private_size = sizeof(GF_VideoOutCtx),
	.flags = GF_FS_REG_MAIN_THREAD,
	.args = VideoOutArgs,
	SETCAPS(VideoOutCaps),
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
#else
const GF_FilterRegister *vout_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_PLAYER
