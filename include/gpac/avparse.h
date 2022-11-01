/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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
\file <gpac/avparse.h>
\brief Utility tools for audio and video raw media parsing.
*/

/*!
\addtogroup media_grp

You will find in this module the documentation of all media tools in GPAC.
*/


/*!
\addtogroup avp_grp AV Parsing
\ingroup media_grp
\brief Utility tools for audio and video raw media parsing.

This section documents the audio and video parsing functions of the GPAC framework.
@{
*/


#include <gpac/bitstream.h>
#include <gpac/mpeg4_odf.h>



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

/*! inserts emulation prevention bytes from buffer_src into buffer_dst
\param buffer_src source buffer (NAL without EPB)
\param buffer_dst destination buffer (NAL with EPB)
\param nal_size source buffer size
\return size of buffer after adding emulation prevention bytes
*/
u32 gf_media_nalu_add_emulation_bytes(const u8 *buffer_src, u8 *buffer_dst, u32 nal_size);

/*! gets the number of emulation prevention bytes to add to a non emulated buffer
\param buffer source buffer (NAL without EPB)
\param nal_size source buffer size
\return the number of emulation prevention bytes to add
*/
u32 gf_media_nalu_emulation_bytes_add_count(u8 *buffer, u32 nal_size);

/*! MPEG-1, MPEG-1, MPEG-4 part 2 decoder specific info (only serialized for MPEG-4) */
typedef struct
{
	/*! video PL*/
	u8 VideoPL;
	/*! set if stream is RAP only*/
	u8 RAP_stream;
	/*! MPEG-4 part 2 video object type*/
	u8 objectType;
	/*! set if object has shape coding*/
	u8 has_shape;
	/*! set if object is an enhancement layer*/
	u8 enh_layer;
	/*! video horizontal size*/
	u16 width;
	/*! video vertical size*/
	u16 height;
	/*! pixel aspect ratio numerator*/
	u8 par_num;
	/*! pixel aspect ratio denominator*/
	u8 par_den;

	/*! video clock rate - frames are spaced by time_increment/clock_rate seconds*/
	u16 clock_rate;
	/*! number of bits to code the time increment (internal use only)*/
	u8 NumBitsTimeIncrement;
	/*! time increment between frames*/
	u32 time_increment;
	/*! framerate, for MPEG 1/2*/
	Double fps;
	/*! position of next object in the bitstream*/
	u32 next_object_start;

	/*! progressive video sequence */
	Bool progresive;
	/*! chroma format */
	u8 chroma_fmt;
} GF_M4VDecSpecInfo;


/*! MPEG (1,2,4) visual object parser (DSI extraction and timing/framing)*/
typedef struct __tag_m4v_parser GF_M4VParser;

#ifndef GPAC_DISABLE_AV_PARSERS
/*! Creates a new MPEG-1/2/4 video parser
\param data buffer to parse
\param data_size size of buffer to parse
\param mpeg12video if set, parses as MPEG-1 or MPEG-2
\return the created parser
*/
GF_M4VParser *gf_m4v_parser_new(u8 *data, u64 data_size, Bool mpeg12video);
/*! Creates a new MPEG-1/2/4 video parser from a bitstream object
\param bs the bitstream object to use for parsing
\param mpeg12video if set, parses as MPEG-1 or MPEG-2
\return the created parser
*/
GF_M4VParser *gf_m4v_parser_bs_new(GF_BitStream *bs, Bool mpeg12video);
/*! Deletes a MPEG-1/2/4 video parser
\param m4v the mpeg video parser
*/
void gf_m4v_parser_del(GF_M4VParser *m4v);
/*! Deletes a MPEG-1/2/4 video parser without destroying associated bitstream
\param m4v the mpeg video parser
*/
void gf_m4v_parser_del_no_bs(GF_M4VParser *m4v);
/*! parses the decoder specific info (if found)
\param m4v the mpeg video parser
\param dsi the decoder spcific info structure to fill
\return GF_OK if found, GF_EOS if not enough data, error otherwise
*/
GF_Err gf_m4v_parse_config(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi);
/*! resets the parser
\param m4v the mpeg video parser
\param obj_type if not 0, skip next start code and use (obj_type-1) for next obj parsing
*/
void gf_m4v_parser_reset(GF_M4VParser *m4v, u8 obj_type);

