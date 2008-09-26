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
	tmp->channels = gf_list_new();

	tmp->Audio_PL = (u8) -1;
	tmp->Graphics_PL = (u8) -1;
	tmp->OD_PL = (u8) -1;
	tmp->Scene_PL = (u8) -1;
	tmp->Visual_PL = (u8) -1;
	tmp->ms_stack = gf_list_new();
	tmp->mc_stack = gf_list_new();
	tmp->mx = gf_mx_new("ODM");
	return tmp;
}

void gf_odm_del(GF_ObjectManager *odm)
{
	Bool lock;
	u32 i;
	MediaSensorStack *media_sens;

	/*make sure we are not in the media queue*/
	gf_mx_p(odm->term->net_mx);
	gf_list_del_item(odm->term->media_queue, odm);
	gf_mx_v(odm->term->net_mx);

	lock = gf_mx_try_lock(odm->mx);
	i=0;
	while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i))) {
		MS_Stop(media_sens);
		/*and detach from stream object*/
		media_sens->is_init = 0;
	}
	if (odm->mo) odm->mo->odm = NULL;


	gf_list_del(odm->channels);
	gf_list_del(odm->ms_stack);
	gf_list_del(odm->mc_stack);
	gf_odf_desc_del((GF_Descriptor *)odm->OD);
	assert (!odm->net_service);
	if (lock) gf_mx_v(odm->mx);
	gf_mx_del(odm->mx);
	free(odm);
}


void gf_odm_lock(GF_ObjectManager *odm, u32 LockIt)
{
	assert(odm);
	if (LockIt) 
		gf_mx_p(odm->mx);
	else
		gf_mx_v(odm->mx);
}

Bool gf_odm_lock_mo(GF_MediaObject *mo)
{
	if (!mo || !mo->odm) return 0;
	gf_odm_lock(mo->odm, 1);
	/*the ODM may have been destroyed here !!*/
	if (!mo->odm) return 0;
	return 1;
}

GF_EXPORT
void gf_odm_disconnect(GF_ObjectManager *odm, Bool do_remove)
{
	GF_Channel *ch;

	gf_odm_stop(odm, 1);

	/*disconnect sub-scene*/
	if (odm->subscene) gf_inline_disconnect(odm->subscene, do_remove);

	/*no destroy*/
	if (!do_remove) return;

	gf_odm_lock(odm, 1);

	/*unload the decoders before deleting the channels to prevent any access fault*/
	if (odm->codec) gf_term_remove_codec(odm->term, odm->codec);
	if (odm->ocr_codec) gf_term_remove_codec(odm->term, odm->ocr_codec);
	if (odm->oci_codec) gf_term_remove_codec(odm->term, odm->oci_codec);

	/*then delete all the channels in this OD */
	while (gf_list_count(odm->channels)) {
		ch = (GF_Channel*)gf_list_get(odm->channels, 0);
#if 0
		if (ch->clock->mc && ch->clock->mc->stream && ch->clock->mc->stream->odm==odm) {
			ch->clock->mc->stream = NULL;
			ch->clock->mc = NULL;
		}
#endif
		ODM_DeleteChannel(odm, ch);
	}

	/*delete the decoders*/
	if (odm->codec) {
		gf_codec_del(odm->codec);
		odm->codec = NULL;
	}
	if (odm->ocr_codec) {
		gf_codec_del(odm->ocr_codec);
		odm->ocr_codec = NULL;
	}
	if (odm->oci_codec) {
		gf_codec_del(odm->oci_codec);
		odm->oci_codec = NULL;
	}

	/*detach from network service */
	if (odm->net_service) {
		GF_ClientService *ns = odm->net_service;
		if (ns->owner == odm) {
			if (ns->nb_odm_users) ns->nb_odm_users--;
			/*detach it!!*/
			ns->owner = NULL;
			/*try to assign a new root in case this is not scene shutdown*/
			if (ns->nb_odm_users && odm->parentscene) {
				GF_ObjectManager *new_root;
				u32 i = 0;
				while ((new_root = (GF_ObjectManager *)gf_list_enum(odm->parentscene->ODlist, &i)) ) {
					if (new_root == odm) continue;
					if (new_root->net_service != ns) continue;
					ns->owner = new_root;
					break;
				}
			}
		}
		odm->net_service = NULL;
		if (!ns->nb_odm_users) gf_term_close_services(odm->term, ns);
	}

	gf_odm_lock(odm, 0);

	/*delete from the parent scene.*/
	if (odm->parentscene) {
		gf_inline_remove_object(odm->parentscene, odm, do_remove);
		if (odm->subscene) gf_inline_del(odm->subscene);
		gf_odm_del(odm);
		return;
	}
	
	/*this is the scene root OD (may be a remote OD ..) */
	if (odm->term->root_scene) {
		GF_Event evt;
		assert(odm->term->root_scene == odm->subscene);
		gf_inline_del(odm->subscene);
		/*reset main pointer*/
		odm->term->root_scene = NULL;

		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = 0;
		GF_USER_SENDEVENT(odm->term->user, &evt);
	}

	/*delete the ODMan*/
	gf_odm_del(odm);
}

