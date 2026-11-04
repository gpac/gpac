#ifndef GPAC_DISABLE_DASHER

#include "tests.h"

#define GPAC_DASHER_UNIT_TEST
#include "../dasher.c"

//ssr live edge must advertise part count before it is known
unittest(dasher_ssr_live_edge_estimate)
{
	static const struct {
		Bool ssr, previous, previous_edge;
		u32 previous_duration, previous_parts;
		GF_Fraction cdur;
		u32 expected_parts;
	} cases[] = {
		{GF_TRUE, GF_TRUE , GF_FALSE, 2000, 7, {1, 2}, 7}, /* reuse actual count */
		{GF_TRUE, GF_FALSE, GF_FALSE,    0, 0, {1, 2}, 4}, /* first segment */
		{GF_TRUE, GF_FALSE, GF_FALSE,    0, 0, {3, 4}, 3}, /* round up */
		{GF_TRUE, GF_TRUE , GF_FALSE, 1000, 7, {1, 2}, 4}, /* duration mismatch */
		{GF_TRUE, GF_TRUE , GF_TRUE , 2000, 7, {1, 2}, 4}, /* previous edge */
		{GF_TRUE, GF_TRUE , GF_FALSE, 2000, 0, {1, 2}, 4}, /* unknown count */
		{GF_FALSE, GF_TRUE, GF_FALSE, 2000, 7, {1, 2}, 0},
		{GF_TRUE, GF_FALSE, GF_FALSE,    0, 0, {0, 1}, 0},
		{GF_TRUE, GF_FALSE, GF_FALSE,    0, 0, {1, 0}, 0},
		{GF_TRUE, GF_FALSE, GF_FALSE,    0, 0, {1, 10000}, 0} /* rescaled to zero */
	};

	u32 i;
	for (i=0; i<GF_ARRAY_LENGTH(cases); i++) {
		GF_DasherCtx ctx = {0};
		GF_DashStream ds = {0};
		GF_MPD_AdaptationSet set = {0};
		GF_MPD_Representation rep = {0};
		GF_MPD_SegmentTemplate tpl = {0};
		GF_MPD_SegmentTimeline tl = {0};
		GF_MPD_SegmentTimelineEntry prev = {0}, *edge;
		tl.entries = gf_list_new();
		set.representations = gf_list_new();
		gf_list_add(set.representations, &rep);
		set.ssr_mode = cases[i].ssr;
		rep.segment_template = &tpl;
		tpl.segment_timeline = &tl;
		ctx.tpl = GF_TRUE;
		ctx.cdur = cases[i].cdur;
		ds.set = &set;
		ds.rep = &rep;
		ds.stl = GF_TRUE;
		ds.timescale = ds.mpd_timescale = 1000;
		ds.dash_dur = (GF_Fraction) {2, 1};
		ds.seg_start_time = 2000;
		if (cases[i].previous) {
			prev.duration = cases[i].previous_duration;
			prev.nb_parts = cases[i].previous_parts;
			prev.is_ll_edge = cases[i].previous_edge;
			gf_list_add(tl.entries, &prev);
		}

		dasher_insert_timeline_entry(&ctx, &ds, GF_TRUE);
		assert_equal(gf_list_count(tl.entries), (cases[i].previous ? 2u : 1u), "%u");
		edge = gf_list_last(tl.entries);
		assert_not_null(edge);
		if (edge && edge != &prev) {
			assert_true(edge->is_ll_edge);
			assert_true(ds.last_stl_is_ll);
			assert_equal(edge->start_time, (u64)2000, LLU);
			assert_equal(edge->duration, 2000u, "%u");
			assert_equal(edge->nb_parts, cases[i].expected_parts, "%u");
			gf_free(edge);
		}
		gf_list_del(tl.entries);
		gf_list_del(set.representations);
	}
}

