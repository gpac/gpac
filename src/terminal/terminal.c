/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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
#include <gpac/modules/term_ext.h>
#include <gpac/constants.h>
#include <gpac/options.h>
#include <gpac/network.h>
#include <gpac/xml.h>
/*textual command processing*/
#include <gpac/scene_manager.h>

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

static Bool term_script_action(void *opaque, u32 type, GF_Node *n, GF_JSAPIParam *param)
{
	Bool ret;
	GF_Terminal *term = (GF_Terminal *) opaque;

	if (type==GF_JSAPI_OP_MESSAGE) {
		gf_term_message(term, term->root_scene->root_od->net_service->url, param->info.msg, param->info.e);
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
		term->user->EventProc(term->user->opaque, &evt);
		return 1;
	}
	if (type==GF_JSAPI_OP_GET_DCCI) {
		param->scene = term->dcci_doc;
		return 1;
	}
	if (type==GF_JSAPI_OP_GET_SUBSCENE) {
		GF_InlineScene *is = (GF_InlineScene *)gf_node_get_private(n);
		param->scene = is->graph;
		return 1;
	}
	if (type==GF_JSAPI_OP_EVAL_IRI) {
		GF_InlineScene *is;
		if (!n) return 0;
		is = (GF_InlineScene *)gf_sg_get_private(gf_node_get_graph(n));
		if (!param->uri.url) {
			/*wait for compositor to be completely setup*/
			if (is->graph_attached && term->compositor->scene && !term->compositor->msg_type) return 1;
			return 0;
		}
		/*todo*/
		return 0;
	}
	

	if (type==GF_JSAPI_OP_GET_SCENE_URI) {
		GF_InlineScene *is = (GF_InlineScene *)gf_sg_get_private(gf_node_get_graph(n));
		param->uri.url = is->root_od->net_service->url;
		param->uri.nb_params = 0;
		return 1;
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
			return term->user->EventProc(term->user->opaque, &evt);
		} else {
			/*TODO*/
			return 0;
		}
	}
	return 0;
}


static void gf_term_reload_cfg(GF_Terminal *term)
{
	const char *sOpt;
	Double fps;
	u32 mode;
	s32 prio;

	if (!term) return;
	
	/*reload term part*/

	sOpt = gf_cfg_get_key(term->user->config, "Systems", "AlwaysDrawBIFS");
	if (sOpt && !stricmp(sOpt, "yes"))
		term->flags &= ~GF_TERM_SYSDEC_RESYNC;
	else
		term->flags |= GF_TERM_SYSDEC_RESYNC;

	sOpt = gf_cfg_get_key(term->user->config, "Systems", "ForceSingleClock");
	if (sOpt && !stricmp(sOpt, "yes")) 
		term->flags |= GF_TERM_SINGLE_CLOCK;
	else
		term->flags &= ~GF_TERM_SINGLE_CLOCK;

	sOpt = gf_cfg_get_key(term->user->config, "Compositor", "FrameRate");
	if (sOpt) {
		fps = atof(sOpt);
		term->frame_duration = (u32) (1000/fps);
		gf_sc_set_fps(term->compositor, fps);
	}

	if (term->user->init_flags & GF_TERM_NO_VISUAL_THREAD){
		//gf_term_set_threading(term->mediaman, 1);
	} else {
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
	}

	/*default data timeout is 20 sec*/
	term->net_data_timeout = 20000;
	sOpt = gf_cfg_get_key(term->user->config, "Network", "DataTimeout");
	if (sOpt) term->net_data_timeout = atoi(sOpt);

	if (term->root_scene) gf_inline_set_duration(term->root_scene);

#ifndef GPAC_DISABLE_SVG
	if (term->dcci_doc) {
		sOpt = gf_cfg_get_key(term->user->config, "General", "EnvironmentFile");
		gf_sg_reload_xml_doc(sOpt, term->dcci_doc);
	}
#endif
	
	/*reload compositor config*/
	gf_sc_set_option(term->compositor, GF_OPT_RELOAD_CONFIG, 1);
}

static Bool gf_term_get_user_pass(void *usr_cbk, const char *site_url, char *usr_name, char *password)
{
	GF_Event evt;
	GF_Terminal *term = (GF_Terminal *)usr_cbk;
	evt.auth.site_url = site_url;
	evt.auth.user = usr_name;
	evt.auth.password = password;
	return term->user->EventProc(term->user->opaque, &evt);
}


