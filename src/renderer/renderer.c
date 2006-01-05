/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Rendering sub-project
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

#include <gpac/internal/renderer_dev.h>
/*for user and terminal API (options and InputSensor)*/
#include <gpac/terminal.h>
#include <gpac/options.h>

#ifndef GPAC_DISABLE_SVG
#include <gpac/nodes_svg.h>
#endif

/*macro for size event format/send*/
#define GF_USER_SETSIZE(_user, _w, _h)	\
	{	\
		GF_Event evt;	\
		if (_user->EventProc) {	\
			evt.type = GF_EVT_SIZE;	\
			evt.size.width = _w;	\
			evt.size.height = _h;	\
			_user->EventProc(_user->opaque, &evt);	\
		}	\
	}

void gf_sr_simulation_tick(GF_Renderer *sr);


GF_Err gf_sr_set_output_size(GF_Renderer *sr, u32 Width, u32 Height)
{
	GF_Err e;

	gf_sr_lock(sr, 1);
	/*FIXME: need to check for max resolution*/
	sr->width = Width;
	sr->height = Height;
	e = sr->visual_renderer->RecomputeAR(sr->visual_renderer);
	sr->draw_next_frame = 1;
	gf_sr_lock(sr, 0);
	return e;
}

static void gf_sr_set_fullscreen(GF_Renderer *sr)
{
	GF_Err e;
	if (!sr->video_out->SetFullScreen) return;

	/*move to FS*/
	sr->fullscreen = !sr->fullscreen;
	e = sr->video_out->SetFullScreen(sr->video_out, sr->fullscreen, &sr->width, &sr->height);
	if (e) {
		GF_USER_MESSAGE(sr->user, "VideoRenderer", "Cannot switch to fullscreen", e);
		sr->fullscreen = 0;
		e = sr->video_out->SetFullScreen(sr->video_out, 0, &sr->width, &sr->height);
	}
	e = sr->visual_renderer->RecomputeAR(sr->visual_renderer);
	/*force signaling graphics reset*/
	sr->reset_graphics = 1;
}


/*this is needed for:
- audio: since the audio renderer may not be threaded, it must be reconfigured by another thread otherwise 
we lock the audio module
- video: this is typical to OpenGL&co: multithreaded is forbidden, so resizing/fullscreen MUST be done by the same 
thread accessing the HW ressources
*/
static void gf_sr_reconfig_task(GF_Renderer *sr)
{
	u32 width,height;
	
	/*reconfig audio if needed*/
	if (sr->audio_renderer) gf_sr_ar_reconfig(sr->audio_renderer);

	if (sr->msg_type) {
		/*scene size has been overriden*/
		if (sr->msg_type & GF_SR_CFG_OVERRIDE_SIZE) {
			assert(!(sr->override_size_flags & 2));
			sr->override_size_flags |= 2;
			width = sr->scene_width;
			height = sr->scene_height;
			sr->has_size_info = 1;
			gf_sr_set_size(sr, width, height);
			GF_USER_SETSIZE(sr->user, width, height);
		}
		/*size changed from scene cfg: resize window first*/
		if (sr->msg_type & GF_SR_CFG_SET_SIZE) {
			GF_Event evt;
			Bool restore_fs = sr->fullscreen;
			if (restore_fs) gf_sr_set_fullscreen(sr);
			/*send resize event*/
			if (!(sr->msg_type & GF_SR_CFG_WINDOWSIZE_NOTIF) ) {
				evt.type = GF_EVT_SIZE;
				evt.size.width = sr->new_width;
				evt.size.height = sr->new_height;
				sr->video_out->ProcessEvent(sr->video_out, &evt);
			}
			gf_sr_set_output_size(sr, sr->new_width, sr->new_height);
			sr->new_width = sr->new_height = 0;
			if (restore_fs) gf_sr_set_fullscreen(sr);
		}
		/*aspect ratio modif*/
		if (sr->msg_type & GF_SR_CFG_AR) {
			sr->visual_renderer->RecomputeAR(sr->visual_renderer);
		}
		/*fullscreen on/off request*/
		if (sr->msg_type & GF_SR_CFG_FULLSCREEN) {
			gf_sr_set_fullscreen(sr);
			sr->draw_next_frame = 1;
		}
		sr->msg_type = 0;
	}

	/*3D driver changed message, recheck extensions*/
	if (sr->reset_graphics) sr->visual_renderer->GraphicsReset(sr->visual_renderer);
}

GF_Err gf_sr_render_frame(GF_Renderer *sr)
{	

	gf_sr_reconfig_task(sr);
	/*render*/
	gf_sr_simulation_tick(sr);
	return GF_OK;
}

u32 SR_RenderRun(void *par)
{	
	GF_Renderer *sr = (GF_Renderer *) par;
	sr->video_th_state = 1;

	while (sr->video_th_state == 1) {
		/*sleep or render*/
		if (sr->is_hidden) 
			gf_sleep(sr->frame_duration);
		else
			gf_sr_simulation_tick(sr);
	}

	/*destroy video out*/
	sr->video_out->Shutdown(sr->video_out);
	gf_modules_close_interface((GF_BaseInterface *)sr->video_out);
	sr->video_out = NULL;

	sr->video_th_state = 3;
	return 0;
}

/*forces graphics redraw*/
void gf_sr_reset_graphics(GF_Renderer *sr)
{	
	if (sr) {
		gf_sr_lock(sr, 1);
		sr->reset_graphics = 1;
		gf_sr_lock(sr, 0);
	}
}

void SR_ResetFrameRate(GF_Renderer *sr)
{
	u32 i;
	for (i=0; i<GF_SR_FPS_COMPUTE_SIZE; i++) sr->frame_time[i] = 0;
	sr->current_frame = 0;
}

void SR_SetFontEngine(GF_Renderer *sr);
static void gf_sr_on_event(void *cbck, GF_Event *event);

