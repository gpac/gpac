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
#include <gpac/cache.h>
#include "media_memory.h"
#include "media_control.h"



#define GET_TERM()	GF_Terminal *term = (GF_Terminal *) user_priv; if (!term) return;

static GFINLINE GF_Channel *gf_term_get_channel(GF_ClientService *service, LPNETCHANNEL ns)
{
	GF_Channel *ch = (GF_Channel *)ns;
	if (!service || !ch) return NULL;
	if (ch->service != service) return NULL;
	return ch;
}

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
					if (root->subscene) gf_scene_notify_event(root->subscene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, err, GF_FALSE);
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

static void term_on_data_packet(GF_ClientService *service, LPNETCHANNEL netch, char *data, u32 data_size, GF_SLHeader *hdr, GF_Err reception_status)
{
	GF_Channel *ch;

	ch = gf_term_get_channel(service, netch);
	if (!ch)
		return;

	if (reception_status==GF_EOS) {
		gf_es_on_eos(ch);
		return;
	}
	/*otherwise dispatch with error code*/
	gf_es_receive_sl_packet(service, ch, data, data_size, hdr, reception_status);
}

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

static void term_on_media_add(GF_ClientService *service, GF_Descriptor *media_desc, Bool no_scene_check)
{
	u32 i, min_od_id;
	GF_MediaObject *the_mo;
	GF_Scene *scene;
	GF_ObjectManager *odm, *root;
	GF_ObjectDescriptor *od;
	GF_Terminal *term = service->term;

	root = service->owner;
	if (!root) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Service %s] has not root, aborting !\n", service->url));
		return;
	}
	if (root->flags & GF_ODM_DESTROYED) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Service %s] root has been scheduled for destruction - aborting !\n", service->url));
		return;
	}
	scene = root->subscene ? root->subscene : root->parentscene;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Service %s] %s\n", service->url, media_desc ? "Adding new media object" : "Regenerating scene graph"));
	if (!media_desc) {
		if (!no_scene_check) gf_scene_regenerate(scene);
		return;
	}

	switch (media_desc->tag) {
	case GF_ODF_OD_TAG:
	case GF_ODF_IOD_TAG:
		if (root && (root->net_service == service)) {
			od = (GF_ObjectDescriptor *) media_desc;
			break;
		}
	default:
		gf_odf_desc_del(media_desc);
		return;
	}

	gf_term_lock_net(term, 1);
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
		if (mo->odm->net_service && (mo->odm->net_service != service)) continue;

		/*already assigned object - this may happen since the compositor has no control on when objects are declared by the service,
		therefore opening file#video and file#audio may result in the objects being declared twice if the service doesn't
		keep track of declared objects*/
		if (mo->odm->OD) {
			if (od->objectDescriptorID && is_same_od(mo->odm->OD, od)) {
				/*reassign OD ID*/
				mo->OD_ID = od->objectDescriptorID;
				gf_odf_desc_del(media_desc);
				gf_term_lock_net(term, 0);
				return;
			}
			continue;
		}
		if (mo->OD_ID != GF_MEDIA_EXTERNAL_ID) {
			if (mo->OD_ID == od->objectDescriptorID) {
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

		if (!match_esid && !strstr(service->url, url)) {
			if (ext) ext[0] = '#';
			continue;
		}
		if (ext) ext[0] = '#';

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
	}

	/*add a pass on scene->resource to check for min_od_id,
	otherwise we may have another modules declaring an object with ID 0 from
	another thread, which will assert (only one object with a givne OD ID)*/
	for (i=0; i<gf_list_count(scene->resources); i++) {
		GF_ObjectManager *an_odm = gf_list_get(scene->resources, i);

		if (an_odm->OD && (an_odm->OD->objectDescriptorID != GF_MEDIA_EXTERNAL_ID) && (min_od_id < an_odm->OD->objectDescriptorID))
			min_od_id = an_odm->OD->objectDescriptorID;
	}

	if (!odm) {
		odm = gf_odm_new();
		odm->term = term;
		odm->parentscene = scene;
		gf_list_add(scene->resources, odm);
	}
	odm->flags |= GF_ODM_NOT_SETUP;
	odm->OD = od;
	odm->mo = the_mo;
	odm->flags |= GF_ODM_NOT_IN_OD_STREAM;
	if (!od->objectDescriptorID) {
		od->objectDescriptorID = min_od_id + 1;
	}

	if (the_mo) the_mo->OD_ID = od->objectDescriptorID;
	if (!scene->selected_service_id)
		scene->selected_service_id = od->ServiceID;


	/*net is unlocked before seting up the object as this might trigger events going into JS and deadlocks
	with the compositor*/
	gf_term_lock_net(term, 0);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] setup object - MO %08x\n", odm->OD->objectDescriptorID, odm->mo));
	gf_odm_setup_object(odm, service);

	/*OD inserted by service: resetup scene*/
	if (!no_scene_check && scene->is_dynamic_scene) gf_scene_regenerate(scene);
}

