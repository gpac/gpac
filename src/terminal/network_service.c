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
#include <gpac/network.h>

struct on_setup_task
{
	GF_ObjectManager *odm;
	GF_Err reason;
};

static void scene_ns_on_setup_error_task(GF_FSTask *filter_task)
{
	GF_SceneNamespace *scene_ns;
	GF_Scene *scene, *top_scene;
	struct on_setup_task *task = gf_fs_task_get_udta(filter_task);
	GF_Err err = task->reason;
	GF_ObjectManager *root = (GF_ObjectManager *)task->odm;
	assert(root);
	scene_ns = root->scene_ns;
	assert(scene_ns);
	scene = root->subscene ? root->subscene : root->parentscene;
	assert(scene);

	gf_free(task);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM] Service connection failure received from %s - %s\n", scene_ns->url, gf_error_to_string(err) ));

	if (root->scene_ns->owner != root) {
		gf_scene_message(scene, scene_ns->url, "Incompatible module type", GF_SERVICE_ERROR);
		return;
	}
	/*this is service connection*/
	gf_odm_service_media_event(root, GF_EVENT_MEDIA_SETUP_DONE);

	scene_ns->connect_ack = GF_TRUE;
	if (err) {
		char msg[5000];
		snprintf(msg, sizeof(msg), "Cannot open %s", scene_ns->url);
		gf_scene_message(scene, scene_ns->url, msg, err);

		gf_odm_service_media_event(root, GF_EVENT_ERROR);

		/*destroy service only if attached*/
		if (root) {
			//notify before disconnecting
			if (root->subscene) gf_scene_notify_event(root->subscene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, err, GF_FALSE);

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
				gf_fs_send_event(scene->compositor->fsess, &evt);
			} else {
				/*try to reinsert OD for VRML/X3D with multiple URLs:
				1- first remove from parent scene without destroying object, this will trigger a re-setup
					if other URLs are present
					2- then destroy object*/
				gf_scene_remove_object(root->parentscene, root, 0);
				gf_odm_disconnect(root, 1);
			}
			return;
		}
	}
}

static void scene_ns_on_setup_error(GF_Filter *failed_filter, void *udta, GF_Err err)
{
	GF_Scene *scene;
	struct on_setup_task*t = gf_malloc(sizeof(struct on_setup_task));
	t->reason = err;
	t->odm = (GF_ObjectManager *)udta;
	assert(t->odm);
	scene = t->odm->subscene ? t->odm->subscene : t->odm->parentscene;
	assert(scene);
	gf_filter_post_task(scene->compositor->filter, scene_ns_on_setup_error_task, t, "compositor_on_connect");
}



#ifdef FILTER_FIXME
static void term_on_disconnect(GF_ClientService *service, LPNETCHANNEL netch, GF_Err response)
{
	GF_ObjectManager *root;
	GF_Channel *ch;
	GF_Terminal *term = service->term;

	/*may be null upon destroy*/
	root = service->owner;
	if (root && (root->net_service != service)) {
		if (root->net_service) gf_term_message(term, service->url, "Incompatible module type", GF_SERVICE_ERROR);
		return;
	}
	//reset global seek time
	if (term->root_scene && term->root_scene->root_od)
		term->root_scene->root_od->media_start_time = 0;

	/*this is service disconnect*/
	if (!netch) {
		if (service->subservice_disconnect) {
			if (service->owner && service->subservice_disconnect==1) {
				GF_Scene *scene = service->owner->subscene ? service->owner->subscene : service->owner->parentscene;
				/*destroy all media*/
				gf_scene_disconnect(scene, GF_TRUE);
			}
			return;
		}
		/*unregister from valid services*/
		if (gf_list_del_item(term->net_services, service)>=0) {
			/*and queue for destroy*/
			gf_list_add(term->net_services_to_remove, service);
		}
		return;
	}
	/*this is channel disconnect*/

	/*no notif in case of failure for disconnection*/
	ch = gf_term_get_channel(service, netch);
	if (!ch) return;
	/*signal channel state*/
	ch->es_state = GF_ESM_ES_DISCONNECTED;
}
#endif


static Bool is_same_od(GF_ObjectDescriptor *od1, GF_ObjectDescriptor *od2)
{
	GF_ESD *esd1, *esd2;
	if (gf_list_count(od1->ESDescriptors) != gf_list_count(od2->ESDescriptors)) return 0;
	esd1 = gf_list_get(od1->ESDescriptors, 0);
	if (!esd1) return 0;
	esd2 = gf_list_get(od2->ESDescriptors, 0);
	if (!esd2) return 0;
	if (esd1->ESID!=esd2->ESID) return 0;
	if (esd1->decoderConfig->streamType != esd2->decoderConfig->streamType) return 0;
	if (esd1->decoderConfig->objectTypeIndication != esd2->decoderConfig->objectTypeIndication ) return 0;
	return 1;
}

