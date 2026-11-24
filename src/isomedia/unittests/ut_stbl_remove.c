#include "tests.h"
#include <gpac/internal/isomedia_dev.h>

unittest(stts_remove_oversized_run)
{
#if !defined(GPAC_DISABLE_ISOM) \
    && (!defined(GPAC_DISABLE_ISOM_WRITE) || !defined(GPAC_DISABLE_ISOM_FRAGMENTS))
	/* The CLI trigger has three samples in stsz but 259 in its stts run. */
	GF_SttsEntry entry = { .sampleCount = 259, .sampleDelta = 10 };
	GF_TimeToSampleBox stts = { .nb_entries = 1, .alloc_size = 1, .entries = &entry };
	GF_SampleSizeBox stsz = { .sampleCount = 3 };
	GF_SampleTableBox stbl = { .TimeToSample = &stts, .SampleSize = &stsz };
	GF_Err e = stbl_RemoveDTS(&stbl, 2, 1, 10);

	assert_equal(e, GF_ISOM_INVALID_FILE, "%d");
	assert_equal(entry.sampleCount, 259, "%u");
	assert_equal(stts.nb_entries, 1, "%u");
	assert_equal(stsz.sampleCount, 3, "%u");
#endif
}

unittest(stts_remove_inconsistent_totals)
{
#if !defined(GPAC_DISABLE_ISOM) \
    && (!defined(GPAC_DISABLE_ISOM_WRITE) || !defined(GPAC_DISABLE_ISOM_FRAGMENTS))
	u32 i, nb_samples;
	const u32 run_counts[][2] = {
		{ 2, 2 }, /* individually bounded runs whose sum exceeds stsz */
		{ 1, 1 }, /* not enough DTS values to reconstruct the remaining samples */
	};

	for (i=0; i<GF_ARRAY_LENGTH(run_counts); i++) {
		for (nb_samples=1; nb_samples<=2; nb_samples++) {
			GF_SttsEntry entries[2] = {
				{ .sampleCount = run_counts[i][0], .sampleDelta = 10 },
				{ .sampleCount = run_counts[i][1], .sampleDelta = 20 },
			};
			GF_TimeToSampleBox stts = {
				.nb_entries = 2, .alloc_size = 2, .entries = entries,
				.cumulated_start_dts = 100, .w_currentSampleNum = 3,
				.r_FirstSampleInEntry = 1, .r_CurrentDTS = 10,
			};
			GF_TimeToSampleBox original_stts = stts;
			GF_SttsEntry original_entries[2];
			GF_SampleSizeBox stsz = { .sampleCount = 3 };
			GF_SampleTableBox stbl = { .TimeToSample = &stts, .SampleSize = &stsz };
			GF_Err e;

			memcpy(original_entries, entries, sizeof(entries));
			e = stbl_RemoveDTS(&stbl, nb_samples==1 ? 2 : 1, nb_samples, 10);
			assert_equal(e, GF_ISOM_INVALID_FILE, "%d");
			/* A failed expansion must not publish partial timing or cache edits. */
			assert_equal_mem(&stts, &original_stts, sizeof(stts));
			assert_equal_mem(entries, original_entries, sizeof(entries));
			assert_equal(stsz.sampleCount, 3, "%u");
		}
	}
#endif
}

unittest(stts_remove_valid_samples)
{
#if !defined(GPAC_DISABLE_ISOM) \
    && (!defined(GPAC_DISABLE_ISOM_WRITE) || !defined(GPAC_DISABLE_ISOM_FRAGMENTS))
	u32 i;
	const struct {
		u32 sample_count, sample_number, remove_count;
		u32 remaining_count, remaining_delta;
		u64 removed_duration;
	} cases[] = {
		{ 3, 1, 1, 2, 10, 0 }, /* first sample */
		{ 3, 2, 1, 2, 20, 0 }, /* middle sample: preserve the gap */
		{ 3, 3, 1, 2, 10, 0 }, /* last sample fast path */
		{ 1, 1, 1, 0,  0, 0 }, /* only sample fast path */
		{ 3, 1, 2, 1, 10, 20 }, /* bulk prefix, one sample left */
		{ 3, 1, 3, 0,  0, 30 }, /* bulk prefix, no samples left */
	};

	for (i=0; i<GF_ARRAY_LENGTH(cases); i++) {
		GF_SttsEntry entry = { .sampleCount = cases[i].sample_count, .sampleDelta = 10 };
		GF_TimeToSampleBox stts = {
			.nb_entries = 1, .alloc_size = 1, .entries = &entry,
			.cumulated_start_dts = 100,
		};
		GF_SampleSizeBox stsz = { .sampleCount = cases[i].sample_count };
		GF_SampleTableBox stbl = { .TimeToSample = &stts, .SampleSize = &stsz };
		GF_Err e = stbl_RemoveDTS(&stbl, cases[i].sample_number, cases[i].remove_count, 10);

		assert_equal(e, GF_OK, "%d");
		assert_equal(stts.nb_entries, (cases[i].remaining_count ? 1 : 0), "%u");
		assert_equal(stts.cumulated_start_dts, 100 + cases[i].removed_duration, LLU);
		/* stsz is updated by the caller after timing removal succeeds. */
		assert_equal(stsz.sampleCount, cases[i].sample_count, "%u");
		if (cases[i].remaining_count) {
			assert_equal(entry.sampleCount, cases[i].remaining_count, "%u");
			assert_equal(entry.sampleDelta, cases[i].remaining_delta, "%u");
		}
	}
#endif
}

