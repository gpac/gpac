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

#if FILTER_FIXME
//we will need to get events from the filter session to signal connection failure - TODO

static void term_on_connect(GF_ClientService *service, LPNETCHANNEL netch, GF_Err err)
{
	GF_Channel *ch;
	GF_ObjectManager *root;
	GF_Terminal *term = service->term;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] %s connection ACK received from %s - %s\n", netch ? "Channel" : "Service", service->url, gf_error_to_string(err) ));

	root = service->owner;
	if (root && (root->net_service != service)) {
		gf_term_message(term, service->url, "Incompatible module type", GF_SERVICE_ERROR);
		return;
	}
	/*this is service connection*/
	if (!netch) {
		gf_term_service_media_event(service->owner, GF_EVENT_MEDIA_SETUP_DONE);
		if (err) {
			char msg[5000];
			snprintf(msg, sizeof(msg), "Cannot open %s", service->url);
			gf_term_message(term, service->url, msg, err);

			gf_term_service_media_event(service->owner, GF_EVENT_ERROR);

			/*destroy service only if attached*/
			if (root) {
				gf_term_lock_media_queue(term, 1);
				//notify before disconnecting
				if (root->subscene) gf_scene_notify_event(root->subscene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, err, GF_FALSE);

				service->ifce->CloseService(service->ifce);
				root->net_service = NULL;
				if (service->owner && service->nb_odm_users) service->nb_odm_users--;
				service->owner = NULL;
				/*depends on module: some module could forget to call gf_service_disconnect_ack */
				if ( gf_list_del_item(term->net_services, service) >= 0) {
					/*and queue for destroy*/
					gf_list_add(term->net_services_to_remove, service);
				}

				gf_term_lock_media_queue(term, 0);

				if (!root->parentscene) {
					GF_Event evt;
					evt.type = GF_EVENT_CONNECT;
					evt.connect.is_connected = 0;
					gf_term_send_event(term, &evt);
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

		if (!root) {
			/*channel service connect*/
			u32 i;
			GF_ChannelSetup *cs;
			GF_List *ODs;

			if (!gf_list_count(term->channels_pending)) {
				return;
			}
			ODs = gf_list_new();
			gf_term_lock_net(term, 1);
			i=0;
			while ((cs = (GF_ChannelSetup*)gf_list_enum(term->channels_pending, &i))) {
				if (cs->ch->service != service) continue;
				gf_list_rem(term->channels_pending, i-1);
				i--;
				/*even if error do setup (channel needs to be deleted)*/
				if (gf_odm_post_es_setup(cs->ch, cs->dec, err) == GF_OK) {
					if (cs->ch->odm && (gf_list_find(ODs, cs->ch->odm)==-1) ) gf_list_add(ODs, cs->ch->odm);
				}
				gf_free(cs);
			}
			gf_term_lock_net(term, 0);
			/*finally setup all ODs concerned (we do this later in case of scalability)*/
			while (gf_list_count(ODs)) {
				GF_ObjectManager *odm = (GF_ObjectManager*)gf_list_get(ODs, 0);
				gf_list_rem(ODs, 0);
				/*force re-setup*/
				gf_scene_setup_object(odm->parentscene, odm);
			}
			gf_list_del(ODs);
		} else {
			/*setup od*/
			gf_odm_setup_entry_point(root, service->url);
		}
		/*load cache if requested*/
		if (!err && term->enable_cache) {
			err = gf_term_service_cache_load(service);
			/*not a fatal error*/
			if (err) gf_term_message(term, "GPAC Cache", "Cannot load cache", err);
		}
		return;
	}

	/*this is channel connection*/
	ch = gf_term_get_channel(service, netch);
	if (!ch) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Channel connection ACK error: channel not found\n"));
		return;
	}

	/*confirm channel connection even if error - this allow playback of objects even if not all streams are setup*/
	gf_term_lock_net(term, 1);
	gf_es_on_connect(ch);
	gf_term_lock_net(term, 0);

	if (err && ((err!=GF_STREAM_NOT_FOUND) || (ch->esd->decoderConfig->streamType!=GF_STREAM_INTERACT))) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Channel %d connection error: %s\n", ch->esd->ESID, gf_error_to_string(err) ));
		ch->es_state = GF_ESM_ES_UNAVAILABLE;
		/*		return;*/
	}

	if (ch->odm->mo) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Channel %d connected - %d objects opened\n", ch->esd->ESID, ch->odm->mo->num_open ));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Channel %d connected - not attached to the scene\n", ch->esd->ESID));
	}
	/*Plays request are skiped until all channels are connected. We send a PLAY on the objecy in case
		1-OD user has requested a play
		2-this is a channel of the root OD
	*/
	if ( (ch->odm->mo && ch->odm->mo->num_open)
	        || !ch->odm->parentscene
	   ) {
		gf_odm_start(ch->odm, 0);
	}
#if 0
	else if (ch->odm->codec && ch->odm->codec->ck && ch->odm->codec->ck->no_time_ctrl) {
		gf_odm_play(ch->odm);
	}