/*! parses a frame. The parser ALWAYS resync on the next object in the bitstream
thus you can seek the bitstream to copy the payload without re-seeking it
\param m4v the mpeg video parser
\param dsi pointer to the decoder specific info parsed
\param frame_type set to the frame type (I:1, P:2, B:3, 0: no frame header found)
\param time_inc set to the time increment since last frame
\param size set to the size of the compressed frame
\param start set to the position of the first byte in the buffer/bitstream
\param is_coded set to 1 if frame is coded, 0 if skip frame, untouched if no frame found
\return error if any
*/
GF_Err gf_m4v_parse_frame(GF_M4VParser *m4v, GF_M4VDecSpecInfo *dsi, u8 *frame_type, u32 *time_inc, u64 *size, u64 *start, Bool *is_coded);
/*! returns current object start in bitstream
\param m4v the mpeg video parser
\return the the position in the buffer/bitstream
*/
u64 gf_m4v_get_object_start(GF_M4VParser *m4v);
/*! decodes DSI/VOSHeader for MPEG4
\param rawdsi encoded MPEG-4 decoder config
\param rawdsi_size size of encoded MPEG-4 decoder config
\param dsi the decoder spcific info structure to fill
\return error if any
*/
GF_Err gf_m4v_get_config(u8 *rawdsi, u32 rawdsi_size, GF_M4VDecSpecInfo *dsi);
/*! decodes DSI/VOSHeader for MPEG12
\param rawdsi encoded MPEG-1/2 decoder config
\param rawdsi_size size of encoded MPEG-1/2 decoder config
\param dsi the decoder spcific info structure to fill
\return error if any
*/
GF_Err gf_mpegv12_get_config(u8 *rawdsi, u32 rawdsi_size, GF_M4VDecSpecInfo *dsi);

/*! rewrites Profile and Level indicator in MPEG-4 DSI
\param io_dsi encoded MPEG-4 decoder config
\param io_dsi_len size of encoded MPEG-4 decoder config
\param PL the new Profile/level to set
*/
void gf_m4v_rewrite_pl(u8 **io_dsi, u32 *io_dsi_len, u8 PL);
/*! rewrites PAR code in DSI. Negative values will remove the par
\param io_dsi encoded MPEG-4 decoder config
\param io_dsi_len size of encoded MPEG-4 decoder config
\param par_n the numerator of the new aspect ratio to write
\param par_d the denominator of the new aspect ratio to write
\return error if any
*/
GF_Err gf_m4v_rewrite_par(u8 **io_dsi, u32 *io_dsi_len, s32 par_n, s32 par_d);

#endif /*GPAC_DISABLE_AV_PARSERS*/

/*! returns readable description of profile
\param video_pl the Profile/level
\return name of the profile (never NULL)
*/
const char *gf_m4v_get_profile_name(u8 video_pl);

#ifndef GPAC_DISABLE_AV_PARSERS
/*! returns next start code in mpeg 1/2 video buffer
\param pbuffer the video buffer
\param buflen size of the video buffer
\param optr set to the byte offset in the buffer
\param scode set to the start code value if found
\return 0 if found, -1 otherwise
*/
s32 gf_mv12_next_start_code(u8 *pbuffer, u32 buflen, u32 *optr, u32 *scode);
/*! returns next slice start in mpeg 1/2 video buffer
\param pbuffer the video buffer
\param startoffset the offset in the buffer at which analysis shall begin
\param buflen size of the video buffer
\param slice_offset set to the byte offset of the slice start
\return 0 if found, -1 otherwise
*/
s32 gf_mv12_next_slice_start(u8 *pbuffer, u32 startoffset, u32 buflen, u32 *slice_offset);

