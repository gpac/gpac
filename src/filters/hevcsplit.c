/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *					 Yacine Mathurin Boubacar Aziakou
 *					 Samir Mustapha
 *			Copyright (c) Telecom ParisTech 2019
 *					All rights reserved
 *
 *  This file is part of GPAC / HEVC tile split and rewrite filter
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

typedef struct
{
	GF_FilterPid *opid;
	u32 width, height, orig_x, orig_y;
	GF_FilterPacket *cur_pck;
} HEVCTilePid;

typedef struct
{
	GF_FilterPid *ipid;
	GF_List *outputs;
	u32 num_tiles, got_p;
	HEVCState hevc_state;
	u32 hevc_nalu_size_length, cfg_crc, nb_pck;

	//static read/write objects to avoid allocs
	GF_BitStream *bs_au_in;
	GF_BitStream *bs_nal_in;
	GF_BitStream *bs_nal_out;

	//buffer where we will store the rewritten slice with EPB
	u8 *buffer_nal;
	u32 buffer_nal_alloc;

	//buffer where we will store the rewritten slice or nal (sps, pps) without EPB
	u8 *output_no_epb;
	u32 output_no_epb_alloc;

	//buffer where we will store the input slice without EPB - we cannot use EPB removal functions from bitstream object since
	//we don't have gf_bs_read_data support in this mode
	u8 *input_no_epb;
	u32 input_no_epb_alloc;
} GF_HEVCSplitCtx;

//get tiles coordinates in as index in grid
static u32 hevcsplit_get_slice_tile_index(HEVCState *hevc)
{
	u32 tile_x, tile_y;

	HEVCSliceInfo si = hevc->s_info;
	u32 i, tbX = 0, tbY = 0, slX, slY, PicWidthInCtbsY, PicHeightInCtbsX, valX = 0, valY = 0;

	PicWidthInCtbsY = (si.sps->width + si.sps->max_CU_width - 1) / si.sps->max_CU_width;
	PicHeightInCtbsX = (si.sps->height + si.sps->max_CU_height - 1) / si.sps->max_CU_height;

	slY = si.slice_segment_address % PicWidthInCtbsY;
	slX = si.slice_segment_address / PicWidthInCtbsY;
	tile_x = 0;
	tile_y = 0;
	if (si.pps->tiles_enabled_flag) {
		if (si.pps->uniform_spacing_flag) {
			for (i = 0; i < si.pps->num_tile_rows; i++) {
				if (i < si.pps->num_tile_rows - 1)
					valX = (i + 1) * PicHeightInCtbsX / si.pps->num_tile_rows - i * PicHeightInCtbsX / si.pps->num_tile_rows;
				else
					valX = PicHeightInCtbsX - tbX;

				if (slX < tbX + valX) {
					tile_x = i;
					break;
				}

				tbX += valX;
			}
			for (i = 0; i < si.pps->num_tile_columns; i++) {
				if (i < si.pps->num_tile_columns - 1)
					valY = (i + 1) * PicWidthInCtbsY / si.pps->num_tile_columns - i * PicWidthInCtbsY / si.pps->num_tile_columns;
				else
					valY = PicWidthInCtbsY - tbY;
				if (slY < tbY + valY) {
					tile_y = i;
					break;
				}

				tbY += valY;
			}
		} else {
			for (i = 0; i < si.pps->num_tile_rows; i++) {
				if (i < si.pps->num_tile_rows - 1)
					valX = si.pps->row_height[i];
				else
					valX = PicHeightInCtbsX - tbX;
				if (slX < tbX + valX) {
					tile_x = i;
					break;
				}

				tbX += valX;
			}

			for (i = 0; i < si.pps->num_tile_columns; i++) {
				if (i < si.pps->num_tile_columns - 1)
					valY = si.pps->column_width[i];
				else
					valY = PicWidthInCtbsY - tbY;

				if (slY < tbY + valY) {
					tile_y = i;
					break;
				}
				tbY += valY;
			}
		}
	}
	return tile_x * hevc->s_info.pps->num_tile_columns + tile_y;
}