unittest(dasher_ssr_live_edge_fragment_count)
{
	u32 i;
	for (i=0; i<2; i++) {
		GF_DasherCtx ctx = {0};
		GF_DashStream ds = {0};
		GF_DASH_SegmentContext sctx = {0};
		GF_MPD_AdaptationSet set = {0};
		GF_MPD_SegmentTemplate tpl = {0};
		GF_MPD_SegmentTimeline tl = {0};
		GF_MPD_SegmentTimelineEntry completed = {0}, edge = {0}, later = {0};
		ctx.tpl = GF_TRUE;
		ds.set = &set;
		set.ssr_mode = GF_TRUE;
		set.segment_template = &tpl;
		tpl.segment_timeline = &tl;
		tl.entries = gf_list_new();
		completed.duration = 2000;
		edge.start_time = 2000;
		edge.duration = i ? 1000 : 2000;
		edge.is_ll_edge = GF_TRUE;
		edge.nb_parts = 4;
		later.start_time = 4000;
		later.duration = 2000;
		gf_list_add(tl.entries, &completed);
		gf_list_add(tl.entries, &edge);
		/* Other representations can insert entries after the live edge. */
		gf_list_add(tl.entries, &later);
		sctx.stl_start = completed.start_time;
		sctx.nb_frags = 6;

		dasher_set_timeline_parts(&ctx, &ds, &sctx);
		assert_equal(completed.nb_parts, 6u, "%u");
		assert_equal(edge.nb_parts, (i ? 4u : 6u), "%u");
		assert_true(edge.is_ll_edge);
		assert_equal(later.nb_parts, 0u, "%u");
		assert_equal(gf_list_count(tl.entries), 3u, "%u");
		gf_list_del(tl.entries);
	}
}

unittest(dasher_scte35_boundary_uses_first_sap_at_or_after_splice)
{
	const u64 splice_time_90k = 6330602;

	/* A predictive packet after the cue cannot start an independently decodable
	 * HLS segment. */
	assert_false(dasher_scte35_boundary_due(6332400, 90000, GF_FILTER_SAP_NONE, splice_time_90k));

	/* A SAP before the requested splice point is still too early. */
	assert_false(dasher_scte35_boundary_due(6330000, 90000, GF_FILTER_SAP_1, splice_time_90k));

	/* Exact and slightly-later SAPs are valid boundaries. The latter models the
	 * real broadcast sample where the cue is about 20 ms before the IDR. */
	assert_true(dasher_scte35_boundary_due(6330602, 90000, GF_FILTER_SAP_1, splice_time_90k));
	assert_true(dasher_scte35_boundary_due(6332400, 90000, GF_FILTER_SAP_1, splice_time_90k));

	/* Comparison is timescale-safe. 70.36 s in a 1 kHz stream is after the
	 * 70.340022 s SCTE splice time. */
	assert_true(dasher_scte35_boundary_due(70360, 1000, GF_FILTER_SAP_1, splice_time_90k));
	assert_false(dasher_scte35_boundary_due(70300, 1000, GF_FILTER_SAP_1, splice_time_90k));
}

/* Multiple future SCTE breaks must be queued, not discarded while the
 * first break's start/end boundaries are pending. */
static void test_scte35_del_list(GF_List *list)
{
    if (!list) return;
    while (gf_list_count(list)) gf_free(gf_list_pop_front(list));
    gf_list_del(list);
}

