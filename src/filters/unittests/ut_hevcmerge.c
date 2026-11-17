#include "tests.h"
#include <gpac/internal/media_dev.h>

#ifndef GPAC_DISABLE_AV_PARSERS

unittest(hevc_suffix_hash)
{
	const u8 hash[] = { 0x50, 0x01, 0x84, 0x01, 0x02, 0x80 };
	const u8 mixed[] = { 0x50, 0x01, 0x05, 0x04, 0x00, 0x00, 0x03, 0x01, 0x02,
	                     0x84, 0x01, 0x02, 0x80 };
	const u8 hash_first[] = { 0x50, 0x01, 0x84, 0x01, 0x02,
	                          0x05, 0x04, 0x00, 0x00, 0x03, 0x01, 0x02, 0x80 };
	const u8 multiple_hashes[] = { 0x50, 0x01, 0x84, 0x01, 0x02,
	                               0x05, 0x04, 0x00, 0x00, 0x03, 0x01, 0x02,
	                               0x84, 0x01, 0x02, 0x80 };
	const u8 other[] = { 0x50, 0x01, 0x05, 0x04, 0x00, 0x00, 0x03, 0x01, 0x02, 0x80 };
	const u8 malformed[] = { 0x50, 0x01, 0x84, 0x05, 0x02, 0x80 };
	char data[sizeof(multiple_hashes)];
	u32 size;

	memcpy(data, hash, sizeof(hash));
	size = sizeof(hash);
	assert_equal(gf_hevc_filter_suffix_hash(data, &size, GF_FALSE), GF_OK, "%d");
	assert_equal(size, 0U, "%u");
	memcpy(data, mixed, sizeof(mixed));
	size = sizeof(mixed);
	assert_equal(gf_hevc_filter_suffix_hash(data, &size, GF_FALSE), GF_OK, "%d");
	assert_equal(size, (u32) sizeof(other), "%u");
	assert_equal_mem(data, other, sizeof(other));
	memcpy(data, hash_first, sizeof(hash_first));
	size = sizeof(hash_first);
	assert_equal(gf_hevc_filter_suffix_hash(data, &size, GF_FALSE), GF_OK, "%d");
	assert_equal(size, (u32) sizeof(other), "%u");
	assert_equal_mem(data, other, sizeof(other));
	memcpy(data, multiple_hashes, sizeof(multiple_hashes));
	size = sizeof(multiple_hashes);
	assert_equal(gf_hevc_filter_suffix_hash(data, &size, GF_FALSE), GF_OK, "%d");
	assert_equal(size, (u32) sizeof(other), "%u");
	assert_equal_mem(data, other, sizeof(other));
	memcpy(data, other, sizeof(other));
	size = sizeof(other);
	assert_equal(gf_hevc_filter_suffix_hash(data, &size, GF_FALSE), GF_OK, "%d");
	assert_equal(size, (u32) sizeof(other), "%u");
	assert_equal_mem(data, other, sizeof(other));
	memcpy(data, mixed, sizeof(mixed));
	size = sizeof(mixed);
	assert_equal(gf_hevc_filter_suffix_hash(data, &size, GF_TRUE), GF_OK, "%d");
	assert_equal(size, (u32) sizeof(mixed), "%u");
	assert_equal_mem(data, mixed, sizeof(mixed));
	memcpy(data, hash, sizeof(hash));
	size = sizeof(hash);
	assert_equal(gf_hevc_filter_suffix_hash(data, &size, GF_TRUE), GF_OK, "%d");
	assert_equal(size, (u32) sizeof(hash), "%u");
	assert_equal_mem(data, hash, sizeof(hash));
	memcpy(data, malformed, sizeof(malformed));
	size = sizeof(malformed);
	assert_equal(gf_hevc_filter_suffix_hash(data, &size, GF_FALSE), GF_NON_COMPLIANT_BITSTREAM, "%d");
	size = 2;
	assert_equal(gf_hevc_filter_suffix_hash(data, &size, GF_FALSE), GF_NON_COMPLIANT_BITSTREAM, "%d");
}

#endif
