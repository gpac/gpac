/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato, Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-
 *					All rights reserved
 *
 *  This file is part of GPAC / 3GPP/MPEG Media Presentation Description input module
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

#include <gpac/modules/service.h>

#ifndef GPAC_DISABLE_DASH_CLIENT

#include <gpac/dash.h>
#include <gpac/internal/terminal_dev.h>

typedef struct __mpd_module 
{
    /* GPAC Service object (i.e. how this module is seen by the terminal)*/
    GF_ClientService *service;
	GF_InputService *plug;
	
	GF_DashClient *dash;

	/*interface to mpd parser*/
	GF_DASHFileIO dash_io;

	Bool connection_ack_sent;
	Bool in_seek;
	Bool memory_storage;
	Bool use_max_res, immediate_switch, allow_http_abort, enable_buffering;
	u32 use_low_latency;
	Double previous_start_range;
	/*max width & height in all active representations*/
	u32 width, height;
} GF_MPD_In;

typedef struct 
{
	GF_MPD_In *mpdin;
	GF_InputService *segment_ifce;
	Bool service_connected;
	Bool service_descriptor_fetched;
	Bool netio_assigned;
	Bool has_new_data;
	u32 idx;
	GF_DownloadSession *sess;
} GF_MPDGroup;

const char * MPD_MPD_DESC = "MPEG-DASH Streaming";
const char * MPD_MPD_EXT = "3gm mpd";
const char * MPD_M3U8_DESC = "Apple HLS Streaming";
const char * MPD_M3U8_EXT = "m3u8 m3u";

static u32 MPD_RegisterMimeTypes(const GF_InputService *plug)
{
    u32 i, c;
    for (i = 0 ; GF_DASH_MPD_MIME_TYPES[i]; i++)
        gf_term_register_mime_type (plug, GF_DASH_MPD_MIME_TYPES[i], MPD_MPD_EXT, MPD_MPD_DESC);
    c = i;
    for (i = 0 ; GF_DASH_M3U8_MIME_TYPES[i]; i++)
        gf_term_register_mime_type(plug, GF_DASH_M3U8_MIME_TYPES[i], MPD_M3U8_EXT, MPD_M3U8_DESC);
    return c+i;
}

Bool MPD_CanHandleURL(GF_InputService *plug, const char *url)
{
    u32 i;
	char *sExt;
    if (!plug || !url)
      return 0;
    sExt = strrchr(url, '.');
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Can Handle URL request from terminal for %s\n", url));
    for (i = 0 ; GF_DASH_MPD_MIME_TYPES[i]; i++) {
        if (gf_term_check_extension(plug, GF_DASH_MPD_MIME_TYPES[i], MPD_MPD_EXT, MPD_MPD_DESC, sExt))
            return 1;
    }
    for (i = 0 ; GF_DASH_M3U8_MIME_TYPES[i]; i++) {
        if (gf_term_check_extension(plug, GF_DASH_M3U8_MIME_TYPES[i], MPD_M3U8_EXT, MPD_M3U8_DESC, sExt))
            return 1;
    }

	return gf_dash_check_mpd_root_type(url);
}

static void MPD_NotifyData(GF_MPDGroup *group, Bool chunk_flush)
{
	GF_NetworkCommand com;
	memset(&com, 0, sizeof(GF_NetworkCommand));
	com.base.command_type = chunk_flush ? GF_NET_SERVICE_PROXY_CHUNK_RECEIVE : GF_NET_SERVICE_PROXY_SEGMENT_RECEIVE;
	group->segment_ifce->ServiceCommand(group->segment_ifce, &com);
}