void gf_term_pause_all_clocks(GF_Terminal *term, Bool pause)
{
	u32 i, j;
	GF_ClientService *ns;
	/*pause all clocks on all services*/
	i=0;
	while ( (ns = (GF_ClientService*)gf_list_enum(term->net_services, &i)) ) {
		GF_Clock *ck;
		j=0;
		while ( (ck = (GF_Clock *)gf_list_enum(ns->Clocks, &j)) ) {
			if (pause) gf_clock_pause(ck);
			else gf_clock_resume(ck);
		}
	}
}

static void gf_term_set_play_state(GF_Terminal *term, u32 PlayState, Bool reset_audio, Bool pause_clocks)
{
	/*only play/pause if connected*/
	if (!term || !term->root_scene) return;
	/*and if not already paused/playing*/
	if (!term->play_state && !PlayState) return;
	if (term->play_state && (PlayState==GF_STATE_PAUSED)) return;

	/*pause compositor*/
	if ((PlayState==GF_STATE_PLAYING) && reset_audio)
		gf_sc_set_option(term->compositor, GF_OPT_PLAY_STATE, 0xFF);
	else
		gf_sc_set_option(term->compositor, GF_OPT_PLAY_STATE, PlayState);

	if (PlayState==GF_STATE_STEP_PAUSE) PlayState = term->play_state ? GF_STATE_PLAYING : GF_STATE_PAUSED;
	if (term->play_state == PlayState) return;
	term->play_state = PlayState;

	if (!pause_clocks) return;

	gf_term_pause_all_clocks(term, PlayState ? 1 : 0);
}

static void gf_term_connect_from_time_ex(GF_Terminal * term, const char *URL, u64 startTime, Bool pause_at_first_frame, Bool secondary_scene)
{
	GF_InlineScene *is;
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
	is = gf_inline_new(NULL);
	gf_sg_set_script_action(is->graph, term_script_action, term);
	odm = gf_odm_new();

	is->root_od =  odm;
	term->root_scene = is;
	odm->parentscene = NULL;
	odm->subscene = is;
	odm->term = term;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] root scene created\n", URL));
	gf_term_lock_net(term, 0);

	odm->media_start_time = startTime;
	/*render first visual frame and pause*/
	if (pause_at_first_frame)
		gf_term_set_play_state(term, GF_STATE_STEP_PAUSE, 0, 0);
	/*connect - we don't have any parentID */
	gf_term_connect_object(term, odm, (char *) URL, NULL);
}