#endif
}

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
				gf_scene_disconnect(scene, 1);
			}
			return;
		}
		gf_term_lock_media_queue(term, 1);
		/*unregister from valid services*/
		if (gf_list_del_item(term->net_services, service)>=0) {
			/*and queue for destroy*/
			gf_list_add(term->net_services_to_remove, service);
		}
		gf_term_lock_media_queue(term, 0);
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

void gf_scene_insert_object(GF_Scene *scene, GF_SceneNamespace *sns, GF_FilterPid *pid)
{
	u32 i, min_od_id;
	GF_MediaObject *the_mo;
	GF_ObjectManager *odm, *root;
	const GF_PropertyValue *v;
	u32 mtype=0;
	u32 moti=0;
	u32 pid_odid=0;
	u32 ServiceID=0;

	root = sns->owner;
	if (!root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Service %s] has not root, aborting !\n", sns->url));
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

	v = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (!v) return;
	mtype = v->value.uint;

	v = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_ID);
	if (v)
		ServiceID = v->value.uint;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Service %s] Adding new media\n", sns->url));

	/*object declared this way are not part of an OD stream and are considered as dynamic*/
	/*	od->objectDescriptorID = GF_MEDIA_EXTERNAL_ID; */

	/*check if we have a mediaObject in the scene not attached and matching this object*/
	the_mo = NULL;
	odm = NULL;
	min_od_id = 0;
	for (i=0; i<gf_list_count(scene->scene_objects); i++) {
		char *frag, *ext;
		GF_ESD *esd;
		char *url;
		u32 match_esid = 0;
		GF_MediaObject *mo = gf_list_get(scene->scene_objects, i);

		if ((mo->OD_ID != GF_MEDIA_EXTERNAL_ID) && (min_od_id<mo->OD_ID))
			min_od_id = mo->OD_ID;

		if (!mo->odm) continue;
		/*if object is attached to a service, don't bother looking in a different one*/
		if (mo->odm->scene_ns && (mo->odm->scene_ns != sns)) continue;

		/*already assigned object - this may happen since the compositor has no control on when objects are declared by the service,
		therefore opening file#video and file#audio may result in the objects being declared twice if the service doesn't
		keep track of declared objects*/
		if (mo->odm->ID) {
			if (pid_odid && (mo->odm->type==mtype) && (mo->odm->original_oti == moti)) {
				assert(mo->odm->pid);
				return;
			}
			continue;
		}
		if (mo->OD_ID != GF_MEDIA_EXTERNAL_ID) {
			if (mo->OD_ID == pid_odid) {
				the_mo = mo;
				odm = mo->odm;
				break;
			}
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

		if (!match_esid && !strstr(sns->url, url)) {
			if (ext) ext[0] = '#';
			continue;
		}
		if (ext) ext[0] = '#';

#if FILTER_FIXME
		esd = gf_list_get(od->ESDescriptors, 0);
		if (match_esid && (esd->ESID != match_esid))
			continue;
		/*match type*/
		switch (esd->decoderConfig->streamType) {
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
		if (frag) {
			u32 frag_id = 0;
			u32 ID = od->objectDescriptorID;
			if (ID==GF_MEDIA_EXTERNAL_ID) ID = esd->ESID;
			frag++;
			frag_id = atoi(frag);
			if (ID!=frag_id) continue;
		}
		the_mo = mo;
		odm = mo->odm;
		break;
#endif
	}

	/*add a pass on scene->resource to check for min_od_id,
	otherwise we may have another modules declaring an object with ID 0 from
	another thread, which will assert (only one object with a givne OD ID)*/
	for (i=0; i<gf_list_count(scene->resources); i++) {
		GF_ObjectManager *an_odm = gf_list_get(scene->resources, i);

		if ((an_odm->ID != GF_MEDIA_EXTERNAL_ID) && (min_od_id < an_odm->ID))
			min_od_id = an_odm->ID;
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
	odm->pid = pid;
	odm->type = mtype;
	odm->flags |= GF_ODM_NOT_IN_OD_STREAM;
	gf_filter_pid_set_udta(pid, odm);

	if (the_mo) the_mo->OD_ID = odm->ID;
	if (!scene->selected_service_id)
		scene->selected_service_id = ServiceID;


	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] setup object - MO %08x\n", odm->ID, odm->mo));
	gf_odm_setup_object(odm, sns, NULL);

}

GF_SceneNamespace *gf_scene_ns_new(GF_Scene *scene, GF_ObjectManager *owner, const char *url, const char *parent_url)
{
	GF_Scene *top_scene = scene;
	char *sURL;
	GF_SceneNamespace *sns;

	GF_SAFEALLOC(sns, GF_SceneNamespace);
	if (!sns) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Compose] Failed to allocate namespace\n"));
		return NULL;
	}
	sns->owner = owner;
	sns->url = gf_url_concatenate(parent_url, url);
	sns->Clocks = gf_list_new();

	while (top_scene->parent_scene) top_scene = top_scene->parent_scene;
	gf_list_add(top_scene->namespaces, sns);

	return sns;
}



void gf_scene_ns_del(GF_SceneNamespace *sns)
{
	while (gf_list_count(sns->Clocks)) {
		GF_Clock *ck = gf_list_pop_back(sns->Clocks);
		gf_clock_del(ck);
	}
	gf_list_del(sns->Clocks);
	if (sns->url) gf_free(sns->url);
	gf_free(sns);
}