//get the width and the height of the tile in pixel
static void hevcsplit_get_tile_pixel_coords(HEVCState *hevc, u32 index_row, u32 index_col, u32 pps_id, u32 *width, u32 *height, u32 *tx, u32 *ty)
{
	u32 i, sps_id, tbX = 0, tbY = 0, PicWidthInCtbsY, PicHeightInCtbsX, max_CU_width, max_CU_height;

	sps_id = hevc->pps[pps_id].sps_id;
	max_CU_width = hevc->sps[sps_id].max_CU_width;
	max_CU_height = hevc->sps[sps_id].max_CU_height;
	PicWidthInCtbsY = (hevc->sps[sps_id].width + max_CU_width - 1) / max_CU_width;
	PicHeightInCtbsX = (hevc->sps[sps_id].height + max_CU_height - 1) / max_CU_height;

	if (tx) *tx = 0;
	if (ty) *ty = 0;

	if (!hevc->pps[pps_id].tiles_enabled_flag) {
 		*width = hevc->sps[sps_id].width;
 		*height = hevc->sps[sps_id].height;
 		*tx = 0;
 		*ty = 0;
	}

	if (hevc->pps[pps_id].uniform_spacing_flag) {
		if (index_row < hevc->pps[pps_id].num_tile_rows - 1) {
			for (i = 0; i < index_row; i++)
				tbX += ((i + 1) * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows - i * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows);

			*height = (index_row + 1) * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows - index_row * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows;
		} else {
			for (i = 0; i < hevc->pps[pps_id].num_tile_rows - 1; i++)
				tbX += ((i + 1) * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows - i * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows);
			*height = PicHeightInCtbsX - tbX;
		}

		if (index_col < hevc->pps[pps_id].num_tile_columns - 1) {
			for (i = 0; i < index_col; i++)
				tbY += ((i + 1) * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns - i * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns);
			*width = (index_col + 1) * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns - index_col * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns;
		} else {
			for (i = 0; i < hevc->pps[pps_id].num_tile_columns - 1; i++)
				tbY += ((i + 1) * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns - i * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns);
			*width = PicWidthInCtbsY - tbY;
		}
	} else {
		if (index_row < hevc->pps[pps_id].num_tile_rows - 1) {

			for (i = 0; i < index_row; i++)
				tbX += hevc->pps[pps_id].row_height[i];

			*height = hevc->pps[pps_id].row_height[index_row];
		} else {
			for (i = 0; i < hevc->pps[pps_id].num_tile_rows - 1; i++)
				tbX += hevc->pps[pps_id].row_height[i];
			*height = PicHeightInCtbsX - tbX;
		}

		if (index_col < hevc->pps[pps_id].num_tile_columns - 1) {
			*width = hevc->pps[pps_id].column_width[index_col];

			for (i = 0; i < index_col; i++)
				tbY += hevc->pps[pps_id].column_width[i];
		} else {
			for (i = 0; i < hevc->pps[pps_id].num_tile_columns - 1; i++)
				tbY += hevc->pps[pps_id].column_width[i];
			*width = PicWidthInCtbsY - tbY;
		}
	}

	*width = *width * max_CU_width;
	*height = *height * max_CU_height;
	if (ty) *ty = tbX * max_CU_height;
	if (tx) *tx = tbY * max_CU_width;

	if (*tx + *width > hevc->sps[sps_id].width)
		*width = hevc->sps[sps_id].width - *tx;
	if (*ty + *height > hevc->sps[sps_id].height)
		*height = hevc->sps[sps_id].height - *ty;
}

//rewrite the profile and level
static void hevc_write_profile_tier_level(GF_BitStream *bs_in, GF_BitStream *bs_out, Bool ProfilePresentFlag, u8 MaxNumSubLayersMinus1)
{
	u8 j;
	Bool sub_layer_profile_present_flag[8], sub_layer_level_present_flag[8];
	if (ProfilePresentFlag) {
		gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
		gf_bs_write_long_int(bs_out, gf_bs_read_long_int(bs_in, 32), 32);
		gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4);
		gf_bs_write_long_int(bs_out, gf_bs_read_long_int(bs_in, 44), 44);
	}
	gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);

	for (j = 0; j < MaxNumSubLayersMinus1; j++) {
		sub_layer_profile_present_flag[j] = gf_bs_read_int(bs_in, 1);
		gf_bs_write_int(bs_out, sub_layer_profile_present_flag[j], 1);
		sub_layer_level_present_flag[j] = gf_bs_read_int(bs_in, 1);
		gf_bs_write_int(bs_out, sub_layer_level_present_flag[j], 1);
	}
	if (MaxNumSubLayersMinus1 > 0)
		for (j = MaxNumSubLayersMinus1; j < 8; j++)
			gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 2), 2);


	for (j = 0; j < MaxNumSubLayersMinus1; j++) {
		if (sub_layer_profile_present_flag[j]) {
			gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
			gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 32), 32);
			gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4);
			gf_bs_write_long_int(bs_out, gf_bs_read_long_int(bs_in, 44), 44);
		}
		if (sub_layer_level_present_flag[j])
			gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
	}
}

