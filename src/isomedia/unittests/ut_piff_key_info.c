#include "tests.h"
#include <gpac/isomedia.h>

unittest(piff_default_key_info)
{
#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)
	/* Single-key encoding: three prefix bytes, IV size, then the full KID. */
	struct {
		u8 prefix[3];
		u8 iv_size;
		u8 key_id[16];
	} key_info = {{0}, 8, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}};
	GF_GenericSampleDescription desc = {0};
	GF_ISOFile *file;
	GF_Err e;
	u32 track, desc_index, returned_size;
	const u8 *returned_key_info;
	Bool encrypted;

	/* Edit mode uses a temporary backing file; delete without writing output. */
	file = gf_isom_open("piff-key-info.mp4", GF_ISOM_WRITE_EDIT, NULL);
	assert_not_null(file);
	if (!file) return;
	track = gf_isom_new_track(file, 0, GF_ISOM_MEDIA_VISUAL, 1000);
	assert_true(track != 0);
	if (!track) goto exit;
	desc.codec_tag = GF_4CC('t', 'e', 's', 't');
	e = gf_isom_new_generic_sample_description(file, track, NULL, NULL, &desc, &desc_index);
	assert_equal(e, GF_OK, "%d");
	if (e) goto exit;

	/* Create PIFF protection through the same API that stores its key record. */
	e = gf_isom_set_cenc_protection(file, track, desc_index, GF_ISOM_PIFF_SCHEME,
		0x00010000, GF_TRUE, 0, 0, (u8 *) &key_info, sizeof(key_info));
	assert_equal(e, GF_OK, "%d");
	if (e) goto exit;
	e = gf_isom_cenc_get_default_info(file, track, desc_index, NULL, &encrypted,
		NULL, NULL, &returned_key_info, &returned_size);
	assert_equal(e, GF_OK, "%d");
	if (e) goto exit;

	/* Consumers must receive all 20 bytes, not a truncated 19-byte record. */
	assert_true(encrypted);
	assert_not_null(returned_key_info);
	if (!returned_key_info) goto exit;
	assert_equal(returned_size, (u32) sizeof(key_info), "%u");
	assert_true(gf_cenc_validate_key_info(returned_key_info, returned_size));
	assert_equal_mem(returned_key_info, &key_info, sizeof(key_info));
exit:
	gf_isom_delete(file);
#endif
}
