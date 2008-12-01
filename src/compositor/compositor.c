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

#include <gpac/internal/compositor_dev.h>
/*for user and terminal API (options and InputSensor)*/
#include <gpac/internal/terminal_dev.h>
#include <gpac/options.h>
#include <gpac/utf.h>

#ifdef GPAC_TRISCOPE_MODE
#include "../src/compositor/triscope_renoir/triscope_renoir.h"
#endif

#include "nodes_stacks.h"

#include "visual_manager.h"
#include "texturing.h"

#ifdef TRISCOPE_TEST_FRAMERATE
#include "chrono.h"
#endif

#define SC_DEF_WIDTH	320
#define SC_DEF_HEIGHT	240


/*macro for size event format/send*/
#define GF_USER_SETSIZE(_user, _w, _h)	\
	{	\
		GF_Event evt;	\
		if (_user->EventProc) {	\
			evt.type = GF_EVENT_SIZE;	\
			evt.size.width = _w;	\
			evt.size.height = _h;	\
			_user->EventProc(_user->opaque, &evt);	\
		}	\
	}

void gf_sc_simulation_tick(GF_Compositor *compositor);


GF_Err gf_sc_set_output_size(GF_Compositor *compositor, u32 Width, u32 Height)
{
	gf_sc_lock(compositor, 1);
	/*FIXME: need to check for max resolution*/
	compositor->display_width = Width;
	compositor->display_height = Height;
	compositor->recompute_ar = 1;
	compositor->draw_next_frame = 1;
	gf_sc_lock(compositor, 0);
	return GF_OK;
}

static void gf_sc_set_fullscreen(GF_Compositor *compositor)
{
	GF_Err e;
	if (!compositor->video_out->SetFullScreen) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Switching fullscreen %s\n", compositor->fullscreen ? "off" : "on"));
	/*move to FS*/
	compositor->fullscreen = !compositor->fullscreen;
	if (compositor->fullscreen && (compositor->scene_width>compositor->scene_height)
#ifndef GPAC_DISABLE_3D
			&& !compositor->visual->type_3d 
#endif
			) {
		e = compositor->video_out->SetFullScreen(compositor->video_out, 2, &compositor->display_width, &compositor->display_height);
	} else {
		e = compositor->video_out->SetFullScreen(compositor->video_out, compositor->fullscreen, &compositor->display_width, &compositor->display_height);
	}
	if (e) {
		GF_USER_MESSAGE(compositor->user, "Compositor", "Cannot switch to fullscreen", e);
		compositor->fullscreen = 0;
		e = compositor->video_out->SetFullScreen(compositor->video_out, 0, &compositor->display_width, &compositor->display_height);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] recomputing aspect ratio\n"));
	compositor->recompute_ar = 1;
	/*force signaling graphics reset*/
	compositor->reset_graphics = 1;
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
	u32 width,height;
	
	/*reconfig audio if needed (non-threaded compositors)*/
	if (compositor->audio_renderer && !compositor->audio_renderer->th) gf_sc_ar_reconfig(compositor->audio_renderer);

	if (compositor->msg_type) {

		compositor->msg_type |= GF_SR_IN_RECONFIG;

		/*scene size has been overriden*/
		if (compositor->msg_type & GF_SR_CFG_OVERRIDE_SIZE) {
			assert(!(compositor->override_size_flags & 2));
			compositor->msg_type &= ~GF_SR_CFG_OVERRIDE_SIZE;
			compositor->override_size_flags |= 2;
			width = compositor->scene_width;
			height = compositor->scene_height;
			compositor->has_size_info = 1;
			gf_sc_set_size(compositor, width, height);
			GF_USER_SETSIZE(compositor->user, width, height);
		}
		/*size changed from scene cfg: resize window first*/
		if (compositor->msg_type & GF_SR_CFG_SET_SIZE) {
			u32 fs_width, fs_height;
			Bool restore_fs = compositor->fullscreen;
			compositor->msg_type &= ~GF_SR_CFG_SET_SIZE;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Changing display size to %d x %d\n", compositor->new_width, compositor->new_height));
			fs_width = fs_height = 0;
			if (restore_fs) {
				fs_width = compositor->display_width;
				fs_height = compositor->display_height;
			}
			evt.type = GF_EVENT_SIZE;
			evt.size.width = compositor->new_width;
			evt.size.height = compositor->new_height;
			compositor->new_width = compositor->new_height = 0;
			/*send resize event*/
			if (!(compositor->msg_type & GF_SR_CFG_WINDOWSIZE_NOTIF)) {
				compositor->video_out->ProcessEvent(compositor->video_out, &evt);
			}
			compositor->msg_type &= ~GF_SR_CFG_WINDOWSIZE_NOTIF;

			if (restore_fs) {
				//gf_sc_set_fullscreen(compositor);
				compositor->display_width = fs_width;
				compositor->display_height = fs_height;
				compositor->recompute_ar = 1;
			} else {
				gf_sc_set_output_size(compositor, evt.size.width, evt.size.height);
			}
			compositor->reset_graphics = 1;
			
		}
		/*aspect ratio modif*/
		if (compositor->msg_type & GF_SR_CFG_AR) {
			compositor->msg_type &= ~GF_SR_CFG_AR;
			compositor->recompute_ar = 1;
		}
		/*fullscreen on/off request*/
		if (compositor->msg_type & GF_SR_CFG_FULLSCREEN) {
			compositor->msg_type &= ~GF_SR_CFG_FULLSCREEN;
			gf_sc_set_fullscreen(compositor);
			compositor->draw_next_frame = 1;
		}
		compositor->msg_type &= ~GF_SR_IN_RECONFIG;
	}

	/*3D driver changed message, recheck extensions*/
	if (compositor->reset_graphics) {
#ifndef GPAC_DISABLE_3D
		compositor->offscreen_width = compositor->offscreen_height = 0;
#endif
		evt.type = GF_EVENT_SYS_COLORS;
		if (compositor->user->EventProc && compositor->user->EventProc(compositor->user->opaque, &evt) ) {
			u32 i;
			for (i=0; i<28; i++) {
				compositor->sys_colors[i] = evt.sys_cols.sys_colors[i] & 0x00FFFFFF;
			}
		}
	}
}

Bool gf_sc_draw_frame(GF_Compositor *compositor)
{	
	gf_sc_simulation_tick(compositor);
	return compositor->draw_next_frame;
}

static u32 gf_sc_proc(void *par)
{	
	GF_Compositor *compositor = (GF_Compositor *) par;
	compositor->video_th_state = 1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Compositor] Entering thread ID %d\n", gf_th_id() ));

	

#ifdef GPAC_TRISCOPE_MODE	
/* scene creation*/	
	compositor->RenoirHandler = CreateRenoirScene();
#endif	
	
	while (compositor->video_th_state == 1) {
		if (compositor->is_hidden) 
			gf_sleep(compositor->frame_duration);
		else	

			gf_sc_simulation_tick(compositor);

	}
	/*destroy video out here if w're using openGL, to avoid threading issues*/
	compositor->video_out->Shutdown(compositor->video_out);
	gf_modules_close_interface((GF_BaseInterface *)compositor->video_out);
	compositor->video_out = NULL;
	compositor->video_th_state = 3;
	return 0;
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
	for (i=0; i<GF_SR_FPS_COMPUTE_SIZE; i++) compositor->frame_time[i] = 0;
	compositor->current_frame = 0;
}

static Bool gf_sc_on_event(void *cbck, GF_Event *event);

static Bool gf_sc_set_check_raster2d(GF_Raster2D *ifce)
{
	/*check base*/
	if (!ifce->stencil_new || !ifce->surface_new) return 0;
	/*if these are not set we cannot draw*/
	if (!ifce->surface_clear || !ifce->surface_set_path || !ifce->surface_fill) return 0;
	/*check we can init a surface with the current driver (the rest is optional)*/
	if (ifce->surface_attach_to_buffer) return 1;
	return 0;
}


static GF_Err gf_sc_load(GF_Compositor *compositor)
{
	compositor->strike_bank = gf_list_new();
	compositor->visuals = gf_list_new();

	GF_SAFEALLOC(compositor->traverse_state, GF_TraverseState);
	compositor->traverse_state->vrml_sensors = gf_list_new();
	compositor->traverse_state->use_stack = gf_list_new();
	compositor->sensors = gf_list_new();
	compositor->previous_sensors = gf_list_new();
	compositor->hit_use_stack = gf_list_new();
	compositor->prev_hit_use_stack = gf_list_new();
	compositor->focus_ancestors = gf_list_new();
	compositor->focus_use_stack = gf_list_new();
	
	/*setup main visual*/
	compositor->visual = visual_new(compositor);
	compositor->visual->GetSurfaceAccess = compositor_2d_get_video_access;
	compositor->visual->ReleaseSurfaceAccess = compositor_2d_release_video_access;

	compositor->visual->DrawBitmap = compositor_2d_draw_bitmap;


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
	compositor->gravity_on = 1;

	/*create default unit sphere and box for bounds*/
	compositor->unit_bbox = new_mesh();
	mesh_new_unit_bbox(compositor->unit_bbox);
#endif

	return GF_OK;
}


