/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
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
 *	\file <gpac/constants.h>
 *	\brief Most constants defined in GPAC are in this file.
 *  \addtogroup cst_grp
 *	\brief Constants used within GPAC
 *
 *	This section documents some constants used in the GPAC framework which are not related to
 *	any specific sub-project.
 *	@{
 */


/*!
 *	\brief Supported media stream types
 *	\hideinitializer
 *
 * Supported media stream types for media objects.
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
	*\n\note
	*This stream type (MPEG-4 user-private) is reserved for streams used to create a scene decoder
	*handling the scene without input streams, as is the case for file readers (BT/VRML/XML..).\n
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


/*!
 * \brief Pixel Formats
 *
 *	Supported pixel formats for everything using video
*/
#ifndef GF_4CC
#define GF_4CC(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))
#endif
typedef enum
{
	/*!8 bit GREY */
	GF_PIXEL_GREYSCALE	=	GF_4CC('G','R','E','Y'),
	/*!16 bit greyscale*/
	GF_PIXEL_ALPHAGREY	=	GF_4CC('G','R','A','L'),
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

	/*!32 bit ARGB. Component ordering in bytes is B-G-R-A.*/
	GF_PIXEL_ARGB		=	GF_4CC('A','R','G','B'),
	/*!32 bit RGBA (openGL like). Component ordering in bytes is R-G-B-A.*/
	GF_PIXEL_RGBA		=	GF_4CC('R','G','B', 'A'),
	/*!RGB24 + depth plane. Component ordering in bytes is R-G-B-D.*/
	GF_PIXEL_RGBD		=	GF_4CC('R', 'G', 'B', 'D'),
	/*!RGB24 + depth plane (7 lower bits) + shape mask. Component ordering in bytes is R-G-B-(S+D).*/
	GF_PIXEL_RGBDS		=	GF_4CC('3', 'C', 'D', 'S'),
	/*!Stereo RGB24 */
	GF_PIXEL_RGBS		=	GF_4CC('R', 'G', 'B', 'S'),
	/*!Stereo RGBA. Component ordering in bytes is R-G-B-A. */
	GF_PIXEL_RGBAS		=	GF_4CC('R', 'G', 'A', 'S'),

	/*internal format for OpenGL using pachek RGB 24 bit plus planar depth plane at the end of the image*/
	GF_PIXEL_RGB_DEPTH = GF_4CC('R', 'G', 'B', 'd'),

	/*!YUV packed 422 format*/
	GF_PIXEL_YUYV		=	GF_4CC('Y','U','Y','2'),
	/*!YUV packed 422 format*/
	GF_PIXEL_YVYU		=	GF_4CC('Y','V','Y','U'),
	/*!YUV packed 422 format*/
	GF_PIXEL_UYVY		=	GF_4CC('U','Y','V','Y'),
	/*!YUV packed 422 format*/
	GF_PIXEL_VYUY		=	GF_4CC('V','Y','U','Y'),

	/*!YUV planar format*/
	GF_PIXEL_YUV		=	GF_4CC('Y','V','1','2'),
	/*!YUV420p in 10 bits mode, all components are stored as shorts*/
	GF_PIXEL_YUV_10	=	GF_4CC('Y','0','1','0'),
	/*!YUV420p + Alpha plane*/
	GF_PIXEL_YUVA		=	GF_4CC('Y', 'U', 'V', 'A'),
	/*!YUV420p + Depth plane*/
	GF_PIXEL_YUVD		=	GF_4CC('Y', 'U', 'V', 'D'),
	/*!420 Y planar UV interleaved*/
	GF_PIXEL_NV21		=	GF_4CC('N','V','2','1'),
	/*!420 Y planar UV interleaved, 10 bits */
	GF_PIXEL_NV21_10	=	GF_4CC('N','2','1','0'),
	/*!420 Y planar VU interleaved (U and V swapped) */
	GF_PIXEL_NV12		=	GF_4CC('N','V','1','2'),
	/*!420 Y planar VU interleaved (U and V swapped), 10 bits */
	GF_PIXEL_NV12_10	=	GF_4CC('N','1','2','0'),
	GF_PIXEL_YUV422		=	GF_4CC('Y','4','4','2'),
	GF_PIXEL_YUV422_10	=	GF_4CC('Y','2','1','0'),
	GF_PIXEL_YUV444		=	GF_4CC('Y','4','4','4'),
	GF_PIXEL_YUV444_10	=	GF_4CC('Y','4','1','0')

} GF_PixelFormat;


/*! enumerates GPAC pixel formats
 \param[in,out] idx index of the pixel format to query, incremented at each call
 \param[out] out_name output name of the pixel format
 \return pixel format code
*/
u32 gf_pixel_fmt_enum(u32 *idx, const char **out_name);

/*! enumerates GPAC pixel formats
 \param pf_name name of the pixel format
 \return pixel format code
*/
u32 gf_pixel_fmt_parse(const char *pf_name);

/*! gets name of pixel formats
 \param pfmt  pixel format code
 \return pixel format name
*/
const char *gf_pixel_fmt_name(u32 pfmt);

/*! gets short name of pixel formats, as used for file extensions
 \param pfmt  pixel format code
 \return pixel format short name
*/
const char *gf_pixel_fmt_sname(u32 pfmt);


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

/*!
 * \brief Codec IDs
 *
 * Codec ID identifies the stream coding type. The enum is devided into values less than 255, which are equivalent
 * to MPEG-4 systems ObjectTypeIndication. Other values are 4CCs, usually matching ISOMEDIA sample entry types*/
enum
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
	/*! codecid for SAF streams*/
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
	/*! codecid for MPEG-1 Audio streams*/
	GF_CODECID_MPEG_AUDIO = 0x6B,
	/*! codecid for JPEG streams*/
	GF_CODECID_JPEG = 0x6C,
	/*! codecid for PNG streams*/
	GF_CODECID_PNG = 0x6D,
	/*! codecid for JPEG-2000 streams*/
	GF_CODECID_J2K = 0x6E,

	GF_CODECID_LAST_MPEG4_MAPPING = 0xFF,

	/*!H263 visual streams*/
	GF_CODECID_S263 = GF_4CC('s','2','6','3'),
	GF_CODECID_H263 = GF_4CC('h','2','6','3'),

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
	/*! codecid for DRA audio streams*/
	GF_CODECID_DRA = GF_4CC('d','r','a','1'),
	/*! codecid for ITU G719 audio streams*/
	GF_CODECID_G719 = GF_4CC('g','7','1','9'),
	/*! codecid for DTS  Express low bit rate audio*/
	GF_CODECID_DTS_LBR = GF_4CC('d','t','s','e'),
	/*! codecid for DTS Coherent Acoustics audio streams*/
	GF_CODECID_DTS_CA = GF_4CC('d','t','s','c'),
	/*! codecid for DTS-HD High Resolution audio streams*/
	GF_CODECID_DTS_HD_HR = GF_4CC('d','t','s','h'),
	/*! codecid for DTS-HD Master audio streams*/
	GF_CODECID_DTS_HD_MASTER = GF_4CC('d','t','s','l'),
	/*! codecid for DTS-X Master audio streams*/
	GF_CODECID_DTS_X = GF_4CC('d','t','s','x'),

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

	/*!
	 * \brief OGG DecoderConfig
	 *
	 *	The DecoderConfig for theora, vorbis, flac and opus contains all intitialization ogg packets for the codec
	 * and is formated as follows:\n
	 *\code
	  while (dsi_size) {
			bit(16) packet_size;
			char packet[packet_size];
			dsi_size -= packet_size;
		}
	 *\endcode
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
	/*! codecid for raw audio PCM, as used in AVI*/
	GF_CODECID_PCM = GF_4CC('P','C','M',' '),
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
};

