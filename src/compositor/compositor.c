/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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

#include <gpac/internal/compositor_dev.h>
#include <gpac/utf.h>
#include <gpac/modules/hardcoded_proto.h>
#include <gpac/modules/compositor_ext.h>

#include "nodes_stacks.h"

#include "visual_manager.h"
#include "texturing.h"

static void gf_sc_recompute_ar(GF_Compositor *compositor, GF_Node *top_node);

#define SC_DEF_WIDTH	320
#define SC_DEF_HEIGHT	240


GF_EXPORT
Bool gf_sc_send_event(GF_Compositor *compositor, GF_Event *evt)
{
	return gf_filter_forward_gf_event(compositor->filter, evt, GF_FALSE, GF_FALSE);
}


void gf_sc_next_frame_state(GF_Compositor *compositor, u32 state)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Forcing frame redraw state: %d\n", state));
	if (state==GF_SC_DRAW_FLUSH) {
		if (!compositor->skip_flush)
			compositor->skip_flush = 2;

		//if in OpenGL mode ignore refresh events (content of the window is still OK). This is only used for overlays in 2d
		if (!compositor->frame_draw_type
#ifndef GPAC_DISABLE_3D
		        && !compositor->visual->type_3d && !compositor->hybrid_opengl
#endif
		   ) {
			compositor->frame_draw_type = state;
		}
	} else {
		compositor->frame_draw_type = state;
	}
}


static void gf_sc_set_fullscreen(GF_Compositor *compositor)
{
	GF_Err e;
	if (!compositor->video_out->SetFullScreen) return;

	GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Switching fullscreen %s\n", compositor->fullscreen ? "off" : "on"));
	/*move to FS*/
	compositor->fullscreen = !compositor->fullscreen;

	//gf_sc_ar_control(compositor->audio_renderer, GF_SC_AR_PAUSE);

	//in windows (and other?) we may get blocked by SetWindowPos in the fullscreen method until another window thread dispatches a resize event,
	//which would try to grab the compositor mutex and thus deadlock us
	//to avoid this, unlock the compositor just for the SetFullscreen
	gf_mx_v(compositor->mx);
	if (compositor->fullscreen && (compositor->scene_width>=compositor->scene_height)
#ifndef GPAC_DISABLE_3D
	        && !compositor->visual->type_3d
#endif
	   ) {
		e = compositor->video_out->SetFullScreen(compositor->video_out, 2, &compositor->display_width, &compositor->display_height);
	} else {
		e = compositor->video_out->SetFullScreen(compositor->video_out, compositor->fullscreen, &compositor->display_width, &compositor->display_height);
	}
	gf_mx_p(compositor->mx);

	//gf_sc_ar_control(compositor->audio_renderer, GF_SC_AR_RESUME);

	if (e) {
		GF_Event evt;
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_MESSAGE;
		evt.message.message = "Cannot switch to fullscreen";
		evt.message.error = e;
		gf_sc_send_event(compositor, &evt);
		compositor->fullscreen = 0;
		compositor->video_out->SetFullScreen(compositor->video_out, 0, &compositor->display_width, &compositor->display_height);
	}
	GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] recomputing aspect ratio\n"));
	compositor->recompute_ar = 1;
	/*force signaling graphics reset*/
	if (!compositor->reset_graphics) gf_sc_reset_graphics(compositor);
}


/*this is needed for:
- audio: since the audio compositor may not be threaded, it must be reconfigured by another thread otherwise
we lock the audio module
- video: this is typical to OpenGL&co: multithreaded is forbidden, so resizing/fullscreen MUST be done by the same
thread accessing the HW resources
*/
static void gf_sc_reconfig_task(GF_Compositor *compositor)
{
	GF_Event evt;
	Bool notif_size=GF_FALSE;
	u32 width,height;

	/*reconfig audio if needed (non-threaded compositors)*/
	if (compositor->audio_renderer
#ifdef ENABLE_AOUT
	 	&& !compositor->audio_renderer->th
#endif
	)
	if (compositor->msg_type) {

		compositor->msg_type |= GF_SR_IN_RECONFIG;

		if (compositor->msg_type & GF_SR_CFG_INITIAL_RESIZE) {
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_VIDEO_SETUP;
			evt.setup.width = compositor->new_width;
			evt.setup.height = compositor->new_height;
			evt.setup.system_memory = compositor->video_memory ? GF_FALSE : GF_TRUE;
			evt.setup.disable_vsync = compositor->bench_mode ? GF_TRUE : GF_FALSE;

#ifndef GPAC_DISABLE_3D
			if (compositor->hybrid_opengl || compositor->force_opengl_2d) {
				evt.setup.use_opengl = GF_TRUE;
				evt.setup.system_memory = GF_FALSE;
				evt.setup.back_buffer = GF_TRUE;
			}

#endif
			compositor->video_out->ProcessEvent(compositor->video_out, &evt);

			if (evt.setup.use_opengl) {
				gf_opengl_init();
			}

			compositor->msg_type &= ~GF_SR_CFG_INITIAL_RESIZE;
		}
		/*scene size has been overridden*/
		if (compositor->msg_type & GF_SR_CFG_OVERRIDE_SIZE) {
			assert(!(compositor->override_size_flags & 2));
			compositor->msg_type &= ~GF_SR_CFG_OVERRIDE_SIZE;
			compositor->override_size_flags |= 2;
			width = compositor->scene_width;
			height = compositor->scene_height;
			compositor->has_size_info = 1;
			gf_sc_set_size(compositor, width, height);

			evt.type = GF_EVENT_SIZE;
			evt.size.width = width;
			evt.size.height = height;
			gf_sc_send_event(compositor, &evt);
		}
		/*size changed from scene cfg: resize window first*/
		if (compositor->msg_type & GF_SR_CFG_SET_SIZE) {
			u32 fs_width, fs_height;
			Bool restore_fs = compositor->fullscreen;

			GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Changing display size to %d x %d\n", compositor->new_width, compositor->new_height));
			fs_width = fs_height = 0;
			if (restore_fs) {
#if defined(GPAC_CONFIG_ANDROID) || defined(GPAC_CONFIG_IOS)
				if ((compositor->new_width>compositor->display_width) || (compositor->new_height>compositor->display_height)) {
					u32 w = compositor->display_width;
					compositor->display_width = compositor->display_height;
					compositor->display_height = w;
					compositor->recompute_ar = 1;
				}
#endif
				fs_width = compositor->display_width;
				fs_height = compositor->display_height;
			}
			evt.type = GF_EVENT_SIZE;
			evt.size.width = compositor->new_width;
			evt.size.height = compositor->new_height;

			/*send resize event*/
			if ( !(compositor->msg_type & GF_SR_CFG_WINDOWSIZE_NOTIF)) {
				compositor->video_out->ProcessEvent(compositor->video_out, &evt);
			}

			compositor->msg_type &= ~GF_SR_CFG_WINDOWSIZE_NOTIF;

			if (restore_fs) {
				if ((compositor->display_width != fs_width) || (compositor->display_height != fs_height)) {
					compositor->display_width = fs_width;
					compositor->display_height = fs_height;
					compositor->recompute_ar = 1;
				}
			} else {
				compositor->display_width = evt.size.width;
				compositor->display_height = evt.size.height;
				compositor->recompute_ar = 1;
				gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
			}
			notif_size=1;
			compositor->new_width = compositor->new_height = 0;
			compositor->msg_type &= ~GF_SR_CFG_SET_SIZE;
			GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Display size changed to %d x %d\n", compositor->new_width, compositor->new_height));
		}
		/*aspect ratio modif*/
		if (compositor->msg_type & GF_SR_CFG_AR) {
			compositor->msg_type &= ~GF_SR_CFG_AR;
			compositor->recompute_ar = 1;
		}
		/*fullscreen on/off request*/
		if (compositor->msg_type & GF_SR_CFG_FULLSCREEN) {
			compositor->msg_type &= ~GF_SR_CFG_FULLSCREEN;
			//video is about to resetup, wait for the setup
			if (compositor->recompute_ar) {
				compositor->fullscreen_postponed = 1;
			} else {
				gf_sc_set_fullscreen(compositor);
				gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
				notif_size=1;
			}
		}
		compositor->msg_type &= ~GF_SR_IN_RECONFIG;
	}
	if (notif_size) {
		gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_WIDTH, &PROP_UINT(compositor->display_width));
		gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_HEIGHT, &PROP_UINT(compositor->display_height));
	}

	/*3D driver changed message, recheck extensions*/
	if (compositor->reset_graphics) {
#ifndef GPAC_DISABLE_3D
		compositor->offscreen_width = compositor->offscreen_height = 0;
#endif
		gf_sc_lock(compositor, 0);
		evt.type = GF_EVENT_SYS_COLORS;
		if (compositor->video_out->ProcessEvent(compositor->video_out, &evt) ) {
			u32 i;
			for (i=0; i<28; i++) {
				compositor->sys_colors[i] = evt.sys_cols.sys_colors[i] & 0x00FFFFFF;
			}
		}
		gf_sc_lock(compositor, 1);
	}
}


static void gf_sc_frame_ifce_done(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	GF_FilterFrameInterface *frame_ifce = gf_filter_pck_get_frame_interface(pck);
	GF_Compositor *compositor = gf_filter_get_udta(filter);
	if (frame_ifce) {
		if (compositor->fb.video_buffer) {
			gf_sc_release_screen_buffer(compositor, &compositor->fb);
			compositor->fb.video_buffer = NULL;
		}
	}
	compositor->frame_ifce.user_data = NULL;
	compositor->flush_pending = (compositor->skip_flush!=1) ? GF_TRUE : GF_FALSE;
	compositor->skip_flush = 0;
}

GF_Err gf_sc_frame_ifce_get_plane(GF_FilterFrameInterface *frame_ifce, u32 plane_idx, const u8 **outPlane, u32 *outStride)
{
	GF_Err e = GF_BAD_PARAM;
	GF_Compositor *compositor = frame_ifce->user_data;

	if (plane_idx==0) {
		e = GF_OK;
		if (!compositor->fb.video_buffer)
			e = gf_sc_get_screen_buffer(compositor, &compositor->fb, 0);
	}
	*outPlane = compositor->fb.video_buffer;
	*outStride = compositor->fb.pitch_y;
	return e;
}
#ifndef GPAC_DISABLE_3D
GF_Err gf_sc_frame_ifce_get_gl_texture(GF_FilterFrameInterface *frame_ifce, u32 plane_idx, u32 *gl_tex_format, u32 *gl_tex_id, GF_Matrix_unexposed * texcoordmatrix)
{
	GF_Compositor *compositor = frame_ifce->user_data;
	if (!compositor->fbo_tx_id) return GF_BAD_PARAM;
	if (plane_idx) return GF_BAD_PARAM;
	if (gl_tex_id) *gl_tex_id = compositor->fbo_tx_id;
	if (gl_tex_format) *gl_tex_format = compositor_3d_get_fbo_pixfmt();
	//framebuffer is already oriented as a GL texture not as an image
	if (texcoordmatrix)
		gf_mx_add_scale(texcoordmatrix, FIX_ONE, -FIX_ONE, FIX_ONE);
	return GF_OK;
}
#endif

static void gf_sc_flush_video(GF_Compositor *compositor, Bool locked)
{
	GF_Window rc;

	//release compositor in case we have vsync
	if (locked) gf_sc_lock(compositor, 0);
	rc.x = rc.y = 0;
	rc.w = compositor->display_width;
	rc.h = compositor->display_height;
	compositor->video_out->Flush(compositor->video_out, &rc);
	compositor->flush_pending = GF_FALSE;
	if (locked) gf_sc_lock(compositor, 1);
}

void gf_sc_render_frame(GF_Compositor *compositor);

GF_EXPORT
Bool gf_sc_draw_frame(GF_Compositor *compositor, Bool no_flush, s32 *ms_till_next)
{
	Bool ret = GF_FALSE;

	gf_sc_ar_send_or_reconfig(compositor->audio_renderer);

	//frame still pending
	if (compositor->frame_ifce.user_data)
		return GF_TRUE;

	if (compositor->flush_pending) {
		gf_sc_flush_video(compositor, GF_FALSE);
	}
	compositor->skip_flush = no_flush ? 1 : 0;

	gf_sc_render_frame(compositor);

	if (ms_till_next) {
		if ( compositor->ms_until_next_frame == GF_INT_MAX)
			*ms_till_next = compositor->frame_duration;
		else
			*ms_till_next = compositor->ms_until_next_frame;
	}
	//next frame is late, we should redraw
	if (compositor->ms_until_next_frame < 0) ret = GF_TRUE;
	else if (compositor->frame_draw_type) ret = GF_TRUE;
	else if (compositor->fonts_pending>0) ret = GF_TRUE;

	return ret;
}


/*forces graphics redraw*/
GF_EXPORT
void gf_sc_reset_graphics(GF_Compositor *compositor)
{
	if (compositor) {
		Bool locked = gf_mx_try_lock(compositor->mx);
		compositor->reset_graphics = 1;
		if (locked) gf_mx_v(compositor->mx);
	}
}

static void gf_sc_reset_framerate(GF_Compositor *compositor)
{
	u32 i;
	for (i=0; i<GF_SR_FPS_COMPUTE_SIZE; i++) compositor->frame_time[i] = compositor->frame_dur[i] = 0;
	compositor->current_frame = 0;
}


enum
{
	GF_COMPOSITOR_THREAD_START = 0,
	GF_COMPOSITOR_THREAD_RUN,
	GF_COMPOSITOR_THREAD_ABORTING,
	GF_COMPOSITOR_THREAD_DONE,
	GF_COMPOSITOR_THREAD_INIT_FAILED,
};

static GF_Err nullvout_setup(struct _video_out *vout, void *os_handle, void *os_display, u32 init_flags)
{
	return GF_OK;
}
static void nullvout_shutdown(struct _video_out *vout)
{
	return;
}
static GF_Err nullvout_flush(struct _video_out *vout, GF_Window *dest)
{
	return GF_OK;
}
static GF_Err nullvout_fullscreen(struct _video_out *vout, Bool fs_on, u32 *new_disp_width, u32 *new_disp_height)
{
	return GF_OK;
}
static GF_Err nullvout_evt(struct _video_out *vout, GF_Event *event)
{
	return GF_OK;
}
static GF_Err nullvout_lock(struct _video_out *vout, GF_VideoSurface *video_info, Bool do_lock)
{
	return GF_IO_ERR;
}
static GF_Err nullvout_blit(struct _video_out *vout, GF_VideoSurface *video_src, GF_Window *src_wnd, GF_Window *dst_wnd, u32 overlay_type)
{
	return GF_OK;
}

GF_VideoOutput null_vout = {
	.Setup = nullvout_setup,
	.Shutdown = nullvout_shutdown,
	.Flush = nullvout_flush,
	.SetFullScreen = nullvout_fullscreen,
	.ProcessEvent = nullvout_evt,
	.LockBackBuffer = nullvout_lock,
	.Blit = nullvout_blit,
	.hw_caps = GF_VIDEO_HW_HAS_RGB | GF_VIDEO_HW_HAS_RGBA | GF_VIDEO_HW_HAS_YUV | GF_VIDEO_HW_HAS_YUV_OVERLAY | GF_VIDEO_HW_HAS_STRETCH
};

static GF_Err gl_vout_evt(struct _video_out *vout, GF_Event *evt)
{
	GF_Compositor *compositor = (GF_Compositor *)vout->opaque;
	if (!evt || (evt->type != GF_EVENT_VIDEO_SETUP)) return GF_OK;

	if (!compositor->player && (compositor->passthrough_pfmt != GF_PIXEL_RGB)) {
		u32 pfmt = compositor->forced_alpha ? GF_PIXEL_RGBA : GF_PIXEL_RGB;
		compositor->passthrough_pfmt = pfmt;
		compositor->opfmt = pfmt;
		if (compositor->vout) {
			u32 stride = compositor->output_width * ( (pfmt == GF_PIXEL_RGBA) ? 4 : 3 );
			gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_PIXFMT, &PROP_UINT(pfmt));
			gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_STRIDE, &PROP_UINT(stride));
		}
	}

	
#ifndef GPAC_DISABLE_3D
	return compositor_3d_setup_fbo(evt->setup.width, evt->setup.height, &compositor->fbo_id, &compositor->fbo_tx_id, &compositor->fbo_depth_id);
#else
	return GF_NOT_SUPPORTED;
#endif
}

GF_VideoOutput gl_vout = {
	.Setup = nullvout_setup,
	.Shutdown = nullvout_shutdown,
	.Flush = nullvout_flush,
	.SetFullScreen = nullvout_fullscreen,
	.ProcessEvent = gl_vout_evt,
	.LockBackBuffer = NULL,
	.Blit = NULL,
	.hw_caps = GF_VIDEO_HW_OPENGL
};

static GF_Err rawvout_lock(struct _video_out *vout, GF_VideoSurface *vi, Bool do_lock)
{
	GF_Compositor *compositor = (GF_Compositor *)vout->opaque;

	if (do_lock) {
		u32 pfmt;
		if (!vi) return GF_BAD_PARAM;
		pfmt = compositor->opfmt;
		if (!pfmt && compositor->passthrough_txh) pfmt = compositor->passthrough_txh->pixelformat;

		if (!pfmt) {
			pfmt = compositor->forced_alpha ? GF_PIXEL_RGBA : GF_PIXEL_RGB;
		}

		memset(vi, 0, sizeof(GF_VideoSurface));
		vi->width = compositor->display_width;
		vi->height = compositor->display_height;
		gf_pixel_get_size_info(pfmt, compositor->display_width, compositor->display_height, NULL, &vi->pitch_y, NULL, NULL, NULL);
		if (compositor->passthrough_txh && !compositor->passthrough_txh->frame_ifce && (pfmt == compositor->passthrough_txh->pixelformat)) {
			if (!compositor->passthrough_pck) {
				compositor->passthrough_pck = gf_filter_pck_new_clone(compositor->vout, compositor->passthrough_txh->stream->pck, &compositor->passthrough_data);
				if (!compositor->passthrough_pck) return GF_OUT_OF_MEM;
			}

			vi->video_buffer = compositor->passthrough_data;
			vi->pitch_y = compositor->passthrough_txh->stride;
		} else {
			vi->video_buffer = compositor->framebuffer;
		}
		vi->pitch_x = gf_pixel_get_bytes_per_pixel(pfmt);
		vi->pixel_format = pfmt;
		compositor->passthrough_pfmt = pfmt;
	}
	return GF_OK;
}

static GF_Err rawvout_evt(struct _video_out *vout, GF_Event *evt)
{
	u32 pfmt, stride, stride_uv;
	GF_Compositor *compositor = (GF_Compositor *)vout->opaque;
	if (!evt || (evt->type != GF_EVENT_VIDEO_SETUP)) return GF_OK;

	pfmt = compositor->opfmt;
	if (!pfmt) {
		pfmt = compositor->forced_alpha ? GF_PIXEL_RGBA : GF_PIXEL_RGB;
	}

	compositor->passthrough_pfmt = pfmt;
	stride=0;
	stride_uv = 0;
	gf_pixel_get_size_info(pfmt, evt->setup.width, evt->setup.height, &compositor->framebuffer_size, &stride, &stride_uv, NULL, NULL);

	if (compositor->vout) {
		gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_PIXFMT, &PROP_UINT(pfmt));
		gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_STRIDE, &PROP_UINT(stride));
		gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_STRIDE_UV, stride_uv ? &PROP_UINT(stride_uv) : NULL);
	}

	if (compositor->framebuffer_size > compositor->framebuffer_alloc) {
		compositor->framebuffer_alloc = compositor->framebuffer_size;
		compositor->framebuffer = gf_realloc(compositor->framebuffer, sizeof(char)*compositor->framebuffer_size);
	}
	if (!compositor->framebuffer) return GF_OUT_OF_MEM;
	memset(compositor->framebuffer, 0, sizeof(char)*compositor->framebuffer_size);

#ifndef GPAC_DISABLE_3D
	if (compositor->needs_offscreen_gl && (compositor->ogl != GF_SC_GLMODE_OFF))
		return compositor_3d_setup_fbo(evt->setup.width, evt->setup.height, &compositor->fbo_id, &compositor->fbo_tx_id, &compositor->fbo_depth_id);
#endif

	evt->setup.use_opengl = GF_FALSE;
	return GF_OK;
}

GF_VideoOutput raw_vout = {
	.Setup = nullvout_setup,
	.Shutdown = nullvout_shutdown,
	.Flush = nullvout_flush,
	.SetFullScreen = nullvout_fullscreen,
	.ProcessEvent = rawvout_evt,
	.LockBackBuffer = rawvout_lock,
	.Blit = NULL,
	.hw_caps = GF_VIDEO_HW_HAS_RGB | GF_VIDEO_HW_HAS_RGBA
};

void gf_sc_setup_passthrough(GF_Compositor *compositor)
{
	u32 timescale;
	Bool is_raw_out = GF_FALSE;
	u32 update_pfmt = 0;
	compositor->passthrough_inplace = GF_FALSE;

	if (!compositor->passthrough_txh) {
		compositor->passthrough_timescale = 0;
		return;
	}

	timescale = gf_filter_pck_get_timescale(compositor->passthrough_txh->stream->pck);
	if (compositor->passthrough_timescale != timescale) {

		compositor->passthrough_timescale = timescale;
		gf_filter_pid_copy_properties(compositor->vout, compositor->passthrough_txh->stream->odm->pid);
		if  (compositor->video_out==&raw_vout) {
			update_pfmt = compositor->opfmt ? compositor->opfmt :  compositor->passthrough_txh->pixelformat;
		} else {
			update_pfmt = GF_PIXEL_RGB;
		}
	}
	if (compositor->video_out == &raw_vout) {
		is_raw_out = GF_TRUE;
		if (!compositor->opfmt || (compositor->opfmt == compositor->passthrough_txh->pixelformat) ) {
			if (!compositor->passthrough_txh->frame_ifce) {
				compositor->passthrough_inplace = GF_TRUE;
				update_pfmt = GF_FALSE;
			}
			if (!compositor->opfmt && (compositor->passthrough_pfmt != compositor->passthrough_txh->pixelformat)) {
				compositor->passthrough_pfmt = compositor->passthrough_txh->pixelformat;
				gf_filter_pid_copy_properties(compositor->vout, compositor->passthrough_txh->stream->odm->pid);
			}
		}
	}

	if (update_pfmt) {
		u32 stride=0, stride_uv=0, out_size=0;
		gf_pixel_get_size_info(update_pfmt, compositor->display_width, compositor->display_width, &out_size, &stride, &stride_uv, NULL, NULL);
		gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_PIXFMT, &PROP_UINT(update_pfmt));
		gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_STRIDE, &PROP_UINT(stride));
		gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_STRIDE_UV, stride_uv ? &PROP_UINT(stride_uv) : NULL);

		if (is_raw_out && !compositor->passthrough_inplace && (compositor->framebuffer_size != out_size) ) {
			compositor->framebuffer_size = out_size;
			if (out_size > compositor->framebuffer_alloc) {
				compositor->framebuffer_alloc = out_size;
				compositor->framebuffer = gf_realloc(compositor->framebuffer, out_size);
			}
		}
	}
}

static GF_Err gf_sc_load_driver(GF_Compositor *compositor)
{
	GF_Err e;
	const char *sOpt;
	void *os_disp=NULL;

	//filter mode
	if (!compositor->player) {
		compositor->video_out = &null_vout;

#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
			nullvout_setup(NULL, NULL, NULL, 0);
			nullvout_fullscreen(NULL, GF_FALSE, NULL, NULL);
			nullvout_evt(NULL, NULL);
			nullvout_lock(NULL, NULL, GF_FALSE);
			nullvout_blit(NULL, NULL, NULL, NULL, 0);
		}
#endif

		e = gf_filter_request_opengl(compositor->filter);
		if (e) return e;
#ifndef GPAC_DISABLE_3D
		gf_sc_load_opengl_extensions(compositor, GF_TRUE);
		if (!compositor->gl_caps.fbo)
#endif
		{
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Compositor] No support for OpenGL framebuffer object, cannot run in GL mode.\n"));
			compositor->drv = GF_SC_DRV_OFF;
			return GF_NOT_SUPPORTED;
		}
		compositor->video_out = &gl_vout;
		compositor->video_out->opaque = compositor;
		return GF_OK;
	}

	//player mode
