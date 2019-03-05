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



/*for OD service types*/
#include <gpac/constants.h>
/*for URL concatenation*/
#include <gpac/network.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/internal/compositor_dev.h>
#include "media_control.h"
#include <gpac/nodes_x3d.h>
#include <gpac/options.h>

/*SVG properties*/
#ifndef GPAC_DISABLE_SVG
#include <gpac/scenegraph_svg.h>
#endif

#include "input_sensor.h"
#include "media_memory.h"

GF_EXPORT
Double gf_scene_get_time(void *_is)
{
	GF_Scene *scene = (GF_Scene *)_is;
#if 1
	u32 ret;
	GF_Clock *ck;
	assert(scene);
	ck = scene->scene_codec ? scene->scene_codec->ck : scene->dyn_ck;
	if (!ck) return 0.0;
	ret = gf_clock_time(ck);
	if (scene->root_od->media_stop_time && (scene->root_od->media_stop_time<ret)) ret = (u32) scene->root_od->media_stop_time;
	return ret/1000.0;
#else
	return scene->simulation_time;
#endif
}

#ifndef GPAC_DISABLE_VRML

void gf_storage_save(M_Storage *storage);
#endif

static void inline_on_media_event(GF_Scene *scene, u32 type)
{
	gf_term_service_media_event(scene->scene_codec->odm, type);
}

GF_EXPORT
GF_Scene *gf_scene_new(GF_Scene *parentScene)
{
	GF_Scene *tmp;
	GF_SAFEALLOC(tmp, GF_Scene);
	if (! tmp) return NULL;

	tmp->mx_resources = gf_mx_new("SceneResources");
	tmp->resources = gf_list_new();
	tmp->scene_objects = gf_list_new();
	tmp->extra_scenes = gf_list_new();
	tmp->declared_addons = gf_list_new();
	/*init inline scene*/
	if (parentScene) {
		tmp->graph = gf_sg_new_subscene(parentScene->graph);
	} else {
		tmp->graph = gf_sg_new();
	}

	gf_sg_set_private(tmp->graph, tmp);
	gf_sg_set_node_callback(tmp->graph, gf_term_node_callback);
	gf_sg_set_scene_time_callback(tmp->graph, gf_scene_get_time);

	//copy over pause_at_first_frame flag so that new subscene is not paused right away
	if (parentScene)
		tmp->first_frame_pause_type = parentScene->first_frame_pause_type;

#ifndef GPAC_DISABLE_VRML
	tmp->extern_protos = gf_list_new();
	gf_sg_set_proto_loader(tmp->graph, gf_inline_get_proto_lib);

	tmp->storages = gf_list_new();
	tmp->keynavigators = gf_list_new();

#endif
	tmp->on_media_event = inline_on_media_event;
	return tmp;
}

static void gf_scene_reset_urls(GF_Scene *scene)
{
#define SFURL_RESET(__url) if (__url.url) gf_free(__url.url);\
	memset(&__url, 0, sizeof(SFURL));

	SFURL_RESET(scene->audio_url);
	SFURL_RESET(scene->visual_url);
	SFURL_RESET(scene->text_url);
	SFURL_RESET(scene->dims_url);
}

GF_EXPORT
void gf_scene_del(GF_Scene *scene)
{
	gf_mx_p(scene->root_od->term->net_mx);

	gf_list_del(scene->resources);
	assert(!gf_list_count(scene->extra_scenes) );
	gf_list_del(scene->extra_scenes);

#ifndef GPAC_DISABLE_VRML
	while (gf_list_count(scene->extern_protos)) {
		GF_ProtoLink *pl = (GF_ProtoLink *)gf_list_get(scene->extern_protos, 0);
		gf_list_rem(scene->extern_protos, 0);
		gf_free(pl);
	}
	gf_list_del(scene->extern_protos);
#endif

	/*delete scene decoder */
	if (scene->scene_codec) {
		GF_SceneDecoder *dec = (GF_SceneDecoder *)scene->scene_codec->decio;
		/*make sure the scene codec doesn't have anything left in the scene graph*/
		if (dec->ReleaseScene) dec->ReleaseScene(dec);

		gf_term_remove_codec(scene->root_od->term, scene->scene_codec);
		gf_codec_del(scene->scene_codec);
		/*reset pointer to NULL in case nodes try to access scene time*/
		scene->scene_codec = NULL;
	}

	/*delete the scene graph*/
	gf_sg_del(scene->graph);

	if (scene->od_codec) {
		gf_term_remove_codec(scene->root_od->term, scene->od_codec);
		gf_codec_del(scene->od_codec);
		scene->od_codec = NULL;
	}
	/*don't touch the root_od, will be deleted by the parent scene*/

	/*clean all remaining associations*/
	while (gf_list_count(scene->scene_objects)) {
		GF_MediaObject *obj = (GF_MediaObject *)gf_list_get(scene->scene_objects, 0);
		if (obj->odm) obj->odm->mo = NULL;
		gf_list_rem(scene->scene_objects, 0);
		gf_sg_vrml_mf_reset(&obj->URLs, GF_SG_VRML_MFURL);
		gf_mo_del(obj);
	}
	gf_list_del(scene->scene_objects);
#ifndef GPAC_DISABLE_VRML
	gf_list_del(scene->storages);
	gf_list_del(scene->keynavigators);
#endif

	gf_list_del(scene->declared_addons);

	gf_scene_reset_urls(scene);

	if (scene->fragment_uri) gf_free(scene->fragment_uri);
	if (scene->redirect_xml_base) gf_free(scene->redirect_xml_base);

	gf_mx_v(scene->root_od->term->net_mx);
	gf_mx_del(scene->mx_resources);
	gf_free(scene);
}

GF_EXPORT
GF_ObjectManager *gf_scene_find_odm(GF_Scene *scene, u16 OD_ID)
{
	GF_ObjectManager *odm;
	u32 i=0;
	while ((odm = (GF_ObjectManager *)gf_list_enum(scene->resources, &i))) {
		if (odm->OD && odm->OD->objectDescriptorID == OD_ID) return odm;
	}
	return NULL;
}

GF_EXPORT
void gf_scene_disconnect(GF_Scene *scene, Bool for_shutdown)
{
	u32 i;
	GF_MediaObject *obj;
	GF_ObjectManager *odm;
	GF_SceneDecoder *dec = NULL;
	if (scene->scene_codec) dec = (GF_SceneDecoder *)scene->scene_codec->decio;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Scene] disconnecting\n"));

	gf_term_lock_compositor(scene->root_od->term, GF_TRUE);

	/*force unregistering of inline nodes (for safety)*/
	if (for_shutdown && scene->root_od->mo) {
		/*reset private stack of all inline nodes still registered*/
		while (gf_mo_event_target_count(scene->root_od->mo)) {
			GF_Node *n = (GF_Node *)gf_event_target_get_node(gf_mo_event_target_get(scene->root_od->mo, 0));
			gf_mo_event_target_remove_by_index(scene->root_od->mo, 0);
#ifndef GPAC_DISABLE_VRML
			switch (gf_node_get_tag(n)) {
			case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
			case TAG_X3D_Inline:
#endif
				gf_node_set_private(n, NULL);
				break;
			}
#endif
		}
	}

	//Ivica patch: Remove all Registered InputSensor nodes -> shut down the InputSensor threads -> prevent illegal access on deleted pointers
#ifndef GPAC_DISABLE_VRML
	if (for_shutdown) {
		i = 0;
		while ((odm = (GF_ObjectManager *)gf_list_enum(scene->resources, &i))) {
			if (for_shutdown && odm->mo) {
				obj = odm->mo;
				while (gf_mo_event_target_count(obj)) {
					GF_Node *n = (GF_Node *)gf_event_target_get_node(gf_mo_event_target_get(obj, 0));
					if (n) {
						switch (gf_node_get_tag(n)) {
						case TAG_MPEG4_InputSensor:
						{
							M_InputSensor* is = (M_InputSensor*)n;
							is->enabled = 0;
							InputSensorModified(n);
							break;
						}
						}
					}
					gf_mo_event_target_remove_by_index(obj, 0);
				}
			}
		}
	}
#endif

	/*remove all associated eventTargets*/
	i=0;
	while ((obj = (GF_MediaObject *)gf_list_enum(scene->scene_objects, &i))) {
		gf_mo_event_target_reset(obj);
	}

#ifndef GPAC_DISABLE_VRML
	while (gf_list_count(scene->storages)) {
		M_Storage *storage = (M_Storage *)gf_list_get(scene->storages, 0);
		gf_list_rem(scene->storages, 0);
		if (storage->_auto) gf_storage_save(storage);
	}
#endif

	if (scene->root_od->term->root_scene == scene) {
		gf_sc_set_scene(scene->root_od->term->compositor, NULL);
	}

	gf_scene_reset_addons(scene);

	/*release the scene - at this stage, we no longer have any node stack refering to our media objects */
	if (dec && dec->ReleaseScene) dec->ReleaseScene(dec);
	gf_sc_node_destroy(scene->root_od->term->compositor, NULL, scene->graph);
	gf_sg_reset(scene->graph);
	scene->graph_attached = 0;

	/*reset statc ressource flag since we destroyed scene objects*/
	scene->static_media_ressources = GF_FALSE;


	/*disconnect and kill all objects*/
	if (!for_shutdown && scene->static_media_ressources) {
		i=0;
		/*stop all objects but DON'T DESTROY THEM*/
		while ((odm = (GF_ObjectManager *)gf_list_enum(scene->resources, &i))) {
			if (odm->state) gf_odm_disconnect(odm, GF_FALSE);
		}
		/*reset all stream associations*/
		i=0;
		while ((obj = (GF_MediaObject*)gf_list_enum(scene->scene_objects, &i))) {
			gf_sg_vrml_mf_reset(&obj->URLs, GF_SG_VRML_MFURL);
			gf_mo_event_target_reset(obj);
		}
	} else {
		while (gf_list_count(scene->resources)) {
			odm = (GF_ObjectManager *)gf_list_get(scene->resources, 0);
			gf_odm_disconnect(odm, (for_shutdown || !scene->static_media_ressources) ? 2 : 0);
		}
#ifndef GPAC_DISABLE_VRML
		while (gf_list_count(scene->extern_protos)) {
			GF_ProtoLink *pl = (GF_ProtoLink *)gf_list_get(scene->extern_protos, 0);
			gf_list_rem(scene->extern_protos, 0);
			gf_free(pl);
		}
#endif
	}

	/*remove stream associations*/
	while (gf_list_count(scene->scene_objects)) {
		obj = (GF_MediaObject*)gf_list_get(scene->scene_objects, 0);
		gf_list_rem(scene->scene_objects, 0);
		if (obj->odm) obj->odm->mo = NULL;
		gf_sg_vrml_mf_reset(&obj->URLs, GF_SG_VRML_MFURL);
		gf_mo_del(obj);
	}

	//reset URLs
	gf_scene_reset_urls(scene);

	scene->object_attached = 0;
	gf_term_lock_compositor(scene->root_od->term, GF_FALSE);
}

static void gf_scene_insert_object(GF_Scene *scene, GF_MediaObject *mo, Bool lock_timelines, GF_MediaObject *sync_ref, Bool keep_fragment, GF_Scene *original_parent_scene)
{
	GF_ObjectManager *root_od;
	GF_ObjectManager *odm;
	char *url;
	if (!mo || !scene) return;

	odm = gf_odm_new();
	/*remember OD*/
	odm->mo = mo;
	mo->odm = odm;
	odm->parentscene = scene;
	odm->OD = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	odm->OD->objectDescriptorID = GF_MEDIA_EXTERNAL_ID;
	odm->parentscene = scene;
	odm->term = scene->root_od->term;
	root_od = scene->root_od;
	if (scene->force_single_timeline) lock_timelines = GF_TRUE;

	url = mo->URLs.vals[0].url;
	if (url) {
		if (!stricmp(url, "KeySensor")) {
			GF_ESD *esd = gf_odf_desc_esd_new(0);
			esd->decoderConfig->streamType = GF_STREAM_INTERACT;
			esd->decoderConfig->objectTypeIndication = 1;
			gf_free(esd->decoderConfig->decoderSpecificInfo->data);
			esd->decoderConfig->decoderSpecificInfo->data = gf_strdup(" KeySensor");
			esd->decoderConfig->decoderSpecificInfo->data[0] = 9;
			esd->decoderConfig->decoderSpecificInfo->dataLength = 10;
			esd->ESID = esd->OCRESID = 65534;
			gf_list_add(odm->OD->ESDescriptors, esd);
		} else if (!stricmp(url, "StringSensor")) {
			GF_ESD *esd = gf_odf_desc_esd_new(0);
			esd->decoderConfig->streamType = GF_STREAM_INTERACT;
			esd->decoderConfig->objectTypeIndication = 1;
			gf_free(esd->decoderConfig->decoderSpecificInfo->data);
			esd->decoderConfig->decoderSpecificInfo->data = gf_strdup(" StringSensor");
			esd->decoderConfig->decoderSpecificInfo->data[0] = 12;
			esd->decoderConfig->decoderSpecificInfo->dataLength = 13;
			esd->ESID = esd->OCRESID = 65534;
			gf_list_add(odm->OD->ESDescriptors, esd);
		} else if (!stricmp(url, "Mouse")) {
			GF_ESD *esd = gf_odf_desc_esd_new(0);
			esd->decoderConfig->streamType = GF_STREAM_INTERACT;
			esd->decoderConfig->objectTypeIndication = 1;
			gf_free(esd->decoderConfig->decoderSpecificInfo->data);
			esd->decoderConfig->decoderSpecificInfo->data = gf_strdup(" Mouse");
			esd->decoderConfig->decoderSpecificInfo->data[0] = 5;
			esd->decoderConfig->decoderSpecificInfo->dataLength = 6;
			esd->ESID = esd->OCRESID = 65534;
			gf_list_add(odm->OD->ESDescriptors, esd);
		} else {
			if (!keep_fragment) {
				char *frag = strrchr(mo->URLs.vals[0].url, '#');
				if (frag) frag[0] = 0;
				odm->OD->URLString = gf_strdup(mo->URLs.vals[0].url);
				if (frag) frag[0] = '#';
			} else {
				odm->OD->URLString = gf_strdup(mo->URLs.vals[0].url);
			}
			if (lock_timelines) odm->flags |= GF_ODM_INHERIT_TIMELINE;
		}
	}

	/*HACK - temp storage of sync ref*/
	if (sync_ref) odm->ocr_codec = (struct _generic_codec *)sync_ref;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Scene] Inserting new MediaObject %08x for resource %s\n", odm->mo, url));
	gf_mx_p(scene->mx_resources);
	gf_list_add(scene->resources, odm);
	gf_mx_v(scene->mx_resources);
	if (original_parent_scene) {
		gf_odm_setup_object(odm, original_parent_scene->root_od->net_service);
	} else {
		gf_odm_setup_object(odm, root_od->net_service);
	}
}

