/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2017-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / inspection filter
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

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <gpac/list.h>
#include <gpac/xml.h>
#include <gpac/internal/media_dev.h>

typedef struct
{
	GF_FilterPid *src_pid;
	FILE *tmp;
	u64 pck_num;
	u32 idx;
	u8 dump_pid; //0: no, 1: configure/reconfig, 2: info update
	u8 init_pid_config_done;
	u64 pck_for_config;
	u64 prev_dts, prev_cts, init_ts;
	u32 codec_id;
	u32 stream_type;

#ifndef GPAC_DISABLE_AV_PARSERS
	HEVCState *hevc_state;
	AVCState *avc_state;
	VVCState *vvc_state;
	AV1State *av1_state;
	GF_M4VParser *mv124_state;
	GF_M4VDecSpecInfo dsi;
#endif
	GF_VPConfig *vpcc;

	//for analyzing
	GF_BitStream *bs;

	Bool has_svcc;
	u32 nalu_size_length;
	Bool is_adobe_protected;
	Bool is_cenc_protected;
	Bool aborted;

	GF_Fraction tmcd_rate;
	u32 tmcd_flags;
	u32 tmcd_fpt;

	u32 opus_channel_count;

	u32 csize;
	Bool buffer_done, no_analysis;
	u32 buf_start_time;
	u64 last_pcr;
	GF_FilterClockType last_clock_type;
} PidCtx;

enum
{
	INSPECT_MODE_PCK=0,
	INSPECT_MODE_BLOCK,
	INSPECT_MODE_REFRAME,
	INSPECT_MODE_RAW
};

enum
{
	INSPECT_TEST_NO=0,
	INSPECT_TEST_NOPROP,
	INSPECT_TEST_NETWORK,
	INSPECT_TEST_NETX,
	INSPECT_TEST_ENCODE,
	INSPECT_TEST_ENCX,
	INSPECT_TEST_NOCRC,
	INSPECT_TEST_NOBR
};

enum
{
	INSPECT_ANALYZE_OFF=0,
	INSPECT_ANALYZE_ON,
	INSPECT_ANALYZE_BS,
	INSPECT_ANALYZE_BS_BITS,
};

typedef struct
{
	u32 mode;
	Bool interleave;
	Bool dump_data;
	Bool deep;
	char *log;
	char *fmt;
	u32 analyze;
	Bool props, hdr, allp, info, pcr, xml, full;
	Double speed, start;
	u32 test;
	GF_Fraction dur;
	Bool crc, dtype;
	Bool fftmcd;
	u32 buffer, mbuffer, rbuffer;

	FILE *dump;
	Bool dump_log;

	GF_List *src_pids;

	Bool is_prober, probe_done, hdr_done, dump_pck;
	Bool args_updated;
	Bool has_seen_eos;
} GF_InspectCtx;


GF_Err gf_bs_set_logger(GF_BitStream *bs, void (*on_bs_log)(void *udta, const char *field_name, u32 nb_bits, u64 field_val, s32 idx1, s32 idx2, s32 idx3), void *udta);

static void inspect_printf(FILE *dump, const char *fmt, ...)
{
	va_list list;
	if (dump == NULL) {
#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_APP, GF_LOG_INFO) ) {
			gf_log_lt(GF_LOG_INFO, GF_LOG_APP);
			va_start(list, fmt);
			gf_log_va_list(GF_LOG_INFO, GF_LOG_APP, fmt, list);
			va_end(list);
		}
#endif
	} else {
		va_start(list, fmt);
		gf_vfprintf(dump, fmt, list);
		va_end(list);
	}
}

#define DUMP_ATT_STR(_name, _val) if (ctx->xml) { \
		inspect_printf(dump, " %s=\"%s\"", _name, _val); \
	} else { \
		inspect_printf(dump, " %s %s", _name, _val); \
	}

#define DUMP_ATT_LLU(_name, _val) if (ctx->xml) { \
		inspect_printf(dump, " %s=\""LLU"\"", _name, _val);\
	} else {\
		inspect_printf(dump, " %s "LLU, _name, _val);\
	}

#define DUMP_ATT_U(_name, _val) if (ctx->xml) { \
		inspect_printf(dump, " %s=\"%u\"", _name, _val);\
	} else {\
		inspect_printf(dump, " %s %u", _name, _val);\
	}

#define DUMP_ATT_D(_name, _val) if (ctx->xml) { \
		inspect_printf(dump, " %s=\"%d\"", _name, _val);\
	} else {\
		inspect_printf(dump, " %s %d", _name, _val);\
	}

#define DUMP_ATT_X(_name, _val)  if (ctx->xml) { \
		inspect_printf(dump, " %s=\"0x%08X\"", _name, _val);\
	} else {\
		inspect_printf(dump, " %s 0x%08X", _name, _val);\
	}

#define DUMP_ATT_FRAC(_name, _val)  if (ctx->xml) { \
		inspect_printf(dump, " %s=\"%d/%u\"", _name, _val.num, _val.den);\
	} else {\
		inspect_printf(dump, " %s %d/%u", _name, _val.num, _val.den);\
	}



#ifndef GPAC_DISABLE_AV_PARSERS
static u32 inspect_get_nal_size(char *ptr, u32 nalh_size)
{
	u32 nal_size=0;
	u32 v = nalh_size;
	while (v) {
		nal_size |= (u8) *ptr;
		ptr++;
		v-=1;
		if (v) nal_size <<= 8;
	}
	return nal_size;
}

typedef struct
{
	u32 code;
	const char *name;
} tag_to_name;

static const tag_to_name SEINames[] =
{
	{0, "buffering_period"},
	{1, "pic_timing"},
	{2, "pan_scan_rect"},
	{3, "filler_payload"},
	{4, "itu_t_t35"},
	{5, "user_data_unregistered"},
	{6, "recovery_point"},
	{7, "dec_ref_pic_marking_repetition"},
	{8, "spare_pic"},
	{9, "scene_info"},
	{10, "sub_seq_info"},
	{11, "sub_seq_layer_characteristics"},
	{12, "sub_seq_characteristics"},
	{13, "full_frame_freeze"},
	{14, "full_frame_freeze_release"},
	{15, "picture_snapshot"},
	{16, "progressive_refinement_segment_start"},
	{17, "progressive_refinement_segment_end"},
	{18, "motion_constrained_slice_group_set"},
	{19, "film_grain_characteristics"},
	{20, "deblocking_filter_display_preference"},
	{21, "stereo_video_info"},
	{22, "post_filter_hint"},
	{23, "tone_mapping_info"},
	{24, "scalability_info"},
	{25, "sub_pic_scalable_layer"},
	{26, "non_required_layer_rep"},
	{27, "priority_layer_info"},
	{28, "layers_not_present"},
	{29, "layer_dependency_change"},
	{30, "scalable_nesting"},
	{31, "base_layer_temporal_hrd"},
	{32, "quality_layer_integrity_check"},
	{33, "redundant_pic_property"},
	{34, "tl0_dep_rep_index"},
	{35, "tl_switching_point"},
	{36, "parallel_decoding_info"},
	{37, "mvc_scalable_nesting"},
	{38, "view_scalability_info"},
	{39, "multiview_scene_info"},
	{40, "multiview_acquisition_info"},
	{41, "non_required_view_component"},
	{42, "view_dependency_change"},
	{43, "operation_points_not_present"},
	{44, "base_view_temporal_hrd"},
	{45, "frame_packing_arrangement"},
	{47, "display_orientation"},
	{56, "green_metadata"},
	{128, "structure_of_pictures_info"},
	{129, "active_parameter_sets"},
	{130, "decoding_unit_info"},
	{131, "temporal_sub_layer_zero_index"},
	{132, "decoded_picture_hash"},
	{133, "scalable_nesting"},
	{134, "region_refresh_info"},
	{135, "no_display"},
	{136, "time_code"},
	{137, "mastering_display_colour_volume"},
	{138, "segmented_rect_frame_packing_arrangement"},
	{140, "temporal_motion_constrained_tile_sets"},
	{141, "knee_function_info"},
	{142, "colour_remapping_info"},
	{143, "deinterlaced_field_identification"},
	{144, "content_light_level_info"},
	{145, "dependent_rap_indication"},
	{146, "coded_region_completion"},
	{147, "alternative_transfer_characteristics"},
	{148, "ambient_viewing_environment"},
	{160, "layers_not_present"},
	{161, "inter_layer_constrained_tile_sets"},
	{162, "bsp_nesting"},
	{163, "bsp_initial_arrival_time"},
	{164, "sub_bitstream_property"},
	{165, "alpha_channel_info"},
	{166, "overlay_info"},
	{167, "temporal_mv_prediction_constraints"},
	{168, "frame_field_info"},
	{176, "three_dimensional_reference_displays_info"},
	{177, "depth_representation_info"},
	{178, "multiview_scene_info"},
	{179, "multiview_acquisition_info"},
	{180, "multiview_view_position"},
	{181, "alternative_depth_info"}
};

static const char *get_sei_name(u32 sei_type, u32 is_hevc)
{
	u32 i, count = sizeof(SEINames) / sizeof(tag_to_name);
	for (i=0; i<count; i++) {
		if (SEINames[i].code == sei_type) return SEINames[i].name;
	}
	return "Unknown";
}

static const tag_to_name HEVCNalNames[] =
{
	{GF_HEVC_NALU_SLICE_TRAIL_N, "TRAIL_N slice segment"},
	{GF_HEVC_NALU_SLICE_TRAIL_R, "TRAIL_R slice segment"},
	{GF_HEVC_NALU_SLICE_TSA_N, "TSA_N slice segment"},
	{GF_HEVC_NALU_SLICE_TSA_R, "TSA_R slice segment"},
	{GF_HEVC_NALU_SLICE_STSA_N, "STSA_N slice segment"},
	{GF_HEVC_NALU_SLICE_STSA_R, "STSA_R slice segment"},
	{GF_HEVC_NALU_SLICE_RADL_N, "RADL_N slice segment"},
	{GF_HEVC_NALU_SLICE_RADL_R, "RADL_R slice segment"},
	{GF_HEVC_NALU_SLICE_RASL_N, "RASL_N slice segment"},
	{GF_HEVC_NALU_SLICE_RASL_R, "RASL_R slice segment"},
	{GF_HEVC_NALU_SLICE_BLA_W_LP, "Broken link access slice (W LP)"},
	{GF_HEVC_NALU_SLICE_BLA_W_DLP, "Broken link access slice (W DLP)"},
	{GF_HEVC_NALU_SLICE_BLA_N_LP, "Broken link access slice (N LP)"},
	{GF_HEVC_NALU_SLICE_IDR_W_DLP, "IDR slice (W DLP)"},
	{GF_HEVC_NALU_SLICE_IDR_N_LP, "IDR slice (N LP)"},
	{GF_HEVC_NALU_SLICE_CRA, "CRA slice"},
	{GF_HEVC_NALU_VID_PARAM, "Video Parameter Set"},
	{GF_HEVC_NALU_SEQ_PARAM, "Sequence Parameter Set"},
	{GF_HEVC_NALU_PIC_PARAM, "Picture Parameter Set"},
	{GF_HEVC_NALU_ACCESS_UNIT, "AU Delimiter"},
	{GF_HEVC_NALU_END_OF_SEQ, "End of Sequence"},
	{GF_HEVC_NALU_END_OF_STREAM, "End of Stream"},
	{GF_HEVC_NALU_FILLER_DATA, "Filler Data"},
	{GF_HEVC_NALU_SEI_PREFIX, "SEI Prefix"},
	{GF_HEVC_NALU_SEI_SUFFIX, "SEI Suffix"},
	{GF_HEVC_NALU_FF_AGGREGATOR, "HEVCAggregator"},
	{GF_HEVC_NALU_FF_EXTRACTOR, "HEVCExtractor"},
	{GF_HEVC_NALU_DV_RPU, "UNSPEC_DolbyVision_RPU"},
	{GF_HEVC_NALU_DV_EL, "UNSPEC_DolbyVision_EL"}
};

static const char *get_hevc_nal_name(u32 nal_type)
{
	u32 i, count = sizeof(HEVCNalNames) / sizeof(tag_to_name);
	for (i=0; i<count; i++) {
		if (HEVCNalNames[i].code == nal_type) return HEVCNalNames[i].name;
	}
	return NULL;
}

static const tag_to_name VVCNalNames[] =
{
	{GF_VVC_NALU_SLICE_TRAIL, "Slice_TRAIL"},
	{GF_VVC_NALU_SLICE_STSA, "Slice_STSA"},
	{GF_VVC_NALU_SLICE_RADL, "Slice_RADL"},
	{GF_VVC_NALU_SLICE_RASL, "Slice_RASL"},
	{GF_VVC_NALU_SLICE_IDR_W_RADL, "IDR_RADL"},
	{GF_VVC_NALU_SLICE_IDR_N_LP, "IDR"},
	{GF_VVC_NALU_SLICE_CRA, "CRA"},
	{GF_VVC_NALU_SLICE_GDR, "GDR"},
	{GF_VVC_NALU_OPI, "OperationPointInfo"},
	{GF_VVC_NALU_DEC_PARAM, "DecoderParameterSet"},
	{GF_VVC_NALU_VID_PARAM, "VideoParameterSet"},
	{GF_VVC_NALU_SEQ_PARAM, "SequenceParameterSet"},
	{GF_VVC_NALU_PIC_PARAM, "PictureParameterSet"},
	{GF_VVC_NALU_APS_PREFIX, "AdaptationParameterSet_Prefix"},
	{GF_VVC_NALU_APS_SUFFIX, "AdaptationParameterSet_Suffix"},
	{GF_VVC_NALU_ACCESS_UNIT, "AUDelimiter"},
	{GF_VVC_NALU_END_OF_SEQ, "EOS"},
	{GF_VVC_NALU_END_OF_STREAM, "EOB"},
	{GF_VVC_NALU_FILLER_DATA, "FillerData"},
	{GF_VVC_NALU_SEI_PREFIX, "SEI_Prefix"},
	{GF_VVC_NALU_SEI_SUFFIX, "SEI_Suffix"},
	{GF_VVC_NALU_PIC_HEADER, "PictureHeader"}
};

static const char *get_vvc_nal_name(u32 nal_type)
{
	u32 i, count = sizeof(VVCNalNames) / sizeof(tag_to_name);
	for (i=0; i<count; i++) {
		if (VVCNalNames[i].code == nal_type) return VVCNalNames[i].name;
	}
	return NULL;
}

static const tag_to_name AVCNalNames[] =
{
	{GF_AVC_NALU_NON_IDR_SLICE, "Non IDR slice"},
	{GF_AVC_NALU_DP_A_SLICE, "DP Type A slice"},
	{GF_AVC_NALU_DP_B_SLICE, "DP Type B slice"},
	{GF_AVC_NALU_DP_C_SLICE, "DP Type C slice"},
	{GF_AVC_NALU_IDR_SLICE, "IDR slice"},
	{GF_AVC_NALU_SEI, "SEI Message"},
	{GF_AVC_NALU_SEQ_PARAM, "SequenceParameterSet"},
	{GF_AVC_NALU_PIC_PARAM, "PictureParameterSet"},
	{GF_AVC_NALU_ACCESS_UNIT, "AccessUnit delimiter"},
	{GF_AVC_NALU_END_OF_SEQ, "EndOfSequence"},
	{GF_AVC_NALU_END_OF_STREAM, "EndOfStream"},
	{GF_AVC_NALU_FILLER_DATA, "Filler data"},
	{GF_AVC_NALU_SEQ_PARAM_EXT, "SequenceParameterSetExtension"},
	{GF_AVC_NALU_SVC_PREFIX_NALU, "SVCPrefix"},
	{GF_AVC_NALU_SVC_SUBSEQ_PARAM, "SVCSubsequenceParameterSet"},
	{GF_AVC_NALU_SLICE_AUX, "Auxiliary Slice"},
	{GF_AVC_NALU_SVC_SLICE, "SVCSlice"},
	{GF_AVC_NALU_DV_RPU, "DV_RPU"},
	{GF_AVC_NALU_DV_EL, "DV_EL"},
	{GF_AVC_NALU_FF_AGGREGATOR, "SVCAggregator"},
	{GF_AVC_NALU_FF_EXTRACTOR, "SVCExtractor"}
};

static const char *get_avc_nal_name(u32 nal_type)
{
	u32 i, count = sizeof(AVCNalNames) / sizeof(tag_to_name);
	for (i=0; i<count; i++) {
		if (AVCNalNames[i].code == nal_type) return AVCNalNames[i].name;
	}
	return NULL;
}

typedef struct
{
	FILE *dump;
	Bool dump_bits;
} InspectLogCbk;

static void inspect_log_bs(Bool is_nalu, void *udta, const char *field_name, u32 nb_bits, u64 field_val, s32 idx1, s32 idx2, s32 idx3)
{
	InspectLogCbk *cbk = (InspectLogCbk*)udta;

	if (!nb_bits)
		return;

	if (is_nalu)
		inspect_printf(cbk->dump, "\"");

	inspect_printf(cbk->dump, " %s", field_name);
	if (idx1>=0) {
		inspect_printf(cbk->dump, "_%d", idx1);
		if (idx2>=0) {
			inspect_printf(cbk->dump, "_%d", idx2);
			if (idx3>=0) {
				inspect_printf(cbk->dump, "_%d", idx3);
			}
		}
	}
	inspect_printf(cbk->dump, "=\""LLD, field_val);
	if (cbk->dump_bits && ((s32) nb_bits > 1) )
		inspect_printf(cbk->dump, "(%u)", nb_bits);

	if (!is_nalu)
		inspect_printf(cbk->dump, "\"");
}

static void shifted_bs_log(void *udta, const char *field_name, u32 nb_bits, u64 field_val, s32 idx1, s32 idx2, s32 idx3)
{
	inspect_log_bs(GF_TRUE, udta, field_name, nb_bits, field_val, idx1, idx2, idx3);
}
static void regular_bs_log(void *udta, const char *field_name, u32 nb_bits, u64 field_val, s32 idx1, s32 idx2, s32 idx3)
{
	inspect_log_bs(GF_FALSE, udta, field_name, nb_bits, field_val, idx1, idx2, idx3);
}

static void dump_clli(FILE *dump, GF_BitStream *bs)
{
	u16 max_content_light_level = gf_bs_read_int(bs, 16);
	u16 max_pic_average_light_level = gf_bs_read_int(bs, 16);
	inspect_printf(dump, " max_content_light_level=\"%u\" max_pic_average_light_level=\"%u\"", max_content_light_level, max_pic_average_light_level);
}

static void dump_mdcv(FILE *dump, GF_BitStream *bs, Bool isMPEG)
{
	u8 c;
	u16 display_primaries_x[3];
	u16 display_primaries_y[3];
	u16 white_point_x;
	u16 white_point_y;
	u32 max_display_mastering_luminance;
	u32 min_display_mastering_luminance;
	for(c=0;c<3;c++) {
		display_primaries_x[c] = gf_bs_read_int(bs, 16);
		display_primaries_y[c] = gf_bs_read_int(bs, 16);
	}
	white_point_x = gf_bs_read_int(bs, 16);
	white_point_y = gf_bs_read_int(bs, 16);
	max_display_mastering_luminance = gf_bs_read_int(bs, 32);
	min_display_mastering_luminance = gf_bs_read_int(bs, 32);
	inspect_printf(dump, " display_primaries_x=\"%.04f %.04f %.04f\" display_primaries_y=\"%.04f %.04f %.04f\" white_point_x=\"%.04f\" white_point_y=\"%.04f\" max_display_mastering_luminance=\"%.04f\" min_display_mastering_luminance=\"%.04f\"",
			   display_primaries_x[0]*1.0/(isMPEG?50000:65536),
			   display_primaries_x[1]*1.0/(isMPEG?50000:65536),
			   display_primaries_x[2]*1.0/(isMPEG?50000:65536),
			   display_primaries_y[0]*1.0/(isMPEG?50000:65536),
			   display_primaries_y[1]*1.0/(isMPEG?50000:65536),
			   display_primaries_y[2]*1.0/(isMPEG?50000:65536),
			   white_point_x*1.0/(isMPEG?50000:65536),
			   white_point_y*1.0/(isMPEG?50000:65536),
			   max_display_mastering_luminance*1.0/(isMPEG?10000:256),
			   min_display_mastering_luminance*1.0/(isMPEG?10000:(1<<14)));
}

static u32 dump_t35(FILE *dump, GF_BitStream *bs)
{
	u32 read_bytes = 1;
	u32 country_code = gf_bs_read_u8(bs);
	inspect_printf(dump, " country_code=\"0x%x\"", country_code);
	if (country_code == 0xFF) {
		u32 country_code_extension = gf_bs_read_u8(bs);
		read_bytes++;
		inspect_printf(dump, " country_code_extension=\"0x%x\"", country_code_extension);
	}
	if (country_code == 0xB5) { // USA
		u32 terminal_provider_code = gf_bs_read_u16(bs);
		u32 terminal_provider_oriented_code = gf_bs_read_u16(bs);
		u32 application_identifier = gf_bs_read_u8(bs);
		u32 application_mode = gf_bs_read_u8(bs);
		read_bytes+=6;
		inspect_printf(dump, " terminal_provider_code=\"0x%x\" terminal_provider_oriented_code=\"0x%x\" application_identifier=\"%u\" application_mode=\"%u\"",
				   terminal_provider_code, terminal_provider_oriented_code,
				   application_identifier, application_mode);
	}
	return read_bytes;
}

static void dump_sei(FILE *dump, GF_BitStream *bs, Bool is_hevc)
{
	u32 i;
	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	//skip nal header
	gf_bs_read_int(bs, is_hevc ? 16 : 8);

	while (gf_bs_available(bs) ) {
		u32 sei_type = 0;
		u32 sei_size = 0;
		while (gf_bs_peek_bits(bs, 8, 0) == 0xFF) {
			sei_type += 255;
			gf_bs_read_int(bs, 8);
		}
		sei_type += gf_bs_read_int(bs, 8);
		while (gf_bs_peek_bits(bs, 8, 0) == 0xFF) {
			sei_size += 255;
			gf_bs_read_int(bs, 8);
		}
		sei_size += gf_bs_read_int(bs, 8);

		inspect_printf(dump, "    <SEIMessage ptype=\"%u\" psize=\"%u\" type=\"%s\"", sei_type, sei_size, get_sei_name(sei_type, is_hevc) );
		if (sei_type == 144) {
			dump_clli(dump, bs);
		} else if (sei_type == 137) {
			dump_mdcv(dump, bs, GF_TRUE);
		} else if (sei_type == 4) {
			i = dump_t35(dump, bs);
			while (i < sei_size) {
				gf_bs_read_u8(bs);
				i++;
			}
		} else {
			i=0;
			while (i < sei_size) {
				gf_bs_read_u8(bs);
				i++;
			}
		}
		inspect_printf(dump, "/>\n");
		if (gf_bs_peek_bits(bs, 8, 0) == 0x80) {
			break;
		}
	}
}


