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

/*
		this file contains some constants used through-out the SDK, without a particulary sub-project attached to	
*/

/*
	supported stream types
*/
enum
{
	/*MPEG-4 stream types*/
	GF_STREAM_OD		= 0x01,
	GF_STREAM_OCR		= 0x02,
	GF_STREAM_SCENE		= 0x03,
	GF_STREAM_VISUAL	= 0x04,
	GF_STREAM_AUDIO		= 0x05,
	GF_STREAM_MPEG7		= 0x06,
	GF_STREAM_IPMP		= 0x07,
	GF_STREAM_OCI		= 0x08,
	GF_STREAM_MPEGJ		= 0x09,
	GF_STREAM_INTERACT	= 0x0A,
	GF_STREAM_IPMP_TOOL	= 0x0B,
	GF_STREAM_FONT		= 0x0C,
	GF_STREAM_TEXT		= 0x0D,

	/*GPAC internal stream types*/
};



/*supported pixel formats for everything using video*/
typedef enum
{
	/*8 bit GREY */
	GF_PIXEL_GREYSCALE	=	FOUR_CHAR_INT('G','R','E','Y'),
	/*16 bit greyscale*/
	GF_PIXEL_ALPHAGREY	=	FOUR_CHAR_INT('G','R','A','L'),
	/*15 bit RGB*/
	GF_PIXEL_RGB_555	=	FOUR_CHAR_INT('R','5','5','5'),
	/*16 bit RGB*/
	GF_PIXEL_RGB_565	=	FOUR_CHAR_INT('R','5','6','5'),
	/*24 bit RGB*/
	GF_PIXEL_RGB_24		=	FOUR_CHAR_INT('R','G','B','3'),
	/*24 bit BGR - used for graphics cards video format signaling*/
	GF_PIXEL_BGR_24		=	FOUR_CHAR_INT('B','G','R','3'),
	/*32 bit RGB*/
	GF_PIXEL_RGB_32		=	FOUR_CHAR_INT('R','G','B','4'),
	/*32 bit BGR - used for graphics cards video format signaling*/
	GF_PIXEL_BGR_32		=	FOUR_CHAR_INT('B','G','R','4'),

	/*32 bit ARGB.
	
	  Note on textures using 32 bit ARGB: 
		on little endian machines, shall be ordered in memory as BGRA, 
		on big endians, shall be ordered in memory as ARGB
	  so that *(u32*)pixel_mem is always ARGB (0xAARRGGBB).
	*/
	GF_PIXEL_ARGB		=	FOUR_CHAR_INT('A','R','G','B'),
	/*32bit RGBA (openGL like)*/
	GF_PIXEL_RGBA		=	FOUR_CHAR_INT('R','G','B', 'A'),

	/*	YUV packed formats sampled 1:2:2 horiz, 1:1:1 vert*/
	GF_PIXEL_YUY2		=	FOUR_CHAR_INT('Y','U','Y','2'),
	GF_PIXEL_YVYU		=	FOUR_CHAR_INT('Y','V','Y','U'),
	GF_PIXEL_UYVY		=	FOUR_CHAR_INT('U','Y','V','Y'),
	GF_PIXEL_VYUY		=	FOUR_CHAR_INT('V','Y','U','Y'),
	GF_PIXEL_Y422		=	FOUR_CHAR_INT('Y','4','2','2'),
	GF_PIXEL_UYNV		=	FOUR_CHAR_INT('U','Y','N','V'),
	GF_PIXEL_YUNV		=	FOUR_CHAR_INT('Y','U','N','V'),
	GF_PIXEL_V422		=	FOUR_CHAR_INT('V','4','2','2'),
	
	/*	YUV planar formats sampled 1:2:2 horiz, 1:2:2 vert*/
	GF_PIXEL_YV12		=	FOUR_CHAR_INT('Y','V','1','2'),
	GF_PIXEL_IYUV		=	FOUR_CHAR_INT('I','Y','U','V'),
	GF_PIXEL_I420		=	FOUR_CHAR_INT('I','4','2','0'),

	/*YV12 + Alpha plane*/
	GF_PIXEL_YUVA		=	FOUR_CHAR_INT('Y', 'U', 'V', 'A')

} GF_PixelFormat;



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





/*extensions for undefined codecs - this allows demuxers and codecs to talk the same language*/

/*this is the OTI (user-priv) used for all undefined codec using MP4/QT 4CC codes*/
#define GPAC_QT_CODECS_OTI				0x80