static void gf_scene_reinsert_object(GF_Scene *scene, GF_MediaObject *mo)
{
	u32 i;
	gf_free(mo->URLs.vals[0].url);
	mo->URLs.vals[0].url = NULL;
	for (i=0; i<mo->URLs.count-1; i++) mo->URLs.vals[i].url = mo->URLs.vals[i+1].url;
	mo->URLs.vals[mo->URLs.count-1].url = NULL;
	mo->URLs.count-=1;
	/*FIXME - we should re-ananlyse whether the fragment is important or not ...*/
	gf_scene_insert_object(scene, mo, GF_FALSE, NULL, GF_FALSE, NULL);
}


void gf_scene_remove_object(GF_Scene *scene, GF_ObjectManager *odm, u32 for_shutdown)
{
	u32 i;
	GF_MediaObject *obj;

	gf_term_lock_net(odm->term, GF_TRUE);
	gf_list_del_item(scene->resources, odm);
	gf_term_lock_net(odm->term, GF_FALSE);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Scene] removing ODM %d\n", odm->OD ? odm->OD->objectDescriptorID : GF_MEDIA_EXTERNAL_ID));


	i=0;
	while ((obj = (GF_MediaObject*)gf_list_enum(scene->scene_objects, &i))) {
		if (
		    /*assigned object*/
		    (obj->odm==odm) ||
		    /*remote OD*/
		    ((obj->OD_ID!=GF_MEDIA_EXTERNAL_ID) && odm->OD && (obj->OD_ID == odm->OD->objectDescriptorID) ) ||
		    /*dynamic OD*/
		    (obj->URLs.count && odm->OD && odm->OD->URLString && !stricmp(obj->URLs.vals[0].url, odm->OD->URLString))
		) {
			u32 discard_obj = 0;
			gf_odm_lock(odm, 1);
			obj->flags = 0;
			if (obj->odm) obj->odm->mo = NULL;
			odm->mo = NULL;
			obj->odm = NULL;

			obj->frame = NULL;
			obj->framesize = obj->timestamp = 0;

			gf_odm_lock(odm, 0);

			/*if graph not attached we can remove the link (this is likely scene shutdown for some error)*/
			if (!scene->graph_attached) {
#ifndef GPAC_DISABLE_VRML
				GF_ProtoLink *pl;
				u32 j=0;
				while ((pl = (GF_ProtoLink *)gf_list_enum(scene->extern_protos, &j))) {
					if (pl->mo==obj) {
						pl->mo = NULL;
						break;
					}
				}
#endif
				discard_obj = 1;
			} else if (!for_shutdown) {
				/*if dynamic OD and more than 1 URLs resetup*/
				if ((obj->OD_ID==GF_MEDIA_EXTERNAL_ID) && (obj->URLs.count>1)) {
					discard_obj = 0;
					gf_scene_reinsert_object(scene, obj);
				} else {
					discard_obj = 2;
				}
			}
			/*discard media object*/
			else if (for_shutdown==2)
				discard_obj = 1;

			/*reset private stack of all inline nodes still registered*/
			if (discard_obj) {
				while (gf_mo_event_target_count(obj)) {
					GF_Node *n = (GF_Node *)gf_event_target_get_node(gf_mo_event_target_get(obj, 0));
					gf_mo_event_target_remove_by_index(obj, 0);
#ifndef GPAC_DISABLE_VRML
					switch (gf_node_get_tag(n)) {
					case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
					case TAG_X3D_Inline:
#endif
						if (obj->num_open) gf_mo_stop(obj);
						gf_node_set_private(n, NULL);
						break;
					}
#endif
				}
			}

			if ((discard_obj==1) && !obj->num_open) {
				gf_list_rem(scene->scene_objects, i-1);
				gf_sg_vrml_mf_reset(&obj->URLs, GF_SG_VRML_MFURL);
				gf_mo_del(obj);
			}
			return;
		}
	}
}


//browse all channels and update buffering info
void gf_scene_buffering_info(GF_Scene *scene)
{
	u32 i, j, max_buffer, cur_buffer, max_buff_val=0;
	GF_Channel *ch;
	GF_Event evt;
	GF_ObjectManager *odm;
	if (!scene) return;

	max_buffer = cur_buffer = 0;

	/*get buffering on root OD*/
	j=0;
	while ((ch = (GF_Channel*)gf_list_enum(scene->root_od->channels, &j))) {
		/*count only re-buffering channels*/
		if (!ch->BufferOn) continue;

		max_buffer += ch->MaxBuffer;
		cur_buffer += (ch->BufferTime>0) ? ch->BufferTime : 1;
	}

	/*get buffering on all ODs*/
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
		if (!odm->codec) continue;
		j=0;
		while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &j))) {
			if (max_buff_val < ch->MaxBuffer) max_buff_val = ch->MaxBuffer;

			/*count only re-buffering channels*/
			if (!ch->BufferOn) continue;
			if (ch->MaxBuffer) {
				max_buffer += ch->MaxBuffer;
				cur_buffer += (ch->BufferTime>0) ? ch->BufferTime : 0;
			}
		}
	}

	evt.type = GF_EVENT_PROGRESS;
	evt.progress.progress_type = 0;
	evt.progress.service = scene->root_od->net_service->url;
	if (!max_buffer || !cur_buffer || (max_buffer <= cur_buffer)) {
		if (!max_buffer) max_buffer=max_buff_val;
		evt.progress.done = evt.progress.total = max_buffer;
	} else {
		evt.progress.done = cur_buffer;
		evt.progress.total = max_buffer;
	}
	gf_term_send_event(scene->root_od->term, &evt);
}



void gf_scene_notify_event(GF_Scene *scene, u32 event_type, GF_Node *n, void *_event, GF_Err code, Bool no_queueing)
{
	/*fire resize event*/
#ifndef GPAC_DISABLE_SVG
	GF_Node *root;
	u32 i, count;
	u32 w, h;
	GF_DOM_Event evt, *dom_event;
	dom_event = (GF_DOM_Event *)_event;

	if (!scene) return;
	root = gf_sg_get_root_node(scene->graph);

	if (!dom_event) {
		memset(&evt, 0, sizeof(GF_DOM_Event));
		dom_event = &evt;
		w = h = 0;
		gf_sg_get_scene_size_info(scene->graph, &w, &h);
		evt.type = event_type;
		evt.screen_rect.width = INT2FIX(w);
		evt.screen_rect.height = INT2FIX(h);
		evt.key_flags = scene->is_dynamic_scene ? (scene->vr_type ? 2 : 1) : 0;
		if (root) {
#ifndef GPAC_DISABLE_VRML
			switch (gf_node_get_tag(root)) {
			case TAG_MPEG4_Group:
			case TAG_MPEG4_Layer3D:
				evt.detail = 1;
				break;
#ifndef GPAC_DISABLE_X3D
			case TAG_X3D_Group:
				evt.detail = 2;
				break;
#endif
			}
#endif
		}

		evt.error_state = code;
	}
	if (n) {
		if (no_queueing) {
			gf_dom_event_fire(n, dom_event);
		} else {
			gf_sc_queue_dom_event(scene->root_od->term->compositor, n, dom_event);
		}
	} else {
		if (root) {
			if (no_queueing) {
				gf_dom_event_fire(root, dom_event);
			} else {
				gf_sc_queue_dom_event(scene->root_od->term->compositor, root, dom_event);
			}
		}

		count=scene->root_od->mo ? gf_mo_event_target_count(scene->root_od->mo) : 0;
		for (i=0; i<count; i++) {
			GF_Node *an = gf_event_target_get_node(gf_mo_event_target_get(scene->root_od->mo, i));
			if (no_queueing) {
				gf_dom_event_fire(an, dom_event);
			} else {
				gf_sc_queue_dom_event(scene->root_od->term->compositor, an, dom_event);
			}
		}
	}
#endif
}


GF_EXPORT
void gf_scene_attach_to_compositor(GF_Scene *scene)
{
	char *url;
	if (!scene->root_od) return;

	if ((scene->graph_attached==1) || (gf_sg_get_root_node(scene->graph)==NULL) ) {
		gf_term_invalidate_compositor(scene->root_od->term);
		return;
	}
	scene->graph_attached = 1;

	/*locate fragment IRI*/
	if (scene->root_od->net_service && scene->root_od->net_service->url) {
		if (scene->fragment_uri) {
			gf_free(scene->fragment_uri);
			scene->fragment_uri = NULL;
		}
		url = strchr(scene->root_od->net_service->url, '#');
		if (url) scene->fragment_uri = gf_strdup(url+1);
	}

	/*main display scene, setup compositor*/
	if (scene->root_od->term->root_scene == scene) {
		gf_sc_set_scene(scene->root_od->term->compositor, scene->graph);
	}
	else {
		u32 i, count;
		count = scene->root_od->mo ? gf_mo_event_target_count(scene->root_od->mo) : 0;
		for (i=0; i<count; i++) {
			gf_node_dirty_parents( gf_event_target_get_node(gf_mo_event_target_get(scene->root_od->mo, i)));
		}
		gf_term_invalidate_compositor(scene->root_od->term);

		if (scene->root_od->parentscene->is_dynamic_scene) {
			u32 w, h;
			gf_sg_get_scene_size_info(scene->graph, &w, &h);
			gf_sc_set_size(scene->root_od->term->compositor, w, h);
		}
		/*trigger a scene attach event*/
		gf_scene_notify_event(scene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, GF_OK, GF_FALSE);
	}
}

static GF_MediaObject *IS_CheckExistingObject(GF_Scene *scene, MFURL *urls, u32 type)
{
	GF_MediaObject *obj;
	u32 i = 0;
	while ((obj = (GF_MediaObject *)gf_list_enum(scene->scene_objects, &i))) {
		if (type && (type != obj->type)) continue;
		if ((obj->OD_ID == GF_MEDIA_EXTERNAL_ID) && gf_mo_is_same_url(obj, urls, NULL, 0)) return obj;
		else if ((obj->OD_ID != GF_MEDIA_EXTERNAL_ID) && (obj->OD_ID == urls->vals[0].OD_ID)) return obj;
	}
	return NULL;
}

static GFINLINE Bool is_match_obj_type(u32 type, u32 hint_type)
{
	if (!hint_type) return GF_TRUE;
	if (type==hint_type) return GF_TRUE;
	/*TEXT are used by animation stream*/
	if ((type==GF_MEDIA_OBJECT_TEXT) && (hint_type==GF_MEDIA_OBJECT_UPDATES)) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
GF_MediaObject *gf_scene_get_media_object_ex(GF_Scene *scene, MFURL *url, u32 obj_type_hint, Bool lock_timelines, GF_MediaObject *sync_ref, Bool force_new_if_not_attached, GF_Node *node)
{
	GF_MediaObject *obj;
	GF_Scene *original_parent_scene = NULL;
	Bool keep_fragment = GF_TRUE;
	Bool first_pass = force_new_if_not_attached ? GF_FALSE : GF_TRUE;
	u32 i, OD_ID;

	OD_ID = gf_mo_get_od_id(url);
	if (!OD_ID) return NULL;

	gf_mx_p(scene->mx_resources);

	/*we may have overriden the time lines in parent scene, thus all objects in this scene have the same clock*/
	if (scene->root_od->parentscene && scene->root_od->parentscene->force_single_timeline)
		lock_timelines = GF_TRUE;

	/*the first pass is needed to detect objects already inserted and registered with the given nodes, regardless of
	the force_new_if_not_attached flag. This ty^pically occurs when a resource is first created then linked to an animation/inline*/
restart:
	obj = NULL;
	i=0;
	while ((obj = (GF_MediaObject *)gf_list_enum(scene->scene_objects, &i))) {
		Bool odm_matches = GF_FALSE;

		if (
		    /*regular OD scheme*/
		    (OD_ID != GF_MEDIA_EXTERNAL_ID && (obj->OD_ID==OD_ID))
		    ||
		    /*dynamic OD scheme - !! obj->OD_ID may different from GF_MEDIA_EXTERNAL_ID when ODs are
		    directly added to the terminal by the service*/
		    ((OD_ID == GF_MEDIA_EXTERNAL_ID)
		     /*if object type unknown (media control, media sensor), return first obj matching URL
		     otherwise check types*/
		     && is_match_obj_type(obj->type, obj_type_hint)
		     /*locate sub-url in given one and handle fragments (viewpoint/segments/...)*/
		     && gf_mo_is_same_url(obj, url, &keep_fragment, obj_type_hint)
		    )
		) {
			odm_matches = GF_TRUE;
		}

		if (!odm_matches) continue;

		if (obj->odm) {
			Bool can_reuse = GF_TRUE;
			Bool timeline_locked = (obj->odm->flags & GF_ODM_INHERIT_TIMELINE) ? GF_TRUE : GF_FALSE;

			//addon object always share the timeline
			if (obj->odm->addon || obj->odm->parentscene->root_od->addon)
				timeline_locked = lock_timelines = 1;

			if (timeline_locked != lock_timelines)
				continue;

			gf_term_lock_media_queue(scene->root_od->term, GF_TRUE);
			if (obj->odm->flags & GF_ODM_DESTROYED) can_reuse = GF_FALSE;
			else if (obj->odm->action_type == GF_ODM_ACTION_DELETE) {
				/*check if object is being destroyed (no longer in the queue)*/
				if (gf_list_del_item(scene->root_od->term->media_queue, obj->odm)<0) {
					can_reuse = GF_FALSE;
				}
				/*otherwise reuse object, discard current destroy command*/
				else {
					obj->odm->action_type = GF_ODM_ACTION_PLAY;
				}
			}
			gf_term_lock_media_queue(scene->root_od->term, GF_FALSE);
			if (!can_reuse) continue;

		}

		if (!first_pass && !force_new_if_not_attached) {
			if (node && (gf_mo_event_target_find_by_node(obj, node)<0))
				gf_mo_event_target_add_node(obj, node);
			gf_mx_v(scene->mx_resources);
			return obj;
		}
		/*special case where the URL is requested twice for the same node: use the existing resource*/
		else if (node && (gf_mo_event_target_find_by_node(obj, node)>=0)) {
			gf_mx_v(scene->mx_resources);
			return obj;
		}
	}
	if (first_pass) {
		first_pass = GF_FALSE;
		goto restart;
	}

	/*we cannot create an OD manager at this point*/
	if (obj_type_hint==GF_MEDIA_OBJECT_UNDEF) {
		gf_mx_v(scene->mx_resources);
		return NULL;
	}

	/*create a new object identification*/
	obj = gf_mo_new();
	obj->OD_ID = OD_ID;
	obj->type = obj_type_hint;

	/*register node with object*/
	if (node) {
		gf_mo_event_target_add_node(obj, node);

		original_parent_scene = (GF_Scene*) gf_sg_get_private(gf_node_get_graph(node));
	}

	/*if animation stream object, remember originating node
		!! FIXME - this should be cleaned up !!
	*/
	if (obj->type == GF_MEDIA_OBJECT_UPDATES)
		obj->node_ptr = node;

	gf_list_add(scene->scene_objects, obj);
	if (OD_ID == GF_MEDIA_EXTERNAL_ID) {
		gf_sg_vrml_copy_mfurl(&obj->URLs, url);
		gf_scene_insert_object(scene, obj, lock_timelines, sync_ref, keep_fragment, original_parent_scene);
		/*safety check!!!*/
		if (gf_list_find(scene->scene_objects, obj)<0) {
			gf_mx_v(scene->mx_resources);
			return NULL;
		}

		if (obj->odm==NULL) {
			gf_list_del_item(scene->scene_objects, obj);
			gf_mo_del(obj);
			gf_mx_v(scene->mx_resources);
			return NULL;
		}
	}

	gf_mx_v(scene->mx_resources);
	return obj;
}

GF_MediaObject *gf_scene_get_media_object(GF_Scene *scene, MFURL *url, u32 obj_type_hint, Bool lock_timelines)
{
	return gf_scene_get_media_object_ex(scene, url, obj_type_hint, lock_timelines, NULL, GF_FALSE, NULL);
}

GF_EXPORT
void gf_scene_setup_object(GF_Scene *scene, GF_ObjectManager *odm)
{
	GF_MediaObject *obj;
	u32 i;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Scene] Setup object manager %d (MO %p)\n", odm->OD->objectDescriptorID, odm->mo));

	/*an object may already be assigned (when using ESD URLs, setup is performed twice)*/
	if (odm->mo != NULL) goto existing;

	i=0;
	while ((obj = (GF_MediaObject*)gf_list_enum(scene->scene_objects, &i))) {
		/*make sure services are different*/
		if (obj->odm && (odm->net_service != obj->odm->net_service)) continue;

		if (obj->OD_ID==GF_MEDIA_EXTERNAL_ID) {
			//assert(obj->odm);
			if (obj->odm == odm) {
				/*assign FINAL OD, not parent*/
				obj->odm = odm;
				odm->mo = obj;
				goto existing;
			}
		}
		else if (obj->OD_ID == odm->OD->objectDescriptorID) {
			assert(obj->odm==NULL);
			obj->odm = odm;
			odm->mo = obj;
			goto existing;
		}
	}
	/*newly created OD*/
	odm->mo = gf_mo_new();
	gf_list_add(scene->scene_objects, odm->mo);
	odm->mo->odm = odm;
	odm->mo->OD_ID = odm->OD->objectDescriptorID;

existing:
	/*setup object type*/
	if (!odm->codec) odm->mo->type = GF_MEDIA_OBJECT_SCENE;
	else if (odm->codec->type == GF_STREAM_VISUAL) odm->mo->type = GF_MEDIA_OBJECT_VIDEO;
	else if (odm->codec->type == GF_STREAM_AUDIO) odm->mo->type = GF_MEDIA_OBJECT_AUDIO;
	else if (odm->codec->type == GF_STREAM_TEXT) odm->mo->type = GF_MEDIA_OBJECT_TEXT;
	else if (odm->codec->type == GF_STREAM_SCENE) odm->mo->type = GF_MEDIA_OBJECT_UPDATES;

	/*update info*/
	gf_mo_update_caps(odm->mo);
	/*media object playback has already been requested by the scene, trigger media start*/
	if (odm->mo->num_open && !odm->state) {
		gf_odm_start(odm, 0);
		if (odm->mo->speed != FIX_ONE) gf_odm_set_speed(odm, odm->mo->speed, GF_TRUE);
	}
	if ((odm->mo->type==GF_MEDIA_OBJECT_VIDEO) && scene->is_dynamic_scene && !odm->parentscene->root_od->addon) {
		gf_scene_force_size_to_video(scene, odm->mo);
	}
	/*invalidate scene for all nodes using the OD*/
	gf_term_invalidate_compositor(odm->term);
}

