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

#include <gpac/internal/compositor_dev.h>
/*for user and terminal API (options and InputSensor)*/
#include <gpac/internal/terminal_dev.h>
#include <gpac/options.h>
#include <gpac/utf.h>
#include <gpac/modules/hardcoded_proto.h>

#include "nodes_stacks.h"

#include "visual_manager.h"
#include "texturing.h"

static void gf_sc_recompute_ar(GF_Compositor *compositor, GF_Node *top_node);

#define SC_DEF_WIDTH	320
#define SC_DEF_HEIGHT	240

void gf_sc_next_frame_state(GF_Compositor *compositor, u32 state)
{
//	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Forcing frame redraw state: %d\n", state));
	if (state==GF_SC_DRAW_FLUSH) {
		if (!compositor->skip_flush)
			compositor->skip_flush = 2;
		
		//if in openGL mode ignore refresh events (content of the window is still OK). This is only used for overlays in 2d
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
		gf_term_send_event(compositor->term, &evt);
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
thread accessing the HW ressources
*/
static void gf_sc_reconfig_task(GF_Compositor *compositor)
{
	GF_Event evt;
	Bool notif_size=GF_FALSE;
	u32 width,height;

	/*reconfig audio if needed (non-threaded compositors)*/
	if (compositor->audio_renderer && !compositor->audio_renderer->th) gf_sc_ar_reconfig(compositor->audio_renderer);

	if (compositor->msg_type) {

		compositor->msg_type |= GF_SR_IN_RECONFIG;

		if (compositor->msg_type & GF_SR_CFG_INITIAL_RESIZE) {
			GF_Event evt;
			memset(&evt, 0, sizeof(GF_Event));
			evt.type = GF_EVENT_VIDEO_SETUP;
			evt.setup.width = compositor->new_width;
			evt.setup.height = compositor->new_height;
			evt.setup.system_memory = compositor->video_memory ? GF_FALSE : GF_TRUE;
			
#ifdef OPENGL_RASTER
			if (compositor->opengl_raster) {
				evt.setup.opengl_mode = 1;
				evt.setup.system_memory = GF_FALSE;
				evt.setup.back_buffer = GF_TRUE;
			}
#endif
			
#ifndef GPAC_DISABLE_3D
			if (compositor->hybrid_opengl || compositor->force_opengl_2d) {
				evt.setup.opengl_mode = 1;
				evt.setup.system_memory = GF_FALSE;
				evt.setup.back_buffer = GF_TRUE;
			}
			
#endif
			compositor->video_out->ProcessEvent(compositor->video_out, &evt);		
			compositor->msg_type &= ~GF_SR_CFG_INITIAL_RESIZE;
		}
		/*scene size has been overriden*/
		if (compositor->msg_type & GF_SR_CFG_OVERRIDE_SIZE) {
			GF_Event evt;

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
			gf_term_send_event(compositor->term, &evt);
		}
		/*size changed from scene cfg: resize window first*/
		if (compositor->msg_type & GF_SR_CFG_SET_SIZE) {
			u32 fs_width, fs_height;
			Bool restore_fs = compositor->fullscreen;

			GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Changing display size to %d x %d\n", compositor->new_width, compositor->new_height));
			fs_width = fs_height = 0;
			if (restore_fs) {
#if defined(GPAC_ANDROID) || defined(GPAC_IPHONE)
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
	if (notif_size && compositor->video_listeners) {
		u32 k=0;
		GF_VideoListener *l;
		while ((l = gf_list_enum(compositor->video_listeners, &k))) {
			if ( compositor->user && (compositor->user->init_flags & GF_TERM_WINDOW_TRANSPARENT) )
				l->on_video_reconfig(l->udta, compositor->display_width, compositor->display_height, 4);
			else
				l->on_video_reconfig(l->udta, compositor->display_width, compositor->display_height, 3);
		}
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

GF_EXPORT
Bool gf_sc_draw_frame(GF_Compositor *compositor, Bool no_flush, s32 *ms_till_next)
{
	Bool ret = GF_FALSE;

	if (no_flush)
		compositor->skip_flush=1;

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
	else if (compositor->fonts_pending) ret = GF_TRUE;

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

static Bool gf_sc_on_event(void *cbck, GF_Event *event);

static Bool gf_sc_set_check_raster2d(GF_Compositor *compositor, GF_Raster2D *ifce)
{
	/*check base*/
	if (!ifce->stencil_new || !ifce->surface_new) return 0;
	/*if these are not set we cannot draw*/
	if (!ifce->surface_clear || !ifce->surface_set_path || !ifce->surface_fill) return 0;
	/*check we can init a surface with the current driver (the rest is optional)*/
	if (ifce->surface_attach_to_buffer) return 1;
	return GF_FALSE;
}

static GF_Err gf_sc_load(GF_Compositor *compositor)
{
	compositor->strike_bank = gf_list_new();
	compositor->visuals = gf_list_new();

	GF_SAFEALLOC(compositor->traverse_state, GF_TraverseState);
	if (!compositor->traverse_state) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPOSE, ("[Compositor] Failed to initilaize compositor\n"));
		return GF_OUT_OF_MEM;
	}

	compositor->traverse_state->vrml_sensors = gf_list_new();
	compositor->traverse_state->use_stack = gf_list_new();
#ifndef GPAC_DISABLE_3D
	compositor->traverse_state->local_lights = gf_list_new();
#endif
	compositor->sensors = gf_list_new();
	compositor->previous_sensors = gf_list_new();
	compositor->hit_use_stack = gf_list_new();
	compositor->prev_hit_use_stack = gf_list_new();
	compositor->focus_ancestors = gf_list_new();
	compositor->focus_use_stack = gf_list_new();

	compositor->env_tests = gf_list_new();

	/*setup main visual*/
	compositor->visual = visual_new(compositor);
	compositor->visual->GetSurfaceAccess = compositor_2d_get_video_access;
	compositor->visual->ReleaseSurfaceAccess = compositor_2d_release_video_access;
	compositor->visual->CheckAttached = compositor_2d_check_attached;
	compositor->visual->ClearSurface = compositor_2d_clear_surface;

	if (compositor->video_out->FlushRectangles)
		compositor->visual->direct_flush = GF_TRUE;

	compositor_2d_init_callbacks(compositor);

	// default value for depth gain is not zero
#ifdef GF_SR_USE_DEPTH
	compositor->traverse_state->depth_gain = FIX_ONE;
#endif

	gf_list_add(compositor->visuals, compositor->visual);

	compositor->zoom = compositor->scale_x = compositor->scale_y = FIX_ONE;

	/*create a drawable for focus highlight*/
	compositor->focus_highlight = drawable_new();
	/*associate a dummy node for traversing*/
	compositor->focus_highlight->node = gf_node_new(NULL, TAG_UndefinedNode);
	gf_node_register(compositor->focus_highlight->node, NULL);
	gf_node_set_callback_function(compositor->focus_highlight->node, drawable_traverse_focus);


#ifndef GPAC_DISABLE_3D
	/*default collision mode*/
	compositor->collide_mode = GF_COLLISION_DISPLACEMENT; //GF_COLLISION_NORMAL
	compositor->gravity_on = GF_TRUE;
	
	/*create default unit sphere and box for bounds*/
	compositor->unit_bbox = new_mesh();
	mesh_new_unit_bbox(compositor->unit_bbox);
	gf_mx_init(compositor->traverse_state->model_matrix);
#endif

	compositor->was_system_memory=1;
	return GF_OK;
}


static GF_Err gf_sc_create(GF_Compositor *compositor)
{
	const char *sOpt;

	/*load video out*/
	sOpt = gf_cfg_get_key(compositor->user->config, "Video", "DriverName");
	if (sOpt) {
		compositor->video_out = (GF_VideoOutput *) gf_modules_load_interface_by_name(compositor->user->modules, sOpt, GF_VIDEO_OUTPUT_INTERFACE);
		if (compositor->video_out) {
			compositor->video_out->evt_cbk_hdl = compositor;
			compositor->video_out->on_event = gf_sc_on_event;
			/*init hw*/
			if (!compositor->video_out->Setup || compositor->video_out->Setup(compositor->video_out, compositor->user->os_window_handler, compositor->user->os_display, compositor->user->init_flags) != GF_OK) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Failed to Setup Video Driver %s!\n", sOpt));
				gf_modules_close_interface((GF_BaseInterface *)compositor->video_out);
				compositor->video_out = NULL;
			}
		}
	}

	if (!compositor->video_out) {
		GF_VideoOutput *raw_out = NULL;
		u32 i, count = gf_modules_get_count(compositor->user->modules);
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Trying to find a suitable video driver amongst %d modules...\n", count));
		for (i=0; i<count; i++) {
			compositor->video_out = (GF_VideoOutput *) gf_modules_load_interface(compositor->user->modules, i, GF_VIDEO_OUTPUT_INTERFACE);
			if (!compositor->video_out) continue;
			compositor->video_out->evt_cbk_hdl = compositor;
			compositor->video_out->on_event = gf_sc_on_event;
			//in enum mode, only use raw out if everything else failed ...
			if (!stricmp(compositor->video_out->module_name, "Raw Video Output")) {
				raw_out = compositor->video_out;
				compositor->video_out = NULL;
				continue;
			}

			/*init hw*/
			if (compositor->video_out->Setup && compositor->video_out->Setup(compositor->video_out, compositor->user->os_window_handler, compositor->user->os_display, compositor->user->init_flags)==GF_OK) {
				gf_cfg_set_key(compositor->user->config, "Video", "DriverName", compositor->video_out->module_name);
				break;
			}
			gf_modules_close_interface((GF_BaseInterface *)compositor->video_out);
			compositor->video_out = NULL;
		}
		if (raw_out) {
			if (compositor->video_out) gf_modules_close_interface((GF_BaseInterface *)raw_out);
			else {
				compositor->video_out = raw_out;
				compositor->video_out ->Setup(compositor->video_out, compositor->user->os_window_handler, compositor->user->os_display, compositor->user->init_flags);
			}
		}
	}
	if (!compositor->video_out ) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Failed to create compositor->video_out, did not find any suitable driver.\n"));
		return GF_IO_ERR;
	}

	sOpt = gf_cfg_get_key(compositor->user->config, "Video", "DPI");
	if (sOpt) {
		compositor->video_out->dpi_x = compositor->video_out->dpi_y = atoi(sOpt);
	}

	/*try to load a raster driver*/
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "Raster2D");
	if (sOpt) {
		compositor->rasterizer = (GF_Raster2D *) gf_modules_load_interface_by_name(compositor->user->modules, sOpt, GF_RASTER_2D_INTERFACE);
		if (!compositor->rasterizer) {
			sOpt = NULL;
		} else if (!gf_sc_set_check_raster2d(compositor, compositor->rasterizer)) {
			gf_modules_close_interface((GF_BaseInterface *)compositor->rasterizer);
			compositor->rasterizer = NULL;
			sOpt = NULL;
		}
	}
	if (!compositor->rasterizer) {
		u32 i, count;
		count = gf_modules_get_count(compositor->user->modules);
		for (i=0; i<count; i++) {
			compositor->rasterizer = (GF_Raster2D *) gf_modules_load_interface(compositor->user->modules, i, GF_RASTER_2D_INTERFACE);
			if (!compositor->rasterizer) continue;
			if (gf_sc_set_check_raster2d(compositor, compositor->rasterizer)) break;
			gf_modules_close_interface((GF_BaseInterface *)compositor->rasterizer);
			compositor->rasterizer = NULL;
		}
		if (compositor->rasterizer) gf_cfg_set_key(compositor->user->config, "Compositor", "Raster2D", compositor->rasterizer->module_name);
	}
	if (!compositor->rasterizer) {
		if (compositor->video_out->Shutdown) compositor->video_out->Shutdown(compositor->video_out);
		gf_modules_close_interface((GF_BaseInterface *)compositor->video_out);
		compositor->video_out = NULL;
		return GF_IO_ERR;
	}

	/*and init*/
	if (gf_sc_load(compositor) != GF_OK) {
		gf_modules_close_interface((GF_BaseInterface *)compositor->rasterizer);
		compositor->rasterizer = NULL;
		compositor->video_out->Shutdown(compositor->video_out);
		gf_modules_close_interface((GF_BaseInterface *)compositor->video_out);
		compositor->video_out = NULL;
		return GF_IO_ERR;
	}

	compositor->textures = gf_list_new();
	compositor->textures_gc = gf_list_new();
	compositor->frame_rate = 30.0;
	compositor->frame_duration = 33;
	compositor->time_nodes = gf_list_new();
	compositor->event_queue = gf_list_new();
	compositor->event_queue_back = gf_list_new();
	compositor->evq_mx = gf_mx_new("EventQueue");

#ifdef GF_SR_USE_VIDEO_CACHE
	compositor->cached_groups = gf_list_new();
	compositor->cached_groups_queue = gf_list_new();
#endif

	/*load audio renderer*/
	if (!compositor->audio_renderer)
		compositor->audio_renderer = gf_sc_ar_load(compositor->user);

	gf_sc_reset_framerate(compositor);
	compositor->font_manager = gf_font_manager_new(compositor->user);

	compositor->extra_scenes = gf_list_new();
	compositor->interaction_level = GF_INTERACT_NORMAL | GF_INTERACT_INPUT_SENSOR | GF_INTERACT_NAVIGATION;

	compositor->scene_sampled_clock = 0;
	compositor->video_th_id = gf_th_id();

	gf_sc_set_option(compositor, GF_OPT_RELOAD_CONFIG, 1);
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

	return GF_OK;
}

enum
{
	GF_COMPOSITOR_THREAD_START = 0,
	GF_COMPOSITOR_THREAD_RUN,
	GF_COMPOSITOR_THREAD_ABORTING,
	GF_COMPOSITOR_THREAD_DONE,
	GF_COMPOSITOR_THREAD_INIT_FAILED,
};

static u32 gf_sc_proc(void *par)
{
	GF_Err e;
	GF_Compositor *compositor = (GF_Compositor *) par;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Compositor] Entering thread ID %d\n", gf_th_id() ));

	compositor->video_th_state = GF_COMPOSITOR_THREAD_START;
	e = gf_sc_create(compositor);
	if (e != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Compositor] Failed to initialize compositor: %s\n", gf_error_to_string(e) ));
		compositor->video_th_state = GF_COMPOSITOR_THREAD_INIT_FAILED;
		return 1;
	}

	compositor->video_th_state = GF_COMPOSITOR_THREAD_RUN;
	while (compositor->video_th_state == GF_COMPOSITOR_THREAD_RUN) {
		//simulation tick is self-regulating. Call it regardless of the visibility status
		gf_sc_render_frame(compositor);
	}

#ifndef GPAC_DISABLE_3D
	visual_3d_reset_graphics(compositor->visual);
	compositor_2d_reset_gl_auto(compositor);
#endif
	gf_sc_texture_cleanup_hw(compositor);


	/*destroy video out here if we're using openGL, to avoid threading issues*/
	compositor->video_out->Shutdown(compositor->video_out);
	gf_modules_close_interface((GF_BaseInterface *)compositor->video_out);
	compositor->video_out = NULL;
	compositor->video_th_state = GF_COMPOSITOR_THREAD_DONE;
	return 0;
}

GF_EXPORT
GF_Compositor *gf_sc_new(GF_User *user, Bool self_threaded, GF_Terminal *term)
{
	GF_Err e;
	GF_Compositor *tmp;

	GF_SAFEALLOC(tmp, GF_Compositor);
	if (!tmp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Failed to allocate compositor : OUT OF MEMORY!\n"));
		return NULL;
	}
	tmp->user = user;
	tmp->term = term;
	tmp->mx = gf_mx_new("Compositor");

	/*load proto modules*/
	if (user) {
		u32 i;
		tmp->proto_modules = gf_list_new();
		for (i=0; i< gf_modules_get_count(user->modules); i++) {
			GF_HardcodedProto *ifce = (GF_HardcodedProto *) gf_modules_load_interface(user->modules, i, GF_HARDCODED_PROTO_INTERFACE);
			if (ifce) gf_list_add(tmp->proto_modules, ifce);
		}
	}

	/*force initial for 2D/3D setup*/
	tmp->msg_type |= GF_SR_CFG_INITIAL_RESIZE;
	/*set default size if owning output*/
	if (tmp->user && !tmp->user->os_window_handler) {
		const char *opt;
		tmp->new_width = SC_DEF_WIDTH;
		tmp->new_height = SC_DEF_HEIGHT;
		opt = gf_cfg_get_key(user->config, "Compositor", "DefaultWidth");
		if (opt) tmp->new_width = atoi(opt);
		opt = gf_cfg_get_key(user->config, "Compositor", "DefaultHeight");
		if (opt) tmp->new_height = atoi(opt);

		tmp->msg_type |= GF_SR_CFG_SET_SIZE;
	}


	if (self_threaded) {

		tmp->VisualThread = gf_th_new("Compositor");
		gf_th_run(tmp->VisualThread, gf_sc_proc, tmp);

		/*wait until init is done*/
		while (tmp->video_th_state < GF_COMPOSITOR_THREAD_RUN) {
			gf_sleep(1);
		}
		/*init failure*/
		if (tmp->video_th_state == GF_COMPOSITOR_THREAD_INIT_FAILED) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("GF_COMPOSITOR_THREAD_INIT_FAILED : Deleting compositor.\n"));
			gf_sc_del(tmp);
			return NULL;
		}
	} else {
		e = gf_sc_create(tmp);
		if (e) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Error while calling gf_sc_create() : %s, deleting compositor.\n", gf_error_to_string(e)));
			gf_sc_del(tmp);
			return NULL;
		}
	}

	if ((tmp->user->init_flags & GF_TERM_NO_REGULATION) || !tmp->VisualThread)
		tmp->no_regulation = GF_TRUE;
	
	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI]\tCompositor Cycle Log\tNetworks\tDecoders\tFrame\tDirect Draw\tVisual Config\tEvent\tRoute\tSMIL Timing\tTime node\tTexture\tSMIL Anim\tTraverse setup\tTraverse (and direct Draw)\tTraverse (and direct Draw) without anim\tIndirect Draw\tTraverse And Draw (Indirect or Not)\tFlush\tCycle\n"));
	return tmp;
}