GF_EXPORT
GF_Terminal *gf_term_new(GF_User *user)
{
	u32 i;
	GF_Terminal *tmp;
	const char *cf;

	if (!check_user(user)) return NULL;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Creating terminal\n"));

	tmp = (GF_Terminal*)malloc(sizeof(GF_Terminal));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_Terminal));

	/*just for safety*/
	gf_sys_init();

	tmp->user = user;

	/*this is not changeable at runtime*/
	if (user->init_flags & GF_TERM_NO_VISUAL_THREAD) {
		tmp->flags |= GF_TERM_DRAW_FRAME;
	} else {
		cf = gf_cfg_get_key(user->config, "Systems", "NoVisualThread");
		if (!cf || !stricmp(cf, "no")) {
			tmp->flags &= ~GF_TERM_DRAW_FRAME;
		} else {
			tmp->flags |= GF_TERM_DRAW_FRAME;
		}
	}

	/*setup scene compositor*/
	tmp->compositor = gf_sc_new(user, !(tmp->flags & GF_TERM_DRAW_FRAME) , tmp);
	if (!tmp->compositor) {
		free(tmp);
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
	tmp->channels_pending = gf_list_new();
	tmp->media_queue = gf_list_new();
	
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
	for (i=0; i< gf_list_count(tmp->extensions); i++) {
		GF_TermExt *ifce = gf_list_get(tmp->extensions, i);
		if (!ifce->process(ifce, tmp, GF_TERM_EXT_START)) {
			gf_list_rem(tmp->extensions, i);
			i--;
		} else if (ifce->caps & GF_TERM_EXT_CAP_NOT_THREADED) {
			gf_list_add(tmp->unthreaded_extensions, ifce);
		}
	}
	if (!gf_list_count(tmp->unthreaded_extensions)) {
		gf_list_del(tmp->unthreaded_extensions);
		tmp->unthreaded_extensions = NULL;
	}

	cf = gf_cfg_get_key(user->config, "General", "GUIFile");
	if (cf) {
		gf_term_connect_from_time_ex(tmp, cf, 0, 0, 1);
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

	/*wait for destroy*/
	e = GF_IO_ERR;
	timeout = 1000;
	while (term->root_scene || gf_list_count(term->net_services) || gf_list_count(term->net_services_to_remove)) {
		gf_sleep(30);
		/*this shouldn't be needed but unfortunately there's a bug hanging around there...*/
		timeout--;
		if (!timeout) break;
	}
	if (timeout) {
		assert(!gf_list_count(term->net_services));
		assert(!gf_list_count(term->net_services_to_remove));
		e = GF_OK;
	} 
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] All network services deleted\n"));

	/*unload extensions*/
	for (i=0; i< gf_list_count(term->extensions); i++) {
		GF_TermExt *ifce = gf_list_get(term->extensions, i);
		ifce->process(ifce, term, GF_TERM_EXT_STOP);
	}

	/*stop the media manager */
	gf_term_stop_scheduler(term);

	/*unload extensions*/
	for (i=0; i< gf_list_count(term->extensions); i++) {
		GF_TermExt *ifce = gf_list_get(term->extensions, i);
		gf_modules_close_interface((GF_BaseInterface *) ifce);
	}
	gf_list_del(term->extensions);
	if (term->unthreaded_extensions) gf_list_del(term->unthreaded_extensions);

	/*delete compositor before the input sensor stacks to avoid receiving events from the compositor
	when destroying these stacks*/
	gf_sc_del(term->compositor);

	gf_list_del(term->net_services);
	gf_list_del(term->net_services_to_remove);
	gf_list_del(term->input_streams);
	gf_list_del(term->x3d_sensors);
	assert(!gf_list_count(term->channels_pending));
	gf_list_del(term->channels_pending);
	assert(!gf_list_count(term->media_queue));
	assert(!term->nodes_pending);
	gf_list_del(term->media_queue);
	if (term->downloader) gf_dm_del(term->downloader);

	if (term->dcci_doc) {
		if (term->dcci_doc->modified) {
			char *pref_file = (char *)gf_cfg_get_key(term->user->config, "General", "EnvironmentFile");
			GF_SceneDumper *dumper = gf_sm_dumper_new(term->dcci_doc, pref_file, ' ', GF_SM_DUMP_AUTO_XML);
			if (!dumper) return GF_IO_ERR;
			e = gf_sm_dump_graph(dumper, 1, 0);
			gf_sm_dumper_del(dumper);
		}
		gf_sg_del(term->dcci_doc);
	}
	gf_mx_del(term->net_mx);

	gf_sys_close();
	free(term);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Terminal destroyed\n"));
	return e;
}


void gf_term_message(GF_Terminal *term, const char *service, const char *message, GF_Err error)
{
	if (!term || !term->user) return;
	GF_USER_MESSAGE(term->user, service, message, error);
}

GF_Err gf_term_step_clocks(GF_Terminal * term, u32 ms_diff)
{
	u32 i, j;
	GF_ClientService *ns;
	/*only play/pause if connected*/
	if (!term || !term->root_scene || !term->root_scene->root_od) return GF_BAD_PARAM;
	if (!term->play_state) return GF_BAD_PARAM;

	gf_sc_lock(term->compositor, 1);
	i=0;
	while ( (ns = (GF_ClientService*)gf_list_enum(term->net_services, &i)) ) {
		GF_Clock *ck;
		j=0;
		while ( (ck = (GF_Clock *)gf_list_enum(ns->Clocks, &j)) ) {
			ck->init_time += ms_diff;
		}
	}
	term->compositor->step_mode = 1;
	term->compositor->draw_next_frame = 1;
	gf_sc_lock(term->compositor, 0);
	return GF_OK;
}

