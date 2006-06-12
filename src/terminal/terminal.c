
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
#include <gpac/internal/renderer_dev.h>
#include <gpac/constants.h>
#include <gpac/options.h>

u32 gf_term_get_time(GF_Terminal *term)
{
	assert(term);
	return gf_sr_get_clock(term->renderer);
}

void gf_term_invalidate_renderer(GF_Terminal *term)
{
	gf_sr_invalidate(term->renderer, NULL);
}

static Bool check_user(GF_User *user)
{
	if (!user->config) return 0;
	if (!user->modules) return 0;
	if (!user->opaque) return 0;
	return 1;
}

/*script API*/
typedef struct
{
	GF_DownloadSession * sess;
	void (*OnDone)(void *cbck, Bool success, const char *the_file_path);
	void *cbk;
} JSDownload;

static void JS_OnData(void *cbck, char *data, u32 data_size, u32 state, GF_Err e)
{
	JSDownload *jsdnload = (JSDownload *)cbck;
	if (e<GF_OK) {
		jsdnload->OnDone(jsdnload->cbk, 0, NULL);
		gf_term_download_del(jsdnload->sess);
		free(jsdnload);
	}
	else if (e==GF_EOS) {
		const char *szCache = gf_dm_sess_get_cache_name(jsdnload->sess);
		jsdnload->OnDone(jsdnload->cbk, 1, szCache);
		gf_term_download_del(jsdnload->sess);
		free(jsdnload);
	}
}

static Bool OnJSGetScriptFile(void *opaque, GF_SceneGraph *parent_graph, const char *url, void (*OnDone)(void *cbck, Bool success, const char *the_file_path), void *cbk)
{
	JSDownload *jsdnload;
	GF_InlineScene *is;
	if (!parent_graph || !OnDone) return 0;
	is = gf_sg_get_private(parent_graph);
	if (!is) return 0;
	GF_SAFEALLOC(jsdnload, sizeof(JSDownload));
	jsdnload->OnDone = OnDone;
	jsdnload->cbk = cbk;
	jsdnload->sess = gf_term_download_new(is->root_od->net_service, url, 0, JS_OnData, jsdnload);
	if (!jsdnload->sess) {
		free(jsdnload);
		OnDone(cbk, 0, NULL);
		return 0;
	}
	return 1;
}