/*! Gets a textual description for the given codecID
 \param codecid target codec ID
 \return textual description of the stream
*/
const char *gf_codecid_name(u32 codecid);

/*! Enumerates supported codec format
 \param idx 0-based index, to incremented at each call
 \param short_name pointer for codec name
 \param long_name pointer for codec description
 \return codec ID
*/
u32 gf_codecid_enum(u32 idx, const char **short_name, const char **long_name);

/*! Gets a textual description for the given MPEG-4 stream type and object type
 \param stream_type stream type of the stream
 \param oti ObjectTypeIndication of the stream
 \return textual description of the stream
*/
const char *gf_codecid_name_oti(u32 stream_type, u32 oti);

/*! Gets the associated streamtype for the given codecID
 \param codecid target codec ID
 \return stream type if known, GF_STREAM_UNKNOWN otherwise
*/
u32 gf_codecid_type(u32 codecid);

/*! Gets the associated ObjectTypeIndication if any for the given codecID
 \param codecid target codec ID
 \return ObjectTypeIndication if defined, 0 otherwise
*/
u8 gf_codecid_oti(u32 codecid);

/*! Gets the associated 4CC used by isomedia or RFC6381
 \param codecid target codec ID
 \return RFC 4CC of codec, 0 if not mapped/known
*/
u32 gf_codecid_4cc_type(u32 codecid);

