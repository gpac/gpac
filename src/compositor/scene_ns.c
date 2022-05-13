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
#include <gpac/network.h>


Bool scene_ns_on_setup_error(GF_Filter *failed_filter, void *udta, GF_Err err)
{
	GF_SceneNamespace *scene_ns;
	GF_Scene *scene;
	GF_ObjectManager *root = (GF_ObjectManager *)udta;
	assert(root);
	scene_ns = root->scene_ns;
	assert(scene_ns);
	scene = root->subscene ? root->subscene : root->parentscene;
	assert(scene);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[ODM] Service connection failure received from %s - %s\n", scene_ns->url, gf_error_to_string(err) ));

	if (root->scene_ns->owner != root) {
		gf_scene_message(scene, scene_ns->url, "Incompatible module type", GF_SERVICE_ERROR);
		return GF_FALSE;
	}
	/*this is service connection*/
	gf_odm_service_media_event(root, GF_EVENT_MEDIA_SETUP_DONE);

	scene_ns->connect_ack = GF_TRUE;
	if (err) {
		char msg[5000];
		snprintf(msg, sizeof(msg), "Cannot open %s", scene_ns->url);
		gf_scene_message(scene, scene_ns->url, msg, err);

		if (root->mo) root->mo->connect_state = MO_CONNECT_FAILED;
		gf_odm_service_media_event(root, GF_EVENT_ERROR);

		/*destroy service only if attached*/
		if (root) {
			GF_Scene *top_scene;
			//notify before disconnecting
			if (root->subscene) gf_scene_notify_event(root->subscene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, err, GF_FALSE);

			//detach clocks before destroying namspace
			root->ck = NULL;
			if (root->subscene) {
				u32 i, count = gf_list_count(root->subscene->resources);
				for (i=0; i<count; i++) {
					GF_ObjectManager *anodm = gf_list_get(root->subscene->resources, i);
					anodm->ck = NULL;
					if (anodm->scene_ns==root->scene_ns)
					 	anodm->scene_ns = NULL;
				}
			}

			root->scene_ns = NULL;
			if (scene_ns->owner && scene_ns->nb_odm_users) scene_ns->nb_odm_users--;
			scene_ns->owner = NULL;

			top_scene = gf_scene_get_root_scene(scene);

			//source filter no longer exists
			scene_ns->source_filter = NULL;
			gf_scene_ns_del(scene_ns, top_scene);

			if (!root->parentscene) {
				GF_Event evt;
				evt.type = GF_EVENT_CONNECT;
				evt.connect.is_connected = 0;
				gf_filter_send_gf_event(scene->compositor->filter, &evt);
			} else {
				/*try to reinsert OD for VRML/X3D with multiple URLs:
				1- first remove from parent scene without destroying object, this will trigger a re-setup
					if other URLs are present
					2- then destroy object*/
				gf_scene_remove_object(root->parentscene, root, 0);
				gf_odm_disconnect(root, 1);
			}
			return GF_FALSE;
		}
	}
	return GF_FALSE;
}


