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



/*for OD service types*/
#include <gpac/constants.h>
#include <gpac/internal/terminal_dev.h>
#include "media_control.h"
#include <gpac/compositor.h>
#include <gpac/nodes_x3d.h>

/*SVG properties*/
#ifndef GPAC_DISABLE_SVG
#include <gpac/scenegraph_svg.h>
#endif

/*extern proto fetcher*/
typedef struct
{
	MFURL *url;
	GF_MediaObject *mo;
} ProtoLink;

GF_EXPORT
Double gf_inline_get_time(void *_is)
{
	GF_InlineScene *is = (GF_InlineScene *)_is;
#if 0
	u32 ret;
	GF_Clock *ck;
	assert(is);
	ck = is->scene_codec ? is->scene_codec->ck : is->dyn_ck;
	if (!ck) return 0.0;
	ret = gf_clock_time(ck);
	if (is->root_od->media_stop_time && (is->root_od->media_stop_time<ret)) ret = (u32) is->root_od->media_stop_time;
	return ret/1000.0;
#else
	return is->simulation_time;
#endif
}

void gf_inline_sample_time(GF_InlineScene *is)
{
	u32 ret;
	GF_Clock *ck;
	ck = is->scene_codec ? is->scene_codec->ck : is->dyn_ck;
	if (!ck) 
		is->simulation_time = 0;
	else {
		ret = gf_clock_time(ck);
		if (is->root_od->media_stop_time && (is->root_od->media_stop_time<ret)) ret = (u32) is->root_od->media_stop_time;
		is->simulation_time = ((Double) ret) / 1000.0;
	}
}

GF_InlineScene *gf_inline_new(GF_InlineScene *parentScene)
{
	GF_InlineScene *tmp;
	GF_SAFEALLOC(tmp, GF_InlineScene);
	if (! tmp) return NULL;

	tmp->ODlist = gf_list_new();
	tmp->media_objects = gf_list_new();
	tmp->extern_protos = gf_list_new();
	tmp->inline_nodes = gf_list_new();
	tmp->extra_scenes = gf_list_new();
	/*init inline scene*/
	if (parentScene) {
		tmp->graph = gf_sg_new_subscene(parentScene->graph);
	} else {
		tmp->graph = gf_sg_new();
	}

	gf_sg_set_private(tmp->graph, tmp);
	gf_sg_set_node_callback(tmp->graph, gf_term_node_callback);
	gf_sg_set_scene_time_callback(tmp->graph, gf_inline_get_time);

	gf_sg_set_proto_loader(tmp->graph, gf_inline_get_proto_lib);
	return tmp;
}

void gf_inline_del(GF_InlineScene *is)
{
	gf_list_del(is->ODlist);
	gf_list_del(is->inline_nodes);
	assert(!gf_list_count(is->extra_scenes) );
	gf_list_del(is->extra_scenes);

	while (gf_list_count(is->extern_protos)) {
		ProtoLink *pl = (ProtoLink *)gf_list_get(is->extern_protos, 0);
		gf_list_rem(is->extern_protos, 0);
		free(pl);
	}
	gf_list_del(is->extern_protos);

	/*delete scene decoder */
	if (is->scene_codec) {
		GF_SceneDecoder *dec = (GF_SceneDecoder *)is->scene_codec->decio;
		/*make sure the scene codec doesn't have anything left in the scene graph*/
		if (dec->ReleaseScene) dec->ReleaseScene(dec);

		gf_term_remove_codec(is->root_od->term, is->scene_codec);
		gf_codec_del(is->scene_codec);
		/*reset pointer to NULL in case nodes try to access scene time*/
		is->scene_codec = NULL;
	}

	/*delete the scene graph*/
	gf_sg_del(is->graph);

	if (is->od_codec) {
		gf_term_remove_codec(is->root_od->term, is->od_codec);
		gf_codec_del(is->od_codec);
		is->od_codec = NULL;
	}
	/*don't touch the root_od, will be deleted by the parent scene*/

	/*clean all remaining associations*/
	while (gf_list_count(is->media_objects)) {
		GF_MediaObject *obj = (GF_MediaObject *)gf_list_get(is->media_objects, 0);
		if (obj->odm) obj->odm->mo = NULL;
		gf_list_rem(is->media_objects, 0);
		gf_sg_vrml_mf_reset(&obj->URLs, GF_SG_VRML_MFURL);
		gf_list_del(obj->nodes);
		free(obj);
	}
	gf_list_del(is->media_objects);

	if (is->audio_url.url) free(is->audio_url.url);
	if (is->visual_url.url) free(is->visual_url.url);
	if (is->text_url.url) free(is->text_url.url);
	if (is->fragment_uri) free(is->fragment_uri);
	if (is->redirect_xml_base) free(is->redirect_xml_base);
	free(is);
}

GF_EXPORT
GF_ObjectManager *gf_inline_find_odm(GF_InlineScene *is, u16 OD_ID)
{
	GF_ObjectManager *odm;
	u32 i=0;
	while ((odm = (GF_ObjectManager *)gf_list_enum(is->ODlist, &i))) {
		if (odm->OD && odm->OD->objectDescriptorID == OD_ID) return odm;
	}
	return NULL;
}

GF_EXPORT
void gf_inline_disconnect(GF_InlineScene *is, Bool for_shutdown)
{
	u32 i;
	GF_MediaObject *obj;
	GF_Node *root_node;
	GF_ObjectManager *odm;
	GF_SceneDecoder *dec = NULL;
	if (is->scene_codec) dec = (GF_SceneDecoder *)is->scene_codec->decio;

	gf_term_lock_compositor(is->root_od->term, 1);
	
	/*disconnect / kill all objects BEFORE reseting the scene graph since we have 
	potentially registered Inline nodes of the graph with the sub-scene*/
	if (!for_shutdown && is->static_media_ressources) {
		i=0;
		/*stop all objects but DON'T DESTROY THEM*/
		while ((odm = (GF_ObjectManager *)gf_list_enum(is->ODlist, &i))) {
			if (odm->state) gf_odm_disconnect(odm, 0);
		}
		/*reset all stream associations*/
		i=0;
		while ((obj = (GF_MediaObject*)gf_list_enum(is->media_objects, &i))) {
			gf_sg_vrml_mf_reset(&obj->URLs, GF_SG_VRML_MFURL);
			gf_list_reset(obj->nodes);
		}
	} else {
		while (gf_list_count(is->ODlist)) {
			odm = (GF_ObjectManager *)gf_list_get(is->ODlist, 0);
			gf_odm_disconnect(odm, (for_shutdown || !is->static_media_ressources) ? 1 : 0);
		}
	}
	
	root_node = gf_sg_get_root_node(is->graph);
	/*reset private stack of all inline nodes still registered*/
	while (gf_list_count(is->inline_nodes)) {
		GF_Node *n = (GF_Node *)gf_list_get(is->inline_nodes, 0);
		gf_list_rem(is->inline_nodes, 0);
		switch (gf_node_get_tag(n)) {
		case TAG_MPEG4_Inline:
		case TAG_X3D_Inline:
			gf_node_set_private(n, NULL);
			break;
		}
	}

	/*remove all associated eventTargets - THIS ENEDS CLEANUP*/
	i=0;
	while ((obj = (GF_MediaObject *)gf_list_enum(is->ODlist, &i))) {
		if (obj->nodes) gf_list_reset(obj->nodes);
	}

	if (is->graph_attached && (is->root_od->term->root_scene == is)) {
		gf_sc_set_scene(is->root_od->term->compositor, NULL);
	}
	/*release the scene*/
	if (dec && dec->ReleaseScene) dec->ReleaseScene(dec);
	gf_sg_reset(is->graph);
	is->graph_attached = 0;
	
	gf_term_lock_compositor(is->root_od->term, 0);


	assert(!gf_list_count(is->extra_scenes) );
	/*reset statc ressource flag since we destroyed scene objects*/
	is->static_media_ressources = 0;

	/*remove stream associations*/
	while (gf_list_count(is->media_objects)) {
		obj = (GF_MediaObject*)gf_list_get(is->media_objects, 0);
		gf_list_rem(is->media_objects, 0);
		if (obj->odm) obj->odm->mo = NULL;
		gf_sg_vrml_mf_reset(&obj->URLs, GF_SG_VRML_MFURL);
		gf_list_del(obj->nodes);
		free(obj);
	}
}

