/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Media terminal sub-project
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


#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include <gpac/internal/scenegraph_dev.h>
#include <gpac/term_info.h>
#include <gpac/constants.h>
#include <gpac/options.h>
#include <gpac/network.h>
#include <gpac/xml.h>
#include "../utils/module_wrap.h"

#include "media_control.h"

/*textual command processing*/
#include <gpac/scene_manager.h>

GF_Compositor *gf_sc_from_filter(GF_Filter *filter);


u32 gf_comp_play_from_time(GF_Compositor *compositor, u64 from_time, u32 pause_at_first_frame);

static Bool check_user(GF_User *user)
{
	if (!user->config) return 0;
	if (!user->modules) return 0;
	if (!user->opaque) return 0;
	return 1;
}


Bool gf_term_send_event(GF_Terminal *term, GF_Event *evt)
{
	return gf_fs_send_event(term->fsess, evt);
}


#ifdef FILTER_FIXME
static Bool gf_term_get_user_pass(void *usr_cbk, const char *site_url, char *usr_name, char *password)
{
	GF_Event evt;
	GF_Terminal *term = (GF_Terminal *)usr_cbk;
	evt.type = GF_EVENT_AUTHORIZATION;
	evt.auth.site_url = site_url;
	evt.auth.user = usr_name;
	evt.auth.password = password;
	return gf_term_send_event(term, &evt);
}
#endif

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
			while ((ck = (GF_Clock *)gf_list_enum(ns->Clocks, &j))) {
				ck->init_time += ms_diff;
				ck->media_time_at_init += ms_diff;
				//make sure we don't touch clock while doing resume/pause below
				if (force_resume_pause)
					ck->Paused++;
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
				while ((ck = (GF_Clock *)gf_list_enum(ns->Clocks, &j))) {
					ck->Paused--;
				}
			}
		}

		gf_sc_lock(compositor, 0);

	}

	gf_sc_flush_next_audio(compositor);
	return GF_OK;
}

static void gf_term_set_play_state(GF_Compositor *compositor, u32 PlayState, Bool reset_audio, Bool pause_clocks)
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


