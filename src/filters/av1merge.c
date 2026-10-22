/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau
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
	u32 tile_id;
	u32 obu_start, payload_start, header_size;
	u64 header_end_bits;
	ObuType obu_type;

	//used both to check if dsi changed
	//and if dsi is different between tiles
	u32 dsi_crc;

	u32 width;
	u32 height;

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
	Bool cols_forced, rows_forced, grid_changed;
	AV1TilePidCtx *incomplete_tile;
	Bool drain;
	u8 tile_size_bytes;
	u8 *buffer;
	u32 buffer_alloc;
	AV1State rewrite_state;

	GF_List *ordered_ipids /*AV1TilePidCtx, ordered by #TileID*/;

	Bool is_playing;
	GF_FEVT_Play play_evt;

	GF_BitStream *bs_in;
	GF_BitStream *bs_out;
	GF_BitStream *bs_pck;
} GF_AV1MergeCtx;

static void release_buffer(GF_Filter *filter, GF_FilterPid *pid, GF_FilterPacket *pck)
{
	u32 size;
	gf_free((void *) gf_filter_pck_get_data(pck, &size));
}

static GFINLINE int bs_rewrite(GF_BitStream *bs_ori, GF_BitStream *bs_new, s32 bits)
{
	const int val = gf_bs_read_int(bs_ori, bits);
	gf_bs_write_int(bs_new, val, bits);
	return val;
}

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
		u32 width = gf_bs_read_int(bs_ori, frame_width_bits) + 1;
		u32 height = gf_bs_read_int(bs_ori, frame_height_bits) + 1;

		width *= ctx->cols;
		height *= ctx->rows;
		frame_width_bits = MAX(1, count_bits(width-1));
		frame_height_bits = MAX(1, count_bits(height-1));

		gf_bs_write_int(bs_new, frame_width_bits - 1, 4);
		gf_bs_write_int(bs_new, frame_height_bits - 1, 4);
		gf_bs_write_int(bs_new, width - 1, frame_width_bits);
		gf_bs_write_int(bs_new, height - 1, frame_height_bits);
	}
}

u64 av1_bit_position(GF_BitStream *bs);
static void av1merge_copy_bits(GF_BitStream *src, GF_BitStream *dst, u64 end)
{
	while (av1_bit_position(src) < end) {
		u32 bits = (u32) MIN(end - av1_bit_position(src), 32);
		bs_rewrite(src, dst, bits);
	}
}

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

//write the size after producing the payload, including when its LEB128 grows
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

static GF_Err av1merge_rewrite_config(GF_AV1MergeCtx *ctx, GF_FilterPid *opid)
{
	u32 i, new_size;
	u8 *new_dsi;
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
			gf_bs_get_content(dst, &obu->obu, &new_size);
			obu->obu_length = new_size;
			gf_bs_del(src);
			gf_bs_del(dst);
		}
	}
	gf_odf_av1_cfg_write(av1c, &new_dsi, &new_size);
	gf_odf_av1_cfg_del(av1c);
	gf_filter_pid_set_property(opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(new_dsi, new_size));
	return GF_OK;
}

void gf_bs_log_idx(GF_BitStream *bs, u32 nBits, const char *fname, s64 val, s32 idx1, s32 idx2, s32 idx3);
#define gf_bs_log(_bs, _nBits, _fname, _val) gf_bs_log_idx(_bs, _nBits, _fname, _val, -1, -1, -1)
static u32 gf_bs_read_int_log_idx3(GF_BitStream *bs, u32 nBits, const char *fname, s32 idx1, s32 idx2, s32 idx3)
{
	u32 val = gf_bs_read_int(bs, nBits);
	gf_bs_log_idx(bs, nBits, fname, val, idx1, idx2, idx3);
	return val;
}

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