//also used by HEVCmerge, don't use context bitstream objects
void hevc_rewrite_sps(char *in_SPS, u32 in_SPS_length, u32 width, u32 height, char **out_SPS, u32 *out_SPS_length)
{
	GF_BitStream *bs_in, *bs_out;
	u64 length_no_use = 4096;
	u8 *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0, sps_ext_or_max_sub_layers_minus1;
	u8 max_sub_layers_minus1 = 0, layer_id;
	Bool conformance_window_flag, multiLayerExtSpsFlag;
	u32 chroma_format_idc;

	bs_in = gf_bs_new(in_SPS, in_SPS_length, GF_BITSTREAM_READ);
	gf_bs_enable_emulation_byte_removal(bs_in, GF_TRUE);
	bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
	/*dst_buffer_size =*/ gf_bs_get_size(bs_out);
	if (!bs_in) goto exit;

	//copy NAL Header
	gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 7), 7);
	layer_id = gf_bs_read_int(bs_in, 6);
	gf_bs_write_int(bs_out, layer_id, 6);
	gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 3), 3);

	gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4); //copy vps_id

	if (layer_id == 0)
	{
		max_sub_layers_minus1 = gf_bs_read_int(bs_in, 3);
		gf_bs_write_int(bs_out, max_sub_layers_minus1, 3);
	}
	else
	{
		sps_ext_or_max_sub_layers_minus1 = gf_bs_read_int(bs_in, 3);
		gf_bs_write_int(bs_out, sps_ext_or_max_sub_layers_minus1, 3);
	}
	multiLayerExtSpsFlag = (layer_id != 0) && (sps_ext_or_max_sub_layers_minus1 == 7);
	if (!multiLayerExtSpsFlag) {
		gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1);
		hevc_write_profile_tier_level(bs_in, bs_out, 1, max_sub_layers_minus1);
	}

	gf_bs_set_ue(bs_out, gf_bs_get_ue(bs_in)); //copy sps_id

	if (multiLayerExtSpsFlag) {
		u8 update_rep_format_flag = gf_bs_read_int(bs_in, 1);
		gf_bs_write_int(bs_out, update_rep_format_flag, 1);
		if (update_rep_format_flag) {
			gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
		}
	}
	else {
		chroma_format_idc = gf_bs_get_ue(bs_in);
		gf_bs_set_ue(bs_out, chroma_format_idc);
		if (chroma_format_idc == 3)
			gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1); // copy separate_colour_plane_flag
		/*w =*/ gf_bs_get_ue(bs_in); //skip width bits in input bitstream
		/*h =*/ gf_bs_get_ue(bs_in); //skip height bits in input bitstream

		//Copy the new width and height in output bitstream
		gf_bs_set_ue(bs_out, width);
		gf_bs_set_ue(bs_out, height);

		//Get rid of the bit conformance_window_flag
		conformance_window_flag = gf_bs_read_int(bs_in, 1);
		//put the new conformance flag to zero
		gf_bs_write_int(bs_out, 0, 1);

		//Skip the bits related to conformance_window_offset
		if (conformance_window_flag)
		{
			gf_bs_get_ue(bs_in);
			gf_bs_get_ue(bs_in);
			gf_bs_get_ue(bs_in);
			gf_bs_get_ue(bs_in);
		}
	}

	//copy and write the rest of the bits in the byte
	while (gf_bs_get_bit_position(bs_in) != 8) {
		gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1);
	}

	//copy and write the rest of the bytes
	while (gf_bs_get_size(bs_in) != gf_bs_get_position(bs_in)) {
		gf_bs_write_int(bs_out, gf_bs_read_u8(bs_in), 8); //watchout, not aligned in destination bitstream
	}


	gf_bs_align(bs_out);						//align

	gf_free(data_without_emulation_bytes);
	data_without_emulation_bytes = NULL;
	data_without_emulation_bytes_size = 0;

	gf_bs_get_content(bs_out, &data_without_emulation_bytes, &data_without_emulation_bytes_size);

	*out_SPS_length = data_without_emulation_bytes_size + gf_media_nalu_emulation_bytes_add_count(data_without_emulation_bytes, data_without_emulation_bytes_size);
	*out_SPS = gf_malloc(*out_SPS_length);
	*out_SPS_length = gf_media_nalu_add_emulation_bytes(data_without_emulation_bytes, *out_SPS, data_without_emulation_bytes_size);

	exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
	gf_free(data_without_emulation_bytes);
}


