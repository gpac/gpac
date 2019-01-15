/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2010-2017
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
//#include "helper.h"
#include <gpac/internal/media_dev.h>

typedef struct
{
	GF_FilterPid *opid;
	u32 width, height, orig_x, orig_y;
	GF_FilterPacket *cur_pck;
} HEVCTilePid;

typedef struct
{
	//options
	//internal
	GF_FilterPid *ipid;
	GF_List *opids;
	HEVCTilePid tile_pids[64]; // GF_FilterPid *opid;
	u32 num_tiles;
	HEVCState hevc_state;
	u32 hevc_nalu_size_length, cfg_crc;
} GF_HEVCSplitCtx;

u32 bs_get_ue(GF_BitStream *bs);
s32 bs_get_se(GF_BitStream *bs);

/**
 * Visualization of VPS properties and return VPS ID
	 *
	 * @param buffer
	 * @param bz
	 * @param hevc
	 * @return
	 */
u32 parse_print_VPS(char *buffer, u32 bz, HEVCState* hevc) {
	u32 i = gf_media_hevc_read_vps(buffer, bz, hevc);
	printf("=== Visualization of VPS with id: %d ===\n", (*hevc).vps[i].id);
	printf("num_profile_tier_level:	%hhu\n", (*hevc).vps[i].num_profile_tier_level);
	return i;
}

/**
 * Visualization of SPS properties and return SPS ID
	 *
	 * @param buffer
	 * @param bz
	 * @param hevc
	 * @return
	 */
u32 parse_print_SPS(char *buffer, u32 bz, HEVCState* hevc) {

	u32 i = gf_media_hevc_read_sps(buffer, bz, hevc);
	printf("=== Visualization of SPS with id: %d ===\n", (*hevc).sps[i].id);
	printf("width:	%u\n", (*hevc).sps[i].width);
	printf("height:	%u\n", (*hevc).sps[i].height);
	printf("pic_width_in_luma_samples:	%u\n", (*hevc).sps[i].max_CU_width);
	printf("pic_heigth_in_luma_samples:	%u\n", (*hevc).sps[i].max_CU_height);
	printf("cw_left:	%u\n", (*hevc).sps[i].cw_left);
	printf("cw_right:	%u\n", (*hevc).sps[i].cw_right);
	printf("cw_top:	%u\n", (*hevc).sps[i].cw_top);
	printf("cw_bottom:	%u\n", (*hevc).sps[i].cw_bottom);
	return i;
}

/**
 * Visualization of PPS properties and return PPS ID
 * @param buffer
 * @param bz
 * @param hevc
 * @return
 */

u32 parse_print_PPS(char *buffer, u32 bz, HEVCState* hevc) {
	u32 i = gf_media_hevc_read_pps(buffer, bz, hevc);
	printf("=== Visualization of PPS with id: %d ===\n", (*hevc).pps[i].id);
	printf("tiles_enabled_flag:	%u\n", (*hevc).pps[i].tiles_enabled_flag);
	printf("uniform_spacing_flag:	%d\n", (*hevc).pps[i].uniform_spacing_flag);
	printf("num_tile_columns:	%u\n", (*hevc).pps[i].num_tile_columns);
	printf("num_tile_rows:	%u\n", (*hevc).pps[i].num_tile_rows);
	return i;
}

void bs_set_ue(GF_BitStream *bs, u32 num) {
	s32 length = 1;
	s32 temp = ++num;

	while (temp != 1) {
		temp >>= 1;
		length += 2;
	}

	gf_bs_write_int(bs, 0, length >> 1);
	gf_bs_write_int(bs, num, (length + 1) >> 1);
}

void bs_set_se(GF_BitStream *bs, s32 num)
{
	u32 v;
	if (num <= 0)
		v = (-1 * num) << 1;
	else
		v = (num << 1) - 1;

	bs_set_ue(bs, v);
}

u32 new_address(int x, int y, u32 num_CTU_height[], u32 num_CTU_width[], int num_CTU_width_tot, u32 sliceAdresse)
{
	int sum_height = 0, sum_width = 0, adress = 0;
	int i, j;
	for (i = 0; i < x; i++) sum_height += num_CTU_height[i];
	for (j = 0; j < y; j++) sum_width += num_CTU_width[j];
	sum_height += (sliceAdresse / num_CTU_width[j]);
	sum_width += (sliceAdresse % num_CTU_width[j]);
	adress = sum_height * num_CTU_width_tot + sum_width;

	//adress= sum_height;
	return adress;
}

u32 sum_array(u32 *array, u32 length)
{
	u32 i, sum = 0;
	for (i = 0; i < length; i++)
	{
		sum += array[i];
	}
	return sum;
}

