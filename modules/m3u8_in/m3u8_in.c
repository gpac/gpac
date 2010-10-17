/**
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC
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
 *  Written by Pierre Souchay for VizionR SAS
 *
 */

#include <gpac/modules/service.h>
#include <gpac/internal/terminal_dev.h>
#include <gpac/thread.h>
#include <gpac/network.h>
#include "m3u8_in.h"
#include <gpac/list.h>
#include "m3u8_parser.h"

#define MYLOG(xx) GF_LOG( GF_LOG_INFO, GF_LOG_NETWORK, xx )


typedef struct s_M3UREADER
{
	GF_ClientService *service;
	char * rootURL;
	GF_DownloadSession * dnload;
	char is_remote;
	char needs_connection;
	VariantPlaylist * playlist;
	u32 preferredBitrate;

    PlaylistElement *ple;

    /* Service really managing the media */
    GF_InputService *media_ifce;
    GF_Thread *dl_thread;

} M3U8Reader;


static Bool M3U8_CanHandleURL(GF_InputService *plug, const char *url)
{
	char *sExt;
	MYLOG(("***** M3U8_CanHandleURL %s\n", url));
	sExt = strrchr(url, '.');
	if (!sExt) return 0;
	if (gf_term_check_extension(plug, "application/vnd.apple.mpegurl", "m3u8", "M3U8 Playlist", sExt))
		return 1;
	if (gf_term_check_extension(plug, "application/x-mpegURL", "m3u8", "M3U8 Playlist", sExt))
		return 1;
	if (gf_term_check_extension(plug, "text/plain", "m3u8", "M3U8 Playlist", sExt))
		return 1;
	return 0;
}

static Bool m3u8_is_local(const char *url)
{
	if (!strnicmp(url, "file://", 7)) return 1;
	if (strstr(url, "://")) return 0;
	return 1;
}


PlaylistElement * findNextPlaylistElementToPlay(M3U8Reader *read){
	VariantPlaylist * pl;
	Program * p;
	PlaylistElement * toPlay;
	assert( read );
	assert( read->playlist);
	pl = read->playlist;
	p = variant_playlist_get_current_program(pl);
	if (p == NULL)
		return NULL;
	toPlay = gf_list_get( p->bitrates, 0);
	if (toPlay == NULL)
		return NULL;
	switch (toPlay->elementType){
		case TYPE_PLAYLIST:
			return gf_list_get(toPlay->element.playlist.elements, 0);
		case TYPE_UNKNOWN:
			/* We have to download the file ! */
		case TYPE_STREAM:
		default:
			return toPlay;
	}
	
}


GF_Err M3U8_parseM3U8File(M3U8Reader * read){
	const char * szCache;
	GF_Err e;
	MYLOG(("***** M3U8_parseM3U8File\n"));
	szCache = gf_dm_sess_get_cache_name(read->dnload);
	if (!szCache){
		e = GF_CORRUPTED_DATA; 
		goto end;
	}
	
	e = parse_root_playlist(szCache, & (read->playlist), read->rootURL);
    if (e){
		MYLOG(("***** Failed to parse root playlist : %s\n", szCache));
	} else {
		PlaylistElement * element = findNextPlaylistElementToPlay(read);
		if (element != NULL){
			MYLOG(("FOUND Program : %s\n", element->url));
			playlist_element_dump( element, 2 );
		}
	}
end:
	gf_term_on_connect(read->service, NULL, e);
	gf_term_download_del(read->dnload);
	read->dnload = NULL;
	return e;
}

void M3U8_NetIO(void *cbk, GF_NETIO_Parameter *param)
{
	M3U8Reader *read = (M3U8Reader *) cbk;
	MYLOG(("***** M3U8_NetIO %d\n", param->msg_type));
	gf_term_download_update_stats(read->dnload);
	
}

void M3U8_DownloadFile(GF_InputService *plug, const char *url)
{
    GF_Err e;
	M3U8Reader *read = (M3U8Reader*) plug->priv;
    MYLOG(("***** M3U8_DownloadFile %s\n", url));
	read->dnload = gf_term_download_new(read->service, url, GF_NETIO_SESSION_NOT_THREADED, M3U8_NetIO, read);
	if (!read->dnload) {
		MYLOG(("***** M3U8_DownloadFile FAILED\n"));
		gf_term_on_connect(read->service, NULL, GF_NOT_SUPPORTED);
	}
    /* do the actual download (because not threaded )*/
	e = gf_dm_sess_process(read->dnload);
	if (e!=GF_OK) {
        gf_term_on_connect(read->service, NULL, GF_IO_ERR);
    }
}