static void hevcsplit_rewrite_pps_no_grid(GF_HEVCSplitCtx *ctx, char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length)
{
	u32 i, out_size_no_epb;
	u8 cu_qp_delta_enabled_flag, tiles_enabled_flag, loop_filter_across_slices_enabled_flag;

	gf_bs_reassign_buffer(ctx->bs_nal_in, in_PPS, in_PPS_length);
	gf_bs_enable_emulation_byte_removal(ctx->bs_nal_in, GF_TRUE);


	if (!ctx->bs_nal_out) ctx->bs_nal_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_nal_out, ctx->output_no_epb, ctx->output_no_epb_alloc);

	//Read and write NAL header bits
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 16), 16);
	gf_bs_set_ue(ctx->bs_nal_out, gf_bs_get_ue(ctx->bs_nal_in)); //pps_pic_parameter_set_id
	gf_bs_set_ue(ctx->bs_nal_out, gf_bs_get_ue(ctx->bs_nal_in)); //pps_seq_parameter_set_id
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 7), 7); //from dependent_slice_segments_enabled_flag to cabac_init_present_flag
	gf_bs_set_ue(ctx->bs_nal_out, gf_bs_get_ue(ctx->bs_nal_in)); //num_ref_idx_l0_default_active_minus1
	gf_bs_set_ue(ctx->bs_nal_out, gf_bs_get_ue(ctx->bs_nal_in)); //num_ref_idx_l1_default_active_minus1
	gf_bs_set_se(ctx->bs_nal_out, gf_bs_get_se(ctx->bs_nal_in)); //init_qp_minus26
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 2), 2); //from constrained_intra_pred_flag to transform_skip_enabled_flag
	cu_qp_delta_enabled_flag = gf_bs_read_int(ctx->bs_nal_in, 1); //cu_qp_delta_enabled_flag
	gf_bs_write_int(ctx->bs_nal_out, cu_qp_delta_enabled_flag, 1); //
	if (cu_qp_delta_enabled_flag)
		gf_bs_set_ue(ctx->bs_nal_out, gf_bs_get_ue(ctx->bs_nal_in)); // diff_cu_qp_delta_depth
	gf_bs_set_se(ctx->bs_nal_out, gf_bs_get_se(ctx->bs_nal_in)); // pps_cb_qp_offset
	gf_bs_set_se(ctx->bs_nal_out, gf_bs_get_se(ctx->bs_nal_in)); // pps_cr_qp_offset
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 4), 4); // from pps_slice_chroma_qp_offsets_present_flag to transquant_bypass_enabled_flag

	tiles_enabled_flag = gf_bs_read_int(ctx->bs_nal_in, 1); // tiles_enabled_flag
	gf_bs_write_int(ctx->bs_nal_out, 0, 1); //discard tile enable in dest

	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1); // entropy_coding_sync_enabled_flag

	//read tile info from source
	if (tiles_enabled_flag) {
		u32 num_tile_columns_minus1 = gf_bs_get_ue(ctx->bs_nal_in);
		u32 num_tile_rows_minus1 = gf_bs_get_ue(ctx->bs_nal_in);
		u8 uniform_spacing_flag = gf_bs_read_int(ctx->bs_nal_in, 1);

		if (!uniform_spacing_flag) {
			for (i = 0; i < num_tile_columns_minus1; i++)
				gf_bs_get_ue(ctx->bs_nal_in);
			for (i = 0; i < num_tile_rows_minus1; i++)
				gf_bs_get_ue(ctx->bs_nal_in);
		}
		gf_bs_read_int(ctx->bs_nal_in, 1);
	}

	loop_filter_across_slices_enabled_flag = gf_bs_read_int(ctx->bs_nal_in, 1);
	gf_bs_write_int(ctx->bs_nal_out, loop_filter_across_slices_enabled_flag, 1);

	//copy and write the rest of the bits in the byte
	while (gf_bs_get_bit_position(ctx->bs_nal_in) != 8) {
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1);
	}

	//copy and write the rest of the bytes
	while (gf_bs_get_size(ctx->bs_nal_in) != gf_bs_get_position(ctx->bs_nal_in)) {
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_u8(ctx->bs_nal_in), 8); //watchout, not aligned in destination bitstream
	}

	//align
	gf_bs_align(ctx->bs_nal_out);

	gf_bs_get_content_no_truncate(ctx->bs_nal_out, &ctx->output_no_epb, &out_size_no_epb, &ctx->output_no_epb_alloc);

	*out_PPS_length = out_size_no_epb + gf_media_nalu_emulation_bytes_add_count(ctx->output_no_epb, out_size_no_epb);
	*out_PPS = gf_malloc(*out_PPS_length);
	gf_media_nalu_add_emulation_bytes(ctx->output_no_epb, *out_PPS, out_size_no_epb);
}

