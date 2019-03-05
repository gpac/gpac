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

#include "media_memory.h"


/*creates service for given OD / URL
SHALL ONLY BE USED BY MAIN CLIENT*/
static void gf_term_connect_object(GF_Terminal *app, GF_ObjectManager *odm, char *serviceURL, char *parent_url);

void gf_term_load_shortcuts(GF_Terminal *term);

u32 gf_term_get_time(GF_Terminal *term)
{
	assert(term);
	return gf_sc_get_clock(term->compositor);
}

void gf_term_invalidate_compositor(GF_Terminal *term)
{
	gf_sc_invalidate(term->compositor, NULL);
}

static Bool check_user(GF_User *user)
{
	if (!user->config) return 0;
	if (!user->modules) return 0;
	if (!user->opaque) return 0;
	return 1;
}

void gf_term_message_ex(GF_Terminal *term, const char *service, const char *message, GF_Err error, Bool no_filtering)
{
	GF_Event evt;
	if (!term || !term->user) return;
	memset(&evt, 0, sizeof(GF_Event));
	evt.type = GF_EVENT_MESSAGE;
	evt.message.service = service;
	evt.message.message = message;
	evt.message.error = error;

	if (no_filtering) {
		if (term->user->EventProc) {
			term->nb_calls_in_event_proc++;
			term->user->EventProc(term->user->opaque, &evt);
			term->nb_calls_in_event_proc--;
		}
	} else {
		gf_term_send_event(term, &evt);
	}
}

GF_EXPORT
void gf_term_message(GF_Terminal *term, const char *service, const char *message, GF_Err error)
{
	gf_term_message_ex(term, service, message, error, 0);
}

static Bool term_script_action(void *opaque, u32 type, GF_Node *n, GF_JSAPIParam *param)
{
	Bool ret;
	GF_Terminal *term = (GF_Terminal *) opaque;

	if (type==GF_JSAPI_OP_MESSAGE) {
		gf_term_message_ex(term, term->root_scene->root_od->net_service->url, param->info.msg, param->info.e, 1);
		return 1;
	}
	if (type==GF_JSAPI_OP_GET_TERM) {
		param->term = term;
		return 1;
	}
	if (type==GF_JSAPI_OP_RESOLVE_XLINK) {
#ifndef GPAC_DISABLE_SVG
		param->uri.url = (char *) gf_term_resolve_xlink(n, (char *) param->uri.url);
		return 1;
#else
		return 0;
#endif
	}
	if (type==GF_JSAPI_OP_GET_OPT) {
		param->gpac_cfg.key_val = gf_cfg_get_key(term->user->config, param->gpac_cfg.section, param->gpac_cfg.key);
		return 1;
	}
	if (type==GF_JSAPI_OP_SET_OPT) {
		gf_cfg_set_key(term->user->config, param->gpac_cfg.section, param->gpac_cfg.key, param->gpac_cfg.key_val);
		return 1;
	}
	if (type==GF_JSAPI_OP_GET_DOWNLOAD_MANAGER) {
		param->dnld_man = term->downloader;
		return 1;
	}
	if (type==GF_JSAPI_OP_SET_TITLE) {
		GF_Event evt;
		if (!term->user->EventProc) return 0;
		evt.type = GF_EVENT_SET_CAPTION;
		evt.caption.caption = param->uri.url;
		gf_term_send_event(term, &evt);
		return 1;
	}
	if (type==GF_JSAPI_OP_GET_DCCI) {
		param->scene = term->dcci_doc;
		return 1;
	}
	if (type==GF_JSAPI_OP_GET_SUBSCENE) {
		GF_Scene *scene = (GF_Scene *)gf_node_get_private(n);
		param->scene = scene->graph;
		return 1;
	}

	if (type==GF_JSAPI_OP_RESOLVE_URI) {
		char *url;
		char new_url[GF_MAX_PATH], localized_url[GF_MAX_PATH];
		Bool result;
		GF_Scene *scene = (GF_Scene *)gf_sg_get_private(gf_node_get_graph(n));
		url = (char *)param->uri.url;
		if (!url) {
			param->uri.url = gf_strdup(scene->root_od->net_service->url);
			param->uri.nb_params = 0;
			return 1;
		}

		result = gf_term_relocate_url(term, url, scene->root_od->net_service->url, new_url, localized_url);
		if (result) param->uri.url = gf_strdup(new_url);
		else param->uri.url = gf_url_concatenate(scene->root_od->net_service->url, url);
		return 1;
	}

	/*special case for pause/stop/resume*/
	if (type==GF_JSAPI_OP_PAUSE_SVG) {
		GF_SceneGraph *graph = gf_node_get_graph(n);
		if (n == gf_sg_get_root_node(graph)) {
			GF_Scene *scene = (GF_Scene *)gf_sg_get_private(graph);
			GF_Clock *ck = scene->scene_codec ? scene->scene_codec->ck : scene->dyn_ck;
			if (ck) gf_clock_pause(ck);
			return 1;
		}
	}
	if (type==GF_JSAPI_OP_RESUME_SVG) {
		GF_SceneGraph *graph = gf_node_get_graph(n);
		if (n == gf_sg_get_root_node(graph)) {
			GF_Scene *scene = (GF_Scene *)gf_sg_get_private(graph);
			GF_Clock *ck = scene->scene_codec ? scene->scene_codec->ck : scene->dyn_ck;
			if (ck) gf_clock_resume(ck);
			return 1;
		}
	}
	if (type==GF_JSAPI_OP_RESTART_SVG) {
		GF_SceneGraph *graph = gf_node_get_graph(n);
		if (n == gf_sg_get_root_node(graph)) {
			GF_Scene *scene = (GF_Scene *)gf_sg_get_private(graph);
			GF_Clock *ck = scene->scene_codec ? scene->scene_codec->ck : scene->dyn_ck;
			if (ck) {
				Bool is_paused = ck->Paused;
				if (is_paused) gf_clock_resume(ck);
				gf_scene_restart_dynamic(scene, 0, 0, 0);
				if (is_paused) gf_clock_pause(ck);
			}
			return 1;
		}
		return 0;
	}
	if (type==GF_JSAPI_OP_SET_SCENE_SPEED) {
		GF_SceneGraph *graph = gf_node_get_graph(n);
		if (n == gf_sg_get_root_node(graph)) {
			GF_Scene *scene = (GF_Scene *)gf_sg_get_private(graph);
			GF_Clock *ck = scene->scene_codec ? scene->scene_codec->ck : scene->dyn_ck;
			if (ck) {
				gf_clock_set_speed(ck, param->val);
			}
			return 1;
		}
		return 0;
	}


	ret = gf_sc_script_action(term->compositor, type, n, param);
	if (ret) return ret;

	if (type==GF_JSAPI_OP_LOAD_URL) {
		if (gf_sg_get_private(gf_node_get_graph(n)) == term->root_scene) {
			GF_Event evt;
			if (!term->user->EventProc) return 0;
			evt.type = GF_EVENT_NAVIGATE;
			evt.navigate.to_url = param->uri.url;
			evt.navigate.parameters = param->uri.params;
			evt.navigate.param_count = param->uri.nb_params;
			return gf_term_send_event(term, &evt);
		} else {
			/*TODO*/
			return 0;
		}
	}
	return 0;
}


static Bool term_find_res(GF_TermLocales *loc, char *parent, char *path, char *relocated_path, char *localized_rel_path)
{
	FILE *f;

	if (loc->szAbsRelocatedPath) gf_free(loc->szAbsRelocatedPath);
	loc->szAbsRelocatedPath = gf_url_concatenate(parent, path);
	if (!loc->szAbsRelocatedPath) loc->szAbsRelocatedPath = gf_strdup(path);

	f = gf_fopen(loc->szAbsRelocatedPath, "rb");
	if (f) {
		gf_fclose(f);
		strcpy(localized_rel_path, path);
		strcpy(relocated_path, loc->szAbsRelocatedPath);
		return 1;
	}
	return 0;
}