GF_EXPORT
void gf_scene_set_duration(GF_Scene *scene)
{
	Double dur;
	u32 i;
	u64 max_dur;
	GF_ObjectManager *odm;
#ifndef GPAC_DISABLE_VRML
	MediaSensorStack *media_sens;
#endif
	GF_Clock *ck;

	/*this is not normative but works in so many cases... set the duration to the max duration
	of all streams sharing the clock*/
	ck = gf_odm_get_media_clock(scene->root_od);
	max_dur = scene->root_od->duration;
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
		if (!odm->codec) continue;
		if (!ck || gf_odm_shares_clock(odm, ck)) {
			if (odm->duration>max_dur) max_dur = odm->duration;
		}
	}
	if (scene->duration == max_dur) return;

	scene->duration = max_dur;
	if (scene->is_dynamic_scene && !scene->root_od->duration) scene->root_od->duration = max_dur;

	dur = (Double) (s64) scene->duration;
	dur /= 1000;

#ifndef GPAC_DISABLE_VRML
	i=0;
	while ((media_sens = (MediaSensorStack*)gf_list_enum(scene->root_od->ms_stack, &i))) {
		if (media_sens->sensor->isActive) {
			media_sens->sensor->mediaDuration = dur;
			gf_node_event_out((GF_Node *) media_sens->sensor, 3/*"mediaDuration"*/);
		}
	}
#endif

	if ((scene == scene->root_od->term->root_scene) && scene->root_od->term->user->EventProc) {
		GF_Event evt;
		evt.type = GF_EVENT_DURATION;
		evt.duration.duration = dur;
		evt.duration.can_seek = (scene->root_od->flags & GF_ODM_NO_TIME_CTRL) ? GF_FALSE : GF_TRUE;
		if (dur<1.0) evt.duration.can_seek = 0;
		gf_term_send_event(scene->root_od->term,&evt);
	}

}

GF_EXPORT
void gf_scene_set_timeshift_depth(GF_Scene *scene)
{
	u32 i;
	u32 max_timeshift;
	GF_ObjectManager *odm;
	GF_Clock *ck;

	ck = gf_odm_get_media_clock(scene->root_od);
	max_timeshift = scene->root_od->timeshift_depth;
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
		if (!odm->codec) continue;
		if (!ck || gf_odm_shares_clock(odm, ck)) {
			if (odm->timeshift_depth > max_timeshift) max_timeshift = odm->timeshift_depth;
		}
	}
	if (scene->timeshift_depth == max_timeshift) return;

	scene->timeshift_depth = max_timeshift;
	if (scene->is_dynamic_scene && !scene->root_od->timeshift_depth) scene->root_od->timeshift_depth = max_timeshift;
	if (scene->root_od->addon && (scene->root_od->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
		if (scene->root_od->parentscene->is_dynamic_scene && (scene->root_od->parentscene->timeshift_depth < max_timeshift)) {
			scene->root_od->parentscene->timeshift_depth = max_timeshift;
			scene->root_od->parentscene->root_od->timeshift_depth = max_timeshift;
			gf_scene_notify_event(scene->root_od->parentscene, GF_EVENT_TIMESHIFT_DEPTH, NULL, NULL, GF_OK, GF_FALSE);
		}
	} else {
		gf_scene_notify_event(scene, GF_EVENT_TIMESHIFT_DEPTH, NULL, NULL, GF_OK, GF_FALSE);
	}
}


GF_MediaObject *gf_scene_find_object(GF_Scene *scene, u16 ODID, char *url)
{
	u32 i;
	GF_MediaObject *mo;
	if (!url && !ODID) return NULL;
	i=0;
	while ((mo = (GF_MediaObject *)gf_list_enum(scene->scene_objects, &i))) {
		if ((ODID==GF_MEDIA_EXTERNAL_ID) && url) {
			if (mo->URLs.count && !stricmp(mo->URLs.vals[0].url, url)) return mo;
		} else if (mo->OD_ID==ODID) {
			return mo;
		}
	}
	return NULL;
}


GF_EXPORT
void gf_scene_register_extra_graph(GF_Scene *scene, GF_SceneGraph *extra_scene, Bool do_remove)
{
	if (do_remove) {
		if (gf_list_find(scene->extra_scenes, extra_scene)<0) return;
		gf_list_del_item(scene->extra_scenes, extra_scene);
		/*for root scene*/
		if (scene->root_od->term->root_scene == scene) {
			gf_sc_register_extra_graph(scene->root_od->term->compositor, extra_scene, 1);
		}
	} else {
		if (gf_list_find(scene->extra_scenes, extra_scene)>=0) return;
		gf_list_add(scene->extra_scenes, extra_scene);
		/*for root scene*/
		if (scene->root_od->term->root_scene == scene) {
			gf_sc_register_extra_graph(scene->root_od->term->compositor, extra_scene, 0);
		}
	}
}


static void gf_scene_get_video_size(GF_MediaObject *mo, u32 *w, u32 *h)
{
	u32 pixel_ar;
	if (!gf_mo_get_visual_info(mo, w, h, NULL, &pixel_ar, NULL, NULL)) return;
	if (pixel_ar) {
		u32 n, d;
		n = (pixel_ar>>16) & 0x0000FFFF;
		d = (pixel_ar) & 0x0000FFFF;
		*w = (*w * n) / d;
	}
#ifndef GPAC_DISABLE_3D
	if (mo->odm) {
		if (mo->odm->term->compositor->frame_packing==GF_3D_STEREO_TOP) *h /= 2;
		else if (mo->odm->term->compositor->frame_packing==GF_3D_STEREO_SIDE) *w /= 2;
	}
#endif
}

void gf_scene_force_size_to_video(GF_Scene *scene, GF_MediaObject *mo)
{
	u32 w, h;
	gf_scene_get_video_size(mo, &w, &h);

	if (w && h) gf_scene_force_size(scene, w, h);
}

#ifndef GPAC_DISABLE_VRML

static void IS_UpdateVideoPos(GF_Scene *scene)
{
	MFURL url;
	M_Transform2D *tr;
	GF_MediaObject *mo;
	u32 w, h, v_w, v_h;
	if (!scene->visual_url.OD_ID && !scene->visual_url.url) return;

	if (scene->vr_type) return;

	url.count = 1;
	url.vals = &scene->visual_url;
	mo = IS_CheckExistingObject(scene, &url, GF_MEDIA_OBJECT_VIDEO);
	if (!mo) return;
	tr = (M_Transform2D *) gf_sg_find_node_by_name(scene->graph, "DYN_TRANS");
	if (!tr) return;

	gf_sg_get_scene_size_info(scene->graph, &w, &h);
	if (!w || !h) return;

	gf_scene_get_video_size(mo, &v_w, &v_h);
	if (scene->force_size_set) {
		if (v_w && v_h) {
			tr->scale.x = gf_divfix(INT2FIX(w), INT2FIX(v_w));
			tr->scale.y = gf_divfix(INT2FIX(h), INT2FIX(v_h));
		}
		tr->translation.x = tr->translation.y = 0;
	} else {
		tr->scale.x = tr->scale.y = FIX_ONE;
		tr->translation.x = INT2FIX((s32) (w - v_w)) / 2;
		tr->translation.y = INT2FIX((s32) (h - v_h)) / 2;
	}
	gf_node_dirty_set((GF_Node *)tr, 0, 0);


	gf_scene_set_addon_layout_info(scene, scene->addon_position, scene->addon_size_factor);

	if (scene->root_od->term->root_scene == scene) {
		//if (scene->graph_attached) gf_sc_set_scene(scene->root_od->term->compositor, NULL);
		//gf_sc_set_scene(scene->root_od->term->compositor, scene->graph);
	}
}

static GF_Node *is_create_node(GF_SceneGraph *sg, u32 tag, const char *def_name)
{
	GF_Node *n = gf_node_new(sg, tag);
	if (n) {
		if (def_name) gf_node_set_id(n, gf_sg_get_next_available_node_id(sg), def_name);
		gf_node_init(n);
	}
	return n;
}

static Bool is_odm_url(SFURL *url, GF_ObjectManager *odm)
{
	if (!url->OD_ID && !url->url) return 0;
	if (odm->OD->objectDescriptorID != GF_MEDIA_EXTERNAL_ID) return (url->OD_ID==odm->OD->objectDescriptorID) ? 1 : 0;
	if (!url->url || !odm->OD->URLString) return 0;
	return !stricmp(url->url, odm->OD->URLString);
}

static void set_media_url(GF_Scene *scene, SFURL *media_url, GF_Node *node,  MFURL *node_url, u32 type)
{
	u32 w, h;
	SFURL *sfu;
	Bool url_changed = 0;

	/*scene url is not set, find the first one*/
	if (!media_url->OD_ID  ) {
		u32 count, i;
		GF_ObjectManager *odm = NULL;
		count = gf_list_count(scene->resources);
		for (i=0; i<count; i++) {
			odm = (GF_ObjectManager*)gf_list_get(scene->resources, i);
			if (odm->scalable_addon || !odm->OD)
				continue;

			if (type==GF_STREAM_TEXT) {
				if (!odm->codec || ((odm->codec->type!=type) && (odm->codec->type!=GF_STREAM_ND_SUBPIC))) continue;
			}
			else if (type==GF_STREAM_SCENE) {
				if (!odm->subscene || (!odm->subscene->scene_codec && !odm->subscene->is_dynamic_scene) ) continue;

				if (odm->subscene->root_od->addon)
					continue;
			}
			else {
				if (!odm->codec || (odm->codec->type!=type)) continue;

				//browse from this ODM on untill we find an object with the desired OTI (if any)
				if ((type==GF_STREAM_AUDIO) && odm->term->prefered_audio_codec_oti) {
					u32 j;
					for (j=i; j<count; j++) {
						GF_ObjectManager*an_odm = (GF_ObjectManager*)gf_list_get(scene->resources, j);
						if (an_odm->codec && (an_odm->codec->oti==odm->term->prefered_audio_codec_oti)) {
							odm = an_odm;
							break;
						}
					}
				}
			}

			if (scene->selected_service_id && (scene->selected_service_id != odm->OD->ServiceID)) {
				//objects inserted from broadcast may have been played but not yet registered with the scene, we need to force a stop
				if ((odm->mo && !odm->mo->num_open) || !odm->mo) {
					if (odm->state==GF_ODM_STATE_PLAY) {
						/*do not stop directly*/
						gf_term_lock_media_queue(odm->term, GF_TRUE);
						/*if object not in media queue, add it*/
						if (gf_list_find(odm->term->media_queue, odm)<0) {
							gf_list_add(odm->term->media_queue, odm);
						}
						odm->action_type = GF_ODM_ACTION_STOP;
						gf_term_lock_media_queue(odm->term, GF_FALSE);
					}
				}
				continue;
			}


			media_url->OD_ID = odm->OD->objectDescriptorID;
			if (media_url->OD_ID==GF_MEDIA_EXTERNAL_ID) media_url->url = gf_strdup(odm->net_service->url);

			if (!scene->dyn_ck) {
				if (odm->subscene && odm->subscene->scene_codec) {
					scene->dyn_ck = odm->subscene->scene_codec->ck;
				} else if (odm->codec) {
					scene->dyn_ck = odm->codec->ck;
				}
			}

			if (odm->mo && (type==GF_STREAM_VISUAL)) {
				gf_scene_get_video_size(odm->mo, &w, &h);
				if (w && h) {
					scene->force_size_set = 0;
					gf_sg_set_scene_size_info(scene->graph, w, h, 1);
					gf_scene_force_size(scene, w, h);
				}
			}
			break;
		}
		if (!odm) {
			if (media_url->OD_ID ) url_changed = 1;
			media_url->OD_ID = 0;
			if (media_url->url) {
				gf_free(media_url->url);
				media_url->url = NULL;
			}
		}
	}

	if (media_url->OD_ID) {
		if (!node_url->count) url_changed = 1;
		else if (node_url->vals[0].OD_ID!=media_url->OD_ID) url_changed = 1;
		else if (media_url->OD_ID==GF_MEDIA_EXTERNAL_ID) {
			if (!node_url->vals[0].url || !media_url->url || strcmp(node_url->vals[0].url, media_url->url) ) url_changed = 1;
		}
	} else {
		if (node_url->count) url_changed = 1;
	}

	if (url_changed) {
		gf_sg_vrml_mf_reset(node_url, GF_SG_VRML_MFURL);
		gf_sg_vrml_mf_append(node_url, GF_SG_VRML_MFURL, (void **) &sfu);
		sfu->OD_ID = media_url->OD_ID;
		if (media_url->url) sfu->url = gf_strdup(media_url->url);

		gf_node_changed(node, NULL);
	}

}

