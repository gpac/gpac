/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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


#ifndef _GF_MEDIA_DEV_H_
#define _GF_MEDIA_DEV_H_

#include <gpac/media_tools.h>
#include <gpac/mpeg4_odf.h>

#ifndef GPAC_DISABLE_STREAMING
#include <gpac/ietf.h>
#endif

#ifndef GPAC_DISABLE_ISOM
void gf_media_get_sample_average_infos(GF_ISOFile *file, u32 Track, u32 *avgSize, u32 *MaxSize, u32 *TimeDelta, u32 *maxCTSDelta, u32 *const_duration, u32 *bandwidth);
#endif


#ifndef GPAC_DISABLE_MEDIA_IMPORT
GF_Err gf_import_message(GF_MediaImporter *import, GF_Err e, char *format, ...);
#endif /*GPAC_DISABLE_MEDIA_IMPORT*/


typedef struct
{
	Bool override;
	u16 colour_primaries, transfer_characteristics, matrix_coefficients;
	Bool full_range;
} COLR;

u32 gf_bs_read_ue(GF_BitStream *bs);
s32 gf_bs_read_se(GF_BitStream *bs);
void gf_bs_write_ue(GF_BitStream *bs, u32 num);
void gf_bs_write_se(GF_BitStream *bs, s32 num);


#ifndef GPAC_DISABLE_ISOM
GF_Err gf_media_get_color_info(GF_ISOFile *file, u32 track, u32 sampleDescriptionIndex, u32 *colour_type, u16 *colour_primaries, u16 *transfer_characteristics, u16 *matrix_coefficients, Bool *full_range_flag);
#endif

#ifndef GPAC_DISABLE_AV_PARSERS

u32 gf_latm_get_value(GF_BitStream *bs);

#define GF_SVC_SSPS_ID_SHIFT	16

/*return nb bytes from current data until the next start code and set the size of the next start code (3 or 4 bytes)
returns data_len if no startcode found and sets sc_size to 0 (last nal in payload)*/
u32 gf_media_nalu_next_start_code(const u8 *data, u32 data_len, u32 *sc_size);

u32 gf_media_nalu_emulation_bytes_remove_count(const u8 *buffer, u32 nal_size);
u32 gf_media_nalu_remove_emulation_bytes(const u8 *buffer_src, u8 *buffer_dst, u32 nal_size);


enum
{
	/*SPS has been parsed*/
	AVC_SPS_PARSED = 1,
	/*SPS has been declared to the upper layer*/
	AVC_SPS_DECLARED = 1<<1,
	/*SUB-SPS has been parsed*/
	AVC_SUBSPS_PARSED = 1<<2,
	/*SUB-SPS has been declared to the upper layer*/
	AVC_SUBSPS_DECLARED = 1<<3,
	/*SPS extension has been parsed*/
	AVC_SPS_EXT_DECLARED = 1<<4,
};

typedef struct
{
	u8 cpb_removal_delay_length_minus1;
	u8 dpb_output_delay_length_minus1;
	u8 time_offset_length;
	/*to be eventually completed by other hrd members*/
} AVC_HRD;

typedef struct
{
	s32 timing_info_present_flag;
	u32 num_units_in_tick;
	u32 time_scale;
	s32 fixed_frame_rate_flag;

	Bool aspect_ratio_info_present_flag;
	u32 par_num, par_den;

	Bool overscan_info_present_flag;
	Bool video_signal_type_present_flag;
	u8 video_format;
	Bool video_full_range_flag;

	Bool chroma_location_info_present_flag;
	u8 chroma_sample_loc_type_top_field, chroma_sample_loc_type_bottom_field;

	Bool colour_description_present_flag;
	u8 colour_primaries;
	u8 transfer_characteristics;
	u8 matrix_coefficients;

	Bool nal_hrd_parameters_present_flag;
	Bool vcl_hrd_parameters_present_flag;
	Bool low_delay_hrd_flag;
	AVC_HRD hrd;

	Bool pic_struct_present_flag;

	/*to be eventually completed by other vui members*/
} AVC_VUI;

typedef struct
{
	u32 left;
	u32 right;
	u32 top;
	u32 bottom;

} AVC_CROP;


typedef struct
{
	s32 profile_idc;
	s32 level_idc;
	s32 prof_compat;
	s32 log2_max_frame_num;
	u32 poc_type, poc_cycle_length;
	s32 log2_max_poc_lsb;
	s32 delta_pic_order_always_zero_flag;
	s32 offset_for_non_ref_pic, offset_for_top_to_bottom_field;
	Bool frame_mbs_only_flag;
	Bool mb_adaptive_frame_field_flag;
	u32 max_num_ref_frames;
	Bool gaps_in_frame_num_value_allowed_flag;
	u8 chroma_format;
	u8 luma_bit_depth_m8;
	u8 chroma_bit_depth_m8;
	u32 ChromaArrayType;

	s16 offset_for_ref_frame[256];

	u32 width, height;

	Bool vui_parameters_present_flag;
	AVC_VUI vui;
	AVC_CROP crop;

	/*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 sent*/
	u32 state;

	u32 sbusps_crc;

	/*for SVC stats during import*/
	u32 nb_ei, nb_ep, nb_eb;
} AVC_SPS;

typedef struct
{
	s32 id; /* used to compare pps when storing SVC PSS */
	s32 sps_id;
	Bool entropy_coding_mode_flag;
	s32 pic_order_present;			/* pic_order_present_flag*/
	s32 redundant_pic_cnt_present;	/* redundant_pic_cnt_present_flag */
	u32 slice_group_count;			/* num_slice_groups_minus1 + 1*/
	u32 mb_slice_group_map_type;
	u32 pic_size_in_map_units_minus1;
	u32 slice_group_change_rate_minus1;
	/*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 sent*/
	u32 status;
	Bool weighted_pred_flag;
	u8 weighted_bipred_idc;
	Bool deblocking_filter_control_present_flag;
	u32 num_ref_idx_l0_default_active_minus1, num_ref_idx_l1_default_active_minus1;
} AVC_PPS;

