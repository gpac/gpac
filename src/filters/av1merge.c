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
	Bool in_error;

	AV1State state;
	u32 last_size;

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
	GF_List *ipids; //GF_FilterPid
	GF_FilterPid *opid;
	Bool in_error;

	u32 cols;
	u32 rows;

	GF_List *ordered_ipids /*AV1TilePidCtx, ordered by #TileID*/;

	Bool is_playing;
	GF_FEVT_Play play_evt;

	GF_BitStream *bs_in;
	GF_BitStream *bs_out;
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

static void bs_copy_all(GF_BitStream *bs_ori, GF_BitStream *bs_new, u32 max_ori_pos)
{
		u32 bits;
		while ((bits = gf_bs_bits_available(bs_ori)) && gf_bs_get_position(bs_ori) < max_ori_pos) {
			bs_rewrite(bs_ori, bs_new, bits);
		}
		gf_bs_align(bs_new);
}

//return the position of the obu_size location in the input bitstream
//so that it gets replaced once value is known, or -1 if not present
static int rewrite_obu_header(GF_BitStream *bs_ori, GF_BitStream *bs_new, ObuType *obu_type)
{
	Bool obu_extension_flag;
	bs_rewrite(bs_ori, bs_new, 1); // forbidden bit
	*obu_type = bs_rewrite(bs_ori, bs_new, 4); // obu_type
	obu_extension_flag = bs_rewrite(bs_ori, bs_new, 1); // obu_extension_flag
	if (!bs_rewrite(bs_ori, bs_new, 1)) { // obu_has_size_field
		return -1;
	}
	bs_rewrite(bs_ori, bs_new, 1); // obu_reserved_1bit
	if (obu_extension_flag) {
		bs_rewrite(bs_ori, bs_new, 3); // temporal_id
		bs_rewrite(bs_ori, bs_new, 2); // spatial_id
		bs_rewrite(bs_ori, bs_new, 3); // extension_header_reserved_3bits
	}

	return gf_bs_get_position(bs_ori);
}

static u32 count_bits(int val)
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

static void on_bs_log(void *udta, const char *field_name, u32 nb_bits, u64 field_val, s32 idx1, s32 idx2, s32 idx3)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, (" %s", field_name));
	if (idx1>=0) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("_%d", idx1));
		if (idx2>=0) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("_%d", idx2));
			if (idx3>=0) {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("_%d", idx3));
			}
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("=\""LLD, field_val));
	//GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("(%u)", nb_bits));
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("\""));
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
	Bool decoder_model_info_present_flag, initial_display_delay_present_flag;
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
				REWRITEi(5, "buffer_delay_length_minus_1");
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
		frame_width_bits = count_bits(width-1);
		frame_height_bits = count_bits(height-1);

		gf_bs_write_int(bs_new, frame_width_bits - 1, 4);
		gf_bs_write_int(bs_new, frame_height_bits - 1, 4);
		gf_bs_write_int(bs_new, width - 1, frame_width_bits);
		gf_bs_write_int(bs_new, height - 1, frame_height_bits);
	}
}

