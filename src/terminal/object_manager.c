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
#include <gpac/constants.h>
#include "media_memory.h"
#include "media_control.h"

#include "input_sensor.h"


/*removes the channel ressources and destroy it*/
void ODM_DeleteChannel(GF_ObjectManager *odm, struct _es_channel *ch);

GF_EXPORT
GF_ObjectManager *gf_odm_new()
{
	GF_ObjectManager *tmp;
	GF_SAFEALLOC(tmp, GF_ObjectManager);
	if (!tmp) return NULL;

#ifndef GPAC_DISABLE_VRML
	tmp->ms_stack = gf_list_new();
	tmp->mc_stack = gf_list_new();
#endif
	return tmp;
}

void gf_odm_reset_media_control(GF_ObjectManager *odm, Bool signal_reset)
{
#ifndef GPAC_DISABLE_VRML
	MediaSensorStack *media_sens;
	MediaControlStack *media_ctrl;

	while ((media_sens = (MediaSensorStack *)gf_list_last(odm->ms_stack))) {
		MS_Stop(media_sens);
		/*and detach from stream object*/
		media_sens->stream = NULL;
		gf_list_rem_last(odm->ms_stack);
	}

	while ((media_ctrl = (MediaControlStack *)gf_list_last(odm->mc_stack))) {
		if (signal_reset)
			gf_odm_remove_mediacontrol(odm, media_ctrl);
		media_ctrl->stream = NULL;
		media_ctrl->ck = NULL;
		gf_list_rem_last(odm->mc_stack);
	}
#endif
}


void gf_odm_del(GF_ObjectManager *odm)
{
	if (odm->addon && (odm->addon->root_od==odm)) {
		odm->addon->root_od = NULL;
		odm->addon->started = 0;
	}
	if (odm->upper_layer_odm) {
		odm->upper_layer_odm->lower_layer_odm = NULL;
	}
	if (odm->lower_layer_odm) {
		odm->lower_layer_odm->upper_layer_odm = NULL;
	}
#if FILTER_FIXME
	/*make sure we are not in the media queue*/
	gf_term_lock_media_queue(odm->term, GF_TRUE);
	gf_list_del_item(odm->term->media_queue, odm);
	gf_term_check_connections_for_delete(odm->term, odm);
	gf_term_lock_media_queue(odm->term, GF_FALSE);
#endif

	/*detach media object as referenced by the scene - this should ensures that any attempt to lock the ODM from the
	compositor will fail as the media object is no longer linked to object manager*/
	if (odm->mo) odm->mo->odm = NULL;

#ifndef GPAC_DISABLE_VRML
	gf_odm_reset_media_control(odm, 0);
	gf_list_del(odm->ms_stack);
	gf_list_del(odm->mc_stack);
#endif

	if (odm->raw_frame_sema) gf_sema_del(odm->raw_frame_sema);
	if (odm->extra_pids) {
		while (gf_list_count(odm->extra_pids)) {
			GF_ODMExtraPid *xpid = gf_list_pop_back(odm->extra_pids);
			gf_free(xpid);
		}
		gf_list_del(odm->extra_pids);
	}
	gf_free(odm);
}


void gf_odm_register_pid(GF_ObjectManager *odm, GF_FilterPid *pid, Bool register_only)
{
	u32 es_id=0;
	const GF_PropertyValue *prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	if (!prop) prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (prop) es_id = prop->value.uint;

	if (! odm->pid) {
		odm->pid = pid;
		odm->pid_id = es_id;
	} else {
		GF_ODMExtraPid *xpid;
		if (!odm->extra_pids) odm->extra_pids = gf_list_new();
		GF_SAFEALLOC(xpid, GF_ODMExtraPid);
		xpid->pid = pid;
		xpid->pid_id = es_id;
		gf_list_add(odm->extra_pids, xpid);
	}

	if (register_only) return;

	gf_odm_setup_object(odm, odm->subscene ? odm->scene_ns : odm->parentscene->root_od->scene_ns, pid);
}

GF_EXPORT
void gf_odm_disconnect(GF_ObjectManager *odm, u32 do_remove)
{
	GF_Compositor *compositor = odm->parentscene ? odm->parentscene->compositor : odm->subscene->compositor;

	gf_odm_stop(odm, GF_TRUE);

	/*disconnect sub-scene*/
	if (odm->subscene) gf_scene_disconnect(odm->subscene, do_remove);

	/*no destroy*/
	if (!do_remove) return;

	/*unload the decoders before deleting the channels to prevent any access fault*/
#if FILTER_FIXME
	if (odm->codec) {
		if (odm->codec->type==GF_STREAM_INTERACT) {
			u32 i, count;
			// Remove all Registered InputSensor nodes -> shut down the InputSensor threads -> prevent illegal access on deleted pointers
			GF_MediaObject *obj = odm->mo;
			count = gf_mo_event_target_count(obj);
			for (i=0; i<count; i++) {
				GF_Node *n = (GF_Node *)gf_event_target_get_node(gf_mo_event_target_get(obj, i));
				switch (gf_node_get_tag(n)) {
#ifndef GPAC_DISABLE_VRML
				case TAG_MPEG4_InputSensor:
					((M_InputSensor*)n)->enabled = 0;
					InputSensorModified(n);
					break;
#endif
				default:
					break;
				}
			}
		}
		gf_term_remove_codec(odm->term, odm->codec);
	}
#endif

	/*delete from the parent scene.*/
	if (odm->parentscene) {
		GF_Event evt;
		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = GF_FALSE;
		gf_fs_forward_event(odm->parentscene->compositor->fsess, &evt, GF_FALSE, GF_TRUE);

		gf_scene_remove_object(odm->parentscene, odm, do_remove);
		if (odm->subscene) gf_scene_del(odm->subscene);
		gf_odm_del(odm);
		return;
	}

	/*this is the scene root OD (may be a remote OD ..) */
	if (odm->subscene) {
		GF_Event evt;
		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = GF_FALSE;
		gf_sc_send_event(compositor, &evt);
		gf_scene_del(odm->subscene);
	}

	/*delete the ODMan*/
	gf_odm_del(odm);
}

/*setup service for OD (extract IOD and go)*/
void gf_odm_setup_entry_point(GF_ObjectManager *odm, const char *service_sub_url)
{
#if FILTER_FIXME
	u32 od_type;
	char *ext, *redirect_url;
	char *sub_url = (char *) service_sub_url;
	GF_Descriptor *desc;

	if (odm->flags & GF_ODM_DESTROYED) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM] Root object of service %s has been scheduled for destruction - ignoring object setup\n", service_sub_url));
		return;
	}