typedef struct
{
	s32 idr_pic_flag;
	u8 temporal_id, priority_id, dependency_id, quality_id;
} SVC_NALUHeader;

typedef struct
{
	u8 nal_ref_idc, nal_unit_type, field_pic_flag, bottom_field_flag;
	u32 frame_num, idr_pic_id, poc_lsb, slice_type;
	s32 delta_poc_bottom;
	s32 delta_poc[2];
	s32 redundant_pic_cnt;

	s32 poc;
	u32 poc_msb, poc_msb_prev, poc_lsb_prev, frame_num_prev;
	s32 frame_num_offset, frame_num_offset_prev;

	AVC_SPS *sps;
	AVC_PPS *pps;
	SVC_NALUHeader svc_nalhdr;
} AVCSliceInfo;


typedef struct
{
	u32 frame_cnt;
	u8 exact_match_flag;
	u8 broken_link_flag;
	u8 changing_slice_group_idc;
	u8 valid;
} AVCSeiRecoveryPoint;

typedef struct
{
	u8 pic_struct;
	/*to be eventually completed by other pic_timing members*/
} AVCSeiPicTiming;

typedef struct
{
	Bool rpu_flag;
} AVCSeiItuTT35DolbyVision;

typedef struct
{
	AVCSeiRecoveryPoint recovery_point;
	AVCSeiPicTiming pic_timing;
	AVCSeiItuTT35DolbyVision dovi;
	/*to be eventually completed by other sei*/
} AVCSei;

typedef struct
{
	AVC_SPS sps[32]; /* range allowed in the spec is 0..31 */
	s8 sps_active_idx, pps_active_idx;	/*currently active sps; must be initalized to -1 in order to discard not yet decodable SEIs*/

	AVC_PPS pps[255];

	AVCSliceInfo s_info;
	AVCSei sei;

	Bool is_svc;
	u8 last_nal_type_parsed;
	s8 last_ps_idx, last_sps_idx;
} AVCState;

typedef struct
{
	u32 NALUnitHeader;
	u8 track_ref_index;
	s8 sample_offset;
	u32 data_offset;
	u32 data_length;
} SVC_Extractor;


/*return sps ID or -1 if error*/
s32 gf_avc_read_sps(const u8 *sps_data, u32 sps_size, AVCState *avc, u32 subseq_sps, u32 *vui_flag_pos);
s32 gf_avc_read_sps_bs(GF_BitStream *bs, AVCState *avc, u32 subseq_sps, u32 *vui_flag_pos);
/*return pps ID or -1 if error*/
s32 gf_avc_read_pps(const u8 *pps_data, u32 pps_size, AVCState *avc);
s32 gf_avc_read_pps_bs(GF_BitStream *bs, AVCState *avc);

/*is slice containing intra MB only*/
Bool gf_avc_slice_is_intra(AVCState *avc);
/*parses NALU, updates avc state and returns:
	1 if NALU part of new frame
	0 if NALU part of prev frame
	-1 if bitstream error
*/
s32 gf_avc_parse_nalu(GF_BitStream *bs, AVCState *avc);
/*remove SEI messages not allowed in MP4*/
/*nota: 'buffer' remains unmodified but cannot be set const*/
u32 gf_avc_reformat_sei(u8 *buffer, u32 nal_size, Bool isobmf_rewrite, AVCState *avc);

#ifndef GPAC_DISABLE_AV_PARSERS

/*! VUI modification parameters*/
typedef struct
{
	/*! if true, the structure members will be updated to the actual values written or present in bitstream. If still -1, info was not written in bitstream*/
	Bool update;
	/*! pixel aspect ratio num
	a value of 0 in ar_num or ar_den removes PAR
	a value of -1 in ar_num or ar_den keeps PAR from bitstream
	positive values change PAR
	*/
	s32 ar_num;
	/*! pixel aspect ratio den*/
	s32 ar_den;

	//if set all video info is removed
	Bool remove_video_info;
	//if set timing info is removed
	Bool remove_vui_timing_info;
	//new fullrange, -1 to use info from bitstream
	s32 fullrange;
	//new vidformat flag, -1 to use info from bitstream
	s32 video_format;
	//new color primaries flag, -1 to use info from bitstream
	s32 color_prim;
	//new color transfer characteristics flag, -1 to use info from bitstream
	s32 color_tfc;
	//new color matrix flag, -1 to use info from bitstream
	s32 color_matrix;
} GF_VUIInfo;

GF_Err gf_avc_change_vui(GF_AVCConfig *avcc, GF_VUIInfo *vui_info);

//shortucts for the above for API compatibility
GF_Err gf_avc_change_par(GF_AVCConfig *avcc, s32 ar_n, s32 ar_d);
GF_Err gf_avc_change_color(GF_AVCConfig *avcc, s32 fullrange, s32 vidformat, s32 colorprim, s32 transfer, s32 colmatrix);

GF_Err gf_hevc_change_vui(GF_HEVCConfig *hvcc, GF_VUIInfo *vui);
//shortcut for the above for API compatibility
GF_Err gf_hevc_change_par(GF_HEVCConfig *hvcc, s32 ar_n, s32 ar_d);
GF_Err gf_hevc_change_color(GF_HEVCConfig *hvcc, s32 fullrange, s32 vidformat, s32 colorprim, s32 transfer, s32 colmatrix);
#endif

GF_Err gf_vvc_change_vui(GF_VVCConfig *cfg, GF_VUIInfo *vui);
//shortcut for the above for API compatibility
GF_Err gf_vvc_change_par(GF_VVCConfig *cfg, s32 ar_n, s32 ar_d);
GF_Err gf_vvc_change_color(GF_VVCConfig *cfg, s32 fullrange, s32 vidformat, s32 colorprim, s32 transfer, s32 colmatrix);


