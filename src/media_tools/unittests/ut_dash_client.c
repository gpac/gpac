#include "tests.h"
#include "../dash_client.c"

#ifndef GPAC_DISABLE_DASHIN

static GF_Err dash_event(GF_DASHFileIO *dashio, GF_DASHEventType evt, s32 group_idx, GF_Err setup_error)
{
	return GF_OK;
}

// a live MPD with 2 s segments, available since ast_ago_s seconds, with a 12 s suggested presentation delay
static void write_mpd_spd(const char *path, u32 ast_ago_s)
{
	char ast[40];
	time_t t = (time_t) (gf_net_get_utc() / 1000) - ast_ago_s;
	strftime(ast, sizeof(ast), "%Y-%m-%dT%H:%M:%SZ", gf_gmtime(&t));
	FILE *f = gf_fopen(path, "w");
	gf_fprintf(f, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"dynamic\" profiles=\"urn:mpeg:dash:profile:isoff-live:2011\" "
		"availabilityStartTime=\"%s\" minimumUpdatePeriod=\"PT2S\" timeShiftBufferDepth=\"PT60S\" "
		"suggestedPresentationDelay=\"PT12S\" minBufferTime=\"PT2S\">"
		"<Period id=\"1\" start=\"PT0S\"><AdaptationSet mimeType=\"video/mp4\" codecs=\"avc1.64001E\" segmentAlignment=\"true\">"
		"<SegmentTemplate media=\"$RepresentationID$_$Number$.m4s\" timescale=\"1\" duration=\"2\" startNumber=\"1\"/>"
		"<Representation id=\"a\" bandwidth=\"100000\"/></AdaptationSet></Period></MPD>", ast);
	gf_fclose(f);
}

unittest(dash_presentation_delay_start_point)
{
	const char *path = "ut-dash-spd.mpd";
	GF_DASHFileIO io = {0};
	io.on_dash_event = dash_event;

	GF_DashClient *dash = gf_dash_new(&io, 0, 0, GF_FALSE, GF_FALSE, GF_DASH_SELECT_QUALITY_LOWEST, 0);
	// honour the MPD's suggestedPresentationDelay (dashin spd=0)
	gf_dash_set_suggested_presentation_delay(dash, 0);
	write_mpd_spd(path, 300);
	assert_equal(gf_dash_open(dash, path), GF_OK, "%d");
	assert_equal(gf_dash_setup_period(dash), GF_OK, "%d");
	GF_DASH_Group *group = gf_list_get(dash->groups, 0);
	assert_true(group != NULL);
	if (group) {
		gf_dash_group_timeline_setup(dash->mpd, group, 0);
		u64 now = gf_net_get_utc();
		Double live = (Double) (now - dash->mpd->availabilityStartTime) / 1000;
		Double dur, start = gf_dash_get_segment_start_time(group, &dur);
		u64 available = gf_dash_get_segment_availability_start_time(dash->mpd, group, group->download_segment_index, NULL);

		// playback starts 12 s behind live, in the segment holding that time
		assert_true((start <= live - 12) && (live - 12 < start + dur));
		// that segment ended about 10 s ago: it is available now
		assert_true(available <= now);
	}
	gf_dash_close(dash);
	gf_dash_del(dash);
	gf_file_delete(path);
}

/////////////////////////////////////////////////////////

// a live MPD with two representations whose timelines list segments first..last (2 s each);
// with whole_set, the AdaptationSet also lists every segment from 1 and the representations override it
static void write_mpd_manifest_timeline_position(const char *path, u32 first, u32 last, Bool whole_set)
{
	char tl[256];
	FILE *f = gf_fopen(path, "w");
	gf_fprintf(f, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"dynamic\" profiles=\"urn:mpeg:dash:profile:isoff-live:2011\" "
		"availabilityStartTime=\"1970-01-01T00:00:00Z\" minimumUpdatePeriod=\"PT2S\" minBufferTime=\"PT2S\">"
		"<Period id=\"1\" start=\"PT0S\"><AdaptationSet mimeType=\"video/mp4\" codecs=\"avc1.64001E\" segmentAlignment=\"true\">");
	if (whole_set)
		gf_fprintf(f, "<SegmentTemplate media=\"$RepresentationID$_$Time$.m4s\" timescale=\"1\">"
			"<SegmentTimeline><S t=\"0\" d=\"2\" r=\"%u\"/></SegmentTimeline></SegmentTemplate>", last-1);
	snprintf(tl, sizeof(tl), "<SegmentTemplate media=\"$RepresentationID$_$Time$.m4s\" timescale=\"1\">"
		"<SegmentTimeline><S t=\"%u\" d=\"2\" r=\"%u\"/></SegmentTimeline></SegmentTemplate>", (first-1)*2, last-first);
	gf_fprintf(f, "<Representation id=\"a\" bandwidth=\"100000\">%s</Representation>"
		"<Representation id=\"b\" bandwidth=\"200000\">%s</Representation>"
		"</AdaptationSet></Period></MPD>", tl, tl);
	gf_fclose(f);
}

unittest(dash_update_manifest_timeline_position)
{
	const char *path = "ut-dash-update.mpd";
	GF_DASHFileIO io = {0};
	u32 whole_set;
	io.on_dash_event = dash_event;

	for (whole_set=0; whole_set<2; whole_set++) {
		GF_DashClient *dash = gf_dash_new(&io, 0, 0, GF_FALSE, GF_FALSE, GF_DASH_SELECT_QUALITY_LOWEST, 0);
		write_mpd_manifest_timeline_position(path, 11, 20, whole_set);
		assert_equal(gf_dash_open(dash, path), GF_OK, "%d");
		assert_equal(gf_dash_setup_period(dash), GF_OK, "%d");
		GF_DASH_Group *group = gf_list_get(dash->groups, 0);
		assert_true(group != NULL);
		if (group) {
			// next segment to fetch: 19, at index 8 of 11..20
			group->download_segment_index = 8;

			// the window moves on by one segment: 19 is now at index 7 of 12..21
			write_mpd_manifest_timeline_position(path, 12, 21, whole_set);
			assert_equal(gf_dash_update_manifest(dash), GF_OK, "%d");
			group = gf_list_get(dash->groups, 0);
			assert_equal(group->download_segment_index, 7, "%u");
		}
		gf_dash_close(dash);
		gf_dash_del(dash);
	}
	gf_file_delete(path);
}

#endif //GPAC_DISABLE_DASHIN
