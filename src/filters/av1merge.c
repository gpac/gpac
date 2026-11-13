/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau, Jean Le Feuvre
 *			Copyright (c) Motion Spell, Telecom Paris 2026
 *					All rights reserved
 *
 *  This file is part of GPAC / AV1 tile merger filter
 *  and largely inspired from the HEVC tile merger filter
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/bitstream.h>
#include <gpac/filters.h>
#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>

#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined(GPAC_DISABLE_AV1MERGE)

//#define AV1MERGE_VERBOSE

#define AV1_SUPERRES_DENOM_MIN 9
#define AV1_SUPERRES_DENOM_BITS 3
#define AV1_SUPERRES_NUM 8

#define AV1_LAST_FRAME 1
#define AV1_REFS_PER_FRAME 7
#define AV1_PRIMARY_REF_NONE 7
#define AV1_NUM_REF_FRAMES 8
#define AV1_ALL_FRAMES ((1 << AV1_NUM_REF_FRAMES) - 1)

typedef struct
{
	GF_FilterPid *pid;

	AV1State state;
	u32 last_size;
	u32 packet_start, packet_end, obu_pos;
	u32 tile_start;
	u32 tile_id; //1-based raster-order index
	Bool has_pos;
	s32 pos_x, pos_y;
	u32 grid_x, grid_y;
	u32 obu_start, payload_start, header_size;
	u64 header_end_bits;
	ObuType obu_type;

	//used both to check if dsi changed
	//and if dsi is different between tiles
	u32 dsi_crc;

	u32 width;
	u32 height;
	u32 render_width, render_height;
	u8 base_q_idx;
	u32 ref_render_width[AV1_NUM_REF_FRAMES], ref_render_height[AV1_NUM_REF_FRAMES];

	//timescale of source pid
	//number of packets processed
	u32 timescale;
	u32 nb_pck;
} AV1TilePidCtx;

typedef struct
{
	GF_List *ipids; //AV1TilePidCtx
	GF_FilterPid *opid;
	Bool in_error;

	u32 cols;
	u32 rows;
	u32 col_width[AV1_MAX_TILE_COLS], row_height[AV1_MAX_TILE_ROWS];
	u32 out_width, out_height, render_width, render_height;
	Bool cols_forced, rows_forced, grid_changed, has_gaps;
	AV1TilePidCtx *incomplete_tile;
	Bool drain;
	u8 tile_size_bytes;
	u8 *buffer;
	u32 buffer_alloc;
	AV1State rewrite_state;

	GF_List *ordered_ipids /*AV1TilePidCtx, in raster order*/;

	Bool is_playing;
	GF_FEVT_Play play_evt;

	GF_BitStream *bs_in;
	GF_BitStream *bs_out;
	GF_BitStream *bs_pck;
} GF_AV1MergeCtx;

//Releases shared output packet storage; no failure return.
static void release_buffer(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	u32 size;
	gf_free((void *) gf_filter_pck_get_data(pck, &size));
}

//Copies one bit field and returns its value; bitstream errors are not checked here.
static GFINLINE int bs_rewrite(GF_BitStream *bs_ori, GF_BitStream *bs_new, s32 bits)
{
	const int val = gf_bs_read_int(bs_ori, bits);
	gf_bs_write_int(bs_new, val, bits);
	return val;
}

//Returns the number of bits needed for an unsigned value, or zero for zero; cannot fail.
static u32 count_bits(u32 val)
{
	u32 count = 0;
	while (val) {
		count++;
		val >>= 1;
	}
	return count;
}

#ifdef AV1MERGE_VERBOSE
//Copies and prints one named bit field; bitstream errors are not checked here.
static GFINLINE int bs_rewriten(GF_BitStream *bs_ori, GF_BitStream *bs_new, s32 bits, const char *name_unused)
{
	const int val = gf_bs_read_int(bs_ori, bits);
	gf_bs_write_int(bs_new, val, bits);
	printf("%s: %d\n", name_unused, val);
	return val;
}

#define REWRITE(bits, name) bs_rewriten(ctx->bs_in, ctx->bs_out, bits, name)
#define REWRITEi(bits, name) bs_rewriten(bs_ori, bs_new, bits, name)
#else
#define REWRITE(bits, name_unused) bs_rewrite(ctx->bs_in, ctx->bs_out, bits)
#define REWRITEi(bits, name_unused) bs_rewrite(bs_ori, bs_new, bits)
#endif

//Copies the sequence header prefix with merged dimensions; asserts on VFR timing or excessive UVLC leading zeros.
static void av1merge_rewrite_seq_hdr(GF_AV1MergeCtx *ctx, GF_BitStream *bs_ori, GF_BitStream *bs_new)
{
	u32 i;
	Bool decoder_model_info_present_flag = GF_FALSE, initial_display_delay_present_flag;
	u8 buffer_delay_length_minus_1 = 0;

	REWRITEi(3, "seq_profile");
	REWRITEi(1, "still_picture");
	if (REWRITEi(1, "reduced_still_picture_header")) {
		REWRITEi(5, "seq_level_idx_0");
	} else {
		if (REWRITEi(1, "timing_info_present_flag")) {
			//timing_info
			REWRITEi(32, "num_units_in_display_tick");
			REWRITEi(32, "time_scale");
			if (REWRITEi(1, "equal_picture_interval")) {
				//num_ticks_per_picture_minus_1 is uvlc
				{
					u8 leadingZeros = 0;
					while (1) {
						Bool done = bs_rewrite(bs_ori, bs_new, 1);
						if (done)
							break;
						leadingZeros++;
						if (leadingZeros >= 32) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1Merge] Leading zeros exceed 32. Abort.\n"));
							gf_fatal_assert(0);
						}
					}
					bs_rewrite(bs_ori, bs_new, leadingZeros);
				}
			}
			else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1Merge] VFR not supported. Abort.\n"));
				gf_fatal_assert(0);
			}

			decoder_model_info_present_flag = REWRITEi(1, "decoder_model_info_present_flag");
			if (decoder_model_info_present_flag) {
				//decoder_model_info
				buffer_delay_length_minus_1 = REWRITEi(5, "buffer_delay_length_minus_1");
				REWRITEi(32, "num_units_in_decoding_tick");
				REWRITEi(5, "buffer_removal_time_length");
				REWRITEi(5, "frame_presentation_time_length_minus1");
			}
		}
		initial_display_delay_present_flag = REWRITEi(1, "initial_display_delay_present_flag");
		u8 operating_points_count = REWRITEi(5, "operating_points_count_minus1") + 1;
		for (i = 0; i < operating_points_count; i++) {
			u8 seq_level_idx_i = 0;
			REWRITEi(12, "operating_point_idc");
			seq_level_idx_i = REWRITEi(5, "seq_level_idx_i");
			if (seq_level_idx_i > 7) {
				REWRITEi(1, "seq_tier");
			}


			if (decoder_model_info_present_flag) {
				if (REWRITEi(1, "decoder_model_present_for_this_op")) {
					//operating_parameters_info
					REWRITEi(buffer_delay_length_minus_1+1, "decoder_buffer_delay");
					REWRITEi(buffer_delay_length_minus_1+1, "encoder_buffer_delay");
					REWRITEi(1, "low_delay_mode_flag");
				}
			}
			if (initial_display_delay_present_flag) {
				Bool initial_display_delay_present_for_this_op = REWRITEi(1, "initial_display_delay_present_for_this_op");
				if (initial_display_delay_present_for_this_op) {
					REWRITEi(4, "initial_display_delay_minus1_for_this_op_i");
				}
			}
		}
	}

	//update size
	{
		u32 frame_width_bits = gf_bs_read_int(bs_ori, 4) + 1;
		u32 frame_height_bits = gf_bs_read_int(bs_ori, 4) + 1;
		gf_bs_read_int(bs_ori, frame_width_bits);
		gf_bs_read_int(bs_ori, frame_height_bits);
		u32 width = ctx->out_width;
		u32 height = ctx->out_height;
		frame_width_bits = MAX(1, count_bits(width-1));
		frame_height_bits = MAX(1, count_bits(height-1));

		gf_bs_write_int(bs_new, frame_width_bits - 1, 4);
		gf_bs_write_int(bs_new, frame_height_bits - 1, 4);
		gf_bs_write_int(bs_new, width - 1, frame_width_bits);
		gf_bs_write_int(bs_new, height - 1, frame_height_bits);
	}
}

u64 av1_bit_position(GF_BitStream *bs);
//Copies bits up to an absolute source position; assumes a valid range and does not check bitstream errors.
static void av1merge_copy_bits(GF_BitStream *src, GF_BitStream *dst, u64 end)
{
	while (av1_bit_position(src) < end) {
		u32 bits = (u32) MIN(end - av1_bit_position(src), 32);
		bs_rewrite(src, dst, bits);
	}
}

//Copies the remaining OBU bits and regenerates byte padding; requires a nonempty OBU with valid trailing bits.
static void av1merge_copy_trailing_bits(GF_BitStream *src, GF_BitStream *dst, const u8 *data, u32 end)
{
	u64 end_bits = (u64) end * 8;
	u8 last = data[end-1];
	//trailing_one_bit followed by zero padding
	while (last && !(last & 1)) {
		end_bits--;
		last >>= 1;
	}
	av1merge_copy_bits(src, dst, end_bits);
	gf_bs_align(dst);
}

//Appends an OBU with its final LEB128 payload size and resets the payload writer; no local error reporting.
static void av1merge_flush_obu(GF_BitStream *payload, GF_BitStream *dst, const u8 *header, u32 header_size)
{
	u8 *data = NULL;
	u32 size = 0;
	gf_bs_get_content(payload, &data, &size);
	gf_bs_reassign_buffer(payload, NULL, 0);
	gf_bs_write_data(dst, header, header_size);
	gf_av1_leb128_write(dst, size);
	gf_bs_write_data(dst, data, size);
	gf_free(data);
}

//Normalizes av1C sequence dimensions; returns config parsing, allocation or serialization errors.
//Sequence rewriting may also assert on unsupported timing.
static GF_Err av1merge_rewrite_config(GF_AV1MergeCtx *ctx, u8 **new_dsi, u32 *new_size)
{
	u32 i;
	GF_AV1Config *av1c = gf_odf_av1_cfg_read_bs(ctx->bs_in);
	if (!av1c) return GF_NON_COMPLIANT_BITSTREAM;

	for (i = 0; i < gf_list_count(av1c->obu_array); ++i) {
		GF_AV1_OBUArrayEntry *obu = gf_list_get(av1c->obu_array, i);
		if (obu->obu_type == OBU_SEQUENCE_HEADER) {
			u32 header_size = 1 + ((obu->obu[0] >> 2) & 1);
			GF_BitStream *src = gf_bs_new(obu->obu, obu->obu_length, GF_BITSTREAM_READ);
			GF_BitStream *dst = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			if (!src || !dst) {
				gf_bs_del(src);
				gf_bs_del(dst);
				gf_odf_av1_cfg_del(av1c);
				return GF_OUT_OF_MEM;
			}
			gf_bs_seek(src, header_size);
			gf_av1_leb128_read(src, NULL);
			av1merge_rewrite_seq_hdr(ctx, src, ctx->bs_out);
			av1merge_copy_trailing_bits(src, ctx->bs_out, obu->obu, obu->obu_length);
			av1merge_flush_obu(ctx->bs_out, dst, obu->obu, header_size);
			gf_free(obu->obu);
			u32 size;
			gf_bs_get_content(dst, &obu->obu, &size);
			obu->obu_length = size;
			gf_bs_del(src);
			gf_bs_del(dst);
		}
	}
	GF_Err e = gf_odf_av1_cfg_write(av1c, new_dsi, new_size);
	gf_odf_av1_cfg_del(av1c);
	return e;
}