static GF_Compositor *gf_sc_create(GF_User *user)
{
	const char *sOpt;
	GF_Compositor *tmp;

	GF_SAFEALLOC(tmp, GF_Compositor);
	if (!tmp) return NULL;
	tmp->user = user;

	/*load video out*/
	sOpt = gf_cfg_get_key(user->config, "Video", "DriverName");
	if (sOpt) {
		tmp->video_out = (GF_VideoOutput *) gf_modules_load_interface_by_name(user->modules, sOpt, GF_VIDEO_OUTPUT_INTERFACE);
		if (tmp->video_out) {
			tmp->video_out->evt_cbk_hdl = tmp;
			tmp->video_out->on_event = gf_sc_on_event;
			/*init hw*/
			if (tmp->video_out->Setup(tmp->video_out, user->os_window_handler, user->os_display, user->init_flags) != GF_OK) {
				gf_modules_close_interface((GF_BaseInterface *)tmp->video_out);
				tmp->video_out = NULL;
			}
		} else {
			sOpt = NULL;
		}
	}

	if (!tmp->video_out) {
		u32 i, count;
		count = gf_modules_get_count(user->modules);
		for (i=0; i<count; i++) {
			tmp->video_out = (GF_VideoOutput *) gf_modules_load_interface(user->modules, i, GF_VIDEO_OUTPUT_INTERFACE);
			if (!tmp->video_out) continue;
			tmp->video_out->evt_cbk_hdl = tmp;
			tmp->video_out->on_event = gf_sc_on_event;
			/*init hw*/
			if (tmp->video_out->Setup(tmp->video_out, user->os_window_handler, user->os_display, user->init_flags)==GF_OK) {
				gf_cfg_set_key(user->config, "Video", "DriverName", tmp->video_out->module_name);
				break;
			}
			gf_modules_close_interface((GF_BaseInterface *)tmp->video_out);
			tmp->video_out = NULL;
		}
	}
	if (!tmp->video_out ) {
		free(tmp);
		return NULL;
	}
#ifdef ENABLE_JOYSTICK
	sOpt = gf_cfg_get_key(user->config, "General", "JoystickCenteredMode");
	if (sOpt) {
		if (!stricmp(sOpt, "no")) tmp->video_out->centered_mode=0; 
		else tmp->video_out->centered_mode=1;
	} else tmp->video_out->centered_mode=1;
#endif


	/*try to load a raster driver*/
	sOpt = gf_cfg_get_key(user->config, "Compositor", "Raster2D");
	if (sOpt) {
		tmp->rasterizer = (GF_Raster2D *) gf_modules_load_interface_by_name(user->modules, sOpt, GF_RASTER_2D_INTERFACE);
		if (!tmp->rasterizer) {
			sOpt = NULL;
		} else if (!gf_sc_set_check_raster2d(tmp->rasterizer)) {
			gf_modules_close_interface((GF_BaseInterface *)tmp->rasterizer);
			tmp->rasterizer = NULL;
			sOpt = NULL;
		}
	}
	if (!tmp->rasterizer) {
		u32 i, count;
		count = gf_modules_get_count(user->modules);
		for (i=0; i<count; i++) {
			tmp->rasterizer = (GF_Raster2D *) gf_modules_load_interface(user->modules, i, GF_RASTER_2D_INTERFACE);
			if (!tmp->rasterizer) continue;
			if (gf_sc_set_check_raster2d(tmp->rasterizer)) break;
			gf_modules_close_interface((GF_BaseInterface *)tmp->rasterizer);
			tmp->rasterizer = NULL;
		}
		if (tmp->rasterizer) gf_cfg_set_key(user->config, "Compositor", "Raster2D", tmp->rasterizer->module_name);
	}
	if (!tmp->rasterizer) {
		tmp->video_out->Shutdown(tmp->video_out);
		gf_modules_close_interface((GF_BaseInterface *)tmp->video_out);
		free(tmp);
		return NULL;
	}

	/*and init*/
	if (gf_sc_load(tmp) != GF_OK) {
		gf_modules_close_interface((GF_BaseInterface *)tmp->rasterizer);
		tmp->video_out->Shutdown(tmp->video_out);
		gf_modules_close_interface((GF_BaseInterface *)tmp->video_out);
		free(tmp);
		return NULL;
	}

	tmp->mx = gf_mx_new("Compositor");
	tmp->textures = gf_list_new();
	tmp->frame_rate = 30.0;	
	tmp->frame_duration = 33;
	tmp->time_nodes = gf_list_new();
#ifdef GF_SR_EVENT_QUEUE
	tmp->events = gf_list_new();
	tmp->ev_mx = gf_mx_new("EventQueue");
#endif

#ifdef GF_SR_USE_VIDEO_CACHE
	tmp->cached_groups = gf_list_new();
	tmp->cached_groups_queue = gf_list_new();
#endif
	
	gf_sc_reset_framerate(tmp);	
	tmp->font_manager = gf_font_manager_new(user);
	
	tmp->extra_scenes = gf_list_new();
	tmp->interaction_level = GF_INTERACT_NORMAL | GF_INTERACT_INPUT_SENSOR | GF_INTERACT_NAVIGATION;
	return tmp;
}

GF_Compositor *gf_sc_new(GF_User *user, Bool self_threaded, GF_Terminal *term)
{
	GF_Compositor *tmp = gf_sc_create(user);
	if (!tmp) return NULL;
	tmp->term = term;

	/**/
	tmp->audio_renderer = gf_sc_ar_load(user);	
	if (!tmp->audio_renderer) GF_USER_MESSAGE(user, "", "NO AUDIO RENDERER", GF_OK);

	gf_mx_p(tmp->mx);

	/*run threaded*/
	if (self_threaded) {
		tmp->VisualThread = gf_th_new("Compositor");
		gf_th_run(tmp->VisualThread, gf_sc_proc, tmp);
		while (tmp->video_th_state!=1) {
			gf_sleep(10);
			if (tmp->video_th_state==3) {
				gf_mx_v(tmp->mx);
				gf_sc_del(tmp);
				return NULL;
			}
		}
	} else {
#ifdef GPAC_TRISCOPE_MODE	
/* scene creation*/	
	tmp->RenoirHandler = CreateRenoirScene();
#endif	
    }

	/*set default size if owning output*/
	if (!tmp->user->os_window_handler) {
		gf_sc_set_size(tmp, SC_DEF_WIDTH, SC_DEF_HEIGHT);
	}

	gf_mx_v(tmp->mx);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI]\tCompositor Cycle Log\tNetworks\tDecoders\tFrame\tDirect Draw\tVisual Config\tEvent\tRoute\tSMIL Timing\tTime node\tTexture\tSMIL Anim\tTraverse setup\tTraverse (and direct Draw)\tTraverse (and direct Draw) without anim\tIndirect Draw\tTraverse And Draw (Indirect or Not)\tFlush\tCycle\n"));

	return tmp;
}


void gf_sc_del(GF_Compositor *compositor)
{
	if (!compositor) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Destroying\n"));
	gf_sc_lock(compositor, 1);

	if (compositor->VisualThread) {
		compositor->video_th_state = 2;
		while (compositor->video_th_state!=3) gf_sleep(10);
		gf_th_del(compositor->VisualThread);
	}
	if (compositor->video_out) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Closing video output\n"));
		compositor->video_out->Shutdown(compositor->video_out);
		gf_modules_close_interface((GF_BaseInterface *)compositor->video_out);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Closing visual compositor\n"));

	gf_node_unregister(compositor->focus_highlight->node, NULL);
	drawable_del_ex(compositor->focus_highlight, compositor);

	if (compositor->selected_text) free(compositor->selected_text);
	if (compositor->sel_buffer) free(compositor->sel_buffer);

	visual_del(compositor->visual);
	gf_list_del(compositor->sensors);
	gf_list_del(compositor->previous_sensors);
	gf_list_del(compositor->visuals);
	gf_list_del(compositor->strike_bank);
	gf_list_del(compositor->hit_use_stack);
	gf_list_del(compositor->prev_hit_use_stack);
	gf_list_del(compositor->focus_ancestors);
	gf_list_del(compositor->focus_use_stack);


	gf_list_del(compositor->traverse_state->vrml_sensors);
	gf_list_del(compositor->traverse_state->use_stack);
	free(compositor->traverse_state);

#ifndef GPAC_DISABLE_3D
	if (compositor->unit_bbox) mesh_free(compositor->unit_bbox);
#endif

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Unloading visual compositor module\n"));

	if (compositor->audio_renderer) gf_sc_ar_del(compositor->audio_renderer);

#ifdef GF_SR_EVENT_QUEUE
	gf_mx_p(compositor->ev_mx);
	while (gf_list_count(compositor->events)) {
		GF_Event *ev = (GF_Event *)gf_list_get(compositor->events, 0);
		gf_list_rem(compositor->events, 0);
		free(ev);
	}
	gf_mx_v(compositor->ev_mx);
	gf_mx_del(compositor->ev_mx);
	gf_list_del(compositor->events);
#endif

	if (compositor->font_manager) gf_font_manager_del(compositor->font_manager);

#ifdef GF_SR_USE_VIDEO_CACHE
	gf_list_del(compositor->cached_groups);
	gf_list_del(compositor->cached_groups_queue);
#endif

	gf_list_del(compositor->textures);
	gf_list_del(compositor->time_nodes);
	gf_list_del(compositor->extra_scenes);
	gf_sc_lock(compositor, 0);
	gf_mx_del(compositor->mx);
	free(compositor);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Destroyed\n"));
}

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


