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
#include <gpac/internal/scenegraph_dev.h>

#ifdef GPAC_HAS_SPIDERMONKEY

#include <gpac/html5_media.h>
#include <gpac/html5_mse.h>

GF_HTML_MediaTimeRanges *gf_html_timeranges_new(u32 timescale)
{
	GF_HTML_MediaTimeRanges *ranges;
	GF_SAFEALLOC(ranges, GF_HTML_MediaTimeRanges);
	if (!ranges) return NULL;
	ranges->times = gf_list_new();
	ranges->timescale = timescale;
	return ranges;
}

static GF_Err gf_html_timeranges_add_time(GF_HTML_MediaTimeRanges *timeranges, u64 time)
{
	u64 *t;
	if (!timeranges) return GF_BAD_PARAM;
	t = (u64 *)gf_malloc(sizeof(u64));
	*t = time;
	gf_list_add(timeranges->times, t);
	return GF_OK;
}

GF_Err gf_html_timeranges_add_start(GF_HTML_MediaTimeRanges *timeranges, u64 start)
{
	return gf_html_timeranges_add_time(timeranges, start);
}

GF_Err gf_html_timeranges_add_end(GF_HTML_MediaTimeRanges *timeranges, u64 end)
{
	return gf_html_timeranges_add_time(timeranges, end);
}

void gf_html_timeranges_reset(GF_HTML_MediaTimeRanges *ranges)
{
	while (gf_list_count(ranges->times))
	{
		u64 *d = (u64 *)gf_list_get(ranges->times, 0);
		gf_free(d);
		gf_list_rem(ranges->times, 0);
	}
}

void gf_html_timeranges_del(GF_HTML_MediaTimeRanges *ranges)
{
	gf_html_timeranges_reset(ranges);
	gf_list_del(ranges->times);
	ranges->times = NULL;
	gf_free(ranges);
}

void gf_html_timeranges_merge(GF_HTML_MediaTimeRanges *ranges) {
	u32 i, count;
	u64 *prev_end = NULL;
	GF_List *merged = gf_list_new();

	count = gf_list_count(ranges->times);
	for (i = 0; i < count; i+=2) {
		u64 *start = (u64 *)gf_list_get(ranges->times, i);
		u64 *end = (u64 *)gf_list_get(ranges->times, i+1);
		if (prev_end == NULL || *start > *prev_end) {
			if (prev_end) {
				gf_list_add(merged, prev_end);
			}
			gf_list_add(merged, start);
		} else if (*start == *prev_end) {
			gf_free(start);
		}
		prev_end = end;
	}
	if (prev_end) {
		gf_list_add(ranges->times, prev_end);
	}
	gf_list_del(ranges->times);
	ranges->times = merged;
}


GF_HTML_MediaTimeRanges *gf_html_timeranges_union(GF_HTML_MediaTimeRanges *a, GF_HTML_MediaTimeRanges *b)
{
	GF_HTML_MediaTimeRanges *union_ranges;
	u32 i, j, count_a, count_b;
	union_ranges = gf_html_timeranges_new(a->timescale);
	union_ranges->c = a->c;
	union_ranges->_this = a->_this;

	count_a = gf_list_count(a->times);
	if (b) {
		count_b = gf_list_count(b->times);
	} else {
		count_b = 0;
	}
	if (count_a == 0 && count_b == 0) {
		return NULL;
	} else if (count_a == 0) {
		GF_HTML_MediaTimeRanges *tmp = a;
		a = b;
		b = tmp;
		count_a = count_b;
		count_b = 0;
	}
	i = 0;
	j = 0;
	while (i < count_a) {
		Bool add_a = GF_TRUE;
		u64 *starta = (u64 *)gf_list_get(a->times, i);
		u64 *enda = (u64 *)gf_list_get(a->times, i+1);
		while (j < count_b) {
			u64 *startb = (u64 *)gf_list_get(b->times, j);
			u64 *endb = (u64 *)gf_list_get(b->times, j+1);
			if (*enda*b->timescale < *startb*a->timescale) {
				/* a ends before b starts, there is no overlap, we can add a to the union */
				gf_list_add(union_ranges->times, starta);
				gf_list_add(union_ranges->times, enda);
				add_a = GF_FALSE;
				/* force to get the next a */
				i+=2;
				break;
			} else if (*endb*a->timescale < *starta*b->timescale) {
				/* b ends before a starts, there is no overlap, we can add b to the union */
				*startb = (u64)((*startb * a->timescale)*1.0 / b->timescale);
				gf_list_add(union_ranges->times, startb);
				*endb = (u64)((*endb * a->timescale)*1.0 / b->timescale);
				gf_list_add(union_ranges->times, endb);
				j+=2;
			} else { /* there is some overlap */
				if (*starta*b->timescale <= *startb*a->timescale) { /* the overlap is at the end of a */
					if (*endb*a->timescale <= *enda*b->timescale) { /* b is contained in a */
						/* ignore b, move on to the next b */
						j+=2;
					} else { /* *endb > *enda, the overlap is only at the start of b */
						/* update start of b */
						*startb = (u64)((*starta * b->timescale)*1.0 / a->timescale);
						/* ignore a, move on to the next a */
						i+=2;
						break;
					}
				} else { /* *starta > *startb, the overlap is at the end of b */
					if (*enda*b->timescale <= *endb*a->timescale) { /* a is contained in b */
						/* ignore a */
						add_a = GF_FALSE;
						/* force to get the next a */
						i+=2;
						break;
					} else { /* *enda > *endb, the overlap is at the beginning of a */
						/* update start of a */
						*starta = (u64)((*startb * a->timescale)*1.0 / b->timescale);
						/* ignore b, move on to the next b */
						j+=2;
					}
				}
			}
		}
		/* we've processed all b, but a has not been added */
		/* first check if the next a is not contiguous */
		if (add_a == GF_TRUE && i+2 < count_a) {
			u64 *next_starta = (u64 *)gf_list_get(a->times, i+2);
			//u64 *next_enda = (u64 *)gf_list_get(a->times, i+3);
			if (*enda == *next_starta) {
				*next_starta = *starta;
			}
			add_a = GF_FALSE;
		}
		if (add_a) {
			gf_list_add(union_ranges->times, starta);
			gf_list_add(union_ranges->times, enda);
		}
		i+=2;
	}
	gf_html_timeranges_merge(union_ranges);
	return union_ranges;
}