/*setup service for OD (extract IOD and go)*/
void gf_odm_setup_entry_point(GF_ObjectManager *odm, const char *service_sub_url)
{
	u32 od_type;
	char *ext;
	char *sub_url = (char *) service_sub_url;
	GF_Terminal *term;
	GF_Descriptor *desc;
	GF_IPMP_ToolList *toolList;

//	assert(odm->OD==NULL);

	term = odm->term;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM] Setting up root object for %s\n", odm->net_service->url));

	if (odm->subscene) od_type = GF_MEDIA_OBJECT_SCENE;
	else if (odm->mo) {
		od_type = odm->mo->type;
		if (!sub_url && odm->mo->URLs.count && odm->mo->URLs.vals[0].url) {
			sub_url = odm->mo->URLs.vals[0].url;
		}
	}
	else od_type = GF_MEDIA_OBJECT_UNDEF;

	/*for remote ODs, get expected OD type in case the service needs to generate the IOD on the fly*/
	if (odm->parentscene && odm->OD && odm->OD->URLString) {
		GF_MediaObject *mo;
		mo = gf_inline_find_object(odm->parentscene, odm->OD->objectDescriptorID, odm->OD->URLString);
		if (mo) od_type = mo->type;
		ext = strchr(odm->OD->URLString, '#');
		if (ext) sub_url = ext;
	}

	desc = odm->net_service->ifce->GetServiceDescriptor(odm->net_service->ifce, od_type, sub_url); 
	if (odm->OD) return;

	if (!desc) {
		/*if desc is NULL for a media, the media will be declared later by the service (gf_term_media_add)*/
		if (od_type != GF_MEDIA_OBJECT_SCENE)
			return;
		/*create empty service descriptor, this will automatically create a dynamic scene*/
		desc = gf_odf_desc_new(GF_ODF_OD_TAG);
	}
	odm->net_service->nb_odm_users++;

	if (!gf_list_count( ((GF_ObjectDescriptor*)desc)->ESDescriptors)) {
		/*new subscene*/
		if (!odm->subscene) {
			assert(odm->parentscene);
			odm->subscene = gf_inline_new(odm->parentscene);
			odm->subscene->root_od = odm;
		}
	}

	toolList = NULL;
	switch (desc->tag) {
	case GF_ODF_IOD_TAG:
	{
		GF_InitialObjectDescriptor *the_iod = (GF_InitialObjectDescriptor *)desc;
		odm->OD = (GF_ObjectDescriptor*)malloc(sizeof(GF_ObjectDescriptor));
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
		toolList = the_iod->IPMPToolList;
		free(the_iod);
	}
		break;
	case GF_ODF_OD_TAG:
		odm->Audio_PL = odm->Graphics_PL = odm->OD_PL = odm->Scene_PL = odm->Visual_PL = (u8) -1;
		odm->OD = (GF_ObjectDescriptor *)desc;
		break;
	default:
		gf_term_message(odm->term, odm->net_service->url, "MPEG4 Service Setup Failure", GF_ODF_INVALID_DESCRIPTOR);
		goto err_exit;
	}
	
	if (toolList) {
		Bool ipmp_failed = 0;
/*
		GF_IPMP_Tool *ipmpt;
		i=0;
		while ((ipmpt = gf_list_enum(toolList->ipmp_tools, &i))) {
			if (!Term_CheckIPMPTool(odm->term, ipmpt)) {
				ipmp_failed = 1;
				break;
			}
		}
*/
		gf_odf_desc_del((GF_Descriptor *)toolList);
		if (ipmp_failed) {
			gf_term_message(odm->term, odm->net_service->url, "MPEG4 IPMP Setup Failure - cannot process content", GF_SERVICE_ERROR);
			goto err_exit;
		}
	}

	gf_term_lock_net(term, 1);
	gf_odm_setup_object(odm, odm->net_service);
	gf_term_lock_net(term, 0);

	return;

err_exit:
	if (!odm->parentscene) {
		GF_Event evt;
		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = 0;
		GF_USER_SENDEVENT(odm->term->user, &evt);
	}

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

static void ODM_SelectAlternateStream(GF_ObjectManager *odm, u32 lang_code, u8 stream_type)
{
	u32 i;
	GF_ESD *esd;
	u16 def_id, es_id;

	def_id = 0;
	i=0;
	while ( (esd = (GF_ESD *)gf_list_enum(odm->OD->ESDescriptors, &i)) ) {
		if (esd->decoderConfig->streamType != stream_type) continue;

		if (!esd->langDesc) {
			if (!def_id) def_id = esd->ESID;
			continue;
		}
		if (esd->langDesc->langCode==lang_code) {
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
}


/*Validate the streams in this OD, and check if we have to setup an inline scene*/
GF_Err ODM_ValidateOD(GF_ObjectManager *odm, Bool *hasInline)
{
	u32 i;
	u16 es_id;
	GF_ESD *esd, *base_scene;
	const char *sOpt;
	u32 lang, nb_od, nb_ocr, nb_scene, nb_mp7, nb_ipmp, nb_oci, nb_mpj, nb_other, prev_st;

	nb_od = nb_ocr = nb_scene = nb_mp7 = nb_ipmp = nb_oci = nb_mpj = nb_other = 0;
	prev_st = 0;

	*hasInline = 0;

	/*step 1: validate OD*/
	i=0;
	while ((esd = (GF_ESD *)gf_list_enum(odm->OD->ESDescriptors, &i))) {
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_OD: nb_od++; break;
		case GF_STREAM_OCR: nb_ocr++; break;
		case GF_STREAM_SCENE: nb_scene++; break;
		case GF_STREAM_MPEG7: nb_mp7++; break;
		case GF_STREAM_IPMP: nb_ipmp++; break;
		case GF_STREAM_OCI: nb_oci++; break;
		case GF_STREAM_MPEGJ: nb_mpj++; break;
		case GF_STREAM_PRIVATE_SCENE: nb_scene++; break;
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
	sOpt = gf_cfg_get_key(odm->term->user->config, "Systems", "Language3CC");
	if (!sOpt) {
		gf_cfg_set_key(odm->term->user->config, "Systems", "Language3CC", "und");
		sOpt = "und";
	}
	lang = (sOpt[0]<<16) | (sOpt[1]<<8) | sOpt[2];
#if 0
	if (gf_list_count(odm->OD->ESDescriptors)>1) {
		ODM_SelectAlternateStream(odm, lang, GF_STREAM_SCENE);
		ODM_SelectAlternateStream(odm, lang, GF_STREAM_OD);
		ODM_SelectAlternateStream(odm, lang, GF_STREAM_VISUAL);
		ODM_SelectAlternateStream(odm, lang, GF_STREAM_AUDIO);
		ODM_SelectAlternateStream(odm, lang, GF_STREAM_IPMP);
		ODM_SelectAlternateStream(odm, lang, GF_STREAM_INTERACT);
		ODM_SelectAlternateStream(odm, lang, GF_STREAM_TEXT);
	}
#endif
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
	/*no dependency to external stream, this is an inline*/
	return GF_OK;
}



/*connection of OD and setup of streams. The streams are not requested if the OD
is in an unexecuted state
the ODM is created either because of IOD / remoteOD, or by the OD codec. In the later
case, the GF_InlineScene pointer will be set by the OD codec.*/
GF_EXPORT
void gf_odm_setup_object(GF_ObjectManager *odm, GF_ClientService *serv)
{
	Bool hasInline;
	u32 i, numOK;
	GF_Err e;
	GF_ESD *esd;
	GF_MediaObject *syncRef;

	if (!odm->net_service) odm->net_service = serv;
	
	/*if this is a remote OD, we need a new manager and a new service...*/
	if (odm->OD->URLString) {
		GF_ClientService *parent = odm->net_service;
		char *url = odm->OD->URLString;
		odm->OD->URLString = NULL;
		/*store original OD ID */
		if (!odm->current_time) odm->current_time = odm->OD->objectDescriptorID;

		gf_odf_desc_del((GF_Descriptor *)odm->OD);
		odm->OD = NULL;
		odm->net_service = NULL;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Object redirection to %s\n", url));
		
		/*if object is a scene, create the inline before connecting the object.
			This is needed in irder to register the nodes using the resource for event
			propagation (stored at the inline level)
		*/
		if (odm->mo && (odm->mo->type==GF_MEDIA_OBJECT_SCENE)) {
			odm->subscene = gf_inline_new(odm->parentscene);
			odm->subscene->root_od = odm;
		}
		gf_term_connect_object(odm->term, odm, url, parent);
		free(url);
		return;
	}
	/*restore OD ID */
	if (odm->current_time) {
		odm->OD->objectDescriptorID = odm->current_time;
		odm->current_time = 0;
		odm->flags |= GF_ODM_REMOTE_OD;
	}

	/*HACK - temp storage of sync ref*/
	syncRef = (GF_MediaObject*)odm->ocr_codec;
	odm->ocr_codec = NULL;

	e = ODM_ValidateOD(odm, &hasInline);
	if (e) {
		gf_term_message(odm->term, odm->net_service->url, "MPEG-4 Service Error", e);
		gf_odm_disconnect(odm, 1);
		return;
	}

	if (odm->mo && (odm->mo->type == GF_MEDIA_OBJECT_UPDATES)) {
		hasInline = 0;
	}

	/*if there is a BIFS stream in the OD, we need an GF_InlineScene (except if we already 
	have one, which means this is the first IOD)*/
	if (hasInline && !odm->subscene) {
		odm->subscene = gf_inline_new(odm->parentscene);
		odm->subscene->root_od = odm;
	}

	numOK = odm->pending_channels = 0;
	/*empty IOD, use a dynamic scene*/
	if (!gf_list_count(odm->OD->ESDescriptors) && odm->subscene) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM%d] No streams in object - taking over scene graph generation\n",odm->OD->objectDescriptorID));
		assert(odm->subscene->root_od==odm);
		odm->subscene->is_dynamic_scene = 1;
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Setting up object streams\n"));
		/*avoid channels PLAY request when confirming connection (sync network service)*/
		odm->state = GF_ODM_STATE_IN_SETUP;

		i=0;
		while ((esd = (GF_ESD *)gf_list_enum(odm->OD->ESDescriptors, &i)) ) {
			e = gf_odm_setup_es(odm, esd, serv, syncRef);
			/*notify error but still go on, all streams are not so usefull*/
			if (e==GF_OK) {
				numOK++;
			} else {
				gf_term_message(odm->term, odm->net_service->url, "Stream Setup Failure", e);
			}
		}
		odm->state = GF_ODM_STATE_STOP;
	}

	/*special case for ODs only having OCRs: force a START since they're never refered to by media nodes*/
	if (odm->ocr_codec) gf_odm_start(odm);

#if 0
	/*clean up - note that this will not be performed if one of the stream is using ESD URL*/
	if (!numOK) {
		gf_odm_disconnect(odm, 1);
		return;
	}
#endif
	
	/*setup mediaobject info except for top-level OD*/
	if (odm->parentscene) {
		gf_inline_setup_object(odm->parentscene, odm);
	} else {
		/*othewise send a connect ack for top level*/
		GF_Event evt;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM] Root object connected !\n", odm->net_service->url));
		
		evt.type = GF_EVENT_CONNECT;
		evt.connect.is_connected = 1;
		GF_USER_SENDEVENT(odm->term->user, &evt);
	}

	/* and connect ONLY if main scene - inlines are connected when attached to Inline nodes*/
	if (!odm->parentscene) {
		assert(odm->subscene == odm->term->root_scene);
		assert(odm->subscene->root_od==odm);
		gf_odm_start(odm);
	}
		
	/*for objects inserted by user (subs & co), auto select*/
	if (odm->term->root_scene->is_dynamic_scene && (odm->OD->objectDescriptorID==GF_ESM_DYNAMIC_OD_ID) && (odm->flags & GF_ODM_REMOTE_OD)) {
		GF_Event evt;
		if (odm->OD_PL) {
			gf_inline_select_object(odm->term->root_scene, odm);
			odm->OD_PL = 0;
		}
		evt.type = GF_EVENT_STREAMLIST;
		GF_USER_SENDEVENT(odm->term->user,&evt);
	}
}



