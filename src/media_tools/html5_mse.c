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
#include <gpac/internal/isomedia_dev.h>

GF_HTML_MediaSource *gf_mse_media_source_new()
{
	GF_HTML_MediaSource *ms;
	GF_SAFEALLOC(ms, GF_HTML_MediaSource);
	ms->sourceBuffers.list = gf_list_new();
	ms->sourceBuffers.parent = ms;
	ms->sourceBuffers.evt_target = gf_dom_event_target_new(GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFERLIST, &ms->sourceBuffers);
	ms->activeSourceBuffers.list = gf_list_new();
	ms->activeSourceBuffers.parent = ms;
	ms->activeSourceBuffers.evt_target = gf_dom_event_target_new(GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFERLIST, &ms->activeSourceBuffers);
	ms->reference_count = 1;
	ms->evt_target = gf_dom_event_target_new(GF_DOM_EVENT_TARGET_MSE_MEDIASOURCE, ms);
	ms->durationType = DURATION_NAN;
	return ms;
}

GF_EXPORT
void gf_mse_mediasource_del(GF_HTML_MediaSource *ms, Bool del_js)
{
	if (ms) {
		if (del_js) {
			/* finalize the object from the JS perspective */
			ms->c = NULL;
			ms->_this = NULL;
		}
		/* only delete the object if it is not used elsewhere */
		if (ms->reference_count) {
			ms->reference_count--;
		}
		if (!ms->reference_count) {
			u32 i;
			for (i = 0; i < gf_list_count(ms->sourceBuffers.list); i++) {
				GF_HTML_SourceBuffer *sb = (GF_HTML_SourceBuffer *)gf_list_get(ms->sourceBuffers.list, i);

				if (sb->parser && sb->parser_connected)
					sb->parser->CloseService(sb->parser);

				gf_mse_source_buffer_del(sb);
			}
			gf_list_del(ms->sourceBuffers.list);
			/* all source buffer should have been deleted in the deletion of sourceBuffers */
			gf_list_del(ms->activeSourceBuffers.list);
			if (ms->blobURI) gf_free(ms->blobURI);
			gf_free(ms->evt_target);
			gf_free(ms);
		}
	}
}

void gf_mse_fire_event(GF_DOMEventTarget *target, GF_EventType event_type)
{
	GF_SceneGraph *sg = NULL;
	GF_DOM_Event  mse_event;
	memset(&mse_event, 0, sizeof(GF_DOM_Event));
	mse_event.type = event_type;
	mse_event.target = target->ptr;
	mse_event.target_type = target->ptr_type;
	switch(target->ptr_type) {
	case GF_DOM_EVENT_TARGET_MSE_MEDIASOURCE:
	{
		GF_HTML_MediaSource *ms = (GF_HTML_MediaSource *)target->ptr;
		sg = ms->sg;
	}
	break;
	case GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFERLIST:
	{
		GF_HTML_SourceBufferList *list = (GF_HTML_SourceBufferList *)target->ptr;
		sg = list->parent->sg;
	}
	break;
	case GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFER:
	{
		GF_HTML_SourceBuffer *sb = (GF_HTML_SourceBuffer *)target->ptr;
		sg = sb->mediasource->sg;
	}
	break;
	default:
		break;
	}
	assert(sg);
	gf_sg_fire_dom_event(target, &mse_event, sg, NULL);
}

GF_EXPORT
void gf_mse_mediasource_open(GF_HTML_MediaSource *ms, struct _mediaobj *mo)
{
	if (!ms) return;
	if (!mo) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[MSE] Cannot open media source without a media object\n"));
		return;
	}
	ms->readyState = MEDIA_SOURCE_READYSTATE_OPEN;
	/* getting the Scene Tree node attached to this media source */
	ms->node = (GF_Node *)gf_event_target_get_node(gf_mo_event_target_get(mo, 0));
	ms->sg = gf_node_get_graph(ms->node);
	gf_mse_fire_event(ms->evt_target, GF_EVENT_HTML_MSE_SOURCE_OPEN);
}

GF_EXPORT
void gf_mse_mediasource_close(GF_HTML_MediaSource *ms)
{
	if (!ms) return;
	ms->readyState = MEDIA_SOURCE_READYSTATE_CLOSED;
	gf_mse_fire_event(ms->evt_target, GF_EVENT_HTML_MSE_SOURCE_CLOSE);
}

GF_EXPORT
void gf_mse_mediasource_end(GF_HTML_MediaSource *ms)
{
	if (!ms) return;
	ms->readyState = MEDIA_SOURCE_READYSTATE_ENDED;
	gf_mse_fire_event(ms->evt_target, GF_EVENT_HTML_MSE_SOURCE_ENDED);
}