GF_EXPORT
void gf_sc_del(GF_Compositor *compositor)
{
	if (!compositor) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Destroying\n"));
	compositor->discard_input_events = GF_TRUE;
	gf_sc_lock(compositor, GF_TRUE);

	if (compositor->VisualThread) {
		if (compositor->video_th_state == GF_COMPOSITOR_THREAD_RUN) {
			compositor->video_th_state = GF_COMPOSITOR_THREAD_ABORTING;
			while (compositor->video_th_state != GF_COMPOSITOR_THREAD_DONE) {
				gf_sc_lock(compositor, GF_FALSE);
				gf_sleep(1);
				gf_sc_lock(compositor, GF_TRUE);
			}
		}
		gf_th_del(compositor->VisualThread);
	} else {
#ifndef GPAC_DISABLE_3D
		compositor_2d_reset_gl_auto(compositor);
#endif
		gf_sc_texture_cleanup_hw(compositor);
	}

	if (compositor->video_out) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Closing video output\n"));
		compositor->video_out->Shutdown(compositor->video_out);
		gf_modules_close_interface((GF_BaseInterface *)compositor->video_out);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Closing visual compositor\n"));

	if (compositor->focus_highlight) {
		gf_node_unregister(compositor->focus_highlight->node, NULL);
		drawable_del_ex(compositor->focus_highlight, compositor);
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
#endif

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Unloading visual compositor module\n"));

	if (compositor->audio_renderer) gf_sc_ar_del(compositor->audio_renderer);
	compositor->audio_renderer = NULL;

	/*unload proto modules*/
	if (compositor->proto_modules) {
		u32 i;
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
	if (compositor->evq_mx) gf_mx_v(compositor->evq_mx);
	if (compositor->evq_mx) gf_mx_del(compositor->evq_mx);
	gf_list_del(compositor->event_queue);
	gf_list_del(compositor->event_queue_back);

	if (compositor->font_manager) gf_font_manager_del(compositor->font_manager);

#ifdef GF_SR_USE_VIDEO_CACHE
	gf_list_del(compositor->cached_groups);
	gf_list_del(compositor->cached_groups_queue);
#endif

	if (compositor->textures) gf_list_del(compositor->textures);
	if (compositor->textures_gc) gf_list_del(compositor->textures_gc);
	if (compositor->time_nodes) gf_list_del(compositor->time_nodes);
	if (compositor->extra_scenes) gf_list_del(compositor->extra_scenes);
	if (compositor->video_listeners) gf_list_del(compositor->video_listeners);

	gf_sc_lock(compositor, GF_FALSE);
	gf_mx_del(compositor->mx);
	gf_free(compositor);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Destroyed\n"));
}

GF_EXPORT
void gf_sc_set_fps(GF_Compositor *compositor, Double fps)
{
	if (fps) {
		compositor->frame_rate = fps;
		compositor->frame_duration = (u32) (1000 / fps);
		gf_sc_reset_framerate(compositor);
	}
}

u32 gf_sc_get_audio_buffer_length(GF_Compositor *compositor)
{
	if (!compositor || !compositor->audio_renderer || !compositor->audio_renderer->audio_out) return 0;
	return compositor->audio_renderer->audio_out->GetTotalBufferTime(compositor->audio_renderer->audio_out);
}


static void gf_sc_set_play_state(GF_Compositor *compositor, u32 PlayState)
{
	if (!compositor || !compositor->audio_renderer) return;
	if (!compositor->paused && !PlayState) return;
	if (compositor->paused && (PlayState==GF_STATE_PAUSED)) return;

	/*step mode*/
	if (PlayState==GF_STATE_STEP_PAUSE) {
		compositor->step_mode = GF_TRUE;
		gf_sc_flush_next_audio(compositor);
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
	if (!compositor->bench_mode) {
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

Bool gf_sc_get_size(GF_Compositor *compositor, u32 *Width, u32 *Height)
{
	*Height = compositor->scene_height;
	*Width = compositor->scene_width;
	return 1;
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
	Bool draw_mode;

	GF_VisualManager *visual;
	u32 i=0;
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
		if (visual->raster_surface) compositor->rasterizer->surface_delete(visual->raster_surface);
		visual->raster_surface = NULL;
	}

	gf_list_reset(compositor->sensors);
	gf_list_reset(compositor->previous_sensors);

	/*reset traverse state*/
	draw_mode = compositor->traverse_state->immediate_draw;
	gf_list_del(compositor->traverse_state->vrml_sensors);
	gf_list_del(compositor->traverse_state->use_stack);
#ifndef GPAC_DISABLE_3D
	gf_list_del(compositor->traverse_state->local_lights);
#endif
	
	memset(compositor->traverse_state, 0, sizeof(GF_TraverseState));
	
	compositor->traverse_state->vrml_sensors = gf_list_new();
	compositor->traverse_state->use_stack = gf_list_new();
#ifndef GPAC_DISABLE_3D
	compositor->traverse_state->local_lights = gf_list_new();
#endif
	
	gf_mx2d_init(compositor->traverse_state->transform);
	gf_cmx_init(&compositor->traverse_state->color_mat);
	compositor->traverse_state->immediate_draw = draw_mode;

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
	compositor->frame_number = 0;
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
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Reseting audio compositor\n"));
		gf_sc_ar_reset(compositor->audio_renderer);
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Reseting event queue\n"));
	gf_mx_p(compositor->evq_mx);
	while (gf_list_count(compositor->event_queue)) {
		GF_QueuedEvent *qev = (GF_QueuedEvent*)gf_list_get(compositor->event_queue, 0);
		gf_list_rem(compositor->event_queue, 0);
		gf_free(qev);
	}
	gf_mx_v(compositor->evq_mx);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Reseting compositor module\n"));
	/*reset main surface*/
	gf_sc_reset(compositor, scene_graph ? 1 : 0);

	/*set current graph*/
	compositor->scene = scene_graph;
	do_notif = GF_FALSE;
	if (scene_graph) {
#ifndef GPAC_DISABLE_SVG
		SVG_Length *w, *h;
		SVG_ViewBox *vb;
		Bool is_svg = GF_FALSE;
		u32 tag;
		GF_Node *top_node;
#endif
		const char *opt;
		Bool had_size_info = compositor->has_size_info;
		/*get pixel size if any*/
		gf_sg_get_scene_size_info(compositor->scene, &width, &height);
		compositor->has_size_info = (width && height) ? 1 : 0;
		if (compositor->has_size_info != had_size_info) compositor->scene_width = compositor->scene_height = 0;

		compositor->video_memory = gf_scene_is_dynamic_scene(scene_graph);

#ifndef GPAC_DISABLE_3D
		compositor->visual->camera.world_bbox.is_set = 0;
#endif

		/*default back color is black*/
		if (! (compositor->user->init_flags & GF_TERM_WINDOWLESS)) {
			if (compositor->default_back_color) {
				compositor->back_color = compositor->default_back_color;
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
		if (is_svg && ! (compositor->user->init_flags & GF_TERM_WINDOWLESS)) compositor->back_color = 0xFFFFFFFF;

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
		if (compositor->user->init_flags & GF_TERM_WINDOWLESS) {
			opt = gf_cfg_get_key(compositor->user->config, "Compositor", "ColorKey");
			if (opt) {
				u32 r, g, b, a;
				sscanf(opt, "%02X%02X%02X%02X", &a, &r, &g, &b);
				compositor->back_color = GF_COL_ARGB(0xFF, r, g, b);
			}
		}

		/*set scene size only if different, otherwise keep scaling/FS*/
		if ( !width || (compositor->scene_width!=width) || !height || (compositor->scene_height!=height)) {
			do_notif = do_notif || compositor->has_size_info || (!compositor->scene_width && !compositor->scene_height);
			gf_sc_set_scene_size(compositor, width, height, 0);

			/*get actual size in pixels*/
			width = compositor->scene_width;
			height = compositor->scene_height;

			opt = gf_cfg_get_key(compositor->user->config, "Compositor", "ScreenWidth");
			if (opt) width = atoi(opt);
			opt = gf_cfg_get_key(compositor->user->config, "Compositor", "ScreenHeight");
			if (opt) height = atoi(opt);

			if (!compositor->user->os_window_handler) {
				/*only notify user if we are attached to a window*/
				//do_notif = 0;
				if (compositor->video_out->max_screen_width && (width > compositor->video_out->max_screen_width))
					width = compositor->video_out->max_screen_width;
				if (compositor->video_out->max_screen_height && (height > compositor->video_out->max_screen_height))
					height = compositor->video_out->max_screen_height;

				gf_sc_set_size(compositor,width, height);
			}
		}
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
		gf_term_send_event(compositor->term, &evt);
	}
	return GF_OK;
}

void gf_sc_lock_audio(GF_Compositor *compositor, Bool doLock)
{
	if (compositor->audio_renderer) {
		gf_mixer_lock(compositor->audio_renderer->mixer, doLock);
	}
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("line %d lock_ok %d\n", __LINE__, lock_ok));

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
		gf_term_send_event(compositor->term, &evt);
	}

	return GF_OK;
}

GF_EXPORT
void gf_sc_reload_config(GF_Compositor *compositor)
{
	const char *sOpt;


	/*changing drivers needs exclusive access*/
	gf_sc_lock(compositor, 1);


	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "FrameRate");
	if (!sOpt) {
		sOpt = "30.0";
		gf_cfg_set_key(compositor->user->config, "Compositor", "FrameRate", "30.0");
	}
	gf_sc_set_fps(compositor, atof(sOpt));

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "ForceSceneSize");
	if (sOpt && ! stricmp(sOpt, "yes")) {
		compositor->override_size_flags = 1;
	} else {
		compositor->override_size_flags = 0;
	}

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "AntiAlias");
	if (sOpt) {
		if (! stricmp(sOpt, "None")) gf_sc_set_option(compositor, GF_OPT_ANTIALIAS, GF_ANTIALIAS_NONE);
		else if (! stricmp(sOpt, "Text")) gf_sc_set_option(compositor, GF_OPT_ANTIALIAS, GF_ANTIALIAS_TEXT);
		else gf_sc_set_option(compositor, GF_OPT_ANTIALIAS, GF_ANTIALIAS_FULL);
	} else {
		gf_cfg_set_key(compositor->user->config, "Compositor", "AntiAlias", "All");
		gf_sc_set_option(compositor, GF_OPT_ANTIALIAS, GF_ANTIALIAS_FULL);
	}


	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "FocusHighlightFill");
	if (sOpt) sscanf(sOpt, "%x", &compositor->highlight_fill);
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "FocusHighlightStroke");
	if (sOpt) sscanf(sOpt, "%x", &compositor->highlight_stroke);
	else compositor->highlight_stroke = 0xFF000000;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "FocusHighlightStrokeWidth");
	if (sOpt) {
		Float v;
		sscanf(sOpt, "%f", &v);
		compositor->highlight_stroke_width = FLT2FIX(v);
	}
	else compositor->highlight_stroke_width = FIX_ONE;


	compositor->text_sel_color = 0xFFAAAAFF;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "TextSelectHighlight");
	if (sOpt) sscanf(sOpt, "%x", &compositor->text_sel_color);
	if (!compositor->text_sel_color) compositor->text_sel_color = 0xFFAAAAFF;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "ScalableZoom");
	compositor->scalable_zoom = (!sOpt || !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DisableYUV");
	compositor->enable_yuv_hw = (sOpt && !stricmp(sOpt, "yes") ) ? 0 : 1;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DisablePartialHardwareBlit");
	compositor->disable_partial_hw_blit = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;


	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "StressMode");
	gf_sc_set_option(compositor, GF_OPT_STRESS_MODE, (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0);

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "HighSpeed");
	gf_sc_set_option(compositor, GF_OPT_HIGHSPEED, (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0);

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "BoundingVolume");
	if (sOpt) {
		if (! stricmp(sOpt, "Box")) gf_sc_set_option(compositor, GF_OPT_DRAW_BOUNDS, GF_BOUNDS_BOX);
		else if (! stricmp(sOpt, "AABB")) gf_sc_set_option(compositor, GF_OPT_DRAW_BOUNDS, GF_BOUNDS_AABB);
		else gf_sc_set_option(compositor, GF_OPT_DRAW_BOUNDS, GF_BOUNDS_NONE);
	} else {
		gf_cfg_set_key(compositor->user->config, "Compositor", "BoundingVolume", "None");
		gf_sc_set_option(compositor, GF_OPT_DRAW_BOUNDS, GF_BOUNDS_NONE);
	}

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "TextureTextMode");
	if (sOpt && !stricmp(sOpt, "Always")) compositor->texture_text_mode = GF_TEXTURE_TEXT_ALWAYS;
	else if (sOpt && !stricmp(sOpt, "Never")) compositor->texture_text_mode = GF_TEXTURE_TEXT_NEVER;
	else compositor->texture_text_mode = GF_TEXTURE_TEXT_DEFAULT;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "Output8bit");
	if (!sOpt) {
		sOpt = (compositor->video_out->max_screen_bpp > 8) ? "no" : "yes";
		gf_cfg_set_key(compositor->user->config, "Compositor", "Output8bit", sOpt);
	}
	if (compositor->video_out->max_screen_bpp <= 8) {
		if (sOpt && !strcmp(sOpt, "yes")) compositor->output_as_8bit = GF_TRUE;
	} else if (sOpt && !strcmp(sOpt, "forced")) {
		compositor->output_as_8bit = GF_TRUE;
	}

	if (compositor->audio_renderer) {
		sOpt = gf_cfg_get_key(compositor->user->config, "Audio", "NoResync");
		compositor->audio_renderer->disable_resync = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;

		sOpt = gf_cfg_get_key(compositor->user->config, "Audio", "DisableMultiChannel");
		compositor->audio_renderer->disable_multichannel = (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0;
	}

#ifdef GF_SR_USE_VIDEO_CACHE
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "VideoCacheSize");
	compositor->video_cache_max_size = sOpt ? atoi(sOpt) : 0;
	compositor->video_cache_max_size *= 1024;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "CacheScale");
	compositor->cache_scale = sOpt ? atoi(sOpt) : 100;
	if (!compositor->cache_scale) compositor->cache_scale = 100;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "CacheTolerance");
	compositor->cache_tolerance = sOpt ? atoi(sOpt) : 30;