#ifndef GPAC_DISABLE_3D
	sOpt = gf_opts_get_key("core", "video-output");
	if (sOpt && !strcmp(sOpt, "glfbo")) {
		compositor->video_out = &gl_vout;
		compositor->video_out->opaque = compositor;
		sOpt = gf_opts_get_key("core", "glfbo-txid");
		if (sOpt) {
		 	compositor->fbo_tx_id = atoi(sOpt);
		 	compositor->external_tx_id = GF_TRUE;
		}
		if (!compositor->fbo_tx_id) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Compositor] glfbo driver specified but no target texture ID found, cannot initialize\n"));
			compositor->drv = GF_SC_DRV_OFF;
			return GF_BAD_PARAM;
		}
		gf_sc_load_opengl_extensions(compositor, GF_TRUE);
		if (!compositor->gl_caps.fbo) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Compositor] No support for OpenGL framebuffer object, cannot run in glfbo mode.\n"));
			compositor->drv = GF_SC_DRV_OFF;
			return GF_NOT_SUPPORTED;
		}
		return GF_OK;
	}
#endif

	compositor->video_out = (GF_VideoOutput *) gf_module_load(GF_VIDEO_OUTPUT_INTERFACE, NULL);

	if (!compositor->video_out ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Compositor] Failed to load a video output module.\n"));
		return GF_IO_ERR;
	}

	sOpt = gf_opts_get_key("temp", "window-display");
	if (sOpt) sscanf(sOpt, "%p", &os_disp);

	compositor->video_out->evt_cbk_hdl = compositor;
	compositor->video_out->on_event = gf_sc_on_event;
	/*init hw*/
	e = compositor->video_out->Setup(compositor->video_out, compositor->os_wnd, os_disp, compositor->init_flags);
	if (e != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Compositor] Error setuing up video output: %s\n", gf_error_to_string(e) ));
		return e;
	}
	if (!gf_opts_get_key("core", "video-output")) {
		gf_opts_set_key("core", "video-output", compositor->video_out->module_name);
	}
	gf_filter_register_opengl_provider(compositor->filter, GF_TRUE);
	return GF_OK;
}

#ifndef GPAC_DISABLE_3D
static void gf_sc_check_video_driver(GF_Compositor *compositor)
{
	Bool force_gl_out = GF_FALSE;
	if (compositor->player) return;
	if (compositor->video_out == &null_vout) return;

	if (compositor->visual->type_3d || compositor->hybrid_opengl) {
		force_gl_out = GF_TRUE;
	} else if (compositor->needs_offscreen_gl && (compositor->ogl!=GF_SC_GLMODE_OFF) ) {
		force_gl_out = GF_TRUE;
	}

	if (force_gl_out) {
		GF_Err e;
		if (compositor->video_out != &raw_vout) return;
		e = gf_sc_load_driver(compositor);
		if (e) {
			compositor->video_out = &raw_vout;
			compositor->video_memory = 2;
		}
		return;
	}

	if (compositor->video_out == &raw_vout) {
		if (compositor->needs_offscreen_gl) {
			gf_filter_request_opengl(compositor->filter);
		}
		return;
	}
	compositor->video_out = &raw_vout;
	compositor->video_memory = 2;
}
#endif

GF_Err gf_sc_load(GF_Compositor *compositor)
{
	u32 i;
	const char *sOpt;

	compositor->mx = gf_mx_new("Compositor");
	if (!compositor->mx) return GF_OUT_OF_MEM;

	/*load proto modules*/
	compositor->proto_modules = gf_list_new();
	if (!compositor->proto_modules) return GF_OUT_OF_MEM;
	for (i=0; i< gf_modules_count(); i++) {
		GF_HardcodedProto *ifce = (GF_HardcodedProto *) gf_modules_load(i, GF_HARDCODED_PROTO_INTERFACE);
		if (ifce) gf_list_add(compositor->proto_modules, ifce);
	}

	compositor->init_flags = 0;
	compositor->os_wnd = NULL;
	sOpt = gf_opts_get_key("temp", "window-handle");
	if (sOpt) sscanf(sOpt, "%p", &compositor->os_wnd);
	sOpt = gf_opts_get_key("temp", "window-flags");
	if (sOpt) compositor->init_flags = atoi(sOpt);

	/*force initial for 2D/3D setup*/
	compositor->msg_type |= GF_SR_CFG_INITIAL_RESIZE;
	/*set default size if owning output*/
	if (!compositor->os_wnd) {
		compositor->new_width = compositor->osize.x>0 ? compositor->osize.x : SC_DEF_WIDTH;
		compositor->new_height = compositor->osize.y>0 ? compositor->osize.y : SC_DEF_HEIGHT;
		compositor->msg_type |= GF_SR_CFG_SET_SIZE;
	}

	if (!compositor->player)
		compositor->init_flags = GF_VOUT_INIT_HIDE;

	if (gf_opts_get_key("temp", "gpac-help")) {
		gf_opts_set_key("temp", "no-video", "yes");
		compositor->noaudio = GF_TRUE;
	}

	//used for GUI help and in bench mode without vout
	if (gf_opts_get_bool("temp", "no-video")) {
		compositor->video_out = &null_vout;
		compositor->ogl = GF_SC_GLMODE_OFF;
	} else if (compositor->player || (compositor->drv==GF_SC_DRV_ON)  || (compositor->ogl==GF_SC_GLMODE_HYBRID) ){
		GF_Err e;

		e = gf_sc_load_driver(compositor);
		if (e) return e;
	} else {
		raw_vout.opaque = compositor;
		compositor->video_out = &raw_vout;
		compositor->video_memory = 2;
	}

	compositor->strike_bank = gf_list_new();
	if (!compositor->strike_bank) return GF_OUT_OF_MEM;
	compositor->visuals = gf_list_new();
	if (!compositor->visuals) return GF_OUT_OF_MEM;

	GF_SAFEALLOC(compositor->traverse_state, GF_TraverseState);
	if (!compositor->traverse_state) return GF_OUT_OF_MEM;

	compositor->traverse_state->vrml_sensors = gf_list_new();
	if (!compositor->traverse_state->vrml_sensors) return GF_OUT_OF_MEM;
	compositor->traverse_state->use_stack = gf_list_new();
	if (!compositor->traverse_state->use_stack) return GF_OUT_OF_MEM;
#ifndef GPAC_DISABLE_3D
	compositor->traverse_state->local_lights = gf_list_new();
	if (!compositor->traverse_state->local_lights) return GF_OUT_OF_MEM;
#endif
	compositor->sensors = gf_list_new();
	if (!compositor->sensors) return GF_OUT_OF_MEM;
	compositor->previous_sensors = gf_list_new();
	if (!compositor->previous_sensors) return GF_OUT_OF_MEM;
	compositor->hit_use_stack = gf_list_new();
	if (!compositor->hit_use_stack) return GF_OUT_OF_MEM;
	compositor->prev_hit_use_stack = gf_list_new();
	if (!compositor->prev_hit_use_stack) return GF_OUT_OF_MEM;
	compositor->focus_ancestors = gf_list_new();
	if (!compositor->focus_ancestors) return GF_OUT_OF_MEM;
	compositor->focus_use_stack = gf_list_new();
	if (!compositor->focus_use_stack) return GF_OUT_OF_MEM;

	compositor->env_tests = gf_list_new();
	if (!compositor->env_tests) return GF_OUT_OF_MEM;

	/*setup main visual*/
	compositor->visual = visual_new(compositor);
	if (!compositor->visual) return GF_OUT_OF_MEM;
	compositor->visual->GetSurfaceAccess = compositor_2d_get_video_access;
	compositor->visual->ReleaseSurfaceAccess = compositor_2d_release_video_access;
	compositor->visual->CheckAttached = compositor_2d_check_attached;
	compositor->visual->ClearSurface = compositor_2d_clear_surface;

	compositor_2d_init_callbacks(compositor);

	if (compositor->video_out->FlushRectangles)
		compositor->visual->direct_flush = GF_TRUE;

	// default value for depth gain is not zero
#ifdef GF_SR_USE_DEPTH
	compositor->traverse_state->depth_gain = FIX_ONE;
#endif

	gf_list_add(compositor->visuals, compositor->visual);

	compositor->zoom = compositor->scale_x = compositor->scale_y = FIX_ONE;

	/*create a drawable for focus highlight*/
	compositor->focus_highlight = drawable_new();
	if (!compositor->focus_highlight) return GF_OUT_OF_MEM;
	/*associate a dummy node for traversing*/
	compositor->focus_highlight->node = gf_node_new(NULL, TAG_UndefinedNode);
	if (!compositor->focus_highlight->node) return GF_OUT_OF_MEM;
	gf_node_register(compositor->focus_highlight->node, NULL);
	gf_node_set_callback_function(compositor->focus_highlight->node, drawable_traverse_focus);


#ifndef GPAC_DISABLE_3D
	/*default collision mode*/
	compositor->collide_mode = GF_COLLISION_DISPLACEMENT; //GF_COLLISION_NORMAL
	compositor->gravity_on = GF_TRUE;

	/*create default unit sphere and box for bounds*/
	compositor->unit_bbox = new_mesh();
	if (!compositor->unit_bbox) return GF_OUT_OF_MEM;
	mesh_new_unit_bbox(compositor->unit_bbox);
	gf_mx_init(compositor->traverse_state->model_matrix);
#endif

	compositor->was_system_memory=1;

	compositor->systems_pids = gf_list_new();
	if (!compositor->systems_pids) return GF_OUT_OF_MEM;
	compositor->textures = gf_list_new();
	if (!compositor->textures) return GF_OUT_OF_MEM;
	compositor->textures_gc = gf_list_new();
	if (!compositor->textures_gc) return GF_OUT_OF_MEM;
	if (!compositor->fps.num || !compositor->fps.den) {
		compositor->fps.num = 30;
		compositor->fps.den = 1;
	}
	compositor->frame_duration = compositor->fps.num * 1000;
	compositor->frame_duration /= compositor->fps.den;

	compositor->time_nodes = gf_list_new();
	if (!compositor->time_nodes) return GF_OUT_OF_MEM;
	compositor->event_queue = gf_list_new();
	if (!compositor->event_queue) return GF_OUT_OF_MEM;
	compositor->event_queue_back = gf_list_new();
	if (!compositor->event_queue_back) return GF_OUT_OF_MEM;
	compositor->evq_mx = gf_mx_new("EventQueue");
	if (!compositor->evq_mx) return GF_OUT_OF_MEM;

#ifdef GF_SR_USE_VIDEO_CACHE
	compositor->cached_groups = gf_list_new();
	if (!compositor->cached_groups) return GF_OUT_OF_MEM;
	compositor->cached_groups_queue = gf_list_new();
	if (!compositor->cached_groups_queue) return GF_OUT_OF_MEM;
#endif

	/*load audio renderer*/
	if (!compositor->audio_renderer)
		compositor->audio_renderer = gf_sc_ar_load(compositor, compositor->init_flags);

	gf_sc_reset_framerate(compositor);
	compositor->font_manager = gf_filter_get_font_manager(compositor->filter);
	if (!compositor->font_manager) return GF_OUT_OF_MEM;

	compositor->extra_scenes = gf_list_new();
	if (!compositor->extra_scenes) return GF_OUT_OF_MEM;
	compositor->interaction_level = GF_INTERACT_NORMAL | GF_INTERACT_INPUT_SENSOR | GF_INTERACT_NAVIGATION;

	compositor->scene_sampled_clock = 0;
	compositor->video_th_id = gf_th_id();

	/*load extensions*/
	compositor->extensions = gf_list_new();
	if (!compositor->extensions) return GF_OUT_OF_MEM;
	compositor->unthreaded_extensions = gf_list_new();
	if (!compositor->unthreaded_extensions) return GF_OUT_OF_MEM;
	for (i=0; i< gf_modules_count(); i++) {
		GF_CompositorExt *ifce = (GF_CompositorExt *) gf_modules_load(i, GF_COMPOSITOR_EXT_INTERFACE);
		if (ifce) {

			if (!ifce->process(ifce, GF_COMPOSITOR_EXT_START, compositor)) {
				gf_modules_close_interface((GF_BaseInterface *) ifce);
				continue;
			}
			gf_list_add(compositor->extensions, ifce);
			if (ifce->caps & GF_COMPOSITOR_EXTENSION_NOT_THREADED)
				gf_list_add(compositor->unthreaded_extensions, ifce);
		}
	}

	if (!gf_list_count(compositor->unthreaded_extensions)) {
		gf_list_del(compositor->unthreaded_extensions);
		compositor->unthreaded_extensions = NULL;
	}

	compositor->display_width = 320;
	compositor->display_height = 240;
	compositor->recompute_ar = GF_TRUE;
	compositor->scene_sampled_clock = 0;
	if (compositor->autoconfig_opengl || compositor->hybrid_opengl)
		gf_sc_recompute_ar(compositor, NULL);

	/*try to load GL extensions*/
#ifndef GPAC_DISABLE_3D
	gf_sc_load_opengl_extensions(compositor, GF_FALSE);
#endif

	gf_sc_reload_config(compositor);


	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI]\tCompositor Cycle Log\tNetworks\tDecoders\tFrame\tDirect Draw\tVisual Config\tEvent\tRoute\tSMIL Timing\tTime node\tTexture\tSMIL Anim\tTraverse setup\tTraverse (and direct Draw)\tTraverse (and direct Draw) without anim\tIndirect Draw\tTraverse And Draw (Indirect or Not)\tFlush\tCycle\n"));
	return GF_OK;
}

void gf_sc_unload(GF_Compositor *compositor)
{
	u32 i;
	if (!compositor) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Destroying\n"));
	compositor->discard_input_events = GF_TRUE;
	gf_sc_lock(compositor, GF_TRUE);

#ifndef GPAC_DISABLE_3D
	compositor_2d_reset_gl_auto(compositor);
#endif
	gf_sc_texture_cleanup_hw(compositor);

	if (compositor->video_out) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Closing video output\n"));
		compositor->video_out->Shutdown(compositor->video_out);
		gf_modules_close_interface((GF_BaseInterface *)compositor->video_out);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Closing visual compositor\n"));

	if (compositor->focus_highlight) {
		gf_node_unregister(compositor->focus_highlight->node, NULL);
		drawable_del_ex(compositor->focus_highlight, compositor, GF_FALSE);
	}
	if (compositor->selected_text) gf_free(compositor->selected_text);
	if (compositor->sel_buffer) gf_free(compositor->sel_buffer);

	if (compositor->visual) visual_del(compositor->visual);
	if (compositor->sensors) gf_list_del(compositor->sensors);
	if (compositor->previous_sensors) gf_list_del(compositor->previous_sensors);
	if (compositor->visuals) gf_list_del(compositor->visuals);
	if (compositor->strike_bank) gf_list_del(compositor->strike_bank);
	if (compositor->hit_use_stack) gf_list_del(compositor->hit_use_stack);
	if (compositor->prev_hit_use_stack) gf_list_del(compositor->prev_hit_use_stack);
	if (compositor->focus_ancestors) gf_list_del(compositor->focus_ancestors);
	if (compositor->focus_use_stack) gf_list_del(compositor->focus_use_stack);
	if (compositor->env_tests) gf_list_del(compositor->env_tests);
	if (compositor->systems_pids) gf_list_del(compositor->systems_pids);

	if (compositor->traverse_state) {
		gf_list_del(compositor->traverse_state->vrml_sensors);
		gf_list_del(compositor->traverse_state->use_stack);
#ifndef GPAC_DISABLE_3D
		gf_list_del(compositor->traverse_state->local_lights);
#endif
		gf_free(compositor->traverse_state);
	}

#ifndef GPAC_DISABLE_3D
	if (compositor->unit_bbox) mesh_free(compositor->unit_bbox);

	if (compositor->screen_buffer) gf_free(compositor->screen_buffer);
	if (compositor->line_buffer) gf_free(compositor->line_buffer);
#endif
	if (compositor->framebuffer) gf_free(compositor->framebuffer);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Unloading visual compositor module\n"));

	if (compositor->audio_renderer) gf_sc_ar_del(compositor->audio_renderer);
	compositor->audio_renderer = NULL;

	/*unload proto modules*/
	if (compositor->proto_modules) {
		for (i=0; i< gf_list_count(compositor->proto_modules); i++) {
			GF_HardcodedProto *ifce = gf_list_get(compositor->proto_modules, i);
			gf_modules_close_interface((GF_BaseInterface *) ifce);
		}
		gf_list_del(compositor->proto_modules);
	}
	if (compositor->evq_mx) gf_mx_p(compositor->evq_mx);
	while (gf_list_count(compositor->event_queue)) {
		GF_QueuedEvent *qev = (GF_QueuedEvent *)gf_list_get(compositor->event_queue, 0);
		gf_list_rem(compositor->event_queue, 0);
		gf_free(qev);
	}
	while (gf_list_count(compositor->event_queue_back)) {
		GF_QueuedEvent *qev = (GF_QueuedEvent *)gf_list_get(compositor->event_queue_back, 0);
		gf_list_rem(compositor->event_queue, 0);
		gf_free(qev);
	}
	if (compositor->evq_mx) {
		gf_mx_v(compositor->evq_mx);
		gf_mx_del(compositor->evq_mx);
	}
	gf_list_del(compositor->event_queue);
	gf_list_del(compositor->event_queue_back);

#ifdef GF_SR_USE_VIDEO_CACHE
	gf_list_del(compositor->cached_groups);
	gf_list_del(compositor->cached_groups_queue);
#endif

	if (compositor->textures) gf_list_del(compositor->textures);
	if (compositor->textures_gc) gf_list_del(compositor->textures_gc);
	if (compositor->time_nodes) gf_list_del(compositor->time_nodes);
	if (compositor->extra_scenes) gf_list_del(compositor->extra_scenes);
	if (compositor->input_streams) gf_list_del(compositor->input_streams);
	if (compositor->x3d_sensors) gf_list_del(compositor->x3d_sensors);


	/*unload extensions*/
	for (i=0; i< gf_list_count(compositor->extensions); i++) {
		GF_CompositorExt *ifce = gf_list_get(compositor->extensions, i);
		ifce->process(ifce, GF_COMPOSITOR_EXT_STOP, NULL);
	}

	/*remove all event filters*/
//	gf_list_reset(term->event_filters);

	/*unload extensions*/
	for (i=0; i< gf_list_count(compositor->extensions); i++) {
		GF_CompositorExt *ifce = gf_list_get(compositor->extensions, i);
		gf_modules_close_interface((GF_BaseInterface *) ifce);
	}
	gf_list_del(compositor->extensions);
	gf_list_del(compositor->unthreaded_extensions);

	gf_sc_lock(compositor, GF_FALSE);
	gf_mx_del(compositor->mx);
	if (compositor->reload_url) gf_free(compositor->reload_url);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Destroyed\n"));
}

void gf_sc_set_fps(GF_Compositor *compositor, GF_Fraction fps)
{
	if (fps.den) {
		u64 dur;
		compositor->fps = fps;
		dur = fps.den;
		dur *= 1000;
		dur /= fps.num;

		compositor->frame_duration = (u32) dur;
		gf_sc_reset_framerate(compositor);

		if (compositor->vout && !compositor->timescale) {
			gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_TIMESCALE, &PROP_UINT(fps.num) );

		}
	}
}

static void gf_sc_set_play_state(GF_Compositor *compositor, u32 PlayState)
{
	if (!compositor || !compositor->audio_renderer) return;
	if (!compositor->paused && !PlayState) return;
	if (compositor->paused && (PlayState==GF_STATE_PAUSED)) return;

	/*step mode*/
	if (PlayState==GF_STATE_STEP_PAUSE) {
		compositor->step_mode = GF_TRUE;
		compositor->paused = 1;
	} else {
		compositor->step_mode = GF_FALSE;
		if (compositor->audio_renderer) gf_sc_ar_control(compositor->audio_renderer, (compositor->paused && (PlayState==0xFF)) ? GF_SC_AR_RESET_HW_AND_PLAY : (compositor->paused ? GF_SC_AR_RESUME : GF_SC_AR_PAUSE) );
		compositor->paused = (PlayState==GF_STATE_PAUSED) ? 1 : 0;
	}
}

GF_EXPORT
u32 gf_sc_get_clock(GF_Compositor *compositor)
{
	if (!compositor->bench_mode && compositor->player) {
		return gf_sc_ar_get_clock(compositor->audio_renderer);
	}
	return compositor->scene_sampled_clock;
}

GF_EXPORT
GF_Err gf_sc_set_scene_size(GF_Compositor *compositor, u32 Width, u32 Height, Bool force_size)
{
	if (!Width || !Height) {
		if (compositor->override_size_flags) {
			/*specify a small size to detect biggest bitmap but not 0 in case only audio..*/
			compositor->scene_height = SC_DEF_HEIGHT;
			compositor->scene_width = SC_DEF_WIDTH;
		} else {
			/*use current res*/
			compositor->scene_width = compositor->new_width ? compositor->new_width : compositor->display_width;
			compositor->scene_height = compositor->new_height ? compositor->new_height : compositor->display_height;
		}
	} else {
		compositor->scene_height = Height;
		compositor->scene_width = Width;
	}
	if (force_size)
		compositor->has_size_info = 1;
	return GF_OK;
}

#ifndef GPAC_DISABLE_SVG
GF_EXPORT
Fixed gf_sc_svg_convert_length_to_display(GF_Compositor *compositor, SVG_Length *length)
{
	/* Assuming the environment is 90dpi*/
	u32 dpi = 90;
	if (!length) return 0;

	switch (length->type) {
	case SVG_NUMBER_PERCENTAGE:
		break;
	case SVG_NUMBER_EMS:
		break;
	case SVG_NUMBER_EXS:
		break;
	case SVG_NUMBER_VALUE:
		break;
	case SVG_NUMBER_PX:
		return length->value;
	case SVG_NUMBER_CM:
		return gf_mulfix(length->value, dpi*FLT2FIX(0.39));
		break;
	case SVG_NUMBER_MM:
		return gf_mulfix(length->value, dpi*FLT2FIX(0.039));
	case SVG_NUMBER_IN:
		return length->value * dpi;
	case SVG_NUMBER_PT:
		return (dpi * length->value) / 12;
	case SVG_NUMBER_PC:
		return (dpi*length->value) / 6;
	case SVG_NUMBER_INHERIT:
		break;
	}
	return length->value;
}
#endif

void compositor_set_ar_scale(GF_Compositor *compositor, Fixed scaleX, Fixed scaleY)
{
	compositor->trans_x = gf_muldiv(compositor->trans_x, scaleX, compositor->scale_x);
	compositor->trans_y = gf_muldiv(compositor->trans_y, scaleY, compositor->scale_y);

	compositor->zoom_changed = 1;
	compositor->scale_x = scaleX;
	compositor->scale_y = scaleY;

	compositor_2d_set_user_transform(compositor, compositor->zoom, compositor->trans_x, compositor->trans_y, 1);
}

static void gf_sc_reset(GF_Compositor *compositor, Bool has_scene)
{
	Bool mode2d;
	GF_VisualManager *visual;
	u32 i=0;

	//init failed
	if (!compositor->traverse_state)
		return;

	while ((visual = (GF_VisualManager *)gf_list_enum(compositor->visuals, &i))) {
		/*reset display list*/
		visual->cur_context = visual->context;
		if (visual->cur_context) visual->cur_context->drawable = NULL;
		while (visual->prev_nodes) {
			struct _drawable_store *cur = visual->prev_nodes;
			visual->prev_nodes = cur->next;
			gf_free(cur);
		}
		visual->last_prev_entry = NULL;
		visual->to_redraw.count = 0;

		/*reset the surface as well*/
		if (visual->raster_surface) gf_evg_surface_delete(visual->raster_surface);
		visual->raster_surface = NULL;
	}

	gf_list_reset(compositor->sensors);
	gf_list_reset(compositor->previous_sensors);

	/*reset traverse state*/
	mode2d = compositor->traverse_state->immediate_draw;
	gf_list_del(compositor->traverse_state->vrml_sensors);
	gf_list_del(compositor->traverse_state->use_stack);
#ifndef GPAC_DISABLE_3D
	gf_list_del(compositor->traverse_state->local_lights);

	compositor_3d_delete_fbo(&compositor->fbo_id, &compositor->fbo_tx_id, &compositor->fbo_depth_id, compositor->external_tx_id);
#endif

	memset(compositor->traverse_state, 0, sizeof(GF_TraverseState));

	compositor->traverse_state->vrml_sensors = gf_list_new();
	compositor->traverse_state->use_stack = gf_list_new();
#ifndef GPAC_DISABLE_3D
	compositor->traverse_state->local_lights = gf_list_new();
#endif

	gf_mx2d_init(compositor->traverse_state->transform);
	gf_cmx_init(&compositor->traverse_state->color_mat);
	compositor->traverse_state->immediate_draw = mode2d;

#ifdef GF_SR_USE_DEPTH
	compositor->traverse_state->depth_gain = FIX_ONE;
	compositor->traverse_state->depth_offset = 0;
#endif

#ifndef GPAC_DISABLE_3D
	//force a recompute of the canvas
	if (has_scene && compositor->hybgl_txh) {
		compositor->hybgl_txh->width = compositor->hybgl_txh->height = 0;
	}
#endif

	assert(!compositor->visual->overlays);

	compositor->reset_graphics = 0;
	compositor->trans_x = compositor->trans_y = 0;
	compositor->zoom = FIX_ONE;
	compositor->grab_node = NULL;
	compositor->grab_use = NULL;
	compositor->focus_node = NULL;
	compositor->focus_text_type = 0;
	if (compositor->video_memory!=2)
		compositor->video_memory = compositor->was_system_memory ? 0 : 1;
	compositor->rotation = 0;

	gf_list_reset(compositor->focus_ancestors);
	gf_list_reset(compositor->focus_use_stack);

#ifdef GF_SR_USE_VIDEO_CACHE
	gf_list_reset(compositor->cached_groups);
	compositor->video_cache_current_size = 0;
	gf_list_reset(compositor->cached_groups_queue);
#endif

	/*force resetup in case we're switching coord system*/
	compositor->root_visual_setup = 0;
	compositor_set_ar_scale(compositor, compositor->scale_x, compositor->scale_x);
}

