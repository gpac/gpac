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
#include <gpac/network.h>



#define GET_TERM()	GF_Terminal *term = (GF_Terminal *) user_priv; if (!term) return;

static GFINLINE GF_Channel *gf_term_get_channel(GF_ClientService *service, LPNETCHANNEL ns)
{
	GF_Channel *ch = (GF_Channel *)ns;
	if (!service || !ch) return NULL;
	if (ch->chan_id != (u32) ch) return NULL;
	if (ch->service != service) return NULL;
	return ch;
}

static void term_on_message(void *user_priv, GF_ClientService *service, GF_Err error, const char *message)
{
	GET_TERM();

	/*check for UDP timeout*/
	if (error==GF_IP_UDP_TIMEOUT) {
		const char *sOpt = gf_cfg_get_key(term->user->config, "Network", "AutoReconfigUDP");
		if (sOpt && !stricmp(sOpt, "yes")) {
			char szMsg[1024];
			sprintf(szMsg, "!! UDP down (%s) - Retrying with TCP !!\n", message);
			gf_term_message(term, service->url, szMsg, GF_OK);

			/*reload scene*/
			if (term->reload_url) free(term->reload_url);
			term->reload_state = 1;
			term->reload_url = strdup(term->root_scene->root_od->net_service->url);
			gf_cfg_set_key(term->user->config, "Network", "UDPNotAvailable", "yes");
			return;
		}
	}
	gf_term_message(term, service->url, message, error);
}