void slice_address_calculation(HEVCState *hevc, u32 *address, u32 tile_x, u32 tile_y)
{
	HEVCSliceInfo *si = &hevc->s_info;
	u32 PicWidthInCtbsY;

	PicWidthInCtbsY = si->sps->width / si->sps->max_CU_width;
	if (PicWidthInCtbsY * si->sps->max_CU_width < si->sps->width) PicWidthInCtbsY++;

	*address = tile_x / si->sps->max_CU_width + (tile_y / si->sps->max_CU_width) * PicWidthInCtbsY;
}
//Transform slice 1d address into 2D address
u32 get_newSliceAddress_and_tilesCordinates(u32 *x, u32 *y, HEVCState *hevc)
{
	HEVCSliceInfo si = hevc->s_info;
	u32 i, tbX = 0, tbY = 0, slX, slY, PicWidthInCtbsY, PicHeightInCtbsX, valX = 0, valY = 0;

	PicWidthInCtbsY = (si.sps->width + si.sps->max_CU_width - 1) / si.sps->max_CU_width;
	PicHeightInCtbsX = (si.sps->height + si.sps->max_CU_height - 1) / si.sps->max_CU_height;

	slY = si.slice_segment_address % PicWidthInCtbsY;
	slX = si.slice_segment_address / PicWidthInCtbsY;
	*x = 0;
	*y = 0;
	if (si.pps->tiles_enabled_flag)
	{
		if (si.pps->uniform_spacing_flag)
		{
			for (i = 0; i < si.pps->num_tile_rows; i++)
			{
				if (i < si.pps->num_tile_rows - 1)
					valX = (i + 1) * PicHeightInCtbsX / si.pps->num_tile_rows - i * PicHeightInCtbsX / si.pps->num_tile_rows;
				else
					valX = PicHeightInCtbsX - tbX;
				if (slX < tbX + valX)
				{
					*x = i;
					break;
				}
				else
					tbX += valX;
			}
			for (i = 0; i < si.pps->num_tile_columns; i++)
			{
				if (i < si.pps->num_tile_columns - 1)
					valY = (i + 1) * PicWidthInCtbsY / si.pps->num_tile_columns - i * PicWidthInCtbsY / si.pps->num_tile_columns;
				else
					valY = PicWidthInCtbsY - tbY;
				if (slY < tbY + valY)
				{
					*y = i;
					break;
				}
				else
					tbY += valY;
			}
		}
		else
		{
			for (i = 0; i < si.pps->num_tile_rows; i++)
			{
				if (i < si.pps->num_tile_rows - 1)
					valX = si.pps->row_height[i];
				else
					valX = PicHeightInCtbsX - tbX;
				if (slX < tbX + valX)
				{
					*x = i;
					break;
				}
				else
					tbX += valX;
			}
			for (i = 0; i < si.pps->num_tile_columns; i++)
			{
				if (i < si.pps->num_tile_columns - 1)
					valY = si.pps->column_width[i];
				else
					valY = PicWidthInCtbsY - tbY;
				if (slY < tbY + valY)
				{
					*y = i;
					break;
				}
				else
					tbY += valY;
			}
		}
	}

	return (slX - tbX)*valX + slY - tbY;
}
//get the width and the height of the tile in pixel
void get_size_of_tile(HEVCState *hevc, u32 index_row, u32 index_col, u32 pps_id, u32 *width, u32 *height, u32 *tx, u32 *ty)
{
	u32 i, sps_id, tbX = 0, tbY = 0, PicWidthInCtbsY, PicHeightInCtbsX, max_CU_width, max_CU_height;

	sps_id = hevc->pps[pps_id].sps_id;
	max_CU_width = hevc->sps[sps_id].max_CU_width;
	max_CU_height = hevc->sps[sps_id].max_CU_height;
	PicWidthInCtbsY = (hevc->sps[sps_id].width + max_CU_width - 1) / max_CU_width;
	PicHeightInCtbsX = (hevc->sps[sps_id].height + max_CU_height - 1) / max_CU_height;

	if (tx) *tx = 0;
	if (ty) *ty = 0;

	if (hevc->pps[pps_id].tiles_enabled_flag)
	{
		if (hevc->pps[pps_id].uniform_spacing_flag)
		{
			if (index_row < hevc->pps[pps_id].num_tile_rows - 1) {
				for (i = 0; i < index_row; i++)
					tbX += ((i + 1) * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows - i * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows);

				*height = (index_row + 1) * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows - index_row * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows;
			}
			else {
				for (i = 0; i < hevc->pps[pps_id].num_tile_rows - 1; i++)
					tbX += ((i + 1) * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows - i * PicHeightInCtbsX / hevc->pps[pps_id].num_tile_rows);
				*height = PicHeightInCtbsX - tbX;
			}

			if (index_col < hevc->pps[pps_id].num_tile_columns - 1) {
				for (i = 0; i < index_col; i++)
					tbY += ((i + 1) * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns - i * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns);
				*width = (index_col + 1) * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns - index_col * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns;
			}
			else {
				for (i = 0; i < hevc->pps[pps_id].num_tile_columns - 1; i++)
					tbY += ((i + 1) * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns - i * PicWidthInCtbsY / hevc->pps[pps_id].num_tile_columns);
				*width = PicWidthInCtbsY - tbY;
			}
		}
		else
		{
			if (index_row < hevc->pps[pps_id].num_tile_rows - 1)
				*height = hevc->pps[pps_id].row_height[index_row];
			else {
				for (i = 0; i < hevc->pps[pps_id].num_tile_rows - 1; i++)
					tbX += hevc->pps[pps_id].row_height[i];
				*height = PicHeightInCtbsX - tbX;
			}
			if (index_col < hevc->pps[pps_id].num_tile_columns - 1)
				*width = hevc->pps[pps_id].column_width[index_col];
			else
			{
				for (i = 0; i < hevc->pps[pps_id].num_tile_columns - 1; i++)
					tbY += hevc->pps[pps_id].column_width[i];
				*width = PicWidthInCtbsY - tbY;
			}

		}
		*width = *width * max_CU_width;
		*height = *height * max_CU_height;
		if (ty) *ty = tbX * max_CU_height;
		if (tx) *tx = tbY * max_CU_width;
	}
}
//rewrite the profile and level
void write_profile_tier_level(GF_BitStream *bs_in, GF_BitStream *bs_out, Bool ProfilePresentFlag, u8 MaxNumSubLayersMinus1)
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

