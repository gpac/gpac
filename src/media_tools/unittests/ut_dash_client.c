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

#ifndef GPAC_DISABLE_DASHIN
typedef struct {
	GF_DashClient *dash;
	u32 resets, period_ends, cached_segments;
} DashResetTest;

static GF_Err dash_test_event(GF_DASHFileIO *io, GF_DASHEventType event, s32 group_index, GF_Err error)
{
	DashResetTest *test = io->udta;
	if (test && (event == GF_DASH_EVENT_DESTROY_PLAYBACK)) {
		GF_DASH_Group *group = gf_list_get(test->dash->groups, 0);
		GF_MPD_Period *period = gf_list_get(test->dash->mpd->periods, 0);
		assert_not_null(group);
		assert_not_null(period);
		if (!group || !period) return GF_OK;
		/* The client can still query the old owners while retiring playback. */
		assert_true(group->period == period);
		assert_equal(period->start, (u64)0, LLU);
		assert_true(group->adaptation_set == gf_list_get(period->adaptation_sets, 0));
		assert_equal(gf_list_count(group->adaptation_set->representations), 1, "%u");
		assert_true(gf_dash_get_group_udta(test->dash, 0) == test);
		gf_dash_set_group_udta(test->dash, 0, NULL);
		test->cached_segments = group->nb_cached_segments;
		test->resets++;
	}
	if (test && (event == GF_DASH_EVENT_END_OF_PERIOD)) {
		GF_MPD_Period *period = gf_list_get(test->dash->mpd->periods, 0);
		assert_not_null(period);
		if (!period) return GF_OK;
		assert_equal(test->resets, 1, "%u");
		assert_equal(gf_list_count(test->dash->groups), 0, "%u");
		assert_equal(test->dash->dash_state, GF_DASH_STATE_SETUP, "%d");
		assert_equal(period->start, (u64)1000, LLU);
		assert_equal(group_index, 0, "%d");
		test->period_ends++;
	}
	return GF_OK;
}
#endif