static GF_Err MPD_ClientQuery(GF_InputService *ifce, GF_NetworkCommand *param)
{
	u32 i;
	GF_Err e;
	GF_MPD_In *mpdin = (GF_MPD_In *) ifce->proxy_udta;
    if (!param || !ifce || !ifce->proxy_udta) return GF_BAD_PARAM;

	/*gets byte range of init segment (for local validation)*/
	if (param->command_type==GF_NET_SERVICE_QUERY_INIT_RANGE) {
		param->url_query.next_url = NULL;
		param->url_query.start_range = 0;
		param->url_query.end_range = 0;
		
		mpdin->in_seek = 0;

		for (i=0; i<gf_dash_get_group_count(mpdin->dash); i++) {
			GF_MPDGroup *group;
			if (!gf_dash_is_group_selected(mpdin->dash, i)) continue;
			group = gf_dash_get_group_udta(mpdin->dash, i);
			if (group->segment_ifce == ifce) {
				gf_dash_group_get_segment_init_url(mpdin->dash, i, &param->url_query.start_range, &param->url_query.end_range);
				return GF_OK;
			}
		}		
		return GF_SERVICE_ERROR;
	}

	/*gets URL and byte range of next segment - if needed, adds bitstream switching segment info*/
	if (param->command_type==GF_NET_SERVICE_QUERY_NEXT) {
		Bool group_done;
		u32 nb_segments_cached;
		u32 group_idx=0;
		GF_MPDGroup *group=NULL;
		const char *src_url;
		Bool discard_first_cache_entry = param->url_query.drop_first_segment;
        u32 timer = gf_sys_clock();
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Service Query Next request from input service %s\n", ifce->module_name));


		param->url_query.discontinuity_type = 0;
		if (mpdin->in_seek) {
			mpdin->in_seek = 0;
			param->url_query.discontinuity_type = 2;
			discard_first_cache_entry = 0;
		}

		for (i=0; i<gf_dash_get_group_count(mpdin->dash); i++) {
			if (!gf_dash_is_group_selected(mpdin->dash, i)) continue;
			group = gf_dash_get_group_udta(mpdin->dash, i);
			if (group->segment_ifce == ifce) {
				group_idx = i;
				break;
			}
			group=NULL;
		}		
		
		if (!group) {
			return GF_SERVICE_ERROR;
		}
		//update group idx
		if (group->idx != group_idx) {
			group->idx = group_idx;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] New AdaptationSet detected after MPD update ?\n"));
		}

		if (discard_first_cache_entry) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Discarding first segment in cache\n"));
			gf_dash_group_discard_segment(mpdin->dash, group_idx);
		}

		while (gf_dash_is_running(mpdin->dash) ) {
			group_done=0;
			nb_segments_cached = gf_dash_group_get_num_segments_ready(mpdin->dash, group_idx, &group_done);
			if (nb_segments_cached>=1) 
				break;

			if (group_done) {
				if (!gf_dash_get_period_switch_status(mpdin->dash) && !gf_dash_in_last_period(mpdin->dash) ) {
					GF_NetworkCommand com;
					memset(&com, 0, sizeof(GF_NetworkCommand));
					com.command_type = GF_NET_BUFFER_QUERY;
					while (gf_dash_get_period_switch_status(mpdin->dash) != 1) {
						gf_term_on_command(mpdin->service, &com, GF_OK);
						if (!com.buffer.occupancy) {
							gf_dash_request_period_switch(mpdin->dash);
							break;
						}
						gf_sleep(30);
					}
				} 
				return GF_EOS;
			}

			if (mpdin->use_low_latency) {
				Bool is_switched=GF_FALSE;
				gf_dash_group_probe_current_download_segment_location(mpdin->dash, group_idx, &param->url_query.next_url, NULL, &param->url_query.next_url_init_or_switch_segment, &src_url, &is_switched);

				if (param->url_query.next_url) {
					param->url_query.is_current_download = 1;
					param->url_query.has_new_data = group->has_new_data;
					param->url_query.discontinuity_type = is_switched ? 1 : 0;
					group->has_new_data = 0;
					return GF_OK;
				}
	            return GF_BUFFER_TOO_SMALL;
			}
			param->url_query.is_current_download = 0;

            return GF_BUFFER_TOO_SMALL;
        }
	
		param->url_query.is_current_download = 0;
		nb_segments_cached = gf_dash_group_get_num_segments_ready(mpdin->dash, group_idx, &group_done);
        if (nb_segments_cached < 1) {
            GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[MPD_IN] No more file in cache, EOS\n"));
            return GF_EOS;
        } else {
            GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Had to wait for %u ms for the only cache file to be downloaded\n", (gf_sys_clock() - timer)));
        }

		e = gf_dash_group_get_next_segment_location(mpdin->dash, group_idx, param->url_query.dependent_representation_index, &param->url_query.next_url, &param->url_query.start_range, &param->url_query.end_range, 
								NULL, &param->url_query.next_url_init_or_switch_segment, &param->url_query.switch_start_range , &param->url_query.switch_end_range,
								&src_url, &param->url_query.has_next);
		if (e)
			return e;

        {
            u32 timer2 = gf_sys_clock() - timer ;
            if (timer2 > 1000) {
                GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Waiting for download to end took a long time : %u ms\n", timer2));
            }
			if (param->url_query.end_range) {
				GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[MPD_IN] Switching segment playback to %s (Media Range: "LLD"-"LLD") at UTC "LLU" ms\n", src_url, param->url_query.start_range, param->url_query.end_range, gf_net_get_utc() ));
			} else {
	            GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[MPD_IN] Switching segment playback to %s at UTC "LLU" ms\n", src_url, gf_net_get_utc()));
			}
            GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[MPD_IN] segment start time %g sec\n", gf_dash_group_current_segment_start_time(mpdin->dash, group_idx) ));

			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Waited %d ms - Elements in cache: %u/%u\n\tCache file name %s\n", timer2, gf_dash_group_get_num_segments_ready(mpdin->dash, group_idx, &group_done), gf_dash_group_get_max_segments_in_cache(mpdin->dash, group_idx), param->url_query.next_url ));
        }    
    }


    return GF_OK;
}

