/*
*
*
*
*/

#include <gpac/bitstream.h>
#include <gpac/filters.h>
#include <gpac/avparse.h>
#include <gpac/constants.h>
#include <gpac/internal/media_dev.h>
#include <math.h>

#define SEI_PREFIX 39
#define SEI_SUFFIX 40
typedef struct
{
	u32 width, height;
	
} HEVCTilePidProp;

typedef struct
{
	Bool address_computed;
	char *out_slice_header, nal_pck;
	u32 slice_segment_address, width, height, buffer_nal_alloc;
	HEVCTilePidProp tile_pid_prop[2];
	u32 nal_pck_alloc, sum_height, sum_width;
	u32 hevc_nalu_size_length;
	u32 dsi_crc;
	HEVCState hevc_state;
	u8 pid_idx, tile_pos_row, tile_pos_col;
} HEVCTilePidCtx;

typedef struct
{
	GF_FilterPid *opid;
	Bool add_column, found_sei_prefix, found_sei_suffix;
	u32 num_tiles;
	s32 base_pps_init_qp_delta_minus26;
	u32 out_width, out_height, grid_height;
	char *buffer_nal, data_without_emulation_bytes;
	char *dsi_ptr;
	u32 buffer_nal_alloc, num_CTU_width_tot, dsi_size;
	GF_BitStream *bs_nal_in;
	HEVCTilePidProp tile_prop[2];
	int *tile_width, *tile_height;
	u8 num_video_counter, num_video_per_height, nb_ipid;
	u8 *on_row, *on_col, hevc_nalu_size_length, pid_idx;
	HEVCTilePidCtx *addr_tile_pid[25];
} GF_HEVCSplitCtx;

u32 bs_get_ue(GF_BitStream *bs);

s32 bs_get_se(GF_BitStream *bs);

static void bs_set_ue(GF_BitStream *bs, u32 num) {
	s32 length = 1;
	s32 temp = ++num;

	while (temp != 1) {
		temp >>= 1;
		length += 2;
	}

	gf_bs_write_int(bs, 0, length >> 1);
	gf_bs_write_int(bs, num, (length + 1) >> 1);
}

static void bs_set_se(GF_BitStream *bs, s32 num)
{
	u32 v;
	if (num <= 0)
		v = (-1 * num) << 1;
	else
		v = (num << 1) - 1;

	bs_set_ue(bs, v);
}

//rewrite the profile and level
static void write_profile_tier_level(GF_BitStream *bs_in, GF_BitStream *bs_out, Bool ProfilePresentFlag, u8 MaxNumSubLayersMinus1)
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

static void rewrite_SPS(char *in_SPS, u32 in_SPS_length, u32 width, u32 height, char **out_SPS, u32 *out_SPS_length)
{
	GF_BitStream *bs_in, *bs_out;
	u64 length_no_use = 4096, dst_buffer_size;
	char *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0, sps_ext_or_max_sub_layers_minus1;
	u8 max_sub_layers_minus1 = 0, layer_id;
	Bool conformance_window_flag, multiLayerExtSpsFlag;
	u32 chroma_format_idc;


	bs_in = gf_bs_new(in_SPS, in_SPS_length, GF_BITSTREAM_READ);
	gf_bs_enable_emulation_byte_removal(bs_in, GF_TRUE);
	bs_out = gf_bs_new(NULL, length_no_use, GF_BITSTREAM_WRITE);
	dst_buffer_size = gf_bs_get_size(bs_out);
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

	*out_SPS_length = data_without_emulation_bytes_size + avc_emulation_bytes_add_count(data_without_emulation_bytes, data_without_emulation_bytes_size);
	*out_SPS = gf_malloc(*out_SPS_length);
	gf_media_nalu_add_emulation_bytes(data_without_emulation_bytes, *out_SPS, data_without_emulation_bytes_size);

exit:
	gf_bs_del(bs_in);
	gf_bs_del(bs_out);
	gf_free(data_without_emulation_bytes);
}