static void IS_InsertObject(GF_InlineScene *is, GF_MediaObject *mo, Bool lock_timelines, GF_MediaObject *sync_ref, Bool keep_fragment)
{
	GF_ObjectManager *root_od;
	GF_ObjectManager *odm;
	char *url;
	if (!mo || !is) return;

	odm = gf_odm_new();
	/*remember OD*/
	odm->mo = mo;
	mo->odm = odm;
	odm->parentscene = is;
	odm->OD = (GF_ObjectDescriptor *) gf_odf_desc_new(GF_ODF_OD_TAG);
	odm->OD->objectDescriptorID = GF_ESM_DYNAMIC_OD_ID;
	odm->parentscene = is;
	odm->term = is->root_od->term;
	root_od = is->root_od;

	url = mo->URLs.vals[0].url;
	if (!stricmp(url, "KeySensor")) {
		GF_ESD *esd = gf_odf_desc_esd_new(0);
		esd->decoderConfig->streamType = GF_STREAM_INTERACT;
		esd->decoderConfig->objectTypeIndication = 1;
		free(esd->decoderConfig->decoderSpecificInfo->data);
		esd->decoderConfig->decoderSpecificInfo->data = strdup(" KeySensor");
		esd->decoderConfig->decoderSpecificInfo->data[0] = 9;
		esd->decoderConfig->decoderSpecificInfo->dataLength = 10;
		esd->ESID = esd->OCRESID = 65534;
		gf_list_add(odm->OD->ESDescriptors, esd);
	} else if (!stricmp(url, "StringSensor")) {
		GF_ESD *esd = gf_odf_desc_esd_new(0);
		esd->decoderConfig->streamType = GF_STREAM_INTERACT;
		esd->decoderConfig->objectTypeIndication = 1;
		free(esd->decoderConfig->decoderSpecificInfo->data);
		esd->decoderConfig->decoderSpecificInfo->data = strdup(" StringSensor");
		esd->decoderConfig->decoderSpecificInfo->data[0] = 12;
		esd->decoderConfig->decoderSpecificInfo->dataLength = 13;
		esd->ESID = esd->OCRESID = 65534;
		gf_list_add(odm->OD->ESDescriptors, esd);
	} else if (!stricmp(url, "Mouse")) {
		GF_ESD *esd = gf_odf_desc_esd_new(0);
		esd->decoderConfig->streamType = GF_STREAM_INTERACT;
		esd->decoderConfig->objectTypeIndication = 1;
		free(esd->decoderConfig->decoderSpecificInfo->data);
		esd->decoderConfig->decoderSpecificInfo->data = strdup(" Mouse");
		esd->decoderConfig->decoderSpecificInfo->data[0] = 5;
		esd->decoderConfig->decoderSpecificInfo->dataLength = 6;
		esd->ESID = esd->OCRESID = 65534;
		gf_list_add(odm->OD->ESDescriptors, esd);
	} else {
		if (!keep_fragment) {
			char *frag = strrchr(mo->URLs.vals[0].url, '#');
			if (frag) frag[0] = 0;
			odm->OD->URLString = strdup(mo->URLs.vals[0].url);
			if (frag) frag[0] = '#';
		} else {
			odm->OD->URLString = strdup(mo->URLs.vals[0].url);
		}
		if (lock_timelines) odm->flags |= GF_ODM_INHERIT_TIMELINE;
	}

	/*HACK - temp storage of sync ref*/
	if (sync_ref) odm->ocr_codec = (struct _generic_codec *)sync_ref;

	gf_list_add(is->ODlist, odm);
	gf_odm_setup_object(odm, root_od->net_service);
}

static void IS_ReinsertObject(GF_InlineScene *is, GF_MediaObject *mo)
{
	u32 i;
	free(mo->URLs.vals[0].url);
	mo->URLs.vals[0].url = NULL;
	for (i=0; i<mo->URLs.count-1; i++) mo->URLs.vals[i].url = mo->URLs.vals[i+1].url;
	mo->URLs.vals[mo->URLs.count-1].url = NULL;
	mo->URLs.count-=1;
	/*FIXME - we should re-ananlyse whether the fragment is important or not ...*/
	IS_InsertObject(is, mo, 0, NULL, 0);
}


void gf_inline_remove_object(GF_InlineScene *is, GF_ObjectManager *odm, Bool for_shutdown)
{
	u32 i;
	GF_MediaObject *obj;

	gf_list_del_item(is->ODlist, odm);


	i=0;
	while ((obj = (GF_MediaObject*)gf_list_enum(is->media_objects, &i))) {
		if (
			/*assigned object*/
			(obj->odm==odm) || 
			/*remote OD*/
			((obj->OD_ID!=GF_ESM_DYNAMIC_OD_ID) && odm->OD && (obj->OD_ID == odm->OD->objectDescriptorID) ) ||
			/*dynamic OD*/
			(obj->URLs.count && odm->OD && odm->OD->URLString && !stricmp(obj->URLs.vals[0].url, odm->OD->URLString)) 
		) {
			gf_odm_lock(odm, 1);
			obj->flags = 0;
			if (obj->odm) obj->odm->mo = NULL;
			odm->mo = NULL;
			obj->odm = NULL;

			obj->frame = NULL;
			obj->framesize = obj->timestamp = 0;

			gf_odm_lock(odm, 0);

			/*if graph not attached we can remove the link (this is likely scene shutdown for some error)*/
			if (!is->graph_attached) {
				ProtoLink *pl;
				u32 j=0;
				while ((pl = (ProtoLink *)gf_list_enum(is->extern_protos, &j))) {
					if (pl->mo==obj) {
						pl->mo = NULL;
						break;
					}
				}
				gf_list_rem(is->media_objects, i-1);
				gf_sg_vrml_mf_reset(&obj->URLs, GF_SG_VRML_MFURL);
				gf_list_del(obj->nodes);
				free(obj);
			} else if (!for_shutdown) {
				/*if dynamic OD and more than 1 URLs resetup*/
				if ((obj->OD_ID==GF_ESM_DYNAMIC_OD_ID) && (obj->URLs.count>1)) IS_ReinsertObject(is, obj);
			}
			return;
		}
	}
}

u32 URL_GetODID(MFURL *url)
{
	u32 i, j, tmpid;
	char *str, *s_url;
	u32 id = 0;

	if (!url) return 0;
	
	for (i=0; i<url->count; i++) {
		if (url->vals[i].OD_ID) {
			/*works because OD ID 0 is forbidden in MPEG4*/
			if (!id) {
				id = url->vals[i].OD_ID;
			}
			/*bad url, only one object can be described in MPEG4 urls*/
			else if (id != url->vals[i].OD_ID) return 0;
		} else if (url->vals[i].url && strlen(url->vals[i].url)) {
			/*format: od:ID or od:ID#segment - also check for "ID" in case...*/
			str = url->vals[i].url;
			if (!strnicmp(str, "od:", 3)) str += 3;
			/*remove segment info*/
			s_url = strdup(str);
			j = 0;
			while (j<strlen(s_url)) {
				if (s_url[j]=='#') {
					s_url[j] = 0;
					break;
				}
				j++;
			}
			j = sscanf(s_url, "%d", &tmpid);
			/*be carefull, an url like "11-regression-test.mp4" will return 1 on sscanf :)*/
			if (j==1) {
				char szURL[20];
				sprintf(szURL, "%d", tmpid);
				if (stricmp(szURL, s_url)) j = 0;
			}
			free(s_url);

			if (j!= 1) {
				/*dynamic OD if only one URL specified*/
				if (!i) return GF_ESM_DYNAMIC_OD_ID;
				/*otherwise ignore*/
				continue;
			}
			if (!id) {
				id = tmpid;
				continue;
			}
			/*bad url, only one object can be described in MPEG4 urls*/
			else if (id != tmpid) return 0;
		}
	}
	return id;
}