#endif

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "TextureFromDecoderMemory");
	compositor->texture_from_decoder_memory = (sOpt && !strcmp(sOpt, "yes")) ? GF_TRUE : GF_FALSE;
	if (!sOpt)
		gf_cfg_set_key(compositor->user->config, "Compositor", "TextureFromDecoderMemory", "no");

#ifndef GPAC_DISABLE_3D

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "OpenGLMode");

	if (! (compositor->video_out->hw_caps & GF_VIDEO_HW_OPENGL)) {
		if (sOpt && strcmp(sOpt, "disable")) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] OpenGL mode %s requested but no opengl-capable output - disabling openGL\n", sOpt));
		}
		compositor->force_opengl_2d = 0;
		compositor->autoconfig_opengl = 0;
		compositor->hybrid_opengl = 0;
	} else {
		compositor->force_opengl_2d = (sOpt && !strcmp(sOpt, "always")) ? 1 : 0;
		if (!sOpt) {
			compositor->recompute_ar = 1;
			compositor->autoconfig_opengl = 1;
		} else {
			compositor->hybrid_opengl = !strcmp(sOpt, "hybrid") ? 1 : 0;
#ifdef OPENGL_RASTER
			compositor->opengl_raster = !strcmp(sOpt, "raster") ? 1 : 0;
			if (compositor->opengl_raster) compositor->traverse_state->immediate_draw = GF_TRUE;
#endif
		}
	}

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "EnablePBO");
	if (!sOpt) gf_cfg_set_key(compositor->user->config, "Compositor", "EnablePBO", "no");
	compositor->enable_pbo = (sOpt && !strcmp(sOpt, "yes")) ? 1 : 0;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DefaultNavigationMode");
	if (sOpt && !strcmp(sOpt, "Walk")) compositor->default_navigation_mode = GF_NAVIGATE_WALK;
	else if (sOpt && !strcmp(sOpt, "Examine")) compositor->default_navigation_mode = GF_NAVIGATE_EXAMINE;
	else if (sOpt && !strcmp(sOpt, "Fly")) compositor->default_navigation_mode = GF_NAVIGATE_FLY;