static void scene_video_mouse_move(void *param, GF_FieldInfo *field)
{
	u32 i, count;
	GF_Scene *scene = (GF_Scene *) param;
	SFVec2f tx_coord = * ((SFVec2f *) field->far_ptr);

	if (!scene->visual_url.OD_ID) return;

	count = gf_list_count(scene->resources);
	for (i=0; i<count; i++) {
		GF_ObjectManager *odm = gf_list_get(scene->resources, i);
		if (!odm->mo) continue;

		if (odm->codec && odm->mo->OD_ID && (odm->mo->OD_ID != GF_MEDIA_EXTERNAL_ID) && (odm->mo->OD_ID==scene->visual_url.OD_ID)) {
			GF_Err e;
			u32 w, h;
			GF_CodecCapability cap;
			cap.CapCode = GF_CODEC_INTERACT_COORDS;
			w = FIX2INT( tx_coord.x * 0xFFFF);
			h = FIX2INT( tx_coord.y * 0xFFFF);
			cap.cap.valueInt = w<<16 | h;

			e = gf_codec_set_capability(odm->codec, cap);
			if (e==GF_NOT_SUPPORTED) {
				GF_Node *n = gf_sg_find_node_by_name(scene->graph, "DYN_TOUCH");
				if (n) ((M_TouchSensor *)n)->enabled = GF_FALSE;
			}
			return;
		}
	}
}

static GF_Node *load_vr_proto_node(GF_SceneGraph *sg, const char *name, const char *def_name)
{
	GF_Proto *proto;
	GF_Node *node;
	if (!name) name = "urn:inet:gpac:builtin:VRGeometry";

	proto = gf_sg_find_proto(sg, 0, (char *) name);
	if (!proto) {
		MFURL *url;
		proto = gf_sg_proto_new(sg, 0,  (char *) name, GF_FALSE);
		url = gf_sg_proto_get_extern_url(proto);
		if (url)
			url->vals = gf_malloc(sizeof(SFURL));
		if (!url || !url->vals) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to allocate VR proto\n"));
			return NULL;
		}
		url->count=1;
		url->vals[0].url = gf_strdup(name);
	}
	node = gf_sg_proto_create_instance(sg, proto);
	if (node) {
		if (def_name) gf_node_set_id(node, gf_sg_get_next_available_node_id(sg), def_name);
		gf_node_init(node);
	}
	return node;
}