static void gather_buffer_level(GF_ObjectManager *odm, GF_ClientService *service, GF_NetworkCommand *com, u32 *max_buffer_time)
{
	u32 j, count = gf_list_count(odm->channels);
	for (j=0; j<count; j++) {
		GF_Channel *ch = (GF_Channel *)gf_list_get(odm->channels, j);
		if (ch->service != service) continue;
		if (ch->es_state != GF_ESM_ES_RUNNING) continue;
		if (com->base.on_channel && (com->base.on_channel != ch)) continue;
		if (/*!ch->MaxBuffer || */ch->dispatch_after_db || ch->bypass_sl_and_db || ch->IsEndOfStream) continue;
		//perform buffer management only on base layer  -this is because we don't signal which ESs are on/off in the underlying service ...
		if (ch->esd->dependsOnESID) continue;
		if (ch->MaxBuffer>com->buffer.max) com->buffer.max = ch->MaxBuffer;
		if (ch->MinBuffer<com->buffer.min) com->buffer.min = ch->MinBuffer;
		if (ch->IsClockInit) {
			s32 buf_time = (s32) (ch->BufferTime / FIX2FLT(ch->clock->speed) );
			if (!buf_time && ch->BufferTime) buf_time = ch->BufferTime;

			if (buf_time > (s32) *max_buffer_time)
				*max_buffer_time = buf_time ;

			/*if we don't have more units (compressed or not) than requested max for the composition memory, request more data*/
			if (ch->odm->codec && ch->odm->codec->CB && (odm->codec->CB->UnitCount + ch->AU_Count <= odm->codec->CB->Capacity)) {
				com->buffer.occupancy = 0;
			} else if ( (u32) buf_time < com->buffer.occupancy ) {
				com->buffer.occupancy = buf_time;
			}
		} else {
			com->buffer.occupancy = 0;
		}
	}
}