//	assert(odm->OD==NULL);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM] Setting up root object for %s\n", odm->net_service->url));

	if (odm->subscene) {
		char *sep = strchr(sub_url, '#');
		if (sep && !strnicmp(sep, "#LIVE360", 8)) {
			sep[0] = 0;
			odm->subscene->vr_type = 1;
		}
		od_type = GF_MEDIA_OBJECT_SCENE;
	} else if (odm->mo) {
		od_type = odm->mo->type;
		if (!sub_url && odm->mo->URLs.count && odm->mo->URLs.vals[0].url) {
			sub_url = odm->mo->URLs.vals[0].url;
		}
	}
	else od_type = GF_MEDIA_OBJECT_UNDEF;

	/*for remote ODs, get expected OD type in case the service needs to generate the IOD on the fly*/
	if (odm->parentscene && odm->OD && odm->OD->URLString) {
		GF_MediaObject *mo;
		mo = gf_scene_find_object(odm->parentscene, odm->ID, odm->OD->URLString);
		if (mo) od_type = mo->type;
		ext = strchr(odm->OD->URLString, '#');
		if (ext) sub_url = ext;
	}

	desc = odm->net_service->ifce->GetServiceDescriptor(odm->net_service->ifce, od_type, sub_url);

	/*entry point is already setup (bad design in GPAC, happens with BIFS TS and DASH*/
	if (odm->OD) {
		if (!desc) return;
		if (gf_list_count(odm->OD->ESDescriptors)) {
			gf_odf_desc_del(desc);
			return;
		}
		gf_odf_desc_del((GF_Descriptor *) odm->OD);
		odm->OD=NULL;
		if (odm->subscene)
			odm->subscene->is_dynamic_scene = GF_FALSE;
	}

	if (!desc) {
		if (odm->OD && !gf_list_count(odm->OD->ESDescriptors))
			return;

		/*if desc is NULL for a media, the media will be declared later by the service (gf_term_media_add)*/
		if (od_type != GF_MEDIA_OBJECT_SCENE) {
			return;
		}
		/*create empty service descriptor, this will automatically create a dynamic scene*/
		desc = gf_odf_desc_new(GF_ODF_OD_TAG);
	}
	odm->flags |= GF_ODM_SERVICE_ENTRY;

	if (!gf_list_count( ((GF_ObjectDescriptor*)desc)->ESDescriptors)) {
		/*new subscene*/
		if (!odm->subscene) {
			assert(odm->parentscene);
			odm->subscene = gf_scene_new(NULL, odm->parentscene);
			odm->subscene->root_od = odm;
		}
	}

	redirect_url = NULL;
	switch (desc->tag) {
	case GF_ODF_IOD_TAG:
	{
		GF_InitialObjectDescriptor *the_iod = (GF_InitialObjectDescriptor *)desc;
		odm->OD = (GF_ObjectDescriptor*)gf_malloc(sizeof(GF_ObjectDescriptor));
		memcpy(odm->OD, the_iod, sizeof(GF_ObjectDescriptor));
		odm->OD->tag = GF_ODF_OD_TAG;
		/*Check P&Ls of this IOD*/
		odm->Audio_PL = the_iod->audio_profileAndLevel;
		odm->Graphics_PL = the_iod->graphics_profileAndLevel;
		odm->OD_PL = the_iod->OD_profileAndLevel;
		odm->Scene_PL = the_iod->scene_profileAndLevel;
		odm->Visual_PL = the_iod->visual_profileAndLevel;
		odm->flags |= GF_ODM_HAS_PROFILES;
		if (the_iod->inlineProfileFlag) odm->flags |= GF_ODM_INLINE_PROFILES;
		redirect_url = the_iod->URLString;
		odm->OD->URLString = NULL;
		gf_odf_desc_del((GF_Descriptor *) the_iod->IPMPToolList);
		gf_free(the_iod);
	}
	break;
	case GF_ODF_OD_TAG:
		odm->Audio_PL = odm->Graphics_PL = odm->OD_PL = odm->Scene_PL = odm->Visual_PL = (u8) -1;
		odm->OD = (GF_ObjectDescriptor *)desc;
		redirect_url = odm->OD->URLString;
		odm->OD->URLString = NULL;
		break;
	default:
		gf_term_message(odm->term, odm->net_service->url, "MPEG4 Service Setup Failure", GF_ODF_INVALID_DESCRIPTOR);
		goto err_exit;
	}

	gf_odm_setup_object(odm, odm->net_service);

	if (redirect_url && !strnicmp(redirect_url, "views://", 8)) {
		gf_scene_generate_views(odm->subscene ? odm->subscene : odm->parentscene , (char *) redirect_url + 8, (char*)odm->parentscene ? odm->parentscene->root_od->net_service->url : NULL);
	}
	/*it may happen that this object was inserted in a dynamic scene from a service through a URL redirect. In which case,
	the scene regeneration might not have been completed since the redirection was not done yet - force a scene regenerate*/
	else if (odm->parentscene && odm->parentscene->is_dynamic_scene) {
		gf_scene_regenerate(odm->parentscene);
	}

	gf_free(redirect_url);

	return;

err_exit:
	if (!odm->parentscene) {
		GF_Event evt;
		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = GF_FALSE;
		gf_sc_send_event(odm->parentscene->compositor, &evt);
	}
	if (redirect_url)
		gf_free(redirect_url);
#endif
}


/*locate ESD by ID*/
static GF_ESD *od_get_esd(GF_ObjectDescriptor *OD, u16 ESID)
{
	GF_ESD *esd;
	u32 i = 0;
	while ((esd = (GF_ESD *)gf_list_enum(OD->ESDescriptors, &i)) ) {
		if (esd->ESID==ESID) return esd;
	}
	return NULL;
}

static void ODM_SelectAlternateStream(GF_ObjectManager *odm, const char *lang_3cc, const char *lang_2cc, u8 stream_type)
{
#if FILTER_FIXME
	u32 i;
	GF_ESD *esd;
	u16 def_id, es_id;
	char esCode[4];

	def_id = 0;
	i=0;
	while ( (esd = (GF_ESD *)gf_list_enum(odm->OD->ESDescriptors, &i)) ) {
		if (esd->decoderConfig->streamType != stream_type) continue;

		if (!esd->langDesc) {
			if (!def_id) def_id = esd->ESID;
			continue;
		}
		esCode[0] = esd->langDesc->langCode>>16;
		esCode[1] = esd->langDesc->langCode>>8;
		esCode[2] = esd->langDesc->langCode;
		esCode[3] = 0;
		
		if (!stricmp(esCode, lang_3cc) || !strnicmp(esCode, lang_2cc, 2)) {
			def_id = esd->ESID;
			break;
		} else if (!def_id) {
			def_id = esd->ESID;
		}
	}

	/*remove all other media streams*/
	i=0;
	while ((esd = (GF_ESD *)gf_list_enum(odm->OD->ESDescriptors, &i)) ) {
		if (esd->decoderConfig->streamType != stream_type) continue;

		/*get base stream ID for this stream*/
		es_id = esd->ESID;
		if (esd->dependsOnESID && (esd->dependsOnESID != es_id)) {
			es_id = esd->dependsOnESID;
			while (es_id) {
				GF_ESD *base = od_get_esd(odm->OD, es_id);
				if (!base) break;
				/*forbidden except for BIFS->OD*/
				if (base->decoderConfig->streamType != stream_type) break;
				if (!base->dependsOnESID) break;
				es_id = base->dependsOnESID;
			}
		}
		/*not part of same object as base, remove*/
		if (es_id != def_id) {
			gf_list_del_item(odm->OD->ESDescriptors, esd);
			gf_odf_desc_del((GF_Descriptor*) esd);
			i--;
		}
	}
#endif
}

/*Validate the streams in this OD, and check if we have to setup an inline scene*/
GF_Err ODM_ValidateOD(GF_ObjectManager *odm, Bool *hasInline)
{
#if FILTER_FIXME
	u32 i;
	u16 es_id, ck_id;
	GF_ESD *esd, *base_scene;
	const char *lang_3cc, *lang_2cc;
	u32 nb_od, nb_ocr, nb_scene, nb_mp7, nb_ipmp, nb_oci, nb_mpj, nb_other, prev_st;

	nb_od = nb_ocr = nb_scene = nb_mp7 = nb_ipmp = nb_oci = nb_mpj = nb_other = 0;
	prev_st = 0;

	*hasInline = 0;

	/*step 1: validate OD*/
	ck_id = 0;
	i=0;
	while ((esd = (GF_ESD *)gf_list_enum(odm->OD->ESDescriptors, &i))) {
		assert(esd->decoderConfig);
		//force single clock ID in one object
		if (!ck_id) ck_id = esd->OCRESID;
		else esd->OCRESID = ck_id;

		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_OD:
			nb_od++;
			break;
		case GF_STREAM_OCR:
			nb_ocr++;
			break;
		case GF_STREAM_SCENE:
			switch (esd->decoderConfig->objectTypeIndication) {
			case GPAC_OTI_SCENE_AFX:
			case GPAC_OTI_SCENE_SYNTHESIZED_TEXTURE:
				break;
			default:
				nb_scene++;
				break;
			}
			break;
		case GF_STREAM_MPEG7:
			nb_mp7++;
			break;
		case GF_STREAM_IPMP:
			nb_ipmp++;
			break;
		case GF_STREAM_OCI:
			nb_oci++;
			break;
		case GF_STREAM_MPEGJ:
			nb_mpj++;
			break;
		case GF_STREAM_PRIVATE_SCENE:
			nb_scene++;
			break;
		/*all other streams shall not be mixed: text, video, audio, interaction, font*/
		default:
			if (esd->decoderConfig->streamType!=prev_st) nb_other++;
			prev_st = esd->decoderConfig->streamType;
			break;
		}
	}
	/*cf spec on stream aggregation*/
	if (nb_other>1) return GF_ODF_INVALID_DESCRIPTOR;	/*no more than one base media type*/
	if (nb_od && !nb_scene) return GF_ODF_INVALID_DESCRIPTOR; /*if OD we must have scene description*/
	if (nb_other && nb_scene) return GF_ODF_INVALID_DESCRIPTOR; /*scene OR media*/
	if (nb_ocr>1) return GF_ODF_INVALID_DESCRIPTOR; /*only ONE OCR*/
	if (nb_oci>1) return GF_ODF_INVALID_DESCRIPTOR; /*only ONE OCI*/
	if (nb_mp7>1) return GF_ODF_INVALID_DESCRIPTOR; /*only ONE MPEG-7 - this is not in the spec, but since MPEG-7 = OCI++ this seems reasonable*/
	if (nb_mpj>1) return GF_ODF_INVALID_DESCRIPTOR; /*only ONE MPEG-J - this is not in the spec but well...*/

	/*the rest should be OK*/

	/*select independant streams - check language and (TODO) bitrate & term caps*/
	lang_3cc = gf_cfg_get_key(odm->term->user->config, "Systems", "Language3CC");
	if (!lang_3cc) {
		lang_3cc = "eng";
		gf_cfg_set_key(odm->term->user->config, "Systems", "Language3CC", "eng");
		gf_cfg_set_key(odm->term->user->config, "Systems", "Language2CC", "en");
		gf_cfg_set_key(odm->term->user->config, "Systems", "LanguageName", "English");
	}
	lang_2cc = gf_cfg_get_key(odm->term->user->config, "Systems", "Language2CC");
	if (!lang_2cc) {
		lang_2cc = "en";
		gf_cfg_set_key(odm->term->user->config, "Systems", "Language3CC", "eng");
		gf_cfg_set_key(odm->term->user->config, "Systems", "Language2CC", "en");
		gf_cfg_set_key(odm->term->user->config, "Systems", "LanguageName", "English");
	}

	if (gf_list_count(odm->OD->ESDescriptors)>1) {
		ODM_SelectAlternateStream(odm, lang_3cc, lang_2cc, GF_STREAM_SCENE);
		ODM_SelectAlternateStream(odm, lang_3cc, lang_2cc, GF_STREAM_OD);
		ODM_SelectAlternateStream(odm, lang_3cc, lang_2cc, GF_STREAM_VISUAL);
		ODM_SelectAlternateStream(odm, lang_3cc, lang_2cc, GF_STREAM_AUDIO);
		ODM_SelectAlternateStream(odm, lang_3cc, lang_2cc, GF_STREAM_IPMP);
		ODM_SelectAlternateStream(odm, lang_3cc, lang_2cc, GF_STREAM_INTERACT);
		ODM_SelectAlternateStream(odm, lang_3cc, lang_2cc, GF_STREAM_TEXT);
	}

	/*no scene, OK*/
	if (!nb_scene) return GF_OK;

	/*check if inline or animation stream*/
	*hasInline = 1;
	base_scene = NULL;
	i=0;
	while ( (esd = (GF_ESD *)gf_list_enum(odm->OD->ESDescriptors, &i)) ) {
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_PRIVATE_SCENE:
		case GF_STREAM_SCENE:
			base_scene = esd;
			break;
		default:
			break;
		}
		if (base_scene) break;
	}

	/*we have a scene stream without dependancies, this is an inline*/
	if (!base_scene || !base_scene->dependsOnESID) return GF_OK;

	/*if the stream the base scene depends on is in this OD, this is in inline*/
	es_id = base_scene->dependsOnESID;
	while (es_id) {
		esd = od_get_esd(odm->OD, es_id);
		/*the stream this stream depends on is not in this OD, this is some anim stream*/
		if (!esd) {
			*hasInline = 0;
			return GF_OK;
		}
		es_id = esd->dependsOnESID;
		/*should be forbidden (circular reference), we assume this describes inline (usually wrong BIFS->OD setup)*/
		if (es_id==base_scene->ESID) break;
	}