static void gf_sc_pause(GF_Compositor *compositor, u32 PlayState)
{
	if (!compositor || !compositor->audio_renderer) return;
	if (!compositor->paused && !PlayState) return;
	if (compositor->paused && (PlayState==GF_STATE_PAUSED)) return;

	/*step mode*/
	if (PlayState==GF_STATE_STEP_PAUSE) {
		compositor->step_mode = 1;
		/*resume for next step*/
		if (compositor->paused && compositor->term) gf_term_set_option(compositor->term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
	} else {
		if (compositor->audio_renderer) gf_sc_ar_control(compositor->audio_renderer, (compositor->paused && (PlayState==0xFF)) ? 2 : compositor->paused);
		compositor->paused = (PlayState==GF_STATE_PAUSED) ? 1 : 0;
	}
}

u32 gf_sc_get_clock(GF_Compositor *compositor)
{
	return gf_sc_ar_get_clock(compositor->audio_renderer);
}

static GF_Err gf_sc_set_scene_size(GF_Compositor *compositor, u32 Width, u32 Height)
{
	if (!Width || !Height) {
		if (compositor->override_size_flags) {
			/*specify a small size to detect biggest bitmap but not 0 in case only audio..*/
			compositor->scene_height = SC_DEF_HEIGHT;
			compositor->scene_width = SC_DEF_WIDTH;
		} else {
			/*use current res*/
			compositor->scene_width = compositor->display_width ? compositor->display_width : compositor->new_width;
			compositor->scene_height = compositor->display_height ? compositor->display_height : compositor->new_height;
		}
	} else {
		compositor->scene_height = Height;
		compositor->scene_width = Width;
	}
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

	compositor->scale_x = scaleX;
	compositor->scale_y = scaleY;
	compositor->zoom_changed = 1;

	compositor_2d_set_user_transform(compositor, compositor->zoom, compositor->trans_x, compositor->trans_y, 1);
}

static void gf_sc_reset(GF_Compositor *compositor)
{
	Bool direct_draw;
	
	GF_VisualManager *visual;
	u32 i=0;
	while ((visual = (GF_VisualManager *)gf_list_enum(compositor->visuals, &i))) {
		/*reset display list*/
		visual->cur_context = visual->context;
		if (visual->cur_context) visual->cur_context->drawable = NULL;
		while (visual->prev_nodes) {
			struct _drawable_store *cur = visual->prev_nodes;
			visual->prev_nodes = cur->next;
			free(cur);
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
	direct_draw = compositor->traverse_state->direct_draw;
	gf_list_del(compositor->traverse_state->vrml_sensors);
	gf_list_del(compositor->traverse_state->use_stack);
	memset(compositor->traverse_state, 0, sizeof(GF_TraverseState));
	compositor->traverse_state->vrml_sensors = gf_list_new();
	compositor->traverse_state->use_stack = gf_list_new();
	gf_mx2d_init(compositor->traverse_state->transform);
	gf_cmx_init(&compositor->traverse_state->color_mat);
	compositor->traverse_state->direct_draw = direct_draw;

	assert(!compositor->visual->overlays);

	compositor->reset_graphics = 0;
	compositor->trans_x = compositor->trans_y = 0;
	compositor->zoom = FIX_ONE;
	compositor->grab_node = NULL;
	compositor->grab_use = NULL;
	compositor->focus_node = NULL;
	compositor->focus_text_type = 0;
	compositor->frame_number = 0;
	compositor->video_memory = 0;
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, (scene_graph ? "[Compositor] Attaching new scene\n" : "[Compositor] Detaching scene\n"));

	if (compositor->audio_renderer && (compositor->scene != scene_graph)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Reseting audio compositor\n"));
		gf_sc_ar_reset(compositor->audio_renderer);
	}

#ifdef GF_SR_EVENT_QUEUE
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Reseting event queue\n"));
	gf_mx_p(compositor->ev_mx);
	while (gf_list_count(compositor->events)) {
		GF_Event *ev = (GF_Event*)gf_list_get(compositor->events, 0);
		gf_list_rem(compositor->events, 0);
		free(ev);
	}
#endif
	
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Reseting compositor module\n"));
	/*reset main surface*/
	gf_sc_reset(compositor);

	/*set current graph*/
	compositor->scene = scene_graph;
	do_notif = 0;
	if (scene_graph) {
#ifndef GPAC_DISABLE_SVG
		SVG_Length *w, *h;
		Bool is_svg = 0;
#endif
		const char *opt;
		u32 tag;
		GF_Node *top_node;
		Bool had_size_info = compositor->has_size_info;
		/*get pixel size if any*/
		gf_sg_get_scene_size_info(compositor->scene, &width, &height);
		compositor->has_size_info = (width && height) ? 1 : 0;
		if (compositor->has_size_info != had_size_info) compositor->scene_width = compositor->scene_height = 0;

		/*default back color is black*/
		if (! (compositor->user->init_flags & GF_TERM_WINDOWLESS)) compositor->back_color = 0xFF000000;

		top_node = gf_sg_get_root_node(compositor->scene);
		tag = 0;
		if (top_node) tag = gf_node_get_tag(top_node);

#ifndef GPAC_DISABLE_SVG
		w = h = NULL;
		if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
			GF_FieldInfo info;
			is_svg = 1;
			if (gf_node_get_attribute_by_tag(top_node, TAG_SVG_ATT_width, 0, 0, &info)==GF_OK) 
				w = info.far_ptr;
			if (gf_node_get_attribute_by_tag(top_node, TAG_SVG_ATT_height, 0, 0, &info)==GF_OK) 
				h = info.far_ptr;
		}
		/*default back color is white*/
		if (is_svg && ! (compositor->user->init_flags & GF_TERM_WINDOWLESS)) compositor->back_color = 0xFFFFFFFF;

		/*hack for SVG where size is set in % - negotiate a canvas size*/
		if (!compositor->has_size_info && w && h) {
			do_notif = 1;
			if (w->type!=SVG_NUMBER_PERCENTAGE) {
				width = FIX2INT(gf_sc_svg_convert_length_to_display(compositor, w) );
			} else {
				width = SC_DEF_WIDTH;
				do_notif = 0;
			}
			if (h->type!=SVG_NUMBER_PERCENTAGE) {
				height = FIX2INT(gf_sc_svg_convert_length_to_display(compositor, h) );
			} else {
				height = SC_DEF_HEIGHT;
				do_notif = 0;
			}
		}
		/*we consider that SVG has no size onfo per say, everything is handled by the viewBox if any*/
		if (is_svg) compositor->has_size_info = 0;
#endif
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
			gf_sc_set_scene_size(compositor, width, height);

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
#ifdef GF_SR_EVENT_QUEUE
	gf_mx_v(compositor->ev_mx);
#endif
	
	gf_sc_lock(compositor, 0);
	if (scene_graph)
		compositor->draw_next_frame = 1;
	/*here's a nasty trick: the app may respond to this by calling a gf_sc_set_size from a different
	thread, but in an atomic way (typically happen on Win32 when changing the window size). WE MUST
	NOTIFY THE SIZE CHANGE AFTER RELEASING THE RENDERER MUTEX*/
	if (do_notif && compositor->user->EventProc) {
		GF_Event evt;
		/*wait for user ack*/
		compositor->draw_next_frame = 0;

		evt.type = GF_EVENT_SCENE_SIZE;
		evt.size.width = width;
		evt.size.height = height;
		compositor->user->EventProc(compositor->user->opaque, &evt);
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

GF_Err gf_sc_set_size(GF_Compositor *compositor, u32 NewWidth, u32 NewHeight)
{
	Bool lock_ok;
	if (!NewWidth || !NewHeight) {
		compositor->override_size_flags &= ~2;
		return GF_OK;
	}
	/*EXTRA CARE HERE: the caller (user app) is likely a different thread than the compositor one, and depending on window 
	manager we may get called here as a result of a message sent to user but not yet returned */
	lock_ok = gf_mx_try_lock(compositor->mx);
	
	compositor->new_width = NewWidth;
	compositor->new_height = NewHeight;
	compositor->msg_type |= GF_SR_CFG_SET_SIZE;
	
	/*if same size only request for video setup */
	if ((compositor->display_width == NewWidth) && (compositor->display_height == NewHeight) ) 
		compositor->msg_type |= GF_SR_CFG_WINDOWSIZE_NOTIF;
	
	if (lock_ok) gf_sc_lock(compositor, 0);

	return GF_OK;
}

void gf_sc_reload_config(GF_Compositor *compositor)
{
	const char *sOpt;

	/*changing drivers needs exclusive access*/
	gf_sc_lock(compositor, 1);
	
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

	compositor->text_sel_color = 0xFFAAAAFF;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "TextSelectHighlight");
	if (sOpt) sscanf(sOpt, "%x", &compositor->text_sel_color);
	if (!compositor->text_sel_color) compositor->text_sel_color = 0xFFAAAAFF;

	/*load options*/
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "DirectDraw");
	if (sOpt && ! stricmp(sOpt, "yes")) 
		compositor->traverse_state->direct_draw = 1;
	else
		compositor->traverse_state->direct_draw = 0;
	
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

#ifndef GPAC_DISABLE_3D

	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "ForceOpenGL");
	compositor->force_opengl_2d = (sOpt && !strcmp(sOpt, "yes")) ? 1 : 0;


	/*currently:
	- no tesselator for GL-ES, so use raster outlines.
	- no support for npow2 textures, and no support for DrawPixels
	*/
#ifndef GPAC_USE_OGL_ES
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "RasterOutlines");
	compositor->raster_outlines = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "EmulatePOW2");
	compositor->emul_pow2 = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "BitmapCopyPixels");
	compositor->bitmap_use_pixels = (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0;
#else
	compositor->raster_outlines = 1;
	compositor->emul_pow2 = 1;
	compositor->bitmap_use_pixels = 0;
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

#endif
	
#ifdef GPAC_TRISCOPE_MODE
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "3dsDepthBuffGain");
	if (sOpt) sscanf(sOpt, "%f", &((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthGain); else ((GF_RenoirHandler *) compositor->RenoirHandler)->MapDepthGain = 1;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "3dsDepthBuffOffset");
	if (sOpt) sscanf(sOpt, "%f", &((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthOffset); else ((GF_RenoirHandler *) compositor->RenoirHandler)->MapDepthOffset = 0;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "FlatDepthBuffGain");
	if (sOpt) sscanf(sOpt, "%f", &((GF_RenoirHandler *)compositor->RenoirHandler)->FlatDepthGain); else ((GF_RenoirHandler *) compositor->RenoirHandler)->FlatDepthGain = 1.0;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "OGLDepthBuffGain");
	if (sOpt) sscanf(sOpt, "%f", &compositor->OGLDepthGain); else compositor->OGLDepthGain = 1.0;
	sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "OGLDepthBuffOffset");
	if (sOpt) sscanf(sOpt, "%f", &compositor->OGLDepthOffset); else compositor->OGLDepthOffset = 0;