typedef struct
{
	Bool profile_present_flag, level_present_flag, tier_flag;
	u8 profile_space;
	u8 profile_idc;
	u32 profile_compatibility_flag;
	u8 level_idc;
} HEVC_SublayerPTL;

typedef struct
{
	u8 profile_space, tier_flag, profile_idc, level_idc;
	u32 profile_compatibility_flag;
	Bool general_progressive_source_flag;
	Bool general_interlaced_source_flag;
	Bool general_non_packed_constraint_flag;
	Bool general_frame_only_constraint_flag;
	u64 general_reserved_44bits;

	HEVC_SublayerPTL sub_ptl[8];
} HEVC_ProfileTierLevel;

typedef struct
{
	u32 num_negative_pics;
	u32 num_positive_pics;
	s32 delta_poc[16];
} HEVC_ReferencePictureSets;

typedef struct
{
	s32 id, vps_id;
	/*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 stored*/
	u32 state;
	u32 crc;
	u32 width, height;

	HEVC_ProfileTierLevel ptl;

	u8 chroma_format_idc;
	Bool cw_flag ;
	u32 cw_left, cw_right, cw_top, cw_bottom;
	u8 bit_depth_luma;
	u8 bit_depth_chroma;
	u8 log2_max_pic_order_cnt_lsb;
	Bool separate_colour_plane_flag;

	u32 max_CU_width, max_CU_height, max_CU_depth;
	u32 bitsSliceSegmentAddress;

	u32 num_short_term_ref_pic_sets, num_long_term_ref_pic_sps;
	HEVC_ReferencePictureSets rps[64];


	Bool aspect_ratio_info_present_flag, long_term_ref_pics_present_flag, temporal_mvp_enable_flag, sample_adaptive_offset_enabled_flag;
	u8 sar_idc;
	u16 sar_width, sar_height;
	Bool has_timing_info;
	u32 num_units_in_tick, time_scale;
	Bool poc_proportional_to_timing_flag;
	u32 num_ticks_poc_diff_one_minus1;

	Bool video_full_range_flag;
	Bool colour_description_present_flag;
	u8 colour_primaries, transfer_characteristic, matrix_coeffs;
	u32 rep_format_idx;

	u8 sps_ext_or_max_sub_layers_minus1, max_sub_layers_minus1, update_rep_format_flag, sub_layer_ordering_info_present_flag, scaling_list_enable_flag, infer_scaling_list_flag, scaling_list_ref_layer_id, scaling_list_data_present_flag, asymmetric_motion_partitions_enabled_flag, pcm_enabled_flag, strong_intra_smoothing_enable_flag, vui_parameters_present_flag;
	u32 log2_diff_max_min_luma_coding_block_size;
	u32 log2_min_transform_block_size, log2_min_luma_coding_block_size, log2_max_transform_block_size;
	u32 max_transform_hierarchy_depth_inter, max_transform_hierarchy_depth_intra;

	u8 pcm_sample_bit_depth_luma_minus1, pcm_sample_bit_depth_chroma_minus1, pcm_loop_filter_disable_flag;
	u32 log2_min_pcm_luma_coding_block_size_minus3, log2_diff_max_min_pcm_luma_coding_block_size;
	u8 overscan_info_present, overscan_appropriate, video_signal_type_present_flag, video_format;

	u8 chroma_loc_info_present_flag;
	u32 chroma_sample_loc_type_top_field, chroma_sample_loc_type_bottom_field;

	u8 neutral_chroma_indication_flag, field_seq_flag, frame_field_info_present_flag;
	u8 default_display_window_flag;
	u32 left_offset, right_offset, top_offset, bottom_offset;
	u8 hrd_parameters_present_flag;
} HEVC_SPS;

typedef struct
{
	s32 id;
	u32 sps_id;
	/*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 stored*/
	u32 state;
	u32 crc;

	Bool dependent_slice_segments_enabled_flag, tiles_enabled_flag, uniform_spacing_flag;
	u32 num_extra_slice_header_bits, num_ref_idx_l0_default_active, num_ref_idx_l1_default_active;
	Bool slice_segment_header_extension_present_flag, output_flag_present_flag, lists_modification_present_flag, cabac_init_present_flag;
	Bool weighted_pred_flag, weighted_bipred_flag, slice_chroma_qp_offsets_present_flag, deblocking_filter_override_enabled_flag, loop_filter_across_slices_enabled_flag, entropy_coding_sync_enabled_flag;
	Bool loop_filter_across_tiles_enabled_flag;
	s32 pic_init_qp_minus26;
	u32 num_tile_columns, num_tile_rows;
	u32 column_width[22], row_height[20];

	Bool sign_data_hiding_flag, constrained_intra_pred_flag, transform_skip_enabled_flag, cu_qp_delta_enabled_flag, transquant_bypass_enable_flag;
	u32 diff_cu_qp_delta_depth, pic_cb_qp_offset, pic_cr_qp_offset;

	Bool deblocking_filter_control_present_flag, pic_disable_deblocking_filter_flag, pic_scaling_list_data_present_flag;
	u32 beta_offset_div2, tc_offset_div2, log2_parallel_merge_level_minus2;

} HEVC_PPS;

typedef struct RepFormat
{
	u32 chroma_format_idc;
	u32 pic_width_luma_samples;
	u32 pic_height_luma_samples;
	u32 bit_depth_luma;
	u32 bit_depth_chroma;
	u8 separate_colour_plane_flag;
} HEVC_RepFormat;

typedef struct
{
	u16 avg_bit_rate, max_bit_rate, avg_pic_rate;
	u8 constant_pic_rate_idc;
} HEVC_RateInfo;