//browse all channels and update buffering info
void gf_inline_buffering_info(GF_InlineScene *is)
{
	u32 i, j, max_buffer, cur_buffer;
	GF_Channel *ch;
	GF_Event evt;
	GF_ObjectManager *odm;
	if (!is) return;

	max_buffer = cur_buffer = 0;

	/*get buffering on root OD*/
	j=0;
	while ((ch = (GF_Channel*)gf_list_enum(is->root_od->channels, &j))) {
		/*count only re-buffering channels*/
		if (!ch->BufferOn) continue;

		max_buffer += ch->MaxBuffer;
		cur_buffer += (ch->BufferTime>0) ? ch->BufferTime : 1;
	}

	/*get buffering on all ODs*/
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i))) {
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
	evt.progress.service = is->root_od->net_service->url;
	if (!max_buffer || !cur_buffer || (max_buffer <= cur_buffer)) {
		evt.progress.done = evt.progress.total = max_buffer;
	} else {
		evt.progress.done = cur_buffer;
		evt.progress.total = max_buffer;
	}
	GF_USER_SENDEVENT(is->root_od->term->user, &evt);
}


static Bool Inline_SetScene(M_Inline *root)
{
	GF_MediaObject *mo;
	GF_InlineScene *parent;
	GF_SceneGraph *graph = gf_node_get_graph((GF_Node *) root);
	parent = (GF_InlineScene *)gf_sg_get_private(graph);
	if (!parent) return 0;

	mo = gf_inline_get_media_object_ex(parent, &root->url, GF_MEDIA_OBJECT_SCENE, 0, NULL, 0, (GF_Node*)root);
	if (!mo || !mo->odm) return 0;

	if (!mo->odm->subscene) {
		gf_term_invalidate_compositor(parent->root_od->term);
		return 0;
	}
	/*assign inline scene as private stack of inline node, and remember inline node for event propagation*/
	gf_node_set_private((GF_Node *)root, mo->odm->subscene);
	gf_list_add(mo->odm->subscene->inline_nodes, root);
	/*play*/
	gf_mo_play(mo, 0, -1, 0);
	return 1;
}

static Bool gf_mo_is_same_url_ex(GF_MediaObject *obj, MFURL *an_url, Bool *keep_fragment, u32 obj_hint_type)
{
	Bool include_sub_url = 0;
	u32 i;
	char szURL1[GF_MAX_PATH], szURL2[GF_MAX_PATH], *ext;

	if (keep_fragment) *keep_fragment = 0;
	if (obj->OD_ID==GF_ESM_DYNAMIC_OD_ID) {
		if (!obj->URLs.count) {
			if (!obj->odm) return 0;
			strcpy(szURL1, obj->odm->net_service->url);
		} else {
			strcpy(szURL1, obj->URLs.vals[0].url);
		}
	} else {
		if (!obj->URLs.count) return 0;
		strcpy(szURL1, obj->URLs.vals[0].url);
	}

	/*don't analyse audio/video to locate segments or viewports*/
	if (obj->type==GF_MEDIA_OBJECT_AUDIO) 
		include_sub_url = 1;
	else if (obj->type==GF_MEDIA_OBJECT_VIDEO) 
		include_sub_url = 1;
	else if ((obj->type==GF_MEDIA_OBJECT_SCENE) && keep_fragment && obj->odm) {
		GF_ClientService *ns;
		u32 j;
		/*for remoteODs/dynamic ODs, check if one of the running service cannot be used*/
		for (i=0; i<an_url->count; i++) {
			char *frag = strrchr(an_url->vals[i].url, '#');
			j=0;
			/*this is the same object (may need some refinement)*/
			if (!stricmp(szURL1, an_url->vals[i].url)) return 1;

			/*fragment is a media segment, same URL*/
			if (frag ) {
				Bool same_res = 0;
				frag[0] = 0;
				same_res = !strncmp(an_url->vals[i].url, szURL1, strlen(an_url->vals[i].url)) ? 1 : 0;
				frag[0] = '#';

				/*if we're talking about the same resource, check if the fragment can be matched*/
				if (same_res) {
					/*if the expected type is a segment (undefined media type) 
					and the fragment is a media segment, same URL
					*/
					if (obj->odm->subscene && (gf_sg_find_node_by_name(obj->odm->subscene->graph, frag+1)!=NULL) )
						return 1;
				
					/*if the expected type is a segment (undefined media type) 
					and the fragment is a media segment, same URL
					*/
					if (!obj_hint_type && gf_odm_find_segment(obj->odm, frag+1))
						return 1;
				}
			}

			while ( (ns = (GF_ClientService*)gf_list_enum(obj->odm->term->net_services, &j)) ) {
				/*sub-service of an existing service - don't touch any fragment*/
				if (gf_term_service_can_handle_url(ns, an_url->vals[i].url)) {
					*keep_fragment = 1;
					return 0;
				}
			}
		}
	}

	/*check on full URL without removing fragment IDs*/
	if (include_sub_url) {
		for (i=0; i<an_url->count; i++) {
			if (!stricmp(szURL1, an_url->vals[i].url)) return 1;
		}
		return 0;
	}
	ext = strrchr(szURL1, '#');
	if (ext) ext[0] = 0;
	for (i=0; i<an_url->count; i++) {
		if (!an_url->vals[i].url) return 0;
		strcpy(szURL2, an_url->vals[i].url);
		ext = strrchr(szURL2, '#');
		if (ext) ext[0] = 0;
		if (!stricmp(szURL1, szURL2)) return 1;
	}
	return 0;
}

Bool gf_mo_is_same_url(GF_MediaObject *obj, MFURL *an_url)
{
	return gf_mo_is_same_url_ex(obj, an_url, NULL, 0);
}

void gf_inline_on_modified(GF_Node *node)
{
	u32 ODID;
	GF_MediaObject *mo;
	M_Inline *pInline = (M_Inline *) node;
	GF_InlineScene *pIS = (GF_InlineScene *)gf_node_get_private(node);

	ODID = URL_GetODID(&pInline->url);
	if (pIS) {
		mo = (pIS->root_od) ? pIS->root_od->mo : NULL;

		/*disconnect current inline if we're the last one using it (same as regular OD session leave/join)*/
		if (mo) {
			Bool changed = 1;
			if (ODID != GF_ESM_DYNAMIC_OD_ID) {
				if (ODID && (ODID==pIS->root_od->OD->objectDescriptorID)) changed = 0;
			} else {
				if (gf_mo_is_same_url(mo, &pInline->url) ) changed = 0;
			}
			if (mo->num_open) {
				if (!changed) return;
				mo->num_open --;
				if (!mo->num_open) {
					gf_odm_stop(pIS->root_od, 1);
					gf_inline_disconnect(pIS, 1);
					assert(gf_list_count(pIS->ODlist) == 0);
				}
			}
		}
	}	
	if (ODID) Inline_SetScene(pInline);
}


static void IS_CheckMediaRestart(GF_InlineScene *is)
{
	/*no ctrl if no duration*/
	if (!is->duration) return;
	if (!is->needs_restart) gf_odm_check_segment_switch(is->root_od);
	if (is->needs_restart) return;

	if (is->root_od->media_ctrl && is->root_od->media_ctrl->control->loop) {
		GF_Clock *ck = gf_odm_get_media_clock(is->root_od);
		if (ck->has_seen_eos) {
			u32 now = gf_clock_time(ck);
			u64 dur = is->duration;
			if (is->root_od->media_ctrl->current_seg) {
				/*only process when all segments are played*/
				if (gf_list_count(is->root_od->media_ctrl->seg) <= is->root_od->media_ctrl->current_seg) {
					is->needs_restart = 1;
					is->root_od->media_ctrl->current_seg = 0;
				}
			}
			else {
				Double s, e;
				s = now; s/=1000;
				e = -1;
				MC_GetRange(is->root_od->media_ctrl, &s, &e);
				if ((e>=0) && (e<GF_MAX_FLOAT)) dur = (u32) (e*1000);
				if (dur<now) {
					is->needs_restart = 1;
					is->root_od->media_ctrl->current_seg = 0;
				}
			}
		} else {
			/*trigger render until to watch for restart...*/
			gf_term_invalidate_compositor(is->root_od->term);
		}
	}
}

