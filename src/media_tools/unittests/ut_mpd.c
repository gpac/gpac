#include "tests.h"
#include "../mpd.c"

// functions used by mpd.c, needed for linking
#include "../m3u8.c"
void gf_xml_dump_string(FILE* file, const char *before, const char *str, const char *after) {
	if (before) gf_fprintf(file, "%s", before);
	if (str) {
		for (const char *p = str; *p; p++) {
			switch (*p) {
			case '&': gf_fprintf(file, "&amp;"); break;
			case '<': gf_fprintf(file, "&lt;"); break;
			case '>': gf_fprintf(file, "&gt;"); break;
			case '\'': gf_fprintf(file, "&apos;"); break;
			case '"': gf_fprintf(file, "&quot;"); break;
			default: gf_fprintf(file, "%c", *p); break;
			}
		}
	}
	if (after) gf_fprintf(file, "%s", after);
}

unittest(mpd_event_streams)
{
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_Err e = gf_xml_dom_parse_string(dom, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\">\n"
". <Period id=\"1\" start=\"PT0S\">\n"
"    <EventStream schemeIdUri=\"urn:scte:scte35:2014:xml+bin\" timescale=\"1000\">\n"
"      <Event presentationTime=\"1765533611520\" duration=\"38400\"  id=\"14596012\">\n"
"       <Signal xmlns=\"http://www.scte.org/schemas/35/2016\">\n"
"          <Binary>/DAgAAAAAAAAAP/wDwUA3resf//+ADS8AMAAAAAAAORhJCQ=</Binary>\n"
"        </Signal>\n"
"      </Event>\n"
"      <Event presentationTime=\"1765533732480\" duration=\"38400\" id=\"14596013\">\n"
"        <Signal xmlns=\"http://www.scte.org/schemas/35/2016\">\n"
"          <Binary>/DAgAAAAAAAAAP/wDwUA3retf//+ADS8AMAAAAAAAORhJCQ=</Binary>\n"
"        </Signal>\n"
"      </Event>\n"
"    </EventStream>\n"
"  </Period>\n"
"</MPD>\n");
	assert_true(e == GF_OK);
	GF_MPD *mpd = gf_mpd_new();
	e = gf_mpd_init_from_dom(gf_xml_dom_get_root(dom), mpd, NULL);
	assert_true(e == GF_OK);

	GF_List *evts = ((GF_MPD_Period *)gf_list_get(mpd->periods, 0))->event_streams;
	assert_equal(gf_list_count(evts), 1, "%u");
	GF_MPD_EventStream *es = gf_list_get(evts, 0);
	assert_equal(gf_list_count(es->entries), 2, "%u");

    GF_MPD_EventStreamEntry *entry = gf_list_get(es->entries, 1);
    assert_equal(entry->presentation_time, (s64)1765533732480, LLD);
    assert_equal(entry->duration, 38400, "%u");
    assert_equal(entry->id, 14596013, "%u");
    assert_equal(entry->message_size, 35, "%u");

	{
		u32 sz = 2*entry->message_size+3;
		u8 *b64 = gf_malloc(sizeof(char)*sz);
		assert_true(b64 != NULL);
		sz = gf_base64_encode((u8*)entry->message, entry->message_size, b64, sz);
		b64[sz] = 0;
		assert_equal(sz, 48, "%u");
		assert_equal_str(b64, "/DAgAAAAAAAAAP/wDwUA3retf//+ADS8AMAAAAAAAORhJCQ=");
		gf_free(b64);
	}

	gf_xml_dom_del(dom);
	gf_mpd_del(mpd);
}