GF_Err gf_sc_set_scene(GF_Compositor *compositor, GF_SceneGraph *scene_graph)
{
	u32 width, height;
	Bool do_notif;

	if (!compositor) return GF_BAD_PARAM;

	gf_sc_lock(compositor, 1);
	if (scene_graph && !compositor->scene) {
		GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Attaching new scene\n"));
	} else if (!scene_graph && compositor->scene) {
		GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Detaching scene\n"));
	}

	if (compositor->audio_renderer && (compositor->scene != scene_graph)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] resetting audio compositor\n"));
		gf_sc_ar_reset(compositor->audio_renderer);
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] resetting event queue\n"));
	gf_mx_p(compositor->evq_mx);
	while (gf_list_count(compositor->event_queue)) {
		GF_QueuedEvent *qev = (GF_QueuedEvent*)gf_list_get(compositor->event_queue, 0);
		gf_list_rem(compositor->event_queue, 0);
		gf_free(qev);
	}
	gf_mx_v(compositor->evq_mx);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] resetting compositor module\n"));
	/*reset main surface*/
	gf_sc_reset(compositor, scene_graph ? 1 : 0);

	/*set current graph*/
	compositor->scene = scene_graph;
	do_notif = GF_FALSE;
	if (scene_graph) {
		GF_Scene *scene_ctx = gf_sg_get_private(scene_graph);
#ifndef GPAC_DISABLE_SVG
		SVG_Length *w, *h;
		SVG_ViewBox *vb;
		Bool is_svg = GF_FALSE;
		u32 tag;
		GF_Node *top_node;
#endif
		Bool had_size_info = compositor->has_size_info;

		compositor->timed_nodes_valid = GF_TRUE;
		if (scene_ctx && scene_ctx->is_dynamic_scene)
			compositor->timed_nodes_valid = GF_FALSE;

		/*get pixel size if any*/
		gf_sg_get_scene_size_info(compositor->scene, &width, &height);
		compositor->has_size_info = (width && height) ? 1 : 0;
		if (compositor->has_size_info != had_size_info) compositor->scene_width = compositor->scene_height = 0;

		if (compositor->video_memory!=2)
			compositor->video_memory = gf_scene_is_dynamic_scene(scene_graph);

#ifndef GPAC_DISABLE_3D
		compositor->visual->camera.world_bbox.is_set = 0;
#endif

		/*default back color is black*/
		if (! (compositor->init_flags & GF_VOUT_WINDOWLESS)) {
			if (compositor->bc) {
				compositor->back_color = compositor->bc;
			} else {
				compositor->back_color = 0xFF000000;
			}
		}

#ifndef GPAC_DISABLE_SVG
		top_node = gf_sg_get_root_node(compositor->scene);
		tag = 0;
		if (top_node) tag = gf_node_get_tag(top_node);

		w = h = NULL;
		vb = NULL;
		if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
			GF_FieldInfo info;
			is_svg = 1;
			if (gf_node_get_attribute_by_tag(top_node, TAG_SVG_ATT_width, 0, 0, &info)==GF_OK)
				w = info.far_ptr;
			if (gf_node_get_attribute_by_tag(top_node, TAG_SVG_ATT_height, 0, 0, &info)==GF_OK)
				h = info.far_ptr;
			if (gf_node_get_attribute_by_tag(top_node, TAG_SVG_ATT_viewBox, 0, 0, &info)==GF_OK)
				vb = info.far_ptr;
		}
		/*default back color is white*/
		if (is_svg && ! (compositor->init_flags & GF_VOUT_WINDOWLESS)) compositor->back_color = 0xFFFFFFFF;

		/*hack for SVG where size is set in % - negotiate a canvas size*/
		if (!compositor->has_size_info && w && h && vb) {
			do_notif = 1;
			if (w->type!=SVG_NUMBER_PERCENTAGE) {
				width = FIX2INT(gf_sc_svg_convert_length_to_display(compositor, w) );
			} else if ((u32) FIX2INT(vb->width)<compositor->video_out->max_screen_width)  {
				width = FIX2INT(vb->width);
			} else {
				width = SC_DEF_WIDTH;
				do_notif = 0;
			}
			if (h->type!=SVG_NUMBER_PERCENTAGE) {
				height = FIX2INT(gf_sc_svg_convert_length_to_display(compositor, h) );
			} else if ((u32) FIX2INT(vb->height)<compositor->video_out->max_screen_height)  {
				height = FIX2INT(vb->height);
			} else {
				height = SC_DEF_HEIGHT;
				do_notif = 0;
			}
		}
		/*we consider that SVG has no size onfo per say, everything is handled by the viewBox if any*/
		if (is_svg) {
			compositor->has_size_info = 0;
			gf_sc_focus_switch_ring(compositor, 0, NULL, 0);
		} else
#endif
		{
			GF_Node *keynav = gf_scene_get_keynav(compositor->scene, NULL);
			if (keynav) gf_sc_change_key_navigator(compositor, keynav);
		}

		/*default back color is key color*/
		if (compositor->init_flags & GF_VOUT_WINDOWLESS) {
			if (compositor->ckey) compositor->back_color = compositor->ckey;
		}

		/*set scene size only if different, otherwise keep scaling/FS*/
		if ( !width || (compositor->scene_width!=width) || !height || (compositor->scene_height!=height)) {
			do_notif = do_notif || compositor->has_size_info || (!compositor->scene_width && !compositor->scene_height);
			gf_sc_set_scene_size(compositor, width, height, 0);

			/*get actual size in pixels*/
			width = compositor->scene_width;
			height = compositor->scene_height;

			if (!compositor->os_wnd) {
				/*only notify user if we are attached to a window*/
				//do_notif = 0;
				if (compositor->video_out->max_screen_width && (width > compositor->video_out->max_screen_width)) {
					height *= compositor->video_out->max_screen_width;
					height /= width;
					width = compositor->video_out->max_screen_width;
				}
				if (compositor->video_out->max_screen_height && (height > compositor->video_out->max_screen_height)) {
					width *= compositor->video_out->max_screen_height;
					width /= height;
					height = compositor->video_out->max_screen_height;
				}

				gf_sc_set_size(compositor,width, height);
			}
		}
	} else {
		gf_sc_ar_reset(compositor->audio_renderer);
		compositor->needs_offscreen_gl = GF_FALSE;
	}

	gf_sc_reset_framerate(compositor);

	gf_sc_lock(compositor, 0);
	if (scene_graph)
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);

	/*here's a nasty trick: the app may respond to this by calling a gf_sc_set_size from a different
	thread, but in an atomic way (typically happen on Win32 when changing the window size). WE MUST
	NOTIFY THE SIZE CHANGE AFTER RELEASING THE RENDERER MUTEX*/
	if (do_notif) {
		GF_Event evt;
		/*wait for user ack*/
//		gf_sc_next_frame_state(compositor, GF_SC_DRAW_NONE);

		evt.type = GF_EVENT_SCENE_SIZE;
		evt.size.width = width;
		evt.size.height = height;
		gf_sc_send_event(compositor, &evt);
	}
	return GF_OK;
}

GF_EXPORT
void gf_sc_lock(GF_Compositor *compositor, Bool doLock)
{
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Compositor] Thread ID %d is %s the scene\n", gf_th_id(), doLock ? "locking" : "unlocking" ));
	if (doLock)
		gf_mx_p(compositor->mx);
	else {
		gf_mx_v(compositor->mx);
	}
}

GF_EXPORT
GF_Err gf_sc_set_size(GF_Compositor *compositor, u32 NewWidth, u32 NewHeight)
{
	Bool lock_ok;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("sc_set_size %dx%d\n", NewWidth, NewHeight));
	if (compositor->osize.x && compositor->osize.y) {
		NewWidth = compositor->osize.x;
		NewHeight = compositor->osize.y;
	}

	if ((compositor->display_width == NewWidth) && (compositor->display_height == NewHeight))
		return GF_OK;

	if (!NewWidth || !NewHeight) {
		compositor->override_size_flags &= ~2;
		compositor->msg_type |= GF_SR_CFG_AR;
		return GF_OK;
	}
	/*EXTRA CARE HERE: the caller (user app) is likely a different thread than the compositor one, and depending on window
	manager we may get called here as a result of a message sent to user but not yet returned */
	lock_ok = gf_mx_try_lock(compositor->mx);

	compositor->new_width = NewWidth;
	compositor->new_height = NewHeight;
	compositor->msg_type |= GF_SR_CFG_SET_SIZE;
	assert(compositor->new_width);

	/*if same size only request for video setup */
	compositor->msg_type &= ~GF_SR_CFG_WINDOWSIZE_NOTIF;
	if ((compositor->display_width == NewWidth) && (compositor->display_height == NewHeight) ) {
		compositor->msg_type |= GF_SR_CFG_WINDOWSIZE_NOTIF;
	}
	if (lock_ok) gf_sc_lock(compositor, 0);

	//forward scene size event before actual resize in case the user cancels the resize
	{
		GF_Event evt;
		evt.type = GF_EVENT_SCENE_SIZE;
		evt.size.width = NewWidth;
		evt.size.height = NewHeight;
		gf_sc_send_event(compositor, &evt);
	}

	return GF_OK;
}

GF_EXPORT
void gf_sc_reload_config(GF_Compositor *compositor)
{
	/*changing drivers needs exclusive access*/
	gf_sc_lock(compositor, 1);

	gf_sc_set_fps(compositor, compositor->fps);

	if (compositor->fsize)
		compositor->override_size_flags |= 1;
	else
		compositor->override_size_flags &= ~1;


#ifndef GPAC_DISABLE_3D

	if (! (compositor->video_out->hw_caps & GF_VIDEO_HW_OPENGL)) {
		if (compositor->player && (compositor->ogl > GF_SC_GLMODE_OFF)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] OpenGL mode requested but no opengl-capable output - disabling OpenGL\n"));
		}
		compositor->force_opengl_2d = 0;
		compositor->autoconfig_opengl = 0;
		compositor->hybrid_opengl = 0;
	} else {
		compositor->force_opengl_2d = (compositor->ogl==GF_SC_GLMODE_ON) ? 1 : 0;
		if (compositor->ogl == GF_SC_GLMODE_AUTO) {
			compositor->recompute_ar = 1;
			compositor->autoconfig_opengl = 1;
		} else {
			compositor->hybrid_opengl = (compositor->ogl==GF_SC_GLMODE_HYBRID) ? 1 : 0;
		}
	}

#ifdef GPAC_USE_GLES1X
	compositor->linegl = 1;
	compositor->epow2 = 1;
#endif

	compositor->visual->nb_views = compositor->nbviews;
	compositor->visual->camlay = compositor->camlay;
	compositor->visual->autostereo_type = compositor->stereo;
	if (compositor->visual->autostereo_type == GF_3D_STEREO_5VSP19) {
		if (compositor->visual->nb_views != 5) {
			if (compositor->visual->nb_views) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] SPV19 interleaving used but only %d views indicated - adjusting to 5 view\n", compositor->visual->nb_views ));
			}
			compositor->visual->nb_views = 5;
		}
	}
	else if (compositor->visual->autostereo_type == GF_3D_STEREO_8VALIO) {
		if (compositor->visual->nb_views != 8) {
			if (compositor->visual->nb_views) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] ALIO interleaving used but only %d views indicated - adjusting to 8 view\n", compositor->visual->nb_views ));
				compositor->visual->nb_views = 8;
			}
		}
	}

	if ((compositor->visual->autostereo_type!=GF_3D_STEREO_NONE) && (compositor->visual->autostereo_type <= GF_3D_STEREO_HEADSET)) {
		compositor->visual->nb_views = 2;
	}

	if (compositor->visual->autostereo_type)
		compositor->force_opengl_2d = 1;

	switch (compositor->visual->autostereo_type) {
	case GF_3D_STEREO_ANAGLYPH:
	case GF_3D_STEREO_COLUMNS:
	case GF_3D_STEREO_ROWS:
		if (compositor->visual->nb_views != 2) {
			if (compositor->visual->nb_views) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] Stereo interleaving used but %d views indicated - adjusting to 2 view\n", compositor->visual->nb_views ));
			}
			compositor->visual->nb_views = 2;
		}
		break;
	}
	if (!compositor->visual->nb_views) compositor->visual->nb_views = 1;

#endif //GPAC_DISABLE_3D

	/*load defer mode only once hybrid_opengl is known. If no hybrid OpenGL and no backbuffer 2D, disable defer rendering*/
	if (!compositor->hybrid_opengl && compositor->video_out->hw_caps & GF_VIDEO_HW_DIRECT_ONLY) {
		compositor->traverse_state->immediate_draw = 1;
	} else {
		if (compositor->mode2d==GF_DRAW_MODE_IMMEDIATE)
			compositor->traverse_state->immediate_draw = 1;
		else if (compositor->mode2d==GF_DRAW_MODE_DEFER_DEBUG) {
			compositor->traverse_state->immediate_draw = 0;
			compositor->debug_defer = 1;
		} else {
		 	compositor->traverse_state->immediate_draw = 0;
		}
	}


#ifdef GF_SR_USE_DEPTH

#ifndef GPAC_DISABLE_3D
	/*if auto-stereo mode, turn on display depth by default*/
	if (compositor->visual->autostereo_type && !compositor->dispdepth) {
		compositor->dispdepth = -1;
	}
#endif

#endif

	if (!compositor->video_out->dispdist) {
		compositor->video_out->dispdist = compositor->dispdist;
	}
	if (compositor->sgaze) compositor->gazer_enabled = GF_TRUE;

	if (!compositor->video_out->dpi_x) {
		compositor->video_out->dpi_x = compositor->dpi.x;
		compositor->video_out->dpi_y = compositor->dpi.y;
	}

	gf_sc_reset_graphics(compositor);
	gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);

	if (compositor->audio_renderer) {
		gf_sc_ar_set_volume(compositor->audio_renderer, compositor->avol);
		gf_sc_ar_set_pan(compositor->audio_renderer, compositor->apan);
	}
	gf_sc_lock(compositor, 0);
}

static void gf_sc_set_play_state_internal(GF_Compositor *compositor, u32 PlayState, Bool reset_audio, Bool pause_clocks);

GF_EXPORT
GF_Err gf_sc_set_option(GF_Compositor *compositor, GF_CompositorOption type, u32 value)
{
	GF_Err e;
	gf_sc_lock(compositor, 1);

	e = GF_OK;
	switch (type) {
	case GF_OPT_PLAY_STATE:
		gf_sc_set_play_state(compositor, value);
		break;
	case GF_OPT_AUDIO_VOLUME:
		gf_sc_ar_set_volume(compositor->audio_renderer, value);
		break;
	case GF_OPT_AUDIO_PAN:
		gf_sc_ar_set_pan(compositor->audio_renderer, value);
		break;
	case GF_OPT_AUDIO_MUTE:
		gf_sc_ar_mute(compositor->audio_renderer, value);
		break;
	case GF_OPT_OVERRIDE_SIZE:
		compositor->override_size_flags = value ? 1 : 0;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_STRESS_MODE:
		compositor->stress = value;
		break;
	case GF_OPT_ANTIALIAS:
		compositor->aa = value;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_HIGHSPEED:
		compositor->fast = value;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_DRAW_BOUNDS:
		compositor->bvol = value;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_TEXTURE_TEXT:
		compositor->textxt = value;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_ASPECT_RATIO:
		compositor->aspect_ratio = value;
		compositor->msg_type |= GF_SR_CFG_AR;
		break;
	case GF_OPT_INTERACTION_LEVEL:
		compositor->interaction_level = value;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_REFRESH:
		gf_sc_reset_graphics(compositor);
		compositor->traverse_state->invalidate_all = 1;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_FULLSCREEN:
		if (compositor->fullscreen != value) compositor->msg_type |= GF_SR_CFG_FULLSCREEN;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_ORIGINAL_VIEW:
		compositor_2d_set_user_transform(compositor, FIX_ONE, 0, 0, 0);
		gf_sc_set_size(compositor, compositor->scene_width, compositor->scene_height);
		break;
	case GF_OPT_VISIBLE:
		compositor->is_hidden = !value;
		if (compositor->video_out->ProcessEvent) {
			GF_Event evt;
			evt.type = GF_EVENT_SHOWHIDE;
			evt.show.show_type = value ? 1 : 0;
			e = compositor->video_out->ProcessEvent(compositor->video_out, &evt);
		}
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_FREEZE_DISPLAY:
		compositor->freeze_display = value;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_FORCE_AUDIO_CONFIG:
		if (value) {
			compositor->audio_renderer->config_forced++;
		} else if (compositor->audio_renderer->config_forced) {
			compositor->audio_renderer->config_forced --;
		}
		break;
	case GF_OPT_RELOAD_CONFIG:
		gf_sc_reload_config(compositor);
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_DRAW_MODE:
		if (!(compositor->video_out->hw_caps & GF_VIDEO_HW_DIRECT_ONLY)) {
			compositor->traverse_state->immediate_draw = (value==GF_DRAW_MODE_IMMEDIATE) ? 1 : 0;
			compositor->debug_defer = (value==GF_DRAW_MODE_DEFER_DEBUG) ? 1 : 0;
			/*force redraw*/
			gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		}
		break;
	case GF_OPT_SCALABLE_ZOOM:
		compositor->sz = value;
		/*emulate size message to force AR recompute*/
		compositor->msg_type |= GF_SR_CFG_AR;
		break;
	case GF_OPT_USE_OPENGL:
		if (compositor->force_opengl_2d != value) {
			compositor->force_opengl_2d = value;
			/*force resetup*/
			compositor->root_visual_setup = 0;
			/*force texture setup when switching to OpenGL*/
			if (value) gf_sc_reset_graphics(compositor);
			/*force redraw*/
			gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		}
		break;
	case GF_OPT_VIDEO_BENCH:
		compositor->bench_mode = value ? GF_TRUE : GF_FALSE;
		break;

	case GF_OPT_YUV_HARDWARE:
		compositor->yuvhw = value;
		break;
	case GF_OPT_NAVIGATION_TYPE:
		compositor->rotation = 0;
		compositor_2d_set_user_transform(compositor, FIX_ONE, 0, 0, 0);
#ifndef GPAC_DISABLE_3D
		compositor_3d_reset_camera(compositor);
#endif
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_NAVIGATION:
		if (! gf_sc_navigation_supported(compositor, value)) {
			e = GF_NOT_SUPPORTED;
		}
#ifndef GPAC_DISABLE_3D
		else if (compositor->visual->type_3d || compositor->active_layer) {
			GF_Camera *cam = compositor_3d_get_camera(compositor);
			cam->navigate_mode = value;
		}
#endif
		else {
			compositor->navigate_mode = value;
		}
		break;

	case GF_OPT_HEADLIGHT:
#ifndef GPAC_DISABLE_3D
		if (compositor->visual->type_3d || compositor->active_layer) {
			GF_Camera *cam = compositor_3d_get_camera(compositor);
			if (cam->navigation_flags & NAV_ANY) {
				if (value)
					cam->navigation_flags |= NAV_HEADLIGHT;
				else
					cam->navigation_flags &= ~NAV_HEADLIGHT;
				break;
			}
		}
#endif
		e = GF_NOT_SUPPORTED;
		break;

#ifndef GPAC_DISABLE_3D

	case GF_OPT_EMULATE_POW2:
		compositor->epow2 = value;
		break;
	case GF_OPT_POLYGON_ANTIALIAS:
		compositor->paa = value;
		break;
	case GF_OPT_BACK_CULL:
		compositor->bcull = value;
		break;
	case GF_OPT_WIREFRAME:
		compositor->wire = value;
		break;
	case GF_OPT_NORMALS:
		compositor->norms = value;
		break;
#ifdef GPAC_HAS_GLU
	case GF_OPT_RASTER_OUTLINES:
		compositor->linegl = value;
		break;
#endif

	case GF_OPT_NO_RECT_TEXTURE:
		if (value != compositor->rext) {
			compositor->rext = value;
			/*RECT texture support - we must reload HW*/
			gf_sc_reset_graphics(compositor);
		}
		break;
	case GF_OPT_COLLISION:
		compositor->collide_mode = value;
		break;
	case GF_OPT_GRAVITY:
	{
		GF_Camera *cam = compositor_3d_get_camera(compositor);
		compositor->gravity_on = value;
		/*force collision pass*/
		cam->last_pos.z -= 1;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
	}
	break;
#endif

	case GF_OPT_VIDEO_CACHE_SIZE:
#ifdef GF_SR_USE_VIDEO_CACHE
		compositor_set_cache_memory(compositor, 1024*value);
#else
		e = GF_NOT_SUPPORTED;
#endif
		break;
	case GF_OPT_MULTIVIEW_MODE:
		compositor->multiview_mode = value;
		break;

	case GF_OPT_HTTP_MAX_RATE:
	{
		GF_DownloadManager *dm = gf_filter_get_download_manager(compositor->filter);
		if (!dm) return GF_SERVICE_ERROR;
		gf_dm_set_data_rate(dm, value);
		e = GF_OK;
		break;
	}

	default:
		e = GF_BAD_PARAM;
		break;
	}
	gf_sc_lock(compositor, 0);
	return e;
}

GF_EXPORT
Bool gf_sc_is_over(GF_Compositor *compositor, GF_SceneGraph *scene_graph)
{
	u32 i, count;
	count = gf_list_count(compositor->time_nodes);
	for (i=0; i<count; i++) {
		GF_TimeNode *tn = (GF_TimeNode *)gf_list_get(compositor->time_nodes, i);
		if (tn->needs_unregister) continue;

		if (scene_graph && (gf_node_get_graph((GF_Node *)tn->udta) != scene_graph))
			continue;

		switch (gf_node_get_tag((GF_Node *)tn->udta)) {
#ifndef GPAC_DISABLE_VRML
		case TAG_MPEG4_TimeSensor:
#endif
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_TimeSensor:
#endif
			return 0;

#ifndef GPAC_DISABLE_VRML
		case TAG_MPEG4_MovieTexture:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_MovieTexture:
#endif
			if (((M_MovieTexture *)tn->udta)->loop) return 0;
			break;
		case TAG_MPEG4_AudioClip:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_AudioClip:
#endif
			if (((M_AudioClip*)tn->udta)->loop) return 0;
			break;
		case TAG_MPEG4_AnimationStream:
			if (((M_AnimationStream*)tn->udta)->loop) return 0;
			break;
#endif
		}
	}
	/*FIXME - this does not work with SVG/SMIL*/
	return 1;
}