GF_EXPORT
void gf_term_connect_from_time(GF_Terminal * term, const char *URL, u64 startTime, Bool pause_at_first_frame)
{
	gf_term_connect_from_time_ex(term, URL, startTime, pause_at_first_frame, 0);
}

GF_EXPORT
void gf_term_connect(GF_Terminal * term, const char *URL)
{
	gf_term_connect_from_time(term, URL, 0, 0);
}

GF_EXPORT
void gf_term_disconnect(GF_Terminal *term)
{
	if (!term->root_scene) return;
	/*resume*/
	if (term->play_state) gf_term_set_play_state(term, GF_STATE_PLAYING, 1, 1);

	if (term->root_scene->root_od) {
		gf_odm_disconnect(term->root_scene->root_od, 1);
	} else {
		gf_inline_del(term->root_scene);
		term->root_scene = NULL;
	}
	while (term->root_scene || gf_list_count(term->net_services_to_remove)) {
		gf_term_handle_services(term);
		gf_sleep(10);
	}
}

GF_EXPORT
const char *gf_term_get_url(GF_Terminal *term)
{
	if (!term || !term->root_scene) return NULL;
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
		return GF_OK;
	case GF_OPT_MEDIA_CACHE:
		gf_term_set_cache_state(term, value);
		return GF_OK;
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
Double gf_term_get_simulation_frame_rate(GF_Terminal *term)
{
	return term ? term->compositor->frame_rate : 0.0;
}


/*returns 0 if any of the clock still hasn't seen EOS*/
u32 Term_CheckClocks(GF_ClientService *ns, GF_InlineScene *is)
{
	GF_Clock *ck;
	u32 i;
	if (is) {
		GF_ObjectManager *odm;
		if (is->root_od->net_service != ns) {
			if (!Term_CheckClocks(is->root_od->net_service, is)) return 0;
		}
		i=0;
		while ( (odm = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i)) ) {
			if (odm->net_service != ns) {
				if (!Term_CheckClocks(odm->net_service, NULL)) return 0;
			}
		}
	}
	i=0;
	while ( (ck = (GF_Clock *)gf_list_enum(ns->Clocks, &i) ) ) {
		if (!ck->has_seen_eos) return 0;
	}
	return 1;
}

u32 Term_CheckIsOver(GF_Terminal *term)
{
	if (!term->root_scene) return 1;
	/*if input sensors consider the scene runs forever*/
	if (gf_list_count(term->input_streams)) return 0;
	if (gf_list_count(term->x3d_sensors)) return 0;

	/*check no clocks are still running*/
	if (!Term_CheckClocks(term->root_scene->root_od->net_service, term->root_scene)) return 0;
	if (term->root_scene->is_dynamic_scene) return 1;
	/*ask compositor if there are sensors*/
	return gf_sc_get_option(term->compositor, GF_OPT_IS_FINISHED);
}

/*get rendering option*/
GF_EXPORT
u32 gf_term_get_option(GF_Terminal * term, u32 type)
{
	if (!term) return 0;
	switch (type) {
	case GF_OPT_HAS_JAVASCRIPT: return gf_sg_has_scripting();
	case GF_OPT_IS_FINISHED: return Term_CheckIsOver(term);
	case GF_OPT_PLAY_STATE: 
		if (term->compositor->step_mode) return GF_STATE_STEP_PAUSE;
		if (term->root_scene) {
			GF_Clock *ck = term->root_scene->dyn_ck;
			if (!ck) {
				if (!term->root_scene->scene_codec) return GF_STATE_PAUSED;
				ck = term->root_scene->scene_codec->ck;
				if (!ck) return GF_STATE_PAUSED;
			}
			if (ck->Buffering)
				return GF_STATE_STEP_PAUSE;
		}
		if (term->play_state) return GF_STATE_PAUSED;
		return GF_STATE_PLAYING;
	case GF_OPT_MEDIA_CACHE: 
		if (!term->enable_cache) return GF_MEDIA_CACHE_DISABLED;
		else if (term->root_scene && term->root_scene->root_od->net_service->cache) return GF_MEDIA_CACHE_RUNNING;
		else return GF_MEDIA_CACHE_ENABLED;
	case GF_OPT_CAN_SELECT_STREAMS: return (term->root_scene && term->root_scene->is_dynamic_scene) ? 1 : 0;
	default: return gf_sc_get_option(term->compositor, type);
	}
}


