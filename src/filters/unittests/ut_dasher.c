#include "tests.h"
#include <gpac/filters.h>

Bool dasher_scte35_boundary_due(u64 packet_cts, u32 packet_timescale, u32 sap_type, u64 boundary_time);

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
GF_Err dasher_scte35_schedule_boundary(Bool, u32, u32, u64, u64, u64, GF_List **, GF_List **);
Bool dasher_scte35_next_cut(GF_List *, u64 *);
Bool dasher_scte35_consume_due(GF_List *, u64, u32, u32);

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
    assert_equal(when, 180000, LLU);
    assert_false(dasher_scte35_consume_due(a, 180000, 90000, GF_FILTER_SAP_NONE));
    assert_false(dasher_scte35_consume_due(a, 179999, 90000, GF_FILTER_SAP_1));
    assert_true(dasher_scte35_consume_due(a, 180000, 90000, GF_FILTER_SAP_1));
    assert_true(dasher_scte35_next_cut(a, &when));
    assert_equal(when, 270000, LLU);
    assert_equal(dasher_scte35_schedule_boundary(GF_TRUE, GF_STREAM_VISUAL, 100, 180000, 90000, 0, &a, &seen_a), GF_OK, "%d");
    assert_equal(gf_list_count(a), 3, "%d");
    assert_true(dasher_scte35_consume_due(a, 270000, 90000, GF_FILTER_SAP_1));
    assert_true(dasher_scte35_next_cut(a, &when));
    assert_equal(when, 360000, LLU);
    assert_true(dasher_scte35_consume_due(a, 360000, 90000, GF_FILTER_SAP_1));
    assert_true(dasher_scte35_next_cut(a, &when));
    assert_equal(when, 540000, LLU);
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

Bool dasher_ssai_boundary_reached(u64 source_cts, u32 timescale, u64 cut90k);
unittest(dasher_ssai_common_cut_clock_and_audio_sample_tolerance)
{
	const u64 cut = 6332400;
	assert_false(dasher_ssai_boundary_reached(6332399, 90000, cut));
	assert_true(dasher_ssai_boundary_reached(6332400, 90000, cut));
	assert_true(dasher_ssai_boundary_reached(3377280, 48000, cut));
	assert_false(dasher_ssai_boundary_reached(3377279, 48000, cut));
}

void dasher_scte35_prune_seen(GF_List *, u64);

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