void rewrite_SPS(char *in_SPS, u32 in_SPS_length, u32 width, u32 height, HEVCState *hevc, char **out_SPS, u32 *out_SPS_length)
{
	GF_BitStream *bs_in, *bs_out;
	u64 length_no_use = 0;
	char *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0, sps_ext_or_max_sub_layers_minus1;
	u8 max_sub_layers_minus1 = 0, layer_id;
	Bool conformance_window_flag, multiLayerExtSpsFlag;
	u32 chroma_format_idc;

	bs_in = gf_bs_new(in_SPS, in_SPS_length, GF_BITSTREAM_READ);
	gf_bs_enable_emulation_byte_removal(bs_in, GF_TRUE);
	bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
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
		write_profile_tier_level(bs_in, bs_out, 1, max_sub_layers_minus1);
	}

	bs_set_ue(bs_out, bs_get_ue(bs_in)); //copy sps_id

	if (multiLayerExtSpsFlag) {
		u8 update_rep_format_flag = gf_bs_read_int(bs_in, 1);
		gf_bs_write_int(bs_out, update_rep_format_flag, 1);
		if (update_rep_format_flag) {
			gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 8), 8);
		}
	}
	else {
		u32 w, h;
		chroma_format_idc = bs_get_ue(bs_in);
		bs_set_ue(bs_out, chroma_format_idc);
		if (chroma_format_idc == 3)
			gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1); // copy separate_colour_plane_flag
		w = bs_get_ue(bs_in); //skip width bits in input bitstream
		h = bs_get_ue(bs_in); //skip height bits in input bitstream
		//get_size_of_slice(&width, &height, hevc); // get the width and height of the tile

		//Copy the new width and height in output bitstream
		bs_set_ue(bs_out, width);
		bs_set_ue(bs_out, height);

		//Get rid of the bit conformance_window_flag
		conformance_window_flag = gf_bs_read_int(bs_in, 1);
		//put the new conformance flag to zero
		gf_bs_write_int(bs_out, 0, 1);

		//Skip the bits related to conformance_window_offset
		if (conformance_window_flag)
		{
			bs_get_ue(bs_in);
			bs_get_ue(bs_in);
			bs_get_ue(bs_in);
			bs_get_ue(bs_in);
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
	gf_media_nalu_add_emulation_bytes(data_without_emulation_bytes, *out_SPS, data_without_emulation_bytes_size);

exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
	gf_free(data_without_emulation_bytes);
}