//ns(n) as per AV1 section 4.10.7
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
	u32 tileWidthSb = sbCols, tileHeightSb = sbRows;
	sbCols *= ctx->cols;
	sbRows *= ctx->rows;
	minLog2tileCols = aom_av1_tile_log2(maxTileWidthSb, sbCols);
	minLog2Tiles = MAX(minLog2tileCols, aom_av1_tile_log2(MAX_TILE_AREA >> (2 * sbSize), sbRows * sbCols));
	maxTileAreaSb = sbRows * sbCols;
	if (minLog2Tiles)
		maxTileAreaSb >>= minLog2Tiles + 1;
	u32 maxTileHeightSb = MAX(maxTileAreaSb / tileWidthSb, 1);
	if (tileWidthSb > maxTileWidthSb || tileHeightSb > maxTileHeightSb)
		return GF_NOT_SUPPORTED;

	gf_bs_write_int(ctx->bs_out, 0, 1); //uniform_tile_spacing_flag
	for (i = 0; i < ctx->cols; i++)
		av1merge_write_ns(ctx->bs_out, MIN(sbCols - i * tileWidthSb, maxTileWidthSb), tileWidthSb-1);
	for (i = 0; i < ctx->rows; i++)
		av1merge_write_ns(ctx->bs_out, MIN(sbRows - i * tileHeightSb, maxTileHeightSb), tileHeightSb-1);
	if (ctx->cols > 1 || ctx->rows > 1) {
		gf_bs_write_int(ctx->bs_out, 0, count_bits(ctx->cols-1) + count_bits(ctx->rows-1)); //context_update_tile_id
		gf_bs_write_int(ctx->bs_out, ctx->tile_size_bytes-1, 2);
	}
	return GF_OK;
}

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

static void av1_frame_size(GF_AV1MergeCtx *ctx, GF_BitStream *bs_ori, GF_BitStream *bs_new, AV1State *state, Bool frame_size_override_flag)
{
	if (frame_size_override_flag) {
		u32 frame_width_minus_1, frame_height_minus_1;
		u8 n = state->frame_width_bits_minus_1 + 1;
		frame_width_minus_1 = gf_bs_read_int(bs_ori, n);
		gf_bs_write_int(bs_new, (frame_width_minus_1 + 1) * ctx->cols - 1, MAX(1, count_bits(state->sequence_width * ctx->cols - 1)));
		n = state->frame_height_bits_minus_1 + 1;
		frame_height_minus_1 = gf_bs_read_int(bs_ori, n);
		gf_bs_write_int(bs_new, (frame_height_minus_1 + 1) * ctx->rows - 1, MAX(1, count_bits(state->sequence_height * ctx->rows - 1)));
		state->width = frame_width_minus_1 + 1;
		state->height = frame_height_minus_1 + 1;
	} else {
		state->width = state->sequence_width;
		state->height = state->sequence_height;
	}
	superres_params(bs_ori, bs_new, state);
	//compute_image_size(); //no bits
}