static void gf_inspect_dump_nalu_internal(FILE *dump, u8 *ptr, u32 ptr_size, Bool is_svc, HEVCState *hevc, AVCState *avc, VVCState *vvc, u32 nalh_size, Bool dump_crc, Bool is_encrypted, u32 full_bs_dump, PidCtx *pctx)
{
	s32 res = 0;
	u8 type, nal_ref_idc;
	u8 dependency_id, quality_id, temporal_id;
	u8 track_ref_index;
	s8 sample_offset;
	u32 data_offset, data_size;
	s32 idx;
	InspectLogCbk lcbk;
	GF_BitStream *bs = NULL;
	const char *nal_name;

	if (full_bs_dump<INSPECT_ANALYZE_BS)
		full_bs_dump = 0;
	else {
		lcbk.dump = dump;
		lcbk.dump_bits = full_bs_dump==INSPECT_ANALYZE_BS_BITS ? GF_TRUE : GF_FALSE;
	}

	if (!ptr_size) {
		inspect_printf(dump, "error=\"invalid nal size 0\"/>\n");
		return;
	}

	if (dump_crc) inspect_printf(dump, "crc=\"%u\" ", gf_crc_32(ptr, ptr_size) );

	if (hevc) {
#ifndef GPAC_DISABLE_HEVC

		if (ptr_size<=1) {
			inspect_printf(dump, "error=\"invalid nal size 1\"/>\n");
			return;
		}

		if (full_bs_dump) {
			if (pctx) {
				if (!pctx->bs)
					pctx->bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
				else
					gf_bs_reassign_buffer(pctx->bs, ptr, ptr_size);
				bs = pctx->bs;
			} else {
				bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
			}
			gf_bs_set_logger(bs, regular_bs_log, &lcbk);
			res = gf_hevc_parse_nalu_bs(bs, hevc, &type, &temporal_id, &quality_id);
		} else {
			bs = NULL;
			res = gf_hevc_parse_nalu(ptr, ptr_size, hevc, &type, &temporal_id, &quality_id);
			inspect_printf(dump, "code=\"%d\"", type);
		}

		if (res==-1) {
			inspect_printf(dump, " status=\"error parsing\"", type);
		}
		inspect_printf(dump, " type=\"", type);
		nal_name = get_hevc_nal_name(type);
		if (nal_name)
			gf_fputs(nal_name, dump);
		else
			inspect_printf(dump, "UNKNOWN (parsing return %d)", res);

		//specific dump
		switch (type) {
		case GF_HEVC_NALU_VID_PARAM:
			if (full_bs_dump) break;
			idx = gf_hevc_read_vps(ptr, ptr_size, hevc);
			if (idx<0) inspect_printf(dump, "\" vps_id=\"PARSING FAILURE");
			else inspect_printf(dump, "\" vps_id=\"%d", idx);
			break;
		case GF_HEVC_NALU_SEQ_PARAM:
			if (full_bs_dump) break;
			idx = gf_hevc_read_sps(ptr, ptr_size, hevc);
			if (idx<0) {
				inspect_printf(dump, "\" sps_id=\"PARSING FAILURE");
				break;
			}
			{
			HEVC_SPS *sps= &hevc->sps[idx];
			inspect_printf(dump, "\" sps_id=\"%d", idx);

			inspect_printf(dump, "\" aspect_ratio_info_present_flag=\"%d", sps->aspect_ratio_info_present_flag);
			inspect_printf(dump, "\" bit_depth_chroma=\"%d", sps->bit_depth_chroma);
			inspect_printf(dump, "\" bit_depth_luma=\"%d", sps->bit_depth_luma);
			inspect_printf(dump, "\" chroma_format_idc=\"%d", sps->chroma_format_idc);
			inspect_printf(dump, "\" colour_description_present_flag=\"%d", sps->colour_description_present_flag);
			inspect_printf(dump, "\" colour_primaries=\"%d", sps->colour_primaries);
			inspect_printf(dump, "\" cw_flag=\"%d", sps->cw_flag);
			if (sps->cw_flag) {
				inspect_printf(dump, "\" cw_bottom=\"%d", sps->cw_bottom);
				inspect_printf(dump, "\" cw_top=\"%d", sps->cw_top);
				inspect_printf(dump, "\" cw_left=\"%d", sps->cw_left);
				inspect_printf(dump, "\" cw_right=\"%d", sps->cw_right);
			}
			inspect_printf(dump, "\" height=\"%d", sps->height);
			inspect_printf(dump, "\" width=\"%d", sps->width);
			inspect_printf(dump, "\" log2_max_pic_order_cnt_lsb=\"%d", sps->log2_max_pic_order_cnt_lsb);
			inspect_printf(dump, "\" long_term_ref_pics_present_flag=\"%d", sps->long_term_ref_pics_present_flag);
			inspect_printf(dump, "\" matrix_coeffs=\"%d", sps->matrix_coeffs);
			inspect_printf(dump, "\" max_CU_depth=\"%d", sps->max_CU_depth);
			inspect_printf(dump, "\" max_CU_width=\"%d", sps->max_CU_width);
			inspect_printf(dump, "\" max_CU_height=\"%d", sps->max_CU_height);
			inspect_printf(dump, "\" num_long_term_ref_pic_sps=\"%d", sps->num_long_term_ref_pic_sps);
			inspect_printf(dump, "\" num_short_term_ref_pic_sets=\"%d", sps->num_short_term_ref_pic_sets);
			inspect_printf(dump, "\" has_timing_info=\"%d", sps->has_timing_info);
			if (sps->has_timing_info) {
				inspect_printf(dump, "\" time_scale=\"%d", sps->time_scale);
				inspect_printf(dump, "\" num_ticks_poc_diff_one_minus1=\"%d", sps->num_ticks_poc_diff_one_minus1);
				inspect_printf(dump, "\" num_units_in_tick=\"%d", sps->num_units_in_tick);
				inspect_printf(dump, "\" poc_proportional_to_timing_flag=\"%d", sps->poc_proportional_to_timing_flag);
			}
			inspect_printf(dump, "\" rep_format_idx=\"%d", sps->rep_format_idx);
			inspect_printf(dump, "\" sample_adaptive_offset_enabled_flag=\"%d", sps->sample_adaptive_offset_enabled_flag);
			inspect_printf(dump, "\" sar_idc=\"%d", sps->sar_idc);
			inspect_printf(dump, "\" separate_colour_plane_flag=\"%d", sps->separate_colour_plane_flag);
			inspect_printf(dump, "\" temporal_mvp_enable_flag=\"%d", sps->temporal_mvp_enable_flag);
			inspect_printf(dump, "\" transfer_characteristic=\"%d", sps->transfer_characteristic);
			inspect_printf(dump, "\" video_full_range_flag=\"%d", sps->video_full_range_flag);
			inspect_printf(dump, "\" sps_ext_or_max_sub_layers_minus1=\"%d", sps->sps_ext_or_max_sub_layers_minus1);
			inspect_printf(dump, "\" max_sub_layers_minus1=\"%d", sps->max_sub_layers_minus1);
			inspect_printf(dump, "\" update_rep_format_flag=\"%d", sps->update_rep_format_flag);
			inspect_printf(dump, "\" sub_layer_ordering_info_present_flag=\"%d", sps->sub_layer_ordering_info_present_flag);
			inspect_printf(dump, "\" scaling_list_enable_flag=\"%d", sps->scaling_list_enable_flag);
			inspect_printf(dump, "\" infer_scaling_list_flag=\"%d", sps->infer_scaling_list_flag);
			inspect_printf(dump, "\" scaling_list_ref_layer_id=\"%d", sps->scaling_list_ref_layer_id);
			inspect_printf(dump, "\" scaling_list_data_present_flag=\"%d", sps->scaling_list_data_present_flag);
			inspect_printf(dump, "\" asymmetric_motion_partitions_enabled_flag=\"%d", sps->asymmetric_motion_partitions_enabled_flag);
			inspect_printf(dump, "\" pcm_enabled_flag=\"%d", sps->pcm_enabled_flag);
			inspect_printf(dump, "\" strong_intra_smoothing_enable_flag=\"%d", sps->strong_intra_smoothing_enable_flag);
			inspect_printf(dump, "\" vui_parameters_present_flag=\"%d", sps->vui_parameters_present_flag);
			inspect_printf(dump, "\" log2_diff_max_min_luma_coding_block_size=\"%d", sps->log2_diff_max_min_luma_coding_block_size);
			inspect_printf(dump, "\" log2_min_transform_block_size=\"%d", sps->log2_min_transform_block_size);
			inspect_printf(dump, "\" log2_min_luma_coding_block_size=\"%d", sps->log2_min_luma_coding_block_size);
			inspect_printf(dump, "\" log2_max_transform_block_size=\"%d", sps->log2_max_transform_block_size);
			inspect_printf(dump, "\" max_transform_hierarchy_depth_inter=\"%d", sps->max_transform_hierarchy_depth_inter);
			inspect_printf(dump, "\" max_transform_hierarchy_depth_intra=\"%d", sps->max_transform_hierarchy_depth_intra);
			inspect_printf(dump, "\" pcm_sample_bit_depth_luma_minus1=\"%d", sps->pcm_sample_bit_depth_luma_minus1);
			inspect_printf(dump, "\" pcm_sample_bit_depth_chroma_minus1=\"%d", sps->pcm_sample_bit_depth_chroma_minus1);
			inspect_printf(dump, "\" pcm_loop_filter_disable_flag=\"%d", sps->pcm_loop_filter_disable_flag);
			inspect_printf(dump, "\" log2_min_pcm_luma_coding_block_size_minus3=\"%d", sps->log2_min_pcm_luma_coding_block_size_minus3);
			inspect_printf(dump, "\" log2_diff_max_min_pcm_luma_coding_block_size=\"%d", sps->log2_diff_max_min_pcm_luma_coding_block_size);
			inspect_printf(dump, "\" overscan_info_present=\"%d", sps->overscan_info_present);
			inspect_printf(dump, "\" overscan_appropriate=\"%d", sps->overscan_appropriate);
			inspect_printf(dump, "\" video_signal_type_present_flag=\"%d", sps->video_signal_type_present_flag);
			inspect_printf(dump, "\" video_format=\"%d", sps->video_format);
			inspect_printf(dump, "\" chroma_loc_info_present_flag=\"%d", sps->chroma_loc_info_present_flag);
			inspect_printf(dump, "\" chroma_sample_loc_type_top_field=\"%d", sps->chroma_sample_loc_type_top_field);
			inspect_printf(dump, "\" chroma_sample_loc_type_bottom_field=\"%d", sps->chroma_sample_loc_type_bottom_field);
			inspect_printf(dump, "\" neutral_chroma_indication_flag=\"%d", sps->neutral_chroma_indication_flag);
			inspect_printf(dump, "\" field_seq_flag=\"%d", sps->field_seq_flag);
			inspect_printf(dump, "\" frame_field_info_present_flag=\"%d", sps->frame_field_info_present_flag);
			inspect_printf(dump, "\" default_display_window_flag=\"%d", sps->default_display_window_flag);
			inspect_printf(dump, "\" left_offset=\"%d", sps->left_offset);
			inspect_printf(dump, "\" right_offset=\"%d", sps->right_offset);
			inspect_printf(dump, "\" top_offset=\"%d", sps->top_offset);
			inspect_printf(dump, "\" bottom_offset=\"%d", sps->bottom_offset);
			inspect_printf(dump, "\" hrd_parameters_present_flag=\"%d", sps->hrd_parameters_present_flag);
			}
			break;
		case GF_HEVC_NALU_PIC_PARAM:
			if (full_bs_dump) break;
			idx = gf_hevc_read_pps(ptr, ptr_size, hevc);
			if (idx<0) {
				inspect_printf(dump, "\" pps_id=\"PARSING FAILURE");
				break;
			}
			{
			HEVC_PPS *pps= &hevc->pps[idx];
			inspect_printf(dump, "\" pps_id=\"%d", idx);

			inspect_printf(dump, "\" cabac_init_present_flag=\"%d", pps->cabac_init_present_flag);
			inspect_printf(dump, "\" dependent_slice_segments_enabled_flag=\"%d", pps->dependent_slice_segments_enabled_flag);
			inspect_printf(dump, "\" entropy_coding_sync_enabled_flag=\"%d", pps->entropy_coding_sync_enabled_flag);
			inspect_printf(dump, "\" lists_modification_present_flag=\"%d", pps->lists_modification_present_flag);
			inspect_printf(dump, "\" loop_filter_across_slices_enabled_flag=\"%d", pps->loop_filter_across_slices_enabled_flag);
			inspect_printf(dump, "\" loop_filter_across_tiles_enabled_flag=\"%d", pps->loop_filter_across_tiles_enabled_flag);
			inspect_printf(dump, "\" num_extra_slice_header_bits=\"%d", pps->num_extra_slice_header_bits);
			inspect_printf(dump, "\" num_ref_idx_l0_default_active=\"%d", pps->num_ref_idx_l0_default_active);
			inspect_printf(dump, "\" num_ref_idx_l1_default_active=\"%d", pps->num_ref_idx_l1_default_active);
			inspect_printf(dump, "\" tiles_enabled_flag=\"%d", pps->tiles_enabled_flag);
			if (pps->tiles_enabled_flag) {
				inspect_printf(dump, "\" uniform_spacing_flag=\"%d", pps->uniform_spacing_flag);
				if (!pps->uniform_spacing_flag) {
					u32 k;
					inspect_printf(dump, "\" num_tile_columns=\"%d", pps->num_tile_columns);
					inspect_printf(dump, "\" num_tile_rows=\"%d", pps->num_tile_rows);
					inspect_printf(dump, "\" colomns_width=\"");
					for (k=0; k<pps->num_tile_columns-1; k++)
						inspect_printf(dump, "%d ", pps->column_width[k]);
					inspect_printf(dump, "\" rows_height=\"");
					for (k=0; k<pps->num_tile_rows-1; k++)
						inspect_printf(dump, "%d ", pps->row_height[k]);
				}
			}
			inspect_printf(dump, "\" output_flag_present_flag=\"%d", pps->output_flag_present_flag);
			inspect_printf(dump, "\" pic_init_qp_minus26=\"%d", pps->pic_init_qp_minus26);
			inspect_printf(dump, "\" slice_chroma_qp_offsets_present_flag=\"%d", pps->slice_chroma_qp_offsets_present_flag);
			inspect_printf(dump, "\" slice_segment_header_extension_present_flag=\"%d", pps->slice_segment_header_extension_present_flag);
			inspect_printf(dump, "\" weighted_pred_flag=\"%d", pps->weighted_pred_flag);
			inspect_printf(dump, "\" weighted_bipred_flag=\"%d", pps->weighted_bipred_flag);

			inspect_printf(dump, "\" sign_data_hiding_flag=\"%d", pps->sign_data_hiding_flag);
			inspect_printf(dump, "\" constrained_intra_pred_flag=\"%d", pps->constrained_intra_pred_flag);
			inspect_printf(dump, "\" transform_skip_enabled_flag=\"%d", pps->transform_skip_enabled_flag);
			inspect_printf(dump, "\" cu_qp_delta_enabled_flag=\"%d", pps->cu_qp_delta_enabled_flag);
			if (pps->cu_qp_delta_enabled_flag)
				inspect_printf(dump, "\" diff_cu_qp_delta_depth=\"%d", pps->diff_cu_qp_delta_depth);
			inspect_printf(dump, "\" transquant_bypass_enable_flag=\"%d", pps->transquant_bypass_enable_flag);
			inspect_printf(dump, "\" pic_cb_qp_offset=\"%d", pps->pic_cb_qp_offset);
			inspect_printf(dump, "\" pic_cr_qp_offset=\"%d", pps->pic_cr_qp_offset);

			inspect_printf(dump, "\" deblocking_filter_control_present_flag=\"%d", pps->deblocking_filter_control_present_flag);
			if (pps->deblocking_filter_control_present_flag) {
				inspect_printf(dump, "\" deblocking_filter_override_enabled_flag=\"%d", pps->deblocking_filter_override_enabled_flag);
				inspect_printf(dump, "\" pic_disable_deblocking_filter_flag=\"%d", pps->pic_disable_deblocking_filter_flag);
				inspect_printf(dump, "\" beta_offset_div2=\"%d", pps->beta_offset_div2);
				inspect_printf(dump, "\" tc_offset_div2=\"%d", pps->tc_offset_div2);
			}
			inspect_printf(dump, "\" pic_scaling_list_data_present_flag=\"%d", pps->pic_scaling_list_data_present_flag);
			inspect_printf(dump, "\" log2_parallel_merge_level_minus2=\"%d", pps->log2_parallel_merge_level_minus2);
			}
			break;
		case GF_HEVC_NALU_ACCESS_UNIT:
			if (ptr_size<3) {
				inspect_printf(dump, "\" status=\"CORRUPTED NAL");
				break;
			}
			inspect_printf(dump, "\" primary_pic_type=\"%d", ptr[2] >> 5);
			break;
		//extractor
		case GF_HEVC_NALU_FF_EXTRACTOR:
		{
			u32 remain = ptr_size-2;
			char *s = ptr+2;

			gf_fputs(" ", dump);

			while (remain) {
				u32 mode = s[0];
				remain -= 1;
				s += 1;
				if (mode) {
					u32 len = s[0];
					if (len+1>remain) {
						inspect_printf(dump, "error=\"invalid inband data extractor size: %d vs %d remaining\"/>\n", len, remain);
						return;
					}
					remain -= len+1;
					s += len+1;
					inspect_printf(dump, "\" inband_size=\"%d", len);
				} else {
					if (remain < 2 + 2*nalh_size) {
						inspect_printf(dump, "error=\"invalid ref data extractor size: %d vs %d remaining\"/>\n", 2 + 2*nalh_size, remain);
						return;
					}
					track_ref_index = (u8) s[0];
					sample_offset = (s8) s[1];
					data_offset = inspect_get_nal_size(&s[2], nalh_size);
					data_size = inspect_get_nal_size(&s[2+nalh_size], nalh_size);
					inspect_printf(dump, "\" track_ref_index=\"%d\" sample_offset=\"%d\" data_offset=\"%d\" data_size=\"%d", track_ref_index, sample_offset, data_offset, data_size);

					remain -= 2 + 2*nalh_size;
					s += 2 + 2*nalh_size;
				}
			}
		}
			break;
		default:
			break;
		}
		gf_fputs("\"", dump);

		if (!full_bs_dump && (type < GF_HEVC_NALU_VID_PARAM)) {
			inspect_printf(dump, " slice=\"%s\" poc=\"%d\"", (hevc->s_info.slice_type==GF_HEVC_SLICE_TYPE_I) ? "I" : (hevc->s_info.slice_type==GF_HEVC_SLICE_TYPE_P) ? "P" : (hevc->s_info.slice_type==GF_HEVC_SLICE_TYPE_B) ? "B" : "Unknown", hevc->s_info.poc);
			inspect_printf(dump, " first_slice_in_pic=\"%d\"", hevc->s_info.first_slice_segment_in_pic_flag);
			inspect_printf(dump, " dependent_slice_segment=\"%d\"", hevc->s_info.dependent_slice_segment_flag);

			if (!gf_sys_is_test_mode()) {
				inspect_printf(dump, " redundant_pic_cnt=\"%d\"", hevc->s_info.redundant_pic_cnt);
				inspect_printf(dump, " slice_qp_delta=\"%d\"", hevc->s_info.slice_qp_delta);
				inspect_printf(dump, " slice_segment_address=\"%d\"", hevc->s_info.slice_segment_address);
				inspect_printf(dump, " slice_type=\"%d\"", hevc->s_info.slice_type);
			}
		}
		if (!full_bs_dump)
			inspect_printf(dump, " layer_id=\"%d\" temporal_id=\"%d\"", quality_id, temporal_id);

		if (bs) {
			if (!pctx)
				gf_bs_del(bs);
			else
				gf_bs_set_logger(bs, NULL, NULL);
		}

		if ((type == GF_HEVC_NALU_SEI_PREFIX) || (type == GF_HEVC_NALU_SEI_SUFFIX)) {
			inspect_printf(dump, ">\n");
			if (pctx) {
				if (!pctx->bs)
					pctx->bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
				else
					gf_bs_reassign_buffer(pctx->bs, ptr, ptr_size);
				bs = pctx->bs;
			} else {
				bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
			}
			dump_sei(dump, bs, GF_TRUE);
			if (!pctx) gf_bs_del(bs);
			inspect_printf(dump, "   </NALU>\n");
		} else {
			inspect_printf(dump, "/>\n");
		}

#else
		inspect_printf(dump, "/>\n");
#endif //GPAC_DISABLE_HEVC
		return;
	}

	if (vvc) {
		u8 lid, tid;

		if (ptr_size<=1) {
			inspect_printf(dump, "error=\"invalid nal size 1\"/>\n");
			return;
		}

		if (full_bs_dump) {
			vvc->parse_mode = 2;
			if (pctx) {
				if (!pctx->bs)
					pctx->bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
				else
					gf_bs_reassign_buffer(pctx->bs, ptr, ptr_size);
				bs = pctx->bs;
			} else {
				bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
			}
			gf_bs_set_logger(bs, regular_bs_log, &lcbk);
			res = gf_vvc_parse_nalu_bs(bs, vvc, &type, &lid, &tid);
		} else {
			vvc->parse_mode = 0;
			bs = NULL;
			u32 forb_zero = (ptr[0] & 0x80) ? 1 : 0;
			u32 res_zero = (ptr[0] & 0x40) ? 1 : 0;
			lid = (ptr[0] & 0x3F);
			tid = (ptr[1] & 0x7);
			if (forb_zero || res_zero || !tid) {
				inspect_printf(dump, "error=\"invalid header (forb %d res_zero %d tid %d)\"/>\n", forb_zero, res_zero, tid);
				return;
			}
			tid -= 1;
			type = ptr[1]>>3;

			res = gf_vvc_parse_nalu(ptr, ptr_size, vvc, &type, &lid, &tid);
			inspect_printf(dump, "code=\"%d\" temporalid=\"%d\" layerid=\"%d\"", type, tid, lid);
		}
		if (res==-1) {
			inspect_printf(dump, " status=\"error parsing\"", type);
		}

		inspect_printf(dump, " type=\"");
		nal_name = get_vvc_nal_name(type);
		if (nal_name)
			inspect_printf(dump, nal_name);
		else {
			inspect_printf(dump, "unknown");
			res = -2;
		}
		//specific dump
		switch (type) {
		case GF_VVC_NALU_VID_PARAM:
			if ((res>=0) && !full_bs_dump) {
				u32 j;
				VVC_VPS *vps = &vvc->vps[vvc->last_parsed_vps_id];
				inspect_printf(dump, "\" id=\"%d\" num_ptl=\"%d\" max_layers=\"%d\" max_sublayers=\"%d", vps->id, vps->num_ptl, vps->max_layers, vps->max_sub_layers);
				if (vps->max_layers>1) {
					inspect_printf(dump, "\" max_layer_id=\"%d\" all_layers_independent=\"%d\" each_layer_is_ols=\"%d", vps->max_layer_id, vps->all_layers_independent, vps->each_layer_is_ols);
				}
				for (j=0; j<vps->num_ptl; j++) {
					VVC_ProfileTierLevel *ptl = &vps->ptl[j];
					inspect_printf(dump, "\" general_level_idc=\"%d\" frame_only_constraint=\"%d\" multilayer_enabled=\"%d\" max_tid=\"%d", ptl->general_level_idc, ptl->frame_only_constraint, ptl->multilayer_enabled, ptl->ptl_max_tid);

					if (ptl->pt_present) {
						inspect_printf(dump, "\" general_profile_idc=\"%d\" general_tier_flag=\"%d\" gci_present=\"%d", ptl->general_profile_idc, ptl->general_tier_flag, ptl->gci_present);
					}
				}
			}
			res = -2;
			break;
		case GF_VVC_NALU_SEQ_PARAM:
			if ((res>=0) && !full_bs_dump) {
				VVC_SPS *sps = &vvc->sps[vvc->last_parsed_sps_id];

				inspect_printf(dump, "\" id=\"%d\" vps_id=\"%d\" max_sublayers=\"%d\" chroma_idc=\"%d\" bit_depth=\"%d\" CTBsizeY=\"%d\" gdr_enabled=\"%d\" ref_pic_sampling=\"%d\" subpic_info_present=\"%d\" poc_msb_cycle_flag=\"%d", sps->id, sps->vps_id, sps->max_sublayers, sps->chroma_format_idc, sps->bitdepth, 1<<sps->log2_ctu_size, sps->gdr_enabled, sps->ref_pic_resampling, sps->subpic_info_present, sps->poc_msb_cycle_flag);
				if (sps->ref_pic_resampling) {
					inspect_printf(dump, "\" res_change_in_clvs=\"%d", sps->res_change_in_clvs);
				}
				inspect_printf(dump, "\" width=\"%d\" height=\"%d", sps->width, sps->height);
				if (!sps->vps_id) {
					VVC_ProfileTierLevel *ptl = &vvc->vps[0].ptl[0];
					inspect_printf(dump, "\" general_level_idc=\"%d\" frame_only_constraint=\"%d\" multilayer_enabled=\"%d\" max_tid=\"%d", ptl->general_level_idc, ptl->frame_only_constraint, ptl->multilayer_enabled, ptl->ptl_max_tid);

					if (ptl->pt_present) {
						inspect_printf(dump, "\" general_profile_idc=\"%d\" general_tier_flag=\"%d\" gci_present=\"%d", ptl->general_profile_idc, ptl->general_tier_flag, ptl->gci_present);
					}
				}
				inspect_printf(dump, "\" conf_window=\"%d", sps->conf_window);
				if (sps->conf_window) {
					inspect_printf(dump, "\" cw_left=\"%d\" cw_right=\"%d\" cw_top=\"%d\" cw_bottom=\"%d", sps->cw_left, sps->cw_right, sps->cw_top, sps->cw_bottom);
				}
			}
			res=-2;
			break;
		case GF_VVC_NALU_PIC_PARAM:
			if ((res>=0) && !full_bs_dump) {
				VVC_PPS *pps = &vvc->pps[vvc->last_parsed_pps_id];
				inspect_printf(dump, "\" id=\"%d\" sps_id=\"%d\" width=\"%d\" height=\"%d\" mixed_nal_types=\"%d\" conf_window=\"%d", pps->id, pps->sps_id, pps->width, pps->height, pps->mixed_nal_types, pps->conf_window);

				if (pps->conf_window) {
					inspect_printf(dump, "\" cw_left=\"%d\" cw_right=\"%d\" cw_top=\"%d\" cw_bottom=\"%d", pps->cw_left, pps->cw_right, pps->cw_top, pps->cw_bottom);
				}
				inspect_printf(dump, "\" output_flag_present_flag=\"%d\" no_pic_partition_flag=\"%d\" subpic_id_mapping_present_flag=\"%d", pps->output_flag_present_flag, pps->no_pic_partition_flag, pps->subpic_id_mapping_present_flag);
			}
			res=-2;
			break;
		default:
			break;
		}
		inspect_printf(dump, "\"");

		//picture header or slice
		if ((type!=GF_VVC_NALU_PIC_HEADER) && (type>GF_VVC_NALU_SLICE_GDR))
			res = -2;
		if ((res>=0) && !full_bs_dump) {
			if (type!=GF_VVC_NALU_PIC_HEADER)
				inspect_printf(dump, " picture_header_in_slice_header_flag=\"%d\"", vvc->s_info.picture_header_in_slice_header_flag);

			if ((type==GF_VVC_NALU_PIC_HEADER) || vvc->s_info.picture_header_in_slice_header_flag) {
				inspect_printf(dump, " pps_id=\"%d\" poc=\"%d\" irap_or_gdr_pic=\"%d\" non_ref_pic=\"%d\" inter_slice_allowed_flag=\"%d\" poc_lsb=\"%d\"", vvc->s_info.pps->id, vvc->s_info.poc, vvc->s_info.irap_or_gdr_pic, vvc->s_info.non_ref_pic, vvc->s_info.inter_slice_allowed_flag, vvc->s_info.poc_lsb);
				if (vvc->s_info.irap_or_gdr_pic)
					inspect_printf(dump, " gdr_pic=\"%d\" gdr_recovery_count=\"%d\"", vvc->s_info.gdr_pic, vvc->s_info.gdr_recovery_count);
				if (vvc->s_info.inter_slice_allowed_flag)
					inspect_printf(dump, " intra_slice_allowed_flag=\"%d\"", vvc->s_info.intra_slice_allowed_flag);
				if (vvc->s_info.sps->poc_msb_cycle_flag && vvc->s_info.poc_msb_cycle_present_flag)
					inspect_printf(dump, " poc_msb_cycle=\"%d\"", vvc->s_info.poc_msb_cycle);
			}
			if (type!=GF_VVC_NALU_PIC_HEADER)
				inspect_printf(dump, " slice_type=\"%d\"", vvc->s_info.slice_type);
		}

		if (bs) {
			if (!pctx)
				gf_bs_del(bs);
			else
				gf_bs_set_logger(bs, NULL, NULL);
		}

		if ((type == GF_VVC_NALU_SEI_PREFIX) || (type == GF_VVC_NALU_SEI_SUFFIX)) {
			inspect_printf(dump, ">\n");
			if (pctx) {
				if (!pctx->bs)
					pctx->bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
				else
					gf_bs_reassign_buffer(pctx->bs, ptr, ptr_size);
				bs = pctx->bs;
			} else {
				bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
			}
			dump_sei(dump, bs, GF_TRUE);
			if (!pctx) gf_bs_del(bs);
			inspect_printf(dump, "   </NALU>\n");
		} else {
			inspect_printf(dump, "/>\n");
		}
		return;
	}

	//avc
	if (!ptr_size) {
		inspect_printf(dump, "error=\"invalid nal size 1\"/>\n");
		return;
	}
	type = ptr[0] & 0x1F;
	nal_ref_idc = ptr[0] & 0x60;
	nal_ref_idc>>=5;
	if (! full_bs_dump)
		inspect_printf(dump, "code=\"%d\" ", type);

	inspect_printf(dump, "type=\"");
	res = -2;
	nal_name = get_avc_nal_name(type);
	if (type == GF_AVC_NALU_SVC_SLICE) nal_name = is_svc ? "SVCSlice" : "CodedSliceExtension";
	if (nal_name) {
		gf_fputs(nal_name, dump);
	} else {
		gf_fputs("unknown", dump);
	}

	if (pctx) {
		if (!pctx->bs)
			pctx->bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
		else
			gf_bs_reassign_buffer(pctx->bs, ptr, ptr_size);
		bs = pctx->bs;
	} else {
		bs = gf_bs_new(ptr, ptr_size, GF_BITSTREAM_READ);
	}

	if (full_bs_dump)
		gf_bs_set_logger(bs, shifted_bs_log, &lcbk);

	//specific dump
	switch (type) {
	case GF_AVC_NALU_NON_IDR_SLICE:
	case GF_AVC_NALU_DP_A_SLICE:
	case GF_AVC_NALU_DP_B_SLICE:
	case GF_AVC_NALU_DP_C_SLICE:
	case GF_AVC_NALU_IDR_SLICE:
		if (is_encrypted) break;
		res = gf_avc_parse_nalu(bs, avc);
		break;
	case GF_AVC_NALU_SEQ_PARAM:
		if (is_encrypted) break;
		idx = gf_avc_read_sps_bs(bs, avc, 0, NULL);
		if (idx<0) {
			inspect_printf(dump, "\" sps_id=\"PARSING FAILURE");
			break;
		}
		if (full_bs_dump) break;
		inspect_printf(dump, "\" sps_id=\"%d", idx);
		inspect_printf(dump, "\" frame_mbs_only_flag=\"%d", avc->sps->frame_mbs_only_flag);
		inspect_printf(dump, "\" mb_adaptive_frame_field_flag=\"%d", avc->sps->mb_adaptive_frame_field_flag);
		inspect_printf(dump, "\" vui_parameters_present_flag=\"%d", avc->sps->vui_parameters_present_flag);
		inspect_printf(dump, "\" max_num_ref_frames=\"%d", avc->sps->max_num_ref_frames);
		inspect_printf(dump, "\" gaps_in_frame_num_value_allowed_flag=\"%d", avc->sps->gaps_in_frame_num_value_allowed_flag);
		inspect_printf(dump, "\" chroma_format_idc=\"%d", avc->sps->chroma_format);
		inspect_printf(dump, "\" bit_depth_luma_minus8=\"%d", avc->sps->luma_bit_depth_m8);
		inspect_printf(dump, "\" bit_depth_chroma_minus8=\"%d", avc->sps->chroma_bit_depth_m8);
		inspect_printf(dump, "\" width=\"%d", avc->sps->width);
		inspect_printf(dump, "\" height=\"%d", avc->sps->height);
		inspect_printf(dump, "\" crop_top=\"%d", avc->sps->crop.top);
		inspect_printf(dump, "\" crop_left=\"%d", avc->sps->crop.left);
		inspect_printf(dump, "\" crop_bottom=\"%d", avc->sps->crop.bottom);
		inspect_printf(dump, "\" crop_right=\"%d", avc->sps->crop.right);
		if (avc->sps->vui_parameters_present_flag) {
			inspect_printf(dump, "\" vui_video_full_range_flag=\"%d", avc->sps->vui.video_full_range_flag);
			inspect_printf(dump, "\" vui_video_signal_type_present_flag=\"%d", avc->sps->vui.video_signal_type_present_flag);
			inspect_printf(dump, "\" vui_aspect_ratio_info_present_flag=\"%d", avc->sps->vui.aspect_ratio_info_present_flag);
			inspect_printf(dump, "\" vui_aspect_ratio_num=\"%d", avc->sps->vui.par_num);
			inspect_printf(dump, "\" vui_aspect_ratio_den=\"%d", avc->sps->vui.par_den);
			inspect_printf(dump, "\" vui_overscan_info_present_flag=\"%d", avc->sps->vui.overscan_info_present_flag);
			inspect_printf(dump, "\" vui_colour_description_present_flag=\"%d", avc->sps->vui.colour_description_present_flag);
			inspect_printf(dump, "\" vui_colour_primaries=\"%d", avc->sps->vui.colour_primaries);
			inspect_printf(dump, "\" vui_transfer_characteristics=\"%d", avc->sps->vui.transfer_characteristics);
			inspect_printf(dump, "\" vui_matrix_coefficients=\"%d", avc->sps->vui.matrix_coefficients);
			inspect_printf(dump, "\" vui_low_delay_hrd_flag=\"%d", avc->sps->vui.low_delay_hrd_flag);
		}
		inspect_printf(dump, "\" log2_max_poc_lsb=\"%d", avc->sps->log2_max_poc_lsb);
		inspect_printf(dump, "\" log2_max_frame_num=\"%d", avc->sps->log2_max_frame_num);
		inspect_printf(dump, "\" delta_pic_order_always_zero_flag=\"%d", avc->sps->delta_pic_order_always_zero_flag);
		inspect_printf(dump, "\" offset_for_non_ref_pic=\"%d", avc->sps->offset_for_non_ref_pic);

		break;
	case GF_AVC_NALU_PIC_PARAM:
		if (is_encrypted) break;
		idx = gf_avc_read_pps_bs(bs, avc);
		if (idx<0) {
			inspect_printf(dump, "\" pps_id=\"PARSING FAILURE");
			break;
		}
		if (full_bs_dump) break;
		inspect_printf(dump, "\" pps_id=\"%d\" sps_id=\"%d", idx, avc->pps[idx].sps_id);
		inspect_printf(dump, "\" entropy_coding_mode_flag=\"%d", avc->pps[idx].entropy_coding_mode_flag);
		inspect_printf(dump, "\" deblocking_filter_control_present_flag=\"%d", avc->pps[idx].deblocking_filter_control_present_flag);
		inspect_printf(dump, "\" mb_slice_group_map_type=\"%d", avc->pps[idx].mb_slice_group_map_type);
		inspect_printf(dump, "\" num_ref_idx_l0_default_active_minus1=\"%d", avc->pps[idx].num_ref_idx_l0_default_active_minus1);
		inspect_printf(dump, "\" num_ref_idx_l1_default_active_minus1=\"%d", avc->pps[idx].num_ref_idx_l1_default_active_minus1);
		inspect_printf(dump, "\" pic_order_present=\"%d", avc->pps[idx].pic_order_present);
		inspect_printf(dump, "\" pic_size_in_map_units_minus1=\"%d", avc->pps[idx].pic_size_in_map_units_minus1);
		inspect_printf(dump, "\" redundant_pic_cnt_present=\"%d", avc->pps[idx].redundant_pic_cnt_present);
		inspect_printf(dump, "\" slice_group_change_rate_minus1=\"%d", avc->pps[idx].slice_group_change_rate_minus1);
		inspect_printf(dump, "\" slice_group_count=\"%d", avc->pps[idx].slice_group_count);
		inspect_printf(dump, "\" weighted_pred_flag=\"%d", avc->pps[idx].weighted_pred_flag);
		inspect_printf(dump, "\" weighted_bipred_idc=\"%d", avc->pps[idx].weighted_bipred_idc);
		break;
	case GF_AVC_NALU_ACCESS_UNIT:
		if (is_encrypted) break;
		if (full_bs_dump) break;
		inspect_printf(dump, "\" primary_pic_type=\"%d", gf_bs_read_u8(bs) >> 5);
		break;
	case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
		if (is_encrypted) break;
		idx = gf_avc_read_sps_bs(bs, avc, 1, NULL);
		if (idx<0) {
			inspect_printf(dump, "\" status=\"CORRUPTED NAL");
			break;
		}
		if (full_bs_dump) break;
		inspect_printf(dump, "\" sps_id=\"%d", idx - GF_SVC_SSPS_ID_SHIFT);
		break;
	case GF_AVC_NALU_SVC_SLICE:
		if (is_encrypted) break;
		gf_avc_parse_nalu(bs, avc);
		if (full_bs_dump) break;
		if (ptr_size<4) {
			inspect_printf(dump, "\" status=\"CORRUPTED NAL");
			break;
		}
		dependency_id = (ptr[2] & 0x70) >> 4;
		quality_id = (ptr[2] & 0x0F);
		temporal_id = (ptr[3] & 0xE0) >> 5;
		inspect_printf(dump, "\" dependency_id=\"%d\" quality_id=\"%d\" temporal_id=\"%d", dependency_id, quality_id, temporal_id);
		inspect_printf(dump, "\" poc=\"%d", avc->s_info.poc);
		break;
	case GF_AVC_NALU_SVC_PREFIX_NALU:
		if (is_encrypted) break;
		if (full_bs_dump) break;
		if (ptr_size<4) {
			inspect_printf(dump, "\" status=\"CORRUPTED NAL");
			break;
		}
		dependency_id = (ptr[2] & 0x70) >> 4;
		quality_id = (ptr[2] & 0x0F);
		temporal_id = (ptr[3] & 0xE0) >> 5;
		inspect_printf(dump, "\" dependency_id=\"%d\" quality_id=\"%d\" temporal_id=\"%d", dependency_id, quality_id, temporal_id);
		break;
	//extractor
	case GF_AVC_NALU_FF_EXTRACTOR:
		if (is_encrypted) break;
		if (ptr_size<7+nalh_size+nalh_size) {
			inspect_printf(dump, "\" status=\"CORRUPTED NAL");
			break;
		}
		track_ref_index = (u8) ptr[4];
		sample_offset = (s8) ptr[5];
		data_offset = inspect_get_nal_size(&ptr[6], nalh_size);
		data_size = inspect_get_nal_size(&ptr[6+nalh_size], nalh_size);
		inspect_printf(dump, "\" track_ref_index=\"%d\" sample_offset=\"%d\" data_offset=\"%d\" data_size=\"%d\"", track_ref_index, sample_offset, data_offset, data_size);
		break;
	default:
		break;
	}
	gf_fputs("\"", dump);

	if (!full_bs_dump) {
		if (nal_ref_idc) {
			inspect_printf(dump, " nal_ref_idc=\"%d\"", nal_ref_idc);
		}
		if (res>=0) {
			inspect_printf(dump, " poc=\"%d\" pps_id=\"%d\" field_pic_flag=\"%d\"", avc->s_info.poc, avc->s_info.pps->id, (int)avc->s_info.field_pic_flag);
		}
	}

	if (res == -1)
		inspect_printf(dump, " status=\"error decoding slice\"");

	if (!is_encrypted && (type == GF_AVC_NALU_SEI)) {
		inspect_printf(dump, ">\n");
		gf_bs_set_logger(bs, NULL, NULL);
		dump_sei(dump, bs, GF_FALSE);
		inspect_printf(dump, "   </NALU>\n");
	} else {
		inspect_printf(dump, "/>\n");
	}

	if (bs) {
		if (!pctx)
			gf_bs_del(bs);
		else
			gf_bs_set_logger(bs, NULL, NULL);
	}
}