void rewrite_PPS(Bool extract, char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length, u32 num_tile_columns_minus1, u32 num_tile_rows_minus1, u32 uniform_spacing_flag, u32 column_width_minus1[], u32 row_height_minus1[])
{
	u64 length_no_use = 0;
	u8 cu_qp_delta_enabled_flag, tiles_enabled_flag, loop_filter_across_slices_enabled_flag;

	GF_BitStream *bs_in;
	GF_BitStream *bs_out;

	char *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0;

	bs_in = gf_bs_new(in_PPS, in_PPS_length, GF_BITSTREAM_READ);
	gf_bs_enable_emulation_byte_removal(bs_in, GF_TRUE);
	bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
	if (!bs_in) goto exit;

	//Read and write NAL header bits
	gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 16), 16);
	bs_set_ue(bs_out, bs_get_ue(bs_in)); //pps_pic_parameter_set_id
	bs_set_ue(bs_out, bs_get_ue(bs_in)); //pps_seq_parameter_set_id
	gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 7), 7); //from dependent_slice_segments_enabled_flag to cabac_init_present_flag
	bs_set_ue(bs_out, bs_get_ue(bs_in)); //num_ref_idx_l0_default_active_minus1
	bs_set_ue(bs_out, bs_get_ue(bs_in)); //num_ref_idx_l1_default_active_minus1
	bs_set_se(bs_out, bs_get_se(bs_in)); //init_qp_minus26
	gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 2), 2); //from constrained_intra_pred_flag to transform_skip_enabled_flag
	cu_qp_delta_enabled_flag = gf_bs_read_int(bs_in, 1); //cu_qp_delta_enabled_flag
	gf_bs_write_int(bs_out, cu_qp_delta_enabled_flag, 1); //
	if (cu_qp_delta_enabled_flag)
		bs_set_ue(bs_out, bs_get_ue(bs_in)); // diff_cu_qp_delta_depth
	bs_set_se(bs_out, bs_get_se(bs_in)); // pps_cb_qp_offset
	bs_set_se(bs_out, bs_get_se(bs_in)); // pps_cr_qp_offset
	gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 4), 4); // from pps_slice_chroma_qp_offsets_present_flag to transquant_bypass_enabled_flag

	//tile//////////////////////

	if (extract)
	{
		tiles_enabled_flag = gf_bs_read_int(bs_in, 1); // tiles_enabled_flag
		gf_bs_write_int(bs_out, 0, 1);
		gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1); // entropy_coding_sync_enabled_flag
		if (tiles_enabled_flag)
		{
			u32 num_tile_columns_minus1 = bs_get_ue(bs_in);
			u32 num_tile_rows_minus1 = bs_get_ue(bs_in);
			u8 uniform_spacing_flag = gf_bs_read_int(bs_in, 1);
			if (!uniform_spacing_flag)
			{
				u32 i;
				for (i = 0; i < num_tile_columns_minus1; i++)
					bs_get_ue(bs_in);
				for (i = 0; i < num_tile_rows_minus1; i++)
					bs_get_ue(bs_in);
			}
			gf_bs_read_int(bs_in, 1);
		}
	}
	else {
		u32 loop_filter_across_tiles_enabled_flag;
		gf_bs_read_int(bs_in, 1);
		gf_bs_write_int(bs_out, 1, 1);//tiles_enable_flag ----from 0 to 1
		tiles_enabled_flag = 1;//always enable
		gf_bs_write_int(bs_out, gf_bs_read_int(bs_in, 1), 1);//entropy_coding_sync_enabled_flag    
		bs_set_ue(bs_out, num_tile_columns_minus1);//write num_tile_columns_minus1 
		bs_set_ue(bs_out, num_tile_rows_minus1);//num_tile_rows_minus1
		gf_bs_write_int(bs_out, uniform_spacing_flag, 1);  //uniform_spacing_flag

		if (!uniform_spacing_flag)
		{
			u32 i;
			for (i = 0; i < num_tile_columns_minus1; i++)
				bs_set_ue(bs_out, column_width_minus1[i] - 1);//column_width_minus1[i]
			for (i = 0; i < num_tile_rows_minus1; i++)
				bs_set_ue(bs_out, row_height_minus1[i] - 1);//row_height_minus1[i]
		}
		loop_filter_across_tiles_enabled_flag = 1;//loop_filter_across_tiles_enabled_flag
		gf_bs_write_int(bs_out, loop_filter_across_tiles_enabled_flag, 1);
	}

	loop_filter_across_slices_enabled_flag = gf_bs_read_int(bs_in, 1);
	gf_bs_write_int(bs_out, loop_filter_across_slices_enabled_flag, 1);

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

	*out_PPS_length = data_without_emulation_bytes_size + gf_media_nalu_emulation_bytes_add_count(data_without_emulation_bytes, data_without_emulation_bytes_size);
	*out_PPS = gf_malloc(*out_PPS_length);
	gf_media_nalu_add_emulation_bytes(data_without_emulation_bytes, *out_PPS, data_without_emulation_bytes_size);

exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
	gf_free(data_without_emulation_bytes);
}