static u32 download_media_files(void *par) 
{
    GF_Err e;
	M3U8Reader *read = (M3U8Reader*) par;

        /*
    while (1) {
        period = gf_list_get(mpdin->mpd.periods, mpdin->active_period_index);
        rep = &period->representations[mpdin->active_rep_index];
        new_seg_url = rep->segment_urls[mpdin->active_segment_index];
        if (rep->default_base_url) {
            new_base_seg_url = gf_url_concatenate(rep->default_base_url, new_seg_url);
        } else {
            new_base_seg_url = gf_strdup(new_seg_url);
        }

        gf_dm_setup_from_url(read->dnload, new_base_seg_url);
        gf_dm_sess_dash_reset(read->dnload);
        fprintf(stderr, "Downloading new media file: %s\n", new_base_seg_url);
        gf_free(new_base_seg_url);

        e = gf_dm_sess_process(read->dnload);     
        if (e == GF_OK) {
			//signal the slave service the segment has been downloaded
			if (read->dnload) {
			}
//            if (mpdin->active_segment_index<10) {
            if (mpdin->active_segment_index<(rep->nb_segments-1)) {
                mpdin->active_segment_index++;
            } else {
                break;
            }
        } else {
            break;
        }
        gf_sleep(1000);
    }
    */
    gf_sleep(1000);
    return 0;
}

static GF_Err M3U8_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char szURL[2048];
	GF_Err e;
	FILE * f;
	M3U8Reader *read = plug->priv;
	MYLOG(("***** M3U8_ConnectService %s\n", url));
	if (read->rootURL){
		gf_free(read->rootURL);
		read->rootURL = NULL;
	}
	assert( url );
	read->rootURL = strdup(url);
	read->service = serv;

	if (read->dnload) gf_term_download_del(read->dnload);
	read->dnload = NULL;

	/*read->service_type = 0;*/
	strcpy(szURL, url);

	/*remote fetch*/
	read->is_remote = !m3u8_is_local(szURL);
	if (read->is_remote) {
		MYLOG(("***** M3U8_ConnectService Remote %s\n", url));
		read->needs_connection = 1;
		M3U8_DownloadFile(plug, szURL);
	} else {
		MYLOG(("***** M3U8_ConnectService local %s\n", url));
		f = fopen(szURL, "rb");
		if (!f) {
			e = GF_URL_ERROR;
		} else {
			e = GF_OK;
		}
	}
    e = M3U8_parseM3U8File( read );
    if (e){
	    MYLOG(("***** Error while reading playlist %d\n", e));
        gf_term_on_connect(read->service, NULL, e);
        return GF_OK;
    } else {
	    read->ple = findNextPlaylistElementToPlay(read);

    	if (read->dnload) gf_term_download_del(read->dnload);
        read->dnload = gf_term_download_new(read->service, read->ple->url, GF_NETIO_SESSION_NOT_THREADED, 
                                            M3U8_NetIO, read);
        if (!read->dnload) {
            gf_term_on_connect(read->service, NULL, GF_BAD_PARAM);
            return GF_OK;
        } else {
            e = gf_dm_sess_process(read->dnload);
            if (e!=GF_OK) {
                gf_term_on_connect(read->service, NULL, e);
                return GF_OK;
            } else {
                const char *local_url = gf_dm_sess_get_cache_name(read->dnload);
                /*load from mime type*/	            
                const char *sPlug = gf_cfg_get_key(read->service->term->user->config, "MimeTypes", "video/mp2t");
                if (sPlug) sPlug = strrchr(sPlug, '"');
                if (sPlug) {
	                sPlug += 2;
	                read->media_ifce = (GF_InputService *) gf_modules_load_interface_by_name(read->service->term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
                    if (read->media_ifce) {
#if 1
						gf_th_run(read->dl_thread, download_media_files, read);
#else                            
                        e = download_media_files(read);
                        if (e) {
                            gf_term_on_connect(read->service, NULL, GF_BAD_PARAM);
                            return GF_OK;
                        }
#endif
                        /* Forward the ConnectService message to the appropriate service for this type of media */
                        read->media_ifce->ConnectService(read->media_ifce, serv, local_url);
                    } else {
                        gf_term_on_connect(read->service, NULL, GF_BAD_PARAM);
                        return GF_OK;
                    }
                } else {
                    gf_term_on_connect(read->service, NULL, GF_BAD_PARAM);
                    return GF_OK;
                }
		    }
        }
    }	

    gf_term_on_connect(serv, NULL, e);
	return GF_OK;
}

static GF_Err M3U8_CloseService(GF_InputService *plug)
{
	M3U8Reader *read = plug->priv;
	MYLOG(("***** M3U8_CloseService %p\n", plug->priv));
	if (read == NULL)
		return GF_OK;
	if (read->media_ifce) {
        read->media_ifce->CloseService(read->media_ifce);
    }
	MYLOG(("***** M3U8_CloseService2\n"));
	gf_term_on_disconnect(read->service, NULL, GF_OK);
	if (read->dnload){
		MYLOG(("***** M3U8_CloseService : stopping download\n"));
		gf_term_download_del(read->dnload);
		read->dnload = NULL;
	}
	
	return GF_OK;
}

static Bool M3U8_CanHandleURLInService(GF_InputService *plug, const char *url)
{
	MYLOG(("***** M3U8_CanHandleURLInService %s\n", url));
	return 0;
}