static void term_on_command(GF_ClientService *service, GF_NetworkCommand *com, GF_Err response)
{
	GF_Channel *ch;
	GF_Terminal *term = service->term;

	if (com->command_type==GF_NET_BUFFER_QUERY) {
		GF_List *od_list;
		u32 i, max_buffer_time;
		GF_ObjectManager *odm;
		com->buffer.max = 0;
		com->buffer.min = com->buffer.occupancy = (u32) -1;
		if (!service->owner) {
			com->buffer.occupancy = 0;
			return;
		}

		/*browse all channels in the scene, running on this service, and get buffer info*/
		od_list = NULL;
		if (service->owner->subscene) {
			od_list = service->owner->subscene->resources;
		} else if (service->owner->parentscene) {
			od_list = service->owner->parentscene->resources;
		}
		if (!od_list) {
			com->buffer.occupancy = 0;
			return;
		}
		/*get exclusive access to media scheduler, to make sure ODs are not being
		manipulated*/
		gf_mx_p(term->mm_mx);
		max_buffer_time=0;
		if (!gf_list_count(od_list))
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM] No object manager found for the scene (URL: %s), buffer occupancy will remain unchanged\n", service->url));
		i=0;
		while ((odm = (GF_ObjectManager*)gf_list_enum(od_list, &i))) {
			if (!odm->codec) continue;
			gather_buffer_level(odm, service, com, &max_buffer_time);
		}
		gf_mx_v(term->mm_mx);
		if (com->buffer.occupancy==(u32) -1) com->buffer.occupancy = 0;

		//in bench mode return the 1 if one of the buffer is full (eg sleep until all buffers are not full), 0 otherwise
		if (term->bench_mode) {
			com->buffer.occupancy = (max_buffer_time>com->buffer.max) ? 2 : 0;
			com->buffer.max = 1;
			com->buffer.min = 0;
		}
		return;
	}
	if (com->command_type==GF_NET_SERVICE_INFO) {
		GF_Event evt;
		evt.type = GF_EVENT_METADATA;
		gf_term_send_event(term, &evt);
		return;
	}
	if (com->command_type==GF_NET_SERVICE_MEDIA_CAP_QUERY) {
		gf_sc_get_av_caps(term->compositor, &com->mcaps.width, &com->mcaps.height, &com->mcaps.display_bit_depth, &com->mcaps.audio_bpp, &com->mcaps.channels, &com->mcaps.sample_rate);
		return;
	}
	if (com->command_type==GF_NET_SERVICE_EVENT) {
		/*check for UDP timeout*/
		if (com->send_event.evt.message.error == GF_IP_UDP_TIMEOUT) {
			const char *sOpt = gf_cfg_get_key(term->user->config, "Network", "AutoReconfigUDP");
			if (sOpt && !stricmp(sOpt, "yes")) {
				char szMsg[1024];
				sprintf(szMsg, "!! UDP down (%s) - Retrying with TCP !!\n", com->send_event.evt.message.message);
				gf_term_message(term, service->url, szMsg, GF_IP_NETWORK_FAILURE);

				/*reload scene - FIXME this shall work on inline nodes, not on the root !*/
				if (term->reload_url) gf_free(term->reload_url);
				term->reload_state = 1;
				term->reload_url = gf_strdup(term->root_scene->root_od->net_service->url);
				gf_cfg_set_key(term->user->config, "Network", "UDPNotAvailable", "yes");
				return;
			}
		}

		com->send_event.res = 0;
		if (term->user->EventProc) com->send_event.res = term->user->EventProc(term->user->opaque, &com->send_event.evt);
		return;
	}

	if (com->command_type==GF_NET_ASSOCIATED_CONTENT_LOCATION) {
		GF_Scene *scene = NULL;
		if (service->owner->subscene) {
			scene = service->owner->subscene;
		} else if (service->owner->parentscene) {
			scene = service->owner->parentscene;
		}
		if (scene)
			gf_scene_register_associated_media(scene, &com->addon_info);
		return;
	}
	if (com->command_type==GF_NET_ASSOCIATED_CONTENT_TIMING) {
		GF_Scene *scene = NULL;
		if (service->owner->subscene) {
			scene = service->owner->subscene;
		} else if (service->owner->parentscene) {
			scene = service->owner->parentscene;
		}
		if (scene)
			gf_scene_notify_associated_media_timeline(scene, &com->addon_time);
		return;
	}
	if (com->command_type==GF_NET_SERVICE_SEEK) {
		GF_Scene *scene = NULL;
		if (service->owner->subscene) {
			scene = service->owner->subscene;
		} else if (service->owner->parentscene) {
			scene = service->owner->parentscene;
		}
		if (scene && scene->is_dynamic_scene) {
			gf_sc_lock(term->compositor, 1);
			gf_scene_restart_dynamic(scene, (u64) (com->play.start_range*1000) );
			gf_sc_lock(term->compositor, 0);
		}
		return;
	}


	if (!com->base.on_channel) return;

	ch = gf_term_get_channel(service, com->base.on_channel);
	if (!ch) return;

	switch (com->command_type) {
	/*SL reconfiguration*/
	case GF_NET_CHAN_RECONFIG:
		gf_term_lock_net(term, 1);
		gf_es_reconfig_sl(ch, &com->cfg.sl_config, com->cfg.use_m2ts_sections);
		gf_term_lock_net(term, 0);
		return;
	/*time mapping (TS to media-time)*/
	case GF_NET_CHAN_MAP_TIME:

		if (ch->esd->dependsOnESID) {
			//ignore everything
		} else {
			u32 i;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: before mapping: seed TS %d - TS offset %d\n", ch->esd->ESID, ch->seed_ts, ch->ts_offset));
			ch->seed_ts = com->map_time.timestamp;
			ch->ts_offset = (u32) (com->map_time.media_time*1000);
			GF_LOG(GF_LOG_INFO, GF_LOG_SYNC, ("[SyncLayer] ES%d: mapping TS "LLD" to media time %f - current time %d\n", ch->esd->ESID, com->map_time.timestamp, com->map_time.media_time, gf_clock_time(ch->clock)));
			GF_LOG(GF_LOG_DEBUG, GF_LOG_SYNC, ("[SyncLayer] ES%d: after mapping: seed TS %d - TS offset %d\n", ch->esd->ESID, ch->seed_ts, ch->ts_offset));

			if (com->map_time.reset_buffers) {
				gf_es_reset_buffers(ch);
			}
			/*if we were reassembling an AU, do not perform clock init check when dispatching it since we computed its timestamps
			according to the previous clock origin*/
			else {
				gf_mx_p(ch->mx);
				ch->skip_time_check_for_pending = 1;
				gf_mx_v(ch->mx);
			}
			/*if the channel is the clock, force a re-init*/
			if (gf_es_owns_clock(ch)) {
				ch->IsClockInit = 0;
				gf_clock_reset(ch->clock);
			}
			else if (ch->odm->flags & GF_ODM_INHERIT_TIMELINE) {
				ch->IsClockInit = 0;
//				ch->ts_offset -= ch->seed_ts*1000/ch->ts_res;
			}

			for (i=0; i<gf_list_count(ch->odm->channels); i++) {
				GF_Channel *a_ch = gf_list_get(ch->odm->channels, i);
				if (ch==a_ch) continue;
				if (! a_ch->esd->dependsOnESID) continue;
				a_ch->seed_ts = ch->seed_ts;
				a_ch->IsClockInit = 0;
				a_ch->ts_offset = ch->ts_offset;
			}
		}
		break;
	/*duration changed*/
	case GF_NET_CHAN_DURATION:
		gf_odm_set_duration(ch->odm, ch, (u32) (1000*com->duration.duration));
		break;
	case GF_NET_CHAN_BUFFER_QUERY:
		if (ch->IsEndOfStream) {
			com->buffer.max = com->buffer.min = com->buffer.occupancy = 0;
		} else {
			com->buffer.max = ch->MaxBuffer;
			com->buffer.min = ch->MinBuffer;
			com->buffer.occupancy = (u32) (ch->BufferTime / FIX2FLT(ch->clock->speed) );
		}
		break;
	case GF_NET_CHAN_DRM_CFG:
		gf_term_lock_net(term, 1);
		gf_es_config_drm(ch, &com->drm_cfg);
		gf_term_lock_net(term, 0);
		return;
	case GF_NET_CHAN_GET_ESD:
		gf_term_lock_net(term, 1);
		com->cache_esd.esd = ch->esd;
		com->cache_esd.is_iod_stream = (ch->odm->subscene /*&& (ch->odm->subscene->root_od==ch->odm)*/) ? 1 : 0;
		gf_term_lock_net(term, 0);
		return;
	case GF_NET_CHAN_RESET:
		gf_es_reset_buffers(ch);
		break;
	case GF_NET_CHAN_PAUSE:
		gf_es_buffer_on(ch);
		break;
	case GF_NET_CHAN_RESUME:
		gf_es_buffer_off(ch);
		break;
	case GF_NET_CHAN_BUFFER:
		ch->BufferTime = 100 * com->buffer.occupancy / com->buffer.max;
		gf_scene_buffering_info(ch->odm->parentscene ? ch->odm->parentscene : ch->odm->subscene);
		break;
	default:
		return;
	}
}