#endif
	/*no dependency to external stream, this is an inline*/
	return GF_OK;
}

static Bool gf_odm_should_auto_select(GF_ObjectManager *odm)
{
#if FILTER_FIXME
	u32 i, count;
	if (gf_codec_is_scene_or_image(odm->codec)) return GF_TRUE;

	if (odm->parentscene && !odm->parentscene->is_dynamic_scene) {
		return GF_TRUE;
	}

	if (odm->parentscene && odm->parentscene->root_od->addon) {
		if (odm->parentscene->root_od->addon->addon_type == GF_ADDON_TYPE_MAIN)
			return GF_FALSE;
	}

	count = gf_list_count(odm->parentscene->resources);
	for (i=0; i<count; i++) {
		GF_ObjectManager *an_odm = gf_list_get(odm->parentscene->resources, i);
		if (an_odm==odm) continue;
		if (!an_odm->codec) continue;
		if (an_odm->codec->type != odm->codec->type) continue;
		//same type - if the first one has been autumatically activated, do not activate this one
		if (an_odm->state == GF_ODM_STATE_PLAY) return GF_FALSE;
	}
#endif
	return GF_TRUE;
}


void gf_odm_setup_remote_object(GF_ObjectManager *odm, GF_SceneNamespace *parent_ns, char *remote_url)
{
	char *parent_url = NULL;
	if (!remote_url) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] No URL specified for remote object - ignoring object setup\n", odm->ID));
		return;
	}

	if (!odm->scene_ns) {
		if (odm->flags & GF_ODM_DESTROYED) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] Object has been scheduled for destruction - ignoring object setup\n", odm->ID));
			return;
		}
		odm->scene_ns = parent_ns;
		odm->scene_ns->nb_odm_users++;
	}

	/*store original OD ID */
	if (!odm->media_current_time) odm->media_current_time = odm->ID;

	//detach it
	odm->scene_ns = NULL;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] Object redirection to %s (MO %08x)\n", odm->ID, remote_url, odm->mo));

	/*if object is a scene, create the inline before connecting the object.
		This is needed in irder to register the nodes using the resource for event
		propagation (stored at the inline level)
	*/
	if (odm->mo && (odm->mo->type==GF_MEDIA_OBJECT_SCENE)) {
		odm->subscene = gf_scene_new(NULL, odm->parentscene);
		odm->subscene->root_od = odm;
	}
	parent_url = parent_ns ? parent_ns->url : NULL;
	if (parent_url && !strnicmp(parent_url, "views://", 8))
		parent_url = NULL;

	//make sure we don't have an ID before attempting to connect
	odm->ID = 0;
	odm->ServiceID = 0;
	gf_scene_ns_connect_object(odm->subscene ? odm->subscene : odm->parentscene, odm, remote_url, parent_url);
}