GF_HTML_MediaTimeRanges *gf_html_timeranges_intersection(GF_HTML_MediaTimeRanges *a, GF_HTML_MediaTimeRanges *b)
{
	GF_HTML_MediaTimeRanges *intersection_ranges;
	u32 i, j, count_a, count_b;
	if (!a || !b) return NULL;
	intersection_ranges = gf_html_timeranges_new(a->timescale);
	intersection_ranges->c = a->c;
	intersection_ranges->_this = a->_this;
	count_a = gf_list_count(a->times);
	count_b = gf_list_count(b->times);
	if (count_a != 0 && count_b != 0) {
		i = 0;
		j = 0;
		while (i < count_a) {
			u64 *starta = (u64 *)gf_list_get(a->times, i);
			u64 *enda = (u64 *)gf_list_get(a->times, i+1);
			while (j < count_b) {
				u64 *startb = (u64 *)gf_list_get(b->times, j);
				u64 *endb = (u64 *)gf_list_get(b->times, j+1);
				if (*enda*b->timescale < *startb*a->timescale) {
					/* this is no intersection with this a */
					/* force to get the next a */
					i+=2;
					break;
				} else if (*endb*a->timescale < *starta*b->timescale) {
					/* this is no intersection with this b */
					j+=2;
				} else { /* there is an intersection */
					if (*starta*b->timescale <= *startb*a->timescale) { /* the intersection starts at the beginning of b */
						gf_list_add(intersection_ranges->times, startb);
						if (*endb*a->timescale <= *enda*b->timescale) { /* b is contained in a */
							gf_list_add(intersection_ranges->times, endb);
							*starta = (u64)((*endb * a->timescale)*1.0 / b->timescale);
							/* move on to the next b */
							j+=2;
						} else { /* *endb > *enda, the intersection ends at the end of a */
							gf_list_add(intersection_ranges->times, enda);
							/* update start of b */
							*startb = (u64)((*enda * b->timescale)*1.0 / a->timescale);
							/* move on to the next a */
							i+=2;
							break;
						}
					} else { /* *starta > *startb, the intersection starts at the beginning of a */
						gf_list_add(intersection_ranges->times, startb);
						if (*enda*b->timescale <= *endb*a->timescale) { /* a is contained in b */
							gf_list_add(intersection_ranges->times, enda);
							*startb = (u64)((*enda * b->timescale)*1.0 / a->timescale);
							/* move on to the next a */
							i+=2;
							break;
						} else { /* *enda > *endb, the intersection ends at the end of b */
							gf_list_add(intersection_ranges->times, endb);
							/* update start of a */
							*starta = (u64)((*endb * a->timescale)*1.0 / b->timescale);
							/* move on to the next b */
							j+=2;
						}
					}
				}
			}
		}
	}
	return intersection_ranges;
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
	u32 i, count;
	count = gf_list_count(tracklist->tracks);
	for (i = 0; i < count; i++) {
		GF_HTML_Track *track = (GF_HTML_Track *)gf_list_get(tracklist->tracks, i);
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
	if (!track) return NULL;
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
	if (!me) return NULL;
	me->node                    = media_node;
	me->controller              = mc;
	me->audioTracks.tracks      = gf_list_new();
	me->videoTracks.tracks      = gf_list_new();
	me->textTracks.tracks       = gf_list_new();
	me->buffered					= gf_html_timeranges_new(1);
	me->played		            = gf_html_timeranges_new(1);
	me->seekable					= gf_html_timeranges_new(1);
	return me;
}

void gf_html_media_element_del(GF_HTML_MediaElement *me)
{
	if (me->startDate) gf_free(me->startDate);
	if (me->currentSrc) gf_free(me->currentSrc);
	gf_html_tracklist_del(&me->audioTracks);
	gf_html_tracklist_del(&me->videoTracks);
	gf_html_tracklist_del(&me->textTracks);
	gf_html_timeranges_del(me->buffered);
	gf_html_timeranges_del(me->seekable);
	gf_html_timeranges_del(me->played);
	gf_free(me);
}

GF_HTML_MediaController *gf_html_media_controller_new()
{
	GF_HTML_MediaController *mc;
	GF_SAFEALLOC(mc, GF_HTML_MediaController);
	if (!mc) return NULL;
	mc->buffered          = gf_html_timeranges_new(1);
	mc->played            = gf_html_timeranges_new(1);
	mc->seekable          = gf_html_timeranges_new(1);
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
	gf_html_timeranges_del(mc->buffered);
	gf_html_timeranges_del(mc->seekable);
	gf_html_timeranges_del(mc->played);
	gf_free(mc);
}

#endif