void ODM_CheckChannelService(GF_Channel *ch)
{
	if (ch->service == ch->odm->net_service) return;
	/*if the stream has created a service check if close is needed or not*/
	if (ch->esd->URLString && !ch->service->nb_ch_users) 
		gf_term_close_services(ch->odm->term, ch->service);
}

/*setup channel, clock and query caps*/
GF_EXPORT
GF_Err gf_odm_setup_es(GF_ObjectManager *odm, GF_ESD *esd, GF_ClientService *serv, GF_MediaObject *sync_ref)
{
	GF_CodecCapability cap;
	GF_Channel *ch;
	GF_Clock *ck;
	GF_List *ck_namespace;
	GF_Codec *dec;
	s8 flag;
	u16 clockID;
	Bool emulated_od = 0;
	GF_Err e;
	GF_InlineScene *is;

	/*find the clock for this new channel*/
	ck = NULL;
	flag = (s8) -1;

	/*sync reference*/
	if (sync_ref && sync_ref->odm && sync_ref->odm->codec) {
		ck = sync_ref->odm->codec->ck;
		goto clock_setup;
	}
	/*timeline override*/
	if (odm->flags & GF_ODM_INHERIT_TIMELINE) {
		if (odm->parentscene->root_od->subscene->scene_codec)
			ck = odm->parentscene->root_od->subscene->scene_codec->ck;
		else
			ck = odm->parentscene->root_od->subscene->dyn_ck;
		goto clock_setup;
	}

	/*get clocks namespace (eg, parent scene)*/
	is = odm->subscene ? odm->subscene : odm->parentscene;
	if (is->force_sub_clock_id) esd->OCRESID = is->force_sub_clock_id;

	ck_namespace = odm->net_service->Clocks;
	/*little trick for non-OD addressing: if object is a remote one, and service owner already has clocks, 
	override OCR. This will solve addressing like file.avi#audio and file.avi#video*/
	if (!esd->OCRESID && (odm->flags & GF_ODM_REMOTE_OD) && (gf_list_count(ck_namespace)==1) ) {
		ck = (GF_Clock*)gf_list_get(ck_namespace, 0);
		esd->OCRESID = ck->clockID;
	}
	/*for dynamic scene, force all streams to be sync on main OD stream (one timeline, no need to reload ressources)*/
	else if (odm->term->root_scene->is_dynamic_scene) {
		GF_ObjectManager *root_od = odm->term->root_scene->root_od;
		if (gf_list_count(root_od->net_service->Clocks)==1) {
			ck = (GF_Clock*)gf_list_get(root_od->net_service->Clocks, 0);
			esd->OCRESID = ck->clockID;
			goto clock_setup;
		}
	}

	/*do we have an OCR specified*/
	clockID = esd->OCRESID;
	/*if OCR stream force self-synchro !!*/
	if (esd->decoderConfig->streamType==GF_STREAM_OCR) clockID = esd->ESID;
	if (!clockID) {
		/*if no clock ID but depandancy, force the clock to be the base layer for AV but not systems (animation streams, ..)*/
		if ((esd->decoderConfig->streamType==GF_STREAM_VISUAL) || (esd->decoderConfig->streamType==GF_STREAM_AUDIO)) clockID = esd->dependsOnESID;
		if (!clockID) clockID = esd->ESID;
	}

	/*override clock dependencies if specified*/
	if (odm->term->flags & GF_TERM_SINGLE_CLOCK) {
		if (is->scene_codec) {
			clockID = is->scene_codec->ck->clockID;
		} else if (is->od_codec) {
			clockID = is->od_codec->ck->clockID;
		}
		ck_namespace = odm->term->root_scene->root_od->net_service->Clocks;
	}
	/*if the GF_Clock is the stream, check if we have embedded OCR in the stream...*/
	if (clockID == esd->ESID) {
		flag = (esd->slConfig && esd->slConfig->OCRLength > 0);
	}

	if (!esd->slConfig) {
		esd->slConfig = (GF_SLConfig *)gf_odf_desc_new(GF_ODF_SLC_TAG);
		esd->slConfig->timestampResolution = 1000;
	}

	/*attach clock in namespace*/
	ck = gf_clock_attach(ck_namespace, is, clockID, esd->ESID, flag);
	if (!ck) return GF_OUT_OF_MEM;
	esd->OCRESID = ck->clockID;

clock_setup:
	/*create a channel for this stream*/
	ch = gf_es_new(esd);
	if (!ch) return GF_OUT_OF_MEM;
	ch->clock = ck;
	ch->service = serv;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[ODM] Creating codec for stream %d\n", ch->esd->ESID));

	/*setup the decoder for this stream or find the existing one.*/
	e = GF_OK;
	dec = NULL;
	switch (esd->decoderConfig->streamType) {
	case GF_STREAM_OD:
		//OD - MUST be in inline
		if (!odm->subscene) {
			e = GF_NON_COMPLIANT_BITSTREAM;
			break;
		}
		/*OD codec acts as main scene codec when used to generate scene graph*/
		if (! odm->subscene->od_codec) {
			odm->subscene->od_codec = gf_codec_new(odm, esd, odm->OD_PL, &e);
			if (e) return e;
			gf_term_add_codec(odm->term, odm->subscene->od_codec);
		} 
		dec = odm->subscene->od_codec;
		break;
	case GF_STREAM_OCR:
		/*OD codec acts as main scene codec when used to generate scene graph*/
		dec = odm->ocr_codec = gf_codec_new(odm, esd, odm->OD_PL, &e);
		gf_term_add_codec(odm->term, odm->ocr_codec);
		break;
	case GF_STREAM_SCENE:
		/*animationStream */
		if (!odm->subscene) {
			if (!odm->codec) {
				odm->codec = gf_codec_new(odm, esd, odm->Scene_PL, &e);
				gf_term_add_codec(odm->term, odm->codec);
			}
			dec = odm->codec;
		}
		/*inline scene*/
		else {
			if (! odm->subscene->scene_codec) {
				odm->subscene->scene_codec = gf_codec_new(odm, esd, odm->Scene_PL, &e);
				if (!e) gf_term_add_codec(odm->term, odm->subscene->scene_codec);
			}
			dec = odm->subscene->scene_codec;
		}
		break;
	case GF_STREAM_OCI:
		/*OCI - only one per OD */
		if (odm->oci_codec) {
			e = GF_NON_COMPLIANT_BITSTREAM;
		} else {
			odm->oci_codec = gf_codec_new(odm, esd, odm->OD_PL, &e);
			odm->oci_codec->odm = odm;
			gf_term_add_codec(odm->term, odm->oci_codec);
		}
		break;

	case GF_STREAM_AUDIO:
	case GF_STREAM_VISUAL:
		/*we have a media or user-specific codec...*/
		if (!odm->codec) {
			odm->codec = gf_codec_new(odm, esd, (esd->decoderConfig->streamType==GF_STREAM_VISUAL) ? odm->Visual_PL : odm->Audio_PL, &e);
			if (!e) gf_term_add_codec(odm->term, odm->codec);
		}
		dec = odm->codec;
		break;

	/*interaction stream*/
	case GF_STREAM_INTERACT:
		if (!odm->codec) {
			odm->codec = gf_codec_new(odm, esd, odm->OD_PL, &e);
			if (!e) {
				IS_Configure(odm->codec->decio, odm->parentscene, esd->URLString ? 1 : 0);
				gf_term_add_codec(odm->term, odm->codec);
				/*register it*/
				gf_list_add(odm->term->input_streams, odm->codec);
			}
		}
		dec = odm->codec;
		if ((esd->ESID==esd->OCRESID) &&(esd->ESID>=65530)) {
			emulated_od = 1;
		}
		break;

	case GF_STREAM_PRIVATE_SCENE:
		if (odm->subscene) {
			assert(!odm->subscene->scene_codec);
			odm->subscene->scene_codec = gf_codec_new(odm, esd, odm->Scene_PL, &e);
			if (odm->subscene->scene_codec) {
				gf_term_add_codec(odm->term, odm->subscene->scene_codec);
			}
			dec = odm->subscene->scene_codec;
		} else {
			/*this is a bit tricky: the scene decoder needs to ba called with the dummy streams of this 
			object, so we associate the main decoder to this object*/
			odm->codec = dec = gf_codec_use_codec(odm->parentscene->scene_codec, odm);
			gf_term_add_codec(odm->term, odm->codec);
		}
		break;
	/*all other cases*/
	default:
		if (!odm->codec) {
			odm->codec = gf_codec_new(odm, esd, odm->OD_PL, &e);
			if (!e) gf_term_add_codec(odm->term, odm->codec);

		}
		dec = odm->codec;
		break;
	}

	/*if we have a decoder, set up the channel and co.*/
	if (!dec) {
		if (e) {
			gf_es_del(ch);
			return e;
		}
	}

	/*setup scene decoder*/
	if (dec->decio && (dec->decio->InterfaceType==GF_SCENE_DECODER_INTERFACE) ) {
		GF_SceneDecoder *sdec = (GF_SceneDecoder *) dec->decio;
		is = odm->subscene ? odm->subscene : odm->parentscene;
		if (sdec->AttachScene) {
			/*if a node asked for this media object, use the scene graph of the node (AnimationStream in PROTO)*/
			if (odm->mo && odm->mo->node_ptr) {
				GF_SceneGraph *sg = is->graph;
				/*FIXME - this MUST be cleaned up*/
				is->graph = gf_node_get_graph((GF_Node*)odm->mo->node_ptr);
				sdec->AttachScene(sdec, is, (is->scene_codec==dec) ? 1: 0);
				is->graph = sg;
				odm->mo->node_ptr = NULL;
			} else {
				sdec->AttachScene(sdec, is, (is->scene_codec==dec) ? 1: 0);
			}
		}
	}

	ch->es_state = GF_ESM_ES_SETUP;
	ch->odm = odm;

	/*get media padding BEFORE channel setup, since we use it on channel connect ack*/
	if (dec) {
		cap.CapCode = GF_CODEC_PADDING_BYTES;
		gf_codec_get_capability(dec, &cap);
		ch->media_padding_bytes = cap.cap.valueInt;

		cap.CapCode = GF_CODEC_RESILIENT;
		gf_codec_get_capability(dec, &cap);
		ch->codec_resilient = cap.cap.valueInt;
	}

	if (emulated_od) {
		ch->service = NULL;
	}

	/*one more channel to wait for*/
	odm->pending_channels++;

	/*service redirection*/
	if (esd->URLString) {
		GF_ChannelSetup *cs;
		/*here we have a pb with the MPEG4 model: streams are supposed to be attachable as soon as the OD 
		update is received, but this is not true with ESD URLs, where service setup may take some time (file
		downloading, authentification, etc...). We therefore need to wait for the service connect response before 
		setting up the channel...*/
		cs = (GF_ChannelSetup*)malloc(sizeof(GF_ChannelSetup));
		cs->ch = ch;
		cs->dec = dec;

		/*HACK: special case when OD resources are statically described in the ESD itself (ISMA streaming)*/
		if ((ch->esd->decoderConfig->streamType==GF_STREAM_OD) && strstr(ch->esd->URLString, "data:application/mpeg4-od-au;") ) 
			dec->flags |= GF_ESM_CODEC_IS_STATIC_OD;

		gf_term_lock_net(odm->term, 1);
		gf_list_add(odm->term->channels_pending, cs);
		e = gf_term_connect_remote_channel(odm->term, ch, esd->URLString);
		if (e) {
			s32 i = gf_list_find(odm->term->channels_pending, cs);
			if (i>=0) {
				gf_list_rem(odm->term->channels_pending, (u32) i);
				free(cs);
				odm->pending_channels--;
				ODM_CheckChannelService(ch);
				gf_es_del(ch);
			}
		}
		gf_term_lock_net(odm->term, 0);
		if (ch->service->owner) {
			gf_list_del_item(odm->term->channels_pending, cs);
			free(cs);
			return gf_odm_post_es_setup(ch, dec, GF_OK);
		}
		return e;
	}

	/*regular setup*/
	return gf_odm_post_es_setup(ch, dec, GF_OK);
}

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
	if (dec) gf_list_insert(ch->odm->channels, ch, 0);

	if (ch->service) {
		ch->es_state = GF_ESM_ES_WAIT_FOR_ACK;
		if (ch->esd->URLString) {
			strcpy(szURL, ch->esd->URLString);
		} else {
			sprintf(szURL, "ES_ID=%d", ch->esd->ESID);
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
			gf_list_rem(ch->odm->channels, 0);
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
	if ( (ch->odm->state==GF_ODM_STATE_PLAY) 
		/*HACK: special case when OD resources are statically described in the ESD itself (ISMA streaming)*/
//		|| 	(dec && (dec->flags & GF_ESM_CODEC_IS_STATIC_OD)) 
	) {

		gf_term_lock_net(ch->odm->term, 1);
		gf_list_del_item(ch->odm->term->media_queue, ch->odm);

		gf_es_start(ch);
		com.command_type = GF_NET_CHAN_PLAY;
		com.base.on_channel = ch;
		com.play.speed = FIX2FLT(ch->clock->speed);
		com.play.start_range = gf_clock_time(ch->clock);
		com.play.start_range /= 1000;
		com.play.end_range = -1.0;
		gf_term_service_command(ch->service, &com);
		if (dec && (dec->Status!=GF_ESM_CODEC_PLAY)) gf_term_start_codec(dec);
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
	if (!count && odm->oci_codec)
		count = gf_codec_remove_channel(odm->oci_codec, ch);
	if (!count && odm->subscene) {
		if (odm->subscene->scene_codec) count = gf_codec_remove_channel(odm->subscene->scene_codec, ch);
		if (!count) count = gf_codec_remove_channel(odm->subscene->od_codec, ch);
	}
	assert(count);
	
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

/*this is the tricky part: make sure the net is locked before doing anything since an async service 
reply could destroy the object we're queuing for play*/
void gf_odm_start(GF_ObjectManager *odm)
{
	gf_term_lock_net(odm->term, 1);

	/*only if not open & ready (not waiting for ACK on channel setup)*/
	if (!odm->state && !odm->pending_channels && odm->OD) {
		GF_Channel *ch;
		u32 i = 0;
		odm->state = GF_ODM_STATE_PLAY;

		/*look for a given segment name to play*/
		if (odm->subscene) {
			char *url, *frag;
			assert(odm->subscene->root_od==odm);

			url = (odm->mo && odm->mo->URLs.count) ? odm->mo->URLs.vals[0].url : odm->net_service->url;
			frag = strrchr(url, '#');
			if (frag) {
				GF_Segment *seg = gf_odm_find_segment(odm, frag+1);
				if (seg) {
					odm->media_start_time = (u64) ((s64) seg->startTime*1000);
					odm->media_stop_time =  (u64) ((s64) (seg->startTime + seg->Duration)*1000);
				}
			}
		}

		/*start all channels and postpone play - this assures that all channels of a multiplexed are setup
		before one starts playing*/
		while ( (ch = (GF_Channel*)gf_list_enum(odm->channels, &i)) ) {
			gf_es_start(ch);
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d] CH%d: At OTB %d starting channel\n", odm->OD->objectDescriptorID, ch->esd->ESID, gf_clock_time(ch->clock)));
		}
		if (gf_list_find(odm->term->media_queue, odm)<0) gf_list_add(odm->term->media_queue, odm);
	}
	gf_term_lock_net(odm->term, 0);
}

void gf_odm_play(GF_ObjectManager *odm)
{
	GF_Channel *ch;
	u32 i;
	u32 nb_failure;
	u64 range_end;
	Bool skip_od_st;
	GF_NetworkCommand com;
	MediaControlStack *ctrl;
	GF_Clock *parent_ck = NULL;

	if (odm->parentscene) {
		parent_ck = gf_odm_get_media_clock(odm->parentscene->root_od);
		if (!gf_odm_shares_clock(odm, parent_ck)) parent_ck = NULL;
	}

	skip_od_st = (odm->subscene && odm->subscene->static_media_ressources) ? 1 : 0;
	range_end = odm->media_stop_time;
//	odm->media_stop_time = 0;

	nb_failure = gf_list_count(odm->channels);

	/*send play command*/
	com.command_type = GF_NET_CHAN_PLAY;
	i=0;
	while ( (ch = (GF_Channel*)gf_list_enum(odm->channels, &i)) ) {
		Double ck_time;

		if (ch->ipmp_tool) {
			GF_IPMPEvent evt;
			GF_Err e;
			memset(&evt, 0, sizeof(evt));
			evt.event_type=GF_IPMP_TOOL_GRANT_ACCESS;
			evt.channel = ch;
			e = ch->ipmp_tool->process(ch->ipmp_tool, &evt);
			if (e) {
				gf_term_message(odm->term, NULL, "PLAY access is not granted on channel - please check your license", e);
				gf_es_stop(ch);
				continue;
			}
		}
		nb_failure --;

		com.base.on_channel = ch;
		com.play.speed = 1.0;
		/*play from requested time (seeking or non-mpeg4 media control)*/
		if (odm->media_start_time && !ch->clock->clock_init) {
			ck_time = (Double) (s64) odm->media_start_time;
			ck_time /= 1000;
		}
		/*play from current time*/
		else {
			ck_time = gf_clock_time(ch->clock);
			ck_time /= 1000;
			/*handle initial start - MPEG-4 is a bit annoying here, streams are not started through OD but through
			scene nodes. If the stream runs on the BIFS/OD clock, the clock is already started at this point and we're 
			sure to get at least a one-frame delay in PLAY, so just remove it - note we're generous but this shouldn't hurt*/
			if (ck_time<=0.5) ck_time = 0;
		}
		com.play.start_range = ck_time;

		if (range_end) {
			com.play.end_range = (s64) range_end / 1000.0;
		} else {
			if (!odm->subscene && gf_odm_shares_clock(odm->parentscene->root_od, ch->clock)
				&& (odm->parentscene->root_od->media_stop_time != odm->parentscene->root_od->duration)
			) {
				com.play.end_range = (s64) odm->parentscene->root_od->media_stop_time / 1000.0;
			} else {
				com.play.end_range = -1;
			}
		}

		/*if object shares parent scene clock, do not use media control*/
		ctrl = parent_ck ? NULL : ODM_GetMediaControl(odm);
		/*override range and speed with MC*/
		if (ctrl) {
			MC_GetRange(ctrl, &com.play.start_range, &com.play.end_range);
			com.play.speed = FIX2FLT(ctrl->control->mediaSpeed);
			/*if the channel doesn't control the clock, jump to current time in the controled range, not just the begining*/
			if ((ch->esd->ESID!=ch->clock->clockID) && (ck_time>com.play.start_range) && (com.play.end_range>com.play.start_range) && (ck_time<com.play.end_range)) {
				com.play.start_range = ck_time;
			}
			gf_clock_set_speed(ch->clock, ctrl->control->mediaSpeed);
			/*if requested seek time AND media control, adjust start range to current play time*/
			if (odm->media_start_time) {
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
		}
		/*full object playback*/
		if (com.play.end_range<=0) {
			odm->media_stop_time = odm->subscene ? 0 : odm->duration;
		} else {
			/*segment playback - since our timing is in ms whereas segment ranges are double precision, 
			make sure we have a LARGER range in ms, otherwise media sensors won't deactivate properly*/
			odm->media_stop_time = (u64) ceil(1000 * com.play.end_range);
		}

		/*don't replay OD channel, only init clock if needed*/
		if (!ch->service || (skip_od_st && (ch->esd->decoderConfig->streamType==GF_STREAM_OD))) {
			Bool gf_es_owns_clock(GF_Channel *ch);

			if (gf_es_owns_clock(ch) ) 
				gf_clock_set_time(ch->clock, (u32) (com.play.start_range*1000));

			ch->IsClockInit = 1;
			if (ch->BufferOn) {
				ch->BufferOn = 0;
				gf_clock_buffer_off(ch->clock);
			}
		} else {
			gf_term_service_command(ch->service, &com);
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d] CH%d: At OTB %d requesting PLAY from %g to %g (clock init %d)\n", odm->OD->objectDescriptorID, ch->esd->ESID, gf_clock_time(ch->clock), com.play.start_range, com.play.end_range, ch->clock->clock_init));
		}
	}
//	odm->media_start_time = 0;

	if (nb_failure) {
		odm->state = GF_ODM_STATE_BLOCKED;
		return;
	}

	gf_term_service_media_event(odm, GF_EVENT_MEDIA_DATA_REQUEST);


	/*start codecs last (otherwise we end up pulling data from channels not yet connected->pbs when seeking)*/
	if (odm->codec) {
		/*reset*/
		if (odm->codec->CB) {
			gf_cm_set_status(odm->codec->CB, CB_STOP);
			odm->codec->CB->HasSeenEOS = 0;
		}
		gf_term_start_codec(odm->codec);
	} else if (odm->subscene) {
		if (odm->subscene->scene_codec) gf_term_start_codec(odm->subscene->scene_codec);
		if (!skip_od_st && odm->subscene->od_codec) gf_term_start_codec(odm->subscene->od_codec);

		if (odm->flags & GF_ODM_REGENERATE_SCENE) {
			odm->flags &= ~GF_ODM_REGENERATE_SCENE;
			gf_inline_regenerate(odm->subscene);
		}
	}
	if (odm->ocr_codec) gf_term_start_codec(odm->ocr_codec);
	if (odm->oci_codec) gf_term_start_codec(odm->oci_codec);
}


void gf_odm_stop(GF_ObjectManager *odm, Bool force_close)
{
	GF_Channel *ch;
	u32 i;
	MediaControlStack *ctrl;
	MediaSensorStack *media_sens;
	GF_NetworkCommand com;
	
	if (!odm->state) return;

#if 0
	/*Handle broadcast environment, do not stop the object if no time control and instruction
	comes from the scene*/
	if (odm->no_time_ctrl && !force_close) {
		GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d] - broadcast detected, ignoring Stop from scene\n", odm->OD->objectDescriptorID);
		return;
	}
#endif


	gf_term_lock_net(odm->term, 1);
	gf_list_del_item(odm->term->media_queue, odm);
	gf_term_lock_net(odm->term, 0);

	/*little opt for image codecs: don't actually stop the OD*/
	if (!force_close && odm->codec && odm->codec->CB) {
		if (odm->codec->CB->Capacity==1) return;
	}

	/*object was not unlocked, decoders were not started*/
	if (odm->state==GF_ODM_STATE_BLOCKED) {
		odm->current_time = 0;
		return;
	}

	/*stop codecs*/
	if (odm->codec) {
		gf_term_stop_codec(odm->codec);
	} else if (odm->subscene) {
		u32 i=0;
		GF_ObjectManager *sub_odm;
		if (odm->subscene->scene_codec) gf_term_stop_codec(odm->subscene->scene_codec);
		if (odm->subscene->od_codec) gf_term_stop_codec(odm->subscene->od_codec);

		/*stops all resources of the subscene as well*/
		while ((sub_odm=(GF_ObjectManager *)gf_list_enum(odm->subscene->ODlist, &i))) {
			gf_odm_stop(sub_odm, force_close);
		}
	}
	if (odm->ocr_codec) gf_term_stop_codec(odm->ocr_codec);
	if (odm->oci_codec) gf_term_stop_codec(odm->oci_codec);

	gf_term_lock_net(odm->term, 1);

	/*send stop command*/
	com.command_type = GF_NET_CHAN_STOP;
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i)) ) {

		if (ch->ipmp_tool) {
			GF_IPMPEvent evt;
			memset(&evt, 0, sizeof(evt));
			evt.event_type=GF_IPMP_TOOL_RELEASE_ACCESS;
			evt.channel = ch;
			ch->ipmp_tool->process(ch->ipmp_tool, &evt);
		}
		
		if (ch->service) {
			com.base.on_channel = ch;
			gf_term_service_command(ch->service, &com);
			GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[ODM%d] CH %d At OTB %d requesting STOP\n", odm->OD->objectDescriptorID, ch->esd->ESID, gf_clock_time(ch->clock)));
		}
	}
	gf_term_service_media_event(odm, GF_EVENT_MEDIA_STOP);

	/*stop channels*/
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i)) ) {
		/*stops clock if this is a scene stop*/
		if (!(odm->flags & GF_ODM_INHERIT_TIMELINE) && odm->subscene) gf_clock_stop(ch->clock);
		gf_es_stop(ch);
	}

	gf_term_lock_net(odm->term, 0);

	odm->state = GF_ODM_STATE_STOP;
	odm->current_time = 0;

	/*reset media sensor(s)*/
	if (force_close!=2) {
		i = 0;
		while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i))){
			MS_Stop(media_sens);
		}
	}
	/*reset media control state*/
	ctrl = ODM_GetMediaControl(odm);
	if (ctrl) ctrl->current_seg = 0;
}