static void gf_inline_traverse(GF_Node *n, void *rs, Bool is_destroy)
{
	GF_InlineScene *is = (GF_InlineScene *)gf_node_get_private(n);

	if (is_destroy) {
		GF_MediaObject *mo = (is && is->root_od) ? is->root_od->mo : NULL;

		if (is) gf_list_del_item(is->inline_nodes, n);

		if (!mo) return;
		/*disconnect current inline if we're the last one using it (same as regular OD session leave/join)*/
		if (mo->num_open) {
			mo->num_open --;
			if (!mo->num_open) {
				/*this is unspecified in the spec: whenever an inline not using the 
				OD framework is destroyed, destroy the associated resource*/
				if (mo->OD_ID == GF_ESM_DYNAMIC_OD_ID) {
					gf_odm_disconnect(is->root_od, 1);

					/*get parent scene and remove MediaObject in case the ressource
					gets re-requested later on*/
					is = (GF_InlineScene *)gf_sg_get_private(gf_node_get_graph((GF_Node *) n) );
					gf_list_del_item(is->media_objects, mo);
					gf_sg_vrml_mf_reset(&mo->URLs, GF_SG_VRML_MFURL);
					gf_list_del(mo->nodes);
					free(mo);
				} else {
					gf_odm_stop(is->root_od, 1);
					gf_inline_disconnect(is, 1);
					assert(gf_list_count(is->ODlist) == 0);
				}
			}
		}
		return;
	}


	//if no private scene is associated	get the node parent graph, retrieve the IS and find the OD
	if (!is) {
		M_Inline *inl = (M_Inline *)n;
		Inline_SetScene(inl);
		is = (GF_InlineScene *)gf_node_get_private(n);
		if (!is) {
			/*just like protos, we must invalidate parent graph until attached*/
			if (inl->url.count)
				gf_node_dirty_set(n, 0, 1);
			return;
		}
	}

	IS_CheckMediaRestart(is);

	/*if we need to restart, shutdown graph and do it*/
	if (is->needs_restart) {
		u32 current_seg = 0;
		/*special case: scene change*/
		if (is->needs_restart==2) {
			is->needs_restart = 0;
			gf_inline_on_modified(n);
			return;
		}

		if (is->root_od->media_ctrl) current_seg = is->root_od->media_ctrl->current_seg;
		is->needs_restart = 0;

		if (is->is_dynamic_scene) {
			if (is->root_od->media_ctrl) is->root_od->media_ctrl->current_seg = current_seg;
			gf_inline_restart_dynamic(is, 0);
		} else {
			/*we cannot use gf_mo_restart since it only sets the needs_restart for inline scenes. 
			The rational is that gf_mo_restart can be called from the parent scene (OK) or from the scene itself, in 
			which case shutting down the graph would crash the compositor. We therefore need two render passes to 
			safely restart an inline scene*/

			/*1- stop main object from playing but don't disconnect channels*/
			gf_odm_stop(is->root_od, 1);
			/*2- close all ODs inside the scene and reset the graph*/
			gf_inline_disconnect(is, 0);
			if (is->root_od->media_ctrl) is->root_od->media_ctrl->current_seg = current_seg;
			/*3- restart the scene*/
			gf_odm_start(is->root_od);
		}
		gf_node_dirty_set(n, 0, 1);
		return;
	} 
	
	/*if not attached return (attaching the graph cannot be done in render since render is not called while unattached :) */
	if (!is->graph_attached) {
		/*just like protos, we must invalidate parent graph until attached*/
		gf_node_dirty_set(n, 0, 1);
		return;
	}
	/*clear dirty flags for any sub-inlines, bitmaps or protos*/
	gf_node_dirty_clear(n, 0);
	
	gf_sc_traverse_subscene(is->root_od->term->compositor, n, is->graph, rs);
}


static void gf_is_resize_event(GF_InlineScene *is)
{
	/*fire resize event*/
#ifndef GPAC_DISABLE_SVG
	u32 i, count;
	u32 w, h;
	GF_DOM_Event evt;
	memset(&evt, 0, sizeof(GF_DOM_Event));
	w = h = 0;
	gf_sg_get_scene_size_info(is->graph, &w, &h);
	evt.type = GF_EVENT_RESIZE;
	evt.screen_rect.width = INT2FIX(w);
	evt.screen_rect.height = INT2FIX(h);
	gf_dom_event_fire(gf_sg_get_root_node(is->graph), &evt);
	
	count=gf_list_count(is->inline_nodes);
	for (i=0;i<count; i++) {
		gf_dom_event_fire( gf_list_get(is->inline_nodes, i), &evt );
	}
#endif
}

GF_EXPORT
void gf_inline_attach_to_compositor(GF_InlineScene *is)
{
	char *url;
	if ((is->graph_attached==1) || (gf_sg_get_root_node(is->graph)==NULL) ) {
		gf_term_invalidate_compositor(is->root_od->term);
		return;
	}
	is->graph_attached = 1;

	/*locate fragment IRI*/
	if (!is->root_od || !is->root_od->net_service || !is->root_od->net_service->url) return;
	if (is->fragment_uri) {
		free(is->fragment_uri);
		is->fragment_uri = NULL;
	}
	url = strchr(is->root_od->net_service->url, '#');
	if (url) is->fragment_uri = strdup(url+1);

	/*main display scene, setup compositor*/
	if (is->root_od->term->root_scene == is) {
		gf_sc_set_scene(is->root_od->term->compositor, is->graph);
	}
	else {
		u32 i, count=gf_list_count(is->inline_nodes);
		for (i=0;i<count; i++) 
			gf_node_dirty_parents( gf_list_get(is->inline_nodes, i) );
		gf_term_invalidate_compositor(is->root_od->term);

		if (is->root_od->parentscene->is_dynamic_scene) {
			u32 w, h;
			gf_sg_get_scene_size_info(is->graph, &w, &h);
			gf_sc_set_size(is->root_od->term->compositor, w, h);
		}
		gf_is_resize_event(is);
	}
}

static GF_MediaObject *IS_CheckExistingObject(GF_InlineScene *is, MFURL *urls, u32 type)
{
	GF_MediaObject *obj;
	u32 i = 0;
	while ((obj = (GF_MediaObject *)gf_list_enum(is->media_objects, &i))) {
		if (type && (type != obj->type)) continue;
		if ((obj->OD_ID == GF_ESM_DYNAMIC_OD_ID) && gf_mo_is_same_url(obj, urls)) return obj;
		else if ((obj->OD_ID != GF_ESM_DYNAMIC_OD_ID) && (obj->OD_ID == urls->vals[0].OD_ID)) return obj;
	}
	return NULL;
}

static GFINLINE Bool is_match_obj_type(u32 type, u32 hint_type)
{
	if (!hint_type) return 1;
	if (type==hint_type) return 1;
	/*TEXT are used by animation stream*/
	if ((type==GF_MEDIA_OBJECT_TEXT) && (hint_type==GF_MEDIA_OBJECT_UPDATES)) return 1;
	return 0;
}

GF_MediaObject *gf_inline_get_media_object_ex(GF_InlineScene *is, MFURL *url, u32 obj_type_hint, Bool lock_timelines, GF_MediaObject *sync_ref, Bool always_load_new, GF_Node *node)
{
	GF_MediaObject *obj;
	Bool keep_fragment = 1;
	u32 i, OD_ID;

	OD_ID = URL_GetODID(url);
	if (!OD_ID) return NULL;

	if (!always_load_new) {
		obj = NULL;
		i=0;
		while ((obj = (GF_MediaObject *)gf_list_enum(is->media_objects, &i))) {
			if (
				/*regular OD scheme*/
				(OD_ID != GF_ESM_DYNAMIC_OD_ID && (obj->OD_ID==OD_ID)) 
			||
				/*dynamic OD scheme*/
				((OD_ID == GF_ESM_DYNAMIC_OD_ID) && (obj->OD_ID==GF_ESM_DYNAMIC_OD_ID)
					/*if object type unknown (media control, media sensor), return first obj matching URL
					otherwise check types*/
					&& is_match_obj_type(obj->type, obj_type_hint)
					/*locate sub-url in given one and handle fragments (viewpoint/segments/...)*/
					&& gf_mo_is_same_url_ex(obj, url, &keep_fragment, obj_type_hint) 
				)
			) {
				
				if (node && (gf_list_find(obj->nodes, node)<0))
					gf_list_add(obj->nodes, node);
				return obj;
			}
		}
	}
	/*we cannot create an OD manager at this point*/
	if (obj_type_hint==GF_MEDIA_OBJECT_UNDEF) return NULL;

	/*create a new object identification*/
	obj = gf_mo_new();
	obj->OD_ID = OD_ID;
	obj->type = obj_type_hint;

	/*register node with object*/
	if (node)
		gf_list_add(obj->nodes, node);
	
	/*if animation stream object, remember originating node
		!! FIXME - this should be cleaned up !! 
	*/
	if (obj->type == GF_MEDIA_OBJECT_UPDATES)
		obj->node_ptr = node;

	gf_list_add(is->media_objects, obj);
	if (OD_ID == GF_ESM_DYNAMIC_OD_ID) {
		gf_sg_vrml_field_copy(&obj->URLs, url, GF_SG_VRML_MFURL);
		IS_InsertObject(is, obj, lock_timelines, sync_ref, keep_fragment);
		/*safety check!!!*/
		if (gf_list_find(is->media_objects, obj)<0) 
			return NULL;
	}
	return obj;
}