unittest(stts_remove_multiple_runs)
{
#if !defined(GPAC_DISABLE_ISOM) \
    && (!defined(GPAC_DISABLE_ISOM_WRITE) || !defined(GPAC_DISABLE_ISOM_FRAGMENTS))
	u32 i, j, sample_number = 0;
	u64 dts = 0;
	/* Removing the middle sample keeps DTS 0, 10, 40, 60. */
	GF_SttsEntry entries[4] = {
		{ .sampleCount = 2, .sampleDelta = 10 },
		{ .sampleCount = 3, .sampleDelta = 20 },
	};
	GF_TimeToSampleBox stts = { .nb_entries = 2, .alloc_size = 4, .entries = entries };
	GF_SampleSizeBox stsz = { .sampleCount = 5 };
	GF_SampleTableBox stbl = { .TimeToSample = &stts, .SampleSize = &stsz };
	const u64 expected_dts[] = { 0, 10, 40, 60 };
	const u32 expected_duration[] = { 10, 30, 20, 20 };
	GF_Err e = stbl_RemoveDTS(&stbl, 3, 1, 10);

	assert_equal(e, GF_OK, "%d");
	assert_equal(stts.nb_entries, 3, "%u");
	assert_equal(stts.w_currentSampleNum, 4, "%u");
	assert_equal(stts.w_LastDTS, (u64)60, LLU);
	for (i=0; i<stts.nb_entries; i++) {
		/* Check the reconstructed runs against the retained sample timeline. */
		for (j=0; j<entries[i].sampleCount; j++) {
			assert_less(sample_number, (u32)GF_ARRAY_LENGTH(expected_dts), "%u");
			if (sample_number >= GF_ARRAY_LENGTH(expected_dts)) return;
			assert_equal(dts, expected_dts[sample_number], LLU);
			assert_equal(entries[i].sampleDelta, expected_duration[sample_number], "%u");
			dts += entries[i].sampleDelta;
			sample_number++;
		}
	}
	assert_equal(sample_number, 4, "%u");
#endif
}

unittest(stts_remove_invalid_range)
{
#if !defined(GPAC_DISABLE_ISOM) \
    && (!defined(GPAC_DISABLE_ISOM_WRITE) || !defined(GPAC_DISABLE_ISOM_FRAGMENTS))
	u32 i;
	const struct {
		u32 sample_count, sample_number, remove_count;
	} cases[] = {
		{ 0, 1, 1 }, /* empty sample table */
		{ 3, 0, 1 }, /* sample numbers start at one */
		{ 3, 1, 0 }, /* no sample selected for removal */
		{ 3, 4, 1 }, /* removal beyond the final sample */
		{ 3, 1, 4 }, /* bulk removal beyond the sample count */
		{ 3, 2, 2 }, /* bulk removal is only supported at the beginning */
	};

	for (i=0; i<GF_ARRAY_LENGTH(cases); i++) {
		GF_SttsEntry entry = { .sampleCount = cases[i].sample_count, .sampleDelta = 10 };
		GF_TimeToSampleBox stts = { .nb_entries = 1, .alloc_size = 1, .entries = &entry };
		GF_TimeToSampleBox original_stts = stts;
		GF_SampleSizeBox stsz = { .sampleCount = cases[i].sample_count };
		GF_SampleTableBox stbl = { .TimeToSample = &stts, .SampleSize = &stsz };
		GF_Err e = stbl_RemoveDTS(&stbl, cases[i].sample_number, cases[i].remove_count, 10);

		assert_equal(e, GF_BAD_PARAM, "%d");
		assert_equal_mem(&stts, &original_stts, sizeof(stts));
		assert_equal(entry.sampleCount, cases[i].sample_count, "%u");
	}
#endif
}