GF_EXPORT
GF_Err gf_term_set_size(GF_Terminal * term, u32 NewWidth, u32 NewHeight)
{
	if (!term) return GF_BAD_PARAM;
	return gf_sc_set_size(term->compositor, NewWidth, NewHeight);
}

void gf_term_handle_services(GF_Terminal *term)
{
	GF_ClientService *ns;

	/*we could run into a deadlock if some thread has requested opening of a URL. If we cannot
	grab the network now, we'll do our management at the next cycle*/
	if (!gf_mx_try_lock(term->net_mx))
		return;

	/*play ODs that need it*/
	while (gf_list_count(term->media_queue)) {
		GF_ObjectManager *odm = (GF_ObjectManager *)gf_list_get(term->media_queue, 0);
		gf_list_rem(term->media_queue, 0);
		/*unlock net before sending play/pause*/
		gf_mx_v(term->net_mx);
		/*this is a stop*/
		if (odm->media_start_time == (u64)-1) {
			odm->media_start_time = 0;
			gf_odm_stop(odm, 0);
		} 
		/*this is a play*/
		else {
			gf_odm_play(odm);
		}
	
		/*relock net before sending play/pause*/
		gf_mx_p(term->net_mx);
	}
	gf_mx_v(term->net_mx);

	/*lock to avoid any start attemps from compositor*/
	if (!gf_mx_try_lock(term->compositor->mx)) 
		return;
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

	/*extensions*/
	if (!term->reload_state && term->unthreaded_extensions) {
		u32 i, count;
		count = gf_list_count(term->unthreaded_extensions);
		for (i=0; i<count; i++) {
			GF_TermExt *ifce = gf_list_get(term->unthreaded_extensions, i);
			ifce->process(ifce, term, GF_TERM_EXT_PROCESS);
		}
	}

	
	/*need to reload*/
	if (term->reload_state == 1) {
		term->reload_state = 0;
		gf_term_disconnect(term);
		term->reload_state = 2;
	}
	if (term->reload_state == 2) {
		if (gf_list_count(term->net_services)) return;
		term->reload_state = 0;
		gf_term_connect(term, term->reload_url);
		free(term->reload_url);
		term->reload_url = NULL;
	}
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

void gf_term_close_services(GF_Terminal *term, GF_ClientService *ns)
{
	GF_Err e;

	/*prevent the media manager / term to access the list of services to destroy, otherwise
	we could unload the module while poping its CloseService() call stack which can lead to 
	random crashes (return adresses no longer valid) - cf any "stress mode" playback of a playlist*/
	gf_mx_p(term->net_mx);
	ns->owner = NULL;
	e = ns->ifce->CloseService(ns->ifce);
	/*if error don't wait for ACK to remove from main list*/
	if (e) {
		gf_list_del_item(term->net_services, ns);
		if (gf_list_find(term->net_services_to_remove, ns)<0)
			gf_list_add(term->net_services_to_remove, ns);
	}
	gf_mx_v(term->net_mx);
}

void gf_term_lock_compositor(GF_Terminal *term, Bool LockIt)
{
	gf_sc_lock(term->compositor, LockIt);
}


void gf_term_lock_net(GF_Terminal *term, Bool LockIt)
{
	if (LockIt) {
		gf_mx_p(term->net_mx);
	} else {
		gf_mx_v(term->net_mx);
	}
}

static void mae_collect_info(GF_ClientService *net, GF_ObjectManager *odm, GF_DOMMediaAccessEvent *mae, u32 transport, u32 *min_time, u32 *min_buffer)
{
	u32 i=0;
	GF_Channel *ch;

	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
		u32 val;
		if (ch->service != net) continue;

		mae->bufferValid = 1;
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
		if (mae->nb_streams<20) {
			mae->streams[mae->nb_streams].streamType = ch->esd->decoderConfig->streamType;
			mae->streams[mae->nb_streams].mediaType = ch->esd->decoderConfig->objectTypeIndication;
			mae->streams[mae->nb_streams].transport = transport;
			mae->nb_streams ++;
		}
	}
}