GF_EXPORT
void gf_inspect_dump_nalu(FILE *dump, u8 *ptr, u32 ptr_size, Bool is_svc, HEVCState *hevc, AVCState *avc, VVCState *vvc, u32 nalh_size, Bool dump_crc, Bool is_encrypted)
{
	if (!dump) return;
	gf_inspect_dump_nalu_internal(dump, ptr, ptr_size, is_svc, hevc, avc, vvc, nalh_size, dump_crc, is_encrypted, 0, NULL);
}

static void av1_dump_tile(FILE *dump, u32 idx, AV1Tile *tile)
{
	inspect_printf(dump, "     <Tile number=\"%d\" start=\"%d\" size=\"%d\"/>\n", idx, tile->obu_start_offset, tile->size);
}

static u64 gf_inspect_dump_obu_internal(FILE *dump, AV1State *av1, u8 *obu_ptr, u64 obu_ptr_length, ObuType obu_type, u64 obu_size, u32 hdr_size, Bool dump_crc, PidCtx *pctx, u32 full_dump)
{
	//when the pid context is not set, obu_size (which includes the header size in gpac) must be set
	assert(pctx || obu_size >= 2);

	if (pctx) {
		InspectLogCbk lcbk;

		if (full_dump>=INSPECT_ANALYZE_BS) {
			lcbk.dump = dump;
			lcbk.dump_bits = (full_dump==INSPECT_ANALYZE_BS_BITS) ? GF_TRUE : GF_FALSE;
			gf_bs_set_logger(pctx->bs, regular_bs_log, &lcbk);

			inspect_printf(dump, "   <OBU");
		}
		gf_av1_parse_obu(pctx->bs, &obu_type, &obu_size, &hdr_size, pctx->av1_state);


		if (full_dump>=INSPECT_ANALYZE_BS) {
			gf_bs_set_logger(pctx->bs, NULL, NULL);
		} else {
			full_dump = 0;
		}
	}

	if (!full_dump) {
		inspect_printf(dump, "   <OBU");
	}


#define DUMP_OBU_INT(_v) inspect_printf(dump, #_v"=\"%d\" ", av1->_v);
#define DUMP_OBU_INT2(_n, _v) inspect_printf(dump, _n"=\"%d\" ", _v);

	inspect_printf(dump, " size=\""LLU"\" type=\"%s\" header_size=\"%d\" ", obu_size, gf_av1_get_obu_name(obu_type), hdr_size);

	if (!full_dump) {
		inspect_printf(dump, "has_size_field=\"%d\" has_ext=\"%d\" temporalID=\"%d\" spatialID=\"%d\" ", av1->obu_has_size_field, av1->obu_extension_flag, av1->temporal_id , av1->spatial_id);
	}

	if (dump_crc && (obu_size<0xFFFFFFFFUL))
		inspect_printf(dump, "crc=\"%u\" ", gf_crc_32(obu_ptr, (u32) obu_size) );
	switch (obu_type) {
	case OBU_SEQUENCE_HEADER:
		if (full_dump) break;
		DUMP_OBU_INT(sequence_width)
		DUMP_OBU_INT(sequence_height)
		DUMP_OBU_INT(still_picture)
		DUMP_OBU_INT(OperatingPointIdc)
		DUMP_OBU_INT2("profile", av1->config->seq_profile)
		DUMP_OBU_INT2("level", av1->config->seq_level_idx_0)
		DUMP_OBU_INT(bit_depth)
		DUMP_OBU_INT2("monochrome", av1->config->monochrome)
		DUMP_OBU_INT(color_description_present_flag)
		DUMP_OBU_INT(color_primaries)
		DUMP_OBU_INT(transfer_characteristics)
		DUMP_OBU_INT(matrix_coefficients)
		DUMP_OBU_INT(color_range)
		DUMP_OBU_INT2("chroma_subsampling_x", av1->config->chroma_subsampling_x)
		DUMP_OBU_INT2("chroma_subsampling_y", av1->config->chroma_subsampling_y)
		DUMP_OBU_INT2("chroma_sample_position", av1->config->chroma_sample_position)
		DUMP_OBU_INT(film_grain_params_present)
		break;
	case OBU_FRAME_HEADER:
	case OBU_FRAME:
		if (!full_dump) {
			if (av1->frame_id_numbers_present_flag) {
				DUMP_OBU_INT2("delta_frame_id_length_minus_2", av1->delta_frame_id_length_minus_2)
			}
			if (av1->reduced_still_picture_header) {
				DUMP_OBU_INT(reduced_still_picture_header)
			}
			DUMP_OBU_INT2("uncompressed_header_bytes", av1->frame_state.uncompressed_header_bytes);
			if (av1->frame_state.uncompressed_header_bytes) {
				if (av1->frame_state.frame_type==AV1_KEY_FRAME) inspect_printf(dump, "frame_type=\"key\" ");
				else if (av1->frame_state.frame_type==AV1_INTER_FRAME) inspect_printf(dump, "frame_type=\"inter\" ");
				else if (av1->frame_state.frame_type==AV1_INTRA_ONLY_FRAME) inspect_printf(dump, "frame_type=\"intra_only\" ");
				else if (av1->frame_state.frame_type==AV1_SWITCH_FRAME) inspect_printf(dump, "frame_type=\"switch\" ");
				inspect_printf(dump, "refresh_frame_flags=\"%d\" ", av1->frame_state.refresh_frame_flags);

				if (!av1->frame_state.show_existing_frame) {
					DUMP_OBU_INT2("show_frame", av1->frame_state.show_frame);
				} else {
					DUMP_OBU_INT2("show_existing_frame", av1->frame_state.show_existing_frame);
					DUMP_OBU_INT2("frame_to_show_map_idx", av1->frame_state.frame_to_show_map_idx);
				}
				DUMP_OBU_INT(width);
				DUMP_OBU_INT(height);
			}
			if (obu_type==OBU_FRAME_HEADER)
				break;
		}

	case OBU_TILE_GROUP:
		if (av1->frame_state.nb_tiles_in_obu) {
			u32 i;
			DUMP_OBU_INT2("nb_tiles", av1->frame_state.nb_tiles_in_obu)
			fprintf(dump, ">\n");
			for (i = 0; i < av1->frame_state.nb_tiles_in_obu; i++) {
				av1_dump_tile(dump, i, &av1->frame_state.tiles[i]);
			}
		} else {
			inspect_printf(dump, "nb_tiles=\"unknown\">\n");
		}
		inspect_printf(dump, "   </OBU>\n");
		break;
	case OBU_METADATA:
		{
			GF_BitStream *bs = gf_bs_new(obu_ptr+hdr_size, obu_ptr_length-hdr_size, GF_BITSTREAM_READ);
			u32 metadata_type = (u32)gf_av1_leb128_read(bs, NULL);
			DUMP_OBU_INT2("metadata_type", metadata_type);
			switch (metadata_type) {
				case OBU_METADATA_TYPE_ITUT_T35:
					dump_t35(dump, bs);
					break;
				case OBU_METADATA_TYPE_HDR_CLL:
					dump_clli(dump, bs);
					break;
				case OBU_METADATA_TYPE_HDR_MDCV:
					dump_mdcv(dump, bs, GF_FALSE);
					break;
				default:
					break;
			}
			gf_bs_del(bs);
		}
		break;
	default:
		break;

	}
	if ((obu_type != OBU_TILE_GROUP) && (obu_type != OBU_FRAME) )
		inspect_printf(dump, "/>\n");

	return obu_size;
}

GF_EXPORT
void gf_inspect_dump_obu(FILE *dump, AV1State *av1, u8 *obu_ptr, u64 obu_ptr_length, ObuType obu_type, u64 obu_size, u32 hdr_size, Bool dump_crc)
{
	if (!dump) return;
	gf_inspect_dump_obu_internal(dump, av1, obu_ptr, obu_ptr_length, obu_type, obu_size, hdr_size, dump_crc, NULL, 0);
}

static void gf_inspect_dump_prores_internal(FILE *dump, u8 *ptr, u64 frame_size, Bool dump_crc, PidCtx *pctx)
{
	GF_ProResFrameInfo prores_frame;
	GF_Err e;
	GF_BitStream *bs;
	if (pctx) {
		gf_bs_reassign_buffer(pctx->bs, ptr, frame_size);
		bs = pctx->bs;
	} else {
		bs = gf_bs_new(ptr, frame_size, GF_BITSTREAM_READ);
	}
	e = gf_media_prores_parse_bs(bs, &prores_frame);
	if (!pctx) gf_bs_del(bs);

	if (e) {
		inspect_printf(dump, "   <!-- Error reading frame %s -->\n", gf_error_to_string(e) );
		return;
	}
	inspect_printf(dump, "   <ProResFrame framesize=\"%d\" frameID=\"%s\" version=\"%d\""
		, prores_frame.frame_size
		, gf_4cc_to_str(prores_frame.frame_identifier)
		, prores_frame.version
	);
	inspect_printf(dump, " encoderID=\"%s\" width=\"%d\" height=\"%d\""
		, gf_4cc_to_str(prores_frame.encoder_id)
		, prores_frame.width
		, prores_frame.height
	);
	switch (prores_frame.chroma_format) {
	case 0: inspect_printf(dump, " chromaFormat=\"reserved(0)\""); break;
	case 1: inspect_printf(dump, " chromaFormat=\"reserved(1)\""); break;
	case 2: inspect_printf(dump, " chromaFormat=\"4:2:2\""); break;
	case 3: inspect_printf(dump, " chromaFormat=\"4:4:4\""); break;
	}
	switch (prores_frame.interlaced_mode) {
	case 0: inspect_printf(dump, " interlacedMode=\"progressive\""); break;
	case 1: inspect_printf(dump, " interlacedMode=\"interlaced_top_first\""); break;
	case 2: inspect_printf(dump, " interlacedMode=\"interlaced_bottom_first\""); break;
	case 3: inspect_printf(dump, " interlacedMode=\"reserved\""); break;
	}
	switch (prores_frame.aspect_ratio_information) {
	case 0: inspect_printf(dump, " aspectRatio=\"unknown\""); break;
	case 1: inspect_printf(dump, " aspectRatio=\"1:1\""); break;
	case 2: inspect_printf(dump, " aspectRatio=\"4:3\""); break;
	case 3: inspect_printf(dump, " aspectRatio=\"16:9\""); break;
	default: inspect_printf(dump, " aspectRatio=\"reserved(%d)\"", prores_frame.aspect_ratio_information); break;
	}
	switch (prores_frame.framerate_code) {
	case 0: inspect_printf(dump, " framerate=\"unknown\""); break;
	case 1: inspect_printf(dump, " framerate=\"23.976\""); break;
	case 2: inspect_printf(dump, " framerate=\"24\""); break;
	case 3: inspect_printf(dump, " framerate=\"25\""); break;
	case 4: inspect_printf(dump, " framerate=\"29.97\""); break;
	case 5: inspect_printf(dump, " framerate=\"30\""); break;
	case 6: inspect_printf(dump, " framerate=\"50\""); break;
	case 7: inspect_printf(dump, " framerate=\"59.94\""); break;
	case 8: inspect_printf(dump, " framerate=\"60\""); break;
	case 9: inspect_printf(dump, " framerate=\"100\""); break;
	case 10: inspect_printf(dump, " framerate=\"119.88\""); break;
	case 11: inspect_printf(dump, " framerate=\"120\""); break;
	default: inspect_printf(dump, " framerate=\"reserved(%d)\"", prores_frame.framerate_code); break;
	}
	switch (prores_frame.color_primaries) {
	case 0: case 2: inspect_printf(dump, " colorPrimaries=\"unknown\""); break;
	case 1: inspect_printf(dump, " colorPrimaries=\"BT.709\""); break;
	case 5: inspect_printf(dump, " colorPrimaries=\"BT.601-625\""); break;
	case 6: inspect_printf(dump, " colorPrimaries=\"BT.601-525\""); break;
	case 9: inspect_printf(dump, " colorPrimaries=\"BT.2020\""); break;
	case 11: inspect_printf(dump, " colorPrimaries=\"P3\""); break;
	case 12: inspect_printf(dump, " colorPrimaries=\"P3-D65\""); break;
	default: inspect_printf(dump, " colorPrimaries=\"reserved(%d)\"", prores_frame.color_primaries); break;
	}
	switch (prores_frame.matrix_coefficients) {
	case 0: case 2: inspect_printf(dump, " matrixCoefficients=\"unknown\""); break;
	case 1: inspect_printf(dump, " matrixCoefficients=\"BT.709\""); break;
	case 6: inspect_printf(dump, " matrixCoefficients=\"BT.601\""); break;
	case 9: inspect_printf(dump, " matrixCoefficients=\"BT.2020\""); break;
	default: inspect_printf(dump, " matrixCoefficients=\"reserved(%d)\"", prores_frame.matrix_coefficients); break;
	}
	switch (prores_frame.transfer_characteristics) {
	case 0: inspect_printf(dump, " transferCharacteristics=\"unknown\""); break;
	case 1: inspect_printf(dump, " transferCharacteristics=\"BT-709\""); break;
	case 16: inspect_printf(dump, " transferCharacteristics=\"ST-2084\""); break;
	case 18: inspect_printf(dump, " transferCharacteristics=\"STD-B67\""); break;
	case 2:
	default:
		inspect_printf(dump, " transferCharacteristics=\"unspecified\""); break;
	}
	switch (prores_frame.alpha_channel_type) {
	case 0: inspect_printf(dump, " alphaChannel=\"none\""); break;
	case 1: inspect_printf(dump, " alphaChannel=\"8bits\""); break;
	case 2: inspect_printf(dump, " alphaChannel=\"16bits\""); break;
	default: inspect_printf(dump, " alphaChannel=\"reserved(%d)\"", prores_frame.alpha_channel_type); break;
	}
	inspect_printf(dump, " numPictures=\"%d\"" , prores_frame.transfer_characteristics, prores_frame.nb_pic);

	if (dump_crc) {
		inspect_printf(dump, " crc=\"%d\"" , gf_crc_32(ptr, (u32) frame_size) );
	}
	if (!prores_frame.load_luma_quant_matrix && !prores_frame.load_chroma_quant_matrix) {
		inspect_printf(dump, "/>\n");
	} else {
		u32 j, k;
		inspect_printf(dump, ">\n");
		if (prores_frame.load_luma_quant_matrix) {
			inspect_printf(dump, "    <LumaQuantMatrix coefs=\"");
			for (j=0; j<8; j++) {
				for (k=0; k<8; k++) {
					inspect_printf(dump, " %02X", prores_frame.luma_quant_matrix[j][k]);
				}
			}
			inspect_printf(dump, "\">\n");
		}
		if (prores_frame.load_chroma_quant_matrix) {
			inspect_printf(dump, "    <ChromaQuantMatrix coefs=\"");
			for (j=0; j<8; j++) {
				for (k=0; k<8; k++) {
					inspect_printf(dump, " %02X", prores_frame.chroma_quant_matrix[j][k]);
				}
			}
			inspect_printf(dump, "\">\n");
		}
		inspect_printf(dump, "   </ProResFrame>\n");
	}
}

GF_EXPORT
void gf_inspect_dump_prores(FILE *dump, u8 *ptr, u64 frame_size, Bool dump_crc)
{
	if (!dump) return;
	gf_inspect_dump_prores_internal(dump, ptr, frame_size, dump_crc, NULL);
}

static void gf_inspect_dump_opus_internal(FILE *dump, u8 *ptr, u32 size, u32 channel_count, Bool dump_crc, PidCtx *pctx)
{
	u32 pck_offset=0;
	u32 k;

	if (pctx) channel_count = pctx->opus_channel_count;

	for (k=0; k<channel_count; k++) {
		u8 self_delimited = (k != channel_count-1);
		GF_OpusPacketHeader pckh;
		u8 headerres;

		headerres = gf_opus_parse_packet_header(ptr+pck_offset, size-pck_offset, self_delimited, &pckh);
		if (!headerres) break;

		inspect_printf(dump, "    <OpusPacket offset=\"%d\" self_delimited=\"%d\"", pck_offset, pckh.self_delimited);
		inspect_printf(dump, " header_size=\"%d\" config=\"%d\" stereo=\"%d\" code=\"%d\"", pckh.size, pckh.TOC_config, pckh.TOC_stereo, pckh.TOC_code);
		if (pckh.TOC_code == 0) {
			inspect_printf(dump, " nb_frames=\"%d\" frame_lengths=\"%d\"/>\n", pckh.nb_frames, pckh.frame_lengths[0]);
		} else if (pckh.TOC_code == 1) {
			inspect_printf(dump, " nb_frames=\"%d\" frame_lengths=\"%d %d\"/>\n", pckh.nb_frames, pckh.frame_lengths[0], pckh.frame_lengths[1]);
		} else if (pckh.TOC_code == 2) {
			inspect_printf(dump, " nb_frames=\"%d\" frame_lengths=\"%d %d\"/>\n", pckh.nb_frames, pckh.frame_lengths[0], pckh.frame_lengths[1]);
		} else if (pckh.TOC_code == 3) {
			u32 j;
			inspect_printf(dump, " vbr=\"%d\" padding=\"%d\" padding_length=\"%d\" nb_frames=\"%d\"", pckh.code3_vbr, pckh.code3_padding, pckh.code3_padding_length, pckh.nb_frames);
			inspect_printf(dump, " frame_lengths=\"");
			for(j=0;j<pckh.nb_frames;j++) {
				if (j!=0) fprintf(dump, " ");
				inspect_printf(dump, "%d", pckh.frame_lengths[j]);
			}
			inspect_printf(dump, "\"");
			if (dump_crc) {
				inspect_printf(dump, " crc=\"%d\"" , gf_crc_32(ptr, (u32) size) );
			}
			inspect_printf(dump, "/>\n");
		}

		if (self_delimited) {
			if (pck_offset+pckh.packet_size >= size) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Opus] Not enough data to parse next self-delimited packet!\n"));
			}
			pck_offset += pckh.packet_size;
		}
	}
}

GF_EXPORT
void gf_inspect_dump_opus(FILE *dump, u8 *ptr, u64 size, u32 channel_count, Bool dump_crc)
{
	if (!dump) return;
    gf_inspect_dump_opus_internal(dump, ptr, (u32) size, channel_count, dump_crc, NULL);
}