void gf_bs_log_idx(GF_BitStream *bs, u32 nBits, const char *fname, s64 val, s32 idx1, s32 idx2, s32 idx3);
#define gf_bs_log(_bs, _nBits, _fname, _val) gf_bs_log_idx(_bs, _nBits, _fname, _val, -1, -1, -1)

static u32 gf_bs_read_int_log_idx3(GF_BitStream *bs, u32 nBits, const char *fname, s32 idx1, s32 idx2, s32 idx3)
{
	u32 val = gf_bs_read_int(bs, nBits);
	gf_bs_log_idx(bs, nBits, fname, val, idx1, idx2, idx3);
	return val;
}

//Reads AV1 ns(n) with optional logging; requires positive n and asserts on unsupported bit widths.
static u32 av1_read_ns(GF_BitStream *bs, u32 n, const char *fname)
{
	u32 v, res;
	Bool extra_bit;
	int w = (u32)(log(n) / log(2)) + 1;
	u32 m = (1 << w) - n;
	gf_assert(w < 32);
	v = gf_bs_read_int(bs, w - 1);
	if (v < m) {
		if (fname) {
			gf_bs_log(bs, w-1, fname, v);
		}
		return v;
	}
	extra_bit = gf_bs_read_int(bs, 1);
	res = (v << 1) - m + extra_bit;
	if (fname) {
		gf_bs_log(bs, w, fname, res);
	}
	return res;
}

//Writes ns(n) as per AV1 section 4.10.7; caller must supply positive n and value < n.
static void av1merge_write_ns(GF_BitStream *bs, u32 n, u32 value)
{
	u32 w = count_bits(n);
	u32 m = (1U << w) - n;
	if (value < m) {
		gf_bs_write_int(bs, value, w-1);
	} else {
		value += m;
		gf_bs_write_int(bs, value >> 1, w-1);
		gf_bs_write_int(bs, value & 1, 1);
	}
}

//Consumes input tile_info and writes explicit merged spacing; rejects a zero-width input layout
//or output tiles exceeding AV1 width/area-derived height limits.
static GF_Err av1merge_rewrite_tile_info(GF_AV1MergeCtx *ctx, AV1State *state)
{
#define MAX_TILE_WIDTH 4096
#define MAX_TILE_AREA (4096 * 2304)
#define gf_bs_read_int_log(_bs, _nBits, _fname) gf_bs_read_int_log_idx3(_bs, _nBits, _fname, -1, -1, -1)

	u32 aom_av1_tile_log2(u32 blkSize, u32 target);
	u32 i;
	u32 MiCols = 2 * ((state->width + 7) >> 3);
	u32 MiRows = 2 * ((state->height + 7) >> 3);
	u32 sbCols = state->use_128x128_superblock ? ((MiCols + 31) >> 5) : ((MiCols + 15) >> 4);
	u32 sbRows = state->use_128x128_superblock ? ((MiRows + 31) >> 5) : ((MiRows + 15) >> 4);
	u32 sbShift = state->use_128x128_superblock ? 5 : 4;
	u32 sbSize = sbShift + 2;
	u32 maxTileWidthSb = MAX_TILE_WIDTH >> sbSize;
	u32 maxTileAreaSb = MAX_TILE_AREA >> (2 * sbSize);
	u32 minLog2tileCols = aom_av1_tile_log2(maxTileWidthSb, sbCols);
	u32 maxLog2tileCols = aom_av1_tile_log2(1, MIN(sbCols, AV1_MAX_TILE_COLS));
	u32 maxLog2tileRows = aom_av1_tile_log2(1, MIN(sbRows, AV1_MAX_TILE_ROWS));
	u32 minLog2Tiles = MAX(minLog2tileCols, aom_av1_tile_log2(maxTileAreaSb, sbRows * sbCols));

	//read
	Bool uniform_tile_spacing_flag = gf_bs_read_int(ctx->bs_in, 1);
	if (uniform_tile_spacing_flag) {
		u32 startSb, tileWidthSb, tileHeightSb, minLog2tileRows;
		state->tileColsLog2 = minLog2tileCols;
		while (state->tileColsLog2 < maxLog2tileCols) {
			Bool increment_tile_cols_log2 = gf_bs_read_int_log(ctx->bs_in, 1, "increment_tile_cols_log2");
			if (increment_tile_cols_log2 == 1)
				state->tileColsLog2++;
			else
				break;
		}

		tileWidthSb = (sbCols + (1 << state->tileColsLog2) - 1) >> state->tileColsLog2;
		i = 0;
		for (startSb = 0; startSb < sbCols; startSb += tileWidthSb) {
			i += 1;
		}
		state->tileCols = i;
		minLog2tileRows = MAX((int)(minLog2Tiles - state->tileColsLog2), 0);
		state->tileRowsLog2 = minLog2tileRows;
		while (state->tileRowsLog2 < maxLog2tileRows) {
			Bool increment_tile_rows_log2 = gf_bs_read_int_log(ctx->bs_in, 1, "increment_tile_rows_log2");
			if (increment_tile_rows_log2 == 1)
				state->tileRowsLog2++;
			else
				break;
		}

		tileHeightSb = (sbRows + (1 << state->tileRowsLog2) - 1) >> state->tileRowsLog2;
		i = 0;
		for (startSb = 0; startSb < sbRows; startSb += tileHeightSb) {
			i += 1;
		}
		state->tileRows = i;
	}
	else {
		u32 startSb, maxTileHeightSb, widestTileSb;
		widestTileSb = 0;
		startSb = 0;
		for (i = 0; startSb < sbCols; i++) {
			u32 maxWidth = MIN((int)(sbCols - startSb), maxTileWidthSb);
			u32 width_in_sbs_minus_1 = av1_read_ns(ctx->bs_in, maxWidth, "width_in_sbs_minus_1");
			u32 sizeSb = width_in_sbs_minus_1 + 1;
			widestTileSb = MAX(sizeSb, widestTileSb);
			startSb += sizeSb;
		}
		if (!widestTileSb) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[AV1] widest tile is 0, broken bitstream\n"));
			return GF_NON_COMPLIANT_BITSTREAM;
		}
		state->tileCols = i;
		state->tileColsLog2 = aom_av1_tile_log2(1, state->tileCols);

		if (minLog2Tiles > 0)
			maxTileAreaSb = (sbRows * sbCols) >> (minLog2Tiles + 1);
		else
			maxTileAreaSb = sbRows * sbCols;
		maxTileHeightSb = MAX(maxTileAreaSb / widestTileSb, 1);

		startSb = 0;
		for (i = 0; startSb < sbRows; i++) {
			u32 maxHeight = MIN((int)(sbRows - startSb), maxTileHeightSb);
			u32 height_in_sbs_minus_1 = av1_read_ns(ctx->bs_in, maxHeight, "height_in_sbs_minus_1");
			u32 sizeSb = height_in_sbs_minus_1 + 1;
			startSb += sizeSb;
		}

		state->tileRows = i;
		state->tileRowsLog2 = aom_av1_tile_log2(1, state->tileRows);
	}
	if (state->tileColsLog2 > 0 || state->tileRowsLog2 > 0) {
		gf_bs_read_int_log(ctx->bs_in, state->tileRowsLog2 + state->tileColsLog2, "context_update_tile_id");
		state->tile_size_bytes = gf_bs_read_int_log(ctx->bs_in, 2, "tile_size_bytes_minus1") + 1;
	}

	//explicit spacing preserves the input superblock boundaries for any grid,
	//including counts that cannot be represented with uniform spacing
	u32 sbPixels = 1U << sbSize;
	u32 widestTileSb = 0, startSb = 0;
	sbCols = (ctx->out_width + sbPixels - 1) / sbPixels;
	sbRows = (ctx->out_height + sbPixels - 1) / sbPixels;
	for (i = 0; i < ctx->cols; i++)
		widestTileSb = MAX(widestTileSb, (ctx->col_width[i] + sbPixels - 1) / sbPixels);
	minLog2tileCols = aom_av1_tile_log2(maxTileWidthSb, sbCols);
	minLog2Tiles = MAX(minLog2tileCols, aom_av1_tile_log2(MAX_TILE_AREA >> (2 * sbSize), sbRows * sbCols));
	maxTileAreaSb = sbRows * sbCols;
	if (minLog2Tiles)
		maxTileAreaSb >>= minLog2Tiles + 1;
	u32 maxTileHeightSb = MAX(maxTileAreaSb / widestTileSb, 1);
	if (widestTileSb > maxTileWidthSb)
		return GF_NOT_SUPPORTED;

	gf_bs_write_int(ctx->bs_out, 0, 1); //uniform_tile_spacing_flag
	for (i = 0; i < ctx->cols; i++) {
		u32 sizeSb = (ctx->col_width[i] + sbPixels - 1) / sbPixels;
		av1merge_write_ns(ctx->bs_out, MIN(sbCols - startSb, maxTileWidthSb), sizeSb-1);
		startSb += sizeSb;
	}
	startSb = 0;
	for (i = 0; i < ctx->rows; i++) {
		u32 sizeSb = (ctx->row_height[i] + sbPixels - 1) / sbPixels;
		if (sizeSb > maxTileHeightSb) return GF_NOT_SUPPORTED;
		av1merge_write_ns(ctx->bs_out, MIN(sbRows - startSb, maxTileHeightSb), sizeSb-1);
		startSb += sizeSb;
	}
	if (ctx->cols > 1 || ctx->rows > 1) {
		AV1TilePidCtx *first = gf_list_get(ctx->ordered_ipids, 0);
		gf_bs_write_int(ctx->bs_out, first->tile_id-1, count_bits(ctx->cols-1) + count_bits(ctx->rows-1)); //context_update_tile_id
		gf_bs_write_int(ctx->bs_out, ctx->tile_size_bytes-1, 2);
	}
	return GF_OK;
}

//Copies superresolution syntax and computes input coded width; no local validation or error return.
static void superres_params(GF_BitStream *bs_ori, GF_BitStream *bs_new, AV1State *state)
{
	u32 SuperresDenom;
	Bool use_superres;

	if (state->enable_superres) {
		use_superres = REWRITEi(1, "use_superres");
	}
	else {
		use_superres = GF_FALSE;
	}
	if (use_superres) {
		u8 coded_denom = REWRITEi(AV1_SUPERRES_DENOM_BITS, "coded_denom");
		SuperresDenom = coded_denom + AV1_SUPERRES_DENOM_MIN;
	}
	else {
		SuperresDenom = AV1_SUPERRES_NUM;
	}
	state->UpscaledWidth = state->width;
	state->width = (state->UpscaledWidth * AV1_SUPERRES_NUM + (SuperresDenom / 2)) / SuperresDenom;
}