void gf_term_service_media_event(GF_ObjectManager *odm, u32 event_type)
{
#ifndef GPAC_DISABLE_SVG
	u32 i, count, min_buffer, min_time, transport;
	Bool locked;
	GF_DOMMediaAccessEvent mae;
	GF_DOM_Event evt;
	GF_ObjectManager *an_od;
	GF_InlineScene *is;

	if (!odm) return;
	if (odm->mo) {
		count = gf_list_count(odm->mo->nodes);
		if (!count) return;
		if (!(gf_node_get_dom_event_filter(gf_list_get(odm->mo->nodes, 0)) & GF_DOM_EVENT_MEDIA_ACCESS))
			return;
	} else {
		count = 0;
	}


	memset(&mae, 0, sizeof(GF_DOMMediaAccessEvent));
	transport = 0;
	mae.bufferValid = 0;
	mae.session_name = odm->net_service->url;
	if (!strnicmp(mae.session_name, "rtsp:", 5) 
		|| !strnicmp(mae.session_name, "sdp:", 4)
		|| !strnicmp(mae.session_name, "rtp:", 4)
	) 
		transport = 1;
	else if (!strnicmp(mae.session_name, "dvb:", 4)) 
		transport = 2;

	min_time = min_buffer = (u32) -1;
	is = odm->subscene ? odm->subscene : odm->parentscene;
	/*get buffering on root OD*/
	mae_collect_info(odm->net_service, is->root_od, &mae, transport, &min_time, &min_buffer);
	/*get buffering on all ODs*/
	i=0;
	while ((an_od = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i))) {
		mae_collect_info(odm->net_service, an_od, &mae, transport, &min_time, &min_buffer);
	}

	mae.level = min_buffer;
	mae.remaining_time = INT2FIX(min_time) / 60;
	mae.status = 0;

	memset(&evt, 0, sizeof(GF_DOM_Event));
	evt.mae = &mae;
	evt.type = event_type;
	evt.bubbles = 0;	/*the spec says yes but we force it to NO*/

	/*lock scene to prevent concurrent access of scene data*/
	locked = gf_mx_try_lock(odm->term->compositor->mx);
	for (i=0; i<count; i++) {
		GF_Node *node = gf_list_get(odm->mo->nodes, i);
		gf_dom_event_fire(node, &evt);
	}
	if (!count) {
		GF_Node *root = gf_sg_get_root_node(is->graph);
		if (root) gf_dom_event_fire(root, &evt);
	}
	if (locked) gf_sc_lock(odm->term->compositor, 0);
#endif
}


/*connects given OD manager to its URL*/
void gf_term_connect_object(GF_Terminal *term, GF_ObjectManager *odm, char *serviceURL, GF_ClientService *ParentService)
{
	GF_ClientService *ns;
	u32 i;
	GF_Err e;
	gf_term_lock_net(term, 1);

	/*for remoteODs/dynamic ODs, check if one of the running service cannot be used*/
	i=0;
	while ( (ns = (GF_ClientService*)gf_list_enum(term->net_services, &i)) ) {
		if (gf_term_service_can_handle_url(ns, serviceURL)) {
			gf_term_lock_net(term, 0);

			/*wait for service to setup - service may become destroyed if not available*/
			while (1) {
				gf_term_lock_net(term, 1);
				if (!ns->owner) {
					gf_term_lock_net(term, 0);
					return;
				}
				gf_term_lock_net(term, 0);
				if (ns->owner->OD) break;
				gf_sleep(5);
			}
			odm->net_service = ns;
			gf_odm_setup_entry_point(odm, serviceURL);
			return;
		}
	}

	odm->net_service = gf_term_service_new(term, odm, serviceURL, ParentService, &e);
	if (!odm->net_service) {
		gf_term_lock_net(term, 0);
		gf_term_message(term, serviceURL, "Cannot open service", e);
		gf_odm_disconnect(odm, 1);
		return;
	}
	gf_term_lock_net(term, 0);

	/*OK connect*/
	gf_term_service_media_event(odm, GF_EVENT_MEDIA_BEGIN_SESSION_SETUP);
	odm->net_service->ifce->ConnectService(odm->net_service->ifce, odm->net_service, odm->net_service->url);
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
	ns = gf_term_service_new(term, NULL, URL, ch->odm->net_service, &e);
	if (!ns) return e;
	ch->service = ns;
	ns->ifce->ConnectService(ns->ifce, ns, ns->url);

	gf_term_lock_net(term, 0);
	return GF_OK;
}

