/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / exported constants
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

#ifndef _GF_CONSTANTS_H_
#define _GF_CONSTANTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/setup.h>

/*!
\file <gpac/constants.h>
\brief Most constants defined in GPAC are in this file.
\addtogroup cst_grp
\brief Constants

This section documents some constants used in the GPAC framework which are not related to any specific sub-project.

@{
*/


/*!
\brief Supported media stream types
\hideinitializer

Supported media stream types for media objects.
*/
enum
{
	/*!Unknown stream type*/
	GF_STREAM_UNKNOWN = 0,
	/*!MPEG-4 Object Descriptor Stream*/
	GF_STREAM_OD		= 0x01,
	/*!MPEG-4 Object Clock Reference Stream*/
	GF_STREAM_OCR		= 0x02,
	/*!MPEG-4 Scene Description Stream*/
	GF_STREAM_SCENE		= 0x03,
	/*!Visual Stream (Video, Image or MPEG-4 SNHC Tools)*/
	GF_STREAM_VISUAL	= 0x04,
	/*!Audio Stream (Audio, MPEG-4 Structured-Audio Tools)*/
	GF_STREAM_AUDIO		= 0x05,
	/*!MPEG-7 Description Stream*/
	GF_STREAM_MPEG7		= 0x06,
	/*!MPEG-4 Intellectual Property Management and Protection Stream*/
	GF_STREAM_IPMP		= 0x07,
	/*!MPEG-4 Object Content Information Stream*/
	GF_STREAM_OCI		= 0x08,
	/*!MPEG-4 MPEGlet Stream*/
	GF_STREAM_MPEGJ		= 0x09,
	/*!MPEG-4 User Interaction Stream*/
	GF_STREAM_INTERACT	= 0x0A,
	/*!MPEG-4 IPMP Tool Stream*/
	GF_STREAM_IPMP_TOOL	= 0x0B,
	/*!MPEG-4 Font Data Stream*/
	GF_STREAM_FONT		= 0x0C,
	/*!MPEG-4 Streaming Text Stream*/
	GF_STREAM_TEXT		= 0x0D,

	/* From 0x20 to Ox3F, this is the user private range */

	/*!Nero Digital Subpicture Stream*/
	GF_STREAM_ND_SUBPIC = 0x38,

	/*GPAC internal stream types*/


	/*!GPAC Private Scene streams\n
	\n
	\note
	This stream type (MPEG-4 user-private) is reserved for streams used to create a scene decoder
	handling the scene without input streams, as is the case for file readers (BT/VRML/XML..).\n
	*/
	GF_STREAM_PRIVATE_SCENE	= 0x20,

	GF_STREAM_METADATA = 0x21,

	GF_STREAM_ENCRYPTED		= 0xE0,
	/*stream carries files, each file being a complete AU*/
	GF_STREAM_FILE		= 0xE1,

	//other stream types may be declared using their handler 4CC as defined in ISOBMFF
};

/*! Gets the stream type name based on stream type
\param streamType stream type GF_STREAM_XXX as defined in constants.h
\return NULL if unknown, otherwise value
 */
const char *gf_stream_type_name(u32 streamType);

/*! Gets the stream type short name based on stream type (usually the lower case value of the stream name)
\param streamType stream type GF_STREAM_XXX as defined in constants.h
\return NULL if unknown, otherwise value
 */
const char *gf_stream_type_short_name(u32 streamType);

/*! Gets the stream type by name
\param name name of the stream type to query
\return GF_STREAM_UNKNOWN if unknown, otherwise GF_STREAM_XXX value
 */
u32 gf_stream_type_by_name(const char *name);

/*! Enumerates defined stream types
\param idx index of the stream type, 0-based
\param name name of the stream type (used when parsing stream type from textual definition)
\param desc description  of the stream type
\return stream type, or GF_STREAM_UNKNOWN if no more stream types
 */
u32 gf_stream_types_enum(u32 *idx, const char **name, const char **desc);

#ifndef GF_4CC
/*! macro for 4CC*/
#define GF_4CC(a,b,c,d) ((((u32)a)<<24)|(((u32)b)<<16)|(((u32)c)<<8)|(d))
#endif

/*!
\brief Pixel Formats

Supported pixel formats for everything using video
*/
typedef enum
{
	/*!8 bit GREY */
	GF_PIXEL_GREYSCALE	=	GF_4CC('G','R','E','Y'),
	/*!16 bit greyscale, first alpha, then grey*/
	GF_PIXEL_ALPHAGREY	=	GF_4CC('G','R','A','L'),
	/*!16 bit greyscale, first grey, then alpha*/
	GF_PIXEL_GREYALPHA	=	GF_4CC('A','L','G','R'),
	/*!12 bit RGB on 16 bits (4096 colors)*/
	GF_PIXEL_RGB_444	=	GF_4CC('R','4','4','4'),
	/*!15 bit RGB*/
	GF_PIXEL_RGB_555	=	GF_4CC('R','5','5','5'),
	/*!16 bit RGB*/
	GF_PIXEL_RGB_565	=	GF_4CC('R','5','6','5'),
	/*!24 bit RGB*/
	GF_PIXEL_RGB		=	GF_4CC('R','G','B','3'),
	/*!24 bit BGR*/
	GF_PIXEL_BGR		=	GF_4CC('B','G','R','3'),
	/*!32 bit RGB. Component ordering in bytes is R-G-B-X.*/
	GF_PIXEL_RGBX		=	GF_4CC('R','G','B','4'),
	/*!32 bit BGR. Component ordering in bytes is B-G-R-X.*/
	GF_PIXEL_BGRX		=	GF_4CC('B','G','R','4'),
	/*!32 bit RGB. Component ordering in bytes is X-R-G-B.*/
	GF_PIXEL_XRGB		=	GF_4CC('R','G','B','X'),
	/*!32 bit BGR. Component ordering in bytes is X-B-G-R.*/
	GF_PIXEL_XBGR		=	GF_4CC('B','G','R','X'),

	/*!32 bit ARGB. Component ordering in bytes is A-R-G-B.*/
	GF_PIXEL_ARGB		=	GF_4CC('A','R','G','B'),
	/*!32 bit RGBA (OpenGL like). Component ordering in bytes is R-G-B-A.*/
	GF_PIXEL_RGBA		=	GF_4CC('R','G','B', 'A'),
	/*!32 bit BGRA. Component ordering in bytes is B-G-R-A.*/
	GF_PIXEL_BGRA		=	GF_4CC('B','G','R','A'),
	/*!32 bit ABGR. Component ordering in bytes is A-B-G-R.*/
	GF_PIXEL_ABGR		=	GF_4CC('A','B','G','R'),

	/*!RGB24 + depth plane. Component ordering in bytes is R-G-B-D.*/
	GF_PIXEL_RGBD		=	GF_4CC('R', 'G', 'B', 'D'),
	/*!RGB24 + depth plane (7 lower bits) + shape mask. Component ordering in bytes is R-G-B-(S+D).*/
	GF_PIXEL_RGBDS		=	GF_4CC('3', 'C', 'D', 'S'),

	/*internal format for OpenGL using pachek RGB 24 bit plus planar depth plane at the end of the image*/
	GF_PIXEL_RGB_DEPTH = GF_4CC('R', 'G', 'B', 'd'),

	/*generic pixel format uncv from ISO/IEC 23001-17*/
	GF_PIXEL_UNCV = GF_4CC('u', 'n', 'c', 'v'),

	/*!YUV packed 422 format*/
	GF_PIXEL_YUYV		=	GF_4CC('Y','U','Y','V'),
	/*!YUV packed 422 format*/
	GF_PIXEL_YVYU		=	GF_4CC('Y','V','Y','U'),
	/*!YUV packed 422 format*/
	GF_PIXEL_UYVY		=	GF_4CC('U','Y','V','Y'),
	/*!YUV packed 422 format*/
	GF_PIXEL_VYUY		=	GF_4CC('V','Y','U','Y'),

	/*!YUV packed 422 format 10 bits, little endian*/
	GF_PIXEL_YUYV_10		=	GF_4CC('Y','U','Y','L'),
	/*!YUV packed 422 format 10 bits, little endian*/
	GF_PIXEL_YVYU_10		=	GF_4CC('Y','V','Y','L'),
	/*!YUV packed 422 format 10 bits, little endian*/
	GF_PIXEL_UYVY_10		=	GF_4CC('U','Y','V','L'),
	/*!YUV packed 422 format 10 bits, little endian*/
	GF_PIXEL_VYUY_10		=	GF_4CC('V','Y','U','L'),

	/*!YUV planar format*/
	GF_PIXEL_YUV		=	GF_4CC('Y','U','1','2'),
	/*!YVU planar format*/
	GF_PIXEL_YVU		=	GF_4CC('Y','V','1','2'),
	/*!YUV420p in 10 bits mode, little endian*/
	GF_PIXEL_YUV_10	=	GF_4CC('Y','0','1','0'),
	/*!YUV420p + Alpha plane*/
	GF_PIXEL_YUVA		=	GF_4CC('Y', 'U', 'V', 'A'),
	/*!YUV420p + Depth plane*/
	GF_PIXEL_YUVD		=	GF_4CC('Y', 'U', 'V', 'D'),
	/*!420 Y planar UV interleaved*/
	GF_PIXEL_NV21		=	GF_4CC('N','V','2','1'),
	/*!420 Y planar UV interleaved, 10 bits, little endian */
	GF_PIXEL_NV21_10	=	GF_4CC('N','2','1','0'),
	/*!420 Y planar VU interleaved (U and V swapped) */
	GF_PIXEL_NV12		=	GF_4CC('N','V','1','2'),
	/*!420 Y planar VU interleaved (U and V swapped), 10 bits, little endian */
	GF_PIXEL_NV12_10	=	GF_4CC('N','1','2','0'),
	/*!422 YUV*/
	GF_PIXEL_YUV422		=	GF_4CC('Y','4','4','2'),
	/*!422 YUV, 10 bits, little endian*/
	GF_PIXEL_YUV422_10	=	GF_4CC('Y','2','1','0'),
	/*!444 YUV+Alpha*/
	GF_PIXEL_YUVA444	=	GF_4CC('Y','A','4','4'),
	/*!444 YUV*/
	GF_PIXEL_YUV444		=	GF_4CC('Y','4','4','4'),
	/*!444 YUV, 10 bits, little endian*/
	GF_PIXEL_YUV444_10	=	GF_4CC('Y','4','1','0'),
	/*!444 YUV packed*/
	GF_PIXEL_YUV444_PACK	=	GF_4CC('Y','U','V','4'),
	/*!444 VYU packed (v308) */
	GF_PIXEL_VYU444_PACK	=	GF_4CC('V','Y','U','4'),
	/*!444 YUV+Alpha packed*/
	GF_PIXEL_YUVA444_PACK	=	GF_4CC('Y','A','4','p'),
	/*!444 UYVA packed (v408) */
	GF_PIXEL_UYVA444_PACK	=	GF_4CC('U','Y','V','A'),
	/*!444 YUV 10 bit packed little endian (v410)*/
	GF_PIXEL_YUV444_10_PACK	=	GF_4CC('Y','4','1','p'),
	/*!422 YUV 10 bit packed in v210 format*/
	GF_PIXEL_V210			= GF_4CC('v','2','1','0'),

	/*!Unknown format exposed a single OpenGL texture to be consumed using samplerExternalOES*/
	GF_PIXEL_GL_EXTERNAL	=	GF_4CC('E','X','G','L')
} GF_PixelFormat;