enum {
	MHAS_FILLER = 0,
	MHAS_CONFIG,
	MHAS_FRAME,
	MHAS_SCENE_INFO,
	MHAS_RES4,
	MHAS_RES5,
	MHAS_SYNC,
	MHAS_SYNC_GAP,
	MHAS_MARKER,
	MHAS_CRC16,
	MHAS_CRC32,
	MHAS_DESCRIPTOR,
	MHAS_INTERACTION,
	MHAS_LOUDNESS_CRC,
	MHAS_BUFFER_INFO,
	MHAS_GLOBAL_CRC16,
	MHAS_GLOBAL_CRC32,
	MHAS_AUDIO_TRUNCATION,
	MHAS_GEN_DATA,
};
static struct {
	u32 type;
	const char *name;
} mhas_pack_types[] =
{
	{MHAS_FILLER, "filler"},
	{MHAS_CONFIG, "config"},
	{MHAS_FRAME, "frame"},
	{MHAS_SCENE_INFO, "scene_info"},
	{MHAS_SYNC, "sync"},
	{MHAS_SYNC_GAP, "sync_gap"},
	{MHAS_MARKER, "marker"},
	{MHAS_CRC16, "crc16"},
	{MHAS_CRC32, "crc32"},
	{MHAS_DESCRIPTOR, "descriptor"},
	{MHAS_INTERACTION, "interaction"},
	{MHAS_LOUDNESS_CRC, "loudness_drc"},
	{MHAS_BUFFER_INFO, "buffer_info"},
	{MHAS_GLOBAL_CRC16, "global_crc16"},
	{MHAS_GLOBAL_CRC32, "global_crc32"},
	{MHAS_AUDIO_TRUNCATION, "audio_truncation"},
	{MHAS_GEN_DATA, "gen_data"},
};

static void dump_mha_config(FILE *dump, GF_BitStream *bs, const char *indent)
{
	u32 val;
	inspect_printf(dump, "%s<MPEGHConfig", indent);
	val = gf_bs_read_int(bs, 8);
	inspect_printf(dump, " ProfileLevelIndication=\"%d\"", val);
	val = gf_bs_read_int(bs, 5);
	inspect_printf(dump, " usacSamplerateIndex=\"%d\"", val);
	if (val==0x1f) {
		val = gf_bs_read_int(bs, 24);
		inspect_printf(dump, " usacSamplerate=\"%d\"", val);
	}
	val = gf_bs_read_int(bs, 3);
	inspect_printf(dump, " coreSbrFrameLengthIndex=\"%d\"", val);
	gf_bs_read_int(bs, 1);
	val = gf_bs_read_int(bs, 1);
	inspect_printf(dump, " receiverDelayCompensation=\"%d\"", val);
	inspect_printf(dump, "/>\n");
}
static void gf_inspect_dump_mha_frame(FILE *dump, GF_BitStream *bs, const char *indent)
{
	u32 val;
	inspect_printf(dump, "%s<MPEGHFrame", indent);
	val = gf_bs_read_int(bs, 1);
	inspect_printf(dump, " usacIndependencyFlag=\"%d\"", val);
	inspect_printf(dump, "/>\n");
}
static void gf_inspect_dump_mhas(FILE *dump, u8 *ptr, u64 frame_size, Bool dump_crc, PidCtx *pctx)
{
	u64 gf_mpegh_escaped_value(GF_BitStream *bs, u32 nBits1, u32 nBits2, u32 nBits3);
	gf_bs_reassign_buffer(pctx->bs, ptr, frame_size);
	GF_BitStream *bs = pctx->bs;

	inspect_printf(dump, "   <MHASFrame>\n");

	while (gf_bs_available(bs)) {
		u32 i, count;
		const char *type_name="unknown";
		u64 pos;
		u32 type = (u32) gf_mpegh_escaped_value(bs, 3, 8, 8);
		u64 label = gf_mpegh_escaped_value(bs, 2, 8, 32);
		u64 size = gf_mpegh_escaped_value(bs, 11, 24, 24);

		count = GF_ARRAY_LENGTH(mhas_pack_types);
		for (i=0; i<count; i++) {
			if (mhas_pack_types[i].type==type) {
				type_name = mhas_pack_types[i].name;
				break;
			}
		}
		inspect_printf(dump, "    <MHASPacket type=\"%s\" label=\""LLU"\" size=\""LLU"\"", type_name, label, size);

		pos = gf_bs_get_position(bs);
		switch (type) {
		case MHAS_CONFIG:
			inspect_printf(dump, ">\n");
			dump_mha_config(dump, bs, "     ");
			inspect_printf(dump, "    </MHASPacket>\n");
			break;
		case MHAS_FRAME:
			inspect_printf(dump, ">\n");
			gf_inspect_dump_mha_frame(dump, bs, "     ");
			inspect_printf(dump, "    </MHASPacket>\n");
			break;
		case MHAS_BUFFER_INFO:
			if (gf_bs_read_int(bs, 1)) {
				inspect_printf(dump, " buffer_fullness_present=\"1\" buffer_fullness=\""LLU"\"", gf_mpegh_escaped_value(bs, 15,24,32) );
			} else {
				inspect_printf(dump, " buffer_fullness_present=\"0\"");

			}
			inspect_printf(dump, "/>\n");
			break;
		case MHAS_SYNC:
			inspect_printf(dump, " sync=\"0x%02X\"/>\n", gf_bs_read_u8(bs) );
			break;
		case MHAS_SYNC_GAP:
			inspect_printf(dump, " syncSpacingLength=\"0x%02" LLX_SUF "\"/>\n", gf_mpegh_escaped_value(bs, 16, 24, 24) );
			break;
		case MHAS_MARKER:
		case MHAS_DESCRIPTOR:
			inspect_printf(dump, " %s=\"0x", (type==MHAS_MARKER) ? "markers" : "descriptors");
			for (i=0; i<size; i++)
				inspect_printf(dump, "%02X", gf_bs_read_u8(bs) );
			inspect_printf(dump, "\"/>\n");
			break;
		default:
			inspect_printf(dump, "/>\n");
			break;
		}
		gf_bs_align(bs);
		pos = gf_bs_get_position(bs) - pos;
		if (pos < size)
			gf_bs_skip_bytes(bs, size - pos);

	}
	inspect_printf(dump, "   </MHASFrame>\n");
}

#endif


static void finalize_dump(GF_InspectCtx *ctx, u32 streamtype, Bool concat)
{
	char szLine[1025];
	u32 i, count = gf_list_count(ctx->src_pids);
	for (i=0; i<count; i++) {
		PidCtx *pctx = gf_list_get(ctx->src_pids, i);
		//already done
		if (!pctx->tmp) continue;
		//not our streamtype
		if (streamtype && (pctx->stream_type!=streamtype)) continue;

		if (concat) {
			gf_fseek(pctx->tmp, 0, SEEK_SET);
			while (!gf_feof(pctx->tmp)) {
				u32 read = (u32) gf_fread(szLine, 1024, pctx->tmp);
				if (ctx->dump_log) {
					szLine[1024] = 0;
					GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s", szLine));
				} else {
					if (gf_fwrite(szLine, read, ctx->dump) != read) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Inspect] failed to concatenate trace: %s\n", gf_error_to_string(GF_IO_ERR)));
						break;
					}
				}
			}
		}
		gf_fclose(pctx->tmp);
		if (ctx->xml)
			inspect_printf(ctx->dump, "</PIDInspect>\n");
		pctx->tmp = NULL;
	}
}

static void inspect_finalize(GF_Filter *filter)
{
	Bool concat=GF_FALSE;
	GF_InspectCtx *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	if (ctx->is_prober) {
		FILE *fout;
		Bool do_close=GF_FALSE;
		if (!strcmp(ctx->log, "stderr")) fout = stderr;
		else if (!strcmp(ctx->log, "stdout")) fout = stdout;
		else if (!strcmp(ctx->log, "null")) fout = NULL;
		else {
			fout = gf_fopen(ctx->log, "w");
			do_close = GF_TRUE;
		}
		if (fout) {
			inspect_printf(fout, "%u\n", gf_list_count(ctx->src_pids));
			if (do_close) gf_fclose(fout);
		}
	} else {
		if (ctx->dump) {
			if ((ctx->dump!=stderr) && (ctx->dump!=stdout)) concat=GF_TRUE;
			else if (!ctx->interleave) concat=GF_TRUE;
		}


		if (!ctx->interleave && ctx->dump) {
			finalize_dump(ctx, GF_STREAM_AUDIO, concat);
			finalize_dump(ctx, GF_STREAM_VISUAL, concat);
			finalize_dump(ctx, GF_STREAM_SCENE, concat);
			finalize_dump(ctx, GF_STREAM_OD, concat);
			finalize_dump(ctx, GF_STREAM_TEXT, concat);
			finalize_dump(ctx, 0, concat);
		}
	}

	while (gf_list_count(ctx->src_pids)) {
		PidCtx *pctx = gf_list_pop_front(ctx->src_pids);

#ifndef GPAC_DISABLE_AV_PARSERS
		if (pctx->avc_state) gf_free(pctx->avc_state);
		if (pctx->hevc_state) gf_free(pctx->hevc_state);
		if (pctx->vvc_state) gf_free(pctx->vvc_state);
		if (pctx->av1_state) {
			if (pctx->av1_state->config) gf_odf_av1_cfg_del(pctx->av1_state->config);
			gf_av1_reset_state(pctx->av1_state, GF_TRUE);
			gf_free(pctx->av1_state);
		}
#endif
		if (pctx->vpcc) gf_odf_vp_cfg_del(pctx->vpcc);
		if (pctx->bs) gf_bs_del(pctx->bs);
		gf_free(pctx);
	}
	gf_list_del(ctx->src_pids);

	if (ctx->dump) {
		if (ctx->xml) inspect_printf(ctx->dump, "</GPACInspect>\n");
		if ((ctx->dump!=stderr) && (ctx->dump!=stdout)) {

#ifdef GPAC_ENABLE_COVERAGE
			if (gf_sys_is_cov_mode()) {
				gf_fflush(ctx->dump);
				gf_ferror(ctx->dump);
			}
#endif
			gf_fclose(ctx->dump);
		}
	}
}

static void dump_temi_loc(GF_InspectCtx *ctx, PidCtx *pctx, FILE *dump, const char *pname, const GF_PropertyValue *att)
{
	u32 val;
	Bool is_announce = GF_FALSE;

	if (ctx->xml) {
		inspect_printf(dump, " <TEMILocation");
	} else {
		inspect_printf(dump, " TEMILocation");
	}
	if (!pctx->bs)
		pctx->bs = gf_bs_new(att->value.data.ptr, att->value.data.size, GF_BITSTREAM_READ);
	else
		gf_bs_reassign_buffer(pctx->bs, att->value.data.ptr, att->value.data.size);

	while (1) {
		u8 achar =  gf_bs_read_u8(pctx->bs);
		if (!achar) break;
	}

	val = atoi(pname+7);

	DUMP_ATT_D("timeline", val)
	DUMP_ATT_STR("url", att->value.data.ptr)
	if (gf_bs_read_int(pctx->bs, 1)) {
		DUMP_ATT_D("announce", 1)
		is_announce = GF_TRUE;
	}
	if (gf_bs_read_int(pctx->bs, 1)) {
		DUMP_ATT_D("splicing", 1)
	}
	if (gf_bs_read_int(pctx->bs, 1)) {
		DUMP_ATT_D("reload", 1)
	}
	gf_bs_read_int(pctx->bs, 5);
	if (is_announce) {
		GF_Fraction time;
		time.den = gf_bs_read_u32(pctx->bs);
		time.num = gf_bs_read_u32(pctx->bs);

		DUMP_ATT_FRAC("splice_start", time)
	}

	if (ctx->xml) {
		inspect_printf(dump, "/>\n");
	} else {
		inspect_printf(dump, "\n");
	}
}

static void dump_temi_time(GF_InspectCtx *ctx, PidCtx *pctx, FILE *dump, const char *pname, const GF_PropertyValue *att)
{
	u32 val;
	u64 lval;
	if (ctx->xml) {
		inspect_printf(dump, " <TEMITiming");
	} else {
		inspect_printf(dump, " TEMITiming");
	}
	val = atoi(pname+7);
	if (!pctx->bs)
		pctx->bs = gf_bs_new(att->value.data.ptr, att->value.data.size, GF_BITSTREAM_READ);
	else
		gf_bs_reassign_buffer(pctx->bs, att->value.data.ptr, att->value.data.size);

	DUMP_ATT_D("timeline", val)
	val = gf_bs_read_u32(pctx->bs);
	DUMP_ATT_D("media_timescale", val)
	lval = gf_bs_read_u64(pctx->bs);
	DUMP_ATT_LLU("media_timestamp", lval)
	lval = gf_bs_read_u64(pctx->bs);
	DUMP_ATT_LLU("media_pts", lval)

	if (gf_bs_read_int(pctx->bs, 1)) {
		DUMP_ATT_D("reload", 1)
	}
	if (gf_bs_read_int(pctx->bs, 1)) {
		DUMP_ATT_D("paused", 1)
	}
	if (gf_bs_read_int(pctx->bs, 1)) {
		DUMP_ATT_D("discontinuity", 1)
	}
	val = gf_bs_read_int(pctx->bs, 1);
	gf_bs_read_int(pctx->bs, 4);

	if (val) {
		lval = gf_bs_read_u64(pctx->bs);
		DUMP_ATT_LLU("ntp", lval)
	}
	if (ctx->xml) {
		inspect_printf(dump, "/>\n");
	} else {
		inspect_printf(dump, "\n");
	}
}

#ifndef GPAC_DISABLE_AV_PARSERS
static void gf_inspect_dump_truehd_frame(FILE *dump, GF_BitStream *bs)
{
	u8 nibble;
	u16 frame_size, timing;
	u32 sync;
	inspect_printf(dump, " <TrueHDAudioFrame");
	nibble = gf_bs_read_int(bs, 4);
	frame_size = gf_bs_read_int(bs, 12);
	timing = gf_bs_read_u16(bs);
	sync = gf_bs_read_u32(bs);
	inspect_printf(dump, " nibble=\"%u\" size=\"%u\" timing=\"%u\"", nibble, frame_size, timing);
	if (sync != 0xF8726FBA) {
		inspect_printf(dump, " major_sync=\"no\"/>\n");
		return;
	}
	u32 fmt = gf_bs_read_u32(bs);
	u32 sig = gf_bs_read_u16(bs);
	u32 flags = gf_bs_read_u16(bs);
	gf_bs_read_u16(bs);
	Bool vrate = gf_bs_read_int(bs, 1);
	u32 prate = gf_bs_read_int(bs, 15);
	u32 nb_substreams = gf_bs_read_int(bs, 4);
	gf_bs_read_int(bs, 2);
	u32 ext_substream_info = gf_bs_read_int(bs, 2);
	inspect_printf(dump, " major_sync=\"yes\" format=\"%u\" signature=\"%u\" flags=\"0x%04X\" vrate=\"%u\" peak_data_rate=\"%u\" substreams=\"%u\" extended_substream_info=\"%u\" ", fmt, sig, flags, vrate, prate, nb_substreams, ext_substream_info);

	inspect_printf(dump, "/>\n");
}
#endif

static void inspect_dump_property(GF_InspectCtx *ctx, FILE *dump, u32 p4cc, const char *pname, const GF_PropertyValue *att, PidCtx *pctx)
{
	char szDump[GF_PROP_DUMP_ARG_SIZE];

	if (!pname)
		pname = gf_props_4cc_get_name(p4cc);
	else {
		//all properties starting with __ are not dumped
		if (!strncmp(pname, "__", 2))
			return;
		if (!strcmp(pname, "isom_force_ctts") || !strcmp(pname, "reframer_rem_edits") )
			return;
	}

	switch (p4cc) {
	case GF_PROP_PID_DOWNLOAD_SESSION:
	case GF_PROP_PCK_END_RANGE:
		return;
	case GF_PROP_PCK_SENDER_NTP:
	case GF_PROP_PCK_RECEIVER_NTP:
	case GF_PROP_PCK_UTC_TIME:
	case GF_PROP_PCK_MEDIA_TIME:
	case GF_PROP_PID_CENC_HAS_ROLL:
		if (gf_sys_is_test_mode())
			return;
		break;
	}

	if ((att->type==GF_PROP_DATA) && (ctx->analyze || ctx->xml)) {
#ifndef GPAC_DISABLE_AV_PARSERS
		if (p4cc==GF_PROP_PID_CONTENT_LIGHT_LEVEL) {
			GF_BitStream *bs = gf_bs_new(att->value.data.ptr, att->value.data.size, GF_BITSTREAM_READ);
			dump_clli(dump, bs);
			gf_bs_del(bs);
			return;
		}
		else if (p4cc==GF_PROP_PID_MASTER_DISPLAY_COLOUR) {
			GF_BitStream *bs = gf_bs_new(att->value.data.ptr, att->value.data.size, GF_BITSTREAM_READ);
			//mdcv property is always in MPEG units
			dump_mdcv(dump, bs, GF_TRUE);
			gf_bs_del(bs);
			return;
		}
#endif /*GPAC_DISABLE_AV_PARSERS*/
	}


	if (p4cc==GF_PROP_PID_CENC_KEY_INFO) {
		u32 i, nb_keys, kpos;
		u8 *data;
		if (ctx->xml) {
			if (ctx->dtype)
				inspect_printf(dump, " type=\"%s\"", gf_props_get_type_name(att->type) );
			inspect_printf(dump, " %s=\"", pname);
		} else {
			if (ctx->dtype) {
				inspect_printf(dump, "\t%s (%s): ", pname, gf_props_get_type_name(att->type));
			} else {
				inspect_printf(dump, "\t%s: ", pname);
			}
		}
		data = att->value.data.ptr;
		nb_keys = 1;
		if (data[0]) {
			nb_keys = data[1];
			nb_keys <<= 8;
			nb_keys |= data[2];
		}
		if (nb_keys>1) inspect_printf(dump, "[");
		kpos = 3;
		for (i=0; i<nb_keys; i++) {
			u32 j;
			bin128 KID;
			u8 iv_size = data[kpos];
			memcpy(KID, data+kpos+1, 16);
			inspect_printf(dump, "IV_size:%d,KID:0x", iv_size);
			for (j=0; j<16; j++) {
				inspect_printf(dump, "%02X", (unsigned char) data[kpos + 1 + j]);
			}
			kpos+=17;
			if (!iv_size) {
				iv_size = data[kpos];
				inspect_printf(dump, ",const_IV_size:%d", iv_size);
				if (iv_size) {
					inspect_printf(dump, ",const_IV:0x");
					for (j=0; j<iv_size; j++) {
						inspect_printf(dump, "%02X", (unsigned char) data[kpos + 1 + j]);
					}
				}
				kpos += iv_size+1;
			}
		}

		if (nb_keys>1) inspect_printf(dump, "]");
		if (ctx->xml) {
			inspect_printf(dump, "\"");
		} else {
			inspect_printf(dump, "\n");
		}

		return;
	}

	if (gf_sys_is_test_mode() || ctx->test) {
		switch (p4cc) {
		case GF_PROP_PID_FILEPATH:
		case GF_PROP_PID_URL:
		case GF_PROP_PID_MUX_SRC:
			return;
		case GF_PROP_PID_FILE_CACHED:
		case GF_PROP_PID_DURATION:
			if ((ctx->test==INSPECT_TEST_NETWORK) || (ctx->test==INSPECT_TEST_NETX))
				return;
			break;
		case GF_PROP_PID_DECODER_CONFIG:
		case GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT:
		case GF_PROP_PID_DOWN_SIZE:
			if (ctx->test>=INSPECT_TEST_ENCODE)
				return;
			break;
		case GF_PROP_PID_BITRATE:
			if (ctx->test==INSPECT_TEST_NOBR)
				return;
		case GF_PROP_PID_MEDIA_DATA_SIZE:
		case GF_PROP_PID_MAXRATE:
		case GF_PROP_PID_AVG_FRAME_SIZE:
		case GF_PROP_PID_MAX_FRAME_SIZE:
		case GF_PROP_PID_DBSIZE:
			if (ctx->test==INSPECT_TEST_ENCX)
				return;
			break;

		case GF_PROP_PID_ISOM_TRACK_TEMPLATE:
		case GF_PROP_PID_ISOM_MOVIE_TIME:
			if (ctx->test==INSPECT_TEST_NETX)
				return;
			break;

		case GF_PROP_PID_ISOM_TREX_TEMPLATE:
		case GF_PROP_PID_ISOM_STSD_TEMPLATE:
			//TODO once all OK: remove this test and regenerate all hashes
			if (gf_sys_is_test_mode())
				return;
		default:
			if (gf_sys_is_test_mode() && (att->type==GF_PROP_POINTER) )
				return;
			break;
		}
	}

	if (ctx->xml) {
		if (ctx->dtype)
			inspect_printf(dump, " type=\"%s\"", gf_props_get_type_name(att->type) );
			
		if (pname && (strchr(pname, ' ') || strchr(pname, ':'))) {
			u32 i=0, k;
			char *pname_no_space = gf_strdup(pname);
			while (pname_no_space[i]) {
				if (pname_no_space[i]==' ') pname_no_space[i]='_';
				if (pname_no_space[i]==':') pname_no_space[i]='_';
				i++;
			}

			if ((att->type==GF_PROP_UINT_LIST) || (att->type==GF_PROP_4CC_LIST)) {
				for (k=0; k < att->value.uint_list.nb_items; k++) {
					if (k) inspect_printf(dump, ", ");
					if ((att->type==GF_PROP_4CC_LIST) && ! gf_sys_is_test_mode()) {
						inspect_printf(dump, "%s", gf_4cc_to_str(att->value.uint_list.vals[k]) );
					} else {
						inspect_printf(dump, "%d", att->value.uint_list.vals[k]);
					}
				}
			} else if (att->type==GF_PROP_STRING_LIST) {
				for (k=0; k < att->value.string_list.nb_items; k++) {
					if (k) inspect_printf(dump, ", ");
					gf_xml_dump_string(dump, NULL, (char *) att->value.string_list.vals[k], NULL);
				}
			} else if ((att->type==GF_PROP_STRING) || (att->type==GF_PROP_STRING_NO_COPY)) {
				gf_xml_dump_string(dump, NULL, att->value.string, NULL);
			} else {
				inspect_printf(dump, " %s=\"%s\"", pname_no_space, gf_props_dump(p4cc, att, szDump, (GF_PropDumpDataMode) ctx->dump_data));
			}
			gf_free(pname_no_space);
		} else {
			inspect_printf(dump, " %s=\"%s\"", pname ? pname : gf_4cc_to_str(p4cc), gf_props_dump(p4cc, att, szDump, (GF_PropDumpDataMode) ctx->dump_data));
		}
	} else {
		if (ctx->dtype) {
			inspect_printf(dump, "\t%s (%s): ", pname ? pname : gf_4cc_to_str(p4cc), gf_props_get_type_name(att->type));
		} else {
			if (!p4cc && !strncmp(pname, "temi_l", 6))
				dump_temi_loc(ctx, pctx, dump, pname, att);
			else if (!p4cc && !strncmp(pname, "temi_t", 6))
				dump_temi_time(ctx, pctx, dump, pname, att);
			else
				inspect_printf(dump, "\t%s: ", pname ? pname : gf_4cc_to_str(p4cc));
		}

		if ((att->type==GF_PROP_UINT_LIST) || (att->type==GF_PROP_4CC_LIST)) {
			u32 k;
			for (k=0; k < att->value.uint_list.nb_items; k++) {
				if (k) inspect_printf(dump, ", ");
				if ((att->type==GF_PROP_4CC_LIST) && ! gf_sys_is_test_mode()) {
					inspect_printf(dump, "%s", gf_4cc_to_str(att->value.uint_list.vals[k]) );
				} else {
					inspect_printf(dump, "%d", att->value.uint_list.vals[k]);
				}
			}
		} else if (att->type==GF_PROP_STRING_LIST) {
			u32 k;
			for (k=0; k < att->value.string_list.nb_items; k++) {
				if (k) inspect_printf(dump, ", ");
				inspect_printf(dump, "%s", (const char *) att->value.string_list.vals[k]);
			}
		}else{
			inspect_printf(dump, "%s", gf_props_dump(p4cc, att, szDump, (GF_PropDumpDataMode) ctx->dump_data) );
		}
		inspect_printf(dump, "\n");
	}
}