#define MAX_LHVC_LAYERS	4
#define MAX_NUM_LAYER_SETS 1024
typedef struct
{
	s32 id;
	/*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 stored*/
	u32 state;
	s32 bit_pos_vps_extensions;
	u32 crc;
	Bool vps_extension_found;
	u32 max_layers, max_sub_layers, max_layer_id, num_layer_sets;
	Bool temporal_id_nesting;
	HEVC_ProfileTierLevel ptl;

	HEVC_SublayerPTL sub_ptl[8];
	//this is not parsed yet (in VPS VUI)
	HEVC_RateInfo rates[8];


	u32 scalability_mask[16];
	u32 dimension_id[MAX_LHVC_LAYERS][16];
	u32 layer_id_in_nuh[MAX_LHVC_LAYERS];
	u32 layer_id_in_vps[MAX_LHVC_LAYERS];

	u8 num_profile_tier_level, num_output_layer_sets;
	u32 profile_level_tier_idx[MAX_LHVC_LAYERS];
	HEVC_ProfileTierLevel ext_ptl[MAX_LHVC_LAYERS];

	u32 num_rep_formats;
	HEVC_RepFormat rep_formats[16];
	u32 rep_format_idx[16];
	Bool base_layer_internal_flag, base_layer_available_flag;
	u8 num_layers_in_id_list[MAX_NUM_LAYER_SETS];
	u8 direct_dependency_flag[MAX_LHVC_LAYERS][MAX_LHVC_LAYERS];
	Bool output_layer_flag[MAX_LHVC_LAYERS][MAX_LHVC_LAYERS];
	u8 profile_tier_level_idx[MAX_LHVC_LAYERS][MAX_LHVC_LAYERS];
	Bool alt_output_layer_flag[MAX_LHVC_LAYERS];
	u8 num_necessary_layers[MAX_LHVC_LAYERS];
	Bool necessary_layers_flag[MAX_LHVC_LAYERS][MAX_LHVC_LAYERS];
	u8 LayerSetLayerIdList[MAX_LHVC_LAYERS][MAX_LHVC_LAYERS];
	u8 LayerSetLayerIdListMax[MAX_LHVC_LAYERS]; //the highest value in LayerSetLayerIdList[i]
} HEVC_VPS;

typedef struct
{
	AVCSeiRecoveryPoint recovery_point;
	AVCSeiPicTiming pic_timing;
	AVCSeiItuTT35DolbyVision dovi;
} HEVC_SEI;

typedef struct
{
	u8 nal_unit_type;
	u32 frame_num, poc_lsb, slice_type, header_size_with_emulation;
	
	s32 redundant_pic_cnt;

	s32 poc;
	u32 poc_msb, poc_msb_prev, poc_lsb_prev, frame_num_prev;
	s32 frame_num_offset, frame_num_offset_prev;

	Bool dependent_slice_segment_flag;
	Bool first_slice_segment_in_pic_flag;
	u32 slice_segment_address;
	u8 prev_layer_id_plus1;

	//bit offset of the num_entry_point (if present) field
	s32 entry_point_start_bits; 
	u64 header_size_bits;
	//byte offset of the payload start (after byte alignment)
	s32 payload_start_offset;

	s32 slice_qp_delta_start_bits;
	s32 slice_qp_delta;

	HEVC_SPS *sps;
	HEVC_PPS *pps;
} HEVCSliceInfo;

typedef struct _hevc_state
{
	//set by user
	Bool full_slice_header_parse;

	//all other vars set by parser

	HEVC_SPS sps[16]; /* range allowed in the spec is 0..15 */
	s8 sps_active_idx;	/*currently active sps; must be initalized to -1 in order to discard not yet decodable SEIs*/

	HEVC_PPS pps[64];

	HEVC_VPS vps[16];

	HEVCSliceInfo s_info;
	HEVC_SEI sei;

	//-1 or the value of the vps/sps/pps ID of the nal just parsed
	s32 last_parsed_vps_id;
	s32 last_parsed_sps_id;
	s32 last_parsed_pps_id;

	u8 clli_data[4];
	u8 mdcv_data[24];
	u8 clli_valid, mdcv_valid;
} HEVCState;

typedef struct hevc_combine{
	Bool is_hevccombine, first_slice_segment;
	s32 buffer_header_src_alloc; // because payload_start_offset is s32, otherwhise it's an u32
	u8 *buffer_header_src;
}Combine;

enum
{
	GF_HEVC_SLICE_TYPE_B = 0,
	GF_HEVC_SLICE_TYPE_P = 1,
	GF_HEVC_SLICE_TYPE_I = 2,
};
s32 gf_hevc_read_vps(u8 *data, u32 size, HEVCState *hevc);
s32 gf_hevc_read_vps_bs(GF_BitStream *bs, HEVCState *hevc);
s32 gf_hevc_read_sps(u8 *data, u32 size, HEVCState *hevc);
s32 gf_hevc_read_sps_bs(GF_BitStream *bs, HEVCState *hevc);
s32 gf_hevc_read_pps(u8 *data, u32 size, HEVCState *hevc);
s32 gf_hevc_read_pps_bs(GF_BitStream *bs, HEVCState *hevc);
s32 gf_hevc_parse_nalu(u8 *data, u32 size, HEVCState *hevc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id);
Bool gf_hevc_slice_is_intra(HEVCState *hevc);
Bool gf_hevc_slice_is_IDR(HEVCState *hevc);
//parses VPS and rewrites data buffer after removing VPS extension
s32 gf_hevc_read_vps_ex(u8 *data, u32 *size, HEVCState *hevc, Bool remove_extensions);

void gf_hevc_parse_ps(GF_HEVCConfig* hevccfg, HEVCState* hevc, u32 nal_type);
s32 gf_hevc_parse_nalu_bs(GF_BitStream *bs, HEVCState *hevc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id);

GF_Err gf_hevc_get_sps_info_with_state(HEVCState *hevc_state, u8 *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d);

/*parses HEVC SEI and fill state accordingly*/
void gf_hevc_parse_sei(char* buffer, u32 nal_size, HEVCState *hevc);



