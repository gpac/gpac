/*
*			GPAC - Multimedia Framework C SDK
*
 *			Authors: Cyril COncolato
 *			Copyright (c) Telecom ParisTech 2013-
*					All rights reserved
*
*  This file is part of GPAC / Media Source
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
#include <gpac/setup.h>

#ifdef GPAC_HAS_SPIDERMONKEY
#include <gpac/html5_mse.h>

GF_HTML_MediaSource *gf_mse_media_source_new()
{
    GF_HTML_MediaSource *p;
    GF_SAFEALLOC(p, GF_HTML_MediaSource);
    p->sourceBuffers.list = gf_list_new();
    p->activeSourceBuffers.list = gf_list_new();
    return p;
}

GF_HTML_SourceBuffer *gf_mse_source_buffer_new(GF_HTML_MediaSource *mediasource)
{
    char name[256];
    GF_HTML_SourceBuffer *source;
    GF_SAFEALLOC(source, GF_HTML_SourceBuffer);
    sprintf(name, "SourceBuffer_Thread_%p", source);
    source->mediasource = mediasource;
    source->buffered.times = gf_list_new();
    source->input_buffer = gf_list_new();
    source->tracks = gf_list_new();
    source->parser_thread = gf_th_new(name);
    source->parser_mutex = gf_mx_new(name);
    source->abort_mode = MEDIA_SOURCE_ABORT_MODE_NONE;
    source->appendWindowStart = 0;
    source->appendWindowEnd = GF_MAX_DOUBLE;
    gf_list_add(mediasource->sourceBuffers.list, source);
    return source;
}

void gf_mse_source_buffer_del(GF_HTML_SourceBuffer *sb)
{
    GF_HTML_TrackList tlist;
    gf_html_timeranges_del(&sb->buffered);
    gf_list_del(sb->buffered.times);
    gf_list_del(sb->input_buffer);
    tlist.tracks = sb->tracks;
    gf_html_tracklist_del(&tlist);
    gf_free(sb->parser_thread);
    gf_free(sb->parser_mutex);
    gf_free(sb);
}

/*locates input service (demuxer, parser) based on mime type or segment name*/
GF_Err gf_mse_source_buffer_load_parser(GF_HTML_SourceBuffer *sourcebuffer, const char *mime)
{
    GF_InputService *parser = NULL;

    const char *sPlug;
    if (mime) {
        /* Check MIME type to start the right InputService */
        sPlug = gf_cfg_get_key(sourcebuffer->mediasource->service->term->user->config, "MimeTypes", mime);
        if (sPlug) sPlug = strrchr(sPlug, '"');
        if (sPlug) {
            sPlug += 2;
            parser = (GF_InputService *) gf_modules_load_interface_by_name(sourcebuffer->mediasource->service->term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
        }
    }
    if (parser) 
    {
        sourcebuffer->parser = parser;
        parser->query_proxy = gf_mse_proxy;
        parser->proxy_udta = sourcebuffer;
        parser->proxy_type = GF_TRUE;
        return GF_OK;
    }
    else 
    {
        GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MSE] Error locating plugin for source - mime type %s\n", mime));
        return GF_BAD_PARAM;
    }
}

static GF_HTML_Track *gf_mse_source_buffer_add_track(GF_HTML_SourceBuffer *sb, GF_ESD *esd)
{
    GF_HTML_Track *track;
    GF_HTML_TrackType type;

    track = NULL;
    type = HTML_MEDIA_TRACK_TYPE_UNKNOWN;
    if (esd->decoderConfig->streamType==GF_STREAM_VISUAL) 
    {
        type = HTML_MEDIA_TRACK_TYPE_VIDEO;
    } 
    else if (esd->decoderConfig->streamType==GF_STREAM_AUDIO) 
    {
        type = HTML_MEDIA_TRACK_TYPE_AUDIO;
    } 
    else 
    {
        /* TODO: Text tracks */
    }

    track = gf_html_media_track_new(type, "", GF_TRUE, "", "", "", "");
    if (track)
    {
        char mx_name[256];
        track->bin_id = esd->ESID;
        track->buffer = gf_list_new();
        sprintf(mx_name, "track_mutex_%d", track->bin_id);
        track->buffer_mutex = gf_mx_new(mx_name);
        track->timescale = esd->slConfig->timestampResolution;
        gf_list_add(sb->tracks, track);
    }
    return track;
}

