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

	tmp->resources = gf_list_new();
	tmp->scene_objects = gf_list_new();
	tmp->extra_scenes = gf_list_new();
	/*init inline scene*/
	if (parentScene) {
		tmp->graph = gf_sg_new_subscene(parentScene->graph);
	} else {
		tmp->graph = gf_sg_new();
	}

	gf_sg_set_private(tmp->graph, tmp);
	gf_sg_set_node_callback(tmp->graph, gf_term_node_callback);
	gf_sg_set_scene_time_callback(tmp->graph, gf_scene_get_time);

#ifndef GPAC_DISABLE_VRML
	tmp->extern_protos = gf_list_new();
	gf_sg_set_proto_loader(tmp->graph, gf_inline_get_proto_lib);
	
	tmp->storages = gf_list_new();
	tmp->keynavigators = gf_list_new();
	
#endif
	tmp->on_media_event = inline_on_media_event;
	return tmp;
}

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

	if (scene->audio_url.url) gf_free(scene->audio_url.url);
	if (scene->visual_url.url) gf_free(scene->visual_url.url);
	if (scene->text_url.url) gf_free(scene->text_url.url);
	if (scene->dims_url.url) gf_free(scene->dims_url.url);
	if (scene->fragment_uri) gf_free(scene->fragment_uri);
	if (scene->redirect_xml_base) gf_free(scene->redirect_xml_base);

	gf_mx_v(scene->root_od->term->net_mx);
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

	/*release the scene - at this stage, we no longer have any node stack refering to our media objects */
	if (dec && dec->ReleaseScene) dec->ReleaseScene(dec);
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
	gf_list_add(scene->resources, odm);
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
	u32 i, j, max_buffer, cur_buffer;
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
			/*count only re-buffering channels*/
			if (!ch->BufferOn) continue;

			max_buffer += ch->MaxBuffer;
			cur_buffer += (ch->BufferTime>0) ? ch->BufferTime : 1;
		}
	}

	evt.type = GF_EVENT_PROGRESS;
	evt.progress.progress_type = 0;
	evt.progress.service = scene->root_od->net_service->url;
	if (!max_buffer || !cur_buffer || (max_buffer <= cur_buffer)) {
		evt.progress.done = evt.progress.total = max_buffer;
	} else {
		evt.progress.done = cur_buffer;
		evt.progress.total = max_buffer;
	}
	gf_term_send_event(scene->root_od->term, &evt);
}



