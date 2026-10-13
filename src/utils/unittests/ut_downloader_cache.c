#include "../downloader.h"
#include "tests.h"

unittest(cache_rejects_wrapped_content_length)
{
#ifndef GPAC_DISABLE_NETWORK
	GF_DownloadSession sess = {0};
	GF_Mutex *mx = gf_mx_new("cache test");
	DownloadedCacheEntry entry;
	assert_not_null(mx);
	entry = gf_cache_create_entry("", "http://example.invalid/oversized", 0, 0, GF_TRUE, mx);
	assert_not_null(entry);

	// The cache reserves two trailing bytes; a 32-bit maximum length must fail before allocation.
	assert_equal(gf_cache_set_content_length(entry, GF_UINT_MAX), GF_OK, "%d");
	assert_equal(gf_cache_open_write_cache(entry, &sess), GF_BAD_PARAM, "%d");

	gf_cache_close_write_cache(entry, &sess, GF_FALSE);
	gf_cache_delete_entry(entry);
	gf_mx_del(mx);
#endif
}

unittest(cache_accepts_valid_content_length)
{
#ifndef GPAC_DISABLE_NETWORK
	GF_DownloadSession sess = {0};
	GF_Mutex *mx = gf_mx_new("cache test");
	DownloadedCacheEntry entry;
	const u8 *content;
	u32 size, max_valid_size;
	Bool was_modified;
	assert_not_null(mx);
	entry = gf_cache_create_entry("", "http://example.invalid/small", 0, 0, GF_TRUE, mx);
	assert_not_null(entry);

	// A valid advertised length still allocates enough space for the body.
	assert_equal(gf_cache_set_content_length(entry, 4), GF_OK, "%d");
	assert_equal(gf_cache_open_write_cache(entry, &sess), GF_OK, "%d");
	assert_equal(gf_cache_write_to_cache(entry, &sess, "test", 4, mx), GF_OK, "%d");
	content = gf_cache_get_content(entry, &size, &max_valid_size, &was_modified);
	assert_not_null(content);
	assert_equal(size, 4, "%u");
	assert_equal_mem(content, "test", 4);

	gf_cache_close_write_cache(entry, &sess, GF_TRUE);
	gf_cache_delete_entry(entry);
	gf_mx_del(mx);
#endif
}