//Rewrites explicit frame dimensions while retaining input dimensions for parsing; assumes validated sizes.
static void av1_frame_size(GF_AV1MergeCtx *ctx, GF_BitStream *bs_ori, GF_BitStream *bs_new, AV1State *state, Bool frame_size_override_flag)
{
	if (frame_size_override_flag) {
		u32 frame_width_minus_1, frame_height_minus_1;
		u8 n = state->frame_width_bits_minus_1 + 1;
		frame_width_minus_1 = gf_bs_read_int(bs_ori, n);
		gf_bs_write_int(bs_new, ctx->out_width - 1, MAX(1, count_bits(ctx->out_width - 1)));
		n = state->frame_height_bits_minus_1 + 1;
		frame_height_minus_1 = gf_bs_read_int(bs_ori, n);
		gf_bs_write_int(bs_new, ctx->out_height - 1, MAX(1, count_bits(ctx->out_height - 1)));
		state->width = frame_width_minus_1 + 1;
		state->height = frame_height_minus_1 + 1;
	} else {
		state->width = state->sequence_width;
		state->height = state->sequence_height;
	}
	superres_params(bs_ori, bs_new, state);
	//compute_image_size(); //no bits
}

//Replaces input render dimensions with merged render dimensions; assumes validated sizes and reports no errors.
static void av1_render_size(GF_AV1MergeCtx *ctx, GF_BitStream *bs_ori, GF_BitStream *bs_new)
{
	if (gf_bs_read_int(bs_ori, 1)) {
		gf_bs_read_int(bs_ori, 16);
		gf_bs_read_int(bs_ori, 16);
	}
	Bool different = ctx->render_width != ctx->out_width || ctx->render_height != ctx->out_height;
	gf_bs_write_int(bs_new, different, 1);
	if (different) {
		gf_bs_write_int(bs_new, ctx->render_width - 1, 16);
		gf_bs_write_int(bs_new, ctx->render_height - 1, 16);
	}
}

//Copies reference-based size selection or rewrites explicit sizes; assumes valid reference indices and state.
static void frame_size_with_refs(GF_AV1MergeCtx *ctx, GF_BitStream *bs_ori, GF_BitStream *bs_new, AV1State *state, Bool frame_size_override_flag, s8 *ref_frame_idx)
{
	Bool found_ref = GF_FALSE;
	u32 i = 0;
	for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
		found_ref = REWRITEi(1, "found_ref");
		if (found_ref == 1) {
			state->UpscaledWidth = state->RefUpscaledWidth[ref_frame_idx[i]];
			state->width = state->UpscaledWidth;
			state->height = state->RefFrameHeight[ref_frame_idx[i]];
			break;
		}
	}
	if (found_ref == 0) {
		av1_frame_size(ctx, bs_ori, bs_new, state, frame_size_override_flag);
		av1_render_size(ctx, bs_ori, bs_new);
	}
	else {
		superres_params(bs_ori, bs_new, state);
		//compute_image_size();
	}
}

//Rewrites the frame header through tile_info, preserving reference syntax; propagates tile layout errors.
//The caller copies the remaining header fields and checks input parsing separately.
static GF_Err av1merge_rewrite_uncompressed_header(GF_AV1MergeCtx *ctx, AV1State *state)
{
	Bool error_resilient_mode = GF_FALSE, allow_screen_content_tools = GF_FALSE, force_integer_mv = GF_FALSE;
	Bool /*use_ref_frame_mvs = GF_FALSE,*/ FrameIsIntra = GF_FALSE, frame_size_override_flag = GF_FALSE;
	Bool disable_cdf_update = GF_FALSE;
	u8 primary_ref_frame;
	u16 idLen = state->additional_frame_id_length_minus_1 + state->delta_frame_id_length_minus_2 + 3;
	s8 ref_frame_idx[AV1_REFS_PER_FRAME] = {0};
	AV1StateFrame *frame_state = &state->frame_state;

	if (state->reduced_still_picture_header) {
		FrameIsIntra = GF_TRUE;
		frame_state->frame_type = AV1_KEY_FRAME;
		frame_state->show_frame = GF_TRUE;
		frame_state->show_existing_frame = GF_FALSE;
	} else {
		frame_state->show_existing_frame = REWRITE(1, "show_existing_frame");
		if (frame_state->show_existing_frame == GF_TRUE) {
			REWRITE(3, "frame_to_show_map_idx");
			//frame_state->frame_type = state->RefFrameType[frame_state->frame_to_show_map_idx];
			//frame_state->order_hint = state->RefOrderHint[frame_state->frame_to_show_map_idx];
			if (state->decoder_model_info_present_flag && !state->equal_picture_interval) {
				REWRITE(state->frame_presentation_time_length, "frame_presentation_time");
			}

			frame_state->refresh_frame_flags = 0;
			if (state->frame_id_numbers_present_flag) {
				REWRITE(idLen, "display_frame_id");
			}
			/*if (frame_state->frame_type == AV1_KEY_FRAME) {
				frame_state->refresh_frame_flags = AV1_ALL_FRAMES;
			}*/
			/*
			if (film_grain_params_present) {
				load_grain_params(frame_to_show_map_idx)
			}*/
			return GF_OK;
		}
		frame_state->frame_type = REWRITE(2, "frame_type");
		FrameIsIntra = (frame_state->frame_type == AV1_INTRA_ONLY_FRAME || frame_state->frame_type == AV1_KEY_FRAME);
		frame_state->show_frame = REWRITE(1, "show_frame");
		if (frame_state->is_first_frame) {
			frame_state->key_frame = frame_state->seen_seq_header && frame_state->show_frame && frame_state->frame_type == AV1_KEY_FRAME && frame_state->seen_frame_header;
		}
		if (frame_state->frame_type == AV1_SWITCH_FRAME) {
			frame_state->switch_frame = GF_TRUE;
		}
		if (frame_state->show_frame && state->decoder_model_info_present_flag && !state->equal_picture_interval) {
			REWRITE(state->frame_presentation_time_length, "frame_presentation_time");
		}
		if (!frame_state->show_frame)
			REWRITE(1, "showable_frame");
		if (frame_state->frame_type == AV1_SWITCH_FRAME || (frame_state->frame_type == AV1_KEY_FRAME && frame_state->show_frame))
			error_resilient_mode = GF_TRUE;
		else
			error_resilient_mode = REWRITE(1, "error_resilient_mode");
	}

	disable_cdf_update = REWRITE(1, "disable_cdf_update");
	if (state->seq_force_screen_content_tools == 2/*SELECT_SCREEN_CONTENT_TOOLS*/) {
		allow_screen_content_tools = REWRITE(1, "allow_screen_content_tools");
	}
	else {
		allow_screen_content_tools = state->seq_force_screen_content_tools;
	}
	if (allow_screen_content_tools) {
		if (state->seq_force_integer_mv == 2/*SELECT_INTEGER_MV*/) {
			force_integer_mv = REWRITE(1, "force_integer_mv");
		}
		else {
			force_integer_mv = state->seq_force_integer_mv;
		}
	}
	else {
		force_integer_mv = 0;
	}
	if (FrameIsIntra) {
		force_integer_mv = 1;
	}
	if (state->frame_id_numbers_present_flag) {
		REWRITE(idLen, "current_frame_id");
	}
	if (frame_state->frame_type == AV1_SWITCH_FRAME)
		frame_size_override_flag = GF_TRUE;
	else if (state->reduced_still_picture_header)
		frame_size_override_flag = GF_FALSE;
	else
		frame_size_override_flag = REWRITE(1, "frame_size_override_flag");

	frame_state->order_hint = REWRITE(state->OrderHintBits, "order_hint");
	if (FrameIsIntra || error_resilient_mode) {
		primary_ref_frame = AV1_PRIMARY_REF_NONE;
	}
	else {
		primary_ref_frame = REWRITE(3, "primary_ref_frame");
	}

	if (state->decoder_model_info_present_flag) {
		u8 buffer_removal_time_present_flag = REWRITE(1, "buffer_removal_time_present_flag");
		if (buffer_removal_time_present_flag) {
			u32 opNum;
			for (opNum = 0; opNum < state->operating_points_count; opNum++) {
				if (state->decoder_model_present_for_this_op[opNum]) {
					u8 opPtIdc = state->operating_point_idc[opNum];
					u8 inTemporalLayer = (opPtIdc >> state->temporal_id) & 1;
					u8 inSpatialLayer = (opPtIdc >> (state->spatial_id + 8)) & 1;
					if (opPtIdc == 0 || (inTemporalLayer && inSpatialLayer)) {
						REWRITE(state->buffer_removal_time_length, "buffer_removal_time");
					}
				}
			}
		}
	}

	if (frame_state->frame_type == AV1_SWITCH_FRAME || (frame_state->frame_type == AV1_KEY_FRAME && frame_state->show_frame)) {
		frame_state->refresh_frame_flags = AV1_ALL_FRAMES;
	}
	else {
		frame_state->refresh_frame_flags = REWRITE(8, "refresh_frame_flags");
	}
	if (!FrameIsIntra || frame_state->refresh_frame_flags != AV1_ALL_FRAMES) {
		if (error_resilient_mode && state->enable_order_hint) {
			u32 i = 0;
			for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
				u8 ref_order_hint = REWRITE(state->OrderHintBits, "ref_order_hint");
				if (ref_order_hint != state->RefOrderHint[i]) {
					state->RefValid[i] = 0;
				}
				state->RefOrderHint[i] = ref_order_hint;
			}
		}
	}

	if (frame_state->frame_type == AV1_KEY_FRAME) {
		av1_frame_size(ctx, ctx->bs_in, ctx->bs_out, state, frame_size_override_flag);
		av1_render_size(ctx, ctx->bs_in, ctx->bs_out);
		if (allow_screen_content_tools && state->UpscaledWidth == state->width) {
			REWRITE(1, "allow_intrabc");
		}
	}
	else {
		if (frame_state->frame_type == AV1_INTRA_ONLY_FRAME) {
			av1_frame_size(ctx, ctx->bs_in, ctx->bs_out, state, frame_size_override_flag);
			av1_render_size(ctx, ctx->bs_in, ctx->bs_out);
			if (allow_screen_content_tools && state->UpscaledWidth == state->width) {
				REWRITE(1, "allow_intrabc");
			}
		}
		else {
			u32 i = 0;
			Bool frame_refs_short_signaling = GF_FALSE;
			if (state->enable_order_hint) {
				frame_refs_short_signaling = REWRITE(1, "frame_refs_short_signaling");
				if (frame_refs_short_signaling) {
					REWRITE(3, "last_frame_idx");
					REWRITE(3, "gold_frame_idx");
					//unused: av1_set_frame_refs(state, last_frame_idx, gold_frame_idx, ref_frame_idx);
				}
			}
			for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
				if (!frame_refs_short_signaling)
					ref_frame_idx[i] = REWRITE(3, "ref_frame_idx");

				if (state->frame_id_numbers_present_flag) {
					u32 n = state->delta_frame_id_length_minus_2 + 2;
					/*delta_frame_id_minus_1 =*/ REWRITE(n, "delta_frame_id_minus1");
					//DeltaFrameId = delta_frame_id_minus_1 + 1;
					//expectedFrameId[i] = ((current_frame_id + (1 << idLen) - DeltaFrameId) % (1 << idLen));
				}
			}
			if (frame_size_override_flag && !error_resilient_mode) {
				frame_size_with_refs(ctx, ctx->bs_in, ctx->bs_out, state, frame_size_override_flag, ref_frame_idx);
			}
			else {
				av1_frame_size(ctx, ctx->bs_in, ctx->bs_out, state, frame_size_override_flag);
				av1_render_size(ctx, ctx->bs_in, ctx->bs_out);
			}
			frame_state->allow_high_precision_mv = 0;
			if (!force_integer_mv) {
				frame_state->allow_high_precision_mv = REWRITE(1, "allow_high_precision_mv");
			}

			//read_interpolation_filter
			if (!REWRITE(1, "is_filter_switchable")) {
				REWRITE(2, "interpolation_filter");
			}

			REWRITE(1, "is_motion_mode_switchable");
			if (!(error_resilient_mode || !state->enable_ref_frame_mvs)) {
				REWRITE(1, "use_ref_frame_mvs");
			}
		}
	}

	if (!FrameIsIntra) {
		u32 i;
		for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
			u8 refFrame = AV1_LAST_FRAME + i;
			u8 ridx = ref_frame_idx[i];
			if (ridx >= 0) {
				u8 hint = state->RefOrderHint[ridx];
				state->OrderHints[refFrame] = hint;
				/*			if (!enable_order_hint) {
								RefFrameSignBias[refFrame] = 0;
							} else {
								RefFrameSignBias[refFrame] = get_relative_dist(hint, OrderHint) > 0;
							}
				*/
			}

		}
	}

	if (!(state->reduced_still_picture_header || disable_cdf_update))
		REWRITE(1, "disable_frame_end_update_cdf");

	if (primary_ref_frame == AV1_PRIMARY_REF_NONE) {
		//init_non_coeff_cdfs();
		//unused: av1_setup_past_independence(state);
	}
	else {
		//load_cdfs(ref_frame_idx[primary_ref_frame]);
		//unused: av1_load_previous(state, primary_ref_frame, ref_frame_idx);
	}

	return av1merge_rewrite_tile_info(ctx, state);
}