/* Checks if, for a given relative path, there exists a localized version in an given folder
   if this is the case, it returns the absolute localized path, otherwise it returns null.
   if the resource was localized, the last parameter is set to the localized relative path.
*/
static Bool term_check_locales(void *__self, const char *locales_parent_path, const char *rel_path, char *relocated_path, char *localized_rel_path)
{
	char path[GF_MAX_PATH];
	const char *opt;

	GF_TermLocales *loc = (GF_TermLocales*)__self;

	/* Checks if the rel_path argument really contains a relative path (no ':', no '/' at the beginning) */
	if (strstr(rel_path, "://") || (rel_path[0]=='/') || strstr(rel_path, ":\\") || !strncmp(rel_path, "\\\\", 2)) {
		return 0;
	}

	/*Checks if the absolute path is really absolute and points to a local file (no http or else) */
	if (!locales_parent_path ||
	        (locales_parent_path && (locales_parent_path[0] != '/') && strstr(locales_parent_path, "://") && strnicmp(locales_parent_path, "file://", 7))) {
		return 0;
	}
	opt = gf_cfg_get_key(loc->term->user->config, "Systems", "Language2CC");
	if (opt) {
		if (!strcmp(opt, "*") || !strcmp(opt, "un") )
			opt = NULL;
	}

	while (opt) {
		char lan[100];
		char *sep;
		char *sep_lang = strchr(opt, ';');
		if (sep_lang) sep_lang[0] = 0;

		while (strchr(" \t", opt[0]))
			opt++;

		strcpy(lan, opt);

		if (sep_lang) {
			sep_lang[0] = ';';
			opt = sep_lang+1;
		} else {
			opt = NULL;
		}

		while (1) {
			sep = strstr(lan, "-*");
			if (!sep) break;
			strncpy(sep, sep+2, strlen(sep)-2);
		}

		sprintf(path, "locales/%s/%s", lan, rel_path);
		if (term_find_res(loc, (char *) locales_parent_path, (char *) path, relocated_path, localized_rel_path))
			return 1;

		/*recursively remove region (sub)tags*/
		while (1) {
			sep = strrchr(lan, '-');
			if (!sep) break;
			sep[0] = 0;
			sprintf(path, "locales/%s/%s", lan, rel_path);
			if (term_find_res(loc, (char *) locales_parent_path, (char *) path, relocated_path, localized_rel_path))
				return 1;
		}
	}

	if (term_find_res(loc, (char *) locales_parent_path, (char *) rel_path, relocated_path, localized_rel_path))
		return 1;
	/* if we did not find the localized file, both the relocated and localized strings are NULL */
	strcpy(localized_rel_path, "");
	strcpy(relocated_path, "");
	return 0;
}


static void gf_term_reload_cfg(GF_Terminal *term)
{
	const char *sOpt;
	u32 mode;
	s32 prio;

	if (!term) return;

	/*reload term part*/

	sOpt = gf_cfg_get_key(term->user->config, "Systems", "DrawLateFrames");
	if (sOpt && !stricmp(sOpt, "no"))
		term->flags |= GF_TERM_DROP_LATE_FRAMES;
	else
		term->flags &= ~GF_TERM_DROP_LATE_FRAMES;

	sOpt = gf_cfg_get_key(term->user->config, "Systems", "ForceSingleClock");
	if (sOpt && !stricmp(sOpt, "yes"))
		term->flags |= GF_TERM_SINGLE_CLOCK;
	else
		term->flags &= ~GF_TERM_SINGLE_CLOCK;

	sOpt = gf_cfg_get_key(term->user->config, "Systems", "TimeSlice");
	if (!sOpt) {
		sOpt = "30";
		gf_cfg_set_key(term->user->config, "Systems", "TimeSlice", "30");
	}
	term->frame_duration = atoi(sOpt);

	sOpt = gf_cfg_get_key(term->user->config, "Network", "LowLatencyBufferMax");
	if (!sOpt) {
		sOpt = "500";
		gf_cfg_set_key(term->user->config, "Network", "LowLatencyBufferMax", sOpt);
	}
	term->low_latency_buffer_max = atoi(sOpt);


	if (!(term->user->init_flags & GF_TERM_NO_DECODER_THREAD) ) {
		prio = GF_THREAD_PRIORITY_NORMAL;
		sOpt = gf_cfg_get_key(term->user->config, "Systems", "Priority");
		if (sOpt) {
			if (!stricmp(sOpt, "low")) prio = GF_THREAD_PRIORITY_LOWEST;
			else if (!stricmp(sOpt, "normal")) prio = GF_THREAD_PRIORITY_NORMAL;
			else if (!stricmp(sOpt, "high")) prio = GF_THREAD_PRIORITY_HIGHEST;
			else if (!stricmp(sOpt, "real-time")) prio = GF_THREAD_PRIORITY_REALTIME;
		} else {
			gf_cfg_set_key(term->user->config, "Systems", "Priority", "normal");
		}
		gf_term_set_priority(term, prio);

		sOpt = gf_cfg_get_key(term->user->config, "Systems", "ThreadingPolicy");
		if (sOpt) {
			mode = GF_TERM_THREAD_FREE;
			if (!stricmp(sOpt, "Single")) mode = GF_TERM_THREAD_SINGLE;
			else if (!stricmp(sOpt, "Multi")) mode = GF_TERM_THREAD_MULTI;
			gf_term_set_threading(term, mode);
		}
	} else {
		gf_term_set_threading(term, GF_TERM_THREAD_SINGLE);
	}

	term->prefered_audio_codec_oti = 0;
	sOpt = gf_cfg_get_key(term->user->config, "Systems", "DefAudioOTI");
	if (sOpt) {
		if (sscanf(sOpt, "0x%x", & term->prefered_audio_codec_oti) != 1) {
			sscanf(sOpt, "%x", & term->prefered_audio_codec_oti);
		}
	}

	/*default data timeout is 20 sec*/
	term->net_data_timeout = 20000;
	sOpt = gf_cfg_get_key(term->user->config, "Network", "DataTimeout");
	if (sOpt) term->net_data_timeout = atoi(sOpt);

	if (term->root_scene) gf_scene_set_duration(term->root_scene);

#ifndef GPAC_DISABLE_SVG
	if (term->dcci_doc) {
		sOpt = gf_cfg_get_key(term->user->config, "General", "EnvironmentFile");
		gf_sg_reload_xml_doc(sOpt, term->dcci_doc);
	}
#endif

	gf_term_load_shortcuts(term);
}

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

static GF_Err gf_term_step_clocks_intern(GF_Terminal * term, u32 ms_diff, Bool force_resume_pause)
{
	/*only play/pause if connected*/
	if (!term || !term->root_scene || !term->root_scene->root_od) return GF_BAD_PARAM;

	if (ms_diff) {
		u32 i, j;
		GF_ClientService *ns;
		GF_Clock *ck;

		if (term->play_state == GF_STATE_PLAYING) return GF_BAD_PARAM;

		gf_sc_lock(term->compositor, 1);
		i = 0;
		while ((ns = (GF_ClientService*)gf_list_enum(term->net_services, &i))) {
			j = 0;
			while ((ck = (GF_Clock *)gf_list_enum(ns->Clocks, &j))) {
				ck->init_time += ms_diff;
				ck->media_time_at_init += ms_diff;
				//make sure we don't touch clock while doing resume/pause below
				if (force_resume_pause)
					ck->Paused++;
			}
		}
		term->compositor->step_mode = GF_TRUE;
		term->use_step_mode = GF_TRUE;
		gf_sc_next_frame_state(term->compositor, GF_SC_DRAW_FRAME);

		//resume/pause to trigger codecs state change 
		if (force_resume_pause) {
			mediacontrol_resume(term->root_scene->root_od, 0);
			mediacontrol_pause(term->root_scene->root_od);

			//release our safety
			i = 0;
			while ((ns = (GF_ClientService*)gf_list_enum(term->net_services, &i))) {
				j = 0;
				while ((ck = (GF_Clock *)gf_list_enum(ns->Clocks, &j))) {
					ck->Paused--;
				}
			}
		}

		gf_sc_lock(term->compositor, 0);

	}

	gf_sc_flush_next_audio(term->compositor);
	return GF_OK;
}

static void gf_term_set_play_state(GF_Terminal *term, u32 PlayState, Bool reset_audio, Bool pause_clocks)
{
	Bool resume_live = 0;
	u32 prev_state;

	/*only play/pause if connected*/
	if (!term || !term->root_scene) return;

	prev_state = term->play_state;
	term->use_step_mode = GF_FALSE;

	if (PlayState==GF_STATE_PLAY_LIVE) {
		PlayState = GF_STATE_PLAYING;
		resume_live = 1;
		if (term->play_state == GF_STATE_PLAYING) {
			term->play_state = GF_STATE_PAUSED;
			mediacontrol_pause(term->root_scene->root_od);
		}
	}

	/*and if not already paused/playing*/
	if ((term->play_state == GF_STATE_PLAYING) && (PlayState == GF_STATE_PLAYING)) return;
	if ((term->play_state != GF_STATE_PLAYING) && (PlayState == GF_STATE_PAUSED)) return;

	/*pause compositor*/
	if ((PlayState==GF_STATE_PLAYING) && reset_audio)
		gf_sc_set_option(term->compositor, GF_OPT_PLAY_STATE, 0xFF);
	else
		gf_sc_set_option(term->compositor, GF_OPT_PLAY_STATE, PlayState);

	/* step mode specific */
	if (PlayState==GF_STATE_STEP_PAUSE) {
		if (prev_state==GF_STATE_PLAYING) {
			mediacontrol_pause(term->root_scene->root_od);
			term->play_state = GF_STATE_PAUSED;
		} else {
			u32 diff=1;
			if (term->compositor->ms_until_next_frame>0) diff = term->compositor->ms_until_next_frame;
			gf_term_step_clocks_intern(term, diff, GF_TRUE);
		}
		return;
	}

	/* nothing to change*/
	if (term->play_state == PlayState) return;
	term->play_state = PlayState;

	if (term->root_scene->first_frame_pause_type && (PlayState == GF_STATE_PLAYING))
		term->root_scene->first_frame_pause_type = 0;

	if (!pause_clocks) return;

	if (PlayState != GF_STATE_PLAYING) {
		mediacontrol_pause(term->root_scene->root_od);
	} else {
		mediacontrol_resume(term->root_scene->root_od, resume_live);
	}

}

