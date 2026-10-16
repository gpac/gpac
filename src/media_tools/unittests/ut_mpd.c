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

/*************************************/

unittest(mpd_hls_scte35_nonzero_presentation_time_offset)
{
	GF_MPD_Period period = {0};
	GF_MPD_AdaptationSet adaptation_set = {0};
	GF_MPD_Representation representation = {0};
	GF_MPD_SegmentTemplate segment_template = {0};
	GF_DASH_SegmentContext segment = {0};
	GF_MPD_EventStream event_stream = {0};
	GF_MPD_EventStreamEntry event = {0};

	period.event_streams = gf_list_new();
	event_stream.entries = gf_list_new();
	assert_true(period.event_streams != NULL);
	assert_true(event_stream.entries != NULL);

	segment_template.timescale = 90000;
	segment_template.presentation_time_offset = 8070463950ULL;
	adaptation_set.segment_template = &segment_template;
	representation.timescale = 90000;

	event_stream.timescale = 90000;
	event.presentation_time = 8076794552ULL;
	event.duration = 21780000U;
	event.id = 662;
	gf_list_add(event_stream.entries, &event);
	gf_list_add(period.event_streams, &event_stream);

	/* PTO-normalized event time is 6330602 and falls inside this segment. */
	segment.time = 6105600ULL;
	segment.dur = 417600U;

	FILE *output = tmpfile();
	assert_true(output != NULL);
	hls_insert_scte35_info(output, 0, &period, &adaptation_set, &representation, &segment);
	fflush(output);
	rewind(output);

	char text[1024] = {0};
	size_t bytes_read = fread(text, 1, sizeof(text)-1, output);
	text[bytes_read] = 0;
	assert_true(strstr(text, "#EXT-X-DATERANGE:ID=\"662-0000\"") != NULL);
	assert_true(strstr(text, "#EXT-X-CUE-OUT:242") != NULL);
	assert_equal(event.state, 1, "%u");

	fclose(output);
	gf_list_del(event_stream.entries);
	gf_list_del(period.event_streams);
}

/*************************************/

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