GF_EXPORT
void gf_odm_setup_object(GF_ObjectManager *odm, GF_SceneNamespace *parent_ns, GF_FilterPid *for_pid)
{
	GF_Err e;
	GF_MediaObject *syncRef;

	if (!odm->scene_ns) {
		if (odm->flags & GF_ODM_DESTROYED) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM%d] Object has been scheduled for destruction - ignoring object setup\n", odm->ID));
			return;
		}
		odm->scene_ns = parent_ns;
	}
		/*restore OD ID */
	if (odm->media_current_time) {
		odm->ID = odm->media_current_time;
		odm->media_current_time = 0;
		odm->flags |= GF_ODM_REMOTE_OD;
	}

	syncRef = (GF_MediaObject*)odm->sync_ref;

	if (odm->scene_ns->owner &&  (odm->scene_ns->owner->flags & GF_ODM_INHERIT_TIMELINE)) {
		odm->flags |= GF_ODM_INHERIT_TIMELINE;
	}

	/*empty object, use a dynamic scene*/
	if (! odm->pid && odm->subscene) {
		assert(odm->subscene->root_od==odm);
		odm->subscene->is_dynamic_scene = GF_TRUE;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Setting up object streams\n"));

		e = gf_odm_setup_pid(odm, for_pid);
		if (e) {
			GF_LOG(GF_LOG_MEDIA, GF_LOG_ERROR, ("Service %s PID %s Setup Failure: %s", odm->scene_ns->url, gf_filter_pid_get_name(for_pid ? for_pid : odm->pid), gf_error_to_string(e) ));
		}
	}
	/*setup mediaobject info except for top-level OD*/
	if (odm->parentscene) {
		GF_Event evt;

		//this may result in an attempt to lock the compositor, so release the net MX before
		if (!odm->scalable_addon) {
			gf_scene_setup_object(odm->parentscene, odm);
		}

		/*setup node decoder*/
#if FILTER_FIXME
		if (odm->mo && odm->codec && odm->codec->decio && (odm->codec->decio->InterfaceType==GF_NODE_DECODER_INTERFACE) ) {
			GF_NodeDecoder *ndec = (GF_NodeDecoder *) odm->codec->decio;
			GF_Node *n = gf_event_target_get_node(gf_mo_event_target_get(odm->mo, 0));
			if (n) ndec->AttachNode(ndec, n);

			/*not clear in the spec how the streams attached to AFX are started - default to "right now"*/
			gf_odm_start(odm);
		}
#endif
		if (odm->pid==for_pid) {
			evt.type = GF_EVENT_CONNECT;
			evt.connect.is_connected = GF_TRUE;
			gf_fs_forward_event(odm->parentscene->compositor->fsess, &evt, GF_FALSE, GF_TRUE);
		}
	} else if (odm->pid==for_pid) {
		/*othewise send a connect ack for top level*/
		GF_Event evt;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM] Root object connected (%s) !\n", odm->scene_ns->url));

		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = GF_TRUE;
		gf_sc_send_event(odm->subscene->compositor, &evt);
	}

	/* start object*/
	/*object is already started (new PID was inserted for this object)*/
	if (odm->state==GF_ODM_STATE_PLAY) {
		if (odm->pid==for_pid)
			odm->state = GF_ODM_STATE_STOP;
		gf_odm_start(odm);
	}
	/*object is the root, always start*/
	else if (!odm->parentscene) {
		assert(odm->subscene && (odm->subscene->root_od==odm));
		odm->flags &= ~GF_ODM_NOT_SETUP;
		gf_odm_start(odm);
	}
	/*object is a pure OCR object - connect*/
	else if (odm->type==GF_STREAM_OCR) {
		odm->flags &= ~GF_ODM_NOT_SETUP;
		gf_odm_start(odm);
	}
	/*if the object is inserted from a broadcast, start it if not already done. This covers cases where the scene (BIFS, LASeR) and
	the media (images) are both carrouseled and the carrousels are interleaved. If we wait for the scene to trigger a PLAY, we will likely
	have to wait for an entire image carousel period to start filling the buffers, which is sub-optimal
	we also force a prefetch for object declared outside the OD stream to make sure we don't loose any data before object declaration and play
	as can be the case with MPEG2 TS (first video packet right after the PMT) - this should be refined*/
	else if ( ((odm->flags & GF_ODM_NO_TIME_CTRL) || (odm->flags & GF_ODM_NOT_IN_OD_STREAM)) && gf_odm_should_auto_select(odm) && (odm->parentscene->selected_service_id == odm->ServiceID)) {
		Bool force_play = GF_FALSE;

		if (odm->state==GF_ODM_STATE_STOP) {
			odm->flags |= GF_ODM_PREFETCH;
			force_play = GF_TRUE;
		}
#if FILTER_FIXME
		/*the object could have been queued for play when setting up the scene object. If so, remove from queue and start right away*/
		else if ((odm->state==GF_ODM_STATE_PLAY) && (gf_list_del_item(odm->term->media_queue, odm)>=0) ) {
			force_play = GF_TRUE;
		}
#endif

		if (force_play) {
			odm->flags |= GF_ODM_INITIAL_BROADCAST_PLAY;
			odm->parentscene->selected_service_id = odm->ServiceID;

			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d] Inserted from input service %s - forcing play\n", odm->ID, odm->scene_ns->url));
			odm->flags &= ~GF_ODM_NOT_SETUP;
			gf_odm_start(odm);
		}
	}

	odm->flags &= ~GF_ODM_NOT_SETUP;

	/*for objects inserted by user (subs & co), auto select*/
	if (odm->parentscene && odm->parentscene->is_dynamic_scene
	        && (odm->ID==GF_MEDIA_EXTERNAL_ID)
	        && (odm->flags & GF_ODM_REMOTE_OD)
	   ) {
		GF_Event evt;

		if (odm->addon) {
			Bool role_set = 0;

			if (odm->addon->addon_type >= GF_ADDON_TYPE_MAIN) return;

			//check role - for now look into URL, we need to inspect DASH roles
			if (odm->mo && odm->mo->URLs.count && odm->mo->URLs.vals[0].url) {
				char *sep = strchr(odm->mo->URLs.vals[0].url, '?');
				if (sep && strstr(sep, "role=main")) {
					odm->addon->addon_type = GF_ADDON_TYPE_MAIN;
					role_set = 1;
				}
			}

#if FILTER_FIXME
			if (!role_set) {
				GF_NetworkCommand com;
				memset(&com, 0, sizeof(GF_NetworkCommand));
				com.base.command_type = GF_NET_SERVICE_INFO;
				com.info.on_channel = gf_list_get(odm->channels, 0);
				gf_term_service_command(odm->net_service, &com);

				if (com.info.role && !strcmp(com.info.role, "main")) {
					odm->addon->addon_type = GF_ADDON_TYPE_MAIN;
				}
			}
#endif


			if (odm->addon->addon_type == GF_ADDON_TYPE_ADDITIONAL) {
				gf_scene_select_object(odm->parentscene, odm);
			}
			return;
		}

		if (!odm->parentscene) {
			evt.type = GF_EVENT_STREAMLIST;
			gf_sc_send_event(odm->parentscene->compositor, &evt);
			return;
		}
	}
}


#if FILTER_FIXME

void ODM_CheckChannelService(GF_Channel *ch)
{
	if (ch->service == ch->odm->net_service) return;
	/*if the stream has created a service check if close is needed or not*/
	if (ch->esd->URLString && !ch->service->nb_ch_users)
		gf_term_close_service(ch->odm->term, ch->service);
}

#endif

/*setup channel, clock and query caps*/
GF_EXPORT
GF_Err gf_odm_setup_pid(GF_ObjectManager *odm, GF_FilterPid *pid)
{
	GF_Clock *ck;
	GF_List *ck_namespace;
	s8 flag;
	u16 clockID;
	Bool emulated_od = 0;
	GF_Err e;
	Double dur;
	GF_Scene *scene;
	Bool clock_inherited = GF_TRUE;
	const GF_PropertyValue *prop;
	u32 OD_OCR_ID=0;
	u32 es_id=0;

	/*find the clock for this new channel*/
	ck = NULL;
	flag = (s8) -1;
	if (!pid) pid = odm->pid;

	if (odm->ck) {
		ck = odm->ck;
		goto clock_setup;
	}

	/*sync reference*/
	if (odm->sync_ref && odm->sync_ref->odm) {
		ck = odm->sync_ref->odm->ck;
		goto clock_setup;
	}
	/*timeline override*/
	if (odm->flags & GF_ODM_INHERIT_TIMELINE) {
		ck = odm->parentscene->root_od->ck;

		/**/
		if (!ck) {
			GF_ObjectManager *odm_par = odm->parentscene->root_od->parentscene->root_od;
			while (odm_par) {
				ck = odm_par->ck;

				if (ck) break;

				odm_par = (odm_par->parentscene && odm_par->parentscene->root_od->parentscene ) ? odm_par->parentscene->root_od->parentscene->root_od : NULL;
			}
		}
		if (ck)
			goto clock_setup;
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[ODM] Cannot inherit timeline from parent scene for scene %s - new clock will be created\n", odm->scene_ns->url));
	}

	/*get clocks namespace (eg, parent scene)*/
	scene = odm->subscene ? odm->subscene : odm->parentscene;
	if (!scene) return GF_BAD_PARAM;

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_CLOCK_ID);
	if (prop) OD_OCR_ID = prop->value.uint;

	ck_namespace = odm->scene_ns->Clocks;
#ifdef FILTER_FIXME
	odm->set_speed = odm->scene_ns->set_speed;
#endif

	/*little trick for non-OD addressing: if object is a remote one, and service owner already has clocks,
	override OCR. This will solve addressing like file.avi#audio and file.avi#video*/
	if (!OD_OCR_ID && (odm->flags & GF_ODM_REMOTE_OD) && (gf_list_count(ck_namespace)==1) ) {
		ck = (GF_Clock*)gf_list_get(ck_namespace, 0);
		OD_OCR_ID = ck->clockID;
	}
	/*for dynamic scene, force all streams to be sync on main OD stream (one timeline, no need to reload ressources)*/
	else if (odm->parentscene && odm->parentscene->is_dynamic_scene && !odm->subscene) {
		GF_ObjectManager *parent_od = odm->parentscene->root_od;
		if (parent_od->scene_ns && (gf_list_count(parent_od->scene_ns->Clocks)==1)) {
			ck = (GF_Clock*)gf_list_get(parent_od->scene_ns->Clocks, 0);
			if (!odm->ServiceID || (odm->ServiceID==ck->service_id)) {
				OD_OCR_ID = ck->clockID;
				goto clock_setup;
			}
		}
	}

	/*do we have an OCR specified*/
	clockID = OD_OCR_ID;
	/*if OCR stream force self-synchro !!*/
	if (odm->type == GF_STREAM_OCR) clockID = odm->ID;
	if (!clockID) {
		if (odm->ID == GF_MEDIA_EXTERNAL_ID) {
			clockID = (u32) odm->scene_ns;
		} else {
			clockID = odm->ID;
		}
	}

	/*override clock dependencies if specified*/
	if (scene->compositor->force_single_clock) {
		GF_Scene *parent = scene;
		while (parent->parent_scene) parent = parent->parent_scene;

		clockID = scene->root_od->ck->clockID;
		ck_namespace = parent->root_od->scene_ns->Clocks;
	}

	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	if (!prop) prop = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (prop) es_id = prop->value.uint;

	ck = gf_clock_attach(ck_namespace, scene, clockID, es_id, flag);
	if (!ck) return GF_OUT_OF_MEM;

	ck->service_id = odm->ServiceID;
	clock_inherited = GF_FALSE;

	if (scene->root_od->subscene && scene->root_od->subscene->is_dynamic_scene && !scene->root_od->ck)
		scene->root_od->ck = ck;

