/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *					 Yacine Mathurin Boubacar Aziakou
 *					 Samir Mustapha
 *			Copyright (c) Telecom ParisTech 2019
 *					All rights reserved
 *
 *  This file is part of GPAC / HEVC tile merger filter
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
#include <math.h>

typedef struct
{
	GF_FilterPid *pid;
	u32 slice_segment_address, width, height;
	Bool in_error;

	u32 nalu_size_length;
	u32 dsi_crc;
	HEVCState hevc_state;

	//final position in grid - the row index is only used to push non multiple of CU height at the bottom of the grid
	u32 pos_row, pos_col;

	//timescale of source pid
	//number of packets processed
	u32 timescale;
	u32 nb_pck;

	//true if positionning in pixel is given
	Bool has_pos;
	// >=0: positionning in pixel in the Y plane as given by CropOrigin
	// <=0: positionning relative to top-left tile
	s32 pos_x, pos_y;
} HEVCTilePidCtx;


typedef struct {
	//width of column
	u32 width;
	//cumulated height of all slices in column
	u32 height;
	//only used while computing the grid, current position in rows in the column
	u32 row_pos, max_row_pos;
	u32 last_row_idx;
	u32 pos_x;
} HEVCGridInfo;

typedef struct
{
	//options
	Bool strict;

	GF_FilterPid *opid;
	s32 base_pps_init_qp_delta_minus26;
	u32 nb_bits_per_adress_dst;
	u32 out_width, out_height;
	u8 *buffer_nal, *buffer_nal_no_epb, *buffer_nal_in_no_epb;
	u32 buffer_nal_alloc, buffer_nal_no_epb_alloc, buffer_nal_in_no_epb_alloc;
	GF_BitStream *bs_au_in;

	GF_BitStream *bs_nal_in;
	GF_BitStream *bs_nal_out;

 	HEVCGridInfo *grid;
 	u32 nb_cols;

	u8 *sei_suffix_buf;
	u32 sei_suffix_len, sei_suffix_alloc;
	u32 hevc_nalu_size_length;
	u32 max_CU_width, max_CU_height;

	GF_List *pids, *ordered_pids;
	Bool in_error;
} GF_HEVCMergeCtx;

//in src/filters/hevcsplit.c
void hevc_rewrite_sps(char *in_SPS, u32 in_SPS_length, u32 width, u32 height, char **out_SPS, u32 *out_SPS_length);

#if 0 //todo
//rewrite the profile and level
static void write_profile_tier_level(GF_BitStream *ctx->bs_nal_in, GF_BitStream *ctx->bs_nal_out, Bool ProfilePresentFlag, u8 MaxNumSubLayersMinus1)
{
	u8 j;
	Bool sub_layer_profile_present_flag[8], sub_layer_level_present_flag[8];
	if (ProfilePresentFlag) {
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 8), 8);
		gf_bs_write_long_int(ctx->bs_nal_out, gf_bs_read_long_int(ctx->bs_nal_in, 32), 32);
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 4), 4);
		gf_bs_write_long_int(ctx->bs_nal_out, gf_bs_read_long_int(ctx->bs_nal_in, 44), 44);
	}
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 8), 8);

	for (j = 0; j < MaxNumSubLayersMinus1; j++) {
		sub_layer_profile_present_flag[j] = gf_bs_read_int(ctx->bs_nal_in, 1);
		gf_bs_write_int(ctx->bs_nal_out, sub_layer_profile_present_flag[j], 1);
		sub_layer_level_present_flag[j] = gf_bs_read_int(ctx->bs_nal_in, 1);
		gf_bs_write_int(ctx->bs_nal_out, sub_layer_level_present_flag[j], 1);
	}
	if (MaxNumSubLayersMinus1 > 0)
		for (j = MaxNumSubLayersMinus1; j < 8; j++)
			gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 2), 2);


	for (j = 0; j < MaxNumSubLayersMinus1; j++) {
		if (sub_layer_profile_present_flag[j]) {
			gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 8), 8);
			gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 32), 32);
			gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 4), 4);
			gf_bs_write_long_int(ctx->bs_nal_out, gf_bs_read_long_int(ctx->bs_nal_in, 44), 44);
		}
		if (sub_layer_level_present_flag[j])
			gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 8), 8);
	}
}
#endif

static void hevcmerge_rewrite_pps(GF_HEVCMergeCtx *ctx, char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length)
{
	u8 cu_qp_delta_enabled_flag;
	u32 loop_filter_flag;
	u32 pps_size_no_epb;

	gf_bs_reassign_buffer(ctx->bs_nal_in, in_PPS, in_PPS_length);
	gf_bs_enable_emulation_byte_removal(ctx->bs_nal_in, GF_TRUE);
	if (!ctx->bs_nal_out) ctx->bs_nal_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_nal_out, ctx->buffer_nal_no_epb, ctx->buffer_nal_no_epb_alloc);

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


	gf_bs_read_int(ctx->bs_nal_in, 1);
	gf_bs_write_int(ctx->bs_nal_out, 1, 1);
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1);//entropy_coding_sync_enabled_flag
	gf_bs_set_ue(ctx->bs_nal_out, ctx->nb_cols-1);//write num_tile_columns_minus1
	gf_bs_set_ue(ctx->bs_nal_out, 0);//num_tile_rows_minus1
	gf_bs_write_int(ctx->bs_nal_out, 0, 1);  //uniform_spacing_flag

//	if (!uniform_spacing_flag) //always 0
	{
		u32 i;
		for (i = 0; i < ctx->nb_cols-1; i++)
			gf_bs_set_ue(ctx->bs_nal_out, (ctx->grid[i].width / ctx->max_CU_width - 1));

#if 0	//kept for clarity, but not used by the filter since we only use a single row
		for (i = 0; i < 1-1; i++) //num_tile_rows_minus1
			gf_bs_set_ue(ctx->bs_nal_out, (ctx->out_height / ctx->max_CU_height - 1)); // row_height_minus1[i]
#endif

	}
	loop_filter_flag = 1;
	gf_bs_write_int(ctx->bs_nal_out, loop_filter_flag, 1);

	loop_filter_flag = gf_bs_read_int(ctx->bs_nal_in, 1);
	gf_bs_write_int(ctx->bs_nal_out, loop_filter_flag, 1);

	// copy and write the rest of the bits in the byte
	while (gf_bs_get_bit_position(ctx->bs_nal_in) != 8) {
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1);
	}

	//copy and write the rest of the bytes
	while (gf_bs_get_size(ctx->bs_nal_in) != gf_bs_get_position(ctx->bs_nal_in)) {
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_u8(ctx->bs_nal_in), 8); //watchout, not aligned in destination bitstream
	}

	gf_bs_align(ctx->bs_nal_out);

	gf_bs_get_content_no_truncate(ctx->bs_nal_out, &ctx->buffer_nal_no_epb, &pps_size_no_epb, &ctx->buffer_nal_no_epb_alloc);

	*out_PPS_length = pps_size_no_epb + gf_media_nalu_emulation_bytes_add_count(ctx->buffer_nal_no_epb, pps_size_no_epb);
	*out_PPS = gf_malloc(*out_PPS_length);
	gf_media_nalu_add_emulation_bytes(ctx->buffer_nal_no_epb, *out_PPS, pps_size_no_epb);
}