Bool net_check_interface(GF_InputService *ifce)
{
	if (!ifce->CanHandleURL) return 0;
	if (!ifce->ConnectService) return 0;
	if (!ifce->CloseService) return 0;
	if (!ifce->ConnectChannel) return 0;
	if (!ifce->DisconnectChannel) return 0;
	if (!ifce->GetServiceDescriptor) return 0;
	if (!ifce->ServiceCommand) return 0;
	return 1;
}


static char *get_mime_type(GF_Terminal *term, const char *url, GF_Err *ret_code, GF_DownloadSession **the_session)
{
	char * ret = NULL;
	GF_DownloadSession * sess;
	(*ret_code) = GF_OK;
	if (strnicmp(url, "http", 4)) return NULL;

	/*don't use any NetIO and don't issue a HEAD command, always go for GET, use cache and store the session*/
	sess = gf_dm_sess_new(term->downloader, (char *) url, GF_NETIO_SESSION_NOT_THREADED , NULL, NULL, ret_code);
	if (!sess) {
		if (strstr(url, "rtsp://") || strstr(url, "rtp://") || strstr(url, "udp://") || strstr(url, "tcp://") ) (*ret_code) = GF_OK;
		return NULL;
	} else {
		/*start processing the resource, and stop if error or as soon as we get data*/
		while (1) {
			*ret_code = gf_dm_sess_process_headers(sess);
			if (*ret_code) break;
			if (gf_dm_sess_get_status(sess)>=GF_NETIO_DATA_EXCHANGE) {
				const char * mime = gf_dm_sess_mime_type(sess);
				/* The mime type is returned lower case */
				if (mime) {
					ret = gf_strdup(mime);
				}
				break;
			}
		}
	}

	if (the_session && (*ret_code == GF_OK)) {
		*the_session = sess;
	} else {
		gf_dm_sess_del(sess);
	}
	return ret;
}


static Bool check_extension(const char *szExtList, char *szExt)
{
	char szExt2[500];
	if (szExtList[0] != '"') return 0;
	szExtList += 1;

	while (1) {
		u32 i = 0;
		while ((szExtList[0] != ' ') && (szExtList[0] != '"')) {
			szExt2[i] = szExtList[0];
			i++;
			szExtList++;
		}
		szExt2[i] = 0;
		if (!strncmp(szExt, szExt2, strlen(szExt2))) return 1;
		if (szExtList[0]=='"') break;
		else szExtList++;
	}
	return 0;
}