clock_setup:
	assert(ck);
	odm->ck = ck;
	odm->clock_inherited = clock_inherited;

	dur=0;
	prop = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (prop) {
		dur = 1000 * prop->value.frac.num;
		if (prop->value.frac.den) dur /= prop->value.frac.den;
	}
	if ((u32) dur > odm->duration) {
		odm->duration = (u32) dur;
		/*update scene duration*/
		gf_scene_set_duration(odm->subscene ? odm->subscene : odm->parentscene);
	}

#ifdef FILTER_FIXME
	/*setup scene decoder*/
	if (dec && dec->decio && (dec->decio->InterfaceType==GF_SCENE_DECODER_INTERFACE) ) {
		GF_SceneDecoder *sdec = (GF_SceneDecoder *) dec->decio;
		scene = odm->subscene ? odm->subscene : odm->parentscene;
		if (sdec->AttachScene) {
			/*if a node asked for this media object, use the scene graph of the node (AnimationStream in PROTO)*/
			if (odm->mo && odm->mo->node_ptr) {
				GF_SceneGraph *sg = scene->graph;
				/*FIXME - this MUST be cleaned up*/
				scene->graph = gf_node_get_graph((GF_Node*)odm->mo->node_ptr);
				sdec->AttachScene(sdec, scene, (scene->scene_codec==dec) ? 1: 0);
				scene->graph = sg;
				odm->mo->node_ptr = NULL;
			} else {
				sdec->AttachScene(sdec, scene, (scene->scene_codec==dec) ? 1: 0);
			}
		}
	}
#endif

	/*regular setup*/
	return GF_OK;
}

#if FILTER_FIXME

GF_Err gf_odm_post_es_setup(GF_Channel *ch, GF_Codec *dec, GF_Err had_err)
{
	char szURL[2048];
	GF_Err e;
	GF_NetworkCommand com;

	e = had_err;
	if (e) {
		ch->odm->pending_channels--;
		goto err_exit;
	}

	/*insert channel*/
	gf_list_insert(ch->odm->channels, ch, 0);

	if (ch->service) {
		ch->es_state = GF_ESM_ES_WAIT_FOR_ACK;
		if (ch->esd->URLString) {
			strcpy(szURL, ch->esd->URLString);
		} else {
			sprintf(szURL, "ES_ID=%u", ch->esd->ESID);
		}

		/*connect before setup: this is needed in case the decoder cfg is wrong, we may need to get it from
		network config...*/
		e = ch->service->ifce->ConnectChannel(ch->service->ifce, ch, szURL, ch->esd->decoderConfig->upstream);

		/*special case (not really specified in specs ...): if the stream is not found and is an Interaction
		one (ie, used by an InputSensor), consider this means the stream shall be generated by the IS device*/
		if ((e==GF_STREAM_NOT_FOUND) && (ch->esd->decoderConfig->streamType==GF_STREAM_INTERACT)) e = GF_OK;
	} else {
		ch->es_state = GF_ESM_ES_CONNECTED;
		ch->odm->pending_channels--;
	}

	if (e) {
		if (dec) gf_list_rem(ch->odm->channels, 0);
		goto err_exit;
	}
	/*add to decoder*/
	if (dec) {
		e = gf_codec_add_channel(dec, ch);
		if (e) {
			switch (ch->esd->decoderConfig->streamType) {
			case GF_STREAM_VISUAL:
				gf_term_message(ch->odm->term, ch->service->url, "Video Setup failed", e);
				break;
			case GF_STREAM_AUDIO:
				gf_term_message(ch->odm->term, ch->service->url, "Audio Setup failed", e);
				break;
			}
			gf_list_del_item(ch->odm->channels, ch);
			/*disconnect*/
			ch->service->ifce->DisconnectChannel(ch->service->ifce, ch);
			if (ch->esd->URLString) {
				assert(ch->service->nb_ch_users);
				ch->service->nb_ch_users--;
			}
			goto err_exit;
		}
	}

	/*in case a channel is inserted in a running OD, open and play if not in queue*/
	if (ch->odm->state==GF_ODM_STATE_PLAY) {

		gf_term_lock_media_queue(ch->odm->term, 1);
		gf_list_del_item(ch->odm->term->media_queue, ch->odm);
		gf_term_lock_media_queue(ch->odm->term, 1);


		gf_term_lock_net(ch->odm->term, 1);
		gf_es_start(ch);
		memset(&com, 0, sizeof(GF_NetworkCommand));
		com.command_type = GF_NET_CHAN_PLAY;
		com.base.on_channel = ch;
		com.play.speed = FIX2FLT(ch->clock->speed);
		com.play.start_range = gf_clock_time(ch->clock);
		com.play.start_range /= 1000;
		com.play.end_range = 0;
		gf_term_service_command(ch->service, &com);
		if (dec && (dec->Status!=GF_ESM_CODEC_PLAY)) gf_term_start_codec(dec, 0);
		gf_term_lock_net(ch->odm->term, 0);
	}
	return GF_OK;

err_exit:
	ODM_CheckChannelService(ch);
	gf_es_del(ch);
	return e;
}

/*confirmation of channel delete from net*/
void ODM_DeleteChannel(GF_ObjectManager *odm, GF_Channel *ch)
{
	u32 i, count, ch_pos;
	GF_Channel *ch2;
	GF_Clock *ck;

	if (!ch) return;
	assert( odm );
	assert( odm->channels );

	//find a clock with this stream ES_ID
	ck = gf_clock_find(odm->net_service->Clocks, ch->esd->ESID, 0);

	count = gf_list_count(odm->channels);
	ch_pos = count+1;

	for (i=0; i<count; i++) {
		ch2 = (GF_Channel*)gf_list_get(odm->channels, i);
		if (ch2 == ch) {
			ch_pos = i;
			if (ck) continue;
			break;
		}
		//note that when a stream is added, we need to update clocks info ...
		if (ck && ch->clock && (ch2->clock->clockID == ck->clockID)) gf_es_stop(ch2);
	}
	/*remove channel*/
	if (ch_pos != count+1) gf_list_rem(odm->channels, ch_pos);

	/*remove from the codec*/
	count = 0;
	if (!count && odm->codec)
		count = gf_codec_remove_channel(odm->codec, ch);
	if (!count && odm->ocr_codec)
		count = gf_codec_remove_channel(odm->ocr_codec, ch);
#ifndef GPAC_MINIMAL_ODF
	if (!count && odm->oci_codec)
		count = gf_codec_remove_channel(odm->oci_codec, ch);
#endif
	if (!count && odm->subscene) {
		if (odm->subscene->scene_codec) count = gf_codec_remove_channel(odm->subscene->scene_codec, ch);
		if (!count) /*count = */gf_codec_remove_channel(odm->subscene->od_codec, ch);
	}
	if (ch->service) {
		ch->service->ifce->DisconnectChannel(ch->service->ifce, ch);
		if (ch->esd->URLString) {
			assert(ch->service->nb_ch_users);
			ch->service->nb_ch_users--;
		}
		ODM_CheckChannelService(ch);
	}

	//and delete
	gf_es_del(ch);
}

GF_EXPORT
void gf_odm_remove_es(GF_ObjectManager *odm, u16 ES_ID)
{
	GF_ESD *esd;
	GF_Channel *ch;
	u32 i = 0;
	while ((esd = (GF_ESD *)gf_list_enum(odm->OD->ESDescriptors, &i)) ) {
		if (esd->ESID==ES_ID) goto esd_found;
	}
	return;

esd_found:
	/*remove esd*/
	gf_list_rem(odm->OD->ESDescriptors, i-1);
	/*locate channel*/
	ch = NULL;
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i)) ) {
		if (ch->esd->ESID == ES_ID) break;
		ch = NULL;
	}
	/*remove channel*/
	if (ch) ODM_DeleteChannel(odm, ch);
	/*destroy ESD*/
	gf_odf_desc_del((GF_Descriptor *) esd);
}

#endif

/*this is the tricky part: make sure the net is locked before doing anything since an async service
reply could destroy the object we're queuing for play*/
void gf_odm_start(GF_ObjectManager *odm)
{
	/*object is not started - issue channel setup requests*/
	if (!odm->state) {
		/*look for a given segment name to play*/
		if (odm->subscene) {
			char *url, *frag=NULL;
			assert(odm->subscene->root_od==odm);

			url = (odm->mo && odm->mo->URLs.count) ? odm->mo->URLs.vals[0].url : odm->scene_ns->url;
			frag = strrchr(url, '#');

			if (frag) {
				GF_Segment *seg = gf_odm_find_segment(odm, frag+1);
				if (seg) {
					odm->media_start_time = (u64) ((s64) seg->startTime*1000);
					odm->media_stop_time =  (u64) ((s64) (seg->startTime + seg->Duration)*1000);
				}
			}
		}
	}
	gf_odm_play(odm);

}