static void gf_term_connect_from_time_ex(GF_Terminal * term, const char *URL, u64 startTime, u32 pause_at_first_frame, Bool secondary_scene, const char *parent_path)
{
	GF_Scene *scene;
	GF_ObjectManager *odm;
	const char *main_url;
	if (!URL || !strlen(URL)) return;

	if (term->root_scene) {
		if (term->root_scene->root_od && term->root_scene->root_od->net_service) {
			main_url = term->root_scene->root_od->net_service->url;
			if (main_url && !strcmp(main_url, URL)) {
				gf_term_play_from_time(term, 0, pause_at_first_frame);
				return;
			}
		}
		/*disconnect*/
		gf_term_disconnect(term);
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Connecting to %s\n", URL));

	gf_term_lock_net(term, 1);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Creating new root scene\n", URL));
	/*create a new scene*/
	scene = gf_scene_new(NULL);
	gf_sg_set_script_action(scene->graph, term_script_action, term);
	odm = gf_odm_new();

	scene->root_od =  odm;
	term->root_scene = scene;
	odm->parentscene = NULL;
	odm->subscene = scene;
	odm->term = term;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] root scene created\n", URL));
	gf_term_lock_net(term, 0);

	odm->media_start_time = startTime;
	/*render first visual frame and pause*/
	if (pause_at_first_frame) {
		gf_term_set_play_state(term, GF_STATE_STEP_PAUSE, 0, 0);
		scene->first_frame_pause_type = pause_at_first_frame;
	}

	if (!strnicmp(URL, "views://", 8)) {
		odm->OD = (GF_ObjectDescriptor *)gf_odf_desc_new(GF_ODF_OD_TAG);
		gf_scene_generate_views(term->root_scene, (char *) URL+8, (char*)parent_path);
		return;
	}
	else if (!strnicmp(URL, "mosaic://", 9)) {
		odm->OD = (GF_ObjectDescriptor *)gf_odf_desc_new(GF_ODF_OD_TAG);
		gf_scene_generate_mosaic(term->root_scene, (char *) URL+9, (char*)parent_path);
		return;
	}

	/*connect - we don't have any parentID */
	gf_term_connect_object(term, odm, (char *) URL, (char*)parent_path);
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
	u32 i;
	GF_Terminal *tmp;
	const char *cf;

	if (!check_user(user)) return NULL;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Creating terminal\n"));

	tmp = (GF_Terminal*)gf_malloc(sizeof(GF_Terminal));
	if (!tmp) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to allocate GF_Terminal : OUT OF MEMORY ?\n"));
		return NULL;
	}
	memset(tmp, 0, sizeof(GF_Terminal));

	/*just for safety*/
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
		tmp->flags |= GF_TERM_NO_DECODER_THREAD;

	if (user->init_flags & GF_TERM_NO_COMPOSITOR_THREAD) {
		tmp->flags |= GF_TERM_NO_COMPOSITOR_THREAD;
	} else if (user->init_flags & GF_TERM_NO_VISUAL_THREAD) {
		tmp->flags |= GF_TERM_NO_VISUAL_THREAD;
	} else {
		cf = gf_cfg_get_key(user->config, "Systems", "NoVisualThread");
		if (!cf || !stricmp(cf, "no")) {
			tmp->flags &= ~GF_TERM_NO_VISUAL_THREAD;
		} else {
			tmp->flags |= GF_TERM_NO_VISUAL_THREAD;
		}
	}

	/*setup scene compositor*/
	tmp->compositor = gf_sc_new(user, !(tmp->flags & (GF_TERM_NO_VISUAL_THREAD|GF_TERM_NO_COMPOSITOR_THREAD)) , tmp);
	if (!tmp->compositor) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to create Compositor.\n"));
		gf_free(tmp);
		return NULL;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] compositor loaded\n"));
	gf_sc_set_fps(tmp->compositor, 30.0);
	tmp->frame_duration = (u32) (1000/30);

	tmp->downloader = gf_dm_new(user->config);
	gf_dm_set_auth_callback(tmp->downloader, gf_term_get_user_pass, tmp);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] downloader loaded\n"));

	tmp->net_services = gf_list_new();
	tmp->net_services_to_remove = gf_list_new();
	tmp->connection_tasks = gf_list_new();
	tmp->channels_pending = gf_list_new();
	tmp->media_queue = gf_list_new();
	tmp->media_queue_mx = gf_mx_new("MediaQueue");
	tmp->net_mx = gf_mx_new("GlobalNetwork");
	tmp->input_streams = gf_list_new();
	tmp->x3d_sensors = gf_list_new();

	/*mode is changed when reloading cfg*/
	gf_term_init_scheduler(tmp, GF_TERM_THREAD_FREE);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Terminal created - loading config\n"));
	gf_term_reload_cfg(tmp);

#ifndef GPAC_DISABLE_SVG
	cf = gf_cfg_get_key(user->config, "General", "EnvironmentFile");
	if (cf) {
		GF_Err e = gf_sg_new_from_xml_doc(cf, &tmp->dcci_doc);
		if (e!=GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Error %s while loading file %s - user environment disabled\n", gf_error_to_string(e), cf));
		} else {
			gf_sg_set_script_action(tmp->dcci_doc, term_script_action, tmp);
		}
	}
#endif

	/*load extensions*/
	tmp->extensions = gf_list_new();
	for (i=0; i< gf_modules_get_count(user->modules); i++) {
		GF_TermExt *ifce = (GF_TermExt *) gf_modules_load_interface(user->modules, i, GF_TERM_EXT_INTERFACE);
		if (ifce) gf_list_add(tmp->extensions, ifce);
	}

	tmp->unthreaded_extensions = gf_list_new();
	tmp->evt_mx = gf_mx_new("Event Filter");

	for (i=0; i< gf_list_count(tmp->extensions); i++) {
		GF_TermExt *ifce = gf_list_get(tmp->extensions, i);
		if (!ifce->process(ifce, GF_TERM_EXT_START, tmp)) {
			gf_modules_close_interface((GF_BaseInterface *) ifce);
			gf_list_rem(tmp->extensions, i);
			i--;
			continue;
		}

		if (ifce->caps & GF_TERM_EXTENSION_NOT_THREADED)
			gf_list_add(tmp->unthreaded_extensions, ifce);
	}

	gf_mx_p(tmp->mm_mx);
	if (!gf_list_count(tmp->unthreaded_extensions)) {
		gf_list_del(tmp->unthreaded_extensions);
		tmp->unthreaded_extensions = NULL;
	}
	gf_mx_v(tmp->mm_mx);

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

	gf_term_refresh_cache(user->config);
	cf = gf_cfg_get_key(user->config, "General", "GUIFile");
	if (cf) {
		gf_term_connect_from_time_ex(tmp, cf, 0, 0, 1, NULL);
	}
	return tmp;
}

GF_EXPORT
GF_Err gf_term_del(GF_Terminal * term)
{
	GF_Err e;
	u32 timeout, i;

	if (!term) return GF_BAD_PARAM;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Destroying terminal\n"));
	/*close main service*/
	gf_term_disconnect(term);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] main service disconnected\n"));

	/*signal we are being destroyed*/
	term->reload_state = 3;

	/*wait for destroy*/
	e = GF_IO_ERR;
	timeout = 1000;
	while (term->root_scene || gf_list_count(term->net_services) || gf_list_count(term->net_services_to_remove)) {
		gf_sleep(10);
		/*this shouldn't be needed but unfortunately there's a bug hanging around there...*/
		timeout--;
		if (!timeout) break;
	}
	if (timeout) {
		assert(!gf_list_count(term->net_services));
		assert(!gf_list_count(term->net_services_to_remove));
		e = GF_OK;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] All network services deleted\n"));
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Couldn not unload all services in 10 seconds, will leak ...\n"));
	}

	/*unload extensions*/
	for (i=0; i< gf_list_count(term->extensions); i++) {
		GF_TermExt *ifce = gf_list_get(term->extensions, i);
		ifce->process(ifce, GF_TERM_EXT_STOP, NULL);
	}

	/*stop the media manager */
	gf_term_stop_scheduler(term);

	/*remove all event filters*/
	gf_list_reset(term->event_filters);

	/*unload extensions*/
	for (i=0; i< gf_list_count(term->extensions); i++) {
		GF_TermExt *ifce = gf_list_get(term->extensions, i);
		gf_modules_close_interface((GF_BaseInterface *) ifce);
	}
	gf_list_del(term->extensions);
	if (term->unthreaded_extensions) gf_list_del(term->unthreaded_extensions);

	while (term->in_event_filter) gf_sleep(1);
	gf_mx_p(term->evt_mx);
	if (term->event_filters) {
		gf_list_del(term->event_filters);
		term->event_filters = NULL;
	}
	gf_mx_v(term->evt_mx);
	gf_mx_del(term->evt_mx);

	/*delete compositor before the input sensor stacks to avoid receiving events from the compositor
	when destroying these stacks*/
	gf_sc_del(term->compositor);

	gf_list_del(term->net_services);
	gf_list_del(term->net_services_to_remove);
	gf_list_del(term->connection_tasks);
	gf_list_del(term->input_streams);
	gf_list_del(term->x3d_sensors);
	assert(!gf_list_count(term->channels_pending));
	gf_list_del(term->channels_pending);
	assert(!gf_list_count(term->media_queue));
	assert(!term->nodes_pending);
	gf_list_del(term->media_queue);
	if (term->downloader) gf_dm_del(term->downloader);

	gf_mx_del(term->media_queue_mx);

	if (term->locales.szAbsRelocatedPath) gf_free(term->locales.szAbsRelocatedPath);
	gf_list_del(term->uri_relocators);


	if (term->dcci_doc) {
#ifndef GPAC_DISABLE_SCENE_DUMP
		if (term->dcci_doc->modified) {
			char *pref_file = (char *)gf_cfg_get_key(term->user->config, "General", "EnvironmentFile");
			GF_SceneDumper *dumper = gf_sm_dumper_new(term->dcci_doc, pref_file, GF_FALSE, ' ', GF_SM_DUMP_AUTO_XML);
			if (!dumper) return GF_IO_ERR;
			e = gf_sm_dump_graph(dumper, 1, 0);
			gf_sm_dumper_del(dumper);
		}
#endif
		gf_sg_del(term->dcci_doc);
	}
	gf_mx_del(term->net_mx);

	gf_sys_close();
	gf_free(term);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Terminal destroyed\n"));
	return e;
}