/*! enumerates GPAC pixel formats
\param pf_name name of the pixel format
\return pixel format code
*/
GF_PixelFormat gf_pixel_fmt_parse(const char *pf_name);

/*! gets name of pixel formats
\param pfmt pixel format code
\return pixel format name
*/
const char *gf_pixel_fmt_name(GF_PixelFormat pfmt);

/*! checks if pixel format is known, does not throw error message
\param pf_4cc pixel format code or 0
\param pf_name pixel format name or short name or NULL
\return GF_TRUE is format is known, GF_FALSE otherwise
*/
Bool gf_pixel_fmt_probe(GF_PixelFormat pf_4cc, const char *pf_name);

/*! gets short name of pixel formats, as used for file extensions
\param pfmt pixel format code
\return pixel format short name, "unknown" if not found
*/
const char *gf_pixel_fmt_sname(GF_PixelFormat pfmt);

/*! enumerates pixel formats
\param idx index of the pixel format, 0-based
\param name name of the pixel format
\param fileext file extension of the pixel format
\param description description of the pixel format
\return pixel format code, 0 if no more pixel formats are available
*/
GF_PixelFormat gf_pixel_fmt_enum(u32 *idx, const char **name, const char **fileext, const char **description);

/*! gets the list of all supported pixel format names
\return list of supported pixel format names
*/
const char *gf_pixel_fmt_all_names();

/*! gets the list of all supported pixel format names
\return list of supported pixel format short names
*/
const char *gf_pixel_fmt_all_shortnames();

/*! returns size and stride characteristics for the pixel format. If the stride or stride_uv value are not 0, they are used to compute the size. Otherwise no padding at end of line is assumed.
\param pixfmt  pixfmt format code
\param width target frame width
\param height target frame height
\param[out] out_size output frame size
\param[in,out] out_stride output frame stride for single plane or plane 0
\param[in,out] out_stride_uv output frame stride for UV planes
\param[out] out_planes output frame plane numbers
\param[out] out_plane_uv_height height of UV planes
\return error code if any
*/
Bool gf_pixel_get_size_info(GF_PixelFormat pixfmt, u32 width, u32 height, u32 *out_size, u32 *out_stride, u32 *out_stride_uv, u32 *out_planes, u32 *out_plane_uv_height);

/*! Gets the number of bytes per pixel on first plane
\param pixfmt  pixel format code
\return number of bytes per pixel
*/
u32 gf_pixel_get_bytes_per_pixel(GF_PixelFormat pixfmt);

/*! Gets the number of bits per component
\param pixfmt  pixel format code
\return number of bits per component
*/
u32 gf_pixel_is_wide_depth(GF_PixelFormat pixfmt);

/*! Gets the number of component per pixel
\param pixfmt  pixel format code
\return number of bytes per pixel
*/
u32 gf_pixel_get_nb_comp(GF_PixelFormat pixfmt);

/*! Checks if  pixel format is transparent
\param pixfmt  pixel format code
\return GF_TRUE if alpha channel is present, GF_FALSE otherwise
*/
Bool gf_pixel_fmt_is_transparent(GF_PixelFormat pixfmt);

/*! Checks if format is YUV
\param pixfmt  pixel format code
\return GF_TRUE is YUV format, GF_FALSE otherwise (greyscale or RGB)
*/
Bool gf_pixel_fmt_is_yuv(GF_PixelFormat pixfmt);

/*! gets pixel format associated with a given uncompressed video QT code
\param qt_code the desired QT/ISOBMFF uncompressed video code
\return the corresponding pixel format, or 0 if unknown code
*/
GF_PixelFormat gf_pixel_fmt_from_qt_type(u32 qt_code);
/*! gets QY code associated with a given pixel format
\param pixfmt the desired pixel format
\return the corresponding QT code, or 0 if no asociation
*/
u32 gf_pixel_fmt_to_qt_type(GF_PixelFormat pixfmt);

/*! gets uncC configuration (ISO 23001-17)  of pixel format. The configuration is made of the boxes uncC and cmpd in full mode, and uncC only in restricted mode
\param pixfmt the desired pixel format
\param profile_mode if 1, sets profile if known. If 2 and profile is known, use reduced version
\param dsi set to the generated configuration, must be freed by user
\param dsi_size set to the generated configuration size
\return GF_TRU if success, GF_FALSE otherwise
*/
Bool gf_pixel_fmt_get_uncc(GF_PixelFormat pixfmt, u32 profile_mode, u8 **dsi, u32 *dsi_size);