static Bool OnJSAction(void *opaque, u32 type, GF_Node *n, GF_JSAPIParam *param)
{
	Bool ret;
	GF_Terminal *term = (GF_Terminal *) opaque;

	if (type==GF_JSAPI_OP_GET_OPT) {
		param->gpac_cfg.key_val = gf_cfg_get_key(term->user->config, param->gpac_cfg.section, param->gpac_cfg.key);
		return 1;
	}
	if (type==GF_JSAPI_OP_SET_OPT) {
		gf_cfg_set_key(term->user->config, param->gpac_cfg.section, param->gpac_cfg.key, param->gpac_cfg.key_val);
		return 1;
	}

	if (type==GF_JSAPI_OP_GET_SCENE_URI) {
		GF_InlineScene *is = gf_sg_get_private(gf_node_get_graph(n));
		param->uri.url = is->root_od->net_service->url;
		param->uri.nb_params = 0;
		return 1;
	}
	ret = 0;
	if (term->renderer->visual_renderer->ScriptAction)
		ret = term->renderer->visual_renderer->ScriptAction(term->renderer->visual_renderer, type, n, param);

	if (ret) return ret;
	if (type==GF_JSAPI_OP_LOAD_URL) {
		if (gf_sg_get_private(gf_node_get_graph(n)) == term->root_scene) {
			GF_Event evt;
			if (!term->user->EventProc) return 0;
			evt.type = GF_EVT_NAVIGATE;
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
static void OnJSMessage(void *opaque, GF_Err e, const char *msg)
{
	GF_Terminal *term = (GF_Terminal *) opaque;
	gf_term_message(term, term->root_scene->root_od->net_service->url, msg, e);
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
		term->bifs_can_resync = 0;
	else
		term->bifs_can_resync = 1;

	sOpt = gf_cfg_get_key(term->user->config, "Systems", "ForceSingleClock");
	if (sOpt && !stricmp(sOpt, "yes")) 
		term->force_single_clock = 1;
	else
		term->force_single_clock = 0;

	sOpt = gf_cfg_get_key(term->user->config, "Rendering", "FrameRate");
	if (sOpt) {
		fps = atof(sOpt);
		if (term->system_fps != fps) {
			term->system_fps = fps;
			term->half_frame_duration = (u32) (500/fps);
			gf_sr_set_fps(term->renderer, fps);
		}
	}

	if (term->user->init_flags & GF_TERM_INIT_NOT_THREADED){
		gf_mm_set_threading(term->mediaman, 1);
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
		gf_mm_set_priority(term->mediaman, prio);

		sOpt = gf_cfg_get_key(term->user->config, "Systems", "ThreadingPolicy");
		if (sOpt) {
			mode = 0;
			if (!stricmp(sOpt, "Single")) mode = 1;
			else if (!stricmp(sOpt, "Multi")) mode = 2;
			gf_mm_set_threading(term->mediaman, mode);
		}
	}

	/*default data timeout is 20 sec*/
	term->net_data_timeout = 20000;
	sOpt = gf_cfg_get_key(term->user->config, "Network", "DataTimeout");
	if (sOpt) term->net_data_timeout = atoi(sOpt);

	if (term->root_scene) gf_is_set_duration(term->root_scene);
	/*reload renderer config*/
	gf_sr_set_option(term->renderer, GF_OPT_RELOAD_CONFIG, 1);
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

GF_Terminal *gf_term_new(GF_User *user)
{
	GF_Terminal *tmp;
	const char *cf;
	if (!check_user(user)) return NULL;

	tmp = malloc(sizeof(GF_Terminal));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_Terminal));

	/*just for safety*/
	gf_sys_init();

	tmp->user = user;
	tmp->js_ifce.callback = tmp;
	tmp->js_ifce.ScriptMessage = OnJSMessage;
	tmp->js_ifce.ScriptAction = OnJSAction;
	tmp->js_ifce.GetScriptFile = OnJSGetScriptFile;

	/*this is not changeable at runtime*/
	if (user->init_flags & GF_TERM_INIT_NOT_THREADED) {
		tmp->render_frames = 2;
		user->init_flags |= GF_TERM_INIT_NO_AUDIO;
	} else {
		cf = gf_cfg_get_key(user->config, "Systems", "NoVisualThread");
		if (!cf || !stricmp(cf, "no")) {
			tmp->render_frames = 0;
		} else {
			tmp->render_frames = 1;
		}
	}

	/*setup scene renderer*/
	tmp->renderer = gf_sr_new(user, !tmp->render_frames, tmp);
	if (!tmp->renderer) {
		free(tmp);
		return NULL;
	}
	tmp->system_fps = 30.0;
	gf_sr_set_fps(tmp->renderer, tmp->system_fps);
	tmp->half_frame_duration = (u32) (500/tmp->system_fps);

	tmp->downloader = gf_dm_new(user->config);
	gf_dm_set_auth_callback(tmp->downloader, gf_term_get_user_pass, tmp);

	tmp->net_services = gf_list_new();
	tmp->net_services_to_remove = gf_list_new();
	tmp->channels_pending = gf_list_new();
	tmp->od_pending = gf_list_new();
	
	tmp->net_mx = gf_mx_new();
	tmp->input_streams = gf_list_new();
	tmp->x3d_sensors = gf_list_new();

	/*mode is changed when reloading cfg*/
	tmp->mediaman = gf_mm_new(tmp, GF_TERM_THREAD_FREE);
	gf_term_reload_cfg(tmp);
	return tmp;
}

GF_Err gf_term_del(GF_Terminal * term)
{
	GF_Err e;
	u32 timeout;

	if (!term) return GF_BAD_PARAM;

	/*disconnect main scene from the renderer*/
	gf_sr_set_scene(term->renderer, NULL);

	/*close main service*/
	gf_term_disconnect(term);

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

	/*stop the media manager */
	gf_mm_del(term->mediaman);

	/*delete renderer before the input sensor stacks to avoid recieving events from the renderer
	when destroying these stacks*/
	gf_sr_del(term->renderer);

	gf_list_del(term->net_services);
	gf_list_del(term->net_services_to_remove);
	gf_list_del(term->input_streams);
	gf_list_del(term->x3d_sensors);
	assert(!gf_list_count(term->channels_pending));
	gf_list_del(term->channels_pending);
	assert(!gf_list_count(term->od_pending));
	assert(!term->nodes_pending);
	gf_list_del(term->od_pending);
	if (term->downloader) gf_dm_del(term->downloader);
	gf_mx_del(term->net_mx);
	gf_sys_close();
	free(term);
	return e;
}


void gf_term_message(GF_Terminal *term, const char *service, const char *message, GF_Err error)
{
	if (!term || !term->user) return;
	GF_USER_MESSAGE(term->user, service, message, error);
}

static void gf_term_set_play_state(GF_Terminal *term, u32 PlayState, Bool reset_audio, Bool pause_clocks)
{
	u32 i, j;
	GF_ClientService *ns;
	/*only play/pause if connected*/
	if (!term || !term->root_scene) return;
	/*and if not already paused/playing*/
	if (!term->play_state && !PlayState) return;
	if (term->play_state && (PlayState==GF_STATE_PAUSED)) return;

	/*pause renderer*/
	if ((PlayState==GF_STATE_PLAYING) && reset_audio)
		gf_sr_set_option(term->renderer, GF_OPT_PLAY_STATE, 0xFF);
	else
		gf_sr_set_option(term->renderer, GF_OPT_PLAY_STATE, PlayState);

	if (PlayState==GF_STATE_STEP_PAUSE) PlayState = term->play_state ? GF_STATE_PLAYING : GF_STATE_PAUSED;
	if (term->play_state == PlayState) return;
	term->play_state = PlayState;

	if (!pause_clocks) return;

	/*pause all clocks on all services*/
	i=0;
	while ( (ns = gf_list_enum(term->net_services, &i)) ) {
		GF_Clock *ck;
		j=0;
		while ( (ck = gf_list_enum(ns->Clocks, &j)) ) {
			if (PlayState) gf_clock_pause(ck);
			else gf_clock_resume(ck);
		}
	}
}

GF_Err gf_term_step_clocks(GF_Terminal * term, u32 ms_diff)
{
	u32 i, j;
	GF_ClientService *ns;
	/*only play/pause if connected*/
	if (!term || !term->root_scene || !term->root_scene->root_od) return GF_BAD_PARAM;
	if (!term->play_state) return GF_BAD_PARAM;

	gf_sr_lock(term->renderer, 1);
	i=0;
	while ( (ns = gf_list_enum(term->net_services, &i)) ) {
		GF_Clock *ck;
		j=0;
		while ( (ck = gf_list_enum(ns->Clocks, &j)) ) {
			ck->init_time += ms_diff;
		}
	}
	term->renderer->step_mode = 1;
	term->renderer->draw_next_frame = 1;
	gf_sr_lock(term->renderer, 0);
	return GF_OK;
}

void gf_term_connect_from_time(GF_Terminal * term, const char *URL, u32 startTime, Bool pause_at_first_frame)
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

	gf_term_lock_net(term, 1);
	/*create a new scene*/
	is = gf_is_new(NULL);
	odm = gf_odm_new();
	gf_sg_set_javascript_api(is->graph, &term->js_ifce);

	is->root_od = odm;
	term->root_scene = is;
	odm->parentscene = NULL;
	odm->subscene = is;
	odm->term = term;
	gf_term_lock_net(term, 0);

	odm->media_start_time = (u32) startTime;
	/*render first visual frame and pause*/
	if (pause_at_first_frame)
		gf_term_set_play_state(term, GF_STATE_STEP_PAUSE, 0, 0);
	/*connect - we don't have any parentID */
	gf_term_connect_object(term, odm, (char *) URL, NULL);
}

void gf_term_connect(GF_Terminal * term, const char *URL)
{
	gf_term_connect_from_time(term, URL, 0, 0);
}

void gf_term_disconnect(GF_Terminal *term)
{
	if (!term->root_scene) return;
	/*resume*/
	if (term->play_state) gf_term_set_play_state(term, GF_STATE_PLAYING, 1, 1);

	gf_sr_set_scene(term->renderer, NULL);
	gf_odm_disconnect(term->root_scene->root_od, 1);
	while (term->root_scene || gf_list_count(term->net_services_to_remove)) {
		gf_term_handle_services(term);
		gf_sleep(10);
	}
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
		return gf_sr_set_option(term->renderer, type, value);
	}
}