GF_MediaObject *gf_inline_get_media_object(GF_InlineScene *is, MFURL *url, u32 obj_type_hint, Bool lock_timelines)
{
	return gf_inline_get_media_object_ex(is, url, obj_type_hint, lock_timelines, NULL, 0, NULL);
}

GF_EXPORT
void gf_inline_setup_object(GF_InlineScene *is, GF_ObjectManager *odm)
{
	GF_MediaObject *obj;
	u32 i;

	/*an object may already be assigned (when using ESD URLs, setup is performed twice)*/
	if (odm->mo != NULL) goto existing;

	i=0;
	while ((obj = (GF_MediaObject*)gf_list_enum(is->media_objects, &i))) {
		if (obj->OD_ID==GF_ESM_DYNAMIC_OD_ID) {
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
	gf_list_add(is->media_objects, odm->mo);
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
		gf_odm_start(odm);
		if (odm->mo->speed != FIX_ONE) gf_odm_set_speed(odm, odm->mo->speed);
	}
	if ((odm->mo->type==GF_MEDIA_OBJECT_VIDEO) && is->is_dynamic_scene) {
		gf_inline_force_scene_size_video(is, odm->mo);
	}
	/*invalidate scene for all nodes using the OD*/
	gf_term_invalidate_compositor(odm->term);
}

void gf_inline_restart(GF_InlineScene *is)
{
	is->needs_restart = 1;
	gf_term_invalidate_compositor(is->root_od->term);
}


GF_EXPORT
void gf_inline_set_duration(GF_InlineScene *is)
{
	Double dur;
	u32 i;
	u64 max_dur;
	GF_ObjectManager *odm;
	MediaSensorStack *media_sens;
	GF_Clock *ck;

	/*this is not normative but works in so many cases... set the duration to the max duration
	of all streams sharing the clock*/
	ck = gf_odm_get_media_clock(is->root_od);
	max_dur = is->root_od->duration;
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i))) {
		if (!odm->codec) continue;
		if (!ck || gf_odm_shares_clock(odm, ck)) {
			if (odm->duration>max_dur) max_dur = odm->duration;
		}
	}
	if (is->duration == max_dur) return;

	is->duration = max_dur;
	dur = (Double) (s64) is->duration;
	dur /= 1000;
	
	i=0;
	while ((media_sens = (MediaSensorStack*)gf_list_enum(is->root_od->ms_stack, &i))) {
		if (media_sens->sensor->isActive) {
			media_sens->sensor->mediaDuration = dur;
			gf_node_event_out_str((GF_Node *) media_sens->sensor, "mediaDuration");
		}
	}

	if ((is == is->root_od->term->root_scene) && is->root_od->term->user->EventProc) {
		GF_Event evt;
		evt.type = GF_EVENT_DURATION;
		evt.duration.duration = dur;
		evt.duration.can_seek = !(is->root_od->flags & GF_ODM_NO_TIME_CTRL);
		if (dur<2.0) evt.duration.can_seek = 0;
		GF_USER_SENDEVENT(is->root_od->term->user,&evt);
	}

}


static Bool IS_IsHardcodedProto(MFURL *url, GF_Config *cfg)
{
	u32 i;
	const char *sOpt = gf_cfg_get_key(cfg, "Systems", "hardcoded_protos");
	for (i=0; i<url->count; i++) {
		if (!url->vals[i].url) continue;
		if (strstr(url->vals[i].url, "urn:inet:gpac:builtin")) return 1;
		if (sOpt && strstr(sOpt, url->vals[i].url)) return 1;
	}
	return 0;
}

void IS_LoadExternProto(GF_InlineScene *is, MFURL *url)
{
	u32 i;
	ProtoLink *pl;
	if (!url || !url->count) return;

	/*internal, don't waste ressources*/
	if (IS_IsHardcodedProto(url, is->root_od->term->user->config)) return;
	
	i=0;
	while ((pl = (ProtoLink*)gf_list_enum(is->extern_protos, &i)) ) {
		if (pl->url == url) return;
		if (pl->url->vals[0].OD_ID && (pl->url->vals[0].OD_ID == url->vals[0].OD_ID)) return;
		if (pl->url->vals[0].url && url->vals[0].url && !stricmp(pl->url->vals[0].url, url->vals[0].url) ) return;
	}
	pl = (ProtoLink*)malloc(sizeof(ProtoLink));
	pl->url = url;
	gf_list_add(is->extern_protos, pl);
	pl->mo = gf_inline_get_media_object(is, url, GF_MEDIA_OBJECT_SCENE, 0);
	/*this may already be destroyed*/
	if (pl->mo) gf_mo_play(pl->mo, 0, -1, 0);
}

GF_EXPORT
GF_SceneGraph *gf_inline_get_proto_lib(void *_is, MFURL *lib_url)
{
	ProtoLink *pl;
	u32 i;
	GF_InlineScene *is = (GF_InlineScene *) _is;
	if (!is || !lib_url->count) return NULL;

	if (IS_IsHardcodedProto(lib_url, is->root_od->term->user->config)) return GF_SG_INTERNAL_PROTO;

	i=0;
	while ((pl = (ProtoLink*)gf_list_enum(is->extern_protos, &i))) {
		if (!pl->mo) continue;
		if (URL_GetODID(pl->url) != GF_ESM_DYNAMIC_OD_ID) {
			if (URL_GetODID(pl->url) == URL_GetODID(lib_url)) {
				if (!pl->mo->odm || !pl->mo->odm->subscene) return NULL;
				return pl->mo->odm->subscene->graph;
			}
		} else if (lib_url->vals[0].url) {
			if (gf_mo_is_same_url(pl->mo, lib_url)) {
				if (!pl->mo->odm || !pl->mo->odm->subscene) return NULL;
				return pl->mo->odm->subscene->graph;
			}
		}
	}

	/*not found, create loader*/
	IS_LoadExternProto(is, lib_url);

	/*and return NULL*/
	return NULL;
}

GF_ObjectManager *IS_GetProtoSceneByGraph(void *_is, GF_SceneGraph *sg)
{
	u32 i;
	ProtoLink *pl;
	GF_InlineScene *is = (GF_InlineScene *) _is;
	if (!is) return NULL;
	i=0;
	while ((pl = (ProtoLink*)gf_list_enum(is->extern_protos, &i))) {
		if (pl->mo->odm && pl->mo->odm->subscene && (pl->mo->odm->subscene->graph==sg)) return pl->mo->odm;
	}
	return NULL;
}


Bool IS_IsProtoLibObject(GF_InlineScene *is, GF_ObjectManager *odm)
{
	u32 i;
	ProtoLink *pl;
	i=0;
	while ((pl = (ProtoLink*)gf_list_enum(is->extern_protos, &i))) {
		if (pl->mo->odm == odm) return 1;
	}
	return 0;
}