GF_HTML_SourceBuffer *gf_mse_source_buffer_new(GF_HTML_MediaSource *mediasource)
{
	char name[256];
	GF_HTML_SourceBuffer *source;
	GF_SAFEALLOC(source, GF_HTML_SourceBuffer);
	sprintf(name, "SourceBuffer_Thread_%p", source);
	source->mediasource = mediasource;
	source->buffered = gf_html_timeranges_new(1);
	source->input_buffer = gf_list_new();
	source->tracks = gf_list_new();
	source->threads = gf_list_new();
	source->parser_thread = gf_th_new(name);
	source->remove_thread = gf_th_new(name);
	source->append_mode = MEDIA_SOURCE_APPEND_MODE_SEGMENTS;
	source->appendWindowStart = 0;
	source->appendWindowEnd = GF_MAX_DOUBLE;
	source->evt_target = gf_dom_event_target_new(GF_DOM_EVENT_TARGET_MSE_SOURCEBUFFER, source);
	source->timescale = 1;
	return source;
}

void gf_mse_mediasource_add_source_buffer(GF_HTML_MediaSource *ms, GF_HTML_SourceBuffer *sb)
{
	gf_list_add(ms->sourceBuffers.list, sb);
	gf_mse_fire_event(ms->sourceBuffers.evt_target, GF_EVENT_HTML_MSE_ADD_SOURCE_BUFFER);
}

/* Not yet used
void gf_mse_add_active_source_buffer(GF_HTML_MediaSource *ms, GF_HTML_SourceBuffer *sb)
{
    gf_list_add(ms->activeSourceBuffers.list, sb);
	gf_mse_fire_event(ms->activeSourceBuffers.evt_target, GF_EVENT_HTML_MSE_ADD_SOURCE_BUFFER);
} */

void gf_mse_remove_active_source_buffer(GF_HTML_MediaSource *ms, GF_HTML_SourceBuffer *sb) {
	s32 activePos;
	activePos = gf_list_find(ms->activeSourceBuffers.list, sb);
	if (activePos >= 0) {
		gf_list_rem(ms->activeSourceBuffers.list, activePos);
		gf_mse_fire_event(ms->activeSourceBuffers.evt_target, GF_EVENT_HTML_MSE_REMOVE_SOURCE_BUFFER);
	}
}

static void gf_mse_reset_input_buffer(GF_List *input_buffer)
{
	while (gf_list_count(input_buffer)) {
		GF_HTML_ArrayBuffer *b = (GF_HTML_ArrayBuffer *)gf_list_get(input_buffer, 0);
		gf_list_rem(input_buffer, 0);
		gf_arraybuffer_del(b, GF_FALSE);
	}
}

/* Deletes all unparsed data buffers from all tracks in the source buffer */
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
	sb->group_end_timestamp_set = GF_FALSE;
	gf_mse_reset_input_buffer(sb->input_buffer);
	sb->append_state = MEDIA_SOURCE_APPEND_STATE_WAITING_FOR_SEGMENT;
}

GF_Err gf_mse_source_buffer_abort(GF_HTML_SourceBuffer *sb)
{
	if (sb->updating) {
		/* setting to false should stop the parsing thread */
		sb->updating = GF_FALSE;
		gf_mse_fire_event(sb->evt_target, GF_EVENT_HTML_MSE_UPDATE_ABORT);
		gf_mse_fire_event(sb->evt_target, GF_EVENT_HTML_MSE_UPDATE_END);
	}
	gf_mse_source_buffer_reset_parser(sb);
	sb->appendWindowStart = 0;
	sb->appendWindowEnd = GF_MAX_DOUBLE;
	return GF_OK;
}

GF_Err gf_mse_remove_source_buffer(GF_HTML_MediaSource *ms, GF_HTML_SourceBuffer *sb) {
	s32 pos;
	pos = gf_list_find(ms->sourceBuffers.list, sb);
	if (pos < 0) {
		return GF_NOT_FOUND;
	} else {
		gf_mse_source_buffer_abort(sb);
		/* TODO: update the audio/video/text tracks */
		gf_mse_remove_active_source_buffer(ms, sb);
		gf_list_rem(ms->sourceBuffers.list, pos);
		gf_mse_fire_event(ms->sourceBuffers.evt_target, GF_EVENT_HTML_MSE_REMOVE_SOURCE_BUFFER);
		gf_mse_source_buffer_del(sb);
	}
	return GF_OK;
}

/* TODO: not yet used
void gf_mse_detach(GF_HTML_MediaSource *ms) {
	u32 count;
	u32 i;
	GF_HTML_SourceBuffer *sb;
	ms->readyState = MEDIA_SOURCE_READYSTATE_CLOSED;
	ms->durationType = DURATION_NAN;
	count = gf_list_count(ms->sourceBuffers.list);
	for (i = 0; i < count; i++) {
		sb = (GF_HTML_SourceBuffer *)gf_list_get(ms->sourceBuffers.list, i);
		gf_mse_remove_source_buffer(ms, sb);
	}
} */