/*locates input service (demuxer) based on mime type or segment name*/
static GF_Err MPD_LoadMediaService(GF_MPD_In *mpdin, u32 group_index, const char *mime, const char *init_segment_name)
{
	GF_InputService *segment_ifce;
	u32 i;
    const char *sPlug;
	if (mime) {
		/* Check MIME type to start the right InputService */
		sPlug = gf_cfg_get_key(mpdin->service->term->user->config, "MimeTypes", mime);
		if (sPlug) sPlug = strrchr(sPlug, '"');
		if (sPlug) {
			sPlug += 2;
			segment_ifce = (GF_InputService *) gf_modules_load_interface_by_name(mpdin->service->term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
			if (segment_ifce) {
				GF_MPDGroup *group;
				GF_SAFEALLOC(group, GF_MPDGroup);
				group->segment_ifce = segment_ifce;
				group->segment_ifce->proxy_udta = mpdin;
				group->segment_ifce->query_proxy = MPD_ClientQuery;
				group->mpdin = mpdin;
				group->idx = group_index;
				gf_dash_set_group_udta(mpdin->dash, group_index, group);
				return GF_OK;
			}
		}
	}
	if (init_segment_name) {
		for (i=0; i< gf_modules_get_count(mpdin->service->term->user->modules); i++) {
			GF_InputService *ifce = (GF_InputService *) gf_modules_load_interface(mpdin->service->term->user->modules, i, GF_NET_CLIENT_INTERFACE);
			if (!ifce) continue;
			
			if (ifce->CanHandleURL && ifce->CanHandleURL(ifce, init_segment_name)) {
				GF_MPDGroup *group;
				GF_SAFEALLOC(group, GF_MPDGroup);
				group->segment_ifce = ifce;
				group->segment_ifce->proxy_udta = mpdin;
				group->segment_ifce->query_proxy = MPD_ClientQuery;
				group->mpdin = mpdin;
				group->idx = group_index;
				gf_dash_set_group_udta(mpdin->dash, group_index, group);
				return GF_OK;
			}
			gf_modules_close_interface((GF_BaseInterface *) ifce);
		}
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD_IN] Error locating plugin for segment - mime type %s - name %s\n", mime, init_segment_name));
    return GF_CODEC_NOT_FOUND;
}



GF_InputService *MPD_GetInputServiceForChannel(GF_MPD_In *mpdin, LPNETCHANNEL channel)
{
	GF_Channel *ch;
	if (!channel) {
		if (gf_dash_is_group_selected(mpdin->dash, 0)) {
			GF_MPDGroup *mudta = gf_dash_get_group_udta(mpdin->dash, 0);
			return mudta ? mudta->segment_ifce : NULL;
		}
		return NULL;
	}

	ch = (GF_Channel *) channel;
	assert(ch->odm && ch->odm->OD);
	return (GF_InputService *) ch->odm->OD->service_ifce;
}

s32 MPD_GetGroupIndexForChannel(GF_MPD_In *mpdin, LPNETCHANNEL channel)
{
	u32 i;
	GF_InputService *ifce = MPD_GetInputServiceForChannel(mpdin, channel);
	if (!ifce) return -1;

	for (i=0; i<gf_dash_get_group_count(mpdin->dash); i++) {
		GF_MPDGroup *group = gf_dash_get_group_udta(mpdin->dash, i);
		if (!group) continue;
		if (group->segment_ifce == ifce) return i;
	}
	return -1;
}

GF_Err MPD_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Channel Connection (%p) request from terminal for %s\n", channel, url));
	return segment_ifce->ConnectChannel(segment_ifce, channel, url, upstream);
}

GF_Err MPD_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Disconnect channel (%p) request from terminal \n", channel));

	return segment_ifce->DisconnectChannel(segment_ifce, channel);
}


static void mpdin_dash_segment_netio(void *cbk, GF_NETIO_Parameter *param)
{
	GF_MPDGroup *group = (GF_MPDGroup *)cbk;

	if (param->msg_type == GF_NETIO_PARSE_HEADER) {
		if (!strcmp(param->name, "Dash-Newest-Segment")) {
			gf_dash_resync_to_segment(group->mpdin->dash, param->value, gf_dm_sess_get_header(param->sess, "Dash-Oldest-Segment") );
		}
	}

	if (param->msg_type == GF_NETIO_DATA_EXCHANGE) {
		group->has_new_data = 1;

		if (param->reply) {
			u32 bytes_per_sec;
			const char *url;
			gf_dm_sess_get_stats(group->sess, NULL, &url, NULL, NULL, &bytes_per_sec, NULL);
			GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[MPD_IN] End of chunk received for %s at UTC "LLU" ms - estimated bandwidth %d kbps - chunk start at UTC "LLU"\n", url, gf_net_get_utc(), 8*bytes_per_sec/1000, gf_dm_sess_get_utc_start(group->sess)));

			if (group->mpdin->use_low_latency) 
				MPD_NotifyData(group, 1);
		} else if (group->mpdin->use_low_latency==2) {
			MPD_NotifyData(group, 1);
		}
	
		if (group->mpdin->allow_http_abort)
			gf_dash_group_check_bandwidth(group->mpdin->dash, group->idx);
	}
	if (param->msg_type == GF_NETIO_DATA_TRANSFERED) {
		u32 bytes_per_sec;
		const char *url;
		u64 start_time = gf_dm_sess_get_utc_start(group->sess);
		gf_dm_sess_get_stats(group->sess, NULL, &url, NULL, NULL, &bytes_per_sec, NULL);
		GF_LOG(GF_LOG_INFO, GF_LOG_DASH, ("[MPD_IN] End of file %s download at UTC "LLU" ms - estimated bandwidth %d kbps - started file or last chunk at UTC "LLU"\n", url, gf_net_get_utc(), 8*bytes_per_sec/1000, gf_dm_sess_get_utc_start(group->sess)));
	}
}

