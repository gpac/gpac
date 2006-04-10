/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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

#include <gpac/tools.h>

/*! \addtogroup cst_grp constants
 *	\brief Constants used within GPAC
 *
 *	This section documents some constants used in the GPAC framework which are not related to
 *	any specific sub-project.
 *	\ingroup utils_grp
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

	/*GPAC internal stream types*/


	/*!GPAC Private Scene streams\n
	*\n\note
	*this streamtype (MPEG-4 user-private) is reserved for streams only used to create a scene decoder handling the service 
	*by itself, as is the case of the BT/VRML reader and can be used by many other things.\n
	*The decoderSpecificInfo carried is as follows:
	 \code 
		u32 file_size:	total file size 
		char file_name[dsi_size - sizeof(u32)]: local file name. 
		\n\note: File may be a cache file, it is the decoder responsability to check if the file is completely
		downloaded before parsing if needed.
	 \endcode 
	*The inBufferLength param for decoders using these streams is the stream clock in ms (no input data is given).\n
	*There is a dummy module available generating this stream and taking care of proper clock init in case of seeking.\n
	*This is a reentrant streamtype: if any media object with this streamtype also exist in the scene, they will be 
	*attached to the scene decoder (except when a new inline scene is detected, in which case a new decoder will 
	*be created). This allows for animation/sprite usage along with the systems timing/stream management.\n
	*\n
	*the objectTypeIndication currently in use for these streams are\n
	*0x00	-	 Forbidden\n
	*0x01	-	 VRML/BT/XMT/SWF loader (similar to MP4Box context loading)\n
	*0x02	-	 SVG loader\n
	*0x03	-	 LASeR XML loader\n
	*/
	GF_STREAM_PRIVATE_SCENE	= 0x20,
};


/*!
 *	Media Object types
 * 
 *	This type provides a hint to network modules which may have to generate an service descriptor on the fly.
 *	They occur only if objects/services used in the scene are not referenced through ObjectDescriptors (MPEG-4) 
 *	but direct through URL
*/
enum
{
	/*!service descriptor expected is of undefined type. This should be treated like GF_MEDIA_OBJECT_SCENE*/
	GF_MEDIA_OBJECT_UNDEF = 0,
	/*!service descriptor expected is of SCENE type and shall contain a scene stream and OD one if needed*/
	GF_MEDIA_OBJECT_SCENE,
	/*!service descriptor expected is of SCENE type (animation streams)*/
	GF_MEDIA_OBJECT_BIFS,
	/*!service descriptor expected is of VISUAL type*/
	GF_MEDIA_OBJECT_VIDEO,
	/*!service descriptor expected is of AUDIO type*/
	GF_MEDIA_OBJECT_AUDIO,
	/*!service descriptor expected is of TEXT type (3GPP/MPEG4)*/
	GF_MEDIA_OBJECT_TEXT,
	/*!service descriptor expected is of UserInteraction type (MPEG-4 InputSensor)*/
	GF_MEDIA_OBJECT_INTERACT,
};


/*!
 * \brief Pixel Formats
 * 
 *	Supported pixel formats for everything using video
 *\note	For textures using 32 bit ARGB/RGB_32/BGR_32:
 *\li on little endian machines, shall be ordered in memory as BGRA, 
 *\li on big endians, shall be ordered in memory as ARGB
 *so that *(u32*)pixel_mem is always ARGB (0xAARRGGBB).
*/
typedef enum
{
	/*!8 bit GREY */
	GF_PIXEL_GREYSCALE	=	GF_4CC('G','R','E','Y'),
	/*!16 bit greyscale*/
	GF_PIXEL_ALPHAGREY	=	GF_4CC('G','R','A','L'),
	/*!15 bit RGB*/
	GF_PIXEL_RGB_555	=	GF_4CC('R','5','5','5'),
	/*!16 bit RGB*/
	GF_PIXEL_RGB_565	=	GF_4CC('R','5','6','5'),
	/*!24 bit RGB*/
	GF_PIXEL_RGB_24		=	GF_4CC('R','G','B','3'),
	/*!24 bit BGR - used for graphics cards video format signaling*/
	GF_PIXEL_BGR_24		=	GF_4CC('B','G','R','3'),
	/*!32 bit RGB*/
	GF_PIXEL_RGB_32		=	GF_4CC('R','G','B','4'),
	/*!32 bit BGR - used for graphics cards video format signaling*/
	GF_PIXEL_BGR_32		=	GF_4CC('B','G','R','4'),

	/*!32 bit ARGB.*/
	GF_PIXEL_ARGB		=	GF_4CC('A','R','G','B'),
	/*!32 bit RGBA (openGL like)*/
	GF_PIXEL_RGBA		=	GF_4CC('R','G','B', 'A'),

	/*!YUV packed format*/
	GF_PIXEL_YUY2		=	GF_4CC('Y','U','Y','2'),
	/*!YUV packed format*/
	GF_PIXEL_YVYU		=	GF_4CC('Y','V','Y','U'),
	/*!YUV packed format*/
	GF_PIXEL_UYVY		=	GF_4CC('U','Y','V','Y'),
	/*!YUV packed format*/
	GF_PIXEL_VYUY		=	GF_4CC('V','Y','U','Y'),
	/*!YUV packed format*/
	GF_PIXEL_Y422		=	GF_4CC('Y','4','2','2'),
	/*!YUV packed format*/
	GF_PIXEL_UYNV		=	GF_4CC('U','Y','N','V'),
	/*!YUV packed format*/
	GF_PIXEL_YUNV		=	GF_4CC('Y','U','N','V'),
	/*!YUV packed format*/
	GF_PIXEL_V422		=	GF_4CC('V','4','2','2'),
	
	/*!YUV planar format*/
	GF_PIXEL_YV12		=	GF_4CC('Y','V','1','2'),
	/*!YUV planar format*/
	GF_PIXEL_IYUV		=	GF_4CC('I','Y','U','V'),
	/*!YUV planar format*/
	GF_PIXEL_I420		=	GF_4CC('I','4','2','0'),

	/*!YV12 + Alpha plane*/
	GF_PIXEL_YUVA		=	GF_4CC('Y', 'U', 'V', 'A')

} GF_PixelFormat;

