#include "tests.h"
#include <gpac/bitstream.h>

unittest(bs_reassign_read_resets_overflow)
{
	static const u8 d1[] = { 0x12, 0x34 };
	static const u8 d2[] = { 0xab, 0xcd, 0xef, 0x01 };
	u32 old_level = gf_log_get_tool_level(GF_LOG_CORE);
	GF_BitStream *bs = gf_bs_new(d1, sizeof(d1), GF_BITSTREAM_READ);
	gf_log_set_tool_level(GF_LOG_CORE, GF_LOG_QUIET);

	gf_bs_read_u32(bs); /* reads past the end of d1 */
	assert_true(gf_bs_is_overflow(bs));

	assert_equal(gf_bs_reassign_buffer(bs, d2, sizeof(d2)), GF_OK, "%d");
	assert_false(gf_bs_is_overflow(bs));
	assert_equal(gf_bs_read_u8(bs), 0xab, "%u");
	assert_false(gf_bs_is_overflow(bs));

	gf_bs_del(bs);
	gf_log_set_tool_level(GF_LOG_CORE, old_level);
}
