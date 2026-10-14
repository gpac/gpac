#include "tests.h"
#include "../mpd.c"

// functions used by mpd.c, needed for linking
#include "../m3u8.c"
void gf_xml_dump_string(FILE* file, const char *before, const char *str, const char *after) {}

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

	/* PTO-normalized event time is 6330602; CUE-OUT belongs on the first
	 * segment starting at or after that splice point. */
	segment.time = 6523200ULL;
	segment.dur = 417600U;

	FILE *output = tmpfile();
	assert_true(output != NULL);
	hls_insert_scte35_info(output, 0, &period, &adaptation_set, &representation, &segment, GF_FALSE);
	fflush(output);
	rewind(output);

	char text[1024] = {0};
	size_t bytes_read = fread(text, 1, sizeof(text)-1, output);
	text[bytes_read] = 0;
	assert_true(strstr(text, "#EXT-X-DATERANGE:ID=\"662-0000\"") == NULL);
	assert_true(strstr(text, "#EXT-X-CUE-OUT:242") != NULL);
	assert_equal(event.state, 1, "%u");

	fclose(output);
	gf_list_del(event_stream.entries);
	gf_list_del(period.event_streams);
}

/*************************************/

unittest(mpd_hls_scte35_cues_follow_segment_boundaries)
{
	GF_MPD_Period period = {0};
	GF_MPD_AdaptationSet adaptation_set = {0};
	GF_MPD_Representation representation = {0};
	GF_DASH_SegmentContext segment = {0};
	GF_MPD_EventStream event_stream = {0};
	GF_MPD_EventStreamEntry event = {0};
	char text[1024];
	size_t bytes_read;
	FILE *output;

	period.event_streams = gf_list_new();
	event_stream.entries = gf_list_new();
	assert_true(period.event_streams != NULL);
	assert_true(event_stream.entries != NULL);

	representation.timescale = 1000;
	event_stream.timescale = 1000;
	event.presentation_time = 10300;
	event.duration = 5000;
	event.id = 7;
	gf_list_add(event_stream.entries, &event);
	gf_list_add(period.event_streams, &event_stream);

	/* The splice occurs inside this segment. A cue before this segment would
	 * start replacement 300 ms too early, so no marker belongs here. */
	segment.time = 10000;
	segment.dur = 4000;
	output = tmpfile();
	assert_true(output != NULL);
	hls_insert_scte35_info(output, 0, &period, &adaptation_set, &representation, &segment, GF_FALSE);
	fflush(output);
	rewind(output);
	memset(text, 0, sizeof(text));
	bytes_read = fread(text, 1, sizeof(text)-1, output);
	text[bytes_read] = 0;
	assert_true(strstr(text, "#EXT-X-DATERANGE:ID=\"7-0000\"") != NULL);
	assert_true(strstr(text, "#EXT-X-CUE-OUT") == NULL);
	assert_equal(event.state, 0, "%u");
	fclose(output);

	/* CUE-OUT is emitted immediately before the first segment whose start is
	 * at or after the splice point. */
	segment.time = 14000;
	segment.dur = 4000;
	output = tmpfile();
	assert_true(output != NULL);
	hls_insert_scte35_info(output, 0, &period, &adaptation_set, &representation, &segment, GF_FALSE);
	fflush(output);
	rewind(output);
	memset(text, 0, sizeof(text));
	bytes_read = fread(text, 1, sizeof(text)-1, output);
	text[bytes_read] = 0;
	assert_true(strstr(text, "#EXT-X-DATERANGE:ID=\"7-0000\"") == NULL);
	assert_true(strstr(text, "#EXT-X-CUE-OUT:5") != NULL);
	assert_true(strstr(text, "#EXT-X-CUE-IN") == NULL);
	assert_equal(event.state, 1, "%u");
	fclose(output);

	/* The break ends at 15.3 s, inside the same 14-18 s segment. Do not put
	 * CUE-IN before media that still belongs to the break. */
	output = tmpfile();
	assert_true(output != NULL);
	hls_insert_scte35_info(output, 0, &period, &adaptation_set, &representation, &segment, GF_FALSE);
	fflush(output);
	rewind(output);
	memset(text, 0, sizeof(text));
	bytes_read = fread(text, 1, sizeof(text)-1, output);
	text[bytes_read] = 0;
	assert_true(strstr(text, "#EXT-X-CUE-IN") == NULL);
	assert_equal(event.state, 1, "%u");
	fclose(output);

	/* CUE-IN is emitted before the first segment starting at or after the
	 * return point. */
	segment.time = 18000;
	segment.dur = 4000;
	output = tmpfile();
	assert_true(output != NULL);
	hls_insert_scte35_info(output, 0, &period, &adaptation_set, &representation, &segment, GF_FALSE);
	fflush(output);
	rewind(output);
	memset(text, 0, sizeof(text));
	bytes_read = fread(text, 1, sizeof(text)-1, output);
	text[bytes_read] = 0;
	assert_true(strstr(text, "#EXT-X-CUE-IN") != NULL);
	assert_equal(event.state, 0, "%u");
	fclose(output);

	gf_list_del(event_stream.entries);
	gf_list_del(period.event_streams);
}