void mpdin_dash_io_delete_cache_file(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *cache_url)
{
	gf_dm_delete_cached_file_entry_session((GF_DownloadSession *)session, cache_url);
}

GF_DASHFileIOSession mpdin_dash_io_create(GF_DASHFileIO *dashio, Bool persistent, const char *url, s32 group_idx)
{
	GF_MPDGroup *group = NULL;
	GF_DownloadSession *sess;
	u32 flags = GF_NETIO_SESSION_NOT_THREADED;
	GF_MPD_In *mpdin = (GF_MPD_In *)dashio->udta;
	if (mpdin->memory_storage)
		flags |= GF_NETIO_SESSION_MEMORY_CACHE;

	if (persistent) flags |= GF_NETIO_SESSION_PERSISTENT;

	if (group_idx>=0) {
		group = gf_dash_get_group_udta(mpdin->dash, group_idx);
	}
	if (group) {
		group->netio_assigned = GF_TRUE;
		group->sess = sess = gf_term_download_new(mpdin->service, url, flags, mpdin_dash_segment_netio, group);
	} else {
		sess = gf_term_download_new(mpdin->service, url, flags, NULL, NULL);
	}
	return (GF_DASHFileIOSession ) sess;
}
void mpdin_dash_io_del(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	gf_term_download_del((GF_DownloadSession *)session);
}
void mpdin_dash_io_abort(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
    gf_dm_sess_abort((GF_DownloadSession *)session);
}
GF_Err mpdin_dash_io_setup_from_url(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *url, s32 group_idx)
{
	if (group_idx>=0) {
		GF_MPD_In *mpdin = (GF_MPD_In *)dashio->udta;
		GF_MPDGroup *group = gf_dash_get_group_udta(mpdin->dash, group_idx);
		if (group && !group->netio_assigned) {
			group->netio_assigned = GF_TRUE;
			group->sess = (GF_DownloadSession *)session;
			gf_dm_sess_reassign((GF_DownloadSession *)session, 0xFFFFFFFF, mpdin_dash_segment_netio, group);
		}
	}
	return gf_dm_sess_setup_from_url((GF_DownloadSession *)session, url);
}
GF_Err mpdin_dash_io_set_range(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, u64 start_range, u64 end_range, Bool discontinue_cache)
{
	return gf_dm_sess_set_range((GF_DownloadSession *)session, start_range, end_range, discontinue_cache);
}
GF_Err mpdin_dash_io_init(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_process_headers((GF_DownloadSession *)session);
}
GF_Err mpdin_dash_io_run(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_process((GF_DownloadSession *)session);
}
const char *mpdin_dash_io_get_url(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_resource_name((GF_DownloadSession *)session);
}
const char *mpdin_dash_io_get_cache_name(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_cache_name((GF_DownloadSession *)session);
}
const char *mpdin_dash_io_get_mime(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_mime_type((GF_DownloadSession *)session);
}

const char *mpdin_dash_io_get_header_value(GF_DASHFileIO *dashio, GF_DASHFileIOSession session, const char *header_name)
{
	return gf_dm_sess_get_header((GF_DownloadSession *)session, header_name);
}

u64 mpdin_dash_io_get_utc_start_time(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	return gf_dm_sess_get_utc_start((GF_DownloadSession *)session);
}



u32 mpdin_dash_io_get_bytes_per_sec(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 bps=0;
//	GF_DownloadSession *sess = (GF_DownloadSession *)session;
    gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, NULL, NULL, &bps, NULL);
	return bps;
}
u32 mpdin_dash_io_get_total_size(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 size=0;
//	GF_DownloadSession *sess = (GF_DownloadSession *)session;
    gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, &size, NULL, NULL, NULL);
	return size;
}
u32 mpdin_dash_io_get_bytes_done(GF_DASHFileIO *dashio, GF_DASHFileIOSession session)
{
	u32 size=0;
//	GF_DownloadSession *sess = (GF_DownloadSession *)session;
    gf_dm_sess_get_stats((GF_DownloadSession *)session, NULL, NULL, NULL, &size, NULL, NULL);
	return size;
}