static void create_movie(GF_Scene *scene, GF_Node *root, const char *tr_name, const char *texture_name, const char *name_geo)
{
	M_MovieTexture *mt;
	GF_Node *n1, *n2;

	/*create a shape and bitmap node*/
	n2 = is_create_node(scene->graph, TAG_MPEG4_Transform2D, tr_name);
	gf_node_list_add_child( &((GF_ParentNode *)root)->children, n2);
	gf_node_register(n2, root);
	n1 = n2;
	n2 = is_create_node(scene->graph, TAG_MPEG4_Shape, NULL);
	gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
	gf_node_register(n2, n1);
	n1 = n2;
	n2 = is_create_node(scene->graph, TAG_MPEG4_Appearance, NULL);
	((M_Shape *)n1)->appearance = n2;
	gf_node_register(n2, n1);

	/*note we create a movie texture even for images...*/
	mt = (M_MovieTexture *) is_create_node(scene->graph, TAG_MPEG4_MovieTexture, texture_name);
	mt->startTime = gf_scene_get_time(scene);
	((M_Appearance *)n2)->texture = (GF_Node *)mt;
	gf_node_register((GF_Node *)mt, n2);

	if (scene->srd_type) {
		GF_Node *app = n2;

		if (scene->vr_type) {
			n2 = load_vr_proto_node(scene->graph, NULL, name_geo);
		} else {
			n2 = is_create_node(scene->graph, TAG_MPEG4_Rectangle, name_geo);
		}

		((M_Shape *)n1)->geometry = n2;
		gf_node_register(n2, n1);
		//force  appearance material2D.filled = TRUE
		n2 = is_create_node(scene->graph, TAG_MPEG4_Material2D, NULL);
		((M_Material2D *)n2)->filled = GF_TRUE;
		((M_Appearance *)app)->material = n2;
		gf_node_register(n2, app);
	} else if (scene->vr_type) {
		n2 = is_create_node(scene->graph, TAG_MPEG4_Sphere, name_geo);
		((M_Shape *)n1)->geometry = n2;
		gf_node_register(n2, n1);
	} else {
		n2 = is_create_node(scene->graph, TAG_MPEG4_Bitmap, name_geo);
		((M_Shape *)n1)->geometry = n2;
		gf_node_register(n2, n1);
	}
}
/*regenerates the scene graph for dynamic scene.
This will also try to reload any previously presented streams. Note that in the usual case the scene is generated
just once when receiving the first OD AU (ressources are NOT destroyed when seeking), but since the network may need
to update the OD ressources, we still take care of it*/
void gf_scene_regenerate(GF_Scene *scene)
{
	GF_Node *n1, *n2, *root;
	GF_Event evt;
	M_AudioClip *ac;
	M_MovieTexture *mt;
	M_AnimationStream *as;
	M_Inline *dims;
	M_Transform2D *addon_tr;
	M_Layer2D *addon_layer;
	M_Inline *addon_scene;
	if (scene->is_dynamic_scene != 1) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Inline] Regenerating scene graph for service %s\n", scene->root_od->net_service->url));

	gf_sc_lock(scene->root_od->term->compositor, 1);

	ac = (M_AudioClip *) gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO1");

	/*this is the first time, generate a scene graph*/
	if (!ac) {
		GF_Event evt;

		/*create an OrderedGroup*/
		n1 = is_create_node(scene->graph, scene->vr_type ? TAG_MPEG4_Group : TAG_MPEG4_OrderedGroup, NULL);
		gf_sg_set_root_node(scene->graph, n1);
		gf_node_register(n1, NULL);
		root = n1;

		if (! scene->root_od->parentscene) {
			n2 = is_create_node(scene->graph, TAG_MPEG4_Background2D, "DYN_BACK");
			gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
			gf_node_register(n2, n1);
		}

		//create VP info regardless of VR type
		if (scene->vr_type) {
			const char *opt;
			n2 = is_create_node(scene->graph, TAG_MPEG4_Viewpoint, "DYN_VP");
			((M_Viewpoint *)n2)->position.z = 0;
			((M_Viewpoint *)n2)->fieldOfView = GF_PI/2;

			opt = gf_cfg_get_key(scene->root_od->term->user->config, "Compositor", "VRDefaultFOV");
			if (!opt) {
				opt="1.570796326794897";
				gf_cfg_set_key(scene->root_od->term->user->config, "Compositor", "VRDefaultFOV", opt);
			}
			((M_Viewpoint *)n2)->fieldOfView = FLT2FIX( atof(opt) );

			gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
			gf_node_register(n2, n1);

			n2 = is_create_node(scene->graph, TAG_MPEG4_NavigationInfo, NULL);
			gf_free( ((M_NavigationInfo *)n2)->type.vals[0] );
			((M_NavigationInfo *)n2)->type.vals[0] = gf_strdup("VR");
			gf_free( ((M_NavigationInfo *)n2)->type.vals[1] );
			((M_NavigationInfo *)n2)->type.vals[1] = gf_strdup("NONE");
			((M_NavigationInfo *)n2)->type.count = 2;
			((M_NavigationInfo *)n2)->avatarSize.count = 0;

			gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
			gf_node_register(n2, n1);
		}

		/*create an sound2D and an audioClip node*/
		n2 = is_create_node(scene->graph, TAG_MPEG4_Sound2D, NULL);
		gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
		gf_node_register(n2, n1);

		ac = (M_AudioClip *) is_create_node(scene->graph, TAG_MPEG4_AudioClip, "DYN_AUDIO1");
		ac->startTime = gf_scene_get_time(scene);
		((M_Sound2D *)n2)->source = (GF_Node *)ac;
		gf_node_register((GF_Node *)ac, n2);


		/*transform for any translation due to scene resize (3GPP)*/
		n2 = is_create_node(scene->graph, TAG_MPEG4_Transform2D, "DYN_TRANS");
		gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
		gf_node_register(n2, n1);
		n1 = n2;

		/*create a touch sensor for the video*/
		n2 = is_create_node(scene->graph, TAG_MPEG4_TouchSensor, "DYN_TOUCH");
		gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
		gf_node_register(n2, n1);
		gf_sg_route_new_to_callback(scene->graph, n2, 3/*"hitTexCoord_changed"*/, scene, scene_video_mouse_move);

		create_movie(scene, n1, "TR1", "DYN_VIDEO1", "DYN_GEOM1");

		if (! scene->vr_type) {
			/*text streams controlled through AnimationStream*/
			n1 = gf_sg_get_root_node(scene->graph);
			as = (M_AnimationStream *) is_create_node(scene->graph, TAG_MPEG4_AnimationStream, "DYN_TEXT");
			gf_node_list_add_child( &((GF_ParentNode *)n1)->children, (GF_Node*)as);
			gf_node_register((GF_Node *)as, n1);


			/*3GPP DIMS streams controlled */
			n1 = gf_sg_get_root_node(scene->graph);
			dims = (M_Inline *) is_create_node(scene->graph, TAG_MPEG4_Inline, "DIMS_SCENE");
			gf_node_list_add_child( &((GF_ParentNode *)n1)->children, (GF_Node*)dims);
			gf_node_register((GF_Node *)dims, n1);

			/*PVR version of live content*/
			n1 = gf_sg_get_root_node(scene->graph);
			addon_scene = (M_Inline *) is_create_node(scene->graph, TAG_MPEG4_Inline, "PVR_SCENE");
			gf_node_list_add_child( &((GF_ParentNode *)n1)->children, (GF_Node*)addon_scene);
			gf_node_register((GF_Node *)addon_scene, (GF_Node *)n1);

			/*Media addon scene*/
			n1 = gf_sg_get_root_node(scene->graph);
			addon_tr = (M_Transform2D *) is_create_node(scene->graph, TAG_MPEG4_Transform2D, "ADDON_TRANS");
			gf_node_list_add_child( &((GF_ParentNode *)n1)->children, (GF_Node*)addon_tr);
			gf_node_register((GF_Node *)addon_tr, n1);

			addon_layer = (M_Layer2D *) is_create_node(scene->graph, TAG_MPEG4_Layer2D, "ADDON_LAYER");
			gf_node_list_add_child( &((GF_ParentNode *)addon_tr)->children, (GF_Node*)addon_layer);
			gf_node_register((GF_Node *)addon_layer, (GF_Node *)addon_tr);

			addon_scene = (M_Inline *) is_create_node(scene->graph, TAG_MPEG4_Inline, "ADDON_SCENE");
			gf_node_list_add_child( &((GF_ParentNode *)addon_layer)->children, (GF_Node*)addon_scene);
			gf_node_register((GF_Node *)addon_scene, (GF_Node *)addon_layer);
		}
		//VR mode, add VR headup
		else {
			GF_Node *vrhud = load_vr_proto_node(scene->graph, "urn:inet:gpac:builtin:VRHUD", NULL);
			gf_node_list_add_child( &((GF_ParentNode *)root)->children, (GF_Node*)vrhud);
			gf_node_register(vrhud, root);
		}


		//send activation for sensors
		memset(&evt, 0, sizeof(GF_Event));
		evt.type = GF_EVENT_SENSOR_REQUEST;
		evt.activate_sensor.activate = scene->vr_type;
		evt.activate_sensor.sensor_type = GF_EVENT_SENSOR_ORIENTATION;
		if (gf_term_send_event(scene->root_od->term, &evt)==GF_TRUE) {
			scene->root_od->term->orientation_sensors_active = scene->vr_type;
		} else {
			scene->root_od->term->orientation_sensors_active = GF_FALSE;
		}
	}

	if (scene->ambisonic_type) {
		char szName[20];
		SFURL url;
		u32 i, count;
		GF_Node *an, *root = gf_sg_get_root_node(scene->graph);
		url.url = NULL;
		url.OD_ID = 0;

		count = gf_list_count(scene->resources);
		for (i=0; i<count; i++) {
			GF_ObjectManager *odm = gf_list_get(scene->resources, i);
			if (!odm->ambi_ch_id) continue;

			sprintf(szName, "DYN_AUDIO%d", odm->ambi_ch_id);
			an = gf_sg_find_node_by_name(scene->graph, szName);
			if (!an) {
				/*create an sound2D and an audioClip node*/
				an = is_create_node(scene->graph, TAG_MPEG4_Sound2D, NULL);
				gf_node_list_add_child( &((GF_ParentNode *)root)->children, an);
				gf_node_register(an, root);

				ac = (M_AudioClip *) is_create_node(scene->graph, TAG_MPEG4_AudioClip, szName);
				ac->startTime = gf_scene_get_time(scene);
				((M_Sound2D *)an)->source = (GF_Node *)ac;
				gf_node_register((GF_Node *)ac, an);
			}
			ac = (M_AudioClip *) gf_sg_find_node_by_name(scene->graph, szName);

			url.OD_ID = odm->OD->objectDescriptorID;
			set_media_url(scene, &url, (GF_Node*)ac, &ac->url, GF_STREAM_AUDIO);
		}
	} else {
		ac = (M_AudioClip *) gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO1");
		set_media_url(scene, &scene->audio_url, (GF_Node*)ac, &ac->url, GF_STREAM_AUDIO);
	}


	if (scene->srd_type) {
		char szName[20], szTex[20], szGeom[20];
		u32 i, nb_srd = 0, srd_missing = 0;
		GF_ObjectManager *a_odm;
		SFURL url;
		u32 sw, sh;
		s32 min_x, max_x, min_y, max_y;
		i=0;

		//we use 0 (and not INT_MAX) to always display the same thing regardless of holes in the srd description
		min_x = min_y = 0;
		max_x = max_y = 0;

		while ((a_odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
			if (!a_odm->mo || !a_odm->mo->srd_w) {
				srd_missing++;
				continue;
			}
			if ((s32) a_odm->mo->srd_x < min_x) min_x = (s32) a_odm->mo->srd_x;
			if ((s32) a_odm->mo->srd_y < min_y) min_y = (s32) a_odm->mo->srd_y;

			if (!max_x)
				max_x = a_odm->mo->srd_full_w;
			if ((s32) a_odm->mo->srd_x + (s32) a_odm->mo->srd_w > min_x + max_x)
				max_x = (s32) a_odm->mo->srd_x + (s32) a_odm->mo->srd_w - min_x;

			if (!max_y)
				max_y = a_odm->mo->srd_full_h;

			if ((s32) a_odm->mo->srd_y + (s32) a_odm->mo->srd_h > min_y + max_y)
				max_y = (s32) a_odm->mo->srd_y + (s32) a_odm->mo->srd_h - min_y;

			nb_srd++;
		}

		n1 = gf_sg_find_node_by_name(scene->graph, "DYN_TRANS");
		for (i=1; i<nb_srd+srd_missing; i++) {
			sprintf(szName, "TR%d", i+1);
			sprintf(szTex, "DYN_VIDEO%d", i+1);
			sprintf(szGeom, "DYN_GEOM%d", i+1);
			n2 = gf_sg_find_node_by_name(scene->graph, szGeom);
			if (!n2) {
				create_movie(scene, n1, szName, szTex, szGeom);
			}
		}
		assert(max_x>min_x);
		assert(max_y>min_y);

		scene->srd_min_x = min_x;
		scene->srd_min_y = min_y;
		scene->srd_max_x = max_x;
		scene->srd_max_y = max_y;

		url.url = NULL;
		gf_sg_get_scene_size_info(scene->graph, &sw, &sh);
		i=0;
		while ((a_odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
			if (a_odm->mo && a_odm->mo->srd_w) {
				Fixed tw, th, tx, ty;

				sprintf(szName, "TR%d", i);
				sprintf(szTex, "DYN_VIDEO%d", i);
				sprintf(szGeom, "DYN_GEOM%d", i);
				url.OD_ID = a_odm->OD->objectDescriptorID;

				mt = (M_MovieTexture *) gf_sg_find_node_by_name(scene->graph, szTex);
				if (!mt) continue;

				set_media_url(scene, &url, (GF_Node*)mt, &mt->url, GF_STREAM_VISUAL);

				if (!scene->dyn_ck && a_odm->codec) {
					scene->dyn_ck = a_odm->codec->ck;
				}

				if (scene->vr_type) {
					n2 = gf_sg_find_node_by_name(scene->graph, szGeom);
					gf_node_changed(n2, NULL);
				} else {
					tw = INT2FIX( sw * a_odm->mo->srd_w) /  (max_x - min_x);
					th = INT2FIX(sh * a_odm->mo->srd_h) / (max_y - min_y);

					n2 = gf_sg_find_node_by_name(scene->graph, szGeom);
					((M_Rectangle *)n2)->size.x = tw;
					((M_Rectangle *)n2)->size.y = th;
					gf_node_changed(n2, NULL);

					tx = INT2FIX(a_odm->mo->srd_x * sw) / (max_x - min_x);
					tx = tx - INT2FIX(sw) / 2 + INT2FIX(tw) / 2;

					ty = INT2FIX(a_odm->mo->srd_y * sh) / (max_y - min_y);
					ty = INT2FIX(sh) / 2 - ty - INT2FIX(th) / 2;

					addon_tr = (M_Transform2D  *) gf_sg_find_node_by_name(scene->graph, szName);
					addon_tr->translation.x = tx;
					addon_tr->translation.y = ty;
					gf_node_changed((GF_Node *)addon_tr, NULL);
				}
			}
		}
	} else {
		mt = (M_MovieTexture *) gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO1");
		set_media_url(scene, &scene->visual_url, (GF_Node*)mt, &mt->url, GF_STREAM_VISUAL);
	}


	if (! scene->vr_type) {
		as = (M_AnimationStream *) gf_sg_find_node_by_name(scene->graph, "DYN_TEXT");
		set_media_url(scene, &scene->text_url, (GF_Node*)as, &as->url, GF_STREAM_TEXT);

		dims = (M_Inline *) gf_sg_find_node_by_name(scene->graph, "DIMS_SCENE");
		set_media_url(scene, &scene->dims_url, (GF_Node*)dims, &dims->url, GF_STREAM_SCENE);
	}

	gf_sc_lock(scene->root_od->term->compositor, 0);

	/*disconnect to force resize*/
	if (scene->root_od->term->root_scene == scene) {
		gf_sc_set_scene(scene->root_od->term->compositor, scene->graph);
		scene->graph_attached = 1;
		evt.type = GF_EVENT_STREAMLIST;
		gf_term_send_event(scene->root_od->term, &evt);
		IS_UpdateVideoPos(scene);
	} else {
		gf_scene_notify_event(scene, scene->graph_attached ? GF_EVENT_STREAMLIST : GF_EVENT_SCENE_ATTACHED, NULL, NULL, GF_OK, GF_FALSE);
		scene->graph_attached = 1;
		gf_term_invalidate_compositor(scene->root_od->term);
	}
}

void gf_scene_toggle_addons(GF_Scene *scene, Bool show_addons)
{
	M_Inline *dscene = (M_Inline *) gf_sg_find_node_by_name(scene->graph, "ADDON_SCENE");

	if (show_addons) {
		GF_AssociatedContentLocation addon_info;
		memset(&addon_info, 0, sizeof(GF_AssociatedContentLocation));
		addon_info.timeline_id = -100;
		gf_scene_register_associated_media(scene, &addon_info);
	} else {
		gf_sg_vrml_mf_reset(&dscene->url, GF_SG_VRML_MFURL);
	}
	gf_node_changed((GF_Node *)dscene, NULL);
}

#else
/*!!fixme - we would need an SVG scene in case no VRML support is present !!!*/
GF_EXPORT
void gf_scene_regenerate(GF_Scene *scene) {}
GF_EXPORT
void gf_scene_restart_dynamic(GF_Scene *scene, s64 from_time, Bool restart_only, Bool disable_addon_check) {}
GF_EXPORT
void gf_scene_select_object(GF_Scene *scene, GF_ObjectManager *odm) {}
GF_EXPORT
void gf_scene_toggle_addons(GF_Scene *scene, Bool show_addons) { }
GF_EXPORT
void gf_scene_resume_live(GF_Scene *subscene) { }
GF_EXPORT
void gf_scene_set_addon_layout_info(GF_Scene *scene, u32 position, u32 size_factor) {}
GF_EXPORT
void gf_scene_select_main_addon(GF_Scene *scene, GF_ObjectManager *odm, Bool set_on, u32 current_clock_time) { }

#endif	/*GPAC_DISABLE_VRML*/

#ifndef GPAC_DISABLE_VRML

static Bool check_odm_deactivate(SFURL *url, GF_ObjectManager *odm, GF_Node *n)
{
	GF_FieldInfo info;
	if (!is_odm_url(url, odm) || !n) return 0;

	if (url->url) gf_free(url->url);
	url->url = NULL;
	url->OD_ID = 0;

	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_reset(info.far_ptr, GF_SG_VRML_MFURL);
	gf_node_get_field_by_name(n, "stopTime", &info);
	*((SFTime *)info.far_ptr) = gf_node_get_scene_time(n);
	gf_node_changed(n, NULL);
	return 1;
}

static void odm_deactivate(GF_Node *n)
{
	GF_FieldInfo info;

	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_reset(info.far_ptr, GF_SG_VRML_MFURL);
	gf_node_get_field_by_name(n, "stopTime", &info);
	*((SFTime *)info.far_ptr) = gf_node_get_scene_time(n);
	gf_node_changed(n, NULL);
}

static void odm_activate(SFURL *url, GF_Node *n)
{
	SFURL *sfu;
	GF_FieldInfo info;

	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_reset(info.far_ptr, GF_SG_VRML_MFURL);
	if (url->OD_ID || url->url) {
		gf_sg_vrml_mf_append(info.far_ptr, GF_SG_VRML_MFURL, (void **) &sfu);
		sfu->OD_ID = url->OD_ID;
		if (url->url) sfu->url = gf_strdup(url->url);

		gf_node_get_field_by_name(n, "startTime", &info);
		*((SFTime *)info.far_ptr) = 0.0;
		gf_node_get_field_by_name(n, "stopTime", &info);
		*((SFTime *)info.far_ptr) = 0.0;
	}

	gf_node_changed(n, NULL);
}

void gf_scene_set_service_id(GF_Scene *scene, u32 service_id)
{
	if (!scene->is_dynamic_scene) return;

	gf_sc_lock(scene->root_od->term->compositor, 1);
	if (scene->selected_service_id != service_id) {
		u32 i;
		GF_ObjectManager *odm, *remote_odm = NULL;
		//delete all objects with given service ID
		i=0;
		while ((odm = gf_list_enum(scene->resources, &i))) {
			if (odm->OD->ServiceID!=scene->selected_service_id) continue;
			if (odm->OD->URLString) {
				remote_odm = odm;
				assert(remote_odm->net_service->nb_odm_users);
				remote_odm->net_service->nb_odm_users--;
				remote_odm->net_service = scene->root_od->net_service;
				remote_odm->net_service->nb_odm_users++;
			}
			//delete all objects from this service
			else if (remote_odm) {
				gf_term_lock_media_queue(scene->root_od->term, 1);
				if (odm->net_service==remote_odm->net_service) odm->net_service->owner = odm;
				odm->action_type = GF_ODM_ACTION_DELETE;
				gf_list_add(scene->root_od->term->media_queue, odm);
				gf_term_lock_media_queue(scene->root_od->term, 0);
			}
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Scene] Switching %s from service %d to service %d (media time %g)\n", scene->root_od->net_service->url, scene->selected_service_id, service_id, (Double)scene->root_od->media_start_time/1000.0));

		scene->selected_service_id = service_id;
		scene->audio_url.OD_ID = 0;
		scene->visual_url.OD_ID = 0;
		scene->text_url.OD_ID = 0;
		scene->dims_url.OD_ID = 0;
		scene->force_size_set = 0;
		//reset clock since we change service IDs, but request a PLAY from the current time
		if (scene->dyn_ck) {
			scene->root_od->media_start_time = gf_clock_media_time(scene->dyn_ck);
			scene->dyn_ck = NULL;
		}
		if (remote_odm) {
			i=0;
			while ((odm = gf_list_enum(scene->resources, &i))) {
				if (odm->OD->ServiceID!=scene->selected_service_id) continue;
				if (odm->OD->RedirectOnly) {
					//gf_odm_setup_object will increment the number of odms in net service (it's supposed to
					//be called only upon startup, but we reuse the function). Since we are already registered
					//with the service, decrement before calling
					odm->net_service->nb_odm_users--;
					gf_odm_setup_object(odm, odm->net_service);
				}
				break;
			}
		}
		gf_scene_regenerate(scene);
	}
	gf_sc_lock(scene->root_od->term->compositor, 0);
}

GF_EXPORT
void gf_scene_select_object(GF_Scene *scene, GF_ObjectManager *odm)
{
	char *url;
	if (!scene->is_dynamic_scene || !scene->graph_attached || !odm) return;

	if (!odm->codec) {
		if (!odm->addon) return;
	}

	if (odm->OD->ServiceID && scene->selected_service_id && (scene->selected_service_id != odm->OD->ServiceID)) {
		gf_scene_set_service_id(scene, odm->OD->ServiceID);
		return;
	}


	if (odm->state) {
		if (check_odm_deactivate(&scene->audio_url, odm, gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO1")) ) return;
		if (check_odm_deactivate(&scene->visual_url, odm, gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO1") )) return;
		if (check_odm_deactivate(&scene->text_url, odm, gf_sg_find_node_by_name(scene->graph, "DYN_TEXT") )) return;
	}


	if (!odm->codec && odm->subscene) {
		M_Inline *dscene = (M_Inline *) gf_sg_find_node_by_name(scene->graph, "ADDON_SCENE");

		if (!dscene)
			return;

		if (odm->addon && odm->addon->addon_type==GF_ADDON_TYPE_MAIN) {
			return;
		}

		gf_sg_vrml_field_copy(&dscene->url, &odm->mo->URLs, GF_SG_VRML_MFURL);
		gf_node_changed((GF_Node *)dscene, NULL);
		//do not update video pos for addons, this is done only when setting up the main video object
		return;
	}

	if (odm->codec->type == GF_STREAM_AUDIO) {
		M_AudioClip *ac = (M_AudioClip *) gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO1");
		if (!ac) return;
		if (scene->audio_url.url) gf_free(scene->audio_url.url);
		scene->audio_url.url = NULL;
		scene->audio_url.OD_ID = odm->OD->objectDescriptorID;
		if (!ac->url.count) gf_sg_vrml_mf_alloc(&ac->url, GF_SG_VRML_MFURL, 1);
		ac->url.vals[0].OD_ID = odm->OD->objectDescriptorID;
		if (ac->url.vals[0].url) {
			gf_free(ac->url.vals[0].url);
			ac->url.vals[0].url = NULL;
		}
		url = odm->mo->URLs.count ? odm->mo->URLs.vals[0].url : NULL;
		if (url) {
			scene->audio_url.url = gf_strdup(url);
			ac->url.vals[0].url = gf_strdup(url);
		}
		ac->startTime = gf_scene_get_time(scene);
		gf_node_changed((GF_Node *)ac, NULL);
		return;
	}

	if (odm->codec->type == GF_STREAM_VISUAL) {
		M_MovieTexture *mt = (M_MovieTexture*) gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO1");
		if (!mt) return;
		if (scene->visual_url.url) gf_free(scene->visual_url.url);
		scene->visual_url.url = NULL;
		scene->visual_url.OD_ID = odm->OD->objectDescriptorID;
		if (!mt->url.count) gf_sg_vrml_mf_alloc(&mt->url, GF_SG_VRML_MFURL, 1);
		mt->url.vals[0].OD_ID = odm->OD->objectDescriptorID;
		if (mt->url.vals[0].url) gf_free(mt->url.vals[0].url);
		url = odm->mo->URLs.count ? odm->mo->URLs.vals[0].url : NULL;
		if (url) {
			scene->visual_url.url = gf_strdup(url);
			mt->url.vals[0].url = gf_strdup(url);
		}
		mt->startTime = gf_scene_get_time(scene);
		gf_node_changed((GF_Node *)mt, NULL);
		if (odm->mo) gf_scene_force_size_to_video(scene, odm->mo);
		scene->selected_service_id = odm->OD->ServiceID;
		return;
	}


	if (odm->codec->type == GF_STREAM_TEXT) {
		M_AnimationStream *as = (M_AnimationStream*) gf_sg_find_node_by_name(scene->graph, "DYN_TEXT");
		if (!as) return;
		if (scene->text_url.url) gf_free(scene->text_url.url);
		scene->text_url.url = NULL;
		scene->text_url.OD_ID = odm->OD->objectDescriptorID;
		if (!as->url.count) gf_sg_vrml_mf_alloc(&as->url, GF_SG_VRML_MFURL, 1);
		as->url.vals[0].OD_ID = odm->OD->objectDescriptorID;
		if (as->url.vals[0].url) gf_free(as->url.vals[0].url);
		url = odm->mo->URLs.count ? odm->mo->URLs.vals[0].url : NULL;
		if (url) {
			scene->text_url.url = gf_strdup(url);
			as->url.vals[0].url = gf_strdup(url);
		}
		as->startTime = gf_scene_get_time(scene);
		gf_node_changed((GF_Node *)as, NULL);
		return;
	}
}

void gf_scene_select_main_addon(GF_Scene *scene, GF_ObjectManager *odm, Bool set_on, u32 current_clock_time)
{
	GF_DOM_Event devt;
	const char *opt = gf_cfg_get_key(scene->root_od->term->user->config, "Systems", "DebugPVRScene");
	M_Inline *dscene = (M_Inline *) gf_sg_find_node_by_name(scene->graph, (opt && !strcmp(opt, "yes")) ? "ADDON_SCENE" : "PVR_SCENE");

	if (scene->main_addon_selected==set_on) return;
	scene->main_addon_selected = set_on;

	if (set_on) {
		odm_deactivate(gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO1"));
		odm_deactivate(gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO1"));
		odm_deactivate(gf_sg_find_node_by_name(scene->graph, "DYN_TEXT"));


		if (!odm->subscene->graph_attached) {
			odm->flags &= ~GF_ODM_REGENERATE_SCENE;
			gf_scene_regenerate(odm->subscene);
		} else {
			odm->subscene->needs_restart = 1;
		}

		//main addon is vod not live, store clock
		if (! odm->timeshift_depth &&  !scene->sys_clock_at_main_activation) {
			scene->sys_clock_at_main_activation = gf_sys_clock();
			scene->obj_clock_at_main_activation = current_clock_time;
		}


		gf_sg_vrml_field_copy(&dscene->url, &odm->mo->URLs, GF_SG_VRML_MFURL);
		gf_node_changed((GF_Node *)dscene, NULL);
	} else {
		GF_Clock *ck = scene->scene_codec ? scene->scene_codec->ck : scene->dyn_ck;
		//reactivating the main content will trigger a reset on the clock - remember where we are and resume from this point
		scene->root_od->media_start_time = gf_clock_media_time(ck);

		scene->sys_clock_at_main_activation = 0;
		scene->obj_clock_at_main_activation = 0;

		odm_activate(&scene->audio_url, gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO1"));
		odm_activate(&scene->visual_url, gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO1"));
		odm_activate(&scene->text_url, gf_sg_find_node_by_name(scene->graph, "DYN_TEXT"));

		gf_sg_vrml_mf_reset(&dscene->url, GF_SG_VRML_MFURL);
		gf_node_changed((GF_Node *)dscene, NULL);
	}

	memset(&devt, 0, sizeof(GF_DOM_Event));
	devt.type = GF_EVENT_MAIN_ADDON_STATE;
	devt.detail = set_on;
	gf_scene_notify_event(scene, GF_EVENT_MAIN_ADDON_STATE, NULL, &devt, GF_OK, GF_FALSE);

}

GF_EXPORT
void gf_scene_set_addon_layout_info(GF_Scene *scene, u32 position, u32 size_factor)
{
	MFURL url;
	M_Transform2D *tr;
	M_Layer2D *layer;
	GF_MediaObject *mo;
	s32 w, h, v_w, v_h;
	if (!scene->visual_url.OD_ID && !scene->visual_url.url) return;

	url.count = 1;
	url.vals = &scene->visual_url;
	mo = IS_CheckExistingObject(scene, &url, GF_MEDIA_OBJECT_VIDEO);
	if (!mo) return;

	scene->addon_position = position;
	scene->addon_size_factor = size_factor;

	gf_scene_get_video_size(mo, (u32 *) &v_w, (u32 *) &v_h);
	w = v_w;
	h = v_h;
	switch (size_factor) {
	case 0:
		v_w /= 2;
		v_h /= 2;
		break;
	case 1:
		v_w /= 3;
		v_h /= 3;
		break;
	case 2:
	default:
		v_w /= 4;
		v_h /= 4;
		break;
	}

	layer = (M_Layer2D *) gf_sg_find_node_by_name(scene->graph, "ADDON_LAYER");
	if (!layer) return;
	layer->size.x = INT2FIX(v_w);
	layer->size.y = INT2FIX(v_h);
	gf_node_dirty_set((GF_Node *)layer, 0, 0);

	tr = (M_Transform2D *) gf_sg_find_node_by_name(scene->graph, "ADDON_TRANS");
	if (!tr) return;
	switch (position) {
	case 0:
		tr->translation.x = INT2FIX(w - v_w) / 2;
		tr->translation.y = INT2FIX(v_h - h) / 2;
		break;
	case 1:
		tr->translation.x = INT2FIX(w - v_w) / 2;
		tr->translation.y = INT2FIX(h - v_h) / 2;
		break;
	case 2:
		tr->translation.x = INT2FIX(v_w - w) / 2;
		tr->translation.y = INT2FIX(v_h - h) / 2;
		break;
	case 3:
		tr->translation.x = INT2FIX(v_w - w) / 2;
		tr->translation.y = INT2FIX(h - v_h) / 2;
		break;
	}
	gf_node_dirty_set((GF_Node *)tr, 0, 0);
}

GF_EXPORT
void gf_scene_resume_live(GF_Scene *subscene)
{
	if (subscene->main_addon_selected)
		mediacontrol_resume(subscene->root_od, 1);
}

void gf_scene_restart_dynamic(GF_Scene *scene, s64 from_time, Bool restart_only, Bool disable_addon_check)
{
	u32 i;
	GF_Clock *ck;
	GF_List *to_restart;
	GF_ObjectManager *odm;
	if (restart_only) {
		from_time = 0;
	}

	ck = scene->dyn_ck;
	if (scene->scene_codec) ck = scene->scene_codec->ck;
	if (!ck) return;

	//first pass to check if we need to enable the addon acting as time shifting
	if (!disable_addon_check) {
		i=0;
		while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {

			if (odm->addon && (odm->addon->addon_type==GF_ADDON_TYPE_MAIN)) {
				//assign clock if not yet available
				if (odm->addon->root_od->subscene && !odm->addon->root_od->subscene->dyn_ck)
					odm->addon->root_od->subscene->dyn_ck = scene->dyn_ck;

				//we're timeshifting through the main addon, activate it
				if (from_time < -1) {
					gf_scene_select_main_addon(scene, odm, GF_TRUE, gf_clock_time(ck));

					/*no timeshift, this is a VoD associated with the live broadcast: get current time*/
					if (! odm->timeshift_depth) {
						s64 live_clock = scene->obj_clock_at_main_activation + gf_sys_clock() - scene->sys_clock_at_main_activation;

						from_time += 1;
						if (live_clock + from_time < 0) from_time = 0;
						else from_time = live_clock + from_time;
					}
				} else if (scene->main_addon_selected) {
					gf_scene_select_main_addon(scene, odm, GF_FALSE, 0);
				}
			}
		}
	}

	to_restart = gf_list_new();
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
		if (gf_odm_shares_clock(odm, ck)) {
			if (odm->state != GF_ODM_STATE_BLOCKED) {

				//object is not an addon and main addon is selected, do not add
				if (!odm->addon && scene->main_addon_selected) {
				}
				//object is an addon and enabled, restart if main and main is enabled, or if not main
				else if (odm->addon && odm->addon->enabled) {
					if (odm->addon->addon_type==GF_ADDON_TYPE_MAIN) {
						if (scene->main_addon_selected) {
							gf_list_add(to_restart, odm);
						}
					} else {
						gf_list_add(to_restart, odm);
					}
				} else if (!scene->selected_service_id || (scene->selected_service_id==odm->OD->ServiceID)) {
					gf_list_add(to_restart, odm);
				}

				if (odm->state == GF_ODM_STATE_PLAY) {
					gf_odm_stop(odm, 1);
				}
			}
		}
	}

	if (!restart_only) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Scene] Restarting from "LLD"\n", LLD_CAST from_time));
		/*reset clock*/
		gf_clock_reset(ck);

		//used by SVG for JSAPIs..;
		if (!scene->is_dynamic_scene) gf_clock_set_time(ck, 0);
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Scene] Restarting scene from current clock %d\n", gf_clock_time(ck) ));
	}


	/*restart objects*/
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(to_restart, &i))) {
		if (from_time<0) {
			odm->media_stop_time = from_time + 1;
		} else {
			odm->media_start_time = from_time;
		}

		if (odm->subscene && odm->subscene->is_dynamic_scene) {
			gf_scene_restart_dynamic(odm->subscene, from_time, 0, 0);
		} else {
			gf_odm_start(odm, 0);
		}
	}
	gf_list_del(to_restart);

	/*also check nodes since they may be deactivated (end of stream)*/
	if (scene->is_dynamic_scene) {
		M_AudioClip *ac = (M_AudioClip *) gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO1");
		M_MovieTexture *mt = (M_MovieTexture *) gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO1");
		M_AnimationStream *as = (M_AnimationStream *) gf_sg_find_node_by_name(scene->graph, "DYN_TEXT");
		if (ac) {
			ac->startTime = gf_scene_get_time(scene);
			gf_node_changed((GF_Node *)ac, NULL);
		}
		if (mt) {
			mt->startTime = gf_scene_get_time(scene);
			gf_node_changed((GF_Node *)mt, NULL);
		}
		if (as) {
			as->startTime = gf_scene_get_time(scene);
			gf_node_changed((GF_Node *)as, NULL);
		}
	}
}

#endif /*GPAC_DISABLE_VRML*/


GF_EXPORT
void gf_scene_force_size(GF_Scene *scene, u32 width, u32 height)
{
	Bool skip_notif = GF_FALSE;

	/*for now only allowed when no scene info*/
	if (!scene->is_dynamic_scene) return;

	GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Scene] Forcing scene size to %d x %d\n", width, height));

	if (scene->vr_type) {
		/*for 360 don't set scene size to full-size , only half of it*/
		width /= 2;
		height /= 2;
		/*if we already processed a force size in 360, don't do it again*/
		if (scene->force_size_set)
			return;

#ifndef GPAC_DISABLE_VRML
		scene->force_size_set = GF_TRUE;
		if (! scene->srd_type) {
			GF_Node *node = gf_sg_find_node_by_name(scene->graph, "DYN_GEOM1");
			if (node && (((M_Sphere *)node)->radius == FIX_ONE)) {
				u32 radius = MAX(width, height)/4;

				((M_Sphere *)node)->radius = - INT2FIX(radius);
				gf_node_changed(node, NULL);
			}
		}
#endif /* GPAC_DISABLE_VRML */
	}

	if (scene->is_dynamic_scene) {
		GF_NetworkCommand com;

		memset(&com, 0, sizeof(GF_NetworkCommand));
		if (!scene->vr_type) {
			com.base.command_type = GF_NET_SERVICE_HAS_FORCED_VIDEO_SIZE;
			gf_term_service_command(scene->root_od->net_service, &com);
		}

		if (scene->root_od->term->root_scene == scene) {
			if (com.par.width && com.par.height) {
				gf_sc_set_scene_size(scene->root_od->term->compositor, width, height, 1);
				if (!scene->force_size_set) {
					gf_sc_set_size(scene->root_od->term->compositor, com.par.width, com.par.height);
					scene->force_size_set = 1;
				} else {
					gf_sc_set_size(scene->root_od->term->compositor, 0, 0);
				}
			} else {
				if (scene->vr_type) {
					width = MAX(width, height) / 2;
					gf_sg_set_scene_size_info(scene->graph, 0, 0, 1);
				} else {
					/*need output resize*/
					gf_sg_set_scene_size_info(scene->graph, width, height, 1);
					gf_sc_set_scene(scene->root_od->term->compositor, scene->graph);
					gf_sc_set_size(scene->root_od->term->compositor, width, height);
				}
			}

		} else if (!scene->force_size_set) {
			if (com.par.width && com.par.height) {
				width = com.par.width;
				height = com.par.height;
			}
			if (scene->vr_type) {
				gf_sg_set_scene_size_info(scene->graph, 0, 0, 1);
			} else {
				gf_sg_set_scene_size_info(scene->graph, width, height, 1);
			}
			scene->force_size_set = 1;
			} else {
			u32 w, h;
			gf_sg_get_scene_size_info(scene->graph, &w, &h);
			if (!com.par.width && !com.par.height && ((w<width) || (h<height)) ) {
				gf_sg_set_scene_size_info(scene->graph, width, height, 1);
			} else {
				GF_DOM_Event devt;
				memset(&devt, 0, sizeof(GF_DOM_Event));
				devt.type = GF_EVENT_SCENE_SIZE;
				devt.screen_rect.width = INT2FIX(width);
				devt.screen_rect.height = INT2FIX(height);

				devt.key_flags = scene->is_dynamic_scene ? (scene->vr_type ? 2 : 1) : 0;

				gf_scene_notify_event(scene, GF_EVENT_SCENE_SIZE, NULL, &devt, GF_OK, GF_FALSE);

				skip_notif = GF_TRUE;

				width = w;
				height = h;
			}
		}
	}
	else if (scene->root_od->parentscene && scene->root_od->parentscene->is_dynamic_scene) {
		gf_sg_set_scene_size_info(scene->root_od->parentscene->graph, width, height, gf_sg_use_pixel_metrics(scene->root_od->parentscene->graph));
		if (scene->root_od->term->root_scene == scene->root_od->parentscene) {
			if (width && height) {
				gf_sc_set_scene_size(scene->root_od->term->compositor, width, height, GF_TRUE);
				gf_sc_set_size(scene->root_od->term->compositor, width, height);
			}
		}
	}

	if (scene->vr_type) {
		gf_sg_set_scene_size_info(scene->graph, 0, 0, GF_TRUE);
	} else {
		gf_sg_set_scene_size_info(scene->graph, width, height, GF_TRUE);
	}
	if (scene->srd_type)
		gf_scene_regenerate(scene);

#ifndef GPAC_DISABLE_VRML
	IS_UpdateVideoPos(scene);
#endif

	if (skip_notif) return;

	gf_scene_notify_event(scene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, GF_OK, GF_FALSE);
}


GF_EXPORT
Bool gf_scene_process_anchor(GF_Node *caller, GF_Event *evt)
{
	GF_Terminal *term;
#ifndef GPAC_DISABLE_VRML
	u32 i;
	M_Inline *inl;
#endif
	GF_Scene *scene;
	GF_SceneGraph *sg = gf_node_get_graph(caller);
	if (!sg) return 1;
	scene = (GF_Scene *)gf_sg_get_private(sg);
	if (!scene) return 1;
	term = scene->root_od->term;

	/*if main scene forward to user. If no params or first one not "self" forward to user*/
	if ((term->root_scene==scene) || !evt->navigate.parameters || !evt->navigate.param_count || (stricmp(evt->navigate.parameters[0], "self") && stricmp(evt->navigate.parameters[0], "_self"))) {
		if (term->user->EventProc) return gf_term_send_event(term, evt);
		return 1;
	}

	if (!scene->root_od->mo) return 1;

	/*FIXME this is too restrictive, we assume the navigate URL is really a presentation one...*/
#ifndef GPAC_DISABLE_VRML
	i=0;
	while ((inl = (M_Inline*)gf_mo_event_target_enum_node(scene->root_od->mo, &i))) {
		switch (gf_node_get_tag((GF_Node *)inl)) {
		case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
		case TAG_X3D_Inline:
#endif
			gf_sg_vrml_mf_reset(&inl->url, GF_SG_VRML_MFURL);
			gf_sg_vrml_mf_alloc(&inl->url, GF_SG_VRML_MFURL, 1);
			inl->url.vals[0].url = gf_strdup(evt->navigate.to_url ? evt->navigate.to_url : "");
			/*signal URL change but don't destroy inline scene now since we got this event from inside the scene,
			this could crash compositors*/
			scene->needs_restart = 2;
			break;
		}
	}
#endif

	return 1;
}

GF_EXPORT
GF_Compositor *gf_sc_get_compositor(GF_Node *node)
{
	GF_Scene *scene;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	if (!sg) return NULL;
	scene = (GF_Scene *)gf_sg_get_private(sg);
	if (!scene) return NULL;
	return scene->root_od->term->compositor;
}

const char *gf_scene_get_fragment_uri(GF_Node *node)
{
	GF_SceneGraph *sg = gf_node_get_graph(node);
	GF_Scene *scene = sg ? (GF_Scene *) gf_sg_get_private(sg) : NULL;
	if (!scene) return NULL;
	return scene->fragment_uri;
}
void gf_scene_set_fragment_uri(GF_Node *node, const char *uri)
{
	GF_SceneGraph *sg = gf_node_get_graph(node);
	GF_Scene *scene = sg ? (GF_Scene *) gf_sg_get_private(sg) : NULL;
	if (!scene) return;
	if (scene->fragment_uri) {
		gf_free(scene->fragment_uri);
		scene->fragment_uri = NULL;
	}
	if (uri) scene->fragment_uri = gf_strdup(uri);
}


GF_Node *gf_scene_get_subscene_root(GF_Node *node)
{
	GF_Scene *scene;
	if (!node) return NULL;
	switch (gf_node_get_tag(node)) {
#ifndef GPAC_DISABLE_VRML
	case TAG_MPEG4_Inline:
#ifndef GPAC_DISABLE_X3D
	case TAG_X3D_Inline:
#endif
		break;
#endif
	default:
		return NULL;
	}
	scene = (GF_Scene *)gf_node_get_private(node);
	if (!scene) return NULL;
	if (!scene->graph) return NULL;
	return gf_sg_get_root_node(scene->graph);
}

/*returns 0 if any of the clock still hasn't seen EOS*/
Bool gf_scene_check_clocks(GF_ClientService *ns, GF_Scene *scene, Bool check_buffering)
{
	GF_Clock *ck;
	Bool initialized = GF_FALSE;
	u32 i;
	if (scene) {
		GF_ObjectManager *odm;
		if (scene->root_od->net_service != ns) {
			if (!gf_scene_check_clocks(scene->root_od->net_service, scene, check_buffering)) return 0;
		}
		i=0;
		while ( (odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i)) ) {
			if (odm->net_service && (odm->net_service != ns)) {
				if (!gf_scene_check_clocks(odm->net_service, NULL, check_buffering)) return 0;
			} else if (odm->codec && odm->codec->CB) {
				initialized = GF_TRUE;
				if (!check_buffering) {
					if (!gf_cm_is_eos(odm->codec->CB) ) {
						return 0;
					}
				} else {
					if (odm->codec->ck->Buffering) {
						return 0;
					}
				}
			}
		}
	}
	i=0;
	while ( (ck = (GF_Clock *)gf_list_enum(ns->Clocks, &i) ) ) {
		initialized = GF_TRUE;
		if (!check_buffering) {
			if (!ck->has_seen_eos) return 0;
		} else {
			if (ck->Buffering) return 0;
		}

	}

	if (!check_buffering && scene) {
		if (scene->scene_codec) {
			initialized = GF_TRUE;
			if (scene->scene_codec->Status != GF_ESM_CODEC_STOP) return 0;
		}
		if (scene->od_codec && (scene->od_codec->Status != GF_ESM_CODEC_STOP)) return 0;
	}
	if (!initialized) return 0;
	return 1;
}

const char *gf_scene_get_service_url(GF_SceneGraph *sg)
{
	GF_Scene *scene = gf_sg_get_private(sg);
	if (scene) return scene->root_od->net_service->url;
	return NULL;
}

Bool gf_scene_is_over(GF_SceneGraph *sg)
{
	u32 i, count;
	GF_Scene *scene = gf_sg_get_private(sg);
	if (!scene) return 0;
	if (scene->scene_codec)
		return (scene->scene_codec->Status==GF_ESM_CODEC_EOS) ? 1 : 0;

	count = gf_list_count(scene->resources);
	for (i=0; i<count; i++) {
		GF_ObjectManager *odm = gf_list_get(scene->resources, i);
		if (odm->codec && (odm->codec->Status != GF_ESM_CODEC_EOS) && (odm->codec->Status!=GF_ESM_CODEC_STOP)) return 0;
		if (odm->subscene && !gf_scene_is_over(odm->subscene->graph) ) return 0;
	}
	return 1;
}

GF_SceneGraph *gf_scene_enum_extra_scene(GF_SceneGraph *sg, u32 *i)
{
	GF_Scene *scene = gf_sg_get_private(sg);
	if (!scene) return NULL;
	return gf_list_enum(scene->extra_scenes, i);
}

Bool gf_scene_is_dynamic_scene(GF_SceneGraph *sg)
{
	GF_Scene *scene = gf_sg_get_private(sg);
	if (!scene) return 0;
	return scene->is_dynamic_scene ? 1 : 0;
}

#define USE_TEXTURES	0

void gf_scene_generate_views(GF_Scene *scene, char *url, char *parent_path)
{
#ifndef GPAC_DISABLE_VRML
	char *url_search;
	Bool use_old_syntax = 1;
	GF_Node *n1, *switcher;
#if USE_TEXTURES
	GF_Node *n2;
	M_MovieTexture *mt;
#else
	M_Inline *inl;
#endif
	GF_Event evt;
	gf_sc_node_destroy(scene->root_od->term->compositor, NULL, scene->graph);
	gf_sg_reset(scene->graph);

	scene->force_single_timeline = 1;
	n1 = is_create_node(scene->graph, TAG_MPEG4_OrderedGroup, NULL);
	gf_sg_set_root_node(scene->graph, n1);
	gf_node_register(n1, NULL);

	switcher = is_create_node(scene->graph, TAG_MPEG4_Switch, NULL);
	gf_node_register(switcher, n1);
	gf_node_list_add_child( &((GF_ParentNode *)n1)->children, switcher);
	((M_Switch*)switcher)->whichChoice = -2;

	if (strstr(url, "::")) use_old_syntax = 0;

	url_search = url;
	while (1) {
		char *sep;

		if (use_old_syntax) {
			sep = strchr(url_search, ':');
			/*if :// or :\ is found, skip it*/
			if (sep && ( ((sep[1] == '/') && (sep[2] == '/')) || (sep[1] == '\\') ) ) {
				url_search = sep+1;
				continue;
			}
		} else {
			sep = strstr(url_search, "::");
		}
		if (sep) sep[0] = 0;

#if USE_TEXTURES
		/*create a shape and bitmap node*/
		n2 = is_create_node(scene->graph, TAG_MPEG4_Shape, NULL);
		gf_node_list_add_child( &((M_Switch *)switcher)->choice, n2);
		gf_node_register(n2, switcher);
		n1 = n2;
		n2 = is_create_node(scene->graph, TAG_MPEG4_Appearance, NULL);
		((M_Shape *)n1)->appearance = n2;
		gf_node_register(n2, n1);

		/*note we create a movie texture even for images...*/
		mt = (M_MovieTexture *) is_create_node(scene->graph, TAG_MPEG4_MovieTexture, NULL);
		mt->startTime = gf_scene_get_time(scene);
		((M_Appearance *)n2)->texture = (GF_Node *)mt;
		gf_node_register((GF_Node *)mt, n2);

		n2 = is_create_node(scene->graph, TAG_MPEG4_Bitmap, NULL);
		((M_Shape *)n1)->geometry = n2;
		gf_node_register(n2, n1);

		gf_sg_vrml_mf_reset(&mt->url, GF_SG_VRML_MFURL);
		gf_sg_vrml_mf_append(&mt->url, GF_SG_VRML_MFURL, NULL);
		mt->url.vals[0].url = gf_url_concatenate(parent_path, url);
#else
		inl = (M_Inline *) is_create_node(scene->graph, TAG_MPEG4_Inline, NULL);
		gf_node_list_add_child( &((M_Switch *)switcher)->choice, (GF_Node *)inl);
		gf_node_register((GF_Node*) inl, switcher);

		gf_sg_vrml_mf_reset(&inl->url, GF_SG_VRML_MFURL);
		gf_sg_vrml_mf_append(&inl->url, GF_SG_VRML_MFURL, NULL);
		inl->url.vals[0].url = gf_url_concatenate(parent_path, url);
#endif

		if (!sep) break;
		sep[0] = ':';
		if (use_old_syntax) {
			url = sep+1;
		} else {
			url = sep+2;
		}
		url_search = url;
	}

	gf_sc_set_option(scene->root_od->term->compositor, GF_OPT_USE_OPENGL, 1);

	scene->is_dynamic_scene = 2;
	gf_sg_set_scene_size_info(scene->graph, 0, 0, 1);

	gf_scene_attach_to_compositor(scene);

	evt.type = GF_EVENT_CONNECT;
	evt.connect.is_connected = 1;
	gf_term_send_event(scene->root_od->term, &evt);
#endif
}

void gf_scene_generate_mosaic(GF_Scene *scene, char *url, char *parent_path)
{
#ifndef GPAC_DISABLE_VRML
	char *url_search, *cur_url;
	Bool use_old_syntax = 1;
	GF_Node *n1;
	M_Inline *inl;
	Bool first_pass = GF_TRUE;
	u32 nb_items=0, nb_rows=0, nb_cols=0;
	s32 width=1920, height=1080, x, y, tw=0, th=0;

	GF_Event evt;
	gf_sc_node_destroy(scene->root_od->term->compositor, NULL, scene->graph);
	gf_sg_reset(scene->graph);

	scene->force_single_timeline = GF_FALSE;
	n1 = is_create_node(scene->graph, TAG_MPEG4_OrderedGroup, NULL);
	gf_sg_set_root_node(scene->graph, n1);
	gf_node_register(n1, NULL);

	if (strstr(url, "::")) use_old_syntax = 0;

restart:
	url_search = cur_url = url;
	x = y = 0;
	while (1) {
		char *sep;

		if (use_old_syntax) {
			sep = strchr(url_search, ':');
			/*if :// or :\ is found, skip it*/
			if (sep && ( ((sep[1] == '/') && (sep[2] == '/')) || (sep[1] == '\\') ) ) {
				url_search = sep+1;
				continue;
			}
		} else {
			sep = strstr(url_search, "::");
		}
		if (sep) sep[0] = 0;

		if (first_pass) {
			nb_items ++;
		} else {
			GF_Node *tr = is_create_node(scene->graph, TAG_MPEG4_Transform2D, NULL);
			GF_Node *layer = is_create_node(scene->graph, TAG_MPEG4_Layer2D, NULL);
			gf_node_register(tr, n1);
			gf_node_list_add_child( &((GF_ParentNode *)n1)->children, tr);

			((M_Transform2D *)tr)->translation.x = (Fixed)(-width/2 + tw/2 + x*tw);
			((M_Transform2D *)tr)->translation.y = (Fixed)(height/2 - th/2 - y*th);

			x++;
			if (x==nb_cols) {
				y++;
				x=0;
			}

			gf_node_register(layer, tr);
			gf_node_list_add_child( &((M_Transform2D *)tr)->children, layer);
			((M_Layer2D *)layer)->size.x = INT2FIX(tw);
			((M_Layer2D *)layer)->size.y = INT2FIX(th);

			inl = (M_Inline *) is_create_node(scene->graph, TAG_MPEG4_Inline, NULL);
			gf_node_list_add_child( &((M_Layer2D *)layer)->children, (GF_Node *)inl);
			gf_node_register((GF_Node*) inl, layer);

			gf_sg_vrml_mf_reset(&inl->url, GF_SG_VRML_MFURL);
			gf_sg_vrml_mf_append(&inl->url, GF_SG_VRML_MFURL, NULL);
			inl->url.vals[0].url = gf_url_concatenate(parent_path, cur_url);
		}

		if (!sep) break;
		sep[0] = ':';
		if (use_old_syntax) {
			cur_url = sep+1;
		} else {
			cur_url = sep+2;
		}
		url_search = cur_url;
	}
	if (first_pass) {
		first_pass = GF_FALSE;
		nb_cols=(u32) gf_ceil( gf_sqrt(nb_items) );
		nb_rows=nb_items/nb_cols;
		if (nb_cols * nb_rows < nb_items) nb_rows++;
		tw = width/nb_cols;
		th = height/nb_rows;
		goto restart;
	}

	scene->is_dynamic_scene = 2;
	gf_sg_set_scene_size_info(scene->graph, width, height, 1);

	gf_scene_attach_to_compositor(scene);

	evt.type = GF_EVENT_CONNECT;
	evt.connect.is_connected = 1;
	gf_term_send_event(scene->root_od->term, &evt);
#endif
}

void gf_scene_reset_addon(GF_AddonMedia *addon, Bool disconnect)
{
	if (addon->root_od) {
		GF_Terminal *term = addon->root_od->term;
		gf_term_lock_net(term, GF_TRUE);
		addon->root_od->addon = NULL;
		if (disconnect) {				
			gf_scene_remove_object(addon->root_od->parentscene, addon->root_od, 2);
			gf_odm_disconnect(addon->root_od, 1);
		}
		gf_term_lock_net(term, GF_FALSE);
	}

	if (addon->url) gf_free(addon->url);
	gf_free(addon);
}

void gf_scene_reset_addons(GF_Scene *scene)
{
	while (gf_list_count(scene->declared_addons)) {
		GF_AddonMedia *addon = gf_list_last(scene->declared_addons);
		gf_list_rem_last(scene->declared_addons);

		gf_scene_reset_addon(addon, GF_FALSE);
	}
}

static void load_associated_media(GF_Scene *scene, GF_AddonMedia *addon)
{
	GF_MediaObject *mo;
	MFURL url;
	SFURL sfurl;

	if (!addon->enabled) return;

	url.count=1;
	url.vals = &sfurl;
	url.vals[0].OD_ID = GF_MEDIA_EXTERNAL_ID;
	url.vals[0].url = (char *)addon->url;

	//we may need to change the object type once we have more ideas what the external resource is about.
	//By default we start with scene
	//we force the timeline of the addon to be locked with the main scene
	mo = gf_scene_get_media_object(scene, &url, GF_MEDIA_OBJECT_SCENE, GF_TRUE);

	if (!mo || !mo->odm) {
		assert(0);
		return;
	}

	addon->root_od = mo->odm;
	mo->odm->addon = addon;
}

GF_EXPORT
void gf_scene_register_associated_media(GF_Scene *scene, GF_AssociatedContentLocation *addon_info)
{
	GF_AddonMedia *addon = NULL;
	GF_Event evt;
	u32 i, count;
	Bool new_addon = 0;

	if (!scene->is_dynamic_scene) return;

	count = gf_list_count(scene->declared_addons);
	for (i=0; i<count; i++) {
		Bool my_addon = 0;
		addon = gf_list_get(scene->declared_addons, i);
		if ((addon_info->timeline_id>=0) && addon->timeline_id==addon_info->timeline_id) {
			my_addon = 1;
		} else if (addon->url && addon_info->external_URL && !strcmp(addon->url, addon_info->external_URL)) {
			my_addon = 1;
			//send message to service handler
		}
		//this is an already received addon
		if (my_addon) {
			if (addon_info->disable_if_defined) {
				addon->enabled = GF_FALSE;

				if (addon->root_od) {
					gf_scene_toggle_addons(scene, GF_FALSE);
					gf_scene_remove_object(addon->root_od->parentscene, addon->root_od, 2);
					gf_odm_disconnect(addon->root_od, 1);
				}
				if (addon->root_od) {
					addon->root_od->addon = NULL;
				}
				return;
			}

			if (addon_info->enable_if_defined)
				addon->enabled = GF_TRUE;

			//declaration of start time
			if (addon->is_splicing && (addon->splice_start<0) && addon_info->is_splicing) {
				addon->splice_in_pts = addon_info->splice_time_pts;

				if (addon->splice_in_pts) {
					addon->media_pts = (u64) (addon_info->splice_start_time);
					addon->splice_start = addon_info->splice_start_time / 90;
					addon->splice_end = addon_info->splice_end_time / 90;
				} else {
					addon->media_pts = (u64)(addon_info->splice_start_time * 90000);
					addon->splice_start = addon_info->splice_start_time * 1000;
					addon->splice_end = addon_info->splice_end_time * 1000;
				}
			}
			
			//restart addon
			if (!addon->root_od && addon->timeline_ready && addon->enabled) {
				load_associated_media(scene, addon);
			}
			//nothing associated, deactivate addon
			if (!addon_info->external_URL || !strlen(addon_info->external_URL) ) {
				gf_list_rem(scene->declared_addons, i);
				gf_scene_reset_addon(addon, GF_TRUE);
				gf_scene_toggle_addons(scene, GF_FALSE);
			} else if (strcmp(addon_info->external_URL, addon->url)) {
				//reconfigure addon
				gf_free(addon->url);
				addon->url = NULL;
				break;
			}
			return;
		}
		addon = NULL;
	}

	if (!addon_info->external_URL || !strlen(addon_info->external_URL) ) {
		return;
	}

	if (!addon) {
		GF_SAFEALLOC(addon, GF_AddonMedia);
		if (!addon) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Terminal] Failed to allocate media addon\n"));
			return;
		}
		addon->timeline_id = addon_info->timeline_id;
		gf_list_add(scene->declared_addons, addon);
		new_addon = 1;
	}

	addon->is_splicing = addon_info->is_splicing;
	addon->activation_time = gf_scene_get_time(scene)+addon_info->activation_countdown;
	addon->url = gf_strdup(addon_info->external_URL);
	addon->media_timescale = 1;
	addon->timeline_ready = (addon_info->timeline_id<0) ? 1 : 0;
	addon->splice_in_pts = addon_info->splice_time_pts;
	if (addon_info->is_splicing) {
		addon->addon_type = GF_ADDON_TYPE_SPLICED;
		scene->has_splicing_addons = GF_TRUE;

		if (addon->splice_in_pts) {
			addon->media_pts = (u64) (addon_info->splice_start_time);
			addon->splice_start = addon_info->splice_start_time / 90;
			addon->splice_end = addon_info->splice_end_time / 90;
		} else {
			addon->media_pts = (u64)(addon_info->splice_start_time * 90000);

			addon->splice_start = addon_info->splice_start_time * 1000;
			addon->splice_end = addon_info->splice_end_time * 1000;
		}
	} else {
		addon->splice_start = addon_info->splice_start_time;
		addon->splice_end = addon_info->splice_end_time;
	}

	if (!new_addon) return;

	//notify we found a new addon

	if (! scene->root_od->parentscene) {
		evt.type = GF_EVENT_ADDON_DETECTED;
		evt.addon_connect.addon_url = addon->url;
		addon->enabled = gf_term_send_event(scene->root_od->term,&evt);

		if (addon->timeline_ready)
			load_associated_media(scene, addon);
	} else {
		GF_DOM_Event devt;
		memset(&devt, 0, sizeof(GF_DOM_Event));
		devt.type = GF_EVENT_ADDON_DETECTED;
		devt.addon_url = addon->url;
		addon->enabled = 0;

		gf_scene_notify_event(scene, GF_EVENT_SCENE_ATTACHED, NULL, &devt, GF_OK, GF_TRUE);
	}

}