//Writes a tile-group payload with size prefixes and optional explicit range; assumes valid contiguous tiles.
static void av1merge_write_tile_group(GF_AV1MergeCtx *ctx, u32 first, u32 last)
{
	u32 tile_num, num_tiles = ctx->cols * ctx->rows;
	if (num_tiles > 1) {
		gf_bs_write_int(ctx->bs_out, ctx->has_gaps, 1); //tile_start_and_end_present_flag
		if (ctx->has_gaps) {
			u32 bits = count_bits(ctx->cols-1) + count_bits(ctx->rows-1);
			AV1TilePidCtx *start = gf_list_get(ctx->ordered_ipids, first);
			AV1TilePidCtx *end = gf_list_get(ctx->ordered_ipids, last);
			gf_bs_write_int(ctx->bs_out, start->tile_id-1, bits);
			gf_bs_write_int(ctx->bs_out, end->tile_id-1, bits);
		}
	}
	gf_bs_align(ctx->bs_out);
	for (tile_num = first; tile_num <= last; tile_num++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, tile_num);
		if (tile_num < last) {
			u32 i;
			//tile_size_minus_1 is little endian
			for (i = 0; i < ctx->tile_size_bytes; i++)
				gf_bs_write_u8(ctx->bs_out, (tile->last_size-1) >> (8*i));
		}
		gf_bs_write_data(ctx->bs_out, ctx->buffer + tile->tile_start, tile->last_size);
	}
}

//Emits one OBU per populated contiguous raster range, preserving extension bits; assumes validated input offsets.
static void av1merge_write_sparse_groups(GF_AV1MergeCtx *ctx, AV1TilePidCtx *base)
{
	u32 first = 0, count = gf_list_count(ctx->ordered_ipids);
	u8 header[2];
	memcpy(header, ctx->buffer + base->obu_start, base->header_size);
	header[0] = (header[0] & 0x87) | (OBU_TILE_GROUP << 3);
	while (first < count) {
		u32 last = first;
		while (last+1 < count) {
			AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, last);
			AV1TilePidCtx *next = gf_list_get(ctx->ordered_ipids, last+1);
			if (next->tile_id != tile->tile_id+1) break;
			last++;
		}
		av1merge_write_tile_group(ctx, first, last);
		av1merge_flush_obu(ctx->bs_out, ctx->bs_pck, header, base->header_size);
		first = last+1;
	}
}

#undef REWRITE

//Identifies OBUs requiring synchronized frame processing; returns false for other types, without errors.
static Bool av1merge_frame_obu(ObuType type)
{
	return (type == OBU_FRAME) || (type == OBU_FRAME_HEADER) || (type == OBU_TILE_GROUP);
}

//Parses one input OBU and records its tile payload; returns EOS at packet end.
//Rejects malformed OBUs, unsupported types/sizes, multi-tile frames, superresolution or missing reference sizes.
static GF_Err av1merge_next_obu(GF_AV1MergeCtx *ctx, AV1TilePidCtx *tile)
{
	GF_Err e;
	u64 size, end;
	AV1State *state = &tile->state;
	void av1_parse_frame_header(GF_BitStream *bs, AV1State *state);
	GF_Err av1_parse_tile_group(GF_BitStream *bs, AV1State *state, u64 obu_start, u64 obu_size);
	if (tile->obu_pos == tile->packet_end) return GF_EOS;
	tile->obu_start = tile->obu_pos;
	gf_bs_seek(ctx->bs_in, tile->obu_start);
	e = gf_av1_parse_obu_header(ctx->bs_in, &tile->obu_type, &state->obu_extension_flag,
		&state->obu_has_size_field, &state->temporal_id, &state->spatial_id);
	if (e) return e;
	if (!state->obu_has_size_field) return GF_NOT_SUPPORTED;
	if (tile->obu_type == OBU_REDUNDANT_FRAME_HEADER || tile->obu_type == OBU_TILE_LIST)
		return GF_NOT_SUPPORTED;
	tile->header_size = (u32) gf_bs_get_position(ctx->bs_in) - tile->obu_start;
	size = gf_av1_leb128_read(ctx->bs_in, NULL);
	tile->payload_start = (u32) gf_bs_get_position(ctx->bs_in);
	if (tile->payload_start > tile->packet_end || size > tile->packet_end - tile->payload_start)
		return GF_NON_COMPLIANT_BITSTREAM;
	end = tile->payload_start + size;
	tile->obu_pos = (u32) end;
	tile->last_size = 0;
	if (tile->obu_type == OBU_FRAME || tile->obu_type == OBU_FRAME_HEADER) {
		AV1ParseOffsets offsets = {0};
		offsets.frame_size_ref = -1;
		state->parse_offsets = &offsets;
		av1_parse_frame_header(ctx->bs_in, state);
		state->parse_offsets = NULL;
		tile->header_end_bits = av1_bit_position(ctx->bs_in);
		if (tile->header_end_bits > end * 8 || gf_bs_is_overflow(ctx->bs_in))
			return GF_NON_COMPLIANT_BITSTREAM;
		if (!state->frame_state.show_existing_frame && (state->tileCols != 1 || state->tileRows != 1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Each input frame must contain one tile\n"));
			return GF_NOT_SUPPORTED;
		}
		if (state->width != tile->width || state->height != tile->height || state->UpscaledWidth != state->width)
			return GF_NOT_SUPPORTED;
		if (!state->frame_state.show_existing_frame) {
			//base_q_idx immediately follows tile_info().
			if (offsets.tile_info_end < (u64) tile->payload_start * 8
				|| offsets.tile_info_end + 8 > tile->header_end_bits)
				return GF_NON_COMPLIANT_BITSTREAM;
			gf_bs_seek(ctx->bs_in, offsets.tile_info_end / 8);
			if (offsets.tile_info_end % 8) gf_bs_read_int(ctx->bs_in, offsets.tile_info_end % 8);
			tile->base_q_idx = gf_bs_read_int(ctx->bs_in, 8);
			gf_bs_seek(ctx->bs_in, tile->header_end_bits / 8);
			if (tile->header_end_bits % 8) gf_bs_read_int(ctx->bs_in, tile->header_end_bits % 8);
			if (offsets.render_size) {
				gf_bs_seek(ctx->bs_in, offsets.render_size / 8);
				if (offsets.render_size % 8) gf_bs_read_int(ctx->bs_in, offsets.render_size % 8);
				tile->render_width = tile->width;
				tile->render_height = tile->height;
				if (gf_bs_read_int(ctx->bs_in, 1)) {
					tile->render_width = gf_bs_read_int(ctx->bs_in, 16) + 1;
					tile->render_height = gf_bs_read_int(ctx->bs_in, 16) + 1;
				}
				gf_bs_seek(ctx->bs_in, tile->header_end_bits / 8);
				if (tile->header_end_bits % 8) gf_bs_read_int(ctx->bs_in, tile->header_end_bits % 8);
			} else if (offsets.frame_size_ref >= 0) {
				u32 ref = offsets.frame_size_ref;
				tile->render_width = tile->ref_render_width[ref];
				tile->render_height = tile->ref_render_height[ref];
				if (!tile->render_width || !tile->render_height) return GF_NON_COMPLIANT_BITSTREAM;
			}
			u32 i;
			for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
				if (state->frame_state.refresh_frame_flags & (1U << i)) {
					tile->ref_render_width[i] = tile->render_width;
					tile->ref_render_height[i] = tile->render_height;
				}
			}
		}
		gf_bs_align(ctx->bs_in);
	}
	if ((tile->obu_type == OBU_FRAME && !state->frame_state.show_existing_frame) || tile->obu_type == OBU_TILE_GROUP) {
		if (!state->frame_state.seen_frame_header || state->tileCols != 1 || state->tileRows != 1)
			return GF_NON_COMPLIANT_BITSTREAM;
		e = av1_parse_tile_group(ctx->bs_in, state, tile->obu_start, end - tile->obu_start);
		if (e) return e;
		if (state->frame_state.nb_tiles_in_obu != 1 || !state->frame_state.tiles[0].size)
			return GF_NON_COMPLIANT_BITSTREAM;
		tile->tile_start = tile->obu_start + state->frame_state.tiles[0].obu_start_offset;
		tile->last_size = state->frame_state.tiles[0].size;
	}
	return GF_OK;
}