static void gf_term_connect_from_time_ex(GF_Compositor *compositor, const char *URL, u64 startTime, u32 pause_at_first_frame, Bool secondary_scene, const char *parent_path)
{
	GF_Scene *scene;
	GF_ObjectManager *odm;
	const char *main_url;
	if (!URL || !strlen(URL)) return;

	if (compositor->root_scene) {
		if (compositor->root_scene->root_od && compositor->root_scene->root_od->scene_ns) {
			main_url = compositor->root_scene->root_od->scene_ns->url;
			if (main_url && !strcmp(main_url, URL)) {
				gf_comp_play_from_time(compositor, 0, pause_at_first_frame);
				return;
			}
		}
		/*disconnect*/
		gf_sc_disconnect(compositor);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Connecting to %s\n", URL));

	assert(!compositor->root_scene);

	/*create a new scene*/
	scene = gf_scene_new(compositor, NULL);
	odm = gf_odm_new();
	scene->root_od = odm;
	odm->subscene = scene;
	//by default all scenes are dynamic, until we get a BIFS attached
	scene->is_dynamic_scene = GF_TRUE;

	odm->media_start_time = startTime;

	// we are not in compositor:process at this point of time since the terminal thread drives the compositor
	compositor->root_scene = scene;

	/*render first visual frame and pause*/
	if (pause_at_first_frame) {
		gf_term_set_play_state(compositor, GF_STATE_STEP_PAUSE, 0, 0);
		scene->first_frame_pause_type = pause_at_first_frame;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] root scene created\n", URL));

	if (!strnicmp(URL, "views://", 8)) {
		gf_scene_generate_views(compositor->root_scene, (char *) URL+8, (char*)parent_path);
		return;
	}

	gf_scene_ns_connect_object(scene, odm, (char *) URL, (char*)parent_path);
}

void gf_term_refresh_cache(GF_Config *cfg)
{
	u32 i, count;
	count = gf_cfg_get_section_count(cfg);
	for (i=0; i<count; i++) {
		const char *opt;
		u32 sec, frac, exp;
		Bool force_delete;
		const char *file;
		const char *name = gf_cfg_get_section_name(cfg, i);
		if (strncmp(name, "@cache=", 7)) continue;

		file = gf_cfg_get_key(cfg, name, "cacheFile");
		opt = gf_cfg_get_key(cfg, name, "expireAfterNTP");
		if (!opt) {
			if (file) gf_delete_file((char*) file);
			gf_cfg_del_section(cfg, name);
			i--;
			count--;
			continue;
		}

		force_delete = 0;
		if (file) {
			FILE *t = gf_fopen(file, "r");
			if (!t) force_delete = 1;
			else gf_fclose(t);
		}
		sscanf(opt, "%u", &exp);
		gf_net_get_ntp(&sec, &frac);
		if (exp && (exp<sec)) force_delete=1;

		if (force_delete) {
			if (file) gf_delete_file((char*) opt);

			gf_cfg_del_section(cfg, name);
			i--;
			count--;
			continue;
		}
	}
}

Bool gf_term_is_type_supported(GF_Terminal *term, const char* mime)
{
	if (mime) {
		/* TODO: handle codecs and params */
		const char *sPlug = gf_cfg_get_key(term->user->config, "MimeTypes", mime);
		if (sPlug) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

GF_EXPORT
GF_Terminal *gf_term_new(GF_User *user)
{
	Bool force_single_thread = GF_FALSE;
	GF_Terminal *tmp;
	GF_Filter *comp_filter;

	if (!check_user(user)) return NULL;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Creating terminal\n"));

	tmp = (GF_Terminal*)gf_malloc(sizeof(GF_Terminal));
	if (!tmp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to allocate GF_Terminal : OUT OF MEMORY ?\n"));
		return NULL;
	}
	memset(tmp, 0, sizeof(GF_Terminal));

	/*just for safety in case not done before*/
	gf_sys_init(GF_MemTrackerNone);

	tmp->user = user;

	if (user->init_flags & GF_TERM_NO_DECODER_THREAD) {
		if (user->init_flags & GF_TERM_NO_VISUAL_THREAD) {
			user->init_flags |= GF_TERM_NO_COMPOSITOR_THREAD;
			user->init_flags &= ~GF_TERM_NO_VISUAL_THREAD;
		}
	}

	/*this is not changeable at runtime*/
	if (user->init_flags & GF_TERM_NO_DECODER_THREAD)
		force_single_thread = GF_TRUE;

	tmp->fsess = gf_fs_new(force_single_thread ? 0 : 3, GF_FS_SCHEDULER_LOCK_FREE, user, GF_FALSE);
	if (!tmp->fsess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to create filter session.\n"));
		gf_free(tmp);
		return NULL;
	}

	comp_filter = gf_fs_load_filter(tmp->fsess, "compositor");
	tmp->compositor = comp_filter ? gf_sc_from_filter(comp_filter) : NULL;
	if (!tmp->compositor) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to load compositor filter.\n"));
		gf_fs_del(tmp->fsess);
		gf_free(tmp);
		return NULL;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] compositor loaded\n"));
	gf_sc_set_fps(tmp->compositor, 30.0);


#ifdef FILTER_FIXME
	tmp->downloader = gf_dm_new(user->config);
	gf_dm_set_auth_callback(tmp->downloader, gf_term_get_user_pass, tmp);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] downloader loaded\n"));


	tmp->unthreaded_extensions = gf_list_new();
	tmp->evt_mx = gf_mx_new("Event Filter");


	if (0 == gf_cfg_get_key_count(user->config, "MimeTypes")) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Terminal] Initializing Mime Types..."));
		/* No mime-types detected, probably the first launch */
		for (i=0; i< gf_modules_get_count(user->modules); i++) {
			GF_BaseInterface *ifce = gf_modules_load_interface(user->modules, i, GF_NET_CLIENT_INTERFACE);
			if (ifce) {
				GF_InputService * service = (GF_InputService*) ifce;
				GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Asking mime types supported for new module %s...\n", ifce->module_name));
				if (service->RegisterMimeTypes) {
					u32 num = service->RegisterMimeTypes(service);
					GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] module %s has registered %u new mime-types.\n", ifce->module_name, num));
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] Module %s has not declared any RegisterMimeTypes method, cannot guess its supported mime-types.\n", ifce->module_name));
				}
				gf_modules_close_interface(ifce);
			}
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Terminal] Finished Initializing Mime Types."));
	}

	tmp->uri_relocators = gf_list_new();
	tmp->locales.relocate_uri = term_check_locales;
	tmp->locales.term = tmp;
	gf_list_add(tmp->uri_relocators, &tmp->locales);

	tmp->speed_ratio = FIX_ONE;

#endif

	gf_filter_post_process_task(comp_filter);
	gf_fs_run(tmp->fsess);

	return tmp;
}

GF_EXPORT
GF_Err gf_term_del(GF_Terminal * term)
{
	if (!term) return GF_BAD_PARAM;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Destroying terminal\n"));
	/*close main service*/
	gf_term_disconnect(term);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] main service disconnected\n"));

	/*stop the media manager */
	gf_fs_del(term->fsess);

	gf_sys_close();
	gf_free(term);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Terminal destroyed\n"));
	return GF_OK;
}

GF_EXPORT
GF_Err gf_term_step_clocks(GF_Terminal * term, u32 ms_diff)
{
	return gf_sc_step_clocks_intern(term->compositor, ms_diff, GF_FALSE);

}
GF_EXPORT
void gf_term_connect_from_time(GF_Terminal * term, const char *URL, u64 startTime, u32 pause_at_first_frame)
{
	if (!term) return;
	gf_term_connect_from_time_ex(term->compositor, URL, startTime, pause_at_first_frame, 0, NULL);
}