unittest(mpd_segment_event_associations_roundtrip)
{
	GF_DASH_SegmentContext *seg;
	GF_List *segments = gf_list_new();
	GF_MPD_Representation *rep = gf_mpd_representation_new();
	FILE *out = gf_file_temp(NULL);
	char serialized[8192];
	char xml[9000];
	GF_DOMParser *dom = gf_xml_dom_new();
	GF_MPD *mpd = gf_mpd_new();
	GF_List *restored;
	GF_DASH_SegmentContext *read_seg;
	GF_DASH_SegmentEventAssociation *a;
	GF_Err e;

	GF_SAFEALLOC(seg, GF_DASH_SegmentContext);
	assert_true(seg != NULL);
	seg->time = 1000;
	seg->dur = 600;
	seg->seg_num = 13;
	seg->filename = gf_strdup("segment13.m4s");
	assert_equal(gf_dash_segment_event_association_add(seg, "urn:scte:scte35:2014:xml+bin?x=1&y=2", 8076794552LL, 662, GF_DASH_SEGMENT_EVENT_START), GF_OK, "%d");
	assert_equal(gf_dash_segment_event_association_add(seg, "urn:scte:scte35:2014:xml+bin?x=1&y=2", -5LL, 662, GF_DASH_SEGMENT_EVENT_END), GF_OK, "%d");
	assert_equal(gf_list_count(seg->event_associations), 2, "%u");
	gf_list_add(segments, seg);
	gf_mpd_print_dasher_segments(out, segments, 0);
	gf_fseek(out, 0, SEEK_SET);
	size_t len = gf_fread(serialized, sizeof(serialized)-1, out);
	serialized[len] = 0;
	assert_true(strstr(serialized, "eventAssociation") != NULL);
	assert_true(strstr(serialized, "events_explicit=\"1\"") != NULL);
	assert_true(strstr(serialized, "boundary=\"2\"") != NULL);
	assert_true(strstr(serialized, "x=1&amp;y=2") != NULL);
	snprintf(xml, sizeof(xml), "<MPD xmlns:gpac=\"urn:gpac:filters:dasher:2018\">%s</MPD>", serialized);
	e = gf_xml_dom_parse_string(dom, xml);
	assert_equal(e, GF_OK, "%d");
	GF_XMLNode *segments_node = gf_list_get(gf_xml_dom_get_root(dom)->content, 0);
	restored = gf_mpd_parse_segments_context(mpd, segments_node);
	assert_true(restored != NULL);
	assert_equal(gf_list_count(restored), 1, "%u");
	read_seg = gf_list_get(restored, 0);
	assert_equal(gf_list_count(read_seg->event_associations), 2, "%u");
	a = gf_list_get(read_seg->event_associations, 0);
	assert_equal_str(a->scheme_id_uri, "urn:scte:scte35:2014:xml+bin?x=1&y=2");
	assert_equal(a->presentation_time, (s64)8076794552LL, LLD);
	assert_equal(a->id, 662, "%u");
	assert_equal(a->boundary_type, GF_DASH_SEGMENT_EVENT_START, "%u");
	a = gf_list_get(read_seg->event_associations, 1);
	assert_equal(a->boundary_type, GF_DASH_SEGMENT_EVENT_END, "%u");
	assert_equal(a->presentation_time, (s64)-5, LLD);

	/* Exercise the public owning representation's existing cleanup path. */
	rep->state_seg_list = segments;
	gf_mpd_representation_free(rep);
	rep = gf_mpd_representation_new();
	rep->state_seg_list = restored;
	gf_mpd_representation_free(rep);
	gf_xml_dom_del(dom);
	gf_mpd_del(mpd);
	gf_fclose(out);
}


/* The segment-context ABI maps SCTE events to physical boundaries. The writer
 * must not infer a different placement from each rendition's clock origin. */
unittest(mpd_hls_explicit_scte35_boundaries)
{
    GF_MPD *mpd = gf_mpd_new();
    GF_MPD_Period *period = NULL;
    GF_MPD_AdaptationSet *as = NULL;
    GF_MPD_Representation *rep = NULL;
    GF_MPD_EventStream *es;
    GF_MPD_EventStreamEntry *evt;
    GF_DASH_SegmentContext *sctx;
    char buf[16384];
    FILE *out = gf_file_temp(NULL);
    const char *scheme = "urn:scte:scte35:2014:xml+bin";
    const s64 source_pts = 8076794552LL;
    u32 i;
    size_t len;
    assert_true(mpd != NULL);
    assert_true(out != NULL);

    period = gf_mpd_period_new();
    assert_true(period != NULL);
    gf_list_add(mpd->periods, period);
    assert_true(period->event_streams != NULL);
    GF_SAFEALLOC(es, GF_MPD_EventStream);
    assert_true(es != NULL);
    es->scheme_id_uri = gf_strdup(scheme);
    es->timescale = 90000;
    es->entries = gf_list_new();
    GF_SAFEALLOC(evt, GF_MPD_EventStreamEntry);
    assert_true(evt != NULL);
    evt->id = 662;
    evt->presentation_time = source_pts;
    evt->duration = 18000000;
    gf_list_add(es->entries, evt);
    gf_list_add(period->event_streams, es);

    as = gf_mpd_adaptation_set_new();
    assert_true(as != NULL);
    rep = gf_mpd_representation_new();
    assert_true(rep != NULL);
    gf_list_add(as->representations, rep);
    gf_list_add(period->adaptation_sets, as);
    rep->state_seg_list = gf_list_new();
    rep->timescale = 90000;
    rep->timescale_mpd = 1000;
    rep->hls_max_seg_dur.num = 10;
    rep->hls_max_seg_dur.den = 1;
    as->starts_with_sap = GF_FILTER_SAP_1;
    for (i=0; i<3; i++) {
        const u32 numbers[3] = {12, 13, 53};
        const u64 times[3] = {120000, 130000, 530000};
        GF_SAFEALLOC(sctx, GF_DASH_SegmentContext);
        assert_true(sctx != NULL);
        sctx->time = times[i];
        sctx->dur = 900000; /* 10s at media timescale; time is 1k MPD */
        sctx->seg_num = numbers[i];
        {
            char name[32];
            snprintf(name, sizeof(name), "seg%u.m4s", numbers[i]);
            sctx->filename = gf_strdup(name);
        }
        /* An empty list explicitly disables the upstream clock-based inference. */
        sctx->event_associations = gf_list_new();
        assert_true(sctx->event_associations != NULL);
        if (numbers[i] == 13)
            assert_equal(gf_dash_segment_event_association_add(sctx, scheme, source_pts, 662, GF_DASH_SEGMENT_EVENT_START), GF_OK, "%d");
        else if (numbers[i] == 53)
            assert_equal(gf_dash_segment_event_association_add(sctx, scheme, source_pts, 662, GF_DASH_SEGMENT_EVENT_END), GF_OK, "%d");
        gf_list_add(rep->state_seg_list, sctx);
    }
    assert_true(hls_has_explicit_segment_events(rep));
    /* The old implementation would place CUE-OUT based on unrelated source
     * timestamps, and depend on mutable event state to generate CUE-IN. */
    assert_equal(gf_mpd_write_m3u8_playlist(mpd, period, as, rep, "test.m3u8", 7, 0, NULL, GF_FALSE, GF_TRUE, out), GF_OK, "%d");
    gf_fseek(out, 0, SEEK_SET);
    len = gf_fread(buf, sizeof(buf)-1, out);
    buf[len] = 0;
    assert_true(strstr(buf, "#EXT-X-CUE-OUT:200") != NULL);
    assert_true(strstr(buf, "#EXT-X-CUE-IN") != NULL);
    assert_true(strstr(buf, "#EXT-X-DATERANGE:ID=\"662-0000\"") != NULL);
    assert_equal(evt->state, 0, "%d");
    /* Exactly one OUT and one IN, at their annotated segment positions. */
    const char *cue_out = strstr(buf, "#EXT-X-CUE-OUT:");
    const char *cue_in = strstr(buf, "#EXT-X-CUE-IN");
    assert_true(cue_out && cue_in && cue_out < cue_in);
    assert_true(strstr(buf, "seg12.m4s") < cue_out);
    assert_true(cue_out < strstr(buf, "seg13.m4s"));
    assert_true(strstr(buf, "seg13.m4s") < cue_in);
    assert_true(cue_in < strstr(buf, "seg53.m4s"));
    assert_true(strstr(cue_out+1, "#EXT-X-CUE-OUT:") == NULL);
    assert_true(strstr(cue_in+1, "#EXT-X-CUE-IN") == NULL);
    /* Note: playlist has three artificial segments but their event positions
     * are independent of the source event's 90 kHz absolute clock. */
    gf_fclose(out);
    gf_mpd_del(mpd);
}