void gf_odm_on_eos(GF_ObjectManager *odm, GF_Channel *on_channel)
{
	if (gf_odm_check_segment_switch(odm)) return;

	gf_term_service_media_event(odm, GF_EVENT_MEDIA_END_OF_DATA);
	
	if (odm->codec && (on_channel->esd->decoderConfig->streamType==odm->codec->type)) {
		gf_codec_set_status(odm->codec, GF_ESM_CODEC_EOS);
		return;
	} 
	if (on_channel->esd->decoderConfig->streamType==GF_STREAM_OCR) {
		gf_codec_set_status(odm->ocr_codec, GF_ESM_CODEC_EOS);
		return;
	}
	if (on_channel->esd->decoderConfig->streamType==GF_STREAM_OCI) {
		gf_codec_set_status(odm->oci_codec, GF_ESM_CODEC_EOS);
		return;
	}
	if (!odm->subscene) return;

	if (odm->subscene->scene_codec && (gf_list_find(odm->subscene->scene_codec->inChannels, on_channel)>=0) ) {
		gf_codec_set_status(odm->subscene->scene_codec, GF_ESM_CODEC_EOS);
		return;
	}

	if (on_channel->esd->decoderConfig->streamType==GF_STREAM_OD) {
		gf_codec_set_status(odm->subscene->od_codec, GF_ESM_CODEC_EOS);
		return;
	}
}