enum
{
	GF_VVC_SLICE_TYPE_B = 0,
	GF_VVC_SLICE_TYPE_P = 1,
	GF_VVC_SLICE_TYPE_I = 2,
	GF_VVC_SLICE_TYPE_UNKNOWN = 10,
};

#define VVC_MAX_REF_PICS	64

#define VVC_RPL_ST 0
#define VVC_RPL_LT 1
#define VVC_RPL_IL 2

typedef struct
{
	u32 num_ref_entries;
	u32 nb_short_term_pictures, nb_long_term_pictures, nb_inter_layer_pictures;

	u8 ref_pic_type[VVC_MAX_REF_PICS];
//	u32 ref_pic_id[VVC_MAX_REF_PICS];
//	s32 poc[VVC_MAX_REF_PICS];
//	u32 nb_active_pics;
//	u8 delta_poc_msb_present[VVC_MAX_REF_PICS];
//	s32 delta_poc_msb_cycle_lt[VVC_MAX_REF_PICS];
	u8 ltrp_in_header_flag;
//	u32 inter_layer_ref_pic_id[VVC_MAX_REF_PICS];
} VVC_RefPicList;

#define VVC_MAX_TILE_COLS 30
#define VVC_MAX_TILE_ROWS 33

//max number of subpics supported in GPAC
#define VVC_MAX_SUBPIC	200

typedef struct
{
	u16 x, y, w, h;
	u16 id;
	u16 num_slices;
} VVC_SubpicInfo;

typedef struct
{
	s32 id;
	u32 vps_id;
	u8 state;

	//all flags needed for further PPS , picture header or slice header parsing
	u8 max_sublayers, chroma_format_idc, log2_ctu_size, sps_ptl_dpb_hrd_params_present_flag;
	u8 gdr_enabled, ref_pic_resampling, res_change_in_clvs, explicit_scaling_list_enabled_flag;
	u8 virtual_boundaries_enabled_flag, virtual_boundaries_present_flag, joint_cbcr_enabled_flag;
	u8 dep_quant_enabled_flag, sign_data_hiding_enabled_flag, transform_skip_enabled_flag;
	u8 ph_num_extra_bits, sh_num_extra_bits, partition_constraints_override_enabled_flag;
	u8 alf_enabled_flag, ccalf_enabled_flag, lmcs_enabled_flag, long_term_ref_pics_flag, inter_layer_prediction_enabled_flag;
	u8 weighted_pred_flag, weighted_bipred_flag, temporal_mvp_enabled_flag, mmvd_fullpel_only_enabled_flag, bdof_control_present_in_ph_flag;
	u8 dmvr_control_present_in_ph_flag, prof_control_present_in_ph_flag, sao_enabled_flag, idr_rpl_present_flag;
	u8 entry_point_offsets_present_flag, entropy_coding_sync_enabled_flag;

	u8 conf_window;
	u32 cw_left, cw_right, cw_top, cw_bottom;

	//subpic info, not fully implemented yet
	u8 subpic_info_present, independent_subpic_flags, subpic_same_size, subpicid_mapping_explicit, subpicid_mapping_present;
	u32 nb_subpics; //up to VVC_MAX_SUBPIC
	u32 subpicid_len;
	VVC_SubpicInfo subpics[VVC_MAX_SUBPIC];

	Bool has_timing_info;
	u32 num_units_in_tick, time_scale;
	u32 width, height;

	u32 bitdepth;

	//POC compute
	u8 log2_max_poc_lsb, poc_msb_cycle_flag;
	u32 poc_msb_cycle_len;

	//reference picture lists
	u32 num_ref_pic_lists[2];
	VVC_RefPicList rps[2][64];

	//VUI
	u8 aspect_ratio_info_present_flag;
	u8 sar_idc;
	u16 sar_width, sar_height;

	u8 overscan_info_present_flag;
	u8 video_signal_type_present_flag;
	u8 video_format;
	u8 video_full_range_flag;

	u8 colour_description_present_flag;
	u8 colour_primaries;
	u8 transfer_characteristics;
	u8 matrix_coefficients;

	//SPS range extensions, not yet parsed
	u8 ts_residual_coding_rice_present_in_sh_flag, reverse_last_sig_coeff_enabled_flag;
} VVC_SPS;

typedef struct
{
	s32 id;
	u32 sps_id;
	u8 state;
	//all flags needed for further picture header or slice header parsing
	u8 mixed_nal_types, output_flag_present_flag, no_pic_partition_flag, subpic_id_mapping_present_flag, rect_slice_flag;
	u8 alf_info_in_ph_flag, rpl_info_in_ph_flag, cu_qp_delta_enabled_flag, cu_chroma_qp_offset_list_enabled_flag, weighted_pred_flag;
	u8 weighted_bipred_flag, wp_info_in_ph_flag, qp_delta_info_in_ph_flag, sao_info_in_ph_flag, dbf_info_in_ph_flag;
	u8 deblocking_filter_disabled_flag, deblocking_filter_override_enabled_flag, chroma_tool_offsets_present_flag;
	u8 slice_chroma_qp_offsets_present_flag, picture_header_extension_present_flag, rpl1_idx_present_flag;
	u8 cabac_init_present_flag, slice_header_extension_present_flag, single_slice_per_subpic_flag;

	u32 num_ref_idx_default_active[2];
	u32 num_tiles_in_pic, num_tile_rows, num_tile_cols, slice_address_len, num_slices_in_pic;

	u32 width, height;
	u8 conf_window;
	u32 cw_left, cw_right, cw_top, cw_bottom;

	VVC_SubpicInfo subpics[VVC_MAX_SUBPIC];

	//tile info
	u32 tile_rows_height_ctb[VVC_MAX_TILE_ROWS];
	u32 tile_cols_width_ctb[VVC_MAX_TILE_COLS];
	u32 pic_width_in_ctbsY, pic_height_in_ctbsY;
} VVC_PPS;