//return the new size slice - slice data is stored in ctx->buffer_nal
static u32 hevcsplit_remove_slice_address(GF_HEVCSplitCtx *ctx, u8 *in_slice, u32 in_slice_length)
{
	u32 inslice_size_no_epb, outslice_size_epb;
	u64 header_end;
	u32 num_entry_point_start;
	u32 pps_id;
	Bool RapPicFlag = GF_FALSE;
	HEVC_PPS *pps;
	HEVC_SPS *sps;
	u32 al, slice_size, slice_offset_orig, slice_offset_dst;
	u32 first_slice_segment_in_pic_flag;
	//u32 dependent_slice_segment_flag;
	u8 nal_unit_type;
	HEVCState *hevc = &ctx->hevc_state;

	if (ctx->input_no_epb_alloc < in_slice_length) {
		ctx->input_no_epb = gf_realloc(ctx->input_no_epb, in_slice_length);
		ctx->input_no_epb_alloc = in_slice_length;
	}
	inslice_size_no_epb = gf_media_nalu_remove_emulation_bytes(in_slice, ctx->input_no_epb, in_slice_length);

	gf_bs_reassign_buffer(ctx->bs_nal_in, ctx->input_no_epb, inslice_size_no_epb);
	gf_bs_enable_emulation_byte_removal(ctx->bs_nal_in, GF_FALSE);

	if (!ctx->bs_nal_out) ctx->bs_nal_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_nal_out, ctx->output_no_epb, ctx->output_no_epb_alloc);


	assert(hevc->s_info.header_size_bits >= 0);
	assert(hevc->s_info.entry_point_start_bits >= 0);
	header_end = (u64) hevc->s_info.header_size_bits;

	num_entry_point_start = (u32) hevc->s_info.entry_point_start_bits;

	// nal_unit_header			 
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1);
	nal_unit_type = gf_bs_read_int(ctx->bs_nal_in, 6);
	gf_bs_write_int(ctx->bs_nal_out, nal_unit_type, 6);
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 9), 9);

	first_slice_segment_in_pic_flag = gf_bs_read_int(ctx->bs_nal_in, 1);    //first_slice_segment_in_pic_flag
	gf_bs_write_int(ctx->bs_nal_out, 1, 1);

	switch (nal_unit_type) {
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
		RapPicFlag = GF_TRUE;
		break;
	case GF_HEVC_NALU_SLICE_BLA_W_LP:
	case GF_HEVC_NALU_SLICE_BLA_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_N_LP:
	case GF_HEVC_NALU_SLICE_CRA:
		RapPicFlag = GF_TRUE;
		break;
	}

	if (RapPicFlag) {
		//no_output_of_prior_pics_flag
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1);
	}

	pps_id = gf_bs_get_ue(ctx->bs_nal_in);					 //pps_id
	gf_bs_set_ue(ctx->bs_nal_out, pps_id);

	pps = &hevc->pps[pps_id];
	sps = &hevc->sps[pps->sps_id];

	//dependent_slice_segment_flag = 0;
	if (!first_slice_segment_in_pic_flag && pps->dependent_slice_segments_enabled_flag) {
		/*dependent_slice_segment_flag =*/ gf_bs_read_int(ctx->bs_nal_in, 1);
//	} else {
//		dependent_slice_segment_flag = GF_FALSE;
	}

	if (!first_slice_segment_in_pic_flag) {
		gf_bs_read_int(ctx->bs_nal_in, sps->bitsSliceSegmentAddress);
	}
	//else original slice segment address = 0

	//nothing to write for slice adress, we remove the adress

	//copy over until num_entry_points
	while (num_entry_point_start != (gf_bs_get_position(ctx->bs_nal_in) - 1) * 8 + gf_bs_get_bit_position(ctx->bs_nal_in)) {
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1);
	}

	//no tilin, don't write num_entry_points

	//write slice extension to 0
	if (pps->slice_segment_header_extension_present_flag)
		gf_bs_write_int(ctx->bs_nal_out, 0, 1);

	//we may have unparsed data in the source bitstream (slice header) due to entry points or slice segment extensions
	//TODO: we might want to copy over the slice extension header bits
	while (header_end != (gf_bs_get_position(ctx->bs_nal_in) - 1) * 8 + gf_bs_get_bit_position(ctx->bs_nal_in))
		gf_bs_read_int(ctx->bs_nal_in, 1);

	//read byte_alignment() is bit=1 + x bit=0
	al = gf_bs_read_int(ctx->bs_nal_in, 1);
	if (al != 1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCTileSplit] source slice header not properly aligned\n"));
	}

	//write byte_alignment() is bit=1 + x bit=0
	gf_bs_write_int(ctx->bs_nal_out, 1, 1);
	gf_bs_align(ctx->bs_nal_out);					//align

	 /* get output slice header*/
	u32 outslice_size_no_epb;
	gf_bs_get_content_no_truncate(ctx->bs_nal_out, &ctx->output_no_epb, &outslice_size_no_epb, &ctx->output_no_epb_alloc);
	/* slice_size: the rest of the slice after the header (no_emulation_bytes in it) in source.*/
	slice_size = (u32) gf_bs_available(ctx->bs_nal_in);
	 /* slice_offset_orig: Immediate next byte after header_end (start of the slice_payload) */
	slice_offset_orig = (u32) gf_bs_get_position(ctx->bs_nal_in);
	/* slice_offset_dst: end of header in dest (=dst_buf_size since we have not copied slice data yet)*/
	slice_offset_dst = outslice_size_no_epb;
	/*final slice size: add source slice size */
	outslice_size_no_epb += slice_size;
	if (ctx->output_no_epb_alloc < outslice_size_no_epb) {
		ctx->output_no_epb = gf_realloc(ctx->output_no_epb, outslice_size_no_epb);
		ctx->output_no_epb_alloc = outslice_size_no_epb;
	}
	memcpy(ctx->output_no_epb + slice_offset_dst, ctx->input_no_epb + slice_offset_orig, sizeof(char) * slice_size);

	outslice_size_epb = outslice_size_no_epb + gf_media_nalu_emulation_bytes_add_count(ctx->output_no_epb, outslice_size_no_epb);

	if (ctx->buffer_nal_alloc < outslice_size_epb) {
		ctx->buffer_nal = gf_realloc(ctx->buffer_nal, outslice_size_epb);
		ctx->buffer_nal_alloc = outslice_size_epb;
	}
	gf_media_nalu_add_emulation_bytes(ctx->output_no_epb, ctx->buffer_nal, outslice_size_no_epb);
	return outslice_size_epb;
}

static char *hevcsplit_rewrite_nal(GF_Filter *filter, GF_HEVCSplitCtx *ctx, char *in_nal, u32 in_nal_size, u32 *out_tile_index, u32 *out_nal_size)
{
	u8 nal_unit_type, temporal_id, layer_id;
	u32 buf_size;
	HEVCState *hevc = &ctx->hevc_state;

	gf_media_hevc_parse_nalu(in_nal, in_nal_size, hevc, &nal_unit_type, &temporal_id, &layer_id);
	switch (nal_unit_type) {
	//all VCL nal, remove slice adress
	case GF_HEVC_NALU_SLICE_TRAIL_N:
	case GF_HEVC_NALU_SLICE_TRAIL_R:
	case GF_HEVC_NALU_SLICE_TSA_N:
	case GF_HEVC_NALU_SLICE_TSA_R:
	case GF_HEVC_NALU_SLICE_STSA_N:
	case GF_HEVC_NALU_SLICE_STSA_R:
	case GF_HEVC_NALU_SLICE_RADL_N:
	case GF_HEVC_NALU_SLICE_RADL_R:
	case GF_HEVC_NALU_SLICE_RASL_N:
	case GF_HEVC_NALU_SLICE_RASL_R:
	case GF_HEVC_NALU_SLICE_BLA_W_LP:
	case GF_HEVC_NALU_SLICE_BLA_W_DLP:
	case GF_HEVC_NALU_SLICE_BLA_N_LP:
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
	case GF_HEVC_NALU_SLICE_CRA:
		*out_tile_index = hevcsplit_get_slice_tile_index(hevc);
		buf_size = hevcsplit_remove_slice_address(ctx, in_nal, in_nal_size);
		*out_nal_size = buf_size;
		return ctx->buffer_nal;
	//non-vcl, write to bitstream
	default:
		*out_nal_size = in_nal_size;
		return in_nal;
	}
	return NULL;
}

