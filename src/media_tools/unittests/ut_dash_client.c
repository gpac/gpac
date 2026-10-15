#include "tests.h"
#include "../dash_client.c"

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
#endif
}
