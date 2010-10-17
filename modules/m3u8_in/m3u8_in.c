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
	GF_Err e;
	M3U8Reader *read = (M3U8Reader *) cbk;
	MYLOG(("***** M3U8_NetIO %d\n", param->msg_type));
	gf_term_download_update_stats(read->dnload);
	
	if ( param->msg_type==GF_NETIO_DATA_TRANSFERED ) {
		gf_term_download_update_stats(read->dnload);
		e = M3U8_parseM3U8File( read );
		MYLOG(("***** M3U8_NetIO DONE %d\n", param->msg_type));
		/*gf_term_download_del(read->dnload);
		MYLOG(("***** M3U8_NetIO deleted %d\n", param->msg_type));*/
		if (e){
				MYLOG(("***** Error while reading playlist %d\n", e));
		} else {
			PlaylistElement *ple = findNextPlaylistElementToPlay(read);
		}
		return;
	}
	if (param->error && read->needs_connection) {
		gf_term_on_connect(read->service, NULL, param->error);
	}
	/*we never receive data from here since the downloader is not threaded*/
}



void M3U8_DownloadFile(GF_InputService *plug, const char *url)
{
	M3U8Reader *read = (M3U8Reader*) plug->priv;
    MYLOG(("***** M3U8_DownloadFile %s\n", url));
	read->dnload = gf_term_download_new(read->service, url, 0, M3U8_NetIO, read);
	if (!read->dnload) {
		MYLOG(("***** M3U8_DownloadFile FAILED\n"));
		gf_term_on_connect(read->service, NULL, GF_NOT_SUPPORTED);
	}
	/*service confirm is done once fetched, but start the demuxer thread*/
	/*gf_th_run(read->demuxer, OggDemux, read);*/
}


static GF_Err M3U8_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
	char szURL[2048];
	GF_Err reply;
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
		return GF_OK;
	} else {
		MYLOG(("***** M3U8_ConnectService local %s\n", url));
		f = fopen(szURL, "rb");
		if (!f) {
			reply = GF_URL_ERROR;
		} else {
			reply = GF_OK;
		}
	}
	gf_term_on_connect(serv, NULL, reply);
	return GF_OK;
}

static GF_Err M3U8_CloseService(GF_InputService *plug)
{
	M3U8Reader *read = plug->priv;
	MYLOG(("***** M3U8_CloseService %p\n", plug->priv));
	if (read == NULL)
		return GF_OK;
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
	/*char szURL[2048], *sep;*/
	/*M3U8Reader *read = (M3U8Reader *)plug->priv;*/
	MYLOG(("***** M3U8_CanHandleURLInService %s\n", url));
	/*const char *this_url = gf_term_get_service_url(read->service);*/
	return 0;
}

static GF_Descriptor *M3U8_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
	/*u32 i;
	GF_ObjectDescriptor *od;*/
	MYLOG(("***** M3U8_CanHandleURLInService %s\n", sub_url));
	/*OGGStream *st;
	OGGReader *read = plug->priv;*/
	/*since we don't handle multitrack in ogg yes, we don't need to check sub_url, only use expected type*/
	
	/*single object*/
	/*
	if ((expect_type==GF_MEDIA_OBJECT_AUDIO) || (expect_type==GF_MEDIA_OBJECT_VIDEO)) {
		if ((expect_type==GF_MEDIA_OBJECT_AUDIO) && !read->has_audio) return NULL;
		if ((expect_type==GF_MEDIA_OBJECT_VIDEO) && !read->has_video) return NULL;
		i=0;
		while ((st = gf_list_enum(read->streams, &i))) {
			if ((expect_type==GF_MEDIA_OBJECT_AUDIO) && (st->info.streamType!=GF_STREAM_AUDIO)) continue;
			if ((expect_type==GF_MEDIA_OBJECT_VIDEO) && (st->info.streamType!=GF_STREAM_VISUAL)) continue;
			
			od = OGG_GetOD(st);
			read->is_single_media = 1;
			return (GF_Descriptor *) od;
		}
		//not supported yet - we need to know what's in the ogg stream for that
	}
	read->is_inline = 1;*/
	return NULL;
}