static void av1_render_size(GF_AV1MergeCtx *ctx, GF_BitStream *bs_ori, GF_BitStream *bs_new, AV1State *state)
{
	Bool render_and_frame_size_different = REWRITEi(1, "render_and_frame_size_different_flag");
	if (render_and_frame_size_different == GF_TRUE) {
		gf_bs_write_int(bs_new, (gf_bs_read_int(bs_ori, 16) + 1) * ctx->cols - 1, 16);
		gf_bs_write_int(bs_new, (gf_bs_read_int(bs_ori, 16) + 1) * ctx->rows - 1, 16);
	}
}

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
		av1_render_size(ctx, bs_ori, bs_new, state);
	}
	else {
		superres_params(bs_ori, bs_new, state);
		//compute_image_size();
	}
}

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

	/*if ((frame_state->frame_type == AV1_KEY_FRAME) && frame_state->show_frame) {
		u32 i;
		for (i = 0; i < AV1_NUM_REF_FRAMES; i++) {
			state->RefValid[i] = 0;
			state->RefOrderHint[i] = 0;
		}
		for (i = 0; i < AV1_REFS_PER_FRAME; i++) {
			state->OrderHints[AV1_LAST_FRAME + i] = 0;
		}
	}*/

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
		av1_render_size(ctx, ctx->bs_in, ctx->bs_out, state);
		if (allow_screen_content_tools && state->UpscaledWidth == state->width) {
			REWRITE(1, "allow_intrabc");
		}
	}
	else {
		if (frame_state->frame_type == AV1_INTRA_ONLY_FRAME) {
			av1_frame_size(ctx, ctx->bs_in, ctx->bs_out, state, frame_size_override_flag);
			av1_render_size(ctx, ctx->bs_in, ctx->bs_out, state);
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
				av1_render_size(ctx, ctx->bs_in, ctx->bs_out, state);
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

static void av1merge_write_tile_group(GF_AV1MergeCtx *ctx)
{
	u32 tile_num, num_tiles = ctx->cols * ctx->rows;
	if (num_tiles > 1)
		gf_bs_write_int(ctx->bs_out, 0, 1); //tile_start_and_end_present_flag
	gf_bs_align(ctx->bs_out);
	for (tile_num = 0; tile_num < num_tiles; tile_num++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, tile_num);
		if (tile_num + 1 < num_tiles) {
			u32 i;
			//tile_size_minus_1 is little endian
			for (i = 0; i < ctx->tile_size_bytes; i++)
				gf_bs_write_u8(ctx->bs_out, (tile->last_size-1) >> (8*i));
		}
		gf_bs_write_data(ctx->bs_out, ctx->buffer + tile->tile_start, tile->last_size);
	}
}

#undef REWRITE

static Bool av1merge_frame_obu(ObuType type)
{
	return (type == OBU_FRAME) || (type == OBU_FRAME_HEADER) || (type == OBU_TILE_GROUP);
}

//parse one input OBU using that input's dimensions and reference state
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
		av1_parse_frame_header(ctx->bs_in, state);
		tile->header_end_bits = av1_bit_position(ctx->bs_in);
		if (tile->header_end_bits > end * 8 || gf_bs_is_overflow(ctx->bs_in))
			return GF_NON_COMPLIANT_BITSTREAM;
		if (!state->frame_state.show_existing_frame && (state->tileCols != 1 || state->tileRows != 1)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Each input frame must contain one tile\n"));
			return GF_NOT_SUPPORTED;
		}
		if (state->width != tile->width || state->height != tile->height || state->UpscaledWidth != state->width)
			return GF_NOT_SUPPORTED;
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
				if (tile->last_size)
					ctx->tile_size_bytes = MAX(ctx->tile_size_bytes, (count_bits(tile->last_size-1) + 7) / 8);
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
			if (base->obu_type == OBU_FRAME_HEADER)
				gf_bs_write_int(ctx->bs_out, 1, 1); //trailing_one_bit
			gf_bs_align(ctx->bs_out);
			if (base->obu_type == OBU_FRAME && base->last_size)
				av1merge_write_tile_group(ctx);
		} else if (base->obu_type == OBU_TILE_GROUP) {
			av1merge_write_tile_group(ctx);
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

static GF_Err av1merge_check_seq_hdr(GF_AV1MergeCtx *ctx, AV1TilePidCtx *pid_base, AV1TilePidCtx *pid_o)
{
	GF_Err e = GF_OK;
	char *src_base = gf_filter_pid_get_source(pid_base->pid);
	char *src_o = gf_filter_pid_get_source(pid_o->pid);

	if (pid_base->dsi_crc != pid_o->dsi_crc) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[AV1Merge] Sequence header differs between \"%s\" and \"%s\", expected the same\n", src_base, src_o));
		e = GF_BAD_PARAM;
	}

	if (src_base) gf_free(src_base);
	if (src_o) gf_free(src_o);
	return e;
}

static GF_Err av1merge_grid_size(GF_AV1MergeCtx *ctx, u32 count)
{
	if (!count) return GF_BAD_PARAM;
	if (ctx->cols_forced && ctx->rows_forced) {
		if ((u64) ctx->cols * ctx->rows != count) return GF_BAD_PARAM;
	} else if (ctx->cols_forced) {
		if (count % ctx->cols) return GF_BAD_PARAM;
		ctx->rows = count / ctx->cols;
	} else if (ctx->rows_forced) {
		if (count % ctx->rows) return GF_BAD_PARAM;
		ctx->cols = count / ctx->rows;
	} else {
		ctx->cols = count;
		if (count > 1) {
			while (--ctx->cols) {
				if (!(count % ctx->cols)) break;
			}
		}
		ctx->rows = count / ctx->cols;
	}
	if (ctx->cols > AV1_MAX_TILE_COLS || ctx->rows > AV1_MAX_TILE_ROWS)
		return GF_BAD_PARAM;
	return GF_OK;
}