void gf_odm_play(GF_ObjectManager *odm)
{
	u32 i;
	u64 range_end;
	Bool skip_od_st;
	Bool media_control_paused = 0;
	Bool start_range_is_clock = 0;
	Double ck_time;
	GF_Clock *clock = odm->ck;
	GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;
#endif
	GF_FilterEvent com;
	GF_Clock *parent_ck = NULL;

	if (odm->mo && odm->mo->pck && !(odm->flags & GF_ODM_PREFETCH)) {
		/*reset*/
		gf_filter_pck_unref(odm->mo->pck);
		odm->mo->pck = NULL;
	}
	odm->flags &= ~GF_ODM_PREFETCH;

	if (odm->parentscene) {
		parent_ck = gf_odm_get_media_clock(odm->parentscene->root_od);
		if (!gf_odm_shares_clock(odm, parent_ck)) parent_ck = NULL;
	}

	//PID not yet attached, mark as state_play and wait for error or OK
	if (!odm->pid ) {
		odm->state = GF_ODM_STATE_PLAY;
		return;
	}

	skip_od_st = (odm->subscene && odm->subscene->static_media_ressources) ? 1 : 0;
	range_end = odm->media_stop_time;
//	odm->media_stop_time = 0;

	/*send play command*/
	GF_FEVT_INIT(com, GF_FEVT_PLAY, odm->pid)

	if (odm->flags & GF_ODM_INITIAL_BROADCAST_PLAY) {
		odm->flags &= ~GF_ODM_INITIAL_BROADCAST_PLAY;
		com.play.initial_broadcast_play = 1;
	}


	/*play from requested time (seeking or non-mpeg4 media control)*/
	if (odm->media_start_time && !clock->clock_init) {
		ck_time = (Double) (s64) odm->media_start_time;
		ck_time /= 1000;
	}
	else if (odm->parentscene && odm->parentscene->root_od->media_start_time && !clock->clock_init) {
		ck_time = (Double) (s64) odm->parentscene->root_od->media_start_time;
		ck_time /= 1000;
	}
	/*play from current time*/
	else {
		ck_time = gf_clock_media_time(clock);
		ck_time /= 1000;
		start_range_is_clock = 1;
	}

	/*handle initial start - MPEG-4 is a bit annoying here, streams are not started through OD but through
	scene nodes. If the stream runs on the BIFS/OD clock, the clock is already started at this point and we're
	sure to get at least a one-frame delay in PLAY, so just remove it - note we're generous but this shouldn't hurt*/
	if (ck_time<=0.5) ck_time = 0;


	/*adjust time for addons*/
	if (odm->parentscene && odm->parentscene->root_od->addon) {
		com.play.initial_broadcast_play = 0;
		//addon timing is resolved against timestamps, not media time
		if (start_range_is_clock) {
			if (!gf_clock_is_started(clock)) {
				ck_time = (Double) odm->parentscene->root_od->addon->media_pts;
				ck_time /= 90000;
			} else {
				ck_time = gf_clock_time(clock);
				ck_time /= 1000;
			}
		}
		ck_time = gf_scene_adjust_time_for_addon(odm->parentscene->root_od->addon, ck_time, &com.play.timestamp_based);
		//we are having a play request for an addon without the main content being active - we no longer have timestamp info from the main content
		if (!clock->clock_init && com.play.timestamp_based)
			com.play.timestamp_based = 2;

		if (ck_time<0)
			ck_time=0;

		if (odm->scalable_addon) {
			//this is a scalable extension to an object in the parent scene
			gf_scene_select_scalable_addon(odm->parentscene->root_od->parentscene, odm);
		}
	}

	com.play.start_range = ck_time;

	if (range_end) {
		com.play.end_range = (s64) range_end / 1000.0;
	} else {
		if (!odm->subscene && odm->parentscene && gf_odm_shares_clock(odm->parentscene->root_od, clock)
				&& (odm->parentscene->root_od->media_stop_time != odm->parentscene->root_od->duration)
		) {
			com.play.end_range = (s64) odm->parentscene->root_od->media_stop_time / 1000.0;
		} else {
			com.play.end_range = 0;
		}
	}

	com.play.speed = clock->speed;

#ifndef GPAC_DISABLE_VRML
	ctrl = parent_ck ? parent_ck->mc : gf_odm_get_mediacontrol(odm);
	/*override range and speed with MC*/
	if (ctrl) {
		//for addon, use current clock settings (media control is ignored)
		if (!odm->parentscene || !odm->parentscene->root_od->addon) {
			//this is fake timeshift, eg we are playing a VoD as a timeshift service: stop and start times have already been adjusted
			if (ctrl->control->mediaStopTime<0 && !odm->timeshift_depth) {
			} else {
				MC_GetRange(ctrl, &com.play.start_range, &com.play.end_range);
			}
		}

		com.play.speed = FIX2FLT(ctrl->control->mediaSpeed);
		/*if the channel doesn't control the clock, jump to current time in the controled range, not just the beginning*/
		if ((ctrl->stream != odm) && (ck_time>com.play.start_range) && (com.play.end_range>com.play.start_range)
		&& (ck_time<com.play.end_range)) {
			com.play.start_range = ck_time;
		}
		if (ctrl->paused) media_control_paused = 1;

		gf_clock_set_speed(clock, ctrl->control->mediaSpeed);
		if (odm->mo) odm->mo->speed = ctrl->control->mediaSpeed;

#if 0
		/*if requested seek time AND media control, adjust start range to current play time*/
		if ((com.play.speed>=0) && odm->media_start_time) {
			if ((com.play.start_range>=0) && (com.play.end_range>com.play.start_range)) {
				if (ctrl->control->loop) {
					Double active_dur = com.play.end_range - com.play.start_range;
					while (ck_time>active_dur) ck_time -= active_dur;
				} else {
					ck_time = 0;
					//com.play.start_range = com.play.end_range;
				}
			}
			com.play.start_range += ck_time;
		}
#endif

	}
#endif //GPAC_DISABLE_VRML

	/*full object playback*/
	if (com.play.end_range<=0) {
		if (com.play.speed<0) {
			odm->media_stop_time = 0;
		} else {
			odm->media_stop_time = odm->subscene ? 0 : odm->duration;
		}
	} else {
		/*segment playback - since our timing is in ms whereas segment ranges are double precision,
		make sure we have a LARGER range in ms, otherwise media sensors won't deactivate properly*/
		odm->media_stop_time = (u64) ceil(1000 * com.play.end_range);
	}

	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d %s] PID %s: At OTB %u requesting PLAY from %g to %g (clock init %d) - speed %g\n", odm->ID, odm->scene_ns->url, gf_filter_pid_get_name(odm->pid), gf_clock_time(clock), com.play.start_range, com.play.end_range, clock->clock_init, com.play.speed));


	if (odm->state != GF_ODM_STATE_PLAY) {
		odm->state = GF_ODM_STATE_PLAY;
		odm->nb_buffering ++;
		//start buffering
		gf_clock_buffer_on(odm->ck);
		odm->has_seen_eos = GF_FALSE;

		if (gf_list_find(scene->compositor->systems_pids, odm->pid)<0)
			gf_list_add(scene->compositor->systems_pids, odm->pid);

		gf_filter_pid_send_event(odm->pid, &com);
	}
	if (odm->extra_pids) {
		u32 k, count = gf_list_count(odm->extra_pids);
		for (k=0; k<count; k++) {
			GF_ODMExtraPid *xpid = gf_list_get(odm->extra_pids, k);
			if (xpid->state != GF_ODM_STATE_PLAY) {
				xpid->state = GF_ODM_STATE_PLAY;
				com.base.on_pid = xpid->pid;
				odm->has_seen_eos = GF_FALSE;
				odm->nb_buffering ++;
				//start buffering
				gf_clock_buffer_on(odm->ck);

				if (gf_list_find(scene->compositor->systems_pids, xpid->pid)<0)
					gf_list_add(scene->compositor->systems_pids, xpid->pid);

				gf_filter_pid_send_event(xpid->pid, &com);
			}
		}
	}

//	odm->media_start_time = 0;

	if (odm->parentscene) {
		if (odm->parentscene->root_od->addon) {
			odm->parentscene->root_od->addon->started = 1;
		}
		if (odm->parentscene->first_frame_pause_type) {
			media_control_paused = GF_TRUE;
		}
	} else if (odm->subscene && odm->subscene->first_frame_pause_type) {
		media_control_paused = GF_TRUE;
	}