static void rewrite_PPS(Bool extract, char *in_PPS, u32 in_PPS_length, char **out_PPS, u32 *out_PPS_length, u8 num_tile_rows_minus1, u8 num_tile_columns_minus1, u32 uniform_spacing_flag, GF_HEVCSplitCtx *ctx)
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
			u8 i;
			for (i = 0; i < num_tile_columns_minus1; i++)
				bs_set_ue(bs_out, (ctx->tile_prop[i].width / 64 - 1)); // column_width_minus1[i], todo: not a ctu size
			i = 0;
			for (i = 0; i < num_tile_rows_minus1; i++)
				bs_set_ue(bs_out, (ctx->tile_prop[i].height / 64 - 1)); // row_height_minus1[i]
		}
		loop_filter_across_tiles_enabled_flag = 1; // loop_filter_across_tiles_enabled_flag
		gf_bs_write_int(bs_out, loop_filter_across_tiles_enabled_flag, 1);
	}

	loop_filter_across_slices_enabled_flag = gf_bs_read_int(bs_in, 1);
	gf_bs_write_int(bs_out, loop_filter_across_slices_enabled_flag, 1);

	// copy and write the rest of the bits in the byte
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

static char* rewrite_slice_address(GF_HEVCSplitCtx *ctx, HEVCTilePidCtx *tile_pid, u32 new_address, char *in_slice, u32 in_slice_length, char **out_slice, u32 *out_slice_length, u32 final_width, u32 final_height)
{
	char *data_without_emulation_bytes = NULL;
	u32 data_without_emulation_bytes_size = 0;
	u64 header_end, bs_ori_size;
	u32 dst_buf_size = 0;
	char *dst_buf = NULL;
	u32 num_entry_point_start;
	u32 pps_id;
	GF_BitStream *bs_ori, *bs_rw; // *pre_bs_ori;
	Bool IDRPicFlag = GF_FALSE;
	Bool RapPicFlag = GF_FALSE;
	u32 nb_bits_per_adress_dst = 0, slice_qp_delta_start;
	HEVC_PPS *pps;
	HEVC_SPS *sps;
	u64 al, slice_size, slice_offset_orig, slice_offset_dst;
	u32 first_slice_segment_in_pic_flag;
	u32 dependent_slice_segment_flag;
	int address_ori;
	u8 nal_unit_type;
	s32 new_slice_qp_delta;

	HEVCState *hevc = &tile_pid->hevc_state;


	//buff_crop_origin_x = gf_malloc(6 * sizeof(char));
	data_without_emulation_bytes = gf_malloc(in_slice_length * sizeof(char)); // In slice content shall be written as a character, sizeof(char) is the length on which each character is coded.
	/*data_without_emulation_bytes = gf_malloc(in_slice_length * sizeof(char));*/
	data_without_emulation_bytes_size = avc_remove_emulation_bytes(in_slice, data_without_emulation_bytes, in_slice_length);
	bs_ori = gf_bs_new(data_without_emulation_bytes, data_without_emulation_bytes_size, GF_BITSTREAM_READ);
	//bs_ori = gf_bs_new(in_slice, in_slice_length, GF_BITSTREAM_READ);
	//gf_bs_enable_emulation_byte_removal(bs_ori, GF_TRUE);
	bs_ori_size = gf_bs_get_refreshed_size(bs_ori);
	bs_rw = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	assert(hevc->s_info.header_size_bits >= 0);
	assert(hevc->s_info.entry_point_start_bits >= 0);
	header_end = (u64)hevc->s_info.header_size_bits;

	num_entry_point_start = (u32)hevc->s_info.entry_point_start_bits;
	slice_qp_delta_start = (u32)hevc->s_info.slice_qp_delta_start_bits;

	// nal_unit_header			 
	gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	nal_unit_type = gf_bs_read_int(bs_ori, 6);
	gf_bs_write_int(bs_rw, nal_unit_type, 6);
	gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 9), 9);

	first_slice_segment_in_pic_flag = gf_bs_read_int(bs_ori, 1);    //first_slice_segment_in_pic_flag
	if (new_address == 0)
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
	
	//copy over bits until start of slice_qp_delta
	while (slice_qp_delta_start != (gf_bs_get_position(bs_ori) - 1) * 8 + gf_bs_get_bit_position(bs_ori))
	{
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	}
	new_slice_qp_delta = hevc->pps->pic_init_qp_minus26 + hevc->s_info.slice_qp_delta - ctx->base_pps_init_qp_delta_minus26; // no restriction for first tlie cause the result is correct
	//printf("base_pps_init_qp_delta_minus26 = %i\tpic_init_qp_minus26 = %i\thevc->s_info.slice_qp_delta = %i\tnew_slice_qp_delta = %i\n", base_pps_init_qp_delta_minus26, hevc->pps->pic_init_qp_minus26, hevc->s_info.slice_qp_delta, new_slice_qp_delta);
	bs_set_se(bs_rw, new_slice_qp_delta);
	bs_get_se(bs_ori);
	
	//copy over until num_entry_points
	while (num_entry_point_start != (gf_bs_get_position(bs_ori) - 1) * 8 + gf_bs_get_bit_position(bs_ori)) //Copy till the end of the header
	{
		gf_bs_write_int(bs_rw, gf_bs_read_int(bs_ori, 1), 1);
	}
	//write num_entry_points to 0 (always present since we use tiling)
	bs_set_ue(bs_rw, 0);

	//write slice extension to 0
	if (pps->slice_segment_header_extension_present_flag)
		gf_bs_write_int(bs_rw, 0, 1);


	//we may have unparsed data in the source bitstream (slice header) due to entry points or slice segment extensions
	//TODO: we might want to copy over the slice extension header bits
	while (header_end != (gf_bs_get_position(bs_ori) - 1) * 8 + gf_bs_get_bit_position(bs_ori))
		gf_bs_read_int(bs_ori, 1);

	//read byte_alignment() is bit=1 + x bit=0
	al = gf_bs_read_int(bs_ori, 1);
	assert(al == 1);
	gf_bs_align(bs_ori);

	//write byte_alignment() is bit=1 + x bit=0
	gf_bs_write_int(bs_rw, 1, 1);
	gf_bs_align(bs_rw);	// align