void rewrite_slice_address(s32 new_address, char *in_slice, u32 in_slice_length, char **out_slice, u32 *out_slice_length, HEVCState* hevc, u32 final_width, u32 final_height)
{
	char *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0;
	char *dst_buf = NULL;
	u32 dst_buf_size = 0;
	u32 header_end, num_entry_point_start;
	u32 pps_id;
	GF_BitStream *bs_ori, *bs_rw;
	Bool IDRPicFlag = GF_FALSE;
	Bool RapPicFlag = GF_FALSE;
	u32 nb_bits_per_adress_dst = 0;
	u8 nal_unit_type;
	HEVC_PPS *pps;
	HEVC_SPS *sps;
	u32 al, slice_size, slice_offset_orig, slice_offset_dst;
	u32 first_slice_segment_in_pic_flag;
	u32 dependent_slice_segment_flag;
	int address_ori;

	bs_ori = gf_bs_new(in_slice, in_slice_length, GF_BITSTREAM_READ);
	gf_bs_enable_emulation_byte_removal(bs_ori, GF_TRUE);
	bs_rw = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	assert(hevc->s_info.header_size_bits >= 0);
	assert(hevc->s_info.entry_point_start_bits >= 0);
	header_end = (u32)hevc->s_info.header_size_bits;

	num_entry_point_start = (u32)hevc->s_info.entry_point_start_bits;

	//nal_unit_header			 
	gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	nal_unit_type = gf_bs_read_int(bs_ori, 6);
	gf_bs_write_int(bs_rw, nal_unit_type, 6);
	gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 9), 9);


	first_slice_segment_in_pic_flag = gf_bs_read_int(bs_ori, 1);    //first_slice_segment_in_pic_flag
	if (new_address <= 0)
		gf_bs_write_int(bs_rw, 1, 1);
	else
		gf_bs_write_int(bs_rw, 0, 1);


	switch (nal_unit_type) {
	case GF_HEVC_NALU_SLICE_IDR_W_DLP:
	case GF_HEVC_NALU_SLICE_IDR_N_LP:
		IDRPicFlag = GF_TRUE;
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
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	}

	pps_id = bs_get_ue(bs_ori);					 //pps_id
	bs_set_ue(bs_rw, pps_id);

	pps = &hevc->pps[pps_id];
	sps = &hevc->sps[pps->sps_id];

	dependent_slice_segment_flag = 0;
	if (!first_slice_segment_in_pic_flag && pps->dependent_slice_segments_enabled_flag) //dependent_slice_segment_flag READ
	{
		dependent_slice_segment_flag = gf_bs_read_int(bs_ori, 1);
	}
	else
	{
		dependent_slice_segment_flag = GF_FALSE;
	}
	if (!first_slice_segment_in_pic_flag) 						    //slice_segment_address READ
	{
		address_ori = gf_bs_read_int(bs_ori, sps->bitsSliceSegmentAddress);
	}
	else {} 	//original slice segment address = 0

	if (new_address > 0) //new address != 0
	{
		if (pps->dependent_slice_segments_enabled_flag)				    //dependent_slice_segment_flag WRITE
		{
			gf_bs_write_int(bs_rw, dependent_slice_segment_flag, 1);
		}

		if (final_width && final_height) {
			u32 nb_CTUs = ((final_width + sps->max_CU_width - 1) / sps->max_CU_width) * ((final_height + sps->max_CU_height - 1) / sps->max_CU_height);
			nb_bits_per_adress_dst = 0;
			while (nb_CTUs > (u32)(1 << nb_bits_per_adress_dst)) {
				nb_bits_per_adress_dst++;
			}
		}
		else {
			nb_bits_per_adress_dst = sps->bitsSliceSegmentAddress;
		}

		gf_bs_write_int(bs_rw, new_address, nb_bits_per_adress_dst);	    //slice_segment_address WRITE
	}
	else {} //new_address = 0

	//copy over until num_entry_points
	while (num_entry_point_start != (gf_bs_get_position(bs_ori) - 1) * 8 + gf_bs_get_bit_position(bs_ori)) //Copy till the end of the header
	{
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	}
	//write num_entry_points
	if (new_address >= 0) {
		bs_set_ue(bs_rw, 0);
	}

	//write slice extension to 0
	if (pps->slice_segment_header_extension_present_flag)
		gf_bs_write_int(bs_rw, 0, 1);

	//byte_alignment() is bit=1 + x bit=0
	gf_bs_write_int(bs_rw, 1, 1);
	gf_bs_align(bs_rw);					//align

	while (header_end != (gf_bs_get_position(bs_ori) - 1) * 8 + gf_bs_get_bit_position(bs_ori))
		gf_bs_read_int(bs_ori, 1);


	al = gf_bs_read_int(bs_ori, 1);
	assert(al == 1);
	gf_bs_align(bs_ori);

#if 1
	gf_bs_get_content(bs_rw, &dst_buf, &dst_buf_size);
	slice_size = (u32) gf_bs_available(bs_ori);
	slice_offset_orig = (u32)gf_bs_get_position(bs_ori);
	slice_offset_dst = dst_buf_size;
	dst_buf_size += slice_size;
	dst_buf = gf_realloc(dst_buf, sizeof(char)*dst_buf_size);
	memcpy(dst_buf + slice_offset_dst, in_slice + slice_offset_orig, sizeof(char) * slice_size);

	data_without_emulation_bytes = dst_buf;
	data_without_emulation_bytes_size = dst_buf_size;