u32 hevcmerge_rewrite_slice(GF_HEVCMergeCtx *ctx, HEVCTilePidCtx *tile_pid, char *in_slice, u32 in_slice_length)
{
	u64 header_end;
	u32 out_slice_size_no_epb = 0, out_slice_length;
	u32 num_entry_point_start;
	u32 pps_id;
	Bool RapPicFlag = GF_FALSE;
	u32 slice_qp_delta_start;
	HEVC_PPS *pps;
	HEVC_SPS *sps;
	u32 al, slice_size, in_slice_size_no_epb, slice_offset_orig, slice_offset_dst;
	u32 first_slice_segment_in_pic_flag;
	u32 dependent_slice_segment_flag;
	u8 nal_unit_type;
	s32 new_slice_qp_delta;

	HEVCState *hevc = &tile_pid->hevc_state;

	//we remove EPB directly rather than from bs reader, since we will have to copy the entire payload without EPB
	//and gf_bs_read_data does not check for EPB
	if (ctx->buffer_nal_in_no_epb_alloc<in_slice_length) {
		ctx->buffer_nal_in_no_epb_alloc = in_slice_length;
		ctx->buffer_nal_in_no_epb = gf_realloc(ctx->buffer_nal_in_no_epb, in_slice_length);
	}
	in_slice_size_no_epb = gf_media_nalu_remove_emulation_bytes(in_slice, ctx->buffer_nal_in_no_epb, in_slice_length);
	gf_bs_reassign_buffer(ctx->bs_nal_in, ctx->buffer_nal_in_no_epb, in_slice_size_no_epb);
	//disable EPB removal
	gf_bs_enable_emulation_byte_removal(ctx->bs_nal_in, GF_FALSE);
	if (!ctx->bs_nal_out) ctx->bs_nal_out = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	else gf_bs_reassign_buffer(ctx->bs_nal_out, ctx->buffer_nal_no_epb, ctx->buffer_nal_no_epb_alloc);

	assert(hevc->s_info.header_size_bits >= 0);
	assert(hevc->s_info.entry_point_start_bits >= 0);
	header_end = (u64)hevc->s_info.header_size_bits;

	num_entry_point_start = (u32)hevc->s_info.entry_point_start_bits;
	slice_qp_delta_start = (u32)hevc->s_info.slice_qp_delta_start_bits;

	// nal_unit_header			 
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1);
	nal_unit_type = gf_bs_read_int(ctx->bs_nal_in, 6);
	gf_bs_write_int(ctx->bs_nal_out, nal_unit_type, 6);
	gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 9), 9);

	first_slice_segment_in_pic_flag = gf_bs_read_int(ctx->bs_nal_in, 1);    //first_slice_segment_in_pic_flag
	if (tile_pid->slice_segment_address == 0)
		gf_bs_write_int(ctx->bs_nal_out, 1, 1);
	else
		gf_bs_write_int(ctx->bs_nal_out, 0, 1);

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

	pps_id = gf_bs_get_ue(ctx->bs_nal_in);
	gf_bs_set_ue(ctx->bs_nal_out, pps_id);

	pps = &hevc->pps[pps_id];
	sps = &hevc->sps[pps->sps_id];

	dependent_slice_segment_flag = 0;
	if (!first_slice_segment_in_pic_flag && pps->dependent_slice_segments_enabled_flag) {
		dependent_slice_segment_flag = gf_bs_read_int(ctx->bs_nal_in, 1);
	} else {
		dependent_slice_segment_flag = GF_FALSE;
	}
	if (!first_slice_segment_in_pic_flag) {
		/*address_ori = */gf_bs_read_int(ctx->bs_nal_in, sps->bitsSliceSegmentAddress);
	}
	//else original slice segment address = 0

	if (tile_pid->slice_segment_address > 0) {
		if (pps->dependent_slice_segments_enabled_flag) {
			gf_bs_write_int(ctx->bs_nal_out, dependent_slice_segment_flag, 1);
		}
		gf_bs_write_int(ctx->bs_nal_out, tile_pid->slice_segment_address, ctx->nb_bits_per_adress_dst);
	}
	//else first slice in pic, no adress

	//copy over bits until start of slice_qp_delta
	while (slice_qp_delta_start != (gf_bs_get_position(ctx->bs_nal_in) - 1) * 8 + gf_bs_get_bit_position(ctx->bs_nal_in)) {
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1);
	}
	//compute new qp delta
	new_slice_qp_delta = hevc->s_info.pps->pic_init_qp_minus26 + hevc->s_info.slice_qp_delta - ctx->base_pps_init_qp_delta_minus26;
	gf_bs_set_se(ctx->bs_nal_out, new_slice_qp_delta);
	gf_bs_get_se(ctx->bs_nal_in);

	//copy over until num_entry_points
	while (num_entry_point_start != (gf_bs_get_position(ctx->bs_nal_in) - 1) * 8 + gf_bs_get_bit_position(ctx->bs_nal_in)) {
		gf_bs_write_int(ctx->bs_nal_out, gf_bs_read_int(ctx->bs_nal_in, 1), 1);
	}
	//write num_entry_points to 0 (always present since we use tiling)
	gf_bs_set_ue(ctx->bs_nal_out, 0);

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
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] source slice header not properly aligned\n"));
	}
	gf_bs_align(ctx->bs_nal_in);

	//write byte_alignment() is bit=1 + x bit=0
	gf_bs_write_int(ctx->bs_nal_out, 1, 1);
	gf_bs_align(ctx->bs_nal_out);	// align

	//get the final slice header
	gf_bs_get_content_no_truncate(ctx->bs_nal_out, &ctx->buffer_nal_no_epb, &out_slice_size_no_epb, &ctx->buffer_nal_no_epb_alloc);

 	//get the slice payload size (no EPB in it) and offset in payload
	slice_size = (u32) gf_bs_available(ctx->bs_nal_in);
	slice_offset_orig = (u32) gf_bs_get_position(ctx->bs_nal_in);
	//slice data offset in output slice
	slice_offset_dst = out_slice_size_no_epb;
	//update output slice size
	out_slice_size_no_epb += slice_size;

	//copy slice data
	if (ctx->buffer_nal_no_epb_alloc < out_slice_size_no_epb) {
		ctx->buffer_nal_no_epb_alloc = out_slice_size_no_epb;
		ctx->buffer_nal_no_epb = gf_realloc(ctx->buffer_nal_no_epb, out_slice_size_no_epb);
	}
	memcpy(ctx->buffer_nal_no_epb + slice_offset_dst, ctx->buffer_nal_in_no_epb + slice_offset_orig, sizeof(char) * slice_size);

	//insert epb
	out_slice_length = out_slice_size_no_epb + gf_media_nalu_emulation_bytes_add_count(ctx->buffer_nal_no_epb, out_slice_size_no_epb);
	if (ctx->buffer_nal_alloc < out_slice_length) {
		ctx->buffer_nal_alloc = out_slice_length;
		ctx->buffer_nal = gf_realloc(ctx->buffer_nal, out_slice_length);
	}
	gf_media_nalu_add_emulation_bytes(ctx->buffer_nal_no_epb, ctx->buffer_nal, out_slice_size_no_epb);
	return out_slice_length;
}