void gf_odm_set_duration(GF_ObjectManager *odm, GF_Channel *ch, u64 stream_duration)
{
	if (odm->codec) {
		if (ch->esd->decoderConfig->streamType == odm->codec->type)
			if (odm->duration < stream_duration)
				odm->duration = stream_duration;
	} else if (odm->ocr_codec) {
		if (ch->esd->decoderConfig->streamType == odm->ocr_codec->type)
			if (odm->duration < stream_duration)
				odm->duration = stream_duration;
	} else if (odm->subscene && odm->subscene->scene_codec) {
		//if (gf_list_find(odm->subscene->scene_codec->inChannels, ch) >= 0) {
			if (odm->duration < stream_duration) odm->duration = stream_duration;
		//}
	}

	/*update scene duration*/
	gf_inline_set_duration(odm->subscene ? odm->subscene : (odm->parentscene ? odm->parentscene : odm->term->root_scene));
}


GF_Clock *gf_odm_get_media_clock(GF_ObjectManager *odm)
{
	if (odm->codec) return odm->codec->ck;
	if (odm->ocr_codec) return odm->ocr_codec->ck;
	if (odm->subscene && odm->subscene->scene_codec) return odm->subscene->scene_codec->ck;
	if (odm->subscene && odm->subscene->dyn_ck) return odm->subscene->dyn_ck;
	return NULL;
}