#define VVC_MAX_LAYERS	4
#define VVC_MAX_NUM_LAYER_SETS 1024


typedef struct
{
	u8 profile_present_flag, level_present_flag;
	u8 sublayer_level_idc;

} VVC_SublayerPTL;

typedef struct
{
	u8 pt_present;
	u8 ptl_max_tid;

	u8 general_profile_idc, general_tier_flag, general_level_idc, frame_only_constraint, multilayer_enabled;
	VVC_SublayerPTL sub_ptl[8];

	u8 num_sub_profiles;
	u32 sub_profile_idc[255];

	u8 gci_present;
	//holds 81 bits, the last byte contains the remainder (low bit set, not high)
	u8 gci[12];
} VVC_ProfileTierLevel;

typedef struct
{
	s32 id;
	u8 state;

	HEVC_RateInfo rates[8];
	u32 max_layers, max_sub_layers;
	Bool all_layers_independent, each_layer_is_ols;
	u32 max_layer_id; //, num_layer_sets;

	u16 num_ptl; //max 256
	VVC_ProfileTierLevel ptl[256];
} VVC_VPS;

typedef struct
{
	u8 nal_unit_type;
	u32 frame_num, poc_lsb, slice_type;

	u8 poc_msb_cycle_present_flag;
	s32 poc;
	u32 poc_msb, poc_msb_cycle, poc_msb_prev, poc_lsb_prev, frame_num_prev;

	VVC_SPS *sps;
	VVC_PPS *pps;

	u8 picture_header_in_slice_header_flag, inter_slice_allowed_flag, intra_slice_allowed_flag;
	u8 irap_or_gdr_pic;
	u8 non_ref_pic;
	u8 gdr_pic;
	u32 gdr_recovery_count;
	u8 recovery_point_valid, lmcs_enabled_flag, explicit_scaling_list_enabled_flag, temporal_mvp_enabled_flag;

	u8 prev_layer_id_plus1;
	u8 compute_poc_defer;

	//picture header RPL state
	VVC_RefPicList ph_rpl[2];
	s32 ph_rpl_idx[2];

	//slive RPL state
	VVC_RefPicList rpl[2];
	s32 rpl_idx[2];

	//slice header size in bytes
	u32 payload_start_offset;
} VVCSliceInfo;

/*TODO once we add HLS parsing (FDIS) */
typedef struct _vvc_state
{
	s8 sps_active_idx;	/*currently active sps; must be initalized to -1 in order to discard not yet decodable SEIs*/

	//-1 or the value of the vps/sps/pps ID of the nal just parsed
	s32 last_parsed_vps_id;
	s32 last_parsed_sps_id;
	s32 last_parsed_pps_id;
	s32 last_parsed_aps_id;

	VVC_SPS sps[16];
	VVC_PPS pps[64];
	VVC_VPS vps[16];

	VVCSliceInfo s_info;

	//0: minimal parsing, used by most tools. Slice header and picture header are skipped
	//1: full parsing, error check: used to retrieve end of slice header
	//2: full parsing, no error check (used by dumpers)
	u32 parse_mode;


	u8 clli_data[4];
	u8 mdcv_data[24];
	u8 clli_valid, mdcv_valid;
} VVCState;

s32 gf_vvc_parse_nalu_bs(GF_BitStream *bs, VVCState *vvc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id);
void gf_vvc_parse_sei(char* buffer, u32 nal_size, VVCState *vvc);
Bool gf_vvc_slice_is_ref(VVCState *vvc);
s32 gf_vvc_parse_nalu(u8 *data, u32 size, VVCState *vvc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id);

void gf_vvc_parse_ps(GF_VVCConfig* hevccfg, VVCState* vvc, u32 nal_type);


GF_Err gf_media_parse_ivf_file_header(GF_BitStream *bs, u32 *width, u32*height, u32 *codec_fourcc, u32 *timebase_num, u32 *timebase_den, u32 *num_frames);



#define VP9_MAX_FRAMES_IN_SUPERFRAME 16

GF_Err gf_vp9_parse_sample(GF_BitStream *bs, GF_VPConfig *vp9_cfg, Bool *key_frame, u32 *FrameWidth, u32 *FrameHeight, u32 *renderWidth, u32 *renderHeight);
GF_Err gf_vp9_parse_superframe(GF_BitStream *bs, u64 ivf_frame_size, u32 *num_frames_in_superframe, u32 frame_sizes[VP9_MAX_FRAMES_IN_SUPERFRAME], u32 *superframe_index_size);



#define AV1_MAX_TILE_ROWS 64
#define AV1_MAX_TILE_COLS 64

typedef enum {
	AV1_KEY_FRAME = 0,
	AV1_INTER_FRAME = 1,
	AV1_INTRA_ONLY_FRAME = 2,
	AV1_SWITCH_FRAME = 3,
} AV1FrameType;

typedef struct
{
	//offset in bytes after first byte of obu, including its header
	u32 obu_start_offset;
	u32 size;
} AV1Tile;

typedef struct
{
	Bool is_first_frame;
	Bool seen_frame_header, seen_seq_header;
	Bool key_frame, show_frame;
	AV1FrameType frame_type;
	GF_List *header_obus, *frame_obus; /*GF_AV1_OBUArrayEntry*/
	AV1Tile tiles[AV1_MAX_TILE_ROWS * AV1_MAX_TILE_COLS];
	u32 nb_tiles_in_obu;
	u8 refresh_frame_flags;
	u8 order_hint;
	u8 allow_high_precision_mv;
	u8 show_existing_frame, frame_to_show_map_idx;
	//indicates the size of the uncompressed_header syntax element. This is set back to 0 at the next OBU parsing
	u16 uncompressed_header_bytes;
} AV1StateFrame;

#define AV1_NUM_REF_FRAMES	8

typedef struct
{
	s32 coefs[AV1_NUM_REF_FRAMES][6];
} AV1GMParams;