GF_EXPORT
GF_Err gf_term_step_clocks(GF_Terminal * term, u32 ms_diff)
{
	return gf_term_step_clocks_intern(term, ms_diff, GF_FALSE);

}
GF_EXPORT
void gf_term_connect_from_time(GF_Terminal * term, const char *URL, u64 startTime, u32 pause_at_first_frame)
{
	gf_term_connect_from_time_ex(term, URL, startTime, pause_at_first_frame, 0, NULL);
}

GF_EXPORT
void gf_term_connect(GF_Terminal * term, const char *URL)
{
	gf_term_connect_from_time_ex(term, URL, 0, 0, 0, NULL);
}

GF_EXPORT
void gf_term_connect_with_path(GF_Terminal * term, const char *URL, const char *parent_path)
{
	gf_term_connect_from_time_ex(term, URL, 0, 0, 0, parent_path);
}

GF_EXPORT
void gf_term_disconnect(GF_Terminal *term)
{
	Bool handle_services;
	if (!term || !term->root_scene) return;

	if (term->nb_calls_in_event_proc) {
		if (!term->disconnect_request_status)
			term->disconnect_request_status = 1;

		return;
	}

	/*resume*/
	if (term->play_state != GF_STATE_PLAYING) gf_term_set_play_state(term, GF_STATE_PLAYING, 1, 1);

	if (term->root_scene->root_od) {

		gf_term_lock_media_queue(term, 1);

		term->root_scene->root_od->action_type = GF_ODM_ACTION_DELETE;
		if (gf_list_find(term->media_queue, term->root_scene->root_od)<0)
			gf_list_add(term->media_queue, term->root_scene->root_od);

		gf_term_lock_media_queue(term, 0);
	} else {
		gf_scene_del(term->root_scene);
		term->root_scene = NULL;
	}
	handle_services = 0;
	if (term->flags & GF_TERM_NO_COMPOSITOR_THREAD)
		handle_services = 1;
	/*if an unthreaded term extension decides to disconnect the scene (validator does so), we must flush services now
	because we are called from gf_term_handle_services*/
	if (term->thread_id_handling_services == gf_th_id())
		handle_services = 1;

	while (term->root_scene || gf_list_count(term->net_services_to_remove) || gf_list_count(term->connection_tasks)  || gf_list_count(term->media_queue) ) {
		if (handle_services) {
			gf_term_handle_services(term);
		}
		gf_sleep(10);
	}
}

GF_EXPORT
const char *gf_term_get_url(GF_Terminal *term)
{
	if (!term || !term->root_scene || !term->root_scene->root_od || !term->root_scene->root_od->net_service) return NULL;
	return term->root_scene->root_od->net_service->url;
}

static GF_Err gf_term_set_cache_state(GF_Terminal *term, u32 state)
{
	GF_Err e;
	if (!term) return GF_BAD_PARAM;
	if (term->enable_cache && (state==GF_MEDIA_CACHE_ENABLED)) return GF_OK;
	if (!term->enable_cache && (state!=GF_MEDIA_CACHE_ENABLED)) return GF_OK;

	term->enable_cache = !term->enable_cache;
	/*not connected, nothing to do*/
	if (!term->root_scene) return GF_OK;
	gf_term_lock_net(term, 1);
	if (term->enable_cache) {
		/*otherwise start cache*/
		e = gf_term_service_cache_load(term->root_scene->root_od->net_service);
	} else {
		e = gf_term_service_cache_close(term->root_scene->root_od->net_service, (state==GF_MEDIA_CACHE_DISCARD) ? 1 : 0);
	}
	gf_term_lock_net(term, 0);
	return e;
}


/*set rendering option*/
GF_EXPORT
GF_Err gf_term_set_option(GF_Terminal * term, u32 type, u32 value)
{
	if (!term) return GF_BAD_PARAM;
	switch (type) {
	case GF_OPT_PLAY_STATE:
		gf_term_set_play_state(term, value, 0, 1);
		return GF_OK;
	case GF_OPT_RELOAD_CONFIG:
		gf_term_reload_cfg(term);
		/*reload compositor config*/
		gf_sc_set_option(term->compositor, GF_OPT_RELOAD_CONFIG, 1);
		return GF_OK;
	case GF_OPT_MEDIA_CACHE:
		gf_term_set_cache_state(term, value);
		return GF_OK;
	case GF_OPT_HTTP_MAX_RATE:
		gf_dm_set_data_rate(term->downloader, value);
		return GF_OK;
	case GF_OPT_VIDEO_BENCH:
		term->bench_mode = value;
		return gf_sc_set_option(term->compositor, type, value);
	case GF_OPT_MULTIVIEW_MODE:
		term->compositor->multiview_mode = value;
		return gf_sc_set_option(term->compositor, type, value);

	default:
		return gf_sc_set_option(term->compositor, type, value);
	}
}