static void inspect_dump_packet_fmt(GF_Filter *filter, GF_InspectCtx *ctx, FILE *dump, GF_FilterPacket *pck, PidCtx *pctx, u64 pck_num)
{
	char szDump[GF_PROP_DUMP_ARG_SIZE];
	u32 size=0;
	const char *data=NULL;
	char *str = ctx->fmt;
	assert(str);

	if (pck) {
		data = gf_filter_pck_get_data(pck, &size);
		pctx->csize += size;
	}

	while (str) {
		char csep;
		char *sep, *key;
		if (!str[0]) break;

		if ((str[0] != '$') && (str[0] != '%') && (str[0] != '@')) {
			inspect_printf(dump, "%c", str[0]);
			str = str+1;
			continue;
		}
		csep = str[0];
		if (str[1] == csep) {
			inspect_printf(dump, "%c", str[0]);
			str = str+2;
			continue;
		}
		sep = strchr(str+1, csep);
		if (!sep) {
			inspect_printf(dump, "%c", str[0]);
			str = str+1;
			continue;
		}
		sep[0] = 0;
		key = str+1;

		if (!pck) {
			if (!strcmp(key, "lf") || !strcmp(key, "n")) inspect_printf(dump, "\n" );
			else if (!strcmp(key, "t")) inspect_printf(dump, "\t" );
			else if (!strncmp(key, "pid.", 4)) inspect_printf(dump, "%s", key+4);
			else inspect_printf(dump, "%s", key);
			sep[0] = csep;
			str = sep+1;
			continue;
		}

		if (!strcmp(key, "pn")) inspect_printf(dump, LLU, pck_num);
		else if (!strcmp(key, "dts")) {
			u64 ts = gf_filter_pck_get_dts(pck);
			if (ts==GF_FILTER_NO_TS) inspect_printf(dump, "N/A");
			else inspect_printf(dump, LLU, ts );
		}
		else if (!strcmp(key, "cts")) {
			u64 ts = gf_filter_pck_get_cts(pck);
			if (ts==GF_FILTER_NO_TS) inspect_printf(dump, "N/A");
			else inspect_printf(dump, LLU, ts );
		}
		else if (!strcmp(key, "ddts")) {
			u64 ts = gf_filter_pck_get_dts(pck);
			if ((ts==GF_FILTER_NO_TS) || (pctx->prev_dts==GF_FILTER_NO_TS)) inspect_printf(dump, "N/A");
			else if (pctx->pck_num<=1) inspect_printf(dump, "N/A");
			else {
				s64 diff = ts;
				diff -= pctx->prev_dts;
				inspect_printf(dump, LLD, diff);
			}
			pctx->prev_dts = ts;
		}
		else if (!strcmp(key, "dcts")) {
			u64 ts = gf_filter_pck_get_cts(pck);
			if ((ts==GF_FILTER_NO_TS) || (pctx->prev_cts==GF_FILTER_NO_TS)) inspect_printf(dump, "N/A");
			else if (pctx->pck_num<=1) inspect_printf(dump, "N/A");
			else {
				s64 diff = ts;
				diff -= pctx->prev_cts;
				inspect_printf(dump, LLD, diff);
			}
			pctx->prev_cts = ts;
		}
		else if (!strcmp(key, "ctso")) {
			u64 dts = gf_filter_pck_get_dts(pck);
			u64 cts = gf_filter_pck_get_cts(pck);
			if (dts==GF_FILTER_NO_TS) dts = cts;
			if (cts==GF_FILTER_NO_TS) inspect_printf(dump, "N/A");
			else inspect_printf(dump, LLD, ((s64)cts) - ((s64)dts) );
		}

		else if (!strcmp(key, "dur")) inspect_printf(dump, "%u", gf_filter_pck_get_duration(pck) );
		else if (!strcmp(key, "frame")) {
			Bool start, end;
			gf_filter_pck_get_framing(pck, &start, &end);
			if (start && end) inspect_printf(dump, "frame_full");
			else if (start) inspect_printf(dump, "frame_start");
			else if (end) inspect_printf(dump, "frame_end");
			else inspect_printf(dump, "frame_cont");
		}
		else if (!strcmp(key, "sap") || !strcmp(key, "rap")) {
			u32 sap = gf_filter_pck_get_sap(pck);
			if (sap==GF_FILTER_SAP_4_PROL) {
				inspect_printf(dump, "4 (prol)");
			} else {
				inspect_printf(dump, "%u", sap );
			}
		}
		else if (!strcmp(key, "ilace")) inspect_printf(dump, "%d", gf_filter_pck_get_interlaced(pck) );
		else if (!strcmp(key, "corr")) inspect_printf(dump, "%d", gf_filter_pck_get_corrupted(pck) );
		else if (!strcmp(key, "seek")) inspect_printf(dump, "%d", gf_filter_pck_get_seek_flag(pck) );
		else if (!strcmp(key, "bo")) {
			u64 bo = gf_filter_pck_get_byte_offset(pck);
			if (bo==GF_FILTER_NO_BO) inspect_printf(dump, "N/A");
			else inspect_printf(dump, LLU, bo);
		}
		else if (!strcmp(key, "roll")) inspect_printf(dump, "%d", gf_filter_pck_get_roll_info(pck) );
		else if (!strcmp(key, "crypt")) inspect_printf(dump, "%d", gf_filter_pck_get_crypt_flags(pck) );
		else if (!strcmp(key, "vers")) inspect_printf(dump, "%d", gf_filter_pck_get_carousel_version(pck) );
		else if (!strcmp(key, "size")) inspect_printf(dump, "%d", size );
		else if (!strcmp(key, "csize")) inspect_printf(dump, "%d", pctx->csize);
		else if (!strcmp(key, "crc")) inspect_printf(dump, "0x%08X", gf_crc_32(data, size) );
		else if (!strcmp(key, "lf") || !strcmp(key, "n")) inspect_printf(dump, "\n" );
		else if (!strcmp(key, "t")) inspect_printf(dump, "\t" );
		else if (!strcmp(key, "data")) {
			u32 i;
			if ((pctx->stream_type==GF_STREAM_TEXT) && gf_utf8_is_legal(data, size)) {
				for (i=0; i<size; i++) {
					inspect_printf(dump, "%c", data[i]);
				}
			} else {
				for (i=0; i<size; i++) {
					inspect_printf(dump, "%02X", (unsigned char) data[i]);
				}
			}
		}
		else if (!strcmp(key, "lp")) {
			u8 flags = gf_filter_pck_get_dependency_flags(pck);
			flags >>= 6;
			inspect_printf(dump, "%u", flags);
		}
		else if (!strcmp(key, "depo")) {
			u8 flags = gf_filter_pck_get_dependency_flags(pck);
			flags >>= 4;
			flags &= 0x3;
			inspect_printf(dump, "%u", flags);
		}
		else if (!strcmp(key, "depf")) {
			u8 flags = gf_filter_pck_get_dependency_flags(pck);
			flags >>= 2;
			flags &= 0x3;
			inspect_printf(dump, "%u", flags);
		}
		else if (!strcmp(key, "red")) {
			u8 flags = gf_filter_pck_get_dependency_flags(pck);
			flags &= 0x3;
			inspect_printf(dump, "%u", flags);
		}
		else if (!strcmp(key, "start") || !strcmp(key, "end") || !strcmp(key, "startc") || !strcmp(key, "endc")) {
			u64 ts = gf_filter_pck_get_cts(pck);
			if (ts==GF_FILTER_NO_TS) inspect_printf(dump, "N/A");
			else {
				if (!strcmp(key, "end"))
					ts += gf_filter_pck_get_duration(pck);
				ts *= 1000;
				ts /= gf_filter_pid_get_timescale(pctx->src_pid);

				u32 h, m, s, ms;
				u64 time = ts/1000;
				h = (u32) (time/3600);
				m = (u32) (time/60 - h*60);
				s = (u32) (time - m*60 - h*3660);
				ms = (u32) (ts - 1000*time);
				if (!strcmp(key, "startc") || !strcmp(key, "endc"))
					inspect_printf(dump, "%02d:%02d:%02d,%03d", h, m, s, ms);
				else
					inspect_printf(dump, "%02d:%02d:%02d.%03d", h, m, s, ms);
			}
		}
		else if (!strcmp(key, "ck")) {
			GF_FilterClockType ck_type = gf_filter_pck_get_clock_type(pck);
			inspect_printf(dump, "%d", ck_type );
		} else if (!strcmp(key, "pcr") || !strcmp(key, "pcrd") || !strcmp(key, "pcrc")) {
			u64 clock_val;
			u32 ck_timescale;
			GF_FilterClockType ck_type;
			ck_type = gf_filter_pid_get_clock_info(pctx->src_pid, &clock_val, &ck_timescale);
			if (ck_type) {
				u32 timescale = gf_filter_pck_get_timescale(pck);
				pctx->last_pcr = gf_timestamp_rescale(clock_val, ck_timescale, timescale);
				pctx->last_clock_type = ck_type;
			} else {
				ck_type = pctx->last_clock_type;
			}
			clock_val = pctx->last_pcr;

			if (ck_type) {
				u64 ts = GF_FILTER_NO_TS;
				Bool dump_diff=GF_FALSE;
				u32 timescale = gf_filter_pck_get_timescale(pck);
				clock_val = gf_timestamp_rescale(clock_val, ck_timescale, timescale);
				if (!strcmp(key, "pcrd")) {
					ts = gf_filter_pck_get_dts(pck);
					dump_diff = GF_TRUE;
				}
				else if (!strcmp(key, "pcrc")) {
					ts = gf_filter_pck_get_cts(pck);
					dump_diff = GF_TRUE;
				}
				if (dump_diff) {
					if (ts>clock_val)
						inspect_printf(dump, "-"LLU, ts - clock_val);
					else
						inspect_printf(dump, LLU, clock_val - ts);
				} else {
					inspect_printf(dump, LLU, clock_val);
				}
			} else {
				inspect_printf(dump, "N/A");
			}
		}
		else if (!strncmp(key, "pid.", 4)) {
			const GF_PropertyValue *prop = NULL;
			u32 prop_4cc=0;
			const char *pkey = key+4;
			prop_4cc = gf_props_get_id(pkey);
			if (!prop_4cc && strlen(pkey)==4) prop_4cc = GF_4CC(pkey[0],pkey[1],pkey[2],pkey[3]);

			if (prop_4cc) {
				prop = gf_filter_pid_get_property(pctx->src_pid, prop_4cc);
			}
			if (!prop) prop = gf_filter_pid_get_property_str(pctx->src_pid, key);

			if (prop) {
				inspect_printf(dump, "%s", gf_props_dump(prop_4cc, prop, szDump, (GF_PropDumpDataMode) ctx->dump_data) );
			} else {
				inspect_printf(dump, "N/A");
			}
		}
		else if (!strcmp(key, "fn")) {
			inspect_printf(dump, "%s", gf_filter_get_name(filter) );
		}
		else {
			const GF_PropertyValue *prop = NULL;
			u32 prop_4cc = gf_props_get_id(key);
			if (!prop_4cc && strlen(key)==4) prop_4cc = GF_4CC(key[0],key[1],key[2],key[3]);

			if (prop_4cc) {
				prop = gf_filter_pck_get_property(pck, prop_4cc);
			}
			if (!prop) prop = gf_filter_pck_get_property_str(pck, key);

			if (prop) {
				inspect_printf(dump, "%s", gf_props_dump(prop_4cc, prop, szDump, (GF_PropDumpDataMode) ctx->dump_data) );
			} else {
				inspect_printf(dump, "N/A");
			}
		}

		sep[0] = csep;
		str = sep+1;
	}
}

void gf_m4v_parser_set_inspect(GF_M4VParser *m4v);
u32 gf_m4v_parser_get_obj_type(GF_M4VParser *m4v);

#ifndef GPAC_DISABLE_AV_PARSERS
static const char *get_frame_type_name(u32 ftype)
{
	switch (ftype) {
	case 2: return "B";
	case 1: return "P";
	case 0: return "I";
	default: return "unknown";
	}
}
static void inspect_dump_mpeg124(PidCtx *pctx, char *data, u32 size, FILE *dump)
{
	u8 ftype;
	u32 tinc, o_type;
	u64 fsize, start;
	Bool is_coded, is_m4v=(pctx->codec_id==GF_CODECID_MPEG4_PART2) ? GF_TRUE : GF_FALSE;
	GF_Err e;
	GF_M4VParser *m4v = gf_m4v_parser_new(data, size, !is_m4v);

	gf_m4v_parser_set_inspect(m4v);
	while (1) {
		ftype = 0;
		is_coded = GF_FALSE;
		e = gf_m4v_parse_frame(m4v, &pctx->dsi, &ftype, &tinc, &fsize, &start, &is_coded);
		if (e)
			break;

		o_type = gf_m4v_parser_get_obj_type(m4v);
		if (is_m4v) {
			inspect_printf(dump, "<MPEG4P2VideoObj type=\"0x%02X\"", o_type);
			switch (o_type) {
			/*vosh*/
			case M4V_VOS_START_CODE:
				inspect_printf(dump, " name=\"VOS\" PL=\"%d\"", pctx->dsi.VideoPL);
				break;
			case M4V_VOL_START_CODE:
				inspect_printf(dump, " name=\"VOL\" RAPStream=\"%d\" objectType=\"%d\" par=\"%d/%d\" hasShape=\"%d\"", pctx->dsi.RAP_stream, pctx->dsi.objectType, pctx->dsi.par_num, pctx->dsi.par_den, pctx->dsi.has_shape);
				if (pctx->dsi.clock_rate)
					inspect_printf(dump, " clockRate=\"%d\"", pctx->dsi.clock_rate);
				if (pctx->dsi.time_increment)
					inspect_printf(dump, " timeIncrement=\"%d\"", pctx->dsi.time_increment);
				if (!pctx->dsi.has_shape)
					inspect_printf(dump, " width=\"%d\" height=\"%d\"", pctx->dsi.width, pctx->dsi.height);

				break;

			case M4V_VOP_START_CODE:
				inspect_printf(dump, " name=\"VOP\" RAP=\"%d\" frameType=\"%s\" timeInc=\"%d\" isCoded=\"%d\"", (ftype==1) ? 1 : 0, get_frame_type_name(ftype), tinc, is_coded);
				break;
			case M4V_GOV_START_CODE:
				inspect_printf(dump, " name=\"GOV\"");
				break;
			/*don't interest us*/
			case M4V_UDTA_START_CODE:
				inspect_printf(dump, " name=\"UDTA\"");
				break;

	 		case M4V_VO_START_CODE:
				inspect_printf(dump, " name=\"VO\"");
				break;
	 		case M4V_VISOBJ_START_CODE:
				inspect_printf(dump, " name=\"VisObj\"");
				break;
			default:
				break;
			}
			inspect_printf(dump, "/>\n");
		} else {
			inspect_printf(dump, "<MPEG12VideoObj type=\"0x%02X\"", o_type);
			switch (o_type) {
			case M2V_SEQ_START_CODE:
				inspect_printf(dump, " name=\"SeqStart\" width=\"%d\" height=\"%d\" sar=\"%d/%d\" fps=\"%f\"", pctx->dsi.width, pctx->dsi.height, pctx->dsi.par_num, pctx->dsi.par_den, pctx->dsi.fps);
				break;

			case M2V_EXT_START_CODE:
				inspect_printf(dump, " name=\"SeqStartEXT\" width=\"%d\" height=\"%d\" PL=\"%d\"", pctx->dsi.width, pctx->dsi.height, pctx->dsi.VideoPL);
				break;
			case M2V_PIC_START_CODE:
				inspect_printf(dump, " name=\"PicStart\" frameType=\"%s\" isCoded=\"%d\"", get_frame_type_name(ftype), is_coded);
				break;
			case M2V_GOP_START_CODE:
				inspect_printf(dump, " name=\"GOPStart\"");
				break;
			default:
				break;
			}
			inspect_printf(dump, "/>\n");
		}
	}
	gf_m4v_parser_del(m4v);
}
#endif

static void inspect_format_tmcd_internal(const u8 *data, u32 size, u32 tmcd_flags, u32 tc_num, u32 tc_den, u32 tmcd_fpt, char szFmt[100], GF_BitStream *bs, Bool force_ff, FILE *dump)
{
	u32 h, m, s, f, value;
	Bool neg=GF_FALSE;
	u32 parse_fmt = 1;
	Bool is_drop = GF_FALSE;
	GF_BitStream *loc_bs = NULL;

	if (szFmt)
		szFmt[0] = 0;

	if (!tc_num || !tc_den)
		return;

	if (!bs) {
		loc_bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
		bs = loc_bs;
	} else {
		gf_bs_reassign_buffer(bs, data, size);
	}

	value = gf_bs_read_u32(bs);
	gf_bs_seek(bs, 0);

	if (tmcd_flags & 0x00000001)
		is_drop = GF_TRUE;

	if (tmcd_flags & 0x00000004)
		neg = GF_TRUE;

	if (dump)
		inspect_printf(dump, "<TimeCode");

	if (!force_ff && !(tmcd_flags & 0x00000008)) {
		h = gf_bs_read_u8(bs);
		neg = gf_bs_read_int(bs, 1);
		m = gf_bs_read_int(bs, 7);
		s = gf_bs_read_u8(bs);
		f = gf_bs_read_u8(bs);

		if (tmcd_fpt && (f > tmcd_fpt))
			parse_fmt = 2;
		else if ((m>=60) || (s>=60))
			parse_fmt = 2;
		else
			parse_fmt = 0;
	}

	if (parse_fmt) {
		u64 nb_secs, nb_frames = value;
		neg = GF_FALSE;
		if (parse_fmt==2) force_ff = GF_TRUE;

		if (!force_ff && tmcd_fpt)
			nb_frames *= tmcd_fpt;

		if (is_drop && tc_num) {
			u32 tc_drop_frames, min_10;

			nb_secs = nb_frames;
			nb_secs *= tc_den;
			nb_secs /= tc_num;

			m = (u32) (nb_secs/60);

			min_10 = m / 10;
			m-= min_10;

			tc_drop_frames = m*2;
			nb_frames += tc_drop_frames;
		}

		if (!tmcd_fpt) tmcd_fpt = 30;

		nb_secs = nb_frames / tmcd_fpt;
		h = (u32) nb_secs/3600;
		m = (u32) (nb_secs/60 - h*60);
		s = (u32) (nb_secs - m*60 - h*3600);

		nb_secs *= tmcd_fpt;

		f = (u32) (nb_frames - nb_secs);
		if (tmcd_fpt && (f==tmcd_fpt)) {
			f = 0;
			s++;
			if (s==60) {
				s = 0;
				m++;
				if (m==60) {
					m = 0;
					h++;
				}
			}
		}
	}
	if (dump)
		inspect_printf(dump, " time=\"%s%02d:%02d:%02d%c%02d\"/>\n", neg ? "-" : "", h, m, s, is_drop ? ';' : ':', f);
	else if (szFmt)
		sprintf(szFmt, "%s%02d:%02d:%02d%c%02d", neg ? "-" : "", h, m, s, is_drop ? ';' : ':', f);

	if (loc_bs) gf_bs_del(loc_bs);
}


GF_EXPORT
void gf_inspect_format_timecode(const u8 *data, u32 size, u32 tmcd_flags, u32 tc_num, u32 tc_den, u32 tmcd_fpt, char szFmt[100])
{
	inspect_format_tmcd_internal(data, size, tmcd_flags, tc_num, tc_den, tmcd_fpt, szFmt, NULL, GF_FALSE, NULL);
}

#ifndef GPAC_DISABLE_AV_PARSERS
static void inspect_dump_tmcd(GF_InspectCtx *ctx, PidCtx *pctx, const u8 *data, u32 size, FILE *dump)
{
	inspect_format_tmcd_internal(data, size, pctx->tmcd_flags, pctx->tmcd_rate.num, pctx->tmcd_rate.den, pctx->tmcd_fpt, NULL, pctx->bs, ctx->fftmcd, dump);
}

static void inspect_dump_vpx(GF_InspectCtx *ctx, FILE *dump, u8 *ptr, u64 frame_size, Bool dump_crc, PidCtx *pctx, u32 vpversion)
{
	GF_Err e;
	Bool key_frame = GF_FALSE;
	u32 width = 0, height = 0, renderWidth, renderHeight;
	u32 num_frames_in_superframe = 0, superframe_index_size = 0, i = 0;
	u32 frame_sizes[VP9_MAX_FRAMES_IN_SUPERFRAME];
	gf_bs_reassign_buffer(pctx->bs, ptr, frame_size);
	InspectLogCbk lcbk;

	if (ctx->analyze>=INSPECT_ANALYZE_BS) {
		lcbk.dump = dump;
		lcbk.dump_bits = ctx->analyze==INSPECT_ANALYZE_BS_BITS ? GF_TRUE : GF_FALSE;
		gf_bs_set_logger(pctx->bs, regular_bs_log, &lcbk);
	}

	/*check if it is a superframe*/
	e = gf_vp9_parse_superframe(pctx->bs, frame_size, &num_frames_in_superframe, frame_sizes, &superframe_index_size);

	inspect_printf(dump, "<VP%d%sFrame", vpversion, superframe_index_size ? "Super" : "");
	if (e) {
		inspect_printf(dump, " status=\"error parsing superframe\"/>\n");
		gf_bs_set_logger(pctx->bs, NULL, NULL);
		return;
	}
	if (superframe_index_size)
		inspect_printf(dump, " nb_frames=\"%u\" index_size=\"%u\">\n", num_frames_in_superframe, superframe_index_size);
	else {
		assert(num_frames_in_superframe==1);
	}
	for (i = 0; i < num_frames_in_superframe; ++i) {
		u64 pos2 = gf_bs_get_position(pctx->bs);

		if (superframe_index_size)
			inspect_printf(dump, "<VP%dFrame", vpversion);

		inspect_printf(dump, " size=\"%u\"", frame_sizes[i]);
		if (gf_vp9_parse_sample(pctx->bs, pctx->vpcc, &key_frame, &width, &height, &renderWidth, &renderHeight) != GF_OK) {
			inspect_printf(dump, " status=\"error parsing frame\"/>\n");
			goto exit;
		}
		inspect_printf(dump, " key_frame=\"%u\" width=\"%u\" height=\"%u\" renderWidth=\"%u\" renderHeight=\"%u\"", key_frame, width, height, renderWidth, renderHeight);
		e = gf_bs_seek(pctx->bs, pos2 + frame_sizes[i]);
		if (e) {
			inspect_printf(dump, " status=\"error seeking %s (offset "LLU")\"/>\n", gf_error_to_string(e), pos2 + frame_sizes[i]);
			goto exit;
		}

		inspect_printf(dump, "/>\n");
	}

exit:
	if (superframe_index_size)
		inspect_printf(dump, "</VP%dSuperFrame>\n", vpversion);
	gf_bs_set_logger(pctx->bs, NULL, NULL);
}

static void inspect_dump_ac3_eac3(GF_InspectCtx *ctx, FILE *dump, u8 *ptr, u64 frame_size, Bool dump_crc, PidCtx *pctx, Bool is_ec3)
{
	GF_AC3Header hdr;
	InspectLogCbk lcbk;

	if (!pctx->bs) pctx->bs = gf_bs_new(ptr, frame_size, GF_BITSTREAM_READ);
	else gf_bs_reassign_buffer(pctx->bs, ptr, frame_size);

	if (ctx->analyze>=INSPECT_ANALYZE_BS) {
		lcbk.dump = dump;
		lcbk.dump_bits = ctx->analyze==INSPECT_ANALYZE_BS_BITS ? GF_TRUE : GF_FALSE;
		gf_bs_set_logger(pctx->bs, regular_bs_log, &lcbk);
	}
	inspect_printf(dump, "<%sSample ", is_ec3 ? "EC3" : "AC3");
	memset(&hdr, 0, sizeof(GF_AC3Header));
	gf_ac3_parser_bs(pctx->bs, &hdr, GF_TRUE);
	if (ctx->analyze<INSPECT_ANALYZE_BS) {
		inspect_printf(dump, "bitrate=\"%u\" channels=\"%u\" framesize=\"%u\"", hdr.is_ec3 ? hdr.brcode*1000 : gf_ac3_get_bitrate(hdr.brcode), hdr.streams[0].channels, hdr.framesize);
		if (hdr.is_ec3) {
			inspect_printf(dump, " nb_streams=\"%u\" ", hdr.nb_streams);
		}
	}

	inspect_printf(dump, "/>\n");
	gf_bs_set_logger(pctx->bs, NULL, NULL);
}

#endif /*GPAC_DISABLE_AV_PARSERS*/