void gf_mse_source_buffer_del(GF_HTML_SourceBuffer *sb)
{
	GF_HTML_TrackList tlist;
	gf_html_timeranges_del(sb->buffered);
	gf_mse_reset_input_buffer(sb->input_buffer);
	gf_list_del(sb->input_buffer);

	if (sb->prev_buffer) gf_arraybuffer_del((GF_HTML_ArrayBuffer *)sb->prev_buffer, GF_FALSE);

	tlist.tracks = sb->tracks;
	gf_html_tracklist_del(&tlist);
	{
		u32 i, count;
		count = gf_list_count(sb->threads);
		for(i = 0; i < count; i++) {
			GF_Thread *t = (GF_Thread *)gf_list_get(sb->threads, i);
			gf_th_del(t);
		}
		gf_list_del(sb->threads);
	}
	gf_th_del(sb->parser_thread);
	gf_th_del(sb->remove_thread);

	if (sb->service_desc) gf_odf_desc_del((GF_Descriptor *)sb->service_desc);
	gf_free(sb);

}

void gf_mse_source_buffer_set_update(GF_HTML_SourceBuffer *sb, Bool update)
{
	if (sb->updating == update) return;
	sb->updating = update;
	if (update) {
		gf_mse_fire_event(sb->evt_target, GF_EVENT_HTML_MSE_UPDATE_START);
	} else {
		gf_mse_fire_event(sb->evt_target, GF_EVENT_HTML_MSE_UPDATE);
		gf_mse_fire_event(sb->evt_target, GF_EVENT_HTML_MSE_UPDATE_END);
	}
}

/*locates and loads an input service (demuxer, parser) for this source buffer based on mime type or segment name*/
GF_Err gf_mse_source_buffer_load_parser(GF_HTML_SourceBuffer *sourcebuffer, const char *mime)
{
	GF_InputService *parser = NULL;

	const char *sPlug;
	if (mime) {
		/* strip the 'codecs' and 'profile' parameters from the MIME type */
		char *param = (char *)strchr(mime, ';');
		if (param) {
			*param = 0;
		}

		/* Check MIME type to start the right InputService */
		sPlug = gf_cfg_get_key(sourcebuffer->mediasource->service->term->user->config, "MimeTypes", mime);
		if (sPlug) sPlug = strrchr(sPlug, '"');
		if (sPlug) {
			sPlug += 2;
			parser = (GF_InputService *) gf_modules_load_interface_by_name(sourcebuffer->mediasource->service->term->user->modules, sPlug, GF_NET_CLIENT_INTERFACE);
		}

		if (param) {
			*param = ';';
		}
	}
	if (parser) {
		sourcebuffer->parser = parser;
		parser->query_proxy = gf_mse_proxy;
		parser->proxy_udta = sourcebuffer;
		parser->proxy_type = GF_TRUE;
		return GF_OK;
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[MSE] Error locating plugin for source - mime type %s\n", mime));
		return GF_BAD_PARAM;
	}
}