static GF_Err gf_mse_source_buffer_setup_tracks(GF_HTML_SourceBuffer *sb)
{
    sb->service_desc = (GF_ObjectDescriptor *)sb->parser->GetServiceDescriptor(sb->parser, GF_MEDIA_OBJECT_UNDEF, NULL);
    if (sb->service_desc) 
    {
        u32 count;
        count = gf_list_count(sb->service_desc->ESDescriptors);
        if (count)
        {
            u32 i;
            for (i = 0; i < count; i++)
            {
                GF_ESD *esd = (GF_ESD *)gf_list_get(sb->service_desc->ESDescriptors, i);
                gf_mse_source_buffer_add_track(sb, esd);
            }
        }
        return GF_OK;
    } else {
        return GF_BAD_PARAM;
    }
}

static GF_Err gf_mse_source_buffer_store_track_desc(GF_HTML_SourceBuffer *sb, GF_ObjectDescriptor *od)
{
    u32 i;
    u32 count;
    GF_ESD *esd;
    Bool found = GF_FALSE;
    GF_HTML_Track *track;

    esd = (GF_ESD *)gf_list_get(od->ESDescriptors, 0);
    count = gf_list_count(sb->tracks);
    for (i = 0; i < count; i++)
    {
        track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
        if (track->bin_id == esd->ESID)
        {
            track->od = od;
            found = GF_TRUE;
            break;
        }
    }
    if (!found)
    {
        track = gf_mse_source_buffer_add_track(sb, esd);
        if (track)
        {
            track->od = od;
        } else {
            /* TODO: error */
            return GF_BAD_PARAM;
        }
    } 
    return GF_OK;
}

void gf_mse_source_buffer_update_buffered(GF_HTML_SourceBuffer *sb)
{
    u32 i;
    u32 track_count;
    double start= 0;
    double end = 0;
    Bool start_set = GF_FALSE;
    Bool end_set = GF_FALSE;

    /* cleaning the current list */
    gf_html_timeranges_del(&(sb->buffered));

    /* merging the start and end for all tracks */
    track_count = gf_list_count(sb->tracks);
    for (i = 0; i < track_count; i++)
    {
        u32 j;
        u32 packet_count;
        GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
        gf_mx_p(track->buffer_mutex);
        packet_count = gf_list_count(track->buffer);
        for (j = 0; j < packet_count; j++)
        {
            GF_MSE_Packet *packet = (GF_MSE_Packet *)gf_list_get(track->buffer, j);
            if (packet) {
                double packet_start = (packet->sl_header.compositionTimeStamp * 1.0 )/ track->timescale;
                double packet_end = ((packet->sl_header.compositionTimeStamp + packet->sl_header.au_duration) * 1.0) / track->timescale;
                if (!start_set) 
                {
                    start = packet_start;
                    start_set = GF_TRUE;
                } 
                else 
                {
                    if (start > packet_start)
                    {
                        start = packet_start;
                    }
                }
                if (!end_set) 
                {
                    end = packet_end;
                    end_set = GF_TRUE;
                }
                else
                {
                    if (end < packet_end)
                    {
                        end = packet_end;
                    }
                }
            }
        }
        gf_mx_v(track->buffer_mutex);
    }

    /* Creating only one range for now */
    if (start_set && end_set) {
        gf_media_time_ranges_add(&sb->buffered, start, end);
    }
}

