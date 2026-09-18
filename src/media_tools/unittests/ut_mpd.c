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