/*!
 \cond DUMMY_DOXY_SECTION
*/

/*AVC NAL unit types*/
#define GF_AVC_NALU_NON_IDR_SLICE 0x1
#define GF_AVC_NALU_DP_A_SLICE 0x2
#define GF_AVC_NALU_DP_B_SLICE 0x3
#define GF_AVC_NALU_DP_C_SLICE 0x4
#define GF_AVC_NALU_IDR_SLICE 0x5
#define GF_AVC_NALU_SEI 0x6
#define GF_AVC_NALU_SEQ_PARAM 0x7
#define GF_AVC_NALU_PIC_PARAM 0x8
#define GF_AVC_NALU_ACCESS_UNIT 0x9
#define GF_AVC_NALU_END_OF_SEQ 0xa
#define GF_AVC_NALU_END_OF_STREAM 0xb
#define GF_AVC_NALU_FILLER_DATA 0xc

#define GF_AVC_TYPE_P 0
#define GF_AVC_TYPE_B 1
#define GF_AVC_TYPE_I 2
#define GF_AVC_TYPE_SP 3
#define GF_AVC_TYPE_SI 4
#define GF_AVC_TYPE2_P 5
#define GF_AVC_TYPE2_B 6
#define GF_AVC_TYPE2_I 7
#define GF_AVC_TYPE2_SP 8
#define GF_AVC_TYPE2_SI 9


/*rate sizes - note that these sizes INCLUDE the rate_type header byte*/
static const u32 GF_QCELP_RATE_TO_SIZE [] = {0, 1, 1, 4, 2, 8, 3, 17, 4, 35, 5, 8, 14, 1};
static const u32 GF_QCELP_RATE_TO_SIZE_NB = 7;
static const u32 GF_SMV_EVRC_RATE_TO_SIZE [] = {0, 1, 1, 3, 2, 6, 3, 11, 4, 23, 5, 1};
static const u32 GF_SMV_EVRC_RATE_TO_SIZE_NB = 6;
static const u32 GF_AMR_FRAME_SIZE[16] = { 12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0 };
static const u32 GF_AMR_WB_FRAME_SIZE[16] = { 17, 23, 32, 36, 40, 46, 50, 58, 60, 5, 5, 0, 0, 0, 0, 0 };



/*!
 \endcond
*/


/*!
 * \brief Extra ObjectTypeIndication
 *
 *	ObjectTypeIndication for codecs not defined in MPEG-4. Since GPAC signals streams through MPEG-4 Descriptions,
 *	it needs extensions for non-MPEG-4 streams such as AMR, H263 , etc.\n
 *\note The decoder specific info for such streams is always carried encoded, with the following syntax:\n
 *	DSI Syntax for audio streams
 \code 
 *	u32 codec_four_cc: the codec 4CC reg code
 *	u16 sample_rate: sampling rate or 0 if unknown
 *	u8 nb_channels: num channels or 0 if unknown
 *	u8 nb_bits_per_sample: nb bits or 0 if unknown
 *	u8 num_samples: num audio samples per frame or 0 if unknown
 *	char *data: per-codec extensions till end of DSI bitstream
 \endcode
 \n
 *	DSI Syntax for video streams
 \code 
 *	u32 codec_four_cc: the codec 4CC reg code
 *	u16 width: video width or 0 if unknown
 *	u16 height: video height or 0 if unknown
 *	char *data: per-codec extensions till end of DSI bitstream
 \endcode
*/
#define GPAC_EXTRA_CODECS_OTI				0x80


/*!
 * \brief OGG ObjectTypeIndication
 *
 *	Object type indication for all OGG media. The DSI contains all intitialization ogg packets for the codec
 * and is formated as follows:\n
 *\code 
	while (dsi_size) {
		bit(16) packet_size;
		char packet[packet_size];
		dsi_size -= packet_size;
	}\endcode
*/
#define GPAC_OGG_MEDIA_OTI		0xDD



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
	GF_AUDIO_CH_BACK_LEFT = (1<<4),
	/*!Back Right Audio Channel*/
	GF_AUDIO_CH_BACK_RIGHT = (1<<5),
	/*!Back Center Audio Channel*/
	GF_AUDIO_CH_BACK_CENTER = (1<<6),
	/*!Side Left Audio Channel*/
	GF_AUDIO_CH_SIDE_LEFT = (1<<7),
	/*!Side Right Audio Channel*/
	GF_AUDIO_CH_SIDE_RIGHT = (1<<8)
};


/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_CONSTANTS_H_*/
