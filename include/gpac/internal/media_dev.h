/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#ifndef GPAC_DISABLE_STREAMING
#include <gpac/ietf.h>
#endif

#ifndef GPAC_DISABLE_ISOM
void gf_media_get_sample_average_infos(GF_ISOFile *file, u32 Track, u32 *avgSize, u32 *MaxSize, u32 *TimeDelta, u32 *maxCTSDelta, u32 *const_duration, u32 *bandwidth);
#endif


#ifndef GPAC_DISABLE_MEDIA_IMPORT
GF_Err gf_import_message(GF_MediaImporter *import, GF_Err e, char *format, ...);
#endif /*GPAC_DISABLE_MEDIA_IMPORT*/

#ifndef GPAC_DISABLE_AV_PARSERS

u32 gf_latm_get_value(GF_BitStream *bs);

#define GF_SVC_SSPS_ID_SHIFT	16	

/*returns 0 if not a start code, or size of start code (3 or 4 bytes). If start code, bitstream
is positionned AFTER start code*/
u32 gf_media_nalu_is_start_code(GF_BitStream *bs);

/*returns size of chunk between current and next startcode (excluding startcode sizes), 0 if no more startcodes (eos)*/
u32 gf_media_nalu_next_start_code_bs(GF_BitStream *bs);

/*return nb bytes from current data until the next start code and set the size of the next start code (3 or 4 bytes)
returns data_len if no startcode found and sets sc_size to 0 (last nal in payload)*/
u32 gf_media_nalu_next_start_code(u8 *data, u32 data_len, u32 *sc_size);

/*returns NAL unit type - bitstream must be sync'ed!!*/
u8 AVC_NALUType(GF_BitStream *bs);
Bool SVC_NALUIsSlice(u8 type);


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

	u32 par_num, par_den;

	Bool nal_hrd_parameters_present_flag;
	Bool vcl_hrd_parameters_present_flag;
	AVC_HRD hrd;

	Bool pic_struct_present_flag;
	/*to be eventually completed by other vui members*/
} AVC_VUI;

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
	u8 chroma_format;
	u8 luma_bit_depth_m8;
	u8 chroma_bit_depth_m8;

	s16 offset_for_ref_frame[256];

	u32 width, height;

	AVC_VUI vui;
	
	/*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 sent*/
	u32 state;

	/*for SVC stats during import*/
	u32 nb_ei, nb_ep, nb_eb;
} AVC_SPS;

typedef struct 
{
	s32 id; /* used to compare pps when storing SVC PSS */
	s32 sps_id;
	s32 pic_order_present;			/* pic_order_present_flag*/
	s32 redundant_pic_cnt_present;	/* redundant_pic_cnt_present_flag */
	u32 slice_group_count;			/* num_slice_groups_minus1 + 1*/
	/*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 sent*/
	u32 status;

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
	SVC_NALUHeader NalHeader;
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
	AVCSeiRecoveryPoint recovery_point;
	AVCSeiPicTiming pic_timing;
	/*to be eventually completed by other sei*/
} AVCSei;

typedef struct
{
	AVC_SPS sps[32]; /* range allowed in the spec is 0..31 */
	s8 sps_active_idx;	/*currently active sps; must be initalized to -1 in order to discard not yet decodable SEIs*/

	AVC_PPS pps[255];

	AVCSliceInfo s_info;
	AVCSei sei;

	Bool is_svc;
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
s32 gf_media_avc_read_sps(const char *sps_data, u32 sps_size, AVCState *avc, u32 subseq_sps, u32 *vui_flag_pos);
/*return pps ID or -1 if error*/
s32 gf_media_avc_read_pps(const char *pps_data, u32 pps_size, AVCState *avc);
/*return sps ID or -1 if error*/
s32 gf_media_avc_read_sps_ext(const char *spse_data, u32 spse_size);
/*is slice an IDR*/
Bool gf_media_avc_slice_is_IDR(AVCState *avc);
/*is slice containing intra MB only*/
Bool gf_media_avc_slice_is_intra(AVCState *avc);
/*parses NALU, updates avc state and returns:
	1 if NALU part of new frame
	0 if NALU part of prev frame
	-1 if bitstream error
*/
s32 gf_media_avc_parse_nalu(GF_BitStream *bs, u32 nal_hdr, AVCState *avc);
/*remove SEI messages not allowed in MP4*/
/*nota: 'buffer' remains unmodified but cannot be set const*/
u32 gf_media_avc_reformat_sei(char *buffer, u32 nal_size, AVCState *avc);

#ifndef GPAC_DISABLE_ISOM
GF_Err gf_media_avc_change_par(GF_AVCConfig *avcc, s32 ar_n, s32 ar_d);
GF_Err gf_media_hevc_change_par(GF_HEVCConfig *hvcc, s32 ar_n, s32 ar_d);
#endif



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

	u32 rep_format_idx;
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

	u32 num_tile_columns, num_tile_rows;
	u32 column_width[22], row_height[20];
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
	u8 constand_pic_rate_idc;
} HEVC_RateInfo;