#ifdef GPAC_HAS_GLU
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "RasterOutlines");
	compositor->raster_outlines = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
#endif
	/*currently:
	- no support for npow2 textures, and no support for DrawPixels
	*/
#ifndef GPAC_USE_GLES1X
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "EmulatePOW2");
	compositor->emul_pow2 = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
#else
	compositor->raster_outlines = 1;
	compositor->emul_pow2 = 1;
#endif

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "PolygonAA");
	compositor->poly_aa = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "BackFaceCulling");
	if (sOpt && !stricmp(sOpt, "Off")) compositor->backcull = GF_BACK_CULL_OFF;
	else if (sOpt && !stricmp(sOpt, "Alpha")) compositor->backcull = GF_BACK_CULL_ALPHA;
	else compositor->backcull = GF_BACK_CULL_ON;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "Wireframe");
	if (sOpt && !stricmp(sOpt, "WireOnly")) compositor->wiremode = GF_WIREFRAME_ONLY;
	else if (sOpt && !stricmp(sOpt, "WireOnSolid")) compositor->wiremode = GF_WIREFRAME_SOLID;
	else compositor->wiremode = GF_WIREFRAME_NONE;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DrawNormals");
	if (sOpt && !stricmp(sOpt, "PerFace")) compositor->draw_normals = GF_NORMALS_FACE;
	else if (sOpt && !stricmp(sOpt, "PerVertex")) compositor->draw_normals = GF_NORMALS_VERTEX;
	else compositor->draw_normals = GF_NORMALS_NONE;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DisableGLUScale");
	compositor->disable_glu_scale = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DisableRectExt");
	compositor->disable_rect_ext = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DisableGLCulling");
	compositor->disable_gl_cull = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DisableYUVGL");
	compositor->disable_yuvgl = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DepthScale");
	if (!sOpt) {
		sOpt = "100";
		gf_cfg_set_key(compositor->user->config, "Compositor", "DepthScale", sOpt);
	}
	compositor->depth_gl_scale = FLT2FIX( (Float) atof(sOpt) );

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DepthType");
	if (!sOpt) {
		sOpt = "VertexArray";
		gf_cfg_set_key(compositor->user->config, "Compositor", "DepthType", sOpt);
	}
	if (sOpt && !strcmp(sOpt, "Points")) compositor->depth_gl_type = GF_SC_DEPTH_GL_POINTS;
	else if (sOpt && !strnicmp(sOpt, "Strips", 6)) {
		compositor->depth_gl_type = GF_SC_DEPTH_GL_STRIPS;
		compositor->depth_gl_strips_filter = 0;
		if (strlen(sOpt)>7) compositor->depth_gl_strips_filter = FLT2FIX( (Float) atof(sOpt+7) );
	}
	else if (sOpt && !strcmp(sOpt, "VertexArray")) compositor->depth_gl_type = GF_SC_DEPTH_GL_VBO;
	else compositor->depth_gl_type = GF_SC_DEPTH_GL_NONE;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "NumViews");
	if (!sOpt) {
		sOpt = "1";
		gf_cfg_set_key(compositor->user->config, "Compositor", "NumViews", "1");
	}
	compositor->visual->nb_views = atoi(sOpt);
	if (!compositor->visual->nb_views) compositor->visual->nb_views = 1;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "StereoType");
	if (!sOpt) {
		sOpt = "none";
		gf_cfg_set_key(compositor->user->config, "Compositor", "StereoType", "none");
	}
	if (!strcmp(sOpt, "SideBySide")) compositor->visual->autostereo_type = GF_3D_STEREO_SIDE;
	else if (!strcmp(sOpt, "TopToBottom")) compositor->visual->autostereo_type = GF_3D_STEREO_TOP;
	else if (!strcmp(sOpt, "Custom")) compositor->visual->autostereo_type = GF_3D_STEREO_CUSTOM;
	/*built-in interleavers*/
	else if (!strcmp(sOpt, "Anaglyph")) compositor->visual->autostereo_type = GF_3D_STEREO_ANAGLYPH;
	else if (!strcmp(sOpt, "Columns")) compositor->visual->autostereo_type = GF_3D_STEREO_COLUMNS;
	else if (!strcmp(sOpt, "Rows")) compositor->visual->autostereo_type = GF_3D_STEREO_ROWS;
	else if (!strcmp(sOpt, "SPV19")) {
		compositor->visual->autostereo_type = GF_3D_STEREO_5VSP19;
		if (compositor->visual->nb_views != 5) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] SPV19 interleaving used but only %d views indicated - adjusting to 5 view\n", compositor->visual->nb_views ));
			compositor->visual->nb_views = 5;
			gf_cfg_set_key(compositor->user->config, "Compositor", "NumViews", "5");
		}
	}
	else if (!strcmp(sOpt, "ALIO")) {
		compositor->visual->autostereo_type = GF_3D_STEREO_8VALIO;
		if (compositor->visual->nb_views != 8) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] ALIO interleaving used but only %d views indicated - adjusting to 8 view\n", compositor->visual->nb_views ));
			compositor->visual->nb_views = 8;
			gf_cfg_set_key(compositor->user->config, "Compositor", "NumViews", "8");
		}
	}
	else if (!strcmp(sOpt, "StereoHeadset")) compositor->visual->autostereo_type = GF_3D_STEREO_HEADSET;
	else {
		compositor->visual->autostereo_type = GF_3D_STEREO_NONE;
		compositor->visual->nb_views = 1;
		gf_cfg_set_key(compositor->user->config, "Compositor", "NumViews", "1");
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
			GF_LOG(GF_LOG_WARNING, GF_LOG_COMPOSE, ("[Compositor] Stereo interleaving used but %d views indicated - adjusting to 2 view\n", compositor->visual->nb_views ));
			compositor->visual->nb_views = 2;
			gf_cfg_set_key(compositor->user->config, "Compositor", "NumViews", "2");
		}
		break;
	}

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "FramePacking");
	if (!sOpt) {
		sOpt = "None";
		gf_cfg_set_key(compositor->user->config, "Compositor", "FramePacking", "None");
	}
	if (!strcmp(sOpt, "Side")) compositor->frame_packing = GF_3D_STEREO_SIDE;
	else if (!strcmp(sOpt, "Top")) compositor->frame_packing = GF_3D_STEREO_TOP;
	else compositor->frame_packing = GF_3D_STEREO_NONE;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "CameraLayout");
	if (!sOpt) {
		sOpt = "Straight";
		gf_cfg_set_key(compositor->user->config, "Compositor", "CameraLayout", "Straight");
	}
	if (!strcmp(sOpt, "Linear")) compositor->visual->camera_layout = GF_3D_CAMERA_LINEAR;
	else if (!strcmp(sOpt, "Circular")) compositor->visual->camera_layout = GF_3D_CAMERA_CIRCULAR;
	else if (!strcmp(sOpt, "OffAxis")) compositor->visual->camera_layout = GF_3D_CAMERA_OFFAXIS;
	else compositor->visual->camera_layout = GF_3D_CAMERA_STRAIGHT;

	compositor->interoccular_distance = FLT2FIX(6.3f);
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "EyeSeparation");
	if (sOpt) compositor->interoccular_distance = FLT2FIX( atof(sOpt)) ;
	else gf_cfg_set_key(compositor->user->config, "Compositor", "EyeSeparation", "6.3");

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "ReverseViews");
	if (sOpt && !strcmp(sOpt, "yes")) compositor->visual->reverse_views = 1;

	compositor->tile_visibility_nb_tests = 30;
	compositor->tile_visibility_threshold = 0;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "TileVisibilityTest");
	if (!sOpt || !strstr(sOpt, "-")) {
		gf_cfg_set_key(compositor->user->config, "Compositor", "TileVisibilityTest", "30-0");
	} else {
		sscanf(sOpt, "%u-%u", &compositor->tile_visibility_nb_tests, &compositor->tile_visibility_threshold);
	}

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "TileVisibilityDebug");
	if (!sOpt) {
		gf_cfg_set_key(compositor->user->config, "Compositor", "TileVisibilityDebug", "no");
	} else if (!strcmp(sOpt, "yes")) {
		compositor->tile_visibility_debug = GF_TRUE;
	}
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "TileVisibilityForced");
	if (!sOpt) {
		gf_cfg_set_key(compositor->user->config, "Compositor", "TileVisibilityForced", "no");
	} else if (!strcmp(sOpt, "yes")) {
		compositor->force_all_tiles_visible = GF_TRUE;
	}