void gf_scene_insert_pid(GF_Scene *scene, GF_SceneNamespace *sns, GF_FilterPid *pid, Bool is_in_iod)
{
	u32 i, min_od_id;
	GF_MediaObject *the_mo;
	GF_ObjectManager *odm, *root;
	const GF_PropertyValue *v;
	u32 mtype=0;
	u32 moti=0;
	u32 pid_odid=0;
	u32 pid_id=0;
	u32 ServiceID=0;

	root = sns->owner;
	if (!root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Service %s] has no root, aborting !\n", sns->url));
		return;
	}
	if (root->flags & GF_ODM_DESTROYED) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Service %s] root has been scheduled for destruction - aborting !\n", sns->url));
		return;
	}
	scene = root->subscene ? root->subscene : root->parentscene;
	if (scene->root_od->addon && (scene->root_od->addon->addon_type == GF_ADDON_TYPE_MAIN)) {
		scene->root_od->flags |= GF_ODM_REGENERATE_SCENE;
	}

	if (scene->is_dynamic_scene) {
		if (!root->ID)
			root->ID = 1;
			
		if (!root->scene_ns->connect_ack) {
			/*othewise send a connect ack for top level*/
			GF_Event evt;
			root->scene_ns->connect_ack = GF_TRUE;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM] Root object connected (%s) !\n", root->scene_ns->url));

			evt.type = GF_EVENT_CONNECT;
			evt.connect.is_connected = GF_TRUE;
			gf_sc_send_event(scene->compositor, &evt);
		}
	}

	v = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!v) return;
	mtype = v->value.uint;

	v = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_ID);
	if (v) ServiceID = v->value.uint;

	v = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (v) pid_id = v->value.uint;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Service %s] Adding new media\n", sns->url));

	/*object declared this way are not part of an OD stream and are considered as dynamic*/
	/*	od->objectDescriptorID = GF_MEDIA_EXTERNAL_ID; */

	/*check if we have a mediaObject in the scene not attached and matching this object*/
	the_mo = NULL;
	odm = NULL;
	min_od_id = 0;
	for (i=0; i<gf_list_count(scene->scene_objects); i++) {
		char *frag, *ext;
		char *url;
		u32 match_esid = 0;
		GF_MediaObject *mo = gf_list_get(scene->scene_objects, i);

		if ((mo->OD_ID != GF_MEDIA_EXTERNAL_ID) && (min_od_id<mo->OD_ID))
			min_od_id = mo->OD_ID;

		if (!mo->odm) continue;
		/*if object is attached to a service, don't bother looking in a different one*/
		if (mo->odm->scene_ns && (mo->odm->scene_ns != sns)) {
			Bool in_parent = gf_filter_pid_is_filter_in_parents(pid, mo->odm->scene_ns->source_filter);
			if (!in_parent) continue;
			//got it !
		}

		/*already assigned object*/
		if (mo->odm->ID) continue;

		/*we cannot match yet the object, we need the OD command for that*/
		if (mo->OD_ID != GF_MEDIA_EXTERNAL_ID) {
			continue;
		}
		if (!mo->URLs.count || !mo->URLs.vals[0].url) continue;

		frag = NULL;
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

		if (!match_esid && !strstr(mo->odm->scene_ns->url, url)) {
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
		case GF_STREAM_PRIVATE_MEDIA:
			if ((mo->type != GF_MEDIA_OBJECT_AUDIO) && (mo->type != GF_MEDIA_OBJECT_VIDEO)) continue;
			break;
		case GF_STREAM_SCENE:
			if (mo->type != GF_MEDIA_OBJECT_UPDATES) continue;
			break;
		default:
			continue;
		}
#if FILTER_FIXME
		if (frag) {
			u32 frag_id = 0;
			u32 ID = od->objectDescriptorID;
			if (ID==GF_MEDIA_EXTERNAL_ID) ID = esd->ESID;
			frag++;
			frag_id = atoi(frag);
			if (ID!=frag_id) continue;
		}
#endif
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

		if ((an_odm->ID != GF_MEDIA_EXTERNAL_ID) && (min_od_id < an_odm->ID))
			min_od_id = an_odm->ID;
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
	if (!odm->pid)
		odm->type = mtype;

	//register PID with ODM, but don't call setup object
	gf_odm_register_pid(odm, pid, GF_TRUE);

	gf_filter_pid_set_udta(pid, odm);

	if (the_mo) the_mo->OD_ID = odm->ID;
	if (!scene->selected_service_id)
		scene->selected_service_id = ServiceID;

	if (odm->parentscene && odm->parentscene->is_dynamic_scene)
		odm->flags |= GF_ODM_NOT_IN_OD_STREAM;

	//we insert right away the PID as a new object if the scene is dynamic
	//if the scene is not dynamic, we wait for the corresponding OD update
	//FILTER_FIXME: needs rework to enable attaching subtitle to a non-dynamic scene
	//otherwise if subscene, this is an IOD
	if (odm->subscene || (odm->flags & GF_ODM_NOT_IN_OD_STREAM) ) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] setup object - MO %08x\n", odm->ID, odm->mo));
		gf_odm_setup_object(odm, sns, pid);
	} else {
		//cannot setup until we get the associated OD_Update
		odm->ID = 0;
	}
}

