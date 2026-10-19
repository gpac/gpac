#include "tests.h"
#include "../dash_client.c"

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