static GF_InputService *gf_term_can_handle_service(GF_Terminal *term, const char *url, const char *parent_url, Bool no_mime_check, char **out_url, GF_Err *ret_code, GF_DownloadSession **the_session, char **out_mime_type)
{
	u32 i;
	GF_Err e;
	char *sURL, *qm, *frag, *ext, *mime_type, *url_res;
	char szExt[50];
	const char *force_module = NULL;
	GF_InputService *ifce;
	Bool skip_mime = 0;
	memset(szExt, 0, sizeof(szExt));

	(*ret_code) = GF_OK;
	ifce = NULL;
	mime_type = NULL;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Looking for plugin for URL %s\n", url));
	*out_url = NULL;
	*out_mime_type = NULL;
	sURL = NULL;
	if (!url || !strncmp(url, "\\\\", 2) ) {
		(*ret_code) = GF_URL_ERROR;
		goto exit;
	}

	if (!strnicmp(url, "libplayer://", 12)) {
		force_module = "LibPlayer";
	}

	/*used by GUIs scripts to skip URL concatenation*/
	if (!strncmp(url, "gpac://", 7)) sURL = gf_strdup(url+7);
	/*opera-style localhost URLs*/
	else if (!strncmp(url, "file://localhost", 16)) sURL = gf_strdup(url+16);

	else if (parent_url) sURL = gf_url_concatenate(parent_url, url);

	/*path absolute*/
	if (!sURL) sURL = gf_strdup(url);

	if (gf_url_is_local(sURL))
		gf_url_to_fs_path(sURL);

	if (the_session) *the_session = NULL;
	if (no_mime_check) {
		mime_type = NULL;
	} else {
		/*fetch a mime type if any. If error don't even attempt to open the service
		TRYTOFIXME: it would be nice to reuse the downloader created while fetching the mime type, however
		we don't know if the plugin will want it threaded or not....
		*/
		mime_type = get_mime_type(term, sURL, &e, the_session);
		if (e) {
			(*ret_code) = e;
			goto exit;
		}
	}

	if (mime_type &&
	        (!stricmp(mime_type, "text/plain")
	         || !stricmp(mime_type, "video/quicktime")
	         || !stricmp(mime_type, "application/octet-stream")
	        )
	   ) {
		skip_mime = 1;
	}

	ifce = NULL;

	/*load from mime type*/
	if (mime_type && !skip_mime) {
		const char *sPlug = gf_cfg_get_key(term->user->config, "MimeTypes", mime_type);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Mime type found: %s\n", mime_type));
		if (!sPlug) {
			*out_mime_type = mime_type;
			mime_type=NULL;
		}
		if (sPlug) sPlug = strrchr(sPlug, '"');
		if (sPlug) {
			sPlug += 2;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("%s:%d FOUND matching module %s\n", __FILE__, __LINE__, sPlug));
			ifce = (GF_InputService *) gf_modules_load_interface_by_name(term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
			if (force_module && ifce && !strstr(ifce->module_name, force_module)) {
				gf_modules_close_interface((GF_BaseInterface *) ifce);
				ifce = NULL;
			}
			if (ifce && !net_check_interface(ifce) ) {
				gf_modules_close_interface((GF_BaseInterface *) ifce);
				ifce = NULL;
			}
		}
	}

	/* The file extension, if any, is before '?' if any or before '#' if any.*/
	url_res = strrchr(sURL, '/');
	if (!url_res) url_res = strrchr(sURL, '\\');
	if (!url_res) url_res = sURL;
	qm = strchr(url_res, '?');
	if (qm) {
		qm[0] = 0;
		ext = strrchr(url_res, '.');
		qm[0] = '?';
	} else {
		frag = strchr(url_res, '#');
		if (frag) {
			frag[0] = 0;
			ext = strrchr(url_res, '.');
			frag[0] = '#';
		} else {
			ext = strrchr(url_res, '.');
		}
	}
	if (ext && !stricmp(ext, ".gz")) {
		char *anext;
		ext[0] = 0;
		anext = strrchr(sURL, '.');
		ext[0] = '.';
		ext = anext;
	}
	/*no mime type: either local or streaming. If streaming discard extension checking*/
	if (!ifce && !mime_type && strstr(sURL, "://") && strnicmp(sURL, "file://", 7)) ext = NULL;

	/*browse extensions for prefered module*/
	if (!ifce && ext) {
		u32 keyCount;
		strncpy(szExt, &ext[1], 49);
		ext = strrchr(szExt, '?');
		if (ext) ext[0] = 0;
		ext = strrchr(szExt, '#');
		if (ext) ext[0] = 0;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] No mime type found - checking by extension %s\n", szExt));
		assert( term && term->user && term->user->modules);
		keyCount = gf_cfg_get_key_count(term->user->config, "MimeTypes");
		for (i=0; i<keyCount; i++) {
			char *sPlug;
			const char *sKey;
			const char *sMime;
			sMime = gf_cfg_get_key_name(term->user->config, "MimeTypes", i);
			if (!sMime) continue;
			sKey = gf_cfg_get_key(term->user->config, "MimeTypes", sMime);
			if (!sKey) continue;
			if (!check_extension(sKey, szExt)) continue;
			sPlug = strrchr(sKey, '"');
			if (!sPlug) continue;	/*bad format entry*/
			sPlug += 2;

			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Trying module[%i]=%s, mime=%s\n", i, sPlug, sMime));
			ifce = (GF_InputService *) gf_modules_load_interface_by_name(term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
			if (!ifce) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] module[%i]=%s, mime=%s, cannot be loaded for GF_NET_CLIENT_INTERFACE.\n", i, sPlug, sMime));
				continue;
			}
			if (force_module && ifce && !strstr(ifce->module_name, force_module)) {
				gf_modules_close_interface((GF_BaseInterface *) ifce);
				ifce = NULL;
				continue;
			}
			if (ifce && !net_check_interface(ifce)) {
				gf_modules_close_interface((GF_BaseInterface *) ifce);
				ifce = NULL;
				continue;
			}
			break;
		}
	}

	/*browse all modules*/
	if (!ifce) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Not found any interface, trying browsing all modules...\n"));
		for (i=0; i< gf_modules_get_count(term->user->modules); i++) {
			ifce = (GF_InputService *) gf_modules_load_interface(term->user->modules, i, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Checking if module %s supports URL %s\n", ifce->module_name, sURL));
			if (force_module && ifce && !strstr(ifce->module_name, force_module)) {
			}
			else if (net_check_interface(ifce) && ifce->CanHandleURL(ifce, sURL)) {
				break;
			}
			gf_modules_close_interface((GF_BaseInterface *) ifce);
			ifce = NULL;
		}
	}
exit:
	if (!ifce) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Did not find any input plugin for URL %s (%s)\n", sURL ? sURL : url, mime_type ? mime_type : "no mime type"));
		if (sURL) gf_free(sURL);
		if ( (*ret_code) == GF_OK) (*ret_code) = GF_NOT_SUPPORTED;
		*out_url = NULL;

		if (the_session && *the_session) {
			gf_dm_sess_del(*the_session);
		}
		if (mime_type) gf_free(mime_type);
		mime_type = NULL;
		if (*out_mime_type) gf_free(*out_mime_type);
		*out_mime_type = NULL;
	} else {
		*out_url = sURL;
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Terminal] Found input plugin %s for URL %s (%s)\n", ifce->module_name, sURL, mime_type ? mime_type : "no mime type"));
	}
	if (mime_type)
		*out_mime_type = mime_type;
	return ifce;
}