/* create a track based on the ESD and adds it to the source buffer */
static GF_HTML_Track *gf_mse_source_buffer_add_track(GF_HTML_SourceBuffer *sb, GF_ESD *esd)
{
	GF_HTML_Track *track;
	GF_HTML_TrackType type;

	track = NULL;
	type = HTML_MEDIA_TRACK_TYPE_UNKNOWN;
	if (esd->decoderConfig->streamType==GF_STREAM_VISUAL) {
		type = HTML_MEDIA_TRACK_TYPE_VIDEO;
	} else if (esd->decoderConfig->streamType==GF_STREAM_AUDIO) {
		type = HTML_MEDIA_TRACK_TYPE_AUDIO;
	} else {
		/* TODO: Text tracks */
	}

	track = gf_html_media_track_new(type, "", GF_TRUE, "", "", "", "");
	if (track) {
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

/* Creates the different tracks based on the demuxer parser information
   Needs the parser to be setup and the initialization segment passed */
static GF_Err gf_mse_source_buffer_setup_tracks(GF_HTML_SourceBuffer *sb)
{
	if (!sb || !sb->parser || !sb->parser_connected) return GF_BAD_PARAM;
	sb->service_desc = (GF_ObjectDescriptor *)sb->parser->GetServiceDescriptor(sb->parser, GF_MEDIA_OBJECT_UNDEF, NULL);
	if (sb->service_desc) {
		u32 count;
		count = gf_list_count(sb->service_desc->ESDescriptors);
		if (count) {
			u32 i;
			for (i = 0; i < count; i++) {
				GF_ESD *esd = (GF_ESD *)gf_list_get(sb->service_desc->ESDescriptors, i);
				gf_mse_source_buffer_add_track(sb, esd);
			}
		}
		return GF_OK;
	} else {
		return GF_BAD_PARAM;
	}
}

/* Adds the ObjectDescriptor to the associated track (creating a new track if needed)
   Called in response to addmedia events received from the parser */
static GF_Err gf_mse_source_buffer_store_track_desc(GF_HTML_SourceBuffer *sb, GF_ObjectDescriptor *od)
{
	u32 i;
	u32 count;
	GF_ESD *esd;
	Bool found = GF_FALSE;
	GF_HTML_Track *track;

	esd = (GF_ESD *)gf_list_get(od->ESDescriptors, 0);
	count = gf_list_count(sb->tracks);
	for (i = 0; i < count; i++) {
		track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
		if (track->bin_id == esd->ESID) {
			track->od = od;
			found = GF_TRUE;
			break;
		}
	}
	if (!found) {
		track = gf_mse_source_buffer_add_track(sb, esd);
		if (track) {
			track->od = od;
		} else {
			/* TODO: error */
			return GF_BAD_PARAM;
		}
	}
	return GF_OK;
}

#define SECONDS_TO_TIMESCALE(s) ((s)*track->timescale)
#define TIMESCALE_TO_SECONDS(u) ((u)*1.0/track->timescale)


GF_HTML_MediaTimeRanges *gf_mse_timeranges_from_track_packets(GF_HTML_Track *track) {
	u32 i, count;
	GF_HTML_MediaTimeRanges *ranges;
	u64 start;
	u64 end=0;
	Bool end_set = GF_FALSE;
	GF_MSE_Packet *packet;

	ranges = gf_html_timeranges_new(track->timescale);
	count = gf_list_count(track->buffer);
	for (i = 0; i < count; i++) {
		packet = (GF_MSE_Packet *)gf_list_get(track->buffer, i);
		if (end_set == GF_FALSE|| packet->sl_header.compositionTimeStamp > end) {
			if (end_set == GF_TRUE) {
				gf_html_timeranges_add_end(ranges, end);
			}
			start = packet->sl_header.compositionTimeStamp;
			gf_html_timeranges_add_start(ranges, start);
			end = packet->sl_header.compositionTimeStamp + packet->sl_header.au_duration;
			end_set = GF_TRUE;
		} else if (packet->sl_header.compositionTimeStamp == end) {
			end = packet->sl_header.compositionTimeStamp + packet->sl_header.au_duration;
		}
	}
	if (end_set == GF_TRUE) {
		gf_html_timeranges_add_end(ranges, end);
	}
	return ranges;
}

/* Traverses the list of Access Units already demuxed & parsed to update the buffered status */
void gf_mse_source_buffer_update_buffered(GF_HTML_SourceBuffer *sb) {
	u32 i;
	u32 track_count;

	track_count = gf_list_count(sb->tracks);
	gf_html_timeranges_reset(sb->buffered);
	for (i = 0; i < track_count; i++) {
		GF_HTML_MediaTimeRanges *track_ranges;
		GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
		gf_mx_p(track->buffer_mutex);
		track_ranges = gf_mse_timeranges_from_track_packets(track);
		if (i != 0) {
			GF_HTML_MediaTimeRanges *tmp;
			tmp = gf_html_timeranges_intersection(sb->buffered, track_ranges);
			gf_html_timeranges_del(track_ranges);
			gf_list_del(sb->buffered->times);
			sb->buffered->times = tmp->times;
			sb->buffered->timescale = tmp->timescale;
			gf_free(tmp);
		} else {
			gf_list_del(sb->buffered->times);
			sb->buffered->times = track_ranges->times;
			sb->buffered->timescale = track_ranges->timescale;
			gf_free(track_ranges);
		}
		gf_mx_v(track->buffer_mutex);
	}
}

void gf_mse_source_buffer_set_timestampOffset(GF_HTML_SourceBuffer *sb, double d) {
	u32 i;
	sb->timestampOffset = (s64)(d*sb->timescale);
	if (sb->append_mode == MEDIA_SOURCE_APPEND_MODE_SEQUENCE) {
		sb->group_start_timestamp_flag = GF_TRUE;
		sb->group_start_timestamp = sb->timestampOffset;
	}
	for (i = 0; i < gf_list_count(sb->tracks); i++) {
		GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
		track->timestampOffset = sb->timestampOffset*track->timescale;
	}
}

void gf_mse_source_buffer_set_timescale(GF_HTML_SourceBuffer *sb, u32 new_timescale) {
	u32 old_timescale = sb->timescale;
	if (old_timescale == new_timescale) return;
	sb->timescale = new_timescale;
	sb->timestampOffset = (s64)((sb->timestampOffset * new_timescale * 1.0)/old_timescale);
	if (sb->group_start_timestamp_flag) {
		sb->group_start_timestamp = (u64)((sb->group_start_timestamp * new_timescale * 1.0)/old_timescale);
	}
	if (sb->group_end_timestamp_set) {
		sb->group_end_timestamp = (u64)((sb->group_end_timestamp * new_timescale * 1.0)/old_timescale);
	}
	sb->remove_start = (u64)((sb->remove_start * new_timescale * 1.0)/old_timescale);
	sb->remove_end = (u64)((sb->remove_end * new_timescale * 1.0)/old_timescale);

}

void gf_mse_packet_del(GF_MSE_Packet *packet) {
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
			gf_mx_v(track->buffer_mutex);
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

	gf_mx_p(track->buffer_mutex);
	i = 0;
	while (i < gf_list_count(track->buffer)) {
		GF_MSE_Packet *frame = (GF_MSE_Packet *)gf_list_get(track->buffer, i);
		if (frame->sl_header.compositionTimeStamp >= to) {
			break;
		} else if (frame->sl_header.compositionTimeStamp >= from && frame->sl_header.compositionTimeStamp < to) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Removing frame with PTS %g s (%d frames remaining)\n", TIMESCALE_TO_SECONDS(frame->sl_header.compositionTimeStamp), gf_list_count(track->buffer)));
			gf_list_rem(track->buffer, i);
		} else {
			i++;
		}
	}
	gf_mx_v(track->buffer_mutex);
}

static void gf_mse_track_buffer_add_packet(GF_HTML_Track *track, GF_MSE_Packet *frame)
{
	u32 i, count;
	Bool inserted = GF_FALSE;

	gf_mx_p(track->buffer_mutex);
	/* TODO: improve insertion*/
	count = gf_list_count(track->buffer);
	for (i = 0; i < count; i++) {
		GF_MSE_Packet *next_frame =  (GF_MSE_Packet *)gf_list_get(track->buffer, i);
		if (frame->sl_header.decodingTimeStamp < next_frame->sl_header.decodingTimeStamp) {
			gf_list_insert(track->buffer, frame, i);
			/* if the frame had no duration, we can now tell its duration because of the next frame */
			if (!frame->sl_header.au_duration) {
				frame->sl_header.au_duration = (u32)(next_frame->sl_header.decodingTimeStamp - frame->sl_header.decodingTimeStamp);
				/* we need also to check the duration of the previous frame */
				if (i > 0) {
					GF_MSE_Packet *prev_frame =  (GF_MSE_Packet *)gf_list_get(track->buffer, i-1);
					/* we update the frame duration if the newly inserted frame modifies it */
					if (!prev_frame->sl_header.au_duration ||
					        prev_frame->sl_header.au_duration > frame->sl_header.decodingTimeStamp - prev_frame->sl_header.decodingTimeStamp) {
						prev_frame->sl_header.au_duration = (u32)(frame->sl_header.decodingTimeStamp - prev_frame->sl_header.decodingTimeStamp);
					}
				}
			}
			inserted = GF_TRUE;
			break;
		}
	}
	if (!inserted) {
		gf_list_add(track->buffer, frame);
		/* if the frame is inserted last, we cannot know its duration until a new frame is appended or unless the transport format carried it */
		count = gf_list_count(track->buffer);
		if (count > 1) {
			GF_MSE_Packet *prev_frame =  (GF_MSE_Packet *)gf_list_get(track->buffer, count-2);
			/* we update the frame duration if the newly inserted frame modifies it */
			if (!prev_frame->sl_header.au_duration ||
			        prev_frame->sl_header.au_duration > frame->sl_header.decodingTimeStamp - prev_frame->sl_header.decodingTimeStamp) {
				prev_frame->sl_header.au_duration = (u32)(frame->sl_header.decodingTimeStamp - prev_frame->sl_header.decodingTimeStamp);
			}
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Adding frame with PTS %g s and duration %g s (%d frames in buffer)\n", TIMESCALE_TO_SECONDS(frame->sl_header.compositionTimeStamp), TIMESCALE_TO_SECONDS(frame->sl_header.au_duration), gf_list_count(track->buffer)));
	gf_mx_v(track->buffer_mutex);
}

static GF_Err gf_mse_process_coded_frame(GF_HTML_SourceBuffer    *sb,
        GF_HTML_Track           *track,
        GF_MSE_Packet           *frame,
        Bool *stored)
{
	s64 PTS_with_offset = frame->sl_header.compositionTimeStamp + frame->sl_header.timeStampOffset;
	s64 DTS_with_offset = frame->sl_header.decodingTimeStamp + frame->sl_header.timeStampOffset;
	*stored = GF_FALSE;

	if (sb->append_mode == MEDIA_SOURCE_APPEND_MODE_SEQUENCE && sb->group_start_timestamp_flag) {
		u32 i, count;
		/* compute the new offset without taking care of the previous one, since this is a new coded frame group */
		/* first adjust existing times to the new timescale */
		gf_mse_source_buffer_set_timescale(sb, track->timescale);
		sb->timestampOffset = (sb->group_start_timestamp - frame->sl_header.compositionTimeStamp);
		track->timestampOffset = (sb->group_start_timestamp - frame->sl_header.compositionTimeStamp);
		sb->group_end_timestamp = sb->group_start_timestamp;
		count = gf_list_count(sb->tracks);
		for (i = 0; i < count; i++) {
			GF_HTML_Track *t = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
			t->needs_rap = GF_TRUE;
		}
		sb->group_start_timestamp_flag = GF_FALSE;
	}

	if (track->timestampOffset != 0) {
		frame->sl_header.timeStampOffset = track->timestampOffset;
		PTS_with_offset = frame->sl_header.compositionTimeStamp + frame->sl_header.timeStampOffset;
		DTS_with_offset = frame->sl_header.decodingTimeStamp + frame->sl_header.timeStampOffset;
	}

	if (track->last_dts_set) {
		if (DTS_with_offset < (s64) track->last_dts ||
		        DTS_with_offset - track->last_dts > 2*track->last_dur) {
			/* A discontinuity in the timestamps is detected, this triggers the start of a new coded frame group */
			if (sb->append_mode == MEDIA_SOURCE_APPEND_MODE_SEGMENTS) {
				/* the current group ends at the start of this frame */
				/* check if sb.timescale has to be adjusted first with gf_mse_source_buffer_set_timescale(sb, track->timescale);*/
				sb->group_end_timestamp = PTS_with_offset;
				sb->group_end_timestamp_set = GF_TRUE;
			} else { /* sb->append_mode == MEDIA_SOURCE_APPEND_MODE_SEQUENCE */
				/* check if sb.timescale has to be adjusted first with gf_mse_source_buffer_set_timescale(sb, track->timescale);*/
				sb->group_start_timestamp = sb->group_end_timestamp;
				sb->group_start_timestamp_flag = GF_TRUE;
			}
			{
				u32 i, count;
				count = gf_list_count(sb->tracks);
				for (i = 0; i < count; i++) {
					GF_HTML_Track *t = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
					t->last_dts_set = GF_FALSE;
					t->last_dts = 0;
					t->last_dur = 0;
					t->highest_pts_set = GF_FALSE;
					t->highest_pts = 0;
					t->needs_rap = GF_TRUE;
				}
			}
			return gf_mse_process_coded_frame(sb, track, frame, stored);
		}
	}

	/* we only update the timestamps in the frame when we are sure the offset is the right one */
	frame->sl_header.compositionTimeStamp += frame->sl_header.timeStampOffset;
	frame->sl_header.decodingTimeStamp += frame->sl_header.timeStampOffset;
	frame->sl_header.timeStampOffset = 0;

	if (frame->sl_header.compositionTimeStamp < SECONDS_TO_TIMESCALE(sb->appendWindowStart)) {
		track->needs_rap = GF_TRUE;
		return GF_OK;
	}

	if (frame->sl_header.compositionTimeStamp + frame->sl_header.au_duration > SECONDS_TO_TIMESCALE(sb->appendWindowEnd)) {
		track->needs_rap = GF_TRUE;
		return GF_OK;
	}

	if (track->needs_rap) {
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
			gf_mse_remove_frames_from_to(track, overlapped_packet->sl_header.compositionTimeStamp,
			                             overlapped_packet->sl_header.compositionTimeStamp + (u64)SECONDS_TO_TIMESCALE(0.000001));
		}
	}

	if (!track->highest_pts_set) {
		/* this is the first time a frame is processed in the append sequence */
		gf_mse_remove_frames_from_to(track, frame->sl_header.compositionTimeStamp, frame->sl_header.compositionTimeStamp + frame->sl_header.au_duration);
	} else if (track->highest_pts <= frame->sl_header.compositionTimeStamp) {
		/* the highest pts has already been set in this append sequence, so we just need to remove frames from that point on, it's safe */
		gf_mse_remove_frames_from_to(track, track->highest_pts, track->highest_pts + track->last_dur);
	}

	/* remove dependencies !! */

	/* TODO: spliced frames */

	*stored = GF_TRUE;
	/* adds the packet and update the previous frame duration */
	gf_mse_track_buffer_add_packet(track, frame);

	track->last_dts = frame->sl_header.decodingTimeStamp;
	track->last_dts_set = GF_TRUE;
	if (frame->sl_header.au_duration) {
		track->last_dur = frame->sl_header.au_duration;
	} else {
		/* assuming CFR - FIXME */
		frame->sl_header.au_duration = track->last_dur;
	}

	if (!track->highest_pts_set ||
	        (frame->sl_header.compositionTimeStamp + track->last_dur) > track->highest_pts) {
		track->highest_pts_set = GF_TRUE;
		track->highest_pts = frame->sl_header.compositionTimeStamp + frame->sl_header.au_duration;
	}

	if (!sb->group_end_timestamp_set || (frame->sl_header.compositionTimeStamp + frame->sl_header.au_duration > sb->group_end_timestamp)) {
		/* check if sb.timescale has to be adjusted first with gf_mse_source_buffer_set_timescale(sb, track->timescale);*/
		sb->group_end_timestamp = frame->sl_header.compositionTimeStamp + frame->sl_header.au_duration;
		sb->group_end_timestamp_set = GF_TRUE;
	}

	return GF_OK;
}

/* Thread run function: called as a result of an append buffer
 * Parses/Demultiplexes media segments and places the parsed AU in the track buffers
 *
 * \param par the GF_HTML_SourceBuffer object used for the append
 */
u32 gf_mse_parse_segment(void *par)
{
	GF_MSE_Packet           *packet;
	GF_HTML_Track           *track;
	GF_HTML_SourceBuffer    *sb = (GF_HTML_SourceBuffer *)par;
	u32                     i;
	u32                     track_count;

	if (!sb->parser_connected) {
		GF_HTML_ArrayBuffer *buffer = (GF_HTML_ArrayBuffer *)gf_list_get(sb->input_buffer, 0);
		gf_list_rem(sb->input_buffer, 0);
		assert(buffer);
		/* we expect an initialization segment to connect the service */
		if (!buffer->is_init) {
			/* TODO: set the media element decode error and run the end of stream algo */
			goto exit;
		}
		sb->parser->ConnectService(sb->parser, sb->mediasource->service, buffer->url);
		sb->prev_buffer = buffer;
	} else {
		/* we expect a media segment */
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Starting to fetch AUs from parser\n"));

		/* For each track, ask the parser for all the Access Unit, until it is empty
		 * AU are placed as GF_MSE_Packets in the track buffer
		 */
		track_count = gf_list_count(sb->tracks);
		while (sb->updating) {
			u32 track_with_data = 0;
			for (i = 0; i < track_count; i++) {
				Bool stored = GF_FALSE;
				GF_Err e;
				track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
				GF_SAFEALLOC(packet, GF_MSE_Packet);
				assert(track->channel);
				e = sb->parser->ChannelGetSLP(sb->parser, track->channel,
				                              &packet->data, &packet->size, &packet->sl_header,
				                              &packet->is_compressed, &packet->status, &packet->is_new_data);
				assert(e == GF_OK);
				if (packet->status == GF_OK && packet->is_new_data && packet->size) {
					char *data;
					assert(packet->is_new_data && packet->size);
					data = (char *)gf_malloc(packet->size);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] New AU parsed with PTS %g s\n", TIMESCALE_TO_SECONDS(packet->sl_header.compositionTimeStamp)));
					memcpy(data, packet->data, packet->size);
					packet->data = data;
					gf_mse_process_coded_frame(sb, track, packet, &stored);
					track_with_data++;
					sb->parser->ChannelReleaseSLP(sb->parser, track->channel);
				}

				if (!stored) gf_free(packet);
			}
			if (!track_with_data) {
				/* try to delete the previous buffer */
				gf_arraybuffer_del((GF_HTML_ArrayBuffer *)sb->prev_buffer, GF_FALSE);
				sb->prev_buffer = NULL;
				/* get ready to receive a new segment and start a new thread */
				break;
			}
		}
		/* reaching here, the parser does not have enough data to produce new AU */
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Done fetching AUs from parser\n"));
	}
exit:
	gf_mse_source_buffer_set_update(sb, GF_FALSE);
	return 0;
}


void gf_mse_source_buffer_append_arraybuffer(GF_HTML_SourceBuffer *sb, GF_HTML_ArrayBuffer *buffer)
{
	assert(sb->parser);
	gf_mse_source_buffer_set_update(sb, GF_TRUE);

	buffer->url = (char *)gf_malloc(256);
	sprintf(buffer->url, "gmem://%d@%p", buffer->length, buffer->data);
	buffer->reference_count++;
	buffer->is_init = (gf_isom_probe_file(buffer->url) == 2 ? GF_TRUE : GF_FALSE);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Appending segment %s to SourceBuffer %p\n", buffer->url, sb));

	gf_list_add(sb->input_buffer, buffer);
	/* Call the parser (asynchronously) and return */
	/* the updating attribute will be positioned back to 0 when the parser is done */
	{
		GF_Thread *t = gf_th_new(NULL);
		gf_list_add(sb->threads, t);
		gf_th_run(t, gf_mse_parse_segment, sb);
	}
}

/* Threaded function called upon request from JS
   - Removes data in each track buffer until the next RAP is found */
static u32 gf_mse_source_buffer_remove(void *par)
{
	GF_HTML_SourceBuffer    *sb = (GF_HTML_SourceBuffer *)par;
	u32                     i;
	u32                     j;
	u32                     track_count;
	u32                     frame_count;
	u64                     end = 0;

	gf_mse_source_buffer_set_update(sb, GF_TRUE);

	GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Removing media until next RAP\n"));

	track_count = gf_list_count(sb->tracks);
	for (i = 0; i < track_count; i++) {
		GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(sb->tracks, i);
		gf_mse_source_buffer_set_timescale(sb, track->timescale);
		/* find the next random access point */
		gf_mx_p(track->buffer_mutex);
		frame_count = gf_list_count(track->buffer);
		for (j = 0; j < frame_count; j++) {
			GF_MSE_Packet *frame = (GF_MSE_Packet *)gf_list_get(track->buffer, j);
			if (frame->sl_header.randomAccessPointFlag &&
			        frame->sl_header.compositionTimeStamp >= sb->remove_end) {
				end = frame->sl_header.compositionTimeStamp;
				break;
			}
		}
		gf_mx_v(track->buffer_mutex);
		if (!end) end = (u64)SECONDS_TO_TIMESCALE(sb->remove_end);

		/* remove up to the next RAP found */
		gf_mse_remove_frames_from_to(track, sb->remove_start, end);
	}
	gf_mse_source_buffer_set_update(sb, GF_FALSE);
	return 0;
}

void gf_mse_remove(GF_HTML_SourceBuffer *sb, double start, double end)
{
	sb->remove_start = (u64)(start*sb->timescale);
	sb->remove_end = (u64)(end*sb->timescale);
	{
		GF_Thread *t = gf_th_new(NULL);
		gf_list_add(sb->threads, t);
		gf_th_run(t, gf_mse_source_buffer_remove, sb);
	}
}

/* Callback functions used by a media parser when parsing events happens */
GF_Err gf_mse_proxy(GF_InputService *parser, GF_NetworkCommand *command)
{
	if (!parser || !command || !parser->proxy_udta) {
		return GF_BAD_PARAM;
	} else {
		GF_HTML_SourceBuffer *sb = (GF_HTML_SourceBuffer *)parser->proxy_udta;
		switch (command->command_type) {
		case GF_NET_SERVICE_QUERY_INIT_RANGE:
			break;
		case GF_NET_SERVICE_QUERY_NEXT:
			/* The parser is asking for the next media segment in the buffer,
			   check for the media time and give the right one */
		{
			GF_HTML_ArrayBuffer *buffer;
			/* The input buffer should not be modified by append operations at the same time, no need to protect access */
			buffer = (GF_HTML_ArrayBuffer *)gf_list_get(sb->input_buffer, 0);
			if (buffer) {
				command->url_query.discontinuity_type = 0;
				command->url_query.current_download = GF_FALSE;
				command->url_query.start_range = 0;
				command->url_query.end_range = 0;
				command->url_query.switch_start_range = 0;
				command->url_query.switch_end_range = 0;
				command->url_query.next_url_init_or_switch_segment = NULL;
				if (buffer->is_init) {
					GF_HTML_ArrayBuffer *next = (GF_HTML_ArrayBuffer *)gf_list_get(sb->input_buffer, 1);
					command->url_query.discontinuity_type = 1;
					if (next) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Next segment to parse %s with init \n", next->url, buffer->url));
						command->url_query.next_url = next->url;
						command->url_query.next_url_init_or_switch_segment = buffer->url;
						gf_list_rem(sb->input_buffer, 0);
						gf_list_rem(sb->input_buffer, 0);
					} else {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Only one init segment to parse %s, need to wait\n", buffer->url));
						command->url_query.next_url = NULL;
					}
				} else {
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE] Next segment to parse %s\n", buffer->url));
					command->url_query.next_url = buffer->url;
					gf_list_rem(sb->input_buffer, 0);
				}
				sb->prev_buffer = buffer;
			} else {
				command->url_query.next_url = NULL;
				command->url_query.discontinuity_type = 0;
			}
		}
		break;
		case GF_NET_SERVICE_STATUS_PROXY:
			/* The parser is informing the proxy about its status changes:
			    - new track found
				- all tracks parsed
				- connect/disconnect
				- */
			if (command->status.is_add_media) {
				if (command->status.desc) {
					gf_mse_source_buffer_store_track_desc(sb, (GF_ObjectDescriptor *)command->status.desc);
				} else {
					/* this is the last add media, we can switch updating to false */
					/* the first init segment was correctly processed */
					gf_mse_source_buffer_set_update(sb, GF_FALSE);
					/* TODO: set active tracks and send addsourcebuffer event */
					/* TODO: send media loadedmetadata event */
				}
				gf_service_declare_media(sb->mediasource->service, command->status.desc, (command->status.desc ? GF_TRUE : GF_FALSE));
			}
			/* general connection/disconnection messages from the media parser (not track related) */
			else if (!command->status.channel) {
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
					gf_service_connect_ack(sb->mediasource->service, command->status.channel, command->status.e);
				} else {
					gf_service_disconnect_ack(sb->mediasource->service, command->status.channel, command->status.e);
				}
			}
			/* channel (track related) specific connection/disconnection messages from the media parser */
			else {
				if (!command->status.is_disconnect) {
					gf_service_connect_ack(sb->mediasource->service, command->status.channel, command->status.e);
				} else {
					gf_service_disconnect_ack(sb->mediasource->service, command->status.channel, command->status.e);
				}
			}
			break;
		default:
			gf_service_command(sb->mediasource->service, command, GF_OK);
			break;
		}
		return GF_OK;
	}
}

