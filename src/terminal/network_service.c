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
		char *sOpt = gf_cfg_get_key(term->user->config, "Network", "AutoReconfigUDP");
		if (sOpt && !stricmp(sOpt, "yes")) {
			sOpt = gf_cfg_get_key(term->user->config, "Network", "UDPNotAvailable");
			/*if option is already set don't bother try reconfig*/
			if (!sOpt || stricmp(sOpt, "yes")) {
				char szMsg[1024];
				sprintf(szMsg, "UDP down (%s) - Retrying with TCP", message);
				gf_term_message(term, service->url, szMsg, GF_OK);
				/*reconnect top-level*/
				sOpt = strdup(term->root_scene->root_od->net_service->url);
				gf_term_disconnect(term);
				gf_cfg_set_key(term->user->config, "Network", "UDPNotAvailable", "yes");
				gf_term_connect(term, sOpt);
				free(sOpt);
				return;
			}
		}
	}
	gf_term_message(term, service->url, message, error);
}

static void term_on_connect(void *user_priv, GF_ClientService *service, LPNETCHANNEL netch, GF_Err err)
{
	GF_Channel *ch;
	GF_ObjectManager *root;
	GET_TERM();

	root = service->owner;
	if (root && (root->net_service != service)) {
		gf_term_message(term, service->url, "Incomaptible module type", GF_SERVICE_ERROR);
		return;
	}
	/*this is service connection*/
	if (!netch) {
		if (err) {
			char msg[5000];
			sprintf(msg, "Cannot open %s", service->url);
			gf_term_message(term, service->url, msg, err);

			/*destroy service only if attached*/
			if (root) {
				gf_term_lock_net(term, 1);
				root->net_service = NULL;
				gf_list_del_item(term->net_services, service);
				/*and queue for destroy*/
				gf_list_add(term->net_services_to_remove, service);
				gf_term_lock_net(term, 0);
				if (!root->parentscene) {
					GF_Event evt;
					evt.type = GF_EVT_CONNECT;
					evt.connect.is_connected = 0;
					GF_USER_SENDEVENT(term->user, &evt);
				}
				return;
			}
		}

		if (!root) {
			/*channel service connect*/
			u32 i;
			GF_List *ODs = gf_list_new();
			gf_term_lock_net(term, 1);
			for (i=0; i<gf_list_count(term->channels_pending); i++) {
				GF_ChannelSetup *cs = gf_list_get(term->channels_pending, i);
				if (cs->ch->service != service) continue;
				gf_list_rem(term->channels_pending, i);
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
				GF_ObjectManager *odm = gf_list_get(ODs, 0);
				gf_list_rem(ODs, 0);
				/*force re-setup*/
				gf_is_setup_object(odm->parentscene, odm);
			}
			gf_list_del(ODs);
		} else {
			/*setup od*/
			gf_odm_setup_entry_point(root, NULL);
		}
		/*load cache if requested*/
		if (!err && term->enable_cache) {
			err = gf_term_service_cache_load(service);
			/*not a fatal error*/
			if (err) gf_term_message(term, "GPAC Cache", "Cannot load cache", err);
		}
	}

	ch = gf_term_get_channel(service, netch);
	if (!ch) return;

	/*this is channel connection*/
	if (err) {
		gf_term_message(term, service->url, "Channel Connection Failed", err);
		ch->es_state = GF_ESM_ES_UNAVAILABLE;
		return;
	}

	gf_term_lock_net(term, 1);
	gf_es_on_connect(ch);
	gf_term_lock_net(term, 0);

	/*in case the OD user has requested a play send a PLAY on the object (Play request are skiped
	until all channels are connected) */
	if (ch->odm->mo && ch->odm->mo->num_open) {
		gf_odm_start(ch->odm);
	}
	/*if this is a channel in the root OD play */
	else if (! ch->odm->parentscene) {
		gf_odm_start(ch->odm);
	}
}