GF_EXPORT
u32 gf_term_play_from_time(GF_Terminal *term, u64 from_time, Bool pause_at_first_frame)
{
	if (!term || !term->root_scene || !term->root_scene->root_od) return 0;
	if (term->root_scene->root_od->flags & GF_ODM_NO_TIME_CTRL) return 1;

	/*for dynamic scene OD ressources are static and all object use the same clock, so don't restart the root 
	OD, just act as a mediaControl on all playing streams*/
	if (term->root_scene->is_dynamic_scene) {
		/*exit pause mode*/
		gf_term_set_play_state(term, GF_STATE_PLAYING, 1, 1);

		if (pause_at_first_frame)
			gf_term_set_play_state(term, GF_STATE_STEP_PAUSE, 0, 0);

		gf_sc_lock(term->compositor, 1);
		gf_inline_restart_dynamic(term->root_scene, from_time);
		gf_sc_lock(term->compositor, 0);
		return 2;
	} 

	/*pause everything*/
	gf_term_set_play_state(term, GF_STATE_PAUSED, 0, 1);
	/*stop root*/
	gf_odm_stop(term->root_scene->root_od, 1);
	gf_inline_disconnect(term->root_scene, 0);
	/*make sure we don't have OD queued*/
	while (gf_list_count(term->media_queue)) gf_list_rem(term->media_queue, 0);
	term->root_scene->root_od->media_start_time = from_time;

	gf_odm_start(term->root_scene->root_od);
	gf_term_set_play_state(term, pause_at_first_frame ? GF_STATE_STEP_PAUSE : GF_STATE_PLAYING, 0, 1);
	return 2;
}


GF_EXPORT
Bool gf_term_user_event(GF_Terminal * term, GF_Event *evt)
{
	if (term) return gf_sc_user_event(term->compositor, evt);
	return 0;
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
	if (!term || !term->root_scene) return 0;
	if (term->root_scene->scene_codec && term->root_scene->scene_codec->ck) return gf_clock_ellapse_time(term->root_scene->scene_codec->ck);
	else if (term->root_scene->dyn_ck) return gf_clock_ellapse_time(term->root_scene->dyn_ck);
	return 0;
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
	if (term->reload_url) free(term->reload_url);
	term->reload_url = NULL;
	if (term->root_scene) 
		term->reload_url = gf_url_concatenate(term->root_scene->root_od->net_service->url, toURL);
	if (!term->reload_url) term->reload_url = strdup(toURL);
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

	sfurl.OD_ID = GF_ESM_DYNAMIC_OD_ID;
	sfurl.url = (char *) url;
	mfurl.count = 1;
	mfurl.vals = &sfurl;
	/*only text tracks are supported for now...*/
	mo = gf_inline_get_media_object(term->root_scene, &mfurl, GF_MEDIA_OBJECT_TEXT, 1);
	if (mo) {
		/*check if we must deactivate it*/
		if (mo->odm) {
			if (mo->num_open && !auto_play) {
				gf_inline_select_object(term->root_scene, mo->odm);
			} else {
				mo->odm->OD_PL = auto_play ? 1 : 0;
			}
		} else {
			gf_list_del_item(term->root_scene->media_objects, mo);
			gf_sg_vrml_mf_reset(&mo->URLs, GF_SG_VRML_MFURL);
			free(mo);
			mo = NULL;
		}
	}
	return mo ? GF_OK : GF_NOT_SUPPORTED;
}


void gf_term_attach_service(GF_Terminal *term, GF_InputService *service_hdl)
{
	Bool net_check_interface(GF_InputService *ifce);
	GF_InlineScene *is;
	GF_ObjectManager *odm;
	if (!net_check_interface(service_hdl)) return;

	if (term->root_scene) gf_term_disconnect(term);

	gf_term_lock_net(term, 1);
	
	/*create a new scene*/
	is = gf_inline_new(NULL);
	odm = gf_odm_new();
	gf_sg_set_script_action(is->graph, term_script_action, term);

	is->root_od = odm;
	term->root_scene = is;
	odm->parentscene = NULL;
	odm->subscene = is;
	odm->term = term;

	GF_SAFEALLOC(odm->net_service , GF_ClientService);
	odm->net_service->term = term;
	odm->net_service->owner = odm;
	odm->net_service->ifce = service_hdl;
	odm->net_service->url = strdup("Internal Service Handler");
	odm->net_service->Clocks = gf_list_new();
	gf_list_add(term->net_services, odm->net_service);

	gf_term_lock_net(term, 0);

	/*OK connect*/
	odm->net_service->ifce->ConnectService(odm->net_service->ifce, odm->net_service, odm->net_service->url);
}