void gf_scene_insert_pid(GF_Scene *scene, GF_SceneNamespace *sns, GF_FilterPid *pid, Bool is_in_iod)
{
	u32 i, min_od_id;
	GF_MediaObject *the_mo;
	GF_ObjectManager *odm, *root;
	const GF_PropertyValue *v;
	u32 mtype=0;
	u32 pid_odid=0;
	u32 pid_id=0;
	u32 ServiceID=0;

	root = sns->owner;
	if (!root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPTIME, ("[Service %s] has no root, aborting !\n", sns->url));
		return;
	}
	if (root->flags & GF_ODM_DESTROYED) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPTIME, ("[Service %s] root has been scheduled for destruction - aborting !\n", sns->url));
		return;
	}
	scene = root->subscene ? root->subscene : root->parentscene;
	if (scene->root_od->addon && (scene->root_od->addon->addon_type == GF_ADDON_TYPE_MAIN)) {
		scene->root_od->flags |= GF_ODM_REGENERATE_SCENE;
	}

	if (scene->is_dynamic_scene) {
		if (!root->ID)
			root->ID = 1;
	}

	v = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!v) return;
	mtype = v->value.uint;

	v = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_ID);
	if (v) ServiceID = v->value.uint;

	v = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (v) pid_id = v->value.uint;

	if (scene->compositor->autofps && (mtype==GF_STREAM_VISUAL) ) {
		v = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (v && v->value.frac.den && v->value.frac.num && (v->value.frac.den!=v->value.frac.num)) {
			scene->compositor->autofps = GF_FALSE;
			gf_sc_set_fps(scene->compositor, v->value.frac);
		}
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[Service %s] Adding new media\n", sns->url));

	/*object declared this way are not part of an OD stream and are considered as dynamic*/
	/*	od->objectDescriptorID = GF_MEDIA_EXTERNAL_ID; */

	/*check if we have a mediaObject in the scene not attached and matching this object*/
	the_mo = NULL;
	odm = NULL;
	min_od_id = 0;
	for (i=0; i<gf_list_count(scene->scene_objects); i++) {
		char *frag = NULL;
		char *ext, *url;
		u32 match_esid = 0;
		Bool type_matched = GF_FALSE;
		GF_MediaObject *mo = gf_list_get(scene->scene_objects, i);

		if ((mo->OD_ID != GF_MEDIA_EXTERNAL_ID) && (min_od_id<mo->OD_ID))
			min_od_id = mo->OD_ID;

		//got the service, check the type
		/*match type*/
		switch (mtype) {
		case GF_STREAM_VISUAL:
			if (mo->type == GF_MEDIA_OBJECT_VIDEO) type_matched = GF_TRUE;
			break;
		case GF_STREAM_AUDIO:
			if (mo->type == GF_MEDIA_OBJECT_AUDIO) type_matched = GF_TRUE;
			break;
		case GF_STREAM_SCENE:
			if (mo->type == GF_MEDIA_OBJECT_UPDATES) type_matched = GF_TRUE;
			break;
		default:
			break;
		}
		if (!mo->odm) {
			u32 j;
			//if not external OD, look for ODM with same ID
			if (mo->OD_ID != GF_MEDIA_EXTERNAL_ID) {
				for (j=0; j<gf_list_count(scene->resources); j++) {
					GF_ObjectManager *an_odm = gf_list_get(scene->resources, j);
					if (an_odm->ID == mo->OD_ID) {
						mo->odm = an_odm;
						break;
					}
				}
			}
			if (!mo->odm) {
				continue;
			}
		}
		/*if object is attached to a service, don't bother looking in a different one*/
		if (mo->odm->scene_ns && (mo->odm->scene_ns != sns)) {
			Bool mine = 0;
			if (mo->odm->scene_ns->source_filter) {
				mine = gf_filter_pid_is_filter_in_parents(pid, mo->odm->scene_ns->source_filter);
			}
			//passthrough ODM not yet assigned, consider it ours
			else if (!mo->odm->pid && (mo->odm->flags & GF_ODM_PASSTHROUGH)) {
				mine = GF_TRUE;
			}
			if (!mine) continue;

			if (type_matched) {
				the_mo = mo;
				odm = mo->odm;
				pid_odid = odm->ID = mo->OD_ID;
				break;
			}
		}

		/*already assigned object*/
		if (mo->odm->ID) continue;

		/*we cannot match yet the object, we need the OD command for that*/
		if (mo->OD_ID != GF_MEDIA_EXTERNAL_ID) {
			continue;
		}
		if (!mo->URLs.count || !mo->URLs.vals[0].url) continue;

		ext = strrchr(mo->URLs.vals[0].url, '#');
		if (ext) {
			frag = strchr(ext, '=');
			ext[0] = 0;
		}
		url = mo->URLs.vals[0].url;
		if (!strnicmp(url, "file://localhost", 16)) url += 16;
		else if (!strnicmp(url, "file://", 7)) url += 7;
		else if (!strnicmp(url, "gpac://", 7)) url += 7;
		else if (!strnicmp(url, "pid://", 6)) match_esid = atoi(url+6);

		if (!match_esid && mo->odm->scene_ns && !strstr(mo->odm->scene_ns->url, url)) {
			if (ext) ext[0] = '#';
			continue;
		}
		if (ext) ext[0] = '#';

		if (match_esid && (pid_id != match_esid))
			continue;

		/*match type*/
		switch (mtype) {
		case GF_STREAM_VISUAL:
			if (mo->type != GF_MEDIA_OBJECT_VIDEO) continue;
			break;
		case GF_STREAM_AUDIO:
			if (mo->type != GF_MEDIA_OBJECT_AUDIO) continue;
			break;
		case GF_STREAM_SCENE:
			if (mo->type != GF_MEDIA_OBJECT_UPDATES) continue;
			break;
		default:
			continue;
		}

		if (frag) {
			u32 frag_id = 0;
			u32 ID = mo->OD_ID;
			if (ID==GF_MEDIA_EXTERNAL_ID) ID = pid_id;
			frag++;
			frag_id = atoi(frag);
			if (ID!=frag_id) continue;
		}

		the_mo = mo;
		odm = mo->odm;
		pid_odid = odm->ID = mo->OD_ID;
		if (mo->OD_ID == GF_MEDIA_EXTERNAL_ID)
			odm->flags |= GF_ODM_NOT_IN_OD_STREAM;
		break;
	}

	/*add a pass on scene->resource to check for min_od_id,
	otherwise we may have another modules declaring an object with ID 0 from
	another thread, which will assert (only one object with a givne OD ID)*/
	for (i=0; i<gf_list_count(scene->resources); i++) {
		GF_ObjectManager *an_odm = gf_list_get(scene->resources, i);

		if (an_odm->ID == GF_MEDIA_EXTERNAL_ID) continue;

		if (min_od_id < an_odm->ID)
			min_od_id = an_odm->ID;

		if (odm) continue;
		//check for OD replacement, look for ODMs with no PID assigned and a source filter origin for this pid
		if (an_odm->pid) continue;

		if (!an_odm->scene_ns->source_filter) continue;
		if (! gf_filter_pid_is_filter_in_parents(pid, an_odm->scene_ns->source_filter))
			continue;

		odm = an_odm;
		pid_odid = an_odm->ID;
		break;
	}

	if (is_in_iod) {
		odm = scene->root_od;
		//for inline cases, the media object may already be setup
		the_mo = odm->mo;
		if (the_mo) pid_odid = the_mo->OD_ID;
	}

	if (!odm) {
		odm = gf_odm_new();
		odm->parentscene = scene;
		gf_list_add(scene->resources, odm);
	}
	odm->flags |= GF_ODM_NOT_SETUP;
	if (!pid_odid) {
		pid_odid = min_od_id + 1;
	}
	odm->ID = pid_odid;
	odm->mo = the_mo;
	odm->ServiceID = ServiceID;
	if (!odm->pid) {
		odm->type = mtype;
		if (mtype == GF_STREAM_AUDIO) {
			scene->compositor->audio_renderer->nb_audio_objects++;
			scene->compositor->audio_renderer->scene_ready = GF_FALSE;
		}
	}
	if (gf_filter_pid_is_sparse(pid)) {
		odm->flags |= GF_ODM_IS_SPARSE;
	}

	//register PID with ODM, but don't call setup object
	gf_odm_register_pid(odm, pid, GF_TRUE);

	gf_filter_pid_set_udta(pid, odm);

	v = gf_filter_pid_get_property(pid, GF_PROP_PID_PLAYBACK_MODE);
	if (!v || !v->value.uint) odm->flags |= GF_ODM_NO_TIME_CTRL;

	if (the_mo) the_mo->OD_ID = odm->ID;
	if (!scene->selected_service_id)
		scene->selected_service_id = ServiceID;

	if (odm->parentscene && odm->parentscene->is_dynamic_scene)
		odm->flags |= GF_ODM_NOT_IN_OD_STREAM;

	//we insert right away the PID as a new object if the scene is dynamic
	//if the scene is not dynamic, we wait for the corresponding OD update
	//otherwise if subscene, this is an IOD
	if (odm->subscene || (odm->flags & GF_ODM_NOT_IN_OD_STREAM) ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[ODM%d] setup object - MO %08x\n", odm->ID, odm->mo));
		gf_odm_setup_object(odm, sns, pid);
	} else {
		//cannot setup until we get the associated OD_Update
		odm->ID = 0;
	}

	if (scene->is_dynamic_scene && !root->scene_ns->connect_ack) {
		/*othewise send a connect ack for top level*/
		GF_Event evt;
		root->scene_ns->connect_ack = GF_TRUE;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_COMPTIME, ("[ODM] Root object connected (%s) !\n", root->scene_ns->url));

		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = GF_TRUE;
		gf_sc_send_event(scene->compositor, &evt);
	}
}