#else
	while (gf_bs_available(bs_ori)) {
		gf_bs_write_u8(bs_rw, gf_bs_read_u8(bs_ori));
	}
	gf_bs_truncate(bs_rw);

	gf_free(data_without_emulation_bytes);
	data_without_emulation_bytes = NULL;
	data_without_emulation_bytes_size = 0;

	gf_bs_get_content(bs_rw, &data_without_emulation_bytes, &data_without_emulation_bytes_size);
#endif

	*out_slice_length = data_without_emulation_bytes_size + gf_media_nalu_emulation_bytes_add_count(data_without_emulation_bytes, data_without_emulation_bytes_size);
	*out_slice = gf_malloc(*out_slice_length);

	gf_media_nalu_add_emulation_bytes(data_without_emulation_bytes, *out_slice, data_without_emulation_bytes_size);


	gf_bs_del(bs_ori);
	gf_bs_del(bs_rw);
	gf_free(data_without_emulation_bytes);

}

char* extract(HEVCState *hevc, char *buffer, u32 buffer_length, u32 *original_coord, u32 *out_size)
{
	char *buffer_temp = NULL;
	u8 nal_unit_type, temporal_id, layer_id;
	u32 buffer_temp_length = 0, newAddress,i,j;
	s32 pps_id = -1;

	gf_media_hevc_parse_nalu(buffer, buffer_length, hevc, &nal_unit_type, &temporal_id, &layer_id);
	switch (nal_unit_type) {
		// For all those cases
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

		newAddress = get_newSliceAddress_and_tilesCordinates(&i, &j, hevc);
		*original_coord = i * hevc->s_info.pps->num_tile_columns + j; // matching_opid (or *original_coord = newAddress)
		//rewrite and copy the Slice
		rewrite_slice_address(-1, buffer, buffer_length, &buffer_temp, &buffer_temp_length, hevc, 0, 0);

		*out_size = buffer_temp_length;
		return buffer_temp;

	default: // if(nal_unit_type <= GF_HEVC_NALU_SLICE_CRA)  // VCL
		*out_size = buffer_length;
		return buffer;
	}
	// return NULL;
}