GF_EXPORT
GF_Err gf_term_scene_update(GF_Terminal *term, char *type, char *com)
{
	GF_Err e;
	GF_StreamContext *sc;
	GF_ESD *esd;
	Bool is_xml = 0;
	Double time = 0;
	u32 i, tag;
	GF_SceneLoader load;

	if (!term) return GF_BAD_PARAM;

	memset(&load, 0, sizeof(GF_SceneLoader));
	load.localPath = gf_cfg_get_key(term->user->config, "General", "CacheDirectory");
	load.flags = GF_SM_LOAD_FOR_PLAYBACK | GF_SM_LOAD_CONTEXT_READY;
	load.type = GF_SM_LOAD_BT;

	if (!term->root_scene) {
		gf_term_lock_net(term, 1);
		/*create a new scene*/
		term->root_scene = gf_inline_new(NULL);
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
	time = gf_inline_get_time(term->root_scene);


	if (type && (!stricmp(type, "application/x-laser+xml") || !stricmp(type, "laser"))) {
		load.type = GF_SM_LOAD_XSR;
		time = gf_inline_get_time(term->root_scene);
	}
	else if (type && (!stricmp(type, "image/svg+xml") || !stricmp(type, "svg")) ) {
		load.type = GF_SM_LOAD_XSR;
		time = gf_inline_get_time(term->root_scene);
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
			time = gf_inline_get_time(term->root_scene);
		} else if (tag>=GF_NODE_RANGE_FIRST_X3D) {
			load.type = is_xml ? GF_SM_LOAD_X3D : GF_SM_LOAD_X3DV;
		} else {
			load.type = is_xml ? GF_SM_LOAD_XMTA : GF_SM_LOAD_BT;
			time = gf_inline_get_time(term->root_scene);
		}
	}

	e = gf_sm_load_string(&load, com, 1);
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
		gf_inline_attach_to_compositor(term->root_scene);
	}
	gf_sm_del(load.ctx);
	return e;
}

GF_Err gf_term_get_screen_buffer(GF_Terminal *term, GF_VideoSurface *framebuffer)
{
	if (!term) return GF_BAD_PARAM;
	return gf_sc_get_screen_buffer(term->compositor, framebuffer, 0);
}

GF_Err gf_term_release_screen_buffer(GF_Terminal *term, GF_VideoSurface *framebuffer)
{
	if (!term) return GF_BAD_PARAM;
	return gf_sc_release_screen_buffer(term->compositor, framebuffer);
}


static void gf_term_sample_scenetime(GF_InlineScene *scene)
{
	u32 i, count;
	gf_inline_sample_time(scene);
	count = gf_list_count(scene->ODlist);
	for (i=0; i<count; i++) {
		GF_ObjectManager *odm = gf_list_get(scene->ODlist, i);
		if (odm->subscene) gf_term_sample_scenetime(odm->subscene);
	}
}

void gf_term_sample_clocks(GF_Terminal *term)
{
	gf_term_sample_scenetime(term->root_scene);
}

const char *gf_term_get_text_selection(GF_Terminal *term, Bool probe_only)
{
	Bool has_text;
	if (!term) return NULL;
	has_text = gf_sc_has_text_selection(term->compositor);
	if (!has_text) return NULL;
	if (probe_only) return "";
	return gf_sc_get_selected_text(term->compositor);
}


GF_Err gf_term_paste_text(GF_Terminal *term, const char *txt, Bool probe_only)
{
	if (!term) return GF_BAD_PARAM;
	if (probe_only) return gf_sc_paste_text(term->compositor, NULL);
	return gf_sc_paste_text(term->compositor, txt);
}