unittest(dasher_scte35_multi_break_ordering_dedup_and_renditions)
{
    GF_List *a=NULL, *seen_a=NULL, *b=NULL, *seen_b=NULL;
    u64 when = 0;
    /* Both events are announced before the first splice RAP. */
    assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL, 100, 180000, 90000, 0, &a, &seen_a), GF_OK, "%d");
    assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL, 101, 360000, 180000, 0, &a, &seen_a), GF_OK, "%d");
    assert_equal(gf_list_count(a), 4, "%d");
    /* Events may arrive out of timestamp order on another representation. */
    assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL, 101, 360000, 180000, 0, &b, &seen_b), GF_OK, "%d");
    assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL, 100, 180000, 90000, 0, &b, &seen_b), GF_OK, "%d");
    assert_equal(gf_list_count(b), 4, "%d");
    /* Repeated event, even after its start has been consumed, cannot re-arm. */
    assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL, 100, 180000, 90000, 0, &a, &seen_a), GF_OK, "%d");
    assert_equal(gf_list_count(a), 4, "%d");
    assert_true(dasher_scte35_next_cut(a, &when));
    assert_equal(when, (u64)180000, LLU);
    assert_false(dasher_scte35_consume_due(a, 180000, 90000, GF_FILTER_SAP_NONE));
    assert_false(dasher_scte35_consume_due(a, 179999, 90000, GF_FILTER_SAP_1));
    assert_true(dasher_scte35_consume_due(a, 180000, 90000, GF_FILTER_SAP_1));
    assert_true(dasher_scte35_next_cut(a, &when));
    assert_equal(when, (u64)270000, LLU);
    assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL, 100, 180000, 90000, 0, &a, &seen_a), GF_OK, "%d");
    assert_equal(gf_list_count(a), 3, "%d");
    assert_true(dasher_scte35_consume_due(a, 270000, 90000, GF_FILTER_SAP_1));
    assert_true(dasher_scte35_next_cut(a, &when));
    assert_equal(when, (u64)360000, LLU);
    assert_true(dasher_scte35_consume_due(a, 360000, 90000, GF_FILTER_SAP_1));
    assert_true(dasher_scte35_next_cut(a, &when));
    assert_equal(when, (u64)540000, LLU);
    assert_true(dasher_scte35_consume_due(a, 540000, 90000, GF_FILTER_SAP_1));
    assert_false(dasher_scte35_next_cut(a, &when));

    /* If several transitions occur before the same SAP, only one cut. */
    assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL, 102, 630000, 5000, 0, &a, &seen_a), GF_OK, "%d");
    assert_true(dasher_scte35_consume_due(a, 640000, 90000, GF_FILTER_SAP_1));
    assert_false(dasher_scte35_next_cut(a, &when));
    /* Audio never independently schedules video RAP cuts. */
    assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_AUDIO, 100, 180000, 90000, 0, &a, &seen_a), GF_OK, "%d");
    assert_false(dasher_scte35_next_cut(a, &when));
    test_scte35_del_list(a); test_scte35_del_list(seen_a);
    test_scte35_del_list(b); test_scte35_del_list(seen_b);
}

unittest(dasher_ssai_common_cut_clock_and_audio_sample_tolerance)
{
	const u64 cut = 6332400;
	assert_false(dasher_ssai_boundary_reached(6332399, 90000, cut));
	assert_true(dasher_ssai_boundary_reached(6332400, 90000, cut));
	assert_true(dasher_ssai_boundary_reached(3377280, 48000, cut));
	assert_false(dasher_ssai_boundary_reached(3377279, 48000, cut));
}

unittest(dasher_scte35_live_history_bounded_and_stale_replay)
{
    GF_List *pending=NULL, *seen=NULL;
    u64 watermark=0;
    u32 i;
    /* Long-lived channel: after each completed break, history is reclaimed. */
    for (i=0; i<4096; i++) {
        u64 at = 180000ULL + 360000ULL*i;
        assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL,
            i+1, at, 90000, watermark, &pending, &seen), GF_OK, "%d");
        assert_equal(gf_list_count(pending), 2, "%u");
        assert_true(dasher_scte35_consume_due(pending, at, 90000, GF_FILTER_SAP_1));
        watermark=at;
        dasher_scte35_prune_seen(seen, watermark);
        assert_equal(gf_list_count(seen), 1, "%u");
        assert_true(dasher_scte35_consume_due(pending, at+90000, 90000, GF_FILTER_SAP_1));
        watermark=at+90000;
        dasher_scte35_prune_seen(seen, watermark);
        assert_equal(gf_list_count(pending), 0, "%u");
        assert_equal(gf_list_count(seen), 0, "%u");
        /* Repeated copies arriving after retirement must not re-arm. */
        assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL,
            i+1, at, 90000, watermark, &pending, &seen), GF_OK, "%d");
        assert_equal(gf_list_count(pending), 0, "%u");
    }
    test_scte35_del_list(pending);
    test_scte35_del_list(seen);
}

#endif // GPAC_DISABLE_DASHER