static GF_Err av1merge_rewrite_config(GF_AV1MergeCtx *ctx, AV1TilePidCtx *tile_pid, GF_FilterPid *opid)
{
	u32 i;
	u8 *new_dsi;
	u32 new_size;
	GF_AV1Config *av1c = gf_odf_av1_cfg_read_bs(ctx->bs_in);
	if (!av1c) return GF_NON_COMPLIANT_BITSTREAM;

	// for all the list objects in param_array
	for (i = 0; i < gf_list_count(av1c->obu_array); ++i) {
		GF_AV1_OBUArrayEntry *obu = gf_list_get(av1c->obu_array, i);
		if (obu->obu_type == OBU_SEQUENCE_HEADER) {
			ObuType obu_type;
			int obu_size_pos;
			u8 obu_size_lebsize = 0;
			u64 val;
			GF_BitStream *bs_ori = gf_bs_new(obu->obu, obu->obu_length, GF_BITSTREAM_READ);
			GF_BitStream *bs_new = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			if (!bs_ori || !bs_new) {
				gf_bs_del(bs_ori);
				gf_bs_del(bs_new);
				return GF_OUT_OF_MEM;
			}

			//OBU header and size
			obu_size_pos = rewrite_obu_header(bs_ori, bs_new, &obu_type);
			if (obu_size_pos > 0) {
				val = gf_av1_leb128_read(bs_ori, &obu_size_lebsize);
				gf_av1_leb128_write(bs_new, val);
			}

			//populate AV1State for later steps (tile_info(), ...)
			if (!tile_pid->state.config) {
				GF_BitStream *bs = gf_bs_new(obu->obu+obu_size_pos+obu_size_lebsize, obu->obu_length-(obu_size_pos+obu_size_lebsize), GF_BITSTREAM_READ);
				void av1_parse_sequence_header_obu(GF_BitStream *bs, AV1State *state);
				tile_pid->state.config = gf_odf_av1_cfg_new();
				gf_list_del(tile_pid->state.config->obu_array);
				*tile_pid->state.config = *av1c;
				tile_pid->state.config->obu_array = NULL;
				av1_parse_sequence_header_obu(bs, &tile_pid->state);
				gf_bs_del(bs);
			}

			av1merge_rewrite_seq_hdr(ctx, bs_ori, bs_new);

			//copy the rest of the bitstream
			bs_copy_all(bs_ori, bs_new, UINT32_MAX);

			//update OBU size field
			if (obu_size_pos > 0) {
				u64 pos = gf_bs_get_position(bs_new);
				gf_fatal_assert(gf_bs_get_bit_position(bs_new) % 8 == 0);
				val = pos - (obu_size_pos + gf_av1_leb128_size(val));
				gf_bs_seek(bs_new, obu_size_pos);
				int written = gf_av1_leb128_write(bs_new, val);

				//safety check: in-place modification assumes same leb128 encoding size
				gf_fatal_assert(written == obu_size_lebsize);

				gf_bs_seek(bs_new, pos);
			}

			gf_free(obu->obu);
			u32 obu_length = 0;
			gf_bs_get_content(bs_new, &obu->obu, &obu_length);
			gf_bs_reassign_buffer(bs_new, NULL, 0);
			obu->obu_length = obu_length;
			gf_bs_del(bs_ori);
			gf_bs_del(bs_new);
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

static void av1merge_rewrite_tile_info(GF_AV1MergeCtx *ctx, AV1State *state)
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
	Bool uniform_tile_spacing_flag = gf_bs_read_int(ctx->bs_in, 1);// "uniform_tile_spacing_flag"
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
			return;
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

	//write
	gf_bs_write_int(ctx->bs_out, 1/*uniform_tile_spacing_flag*/, 1);

	//u32 startSb, tileWidthSb, tileHeightSb;
	u32 minLog2tileRows = count_bits(ctx->rows);
	state->tileColsLog2 = minLog2tileCols;
	while (state->tileColsLog2 < ctx->cols) {
		Bool increment_tile_cols_log2 = GF_FALSE;
		gf_bs_write_int(ctx->bs_out, increment_tile_cols_log2, 1);
		state->tileColsLog2++;
	}
	gf_bs_write_int(ctx->bs_out, 0/*increment_tile_cols_log2*/, 1);

	state->tileCols = ctx->cols;
	minLog2tileRows = MAX((int)(minLog2Tiles - state->tileColsLog2), 0);
	state->tileRowsLog2 = minLog2tileRows;
	while (state->tileRowsLog2 < ctx->rows) {
		Bool increment_tile_rows_log2 = GF_FALSE;
		gf_bs_write_int(ctx->bs_out, increment_tile_rows_log2, 1);
		state->tileRowsLog2++;
	}
	gf_bs_write_int(ctx->bs_out, 0/*increment_tile_rows_log2*/, 1);

	state->tileRows = ctx->rows;

	if (state->tileColsLog2 > 0 || state->tileRowsLog2 > 0) {
		gf_bs_write_int(ctx->bs_out, 0/*context_update_tile_id*/, state->tileRowsLog2 + state->tileColsLog2);
		state->tile_size_bytes = ctx->rows * ctx->cols;
		
		for (i = 0; i < gf_list_count(ctx->ordered_ipids); i++) {
			AV1TilePidCtx *tile_pid = gf_list_get(ctx->ordered_ipids, i);
			state->tile_size_bytes = MAX(state->tile_size_bytes, 1+count_bits(tile_pid->last_size-1)/8);
		}
		gf_bs_write_int(ctx->bs_out, 2, state->tile_size_bytes - 1);
	}
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

static void av1_frame_size(GF_BitStream *bs_ori, GF_BitStream *bs_new, AV1State *state, Bool frame_size_override_flag)
{
	if (frame_size_override_flag) {
		u32 frame_width_minus_1, frame_height_minus_1;
		u8 n = state->frame_width_bits_minus_1 + 1;
		frame_width_minus_1 = REWRITEi(n, "frame_width_minus_1");
		n = state->frame_height_bits_minus_1 + 1;
		frame_height_minus_1 = REWRITEi(n, "frame_height_minus_1");
		state->width = frame_width_minus_1 + 1;
		state->height = frame_height_minus_1 + 1;
	} else {
		state->width = state->sequence_width;
		state->height = state->sequence_height;
	}
	superres_params(bs_ori, bs_new, state);
	//compute_image_size(); //no bits
}

static void av1_render_size(GF_BitStream *bs_ori, GF_BitStream *bs_new, AV1State *state)
{
	Bool render_and_frame_size_different = REWRITEi(1, "render_and_frame_size_different_flag");
	if (render_and_frame_size_different == GF_TRUE) {
		REWRITEi(16, "render_width_minus_1");
		REWRITEi(16, "render_height_minus_1");
	}
}

static void frame_size_with_refs(GF_BitStream *bs_ori, GF_BitStream *bs_new, AV1State *state, Bool frame_size_override_flag, s8 *ref_frame_idx)
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
		av1_frame_size(bs_ori, bs_new, state, frame_size_override_flag);
		av1_render_size(bs_ori, bs_new, state);
	}
	else {
		superres_params(bs_ori, bs_new, state);
		//compute_image_size();
	}
}

static void av1merge_rewrite_uncompressed_header(GF_AV1MergeCtx *ctx, AV1State *state)
{
	Bool error_resilient_mode = GF_FALSE, allow_screen_content_tools = GF_FALSE, force_integer_mv = GF_FALSE;
	Bool /*use_ref_frame_mvs = GF_FALSE,*/ FrameIsIntra = GF_FALSE, frame_size_override_flag = GF_FALSE;
	Bool disable_cdf_update = GF_FALSE;
	u8 showable_frame;
	u8 primary_ref_frame;
	u16 idLen = 0;
	u32 idx;
	s8 ref_frame_idx[AV1_REFS_PER_FRAME];
	AV1StateFrame *frame_state = &state->frame_state;

	/*if (state->frame_id_numbers_present_flag) {
		idLen = (state->additional_frame_id_length_minus_1 + state->delta_frame_id_length_minus_2 + 3);
	}
	frame_state->refresh_frame_flags = 0;

	showable_frame = 0;
	if (state->reduced_still_picture_header) {
		frame_state->key_frame = GF_TRUE;
		FrameIsIntra = GF_TRUE;
		frame_state->frame_type = AV1_KEY_FRAME;
		frame_state->show_frame = GF_TRUE;
		frame_state->show_existing_frame = 0;
	}
	else*/
	if (!state->reduced_still_picture_header) {
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
			return;
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
		if (frame_state->show_frame) {
			showable_frame = frame_state->frame_type != AV1_KEY_FRAME;

		}
		else {
			showable_frame = REWRITE(1, "showable_frame");
		}
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

	u8 allow_intrabc = 0;
	if (frame_state->frame_type == AV1_KEY_FRAME) {
		av1_frame_size(ctx->bs_in, ctx->bs_out, state, frame_size_override_flag);
		av1_render_size(ctx->bs_in, ctx->bs_out, state);
		if (allow_screen_content_tools && state->UpscaledWidth == state->width) {
			allow_intrabc = REWRITE(1, "allow_intrabc");
		}
	}
	else {
		if (frame_state->frame_type == AV1_INTRA_ONLY_FRAME) {
			av1_frame_size(ctx->bs_in, ctx->bs_out, state, frame_size_override_flag);
			av1_render_size(ctx->bs_in, ctx->bs_out, state);
			if (allow_screen_content_tools && state->UpscaledWidth == state->width) {
				allow_intrabc = REWRITE(1, "allow_intrabc");
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
				frame_size_with_refs(ctx->bs_in, ctx->bs_out, state, frame_size_override_flag, ref_frame_idx);
			}
			else {
				av1_frame_size(ctx->bs_in, ctx->bs_out, state, frame_size_override_flag);
				av1_render_size(ctx->bs_in, ctx->bs_out, state);
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

	av1merge_rewrite_tile_info(ctx, state);
}

static void av1merge_write_tile_group(GF_AV1MergeCtx *ctx, AV1State *state)
{
	int num_tiles = ctx->cols * ctx->rows;
	Bool tile_start_and_end_present_flag = GF_FALSE;
	if (num_tiles > 1)
		gf_bs_write_int(ctx->bs_out, tile_start_and_end_present_flag, 1);
	int tg_start = 0;
	int tg_end = num_tiles - 1;
	gf_bs_align(ctx->bs_out);
	for (int tile_num = tg_start; tile_num <= tg_end; tile_num++) {
		Bool lastTile = (tile_num == tg_end);
		AV1TilePidCtx *tile_pid = gf_list_get(ctx->ordered_ipids, tile_num);
		if (!lastTile) {
			for (int i=state->tile_size_bytes-1; i>=0; i--) {
				gf_bs_write_u8(ctx->bs_out, ((tile_pid->last_size-1) >> (8 * i)) & 0xFF);
			}
		}

		bs_copy_all(ctx->bs_in, ctx->bs_out, gf_bs_get_position(ctx->bs_in) + state->frame_state.tiles[tile_num].size);
	}
}

#undef REWRITE

static void av1merge_rewrite_pck(GF_AV1MergeCtx *ctx, AV1State *state)
{
	while (gf_bs_available(ctx->bs_in) > 0) {
		//OBU header and size
		u8 obu_size_lebsize = 0;
		ObuType obu_type;
		u64 obu_size, end_pos, obu_pos = gf_bs_get_position(ctx->bs_in);
		int obu_size_pos = rewrite_obu_header(ctx->bs_in, ctx->bs_out, &obu_type);
		gf_fatal_assert(obu_size_pos > 0);
		obu_size = gf_av1_leb128_read(ctx->bs_in, &obu_size_lebsize);
		gf_av1_leb128_write(ctx->bs_out, obu_size);
		end_pos = gf_bs_get_position(ctx->bs_in) + obu_size;

		if (obu_type == OBU_SEQUENCE_HEADER) {
			av1merge_rewrite_seq_hdr(ctx, ctx->bs_in, ctx->bs_out);
		}
		else if (obu_type == OBU_FRAME) {
			//locate end of uncompressed_header()
			u64 start_pos = gf_bs_get_position(ctx->bs_in);
			void av1_parse_frame_header(GF_BitStream *bs, AV1State *state);
#ifdef AV1MERGE_VERBOSE
			GF_Err gf_bs_set_logger(GF_BitStream *bs, void (*on_bs_log)(void *udta, const char *field_name, u32 nb_bits, u64 field_val, s32 idx1, s32 idx2, s32 idx3), void *udta);
			gf_bs_set_logger(ctx->bs_in, on_bs_log, ctx);
#endif
			av1_parse_frame_header(ctx->bs_in, state);
#ifdef AV1MERGE_VERBOSE
			gf_bs_set_logger(ctx->bs_in, NULL, ctx);
#endif
			gf_bs_align(ctx->bs_in);
			u64 tile_group_start_pos = gf_bs_get_position(ctx->bs_in);

			//parse to fill state with positions of frame data
			//do it before as we will modify the state while rewriting the tile group
			GF_Err av1_parse_tile_group(GF_BitStream *bs, AV1State *state, u64 obu_start, u64 obu_size);
			av1_parse_tile_group(ctx->bs_in, state, obu_pos, end_pos-obu_pos);

			//then rewrite and modify state as needed:
			//rewrite until tile_info(), then copy rest of uncompressed_header()
			gf_bs_seek(ctx->bs_in, start_pos);
			av1merge_rewrite_uncompressed_header(ctx, state);
			bs_copy_all(ctx->bs_in, ctx->bs_out, tile_group_start_pos);
			gf_bs_seek(ctx->bs_in, tile_group_start_pos);

			//use general parser to locate tile_info and rewrite it
			av1merge_write_tile_group(ctx, state);
			gf_bs_align(ctx->bs_out);
		}
		
		//copy the rest of the bitstream
		bs_copy_all(ctx->bs_in, ctx->bs_out, end_pos);
		gf_bs_align(ctx->bs_in);

		//update OBU size field
		if (obu_size_pos > 0) {
			gf_fatal_assert(gf_bs_get_bit_position(ctx->bs_out) % 8 == 0);
			obu_size = end_pos - (obu_size_pos + gf_av1_leb128_size(obu_size));
			gf_bs_seek(ctx->bs_out, obu_size_pos);
			int written = gf_av1_leb128_write(ctx->bs_out, obu_size);

			//safety check: in-place modification assumes same leb128 encoding size
			gf_fatal_assert(written == obu_size_lebsize);

			gf_bs_seek(ctx->bs_out, end_pos);
		}
	}
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

static GF_Err av1merge_rebuild_grid(GF_AV1MergeCtx *ctx, GF_FilterPid *pid)
{
	u32 i, count;
	u32 nb_pids = gf_list_count(ctx->ipids);

	//grid layout
#if 0 //see commented options for this filter
	if (ctx->cols && !ctx->rows) {
		if (nb_pids % ctx->cols) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Number of PIDs is not a multiple of the number of columns\n"));
			return GF_BAD_PARAM;
		}
		ctx->rows = nb_pids / ctx->cols;
	}
	if (ctx->rows && !ctx->cols) {
		if (nb_pids % ctx->rows) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Number of PIDs is not a multiple of the number of rows\n"));
			return GF_BAD_PARAM;
		}
		ctx->cols = nb_pids / ctx->rows;
	}
	if (!ctx->cols || !ctx->rows)
#endif
	{
		ctx->cols = nb_pids;
		if (nb_pids == 1)
			ctx->rows = 1;
		else while (--ctx->cols) {
			if (nb_pids % ctx->cols == 0) {
				ctx->rows = nb_pids / ctx->cols;
				break;
			}
		}
		if (!ctx->rows) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Cannot determine grid configuration\n"));
			return GF_BAD_PARAM;
		}
	}

	//re-order PIDs based on their #ID property to match the grid layout:
	// - first reset the ordered list
	gf_list_reset(ctx->ordered_ipids);
	count = gf_list_count(ctx->ipids);
	for (i = 0; i < gf_list_count(ctx->ipids); i++)
		gf_list_add(ctx->ordered_ipids, NULL);
	// - then add based on #TileID
	for (i = 0; i < count; i++) {
		AV1TilePidCtx *apidctx = gf_list_get(ctx->ipids, i);
		const GF_PropertyValue *p = gf_filter_pid_get_property(apidctx->pid, GF_PROP_PID_TILE_ID);
		if (!p) {
			char * src = gf_filter_pid_get_source(apidctx->pid);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Missing #TileID property for PID \"%s\"\n", src));
			if (src) gf_free(src);
			return GF_BAD_PARAM;
		}
		u32 id = p->value.uint;
		if (id > count) {
			char * src = gf_filter_pid_get_source(apidctx->pid);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Invalid #TileID %u for PID \"%s\"\n", id, src));
			if (src) gf_free(src);
			return GF_BAD_PARAM;
		}
		if (gf_list_get(ctx->ordered_ipids, id-1)) {
			char * src = gf_filter_pid_get_source(apidctx->pid);
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] Duplicate #TileID %u for PID \"%s\"\n", id, src));
			if (src) gf_free(src);
			return GF_BAD_PARAM;
		}
		gf_list_insert(ctx->ordered_ipids, apidctx, id-1);
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[AV1Merge] Grid configuration: %ux%u\n", ctx->cols, ctx->rows));
	return GF_OK;
}

static GF_Err av1merge_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	Bool grid_config_changed = GF_FALSE;
	u32 cfg_crc = 0, pid_width, pid_height;
	const GF_PropertyValue *p, *dsi;
	GF_Err e;
	u8 i;
	GF_AV1MergeCtx *ctx = (GF_AV1MergeCtx*)gf_filter_get_udta(filter);
	AV1TilePidCtx *tile_pid;

	if (ctx->in_error)
		return GF_BAD_PARAM;

	if (is_remove) {
		tile_pid = gf_filter_pid_get_udta(pid);
		gf_list_del_item(ctx->ipids, tile_pid);
		gf_free(tile_pid);
		if (!gf_list_count(ctx->ipids)) {
			if (ctx->opid) {
				gf_filter_pid_remove(ctx->opid);
				ctx->opid = NULL;
			}
			return GF_OK;
		}
		grid_config_changed = GF_TRUE;
		goto reconfig_grid;
	}

	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (!p) return GF_OK;
	pid_width = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (!p) return GF_OK;
	pid_height = p->value.uint;

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	if (!dsi) return GF_OK;

	tile_pid = gf_filter_pid_get_udta(pid);
	//not set, first time we see this pid
	if (!tile_pid) {
		GF_SAFEALLOC(tile_pid, AV1TilePidCtx);
		if (!tile_pid) return GF_OUT_OF_MEM;

		gf_filter_pid_set_udta(pid, tile_pid);
		tile_pid->pid = pid;
		tile_pid->width = pid_width;
		tile_pid->height = pid_height;
		gf_list_add(ctx->ipids, tile_pid);
		grid_config_changed = GF_TRUE;

		gf_filter_pid_set_framing_mode(pid, GF_TRUE);

		//it may happen that we are already running when we get this pid connection, typically with very large number of tiles
		//post a play event on pid in this case
		if (ctx->is_playing) {
			GF_FilterEvent fevt;
			fevt.play = ctx->play_evt;
			fevt.base.on_pid = pid;
			gf_filter_pid_send_event(pid, &fevt);
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) tile_pid->timescale = p->value.uint;

	//check if config has changed - if not, we ignore the reconfig
	//note that we don't copy all properties of input pids to the output in this case
	cfg_crc = 0;
	if (dsi->value.data.ptr && dsi->value.data.size) {
		cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
	}
	if (cfg_crc == tile_pid->dsi_crc)
		return GF_OK;
	tile_pid->dsi_crc = cfg_crc;

	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, pid);
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_AV1));
	}

	if ((pid_width != tile_pid->width) || (pid_height != tile_pid->height)) {
		tile_pid->width = pid_width;
		tile_pid->height = pid_height;
		grid_config_changed = GF_TRUE;
	}