GF_EXPORT
u32 gf_sc_get_option(GF_Compositor *compositor, GF_CompositorOption type)
{
	switch (type) {
	case GF_OPT_PLAY_STATE:
		return compositor->paused ? 1 : 0;
	case GF_OPT_OVERRIDE_SIZE:
		return (compositor->override_size_flags & 1) ? 1 : 0;
	case GF_OPT_IS_FINISHED:
		if (compositor->interaction_sensors) return 0;
	case GF_OPT_IS_OVER:
		return gf_sc_is_over(compositor, NULL);
	case GF_OPT_STRESS_MODE:
		return compositor->stress;
	case GF_OPT_AUDIO_VOLUME:
		return compositor->audio_renderer->volume;
	case GF_OPT_AUDIO_PAN:
		return compositor->audio_renderer->pan;
	case GF_OPT_AUDIO_MUTE:
		return compositor->audio_renderer->mute;

	case GF_OPT_ANTIALIAS:
		return compositor->aa;
	case GF_OPT_HIGHSPEED:
		return compositor->fast;
	case GF_OPT_ASPECT_RATIO:
		return compositor->aspect_ratio;
	case GF_OPT_FULLSCREEN:
		return compositor->fullscreen;
	case GF_OPT_INTERACTION_LEVEL:
		return compositor->interaction_level;
	case GF_OPT_VISIBLE:
		return !compositor->is_hidden;
	case GF_OPT_FREEZE_DISPLAY:
		return compositor->freeze_display;
	case GF_OPT_TEXTURE_TEXT:
		return compositor->textxt;
	case GF_OPT_USE_OPENGL:
		return compositor->force_opengl_2d;

	case GF_OPT_DRAW_MODE:
		if (compositor->traverse_state->immediate_draw) return GF_DRAW_MODE_IMMEDIATE;
		if (compositor->debug_defer) return GF_DRAW_MODE_DEFER_DEBUG;
		return GF_DRAW_MODE_DEFER;
	case GF_OPT_SCALABLE_ZOOM:
		return compositor->sz;
	case GF_OPT_YUV_HARDWARE:
		return compositor->yuvhw;
	case GF_OPT_YUV_FORMAT:
		return compositor->yuvhw ? compositor->video_out->yuv_pixel_format : 0;
	case GF_OPT_NAVIGATION_TYPE:
#ifndef GPAC_DISABLE_3D
		if (compositor->navigation_disabled) return GF_NAVIGATE_TYPE_NONE;
		if (compositor->visual->type_3d || compositor->active_layer) {
			GF_Camera *cam = compositor_3d_get_camera(compositor);
			if ((cam->navigation_flags & NAV_SELECTABLE)) return GF_NAVIGATE_TYPE_3D;
			if (!(cam->navigation_flags & NAV_ANY)) return GF_NAVIGATE_TYPE_NONE;
//			return ((cam->is_3D || compositor->active_layer) ? GF_NAVIGATE_TYPE_3D : GF_NAVIGATE_TYPE_2D);
			return GF_NAVIGATE_TYPE_3D;
		} else
#endif
		{
			return GF_NAVIGATE_TYPE_2D;
		}
	case GF_OPT_NAVIGATION:
#ifndef GPAC_DISABLE_3D
		if (compositor->visual->type_3d || compositor->active_layer) {
			GF_Camera *cam = compositor_3d_get_camera(compositor);
			return cam->navigate_mode;
		}
#endif
		return compositor->navigate_mode;

	case GF_OPT_HEADLIGHT:
		return 0;
	case GF_OPT_COLLISION:
		return GF_COLLISION_NONE;
	case GF_OPT_GRAVITY:
		return 0;

	case GF_OPT_VIDEO_CACHE_SIZE:
#ifdef GF_SR_USE_VIDEO_CACHE
		return compositor->vcsize/1024;
#else
		return 0;
#endif
		break;
	case GF_OPT_NUM_STEREO_VIEWS:
#ifndef GPAC_DISABLE_3D
		if (compositor->visual->type_3d) {
			if (compositor->visual->nb_views && compositor->visual->autostereo_type > GF_3D_STEREO_LAST_SINGLE_BUFFER)
				return compositor->visual->nb_views;
		}
#endif
		return 1;

	default:
		return 0;
	}
}

GF_EXPORT
void gf_sc_map_point(GF_Compositor *compositor, s32 X, s32 Y, Fixed *bifsX, Fixed *bifsY)
{
	/*coordinates are in user-like OS....*/
	X = X - compositor->display_width/2;
	Y = compositor->display_height/2 - Y;
	*bifsX = INT2FIX(X);
	*bifsY = INT2FIX(Y);
}


GF_EXPORT
GF_Err gf_sc_get_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer, u32 depth_dump_mode)
{
	GF_Err e;
	if (!compositor || !framebuffer) return GF_BAD_PARAM;
	gf_mx_p(compositor->mx);

#ifndef GPAC_DISABLE_3D
	if (compositor->visual->type_3d || compositor->hybrid_opengl)
		e = compositor_3d_get_screen_buffer(compositor, framebuffer, depth_dump_mode);
	else
#endif
		/*no depth dump in 2D mode*/
		if (depth_dump_mode) e = GF_NOT_SUPPORTED;
		else e = compositor->video_out->LockBackBuffer(compositor->video_out, framebuffer, 1);

	if (e != GF_OK) gf_mx_v(compositor->mx);
	return e;
}

GF_EXPORT
GF_Err gf_sc_get_offscreen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer, u32 view_idx, GF_CompositorGrabMode depth_dump_mode)
{
	if (!compositor || !framebuffer) return GF_BAD_PARAM;
#ifndef GPAC_DISABLE_3D
	if (compositor->visual->type_3d && compositor->visual->nb_views && (compositor->visual->autostereo_type > GF_3D_STEREO_LAST_SINGLE_BUFFER)) {
		GF_Err e;
		gf_mx_p(compositor->mx);
		e = compositor_3d_get_offscreen_buffer(compositor, framebuffer, view_idx, depth_dump_mode);
		if (e != GF_OK) gf_mx_v(compositor->mx);
		return e;
	}
#endif
	return GF_BAD_PARAM;
}


GF_EXPORT
GF_Err gf_sc_release_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer)
{
	GF_Err e;
	if (!compositor || !framebuffer) return GF_BAD_PARAM;

#ifndef GPAC_DISABLE_3D
	if (compositor->visual->type_3d || compositor->hybrid_opengl)
		e = compositor_3d_release_screen_buffer(compositor, framebuffer);
	else
#endif
		e = compositor->video_out->LockBackBuffer(compositor->video_out, framebuffer, 0);

	gf_mx_v(compositor->mx);
	return e;
}

GF_EXPORT
Double gf_sc_get_fps(GF_Compositor *compositor, Bool absoluteFPS)
{
	Double fps;
	u32 fidx, num, frames, run_time;

	gf_mx_p(compositor->mx);

	if (absoluteFPS) {
		/*start from last frame and get first frame time*/
		fidx = compositor->current_frame;
		frames = 0;
		run_time = compositor->frame_dur[fidx];
		for (num=0; num<GF_SR_FPS_COMPUTE_SIZE; num++) {
			run_time += compositor->frame_dur[fidx];
			frames++;
			if (frames==GF_SR_FPS_COMPUTE_SIZE) break;
			if (!fidx) {
				fidx = GF_SR_FPS_COMPUTE_SIZE-1;
			} else {
				fidx--;
			}
		}
	} else {
		/*start from last frame and get first frame time*/
		fidx = compositor->current_frame;
		run_time = compositor->frame_time[fidx];
		fidx = (fidx+1)% GF_SR_FPS_COMPUTE_SIZE;
		s64 diff = run_time;
		diff -= compositor->frame_time[fidx];
		if (diff<0) {
			diff += 0xFFFFFFFFUL;
			assert(diff >= 0);
		}
		run_time = (u32) diff;
		frames = GF_SR_FPS_COMPUTE_SIZE-1;
	}


	gf_mx_v(compositor->mx);

	if (!run_time) return ((Double) compositor->fps.num)/compositor->fps.den;
	fps = 1000*frames;
	fps /= run_time;
	return fps;
}

GF_EXPORT
void gf_sc_register_time_node(GF_Compositor *compositor, GF_TimeNode *tn)
{
	/*may happen with DEF/USE */
	if (tn->is_registered) return;
	if (tn->needs_unregister) return;
	gf_list_add(compositor->time_nodes, tn);
	tn->is_registered = 1;
}

GF_EXPORT
void gf_sc_unregister_time_node(GF_Compositor *compositor, GF_TimeNode *tn)
{
	gf_list_del_item(compositor->time_nodes, tn);
}

static void gf_sc_recompute_ar(GF_Compositor *compositor, GF_Node *top_node)
{
	Bool force_pause;

//	force_pause = compositor->audio_renderer->Frozen ? 0 : 1;
	force_pause = GF_FALSE;

#ifndef GPAC_DISABLE_LOG
	compositor->visual_config_time = 0;
#endif
	if (compositor->recompute_ar) {
#ifndef GPAC_DISABLE_3D
		u32 prev_type_3d = compositor->visual->type_3d;
#endif
#ifndef GPAC_DISABLE_LOG
		u32 time=0;

		if (gf_log_tool_level_on(GF_LOG_RTI, GF_LOG_DEBUG)) {
			time = gf_sys_clock();
		}
#endif
		if (force_pause)
			gf_sc_ar_control(compositor->audio_renderer, GF_SC_AR_PAUSE);

#ifndef GPAC_DISABLE_3D
		if (compositor->autoconfig_opengl) {
			compositor->visual->type_3d = 1;
		}
		if (compositor->visual->type_3d) {
			compositor_3d_set_aspect_ratio(compositor);
			gf_sc_load_opengl_extensions(compositor, compositor->visual->type_3d ? GF_TRUE : GF_FALSE);
#ifndef GPAC_USE_GLES1X
			visual_3d_init_shaders(compositor->visual);
#endif
			if (compositor->autoconfig_opengl) {
				u32 mode = compositor->ogl;
				compositor->autoconfig_opengl = 0;
				compositor->force_opengl_2d = 0;
				compositor->hybrid_opengl = GF_FALSE;
				compositor->visual->type_3d = prev_type_3d;

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
				//enable hybrid mode by default
				if (compositor->visual->compositor->shader_mode_disabled && (mode==GF_SC_GLMODE_HYBRID) ) {
					mode = GF_SC_GLMODE_OFF;
				}
#endif
				switch (mode) {
				case GF_SC_GLMODE_OFF:
					break;
				case GF_SC_GLMODE_ON:
					compositor->force_opengl_2d = 1;
					if (!compositor->visual->type_3d)
						compositor->visual->type_3d = 1;
					break;
				case GF_SC_GLMODE_AUTO:
				case GF_SC_GLMODE_HYBRID:
					compositor->hybrid_opengl = GF_TRUE;
					break;
				}
			}

		}
		if (!compositor->visual->type_3d)
#endif
		{
			compositor_2d_set_aspect_ratio(compositor);
#ifndef GPAC_DISABLE_3D
			if (compositor->hybrid_opengl) {
				gf_sc_load_opengl_extensions(compositor, GF_TRUE);
#ifndef GPAC_USE_GLES1X
				visual_3d_init_shaders(compositor->visual);
#endif
				if (!compositor->visual->hybgl_drawn.list) {
					ra_init(&compositor->visual->hybgl_drawn);
				}
			}
#endif
		}

		if (force_pause )
			gf_sc_ar_control(compositor->audio_renderer, GF_SC_AR_RESUME);

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_RTI, GF_LOG_DEBUG)) {
			compositor->visual_config_time = gf_sys_clock() - time;
		}
#endif

#ifndef GPAC_DISABLE_VRML
		compositor_evaluate_envtests(compositor, 0);
#endif
		//fullscreen was postponed, retry now that the AR has been recomputed
		if (compositor->fullscreen_postponed) {
			compositor->fullscreen_postponed = 0;
			compositor->msg_type |= GF_SR_CFG_FULLSCREEN;
		}

		if (compositor->vout) {
			gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_WIDTH, &PROP_UINT(compositor->output_width) );
			gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_HEIGHT, &PROP_UINT(compositor->output_height) );
			gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_FPS, &PROP_FRAC(compositor->fps) );
		}
	}
}



static void gf_sc_setup_root_visual(GF_Compositor *compositor, GF_Node *top_node)
{
	if (top_node && !compositor->root_visual_setup) {
		GF_SceneGraph *scene = compositor->scene;
		u32 node_tag;
#ifndef GPAC_DISABLE_3D
		Bool force_navigate=0;
		Bool was_3d = compositor->visual->type_3d;
#endif
		compositor->root_visual_setup = 1;
		compositor->visual->center_coords = 1;

		compositor->traverse_state->pixel_metrics = 1;
		compositor->traverse_state->min_hsize = INT2FIX(MIN(compositor->scene_width, compositor->scene_height)) / 2;

		node_tag = gf_node_get_tag(top_node);
		switch (node_tag) {

#ifndef GPAC_DISABLE_VRML
		case TAG_MPEG4_OrderedGroup:
		case TAG_MPEG4_Layer2D:
#ifndef GPAC_DISABLE_3D
			/*move to perspective 3D when simulating depth*/
#ifdef GF_SR_USE_DEPTH
			if (compositor->dispdepth) {
				compositor->visual->type_3d = 0;
				compositor->visual->camera.is_3D = 0;
			} else
#endif
			{
				compositor->visual->type_3d = 0;
				compositor->visual->camera.is_3D = 0;
			}
#endif
			compositor->traverse_state->pixel_metrics = gf_sg_use_pixel_metrics(scene);
			break;
		case TAG_MPEG4_Group:
		case TAG_MPEG4_Layer3D:
#ifndef GPAC_DISABLE_3D
			compositor->visual->type_3d = 2;
			compositor->visual->camera.is_3D = 1;
#endif
			compositor->traverse_state->pixel_metrics = gf_sg_use_pixel_metrics(scene);
			break;
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Group:
#ifndef GPAC_DISABLE_3D
			compositor->visual->type_3d = 3;
#endif
			compositor->traverse_state->pixel_metrics = gf_sg_use_pixel_metrics(scene);
			break;
#endif /*GPAC_DISABLE_X3D*/



#endif /*GPAC_DISABLE_VRML*/

#ifndef GPAC_DISABLE_SVG
		case TAG_SVG_svg:
#ifndef GPAC_DISABLE_3D
#ifdef GF_SR_USE_DEPTH
			if (compositor->dispdepth>=0) {
				compositor->visual->type_3d = 2;
				compositor->visual->camera.is_3D = 1;
			} else
#endif
			{
				compositor->visual->type_3d = 0;
				compositor->visual->camera.is_3D = 0;
			}
#endif
			compositor->visual->center_coords = 0;
			compositor->root_visual_setup = 2;
			break;
#endif /*GPAC_DISABLE_SVG*/
		}

		/*!! by default we don't set the focus on the content - this is conform to SVG and avoids displaying the
		focus box for MPEG-4 !! */

		/*setup OpenGL & camera mode*/
#ifndef GPAC_DISABLE_3D
		if (compositor->inherit_type_3d && !compositor->visual->type_3d) {
			compositor->visual->type_3d = 2;
			compositor->visual->camera.is_3D = 1;
		}
		/*request for OpenGL drawing in 2D*/
		else if ( (compositor->force_opengl_2d && !compositor->visual->type_3d)
		|| (compositor->hybrid_opengl && compositor->force_type_3d)) {

			compositor->force_type_3d=0;
			compositor->visual->type_3d = 1;
			if (compositor->force_opengl_2d==2) force_navigate=1;
		}

		if (compositor->drv==GF_SC_DRV_AUTO)
			gf_sc_check_video_driver(compositor);

		if (! (compositor->video_out->hw_caps & GF_VIDEO_HW_OPENGL)) {
			compositor->visual->type_3d = 0;
			compositor->visual->camera.is_3D = 0;
		}
		compositor->visual->camera.is_3D = (compositor->visual->type_3d>1) ? 1 : 0;
		if (!compositor->visual->camera.is_3D)
			camera_set_2d(&compositor->visual->camera);

		camera_invalidate(&compositor->visual->camera);
		if (force_navigate) {
			compositor->visual->camera.navigate_mode = GF_NAVIGATE_EXAMINE;
			compositor->visual->camera.had_nav_info = 0;
		}
#endif

		GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Main scene setup - pixel metrics %d - center coords %d\n", compositor->traverse_state->pixel_metrics, compositor->visual->center_coords));

		compositor->recompute_ar = 1;
#ifndef GPAC_DISABLE_3D
		/*change in 2D/3D config, force AR recompute/video restup*/
		if (was_3d != compositor->visual->type_3d) compositor->recompute_ar = was_3d ? 1 : 2;
#endif
	}
}


static Bool gf_sc_draw_scene(GF_Compositor *compositor)
{
	u32 flags;

	GF_Node *top_node = gf_sg_get_root_node(compositor->scene);

	if (!top_node && !compositor->visual->last_had_back && !compositor->visual->cur_context) {
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Scene has no root node, nothing to draw\n"));
		//nothing to draw, skip flush
		compositor->skip_flush = 1;
		return GF_FALSE;
	}

#ifdef GF_SR_USE_VIDEO_CACHE
	if (!compositor->vcsize)
		compositor->traverse_state->in_group_cache = 1;
#endif

	flags = compositor->traverse_state->immediate_draw;
	if (compositor->video_setup_failed) {
		compositor->skip_flush = 1;
	}
	else {
		if (compositor->nodes_pending) {
			u32 i, count, n_count;
			i=0;
			count = gf_list_count(compositor->nodes_pending);
			while (i<count) {
				GF_Node *n = (GF_Node *)gf_list_get(compositor->nodes_pending, i);
				gf_node_traverse(n, NULL);
				if (!compositor->nodes_pending) break;
				n_count = gf_list_count(compositor->nodes_pending);
				if (n_count==count) i++;
				else count=n_count;
			}
		}
		if (compositor->passthrough_txh) {
			gf_sc_setup_passthrough(compositor);
			compositor->traverse_state->immediate_draw = 1;
		}

		if (! visual_draw_frame(compositor->visual, top_node, compositor->traverse_state, 1)) {
			/*android backend uses opengl without telling it to us, we need an ugly hack here ...*/
#ifdef GPAC_CONFIG_ANDROID
			compositor->skip_flush = 0;
#else
			if (compositor->skip_flush==2) {
				compositor->skip_flush = 0;
			} else {
				compositor->skip_flush = 1;
			}
#endif
		}

	}


	compositor->traverse_state->immediate_draw = flags;
#ifdef GF_SR_USE_VIDEO_CACHE
	gf_list_reset(compositor->cached_groups_queue);
#endif
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Frame %d - drawing done\n", compositor->frame_number));

	/*only send the resize notification once the frame has been dra*/
	if (compositor->recompute_ar) {
		compositor_send_resize_event(compositor, NULL, 0, 0, 0, 1);
		compositor->recompute_ar = 0;
	}
	compositor->zoom_changed = 0;
	compositor->audio_renderer->scene_ready = GF_TRUE;
	return GF_TRUE;
}


#ifndef GPAC_DISABLE_LOG
//defined in smil_anim ...
extern u32 time_spent_in_anim;
#endif

static void compositor_release_textures(GF_Compositor *compositor, Bool frame_drawn)
{
	u32 i, count;
	/*release all textures - we must release them to handle a same OD being used by several textures*/
	count = gf_list_count(compositor->textures);
	for (i=0; i<count; i++) {
		GF_TextureHandler *txh = (GF_TextureHandler *)gf_list_get(compositor->textures, i);
		gf_sc_texture_release_stream(txh);
		if (frame_drawn && txh->tx_io && !(txh->flags & GF_SR_TEXTURE_USED))
			gf_sc_texture_reset(txh);
		/*remove the use flag*/
		txh->flags &= ~GF_SR_TEXTURE_USED;
	}
}

GF_EXPORT
void gf_sc_invalidate_next_frame(GF_Compositor *compositor)
{
	if (compositor) {
		//force frame draw type
		compositor->frame_draw_type = GF_SC_DRAW_FRAME;
		//reset frame produced flag and run until it is set (backbuffer ready to be swapped)
		compositor->frame_was_produced = GF_FALSE;
	}
}

GF_EXPORT
Bool gf_sc_frame_was_produced(GF_Compositor *compositor)
{
	return compositor ? compositor->frame_was_produced : GF_TRUE;
}