GF_MediaObject *gf_inline_find_object(GF_InlineScene *is, u16 ODID, char *url)
{
	u32 i;
	GF_MediaObject *mo;
	if (!url && !ODID) return NULL;
	i=0;
	while ((mo = (GF_MediaObject *)gf_list_enum(is->media_objects, &i))) {
		if (ODID==GF_ESM_DYNAMIC_OD_ID) {
			if (mo->URLs.count && !stricmp(mo->URLs.vals[0].url, url)) return mo;
		} else if (mo->OD_ID==ODID) return mo;
	}
	return NULL;
}


const char *IS_GetSceneViewName(GF_InlineScene *is) 
{
	char *seg_name;
	/*check any viewpoint*/
	seg_name = strrchr(is->root_od->net_service->url, '#');
	if (!seg_name) return NULL;
	seg_name += 1;
	/*look for a media segment with this name - if none found, this is a viewpoint name*/
	if (gf_odm_find_segment(is->root_od, seg_name) != NULL) return NULL;
	return seg_name;
}

GF_EXPORT
Bool gf_inline_default_scene_viewpoint(GF_Node *node)
{
	const char *nname, *sname;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	GF_InlineScene *is = sg ? (GF_InlineScene *) gf_sg_get_private(sg) : NULL;
	if (!is) return 0;

	nname = gf_node_get_name(node);
	if (!nname) return 0;
	sname = IS_GetSceneViewName(is);
	if (!sname) return 0;
	return (!strcmp(nname, sname));
}

GF_EXPORT
void gf_inline_register_extra_graph(GF_InlineScene *is, GF_SceneGraph *extra_scene, Bool do_remove)
{
	if (do_remove) {
		if (gf_list_find(is->extra_scenes, extra_scene)<0) return;
		gf_list_del_item(is->extra_scenes, extra_scene);
		/*for root scene*/
		if (is->root_od->term->root_scene == is) {
			gf_sc_register_extra_graph(is->root_od->term->compositor, extra_scene, 1);
		}
	} else {
		if (gf_list_find(is->extra_scenes, extra_scene)>=0) return;
		gf_list_add(is->extra_scenes, extra_scene);
		/*for root scene*/
		if (is->root_od->term->root_scene == is) {
			gf_sc_register_extra_graph(is->root_od->term->compositor, extra_scene, 0);
		}
	}
}


static void gf_inline_get_video_size(GF_MediaObject *mo, u32 *w, u32 *h)
{
	u32 pixel_ar;
	if (!gf_mo_get_visual_info(mo, w, h, NULL, &pixel_ar, NULL)) return;
	if (pixel_ar) {
		u32 n, d;
		n = (pixel_ar>>16) & 0xFF;
		d = (pixel_ar) & 0xFF;
		*w = (*w * n) / d;
	}
}

static void IS_UpdateVideoPos(GF_InlineScene *is)
{
	MFURL url;
	M_Transform2D *tr;
	GF_MediaObject *mo;
	u32 w, h, v_w, v_h;
	if (!is->visual_url.OD_ID && !is->visual_url.url) return;

	url.count = 1;
	url.vals = &is->visual_url;
	mo = IS_CheckExistingObject(is, &url, GF_MEDIA_OBJECT_VIDEO);
	if (!mo) return;
	tr = (M_Transform2D *) gf_sg_find_node_by_name(is->graph, "DYN_TRANS");
	if (!tr) return;

	gf_sg_get_scene_size_info(is->graph, &w, &h);
	if (!w || !h) return;

	gf_inline_get_video_size(mo, &v_w, &v_h);
	tr->translation.x = INT2FIX((s32) (w - v_w)) / 2;
	tr->translation.y = INT2FIX((s32) (h - v_h)) / 2;
	gf_node_dirty_set((GF_Node *)tr, 0, 0);

	if (is->root_od->term->root_scene == is) {
		//if (is->graph_attached) gf_sc_set_scene(is->root_od->term->compositor, NULL);
		gf_sc_set_scene(is->root_od->term->compositor, is->graph);
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
	if (odm->OD->objectDescriptorID != GF_ESM_DYNAMIC_OD_ID) return (url->OD_ID==odm->OD->objectDescriptorID) ? 1 : 0;
	if (!url->url || !odm->OD->URLString) return 0;
	return !stricmp(url->url, odm->OD->URLString);
}

void gf_inline_force_scene_size_video(GF_InlineScene *is, GF_MediaObject *mo)
{
	u32 w, h;
	gf_inline_get_video_size(mo, &w, &h);
	gf_inline_force_scene_size(is, w, h);
}


/*regenerates the scene graph for dynamic scene.
This will also try to reload any previously presented streams. Note that in the usual case the scene is generated
just once when receiving the first OD AU (ressources are NOT destroyed when seeking), but since the network may need
to update the OD ressources, we still kake care of it*/
void gf_inline_regenerate(GF_InlineScene *is)
{
	u32 i, nb_obj, w, h;
	GF_Node *n1, *n2;
	SFURL *sfu;
	GF_Event evt;
	GF_ObjectManager *first_odm, *odm;
	M_AudioClip *ac;
	M_MovieTexture *mt;
	M_AnimationStream *as;
	M_Inline *dims;

	if (!is->is_dynamic_scene) return;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Inline] Regenerating scene graph for service %s\n", is->root_od->net_service->url));

	gf_sc_lock(is->root_od->term->compositor, 1);

	if (is->root_od->term->root_scene == is) 
		gf_sc_set_scene(is->root_od->term->compositor, NULL);

	gf_sg_reset(is->graph);
	gf_sg_get_scene_size_info(is->graph, &w, &h);
	gf_sg_set_scene_size_info(is->graph, w, h, 1);

	n1 = is_create_node(is->graph, TAG_MPEG4_OrderedGroup, NULL);
	gf_sg_set_root_node(is->graph, n1);
	gf_node_register(n1, NULL);

	n2 = is_create_node(is->graph, TAG_MPEG4_Sound2D, NULL);
	gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
	gf_node_register(n2, n1);

	ac = (M_AudioClip *) is_create_node(is->graph, TAG_MPEG4_AudioClip, "DYN_AUDIO");
	ac->startTime = gf_inline_get_time(is);
	((M_Sound2D *)n2)->source = (GF_Node *)ac;
	gf_node_register((GF_Node *)ac, n2);

	nb_obj = 0;
	first_odm = NULL;
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i))) {
		if (!odm->codec || (odm->codec->type!=GF_STREAM_AUDIO)) continue;

		if (is_odm_url(&is->audio_url, odm)) {
			gf_sg_vrml_mf_append(&ac->url, GF_SG_VRML_MFURL, (void **) &sfu);
			sfu->OD_ID = is->audio_url.OD_ID;
			if (is->audio_url.url) sfu->url = strdup(is->audio_url.url);
			first_odm = NULL;
			nb_obj++;
			break;
		}
		if (!first_odm) first_odm = odm;
	}
	if (first_odm) {
		if (is->audio_url.url) free(is->audio_url.url);
		is->audio_url.url = NULL;
		is->audio_url.OD_ID = first_odm->OD->objectDescriptorID;
		if (is->audio_url.OD_ID==GF_ESM_DYNAMIC_OD_ID) is->audio_url.url = strdup(first_odm->net_service->url);
		gf_sg_vrml_mf_append(&ac->url, GF_SG_VRML_MFURL, (void **) &sfu);
		sfu->OD_ID = is->audio_url.OD_ID;
		if (is->audio_url.url) sfu->url = strdup(is->audio_url.url);
		nb_obj++;

		if (!is->dyn_ck) is->dyn_ck = first_odm->codec->ck;
	}

	/*transform for any translation due to scene resize (3GPP)*/
	n2 = is_create_node(is->graph, TAG_MPEG4_Transform2D, "DYN_TRANS");
	gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
	gf_node_register(n2, n1);
	n1 = n2;

	n2 = is_create_node(is->graph, TAG_MPEG4_Shape, NULL);
	gf_node_list_add_child( &((GF_ParentNode *)n1)->children, n2);
	gf_node_register(n2, n1);
	n1 = n2;
	n2 = is_create_node(is->graph, TAG_MPEG4_Appearance, NULL);
	((M_Shape *)n1)->appearance = n2;
	gf_node_register(n2, n1);

	/*note we create a movie texture even for images...*/
	mt = (M_MovieTexture *) is_create_node(is->graph, TAG_MPEG4_MovieTexture, "DYN_VIDEO");
	mt->startTime = gf_inline_get_time(is);
	((M_Appearance *)n2)->texture = (GF_Node *)mt;
	gf_node_register((GF_Node *)mt, n2);

	first_odm = NULL;
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i))) {
		if (!odm->codec || (odm->codec->type!=GF_STREAM_VISUAL)) continue;

		if (is_odm_url(&is->visual_url, odm)) {
			gf_sg_vrml_mf_append(&mt->url, GF_SG_VRML_MFURL, (void **) &sfu);
			sfu->OD_ID = is->visual_url.OD_ID;
			if (is->visual_url.url) sfu->url = strdup(is->visual_url.url);
			if (odm->mo) {
				gf_inline_get_video_size(odm->mo, &w, &h);
				gf_sg_set_scene_size_info(is->graph, w, h, 1);
			}
			first_odm = NULL;
			nb_obj++;
			break;
		}
		if (!first_odm)
			first_odm = odm;
	}
	if (first_odm) {
		if (is->visual_url.url) free(is->visual_url.url);
		is->visual_url.url = NULL;
		is->visual_url.OD_ID = first_odm->OD->objectDescriptorID;
		if (is->visual_url.OD_ID==GF_ESM_DYNAMIC_OD_ID) is->visual_url.url = strdup(first_odm->net_service->url);

		gf_sg_vrml_mf_append(&mt->url, GF_SG_VRML_MFURL, (void **) &sfu);
		sfu->OD_ID = is->visual_url.OD_ID;
		if (is->visual_url.url) sfu->url = strdup(is->visual_url.url);
	
		if (first_odm->mo) {
			gf_inline_get_video_size(first_odm->mo, &w, &h);
			gf_sg_set_scene_size_info(is->graph, w, h, 1);
		}
		nb_obj++;
		if (!is->dyn_ck) is->dyn_ck = first_odm->codec->ck;
	}

	n2 = is_create_node(is->graph, TAG_MPEG4_Bitmap, NULL);
	((M_Shape *)n1)->geometry = n2;
	gf_node_register(n2, n1);


	/*text streams controlled through AnimationStream*/
	n1 = gf_sg_get_root_node(is->graph);
	as = (M_AnimationStream *) is_create_node(is->graph, TAG_MPEG4_AnimationStream, "DYN_TEXT");
	gf_node_list_add_child( &((GF_ParentNode *)n1)->children, (GF_Node*)as);
	gf_node_register((GF_Node *)as, n1);

	first_odm = NULL;
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i))) {
		if (!odm->codec || ((odm->codec->type!=GF_STREAM_TEXT) && (odm->codec->type!=GF_STREAM_ND_SUBPIC)) ) continue;

		if (!nb_obj || is_odm_url(&is->text_url, odm)) {
			if (is->text_url.url) free(is->text_url.url);
			is->text_url.url = NULL;

			gf_sg_vrml_mf_append(&as->url, GF_SG_VRML_MFURL, (void **) &sfu);
			sfu->OD_ID = is->text_url.OD_ID = odm->OD->objectDescriptorID;
			if (odm->OD->objectDescriptorID == GF_ESM_DYNAMIC_OD_ID) {
				sfu->url = strdup(odm->net_service->url);
				is->text_url.url = strdup(odm->net_service->url);
			}
			first_odm = NULL;
			if (!is->dyn_ck) is->dyn_ck = odm->codec->ck;
			break;
		}
		if (!first_odm) first_odm = odm;
	}
	if (first_odm) {
		if (is->text_url.url) free(is->text_url.url);
		is->text_url.url = NULL;
		gf_sg_vrml_mf_append(&as->url, GF_SG_VRML_MFURL, (void **) &sfu);
		sfu->OD_ID = is->text_url.OD_ID = first_odm->OD->objectDescriptorID;
		if (is->text_url.OD_ID==GF_ESM_DYNAMIC_OD_ID) {
			is->text_url.url = strdup(first_odm->net_service->url);
			sfu->url = strdup(first_odm->net_service->url);
		}
		if (!is->dyn_ck) is->dyn_ck = first_odm->codec->ck;
	}


	/*3GPP DIMS streams controlled */
	n1 = gf_sg_get_root_node(is->graph);
	dims = (M_Inline *) is_create_node(is->graph, TAG_MPEG4_Inline, "DYN_SCENE");
	gf_node_list_add_child( &((GF_ParentNode *)n1)->children, (GF_Node*)dims);
	gf_node_register((GF_Node *)dims, n1);

	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i))) {
		if (!odm->subscene || !odm->subscene->scene_codec) continue;

		gf_sg_vrml_mf_append(&dims->url, GF_SG_VRML_MFURL, (void **) &sfu);
		sfu->OD_ID = odm->OD->objectDescriptorID;
		if (odm->OD->objectDescriptorID == GF_ESM_DYNAMIC_OD_ID) {
			sfu->url = strdup(odm->net_service->url);
		}
		if (!is->dyn_ck) is->dyn_ck = odm->subscene->scene_codec->ck;
		break;
	}

	gf_sc_lock(is->root_od->term->compositor, 0);
	
	/*disconnect to force resize*/
	if (is->root_od->term->root_scene == is) {
		if (is->graph_attached) gf_sc_set_scene(is->root_od->term->compositor, NULL);
		gf_sc_set_scene(is->root_od->term->compositor, is->graph);
		is->graph_attached = 1;
		evt.type = GF_EVENT_STREAMLIST;
		GF_USER_SENDEVENT(is->root_od->term->user,&evt);
		IS_UpdateVideoPos(is);
	} else {
		is->graph_attached = 1;
		gf_term_invalidate_compositor(is->root_od->term);
	}
}