/*!
\brief Codec IDs

Codec ID identifies the stream coding type. The enum is divided into values less than 255, which are equivalent to MPEG-4 systems ObjectTypeIndication. Other values are 4CCs, usually matching ISOMEDIA sample entry types.

Unless specified otherwise the decoder configuration is:
- the one specified in MPEG-4 systems if any
- or the one defined in ISOBMFF and derived specs (3GPP, dolby, etc) if any
- or NULL
*/
typedef enum
{
	/*!Never used by PID declarations, but used by filters caps*/
	GF_CODECID_NONE = 0,
	/*! codecid for BIFS v1*/
	GF_CODECID_BIFS = 0x01,
	/*! codecid for OD v1*/
	GF_CODECID_OD_V1 = 0x01,
	/*! codecid for BIFS v2*/
	GF_CODECID_BIFS_V2 = 0x02,
	/*! codecid for OD v2*/
	GF_CODECID_OD_V2 = 0x02,
	/*! codecid for BIFS InputSensor streams*/
	GF_CODECID_INTERACT = 0x03,
	/*! codecid for streams with extended BIFS config*/
	GF_CODECID_BIFS_EXTENDED = 0x04,
	/*! codecid for AFX streams with AFXConfig*/
	GF_CODECID_AFX = 0x05,
	/*! codecid for Font data streams */
	GF_CODECID_FONT = 0x06,
	/*! codecid for synthesized texture streams */
	GF_CODECID_SYNTHESIZED_TEXTURE = 0x07,
	/*! codecid for streaming text streams */
	GF_CODECID_TEXT_MPEG4 = 0x08,
	/*! codecid for LASeR streams*/
	GF_CODECID_LASER = 0x09,
	/*! codecid for SAF streams when stored in MP4 ...*/
	GF_CODECID_SAF = 0x0A,

	/*! codecid for MPEG-4 Video Part 2 streams*/
	GF_CODECID_MPEG4_PART2 = 0x20,
	/*! codecid for MPEG-4 Video Part 10 (H.264 | AVC ) streams*/
	GF_CODECID_AVC = 0x21,
	/*! codecid for AVC Parameter sets streams*/
	GF_CODECID_AVC_PS = 0x22,
	/*! codecid for HEVC video */
	GF_CODECID_HEVC = 0x23,
	/*! codecid for H264-SVC streams*/
	GF_CODECID_SVC = 0x24,
	/*! codecid for HEVC layered streams*/
	GF_CODECID_LHVC = 0x25,
	/*! codecid for H264-SVC streams*/
	GF_CODECID_MVC = 0x29,
	/*! codecid for MPEG-4 AAC streams*/
	GF_CODECID_AAC_MPEG4 = 0x40,
	/*! codecid for MPEG-2 Visual Simple Profile streams*/
	GF_CODECID_MPEG2_SIMPLE = 0x60,
	/*! codecid for MPEG-2 Visual Main Profile streams*/
	GF_CODECID_MPEG2_MAIN = 0x61,
	/*! codecid for MPEG-2 Visual SNR Profile streams*/
	GF_CODECID_MPEG2_SNR = 0x62,
	/*! codecid for MPEG-2 Visual SNR Profile streams*/
	GF_CODECID_MPEG2_SPATIAL = 0x63,
	/*! codecid for MPEG-2 Visual SNR Profile streams*/
	GF_CODECID_MPEG2_HIGH = 0x64,
	/*! codecid for MPEG-2 Visual SNR Profile streams*/
	GF_CODECID_MPEG2_422 = 0x65,
	/*! codecid for MPEG-2 AAC Main Profile streams*/
	GF_CODECID_AAC_MPEG2_MP = 0x66,
	/*! codecid for MPEG-2 AAC Low Complexity Profile streams*/
	GF_CODECID_AAC_MPEG2_LCP = 0x67,
	/*! codecid for MPEG-2 AAC Scalable Sampling Rate Profile streams*/
	GF_CODECID_AAC_MPEG2_SSRP = 0x68,
	/*! codecid for MPEG-2 Audio Part 3 streams*/
	GF_CODECID_MPEG2_PART3 = 0x69,
	/*! codecid for MPEG-1 Video streams*/
	GF_CODECID_MPEG1 = 0x6A,
	/*! codecid for MPEG-1 Audio streams, layer 3*/
	GF_CODECID_MPEG_AUDIO = 0x6B,
	/*! codecid for JPEG streams*/
	GF_CODECID_JPEG = 0x6C,
	/*! codecid for PNG streams*/
	GF_CODECID_PNG = 0x6D,

	GF_CODECID_LAST_MPEG4_MAPPING = 0xFF,

	/*! codecid for JPEG-2000 streams*/
	GF_CODECID_J2K = GF_4CC('j','p','2','k'),

	/*!H263 visual streams*/
	GF_CODECID_S263 = GF_4CC('s','2','6','3'),
	GF_CODECID_H263 = GF_4CC('h','2','6','3'),

	/*! codecid for HEVC tiles */
	GF_CODECID_HEVC_TILES = GF_4CC( 'h', 'v', 't', '1' ),
	/*! codecid for explicitly loading HEVC merger , internal to gpac */
	GF_CODECID_HEVC_MERGE = GF_4CC( 'h', 'v', 'c', 'm' ),

	/*! codecid for EVRC Voice streams*/
	GF_CODECID_EVRC	= GF_4CC('s','e','v','c'),
	/*! codecid for SMV Voice streams*/
	GF_CODECID_SMV		= GF_4CC('s','s','m','v'),
	/*! codecid for 13K Voice / QCELP audio streams*/
	GF_CODECID_QCELP = GF_4CC('s','q','c','p'),
	/*! codecid for AMR*/
	GF_CODECID_AMR = GF_4CC('s','a','m','r'),
	/*! codecid for AMR-WB*/
	GF_CODECID_AMR_WB = GF_4CC('s','a','w','b'),
	/*! codecid for EVRC, PacketVideo MUX*/
	GF_CODECID_EVRC_PV	= GF_4CC('p','e','v','c'),

	/*! codecid for SMPTE VC-1 Video streams*/
	GF_CODECID_SMPTE_VC1 = GF_4CC('v','c','-','1'),
	/*! codecid for Dirac Video streams*/
	GF_CODECID_DIRAC = GF_4CC('d','r','a','c'),
	/*! codecid for AC-3 audio streams*/
	GF_CODECID_AC3 = GF_4CC('a','c','-','3'),
	/*! codecid for enhanced AC-3 audio streams*/
	GF_CODECID_EAC3 = GF_4CC('e','c','-','3'),
	/*! codecid for Dolby TrueHS audio streams*/
	GF_CODECID_TRUEHD = GF_4CC('m','l','p','a'),
	/*! codecid for DRA audio streams*/
	GF_CODECID_DRA = GF_4CC('d','r','a','1'),
	/*! codecid for ITU G719 audio streams*/
	GF_CODECID_G719 = GF_4CC('g','7','1','9'),
	/*! codecid for DTS Express low bit rate audio*/
	GF_CODECID_DTS_EXPRESS_LBR = GF_4CC('d','t','s','e'),
	/*! codecid for DTS Coherent Acoustics audio streams*/
	GF_CODECID_DTS_CA = GF_4CC('d','t','s','c'),
	/*! codecid for DTS-HD High Resolution and Master audio streams*/
	GF_CODECID_DTS_HD_HR_MASTER = GF_4CC('d','t','s','h'),
	/*! codecid for DTS-HD Lossless (no core)*/
	GF_CODECID_DTS_HD_LOSSLESS = GF_4CC('d','t','s','l'),
	/*! codecid for DTS-X UHD Profile 2 audio streams*/
	GF_CODECID_DTS_X = GF_4CC('d','t','s','x'),
	/*! codecid for DTS-X UHD Profile 3 audio streams*/
	GF_CODECID_DTS_Y = GF_4CC('d','t','s','y'),

	/*! codecid for DVB EPG*/
	GF_CODECID_DVB_EIT = GF_4CC('e','i','t',' '),

	/*! codecid for streaming SVG*/
	GF_CODECID_SVG = GF_4CC('s','g','g',' '),
	/*! codecid for streaming SVG + gz*/
	GF_CODECID_SVG_GZ = GF_4CC('s','v','g','z'),
	/*! codecid for DIMS (dsi = 3GPP DIMS configuration)*/
	GF_CODECID_DIMS = GF_4CC('d','i','m','s'),
	/*! codecid for streaming VTT*/
	GF_CODECID_WEBVTT = GF_4CC('w','v','t','t'),
	/*! codecid for streaming simple text*/
	GF_CODECID_SIMPLE_TEXT = GF_4CC('s','t','x','t'),
	/*! codecid for meta data streams in text format*/
	GF_CODECID_META_TEXT = GF_4CC('m','e','t','t'),
	/*! codecid for meta data streams in XML format*/
	GF_CODECID_META_XML = GF_4CC('m','e','t','x'),
	/*! codecid for subtitle streams in text format*/
	GF_CODECID_SUBS_TEXT = GF_4CC('s','b','t','t'),
	/*! codecid for subtitle streams in xml format*/
	GF_CODECID_SUBS_XML = GF_4CC('s','t','p','p'),

	/*! codecid for subtitle/text streams in tx3g / apple text format*/
	GF_CODECID_TX3G = GF_4CC( 't', 'x', '3', 'g' ),

	/*! codecid for SSA / ASS text streams (only demux -> tx3f conv)*/
	GF_CODECID_SUBS_SSA = GF_4CC( 'a', 's', 's', 'a' ),

	GF_CODECID_DVB_SUBS = GF_4CC( 'd', 'v', 'b', 's' ),
	GF_CODECID_DVB_TELETEXT = GF_4CC( 'd', 'v', 'b', 't' ),
	/*!
		\brief OGG DecoderConfig

	 The DecoderConfig for theora, vorbis and speek contains all intitialization ogg packets for the codec
	  and is formatted as follows:\n
	 \code
	  while (dsi_size) {
			bit(16) packet_size;
			char packet[packet_size];
			dsi_size -= packet_size;
		}
	 \endcode

	 The DecoderConfig for FLAC is the full flac header without "fLaC" magic

	 The DecoderConfig for OPUS is the full opus header without "OpusHead" magic

	*/
	/*! codecid for theora video streams*/
	GF_CODECID_THEORA = GF_4CC('t','h','e','u'),
	/*! codecid for vorbis audio streams*/
	GF_CODECID_VORBIS = GF_4CC('v','o','r','b'),
	/*! codecid for flac audio streams*/
	GF_CODECID_FLAC = GF_4CC('f','l','a','c'),
	/*! codecid for speex audio streams*/
	GF_CODECID_SPEEX = GF_4CC('s','p','e','x'),
	/*! codecid for opus audio streams*/
	GF_CODECID_OPUS = GF_4CC('O','p','u','s'),
	/*! codecid for subpic DVD subtittles - the associated stream type is text*/
	GF_CODECID_SUBPIC = GF_4CC('s','u','b','p'),
	/*! codecid for ADPCM audio, as used in AVI*/
	GF_CODECID_ADPCM = GF_4CC('A','P','C','M'),
	/*! codecid for IBM CVSD audio, as used in AVI*/
	GF_CODECID_IBM_CVSD = GF_4CC('C','S','V','D'),
	/*! codecid for ALAW audio, as used in AVI*/
	GF_CODECID_ALAW = GF_4CC('A','L','A','W'),
	/*! codecid for MULAW audio, as used in AVI*/
	GF_CODECID_MULAW = GF_4CC('M','L','A','W'),
	/*! codecid for OKI ADPCM audio, as used in AVI*/
	GF_CODECID_OKI_ADPCM = GF_4CC('O','P','C','M'),
	/*! codecid for DVI ADPCM audio, as used in AVI*/
	GF_CODECID_DVI_ADPCM = GF_4CC('D','P','C','M'),
	/*! codecid for DIGISTD audio, as used in AVI*/
	GF_CODECID_DIGISTD = GF_4CC('D','S','T','D'),
	/*! codecid for Yamaha ADPCM audio, as used in AVI*/
	GF_CODECID_YAMAHA_ADPCM = GF_4CC('Y','P','C','M'),
	/*! codecid for TrueSpeech audio, as used in AVI*/
	GF_CODECID_DSP_TRUESPEECH = GF_4CC('T','S','P','E'),
	/*! codecid for GSM 610 audio, as used in AVI*/
	GF_CODECID_GSM610 = GF_4CC('G','6','1','0'),
	/*! codecid for IBM MULAW audio, as used in AVI*/
	GF_CODECID_IBM_MULAW = GF_4CC('I','U','L','W'),
	/*! codecid for IBM ALAW audio, as used in AVI*/
	GF_CODECID_IBM_ALAW = GF_4CC('I','A','L','W'),
	/*! codecid for IBM ADPCM audio, as used in AVI*/
	GF_CODECID_IBM_ADPCM = GF_4CC('I','P','C','M'),
	/*! codecid for Flash/ShockWave streams*/
	GF_CODECID_FLASH = GF_4CC( 'f', 'l', 's', 'h' ),
	/*! codecid for RAW media streams. No decoder config associated (config through PID properties)*/
	GF_CODECID_RAW = GF_4CC('R','A','W','M'),
	/*! codecid for RAW media streams using UNCV  decoder config*/
	GF_CODECID_RAW_UNCV = GF_4CC('U','N','C','V'),

	GF_CODECID_AV1 = GF_4CC('A','V','1',' '),

	GF_CODECID_VP8 = GF_4CC('V','P','0','8'),
	GF_CODECID_VP9 = GF_4CC('V','P','0','9'),
	GF_CODECID_VP10 = GF_4CC('V','P','1','0'),

	/*MPEG-H audio*/
	GF_CODECID_MPHA = GF_4CC('m','p','h','a'),
	/*MPEG-H mux audio*/
	GF_CODECID_MHAS = GF_4CC('m','h','a','s'),

	/*QT ProRes*/
	GF_CODECID_APCH	= GF_4CC( 'a', 'p', 'c', 'h' ),
	GF_CODECID_APCO	= GF_4CC( 'a', 'p', 'c', 'o' ),
	GF_CODECID_APCN	= GF_4CC( 'a', 'p', 'c', 'n' ),
	GF_CODECID_APCS	= GF_4CC( 'a', 'p', 'c', 's' ),
	GF_CODECID_AP4X	= GF_4CC( 'a', 'p', '4', 'x' ),
	GF_CODECID_AP4H	= GF_4CC( 'a', 'p', '4', 'h' ),

	GF_CODECID_TMCD = GF_4CC('t','m','c','d'),

	/*! codecid for FFV1*/
	GF_CODECID_FFV1 = GF_4CC('f','f','v','1'),

	GF_CODECID_FFMPEG = GF_4CC('F','F','I','D'),

	/*! codecid for VVC video */
	GF_CODECID_VVC = GF_4CC('v','v','c',' '),
	GF_CODECID_VVC_SUBPIC = GF_4CC('v','v','c','s'),

	/*! codecid for USAC / xHE-AACv2 audio */
	GF_CODECID_USAC = GF_4CC('u','s','a','c'),

	/*! codecid for MPEG-1 Audio streams, layer 1*/
	GF_CODECID_MPEG_AUDIO_L1 = GF_4CC('m','p','a','1'),

	GF_CODECID_MSPEG4_V3 = GF_4CC('D','I','V','3'),

	GF_CODECID_ALAC = GF_4CC('A','L','A','C'),

	//fake codec IDs for RTP
	GF_CODECID_FAKE_MP2T = GF_4CC('M','P','2','T')
} GF_CodecID;