static void gf_mse_source_buffer_reset_parser(GF_HTML_SourceBuffer *sb)
{
    u32 i, track_count;
    track_count = gf_list_count(sb->tracks);

    /* wait until all remaining entire AU are parsed and then flush the remaining bytes in the parser */

    for (i = 0; i < track_count; i++) 
    {
        GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
        track->last_dts_set     = GF_FALSE;
        track->highest_pts_set  = GF_FALSE;
        track->needs_rap        = GF_TRUE;
    }
    sb->highest_end_timestamp_set = GF_FALSE;
    while (gf_list_count(sb->input_buffer)) {
        GF_MSE_Packet *packet = (GF_MSE_Packet *)gf_list_rem(sb->input_buffer, 0);
        gf_mse_packet_del(packet);
    }
    sb->append_state = MEDIA_SOURCE_APPEND_STATE_WAITING_FOR_SEGMENT;
}

GF_Err gf_mse_source_buffer_abort(GF_HTML_SourceBuffer *sb, GF_HTML_MediaSource_AbortMode mode)
{
    if (sb->continuation_timestamp_flag == GF_FALSE)
    {
        if (sb->abort_mode == MEDIA_SOURCE_ABORT_MODE_CONTINUATION && !sb->highest_end_timestamp_set)
        {
            return GF_BAD_PARAM;
        }

        if (sb->highest_end_timestamp_set) {
            sb->continuation_timestamp = sb->highest_end_timestamp;
            sb->continuation_timestamp_flag = GF_TRUE;
        }
    }

    if (sb->updating) 
    {
        sb->updating = GF_FALSE;
    }
    sb->abort_mode = mode;
    sb->appendWindowStart = 0;
    sb->appendWindowEnd = GF_MAX_DOUBLE;

    gf_mse_source_buffer_reset_parser(sb);
    return GF_OK;
}

void gf_mse_packet_del(GF_MSE_Packet *packet)
{
    gf_free(packet->data);
    gf_free(packet);
}

static GF_MSE_Packet *gf_mse_find_overlapped_packet(GF_HTML_Track           *track, 
                                                    GF_MSE_Packet           *packet)
{
    u32     i;
    u32     count;
    Bool    found_previous;
    
    found_previous = GF_FALSE;
    gf_mx_p(track->buffer_mutex);
    count = gf_list_count(track->buffer);
    for (i = 0; i < count; i++)
    {
        GF_MSE_Packet *p = (GF_MSE_Packet *)gf_list_get(track->buffer, i);
        if (p->sl_header.compositionTimeStamp < packet->sl_header.compositionTimeStamp) {
            found_previous = GF_TRUE;
        }
        if (found_previous == GF_TRUE && p->sl_header.compositionTimeStamp > packet->sl_header.compositionTimeStamp) {
            return p;
        }
    }
    gf_mx_v(track->buffer_mutex);
    return NULL;
}

static void gf_mse_remove_frames_from_to(GF_HTML_Track *track, 
                                         u64           from,
                                         u64           to)
{
    u32     i;
    u32     frame_count;
    //Bool    found_previous;

    //found_previous = GF_FALSE;
    gf_mx_p(track->buffer_mutex);
    frame_count = gf_list_count(track->buffer);
    for (i = 0; i < frame_count; i++)
    {
        GF_MSE_Packet *frame = (GF_MSE_Packet *)gf_list_get(track->buffer, i);
        if (frame->sl_header.compositionTimeStamp >= from && frame->sl_header.compositionTimeStamp < to) 
        {
            GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Removing frame %g (%d frames)\n", (frame->sl_header.compositionTimeStamp*1.0)/track->timescale, gf_list_count(track->buffer)));
            gf_list_rem(track->buffer, i);
        }
    }
    gf_mx_v(track->buffer_mutex);
}