/* Track Buffer Managment:
 * - Access Units are parsed/demultiplexed from the SourceBuffer input buffer and placed into individual track buffers
 * - The parsing/demux is done in a separate thread (so access to the the track buffer is protected by a mutex)
 *
 * Track Buffer Length: number of Access Units */
#define MSE_TRACK_BUFFER_LENGTH 1000

/* When an Access Unit can be released, we check if it needs to be kept or not in the track buffer */
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

/* Requests from the decoders to get the next Access Unit: we get it from the track buffer */
GF_EXPORT
GF_Err gf_mse_track_buffer_get_next_packet(GF_HTML_Track *track,
        char **out_data_ptr, u32 *out_data_size,
        GF_SLHeader *out_sl_hdr, Bool *sl_compressed,
        GF_Err *out_reception_status, Bool *is_new_data)
{
	GF_MSE_Packet *packet;
	u32 count;
	gf_mx_p(track->buffer_mutex);
	count = gf_list_count(track->buffer);
	packet = (GF_MSE_Packet *)gf_list_get(track->buffer, track->packet_index);
	if (packet) {
		*out_data_ptr = packet->data;
		*out_data_size = packet->size;
		*out_sl_hdr = packet->sl_header;
		*sl_compressed = packet->is_compressed;
		*out_reception_status = packet->status;
		*is_new_data = packet->is_new_data;
		packet->is_new_data = GF_FALSE;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[MSE_IN] Sending AU #%d/%d to decoder with PTS %g s\n", track->packet_index, count, TIMESCALE_TO_SECONDS(packet->sl_header.compositionTimeStamp)));
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