void ODM_SetMediaControl(GF_ObjectManager *odm, MediaControlStack *ctrl)
{
	u32 i;
	GF_Channel *ch;

	/*keep track of it*/
	if (ctrl && (gf_list_find(odm->mc_stack, ctrl) < 0)) gf_list_add(odm->mc_stack, ctrl);
	if (ctrl && !ctrl->control->enabled) return;

	if (odm->subscene && odm->subscene->is_dynamic_scene) {
		if (odm->subscene->dyn_ck) {
			/*deactivate current control*/
			if (ctrl && odm->subscene->dyn_ck->mc) {
				odm->subscene->dyn_ck->mc->control->enabled = 0;
				gf_node_event_out_str((GF_Node *)odm->subscene->dyn_ck->mc->control, "enabled");
			}
			odm->subscene->dyn_ck->mc = ctrl;
		}
	} else {
		/*for each clock in the controled OD*/
		i=0;
		while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
			if (ch->clock->mc != ctrl) {
				/*deactivate current control*/
				if (ctrl && ch->clock->mc) {
					ch->clock->mc->control->enabled = 0;
					gf_node_event_out_str((GF_Node *)ch->clock->mc->control, "enabled");
				}
				/*and attach this control to the clock*/
				ch->clock->mc = ctrl;
			}
		}
	}
	/*store active control on media*/
	odm->media_ctrl = ODM_GetMediaControl(odm);
}

