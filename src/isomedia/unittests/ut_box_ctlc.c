#include "tests.h"
#include <gpac/internal/isomedia_dev.h>

unittest(ctlc_box_roundtrip)
{
	GF_Err e;
	GF_BitStream *bs;
	u8 *data = NULL;
	u32 size = 0;
	GF_Box *box = NULL;
	GF_ContentTypeForLoudnessControlBox *ptr;

	/* content_type=2 (Music), flags=3 (Advertisement + Immersive) */
	ptr = (GF_ContentTypeForLoudnessControlBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CTLC);
	assert_not_null(ptr);
	if (!ptr) return;
	ptr->content_type = 2;
	ptr->flags = GF_ISOM_CTLC_FLAG_ADVERTISEMENT | GF_ISOM_CTLC_FLAG_IMMERSIVE_AUDIO;

	e = gf_isom_box_size((GF_Box *)ptr);
	assert_equal(e, GF_OK, "%d");
	if (e != GF_OK) {
		gf_isom_box_del((GF_Box *)ptr);
		return;
	}
	assert_equal(ptr->size, (u64)13, LLU);

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	assert_not_null(bs);
	if (!bs) {
		gf_isom_box_del((GF_Box *)ptr);
		return;
	}
	e = gf_isom_box_write((GF_Box *)ptr, bs);
	assert_equal(e, GF_OK, "%d");
	if (e != GF_OK) {
		gf_bs_del(bs);
		gf_isom_box_del((GF_Box *)ptr);
		return;
	}
	gf_bs_get_content(bs, &data, &size);
	gf_bs_del(bs);
	gf_isom_box_del((GF_Box *)ptr);

	assert_not_null(data);
	assert_equal(size, 13, "%u");
	if (!data) return;

	/* parse the serialized bytes back and check the registry dispatched to the
	 * ctlc box functions rather than falling back to a generic unknown box */
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	assert_not_null(bs);
	if (!bs) {
		gf_free(data);
		return;
	}
	e = gf_isom_box_parse(&box, bs);
	gf_bs_del(bs);
	gf_free(data);
	assert_equal(e, GF_OK, "%d");
	assert_not_null(box);
	if ((e != GF_OK) || !box) {
		if (box) gf_isom_box_del(box);
		return;
	}
	assert_equal(box->type, GF_ISOM_BOX_TYPE_CTLC, "%u");
	if (box->type != GF_ISOM_BOX_TYPE_CTLC) {
		gf_isom_box_del(box);
		return;
	}

	ptr = (GF_ContentTypeForLoudnessControlBox *) box;
	assert_equal(ptr->version, 0, "%u");
	assert_equal(ptr->flags, (GF_ISOM_CTLC_FLAG_ADVERTISEMENT | GF_ISOM_CTLC_FLAG_IMMERSIVE_AUDIO), "%u");
	assert_equal(ptr->content_type, 2, "%u");

	gf_isom_box_del(box);
}

unittest(ctlc_box_truncated)
{
	GF_Err e;
	GF_BitStream *bs;
	GF_Box *box = NULL;
	u32 log_level;

	/* a ctlc box declaring a size too small to hold the mandatory content_type byte */
	u8 truncated_ctlc[] = { 0x00, 0x00, 0x00, 0x0C, 'c', 't', 'l', 'c', 0x00, 0x00, 0x00, 0x00 };

	bs = gf_bs_new(truncated_ctlc, sizeof(truncated_ctlc), GF_BITSTREAM_READ);
	assert_not_null(bs);
	if (!bs) return;

	/* This malformed box is expected to be rejected. Keep the parser's
	 * diagnostic from making the unit-test run look like it failed. */
	log_level = gf_log_get_tool_level(GF_LOG_CONTAINER);
	gf_log_set_tool_level(GF_LOG_CONTAINER, GF_LOG_QUIET);

	e = gf_isom_box_parse(&box, bs);
	gf_bs_del(bs);

	gf_log_set_tool_level(GF_LOG_CONTAINER, (GF_LOG_Level) log_level);

	assert_true(e != GF_OK);
	if (box) gf_isom_box_del(box);
}