GF_SceneNamespace *gf_scene_ns_new(GF_Scene *scene, GF_ObjectManager *owner, const char *url, const char *parent_url)
{
	char *frag;
	GF_SceneNamespace *sns;

	GF_SAFEALLOC(sns, GF_SceneNamespace);
	if (!sns) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_COMPTIME, ("[Compose] Failed to allocate namespace\n"));
		return NULL;
	}
	sns->owner = owner;
	sns->url = gf_url_concatenate(parent_url, url);
	sns->clocks = gf_list_new();
	frag = strchr(sns->url, '#');
	if (frag) {
		sns->url_frag = gf_strdup(frag+1);
		frag[0] = 0;
	}

	//move to top scene
	scene = gf_scene_get_root_scene(scene);
	gf_list_add(scene->namespaces, sns);

	return sns;
}



void gf_scene_ns_del(GF_SceneNamespace *sns, GF_Scene *root_scene)
{
	gf_list_del_item(root_scene->namespaces, sns);

	if (sns->source_filter) {
		gf_filter_remove_src(root_scene->compositor->filter, sns->source_filter);
	}
	if (sns->clocks) {
		while (gf_list_count(sns->clocks)) {
			GF_Clock *ck = gf_list_pop_back(sns->clocks);
			gf_clock_del(ck);
		}
		gf_list_del(sns->clocks);
	}
	if (sns->url) gf_free(sns->url);
	if (sns->url_frag) gf_free(sns->url_frag);
	gf_free(sns);
}


