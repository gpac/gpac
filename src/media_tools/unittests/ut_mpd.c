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
    GF_SAFEALLOC(as->segment_template, GF_MPD_SegmentTemplate);
    assert_true(as->segment_template != NULL);
    as->segment_template->timescale = 90000;
    as->segment_template->presentation_time_offset = 8070463950ULL;
    rep->hls_max_seg_dur.num = 10;
    rep->hls_max_seg_dur.den = 1;
    as->starts_with_sap = GF_FILTER_SAP_1;
    for (i=0; i<3; i++) {
        const u32 numbers[3] = {12, 13, 53};
        const u64 times[3] = {67000, 70360, 312360};
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
    assert_true(strstr(buf, "1970-01-01T00:01:10.340Z") != NULL);
    assert_true(strstr(buf, "1970-01-01T00:01:10.360Z") == NULL);
    assert_true(strstr(buf, "#EXT-X-DATERANGE") < strstr(buf, "seg12.m4s"));
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

/* Producer-independent writer acceptance for standalone #11.01. */
unittest(mpd_hls_explicit_short_break_and_orphan_policy)
{
    const char *scheme = "urn:scte:scte35:2014:xml+bin";
    GF_MPD_Period period = {0};
    GF_MPD_AdaptationSet as = {0};
    GF_MPD_Representation rep = {0};
    GF_MPD_EventStream es = {0};
    GF_MPD_EventStreamEntry evt = {0};
    GF_DASH_SegmentContext seg[3] = {{0}};
    GF_DASH_SegmentEventAssociation *end_assoc;
    FILE *out;
    char text[4096];
    size_t n;
    period.event_streams = gf_list_new();
    es.entries = gf_list_new();
    rep.state_seg_list = gf_list_new();
    assert_true(period.event_streams && es.entries && rep.state_seg_list);
    es.scheme_id_uri = (char *)scheme;
    es.timescale = rep.timescale = rep.timescale_mpd = 1000;
    evt.id = 100;
    evt.presentation_time = 1920;
    evt.duration = 1000;
    gf_list_add(es.entries, &evt);
    gf_list_add(period.event_streams, &es);
    seg[0].time = 0; seg[0].dur = 3000; seg[0].filename = "s0.m4s";
    seg[1].time = 3000; seg[1].dur = 3000; seg[1].filename = "s1.m4s";
    seg[2].time = 6000; seg[2].dur = 960; seg[2].filename = "s2.m4s";
    for (u32 i=0; i<3; i++) gf_list_add(rep.state_seg_list, &seg[i]);
    assert_equal(gf_dash_segment_event_association_add(&seg[1], scheme, 1920, 100, GF_DASH_SEGMENT_EVENT_START), GF_OK, "%d");
    assert_equal(gf_dash_segment_event_association_add(&seg[1], scheme, 1920, 100, GF_DASH_SEGMENT_EVENT_END), GF_OK, "%d");

    /* Both transitions on one segment: retain SCTE metadata, no ad window. */
    out = gf_file_temp(NULL); assert_true(out != NULL);
    /* Event lies entirely inside segment 0, with no replaceable media.
     * DATERANGE must survive even if no START association is provided. */
    hls_insert_scte35_event_metadata(out, 0, &period, &as, &rep, &seg[0]);
    hls_insert_scte35_associations(out, &period, &rep, &seg[1], GF_TRUE);
    gf_fseek(out, 0, SEEK_SET);
    n = gf_fread(text, sizeof(text)-1, out); text[n] = 0;
    assert_true(strstr(text, "#EXT-X-DATERANGE:ID=\"100-0000\"") != NULL);
    assert_true(strstr(text, "1970-01-01T00:00:01.920Z") != NULL);
    assert_true(strstr(text, "#EXT-X-CUE-OUT") == NULL);
    assert_true(strstr(text, "#EXT-X-CUE-IN") == NULL);
    gf_fclose(out);

    /* Move return to a later physical segment; valid replaceable break. */
    end_assoc = gf_list_get(seg[1].event_associations, 1);
    end_assoc->boundary_type = 99; /* ignored by association writer */
    assert_equal(gf_dash_segment_event_association_add(&seg[2], scheme, 1920, 100, GF_DASH_SEGMENT_EVENT_END), GF_OK, "%d");
    out = gf_file_temp(NULL); assert_true(out != NULL);
    hls_insert_scte35_associations(out, &period, &rep, &seg[1], GF_TRUE);
    gf_fseek(out, 0, SEEK_SET);
    n = gf_fread(text, sizeof(text)-1, out); text[n] = 0;
    assert_true(strstr(text, "#EXT-X-CUE-OUT:1") != NULL);
    gf_fclose(out);
    out = gf_file_temp(NULL); assert_true(out != NULL);
    hls_insert_scte35_associations(out, &period, &rep, &seg[2], GF_TRUE);
    gf_fseek(out, 0, SEEK_SET);
    n = gf_fread(text, sizeof(text)-1, out); text[n] = 0;
    assert_true(strstr(text, "#EXT-X-CUE-IN") != NULL);
    gf_fclose(out);

    /* Finalized playlist without a return cannot signal CUE-OUT; live may. */
    end_assoc = gf_list_get(seg[2].event_associations, 0);
    end_assoc->boundary_type = 99;
    out = gf_file_temp(NULL); assert_true(out != NULL);
    hls_insert_scte35_associations(out, &period, &rep, &seg[1], GF_TRUE);
    gf_fseek(out, 0, SEEK_SET);
    n = gf_fread(text, sizeof(text)-1, out); text[n] = 0;
    assert_true(strstr(text, "#EXT-X-CUE-OUT") == NULL);
    gf_fclose(out);
    out = gf_file_temp(NULL); assert_true(out != NULL);
    hls_insert_scte35_associations(out, &period, &rep, &seg[1], GF_FALSE);
    gf_fseek(out, 0, SEEK_SET);
    n = gf_fread(text, sizeof(text)-1, out); text[n] = 0;
    assert_true(strstr(text, "#EXT-X-CUE-OUT:1") != NULL);
    gf_fclose(out);

    for (u32 i=0; i<3; i++) gf_dash_segment_event_associations_del(&seg[i]);
    gf_list_del(rep.state_seg_list);
    gf_list_del(es.entries);
    gf_list_del(period.event_streams);
}


/* Standalone #11.x acceptance: a live sliding window may outlive its CUE-OUT.
 * Preserve the original SCTE event identity even when its replacement start
 * is no longer in the published playlist. Rewrites must be deterministic. */
unittest(mpd_hls_explicit_live_window_rewrite)
{
    const char *scheme = "urn:scte:scte35:2014:xml+bin";
    GF_MPD mpd = {0};
    GF_MPD_Period period = {0};
    GF_MPD_AdaptationSet as = {0};
    GF_MPD_Representation rep = {0};
    GF_MPD_EventStream es = {0};
    GF_MPD_EventStreamEntry evt = {0};
    GF_DASH_SegmentContext seg[8] = {{0}};
    u8 message[4] = {0xfc, 0x30, 0x01, 0x02};
    char files[8][32];
    char full[8192], window[8192], rewrite[8192];
    FILE *out;
    size_t sz;

    mpd.type = GF_MPD_TYPE_DYNAMIC;
    mpd.availabilityStartTime = 0;
    period.event_streams = gf_list_new();
    es.entries = gf_list_new();
    rep.state_seg_list = gf_list_new();
    assert_true(period.event_streams && es.entries && rep.state_seg_list);
    es.scheme_id_uri = (char *)scheme;
    es.timescale = rep.timescale = rep.timescale_mpd = 1000;
    evt.id = 100;
    evt.presentation_time = 1920;
    evt.duration = 8500; /* original break ends at 10.420 s */
    evt.message = message;
    evt.message_size = sizeof(message);
    gf_list_add(es.entries, &evt);
    gf_list_add(period.event_streams, &es);
    rep.hls_max_seg_dur.num = 2;
    rep.hls_max_seg_dur.den = 1;
    as.starts_with_sap = GF_FILTER_SAP_1;
    for (u32 i = 0; i < 8; i++) {
        seg[i].time = 2000 * i;
        seg[i].dur = 2000;
        seg[i].seg_num = i + 1;
        snprintf(files[i], sizeof(files[i]), "chunk%u.m4s", i);
        seg[i].filename = files[i];
        seg[i].event_associations = gf_list_new();
        assert_true(seg[i].event_associations != NULL);
        gf_list_add(rep.state_seg_list, &seg[i]);
    }
    assert_equal(gf_dash_segment_event_association_add(&seg[1], scheme, 1920, 100, GF_DASH_SEGMENT_EVENT_START), GF_OK, "%d");
    assert_equal(gf_dash_segment_event_association_add(&seg[6], scheme, 1920, 100, GF_DASH_SEGMENT_EVENT_END), GF_OK, "%d");

    out = tmpfile(); assert_true(out != NULL);
    assert_equal(gf_mpd_write_m3u8_playlist(&mpd, &period, &as, &rep, "test.m3u8", 7, 0, NULL, GF_FALSE, GF_FALSE, out), GF_OK, "%d");
    fflush(out); rewind(out);
    sz = fread(full, 1, sizeof(full)-1, out); full[sz] = 0;
    assert_true(strstr(full, "#EXT-X-DATERANGE:ID=\"100-0000\"") != NULL);
    assert_true(strstr(full, "1970-01-01T00:00:01.920Z") != NULL);
    assert_true(strstr(full, "SCTE35-OUT=0xFC300102") != NULL);
    assert_true(strstr(full, "#EXT-X-CUE-OUT:") < strstr(full, "chunk1.m4s"));
    assert_true(strstr(full, "#EXT-X-CUE-IN") < strstr(full, "chunk6.m4s"));
    fclose(out);

    /* Sliding window begins inside the original break, after CUE-OUT. */
    rep.tsb_first_entry = 3;
    out = tmpfile(); assert_true(out != NULL);
    assert_equal(gf_mpd_write_m3u8_playlist(&mpd, &period, &as, &rep, "test.m3u8", 7, 0, NULL, GF_FALSE, GF_FALSE, out), GF_OK, "%d");
    fflush(out); rewind(out);
    sz = fread(window, 1, sizeof(window)-1, out); window[sz] = 0;
    assert_true(strstr(window, "#EXT-X-DATERANGE:ID=\"100-0000\"") != NULL);
    assert_true(strstr(window, "1970-01-01T00:00:01.920Z") != NULL);
    assert_true(strstr(window, "SCTE35-OUT=0xFC300102") != NULL);
    assert_true(strstr(window, "#EXT-X-CUE-OUT:") == NULL);
    assert_true(strstr(window, "#EXT-X-CUE-IN") != NULL);
    assert_true(strstr(window, "#EXT-X-CUE-IN") < strstr(window, "chunk6.m4s"));
    assert_true(strstr(window, "chunk0.m4s") == NULL);
    assert_true(strstr(window, "chunk1.m4s") == NULL);
    fclose(out);

    out = tmpfile(); assert_true(out != NULL);
    assert_equal(gf_mpd_write_m3u8_playlist(&mpd, &period, &as, &rep, "test.m3u8", 7, 0, NULL, GF_FALSE, GF_FALSE, out), GF_OK, "%d");
    fflush(out); rewind(out);
    sz = fread(rewrite, 1, sizeof(rewrite)-1, out); rewrite[sz] = 0;
    assert_equal_str(window, rewrite);
    fclose(out);

    /* Now physically purge the original CUE-OUT and two more old segments.
     * The previous assertions exercised tsb_first_entry only; this also
     * exercises the absence of the START association from state_seg_list. */
    for (u32 i=0; i<3; i++) {
        gf_dash_segment_event_associations_del(&seg[i]);
        gf_list_rem(rep.state_seg_list, 0);
    }
    rep.tsb_first_entry = 0;
    out = tmpfile(); assert_true(out != NULL);
    assert_equal(gf_mpd_write_m3u8_playlist(&mpd, &period, &as, &rep, "test.m3u8", 7, 0, NULL, GF_FALSE, GF_FALSE, out), GF_OK, "%d");
    fflush(out); rewind(out);
    sz = fread(rewrite, 1, sizeof(rewrite)-1, out); rewrite[sz] = 0;
    assert_true(strstr(rewrite, "#EXT-X-DATERANGE:ID=\"100-0000\"") != NULL);
    assert_true(strstr(rewrite, "1970-01-01T00:00:01.920Z") != NULL);
    assert_true(strstr(rewrite, "SCTE35-OUT=0xFC300102") != NULL);
    assert_true(strstr(rewrite, "#EXT-X-CUE-OUT:") == NULL);
    assert_true(strstr(rewrite, "#EXT-X-CUE-IN") != NULL);
    assert_true(strstr(rewrite, "#EXT-X-CUE-IN") < strstr(rewrite, "chunk6.m4s"));
    fclose(out);

    for (u32 i=3; i<8; i++) gf_dash_segment_event_associations_del(&seg[i]);
    gf_list_del(rep.state_seg_list);
    gf_list_del(es.entries);
    gf_list_del(period.event_streams);
}

/* Both transitions can resolve to the same physical cut. Do not manufacture
 * a zero-length ad window, even in live output. Neither should a finalized
 * VOD contain an unmatched CUE-OUT. Keep the event's DATERANGE in both. */
unittest(mpd_hls_explicit_short_break_endlist)
{
    const char *scheme = "urn:scte:scte35:2014:xml+bin";
    GF_MPD mpd = {0};
    GF_MPD_Period period = {0};
    GF_MPD_AdaptationSet as = {0};
    GF_MPD_Representation rep = {0};
    GF_MPD_EventStream es = {0};
    GF_MPD_EventStreamEntry evt = {0};
    GF_DASH_SegmentContext seg[3] = {{0}};
    char text[8192];
    FILE *out;
    size_t sz;
    period.event_streams = gf_list_new();
    es.entries = gf_list_new();
    rep.state_seg_list = gf_list_new();
    assert_true(period.event_streams && es.entries && rep.state_seg_list);
    es.scheme_id_uri = (char *)scheme;
    es.timescale = rep.timescale = rep.timescale_mpd = 1000;
    evt.id = 101;
    evt.presentation_time = 1920;
    evt.duration = 40;
    gf_list_add(es.entries, &evt);
    gf_list_add(period.event_streams, &es);
    rep.hls_max_seg_dur.num = 2;
    rep.hls_max_seg_dur.den = 1;
    as.starts_with_sap = GF_FILTER_SAP_1;
    for (u32 i=0; i<3; i++) {
        static char *names[] = {"s0.m4s", "s1.m4s", "s2.m4s"};
        seg[i].time = 2000 * i;
        seg[i].dur = 2000;
        seg[i].seg_num = i + 1;
        seg[i].filename = names[i];
        seg[i].event_associations = gf_list_new();
        assert_true(seg[i].event_associations != NULL);
        gf_list_add(rep.state_seg_list, &seg[i]);
    }
    assert_equal(gf_dash_segment_event_association_add(&seg[1], scheme, 1920, 101, GF_DASH_SEGMENT_EVENT_START), GF_OK, "%d");
    assert_equal(gf_dash_segment_event_association_add(&seg[1], scheme, 1920, 101, GF_DASH_SEGMENT_EVENT_END), GF_OK, "%d");
    for (u32 live=0; live<2; live++) {
        mpd.type = live ? GF_MPD_TYPE_DYNAMIC : GF_MPD_TYPE_STATIC;
        out = tmpfile(); assert_true(out != NULL);
        assert_equal(gf_mpd_write_m3u8_playlist(&mpd, &period, &as, &rep, "test.m3u8", 7, 0, NULL, GF_FALSE, GF_TRUE, out), GF_OK, "%d");
        fflush(out); rewind(out);
        sz = fread(text, 1, sizeof(text)-1, out); text[sz] = 0;
        assert_true(strstr(text, "#EXT-X-DATERANGE:ID=\"101-0000\"") != NULL);
        assert_true(strstr(text, "1970-01-01T00:00:01.920Z") != NULL);
        assert_true(strstr(text, "#EXT-X-CUE-OUT:") == NULL);
        assert_true(strstr(text, "#EXT-X-CUE-IN") == NULL);
        fclose(out);
    }
    /* START-only event on finalized VOD is never replaceable. */
    GF_DASH_SegmentEventAssociation *end = gf_list_get(seg[1].event_associations, 1);
    end->boundary_type = 99;
    mpd.type = GF_MPD_TYPE_STATIC;
    out = tmpfile(); assert_true(out != NULL);
    assert_equal(gf_mpd_write_m3u8_playlist(&mpd, &period, &as, &rep, "test.m3u8", 7, 0, NULL, GF_FALSE, GF_TRUE, out), GF_OK, "%d");
    fflush(out); rewind(out);
    sz = fread(text, 1, sizeof(text)-1, out); text[sz] = 0;
    assert_true(strstr(text, "#EXT-X-DATERANGE:ID=\"101-0000\"") != NULL);
    assert_true(strstr(text, "#EXT-X-CUE-OUT:") == NULL);
    assert_true(strstr(text, "#EXT-X-CUE-IN") == NULL);
    fclose(out);
    for (u32 i=0; i<3; i++) gf_dash_segment_event_associations_del(&seg[i]);
    gf_list_del(rep.state_seg_list);
    gf_list_del(es.entries);
    gf_list_del(period.event_streams);
}

/* The same SCTE event and payload must yield the same DATERANGE across
 * independently clocked A/V representations, while their physical cut
 * locations follow their own explicit segment associations. */
unittest(mpd_hls_explicit_two_rendition_event_fidelity)
{
    const char *scheme = "urn:scte:scte35:2014:xml+bin";
    GF_MPD mpd = {0};
    GF_MPD_Period period = {0};
    GF_MPD_AdaptationSet as = {0};
    GF_MPD_EventStream es = {0};
    GF_MPD_EventStreamEntry evt = {0};
    GF_MPD_Representation rep[2] = {{0}};
    GF_DASH_SegmentContext seg[2][4] = {{{0}}};
    char files[2][4][32];
    char playlists[2][8192];
    u8 message[3] = {0xfc, 0x11, 0x35};
    period.event_streams = gf_list_new();
    es.entries = gf_list_new();
    assert_true(period.event_streams && es.entries);
    es.scheme_id_uri = (char *)scheme;
    es.timescale = 90000;
    evt.id = 662;
    evt.presentation_time = 172800; /* 1.920 s */
    evt.duration = 90000; /* 1 s */
    evt.message = message;
    evt.message_size = sizeof(message);
    gf_list_add(es.entries, &evt);
    gf_list_add(period.event_streams, &es);
    as.starts_with_sap = GF_FILTER_SAP_1;
    for (u32 r=0; r<2; r++) {
        rep[r].state_seg_list = gf_list_new();
        rep[r].timescale = r ? 48000 : 90000;
        rep[r].timescale_mpd = 1000;
        rep[r].hls_max_seg_dur.num = 3;
        rep[r].hls_max_seg_dur.den = 1;
        assert_true(rep[r].state_seg_list != NULL);
        for (u32 i=0; i<4; i++) {
            u64 starts[2][4] = {{0,2000,4000,6000},{0,2050,4080,6040}};
            u64 ends[2][4] = {{2000,2000,2000,2000},{2050,2030,1960,2000}};
            GF_DASH_SegmentContext *s = &seg[r][i];
            s->time = starts[r][i];
            s->dur = (ends[r][i] * rep[r].timescale)/1000;
            s->seg_num = i+1;
            snprintf(files[r][i], sizeof(files[r][i]), "%c%u.m4s", r ? 'a':'v', i);
            s->filename = files[r][i];
            s->event_associations = gf_list_new();
            assert_true(s->event_associations != NULL);
            gf_list_add(rep[r].state_seg_list, s);
        }
        assert_equal(gf_dash_segment_event_association_add(&seg[r][1], scheme, 172800, 662, GF_DASH_SEGMENT_EVENT_START), GF_OK, "%d");
        assert_equal(gf_dash_segment_event_association_add(&seg[r][2], scheme, 172800, 662, GF_DASH_SEGMENT_EVENT_END), GF_OK, "%d");
        FILE *out = tmpfile(); assert_true(out != NULL);
        assert_equal(gf_mpd_write_m3u8_playlist(&mpd, &period, &as, &rep[r], "test.m3u8", 7, 0, NULL, GF_FALSE, GF_TRUE, out), GF_OK, "%d");
        fflush(out); rewind(out);
        size_t sz = fread(playlists[r], 1, sizeof(playlists[r])-1, out);
        playlists[r][sz] = 0;
        assert_true(strstr(playlists[r], "#EXT-X-DATERANGE:ID=\"662-0000\"") != NULL);
        assert_true(strstr(playlists[r], "1970-01-01T00:00:01.920Z") != NULL);
        assert_true(strstr(playlists[r], "SCTE35-OUT=0xFC1135") != NULL);
        assert_true(strstr(playlists[r], "#EXT-X-CUE-OUT:1") < strstr(playlists[r], files[r][1]));
        assert_true(strstr(playlists[r], "#EXT-X-CUE-IN") < strstr(playlists[r], files[r][2]));
        fclose(out);
    }
    assert_equal(evt.state, 0, "%u");
    for (u32 r=0; r<2; r++) {
        for (u32 i=0; i<4; i++) gf_dash_segment_event_associations_del(&seg[r][i]);
        gf_list_del(rep[r].state_seg_list);
    }
    gf_list_del(es.entries);
    gf_list_del(period.event_streams);
}