void gf_sc_render_frame(GF_Compositor *compositor)
{
#ifndef GPAC_DISABLE_SCENEGRAPH
	GF_SceneGraph *sg;
#endif
	GF_List *temp_queue;
	u32 in_time, end_time, i, count, frame_duration, frame_ts, frame_draw_type_bck;
	Bool frame_drawn, has_timed_nodes=GF_FALSE, has_tx_streams=GF_FALSE, all_tx_done=GF_TRUE;

#ifndef GPAC_DISABLE_LOG
	s32 event_time, route_time, smil_timing_time=0, time_node_time, texture_time, traverse_time, flush_time, txtime;
#endif

	/*lock compositor for the whole cycle*/
	gf_sc_lock(compositor, 1);
//	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Entering render_frame \n"));

	in_time = gf_sys_clock();

	gf_sc_texture_cleanup_hw(compositor);

	compositor->frame_was_produced = GF_FALSE;
	/*first thing to do, let the video output handle user event if it is not threaded*/
	compositor->video_out->ProcessEvent(compositor->video_out, NULL);

	//handle all unthreaded extensions
	if (compositor->unthreaded_extensions) {
		count = gf_list_count(compositor->unthreaded_extensions);
		for (i=0; i<count; i++) {
			GF_CompositorExt *ifce = gf_list_get(compositor->unthreaded_extensions, i);
			ifce->process(ifce, GF_COMPOSITOR_EXT_PROCESS, NULL);
		}
	}

	if (compositor->freeze_display) {
		gf_sc_lock(compositor, 0);
		if (!compositor->bench_mode && compositor->player) {
			compositor->scene_sampled_clock = gf_sc_ar_get_clock(compositor->audio_renderer);
		}
		return;
	}

	gf_sc_reconfig_task(compositor);

	/* if there is no scene*/
	if (!compositor->scene && !gf_list_count(compositor->extra_scenes) ) {
		gf_sc_draw_scene(compositor);
		if (compositor->player) {
			//increase clock in bench mode before releasing mutex
			if (compositor->bench_mode && (compositor->force_bench_frame==1)) {
				compositor->scene_sampled_clock += compositor->frame_duration;
			}
		} else {
			if (compositor->root_scene && compositor->root_scene->graph_attached) {
				compositor->check_eos_state = 2;
			}
		}
		gf_sc_lock(compositor, 0);
		compositor->force_bench_frame=0;
		compositor->frame_draw_type = 0;
		compositor->recompute_ar = 0;
		compositor->ms_until_next_frame = compositor->frame_duration;
		return;
	}

	if (compositor->reset_graphics) {
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		visual_reset_graphics(compositor->visual);

#ifndef GPAC_DISABLE_3D
		compositor_3d_delete_fbo(&compositor->fbo_id, &compositor->fbo_tx_id, &compositor->fbo_depth_id, compositor->external_tx_id);
#endif

	}

	/*process pending user events*/
#ifndef GPAC_DISABLE_LOG
	event_time = gf_sys_clock();
#endif
	//swap event queus
	gf_mx_p(compositor->evq_mx);
	temp_queue = compositor->event_queue;
	compositor->event_queue = compositor->event_queue_back;
	compositor->event_queue_back = temp_queue;
	gf_mx_v(compositor->evq_mx);
	while (gf_list_count(compositor->event_queue_back)) {
		GF_QueuedEvent *qev = (GF_QueuedEvent*)gf_list_get(compositor->event_queue_back, 0);
		gf_list_rem(compositor->event_queue_back, 0);

		if (qev->target) {
#ifndef GPAC_DISABLE_SVG
			gf_sg_fire_dom_event(qev->target, &qev->dom_evt, qev->sg, NULL);
#endif
		} else if (qev->node) {
#ifndef GPAC_DISABLE_SVG
			gf_dom_event_fire(qev->node, &qev->dom_evt);
#endif
		} else {
			gf_sc_exec_event(compositor, &qev->evt);
		}
		gf_free(qev);
	}
#ifndef GPAC_DISABLE_LOG
	event_time = gf_sys_clock() - event_time;
#endif

	if (compositor->player) {
		if (!compositor->bench_mode) {
			compositor->scene_sampled_clock = gf_sc_ar_get_clock(compositor->audio_renderer);
		} else {
			if (compositor->force_bench_frame==1) {
				//a system frame is pending on a future frame - we must increase our time
				compositor->scene_sampled_clock += compositor->frame_duration;
			}
			compositor->force_bench_frame = 0;
		}
	}


	//first update all natural textures to figure out timing
	compositor->frame_delay = 0;
	compositor->ms_until_next_frame = GF_INT_MAX;
	frame_duration = compositor->frame_duration;

	if (compositor->auto_rotate)
		compositor_handle_auto_navigation(compositor);

	/*first update time nodes since they may trigger play/stop request*/

	compositor->force_late_frame_draw = GF_FALSE;

#ifndef GPAC_DISABLE_SVG

#ifndef GPAC_DISABLE_LOG
	smil_timing_time = gf_sys_clock();
#endif
	if (gf_smil_notify_timed_elements(compositor->scene)) {
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
	}
#ifndef GPAC_DISABLE_SCENEGRAPH
	i = 0;
	while ((sg = (GF_SceneGraph*)gf_list_enum(compositor->extra_scenes, &i))) {
		if (gf_smil_notify_timed_elements(sg)) {
			gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		}
	}
#endif

#ifndef GPAC_DISABLE_LOG
	smil_timing_time = gf_sys_clock() - smil_timing_time;
#endif

#endif //GPAC_DISABLE_SVG


#ifndef GPAC_DISABLE_LOG
	time_node_time = gf_sys_clock();
#endif
	/*update all timed nodes */
	count = gf_list_count(compositor->time_nodes);
	for (i=0; i<count; i++) {
		GF_TimeNode *tn = (GF_TimeNode *)gf_list_get(compositor->time_nodes, i);
		if (!tn->needs_unregister) tn->UpdateTimeNode(tn);
		if (tn->needs_unregister) {
			tn->is_registered = 0;
			tn->needs_unregister = 0;
			gf_list_rem(compositor->time_nodes, i);
			i--;
			count--;
			continue;
		}
		has_timed_nodes = compositor->timed_nodes_valid;
	}
#ifndef GPAC_DISABLE_LOG
	time_node_time = gf_sys_clock() - time_node_time;
#endif


	compositor->passthrough_txh = NULL;
#ifndef GPAC_DISABLE_LOG
	texture_time = gf_sys_clock();
#endif
	//compute earliest frame TS in all textures that need refresh (skip textures with same timing)
	frame_ts = (u32) -1;

	/*update all video textures*/
	frame_draw_type_bck = compositor->frame_draw_type;
	compositor->frame_draw_type = 0;
	has_tx_streams = GF_FALSE;
	count = gf_list_count(compositor->textures);
	for (i=0; i<count; i++) {
		GF_TextureHandler *txh = (GF_TextureHandler *)gf_list_get(compositor->textures, i);
		if (!txh) break;
		//this is not a natural (video) texture
		if (! txh->stream) continue;
		has_tx_streams = GF_TRUE;

		/*signal graphics reset before updating*/
		if (compositor->reset_graphics && txh->tx_io) gf_sc_texture_reset(txh);
		txh->update_texture_fcnt(txh);

		if (!txh->stream_finished && txh->is_open) {
			u32 d = gf_mo_get_min_frame_dur(txh->stream);
			if (d && (d < frame_duration)) frame_duration = d;
			//if the texture needs update (new frame), compute its timestamp in system timebase
			if (txh->needs_refresh) {
				u32 ts = gf_mo_map_timestamp_to_sys_clock(txh->stream, txh->stream->timestamp);
				if (ts<frame_ts) frame_ts = ts;
			}
			//VFR and one frame is pending
			else if (compositor->vfr && (compositor->ms_until_next_frame>0) && (compositor->ms_until_next_frame!=GF_INT_MAX)) {
				compositor->frame_draw_type = GF_SC_DRAW_FRAME;
			}
			all_tx_done=0;
		}
	}
#ifndef GPAC_DISABLE_LOG
	texture_time = gf_sys_clock() - texture_time;
#endif
	if (compositor->player) {
		if (frame_draw_type_bck)
			compositor->frame_draw_type = frame_draw_type_bck;
	}
	//in non player mode, don't redraw frame if due to end of streams on textures
	else if (!frame_draw_type_bck && compositor->frame_draw_type && all_tx_done)
		compositor->frame_draw_type = 0;


	if (!compositor->player) {
		if (compositor->passthrough_txh && compositor->passthrough_txh->frame_ifce && !compositor->passthrough_txh->frame_ifce->get_plane) {
			compositor->passthrough_txh = NULL;
		}
		if (!compositor->passthrough_txh && frame_draw_type_bck)
			compositor->frame_draw_type = frame_draw_type_bck;

		if (compositor->passthrough_txh && compositor->passthrough_txh->width && compositor->passthrough_txh->needs_refresh) {
			compositor->scene_sampled_clock = compositor->passthrough_txh->last_frame_time/* + dur*/;
		}
		//we were buffering and are now no longer, update scene clock
		if (compositor->passthrough_txh && compositor->passthrough_check_buffer && !gf_mo_is_buffering(compositor->passthrough_txh->stream)) {
			u32 dur = gf_mo_get_min_frame_dur(compositor->passthrough_txh->stream);
			compositor->passthrough_check_buffer = GF_FALSE;
			compositor->scene_sampled_clock = compositor->passthrough_txh->last_frame_time + dur;
		}
	}
	
	//it may happen that we have a reconfigure request at this stage, especially if updating one of the textures
	//forced a relayout - do it right away
	if (compositor->msg_type) {
		//release textures !
		compositor_release_textures(compositor, GF_FALSE);
		gf_sc_lock(compositor, 0);
		return;
	}


	if (compositor->focus_text_type) {
		if (!compositor->caret_next_draw_time) {
			compositor->caret_next_draw_time = gf_sys_clock();
			compositor->show_caret = 1;
		}
		if (compositor->caret_next_draw_time <= compositor->last_frame_time) {
			compositor->frame_draw_type = GF_SC_DRAW_FRAME;
			compositor->caret_next_draw_time+=500;
			compositor->show_caret = !compositor->show_caret;
			compositor->text_edit_changed = 1;
		}
	}


#ifndef GPAC_DISABLE_VRML
	/*execute all routes:
		before updating composite textures, otherwise nodes inside composite texture may never see their dirty flag set
		after updating timed nodes to trigger all events based on time
	*/
#ifndef GPAC_DISABLE_LOG
	route_time = gf_sys_clock();
#endif

	gf_sg_activate_routes(compositor->scene);

#ifndef GPAC_DISABLE_SCENEGRAPH
	i = 0;
	while ((sg = (GF_SceneGraph*)gf_list_enum(compositor->extra_scenes, &i))) {
		gf_sg_activate_routes(sg);
	}
#endif

#ifndef GPAC_DISABLE_LOG
	route_time = gf_sys_clock() - route_time;
#endif

#else
#ifndef GPAC_DISABLE_LOG
	route_time = 0;
#endif
#endif /*GPAC_DISABLE_VRML*/


	/*setup root visual BEFORE updating the composite textures (since they may depend on root setup)*/
	gf_sc_setup_root_visual(compositor, gf_sg_get_root_node(compositor->scene));

	/*setup display before updating composite textures (some may require a valid OpenGL context)*/
	gf_sc_recompute_ar(compositor, gf_sg_get_root_node(compositor->scene) );

	if (compositor->video_setup_failed)	{
		compositor_release_textures(compositor, GF_FALSE);
		gf_sc_lock(compositor, 0);
		return;
	}

#ifndef GPAC_DISABLE_LOG
	txtime = gf_sys_clock();
#endif
	/*update all composite textures*/
	compositor->texture_inserted = GF_FALSE;
	count = gf_list_count(compositor->textures);
	for (i=0; i<count; i++) {
		GF_TextureHandler *txh = (GF_TextureHandler *)gf_list_get(compositor->textures, i);
		if (!txh) break;
		//this is not a composite texture
		if (txh->stream) continue;
		/*signal graphics reset before updating*/
		if (compositor->reset_graphics && txh->tx_io) gf_sc_texture_reset(txh);
		txh->update_texture_fcnt(txh);
		if (compositor->texture_inserted) {
			compositor->texture_inserted = GF_FALSE;
			count = gf_list_count(compositor->textures);
			i = gf_list_find(compositor->textures, txh);
		}
	}

	//it may happen that we have a reconfigure request at this stage, especially if updating one of the textures update
	//forced a relayout - do it right away
	if (compositor->msg_type) {
		//reset AR recompute flag, it will be reset when msg is handled
		compositor->recompute_ar = 0;
		compositor_release_textures(compositor, GF_FALSE);
		gf_sc_lock(compositor, 0);
		return;
	}
#ifndef GPAC_DISABLE_LOG
	texture_time += gf_sys_clock() - txtime;
#endif

	compositor->text_edit_changed = 0;
	compositor->rebuild_offscreen_textures = 0;

	if (compositor->force_next_frame_redraw) {
		compositor->force_next_frame_redraw=0;
		compositor->frame_draw_type = GF_SC_DRAW_FRAME;
	}

	//if hidden and player mode, do not draw the scene
	if (compositor->is_hidden && compositor->player) {
		compositor->frame_draw_type = 0;
	}

	if (!compositor->player) {
		if (compositor->check_eos_state<=1) {
			/*check if we have to force a frame dispatch */

			//no passthrough texture
			if (!compositor->passthrough_txh) {
				if (!compositor->vfr) {
					//in CFR and no texture associated, always force a redraw
					if (!has_tx_streams && has_timed_nodes)
						compositor->frame_draw_type = GF_SC_DRAW_FRAME;
					//otherwise if texture(s) but not all done, force a redraw
					else if (!all_tx_done)
						compositor->frame_draw_type = GF_SC_DRAW_FRAME;
					//we still have active system pids (BIFS/LASeR/DIMS commands), force a redraw
					if (gf_list_count(compositor->systems_pids))
						compositor->frame_draw_type = GF_SC_DRAW_FRAME;
					//validator always triggers redraw to make sure the scene clock is incremented so that events can be fired
					if (compositor->validator_mode)
						compositor->frame_draw_type = GF_SC_DRAW_FRAME;
				}
			}
			//we have a passthrough texture, only generate an output frame when we have a new input on the passthrough
			else {
				//still waiting for initialization either from the passthrough stream or a texture used in the scene
				if (!compositor->passthrough_txh->width || !compositor->passthrough_txh->stream->pck || (compositor->ms_until_next_frame<0) ) {
					compositor->frame_draw_type = GF_SC_DRAW_NONE;
					//prevent release of frame
					if (compositor->passthrough_txh->needs_release)
						compositor->passthrough_txh->needs_release = 2;
				}
				//done
				else if ((compositor->passthrough_txh->stream_finished) && (compositor->ms_until_next_frame >= 0)) {
					compositor->check_eos_state = 2;
				}
			}
			if (compositor->frame_draw_type==GF_SC_DRAW_FRAME)
				compositor->check_eos_state = 0;
		}

	}

	frame_drawn = (compositor->frame_draw_type==GF_SC_DRAW_FRAME) ? 1 : 0;

	/*if invalidated, draw*/
	if (compositor->frame_draw_type) {
		Bool emit_frame = GF_FALSE;
		Bool textures_released = 0;

#ifndef GPAC_DISABLE_LOG
		traverse_time = gf_sys_clock();
		time_spent_in_anim = 0;
#endif

		if (compositor->traverse_state->immediate_draw) {
			compositor->frame_draw_type = GF_SC_DRAW_FRAME;
		}

		/*video flush only*/
		if (compositor->frame_draw_type==GF_SC_DRAW_FLUSH) {
			compositor->frame_draw_type = 0;
		}
		/*full render*/
		else {
			Bool scene_drawn;
			compositor->frame_draw_type = 0;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Redrawing scene - STB %d\n", compositor->scene_sampled_clock));
			scene_drawn = gf_sc_draw_scene(compositor);

#ifndef GPAC_DISABLE_LOG
			traverse_time = gf_sys_clock() - traverse_time;
#endif

			if (compositor->ms_until_next_frame < 0) {
				emit_frame = GF_FALSE;
				compositor->last_error = GF_OK;
			}
			else if (!scene_drawn)
				emit_frame = GF_FALSE;
			else if (compositor->frame_draw_type)
				emit_frame = GF_FALSE;
			else if (compositor->fonts_pending>0)
				emit_frame = GF_FALSE;
			else if (compositor->clipframe && (!compositor->visual->frame_bounds.width || !compositor->visual->frame_bounds.height)) {
				emit_frame = GF_FALSE;
			}
			else
				emit_frame = GF_TRUE;

#ifdef GPAC_CONFIG_ANDROID
            if (!emit_frame && scene_drawn) {
				compositor->frame_was_produced = GF_TRUE;
            }
#endif

		}
		/*and flush*/
#ifndef GPAC_DISABLE_LOG
		flush_time = gf_sys_clock();
#endif

		if (compositor->init_flags & GF_VOUT_INIT_HIDE)
			compositor->skip_flush = 1;

		//new frame generated, emit packet
		if (emit_frame) {
			u64 pck_frame_ts=0;
			GF_FilterPacket *pck;
			frame_ts = 0;
			if (compositor->passthrough_pck) {
				pck = compositor->passthrough_pck;
				compositor->passthrough_pck = NULL;
				pck_frame_ts = gf_filter_pck_get_cts(pck);
			} else {
				u32 offset_x=0, offset_y=0;

				//assign udta of frame interface event when using shared packet, as it is used to test when frame is released
				compositor->frame_ifce.user_data = compositor;

				u32 nb_bpp=0;
				u32 pf = compositor->passthrough_pfmt;
				if (compositor->video_out!=&raw_vout)
					pf = compositor->opfmt;
				if (!pf && compositor->forced_alpha)
					pf = GF_PIXEL_RGBA;

				u32 c_w=compositor->display_width, c_h=compositor->display_height;
				if (compositor->clipframe) {
					offset_x = compositor->visual->frame_bounds.x + compositor->display_width/2;
					offset_y = compositor->display_height/2 - compositor->visual->frame_bounds.y;
					switch (pf) {
					case GF_PIXEL_ARGB:
					case GF_PIXEL_RGBA:
						nb_bpp = 4;
						break;
					case GF_PIXEL_RGB:
					case GF_PIXEL_BGR:
						nb_bpp = 3;
						break;
					}
					if (nb_bpp) {
						c_w = compositor->visual->frame_bounds.width;
						c_h = compositor->visual->frame_bounds.height;

						gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_WIDTH, &PROP_UINT(c_w));
						gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_HEIGHT, &PROP_UINT(c_h));
						gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_STRIDE, NULL);
						gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_STRIDE_UV, NULL);


						gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(offset_x, offset_y));
						gf_filter_pid_set_property(compositor->vout, GF_PROP_PID_ORIG_SIZE, &PROP_VEC2I_INT(compositor->display_width, compositor->display_height));
					}
				}
				if ((c_w==compositor->display_width) && (c_h==compositor->display_height)) {
					if (compositor->video_out==&raw_vout) {
						pck = gf_filter_pck_new_shared(compositor->vout, compositor->framebuffer, compositor->framebuffer_size, gf_sc_frame_ifce_done);
					} else {
						compositor->frame_ifce.get_plane = gf_sc_frame_ifce_get_plane;
						compositor->frame_ifce.get_gl_texture = NULL;
#ifndef GPAC_DISABLE_3D
						if (compositor->fbo_tx_id) {
							compositor->frame_ifce.get_gl_texture = gf_sc_frame_ifce_get_gl_texture;
						}
#endif
						compositor->frame_ifce.flags = GF_FRAME_IFCE_BLOCKING;
						pck = gf_filter_pck_new_frame_interface(compositor->vout, &compositor->frame_ifce, gf_sc_frame_ifce_done);
					}
				} else {
					u32 j, osize = c_w * c_h * nb_bpp;
					Bool release_fb=GF_FALSE;
					u8 *output;
					u8 *src=NULL;
					pck = NULL;
					if (compositor->video_out==&raw_vout) {
						src = compositor->framebuffer;
					} else {
						if (gf_sc_get_screen_buffer(compositor, &compositor->fb, 0) == GF_OK) {
							src = compositor->fb.video_buffer;
							release_fb = GF_TRUE;
						}
					}
					if (src) {
						src += offset_x * nb_bpp + offset_y * nb_bpp * compositor->display_width;
						pck = gf_filter_pck_new_alloc(compositor->vout, osize, &output);
						if (output) {
							for (j=0; j<c_h; j++) {
								memcpy(output, src, nb_bpp*c_w);
								output += nb_bpp*c_w;
								src += nb_bpp * compositor->display_width;
							}
						}
						compositor->frame_ifce.user_data = NULL;
						if (release_fb)
							gf_sc_release_screen_buffer(compositor, &compositor->fb);
					}
				}

				if (!pck) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate output video packet\n"));
					compositor_release_textures(compositor, frame_drawn);
					gf_sc_lock(compositor, 0);
					return;
				}
				if (compositor->passthrough_txh) {
					gf_filter_pck_merge_properties(compositor->passthrough_txh->stream->pck, pck);
					pck_frame_ts = gf_filter_pck_get_cts(compositor->passthrough_txh->stream->pck);
				} else {
					//no texture found/updated, use scene sampled clock
					if (compositor->timescale) {
						frame_ts = compositor->frame_number * compositor->fps.den * compositor->timescale;
						frame_ts /= compositor->fps.num;
					} else {
						frame_ts = compositor->frame_number * compositor->fps.den;
					}
					gf_filter_pck_set_cts(pck, frame_ts);

					if (compositor->hint_extra_scene_cts_plus_one) {
						gf_filter_pck_set_cts(pck, compositor->hint_extra_scene_cts_plus_one-1);
						compositor->hint_extra_scene_cts_plus_one = 0;
					}
					if (compositor->hint_extra_scene_dur_plus_one) {
						gf_filter_pck_set_duration(pck, compositor->hint_extra_scene_dur_plus_one-1);
						compositor->hint_extra_scene_dur_plus_one = 0;
					}
				}
			}
			if (pck_frame_ts) {
				u64 ts = gf_timestamp_rescale(pck_frame_ts, compositor->passthrough_timescale, 1000);
				frame_ts = (u32) ts;
			}

			gf_filter_pck_send(pck);

			gf_sc_ar_update_video_clock(compositor->audio_renderer, frame_ts);

			if (!compositor->player) {
				//in regular mode don't change fps once we sent one frame
				compositor->autofps = GF_FALSE;
				if (compositor->passthrough_txh) {
					// update clock if not buffering
					if (!gf_mo_is_buffering(compositor->passthrough_txh->stream))
					{
						u64 next_frame;
						next_frame = pck_frame_ts + gf_filter_pck_get_duration(pck);
						next_frame *= 1000;
						next_frame /= compositor->passthrough_timescale;
						assert(next_frame>=compositor->scene_sampled_clock);
						compositor->scene_sampled_clock = (u32) next_frame;
					}
					// if buffering, remember to update clock at next frame
					else {
						compositor->passthrough_check_buffer = GF_TRUE;
					}
					if (compositor->passthrough_txh->stream_finished)
						compositor->check_eos_state = 2;
				} else {
					u64 res;
					compositor->last_frame_ts = frame_ts;
					res = (compositor->frame_number+1);
					res *= compositor->fps.den;
					res *= 1000;
					res /= compositor->fps.num;
					compositor->scene_sampled_clock = (u32) res;
				}
				GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Send video frame TS %u (%u ms) - next frame scene clock %d ms - passthrough %p\n", frame_ts, (frame_ts*1000)/compositor->fps.num, compositor->scene_sampled_clock, compositor->passthrough_txh));
				compositor->frame_was_produced = GF_TRUE;
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Send video frame TS %u - AR clock %d\n", frame_ts, compositor->audio_renderer->current_time));
			 	if (compositor->bench_mode) {
					compositor->force_bench_frame = 1;
				}
				compositor->frame_was_produced = GF_TRUE;
			}
		} else if (!compositor->player) {
			frame_drawn = GF_FALSE;
			//prevent release of frame
			if (compositor->passthrough_txh && compositor->passthrough_txh->needs_release)
				compositor->passthrough_txh->needs_release = 2;
		}

		if (compositor->passthrough_pck) {
		 	gf_filter_pck_discard(compositor->passthrough_pck);
		 	compositor->passthrough_pck = NULL;
		}

		//if no overlays, release textures before flushing, otherwise we might loose time waiting for vsync
		if (!compositor->visual->has_overlays) {
			compositor_release_textures(compositor, frame_drawn);
			textures_released = 1;
		}

		//no flush pending
		if (!compositor->flush_pending && !compositor->skip_flush) {
			gf_sc_flush_video(compositor, GF_TRUE);
		}

#ifndef GPAC_DISABLE_LOG
		flush_time = gf_sys_clock() - flush_time;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[Compositor] done flushing frame in %d ms\n", flush_time));
#endif

		visual_2d_draw_overlays(compositor->visual);
		compositor->last_had_overlays = compositor->visual->has_overlays;

		if (!textures_released) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[Compositor] Releasing textures after flush\n" ));
			compositor_release_textures(compositor, frame_drawn);
		}

		if (compositor->stress) {
			gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
			gf_sc_reset_graphics(compositor);
		}
		compositor->reset_fonts = 0;

	} else {

		//frame not drawn, release textures
		compositor_release_textures(compositor, frame_drawn);

#ifndef GPAC_DISABLE_LOG
		traverse_time = 0;
		time_spent_in_anim = 0;
		flush_time = 0;
		compositor->traverse_setup_time = 0;
		compositor->traverse_and_direct_draw_time = 0;
		compositor->indirect_draw_time = 0;
#endif

		if (!compositor->player) {
			//if systems frames are pending (not executed because too early), increase clock
			if ((compositor->sys_frames_pending && (compositor->ms_until_next_frame>=0) )
			//if timed nodes or validator mode (which acts as a timed node firing events), increase scene clock
			//in vfr mode
			|| (compositor->vfr && (has_timed_nodes || compositor->validator_mode) )
			) {
				u64 res;
				compositor->sys_frames_pending = GF_FALSE;
				compositor->frame_number++;
				res = compositor->frame_number;
				res *= compositor->fps.den;
				res *= 1000;
				res /= compositor->fps.num;
				assert(res >= compositor->scene_sampled_clock);
				compositor->scene_sampled_clock = (u32) res;
			}

			if (all_tx_done && !has_timed_nodes) {
				//we were in eos, notify we are done
				if (compositor->check_eos_state)
					compositor->check_eos_state = 2;
				//we were not in eos, force flushing pending audio
				else
					compositor->flush_audio = GF_TRUE;
			}
		}
	}
	compositor->reset_graphics = 0;

	compositor->last_frame_time = gf_sys_clock();
	end_time = compositor->last_frame_time - in_time;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI]\tCompositor Cycle Log\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
	                                  compositor->networks_time,
	                                  compositor->decoders_time,
	                                  compositor->frame_number,
	                                  compositor->traverse_state->immediate_draw,
	                                  compositor->visual_config_time,
	                                  event_time,
	                                  route_time,
	                                  smil_timing_time,
	                                  time_node_time,
	                                  texture_time,
	                                  time_spent_in_anim,
	                                  compositor->traverse_setup_time,
	                                  compositor->traverse_and_direct_draw_time,
	                                  compositor->traverse_and_direct_draw_time - time_spent_in_anim,
	                                  compositor->indirect_draw_time,
	                                  traverse_time,
	                                  flush_time,
	                                  end_time));

	if (frame_drawn) {
		compositor->current_frame = (compositor->current_frame+1) % GF_SR_FPS_COMPUTE_SIZE;
		compositor->frame_dur[compositor->current_frame] = end_time;
		compositor->frame_time[compositor->current_frame] = compositor->last_frame_time;
		compositor->frame_number++;
	}
	if (compositor->bench_mode && (frame_drawn || (has_timed_nodes&&all_tx_done) )) {
		//in bench mode we always increase the clock of the fixed target simulation rate - this needs refinement if video is used ...
		compositor->scene_sampled_clock += frame_duration;
	}
	gf_sc_lock(compositor, 0);

	if (frame_drawn) compositor->step_mode = GF_FALSE;

	/*old arch code kept for reference*/
#if 0
	/*let the owner decide*/
	if (compositor->no_regulation)
		return;

	/*we are in bench mode, just release for a moment the composition, oherwise we will constantly lock the compositor which may have impact on scene decoding*/
	if (compositor->bench_mode) {
		gf_sleep(0);
		return;
	}

	//we have a pending frame, return asap - we could sleep until frames matures but this give weird regulation
	if (compositor->ms_until_next_frame != GF_INT_MAX) {
		compositor->ms_until_next_frame -= end_time;

		if (compositor->ms_until_next_frame<=0) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Next frame already due (%d ms late) - not going to sleep\n", - compositor->ms_until_next_frame));
			compositor->ms_until_next_frame=0;
			return;
		}

		compositor->ms_until_next_frame = MIN(compositor->ms_until_next_frame, (s32) frame_duration );
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Next frame due in %d ms\n", compositor->ms_until_next_frame));
		if (compositor->ms_until_next_frame > 2) {
			u64 start = gf_sys_clock_high_res();
			u64 diff=0;
			u64 wait_for = 1000 * (u64) compositor->ms_until_next_frame;
			while (! compositor->msg_type) {
				gf_sleep(1);
				diff = gf_sys_clock_high_res() - start;
				if (diff >= wait_for)
					break;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Compositor slept %d ms until next frame (msg type %d)\n", diff/1000, compositor->msg_type));
		}
		return;
	}

	if (end_time > frame_duration) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Compositor did not go to sleep\n"));
		return;
	}

	/*compute sleep time till next frame*/
	end_time %= frame_duration;
	gf_sleep(frame_duration - end_time);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Compositor slept for %d ms\n", frame_duration - end_time));