static GF_Err hevcsplit_rewrite_dsi(GF_HEVCSplitCtx *ctx, GF_FilterPid *opid, char *data, u32 size, u32 new_width, u32 new_height)
{
	u32 i, j;
	u8 *new_dsi;
	u32 new_size;
	GF_HEVCConfig *hvcc = NULL;
	// Profile, tier and level syntax ( nal class: Reserved and unspecified)
	hvcc = gf_odf_hevc_cfg_read(data, size, GF_FALSE); 
	if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;

	// for all the list objects in param_array
	for (i = 0; i < gf_list_count(hvcc->param_array); i++) { // hvcc->param_array:list object
		GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(hvcc->param_array, i); // ar contains the i-th item in param_array
		for (j = 0; j < gf_list_count(ar->nalus); j++) { // for all the nalus the i-th param got
			/*! used for storing AVC sequenceParameterSetNALUnit and pictureParameterSetNALUnit*/
			GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j); // store j-th nalus in *sl

			if (ar->type == GF_HEVC_NALU_SEQ_PARAM) {
				char *outSPS=NULL;
				u32 outSize=0;
				hevc_rewrite_sps(sl->data, sl->size, new_width, new_height, &outSPS, &outSize);
				gf_free(sl->data);
				sl->data = outSPS;
				sl->size= outSize;
			}
			else if (ar->type == GF_HEVC_NALU_VID_PARAM) {
			}
			else if (ar->type == GF_HEVC_NALU_PIC_PARAM) {
				char *outPPS=NULL;
				u32 outSize=0;
				hevcsplit_rewrite_pps_no_grid(ctx, sl->data, sl->size, &outPPS, &outSize);
				gf_free(sl->data);
				sl->data = outPPS;
				sl->size = outSize;
			}
		}
	}
	gf_odf_hevc_cfg_write(hvcc, &new_dsi, &new_size);
	gf_odf_hevc_cfg_del(hvcc);

	gf_filter_pid_set_property(opid, GF_PROP_PID_DECODER_CONFIG, &PROP_DATA_NO_COPY(new_dsi, new_size));
	return GF_OK;
}

static void hevcsplit_write_nal(char *output_nal, char *rewritten_nal, u32 out_nal_size, u32 hevc_nalu_size_length)
{
	u32 n = 8*(hevc_nalu_size_length);
	while (n) {
		u32 v = (out_nal_size >> (n-8)) & 0xFF;
		*output_nal = v;
		output_nal++;
		n-=8;
	}
	memcpy(output_nal, rewritten_nal, out_nal_size);
}