GF_SceneNamespace *gf_scene_ns_new(GF_Scene *scene, GF_ObjectManager *owner, const char *url, const char *parent_url)
{
	GF_SceneNamespace *sns;

	GF_SAFEALLOC(sns, GF_SceneNamespace);
	if (!sns) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Compose] Failed to allocate namespace\n"));
		return NULL;
	}
	sns->owner = owner;
	sns->url = gf_url_concatenate(parent_url, url);
	sns->Clocks = gf_list_new();

	//move to top scene
	scene = gf_scene_get_root_scene(scene);
	gf_list_add(scene->namespaces, sns);

	return sns;
}



void gf_scene_ns_del(GF_SceneNamespace *sns, GF_Scene *root_scene)
{
	gf_list_del_item(root_scene->namespaces, sns);

	if (sns->source_filter) {
		gf_filter_remove(sns->source_filter, root_scene->compositor->filter);
	}

	while (gf_list_count(sns->Clocks)) {
		GF_Clock *ck = gf_list_pop_back(sns->Clocks);
		gf_clock_del(ck);
	}
	gf_list_del(sns->Clocks);
	if (sns->url) gf_free(sns->url);
	gf_free(sns);
}


/*connects given OD manager to its URL*/
void gf_scene_ns_connect_object(GF_Scene *scene, GF_ObjectManager *odm, char *serviceURL, char *parent_url)
{
	GF_SceneNamespace *ns;
	u32 i, count;
	GF_Err e;
	Bool reloc_result=GF_FALSE;
	GF_Filter *filter;
	GF_FilterSession *fsess = gf_filter_get_session(scene->compositor->filter);

	
#if FILTER_FIXME
	Bool net_locked;
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

			/*wait for service to setup - service may become destroyed if not available*/
			while (1) {
				if (!ns->owner) {
					return;
				}
				if (ns->owner->OD) break;
				gf_sleep(5);
			}

			if (odm->net_service) {
				return;
			}
			if (odm->flags & GF_ODM_DESTROYED) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] Object has been scheduled for destruction - ignoring object setup\n", odm->ID));
				return;
			}
			odm->net_service = ns;
			odm->net_service->nb_odm_users ++;
			gf_odm_setup_entry_point(odm, serviceURL);
			return;
		}
	}

#endif


	odm->scene_ns = gf_scene_ns_new(scene->compositor->root_scene, odm, serviceURL, (odm->addon || reloc_result) ? NULL : parent_url);

	if (!odm->scene_ns) {
		gf_scene_message(scene, serviceURL, "Cannot create scene service", GF_OUT_OF_MEM);
		gf_odm_disconnect(odm, 1);
		return;
	}
	odm->scene_ns->nb_odm_users++;
	assert(odm->scene_ns->owner == odm);

	odm->scene_ns->source_filter = gf_fs_load_source(fsess, serviceURL, parent_url, &e);
	if (!odm->scene_ns->source_filter) {
		gf_scene_message(scene, serviceURL, "Cannot find filter for service", e);
		gf_odm_disconnect(odm, 1);
		return;
	}

	gf_filter_set_setup_failure_callback(odm->scene_ns->source_filter, scene_ns_on_setup_error, odm);

	/*OK connect*/
	gf_odm_service_media_event(odm, GF_EVENT_MEDIA_SETUP_BEGIN);
}


GF_EXPORT
Bool gf_term_is_supported_url(GF_Terminal *term, const char *fileName, Bool use_parent_url, Bool no_mime_check)
{
#ifdef FILTER_FIXME
	GF_InputService *ifce;
	GF_Err e;
	char *sURL;
	char *mime=NULL;
	char *parent_url = NULL;
	if (use_parent_url && term->root_scene) parent_url = term->root_scene->root_od->net_service->url;

	ifce = gf_term_can_handle_service(term, fileName, parent_url, no_mime_check, &sURL, &e, NULL, &mime);
	if (!ifce) return 0;
	gf_modules_close_interface((GF_BaseInterface *) ifce);
	gf_free(sURL);
	if (mime) gf_free(mime);
#endif
	return 1;
}