MediaControlStack *ODM_GetMediaControl(GF_ObjectManager *odm)
{
	GF_Clock *ck;
	ck = gf_odm_get_media_clock(odm);
	if (!ck) return NULL;
	return ck->mc;
}

MediaControlStack *ODM_GetObjectMediaControl(GF_ObjectManager *odm)
{
	MediaControlStack *ctrl;
	ctrl = ODM_GetMediaControl(odm);
	if (!ctrl) return NULL;
	/*inline scene control*/
	if (odm->subscene && (ctrl->stream->odm == odm->subscene->root_od) ) return ctrl;
	if (ctrl->stream->OD_ID != odm->OD->objectDescriptorID) return NULL;
	return ctrl;
}

void ODM_RemoveMediaControl(GF_ObjectManager *odm, MediaControlStack *ctrl)
{
	gf_list_del_item(odm->mc_stack, ctrl);
	/*removed. Note the spec doesn't say what to do in this case...*/
	if (odm->media_ctrl == ctrl) ODM_SetMediaControl(odm, NULL);
}

Bool ODM_SwitchMediaControl(GF_ObjectManager *odm, MediaControlStack *ctrl)
{
	u32 i;
	MediaControlStack *st2;
	if (!ctrl->control->enabled) return 0;

	/*for all media controls other than this one force enable to false*/
	i=0;
	while ((st2 = (MediaControlStack *)gf_list_enum(odm->mc_stack, &i))) {
		if (st2 == ctrl) continue;
		if (st2->control->enabled) {
			st2->control->enabled = 0;
			gf_node_event_out_str((GF_Node *) st2->control, "enabled");
		}
		st2->enabled = 0;
	}
	if (ctrl == odm->media_ctrl) return 0;
	ODM_SetMediaControl(odm, ctrl);
	return 1;
}