#endif /* GPAC_DISABLE_AV_PARSERS*/

#ifndef GPAC_DISABLE_AV_PARSERS

/*MP3 tools*/
/*! gets the number of channels in an MPEG-1/2/3 audio frame
\param hdr the frame header
\return the number of channels
*/
u8 gf_mp3_num_channels(u32 hdr);
/*! gets the sampling rate of an MPEG-1/2/3 audio frame
\param hdr the frame header
\return the sampling rate
*/
u16 gf_mp3_sampling_rate(u32 hdr);
/*! gets the window size (number of samples) in an MPEG-1/2/3 audio frame
\param hdr the frame header
\return the window size
*/
u16 gf_mp3_window_size(u32 hdr);
/*! gets the bitrate of an MPEG-1/2/3 audio frame
\param hdr the frame header
\return the bit rate
*/
u32 gf_mp3_bit_rate(u32 hdr);
/*! gets the MPEG-4 object type indication of an MPEG-1/2/3 audio frame
\param hdr the frame header
\return the object type indication
*/
u8 gf_mp3_object_type_indication(u32 hdr);
/*! gets the layer of an MPEG-1/2/3 audio frame
\param hdr the frame header
\return the layer
*/
u8 gf_mp3_layer(u32 hdr);
/*! gets the frame size of an MPEG-1/2/3 audio frame
\param hdr the frame header
\return the size in bytes (includes the 4 bytes header size)
*/
u16 gf_mp3_frame_size(u32 hdr);
/*! locates the next frame start in an MPEG-1/2/3 audio stream file
\param fin file to search
\return the frame header value, or 0 if not found
*/
u32 gf_mp3_get_next_header(FILE* fin);
/*! locates the next frame start in an MPEG-1/2/3 audio buffer
\param buffer buffer to search
\param size size of buffer to search
\param pos set to the start position of the frame header in the buffer
\return the frame header value, or 0 if not found
*/
u32 gf_mp3_get_next_header_mem(const u8 *buffer, u32 size, u32 *pos);

#endif /*GPAC_DISABLE_AV_PARSERS*/

/*! gets the version size of an MPEG-1/2/3 audio frame
\param hdr the frame header
\return the version
*/
u8 gf_mp3_version(u32 hdr);
/*! gets the version name of an MPEG-1/2/3 audio frame
\param hdr the frame header
\return the version name
*/
const char *gf_mp3_version_name(u32 hdr);


#if !defined(GPAC_DISABLE_AV_PARSERS) && !defined (GPAC_DISABLE_OGG)

/*! OGG audio codec descriptor*/
typedef struct ogg_audio_codec_desc_t
{
	/*! name of the codec*/
	const char* codec_name;
	/*! private*/
	void *parserPrivateState;
	/*! number of channels*/
	int channels;
	/*! samplerate*/
	int sample_rate;

	/*! process the data and returns the sample block_size (i.e. duration)*/
	GF_Err (*process)(struct ogg_audio_codec_desc_t *parserState, u8 *data, u32 data_length, void *importer, Bool *destroy_esd, u32 *track, u32 *di, u64 *duration, int *block_size);
	/*! release private state*/
	void (*release)(struct ogg_audio_codec_desc_t *parserState);
} ogg_audio_codec_desc;

/*! OGG Vorbis parser*/
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

/*! parses vorbis header packets
 	initializes the parser on success, leave it to NULL otherwise
\param vp pointer to a vorbis parser to use
\param data source buffer
\param data_len size of buffer
\return GF_TRUE if success, GF_FALSE otherwise
*/
Bool gf_vorbis_parse_header(GF_VorbisParser *vp, u8 *data, u32 data_len);

/*! checks vorbis frame
\param vp the vorbis parser to use
\param data source buffer
\param data_len size of buffer
\return 0 if init error or not a vorbis frame, otherwise returns the number of audio samples in this frame
*/
u32 gf_vorbis_check_frame(GF_VorbisParser *vp, u8 *data, u32 data_len);