unittest(dash_evicted_period_reset)
{
#ifndef GPAC_DISABLE_DASHIN
	char initial_mpd[] =
		"<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"dynamic\">"
		"<Period start=\"PT0S\"><AdaptationSet mimeType=\"video/mp4\">"
		"<Representation id=\"r\" bandwidth=\"1000\">"
		"<SegmentTemplate media=\"segment-$Number$.m4s\" duration=\"4\"/>"
		"</Representation></AdaptationSet></Period></MPD>";
	char updated_mpd[] =
		"<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"dynamic\">"
		"<Period start=\"PT1S\"><AdaptationSet mimeType=\"video/mp4\">"
		"<Representation id=\"r\" bandwidth=\"1000\">"
		"<SegmentTemplate media=\"segment-$Number$.m4s\" duration=\"4\"/>"
		"</Representation></AdaptationSet></Period></MPD>";
	char two_periods_mpd[] =
		"<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" type=\"dynamic\">"
		"<Period start=\"PT1S\"><AdaptationSet mimeType=\"video/mp4\">"
		"<Representation id=\"r\" bandwidth=\"1000\">"
		"<SegmentTemplate media=\"segment-$Number$.m4s\" duration=\"4\"/>"
		"</Representation></AdaptationSet></Period><Period start=\"PT41S\"/></MPD>";
	struct {
		const char *name;
		char *update;
		Bool buffered, expected_last_period;
	} cases[] = {
		{ .name = "eviction before media", .update = updated_mpd, .buffered = GF_FALSE, .expected_last_period = GF_TRUE },
		{ .name = "eviction with buffered media", .update = updated_mpd, .buffered = GF_TRUE, .expected_last_period = GF_TRUE },
		{ .name = "eviction with two replacement Periods", .update = two_periods_mpd, .buffered = GF_TRUE, .expected_last_period = GF_FALSE }
	};
	u32 case_index;

	/* Evict before media, with buffered media, and with two replacement Periods. */
	for (case_index=0; case_index<sizeof(cases)/sizeof(cases[0]); case_index++) {
		GF_DashClient dash = {0};
		GF_DASHFileIO io = {0};
		DashResetTest test = { .dash = &dash };
		GF_DOMParser *parser = gf_xml_dom_new();
		GF_DASH_Group *group;
		char *update = cases[case_index].update;
		GF_Err e;
		Bool cache_is_full;
		int failures_before = checks_failed;

		io.on_dash_event = dash_test_event;
		io.udta = &test;
		dash.dash_io = &io;
		dash.speed = 1;
		dash.seek_pending = -1;
		dash.dash_state = GF_DASH_STATE_RUNNING;
		dash.period_groups_setup = GF_TRUE;
		dash.mpd = gf_mpd_new();
		e = gf_xml_dom_parse_string(parser, initial_mpd);
		assert_equal(e, GF_OK, "%d");
		if (e) goto cleanup;
		e = gf_mpd_init_from_dom(gf_xml_dom_get_root(parser), dash.mpd, NULL);
		assert_equal(e, GF_OK, "%d");
		if (e) goto cleanup;
		e = gf_dash_setup_groups(&dash);
		assert_equal(e, GF_OK, "%d");
		if (e) goto cleanup;
		assert_equal(gf_list_count(dash.groups), 1, "%u");
		if (gf_list_count(dash.groups) != 1) goto cleanup;
		group = gf_list_get(dash.groups, 0);
		group->selection = GF_DASH_GROUP_SELECTED;
		gf_dash_set_group_udta(&dash, 0, &test);
		if (cases[case_index].buffered) {
			/* A buffered cache entry must be retired along with its playback owner. */
			group->nb_cached_segments = 1;
			group->cached[0].url = gf_strdup("buffered.m4s");
			group->cached[0].duration = 4000;
		}

		/* Process the refresh normally: it must stop downloading after the reset. */
		dash.pending_mpd = gf_mpd_new();
		e = gf_xml_dom_parse_string(parser, update);
		assert_equal(e, GF_OK, "%d");
		if (e) goto cleanup;
		e = gf_mpd_init_from_dom(gf_xml_dom_get_root(parser), dash.pending_mpd, NULL);
		assert_equal(e, GF_OK, "%d");
		if (e) goto cleanup;
		dash.force_mpd_update = GF_TRUE;
		e = gf_dash_process(&dash);
		assert_equal(e, GF_OK, "%d");
		assert_true(dash.pending_mpd == NULL);
		if (e || dash.pending_mpd) goto cleanup;
		assert_equal(test.resets, 1, "%u");
		assert_equal(test.period_ends, 1, "%u");
		assert_equal(test.cached_segments, (u32)cases[case_index].buffered, "%u");
		assert_equal(gf_list_count(dash.groups), 0, "%u");
		/* Avoid querying stale groups when exercising this test on unfixed code. */
		if (gf_list_count(dash.groups)) goto cleanup;
		assert_equal(dash.active_period_index, 0, "%u");
		assert_equal(dash.dash_state, GF_DASH_STATE_SETUP, "%d");
		assert_equal(dash.request_period_switch, 0, "%u");
		assert_false(dash.period_groups_setup);
		assert_false(gf_dash_in_last_period(&dash, GF_TRUE));

		/* A forced changed refresh may arrive immediately, before rebuilding. */
		dash.pending_mpd = gf_mpd_new();
		e = gf_xml_dom_parse_string(parser, update);
		assert_equal(e, GF_OK, "%d");
		if (e) goto cleanup;
		e = gf_mpd_init_from_dom(gf_xml_dom_get_root(parser), dash.pending_mpd, NULL);
		assert_equal(e, GF_OK, "%d");
		if (e) goto cleanup;
		dash.force_mpd_update = GF_TRUE;
		e = dash_check_mpd_update_and_cache(&dash, &cache_is_full);
		assert_equal(e, GF_OK, "%d");
		if (e) goto cleanup;
		assert_equal(test.resets, 1, "%u");
		assert_equal(test.period_ends, 1, "%u");
		e = gf_dash_setup_groups(&dash);
		assert_equal(e, GF_OK, "%d");
		if (e) goto cleanup;
		assert_equal(gf_list_count(dash.groups), 1, "%u");
		if (gf_list_count(dash.groups) != 1) goto cleanup;
		group = gf_list_get(dash.groups, 0);
		assert_false(group->done);
		assert_true(group->period == gf_list_get(dash.mpd->periods, 0));
		assert_equal(group->period->start, (u64)1000, LLU);
		assert_equal(group->nb_cached_segments, 0, "%u");
		/* Only the last replacement Period is eligible for final EOS. */
		dash.dash_state = GF_DASH_STATE_RUNNING;
		assert_equal(gf_dash_in_last_period(&dash, GF_TRUE), cases[case_index].expected_last_period, "%d");

cleanup:
		if (checks_failed != failures_before)
			printf("\nDASH eviction scenario: %s\n", cases[case_index].name);
		io.udta = NULL;
		gf_dash_reset_groups(&dash);
		gf_mpd_del(dash.mpd);
		gf_mpd_del(dash.pending_mpd);
		gf_xml_dom_del(parser);
	}
}
#endif