void gf_scene_notify_event(GF_Scene *scene, u32 event_type, GF_Node *n, void *_event, GF_Err code)
{
	/*fire resize event*/
#ifndef GPAC_DISABLE_SVG
	GF_Node *root;
	u32 i, count;
	u32 w, h;
	GF_DOM_Event evt, *event;
	event = (GF_DOM_Event *)_event;

	if (!scene) return;
	root = gf_sg_get_root_node(scene->graph);

	if (!event) {
		memset(&evt, 0, sizeof(GF_DOM_Event));
		event = &evt;
		w = h = 0;
		gf_sg_get_scene_size_info(scene->graph, &w, &h);
		evt.type = event_type;
		evt.screen_rect.width = INT2FIX(w);
		evt.screen_rect.height = INT2FIX(h);
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
		gf_dom_event_fire(n, event);
	} else {
		if (root) gf_dom_event_fire(root, event);
	
		count=scene->root_od->mo ? gf_mo_event_target_count(scene->root_od->mo) : 0;
		for (i=0;i<count; i++) {
			gf_dom_event_fire(gf_event_target_get_node(gf_mo_event_target_get(scene->root_od->mo, i)), event);
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
		for (i=0;i<count; i++) {
			gf_node_dirty_parents( gf_event_target_get_node(gf_mo_event_target_get(scene->root_od->mo, i)));
		}
		gf_term_invalidate_compositor(scene->root_od->term);

		if (scene->root_od->parentscene->is_dynamic_scene) {
			u32 w, h;
			gf_sg_get_scene_size_info(scene->graph, &w, &h);
			gf_sc_set_size(scene->root_od->term->compositor, w, h);
		}
		/*trigger a scene attach event*/
		gf_scene_notify_event(scene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, GF_OK);
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

GF_MediaObject *gf_scene_get_media_object_ex(GF_Scene *scene, MFURL *url, u32 obj_type_hint, Bool lock_timelines, GF_MediaObject *sync_ref, Bool force_new_if_not_attached, GF_Node *node)
{
	GF_MediaObject *obj;
	GF_Scene *original_parent_scene = NULL;
	Bool keep_fragment = GF_TRUE;
	Bool first_pass = force_new_if_not_attached ? GF_FALSE : GF_TRUE;
	u32 i, OD_ID;

	OD_ID = gf_mo_get_od_id(url);
	if (!OD_ID) return NULL;

	gf_term_lock_net(scene->root_od->term, GF_TRUE);

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
			gf_term_lock_net(scene->root_od->term, GF_FALSE);
			return obj;
		}
		/*special case where the URL is requested twice for the same node: use the existing resource*/
		else if (node && (gf_mo_event_target_find_by_node(obj, node)>=0)) {
			gf_term_lock_net(scene->root_od->term, GF_FALSE);
			return obj;
		}
	}
	if (first_pass) {
		first_pass = GF_FALSE;
		goto restart;
	}

	/*we cannot create an OD manager at this point*/
	if (obj_type_hint==GF_MEDIA_OBJECT_UNDEF) {
		gf_term_lock_net(scene->root_od->term, GF_FALSE);
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
			gf_term_lock_net(scene->root_od->term, GF_FALSE);
			return NULL;
		}

		if (obj->odm==NULL) {
			gf_list_del_item(scene->scene_objects, obj); 
			gf_mo_del(obj);
			gf_term_lock_net(scene->root_od->term, GF_FALSE);
			return NULL;
		}
	}

	gf_term_lock_net(scene->root_od->term, GF_FALSE);
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
		if (odm->mo->speed != FIX_ONE) gf_odm_set_speed(odm, odm->mo->speed);
	}
	if ((odm->mo->type==GF_MEDIA_OBJECT_VIDEO) && scene->is_dynamic_scene) {
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
			gf_node_event_out_str((GF_Node *) media_sens->sensor, "mediaDuration");
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

GF_MediaObject *gf_scene_find_object(GF_Scene *scene, u16 ODID, char *url)
{
	u32 i;
	GF_MediaObject *mo;
	if (!url && !ODID) return NULL;
	i=0;
	while ((mo = (GF_MediaObject *)gf_list_enum(scene->scene_objects, &i))) {
		if (ODID==GF_MEDIA_EXTERNAL_ID) {
			if (mo->URLs.count && !stricmp(mo->URLs.vals[0].url, url)) return mo;
		} else if (mo->OD_ID==ODID) return mo;
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

	url.count = 1;
	url.vals = &scene->visual_url;
	mo = IS_CheckExistingObject(scene, &url, GF_MEDIA_OBJECT_VIDEO);
	if (!mo) return;
	tr = (M_Transform2D *) gf_sg_find_node_by_name(scene->graph, "DYN_TRANS");
	if (!tr) return;

	gf_sg_get_scene_size_info(scene->graph, &w, &h);
	if (!w || !h) return;

	gf_scene_get_video_size(mo, &v_w, &v_h);
	tr->translation.x = INT2FIX((s32) (w - v_w)) / 2;
	tr->translation.y = INT2FIX((s32) (h - v_h)) / 2;
	gf_node_dirty_set((GF_Node *)tr, 0, 0);

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
	if (!media_url->OD_ID) {
		u32 i=0;
		GF_ObjectManager *odm = NULL;
		while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
			if (type==GF_STREAM_TEXT) {
				if (!odm->codec || ((odm->codec->type!=type) && (odm->codec->type!=GF_STREAM_ND_SUBPIC))) continue;
			}
			else if (type==GF_STREAM_SCENE) {
				if (!odm->subscene || (!odm->subscene->scene_codec && !odm->subscene->is_dynamic_scene) ) continue;
			}
			else {
				if (!odm->codec || (odm->codec->type!=type)) continue;
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
				gf_sg_set_scene_size_info(scene->graph, w, h, 1);
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

/*regenerates the scene graph for dynamic scene.
This will also try to reload any previously presented streams. Note that in the usual case the scene is generated
just once when receiving the first OD AU (ressources are NOT destroyed when seeking), but since the network may need
to update the OD ressources, we still take care of it*/
void gf_scene_regenerate(GF_Scene *scene)
{
	GF_Node *n1, *n2;
	GF_Event evt;
	M_AudioClip *ac;
	M_MovieTexture *mt;
	M_AnimationStream *as;
	M_Inline *dims;

	if (scene->is_dynamic_scene != 1) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Inline] Regenerating scene graph for service %s\n", scene->root_od->net_service->url));

	gf_sc_lock(scene->root_od->term->compositor, 1);

	ac = (M_AudioClip *) gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO");
	
	/*this is the first time, generate a scene graph*/
	if (!ac) {
		/*create an OrderedGroup*/
		n1 = is_create_node(scene->graph, TAG_MPEG4_OrderedGroup, NULL);
		gf_sg_set_root_node(scene->graph, n1);
		gf_node_register(n1, NULL);

		if (! scene->root_od->parentscene) {
			n2 = is_create_node(scene->graph, TAG_MPEG4_Background2D, "DYN_BACK");
			gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
			gf_node_register(n2, n1);
		}

		/*create an sound2D and an audioClip node*/
		n2 = is_create_node(scene->graph, TAG_MPEG4_Sound2D, NULL);
		gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
		gf_node_register(n2, n1);

		ac = (M_AudioClip *) is_create_node(scene->graph, TAG_MPEG4_AudioClip, "DYN_AUDIO");
		ac->startTime = gf_scene_get_time(scene);
		((M_Sound2D *)n2)->source = (GF_Node *)ac;
		gf_node_register((GF_Node *)ac, n2);


		/*transform for any translation due to scene resize (3GPP)*/
		n2 = is_create_node(scene->graph, TAG_MPEG4_Transform2D, "DYN_TRANS");
		gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
		gf_node_register(n2, n1);
		n1 = n2;
		
		/*create a shape and bitmap node*/
		n2 = is_create_node(scene->graph, TAG_MPEG4_Shape, NULL);
		gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
		gf_node_register(n2, n1);
		n1 = n2;
		n2 = is_create_node(scene->graph, TAG_MPEG4_Appearance, NULL);
		((M_Shape *)n1)->appearance = n2;
		gf_node_register(n2, n1);

		/*note we create a movie texture even for images...*/
		mt = (M_MovieTexture *) is_create_node(scene->graph, TAG_MPEG4_MovieTexture, "DYN_VIDEO");
		mt->startTime = gf_scene_get_time(scene);
		((M_Appearance *)n2)->texture = (GF_Node *)mt;
		gf_node_register((GF_Node *)mt, n2);

		n2 = is_create_node(scene->graph, TAG_MPEG4_Bitmap, NULL);
		((M_Shape *)n1)->geometry = n2;
		gf_node_register(n2, n1);


		/*text streams controlled through AnimationStream*/
		n1 = gf_sg_get_root_node(scene->graph);
		as = (M_AnimationStream *) is_create_node(scene->graph, TAG_MPEG4_AnimationStream, "DYN_TEXT");
		gf_node_list_add_child( &((GF_ParentNode *)n1)->children, (GF_Node*)as);
		gf_node_register((GF_Node *)as, n1);


		/*3GPP DIMS streams controlled */
		n1 = gf_sg_get_root_node(scene->graph);
		dims = (M_Inline *) is_create_node(scene->graph, TAG_MPEG4_Inline, "DYN_SCENE");
		gf_node_list_add_child( &((GF_ParentNode *)n1)->children, (GF_Node*)dims);
		gf_node_register((GF_Node *)dims, n1);

	}

	ac = (M_AudioClip *) gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO");
	set_media_url(scene, &scene->audio_url, (GF_Node*)ac, &ac->url, GF_STREAM_AUDIO);

	mt = (M_MovieTexture *) gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO");
	set_media_url(scene, &scene->visual_url, (GF_Node*)mt, &mt->url, GF_STREAM_VISUAL);

	as = (M_AnimationStream *) gf_sg_find_node_by_name(scene->graph, "DYN_TEXT");
	set_media_url(scene, &scene->text_url, (GF_Node*)as, &as->url, GF_STREAM_TEXT);

	dims = (M_Inline *) gf_sg_find_node_by_name(scene->graph, "DYN_SCENE");
	set_media_url(scene, &scene->dims_url, (GF_Node*)dims, &dims->url, GF_STREAM_SCENE);

	gf_sc_lock(scene->root_od->term->compositor, 0);
	
	/*disconnect to force resize*/
	if (scene->root_od->term->root_scene == scene) {
		gf_sc_set_scene(scene->root_od->term->compositor, scene->graph);
		scene->graph_attached = 1;
		evt.type = GF_EVENT_STREAMLIST;
		gf_term_send_event(scene->root_od->term, &evt);
		IS_UpdateVideoPos(scene);
	} else {
		scene->graph_attached = 1;
		gf_scene_notify_event(scene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, GF_OK);
		gf_term_invalidate_compositor(scene->root_od->term);
	}
}
#else
/*!!fixme - we would need an SVG scene in case no VRML support is present !!!*/
void gf_scene_regenerate(GF_Scene *scene) {}
void gf_scene_restart_dynamic(GF_Scene *scene, u64 from_time) {}
void gf_scene_select_object(GF_Scene *scene, GF_ObjectManager *odm) {}
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

void gf_scene_select_object(GF_Scene *scene, GF_ObjectManager *odm)
{
	char *url;
	if (!scene->is_dynamic_scene || !scene->graph_attached || !odm) return;
	
	if (!odm->codec) return;

	if (odm->state) {
		if (check_odm_deactivate(&scene->audio_url, odm, gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO")) ) return;
		if (check_odm_deactivate(&scene->visual_url, odm, gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO") )) return;
		if (check_odm_deactivate(&scene->text_url, odm, gf_sg_find_node_by_name(scene->graph, "DYN_TEXT") )) return;
	}

	if (odm->codec->type == GF_STREAM_AUDIO) {
		M_AudioClip *ac = (M_AudioClip *) gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO");
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
		M_MovieTexture *mt = (M_MovieTexture*) gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO");
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

void gf_scene_restart_dynamic(GF_Scene *scene, u64 from_time)
{
	u32 i;
	GF_Clock *ck;
	GF_List *to_restart;
	GF_ObjectManager *odm;

	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Scene] Restarting from "LLD"\n", LLD_CAST from_time));

	ck = scene->dyn_ck;
	if (scene->scene_codec) ck = scene->scene_codec->ck;
	if (!ck) return;

	to_restart = gf_list_new();


	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i))) {
		if (gf_odm_shares_clock(odm, ck)) {
			if (odm->state != GF_ODM_STATE_BLOCKED) {
				gf_list_add(to_restart, odm);
				if (odm->state == GF_ODM_STATE_PLAY) {
					gf_odm_stop(odm, 1);
				}
			}
		}
	}

	/*reset clock*/
	gf_clock_reset(ck);
	if (!scene->is_dynamic_scene) gf_clock_set_time(ck, 0);

	/*restart objects*/
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(to_restart, &i))) {
		odm->media_start_time = from_time;
		gf_odm_start(odm, 0);
	}
	gf_list_del(to_restart);

	/*also check nodes since they may be deactivated (end of stream)*/
	if (scene->is_dynamic_scene) {
		M_AudioClip *ac = (M_AudioClip *) gf_sg_find_node_by_name(scene->graph, "DYN_AUDIO");
		M_MovieTexture *mt = (M_MovieTexture *) gf_sg_find_node_by_name(scene->graph, "DYN_VIDEO");
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
	/*for now only allowed when no scene info*/
	if (!scene->is_dynamic_scene) return;
	
	gf_sc_lock(scene->root_od->term->compositor, 1);

	GF_LOG(GF_LOG_INFO, GF_LOG_COMPOSE, ("[Compositor] Changing scene size to %d x %d\n", width, height));
	
	if (scene->root_od->term->root_scene == scene) {
		GF_NetworkCommand com;

		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.base.command_type = GF_NET_SERVICE_HAS_FORCED_VIDEO_SIZE;
		gf_term_service_command(scene->root_od->net_service, &com);

		if (com.par.width && com.par.height) {
			gf_sc_set_scene_size(scene->root_od->term->compositor, width, height, 1);
			if (!scene->force_size_set) {
				gf_sc_set_size(scene->root_od->term->compositor, com.par.width, com.par.height);
				scene->force_size_set = 1;
			} else {
				gf_sc_set_size(scene->root_od->term->compositor, 0, 0);
			}
		} else {
			/*need output resize*/
			gf_sg_set_scene_size_info(scene->graph, width, height, gf_sg_use_pixel_metrics(scene->graph));
			gf_sc_set_scene(scene->root_od->term->compositor, scene->graph);
			gf_sc_set_size(scene->root_od->term->compositor, width, height);
		}
	}
	else if (scene->root_od->parentscene && scene->root_od->parentscene->is_dynamic_scene) {
		gf_sg_set_scene_size_info(scene->root_od->parentscene->graph, width, height, gf_sg_use_pixel_metrics(scene->root_od->parentscene->graph));
		if (scene->root_od->term->root_scene == scene->root_od->parentscene) {
			if (width && height) {
				gf_sc_set_scene_size(scene->root_od->term->compositor, width, height, 1);
				gf_sc_set_size(scene->root_od->term->compositor, width, height);
			}
		}
	}
	gf_sg_set_scene_size_info(scene->graph, width, height, gf_sg_use_pixel_metrics(scene->graph));

#ifndef GPAC_DISABLE_VRML
	IS_UpdateVideoPos(scene);
#endif

	gf_sc_lock(scene->root_od->term->compositor, 0);

	gf_scene_notify_event(scene, GF_EVENT_SCENE_ATTACHED, NULL, NULL, GF_OK);
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
Bool gf_scene_check_clocks(GF_ClientService *ns, GF_Scene *scene)
{
	GF_Clock *ck;
	u32 i;
	if (scene) {
		GF_ObjectManager *odm;
		if (scene->root_od->net_service != ns) {
			if (!gf_scene_check_clocks(scene->root_od->net_service, scene)) return 0;
		}
		i=0;
		while ( (odm = (GF_ObjectManager*)gf_list_enum(scene->resources, &i)) ) {
			if (odm->net_service != ns) {
				if (!gf_scene_check_clocks(odm->net_service, NULL)) return 0;
			} else if (odm->codec && odm->codec->CB && !gf_cm_is_eos(odm->codec->CB) ) {
				return 0;
			}
		}
	}
	i=0;
	while ( (ck = (GF_Clock *)gf_list_enum(ns->Clocks, &i) ) ) {
		if (!ck->has_seen_eos) return 0;
	}
	if (scene->scene_codec && (scene->scene_codec->Status != GF_ESM_CODEC_STOP)) return 0;
	if (scene->od_codec && (scene->od_codec->Status != GF_ESM_CODEC_STOP)) return 0;
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

	gf_sg_set_scene_size_info(scene->graph, 0, 0, 1);
	gf_sc_set_scene(scene->root_od->term->compositor, scene->graph);
	scene->graph_attached = 1;
	scene->is_dynamic_scene = 2;

	evt.type = GF_EVENT_CONNECT;
	evt.connect.is_connected = 1;
	gf_term_send_event(scene->root_od->term, &evt);
}