GF_EXPORT
void gf_term_connect(GF_Terminal * term, const char *URL)
{
	if (!term) return;
	gf_term_connect_from_time_ex(term->compositor, URL, 0, 0, 0, NULL);
}

void gf_sc_connect(GF_Compositor *compositor, const char *URL)
{
	gf_term_connect_from_time_ex(compositor, URL, 0, 0, 0, NULL);
}

GF_EXPORT
void gf_term_connect_with_path(GF_Terminal * term, const char *URL, const char *parent_path)
{
	if (!term) return;
	gf_term_connect_from_time_ex(term->compositor, URL, 0, 0, 0, parent_path);
}

GF_EXPORT
void gf_sc_disconnect(GF_Compositor *compositor)
{
	/*resume*/
	if (compositor->play_state != GF_STATE_PLAYING) gf_term_set_play_state(compositor, GF_STATE_PLAYING, 1, 1);


	if (compositor->root_scene->root_od) {
		GF_ObjectManager *root = compositor->root_scene->root_od;
		gf_sc_lock(compositor, GF_TRUE);
		compositor->root_scene = NULL;
		gf_odm_disconnect(root, 2);
		gf_sc_lock(compositor, GF_FALSE);
	}
}

GF_EXPORT
void gf_term_disconnect(GF_Terminal *term)
{
	if (term) gf_sc_disconnect(term->compositor);
}

GF_EXPORT
const char *gf_term_get_url(GF_Terminal *term)
{
	GF_Compositor *compositor = term ? term->compositor : NULL;
	if (!compositor || !compositor->root_scene || !compositor->root_scene->root_od || !compositor->root_scene->root_od->scene_ns) return NULL;
	return compositor->root_scene->root_od->scene_ns->url;
}

/*set rendering option*/
GF_EXPORT
GF_Err gf_term_set_option(GF_Terminal * term, u32 type, u32 value)
{
	if (!term) return GF_BAD_PARAM;
	switch (type) {
	case GF_OPT_PLAY_STATE:
		gf_term_set_play_state(term->compositor, value, 0, 1);
		return GF_OK;
#ifdef FILTER_FIXME
	case GF_OPT_HTTP_MAX_RATE:
		gf_dm_set_data_rate(term->downloader, value);
		return GF_OK;
#endif
	default:
		return gf_sc_set_option(term->compositor, type, value);
	}
}

GF_EXPORT
GF_Err gf_term_set_simulation_frame_rate(GF_Terminal * term, Double frame_rate)
{
	if (!term) return GF_BAD_PARAM;
	gf_sc_set_fps(term->compositor, frame_rate);
	return GF_OK;
}

GF_EXPORT
Double gf_term_get_simulation_frame_rate(GF_Terminal *term, u32 *nb_frames_drawn)
{
	if (!term) return 0.0;
	if (nb_frames_drawn) *nb_frames_drawn = term->compositor->frame_number;
	return term->compositor->frame_rate;
}



/*get rendering option*/
static
u32 gf_comp_get_option(GF_Compositor *compositor, u32 type)
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
#if FILTER_FIXME
		return gf_dm_get_data_rate(compositor->downloader);
#else
		return 0;
#endif
	case GF_OPT_VIDEO_BENCH:
		return compositor->bench_mode ? GF_TRUE : GF_FALSE;
	case GF_OPT_ORIENTATION_SENSORS_ACTIVE:
		return compositor->orientation_sensors_active;
	default:
		return gf_sc_get_option(compositor, type);
	}
}

/*get rendering option*/
GF_EXPORT
u32 gf_term_get_option(GF_Terminal * term, u32 type)
{
	return term ? gf_comp_get_option(term->compositor, type) : 0;
}

GF_EXPORT
GF_Err gf_term_set_size(GF_Terminal * term, u32 NewWidth, u32 NewHeight)
{
	if (!term) return GF_BAD_PARAM;
	return gf_sc_set_size(term->compositor, NewWidth, NewHeight);
}

typedef struct
{
	GF_ObjectManager *odm;
	char *service_url, *parent_url;
} GF_TermConnectObject;


#ifdef FILTER_FIXME

/* Browses all registered relocators (ZIP-based, ISOFF-based or file-system-based to relocate a URI based on the locale */
GF_EXPORT
Bool gf_term_relocate_url(GF_Terminal *term, const char *service_url, const char *parent_url, char *out_relocated_url, char *out_localized_url)
{
	u32 i, count;

	count = gf_list_count(term->uri_relocators);
	for (i=0; i<count; i++) {
		Bool result;
		GF_URIRelocator *uri_relocator = gf_list_get(term->uri_relocators, i);
		result = uri_relocator->relocate_uri(uri_relocator, parent_url, service_url, out_relocated_url, out_localized_url);
		if (result) return 1;
	}
	return 0;
}
#endif