#ifndef GPAC_DISABLE_DASHIN

static const char *multicast_header(GF_DASHFileIO *io, GF_DASHFileIOSession session, const char *name)
{
	return !strcmp(name, "x-mcast-first-seg") ? io->udta : NULL;
}

#endif /* GPAC_DISABLE_DASHIN */

/* The harness registers tests even when DASH support is disabled. */
unittest(dash_multicast_template_literals)
{
#ifndef GPAC_DISABLE_DASHIN
	const struct {
		const char *media;
		const char *first_segment;
		u32 expected_offset_ms;
	} cases[] = {
		/* Percent directives in a URL are literal data, never scanf conversions. */
		{"%s0123456789seg-$Number$.m4s", "%s0123456789seg-3.m4s", 6000},
		{"seg-$Number$.m4s%n", "seg-3.m4s%n", 6000},
		{"seg-$Number$.m4s", "seg-3.m4s", 6000},
		{"seg-$Number%05d$.m4s", "seg-00003.m4s", 6000},
		/* The padding width keeps a numeric suffix out of the segment number. */
		{"seg-$Number%05d$123.m4s", "seg-00003123.m4s", 6000},
		{"seg-$Time$.m4s", "seg-6.m4s", 8000},
	};
	GF_MPD mpd = {0};
	GF_MPD_Period period = {0};
	GF_MPD_AdaptationSet adaptation_set = {0};
	GF_MPD_Representation representation = {0};
	GF_MPD_SegmentTemplate segment_template = {0};
	GF_DASHFileIO io = {0};
	GF_DashClient dash = {0};
	GF_DASH_Group group = {0};
	u32 i;

	/* A static manifest stops immediately after multicast clock setup. */
	mpd.type = GF_MPD_TYPE_STATIC;
	mpd.availabilityStartTime = 1000;
	mpd.periods = gf_list_new();
	period.adaptation_sets = gf_list_new();
	adaptation_set.representations = gf_list_new();
	assert_true(mpd.periods != NULL);
	assert_true(period.adaptation_sets != NULL);
	assert_true(adaptation_set.representations != NULL);
	gf_list_add(mpd.periods, &period);
	gf_list_add(period.adaptation_sets, &adaptation_set);
	gf_list_add(adaptation_set.representations, &representation);
	representation.segment_template = &segment_template;
	segment_template.timescale = 1;
	segment_template.duration = 2;
	segment_template.start_number = 1;
	io.get_header_value = multicast_header;
	dash.dash_io = &io;
	dash.mpd = &mpd;
	dash.base_url = "http://localhost/gmcast/test/manifest.mpd";
	group.dash = &dash;
	group.adaptation_set = &adaptation_set;

	for (i=0; i<sizeof(cases)/sizeof(cases[0]); i++) {
		segment_template.media = gf_strdup(cases[i].media);
		assert_true(segment_template.media != NULL);
		io.udta = (void *) cases[i].first_segment;
		dash.mcast_clock_state = GF_DASH_MCAST_INIT;
		dash.utc_drift_estimate = 0;
		dash.mcast_skip_segments_ms = 0;
		group.timeline_setup = GF_FALSE;
		gf_dash_group_timeline_setup_single(&mpd, &group, 10000);
		assert_equal(dash.mcast_clock_state, GF_DASH_MCAST_SYNC_SOURCE, "%u");
		assert_equal(dash.mcast_skip_segments_ms, cases[i].expected_offset_ms, "%u");
		assert_true(group.timeline_setup);
		gf_free(segment_template.media);
	}

	gf_list_del(adaptation_set.representations);
	gf_list_del(period.adaptation_sets);
	gf_list_del(mpd.periods);
#endif /* GPAC_DISABLE_DASHIN */
}