reconfig_grid:
	//update grid
	if (grid_config_changed) {
		e = av1merge_rebuild_grid(ctx, pid);
		if (e) {
			ctx->in_error = GF_TRUE;
			return e;
		}
	}

	//check configs are compatible - for now we only warn but still process
	tile_pid = gf_list_get(ctx->ipids, 0);
	for (i=0; i<gf_list_count(ctx->ipids); i++) {
		AV1TilePidCtx *apidctx = gf_list_get(ctx->ipids, i);
		e = av1merge_check_seq_hdr(ctx, tile_pid, apidctx);
		if (e) {
			apidctx->in_error = GF_TRUE;
			return e;
		}

		if (apidctx->width != tile_pid->width || apidctx->height != tile_pid->height) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[AV1Merge] Tile size mismatch: expected %dx%d, got %dx%d\n", tile_pid->width, tile_pid->height, apidctx->width, apidctx->height));
			apidctx->in_error = GF_TRUE;
			return e;
		}
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->cols * tile_pid->width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->rows * tile_pid->height));

	dsi = gf_filter_pid_get_property(tile_pid->pid, GF_PROP_PID_DECODER_CONFIG);
	gf_assert(dsi);
	gf_bs_reassign_buffer(ctx->bs_in, dsi->value.data.ptr, dsi->value.data.size);
	return av1merge_rewrite_config(ctx, tile_pid, ctx->opid);
}

