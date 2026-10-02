#include "tests.h"
#include <gpac/internal/isomedia_dev.h>

GF_Err stbl_AppendSize(GF_SampleTableBox *stbl, u32 size, u32 nb_pack);

unittest(stbl_append_size_rejects_cumulative_overflow)
{
	GF_SampleTableBox stbl = {0};
	GF_SampleSizeBox stsz = {0};
	u32 log_level = gf_log_get_tool_level(GF_LOG_CONTAINER);
	stbl.SampleSize = &stsz;
	stsz.sampleCount = GF_UINT_MAX - 1;
	stsz.sampleSize = 1;

	/* A packed two-sample fragment cannot wrap the existing sample count. */
	gf_log_set_tool_level(GF_LOG_CONTAINER, GF_LOG_QUIET);
	assert_equal(stbl_AppendSize(&stbl, 2, 2), GF_ISOM_INVALID_FILE, "%d");
	assert_equal(stsz.sampleCount, GF_UINT_MAX - 1, "%u");
	assert_equal(stsz.sampleSize, 1, "%u");
	assert_true(stsz.sizes == NULL);
	assert_equal(stsz.alloc_size, 0, "%u");

	/* The constant-size fast path needs the same count check. */
	assert_equal(stbl_AppendSize(&stbl, 1, 2), GF_ISOM_INVALID_FILE, "%d");
	assert_equal(stsz.sampleCount, GF_UINT_MAX - 1, "%u");
	gf_log_set_tool_level(GF_LOG_CONTAINER, (GF_LOG_Level) log_level);

	/* The largest representable count itself remains valid. */
	assert_equal(stbl_AppendSize(&stbl, 1, 1), GF_OK, "%d");
	assert_equal(stsz.sampleCount, GF_UINT_MAX, "%u");
}

unittest(stbl_append_size_keeps_valid_packed_sizes)
{
	GF_SampleTableBox stbl = {0};
	GF_SampleSizeBox stsz = {0};
	stbl.SampleSize = &stsz;
	stsz.sampleCount = 100;
	stsz.sampleSize = 1;

	/* A valid packed fragment with a new size expands the constant-size table. */
	assert_equal(stbl_AppendSize(&stbl, 2, 2), GF_OK, "%d");
	assert_equal(stsz.sampleCount, 102, "%u");
	assert_greater_equal(stsz.alloc_size, 102, "%u");
	assert_not_null(stsz.sizes);
	if (stsz.sizes) {
		assert_equal(stsz.sizes[0], 1, "%u");
		assert_equal(stsz.sizes[99], 1, "%u");
		assert_equal(stsz.sizes[100], 2, "%u");
		assert_equal(stsz.sizes[101], 2, "%u");
		gf_free(stsz.sizes);
	}
}