static GF_Err M3U8_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
	/*u32 ES_ID, i;*/
	GF_Err e = GF_OK;
	M3U8Reader *read = plug->priv;
	/*OGGStream *st;
	
	
	e = GF_STREAM_NOT_FOUND;
	if (strstr(url, "ES_ID")) {
		sscanf(url, "ES_ID=%d", &ES_ID);
	}*/
	/*URL setup*/
	/*
		else if (!read->es_ch && OGG_CanHandleURL(plug, url)) ES_ID = 3;
	i=0;
	while ((st = gf_list_enum(read->streams, &i))) {
		if (st->ESID==ES_ID) {
			st->ch = channel;
			e = GF_OK;
			break;
		}
	}*/
	gf_term_on_connect(read->service, channel, e);
	return e;
}

static GF_Err M3U8_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
	GF_Err e = GF_OK;
	M3U8Reader *read = plug->priv;
	/*
	
	OGGStream *st;
	u32 i=0;
	
	
	e = GF_STREAM_NOT_FOUND;
	while ((st = gf_list_enum(read->streams, &i))) {
		if (st->ch==channel) {
			st->ch = NULL;
			e = GF_OK;
			break;
		}
	}*/
	gf_term_on_disconnect(read->service, channel, e);
	return e;
}

static GF_Err M3U8_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
	MYLOG(("***** M3U8_ServiceCommand %s\n", com));
	/*
	OGGStream *st;
	u32 i;
	OGGReader *read = plug->priv;
	
	if (!com->base.on_channel) {
		// if live session we may cache
		if (read->is_live && (com->command_type==GF_NET_IS_CACHABLE)) return GF_OK;
		return GF_NOT_SUPPORTED;
	}
	switch (com->command_type) {
		case GF_NET_CHAN_SET_PULL:
			//no way to demux streams independently, and we keep OD as dynamic ogfile to handle
			// chained streams
			return GF_NOT_SUPPORTED;
		case GF_NET_CHAN_INTERACTIVE:
			//live: return GF_NOT_SUPPORTED;
			return GF_OK;
		case GF_NET_CHAN_BUFFER:
			com->buffer.min = com->buffer.max = 0;
			if (read->is_live) com->buffer.max = read->data_buffer_ms;
			return GF_OK;
		case GF_NET_CHAN_SET_PADDING: return GF_NOT_SUPPORTED;
			
		case GF_NET_CHAN_DURATION:
			com->duration.duration = read->dur;
			return GF_OK;
		case GF_NET_CHAN_PLAY:
			read->start_range = com->play.start_range;
			read->end_range = com->play.end_range;
			i=0;
			while ((st = gf_list_enum(read->streams, &i))) {
				if (st->ch == com->base.on_channel) {
					st->is_running = 1;
					st->map_time = read->dur ? 1 : 0;
					if (!read->nb_playing) read->do_seek = 1;
					read->nb_playing ++;
					break;
				}
			}
			//recfg duration in case
			if (!read->is_remote && read->dur) { 
				GF_NetworkCommand rcfg;
				rcfg.base.on_channel = NULL;
				rcfg.base.command_type = GF_NET_CHAN_DURATION;
				rcfg.duration.duration = read->dur;
				gf_term_on_command(read->service, &rcfg, GF_OK);
			}
			return GF_OK;
		case GF_NET_CHAN_STOP:
			i=0;
			while ((st = gf_list_enum(read->streams, &i))) {
				if (st->ch == com->base.on_channel) {
					st->is_running = 0;
					read->nb_playing --;
					break;
				}
			}
			return GF_OK;
		default:
			return GF_OK;
	}*/
	return GF_OK;
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

	reader = gf_malloc(sizeof(M3U8Reader));
	memset(reader, 0, sizeof(M3U8Reader));
	reader->playlist = NULL;
	reader->rootURL = NULL;
	/*
	reader->streams = gf_list_new();
	reader->demuxer = gf_th_new("M3U8Demux");
	reader->data_buffer_ms = 1000;
*/
	plug->priv = reader;
	return plug;
}

void M3U8_DeleteDemux(void *ifce)
{
	GF_InputService *plug = (GF_InputService *) ifce;
	M3U8Reader *read = plug->priv;
	
	/*gf_th_del(read->demuxer);*/

	/*just in case something went wrong
	while (gf_list_count(read->streams)) {
		M3U8Stream *st = gf_list_get(read->streams, 0);
		gf_list_rem(read->streams, 0);
		ogg_stream_clear(&st->os);
		if (st->dsi) gf_free(st->dsi);
		gf_free(st);
	}
	gf_list_del(read->streams);
	 */
	if (read){
		if (read->rootURL)
			gf_free(read->rootURL);
		read->rootURL = NULL;
		gf_free(read);
	}
	gf_free(plug);
}