static GF_Err hevcsplit_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 cfg_crc = 0, codecid, o_width, o_height;
	s32 pps_id = -1, sps_id = -1;
	const GF_PropertyValue *p, *dsi;
	GF_Err e;
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx*)gf_filter_get_udta(filter);

	if (is_remove) {
		GF_FilterPid *opid;
		HEVCTilePid  *tpid;
		u32 count;
		count = gf_filter_get_opid_count(filter);
		for (u32 i=0; i<count; i++) {
			opid = gf_filter_get_opid(filter, i);
			tpid = gf_filter_pid_get_udta(opid);

			if (tpid->opid) {
				gf_filter_pid_remove(tpid->opid);
				tpid->opid = NULL;
			}
			if (tpid) gf_free(tpid);
			if(opid){
				gf_filter_pid_set_udta(opid, NULL);
				gf_filter_pid_remove(opid);
			}
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	// checks if input pid matchs its destination filter.
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	codecid = p ? p->value.uint : 0;
	if (!codecid || (codecid != GF_CODECID_HEVC)) {
		return GF_NOT_SUPPORTED;
	}

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	cfg_crc = 0;
	if (dsi && dsi->value.data.ptr && dsi->value.data.size) {
		cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
	}
	//same config, skip reconf
	if (cfg_crc == ctx->cfg_crc) return GF_OK;
	ctx->cfg_crc = cfg_crc;
	ctx->ipid = pid;

	// parse otherwise they should refer to something else
	u32 i, j;
	GF_HEVCConfig *hvcc = NULL;

	memset(&ctx->hevc_state, 0, sizeof(HEVCState));
	ctx->hevc_state.full_slice_header_parse = GF_TRUE;

	hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_FALSE);
	if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
	ctx->hevc_nalu_size_length = hvcc->nal_unit_size;

	for (i = 0; i < gf_list_count(hvcc->param_array); i++) { // hvcc->param_array:list object
		// ar contains the i-th item in param_array
		GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(hvcc->param_array, i);
		for (j = 0; j < gf_list_count(ar->nalus); j++) { // for all the nalus the i-th param got
			/*! used for storing AVC sequenceParameterSetNALUnit and pictureParameterSetNALUnit*/
			GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j); // store j-th nalus in *sl
			s32 idx;

			if (ar->type == GF_HEVC_NALU_SEQ_PARAM) {
				idx = gf_media_hevc_read_sps(sl->data, sl->size, &ctx->hevc_state);
				if ((idx >= 0) && sps_id<0)
					sps_id = idx;
			}
			else if (ar->type == GF_HEVC_NALU_PIC_PARAM) {
				s32 id = gf_media_hevc_read_pps(sl->data, sl->size, &ctx->hevc_state);
				if ((id >= 0) && pps_id<0)
					pps_id = id;
			}
			else if (ar->type == GF_HEVC_NALU_VID_PARAM) {
				/*s32 id = */ gf_media_hevc_read_vps(sl->data, sl->size, &ctx->hevc_state);
			}
		}
	}
	gf_odf_hevc_cfg_del(hvcc);

	if (pps_id < 0) return GF_NON_COMPLIANT_BITSTREAM;
	if (sps_id < 0) return GF_NON_COMPLIANT_BITSTREAM;

	o_width = ctx->hevc_state.sps[sps_id].width;
	o_height = ctx->hevc_state.sps[sps_id].height;
	ctx->num_tiles = ctx->hevc_state.pps[pps_id].num_tile_rows * ctx->hevc_state.pps[pps_id].num_tile_columns;

	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[HEVCTileSplit] Input stream %dx%d with %d tiles:\n", o_width, o_height, ctx->num_tiles));

	// Done with parsing SPS/pps/vps
	u32 rows = ctx->hevc_state.pps[pps_id].num_tile_rows;
	u32 cols = ctx->hevc_state.pps[pps_id].num_tile_columns;

	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			u32 tile_idx = i * rows + j;
			HEVCTilePid *tpid = gf_list_get(ctx->outputs, tile_idx);
			if (!tpid) {
				assert(gf_list_count(ctx->outputs) == tile_idx);

				GF_SAFEALLOC(tpid, HEVCTilePid);
				gf_list_add(ctx->outputs, tpid);
				tpid->opid = gf_filter_pid_new(filter);
				gf_filter_pid_set_udta(tpid->opid, tpid);
			}

			hevcsplit_get_tile_pixel_coords(&ctx->hevc_state, i, j, pps_id, &tpid->width, &tpid->height, &tpid->orig_x, &tpid->orig_y);
			gf_filter_pid_copy_properties(tpid->opid, ctx->ipid);
			// for each output pid, set decoder_config, width and height.
			gf_filter_pid_set_property(tpid->opid, GF_PROP_PID_WIDTH, &PROP_UINT(tpid->width));
			gf_filter_pid_set_property(tpid->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(tpid->height));
			gf_filter_pid_set_property(tpid->opid, GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(tpid->orig_x, tpid->orig_y));
			gf_filter_pid_set_property(tpid->opid, GF_PROP_PID_ORIG_SIZE, &PROP_VEC2I_INT(o_width, o_height));
			// rewrite the decoder config
			e = hevcsplit_rewrite_dsi(ctx, tpid->opid, dsi->value.data.ptr, dsi->value.data.size, tpid->width, tpid->height);
			if (e) return e;

			GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[HEVCTileSplit] output pid %dx%d (position was %dx%d)\n", tpid->width, tpid->height, tpid->orig_x, tpid->orig_y));
		}
	}
	// reaggregate to form complete frame.
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