GF_EXPORT
GF_Err gf_term_set_simulation_frame_rate(GF_Terminal * term, Double frame_rate)
{
	if (!term) return GF_BAD_PARAM;
	term->frame_duration = (u32) (1000.0 / frame_rate);
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



u32 gf_term_check_end_of_scene(GF_Terminal *term, Bool skip_interactions)
{
	if (!term->root_scene || !term->root_scene->root_od || !term->root_scene->root_od->net_service) return 1;
	if (!skip_interactions) {
		/*if input sensors consider the scene runs forever*/
		if (gf_list_count(term->input_streams)) return 0;
		if (gf_list_count(term->x3d_sensors)) return 0;
	}
	/*check no clocks are still running*/
	if (!gf_scene_check_clocks(term->root_scene->root_od->net_service, term->root_scene, 0)) return 0;
	if (term->root_scene->is_dynamic_scene) return 1;

	/*ask compositor if there are sensors*/
	return gf_sc_get_option(term->compositor, skip_interactions ? GF_OPT_IS_OVER : GF_OPT_IS_FINISHED);
}

/*get rendering option*/
GF_EXPORT
u32 gf_term_get_option(GF_Terminal * term, u32 type)
{
	if (!term) return 0;
	switch (type) {
	case GF_OPT_HAS_JAVASCRIPT:
		return gf_sg_has_scripting();
	case GF_OPT_IS_FINISHED:
		return gf_term_check_end_of_scene(term, 0);
	case GF_OPT_IS_OVER:
		return gf_term_check_end_of_scene(term, 1);
	case GF_OPT_MAIN_ADDON:
		return term->root_scene ? term->root_scene->main_addon_selected : 0;

	case GF_OPT_PLAY_STATE:
		if (term->compositor->step_mode) return GF_STATE_STEP_PAUSE;
		if (term->root_scene) {
			GF_Clock *ck = term->root_scene->dyn_ck;
			if (!ck) {
				if (!term->root_scene->scene_codec) return GF_STATE_PAUSED;
				ck = term->root_scene->scene_codec->ck;
				if (!ck) return GF_STATE_PAUSED;
			}
//			if (ck->Buffering) return GF_STATE_PLAYING;
		}
		if (term->play_state != GF_STATE_PLAYING) return GF_STATE_PAUSED;
		return GF_STATE_PLAYING;
	case GF_OPT_MEDIA_CACHE:
		if (!term->enable_cache) return GF_MEDIA_CACHE_DISABLED;
		else if (term->root_scene && term->root_scene->root_od->net_service->cache) return GF_MEDIA_CACHE_RUNNING;
		else return GF_MEDIA_CACHE_ENABLED;
	case GF_OPT_CAN_SELECT_STREAMS:
		return (term->root_scene && term->root_scene->is_dynamic_scene) ? 1 : 0;
	case GF_OPT_HTTP_MAX_RATE:
		return gf_dm_get_data_rate(term->downloader);
	case GF_OPT_VIDEO_BENCH:
		return term->bench_mode ? GF_TRUE : GF_FALSE;
	case GF_OPT_ORIENTATION_SENSORS_ACTIVE:
		return term->orientation_sensors_active;
	default:
		return gf_sc_get_option(term->compositor, type);
	}
}

static void gf_term_cleanup_pending_session(GF_Terminal *term, GF_ClientService *ns)
{
	if (gf_list_find(term->net_services, ns)<0) return;

	if (ns && ns->pending_service_session) {
		gf_dm_sess_del(ns->pending_service_session);
		ns->pending_service_session = NULL;
	}
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


void gf_term_handle_services(GF_Terminal *term)
{
	GF_ClientService *ns;


	if (term->disconnect_request_status == 1) {
		term->disconnect_request_status = 2;
		term->thread_id_handling_services = gf_th_id();
		gf_term_disconnect(term);
		return;
	}


	/*we could run into a deadlock if some thread has requested opening of a URL. If we cannot
	grab the media queue now, we'll do our management at the next cycle*/
	if (!gf_mx_try_lock(term->media_queue_mx))
		return;

	term->thread_id_handling_services = gf_th_id();

	/*play ODs that need it*/
	while (gf_list_count(term->media_queue)) {
		Bool destroy = 0;
		u32 act_type;
		GF_ObjectManager *odm = (GF_ObjectManager *)gf_list_get(term->media_queue, 0);
		gf_list_rem(term->media_queue, 0);
		/*unlock media queue before sending play/pause*/
		gf_term_lock_media_queue(term, 0);

		act_type = odm->action_type;
		odm->action_type = GF_ODM_ACTION_PLAY;
		switch (act_type) {
		case GF_ODM_ACTION_STOP:
			if (odm->mo /*&& odm->codec && odm->codec->CB  && (odm->codec->CB->Capacity==1)*/)
			{
				if (odm->addon) odm->flags |= GF_ODM_REGENERATE_SCENE;

				else if (odm->mo->OD_ID==GF_MEDIA_EXTERNAL_ID) destroy = 1;
				else if (odm->OD && (odm->OD->objectDescriptorID==GF_MEDIA_EXTERNAL_ID)) destroy = 1;
			}
			if (destroy) {
				gf_odm_disconnect(odm, 2);
			} else {
				gf_odm_stop(odm, 0);
			}
			break;
		case GF_ODM_ACTION_PLAY:
			gf_odm_play(odm);
			break;
		case GF_ODM_ACTION_DELETE:
			gf_odm_disconnect(odm, 2);
			break;
		case GF_ODM_ACTION_SCENE_DISCONNECT:
			assert(odm->subscene);
			gf_odm_stop(odm, 1);
			gf_scene_disconnect(odm->subscene, 1);
			break;
		case GF_ODM_ACTION_SCENE_RECONNECT:
			assert(odm->subscene);
			gf_scene_disconnect(odm->subscene, 0);
			break;
		case GF_ODM_ACTION_SCENE_INLINE_RESTART:
#ifndef GPAC_DISABLE_VRML
			gf_scene_mpeg4_inline_restart(odm->subscene);
#endif
			break;
		case GF_ODM_ACTION_SETUP:
			gf_odm_setup_task(odm);
			break;
		}

		/*relock before sending play/pause*/
		gf_term_lock_media_queue(term, 1);
	}

	/*finally process all connection tasks - we MUST do that after processing ODM tasks, as an ODM might have just destroyed
	a service we could query during the connection step*/
	while (gf_list_count(term->connection_tasks)) {
		GF_TermConnectObject *connect = gf_list_get(term->connection_tasks, 0);
		gf_list_rem(term->connection_tasks, 0);

		/*unlock media queue before sending connect*/
		gf_term_lock_media_queue(term, 0);

//		gf_mx_p(term->net_mx);

		/*if object has already been attached to its service (eg, task was posted but media_add occured inbetween), ignore*/
		if (!connect->odm->net_service && !(connect->odm->flags & GF_ODM_DESTROYED) ) {
			gf_term_connect_object(term, connect->odm, connect->service_url, connect->parent_url);
		} else if (connect->odm->OD && connect->odm->OD->RedirectOnly) {
			gf_term_connect_object(term, connect->odm, connect->service_url, connect->parent_url);
		}

//		gf_mx_v(term->net_mx);

		gf_free(connect->service_url);
		if (connect->parent_url) gf_free(connect->parent_url);
		gf_free(connect);

		/*relock media queue after sending connect*/
		gf_term_lock_media_queue(term, 1);
	}

	gf_term_lock_media_queue(term, 0);

	/*lock to avoid any start attemps from compositor*/
	if (gf_mx_try_lock(term->compositor->mx)) {
		while (gf_list_count(term->net_services_to_remove)) {
			gf_mx_p(term->net_mx);
			ns = (GF_ClientService*)gf_list_get(term->net_services_to_remove, 0);
			if (ns) gf_list_rem(term->net_services_to_remove, 0);
			gf_mx_v(term->net_mx);
			if (!ns) break;
			gf_term_service_del(ns);
		}

		if (term->nodes_pending) {
			u32 i, count, n_count;
			i=0;
			count = gf_list_count(term->nodes_pending);
			while (i<count) {
				GF_Node *n = (GF_Node *)gf_list_get(term->nodes_pending, i);
				gf_node_traverse(n, NULL);
				if (!term->nodes_pending) break;
				n_count = gf_list_count(term->nodes_pending);
				if (n_count==count) i++;
				else count=n_count;
			}
		}
		gf_sc_lock(term->compositor, 0);
	}

	/*extensions*/
	gf_mx_p(term->mm_mx);
	if (!term->reload_state && term->unthreaded_extensions) {
		u32 i, count;
		count = gf_list_count(term->unthreaded_extensions);
		for (i=0; i<count; i++) {
			GF_TermExt *ifce = gf_list_get(term->unthreaded_extensions, i);
			ifce->process(ifce, GF_TERM_EXT_PROCESS, NULL);
		}
	}
	gf_mx_v(term->mm_mx);


	/*need to reload*/
	if (term->reload_state == 1) {
		term->reload_state = 0;
		gf_term_disconnect(term);
		term->reload_state = 2;
	}
	if (term->reload_state == 2) {
		if (! gf_list_count(term->net_services)) {
			term->reload_state = 0;
			if (term->reload_url) {
				gf_term_connect(term, term->reload_url);
				gf_free(term->reload_url);
			}
			term->reload_url = NULL;
		}
	}
	term->thread_id_handling_services = 0;
}

void gf_term_queue_node_traverse(GF_Terminal *term, GF_Node *node)
{
	gf_sc_lock(term->compositor, 1);
	if (!term->nodes_pending) term->nodes_pending = gf_list_new();
	gf_list_add(term->nodes_pending, node);
	gf_sc_lock(term->compositor, 0);
}
void gf_term_unqueue_node_traverse(GF_Terminal *term, GF_Node *node)
{
	gf_sc_lock(term->compositor, 1);
	if (term->nodes_pending) {
		gf_list_del_item(term->nodes_pending, node);
		if (!gf_list_count(term->nodes_pending)) {
			gf_list_del(term->nodes_pending);
			term->nodes_pending = NULL;
		}
	}
	gf_sc_lock(term->compositor, 0);
}

void gf_term_check_connections_for_delete(GF_Terminal *term, GF_ObjectManager *odm)
{
	GF_TermConnectObject *ct;
	u32 i = 0;
	while (NULL != (ct = (gf_list_enum(term->connection_tasks, &i)))) {
		if (ct->odm == odm) {
			i--;
			gf_list_rem(term->connection_tasks, i);
			if (ct->parent_url) gf_free(ct->parent_url);
			gf_free(ct->service_url);
			gf_free(ct);
		}
	}
}

void gf_term_close_service(GF_Terminal *term, GF_ClientService *ns)
{
	GF_Err e;

	/*prevent the media manager / term to access the list of services to destroy, otherwise
	we could unload the module while poping its CloseService() call stack which can lead to
	random crashes (return addresses no longer valid) - cf any "stress mode" playback of a playlist*/
	gf_term_lock_media_queue(term, 1);

#if 0
	{
		GF_ObjectManager *odm;
		u32 i = 0;
		while (odm = gf_list_enum(term->media_queue, &i)) {
			assert(odm->net_service != ns);
		}
	}

	{
		GF_TermConnectObject *ct;
		u32 i = 0;
		while (ct = gf_list_enum(term->connection_tasks, &i)) {
			assert(ct->odm->net_service != ns);
		}
	}
#endif

	ns->owner = NULL;
	e = ns->ifce->CloseService(ns->ifce);

	/*if error don't wait for ACK to remove from main list*/
	if (e) {
		gf_list_del_item(term->net_services, ns);
		if (gf_list_find(term->net_services_to_remove, ns)<0)
			gf_list_add(term->net_services_to_remove, ns);
	}
	gf_term_lock_media_queue(term, 0);
}

GF_EXPORT
void gf_term_lock_compositor(GF_Terminal *term, Bool LockIt)
{
	gf_sc_lock(term->compositor, LockIt);
}

/*locks media quaue*/
GF_EXPORT
void gf_term_lock_media_queue(GF_Terminal *term, Bool LockIt)
{
	if (LockIt) {
		gf_mx_p(term->media_queue_mx);
	} else {
		gf_mx_v(term->media_queue_mx);
	}
}

void gf_term_lock_net(GF_Terminal *term, Bool LockIt)
{
	if (LockIt) {
		gf_mx_p(term->net_mx);
	} else {
		gf_mx_v(term->net_mx);
	}
}

#ifndef GPAC_DISABLE_SVG
void media_event_collect_info(GF_ClientService *net, GF_ObjectManager *odm, GF_DOMMediaEvent *media_event, u32 *min_time, u32 *min_buffer)
{
	u32 i=0;
	GF_Channel *ch;

	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
		u32 val;
		if (ch->service != net) continue;

		if (ch->BufferOn)
			media_event->bufferValid = GF_TRUE;

		if (ch->BufferTime>0) {
			if (ch->MaxBuffer) {
				val = (ch->BufferTime * 100) / ch->MaxBuffer;
				if (*min_buffer > val) *min_buffer = val;
			} else {
				if (*min_buffer > 100) *min_buffer = 100;
			}
			if (*min_time > (u32) ch->BufferTime)
				*min_time = ch->BufferTime;
		} else {
			*min_time = 0;
			*min_buffer = 0;
		}
	}
}
#endif

void gf_term_service_media_event_with_download(GF_ObjectManager *odm, GF_EventType event_type, u64 loaded_size, u64 total_size, u32 bytes_per_sec)
{
#ifndef GPAC_DISABLE_SVG
	u32 i, count, min_buffer, min_time;
	GF_DOM_Event evt;
	GF_ObjectManager *an_od;
	GF_Scene *scene;

	if (!odm || !odm->net_service) return;
	if (odm->mo) {
		count = gf_mo_event_target_count(odm->mo);

		//for dynamic scenes, check if we have listeners on the root object of the scene containing this media
		if (odm->parentscene
		        && odm->parentscene->is_dynamic_scene
		        && odm->parentscene->root_od->mo
		        && (odm->parentscene->root_od->net_service==odm->net_service)
		   ) {
			odm = odm->parentscene->root_od;
			count = gf_mo_event_target_count(odm->mo);
		}
		if (!count) return;

		if (0 && !(gf_node_get_dom_event_filter((GF_Node *)gf_event_target_get_node(gf_mo_event_target_get(odm->mo, 0))) & GF_DOM_EVENT_MEDIA))
			return;
	} else {
		count = 0;
	}


	memset(&evt, 0, sizeof(GF_DOM_Event));

	evt.media_event.bufferValid = GF_FALSE;
	evt.media_event.session_name = odm->net_service->url;

	min_time = min_buffer = (u32) -1;
	scene = odm->subscene ? odm->subscene : odm->parentscene;
	if (!scene) return;
	
	/*get buffering on root OD*/
	media_event_collect_info(odm->net_service, scene->root_od, &evt.media_event, &min_time, &min_buffer);
	gf_mx_p(scene->mx_resources);
	/*get buffering on all ODs*/
	i=0;
	while ((an_od = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
		if (odm->net_service == an_od->net_service)
			media_event_collect_info(odm->net_service, an_od, &evt.media_event, &min_time, &min_buffer);
	}
	gf_mx_v(scene->mx_resources);

	if (min_buffer != (u32) -1)
		evt.media_event.level = min_buffer;
	if (min_time != (u32) -1)
		evt.media_event.remaining_time = INT2FIX(min_time) / 60;
	evt.media_event.status = 0;
	evt.media_event.loaded_size = loaded_size;
	evt.media_event.total_size = total_size;

	evt.type = event_type;
	evt.bubbles = 0;	/*the spec says yes but we force it to NO*/

	//these events may be triggered from any input or decoding threads. Sync processing cannot be
	//achieved in most cases, because we may run into deadlocks, especially if the event
	//was triggered by a service opened by JS
	for (i=0; i<count; i++) {
		GF_DOMEventTarget *target = (GF_DOMEventTarget *)gf_list_get(odm->mo->evt_targets, i);
		if (target)
			gf_sc_queue_dom_event_on_target(scene->root_od->term->compositor, &evt, target, scene->graph);
	}
	if (!count) {
		GF_Node *root = gf_sg_get_root_node(scene->graph);
		if (root) gf_sc_queue_dom_event(scene->root_od->term->compositor, root, &evt);
	}
#endif
}

void gf_term_service_media_event(GF_ObjectManager *odm, GF_EventType event_type)
{
	gf_term_service_media_event_with_download(odm, event_type, 0, 0, 0);
}

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

/*in most cases we cannot directly connect an object, as the request may come from a different thread than the one handling
ODM disconnection. We could therefore end up attaching an object to a service currently being destroyed because of a concurrent
odm_disconnect*/
void gf_term_post_connect_object(GF_Terminal *term, GF_ObjectManager *odm, char *serviceURL, char *parent_url)
{
	GF_TermConnectObject *connect;
	GF_SAFEALLOC(connect, GF_TermConnectObject);
	if (!connect) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to allocate media connection task\n"));
		return;
	}
	connect->odm = odm;
	connect->service_url = gf_strdup(serviceURL);
	connect->parent_url = parent_url ? gf_strdup(parent_url) : NULL;

	gf_term_lock_media_queue(term, 1);
	gf_list_add(term->connection_tasks, connect);
	gf_term_lock_media_queue(term, 0);
}

/*connects given OD manager to its URL*/
static void gf_term_connect_object(GF_Terminal *term, GF_ObjectManager *odm, char *serviceURL, char *parent_url)
{
	GF_ClientService *ns;
	u32 i, count;
	GF_Err e;
	Bool reloc_result, net_locked;
	char relocated_url[GF_MAX_PATH], localized_url[GF_MAX_PATH];

	/*try to relocate the url*/
	reloc_result = gf_term_relocate_url(term, serviceURL, parent_url, relocated_url, localized_url);
	if (reloc_result) serviceURL = (char *) relocated_url;

	/*check cache*/
	if (parent_url) {
		count = gf_cfg_get_section_count(term->user->config);
		for (i=0; i<count; i++) {
			u32 exp, sec, frac;
			const char *opt, *service_cache;
			const char *name = gf_cfg_get_section_name(term->user->config, i);
			if (strncmp(name, "@cache=", 7)) continue;
			opt = gf_cfg_get_key(term->user->config, name, "serviceURL");
			if (!opt || stricmp(opt, parent_url)) continue;
			opt = gf_cfg_get_key(term->user->config, name, "cacheName");
			if (!opt || stricmp(opt, serviceURL)) continue;

			service_cache = (char*)gf_cfg_get_key(term->user->config, name, "cacheFile");
			opt = gf_cfg_get_key(term->user->config, name, "expireAfterNTP");
			if (opt) {
				sscanf(opt, "%u", &exp);
				gf_net_get_ntp(&sec, &frac);
				if (exp && (exp<sec)) {
					opt = gf_cfg_get_key(term->user->config, name, "cacheFile");
					if (opt) gf_delete_file((char*) opt);
					gf_cfg_del_section(term->user->config, name);
					i--;
					count--;
					service_cache = NULL;
				}
			}
			if (service_cache) serviceURL = (char *)service_cache;
			break;
		}
	}

	/*for remoteODs/dynamic ODs, check if one of the running service cannot be used
	net mutex may be locked at this time (if another service sends a connect OK)*/
	net_locked = gf_mx_try_lock(term->net_mx);
	i=0;
	while ( (ns = (GF_ClientService*)gf_list_enum(term->net_services, &i)) ) {
		/*we shall not have a service scheduled for destruction here*/
		if (ns->owner && ( (ns->owner->flags & GF_ODM_DESTROYED) || (ns->owner->action_type == GF_ODM_ACTION_DELETE)) )
			continue;

		/*if service has timeline locked to its parent scene, only reuse it if new object does as well*/
		if (ns->owner->flags & GF_ODM_INHERIT_TIMELINE) {
			if (!(odm->flags & GF_ODM_INHERIT_TIMELINE)) continue;
		}

		if (gf_term_service_can_handle_url(ns, serviceURL)) {
			if (net_locked) {
				gf_term_lock_net(term, 0);
			}

			/*wait for service to setup - service may become destroyed if not available*/
			while (1) {
				net_locked = gf_mx_try_lock(term->net_mx);
				if (!ns->owner) {
					if (net_locked) {
						gf_term_lock_net(term, 0);
					}
					return;
				}
				if (net_locked) {
					gf_term_lock_net(term, 0);
				}

				if (ns->owner->OD) break;
				gf_sleep(5);
			}

			gf_mx_p(term->net_mx);
			if (odm->net_service) {
				gf_mx_v(term->net_mx);
				return;
			}
			if (odm->flags & GF_ODM_DESTROYED) {
				gf_mx_v(term->net_mx);
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] Object has been scheduled for destruction - ignoring object setup\n", odm->OD->objectDescriptorID));
				return;
			}
			odm->net_service = ns;
			odm->net_service->nb_odm_users ++;
			gf_odm_setup_entry_point(odm, serviceURL);
			gf_mx_v(term->net_mx);
			return;
		}
	}
	if (net_locked)
		gf_term_lock_net(term, 0);

	if (odm->net_service) {
		assert(odm->net_service->owner != odm);
		assert(odm->OD);
		assert(odm->OD->URLString);
		assert(odm->net_service->nb_odm_users);
		odm->net_service->nb_odm_users--;
	}
	odm->net_service = gf_term_service_new(term, odm, serviceURL, (odm->addon || reloc_result) ? NULL : parent_url, &e);
	if (!odm->net_service) {
		gf_term_message(term, serviceURL, "Cannot open service", e);
		gf_odm_disconnect(odm, 1);
		return;
	}
	odm->net_service->nb_odm_users++;

	assert(odm->net_service->owner == odm);

	/*OK connect*/
	gf_term_service_media_event(odm, GF_EVENT_MEDIA_SETUP_BEGIN);
	odm->net_service->ifce->ConnectService(odm->net_service->ifce, odm->net_service, odm->net_service->url);

	/*remove pending download session if any*/
	gf_term_cleanup_pending_session(term, ns);
}

