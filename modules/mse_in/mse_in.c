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

static u32 MSE_RegisterMimeTypes(const GF_InputService *plug)
{
	return 0;
}

static Bool MSE_CanHandleURL(GF_InputService *plug, const char *url)
{
    if (!plug || !url)
      return GF_FALSE;
    if (!strncmp(url, "blob:", 5)) {
        return GF_TRUE;
    } else {
        return GF_FALSE;
    }
}

static GF_HTML_SourceBuffer *MSE_GetSourceBufferForChannel(GF_HTML_MediaSource *mediasource, LPNETCHANNEL channel)
{
    if (gf_list_count(mediasource->sourceBuffers.list)) {
        GF_HTML_SourceBuffer *sb = (GF_HTML_SourceBuffer *)gf_list_get(mediasource->sourceBuffers.list, 0);
        if (sb) return sb;
    }
    return NULL;
}

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

static GF_Err MSE_ConnectChannel(GF_InputService *plug, LPNETCHANNEL channel, const char *url, Bool upstream)
{
    u32 ESID;
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_SourceBuffer *sb = MSE_GetSourceBufferForChannel(msein->mediasource, channel);
    if (!plug || !plug->priv || !sb || !sb->parser) return GF_SERVICE_ERROR;
    if (strstr(url, "ES_ID")) {
        GF_HTML_Track *track;
        sscanf(url, "ES_ID=%ud", &ESID);
        track = gf_mse_get_track_by_esid(sb, ESID);
        if (!track) {
            return GF_BAD_PARAM;
        } else {
            track->channel = channel;
            GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Channel Connection (%p) request from terminal for %s\n", channel, url));
            return sb->parser->ConnectChannel(sb->parser, channel, url, upstream);
        }
    } else {
        return GF_BAD_PARAM;
    }
}

static GF_Err MSE_DisconnectChannel(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_SourceBuffer *sb = MSE_GetSourceBufferForChannel(msein->mediasource, channel);
    if (!plug || !plug->priv || !sb || !sb->parser) return GF_SERVICE_ERROR;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Disconnect channel (%p) request from terminal \n", channel));
    return sb->parser->DisconnectChannel(sb->parser, channel);
}

static GF_Err MSE_ConnectService(GF_InputService *plug, GF_ClientService *serv, const char *url)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;

    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Service Connection request (%p) from terminal for %s\n", serv, url));

    if (!msein|| !serv || !url) return GF_BAD_PARAM;

    {
        GF_DOM_Event            mse_event;
        GF_Node                 *media_node;
        GF_HTML_MediaSource     *ms = NULL;

        sscanf(url, "blob:%p", &ms);
        msein->mediasource = ms;
        ms->service = serv;
        ms->readyState = 1;

        memset(&mse_event, 0, sizeof(GF_DOM_Event));
        mse_event.type = GF_EVENT_HTML_MSE_SOURCE_OPEN;
        ms->target = gf_mo_event_target_get(serv->owner->mo, 0);
        media_node = (GF_Node *)gf_event_target_get_node(ms->target);
        ms->node = media_node;
        gf_dom_event_fire(media_node, &mse_event);
        //gf_mo_event_target_remove_by_node(serv->owner->mo, media_node);
        //ms->target = gf_mo_event_target_add_object(serv->owner->mo, ms);
        //sg_fire_dom_event(ms->target, &mse_event, media_node->sgprivate->scenegraph, NULL);
    }
    return GF_OK;
}

static GF_Descriptor *MSE_GetServiceDesc(GF_InputService *plug, u32 expect_type, const char *sub_url)
{
    //GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    //if (msein->mediasource && gf_list_count(msein->mediasource->sourceBuffers.list)) 
    //{
    //    GF_HTML_SourceBuffer *sb = (GF_HTML_SourceBuffer *)gf_list_get(msein->mediasource->sourceBuffers.list, 0);
    //    if (!sb->service_desc) 
    //    { 
    //        /* WARNING: we should not reach this point, the descriptor should be fetched upon MSE connection */
    //        sb->service_desc = (GF_ObjectDescriptor *)sb->parser->GetServiceDescriptor(sb->parser, expect_type, sub_url);
    //    } 
    //    return (GF_Descriptor *)sb->service_desc;
    //}
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Service Description request from terminal for %s\n", sub_url));
    return NULL;
}

static GF_Err MSE_CloseService(GF_InputService *plug)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    assert( msein );
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Close Service (%p) request from terminal\n", msein->mediasource->service));

    gf_term_on_disconnect(msein->mediasource->service, NULL, GF_OK);

    return GF_OK;
}

static GF_Err MSE_ServiceCommand(GF_InputService *plug, GF_NetworkCommand *com)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_SourceBuffer *sb = NULL;

    if (!plug || !plug->priv || !com ) return GF_SERVICE_ERROR;

    /* channel independant commands */
    switch (com->command_type) {
    case GF_NET_SERVICE_INFO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Info command from terminal on Service (%p)\n", msein->mediasource->service));
        return GF_NOT_SUPPORTED;
    case GF_NET_SERVICE_HAS_AUDIO:
        GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Has Audio command from terminal on Service (%p)\n", msein->mediasource->service));
        return GF_OK;
    case GF_NET_SERVICE_QUALITY_SWITCH:
        return GF_NOT_SUPPORTED;
    }

    if (!com->base.on_channel) return GF_NOT_SUPPORTED;
    sb = MSE_GetSourceBufferForChannel(msein->mediasource, com->base.on_channel);
    if (!sb || !sb->parser) return GF_NOT_SUPPORTED;

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

static GF_Err MSE_ChannelGetSLP(GF_InputService *plug, LPNETCHANNEL channel, char **out_data_ptr, u32 *out_data_size, 
								GF_SLHeader *out_sl_hdr, Bool *sl_compressed, GF_Err *out_reception_status, Bool *is_new_data)
{
    GF_MSE_Packet *packet;
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_SourceBuffer *sb = MSE_GetSourceBufferForChannel(msein->mediasource, channel);
    GF_HTML_Track *track = MSE_GetTrackForChannel(sb, channel);
    if (!plug || !plug->priv || !sb || !sb->parser || !track) return GF_SERVICE_ERROR;
	gf_mse_track_buffer_get_next_packet(track, out_data_ptr, out_data_size, out_sl_hdr, sl_compressed, out_reception_status, is_new_data);
    return GF_OK;
}

static GF_Err MSE_ChannelReleaseSLP(GF_InputService *plug, LPNETCHANNEL channel)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_HTML_SourceBuffer *sb = MSE_GetSourceBufferForChannel(msein->mediasource, channel);
    GF_HTML_Track *track = MSE_GetTrackForChannel(sb, channel);
    if (!plug || !plug->priv || !sb || !sb->parser || !track) return GF_SERVICE_ERROR;
	gf_mse_track_buffer_release_packet(track);
    return GF_OK;
}

static Bool MSE_CanHandleURLInService(GF_InputService *plug, const char *url)
{
    GF_MSE_In *msein = (GF_MSE_In*) plug->priv;
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Received Can Handle URL In Service (%p) request from terminal for %s\n", msein->mediasource->service, url));
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