Bool gf_odm_shares_clock(GF_ObjectManager *odm, GF_Clock *ck)
{
	u32 i = 0;
	GF_Channel *ch;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
		if (ch->clock == ck) return 1;
	}
	return 0;
}



void gf_odm_pause(GF_ObjectManager *odm)
{
	u32 i;
	GF_NetworkCommand com;
	MediaSensorStack *media_sens;
	GF_Channel *ch;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;


	/*stop codecs, and update status for media codecs*/
	if (odm->codec) {
		gf_term_stop_codec(odm->codec);
		gf_codec_set_status(odm->codec, GF_ESM_CODEC_PAUSE);
	} else if (odm->subscene) {
		if (odm->subscene->scene_codec) {
			gf_codec_set_status(odm->subscene->scene_codec, GF_ESM_CODEC_PAUSE);
			gf_term_stop_codec(odm->subscene->scene_codec);
		}
		if (odm->subscene->od_codec) gf_term_stop_codec(odm->subscene->od_codec);
	}
	if (odm->ocr_codec) gf_term_stop_codec(odm->ocr_codec);
	if (odm->oci_codec) gf_term_stop_codec(odm->oci_codec);

	com.command_type = GF_NET_CHAN_PAUSE;
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i))) {
		gf_clock_pause(ch->clock);
		com.base.on_channel = ch;
		gf_term_service_command(ch->service, &com);
	}

	/*mediaSensor  shall generate isActive false when paused*/
	i=0;
	while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i)) ) {
		if (media_sens && media_sens->sensor->isActive) {
			media_sens->sensor->isActive = 0;
			gf_node_event_out_str((GF_Node *) media_sens->sensor, "isActive");
		}
	}
}

void gf_odm_resume(GF_ObjectManager *odm)
{
	u32 i;
	GF_NetworkCommand com;
	GF_Channel *ch;
	MediaSensorStack *media_sens;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;


	/*start codecs, and update status for media codecs*/
	if (odm->codec) {
		gf_term_start_codec(odm->codec);
		gf_codec_set_status(odm->codec, GF_ESM_CODEC_PLAY);
	} else if (odm->subscene) {
		if (odm->subscene->scene_codec) {
			gf_codec_set_status(odm->subscene->scene_codec, GF_ESM_CODEC_PLAY);
			gf_term_start_codec(odm->subscene->scene_codec);
		}
		if (odm->subscene->od_codec) gf_term_start_codec(odm->subscene->od_codec);
	}
	if (odm->ocr_codec) gf_term_start_codec(odm->ocr_codec);
	if (odm->oci_codec) gf_term_start_codec(odm->oci_codec);
	
	com.command_type = GF_NET_CHAN_RESUME;
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i)) ){
		gf_clock_resume(ch->clock);
		com.base.on_channel = ch;
		gf_term_service_command(ch->service, &com);
	}

	/*mediaSensor shall generate isActive TRUE when resumed*/
	i=0;
	while ((media_sens = (MediaSensorStack *)gf_list_enum(odm->ms_stack, &i)) ){
		if (media_sens && !media_sens->sensor->isActive) {
			media_sens->sensor->isActive = 1;
			gf_node_event_out_str((GF_Node *) media_sens->sensor, "isActive");
		}
	}
}

void gf_odm_set_speed(GF_ObjectManager *odm, Fixed speed)
{
	u32 i;
	GF_NetworkCommand com;
	GF_Channel *ch;

	if (odm->flags & GF_ODM_NO_TIME_CTRL) return;

	com.command_type = GF_NET_CHAN_SET_SPEED;
	com.play.speed = FIX2FLT(speed);
	i=0;
	while ((ch = (GF_Channel*)gf_list_enum(odm->channels, &i)) ) {
		gf_clock_set_speed(ch->clock, speed);
		com.play.on_channel = ch;
		gf_term_service_command(ch->service, &com);
	}
}

GF_Segment *gf_odm_find_segment(GF_ObjectManager *odm, char *descName)
{
	GF_Segment *desc;
	u32 i = 0;
	while ( (desc = (GF_Segment *)gf_list_enum(odm->OD->OCIDescriptors, &i)) ){
		if (desc->tag != GF_ODF_SEGMENT_TAG) continue;
		if (!stricmp(desc->SegmentName, descName)) return desc;
	}
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
}

Bool gf_odm_check_segment_switch(GF_ObjectManager *odm)
{
	u32 count, i;
	GF_Segment *cur, *next;
	MediaControlStack *ctrl = ODM_GetMediaControl(odm);

	/*if no control or control not on this object ignore segment switch*/
	if (!ctrl || (ctrl->stream->odm != odm)) return 0;

	count = gf_list_count(ctrl->seg);
	/*reached end of controled stream (no more segments)*/
	if (ctrl->current_seg>=count) return 0;

	/*synth media, trigger if end of segment run-time*/
	if (!odm->codec || ((odm->codec->type!=GF_STREAM_VISUAL) && (odm->codec->type!=GF_STREAM_AUDIO))) {
		GF_Clock *ck = gf_odm_get_media_clock(odm);
		u32 now = gf_clock_time(ck);
		u64 dur = odm->subscene ? odm->subscene->duration : odm->duration;
		cur = (GF_Segment *)gf_list_get(ctrl->seg, ctrl->current_seg);
		if (odm->subscene && odm->subscene->needs_restart) return 0;
		if (cur) dur = (u32) ((cur->Duration+cur->startTime)*1000);
		if (now<=dur) return 0;
	} else {
		/*FIXME - for natural media with scalability, we should only process when all streams of the object are done*/
	}

	/*get current segment and move to next one*/
	cur = (GF_Segment *)gf_list_get(ctrl->seg, ctrl->current_seg);
	ctrl->current_seg ++;

	/*resync in case we have been issuing a play range over several segments*/
	for (i=ctrl->current_seg; i<count; i++) {
		next = (GF_Segment *)gf_list_get(ctrl->seg, i);
		if (
			/*if next seg start is after cur seg start*/
			(cur->startTime < next->startTime) 
			/*if next seg start is before cur seg end*/
			&& (cur->startTime + cur->Duration > next->startTime) 
			/*if next seg start is already passed*/
			&& (1000*next->startTime < odm->current_time)
			/*then next segment was taken into account when requesting play*/
			) {
			cur = next;
			ctrl->current_seg ++;
		}
	}
	/*if last segment in ctrl is done, end of stream*/
	if (ctrl->current_seg >= count) return 0;
	next = (GF_Segment *)gf_list_get(ctrl->seg, ctrl->current_seg);

	/*if next seg start is not in current seg, media needs restart*/
	if ((next->startTime < cur->startTime) || (cur->startTime + cur->Duration < next->startTime))
		MC_Restart(odm);

	return 1;
}