/*! Gets the codecid given the associated short name
 \param cname target codec short name
 \return codecid codec ID
*/
u32 gf_codec_parse(const char *cname);

/*!
 * \brief AFX Object Code
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
 * \brief Audio Channel Configuration
 *
 *	Audio channel flags for spatialization.
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
	GF_AUDIO_CH_BACK_LEFT = (1 << 4),
	/*!Back Right Audio Channel*/
	GF_AUDIO_CH_BACK_RIGHT = (1 << 5),
	/*Between left and center in front Audio Channel*/
	GF_AUDIO_CH_LEFT_CENTER = (1 << 6),
	/*Between right and center in front Audio Channel*/
	GF_AUDIO_CH_RIGHT_CENTER = (1 << 7),
	/*!Back Center Audio Channel*/
	GF_AUDIO_CH_BACK_CENTER = (1 << 8),
	/*!Side Left Audio Channel*/
	GF_AUDIO_CH_SIDE_LEFT = (1<<9),
	/*!Side Right Audio Channel*/
	GF_AUDIO_CH_SIDE_RIGHT = (1<<10),
	/*!top Audio Channel*/
	GF_AUDIO_CH_TOP_CENTER = (1 << 11),
	/*!between left and center above Audio Channel*/
	GF_AUDIO_CH_TOP_FRONT_LEFT = (1 << 12),
	/*!above center Audio Channel*/
	GF_AUDIO_CH_TOP_FRONT_CENTER = (1 << 13),
	/*!between right and center above Audio Channel*/
	GF_AUDIO_CH_TOP_FRONT_RIGHT = (1 << 14),
	/*!Back Left High Audio Channel*/
	GF_AUDIO_CH_TOP_BACK_LEFT = (1 << 15),
	/*!Back top High Audio Channel*/
	GF_AUDIO_CH_TOP_BACK_CENTER = (1 << 16),
	/*!Back Right High Audio Channel*/
	GF_AUDIO_CH_TOP_BACK_RIGHT = (1 << 17)
};


/*!
 * \brief Audio Sample format
 *
 *	Audio sample bit format.
 */
enum
{
	/*! sample = unsigned byte, interleaved channels*/
	GF_AUDIO_FMT_U8 = 1,
	/*! sample = signed short, interleaved channels*/
	GF_AUDIO_FMT_S16,
	/*! sample = signed integer, interleaved channels*/
	GF_AUDIO_FMT_S32,
	/*! sample = 1 float, interleaved channels*/
	GF_AUDIO_FMT_FLT,
	/*! sample = 1 double, interleaved channels*/
	GF_AUDIO_FMT_DBL,
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
	/*! sample = signed integer, interleaved channels*/
	GF_AUDIO_FMT_S24,
	/*! sample = signed integer, planar channels*/
	GF_AUDIO_FMT_S24P,
};


/*! enumerates GPAC audio formats
 \param af_name name of the audio format
 \return audio format code
*/
u32 gf_audio_fmt_parse(const char *af_name);

/*! gets name of audio formats
 \param afmt audio format code
 \return audio format name
*/
const char *gf_audio_fmt_name(u32 afmt);