static void inspect_dump_packet_as_info(GF_InspectCtx *ctx, FILE *dump, GF_FilterPacket *pck, u32 pid_idx, u64 pck_num, PidCtx *pctx)
{
	u32 size, sap, flag;
	u64 ts, dts;
	GF_FilterClockType ck_type;
	GF_FilterFrameInterface *fifce=NULL;

	if (!ctx->dump_log && !dump) return;

	gf_filter_pck_get_data(pck, &size);
	ck_type = ctx->pcr ? gf_filter_pck_get_clock_type(pck) : 0;
	if (!size && !ck_type) {
		fifce = gf_filter_pck_get_frame_interface(pck);
	}

	inspect_printf(dump, "PID %d PCK "LLU, pid_idx, pck_num);

	if (ck_type) {
		ts = gf_filter_pck_get_cts(pck);
		if (ts!=GF_FILTER_NO_TS)
			inspect_printf(dump, " PCR%s "LLU"\n", (ck_type==GF_FILTER_CLOCK_PCR) ? "" : " discontinuity", ts );
		return;
	}

	ts = gf_filter_pck_get_cts(pck);
	if (ts==GF_FILTER_NO_TS) inspect_printf(dump, " cts N/A");
	else inspect_printf(dump, " cts "LLU, ts);

	dts = gf_filter_pck_get_dts(pck);
	if ((dts!=GF_FILTER_NO_TS) && (dts!= ts)) inspect_printf(dump, " dts "LLU, dts);

	flag = gf_filter_pck_get_duration(pck);
	if (flag) inspect_printf(dump, " dur %u", flag);

	sap = gf_filter_pck_get_sap(pck);
	if (sap==GF_FILTER_SAP_4_PROL) {
		inspect_printf(dump, " sap 4 (prol)");
	} else if (sap) {
		inspect_printf(dump, " sap %u", sap);
	}
	flag = gf_filter_pck_get_interlaced(pck);
	if (flag) inspect_printf(dump, " ilace %u", flag);
	flag = gf_filter_pck_get_corrupted(pck);
	if (flag) inspect_printf(dump, " corr %u", flag);
	flag = gf_filter_pck_get_seek_flag(pck);
	if (flag) inspect_printf(dump, " seek %u", flag);
	flag = gf_filter_pck_get_crypt_flags(pck);
	if (flag) inspect_printf(dump, " crypt %u", flag);
	flag = gf_filter_pck_get_carousel_version(pck);
	if (flag) inspect_printf(dump, " vers %u", flag);

	if (!fifce) inspect_printf(dump, " size %u", size);

	inspect_printf(dump, "\n");
}
static void inspect_dump_packet(GF_InspectCtx *ctx, FILE *dump, GF_FilterPacket *pck, u32 pid_idx, u64 pck_num, PidCtx *pctx)
{
	u32 idx=0, size, sap;
	u64 ts;
	u8 dflags = 0;
	GF_FilterClockType ck_type;
	GF_FilterFrameInterface *fifce=NULL;
	Bool start, end;
	u8 *data;

	if (!ctx->dump_log && !dump) return;

	if (!ctx->deep && !ctx->fmt) return;
	if (!ctx->full) {
		inspect_dump_packet_as_info(ctx, dump, pck, pid_idx, pck_num, pctx);
		return;
	}

	data = (u8 *) gf_filter_pck_get_data(pck, &size);
	gf_filter_pck_get_framing(pck, &start, &end);

	ck_type = ctx->pcr ? gf_filter_pck_get_clock_type(pck) : 0;
	if (!size && !ck_type) {
		fifce = gf_filter_pck_get_frame_interface(pck);
	}

	if (ctx->xml) {
		inspect_printf(dump, "<Packet number=\""LLU"\"", pck_num);
		if (ctx->interleave)
			inspect_printf(dump, " PID=\"%d\"", pid_idx);
	} else {
		inspect_printf(dump, "PID %d PCK "LLU" - ", pid_idx, pck_num);
	}
	if (ck_type) {
		ts = gf_filter_pck_get_cts(pck);
		if (ctx->xml) {
			if (ts==GF_FILTER_NO_TS) inspect_printf(dump, " PCR=\"N/A\"");
			else inspect_printf(dump, " PCR=\""LLU"\" ", ts );
			if (ck_type!=GF_FILTER_CLOCK_PCR) inspect_printf(dump, " discontinuity=\"true\"");
			inspect_printf(dump, "/>");
		} else {
			if (ts==GF_FILTER_NO_TS) inspect_printf(dump, " PCR N/A");
			else inspect_printf(dump, " PCR%s "LLU"\n", (ck_type==GF_FILTER_CLOCK_PCR) ? "" : " discontinuity", ts );
		}
		return;
	}
	if (ctx->xml) {
		if (fifce) inspect_printf(dump, " framing=\"interface\"");
		else if (start && end) inspect_printf(dump, " framing=\"complete\"");
		else if (start) inspect_printf(dump, " framing=\"start\"");
		else if (end) inspect_printf(dump, " framing=\"end\"");
		else inspect_printf(dump, " framing=\"continuation\"");
	} else {
		if (fifce) inspect_printf(dump, "interface");
		else if (start && end) inspect_printf(dump, "full frame");
		else if (start) inspect_printf(dump, "frame start");
		else if (end) inspect_printf(dump, "frame end");
		else inspect_printf(dump, "frame continuation");
	}
	ts = gf_filter_pck_get_dts(pck);
	if (ts==GF_FILTER_NO_TS) DUMP_ATT_STR("dts", "N/A")
	else DUMP_ATT_LLU("dts", ts )

	ts = gf_filter_pck_get_cts(pck);
	if (ts==GF_FILTER_NO_TS) DUMP_ATT_STR("cts", "N/A")
	else DUMP_ATT_LLU("cts", ts )

	DUMP_ATT_U("dur", gf_filter_pck_get_duration(pck) )
	sap = gf_filter_pck_get_sap(pck);
	if (sap==GF_FILTER_SAP_4_PROL) {
		DUMP_ATT_STR(sap, "4 (prol)");
	} else {
		DUMP_ATT_U("sap", gf_filter_pck_get_sap(pck) )
	}
	DUMP_ATT_D("ilace", gf_filter_pck_get_interlaced(pck) )
	DUMP_ATT_D("corr", gf_filter_pck_get_corrupted(pck) )
	DUMP_ATT_D("seek", gf_filter_pck_get_seek_flag(pck) )

	ts = gf_filter_pck_get_byte_offset(pck);
	if (ts==GF_FILTER_NO_BO) DUMP_ATT_STR("bo", "N/A")
	else DUMP_ATT_LLU("bo", ts )

	DUMP_ATT_U("roll", gf_filter_pck_get_roll_info(pck) )
	DUMP_ATT_U("crypt", gf_filter_pck_get_crypt_flags(pck) )
	DUMP_ATT_U("vers", gf_filter_pck_get_carousel_version(pck) )

	if (!fifce) {
		DUMP_ATT_U("size", size )
	}
	dflags = gf_filter_pck_get_dependency_flags(pck);
	DUMP_ATT_U("lp", (dflags>>6) & 0x3 )
	DUMP_ATT_U("depo", (dflags>>4) & 0x3 )
	DUMP_ATT_U("depf", (dflags>>2) & 0x3 )
	DUMP_ATT_U("red", (dflags) & 0x3 )

	if (!data) size = 0;
	if (ctx->dump_data) {
		u32 i;
		DUMP_ATT_STR("data", "")
		for (i=0; i<size; i++) {
			inspect_printf(dump, "%02X", (unsigned char) data[i]);
		}
		if (ctx->xml) inspect_printf(dump, "\"");
	} else if (fifce) {
		u32 i;
		char *name = fifce->get_gl_texture ? "Interface_GLTexID" : "Interface_NumPlanes";
		if (ctx->xml) {
			inspect_printf(dump, " %s=\"", name);
		} else {
			inspect_printf(dump, " %s ", name);
		}
		for (i=0; i<4; i++) {
			if (fifce->get_gl_texture) {
				GF_Matrix mx;
				gf_mx_init(mx);
				u32 gl_tex_format, gl_tex_id;
				if (fifce->get_gl_texture(fifce, i, &gl_tex_format, &gl_tex_id, &mx) != GF_OK)
					break;
				if (i) inspect_printf(dump, ",");
				inspect_printf(dump, "%d", gl_tex_id);
			} else {
				u32 stride;
				const u8 *plane;
				if (fifce->get_plane(fifce, i, &plane, &stride) != GF_OK)
					break;
			}
		}
		if (!fifce->get_gl_texture) {
			inspect_printf(dump, "%d", i);
		}

		if (ctx->xml) {
			inspect_printf(dump, "\"");
		}
	} else if (data && (ctx->test!=INSPECT_TEST_NOCRC) ) {
		DUMP_ATT_X("CRC32", gf_crc_32(data, size) )
	}
	if (ctx->xml) {
		if (!ctx->props) goto props_done;

	} else {
		inspect_printf(dump, "\n");
	}

	if (!ctx->props) return;
	while (1) {
		u32 prop_4cc;
		const char *prop_name;
		const GF_PropertyValue * p = gf_filter_pck_enum_properties(pck, &idx, &prop_4cc, &prop_name);
		if (!p) break;
		if (idx==0) inspect_printf(dump, "properties:\n");

		inspect_dump_property(ctx, dump, prop_4cc, prop_name, p, pctx);
	}


props_done:
	if (!ctx->analyze || !size) {
		if (ctx->xml) {
			inspect_printf(dump, "/>\n");
		}
		return;
	}
	inspect_printf(dump, ">\n");

#ifndef GPAC_DISABLE_AV_PARSERS
	if (pctx->hevc_state || pctx->avc_state || pctx->vvc_state) {
		idx=1;

		if (pctx->is_adobe_protected) {
			u8 encrypted_au = data[0];
			if (encrypted_au) {
				inspect_printf(dump, "   <!-- Packet is an Adobe's protected frame and can not be dumped -->\n");
				inspect_printf(dump, "</Packet>\n");
				return;
			}
			else {
				data++;
				size--;
			}
		}
		while (size) {
			u32 nal_size = inspect_get_nal_size((char*)data, pctx->nalu_size_length);
			data += pctx->nalu_size_length;

			if (pctx->nalu_size_length + nal_size > size) {
				inspect_printf(dump, "   <!-- NALU is corrupted: size is %d but only %d remains -->\n", nal_size, size);
				break;
			} else {
				inspect_printf(dump, "   <NALU size=\"%d\" ", nal_size);
				gf_inspect_dump_nalu_internal(dump, data, nal_size, pctx->has_svcc ? 1 : 0, pctx->hevc_state, pctx->avc_state, pctx->vvc_state, pctx->nalu_size_length, ctx->crc, pctx->is_cenc_protected, ctx->analyze, pctx);
			}
			idx++;
			data += nal_size;
			size -= nal_size + pctx->nalu_size_length;
		}
	} else if (pctx->av1_state) {
		gf_bs_reassign_buffer(pctx->bs, data, size);
		while (size) {
			ObuType obu_type = 0;
			u64 obu_size = 0;
			u32 hdr_size = 0;

			obu_size = gf_inspect_dump_obu_internal(dump, pctx->av1_state, (char *) data, obu_size, obu_type, obu_size, hdr_size, ctx->crc, pctx, ctx->analyze);

			if (obu_size > size) {
				inspect_printf(dump, "   <!-- OBU is corrupted: size is %d but only %d remains -->\n", (u32) obu_size, size);
				break;
			}
			data += obu_size;
			size -= (u32)obu_size;
			idx++;
		}
	} else {
		u32 hdr, pos, fsize, i;
		u32 dflag=0;
		switch (pctx->codec_id) {
		case GF_CODECID_MPEG1:
		case GF_CODECID_MPEG2_422:
		case GF_CODECID_MPEG2_SNR:
		case GF_CODECID_MPEG2_HIGH:
		case GF_CODECID_MPEG2_MAIN:
		case GF_CODECID_MPEG4_PART2:
			inspect_dump_mpeg124(pctx, (char *) data, size, dump);
			break;
		case GF_CODECID_MPEG_AUDIO:
		case GF_CODECID_MPEG2_PART3:
		case GF_CODECID_MPEG_AUDIO_L1:
			pos = 0;
			while (size) {
				hdr = gf_mp3_get_next_header_mem(data, size, &pos);
				fsize = gf_mp3_frame_size(hdr);
				inspect_printf(dump, "<MPEGAudioFrame size=\"%d\" layer=\"%d\" version=\"%d\" bitrate=\"%d\" channels=\"%d\" samplesPerFrame=\"%d\" samplerate=\"%d\"/>\n", fsize, gf_mp3_layer(hdr), gf_mp3_version(hdr), gf_mp3_bit_rate(hdr), gf_mp3_num_channels(hdr), gf_mp3_window_size(hdr), gf_mp3_sampling_rate(hdr));
				if (size<pos+fsize) break;
				data += pos + fsize;
				size -= pos + fsize;
			}
			break;
		case GF_CODECID_TMCD:
			inspect_dump_tmcd(ctx, pctx, (char *) data, size, dump);
			break;
		case GF_CODECID_SUBS_TEXT:
		case GF_CODECID_META_TEXT:
			dflag=1;
		case GF_CODECID_SUBS_XML:
		case GF_CODECID_META_XML:
			if (dflag)
				inspect_printf(dump, "<![CDATA[");
			for (i=0; i<size; i++) {
				gf_fputc(data[i], dump);
			}
			if (dflag)
				inspect_printf(dump, "]]>\n");
			break;
		case GF_CODECID_APCH:
		case GF_CODECID_APCO:
		case GF_CODECID_APCN:
		case GF_CODECID_APCS:
		case GF_CODECID_AP4X:
		case GF_CODECID_AP4H:
			gf_inspect_dump_prores_internal(dump, (char *) data, size, ctx->crc, pctx);
			break;

		case GF_CODECID_MPHA:
			gf_bs_reassign_buffer(pctx->bs, data, size);
			gf_inspect_dump_mha_frame(dump, pctx->bs, "");
			break;

		case GF_CODECID_MHAS:
			gf_inspect_dump_mhas(dump, (char *) data, size, ctx->crc, pctx);
			break;
		case GF_CODECID_VP8:
			dflag=1;
		case GF_CODECID_VP9:
			inspect_dump_vpx(ctx, dump, (char *) data, size, ctx->crc, pctx, dflag ? 8 : 9);
			break;
		case GF_CODECID_AC3:
			dflag=1;
		case GF_CODECID_EAC3:
			inspect_dump_ac3_eac3(ctx, dump, (char *) data, size, ctx->crc, pctx, dflag ? 0 : 1);
			break;
		case GF_CODECID_TRUEHD:
			gf_bs_reassign_buffer(pctx->bs, data, size);
			gf_inspect_dump_truehd_frame(dump, pctx->bs);
			break;
		case GF_CODECID_OPUS:
			gf_inspect_dump_opus_internal(dump, data, size, 0, ctx->crc, pctx);
			break;
		case GF_CODECID_ALAC:
		{
			gf_bs_reassign_buffer(pctx->bs, data, size);
			u32 val, partial;

#define get_and_print(name, bits) \
			val = gf_bs_read_int(pctx->bs, bits); \
			inspect_printf(dump, " "name"=\"%u\"", val);

			inspect_printf(dump, " <ALACSegment");

			get_and_print("type", 3);
			get_and_print("reserved", 12);
			get_and_print("partial", 1);
			partial=val;
			get_and_print("shift_off", 2);
			get_and_print("escape", 1);
			if (partial) {
				get_and_print("frameLength", 32);
			}
			inspect_printf(dump, "/>\n");

#undef get_and_print

		}
			break;

		}
	}
#endif
	inspect_printf(dump, "</Packet>\n");
}

#define DUMP_ARRAY(arr, name, loc, _is_svc)\
	if (arr && gf_list_count(arr)) {\
		inspect_printf(dump, "  <%sArray location=\"%s\">\n", name, loc);\
		for (i=0; i<gf_list_count(arr); i++) {\
			slc = gf_list_get(arr, i);\
			inspect_printf(dump, "   <NALU size=\"%d\" ", slc->size);\
			gf_inspect_dump_nalu_internal(dump, slc->data, slc->size, _is_svc, pctx->hevc_state, pctx->avc_state, pctx->vvc_state, nalh_size, ctx->crc, GF_FALSE, ctx->analyze, pctx);\
		}\
		inspect_printf(dump, "  </%sArray>\n", name);\
	}\


#ifndef GPAC_DISABLE_AV_PARSERS
static void inspect_reset_parsers(PidCtx *pctx, void *keep_parser_address)
{
	if ((&pctx->hevc_state != keep_parser_address) && pctx->hevc_state) {
		gf_free(pctx->hevc_state);
		pctx->hevc_state = NULL;
	}
	if ((&pctx->avc_state != keep_parser_address) && pctx->avc_state) {
		gf_free(pctx->avc_state);
		pctx->avc_state = NULL;
	}
	if ((&pctx->vvc_state != keep_parser_address) && pctx->vvc_state) {
		gf_free(pctx->vvc_state);
		pctx->vvc_state = NULL;
	}
	if ((&pctx->av1_state != keep_parser_address) && pctx->av1_state) {
		if (pctx->av1_state->config) gf_odf_av1_cfg_del(pctx->av1_state->config);
		gf_free(pctx->av1_state);
		pctx->av1_state = NULL;
	}
	if ((&pctx->mv124_state != keep_parser_address) && pctx->mv124_state) {
		gf_m4v_parser_del(pctx->mv124_state);
		pctx->mv124_state = NULL;
	}
	if (pctx->vpcc) gf_odf_vp_cfg_del(pctx->vpcc);
}
#endif

static void format_duration(s64 dur, u64 timescale, FILE *dump)
{
	u32 h, m, s, ms;
	const char *name = "duration";
	if (dur==-1) {
		inspect_printf(dump, " duration unknown");
		return;
	}
	//duration probing was disabled
	if (!timescale)
		return;
	if (dur<0) {
		dur = -dur;
		name = "estimated duration";
	}

	dur = (u64) (( ((Double) (s64) dur)/timescale)*1000);
	h = (u32) (dur / 3600000);
	m = (u32) (dur/ 60000) - h*60;
	s = (u32) (dur/1000) - h*3600 - m*60;
	ms = (u32) (dur) - h*3600000 - m*60000 - s*1000;
	if (h<=24) {
		if (h)
			inspect_printf(dump, " %s %02d:%02d:%02d.%03d", name, h, m, s, ms);
		else
			inspect_printf(dump, " %s %02d:%02d.%03d", name, m, s, ms);
	} else {
		u32 d = (u32) (dur / 3600000 / 24);
		h = (u32) (dur/3600000)-24*d;
		if (d<=365) {
			inspect_printf(dump, " %s %d Days, %02d:%02d:%02d.%03d", name, d, h, m, s, ms);
		} else {
			u32 y=0;
			while (d>365) {
				y++;
				d-=365;
				if (y%4) d--;
			}
			inspect_printf(dump, " %s %d Years %d Days, %02d:%02d:%02d.%03d", name, y, d, h, m, s, ms);
		}
	}
}


static void inspect_dump_pid_as_info(GF_InspectCtx *ctx, FILE *dump, GF_FilterPid *pid, u32 pid_idx, Bool is_connect, Bool is_remove, u64 pck_for_config, Bool is_info, PidCtx *pctx)
{
	const GF_PropertyValue *p, *dsi, *dsi_enh;
	Bool is_raw=GF_FALSE;
	Bool is_protected=GF_FALSE;
	u32 codec_id=0;

	if (!ctx->dump_log && !dump) return;

	inspect_printf(dump, "PID");
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ID);
	if (!p) p = gf_filter_pid_get_property(pid, GF_PROP_PID_ESID);
	if (p) inspect_printf(dump, " %d", p->value.uint);

	if (is_remove) {
		inspect_printf(dump, " removed\n");
		return;
	}
	if (!is_connect) {
		inspect_printf(dump, " reconfigure");
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	if (p) {
		if (p->value.uint==GF_STREAM_ENCRYPTED) {
			is_protected = GF_TRUE;
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_ORIG_STREAM_TYPE);
		}
		if (p)
			inspect_printf(dump, " %s", gf_stream_type_short_name(p->value.uint));
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_ID);
	if (p) {
		inspect_printf(dump, " service %d", p->value.uint);
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_NAME);
		if (p) inspect_printf(dump, " \"%s\"", p->value.string);
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SERVICE_PROVIDER);
		if (p) inspect_printf(dump, " (%s)", p->value.string);
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DISABLED);
	if (p && p->value.boolean) inspect_printf(dump, " disabled");

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_ISOM_TRACK_FLAGS);
	if (p && p->value.uint) {
		if (p->value.uint & (1<<1)) inspect_printf(dump, " inMovie");
		if (p->value.uint & (1<<2)) inspect_printf(dump, " inPreview");
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_LANGUAGE);
	if (p && stricmp(p->value.string, "und")) inspect_printf(dump, " language \"%s\"", p->value.string);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DURATION);
	if (p) format_duration((s64) p->value.lfrac.num, (u32) p->value.lfrac.den, dump);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_TIMESCALE);
	if (p) inspect_printf(dump, " timescale %d", p->value.uint);
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DELAY);
	if (p) inspect_printf(dump, " delay "LLD, p->value.longsint);

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (p) {
		if (p->value.uint==GF_CODECID_RAW) {
			is_raw = GF_TRUE;
			inspect_printf(dump, " raw");
		} else {
			codec_id = p->value.uint;
			//fprintf(dump, " codec \"%s\"", gf_codecid_name(codec_id));
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) {
		inspect_printf(dump, " %dx", p->value.uint);
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
		inspect_printf(dump, "%d", p ? p->value.uint  : 0);
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_FPS);
		if (p) {
			u32 tscale = p->value.frac.num;
			u32 sdur = p->value.frac.den;
			gf_media_get_reduced_frame_rate(&tscale, &sdur);
			inspect_printf(dump, " fps %d", tscale);
			if (sdur!=1) inspect_printf(dump, "/%u", sdur);
		}

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAR);
		if (p && p->value.frac.num!=p->value.frac.den)
			inspect_printf(dump, " SAR %d/%u", p->value.frac.num, p->value.frac.den);
		else
			inspect_printf(dump, " SAR 1/1");
		if (is_raw) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_PIXFMT);
			if (p) inspect_printf(dump, " raw format %s", gf_pixel_fmt_name(p->value.uint) );
		}
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) {
		inspect_printf(dump, " %d Hz", p->value.uint);
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_CHANNEL_LAYOUT);
		if (p) {
			inspect_printf(dump, " %s", gf_audio_fmt_get_layout_name(p->value.longuint));
		} else {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
			if (p) inspect_printf(dump, " %d channels", p->value.uint);
		}
		if (is_raw) {
			p = gf_filter_pid_get_property(pid, GF_PROP_PID_AUDIO_FORMAT);
			if (p) inspect_printf(dump, " raw format %s", gf_audio_fmt_name(p->value.uint) );
		}
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_BITRATE);
	if (p) {
		if (p->value.uint<5000) inspect_printf(dump, " %d bps", p->value.uint );
		else inspect_printf(dump, " %d kbps", p->value.uint/1000 );
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NB_FRAMES);
	if (p && (p->value.uint>1)) inspect_printf(dump, " %d frames", p->value.uint);


	if (is_protected) {
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROTECTION_SCHEME_TYPE);
		inspect_printf(dump, " encryption %s", p ? gf_4cc_to_str(p->value.uint) : "unknown");
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_CENC_PATTERN);
		if (p)
			inspect_printf(dump, " %d/%d pattern", p->value.frac.num, p->value.frac.den);
	}

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	dsi_enh = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);
	if (is_raw) {
		inspect_printf(dump, "\n");
		return;
	}

	inspect_printf(dump, " codec");

#ifndef GPAC_DISABLE_AV_PARSERS
	if ((codec_id==GF_CODECID_HEVC) || (codec_id==GF_CODECID_LHVC) || (codec_id==GF_CODECID_HEVC_TILES)) {
		u32 i, j, k;
		HEVCState *hvcs = NULL;
		GF_HEVCConfig *hvcc=NULL;
		if (dsi) {
			hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, (codec_id==GF_CODECID_LHVC) ? GF_TRUE : GF_FALSE);
			if (dsi_enh) {
				GF_SAFEALLOC(hvcs, HEVCState);
				for (i=0; i<gf_list_count(hvcc->param_array); i++) {
					GF_NALUFFParamArray *pa = gf_list_get(hvcc->param_array, i);
					for (j=0; j<gf_list_count(pa->nalus); j++) {
						GF_NALUFFParam *sl = gf_list_get(pa->nalus, j);
						u8 nut, lid, tid;
						gf_hevc_parse_nalu(sl->data, sl->size, hvcs, &nut, &tid, &lid);
					}
				}
			}
		} else if (dsi_enh) {
			hvcc = gf_odf_hevc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size, GF_TRUE);
		}
		inspect_printf(dump, " %s", (codec_id==GF_CODECID_LHVC) ? "L-HEVC" : "HEVC");
		if (codec_id==GF_CODECID_HEVC_TILES) inspect_printf(dump, " tile");

		if (hvcc) {
			if (hvcc->interlaced_source_flag)
				inspect_printf(dump, " interlaced");
			if (!hvcc->is_lhvc) {
				inspect_printf(dump, " PL %s@%g %s %d bpp compatibility 0x%08X", gf_hevc_get_profile_name(hvcc->profile_idc), ((Double)hvcc->level_idc) / 30.0, gf_avc_hevc_get_chroma_format_name(hvcc->chromaFormat), hvcc->luma_bit_depth, hvcc->general_profile_compatibility_flags);
			}
			gf_odf_hevc_cfg_del(hvcc);
		}
		if (dsi && dsi_enh) {
			hvcc = gf_odf_hevc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size, GF_TRUE);
			if (hvcc) {
				inspect_printf(dump, " scalable");
				for (k=0; k<2; k++) {
				u32 ntype = k ? GF_HEVC_NALU_SEQ_PARAM : GF_HEVC_NALU_VID_PARAM;
				for (i=0; i<gf_list_count(hvcc->param_array); i++) {
					GF_NALUFFParamArray *pa = gf_list_get(hvcc->param_array, i);
					if (pa->type!=ntype) continue;
					for (j=0; j<gf_list_count(pa->nalus); j++) {
						GF_NALUFFParam *sl = gf_list_get(pa->nalus, j);
						u8 nut, lid, tid;
						s32 idx = gf_hevc_parse_nalu(sl->data, sl->size, hvcs, &nut, &tid, &lid);
						if ((idx>=0) && (pa->type==GF_HEVC_NALU_SEQ_PARAM)) {
							idx = hvcs->last_parsed_sps_id;
							inspect_printf(dump, " %dx%d", hvcs->sps[idx].width, hvcs->sps[idx].height);
						}
					}
				}
				}
				gf_odf_hevc_cfg_del(hvcc);
			}
		}
		if (hvcs) gf_free(hvcs);
	}
	else if ((codec_id==GF_CODECID_AVC) || (codec_id==GF_CODECID_SVC) || (codec_id==GF_CODECID_MVC)) {
		GF_AVCConfig *avcc=NULL;
		if (dsi) {
			avcc = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		} else if (dsi_enh) {
			avcc = gf_odf_avc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size);
		}
		if (codec_id==GF_CODECID_AVC) inspect_printf(dump, " %s", "AVC|H264");
		else if (codec_id==GF_CODECID_SVC) inspect_printf(dump, " %s", "SVC");
		else if (codec_id==GF_CODECID_MVC) inspect_printf(dump, " %s", "MVC");

		if (avcc) {
			inspect_printf(dump, " PL %s@%g %s %d bpp", gf_avc_get_profile_name(avcc->AVCProfileIndication), ((Double)avcc->AVCLevelIndication)/10.0, gf_avc_hevc_get_chroma_format_name(avcc->chroma_format), avcc->luma_bit_depth );
			gf_odf_avc_cfg_del(avcc);
		}
		if (dsi && dsi_enh) {
			inspect_printf(dump, " scalable");
			avcc = gf_odf_avc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size);
			if (avcc) {
				AVCState *avcs;
				GF_SAFEALLOC(avcs, AVCState);
				for (u32 i=0; i<gf_list_count(avcc->sequenceParameterSets); i++) {
					GF_NALUFFParam *sl = gf_list_get(avcc->sequenceParameterSets, i);
					s32 idx = gf_avc_read_sps(sl->data, sl->size, avcs, 0, NULL);
					if (idx>=0) inspect_printf(dump, " %dx%d", avcs->sps[idx].width, avcs->sps[idx].height);
				}
				gf_free(avcs);
				gf_odf_avc_cfg_del(avcc);
			}
		}
	}
	else if ((codec_id==GF_CODECID_VVC) || (codec_id==GF_CODECID_VVC_SUBPIC)) {
		GF_VVCConfig *vvcc=NULL;
		if (dsi) {
			vvcc = gf_odf_vvc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		} else if (dsi_enh) {
			vvcc = gf_odf_vvc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size);
		}
		if (codec_id==GF_CODECID_VVC) inspect_printf(dump, " VVC%s", (codec_id==GF_CODECID_VVC) ? "" : " subpictures");

		if (vvcc) {
			inspect_printf(dump, " PL %s@%.1g %s %d bpp", gf_vvc_get_profile_name(vvcc->general_profile_idc), ((Double)vvcc->general_level_idc)/16.0, gf_avc_hevc_get_chroma_format_name(vvcc->chroma_format), vvcc->bit_depth );
			if (vvcc->numTemporalLayers>1)
			inspect_printf(dump, " %d sublayers", vvcc->numTemporalLayers);

			gf_odf_vvc_cfg_del(vvcc);
		}
	}
	else if (codec_id==GF_CODECID_AV1) {
		GF_AV1Config *av1c = NULL;
		if (dsi)
			av1c = gf_odf_av1_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		inspect_printf(dump, " AV1");
		if (av1c) {
			inspect_printf(dump, " PL %d@%d", av1c->seq_profile, av1c->seq_level_idx_0);
			if (av1c->chroma_subsampling_x) {
				if (av1c->chroma_subsampling_y) inspect_printf(dump, " YUV 4:2:0");
				else inspect_printf(dump, " YUV 4:2:2");
			} else inspect_printf(dump, " YUV 4:4:4");
			if (av1c->monochrome) inspect_printf(dump, " monochrome");
			if (av1c->twelve_bit) inspect_printf(dump, " 12 bpp");

			gf_odf_av1_cfg_del(av1c);
		}
	}
	else if ((codec_id==GF_CODECID_AAC_MPEG4) || (codec_id==GF_CODECID_AAC_MPEG2_MP) || (codec_id==GF_CODECID_AAC_MPEG2_LCP) || (codec_id==GF_CODECID_AAC_MPEG2_SSRP)) {
		if (dsi) {
			const char *name, *sep;
			GF_M4ADecSpecInfo a_cfg;
			gf_m4a_get_config(dsi->value.data.ptr, dsi->value.data.size, &a_cfg);
			char *signaling = "implicit";
			char *heaac = "";
			if ((codec_id==GF_CODECID_AAC_MPEG4) && a_cfg.has_sbr) {
				if (a_cfg.has_ps) heaac = " HEAACv2";
				else heaac = " HEAACv1";
			}
			if (a_cfg.base_object_type==2) {
				if (a_cfg.has_ps || a_cfg.has_sbr)
					signaling = "backward compatible";
			} else {
				signaling = "hierarchical";
			}
			name = gf_m4a_object_type_name(a_cfg.base_object_type);
			sep = strstr(name, "Audio");
			if (sep) name = sep+6;

			inspect_printf(dump, " %s (aot=%d %s)%s", name, a_cfg.base_object_type, signaling, heaac);
			if (a_cfg.has_sbr) inspect_printf(dump, " SBR %d Hz aot %s", a_cfg.sbr_sr, gf_m4a_object_type_name(a_cfg.sbr_object_type));
			if (a_cfg.has_ps) inspect_printf(dump, " PS");
		} else {
			inspect_printf(dump, " AAC");
		}
	} else if (codec_id==GF_CODECID_MPEG4_PART2) {
		inspect_printf(dump, " MPEG-4 Visual");
		if (dsi) {
			GF_M4VDecSpecInfo vcfg;
			gf_m4v_get_config(dsi->value.data.ptr, dsi->value.data.size, &vcfg);
			inspect_printf(dump, " PL %s", gf_m4v_get_profile_name(vcfg.VideoPL));
			if (vcfg.chroma_fmt) inspect_printf(dump, " %s", gf_avc_hevc_get_chroma_format_name(vcfg.chroma_fmt) );
			if (!vcfg.progresive) inspect_printf(dump, " interlaced");
		}
	} else
