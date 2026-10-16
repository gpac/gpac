/*
 * GPAC - internal DASH segment event associations.
 *
 * This file is part of GPAC, distributed under the LGPL version 2 or later.
 * An association describes a boundary-to-event mapping; the event payload
 * remains in the enclosing Period's EventStream. The details deliberately
 * stay in an internal header so future association kinds do not affect the
 * public GF_DASH_SegmentContext layout.
 */
#ifndef GPAC_INTERNAL_DASH_SEGMENT_EVENTS_H
#define GPAC_INTERNAL_DASH_SEGMENT_EVENTS_H

#include <gpac/mpd.h>

/* Boundary transitions. Other values are reserved for future event types. */
#define GF_DASH_SEGMENT_EVENT_START 1
#define GF_DASH_SEGMENT_EVENT_END   2

typedef struct {
    /* Owned copy of the EventStream's schemeIdUri. */
    char *scheme_id_uri;
    /* Event identity in the original EventStream timescale. */
    s64 presentation_time;
    u32 id;
    /* Boundary transition, not a request to create a segment. */
    u32 boundary_type;
} GF_DASH_SegmentEventAssociation;

static GFINLINE GF_Err gf_dash_segment_event_association_add(GF_DASH_SegmentContext *segment,
        const char *scheme_id_uri, s64 presentation_time, u32 id, u32 boundary_type)
{
    GF_DASH_SegmentEventAssociation *assoc;
    GF_Err e;
    Bool created = GF_FALSE;

    if (!segment || !scheme_id_uri || !scheme_id_uri[0] || !boundary_type)
        return GF_BAD_PARAM;
    GF_SAFEALLOC(assoc, GF_DASH_SegmentEventAssociation);
    if (!assoc) return GF_OUT_OF_MEM;
    assoc->scheme_id_uri = gf_strdup(scheme_id_uri);
    if (!assoc->scheme_id_uri) {
        gf_free(assoc);
        return GF_OUT_OF_MEM;
    }
    assoc->presentation_time = presentation_time;
    assoc->id = id;
    assoc->boundary_type = boundary_type;
    if (!segment->event_associations) {
        segment->event_associations = gf_list_new();
        if (!segment->event_associations) {
            gf_free(assoc->scheme_id_uri);
            gf_free(assoc);
            return GF_OUT_OF_MEM;
        }
        created = GF_TRUE;
    }
    e = gf_list_add(segment->event_associations, assoc);
    if (e) {
        if (created) {
            gf_list_del(segment->event_associations);
            segment->event_associations = NULL;
        }
        gf_free(assoc->scheme_id_uri);
        gf_free(assoc);
    }
    return e;
}

static GFINLINE void gf_dash_segment_event_associations_del(GF_DASH_SegmentContext *segment)
{
    if (!segment || !segment->event_associations) return;
    while (gf_list_count(segment->event_associations)) {
        GF_DASH_SegmentEventAssociation *assoc = gf_list_pop_back(segment->event_associations);
        gf_free(assoc->scheme_id_uri);
        gf_free(assoc);
    }
    gf_list_del(segment->event_associations);
    segment->event_associations = NULL;
}

#endif /* GPAC_INTERNAL_DASH_SEGMENT_EVENTS_H */