void gf_scene_notify_associated_media_timeline(GF_Scene *scene, GF_AssociatedContentTiming *addon_time)
{
	Double prev_time;
	GF_AddonMedia *addon = NULL;

	u32 i, count = gf_list_count(scene->declared_addons);
	for (i=0; i<count; i++) {
		addon = gf_list_get(scene->declared_addons, i);
		if (addon->timeline_id==addon_time->timeline_id)
			break;
		addon = NULL;
	}
	if (!addon) return;

	count = i;
	for (i=0; i<count; i++) {
		GF_AddonMedia *prev_addon = gf_list_get(scene->declared_addons, i);
		//we are adding a non splicing point: discard all previously declared addons
		if (!addon->is_splicing
		        //this is a splicing point, discard all previsously declared splicing addons
		        || prev_addon->is_splicing
		   ) {
			gf_scene_reset_addon(prev_addon, GF_TRUE);
			gf_list_rem(scene->declared_addons, i);
			i--;
			count--;
		}
	}


	prev_time = (Double) addon->media_timestamp;
	prev_time /= addon->media_timescale;

	//loop has been detected
	if ( prev_time  * addon_time->media_timescale > addon_time->media_timestamp + 1.5 * addon_time->media_timescale ) {
		if (!addon->loop_detected) {
			addon->loop_detected = GF_TRUE;
			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("Loop detected in addon - PTS "LLD" (CTS %d) - media time "LLD"\n", addon_time->media_pts, addon_time->media_pts/90, addon_time->media_timestamp));
			addon->past_media_pts = addon_time->media_pts;
			addon->past_media_timestamp = addon_time->media_timestamp;
			addon->past_media_timescale = addon_time->media_timescale;
			addon->past_media_pts_scaled = addon_time->media_pts/90;
		}
	} else if (!addon->loop_detected) {
		addon->media_pts = addon_time->media_pts;
		addon->media_timestamp = addon_time->media_timestamp;
		addon->media_timescale = addon_time->media_timescale;
		assert(addon_time->media_timescale);
		assert(!addon->loop_detected);
	}

	if (!addon->timeline_ready) {
		addon->timeline_ready = GF_TRUE;
		load_associated_media(scene, addon);
	}

	if ((addon->addon_type==GF_ADDON_TYPE_MAIN) && addon->root_od && addon->root_od->duration && !addon->root_od->timeshift_depth) {
		Double dur, tsb;
		dur = (Double) addon->root_od->duration;
		dur /= 1000;
		tsb = (Double) addon->media_timestamp;
		tsb /= addon->media_timescale;
		if (tsb>dur) tsb = dur;
		addon->root_od->parentscene->root_od->timeshift_depth = (u32) (1000*tsb);
		gf_scene_set_timeshift_depth(scene);
	}

	//and forward ntp if any to underlying service
	if (addon_time->ntp && addon->root_od && addon->root_od->net_service) {
		GF_NetworkCommand com;
		memset(&com, 0, sizeof(com));
		com.addon_time = *addon_time;
		gf_term_service_command(addon->root_od->net_service, &com);
	}
}

