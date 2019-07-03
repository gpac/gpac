/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Authoring Tools sub-project
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

#ifndef _GF_PARSERS_AV_H_
#define _GF_PARSERS_AV_H_


#ifdef __cplusplus
extern "C" {
#endif

/*!
 *	\file <gpac/avparse.h>
 *	\brief Utility tools for audio and video raw media parsing.
 */

/*! \defgroup media_grp Media Tools
 *	You will find in this module the documentation of all media tools in GPAC.
*/


/*!
 *	\addtogroup avp_grp AV Parsing
 *	\ingroup media_grp
 *	\brief Utility tools for audio and video raw media parsing.
 *
 *This section documents the audio and video parsing functions of the GPAC framework.
 *	@{
 */


#include <gpac/bitstream.h>



/*!
  Reduces input width/height to common aspect ration num/denum values
 \param width width of the aspect ratio
 \param height height of the aspect ratio
 */
void gf_media_reduce_aspect_ratio(u32 *width, u32 *height);

/*!
 Reduces input FPS to a more compact value (eg 25000/1000 -> 25/1)
 \param timescale timescale of the aspect ratio
 \param sample_dur sample duration of the aspect ratio in the given timescale
 */
void gf_media_get_reduced_frame_rate(u32 *timescale, u32 *sample_dur);

/*inserts emulation prevention bytes from buffer_src into buffer_dst*/
u32 gf_media_nalu_add_emulation_bytes(const u8 *buffer_src, u8 *buffer_dst, u32 nal_size);
/*returns the nal_size without emulation prevention bytes*/
u32 gf_media_nalu_emulation_bytes_add_count(u8 *buffer, u32 nal_size);

/*basic MPEG (1,2,4) visual object parser (DSI extraction and timing/framing)*/
typedef struct
{
	/*video PL*/
	u8 VideoPL;
	u8 RAP_stream, objectType, has_shape, enh_layer;
	/*video resolution*/
	u16 width, height;
	/*pixel aspect ratio*/
	u8 par_num, par_den;

	u16 clock_rate;
	u8 NumBitsTimeIncrement;
	u32 time_increment;
	/*for MPEG 1/2*/
	Double fps;

	u32 next_object_start;
} GF_M4VDecSpecInfo;


typedef struct __tag_m4v_parser GF_M4VParser;

#ifndef GPAC_DISABLE_AV_PARSERS

GF_M4VParser *gf_m4v_parser_new(u8 *data, u64 data_size, Bool mpeg12video);
GF_M4VParser *gf_m4v_parser_bs_new(GF_BitStream *bs, Bool mpeg12video);
void gf_m4v_parser_del(GF_M4VParser *m4v);
void gf_m4v_parser_del_no_bs(GF_M4VParser *m4v);
GF_Err gf_m4v_parse_config(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi);
//if obj_type is not 0, skip next start code and use (obj_type-1) for next obj parsing
void gf_m4v_parser_reset(GF_M4VParser *m4v, u8 obj_type);

/*get a frame. The parser ALWAYS resync on the next object in the bitstream
thus you can seek the bitstream to copy the payload without re-seeking it */
GF_Err gf_m4v_parse_frame(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi, u8 *frame_type, u32 *time_inc, u64 *size, u64 *start, Bool *is_coded);
/*returns current object start in bitstream*/
u64 gf_m4v_get_object_start(GF_M4VParser *m4v);
/*decodes DSI/VOSHeader for MPEG4*/
GF_Err gf_m4v_get_config(u8 *rawdsi, u32 rawdsi_size, GF_M4VDecSpecInfo *dsi);
/*decodes DSI/VOSHeader for MPEG12*/
GF_Err gf_mpegv12_get_config(u8 *rawdsi, u32 rawdsi_size, GF_M4VDecSpecInfo *dsi);

/*rewrites PL code in DSI*/
void gf_m4v_rewrite_pl(u8 **io_dsi, u32 *io_dsi_len, u8 PL);
/*rewrites PAR code in DSI. Negative values will remove the par*/
GF_Err gf_m4v_rewrite_par(u8 **o_data, u32 *o_dataLen, s32 par_n, s32 par_d);

#endif /*GPAC_DISABLE_AV_PARSERS*/

/*returns readable description of profile*/
const char *gf_m4v_get_profile_name(u8 video_pl);

#ifndef GPAC_DISABLE_AV_PARSERS
s32 gf_mv12_next_start_code(u8 *pbuffer, u32 buflen, u32 *optr, u32 *scode);
s32 gf_mv12_next_slice_start(u8 *pbuffer, u32 startoffset, u32 buflen, u32 *slice_offset);

#endif /* GPAC_DISABLE_AV_PARSERS*/

#ifndef GPAC_DISABLE_AV_PARSERS

/*MP3 tools*/
u8 gf_mp3_num_channels(u32 hdr);
u16 gf_mp3_sampling_rate(u32 hdr);
u16 gf_mp3_window_size(u32 hdr);
u32 gf_mp3_bit_rate(u32 hdr);
u8 gf_mp3_object_type_indication(u32 hdr);
u8 gf_mp3_layer(u32 hdr);
u16 gf_mp3_frame_size(u32 hdr);
u32 gf_mp3_get_next_header(FILE* in);
u32 gf_mp3_get_next_header_mem(const u8 *buffer, u32 size, u32 *pos);

#endif /*GPAC_DISABLE_AV_PARSERS*/

u8 gf_mp3_version(u32 hdr);
const char *gf_mp3_version_name(u32 hdr);



#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined (GPAC_DISABLE_OGG)

/*ogg audio tools*/
typedef struct ogg_audio_codec_desc_t
{
	const char* codec_name;
	void *parserPrivateState;
	int channels;
	int sample_rate;

	/*process the data and returns the sample block_size (i.e. duration)*/
	GF_Err (*process)(struct ogg_audio_codec_desc_t *parserState, u8 *data, u32 data_length, void *importer, Bool *destroy_esd, u32 *track, u32 *di, u64 *duration, int *block_size);
	/*release private state*/
	void (*release)(struct ogg_audio_codec_desc_t *parserState);
} ogg_audio_codec_desc;

/*vorbis tools*/
typedef struct
{
	u32 version, num_headers;
	u32 min_block, max_block;
	u32 max_r, avg_r, low_r;

	/*do not touch, parser private*/
	u32 modebits;
	Bool mode_flag[64];
	u32 nb_init;
	GF_BitStream *vbs;

	u32 sample_rate, channels;
} GF_VorbisParser;

/*call with vorbis header packets - initializes the parser on success, leave it to NULL otherwise
returns 1 if success, 0 if error.*/
Bool gf_vorbis_parse_header(GF_VorbisParser *vp, u8 *data, u32 data_len);

/*returns 0 if init error or not a vorbis frame, otherwise returns the number of audio samples
in this frame*/
u32 gf_vorbis_check_frame(GF_VorbisParser *vp, u8 *data, u32 data_length);

/*opus tools*/
typedef struct
{
	u32 version;

	u32 sample_rate, channels;
	u8 OutputChannelCount;
	u16 PreSkip;
	u32 InputSampleRate;
	u16 OutputGain;

	u8 ChannelMappingFamily, StreamCount, CoupledCount;
	u8 ChannelMapping[255];
} GF_OpusParser;

/*call with vorbis header packets - initializes the parser on success, leave it to NULL otherwise
returns 1 if success, 0 if error.*/
Bool gf_opus_parse_header(GF_OpusParser *op, u8 *data, u32 data_len);

/*returns 0 if init error or not a vorbis frame, otherwise returns the number of audio samples
in this frame*/
u32 gf_opus_check_frame(GF_OpusParser *op, u8 *data, u32 data_length);

#endif /*!defined(GPAC_DISABLE_AV_PARSERS) && !defined (GPAC_DISABLE_OGG)*/

/*reads a 32 bit sync safe integer of id3v2*/
u32 gf_id3_read_size(GF_BitStream *bs);


enum
{
	GF_M4A_AAC_MAIN = 1,
	GF_M4A_AAC_LC = 2,
	GF_M4A_AAC_SSR = 3,
	GF_M4A_AAC_LTP = 4,
	GF_M4A_AAC_SBR = 5,
	GF_M4A_AAC_SCALABLE = 6,
	GF_M4A_TWINVQ = 7,
	GF_M4A_CELP = 8,
	GF_M4A_HVXC = 9,
	GF_M4A_TTSI = 12,
	GF_M4A_MAIN_SYNTHETIC = 13,
	GF_M4A_WAVETABLE_SYNTHESIS = 14,
	GF_M4A_GENERAL_MIDI = 15,
	GF_M4A_ALGO_SYNTH_AUDIO_FX = 16,
	GF_M4A_ER_AAC_LC = 17,
	GF_M4A_ER_AAC_LTP = 19,
	GF_M4A_ER_AAC_SCALABLE = 20,
	GF_M4A_ER_TWINVQ = 21,
	GF_M4A_ER_BSAC = 22,
	GF_M4A_ER_AAC_LD = 23,
	GF_M4A_ER_CELP = 24,
	GF_M4A_ER_HVXC = 25,
	GF_M4A_ER_HILN = 26,
	GF_M4A_ER_PARAMETRIC = 27,
	GF_M4A_SSC = 28,
	GF_M4A_AAC_PS = 29,
	GF_M4A_LAYER1 = 32,
	GF_M4A_LAYER2 = 33,
	GF_M4A_LAYER3 = 34,
	GF_M4A_DST = 35,
	GF_M4A_ALS = 36
};

#ifndef GPAC_DISABLE_AV_PARSERS

static const u32 GF_M4ASampleRates[] =
{
	96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

/*new values should now be defined in 23001-8*/
static const u32 GF_M4ANumChannels[] =
{
	1, 2, 3, 4, 5, 6, 8, 2, 3, 4, 7, 8, 24, 8, 12, 10, 12, 14
};

/*returns channel config value (as written in AAC DSI) for the given number of channels*/
u32 gf_m4a_get_channel_cfg(u32 nb_chan);

/*get Audio type from dsi. return audio codec type:*/
typedef struct
{
	u32 nb_chan;
	u32 base_object_type, base_sr, base_sr_index;
	/*SBR*/
	Bool has_sbr;
	u32 sbr_object_type, sbr_sr, sbr_sr_index;
	/*PS*/
	Bool has_ps;
	/*PL indication*/
	u8 audioPL;

	/*program config element*/
	Bool program_config_element_present, mono_mixdown_present, stereo_mixdown_present, matrix_mixdown_idx_present, pseudo_surround_enable ;
	u8 element_instance_tag, object_type, sampling_frequency_index, num_front_channel_elements, num_side_channel_elements, num_back_channel_elements, num_lfe_channel_elements, num_assoc_data_elements, num_valid_cc_elements;
	u8 mono_mixdown_element_number, stereo_mixdown_element_number, matrix_mixdown_idx;

	u8 front_element_is_cpe[15], front_element_tag_select[15];
	u8 side_element_is_cpe[15], side_element_tag_select[15];
	u8 back_element_is_cpe[15], back_element_tag_select[15];
	u8 lfe_element_tag_select[15];
	u8 assoc_data_element_tag_select[15];
	u8 cc_element_is_ind_sw[15], valid_cc_element_tag_select[15];
	u8 comment_field_bytes;
	u8 comments[255];
} GF_M4ADecSpecInfo;

/*parses dsi and updates audioPL*/
GF_Err gf_m4a_get_config(u8 *dsi, u32 dsi_size, GF_M4ADecSpecInfo *cfg);
/*gets audioPL for given cfg*/
u32 gf_m4a_get_profile(GF_M4ADecSpecInfo *cfg);

GF_Err gf_m4a_write_config(GF_M4ADecSpecInfo *cfg, u8 **dsi, u32 *dsi_size);
GF_Err gf_m4a_write_config_bs(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg);
GF_Err gf_m4a_parse_config(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg, Bool size_known);

#endif /*GPAC_DISABLE_AV_PARSERS*/

const char *gf_m4a_object_type_name(u32 objectType);
const char *gf_m4a_get_profile_name(u8 audio_pl);

#ifndef GPAC_DISABLE_AV_PARSERS


typedef struct
{
	u32 bitrate;
	u32 sample_rate;
	u32 framesize;
	u32 channels;
	u16 substreams; //bit-mask, used for channel map > 5.1
	/*only set if full parse*/
	u8 fscod, bsid, bsmod, acmod, lfon, brcode;
} GF_AC3Header;

Bool gf_ac3_parser(u8 *buffer, u32 buffer_size, u32 *pos, GF_AC3Header *out_hdr, Bool full_parse);
Bool gf_ac3_parser_bs(GF_BitStream *bs, GF_AC3Header *hdr, Bool full_parse);
Bool gf_eac3_parser_bs(GF_BitStream *bs, GF_AC3Header *hdr, Bool full_parse);
u32 gf_ac3_get_channels(u32 acmod);
u32 gf_ac3_get_bitrate(u32 brcode);

GF_Err gf_avc_get_sps_info(u8 *sps, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d);
GF_Err gf_avc_get_pps_info(u8 *pps, u32 pps_size, u32 *pps_id, u32 *sps_id);

//hevc_state is optional but shall be used for layer extensions since all size info is in VPS and not SPS
GF_Err gf_hevc_get_sps_info(u8 *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d);

#endif /*GPAC_DISABLE_AV_PARSERS*/

//get chroma format name from chroma format (1: 420, 2: 422, 3: 444
const char * gf_avc_hevc_get_chroma_format_name(u8 chroma_format);
/*get AVC profile name from profile indication*/
const char *gf_avc_get_profile_name(u8 profile_idc);
/*returns true if given profile is in range extensions*/
Bool gf_avc_is_rext_profile(u8 profile_idc);
/*get HEVC profile name from profile indication*/
const char *gf_hevc_get_profile_name(u8 profile_idc);


/*gets image size (bs must contain the whole image)
@codecid: image type (JPEG=0x6C, PNG=0x6D)
@width, height: image resolution - for jpeg max size if thumbnail included*/
void gf_img_parse(GF_BitStream *bs, u32 *codecid, u32 *width, u32 *height, u8 **dsi, u32 *dsi_len);

GF_Err gf_img_jpeg_dec(u8 *jpg, u32 jpg_size, u32 *width, u32 *height, u32 *pixel_format, u8 *dst, u32 *dst_size, u32 dst_nb_comp);

GF_Err gf_img_png_dec(u8 *png, u32 png_size, u32 *width, u32 *height, u32 *pixel_format, u8 *dst, u32 *dst_size);
GF_Err gf_img_file_dec(char *png_file, u32 *codecid, u32 *width, u32 *height, u32 *pixel_format, u8 **dst, u32 *dst_size);
GF_Err gf_img_png_enc(u8 *data, u32 width, u32 height, s32 stride, u32 pixel_format, u8 *dst, u32 *dst_size);



/*!\brief obu types. */
typedef enum {
	OBU_RESERVED_0 = 0,
	OBU_SEQUENCE_HEADER = 1,
	OBU_TEMPORAL_DELIMITER = 2,
	OBU_FRAME_HEADER = 3,
	OBU_TILE_GROUP = 4,
	OBU_METADATA = 5,
	OBU_FRAME = 6,
	OBU_REDUNDANT_FRAME_HEADER = 7,
	OBU_TILE_LIST = 8,
	OBU_RESERVED_9 = 9,
	OBU_RESERVED_10 = 10,
	OBU_RESERVED_11 = 11,
	OBU_RESERVED_12 = 12,
	OBU_RESERVED_13 = 13,
	OBU_RESERVED_14 = 14,
	OBU_PADDING = 15,
} ObuType;

/*!\brief obu metadata types. */
typedef enum {
	OBU_METADATA_TYPE_HDR_CLL = 1,
	OBU_METADATA_TYPE_HDR_MDCV = 2,
	OBU_METADATA_TYPE_SCALABILITY = 3,
	OBU_METADATA_TYPE_ITUT_T35 = 4,
	OBU_METADATA_TYPE_TIMECODE = 5
} ObuMetadataType;

const char *av1_get_obu_name(ObuType obu_type);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif	/*_GF_PARSERS_AV_H_*/