typedef struct
{
	/*importing options*/
	Bool keep_temporal_delim;
	/*parser config*/
	//if set only header frames are stored
	Bool skip_frames;
	//if set, frame OBUs are not pushed to the frame_obus OBU list but are written in the below bitstream
	Bool mem_mode;
	/*bitstream object for mem mode - this bitstream is NOT destroyed by gf_av1_reset_state(state, GF_TRUE) */
	GF_BitStream *bs;
	Bool unframed;
	u8 *frame_obus;
	u32 frame_obus_alloc;

	/*general sequence information*/
	Bool frame_id_numbers_present_flag;
	Bool reduced_still_picture_header;
	Bool decoder_model_info_present_flag;
	u16 OperatingPointIdc;
	u32 width, height, UpscaledWidth;
	u32 sequence_width, sequence_height;
	u32 tb_num, tb_den;

	Bool use_128x128_superblock;
	u8 frame_width_bits_minus_1, frame_height_bits_minus_1;
	u8 equal_picture_interval;
	u8 delta_frame_id_length_minus_2;
	u8 additional_frame_id_length_minus_1;
	u8 seq_force_integer_mv;
	u8 seq_force_screen_content_tools;
	Bool enable_superres;
	Bool enable_order_hint;
	Bool enable_cdef;
	Bool enable_restoration;
	Bool enable_warped_motion;
	u8 OrderHintBits;
	Bool enable_ref_frame_mvs;
	Bool film_grain_params_present;
	u8 buffer_delay_length;
	u8 frame_presentation_time_length;
	u32 buffer_removal_time_length;
	u8 operating_points_count;
	u8 decoder_model_present_for_this_op[32];
	u8 operating_point_idc[32];
	u8 initial_display_delay_present_for_this_op[32];
	u8 initial_display_delay_minus1_for_this_op[32];

	u32 tileRows, tileCols, tileRowsLog2, tileColsLog2;
	u8 tile_size_bytes; /*coding tile header size*/
	Bool separate_uv_delta_q;

	/*Needed for RFC6381*/
	Bool still_picture;
	u8 bit_depth;
	Bool color_description_present_flag;
	u8 color_primaries, transfer_characteristics, matrix_coefficients;
	Bool color_range;

	/*AV1 config record - shall not be null when parsing - this is NOT destroyed by gf_av1_reset_state(state, GF_TRUE) */
	GF_AV1Config *config;

	/*OBU parsing state, reset at each obu*/
	Bool obu_has_size_field, obu_extension_flag;
	u8 temporal_id, spatial_id;
	ObuType obu_type;

	/*inter-frames state */
	u8 RefOrderHint[AV1_NUM_REF_FRAMES];
	u8 RefValid[AV1_NUM_REF_FRAMES];
	u8 OrderHints[AV1_NUM_REF_FRAMES];

	AV1GMParams GmParams;
	AV1GMParams PrevGmParams;
	AV1GMParams SavedGmParams[AV1_NUM_REF_FRAMES];
	u8 RefFrameType[AV1_NUM_REF_FRAMES];

	u32 RefUpscaledWidth[AV1_NUM_REF_FRAMES];
	u32 RefFrameHeight[AV1_NUM_REF_FRAMES];

	/*frame parsing state*/
	AV1StateFrame frame_state;

	/*layer sizes for AVIF a1lx*/
	u32 layer_size[4];


	u8 clli_data[4];
	u8 mdcv_data[24];
	u8 clli_valid, mdcv_valid;

	//set to one if a temporal delim is found when calling aom_av1_parse_temporal_unit_from_section5
	u8 has_temporal_delim;
} AV1State;

GF_Err aom_av1_parse_temporal_unit_from_section5(GF_BitStream *bs, AV1State *state);
GF_Err aom_av1_parse_temporal_unit_from_annexb(GF_BitStream *bs, AV1State *state);
GF_Err aom_av1_parse_temporal_unit_from_ivf(GF_BitStream *bs, AV1State *state);

/*may return GF_BUFFER_TOO_SMALL if not enough bytes*/
GF_Err gf_media_parse_ivf_frame_header(GF_BitStream *bs, u64 *frame_size, u64 *pts);

Bool gf_media_probe_ivf(GF_BitStream *bs);
Bool gf_media_aom_probe_annexb(GF_BitStream *bs);

/*parses one OBU
\param bs bitstream object
\param obu_type OBU type
\param obu_size As an input the size of the input OBU (needed when obu_size is not coded). As an output the coded obu_size value.
\param obu_hdr_size OBU header size
\param state the frame parser
*/
GF_Err gf_av1_parse_obu(GF_BitStream *bs, ObuType *obu_type, u64 *obu_size, u32 *obu_hdr_size, AV1State *state);

Bool av1_is_obu_header(ObuType obu_type);

/*! init av1 frame parsing state
\param state the frame parser
*/
void gf_av1_init_state(AV1State *state);

/*! reset av1 frame parsing state - this does not destroy the structure.
\param state the frame parser
\param is_destroy if TRUE, destroy internal reference picture lists
*/
void gf_av1_reset_state(AV1State *state, Bool is_destroy);

u64 gf_av1_leb128_read(GF_BitStream *bs, u8 *opt_Leb128Bytes);
u32 gf_av1_leb128_size(u64 value);
u64 gf_av1_leb128_write(GF_BitStream *bs, u64 value);
GF_Err gf_av1_parse_obu_header(GF_BitStream *bs, ObuType *obu_type, Bool *obu_extension_flag, Bool *obu_has_size_field, u8 *temporal_id, u8 *spatial_id);

/*! OPUS packet header*/
typedef struct
{
    Bool self_delimited;

    // parsed header size
    u8 size;

    u16 self_delimited_length;
    u8 TOC_config;
    u8 TOC_stereo;
    u8 TOC_code;
    u16 code2_frame_length;
    u8 code3_vbr;
    u8 code3_padding;
    u16 code3_padding_length;

    u8 nb_frames;
    // either explicitly coded (e.g. self_delimited) or computer (e.g. cbr)
    u16 frame_lengths[255];

    // computed packet size
    u32 packet_size;
} GF_OpusPacketHeader;