static GF_Err hevcmerge_rewrite_config(GF_HEVCMergeCtx *ctx, GF_FilterPid *opid, char *data, u32 size)
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
		GF_HEVCParamArray *ar = (GF_HEVCParamArray *) gf_list_get(hvcc->param_array, i); // ar contains the i-th item in param_array
		for (j = 0; j < gf_list_count(ar->nalus); j++) { // for all the nalus the i-th param got
			/*! used for storing AVC sequenceParameterSetNALUnit and pictureParameterSetNALUnit*/
			GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j); // store j-th nalus in *sl

			if (ar->type == GF_HEVC_NALU_SEQ_PARAM) {
				char *outSPS=NULL;
				u32 outSize=0;
				hevc_rewrite_sps(sl->data, sl->size, ctx->out_width, ctx->out_height, &outSPS, &outSize);
				gf_free(sl->data);
				sl->data = outSPS;
				sl->size = outSize;
			}
			else if (ar->type == GF_HEVC_NALU_VID_PARAM) {
			}
			else if (ar->type == GF_HEVC_NALU_PIC_PARAM) {
				char *outPPS=NULL;
				u32 outSize=0;
				hevcmerge_rewrite_pps(ctx, sl->data, sl->size, &outPPS, &outSize);
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

static void hevcmerge_write_nal(GF_HEVCMergeCtx *ctx, char *output_nal, char *rewritten_nal, u32 out_nal_size)
{
	u32 n = 8*(ctx->hevc_nalu_size_length);
	while (n) {
		u32 v = (out_nal_size >> (n-8)) & 0xFF;
		*output_nal = v;
		output_nal++;
		n-=8;
	}
	memcpy(output_nal, rewritten_nal, out_nal_size);
}

static u32 hevcmerge_compute_address(GF_HEVCMergeCtx *ctx, HEVCTilePidCtx *tile_pid, Bool use_y_coord)
{
	u32 i, nb_pids, sum_height = 0, sum_width = 0;
	for (i=0; i<tile_pid->pos_col; i++) {
		sum_width += ctx->grid[i].width / ctx->max_CU_width;
	}

	if (use_y_coord) {
		sum_height = tile_pid->pos_y / ctx->max_CU_height;
 	} else {
		nb_pids = gf_list_count(ctx->pids);
		for (i=0; i<nb_pids; i++) {
			HEVCTilePidCtx *actx = gf_list_get(ctx->pids, i);
			if (actx->pos_col == tile_pid->pos_col) {
				if (actx->pos_row<tile_pid->pos_row) {
					sum_height += actx->height / ctx->max_CU_height;
				}
			}
		}
	}
	return sum_height * (ctx->out_width / ctx->max_CU_width) + sum_width;
}

static GF_Err hevcmerge_rebuild_grid(GF_HEVCMergeCtx *ctx)
{
	u32 nb_cols=0;
	u32 nb_rows=0;
	Bool reorder_rows=GF_FALSE;
	u32 force_last_col_plus_one=0;
	u32 nb_has_pos=0, nb_no_pos=0, nb_rel_pos=0, nb_abs_pos=0;
	u32 min_rel_pos_x = (u32) -1;
	u32 min_rel_pos_y = (u32) -1;
	u32 i, j, max_cols, nb_pids = gf_list_count(ctx->pids);

	for (i=0; i<nb_pids; i++) {
		HEVCTilePidCtx *apidctx = gf_list_get(ctx->pids, i);
		if (apidctx->width % ctx->max_CU_width) nb_cols++;
		if (apidctx->height % ctx->max_CU_height) nb_rows++;
		if (apidctx->has_pos) {
			nb_has_pos++;
			if ((apidctx->pos_x<0) || (apidctx->pos_y<0)) {
				nb_rel_pos++;
			}
			else if ((apidctx->pos_x>0) || (apidctx->pos_y>0)) nb_abs_pos++;

			if ((apidctx->pos_x<=0) && ((u32) (-apidctx->pos_x) < min_rel_pos_x))
				min_rel_pos_x = -apidctx->pos_x;
			if ((apidctx->pos_y<=0) && ((u32) (-apidctx->pos_y) < min_rel_pos_y))
				min_rel_pos_y = -apidctx->pos_y;
		} else {
			nb_no_pos++;
		}
	}
	if ((nb_cols>1) && (nb_rows>1)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] Cannot merge more than one tile not a multiple of CTUs in both width and height, not possible in standard\n"));
		return GF_BAD_PARAM;
	}
	if (nb_has_pos && nb_no_pos) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] Cannot merge tiles with explicit positionning and tiles with implicit positioning, not supported\n"));
		return GF_BAD_PARAM;
	}
	if (nb_rel_pos && nb_abs_pos) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] Cannot merge tiles with both relative explicit positionning and absolute explicit positioning, not supported\n"));
		return GF_BAD_PARAM;
	}
	if (ctx->grid) gf_free(ctx->grid);
	ctx->grid = NULL;

	if (min_rel_pos_x == (u32)-1) min_rel_pos_x=0;
	if (min_rel_pos_y == (u32)-1) min_rel_pos_y=0;

	max_cols = nb_pids;
	if (nb_has_pos) {
		max_cols = 0;
		for (i=0; i<nb_pids; i++) {
			HEVCTilePidCtx *tile1 = gf_list_get(ctx->pids, i);
			if (!tile1->has_pos) continue;

			for (j=0; j<nb_pids; j++) {
				Bool overlap = GF_FALSE;
				HEVCTilePidCtx *tile2 = gf_list_get(ctx->pids, j);
				if (tile2 == tile1) continue;
				if (!tile2->has_pos) continue;

				//for relative positioning, only check we don't have the same indexes
				if (nb_rel_pos) {
					if ((tile1->pos_x==tile2->pos_x) && (tile1->pos_y == tile2->pos_y)) {
						overlap = GF_TRUE;
					}
					continue;
				}

				//make sure we are aligned
				if ((tile1->pos_x<tile2->pos_x) && (tile1->pos_x + (s32) tile1->width > tile2->pos_x)) {
					overlap = GF_TRUE;
				}
				else if ((tile1->pos_x==tile2->pos_x) && (tile1->width != tile2->width)) {
					overlap = GF_TRUE;
				}
				if (tile1->pos_x==tile2->pos_x) {
					if ((tile1->pos_y<tile2->pos_y) && (tile1->pos_y + (s32) tile1->height > tile2->pos_y)) {
						overlap = GF_TRUE;
					}
					else if ((tile1->pos_y==tile2->pos_y) && (tile1->height != tile2->height)) {
						overlap = GF_TRUE;
					}
				}

				if (overlap) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] Overlapping tiles detected, cannot merge\n"));
					return GF_BAD_PARAM;
				}
			}

			if (!max_cols) {
				ctx->grid = gf_malloc(sizeof(HEVCGridInfo));
				memset(&ctx->grid[0], 0, sizeof(HEVCGridInfo));

				ctx->grid[0].width = tile1->width;
				if (nb_rel_pos) {
					ctx->grid[0].pos_x = -tile1->pos_x - min_rel_pos_x;
				} else {
					ctx->grid[0].pos_x = tile1->pos_x;
				}
				max_cols=1;
			} else if (ctx->grid[max_cols-1].pos_x != tile1->pos_x) {
				if (nb_rel_pos) {
					//append
					if (ctx->grid[max_cols-1].pos_x <(u32) -tile1->pos_x) {
						ctx->grid = gf_realloc(ctx->grid, sizeof(HEVCGridInfo) * (max_cols+1) );
						memset(&ctx->grid[max_cols], 0, sizeof(HEVCGridInfo));
						ctx->grid[max_cols].width = tile1->width;
						ctx->grid[max_cols].pos_x = -tile1->pos_x;
						max_cols+=1;
					}
					//insert
					else {
						Bool found=GF_FALSE;
						for (j=0; j<max_cols; j++) {
							if (ctx->grid[j].pos_x == -tile1->pos_x) {
								found=GF_TRUE;
								break;
							}
						}
						if (!found) {
							for (j=0; j<max_cols; j++) {
								if (ctx->grid[j].pos_x > (u32) -tile1->pos_x) {
									ctx->grid = gf_realloc(ctx->grid, sizeof(HEVCGridInfo) * (max_cols+1) );
									memmove(&ctx->grid[j+1], &ctx->grid[j], sizeof(HEVCGridInfo) * (max_cols-j));
									memset(&ctx->grid[j], 0, sizeof(HEVCGridInfo));
									ctx->grid[j].width = tile1->width;
									ctx->grid[j].pos_x = -tile1->pos_x;
									max_cols+=1;
									break;
								}
							}
						}
					}
				} else {
					//append
					if (ctx->grid[max_cols-1].pos_x + ctx->grid[max_cols-1].width <= (u32) tile1->pos_x) {
						ctx->grid = gf_realloc(ctx->grid, sizeof(HEVCGridInfo) * (max_cols+1) );
						memset(&ctx->grid[max_cols], 0, sizeof(HEVCGridInfo));
						ctx->grid[max_cols].width = tile1->width;
						ctx->grid[max_cols].pos_x = tile1->pos_x;
						max_cols+=1;
					}
					//insert
					else {
						for (j=0; j<max_cols; j++) {
							if (ctx->grid[j].pos_x > (u32) tile1->pos_x) {
								ctx->grid = gf_realloc(ctx->grid, sizeof(HEVCGridInfo) * (max_cols+1) );
								memmove(&ctx->grid[j+1], &ctx->grid[j], sizeof(HEVCGridInfo) * (max_cols-j));
								memset(&ctx->grid[j], 0, sizeof(HEVCGridInfo));
								ctx->grid[j].width = tile1->width;
								ctx->grid[j].pos_x = tile1->pos_x;
								max_cols+=1;
								break;
							}
						}
					}
				}
			}
		}
		// pass on the grid to insert empty columns
		if (!nb_rel_pos) {
			for (j=0; j<max_cols-1; j++) {
				assert(ctx->grid[j].pos_x + ctx->grid[j].width <= ctx->grid[j+1].pos_x);
				if (ctx->grid[j].pos_x + ctx->grid[j].width != ctx->grid[j+1].pos_x) {
					u32 new_width = ctx->grid[j+1].pos_x - ctx->grid[j].pos_x - ctx->grid[j].width;
					u32 new_x = ctx->grid[j].pos_x + ctx->grid[j].width;

					ctx->grid = gf_realloc(ctx->grid, sizeof(HEVCGridInfo) * (max_cols+1) );
					memmove(&ctx->grid[j+2], &ctx->grid[j+1], sizeof(HEVCGridInfo) * (max_cols-j-1));

					memset(&ctx->grid[j+1], 0, sizeof(HEVCGridInfo));
					ctx->grid[j+1].width = new_width;
					ctx->grid[j+1].pos_x = new_x;
					max_cols+=1;
					j++;
				}
			}
		}

		//assign cols and rows
		for (i=0; i<nb_pids; i++) {
			HEVCTilePidCtx *tile = gf_list_get(ctx->pids, i);
			tile->pos_col=0;
			tile->pos_row=0;
		}

		for (j=0; j<max_cols; j++) {
			if (nb_rel_pos) {
				ctx->grid[j].height = 0;
			}
			//check non-last colums are multiple of max CU width
			if ((j+1<max_cols) && (ctx->grid[j].width % ctx->max_CU_height) ) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] Invalid grid specification, column %d width %d not a multiple of max CU width and not the last one\n", j+1, ctx->grid[j].width));
				return GF_BAD_PARAM;
			}
			for (i=0; i<nb_pids; i++) {
				HEVCTilePidCtx *tile = gf_list_get(ctx->pids, i);
				if (nb_rel_pos) {
					if (-tile->pos_x != ctx->grid[j].pos_x) continue;
					if (ctx->grid[j].width != tile->width) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] Invalid relative positioning in the same column of tiles with different width %d vs %d\n", tile->width, ctx->grid[j].width));
						return GF_BAD_PARAM;
					}
					tile->pos_col = j;
					tile->pos_row = -tile->pos_y;
					ctx->grid[j].height += tile->height;
				} else {
					if (tile->pos_x != ctx->grid[j].pos_x) continue;
					tile->pos_col = j;
					tile->pos_row = tile->pos_y / ctx->max_CU_height;
					if (tile->pos_row * ctx->max_CU_height != tile->pos_y) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[HEVCMerge] Tile y %d not a multiple of CU height %d, adjusting to next boundary\n", tile->pos_y, ctx->max_CU_height));
						tile->pos_row++;
					}
					if (tile->pos_y + tile->height > ctx->grid[j].height)
						ctx->grid[j].height = tile->pos_y + tile->height;
				}

				if (tile->pos_row > ctx->grid[j].max_row_pos)
					ctx->grid[j].max_row_pos = tile->pos_row;
			}
		}
		//check non-last rows are multiple of max CU height
		for (j=0; j<max_cols; j++) {
			for (i=0; i<nb_pids; i++) {
				HEVCTilePidCtx *tile = gf_list_get(ctx->pids, i);
				if (tile->pos_col != j) continue;
				if ((tile->pos_row < ctx->grid[j].max_row_pos) && (tile->height % ctx->max_CU_height)) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] Invalid grid specification, row %d in column %d height %d not a multiple of max CU height and not the last one\n", tile->pos_row, j+1, tile->height));
					return GF_BAD_PARAM;
				}
			}
		}
		nb_cols = max_cols;

	} else {
		//gather tiles per columns
		ctx->grid = gf_malloc(sizeof(HEVCGridInfo)*nb_pids);
		memset(ctx->grid, 0, sizeof(HEVCGridInfo)*nb_pids);


		nb_cols=0;
		nb_rows=0;
		for (i=0; i<nb_pids; i++) {
			Bool found = GF_FALSE;
			HEVCTilePidCtx *apidctx = gf_list_get(ctx->pids, i);
			//do we fit on a col
			for (j=0; j<nb_cols; j++) {
				if (ctx->grid[j].width == apidctx->width) {
					found = GF_TRUE;
					break;
				}
			}
			//force new column
			if ((apidctx->height % ctx->max_CU_height) && found && ctx->grid[j].last_row_idx)
				found = GF_FALSE;

			if (!found) {
				ctx->grid[nb_cols].width = apidctx->width;
				ctx->grid[nb_cols].height = apidctx->height;
				if (apidctx->width % ctx->max_CU_width) {
					assert(!force_last_col_plus_one);
					force_last_col_plus_one = nb_cols+1;
				}
				if (apidctx->height % ctx->max_CU_height) {
					ctx->grid[nb_cols].last_row_idx = ctx->grid[nb_cols].row_pos +1 ;
					reorder_rows=GF_TRUE;
				}

				ctx->grid[nb_cols].row_pos = 1;
				nb_cols++;
				j=nb_cols-1;
			} else {
				ctx->grid[j].height += apidctx->height;
				if (apidctx->height % ctx->max_CU_height) {
					ctx->grid[j].last_row_idx = ctx->grid[j].row_pos+1;
					reorder_rows=GF_TRUE;
				}
				ctx->grid[j].row_pos ++;
			}
			assert(ctx->grid[j].row_pos);
			apidctx->pos_row = ctx->grid[j].row_pos - 1;
			apidctx->pos_col = j;
		}
		//move last column at end if not a multiple of CTU
		if (force_last_col_plus_one) {
			for (i=0; i<nb_pids; i++) {
				HEVCTilePidCtx *apidctx = gf_list_get(ctx->pids, i);
				//do we fit on a col
				if (apidctx->pos_col == force_last_col_plus_one - 1) {
					apidctx->pos_col = nb_cols-1;
				} else if (apidctx->pos_col > force_last_col_plus_one - 1) {
					apidctx->pos_col -= 1;
				}
			}
		}
		//in each column, push the last_row_idx if any to the bottom and move up other tiles accordingly
		//this ensures that slices with non multiple of maxCTU height are always at the bottom of the grid
		if (reorder_rows) {
			for (i=0; i<nb_cols; i++) {
				if (!ctx->grid[j].last_row_idx) continue;

				for (j=0; j<nb_pids; j++) {
					HEVCTilePidCtx *apidctx = gf_list_get(ctx->pids, j);

					if (apidctx->pos_col != i) continue;

					if (apidctx->pos_row==ctx->grid[i].last_row_idx-1) {
						apidctx->pos_row = ctx->grid[i].row_pos;
					} else if (apidctx->pos_row > ctx->grid[i].last_row_idx-1) {
						apidctx->pos_row -= 1;
					}
				}
			}
		}
	}

	//update final pos
	ctx->out_width = 0;
	ctx->out_height = 0;
	for (i=0; i<nb_cols; i++) {
		ctx->out_width += ctx->grid[i].width;
		if (ctx->grid[i].height > ctx->out_height)
			ctx->out_height = ctx->grid[i].height;
	}
	ctx->nb_cols = nb_cols;

	//recompute slice adresses
	GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[HEVCMerge] Grid reconfigured, output size %dx%d, %d input pids:\n", ctx->out_width, ctx->out_height, nb_pids));
	for (i=0; i<nb_pids; i++) {
		HEVCTilePidCtx *pidctx = gf_list_get(ctx->pids, i);
		pidctx->slice_segment_address = hevcmerge_compute_address(ctx, pidctx, nb_abs_pos ? GF_TRUE : GF_FALSE);

		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("- pid %s (pos %dx%d) size %dx%d new adress %d\n",
				gf_filter_pid_get_name(pidctx->pid),
				nb_has_pos ? pidctx->pos_x : pidctx->pos_col,
				nb_has_pos ? pidctx->pos_y : pidctx->pos_row,
				pidctx->width, pidctx->height, pidctx->slice_segment_address));
	}

	//sort pids according to columns (tiles) and slice adresses to be conformant with spec
	gf_list_reset(ctx->ordered_pids);
	for (i=0; i<nb_cols; i++) {
		for (j=0; j<nb_pids; j++) {
			u32 k;
			Bool inserted = GF_FALSE;
			HEVCTilePidCtx *tile = gf_list_get(ctx->pids, j);
			if (tile->pos_col != i) continue;
			for (k=0; k<gf_list_count(ctx->ordered_pids); k++) {
				HEVCTilePidCtx *tile2 = gf_list_get(ctx->ordered_pids, k);
				if (tile2->pos_col != i) continue;
				if (tile2->slice_segment_address > tile->slice_segment_address) {
					inserted = GF_TRUE;
					gf_list_insert(ctx->ordered_pids, tile, k);
					break;
				}
			}
			if (!inserted) {
				for (k=0; k<gf_list_count(ctx->ordered_pids); k++) {
					HEVCTilePidCtx *tile2 = gf_list_get(ctx->ordered_pids, k);
					if (tile2->pos_col > tile->pos_col) {
						inserted = GF_TRUE;
						gf_list_insert(ctx->ordered_pids, tile, k);
						break;
					}
				}
				if (!inserted)
					gf_list_add(ctx->ordered_pids, tile);
			}
		}
	}
	assert(gf_list_count(ctx->ordered_pids) == gf_list_count(ctx->pids));
	return GF_OK;
}

