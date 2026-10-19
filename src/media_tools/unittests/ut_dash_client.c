#include "tests.h"
#include "../dash_client.c"

static GF_Err dash_event(GF_DASHFileIO *dashio, GF_DASHEventType evt, s32 group_idx, GF_Err setup_error)
{
	return GF_OK;
}

// a live MPD with 2 s segments, available since ast_ago_s seconds, with a 12 s suggested presentation delay
static void write_mpd(const char *path, u32 ast_ago_s)
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
	write_mpd(path, 300);
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