GF_Err mpdin_dash_io_on_dash_event(GF_DASHFileIO *dashio, GF_DASHEventType dash_evt, s32 group_idx, GF_Err error_code)
{
	GF_Err e;
	u32 i;
	GF_MPD_In *mpdin = (GF_MPD_In *)dashio->udta;

	if (dash_evt==GF_DASH_EVENT_PERIOD_SETUP_ERROR) {
		if (!mpdin->connection_ack_sent) {
	        gf_term_on_connect(mpdin->service, NULL, error_code);
			mpdin->connection_ack_sent= GF_TRUE;
		}
		return GF_OK;
	}

	if (dash_evt==GF_DASH_EVENT_SELECT_GROUPS) {
		const char *opt;
		for (i=0; i<gf_dash_get_group_count(mpdin->dash); i++) {
			/*todo: select groups based on user criteria*/
			gf_dash_group_select(mpdin->dash, i, 1);
		}
		opt = gf_modules_get_option((GF_BaseInterface *)mpdin->plug, "Systems", "Language3CC");
		if (opt && strcmp(opt, "und"))
			gf_dash_groups_set_language(mpdin->dash, opt);

		return GF_OK;
	}

	/*for all selected groups, create input service and connect to init/first segment*/
	if (dash_evt==GF_DASH_EVENT_CREATE_PLAYBACK) {

		/*select input services if possible*/
		for (i=0; i<gf_dash_get_group_count(mpdin->dash); i++) {
			const char *mime, *init_segment;			
			if (!gf_dash_is_group_selected(mpdin->dash, i))
				continue;

			mime = gf_dash_group_get_segment_mime(mpdin->dash, i);
			init_segment = gf_dash_group_get_segment_init_url(mpdin->dash, i, NULL, NULL);
			e = MPD_LoadMediaService(mpdin, i, mime, init_segment);
			if (e != GF_OK) {
				gf_dash_group_select(mpdin->dash, i, 0);
			} else {
				u32 w, h;
				/*connect our media service*/
				GF_MPDGroup *group = gf_dash_get_group_udta(mpdin->dash, i);
				gf_dash_group_get_video_info(mpdin->dash, i, &w, &h);
				if (w && h && w>mpdin->width && h>mpdin->height) {
					mpdin->width = w;
					mpdin->height = h;
				}

				e = group->segment_ifce->ConnectService(group->segment_ifce, mpdin->service, init_segment);
				if (e) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[MPD_IN] Unable to connect input service to %s\n", init_segment));
					gf_dash_group_select(mpdin->dash, i, 0);
				} else {
					group->service_connected = 1;
				}
			}
		}

		if (!mpdin->connection_ack_sent) {
	        gf_term_on_connect(mpdin->service, NULL, GF_OK);
			mpdin->connection_ack_sent=1;
		}
		return GF_OK;
	}

	/*for all running services, stop service*/
	if (dash_evt==GF_DASH_EVENT_DESTROY_PLAYBACK) {

		mpdin->service->subservice_disconnect = 1;
		gf_term_on_disconnect(mpdin->service, NULL, GF_OK);
		mpdin->service->subservice_disconnect = 2;

		for (i=0; i<gf_dash_get_group_count(mpdin->dash); i++) {
			GF_MPDGroup *group = gf_dash_get_group_udta(mpdin->dash, i);
			if (!group) continue;
			if (group->segment_ifce) {
				if (group->service_connected) {
					group->segment_ifce->CloseService(group->segment_ifce);
					group->service_connected = 0;
				}
				gf_modules_close_interface((GF_BaseInterface *) group->segment_ifce);
			}
			gf_free(group);			
			gf_dash_set_group_udta(mpdin->dash, i, NULL);
		}
		mpdin->service->subservice_disconnect = 0;
		return GF_OK;
	}

	if (dash_evt==GF_DASH_EVENT_BUFFERING) {
		u32 tot, done;
		gf_dash_get_buffer_info_buffering(mpdin->dash, &tot, &done);
		fprintf(stderr, "DASH: Buffering %g%% out of %d ms\n", (100.0*done)/tot, tot);
		return GF_OK;
	}
	if (dash_evt==GF_DASH_EVENT_SEGMENT_AVAILABLE) {
		if (group_idx>=0) {
			GF_MPDGroup *group = gf_dash_get_group_udta(mpdin->dash, group_idx);
			if (group) MPD_NotifyData(group, 0);
		}
	}
	return GF_OK;
}