//Reassembles synchronized input OBUs; rejects incompatible frame structure/QP or ambiguous/oversized render geometry.
//Propagates parsing/layout errors; returns EOS and records the short input when a temporal unit is incomplete.
static GF_Err av1merge_rewrite_pck(GF_AV1MergeCtx *ctx)
{
	GF_Err e;
	u32 i, count = gf_list_count(ctx->ordered_ipids);
	AV1TilePidCtx *base = gf_list_get(ctx->ordered_ipids, 0);
	while ((e = av1merge_next_obu(ctx, base)) == GF_OK) {
		if (av1merge_frame_obu(base->obu_type)) {
			//ther inputs may carry different metadata, but must have the same
			//frame/OBU structure within synchronized TUs
			ctx->tile_size_bytes = (base->obu_type == OBU_FRAME) ? 1 : 4;
			for (i = 0; i < count; i++) {
				AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
				if (i) {
					do {
						e = av1merge_next_obu(ctx, tile);
						if (e == GF_EOS) {
							ctx->incomplete_tile = tile;
							return GF_EOS;
						}
						if (e) return e;
					} while (!av1merge_frame_obu(tile->obu_type));
				}
				if (tile->obu_type != base->obu_type
					|| tile->state.frame_state.show_existing_frame != base->state.frame_state.show_existing_frame
					|| tile->state.frame_state.frame_type != base->state.frame_state.frame_type
					|| tile->state.frame_state.order_hint != base->state.frame_state.order_hint
					|| tile->state.temporal_id != base->state.temporal_id || tile->state.spatial_id != base->state.spatial_id)
					return GF_NON_COMPLIANT_BITSTREAM;
				if ((base->obu_type == OBU_FRAME || base->obu_type == OBU_FRAME_HEADER)
					&& !base->state.frame_state.show_existing_frame && tile->base_q_idx != base->base_q_idx) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Tile %u base_q_idx %u differs from %u: all tiles in a frame must use the same base quantizer index\n",
						tile->tile_id, tile->base_q_idx, base->base_q_idx));
					return GF_NON_COMPLIANT_BITSTREAM;
				}
				if (tile->last_size)
					ctx->tile_size_bytes = MAX(ctx->tile_size_bytes, (count_bits(tile->last_size-1) + 7) / 8);
			}
			if ((base->obu_type == OBU_FRAME || base->obu_type == OBU_FRAME_HEADER)
				&& !base->state.frame_state.show_existing_frame) {
				u32 render_cols[AV1_MAX_TILE_COLS] = {0}, render_rows[AV1_MAX_TILE_ROWS] = {0};
				Bool custom_width = GF_FALSE, custom_height = GF_FALSE;
				ctx->render_width = ctx->render_height = 0;
				for (i = 0; i < count; i++) {
					AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
					u32 col = (tile->tile_id-1) % ctx->cols, row = (tile->tile_id-1) / ctx->cols;
					if ((render_cols[col] && tile->render_width != render_cols[col])
						|| (render_rows[row] && tile->render_height != render_rows[row])) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Render widths must match within columns and render heights within rows\n"));
						return GF_NOT_SUPPORTED;
					}
					render_cols[col] = tile->render_width;
					render_rows[row] = tile->render_height;
					if (tile->render_width != tile->width) custom_width = GF_TRUE;
					if (tile->render_height != tile->height) custom_height = GF_TRUE;
				}
				for (i = 0; i < ctx->cols; i++) {
					if (!render_cols[i] && custom_width) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Cannot derive the render width of empty column %u from custom input render sizes\n", i+1));
						return GF_NOT_SUPPORTED;
					}
					ctx->render_width += render_cols[i] ? render_cols[i] : ctx->col_width[i];
				}
				for (i = 0; i < ctx->rows; i++) {
					if (!render_rows[i] && custom_height) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Cannot derive the render height of empty row %u from custom input render sizes\n", i+1));
						return GF_NOT_SUPPORTED;
					}
					ctx->render_height += render_rows[i] ? render_rows[i] : ctx->row_height[i];
				}
				if (ctx->render_width > 65536 || ctx->render_height > 65536)
					return GF_NOT_SUPPORTED;
			}
		}

		gf_bs_seek(ctx->bs_in, base->payload_start);
		if (base->obu_type == OBU_SEQUENCE_HEADER) {
			av1merge_rewrite_seq_hdr(ctx, ctx->bs_in, ctx->bs_out);
			av1merge_copy_trailing_bits(ctx->bs_in, ctx->bs_out, ctx->buffer, base->obu_pos);
		} else if (base->obu_type == OBU_FRAME || base->obu_type == OBU_FRAME_HEADER) {
			ctx->rewrite_state = base->state;
			e = av1merge_rewrite_uncompressed_header(ctx, &ctx->rewrite_state);
			if (e) return e;
			av1merge_copy_bits(ctx->bs_in, ctx->bs_out, base->header_end_bits);
			if (base->obu_type == OBU_FRAME_HEADER || ctx->has_gaps)
				gf_bs_write_int(ctx->bs_out, 1, 1); //trailing_one_bit
			gf_bs_align(ctx->bs_out);
			if (ctx->has_gaps && base->obu_type == OBU_FRAME) {
				u8 header[2];
				memcpy(header, ctx->buffer + base->obu_start, base->header_size);
				header[0] = (header[0] & 0x87) | (OBU_FRAME_HEADER << 3);
				av1merge_flush_obu(ctx->bs_out, ctx->bs_pck, header, base->header_size);
				if (base->last_size) av1merge_write_sparse_groups(ctx, base);
				continue;
			}
			if (base->obu_type == OBU_FRAME && base->last_size)
				av1merge_write_tile_group(ctx, 0, count-1);
		} else if (base->obu_type == OBU_TILE_GROUP) {
			if (ctx->has_gaps) {
				av1merge_write_sparse_groups(ctx, base);
				continue;
			}
			av1merge_write_tile_group(ctx, 0, count-1);
		} else {
			gf_bs_write_data(ctx->bs_out, ctx->buffer + base->payload_start, base->obu_pos - base->payload_start);
		}
		av1merge_flush_obu(ctx->bs_out, ctx->bs_pck, ctx->buffer + base->obu_start, base->header_size);
	}
	if (e != GF_EOS) return e;
	for (i = 1; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		while ((e = av1merge_next_obu(ctx, tile)) == GF_OK) {
			if (av1merge_frame_obu(tile->obu_type)) {
				ctx->incomplete_tile = base;
				return GF_EOS;
			}
		}
		if (e != GF_EOS) return e;
	}
	return GF_OK;
}

//Copies base PID properties and removes tile-specific/spatial metadata; property errors are not propagated.
static void av1merge_copy_output_properties(GF_AV1MergeCtx *ctx, GF_FilterPid *pid)
{
	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CROP_POS, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ORIG_SIZE, NULL);
	gf_filter_pid_set_property_str(ctx->opid, "av1_split", NULL);
	//The track template and spatial user data describe an individual input tile.
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_ISOM_TRACK_TEMPLATE, NULL);
	gf_filter_pid_set_property_str(ctx->opid, "udta_GSRD", NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SRD, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SRD_REF, NULL);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SRD_MAP, NULL);
}

//Tests a complete implicit grid and optionally assigns indices; returns false on size, alignment or count conflicts.
static Bool av1merge_try_implicit_grid(GF_AV1MergeCtx *ctx, u32 rows, u32 sb_size, Bool assign)
{
	u32 count = gf_list_count(ctx->ordered_ipids), start = 0, col = 0;
	u32 row_height[AV1_MAX_TILE_ROWS];
	u64 width = 0, height = 0;
	if (!rows || count % rows) return GF_FALSE;
	u32 cols = count / rows;
	if (cols > AV1_MAX_TILE_COLS || (ctx->rows_forced && rows != ctx->rows)
		|| (ctx->cols_forced && cols != ctx->cols))
		return GF_FALSE;
	while (start < count) {
		AV1TilePidCtx *first = gf_list_get(ctx->ordered_ipids, start);
		u32 end = start+1, group_cols;
		while (end < count) {
			AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, end);
			if (tile->width != first->width) break;
			end++;
		}
		if ((end-start) % rows) return GF_FALSE;
		group_cols = (end-start) / rows;
		if ((first->width % sb_size) && (group_cols > 1 || col+group_cols < cols))
			return GF_FALSE;
		width += (u64) first->width * group_cols;
		if (width > 65536) return GF_FALSE;
		for (u32 r = 0; r < rows; r++) {
			for (u32 c = 0; c < group_cols; c++) {
				AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, start + r*group_cols + c);
				if (!col && !c) {
					row_height[r] = tile->height;
					height += tile->height;
					if (height > 65536 || (r+1 < rows && tile->height % sb_size))
						return GF_FALSE;
				}
				if (tile->height != row_height[r]) return GF_FALSE;
				if (assign) {
					tile->grid_x = col+c;
					tile->grid_y = r;
				}
			}
		}
		col += group_cols;
		start = end;
	}
	return GF_TRUE;
}

//Stably sorts inputs and searches nearby row counts for a complete grid; fails on list insertion or no valid packing.
static GF_Err av1merge_implicit_grid(GF_AV1MergeCtx *ctx)
{
	u32 count = gf_list_count(ctx->ipids), target_rows = 1;
	AV1TilePidCtx *base = gf_list_get(ctx->ipids, 0);
	u32 sb_size = base->state.use_128x128_superblock ? 128 : 64;
	for (u32 i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ipids, i);
		u32 j;
		for (j = 0; j < gf_list_count(ctx->ordered_ipids); j++) {
			AV1TilePidCtx *other = gf_list_get(ctx->ordered_ipids, j);
			Bool partial_width = tile->width % sb_size ? GF_TRUE : GF_FALSE;
			Bool other_partial_width = other->width % sb_size ? GF_TRUE : GF_FALSE;
			Bool partial_height = tile->height % sb_size ? GF_TRUE : GF_FALSE;
			Bool other_partial_height = other->height % sb_size ? GF_TRUE : GF_FALSE;
			if (partial_width != other_partial_width) {
				if (!partial_width) break;
			} else if (tile->width != other->width) {
				if (tile->width < other->width) break;
			} else if (partial_height != other_partial_height) {
				if (!partial_height) break;
			} else if (tile->height < other->height) {
				break;
			}
		}
		GF_Err e = gf_list_insert(ctx->ordered_ipids, tile, j);
		if (e) return e;
	}
	//Group equal widths into columns and equal heights into common rows.
	while (target_rows < AV1_MAX_TILE_ROWS && target_rows * target_rows < count)
		target_rows++;
	for (u32 distance = 0; distance < AV1_MAX_TILE_ROWS; distance++) {
		u32 candidates[2] = {distance < target_rows ? target_rows-distance : 0, target_rows+distance};
		for (u32 i = 0; i < (distance ? 2 : 1); i++) {
			u32 rows = candidates[i];
			if (!rows || rows > AV1_MAX_TILE_ROWS) continue;
			if (!av1merge_try_implicit_grid(ctx, rows, sb_size, GF_FALSE)) continue;
			av1merge_try_implicit_grid(ctx, rows, sb_size, GF_TRUE);
			return GF_OK;
		}
	}
	GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Cannot pack %u inputs into a complete AV1 grid satisfying the requested rows/columns: columns need equal widths, rows need equal heights, and only the final edges may be unaligned\n", count));
	return GF_BAD_PARAM;
}

