#include "tests.h"
#include <gpac/filters.h>

/* Kept visible with GF_STATIC specifically for source-level regression tests. */
u64 dasher_scte35_translate_event(u64, u32, u64, u64, s64, u64, Bool, u32);
Bool dasher_scte35_transition_at_segment(u64, u64, Bool, u64);

unittest(dasher_scte35_f16_independent_rendition_origins)
{
    /* F16 source SCTE event PTS, independent video/audio media origins.
     * Both next physical boundaries follow the same 90k event. */
    const u64 event90k = 8076794552ULL;
    const u64 vstart = 8076796350ULL;
    const u64 astart = 8076797417ULL;
    const u64 vfirst = 8070463950ULL;
    const u64 afirst = 8070192137ULL;
    const u64 vevt = dasher_scte35_translate_event(event90k, 90000, vfirst, vfirst, 0, 0, GF_FALSE, 90000);
    const u64 aevt = dasher_scte35_translate_event(event90k, 90000, afirst, afirst, 0, 0, GF_FALSE, 90000);
    const u64 vnext = vstart-vfirst, anext = astart-afirst;
    assert_equal(vevt, (u64)6330602, LLU);
    assert_equal(aevt, (u64)6602415, LLU);
    assert_true(dasher_scte35_transition_at_segment(vevt, vnext, GF_TRUE, vnext-228000));
    assert_true(dasher_scte35_transition_at_segment(aevt, anext, GF_TRUE, anext-226800));
    assert_false(dasher_scte35_transition_at_segment(aevt, anext-226800, GF_TRUE, anext-453600));
}

unittest(dasher_scte35_mpd_scale_and_no_duplicate_boundary)
{
    const u64 evt = dasher_scte35_translate_event(8076794552ULL, 90000,
        8070192137ULL, 8070192137ULL, 0, 0, GF_FALSE, 1000);
    assert_equal(evt, (u64)73360, LLU);
    assert_true(dasher_scte35_transition_at_segment(100, 100, GF_TRUE, 90));
    assert_false(dasher_scte35_transition_at_segment(100, 101, GF_TRUE, 100));
    assert_true(dasher_scte35_transition_at_segment(10, 12, GF_FALSE, 0));
}