/*! Gets a textual description for the given codecID
\param codecid target codec ID
\return textual description of the stream
*/
const char *gf_codecid_name(GF_CodecID codecid);

/*! Enumerates supported codec format
\param idx 0-based index, to incremented at each call
\param short_name pointer for codec name
\param long_name pointer for codec description
\return codec ID
*/
GF_CodecID gf_codecid_enum(u32 idx, const char **short_name, const char **long_name);

/*! Gets the associated streamtype for the given codecID
\param codecid target codec ID
\return stream type if known, GF_STREAM_UNKNOWN otherwise
*/
u32 gf_codecid_type(GF_CodecID codecid);

/*! Gets alternate ID of codec if any
\param codecid target codec ID
\return alternate codec ID if known, GF_CODECID_NONE otherwise
*/
GF_CodecID gf_codecid_alt(GF_CodecID codecid);

/*! Gets the associated ObjectTypeIndication if any for the given codecID
\param codecid target codec ID
\return ObjectTypeIndication if defined, 0 otherwise
*/
u8 gf_codecid_oti(GF_CodecID codecid);

/*! Gets the codecID from a given ObjectTypeIndication
\param stream_type stream type of the stream
\param oti ObjectTypeIndication of the stream
\return the codecID for this OTI
*/
GF_CodecID gf_codecid_from_oti(u32 stream_type, u32 oti);

/*! Gets the associated 4CC used by isomedia or RFC6381
\param codecid target codec ID
\return RFC 4CC of codec, 0 if not mapped/known
*/
u32 gf_codecid_4cc_type(GF_CodecID codecid);

/*! Checks if reframer/unframer exists for this codec in gpac
\param codecid target codec ID
\return GF_TRUE if reframer/unframer exist (bitstream reparse is possible), GF_FALSE otherwise
*/
Bool gf_codecid_has_unframer(GF_CodecID codecid);

/*! Gets the codecid given the associated short name
\param cname target codec short name
\return codecid codec ID
*/
GF_CodecID gf_codecid_parse(const char *cname);

/*! Gets the raw file ext (one or more, | separated) for the given codecid
\param codecid codec ID
\return returns file extension
*/
const char *gf_codecid_file_ext(GF_CodecID codecid);

/*! Gets the raw file mime type for the given codecid
\param codecid codec ID
\return returns file mime type
*/
const char *gf_codecid_mime(GF_CodecID codecid);

/*! Gets the codecid from isomedia code point
\param isobmftype isomedia code point
\return codec ID, 0 if not mapped/known
*/
GF_CodecID gf_codec_id_from_isobmf(u32 isobmftype);

/*!
\brief AFX Object Code
*/
enum
{
	/*!3D Mesh Compression*/
	GPAC_AFX_3DMC = 0x00,
	/*!Wavelet Subdivision Surface*/
	GPAC_AFX_WAVELET_SUBDIVISION = 0x01,
	/*!MeshGrid*/
	GPAC_AFX_MESHGRID = 0x02,
	/*!Coordinate Interpolator*/
	GPAC_AFX_COORDINATE_INTERPOLATOR = 0x03,
	/*!Orientation Interpolator*/
	GPAC_AFX_ORIENTATION_INTERPOLATOR = 0x04,
	/*!Position Interpolator*/
	GPAC_AFX_POSITION_INTERPOLATOR = 0x05,
	/*!Octree Image*/
	GPAC_AFX_OCTREE_IMAGE = 0x06,
	/*!BBA*/
	GPAC_AFX_BBA = 0x07,
	/*!PointTexture*/
	GPAC_AFX_POINT_TEXTURE = 0x08,
	/*!3DMC Extension*/
	GPAC_AFX_3DMC_EXT = 0x09,
	/*!FootPrint representation*/
	GPAC_AFX_FOOTPRINT = 0x0A,
	/*!Animated Mesh Compression*/
	GPAC_AFX_ANIMATED_MESH = 0x0B,
	/*!Scalable Complexity*/
	GPAC_AFX_SCALABLE_COMPLEXITY = 0x0C,
};
/*! Gets a textual description of an AFX stream type
\param afx_code target stream type descriptor
\return textural description of the AFX stream
*/
const char *gf_stream_type_afx_name(u8 afx_code);


/*channel cfg flags - DECODERS MUST OUTPUT STEREO/MULTICHANNEL IN THIS ORDER*/

/*!
\brief Audio Channel Configuration

Audio channel flags for spatialization.
 \note Decoders must output stereo/multichannel audio channels in this order in the decoded audio frame.
*/
enum
{
	/*!Left Audio Channel*/
	GF_AUDIO_CH_FRONT_LEFT = (1),
	/*!Right Audio Channel*/
	GF_AUDIO_CH_FRONT_RIGHT = (1<<1),
	/*!Center Audio Channel - may also be used to signal monophonic audio*/
	GF_AUDIO_CH_FRONT_CENTER = (1<<2),
	/*!LFE Audio Channel*/
	GF_AUDIO_CH_LFE = (1<<3),
	/*!Back Left Audio Channel*/
	GF_AUDIO_CH_SURROUND_LEFT = (1 << 4),
	/*!Back Right Audio Channel*/
	GF_AUDIO_CH_SURROUND_RIGHT = (1 << 5),
	/*Between left and center in front Audio Channel*/
	GF_AUDIO_CH_FRONT_CENTER_LEFT = (1 << 6),
	/*Between right and center in front Audio Channel*/
	GF_AUDIO_CH_FRONT_CENTER_RIGHT = (1 << 7),
	/*!Side Left Audio Channel*/
	GF_AUDIO_CH_REAR_SURROUND_LEFT = (1<<8),
	/*!Side Right Audio Channel*/
	GF_AUDIO_CH_REAR_SURROUND_RIGHT = (1<<9),
	/*!Back Center Audio Channel*/
	GF_AUDIO_CH_REAR_CENTER = (1 << 10),
	/*!Left surround direct Channel*/
	GF_AUDIO_CH_SURROUND_DIRECT_LEFT = (1 << 11),
	/*!Right surround direct Channel*/
	GF_AUDIO_CH_SURROUND_DIRECT_RIGHT = (1 << 12),
	/*!Left side surround Channel*/
	GF_AUDIO_CH_SIDE_SURROUND_LEFT = (1 << 13),
	/*!Right side surround Channel*/
	GF_AUDIO_CH_SIDE_SURROUND_RIGHT = (1 << 14),
	/*!Left wide front Channel*/
	GF_AUDIO_CH_WIDE_FRONT_LEFT = (1 << 15),
	/*!Right wide front Channel*/
	GF_AUDIO_CH_WIDE_FRONT_RIGHT = (1 << 16),
	/*!Left front top Channel*/
	GF_AUDIO_CH_FRONT_TOP_LEFT = (1 << 17),
	/*!Right front top Channel*/
	GF_AUDIO_CH_FRONT_TOP_RIGHT = (1 << 18),
	/*!Center front top Channel*/
	GF_AUDIO_CH_FRONT_TOP_CENTER = (1 << 19),
	/*!Left surround top Channel*/
	GF_AUDIO_CH_SURROUND_TOP_LEFT = (1 << 20),
	/*!Right surround top Channel*/
	GF_AUDIO_CH_SURROUND_TOP_RIGHT = (1 << 21),
	/*!Center surround top Channel*/
	GF_AUDIO_CH_REAR_CENTER_TOP = (1 << 22),
	/*!Left side surround top Channel*/
	GF_AUDIO_CH_SIDE_SURROUND_TOP_LEFT = (1 << 23),
	/*!Left side surround top Channel*/
	GF_AUDIO_CH_SIDE_SURROUND_TOP_RIGHT = (1 << 24),
	/*!Center surround top Channel*/
	GF_AUDIO_CH_CENTER_SURROUND_TOP = (1 << 25),
	/*!LFE 2  Channel*/
	GF_AUDIO_CH_LFE2 = (1 << 26),
	/*!Left front bottom Channel*/
	GF_AUDIO_CH_FRONT_BOTTOM_LEFT = (1 << 27),
	/*!Right front bottom Channel*/
	GF_AUDIO_CH_FRONT_BOTTOM_RIGHT = (1 << 28),
	/*!Center front bottom Channel*/
	GF_AUDIO_CH_FRONT_BOTTOM_CENTER = (1 << 29),
	/*!Left surround bottom Channel*/
	GF_AUDIO_CH_SURROUND_BOTTOM_LEFT = (1 << 30),
	/*!Right surround bottom Channel*/
	GF_AUDIO_CH_SURROUND_BOTTOM_RIGHT = 0x80000000 //(1 << 31)
};
/*64 bit flags are defined as macro to avoid msvc compil warnings*/
/*!Left edge of screen Channel*/
#define GF_AUDIO_CH_SCREEN_EDGE_LEFT	0x2000000000ULL
/*!Right edge of screen Channel*/
#define GF_AUDIO_CH_SCREEN_EDGE_RIGHT	0x4000000000ULL
/*!left back surround Channel*/
#define GF_AUDIO_CH_BACK_SURROUND_LEFT	0x20000000000ULL
/*!right back surround Channel*/
#define GF_AUDIO_CH_BACK_SURROUND_RIGHT	0x40000000000ULL 