static GF_Err hevcmerge_check_sps_pps(GF_HEVCMergeCtx *ctx, HEVCTilePidCtx *pid_base, HEVCTilePidCtx *pid_o)
{
	Bool all_ok = GF_TRUE;
	u32 i;
	HEVCState *state_base = &pid_base->hevc_state;
	HEVCState *state_o = &pid_o->hevc_state;
	char *src_base = gf_filter_pid_get_source(pid_base->pid);
	char *src_o = gf_filter_pid_get_source(pid_o->pid);

	for (i=0; i<16; i++) {
		HEVC_SPS *sps_base = &state_base->sps[i];
		HEVC_SPS *sps_o = &state_o->sps[i];

#define CHECK_SPS_VAL(__name)	\
			if (sps_base->__name != sps_o->__name ) { \
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[HEVCMerge] "#__name" differs in SPS between %s and %s, undefined results\n", src_base, src_o));\
				all_ok = GF_FALSE;\
			}\

		if (!sps_base->state && !sps_o->state) continue;
		if (!sps_base->state && sps_o->state) {
			all_ok = GF_FALSE;
			break;
		}
		if (sps_base->state && !sps_o->state) {
			all_ok = GF_FALSE;
			break;
		}
		CHECK_SPS_VAL(aspect_ratio_info_present_flag)
		CHECK_SPS_VAL(chroma_format_idc)
		CHECK_SPS_VAL(cw_flag)
		CHECK_SPS_VAL(cw_flag)
		CHECK_SPS_VAL(cw_left)
		CHECK_SPS_VAL(cw_right)
		CHECK_SPS_VAL(cw_top)
		CHECK_SPS_VAL(cw_bottom)
		CHECK_SPS_VAL(bit_depth_luma)
		CHECK_SPS_VAL(bit_depth_chroma)
		CHECK_SPS_VAL(log2_max_pic_order_cnt_lsb)
		CHECK_SPS_VAL(separate_colour_plane_flag)
		CHECK_SPS_VAL(max_CU_depth)
		CHECK_SPS_VAL(num_short_term_ref_pic_sets)
		CHECK_SPS_VAL(num_long_term_ref_pic_sps)
		//HEVC_ReferencePictureSets rps[64];
		CHECK_SPS_VAL(aspect_ratio_info_present_flag)
		CHECK_SPS_VAL(long_term_ref_pics_present_flag)
		CHECK_SPS_VAL(temporal_mvp_enable_flag)
		CHECK_SPS_VAL(sample_adaptive_offset_enabled_flag)
		CHECK_SPS_VAL(sar_idc)
		CHECK_SPS_VAL(sar_width)
		CHECK_SPS_VAL(sar_height)
//		CHECK_SPS_VAL(has_timing_info)
//		CHECK_SPS_VAL(num_units_in_tick)
//		CHECK_SPS_VAL(time_scale)
//		CHECK_SPS_VAL(poc_proportional_to_timing_flag)
		CHECK_SPS_VAL(num_ticks_poc_diff_one_minus1)
		CHECK_SPS_VAL(video_full_range_flag)
		CHECK_SPS_VAL(colour_description_present_flag)
		CHECK_SPS_VAL(colour_primaries)
		CHECK_SPS_VAL(transfer_characteristic)
		CHECK_SPS_VAL(matrix_coeffs)
		CHECK_SPS_VAL(rep_format_idx);
		CHECK_SPS_VAL(sps_ext_or_max_sub_layers_minus1)
		CHECK_SPS_VAL(max_sub_layers_minus1)
		CHECK_SPS_VAL(update_rep_format_flag)
		CHECK_SPS_VAL(sub_layer_ordering_info_present_flag)
		CHECK_SPS_VAL(scaling_list_enable_flag)
		CHECK_SPS_VAL(infer_scaling_list_flag)
		CHECK_SPS_VAL(scaling_list_ref_layer_id)
		CHECK_SPS_VAL(scaling_list_data_present_flag)
		CHECK_SPS_VAL(asymmetric_motion_partitions_enabled_flag)
		CHECK_SPS_VAL(pcm_enabled_flag)
		CHECK_SPS_VAL(strong_intra_smoothing_enable_flag)
		CHECK_SPS_VAL(vui_parameters_present_flag)
		CHECK_SPS_VAL(log2_diff_max_min_luma_coding_block_size)
		CHECK_SPS_VAL(log2_min_transform_block_size)
		CHECK_SPS_VAL(log2_min_luma_coding_block_size)
		CHECK_SPS_VAL(log2_max_transform_block_size)
		CHECK_SPS_VAL(max_transform_hierarchy_depth_inter)
		CHECK_SPS_VAL(max_transform_hierarchy_depth_intra)
		CHECK_SPS_VAL(pcm_sample_bit_depth_luma_minus1)
		CHECK_SPS_VAL(pcm_sample_bit_depth_chroma_minus1)
		CHECK_SPS_VAL(pcm_loop_filter_disable_flag)
		CHECK_SPS_VAL(log2_min_pcm_luma_coding_block_size_minus3)
		CHECK_SPS_VAL(log2_diff_max_min_pcm_luma_coding_block_size)
		CHECK_SPS_VAL(overscan_info_present)
		CHECK_SPS_VAL(overscan_appropriate)
		CHECK_SPS_VAL(video_signal_type_present_flag)
		CHECK_SPS_VAL(video_format)
		CHECK_SPS_VAL(chroma_loc_info_present_flag)
		CHECK_SPS_VAL(chroma_sample_loc_type_top_field)
		CHECK_SPS_VAL(chroma_sample_loc_type_bottom_field)
		CHECK_SPS_VAL(neutra_chroma_indication_flag)
		CHECK_SPS_VAL(field_seq_flag)
		CHECK_SPS_VAL(frame_field_info_present_flag)
		CHECK_SPS_VAL(default_display_window_flag)
		CHECK_SPS_VAL(left_offset)
		CHECK_SPS_VAL(right_offset)
		CHECK_SPS_VAL(top_offset)
		CHECK_SPS_VAL(bottom_offset)
	}

	for (i=0; i<64; i++) {
		HEVC_PPS *pps_base = &state_base->pps[i];
		HEVC_PPS *pps_o = &state_o->pps[i];

#define CHECK_PPS_VAL(__name)	\
			if (pps_base->__name != pps_o->__name ) { \
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[HEVCMerge] "#__name" differs in PPS, undefined results\n"));\
				all_ok = GF_FALSE;\
			}\

		if (!pps_base->state && !pps_o->state) continue;
		if (!pps_base->state && pps_o->state) {
			all_ok = GF_FALSE;
			break;
		}
		if (pps_base->state && !pps_o->state) {
			all_ok = GF_FALSE;
			break;
		}
		//we don't check tile config nor init qp since we rewrite these
//		CHECK_PPS_VAL(loop_filter_across_tiles_enabled_flag)
//		CHECK_PPS_VAL(pic_init_qp_minus26)

		CHECK_PPS_VAL(dependent_slice_segments_enabled_flag)
		CHECK_PPS_VAL(num_extra_slice_header_bits)
		CHECK_PPS_VAL(num_ref_idx_l0_default_active)
		CHECK_PPS_VAL(num_ref_idx_l1_default_active)
		CHECK_PPS_VAL(slice_segment_header_extension_present_flag)
		CHECK_PPS_VAL(output_flag_present_flag)
		CHECK_PPS_VAL(lists_modification_present_flag)
		CHECK_PPS_VAL(cabac_init_present_flag)
		CHECK_PPS_VAL(weighted_pred_flag)
		CHECK_PPS_VAL(weighted_bipred_flag)
		CHECK_PPS_VAL(slice_chroma_qp_offsets_present_flag)
		CHECK_PPS_VAL(deblocking_filter_override_enabled_flag)
		CHECK_PPS_VAL(loop_filter_across_slices_enabled_flag)
		CHECK_PPS_VAL(entropy_coding_sync_enabled_flag)
		CHECK_PPS_VAL(sign_data_hiding_flag)
		CHECK_PPS_VAL(constrained_intra_pred_flag)
		CHECK_PPS_VAL(transform_skip_enabled_flag)
		CHECK_PPS_VAL(cu_qp_delta_enabled_flag)
		CHECK_PPS_VAL(transquant_bypass_enable_flag)
		CHECK_PPS_VAL(diff_cu_qp_delta_depth)
		CHECK_PPS_VAL(pic_cb_qp_offset)
		CHECK_PPS_VAL(pic_cr_qp_offset)
		CHECK_PPS_VAL(deblocking_filter_control_present_flag)
		CHECK_PPS_VAL(pic_disable_deblocking_filter_flag)
		CHECK_PPS_VAL(pic_scaling_list_data_present_flag)
		CHECK_PPS_VAL(beta_offset_div2)
		CHECK_PPS_VAL(tc_offset_div2)
		CHECK_PPS_VAL(log2_parallel_merge_level_minus2)
	}

	if (src_base) gf_free(src_base);
	if (src_o) gf_free(src_o);
	if (all_ok || !ctx->strict) return GF_OK;
	return GF_BAD_PARAM;
}

static GF_Err hevcmerge_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	Bool grid_config_changed = GF_FALSE;
	u32 cfg_crc = 0, pid_width, pid_height;
	const GF_PropertyValue *p, *dsi;
	GF_Err e;
	u8 j, i;
	GF_HEVCMergeCtx *ctx = (GF_HEVCMergeCtx*)gf_filter_get_udta(filter);
	HEVCTilePidCtx *tile_pid;

	pid_width = sizeof(HEVCState);
	pid_width = sizeof(HEVC_SPS);
	pid_width = sizeof(HEVC_PPS);

	if (ctx->in_error)
		return GF_BAD_PARAM;
	
	if (is_remove) {
		tile_pid = gf_filter_pid_get_udta(pid);
		gf_list_del_item(ctx->pids, tile_pid);
		gf_free(tile_pid);
		if (!gf_list_count(ctx->pids)) {
			if (ctx->opid)
				gf_filter_pid_remove(ctx->opid);

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
		GF_SAFEALLOC(tile_pid, HEVCTilePidCtx);
		gf_filter_pid_set_udta(pid, tile_pid);
		tile_pid->pid = pid;
		tile_pid->hevc_state.full_slice_header_parse = GF_TRUE;
		gf_list_add(ctx->pids, tile_pid);

		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	}
	assert(tile_pid);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) tile_pid->timescale = p->value.uint;

	//check if config (vps/sps/pps) has changed - if not, we ignore the reconfig
	//note that we don't copy all properties of input pids to the output in this case
	cfg_crc = 0;
	if (dsi && dsi->value.data.ptr && dsi->value.data.size) {
		cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
	}
	if (cfg_crc == tile_pid->dsi_crc) return GF_OK;
	tile_pid->dsi_crc = cfg_crc;

	//update this pid's config by parsing sps/vps/pps and check if we need to change anything
	GF_HEVCConfig *hvcc = NULL;
	hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_FALSE);
	if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
	tile_pid->nalu_size_length = hvcc->nal_unit_size;
	ctx->hevc_nalu_size_length = 4;
	e = GF_OK;
	for (i = 0; i < gf_list_count(hvcc->param_array); i++) { 
		GF_HEVCParamArray *ar = (GF_HEVCParamArray *) gf_list_get(hvcc->param_array, i);
		for (j = 0; j < gf_list_count(ar->nalus); j++) {
			GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j); 
			s32 idx = 0;

			if (ar->type == GF_HEVC_NALU_SEQ_PARAM) {
				idx = gf_media_hevc_read_sps(sl->data, sl->size, &tile_pid->hevc_state);
				if (idx>=0) {
					if (!ctx->max_CU_width) {
						ctx->max_CU_width = tile_pid->hevc_state.sps[idx].max_CU_width;
					} else if (ctx->max_CU_width != tile_pid->hevc_state.sps[idx].max_CU_width) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] Cannot merge tiles not using the same max CU width (%d vs %d)\n", ctx->max_CU_width, tile_pid->hevc_state.sps[idx].max_CU_width));
						e = GF_BAD_PARAM;
						break;
					}
					if (!ctx->max_CU_height) {
						ctx->max_CU_height = tile_pid->hevc_state.sps[idx].max_CU_height;
					} else if (ctx->max_CU_height != tile_pid->hevc_state.sps[idx].max_CU_height) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[HEVCMerge] Cannot merge tiles not using the same max CU height (%d vs %d)\n", ctx->max_CU_height, tile_pid->hevc_state.sps[idx].max_CU_height));
						e = GF_BAD_PARAM;
						break;
					}
				}
			}
			else if (ar->type == GF_HEVC_NALU_VID_PARAM) {
				idx = gf_media_hevc_read_vps(sl->data, sl->size, &tile_pid->hevc_state);
			}
			else if (ar->type == GF_HEVC_NALU_PIC_PARAM) {
				idx = gf_media_hevc_read_pps(sl->data, sl->size, &tile_pid->hevc_state);
			}
			if (idx < 0) {
				// WARNING
				e = GF_NON_COMPLIANT_BITSTREAM;
				break;
			}
		}
		if (e) break;
	}
	gf_odf_hevc_cfg_del(hvcc);
	if (e) return e;
	if (!ctx->opid) {
		ctx->opid = gf_filter_pid_new(filter);
		gf_filter_pid_copy_properties(ctx->opid, pid);
	}

	if ((pid_width != tile_pid->width) || (pid_height != tile_pid->height)) {
		tile_pid->width = pid_width;
		tile_pid->height = pid_height;
		grid_config_changed = GF_TRUE;
	}
	//todo further testing we might want to force a grid reconfig even if same width / height

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CROP_POS);
	if (p) {
		s32 pos_x, pos_y;
		if (!tile_pid->has_pos)
			grid_config_changed = GF_TRUE;

		tile_pid->has_pos = GF_TRUE;
		if (p->value.vec2i.x>0) {
			pos_x = p->value.vec2i.x / ctx->max_CU_width;
			if (pos_x * ctx->max_CU_width != p->value.vec2i.x) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[HEVCMerge] CropOrigin X %d is not a multiple of max CU width %d, adjusting to next boundary\n", p->value.vec2i.x, ctx->max_CU_width));
				pos_x++;
			}
			pos_x *= ctx->max_CU_width;
		} else {
			pos_x = p->value.vec2i.x;
		}

		if (p->value.vec2i.y>0) {
			pos_y = p->value.vec2i.y / ctx->max_CU_height;
			if (pos_y * ctx->max_CU_height != p->value.vec2i.y) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[HEVCMerge] CropOrigin Y %d is not a multiple of max CU height %d, adjusting to next boundary\n", p->value.vec2i.y, ctx->max_CU_height));
				pos_y++;
			}
			pos_y *= ctx->max_CU_height;
		} else {
			pos_y = p->value.vec2i.y;
		}
		if ((pos_x != tile_pid->pos_x) || (pos_y != tile_pid->pos_y))
			grid_config_changed = GF_TRUE;
		tile_pid->pos_x = pos_x;
		tile_pid->pos_y = pos_y;
	} else {
		if (tile_pid->has_pos)
			grid_config_changed = GF_TRUE;
		tile_pid->has_pos = GF_FALSE;
		tile_pid->pos_x = tile_pid->pos_y = 0;
	}