#if 1
	gf_bs_get_content(bs_rw, &dst_buf, &dst_buf_size); /* bs_rw: header.*/
	slice_size = gf_bs_available(bs_ori); /* Slice_size: the rest of the slice after the header (no_emulation_bytes in it). */
	slice_offset_orig = gf_bs_get_position(bs_ori); /* Slice_offset_orig: Immediate next byte after header_end (start of the slice_payload) */
	slice_offset_dst = dst_buf_size;/* Slice_offset_dst: bs_rw_size (size of our new header). */
	dst_buf_size += slice_size;/* Size of our new header plus the payload itself.*/
	dst_buf = gf_realloc(dst_buf, sizeof(char) * dst_buf_size);/* A buffer for our new header plus the payload. */
	memcpy(dst_buf + slice_offset_dst, data_without_emulation_bytes + slice_offset_orig, sizeof(char) * slice_size);

	gf_free(data_without_emulation_bytes);
	data_without_emulation_bytes = dst_buf;
	data_without_emulation_bytes_size = dst_buf_size;
#elif 0
	gf_bs_get_content(bs_rw, &dst_buf, &dst_buf_size); /* bs_rw: header.*/
	
	gf_free(data_without_emulation_bytes);
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

	u32 slice_length = data_without_emulation_bytes_size + avc_emulation_bytes_add_count(data_without_emulation_bytes, data_without_emulation_bytes_size);
	if (*out_slice_length < slice_length)
		*out_slice = gf_realloc (*out_slice, slice_length);
	
	*out_slice_length = slice_length;
	gf_media_nalu_add_emulation_bytes(data_without_emulation_bytes, *out_slice, data_without_emulation_bytes_size);

	gf_bs_del(bs_ori);
	gf_bs_del(bs_rw);
	gf_free(data_without_emulation_bytes);
	return *out_slice;
}