/*!
\brief Audio Sample format

 Audio sample bit format.
*/
typedef enum
{
	/*! sample = unsigned byte, interleaved channels*/
	GF_AUDIO_FMT_U8 = 1,
	/*! sample = signed short Little Endian, interleaved channels*/
	GF_AUDIO_FMT_S16,
	/*! sample = signed short Big Endian, interleaved channels*/
	GF_AUDIO_FMT_S16_BE,
	/*! sample = signed integer, interleaved channels*/
	GF_AUDIO_FMT_S32,
	/*! sample = signed integer big-endian, interleaved channels*/
	GF_AUDIO_FMT_S32_BE,
	/*! sample = 1 float, interleaved channels*/
	GF_AUDIO_FMT_FLT,
	/*! sample = 1 float bytes in big endian, interleaved channels*/
	GF_AUDIO_FMT_FLT_BE,
	/*! sample = 1 double, interleaved channels*/
	GF_AUDIO_FMT_DBL,
	/*! sample = 1 double bytes in big-endian order, interleaved channels*/
	GF_AUDIO_FMT_DBL_BE,
	/*! sample = signed integer, interleaved channels*/
	GF_AUDIO_FMT_S24,
	/*! sample = signed integer gig-endian, interleaved channels*/
	GF_AUDIO_FMT_S24_BE,
	/*! not a format, indicates the value of last packed format*/
	GF_AUDIO_FMT_LAST_PACKED,
	/*! sample = unsigned byte, planar channels*/
	GF_AUDIO_FMT_U8P,
	/*! sample = signed short, planar channels*/
	GF_AUDIO_FMT_S16P,
	/*! sample = signed integer, planar channels*/
	GF_AUDIO_FMT_S32P,
	/*! sample = 1 float, planar channels*/
	GF_AUDIO_FMT_FLTP,
	/*! sample = 1 double, planar channels*/
	GF_AUDIO_FMT_DBLP,
	/*! sample = signed integer, planar channels*/
	GF_AUDIO_FMT_S24P,
} GF_AudioFormat;


/*! enumerates GPAC audio formats
\param af_name name of the audio format
\return audio format code
*/
GF_AudioFormat gf_audio_fmt_parse(const char *af_name);

/*! gets name of audio formats
\param afmt audio format code
\return audio format name
*/
const char *gf_audio_fmt_name(GF_AudioFormat afmt);

/*! gets short name of audio formats, as used for file extensions
\param afmt audio format code
\return audio format short name
*/
const char *gf_audio_fmt_sname(GF_AudioFormat afmt);


/*! gets the list of all supported audio format names
\return list of supported audio format names
*/
const char *gf_audio_fmt_all_names();

/*! gets the list of all supported audio format names
\return list of supported audio format short names
*/
const char *gf_audio_fmt_all_shortnames();

/*! returns number of bits per sample for the given format
\param afmt desired audio format
\return bit depth of format
*/
u32 gf_audio_fmt_bit_depth(GF_AudioFormat afmt);

/*! Check if a given audio format is planar
\param afmt desired audio format
\return GF_TRUE if the format is planar, false otherwise
*/
Bool gf_audio_fmt_is_planar(GF_AudioFormat afmt);

/*! Returns audio format for raw audio ISOBMFF sample description type
\param msubtype ISOBMFF sample description type
\return the associated audio format or 0 if not known
 */
GF_AudioFormat gf_audio_fmt_from_isobmf(u32 msubtype);

/*! Returns QTFF/ISOBMFF sample description 4CC of an audio format
\param afmt audio format to query
\return the associated 4CC or 0 if not known
 */
u32 gf_audio_fmt_to_isobmf(GF_AudioFormat afmt);

/*! enumerates audio formats
\param idx index of the audio format, 0-based
\param name name of the audio format
\param fileext file extension of the pixel format
\param desc audio format description
\return audio format or 0 if no more audio formats are available
*/
GF_AudioFormat gf_audio_fmt_enum(u32 *idx, const char **name, const char **fileext, const char **desc);

/*! get CICP layout code point from audio configuration
\param nb_chan total number of channels
\param nb_surr number of surround channels
\param nb_lfe number of LFE channels
\return CICP layout code point format or 0 if unknown
*/
u32 gf_audio_fmt_get_cicp_layout(u32 nb_chan, u32 nb_surr, u32 nb_lfe);

/*! get channel layout mask  from CICP layout
\param cicp_layout channel layout CICP code point
\return layout mask or 0 if unknown
*/
u64 gf_audio_fmt_get_layout_from_cicp(u32 cicp_layout);

/*! get CICP layout name
\param cicp_layout channel layout CICP code point
\return name of layout of "unknown" if unknown
*/
const char *gf_audio_fmt_get_layout_name_from_cicp(u32 cicp_layout);

/*! get channel layout name
\param chan_layout channel layout mask
\return name of layout of "unknown" if unknown
*/
const char *gf_audio_fmt_get_layout_name(u64 chan_layout);


/*! get channel layout from name
\param name channel layout name
\return channel layout mask
*/
u64 gf_audio_fmt_get_layout_from_name(const char *name);

/*! get CICP layout value from channel layout mask 
\param chan_layout channel layout mask
\return CICP code point or 255 if unknown
*/
u32 gf_audio_fmt_get_cicp_from_layout(u64 chan_layout);

/*! get channel count from channel  layout
\param chan_layout channel layout mask
\return number of channels in this layout
*/
u32 gf_audio_fmt_get_num_channels_from_layout(u64 chan_layout);

/*! get dloby chanmap value from cicp layout
\param cicp_layout channel CICP layout
\return dolby chanmap
*/
u16 gf_audio_fmt_get_dolby_chanmap(u32 cicp_layout);

/*! enumerates CICP channel layout
\param idx index of cicp layout value to query
\param short_name set t o CICP name as used in GPAC - may be NULL
\param ch_mask set t o audio channel mask, as used in GPAC - may be NULL
\return CICP code point, or 0 if no more to enumerate*/
u32 gf_audio_fmt_cicp_enum(u32 idx, const char **short_name, u64 *ch_mask);

/*! Color primaries as defined by ISO/IEC 23001-8 / 23091-2
  */
typedef enum
{
	GF_COLOR_PRIM_RESERVED0   = 0,
	GF_COLOR_PRIM_BT709       = 1,
	GF_COLOR_PRIM_UNSPECIFIED = 2,
	GF_COLOR_PRIM_RESERVED    = 3,
	GF_COLOR_PRIM_BT470M      = 4,
	GF_COLOR_PRIM_BT470BG     = 5,
	GF_COLOR_PRIM_SMPTE170M   = 6,
	GF_COLOR_PRIM_SMPTE240M   = 7,
	GF_COLOR_PRIM_FILM        = 8,
	GF_COLOR_PRIM_BT2020      = 9,
	GF_COLOR_PRIM_SMPTE428    = 10,
	GF_COLOR_PRIM_SMPTE431    = 11,
	GF_COLOR_PRIM_SMPTE432    = 12,
	GF_COLOR_PRIM_EBU3213     = 22
} GF_ColorPrimaries;

/*! Color Transfer Characteristics as defined by ISO/IEC 23001-8 / 23091-2
*/
typedef enum
{
	GF_COLOR_TRC_RESERVED0    = 0,
	GF_COLOR_TRC_BT709        = 1,
	GF_COLOR_TRC_UNSPECIFIED  = 2,
	GF_COLOR_TRC_RESERVED     = 3,
	GF_COLOR_TRC_GAMMA22      = 4,
	GF_COLOR_TRC_GAMMA28      = 5,
	GF_COLOR_TRC_SMPTE170M    = 6,
	GF_COLOR_TRC_SMPTE240M    = 7,
	GF_COLOR_TRC_LINEAR       = 8,
	GF_COLOR_TRC_LOG          = 9,
	GF_COLOR_TRC_LOG_SQRT     = 10,
	GF_COLOR_TRC_IEC61966_2_4 = 11,
	GF_COLOR_TRC_BT1361_ECG   = 12,
	GF_COLOR_TRC_IEC61966_2_1 = 13,
	GF_COLOR_TRC_BT2020_10    = 14,
	GF_COLOR_TRC_BT2020_12    = 15,
	GF_COLOR_TRC_SMPTE2084    = 16,
	GF_COLOR_TRC_SMPTE428     = 17,
	GF_COLOR_TRC_ARIB_STD_B67 = 18
} GF_ColorTransferCharacteristic;

/*! MatrixCoefficients as defined by ISO/IEC 23001-8 / 23091-2
*/
typedef enum
{
    GF_COLOR_MX_RGB         = 0,
    GF_COLOR_MX_BT709       = 1,
    GF_COLOR_MX_UNSPECIFIED = 2,
    GF_COLOR_MX_RESERVED    = 3,
    GF_COLOR_MX_FCC47         = 4,
    GF_COLOR_MX_BT470BG     = 5,
    GF_COLOR_MX_SMPTE170M   = 6,
    GF_COLOR_MX_SMPTE240M   = 7,
    GF_COLOR_MX_YCGCO       = 8,
    GF_COLOR_MX_BT2020_NCL  = 9,
    GF_COLOR_MX_BT2020_CL   = 10,
    GF_COLOR_MX_SMPTE2085   = 11,
} GF_ColorMatrixCoefficients;