#endif

}

Bool gf_sc_visual_is_registered(GF_Compositor *compositor, GF_VisualManager *visual)
{
	GF_VisualManager *tmp;
	u32 i = 0;
	while ((tmp = (GF_VisualManager *)gf_list_enum(compositor->visuals, &i))) {
		if (tmp == visual) return 1;
	}
	return 0;
}

void gf_sc_visual_register(GF_Compositor *compositor, GF_VisualManager *visual)
{
	if (gf_sc_visual_is_registered(compositor, visual)) return;
	gf_list_add(compositor->visuals, visual);
}

void gf_sc_visual_unregister(GF_Compositor *compositor, GF_VisualManager *visual)
{
	gf_list_del_item(compositor->visuals, visual);
}

void gf_sc_traverse_subscene_ex(GF_Compositor *compositor, GF_Node *inline_parent, GF_SceneGraph *subscene, void *rs)
{
	Fixed min_hsize, vp_scale;
	Bool use_pm, prev_pm, prev_coord;
	SFVec2f prev_vp;
	s32 flip_coords;
	u32 h, w, tag;
	GF_Matrix2D transf;
	GF_SceneGraph *in_scene;
	GF_Node *inline_root;
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;

	/*we don't traverse subscenes until the root one is setup*/
	if (!compositor->root_visual_setup) return;

	inline_root = gf_sg_get_root_node(subscene);
	if (!inline_root) return;

	if (!gf_scene_is_over(subscene))
		tr_state->subscene_not_over ++;

	flip_coords = 0;
	in_scene = gf_node_get_graph(inline_root);
	w = h = 0;

	/*check parent/child doc orientation*/

	/*check child doc*/
	tag = gf_node_get_tag(inline_root);
	if (tag < GF_NODE_RANGE_LAST_VRML) {
#ifndef GPAC_DISABLE_VRML
		u32 new_tag = 0;
		use_pm = gf_sg_use_pixel_metrics(in_scene);
		if (gf_node_get_tag(inline_parent)>GF_NODE_RANGE_LAST_VRML) {
			/*moving from SVG to VRML-based, need positive translation*/
			flip_coords = 1;

			/*if our root nodes are not LayerXD ones, insert one. This prevents mixing
			of bindable stacks and gives us free 2D/3D integration*/
			if (tag==TAG_MPEG4_OrderedGroup) {
				new_tag = TAG_MPEG4_Layer2D;
			} else if (tag==TAG_MPEG4_Group) {
				new_tag = TAG_MPEG4_Layer3D;
			}
#ifndef GPAC_DISABLE_X3D
			else if (tag==TAG_X3D_Group) {
				new_tag = TAG_MPEG4_Layer3D;
			}
#endif
		}
#if !defined(GPAC_DISABLE_X3D) && !defined(GPAC_DISABLE_3D)
		/*if the inline root node is a 3D one except Layer3D and we are not in a 3D context, insert
		a Layer3D at the root*/
		else if (!tr_state->visual->type_3d && ((tag==TAG_MPEG4_Group) || (tag==TAG_X3D_Group))) {
			new_tag = TAG_MPEG4_Layer3D;
		}
#endif

		/*create new root*/
		if (new_tag) {
			GF_SceneGraph *sg = gf_node_get_graph(inline_root);
			GF_Node *new_root = gf_node_new(sg, new_tag);
			gf_node_register(new_root, NULL);
			gf_sg_set_root_node(sg, new_root);
			gf_node_list_add_child(& ((GF_ParentNode*)new_root)->children, inline_root);
			gf_node_register(inline_root, new_root);
			gf_node_unregister(inline_root, NULL);
			inline_root = new_root;
			gf_node_init(new_root);
		}

#endif /*GPAC_DISABLE_VRML*/

		gf_sg_get_scene_size_info(in_scene, &w, &h);

	} else {
		use_pm = 1;
		if (gf_node_get_tag(inline_parent)<GF_NODE_RANGE_LAST_VRML) {
			/*moving from VRML-based to SVG, need negative translation*/
			flip_coords = -1;
		}
	}

	min_hsize = tr_state->min_hsize;
	prev_pm = tr_state->pixel_metrics;
	prev_vp = tr_state->vp_size;
	prev_coord = tr_state->fliped_coords;
	vp_scale = FIX_ONE;
	gf_mx2d_init(transf);

	/*compute center<->top-left transform*/
	if (flip_coords)
		gf_mx2d_add_scale(&transf, FIX_ONE, -FIX_ONE);

	/*if scene size is given in the child document, scale to fit the entire vp unless our VP is the full output*/
	if (w && h) {
		if ((compositor->scene_width != tr_state->vp_size.x) || (compositor->scene_height != tr_state->vp_size.y)) {
			Fixed scale_w = tr_state->vp_size.x/w;
			Fixed scale_h = tr_state->vp_size.y/h;
			vp_scale = MIN(scale_w, scale_h);
			gf_mx2d_add_scale(&transf, vp_scale, vp_scale);
		}
	}
	if (flip_coords) {
		gf_mx2d_add_translation(&transf, flip_coords * tr_state->vp_size.x/2, tr_state->vp_size.y/2);
		tr_state->fliped_coords = !tr_state->fliped_coords;
	}

	/*if scene size is given in the child document, scale back vp to take into account the above scale
	otherwise the scene won't be properly clipped*/
	if (w && h) {
		tr_state->vp_size.x = gf_divfix(tr_state->vp_size.x, vp_scale);
		tr_state->vp_size.y = gf_divfix(tr_state->vp_size.y, vp_scale);
	}


	/*compute pixel<->meter transform*/
	if (use_pm != tr_state->pixel_metrics) {
		/*override aspect ratio if any size info is given in the scene*/
		if (w && h) {
			Fixed scale = INT2FIX( MIN(w, h) / 2);
			if (scale) tr_state->min_hsize = scale;
		}
		if (!use_pm) {
			gf_mx2d_add_scale(&transf, tr_state->min_hsize, tr_state->min_hsize);
		} else {
			Fixed inv_scale = gf_invfix(tr_state->min_hsize);
			gf_mx2d_add_scale(&transf, inv_scale, inv_scale);
		}
		tr_state->pixel_metrics = use_pm;
	}

#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		GF_Matrix mx_bck, mx;
		gf_mx_copy(mx_bck, tr_state->model_matrix);

		gf_mx_from_mx2d(&mx, &transf);

		/*copy over z scale*/
		mx.m[10] = mx.m[5];
		gf_mx_add_matrix(&tr_state->model_matrix, &mx);
		gf_node_traverse(inline_root, rs);
		gf_mx_copy(tr_state->model_matrix, mx_bck);

	} else
#endif
	{
		GF_Matrix2D mx_bck;
		gf_mx2d_copy(mx_bck, tr_state->transform);
		gf_mx2d_pre_multiply(&tr_state->transform, &transf);
		gf_node_traverse(inline_root, rs);
		gf_mx2d_copy(tr_state->transform, mx_bck);
	}

	tr_state->pixel_metrics = prev_pm;
	tr_state->min_hsize = min_hsize;
	tr_state->vp_size = prev_vp;
	tr_state->fliped_coords = prev_coord;
}


static Bool gf_sc_handle_event_intern(GF_Compositor *compositor, GF_Event *event, Bool from_user)
{
	Bool ret;

	if ( (compositor->interaction_level & GF_INTERACT_INPUT_SENSOR) && (event->type <= GF_EVENT_LAST_MOUSE)) {
		GF_Event evt = *event;
		gf_sc_input_sensor_mouse_input(compositor, &evt.mouse);
	}

	/*	if (!compositor->interaction_level || (compositor->interaction_level==GF_INTERACT_INPUT_SENSOR) ) {
			if (!from_user) {
				GF_USER_SENDEVENT(compositor->user, event);
			}
			return 0;
		}
	*/
	gf_mx_p(compositor->mx);
	ret = gf_sc_exec_event(compositor, event);
	gf_sc_lock(compositor, GF_FALSE);

//	if (!from_user) { }
	return ret;
}

void gf_sc_traverse_subscene(GF_Compositor *compositor, GF_Node *inline_parent, GF_SceneGraph *subscene, void *rs)
{
	u32 i=0;
	GF_SceneGraph *subsg;

	gf_sc_traverse_subscene_ex(compositor, inline_parent, subscene, rs);

	while ( (subsg = gf_scene_enum_extra_scene(subscene, &i)))
		gf_sc_traverse_subscene_ex(compositor, inline_parent, subsg, rs);

}

static Bool gf_sc_on_event_ex(GF_Compositor *compositor , GF_Event *event, Bool from_user)
{
	//special case for gui, we may get the callback before the script is initialized even of the scene is created
	//so we always store it in the config file
	if ((!compositor || (compositor->player==2)) && (event->type==GF_EVENT_NAVIGATE)) {
		gf_opts_set_key("temp", "gui_load_urls", event->navigate.to_url);
		if (compositor && !compositor->root_scene) return GF_FALSE;
	}
	/*not assigned yet*/
	if (!compositor || !compositor->visual || compositor->discard_input_events) {
		return GF_FALSE;
	}

	/*we're reconfiguring the video output, cancel all messages except GL reconfig (context lost)*/
	if (compositor->msg_type & GF_SR_IN_RECONFIG) {
		if (event->type==GF_EVENT_VIDEO_SETUP) {
			if (event->setup.hw_reset)
				gf_sc_reset_graphics(compositor);

			if (event->setup.back_buffer)
				compositor->recompute_ar = 1;
		}
		return GF_FALSE;
	}
	switch (event->type) {
	case GF_EVENT_SHOWHIDE:
		if (!from_user) {
			/*switch fullscreen off!!!*/
			compositor->is_hidden = event->show.show_type ? GF_FALSE : GF_TRUE;
			break;
		}
	case GF_EVENT_SET_CAPTION:
	case GF_EVENT_MOVE:
		if (!from_user) {
			if (compositor->last_had_overlays) {
				gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
			}
			break;
		}
		compositor->video_out->ProcessEvent(compositor->video_out, event);
		break;

		break;
	case GF_EVENT_REFRESH:
		/*when refreshing a window with overlays we redraw the scene */
		gf_sc_next_frame_state(compositor, compositor->last_had_overlays ? GF_SC_DRAW_FRAME : GF_SC_DRAW_FLUSH);
		break;
	case GF_EVENT_VIDEO_SETUP:
	{
		Bool locked = gf_mx_try_lock(compositor->mx);
		if (event->setup.hw_reset) {
			gf_sc_reset_graphics(compositor);
			compositor->reset_graphics = 2;
		}

		if (event->setup.back_buffer)
			compositor->recompute_ar = 1;
		if (locked) gf_mx_v(compositor->mx);
	}
	break;
	case GF_EVENT_SIZE:
		/*user consumed the resize event, do nothing*/
		if ( gf_sc_send_event(compositor, event) )
			return GF_TRUE;

		/*not consumed and compositor "owns" the output window (created by the output module), resize*/
		if (!compositor->os_wnd) {
			/*EXTRA CARE HERE: the caller (video output) is likely a different thread than the compositor one, and the
			compositor may be locked on the video output (flush or whatever)!!
			*/
			Bool lock_ok = gf_mx_try_lock(compositor->mx);
			if ((compositor->display_width!=event->size.width)
					|| (compositor->display_height!=event->size.height)
					|| (compositor->new_width && (compositor->new_width!=event->size.width))
					|| (compositor->new_width && (compositor->new_height!=event->size.height))
				) {

				//OSX bug with SDL when requesting 4k window we get max screen height but 4k width ...
#if defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_CONFIG_IOS)
				if (compositor->display_width==event->size.width) {
					if (compositor->display_height > 2*event->size.height) {
						event->size.width = compositor->display_width * event->size.height / compositor->display_height;
					}
				}
#endif
				compositor->new_width = event->size.width;
				compositor->new_height = event->size.height;

				compositor->msg_type |= GF_SR_CFG_SET_SIZE;
				if (from_user) compositor->msg_type &= ~GF_SR_CFG_WINDOWSIZE_NOTIF;
			} else {
				/*remove pending resize notif but not resize requests*/
				compositor->msg_type &= ~GF_SR_CFG_WINDOWSIZE_NOTIF;
			}
			if (compositor->disp_ori != event->size.orientation) {
				u32 i;
				compositor->disp_ori = event->size.orientation;
				for (i=0; i<gf_list_count(compositor->textures); i++) {
					GF_TextureHandler *txh = gf_list_get(compositor->textures, i);
					if (!txh || !txh->stream || !txh->stream->odm || !txh->stream->width) continue;
					gf_mo_update_caps_ex(txh->stream, GF_FALSE);
					if (txh->stream->odm->parentscene
						&& txh->stream->odm->parentscene->is_dynamic_scene
						&& (txh->stream->odm->state==GF_ODM_STATE_PLAY)
					) {
						gf_scene_force_size_to_video(txh->stream->odm->parentscene, txh->stream);
					}
				}
			}
			if (lock_ok) gf_sc_lock(compositor, GF_FALSE);
		}
		return GF_FALSE;

	case GF_EVENT_KEYDOWN:
	case GF_EVENT_KEYUP:
	{
		Bool ret;
		switch (event->key.key_code) {
		case GF_KEY_SHIFT:
			if (event->type==GF_EVENT_KEYDOWN) {
				compositor->key_states |= GF_KEY_MOD_SHIFT;
			} else {
				compositor->key_states &= ~GF_KEY_MOD_SHIFT;
			}
			break;
		case GF_KEY_CONTROL:
			if (event->type==GF_EVENT_KEYDOWN) {
				compositor->key_states |= GF_KEY_MOD_CTRL;
			} else {
				compositor->key_states &= ~GF_KEY_MOD_CTRL;
			}
			break;
		case GF_KEY_ALT:
			if (event->type==GF_EVENT_KEYDOWN) {
				compositor->key_states |= GF_KEY_MOD_ALT;
			} else {
				compositor->key_states &= ~GF_KEY_MOD_ALT;
			}
			break;
		}

		ret = GF_FALSE;
		event->key.flags |= compositor->key_states;
		/*key sensor*/
		if ((compositor->interaction_level & GF_INTERACT_INPUT_SENSOR) ) {
			ret = gf_sc_input_sensor_keyboard_input(compositor, event->key.key_code, event->key.hw_code, (event->type==GF_EVENT_KEYUP) ? GF_TRUE : GF_FALSE);
		}
		ret += gf_sc_handle_event_intern(compositor, event, from_user);
		return ret;
	}

	case GF_EVENT_TEXTINPUT:
		if (compositor->interaction_level & GF_INTERACT_INPUT_SENSOR)
			gf_sc_input_sensor_string_input(compositor , event->character.unicode_char);

		return gf_sc_handle_event_intern(compositor, event, from_user);

	case GF_EVENT_MOUSEMOVE:
		event->mouse.button = 0;
	case GF_EVENT_MOUSEDOWN:
	case GF_EVENT_MOUSEUP:
	case GF_EVENT_MOUSEWHEEL:
		event->mouse.key_states = compositor->key_states;
	case GF_EVENT_SENSOR_ORIENTATION:
	case GF_EVENT_MULTITOUCH:
		return gf_sc_handle_event_intern(compositor, event, from_user);

	case GF_EVENT_PASTE_TEXT:
		gf_sc_paste_text(compositor, event->clipboard.text);
		break;
	case GF_EVENT_COPY_TEXT:
		if (gf_sc_has_text_selection(compositor)) {
			const char *str = gf_sc_get_selected_text(compositor);
			event->clipboard.text = str ? gf_strdup(event->clipboard.text) : NULL;
		} else {
			event->clipboard.text = NULL;
		}
		break;
	case GF_EVENT_QUIT:
		if (compositor->audio_renderer)
			compositor->audio_renderer->non_rt_output = 2;
		compositor->check_eos_state = 1;
		return gf_sc_send_event(compositor, event);
	/*when we process events we don't forward them to the user*/
	default:
		return gf_sc_send_event(compositor, event);
	}
	/*if we get here, event has been consumed*/
	return GF_TRUE;
}

GF_EXPORT
Bool gf_sc_on_event(void *cbck, GF_Event *event)
{
	return gf_sc_on_event_ex((GF_Compositor *)cbck, event, GF_FALSE);
}

GF_EXPORT
Bool gf_sc_user_event(GF_Compositor *compositor, GF_Event *event)
{
	switch (event->type) {
	case GF_EVENT_SHOWHIDE:
	case GF_EVENT_MOVE:
	case GF_EVENT_SET_CAPTION:
		compositor->video_out->ProcessEvent(compositor->video_out, event);
		return GF_FALSE;
	default:
		return gf_sc_on_event_ex(compositor, event, GF_TRUE);
	}
}

GF_EXPORT
void gf_sc_register_extra_graph(GF_Compositor *compositor, GF_SceneGraph *extra_scene, Bool do_remove)
{
	gf_sc_lock(compositor, GF_TRUE);
	if (do_remove) gf_list_del_item(compositor->extra_scenes, extra_scene);
	else if (gf_list_find(compositor->extra_scenes, extra_scene)<0) gf_list_add(compositor->extra_scenes, extra_scene);
	gf_sc_lock(compositor, GF_FALSE);
}

Bool gf_sc_script_action(GF_Compositor *compositor, GF_JSAPIActionType type, GF_Node *n, GF_JSAPIParam *param)
{
	switch (type) {
	case GF_JSAPI_OP_GET_SCALE:
		param->val = compositor->zoom;
		return GF_TRUE;
	case GF_JSAPI_OP_SET_SCALE:
		compositor_2d_set_user_transform(compositor, param->val, compositor->trans_x, compositor->trans_y, GF_FALSE);
		return GF_TRUE;
	case GF_JSAPI_OP_GET_ROTATION:
		param->val = gf_divfix(180*compositor->rotation, GF_PI);
		return GF_TRUE;
	case GF_JSAPI_OP_SET_ROTATION:
		compositor->rotation = gf_mulfix(GF_PI, param->val/180);
		compositor_2d_set_user_transform(compositor, compositor->zoom, compositor->trans_x, compositor->trans_y, GF_FALSE);
		return GF_TRUE;
	case GF_JSAPI_OP_GET_TRANSLATE:
		param->pt.x = compositor->trans_x;
		param->pt.y = compositor->trans_y;
		return GF_TRUE;
	case GF_JSAPI_OP_SET_TRANSLATE:
		compositor_2d_set_user_transform(compositor, compositor->zoom, param->pt.x, param->pt.y, GF_FALSE);
		return GF_TRUE;
	/*FIXME - better SMIL timelines support*/
	case GF_JSAPI_OP_GET_TIME:
		param->time = gf_node_get_scene_time(n);
		return GF_TRUE;
	case GF_JSAPI_OP_SET_TIME: /*seek_to(param->time);*/
		return GF_FALSE;
	/*FIXME - this will not work for inline docs, we will have to store the clipper at the <svg> level*/
	case GF_JSAPI_OP_GET_VIEWPORT:
#ifndef GPAC_DISABLE_SVG
		if (compositor_svg_get_viewport(n, &param->rc)) return GF_TRUE;
#endif
		param->rc = gf_rect_center(compositor->traverse_state->vp_size.x, compositor->traverse_state->vp_size.y);
		if (!compositor->visual->center_coords) {
			param->rc.x = 0;
			param->rc.y = 0;
		}
		return GF_TRUE;
	case GF_JSAPI_OP_SET_FOCUS:
		compositor->focus_node = param->node;
		return GF_TRUE;
	case GF_JSAPI_OP_GET_FOCUS:
		param->node = compositor->focus_node;
		return GF_TRUE;

	/*same routine: traverse tree from root to target, collecting both bounds and transform matrix*/
	case GF_JSAPI_OP_GET_LOCAL_BBOX:
	case GF_JSAPI_OP_GET_SCREEN_BBOX:
	case GF_JSAPI_OP_GET_TRANSFORM:
	{
		GF_TraverseState tr_state;
		memset(&tr_state, 0, sizeof(tr_state));
		tr_state.traversing_mode = TRAVERSE_GET_BOUNDS;
		tr_state.visual = compositor->visual;
		tr_state.vp_size = compositor->traverse_state->vp_size;
		tr_state.for_node = n;
		tr_state.ignore_strike = GF_TRUE;
		tr_state.min_hsize = compositor->traverse_state->min_hsize;
		tr_state.pixel_metrics = compositor->traverse_state->pixel_metrics;
		gf_mx2d_init(tr_state.transform);
		gf_mx2d_init(tr_state.mx_at_node);


		if (type==GF_JSAPI_OP_GET_LOCAL_BBOX) {
#ifndef GPAC_DISABLE_SVG
			GF_SAFEALLOC(tr_state.svg_props, SVGPropertiesPointers);
			if (tr_state.svg_props) {
				gf_svg_properties_init_pointers(tr_state.svg_props);
				tr_state.abort_bounds_traverse = GF_TRUE;
				gf_node_traverse(n, &tr_state);
				gf_svg_properties_reset_pointers(tr_state.svg_props);
				gf_free(tr_state.svg_props);
			}
#endif
		} else {
			gf_node_traverse(gf_sg_get_root_node(compositor->scene), &tr_state);
		}
		if (!tr_state.bounds.height && !tr_state.bounds.width && !tr_state.bounds.x && !tr_state.bounds.y)
			tr_state.abort_bounds_traverse = GF_FALSE;

		gf_mx2d_pre_multiply(&tr_state.mx_at_node, &compositor->traverse_state->transform);

		if (type==GF_JSAPI_OP_GET_LOCAL_BBOX) {
			gf_bbox_from_rect(&param->bbox, &tr_state.bounds);
		} else if (type==GF_JSAPI_OP_GET_TRANSFORM) {
			gf_mx_from_mx2d(&param->mx, &tr_state.mx_at_node);
		} else {
			gf_mx2d_apply_rect(&tr_state.mx_at_node, &tr_state.bounds);
			gf_bbox_from_rect(&param->bbox, &tr_state.bounds);
		}
		if (!tr_state.abort_bounds_traverse) param->bbox.is_set = GF_FALSE;
	}
	return GF_TRUE;
	case GF_JSAPI_OP_LOAD_URL:
	{
#ifndef GPAC_DISABLE_VRML
		GF_Node *target;
		char *sub_url = strrchr(param->uri.url, '#');
		if (!sub_url) return GF_FALSE;
		target = gf_sg_find_node_by_name(gf_node_get_graph(n), sub_url+1);
		if (target && (gf_node_get_tag(target)==TAG_MPEG4_Viewport) ) {
			((M_Viewport *)target)->set_bind = 1;
			((M_Viewport *)target)->on_set_bind(n, NULL);
			return GF_TRUE;
		}
#endif
		return GF_FALSE;
	}
	case GF_JSAPI_OP_GET_FPS:
		param->time = gf_sc_get_fps(compositor, GF_FALSE);
		return GF_TRUE;
	case GF_JSAPI_OP_GET_SPEED:
		param->time = 0;
#ifndef GPAC_DISABLE_3D
		if (compositor->visual->type_3d==2) {
			param->time = FIX2FLT(compositor->visual->camera.speed);
		}
#endif
		return GF_TRUE;
	case GF_JSAPI_OP_PAUSE_SVG:
		if (n) {
			u32 tag = gf_node_get_tag(n);
			switch(tag) {
#ifndef GPAC_DISABLE_SVG
			case TAG_SVG_audio:
				svg_pause_audio(n, GF_TRUE);
				break;
			case TAG_SVG_video:
				svg_pause_video(n, GF_TRUE);
				break;
			case TAG_SVG_animation:
				svg_pause_animation(n, GF_TRUE);
				break;
#endif
			}
		} else {
			return GF_FALSE;
		}
		return GF_TRUE;
	case GF_JSAPI_OP_RESUME_SVG:
	{
		u32 tag = gf_node_get_tag(n);
		switch(tag) {
#ifndef GPAC_DISABLE_SVG
		case TAG_SVG_audio:
			svg_pause_audio(n, GF_FALSE);
			break;
		case TAG_SVG_video:
			svg_pause_video(n, GF_FALSE);
			break;
		case TAG_SVG_animation:
			svg_pause_animation(n, GF_FALSE);
			break;
#endif
		}
	}
	return GF_TRUE;
	case GF_JSAPI_OP_GET_DPI_X:
		param->opt = compositor->video_out->dpi_x;
		return GF_TRUE;
	case GF_JSAPI_OP_GET_DPI_Y:
		param->opt = compositor->video_out->dpi_y;
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
	return GF_FALSE;
}

Bool gf_sc_pick_in_clipper(GF_TraverseState *tr_state, GF_Rect *clip)
{
#ifndef GPAC_DISABLE_3D
	if (tr_state->visual->type_3d) {
		SFVec3f pos;
		GF_Matrix mx;
		GF_Ray r;
		gf_mx_copy(mx, tr_state->model_matrix);
		gf_mx_inverse(&mx);
		r = tr_state->ray;
		gf_mx_apply_ray(&mx, &r);
		if (!compositor_get_2d_plane_intersection(&r, &pos)) return GF_FALSE;
		if ( (pos.x < clip->x) || (pos.y > clip->y)
		        || (pos.x > clip->x + clip->width) || (pos.y < clip->y - clip->height) ) return GF_FALSE;

	} else
#endif
	{
		GF_Rect rc = *clip;
		GF_Point2D pt;
		gf_mx2d_apply_rect(&tr_state->transform, &rc);
		pt.x = tr_state->ray.orig.x;
		pt.y = tr_state->ray.orig.y;

		if ( (pt.x < rc.x) || (pt.y > rc.y)
		        || (pt.x > rc.x + rc.width) || (pt.y < rc.y - rc.height) ) return GF_FALSE;
	}
	return GF_TRUE;
}

GF_EXPORT
Bool gf_sc_has_text_selection(GF_Compositor *compositor)
{
	return (compositor->store_text_state==GF_SC_TSEL_FROZEN) ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
const char *gf_sc_get_selected_text(GF_Compositor *compositor)
{
	const u16 *srcp;
	u32 len;
	if (compositor->store_text_state != GF_SC_TSEL_FROZEN) return NULL;

	gf_sc_lock(compositor, GF_TRUE);

	compositor->traverse_state->traversing_mode = TRAVERSE_GET_TEXT;
	if (compositor->sel_buffer) {
		gf_free(compositor->sel_buffer);
		compositor->sel_buffer = NULL;
	}
	compositor->sel_buffer_len = 0;
	compositor->sel_buffer_alloc = 0;
	gf_node_traverse(compositor->text_selection, compositor->traverse_state);
	compositor->traverse_state->traversing_mode = 0;
	if (compositor->sel_buffer) compositor->sel_buffer[compositor->sel_buffer_len]=0;
	srcp = compositor->sel_buffer;

	if (compositor->selected_text) gf_free(compositor->selected_text);
	compositor->selected_text = gf_malloc(sizeof(char)*2*compositor->sel_buffer_len);
	len = gf_utf8_wcstombs((char *) compositor->selected_text, 2*compositor->sel_buffer_len, &srcp);
	if (len == GF_UTF8_FAIL) len = 0;
	compositor->selected_text[len] = 0;
	gf_sc_lock(compositor, GF_FALSE);

	return (const char *) compositor->selected_text;
}


void gf_sc_check_focus_upon_destroy(GF_Node *n)
{
	GF_Compositor *compositor = gf_sc_get_compositor(n);
	if (!compositor) return;

	if (compositor->focus_node==n) {
		compositor->focus_node = NULL;
		compositor->focus_text_type = 0;
		compositor->focus_uses_dom_events = GF_FALSE;
		gf_list_reset(compositor->focus_ancestors);
		gf_list_reset(compositor->focus_use_stack);
	}
	if (compositor->hit_node==n) compositor->hit_node = NULL;
	if (compositor->hit_text==n) compositor->hit_text = NULL;
}

#if 0 //unused
void gf_sc_set_system_pending_frame(GF_Compositor *compositor, Bool frame_pending)
{
	if (frame_pending) {
		if (!compositor->force_bench_frame)
			compositor->force_bench_frame = 1;
	} else {
		//do not increase clock
		compositor->force_bench_frame = 2;
	}
}

void gf_sc_queue_event(GF_Compositor *compositor, GF_Event *evt)
{
	u32 i, count;
	GF_QueuedEvent *qev;
	gf_mx_p(compositor->evq_mx);

	count = gf_list_count(compositor->event_queue);
	for (i=0; i<count; i++) {
		qev = gf_list_get(compositor->event_queue, i);
		if (!qev->node && (qev->evt.type==evt->type)) {
			qev->evt = *evt;
			gf_mx_v(compositor->evq_mx);
			return;
		}
	}
	GF_SAFEALLOC(qev, GF_QueuedEvent);
	if (!qev) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate event for queuing\n"));
	} else {
		qev->evt = *evt;
		gf_list_add(compositor->event_queue, qev);
	}
	gf_mx_v(compositor->evq_mx);
}
#endif


void gf_sc_queue_dom_event(GF_Compositor *compositor, GF_Node *node, GF_DOM_Event *evt)
{
	u32 i, count;
	GF_QueuedEvent *qev;
	gf_mx_p(compositor->evq_mx);

	count = gf_list_count(compositor->event_queue);
	for (i=0; i<count; i++) {
		qev = gf_list_get(compositor->event_queue, i);
		if ((qev->node==node) && (qev->dom_evt.type==evt->type)) {
			qev->dom_evt = *evt;
			gf_mx_v(compositor->evq_mx);
			return;
		}
	}
	GF_SAFEALLOC(qev, GF_QueuedEvent);
	if (!qev) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate event for queuing\n"));
	} else {
		qev->node = node;
		qev->dom_evt = *evt;
		gf_list_add(compositor->event_queue, qev);
	}
	gf_mx_v(compositor->evq_mx);
}