static Bool check_graphics2D_driver(GF_Raster2D *ifce)
{
	/*check base*/
	if (!ifce->stencil_new || !ifce->surface_new) return 0;
	/*if these are not set we cannot draw*/
	if (!ifce->surface_clear || !ifce->surface_set_path || !ifce->surface_fill) return 0;
	/*check we can init a surface with the current driver (the rest is optional)*/
	if (ifce->surface_attach_to_buffer) return 1;
	return 0;
}

static GF_Renderer *SR_New(GF_User *user)
{
	const char *sOpt;
	GF_VisualRenderer *vrend;
	GF_GLConfig cfg, *gl_cfg;
	GF_Renderer *tmp = malloc(sizeof(GF_Renderer));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_Renderer));
	tmp->user = user;

	/*load renderer to check for GL flag*/
	sOpt = gf_cfg_get_key(user->config, "Rendering", "RendererName");
	if (sOpt) {
		tmp->visual_renderer = (GF_VisualRenderer *) gf_modules_load_interface_by_name(user->modules, sOpt, GF_RENDERER_INTERFACE);
		if (!tmp->visual_renderer) sOpt = NULL;
	}
	if (!tmp->visual_renderer) {
		u32 i, count;
		count = gf_modules_get_count(user->modules);
		for (i=0; i<count; i++) {
			tmp->visual_renderer = (GF_VisualRenderer *) gf_modules_load_interface(user->modules, i, GF_RENDERER_INTERFACE);
			if (tmp->visual_renderer) break;
		}
		if (tmp->visual_renderer) gf_cfg_set_key(user->config, "Rendering", "RendererName", tmp->visual_renderer->module_name);
	}

	if (!tmp->visual_renderer) {
		free(tmp);
		return NULL;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.double_buffered = 1;
	gl_cfg = tmp->visual_renderer->bNeedsGL ? &cfg : NULL;
	vrend = tmp->visual_renderer;
	tmp->visual_renderer = NULL;
	/*load video out*/
	sOpt = gf_cfg_get_key(user->config, "Video", "DriverName");
	if (sOpt) {
		tmp->video_out = (GF_VideoOutput *) gf_modules_load_interface_by_name(user->modules, sOpt, GF_VIDEO_OUTPUT_INTERFACE);
		if (tmp->video_out) {
			tmp->video_out->evt_cbk_hdl = tmp;
			tmp->video_out->on_event = gf_sr_on_event;
			/*init hw*/
			if (tmp->video_out->Setup(tmp->video_out, user->os_window_handler, user->os_display, user->dont_override_window_proc, gl_cfg) != GF_OK) {
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
			tmp->video_out->on_event = gf_sr_on_event;
			/*init hw*/
			if (tmp->video_out->Setup(tmp->video_out, user->os_window_handler, user->os_display, user->dont_override_window_proc, gl_cfg)==GF_OK) {
				gf_cfg_set_key(user->config, "Video", "DriverName", tmp->video_out->module_name);
				break;
			}
			gf_modules_close_interface((GF_BaseInterface *)tmp->video_out);
			tmp->video_out = NULL;
		}
	}
	tmp->visual_renderer = vrend;
	if (!tmp->video_out ) {
		gf_modules_close_interface((GF_BaseInterface *)tmp->visual_renderer);
		free(tmp);
		return NULL;
	}

	/*try to load a raster driver*/
	sOpt = gf_cfg_get_key(user->config, "Rendering", "Raster2D");
	if (sOpt) {
		tmp->r2d = (GF_Raster2D *) gf_modules_load_interface_by_name(user->modules, sOpt, GF_RASTER_2D_INTERFACE);
		if (!tmp->r2d) {
			sOpt = NULL;
		} else if (!check_graphics2D_driver(tmp->r2d)) {
			gf_modules_close_interface((GF_BaseInterface *)tmp->r2d);
			tmp->r2d = NULL;
			sOpt = NULL;
		}
	}
	if (!tmp->r2d) {
		u32 i, count;
		count = gf_modules_get_count(user->modules);
		for (i=0; i<count; i++) {
			tmp->r2d = (GF_Raster2D *) gf_modules_load_interface(user->modules, i, GF_RASTER_2D_INTERFACE);
			if (!tmp->r2d) continue;
			if (check_graphics2D_driver(tmp->r2d)) break;
			gf_modules_close_interface((GF_BaseInterface *)tmp->r2d);
			tmp->r2d = NULL;
		}
		if (tmp->r2d) gf_cfg_set_key(user->config, "Rendering", "Raster2D", tmp->r2d->module_name);
	}

	/*and init*/
	if (tmp->visual_renderer->LoadRenderer(tmp->visual_renderer, tmp) != GF_OK) {
		gf_modules_close_interface((GF_BaseInterface *)tmp->visual_renderer);
		tmp->video_out->Shutdown(tmp->video_out);
		gf_modules_close_interface((GF_BaseInterface *)tmp->video_out);
		free(tmp);
		return NULL;
	}

	tmp->mx = gf_mx_new();
	tmp->textures = gf_list_new();
	tmp->frame_rate = 30.0;	
	tmp->frame_duration = 33;
	tmp->time_nodes = gf_list_new();
	tmp->events = gf_list_new();
	tmp->ev_mx = gf_mx_new();
	
	SR_ResetFrameRate(tmp);	
	/*set font engine if any*/
	SR_SetFontEngine(tmp);
	
	tmp->extra_scenes = gf_list_new();
	tmp->interaction_level = GF_INTERACT_NORMAL | GF_INTERACT_INPUT_SENSOR | GF_INTERACT_NAVIGATION;
	return tmp;
}

GF_Renderer *gf_sr_new(GF_User *user, Bool self_threaded, Bool no_audio, GF_Terminal *term)
{
	GF_Renderer *tmp = SR_New(user);
	if (!tmp) return NULL;
	tmp->term = term;

	/**/
	if (!no_audio) {
		tmp->audio_renderer = gf_sr_ar_load(user);	
		if (!tmp->audio_renderer) GF_USER_MESSAGE(user, "", "NO AUDIO RENDERER", GF_OK);
	}

	gf_mx_p(tmp->mx);

	/*run threaded*/
	if (self_threaded) {
		tmp->VisualThread = gf_th_new();
		gf_th_run(tmp->VisualThread, SR_RenderRun, tmp);
		while (tmp->video_th_state!=1) {
			gf_sleep(10);
			if (tmp->video_th_state==3) {
				gf_mx_v(tmp->mx);
				gf_sr_del(tmp);
				return NULL;
			}
		}
	}

	/*set default size if owning output*/
	if (!tmp->user->os_window_handler) {
		gf_sr_set_size(tmp, 320, 20);
	}

	tmp->svg_animated_attributes = gf_list_new();
	
	gf_mx_v(tmp->mx);

	return tmp;
}


void gf_sr_del(GF_Renderer *sr)
{
	if (!sr) return;


	gf_sr_lock(sr, 1);

	if (sr->VisualThread) {
		sr->video_th_state = 2;
		while (sr->video_th_state!=3) gf_sleep(10);
		gf_th_del(sr->VisualThread);
	}
	if (sr->video_out) {
		sr->video_out->Shutdown(sr->video_out);
		gf_modules_close_interface((GF_BaseInterface *)sr->video_out);
	}
	sr->visual_renderer->UnloadRenderer(sr->visual_renderer);
	gf_modules_close_interface((GF_BaseInterface *)sr->visual_renderer);

	if (sr->audio_renderer) gf_sr_ar_del(sr->audio_renderer);

	gf_mx_p(sr->ev_mx);
	while (gf_list_count(sr->events)) {
		GF_UserEvent *ev = gf_list_get(sr->events, 0);
		gf_list_rem(sr->events, 0);
		free(ev);
	}
	gf_mx_v(sr->ev_mx);
	gf_mx_del(sr->ev_mx);
	gf_list_del(sr->events);

	if (sr->font_engine) {
		sr->font_engine->shutdown_font_engine(sr->font_engine);
		gf_modules_close_interface((GF_BaseInterface *)sr->font_engine);
	}
	gf_list_del(sr->textures);
	gf_list_del(sr->time_nodes);
	gf_list_del(sr->extra_scenes);

	gf_list_del(sr->svg_animated_attributes);

	gf_sr_lock(sr, 0);
	gf_mx_del(sr->mx);
	free(sr);
}

void gf_sr_set_fps(GF_Renderer *sr, Double fps)
{
	if (fps) {
		sr->frame_rate = fps;
		sr->frame_duration = (u32) (1000 / fps);
		SR_ResetFrameRate(sr);	
	}
}

u32 gf_sr_get_audio_buffer_length(GF_Renderer *sr)
{
	if (!sr || !sr->audio_renderer || !sr->audio_renderer->audio_out) return 0;
	return sr->audio_renderer->audio_out->GetTotalBufferTime(sr->audio_renderer->audio_out);
}


static void gf_sr_pause(GF_Renderer *sr, u32 PlayState)
{
	if (!sr || !sr->audio_renderer) return;
	if (!sr->paused && !PlayState) return;
	if (sr->paused && (PlayState==GF_STATE_PAUSED)) return;

	/*step mode*/
	if (PlayState==GF_STATE_STEP_PAUSE) {
		sr->step_mode = 1;
		/*resume for next step*/
		if (sr->paused && sr->term) gf_term_set_option(sr->term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
	} else {
		if (sr->audio_renderer) gf_sr_ar_control(sr->audio_renderer, (sr->paused && (PlayState==0xFF)) ? 2 : sr->paused);
		sr->paused = (PlayState==GF_STATE_PAUSED) ? 1 : 0;
	}
}

u32 gf_sr_get_clock(GF_Renderer *sr)
{
	return gf_sr_ar_get_clock(sr->audio_renderer);
}

static GF_Err SR_SetSceneSize(GF_Renderer *sr, u32 Width, u32 Height)
{
	if (!Width || !Height) {
		sr->has_size_info = 0;
		if (sr->override_size_flags) {
			/*specify a small size to detect biggest bitmap but not 0 in case only audio..*/
			sr->scene_height = 20;
			sr->scene_width = 320;
		} else {
			/*use current res*/
			sr->scene_width = sr->width;
			sr->scene_height = (sr->height==20) ? 240 : sr->height;
		}
	} else {
		sr->has_size_info = 1;
		sr->scene_height = Height;
		sr->scene_width = Width;
	}
	return GF_OK;
}

static Fixed convert_svg_length_to_user(GF_Renderer *sr, SVG_Length *length)
{
	// Assuming the environment is 90dpi
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
		return gf_mulfix(length->value, FLT2FIX(35.43307f));
		break;
	case SVG_NUMBER_MM:
		return gf_mulfix(length->value, FLT2FIX(3.543307f));
	case SVG_NUMBER_IN:
		return length->value * 90;
	case SVG_NUMBER_PT:
		return 5 * length->value / 4;
	case SVG_NUMBER_PC:
		return length->value * 15;
	case SVG_NUMBER_INHERIT:
		break;
	}
	return length->value;
}

GF_Err gf_sr_set_scene(GF_Renderer *sr, GF_SceneGraph *scene_graph)
{
	u32 width, height;
	Bool do_notif;

	if (!sr) return GF_BAD_PARAM;

	gf_sr_lock(sr, 1);

	if (sr->audio_renderer && (sr->scene != scene_graph)) 
		gf_sr_ar_reset(sr->audio_renderer);

	gf_mx_p(sr->ev_mx);
	while (gf_list_count(sr->events)) {
		GF_UserEvent *ev = gf_list_get(sr->events, 0);
		gf_list_rem(sr->events, 0);
		free(ev);
	}
	
	/*reset main surface*/
	sr->visual_renderer->SceneReset(sr->visual_renderer);

	/*set current graph*/
	sr->scene = scene_graph;
	do_notif = 0;
	if (scene_graph) {
		/*get pixel size if any*/
		gf_sg_get_scene_size_info(sr->scene, &width, &height);
		sr->has_size_info = (width && height) ? 1 : 0;
#ifndef GPAC_DISABLE_SVG
		/*hack for SVG where size is set in %*/
		if (!sr->has_size_info) {
			SVGsvgElement *root = (SVGsvgElement *) gf_sg_get_root_node(sr->scene);
			u32 tag = gf_node_get_tag((GF_Node*)root);
			if ((tag>=GF_NODE_RANGE_FIRST_SVG) && (tag<=GF_NODE_RANGE_LAST_SVG)) {
				SVG_Length l;
				sr->has_size_info = 1;
				sr->aspect_ratio = GF_ASPECT_RATIO_FILL_SCREEN;
				l = root->width;
				if (l.type!=SVG_NUMBER_PERCENTAGE) {
					width = FIX2INT(convert_svg_length_to_user(sr, &l) );
				} else {
					width = FIX2INT(root->viewBox.width);
				}
				l = ((SVGsvgElement*)root)->height;
				if (l.type!=SVG_NUMBER_PERCENTAGE) {
					height = FIX2INT(convert_svg_length_to_user(sr, &l) );
				} else {
					height = FIX2INT(root->viewBox.height);
				}
			}
		}
#endif
		/*set scene size only if different, otherwise keep scaling/FS*/
		if ( !width || (sr->scene_width!=width) || !height || (sr->scene_height!=height)) {
			do_notif = sr->has_size_info ;
			SR_SetSceneSize(sr, width, height);

			/*get actual size in pixels*/
			width = sr->scene_width;
			height = sr->scene_height;

			if (!sr->user->os_window_handler) {
				/*only notify user if we are attached to a window*/
				do_notif = 0;
				if (sr->video_out->max_screen_width && (width > sr->video_out->max_screen_width))
					width = sr->video_out->max_screen_width;
				if (sr->video_out->max_screen_height && (height > sr->video_out->max_screen_height))
					height = sr->video_out->max_screen_height;

				gf_sr_set_size(sr,width, height);
			}
		}
	}

	SR_ResetFrameRate(sr);	
	sr->draw_next_frame = 1;
	gf_mx_v(sr->ev_mx);
	gf_sr_lock(sr, 0);
	/*here's a nasty trick: the app may respond to this by calling a gf_sr_set_size from a different
	thread, but in an atomic way (typically happen on Win32 when changing the window size). WE MUST
	NOTIFY THE SIZE CHANGE AFTER RELEASING THE RENDERER MUTEX*/
	if (do_notif) GF_USER_SETSIZE(sr->user, width, height);
	return GF_OK;
}

void gf_sr_lock_audio(GF_Renderer *sr, Bool doLock)
{
	if (sr->audio_renderer) {
		gf_mixer_lock(sr->audio_renderer->mixer, doLock);
	}
}

void gf_sr_lock(GF_Renderer *sr, Bool doLock)
{
	if (doLock)
		gf_mx_p(sr->mx);
	else {
		gf_mx_v(sr->mx);
	}
}

void gf_sr_refresh(GF_Renderer *sr)
{
	if (sr) sr->draw_next_frame = 1;
}

GF_Err gf_sr_set_size(GF_Renderer *sr, u32 NewWidth, u32 NewHeight)
{
	Bool lock_ok;
	if (!NewWidth || !NewHeight) {
		sr->override_size_flags &= ~2;
		return GF_OK;
	}
	/*EXTRA CARE HERE: the caller (user app) is likely a different thread than the renderer one, and depending on window 
	manager we may get called here as a result of a message sent to user but not yet returned */
	lock_ok = gf_mx_try_lock(sr->mx);
	
	sr->new_width = NewWidth;
	sr->new_height = NewHeight;
	sr->msg_type |= GF_SR_CFG_SET_SIZE;
	
	/*if same size only request for video setup */
	if ((sr->width == NewWidth) && (sr->height == NewHeight) ) 
		sr->msg_type |= GF_SR_CFG_WINDOWSIZE_NOTIF;
	
	if (lock_ok) gf_sr_lock(sr, 0);

	/*force video resize*/
	if (!sr->VisualThread) gf_sr_reconfig_task(sr);
	return GF_OK;
}

void SR_ReloadConfig(GF_Renderer *sr)
{
	const char *sOpt, *dr_name;

	/*changing drivers needs exclusive access*/
	gf_sr_lock(sr, 1);
	
	sOpt = gf_cfg_get_key(sr->user->config, "Rendering", "ForceSceneSize");
	if (sOpt && ! stricmp(sOpt, "yes")) {
		sr->override_size_flags = 1;
	} else {
		sr->override_size_flags = 0;
	}

	sOpt = gf_cfg_get_key(sr->user->config, "Rendering", "AntiAlias");
	if (sOpt) {
		if (! stricmp(sOpt, "None")) gf_sr_set_option(sr, GF_OPT_ANTIALIAS, GF_ANTIALIAS_NONE);
		else if (! stricmp(sOpt, "Text")) gf_sr_set_option(sr, GF_OPT_ANTIALIAS, GF_ANTIALIAS_TEXT);
		else gf_sr_set_option(sr, GF_OPT_ANTIALIAS, GF_ANTIALIAS_FULL);
	} else {
		gf_cfg_set_key(sr->user->config, "Rendering", "AntiAlias", "All");
		gf_sr_set_option(sr, GF_OPT_ANTIALIAS, GF_ANTIALIAS_FULL);
	}

	sOpt = gf_cfg_get_key(sr->user->config, "Rendering", "StressMode");
	gf_sr_set_option(sr, GF_OPT_STRESS_MODE, (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0);

	sOpt = gf_cfg_get_key(sr->user->config, "Rendering", "FastRender");
	gf_sr_set_option(sr, GF_OPT_HIGHSPEED, (sOpt && !stricmp(sOpt, "yes") ) ? 1 : 0);

	sOpt = gf_cfg_get_key(sr->user->config, "Rendering", "BoundingVolume");
	if (sOpt) {
		if (! stricmp(sOpt, "Box")) gf_sr_set_option(sr, GF_OPT_DRAW_BOUNDS, GF_BOUNDS_BOX);
		else if (! stricmp(sOpt, "AABB")) gf_sr_set_option(sr, GF_OPT_DRAW_BOUNDS, GF_BOUNDS_AABB);
		else gf_sr_set_option(sr, GF_OPT_DRAW_BOUNDS, GF_BOUNDS_NONE);
	} else {
		gf_cfg_set_key(sr->user->config, "Rendering", "BoundingVolume", "None");
		gf_sr_set_option(sr, GF_OPT_DRAW_BOUNDS, GF_BOUNDS_NONE);
	}

	sOpt = gf_cfg_get_key(sr->user->config, "FontEngine", "DriverName");
	if (sOpt && sr->font_engine) {
		dr_name = sr->font_engine->module_name;
		if (stricmp(dr_name, sOpt)) SR_SetFontEngine(sr);
	}

	sOpt = gf_cfg_get_key(sr->user->config, "FontEngine", "TextureTextMode");
	if (sOpt && !stricmp(sOpt, "Always")) sr->texture_text_mode = GF_TEXTURE_TEXT_ALWAYS;
	else if (sOpt && !stricmp(sOpt, "3D")) sr->texture_text_mode = GF_TEXTURE_TEXT_3D;
	else sr->texture_text_mode = GF_TEXTURE_TEXT_NONE;

	if (sr->audio_renderer) {
		sOpt = gf_cfg_get_key(sr->user->config, "Audio", "NoResync");
		if (sOpt && !stricmp(sOpt, "yes")) sr->audio_renderer->flags |= GF_SR_AUDIO_NO_RESYNC;
		else sr->audio_renderer->flags &= ~GF_SR_AUDIO_NO_RESYNC;

		sOpt = gf_cfg_get_key(sr->user->config, "Audio", "DisableMultiChannel");
		if (sOpt && !stricmp(sOpt, "yes")) sr->audio_renderer->flags |= GF_SR_AUDIO_NO_MULTI_CH;
		else sr->audio_renderer->flags &= ~GF_SR_AUDIO_NO_MULTI_CH;
	}

	sr->draw_next_frame = 1;

	gf_sr_lock(sr, 0);
}

GF_Err gf_sr_set_option(GF_Renderer *sr, u32 type, u32 value)
{
	GF_Err e;
	gf_sr_lock(sr, 1);

	e = GF_OK;
	switch (type) {
	case GF_OPT_PLAY_STATE: gf_sr_pause(sr, value); break;
	case GF_OPT_AUDIO_VOLUME: gf_sr_ar_set_volume(sr->audio_renderer, value); break;
	case GF_OPT_AUDIO_PAN: gf_sr_ar_set_pan(sr->audio_renderer, value); break;
	case GF_OPT_OVERRIDE_SIZE:
		sr->override_size_flags = value ? 1 : 0;
		sr->draw_next_frame = 1;
		break;
	case GF_OPT_STRESS_MODE: sr->stress_mode = value; break;
	case GF_OPT_ANTIALIAS: sr->antiAlias = value; break;
	case GF_OPT_HIGHSPEED: sr->high_speed = value; break;
	case GF_OPT_DRAW_BOUNDS: sr->draw_bvol = value; break;
	case GF_OPT_TEXTURE_TEXT: sr->texture_text_mode = value; break;
	case GF_OPT_ASPECT_RATIO: 
		sr->aspect_ratio = value; 
		sr->msg_type |= GF_SR_CFG_AR;
		break;
	case GF_OPT_INTERACTION_LEVEL: sr->interaction_level = value; break;
	case GF_OPT_FORCE_REDRAW:
		sr->reset_graphics = 1;
		break;
	case GF_OPT_FULLSCREEN:
		if (sr->fullscreen != value) sr->msg_type |= GF_SR_CFG_FULLSCREEN; break;
	case GF_OPT_ORIGINAL_VIEW:
		e = sr->visual_renderer->SetOption(sr->visual_renderer, type, value);
		gf_sr_set_size(sr, sr->scene_width, sr->scene_height);
		break;
	case GF_OPT_VISIBLE:
		sr->is_hidden = !value;
		if (sr->video_out->ProcessEvent) {
			GF_Event evt;
			evt.type = GF_EVT_SHOWHIDE;
			evt.show.show_type = value ? 1 : 0;
			e = sr->video_out->ProcessEvent(sr->video_out, &evt);
		}
		break;
	case GF_OPT_FREEZE_DISPLAY: 
		sr->freeze_display = value;
		break;
	case GF_OPT_RELOAD_CONFIG: 
		SR_ReloadConfig(sr);
	default: e = sr->visual_renderer->SetOption(sr->visual_renderer, type, value);
	}
	sr->draw_next_frame = 1; 
	gf_sr_lock(sr, 0);
	return e;
}

u32 gf_sr_get_option(GF_Renderer *sr, u32 type)
{
	switch (type) {
	case GF_OPT_PLAY_STATE: return sr->paused ? 1 : 0;
	case GF_OPT_OVERRIDE_SIZE: return (sr->override_size_flags & 1) ? 1 : 0;
	case GF_OPT_IS_FINISHED:
	{
		if (sr->interaction_sensors) return 0;
		if (gf_list_count(sr->time_nodes)) return 0;
		return 1;
	}
	case GF_OPT_STRESS_MODE: return sr->stress_mode;
	case GF_OPT_AUDIO_VOLUME: return sr->audio_renderer->volume;
	case GF_OPT_AUDIO_PAN: return sr->audio_renderer->pan;
	case GF_OPT_ANTIALIAS: return sr->antiAlias;
	case GF_OPT_HIGHSPEED: return sr->high_speed;
	case GF_OPT_ASPECT_RATIO: return sr->aspect_ratio;
	case GF_OPT_FULLSCREEN: return sr->fullscreen;
	case GF_OPT_INTERACTION_LEVEL: return sr->interaction_level;
	case GF_OPT_VISIBLE: return !sr->is_hidden;
	case GF_OPT_FREEZE_DISPLAY: return sr->freeze_display;
	case GF_OPT_TEXTURE_TEXT: return sr->texture_text_mode;
	default: return sr->visual_renderer->GetOption(sr->visual_renderer, type);
	}
}

void gf_sr_map_point(GF_Renderer *sr, s32 X, s32 Y, Fixed *bifsX, Fixed *bifsY)
{
	/*coordinates are in user-like OS....*/
	X = X - sr->width/2;
	Y = sr->height/2 - Y;
	*bifsX = INT2FIX(X);
	*bifsY = INT2FIX(Y);
}


void SR_SetFontEngine(GF_Renderer *sr)
{
	const char *sOpt;
	u32 i, count;
	GF_FontRaster *ifce;

	ifce = NULL;
	sOpt = gf_cfg_get_key(sr->user->config, "FontEngine", "DriverName");
	if (sOpt) ifce = (GF_FontRaster *) gf_modules_load_interface_by_name(sr->user->modules, sOpt, GF_FONT_RASTER_INTERFACE);

	if (!ifce) {
		count = gf_modules_get_count(sr->user->modules);
		for (i=0; i<count; i++) {
			ifce = (GF_FontRaster *) gf_modules_load_interface(sr->user->modules, i, GF_FONT_RASTER_INTERFACE);
			if (ifce) {
				gf_cfg_set_key(sr->user->config, "FontEngine", "DriverName", ifce->module_name);
				sOpt = ifce->module_name;
				break;
			}
		}
	}
	if (!ifce) return;

	/*cannot init font engine*/
	if (ifce->init_font_engine(ifce) != GF_OK) {
		gf_modules_close_interface((GF_BaseInterface *)ifce);
		return;
	}


	/*shutdown current*/
	gf_sr_lock(sr, 1);
	if (sr->font_engine) {
		sr->font_engine->shutdown_font_engine(sr->font_engine);
		gf_modules_close_interface((GF_BaseInterface *)sr->font_engine);
	}
	sr->font_engine = ifce;

	/*success*/
	gf_cfg_set_key(sr->user->config, "FontEngine", "DriverName", sOpt);
		
	sr->draw_next_frame = 1;
	gf_sr_lock(sr, 0);
}

GF_Err gf_sr_get_screen_buffer(GF_Renderer *sr, GF_VideoSurface *framebuffer)
{
	GF_Err e;
	if (!sr || !framebuffer) return GF_BAD_PARAM;
	gf_mx_p(sr->mx);
	e = sr->visual_renderer->GetScreenBuffer(sr->visual_renderer, framebuffer);
	if (e != GF_OK) gf_mx_v(sr->mx);
	return e;
}

GF_Err gf_sr_release_screen_buffer(GF_Renderer *sr, GF_VideoSurface *framebuffer)
{
	GF_Err e;
	if (!sr || !framebuffer) return GF_BAD_PARAM;
	e = sr->visual_renderer->ReleaseScreenBuffer(sr->visual_renderer, framebuffer);
	gf_mx_v(sr->mx);
	return e;
}

Double gf_sr_get_fps(GF_Renderer *sr, Bool absoluteFPS)
{
	Double fps;
	u32 ind, num, frames, run_time;

	/*start from last frame and get first frame time*/
	ind = sr->current_frame;
	frames = 0;
	run_time = sr->frame_time[ind];
	for (num=0; num<GF_SR_FPS_COMPUTE_SIZE; num++) {
		if (absoluteFPS) {
			run_time += sr->frame_time[ind];
		} else {
			run_time += MAX(sr->frame_time[ind], sr->frame_duration);
		}
		frames++;
		if (frames==GF_SR_FPS_COMPUTE_SIZE) break;
		if (!ind) {
			ind = GF_SR_FPS_COMPUTE_SIZE;
		} else {
			ind--;
		}
	}
	if (!run_time) return (Double) sr->frame_rate;
	fps = 1000*frames;
	fps /= run_time;
	return fps;
}

void gf_sr_register_time_node(GF_Renderer *sr, GF_TimeNode *tn)
{
	/*may happen with DEF/USE */
	if (tn->is_registered) return;
	if (tn->needs_unregister) return;
	gf_list_add(sr->time_nodes, tn);
	tn->is_registered = 1;
}
void gf_sr_unregister_time_node(GF_Renderer *sr, GF_TimeNode *tn)
{
	gf_list_del_item(sr->time_nodes, tn);
}


static void SR_UserInputIntern(GF_Renderer *sr, GF_Event *event)
{
	GF_UserEvent *ev;

	if (sr->term && (sr->interaction_level & GF_INTERACT_INPUT_SENSOR) && (event->type<=GF_EVT_MOUSEWHEEL))
		gf_term_mouse_input(sr->term, &event->mouse);

	if (!sr->interaction_level || (sr->interaction_level==GF_INTERACT_INPUT_SENSOR) ) {
		GF_USER_SENDEVENT(sr->user, event);
		return;
	}

	switch (event->type) {
	case GF_EVT_MOUSEMOVE:
	{
		u32 i, count;
		gf_mx_p(sr->ev_mx);
		count = gf_list_count(sr->events);
		for (i=0; i<count; i++) {
			ev = gf_list_get(sr->events, i);
			if (ev->event_type == GF_EVT_MOUSEMOVE) {
				ev->mouse =  event->mouse;
				gf_mx_v(sr->ev_mx);
				return;
			}
		}
		gf_mx_v(sr->ev_mx);
	}
	default:
		ev = malloc(sizeof(GF_UserEvent));
		ev->event_type = event->type;
		if (event->type<=GF_EVT_MOUSEWHEEL) {
			ev->mouse = event->mouse;
		} else if (event->type==GF_EVT_CHAR) {
			ev->character = event->character;
		} else {
			ev->key = event->key;
		}
		gf_mx_p(sr->ev_mx);
		gf_list_add(sr->events, ev);
		gf_mx_v(sr->ev_mx);
		break;
	}
}


GF_Node *gf_sr_pick_node(GF_Renderer *sr, s32 X, s32 Y)
{
	return NULL;
}

static u32 last_lclick_time = 0;

static void SR_ForwardUserEvent(GF_Renderer *sr, GF_UserEvent *ev)
{
	GF_Event event;

	event.type = ev->event_type;
	if (ev->event_type<=GF_EVT_MOUSEWHEEL) {
		event.mouse = ev->mouse;
	} else {
		event.key = ev->key;
	}
	GF_USER_SENDEVENT(sr->user, &event);

	if (ev->event_type==GF_EVT_LEFTUP) {
		u32 now;
		/*emulate doubleclick*/
		now = gf_sys_clock();
		if (now - last_lclick_time < 250) {
			event.type = GF_EVT_LDOUBLECLICK;
			event.mouse.key_states = sr->key_states;
			event.mouse.x = ev->mouse.x;
			event.mouse.y = ev->mouse.y;
			GF_USER_SENDEVENT(sr->user, &event);
		}
		last_lclick_time = now;
	}
}

void gf_sr_simulation_tick(GF_Renderer *sr)
{	
	u32 in_time, end_time, i, count;

	/*lock renderer for the whole render cycle*/
	gf_sr_lock(sr, 1);
	/*first thing to do, let the video output handle user event if it is not threaded*/
	sr->video_out->ProcessEvent(sr->video_out, NULL);

	if (sr->freeze_display) {
		gf_sr_lock(sr, 0);
		gf_sleep(sr->frame_duration);
		return;
	}

	gf_sr_reconfig_task(sr);

	if (!sr->scene) {
		sr->visual_renderer->DrawScene(sr->visual_renderer);
		gf_sr_lock(sr, 0);
		gf_sleep(sr->frame_duration);
		return;
	}

	in_time = gf_sys_clock();
	if (sr->reset_graphics) sr->draw_next_frame = 1;

	/*process pending user events*/
	gf_mx_p(sr->ev_mx);
	while (gf_list_count(sr->events)) {
		GF_UserEvent *ev = gf_list_get(sr->events, 0);
		gf_list_rem(sr->events, 0);
		if (!sr->visual_renderer->ExecuteEvent(sr->visual_renderer, ev)) {
			SR_ForwardUserEvent(sr, ev);
		}
		free(ev);
	}
	gf_mx_v(sr->ev_mx);

	/*execute all routes before updating textures, otherwise nodes inside composite texture may never see their
	dirty flag set*/
	gf_sg_activate_routes(sr->scene);

	/*update all textures*/
	count = gf_list_count(sr->textures);
	for (i=0; i<count; i++) {
		GF_TextureHandler *st = gf_list_get(sr->textures, i);
		/*signal graphics reset before updating*/
		if (sr->reset_graphics && st->hwtx) sr->visual_renderer->TextureHWReset(st);
		st->update_texture_fcnt(st);
	}

	/*if invalidated, draw*/
	if (sr->draw_next_frame) {
		sr->draw_next_frame = 0;
		sr->visual_renderer->DrawScene(sr->visual_renderer);
		sr->reset_graphics = 0;

		if (sr->stress_mode) {
			sr->draw_next_frame = 1;
			sr->reset_graphics = 1;
		}
	}

	/*update all timed nodes */
	for (i=0; i<gf_list_count(sr->time_nodes); i++) {
		GF_TimeNode *tn = gf_list_get(sr->time_nodes, i);
		if (!tn->needs_unregister) tn->UpdateTimeNode(tn);
		if (tn->needs_unregister) {
			tn->is_registered = 0;
			tn->needs_unregister = 0;
			gf_list_rem(sr->time_nodes, i);
			i--;
			continue;
		}
	}

	/*release all textures - we must release them to handle a same OD being used by several textures*/
	count = gf_list_count(sr->textures);
	for (i=0; i<count; i++) {
		GF_TextureHandler *st = gf_list_get(sr->textures, i);
		gf_sr_texture_release_stream(st);
	}
	end_time = gf_sys_clock() - in_time;

	gf_list_reset(sr->svg_animated_attributes);

	gf_sr_lock(sr, 0);

	sr->current_frame = (sr->current_frame+1) % GF_SR_FPS_COMPUTE_SIZE;
	sr->frame_time[sr->current_frame] = end_time;

	/*step mode on, pause and return*/
	if (sr->step_mode) {
		sr->step_mode = 0;
		if (sr->term) gf_term_set_option(sr->term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
		return;
	}
	/*not threaded, let the owner decide*/
	if (!sr->VisualThread) return;


	/*compute sleep time till next frame, otherwise we'll kill the CPU*/
	i=1;
	while (i * sr->frame_duration < end_time) i++;
	in_time = i * sr->frame_duration - end_time;
	gf_sleep(in_time);
}

GF_Err gf_sr_get_viewpoint(GF_Renderer *sr, u32 viewpoint_idx, const char **outName, Bool *is_bound)
{
	if (!sr->visual_renderer->GetViewpoint) return GF_NOT_SUPPORTED;
	return sr->visual_renderer->GetViewpoint(sr->visual_renderer, viewpoint_idx, outName, is_bound);
}
GF_Err gf_sr_set_viewpoint(GF_Renderer *sr, u32 viewpoint_idx, const char *viewpoint_name)
{
	if (!sr->visual_renderer->SetViewpoint) return GF_NOT_SUPPORTED;
	return sr->visual_renderer->SetViewpoint(sr->visual_renderer, viewpoint_idx, viewpoint_name);
}

void gf_sr_render_inline(GF_Renderer *sr, GF_Node *inline_root, void *rs)
{
	if (sr->visual_renderer->RenderInline) sr->visual_renderer->RenderInline(sr->visual_renderer, inline_root, rs);
}

static void gf_sr_on_event(void *cbck, GF_Event *event)
{
	u32 s, c, m;
	GF_Renderer *sr = (GF_Renderer *)cbck;
	/*not assigned yet*/
	if (!sr || !sr->visual_renderer) return;

	switch (event->type) {
	case GF_EVT_REFRESH:
		gf_sr_refresh(sr);
		break;
	case GF_EVT_VIDEO_SETUP:
		gf_sr_reset_graphics(sr);
		break;
	case GF_EVT_SIZE:
		/*resize message from plugin - only indicate a resetup of video, no resize*/
		if (!sr->user->os_window_handler) {
			/*EXTRA CARE HERE: the caller (video output) is likely a different thread than the renderer one, and the
			renderer may be locked on the video output (flush or whatever)!!
			*/
			Bool lock_ok = gf_mx_try_lock(sr->mx);
			sr->new_width = event->size.width;
			sr->new_height = event->size.height;
			sr->msg_type |= GF_SR_CFG_SET_SIZE;
			if (lock_ok) gf_sr_lock(sr, 0);
		}
		break;
	case GF_EVT_VKEYDOWN:
		s = c = m = 0;
		switch (event->key.vk_code) {
		case GF_VK_SHIFT: s = 2; sr->key_states |= GF_KM_SHIFT; break;
		case GF_VK_CONTROL: c = 2; sr->key_states |= GF_KM_CTRL; break;
		case GF_VK_MENU: m = 2; sr->key_states |= GF_KM_ALT; break;
		}
		event->key.key_states = sr->key_states;
		/*key sensor*/
		if (sr->term && (sr->interaction_level & GF_INTERACT_INPUT_SENSOR) ) {
			if (event->key.vk_code<=GF_VK_RIGHT) gf_term_keyboard_input(sr->term, 0, 0, event->key.vk_code, 0, s, c, m);
			else gf_term_keyboard_input(sr->term, event->key.virtual_code, 0, 0, 0, s, c, m);
		}		
		SR_UserInputIntern(sr, event);
		break;

	case GF_EVT_VKEYUP:
		s = c = m = 0;
		switch (event->key.vk_code) {
		case GF_VK_SHIFT: s = 1; sr->key_states &= ~GF_KM_SHIFT; break;
		case GF_VK_CONTROL: c = 1; sr->key_states &= ~GF_KM_CTRL; break;
		case GF_VK_MENU: m = 1; sr->key_states &= ~GF_KM_ALT; break;
		}
		event->key.key_states = sr->key_states;

		/*key sensor*/
		if (sr->term && (sr->interaction_level & GF_INTERACT_INPUT_SENSOR) ) {
			if (event->key.vk_code<=GF_VK_RIGHT) gf_term_keyboard_input(sr->term, 0, 0, 0, event->key.vk_code, s, c, m);
			else gf_term_keyboard_input(sr->term, 0, event->key.virtual_code, 0, 0, s, c, m);
		}
		SR_UserInputIntern(sr, event);
		break;

	/*for key sensor*/
	case GF_EVT_KEYDOWN:
		event->key.key_states = sr->key_states;
		if (sr->term && (sr->interaction_level & GF_INTERACT_INPUT_SENSOR) )
			gf_term_keyboard_input(sr->term, event->key.virtual_code, 0, 0, 0, 0, 0, 0);
		SR_UserInputIntern(sr, event);
		break;

	case GF_EVT_KEYUP:
		event->key.key_states = sr->key_states;
		if (sr->term && (sr->interaction_level & GF_INTERACT_INPUT_SENSOR) )
			gf_term_keyboard_input(sr->term, 0, event->key.virtual_code, 0, 0, 0, 0, 0);
		SR_UserInputIntern(sr, event);
		break;

	case GF_EVT_CHAR:
		if (sr->term && (sr->interaction_level & GF_INTERACT_INPUT_SENSOR) )
			gf_term_string_input(sr->term , event->character.unicode_char);
		SR_UserInputIntern(sr, event);
		break;
	/*switch fullscreen off!!!*/
	case GF_EVT_SHOWHIDE:
		gf_sr_set_option(sr, GF_OPT_FULLSCREEN, 0);
		break;
	/*switch fullscreen off!!!*/
	case GF_EVT_SET_CAPTION:
		sr->video_out->ProcessEvent(sr->video_out, event);
		break;

	case GF_EVT_MOUSEMOVE:
	case GF_EVT_LEFTDOWN:
	case GF_EVT_RIGHTDOWN:
	case GF_EVT_RIGHTUP:
	case GF_EVT_MIDDLEDOWN:
	case GF_EVT_MIDDLEUP:
	case GF_EVT_MOUSEWHEEL:
	case GF_EVT_LEFTUP:
		event->mouse.key_states = sr->key_states;
		SR_UserInputIntern(sr, event);
		break;

	/*when we process events we don't forward them to the user*/
	default:
		GF_USER_SENDEVENT(sr->user, event);
		break;
	}
}


void gf_sr_user_event(GF_Renderer *sr, GF_Event *event)
{
	switch (event->type) {
	case GF_EVT_SHOWHIDE:
	case GF_EVT_SET_CAPTION:
		sr->video_out->ProcessEvent(sr->video_out, event);
		break;
	default:
		gf_sr_on_event(sr, event);
	}
}

void gf_sr_register_extra_graph(GF_Renderer *sr, GF_SceneGraph *extra_scene, Bool do_remove)
{
	gf_sr_lock(sr, 1);
	if (do_remove) gf_list_del_item(sr->extra_scenes, extra_scene);
	else if (gf_list_find(sr->extra_scenes, extra_scene)<0) gf_list_add(sr->extra_scenes, extra_scene);
	gf_sr_lock(sr, 0);
}


u32 gf_sr_get_audio_delay(GF_Renderer *sr)
{
	return sr->audio_renderer ? sr->audio_renderer->audio_delay : 0;
}

void *gf_sr_get_visual_renderer(GF_Renderer *sr)
{
	return sr->visual_renderer;
}