GF_Err gf_term_set_simulation_frame_rate(GF_Terminal * term, Double frame_rate)
{
	if (!term) return GF_BAD_PARAM;
	term->system_fps = frame_rate;
	term->half_frame_duration = (u32) (500.0 / term->system_fps);
	gf_sr_set_fps(term->renderer, term->system_fps);
	return GF_OK;
}

Double gf_term_get_simulation_frame_rate(GF_Terminal *term)
{
	return term ? term->system_fps : 0.0;
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
		while ( (odm = gf_list_enum(is->ODlist, &i)) ) {
			if (odm->net_service != ns) {
				while (odm->remote_OD) odm = odm->remote_OD;
				if (!Term_CheckClocks(odm->net_service, NULL)) return 0;
			}
		}
	}
	i=0;
	while ( (ck = gf_list_enum(ns->Clocks, &i) ) ) {
		if (!ck->has_seen_eos) return 0;
	}
	return 1;
}

u32 Term_CheckIsOver(GF_Terminal *term)
{
	GF_ObjectManager *odm;
	if (!term->root_scene) return 1;
	/*if input sensors consider the scene runs forever*/
	if (gf_list_count(term->input_streams)) return 0;
	if (gf_list_count(term->x3d_sensors)) return 0;
	odm = term->root_scene->root_od;
	while (odm->remote_OD) odm = odm->remote_OD;
	/*check no clocks are still running*/
	if (!Term_CheckClocks(odm->net_service, term->root_scene)) return 0;
	if (term->root_scene->is_dynamic_scene) return 1;
	/*ask renderer if there are sensors*/
	return gf_sr_get_option(term->renderer, GF_OPT_IS_FINISHED);
}

