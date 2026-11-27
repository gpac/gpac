#include "tests.h"

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)

#include <gpac/internal/isomedia_dev.h>

unittest(stbl_add_cts_insert_with_short_ctts)
{
	u32 i;
	GF_DttsEntry *entries;
	GF_CompositionOffsetBox ctts = {0};
	GF_SampleSizeBox stsz = {0};
	GF_SampleTableBox stbl = {0};

	/* 100 entries covering 101 samples (entry 1 is a run of 2), while stsz already counts 212 samples
	(the sample being inserted included). Inserting at sample 3 splits the run of 2, and the 0 padding
	of the uncovered samples needs one more entry: 103 entries, more than the 102 usually reserved. */
	entries = gf_malloc(sizeof(GF_DttsEntry) * 100);
	assert_not_null(entries);
	for (i=0; i<100; i++) {
		entries[i].sampleCount = (i==1) ? 2 : 1;
		entries[i].decodingOffset = 10 + i;
	}
	ctts.entries = entries;
	ctts.nb_entries = 100;
	ctts.alloc_size = 100;
	ctts.w_LastSampleNumber = 101;
	stsz.sampleCount = 212;
	stbl.CompositionOffset = &ctts;
	stbl.SampleSize = &stsz;

	assert_equal(stbl_AddCTS(&stbl, 3, -1000), GF_OK, "%d");
	/* entries may have been reallocated: free the final pointer */
	test_defer(gf_free, ctts.entries);
	assert_equal(ctts.nb_entries, 103, "%u");
	assert_greater_equal(ctts.alloc_size, ctts.nb_entries, "%u");
	assert_equal(ctts.entries[2].decodingOffset, -1000, "%d");
	assert_equal(ctts.entries[2].sampleCount, 1, "%u");
	assert_equal(ctts.entries[102].decodingOffset, 0, "%d");
	assert_equal(ctts.entries[102].sampleCount, 111, "%u");
	assert_equal(ctts.w_LastSampleNumber, 102, "%u");
}

#else
unittest_disabled(stbl_add_cts_insert_with_short_ctts, "ISO media write disabled")
#endif
