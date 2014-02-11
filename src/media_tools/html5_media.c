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

#include <gpac/html5_media.h>
#include <gpac/html5_mse.h>


GF_Err gf_media_time_ranges_add(GF_HTML_MediaTimeRanges *timeranges, double start, double end)
{
    double *d;
    if (!timeranges) return GF_BAD_PARAM;
    d = (double *)gf_malloc(sizeof(double));
    *d = start;
    gf_list_add(timeranges->times, d);
    d = (double *)gf_malloc(sizeof(double));
    *d = end;
    gf_list_add(timeranges->times, d);
    return GF_OK;
}

void gf_html_timeranges_reset(GF_HTML_MediaTimeRanges *range)
{
    while (gf_list_count(range->times))
    {
        double *d = (double *)gf_list_get(range->times, 0);
        gf_free(d);
        gf_list_rem(range->times, 0);
    }
}

void gf_html_timeranges_del(GF_HTML_MediaTimeRanges *range)
{
	gf_html_timeranges_reset(range);
    gf_list_del(range->times);
	range->times = NULL;
}

GF_HTML_Track *html_media_add_new_track_to_list(GF_HTML_TrackList *tracklist,
                                                       GF_HTML_TrackType type, const char *mime, Bool enable_or_selected,
                                                       const char *id, const char *kind, const char *label, const char *lang)
{
    GF_HTML_Track *track;
    track = gf_html_media_track_new(type, mime, enable_or_selected, id, kind, label, lang);
    gf_list_add(tracklist->tracks, track);
    return track;
}

Bool html_media_tracklist_has_track(GF_HTML_TrackList *tracklist, const char *id)
{
    GF_HTML_Track *track;
    u32 i, count;
    count = gf_list_count(tracklist->tracks);
    for (i = 0; i < count; i++) 
    {
        track = (GF_HTML_Track *)gf_list_get(tracklist->tracks, i);
        if (!strcmp(id, track->id)) 
        {
            return 1;
        }
    }
    return 0;
}

GF_HTML_Track *html_media_tracklist_get_track(GF_HTML_TrackList *tracklist, const char *id)
{
    GF_HTML_Track *track = NULL;
    u32 i, count;
    count = gf_list_count(tracklist->tracks);
    for (i = 0; i < count; i++) 
    {
        track = (GF_HTML_Track *)gf_list_get(tracklist->tracks, i);
        if (!strcmp(id, track->id)) 
        {
            return track;
        }
    }
    return NULL;
}

void gf_html_tracklist_del(GF_HTML_TrackList *tlist)
{
    while (gf_list_count(tlist->tracks))
    {
        GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(tlist->tracks, 0);
        gf_html_track_del(track);
        gf_list_rem(tlist->tracks, 0);
    }
    gf_list_del(tlist->tracks);
}

GF_HTML_Track *gf_html_media_track_new(GF_HTML_TrackType type, const char *mime, Bool enable_or_selected,
                                       const char *id, const char *kind, const char *label, const char *lang)
{
    GF_HTML_Track *track;
    GF_SAFEALLOC(track, GF_HTML_Track);
    track->type     = type;
    track->mime     = gf_strdup(mime);
    track->id       = gf_strdup(id);
    track->kind     = gf_strdup(kind);
    track->label    = gf_strdup(label);
    track->language = gf_strdup(lang);
    track->enabled_or_selected = enable_or_selected;
    /* TODO: empty MSE data */
    return track;
}

void gf_html_track_del(GF_HTML_Track *track)
{
    if (track->id)          gf_free(track->id);
    if (track->kind)        gf_free(track->kind);
    if (track->label)       gf_free(track->label);
    if (track->language)    gf_free(track->language);
    if (track->mime)        gf_free(track->mime);

    if (track->buffer_mutex) {
		gf_mx_p(track->buffer_mutex);
		while (gf_list_count(track->buffer))
		{
			GF_MSE_Packet *packet = (GF_MSE_Packet *)gf_list_get(track->buffer, 0);
			gf_mse_packet_del(packet);
			gf_list_rem(track->buffer, 0);
		}
		gf_mx_v(track->buffer_mutex);
		gf_list_del(track->buffer);
		gf_mx_del(track->buffer_mutex);
	} else {
		assert(track->buffer == NULL);
	}
	gf_free(track);
}

GF_HTML_MediaElement *gf_html_media_element_new(GF_Node *media_node, GF_HTML_MediaController *mc)
{
    GF_HTML_MediaElement *me;
    GF_SAFEALLOC(me, GF_HTML_MediaElement);
    me->node                    = media_node;
    me->controller              = mc; 
    me->audioTracks.tracks      = gf_list_new();
    me->videoTracks.tracks      = gf_list_new();
    me->textTracks.tracks       = gf_list_new();
    me->buffered.times          = gf_list_new();
    me->played.times            = gf_list_new();
    me->seekable.times          = gf_list_new();
    return me;
}

void gf_html_media_element_del(GF_HTML_MediaElement *me)
{
    if (me->startDate) gf_free(me->startDate);
    if (me->currentSrc) gf_free(me->currentSrc);
    gf_html_tracklist_del(&me->audioTracks);
    gf_html_tracklist_del(&me->videoTracks);
    gf_html_tracklist_del(&me->textTracks);
    gf_html_timeranges_del(&me->buffered);
    gf_html_timeranges_del(&me->seekable);
    gf_html_timeranges_del(&me->played);
    gf_free(me);
}

GF_DOMEventTarget *gf_html_media_get_event_target_from_node(GF_Node *n) {
	GF_DOMEventTarget *target = NULL;
	//GF_HTML_MediaElement *me = html_media_element_get_from_node(c, n);
	//*target = me->evt_target;
	if (!n->sgprivate->interact) {
		GF_SAFEALLOC(n->sgprivate->interact, struct _node_interactive_ext);
	}
	if (!n->sgprivate->interact->dom_evt) {
		n->sgprivate->interact->dom_evt = gf_dom_event_target_new(GF_DOM_EVENT_TARGET_HTML_MEDIA, n);
	}
	target = n->sgprivate->interact->dom_evt;
	return target;
}

GF_HTML_MediaController *gf_html_media_controller_new()
{
    GF_HTML_MediaController *mc;
    GF_SAFEALLOC(mc, GF_HTML_MediaController);
    mc->buffered.times          = gf_list_new();
    mc->played.times            = gf_list_new();
    mc->seekable.times          = gf_list_new();
    return mc;
}

void gf_html_mediacontroller_del(GF_HTML_MediaController *mc)
{
    u32 i, count;
    /* removing the pointer to that controller from the media elements using it */
    count = gf_list_count(mc->media_elements);
    for (i = 0; i < count; i++) {
        GF_HTML_MediaElement *me = (GF_HTML_MediaElement *)gf_list_get(mc->media_elements, i);
        me->controller =  NULL;
    }
    gf_html_timeranges_del(&mc->buffered);
    gf_html_timeranges_del(&mc->seekable);
    gf_html_timeranges_del(&mc->played);
    gf_free(mc);
}


#endif