/*************************************/

unittest(mpd_hls_scte35_metadata_and_finite_return_policy)
{
	GF_MPD_Period period = {0};
	GF_MPD_AdaptationSet as = {0};
	GF_MPD_Representation rep = {0};
	GF_MPD_EventStream es = {0};
	GF_MPD_EventStreamEntry event = {0};
	GF_DASH_SegmentContext segments[3] = {{0}};
	FILE *out;
	char text[4096] = {0};
	size_t n;

	period.event_streams = gf_list_new();
	es.entries = gf_list_new();
	rep.state_seg_list = gf_list_new();
	assert_true(period.event_streams && es.entries && rep.state_seg_list);
	rep.timescale = es.timescale = 1000;
	event.id = 100;
	event.presentation_time = 1920;
	event.duration = 1000;
	gf_list_add(es.entries, &event);
	gf_list_add(period.event_streams, &es);
	segments[0].time = 0; segments[0].dur = 6000; segments[0].filename = "seg1.m4s";
	segments[1].time = 6000; segments[1].dur = 960; segments[1].filename = "seg2.m4s";
	gf_list_add(rep.state_seg_list, &segments[0]);
	gf_list_add(rep.state_seg_list, &segments[1]);

	/* Short event entirely within one long segment: keep SCTE metadata but
	 * never fabricate an ad opportunity or leave a dangling OUT at ENDLIST. */
	out = tmpfile(); assert_true(out != NULL);
	hls_insert_scte35_info(out, 0, &period, &as, &rep, &segments[0], GF_TRUE);
	fflush(out); rewind(out);
	n = fread(text, 1, sizeof(text)-1, out); text[n] = 0;
	assert_true(strstr(text, "#EXT-X-DATERANGE:ID=\"100-0000\"") != NULL);
	assert_true(strstr(text, "#EXT-X-CUE-OUT") == NULL);
	assert_true(strstr(text, "#EXT-X-CUE-IN") == NULL);
	assert_equal(event.state, 0, "%u");
	fclose(out);

	/* A short event with an actual start AND return segment boundary is a
	 * valid replacement opportunity on a finalized playlist. */
	segments[0].dur = 2000;
	segments[1].time = 2000; segments[1].dur = 1000;
	segments[2].time = 3000; segments[2].dur = 1000; segments[2].filename = "seg3.m4s";
	gf_list_add(rep.state_seg_list, &segments[2]);
	out = tmpfile(); assert_true(out != NULL);
	hls_insert_scte35_info(out, 0, &period, &as, &rep, &segments[1], GF_TRUE);
	assert_true(hls_scte35_has_return_boundary(&rep, 2920));
	fflush(out); rewind(out); memset(text, 0, sizeof(text));
	n = fread(text, 1, sizeof(text)-1, out); text[n] = 0;
	assert_true(strstr(text, "#EXT-X-CUE-OUT:1") != NULL);
	assert_equal(event.state, 1, "%u");
	fclose(out);
	out = tmpfile(); assert_true(out != NULL);
	hls_insert_scte35_info(out, 0, &period, &as, &rep, &segments[2], GF_TRUE);
	fflush(out); rewind(out); memset(text, 0, sizeof(text));
	n = fread(text, 1, sizeof(text)-1, out); text[n] = 0;
	assert_true(strstr(text, "#EXT-X-CUE-IN") != NULL);
	fclose(out);

	/* A rolling live playlist beginning mid-break retains DATERANGE. */
	rep.tsb_first_entry = 1;
	event.state = 0;
	out = tmpfile(); assert_true(out != NULL);
	hls_insert_scte35_info(out, 0, &period, &as, &rep, &segments[1], GF_FALSE);
	fflush(out); rewind(out); memset(text, 0, sizeof(text));
	n = fread(text, 1, sizeof(text)-1, out); text[n] = 0;
	assert_true(strstr(text, "#EXT-X-DATERANGE:ID=\"100-0000\"") != NULL);
	assert_true(strstr(text, "#EXT-X-CUE-OUT:1") != NULL);
	fclose(out);

	gf_list_del(rep.state_seg_list);
	gf_list_del(es.entries);
	gf_list_del(period.event_streams);
}