#endif //GPAC_DISABLE_3D


	/*load defer mode only once hybrid_opengl is known. If no hybrid openGL and no backbuffer 2D, disable defer rendering*/
	if (!compositor->hybrid_opengl && compositor->video_out->hw_caps & GF_VIDEO_HW_DIRECT_ONLY) {
		compositor->traverse_state->immediate_draw = 1;
	} else {
		sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DrawMode");
		if (!sOpt) {
			sOpt = "defer";
			gf_cfg_set_key(compositor->user->config, "Compositor", "DrawMode", sOpt);
		}

		if (!strcmp(sOpt, "immediate")) compositor->traverse_state->immediate_draw = 1;
		else if (!strcmp(sOpt, "defer-debug")) {
			compositor->traverse_state->immediate_draw = 0;
			compositor->debug_defer = 1;
		}
		else compositor->traverse_state->immediate_draw = 0;
	}


#ifdef GF_SR_USE_DEPTH
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "AutoStereoCalibration");
	compositor->auto_calibration = (sOpt && !strcmp(sOpt, "yes")) ? 1 : 0;

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DisplayDepth");
	compositor->display_depth = sOpt ? (!strcmp(sOpt, "auto") ? -1 : atoi(sOpt)) : 0;

#ifndef GPAC_DISABLE_3D
	/*if auto-stereo mode, turn on display depth by default*/
	if (compositor->visual->autostereo_type && !compositor->display_depth) {
		compositor->display_depth = -1;
	}
