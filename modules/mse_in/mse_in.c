/*
 *          GPAC - Multimedia Framework C SDK
 *
 *          Authors: Cyril Concolato, Jean Le Feuvre
 *          Copyright (c) Telecom ParisTech 2013-
 *                  All rights reserved
 *
 *  This file is part of GPAC / Media Source input module
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
#include <gpac/internal/terminal_dev.h>
#include <gpac/html5_mse.h>

typedef struct __mse_module 
{
    GF_HTML_MediaSource *mediasource;
    GF_InputService *plug;
} GF_MSE_In;

/* The MSE plugin has no MIME type associated, it cannot handle basic media resources (it needs a blob)*/
static u32 MSE_RegisterMimeTypes(const GF_InputService *plug)
{
	return 0;
}

/* Only Blob URL are supported */
static Bool MSE_CanHandleURL(GF_InputService *plug, const char *url)
{
    if (!plug || !url)
      return GF_FALSE;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received CanHandleURL request from terminal for URL '%s'\n", url));
    if (!strncmp(url, "blob:", 5)) {
        return GF_TRUE;
    } else {
        return GF_FALSE;
    }
}

/* Returns the source buffer associated with the terminal channel */
static GF_HTML_SourceBuffer *MSE_GetSourceBufferForChannel(GF_HTML_MediaSource *mediasource, LPNETCHANNEL channel)
{
	GF_Channel *ch;
	if (!channel || !mediasource) {
		return NULL;
	} else {
		u32 i;
		GF_InputService *parser;
		ch = (GF_Channel *) channel;
		assert(ch->odm && ch->odm->OD);
		parser = (GF_InputService *) ch->odm->OD->service_ifce;
		if (!parser) return NULL;
		for (i = 0; i < gf_list_count(mediasource->sourceBuffers.list); i++) {
			GF_HTML_SourceBuffer *sb = (GF_HTML_SourceBuffer *)gf_list_get(mediasource->sourceBuffers.list, i);
			if (sb && sb->parser && sb->parser == parser) return sb;
		}
		return NULL;
	}
}

/* For a given source buffer, returns the media track associated with the terminal channel object 
   Association is saved during the ConnectChannel operation */
static GF_HTML_Track *MSE_GetTrackForChannel(GF_HTML_SourceBuffer *sb, LPNETCHANNEL channel)
{
    u32 i;
    u32 count;
    count = gf_list_count(sb->tracks);
    for (i = 0; i < count; i++)
    {
        GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
        if (track->channel == channel)
        {
            return track;
        }
    }
    return NULL;
}

/* For a given source buffer, returns the media track associated with the given ESID
   Association is saved when the initialisation segment has been parsed */
static GF_HTML_Track *gf_mse_get_track_by_esid(GF_HTML_SourceBuffer *sb, u32 ESID)
{
    GF_HTML_Track *track;
    u32 i;
    u32 count;
    count = gf_list_count(sb->tracks);
    for (i=0; i<count; i++)
    {
        track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
        if (track->bin_id == ESID)
        {
            return track;
        }
    }
    return NULL;
}

/* We connect the channel only if the sourcebuffer has been created,
   and a track with the right ESID has been found in the init segment */
static GF_Err MSE_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
    u32 ESID;
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_SourceBuffer *sb = MSE_GetSourceBufferForChannel(msein->mediasource, channel);
    if (!plug || !plug->priv || !sb || !sb->parser) return GF_SERVICE_ERROR;
    if (strstr(url, "ES_ID")) {
        GF_HTML_Track *track;
        sscanf(url, "ES_ID=%u", &ESID);
        track = gf_mse_get_track_by_esid(sb, ESID);
        if (!track) {
            return GF_BAD_PARAM;
        } else {
            track->channel = channel;
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Channel Connection request on Service %p from terminal for URL '%s#%s'\n", channel, msein->mediasource->blobURI, url));
            return sb->parser->ConnectChannel(sb->parser, channel, url, upstream);
        }
    } else {
        return GF_BAD_PARAM;
    }
}