/*get rendering option*/
u32 gf_term_get_option(GF_Terminal * term, u32 type)
{
	if (!term) return 0;
	switch (type) {
	case GF_OPT_HAS_JAVASCRIPT: return gf_sg_has_scripting();
	case GF_OPT_IS_FINISHED: return Term_CheckIsOver(term);
	case GF_OPT_PLAY_STATE: 
		if (term->renderer->step_mode) return GF_STATE_STEP_PAUSE;
		if (term->play_state) return GF_STATE_PAUSED;
		return GF_STATE_PLAYING;
	case GF_OPT_MEDIA_CACHE: 
		if (!term->enable_cache) return GF_MEDIA_CACHE_DISABLED;
		else if (term->root_scene && term->root_scene->root_od->net_service->cache) return GF_MEDIA_CACHE_RUNNING;
		else return GF_MEDIA_CACHE_ENABLED;
	case GF_OPT_CAN_SELECT_STREAMS: return (term->root_scene && term->root_scene->is_dynamic_scene) ? 1 : 0;
	default: return gf_sr_get_option(term->renderer, type);
	}
}


GF_Err gf_term_set_size(GF_Terminal * term, u32 NewWidth, u32 NewHeight)
{
	if (!term) return 0;
	return gf_sr_set_size(term->renderer, NewWidth, NewHeight);
}