GF_ClientService *gf_term_service_new(GF_Terminal *term, struct _od_manager *owner, const char *url, const char *parent_url, GF_Err *ret_code)
{
	GF_DownloadSession *download_session = NULL;
	char *sURL;
	char *mime;
	GF_ClientService *serv;
	GF_InputService *ifce = gf_term_can_handle_service(term, url, parent_url, 0, &sURL, ret_code, &download_session, &mime);
	if (!ifce) {
		if (owner->subscene) gf_scene_notify_event(owner->subscene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, *ret_code, GF_FALSE);
		return NULL;
	}
	GF_SAFEALLOC(serv, GF_ClientService);
	serv->term = term;
	serv->owner = owner;
	serv->ifce = ifce;
	serv->url = sURL;
	serv->mime = mime;
	serv->Clocks = gf_list_new();
	serv->dnloads = gf_list_new();
	serv->pending_service_session = download_session;

	gf_list_add(term->net_services, serv);


	serv->fn_connect_ack = term_on_connect;
	serv->fn_disconnect_ack = term_on_disconnect;
	serv->fn_command = term_on_command;
	serv->fn_data_packet = term_on_data_packet;
	serv->fn_add_media = term_on_media_add;
	return serv;
}

GF_EXPORT
Bool gf_term_is_supported_url(GF_Terminal *term, const char *fileName, Bool use_parent_url, Bool no_mime_check)
{
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
	return 1;
}


Bool gf_term_service_can_handle_url(GF_ClientService *ns, char *url)
{
	/*if no owner attached the service is being deleted, don't query it*/
	if (!ns->owner || !ns->ifce->CanHandleURLInService) return 0;
	return ns->ifce->CanHandleURLInService(ns->ifce, url);
}

GF_Err gf_term_service_command(GF_ClientService *ns, GF_NetworkCommand *com)
{
	if (ns) return ns->ifce->ServiceCommand(ns->ifce, com);
	return GF_OK;
}
GF_Err gf_term_channel_get_sl_packet(GF_ClientService *ns, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	if (!ns->ifce->ChannelGetSLP) return GF_NOT_SUPPORTED;
	return ns->ifce->ChannelGetSLP(ns->ifce, channel, out_data_ptr, out_data_size, out_sl_hdr, sl_compressed, out_reception_status, is_new_data);
}
GF_Err gf_term_channel_release_sl_packet(GF_ClientService *ns, LPNETCHANNEL channel)
{
	if (!ns->ifce->ChannelGetSLP) return GF_NOT_SUPPORTED;
	return ns->ifce->ChannelReleaseSLP(ns->ifce, channel);
}

GF_EXPORT
void gf_service_connect_ack(GF_ClientService *service, LPNETCHANNEL ns, GF_Err response)
{
	assert(service);
	service->fn_connect_ack(service, ns, response);
}

GF_EXPORT
void gf_service_disconnect_ack(GF_ClientService *service, LPNETCHANNEL ns, GF_Err response)
{
	assert(service);
	service->fn_disconnect_ack(service, ns, response);
}
GF_EXPORT
void gf_service_command(GF_ClientService *service, GF_NetworkCommand *com, GF_Err response)
{
	assert(service);
	service->fn_command(service, com, response);
}

GF_EXPORT
void gf_service_send_packet(GF_ClientService *service, LPNETCHANNEL ns, char *data, u32 data_size, GF_SLHeader *hdr, GF_Err reception_status)
{
	assert(service);
	service->fn_data_packet(service, ns, data, data_size, hdr, reception_status);
}

GF_EXPORT
void gf_service_declare_media(GF_ClientService *service, GF_Descriptor *media_desc, Bool no_scene_check)
{
	assert(service);
	service->fn_add_media(service, media_desc, no_scene_check);
}

GF_EXPORT
const char *gf_service_get_url(GF_ClientService *service)
{
	if (!service) return NULL;
	return service->url;
}