/*The decoder specific info for all unknown decoders - it is always carried encoded

	u32 codec_four_cc: the codec 4CC reg code
	- for audio - 
	u16 sample_rate: sampling rate or 0 if unknown
	u8 nb_channels: num channels or 0 if unknown
	u8 nb_bits_per_sample: nb bits or 0 if unknown
	u8 num_samples: num audio samples per frame or 0 if unknown

  	- for video - 
	u16 width: video width or 0 if unknown;
	u16 height: video height or 0 if unknown;

	- till end of DSI bitstream-
	char *data: per-codec extensions 
*/

/*this streamtype (user-priv) is reserved for streams only used to create a scene decoder handling the service 
by itself, as is the case of the BT/VRML reader and can be used by many other things. 
The decoderSpecificInfo carried is simply the local filename of the service (remote files are first entirelly fetched).
The inBufferLength param for decoders using these streams is the stream clock in ms (no input data is given)
There is a dummy module available generating this stream and taking care of proper clock init in case 
of seeking
*this is a reentrant streamtype: if any media object with this streamtype also exist in the scene, they will be 
attached to the scene decoder (except when a new inline scene is detected, in which case a new decoder will 
be created). This allows for animation/sprite usage along with the systems timing/stream management

the objectTypeIndication used for these streams are
		0x00	-	 Forbidden
		0x01	-	 VRML/BT/XMT/SWF loader (eg MP4Box context loading)
		0x02	-	 SVG loader
*/
#define GF_STREAM_PRIVATE_SCENE	0x20

/*object type indication for static OD
	this is used when scene information is not available (or not trustable :), in which case the IOD will 
	only contain the OD stream with this OTI. 
	This OD stream shall send one access unit with all available ressources in the service. Note that it may
	still act as a regular OD stream in case ressources are updated on the fly (broadcast radio/tv for ex).
	The scenegraph wll be regenerated at each OD AU, based on all available objects.
	Using this OTI will enable user stream selection (if provided in GUI) which is otherwise disabled.
	In this mode all clock references are ignored and all streams synchronized on the OD stream.
*/
#define GPAC_STATIC_OD_OTI		0x81

/*object type indication for all OGG media
	DSI is formated as follows:
		while (dsi_size) {
			bit(16) packet_size;
			char packet[packet_size];
			dsi_size -= packet_size;
		}
	this allows storing all initialization pages for vorbis, theora and co
*/
#define GPAC_OGG_MEDIA_OTI		0xDD




/*channel cfg flags - DECODERS MUST OUTPUT STEREO/MULTICHANNEL IN THIS ORDER*/
enum
{
	GF_AUDIO_CH_UNKNOWN = 0,
	GF_AUDIO_CH_FRONT_LEFT = (1),
	GF_AUDIO_CH_FRONT_RIGHT = (1<<1),
	GF_AUDIO_CH_FRONT_CENTER = (1<<2),
	GF_AUDIO_CH_LFE = (1<<3),
	GF_AUDIO_CH_BACK_LEFT = (1<<4),
	GF_AUDIO_CH_BACK_RIGHT = (1<<5),
	GF_AUDIO_CH_BACK_CENTER = (1<<6),
	GF_AUDIO_CH_SIDE_LEFT = (1<<7),
	GF_AUDIO_CH_SIDE_RIGHT = (1<<8)
};

/*Media Object types. This type provides a hint to network modules which may have to generate an OD/IOD on the fly
they occur only if objects/services used in the scene are not referenced through OD IDs but direct URL*/
enum
{
	/*OD expected is of undefined type*/
	GF_MEDIA_OBJECT_UNDEF = 0,
	/*OD expected is of SCENE type (eg, BIFS and OD if needed)*/
	GF_MEDIA_OBJECT_SCENE,
	/*OD expected is of BIFS type (anim streams)*/
	GF_MEDIA_OBJECT_BIFS,
	/*OD expected is of VISUAL type*/
	GF_MEDIA_OBJECT_VIDEO,
	/*OD expected is of AUDIO type*/
	GF_MEDIA_OBJECT_AUDIO,
	/*OD expected is of TEXT type (3GPP/MPEG4)*/
	GF_MEDIA_OBJECT_TEXT,
	/*OD expected is of InputSensor type*/
	GF_MEDIA_OBJECT_INTERACT,
};

#ifdef __cplusplus
}
#endif

#endif	/*_GF_CONSTANTS_H_*/