static GF_Err av1merge_process(GF_Filter *filter)
{
	GF_Err e = GF_OK;
	char *data;
	u32 data_size, i;
	u32 nb_eos, nb_ipid;
	u64 min_dts = GF_FILTER_NO_TS;
	u32 min_dts_timescale = 0;
	GF_FilterPacket *pck_src, *output_pck = NULL;;
	GF_AV1MergeCtx *ctx = (GF_AV1MergeCtx*) gf_filter_get_udta(filter);

	if (ctx->in_error)
		return GF_BAD_PARAM;
	nb_eos = 0;
	nb_ipid = gf_list_count(ctx->ipids);

	//probe input for at least one packet on each pid
	for (i = 0; i < nb_ipid; i++) {
		u64 dts;
		GF_FilterPacket *pck_src;
		AV1TilePidCtx *tile_pid = gf_list_get(ctx->ipids, i);
		if (tile_pid->in_error) {
			nb_eos++;
			continue;
		}

		pck_src = gf_filter_pid_get_packet(tile_pid->pid);
		if (!pck_src) {
			if (gf_filter_pid_is_eos(tile_pid->pid)) {
				nb_eos++;
				continue;
			}
			return GF_OK;
		}
		dts = gf_filter_pck_get_dts(pck_src);

		if (!min_dts_timescale || gf_timestamp_less(dts, tile_pid->timescale, min_dts, min_dts_timescale)) {
			min_dts = dts;
			min_dts_timescale = tile_pid->timescale;
		}
	}
	if (nb_eos == nb_ipid) {
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}

	//reassemble based on the ordered list of pids
	for (i = 0; i < nb_ipid; i++) {
		u64 dts;
		AV1TilePidCtx *tile_pid = gf_list_get(ctx->ordered_ipids, i);
		gf_assert(tile_pid);
		if (tile_pid->in_error)
			continue;
		pck_src = gf_filter_pid_get_packet(tile_pid->pid);
		if (nb_eos) {
			if (pck_src) {
				tile_pid->nb_pck++;
				GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[AV1Merge] pids of unequal duration, skipping packet %d on pid %d (nb_eos=%u)\n", tile_pid->nb_pck, i+1, nb_eos));
				gf_filter_pid_drop_packet(tile_pid->pid);
			}
			continue;
		}
		if (!pck_src) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[AV1Merge] no data on pid %d while merging, eos detected %d\n", i+1, gf_filter_pid_is_eos(tile_pid->pid)));
			continue;
		}

		dts = gf_filter_pck_get_dts(pck_src);
		if (dts * min_dts_timescale != min_dts * tile_pid->timescale) continue;
		data = (char *)gf_filter_pck_get_data(pck_src, &data_size); // data contains only a packet
		// TODO: this is a clock signaling, for now just trash ..
		if (!data) {
			gf_filter_pid_drop_packet(tile_pid->pid);
			continue;
		}
		tile_pid->nb_pck++;
		tile_pid->last_size = data_size;
		tile_pid->state.sequence_width  = tile_pid->state.width  = ctx->cols * tile_pid->width;
		tile_pid->state.sequence_height = tile_pid->state.height = ctx->rows * tile_pid->height;

		gf_bs_reassign_buffer(ctx->bs_in, data, data_size);
		av1merge_rewrite_pck(ctx, &tile_pid->state);
	}
	// end of loop on inputs

	//data lifetime and send
	{
		u8 *out = NULL;
		u32 out_len = 0;
		gf_bs_get_content(ctx->bs_out, &out, &out_len);
		gf_bs_reassign_buffer(ctx->bs_out, NULL, 0);
		if (out && out_len) {
			output_pck = gf_filter_pck_new_shared(ctx->opid, out, out_len, release_buffer);
			if (!output_pck) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[AV1Merge] failed to allocate output packet\n"));
				gf_free(out);
				return GF_OUT_OF_MEM;
			}
			gf_filter_pck_merge_properties(pck_src, output_pck);
		}
		for (i = 0; i < nb_ipid; i++) {
			AV1TilePidCtx *tile_pid = gf_list_get(ctx->ordered_ipids, i);

			// todo: might need to rewrite crypto info
			gf_filter_pid_drop_packet(tile_pid->pid);
		}
		if (output_pck) gf_filter_pck_send(output_pck);
	}

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
	ctx->bs_in = gf_bs_new((char *)ctx, 1, GF_BITSTREAM_READ);
	ctx->bs_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	ctx->ipids = gf_list_new();
	ctx->ordered_ipids = gf_list_new();
	return GF_OK;
}

