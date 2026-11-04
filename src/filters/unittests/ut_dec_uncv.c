#include "tests.h"
#include "../dec_uncv.c"

#ifndef GPAC_DISABLE_UNCVDEC
static u8 *uncv_test_rgb_config(u32 red_bits, u32 tile_cols, u32 tile_rows, u32 *size)
{
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	u8 *data = NULL;

	gf_bs_write_u32(bs, 18); // cmpd: three RGB component definitions
	gf_bs_write_u32(bs, GF_4CC('c','m','p','d'));
	gf_bs_write_u32(bs, 3);
	for (u32 i=0; i<3; i++) gf_bs_write_u16(bs, 4+i);

	gf_bs_write_u32(bs, 59); // uncC: full-box header, profile, components, layout
	gf_bs_write_u32(bs, GF_4CC('u','n','c','C'));
	gf_bs_write_u32(bs, 0); // version and flags
	gf_bs_write_u32(bs, 0); // no profile
	gf_bs_write_u32(bs, 3);
	for (u32 i=0; i<3; i++) {
		gf_bs_write_u16(bs, i); // component definition index
		gf_bs_write_u8(bs, (i ? 8 : red_bits) - 1);
		gf_bs_write_u8(bs, 0); // unsigned integer
		gf_bs_write_u8(bs, 0); // no component alignment
	}
	gf_bs_write_u8(bs, SAMPLING_NONE);
	gf_bs_write_u8(bs, INTERLEAVE_PIXEL);
	gf_bs_write_u8(bs, 0); // no block layout
	gf_bs_write_u8(bs, 0); // big-endian components, no padding flags
	gf_bs_write_u32(bs, 0); // no pixel alignment, components are packed
	gf_bs_write_u32(bs, 0); // no row alignment
	gf_bs_write_u32(bs, 0); // no tile alignment
	gf_bs_write_u32(bs, tile_cols-1);
	gf_bs_write_u32(bs, tile_rows-1);
	gf_bs_get_content(bs, &data, size);
	gf_bs_del(bs);
	return data;
}

static GF_Err uncv_test_configure(UNCVDecCtx *ctx, u32 width, u32 height, u32 red_bits, Bool single_pixel_tiles)
{
	u32 size;
	u8 *data = uncv_test_rgb_config(red_bits, single_pixel_tiles ? width : 1, single_pixel_tiles ? height : 1, &size);
	ctx->width = width;
	ctx->height = height;
	GF_Err e = uncv_config(ctx, data, size);
	gf_free(data);
	return e;
}
#endif

unittest(uncvdec_output_size_bounds)
{
#ifndef GPAC_DISABLE_UNCVDEC
	UNCVDecCtx ctx = {0};
	// One-pixel tiles keep configuration cheap even at the u32 size boundary.
	u32 max_rgb_pixels = GF_UINT_MAX / 3;
	assert_equal(uncv_test_configure(&ctx, max_rgb_pixels, 1, 7, GF_TRUE), GF_OK, "%d");
	assert_equal(ctx.stride, max_rgb_pixels*3, "%u");
	assert_equal(ctx.output_size, max_rgb_pixels*3, "%u");
	assert_equal(uncv_test_configure(&ctx, max_rgb_pixels+1, 2, 7, GF_TRUE), GF_NON_COMPLIANT_BITSTREAM, "%d");
	assert_equal(uncv_test_configure(&ctx, 1, max_rgb_pixels, 7, GF_TRUE), GF_OK, "%d");
	assert_equal(ctx.stride, 3, "%u");
	assert_equal(ctx.output_size, max_rgb_pixels*3, "%u");
	assert_equal(uncv_test_configure(&ctx, 1, max_rgb_pixels+1, 7, GF_TRUE), GF_NON_COMPLIANT_BITSTREAM, "%d");
	// Both dimension fields may be maximal; the intermediate product must not wrap.
	assert_equal(uncv_test_configure(&ctx, GF_UINT_MAX, GF_UINT_MAX, 7, GF_TRUE), GF_NON_COMPLIANT_BITSTREAM, "%d");
	uncv_reset(&ctx);
#endif
}

unittest(uncvdec_reconstructed_rgb)
{
#ifndef GPAC_DISABLE_UNCVDEC
	UNCVDecCtx ctx = {0};
	// A rejected configuration must still allow the next valid reconstruction.
	assert_equal(uncv_test_configure(&ctx, 0x55555556, 2, 7, GF_TRUE), GF_NON_COMPLIANT_BITSTREAM, "%d");
	assert_equal(uncv_test_configure(&ctx, 2, 2, 7, GF_FALSE), GF_OK, "%d");
	assert_false(ctx.passthrough);
	assert_equal(ctx.stride, 6, "%u");
	assert_equal(ctx.output_size, 12, "%u");
	// A seven-bit red component requires expansion to the eight-bit output range.
	u8 input[] = {0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00};
	u8 expected[] = {0xff, 0x80, 0x00, 0x7e, 0xc0, 0x00, 0xff, 0x80, 0x00, 0x7e, 0xc0, 0x00};
	u8 output[sizeof(expected)];
	uncv_start_frame(&ctx, input, sizeof(input));
	for (u32 y=0; y<ctx.height; y++) {
		for (u32 x=0; x<ctx.width; x++) {
			u32 offset = y*ctx.stride + x*ctx.bpp;
			ctx.read_pixel(&ctx, ctx.cfg, x, y, output, offset);
			memcpy(output+offset, ctx.pixel, ctx.bpp);
		}
		uncv_end_line(&ctx, ctx.cfg);
	}
	assert_equal_mem(output, expected, sizeof(expected));
	assert_equal(uncv_test_configure(&ctx, 2, 2, 8, GF_FALSE), GF_OK, "%d");
	assert_true(ctx.passthrough);
	uncv_reset(&ctx);
#endif
}