void gf_term_handle_services(GF_Terminal *term)
{
	GF_ClientService *ns;

	/*play ODs that need it*/
	gf_mx_p(term->net_mx);
	while (gf_list_count(term->od_pending)) {
		GF_ObjectManager *odm = gf_list_get(term->od_pending, 0);
		gf_list_rem(term->od_pending, 0);
		/*this is a stop*/
		if (odm->media_start_time == -1) {
			odm->media_start_time = 0;
			gf_odm_stop(odm, 0);
		} 
		/*this is a play*/
		else {
			gf_odm_play(odm);
		}
	}
	gf_mx_v(term->net_mx);

	/*lock to avoid any start attemps from renderer*/
	gf_sr_lock(term->renderer, 1);
	while (gf_list_count(term->net_services_to_remove)) {
		gf_mx_p(term->net_mx);
		ns = gf_list_get(term->net_services_to_remove, 0);
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
			GF_Node *n = gf_list_get(term->nodes_pending, i);
			gf_node_render(n, NULL);
			if (!term->nodes_pending) break;
			n_count = gf_list_count(term->nodes_pending);
			if (n_count==count) i++;
			else count=n_count;
		}
	}
	gf_sr_lock(term->renderer, 0);

	
	/*need to reload*/
	if (term->reload_state == 1) {
		term->reload_state = 2;
		gf_term_disconnect(term);
	}
	if (term->reload_state == 2) {
		term->reload_state = 0;
		gf_term_connect(term, term->reload_url);
		free(term->reload_url);
		term->reload_url = NULL;
	}
}

void gf_term_add_render_node(GF_Terminal *term, GF_Node *node)
{
	gf_sr_lock(term->renderer, 1);
	if (!term->nodes_pending) term->nodes_pending = gf_list_new();
	gf_list_add(term->nodes_pending, node);
	gf_sr_lock(term->renderer, 0);
}
void gf_term_rem_render_node(GF_Terminal *term, GF_Node *node)
{
	gf_sr_lock(term->renderer, 1);
	if (term->nodes_pending) {
		gf_list_del_item(term->nodes_pending, node);
		if (!gf_list_count(term->nodes_pending)) {
			gf_list_del(term->nodes_pending);
			term->nodes_pending = NULL;
		}
	}
	gf_sr_lock(term->renderer, 0);
}

void gf_term_close_services(GF_Terminal *term, GF_ClientService *ns)
{
	GF_Err e = ns->ifce->CloseService(ns->ifce);
	ns->owner = NULL;
	/*if error don't wait for ACK to remove from main list*/
	if (e) {
		gf_list_del_item(term->net_services, ns);
		gf_list_add(term->net_services_to_remove, ns);
	}
}

void gf_term_lock_renderer(GF_Terminal *term, Bool LockIt)
{
	gf_sr_lock(term->renderer, LockIt);
}