static void av1merge_finalize(GF_Filter *filter)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[AV1Merge] av1merge_finalize.\n"));
	GF_AV1MergeCtx *ctx = (GF_AV1MergeCtx *)gf_filter_get_udta(filter);
	gf_bs_del(ctx->bs_in);
	gf_bs_del(ctx->bs_out);
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
#if 0 //Todo
	{ OFFS(rows), "signal number of expected rows in tile grid", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_NORMAL},
	{ OFFS(cols), "signal number of expected columns in tile grid", GF_PROP_UINT, "0", NULL, GF_FS_ARG_HINT_NORMAL},
#endif
	{0}
};

GF_FilterRegister AV1MergeRegister = {
	.name = "av1merge",
	GF_FS_SET_DESCRIPTION("AV1 Tile merger")
	GF_FS_SET_HELP("This filter merges a set of AV1 PIDs into a single AV1 PID.\n"
		"The filter creates a tiling grid.\n"
		"If neither [-rows]() nor [-cols]() are set then an automatic layout will be created.\n"
		"The order is based on the #TileID property, expected to be in left-right, top-down order.\n"
		"The filter will check the configurations of input PIDs and refuse processing if they are not consistent.\n"
		"The filter assumes that all input PIDs are synchronized (frames share the same timestamp) and will reassemble frames with the same decode time. If PIDs are of unequal duration, the filter will drop frames as soon as one PID is over.\n"
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