void gf_sc_queue_dom_event_on_target(GF_Compositor *compositor, GF_DOM_Event *evt, GF_DOMEventTarget *target, GF_SceneGraph *sg)
{
	u32 i, count;
	GF_QueuedEvent *qev;
	gf_mx_p(compositor->evq_mx);

	count = gf_list_count(compositor->event_queue);
	for (i=0; i<count; i++) {
		qev = gf_list_get(compositor->event_queue, i);
		if ((qev->target==target) && (qev->dom_evt.type==evt->type) && (qev->sg==sg) ) {
			//do not override any pending dowload progress event by new buffer state events
			if ((evt->type!=GF_EVENT_MEDIA_PROGRESS) || !qev->dom_evt.media_event.loaded_size) {
				qev->dom_evt = *evt;
			}
			gf_mx_v(compositor->evq_mx);
			return;
		}
	}

	GF_SAFEALLOC(qev, GF_QueuedEvent);
	if (!qev) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to allocate event for queuing\n"));
	} else {
		qev->sg = sg;
		qev->target = target;
		qev->dom_evt = *evt;
		gf_list_add(compositor->event_queue, qev);
	}
	gf_mx_v(compositor->evq_mx);
}

void sc_cleanup_event_queue(GF_List *evq, GF_Node *node, GF_SceneGraph *sg)
{
	u32 i, count = gf_list_count(evq);
	for (i=0; i<count; i++) {
		Bool del = 0;
		GF_QueuedEvent *qev = gf_list_get(evq, i);
		if (qev->node) {
			if (node == qev->node)
				del = 1;
			if (sg && (gf_node_get_graph(qev->node)==sg))
				del = 1;
		}
		if (qev->sg && (qev->sg==sg))
			del = 1;
		else if (qev->target && (qev->target->ptr_type == GF_DOM_EVENT_TARGET_NODE)) {
			if (node && ((GF_Node *)qev->target->ptr==node))
				del = 1;
			if (sg && (gf_node_get_graph((GF_Node *)qev->target->ptr)==sg))
				del = 1;
		}

		if (del) {
			gf_list_rem(evq, i);
			i--;
			count--;
			gf_free(qev);
		}
	}
}

void gf_sc_node_destroy(GF_Compositor *compositor, GF_Node *node, GF_SceneGraph *sg)
{
	gf_mx_p(compositor->evq_mx);
	sc_cleanup_event_queue(compositor->event_queue, node, sg);
	sc_cleanup_event_queue(compositor->event_queue_back, node, sg);
	gf_mx_v(compositor->evq_mx);
}

GF_EXPORT
Bool gf_sc_navigation_supported(GF_Compositor *compositor, u32 type)
{
	if (compositor->navigation_disabled ) return GF_FALSE;
#ifndef GPAC_DISABLE_3D
	if (compositor->visual->type_3d || compositor->active_layer) {
		GF_Camera *cam = compositor_3d_get_camera(compositor);
		if (cam->navigation_flags & NAV_ANY) {
			return GF_TRUE;
		} else {
#ifndef GPAC_DISABLE_VRML
			M_NavigationInfo *ni = (M_NavigationInfo *)gf_list_get(compositor->visual->navigation_stack, 0);
			if (ni) {
				u32 i;
				for (i=0; i<ni->type.count; i++) {
					if (!ni->type.vals[i]) continue;
					if (!stricmp(ni->type.vals[i], "WALK") && (type==GF_NAVIGATE_WALK)) return GF_TRUE;
					else if (!stricmp(ni->type.vals[i], "NONE") && (type==GF_NAVIGATE_NONE)) return GF_TRUE;
					else if (!stricmp(ni->type.vals[i], "EXAMINE") && (type==GF_NAVIGATE_EXAMINE)) return GF_TRUE;
					else if (!stricmp(ni->type.vals[i], "FLY") && (type==GF_NAVIGATE_FLY)) return GF_TRUE;
					else if (!stricmp(ni->type.vals[i], "VR") && (type==GF_NAVIGATE_VR)) return GF_TRUE;
					else if (!stricmp(ni->type.vals[i], "GAME") && (type==GF_NAVIGATE_GAME)) return GF_TRUE;
					else if (!stricmp(ni->type.vals[i], "ORBIT") && (type==GF_NAVIGATE_ORBIT)) return GF_TRUE;
				}
			}
#endif
			return GF_FALSE;
		}
	} else
#endif
		if ((type!=GF_NAVIGATE_NONE) && (type!=GF_NAVIGATE_SLIDE) && (type!=GF_NAVIGATE_EXAMINE)) {
			return GF_FALSE;
		}
	return GF_TRUE;
}


u32 gf_sc_check_end_of_scene(GF_Compositor *compositor, Bool skip_interactions)
{
	if (!compositor->root_scene || !compositor->root_scene->root_od || !compositor->root_scene->root_od->scene_ns) return 1;

	if (!skip_interactions) {
		/*if input sensors consider the scene runs forever*/
		if (gf_list_count(compositor->input_streams)) return 0;
		if (gf_list_count(compositor->x3d_sensors)) return 0;
	}

	/*check no clocks are still running*/
	if (!gf_scene_check_clocks(compositor->root_scene->root_od->scene_ns, compositor->root_scene, 0)) return 0;
	if (compositor->root_scene->is_dynamic_scene) return 1;

	/*ask compositor if there are sensors*/
	return gf_sc_get_option(compositor, skip_interactions ? GF_OPT_IS_OVER : GF_OPT_IS_FINISHED);
}

void gf_sc_queue_node_traverse(GF_Compositor *compositor, GF_Node *node)
{
	gf_sc_lock(compositor, GF_TRUE);
	if (!compositor->nodes_pending) compositor->nodes_pending = gf_list_new();
	gf_list_add(compositor->nodes_pending, node);
	gf_sc_lock(compositor, GF_FALSE);
}
void gf_sc_unqueue_node_traverse(GF_Compositor *compositor, GF_Node *node)
{
	gf_sc_lock(compositor, GF_TRUE);
	if (compositor->nodes_pending) {
		gf_list_del_item(compositor->nodes_pending, node);
		if (!gf_list_count(compositor->nodes_pending)) {
			gf_list_del(compositor->nodes_pending);
			compositor->nodes_pending = NULL;
		}
	}
	gf_sc_lock(compositor, GF_FALSE);
}

GF_EXPORT
GF_DownloadManager *gf_sc_get_downloader(GF_Compositor *compositor)
{
	return gf_filter_get_download_manager(compositor->filter);
}

void gf_sc_sys_frame_pending(GF_Compositor *compositor, u32 cts, u32 obj_time, GF_Filter *from_filter)
{
	if (!compositor->player) {
		if (!compositor->sys_frames_pending) {
			compositor->sys_frames_pending = GF_TRUE;
			gf_filter_post_process_task(compositor->filter);
		}
		if (!from_filter) return;

		//clock is not realtime but frame base, estimate how fast we generate frames
		//if last frame time diff is less than 1s, consider the output is not consumed in real-time
		//and reschedule asap
		//otherwise reschedule in 1 ms
		//This is needed to avoid high cpu usage when output is realtime (compositor->vout)
		u32 fidx = compositor->current_frame;
		s64 run_time = compositor->frame_time[fidx];
		if (fidx) fidx--;
		else fidx = GF_SR_FPS_COMPUTE_SIZE-1;
		run_time -= compositor->frame_time[fidx];
		if (run_time<0)
			run_time += 0xFFFFFFFFUL;
		if (run_time<1000) {
			//request in 1 us (0 would only reschedule filter if pending packets which may not always be the case, cf vtt dec)
			gf_filter_ask_rt_reschedule(from_filter, 1);
		} else {
			gf_filter_ask_rt_reschedule(from_filter, 1000);
		}
	} else {
		u32 wait_ms;
		if (cts < obj_time) wait_ms = (u32) (0xFFFFFFFFUL + cts - obj_time);
		else wait_ms = cts - obj_time;
		if (wait_ms>1000)
			wait_ms = 1000;

		if (!compositor->ms_until_next_frame || ((s32) wait_ms < compositor->ms_until_next_frame)) {
			compositor->ms_until_next_frame = (s32) wait_ms;
		}
		if (from_filter) {
			gf_filter_ask_rt_reschedule(from_filter, wait_ms*500);
		}
		//in player mode, force scene ready as soon as we have a pending frame
		//otherwise the audio renderer will not increase current time
		compositor->audio_renderer->scene_ready = GF_TRUE;
	}
}


Bool gf_sc_check_sys_frame(GF_Scene *scene, GF_ObjectManager *odm, GF_FilterPid *for_pid, GF_Filter *from_filter, u64 cts_in_ms, u32 dur_in_ms)
{
	Bool is_early=GF_FALSE;
	assert(odm);

	if (for_pid)
		gf_odm_check_buffering(odm, for_pid);

	u32 obj_time = gf_clock_time(odm->ck);
	if (cts_in_ms > obj_time)
		is_early = GF_TRUE;
	else if ((obj_time>0x7FFFFFFF) && (cts_in_ms<0x7FFFFFFF))
		is_early = GF_TRUE;

	if (is_early) {
		gf_sc_sys_frame_pending(scene->compositor, (u32)cts_in_ms, obj_time, from_filter);
		return GF_FALSE;
	}
	if (scene->compositor->vfr) {
		u32 ts = scene->compositor->timescale;
		if (!ts) ts = scene->compositor->fps.num;
		scene->compositor->hint_extra_scene_cts_plus_one = 1 + gf_timestamp_rescale(cts_in_ms, 1000, ts);
		scene->compositor->hint_extra_scene_dur_plus_one = (u32) ( 1 + gf_timestamp_rescale(dur_in_ms, 1000, ts));
	}
	return GF_TRUE;
}


Bool gf_sc_check_gl_support(GF_Compositor *compositor)
{
#ifdef GPAC_DISABLE_3D
	return GF_FALSE;
#else
	if (!compositor->player && !compositor->is_opengl) {
		if (compositor->drv==GF_SC_DRV_OFF) {
			return GF_FALSE;
		}
		compositor->needs_offscreen_gl = GF_TRUE;
		compositor->autoconfig_opengl = 1;
		compositor->recompute_ar = 1;
		return GF_TRUE;
	}
	return GF_TRUE;
#endif

}

GF_EXPORT
u32 gf_sc_get_time_in_ms(GF_Compositor *sc)
{
	if (!sc || !sc->root_scene) return 0;
	GF_Clock *ck = sc->root_scene->root_od->ck;
	if (!ck) return 0;
	return (u32) gf_clock_media_time(ck);
}

GF_EXPORT
void gf_sc_switch_quality(GF_Compositor *compositor, Bool up)
{
	if (compositor)
		gf_scene_switch_quality(compositor->root_scene, up);
}

GF_EXPORT
void gf_sc_toggle_addons(GF_Compositor *compositor, Bool show_addons)
{
	if (!compositor || !compositor->root_scene || !compositor->root_scene->is_dynamic_scene) return;
#ifndef GPAC_DISABLE_VRML
	gf_scene_toggle_addons(compositor->root_scene, show_addons);
#endif
}

#include <gpac/avparse.h>

GF_EXPORT
GF_Err gf_sc_set_speed(GF_Compositor *compositor, Fixed speed)
{
	GF_Fraction fps;
	u32 i, j;
	GF_SceneNamespace *ns;
	Bool restart = 0;
	if (!compositor || !speed) return GF_BAD_PARAM;

	u32 scene_time = gf_sc_get_time_in_ms(compositor);
	if (speed<0) {
		i=0;
		while ( (ns = (GF_SceneNamespace*)gf_list_enum(compositor->root_scene->namespaces, &i)) ) {
			u32 k=0;
			GF_ObjectManager *odm;
			if (!ns->owner || !ns->owner->subscene) continue;

			while ( (odm = gf_list_enum(ns->owner->subscene->resources, &k))) {
				const GF_PropertyValue *p = gf_filter_pid_get_property(odm->pid, GF_PROP_PID_PLAYBACK_MODE);
				if (!p || (p->value.uint!=GF_PLAYBACK_MODE_REWIND))
					return GF_NOT_SUPPORTED;
			}
		}
	}

	/*adjust all clocks on all services, if possible*/
	i=0;
	while ( (ns = (GF_SceneNamespace*)gf_list_enum(compositor->root_scene->namespaces, &i)) ) {
		GF_Clock *ck;
		ns->set_speed = speed;
		j=0;
		while (ns->clocks && (ck = (GF_Clock *)gf_list_enum(ns->clocks, &j)) ) {
			//we will have to reissue a PLAY command since playback direction changed
			if ( gf_mulfix(ck->speed,speed) < 0)
				restart = 1;
			gf_clock_set_speed(ck, speed);

			if (ns->owner) {
				gf_odm_set_speed(ns->owner, speed, GF_FALSE);
				if (ns->owner->subscene) {
					u32 k=0;
					GF_ObjectManager *odm;
					GF_Scene *scene = ns->owner->subscene;
					while ( (odm = gf_list_enum(scene->resources, &k))) {
						gf_odm_set_speed(odm, speed, GF_FALSE);
					}
				}
			}
		}
	}

	if (restart) {
		if (compositor->root_scene->is_dynamic_scene) {
			gf_scene_restart_dynamic(compositor->root_scene, scene_time, 0, 0);
		}
	}

	if (speed<0)
		speed = -speed;

	fps = compositor->fps;
	if (fps.den<1000) {
		fps.num = fps.num * (u32) (1000 * FIX2FLT(speed));
		fps.den *= 1000;
	} else {
		fps.num = (u32) (fps.num * FIX2FLT(speed));
	}
	gf_media_get_reduced_frame_rate(&fps.num, &fps.den);
	gf_sc_set_fps(compositor, fps);
	return GF_OK;
}

GF_EXPORT
Bool gf_sc_is_supported_url(GF_Compositor *compositor, const char *fileName, Bool use_parent_url)
{
	char *parent_url = NULL;
	if (!compositor || !compositor->root_scene || !compositor->root_scene->root_od || !compositor->root_scene->root_od->scene_ns)
		return GF_FALSE;

	if (use_parent_url)
		parent_url = compositor->root_scene->root_od->scene_ns->url;
	return gf_filter_is_supported_source(compositor->filter, fileName, parent_url);
}

#include <gpac/network.h>
/*get rendering option*/
static u32 gf_sc_get_option_internal(GF_Compositor *compositor, u32 type)
{
	if (!compositor) return 0;
	switch (type) {
	case GF_OPT_HAS_JAVASCRIPT:
		return gf_sg_has_scripting();
	case GF_OPT_IS_FINISHED:
		return gf_sc_check_end_of_scene(compositor, 0);
	case GF_OPT_IS_OVER:
		return gf_sc_check_end_of_scene(compositor, 1);
	case GF_OPT_MAIN_ADDON:
		return compositor->root_scene ? compositor->root_scene->main_addon_selected : 0;

	case GF_OPT_PLAY_STATE:
		if (compositor->step_mode) return GF_STATE_STEP_PAUSE;
		if (compositor->root_scene) {
			GF_Clock *ck = compositor->root_scene->root_od->ck;
			if (!ck) return GF_STATE_PAUSED;

//			if (ck->Buffering) return GF_STATE_PLAYING;
		}
		if (compositor->play_state != GF_STATE_PLAYING) return GF_STATE_PAUSED;
		return GF_STATE_PLAYING;
	case GF_OPT_CAN_SELECT_STREAMS:
		return (compositor->root_scene && compositor->root_scene->is_dynamic_scene) ? 1 : 0;
	case GF_OPT_HTTP_MAX_RATE:
	{
		GF_DownloadManager *dm = gf_filter_get_download_manager(compositor->filter);
		if (!dm) return 0;
		return gf_dm_get_data_rate(dm);
	}
	case GF_OPT_VIDEO_BENCH:
		return compositor->bench_mode ? GF_TRUE : GF_FALSE;
	case GF_OPT_ORIENTATION_SENSORS_ACTIVE:
		return compositor->orientation_sensors_active;
	default:
		return gf_sc_get_option(compositor, type);
	}
}

GF_EXPORT
void gf_sc_navigate_to(GF_Compositor *compositor, const char *toURL)
{
	if (!compositor) return;
	if (!toURL && !compositor->root_scene) return;

	if (compositor->reload_url) gf_free(compositor->reload_url);
	compositor->reload_url = NULL;

	if (toURL) {
		if (compositor->root_scene && compositor->root_scene->root_od && compositor->root_scene->root_od->scene_ns)
			compositor->reload_url = gf_url_concatenate(compositor->root_scene->root_od->scene_ns->url, toURL);
		if (!compositor->reload_url) compositor->reload_url = gf_strdup(toURL);
	}
	compositor->reload_state = 1;
}


static GF_Err gf_sc_step_clocks_intern(GF_Compositor *compositor, u32 ms_diff, Bool force_resume_pause)
{
	/*only play/pause if connected*/
	if (!compositor || !compositor->root_scene || !compositor->root_scene->root_od) return GF_BAD_PARAM;

	if (ms_diff) {
		u32 i, j;
		GF_SceneNamespace *ns;
		GF_Clock *ck;

		if (compositor->play_state == GF_STATE_PLAYING) return GF_BAD_PARAM;

		gf_sc_lock(compositor, 1);
		i = 0;
		while ((ns = (GF_SceneNamespace*)gf_list_enum(compositor->root_scene->namespaces, &i))) {
			j = 0;
			while (ns->clocks && (ck = (GF_Clock *)gf_list_enum(ns->clocks, &j))) {
				ck->init_timestamp += ms_diff;
				ck->media_ts_orig += ms_diff;
				ck->media_time_orig += ms_diff;
				//make sure we don't touch clock while doing resume/pause below
				if (force_resume_pause)
					ck->nb_paused++;
			}
		}
		compositor->step_mode = GF_TRUE;
		compositor->use_step_mode = GF_TRUE;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);

		//resume/pause to trigger codecs state change
		if (force_resume_pause) {
			mediacontrol_resume(compositor->root_scene->root_od, 0);
			mediacontrol_pause(compositor->root_scene->root_od);

			//release our safety
			i = 0;
			while ((ns = (GF_SceneNamespace*)gf_list_enum(compositor->root_scene->namespaces, &i))) {
				j = 0;
				while (ns->clocks && (ck = (GF_Clock *)gf_list_enum(ns->clocks, &j))) {
					ck->nb_paused--;
				}
			}
		}

		gf_sc_lock(compositor, 0);

	}
	return GF_OK;
}


static void gf_sc_set_play_state_internal(GF_Compositor *compositor, u32 PlayState, Bool reset_audio, Bool pause_clocks)
{
	Bool resume_live = 0;
	u32 prev_state;

	/*only play/pause if connected*/
	if (!compositor || !compositor->root_scene) return;

	prev_state = compositor->play_state;
	compositor->use_step_mode = GF_FALSE;

	if (PlayState==GF_STATE_PLAY_LIVE) {
		PlayState = GF_STATE_PLAYING;
		resume_live = 1;
		if (compositor->play_state == GF_STATE_PLAYING) {
			compositor->play_state = GF_STATE_PAUSED;
			mediacontrol_pause(compositor->root_scene->root_od);
		}
	}

	/*and if not already paused/playing*/
	if ((compositor->play_state == GF_STATE_PLAYING) && (PlayState == GF_STATE_PLAYING)) return;
	if ((compositor->play_state != GF_STATE_PLAYING) && (PlayState == GF_STATE_PAUSED)) return;

	/*pause compositor*/
	if ((PlayState==GF_STATE_PLAYING) && reset_audio)
		gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, 0xFF);
	else
		gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, PlayState);

	/* step mode specific */
	if (PlayState==GF_STATE_STEP_PAUSE) {
		if (prev_state==GF_STATE_PLAYING) {
			mediacontrol_pause(compositor->root_scene->root_od);
			compositor->play_state = GF_STATE_PAUSED;
		} else {
			u32 diff=1;
			if (compositor->ms_until_next_frame>0) diff = compositor->ms_until_next_frame;
			gf_sc_step_clocks_intern(compositor, diff, GF_TRUE);
		}
		return;
	}

	/* nothing to change*/
	if (compositor->play_state == PlayState) return;
	compositor->play_state = PlayState;

	if (compositor->root_scene->first_frame_pause_type && (PlayState == GF_STATE_PLAYING))
		compositor->root_scene->first_frame_pause_type = 0;

	if (!pause_clocks) return;

	if (PlayState != GF_STATE_PLAYING) {
		mediacontrol_pause(compositor->root_scene->root_od);
	} else {
		mediacontrol_resume(compositor->root_scene->root_od, resume_live);
	}

}