static GF_Descriptor *M3U8_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	M3U8Reader *read = plug->priv;
	MYLOG(("***** M3U8_GetServiceDesc %s\n", sub_url));
    if (read->media_ifce) {
        return read->media_ifce->GetServiceDescriptor(read->media_ifce, expect_type, sub_url);
    } else {
        return NULL;
    }
}

static GF_Err M3U8_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	M3U8Reader *read = plug->priv;
	MYLOG(("***** M3U8_ConnectChannel\n"));
	if (!plug || !plug->priv || !read->media_ifce) return GF_SERVICE_ERROR;
    return read->media_ifce->ConnectChannel(read->media_ifce, channel, url, upstream);
}

static GF_Err M3U8_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	M3U8Reader *read = plug->priv;
	MYLOG(("***** M3U8_DisconnectChannel\n"));
	if (!plug || !plug->priv || !read->media_ifce) return GF_SERVICE_ERROR;
    return read->media_ifce->DisconnectChannel(read->media_ifce, channel);
}

static GF_Err M3U8_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
	M3U8Reader *read = plug->priv;
	MYLOG(("***** M3U8_ChannelGetSLP\n"));
	if (!plug || !plug->priv || !read->media_ifce) return GF_SERVICE_ERROR;
    return read->media_ifce->ChannelGetSLP(read->media_ifce, channel, out_data_ptr, out_data_size, out_sl_hdr, sl_compressed, out_reception_status, is_new_data); 
}

static GF_Err M3U8_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
	M3U8Reader *read = plug->priv;
	MYLOG(("***** M3U8_ChannelReleaseSLP\n"));
	if (!plug || !plug->priv || !read->media_ifce) return GF_SERVICE_ERROR;
    return read->media_ifce->ChannelReleaseSLP(read->media_ifce, channel); 
}


static GF_Err M3U8_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	M3U8Reader *read = plug->priv;
	MYLOG(("***** M3U8_ServiceCommand %s\n", com));
	if (!plug || !plug->priv || !com || !read->media_ifce) return GF_SERVICE_ERROR;
    
    switch(com->command_type) {
        case GF_NET_SERVICE_INFO:
            /* defer to the real input service */
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_SERVICE_HAS_AUDIO:
            /* defer to the real input service */
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_SET_PADDING:
            /* for padding settings, the MPD level should not change anything */
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_SET_PULL:
            /* defer to the real input service */
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_INTERACTIVE:
            /* defer to the real input service */
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_BUFFER:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_BUFFER_QUERY:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_DURATION:
            /* Ignore the duration given by the input service and use the one given in the playlist */
            {
                Double duration;
                duration = (read->ple ? read->ple->durationInfo : 0); 
                com->duration.duration = duration;
        		return GF_OK;
            }
            break;
        case GF_NET_CHAN_PLAY:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_STOP:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_PAUSE:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_RESUME:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_SET_SPEED:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
	    case GF_NET_CHAN_CONFIG:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_GET_PIXEL_AR:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_GET_DSI:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_MAP_TIME:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_RECONFIG:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_DRM_CFG:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_CHAN_GET_ESD:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_BUFFER_QUERY:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_GET_STATS:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_IS_CACHABLE:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        case GF_NET_SERVICE_MIGRATION_INFO:
            return read->media_ifce->ServiceCommand(read->media_ifce, com);
            break;
        default:
            return GF_SERVICE_ERROR;
    }
}

GF_InputService *M3U8_LoadDemux()
{
	M3U8Reader *reader;
	GF_InputService *plug = gf_malloc(sizeof(GF_InputService));
	memset(plug, 0, sizeof(GF_InputService));
	MYLOG(("Loading M3U8\n"));
	GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC M3U8 Reader", "gpac distribution")
	/* Yes, we handle this */
	plug->CanHandleURL = M3U8_CanHandleURL;
	/* Start read m3u8, we do */
	plug->ConnectService = M3U8_ConnectService;
	plug->CloseService = M3U8_CloseService;
	
	/* Foward to service, none of our business */
	plug->GetServiceDescriptor = M3U8_GetServiceDesc;
	plug->ConnectChannel = M3U8_ConnectChannel;
	plug->DisconnectChannel = M3U8_DisconnectChannel;
	plug->CanHandleURLInService = M3U8_CanHandleURLInService;
	
	/* To check for seeking / getTrackInfo... GF_NET_SERVICE_INFO */
	plug->ServiceCommand = M3U8_ServiceCommand;

	/* to transmit packets */
    plug->ChannelGetSLP = M3U8_ChannelGetSLP;
	plug->ChannelReleaseSLP = M3U8_ChannelReleaseSLP;

	reader = gf_malloc(sizeof(M3U8Reader));
	memset(reader, 0, sizeof(M3U8Reader));
	reader->playlist = NULL;
	reader->rootURL = NULL;
	plug->priv = reader;
	return plug;
}

void M3U8_DeleteDemux(void *ifce)
{
	GF_InputService *plug = (GF_InputService *) ifce;
	M3U8Reader *read = plug->priv;
	if (read){
		if (read->rootURL)
			gf_free(read->rootURL);
		read->rootURL = NULL;
		gf_free(read);
	}
	gf_free(plug);
}