#endif//GPAC_DISABLE_AV_PARSERS
	{
		if (codec_id==GF_CODECID_FFMPEG) {
			p = gf_filter_pid_get_property_str(pid, "ffmpeg:codec");
			if (p) {
				inspect_printf(dump, " %s", p->value.string);
			} else {
				p = gf_filter_pid_get_property(pid, GF_PROP_PID_FFMPEG_CODEC_ID);
				if (p && (p->type==GF_PROP_UINT)) codec_id = p->value.uint;
				inspect_printf(dump, " FFMPEG %d", codec_id);
			}
		} else {
			inspect_printf(dump, " %s", gf_codecid_name(codec_id));
		}
		p = gf_filter_pid_get_property(pid, GF_PROP_PID_PROFILE_LEVEL);
		if (p) inspect_printf(dump, " PL %d", p->value.uint);

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_COLR_CHROMAFMT);
		if (p) inspect_printf(dump, " %s", gf_avc_hevc_get_chroma_format_name(p->value.uint) );

		p = gf_filter_pid_get_property(pid, GF_PROP_PID_INTERLACED);
		if (p && p->value.boolean) inspect_printf(dump, " interlaced");
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CLAP_W);
	u32 clap_w = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CLAP_H);
	u32 clap_h = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CLAP_X);
	s32 clap_x = p ? p->value.sint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CLAP_Y);
	s32 clap_y = p ? p->value.sint : 0;
	if (clap_w && clap_h) {
		inspect_printf(dump, " clap %dx%d@%ux%u", clap_x, clap_y, clap_w, clap_h);
	}
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_DOLBY_VISION);
	if (p) {
		GF_BitStream *bs = gf_bs_new(p->value.data.ptr, p->value.data.size, GF_BITSTREAM_READ);
		GF_DOVIDecoderConfigurationRecord *dovi = gf_odf_dovi_cfg_read_bs(bs);
		gf_bs_del(bs);
		if (dovi) {
			inspect_printf(dump, " DolbyVision %d.%d PL %d@%d",
				dovi->dv_version_major, dovi->dv_version_minor, dovi->dv_profile, dovi->dv_level);
			if (dovi->rpu_present_flag) inspect_printf(dump, " rpu");
			if (dovi->bl_present_flag) inspect_printf(dump, " base");
			if (dovi->el_present_flag) inspect_printf(dump, " el");
			if (dovi->dv_bl_signal_compatibility_id) inspect_printf(dump, " compatID %d", dovi->dv_bl_signal_compatibility_id);
			gf_odf_dovi_cfg_del(dovi);
		}
	}

	if (dsi && (codec_id==GF_CODECID_EAC3)) {
		GF_AC3Config ac3cfg;
		gf_odf_ac3_config_parse(dsi->value.data.ptr, dsi->value.data.size, GF_TRUE, &ac3cfg);
		if (ac3cfg.atmos_ec3_ext)
			inspect_printf(dump, " Atmos (CIT %d)", ac3cfg.complexity_index_type);
	}


	inspect_printf(dump, "\n");
}

static void inspect_dump_pid(GF_InspectCtx *ctx, FILE *dump, GF_FilterPid *pid, u32 pid_idx, Bool is_connect, Bool is_remove, u64 pck_for_config, Bool is_info, PidCtx *pctx)
{
	u32 idx=0, nalh_size, i;
#ifndef GPAC_DISABLE_AV_PARSERS
	GF_NALUFFParam *slc;
#endif
	GF_AVCConfig *avcc, *svcc;
	GF_HEVCConfig *hvcc, *lhcc;
	GF_VVCConfig *vvcC;
	Bool is_enh = GF_FALSE;
	Bool is_svc=GF_FALSE;
	char *elt_name = NULL;
	const GF_PropertyValue *p, *dsi, *dsi_enh;

	if (ctx->test==INSPECT_TEST_NOPROP) return;
	if (!ctx->dump_log && !dump) return;

	if (!ctx->full) {
		inspect_dump_pid_as_info(ctx, dump, pid, pid_idx, is_connect, is_remove, pck_for_config, is_info, pctx);
		return;
	}

	//disconnect of src pid (not yet supported)
	if (ctx->xml) {
		if (is_info) {
			elt_name = "PIDInfoUpdate";
		} else if (is_remove) {
			elt_name = "PIDRemove";
		} else {
			elt_name = is_connect ? "PIDConfigure" : "PIDReconfigure";
		}
		inspect_printf(dump, "<%s PID=\"%d\" name=\"%s\"",elt_name, pid_idx, gf_filter_pid_get_name(pid) );

		if (pck_for_config)
			inspect_printf(dump, " packetsSinceLastConfig=\""LLU"\"", pck_for_config);
	} else {
		if (is_info) {
			inspect_printf(dump, "PID %d name %s info update\n", pid_idx, gf_filter_pid_get_name(pid) );
		} else if (is_remove) {
			inspect_printf(dump, "PID %d name %s delete\n", pid_idx, gf_filter_pid_get_name(pid) );
		} else {
			inspect_printf(dump, "PID %d name %s %sonfigure", pid_idx, gf_filter_pid_get_name(pid), is_connect ? "C" : "Rec" );
			if (pck_for_config)
				inspect_printf(dump, " after "LLU" packets", pck_for_config);
			inspect_printf(dump, " - properties:\n");
		}
	}

	if (!is_info) {
		while (1) {
			u32 prop_4cc;
			const char *prop_name;
			p = gf_filter_pid_enum_properties(pid, &idx, &prop_4cc, &prop_name);
			if (!p) break;
			inspect_dump_property(ctx, dump, prop_4cc, prop_name, p, pctx);

			if (prop_name) {
				if (!strcmp(prop_name, "tmcd:flags"))
					pctx->tmcd_flags = p->value.uint;
				else if (!strcmp(prop_name, "tmcd:framerate"))
					pctx->tmcd_rate = p->value.frac;
				else if (!strcmp(prop_name, "tmcd:frames_per_tick"))
					pctx->tmcd_fpt = p->value.uint;

			}
		}
	} else {
		while (ctx->info) {
			u32 prop_4cc;
			const char *prop_name;
			p = gf_filter_pid_enum_info(pid, &idx, &prop_4cc, &prop_name);
			if (!p) break;
			inspect_dump_property(ctx, dump, prop_4cc, prop_name, p, pctx);
		}
	}

	if (!ctx->analyze) {
		if (ctx->xml) {
			inspect_printf(dump, "/>\n");
		}
		return;
	}

	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	if (!p) {
		inspect_printf(dump, "/>\n");
		return;
	}
	pctx->codec_id = p->value.uint;

	dsi = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG);
	dsi_enh = gf_filter_pid_get_property(pid, GF_PROP_PID_DECODER_CONFIG_ENHANCEMENT);

	avcc = NULL;
	svcc = NULL;
	hvcc = NULL;
	lhcc = NULL;
	vvcC = NULL;
	pctx->has_svcc = 0;

	switch (pctx->codec_id) {
	case GF_CODECID_SVC:
	case GF_CODECID_MVC:
		if (!dsi) is_enh = GF_TRUE;
	case GF_CODECID_AVC:
	case GF_CODECID_AVC_PS:
		if (!dsi && !dsi_enh) {
			inspect_printf(dump, "/>\n");
			return;
		}
#ifndef GPAC_DISABLE_AV_PARSERS
		inspect_reset_parsers(pctx, &pctx->avc_state);

		if (!pctx->avc_state) {
			GF_SAFEALLOC(pctx->avc_state, AVCState);
			if (!pctx->avc_state) return;
		}
#endif
		inspect_printf(dump, ">\n");
		inspect_printf(dump, "<AVCParameterSets>\n");
		if (dsi) {
			if (is_enh) {
				svcc = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
				if (svcc)
					pctx->nalu_size_length = svcc->nal_unit_size;
			} else {
				avcc = gf_odf_avc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
				if (avcc)
					pctx->nalu_size_length = avcc->nal_unit_size;
			}
		}
		if (dsi_enh && !svcc) {
			svcc = gf_odf_avc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size);
			if (svcc)
				pctx->nalu_size_length = svcc->nal_unit_size;
		}
		nalh_size = pctx->nalu_size_length;
		is_svc = (svcc != NULL) ? 1 : 0;
#ifndef GPAC_DISABLE_AV_PARSERS
		if (avcc) {
			DUMP_ARRAY(avcc->sequenceParameterSets, "AVCSPS", "decoderConfig", is_svc)
			DUMP_ARRAY(avcc->pictureParameterSets, "AVCPPS", "decoderConfig", is_svc)
			DUMP_ARRAY(avcc->sequenceParameterSetExtensions, "AVCSPSEx", "decoderConfig", is_svc)
		}
		if (is_svc) {
			DUMP_ARRAY(svcc->sequenceParameterSets, "SVCSPS", dsi_enh ? "decoderConfigEnhancement" : "decoderConfig", is_svc)
			DUMP_ARRAY(svcc->pictureParameterSets, "SVCPPS", dsi_enh ? "decoderConfigEnhancement" : "decoderConfig", is_svc)
			pctx->has_svcc = 1;
		}
#endif
		inspect_printf(dump, "</AVCParameterSets>\n");
		break;
	case GF_CODECID_LHVC:
		is_enh = GF_TRUE;
	case GF_CODECID_HEVC:
	case GF_CODECID_HEVC_TILES:
		if (!dsi && !dsi_enh) {
			inspect_printf(dump, "/>\n");
			return;
		}

#ifndef GPAC_DISABLE_AV_PARSERS
		inspect_reset_parsers(pctx, &pctx->hevc_state);

		if (!pctx->hevc_state) {
			GF_SAFEALLOC(pctx->hevc_state, HEVCState);
			if (!pctx->hevc_state) return;
		}
#endif

		if (dsi) {
			if (is_enh && !dsi_enh) {
				lhcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_TRUE);
				if (lhcc)
					pctx->nalu_size_length = lhcc->nal_unit_size;
			} else {
				hvcc = gf_odf_hevc_cfg_read(dsi->value.data.ptr, dsi->value.data.size, GF_FALSE);
				if (hvcc)
					pctx->nalu_size_length = hvcc->nal_unit_size;
			}
		}
		if (dsi_enh && !lhcc) {
			lhcc = gf_odf_hevc_cfg_read(dsi_enh->value.data.ptr, dsi_enh->value.data.size, GF_TRUE);
			if (lhcc)
				pctx->nalu_size_length = lhcc->nal_unit_size;
		}
		nalh_size = pctx->nalu_size_length;

		inspect_printf(dump, ">\n");
		inspect_printf(dump, "<HEVCParameterSets>\n");
#ifndef GPAC_DISABLE_AV_PARSERS
		if (hvcc) {
			for (idx=0; idx<gf_list_count(hvcc->param_array); idx++) {
				GF_NALUFFParamArray *ar = gf_list_get(hvcc->param_array, idx);
				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCSPS", "hvcC", 0)
				} else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCPPS", "hvcC", 0)
				} else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCVPS", "hvcC", 0)
				} else {
					DUMP_ARRAY(ar->nalus, "HEVCUnknownPS", "hvcC", 0)
				}
			}
		}
		if (lhcc) {
			for (idx=0; idx<gf_list_count(lhcc->param_array); idx++) {
				GF_NALUFFParamArray *ar = gf_list_get(lhcc->param_array, idx);
				if (ar->type==GF_HEVC_NALU_SEQ_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCSPS", "lhcC", 0)
				} else if (ar->type==GF_HEVC_NALU_PIC_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCPPS", "lhcC", 0)
				} else if (ar->type==GF_HEVC_NALU_VID_PARAM) {
					DUMP_ARRAY(ar->nalus, "HEVCVPS", "lhcC", 0)
				} else {
					DUMP_ARRAY(ar->nalus, "HEVCUnknownPS", "lhcC", 0)
				}
			}
		}
#endif
		inspect_printf(dump, "</HEVCParameterSets>\n");
		break;

	case GF_CODECID_VVC:
	case GF_CODECID_VVC_SUBPIC:
		if (!dsi) {
			inspect_printf(dump, "/>\n");
			return;
		}

#ifndef GPAC_DISABLE_AV_PARSERS
		inspect_reset_parsers(pctx, &pctx->vvc_state);

		if (!pctx->vvc_state) {
			GF_SAFEALLOC(pctx->vvc_state, VVCState);
			if (!pctx->vvc_state) return;
		}
#endif

		vvcC = gf_odf_vvc_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		if (vvcC)
			pctx->nalu_size_length = vvcC->nal_unit_size;

		nalh_size = pctx->nalu_size_length;

		inspect_printf(dump, ">\n");
		inspect_printf(dump, "<VVCParameterSets>\n");
#ifndef GPAC_DISABLE_AV_PARSERS
		if (vvcC) {
			for (idx=0; idx<gf_list_count(vvcC->param_array); idx++) {
				GF_NALUFFParamArray *ar = gf_list_get(vvcC->param_array, idx);
				if (ar->type==GF_VVC_NALU_SEQ_PARAM) {
					DUMP_ARRAY(ar->nalus, "VVCSPS", "vvcC", 0)
				} else if (ar->type==GF_VVC_NALU_PIC_PARAM) {
					DUMP_ARRAY(ar->nalus, "VVCPPS", "vvcC", 0)
				} else if (ar->type==GF_VVC_NALU_VID_PARAM) {
					DUMP_ARRAY(ar->nalus, "VVCVPS", "vvcC", 0)
				} else if (ar->type==GF_VVC_NALU_APS_PREFIX) {
					DUMP_ARRAY(ar->nalus, "VVCAPS", "vvcC", 0)
				} else if (ar->type==GF_VVC_NALU_SEI_PREFIX) {
					DUMP_ARRAY(ar->nalus, "VVCSEI", "vvcC", 0)
				} else if (ar->type==GF_VVC_NALU_DEC_PARAM) {
					DUMP_ARRAY(ar->nalus, "VVCDCI", "vvcC", 0)
				} else {
					DUMP_ARRAY(ar->nalus, "VVCUnknownPS", "vvcC", 0)
				}
			}
		}
#endif
		inspect_printf(dump, "</VVCParameterSets>\n");
		break;

	case GF_CODECID_AV1:
#ifndef GPAC_DISABLE_AV_PARSERS
		inspect_reset_parsers(pctx, &pctx->av1_state);
		if (!pctx->av1_state) {
			GF_SAFEALLOC(pctx->av1_state, AV1State);
			if (!pctx->av1_state) return;
			gf_av1_init_state(pctx->av1_state);
		}
#endif
		if (!dsi) {
			inspect_printf(dump, "/>\n");
			return;
		}
#ifndef GPAC_DISABLE_AV_PARSERS
		if (pctx->av1_state->config) gf_odf_av1_cfg_del(pctx->av1_state->config);
		pctx->av1_state->config = gf_odf_av1_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		inspect_printf(dump, ">\n");
		inspect_printf(dump, " <OBUConfig>\n");

		idx = 1;
		for (i=0; i<gf_list_count(pctx->av1_state->config->obu_array); i++) {
			ObuType obu_type=0;
			u64 obu_size = 0;
			u32 hdr_size = 0;
			GF_AV1_OBUArrayEntry *obu = gf_list_get(pctx->av1_state->config->obu_array, i);

			if (!pctx->bs)
				pctx->bs = gf_bs_new((const u8 *) obu->obu, (u32) obu->obu_length, GF_BITSTREAM_READ);
			else
				gf_bs_reassign_buffer(pctx->bs, (const u8 *)obu->obu, (u32) obu->obu_length);

			gf_inspect_dump_obu_internal(dump, pctx->av1_state, (char*)obu->obu, obu->obu_length, obu_type, obu_size, hdr_size, ctx->crc, pctx, ctx->analyze);
			idx++;
		}
#endif
		inspect_printf(dump, " </OBUConfig>\n");
		break;

	case GF_CODECID_MPEG1:
	case GF_CODECID_MPEG2_422:
	case GF_CODECID_MPEG2_SNR:
	case GF_CODECID_MPEG2_HIGH:
	case GF_CODECID_MPEG2_MAIN:
	case GF_CODECID_MPEG4_PART2:

#ifndef GPAC_DISABLE_AV_PARSERS
		inspect_reset_parsers(pctx, &pctx->mv124_state);
#endif
		if (!dsi) {
			inspect_printf(dump, "/>\n");
			return;
		}
		inspect_printf(dump, ">\n");
		inspect_printf(dump, " <MPEGVideoConfig>\n");
#ifndef GPAC_DISABLE_AV_PARSERS
		inspect_dump_mpeg124(pctx, dsi->value.data.ptr, dsi->value.data.size, dump);
#endif
		inspect_printf(dump, " </MPEGVideoConfig>\n");
#ifndef GPAC_DISABLE_AV_PARSERS
		if (pctx->mv124_state) gf_m4v_parser_del(pctx->mv124_state);
#endif
		break;
	case GF_CODECID_MPEG_AUDIO:
	case GF_CODECID_MPEG2_PART3:
	case GF_CODECID_MPEG_AUDIO_L1:
	case GF_CODECID_TMCD:
		inspect_printf(dump, "/>\n");
		return;
	case GF_CODECID_SUBS_XML:
	case GF_CODECID_META_XML:
		if (!dsi) {
			inspect_printf(dump, "/>\n");
			return;
		}
		inspect_printf(dump, " <XMLTextConfig>\n");
		for (i=0; i<dsi->value.data.size; i++) {
			gf_fputc(dsi->value.data.ptr[i], dump);
		}
		inspect_printf(dump, "\n </XMLTextConfig>\n");
		break;
	case GF_CODECID_SUBS_TEXT:
	case GF_CODECID_META_TEXT:
		if (!dsi) {
			inspect_printf(dump, "/>\n");
			return;
		}
		inspect_printf(dump, " <TextConfig>\n");
		inspect_printf(dump, "<![CDATA[");
		for (i=0; i<dsi->value.data.size; i++) {
			gf_fputc(dsi->value.data.ptr[i], dump);
		}
		inspect_printf(dump, "]]>\n");
		inspect_printf(dump, " </TextConfig>\n");
		break;
	case GF_CODECID_APCH:
	case GF_CODECID_APCN:
	case GF_CODECID_APCS:
	case GF_CODECID_APCO:
	case GF_CODECID_AP4H:
	case GF_CODECID_AP4X:
		inspect_printf(dump, "/>\n");
		return;
	case GF_CODECID_MPHA:
	case GF_CODECID_MHAS:
		if (!dsi) {
			inspect_printf(dump, "/>\n");
			return;
		}
		{
		u16 size;
		if (!pctx->bs)
			pctx->bs = gf_bs_new(dsi->value.data.ptr, dsi->value.data.size, GF_BITSTREAM_READ);
		else
			gf_bs_reassign_buffer(pctx->bs, dsi->value.data.ptr, dsi->value.data.size);

		inspect_printf(dump, " <MPEGHAudioConfig");
		inspect_printf(dump, " version=\"%d\"", gf_bs_read_u8(pctx->bs) );
		inspect_printf(dump, " ProfileLevelIndication=\"%d\"", gf_bs_read_u8(pctx->bs) );
		inspect_printf(dump, " ReferenceChannelLayout=\"%d\"", gf_bs_read_u8(pctx->bs) );
		size = gf_bs_read_u16(pctx->bs);
		if (size) {
			inspect_printf(dump, ">\n");
#ifndef GPAC_DISABLE_AV_PARSERS
			dump_mha_config(dump, pctx->bs, "  ");
#endif
			inspect_printf(dump, " </MPEGHAudioConfig>\n");
		} else {
			inspect_printf(dump, "/>\n");
		}
		inspect_printf(dump, "/>\n");
		}
		break;
	case GF_CODECID_VP8:
	case GF_CODECID_VP9:
		if (!dsi) {
			inspect_printf(dump, "/>\n");
			return;
		}

#ifndef GPAC_DISABLE_AV_PARSERS
		inspect_reset_parsers(pctx, NULL);
#endif

		pctx->vpcc = gf_odf_vp_cfg_read(dsi->value.data.ptr, dsi->value.data.size);
		inspect_printf(dump, ">\n");
		if (pctx->vpcc) {
			u32 v = (pctx->codec_id==GF_CODECID_VP9) ? 9 : 8;
			inspect_printf(dump, "<VP%dConfiguration profile=\"%d\" level=\"%d\" bit_depth=\"%u\" chroma_subsampling=\"%u\" colour_primaries=\"%u\" transfer_characteristics=\"%u\" matrix_coefficients=\"%u\" video_full_range=\"%u\"", v,
				pctx->vpcc->profile, pctx->vpcc->level, pctx->vpcc->bit_depth, pctx->vpcc->chroma_subsampling, pctx->vpcc->colour_primaries, pctx->vpcc->transfer_characteristics, pctx->vpcc->matrix_coefficients, pctx->vpcc->video_fullRange_flag
			);
			if (pctx->vpcc->codec_initdata && pctx->vpcc->codec_initdata_size) {
				inspect_printf(dump, " init_data=\"");
				for (idx=0; idx<pctx->vpcc->codec_initdata_size; idx++)
					inspect_printf(dump, "%02X", (unsigned char) pctx->vpcc->codec_initdata[idx]);
				inspect_printf(dump, "\"");
			}
			inspect_printf(dump, "/>\n");
		}
		break;

	case GF_CODECID_AAC_MPEG4:
	case GF_CODECID_AAC_MPEG2_MP:
	case GF_CODECID_AAC_MPEG2_LCP:
	case GF_CODECID_AAC_MPEG2_SSRP:
	case GF_CODECID_USAC:
		if (!pctx->no_analysis) {
			pctx->no_analysis = GF_TRUE;
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Inspect] bitstream analysis for codec %s not supported, only configuration is\n", gf_codecid_name(pctx->codec_id)));
		}
		if (dsi) {
#ifndef GPAC_DISABLE_AV_PARSERS
			GF_M4ADecSpecInfo acfg;
			InspectLogCbk lcbk;

			inspect_printf(dump, ">\n");
			inspect_printf(dump, "<MPEG4AudioConfiguration ");

			if (!pctx->bs)
				pctx->bs = gf_bs_new(dsi->value.data.ptr, dsi->value.data.size, GF_BITSTREAM_READ);
			else
				gf_bs_reassign_buffer(pctx->bs, dsi->value.data.ptr, dsi->value.data.size);

			if (ctx->analyze>=INSPECT_ANALYZE_BS) {
				lcbk.dump = dump;
				lcbk.dump_bits = (ctx->analyze==INSPECT_ANALYZE_BS_BITS) ? GF_TRUE : GF_FALSE;
				gf_bs_set_logger(pctx->bs, regular_bs_log, &lcbk);
			}

			gf_m4a_parse_config(pctx->bs, &acfg, GF_TRUE);
			if (ctx->analyze<INSPECT_ANALYZE_BS) {
				inspect_printf(dump, "base_object_type=\"%u\" sample_rate=\"%u\" channels=\"%u\" ", acfg.base_object_type, acfg.base_sr, acfg.nb_chan);
				if (acfg.has_sbr)
					inspect_printf(dump, "sbr_sample_rate=\"%u\" ", acfg.sbr_sr);
				if (acfg.has_ps)
					inspect_printf(dump, "parametricStereo=\"yes\" ");
			}
			if (acfg.comments[0])
				inspect_printf(dump, "comments=\"%s\" ", acfg.comments);
#endif /*GPAC_DISABLE_AV_PARSERS*/
			inspect_printf(dump, "/>\n");
		} else {
			inspect_printf(dump, "/>\n");
			return;
		}
		break;
	case GF_CODECID_AC3:
	case GF_CODECID_EAC3:
		inspect_printf(dump, "/>\n");
		return;

	case GF_CODECID_TRUEHD:
		if (dsi) {
			u16 fmt, prate;
			inspect_printf(dump, ">\n");
			inspect_printf(dump, "<TrueHDAudioConfiguration");

			if (!pctx->bs)
				pctx->bs = gf_bs_new(dsi->value.data.ptr, dsi->value.data.size, GF_BITSTREAM_READ);
			else
				gf_bs_reassign_buffer(pctx->bs, dsi->value.data.ptr, dsi->value.data.size);

			fmt = gf_bs_read_u32(pctx->bs);
			prate = gf_bs_read_int(pctx->bs, 15);

			inspect_printf(dump, " format=\"%u\" peak_data_rate=\"%u\"/>\n", fmt, prate);
		} else {
			inspect_printf(dump, "/>\n");
			return;
		}
		break;

	case GF_CODECID_OPUS:
		if (dsi) {
			GF_OpusConfig opcfg;
			inspect_printf(dump, ">\n");
			inspect_printf(dump, "<OpusConfiguration");
			gf_odf_opus_cfg_parse(dsi->value.data.ptr, dsi->value.data.size, &opcfg);

			inspect_printf(dump, " version=\"%d\" OutputChannelCount=\"%d\" PreSkip=\"%d\" InputSampleRate=\"%d\" OutputGain=\"%d\" ChannelMappingFamily=\"%d\"",
					opcfg.version, opcfg.OutputChannelCount, opcfg.PreSkip, opcfg.InputSampleRate, opcfg.OutputGain, opcfg.ChannelMappingFamily);
			if (opcfg.ChannelMappingFamily) {
				u32 i;
				inspect_printf(dump, " StreamCount=\"%d\" CoupledStreamCount=\"%d\" channelMapping=\"", opcfg.StreamCount, opcfg.CoupledCount);
				for (i=0; i<opcfg.OutputChannelCount; i++) {
					inspect_printf(dump, "%s%d", i ? " " : "", opcfg.ChannelMapping[i]);
				}
				inspect_printf(dump, "\"");
			}
			pctx->opus_channel_count = opcfg.StreamCount;
			pctx->opus_channel_count = (opcfg.StreamCount ? opcfg.StreamCount : 1);
			inspect_printf(dump, "/>\n");
		} else {
			inspect_printf(dump, "/>\n");
			return;
		}
		break;

	case GF_CODECID_ALAC:
		if (dsi) {
			GF_BitStream *bs = gf_bs_new(dsi->value.data.ptr, dsi->value.data.size, GF_BITSTREAM_READ);
			inspect_printf(dump, ">\n");
			inspect_printf(dump, "<ALACConfiguration");
			while (gf_bs_available(bs)) {
				u32 val;

#define get_and_print(name, bits) \
				val = gf_bs_read_int(bs, bits); \
				inspect_printf(dump, " "name"=\"%u\"", val);

				u32 size = gf_bs_read_u32(bs);
				u32 type = gf_bs_read_u32(bs);
				if (type==GF_4CC('a','l','a','c') && (size==36)) {

					get_and_print("version", 32)
					get_and_print("frameLength", 32)
					get_and_print("compatibleVersion", 8)
					get_and_print("bitDepth", 8)
					get_and_print("pb", 8)
					get_and_print("mb", 8)
					get_and_print("kb", 8)
					get_and_print("numChannels", 8)
					get_and_print("maxRun", 16)
					get_and_print("maxFrameBytes", 32)
					get_and_print("avgBitRate", 32)
					get_and_print("sampleRate", 32)
				} else if (type==GF_4CC('c','h','a','n') && (size==24)) {
					get_and_print("version", 32)
					get_and_print("channelLayoutTag", 32)
					get_and_print("reserved1", 32)
					get_and_print("reserved2", 32)
				}
			}
#undef get_and_print

			inspect_printf(dump, "/>\n");
			gf_bs_del(bs);
		} else {
			inspect_printf(dump, "/>\n");
			return;
		}
		break;

	default:
		if (!pctx->no_analysis) {
			pctx->no_analysis = GF_TRUE;
			GF_LOG(GF_LOG_WARNING, GF_LOG_MEDIA, ("[Inspect] bitstream analysis for codec %s not supported\n", gf_codecid_name(pctx->codec_id)));
		}
		inspect_printf(dump, "/>\n");
		return;
	}

	if (avcc) gf_odf_avc_cfg_del(avcc);
	if (svcc) gf_odf_avc_cfg_del(svcc);
	if (hvcc) gf_odf_hevc_cfg_del(hvcc);
	if (lhcc) gf_odf_hevc_cfg_del(lhcc);
	if (vvcC) gf_odf_vvc_cfg_del(vvcC);
	inspect_printf(dump, "</%s>\n", elt_name);
}