u8 gf_opus_parse_packet_header(u8 *data, u32 data_length, Bool self_delimited, GF_OpusPacketHeader *header);

typedef struct
{
	u32 picture_size;
	u16 deprecated_number_of_slices;
	u8 log2_desired_slice_size_in_mb;
} GF_ProResPictureInfo;

typedef struct
{
	u32 frame_size;
	u32 frame_identifier;
	u16 frame_hdr_size;
	u8 version;
	u32 encoder_id;
	u16 width;
	u16 height;
	u8 chroma_format;
	u8 interlaced_mode;
	u8 aspect_ratio_information;
	u8 framerate_code;
	u8 color_primaries;
	u8 transfer_characteristics;
	u8 matrix_coefficients;
	u8 alpha_channel_type;
	u8 load_luma_quant_matrix, load_chroma_quant_matrix;
	u8 luma_quant_matrix[8][8];
	u8 chroma_quant_matrix[8][8];

	u8 nb_pic; //1 or 2
	//for now we don't parse this
//	GF_ProResPictureInfo pictures[2];
} GF_ProResFrameInfo;

GF_Err gf_media_prores_parse_bs(GF_BitStream *bs, GF_ProResFrameInfo *prores_frame);

#endif /*GPAC_DISABLE_AV_PARSERS*/

typedef struct
{
	u8 rate_idx;
	u8 pck_size;
} QCPRateTable;


#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_STREAMING)

GP_RTPPacketizer *gf_rtp_packetizer_create_and_init_from_file(GF_ISOFile *file,
        u32 TrackNum,
        void *cbk_obj,
        void (*OnNewPacket)(void *cbk, GF_RTPHeader *header),
        void (*OnPacketDone)(void *cbk, GF_RTPHeader *header),
        void (*OnDataReference)(void *cbk, u32 payload_size, u32 offset_from_orig),
        void (*OnData)(void *cbk, u8 *data, u32 data_size, Bool is_head),
        u32 Path_MTU,
        u32 max_ptime,
        u32 default_rtp_rate,
        u32 flags,
        u8 PayloadID,
        Bool copy_media,
        u32 InterleaveGroupID,
        u8 InterleaveGroupPriority);

void gf_media_format_ttxt_sdp(GP_RTPPacketizer *builder, char *payload_name, char *sdpLine, u32 w, u32 h, s32 tx, s32 ty, s16 l, u32 max_w, u32 max_h, char *tx3g_base64);

#endif


#ifndef GPAC_DISABLE_VTT

typedef struct _webvtt_parser GF_WebVTTParser;
typedef struct _webvtt_sample GF_WebVTTSample;

GF_WebVTTParser *gf_webvtt_parser_new();
GF_Err gf_webvtt_parser_init(GF_WebVTTParser *parser, FILE *vtt_file, s32 unicode_type, Bool is_srt,
                             void *user, GF_Err (*report_message)(void *, GF_Err, char *, const char *),
                             void (*on_sample_parsed)(void *, GF_WebVTTSample *),
                             void (*on_header_parsed)(void *, const char *));
GF_Err gf_webvtt_parser_parse(GF_WebVTTParser *parser);
void gf_webvtt_parser_del(GF_WebVTTParser *parser);
void gf_webvtt_parser_suspend(GF_WebVTTParser *vttparser);
void gf_webvtt_parser_restart(GF_WebVTTParser *parser);
GF_Err gf_webvtt_parser_parse_payload(GF_WebVTTParser *parser, u64 start, u64 end, const char *vtt_pre, const char *vtt_cueid, const char *vtt_settings);
GF_Err gf_webvtt_parser_flush(GF_WebVTTParser *parser);

#include <gpac/webvtt.h>
void gf_webvtt_parser_cue_callback(GF_WebVTTParser *parser, void (*on_cue_read)(void *, GF_WebVTTCue *), void *udta);
GF_Err gf_webvtt_merge_cues(GF_WebVTTParser *parser, u64 start, GF_List *cues);
GF_Err gf_webvtt_parser_finalize(GF_WebVTTParser *parser, u64 duration);

void gf_webvtt_sample_del(GF_WebVTTSample * samp);
u64 gf_webvtt_sample_get_start(GF_WebVTTSample * samp);
u64 gf_webvtt_sample_get_end(GF_WebVTTSample * samp);



#ifndef GPAC_DISABLE_ISOM
GF_Err gf_webvtt_dump_header(FILE *dump, GF_ISOFile *file, u32 track, Bool box_mode, u32 index);
GF_Err gf_webvtt_parser_dump_done(GF_WebVTTParser *parser, u32 duration);
#endif /* GPAC_DISABLE_ISOM */

#endif /* GPAC_DISABLE_VTT */


#define M4V_VO_START_CODE					0x00
#define M4V_VOL_START_CODE					0x20
#define M4V_VOP_START_CODE					0xB6
#define M4V_VISOBJ_START_CODE				0xB5
#define M4V_VOS_START_CODE					0xB0
#define M4V_GOV_START_CODE					0xB3
#define M4V_UDTA_START_CODE					0xB2


#define M2V_PIC_START_CODE					0x00
#define M2V_SEQ_START_CODE					0xB3
#define M2V_EXT_START_CODE					0xB5
#define M2V_GOP_START_CODE					0xB8
#define M2V_UDTA_START_CODE					0xB2


/*build isobmf dec info from sequence header+ephdr (only seq hdr is parsed, only advanced profile is supprted) */
GF_Err gf_media_vc1_seq_header_to_dsi(const u8 *seq_hdr, u32 seq_hdr_len, u8 **dsi, u32 *dsi_size);


#endif		/*_GF_MEDIA_DEV_H_*/