static GF_Err rewrite_hevc_dsi(GF_HEVCSplitCtx *ctx , GF_FilterPid *opid, char *data, u32 size, u32 new_width, u32 new_height)
{
	u32 i, j;
	char *new_dsi;
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
			u16 hdr = sl->data[0] << 8 | sl->data[1];

			if (ar->type == GF_HEVC_NALU_SEQ_PARAM) {
				char *outSPS;
				u32 outSize;
				rewrite_SPS(sl->data, sl->size, new_width, new_height, &ctx->hevc_state, &outSPS, &outSize);
				gf_free(sl->data);
				sl->data = outSPS;
				sl->size= outSize;
			}
			else if (ar->type == GF_HEVC_NALU_VID_PARAM) {
			}
			else if (ar->type == GF_HEVC_NALU_PIC_PARAM) {
				char *outPPS;
				u32 outSize;
				rewrite_PPS(GF_TRUE, sl->data, sl->size, &outPPS, &outSize, 0, 0, 0, NULL, NULL);
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

static GF_Err hevcsplit_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	u32 dep_id = 0, id = 0, cfg_crc = 0, codecid, did_it = 0, o_width, o_height;
	s32 pps_id = -1, sps_id = -1;
	Bool has_scalable = GF_FALSE;
	Bool is_sublayer = GF_FALSE;
	const GF_PropertyValue *p, *dsi;
	GF_Err e;
	// private stack of the filter returned as an object GF_HEVCSplitCtx.
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx*)gf_filter_get_udta(filter); 

	if (is_remove) {
		// todo!  gf_filter_pid_remove(ctx->opid); on all output pids
		return GF_OK;
	}
	if (!gf_filter_pid_check_caps(pid)) // checks if input pid matchs its destination filter.
		return GF_NOT_SUPPORTED;

	// Gets the build in property of the pid: here the Id of the pid.
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID); 
	// If null (no Id), Gets the build in property of the pid: here ESID Id of the pid.
	if (!p) p = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID); 
	/* 'p' is an object fulfilled above, retrieve from it the variable value 
	as an unsigned int. Q: '.uint' not in the doc, otherwise where */
	if (p) id = p->value.uint; 

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	codecid = p ? p->value.uint : 0; // If p isn't null, get the codecid else codec id = 0;
	if (!codecid) return GF_NOT_SUPPORTED; // if codecid = 0;  
	if (codecid != GF_CODECID_HEVC) {
		return GF_NOT_SUPPORTED; // codec must be HEVC
	}

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	cfg_crc = 0;
	if (dsi && dsi->value.data.ptr && dsi->value.data.size) {
		// Takes buffer and buffer size to return an u32
		cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size); 
	}

	if (cfg_crc == ctx->cfg_crc) return GF_OK;
	ctx->cfg_crc = cfg_crc;
	ctx->ipid = pid;

	// parse otherwise they should refer to something else
	u32 i, j;
	GF_HEVCConfig *hvcc = NULL;
		
	memset(&ctx->hevc_state, 0, sizeof(HEVCState));
	ctx->hevc_state.full_slice_header_parse = GF_TRUE;

	// Profile, tier and level syntax ( nal class: Reserved and unspecified)
	hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_FALSE); 
	if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
	ctx->hevc_nalu_size_length = hvcc->nal_unit_size;
	// for all the list objects in param_array
	for (i = 0; i < gf_list_count(hvcc->param_array); i++) { // hvcc->param_array:list object
		 // ar contains the i-th item in param_array
		GF_HEVCParamArray *ar = (GF_HEVCParamArray *)gf_list_get(hvcc->param_array, i);
		for (j = 0; j < gf_list_count(ar->nalus); j++) { // for all the nalus the i-th param got
			/*! used for storing AVC sequenceParameterSetNALUnit and pictureParameterSetNALUnit*/
			GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j); // store j-th nalus in *sl
			s32 idx;
			u16 hdr = sl->data[0] << 8 | sl->data[1];

			if (ar->type == GF_HEVC_NALU_SEQ_PARAM) {
				idx = gf_media_hevc_read_sps(sl->data, sl->size, &ctx->hevc_state);
				if ((idx >= 0) && sps_id<0)
					sps_id = idx;
			}
			else if (ar->type == GF_HEVC_NALU_VID_PARAM) {
				s32 vps_id = gf_media_hevc_read_vps(sl->data, sl->size, &ctx->hevc_state);
			}
			else if (ar->type == GF_HEVC_NALU_PIC_PARAM) {
				s32 id = gf_media_hevc_read_pps(sl->data, sl->size, &ctx->hevc_state);
				if ((id >= 0) && pps_id<0) 
					pps_id = id;
			}
		}
	}
	gf_odf_hevc_cfg_del(hvcc);

	if (pps_id < 0) return GF_NON_COMPLIANT_BITSTREAM;
	if (sps_id < 0) return GF_NON_COMPLIANT_BITSTREAM;
	// ORIGINAL_SIZE
	o_width = ctx->hevc_state.sps[sps_id].width;
	o_height = ctx->hevc_state.sps[sps_id].height;
	ctx->num_tiles = ctx->hevc_state.pps[pps_id].num_tile_rows * ctx->hevc_state.pps[pps_id].num_tile_columns;

	// Done with parsing SPS/pps/vps
	u32 rows = ctx->hevc_state.pps[pps_id].num_tile_rows;
	u32 cols = ctx->hevc_state.pps[pps_id].num_tile_columns;
	for (i = 0; i < rows; i++) {
		for (j = 0; j < cols; j++) {
			u32 idx = i * cols + j;
			HEVCTilePid *tpid = &ctx->tile_pids[idx];
			get_size_of_tile(&ctx->hevc_state, i, j, pps_id, &tpid->width, &tpid->height, &tpid->orig_x, &tpid->orig_y);

			if (!tpid->opid) {
				tpid->opid = gf_filter_pid_new(filter);
			}
			// Best practice is to copy all properties of ipid in opid, then reassign properties changed by the filter
			gf_filter_pid_copy_properties(tpid->opid, ctx->ipid);
			// for each output pid, set decoder_config, width and height.
			gf_filter_pid_set_property(tpid->opid, GF_PROP_PID_WIDTH, &PROP_UINT(tpid->width));
			gf_filter_pid_set_property(tpid->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(tpid->height));
			gf_filter_pid_set_property(tpid->opid, GF_PROP_PID_CROP_POS, &PROP_VEC2I_INT(tpid->orig_x, tpid->orig_y));
			gf_filter_pid_set_property(tpid->opid, GF_PROP_PID_ORIG_SIZE, &PROP_VEC2I_INT(o_width, o_height));

			// rewrite the decoder config
			e = rewrite_hevc_dsi(ctx, tpid->opid, dsi->value.data.ptr, dsi->value.data.size, tpid->width, tpid->height);
			if (e) return e;
		}
	}

	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