Bool scene_ns_remove_object(GF_Filter *filter, void *callback, u32 *reschedule_ms)
{
	GF_ObjectManager *odm = (GF_ObjectManager *)callback;
	gf_odm_disconnect(odm, 2);
	return GF_FALSE;
}

/*connects given OD manager to its URL*/
void gf_scene_ns_connect_object(GF_Scene *scene, GF_ObjectManager *odm, char *serviceURL, char *parent_url, GF_SceneNamespace *addon_parent_ns)
{
	GF_Err e;
	char *frag;
	char *url_with_args = NULL;
	Bool reloc_result=GF_FALSE;

    if (addon_parent_ns) {
		parent_url = NULL;
    }

	/*check cache*/
	if (parent_url) {
		u32 i, count;
		count = gf_opts_get_section_count();
		for (i=0; i<count; i++) {
			u32 exp, sec, frac;
			const char *opt, *service_cache;
			const char *name = gf_opts_get_section_name(i);
			if (strncmp(name, "@cache=", 7)) continue;
			opt = gf_opts_get_key(name, "serviceURL");
			if (!opt || stricmp(opt, parent_url)) continue;
			opt = gf_opts_get_key(name, "cacheName");
			if (!opt || stricmp(opt, serviceURL)) continue;

			service_cache = (char*)gf_opts_get_key(name, "cacheFile");
			opt = gf_opts_get_key(name, "expireAfterNTP");
			if (opt) {
				sscanf(opt, "%u", &exp);
				gf_net_get_ntp(&sec, &frac);
				if (exp && (exp<sec)) {
					opt = gf_opts_get_key(name, "cacheFile");
					if (opt) gf_file_delete((char*) opt);
					gf_opts_del_section(name);
					service_cache = NULL;
				}
			}
			if (service_cache) serviceURL = (char *)service_cache;
			break;
		}
	}

	frag = strchr(serviceURL, '#');
	if (frag) {
		frag[0] = 0;
		u32 i, count = gf_list_count(scene->compositor->root_scene->namespaces);
		for (i=0; i<count; i++) {
			GF_SceneNamespace *sns = gf_list_get(scene->compositor->root_scene->namespaces, i);
			if (sns->url && !strcmp(sns->url, serviceURL)) {
				frag[0] = '#';
				odm->scene_ns = sns;
				sns->nb_odm_users++;
				return;
			}
		}
	}

	odm->scene_ns = gf_scene_ns_new(scene->compositor->root_scene, odm, serviceURL, (odm->addon || reloc_result) ? NULL : parent_url);

	if (!odm->scene_ns) {
		gf_scene_message(scene, serviceURL, "Cannot create scene service", GF_OUT_OF_MEM);
		gf_odm_disconnect(odm, 1);
		if (frag) frag[0] = '#';
		return;
	}
	odm->scene_ns->nb_odm_users++;
	assert(odm->scene_ns->owner == odm);

	if (frag && !odm->scene_ns->url_frag) {
		odm->scene_ns->url_frag = gf_strdup(frag+1);
	}

	if (!strncmp(serviceURL, "gpid://", 7)) {
		gf_odm_service_media_event(odm, GF_EVENT_MEDIA_SETUP_BEGIN);
		gf_odm_service_media_event(odm, GF_EVENT_MEDIA_SETUP_DONE);
		odm->scene_ns->connect_ack = GF_TRUE;
		odm->flags |= GF_ODM_NOT_IN_OD_STREAM | GF_ODM_PASSTHROUGH;
		if (frag) frag[0] = '#';
		return;
	}
	if (!addon_parent_ns && !parent_url && odm->parentscene && odm->parentscene->root_od->scene_ns)
		parent_url = odm->parentscene->root_od->scene_ns->url;

	//we setup an addon, use the same filter chain (so copy source id) as orginial content
	//so that any scalable streams in the addon can connect to decoders
    if (addon_parent_ns) {
		const char *fid = gf_filter_get_id(addon_parent_ns->source_filter);
		if (fid) {
			char szExt[100];
			char sep_a = gf_filter_get_sep(scene->compositor->filter, GF_FS_SEP_ARGS);
			char sep_v = gf_filter_get_sep(scene->compositor->filter, GF_FS_SEP_NAME);
			gf_dynstrcat(&url_with_args, serviceURL, NULL);
			sprintf(szExt, "%cgpac%cFID%c", sep_a, sep_a, sep_v);
			gf_dynstrcat(&url_with_args, szExt, NULL);
			gf_dynstrcat(&url_with_args, fid, NULL);
		}
	}

	gf_filter_lock_all(scene->compositor->filter, GF_TRUE);
	odm->scene_ns->source_filter = gf_filter_connect_source(scene->compositor->filter, url_with_args ? url_with_args : serviceURL, parent_url, GF_FALSE, &e);
	if (url_with_args) gf_free(url_with_args);

	if (frag) frag[0] = '#';
	if (!odm->scene_ns->source_filter) {
		Bool remove_scene=GF_FALSE;
		Bool remove_filter=GF_FALSE;
		GF_Compositor *compositor = scene->compositor;
		GF_Scene *target_scene = odm->subscene ? NULL : odm->parentscene;

		gf_filter_lock_all(scene->compositor->filter, GF_FALSE);
		if (!odm->mo && target_scene) {
			u32 i;
			for (i=0; i<gf_list_count(target_scene->scene_objects); i++) {
				GF_MediaObject *mo = gf_list_get(target_scene->scene_objects, i);
				if (mo->odm) continue;
				if ((mo->OD_ID != GF_MEDIA_EXTERNAL_ID) && (mo->OD_ID==odm->ID)) {
					odm->mo = mo;
					mo->odm = odm;
					break;
				}
			}
		}

		if (compositor->player && !odm->parentscene) remove_filter = GF_TRUE;

		if (odm->mo) odm->mo->connect_state = MO_CONNECT_FAILED;
		odm->skip_disconnect_state = 1;
		//prevent scene from being disconnected - this can happen if a script catches the event and triggers a disonnection of the parent scene
		if (target_scene) target_scene->root_od->skip_disconnect_state = 1;
		gf_scene_notify_event(scene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, e, GF_TRUE);
		gf_scene_message(scene, serviceURL, "Cannot find filter for service", e);
		odm->skip_disconnect_state = 0;

		if (target_scene) {
			if (target_scene->root_od->skip_disconnect_state==2) remove_scene = GF_TRUE;
			target_scene->root_od->skip_disconnect_state = 0;
		}
		if (remove_scene) {
			gf_filter_post_task(scene->compositor->filter, scene_ns_remove_object, target_scene->root_od, "remove_odm");
		} else {
			gf_filter_post_task(scene->compositor->filter, scene_ns_remove_object, odm, "remove_odm");
		}
		if (remove_filter) {
			gf_filter_remove(compositor->filter);
			compositor->check_eos_state = 3;
		}
		return;
	}
    //make sure we only connect this filter to ourselves
	gf_filter_set_source_restricted(scene->compositor->filter, odm->scene_ns->source_filter, NULL);

	gf_filter_set_setup_failure_callback(scene->compositor->filter, odm->scene_ns->source_filter, scene_ns_on_setup_error, odm);

	gf_filter_lock_all(scene->compositor->filter, GF_FALSE);

	/*OK connect*/
	gf_odm_service_media_event(odm, GF_EVENT_MEDIA_SETUP_BEGIN);
}
