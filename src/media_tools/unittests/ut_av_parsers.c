#include "tests.h"
#include <string.h>
#include <gpac/bitstream.h>
#include <gpac/internal/media_dev.h>

unittest(avc_pps_weighted_bipred_idc)
{
	AVCState *avc;
	s32 pps_id;
	//minimal AVC PPS NAL, byte-aligned:
	//- byte 0 (0x68): NAL header (nal_ref_idc=3, nal_unit_type=8/PPS)
	//- byte 1 (0xCE) + byte 2 (0x78): PPS RBSP with all ue(v)/se(v) fields set
	//  to 0 except weighted_bipred_idc, encoded as 1 (binary "01")
	u8 pps_nal[] = { 0x68, 0xCE, 0x78 };
	GF_BitStream *bs;

	GF_SAFEALLOC(avc, AVCState)
	//fake a registered SPS so the PPS sps_id lookup doesn't reject the PPS
	avc->sps[0].state = 1;

	bs = gf_bs_new(pps_nal, sizeof(pps_nal), GF_BITSTREAM_READ);
	pps_id = gf_avc_read_pps_bs(bs, avc);
	gf_bs_del(bs);

	assert_true(pps_id == 0);
	assert_equal(avc->pps[0].weighted_bipred_idc, 1, "%u");

	gf_free(avc);
}

#ifndef GPAC_DISABLE_AV_PARSERS
static void av1_annexb_test_leb128(GF_BitStream *bs, u32 value)
{
	do {
		u8 byte = value & 127;
		value >>= 7;
		gf_bs_write_u8(bs, byte | (value ? 128 : 0));
	} while (value);
}
#endif

unittest(av1_annexb_partial_temporal_unit)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	//128x64 reduced still-picture headers, with synthetic tile payloads
	const u8 sequence[] = { 0x0a, 0x06, 0x18, 0x19, 0x7f, 0xfe, 0xd8, 0x04 };
	const u8 header[] = { 0x13, 0xc0, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00 };
	const u32 tile_size = 64;
	const u32 obu_size = 2 + sizeof(header) + tile_size;
	u32 i, j, size, payload_size;
	u8 *data, *payload;
	GF_BitStream *bs, *tu = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	GF_BitStream *out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	AV1State *state, *snapshot;
	GF_SAFEALLOC(state, AV1State);
	GF_SAFEALLOC(snapshot, AV1State);
	gf_av1_init_state(state);
	state->config = gf_odf_av1_cfg_new();
	state->mem_mode = GF_TRUE;
	state->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	*snapshot = *state;

	//Use 3 frames to ensure some complete frames precede a truncated frame
	for (i=0; i<3; i++) {
		u32 frame_size = 1 + obu_size;
		if (!i) frame_size += 3 + 1 + sizeof(sequence);
		av1_annexb_test_leb128(tu, frame_size);
		if (!i) {
			gf_bs_write_u8(tu, 2);
			gf_bs_write_u8(tu, 0x12); //temporal delimiter
			gf_bs_write_u8(tu, 0);
			gf_bs_write_u8(tu, sizeof(sequence));
			gf_bs_write_data(tu, sequence, sizeof(sequence));
		}
		gf_bs_write_u8(tu, obu_size);
		gf_bs_write_u8(tu, 0x32); //OBU_FRAME
		gf_bs_write_u8(tu, sizeof(header) + tile_size);
		gf_bs_write_data(tu, header, sizeof(header));
		for (j=0; j<tile_size; j++) gf_bs_write_u8(tu, i+j);
	}
	gf_bs_get_content(tu, &payload, &payload_size);
	av1_annexb_test_leb128(out, payload_size);
	gf_bs_write_data(out, payload, payload_size);
	gf_bs_get_content(out, &data, &size);
	gf_free(payload);
	gf_bs_del(tu);
	gf_bs_del(out);

	//try several truncation points:
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	for (i=size-1; i>0; i--) {
		GF_Err e;
		gf_bs_reassign_buffer(bs, data, i);
		e = aom_av1_parse_temporal_unit_from_annexb(bs, state);
		assert_equal(e, GF_BUFFER_TOO_SMALL, "%d");
		assert_equal_mem(state, snapshot, sizeof(*state));
		assert_equal(state->config->chroma_subsampling_x, 0, "%u");
		assert_equal(gf_bs_get_position(state->bs), (u64) 0, "%llu");
		if (e != GF_BUFFER_TOO_SMALL || memcmp(state, snapshot, sizeof(*state))) break;
	}
	gf_bs_reassign_buffer(bs, data, size);
	//reset any state left by the old parser so the complete-input control also
	//runs when demonstrating the regression against the unpatched build
	gf_av1_reset_state(state, GF_FALSE);
	assert_equal(aom_av1_parse_temporal_unit_from_annexb(bs, state), GF_OK, "%d");
	assert_equal(gf_bs_get_position(bs), (u64) size, "%llu");
	assert_equal(state->width, 128, "%u");
	assert_equal(state->height, 64, "%u");
	assert_equal(state->frame_state.nb_tiles_in_obu, 1, "%u");
	assert_equal(state->frame_state.tiles[0].size, tile_size, "%u");
	assert_equal(state->RefUpscaledWidth[0], 128, "%u");
	assert_equal(state->RefFrameHeight[0], 64, "%u");
	assert_greater(gf_bs_get_position(state->bs), (u64) 0, "%llu");

	gf_av1_reset_state(state, GF_TRUE);
	gf_odf_av1_cfg_del(state->config);
	gf_bs_del(bs);
	gf_free(data);
	gf_free(snapshot);
	gf_free(state);
#endif
}