GF_ClientService *gf_term_get_service_from_url(GF_Terminal *term, const char *url)
{
	u32 i = 0;
	GF_ClientService *ns;
	while ( (ns = (GF_ClientService*)gf_list_enum(term->net_services, &i)) ) {
		if (!strcmp(ns->url, url)) {
			return ns;
		}
	}
	return NULL;
}

/*connects given channel to its URL if needed*/
GF_Err gf_term_connect_remote_channel(GF_Terminal *term, GF_Channel *ch, char *URL)
{
	GF_Err e;
	u32 i;
	GF_ClientService *ns;

	gf_term_lock_net(term, 1);

	/*if service is handled by current service don't create new one*/
	if (gf_term_service_can_handle_url(ch->service, URL)) {
		gf_term_lock_net(term, 0);
		return GF_OK;
	}
	i=0;
	while ( (ns = (GF_ClientService*)gf_list_enum(term->net_services, &i)) ) {
		if (gf_term_service_can_handle_url(ns, URL)) {
			ch->service = ns;
			gf_term_lock_net(term, 0);
			return GF_OK;
		}
	}
	/*use parent OD for parent service*/
	ns = gf_term_service_new(term, NULL, URL, ch->odm->net_service->url, &e);
	if (!ns) return e;
	ch->service = ns;
	ns->ifce->ConnectService(ns->ifce, ns, ns->url);

	gf_term_cleanup_pending_session(term, ns);
	gf_term_lock_net(term, 0);
	return GF_OK;
}