/*! Chroma location values, semantics from CoreVideo - direct match of values  to FFmpeg*/
typedef enum {
	/*! Chroma location is not known*/
    GF_CHROMALOC_UNKNOWN=0,
	/*! Chroma sample is horizontally co-sited with the left column of luma samples, but centered vertically (MPEG-2/4 4:2:0, H.264 default for 4:2:0)*/
    GF_CHROMALOC_LEFT,
    /*! The chroma sample is fully centered ( MPEG-1 4:2:0, JPEG 4:2:0, H.263 4:2:0)*/
    GF_CHROMALOC_CENTER,
    /*! The chroma sample is co-sited with the top-left luma sample (ITU-R 601, SMPTE 274M 296M S314M(DV 4:1:1), mpeg2 4:2:2)*/
    GF_CHROMALOC_TOPLEFT,
    /*! The chroma sample is horizontally centered, but is co-sited with the top row of luma samples*/
    GF_CHROMALOC_TOP,
    /*! The chroma sample is co-sited with the bottom-left luma sample*/
    GF_CHROMALOC_BOTTOMLEFT,
    /*! The chroma sample is horizontally centered, but is co-sited with the bottom row of luma samples*/
    GF_CHROMALOC_BOTTOM,
    /*! The Cr and Cb samples are alternatingly co-sited with the left luma samples of the same field */
    GF_CHROMALOC_DV420,
} GF_ChromaLocation;


/*DIMS unit flags */
/*!
\brief DIMS Unit header flags

DIMS Unit header flags as 3GPP TS 26.142.
 */
enum
{
	/*!S: is-Scene: DIMS unit contains a complete document (svg)*/
	GF_DIMS_UNIT_S = 1,
	/*!M: is-RAP: DIMS unit is a random access point*/
	GF_DIMS_UNIT_M = 1<<1,
	/*!I: is-Redundant: DIMS unit is made of redundant data*/
	GF_DIMS_UNIT_I = 1<<2,
	/*!D: redundant-exit: DIMS unit is the end of redundant data*/
	GF_DIMS_UNIT_D = 1<<3,
	/*!P: priority: DIMS unit is high priority*/
	GF_DIMS_UNIT_P = 1<<4,
	/*!C: compressed: DIMS unit is compressed*/
	GF_DIMS_UNIT_C = 1<<5
};


/*! AVC NAL unit types */
enum
{
	/*! Non IDR AVC slice*/
	GF_AVC_NALU_NON_IDR_SLICE = 1,
	/*! DP_A AVC slice*/
	GF_AVC_NALU_DP_A_SLICE = 2,
	/*! DP_B AVC slice*/
	GF_AVC_NALU_DP_B_SLICE = 3,
	/*! DP_C AVC slice*/
	GF_AVC_NALU_DP_C_SLICE = 4,
	/*! IDR AVC slice*/
	GF_AVC_NALU_IDR_SLICE = 5,
	/*! SEI Message*/
	GF_AVC_NALU_SEI = 6,
	/*! Sequence Parameter Set */
	GF_AVC_NALU_SEQ_PARAM = 7,
	/*! Picture Parameter Set*/
	GF_AVC_NALU_PIC_PARAM = 8,
	/*! Access Unit delimiter*/
	GF_AVC_NALU_ACCESS_UNIT = 9,
	/*! End of Sequence*/
	GF_AVC_NALU_END_OF_SEQ = 10,
	/*! End of stream*/
	GF_AVC_NALU_END_OF_STREAM = 11,
	/*! Filler data*/
	GF_AVC_NALU_FILLER_DATA = 12,
	/*! Sequence Parameter Set Extension*/
	GF_AVC_NALU_SEQ_PARAM_EXT = 13,
	/*! SVC preffix*/
	GF_AVC_NALU_SVC_PREFIX_NALU = 14,
	/*! SVC subsequence parameter set*/
	GF_AVC_NALU_SVC_SUBSEQ_PARAM = 15,
	/*! Auxiliary slice*/
	GF_AVC_NALU_SLICE_AUX = 19,
	/*! SVC slice*/
	GF_AVC_NALU_SVC_SLICE = 20,
	/*! View and dependency representation delimiter */
	GF_AVC_NALU_VDRD = 24,
	/*! Dolby Vision RPU */
	GF_AVC_NALU_DV_RPU = 28,
	/*! Dolby Vision EL */
	GF_AVC_NALU_DV_EL = 30,

	/*! NALU-FF extractor */
	GF_AVC_NALU_FF_AGGREGATOR=30,
	/*! NALU-FF aggregator */
	GF_AVC_NALU_FF_EXTRACTOR=31,
};


/*! AVC slice types */
enum
{
	/*! P slice*/
	GF_AVC_TYPE_P = 0,
	/*! B slice*/
	GF_AVC_TYPE_B = 1,
	/*! I slice*/
	GF_AVC_TYPE_I = 2,
	/*! SP slice*/
	GF_AVC_TYPE_SP = 3,
	/*! SI slice*/
	GF_AVC_TYPE_SI = 4,
	/*! Type2 P slice*/
	GF_AVC_TYPE2_P = 5,
	/*! Type2 B slice*/
	GF_AVC_TYPE2_B = 6,
	/*! Type2 I slice*/
	GF_AVC_TYPE2_I = 7,
	/*! Type2 SP slice*/
	GF_AVC_TYPE2_SP = 8,
	/*! Type2 SI slice*/
	GF_AVC_TYPE2_SI = 9
};

/*! Scheme Type only used internally to signal HLS sample AES in TS */
#define GF_HLS_SAMPLE_AES_SCHEME	GF_4CC('s','a','e','s')


/*! HEVC NAL unit types */
enum
{
	/*! Trail N HEVC slice*/
	GF_HEVC_NALU_SLICE_TRAIL_N = 0,
	/*! Trail R HEVC slice*/
	GF_HEVC_NALU_SLICE_TRAIL_R = 1,
	/*! TSA N HEVC slice*/
	GF_HEVC_NALU_SLICE_TSA_N = 2,
	/*! TSA R HEVC slice*/
	GF_HEVC_NALU_SLICE_TSA_R = 3,
	/*! STSA N HEVC slice*/
	GF_HEVC_NALU_SLICE_STSA_N = 4,
	/*! STSA R HEVC slice*/
	GF_HEVC_NALU_SLICE_STSA_R = 5,
	/*! RADL N HEVC slice*/
	GF_HEVC_NALU_SLICE_RADL_N = 6,
	/*! RADL R HEVC slice*/
	GF_HEVC_NALU_SLICE_RADL_R = 7,
	/*! RASL N HEVC slice*/
	GF_HEVC_NALU_SLICE_RASL_N = 8,
	/*! RASL R HEVC slice*/
	GF_HEVC_NALU_SLICE_RASL_R = 9,
	/*! Reserved non-IRAP SLNR VCL NAL unit types*/
	GF_HEVC_NALU_SLICE_RSV_VCL_N10 = 10,
	GF_HEVC_NALU_SLICE_RSV_VCL_N12 = 12,
	GF_HEVC_NALU_SLICE_RSV_VCL_N14 = 14,
	/*! Reserved non-IRAP sub-layer reference VCL NAL unit types*/
	GF_HEVC_NALU_SLICE_RSV_VCL_R11 = 11,
	GF_HEVC_NALU_SLICE_RSV_VCL_R13 = 13,
	GF_HEVC_NALU_SLICE_RSV_VCL_R15 = 15,
	/*! BLA LP HEVC slice*/
	GF_HEVC_NALU_SLICE_BLA_W_LP = 16,
	/*! BLA DLP HEVC slice*/
	GF_HEVC_NALU_SLICE_BLA_W_DLP = 17,
	/*! BLA no LP HEVC slice*/
	GF_HEVC_NALU_SLICE_BLA_N_LP = 18,
	/*! IDR DLP HEVC slice*/
	GF_HEVC_NALU_SLICE_IDR_W_DLP = 19,
	/*! IDR HEVC slice*/
	GF_HEVC_NALU_SLICE_IDR_N_LP = 20,
	/*! CRA HEVC slice*/
	GF_HEVC_NALU_SLICE_CRA = 21,
	/*! Video Parameter Set*/
	GF_HEVC_NALU_VID_PARAM = 32,
	/*! Sequence Parameter Set*/
	GF_HEVC_NALU_SEQ_PARAM = 33,
	/*! Picture Parameter Set*/
	GF_HEVC_NALU_PIC_PARAM = 34,
	/*! AU delimiter*/
	GF_HEVC_NALU_ACCESS_UNIT = 35,
	/*! End of sequence*/
	GF_HEVC_NALU_END_OF_SEQ = 36,
	/*! End of stream*/
	GF_HEVC_NALU_END_OF_STREAM = 37,
	/*! Filler Data*/
	GF_HEVC_NALU_FILLER_DATA = 38,
	/*! prefix SEI message*/
	GF_HEVC_NALU_SEI_PREFIX = 39,
	/*! suffix SEI message*/
	GF_HEVC_NALU_SEI_SUFFIX = 40,

	/*! NALU-FF aggregator */
	GF_HEVC_NALU_FF_AGGREGATOR=48,
	/*! NALU-FF extractor */
	GF_HEVC_NALU_FF_EXTRACTOR=49,

	/*! Dolby Vision RPU */
	GF_HEVC_NALU_DV_RPU = 62,
	/*! Dolby Vision EL */
	GF_HEVC_NALU_DV_EL = 63
};



/*! VVC NAL unit types - vtm10) */
enum
{
	/*! Trail N VVC slice*/
	GF_VVC_NALU_SLICE_TRAIL = 0,
	/*! STSA N VVC slice*/
	GF_VVC_NALU_SLICE_STSA = 1,
	/*! STSA N VVC slice*/
	GF_VVC_NALU_SLICE_RADL = 2,
	/*! STSA N VVC slice*/
	GF_VVC_NALU_SLICE_RASL = 3,
	/*! IDR with RADL VVC slice*/
	GF_VVC_NALU_SLICE_IDR_W_RADL = 7,
	/*! IDR DLP VVC slice*/
	GF_VVC_NALU_SLICE_IDR_N_LP = 8,
	/*! CRA VVC slice*/
	GF_VVC_NALU_SLICE_CRA = 9,
	/*! CRA VVC slice*/
	GF_VVC_NALU_SLICE_GDR = 10,