void gf_term_delete_net_service(GF_ClientService *ns)
{
	const char *sOpt = gf_cfg_get_key(ns->term->user->config, "StreamingCache", "AutoSave");
	if (ns->cache) gf_term_service_cache_close(ns, (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);

	if (ns->pending_service_session) gf_dm_sess_del(ns->pending_service_session);

	assert(!ns->nb_odm_users);
	assert(!ns->nb_ch_users);
	assert(!ns->owner);

	gf_modules_close_interface((GF_BaseInterface *)ns->ifce);
	gf_free(ns->url);
	gf_free(ns->mime);


	/*delete all the clocks*/
	while (gf_list_count(ns->Clocks)) {
		GF_Clock *ck = (GF_Clock *)gf_list_get(ns->Clocks, 0);
		gf_list_rem(ns->Clocks, 0);
		gf_clock_del(ck);
	}
	gf_list_del(ns->Clocks);

	assert(!gf_list_count(ns->dnloads));
	gf_list_del(ns->dnloads);
	gf_free(ns);
}

GF_EXPORT
GF_InputService *gf_service_get_interface(GF_ClientService *serv)
{
	return serv ? serv->ifce : NULL;
}

GF_EXPORT
GF_DownloadSession *gf_service_download_new(GF_ClientService *service, const char *url, u32 flags, gf_dm_user_io user_io, void *cbk)
{
	GF_Err e;
	GF_DownloadSession * sess;
	char *sURL, *orig_url;
	if (!service) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] service is null, cannot create new download session for %s.\n", url));
		return NULL;
	}

	sURL = gf_url_concatenate(service->url, url);
	/*path was absolute*/
	if (!sURL) sURL = gf_strdup(url);
	assert( service->term );

	orig_url = service->pending_service_session ? (char *) gf_dm_sess_get_original_resource_name(service->pending_service_session) : NULL;
	/*this will take care of URL formatting (%20 etc ..) */
	if (orig_url) orig_url = gf_url_concatenate(service->url, orig_url);

	if (orig_url && !strcmp(orig_url, sURL)) {
		sess = service->pending_service_session;
		service->pending_service_session = NULL;
		/*resetup*/
		gf_dm_sess_reassign(sess, flags, user_io, cbk);
	} else {
		sess = gf_dm_sess_new(service->term->downloader, sURL, flags, user_io, cbk, &e);
	}
	if (orig_url) gf_free(orig_url);

	if (!sess) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_NETWORK, ("[HTTP] session could not be created for %s : %s. service url=%s, url=%s.\n", sURL, gf_error_to_string(e), service->url, url));
		gf_free(sURL);
		sURL = NULL;
		return NULL;
	}
	gf_free(sURL);
	sURL = NULL;
	gf_dm_sess_set_private(sess, service);
	gf_list_add(service->dnloads, sess);
	return sess;
}

GF_EXPORT
void gf_service_download_del(GF_DownloadSession * sess)
{
	Bool locked;
	GF_ClientService *serv;
	if (!sess) return;
	serv = (GF_ClientService *)gf_dm_sess_get_private(sess);

	/*avoid sending data back to user*/
	gf_dm_sess_abort(sess);

	locked = gf_mx_try_lock(serv->term->media_queue_mx);

	/*unregister from service*/
	gf_list_del_item(serv->dnloads, sess);

	/*same as service: this may be called in the downloader thread (typically when download fails)
	so we must queue the downloader and let the term delete it later on*/
	gf_list_add(serv->term->net_services_to_remove, sess);

	if (locked)
		gf_term_lock_media_queue(serv->term, 0);
}

GF_EXPORT
void gf_service_download_update_stats(GF_DownloadSession * sess)
{
	GF_ClientService *serv;
	const char *szURI;
	u32 total_size, bytes_done, bytes_per_sec;
	GF_NetIOStatus net_status;

	if (!sess) return;

	gf_dm_sess_get_stats(sess, NULL, &szURI, &total_size, &bytes_done, &bytes_per_sec, &net_status);
	serv = (GF_ClientService *)gf_dm_sess_get_private(sess);
	switch (net_status) {
	case GF_NETIO_SETUP:
		gf_term_message(serv->term, serv->url, "Connecting", GF_OK);
		break;
	case GF_NETIO_CONNECTED:
		gf_term_message(serv->term, serv->url, "Connected", GF_OK);
		break;
	case GF_NETIO_WAIT_FOR_REPLY:
		gf_term_message(serv->term, serv->url, "Waiting for reply...", GF_OK);
		break;
	case GF_NETIO_PARSE_REPLY:
		gf_term_message(serv->term, serv->url, "Starting download...", GF_OK);
		break;
	case GF_NETIO_DATA_EXCHANGE:
		/*notify some connection / ...*/
		if (total_size) {
			GF_Event evt;
			evt.type = GF_EVENT_PROGRESS;
			evt.progress.progress_type = 1;
			evt.progress.service = szURI;
			evt.progress.total = total_size;
			evt.progress.done = bytes_done;
			evt.progress.bytes_per_seconds = bytes_per_sec;
			gf_term_send_event(serv->term, &evt);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] %s received %d / %d\n", szURI, bytes_done, total_size));
		gf_term_service_media_event_with_download(serv->owner, GF_EVENT_MEDIA_PROGRESS, bytes_done, total_size, bytes_per_sec);
		break;
	case GF_NETIO_DATA_TRANSFERED:
		gf_term_service_media_event(serv->owner, GF_EVENT_MEDIA_LOAD_DONE);
		if (serv->owner && !(serv->owner->flags & GF_ODM_DESTROYED) && serv->owner->duration) {
			GF_Clock *ck = gf_odm_get_media_clock(serv->owner);
			if (!gf_clock_is_started(ck)) {
				GF_LOG(GF_LOG_INFO, GF_LOG_NETWORK, ("[HTTP Resource] Done retrieving file - resuming playback\n"));
				if (serv->is_paused) {
					serv->is_paused = 0;
#ifndef GPAC_DISABLE_VRML
					mediacontrol_resume(serv->owner);
#endif
				}
			}
		}
		break;
	default:
		break;
	}
}

void gf_term_service_del(GF_ClientService *ns)
{
	/*this is a downloader session*/
	if (! * (u32 *) ns) {
		gf_dm_sess_del((GF_DownloadSession * ) ns);
	} else {
		gf_term_delete_net_service(ns);
	}
}