/*! parses opus header packets - initializes the config  on success, leave it to NULL otherwise
\param cfg pointer to a opus config to fill
\param data opus header buffer to parse
\param data_len size of opus header buffer
\return 1 if success, 0 if error
*/
Bool gf_opus_parse_header(GF_OpusConfig *cfg, u8 *data, u32 data_len);

/*! checks if an opus frame is valid
\param cfg pointer to a opus config to use
\param data source buffer
\param data_len size of buffer
\return 0 if init error or not a vorbis frame, otherwise returns the number of audio samples in this frame*/
u32 gf_opus_check_frame(GF_OpusConfig *cfg, u8 *data, u32 data_len);

#endif /*!defined(GPAC_DISABLE_AV_PARSERS) && !defined (GPAC_DISABLE_OGG)*/

/*! reads escaped value according to usac/mpegh
\param bs bitstream object
\param nBits1 first number of bits
\param nBits2 second number of bits
\param nBits3 third number set of bits
\return value read
*/
u64 gf_mpegh_escaped_value(GF_BitStream *bs, u32 nBits1, u32 nBits2, u32 nBits3);

/*! parse profile and level from a MHAS payload
\param ptr the MHAS payhload
\param size size of the MHAS payhload
\param chan_layout set to the channel layout if found, 0 otherwise - optional, may be NULL
\return the MHAS profile found, or -1 of not found
*/
s32 gf_mpegh_get_mhas_pl(u8 *ptr, u32 size, u64 *chan_layout);

/*! reads a 32 bit sync safe integer of id3v2 from a bitstream object
\param bs the bitstream object to use - has to be positioned on the start if an id3v2 size field
\return the id3v2 size field read
*/
u32 gf_id3_read_size(GF_BitStream *bs);

/*! MPEG-4 audio object types*/
enum
{
	/*! AAC main*/
	GF_M4A_AAC_MAIN = 1,
	/*! AAC LC*/
	GF_M4A_AAC_LC = 2,
	/*! AAC SSR*/
	GF_M4A_AAC_SSR = 3,
	/*! AAC LTP*/
	GF_M4A_AAC_LTP = 4,
	/*! AAC SBR*/
	GF_M4A_AAC_SBR = 5,
	/*! AAC scalable*/
	GF_M4A_AAC_SCALABLE = 6,
	/*! TwinVQ*/
	GF_M4A_TWINVQ = 7,
	/*! CELP*/
	GF_M4A_CELP = 8,
	/*! HVXC*/
	GF_M4A_HVXC = 9,
	/*! TTSI*/
	GF_M4A_TTSI = 12,
	/*! Main Synthetic*/
	GF_M4A_MAIN_SYNTHETIC = 13,
	/*! Wavetable synthesis*/
	GF_M4A_WAVETABLE_SYNTHESIS = 14,
	/*! General Midi*/
	GF_M4A_GENERAL_MIDI = 15,
	/*! AudioFX synthesis*/
	GF_M4A_ALGO_SYNTH_AUDIO_FX = 16,
	/*! Error Resilient AAC LC*/
	GF_M4A_ER_AAC_LC = 17,
	/*! Error Resilient AAC LTP*/
	GF_M4A_ER_AAC_LTP = 19,
	/*! Error Resilient AAC Scalable*/
	GF_M4A_ER_AAC_SCALABLE = 20,
	/*! Error Resilient TwinVQ*/
	GF_M4A_ER_TWINVQ = 21,
	/*! Error Resilient BSAC*/
	GF_M4A_ER_BSAC = 22,
	/*! Error Resilient AAC LD*/
	GF_M4A_ER_AAC_LD = 23,
	/*! Error Resilient CELP*/
	GF_M4A_ER_CELP = 24,
	/*! Error Resilient HVXC*/
	GF_M4A_ER_HVXC = 25,
	/*! Error Resilient HILN*/
	GF_M4A_ER_HILN = 26,
	/*! Error Resilient Parametric*/
	GF_M4A_ER_PARAMETRIC = 27,
	/*! SSC*/
	GF_M4A_SSC = 28,
	/*! AAC PS*/
	GF_M4A_AAC_PS = 29,
	/*! MPEG-1/2 layer 1*/
	GF_M4A_LAYER1 = 32,
	/*! MPEG-1/2 layer 2*/
	GF_M4A_LAYER2 = 33,
	/*! MPEG-1/2 layer 3*/
	GF_M4A_LAYER3 = 34,
	/*! DST*/
	GF_M4A_DST = 35,
	/*! ALS*/
	GF_M4A_ALS = 36,
	/*! USAC*/
	GF_M4A_USAC = 42,
};