#if FILTER_FIXME

	gf_term_service_media_event(odm, GF_EVENT_MEDIA_LOAD_START);
	gf_term_service_media_event(odm, GF_EVENT_MEDIA_TIME_UPDATE);


	/*start codecs last (otherwise we end up pulling data from channels not yet connected->pbs when seeking)*/
	if (odm->codec) {
		gf_term_start_codec(odm->codec, 0);
	} else if (odm->subscene) {
		if (odm->subscene->scene_codec) gf_term_start_codec(odm->subscene->scene_codec, 0);
		if (!skip_od_st && odm->subscene->od_codec) gf_term_start_codec(odm->subscene->od_codec, 0);

		if (odm->flags & GF_ODM_REGENERATE_SCENE) {
			odm->flags &= ~GF_ODM_REGENERATE_SCENE;
			if (!odm->subscene->graph_attached)
				gf_scene_regenerate(odm->subscene);
			else
				gf_scene_restart_dynamic(odm->subscene, 0, 1, 0);
		}
	}
	if (odm->ocr_codec) gf_term_start_codec(odm->ocr_codec, 0);
#ifndef GPAC_MINIMAL_ODF
	if (odm->oci_codec) gf_term_start_codec(odm->oci_codec, 0);
#endif

#endif

	if (odm->flags & GF_ODM_PAUSE_QUEUED) {
		odm->flags &= ~GF_ODM_PAUSE_QUEUED;
		media_control_paused = 1;
	}

	if (media_control_paused) {
		gf_odm_pause(odm);
	}
}

Bool gf_odm_owns_clock(GF_ObjectManager *odm, GF_Clock *ck)
{
	u32 i, j;
	GF_ObjectManager *od;
	GF_ODMExtraPid *xpid;

//	if (odm == odm->ck->odm) return GF_TRUE;
	return GF_FALSE;
}

void gf_odm_stop(GF_ObjectManager *odm, Bool force_close)
{
	u32 i, count;
	GF_ODMExtraPid *xpid;
#ifndef GPAC_DISABLE_VRML
	MediaControlStack *ctrl;
	MediaSensorStack *media_sens;
#endif
	GF_Scene *scene = odm->subscene ? odm->subscene : odm->parentscene;
	GF_FilterEvent com;

	//root ODs of dynamic scene may not have seen play/pause request
	if (!odm->state && !odm->scalable_addon && (!odm->subscene || !odm->subscene->is_dynamic_scene) ) return;

	//PID not yet attached, mark as state_stop and wait for error or OK
	if (!odm->pid ) {
		odm->state = GF_ODM_STATE_STOP;
		return;
	}


	if (force_close && odm->mo) odm->mo->flags |= GF_MO_DISPLAY_REMOVE;
	/*stop codecs*/
	if (odm->subscene) {
		u32 i=0;
		GF_ObjectManager *sub_odm;

		/*stops all resources of the subscene as well*/
		while ((sub_odm=(GF_ObjectManager *)gf_list_enum(odm->subscene->resources, &i))) {
			gf_odm_stop(sub_odm, force_close);
		}
	}

	/*send stop command*/
	odm->has_seen_eos = GF_FALSE;
	odm->state = GF_ODM_STATE_STOP;
	GF_FEVT_INIT(com, GF_FEVT_STOP, odm->pid)
	GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d %s] PID %s At OTB %u requesting STOP\n", odm->ID, odm->scene_ns->url, gf_filter_pid_get_name(odm->pid), gf_clock_time(odm->ck) ));

	gf_filter_pid_send_event(odm->pid, &com);
	gf_list_del_item(scene->compositor->systems_pids, odm->pid);

	i=0;
	while ((xpid = (GF_ODMExtraPid*)gf_list_enum(odm->extra_pids, &i)) ) {
		xpid->has_seen_eos = GF_FALSE;
		xpid->state = GF_ODM_STATE_STOP;
		com.base.on_pid = xpid->pid;
		gf_list_del_item(scene->compositor->systems_pids, xpid->pid);
		gf_filter_pid_send_event(xpid->pid, &com);
	}

	if (odm->parentscene && odm->parentscene->root_od->addon) {
		odm->parentscene->root_od->addon->started = 0;
	}

#if FILTER_FIXME
	gf_term_service_media_event(odm, GF_EVENT_ABORT);
#endif

	/*stops clock if this is a scene stop*/
	if (!(odm->flags & GF_ODM_INHERIT_TIMELINE) && odm->subscene && gf_odm_owns_clock(odm, odm->ck) ) {
		gf_clock_stop(odm->ck);
	}
	odm->media_current_time = 0;

#ifndef GPAC_DISABLE_VRML
	/*reset media sensor(s)*/
	i = 0;
	while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i))) {
		MS_Stop(media_sens);
	}
	/*reset media control state*/
	ctrl = gf_odm_get_mediacontrol(odm);
	if (ctrl) ctrl->current_seg = 0;
#endif

}
void gf_odm_on_eos(GF_ObjectManager *odm, GF_FilterPid *pid)
{
	u32 i, count, nb_eos, nb_share_clock, nb_ck_running;
	Bool all_done = GF_TRUE;
#ifndef GPAC_DISABLE_VRML
	if (gf_odm_check_segment_switch(odm)) return;
#endif

	nb_share_clock=0;
	nb_eos = odm->has_seen_eos ? 1 : 0;
	nb_ck_running = 0;


	if (odm->pid==pid) {
		odm->has_seen_eos = GF_TRUE;
	}
	if (!odm->has_seen_eos) all_done = GF_FALSE;

	count = gf_list_count(odm->extra_pids);
	for (i=0; i<count; i++) {
		GF_ODMExtraPid *xpid = gf_list_get(odm->extra_pids, i);
		if (xpid->pid == pid) {
			xpid->has_seen_eos = GF_TRUE;
		}
		if (!xpid->has_seen_eos) all_done = GF_FALSE;
	}
	if (all_done && !odm->ck->has_seen_eos) {
		odm->ck->has_seen_eos = 1;
#ifndef GPAC_DISABLE_VRML
		//check for scene restart upon end of stream
		if (odm->subscene) {
			gf_scene_mpeg4_inline_check_restart(odm->subscene);
		}
#endif
	}

#if FILTER_FIXME
	gf_term_service_media_event(odm, GF_EVENT_MEDIA_LOAD_DONE);
#endif

}




#if FILTER_FIXME
void gf_odm_set_timeshift_depth(GF_ObjectManager *odm, GF_Channel *ch, u32 stream_timeshift)
{
	if (odm->codec) {
		if (ch->esd->decoderConfig->streamType == odm->codec->type)
			if (odm->timeshift_depth != stream_timeshift)
				odm->timeshift_depth = stream_timeshift;
	} else if (odm->ocr_codec) {
		if (ch->esd->decoderConfig->streamType == odm->ocr_codec->type)
			if (odm->timeshift_depth != stream_timeshift)
				odm->timeshift_depth = stream_timeshift;
	} else if (odm->subscene && odm->subscene->scene_codec) {
		//if (gf_list_find(odm->subscene->scene_codec->inChannels, ch) >= 0) {
		if (odm->timeshift_depth != stream_timeshift)
			odm->timeshift_depth = stream_timeshift;
		//}
	}

	/*update scene duration*/
	gf_scene_set_timeshift_depth(odm->subscene ? odm->subscene : (odm->parentscene ? odm->parentscene : odm->term->root_scene));
}
#endif


GF_Clock *gf_odm_get_media_clock(GF_ObjectManager *odm)
{
	while (odm->lower_layer_odm) {
		odm = odm->lower_layer_odm;
	}
	if (odm->ck) return odm->ck;
	return NULL;
}


Bool gf_odm_shares_clock(GF_ObjectManager *odm, GF_Clock *ck)
{
	if (odm->ck == ck) return GF_TRUE;
	return GF_FALSE;
}


void gf_odm_pause(GF_ObjectManager *odm)
{
	u32 i;
#ifndef GPAC_DISABLE_VRML
	MediaSensorStack *media_sens;
#endif
#if FILTER_FIXME
	GF_NetworkCommand com;

	//postpone until the PLAY request
	if (odm->state != GF_ODM_STATE_PLAY) {
		odm->flags |= GF_ODM_PAUSE_QUEUED;
		return;
	}

	if (odm->flags & GF_ODM_PAUSED) return;
	odm->flags |= GF_ODM_PAUSED;

	//cleanup - we need to enter in stop state for broadcast modes
	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;

	com.command_type = GF_NET_CHAN_PAUSE;
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
		gf_clock_pause(ch->clock);
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d %s] CH%d: At OTB %u requesting PAUSE (clock init %d)\n", odm->ID, odm->net_service->url, ch->esd->ESID, gf_clock_time(ch->clock), ch->clock->clock_init ));

		if (odm->state != GF_ODM_STATE_PLAY) continue;

		//if we are in dump mode, the clocks are paused (step-by-step render), but we don't send the pause commands to
		//the network !
		if (odm->term->root_scene->first_frame_pause_type!=2) {
			com.base.on_channel = ch;
			gf_term_service_command(ch->service, &com);
		}
	}
