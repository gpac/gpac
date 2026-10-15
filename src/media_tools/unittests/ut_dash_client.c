#include "tests.h"
#include "../dash_client.c"

static GF_Err dash_event(GF_DASHFileIO *dashio, GF_DASHEventType evt, s32 group_idx, GF_Err setup_error)
{
	return GF_OK;
}

// a live MPD with two representations whose timelines list segments first..last (2 s each);
// with whole_set, the AdaptationSet also lists every segment from 1 and the representations override it
static void write_mpd(const char *path, u32 first, u32 last, Bool whole_set)
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
		write_mpd(path, 11, 20, whole_set);
		assert_equal(gf_dash_open(dash, path), GF_OK, "%d");
		assert_equal(gf_dash_setup_period(dash), GF_OK, "%d");
		GF_DASH_Group *group = gf_list_get(dash->groups, 0);
		assert_true(group != NULL);
		if (group) {
			// next segment to fetch: 19, at index 8 of 11..20
			group->download_segment_index = 8;

			// the window moves on by one segment: 19 is now at index 7 of 12..21
			write_mpd(path, 12, 21, whole_set);
			assert_equal(gf_dash_update_manifest(dash), GF_OK, "%d");
			group = gf_list_get(dash->groups, 0);
			assert_equal(group->download_segment_index, 7, "%u");
		}
		gf_dash_close(dash);
		gf_dash_del(dash);
	}
	gf_file_delete(path);
}