/*! AAC sample rates*/
static const u32 GF_M4ASampleRates[] =
{
	96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

/*! AAC channel configurations*/
static const u32 GF_M4ANumChannels[] =
{
	1, 2, 3, 4, 5, 6, 8, 0, 0, 0, 7, 8, 0, 8, 0
};

#ifndef GPAC_DISABLE_AV_PARSERS

/*! returns channel config value (as written in AAC DSI) for the given number of channels
\param nb_chan the number of channels
\return the MPEG-4 AAC channel config value
*/
u32 gf_m4a_get_channel_cfg(u32 nb_chan);

/*! MPEG-4 Audio decoder specific info*/
typedef struct
{
	/*Number of channels (NOT AAC channel confgiuration), 0 if unknown in which case program_config_element must be set*/
	u32 nb_chan;
	/*base audio object type*/
	u32 base_object_type;
	/*base sample rate*/
	u32 base_sr;
	/*index of base sample rate*/
	u32 base_sr_index;
	/*set if SBR (Spectral Band Replication) is present*/
	Bool has_sbr;
	/*sbr audio object type*/
	u32 sbr_object_type;
	/*SBR sample rate*/
	u32 sbr_sr;
	/*SBR sample index*/
	u32 sbr_sr_index;
	/*set if PS (Parametric Stereo) is present*/
	Bool has_ps;
	/*audio Profile level indication*/
	u8 audioPL;
	/*channel configuration, only set when parsing (when writing, recomputed from nb_chan)*/
	u32 chan_cfg;

	/*set if program config element is present - members until end of struct are ignored/invalid if this is not set*/
	Bool program_config_element_present;
	/*set if mono mixdown is present*/
	Bool mono_mixdown_present;
	/*set if stereo mixdown is present*/
	Bool stereo_mixdown_present;
	/*set if matrix mixdown is present*/
	Bool matrix_mixdown_idx_present;
	/*set if pseudo surround is present*/
	Bool pseudo_surround_enable;
	/*element instance*/
	u8 element_instance_tag;
	/*object type*/
	u8 object_type;
	/*samplerate index*/
	u8 sampling_frequency_index;
	/*number of front channels*/
	u8 num_front_channel_elements;
	/*number of side channels*/
	u8 num_side_channel_elements;
	/*number of back channels*/
	u8 num_back_channel_elements;
	/*number of LFE channels*/
	u8 num_lfe_channel_elements;
	/*number of associated data channels*/
	u8 num_assoc_data_elements;
	/*number of valid CC elements*/
	u8 num_valid_cc_elements;
	/*id of the mono mixdown element*/
	u8 mono_mixdown_element_number;
	/*id of the stereo mixdown element*/
	u8 stereo_mixdown_element_number;
	/*id of the matrix mixdown element*/
	u8 matrix_mixdown_idx;
	/*front channels cpe flags*/
	u8 front_element_is_cpe[15];
	/*front channels select flags*/
	u8 front_element_tag_select[15];
	/*side channels cpe flags*/
	u8 side_element_is_cpe[15];
	/*side channels select flags*/
	u8 side_element_tag_select[15];
	/*back channels cpe flags*/
	u8 back_element_is_cpe[15];
	/*back channels select flags*/
	u8 back_element_tag_select[15];
	/*LFE channels select flags*/
	u8 lfe_element_tag_select[15];
	/*associated data select flags*/
	u8 assoc_data_element_tag_select[15];
	/*CC elements indep flags*/
	u8 cc_element_is_ind_sw[15];
	/*CC elements select flags*/
	u8 valid_cc_element_tag_select[15];
	/*size of comment field*/
	u8 comment_field_bytes;
	/*comment field*/
	u8 comments[255];
	//set after parsing program_config_element
	u32 cpe_channels;
} GF_M4ADecSpecInfo;

/*! parses MPEG-4 audio dsi
\param dsi the buffer containing the decoder config
\param dsi_size size of the buffer containing the decoder config
\param cfg will be filled with the parsed value
\return error code if any
*/
GF_Err gf_m4a_get_config(u8 *dsi, u32 dsi_size, GF_M4ADecSpecInfo *cfg);
/*! gets audio profile and level for a given configuration
\param cfg the parsed MPEG-4 audio configuration
\return audio PL
*/
u32 gf_m4a_get_profile(GF_M4ADecSpecInfo *cfg);

/*! writes MPEG-4 audio dsi in a byte buffer - backward-compatible signaling extensions are not written
\param cfg the configuration to write
\param dsi set to the encoded buffer (to be freed by caller)
\param dsi_size set to the size of the encoded buffer
\return error code if any
*/
GF_Err gf_m4a_write_config(GF_M4ADecSpecInfo *cfg, u8 **dsi, u32 *dsi_size);
/*! writes MPEG-4 audio dsi in a bitstream object - backward-compatible signaling extensions are not written
\param bs the bitstream object to write to
\param cfg the configuration to write
\return error code if any
*/
GF_Err gf_m4a_write_config_bs(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg);
/*! parses MPEG-4 audio dsi from bitstream
\param bs the bitstream object to use (shall start in the beginning of the dsi)
\param cfg will be filled with the parsed value
\param size_known set to GF_TRUE if the bitstream contains the complete DSI (and only it), to parse backward-compatible extensions
\return error code if any
*/
GF_Err gf_m4a_parse_config(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg, Bool size_known);


/*! reads program config element of MPEG-4 audio dsi
\param bs the bitstream object to use (shall start in the beginning of the dsi)
\param cfg the config to fill
\return error code if any
*/
GF_Err gf_m4a_parse_program_config_element(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg);

/*! writes program config element of MPEG-4 audio dsi
\param bs the bitstream object to use (shall start in the beginning of the dsi)
\param cfg the config to write
\return error code if any
*/
GF_Err gf_m4a_write_program_config_element_bs(GF_BitStream *bs, GF_M4ADecSpecInfo *cfg);

#endif /*GPAC_DISABLE_AV_PARSERS*/

/*! gets the name of a given MPEG-4 audio object type
\param objectType the object type
\return the MPEG-4 audio object type name
*/
const char *gf_m4a_object_type_name(u32 objectType);
/*! gets the name of the the given MPEG-4 audio profile
\param audio_pl the profile/level
\return the MPEG-4 audio profile name
*/
const char *gf_m4a_get_profile_name(u8 audio_pl);

#ifndef GPAC_DISABLE_AV_PARSERS

//! \cond old name
typedef struct __ac3_config GF_AC3Header;
//! \endcond 

/*! parses an AC-3 header from a buffer
\param buffer buffer to parse
\param buffer_size size of buffer to parse
\param pos set to start offset (in bytes) of the AC3 header parsed
\param out_hdr will be filled by parser
\param full_parse if GF_TRUE, complete parsing of the header will be done
\return GF_TRUE if success
*/
Bool gf_ac3_parser(u8 *buffer, u32 buffer_size, u32 *pos, GF_AC3Config *out_hdr, Bool full_parse);
/*! parses an AC-3 header from a bitstream
\param bs bitstream to parse
\param hdr will be filled by parser
\param full_parse if GF_TRUE, complete parsing of the header will be done
\return GF_TRUE if success
*/
Bool gf_ac3_parser_bs(GF_BitStream *bs, GF_AC3Config *hdr, Bool full_parse);

/*! parses an EAC-3 header from a buffer and checks for next frame/blocks presence
\param buffer buffer to parse
\param buffer_size size of buffer to parse
\param pos set to start offset (in bytes) of the AC3 header parsed
\param hdr will be filled by parser
\param full_parse if GF_TRUE, complete parsing of the header and check for next frame/blocks presence will be done
\return GF_TRUE if success
*/
Bool gf_eac3_parser(u8 *buffer, u32 buffer_size, u32 *pos, GF_AC3Config *hdr, Bool full_parse);

/*! parses an EAC-3 header from a bitstream
\param bs bitstream to parse
\param hdr will be filled by parser
\param full_parse if GF_TRUE, complete parsing of the header and check for next frame/blocks presence will be done
\return GF_TRUE if success
*/
Bool gf_eac3_parser_bs(GF_BitStream *bs, GF_AC3Config *hdr, Bool full_parse);

/*! gets the number of channels from chan_loc info of EAC3 config
\param chan_loc acmod of the associated frame header
\return number of channels
*/
u32 gf_eac3_get_chan_loc_count(u32 chan_loc);

/*! gets the total number of channels in an AC3 frame, including surround but not lfe
\param acmod acmod of the associated frame header
\return number of channels
*/
u32 gf_ac3_get_total_channels(u32 acmod);
/*! gets the number of surround channels in an AC3 frame
\param acmod acmod of the associated frame header
\return number of surround channels
*/
u32 gf_ac3_get_surround_channels(u32 acmod);
/*! gets the bitrate of an AC3 frame
\param brcode brcode of the associated frame header
\return bitrate in bps
*/
u32 gf_ac3_get_bitrate(u32 brcode);

/*! gets basic information from an AVC Sequence Parameter Set
\param sps SPS NAL buffer
\param sps_size size of buffer
\param sps_id set to the ID
\param width set to the width
\param height set to the height
\param par_n set to the pixel aspect ratio numerator
\param par_d set to the pixel aspect ratio denominator
\return error code if any
*/
GF_Err gf_avc_get_sps_info(u8 *sps, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d);
/*! gets basic information from an AVC Picture Parameter Set
\param pps PPS NAL buffer
\param pps_size size of buffer
\param pps_id set to the ID of the PPS
\param sps_id set to the SPS ID of the PPS
\return error code if any
*/
GF_Err gf_avc_get_pps_info(u8 *pps, u32 pps_size, u32 *pps_id, u32 *sps_id);

/*! gets basic information from an HEVC Sequence Parameter Set
\param sps_data SPS NAL buffer
\param sps_size size of buffer
\param sps_id set to the ID
\param width set to the width
\param height set to the height
\param par_n set to the pixel aspect ratio numerator
\param par_d set to the pixel aspect ratio denominator
\return error code if any
*/
GF_Err gf_hevc_get_sps_info(u8 *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d);

/*! gets basic information from a VVC Sequence Parameter Set
\param sps_data SPS NAL buffer
\param sps_size size of buffer
\param sps_id set to the ID
\param width set to the width
\param height set to the height
\param par_n set to the pixel aspect ratio numerator
\param par_d set to the pixel aspect ratio denominator
\return error code if any
*/
GF_Err gf_vvc_get_sps_info(u8 *sps_data, u32 sps_size, u32 *sps_id, u32 *width, u32 *height, s32 *par_n, s32 *par_d);

#endif /*GPAC_DISABLE_AV_PARSERS*/

/*! get VVC profile name
\param video_prof profile value
\return the profile name
*/
const char *gf_vvc_get_profile_name(u8 video_prof);

/*! gets chroma format name from MPEG chroma format
\param chroma_format the chroma format to query (1: 420, 2: 422, 3: 444)
\return the name of the format
*/
const char * gf_avc_hevc_get_chroma_format_name(u8 chroma_format);
/*! gets AVC profile name from profile indication
\param profile_idc the PL indication
\return the name of the profile
*/
const char *gf_avc_get_profile_name(u8 profile_idc);

/*! checks if avcc extensions are used for this profile
\param profile_idc the PL indication
\return GF_TRUE if extensions must be written in avcc for the given profile
*/
Bool gf_avcc_use_extensions(u8 profile_idc);

/*! gets HEVC profile name from profile indication
\param profile_idc the PL indication
\return the name of the profile
*/
const char *gf_hevc_get_profile_name(u8 profile_idc);


/*! parses an image from a bitstream oject. The bitstream must contain the whole image. If a thumbnail is included in the image, the indicated resolution is the one of the main image.
\param bs the source bitstream
\param codecid set to the codec ID of the image
\param width set to the width of the image
\param height set to the height of the image
\param dsi set to a buffer containing the decoder config of the image if any (in whihc case this buffer shall be freed by the caller)
\param dsi_len set to the allocated buffer size
*/
void gf_img_parse(GF_BitStream *bs, u32 *codecid, u32 *width, u32 *height, u8 **dsi, u32 *dsi_len);

/*! decodes a JPEG image in a preallocated buffer
\param jpg the JPEG buffer
\param jpg_size size of the JPEG buffer
\param width set to width of the image
\param height set to height of the image
\param pixel_format set to pixel format of the image
\param dst buffer to hold the decoded pixels (may be NULL)
\param dst_size size in bytes of the buffer to hold the decoded pixels (may be 0)
\param dst_nb_comp number of components in destination buffer
\return GF_BUFFER_TOO_SMALL if destination buffer is too small or error if any
*/
GF_Err gf_img_jpeg_dec(u8 *jpg, u32 jpg_size, u32 *width, u32 *height, u32 *pixel_format, u8 *dst, u32 *dst_size, u32 dst_nb_comp);

/*! decodes a PNG image in a preallocated buffer
\param png the PNG buffer
\param png_size size of the PNG buffer
\param width set to width of the image
\param height set to height of the image
\param pixel_format set to pixel format of the image
\param dst buffer to hold the decoded pixels (may be NULL)
\param dst_size size in bytes of the buffer to hold the decoded pixels (may be 0)
\return GF_BUFFER_TOO_SMALL if destination buffer is too small or error if any
*/
GF_Err gf_img_png_dec(u8 *png, u32 png_size, u32 *width, u32 *height, u32 *pixel_format, u8 *dst, u32 *dst_size);
/*! encodes a raw image into a PNG image
\param data the pixel data
\param width the pixel width
\param height the pixel height
\param stride the pixel horizontal stride
\param pixel_format pixel format of the image
\param dst buffer to hold the decoded pixels (may be NULL)
\param dst_size set to the size in bytes of the buffer to hold the decoded pixels (may be 0)
\return GF_BUFFER_TOO_SMALL if destination buffer is too small or error if any
*/
GF_Err gf_img_png_enc(u8 *data, u32 width, u32 height, s32 stride, u32 pixel_format, u8 *dst, u32 *dst_size);


/*!\brief OBU types*/
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

/*!\brief OBU metadata types*/
typedef enum {
	OBU_METADATA_TYPE_HDR_CLL = 1,
	OBU_METADATA_TYPE_HDR_MDCV = 2,
	OBU_METADATA_TYPE_SCALABILITY = 3,
	OBU_METADATA_TYPE_ITUT_T35 = 4,
	OBU_METADATA_TYPE_TIMECODE = 5
} ObuMetadataType;

/*! gets the name of a given OBU type
\param obu_type the OBU type
\return the OBU name
*/
const char *gf_av1_get_obu_name(ObuType obu_type);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif	/*_GF_PARSERS_AV_H_*/