static GF_Err gf_mse_process_coded_frame(GF_HTML_SourceBuffer    *sb, 
                                       GF_HTML_Track           *track, 
                                       GF_MSE_Packet           *frame)
{
    if (sb->abort_mode != MEDIA_SOURCE_ABORT_MODE_NONE) 
    {
        switch (sb->abort_mode)
        {
        case MEDIA_SOURCE_ABORT_MODE_CONTINUATION:
            sb->timestampOffset = sb->continuation_timestamp - (frame->sl_header.compositionTimeStamp*1.0/track->timescale);
            break;
        case MEDIA_SOURCE_ABORT_MODE_OFFSET:
            sb->timestampOffset -= (frame->sl_header.compositionTimeStamp*1.0/track->timescale);
            break;
        default:
        	break;
        }
        sb->continuation_timestamp_flag = GF_FALSE;
        sb->abort_mode = MEDIA_SOURCE_ABORT_MODE_NONE;
    }

    if (sb->timestampOffset < 0) {
        u64 offset = (u64)((-sb->timestampOffset)*track->timescale);
        if (offset > frame->sl_header.compositionTimeStamp || offset > frame->sl_header.decodingTimeStamp) 
        {
             return GF_NON_COMPLIANT_BITSTREAM;
        }
    }
    frame->sl_header.compositionTimeStamp  += (u64)(sb->timestampOffset*track->timescale);
    frame->sl_header.decodingTimeStamp     += (u64)(sb->timestampOffset*track->timescale);

    if (track->last_dts_set) 
    {
        if (track->last_dts*track->timescale > frame->sl_header.decodingTimeStamp) 
        {
            return GF_NON_COMPLIANT_BITSTREAM;
        }

        /* why ??? 
         * If last decode timestamp for track buffer is set and decode timestamp is less than last decode timestamp 
         * or the difference between decode timestamp and last decode timestamp is greater than 100 milliseconds, 
         * then call endOfStream("decode") and abort these steps.
         */
        if (frame->sl_header.decodingTimeStamp - track->last_dts*track->timescale > 0.1*track->timescale)
        {
            return GF_NON_COMPLIANT_BITSTREAM;
        }
    }

    if (frame->sl_header.compositionTimeStamp < sb->appendWindowStart*track->timescale) {
        track->needs_rap = GF_TRUE;
        return GF_OK;
    }

    if (frame->sl_header.compositionTimeStamp /* + dur */ > sb->appendWindowEnd*track->timescale) {
        track->needs_rap = GF_TRUE;
        return GF_OK;
    }

    if (track->needs_rap) 
    {
        if (!frame->sl_header.randomAccessPointFlag) {
            return GF_OK;
        }
        track->needs_rap = GF_FALSE;
    }

    if (!track->last_dts_set) {
        /* find a frame in the track buffer whose PTS is less than the current packet and whose PTS + dur is greater than the current packet */
        GF_MSE_Packet           *overlapped_packet;
        overlapped_packet = gf_mse_find_overlapped_packet(track, frame);
        if (overlapped_packet) {
            gf_mse_remove_frames_from_to(track, overlapped_packet->sl_header.compositionTimeStamp, overlapped_packet->sl_header.compositionTimeStamp + (u64)(0.000001*track->timescale));
        }
    } 

    if (!track->highest_pts_set) 
    {
        /* this is the first time a frame is processed in the append sequence */
        gf_mse_remove_frames_from_to(track, frame->sl_header.compositionTimeStamp, frame->sl_header.compositionTimeStamp /* + dur */);
    } 
    else if (track->highest_pts*track->timescale <= frame->sl_header.compositionTimeStamp) 
    {
        /* the highest pts has already been set in this append sequence, so we just need to remove frames from that point on, it's safe */
        gf_mse_remove_frames_from_to(track, (u64)(track->highest_pts*track->timescale), (u64)(track->highest_pts*track->timescale) /* + dur */);
    }

    /* remove dependencies: no way !! */

    /* TODO: spliced frames */

    gf_mx_p(track->buffer_mutex);
    gf_list_add(track->buffer, frame);
    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Adding frame %g (%d frames)\n", (frame->sl_header.compositionTimeStamp*1.0)/track->timescale, gf_list_count(track->buffer)));
    gf_mx_v(track->buffer_mutex);

    track->last_dts = (frame->sl_header.decodingTimeStamp*1.0/track->timescale);
    track->last_dts_set = GF_TRUE;

    if (!track->highest_pts_set || (frame->sl_header.compositionTimeStamp /* + dur */) > track->highest_pts*track->timescale)
    {
        track->highest_pts_set = GF_TRUE;
        track->highest_pts = (frame->sl_header.compositionTimeStamp*1.0/track->timescale /* + dur */);
    }

    if (!sb->highest_end_timestamp_set || (frame->sl_header.compositionTimeStamp*1.0 /* + dur */) > sb->highest_end_timestamp * track->timescale)
    {
        sb->highest_end_timestamp_set = GF_TRUE;
        sb->highest_end_timestamp = (frame->sl_header.compositionTimeStamp*1.0/track->timescale /* + dur */);
    }

    return GF_OK;
}