static GF_Err MSE_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_Track *track;
	GF_HTML_SourceBuffer *sb = MSE_GetSourceBufferForChannel(msein->mediasource, channel);
    if (!plug || !plug->priv || !sb || !sb->parser) return GF_SERVICE_ERROR;
	track = MSE_GetTrackForChannel(sb, channel);
	if (track) {
		track->channel = NULL;
	}
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Disconnect channel request on Service %p from terminal for channel %p\n", msein->mediasource->service, channel));
    return sb->parser->DisconnectChannel(sb->parser, channel);
}

/* Upon service connection, if the URL is a blobURL, we do the following:
   - get the associated MediaSource object, mark it as used
   - trigger a source event to the media node associated with the service */
static GF_Err MSE_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Service Connection request on Service %p from terminal for URL '%s'\n", serv, url));
    if (!msein|| !serv || !url) {
		return GF_BAD_PARAM;
	} else {
        GF_HTML_MediaSource     *ms = NULL;

        sscanf(url, "blob:%p", &ms);
        msein->mediasource = ms;
		ms->reference_count++;
        ms->service = serv;
		gf_mse_mediasource_open(ms, serv->owner->mo);
	    return GF_OK;
    }
}

/* There is no service description (no MPEG-4 IOD) for this module, the associated scene will be generated dynamically */
static GF_Descriptor *MSE_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Service Description request on Service %p from terminal for URL '%s'\n", msein->mediasource->service, sub_url));
    return NULL;
}

/* Indicate that the media source object is unused anymore */
static GF_Err MSE_CloseService(GF_InputService *plug)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    assert( msein );
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Close Service request on Service %p from terminal for URL '%s'\n", msein->mediasource->service, msein->mediasource->blobURI));
	if (msein->mediasource) {
		gf_term_on_disconnect(msein->mediasource->service, NULL, GF_OK);
		gf_mse_mediasource_del(msein->mediasource, GF_FALSE);
		msein->mediasource = NULL;
	}
    return GF_OK;
}

/* Forward all the commands received from the the terminal 
   to the parser associated with the channel on which the command is received */
static GF_Err MSE_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_SourceBuffer *sb = NULL;

    if (!plug || !plug->priv || !com ) return GF_SERVICE_ERROR;

    /* channel independant commands */
    switch (com->command_type) {
    case GF_NET_SERVICE_INFO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Info command from terminal on Service %p for URL '%s'\n", msein->mediasource->service, msein->mediasource->blobURI));
        return GF_NOT_SUPPORTED;
    case GF_NET_SERVICE_HAS_AUDIO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received HasAudio command from terminal on Service %p for URL '%s'\n", msein->mediasource->service, msein->mediasource->blobURI));
        return GF_OK;
    case GF_NET_SERVICE_QUALITY_SWITCH:
        return GF_NOT_SUPPORTED;
	case GF_NET_SERVICE_FLUSH_DATA:
		return GF_NOT_SUPPORTED;
    }

    if (!com->base.on_channel) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received unknown command %d from terminal on Service %p for URL '%s'\n", com->command_type, msein->mediasource->service, msein->mediasource->blobURI));
		return GF_NOT_SUPPORTED;
	}
    sb = MSE_GetSourceBufferForChannel(msein->mediasource, com->base.on_channel);
    if (!sb || !sb->parser) {
        GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] SourceBuffer not ready on Service %p for URL '%s'\n", msein->mediasource->service, msein->mediasource->blobURI));
		return GF_NOT_SUPPORTED;
	}

    switch (com->command_type) {
    case GF_NET_CHAN_PLAY:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_STOP:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_PAUSE:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_RESUME:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_SET_SPEED:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_CONFIG:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_DURATION:
        /* Ignore the duration given by the input service and 
           Note: the duration of the initial segment will be 0 anyway (in MP4).*/
        com->duration.duration = 0;
        return GF_OK;

    case GF_NET_CHAN_BUFFER:
        com->buffer.max = com->buffer.min = 0;
        return GF_OK;

    case GF_NET_CHAN_BUFFER_QUERY:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_GET_DSI:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_SET_PADDING:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_SET_PULL:
        return sb->parser->ServiceCommand(sb->parser, com);

    case GF_NET_CHAN_INTERACTIVE:
        /* we are interactive */
        return GF_OK;

    case GF_NET_CHAN_GET_PIXEL_AR:
        return sb->parser->ServiceCommand(sb->parser, com);

    default:
        return sb->parser->ServiceCommand(sb->parser, com);
    }
}