//Builds source/destination rectangles for populated tiles; incomplete source metadata omits the map without failing.
//Rejects invalid/overflowing SRD values and propagates allocation or property-setting errors.
static GF_Err av1merge_build_srdmap(GF_AV1MergeCtx *ctx, Bool relative)
{
	u32 count = gf_list_count(ctx->ordered_ipids);
	Bool complete = GF_TRUE, has_srd = GF_FALSE;
	GF_PropertyValue map = {0};
	for (u32 i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		const GF_PropertyValue *srd = gf_filter_pid_get_property(tile->pid, GF_PROP_PID_SRD);
		const GF_PropertyValue *ref = gf_filter_pid_get_property(tile->pid, GF_PROP_PID_SRD_REF);
		if (srd || ref) has_srd = GF_TRUE;
		if (!(srd && ref) && !(tile->has_pos && !relative)) complete = GF_FALSE;
	}
	if (!complete) {
		if (has_srd)
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[AV1Merge] SRDMap omitted: every input needs SRD/SRDRef or an absolute CropOrigin to describe its source rectangle\n"));
		return GF_OK;
	}
	map.type = GF_PROP_UINT_LIST;
	map.value.uint_list.nb_items = count * 8;
	map.value.uint_list.vals = gf_malloc(sizeof(u32) * count * 8);
	if (!map.value.uint_list.vals) return GF_OUT_OF_MEM;
	for (u32 i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		const GF_PropertyValue *srd = gf_filter_pid_get_property(tile->pid, GF_PROP_PID_SRD);
		const GF_PropertyValue *ref = gf_filter_pid_get_property(tile->pid, GF_PROP_PID_SRD_REF);
		u32 *rect = &map.value.uint_list.vals[i*8];
		if (srd && ref) {
			s32 source[4] = {srd->value.vec4i.x, srd->value.vec4i.y, srd->value.vec4i.z, srd->value.vec4i.w};
			if (source[0] < 0 || source[1] < 0 || source[2] <= 0 || source[3] <= 0
				|| ref->value.vec2i.x <= 0 || ref->value.vec2i.y <= 0) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Invalid SRD/SRDRef on tile %u: source coordinates must be nonnegative and sizes and reference dimensions must be positive\n", tile->tile_id));
				gf_free(map.value.uint_list.vals);
				return GF_BAD_PARAM;
			}
			for (u32 j = 0; j < 4; j++) {
				u64 value = (u64) source[j] * (j % 2 ? ctx->out_height : ctx->out_width)
					/ (j % 2 ? ref->value.vec2i.y : ref->value.vec2i.x);
				if (value > GF_UINT_MAX) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] SRD on tile %u exceeds the SRDMap coordinate range after scaling\n", tile->tile_id));
					gf_free(map.value.uint_list.vals);
					return GF_BAD_PARAM;
				}
				rect[j] = (u32) value;
			}
		} else {
			rect[0] = tile->pos_x;
			rect[1] = tile->pos_y;
			rect[2] = tile->width;
			rect[3] = tile->height;
		}
		rect[4] = tile->grid_x;
		rect[5] = tile->grid_y;
		rect[6] = tile->width;
		rect[7] = tile->height;
	}
	GF_Err e = gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SRD_MAP, &map);
	gf_free(map.value.uint_list.vals);
	if (e) return e;
	return gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SRD_REF, &PROP_VEC2I_INT(ctx->out_width, ctx->out_height));
}

//Inserts a unique sorted axis boundary; duplicates succeed, but a new value fails when capacity is exhausted.
static GF_Err av1merge_insert_boundary(u32 *values, u32 *count, u32 capacity, u32 value)
{
	u32 i;
	for (i = 0; i < *count; i++) {
		if (values[i] == value) return GF_OK;
		if (values[i] > value) break;
	}
	if (*count == capacity) return GF_BAD_PARAM;
	memmove(values+i+1, values+i, (*count-i) * sizeof(u32));
	values[i] = value;
	(*count)++;
	return GF_OK;
}

//Resolves placement, validates grid geometry and normalized coding configs, then publishes merged properties.
//Rejects mixed/overlapping/subdivided layouts, invalid sizes/alignment/counts or incompatible configs; propagates helper errors.
static GF_Err av1merge_rebuild_grid(GF_AV1MergeCtx *ctx)
{
	u32 i, count = gf_list_count(ctx->ipids);
	u32 cols = 0, rows = 0, positioned = 0;
	u32 col_origin[AV1_MAX_TILE_COLS+1], row_origin[AV1_MAX_TILE_ROWS+1];
	Bool relative = GF_FALSE, absolute = GF_FALSE;
	Bool had_gaps = ctx->has_gaps;
	GF_Err e;
	AV1TilePidCtx *base;
	u8 *new_dsi = NULL;
	u32 new_size = 0;
	if (!count) return GF_BAD_PARAM;
	if (count > AV1_MAX_TILE_COLS * AV1_MAX_TILE_ROWS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Too many inputs for an AV1 tile grid\n"));
		return GF_BAD_PARAM;
	}

	gf_list_reset(ctx->ordered_ipids);
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ipids, i);
		if (tile->has_pos) {
			positioned++;
			if (tile->pos_x < 0 || tile->pos_y < 0) relative = GF_TRUE;
			if (tile->pos_x > 0 || tile->pos_y > 0) absolute = GF_TRUE;
		}
	}
	if (positioned && positioned != count) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Cannot mix explicit and implicit positioning: CropOrigin must be set on every input or none\n"));
		return GF_BAD_PARAM;
	}
	if (relative && absolute) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Cannot mix relative and absolute CropOrigin coordinates: relative row/column indices must be zero or negative on both axes\n"));
		return GF_BAD_PARAM;
	}
	Bool indexed = relative || !positioned;
	if (!positioned) {
		e = av1merge_implicit_grid(ctx);
		if (e) return e;
		gf_list_reset(ctx->ordered_ipids);
	}
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ipids, i);
		u32 j;
		if (positioned) {
			tile->grid_x = relative ? (u32) -(s64) tile->pos_x : (u32) tile->pos_x;
			tile->grid_y = relative ? (u32) -(s64) tile->pos_y : (u32) tile->pos_y;
		}
		for (j = 0; j < gf_list_count(ctx->ordered_ipids); j++) {
			AV1TilePidCtx *other = gf_list_get(ctx->ordered_ipids, j);
			if (other->grid_x == tile->grid_x && other->grid_y == tile->grid_y) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Duplicate CropOrigin %dx%d: two inputs cannot occupy the same grid cell\n", tile->pos_x, tile->pos_y));
				return GF_BAD_PARAM;
			}
			if (other->grid_y > tile->grid_y || (other->grid_y == tile->grid_y && other->grid_x > tile->grid_x))
				break;
		}
		e = gf_list_insert(ctx->ordered_ipids, tile, j);
		if (e) return e;
	}
	base = gf_list_get(ctx->ordered_ipids, 0);
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		tile->tile_id = i+1;
		if (!tile->width || !tile->height) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Tile %u has invalid dimensions %ux%u: width and height must be non-zero\n", tile->tile_id, tile->width, tile->height));
			return GF_BAD_PARAM;
		}
		if (tile->width > 65536 || tile->height > 65536) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Tile %u dimensions %ux%u exceed AV1 frame size fields\n", tile->tile_id, tile->width, tile->height));
			return GF_NOT_SUPPORTED;
		}
		if (!indexed) {
			e = av1merge_insert_boundary(col_origin, &cols, AV1_MAX_TILE_COLS+1, tile->grid_x);
			if (!e) e = av1merge_insert_boundary(col_origin, &cols, AV1_MAX_TILE_COLS+1, tile->grid_x + tile->width);
			if (!e) e = av1merge_insert_boundary(row_origin, &rows, AV1_MAX_TILE_ROWS+1, tile->grid_y);
			if (!e) e = av1merge_insert_boundary(row_origin, &rows, AV1_MAX_TILE_ROWS+1, tile->grid_y + tile->height);
			if (e) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] CropOrigin rectangles exceed the AV1 grid limits of %u columns and %u rows\n", AV1_MAX_TILE_COLS, AV1_MAX_TILE_ROWS));
				return e;
			}
			continue;
		}
		if (tile->grid_y == base->grid_y) {
			if (cols == AV1_MAX_TILE_COLS) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] CropOrigin layout exceeds the AV1 limit of %u columns\n", AV1_MAX_TILE_COLS));
				return GF_BAD_PARAM;
			}
			col_origin[cols++] = tile->grid_x;
		}
		if (!rows || row_origin[rows-1] != tile->grid_y) {
			if (rows == AV1_MAX_TILE_ROWS) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] CropOrigin layout exceeds the AV1 limit of %u rows\n", AV1_MAX_TILE_ROWS));
				return GF_BAD_PARAM;
			}
			row_origin[rows++] = tile->grid_y;
		}
	}
	if (!indexed) {
		cols--;
		rows--;
	}
	if (indexed && (u64) cols * rows != count) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] CropOrigin layout is not a complete rectangular grid: %u inputs for %u columns and %u rows\n", count, cols, rows));
		return GF_BAD_PARAM;
	}
	if ((ctx->cols_forced && ctx->cols != cols) || (ctx->rows_forced && ctx->rows != rows)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] CropOrigin layout has %u columns and %u rows, conflicting with the requested grid (%u columns, %u rows; 0 means unspecified)\n", cols, rows, ctx->cols_forced ? ctx->cols : 0, ctx->rows_forced ? ctx->rows : 0));
		return GF_BAD_PARAM;
	}
	ctx->cols = cols;
	ctx->rows = rows;
	ctx->has_gaps = (u64) cols * rows != count;
	memset(ctx->col_width, 0, sizeof(ctx->col_width));
	memset(ctx->row_height, 0, sizeof(ctx->row_height));
	if (!indexed) {
		for (i = 0; i < cols; i++) ctx->col_width[i] = col_origin[i+1] - col_origin[i];
		for (i = 0; i < rows; i++) ctx->row_height[i] = row_origin[i+1] - row_origin[i];
	}
	ctx->out_width = ctx->out_height = 0;
	u32 sb_size = base->state.use_128x128_superblock ? 128 : 64;
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		u32 col = i % ctx->cols, row = i / ctx->cols;
		if (!indexed) {
			for (col = 0; col+1 < cols && tile->grid_x != col_origin[col]; col++) {}
			for (row = 0; row+1 < rows && tile->grid_y != row_origin[row]; row++) {}
			tile->tile_id = row*cols + col+1;
			if (tile->width != ctx->col_width[col] || tile->height != ctx->row_height[row]) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Tile %u size %ux%u does not match column %u width %u and row %u height %u: input tiles cannot overlap or span multiple grid cells\n", tile->tile_id, tile->width, tile->height, col+1, ctx->col_width[col], row+1, ctx->row_height[row]));
				return GF_BAD_PARAM;
			}
		} else if (tile->grid_x != col_origin[col] || tile->grid_y != row_origin[row]) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] CropOrigin %dx%d on tile %u does not match grid cell %ux%u: every row must contain the same columns\n", tile->pos_x, tile->pos_y, tile->tile_id, col+1, row+1));
			return GF_BAD_PARAM;
		}
		if (!indexed && (tile->grid_x % sb_size || tile->grid_y % sb_size)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] CropOrigin %dx%d on tile %u must align to %u-pixel superblocks; coordinates are not rounded\n", tile->pos_x, tile->pos_y, tile->tile_id, sb_size));
			return GF_NOT_SUPPORTED;
		}
		if (tile->state.use_128x128_superblock != base->state.use_128x128_superblock) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Tile %u uses %u-pixel superblocks, expected %u: all tiles must use the same superblock size\n", tile->tile_id, tile->state.use_128x128_superblock ? 128 : 64, sb_size));
			return GF_BAD_PARAM;
		}
		if (ctx->col_width[col] && ctx->col_width[col] != tile->width) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Tile %u width %u does not match column %u width %u: AV1 requires equal tile widths within each column (different widths are allowed between columns)\n", tile->tile_id, tile->width, col+1, ctx->col_width[col]));
			return GF_BAD_PARAM;
		}
		if (ctx->row_height[row] && ctx->row_height[row] != tile->height) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Tile %u height %u does not match row %u height %u: AV1 requires equal tile heights within each row (different heights are allowed between rows)\n", tile->tile_id, tile->height, row+1, ctx->row_height[row]));
			return GF_BAD_PARAM;
		}
		ctx->col_width[col] = tile->width;
		ctx->row_height[row] = tile->height;
	}
	for (i = 0; i < ctx->cols; i++) {
		u32 offset = col_origin[i] - col_origin[0];
		if (offset != (indexed ? i : ctx->out_width)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Column %u starts at horizontal %s %u, expected %u: gaps and overlaps are not supported\n", i+1, indexed ? "index" : "offset", offset, indexed ? i : ctx->out_width));
			return GF_BAD_PARAM;
		}
		if ((i+1 < ctx->cols && ctx->col_width[i] % sb_size) || ctx->col_width[i] > 65536 - ctx->out_width)
			break;
		ctx->out_width += ctx->col_width[i];
	}
	if (i != ctx->cols) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Column widths must fit AV1 frame size fields and internal boundaries must align to superblocks\n"));
		return GF_NOT_SUPPORTED;
	}
	for (i = 0; i < ctx->rows; i++) {
		u32 offset = row_origin[i] - row_origin[0];
		if (offset != (indexed ? i : ctx->out_height)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Row %u starts at vertical %s %u, expected %u: gaps and overlaps are not supported\n", i+1, indexed ? "index" : "offset", offset, indexed ? i : ctx->out_height));
			return GF_BAD_PARAM;
		}
		if ((i+1 < ctx->rows && ctx->row_height[i] % sb_size) || ctx->row_height[i] > 65536 - ctx->out_height)
			break;
		ctx->out_height += ctx->row_height[i];
	}
	if (i != ctx->rows) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Grid dimensions must fit AV1 frame size fields and internal tile boundaries must align to superblocks\n"));
		return GF_NOT_SUPPORTED;
	}
	u32 x = 0, y = 0;
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		if (!indexed) {
			tile->grid_x -= col_origin[0];
			tile->grid_y -= row_origin[0];
			continue;
		}
		u32 col = i % cols, row = i / cols;
		if (!col) {
			x = 0;
			if (row) y += ctx->row_height[row-1];
		}
		tile->grid_x = x;
		tile->grid_y = y;
		x += ctx->col_width[col];
	}
	//Normalize only the sequence dimensions before comparing coding configurations.
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		const GF_PropertyValue *dsi = gf_filter_pid_get_property(tile->pid, GF_PROP_PID_DECODER_CONFIG);
		u8 *tile_dsi = NULL;
		u32 tile_size = 0;
		gf_bs_reassign_buffer(ctx->bs_in, dsi->value.data.ptr, dsi->value.data.size);
		e = av1merge_rewrite_config(ctx, &tile_dsi, &tile_size);
		if (!e && i && (tile_size != new_size || memcmp(tile_dsi, new_dsi, new_size))) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Coding configuration differs on tile %u\n", tile->tile_id));
			e = GF_BAD_PARAM;
		}
		if (e) {
			gf_free(tile_dsi);
			gf_free(new_dsi);
			return e;
		}
		if (!i) {
			new_dsi = tile_dsi;
			new_size = tile_size;
		} else {
			gf_free(tile_dsi);
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[AV1Merge] Grid configuration: %ux%u (columns %s, rows %s)\n", ctx->cols, ctx->rows,
		ctx->cols_forced ? "user-forced" : "automatic", ctx->rows_forced ? "user-forced" : "automatic"));
	av1merge_copy_output_properties(ctx, base->pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->out_width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->out_height));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(new_dsi, new_size));
	e = av1merge_build_srdmap(ctx, relative);
	if (e) return e;
	if (ctx->has_gaps && !had_gaps) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[AV1Merge] Sparse grid: %u of %u tiles are omitted; no filler is encoded and decoder acceptance or rendering is not guaranteed\n", cols*rows-count, cols*rows));
	}
	ctx->grid_changed = GF_FALSE;
	return GF_OK;
}