reconfig_grid:
	// Update grid
	if (grid_config_changed) {
		e = hevcmerge_rebuild_grid(ctx);
		if (e) {
			ctx->in_error = GF_TRUE;
			return e;
		}
	}

	//check SPS/PPS are compatible - for now we only warn but still process
	tile_pid = gf_list_get(ctx->pids, 0);
	for (i=1; i<gf_list_count(ctx->pids); i++) {
		HEVCTilePidCtx *apidctx = gf_list_get(ctx->pids, i);
		e = hevcmerge_check_sps_pps(ctx, tile_pid, apidctx);
		if (e) {
			apidctx->in_error = GF_TRUE;
			return e;
		}
	}

	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->out_width));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->out_height));

	//recreate DSI based on first pid we have
	tile_pid = gf_list_get(ctx->pids, 0);
	ctx->base_pps_init_qp_delta_minus26 = tile_pid->hevc_state.pps->pic_init_qp_minus26;

	u32 nb_CTUs = ((ctx->out_width + ctx->max_CU_width - 1) / ctx->max_CU_width) * ((ctx->out_height + ctx->max_CU_height - 1) / ctx->max_CU_height);
	ctx->nb_bits_per_adress_dst = 0;
	while (nb_CTUs > (u32)(1 << ctx->nb_bits_per_adress_dst)) {
		ctx->nb_bits_per_adress_dst++;
	}

	dsi = gf_filter_pid_get_property(tile_pid->pid, GF_PROP_PID_DECODER_CONFIG);
	if (!dsi) return GF_OK;

	return hevcmerge_rewrite_config(ctx, ctx->opid, dsi->value.data.ptr, dsi->value.data.size);
}