/* Forward the request for a new packet to the track (ask the parser to parse a new buffer or use already parsed AU) */
static GF_Err MSE_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, 
								GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_SourceBuffer *sb = MSE_GetSourceBufferForChannel(msein->mediasource, channel);
    GF_HTML_Track *track = MSE_GetTrackForChannel(sb, channel);
    if (!plug || !plug->priv || !sb || !sb->parser || !track) return GF_SERVICE_ERROR;
	gf_mse_track_buffer_get_next_packet(track, out_data_ptr, out_data_size, out_sl_hdr, sl_compressed, out_reception_status, is_new_data);
    return GF_OK;
}

/* Indicate to the track that a packet can be released (and potentially deleted) */
static GF_Err MSE_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_SourceBuffer *sb = MSE_GetSourceBufferForChannel(msein->mediasource, channel);
    GF_HTML_Track *track = MSE_GetTrackForChannel(sb, channel);
    if (!plug || !plug->priv || !sb || !sb->parser || !track) return GF_SERVICE_ERROR;
	gf_mse_track_buffer_release_packet(track);
    return GF_OK;
}

/* This module can only handle one blobURL at a time */
static Bool MSE_CanHandleURLInService(GF_InputService *plug, const char *url)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received CanHandleURLInService request on service %p from terminal for URL '%s'\n", msein->mediasource->service, url));
    if (!plug || !plug->priv) return GF_FALSE;
	if (!strcmp(url, msein->mediasource->blobURI)) {
		return GF_TRUE;
	} else {
		return GF_FALSE;
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
    GF_MSE_In *msein;
    GF_InputService *plug;
    if (InterfaceType != GF_NET_CLIENT_INTERFACE) return NULL;

    GF_SAFEALLOC(plug, GF_InputService);
    GF_REGISTER_MODULE_INTERFACE(plug, GF_NET_CLIENT_INTERFACE, "GPAC MSE Loader", "gpac distribution")
    plug->RegisterMimeTypes = MSE_RegisterMimeTypes;
    plug->CanHandleURL = MSE_CanHandleURL;
    plug->ConnectService = MSE_ConnectService;
    plug->CloseService = MSE_CloseService;
    plug->GetServiceDescriptor = MSE_GetServiceDesc;
    plug->ConnectChannel = MSE_ConnectChannel;
    plug->DisconnectChannel = MSE_DisconnectChannel;
    plug->ServiceCommand = MSE_ServiceCommand;
    plug->CanHandleURLInService = MSE_CanHandleURLInService;
    plug->ChannelGetSLP = MSE_ChannelGetSLP;
    plug->ChannelReleaseSLP = MSE_ChannelReleaseSLP;
    GF_SAFEALLOC(msein, GF_MSE_In);
    plug->priv = msein;
    msein->plug = plug;
    return (GF_BaseInterface *)plug;
}
GPAC_MODULE_EXPORT
void ShutdownInterface(GF_BaseInterface *bi)
{
    GF_MSE_In *msein;

    if (bi->InterfaceType!=GF_NET_CLIENT_INTERFACE) return;

    msein = (GF_MSE_In*) ((GF_InputService*)bi)->priv;
    assert(msein);
    gf_free(msein);
    gf_free(bi);
}

GPAC_MODULE_STATIC_DELARATION( mse_in )