void gf_term_lock_net(GF_Terminal *term, Bool LockIt)
{
	if (LockIt) {
		gf_mx_p(term->net_mx);
	} else {
		gf_mx_v(term->net_mx);
	}
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
	while ( (ns = gf_list_enum(term->net_services, &i)) ) {
		if (gf_term_service_can_handle_url(ns, serviceURL)) {
			odm->net_service = ns;
			gf_odm_setup_entry_point(odm, serviceURL);
			gf_term_lock_net(term, 0);
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
	while ( (ns = gf_list_enum(term->net_services, &i)) ) {
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

Bool gf_term_play_from_time(GF_Terminal *term, u32 from_time, Bool pause_at_first_frame)
{
	if (!term || !term->root_scene || !term->root_scene->root_od) return 0;
	if (term->root_scene->root_od->no_time_ctrl) return 1;

	/*for dynamic scene OD ressources are static and all object use the same clock, so don't restart the root 
	OD, just act as a mediaControl on all playing streams*/
	if (term->root_scene->is_dynamic_scene) {
		/*exit pause mode*/
		gf_term_set_play_state(term, GF_STATE_PLAYING, 1, 1);

		if (pause_at_first_frame)
			gf_term_set_play_state(term, GF_STATE_STEP_PAUSE, 0, 0);

		gf_sr_lock(term->renderer, 1);
		gf_is_restart_dynamic(term->root_scene, from_time);
		gf_sr_lock(term->renderer, 0);
		return 2;
	} 

	/*pause everything*/
	gf_term_set_play_state(term, GF_STATE_PAUSED, 0, 1);
	gf_sr_lock(term->renderer, 1);
	gf_sr_set_scene(term->renderer, NULL);
	gf_sr_lock(term->renderer, 0);
	/*stop root*/
	gf_odm_stop(term->root_scene->root_od, 1);
	gf_is_disconnect(term->root_scene, 0);
	/*make sure we don't have OD queued*/
	while (gf_list_count(term->od_pending)) gf_list_rem(term->od_pending, 0);
	term->root_scene->root_od->media_start_time = from_time;

	gf_odm_start(term->root_scene->root_od);
	gf_term_set_play_state(term, pause_at_first_frame ? GF_STATE_STEP_PAUSE : GF_STATE_PLAYING, 0, 1);
	return 2;
}


void gf_term_user_event(GF_Terminal * term, GF_Event *evt)
{
	if (term) gf_sr_user_event(term->renderer, evt);
}


Double gf_term_get_framerate(GF_Terminal *term, Bool absoluteFPS)
{
	if (!term || !term->renderer) return 0;
	return gf_sr_get_fps(term->renderer, absoluteFPS);
}

/*get main scene current time in sec*/
u32 gf_term_get_time_in_ms(GF_Terminal *term)
{
	if (!term || !term->root_scene) return 0;
	if (term->root_scene->scene_codec) return gf_clock_time(term->root_scene->scene_codec->ck);
	else if (term->root_scene->is_dynamic_scene) return gf_clock_time(term->root_scene->dyn_ck);
	return 0;
}

GF_Node *gf_term_pick_node(GF_Terminal *term, s32 X, s32 Y)
{
	if (!term || !term->renderer) return NULL;
	return gf_sr_pick_node(term->renderer, X, Y);
}

void gf_term_reload(GF_Terminal *term)
{
	if (!term->root_scene) return;
	if (term->reload_url) free(term->reload_url);
	term->reload_url = strdup(term->root_scene->root_od->net_service->url);
	term->reload_state = 1;
}

GF_Err gf_term_get_viewpoint(GF_Terminal *term, u32 viewpoint_idx, const char **outName, Bool *is_bound)
{
	return gf_sr_get_viewpoint(term->renderer, viewpoint_idx, outName, is_bound);
}

GF_Err gf_term_set_viewpoint(GF_Terminal *term, u32 viewpoint_idx, const char *viewpoint_name)
{
	return gf_sr_set_viewpoint(term->renderer, viewpoint_idx, viewpoint_name);
}

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
	mo = gf_is_get_media_object(term->root_scene, &mfurl, GF_MEDIA_OBJECT_TEXT);
	if (mo) {
		/*check if we must deactivate it*/
		if (mo->odm) {
			if (mo->num_open && !auto_play) {
				gf_is_select_object(term->root_scene, mo->odm);
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
	is = gf_is_new(NULL);
	odm = gf_odm_new();
	gf_sg_set_javascript_api(is->graph, &term->js_ifce);

	is->root_od = odm;
	term->root_scene = is;
	odm->parentscene = NULL;
	odm->subscene = is;
	odm->term = term;

	odm->net_service = malloc(sizeof(GF_ClientService));
	memset(odm->net_service, 0, sizeof(GF_ClientService));
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

