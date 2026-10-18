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
#endif
}

unittest(dasher_ssr_live_edge_fragment_count)
{
#ifndef GPAC_DISABLE_DASHER
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

#endif // GPAC_DISABLE_DASHER