GF_Err MPD_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    const char *opt;
    GF_Err e;
	s32 shift_utc_sec;
	u32 max_cache_duration, auto_switch_count, init_timeshift;
	GF_DASHInitialSelectionMode first_select_mode;
	Bool keep_files, disable_switching;

    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Service Connection request (%p) from terminal for %s\n", serv, url));

    if (!mpdin|| !serv || !url) return GF_BAD_PARAM;

    mpdin->service = serv;

	mpdin->dash_io.udta = mpdin;
	mpdin->dash_io.delete_cache_file = mpdin_dash_io_delete_cache_file;
	mpdin->dash_io.create = mpdin_dash_io_create;
	mpdin->dash_io.del = mpdin_dash_io_del;
	mpdin->dash_io.abort = mpdin_dash_io_abort;
	mpdin->dash_io.setup_from_url = mpdin_dash_io_setup_from_url;
	mpdin->dash_io.set_range = mpdin_dash_io_set_range;
	mpdin->dash_io.init = mpdin_dash_io_init;
	mpdin->dash_io.run = mpdin_dash_io_run;
	mpdin->dash_io.get_url = mpdin_dash_io_get_url;
	mpdin->dash_io.get_cache_name = mpdin_dash_io_get_cache_name;
	mpdin->dash_io.get_mime = mpdin_dash_io_get_mime;
	mpdin->dash_io.get_header_value = mpdin_dash_io_get_header_value;
	mpdin->dash_io.get_utc_start_time = mpdin_dash_io_get_utc_start_time;
	mpdin->dash_io.get_bytes_per_sec = mpdin_dash_io_get_bytes_per_sec;
	mpdin->dash_io.get_total_size = mpdin_dash_io_get_total_size;
	mpdin->dash_io.get_bytes_done = mpdin_dash_io_get_bytes_done;
	mpdin->dash_io.on_dash_event = mpdin_dash_io_on_dash_event;

	max_cache_duration = 0;
	opt = gf_modules_get_option((GF_BaseInterface *)plug, "Network", "BufferLength");
    if (opt) max_cache_duration = atoi(opt);

    auto_switch_count = 0;
    opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "AutoSwitchCount");
    if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "AutoSwitchCount", "0");
    if (opt) auto_switch_count = atoi(opt);

	keep_files = 0;
    opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "KeepFiles");
    if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "KeepFiles", "no");
    if (opt && !strcmp(opt, "yes")) keep_files = 1;

	disable_switching = 0;
	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "DisableSwitching");
    if (opt && !strcmp(opt, "yes")) disable_switching = 1;

	first_select_mode = 0;
	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "StartRepresentation");
	if (!opt) {
		gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "StartRepresentation", "minBandwidth");
		opt = "minBandwidth";
	}
	if (opt && !strcmp(opt, "maxBandwidth")) first_select_mode = GF_DASH_SELECT_BANDWIDTH_HIGHEST;
    else if (opt && !strcmp(opt, "minQuality")) first_select_mode = GF_DASH_SELECT_QUALITY_LOWEST;
    else if (opt && !strcmp(opt, "maxQuality")) first_select_mode = GF_DASH_SELECT_QUALITY_HIGHEST;
    else first_select_mode = GF_DASH_SELECT_BANDWIDTH_LOWEST;

	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "MemoryStorage");
	if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "MemoryStorage", "yes");
	mpdin->memory_storage = (opt && !strcmp(opt, "yes")) ? 1 : 0;

	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "UseMaxResolution");
	if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "UseMaxResolution", "yes");
	mpdin->use_max_res = (!opt || !strcmp(opt, "yes")) ? 1 : 0;

	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "ImmediateSwitching");
	if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "ImmediateSwitching", "no");
	mpdin->immediate_switch = (opt && !strcmp(opt, "yes")) ? 1 : 0;
	
	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "EnableBuffering");
	if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "EnableBuffering", "no");
	mpdin->enable_buffering = (opt && !strcmp(opt, "yes")) ? 1 : 0;

	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "LowLatency");
	if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "LowLatency", "no");

	if (opt && !strcmp(opt, "chunk") ) mpdin->use_low_latency = 1;
	else if (opt && !strcmp(opt, "always") ) mpdin->use_low_latency = 2;
	else mpdin->use_low_latency = 0;

	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "AllowAbort");
	if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "AllowAbort", "no");
	mpdin->allow_http_abort = (opt && !strcmp(opt, "yes")) ? GF_TRUE : GF_FALSE;
	
	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "ShiftClock");
	if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "ShiftClock", "0");
	shift_utc_sec = opt ? atoi(opt) : 0;
	

	mpdin->in_seek = 0;
	mpdin->previous_start_range = -1;

	init_timeshift = 0;
	opt = gf_modules_get_option((GF_BaseInterface *)plug, "DASH", "InitialTimeshift");
	if (!opt) gf_modules_set_option((GF_BaseInterface *)plug, "DASH", "InitialTimeshift", "0");
	if (opt) init_timeshift = atoi(opt);

	mpdin->dash = gf_dash_new(&mpdin->dash_io, max_cache_duration, auto_switch_count, keep_files, disable_switching, first_select_mode, mpdin->enable_buffering, init_timeshift);

	if (!mpdin->dash) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD_IN] Error - cannot create DASH Client for %s\n", url));
        gf_term_on_connect(mpdin->service, NULL, GF_IO_ERR);
		return GF_OK;
	}

	gf_dash_set_utc_shift(mpdin->dash, shift_utc_sec);

	/*dash thread starts at the end of gf_dash_open */
	e = gf_dash_open(mpdin->dash, url);
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MPD_IN] Error - cannot initialize DASH Client for %s: %s\n", url, gf_error_to_string(e) ));
        gf_term_on_connect(mpdin->service, NULL, e);
		return GF_OK;
	}
	return GF_OK;
}