static GF_Err rewrite_hevc_dsi(GF_HEVCSplitCtx *ctx, GF_FilterPid *opid, char *data, u32 size, u32 new_width, u32 new_height, u8 num_tile_rows_minus1, u8 num_tile_columns_minus1)
{
	u32 i, j;
	char *new_dsi;
	u32 new_size;
	Bool uniform_spacing_flag = GF_FALSE;
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
				char *outSPS;
				u32 outSize;
				rewrite_SPS(sl->data, sl->size, new_width, new_height, &outSPS, &outSize);
				gf_free(sl->data);
				sl->data = outSPS;
				sl->size = outSize;
			}
			else if (ar->type == GF_HEVC_NALU_VID_PARAM) {
			}
			else if (ar->type == GF_HEVC_NALU_PIC_PARAM) {
				char *outPPS;
				u32 outSize;
				rewrite_PPS(GF_FALSE, sl->data, sl->size, &outPPS, &outSize, num_tile_rows_minus1, num_tile_columns_minus1, uniform_spacing_flag, ctx);
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
#if 0
static GF_Err rewrite_hevc_dsi_pps(GF_HEVCSplitCtx *ctx, GF_FilterPid *opid, char *data, u32 size, u8 num_tile_rows_minus1, u8 num_tile_cols_minus1)
{
	u32 i, j;
	char *new_dsi;
	u32 new_size;
	Bool uniform_spacing_flag = GF_FALSE;
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
				
			}
			else if (ar->type == GF_HEVC_NALU_VID_PARAM) {
			}
			else if (ar->type == GF_HEVC_NALU_PIC_PARAM) {
				char *outPPS;
				u32 outSize;
				rewrite_PPS(GF_FALSE, sl->data, sl->size, &outPPS, &outSize, num_tile_rows_minus1, num_tile_cols_minus1, uniform_spacing_flag, ctx); // static config: 2 rows and a col (tbc)
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
#endif

static void bsnal(char *output_nal, char *rewritten_nal, u32 out_nal_size, u32 hevc_nalu_size_length)
{
	GF_BitStream *bsnal = gf_bs_new(output_nal, hevc_nalu_size_length, GF_BITSTREAM_WRITE);
	gf_bs_write_int(bsnal, out_nal_size, 8 * hevc_nalu_size_length); 
	memcpy(output_nal + hevc_nalu_size_length, rewritten_nal, out_nal_size);
	gf_bs_del(bsnal);
}

static u32 compute_address(GF_HEVCSplitCtx *ctx, HEVCTilePidCtx *tile_pid)
{
	u32 i, j, new_address = 0, sum_height = 0, sum_width = 0;
	HEVCTilePidCtx *ptr = NULL, *current_pid = ctx->addr_tile_pid[tile_pid->pid_idx];

#if 1
	if (ctx->pid_idx > 0)
	{
		HEVCTilePidCtx *previous_pid = ctx->addr_tile_pid[tile_pid->pid_idx - 1];
		//if (position_row[k] == position_row[k - 1] && position_col[k] == position_col[k - 1])
		if ((current_pid->tile_pos_row == previous_pid->tile_pos_row) && (current_pid->tile_pos_col == previous_pid->tile_pos_col))
		{
			//for (j = 0; j < *(ctx->on_col + ctx->pid_idx); j++) sum_width += *(ctx->tile_size_x + j) / 64; // num_CTU_width[j]
			for (j = 0; j < current_pid->tile_pos_col; j++) {
				ptr = ctx->addr_tile_pid[j];
				sum_width += ptr->width / 64; // num_CTU_width[j]
			}
			//while (*(ctx->on_row + ctx->pid_idx) == *(ctx->on_row + ctx->pid_idx - 1) && *(ctx->on_col + ctx->pid_idx) == *(ctx->on_col + ctx->pid_idx - 1))
			while (current_pid->tile_pos_row == previous_pid->tile_pos_row && current_pid->tile_pos_col == previous_pid->tile_pos_col)
			{
				ptr = ctx->addr_tile_pid[tile_pid->pid_idx - 1];
				sum_height += ptr->height / 64; // dim_ctu_height[k - 1]
				if (ctx->pid_idx == 1) break;// don't seek for position_col[-1]
				ctx->pid_idx--;
			}
			new_address = sum_height * (ctx->out_width / 64) + sum_width; // ctx->num_CTU_width_tot
		}
		else
		{
			//for (i = 0; i < *(ctx->on_row + ctx->pid_idx); i++) sum_height += *(ctx->tile_size_y + i) / 64; // num_CTU_height[i]
			for (i = 0; i < current_pid->tile_pos_row; i++) {
				ptr = ctx->addr_tile_pid[i];
				sum_height += ptr->height / 64; // num_CTU_height[i]
			}
			//for (j = 0; j < *(ctx->on_col + ctx->pid_idx); j++) sum_width += *(ctx->tile_size_x + j) / 64; // num_CTU_width[j]
			for (j = 0; j < current_pid->tile_pos_col; j++) {
				ptr = ctx->addr_tile_pid[i];
				sum_width += ptr->width / 64; // num_CTU_width[j]
			}
			new_address = sum_height * (ctx->out_width / 64) + sum_width;
		}
	}
	else
	{
		//visualise = *(ctx->tile_size_y);
		//visualise = ptr->tile_pos_row;
		
		for (i = 0; i < current_pid->tile_pos_row; i++) {
			ptr = ctx->addr_tile_pid[i];
			sum_height += ptr->height / 64; // num_CTU_height[i]
		}
		for (j = 0; j < current_pid->tile_pos_col; j++) {
			ptr = ctx->addr_tile_pid[i];
			sum_width += ptr->width / 64; // num_CTU_width[j]
		}
		new_address = sum_height * (ctx->out_width / 64) + sum_width; 
	}
#endif
	current_pid->sum_height = sum_height;
	current_pid->sum_width = sum_width;
	return new_address;
}

static GF_Err hevccombine_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	Bool grid_config_changed = GF_FALSE;
	u32 cfg_crc = 0, pid_width, pid_height;
	const GF_PropertyValue *p, *dsi;
	GF_Err e;
	u32 num_tile_rows_minus1 = 0, num_tile_columns_minus1 = 0;
	u8 j, i;
	
	// private stack of the filter returned as an object GF_HEVCSplitCtx.
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx*)gf_filter_get_udta(filter);
	HEVCTilePidCtx *tile_pid;
	
	if (is_remove) { // Todo: manage tile_pid removal 
		/*tile_pid = gf_filter_pid_get_udta(pid);
		if (tile_pid) gf_free(tile_pid);*/
		//todo: 
		//1: need to reconfigure the grid ?
		//2: if last pid , remove output pid
		return GF_OK;
	}
	// checks if input pid matchs its destination filter.
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
	// Good to go

	tile_pid = gf_filter_pid_get_udta(pid);
	if (!tile_pid) { // Is it forcing or tile_pid needs it ?
		GF_SAFEALLOC(tile_pid, HEVCTilePidCtx);
		gf_filter_pid_set_udta(pid, tile_pid);
		tile_pid->pid_idx = ctx->pid_idx;
		tile_pid->hevc_state.full_slice_header_parse = GF_TRUE;
		ctx->base_pps_init_qp_delta_minus26 = tile_pid->hevc_state.pps->pic_init_qp_minus26;
		ctx->addr_tile_pid[ctx->pid_idx] = tile_pid; // So we can free the address of tile_pid in finalize
	}
	assert(tile_pid);

	cfg_crc = 0;
	if (dsi && dsi->value.data.ptr && dsi->value.data.size) {
		// Takes buffer and buffer size to return an u32
		cfg_crc = gf_crc_32(dsi->value.data.ptr, dsi->value.data.size);
	}
		
	if (cfg_crc == tile_pid->dsi_crc) return GF_OK;
	tile_pid->dsi_crc = cfg_crc;
	// We have new_width and new_height for the combination of the streams.
	// check if we already saw the pid with the same width, height and decoder config. If so, do nothing (no rewrite_hevc_dsi()), else ...
	// parse otherwise they should refer to something else
	GF_HEVCConfig *hvcc = NULL;
	hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_FALSE);
	if (!hvcc) return GF_NON_COMPLIANT_BITSTREAM;
	tile_pid->hevc_nalu_size_length = hvcc->nal_unit_size;
	ctx->hevc_nalu_size_length = 4;
	e = GF_OK;
	for (i = 0; i < gf_list_count(hvcc->param_array); i++) { 
		GF_HEVCParamArray *ar = (GF_HEVCParamArray *) gf_list_get(hvcc->param_array, i);
		for (j = 0; j < gf_list_count(ar->nalus); j++) {
			GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(ar->nalus, j); 
			s32 idx = 0;

			if (ar->type == GF_HEVC_NALU_SEQ_PARAM) {
				idx = gf_media_hevc_read_sps(sl->data, sl->size, &tile_pid->hevc_state);
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
		ctx->tile_prop[ctx->pid_idx].width = tile_pid->width;
		ctx->tile_prop[ctx->pid_idx].height = tile_pid->height;
		grid_config_changed = GF_TRUE;
	}
	// Update grid
	if (grid_config_changed) {
		
		if(ctx->out_height + tile_pid->height <= 500){
			ctx->out_height += tile_pid->height;
			ctx->out_width = tile_pid->width;
			ctx->num_video_per_height++;
			num_tile_rows_minus1 = ctx->pid_idx; 
			num_tile_columns_minus1 = 0;
			ctx->add_column = GF_TRUE;
		}
		else {
				if(ctx->add_column) {
					num_tile_columns_minus1++;
					ctx->out_width += tile_pid->width;
					ctx->add_column = GF_FALSE;
				}
				num_tile_rows_minus1 = (ctx->pid_idx) % ctx->num_video_per_height;
				if (ctx->pid_idx >= ctx->num_video_per_height)
					num_tile_rows_minus1 = ctx->num_video_per_height - 1;
				ctx->grid_height += tile_pid->height;
				if (ctx->grid_height >= 2160) ctx->add_column = GF_TRUE;
		}
		tile_pid->tile_pos_row = num_tile_rows_minus1;
		tile_pid->tile_pos_col = num_tile_columns_minus1;
	}

		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_WIDTH, &PROP_UINT(ctx->out_width));
		gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_HEIGHT, &PROP_UINT(ctx->out_height));
		e = rewrite_hevc_dsi(ctx, ctx->opid, dsi->value.data.ptr, dsi->value.data.size, ctx->out_width, ctx->out_height, num_tile_rows_minus1, num_tile_columns_minus1);
		if (e) return e;

	ctx->pid_idx++;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);
	return GF_OK;
}