static GF_Err av1merge_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 cfg_crc, i;
	const GF_PropertyValue *p, *dsi;
	GF_AV1MergeCtx *ctx = (GF_AV1MergeCtx*) gf_filter_get_udta(filter);
	AV1TilePidCtx *tile = gf_filter_pid_get_udta(pid);
	if (is_remove) {
		if (tile) {
			if (ctx->incomplete_tile == tile) {
				ctx->incomplete_tile = NULL;
				ctx->drain = GF_TRUE;
			}
			gf_list_del_item(ctx->ipids, tile);
			gf_list_del_item(ctx->ordered_ipids, tile);
			gf_odf_av1_cfg_del(tile->state.config);
			gf_free(tile);
			gf_filter_pid_set_udta(pid, NULL);
		}
		ctx->grid_changed = GF_TRUE;
		if (!gf_list_count(ctx->ipids) && ctx->opid) {
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		return GF_OK;
	}
	if (ctx->in_error) return GF_BAD_PARAM;
	if (!gf_filter_pid_check_caps(pid)) return GF_NOT_SUPPORTED;
	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!dsi || !dsi->value.data.size) return GF_OK;
	if (!gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH) || !gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT))
		return GF_OK;
	if (!tile) {
		GF_SAFEALLOC(tile, AV1TilePidCtx);
		if (!tile) return GF_OUT_OF_MEM;
		tile->pid = pid;
		gf_filter_pid_set_udta(pid, tile);
		gf_list_add(ctx->ipids, tile);
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
		ctx->grid_changed = GF_TRUE;
		if (ctx->is_playing) {
			GF_FilterEvent evt;
			evt.play = ctx->play_evt;
			evt.base.on_pid = pid;
			gf_filter_pid_send_event(pid, &evt);
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (tile->width != p->value.uint) ctx->grid_changed = GF_TRUE;
	tile->width = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (tile->height != p->value.uint) ctx->grid_changed = GF_TRUE;
	tile->height = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CROP_POS);
	if (tile->has_pos != (p ? GF_TRUE : GF_FALSE)
		|| (p && (tile->pos_x != p->value.vec2i.x || tile->pos_y != p->value.vec2i.y)))
		ctx->grid_changed = GF_TRUE;
	tile->has_pos = p ? GF_TRUE : GF_FALSE;
	tile->pos_x = p ? p->value.vec2i.x : 0;
	tile->pos_y = p ? p->value.vec2i.y : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	tile->timescale = p ? p->value.uint : 0;

	cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
	if (!tile->state.config || cfg_crc != tile->dsi_crc) {
		gf_odf_av1_cfg_del(tile->state.config);
		memset(&tile->state, 0, sizeof(tile->state));
		tile->render_width = tile->width;
		tile->render_height = tile->height;
		memset(tile->ref_render_width, 0, sizeof(tile->ref_render_width));
		memset(tile->ref_render_height, 0, sizeof(tile->ref_render_height));
		tile->state.config = gf_odf_av1_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		if (!tile->state.config) return GF_NON_COMPLIANT_BITSTREAM;
		for (i = 0; i < gf_list_count(tile->state.config->obu_array); i++) {
			GF_AV1_OBUArrayEntry *obu = gf_list_get(tile->state.config->obu_array, i);
			if (obu->obu_type == OBU_SEQUENCE_HEADER) {
				ObuType type;
				u64 size = obu->obu_length;
				GF_Err e;
				gf_bs_reassign_buffer(ctx->bs_in, obu->obu, obu->obu_length);
				e = gf_av1_parse_obu(ctx->bs_in, &type, &size, NULL, &tile->state);
				if (e) return e;
			}
		}
		tile->dsi_crc = cfg_crc;
		ctx->grid_changed = GF_TRUE;
	}
	//SRD-only changes also require refreshing the output mapping.
	ctx->grid_changed = GF_TRUE;
	//publish a PID to connect the downstream graph; finalize its grid only
	//after all input connections and their first packets are available
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		if (!ctx->opid) return GF_OUT_OF_MEM;
		av1merge_copy_output_properties(ctx, pid);
	}
	return GF_OK;
}

static GF_Err av1merge_process(GF_Filter *filter)
{
	GF_AV1MergeCtx *ctx = (GF_AV1MergeCtx*) gf_filter_get_udta(filter);
	GF_Err e;
	u32 i, count = gf_list_count(ctx->ipids), nb_eos = 0, total_size = 0;
	u64 min_dts = GF_FILTER_NO_TS;
	u32 min_timescale = 0;
	Bool missing = GF_FALSE, discard = GF_FALSE;
	GF_FilterPacket *base_pck, *output;
	u8 *out;
	u32 out_size;
	if (ctx->in_error) return GF_BAD_PARAM;
	if (!count || !ctx->opid || gf_filter_connections_pending(filter)) return GF_OK;
	if (ctx->incomplete_tile) {
		AV1TilePidCtx *tile = ctx->incomplete_tile;
		//the short packet has been consumed. Wait for its successor or EOS,
		//which may arrive in a later process call, before classifying it
		if (gf_filter_pid_get_packet(tile->pid)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Frame count mismatch on tile %u before end of stream\n", tile->tile_id));
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto error;
		}
		if (!gf_filter_pid_is_eos(tile->pid)) return GF_OK;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[AV1Merge] Discarding incomplete final temporal unit: tile %u reached end of stream\n", tile->tile_id));
		ctx->incomplete_tile = NULL;
		ctx->drain = GF_TRUE;
	}

	//keep queued packets owned by the input PIDs until every tile is ready;
	//get_packet may reconfigure a PID, so rebuild the grid after this pass
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ipids, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(tile->pid);
		u64 dts;
		if (!pck) {
			if (gf_filter_pid_is_eos(tile->pid)) nb_eos++;
			else missing = GF_TRUE;
			continue;
		}
		u32 size;
		if (!gf_filter_pck_get_data(pck, &size) || !size) {
			gf_filter_pid_drop_packet(tile->pid);
			missing = GF_TRUE;
			continue;
		}
		dts = gf_filter_pck_get_dts(pck);
		if (!tile->timescale || dts == GF_FILTER_NO_TS) { e = GF_BAD_PARAM; goto error; }
		if (!min_timescale || gf_timestamp_less(dts, tile->timescale, min_dts, min_timescale)) {
			min_dts = dts;
			min_timescale = tile->timescale;
		}
	}
	if (nb_eos == count) {
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}
	//a finished input cannot contribute any more complete grids; drain each
	//remaining input once per call; never drop a packet twice
	if (nb_eos || ctx->drain) {
		for (i = 0; i < count; i++) {
			AV1TilePidCtx *tile = gf_list_get(ctx->ipids, i);
			if (gf_filter_pid_get_packet(tile->pid)) gf_filter_pid_drop_packet(tile->pid);
		}
		return GF_OK;
	}
	if (missing) return GF_OK;
	if (ctx->grid_changed) {
		e = av1merge_rebuild_grid(ctx);
		if (e) goto error;
	}
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		GF_FilterPacket *pck = gf_filter_pid_get_packet(tile->pid);
		u64 dts = gf_filter_pck_get_dts(pck);
		if (!gf_timestamp_equal(dts, tile->timescale, min_dts, min_timescale)) discard = GF_TRUE;
	}
	if (discard) {
		//drop only the earliest timestamp, retaining later packets for a
		//complete grid at their own decode time.
		for (i = 0; i < count; i++) {
			AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
			GF_FilterPacket *pck = gf_filter_pid_get_packet(tile->pid);
			if (gf_timestamp_equal(gf_filter_pck_get_dts(pck), tile->timescale, min_dts, min_timescale))
				gf_filter_pid_drop_packet(tile->pid);
		}
		return GF_OK;
	}

	//concatenate all inputs before rewriting; offsets remain valid until the
	//output has been assembled, independent of the input packet lifetimes
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		u32 size;
		gf_filter_pck_get_data(gf_filter_pid_get_packet(tile->pid), &size);
		if (size > GF_UINT_MAX - total_size) { e = GF_OUT_OF_MEM; goto error; }
		tile->packet_start = tile->obu_pos = total_size;
		total_size += size;
		tile->packet_end = total_size;
	}
	if (total_size > ctx->buffer_alloc) {
		u8 *buffer = gf_realloc(ctx->buffer, total_size);
		if (!buffer) { e = GF_OUT_OF_MEM; goto error; }
		ctx->buffer = buffer;
		ctx->buffer_alloc = total_size;
	}
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		u32 size;
		const u8 *data = gf_filter_pck_get_data(gf_filter_pid_get_packet(tile->pid), &size);
		memcpy(ctx->buffer + tile->packet_start, data, size);
		tile->nb_pck++;
	}
	gf_bs_reassign_buffer(ctx->bs_in, ctx->buffer, total_size);
	e = av1merge_rewrite_pck(ctx);
	if (e == GF_EOS && ctx->incomplete_tile) {
		//never emit a partial TU. Its parser state cannot be used to resume merging:
		//either this is the final unit, or it is an error.
		gf_bs_get_content(ctx->bs_pck, &out, &out_size);
		gf_bs_reassign_buffer(ctx->bs_pck, NULL, 0);
		gf_free(out);
		for (i = 0; i < count; i++) {
			AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
			gf_filter_pid_drop_packet(tile->pid);
		}
		return GF_OK;
	}
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Cannot merge input frames: %s\n", gf_error_to_string(e)));
		goto error;
	}
	gf_bs_get_content(ctx->bs_pck, &out, &out_size);
	gf_bs_reassign_buffer(ctx->bs_pck, NULL, 0);
	output = gf_filter_pck_new_shared(ctx->opid, out, out_size, release_buffer);
	if (!output) { gf_free(out); e = GF_OUT_OF_MEM; goto error; }
	base_pck = gf_filter_pid_get_packet(((AV1TilePidCtx *) gf_list_get(ctx->ordered_ipids, 0))->pid);
	gf_filter_pck_merge_properties(base_pck, output);
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		gf_filter_pid_drop_packet(tile->pid);
	}
	return gf_filter_pck_send(output);