static GF_Err hevcmerge_process(GF_Filter *filter)
{
	char *data;
	u32 pos, nal_length, data_size, i;
	s32 current_poc;
	u8 temporal_id, layer_id, nal_unit_type;
	u32 nb_eos, nb_ipid;
	Bool found_sei_prefix=GF_FALSE, found_sei_suffix=GF_FALSE;
	u64 min_dts = GF_FILTER_NO_TS;
	u32 min_dts_timecale=0;
	GF_FilterPacket *output_pck = NULL;
	GF_HEVCMergeCtx *ctx = (GF_HEVCMergeCtx*) gf_filter_get_udta (filter);

	if (ctx->in_error)
		return GF_BAD_PARAM;
	nb_eos = 0;
	nb_ipid = gf_list_count(ctx->pids);

	//probe input for at least one packet on each pid
	for (i = 0; i < nb_ipid; i++) {
		u64 dts;
		GF_FilterPacket *pck_src;
		HEVCTilePidCtx *tile_pid = gf_list_get(ctx->pids, i);
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

		if (dts * min_dts_timecale < min_dts * tile_pid->timescale) {
			min_dts = dts;
			min_dts_timecale = tile_pid->timescale;
		}
	}
	if (nb_eos == nb_ipid) {
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}
	//reassemble based on the ordered list of pids
	for (i = 0; i < nb_ipid; i++) {
		u64 dts;
		GF_FilterPacket *pck_src;
		HEVCTilePidCtx *tile_pid = gf_list_get(ctx->ordered_pids, i);
		assert(tile_pid);
		if (tile_pid->in_error)
			continue;
		pck_src = gf_filter_pid_get_packet(tile_pid->pid);
		if (nb_eos) {
			if (pck_src) {
				tile_pid->nb_pck++;
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[HEVCMerge] pids of unequal duration, skipping packet %d on pid %d\n", tile_pid->nb_pck, i+1));
				gf_filter_pid_drop_packet(tile_pid->pid);
			}
			continue;
		}
		if (!pck_src) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[HEVCMerge] no data on pid %d while merging, eos detected %d\n", i+1, gf_filter_pid_is_eos(tile_pid->pid) ));
			continue;
		}

		dts = gf_filter_pck_get_dts(pck_src);
		if (dts * min_dts_timecale != min_dts * tile_pid->timescale) continue;
		data = (char *)gf_filter_pck_get_data(pck_src, &data_size); // data contains only a packet 
		// TODO: this is a clock signaling, for now just trash ..
		if (!data) {
			gf_filter_pid_drop_packet(tile_pid->pid);
			continue;
		}
		tile_pid->nb_pck++;

		//parse the access unit for this pid
		gf_bs_reassign_buffer(ctx->bs_au_in, data, data_size);

		while (gf_bs_available(ctx->bs_au_in)) {
			u8 *output_nal;
			u8 *nal_pck;
			u32 nal_pck_size;

			nal_length = gf_bs_read_int(ctx->bs_au_in, tile_pid->nalu_size_length * 8);
			pos = (u32) gf_bs_get_position(ctx->bs_au_in);
			gf_media_hevc_parse_nalu(data + pos, nal_length, &tile_pid->hevc_state, &nal_unit_type, &temporal_id, &layer_id);
			gf_bs_skip_bytes(ctx->bs_au_in, nal_length); //skip nal in bs

			//VCL nal, rewrite dlice header
			if (nal_unit_type < 32) {
				if (!i) current_poc = tile_pid->hevc_state.s_info.poc;
				else if (current_poc != tile_pid->hevc_state.s_info.poc) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[HEVCMerge] merging AU %u with different POC (%d vs %d), undefined results.\n", tile_pid->nb_pck, current_poc, tile_pid->hevc_state.s_info.poc));
				}

				nal_pck_size = hevcmerge_rewrite_slice(ctx, tile_pid, data + pos, nal_length);
				nal_pck = ctx->buffer_nal;
			}
			//NON-vcl, copy for SEI or drop (we should not have any SPS/PPS/VPS in the bitstream, they are in the decoder config prop)
			else {
				gf_media_hevc_parse_nalu(data + pos, nal_length, &tile_pid->hevc_state, &nal_unit_type, &temporal_id, &layer_id);
				// Copy SEI_PREFIX only for the first sample.
				if (nal_unit_type == GF_HEVC_NALU_SEI_PREFIX && !found_sei_prefix) {
					found_sei_prefix = GF_TRUE;
					nal_pck = data + pos;
					nal_pck_size = nal_length;
				}
				// Copy SEI_SUFFIX only as last nalu of the sample.
				else if (nal_unit_type == GF_HEVC_NALU_SEI_SUFFIX && ((i+1 == nb_ipid) || !found_sei_suffix) ) {
					found_sei_suffix = GF_TRUE;
					if (ctx->sei_suffix_alloc<nal_length) {
						ctx->sei_suffix_alloc = nal_length;
						ctx->sei_suffix_buf = gf_realloc(ctx->sei_suffix_buf, nal_length);
					}
					ctx->sei_suffix_len = nal_length;
					memcpy(ctx->sei_suffix_buf, data+pos, nal_length);
					continue;
				}
				else continue;
			}
			if (!output_pck) {
				output_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->hevc_nalu_size_length + nal_pck_size, &output_nal);
				// todo: might need to rewrite crypto info
				gf_filter_pck_merge_properties(pck_src, output_pck);
			}
			else {
				u8 *data_start;
				u32 new_size;
 				gf_filter_pck_expand(output_pck, ctx->hevc_nalu_size_length + nal_pck_size, &data_start, &output_nal, &new_size);
			}
			hevcmerge_write_nal(ctx, output_nal, nal_pck, nal_pck_size);
		}
		gf_filter_pid_drop_packet(tile_pid->pid);
	}
	// end of loop on inputs

	//if we had a SEI suffix, append it
	if (ctx->sei_suffix_len) {
		if (output_pck) {
			u8 *output_nal;
			u8 *data_start;
			u32 new_size;
			gf_filter_pck_expand(output_pck, ctx->hevc_nalu_size_length + ctx->sei_suffix_len, &data_start, &output_nal, &new_size);
			hevcmerge_write_nal(ctx, output_nal, ctx->sei_suffix_buf, ctx->sei_suffix_len);
		}
		ctx->sei_suffix_len = 0;
	}

	if (output_pck)
		gf_filter_pck_send(output_pck);

	return GF_OK;
}