static GF_Err hevccombine_process(GF_Filter *filter)
{
	char *data;
	u32 pos, nal_length, data_size;
	u8 temporal_id, layer_id, nal_unit_type, i; 
	char *output_nal;
	u32 out_nal_size;
	u32 nb_eos, nb_ipid;
	u64 min_dts = GF_FILTER_NO_TS;
	GF_FilterPacket *output_pck = NULL;
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx*) gf_filter_get_udta (filter);
	ctx->pid_idx = 0;
#if 0
	if (did_it == 0) {
		//ctx->num_video_counter--;
		//ctx->num_video_per_height--;
		ctx->on_row = gf_malloc(ctx->num_video_counter * sizeof(int));
		ctx->on_col = gf_malloc(ctx->num_video_counter * sizeof(int));

		for (video_idx = 0; video_idx < ctx->num_video_counter; video_idx++) { // change the "39" to proper script/lettering
		//video_idx++;
			*(ctx->on_row + video_idx) = (video_idx + 1) % ctx->num_video_per_height; // position_row[] is atoi(argv[]) - 1
			if (*(ctx->on_row + video_idx) == 0) *(ctx->on_row + video_idx) = ctx->num_video_per_height;
			*(ctx->on_row + video_idx) -= 1;
			num_tile_rows_minus1 = *(ctx->on_row + video_idx);
			*(ctx->on_col + video_idx) = ceil((video_idx + 1) * pow(ctx->num_video_per_height, -1)) - 1; // position_col[] is atoi(argv[]) - 1
			num_tile_columns_minus1 = *(ctx->on_col + video_idx);
			did_it = 1;
		}
		rewrite_hevc_dsi_pps(ctx, ctx->opid, ctx->dsi_ptr, ctx->dsi_size, num_tile_rows_minus1, num_tile_columns_minus1); // write num_tile_minus1 ...
	}