GF_EXPORT
u32 gf_term_play_from_time(GF_Terminal *term, u64 from_time, u32 pause_at_first_frame)
{
	if (!term || !term->root_scene || !term->root_scene->root_od) return 0;
	if (term->root_scene->root_od->flags & GF_ODM_NO_TIME_CTRL) return 1;

	if (pause_at_first_frame==2) {
		if (gf_term_get_option(term, GF_OPT_PLAY_STATE) != GF_STATE_PLAYING)
			pause_at_first_frame = 1;
		else
			pause_at_first_frame = 0;
	}

	/*for dynamic scene OD ressources are static and all object use the same clock, so don't restart the root
	OD, just act as a mediaControl on all playing streams*/
	if (term->root_scene->is_dynamic_scene) {

		/*exit pause mode*/
		gf_term_set_play_state(term, GF_STATE_PLAYING, 1, 1);

		if (pause_at_first_frame)
			gf_term_set_play_state(term, GF_STATE_STEP_PAUSE, 0, 0);

		gf_sc_lock(term->compositor, 1);
		gf_scene_restart_dynamic(term->root_scene, from_time, 0, 0);
		gf_sc_lock(term->compositor, 0);
		return 2;
	}

	/*pause everything*/
	gf_term_set_play_state(term, GF_STATE_PAUSED, 0, 1);
	/*stop root*/
	gf_odm_stop(term->root_scene->root_od, 1);
	gf_scene_disconnect(term->root_scene, 0);
	/*make sure we don't have OD queued*/
	while (gf_list_count(term->media_queue)) gf_list_rem(term->media_queue, 0);
	term->root_scene->root_od->media_start_time = from_time;

	gf_odm_start(term->root_scene->root_od, 0);
	gf_term_set_play_state(term, GF_STATE_PLAYING, 0, 1);
	if (pause_at_first_frame)
		gf_sc_set_option(term->compositor, GF_OPT_PLAY_STATE, GF_STATE_STEP_PAUSE);
	return 2;
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
	if (!term || !term->root_scene) return 0;
	ck = NULL;
	if (term->root_scene->scene_codec && term->root_scene->scene_codec->ck) ck = term->root_scene->scene_codec->ck;
	else if (term->root_scene->dyn_ck) ck = term->root_scene->dyn_ck;
	if (!ck) return 0;
	return gf_clock_media_time(ck);
}

GF_EXPORT
u32 gf_term_get_elapsed_time_in_ms(GF_Terminal *term)
{
	GF_Clock *ck;
	if (!term || !term->root_scene) return 0;
	ck = NULL;
	if (term->root_scene->scene_codec && term->root_scene->scene_codec->ck) ck = term->root_scene->scene_codec->ck;
	else if (term->root_scene->dyn_ck) ck = term->root_scene->dyn_ck;
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
	if (!toURL && !term->root_scene) return;
	if (term->reload_url) gf_free(term->reload_url);
	term->reload_url = NULL;
	if (toURL) {
		if (term->root_scene && term->root_scene->root_od && term->root_scene->root_od->net_service)
			term->reload_url = gf_url_concatenate(term->root_scene->root_od->net_service->url, toURL);
		if (!term->reload_url) term->reload_url = gf_strdup(toURL);
	}
	term->reload_state = 1;
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
	GF_MediaObject *mo;
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
	return mo ? GF_OK : GF_NOT_SUPPORTED;
}


GF_EXPORT
void gf_term_attach_service(GF_Terminal *term, GF_InputService *service_hdl)
{
	Bool net_check_interface(GF_InputService *ifce);
	GF_Scene *scene;
	GF_ObjectManager *odm;
	GF_ClientService *ns;
	if (!net_check_interface(service_hdl)) return;

	if (term->root_scene) gf_term_disconnect(term);

	gf_term_lock_net(term, 1);

	/*create a new scene*/
	scene = gf_scene_new(NULL);
	odm = gf_odm_new();
	gf_sg_set_script_action(scene->graph, term_script_action, term);

	scene->root_od = odm;
	term->root_scene = scene;
	odm->parentscene = NULL;
	odm->subscene = scene;
	odm->term = term;

	GF_SAFEALLOC(odm->net_service , GF_ClientService);
	if (!odm->net_service) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to allocate network service\n"));
		gf_term_lock_net(term, 0);
		return;
	}
	odm->net_service->term = term;
	odm->net_service->owner = odm;
	odm->net_service->ifce = service_hdl;
	odm->net_service->url = gf_strdup("Internal Service Handler");
	odm->net_service->Clocks = gf_list_new();
	gf_list_add(term->net_services, odm->net_service);

	gf_term_lock_net(term, 0);

	ns = odm->net_service;
	/*OK connect*/
	odm->net_service->ifce->ConnectService(odm->net_service->ifce, odm->net_service, odm->net_service->url);

	gf_term_cleanup_pending_session(term, ns);
}