static Bool check_odm_deactivate(SFURL *url, GF_ObjectManager *odm, GF_Node *n)
{
	GF_FieldInfo info;
	if (!is_odm_url(url, odm) || !n) return 0;

	if (url->url) free(url->url);
	url->url = NULL;
	url->OD_ID = 0;

	gf_node_get_field_by_name(n, "url", &info);
	gf_sg_vrml_mf_reset(info.far_ptr, GF_SG_VRML_MFURL);
	gf_node_get_field_by_name(n, "stopTime", &info);
	*((SFTime *)info.far_ptr) = gf_node_get_scene_time(n);
	gf_node_changed(n, NULL);
	return 1;
}

void gf_inline_select_object(GF_InlineScene *is, GF_ObjectManager *odm)
{
	char *url;
	if (!is->is_dynamic_scene || !is->graph_attached || !odm) return;
	
	if (!odm->codec) return;

	if (odm->state) {
		if (check_odm_deactivate(&is->audio_url, odm, gf_sg_find_node_by_name(is->graph, "DYN_AUDIO")) ) return;
		if (check_odm_deactivate(&is->visual_url, odm, gf_sg_find_node_by_name(is->graph, "DYN_VIDEO") )) return;
		if (check_odm_deactivate(&is->text_url, odm, gf_sg_find_node_by_name(is->graph, "DYN_TEXT") )) return;
	}

	if (odm->codec->type == GF_STREAM_AUDIO) {
		M_AudioClip *ac = (M_AudioClip *) gf_sg_find_node_by_name(is->graph, "DYN_AUDIO");
		if (!ac) return;
		if (is->audio_url.url) free(is->audio_url.url);
		is->audio_url.url = NULL;
		is->audio_url.OD_ID = odm->OD->objectDescriptorID;
		if (!ac->url.count) gf_sg_vrml_mf_alloc(&ac->url, GF_SG_VRML_MFURL, 1);
		ac->url.vals[0].OD_ID = odm->OD->objectDescriptorID;
		if (ac->url.vals[0].url) {
			free(ac->url.vals[0].url);
			ac->url.vals[0].url = NULL; 
		}
		url = odm->mo->URLs.count ? odm->mo->URLs.vals[0].url : NULL;
		if (url) {
			is->audio_url.url = strdup(url);
			ac->url.vals[0].url = strdup(url);
		}
		ac->startTime = gf_inline_get_time(is);
		gf_node_changed((GF_Node *)ac, NULL);
		return;
	}

	if (odm->codec->type == GF_STREAM_VISUAL) {
		M_MovieTexture *mt = (M_MovieTexture*) gf_sg_find_node_by_name(is->graph, "DYN_VIDEO");
		if (!mt) return;
		if (is->visual_url.url) free(is->visual_url.url);
		is->visual_url.url = NULL;
		is->visual_url.OD_ID = odm->OD->objectDescriptorID;
		if (!mt->url.count) gf_sg_vrml_mf_alloc(&mt->url, GF_SG_VRML_MFURL, 1);
		mt->url.vals[0].OD_ID = odm->OD->objectDescriptorID;
		if (mt->url.vals[0].url) free(mt->url.vals[0].url);
		url = odm->mo->URLs.count ? odm->mo->URLs.vals[0].url : NULL;
		if (url) {
			is->visual_url.url = strdup(url);
			mt->url.vals[0].url = strdup(url);
		}
		mt->startTime = gf_inline_get_time(is);
		gf_node_changed((GF_Node *)mt, NULL);
		if (odm->mo) gf_inline_force_scene_size_video(is, odm->mo);
		return;
	}


	if (odm->codec->type == GF_STREAM_TEXT) {
		M_AnimationStream *as = (M_AnimationStream*) gf_sg_find_node_by_name(is->graph, "DYN_TEXT");
		if (!as) return;
		if (is->text_url.url) free(is->text_url.url);
		is->text_url.url = NULL;
		is->text_url.OD_ID = odm->OD->objectDescriptorID;
		if (!as->url.count) gf_sg_vrml_mf_alloc(&as->url, GF_SG_VRML_MFURL, 1);
		as->url.vals[0].OD_ID = odm->OD->objectDescriptorID;
		if (as->url.vals[0].url) free(as->url.vals[0].url);
		url = odm->mo->URLs.count ? odm->mo->URLs.vals[0].url : NULL;
		if (url) {
			is->text_url.url = strdup(url);
			as->url.vals[0].url = strdup(url);
		}
		as->startTime = gf_inline_get_time(is);
		gf_node_changed((GF_Node *)as, NULL);
		return;
	}
}