/*! gets short name of audio formats, as used for file extensions
 \param afmt audio format code
 \return audio format short name
*/
const char *gf_audio_fmt_sname(u32 afmt);


/*! gets the list of all supported audio format names
 \return list of supported audio format names
*/
const char *gf_audio_fmt_all_names();

/*! gets the list of all supported audio format names
 \return list of supported audio format short names
*/
const char *gf_audio_fmt_all_shortnames();

/*! returns number of bots per sample for the given format
 \param afmt desired audio format
 \return bit depth of format
*/
u32 gf_audio_fmt_bit_depth(u32 afmt);

/*! Check if a given audio format is planar
 \param afmt desired audio format
 \return GF_TRUE if the format is planar, false otherwise
*/
Bool gf_audio_fmt_is_planar(u32 afmt);

/*DIMS unit flags */
/*!
 * \brief DIMS Unit header flags
 *
 *	DIMS Unit header flags as 3GPP TS 26.142.
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


/*!
 * AVC NAL unit types
 */
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
	GF_AVC_NALU_VDRD = 24
};


/*!
 * AVC slice types
 */
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

/*!
 HEVC NAL unit types
 */
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

/*! @} */

#define GF_VENDOR_GPAC		GF_4CC('G','P','A','C')

#define GF_LANG_UNKNOWN		GF_4CC('u','n','d',' ')