static GF_Descriptor *MPD_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	u32 i;
	GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Service Description request from terminal for %s\n", sub_url));
	for (i=0; i<gf_dash_get_group_count(mpdin->dash); i++) {
		GF_Descriptor *desc;
		GF_MPDGroup *mudta;
		if (!gf_dash_is_group_selected(mpdin->dash, i))
			continue;
		mudta = gf_dash_get_group_udta(mpdin->dash, i);
		if (!mudta) continue;
		if (mudta->service_descriptor_fetched) continue;

		desc = mudta->segment_ifce->GetServiceDescriptor(mudta->segment_ifce, expect_type, sub_url);
		if (desc) mudta->service_descriptor_fetched = 1;
		return desc;
	}
	return NULL;
}


GF_Err MPD_CloseService(GF_InputService *plug)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
    assert( mpdin );
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Close Service (%p) request from terminal\n", mpdin->service));
   
	if (mpdin->dash)
		gf_dash_close(mpdin->dash);

	gf_term_on_disconnect(mpdin->service, NULL, GF_OK);

	return GF_OK;
}

GF_Err MPD_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	s32 idx;
    GF_Err e;
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = NULL;

	if (!plug || !plug->priv || !com ) return GF_SERVICE_ERROR;

	segment_ifce = MPD_GetInputServiceForChannel(mpdin, com->base.on_channel);

	switch (com->command_type) {
    case GF_NET_SERVICE_INFO:
    {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Info command from terminal on Service (%p)\n", mpdin->service));

		e = GF_OK;
		if (segment_ifce) {
			/* defer to the real input service */
			e = segment_ifce->ServiceCommand(segment_ifce, com);
		}

		if (e!= GF_OK || !com->info.name || 2 > strlen(com->info.name)) {
			gf_dash_get_info(mpdin->dash, &com->info.name, &com->info.comment);
        }
        return GF_OK;
    }
	/*we could get it from MPD*/
    case GF_NET_SERVICE_HAS_AUDIO:
	case GF_NET_SERVICE_FLUSH_DATA:
		if (segment_ifce) {
	        /* defer to the real input service */
		    return segment_ifce->ServiceCommand(segment_ifce, com);
		}
        return GF_NOT_SUPPORTED;

	case GF_NET_SERVICE_HAS_FORCED_VIDEO_SIZE:
		com->par.width = mpdin->use_max_res ? mpdin->width : 0;
		com->par.height = mpdin->use_max_res ? mpdin->height : 0;
        return GF_OK;

	case GF_NET_SERVICE_QUALITY_SWITCH:
		gf_dash_switch_quality(mpdin->dash, com->switch_quality.up, mpdin->immediate_switch);
        return GF_OK;
	}
	/*not supported*/
	if (!com->base.on_channel) return GF_NOT_SUPPORTED;
	
	segment_ifce = MPD_GetInputServiceForChannel(mpdin, com->base.on_channel);
	if (!segment_ifce) return GF_NOT_SUPPORTED;

	switch (com->command_type) {
    case GF_NET_CHAN_INTERACTIVE:
        /* we are interactive (that's the whole point of MPD) */
        return GF_OK;

	/*we should get it from MPD minBufferTime*/
	case GF_NET_CHAN_BUFFER:
		if (mpdin->enable_buffering) {
			com->buffer.max = gf_dash_get_min_buffer_time(mpdin->dash);
			if (! gf_dash_is_dynamic_mpd(mpdin->dash)) { 
				com->buffer.min = 200;
			}
		}
        return GF_OK;

	case GF_NET_CHAN_DURATION:
		/* Ignore the duration given by the input service and use the one given in the MPD
		   Note: the duration of the initial segment will be 0 anyway (in MP4).*/
		com->duration.duration = gf_dash_get_duration(mpdin->dash);
		return GF_OK;

	case GF_NET_CHAN_PLAY:
		/*don't seek if this command is the first PLAY request of objects declared by the subservice*/
		if (!com->play.initial_broadcast_play) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Play command from terminal on channel %p on Service (%p)\n", com->base.on_channel, mpdin->service));

			if (!gf_dash_in_period_setup(mpdin->dash) && !com->play.dash_segment_switch && ! mpdin->in_seek) {
				//Bool skip_seek;
				
				mpdin->in_seek = 1;
				
				/*if start range request is the same as previous one, don't process it
				- this happens at period switch when new objects are declared*/
				//skip_seek = (mpdin->previous_start_range==com->play.start_range) ? 1 : 0;
				mpdin->previous_start_range = com->play.start_range;

				gf_dash_seek(mpdin->dash, com->play.start_range);
			} 
			/*For MPEG-2 TS or formats not using Init Seg: since objects are declared and started once the first
			segment is playing, we will stay in playback_start_range!=-1 until next segment (because we won't have a query_next),
			which will prevent seeking until then ... we force a reset of playback_start_range to allow seeking asap*/
			else if (mpdin->in_seek && (com->play.start_range==0)) {
//				mpdin->in_seek = 0;
			}
			else if (gf_dash_in_period_setup(mpdin->dash) && (com->play.start_range==0)) {
				com->play.start_range = gf_dash_get_playback_start_range(mpdin->dash);
			}

			idx = MPD_GetGroupIndexForChannel(mpdin, com->play.on_channel);
			if (idx>=0) {
				gf_dash_set_group_done(mpdin->dash, idx, 0);
				com->play.dash_segment_switch = gf_dash_group_segment_switch_forced(mpdin->dash, idx);
			}

			/*don't forward commands, we are killing the service anyway ...*/
			if (gf_dash_get_period_switch_status(mpdin->dash) ) return GF_OK;
		} else {
			s32 idx = MPD_GetGroupIndexForChannel(mpdin, com->play.on_channel);
			if (idx>=0)
				com->play.start_range = gf_dash_group_get_start_range(mpdin->dash, idx);
		}

		return segment_ifce->ServiceCommand(segment_ifce, com);

	case GF_NET_CHAN_STOP:
	{
		s32 idx = MPD_GetGroupIndexForChannel(mpdin, com->play.on_channel);
		if (idx>=0) {
			gf_dash_set_group_done(mpdin->dash, (u32) idx, 1);
		}
	}
		return segment_ifce->ServiceCommand(segment_ifce, com);

	/*we could get it from MPD*/
	case GF_NET_CHAN_GET_PIXEL_AR:
        /* defer to the real input service */
        return segment_ifce->ServiceCommand(segment_ifce, com);

	case GF_NET_CHAN_SET_SPEED:
        return segment_ifce->ServiceCommand(segment_ifce, com);

    default:
        return segment_ifce->ServiceCommand(segment_ifce, com);
    }
}