Bool gf_scene_check_addon_restart(GF_AddonMedia *addon, u64 cts, u64 dts)
{
	u32 i;
	GF_ObjectManager*odm;
	GF_Scene *subscene;
	GF_List *to_restart = NULL;

	if (!addon || !addon->loop_detected) return GF_FALSE;
	//warning, we need to compare to media PTS/90 since we already rounded the media_ts to milliseconds (otherwise we would get rounding errors).
	if ((cts == addon->past_media_pts_scaled) || (dts >= addon->past_media_pts_scaled) ) {
	} else {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("Loop not yet active - CTS "LLD" DTS "LLD" media TS "LLD" \n", cts, dts, addon->past_media_pts_scaled));
		return GF_FALSE;
	}

	gf_mx_p(addon->root_od->mx);

	addon->loop_detected = 0;
	addon->media_pts = addon->past_media_pts;
	addon->media_timestamp = addon->past_media_timestamp;
	addon->media_timescale = addon->past_media_timescale;
	assert(addon->past_media_timescale);
	addon->past_media_pts = 0;
	addon->past_media_timestamp = 0;
	addon->past_media_timescale = 0;

	subscene = addon->root_od->subscene;

	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("Looping addon - CTS "LLD" - addon media TS "LLD" (CTS "LLD") addon media time "LLD"\n", cts, addon->media_pts, addon->media_pts/90, addon->media_timestamp));

	gf_mx_v(addon->root_od->mx);

	to_restart = gf_list_new();

	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(subscene->resources, &i))) {
		if (odm->state == GF_ODM_STATE_PLAY) {
			gf_list_add(to_restart, odm);
		}
		gf_odm_stop(odm, GF_FALSE);
	}

	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(to_restart, &i))) {
		gf_odm_start(odm, 2);
	}
	gf_list_del(to_restart);
	return GF_TRUE;
}