static GF_Err hevcmerge_initialize(GF_Filter *filter)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[HEVCMerge] hevcmerge_initialize started.\n"));
	GF_HEVCMergeCtx *ctx = (GF_HEVCMergeCtx *)gf_filter_get_udta(filter);
	ctx->bs_au_in = gf_bs_new((char *)ctx, 1, GF_BITSTREAM_READ);
	ctx->bs_nal_in = gf_bs_new((char *)ctx, 1, GF_BITSTREAM_READ);

	ctx->pids = gf_list_new();
	ctx->ordered_pids = gf_list_new();
	return GF_OK;
}

static void hevcmerge_finalize(GF_Filter *filter)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[HEVCMerge] hevcmerge_finalize.\n"));
	GF_HEVCMergeCtx *ctx = (GF_HEVCMergeCtx *)gf_filter_get_udta(filter);
	if (ctx->buffer_nal) gf_free(ctx->buffer_nal);
	if (ctx->buffer_nal_no_epb) gf_free(ctx->buffer_nal_no_epb);
	if (ctx->buffer_nal_in_no_epb) gf_free(ctx->buffer_nal_in_no_epb);
	gf_bs_del(ctx->bs_au_in);
	gf_bs_del(ctx->bs_nal_in);
	if (ctx->bs_nal_out)
		gf_bs_del(ctx->bs_nal_out);

	if (ctx->grid) gf_free(ctx->grid);
	while (gf_list_count(ctx->pids)) {
		HEVCTilePidCtx *pctx = gf_list_pop_back(ctx->pids);
		gf_free(pctx);
	}
	gf_list_del(ctx->pids);
	gf_list_del(ctx->ordered_pids);

	if (ctx->sei_suffix_buf) gf_free(ctx->sei_suffix_buf);