#endif

	//if we are in dump mode, only the clocks are paused (step-by-step render), the media object is still in play state
	if (odm->parentscene->first_frame_pause_type==2) {
		return;
	}

#ifndef GPAC_DISABLE_VRML
	/*mediaSensor  shall generate isActive false when paused*/
	i=0;
	while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i)) ) {
		if (media_sens && media_sens->sensor->isActive) {
			media_sens->sensor->isActive = 0;
			gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);
		}
	}
#endif

}

void gf_odm_resume(GF_ObjectManager *odm)
{
	u32 i;

#ifndef GPAC_DISABLE_VRML
	MediaSensorStack *media_sens;
	MediaControlStack *ctrl;
#endif

#if FILTER_FIXME
	GF_NetworkCommand com;

	if (odm->flags & GF_ODM_PAUSE_QUEUED) {
		odm->flags &= ~GF_ODM_PAUSE_QUEUED;
		return;
	}

	if (!(odm->flags & GF_ODM_PAUSED)) return;
	odm->flags &= ~GF_ODM_PAUSED;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;

#ifndef GPAC_DISABLE_VRML
	ctrl = gf_odm_get_mediacontrol(odm);
#endif

	com.command_type = GF_NET_CHAN_RESUME;
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i)) ) {
		gf_clock_resume(ch->clock);

		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d %s] CH%d: At OTB %u requesting RESUME (clock init %d)\n", odm->ID, odm->net_service->url, ch->esd->ESID, gf_clock_time(ch->clock), ch->clock->clock_init ));
		if (odm->state!= GF_ODM_STATE_PLAY) continue;

		com.base.on_channel = ch;
		gf_term_service_command(ch->service, &com);

#ifndef GPAC_DISABLE_VRML
		/*override speed with MC*/
		if (ctrl) {
			gf_clock_set_speed(ch->clock, ctrl->control->mediaSpeed);
		}
#endif
	}

#endif


#ifndef GPAC_DISABLE_VRML
	/*mediaSensor shall generate isActive TRUE when resumed*/
	i=0;
	while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i)) ) {
		if (media_sens && !media_sens->sensor->isActive) {
			media_sens->sensor->isActive = 1;
			gf_node_event_out((GF_Node *) media_sens->sensor, 4/*"isActive"*/);
		}
	}
#endif
}

void gf_odm_set_speed(GF_ObjectManager *odm, Fixed speed, Bool adjust_clock_speed)
{
	u32 i;
#if FILTER_FIXME
	GF_NetworkCommand com;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;

	com.command_type = GF_NET_CHAN_SET_SPEED;
	com.play.speed = FIX2FLT(speed);
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i)) ) {
		if (adjust_clock_speed)
			gf_clock_set_speed(ch->clock, speed);

		com.play.on_channel = ch;
		gf_term_service_command(ch->service, &com);
	}
#endif
}

GF_Segment *gf_odm_find_segment(GF_ObjectManager *odm, char *descName)
{
#if FILTER_FIXME
	GF_Segment *desc;
	u32 i = 0;
	if (!odm->OD) return NULL;
	while ( (desc = (GF_Segment *)gf_list_enum(odm->OD->OCIDescriptors, &i)) ) {
		if (desc->tag != GF_ODF_SEGMENT_TAG) continue;
		if (!stricmp(desc->SegmentName, descName)) return desc;
	}
#endif
	return NULL;
}

static void gf_odm_insert_segment(GF_ObjectManager *odm, GF_Segment *seg, GF_List *list)
{
	/*this reorders segments when inserting into list - I believe this is not compliant*/
#if 0
	GF_Segment *desc;
	u32 i = 0;
	while ((desc = gf_list_enum(list, &i))) {
		if (desc == seg) return;
		if (seg->startTime + seg->Duration <= desc->startTime) {
			gf_list_insert(list, seg, i);
			return;
		}
	}
#endif
	gf_list_add(list, seg);
}

/*add segment descriptor and sort them*/
void gf_odm_init_segments(GF_ObjectManager *odm, GF_List *list, MFURL *url)
{
#if FILTER_FIXME
	char *str, *sep;
	char seg1[1024], seg2[1024], seg_url[4096];
	GF_Segment *first_seg, *last_seg, *seg;
	u32 i, j;

	/*browse all URLs*/
	for (i=0; i<url->count; i++) {
		if (!url->vals[i].url) continue;
		str = strstr(url->vals[i].url, "#");
		if (!str) continue;
		str++;
		strcpy(seg_url, str);
		/*segment closed range*/
		if ((sep = strstr(seg_url, "-")) ) {
			strcpy(seg2, sep+1);
			sep[0] = 0;
			strcpy(seg1, seg_url);
			first_seg = gf_odm_find_segment(odm, seg1);
			if (!first_seg) continue;
			last_seg = gf_odm_find_segment(odm, seg2);
		}
		/*segment open range*/
		else if ((sep = strstr(seg_url, "+")) ) {
			sep[0] = 0;
			strcpy(seg1, seg_url);
			first_seg = gf_odm_find_segment(odm, seg_url);
			if (!first_seg) continue;
			last_seg = NULL;
		}
		/*single segment*/
		else {
			first_seg = gf_odm_find_segment(odm, seg_url);
			if (!first_seg) continue;
			gf_odm_insert_segment(odm, first_seg, list);
			continue;
		}
		/*segment range process*/
		gf_odm_insert_segment(odm, first_seg, list);
		j=0;
		while ( (seg = (GF_Segment *)gf_list_enum(odm->OD->OCIDescriptors, &j)) ) {
			if (seg->tag != GF_ODF_SEGMENT_TAG) continue;
			if (seg==first_seg) continue;
			if (seg->startTime + seg->Duration <= first_seg->startTime) continue;
			/*this also includes last_seg insertion !!*/
			if (last_seg && (seg->startTime + seg->Duration > last_seg->startTime + last_seg->Duration) ) continue;
			gf_odm_insert_segment(odm, seg, list);
		}
	}
#endif
}

Bool gf_scene_is_root(GF_Scene *scene)
{
	GF_Scene *s = scene;
	while (s->parent_scene) s=s->parent_scene;
	return (s==scene) ? GF_TRUE : GF_FALSE;
}

GF_Scene *gf_scene_get_root(GF_Scene *scene)
{
	GF_Scene *s = scene;
	while (scene->parent_scene) scene = scene->parent_scene;
	return scene;
}

void gf_odm_signal_eos(GF_ObjectManager *odm)
{
#if FILTER_FIXME
	if (odm->parentscene && !gf_scene_is_root(odm->parentscene) ) {
		GF_ObjectManager *root = odm->parentscene->root_od;
		Bool is_over = 0;

		if (!gf_scene_check_clocks(root->net_service, root->subscene, 0)) return;
		if (root->subscene->is_dynamic_scene)
			is_over = 1;
		else
			is_over = gf_sc_is_over(odm->term->compositor, root->subscene->graph);

		if (is_over) {
			gf_term_service_media_event(root, GF_EVENT_MEDIA_ENDED);
		}
	} else {
		if (gf_term_check_end_of_scene(odm->term, 0)) {
			GF_Event evt;
			evt.type = GF_EVENT_EOS;
			gf_sc_send_event(odm->parentscene->compositor, &evt);
		}
	}
#endif
}


Bool gf_odm_check_buffering(GF_ObjectManager *odm, GF_FilterPid *pid)
{
	u32 timescale;

	assert(odm);
	if (!pid)
		pid = odm->pid;
	if (odm->nb_buffering) {
		u64 buffer_duration = gf_filter_pid_query_buffer_duration(pid);
		if ( ! odm->ck->clock_init) {
			u64 time;
			GF_FilterPacket *pck = gf_filter_pid_get_packet(pid);
			if (!pck) return GF_TRUE;
			timescale = gf_filter_pck_get_timescale(pck);

			time = gf_filter_pck_get_cts(pck);
			if (!time) time = gf_filter_pck_get_dts(pck);
			time *= 1000;
			time /= timescale;
			gf_clock_set_time(odm->ck, time);
//			gf_clock_set_speed(odm->ck, 4.0);
		}
		if (buffer_duration>200000) {
			odm->nb_buffering --;
			gf_clock_buffer_off(odm->ck);
		} else if (gf_filter_pid_has_seen_eos(pid) ) {
			odm->nb_buffering --;
			gf_clock_buffer_off(odm->ck);
		}

	}
	return odm->nb_buffering ? GF_TRUE : GF_FALSE;
}