static GF_Err inspect_process(GF_Filter *filter)
{
	u32 i, count, nb_done=0, nb_hdr_done=0;
	GF_InspectCtx *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	count = gf_list_count(ctx->src_pids);

	if (ctx->args_updated) {
		ctx->args_updated = GF_FALSE;
		for (i=0; i<count; i++) {
			PidCtx *pctx = gf_list_get(ctx->src_pids, i);
			switch (ctx->mode) {
			case INSPECT_MODE_PCK:
			case INSPECT_MODE_REFRAME:
				gf_filter_pid_set_framing_mode(pctx->src_pid, GF_TRUE);
				break;
			default:
				gf_filter_pid_set_framing_mode(pctx->src_pid, GF_FALSE);
				break;
			}
			gf_filter_pid_set_clock_mode(pctx->src_pid, ctx->pcr);
		}
		if (ctx->is_prober || ctx->deep || ctx->fmt) {
			ctx->dump_pck = GF_TRUE;
		} else {
			ctx->dump_pck = GF_FALSE;
		}
	}
	count = gf_list_count(ctx->src_pids);
	for (i=0; i<count; i++) {
		PidCtx *pctx = gf_list_get(ctx->src_pids, i);
		GF_FilterPacket *pck = NULL;
		pck = pctx->src_pid ? gf_filter_pid_get_packet(pctx->src_pid) : NULL;

		if (pctx->init_pid_config_done)
			nb_hdr_done++;

		if (!pck) {
			if (pctx->src_pid && !gf_filter_pid_is_eos(pctx->src_pid))
				continue;
			else
				ctx->has_seen_eos = GF_TRUE;
		}
		if (pctx->aborted)
			continue;

		if (!pctx->buffer_done || ctx->rbuffer) {
			u64 dur=0;
			if (pck && !ctx->has_seen_eos && !gf_filter_pid_eos_received(pctx->src_pid)) {
				dur = gf_filter_pid_query_buffer_duration(pctx->src_pid, GF_FALSE);
				if (!pctx->buffer_done) {
					if ((dur < ctx->buffer * 1000)) {
						GF_LOG(GF_LOG_DEBUG, GF_LOG_MEDIA, ("[Inspect] PID %d buffering buffer %u ms %.02f %%\n", pctx->idx, (u32) (dur/1000), dur*1.0/(ctx->buffer * 10.0)));
						continue;
					}
				} else if (ctx->rbuffer && (dur < ctx->rbuffer * 1000)) {
					pctx->buffer_done = GF_FALSE;
					pctx->buf_start_time = gf_sys_clock();
					GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Inspect] PID %d rebuffering buffer %u ms less than rebuffer %u\n", pctx->idx, (u32) (dur/1000), ctx->rbuffer));
					continue;
				}
			}
			if (!pctx->buffer_done) {
				if (!dur) {
					dur = gf_filter_pid_query_buffer_duration(pctx->src_pid, GF_FALSE);
					GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Inspect] PID %d buffering aborted after %u ms - level %u ms %.02f %%\n", pctx->idx, gf_sys_clock() - pctx->buf_start_time, (u32) (dur/1000), dur*1.0/(ctx->buffer * 10.0)));
				} else {
					GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Inspect] PID %d buffering done in %u ms - level %u ms %.02f %%\n", pctx->idx, gf_sys_clock() - pctx->buf_start_time, (u32) (dur/1000), dur*1.0/(ctx->buffer * 10.0)));
				}
				pctx->buffer_done = GF_TRUE;
			}
		}

		if (pctx->dump_pid) {
			inspect_dump_pid(ctx, pctx->tmp, pctx->src_pid, pctx->idx, pctx->init_pid_config_done ? GF_FALSE : GF_TRUE, GF_FALSE, pctx->pck_for_config, (pctx->dump_pid==2) ? GF_TRUE : GF_FALSE, pctx);
			pctx->dump_pid = 0;
			pctx->init_pid_config_done = 1;
			pctx->pck_for_config=0;

			if (!ctx->hdr_done) {
				ctx->hdr_done=GF_TRUE;
				//dump header on main output file, not on pid one!
				if (ctx->hdr && ctx->fmt && !ctx->xml)
					inspect_dump_packet_fmt(filter, ctx, ctx->dump, NULL, 0, 0);
			}
		}
		if (!pck) continue;
		
		pctx->pck_for_config++;
		pctx->pck_num++;

		if (ctx->dump_pck) {

			if (ctx->is_prober) {
				nb_done++;
			} else {
				if (ctx->fmt) {
					inspect_dump_packet_fmt(filter, ctx, pctx->tmp, pck, pctx, pctx->pck_num);
				} else {
					inspect_dump_packet(ctx, pctx->tmp, pck, pctx->idx, pctx->pck_num, pctx);
				}
			}
		}
		
		if (ctx->dur.num && ctx->dur.den) {
			u64 timescale = gf_filter_pck_get_timescale(pck);
			u64 ts = gf_filter_pck_get_dts(pck);
			u64 dur = gf_filter_pck_get_duration(pck);
			if (ts == GF_FILTER_NO_TS) ts = gf_filter_pck_get_cts(pck);

			if (!pctx->init_ts) pctx->init_ts = ts+1;
			else if (gf_timestamp_greater_or_equal(ts + dur - pctx->init_ts + 1, timescale, ctx->dur.num, ctx->dur.den)) {
				GF_FilterEvent evt;
				GF_FEVT_INIT(evt, GF_FEVT_STOP, pctx->src_pid);

				GF_LOG(GF_LOG_INFO, GF_LOG_MEDIA, ("[Inspect] PID %d (codec %s) done dumping, aborting\n", pctx->idx, gf_codecid_name(pctx->codec_id) ));
				gf_filter_pid_drop_packet(pctx->src_pid);

				gf_filter_pid_send_event(pctx->src_pid, &evt);
				gf_filter_pid_set_discard(pctx->src_pid, GF_TRUE);
				pctx->aborted = GF_TRUE;
				break;
			}
		}
		gf_filter_pid_drop_packet(pctx->src_pid);
	}
	if ((ctx->is_prober && !ctx->probe_done && (nb_done==count) && !ctx->allp)
		|| (!ctx->is_prober && !ctx->allp && !ctx->dump_pck && (nb_hdr_done==count) && !gf_filter_connections_pending(filter))
	) {
		for (i=0; i<count; i++) {
			PidCtx *pctx = gf_list_get(ctx->src_pids, i);
			GF_FilterEvent evt;
			GF_FEVT_INIT(evt, GF_FEVT_STOP, pctx->src_pid);
			gf_filter_pid_send_event(pctx->src_pid, &evt);
		}
		if (ctx->is_prober)
			ctx->probe_done = GF_TRUE;
		return GF_EOS;
	}
	return GF_OK;
}

static GF_Err inspect_config_input(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_FilterEvent evt;
	PidCtx *pctx;
	u32 w, h, sr, ch;
	const GF_PropertyValue *p;
	GF_InspectCtx *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	if (!ctx->src_pids) ctx->src_pids = gf_list_new();

	pctx = gf_filter_pid_get_udta(pid);
	if (pctx) {
		assert(pctx->src_pid == pid);
		if (is_remove)
			pctx->src_pid = NULL;
		else if (!ctx->is_prober)
			pctx->dump_pid = 1;
		return GF_OK;
	}
	GF_SAFEALLOC(pctx, PidCtx);
	if (!pctx) return GF_OUT_OF_MEM;
	if (ctx->analyze)
		pctx->bs = gf_bs_new((u8 *)pctx, 0, GF_BITSTREAM_READ);

	pctx->src_pid = pid;
	gf_filter_pid_set_udta(pid, pctx);


	p = gf_filter_pid_get_property(pid, GF_PROP_PID_STREAM_TYPE);
	pctx->stream_type = p ? p->value.uint : 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_CODECID);
	pctx->codec_id = p ? p->value.uint : 0;

	if (!ctx->buffer) {
		pctx->buffer_done = GF_TRUE;
	} else {
		GF_FEVT_INIT(evt, GF_FEVT_BUFFER_REQ, pid);
		evt.buffer_req.max_playout_us = evt.buffer_req.max_buffer_us = ctx->buffer * 1000;
		if (ctx->mbuffer >= ctx->buffer)
			evt.buffer_req.max_buffer_us = ctx->mbuffer * 1000;
		if (ctx->rbuffer<ctx->buffer)
			evt.buffer_req.min_playout_us = ctx->rbuffer * 1000;
		else
			ctx->rbuffer = 0;

		//if pid is decoded media, don't ask for buffer requirement on pid, set it at decoder level
		evt.buffer_req.pid_only = gf_filter_pid_has_decoder(pid) ? GF_FALSE : GF_TRUE;
		gf_filter_pid_send_event(pid, &evt);
		pctx->buf_start_time = gf_sys_clock();
	}



	w = h = sr = ch = 0;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_WIDTH);
	if (p) w = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_HEIGHT);
	if (p) h = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_SAMPLE_RATE);
	if (p) sr = p->value.uint;
	p = gf_filter_pid_get_property(pid, GF_PROP_PID_NUM_CHANNELS);
	if (p) ch = p->value.uint;

	if (! ctx->interleave) {
		u32 insert_idx = 0;
		u32 i;
		//sort all PIDs by codec IDs
		for (i=0; i<gf_list_count(ctx->src_pids); i++) {
			Bool insert = GF_FALSE;
			PidCtx *actx = gf_list_get(ctx->src_pids, i);

			if (pctx->codec_id < actx->codec_id) {
				insert = GF_TRUE;
			}
			//same codec ID, sort by increasing width/height/samplerate/channels
			else if (pctx->codec_id==actx->codec_id) {
				u32 aw, ah, asr, ach;

				aw = ah = asr = ach = 0;
				p = gf_filter_pid_get_property(actx->src_pid, GF_PROP_PID_WIDTH);
				if (p) aw = p->value.uint;
				p = gf_filter_pid_get_property(actx->src_pid, GF_PROP_PID_HEIGHT);
				if (p) ah = p->value.uint;
				p = gf_filter_pid_get_property(actx->src_pid, GF_PROP_PID_SAMPLE_RATE);
				if (p) asr = p->value.uint;
				p = gf_filter_pid_get_property(actx->src_pid, GF_PROP_PID_NUM_CHANNELS);
				if (p) ach = p->value.uint;

				if (w && aw && (w<aw)) insert = GF_TRUE;
				if (h && ah && (h<ah)) insert = GF_TRUE;
				if (sr && asr && (sr<asr)) insert = GF_TRUE;
				if (ch && ach && (ch<ach)) insert = GF_TRUE;
			}

			if (insert) {
				insert_idx = i+1;
				break;
			}
		}
		if (insert_idx) {
			gf_list_insert(ctx->src_pids, pctx, insert_idx-1);
			for (i=insert_idx; i<gf_list_count(ctx->src_pids); i++) {
				PidCtx *actx = gf_list_get(ctx->src_pids, i);
				actx->idx = i+1;
			}
		} else {
			gf_list_add(ctx->src_pids, pctx);
		}
	} else {
		pctx->tmp = ctx->dump;
		gf_list_add(ctx->src_pids, pctx);
	}

	pctx->idx = gf_list_find(ctx->src_pids, pctx) + 1;

	if (! ctx->interleave && !pctx->tmp && ctx->dump) {
		pctx->tmp = gf_file_temp(NULL);
		if (ctx->xml)
			inspect_printf(pctx->tmp, "<PIDInspect ID=\"%d\" name=\"%s\">\n", pctx->idx, gf_filter_pid_get_name(pid) );
	}

	switch (ctx->mode) {
	case INSPECT_MODE_PCK:
	case INSPECT_MODE_REFRAME:
		gf_filter_pid_set_framing_mode(pid, GF_TRUE);
		break;
	default:
		gf_filter_pid_set_framing_mode(pid, GF_FALSE);
		break;
	}

	if (!ctx->is_prober) {
		pctx->dump_pid = 1;
	}

	gf_filter_pid_init_play_event(pid, &evt, ctx->start, ctx->speed, "Inspect");
	gf_filter_pid_send_event(pid, &evt);

	if (ctx->is_prober || ctx->deep || ctx->fmt) {
		ctx->dump_pck = GF_TRUE;
	} else {
		ctx->dump_pck = GF_FALSE;
	}
	if (ctx->pcr)
		gf_filter_pid_set_clock_mode(pid, GF_TRUE);

	if (!ctx->deep)
		gf_filter_post_process_task(filter);
	return GF_OK;
}

static const GF_FilterCapability InspecterDemuxedCaps[] =
{
	//accept any stream but files, framed
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	{0},
};

static const GF_FilterCapability InspecterReframeCaps[] =
{
	//accept any stream but files, framed
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
};

static GF_Err inspect_update_arg(GF_Filter *filter, const char *arg_name, const GF_PropertyValue *new_val)
{
	GF_InspectCtx  *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);
	ctx->args_updated = GF_TRUE;
	return GF_OK;
}

GF_Err inspect_initialize(GF_Filter *filter)
{
	const char *name = gf_filter_get_name(filter);
	GF_InspectCtx  *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);

	if (ctx->log && !strcmp(ctx->log, "GLOG"))
		ctx->dump_log = GF_TRUE;

	if (name && !strcmp(name, "probe") ) {
		ctx->is_prober = GF_TRUE;
		return GF_OK;
	}

#ifdef GPAC_DISABLE_AVPARSE_LOGS
	if (ctx->analyze>=INSPECT_ANALYZE_BS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Inspect] Bitstream logging is disable in this build\n"));
		return GF_NOT_SUPPORTED;
	}
#endif

	if (!ctx->log) return GF_BAD_PARAM;

	if (ctx->analyze) {
		ctx->xml = GF_TRUE;
	}


	if (ctx->xml || ctx->analyze || gf_sys_is_test_mode() || ctx->fmt) {
		ctx->full = GF_TRUE;
	}
	if (!ctx->full) {
		ctx->mode = INSPECT_MODE_REFRAME;
	}

	switch (ctx->mode) {
	case INSPECT_MODE_RAW:
		break;
	case INSPECT_MODE_REFRAME:
		gf_filter_override_caps(filter, InspecterReframeCaps,  sizeof(InspecterReframeCaps)/sizeof(GF_FilterCapability) );
		break;
	default:
		gf_filter_override_caps(filter, InspecterDemuxedCaps,  sizeof(InspecterDemuxedCaps)/sizeof(GF_FilterCapability) );
		break;
	}
	if (gf_filter_is_temporary(filter))
		return GF_OK;

	if (!strcmp(ctx->log, "stderr")) ctx->dump = stderr;
	else if (!strcmp(ctx->log, "stdout")) ctx->dump = stdout;
	else if (!strcmp(ctx->log, "null")) ctx->dump = NULL;
	else if (!ctx->dump_log) {
		ctx->dump = gf_fopen(ctx->log, "wt");
		if (!ctx->dump) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_MEDIA, ("[Inspect] Failed to open file %s\n", ctx->log));
			return GF_IO_ERR;
		}
	}
	if (ctx->xml && ctx->dump) {
		ctx->fmt = NULL;
		inspect_printf(ctx->dump, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		inspect_printf(ctx->dump, "<GPACInspect>\n");
	}

#ifdef GPAC_ENABLE_COVERAGE
	if (gf_sys_is_cov_mode()) {
		inspect_update_arg(filter, NULL, NULL);
		ctx->args_updated = GF_FALSE;
	}
#endif
	return GF_OK;
}

static Bool inspect_process_event(GF_Filter *filter, const GF_FilterEvent *evt)
{
	PidCtx *pctx;
	GF_InspectCtx *ctx = (GF_InspectCtx *) gf_filter_get_udta(filter);
	if (evt->base.type != GF_FEVT_INFO_UPDATE) return GF_TRUE;
	if (!ctx->info) return GF_TRUE;
	pctx = gf_filter_pid_get_udta(evt->base.on_pid);
	pctx->dump_pid = 2;
	return GF_TRUE;
}

#define OFFS(_n)	#_n, offsetof(GF_InspectCtx, _n)
static const GF_FilterArgs InspectArgs[] =
{
	{ OFFS(log), "set probe log filename to print number of streams, GLOG uses GPAC logs `app@info`(default for android)", GF_PROP_STRING,
#ifdef GPAC_CONFIG_ANDROID
		"GLOG"
#else
		"stdout"
#endif
		, "fileName, stderr, stdout, GLOG or null", 0},
	{ OFFS(mode), "dump mode\n"
	"- pck: dump full packet\n"
	"- blk: dump packets before reconstruction\n"
	"- frame: force reframer\n"
	"- raw: dump source packets without demultiplexing", GF_PROP_UINT, "pck", "pck|blk|frame|raw", 0},
	{ OFFS(interleave), "dump packets as they are received on each PID. If false, logs are reported for each PID at end of session", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(deep), "dump packets along with PID state change, implied when [-fmt]() is set", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(props), "dump packet properties, ignored when [-fmt]() is set", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(dump_data), "enable full data dump (__very large output__), ignored when [-fmt]() is set", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(fmt), "set packet dump format", GF_PROP_STRING, NULL, NULL, GF_FS_ARG_UPDATE|GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(hdr), "print a header corresponding to fmt string without '$' or \"pid\"", GF_PROP_BOOL, "true", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(allp), "analyse for the entire duration, rather than stopping when all PIDs are found", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED},
	{ OFFS(info), "monitor PID info changes", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(full), "full dump of PID properties (always on if XML)", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(pcr), "dump M2TS PCR info", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{ OFFS(speed), "set playback command speed. If negative and start is 0, start is set to -1", GF_PROP_DOUBLE, "1.0", NULL, 0},
	{ OFFS(start), "set playback start offset. A negative value means percent of media duration with -1 equal to duration", GF_PROP_DOUBLE, "0.0", NULL, 0},
	{ OFFS(dur), "set inspect duration", GF_PROP_FRACTION, "0/0", NULL, 0},
	{ OFFS(analyze), "analyze sample content (NALU, OBU)\n"
	"- off: no analyzing\n"
	"- on: simple analyzing\n"
	"- bs: log bitstream syntax (all elements read from bitstream)\n"
	"- full: log bitstream syntax and bit sizes signaled as `(N)` after field value, except 1-bit fields (omitted)", GF_PROP_UINT, "off", "off|on|bs|full", GF_FS_ARG_HINT_ADVANCED|GF_FS_ARG_UPDATE},
	{ OFFS(xml), "use xml formatting (implied if (-analyze]() is set) and disable [-fmt]()", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(crc), "dump crc of samples of subsamples (NALU or OBU) when analyzing", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(fftmcd), "consider timecodes use ffmpeg-compatible signaling rather than QT compliant one", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{ OFFS(dtype), "dump property type", GF_PROP_BOOL, "false", NULL, GF_FS_ARG_UPDATE},
	{ OFFS(buffer), "set playback buffer in ms", GF_PROP_UINT, "0", NULL, GF_ARG_HINT_EXPERT},
	{ OFFS(mbuffer), "set max buffer occupancy in ms. If less than buffer, use buffer", GF_PROP_UINT, "0", NULL, 0},
	{ OFFS(rbuffer), "rebuffer trigger in ms. If 0 or more than buffer, disable rebuffering", GF_PROP_UINT, "0", NULL, GF_FS_ARG_UPDATE},

	{ OFFS(test), "skip predefined set of properties, used for test mode\n"
		"- no: no properties skipped\n"
		"- noprop: all properties/info changes on PID are skipped, only packets are dumped\n"
		"- network: URL/path dump, cache state, file size properties skipped (used for hashing network results)\n"
		"- netx: same as network but skip track duration and templates (used for hashing progressive load of fmp4)\n"
		"- encode: same as network plus skip decoder config (used for hashing encoding results)\n"
		"- encx: same as encode and skip bitrates, media data size and co\n"
		"- nocrc: disable packet CRC dump\n"
		"- nobr: skip bitrate"
		, GF_PROP_UINT, "no", "no|noprop|network|netx|encode|encx|nocrc|nobr", GF_FS_ARG_HINT_EXPERT|GF_FS_ARG_UPDATE},
	{0}
};

static const GF_FilterCapability InspectCaps[] =
{
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_STREAM_TYPE, GF_STREAM_UNKNOWN),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_CODECID, GF_CODECID_NONE),
};

const GF_FilterRegister InspectRegister = {
	.name = "inspect",
	GF_FS_SET_DESCRIPTION("Inspect packets")
	GF_FS_SET_HELP("The inspect filter can be used to dump PID and packets. It may also be used to check parts of payload of the packets.\n"
	"\n"
	"The default options inspect only PID changes.\n"
	"If [-full]() is not set, [-mode=frame]() is forced and PID properties are formatted in human-readable form, one PID per line.\n"
	"Otherwise, all properties are dumped.\n"
	"Note: specifying [-xml](), [-analyze](), [-fmt]() or using `-for-test` will force [-full]() to true.\n"
	"\n"
	"# Custom property duming\n"
	"The packet inspector can be configured to dump specific properties of packets using [-fmt]().\n"
	"When the option is not present, all properties are dumped. Otherwise, only properties identified by `$TOKEN$` "
	"are printed. You may use '$', '@' or '%' for `TOKEN` separator. `TOKEN` can be:\n"
	"- pn: packet (frame in framed mode) number\n"
	"- dts: decoding time stamp in stream timescale, N/A if not available\n"
	"- ddts: difference between current and previous packets decoding time stamp in stream timescale, N/A if not available\n"
	"- cts: composition time stamp in stream timescale, N/A if not available\n"
	"- dcts: difference between current and previous packets composition time stamp in stream timescale, N/A if not available\n"
	"- ctso: difference between composition time stamp and decoding time stamp in stream timescale, N/A if not available\n"
	"- dur: duration in stream timescale\n"
	"- frame: framing status\n"
	"  - interface: complete AU, interface object (no size info). Typically a GL texture\n"
	"  - frame_full: complete AU\n"
	"  - frame_start: beginning of frame\n"
	"  - frame_end: end of frame\n"
	"  - frame_cont: frame continuation (not beginning, not end)\n"
	"- sap or rap: SAP type of the frame\n"
	"- ilace: interlacing flag (0: progressive, 1: top field, 2: bottom field)\n"
	"- corr: corrupted packet flag\n"
	"- seek: seek flag\n"
	"- bo: byte offset in source, N/A if not available\n"
	"- roll: roll info\n"
	"- crypt: crypt flag\n"
	"- vers: carousel version number\n"
	"- size: size of packet\n"
	"- csize: total size of packets received so far\n"
	"- crc: 32 bit CRC of packet\n"
	"- lf or n: insert new line\n"
	"- t: insert tab\n"
	"- data: hex dump of packet (__big output!__) or as string if legal UTF-8\n"
	"- lp: leading picture flag\n"
	"- depo: depends on other packet flag\n"
	"- depf: is depended on other packet flag\n"
	"- red: redundant coding flag\n"
	"- start: packet composition time as HH:MM:SS.ms\n"
	"- startc: packet composition time as HH:MM:SS,ms\n"
	"- end: packet end time as HH:MM:SS.ms\n"
	"- endc: packet end time as HH:MM:SS,ms\n"
	"- ck: clock type used for PCR discontinuities\n"
	"- pcr: MPEG-2 TS last PCR, n/a if not available\n"
	"- pcrd: difference between last PCR and decoding time, n/a if no PCR available\n"
	"- pcrc: difference between last PCR and composition time, n/a if no PCR available\n"
	"- P4CC: 4CC of packet property\n"
	"- PropName: Name of packet property\n"
	"- pid.P4CC: 4CC of PID property\n"
	"- pid.PropName: Name of PID property\n"
	"- fn: Filter name\n"
	"\n"
	"EX fmt=\"PID $pid.ID$ packet $pn$ DTS $dts$ CTS $cts$ $lf$\"\n"
	"This dumps packet number, cts and dts as follows: `PID 1 packet 10 DTS 100 CTS 108 \\n`\n"
	"  \n"
	"An unrecognized keyword or missing property will resolve to an empty string.\n"
	"\n"
	"Note: when dumping in interleaved mode, there is no guarantee that the packets will be dumped in their original sequence order since the inspector fetches one packet at a time on each PID.\n"
	"\n"
	"# Note on playback\n"
	"Buffering can be enabled to check the input filter chain behaviour, e.g. check HAS adaptation logic.\n"
	"The various buffering options control when packets are consumed. Buffering events are logged using `media@info` for state changes and `media@debug` for media filling events.\n"
	"The [-speed]() option is only used to configure the filter chain but is ignored by the filter when consuming packets.\n"
	"If real-time consumption is required, a reframer filter must be setup before the inspect filter.\n"
	"EX gpac -i SRC reframer:rt=on inspect:buffer=10000:rbuffer=1000:mbuffer=30000:speed=2\n"
	"This will play the session at 2x speed, using 30s of maximum buffering, consumming packets after 10s of media are ready and rebuffering if less than 1s of media.\n"
	"\n"
	)
	.private_size = sizeof(GF_InspectCtx),
	.flags = GF_FS_REG_EXPLICIT_ONLY,
	.max_extra_pids = (u32) -1,
	.args = InspectArgs,
	SETCAPS(InspectCaps),
	.initialize = inspect_initialize,
	.finalize = inspect_finalize,
	.process = inspect_process,
	.process_event = inspect_process_event,
	.configure_pid = inspect_config_input,
	.update_arg = inspect_update_arg,
};

static const GF_FilterCapability ProberCaps[] =
{
	//accept any stream but files, framed
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
	CAP_UINT(GF_CAPS_INPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_SCENE),
	CAP_UINT(GF_CAPS_INPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_OD),
	CAP_UINT(GF_CAPS_INPUT,  GF_PROP_PID_STREAM_TYPE, GF_STREAM_TEXT),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_NONE),
	CAP_UINT(GF_CAPS_INPUT_EXCLUDED,  GF_PROP_PID_CODECID, GF_CODECID_RAW),
	CAP_BOOL(GF_CAPS_INPUT_EXCLUDED, GF_PROP_PID_UNFRAMED, GF_TRUE),
	{0},
};

static const GF_FilterArgs ProbeArgs[] =
{
	{ OFFS(log), "set probe log filename to print number of streams, GLOG uses GPAC logs `app@info`(default for android)", GF_PROP_STRING,
#ifdef GPAC_CONFIG_ANDROID
		"GLOG"
#else
		"stdout"
#endif
	, "fileName, stderr, stdout GLOG or null", 0},
	{0}
};


const GF_FilterRegister ProbeRegister = {
	.name = "probe",
	GF_FS_SET_DESCRIPTION("Probe source")
	GF_FS_SET_HELP("The Probe filter is used by applications (typically `MP4Box`) to query demultiplexed PIDs (audio, video, ...) available in a source chain.\n\n"
	"The filter outputs the number of input PIDs in the file specified by [-log]().\n"
	"It is up to the app developer to query input PIDs of the prober and take appropriated decisions.")
	.private_size = sizeof(GF_InspectCtx),
	.flags = GF_FS_REG_EXPLICIT_ONLY | GF_FS_REG_TEMP_INIT,
	.max_extra_pids = (u32) -1,
	.initialize = inspect_initialize,
	.args = ProbeArgs,
	SETCAPS(ProberCaps),
	.finalize = inspect_finalize,
	.process = inspect_process,
	.configure_pid = inspect_config_input,
};

const GF_FilterRegister *inspect_register(GF_FilterSession *session)
{
	return &InspectRegister;
}

const GF_FilterRegister *probe_register(GF_FilterSession *session)
{
	return &ProbeRegister;
}