//	// Mecanism to ensure the file creation for each execution of the program
//	int r = rename("LuT.txt", "LuT.txt");
//	if (r == 0) remove("LuT.txt");
//
//	FILE *LuT = NULL;
//	LuT = fopen("LuT.txt", "a");
//	for (int i = 0; i < ctx->nb_ipid; i++) {
//		HEVCTilePidCtx *tile_pid = ctx->addr_tile_pid[i];
//		fprintf(LuT,"%d %d %d %d\n", tile_pid->sum_width * 64, tile_pid->sum_height * 64, tile_pid->width, tile_pid->height);
//		gf_free(ctx->addr_tile_pid[i]);
//	}
//	fclose(LuT);
}

static const GF_FilterCapability HEVCMergeCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_INPUT,GF_PROP_PID_CODECID, GF_CODECID_HEVC),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_VISUAL),
	CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_HEVC)
};

#define OFFS(_n)	#_n, offsetof(GF_HEVCMergeCtx, _n)

static const GF_FilterArgs HEVCMergeArgs[] =
{
	{ OFFS(strict), "strict comparison of SPS and PPS of input pids - see filter help", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{0}
};

GF_FilterRegister HEVCMergeRegister = {
	.name = "hevcmerge",
	GF_FS_SET_DESCRIPTION("HEVC Tile merger")
	GF_FS_SET_HELP("This filter merges a set of HEVC PIDs into a single motion-constrained tiled HEVC PID.\n"
		"The filter creates a tiling grid with a single row and as many columns as needed.\n"
		"Positioning of tiles can be automatic (implicit) or explicit.\n"
		"The filter will check the SPS and PPS configurations of input PID and warn if they are not aligned but will still process them unless [-strict]() is set.\n"
		"The filter assumes that all input PIDs are synchronized (frames share the same timestamp) and will reassemble frames with the same dts. If pids are of unequal duration, the filter will drop frames as soon as one pid is over.\n"
		"## Implicit Positioning\n"
		"In implicit positioning, results may vary based on the order of input pids declaration.\n"
		"In this mode the filter will automatically allocate new columns for tiles with height not a multiple of max CU height.\n"
		"## Explicit Positioning\n"
		"In explicit positioning, the `CropOrigin` property on input PIDs is used to setup the tile grid. In this case, tiles shall not overlap in the final output.\n"
		"If `CropOrigin` is used, it shall be set on all input sources.\n"
		"If positive coordinates are used, they specify absolute positionning in pixels of the tiles. The coordinates are automatically adjusted to the next multiple of max CU width and height.\n"
		"If negative coordinates are used, they specify relative positioning (eg `0x-1` indicates to place the tile below the tile 0x0).\n"
		"In this mode, it is the caller responsability to set coordinates so that all tiles in a column have the same width and only the last row/column uses non-multiple of max CU width/height values. The filter will complain and abort if this is not respected.\n"
		"- If an horizontal blank is detected in the layout, an empty column in the tiling grid will be inserted.\n"
		"- If a vertical blank is detected in the layout, it is ignored.\n"
		)
	.private_size = sizeof(GF_HEVCMergeCtx),
	SETCAPS(HEVCMergeCaps),
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.initialize = hevcmerge_initialize,
	.finalize = hevcmerge_finalize,
	.args = HEVCMergeArgs,
	.configure_pid = hevcmerge_configure_pid,
	.process = hevcmerge_process,
	.max_extra_pids = -1,
};

const GF_FilterRegister *hevcmerge_register(GF_FilterSession *session)
{
	return &HEVCMergeRegister;
}