Double gf_scene_adjust_time_for_addon(GF_AddonMedia *addon, Double clock_time, u32 *timestamp_based)
{
	Double media_time;
	if (!addon->timeline_ready)
		return clock_time;

	if (timestamp_based)
		*timestamp_based = (addon->timeline_id>=0) ? 0 : 1;

	if (addon->is_splicing) {
		return ((Double)addon->media_timestamp) / addon->media_timescale;
	}

	//get PTS diff (clock is in ms, pt is in 90k)
	media_time = clock_time;
	media_time -= addon->media_pts/90000.0;

	media_time += ((Double)addon->media_timestamp) / addon->media_timescale;
	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("Addon about to start - media time %g\n", media_time));
	return media_time;
}

s64 gf_scene_adjust_timestamp_for_addon(GF_AddonMedia *addon, u64 orig_ts)
{
	s64 media_ts_ms;
	assert(addon->timeline_ready);
	if (addon->is_splicing) {
		if (!addon->min_dts_set || (orig_ts<addon->splice_min_dts)) {
			addon->splice_min_dts = orig_ts;
			addon->min_dts_set = GF_TRUE;
		}
		orig_ts -= addon->splice_min_dts;
	}
	media_ts_ms = orig_ts;
	media_ts_ms -= (addon->media_timestamp*1000) / addon->media_timescale;
	media_ts_ms += (addon->media_pts/90);
	return media_ts_ms;
}

void gf_scene_select_scalable_addon(GF_Scene *scene, GF_ObjectManager *odm)
{
	GF_NetworkCommand com;
	GF_CodecCapability caps;
	Bool nalu_annex_b;
	GF_Channel *ch, *base_ch;
	GF_ObjectManager *odm_base = NULL;
	u32 i, count, mtype;
	Bool force_attach=GF_FALSE;
	ch = gf_list_get(odm->channels, 0);
	if (!ch->esd) return;
	mtype = ch->esd->decoderConfig->streamType;
	count = gf_list_count(scene->resources);
	for (i=0; i<count; i++) {
		odm_base = gf_list_get(scene->resources, i);
		if ((mtype==odm_base->codec->type) && odm_base->codec)
			break;
		odm_base=NULL;
		//todo
		//1- check if we use compatible formats, for now we only do demos with hevc/lhvc
		//2- check dependency IDs if any, for now we only do demos with 2 layers hevc/lhvc
	}
	if (!odm_base) return;

	base_ch = gf_list_get(odm_base->channels, 0);

	switch (base_ch->esd->decoderConfig->objectTypeIndication) {
	case GPAC_OTI_VIDEO_AVC:
	case GPAC_OTI_VIDEO_SVC:
	case GPAC_OTI_VIDEO_MVC:
		switch (ch->esd->decoderConfig->objectTypeIndication) {
		case GPAC_OTI_VIDEO_LHVC:
			if (!odm_base->codec->hybrid_layered_coded) {
				force_attach=GF_TRUE;
			}
			odm_base->codec->hybrid_layered_coded=GF_TRUE;
			break;
		}
		break;
	case GPAC_OTI_VIDEO_HEVC:
		force_attach=GF_TRUE;
		break;
	}

	if (odm_base->upper_layer_odm) {
		force_attach=GF_FALSE;
	} else {
		odm_base->upper_layer_odm = odm;
	}
	odm->lower_layer_odm = odm_base;

	nalu_annex_b = 1;
	if (base_ch->esd->decoderConfig->decoderSpecificInfo && base_ch->esd->decoderConfig->decoderSpecificInfo->dataLength)
		nalu_annex_b = 0;

	if (0 && odm_base->codec->hybrid_layered_coded && ch->esd->decoderConfig->decoderSpecificInfo && ch->esd->decoderConfig->decoderSpecificInfo->dataLength) {
		nalu_annex_b = 0;
		if (force_attach) {
			odm_base->codec->decio->AttachStream(odm_base->codec->decio, ch->esd);
		}
	} else if (force_attach) {
		//we force annexB mode, delete avcC/hvcC
		if (nalu_annex_b && ch->esd->decoderConfig->decoderSpecificInfo) {
			gf_odf_desc_del((GF_Descriptor *)ch->esd->decoderConfig->decoderSpecificInfo);
			ch->esd->decoderConfig->decoderSpecificInfo=NULL;
		}
		odm_base->codec->decio->AttachStream(odm_base->codec->decio, ch->esd);
	}
	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.command_type = GF_NET_CHAN_NALU_MODE;
	//force AnnexB mode and no sync sample seeking
	com.nalu_mode.extract_mode = nalu_annex_b ? 2 : 0;
	count = gf_list_count(odm->channels);
	for (i=0; i<count; i++) {
		com.base.on_channel = ch = gf_list_get(odm->channels, i);
		//we must wait for RAP otherwise we won't be able to detect temporal scalability correctly
		ch->stream_state = 1;
		ch->media_padding_bytes = base_ch->media_padding_bytes;
		gf_term_service_command(ch->service, &com);

	}

	caps.CapCode = GF_CODEC_MEDIA_SWITCH_QUALITY;
	// splicing, signal to the base decoder that we will want low quality and wait for splice activation
	if (odm->parentscene->root_od->addon->is_splicing) {
		caps.cap.valueInt = 0;
	}
	//not splicing, signal to the base decoder that we will want full quality right now
	else {
		caps.cap.valueInt = 2;
	}
	odm_base->codec->decio->SetCapabilities(odm_base->codec->decio, caps);

}
