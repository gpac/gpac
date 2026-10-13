#include "tests.h"
#include <gpac/tools.h>
#include <stddef.h>

unittest(cenc_key_info_iv_sizes)
{
	/* Byte fields preserve the encoded layout, including the big-endian
	   key count, without native integer alignment or byte-order concerns. */
	typedef struct {
		u8 multikey;
		u8 key_count_msb;
		u8 key_count_lsb;
		u8 iv_size;
		u8 key_id[16];
		u8 constant_iv_size;
		u8 constant_iv[17]; /* Includes space for the oversized-IV case. */
	} KeyInfoFixture;
	enum {
		IV_SIZE_64_BITS = 8,
		IV_SIZE_128_BITS = 16,
		OVERSIZED_IV_SIZE = IV_SIZE_128_BITS + 1,
		MAX_ENCODED_IV_SIZE = 255
	};
	KeyInfoFixture key_info = {0};
	const u8 *encoded = (const u8 *) &key_info;
	u32 single_key_size = offsetof(KeyInfoFixture, constant_iv_size);
	u32 constant_iv_header_size = offsetof(KeyInfoFixture, constant_iv);
	u32 old_level = gf_log_get_tool_level(GF_LOG_CORE);
	gf_log_set_tool_level(GF_LOG_CORE, GF_LOG_QUIET);

	/* Both permitted per-sample IV sizes fit the decrypter's stack buffer. */
	key_info.iv_size = IV_SIZE_64_BITS;
	assert_true(gf_cenc_validate_key_info(encoded, single_key_size));
	key_info.iv_size = IV_SIZE_128_BITS;
	assert_true(gf_cenc_validate_key_info(encoded, single_key_size));
	key_info.iv_size = OVERSIZED_IV_SIZE;
	assert_true(!gf_cenc_validate_key_info(encoded, single_key_size));
	key_info.iv_size = MAX_ENCODED_IV_SIZE;
	assert_true(!gf_cenc_validate_key_info(encoded, single_key_size));

	/* Constant IVs have the same bound; a clear default has none. */
	key_info.iv_size = 0;
	key_info.constant_iv_size = IV_SIZE_64_BITS;
	assert_true(gf_cenc_validate_key_info(encoded, constant_iv_header_size + IV_SIZE_64_BITS));
	key_info.constant_iv_size = IV_SIZE_128_BITS;
	assert_true(gf_cenc_validate_key_info(encoded, constant_iv_header_size + IV_SIZE_128_BITS));
	key_info.constant_iv_size = OVERSIZED_IV_SIZE;
	assert_true(!gf_cenc_validate_key_info(encoded, constant_iv_header_size + OVERSIZED_IV_SIZE));
	key_info.constant_iv_size = 0;
	assert_true(gf_cenc_validate_key_info(encoded, constant_iv_header_size));

	/* The HEIF ienc and multikey seig forms include a key count. */
	key_info.multikey = GF_TRUE;
	key_info.key_count_lsb = 1;
	key_info.iv_size = IV_SIZE_128_BITS;
	assert_true(gf_cenc_validate_key_info(encoded, single_key_size));
	key_info.iv_size = MAX_ENCODED_IV_SIZE;
	assert_true(!gf_cenc_validate_key_info(encoded, single_key_size));
	key_info.iv_size = IV_SIZE_128_BITS;
	/* Declaring two keys is invalid when only one key record is present. */
	key_info.key_count_lsb = 2;
	assert_true(!gf_cenc_validate_key_info(encoded, single_key_size));
	gf_log_set_tool_level(GF_LOG_CORE, (GF_LOG_Level) old_level);
}