GF_EXPORT
void gf_service_register_mime(const GF_InputService *ifce, const char *mimeType, const char *extList, const char *description)
{
	u32 len;
	char *buf;
	if (!ifce || !mimeType || !extList || !description) return;

	len = (u32) strlen(extList) + 3 + (u32) strlen(description) + 3 + (u32) strlen(ifce->module_name) + 1;
	buf = (char*)gf_malloc(sizeof(char)*len);
	sprintf(buf, "\"%s\" ", extList);
	strlwr(buf);
	strcat(buf, "\"");
	strcat(buf, description);
	strcat(buf, "\" ");
	strcat(buf, ifce->module_name);
	gf_modules_set_option((GF_BaseInterface *)(GF_BaseInterface *)ifce, "MimeTypes", mimeType, buf);
	gf_free(buf);
}

GF_EXPORT
Bool gf_service_check_mime_register(GF_InputService *ifce, const char *mimeType, const char *extList, const char *description, const char *fileExt)
{
	const char *szExtList;
	char *ext, szExt[500];
	if (!ifce || !mimeType || !extList || !description || !fileExt) return 0;
	memset(szExt, 0, sizeof(szExt));
	/*this is a URL*/
	if ( (strlen(fileExt)>20) || strchr(fileExt, '/')) return 0;

	if (fileExt[0]=='.') fileExt++;
	strcpy(szExt, fileExt);
	strlwr(szExt);
	ext = strchr(szExt, '#');
	if (ext) ext[0]=0;

	szExtList = gf_modules_get_option((GF_BaseInterface *)(GF_BaseInterface *)ifce, "MimeTypes", mimeType);
	if (!szExtList) {
		gf_service_register_mime(ifce, mimeType, extList, description);
		szExtList = gf_modules_get_option((GF_BaseInterface *)(GF_BaseInterface *)ifce, "MimeTypes", mimeType);
	}
	if (!strstr(szExtList, ifce->module_name)) return 0;
	return check_extension((char *)szExtList, szExt);
}

GF_Err gf_term_service_cache_load(GF_ClientService *ns)
{
	GF_Err e;
	const char *sOpt;
	char szName[GF_MAX_PATH], szURL[1024];
	GF_NetworkCommand com;
	u32 i;
	GF_StreamingCache *mcache = NULL;

	/*is service cachable*/
	com.base.on_channel = NULL;
	com.base.command_type = GF_NET_IS_CACHABLE;
	if (ns->ifce->ServiceCommand(ns->ifce, &com) != GF_OK) return GF_OK;

	/*locate a cache*/
	for (i=0; i< gf_modules_get_count(ns->term->user->modules); i++) {
		mcache = (GF_StreamingCache *) gf_modules_load_interface(ns->term->user->modules, i, GF_STREAMING_MEDIA_CACHE);
		if (mcache && mcache->Open && mcache->Close && mcache->Write && mcache->ChannelGetSLP && mcache->ChannelReleaseSLP && mcache->ServiceCommand) break;
		if (mcache) gf_modules_close_interface((GF_BaseInterface *)mcache);
		mcache = NULL;
	}
	if (!mcache) return GF_NOT_SUPPORTED;

	sOpt = gf_cfg_get_key(ns->term->user->config, "StreamingCache", "RecordDirectory");
	if (!sOpt) sOpt = gf_cfg_get_key(ns->term->user->config, "General", "CacheDirectory");
	if (sOpt) {
		strcpy(szName, sOpt);
		if (szName[strlen(szName)-1]!='\\') strcat(szName, "\\");
	} else {
		strcpy(szName, "");
	}
	sOpt = gf_cfg_get_key(ns->term->user->config, "StreamingCache", "BaseFileName");
	if (sOpt) {
		strcat(szName, sOpt);
	} else {
		char *sep;
		strcat(szName, "rec_");
		sOpt = strrchr(ns->url, '/');
		if (!sOpt) sOpt = strrchr(ns->url, '\\');
		if (sOpt) sOpt += 1;
		else {
			sOpt = strstr(ns->url, "://");
			if (sOpt) sOpt += 3;
			else sOpt = ns->url;
		}
		strcpy(szURL, sOpt);
		sep = strrchr(szURL, '.');
		if (sep) sep[0] = 0;
		for (i=0; i<strlen(szURL); i++) {
			switch (szURL[i]) {
			case '/':
			case '\\':
			case '.':
			case ':':
			case '?':
				szURL[i] = '_';
				break;
			}
		}
		strcat(szName, szURL);
	}

	sOpt = gf_cfg_get_key(ns->term->user->config, "StreamingCache", "KeepExistingFiles");
	e = mcache->Open(mcache, ns, szName, (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	if (e) {
		gf_modules_close_interface((GF_BaseInterface *)mcache);
		return e;
	}
	ns->cache = mcache;
	return GF_OK;
}

GF_Err gf_term_service_cache_close(GF_ClientService *ns, Bool no_save)
{
	GF_Err e;
	if (!ns->cache) return GF_OK;
	e = ns->cache->Close(ns->cache, no_save);

	gf_modules_close_interface((GF_BaseInterface *)ns->cache);
	ns->cache = NULL;
	return e;
}