#endif

	if (!compositor->video_out->view_distance) {
		sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "ViewDistance");
		compositor->video_out->view_distance = FLT2FIX( sOpt ? (Float) atof(sOpt) : 50.0f );
	}

#ifndef GPAC_DISABLE_3D
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "FocusDistance");
	compositor->focus_distance = 0;
	if (sOpt)
		compositor->focus_distance = FLT2FIX( atof(sOpt) );
	else
		gf_cfg_set_key(compositor->user->config, "Compositor", "FocusDistance", "0");
#endif

#endif

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "SimulateGaze");
	if (!sOpt) gf_cfg_set_key(compositor->user->config, "Compositor", "SimulateGaze", "no");
	compositor->simulate_gaze = (sOpt && !strcmp(sOpt, "yes")) ? GF_TRUE : GF_FALSE;
	if (compositor->simulate_gaze) compositor->gazer_enabled = GF_TRUE;


	gf_sc_reset_graphics(compositor);
	gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);

	gf_sc_lock(compositor, 0);
}

GF_EXPORT
GF_Err gf_sc_set_option(GF_Compositor *compositor, u32 type, u32 value)
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
		compositor->stress_mode = value;
		break;
	case GF_OPT_ANTIALIAS:
		compositor->antiAlias = value;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_HIGHSPEED:
		compositor->high_speed = value;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_DRAW_BOUNDS:
		compositor->draw_bvol = value;
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		break;
	case GF_OPT_TEXTURE_TEXT:
		compositor->texture_text_mode = value;
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
		compositor->scalable_zoom = value;
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
		compositor->enable_yuv_hw = value;
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
		compositor->emul_pow2 = value;
		break;
	case GF_OPT_POLYGON_ANTIALIAS:
		compositor->poly_aa = value;
		break;
	case GF_OPT_BACK_CULL:
		compositor->backcull = value;
		break;
	case GF_OPT_WIREFRAME:
		compositor->wiremode = value;
		break;
	case GF_OPT_NORMALS:
		compositor->draw_normals = value;
		break;
#ifdef GPAC_HAS_GLU
	case GF_OPT_RASTER_OUTLINES:
		compositor->raster_outlines = value;
		break;
#endif

	case GF_OPT_NO_RECT_TEXTURE:
		if (value != compositor->disable_rect_ext) {
			compositor->disable_rect_ext = value;
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
u32 gf_sc_get_option(GF_Compositor *compositor, u32 type)
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
		return compositor->stress_mode;
	case GF_OPT_AUDIO_VOLUME:
		return compositor->audio_renderer->volume;
	case GF_OPT_AUDIO_PAN:
		return compositor->audio_renderer->pan;
	case GF_OPT_AUDIO_MUTE:
		return compositor->audio_renderer->mute;

	case GF_OPT_ANTIALIAS:
		return compositor->antiAlias;
	case GF_OPT_HIGHSPEED:
		return compositor->high_speed;
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
		return compositor->texture_text_mode;
	case GF_OPT_USE_OPENGL:
		return compositor->force_opengl_2d;

	case GF_OPT_DRAW_MODE:
		if (compositor->traverse_state->immediate_draw) return GF_DRAW_MODE_IMMEDIATE;
		if (compositor->debug_defer) return GF_DRAW_MODE_DEFER_DEBUG;
		return GF_DRAW_MODE_DEFER;
	case GF_OPT_SCALABLE_ZOOM:
		return compositor->scalable_zoom;
	case GF_OPT_YUV_HARDWARE:
		return compositor->enable_yuv_hw;
	case GF_OPT_YUV_FORMAT:
		return compositor->enable_yuv_hw ? compositor->video_out->yuv_pixel_format : 0;
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
		return compositor->video_cache_max_size/1024;
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

GF_Err gf_sc_get_offscreen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer, u32 view_idx, u32 depth_dump_mode)
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
		assert(run_time >= compositor->frame_time[fidx]);
		run_time -= compositor->frame_time[fidx];
		frames = GF_SR_FPS_COMPUTE_SIZE-1;
	}


	gf_mx_v(compositor->mx);

	if (!run_time) return (Double) compositor->frame_rate;
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