	/*! Operation Point Info */
	GF_VVC_NALU_OPI = 12,
	/*! Decode Parameter Set*/
	GF_VVC_NALU_DEC_PARAM = 13,
	/*! Video Parameter Set*/
	GF_VVC_NALU_VID_PARAM = 14,
	/*! Sequence Parameter Set*/
	GF_VVC_NALU_SEQ_PARAM = 15,
	/*! Picture Parameter Set*/
	GF_VVC_NALU_PIC_PARAM = 16,
	/*! APS prefix */
	GF_VVC_NALU_APS_PREFIX = 17,
	/*! APS suffix */
	GF_VVC_NALU_APS_SUFFIX = 18,
	/*! Picture Header*/
	GF_VVC_NALU_PIC_HEADER = 19,
	/*! AU delimiter*/
	GF_VVC_NALU_ACCESS_UNIT = 20,
	/*! End of sequence*/
	GF_VVC_NALU_END_OF_SEQ = 21,
	/*! End of stream*/
	GF_VVC_NALU_END_OF_STREAM = 22,
	/*! prefix SEI message*/
	GF_VVC_NALU_SEI_PREFIX = 23,
	/*! suffix SEI message*/
	GF_VVC_NALU_SEI_SUFFIX = 24,
	/*! Filler Data*/
	GF_VVC_NALU_FILLER_DATA = 25,
};
/*! Number of defined QCELP rate sizes*/
static const unsigned int GF_QCELP_RATE_TO_SIZE_NB = 7;
/*! QCELP rate sizes - note that these sizes INCLUDE the rate_type header byte*/
static const unsigned int GF_QCELP_RATE_TO_SIZE [] = {0, 1, 1, 4, 2, 8, 3, 17, 4, 35, 5, 8, 14, 1};

/*! Number of defined EVRC rate sizes*/
static const unsigned int GF_SMV_EVRC_RATE_TO_SIZE_NB = 6;
/*! EVRC rate sizes - note that these sizes INCLUDE the rate_type header byte*/
static const unsigned int GF_SMV_EVRC_RATE_TO_SIZE [] = {0, 1, 1, 3, 2, 6, 3, 11, 4, 23, 5, 1};

/*! AMR frame sizes*/
static const unsigned int GF_AMR_FRAME_SIZE[16] = { 12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0 };
/*! AMR WB frame sizes*/
static const unsigned int GF_AMR_WB_FRAME_SIZE[16] = { 17, 23, 32, 36, 40, 46, 50, 58, 60, 5, 5, 0, 0, 0, 0, 0 };


/*! out-of-band sample description index for 3GPP (128 and 255 reserved in RFC)*/
#define GF_RTP_TX3G_SIDX_OFFSET	129


/*! RFC6381 codec name max length*/
#define RFC6381_CODEC_NAME_SIZE_MAX 100


/*! ID3v2 tags*/
typedef enum {
	GF_ID3V2_FRAME_AENC = GF_4CC('A','E','N','C'),
	GF_ID3V2_FRAME_APIC = GF_4CC('A','P','I','C'),
	GF_ID3V2_FRAME_COMM = GF_4CC('C','O','M','M'),
	GF_ID3V2_FRAME_COMR = GF_4CC('C','O','M','R'),
	GF_ID3V2_FRAME_ENCR = GF_4CC('E','N','C','R'),
	GF_ID3V2_FRAME_EQUA = GF_4CC('E','Q','U','A'),
	GF_ID3V2_FRAME_ETCO = GF_4CC('E','T','C','O'),
	GF_ID3V2_FRAME_GEOB = GF_4CC('G','E','O','B'),
	GF_ID3V2_FRAME_GRID = GF_4CC('G','R','I','D'),
	GF_ID3V2_FRAME_IPLS = GF_4CC('I','P','L','S'),
	GF_ID3V2_FRAME_LINK = GF_4CC('L','I','N','K'),
	GF_ID3V2_FRAME_MCDI = GF_4CC('M','C','D','I'),
	GF_ID3V2_FRAME_MLLT = GF_4CC('M','L','L','T'),
	GF_ID3V2_FRAME_OWNE = GF_4CC('O','W','N','E'),
	GF_ID3V2_FRAME_PRIV = GF_4CC('P','R','I','V'),
	GF_ID3V2_FRAME_PCNT = GF_4CC('P','C','N','T'),
	GF_ID3V2_FRAME_POPM = GF_4CC('P','O','P','M'),
	GF_ID3V2_FRAME_POSS = GF_4CC('P','O','S','S'),
	GF_ID3V2_FRAME_RBUF = GF_4CC('R','B','U','F'),
	GF_ID3V2_FRAME_RVAD = GF_4CC('R','V','A','D'),
	GF_ID3V2_FRAME_RVRB = GF_4CC('R','V','R','B'),
	GF_ID3V2_FRAME_SYLT = GF_4CC('S','Y','L','T'),
	GF_ID3V2_FRAME_SYTC = GF_4CC('S','Y','T','C'),
	GF_ID3V2_FRAME_TALB = GF_4CC('T','A','L','B'),
	GF_ID3V2_FRAME_TBPM = GF_4CC('T','B','P','M'),
	GF_ID3V2_FRAME_TCAT = GF_4CC('T','C','A','T'),
	GF_ID3V2_FRAME_TCMP = GF_4CC('T','C','M','P'),
	GF_ID3V2_FRAME_TCOM = GF_4CC('T','C','O','M'),
	GF_ID3V2_FRAME_TCON = GF_4CC('T','C','O','N'),
	GF_ID3V2_FRAME_TCOP = GF_4CC('T','C','O','P'),
	GF_ID3V2_FRAME_TDAT = GF_4CC('T','D','A','T'),
	GF_ID3V2_FRAME_TDES = GF_4CC('T','D','E','S'),
	GF_ID3V2_FRAME_TDLY = GF_4CC('T','D','L','Y'),
	GF_ID3V2_FRAME_TDRC = GF_4CC('T','D','R','C'),
	GF_ID3V2_FRAME_TENC = GF_4CC('T','E','N','C'),
	GF_ID3V2_FRAME_TEXT = GF_4CC('T','E','X','T'),
	GF_ID3V2_FRAME_TFLT = GF_4CC('T','F','L','T'),
	GF_ID3V2_FRAME_TIME = GF_4CC('T','I','M','E'),
	GF_ID3V2_FRAME_TIT1 = GF_4CC('T','I','T','1'),
	GF_ID3V2_FRAME_TIT2 = GF_4CC('T','I','T','2'),
	GF_ID3V2_FRAME_TIT3 = GF_4CC('T','I','T','3'),
	GF_ID3V2_FRAME_TKEY = GF_4CC('T','K','E','Y'),
	GF_ID3V2_FRAME_TKWD = GF_4CC('T','K','W','D'),
	GF_ID3V2_FRAME_TLAN = GF_4CC('T','L','A','N'),
	GF_ID3V2_FRAME_TLEN = GF_4CC('T','L','E','N'),
	GF_ID3V2_FRAME_TMED = GF_4CC('T','M','E','D'),
	GF_ID3V2_FRAME_TOAL = GF_4CC('T','O','A','L'),
	GF_ID3V2_FRAME_TOFN = GF_4CC('T','O','F','N'),
	GF_ID3V2_FRAME_TOLY = GF_4CC('T','O','L','Y'),
	GF_ID3V2_FRAME_TOPE = GF_4CC('T','O','P','E'),
	GF_ID3V2_FRAME_TORY = GF_4CC('T','O','R','Y'),
	GF_ID3V2_FRAME_TOWN = GF_4CC('T','O','W','N'),
	GF_ID3V2_FRAME_TPE1 = GF_4CC('T','P','E','1'),
	GF_ID3V2_FRAME_TPE2 = GF_4CC('T','P','E','2'),
	GF_ID3V2_FRAME_TPE3 = GF_4CC('T','P','E','3'),
	GF_ID3V2_FRAME_TPE4 = GF_4CC('T','P','E','4'),
	GF_ID3V2_FRAME_TPOS = GF_4CC('T','P','E','5'),
	GF_ID3V2_FRAME_TPUB = GF_4CC('T','P','U','B'),
	GF_ID3V2_FRAME_TRCK = GF_4CC('T','R','C','K'),
	GF_ID3V2_FRAME_TRDA = GF_4CC('T','R','D','A'),
	GF_ID3V2_FRAME_TRSN = GF_4CC('T','R','S','N'),
	GF_ID3V2_FRAME_TRSO = GF_4CC('T','R','S','O'),
	GF_ID3V2_FRAME_TSIZ = GF_4CC('T','S','I','Z'),
	GF_ID3V2_FRAME_TSO2 = GF_4CC('T','S','O','2'),
	GF_ID3V2_FRAME_TSOA = GF_4CC('T','S','O','A'),
	GF_ID3V2_FRAME_TSOC = GF_4CC('T','S','O','C'),
	GF_ID3V2_FRAME_TSOT = GF_4CC('T','S','O','T'),
	GF_ID3V2_FRAME_TSOP = GF_4CC('T','S','O','P'),
	GF_ID3V2_FRAME_TSRC = GF_4CC('T','S','R','C'),
	GF_ID3V2_FRAME_TSSE = GF_4CC('T','S','S','E'),
	GF_ID3V2_FRAME_TYER = GF_4CC('T','Y','E','R'),
	GF_ID3V2_FRAME_TXXX = GF_4CC('T','X','X','X'),
	GF_ID3V2_FRAME_UFID = GF_4CC('U','F','I','D'),
	GF_ID3V2_FRAME_USER = GF_4CC('U','S','E','R'),
	GF_ID3V2_FRAME_USLT = GF_4CC('U','S','L','T'),
	GF_ID3V2_FRAME_WCOM = GF_4CC('W','C','O','M'),
	GF_ID3V2_FRAME_WCOP = GF_4CC('W','C','O','P'),
	GF_ID3V2_FRAME_WOAF = GF_4CC('W','O','A','F'),
	GF_ID3V2_FRAME_WOAR = GF_4CC('W','O','A','R'),
	GF_ID3V2_FRAME_WOAS = GF_4CC('W','O','A','S'),
	GF_ID3V2_FRAME_WORS = GF_4CC('W','O','R','S'),
	GF_ID3V2_FRAME_WPAY = GF_4CC('W','P','A','Y'),
	GF_ID3V2_FRAME_WPUB = GF_4CC('W','P','U','B'),
	GF_ID3V2_FRAME_WXXX = GF_4CC('W','X','X','X'),
} GF_ID3v2FrameType;