#endif
	
	nb_eos = 0;
	nb_ipid = gf_filter_get_ipid_count(filter);
	
	for (i = 0; i < nb_ipid; i++) {
		u64 dts;
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		GF_FilterPacket *pck_src = gf_filter_pid_get_packet(ipid);
		if (!pck_src) {
			if (gf_filter_pid_is_eos(ipid)) {
				nb_eos++;
				continue;
			}
			return GF_OK;
		}
		dts = gf_filter_pck_get_dts(pck_src);
		if (dts < min_dts) min_dts = dts;
	}
	if (nb_eos == nb_ipid) {
		gf_filter_pid_set_eos(ctx->opid);
		return GF_EOS;
	}
	//get first packet in every input
	ctx->nb_ipid = nb_ipid;
	for (i = 0; i < nb_ipid; i++) {
		u64 dts;
		GF_FilterPid *ipid = gf_filter_get_ipid(filter, i);
		HEVCTilePidCtx *tile_pid = gf_filter_pid_get_udta(ipid); 
		GF_FilterPacket *pck_src = gf_filter_pid_get_packet(ipid);
		assert(pck_src);

		dts = gf_filter_pck_get_dts(pck_src);
		if (dts != min_dts) continue;
		data = (char *)gf_filter_pck_get_data(pck_src, &data_size); // data contains only a packet 
		// TODO: this is a clock signaling, for now just trash ..
		if (!data) {
			gf_filter_pid_drop_packet(ipid);
			continue;
		}
		/*create a bitstream reader from data
		 read from there.
		 data is the whole image cut out into "slice_in_pic"
		*/
		gf_bs_reassign_buffer(ctx->bs_nal_in, data, data_size);

		/* TODO: keep logging during the process */

		while (gf_bs_available(ctx->bs_nal_in))  
		{
			char *nal_pck;
			u32 nal_pck_size;

			nal_length = gf_bs_read_int(ctx->bs_nal_in, tile_pid->hevc_nalu_size_length * 8);
			pos = (u32) gf_bs_get_position(ctx->bs_nal_in);
			gf_media_hevc_parse_nalu(data + pos, nal_length, &tile_pid->hevc_state, &nal_unit_type, &temporal_id, &layer_id);
			gf_bs_skip_bytes(ctx->bs_nal_in, nal_length); //skip nal in bs
			//u32 v = ctx->num_video_counter, h = ctx->num_video_per_height; // useful for debug
#if 0
			if (nal_unit_type == 39) { // change the "39" to proper script/lettering
				video_idx++;
				*(ctx->on_row + video_idx) = (video_idx + 1) % ctx->num_video_per_height; // position_row[] is atoi(argv[]) - 1
				if (*(ctx->on_row + video_idx) == 0) *(ctx->on_row + video_idx) = ctx->num_video_per_height;
				*(ctx->on_row + video_idx) -= 1;
				*(ctx->on_col + video_idx) = ceil((video_idx + 1) * pow(ctx->num_video_per_height, -1)) - 1; // position_col[] is atoi(argv[]) - 1
			}
#endif
			// if (0 && nal_unit_type < 32) {
			if (nal_unit_type < 32) {
				if (!tile_pid->address_computed) { // if each tile comes with a different Id
					tile_pid->slice_segment_address = compute_address(ctx, tile_pid);
					tile_pid->address_computed = GF_TRUE;
					ctx->pid_idx++;
					out_nal_size = ctx->buffer_nal_alloc;
					rewrite_slice_address(ctx, tile_pid, tile_pid->slice_segment_address, data + pos, nal_length, &ctx->buffer_nal, &out_nal_size, ctx->out_width, ctx->out_height);
					nal_pck_size = out_nal_size;
					nal_pck = ctx->buffer_nal;
				}
				else {
					out_nal_size = ctx->buffer_nal_alloc;
					rewrite_slice_address(ctx, tile_pid, tile_pid->slice_segment_address, data + pos, nal_length, &ctx->buffer_nal, &out_nal_size, ctx->out_width, ctx->out_height);
					nal_pck_size = out_nal_size;
					nal_pck = ctx->buffer_nal;
				}

				if (out_nal_size > ctx->buffer_nal_alloc)
					ctx->buffer_nal_alloc = out_nal_size;
			}
			//else forward(pck_src, ctx->cur_pck); // Automatic gf_filter_pck_expand ?
			else {
				gf_media_hevc_parse_nalu(data + pos, nal_length, &tile_pid->hevc_state, &nal_unit_type, &temporal_id, &layer_id);
				// Copy SEI_PREFIX only for the first sample.
				if (nal_unit_type == SEI_PREFIX && !ctx->found_sei_prefix) {
					ctx->found_sei_prefix = GF_TRUE;
					nal_pck = data + pos;
					nal_pck_size = nal_length;
				}
				// Copy SEI_SUFFIX only as last nalu of the sample.
				else if (nal_unit_type == SEI_SUFFIX && i == nb_ipid - 1) {
					nal_pck = data + pos;
					nal_pck_size = nal_length;
				}
				else continue;
			}
			if (!output_pck) {
				output_pck = gf_filter_pck_new_alloc(ctx->opid, ctx->hevc_nalu_size_length + nal_pck_size, &output_nal);
				// todo: might need to rewrite crypto info
				gf_filter_pck_merge_properties(pck_src, output_pck);
			}
			else {
				char *data_start;
				u32 new_size;
 				gf_filter_pck_expand(output_pck, ctx->hevc_nalu_size_length + nal_pck_size, &data_start, &output_nal, &new_size);
			}
			bsnal(output_nal, nal_pck, nal_pck_size, ctx->hevc_nalu_size_length);
		}
		gf_filter_pid_drop_packet(ipid);
	}
	// end of loop on inputs
	if (output_pck) 
		gf_filter_pck_send(output_pck);

	return GF_OK;
}