GF_EXPORT
u32 gf_comp_play_from_time(GF_Compositor *compositor, u64 from_time, u32 pause_at_first_frame)
{
	if (!compositor || !compositor->root_scene || !compositor->root_scene->root_od) return 0;
	if (compositor->root_scene->root_od->flags & GF_ODM_NO_TIME_CTRL) return 1;

	if (pause_at_first_frame==2) {
		if (gf_comp_get_option(compositor, GF_OPT_PLAY_STATE) != GF_STATE_PLAYING)
			pause_at_first_frame = 1;
		else
			pause_at_first_frame = 0;
	}

	/*for dynamic scene OD ressources are static and all object use the same clock, so don't restart the root
	OD, just act as a mediaControl on all playing streams*/
	if (compositor->root_scene->is_dynamic_scene) {

		/*exit pause mode*/
		gf_term_set_play_state(compositor, GF_STATE_PLAYING, 1, 1);

		if (pause_at_first_frame)
			gf_term_set_play_state(compositor, GF_STATE_STEP_PAUSE, 0, 0);

		gf_sc_lock(compositor, 1);
		gf_scene_restart_dynamic(compositor->root_scene, from_time, 0, 0);
		gf_sc_lock(compositor, 0);
		return 2;
	}

	/*pause everything*/
	gf_term_set_play_state(compositor, GF_STATE_PAUSED, 0, 1);
	/*stop root*/
	gf_odm_stop(compositor->root_scene->root_od, 1);
	gf_scene_disconnect(compositor->root_scene, 0);

	compositor->root_scene->root_od->media_start_time = from_time;

	gf_odm_start(compositor->root_scene->root_od);
	gf_term_set_play_state(compositor, GF_STATE_PLAYING, 0, 1);
	if (pause_at_first_frame)
		gf_sc_set_option(compositor, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
	return 2;
}

GF_EXPORT
u32 gf_term_play_from_time(GF_Terminal *term, u64 from_time, u32 pause_at_first_frame)
{
	return term ? gf_comp_play_from_time(term->compositor, from_time, pause_at_first_frame) : 0;
}

GF_EXPORT
Bool gf_term_user_event(GF_Terminal * term, GF_Event *evt)
{
	if (term) return gf_sc_user_event(term->compositor, evt);
	return GF_FALSE;
}


GF_EXPORT
Double gf_term_get_framerate(GF_Terminal *term, Bool absoluteFPS)
{
	if (!term || !term->compositor) return 0;
	return gf_sc_get_fps(term->compositor, absoluteFPS);
}

/*get main scene current time in sec*/
GF_EXPORT
u32 gf_term_get_time_in_ms(GF_Terminal *term)
{
	GF_Clock *ck;
	GF_Compositor *compositor = term ? term->compositor : NULL;
	if (!term || !compositor->root_scene) return 0;
	ck = compositor->root_scene->root_od->ck;
	if (!ck) return 0;
	return gf_clock_media_time(ck);
}

GF_EXPORT
u32 gf_term_get_elapsed_time_in_ms(GF_Terminal *term)
{
	GF_Clock *ck;
	GF_Compositor *compositor = term ? term->compositor : NULL;
	if (!term || !compositor->root_scene) return 0;
	ck = compositor->root_scene->root_od->ck;
	if (!ck) return 0;

	return gf_clock_elapsed_time(ck);
}

GF_Node *gf_term_pick_node(GF_Terminal *term, s32 X, s32 Y)
{
	if (!term || !term->compositor) return NULL;
	return gf_sc_pick_node(term->compositor, X, Y);
}

GF_EXPORT
void gf_term_navigate_to(GF_Terminal *term, const char *toURL)
{
	GF_Compositor *compositor = term ? term->compositor : NULL;
	if (!term) return;
	if (!toURL && !term->compositor->root_scene) return;
#ifdef FILTER_FIXME
	if (term->reload_url) gf_free(term->reload_url);
	term->reload_url = NULL;

	if (toURL) {
		if (compositor->root_scene && compositor->root_scene->root_od && compositor->root_scene->root_od->scene_ns)
			term->reload_url = gf_url_concatenate(compositor->root_scene->root_od->scene_ns->url, toURL);
		if (!term->reload_url) term->reload_url = gf_strdup(toURL);
	}
	term->reload_state = 1;
#endif
}

GF_EXPORT
GF_Err gf_term_get_viewpoint(GF_Terminal *term, u32 viewpoint_idx, const char **outName, Bool *is_bound)
{
	return gf_sc_get_viewpoint(term->compositor, viewpoint_idx, outName, is_bound);
}

GF_EXPORT
GF_Err gf_term_set_viewpoint(GF_Terminal *term, u32 viewpoint_idx, const char *viewpoint_name)
{
	return gf_sc_set_viewpoint(term->compositor, viewpoint_idx, viewpoint_name);
}

GF_EXPORT
GF_Err gf_term_add_object(GF_Terminal *term, const char *url, Bool auto_play)
{
	GF_MediaObject *mo=NULL;
	//this needs refinement
#ifdef FILTER_FIXME
	SFURL sfurl;
	MFURL mfurl;
	if (!url || !term || !term->root_scene || !term->root_scene->is_dynamic_scene) return GF_BAD_PARAM;

	sfurl.OD_ID = GF_MEDIA_EXTERNAL_ID;
	sfurl.url = (char *) url;
	mfurl.count = 1;
	mfurl.vals = &sfurl;
	/*only text tracks are supported for now...*/
	mo = gf_scene_get_media_object(term->root_scene, &mfurl, GF_MEDIA_OBJECT_TEXT, 1);
	if (mo) {
		/*check if we must deactivate it*/
		if (mo->odm) {
			if (mo->num_open && !auto_play) {
				gf_scene_select_object(term->root_scene, mo->odm);
			} else {
				mo->odm->OD_PL = auto_play ? 1 : 0;
			}
		} else {
			gf_list_del_item(term->root_scene->scene_objects, mo);
			gf_sg_vrml_mf_reset(&mo->URLs, GF_SG_VRML_MFURL);
			gf_free(mo);
			mo = NULL;
		}
	}
#endif
	return mo ? GF_OK : GF_NOT_SUPPORTED;
}


GF_EXPORT
GF_Err gf_term_scene_update(GF_Terminal *term, char *type, char *com)
{
	GF_Compositor *compositor = term ? term->compositor : NULL;
#ifndef GPAC_DISABLE_SMGR
	GF_Err e;
	GF_StreamContext *sc;
	GF_ESD *esd;
	Bool is_xml = 0;
	Double time = 0;
	u32 i, tag;
	GF_SceneLoader load;

	if (!term || !com) return GF_BAD_PARAM;

	if (type && (!stricmp(type, "application/ecmascript") || !stricmp(type, "js")) )  {
		return gf_scene_execute_script(compositor->root_scene->graph, com);
	}

	if (!type && com && !strncmp(com, "gpac ", 5)) {
#ifdef FILTER_FIXME
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
		//new add-on
		if (compositor->root_scene && !strncmp(com, "select ", 7)) {
			u32 idx = atoi(com+7);
			gf_term_select_object(term, gf_list_get(term->root_scene->resources, idx));
			return GF_OK;
		}
#endif
		return GF_OK;
	}

	memset(&load, 0, sizeof(GF_SceneLoader));
	load.localPath = gf_cfg_get_key(term->user->config, "General", "CacheDirectory");
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
		load.ctx = gf_sm_new(compositor->root_scene->graph);
#ifdef FILTER_FIXME
		/*restore streams*/
		i=0;
		while ((esd = (GF_ESD*)gf_list_enum(compositor->root_scene->root_od->OD->ESDescriptors, &i)) ) {
			switch (esd->decoderConfig->streamType) {
			case GF_STREAM_OD:
			case GF_STREAM_SCENE:
			case GF_STREAM_PRIVATE_SCENE:
				sc = gf_sm_stream_new(load.ctx, esd->ESID, esd->decoderConfig->streamType, esd->decoderConfig->objectTypeIndication);
				if (esd->decoderConfig->streamType==GF_STREAM_PRIVATE_SCENE) sc->streamType = GF_STREAM_SCENE;
				sc->timeScale = esd->slConfig->timestampResolution;
				break;
			}
		}
#endif
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
GF_Err gf_term_get_screen_buffer(GF_Terminal *term, GF_VideoSurface *framebuffer)
{
	if (!term) return GF_BAD_PARAM;
	return gf_sc_get_screen_buffer(term->compositor, framebuffer, 0);
}

GF_EXPORT
GF_Err gf_term_get_offscreen_buffer(GF_Terminal *term, GF_VideoSurface *framebuffer, u32 view_idx, u32 depth_buffer_type)
{
	if (!term) return GF_BAD_PARAM;
	return gf_sc_get_offscreen_buffer(term->compositor, framebuffer, view_idx, depth_buffer_type);
}

GF_EXPORT
GF_Err gf_term_release_screen_buffer(GF_Terminal *term, GF_VideoSurface *framebuffer)
{
	if (!term) return GF_BAD_PARAM;
	return gf_sc_release_screen_buffer(term->compositor, framebuffer);
}

GF_EXPORT
const char *gf_term_get_text_selection(GF_Terminal *term, Bool probe_only)
{
	Bool has_text;
	if (!term) return NULL;
	has_text = gf_sc_has_text_selection(term->compositor);
	if (!has_text) return NULL;
	if (probe_only) return "";
	return gf_sc_get_selected_text(term->compositor);
}


GF_EXPORT
GF_Err gf_term_paste_text(GF_Terminal *term, const char *txt, Bool probe_only)
{
	if (!term) return GF_BAD_PARAM;
	if (probe_only) return gf_sc_paste_text(term->compositor, NULL);
	return gf_sc_paste_text(term->compositor, txt);
}


enum
{
	GF_ACTION_PLAY,
	GF_ACTION_STOP,
	GF_ACTION_STEP,
	GF_ACTION_EXIT,
	GF_ACTION_MUTE,
	GF_ACTION_VOLUP,
	GF_ACTION_VOLDOWN,
	GF_ACTION_JUMP_FORWARD,
	GF_ACTION_JUMP_BACKWARD,
	GF_ACTION_JUMP_START,
	GF_ACTION_JUMP_END,
	GF_ACTION_VERY_FAST_FORWARD,
	GF_ACTION_FAST_FORWARD,
	GF_ACTION_SLOW_FORWARD,
	GF_ACTION_VERY_FAST_REWIND,
	GF_ACTION_FAST_REWIND,
	GF_ACTION_SLOW_REWIND,
	GF_ACTION_NEXT,
	GF_ACTION_PREVIOUS,
	GF_ACTION_QUALITY_UP,
	GF_ACTION_QUALITY_DOWN,
};

static void set_clocks_speed(GF_Compositor *compositor, Fixed ratio)
{
	u32 i, j;
	GF_SceneNamespace *ns;

	/*pause all clocks on all services*/
	i=0;
	while ( (ns = (GF_SceneNamespace*)gf_list_enum(compositor->root_scene->namespaces, &i)) ) {
		GF_Clock *ck;
		j=0;
		while ( (ck = (GF_Clock *)gf_list_enum(ns->Clocks, &j)) ) {
			Fixed s = gf_mulfix(ck->speed, ratio);
			gf_clock_set_speed(ck, s);
		}
	}
}

GF_EXPORT
GF_Err gf_term_set_speed(GF_Terminal *term, Fixed speed)
{
	Double fps;
	u32 i, j;
	const char *opt;
	GF_SceneNamespace *ns;
	Bool restart = 0;
	u32 scene_time = gf_term_get_time_in_ms(term);

	if (!term || !speed) return GF_BAD_PARAM;

	if (speed<0) {
		i=0;
		while ( (ns = (GF_SceneNamespace*)gf_list_enum(term->compositor->root_scene->namespaces, &i)) ) {
#ifdef FILTER_FIXME
			GF_NetworkCommand com;
			GF_Err e;
			memset(&com, 0, sizeof(GF_NetworkCommand));
			com.base.command_type = GF_NET_SERVICE_CAN_REVERSE_PLAYBACK;
			e = gf_term_service_command(ns, &com);
			if (e != GF_OK) {
				return e;
			}
#endif
		}
	}

	/*adjust all clocks on all services, if possible*/
	i=0;
	while ( (ns = (GF_SceneNamespace*)gf_list_enum(term->compositor->root_scene->namespaces, &i)) ) {
		GF_Clock *ck;
		ns->set_speed = speed;
		j=0;
		while ( (ck = (GF_Clock *)gf_list_enum(ns->Clocks, &j)) ) {
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
		if (term->compositor->root_scene->is_dynamic_scene) {
			gf_scene_restart_dynamic(term->compositor->root_scene, scene_time, 0, 0);
		}
	}

	if (speed<0)
		speed = -speed;

	opt = gf_cfg_get_key(term->user->config, "Compositor", "FrameRate");
	fps = atoi(opt);
	fps *= FIX2FLT(speed);
	if (fps>100) fps = 1000;
	gf_sc_set_fps(term->compositor, fps);

	return GF_OK;
}

#ifdef FILTER_FIXME
GF_EXPORT
void gf_term_process_shortcut(GF_Terminal *term, GF_Event *ev)
{
	GF_Event evt;
	if (ev->type==GF_EVENT_KEYDOWN) {
		u32 i;
		u8 mod = 0;
		if (ev->key.flags & GF_KEY_MOD_CTRL) mod |= GF_KEY_MOD_CTRL;
		if (ev->key.flags & GF_KEY_MOD_ALT) mod |= GF_KEY_MOD_ALT;

		for (i=0; i<MAX_SHORTCUTS; i++) {
			u32 val;
			if (!term->shortcuts[i].code) break;
			if (term->shortcuts[i].mods!=mod) continue;
			if (term->shortcuts[i].code!=ev->key.key_code) continue;

			switch (term->shortcuts[i].action) {
			case GF_ACTION_PLAY:
				if (gf_term_get_option(term, GF_OPT_PLAY_STATE) == GF_STATE_PAUSED) {
					gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PLAYING);
				} else if (term->speed_ratio != FIX_ONE) {
					set_clocks_speed(term, gf_divfix(1, term->speed_ratio) );
					term->speed_ratio = FIX_ONE;
				} else {
					gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_PAUSED);
				}
				break;
			case GF_ACTION_STOP:
				gf_term_play_from_time(term, 0, 1);
				break;
			case GF_ACTION_NEXT:
				evt.type = GF_EVENT_KEYDOWN;
				evt.key.key_code = GF_KEY_MEDIANEXTTRACK;
				gf_term_send_event(term, &evt);
				break;
			case GF_ACTION_PREVIOUS:
				evt.type = GF_EVENT_KEYDOWN;
				evt.key.key_code = GF_KEY_MEDIAPREVIOUSTRACK;
				gf_term_send_event(term, &evt);
				break;

			case GF_ACTION_STEP:
				gf_term_set_option(term, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
				break;
			case GF_ACTION_EXIT:
				memset(&evt, 0, sizeof(GF_Event));
				evt.type = GF_EVENT_QUIT;
				gf_term_send_event(term, &evt);
				break;
			case GF_ACTION_MUTE:
				gf_term_set_option(term, GF_OPT_AUDIO_MUTE, gf_term_get_option(term, GF_OPT_AUDIO_MUTE) ? 0 : 1);
				break;
			case GF_ACTION_VOLUP:
				val = gf_term_get_option(term, GF_OPT_AUDIO_VOLUME);
				if (val<95) val += 5;
				else val = 100;
				gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, val);
				break;
			case GF_ACTION_VOLDOWN:
				val = gf_term_get_option(term, GF_OPT_AUDIO_VOLUME);
				if (val>5) val -= 5;
				else val = 0;
				gf_term_set_option(term, GF_OPT_AUDIO_VOLUME, val);
				break;
			case GF_ACTION_JUMP_FORWARD:
			case GF_ACTION_JUMP_BACKWARD:
			case GF_ACTION_VERY_FAST_REWIND:
			case GF_ACTION_FAST_REWIND:
			case GF_ACTION_SLOW_REWIND:
				if (0 && term->root_scene && !(term->root_scene->root_od->flags & GF_ODM_NO_TIME_CTRL) ) {
					s32 res;
					u32 dur = (u32) term->root_scene->duration ;
					val  = gf_term_get_time_in_ms(term);
					res = val;
					switch (term->shortcuts[i].action) {
					case GF_ACTION_JUMP_BACKWARD:
					case GF_ACTION_FAST_REWIND:
						res -= (s32) (5*dur/100);
						if (res<0) res = 0;
						break;
					case GF_ACTION_VERY_FAST_REWIND:
						res -= (s32) (10*dur/100);
						if (res<0) res = 0;
						break;
					case GF_ACTION_SLOW_REWIND:
						res -= (s32) (dur/100);
						if (res<0) res = 0;
						break;
					default:
						res += (s32) (5*dur/100);
						if (res > (s32)dur) res = dur;
						break;
					}
					gf_term_play_from_time(term, res, 2);
				}
				break;
			case GF_ACTION_JUMP_START:
				if (term->root_scene && !(term->root_scene->root_od->flags & GF_ODM_NO_TIME_CTRL) ) {
					gf_term_play_from_time(term, 0, 2);
				}
				break;
			case GF_ACTION_JUMP_END:
				if (term->root_scene && !(term->root_scene->root_od->flags & GF_ODM_NO_TIME_CTRL) ) {
					gf_term_play_from_time(term, term->root_scene->duration, 2);
				}
				break;
			case GF_ACTION_VERY_FAST_FORWARD:
			case GF_ACTION_FAST_FORWARD:
			case GF_ACTION_SLOW_FORWARD:
				if (term->speed_ratio != FIX_ONE) {
					set_clocks_speed(term, gf_divfix(1, term->speed_ratio) );
					term->speed_ratio = FIX_ONE;
				}
				else {
					switch (term->shortcuts[i].action) {
					case GF_ACTION_VERY_FAST_FORWARD:
						term->speed_ratio = INT2FIX(4);
						break;
					case GF_ACTION_FAST_FORWARD:
						term->speed_ratio = INT2FIX(2);
						break;
					case GF_ACTION_SLOW_FORWARD:
						term->speed_ratio = INT2FIX(1)/4;
						break;
					}
					set_clocks_speed(term, term->speed_ratio);
				}
				break;
			case GF_ACTION_QUALITY_UP:
				gf_term_switch_quality(term, 1);
				break;
			case GF_ACTION_QUALITY_DOWN:
				gf_term_switch_quality(term, 0);
				break;
			}
			break;
		}
	}
}

void gf_term_load_shortcuts(GF_Terminal *term)
{
	char szVal[51];
	u32 i, k, count;

	memset(term->shortcuts, 0, sizeof(GF_Shortcut)*MAX_SHORTCUTS);
	count = gf_cfg_get_key_count(term->user->config, "Shortcuts");
	k = 0;
	for (i=0; i<count; i++) {
		char *name = (char*)gf_cfg_get_key_name(term->user->config, "Shortcuts", i);
		char *val = (char*)gf_cfg_get_key(term->user->config, "Shortcuts", name);
		if (!name || !val) continue;

		strncpy(szVal, val, 50);
		strlwr(szVal);
		val = szVal;

		while (strchr(val, '+')) {
			if (!strnicmp(val, "ctrl+", 5)) {
				val += 5;
				term->shortcuts[k].mods |= GF_KEY_MOD_CTRL;
			}
			if (!strnicmp(val, "alt+", 4)) {
				val += 4;
				term->shortcuts[k].mods |= GF_KEY_MOD_ALT;
			}
		}
#ifndef GPAC_DISABLE_SVG
		term->shortcuts[k].code = gf_dom_get_key_type((char *)val);
#endif
		if (!term->shortcuts[k].code) continue;

		if (!stricmp(name, "Play") || !stricmp(name, "Pause")) term->shortcuts[k].action = GF_ACTION_PLAY;
		else if (!stricmp(name, "Stop")) term->shortcuts[k].action = GF_ACTION_STOP;
		else if (!stricmp(name, "Step")) term->shortcuts[k].action = GF_ACTION_STEP;
		else if (!stricmp(name, "Exit")) term->shortcuts[k].action = GF_ACTION_EXIT;
		else if (!stricmp(name, "Mute")) term->shortcuts[k].action = GF_ACTION_MUTE;
		else if (!stricmp(name, "VolumeUp")) term->shortcuts[k].action = GF_ACTION_VOLUP;
		else if (!stricmp(name, "VolumeDown")) term->shortcuts[k].action = GF_ACTION_VOLDOWN;
		else if (!stricmp(name, "JumpForward")) term->shortcuts[k].action = GF_ACTION_JUMP_FORWARD;
		else if (!stricmp(name, "JumpBackward")) term->shortcuts[k].action = GF_ACTION_JUMP_BACKWARD;
		else if (!stricmp(name, "JumpStart")) term->shortcuts[k].action = GF_ACTION_JUMP_START;
		else if (!stricmp(name, "JumpEnd")) term->shortcuts[k].action = GF_ACTION_JUMP_END;
		else if (!stricmp(name, "VeryFastForward")) term->shortcuts[k].action = GF_ACTION_VERY_FAST_FORWARD;
		else if (!stricmp(name, "FastForward")) term->shortcuts[k].action = GF_ACTION_FAST_FORWARD;
		else if (!stricmp(name, "SlowForward")) term->shortcuts[k].action = GF_ACTION_SLOW_FORWARD;
		else if (!stricmp(name, "VeryFastRewind")) term->shortcuts[k].action = GF_ACTION_VERY_FAST_REWIND;
		else if (!stricmp(name, "FastRewind")) term->shortcuts[k].action = GF_ACTION_FAST_REWIND;
		else if (!stricmp(name, "SlowRewind")) term->shortcuts[k].action = GF_ACTION_SLOW_REWIND;
		else if (!stricmp(name, "Next")) term->shortcuts[k].action = GF_ACTION_NEXT;
		else if (!stricmp(name, "Previous")) term->shortcuts[k].action = GF_ACTION_PREVIOUS;
		else if (!stricmp(name, "QualityUp")) term->shortcuts[k].action = GF_ACTION_QUALITY_UP;
		else if (!stricmp(name, "QualityDown")) term->shortcuts[k].action = GF_ACTION_QUALITY_DOWN;
		else {
			term->shortcuts[k].mods = 0;
			term->shortcuts[k].code = 0;
			continue;
		}
		k++;
		if (k==MAX_SHORTCUTS) break;
	}
}
#endif


GF_EXPORT
void gf_term_switch_quality(GF_Terminal *term, Bool up)
{
	if (term)
		gf_scene_switch_quality(term->compositor->root_scene, up);
}

GF_EXPORT
GF_Err gf_term_get_visual_output_size(GF_Terminal *term, u32 *width, u32 *height)
{
	if (!term) return GF_BAD_PARAM;
	if (width) *width = term->compositor->vp_width;
	if (height) *height = term->compositor->vp_height;
	return GF_OK;
}

GF_EXPORT
u32 gf_term_get_clock(GF_Terminal *term)
{
	return gf_sc_ar_get_clock(term->compositor->audio_renderer);
}



GF_EXPORT
u32 gf_term_process_step(GF_Terminal *term)
{
	return gf_fs_run_step(term->fsess);
}

GF_EXPORT
GF_Err gf_term_process_flush(GF_Terminal *term)
{
	u32 diff, now = gf_sys_clock();
	if (!(term->user->init_flags  & GF_TERM_NO_COMPOSITOR_THREAD) ) return GF_BAD_PARAM;

	/*update till frame mature*/
	while (1) {

		gf_fs_run_step(term->fsess);

		if (!gf_sc_draw_frame(term->compositor, 1, NULL)) {
			if (!term->compositor->root_scene || !term->compositor->root_scene->root_od)
				break;

			//wait for audio to be flushed
			if (gf_sc_check_audio_pending(term->compositor) )
				continue;

			//force end of buffer
			if (gf_scene_check_clocks(term->compositor->root_scene->root_od->scene_ns, term->compositor->root_scene, 1))
				break;

			//consider timeout after 30 s
			diff = gf_sys_clock() - now;
			if (diff>30000) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Waited more than %d ms to flush frame - aborting\n", diff));
				return GF_IP_UDP_TIMEOUT;
			}
		}

		if (! (term->user->init_flags & GF_TERM_NO_REGULATION))
			break;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_term_process_flush_video(GF_Terminal *term)
{
	if (!(term->user->init_flags & GF_TERM_NO_COMPOSITOR_THREAD) ) return GF_BAD_PARAM;
	gf_sc_flush_video(term->compositor);
	return GF_OK;
}