static GF_Err hevcsplit_process(GF_Filter *filter)
{
	u64 min_dts = GF_FILTER_NO_TS;
	u64 min_cts = GF_FILTER_NO_TS;
	u32 idx, nb_eos = 0;
	u32 data_size, nal_length, opid_idx = 0, num_nal = 1; // On default we have a nalu
	char *data;
	Bool has_pic = GF_FALSE;
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx*)gf_filter_get_udta(filter);
	GF_BitStream *bs; // GF_BitStream *bs[MAX_TILES];
	HEVCState hevc;

	memset(&hevc, 0, sizeof(HEVCState));
	hevc.full_slice_header_parse = 1;
	// Gets the first packet in the input pid buffer
	GF_FilterPacket *pck_src = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck_src) {
		if (gf_filter_pid_is_eos(ctx->ipid)) { // if end of stream
			nb_eos++;
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (char *)gf_filter_pck_get_data(pck_src, &data_size); // data contains only a packet 
	//TODO: this is a clock signaling, for now just trash ..
	if (!data) {
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OK;
	}
	//create a bitstream reader from data
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ); // read from there

	char *output_nal, *rewritten_nal;
	u32 out_nal_size;
	while (gf_bs_available(bs))
	{
		// ctx->hevc_nalu_size_length filled using hvcc
		nal_length = gf_bs_read_int(bs, ctx->hevc_nalu_size_length * 8); 
		u32 pos = (u32) gf_bs_get_position(bs);
		// skip the content of the nal from bs to buffer
		gf_bs_skip_bytes(bs, nal_length); 
		// data+pos is an address 
		rewritten_nal = extract(&ctx->hevc_state, data+pos, nal_length, &opid_idx, &out_nal_size); 

		// todo: copy source NAL to ALL destination pids
		if (!rewritten_nal) continue;
		/* 	gf_media_hevc_parse_nalu(rewritten_nal, out_nal_size, hevc, &nal_unit_type, &temporal_id, &layer_id);
		use this function to determine if the return is an SEI, if it's then copy to all the destinations.
		*/	
		HEVCTilePid *tpid = &ctx->tile_pids[opid_idx];
		if (!tpid->opid) {
			// ERROR
			continue;
		}
		// cur_pck is the destination packet, if first usage, then
		if (!tpid->cur_pck) {
			// ctx->hevc_nalu_size_length (field used to indicate the nal_length) + out_nal_size: (the nal itself)
			tpid->cur_pck = gf_filter_pck_new_alloc(tpid->opid, ctx->hevc_nalu_size_length + out_nal_size, &output_nal);
			// todo: might need to rewrite crypto info
			gf_filter_pck_merge_properties(pck_src, tpid->cur_pck);
		}
		else { // to be rexamined !
			char *data_start;
			u32 new_size;
			gf_filter_pck_expand(tpid->cur_pck, ctx->hevc_nalu_size_length + out_nal_size, &data_start, &output_nal, &new_size);
		}
		// ctx->hevc_nalu_size_length = hvcc->nal_unit_size;
		GF_BitStream *bsnal = gf_bs_new(output_nal, ctx->hevc_nalu_size_length, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bsnal, out_nal_size, 8 * ctx->hevc_nalu_size_length); // bsnal is full up there !!.
		// To output_nal, skeep ctx->hevc_nalu_size_length then write "rewritten_nal" on out_nal_size Bytes
		//memcpy(output_nal, out_nal_size, ctx->hevc_nalu_size_length);
		memcpy(output_nal + ctx->hevc_nalu_size_length, rewritten_nal, out_nal_size);
		gf_bs_del(bsnal);
	}
	gf_filter_pid_drop_packet(ctx->ipid);

	// done rewriting all nals from input, send all output
	for (idx = 0; idx < ctx->num_tiles; idx++) {
		HEVCTilePid *tpid = &ctx->tile_pids[idx];
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
	return GF_OK;
}

static void hevcsplit_finalize(GF_Filter *filter)
{
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx *) gf_filter_get_udta(filter);
}

static const GF_FilterCapability HEVCSplitCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	//CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,GF_PROP_PID_TILE_BASE, GF_TRUE),
	//CAP_BOOL(GF_CAPS_INPUT,GF_PROP_PID_TILE_BASE, GF_TRUE),

	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC)
};

#define OFFS(_n)	#_n, offsetof(GF_HEVCSplitCtx, _n)

static const GF_FilterArgs HEVCSplitArgs[] =
{
//	{ OFFS(threading), "Set threading mode", GF_PROP_UINT, "frame", "frameslice|frame|slice", GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister HEVCSplitRegister = {
	.name = "hevcsplit",
	GF_FS_SET_DESCRIPTION("OpenHEVC decoder")
	.private_size = sizeof(GF_HEVCSplitCtx),
	SETCAPS(HEVCSplitCaps),
	//hevc split shall be explicetely loaded
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.initialize = hevcsplit_initialize,
	.finalize = hevcsplit_finalize,
	.args = HEVCSplitArgs,
	.configure_pid = hevcsplit_configure_pid,
	.process = hevcsplit_process,
};

const GF_FilterRegister *hevcsplit_register(GF_FilterSession *session)
{
	return &HEVCSplitRegister;
}