#define MAX_SHVC_LAYERS	4
typedef struct
{
	s32 id; 
	/*used to discard repeated SPSs - 0: not parsed, 1 parsed, 2 stored*/
	u32 state;
	u32 crc;
	u32 max_layers, max_sub_layers, max_layer_id, num_layer_sets;
	Bool temporal_id_nesting;
	HEVC_ProfileTierLevel ptl;

	HEVC_SublayerPTL sub_ptl[8];
	HEVC_RateInfo rates[8];


	u32 scalability_mask[16];
    u32 dimension_id[MAX_SHVC_LAYERS][16];
    u32 layer_id_in_nuh[MAX_SHVC_LAYERS];
    u32 layer_id_in_vps[MAX_SHVC_LAYERS];


	u32 profile_level_tier_idx[MAX_SHVC_LAYERS];
	HEVC_ProfileTierLevel ext_ptl[MAX_SHVC_LAYERS];

	u32 num_rep_formats;
	HEVC_RepFormat rep_formats[16];
    u32 rep_format_idx[16];
} HEVC_VPS;

typedef struct
{
	AVCSeiRecoveryPoint recovery_point;
	AVCSeiPicTiming pic_timing;

} HEVC_SEI;

typedef struct
{
	u8 nal_unit_type;
	s8 temporal_id;

	u32 frame_num, poc_lsb, slice_type;

	s32 redundant_pic_cnt;

	s32 poc;
	u32 poc_msb, poc_msb_prev, poc_lsb_prev, frame_num_prev;
	s32 frame_num_offset, frame_num_offset_prev;

	Bool dependent_slice_segment_flag;
	Bool first_slice_segment_in_pic_flag;
	u32 slice_segment_address;

	HEVC_SPS *sps;
	HEVC_PPS *pps;
} HEVCSliceInfo;

typedef struct _hevc_state
{
	HEVC_SPS sps[16]; /* range allowed in the spec is 0..15 */
	s8 sps_active_idx;	/*currently active sps; must be initalized to -1 in order to discard not yet decodable SEIs*/

	HEVC_PPS pps[64];

	HEVC_VPS vps[16];

	HEVCSliceInfo s_info;
	HEVC_SEI sei;

	Bool is_svc;
} HEVCState;

enum
{
	GF_HEVC_TYPE_B = 0,
	GF_HEVC_TYPE_P = 1,
	GF_HEVC_TYPE_I = 2,
};
s32 gf_media_hevc_read_vps(char *data, u32 size, HEVCState *hevc);
s32 gf_media_hevc_read_sps(char *data, u32 size, HEVCState *hevc);
s32 gf_media_hevc_read_pps(char *data, u32 size, HEVCState *hevc);
s32 gf_media_hevc_parse_nalu(GF_BitStream *bs, HEVCState *hevc, u8 *nal_unit_type, u8 *temporal_id, u8 *layer_id);
Bool gf_media_hevc_slice_is_intra(HEVCState *hevc);
Bool gf_media_hevc_slice_is_IDR(HEVCState *hevc);

GF_Err gf_hevc_get_sps_info_with_state(HEVCState *hevc_state, char *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d);

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
															  void (*OnData)(void *cbk, char *data, u32 data_size, Bool is_head),
															  u32 Path_MTU, 
															  u32 max_ptime, 
															  u32 default_rtp_rate, 
															  u32 flags, 
															  u8 PayloadID, 
															  Bool copy_media, 
															  u32 InterleaveGroupID, 
															  u8 InterleaveGroupPriority);

void gf_media_format_ttxt_sdp(GP_RTPPacketizer *builder, char *payload_name, char *sdpLine, GF_ISOFile *file, u32 track);

#endif


typedef enum
{
	GF_DASH_TEMPLATE_SEGMENT = 0,
	GF_DASH_TEMPLATE_INITIALIZATION,
	GF_DASH_TEMPLATE_TEMPLATE,
	GF_DASH_TEMPLATE_INITIALIZATION_TEMPLATE,
	GF_DASH_TEMPLATE_REPINDEX,
} GF_DashTemplateSegmentType;

GF_Err gf_media_mpd_format_segment_name(GF_DashTemplateSegmentType seg_type, Bool is_bs_switching, char *segment_name, const char *output_file_name, const char *rep_id, const char *seg_rad_name, const char *seg_ext, u64 start_time, u32 bandwidth, u32 segment_number, Bool use_segment_timeline);

#ifndef GPAC_DISABLE_VTT

typedef struct _webvtt_parser GF_WebVTTParser;
typedef struct _webvtt_sample GF_WebVTTSample;

GF_WebVTTParser *gf_webvtt_parser_new();
GF_Err gf_webvtt_parser_init(GF_WebVTTParser *parser, const char *input_file, 
                                    void *user, GF_Err (*report_message)(void *, GF_Err, char *, const char *),
                                    void (*on_sample_parsed)(void *, GF_WebVTTSample *),
                                    void (*on_header_parsed)(void *, const char *));
GF_Err gf_webvtt_parser_parse(GF_WebVTTParser *parser, u32 duration);
u64 gf_webvtt_parser_last_duration(GF_WebVTTParser *parser);
void gf_webvtt_parser_del(GF_WebVTTParser *parser);

void gf_webvtt_sample_del(GF_WebVTTSample * samp);
u64 gf_webvtt_sample_get_start(GF_WebVTTSample * samp);

#ifndef GPAC_DISABLE_ISOM
GF_Err gf_webvtt_dump_header(FILE *dump, GF_ISOFile *file, u32 track, u32 index);
GF_Err gf_webvtt_dump_sample(FILE *dump, GF_WebVTTSample *samp);
GF_Err gf_webvtt_parser_dump_done(GF_WebVTTParser *parser, u32 duration);
#endif /* GPAC_DISABLE_ISOM */

#endif /* GPAC_DISABLE_VTT */

#endif		/*_GF_MEDIA_DEV_H_*/