static GF_Err hevccombine_initialize(GF_Filter *filter)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[Combine] hevccombine_initialize started.\n"));
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx *)gf_filter_get_udta(filter);
	ctx->bs_nal_in = gf_bs_new((char *)ctx, 1, GF_BITSTREAM_READ);
	#ifdef DYNAMIQUE_HEVC_STATE
	ctx->hevc_state = (HEVCState *)gf_malloc(sizeof(HEVCState));
#endif // DYNAMIQUE_HEVC_STATE
	return GF_OK;
}

static void hevccombine_finalize(GF_Filter *filter)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[Combine] hevccombine_finalize.\n"));
	GF_HEVCSplitCtx *ctx = (GF_HEVCSplitCtx *)gf_filter_get_udta(filter);
	HEVCTilePidCtx *tile_pid;
	if (ctx->buffer_nal) gf_free(ctx->buffer_nal);
	gf_bs_del(ctx->bs_nal_in);
	#ifdef DYNAMIQUE_HEVC_STATE
	#endif // DYNAMIQUE_HEVC_STATE

	// Mecanism to ensure the file creation for each execution of the program  
	int r = rename("LuT.txt", "LuT.txt");
	if (r == 0) remove("LuT.txt");

	FILE *LuT = NULL;
	LuT = fopen("LuT.txt", "a");
	for (int i = 0; i < ctx->nb_ipid; i++) {
		tile_pid = ctx->addr_tile_pid[i];
		fprintf(LuT,"%d %d %d %d\n", tile_pid->sum_width * 64, tile_pid->sum_height * 64, tile_pid->width, tile_pid->height);
		gf_free(ctx->addr_tile_pid[i]);
	}
	fclose(LuT);
}

static const GF_FilterCapability hevccombineCaps[] =
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

static const GF_FilterArgs hevccombineArgs[] =
{
	//	{ OFFS(threading), "Set threading mode", GF_PROP_UINT, "frame", "frameslice|frame|slice", GF_FS_ARG_HINT_ADVANCED},
		{0}
};

GF_FilterRegister HevccombineRegister = {
	.name = "hevccombine",
	GF_FS_SET_DESCRIPTION("Stream merger")
	.private_size = sizeof(GF_HEVCSplitCtx),
	SETCAPS(hevccombineCaps),
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.initialize = hevccombine_initialize,
	.finalize = hevccombine_finalize,
	.args = hevccombineArgs,
	.configure_pid = hevccombine_configure_pid,
	.process = hevccombine_process,
	.max_extra_pids = -1,
};

const GF_FilterRegister *hevccombine_register(GF_FilterSession *session)
{
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CODEC, ("[Combine] hevccombine_register started.\n"));
	return &HevccombineRegister;
}