static void term_on_disconnect(void *user_priv, GF_ClientService *service, LPNETCHANNEL netch, GF_Err response)
{
	GF_ObjectManager *root;
	GF_Channel *ch;
	GET_TERM();

	/*may be null upon destroy*/
	root = service->owner;
	if (root && (root->net_service != service)) {
		gf_term_message(term, service->url, "Incompatible module type", GF_SERVICE_ERROR);
		return;
	}
	/*this is service disconnect*/
	if (!netch) {
		gf_term_lock_net(term, 1);
		/*unregister from valid services*/
		gf_list_del_item(term->net_services, service);
		/*and queue for destroy*/
		gf_list_add(term->net_services_to_remove, service);
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

static void term_on_slp_recieved(void *user_priv, GF_ClientService *service, LPNETCHANNEL netch, char *data, u32 data_size, SLHeader *hdr, GF_Err reception_status)
{
	GF_Channel *ch;
	GET_TERM();

	ch = gf_term_get_channel(service, netch);
	if (!ch) return;
	
	if (reception_status==GF_EOS) {
		gf_es_on_eos(ch);
		return;
	}
	/*otherwise dispatch with error code*/
	gf_es_receive_sl_packet(service, ch, data, data_size, hdr, reception_status);
}


static void term_on_command(void *user_priv, GF_ClientService *service, GF_NetworkCommand *com, GF_Err response)
{
	GF_Channel *ch;
	GET_TERM();

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
		if (Channel_OwnsClock(ch)) {
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
		com->buffer.max = ch->MaxBuffer;
		com->buffer.min = ch->MinBuffer;
		com->buffer.occupancy = ch->BufferTime;
		break;
	case GF_NET_CHAN_ISMACRYP_CFG:
		gf_term_lock_net(term, 1);
		gf_es_config_ismacryp(ch, &com->isma_cryp);
		gf_term_lock_net(term, 0);
		return;
	case GF_NET_CHAN_GET_ESD:
		gf_term_lock_net(term, 1);
		com->cache_esd.esd = ch->esd;
		com->cache_esd.is_iod_stream = (ch->odm->subscene && (ch->odm->subscene->root_od==ch->odm)) ? 1 : 0;
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

void NM_OnMimeData(void *dnld, char *data, u32 size, u32 state, GF_Err e)
{
}

char *NM_GetMimeType(GF_Terminal *term, const char *url, GF_Err *ret_code)
{
	char *mime_type;
	GF_DownloadSession * sess;

	(*ret_code) = GF_OK;
	sess = gf_dm_new_session(term->downloader, (char *) url, GF_DOWNLOAD_SESSION_NOT_THREADED, NM_OnMimeData, NULL, NULL, ret_code);
	if (!sess) {
		if (strstr(url, "rtsp://")) (*ret_code) = GF_OK;
		else if (strstr(url, "rtp://")) (*ret_code) = GF_OK;
		return NULL;
	}
	mime_type = (char *) gf_dm_get_mime_type(sess);
	if (mime_type) mime_type = strdup(mime_type);
	else *ret_code = gf_dm_get_last_error(sess);
	gf_dm_free_session(sess);
	return mime_type;
}


static Bool check_extension(char *szExtList, char *szExt)
{
	char szExt2[50];
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

GF_ClientService *gf_term_service_new(GF_Terminal *term, struct _od_manager *owner, const char *url, GF_ClientService *parent_service, GF_Err *ret_code)
{
	GF_ClientService *serv;
	u32 i;
	GF_Err e;
	char *sURL, *ext, *mime_type;
	char szExt[50];
	GF_InputService *ifce;

	if (!url) {
		(*ret_code) = GF_URL_ERROR;
		return NULL;
	}

	sURL = NULL;
	if (parent_service && parent_service->url) {
		sURL = gf_url_concatenate(parent_service->url, url);
	}
	/*path absolute*/
	if (!sURL) {
		char *tmp = (char *) url;
		if (!strnicmp(url, "file:///", 8)) tmp += 8;
		else if (!strnicmp(url, "file://", 7)) tmp += 7;
		sURL = strdup(tmp);
	}

	/*fetch a mime type if any. If error don't even attempt to open the service*/
	mime_type = NM_GetMimeType(term, sURL, &e);
	if (e) {
		free(sURL);
		(*ret_code) = e;
		return NULL;
	}
	
	if (mime_type && (!stricmp(mime_type, "text/plain") || !stricmp(mime_type, "video/quicktime")) ) {
		free(mime_type);
		mime_type = NULL;
	}

	ifce = NULL;

	/*load from mime type*/
	if (mime_type) {
		char *sPlug = gf_cfg_get_key(term->user->config, "MimeTypes", mime_type);
		if (sPlug) sPlug = strrchr(sPlug, '"');
		if (sPlug) {
			sPlug += 2;
			gf_modules_load_interface_by_name(term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE, (void **) &ifce);
			if (ifce && !net_check_interface(ifce) ) {
				gf_modules_close_interface(ifce);
				ifce = NULL;
			}
		}
	}

	ext = strrchr(sURL, '.');
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

		keyCount = gf_cfg_get_key_count(term->user->config, "MimeTypes");
		for (i=0; i<keyCount; i++) {
			char *sMime, *sKey, *sPlug;
			sMime = (char *) gf_cfg_get_key_name(term->user->config, "MimeTypes", i);
			if (!sMime) continue;
			sKey = gf_cfg_get_key(term->user->config, "MimeTypes", sMime);
			if (!sKey) continue;
			if (!check_extension(sKey, szExt)) continue;
			sPlug = strrchr(sKey, '"');
			if (!sPlug) continue;	/*bad format entry*/
			sPlug += 2;

			gf_modules_load_interface_by_name(term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE, (void **) &ifce);
			if (!ifce) continue;
			if (!net_check_interface(ifce)) {
				gf_modules_close_interface(ifce);
				ifce = NULL;
				continue;
			}
			break;
		}
	}

	/*browse all modules*/
	if (!ifce) {
		for (i=0; i< gf_modules_get_count(term->user->modules); i++) {
			gf_modules_load_interface(term->user->modules, i, GF_NET_CLIENT_INTERFACE, (void **) &ifce);
			if (!ifce) continue;
			if (net_check_interface(ifce) && ifce->CanHandleURL(ifce, sURL)) break;
			gf_modules_close_interface(ifce);
			ifce = NULL;
		}
	}

	if (!ifce) {
		free(sURL);
		(*ret_code) = GF_NOT_SUPPORTED;
		return NULL;
	}
	serv = malloc(sizeof(GF_ClientService));
	memset(serv, 0, sizeof(GF_ClientService));
	serv->term = term;
	serv->owner = owner;
	serv->ifce = ifce;
	serv->url = sURL;
	serv->Clocks = gf_list_new();
	serv->dnloads = gf_list_new();
	gf_list_add(term->net_services, serv);

	return serv;
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
GF_Err gf_term_channel_get_sl_packet(GF_ClientService *ns, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	if (!ns->ifce->ChannelGetSLP) return GF_NOT_SUPPORTED;
	return ns->ifce->ChannelGetSLP(ns->ifce, channel, out_data_ptr, out_data_size, out_sl_hdr, sl_compressed, out_reception_status, is_new_data);
}
GF_Err gf_term_channel_release_sl_packet(GF_ClientService *ns, LPNETCHANNEL channel)
{
	if (!ns->ifce->ChannelGetSLP) return GF_NOT_SUPPORTED;
	return ns->ifce->ChannelReleaseSLP(ns->ifce, channel);
}

void gf_term_on_message(GF_ClientService *service, GF_Err error, const char *message)
{
	assert(service);
	term_on_message(service->term, service, error, message);
}
void gf_term_on_connect(GF_ClientService *service, LPNETCHANNEL ns, GF_Err response)
{
	assert(service);
	term_on_connect(service->term, service, ns, response);
}
void gf_term_on_disconnect(GF_ClientService *service, LPNETCHANNEL ns, GF_Err response)
{
	assert(service);
	term_on_disconnect(service->term, service, ns, response);
}
void gf_term_on_command(GF_ClientService *service, GF_NetworkCommand *com, GF_Err response)
{
	assert(service);
	term_on_command(service->term, service, com, response);
}
void gf_term_on_sl_packet(GF_ClientService *service, LPNETCHANNEL ns, char *data, u32 data_size, SLHeader *hdr, GF_Err reception_status)
{
	assert(service);
	term_on_slp_recieved(service->term, service, ns, data, data_size, hdr, reception_status);
}

const char *gf_term_get_service_url(GF_ClientService *service)
{
	if (!service) return NULL;
	return service->url;
}

void NM_DeleteService(GF_ClientService *ns)
{
	char *sOpt = gf_cfg_get_key(ns->term->user->config, "StreamingCache", "AutoSave");
	if (ns->cache) gf_term_service_cache_close(ns, (sOpt && !stricmp(sOpt, "yes")) ? 1 : 0);
	gf_modules_close_interface(ns->ifce);
	free(ns->url);

	/*delete all the clocks*/
	while (gf_list_count(ns->Clocks)) {
		GF_Clock *ck = gf_list_get(ns->Clocks, 0);
		gf_list_rem(ns->Clocks, 0);
		gf_clock_del(ck);
	}
	gf_list_del(ns->Clocks);

	assert(!gf_list_count(ns->dnloads));
	gf_list_del(ns->dnloads);
	free(ns);
}


GF_DownloadSession * gf_term_download_new(GF_ClientService *service, const char *url, u32 flags, void (*OnData)(void *cbk, char *data, u32 data_size, u32 state, GF_Err error), void *cbk)
{
	GF_Err e;
	GF_DownloadSession * sess;
	char *sURL;
	if (!service || !OnData) return NULL;

	sURL = gf_url_concatenate(service->url, url);
	/*path was absolute*/
	if (!sURL) sURL = strdup(url);
	sess = gf_dm_new_session(service->term->downloader, sURL, flags, OnData, cbk, service, &e);
	free(sURL);
	if (!sess) return NULL;
	gf_list_add(service->dnloads, sess);
	return sess;
}

void gf_term_download_del(GF_DownloadSession * sess)
{
	GF_ClientService *serv;
	if (!sess) return;
	serv = gf_dm_get_private_data(sess);

	/*avoid sending data back to user*/
	gf_dm_abort(sess);
	/*unregister from service*/
	gf_list_del_item(serv->dnloads, sess);

	/*same as service: this may be called in the downloader thread (typically when download fails)
	so we must queue the downloader and let the term delete it later on*/
	gf_list_add(serv->term->net_services_to_remove, sess);
}

void gf_term_download_update_stats(GF_DownloadSession * sess)
{
	GF_ClientService *serv;
	char sMsg[1024];
	Float perc;
	u32 total_size, bytes_done, net_status, bytes_per_sec;
	
	gf_dm_get_stats(sess, NULL, NULL, &total_size, &bytes_done, &bytes_per_sec, &net_status);
	serv = gf_dm_get_private_data(sess);
	switch (net_status) {
	case GF_DOWNLOAD_STATE_SETUP:
		gf_term_on_message(serv, GF_OK, "Connecting");
		break;
	case GF_DOWNLOAD_STATE_CONNECTED:
		gf_term_on_message(serv, GF_OK, "Connected");
		break;
	case GF_DOWNLOAD_STATE_WAIT_FOR_REPLY:
		gf_term_on_message(serv, GF_OK, "Waiting for reply...");
		break;
	case GF_DOWNLOAD_STATE_RUNNING:
		/*notify some connection / ...*/
		if (total_size) {
			perc = (Float) (100 * bytes_done) / (Float) total_size;
			sprintf(sMsg, "Download %.2f %% (%.2f kBps)", perc, ((Float)bytes_per_sec)/1024.0f);
			gf_term_on_message(serv, GF_OK, sMsg);
		}
		break;
	}
}

void gf_term_service_del(GF_ClientService *ns)
{
	/*this is a downloader session*/
	if (! * (u32 *) ns) {
		gf_dm_free_session((GF_DownloadSession * ) ns);
	} else {
		NM_DeleteService(ns);
	}
}

void gf_term_register_mime_type(GF_InputService *ifce, const char *mimeType, const char *extList, const char *description)
{
	u32 len;
	char *buf;
	if (!ifce || !mimeType || !extList || !description) return;

	len = strlen(extList) + 3 + strlen(description) + 3 + strlen(ifce->module_name) + 1;
	buf = malloc(sizeof(char)*len);
	sprintf(buf, "\"%s\" ", extList);
	strlwr(buf);
	strcat(buf, "\"");
	strcat(buf, description);
	strcat(buf, "\" ");
	strcat(buf, ifce->module_name);
	gf_modules_set_option(ifce, "MimeTypes", mimeType, buf);
	free(buf);
}

Bool gf_term_check_extension(GF_InputService *ifce, const char *mimeType, const char *extList, const char *description, const char *fileExt)
{
	char *szExtList, szExt[50];
	if (!ifce || !mimeType || !extList || !description || !fileExt) return 0;
	if (fileExt[0]=='.') fileExt++;
	strcpy(szExt, fileExt);
	strlwr(szExt);
	szExtList = strchr(szExt, '#');
	if (szExtList) szExtList[0]=0;


	szExtList = gf_modules_get_option(ifce, "MimeTypes", mimeType);
	if (!szExtList) {
		gf_term_register_mime_type(ifce, mimeType, extList, description);
		szExtList = gf_modules_get_option(ifce, "MimeTypes", mimeType);
	}
	if (!strstr(szExtList, ifce->module_name)) return 0;
	return check_extension(szExtList, szExt);
}

GF_Err gf_term_service_cache_load(GF_ClientService *ns)
{
	GF_Err e;
	char *sOpt, szName[GF_MAX_PATH], szURL[1024];
	GF_NetworkCommand com;
	u32 i;
	GF_StreamingCache *mcache = NULL;

	/*is service cachable*/
	com.base.on_channel = NULL;
	com.base.command_type = GF_NET_IS_CACHABLE;
	if (ns->ifce->ServiceCommand(ns->ifce, &com) != GF_OK) return GF_OK;

	/*locate a cache*/
	for (i=0; i< gf_modules_get_count(ns->term->user->modules); i++) {
		gf_modules_load_interface(ns->term->user->modules, i, GF_STREAMING_MEDIA_CACHE, (void **) &mcache);
		if (mcache && mcache->Open && mcache->Close && mcache->Write && mcache->ChannelGetSLP && mcache->ChannelReleaseSLP && mcache->ServiceCommand) break;
		if (mcache) gf_modules_close_interface(mcache);
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
		sOpt = strrchr(szURL, '.');
		if (sOpt) sOpt[0] = 0;
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
		gf_modules_close_interface(mcache);
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

	gf_modules_close_interface(ns->cache);
	ns->cache = NULL;
	return e;
}