u32 gf_mse_parse_segment(void *par)
{
    GF_MSE_Packet           *packet;
    GF_HTML_Track           *track;
    GF_HTML_SourceBuffer    *sb = (GF_HTML_SourceBuffer *)par;
    u32                     i;
    u32                     track_count;
    u32                     step;

    /* step 0 is for forcing the parser to query the next fragment (if any and if needed), step 1 is for actual parsing */
    step = 0;

    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Starting to fetch AUs from parser\n"));

    /* For each track, ask the parser for all the Access Unit, until it is empty
     * AU are placed as GF_MSE_Packets in the track buffer 
     */
    track_count = gf_list_count(sb->tracks);
    while (sb->updating)
    {
        u32 track_with_data = 0;
        for (i = 0; i < track_count; i++)
        {
            GF_Err e;
            track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
            GF_SAFEALLOC(packet, GF_MSE_Packet);
            e = sb->parser->ChannelGetSLP(sb->parser, track->channel, 
                                          &packet->data, &packet->size, &packet->sl_header, 
                                          &packet->is_compressed, &packet->status, &packet->is_new_data);
            if (e == GF_OK && packet->is_new_data && packet->size) 
            {
                char *data = (char *)gf_malloc(packet->size);
                GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] New AU parsed %g\n", (packet->sl_header.compositionTimeStamp*1.0/track->timescale)));
                memcpy(data, packet->data, packet->size);
                packet->data = data;
                gf_mse_process_coded_frame(sb, track, packet);
                track_with_data++;
                sb->parser->ChannelReleaseSLP(sb->parser, track->channel);
            }
        }
        if (!track_with_data)
        {
            if (step) 
            {
                /* try to delete the previous buffer */
                gf_arraybuffer_del((GF_HTML_ArrayBuffer *)sb->prev_buffer, GF_FALSE);
                /* get ready to receive a new segment and start a new thread */
                sb->updating = GF_FALSE;
                break;
            }
            step++; /* forcing a new call to the parser (the first call only fetched the next URL and did not parse anything)*/
        }
    }

    /* reaching here, the parser does not have enough data to produce new AU */

    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Done fetching AUs from parser\n"));
    return 0;
}


void gf_mse_source_buffer_append_arraybuffer(GF_HTML_SourceBuffer *sb, GF_HTML_ArrayBuffer *buffer)
{
    if (!sb || !buffer || !sb->parser || sb->updating) {
        /* TODO: throw invalid access err or invalid state exception */
        /* TODO: handle buffer full flag */
        /* TODO: check the status of the media source */
        return;
    } else {
        sb->updating = GF_TRUE;
        /* TODO: fire an updatestart event */

        buffer->url = (char *)gf_malloc(256);
        sprintf(buffer->url, "gmem://%d@%p", buffer->length, buffer->data);
        buffer->reference_count++;
        GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Appending segment %s\n", buffer->url));

        /* TODO: detect if the buffer contains an initialization segment or a media segment */
        if (!sb->parser_connected) 
        {
            /* we expect an initialization segment to connect the service */
            sb->parser->ConnectService(sb->parser, sb->mediasource->service, buffer->url);
        } else {
            /* we expect a media segment */
            gf_list_add(sb->input_buffer, buffer);
            /* Call the parser (asynchronously) and return */
            /* the updating attribute should be positioned back to 0 when the parser is done */
            gf_th_run(sb->parser_thread, gf_mse_parse_segment, sb);
        }

    }
}