#endif

	
	/*RECT texture support - we must reload HW*/
	compositor->reset_graphics = 1;

	compositor->draw_next_frame = 1;

	gf_sc_lock(compositor, 0);
}

GF_Err gf_sc_set_option(GF_Compositor *compositor, u32 type, u32 value)
{
	GF_Err e;
	gf_sc_lock(compositor, 1);

	e = GF_OK;
	switch (type) {
	case GF_OPT_PLAY_STATE: 
		gf_sc_pause(compositor, value); 
		break;
	case GF_OPT_AUDIO_VOLUME: gf_sc_ar_set_volume(compositor->audio_renderer, value); break;
	case GF_OPT_AUDIO_PAN: gf_sc_ar_set_pan(compositor->audio_renderer, value); break;
	case GF_OPT_OVERRIDE_SIZE:
		compositor->override_size_flags = value ? 1 : 0;
		compositor->draw_next_frame = 1; 
		break;
	case GF_OPT_STRESS_MODE: 
		compositor->stress_mode = value; 
		break;
	case GF_OPT_ANTIALIAS: 
		compositor->antiAlias = value; 
		compositor->draw_next_frame = 1; 
		break;
	case GF_OPT_HIGHSPEED: 
		compositor->high_speed = value; 
		compositor->draw_next_frame = 1; 
		break;
	case GF_OPT_DRAW_BOUNDS: 
		compositor->draw_bvol = value; 
		compositor->draw_next_frame = 1; 
		break;
	case GF_OPT_TEXTURE_TEXT: 
		compositor->texture_text_mode = value; 
		compositor->draw_next_frame = 1; 
		break;
	case GF_OPT_ASPECT_RATIO: 
		compositor->aspect_ratio = value; 
		compositor->msg_type |= GF_SR_CFG_AR;
		break;
	case GF_OPT_INTERACTION_LEVEL: 
		compositor->interaction_level = value; 
		compositor->draw_next_frame = 1; 
		break;
	case GF_OPT_REFRESH: 
		compositor->reset_graphics = value; 
		break;
	case GF_OPT_FULLSCREEN:
		if (compositor->fullscreen != value) compositor->msg_type |= GF_SR_CFG_FULLSCREEN; 
		compositor->draw_next_frame = 1;
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
		compositor->draw_next_frame = 1; 
		break;
	case GF_OPT_FREEZE_DISPLAY: 
		compositor->freeze_display = value;
		compositor->draw_next_frame = 1; 
		break;
	case GF_OPT_RELOAD_CONFIG: 
		gf_sc_reload_config(compositor);
		compositor->draw_next_frame = 1; 
		break;
	case GF_OPT_DIRECT_DRAW:
		compositor->traverse_state->direct_draw = value ? 1 : 0;
		/*force redraw*/
		compositor->draw_next_frame = 1;
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
			if (value) compositor->reset_graphics = 1;
			/*force redraw*/
			compositor->draw_next_frame = 1;
		}
		break;
	case GF_OPT_YUV_HARDWARE:
		compositor->enable_yuv_hw = value;
		break;
	case GF_OPT_NAVIGATION_TYPE: 
		compositor_2d_set_user_transform(compositor, FIX_ONE, 0, 0, 0);
#ifndef GPAC_DISABLE_3D
		compositor_3d_reset_camera(compositor);
#endif
		break;
	case GF_OPT_NAVIGATION:
		if (compositor->navigation_disabled) {
			e = GF_NOT_SUPPORTED;
		} else
#ifndef GPAC_DISABLE_3D
		if (compositor->visual->type_3d || compositor->active_layer) {
			GF_Camera *cam = compositor_3d_get_camera(compositor);
			if (cam->navigation_flags & NAV_ANY) {
				/*if not specifying mode, try to (un)bind top*/
				if (!value) {
					if (compositor->active_layer) {
						compositor_layer3d_bind_camera(compositor->active_layer, 0, value);
					} else {
						GF_Node *n = (GF_Node*)gf_list_get(compositor->visual->navigation_stack, 0);
						if (n) Bindable_SetSetBind(n, 0);
						else cam->navigate_mode = value;
					}
				} else {
					cam->navigate_mode = value;
				}
				break;
			}
		} else 
#endif
		{
			if ((value!=GF_NAVIGATE_NONE) && (value!=GF_NAVIGATE_SLIDE) && (value!=GF_NAVIGATE_EXAMINE)) {
				e = GF_NOT_SUPPORTED;
				break;
			}
			compositor->navigate_mode = value;
			break;
		}
		e = GF_NOT_SUPPORTED;
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

	case GF_OPT_EMULATE_POW2: compositor->emul_pow2 = value; break;
	case GF_OPT_POLYGON_ANTIALIAS: compositor->poly_aa = value; break;
	case GF_OPT_BACK_CULL: compositor->backcull = value; break;
	case GF_OPT_WIREFRAME: compositor->wiremode = value; break;
	case GF_OPT_NORMALS: compositor->draw_normals = value; break;
#ifndef GPAC_USE_OGL_ES
	case GF_OPT_RASTER_OUTLINES: compositor->raster_outlines = value; break;
#endif

	case GF_OPT_NO_RECT_TEXTURE:
		if (value != compositor->disable_rect_ext) {
			compositor->disable_rect_ext = value;
			/*RECT texture support - we must reload HW*/
			compositor->reset_graphics = 1;
		}
		break;
	case GF_OPT_BITMAP_COPY: compositor->bitmap_use_pixels = value; break;
	case GF_OPT_COLLISION: compositor->collide_mode = value; break;
	case GF_OPT_GRAVITY: 
	{	
		GF_Camera *cam = compositor_3d_get_camera(compositor);
		compositor->gravity_on = value;
		/*force collision pass*/
		cam->last_pos.z -= 1;
		compositor->draw_next_frame = 1;
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

u32 gf_sc_get_option(GF_Compositor *compositor, u32 type)
{
	switch (type) {
	case GF_OPT_PLAY_STATE: return compositor->paused ? 1 : 0;
	case GF_OPT_OVERRIDE_SIZE: return (compositor->override_size_flags & 1) ? 1 : 0;
	case GF_OPT_IS_FINISHED:
	{
		if (compositor->interaction_sensors) return 0;
		if (gf_list_count(compositor->time_nodes)) return 0;
		return 1;
	}
	case GF_OPT_STRESS_MODE: return compositor->stress_mode;
	case GF_OPT_AUDIO_VOLUME: return compositor->audio_renderer->volume;
	case GF_OPT_AUDIO_PAN: return compositor->audio_renderer->pan;
	case GF_OPT_ANTIALIAS: return compositor->antiAlias;
	case GF_OPT_HIGHSPEED: return compositor->high_speed;
	case GF_OPT_ASPECT_RATIO: return compositor->aspect_ratio;
	case GF_OPT_FULLSCREEN: return compositor->fullscreen;
	case GF_OPT_INTERACTION_LEVEL: return compositor->interaction_level;
	case GF_OPT_VISIBLE: return !compositor->is_hidden;
	case GF_OPT_FREEZE_DISPLAY: return compositor->freeze_display;
	case GF_OPT_TEXTURE_TEXT: return compositor->texture_text_mode;
	case GF_OPT_USE_OPENGL: return compositor->force_opengl_2d;

	case GF_OPT_DIRECT_DRAW: return compositor->traverse_state->direct_draw ? 1 : 0;
	case GF_OPT_SCALABLE_ZOOM: return compositor->scalable_zoom;
	case GF_OPT_YUV_HARDWARE: return compositor->enable_yuv_hw;
	case GF_OPT_YUV_FORMAT: return compositor->enable_yuv_hw ? compositor->video_out->yuv_pixel_format : 0;
	case GF_OPT_NAVIGATION_TYPE: 
#ifndef GPAC_DISABLE_3D
		if (compositor->visual->type_3d || compositor->active_layer) {
			GF_Camera *cam = compositor_3d_get_camera(compositor);
			if (!(cam->navigation_flags & NAV_ANY)) return GF_NAVIGATE_TYPE_NONE;
			return ((cam->is_3D || compositor->active_layer) ? GF_NAVIGATE_TYPE_3D : GF_NAVIGATE_TYPE_2D);
		} else 
#endif
		{
			return compositor->navigation_disabled ? GF_NAVIGATE_TYPE_NONE : GF_NAVIGATE_TYPE_2D;
		}
	case GF_OPT_NAVIGATION: 
#ifndef GPAC_DISABLE_3D
		if (compositor->visual->type_3d || compositor->active_layer) {
			GF_Camera *cam = compositor_3d_get_camera(compositor);
			return cam->navigate_mode;
		} 
#endif
		return compositor->navigate_mode;

	case GF_OPT_HEADLIGHT: return 0;
	case GF_OPT_COLLISION: return GF_COLLISION_NONE;
	case GF_OPT_GRAVITY: return 0;

	case GF_OPT_VIDEO_CACHE_SIZE:
#ifdef GF_SR_USE_VIDEO_CACHE
		return compositor->video_cache_max_size/1024;
#else
		return 0;
#endif
		break;
	default: return 0;
	}
}

void gf_sc_map_point(GF_Compositor *compositor, s32 X, s32 Y, Fixed *bifsX, Fixed *bifsY)
{
	/*coordinates are in user-like OS....*/
	X = X - compositor->display_width/2;
	Y = compositor->display_height/2 - Y;
	*bifsX = INT2FIX(X);
	*bifsY = INT2FIX(Y);
}


GF_Err gf_sc_get_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer, u32 depth_dump_mode)
{
	GF_Err e;
	if (!compositor || !framebuffer) return GF_BAD_PARAM;
	gf_mx_p(compositor->mx);

#ifndef GPAC_DISABLE_3D
	if (compositor->visual->type_3d) e = compositor_3d_get_screen_buffer(compositor, framebuffer, depth_dump_mode);
	else
#endif
	/*no depth dump in 2D mode*/
	if (depth_dump_mode) e = GF_NOT_SUPPORTED;
	else e = compositor->video_out->LockBackBuffer(compositor->video_out, framebuffer, 1);
	
	if (e != GF_OK) gf_mx_v(compositor->mx);
	return e;
}

GF_Err gf_sc_release_screen_buffer(GF_Compositor *compositor, GF_VideoSurface *framebuffer)
{
	GF_Err e;
	if (!compositor || !framebuffer) return GF_BAD_PARAM;

#ifndef GPAC_DISABLE_3D
	if (compositor->visual->type_3d) e = compositor_3d_release_screen_buffer(compositor, framebuffer);
	else
#endif
	e = compositor->video_out->LockBackBuffer(compositor->video_out, framebuffer, 0);

	gf_mx_v(compositor->mx);
	return e;
}

Double gf_sc_get_fps(GF_Compositor *compositor, Bool absoluteFPS)
{
	Double fps;
	u32 ind, num, frames, run_time;

	/*start from last frame and get first frame time*/
	ind = compositor->current_frame;
	frames = 0;
	run_time = compositor->frame_time[ind];
	for (num=0; num<GF_SR_FPS_COMPUTE_SIZE; num++) {
		if (absoluteFPS) {
			run_time += compositor->frame_time[ind];
		} else {
			run_time += MAX(compositor->frame_time[ind], compositor->frame_duration);
		}
		frames++;
		if (frames==GF_SR_FPS_COMPUTE_SIZE) break;
		if (!ind) {
			ind = GF_SR_FPS_COMPUTE_SIZE;
		} else {
			ind--;
		}
	}
	if (!run_time) return (Double) compositor->frame_rate;
	fps = 1000*frames;
	fps /= run_time;
	return fps;
}

void gf_sc_register_time_node(GF_Compositor *compositor, GF_TimeNode *tn)
{
	/*may happen with DEF/USE */
	if (tn->is_registered) return;
	if (tn->needs_unregister) return;
	gf_list_add(compositor->time_nodes, tn);
	tn->is_registered = 1;
}
void gf_sc_unregister_time_node(GF_Compositor *compositor, GF_TimeNode *tn)
{
	gf_list_del_item(compositor->time_nodes, tn);
}



GF_Node *gf_sc_pick_node(GF_Compositor *compositor, s32 X, s32 Y)
{
	return NULL;
}

static void gf_sc_forward_event(GF_Compositor *compositor, GF_Event *ev)
{
	GF_USER_SENDEVENT(compositor->user, ev);

	if ((ev->type==GF_EVENT_MOUSEUP) && (ev->mouse.button==GF_MOUSE_LEFT)) {
		u32 now;
		GF_Event event;
		/*emulate doubleclick*/
		now = gf_sys_clock();
		if (now - compositor->last_click_time < DOUBLECLICK_TIME_MS) {
			event.type = GF_EVENT_DBLCLICK;
			event.mouse.key_states = compositor->key_states;
			event.mouse.x = ev->mouse.x;
			event.mouse.y = ev->mouse.y;
			GF_USER_SENDEVENT(compositor->user, &event);
		}
		compositor->last_click_time = now;
	}
}

static void gf_sc_setup_root_visual(GF_Compositor *compositor, GF_Node *top_node)
{
	if (top_node && !compositor->root_visual_setup) {
		u32 node_tag;
#ifndef GPAC_DISABLE_3D
		Bool was_3d = compositor->visual->type_3d;
#endif
		compositor->root_visual_setup = 1;
		compositor->visual->center_coords = 1;

		compositor->traverse_state->pixel_metrics = 1;
		compositor->traverse_state->min_hsize = INT2FIX(MIN(compositor->scene_width, compositor->scene_height)) / 2;

		node_tag = gf_node_get_tag(top_node);
		switch (node_tag) {
		case TAG_MPEG4_OrderedGroup:
		case TAG_MPEG4_Layer2D:
#ifndef GPAC_DISABLE_3D
			compositor->visual->type_3d = 0;
			compositor->visual->camera.is_3D = 0;
#endif
			compositor->traverse_state->pixel_metrics = gf_sg_use_pixel_metrics(compositor->scene);
			break;
		case TAG_MPEG4_Group:
		case TAG_MPEG4_Layer3D:
#ifndef GPAC_DISABLE_3D
			compositor->visual->type_3d = 2;
			compositor->visual->camera.is_3D = 1;
#endif
			compositor->traverse_state->pixel_metrics = gf_sg_use_pixel_metrics(compositor->scene);
			break;
		case TAG_X3D_Group:
#ifndef GPAC_DISABLE_3D
			compositor->visual->type_3d = 3;
#endif
			compositor->traverse_state->pixel_metrics = gf_sg_use_pixel_metrics(compositor->scene);
			break;
#ifndef GPAC_DISABLE_SVG
		case TAG_SVG_svg:
#ifndef GPAC_DISABLE_3D
			compositor->visual->type_3d = 0;
			compositor->visual->camera.is_3D = 0;
#endif
			compositor->visual->center_coords = 0;
			compositor->root_visual_setup = 2;
			break;
#endif
		}

		/*!! by default we don't set the focus on the content - this is conform to SVG and avoids displaying the 
		focus box for MPEG-4 !! */

		/*setup OpenGL & camera mode*/
#ifndef GPAC_DISABLE_3D
		/*request for OpenGL drawing in 2D*/
		if (compositor->force_opengl_2d && !compositor->visual->type_3d)
			compositor->visual->type_3d = 1;

		if (! (compositor->video_out->hw_caps & GF_VIDEO_HW_OPENGL)) {
			compositor->visual->type_3d = 0;
			compositor->visual->camera.is_3D = 0;
		}
		compositor->visual->camera.is_3D = (compositor->visual->type_3d>1) ? 1 : 0;
		camera_invalidate(&compositor->visual->camera);
#endif

		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Main scene setup - pixel metrics %d - center coords %d\n", compositor->traverse_state->pixel_metrics, compositor->visual->center_coords));

#ifndef GPAC_DISABLE_3D
		/*change in 2D/3D config, force AR recompute/video restup*/
		if (was_3d != compositor->visual->type_3d) compositor->recompute_ar = was_3d ? 1 : 2;
#endif

		compositor->recompute_ar = 1;
	}

#ifndef GPAC_DISABLE_LOG
	compositor->visual_config_time = 0;
#endif
	if (compositor->recompute_ar) {
#ifndef GPAC_DISABLE_LOG
		u32 time=0;
		
		if ((gf_log_get_level() >= GF_LOG_DEBUG) && (gf_log_get_tools() & GF_LOG_RTI)) { 
			time = gf_sys_clock();
		}
#endif

#ifndef GPAC_DISABLE_3D
		if (compositor->visual->type_3d) {
			compositor_3d_set_aspect_ratio(compositor);
			gf_sc_load_opengl_extensions(compositor);
		}
		else
#endif
			compositor_2d_set_aspect_ratio(compositor);

		compositor->draw_next_frame = 0;

#ifndef GPAC_DISABLE_LOG
		if ((gf_log_get_level() >= GF_LOG_DEBUG) && (gf_log_get_tools() & GF_LOG_RTI)) { 
			compositor->visual_config_time = gf_sys_clock() - time;
		}
#endif
	} 
}

static void gf_sc_draw_scene(GF_Compositor *compositor)
{
	u32 flags;

	GF_Node *top_node = gf_sg_get_root_node(compositor->scene);

	if (!top_node && !compositor->visual->last_had_back && !compositor->visual->cur_context) {
		//GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Scene has no root node, nothing to draw\n"));
		return;
	}

	gf_sc_setup_root_visual(compositor, top_node);

#ifdef GF_SR_USE_VIDEO_CACHE
	if (!compositor->video_cache_max_size)
		compositor->traverse_state->in_group_cache = 1;
#endif

	flags = compositor->traverse_state->direct_draw;


	visual_draw_frame(compositor->visual, top_node, compositor->traverse_state, 1);


#ifdef GPAC_TRISCOPE_MODE
	/*here comes renoir rendering*/
	RenderRenoirScene (compositor->RenoirHandler);	
#endif
	compositor->traverse_state->direct_draw = flags;
#ifdef GF_SR_USE_VIDEO_CACHE
	gf_list_reset(compositor->cached_groups_queue);
#endif
	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Drawing done\n"));

	/*only send the resize notification once the frame has been dra*/
	if (compositor->recompute_ar) {
		compositor_send_resize_event(compositor, 0, 0, 0, 1);
		compositor->recompute_ar = 0;
	}
	compositor->zoom_changed = 0;
}


#ifndef GPAC_DISABLE_LOG
extern u32 time_spent_in_anim;
#endif

void gf_sc_simulation_tick(GF_Compositor *compositor)
{	
	GF_SceneGraph *sg;
	u32 in_time, end_time, i, count;
	Bool frame_drawn;
#ifndef GPAC_DISABLE_LOG
	s32 event_time, route_time, smil_timing_time, time_node_time, texture_time, traverse_time, flush_time;
#endif
#ifdef TRISCOPE_TEST_FRAMERATE
    double drawing_time;
    static u32 timer_count=0;
#endif
	/*lock compositor for the whole cycle*/
	gf_sc_lock(compositor, 1);

	/*first thing to do, let the video output handle user event if it is not threaded*/
	compositor->video_out->ProcessEvent(compositor->video_out, NULL);

	if (compositor->freeze_display) {
		gf_sc_lock(compositor, 0);
		gf_sleep(compositor->frame_duration);
		return;
	}

	gf_sc_reconfig_task(compositor);

	/* if there is no scene, we draw a black screen to flush the screen */
	if (!compositor->scene && !gf_list_count(compositor->extra_scenes) ) {
		gf_sc_draw_scene(compositor);
		gf_sc_lock(compositor, 0);
		gf_sleep(compositor->frame_duration);
		return;
	}

	in_time = gf_sys_clock();
#ifdef TRISCOPE_TEST_FRAMERATE
     TIMER_START;
#endif
	if (compositor->reset_graphics)
		compositor->draw_next_frame = 1;

#ifdef GF_SR_EVENT_QUEUE
	/*process pending user events*/
#ifndef GPAC_DISABLE_LOG
	event_time = gf_sys_clock();
#endif
	gf_mx_p(compositor->ev_mx);
	while (gf_list_count(compositor->events)) {
		GF_Event *ev = (GF_Event*)gf_list_get(compositor->events, 0);
		gf_list_rem(compositor->events, 0);
		if (!gf_sc_exec_event(compositor, ev)) {
			gf_sc_forward_event(compositor, ev);
		}
		free(ev);
	}
	gf_mx_v(compositor->ev_mx);
#ifndef GPAC_DISABLE_LOG
	event_time = gf_sys_clock() - event_time;
#endif
#else
	event_time = 0;
#endif

	gf_term_sample_clocks(compositor->term);
	
	/*setup root visual before triggering animations in case they in turn use the viewport*/
//	if (!compositor->root_visual_setup)
//		gf_sc_setup_root_visual(compositor, gf_sg_get_root_node(compositor->scene));

#ifndef GPAC_DISABLE_LOG
	route_time = gf_sys_clock();
#endif
	/*execute all routes before updating textures, otherwise nodes inside composite texture may never see their
	dirty flag set*/
	gf_sg_activate_routes(compositor->scene);
	i = 0;
	while ((sg = (GF_SceneGraph*)gf_list_enum(compositor->extra_scenes, &i))) {
		gf_sg_activate_routes(sg);
	}
#ifndef GPAC_DISABLE_LOG
	route_time = gf_sys_clock() - route_time;
#endif

#ifndef GPAC_DISABLE_SVG
#if SVG_FIXME
	{ /* Experimental (Not SVG compliant system events (i.e. battery, cpu ...) triggered to the root node)*/
		GF_Node *root = gf_sg_get_root_node(compositor->scene);
		GF_DOM_Event evt;
		if (gf_dom_listener_count(root)) {
			u32 i, count;
			count = gf_dom_listener_count(root);
			for (i=0;i<count; i++) {
				SVG_SA_listenerElement *l = gf_dom_listener_get(root, i);
				if (l->event.type == GF_EVENT_CPU) {
					GF_SystemRTInfo sys_rti;
					if (gf_sys_get_rti(500, &sys_rti, GF_RTI_ALL_PROCESSES_TIMES)) {
						evt.type = GF_EVENT_CPU;
						evt.cpu_percentage = sys_rti.total_cpu_usage;
						//printf("%d\n",sys_rti.total_cpu_usage);
						gf_dom_event_fire(root, NULL, &evt);
					} 
				} else if (l->event.type == GF_EVENT_BATTERY) { //&& l->observer.target == (SVG_SA_Element *)node) {
					evt.type = GF_EVENT_BATTERY;
					gf_sys_get_battery_state(&evt.onBattery, &evt.batteryState, &evt.batteryLevel);
					gf_dom_event_fire(root, NULL, &evt);
				}
			}
		}
	}
#endif

#ifndef GPAC_DISABLE_LOG
	smil_timing_time = gf_sys_clock();
#endif
	if (gf_smil_notify_timed_elements(compositor->scene)) {
		compositor->draw_next_frame = 1;
	}
	i = 0;
	while ((sg = (GF_SceneGraph*)gf_list_enum(compositor->extra_scenes, &i))) {
		if (gf_smil_notify_timed_elements(sg)) {
			compositor->draw_next_frame = 1;
		}
	}

#ifndef GPAC_DISABLE_LOG
	smil_timing_time = gf_sys_clock() - smil_timing_time;
#endif

#endif

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
	}
#ifndef GPAC_DISABLE_LOG
	time_node_time = gf_sys_clock() - time_node_time;
#endif

#ifndef GPAC_DISABLE_LOG
	texture_time = gf_sys_clock();
#endif
	/*update all textures*/
	count = gf_list_count(compositor->textures);
	for (i=0; i<count; i++) {
		GF_TextureHandler *txh = (GF_TextureHandler *)gf_list_get(compositor->textures, i);
		/*signal graphics reset before updating*/
		if (compositor->reset_graphics && txh->tx_io) gf_sc_texture_reset(txh);
		txh->update_texture_fcnt(txh);
	}
	compositor->reset_graphics = 0;

#ifndef GPAC_DISABLE_LOG
	texture_time = gf_sys_clock() - texture_time;
#endif


	frame_drawn = (compositor->draw_next_frame==1) ? 1 : 0;

	/*if invalidated, draw*/
	if (compositor->draw_next_frame) {
		GF_Window rc;
#ifndef GPAC_DISABLE_LOG
		traverse_time = gf_sys_clock();
		time_spent_in_anim = 0;
#endif

		/*video flush only*/
		if (compositor->draw_next_frame==2) {
			compositor->draw_next_frame = 0;
		} else {
			compositor->draw_next_frame = 0;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPOSE, ("[Compositor] Redrawing scene\n"));
			gf_sc_draw_scene(compositor);
#ifndef GPAC_DISABLE_LOG
			traverse_time = gf_sys_clock() - traverse_time;
#endif
		}
		/*and flush*/
#ifndef GPAC_DISABLE_LOG
		flush_time = gf_sys_clock();
#endif

		if(compositor->user->init_flags & GF_TERM_INIT_HIDE) compositor->skip_flush = 1;

		if (!compositor->skip_flush) {
			rc.x = rc.y = 0; 
			rc.w = compositor->display_width;	
			rc.h = compositor->display_height;		
			compositor->video_out->Flush(compositor->video_out, &rc);
		} else {
			compositor->skip_flush = 0;
		}
#ifndef GPAC_DISABLE_LOG
		flush_time = gf_sys_clock() - flush_time;
#endif

		visual_2d_draw_overlays(compositor->visual);
		compositor->last_had_overlays = compositor->visual->has_overlays;

		if (compositor->stress_mode) {
			compositor->draw_next_frame = 1;
			compositor->reset_graphics = 1;
		}
		compositor->reset_fonts = 0;
	} else {
#ifndef GPAC_DISABLE_LOG
		traverse_time = 0;
		time_spent_in_anim = 0;
		flush_time = 0;
		compositor->traverse_setup_time = 0;
		compositor->traverse_and_direct_draw_time = 0;
		compositor->indirect_draw_time = 0;
#endif
	}

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

	end_time = gf_sys_clock() - in_time;