static GF_Err hevcsplit_process(GF_Filter *filter)
{
	u32 nb_eos = 0, hevc_nalu_size_length;
	u32 data_size, nal_length, opid_idx = 0;
	u8 temporal_id, layer_id, nal_unit_type, i;
	u8 *data;
	u8 *output_nal, *rewritten_nal;
	u32 out_nal_size;
	GF_FilterPid * opid;
	HEVCTilePid * tpid;
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx*)gf_filter_get_udta(filter);
	GF_FilterPacket *pck_src = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck_src) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			nb_eos++;
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (u8*)gf_filter_pck_get_data(pck_src, &data_size);
	//this is a clock signaling, for now just trash ..
	if (!data) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}
	/*create a bitstream reader for the whole packet*/
	gf_bs_reassign_buffer(ctx->bs_au_in, data, data_size);
	ctx->nb_pck++;
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[HEVCTileSplit] splitting frame %d DTS "LLU" CTS "LLU"\n", ctx->nb_pck, gf_filter_pck_get_dts(pck_src), gf_filter_pck_get_cts(pck_src)));

	while (gf_bs_available(ctx->bs_au_in)) {
		// ctx->hevc_nalu_size_length filled using hvcc
		nal_length = gf_bs_read_int(ctx->bs_au_in, ctx->hevc_nalu_size_length * 8);
		u32 pos = (u32) gf_bs_get_position(ctx->bs_au_in);
		// skip the content of the nal from bs to buffer (useful for the next nal)
		gf_bs_skip_bytes(ctx->bs_au_in, nal_length);
		// data+pos is an address
		gf_media_hevc_parse_nalu(data+pos, nal_length, &ctx->hevc_state, &nal_unit_type, &temporal_id, &layer_id);

		// todo: might need to rewrite crypto info

		rewritten_nal = hevcsplit_rewrite_nal(filter, ctx, data+pos, nal_length, &opid_idx, &out_nal_size);
		if (!rewritten_nal) continue;

		hevc_nalu_size_length = ctx->hevc_nalu_size_length;
		//non-vcl NAL, forward to all outputs
		if (nal_unit_type > 34) {
			for (i=0; i<ctx->num_tiles; i++) {
				opid = gf_filter_get_opid(filter, i);
				tpid = gf_filter_pid_get_udta(opid);
				if (!tpid->opid) {
					continue;
				}
				if (!tpid->cur_pck) {
					tpid->cur_pck = gf_filter_pck_new_alloc(tpid->opid, ctx->hevc_nalu_size_length + out_nal_size, &output_nal);
					gf_filter_pck_merge_properties(pck_src, tpid->cur_pck);
				} else {
					u8 *data_start;
					u32 new_size;
					gf_filter_pck_expand(tpid->cur_pck, ctx->hevc_nalu_size_length + out_nal_size, &data_start, &output_nal, &new_size);
				}
				hevcsplit_write_nal(output_nal, rewritten_nal, out_nal_size, hevc_nalu_size_length);
			}
		} else {
			opid = gf_filter_get_opid(filter, opid_idx);
			tpid = gf_filter_pid_get_udta(opid);
			if (!tpid->opid) {
				continue;
			}
			if (!tpid->cur_pck) {
				tpid->cur_pck = gf_filter_pck_new_alloc(tpid->opid, ctx->hevc_nalu_size_length + out_nal_size, &output_nal);
				gf_filter_pck_merge_properties(pck_src, tpid->cur_pck);
			} else {
				u8 *data_start;
				u32 new_size;
				gf_filter_pck_expand(tpid->cur_pck, ctx->hevc_nalu_size_length + out_nal_size, &data_start, &output_nal, &new_size);
			}
			hevcsplit_write_nal(output_nal, rewritten_nal, out_nal_size, hevc_nalu_size_length);
		}
	}
	gf_filter_pid_drop_packet(ctx->ipid);
	// done rewriting all nals from input, send all output
	for (u32 idx = 0; idx < ctx->num_tiles; idx++) {
		opid = gf_filter_get_opid(filter, idx);
		tpid = gf_filter_pid_get_udta(opid);

		if (!tpid->opid) continue;
		if (tpid->cur_pck) {
			gf_filter_pck_send(tpid->cur_pck);
			tpid->cur_pck = NULL;
		}
	}
	return GF_OK;
}

static GF_Err hevcsplit_initialize(GF_Filter *filter)
{
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx *) gf_filter_get_udta(filter);
	ctx->bs_au_in = gf_bs_new((char *)ctx, 1, GF_BITSTREAM_READ);
	ctx->bs_nal_in = gf_bs_new((char *)ctx, 1, GF_BITSTREAM_READ);
	ctx->outputs = gf_list_new();
	return GF_OK;
}

static void hevcsplit_finalize(GF_Filter *filter)
{
	u32 i, count;
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx *) gf_filter_get_udta(filter);
	if (ctx->buffer_nal) gf_free(ctx->buffer_nal);
	if (ctx->output_no_epb) gf_free(ctx->output_no_epb);
	if (ctx->input_no_epb) gf_free(ctx->input_no_epb);

	gf_bs_del(ctx->bs_au_in);
	gf_bs_del(ctx->bs_nal_in);
	if (ctx->bs_nal_out) gf_bs_del(ctx->bs_nal_out);

	count = gf_list_count(ctx->outputs);
	for (i=0; i<count; i++) {
		HEVCTilePid *tpid;
		tpid = gf_list_get(ctx->outputs, i);
		gf_free(tpid);
	}
	gf_list_del(ctx->outputs);
}

static const GF_FilterCapability HEVCSplitCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC)
};

#define OFFS(_n)	#_n, offsetof(GF_HEVCSplitCtx, _n)

static const GF_FilterArgs HEVCSplitArgs[] =
{
	{0}
};

GF_FilterRegister HEVCSplitRegister = {
	.name = "hevcsplit",
	GF_FS_SET_DESCRIPTION("HEVC tile spliter")
	GF_FS_SET_HELP("This filter splits a motion-constrained tiled HEVC PID into N independent HEVC PIDs.")
	.private_size = sizeof(GF_HEVCSplitCtx),
	SETCAPS(HEVCSplitCaps),
	//hevc split shall be explicitly loaded
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.initialize = hevcsplit_initialize,
	.finalize = hevcsplit_finalize,
	.args = HEVCSplitArgs,
	.configure_pid = hevcsplit_configure_pid,
	.process = hevcsplit_process,
};

const GF_FilterRegister* hevcsplit_register(GF_FilterSession *session)
{
	return &HEVCSplitRegister;
}