/*
FIXME : Unused function, create warnings on debian
static void gf_mse_source_buffer_append_error(GF_HTML_SourceBuffer *sb)
{
    sb->updating = GF_FALSE;
    gf_mse_source_buffer_reset_parser(sb);
    TODO: fire events
}
*/


u32 gf_mse_source_buffer_remove(void *par)
{
    GF_HTML_SourceBuffer    *sb = (GF_HTML_SourceBuffer *)par;
    u32                     i;
    u32                     j;
    u32                     track_count;
    u32                     frame_count;
    u64                     end = 0;
    //Bool                    end_set;

    track_count = gf_list_count(sb->tracks);
    for (i = 0; i < track_count; i++)
    {
        GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
        //end_set = GF_FALSE;

        /* find the next random access point */
        gf_mx_p(track->buffer_mutex);
        frame_count = gf_list_count(track->buffer);
        for (j = 0; j < frame_count; j++) 
        {
            GF_MSE_Packet *frame = (GF_MSE_Packet *)gf_list_get(track->buffer, j);
            if ((frame->sl_header.randomAccessPointFlag && 
                 frame->sl_header.compositionTimeStamp >= sb->remove_end*track->timescale) ||
                 (j == frame_count - 1))
            {
                end = frame->sl_header.compositionTimeStamp;
                //end_set = GF_TRUE;
                break;
            }
        }
        gf_mx_v(track->buffer_mutex);

        /* remove up to the next RAP found */
        gf_mse_remove_frames_from_to(track, (u64)sb->remove_start, end);
    }
    sb->updating = GF_FALSE;
    return 0;
}