static GF_Err av1merge_rebuild_grid(GF_AV1MergeCtx *ctx)
{
	u32 i, count = gf_list_count(ctx->ipids);
	GF_Err e = av1merge_grid_size(ctx, count);
	AV1TilePidCtx *base;
	const GF_PropertyValue *dsi;
	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Cannot fit %u inputs in the requested grid (%u columns, %u rows)\n", count, ctx->cols, ctx->rows));
		return e;
	}

	gf_list_reset(ctx->ordered_ipids);
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ipids, i);
		u32 j, id = tile->tile_id;
		for (j = 0; j < gf_list_count(ctx->ordered_ipids); j++) {
			AV1TilePidCtx *other = gf_list_get(ctx->ordered_ipids, j);
			if (other->tile_id == id) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Duplicate #TileID %u\n", id));
				return GF_BAD_PARAM;
			}
			if (other->tile_id > id) break;
		}
		if (!id || id > count) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Missing or invalid #TileID %u (expected 1 to %u)\n", id, count));
			return GF_BAD_PARAM;
		}
		e = gf_list_insert(ctx->ordered_ipids, tile, j);
		if (e) return e;
	}
	base = gf_list_get(ctx->ordered_ipids, 0);
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *tile = gf_list_get(ctx->ordered_ipids, i);
		e = av1merge_check_seq_hdr(ctx, base, tile);
		if (e) return e;
		if (tile->width != base->width || tile->height != base->height)
			return GF_BAD_PARAM;
	}
	u32 sb_size = base->state.use_128x128_superblock ? 128 : 64;
	if ((ctx->cols > 1 && base->width % sb_size) || (ctx->rows > 1 && base->height % sb_size)
		|| (u64) base->width * ctx->cols > 65536 || (u64) base->height * ctx->rows > 65536) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Grid dimensions must fit AV1 frame size fields and internal tile boundaries must align to superblocks\n"));
		return GF_NOT_SUPPORTED;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[AV1Merge] Grid configuration: %ux%u (columns %s, rows %s)\n", ctx->cols, ctx->rows,
		ctx->cols_forced ? "user-forced" : "automatic", ctx->rows_forced ? "user-forced" : "automatic"));
	gf_filter_pid_copy_properties(ctx->opid, base->pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->cols * base->width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->rows * base->height));
	dsi = gf_filter_pid_get_property(base->pid, GF_PROP_PID_DECODER_CONFIG);
	gf_bs_reassign_buffer(ctx->bs_in, dsi->value.data.ptr, dsi->value.data.size);
	e = av1merge_rewrite_config(ctx, ctx->opid);
	if (!e) ctx->grid_changed = GF_FALSE;
	return e;
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
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TILE_ID);
	if (tile->tile_id != (p ? p->value.uint : 0)) ctx->grid_changed = GF_TRUE;
	tile->tile_id = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	tile->timescale = p ? p->value.uint : 0;

	cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
	if (!tile->state.config || cfg_crc != tile->dsi_crc) {
		gf_odf_av1_cfg_del(tile->state.config);
		memset(&tile->state, 0, sizeof(tile->state));
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
	//publish a PID to connect the downstream graph; finalize its grid only
	//after all input connections and their first packets are available
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		if (!ctx->opid) return GF_OUT_OF_MEM;
		gf_filter_pid_copy_properties(ctx->opid, pid);
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
	{ OFFS(rows), "number of rows in tile grid (0: automatic)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(cols), "number of columns in tile grid (0: automatic)", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_NORMAL},
	{0}
};

GF_FilterRegister AV1MergeRegister = {
	.name = "av1merge",
	GF_FS_SET_DESCRIPTION("AV1 Tile merger")
	GF_FS_SET_HELP("This filter merges a set of AV1 PIDs into a single AV1 PID.\n"
		"The filter creates a tiling grid.\n"
		"If neither [-rows]() nor [-cols]() are set then an automatic layout will be created.\n"
		"If only one dimension is set, the other is derived from the input count. The grid must contain exactly one input per cell.\n"
		"Each input must contain one tile per frame, with matching frame headers and coding parameters suitable for tile composition. Internal tile boundaries must align to superblocks.\n"
		"The order is based on the #TileID property, numbered from 1 in left-right, top-down order.\n"
		"The filter will check the configurations of input PIDs and refuse processing if they are not consistent.\n"
		"The filter assumes that all input PIDs are synchronized (frames share the same timestamp) and will reassemble frames with the same decode time. If PIDs are of unequal duration, the filter will drop frames as soon as one PID is over.\n"
		"An incomplete final temporal unit is discarded when one input ends. Frame count mismatches before end of stream are errors.\n"
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

const GF_FilterRegister *av1merge_register(GF_FilterSession *session)
{
	return &AV1MergeRegister;
}
#else
const GF_FilterRegister *av1merge_register(GF_FilterSession *session)
{
	return NULL;
}
#endif // GPAC_DISABLE_AV_PARSERS && GPAC_DISABLE_AV1MERGE