GF_EXPORT
u32 gf_sc_play_from_time(GF_Compositor *compositor, u64 from_time, u32 pause_at_first_frame)
{
	if (!compositor || !compositor->root_scene || !compositor->root_scene->root_od) return 0;
	if (compositor->root_scene->root_od->flags & GF_ODM_NO_TIME_CTRL) return 1;

	if (pause_at_first_frame==2) {
		if (gf_sc_get_option_internal(compositor, GF_OPT_PLAY_STATE) != GF_STATE_PLAYING)
			pause_at_first_frame = 1;
		else
			pause_at_first_frame = 0;
	}

	//in case we seek in non-player mode, adjust frame number to desired seek time
	//this is not really precise (we don't take into account seek) but is enough for vout UI
	if (!compositor->player) {
		u64 output_ts = gf_timestamp_rescale(from_time, 1000, compositor->fps.num);
		compositor->frame_number = (u32) (output_ts / compositor->fps.den);
	}

	/*for dynamic scene OD resources are static and all object use the same clock, so don't restart the root
	OD, just act as a mediaControl on all playing streams*/
	if (compositor->root_scene->is_dynamic_scene) {

		/*exit pause mode*/
		gf_sc_set_play_state_internal(compositor, GF_STATE_PLAYING, 1, 1);

		if (pause_at_first_frame)
			gf_sc_set_play_state_internal(compositor, GF_STATE_STEP_PAUSE, 0, 0);

		gf_sc_lock(compositor, 1);
		gf_scene_restart_dynamic(compositor->root_scene, from_time, 0, 0);
		gf_sc_lock(compositor, 0);
		return 2;
	}

	/*pause everything*/
	gf_sc_set_play_state_internal(compositor, GF_STATE_PAUSED, 0, 1);
	/*stop root*/
	gf_odm_stop(compositor->root_scene->root_od, 1);
	gf_scene_disconnect(compositor->root_scene, 0);

	compositor->root_scene->root_od->media_start_time = from_time;

	gf_odm_start(compositor->root_scene->root_od);
	gf_sc_set_play_state_internal(compositor, GF_STATE_PLAYING, 0, 1);
	if (pause_at_first_frame)
		gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
	return 2;
}

GF_EXPORT
void gf_sc_connect_from_time(GF_Compositor *compositor, const char *URL, u64 startTime, u32 pause_at_first_frame, Bool secondary_scene, const char *parent_path)
{
	GF_Scene *scene;
	GF_ObjectManager *odm;
	Bool is_self = GF_FALSE;
	if (!URL || !strlen(URL))
		is_self = GF_TRUE;

	if (compositor->root_scene) {
		if (is_self && !compositor->root_scene->root_od->ck) {
			odm = compositor->root_scene->root_od;
			odm->media_start_time = startTime;
			/*render first visual frame and pause*/
			if (pause_at_first_frame) {
				gf_sc_set_play_state_internal(compositor, GF_STATE_STEP_PAUSE, 0, 0);
				compositor->root_scene->first_frame_pause_type = pause_at_first_frame;
			}
			return;
		}

		if (compositor->root_scene->root_od && compositor->root_scene->root_od->scene_ns) {
			const char *main_url = compositor->root_scene->root_od->scene_ns->url;
			if (is_self || (main_url && !strcmp(main_url, URL))) {
				gf_sc_play_from_time(compositor, startTime, pause_at_first_frame);
				return;
			}
		}
		if (is_self) return;
		
		/*disconnect*/
		gf_sc_disconnect(compositor);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[Compositor] Connecting to %s\n", URL));

	assert(!compositor->root_scene);

	/*create a new scene*/
	scene = gf_scene_new(compositor, NULL);
	odm = gf_odm_new();
	scene->root_od = odm;
	odm->subscene = scene;
	//by default all scenes are dynamic, until we get a BIFS attached
	scene->is_dynamic_scene = GF_TRUE;

	odm->media_start_time = startTime;

	// we are not in compositor:process at this point of time since the compositor thread drives the compositor
	compositor->root_scene = scene;

	/*render first visual frame and pause*/
	if (pause_at_first_frame) {
		gf_sc_set_play_state_internal(compositor, GF_STATE_STEP_PAUSE, 0, 0);
		scene->first_frame_pause_type = pause_at_first_frame;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[Compositor] root scene created\n", URL));

	if (!strnicmp(URL, "views://", 8)) {
		gf_scene_generate_views(compositor->root_scene, (char *) URL+8, (char*)parent_path);
		return;
	}
	else if (!strnicmp(URL, "mosaic://", 9)) {
		gf_scene_generate_mosaic(compositor->root_scene, (char *) URL+9, (char*)parent_path);
		return;
	}

	gf_scene_ns_connect_object(scene, odm, (char *) URL, (char*)parent_path, NULL);
}

GF_EXPORT
void gf_sc_disconnect(GF_Compositor *compositor)
{
	/*resume*/
	if (compositor->play_state != GF_STATE_PLAYING) gf_sc_set_play_state_internal(compositor, GF_STATE_PLAYING, 1, 1);

	if (compositor->root_scene && compositor->root_scene->root_od) {
		GF_ObjectManager *root = compositor->root_scene->root_od;
		gf_sc_lock(compositor, GF_TRUE);
		compositor->root_scene = NULL;
		gf_odm_disconnect(root, 2);
		gf_sc_lock(compositor, GF_FALSE);
	}
}


#include <gpac/scene_manager.h>

static Bool check_in_scene(GF_Scene *scene, GF_ObjectManager *odm)
{
	u32 i;
	GF_ObjectManager *ptr, *root;
	if (!scene) return 0;
	root = scene->root_od;
	if (odm == root) return 1;
	scene = root->subscene;

	i=0;
	while ((ptr = (GF_ObjectManager *)gf_list_enum(scene->resources, &i))) {
		if (ptr == odm) return 1;
		if (check_in_scene(ptr->subscene, odm)) return 1;
	}
	return 0;
}

GF_EXPORT
GF_Err gf_sc_dump_scene_ex(GF_Compositor *compositor, char *rad_name, char **filename, Bool xml_dump, Bool skip_protos, GF_ObjectManager *scene_od)
{
#ifndef GPAC_DISABLE_SCENE_DUMP
	GF_SceneGraph *sg;
	GF_ObjectManager *odm;
	GF_SceneDumper *dumper;
	GF_List *extra_graphs;
	u32 mode;
	u32 i;
	char *ext;
	GF_Err e;

	if (!compositor || !compositor->root_scene) return GF_BAD_PARAM;
	if (!scene_od) {
		if (!compositor->root_scene) return GF_BAD_PARAM;
		odm = compositor->root_scene->root_od;
	} else {
		odm = scene_od;
		if (! check_in_scene(compositor->root_scene, scene_od))
			odm = compositor->root_scene->root_od;
	}

	if (odm->subscene) {
		if (!odm->subscene->graph) return GF_IO_ERR;
		sg = odm->subscene->graph;
		extra_graphs = odm->subscene->extra_scenes;
	} else {
		if (!odm->parentscene->graph) return GF_IO_ERR;
		sg = odm->parentscene->graph;
		extra_graphs = odm->parentscene->extra_scenes;
	}

	mode = xml_dump ? GF_SM_DUMP_AUTO_XML : GF_SM_DUMP_AUTO_TXT;
	/*figure out best dump format based on extension*/
	ext = odm->scene_ns ? gf_file_ext_start(odm->scene_ns->url) : NULL;
	if (ext) {
		char szExt[20];
		strcpy(szExt, ext);
		strlwr(szExt);
		if (!strcmp(szExt, ".wrl")) mode = xml_dump ? GF_SM_DUMP_X3D_XML : GF_SM_DUMP_VRML;
		else if(!strncmp(szExt, ".x3d", 4) || !strncmp(szExt, ".x3dv", 5) ) mode = xml_dump ? GF_SM_DUMP_X3D_XML : GF_SM_DUMP_X3D_VRML;
		else if(!strncmp(szExt, ".bt", 3) || !strncmp(szExt, ".xmt", 4) || !strncmp(szExt, ".mp4", 4) ) mode = xml_dump ? GF_SM_DUMP_XMTA : GF_SM_DUMP_BT;
	}

	dumper = gf_sm_dumper_new(sg, rad_name, GF_FALSE, ' ', mode);

	if (!dumper) return GF_IO_ERR;
	e = gf_sm_dump_graph(dumper, skip_protos, 0);
	for (i = 0; i < gf_list_count(extra_graphs); i++) {
		GF_SceneGraph *extra = (GF_SceneGraph *)gf_list_get(extra_graphs, i);
		gf_sm_dumper_set_extra_graph(dumper, extra);
		e = gf_sm_dump_graph(dumper, skip_protos, 0);
	}
#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		gf_sm_dumper_set_extra_graph(dumper, NULL);
		gf_sm_dump_get_name(dumper);
	}
#endif

	if (filename) *filename = gf_strdup(gf_sm_dump_get_name(dumper));
	gf_sm_dumper_del(dumper);
	return e;
#else
	return GF_NOT_SUPPORTED;
#endif
}
GF_EXPORT
GF_Err gf_sc_dump_scene(GF_Compositor *compositor, char *rad_name, char **filename, Bool xml_dump, Bool skip_protos)
{
	return gf_sc_dump_scene_ex(compositor, rad_name, filename, xml_dump, skip_protos, NULL);
}

#include <gpac/internal/scenegraph_dev.h>

GF_EXPORT
GF_Err gf_sc_scene_update(GF_Compositor *compositor, char *type, char *com)
{
#ifndef GPAC_DISABLE_SMGR
	GF_Err e;
	GF_StreamContext *sc;
	Bool is_xml = 0;
	Double time = 0;
	u32 i, tag;
	GF_SceneLoader load;

	if (!compositor || !com) return GF_BAD_PARAM;

	if (type && (!stricmp(type, "application/ecmascript") || !stricmp(type, "js")) )  {
#if defined(GPAC_HAS_QJS) && !defined(GPAC_DISABLE_SVG)
		u32 tag;
		GF_Node *root = gf_sg_get_root_node(compositor->root_scene->graph);
		if (!root) return GF_BAD_PARAM;
		tag = gf_node_get_tag(root);
		if (tag >= GF_NODE_RANGE_FIRST_SVG) {
			if (compositor->root_scene->graph->svg_js) {
				GF_Err svg_exec_script(struct __tag_svg_script_ctx *svg_js, GF_SceneGraph *sg, const char *com);
				return svg_exec_script(compositor->root_scene->graph->svg_js, compositor->root_scene->graph, (char *)com);
			}
			return GF_NOT_FOUND;
		}
		return GF_NOT_SUPPORTED;
#else
		return GF_NOT_SUPPORTED;
#endif
	}

	if (!type && !strncmp(com, "gpac ", 5)) {
		com += 5;
		//new add-on
		if (compositor->root_scene && !strncmp(com, "add ", 4)) {
			GF_AssociatedContentLocation addon_info;
			memset(&addon_info, 0, sizeof(GF_AssociatedContentLocation));
			addon_info.external_URL = com + 4;
			addon_info.timeline_id = -100;
			gf_scene_register_associated_media(compositor->root_scene, &addon_info);
			return GF_OK;
		}
		//new splicing add-on
		if (compositor->root_scene && !strncmp(com, "splice ", 7)) {
			char *sep;
			Double start, end;
			Bool is_pts = GF_FALSE;
			GF_AssociatedContentLocation addon_info;
			memset(&addon_info, 0, sizeof(GF_AssociatedContentLocation));
			com += 7;
			if (!strnicmp(com, "pts ", 4)) {
				is_pts = GF_TRUE;
				com += 4;
			}
			sep = strchr(com, ':');
			start = 0;
			end = -1;
			if (sep) {
				sep[0]=0;
				if (sscanf(com, "%lf-%lf", &start, &end) != 2) {
					end = -1;
					sscanf(com, "%lf", &start);
				}
				sep[0]=':';
				addon_info.external_URL = sep+1;
			}
			//splice end, locate first splice with no end set and set it
			else if (sscanf(com, "%lf", &end)==1) {
				u32 count = gf_list_count(compositor->root_scene->declared_addons);
				for (i=0; i<count; i++) {
					GF_AddonMedia *addon = gf_list_get(compositor->root_scene->declared_addons, i);
					if (addon->is_splicing && (addon->splice_end<0) ) {
						addon->splice_end = end;
						break;
					}
				}
				return GF_OK;
			} else {
				//splice now, until end of spliced media
				addon_info.external_URL = com;
			}
			addon_info.is_splicing = GF_TRUE;
			addon_info.timeline_id = -100;
			addon_info.splice_start_time = start;
			addon_info.splice_end_time = end;
			addon_info.splice_time_pts = is_pts;
			gf_scene_register_associated_media(compositor->root_scene, &addon_info);
			return GF_OK;
		}
		//select object
		if (compositor->root_scene && !strncmp(com, "select ", 7)) {
			u32 idx = atoi(com+7);
			gf_scene_select_object(compositor->root_scene, gf_list_get(compositor->root_scene->resources, idx));
			return GF_OK;
		}
		return GF_OK;
	}

	memset(&load, 0, sizeof(GF_SceneLoader));
	load.localPath = gf_opts_get_key("core", "cache");
	load.flags = GF_SM_LOAD_FOR_PLAYBACK | GF_SM_LOAD_CONTEXT_READY;
	load.type = GF_SM_LOAD_BT;

	if (!compositor->root_scene) {

		/*create a new scene*/
		compositor->root_scene = gf_scene_new(compositor, NULL);
		compositor->root_scene->root_od = gf_odm_new();
		compositor->root_scene->root_od->parentscene = NULL;
		compositor->root_scene->root_od->subscene = compositor->root_scene;

		load.ctx = gf_sm_new(compositor->root_scene->graph);
	} else if (compositor->root_scene->root_od) {
		u32 nb_ch = 0;
		load.ctx = gf_sm_new(compositor->root_scene->graph);

		if (compositor->root_scene->root_od->pid) nb_ch++;
		if (compositor->root_scene->root_od->extra_pids) nb_ch += gf_list_count(compositor->root_scene->root_od->extra_pids);
		for (i=0; i<nb_ch; i++) {
			u32 stream_type, es_id, oti;
			const GF_PropertyValue *p;
			GF_FilterPid *pid = compositor->root_scene->root_od->pid;
			if (i) {
				GF_ODMExtraPid *xpid = gf_list_get(compositor->root_scene->root_od->extra_pids, i-1);
				pid = xpid->pid;
			}
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
			if (!p) continue;
			stream_type = p->value.uint;
			switch (stream_type) {
			case GF_STREAM_OD:
			case GF_STREAM_SCENE:
			case GF_STREAM_PRIVATE_SCENE:
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
				if (!p)
					p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
				es_id = p ? p->value.uint : 0;
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
				oti = p ? p->value.uint : 1;

				sc = gf_sm_stream_new(load.ctx, es_id, stream_type, oti);
				if (stream_type==GF_STREAM_PRIVATE_SCENE) sc->streamType = GF_STREAM_SCENE;
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
				sc->timeScale = p ? p->value.uint : 1000;
				break;
			}
		}
	} else {
		return GF_BAD_PARAM;
	}
	load.ctx->max_node_id = gf_sg_get_max_node_id(compositor->root_scene->graph);

	i=0;
	while ((com[i] == ' ') || (com[i] == '\r') || (com[i] == '\n') || (com[i] == '\t')) i++;
	if (com[i]=='<') is_xml = 1;

	load.type = is_xml ? GF_SM_LOAD_XMTA : GF_SM_LOAD_BT;
	time = gf_scene_get_time(compositor->root_scene);


	if (type && (!stricmp(type, "application/x-laser+xml") || !stricmp(type, "laser"))) {
		load.type = GF_SM_LOAD_XSR;
		time = gf_scene_get_time(compositor->root_scene);
	}
	else if (type && (!stricmp(type, "image/svg+xml") || !stricmp(type, "svg")) ) {
		load.type = GF_SM_LOAD_XSR;
		time = gf_scene_get_time(compositor->root_scene);
	}
	else if (type && (!stricmp(type, "model/x3d+xml") || !stricmp(type, "x3d")) ) load.type = GF_SM_LOAD_X3D;
	else if (type && (!stricmp(type, "model/x3d+vrml") || !stricmp(type, "x3dv")) ) load.type = GF_SM_LOAD_X3DV;
	else if (type && (!stricmp(type, "model/vrml") || !stricmp(type, "vrml")) ) load.type = GF_SM_LOAD_VRML;
	else if (type && (!stricmp(type, "application/x-xmt") || !stricmp(type, "xmt")) ) load.type = GF_SM_LOAD_XMTA;
	else if (type && (!stricmp(type, "application/x-bt") || !stricmp(type, "bt")) ) load.type = GF_SM_LOAD_BT;
	else if (gf_sg_get_root_node(compositor->root_scene->graph)) {
		tag = gf_node_get_tag(gf_sg_get_root_node(compositor->root_scene->graph));
		if (tag >= GF_NODE_RANGE_FIRST_SVG) {
			load.type = GF_SM_LOAD_XSR;
			time = gf_scene_get_time(compositor->root_scene);
		} else if (tag>=GF_NODE_RANGE_FIRST_X3D) {
			load.type = is_xml ? GF_SM_LOAD_X3D : GF_SM_LOAD_X3DV;
		} else {
			load.type = is_xml ? GF_SM_LOAD_XMTA : GF_SM_LOAD_BT;
			time = gf_scene_get_time(compositor->root_scene);
		}
	}

	e = gf_sm_load_init(&load);
	if (!e) e = gf_sm_load_string(&load, com, 1);
	gf_sm_load_done(&load);
	if (!e) {
		u32 j, au_count, st_count;
		st_count = gf_list_count(load.ctx->streams);
		for (i=0; i<st_count; i++) {
			sc = (GF_StreamContext*)gf_list_get(load.ctx->streams, i);
			au_count = gf_list_count(sc->AUs);
			for (j=0; j<au_count; j++) {
				GF_AUContext *au = (GF_AUContext *)gf_list_get(sc->AUs, j);
				e = gf_sg_command_apply_list(compositor->root_scene->graph, au->commands, time);
				if (e) break;
			}
		}
	}
	if (!compositor->root_scene->graph_attached) {
		if (!load.ctx->scene_width || !load.ctx->scene_height) {
//			load.ctx->scene_width = term->compositor->width;
//			load.ctx->scene_height = term->compositor->height;
		}
		gf_sg_set_scene_size_info(compositor->root_scene->graph, load.ctx->scene_width, load.ctx->scene_height, load.ctx->is_pixel_metrics);
		gf_scene_attach_to_compositor(compositor->root_scene);
	}
	gf_sm_del(load.ctx);
	return e;
#else
	return GF_NOT_SUPPORTED;
#endif
}

GF_EXPORT
void gf_sc_select_service(GF_Compositor *compositor, u32 service_id)
{
	if (!compositor || !compositor->root_scene) return;
#ifndef GPAC_DISABLE_VRML
	gf_scene_set_service_id(compositor->root_scene, service_id);
#endif
}


GF_EXPORT
const char *gf_sc_get_url(GF_Compositor *compositor)
{
	if (!compositor || !compositor->root_scene || !compositor->root_scene->root_od || !compositor->root_scene->root_od->scene_ns) return NULL;
	return compositor->root_scene->root_od->scene_ns->url;
}

GF_EXPORT
const void *gf_sc_get_main_pid(GF_Compositor *compositor)
{
	if (!compositor || !compositor->root_scene) return NULL;
	GF_ObjectManager *odm = compositor->root_scene->root_od;
	if (odm->pid) return odm->pid;

	if (odm->subscene) {
		GF_ObjectManager *anodm = gf_list_get(odm->subscene->resources, 0);
		if (!anodm) return NULL;
		return anodm->pid;
	}
	return NULL;
}

GF_EXPORT
const char *gf_sc_get_world_info(GF_Compositor *compositor, GF_List *descriptions)
{
	GF_Node *info;
	if (!compositor || !compositor->root_scene || !compositor->root_scene->world_info) return NULL;
	info = (GF_Node*) compositor->root_scene->world_info;

#ifndef GPAC_DISABLE_SVG
	if (gf_node_get_tag(info) == TAG_SVG_title) {
		SVG_Element *elt = (SVG_Element *)info;
		GF_ChildNodeItem *l = elt->children;
		while (l) {
			if (gf_node_get_tag(l->node)==TAG_DOMText) {
				GF_DOMText *txt = (GF_DOMText *)l->node;
				return txt->textContent;
			}
			l = l->next;
		}
	} else
#endif
	{
#ifndef GPAC_DISABLE_VRML
		M_WorldInfo *wi = (M_WorldInfo *) info;
		if (descriptions) {
			u32 i;
			for (i=0; i<wi->info.count; i++) {
				gf_list_add(descriptions, wi->info.vals[i]);
			}
		}
		return wi->title.buffer;
#endif
	}
	return "GPAC";
}

GF_EXPORT
GF_Err gf_sc_add_object(GF_Compositor *compositor, const char *url, Bool auto_play)
{
	GF_MediaObject *mo=NULL;
	//this needs refinement
	SFURL sfurl;
	MFURL mfurl;
	if (!url || !compositor->root_scene || !compositor->root_scene->is_dynamic_scene) return GF_BAD_PARAM;

	sfurl.OD_ID = GF_MEDIA_EXTERNAL_ID;
	sfurl.url = (char *) url;
	mfurl.count = 1;
	mfurl.vals = &sfurl;
	/*only text tracks are supported for now...*/
	mo = gf_scene_get_media_object(compositor->root_scene, &mfurl, GF_MEDIA_OBJECT_TEXT, 1);
	if (mo) {
		/*check if we must deactivate it*/
		if (mo->odm) {
			if (mo->num_open && !auto_play) {
				gf_scene_select_object(compositor->root_scene, mo->odm);
			}
		} else {
			gf_list_del_item(compositor->root_scene->scene_objects, mo);
			gf_sg_vrml_mf_reset(&mo->URLs, GF_SG_VRML_MFURL);
			gf_free(mo);
			mo = NULL;
		}
	}
	return mo ? GF_OK : GF_NOT_SUPPORTED;
}


GF_EXPORT
Double gf_sc_get_simulation_frame_rate(GF_Compositor *compositor, u32 *nb_frames_drawn)
{
	Double fps;
	if (!compositor) return 0.0;
	if (nb_frames_drawn) *nb_frames_drawn = compositor->frame_number;
	fps = compositor->fps.num;
	fps /= compositor->fps.den;
	return fps;
}

GF_EXPORT
u32 gf_sc_get_elapsed_time_in_ms(GF_Compositor *compositor)
{
	u32 i, count;
	if (!compositor || !compositor->root_scene) return 0;

	count = gf_list_count(compositor->root_scene->namespaces);
	for (i=0; i<count; i++) {
		GF_SceneNamespace *sns = gf_list_get(compositor->root_scene->namespaces, i);
		GF_Clock *ck = gf_list_get(sns->clocks, 0);
		if (ck) return gf_clock_elapsed_time(ck);
	}
	return 0;
}

GF_EXPORT
u32 gf_sc_get_current_service_id(GF_Compositor *compositor)
{
	SFURL *the_url;
	GF_MediaObject *mo;
	if (!compositor || !compositor->root_scene) return 0;
	if (!compositor->root_scene->is_dynamic_scene) return compositor->root_scene->root_od->ServiceID;

	if (compositor->root_scene->visual_url.OD_ID || compositor->root_scene->visual_url.url)
		the_url = &compositor->root_scene->visual_url;
	else
		the_url = &compositor->root_scene->audio_url;

	mo = gf_scene_find_object(compositor->root_scene, the_url->OD_ID, the_url->url);
	if (mo && mo->odm) return mo->odm->ServiceID;
	return 0;
}