static void term_on_connect(void *user_priv, GF_ClientService *service, LPNETCHANNEL netch, GF_Err err)
{
	GF_Channel *ch;
	GF_ObjectManager *root;
	GET_TERM();

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Connection ACK received from %s (channel %d) - %s\n", service->url, (u32) netch, gf_error_to_string(err) ));

	root = service->owner;
	if (root && (root->net_service != service)) {
		gf_term_message(term, service->url, "Incomaptible module type", GF_SERVICE_ERROR);
		return;
	}
	/*this is service connection*/
	if (!netch) {
		gf_term_service_media_event(service->owner, GF_EVENT_MEDIA_END_SESSION_SETUP);
		if (err) {
			char msg[5000];
			sprintf(msg, "Cannot open %s", service->url);
			gf_term_message(term, service->url, msg, err);

			gf_term_service_media_event(service->owner, GF_EVENT_MEDIA_ERROR);

			/*destroy service only if attached*/
			if (root) {
				gf_term_lock_net(term, 1);
				service->ifce->CloseService(service->ifce);
				root->net_service = NULL;
				service->owner = NULL;
				/*depends on module: some module could forget to call gf_term_on_disconnect */
				if ( gf_list_del_item(term->net_services, service) >= 0) {
					/*and queue for destroy*/
					gf_list_add(term->net_services_to_remove, service);
				}
				gf_term_lock_net(term, 0);
				if (!root->parentscene) {
					GF_Event evt;
					evt.type = GF_EVENT_CONNECT;
					evt.connect.is_connected = 0;
					GF_USER_SENDEVENT(term->user, &evt);
				} else {
					/*try to reinsert OD for VRML/X3D with multiple URLs:
					1- first remove from parent scene without destroying object, this will trigger a re-setup
					if other URLs are present
					2- then destroy object*/
					gf_inline_remove_object(root->parentscene, root, 0);
					gf_odm_disconnect(root, 1);
				}
				return;
			}
		}

		if (!root) {
			/*channel service connect*/
			u32 i;
			GF_ChannelSetup *cs;
			GF_List *ODs = gf_list_new();
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
				free(cs);
			}
			gf_term_lock_net(term, 0);
			/*finally setup all ODs concerned (we do this later in case of scalability)*/
			while (gf_list_count(ODs)) {
				GF_ObjectManager *odm = (GF_ObjectManager*)gf_list_get(ODs, 0);
				gf_list_rem(ODs, 0);
				/*force re-setup*/
				gf_inline_setup_object(odm->parentscene, odm);
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
	}

	/*this is channel connection*/
	ch = gf_term_get_channel(service, netch);
	if (!ch) return;

	/*confirm channel connection even if error - this allow playback of objects even if not all streams are setup
	*/
	gf_term_lock_net(term, 1);
	gf_es_on_connect(ch);
	gf_term_lock_net(term, 0);

	if (err) {
		gf_term_message(term, service->url, "Channel Connection Failed", err);
		ch->es_state = GF_ESM_ES_UNAVAILABLE;
//		return;
	}

	/*Plays request are skiped until all channels are connected. We send a PLAY on the objecy in case 
		1-OD user has requested a play 
		2-this is a channel of the root OD
	*/
	if ( (ch->odm->mo && ch->odm->mo->num_open) 
		|| !ch->odm->parentscene
	) {
		gf_odm_start(ch->odm);
	}
#if 0
	else if (ch->odm->codec && ch->odm->codec->ck && ch->odm->codec->ck->no_time_ctrl) {
		gf_odm_play(ch->odm);
	}
#endif
}

static void term_on_disconnect(void *user_priv, GF_ClientService *service, LPNETCHANNEL netch, GF_Err response)
{
	GF_ObjectManager *root;
	GF_Channel *ch;
	GET_TERM();

	/*may be null upon destroy*/
	root = service->owner;
	if (root && (root->net_service != service)) {
		if (root->net_service) gf_term_message(term, service->url, "Incompatible module type", GF_SERVICE_ERROR);
		return;
	}
	/*this is service disconnect*/
	if (!netch) {
		gf_term_lock_net(term, 1);
		/*unregister from valid services*/
		if (gf_list_del_item(term->net_services, service)>=0) {
			/*and queue for destroy*/
			gf_list_add(term->net_services_to_remove, service);
		}
		gf_term_lock_net(term, 0);
		return;
	}
	/*this is channel disconnect*/

	/*no notif in case of failure for disconnection*/
	ch = gf_term_get_channel(service, netch);
	if (!ch) return;
	/*signal channel state*/
	ch->es_state = GF_ESM_ES_DISCONNECTED;
}

static void term_on_slp_received(void *user_priv, GF_ClientService *service, LPNETCHANNEL netch, char *data, u32 data_size, GF_SLHeader *hdr, GF_Err reception_status)
{
	GF_Channel *ch;
	GET_TERM();

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
	if (esd1->ESID==esd2->ESID) return 1;
	return 0;
}

static void term_on_media_add(void *user_priv, GF_ClientService *service, GF_Descriptor *media_desc, Bool no_scene_check)
{
	u32 i;
	GF_MediaObject *the_mo;
	GF_InlineScene *is;
	GF_ObjectManager *odm, *root;
	GF_ObjectDescriptor *od;
	GET_TERM();

	root = service->owner;
	is = root->subscene ? root->subscene : root->parentscene;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[Service %s] %s\n", service->url, media_desc ? "Adding new media object" : "Regenerating scene graph"));
	if (!media_desc) {
		if (!no_scene_check) gf_inline_regenerate(is);
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
//	od->objectDescriptorID = GF_ESM_DYNAMIC_OD_ID;

	/*check if we have a mediaObject in the scene not attached and matching this object*/
	the_mo = NULL;
	odm = NULL;
	for (i=0; i<gf_list_count(is->media_objects); i++) {
		char *frag, *ext;
		GF_ESD *esd;
		GF_MediaObject *mo = gf_list_get(is->media_objects, i);
		if (!mo->odm) continue;
		/*already assigned object - this may happen since the compositor has no control on when objects are declared by the service, 
		therefore opening file#video and file#audio may result in the objects being declared twice if the service doesn't
		keep track of declared objects*/
		if (mo->odm->OD) {
			if (is_same_od(mo->odm->OD, od)) {
				/*reassign OD ID*/
				mo->OD_ID = od->objectDescriptorID;
				gf_odf_desc_del(media_desc);
				gf_term_lock_net(term, 0);
				return;
			}
			continue;
		}
		if (mo->OD_ID != GF_ESM_DYNAMIC_OD_ID) {
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
		if (!strstr(service->url, mo->URLs.vals[0].url)) {
			if (ext) ext[0] = '#';
			continue;
		}
		if (ext) ext[0] = '#';

		esd = gf_list_get(od->ESDescriptors, 0);
		/*match type*/
		switch (esd->decoderConfig->streamType) {
		case GF_STREAM_VISUAL:
			if (mo->type != GF_MEDIA_OBJECT_VIDEO) continue;
			break;
		case GF_STREAM_AUDIO:
			if (mo->type != GF_MEDIA_OBJECT_AUDIO) continue;
			break;
		default:
			continue;
		}
		if (frag) {
			u32 frag_id = 0;
			u32 ID = od->objectDescriptorID;
			if (ID==GF_ESM_DYNAMIC_OD_ID) ID = esd->ESID;
			frag++;
			frag_id = atoi(frag);
			if (ID!=frag_id) continue;
		}
		the_mo = mo;
		odm = mo->odm;
		break;
	}

	if (!odm) {
		odm = gf_odm_new();
		odm->term = term;
		odm->parentscene = is;
		gf_list_add(is->ODlist, odm);
	}
	odm->OD = od;
	odm->mo = the_mo;
	if (the_mo) the_mo->OD_ID = od->objectDescriptorID;
	odm->flags |= GF_ODM_NOT_IN_OD_STREAM;
	gf_term_lock_net(term, 0);

	gf_odm_setup_object(odm, service);

	/*OD inserted by service: resetup scene*/
	if (!no_scene_check && is->is_dynamic_scene) gf_inline_regenerate(is);
}

static void term_on_command(void *user_priv, GF_ClientService *service, GF_NetworkCommand *com, GF_Err response)
{
	GF_Channel *ch;
	GET_TERM();

	if (com->command_type==GF_NET_BUFFER_QUERY) {
		GF_List *od_list;
		u32 i;
		GF_ObjectManager *odm;
		com->buffer.max = 0;
		com->buffer.min = com->buffer.occupancy = (u32) -1;
		if (!service->owner) {
			com->buffer.occupancy = 0;
			return;
		}
		
		/*browse all channels in the scene, running on this service, and get buffer info*/
		od_list = NULL;
		if (service->owner->parentscene) {
			od_list = service->owner->parentscene->ODlist;
		} else if (service->owner->subscene) {
			od_list = service->owner->subscene->ODlist;
		}
		if (!od_list) {
			com->buffer.occupancy = 0;
			return;
		}
		/*get exclusive access to media scheduler, to make sure ODs are not being
		manipulated*/
		gf_mx_p(term->mm_mx);
		i=0;
		while ((odm = (GF_ObjectManager*)gf_list_enum(od_list, &i))) {
			u32 j, count;
			count = gf_list_count(odm->channels);
			for (j=0; j<count; j++) {
				GF_Channel *ch = (GF_Channel *)gf_list_get(odm->channels, j);
				if (ch->service != service) continue;
				if (ch->es_state != GF_ESM_ES_RUNNING) continue;
				if (!ch->MaxBuffer || ch->dispatch_after_db || ch->bypass_sl_and_db || ch->IsEndOfStream) continue;
				if (ch->MaxBuffer>com->buffer.max) com->buffer.max = ch->MaxBuffer;
				if (ch->MinBuffer<com->buffer.min) com->buffer.min = ch->MinBuffer;
				if ((ch->AU_Count > 2)  && ((u32) ch->BufferTime<com->buffer.occupancy))
					com->buffer.occupancy = ch->BufferTime;
			}
		}
		gf_mx_v(term->mm_mx);
		if (com->buffer.occupancy==(u32) -1) com->buffer.occupancy = 0;
		return;
	}
	if (com->command_type==GF_NET_SERVICE_INFO) {
		GF_Event evt;
		evt.type = GF_EVENT_METADATA;
		GF_USER_SENDEVENT(term->user, &evt);
		return;
	}

	
	if (!com->base.on_channel) return;


	ch = gf_term_get_channel(service, com->base.on_channel);
	if (!ch) return;

	switch (com->command_type) {
	/*SL reconfiguration*/
	case GF_NET_CHAN_RECONFIG:
		gf_term_lock_net(term, 1);
		gf_es_reconfig_sl(ch, &com->cfg.sl_config);
		gf_term_lock_net(term, 0);
		return;
	/*time mapping (TS to media-time)*/
	case GF_NET_CHAN_MAP_TIME:
		ch->seed_ts = com->map_time.timestamp;
		ch->ts_offset = (u32) (com->map_time.media_time*1000);
		/*
		if (gf_es_owns_clock(ch)) {
			ch->ts_offset = (u32) (com->map_time.media_time*1000);
		} else {
			ch->ts_offset = gf_clock_time(ch->clock);
		}
		*/
		gf_es_map_time(ch, com->map_time.reset_buffers);
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
			com->buffer.occupancy = ch->BufferTime;
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

static void fetch_mime_io(void *dnld, GF_NETIO_Parameter *parameter)
{
	/*this is correct, however some shoutcast servers don't understand HEAD and don't reply at all
	so don't use HEAD*/
#if 0
	/*only get the content type*/
	if (parameter->msg_type==GF_NETIO_GET_METHOD) parameter->name = "HEAD";
#endif


}

static char *get_mime_type(GF_Terminal *term, const char *url, GF_Err *ret_code)
{
	char *mime_type;
	GF_DownloadSession * sess;

	(*ret_code) = GF_OK;
	if (strnicmp(url, "http", 4)) return NULL;

	sess = gf_dm_sess_new(term->downloader, (char *) url, GF_NETIO_SESSION_NOT_THREADED, fetch_mime_io, NULL, ret_code);
	if (!sess) {
		if (strstr(url, "rtsp://") || strstr(url, "rtp://") || strstr(url, "udp://") || strstr(url, "tcp://") ) (*ret_code) = GF_OK;
		return NULL;
	}
	mime_type = (char *) gf_dm_sess_mime_type(sess);
	if (mime_type) mime_type = strdup(mime_type);
	else *ret_code = gf_dm_sess_last_error(sess);
	gf_dm_sess_del(sess);
	return mime_type;
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


static GF_InputService *gf_term_can_handle_service(GF_Terminal *term, const char *url, const char *parent_url, Bool no_mime_check, char **out_url, GF_Err *ret_code)
{
	u32 i;
	GF_Err e;
	char *sURL, *ext, *mime_type;
	char szExt[500];
	GF_InputService *ifce;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Looking for plugin for URL %s\n", url));
	*out_url = NULL;
	if (!url) {
		(*ret_code) = GF_URL_ERROR;
		return NULL;
	}

	sURL = NULL;
	if (parent_url) sURL = gf_url_concatenate(parent_url, url);

	/*path absolute*/
	if (!sURL) sURL = strdup(url);

	if (gf_url_is_local(sURL)) 
		gf_url_to_fs_path(sURL);

	if (no_mime_check) {
		mime_type = NULL;
	} else {
		/*fetch a mime type if any. If error don't even attempt to open the service
		TRYTOFIXME: it would be nice to reuse the downloader created while fetching the mime type, however
		we don't know if the plugin will want it threaded or not....
		*/
		mime_type = get_mime_type(term, sURL, &e);
		if (e) {
			free(sURL);
			(*ret_code) = e;
			return NULL;
		}
	}
	
	if (mime_type && 
		(!stricmp(mime_type, "text/plain") 
			|| !stricmp(mime_type, "video/quicktime")
			|| !stricmp(mime_type, "application/octet-stream")
		) 
	) {
		free(mime_type);
		mime_type = NULL;
	}

	ifce = NULL;

	/*load from mime type*/
	if (mime_type) {
		const char *sPlug = gf_cfg_get_key(term->user->config, "MimeTypes", mime_type);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Mime type found: %s\n", mime_type));
		if (sPlug) sPlug = strrchr(sPlug, '"');
		if (sPlug) {
			sPlug += 2;
			ifce = (GF_InputService *) gf_modules_load_interface_by_name(term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
			if (ifce && !net_check_interface(ifce) ) {
				gf_modules_close_interface((GF_BaseInterface *) ifce);
				ifce = NULL;
			}
		}
	}

	
	ext = strchr(sURL, '#');
	if (ext) {
		char *anext;
		ext[0] = 0;
		anext = strrchr(sURL, '.');
		ext[0] = '#';
		ext = anext;
	} else {
		ext = strrchr(sURL, '.');
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

	if (mime_type) free(mime_type);

	/*browse extensions for prefered module*/
	if (!ifce && ext) {
		u32 keyCount;
		strcpy(szExt, &ext[1]);
		ext = strrchr(szExt, '#');
		if (ext) ext[0] = 0;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] No mime type found - checking by extension %s\n", szExt));
		keyCount = gf_cfg_get_key_count(term->user->config, "MimeTypes");
		for (i=0; i<keyCount; i++) {
			char *sMime, *sPlug;
			const char *sKey;
			sMime = (char *) gf_cfg_get_key_name(term->user->config, "MimeTypes", i);
			if (!sMime) continue;
			sKey = gf_cfg_get_key(term->user->config, "MimeTypes", sMime);
			if (!sKey) continue;
			if (!check_extension(sKey, szExt)) continue;
			sPlug = strrchr(sKey, '"');
			if (!sPlug) continue;	/*bad format entry*/
			sPlug += 2;

			ifce = (GF_InputService *) gf_modules_load_interface_by_name(term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;
			if (!net_check_interface(ifce)) {
				gf_modules_close_interface((GF_BaseInterface *) ifce);
				ifce = NULL;
				continue;
			}
			break;
		}
	}

	/*browse all modules*/
	if (!ifce) {
		for (i=0; i< gf_modules_get_count(term->user->modules); i++) {
			ifce = (GF_InputService *) gf_modules_load_interface(term->user->modules, i, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Checking if module %s supports URL %s\n", ifce->module_name, sURL));
			if (net_check_interface(ifce) && ifce->CanHandleURL(ifce, sURL)) break;
			gf_modules_close_interface((GF_BaseInterface *) ifce);
			ifce = NULL;
		}
	}

	if (!ifce) {
		free(sURL);
		(*ret_code) = GF_NOT_SUPPORTED;
		return NULL;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Terminal] Found input plugin %s for URL %s\n", ifce->module_name, sURL));
	*out_url = sURL;
	return ifce;
}

GF_ClientService *gf_term_service_new(GF_Terminal *term, struct _od_manager *owner, const char *url, GF_ClientService *parent_service, GF_Err *ret_code)
{
	char *sURL;
	GF_ClientService *serv;
	GF_InputService *ifce = gf_term_can_handle_service(term, url, parent_service ? parent_service->url : NULL, 0, &sURL, ret_code);
	if (!ifce) return NULL;

	GF_SAFEALLOC(serv, GF_ClientService);
	serv->term = term;
	serv->owner = owner;
	serv->ifce = ifce;
	serv->url = sURL;
	serv->Clocks = gf_list_new();
	serv->dnloads = gf_list_new();
	gf_list_add(term->net_services, serv);

	return serv;
}

Bool gf_term_is_supported_url(GF_Terminal *term, const char *fileName, Bool use_parent_url, Bool no_mime_check)
{
	GF_InputService *ifce;
	GF_Err e;
	char *sURL;
	char *parent_url = NULL;
	if (use_parent_url && term->root_scene) parent_url = term->root_scene->root_od->net_service->url;

	ifce = gf_term_can_handle_service(term, fileName, parent_url, no_mime_check, &sURL, &e);
	if (!ifce) return 0;
	gf_modules_close_interface((GF_BaseInterface *) ifce);
	free(sURL);
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
	return ns->ifce->ServiceCommand(ns->ifce, com); 
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
void gf_term_on_message(GF_ClientService *service, GF_Err error, const char *message)
{
	assert(service);
	term_on_message(service->term, service, error, message);
}
GF_EXPORT
void gf_term_on_connect(GF_ClientService *service, LPNETCHANNEL ns, GF_Err response)
{
	assert(service);
	term_on_connect(service->term, service, ns, response);
}
GF_EXPORT
void gf_term_on_disconnect(GF_ClientService *service, LPNETCHANNEL ns, GF_Err response)
{
	assert(service);
	term_on_disconnect(service->term, service, ns, response);
}
GF_EXPORT
void gf_term_on_command(GF_ClientService *service, GF_NetworkCommand *com, GF_Err response)
{
	assert(service);
	term_on_command(service->term, service, com, response);
}
GF_EXPORT
void gf_term_on_sl_packet(GF_ClientService *service, LPNETCHANNEL ns, char *data, u32 data_size, GF_SLHeader *hdr, GF_Err reception_status)
{
	assert(service);
	term_on_slp_received(service->term, service, ns, data, data_size, hdr, reception_status);
}

GF_EXPORT
void gf_term_add_media(GF_ClientService *service, GF_Descriptor *media_desc, Bool no_scene_check)
{
	term_on_media_add(service->term, service, media_desc, no_scene_check);
}

GF_EXPORT
const char *gf_term_get_service_url(GF_ClientService *service)
{
	if (!service) return NULL;
	return service->url;
}

void NM_DeleteService(GF_ClientService *ns)
{
	const char *sOpt = gf_cfg_get_key(ns->term->user->config, "StreamingCache", "AutoSave");
	if (ns->cache) gf_term_service_cache_close(ns, (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	gf_modules_close_interface((GF_BaseInterface *)ns->ifce);
	free(ns->url);

	assert(!ns->nb_odm_users);
	assert(!ns->nb_ch_users);
	assert(!ns->owner);

	/*delete all the clocks*/
	while (gf_list_count(ns->Clocks)) {
		GF_Clock *ck = (GF_Clock *)gf_list_get(ns->Clocks, 0);
		gf_list_rem(ns->Clocks, 0);
		gf_clock_del(ck);
	}
	gf_list_del(ns->Clocks);

	assert(!gf_list_count(ns->dnloads));
	gf_list_del(ns->dnloads);
	free(ns);
}

GF_EXPORT
GF_InputService *gf_term_get_service_interface(GF_ClientService *serv)
{
	return serv ? serv->ifce : NULL;
}

GF_EXPORT
GF_DownloadSession *gf_term_download_new(GF_ClientService *service, const char *url, u32 flags, gf_dm_user_io user_io, void *cbk)
{
	GF_Err e;
	GF_DownloadSession * sess;
	char *sURL;
	if (!service || !user_io) return NULL;

	sURL = gf_url_concatenate(service->url, url);
	/*path was absolute*/
	if (!sURL) sURL = strdup(url);
	sess = gf_dm_sess_new(service->term->downloader, sURL, flags, user_io, cbk, &e);
	free(sURL);
	if (!sess) return NULL;
	gf_dm_sess_set_private(sess, service);
	gf_list_add(service->dnloads, sess);
	return sess;
}

GF_EXPORT
void gf_term_download_del(GF_DownloadSession * sess)
{
	GF_ClientService *serv;
	if (!sess) return;
	serv = (GF_ClientService *)gf_dm_sess_get_private(sess);

	/*avoid sending data back to user*/
	gf_dm_sess_abort(sess);
	/*unregister from service*/
	gf_list_del_item(serv->dnloads, sess);

	/*same as service: this may be called in the downloader thread (typically when download fails)
	so we must queue the downloader and let the term delete it later on*/
	gf_list_add(serv->term->net_services_to_remove, sess);
}

GF_EXPORT
void gf_term_download_update_stats(GF_DownloadSession * sess)
{
	GF_ClientService *serv;
	const char *szURI;
	u32 total_size, bytes_done, net_status, bytes_per_sec;
	
	if (!sess) return;

	gf_dm_sess_get_stats(sess, NULL, &szURI, &total_size, &bytes_done, &bytes_per_sec, &net_status);
	serv = (GF_ClientService *)gf_dm_sess_get_private(sess);
	switch (net_status) {
	case GF_NETIO_SETUP:
		gf_term_on_message(serv, GF_OK, "Connecting");
		break;
	case GF_NETIO_CONNECTED:
		gf_term_on_message(serv, GF_OK, "Connected");
		break;
	case GF_NETIO_WAIT_FOR_REPLY:
		gf_term_on_message(serv, GF_OK, "Waiting for reply...");
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
			GF_USER_SENDEVENT(serv->term->user, &evt);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_NETWORK, ("[HTTP] %s received %d / %d\n", szURI, bytes_done, total_size));
		break;
	}
}

void gf_term_service_del(GF_ClientService *ns)
{
	/*this is a downloader session*/
	if (! * (u32 *) ns) {
		gf_dm_sess_del((GF_DownloadSession * ) ns);
	} else {
		NM_DeleteService(ns);
	}
}

GF_EXPORT
void gf_term_register_mime_type(GF_InputService *ifce, const char *mimeType, const char *extList, const char *description)
{
	u32 len;
	char *buf;
	if (!ifce || !mimeType || !extList || !description) return;

	len = strlen(extList) + 3 + strlen(description) + 3 + strlen(ifce->module_name) + 1;
	buf = (char*)malloc(sizeof(char)*len);
	sprintf(buf, "\"%s\" ", extList);
	strlwr(buf);
	strcat(buf, "\"");
	strcat(buf, description);
	strcat(buf, "\" ");
	strcat(buf, ifce->module_name);
	gf_modules_set_option((GF_BaseInterface *)(GF_BaseInterface *)ifce, "MimeTypes", mimeType, buf);
	free(buf);
}

GF_EXPORT
Bool gf_term_check_extension(GF_InputService *ifce, const char *mimeType, const char *extList, const char *description, const char *fileExt)
{
	const char *szExtList;
	char *ext, szExt[500];
	if (!ifce || !mimeType || !extList || !description || !fileExt) return 0;
	/*this is a URL*/
	if ( (strlen(fileExt)>20) || strchr(fileExt, '/')) return 0;

	if (fileExt[0]=='.') fileExt++;
	strcpy(szExt, fileExt);
	strlwr(szExt);
	ext = strchr(szExt, '#');
	if (ext) ext[0]=0;

	szExtList = gf_modules_get_option((GF_BaseInterface *)(GF_BaseInterface *)ifce, "MimeTypes", mimeType);
	if (!szExtList) {
		gf_term_register_mime_type(ifce, mimeType, extList, description);
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
			case '/': case '\\': case '.': case ':': case '?': 
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