#ifdef TRISCOPE_TEST_FRAMERATE
TIMER_STOP;
drawing_time=TIMER_ELAPSED;
printf("frame: %d ", timer_count) ;
printf(" %f sec\n", drawing_time/1000000.0) ;
timer_count++;
#endif

	GF_LOG(GF_LOG_DEBUG, GF_LOG_RTI, ("[RTI]\tCompositor Cycle Log\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", 
		compositor->networks_time, 
		compositor->decoders_time, 
		compositor->frame_number, 
		compositor->traverse_state->direct_draw,
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

	gf_sc_lock(compositor, 0);

	compositor->current_frame = (compositor->current_frame+1) % GF_SR_FPS_COMPUTE_SIZE;
	compositor->frame_time[compositor->current_frame] = end_time;

	compositor->frame_number++;

	/*step mode on, pause and return*/
	if (compositor->step_mode) {
		compositor->step_mode = 0;
		if (compositor->term) gf_term_set_option(compositor->term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		return;
	}
	/*not threaded, let the owner decide*/
	if ((compositor->user->init_flags & GF_TERM_NO_VISUAL_THREAD) || !compositor->frame_duration) return;

	/*compute sleep time till next frame, otherwise we'll kill the CPU*/
	i = end_time / compositor->frame_duration + 1;
//	while (i * compositor->frame_duration < end_time) i++;
	in_time = i * compositor->frame_duration - end_time;
	gf_sleep(in_time);
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

void gf_sc_traverse_subscene(GF_Compositor *compositor, GF_Node *inline_parent, GF_SceneGraph *subscene, void *rs)
{
	Fixed min_hsize;
	Bool use_pm, prev_pm, prev_coord;
	SFVec2f prev_vp;
	s32 flip_coords;
	u32 h, w, tag;
	GF_Matrix2D transf;
	GF_SceneGraph *in_scene;
	GF_Node *inline_root = gf_sg_get_root_node(subscene);
	GF_TraverseState *tr_state = (GF_TraverseState *)rs;
	if (!inline_root) return;

	flip_coords = 0;
	in_scene = gf_node_get_graph(inline_root);
	w = h = 0;

	/*check parent/child doc orientation*/

	/*check child doc*/
	tag = gf_node_get_tag(inline_root);
	if (tag < GF_NODE_RANGE_LAST_VRML) {
		u32 new_tag = 0;
		use_pm = gf_sg_use_pixel_metrics(in_scene);
		if (gf_node_get_tag(inline_parent)>GF_NODE_RANGE_LAST_VRML) {
			/*moving from SVG to VRML-based, need positive translation*/
			flip_coords = 1;

			/*if our root nodes are not LayerXD ones, insert one. This prevents mixing
			of bindable stacks and gives us free 2D/3D integration*/
			if (tag==TAG_MPEG4_OrderedGroup) {
				new_tag = TAG_MPEG4_Layer2D;
			} else if ((tag==TAG_MPEG4_Group) || (tag==TAG_X3D_Group)) {
				new_tag = TAG_MPEG4_Layer3D;
			}
		}
#ifndef GPAC_DISABLE_3D
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
	gf_mx2d_init(transf);

	/*compute center<->top-left transform*/
	if (flip_coords)
		gf_mx2d_add_scale(&transf, FIX_ONE, -FIX_ONE);

	/*if scene size is given in the child document, scale to fit the entire vp*/
	if (w && h) 
		gf_mx2d_add_scale(&transf, tr_state->vp_size.x/w, tr_state->vp_size.y/h);
	if (flip_coords) {
		gf_mx2d_add_translation(&transf, flip_coords * tr_state->vp_size.x/2, tr_state->vp_size.y/2);
		tr_state->fliped_coords = !tr_state->fliped_coords;
	}

	/*if scene size is given in the child document, scale back vp to take into account the above scale
	otherwise the scene won't be properly clipped*/
	if (w && h) {
		tr_state->vp_size.x = INT2FIX(w);
		tr_state->vp_size.y = INT2FIX(h);
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
		gf_mx_add_matrix(&mx, &tr_state->model_matrix);
		gf_mx_copy(tr_state->model_matrix, mx);

		if (tr_state->traversing_mode==TRAVERSE_SORT) {
			visual_3d_matrix_push(tr_state->visual);
			visual_3d_matrix_add(tr_state->visual, mx.m);
			gf_node_traverse(inline_root, rs);
			visual_3d_matrix_pop(tr_state->visual);
		} else {
			gf_node_traverse(inline_root, rs);
		}
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
#ifdef GF_SR_EVENT_QUEUE
	GF_Event *ev;
#else
	Bool ret;
#endif

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
#ifdef GF_SR_EVENT_QUEUE
	switch (event->type) {
	case GF_EVENT_MOUSEMOVE:
	{
		u32 i, count;
		gf_mx_p(compositor->ev_mx);
		count = gf_list_count(compositor->events);
		for (i=0; i<count; i++) {
			ev = (GF_Event *)gf_list_get(compositor->events, i);
			if (ev->type == GF_EVENT_MOUSEMOVE) {
				ev->mouse =  event->mouse;
				gf_mx_v(compositor->ev_mx);
				return 1;
			}
		}
		gf_mx_v(compositor->ev_mx);
	}
	default:
		ev = (GF_Event *)malloc(sizeof(GF_Event));
		ev->type = event->type;
		if (event->type<=GF_EVENT_MOUSEWHEEL) {
			ev->mouse = event->mouse;
		} else if (event->type==GF_EVENT_TEXTINPUT) {
			ev->character = event->character;
		} else {
			ev->key = event->key;
		}
		gf_mx_p(compositor->ev_mx);
		gf_list_add(compositor->events, ev);
		gf_mx_v(compositor->ev_mx);
		break;
	}
	return 0;
#else
	gf_sc_lock(compositor, 1);
	ret = gf_sc_exec_event(compositor, event);
	gf_sc_lock(compositor, 0);
	if (!ret && !from_user) {
		gf_sc_forward_event(compositor, event);
	}
	return ret;
#endif
}


static Bool gf_sc_on_event_ex(GF_Compositor *compositor , GF_Event *event, Bool from_user)
{
	/*not assigned yet*/
	if (!compositor || !compositor->visual) return 0;
	/*we're reconfiguring the video output, cancel all messages*/
	if (compositor->msg_type & GF_SR_IN_RECONFIG) return 0;

	switch (event->type) {
	case GF_EVENT_MOVE:
	case GF_EVENT_REFRESH:
		if (!compositor->draw_next_frame) {
			/*when refreshing the window in 3D or with overlays we redraw the scene */
			if (compositor->last_had_overlays 
#ifndef GPAC_DISABLE_3D
				|| compositor->visual->type_3d
#endif
				) {
				compositor->draw_next_frame = 1;
			}
			/*reflush only*/
			else 
				compositor->draw_next_frame = 2;
		}
		break;
	case GF_EVENT_VIDEO_SETUP:
		gf_sc_reset_graphics(compositor);
		break;
	case GF_EVENT_SIZE:
		/*resize message from plugin: if we own the output, resize*/
		if (!compositor->user->os_window_handler) {
			/*EXTRA CARE HERE: the caller (video output) is likely a different thread than the compositor one, and the
			compositor may be locked on the video output (flush or whatever)!!
			*/
			Bool lock_ok = gf_mx_try_lock(compositor->mx);
			compositor->new_width = event->size.width;
			compositor->new_height = event->size.height;
			compositor->msg_type |= GF_SR_CFG_SET_SIZE;
			if (lock_ok) gf_sc_lock(compositor, 0);
		}
		/*otherwise let the user decide*/
		else {
			return GF_USER_SENDEVENT(compositor->user, event);
		}
		break;

	case GF_EVENT_KEYDOWN:
	case GF_EVENT_KEYUP:
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
		
#ifdef GPAC_TRISCOPE_MODE
		case GF_KEY_JOYSTICK:
			if (event->type==GF_EVENT_KEYUP) {
				const char *sOpt;
				switch (event->key.hw_code) {
				case 4:
					sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "3dsDepthBuffGain");
					if (sOpt) sscanf(sOpt, "%f", &((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthGain); else ((GF_RenoirHandler *) compositor->RenoirHandler)->MapDepthGain = 1;
					((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthGain -= 5;
					sprintf(sOpt, "%f", ((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthGain);
					gf_cfg_set_key(compositor->user->config, "Compositor", "3dsDepthBuffGain", sOpt);
					/*we'll change it also for flat objects, thus both will be the same*/
					gf_cfg_set_key(compositor->user->config, "Compositor", "FlatDepthBuffGain", sOpt);	
					gf_cfg_save(compositor->user->config);
					return 0;

				case 5:
					sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "3dsDepthBuffOffset");
					if (sOpt) sscanf(sOpt, "%f", &((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthOffset); else ((GF_RenoirHandler *) compositor->RenoirHandler)->MapDepthOffset = 0;
					((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthOffset -= 5;
					sprintf(sOpt, "%f", ((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthOffset);
					gf_cfg_set_key(compositor->user->config, "Compositor", "3dsDepthBuffOffset", sOpt);
					gf_cfg_save(compositor->user->config);
					return 0;
					
				case 6:
					sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "3dsDepthBuffGain");
					if (sOpt) sscanf(sOpt, "%f", &((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthGain); else ((GF_RenoirHandler *) compositor->RenoirHandler)->MapDepthGain = 1;
					((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthGain += 5;
					sprintf(sOpt, "%f", ((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthGain);
					gf_cfg_set_key(compositor->user->config, "Compositor", "3dsDepthBuffGain", sOpt);
					/*we'll change it also for flat objects, thus both will be the same*/
					gf_cfg_set_key(compositor->user->config, "Compositor", "FlatDepthBuffGain", sOpt);
					gf_cfg_save(compositor->user->config);
					return 0;
					
				case 7:
					sOpt = gf_cfg_get_key(compositor->user->config, "Compositor", "3dsDepthBuffOffset");
					if (sOpt) sscanf(sOpt, "%f", &((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthOffset); else ((GF_RenoirHandler *) compositor->RenoirHandler)->MapDepthOffset = 0;
					((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthOffset += 5;
					sprintf(sOpt, "%f", ((GF_RenoirHandler *)compositor->RenoirHandler)->MapDepthOffset);
					gf_cfg_set_key(compositor->user->config, "Compositor", "3dsDepthBuffOffset", sOpt);
					gf_cfg_save(compositor->user->config);
					return 0;
					
				default:
				    break;
				}
			}
			break;
#endif
		}	
		
		event->key.flags |= compositor->key_states;
		/*key sensor*/
		if (compositor->term && (compositor->interaction_level & GF_INTERACT_INPUT_SENSOR) ) {
			gf_term_keyboard_input(compositor->term, event->key.key_code, event->key.hw_code, (event->type==GF_EVENT_KEYUP) ? 1 : 0);
		}	
		
		return gf_sc_handle_event_intern(compositor, event, from_user);

	case GF_EVENT_TEXTINPUT:
		if (compositor->term && (compositor->interaction_level & GF_INTERACT_INPUT_SENSOR) )
			gf_term_string_input(compositor->term , event->character.unicode_char);

		return gf_sc_handle_event_intern(compositor, event, from_user);
	/*switch fullscreen off!!!*/
	case GF_EVENT_SHOWHIDE:
		gf_sc_set_option(compositor, GF_OPT_FULLSCREEN, !compositor->fullscreen);
		break;

	case GF_EVENT_SET_CAPTION:
		compositor->video_out->ProcessEvent(compositor->video_out, event);
		break;

	case GF_EVENT_MOUSEMOVE:
		event->mouse.button = 0;
	case GF_EVENT_MOUSEDOWN:
	case GF_EVENT_MOUSEUP:
	case GF_EVENT_MOUSEWHEEL:
		event->mouse.key_states = compositor->key_states;
		return gf_sc_handle_event_intern(compositor, event, from_user);

	/*when we process events we don't forward them to the user*/
	default:
		return GF_USER_SENDEVENT(compositor->user, event);
	}
	/*if we get here, event has been consumed*/
	return 1;
}

static Bool gf_sc_on_event(void *cbck, GF_Event *event)
{
	return gf_sc_on_event_ex((GF_Compositor *)cbck, event, 0);
}

Bool gf_sc_user_event(GF_Compositor *compositor, GF_Event *event)
{
	switch (event->type) {
	case GF_EVENT_SHOWHIDE:
	case GF_EVENT_MOVE:
	case GF_EVENT_SET_CAPTION:
		compositor->video_out->ProcessEvent(compositor->video_out, event);
		return 0;
	default:
		return gf_sc_on_event_ex(compositor, event, 1);
	}
}

void gf_sc_register_extra_graph(GF_Compositor *compositor, GF_SceneGraph *extra_scene, Bool do_remove)
{
	gf_sc_lock(compositor, 1);
	if (do_remove) gf_list_del_item(compositor->extra_scenes, extra_scene);
	else if (gf_list_find(compositor->extra_scenes, extra_scene)<0) gf_list_add(compositor->extra_scenes, extra_scene);
	gf_sc_lock(compositor, 0);
}


u32 gf_sc_get_audio_delay(GF_Compositor *compositor)
{
	return compositor->audio_renderer ? compositor->audio_renderer->audio_delay : 0;
}


Bool gf_sc_script_action(GF_Compositor *compositor, u32 type, GF_Node *n, GF_JSAPIParam *param)
{
	switch (type) {
	case GF_JSAPI_OP_GET_SCALE: param->val = compositor->zoom; return 1;
	case GF_JSAPI_OP_SET_SCALE: compositor_2d_set_user_transform(compositor, param->val, compositor->trans_x, compositor->trans_y, 0); return 1;
	case GF_JSAPI_OP_GET_ROTATION: param->val = gf_divfix(180*compositor->rotation, GF_PI); return 1;
	case GF_JSAPI_OP_SET_ROTATION: compositor->rotation = gf_mulfix(GF_PI, param->val/180); compositor_2d_set_user_transform(compositor, compositor->zoom, compositor->trans_x, compositor->trans_y, 0); return 1;
	case GF_JSAPI_OP_GET_TRANSLATE: param->pt.x = compositor->trans_x; param->pt.y = compositor->trans_y; return 1;
	case GF_JSAPI_OP_SET_TRANSLATE: compositor_2d_set_user_transform(compositor, compositor->zoom, param->pt.x, param->pt.y, 0); return 1;
	/*FIXME - better SMIL timelines support*/
	case GF_JSAPI_OP_GET_TIME: param->time = gf_node_get_scene_time(n); return 1;
	case GF_JSAPI_OP_SET_TIME: /*seek_to(param->time);*/ return 0;
	/*FIXME - this will not work for inline docs, we will have to store the clipper at the <svg> level*/
	case GF_JSAPI_OP_GET_VIEWPORT: 
#ifndef GPAC_DISABLE_SVG
		if (compositor_svg_get_viewport(n, &param->rc)) return 1;
#endif
		param->rc = gf_rect_center(compositor->traverse_state->vp_size.x, compositor->traverse_state->vp_size.y); 
		if (!compositor->visual->center_coords) {
			param->rc.x = 0;
			param->rc.y = 0;
		}
		return 1;
	case GF_JSAPI_OP_SET_FOCUS: compositor->focus_node = param->node; return 1;
	case GF_JSAPI_OP_GET_FOCUS: param->node = compositor->focus_node; return 1;

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
		tr_state.ignore_strike = 1;
		gf_mx2d_init(tr_state.transform);
		gf_mx2d_init(tr_state.mx_at_node);


		if (type==GF_JSAPI_OP_GET_LOCAL_BBOX) {
			GF_SAFEALLOC(tr_state.svg_props, SVGPropertiesPointers);
			gf_svg_properties_init_pointers(tr_state.svg_props);
			tr_state.abort_bounds_traverse=1;
			gf_node_traverse(n, &tr_state);
			gf_svg_properties_reset_pointers(tr_state.svg_props);
			free(tr_state.svg_props);
		} else {
			gf_node_traverse(gf_sg_get_root_node(compositor->scene), &tr_state);
		}
		if (!tr_state.bounds.height && !tr_state.bounds.width && !tr_state.bounds.x && !tr_state.bounds.height)
			tr_state.abort_bounds_traverse=0;

		gf_mx2d_pre_multiply(&tr_state.mx_at_node, &compositor->traverse_state->transform);

		if (type==GF_JSAPI_OP_GET_LOCAL_BBOX) {
			gf_bbox_from_rect(&param->bbox, &tr_state.bounds);
		} else if (type==GF_JSAPI_OP_GET_TRANSFORM) {
			gf_mx_from_mx2d(&param->mx, &tr_state.mx_at_node);
		} else {
			gf_mx2d_apply_rect(&tr_state.mx_at_node, &tr_state.bounds);
			gf_bbox_from_rect(&param->bbox, &tr_state.bounds);
		}
		if (!tr_state.abort_bounds_traverse) param->bbox.is_set = 0;
	}
		return 1;
	case GF_JSAPI_OP_LOAD_URL:
	{
		GF_Node *target;
		char *sub_url = strrchr(param->uri.url, '#');
		if (!sub_url) return 0;
		target = gf_sg_find_node_by_name(gf_node_get_graph(n), sub_url+1);
		if (target && (gf_node_get_tag(target)==TAG_MPEG4_Viewport) ) {
			((M_Viewport *)target)->set_bind = 1;
			((M_Viewport *)target)->on_set_bind(n);
			return 1;
		}
		return 0;
	}
	case GF_JSAPI_OP_GET_FPS:
		param->time = gf_sc_get_fps(compositor, 0);
		return 1;
	case GF_JSAPI_OP_GET_SPEED:
		param->time = 0;
#ifndef GPAC_DISABLE_3D
		if (compositor->visual->type_3d==2) {
			param->time = FIX2FLT(compositor->visual->camera.speed);
		}
#endif
		return 1;
	case GF_JSAPI_OP_PAUSE_SVG:
		{
			u32 tag = gf_node_get_tag(n);
			switch(tag) {
#ifndef GPAC_DISABLE_SVG
			case TAG_SVG_audio: svg_pause_audio(n, 1); break;
			case TAG_SVG_video: svg_pause_video(n, 1); break;
			case TAG_SVG_animation: svg_pause_animation(n, 1); break;
#endif
			}
		}
		return 1;
	case GF_JSAPI_OP_RESUME_SVG:
		{
			u32 tag = gf_node_get_tag(n);
			switch(tag) {
#ifndef GPAC_DISABLE_SVG
			case TAG_SVG_audio: svg_pause_audio(n, 0); break;
			case TAG_SVG_video: svg_pause_video(n, 0); break;
			case TAG_SVG_animation: svg_pause_animation(n, 0); break;
#endif
			}
		}
		return 1;
	}
	return 0;
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
		if (!compositor_get_2d_plane_intersection(&r, &pos)) return 0;
		if ( (pos.x < clip->x) || (pos.y > clip->y) 
			|| (pos.x > clip->x + clip->width) || (pos.y < clip->y - clip->height) ) return 0;

	} else 
#endif
	{
		GF_Rect rc = *clip;
		GF_Point2D pt;
		gf_mx2d_apply_rect(&tr_state->transform, &rc);
		pt.x = tr_state->ray.orig.x;
		pt.y = tr_state->ray.orig.y;

		if ( (pt.x < rc.x) || (pt.y > rc.y) 
			|| (pt.x > rc.x + rc.width) || (pt.y < rc.y - rc.height) ) return 0;
	}
	return 1;
}


Bool gf_sc_has_text_selection(GF_Compositor *compositor)
{
	return (compositor->store_text_state==GF_SC_TSEL_FROZEN) ? 1 : 0;
}

const char *gf_sc_get_selected_text(GF_Compositor *compositor)
{
	const u16 *srcp;
	u32 len;
	if (compositor->store_text_state != GF_SC_TSEL_FROZEN) return NULL;

	gf_sc_lock(compositor, 1);

	compositor->traverse_state->traversing_mode = TRAVERSE_GET_TEXT;
	if (compositor->sel_buffer) {
		free(compositor->sel_buffer);
		compositor->sel_buffer = NULL;
	}
	compositor->sel_buffer_len = 0;
	compositor->sel_buffer_alloc = 0;
	gf_node_traverse(compositor->text_selection, compositor->traverse_state);
	compositor->traverse_state->traversing_mode = 0;
	compositor->sel_buffer[compositor->sel_buffer_len]=0;
	srcp = compositor->sel_buffer;
	if (compositor->selected_text) free(compositor->selected_text);
	compositor->selected_text = malloc(sizeof(char)*2*compositor->sel_buffer_len);
	len = gf_utf8_wcstombs(compositor->selected_text, 2*compositor->sel_buffer_len, &srcp);
	if ((s32)len<0) len = 0;
	compositor->selected_text[len] = 0;
	gf_sc_lock(compositor, 0);

	return compositor->selected_text;
}


void gf_sc_check_focus_upon_destroy(GF_Node *n)
{
	GF_Compositor *compositor = gf_sc_get_compositor(n);
	if (compositor && (compositor->focus_node==n)) {
		compositor->focus_node = NULL;
		compositor->focus_text_type = 0;
		compositor->focus_uses_dom_events = 0;
		gf_list_reset(compositor->focus_ancestors);
		gf_list_reset(compositor->focus_use_stack);
	}
}