GF_Err MPD_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;
    return segment_ifce->ChannelGetSLP(segment_ifce, channel, out_data_ptr, out_data_size, out_sl_hdr, sl_compressed, out_reception_status, is_new_data);
}

GF_Err MPD_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_InputService *segment_ifce = MPD_GetInputServiceForChannel(mpdin, channel);
    if (!plug || !plug->priv || !segment_ifce) return GF_SERVICE_ERROR;
    return segment_ifce->ChannelReleaseSLP(segment_ifce, channel);
}

Bool MPD_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	/**
	* May arrive when using pid:// URLs into a TS stream
	*/
	GF_MPD_In *mpdin = (GF_MPD_In*) plug->priv;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MPD_IN] Received Can Handle URL In Service (%p) request from terminal for %s\n", mpdin->service, url));
	if (!plug || !plug->priv) return GF_SERVICE_ERROR;
	if (gf_dash_get_url(mpdin->dash) && !strcmp(gf_dash_get_url(mpdin->dash) , url)) {
		return 1;
	} else {
		GF_MPDGroup *mudta;
		u32 i;
		for (i=0;i<gf_dash_get_group_count(mpdin->dash); i++) {
			if (!gf_dash_is_group_selected(mpdin->dash, i)) continue;
			
			mudta = gf_dash_get_group_udta(mpdin->dash, i);
			if (mudta && mudta->segment_ifce && mudta->segment_ifce->CanHandleURLInService) {
				return mudta->segment_ifce->CanHandleURLInService(plug, url);
			}
		}
		return 0;
	}
}

GPAC_MODULE_EXPORT
const u32 *QueryInterfaces()
{
    static u32 si [] = {
        GF_NET_CLIENT_INTERFACE,
        0
    };
    return si;
}

GPAC_MODULE_EXPORT
GF_BaseInterface *LoadInterface(u32 InterfaceType)
{
    GF_MPD_In *mpdin;
    GF_InputService *plug;
    if (InterfaceType != GF_NET_CLIENT_INTERFACE) return NULL;

    GF_SAFEALLOC(plug, GF_InputService);
    GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC MPD Loader", "gpac distribution")
    plug->RegisterMimeTypes = MPD_RegisterMimeTypes;
    plug->CanHandleURL = MPD_CanHandleURL;
    plug->ConnectService = MPD_ConnectService;
    plug->CloseService = MPD_CloseService;
    plug->GetServiceDescriptor = MPD_GetServiceDesc;
    plug->ConnectChannel = MPD_ConnectChannel;
    plug->DisconnectChannel = MPD_DisconnectChannel;
    plug->ServiceCommand = MPD_ServiceCommand;
    plug->CanHandleURLInService = MPD_CanHandleURLInService;
    plug->ChannelGetSLP = MPD_ChannelGetSLP;
    plug->ChannelReleaseSLP = MPD_ChannelReleaseSLP;
    GF_SAFEALLOC(mpdin, GF_MPD_In);
    plug->priv = mpdin;
	mpdin->plug = plug;
    return (GF_BaseInterface *)plug;
}

GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
	GF_MPD_In *mpdin;

	if (bi->InterfaceType!=GF_NET_CLIENT_INTERFACE) return;

	mpdin = (GF_MPD_In*) ((GF_InputService*)bi)->priv;
	assert( mpdin );

	if (mpdin->dash)
		gf_dash_del(mpdin->dash);

	gf_free(mpdin);
	gf_free(bi);
}


GPAC_MODULE_STATIC_DELARATION( mpd_in )

#endif //GPAC_DISABLE_DASH_CLIENT