GF_EXPORT
void gf_inline_force_scene_size(GF_InlineScene *is, u32 width, u32 height)
{
	/*for now only allowed when no scene info*/
	if (!is->is_dynamic_scene) return;
	gf_sg_set_scene_size_info(is->graph, width, height, gf_sg_use_pixel_metrics(is->graph));
	
	if (is->root_od->term->root_scene == is) 
		gf_sc_set_scene(is->root_od->term->compositor, is->graph);

	gf_is_resize_event(is);

	IS_UpdateVideoPos(is);
}

void gf_inline_restart_dynamic(GF_InlineScene *is, u64 from_time)
{
	u32 i;
	GF_List *to_restart;
	GF_ObjectManager *odm;

	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[InlineScene] Restarting from "LLD"\n", LLD_CAST from_time));
	to_restart = gf_list_new();
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(is->ODlist, &i))) {
		if (odm->state) {
			gf_list_add(to_restart, odm);
			gf_odm_stop(odm, 1);
		}
	}

	/*reset clock*/
	if (is->dyn_ck) gf_clock_reset(is->dyn_ck);

	/*restart objects*/
	i=0;
	while ((odm = (GF_ObjectManager*)gf_list_enum(to_restart, &i))) {
		odm->media_start_time = from_time;
		gf_odm_start(odm);
	}
	gf_list_del(to_restart);

	/*also check nodes if no media control since they may be deactivated (end of stream)*/
	if (!is->root_od->media_ctrl) {
		M_AudioClip *ac = (M_AudioClip *) gf_sg_find_node_by_name(is->graph, "DYN_AUDIO");
		M_MovieTexture *mt = (M_MovieTexture *) gf_sg_find_node_by_name(is->graph, "DYN_VIDEO");
		M_AnimationStream *as = (M_AnimationStream *) gf_sg_find_node_by_name(is->graph, "DYN_TEXT");
		if (ac) {
			ac->startTime = gf_inline_get_time(is);
			gf_node_changed((GF_Node *)ac, NULL);
		}
		if (mt) {
			mt->startTime = gf_inline_get_time(is);
			gf_node_changed((GF_Node *)mt, NULL);
		}
		if (as) {
			as->startTime = gf_inline_get_time(is);
			gf_node_changed((GF_Node *)as, NULL);
		}
	}
}


GF_EXPORT
Bool gf_inline_process_anchor(GF_Node *caller, GF_Event *evt)
{
	u32 i;
	GF_Terminal *term;
	M_Inline *inl;
	GF_InlineScene *is;
	GF_SceneGraph *sg = gf_node_get_graph(caller);
	if (!sg) return 1;
	is = (GF_InlineScene *)gf_sg_get_private(sg);
	if (!is) return 1;
	term = is->root_od->term;

	/*if main scene forward to user. If no params or first one not "self" forward to user*/
	if ((term->root_scene==is) || !evt->navigate.parameters || !evt->navigate.param_count || (stricmp(evt->navigate.parameters[0], "self") && stricmp(evt->navigate.parameters[0], "_self"))) {
		if (term->user->EventProc) return term->user->EventProc(term->user->opaque, evt);
		return 1;
	}
	/*FIXME this is too restrictive, we assume the navigate URL is really a presentation one...*/
	i=0;
	while ((inl = (M_Inline*)gf_list_enum(is->inline_nodes, &i))) {
		switch (gf_node_get_tag((GF_Node *)inl)) {
		case TAG_MPEG4_Inline:
		case TAG_X3D_Inline:
			gf_sg_vrml_mf_reset(&inl->url, GF_SG_VRML_MFURL);
			gf_sg_vrml_mf_alloc(&inl->url, GF_SG_VRML_MFURL, 1);
			inl->url.vals[0].url = strdup(evt->navigate.to_url ? evt->navigate.to_url : "");
			/*signal URL change but don't destroy inline scene now since we got this event from inside the scene, 
			this could crash compositors*/
			is->needs_restart = 2;
			break;
		}
	}
	return 1;
}

GF_EXPORT
GF_Compositor *gf_sc_get_compositor(GF_Node *node)
{
	GF_InlineScene *is;
	GF_SceneGraph *sg = gf_node_get_graph(node);
	if (!sg) return NULL;
	is = (GF_InlineScene *)gf_sg_get_private(sg);
	if (!is) return NULL;
	return is->root_od->term->compositor;
}

const char *gf_inline_get_fragment_uri(GF_Node *node)
{
	GF_SceneGraph *sg = gf_node_get_graph(node);
	GF_InlineScene *is = sg ? (GF_InlineScene *) gf_sg_get_private(sg) : NULL;
	if (!is) return NULL;
	return is->fragment_uri;
}
void gf_inline_set_fragment_uri(GF_Node *node, const char *uri)
{
	GF_SceneGraph *sg = gf_node_get_graph(node);
	GF_InlineScene *is = sg ? (GF_InlineScene *) gf_sg_get_private(sg) : NULL;
	if (!is) return;
	if (is->fragment_uri) {
		free(is->fragment_uri);
		is->fragment_uri = NULL;
	}
	if (uri) is->fragment_uri = strdup(uri);
}

GF_Node *gf_inline_get_subscene_root(GF_Node *node)
{
	GF_InlineScene *is;
	if (!node) return NULL;
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Inline: case TAG_X3D_Inline: 
		break;
	default:
		return NULL;
	}
	is = (GF_InlineScene *)gf_node_get_private(node);
	if (!is) return NULL;
	return gf_sg_get_root_node(is->graph);
}

GF_Node *gf_inline_get_parent_node(GF_Node *node, u32 idx)
{
	GF_InlineScene *is;
	if (!node) return NULL;
	switch (gf_node_get_tag(node)) {
	case TAG_MPEG4_Inline: case TAG_X3D_Inline: 
		is = (GF_InlineScene *)gf_node_get_private(node);
		break;
	default:
		is = (GF_InlineScene *)gf_sg_get_private(gf_node_get_graph(node));
		break;
	}
	if (!is) return NULL;
	return (GF_Node *) gf_list_get(is->inline_nodes, idx);
}

void InitInline(GF_InlineScene *is, GF_Node *node)
{
	gf_node_set_callback_function(node, gf_inline_traverse);
}