/* Empty explicit lists must persist: otherwise a restarted Dasher could
 * accidentally resume the legacy timestamp-based cue placement. */
unittest(mpd_hls_explicit_empty_segment_restart)
{
    GF_DASH_SegmentContext *sctx;
    GF_List *segments = gf_list_new();
    GF_List *restored;
    GF_MPD_Representation *rep = gf_mpd_representation_new();
    GF_DOMParser *dom = gf_xml_dom_new();
    GF_MPD *mpd = gf_mpd_new();
    FILE *out = gf_file_temp(NULL);
    char xml[4096], segment_xml[3700];
    size_t len;
    GF_Err e;

    GF_SAFEALLOC(sctx, GF_DASH_SegmentContext);
    assert_true(sctx != NULL);
    sctx->time = 15000;
    sctx->dur = 900000;
    sctx->filename = gf_strdup("segment15.m4s");
    sctx->seg_num = 15;
    sctx->event_associations = gf_list_new();
    assert_true(sctx->event_associations != NULL);
    gf_list_add(segments, sctx);
    gf_mpd_print_dasher_segments(out, segments, 0);
    gf_fseek(out, 0, SEEK_SET);
    len = gf_fread(segment_xml, sizeof(segment_xml)-1, out);
    segment_xml[len] = 0;
    assert_true(strstr(segment_xml, "events_explicit=\"1\"") != NULL);
    snprintf(xml, sizeof(xml), "<MPD xmlns:gpac=\"urn:gpac:filters:dasher:2018\">%s</MPD>", segment_xml);
    e = gf_xml_dom_parse_string(dom, xml);
    assert_equal(e, GF_OK, "%d");
    restored = gf_mpd_parse_segments_context(mpd, gf_list_get(gf_xml_dom_get_root(dom)->content, 0));
    assert_true(restored != NULL);
    assert_equal(gf_list_count(restored), 1, "%u");
    sctx = gf_list_get(restored, 0);
    assert_true(sctx->event_associations != NULL);
    assert_equal(gf_list_count(sctx->event_associations), 0, "%u");
    assert_equal(sctx->seg_num, 15, "%u");
    rep->state_seg_list = segments;
    gf_mpd_representation_free(rep);
    rep = gf_mpd_representation_new();
    rep->state_seg_list = restored;
    gf_mpd_representation_free(rep);
    gf_xml_dom_del(dom);
    gf_mpd_del(mpd);
    gf_fclose(out);
}