GF_EXPORT
GF_Err gf_term_scene_update(GF_Terminal *term, char *type, char *com)
{
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
		return gf_scene_execute_script(term->root_scene->graph, com);
	}

	if (!type && com && !strncmp(com, "gpac ", 5)) {
		com += 5;
		//new add-on
		if (term->root_scene && !strncmp(com, "add ", 4)) {
			GF_AssociatedContentLocation addon_info;
			memset(&addon_info, 0, sizeof(GF_AssociatedContentLocation));
			addon_info.external_URL = com + 4;
			addon_info.timeline_id = -100;
			gf_scene_register_associated_media(term->root_scene, &addon_info);
			return GF_OK;
		}
		//new splicing add-on
		if (term->root_scene && !strncmp(com, "splice ", 7)) {
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
				u32 count = gf_list_count(term->root_scene->declared_addons);
				for (i=0; i<count; i++) {
					GF_AddonMedia *addon = gf_list_get(term->root_scene->declared_addons, i);
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
			addon_info.splice_end_time = end;
			addon_info.splice_time_pts = is_pts;
			gf_scene_register_associated_media(term->root_scene, &addon_info);
			return GF_OK;
		}
		//select object
		if (term->root_scene && !strncmp(com, "select ", 7)) {
			u32 idx = atoi(com+7);
			gf_term_select_object(term, gf_list_get(term->root_scene->resources, idx));
			return GF_OK;
		}
		return GF_OK;
	}

	memset(&load, 0, sizeof(GF_SceneLoader));
	load.localPath = gf_cfg_get_key(term->user->config, "General", "CacheDirectory");
	load.flags = GF_SM_LOAD_FOR_PLAYBACK | GF_SM_LOAD_CONTEXT_READY;
	load.type = GF_SM_LOAD_BT;

	if (!term->root_scene) {
		gf_term_lock_net(term, 1);
		/*create a new scene*/
		term->root_scene = gf_scene_new(NULL);
		gf_sg_set_script_action(term->root_scene->graph, term_script_action, term);
		term->root_scene->root_od = gf_odm_new();
		term->root_scene->root_od->parentscene = NULL;
		term->root_scene->root_od->subscene = term->root_scene;
		term->root_scene->root_od->term = term;
		gf_term_lock_net(term, 0);
		load.ctx = gf_sm_new(term->root_scene->graph);
	} else if (term->root_scene->root_od->OD) {
		load.ctx = gf_sm_new(term->root_scene->graph);
		/*restore streams*/
		i=0;
		while ((esd = (GF_ESD*)gf_list_enum(term->root_scene->root_od->OD->ESDescriptors, &i)) ) {
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
	} else {
		return GF_BAD_PARAM;
	}
	load.ctx->max_node_id = gf_sg_get_max_node_id(term->root_scene->graph);

	i=0;
	while ((com[i] == ' ') || (com[i] == '\r') || (com[i] == '\n') || (com[i] == '\t')) i++;
	if (com[i]=='<') is_xml = 1;

	load.type = is_xml ? GF_SM_LOAD_XMTA : GF_SM_LOAD_BT;
	time = gf_scene_get_time(term->root_scene);


	if (type && (!stricmp(type, "application/x-laser+xml") || !stricmp(type, "laser"))) {
		load.type = GF_SM_LOAD_XSR;
		time = gf_scene_get_time(term->root_scene);
	}
	else if (type && (!stricmp(type, "image/svg+xml") || !stricmp(type, "svg")) ) {
		load.type = GF_SM_LOAD_XSR;
		time = gf_scene_get_time(term->root_scene);
	}
	else if (type && (!stricmp(type, "model/x3d+xml") || !stricmp(type, "x3d")) ) load.type = GF_SM_LOAD_X3D;
	else if (type && (!stricmp(type, "model/x3d+vrml") || !stricmp(type, "x3dv")) ) load.type = GF_SM_LOAD_X3DV;
	else if (type && (!stricmp(type, "model/vrml") || !stricmp(type, "vrml")) ) load.type = GF_SM_LOAD_VRML;
	else if (type && (!stricmp(type, "application/x-xmt") || !stricmp(type, "xmt")) ) load.type = GF_SM_LOAD_XMTA;
	else if (type && (!stricmp(type, "application/x-bt") || !stricmp(type, "bt")) ) load.type = GF_SM_LOAD_BT;
	else if (gf_sg_get_root_node(term->root_scene->graph)) {
		tag = gf_node_get_tag(gf_sg_get_root_node(term->root_scene->graph));
		if (tag >= GF_NODE_RANGE_FIRST_SVG) {
			load.type = GF_SM_LOAD_XSR;
			time = gf_scene_get_time(term->root_scene);
		} else if (tag>=GF_NODE_RANGE_FIRST_X3D) {
			load.type = is_xml ? GF_SM_LOAD_X3D : GF_SM_LOAD_X3DV;
		} else {
			load.type = is_xml ? GF_SM_LOAD_XMTA : GF_SM_LOAD_BT;
			time = gf_scene_get_time(term->root_scene);
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
				e = gf_sg_command_apply_list(term->root_scene->graph, au->commands, time);
				if (e) break;
			}
		}
	}
	if (!term->root_scene->graph_attached) {
		if (!load.ctx->scene_width || !load.ctx->scene_height) {
//			load.ctx->scene_width = term->compositor->width;
//			load.ctx->scene_height = term->compositor->height;
		}
		gf_sg_set_scene_size_info(term->root_scene->graph, load.ctx->scene_width, load.ctx->scene_height, load.ctx->is_pixel_metrics);
		gf_scene_attach_to_compositor(term->root_scene);
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

GF_EXPORT
Bool gf_term_forward_event(GF_Terminal *term, GF_Event *evt, Bool consumed, Bool forward_only)
{
	if (!term) return GF_FALSE;

	if (term->event_filters) {
		GF_TermEventFilter *ef;
		u32 i=0;

		gf_mx_p(term->evt_mx);
		term->in_event_filter ++;
		gf_mx_v(term->evt_mx);
		while ((ef=gf_list_enum(term->event_filters, &i))) {
			if (ef->on_event(ef->udta, evt, consumed)) {
				term->in_event_filter --;
				return GF_TRUE;
			}
		}
		term->in_event_filter --;
	}

	if (!forward_only && !consumed && term->user->EventProc) {
		Bool res;
		term->nb_calls_in_event_proc++;
		res = term->user->EventProc(term->user->opaque, evt);
		term->nb_calls_in_event_proc--;
		return res;
	}

	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_term_add_event_filter(GF_Terminal *terminal, GF_TermEventFilter *ef)
{
	GF_Err e;
	if (!terminal || !ef || !ef->on_event) return GF_BAD_PARAM;
	while (terminal->in_event_filter) gf_sleep(1);
	gf_mx_p(terminal->evt_mx);
	if (!terminal->event_filters) terminal->event_filters = gf_list_new();
	e = gf_list_add(terminal->event_filters, ef);
	gf_mx_v(terminal->evt_mx);
	return e;
}

GF_EXPORT
GF_Err gf_term_remove_event_filter(GF_Terminal *terminal, GF_TermEventFilter *ef)
{
	if (!terminal || !ef || !terminal->event_filters) return GF_BAD_PARAM;

	while (terminal->in_event_filter) gf_sleep(1);
	gf_mx_p(terminal->evt_mx);
	gf_list_del_item(terminal->event_filters, ef);
	if (!gf_list_count(terminal->event_filters)) {
		gf_list_del(terminal->event_filters);
		terminal->event_filters=NULL;
	}
	gf_mx_v(terminal->evt_mx);
	return GF_OK;
}


GF_EXPORT
Bool gf_term_send_event(GF_Terminal *term, GF_Event *evt)
{
	return gf_term_forward_event(term, evt, 0, 0);
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

static void set_clocks_speed(GF_Terminal *term, Fixed ratio)
{
	u32 i, j;
	GF_ClientService *ns;

	/*pause all clocks on all services*/
	i=0;
	while ( (ns = (GF_ClientService*)gf_list_enum(term->net_services, &i)) ) {
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
	GF_ClientService *ns;
	Bool restart = 0;
	u32 scene_time = gf_term_get_time_in_ms(term);

	if (!speed) return GF_BAD_PARAM;

	if (speed<0) {
		i=0;
		while ( (ns = (GF_ClientService*)gf_list_enum(term->net_services, &i)) ) {
			GF_NetworkCommand com;
			GF_Err e;
			memset(&com, 0, sizeof(GF_NetworkCommand));
			com.base.command_type = GF_NET_SERVICE_CAN_REVERSE_PLAYBACK;
			e = gf_term_service_command(ns, &com);
			if (e != GF_OK) {
				return e;
			}
		}
	}

	/*adjust all clocks on all services, if possible*/
	i=0;
	while ( (ns = (GF_ClientService*)gf_list_enum(term->net_services, &i)) ) {
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
		if (term->root_scene->is_dynamic_scene) {
			gf_scene_restart_dynamic(term->root_scene, scene_time, 0, 0);
		} else {
		}
	}

	if (speed<0)
		speed = -speed;

	opt = gf_cfg_get_key(term->user->config, "Systems", "TimeSlice");
	if (!opt) opt="30";
	i = (u32) ( atoi(opt) / FIX2FLT(speed) );
	if (!i) i = 1;
	term->frame_duration = i;

	opt = gf_cfg_get_key(term->user->config, "Compositor", "FrameRate");
	fps = atoi(opt);
	fps *= FIX2FLT(speed);
	if (fps>100) fps = 1000;
	gf_sc_set_fps(term->compositor, fps);

	return GF_OK;
}

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

void gf_scene_switch_quality(GF_Scene *scene, Bool up)
{
	u32 i;
	GF_ClientService *root_service = NULL;
	GF_ObjectManager *odm;
	GF_CodecCapability caps;
	GF_NetworkCommand net_cmd;

	if (!scene) return;

	/*send network command*/
	memset(&net_cmd, 0, sizeof(GF_NetworkCommand));
	net_cmd.command_type = GF_NET_SERVICE_QUALITY_SWITCH;
	net_cmd.switch_quality.on_channel = NULL;
	net_cmd.switch_quality.up = up;
	if (scene->root_od->net_service) {
		root_service = scene->root_od->net_service;
		root_service->ifce->ServiceCommand(root_service->ifce, &net_cmd);
	}

	/*notify all codecs in the scene and subscenes*/
	caps.CapCode = GF_CODEC_MEDIA_SWITCH_QUALITY;
	caps.cap.valueInt = up ? 1 : 0;
	if (scene->scene_codec) {
		scene->scene_codec->decio->SetCapabilities(scene->scene_codec->decio, caps);
	}
	i=0;
	while (NULL != (odm = gf_list_enum(scene->resources, &i))) {
		if (odm->codec)
			odm->codec->decio->SetCapabilities(odm->codec->decio, caps);
		if (odm->net_service && (odm->net_service != root_service) && (!odm->subscene || !odm->subscene->is_dynamic_scene) )
			odm->net_service->ifce->ServiceCommand(odm->net_service->ifce, &net_cmd);
		if (odm->subscene)
			gf_scene_switch_quality(odm->subscene, up);

		if (odm->scalable_addon) {
			if (up)
				gf_odm_start(odm, 0);
			else
				gf_odm_stop(odm, GF_FALSE);
		}
	}
}

GF_EXPORT
void gf_term_switch_quality(GF_Terminal *term, Bool up)
{
	gf_scene_switch_quality(term->root_scene, up);
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