/* ID3v2 tags from mpegts.c */
typedef enum {
	ID3V2_FRAME_AENC = GF_4CC('A','E','N','C'),
	ID3V2_FRAME_APIC = GF_4CC('A','P','I','C'),
	ID3V2_FRAME_COMM = GF_4CC('C','O','M','M'),
	ID3V2_FRAME_COMR = GF_4CC('C','O','M','R'),
	ID3V2_FRAME_ENCR = GF_4CC('E','N','C','R'),
	ID3V2_FRAME_EQUA = GF_4CC('E','Q','U','A'),
	ID3V2_FRAME_ETCO = GF_4CC('E','T','C','O'),
	ID3V2_FRAME_GEOB = GF_4CC('G','E','O','B'),
	ID3V2_FRAME_GRID = GF_4CC('G','R','I','D'),
	ID3V2_FRAME_IPLS = GF_4CC('I','P','L','S'),
	ID3V2_FRAME_LINK = GF_4CC('L','I','N','K'),
	ID3V2_FRAME_MCDI = GF_4CC('M','C','D','I'),
	ID3V2_FRAME_MLLT = GF_4CC('M','L','L','T'),
	ID3V2_FRAME_OWNE = GF_4CC('O','W','N','E'),
	ID3V2_FRAME_PRIV = GF_4CC('P','R','I','V'),
	ID3V2_FRAME_PCNT = GF_4CC('P','C','N','T'),
	ID3V2_FRAME_POPM = GF_4CC('P','O','P','M'),
	ID3V2_FRAME_POSS = GF_4CC('P','O','S','S'),
	ID3V2_FRAME_RBUF = GF_4CC('R','B','U','F'),
	ID3V2_FRAME_RVAD = GF_4CC('R','V','A','D'),
	ID3V2_FRAME_RVRB = GF_4CC('R','V','R','B'),
	ID3V2_FRAME_SYLT = GF_4CC('S','Y','L','T'),
	ID3V2_FRAME_SYTC = GF_4CC('S','Y','T','C'),
	ID3V2_FRAME_TALB = GF_4CC('T','E','N','C'),
	ID3V2_FRAME_TBPM = GF_4CC('T','B','P','M'),
	ID3V2_FRAME_TCOM = GF_4CC('T','C','O','M'),
	ID3V2_FRAME_TCON = GF_4CC('T','C','O','N'),
	ID3V2_FRAME_TCOP = GF_4CC('T','C','O','P'),
	ID3V2_FRAME_TDAT = GF_4CC('T','D','A','T'),
	ID3V2_FRAME_TDLY = GF_4CC('T','D','L','Y'),
	ID3V2_FRAME_TENC = GF_4CC('T','E','N','C'),
	ID3V2_FRAME_TEXT = GF_4CC('T','E','X','T'),
	ID3V2_FRAME_TFLT = GF_4CC('T','F','L','T'),
	ID3V2_FRAME_TIME = GF_4CC('T','I','M','E'),
	ID3V2_FRAME_TIT1 = GF_4CC('T','I','T','1'),
	ID3V2_FRAME_TIT2 = GF_4CC('T','I','T','2'),
	ID3V2_FRAME_TIT3 = GF_4CC('T','I','T','3'),
	ID3V2_FRAME_TKEY = GF_4CC('T','K','E','Y'),
	ID3V2_FRAME_TLAN = GF_4CC('T','L','A','N'),
	ID3V2_FRAME_TLEN = GF_4CC('T','L','E','N'),
	ID3V2_FRAME_TMED = GF_4CC('T','M','E','D'),
	ID3V2_FRAME_TOAL = GF_4CC('T','O','A','L'),
	ID3V2_FRAME_TOFN = GF_4CC('T','O','F','N'),
	ID3V2_FRAME_TOLY = GF_4CC('T','O','L','Y'),
	ID3V2_FRAME_TOPE = GF_4CC('T','O','P','E'),
	ID3V2_FRAME_TORY = GF_4CC('T','O','R','Y'),
	ID3V2_FRAME_TOWN = GF_4CC('T','O','W','N'),
	ID3V2_FRAME_TPE1 = GF_4CC('T','P','E','1'),
	ID3V2_FRAME_TPE2 = GF_4CC('T','P','E','2'),
	ID3V2_FRAME_TPE3 = GF_4CC('T','P','E','3'),
	ID3V2_FRAME_TPE4 = GF_4CC('T','P','E','4'),
	ID3V2_FRAME_TPOS = GF_4CC('T','P','E','5'),
	ID3V2_FRAME_TPUB = GF_4CC('T','P','U','B'),
	ID3V2_FRAME_TRCK = GF_4CC('T','R','C','K'),
	ID3V2_FRAME_TRDA = GF_4CC('T','R','D','A'),
	ID3V2_FRAME_TRSN = GF_4CC('T','R','S','N'),
	ID3V2_FRAME_TRSO = GF_4CC('T','R','S','O'),
	ID3V2_FRAME_TSIZ = GF_4CC('T','S','I','Z'),
	ID3V2_FRAME_TSRC = GF_4CC('T','S','R','C'),
	ID3V2_FRAME_TSSE = GF_4CC('T','S','S','E'),
	ID3V2_FRAME_TYER = GF_4CC('T','Y','E','R'),
	ID3V2_FRAME_TXXX = GF_4CC('T','X','X','X'),
	ID3V2_FRAME_UFID = GF_4CC('U','F','I','D'),
	ID3V2_FRAME_USER = GF_4CC('U','S','E','R'),
	ID3V2_FRAME_USLT = GF_4CC('U','S','L','T'),
	ID3V2_FRAME_WCOM = GF_4CC('W','C','O','M'),
	ID3V2_FRAME_WCOP = GF_4CC('W','C','O','P'),
	ID3V2_FRAME_WOAF = GF_4CC('W','O','A','F'),
	ID3V2_FRAME_WOAR = GF_4CC('W','O','A','R'),
	ID3V2_FRAME_WOAS = GF_4CC('W','O','A','S'),
	ID3V2_FRAME_WORS = GF_4CC('W','O','R','S'),
	ID3V2_FRAME_WPAY = GF_4CC('W','P','A','Y'),
	ID3V2_FRAME_WPUB = GF_4CC('W','P','U','B'),
	ID3V2_FRAME_WXXX = GF_4CC('W','X','X','X')
} GF_ID3v2FrameType;

/* meta types from box_code_meta.c - fileimport.c */
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


/* meta types from box_code_meta.c - fileimport.c */
enum {

	GF_S4CC_MPEG4 = GF_4CC('m', 'p', '4', 's'),
	GF_S4CC_LASER = GF_4CC('l', 's', 'r', '1'),
};


/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_CONSTANTS_H_*/