GF_EXPORT
GF_Node *gf_sc_pick_node(GF_Compositor *compositor, s32 X, s32 Y)
{
	return NULL;
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
			gf_sc_load_opengl_extensions(compositor, compositor->visual->type_3d);
#ifndef GPAC_USE_GLES1X
			visual_3d_init_shaders(compositor->visual);
#endif
			if (compositor->autoconfig_opengl) {
				compositor->autoconfig_opengl = 0;
				compositor->force_opengl_2d = 0;
				compositor->visual->type_3d = prev_type_3d;

#if !defined(GPAC_USE_TINYGL) && !defined(GPAC_USE_GLES1X)
				//enable hybrid mode by default
				if (compositor->visual->compositor->shader_only_mode) {
					gf_cfg_set_key(compositor->user->config, "Compositor", "OpenGLMode", "hybrid");
					compositor->hybrid_opengl = 1;
				} else {
					gf_cfg_set_key(compositor->user->config, "Compositor", "OpenGLMode", "disable");
				}
#endif
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
			if (compositor->display_depth) {
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
			if (compositor->display_depth) {
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


static void gf_sc_draw_scene(GF_Compositor *compositor)
{
	u32 flags;

	GF_Node *top_node = gf_sg_get_root_node(compositor->scene);

	if (!top_node && !compositor->visual->last_had_back && !compositor->visual->cur_context) {
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Scene has no root node, nothing to draw\n"));
		//nothing to draw, skip flush
		compositor->skip_flush = 1;
		return;
	}

#ifdef GF_SR_USE_VIDEO_CACHE
	if (!compositor->video_cache_max_size)
		compositor->traverse_state->in_group_cache = 1;
#endif

	flags = compositor->traverse_state->immediate_draw;
	if (compositor->video_setup_failed) {
		compositor->skip_flush = 1;
	}
	else if (! visual_draw_frame(compositor->visual, top_node, compositor->traverse_state, 1)) {
		/*android backend uses opengl without telling it to us, we need an ugly hack here ...*/
#ifdef GPAC_ANDROID
		compositor->skip_flush = 0;
#else
		if (compositor->skip_flush==2) {
			compositor->skip_flush = 0;
		} else {
			compositor->skip_flush = 1;
		}
#endif
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
}


#ifndef GPAC_DISABLE_LOG
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

void gf_sc_flush_video(GF_Compositor *compositor)
{
	GF_Window rc;

	//release compositor in case we have vsync
	gf_sc_lock(compositor, 0);
	rc.x = rc.y = 0;
	rc.w = compositor->display_width;
	rc.h = compositor->display_height;
	compositor->video_out->Flush(compositor->video_out, &rc);
	gf_sc_lock(compositor, 1);
}

GF_EXPORT
void gf_sc_render_frame(GF_Compositor *compositor)
{
#ifndef GPAC_DISABLE_SCENEGRAPH
	GF_SceneGraph *sg;
#endif
	GF_List *temp_queue;
	u32 in_time, end_time, i, count, frame_duration;
	Bool frame_drawn, has_timed_nodes=GF_FALSE, all_tx_done=GF_TRUE;
#ifndef GPAC_DISABLE_LOG
	s32 event_time, route_time, smil_timing_time=0, time_node_time, texture_time, traverse_time, flush_time, txtime;
#endif

	/*lock compositor for the whole cycle*/
	gf_sc_lock(compositor, 1);
//	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Entering render_frame \n"));

	in_time = gf_sys_clock();

	gf_sc_texture_cleanup_hw(compositor);

	/*first thing to do, let the video output handle user event if it is not threaded*/
	compositor->video_out->ProcessEvent(compositor->video_out, NULL);

	if (compositor->freeze_display) {
		gf_sc_lock(compositor, 0);
		if (!compositor->bench_mode) {
			compositor->scene_sampled_clock = gf_sc_ar_get_clock(compositor->audio_renderer);
		}
		if (!compositor->no_regulation) gf_sleep(compositor->frame_duration);
		return;
	}

	gf_sc_reconfig_task(compositor);

	/* if there is no scene*/
	if (!compositor->scene && !gf_list_count(compositor->extra_scenes) ) {
		gf_sc_draw_scene(compositor);
		//increase clock in bench mode before releasing mutex
		if (compositor->bench_mode && (compositor->force_bench_frame==1)) {
			compositor->scene_sampled_clock += compositor->frame_duration;
		}
		gf_sc_lock(compositor, 0);
		if (!compositor->no_regulation) {
			gf_sleep(compositor->bench_mode ? 2 : compositor->frame_duration);
		}
		compositor->force_bench_frame=0;
		compositor->frame_draw_type = 0;
		return;
	}

	if (compositor->reset_graphics) {
		gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		visual_reset_graphics(compositor->visual);
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


	if (!compositor->bench_mode) {
		compositor->scene_sampled_clock = gf_sc_ar_get_clock(compositor->audio_renderer);
	} else {
		if (compositor->force_bench_frame==1) {
			//a system frame is pending on a future frame - we must increase our time
			compositor->scene_sampled_clock += compositor->frame_duration;
		}
		compositor->force_bench_frame = 0;
	}


	//first update all natural textures to figure out timing
	compositor->frame_delay = 0;
	compositor->ms_until_next_frame = GF_INT_MAX;
	frame_duration = compositor->frame_duration;

	if (compositor->auto_rotate) 
		compositor_handle_auto_navigation(compositor);

#ifndef GPAC_DISABLE_LOG
	texture_time = gf_sys_clock();
#endif
	/*update all video textures*/
	count = gf_list_count(compositor->textures);
	for (i=0; i<count; i++) {
		GF_TextureHandler *txh = (GF_TextureHandler *)gf_list_get(compositor->textures, i);
		if (!txh) break;
		//this is not a natural (video) texture
		if (! txh->stream) continue;

		/*signal graphics reset before updating*/
		if (compositor->reset_graphics && txh->tx_io) gf_sc_texture_reset(txh);
		txh->update_texture_fcnt(txh);

		if (!txh->stream_finished) {
			u32 d = gf_mo_get_min_frame_dur(txh->stream);
			if (d && (d < frame_duration)) frame_duration = d;

			all_tx_done=0;
		}
	}

	//it may happen that we have a reconfigure request at this stage, especially if updating one of the textures
	//forced a relayout - do it right away
	if (compositor->msg_type) {
		gf_sc_lock(compositor, 0);
		return;
	}

	compositor->force_late_frame_draw = GF_FALSE;

#ifndef GPAC_DISABLE_LOG
	texture_time = gf_sys_clock() - texture_time;
#endif


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
		has_timed_nodes = GF_TRUE;
	}
#ifndef GPAC_DISABLE_LOG
	time_node_time = gf_sys_clock() - time_node_time;
#endif

	if (compositor->focus_text_type) {
		if (!compositor->caret_next_draw_time) {
			compositor->caret_next_draw_time = gf_sys_clock();
			compositor->show_caret = 1;
		}
		if (compositor->caret_next_draw_time <= compositor->last_frame_time) {
			compositor->frame_draw_type=GF_SC_DRAW_FRAME;
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

	/*setup display before updating composite textures (some may require a valid openGL context)*/
	gf_sc_recompute_ar(compositor, gf_sg_get_root_node(compositor->scene) );

	if (compositor->video_setup_failed)	{
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
		compositor->frame_draw_type=GF_SC_DRAW_FRAME;
	}

	//if hidden and no listener, do not draw the scene
	if (compositor->is_hidden && !compositor->video_listeners) {
		compositor->frame_draw_type = 0;
	}

	frame_drawn = (compositor->frame_draw_type==GF_SC_DRAW_FRAME) ? 1 : 0;

	/*if invalidated, draw*/
	if (compositor->frame_draw_type) {
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
			compositor->frame_draw_type = 0;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Redrawing scene - STB %d\n", compositor->scene_sampled_clock));
			gf_sc_draw_scene(compositor);
#ifndef GPAC_DISABLE_LOG
			traverse_time = gf_sys_clock() - traverse_time;
#endif

			if (compositor->video_listeners && compositor->skip_flush!=1) {
				u32 k=0;
				GF_VideoListener *l;
				while ((l = gf_list_enum(compositor->video_listeners, &k))) {
					l->on_video_frame(l->udta, gf_sc_ar_get_clock(compositor->audio_renderer) );
				}
			}

		}
		/*and flush*/
#ifndef GPAC_DISABLE_LOG
		flush_time = gf_sys_clock();
#endif

		if(compositor->user->init_flags & GF_TERM_INIT_HIDE)
			compositor->skip_flush = 1;

		//if no overlays, release textures before flushing, otherwise we might loose time waiting for vsync
		if (!compositor->visual->has_overlays) {
			compositor_release_textures(compositor, frame_drawn);
			textures_released = 1;
		}

		if (compositor->skip_flush!=1) {
			gf_sc_flush_video(compositor);
		} else {
			compositor->skip_flush = 0;
		}

#ifndef GPAC_DISABLE_LOG
		flush_time = gf_sys_clock() - flush_time;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Compositor] done flushing frame in %d ms\n", flush_time));
#endif

		visual_2d_draw_overlays(compositor->visual);
		compositor->last_had_overlays = compositor->visual->has_overlays;

		if (!textures_released) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Compositor] Releasing textures after flush\n" ));
			compositor_release_textures(compositor, frame_drawn);
		}

		if (compositor->stress_mode) {
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
	compositor->video_frame_pending=0;
	gf_sc_lock(compositor, 0);

	if (frame_drawn) compositor->step_mode = GF_FALSE;

	/*let the owner decide*/
	if (compositor->no_regulation)
		return;

	/*we are in bench mode, just release for a moment the composition, oherwise we will constantly lock the compositor wich may have impact on scene decoding*/
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
			while (! compositor->msg_type && ! compositor->video_frame_pending) {
				gf_sleep(1);
				diff = gf_sys_clock_high_res() - start;
				if (diff >= wait_for)
					break;
			}
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Compositor slept %d ms until next frame (msg type %d - frame pending %d)\n", diff/1000, compositor->msg_type, compositor->video_frame_pending));
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
		/*if the inlined root node is a 3D one except Layer3D and we are not in a 3D context, insert
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

	if (compositor->term && (compositor->interaction_level & GF_INTERACT_INPUT_SENSOR) && (event->type<=GF_EVENT_MOUSEWHEEL)) {
		GF_Event evt = *event;
		gf_term_mouse_input(compositor->term, &evt.mouse);
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
	/*not assigned yet*/
	if (!compositor || !compositor->visual || compositor->discard_input_events) return GF_FALSE;
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
	case GF_EVENT_SET_CAPTION:
	case GF_EVENT_MOVE:
		compositor->video_out->ProcessEvent(compositor->video_out, event);
		break;

	case GF_EVENT_MOVE_NOTIF:
		if (compositor->last_had_overlays) {
			gf_sc_next_frame_state(compositor, GF_SC_DRAW_FRAME);
		}
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
		/*user consummed the resize event, do nothing*/
		if ( gf_term_send_event(compositor->term, event) )
			return GF_TRUE;

		/*not consummed and compositor "owns" the output window (created by the output module), resize*/
		if (!compositor->user->os_window_handler) {
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
#if defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_IPHONE)
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
		if (compositor->term && (compositor->interaction_level & GF_INTERACT_INPUT_SENSOR) ) {
			ret = gf_term_keyboard_input(compositor->term, event->key.key_code, event->key.hw_code, (event->type==GF_EVENT_KEYUP) ? GF_TRUE : GF_FALSE);
		}
		ret += gf_sc_handle_event_intern(compositor, event, from_user);
		return ret;
	}

	case GF_EVENT_TEXTINPUT:
		if (compositor->term && (compositor->interaction_level & GF_INTERACT_INPUT_SENSOR) )
			gf_term_string_input(compositor->term , event->character.unicode_char);

		return gf_sc_handle_event_intern(compositor, event, from_user);
	/*switch fullscreen off!!!*/
	case GF_EVENT_SHOWHIDE_NOTIF:
		compositor->is_hidden = event->show.show_type ? GF_FALSE : GF_TRUE;
		break;

	case GF_EVENT_MOUSEMOVE:
		event->mouse.button = 0;
	case GF_EVENT_MOUSEDOWN:
	case GF_EVENT_MOUSEUP:
	case GF_EVENT_MOUSEWHEEL:
		event->mouse.key_states = compositor->key_states;
	case GF_EVENT_SENSOR_ORIENTATION:
		return gf_sc_handle_event_intern(compositor, event, from_user);

	case GF_EVENT_PASTE_TEXT:
		gf_sc_paste_text(compositor, event->message.message);
		break;
	case GF_EVENT_COPY_TEXT:
		if (gf_sc_has_text_selection(compositor)) {
			event->message.message = gf_sc_get_selected_text(compositor);
		} else {
			event->message.message = NULL;
		}
		break;
	case GF_EVENT_SYNC_LOST:
		compositor->force_late_frame_draw = GF_TRUE;
		break;
	/*when we process events we don't forward them to the user*/
	default:
		return gf_term_send_event(compositor->term, event);
	}
	/*if we get here, event has been consumed*/
	return GF_TRUE;
}

static Bool gf_sc_on_event(void *cbck, GF_Event *event)
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

//	case GF_EVENT_SET_CAPTION:
//		gf_sc_queue_event(compositor, event);
//		return GF_FALSE;
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


u32 gf_sc_get_audio_delay(GF_Compositor *compositor)
{
	return compositor->audio_renderer ? compositor->audio_renderer->audio_delay : 0;
}


Bool gf_sc_script_action(GF_Compositor *compositor, u32 type, GF_Node *n, GF_JSAPIParam *param)
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
			gf_svg_properties_init_pointers(tr_state.svg_props);
			tr_state.abort_bounds_traverse = GF_TRUE;
			gf_node_traverse(n, &tr_state);
			gf_svg_properties_reset_pointers(tr_state.svg_props);
			gf_free(tr_state.svg_props);
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


Bool gf_sc_has_text_selection(GF_Compositor *compositor)
{
	return (compositor->store_text_state==GF_SC_TSEL_FROZEN) ? GF_TRUE : GF_FALSE;
}

const char *gf_sc_get_selected_text(GF_Compositor *compositor)
{
	const u16 *srcp;
	size_t len;
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
	if ((s32)len<0) len = 0;
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

GF_EXPORT
GF_Err gf_sc_add_video_listener(GF_Compositor *sc, GF_VideoListener *vl)
{
	if (!sc|| !vl || !vl->on_video_frame || !vl->on_video_reconfig) return GF_BAD_PARAM;

	gf_sc_lock(sc, GF_TRUE);
	if (!sc->video_listeners) sc->video_listeners = gf_list_new();
	gf_list_add(sc->video_listeners, vl);
	gf_sc_lock(sc, GF_FALSE);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_sc_remove_video_listener(GF_Compositor *sc, GF_VideoListener *vl)
{
	if (!sc|| !vl) return GF_BAD_PARAM;

	gf_sc_lock(sc, GF_TRUE);
	gf_list_del_item(sc->video_listeners, vl);
	if (!gf_list_count(sc->video_listeners)) {
		gf_list_del(sc->video_listeners);
		sc->video_listeners = NULL;
	}
	gf_sc_lock(sc, GF_FALSE);
	return GF_OK;
}

Bool gf_sc_use_raw_texture(GF_Compositor *compositor)
{
	if (!compositor) return 0;
//	if (!compositor->visual->type_3d && !compositor->force_opengl_2d) return 0;
	return compositor->texture_from_decoder_memory;
}

void gf_sc_get_av_caps(GF_Compositor *compositor, u32 *width, u32 *height, u32 *display_bit_depth, u32 *audio_bpp, u32 *channels, u32 *sample_rate)
{
	if (width) *width = compositor->video_out->max_screen_width;
	if (height) *height = compositor->video_out->max_screen_height;
	if (display_bit_depth) *display_bit_depth = compositor->video_out->max_screen_bpp ? compositor->video_out->max_screen_bpp : 8;
	//to do
	if (audio_bpp) *audio_bpp = 8;
	if (channels) *channels = 0;
	if (sample_rate) *sample_rate = 48000;
}

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

void gf_sc_set_video_pending_frame(GF_Compositor *compositor)
{
	compositor->video_frame_pending = GF_TRUE;
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
			qev->dom_evt = *evt;
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

static void sc_cleanup_event_queue(GF_List *evq, GF_Node *node, GF_SceneGraph *sg)
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

Bool gf_sc_use_3d(GF_Compositor *compositor)
{
#ifndef GPAC_DISABLE_3D
	return (compositor->visual->type_3d || compositor->hybrid_opengl) ? 1 : 0;
#else
	return 0;
#endif
}