GF_Err gf_mse_proxy(GF_InputService *parser, GF_NetworkCommand *command)
{
    if (!parser || !command || !parser->proxy_udta) {
        return GF_BAD_PARAM;
    } else {
        GF_HTML_SourceBuffer *sb = (GF_HTML_SourceBuffer *)parser->proxy_udta;
        switch (command->command_type)
        {
        case GF_NET_SERVICE_QUERY_INIT_RANGE:
            /* The parser is asking for an initialization segment:
               check for the media time and give the appropriate one */
            break;
        case GF_NET_SERVICE_QUERY_NEXT:
            /* The parser is asking for the next media segment in the buffer,
               check for the media time and give the right one */
            {
                GF_HTML_ArrayBuffer *buffer;
                /* The input buffer should not be modified by append operations at the same time, no need to protect access */
                buffer = (GF_HTML_ArrayBuffer *)gf_list_get(sb->input_buffer, 0);
                if (buffer)
                {
                    GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Next segment to parse %s\n", buffer->url));
                    command->url_query.next_url = buffer->url;
                    command->url_query.discontinuity_type = 0;
                    command->url_query.start_range = 0;
                    command->url_query.end_range = 0;
                    command->url_query.switch_start_range = 0;
                    command->url_query.switch_end_range = 0;
                    command->url_query.next_url_init_or_switch_segment = NULL;
                    gf_list_rem(sb->input_buffer, 0);
                    sb->prev_buffer = buffer;
                } else {
                    command->url_query.next_url = NULL;
                    command->url_query.discontinuity_type = 0;
                }
            }
            break;
        case GF_NET_SERVICE_STATUS_PROXY:
            /* add media request from underlying parser */
            if (command->status.is_add_media)
            {
                if (command->status.desc)
                {
                    gf_mse_source_buffer_store_track_desc(sb, (GF_ObjectDescriptor *)command->status.desc);
                } 
                else
                {
                    /* this is the last add media, we can switch updating to false */
                    /* the first init segment was correctly processed */
                    sb->updating = GF_FALSE;
                    /* TODO: set active tracks and send addsourcebuffer event */
                    /* TODO: send media loadedmetadata event */
                }
                gf_term_add_media(sb->mediasource->service, command->status.desc, (command->status.desc ? GF_TRUE : GF_FALSE));
            }
            /* general connection/disconnection messages from the media parser (not track related) */
            else if (!command->status.channel) 
            { 
                /* connection message */
                if (!command->status.is_disconnect) {
                    if (command->status.e == GF_OK) {
                        /* nothing needs to be done. Setup is done with final add media */
                        sb->parser_connected = GF_TRUE;
                        sb->mediasource->durationType = DURATION_INFINITY;
                        gf_mse_source_buffer_setup_tracks(sb);
                    } else {
                        /* wrong first init segment */
                        /* TODO: fire an error event */
                    }
                    /* TODO: fire an updateend event */
                    gf_term_on_connect(sb->mediasource->service, command->status.channel, command->status.e);
                } else {
                    gf_term_on_disconnect(sb->mediasource->service, command->status.channel, command->status.e);
                }
            }
            /* channel (track related) specific connection/disconnection messages from the media parser */
            else 
            {
                if (!command->status.is_disconnect) {
                    gf_term_on_connect(sb->mediasource->service, command->status.channel, command->status.e);
                } else {
                    gf_term_on_disconnect(sb->mediasource->service, command->status.channel, command->status.e);
                }
            }
            break;
        default:
            gf_term_on_command(sb->mediasource->service, command, GF_OK);
            break;
        }
        return GF_OK;
    }
}

#define MSE_TRACK_BUFFER_LENGTH 1000 // number of AU per track 

GF_EXPORT
GF_Err gf_mse_track_buffer_release_packet(GF_HTML_Track *track) {
	GF_MSE_Packet *packet;
	gf_mx_p(track->buffer_mutex);
	packet = (GF_MSE_Packet *)gf_list_get(track->buffer, track->packet_index);
	if (packet) {
		track->packet_index++;
		if (gf_list_count(track->buffer) > MSE_TRACK_BUFFER_LENGTH) {
			packet = (GF_MSE_Packet *)gf_list_get(track->buffer, 0);
			track->packet_index--;
			gf_list_rem(track->buffer, 0);
			gf_free(packet->data);
			gf_free(packet);
		}
	}
	gf_mx_v(track->buffer_mutex);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_mse_track_buffer_get_next_packet(GF_HTML_Track *track,
											char **out_data_ptr, u32 *out_data_size, 
											GF_SLHeader *out_sl_hdr, Bool *sl_compressed, 
											GF_Err *out_reception_status, Bool *is_new_data)
{
	GF_MSE_Packet *packet;
	gf_mx_p(track->buffer_mutex);
	packet = (GF_MSE_Packet *)gf_list_get(track->buffer, track->packet_index);
	if (packet)
	{
		*out_data_ptr = packet->data;
		*out_data_size = packet->size;
		*out_sl_hdr = packet->sl_header;
		*sl_compressed = packet->is_compressed;
		*out_reception_status = packet->status;
		*is_new_data = packet->is_new_data;
		packet->is_new_data = GF_FALSE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Sending AU to decoder with TS: %g (%d frames)\n", (packet->sl_header.compositionTimeStamp*1.0/track->timescale), gf_list_count(track->buffer)));
	} else {
		*out_data_ptr = NULL;
		*out_data_size = 0;
		*sl_compressed = GF_FALSE;
		*out_reception_status = GF_OK;
		*is_new_data = GF_FALSE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_In] No AU for decoder\n"));
	}
	gf_mx_v(track->buffer_mutex);
	return GF_OK;
}
#endif