error:
	ctx->in_error = GF_TRUE;
	gf_filter_abort(filter);
	return e;
}

static Bool av1merge_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	GF_AV1MergeCtx *ctx = (GF_AV1MergeCtx *) gf_filter_get_udta(filter);

	switch (evt->base.type) {
	case GF_FEVT_PLAY:
		ctx->is_playing = GF_TRUE;
		ctx->play_evt = evt->play;
		return GF_FALSE;
	case GF_FEVT_STOP:
		ctx->is_playing = GF_FALSE;
		return GF_FALSE;
	default:
		break;
	}
	return GF_FALSE;
}

static GF_Err av1merge_initialize(GF_Filter *filter)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[AV1Merge] av1merge_initialize started.\n"));
	GF_AV1MergeCtx *ctx = (GF_AV1MergeCtx *)gf_filter_get_udta(filter);
	ctx->cols_forced = ctx->cols ? GF_TRUE : GF_FALSE;
	ctx->rows_forced = ctx->rows ? GF_TRUE : GF_FALSE;
	if (ctx->cols > AV1_MAX_TILE_COLS || ctx->rows > AV1_MAX_TILE_ROWS) return GF_BAD_PARAM;
	ctx->bs_in = gf_bs_new((char *)ctx, 1, GF_BITSTREAM_READ);
	ctx->bs_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	ctx->bs_pck = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	ctx->ipids = gf_list_new();
	ctx->ordered_ipids = gf_list_new();
	if (!ctx->bs_in || !ctx->bs_out || !ctx->bs_pck || !ctx->ipids || !ctx->ordered_ipids) return GF_OUT_OF_MEM;
	return GF_OK;
}

static void av1merge_finalize(GF_Filter *filter)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[AV1Merge] av1merge_finalize.\n"));
	GF_AV1MergeCtx *ctx = (GF_AV1MergeCtx *)gf_filter_get_udta(filter);
	gf_bs_del(ctx->bs_in);
	gf_bs_del(ctx->bs_out);
	gf_bs_del(ctx->bs_pck);
	gf_free(ctx->buffer);
	while (gf_list_count(ctx->ipids)) {
		AV1TilePidCtx *pctx = gf_list_pop_back(ctx->ipids);
		gf_odf_av1_cfg_del(pctx->state.config);
		gf_free(pctx);
	}
	gf_list_del(ctx->ipids);
	gf_list_del(ctx->ordered_ipids);
}

static const GF_FilterCapability AV1MergeCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_AV1),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_AV1)
};

#define OFFS(_n)	#_n, offsetof(GF_AV1MergeCtx, _n)

static const GF_FilterArgs AV1MergeArgs[] =
{
	{ OFFS(rows), "required number of rows, constraining explicit or implicit positioning (0: automatic)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(cols), "required number of columns, constraining explicit or implicit positioning (0: automatic)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_NORMAL},
	{0}
};

GF_FilterRegister AV1MergeRegister = {
	.name = "av1merge",
	GF_FS_SET_DESCRIPTION("AV1 Tile merger")
	GF_FS_SET_HELP("This filter merges a set of AV1 PIDs into a single AV1 PID.\n"
		"The filter creates a rectangular tiling grid with one input per populated cell, without overlaps or subdivision of input tiles.\n"
		"Positioning of tiles can be automatic (implicit) or explicit.\n"
		"Each input must contain one tile per frame, with matching frame headers, tile boundaries aligned to superblocks and same coding parameters.\n"
		"All tiles of a merged frame must have the same `base_q_idx`; mismatches are rejected, not adjusted. The common base quantizer index may change between frames.\n"
		"The filter assumes that all input PIDs are synchronized (frames share the same timestamp) and will reassemble frames with the same decode time. If PIDs are of unequal duration, the filter will drop frames as soon as one PID is over.\n"
		"## Implicit Positioning\n"
		"In implicit positioning, results may vary based on the order of input PIDs declaration. Equal-size inputs retain connection order in raster order.\n"
		"In this mode, no input shall have `CropOrigin`. Inputs are grouped by width into columns and by height into common rows, with partial superblock sizes at the final edges.\n"
		"The filter tries complete grids starting with the square root of the input count rounded up, then nearby row counts (fewer rows first on ties).\n"
		"[-rows]() and [-cols]() constrain implicit packing. If no complete grid satisfies the dimensions and constraints, merging is refused; use explicit positions to select another valid arrangement.\n"
		"EX gpac -i top.obu -i bottom.obu av1merge:cols=1 @ -o merged.mp4\n"
		"## Explicit Positioning\n"
		"In explicit positioning, the `CropOrigin` property on input PIDs is used to setup the tile grid. If `CropOrigin` is used, it shall be set on all input sources.\n"
		"If nonnegative coordinates are used, they specify absolute positioning in pixels of the tiles. Coordinates must align to superblocks and are not rounded.\n"
		"Absolute positioning allows gaps: input rectangle boundaries define the rows and columns, including empty cells. Only populated contiguous tile ranges are emitted, using separate frame-header and tile-group OBUs. No filler is encoded.\n"
		"While the bitstream syntax can represent omitted tiles, decoders may refuse such streams: libaom and dav1d reject discontinuous tile groups. Successful decoding and gap rendering are not guaranteed.\n"
		"If negative coordinates are used, they specify relative column and row indices: `-1x0` places an input to the right of `0x0`, and `0x-1` places it below.\n"
		"Both axes must be zero or negative in relative positioning; mixing relative and absolute positions is not supported. Relative grids must be complete with contiguous indices; pixel offsets are computed from the column widths and row heights.\n"
		"Rows, columns and input order are derived from these positions. [-rows]() and [-cols](), when set, must match the layout; they do not rearrange explicitly positioned inputs.\n"
		"Widths must match within each column and heights within each row; columns and rows may have different sizes. Only the final column and row may have non-superblock-aligned sizes. Frame dimensions must match the input PID dimensions; superresolution is not supported.\n"
		"Render widths must also match within columns and render heights within rows. Empty columns or rows use their coded size; custom render scaling on the corresponding axis is rejected when an empty axis size cannot be inferred.\n"
		"The minimum horizontal and vertical positions become the output origin. Inputs are ordered left-right, top-down; `TileID` is not used for placement.\n"
		"- Gaps are supported only with absolute positioning.\n"
		"- Overlaps and input tiles spanning multiple grid cells are not supported.\n"
		"EX gpac -i top.mp4:#CropOrigin=0x0 -i bottom.mp4:#CropOrigin=0x384 av1merge @ -o merged.mp4\n"
		"EX gpac -i bs_0x0.mp4:#CropOrigin=0x0 -i bs_640x0.mp4:#CropOrigin=1280x0 av1merge @ -o merge.mp4\n"
		"EX gpac -i top.obu:#CropOrigin=0x0 -i bottom.obu:#CropOrigin=0x-1 av1merge @ -o merged.mp4\n"
		" \n"
		"## Spatial Relationship Description (SRD)\n"
		"\n"
		"The filter will create an `SRDMap` property in the output PID if every input has `SRD` and `SRDRef` or an absolute `CropOrigin`.\n"
		"The `SRDMap` allows forwarding the logical sources `SRD` in the merged PID. Relative indices and implicit positions alone do not describe source locations.\n"
		"The output PID `SRDRef` is set to the output video size. Any input `SRD` or `SRDMap` is removed.\n"
		"The input `SRDRef` and `SRD` can be manually assigned to inputs.\n"
		"- `SRDRef` gives the width and height of the referential used for the input `SRD`.\n"
		"- `SRD` gives the position and size of the input in the original video, expressed in the input `SRDRef` referential.\n"
		"The inputs do not need to have matching `SRDRef`. SRD source coordinates must be nonnegative, SRD sizes and SRDRef dimensions must be positive, and scaled coordinates must fit unsigned 32-bit fields.\n"
		"\n"
		"Each merged input is described by 8 integers in the output `SRDMap`: `{Ox,Oy,Ow,Oh,Dx,Dy,Dw,Dh}`.\n"
		"- `Ox,Oy,Ow,Oh`: the source `SRD` rescaled to the output `SRDRef` referential (i.e. __where was the input ?__).\n"
		"- `Dx,Dy,Dw,Dh`: the position and size of the input in the reconstructed output (i.e. __where are the input pixels in the output ?__).\n"
		"Note: merged inputs are always listed in `SRDMap` in their tile order in the output bitstream; empty cells have no entry.\n"
		"\n"
		"Alternatively to using `SRD` and `SRDRef`, it is possible to specify absolute `CropOrigin` on the inputs, in which case:\n"
		"- `CropOrigin` gives the location in the source.\n"
		"- The input dimensions give the size in the source, and no rescaling of referential is done.\n"
	)
	.private_size = sizeof(GF_AV1MergeCtx),
	.flags = GF_FS_REG_DYNAMIC_REUSE,
	SETCAPS(AV1MergeCaps),
	.initialize = av1merge_initialize,
	.finalize = av1merge_finalize,
	.args = AV1MergeArgs,
	.configure_pid = av1merge_configure_pid,
	.process = av1merge_process,
	.process_event = av1merge_process_event,
	.max_extra_pids = -1,
	.hint_class_type = GF_FS_CLASS_STREAM
};

//Returns the enabled filter registration; cannot fail.
const GF_FilterRegister *av1merge_register(GF_FilterSession *session)
{
	return &AV1MergeRegister;
}
#else
//Reports the filter unavailable when AV1 merging or parsers are disabled.
const GF_FilterRegister *av1merge_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_AV_PARSERS && GPAC_DISABLE_AV1MERGE