/*! tag base types*/
enum
{
	/*! tag is a string*/
	GF_ITAG_STR=0,
	/*! tag is an 8 bit int*/
	GF_ITAG_INT8,
	/*! tag is a 16 bit int*/
	GF_ITAG_INT16,
	/*! tag is a 32 bit int*/
	GF_ITAG_INT32,
	/*! tag is an 64 bits int*/
	GF_ITAG_INT64,
	/*! tag is a boolean (8bit) */
	GF_ITAG_BOOL,
	/*! tag is ID3 genre tag, either 32 bit int or string*/
	GF_ITAG_ID3_GENRE,
	/*! tag is an fraction on 6 bytes (first 2 unused)*/
	GF_ITAG_FRAC6,
	/*! tag is an fraction on 8 bytes (first 2 and last 2 unused)*/
	GF_ITAG_FRAC8,
	/*! tag is a file*/
	GF_ITAG_FILE,
};
/*! finds a tag by its ID3 value
 \param id3tag ID3 tag value
 \return corresponding tag index, -1 if not found
*/
s32 gf_itags_find_by_id3tag(u32 id3tag);

/*! finds a tag by its itunes value
 \param itag itunes tag value
 \return corresponding tag index, -1 if not found
*/
s32 gf_itags_find_by_itag(u32 itag);

/*! finds a tag by its name
 \param tag_name tag name
 \return corresponding tag index, -1 if not found
*/
s32 gf_itags_find_by_name(const char *tag_name);

/*! gets tag associated type
 \param tag_idx tag index
 \return corresponding tag type, -1 if error
*/
s32 gf_itags_get_type(u32 tag_idx);

/*! gets tag associated name
 \param tag_idx tag index
 \return corresponding tag name, NULL if error
*/
const char *gf_itags_get_name(u32 tag_idx);

/*! gets tag associated alternative names
 \param tag_idx tag index
 \return corresponding tag name, NULL if none
*/
const char *gf_itags_get_alt_name(u32 tag_idx);

/*! gets tag associated itunes tag
 \param tag_idx tag index
 \return corresponding itunes tag, 0 if error
*/
u32 gf_itags_get_itag(u32 tag_idx);

/*! gets tag associated id3 tag
 \param tag_idx tag index
 \return corresponding id3 tag, 0 if error
*/
u32 gf_itags_get_id3tag(u32 tag_idx);

/*! enumerates tags
 \param tag_idx tag index, increased at each call
 \param itag set to itunes tag value - may be NULL
 \param id3tag set to ID3 tag value - may be NULL
 \param type set to tag type - may be NULL
 \return tag name, NULL if error
*/
const char *gf_itags_enum_tags(u32 *tag_idx, u32 *itag, u32 *id3tag, u32 *type);

/*! gets genre name by genre tag
\param tag genre tag (from 1 to max number of ID3 genres)
\return genre name, NULL if not found*/
const char *gf_id3_get_genre(u32 tag);

/*! gets genre tag by genre name
\param name genre name
\return genre tag, 0 if not found*/
u32 gf_id3_get_genre_tag(const char *name);


/*! meta types from box_code_meta.c - fileimport.c */
enum {

	GF_META_ITEM_TYPE_MIME 	= GF_4CC('m', 'i', 'm', 'e'),
	GF_META_ITEM_TYPE_URI 	= GF_4CC('u', 'r', 'i', ' '),
	GF_META_ITEM_TYPE_PICT 	= GF_4CC('p', 'i', 'c', 't'),

	GF_META_TYPE_SVG 	= GF_4CC('s','v','g',' '),
	GF_META_TYPE_SVGZ 	= GF_4CC('s','v','g','z'),
	GF_META_TYPE_SMIL 	= GF_4CC('s','m','i','l'),
	GF_META_TYPE_SMLZ 	= GF_4CC('s','m','l','z'),
	GF_META_TYPE_X3D 	= GF_4CC('x','3','d',' '),
	GF_META_TYPE_X3DZ 	= GF_4CC('x','3','d','z'),
	GF_META_TYPE_XMTA 	= GF_4CC('x','m','t','a'),
	GF_META_TYPE_XMTZ 	= GF_4CC('x','m','t','z'),

	GF_META_TYPE_RVCI 	= GF_4CC('r','v','c','i'),

};


/*! meta types from box_code_meta.c - fileimport.c */
enum {

	GF_S4CC_MPEG4 = GF_4CC('m', 'p', '4', 's'),
	GF_S4CC_LASER = GF_4CC('l', 's', 'r', '1'),
};


/*! CICP code points for color primaries */
enum
{
	GF_CICP_PRIM_RESERVED_0 = 0,
	GF_CICP_PRIM_BT709,
	GF_CICP_PRIM_UNSPECIFIED,
	GF_CICP_PRIM_RESERVED_3,
	GF_CICP_PRIM_BT470M,
	GF_CICP_PRIM_BT470G,
	GF_CICP_PRIM_SMPTE170,
	GF_CICP_PRIM_SMPTE240,
	GF_CICP_PRIM_FILM,
	GF_CICP_PRIM_BT2020,
	GF_CICP_PRIM_SMPTE428,
	GF_CICP_PRIM_SMPTE431,
	GF_CICP_PRIM_SMPTE432,

	GF_CICP_PRIM_EBU3213=22,

	GF_CICP_PRIM_LAST
};

/*! CICP code points for color transfer */
enum
{
	GF_CICP_TRANSFER_RESERVED_0 = 0,
	GF_CICP_TRANSFER_BT709,
	GF_CICP_TRANSFER_UNSPECIFIED,
	GF_CICP_TRANSFER_RESERVED_3,
	GF_CICP_TRANSFER_BT470M,
	GF_CICP_TRANSFER_BT470BG,
	GF_CICP_TRANSFER_SMPTE170,
	GF_CICP_TRANSFER_SMPTE240,
	GF_CICP_TRANSFER_LINEAR,
	GF_CICP_TRANSFER_LOG100,
	GF_CICP_TRANSFER_LOG316,
	GF_CICP_TRANSFER_IEC61966,
	GF_CICP_TRANSFER_BT1361,
	GF_CICP_TRANSFER_SRGB,
	GF_CICP_TRANSFER_BT2020_10,
	GF_CICP_TRANSFER_BT2020_12,
	GF_CICP_TRANSFER_SMPTE2084,
	GF_CICP_TRANSFER_SMPTE428,
	GF_CICP_TRANSFER_STDB67, //prores only

	GF_CICP_TRANSFER_LAST
};


/*! CICP code points for matrix coefficients */
enum
{
	GF_CICP_MX_IDENTITY = 0,
	GF_CICP_MX_BT709,
	GF_CICP_MX_UNSPECIFIED,
	GF_CICP_MX_RESERVED_3,
	GF_CICP_MX_FCC47,
	GF_CICP_MX_BT601_625,
	GF_CICP_MX_SMPTE170,
	GF_CICP_MX_SMPTE240,
	GF_CICP_MX_YCgCo,
	GF_CICP_MX_BT2020,
	GF_CICP_MX_BT2020_CL,
	GF_CICP_MX_YDzDx,

	GF_CICP_MX_LAST
	//the rest is reserved
};

/*! parse CICP color primaries
 \param val CICP color primaries name
\return 0xFFFFFFFF if error , value otherwise
*/
u32 gf_cicp_parse_color_primaries(const char *val);

/*! get CICP color primaries name
\param cicp_prim CICP color primaries code
\return name or "unknown"" if error
*/
const char *gf_cicp_color_primaries_name(u32 cicp_prim);

/*! get CICP color primaries names
\return coma-separated list of GPAC names for CICP color primaries
*/
const char *gf_cicp_color_primaries_all_names();

/*! parse CICP color transfer
 \param val CICP color transfer name
\return 0xFFFFFFFF if error , value otherwise
*/
u32 gf_cicp_parse_color_transfer(const char *val);

/*! get CICP color transfer name
\param cicp_trans CICP color transfer code
\return name or "unknown"" if error
*/
const char *gf_cicp_color_transfer_name(u32 cicp_trans);

/*! get CICP color transfer names
\return coma-separated list of GPAC names for CICP color transfer
*/
const char *gf_cicp_color_transfer_all_names();

/*! parse CICP color matrix coefficients
 \param val CICP color matrix coefficients name
\return 0xFFFFFFFF if error , value otherwise
*/
u32 gf_cicp_parse_color_matrix(const char *val);

/*! get CICP color matrix coefficients name
\param cicp_mx CICP color matrix coefficients code
\return name or "unknown"" if error
*/
const char *gf_cicp_color_matrix_name(u32 cicp_mx);

/*! get CICP color matrix names
\return coma-separated list of GPAC names for CICP color matrix
*/
const char *gf_cicp_color_matrix_all_names();


/*! stereo frame packing types */
enum
{
	/*! monoscopic video*/
	GF_STEREO_NONE = 0,
	/*! left eye in top half of video, right eye in bottom half of video*/
	GF_STEREO_TOP_BOTTOM,
	/*! left eye in left half of video, right eye in right half of video*/
	GF_STEREO_LEFT_RIGHT,
	/*! stereo mapped through mesh*/
	GF_STEREO_CUSTOM,
	/*! left eye in right half of video, right eye in left half of video*/
	GF_STEREO_RIGHT_LEFT,
	/*! left eye in bottom half of video, right eye in top half of video*/
	GF_STEREO_BOTTOM_TOP,
};

/*! 360 projection types */
enum
{
	/*! flat video*/
	GF_PROJ360_NONE = 0,
	/*! cube map projection video is upper half: right, left, up, lower half: down, front, back*/
	GF_PROJ360_CUBE_MAP,
	/*! Equirectangular projection / video*/
	GF_PROJ360_EQR,
	/*! Mesh projection (not supported yet)*/
	GF_PROJ360_MESH
};

/*! user data used by GPAC to store SRD info*/
#define GF_ISOM_UDTA_GPAC_SRD	GF_4CC('G','S','R','D')

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_CONSTANTS_H_*/
