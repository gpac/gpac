/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2024
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
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



#ifndef _GF_ISOMEDIA_H_
#define _GF_ISOMEDIA_H_


#ifdef __cplusplus
extern "C" {
#endif

/*!
\file <gpac/isomedia.h>
\brief ISOBMFF parsing and writing library.
*/

/*!
\addtogroup iso_grp ISO Base Media File
\brief ISOBMF, 3GPP, AVC and HEVC file format utilities.

This section documents the reading and writing of ISOBMF (MP4, 3GPP, AVC, HEVC HEIF ...)
The library supports:
- regular movie read
- regular movie write
- fragmented movie and movie segments read
- fragmented movie and movie segments write
- QT support
- Sample descriptions for most common media found in such files (audio, video, text and subtitles)
- Meta and HEIF read
- Meta and HEIF write
- Common Encryption ISMA E&A and OMA DRM support
- MPEG-4 Systems extensions


All the READ function in this API can be used in EDIT/WRITE mode.
However, some unexpected errors or values may happen in that case, depending
on how much modifications you made (timing, track with 0 samples, ...).
On the other hand, none of the EDIT/WRITE functions will work in READ mode.

The output structure of a edited file will sometimes be different
from the original file, but the media-data and meta-data will be identical.
The only change happens in the file media-data container(s) during edition

When editing the file, you MUST set the final name of the modified file
to something different. This API doesn't allow file overwriting.

@{
 */

#include <gpac/tools.h>


/*! Track reference types

Some track may depend on other tracks for several reasons. They reference these tracks through the following Reference Types
*/
enum
{
	/*! ref type for the OD track dependencies*/
	GF_ISOM_REF_OD = GF_4CC( 'm', 'p', 'o', 'd' ),
	/*! ref type for stream dependencies*/
	GF_ISOM_REF_DECODE = GF_4CC( 'd', 'p', 'n', 'd' ),
	/*! ref type for OCR (Object Clock Reference) dependencies*/
	GF_ISOM_REF_OCR = GF_4CC( 's', 'y', 'n', 'c' ),
	/*! ref type for IPI (Intellectual Property Information) dependencies*/
	GF_ISOM_REF_IPI = GF_4CC( 'i', 'p', 'i', 'r' ),
	/*! this track describes the referenced tr*/
	GF_ISOM_REF_META = GF_4CC( 'c', 'd', 's', 'c' ),
	/*! ref type for Hint tracks*/
	GF_ISOM_REF_HINT = GF_4CC( 'h', 'i', 'n', 't' ),
	/*! ref type for QT Chapter tracks*/
	GF_ISOM_REF_CHAP = GF_4CC( 'c', 'h', 'a', 'p' ),
	/*! ref type for the SVC and SHVC base tracks*/
	GF_ISOM_REF_BASE = GF_4CC( 's', 'b', 'a', 's' ),
	/*! ref type for the SVC and SHVC extractor reference tracks*/
	GF_ISOM_REF_SCAL = GF_4CC( 's', 'c', 'a', 'l' ),
	/*! ref type for the SHVC tile base tracks*/
	GF_ISOM_REF_TBAS = GF_4CC( 't', 'b', 'a', 's' ),
	/*! ref type for the SHVC tile tracks*/
	GF_ISOM_REF_SABT = GF_4CC( 's', 'a', 'b', 't' ),
	/*! ref type for the SHVC oinf track*/
	GF_ISOM_REF_OREF = GF_4CC( 'o', 'r', 'e', 'f' ),
	/*! this track uses fonts carried/defined in the referenced track*/
	GF_ISOM_REF_FONT = GF_4CC( 'f', 'o', 'n', 't' ),
	/*! this track depends on the referenced hint track, i.e., it should only be used if the referenced hint track is used.*/
	GF_ISOM_REF_HIND = GF_4CC( 'h', 'i', 'n', 'd' ),
	/*! this track contains auxiliary depth video information for the referenced video track*/
	GF_ISOM_REF_VDEP = GF_4CC( 'v', 'd', 'e', 'p' ),
	/*! this track contains auxiliary parallax video information for the referenced video track*/
	GF_ISOM_REF_VPLX = GF_4CC( 'v', 'p', 'l', 'x' ),
	/*! this track contains subtitle, timed text or overlay graphical information for the referenced track or any track in the alternate group to which the track belongs, if any*/
	GF_ISOM_REF_SUBT = GF_4CC( 's', 'u', 'b', 't' ),
	/*! thumbnail track*/
	GF_ISOM_REF_THUMB = GF_4CC( 't', 'h', 'm', 'b' ),
	/*DRC*/
	/*! additional audio track*/
	GF_ISOM_REF_ADDA = GF_4CC( 'a', 'd', 'd', 'a' ),
	/*! DRC metadata*/
	GF_ISOM_REF_ADRC = GF_4CC( 'a', 'd', 'r', 'c' ),
	/*! item->track location*/
	GF_ISOM_REF_ILOC = GF_4CC( 'i', 'l', 'o', 'c' ),
	/*! AVC dep stream*/
	GF_ISOM_REF_AVCP = GF_4CC( 'a', 'v', 'c', 'p' ),
	/*! AVC switch to*/
	GF_ISOM_REF_SWTO = GF_4CC( 's', 'w', 't', 'o' ),
	/*! AVC switch from*/
	GF_ISOM_REF_SWFR = GF_4CC( 's', 'w', 'f', 'r' ),

	/*! Time code*/
	GF_ISOM_REF_TMCD = GF_4CC( 't', 'm', 'c', 'd' ),
	/*! Structural dependency*/
	GF_ISOM_REF_CDEP = GF_4CC( 'c', 'd', 'e', 'p' ),
	/*! transcript*/
	GF_ISOM_REF_SCPT = GF_4CC( 's', 'c', 'p', 't' ),
	/*! nonprimary source description*/
	GF_ISOM_REF_SSRC = GF_4CC( 's', 's', 'r', 'c' ),
	/*! layer audio track dependency*/
	GF_ISOM_REF_LYRA = GF_4CC( 'l', 'y', 'r', 'a' ),
	/*! File Delivery Item Information Extension */
	GF_ISOM_REF_FDEL = GF_4CC( 'f', 'd', 'e', 'l' ),
#ifdef GF_ENABLE_CTRN
	/*! Track fragment inherit */
	GF_ISOM_REF_TRIN = GF_4CC( 't', 'r', 'i', 'n' ),
#endif

	/*! Item auxiliary reference */
	GF_ISOM_REF_AUXR = GF_4CC( 'a', 'u', 'x', 'r' ),

	/*! ref type for the VVC subpicture tracks*/
	GF_ISOM_REF_SUBPIC = GF_4CC( 's', 'u', 'b', 'p' ),
};

/*! Track Edit list type*/
typedef enum {
	/*! empty segment in the track (no media for this segment)*/
	GF_ISOM_EDIT_EMPTY		=	0x00,
	/*! dwelled segment in the track (one media sample for this segment)*/
	GF_ISOM_EDIT_DWELL		=	0x01,
	/*! normal segment in the track*/
	GF_ISOM_EDIT_NORMAL		=	0x02
} GF_ISOEditType;

/*! Generic Media Types (YOU HAVE TO USE ONE OF THESE TYPES FOR COMPLIANT ISO MEDIA FILES)*/
enum
{
	/*base media types*/
	GF_ISOM_MEDIA_VISUAL	= GF_4CC( 'v', 'i', 'd', 'e' ),
    GF_ISOM_MEDIA_AUXV      = GF_4CC( 'a', 'u', 'x', 'v' ),
    GF_ISOM_MEDIA_PICT      = GF_4CC( 'p', 'i', 'c', 't' ),
	GF_ISOM_MEDIA_AUDIO		= GF_4CC( 's', 'o', 'u', 'n' ),
	GF_ISOM_MEDIA_HINT		= GF_4CC( 'h', 'i', 'n', 't' ),
	GF_ISOM_MEDIA_META		= GF_4CC( 'm', 'e', 't', 'a' ),
	GF_ISOM_MEDIA_TEXT		= GF_4CC( 't', 'e', 'x', 't' ),
	/*subtitle code point used on ipod - same as text*/
	GF_ISOM_MEDIA_SUBT		= GF_4CC( 's', 'b', 't', 'l' ),
	GF_ISOM_MEDIA_SUBPIC	= GF_4CC( 's', 'u', 'b', 'p' ),
	GF_ISOM_MEDIA_MPEG_SUBT	= GF_4CC( 's', 'u', 'b', 't' ),
	/*closed caption track types for QT/ProRes*/
	GF_ISOM_MEDIA_CLOSED_CAPTION		= GF_4CC( 'c', 'l', 'c', 'p' ),
	/*timecode metadata for QT/ProRes*/
	GF_ISOM_MEDIA_TIMECODE		= GF_4CC( 't', 'm', 'c', 'd' ),
	/*MPEG-4 media types*/
	GF_ISOM_MEDIA_OD		= GF_4CC( 'o', 'd', 's', 'm' ),
	GF_ISOM_MEDIA_OCR		= GF_4CC( 'c', 'r', 's', 'm' ),
	GF_ISOM_MEDIA_SCENE		= GF_4CC( 's', 'd', 's', 'm' ),
	GF_ISOM_MEDIA_MPEG7		= GF_4CC( 'm', '7', 's', 'm' ),
	GF_ISOM_MEDIA_OCI		= GF_4CC( 'o', 'c', 's', 'm' ),
	GF_ISOM_MEDIA_IPMP		= GF_4CC( 'i', 'p', 's', 'm' ),
	GF_ISOM_MEDIA_MPEGJ		= GF_4CC( 'm', 'j', 's', 'm' ),
	/*GPAC-defined, for any track using MPEG-4 systems signaling but with undefined streaml types*/
	GF_ISOM_MEDIA_ESM		= GF_4CC( 'g', 'e', 's', 'm' ),
	/*DIMS media type (same as scene but with a different mediaInfo)*/
	GF_ISOM_MEDIA_DIMS		= GF_4CC( 'd', 'i', 'm', 's' ),
	/*SWF file embedded in media track*/
	GF_ISOM_MEDIA_FLASH		= GF_4CC( 'f', 'l', 's', 'h' ),
	/*QTVR track*/
	GF_ISOM_MEDIA_QTVR		= GF_4CC( 'q', 't', 'v', 'r' ),
	GF_ISOM_MEDIA_JPEG		= GF_4CC( 'j', 'p', 'e', 'g' ),
	GF_ISOM_MEDIA_JP2		= GF_4CC( 'j', 'p', '2', ' ' ),
	GF_ISOM_MEDIA_PNG		= GF_4CC( 'p', 'n', 'g', ' ' ),
};


/*! specific media sub-types - you shall make sure the media sub type is what you expect*/
enum
{
	/*reserved, internal use in the lib. Indicates the track complies to MPEG-4 system
	specification, and the usual OD framework tools may be used*/
	GF_ISOM_SUBTYPE_MPEG4		= GF_4CC( 'M', 'P', 'E', 'G' ),

	/*reserved, internal use in the lib. Indicates the track is of GF_ISOM_SUBTYPE_MPEG4
	but it is encrypted.*/
	GF_ISOM_SUBTYPE_MPEG4_CRYP	= GF_4CC( 'E', 'N', 'C', 'M' ),

	/*restricted video subtype*/
	GF_ISOM_SUBTYPE_RESV	= GF_4CC( 'r', 'e', 's', 'v' ),


	/*AVC/H264 media type - not listed as an MPEG-4 type, ALTHOUGH this library automatically remaps
	GF_AVCConfig to MPEG-4 ESD*/
	GF_ISOM_SUBTYPE_AVC_H264		= GF_4CC( 'a', 'v', 'c', '1' ),
	GF_ISOM_SUBTYPE_AVC2_H264		= GF_4CC( 'a', 'v', 'c', '2' ),
	GF_ISOM_SUBTYPE_AVC3_H264		= GF_4CC( 'a', 'v', 'c', '3' ),
	GF_ISOM_SUBTYPE_AVC4_H264		= GF_4CC( 'a', 'v', 'c', '4' ),
	GF_ISOM_SUBTYPE_SVC_H264		= GF_4CC( 's', 'v', 'c', '1' ),
	GF_ISOM_SUBTYPE_MVC_H264		= GF_4CC( 'm', 'v', 'c', '1' ),

	/*HEVC media type*/
	GF_ISOM_SUBTYPE_HVC1			= GF_4CC( 'h', 'v', 'c', '1' ),
	GF_ISOM_SUBTYPE_HEV1			= GF_4CC( 'h', 'e', 'v', '1' ),
	GF_ISOM_SUBTYPE_HVC2			= GF_4CC( 'h', 'v', 'c', '2' ),
	GF_ISOM_SUBTYPE_HEV2			= GF_4CC( 'h', 'e', 'v', '2' ),
	GF_ISOM_SUBTYPE_LHV1			= GF_4CC( 'l', 'h', 'v', '1' ),
	GF_ISOM_SUBTYPE_LHE1			= GF_4CC( 'l', 'h', 'e', '1' ),
	GF_ISOM_SUBTYPE_HVT1			= GF_4CC( 'h', 'v', 't', '1' ),

	/*VVC media types*/
	GF_ISOM_SUBTYPE_VVC1			= GF_4CC( 'v', 'v', 'c', '1' ),
	GF_ISOM_SUBTYPE_VVI1			= GF_4CC( 'v', 'v', 'i', '1' ),
	GF_ISOM_SUBTYPE_VVS1			= GF_4CC( 'v', 'v', 's', '1' ),
	GF_ISOM_SUBTYPE_VVCN			= GF_4CC( 'v', 'v', 'c', 'N' ),

	/*AV1 media type*/
	GF_ISOM_SUBTYPE_AV01 = GF_4CC('a', 'v', '0', '1'),

	/*Opus media type*/
	GF_ISOM_SUBTYPE_OPUS = GF_4CC('O', 'p', 'u', 's'),
	GF_ISOM_SUBTYPE_FLAC = GF_4CC( 'f', 'L', 'a', 'C' ),

	/* VP */
	GF_ISOM_SUBTYPE_VP08 = GF_4CC('v', 'p', '0', '8'),
	GF_ISOM_SUBTYPE_VP09 = GF_4CC('v', 'p', '0', '9'),
	GF_ISOM_SUBTYPE_VP10 = GF_4CC('v', 'p', '1', '0'),

	/* Dolby Vision */
	GF_ISOM_SUBTYPE_DVHE = GF_4CC('d', 'v', 'h', 'e'),
	GF_ISOM_SUBTYPE_DVH1 = GF_4CC('d', 'v', 'h', '1'),
	GF_ISOM_SUBTYPE_DVA1 = GF_4CC('d', 'v', 'a', '1'),
	GF_ISOM_SUBTYPE_DVAV = GF_4CC('d', 'v', 'a', 'v'),
	GF_ISOM_SUBTYPE_DAV1 = GF_4CC('d', 'a', 'v', '1'),

	/*3GPP(2) extension subtypes*/
	GF_ISOM_SUBTYPE_3GP_H263	= GF_4CC( 's', '2', '6', '3' ),
	GF_ISOM_SUBTYPE_3GP_AMR		= GF_4CC( 's', 'a', 'm', 'r' ),
	GF_ISOM_SUBTYPE_3GP_AMR_WB	= GF_4CC( 's', 'a', 'w', 'b' ),
	GF_ISOM_SUBTYPE_3GP_EVRC	= GF_4CC( 's', 'e', 'v', 'c' ),
	GF_ISOM_SUBTYPE_3GP_QCELP	= GF_4CC( 's', 'q', 'c', 'p' ),
	GF_ISOM_SUBTYPE_3GP_SMV		= GF_4CC( 's', 's', 'm', 'v' ),

	/*3GPP DIMS*/
	GF_ISOM_SUBTYPE_3GP_DIMS	= GF_4CC( 'd', 'i', 'm', 's' ),

	GF_ISOM_SUBTYPE_AC3			= GF_4CC( 'a', 'c', '-', '3' ),
	GF_ISOM_SUBTYPE_EC3			= GF_4CC( 'e', 'c', '-', '3' ),
	GF_ISOM_SUBTYPE_MP3			= GF_4CC( '.', 'm', 'p', '3' ),
	GF_ISOM_SUBTYPE_MLPA		= GF_4CC( 'm', 'l', 'p', 'a' ),

	GF_ISOM_SUBTYPE_MP4A		= GF_4CC( 'm', 'p', '4', 'a' ),
	GF_ISOM_SUBTYPE_MP4S		= GF_4CC( 'm', 'p', '4', 's' ),

	GF_ISOM_SUBTYPE_LSR1		= GF_4CC( 'l', 's', 'r', '1' ),
	GF_ISOM_SUBTYPE_WVTT		= GF_4CC( 'w', 'v', 't', 't' ),
	GF_ISOM_SUBTYPE_STXT		= GF_4CC( 's', 't', 'x', 't' ),
	GF_ISOM_SUBTYPE_STPP		= GF_4CC( 's', 't', 'p', 'p' ),
	GF_ISOM_SUBTYPE_SBTT		= GF_4CC( 's', 'b', 't', 't' ),
	GF_ISOM_SUBTYPE_METT		= GF_4CC( 'm', 'e', 't', 't' ),
	GF_ISOM_SUBTYPE_METX		= GF_4CC( 'm', 'e', 't', 'x' ),
	GF_ISOM_SUBTYPE_TX3G		= GF_4CC( 't', 'x', '3', 'g' ),
	GF_ISOM_SUBTYPE_TEXT		= GF_4CC( 't', 'e', 'x', 't' ),
	GF_ISOM_SUBTYPE_SUBTITLE	= GF_4CC( 's', 'b', 't', 'l' ),


	GF_ISOM_SUBTYPE_RTP			= GF_4CC( 'r', 't', 'p', ' ' ),
	GF_ISOM_SUBTYPE_SRTP		= GF_4CC( 's', 'r', 't', 'p' ),
	GF_ISOM_SUBTYPE_RRTP		= GF_4CC( 'r', 'r', 't', 'p' ),
	GF_ISOM_SUBTYPE_RTCP		= GF_4CC( 'r', 't', 'c', 'p' ),
	GF_ISOM_SUBTYPE_FLUTE		= GF_4CC( 'f', 'd', 'p', ' ' ),

	/* Apple XDCAM */
	GF_ISOM_SUBTYPE_XDVB		= GF_4CC( 'x', 'd', 'v', 'b' ),

	GF_ISOM_SUBTYPE_H263		= GF_4CC( 'h', '2', '6', '3' ),

	GF_ISOM_SUBTYPE_JPEG		= GF_4CC( 'j', 'p', 'e', 'g' ),
	GF_ISOM_SUBTYPE_PNG 		= GF_4CC( 'p', 'n', 'g', ' ' ),
	GF_ISOM_SUBTYPE_MJP2 		= GF_4CC( 'm', 'j', 'p', '2' ),
	GF_ISOM_SUBTYPE_JP2K		= GF_4CC('j','p','2','k'),

	GF_ISOM_SUBTYPE_MH3D_MHA1	= GF_4CC( 'm', 'h', 'a', '1' ),
	GF_ISOM_SUBTYPE_MH3D_MHA2	= GF_4CC( 'm', 'h', 'a', '2' ),
	GF_ISOM_SUBTYPE_MH3D_MHM1	= GF_4CC( 'm', 'h', 'm', '1' ),
	GF_ISOM_SUBTYPE_MH3D_MHM2	= GF_4CC( 'm', 'h', 'm', '2' ),

	GF_ISOM_SUBTYPE_IPCM		= GF_4CC( 'i', 'p', 'c', 'm' ),
	GF_ISOM_SUBTYPE_FPCM		= GF_4CC( 'f', 'p', 'c', 'm' ),

	/* on-screen colours */
	GF_ISOM_SUBTYPE_NCLX 		= GF_4CC( 'n', 'c', 'l', 'x' ),
	GF_ISOM_SUBTYPE_NCLC 		= GF_4CC( 'n', 'c', 'l', 'c' ),
	GF_ISOM_SUBTYPE_PROF 		= GF_4CC( 'p', 'r', 'o', 'f' ),
	GF_ISOM_SUBTYPE_RICC 		= GF_4CC( 'r', 'I', 'C', 'C' ),

	/* QT audio codecs */
	//this one is also used for 24bit RGB
	GF_QT_SUBTYPE_RAW	= GF_4CC('r','a','w',' '),
	GF_QT_SUBTYPE_TWOS 	= GF_4CC('t','w','o','s'),
	GF_QT_SUBTYPE_SOWT 	= GF_4CC('s','o','w','t'),
	GF_QT_SUBTYPE_FL32 	= GF_4CC('f','l','3','2'),
	GF_QT_SUBTYPE_FL64 	= GF_4CC('f','l','6','4'),
	GF_QT_SUBTYPE_IN24 	= GF_4CC('i','n','2','4'),
	GF_QT_SUBTYPE_IN32 	= GF_4CC('i','n','3','2'),
	GF_QT_SUBTYPE_ULAW 	= GF_4CC('u','l','a','w'),
	GF_QT_SUBTYPE_ALAW 	= GF_4CC('a','l','a','w'),
	GF_QT_SUBTYPE_ADPCM 	= GF_4CC(0x6D,0x73,0x00,0x02),
	GF_QT_SUBTYPE_IMA_ADPCM 	= GF_4CC(0x6D,0x73,0x00,0x11),
	GF_QT_SUBTYPE_DVCA 	= GF_4CC('d','v','c','a'),
	GF_QT_SUBTYPE_QDMC 	= GF_4CC('Q','D','M','C'),
	GF_QT_SUBTYPE_QDMC2	= GF_4CC('Q','D','M','2'),
	GF_QT_SUBTYPE_QCELP	= GF_4CC('Q','c','l','p'),
	GF_QT_SUBTYPE_kMP3 	= GF_4CC(0x6D,0x73,0x00,0x55),

	/* QT video codecs */
	GF_QT_SUBTYPE_C608	= GF_4CC( 'c', '6', '0', '8' ),
	GF_QT_SUBTYPE_APCH	= GF_4CC( 'a', 'p', 'c', 'h' ),
	GF_QT_SUBTYPE_APCO	= GF_4CC( 'a', 'p', 'c', 'o' ),
	GF_QT_SUBTYPE_APCN	= GF_4CC( 'a', 'p', 'c', 'n' ),
	GF_QT_SUBTYPE_APCS	= GF_4CC( 'a', 'p', 'c', 's' ),
	GF_QT_SUBTYPE_AP4X	= GF_4CC( 'a', 'p', '4', 'x' ),
	GF_QT_SUBTYPE_AP4H	= GF_4CC( 'a', 'p', '4', 'h' ),
	GF_QT_SUBTYPE_YUYV = GF_4CC('y','u','v','2'),
	GF_QT_SUBTYPE_UYVY = GF_4CC('2','v','u','y'),
	GF_QT_SUBTYPE_YUV444 = GF_4CC('v','3','0','8'),
	GF_QT_SUBTYPE_YUVA444 = GF_4CC('v','4','0','8'),
	GF_QT_SUBTYPE_YUV422_10 = GF_4CC('v','2','1','0'),
	GF_QT_SUBTYPE_YUV444_10 = GF_4CC('v','4','1','0'),
	GF_QT_SUBTYPE_YUV422_16 = GF_4CC('v','2','1','6'),
	GF_QT_SUBTYPE_YUV420 = GF_4CC('j','4','2','0'),
	GF_QT_SUBTYPE_I420 = GF_4CC('I','4','2','0'),
	GF_QT_SUBTYPE_IYUV = GF_4CC('I','Y','U','V'),
	GF_QT_SUBTYPE_YV12 = GF_4CC('y','v','1','2'),
	GF_QT_SUBTYPE_YVYU = GF_4CC('Y','V','Y','U'),
	GF_QT_SUBTYPE_RGBA = GF_4CC('R','G','B','A'),
	GF_QT_SUBTYPE_ABGR = GF_4CC('A','B','G','R'),
	GF_QT_SUBTYPE_ALAC =  GF_4CC('a','l','a','c'),
	GF_QT_SUBTYPE_LPCM =  GF_4CC('l','p','c','m'),
	GF_ISOM_SUBTYPE_FFV1		= GF_4CC( 'F', 'F', 'V', '1' ),

	GF_ISOM_ITEM_TYPE_AUXI 	= GF_4CC('a', 'u', 'x', 'i'),

	GF_QT_SUBTYPE_TMCD = GF_4CC( 't', 'm', 'c', 'd' ),

	GF_ISOM_SUBTYPE_VC1 = GF_4CC( 'v', 'c', '-', '1' ),

	/*GPAC extensions*/
	GF_ISOM_SUBTYPE_DVB_SUBS	= GF_4CC( 'd', 'v', 'b', 's' ),
	GF_ISOM_SUBTYPE_DVB_TELETEXT	= GF_4CC( 'd', 'v', 'b', 't' ),


	GF_ISOM_SUBTYPE_DTSC = GF_4CC('d','t','s','c'),
	GF_ISOM_SUBTYPE_DTSH = GF_4CC('d','t','s','h'),
	GF_ISOM_SUBTYPE_DTSL = GF_4CC('d','t','s','l'),
	GF_ISOM_SUBTYPE_DTSE = GF_4CC('d','t','s','e'),
	GF_ISOM_SUBTYPE_DTSX = GF_4CC('d','t','s','x'),
	GF_ISOM_SUBTYPE_DTSY = GF_4CC('d','t','s','y'),

	GF_ISOM_SUBTYPE_UNCV	= GF_4CC( 'u', 'n', 'c', 'v' ),
	GF_ISOM_ITEM_TYPE_UNCI	= GF_4CC( 'u', 'n', 'c', 'i' ),
};




/*! direction for sample search (including SyncSamples search)
Function using search allways specify the desired time in composition (presentation) time
		(Sample N-1)	DesiredTime		(Sample N)
*/
typedef enum
{
	/*! FORWARD: will give the next sample given the desired time (eg, N)*/
	GF_ISOM_SEARCH_FORWARD		=	1,
	/*! BACKWARD: will give the previous sample given the desired time (eg, N-1)*/
	GF_ISOM_SEARCH_BACKWARD		=	2,
	/*! SYNCFORWARD: will search from the desired point in time for a sync sample if any. If no sync info, behaves as FORWARD*/
	GF_ISOM_SEARCH_SYNC_FORWARD	=	3,
	/*! SYNCBACKWARD: will search till the desired point in time for a sync sample if any. If no sync info, behaves as BACKWARD*/
	GF_ISOM_SEARCH_SYNC_BACKWARD	=	4,
	/*! SYNCSHADOW: use the sync shadow information to retrieve the sample. If no SyncShadow info, behave as SYNCBACKWARD
	\warning deprecated in ISOBMFF*/
	GF_ISOM_SEARCH_SYNC_SHADOW		=	5
} GF_ISOSearchMode;

/*! Predefined File Brand codes (MPEG-4 and JPEG2000)*/
enum
{
	/*file complying to the generic ISO Media File (base specification ISO/IEC 14496-12)
	this is the default brand when creating a new movie*/
	GF_ISOM_BRAND_ISOM = GF_4CC( 'i', 's', 'o', 'm' ),
	/*file complying to the generic ISO Media File (base specification ISO/IEC 14496-12) + Meta extensions*/
	GF_ISOM_BRAND_ISO2 =  GF_4CC( 'i', 's', 'o', '2' ),
	/*file complying to ISO/IEC 14496-1 2001 edition. A .mp4 file without a brand
	is equivalent to a file compatible with this brand*/
	GF_ISOM_BRAND_MP41 = GF_4CC( 'm', 'p', '4', '1' ),
	/*file complying to ISO/IEC 14496-14 (MP4 spec)*/
	GF_ISOM_BRAND_MP42 = GF_4CC( 'm', 'p', '4', '2' ),
	/*file complying to ISO/IEC 15444-3 (JPEG2000) without profile restriction*/
	GF_ISOM_BRAND_MJP2 = GF_4CC( 'm', 'j', 'p', '2' ),
	/*file complying to ISO/IEC 15444-3 (JPEG2000) with simple profile restriction*/
	GF_ISOM_BRAND_MJ2S = GF_4CC( 'm', 'j', '2', 's' ),
	/*old versions of 3GPP spec (without timed text)*/
	GF_ISOM_BRAND_3GP4 = GF_4CC('3', 'g', 'p', '4'),
	GF_ISOM_BRAND_3GP5 = GF_4CC('3', 'g', 'p', '5'),
	/*final version of 3GPP file spec*/
	GF_ISOM_BRAND_3GP6 = GF_4CC('3', 'g', 'p', '6'),
	/*generci 3GPP file (several audio tracks, etc..)*/
	GF_ISOM_BRAND_3GG5 = GF_4CC('3', 'g', 'g', '5'),
	GF_ISOM_BRAND_3GG6 = GF_4CC('3', 'g', 'g', '6'),
	/*3GPP2 file spec*/
	GF_ISOM_BRAND_3G2A = GF_4CC('3', 'g', '2', 'a'),
	/*AVC file spec*/
	GF_ISOM_BRAND_AVC1 = GF_4CC('a', 'v', 'c', '1'),
	/* file complying to ISO/IEC 21000-9:2005 (MPEG-21 spec)*/
	GF_ISOM_BRAND_MP21 = GF_4CC('m', 'p', '2', '1'),
	/*file complying to the generic ISO Media File (base specification ISO/IEC 14496-12) + support for version 1*/
	GF_ISOM_BRAND_ISO4 =  GF_4CC( 'i', 's', 'o', '4' ),
	/* Image File Format */
	GF_ISOM_BRAND_HEIF = GF_4CC('h', 'e', 'i', 'f'),
	GF_ISOM_BRAND_MIF1 = GF_4CC('m', 'i', 'f', '1'),
	GF_ISOM_BRAND_HEIC = GF_4CC('h', 'e', 'i', 'c'),
	GF_ISOM_BRAND_HEIM = GF_4CC('h', 'e', 'i', 'm'),
	GF_ISOM_BRAND_AVIF = GF_4CC('a', 'v', 'i', 'f'),
	GF_ISOM_BRAND_AVCI = GF_4CC('a', 'v', 'c', 'i'),
	GF_ISOM_BRAND_VVIC = GF_4CC('v', 'v', 'i', 'c'),

	/*other iso media brands */
	GF_ISOM_BRAND_ISO1 = GF_4CC( 'i', 's', 'o', '1' ),
	GF_ISOM_BRAND_ISO3 = GF_4CC( 'i', 's', 'o', '3' ),
	GF_ISOM_BRAND_ISO5 = GF_4CC( 'i', 's', 'o', '5' ),
	GF_ISOM_BRAND_ISO6 = GF_4CC( 'i', 's', 'o', '6' ),
	GF_ISOM_BRAND_ISO7 = GF_4CC( 'i', 's', 'o', '7' ),
	GF_ISOM_BRAND_ISO8 = GF_4CC( 'i', 's', 'o', '8' ),
	GF_ISOM_BRAND_ISO9 = GF_4CC( 'i', 's', 'o', '9' ),
	GF_ISOM_BRAND_ISOA = GF_4CC( 'i', 's', 'o', 'a' ),

	/* QT brand*/
	GF_ISOM_BRAND_QT  = GF_4CC( 'q', 't', ' ', ' ' ),

	/* JPEG 2000 Image (.JP2) [ISO 15444-1] */
	GF_ISOM_BRAND_JP2  = GF_4CC( 'j', 'p', '2', ' ' ),

	/* MPEG-4 (.MP4) for SonyPSP */
	GF_ISOM_BRAND_MSNV = GF_4CC( 'M', 'S', 'N', 'V' ),
	/* Apple iTunes AAC-LC (.M4A) Audio */
	GF_ISOM_BRAND_M4A  = GF_4CC( 'M', '4', 'A', ' ' ),
	/* Apple iTunes Video (.M4V) Video */
	GF_ISOM_BRAND_M4V  = GF_4CC( 'M', '4', 'V', ' ' ),

	GF_ISOM_BRAND_HVCE = GF_4CC( 'h', 'v', 'c', 'e' ),
	GF_ISOM_BRAND_HVCI = GF_4CC( 'h', 'v', 'c', 'i' ),
	GF_ISOM_BRAND_HVTI = GF_4CC( 'h', 'v', 't', 'i' ),


	GF_ISOM_BRAND_AV01 = GF_4CC( 'a', 'v', '0', '1'),

	GF_ISOM_BRAND_OPUS = GF_4CC( 'O', 'p', 'u', 's'),

	GF_ISOM_BRAND_ISMA = GF_4CC( 'I', 'S', 'M', 'A' ),

	/* dash related brands (ISO/IEC 23009-1) */
	GF_ISOM_BRAND_DASH = GF_4CC('d','a','s','h'),
	/* Media Segment conforming to the DASH Self-Initializing Media Segment format type */
	GF_ISOM_BRAND_DSMS = GF_4CC('d','s','m','s'),
	/* Media Segment conforming to the general format type */
	GF_ISOM_BRAND_MSDH = GF_4CC('m','s','d','h'),
	/* Media Segment conforming to the Indexed Media Segment format type */
	GF_ISOM_BRAND_MSIX = GF_4CC('m','s','i','x'),
	/* Representation Index Segment used to index MPEG-2 TS based Media Segments */
	GF_ISOM_BRAND_RISX = GF_4CC('r','i','s','x'),
	/* last Media Segment indicator for ISO base media file format */
	GF_ISOM_BRAND_LMSG = GF_4CC('l','m','s','g'),
	/* Single Index Segment used to index MPEG-2 TS based Media Segments */
	GF_ISOM_BRAND_SISX = GF_4CC('s','i','s','x'),
	/* Subsegment Index Segment used to index MPEG-2 TS based Media Segments */
	GF_ISOM_BRAND_SSSS = GF_4CC('s','s','s','s'),

	/* CMAF brand */
	GF_ISOM_BRAND_CMFC = GF_4CC('c','m','f','c'),
	/* CMAF brand with neg ctts */
	GF_ISOM_BRAND_CMF2 = GF_4CC('c','m','f','2'),

	/* from ismacryp.c */
	/* OMA DCF DRM Format 2.0 (OMA-TS-DRM-DCF-V2_0-20060303-A) */
	GF_ISOM_BRAND_ODCF = GF_4CC('o','d','c','f'),
	/* OMA PDCF DRM Format 2.1 (OMA-TS-DRM-DCF-V2_1-20070724-C) */
	GF_ISOM_BRAND_OPF2 = GF_4CC('o','p','f','2'),

	/* compressed brand*/
	GF_ISOM_BRAND_COMP  = GF_4CC( 'c', 'o', 'm', 'p' ),
	GF_ISOM_BRAND_ISOC  = GF_4CC( 'i', 's', 'o', 'C' ),

};

/*! sample roll information type*/
typedef enum
{
	/*! no roll info associated*/
	GF_ISOM_SAMPLE_ROLL_NONE=0,
	/*! roll info describes a roll operation*/
	GF_ISOM_SAMPLE_ROLL,
	/*! roll info describes an audio preroll*/
	GF_ISOM_SAMPLE_PREROLL,
	/*! roll info describes audio preroll but is not set for this sample*/
	GF_ISOM_SAMPLE_PREROLL_NONE
} GF_ISOSampleRollType;

#ifndef GPAC_DISABLE_ISOM

#include <gpac/mpeg4_odf.h>

/*! isomedia file*/
typedef struct __tag_isom GF_ISOFile;

/*! a track ID value - just a 32 bit value but typedefed for API safety*/
typedef u32 GF_ISOTrackID;

/*! @} */

/*!
\addtogroup isosample_grp ISO Sample
\ingroup iso_grp

Media sample for ISOBMFF API.
@{
*/

/*! Random Access Point flag*/
typedef enum {
	/*! redundant sync shadow - only set when reading sample*/
	RAP_REDUNDANT = -1,
	/*! not rap*/
	RAP_NO = 0,
	/*! sync point (IDR)*/
	RAP = 1,
	/*! sync point (IDR)*/
	SAP_TYPE_1 = 1,
	/*! sync point (IDR)*/
	SAP_TYPE_2 = 2,
	/*! RAP, OpenGOP*/
	SAP_TYPE_3 = 3,
	/*! RAP, roll info (GDR or audio preroll)*/
	SAP_TYPE_4 = 4,
} GF_ISOSAPType;

/*! media sample object*/
typedef struct
{
	/*! data size*/
	u32 dataLength;
	/*! data with padding if requested*/
	u8 *data;
	/*! decoding time*/
	u64 DTS;
	/*! relative offset for composition if needed*/
	s32 CTS_Offset;
	/*! SAP type*/
	GF_ISOSAPType IsRAP;
	/*! allocated data size - used only when using static sample in \ref gf_isom_get_sample_ex*/
	u32 alloc_size;
	
	/*! number of packed samples in this sample. If 0 or 1, only 1 sample is present
	only used for constant size and constant duration samples*/
	u32 nb_pack;

	/*! read API only - sample duration (multiply by nb_pack to get full duration)*/
	u32 duration;
} GF_ISOSample;


/*! creates a new empty sample
\return the newly allocated ISO sample*/
GF_ISOSample *gf_isom_sample_new();

/*! delete a sample.
\note The buffer content will be destroyed by default. If you wish to keep the buffer, set dataLength to 0 in the sample before deleting it
the pointer is set to NULL after deletion
\param samp pointer to the target ISO sample
*/
void gf_isom_sample_del(GF_ISOSample **samp);


/*! @} */

/*!
\addtogroup isogen_grp Generic API
\ingroup iso_grp

Generic API functions
@{
*/

/*! Movie file opening modes */
typedef enum
{
	/*! Opens file for dumping: same as read-only but keeps all movie fragments info untouched*/
	GF_ISOM_OPEN_READ_DUMP = 0,
	/*! Opens a file in READ ONLY mode*/
	GF_ISOM_OPEN_READ,
	/*! Opens a file in WRITE ONLY mode. Media Data is captured on the fly and storage mode is always flat (moov at end).
	In this mode, the editing functions are disabled.*/
	GF_ISOM_OPEN_WRITE,
	/*! Opens an existing file in EDIT mode*/
	GF_ISOM_OPEN_EDIT,
	/*! Creates a new file in EDIT mode*/
	GF_ISOM_WRITE_EDIT,
	/*! Opens an existing file and keep fragment information*/
	GF_ISOM_OPEN_KEEP_FRAGMENTS,
	/*! Opens an existing file in READ ONLY mode but enables most of the file edit functions except fragmentation
	Samples may be added to the file in this mode, they will be stored in memory
	*/
	GF_ISOM_OPEN_READ_EDIT,
} GF_ISOOpenMode;

/*! indicates if target file is an IsoMedia file
\param fileName the target local file name or path to probe, gmem:// or gfio:// resource
\return 1 if it is a non-special file, 2 if an init segment, 3 if a media segment, 0 otherwise
*/
u32 gf_isom_probe_file(const char *fileName);

/*! indicates if target file is an IsoMedia file
\param fileName the target local file name or path to probe, gmem:// or gfio:// resource
\param start_range the offset in the file to start probing from
\param end_range the offset in the file at which probing shall stop
\return 1 if it is a non-special file, 2 if an init segment, 3 if a media segment, 4 if empty or no file, 0 otherwise
*/
u32 gf_isom_probe_file_range(const char *fileName, u64 start_range, u64 end_range);

/*! indicates if target file is an IsoMedia file
\param inBuf the buffer to probe
\param inSize the sizeo of the buffer to probe
\returns 1 if it is a non-special file, 2 if an init segment, 3 if a media segment, 0 otherwise (non recognized or too short)
*/
u32 gf_isom_probe_data(const u8*inBuf, u32 inSize);

/*! opens an isoMedia File.
\param fileName name of the file to open, , gmem:// or gfio:// resource. The special name "_gpac_isobmff_redirect" is used to indicate that segment shall be written to a memory buffer passed to callback function set through \ref gf_isom_set_write_callback. SHALL not be NULL.
\param OpenMode file opening mode
\param tmp_dir for the 2 edit modes only, specifies a location for temp file. If NULL, the library will use the default libgpac temporary file management schemes.
\return the created ISO file if no error
*/
GF_ISOFile *gf_isom_open(const char *fileName, GF_ISOOpenMode OpenMode, const char *tmp_dir);

/*! closes the file, write it if new/edited or if pending fragment
\param isom_file the target ISO file
\return error if any
*/
GF_Err gf_isom_close(GF_ISOFile *isom_file);

/*! deletes the movie without saving it
\param isom_file the target ISO file
*/
void gf_isom_delete(GF_ISOFile *isom_file);

/*! gets the last fatal error that occured in the file
ANY FUNCTION OF THIS API WON'T BE PROCESSED IF THE FILE HAS AN ERROR
\note Some function may return an error while the movie has no error
the last error is a FatalError, and is not always set if a bad
param is specified...
\param isom_file the target ISO file
\return error if any
*/
GF_Err gf_isom_last_error(GF_ISOFile *isom_file);

/*! gets the mode of an open file
\param isom_file the target ISO file
\return open mode of the file
*/
u8 gf_isom_get_mode(GF_ISOFile *isom_file);

/*! checks if file is J2K image
\param isom_file the target ISO file
\return GF_TRUE if file is a j2k image, GF_FALSE otherwise
*/
Bool gf_isom_is_JPEG2000(GF_ISOFile *isom_file);


/*! checks if a given four character code matches a known video handler type (vide, auxv, pict, ...)
\param mtype the four character code to check
\return GF_TRUE if the type is a video media type*/
Bool gf_isom_is_video_handler_type(u32 mtype);

/*! gets number of implemented boxes in  (including the internal unknown box wrapper).
\note There can be several times the same type returned due to variation of the box (versions or flags)
\return number of implemented boxes
*/
u32 gf_isom_get_num_supported_boxes();

/*! gets four character code of box given its index. Index 0 is GPAC internal unknown box handler
\param idx 0-based index of the box
\return four character code of the box
*/
u32 gf_isom_get_supported_box_type(u32 idx);

#ifndef GPAC_DISABLE_ISOM_DUMP

/*! prints default box syntax of box given its index. Index 0 is GPAC internal unknown box handler
\param idx 0-based index of the box
\param trace the file object to dump to
\return error if any
*/
GF_Err gf_isom_dump_supported_box(u32 idx, FILE * trace);

#endif

/*! @} */

/*!
\addtogroup isoread_grp ISOBMFF Reading
\ingroup iso_grp

ISOBMF file reading
@{
*/

/*! checks if moov box is before any mdat box
\param isom_file the target ISO file
\return GF_TRUE if if moov box is before any mdat box, GF_FALSE otherwise
*/
Bool gf_isom_moov_first(GF_ISOFile *isom_file);

/*! when reading a file, indicates that file data is missing the indicated bytes
\param isom_file the target ISO file
\param byte_offset number of bytes not present at the beginning of the file
\return error if any
*/
GF_Err gf_isom_set_byte_offset(GF_ISOFile *isom_file, s64 byte_offset);


/*! opens a movie that can be uncomplete in READ_ONLY mode
to use for http streaming & co

start_range and end_range restricts the media byte range in the URL (used by DASH)
if 0 or end_range<=start_range, the entire URL is used when parsing

If the url indicates a gfio or gmem resource, the file can be played from the associated underlying buffer. For gmem, you must call \ref gf_isom_refresh_fragmented and gf_isom_set_removed_bytes whenever the underlying buffer is modified.

\param fileName the name of the local file or cache to open, gmem:// or gfio://
\param start_range only loads starting from indicated byte range
\param end_range loading stops at indicated byte range
\param enable_frag_templates loads fragment and segment boundaries in an internal table
\param isom_file pointer set to the opened file if success
\param BytesMissing is set to the predicted number of bytes missing for the file to be loaded
Note that if the file is not optimized for streaming, this number is not accurate
If the movie is successfully loaded (isom_file non-NULL), BytesMissing is zero
\return error if any
*/
GF_Err gf_isom_open_progressive(const char *fileName, u64 start_range, u64 end_range, Bool enable_frag_templates, GF_ISOFile **isom_file, u64 *BytesMissing);


/*! same as  \ref gf_isom_open_progressive but allows fetching the incomplete box type

\param fileName the name of the local file or cache to open
\param start_range only loads starting from indicated byte range
\param end_range loading stops at indicated byte range
\param enable_frag_templates loads fragment and segment boundaries in an internal table
\param isom_file pointer set to the opened file if success
\param BytesMissing is set to the predicted number of bytes missing for the file to be loaded
\param topBoxType is set to the 4CC of the incomplete top-level box found - may be NULL
Note that if the file is not optimized for streaming, this number is not accurate
If the movie is successfully loaded (isom_file non-NULL), BytesMissing is zero
\return error if any
*/
GF_Err gf_isom_open_progressive_ex(const char *fileName, u64 start_range, u64 end_range, Bool enable_frag_templates, GF_ISOFile **isom_file, u64 *BytesMissing, u32 *topBoxType);

/*! retrieves number of bytes missing.
if requesting a sample fails with error GF_ISOM_INCOMPLETE_FILE, use this function
to get the number of bytes missing to retrieve the sample
\param isom_file the target ISO file
\param trackNumber the desired track to query
\return the number of bytes missing to fetch the sample
*/
u64 gf_isom_get_missing_bytes(GF_ISOFile *isom_file, u32 trackNumber);

/*! checks if a file has movie info (moov box with tracks & dynamic media). Some files may just use
the base IsoMedia structure without "moov" container
\param isom_file the target ISO file
\return GF_TRUE if the file has a movie, GF_FALSE otherwise
*/
Bool gf_isom_has_movie(GF_ISOFile *isom_file);

/*! gets number of tracks
\param isom_file the target ISO file
\return the number of tracks in the movie, or -1 if error*/
u32 gf_isom_get_track_count(GF_ISOFile *isom_file);

/*! gets the movie timescale
\param isom_file the target ISO file
\return the timescale of the movie, 0 if error*/
u32 gf_isom_get_timescale(GF_ISOFile *isom_file);

/*! gets the movie duration computed based on media samples and edit lists
\param isom_file the target ISO file
\return the computed duration of the movie, 0 if error*/
u64 gf_isom_get_duration(GF_ISOFile *isom_file);

/*! gets the original movie duration as written in the file, regardless of the media data
\param isom_file the target ISO file
\return the duration of the movie*/
u64 gf_isom_get_original_duration(GF_ISOFile *isom_file);

/*! time offset since UNIX EPOC for MP4/QT/MJ2K files*/
#define GF_ISOM_MAC_TIME_OFFSET 2082844800

/*! gets the creation info of the movie
\param isom_file the target ISO file
\param creationTime set to the creation time of the movie
\param modificationTime set to the modification time of the movie
\return error if any
*/
GF_Err gf_isom_get_creation_time(GF_ISOFile *isom_file, u64 *creationTime, u64 *modificationTime);


/*! gets the creation info of the movie
\param isom_file the target ISO file
\param trackNumber the target track
\param creationTime set to the creation time of the movie
\param modificationTime set to the modification time of the movie
\return error if any
*/
GF_Err gf_isom_get_track_creation_time(GF_ISOFile *isom_file, u32 trackNumber, u64 *creationTime, u64 *modificationTime);

/*! gets the ID of a track
\param isom_file the target ISO file
\param trackNumber the target track
\return the trackID of the track, or 0 if error*/
GF_ISOTrackID gf_isom_get_track_id(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets a track number by its ID
\param isom_file the target ISO file
\param trackID the target track ID
\return the track number of the track, or 0 if error*/
u32 gf_isom_get_track_by_id(GF_ISOFile *isom_file, GF_ISOTrackID trackID);

/*! gets the track original ID (before cloning from another file)
\param isom_file the target ISO file
\param trackNumber the target track
\return the original trackID of the track, or 0 if error*/
GF_ISOTrackID gf_isom_get_track_original_id(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the enable flag of a track
\param isom_file the target ISO file
\param trackNumber the target track
\return 0: not enabled, 1: enabled, 2: error
*/
u8 gf_isom_is_track_enabled(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets track flags
\param isom_file the target ISO file
\param trackNumber the target track
\return the track flags
*/
u32 gf_isom_get_track_flags(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the track duration - this will try to fix any discrepencies between media duration+edit lists vs track duration
\param isom_file the target ISO file
\param trackNumber the target track
\return the track duration in movie timescale, or 0 if error*/
u64 gf_isom_get_track_duration(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the unmodified track duration - mus be called before any call to \ref gf_isom_get_track_duration
\param isom_file the target ISO file
\param trackNumber the target track
\return the track duration in movie timescale, or 0 if error*/
u64 gf_isom_get_track_duration_orig(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the media type (audio, video, etc) of a track
\param isom_file the target ISO file
\param trackNumber the target track
\return the media type four character code of the media*/
u32 gf_isom_get_media_type(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets media subtype of a sample description entry
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\return the media type four character code of the given sample description*/
u32 gf_isom_get_media_subtype(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets the composition time (media time) given the absolute time in the Movie
\param isom_file the target ISO file
\param trackNumber the target track
\param movieTime desired time in movie timescale
\param mediaTime is set to 0 if the media is not playing at that time (empty time segment)
\return error if any*/
GF_Err gf_isom_get_media_time(GF_ISOFile *isom_file, u32 trackNumber, u32 movieTime, u64 *mediaTime);

/*! gets the number of sample descriptions in the media - a media can have several stream descriptions (eg different codec configurations, different protetcions, different visual sizes).
\param isom_file the target ISO file
\param trackNumber the target track
\return the number of sample descriptions
*/
u32 gf_isom_get_sample_description_count(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the stream description index for a given time in media time
\param isom_file the target ISO file
\param trackNumber the target track
\param for_time the desired time in media timescale
\return the sample description index, or 0 if error or if empty*/
u32 gf_isom_get_sample_description_index(GF_ISOFile *isom_file, u32 trackNumber, u64 for_time);

/*! checks if a sample stream description is self-contained (samples located in the file)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\return GF_TRUE if samples referring to the given stream description are present in the file, GF_FALSE otherwise*/
Bool gf_isom_is_self_contained(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets the media duration (without edit) based on sample table
\param isom_file the target ISO file
\param trackNumber the target track
\return the media duration, 0 if no samples
*/
u64 gf_isom_get_media_duration(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the original media duration (without edit) as indicated in the file
\param isom_file the target ISO file
\param trackNumber the target track
\return the media duration
*/
u64 gf_isom_get_media_original_duration(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the media timescale
\param isom_file the target ISO file
\param trackNumber the target track
\return the timeScale of the media
*/
u32 gf_isom_get_media_timescale(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets media chunking information for non-fragmented files
\param isom_file the target ISO file
\param trackNumber the target track
\param dur_min set to minimum chunk duration in media timescale (optional, can be NULL)
\param dur_avg set to average chunk duration in media timescale (optional, can be NULL)
\param dur_max set to maximum chunk duration in media timescale (optional, can be NULL)
\param size_min set to smallest chunk size in bytes (optional, can be NULL)
\param size_avg set to average chunk size in bytes (optional, can be NULL)
\param size_max set to largest chunk size in bytes (optional, can be NULL)
\return error if any
*/
GF_Err gf_isom_get_chunks_infos(GF_ISOFile *isom_file, u32 trackNumber, u32 *dur_min, u32 *dur_avg, u32 *dur_max, u32 *size_min, u32 *size_avg, u32 *size_max);

/*! gets the handler name. The outName must be:
\param isom_file the target ISO file
\param trackNumber the target track
\param outName set to the handler name (must be non NULL)
\return error if any
*/
GF_Err gf_isom_get_handler_name(GF_ISOFile *isom_file, u32 trackNumber, const char **outName);

/*! checks if the data reference for the given track and sample description is valid and supported
(a data Reference allows to construct a file without integrating the media data, however this library only handles local storage external data references)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\return error if any
*/
GF_Err gf_isom_check_data_reference(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets the location of the data. If both outURL and outURN are set to NULL, the data is in this file
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\param outURL set to URL value of the data reference
\param outURN set to URN value of the data reference
\return error if any
*/
GF_Err gf_isom_get_data_reference(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, const char **outURL, const char **outURN);

/*! gets the number of samples in a track
\param isom_file the target ISO file
\param trackNumber the target track
\return the number of samples, or 0 if error*/
u32 gf_isom_get_sample_count(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the constant sample size for samples of a track
\param isom_file the target ISO file
\param trackNumber the target track
\return constant size of samples or 0 if size not constant
*/
u32 gf_isom_get_constant_sample_size(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the constant sample duration for samples of a track
\param isom_file the target ISO file
\param trackNumber the target track
\return constant duration of samples, or 0 if duration not constant*/
u32 gf_isom_get_constant_sample_duration(GF_ISOFile *isom_file, u32 trackNumber);

/*! sets max audio sample packing in a single ISOSample.
This is mostly used when processing raw audio tracks, for which extracting samples per samples would be too time consuming
\param isom_file the target ISO file
\param trackNumber the target track
\param pack_num_samples the target number of samples to pack in one ISOSample
\return GF_TRUE if packing was successful, GF_FALSE otherwise (non constant size and non constant duration)
*/
Bool gf_isom_enable_raw_pack(GF_ISOFile *isom_file, u32 trackNumber, u32 pack_num_samples);

/*! gets the total media data size of a track (whether in the file or not)
\param isom_file the target ISO file
\param trackNumber the target track
\return total amount of media bytes in track
*/
u64 gf_isom_get_media_data_size(GF_ISOFile *isom_file, u32 trackNumber);

/*! sets sample padding bytes when reading a sample
It may be desired to fetch samples with a bigger allocated buffer than their real size, in case the decoder
reads more data than available. This sets the amount of extra bytes to allocate when reading samples from this track
\note The dataLength of the sample does NOT include padding
\param isom_file the target ISO file
\param trackNumber the target track
\param padding_bytes the amount of bytes to add at the end of a sample data buffer
\return error if any
*/
GF_Err gf_isom_set_sample_padding(GF_ISOFile *isom_file, u32 trackNumber, u32 padding_bytes);

/*! fetches a sample from a track. The sample must be destroyed using \ref gf_isom_sample_del
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\param sampleDescriptionIndex set to the sample description index corresponding to this sample
\return the ISO sample or NULL if not found or end of stream  or incomplete file. Use \ref gf_isom_last_error to check the error code
*/
GF_ISOSample *gf_isom_get_sample(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex);

/*! fetches a sample from a track without allocating a new sample.
This function is the same as \ref gf_isom_get_sample except that it fills in the static_sample passed as argument, potentially reallocating buffers

\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\param sampleDescriptionIndex set to the sample description index corresponding to this sample
\param static_sample a caller-allocated ISO sample to use as the returned sample
\param data_offset set to data offset in file / current bitstream - may be NULL
\return the ISO sample or NULL if not found or end of stream or incomplete file. Use \ref gf_isom_last_error to check the error code
\note If the function returns NULL, the static_sample and its associated data are NOT destroyed
*/
GF_ISOSample *gf_isom_get_sample_ex(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex, GF_ISOSample *static_sample, u64 *data_offset);

/*! gets sample information. This is the same as \ref gf_isom_get_sample but doesn't fetch media data

\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\param sampleDescriptionIndex set to the sample description index corresponding to this sample (optional, can be NULL)
\param data_offset set to the sample start offset in file (optional, can be NULL)
\note When both sampleDescriptionIndex and data_offset are NULL, only DTS, CTS_Offset and RAP indications are retrieved (faster)
\return the ISO sample without data or NULL if not found or end of stream  or incomplete file. Use \ref gf_isom_last_error to check the error code
*/
GF_ISOSample *gf_isom_get_sample_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex, u64 *data_offset);

/*! gets sample information with a user-allocated sample. This is the same as \ref gf_isom_get_sample_info but uses a static allocated sample as input
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\param sampleDescriptionIndex set to the sample description index corresponding to this sample (optional, can be NULL)
\param data_offset set to the sample start offset in file (optional, can be NULL)
\note When both sampleDescriptionIndex and data_offset are NULL, only DTS, CTS_Offset and RAP indications are retrieved (faster)
\param static_sample a caller-allocated ISO sample to use as the returned sample
\return the ISO sample without data or NULL if not found or end of stream  or incomplete file. Use \ref gf_isom_last_error to check the error code
\note If the function returns NULL, the static_sample and its associated data if any are NOT destroyed
*/
GF_ISOSample *gf_isom_get_sample_info_ex(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex, u64 *data_offset, GF_ISOSample *static_sample);

/*! get sample decoding time
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\return decoding time in media timescale
*/
u64 gf_isom_get_sample_dts(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber);

/*! gets sample duration
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\return sample duration in media timescale*/
u32 gf_isom_get_sample_duration(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber);

/*! gets sample size
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\return sample size in bytes*/
u32 gf_isom_get_sample_size(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber);

/*! gets maximum sample size in track
\param isom_file the target ISO file
\param trackNumber the target track
\return max size of any sample in track*/
u32 gf_isom_get_max_sample_size(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets average sample size in a track
\param isom_file the target ISO file
\param trackNumber the target track
\return average size of sample in track
*/
u32 gf_isom_get_avg_sample_size(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets maximum sample duration in track
\param isom_file the target ISO file
\param trackNumber the target track
\return max sample delta in media timescale
*/
u32 gf_isom_get_max_sample_delta(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets average sample duration in track, i.e. the sample delta occuring most often
\param isom_file the target ISO file
\param trackNumber the target track
\return average  sample delta in media timescale
*/
u32 gf_isom_get_avg_sample_delta(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets max sample CTS offset (CTS-DTS) in track
\param isom_file the target ISO file
\param trackNumber the target track
\return max sample cts offset in media timescale*/
u32 gf_isom_get_max_sample_cts_offset(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets sample sync flag. This does not check other sample groups ('rap ' or 'sap ')
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\return GF_TRUE if sample is sync, GF_FALSE otherwise*/
Bool gf_isom_get_sample_sync(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber);

/*! gets sample dependency flags - see ISO/IEC 14496-12 and \ref gf_filter_pck_set_dependency_flags
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\param is_leading set to 1 if sample is a leading picture
\param dependsOn set to the depends_on flag
\param dependedOn set to the depended_on flag
\param redundant set to the redundant flag
\return error if any
*/
GF_Err gf_isom_get_sample_flags(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 *is_leading, u32 *dependsOn, u32 *dependedOn, u32 *redundant);

/*! gets a sample given a desired decoding time and set the sampleDescriptionIndex of this sample

\warning The sample may not be sync even though the sync was requested (depends on the media and the editList)
the SampleNum is optional. If non-NULL, will contain the sampleNumber

\param isom_file the target ISO file
\param trackNumber the target track
\param desiredTime the desired time in media timescale
\param sampleDescriptionIndex set to the sample description index corresponding to this sample (optional, can be NULL)
\param SearchMode the search direction mode
\param sample set to the fetched sample if any. If NULL, sample is not fetched (optional, can be NULL)
\param sample_number set to the fetched sample number if any, set to 0 otherwise (optional, can be NULL)
\param data_offset set to the sample start offset in file (optional, can be NULL)

\return GF_EOS if the desired time exceeds the media duration or error if any
*/
GF_Err gf_isom_get_sample_for_media_time(GF_ISOFile *isom_file, u32 trackNumber, u64 desiredTime, u32 *sampleDescriptionIndex, GF_ISOSearchMode SearchMode, GF_ISOSample **sample, u32 *sample_number, u64 *data_offset);

/*! gets sample number for a given decode time
\param isom_file the target ISO file
\param trackNumber the target track
\param dts the desired decode time in media timescale
\return the sample number or 0 if not found
*/
u32 gf_isom_get_sample_from_dts(GF_ISOFile *isom_file, u32 trackNumber, u64 dts);


/*! enumerates the type and references IDs of a track
\param isom_file the target ISO file
\param trackNumber the target track
\param idx 0-based index of reference to query
\param referenceType set to the four character code of the reference entry
\param referenceCount set to the number of track ID references for  the reference entry
\return list of track IDs, NULL if no references - do NOT modify !*/
const GF_ISOTrackID *gf_isom_enum_track_references(GF_ISOFile *isom_file, u32 trackNumber, u32 idx, u32 *referenceType, u32 *referenceCount);

/*! get the number of track references of a track for a given ReferenceType
\param isom_file the target ISO file
\param trackNumber the target track
\param referenceType the four character code of the reference to query
\return -1 if error or the number of references*/
s32 gf_isom_get_reference_count(GF_ISOFile *isom_file, u32 trackNumber, u32 referenceType);

/*! get the referenced track number for a track and a given ReferenceType and Index
\param isom_file the target ISO file
\param trackNumber the target track
\param referenceType the four character code of the reference to query
\param referenceIndex the 1-based index of the reference to query (see \ref gf_isom_get_reference_count)
\param refTrack set to the track number of the referenced track
\return error if any
*/
GF_Err gf_isom_get_reference(GF_ISOFile *isom_file, u32 trackNumber, u32 referenceType, u32 referenceIndex, u32 *refTrack);

/*! get the referenced track ID for a track and a given ReferenceType and Index
\param isom_file the target ISO file
\param trackNumber the target track
\param referenceType the four character code of the reference to query
\param referenceIndex the 1-based index of the reference to query (see \ref gf_isom_get_reference_count)
\param refTrackID set to the track ID of the referenced track
\return error if any
*/
GF_Err gf_isom_get_reference_ID(GF_ISOFile *isom_file, u32 trackNumber, u32 referenceType, u32 referenceIndex, GF_ISOTrackID *refTrackID);

/*! checks if a track has a reference of given type to another track
\param isom_file the target ISO file
\param trackNumber the target track
\param referenceType the four character code of the reference to query
\param refTrackID set to the track number of the referenced track
\return the reference index if the given track has a reference of type referenceType to refTreckID, 0 otherwise*/
u32 gf_isom_has_track_reference(GF_ISOFile *isom_file, u32 trackNumber, u32 referenceType, GF_ISOTrackID refTrackID);

/*! checks if a track is referenced by another track wuth the given reference type
\param isom_file the target ISO file
\param trackNumber the target track
\param referenceType the four character code of the reference to query
\return the track number of the first track  referencing the target track, 0 otherwise*/
u32 gf_isom_is_track_referenced(GF_ISOFile *isom_file, u32 trackNumber, u32 referenceType);

/*! fetches a sample for a given movie time, handling possible track edit lists.

if no sample is playing, an empty sample is returned with no data and a DTS set to MovieTime when searching in sync modes
if no sample is playing, the closest sample in the edit time-line is returned when searching in regular modes

\warning The sample may not be sync even though the sync was requested (depends on the media and the editList)

\note This function will handle re-timestamping the sample according to the mapping  of the media time-line
on the track time-line. The sample TSs (DTS / CTS offset) are expressed in MEDIA TIME SCALE
(to match the media stream TS resolution as indicated in media header / SLConfig)


\param isom_file the target ISO file
\param trackNumber the target track
\param movieTime the desired movie time in media timescale
\param sampleDescriptionIndex set to the sample description index corresponding to this sample (optional, can be NULL)
\param SearchMode the search direction mode
\param sample set to the fetched sample if any. If NULL, sample is not fetched (optional, can be NULL)
\param sample_number set to the fetched sample number if any, set to 0 otherwise (optional, can be NULL)
\param data_offset set to the sample start offset in file (optional, can be NULL)
\return error if any
*/
GF_Err gf_isom_get_sample_for_movie_time(GF_ISOFile *isom_file, u32 trackNumber, u64 movieTime, u32 *sampleDescriptionIndex, GF_ISOSearchMode SearchMode, GF_ISOSample **sample, u32 *sample_number, u64 *data_offset);

/*! gets edit list type
\param isom_file the target ISO file
\param trackNumber the target track
\param mediaOffset set to the media offset of the edit for time-shifting edits
\return GF_TRUE if complex edit list, GF_FALSE if no edit list or time-shifting only edit list, in which case mediaOffset is set to the CTS of the first sample to present at presentation time 0
A negative value implies that the samples with CTS between 0 and mediaOffset should not be presented (skip)
A positive value value implies that there is nothing to present between 0 and CTS (hold)
*/
Bool gf_isom_get_edit_list_type(GF_ISOFile *isom_file, u32 trackNumber, s64 *mediaOffset);

/*! gets the number of edits in an edit list
\param isom_file the target ISO file
\param trackNumber the target track
\return number of edits
*/
u32 gf_isom_get_edits_count(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the desired edit information
\param isom_file the target ISO file
\param trackNumber the target track
\param EditIndex index of the edit to query (1-based index)
\param EditTime set to the edit time in movie timescale
\param SegmentDuration set to the edit duration in movie timescale
\param MediaTime set to the edit media start time in media timescale
\param EditMode set to the mode of the edit
\return error if any
*/
GF_Err gf_isom_get_edit(GF_ISOFile *isom_file, u32 trackNumber, u32 EditIndex, u64 *EditTime, u64 *SegmentDuration, u64 *MediaTime, GF_ISOEditType *EditMode);

/*! gets the number of languages for the copyright
\param isom_file the target ISO file
\return number of languages, 0 if no copyright*/
u32 gf_isom_get_copyright_count(GF_ISOFile *isom_file);

/*! gets a copyright and its language code
\param isom_file the target ISO file
\param Index the 1-based index of the copyright notice to query
\param threeCharCodes set to the copyright language code
\param notice set to the copyright notice
\return error if any
*/
GF_Err gf_isom_get_copyright(GF_ISOFile *isom_file, u32 Index, const char **threeCharCodes, const char **notice);

/*! gets the number of chapter for movie or track (chapters can be assigned to tracks or to movies)
\param isom_file the target ISO file
\param trackNumber the target track to queryy. If 0, looks for chapter at the movie level
\return number of chapters
*/
u32 gf_isom_get_chapter_count(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the given chapter time and name for a movie or track
\param isom_file the target ISO file
\param trackNumber the target track to queryy. If 0, looks for chapter at the movie level
\param Index the index of the ckhapter to queryy
\param chapter_time set to chapter start time in milliseconds (optional, may be NULL)
\param name set to chapter name (optional, may be NULL)
\return error if any
*/
GF_Err gf_isom_get_chapter(GF_ISOFile *isom_file, u32 trackNumber, u32 Index, u64 *chapter_time, const char **name);

/*! checks if a media has sync points
\param isom_file the target ISO file
\param trackNumber the target track
\return 0 if the media has no sync point info (eg, all samples are RAPs), 1 if the media has sync points (eg some samples are RAPs),  2 if the media has empty sync point info (no samples are RAPs - this will likely only happen
			in scalable context)
*/
u8 gf_isom_has_sync_points(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the number of sync points in a media
\param isom_file the target ISO file
\param trackNumber the target track
\return number of sync points*/
u32 gf_isom_get_sync_point_count(GF_ISOFile *isom_file, u32 trackNumber);

/*! checks if a media track hhas composition time offset
\param isom_file the target ISO file
\param trackNumber the target track
\return 1 if the track uses unsigned compositionTime offsets (B-frames or similar), 2 if the track uses signed compositionTime offsets (B-frames or similar), 0 if the track does not use compositionTime offsets (CTS == DTS)
*/
u32 gf_isom_has_time_offset(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets cts to dts shift value if defined.
This shift is defined only in cases of negative CTS offset (ctts version 1) and not always present in files!
Adding shift to CTS guarantees that the shifted CTS is always greater than the DTS for any sample
\param isom_file the target ISO file
\param trackNumber the target track
\return the shift from composition time to decode time for that track if indicated, or 0 if not found
*/
s64 gf_isom_get_cts_to_dts_shift(GF_ISOFile *isom_file, u32 trackNumber);

/*! checks if a track has sync shadow samples (RAP samples replacing non RAP ones)
\param isom_file the target ISO file
\param trackNumber the target track
\return GF_TRUE if the track has sync shadow samples*/
Bool gf_isom_has_sync_shadows(GF_ISOFile *isom_file, u32 trackNumber);

/*! checks if a track has sample dependencoes indicated
\param isom_file the target ISO file
\param trackNumber the target track
\return GF_TRUE if the track has sample dep indications*/
Bool gf_isom_has_sample_dependency(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets a rough estimation of file size. This only works for completely self-contained files and without fragmentation
for the current time
\param isom_file the target ISO file
\return estimated file size in bytes*/
u64 gf_isom_estimate_size(GF_ISOFile *isom_file);

/*! gets next alternate group ID available
\param isom_file the target ISO file
\return next available ID for alternate groups
*/
u32 gf_isom_get_next_alternate_group_id(GF_ISOFile *isom_file);


/*! gets file name of an opened ISO file
\param isom_file the target ISO file
\return the file name*/
const char *gf_isom_get_filename(GF_ISOFile *isom_file);


/*! gets file brand information
The brand is used to
- differenciate MP4, MJPEG2000 and QT while indicating compatibilities
- identify tools that shall be supported for correct parsing of the file

The function will set brand, minorVersion and AlternateBrandsCount to 0 if no brand indication is found in the file

\param isom_file the target ISO file
\param brand set to the four character code of the brand
\param minorVersion set to an informative integer for the minor version of the major brand (optional, can be NULL)
\param AlternateBrandsCount set to the number of compatible brands (optional, can be NULL).
\return error if any*/
GF_Err gf_isom_get_brand_info(GF_ISOFile *isom_file, u32 *brand, u32 *minorVersion, u32 *AlternateBrandsCount);

/*! gets an alternate brand indication
\note the Major brand should always be indicated in the alternate brands
\param isom_file the target ISO file
\param BrandIndex 1-based index of alternate brand to query (cf \ref gf_isom_get_brand_info)
\param brand set to the four character code of the brand
\return error if any*/
GF_Err gf_isom_get_alternate_brand(GF_ISOFile *isom_file, u32 BrandIndex, u32 *brand);

/*! gets the internal list of brands
\param isom_file the target ISO file
\return the internal list of brands. DO NOT MODIFY the content
*/
const u32 *gf_isom_get_brands(GF_ISOFile *isom_file);

/*! gets the number of padding bits at the end of a given sample if any
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the desired sample number (1-based index)
\param NbBits set to the number of padded bits at the end of the sample
\return error if any*/
GF_Err gf_isom_get_sample_padding_bits(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u8 *NbBits);

/*! checks if a track samples use padding bits or not
\param isom_file the target ISO file
\param trackNumber the target track
\return GF_TRUE if samples have padding bits information, GF_FALSE otherwise*/
Bool gf_isom_has_padding_bits(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets information of a visual track for a given sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\param Width set to the width of the sample description in pixels
\param Height set to the height of the sample description in pixels
\return error if any*/
GF_Err gf_isom_get_visual_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *Width, u32 *Height);

/*! gets bit depth of a sample description of a visual track (for uncompressed media usually)
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param bitDepth the bit depth of each pixel (eg 24 for RGB, 32 for RGBA)
\return error if any
*/
GF_Err gf_isom_get_visual_bit_depth(GF_ISOFile* isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u16 *bitDepth);

/*! gets information of an audio track for a given sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\param SampleRate set to the audio sample rate of the sample description
\param Channels set to the audio channel count of the sample description
\param bitsPerSample set to the audio bits per sample for raw audio of the sample description
\return error if any*/
GF_Err gf_isom_get_audio_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *SampleRate, u32 *Channels, u32 *bitsPerSample);

/*! Audio channel layout description, ISOBMFF style*/
typedef struct
{
	/*! stream structure flags, 1: has channel layout, 2: has objects*/
	u8 stream_structure;
	/*!  order of formats in the stream : 0 unknown, 1: Channels, possibly followed by Objects, 2 Objects, possibly followed by Channels*/
	u8 format_ordering;
	/*! combined channel count of the channel layout and the object count*/
	u8 base_channel_count;

	/*! defined CICP channel layout*/
	u8 definedLayout;
	/*! indicates where the ordering of the audio channels for the definedLayout are specified
	0: as listed for the ChannelConfigurations in ISO/IEC 23091-3
	1: Default order of audio codec specification
	2: Channel ordering #2 of audio codec specification
	3: Channel ordering #3 of audio codec specification
	4: Channel ordering #4 of audio codec specification
	*/
	u8 channel_order_definition;
	/*! indicates if omittedChannelsMap is present*/
	u8 omitted_channels_present;

	/*! number of channels*/
	u32 channels_count;
	struct {
		/*! speaker position*/
		u8 position;
		/*! speaker elevation if position==126*/
		s8 elevation;
		/*! speaker azimuth if position==126*/
		s16 azimuth;
	} layouts[64];
	/*! bit-map of omitted channels using bit positions defined in CICP - only valid if definedLayout is not 0*/
	u64 omittedChannelsMap;
	/*! number of objects in the stream*/
	u8 object_count;
} GF_AudioChannelLayout;

/*! get channel layout info for an audio track, ISOBMFF style
 \param isom_file the target ISO file
 \param trackNumber the target track
 \param sampleDescriptionIndex the target sample description index (1-based)
 \param layout set to the channel/object layout info for this track
 \return GF_NOT_FOUND if not set in file, or other error if any*/
GF_Err gf_isom_get_audio_layout(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_AudioChannelLayout *layout);


/*! gets visual track layout information
\param isom_file the target ISO file
\param trackNumber the target track
\param width set to the width of the track in pixels
\param height set to the height of the track in pixels
\param translation_x set to the horizontal translation of the track in pixels
\param translation_y set to the vertical translation of the track in pixels
\param layer set to the z-order of the track
\return error if any*/
GF_Err gf_isom_get_track_layout_info(GF_ISOFile *isom_file, u32 trackNumber, u32 *width, u32 *height, s32 *translation_x, s32 *translation_y, s16 *layer);

/*! gets matrix of a visual track
\param isom_file the target ISO file
\param trackNumber the target track
\param matrix set to the track matrix - all coord values are expressed as 16.16 fixed point floats
\return error if any*/
GF_Err gf_isom_get_track_matrix(GF_ISOFile *isom_file, u32 trackNumber, u32 matrix[9]);

/*! gets sample (pixel) aspect ratio information of a visual track for a given sample description
The aspect ratio is hSpacing divided by vSpacing
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\param hSpacing horizontal spacing
\param vSpacing vertical spacing
\return error if any*/
GF_Err gf_isom_get_pixel_aspect_ratio(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *hSpacing, u32 *vSpacing);

/*! gets color information of a visual track for a given sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\param colour_type set to the four character code of the colour type mode used (nclx, nclc, prof or ricc currently defined)
\param colour_primaries set to the colour primaries for nclc/nclx as defined in ISO/IEC 23001-8
\param transfer_characteristics set to the colour primaries for nclc/nclx as defined in ISO/IEC 23001-8
\param matrix_coefficients set to the colour primaries for nclc/nclx as defined in ISO/IEC 23001-8
\param full_range_flag set to the colour primaries for nclc as defined in ISO/IEC 23001-8
\return error if any*/
GF_Err gf_isom_get_color_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *colour_type, u16 *colour_primaries, u16 *transfer_characteristics, u16 *matrix_coefficients, Bool *full_range_flag);


/*! gets ICC profile
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\param icc_restricted  set to GF_TRUE of restricted ICC profile, GF_FALSE otherwise
\param icc  set to profile data, NULL if none
\param icc_size  set to profile size, 0 if none
\return error if any*/
GF_Err gf_isom_get_icc_profile(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, Bool *icc_restricted, const u8 **icc, u32 *icc_size);

/*! gets clean aperture (crop window, see ISO/IEC 14496-12) for a sample description
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param cleanApertureWidthN set to nominator of clean aperture horizontal size, may be NULL
\param cleanApertureWidthD set to denominator of clean aperture horizontal size, may be NULL
\param cleanApertureHeightN set to nominator of clean aperture vertical size, may be NULL
\param cleanApertureHeightD set to denominator of clean aperture vertical size, may be NULL
\param horizOffN set to nominator of horizontal offset of clean aperture center minus (width-1)/2 (eg 0 sets center to center of video), may be NULL
\param horizOffD set to denominator of horizontal offset of clean aperture center minus (width-1)/2 (eg 0 sets center to center of video), may be NULL
\param vertOffN set to nominator of vertical offset of clean aperture center minus (height-1)/2 (eg 0 sets center to center of video), may be NULL
\param vertOffD set to denominator of vertical offset of clean aperture center minus (height-1)/2 (eg 0 sets center to center of video), may be NULL
\return error if any
*/
GF_Err gf_isom_get_clean_aperture(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *cleanApertureWidthN, u32 *cleanApertureWidthD, u32 *cleanApertureHeightN, u32 *cleanApertureHeightD, s32 *horizOffN, u32 *horizOffD, s32 *vertOffN, u32 *vertOffD);

/*! content light level info*/
typedef struct  {
	/*! max content ligth level*/
	u16 max_content_light_level;
	/*! max picture average ligth level*/
	u16 max_pic_average_light_level;
} GF_ContentLightLevelInfo;

/*! mastering display colour volume info*/
typedef struct  {
	/*! display primaries*/
	struct {
		u16 x;
		u16 y;
	} display_primaries[3];
	/*! X white point*/
	u16 white_point_x;
	/*! Y white point*/
	u16 white_point_y;
	u32 max_display_mastering_luminance;
	/*! min display mastering luminance*/
	u32 min_display_mastering_luminance;
} GF_MasteringDisplayColourVolumeInfo;

/*! gets master display colour info if any
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\return the mdcv info, or NULL if none or not a valid video track
*/
const GF_MasteringDisplayColourVolumeInfo *gf_isom_get_mastering_display_colour_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets content light level info if any
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\return the clli info, or NULL if none or not a valid video track
*/
const GF_ContentLightLevelInfo *gf_isom_get_content_light_level_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);


/*! gets the media language code of a track
\param isom_file the target ISO file
\param trackNumber the target track
\param lang set to a newly allocated string containg 3 chars (if old files) or longer form (BCP-47) - shall be freed by caller
\return error if any*/
GF_Err gf_isom_get_media_language(GF_ISOFile *isom_file, u32 trackNumber, char **lang);

/*! gets the number of kind (media role) for a given track
\param isom_file the target ISO file
\param trackNumber the target track
\return number of kind defined
*/
u32 gf_isom_get_track_kind_count(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets a given kind (media role) information for a given track
\param isom_file the target ISO file
\param trackNumber the target track
\param index the 1-based index of the kind to retrieve
\param scheme set to the scheme of the kind information - shall be freed by caller
\param value set to the value of the kind information - shall be freed by caller
\return error if any*/
GF_Err gf_isom_get_track_kind(GF_ISOFile *isom_file, u32 trackNumber, u32 index, char **scheme, char **value);

/*! gets the magic number associated with a track. The magic number is usually set by a file muxer, and is not serialized to file
\param isom_file the target ISO file
\param trackNumber the target track
\return the magic number (0 by default)
*/
u64 gf_isom_get_track_magic(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets track group ID of a given track group type for this track
\param isom_file the target ISO file
\param trackNumber the target track
\param track_group_type the target track group type
\return the track group ID, 0 if not found
*/
u32 gf_isom_get_track_group(GF_ISOFile *isom_file, u32 trackNumber, u32 track_group_type);

/*! gets track group ID of a given track group type for this track
\param isom_file the target ISO file
\param trackNumber the target track
\param idx 0-based index of enumeration, incremented by the function if success
\param track_group_type set to the track group type - may be NULL
\param track_group_id set to the track group ID - may be NULL
\return GF_TRUE if success, GF_FALSE otherwise
*/
Bool gf_isom_enum_track_group(GF_ISOFile *isom_file, u32 trackNumber, u32 *idx, u32 *track_group_type, u32 *track_group_id);

/*! checks if file is a single AV file with max one audio, one video, one text and basic od/bifs
\param isom_file the target ISO file
\return GF_TRUE if file is single AV, GF_FALSE otherwise
*/
Bool gf_isom_is_single_av(GF_ISOFile *isom_file);

/*! guesses which specification this file refers to.
\param isom_file the target ISO file
\return possible values are:
	GF_ISOM_BRAND_ISOM: unrecognized std
	GF_ISOM_BRAND_3GP5: 3GP file (max 1 audio, 1 video) without text track
	GF_ISOM_BRAND_3GP6: 3GP file (max 1 audio, 1 video) with text track
	GF_ISOM_BRAND_3GG6: 3GP file multitrack file
	GF_ISOM_BRAND_3G2A: 3GP2 file
	GF_ISOM_BRAND_AVC1: AVC file
	FCC("ISMA"): ISMA file (may overlap with 3GP)
	GF_ISOM_BRAND_MP42: any generic MP4 file (eg with BIFS/OD/MPEG-4 systems stuff)

  for files without movie, returns the file meta handler type
*/
u32 gf_isom_guess_specification(GF_ISOFile *isom_file);


/*! gets the nalu_length_field size used for this sample description if NALU-based (AVC/HEVC/...)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\return number of bytes used to code the NALU size, or 0 if not NALU-based*/
u32 gf_isom_get_nalu_length_field(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets max/average rate information as indicated in ESDS or BTRT boxes. If not found all values are set to 0
if sampleDescriptionIndex is 0, gather for all sample descriptions

\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\param average_bitrate set to the average bitrate in bits per second of the media
\param max_bitrate set to the maximum bitrate in bits per second of the media
\param decode_buffer_size set to the decoder buffer size in bytes of the media
\return error if any*/
GF_Err gf_isom_get_bitrate(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *average_bitrate, u32 *max_bitrate, u32 *decode_buffer_size);


/*! gets the track template of a track. This serializes track box without serializing sample tables nor sample description info
\param isom_file the destination ISO file
\param trackNumber the destination track
\param output will be set to a newly allocated buffer containing the serialized box - caller shall free it
\param output_size will be set to the size of the allocated buffer
\return error if any
*/
GF_Err gf_isom_get_track_template(GF_ISOFile *isom_file, u32 trackNumber, u8 **output, u32 *output_size);

/*! gets the trex template of a track. This serializes trex box
\param isom_file the destination ISO file
\param trackNumber the destination track
\param output will be set to a newly allocated buffer containing the serialized box - caller shall free it
\param output_size will be set to the size of the allocated buffer
\return error if any
*/
GF_Err gf_isom_get_trex_template(GF_ISOFile *isom_file, u32 trackNumber, u8 **output, u32 *output_size);

/*! sets the number of removed bytes form the input bitstream when using gmem:// url
 The number of bytes shall be the total number since the opening of the movie
\param isom_file the target ISO file
\param bytes_removed number of bytes removed
\return error if any
*/
GF_Err gf_isom_set_removed_bytes(GF_ISOFile *isom_file, u64 bytes_removed);

/*! gets the current file offset of the current incomplete top level box not parsed
 This shall be checked to avoid discarding bytes at or after the current top box header
\param isom_file the target ISO file
\param current_top_box_offset set to the offset from file first byte
\return error if any
*/
GF_Err gf_isom_get_current_top_box_offset(GF_ISOFile *isom_file, u64 *current_top_box_offset);

/*! purges the given number of samples, starting from the first sample, from a track of a fragmented file.
 This avoids having sample tables growing in size when reading a fragmented file in pure streaming mode (no seek).
 You should always keep one sample in the track
\param isom_file the target ISO file
\param trackNumber the desired track to purge
\param nb_samples the number of samples to remove
\return error if any
*/
GF_Err gf_isom_purge_samples(GF_ISOFile *isom_file, u32 trackNumber, u32 nb_samples);

#ifndef GPAC_DISABLE_ISOM_DUMP

/*! dumps file structures into XML trace file
\param isom_file the target ISO file
\param trace the file object to dump to
\param skip_init does not dump init segment structure
\param skip_samples does not dump sample tables
\return error if any
*/
GF_Err gf_isom_dump(GF_ISOFile *isom_file, FILE *trace, Bool skip_init, Bool skip_samples);

#endif /*GPAC_DISABLE_ISOM_DUMP*/


/*! gets number of chunks in track
\param isom_file the target ISO file
\param trackNumber the desired track to purge
\return number of chunks in track
*/
u32 gf_isom_get_chunk_count(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets info for a given chunk in track
\param isom_file the target ISO file
\param trackNumber the desired track to purge
\param chunkNumber the 1-based index of the desired chunk
\param chunk_offset set to the chunk offset in bytes from start of file
\param first_sample_num set to the sample number of the first sample in the chunk
\param sample_per_chunk set to number of samples per chunk
\param sample_desc_idx set to sample desc index of samples of this chunk
\param cache_1 updated by function at each call. May be NULL (slower). Must be set to 0 if not querying consecutive chunks
\param cache_2 updated by function at each call. May be NULL (slower). Must be set to 0 if not querying consecutive chunks
\return error if any
*/
GF_Err gf_isom_get_chunk_info(GF_ISOFile *isom_file, u32 trackNumber, u32 chunkNumber, u64 *chunk_offset, u32 *first_sample_num, u32 *sample_per_chunk, u32 *sample_desc_idx, u32 *cache_1, u32 *cache_2);


/*! gets the file offset of the first usable byte of the first mdat box in the file
\param isom_file the target ISO file
\return byte offset
*/
u64 gf_isom_get_first_mdat_start(GF_ISOFile *isom_file);

/*! gets the size of all skip, free and wide boxes present in the file and bytes skipped during parsing (assumes a single file was opened)
\param isom_file the target ISO file
\return size
*/
u64 gf_isom_get_unused_box_bytes(GF_ISOFile *isom_file);

/*! @} */


#ifndef GPAC_DISABLE_ISOM_WRITE

/*!
\addtogroup isowrite_grp ISOBMFF Writing
\ingroup iso_grp

ISOBMF file writing
@{
*/

/*! Movie Storage modes*/
typedef enum
{
	/*! FLAT: the MediaData is stored at the beginning of the file*/
	GF_ISOM_STORE_FLAT = 1,
	/*! STREAMABLE: the MetaData (File Info) is stored at the beginning of the file
	for fast access during download*/
	GF_ISOM_STORE_STREAMABLE,
	/*! INTERLEAVED: Same as STREAMABLE, plus the media data is mixed by chunk  of fixed duration*/
	GF_ISOM_STORE_INTERLEAVED,
	/*! INTERLEAVED +DRIFT: Same as INTERLEAVED, and adds time drift control to avoid creating too long chunks*/
	GF_ISOM_STORE_DRIFT_INTERLEAVED,
	/*! tightly interleaves samples based on their DTS, therefore allowing better placement of samples in the file.
	This is used for both http interleaving and Hinting optimizations*/
	GF_ISOM_STORE_TIGHT,
	/*! FASTSTART: same as FLAT but moves moov before mdat at the end*/
	GF_ISOM_STORE_FASTSTART,
} GF_ISOStorageMode;

/*! freezes order of the current box tree in the file.
By default the library always reorder boxes in the recommended order in the various specifications implemented.
New created tracks or meta items will not have a frozen order of boxes, but the function can be called several time
\param isom_file the target ISO file
\return error if any
*/
GF_Err gf_isom_freeze_order(GF_ISOFile *isom_file);

/*! keeps UTC edit times when storing
\param isom_file the target ISO file
\param keep_utc if GF_TRUE, do not edit times
*/
void gf_isom_keep_utc_times(GF_ISOFile *isom_file, Bool keep_utc);

#endif

/*! Checks if UTC keeping is enabled
\param isom_file the target ISO file
\return GF_TRUE if UTC keeping is enabled
*/
Bool gf_isom_has_keep_utc_times(GF_ISOFile *isom_file);

#ifndef GPAC_DISABLE_ISOM_WRITE

/*! sets the timescale of the movie. This rescales times expressed in movie timescale in edit lists and mvex boxes
\param isom_file the target ISO file
\param timeScale the target timescale
\return error if any
*/
GF_Err gf_isom_set_timescale(GF_ISOFile *isom_file, u32 timeScale);

/*! loads a set of top-level boxes in moov udta and child boxes. UDTA will be replaced if already present
\param isom_file the target ISO file
\param moov_boxes a serialized array of boxes to add
\param moov_boxes_size the size of the serialized array of boxes
\param udta_only only replace/inject udta box and entries
\return error if any
*/
GF_Err gf_isom_load_extra_boxes(GF_ISOFile *isom_file, u8 *moov_boxes, u32 moov_boxes_size, Bool udta_only);

/*! creates a new track
\param isom_file the target ISO file
\param trackID the ID of the track - if 0, the track ID is chosen by the API
\param MediaType the handler type (four character code) of the media
\param TimeScale the time scale of the media
\return the track number or 0 if error*/
u32 gf_isom_new_track(GF_ISOFile *isom_file, GF_ISOTrackID trackID, u32 MediaType, u32 TimeScale);

/*! creates a new track from an encoded trak box.
\param isom_file the target ISO file
\param trackID the ID of the track - if 0, the track ID is chosen by the API
\param MediaType the handler type (four character code) of the media
\param TimeScale the time scale of the media
\param tk_box a serialized trak box to use as template
\param tk_box_size the size of the serialized trak box
\param udta_only only replace/inject udta box and entries
\return the track number or 0 if error*/
u32 gf_isom_new_track_from_template(GF_ISOFile *isom_file, GF_ISOTrackID trackID, u32 MediaType, u32 TimeScale, u8 *tk_box, u32 tk_box_size, Bool udta_only);

/*! removes a track - internal cross dependencies will be updated.
\warning Any OD streams with references to this track through  ODUpdate, ESDUpdate, ESDRemove commands
will be rewritten
\param isom_file the target ISO file
\param trackNumber the target track to remove file
\return error if any
*/
GF_Err gf_isom_remove_track(GF_ISOFile *isom_file, u32 trackNumber);

/*! sets the enable flag of a track
\param isom_file the target ISO file
\param trackNumber the target track
\param enableTrack if GF_TRUE, track is enabled, otherwise disabled
\return error if any
*/
GF_Err gf_isom_set_track_enabled(GF_ISOFile *isom_file, u32 trackNumber, Bool enableTrack);

/*! Track header flags operation type*/
typedef enum
{
	/*! set flags, erasing previous value*/
	GF_ISOM_TKFLAGS_SET = 1,
	/*! add flags*/
	GF_ISOM_TKFLAGS_REM,
	/*! remove flags*/
	GF_ISOM_TKFLAGS_ADD,
} GF_ISOMTrackFlagOp;

#endif //GPAC_DISABLE_ISOM_WRITE

/*! Track header flags*/
enum
{
	/*! track is enabled */
	GF_ISOM_TK_ENABLED = 1,
	/*! track is in regular presentation*/
	GF_ISOM_TK_IN_MOVIE = 1<<1,
	/*! track is in preview*/
	GF_ISOM_TK_IN_PREVIEW = 1<<2,
	/*! track size is an aspect ratio indicator only*/
	GF_ISOM_TK_SIZE_IS_AR = 1<<3
};

#ifndef GPAC_DISABLE_ISOM_WRITE

/*! toggles track flags on or off
\param isom_file the target ISO file
\param trackNumber the target track
\param flags flags to modify
\param op flag operation mode
\return error if any
*/
GF_Err gf_isom_set_track_flags(GF_ISOFile *isom_file, u32 trackNumber, u32 flags, GF_ISOMTrackFlagOp op);

/*! sets creationTime and modificationTime of the movie to the specified dates (no validty check)
\param isom_file the target ISO file
\param create_time the new creation time
\param modif_time the new modification time
\return error if any
*/
GF_Err gf_isom_set_creation_time(GF_ISOFile *isom_file, u64 create_time, u64 modif_time);

/*! sets creationTime and modificationTime of the track to the specified dates
\param isom_file the target ISO file
\param trackNumber the target track
\param create_time the new creation time
\param modif_time the new modification time
\return error if any
*/
GF_Err gf_isom_set_track_creation_time(GF_ISOFile *isom_file, u32 trackNumber, u64 create_time, u64 modif_time);

/*! sets creationTime and modificationTime of the track media header to the specified dates
\param isom_file the target ISO file
\param trackNumber the target track
\param create_time the new creation time
\param modif_time the new modification time
\return error if any
*/
GF_Err gf_isom_set_media_creation_time(GF_ISOFile *isom_file, u32 trackNumber, u64 create_time, u64 modif_time);

/*! changes the ID of a track - all track references present in the file are updated
\param isom_file the target ISO file
\param trackNumber the target track
\param trackID the new track ID
\return error if trackID is already in used in the file*/
GF_Err gf_isom_set_track_id(GF_ISOFile *isom_file, u32 trackNumber, GF_ISOTrackID trackID);

/*! forces to rewrite all dependencies when track ID changes. Used to check if track references are broken during import of a single track
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_rewrite_track_dependencies(GF_ISOFile *isom_file, u32 trackNumber);

/*! adds a sample to a track
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index associated with the sample
\param sample the target sample to add
\return error if any
*/
GF_Err gf_isom_add_sample(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, const GF_ISOSample *sample);

/*! copies all sample dependency, subSample and sample group information from the given sampleNumber in source file to the last added sample in dest file
\param dst the destination ISO file
\param dst_track the destination track
\param src the source ISO file
\param src_track the source track
\param sampleNumber the source sample number
\return error if any
*/
GF_Err gf_isom_copy_sample_info(GF_ISOFile *dst, u32 dst_track, GF_ISOFile *src, u32 src_track, u32 sampleNumber);

/*! adds a sync shadow sample to a track.
- There must be a regular sample with the same DTS.
- Sync Shadow samples MUST be RAP and can only use the same sample description as the sample they shadow
- Currently, adding sync shadow must be done in order (no sample insertion)

\param isom_file the target ISO file
\param trackNumber the target track
\param sample the target shadow sample to add
\return error if any
*/
GF_Err gf_isom_add_sample_shadow(GF_ISOFile *isom_file, u32 trackNumber, GF_ISOSample *sample);

/*! adds data to current sample in the track. This will update the data size.
CANNOT be used with OD media type
There shall not be any other
\param isom_file the target ISO file
\param trackNumber the target track
\param data the data to append to the sample
\param data_size the size of the data to append
\return error if any
*/
GF_Err gf_isom_append_sample_data(GF_ISOFile *isom_file, u32 trackNumber, u8 *data, u32 data_size);

/*! adds sample references to a track
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index associated with the sample
\param sample the target sample to add
\param dataOffset is the offset in bytes of the data in the referenced file.
\return error if any
*/
GF_Err gf_isom_add_sample_reference(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_ISOSample *sample, u64 dataOffset);

/*! sets the duration of the last media sample. If not set, the duration of the last sample is the
duration of the previous one if any, or media TimeScale (default value). This does not modify the edit list if any,
you must modify this using \ref gf_isom_set_edit
\param isom_file the target ISO file
\param trackNumber the target track
\param duration duration of last sample in media timescale
\return error if any
*/
GF_Err gf_isom_set_last_sample_duration(GF_ISOFile *isom_file, u32 trackNumber, u32 duration);

/*! sets the duration of the last media sample. If not set, the duration of the last sample is the
duration of the previous one if any, or media TimeScale (default value). This does not modify the edit list if any,
you must modify this using \ref gf_isom_set_edit.
If both dur_num and dur_den are both zero, forces last sample duration to be the same as previous sample
\param isom_file the target ISO file
\param trackNumber the target track
\param dur_num duration num value
\param dur_den duration num value
\return error if any
*/
GF_Err gf_isom_set_last_sample_duration_ex(GF_ISOFile *isom_file, u32 trackNumber, u32 dur_num, u32 dur_den);

/*! patches last stts entry to make sure the cumulated duration equals the given next_dts value - this will overrite timing of all previous samples using an average dur
\param isom_file the target ISO file
\param trackNumber the target track
\param next_dts target decode time of next sample
\return error if any
*/
GF_Err gf_isom_patch_last_sample_duration(GF_ISOFile *isom_file, u32 trackNumber, u64 next_dts);

/*! adds a track reference to another track
\param isom_file the target ISO file
\param trackNumber the target track
\param referenceType the four character code of the reference
\param ReferencedTrackID the ID of the track referred to
\return error if any
*/
GF_Err gf_isom_set_track_reference(GF_ISOFile *isom_file, u32 trackNumber, u32 referenceType, GF_ISOTrackID ReferencedTrackID);

/*! removes all track references
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_remove_track_references(GF_ISOFile *isom_file, u32 trackNumber);

/*! removes any track reference poiting to a non-existing track
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_purge_track_reference(GF_ISOFile *isom_file, u32 trackNumber);

/*! removes all track references of a given type
\param isom_file the target ISO file
\param trackNumber the target track
\param ref_type the reference type to remove
\return error if any
*/
GF_Err gf_isom_remove_track_reference(GF_ISOFile *isom_file, u32 trackNumber, u32 ref_type);

/*! sets track handler name.
\param isom_file the target ISO file
\param trackNumber the target track
\param nameUTF8 the handler name; either NULL (reset), a UTF-8 formatted string or a UTF8 file resource in the form "file://path/to/file_utf8"
\return error if any
*/
GF_Err gf_isom_set_handler_name(GF_ISOFile *isom_file, u32 trackNumber, const char *nameUTF8);

/*! updates the sample size table - this is needed when using \ref gf_isom_append_sample_data in case the resulting samples
are of same sizes (typically in 3GP speech tracks)
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_refresh_size_info(GF_ISOFile *isom_file, u32 trackNumber);

/*! updates the duration of the movie.This is done automatically when storing the file or editing timesales/edit list, but it is not done when adding samples.
\param isom_file the target ISO file
\return error if any
*/
GF_Err gf_isom_update_duration(GF_ISOFile *isom_file);


/*! updates a given sample of the media. This function updates both media data of sample and sample properties (DTS, CTS, SAP type)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the number of the sample to update
\param sample the new sample
\param data_only if set to GF_TRUE, only the sample data is updated, not other info
\return error if any
*/
GF_Err gf_isom_update_sample(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, GF_ISOSample *sample, Bool data_only);

/*! updates a sample reference in the media. Note that the sample MUST exists, and that sample->data MUST be NULL and sample->dataLength must be NON NULL.
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the number of the sample to update
\param sample the new sample
\param data_offset new offset of sample in referenced file
\return error if any
*/
GF_Err gf_isom_update_sample_reference(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, GF_ISOSample *sample, u64 data_offset);

/*! removes a given sample
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the number of the sample to update
\return error if any
*/
GF_Err gf_isom_remove_sample(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber);


/*! changes media time scale

\param isom_file the target ISO file
\param trackNumber the target track
\param new_timescale the new timescale to set
\param new_tsinc if not 0, changes sample duration and composition offsets to new_tsinc/new_timescale. If non-constant sample dur is used, uses the samllest sample dur in the track. Otherwise, only changes the timescale
\param force_rescale_type type fo rescaling, Ignored if new_tsinc is not 0:
 - if set to 0, rescale timings.
 - if set to 1, only the media timescale is changed but media times are not updated.
 - if set to 2,  media timescale is updated if new_timescale is set, and all sample durations are set to new_tsinc
\return GF_EOS if no action taken (same config), or error if any
*/
GF_Err gf_isom_set_media_timescale(GF_ISOFile *isom_file, u32 trackNumber, u32 new_timescale, u32 new_tsinc, u32 force_rescale_type);



/*! adds sample auxiliary data

\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the sample number. Must be equal or larger to last auxiliary
\param aux_type auxiliary sample data type, shall not be 0
\param aux_info auxiliary sample data specific info type, may be 0
\param data data to add
\param size size of data to add
\return error if any
*/
GF_Err gf_isom_add_sample_aux_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 aux_type, u32 aux_info, u8 *data, u32 size);

/*! sets the save file name of the (edited) movie.
If the movie is edited, the default fileName is the open name suffixed with an internally defined extension "%p_isotmp")"
\note you cannot save an edited file under the same name (overwrite not allowed)
If the movie is created (WRITE mode), the default filename is $OPEN_NAME

\param isom_file the target ISO file
\param filename the new final filename
\return error if any
*/
GF_Err gf_isom_set_final_name(GF_ISOFile *isom_file, char *filename);

/*! sets the storage mode of a file (FLAT, STREAMABLE, INTERLEAVED)
\param isom_file the target ISO file
\param storage_mode the target storage mode
\return error if any
*/
GF_Err gf_isom_set_storage_mode(GF_ISOFile *isom_file, GF_ISOStorageMode storage_mode);

/*! sets the interleaving time of media data (INTERLEAVED mode only)
\param isom_file the target ISO file
\param InterleaveTime the target interleaving time in movie timescale
\return error if any
*/
GF_Err gf_isom_set_interleave_time(GF_ISOFile *isom_file, u32 InterleaveTime);

/*! forces usage of 64 bit chunk offsets
\param isom_file the target ISO file
\param set_on if GF_TRUE, 64 bit chunk offsets are always used; otherwise, they are used only for large files
\return error if any
*/
GF_Err gf_isom_force_64bit_chunk_offset(GF_ISOFile *isom_file, Bool set_on);

/*! compression mode of top-level boxes*/
typedef enum
{
	/*! no compression is used*/
	GF_ISOM_COMP_NONE=0,
	/*! only moov box is compressed*/
	GF_ISOM_COMP_MOOV,
	/*! only moof boxes are compressed*/
	GF_ISOM_COMP_MOOF,
	/*! only moof and sidx boxes are compressed*/
	GF_ISOM_COMP_MOOF_SIDX,
	/*! only moof,  sidx and ssix boxes are compressed*/
	GF_ISOM_COMP_MOOF_SSIX,
	/*! all (moov, moof,  sidx and ssix) boxes are compressed*/
	GF_ISOM_COMP_ALL,
} GF_ISOCompressMode;

enum
{
	/*! forces compressed box even if compress size is larger than uncompressed size*/
	GF_ISOM_COMP_FORCE_ALL	=	0x01,
	/*! wraps ftyp in otyp*/
	GF_ISOM_COMP_WRAP_FTYPE	=	0x02,
};


/*! sets compression mode of file
\param isom_file the target ISO file
\param compress_mode the desired compress mode
\param compress_flags compress mode flags
\return error if any
*/
GF_Err gf_isom_enable_compression(GF_ISOFile *isom_file, GF_ISOCompressMode compress_mode, u32 compress_flags);

/*! sets the copyright in one language
\param isom_file the target ISO file
\param threeCharCode the ISO three character language code for copyright
\param notice the copyright notice to add
\return error if any
*/
GF_Err gf_isom_set_copyright(GF_ISOFile *isom_file, const char *threeCharCode, char *notice);

/*! adds a kind type to a track
\param isom_file the target ISO file
\param trackNumber the target track
\param schemeURI the scheme URI of the added kind
\param value the value of the added kind
\return error if any
*/
GF_Err gf_isom_add_track_kind(GF_ISOFile *isom_file, u32 trackNumber, const char *schemeURI, const char *value);

/*! removes a kind type to the track, all if NULL params
\param isom_file the target ISO file
\param trackNumber the target track
\param schemeURI the scheme URI of the removed kind
\param value the value of the removed kind
\return error if any
*/
GF_Err gf_isom_remove_track_kind(GF_ISOFile *isom_file, u32 trackNumber, const char *schemeURI, const char *value);

/*! changes the handler type of the media
\warning This may completely breaks the parsing of the media track
\param isom_file the target ISO file
\param trackNumber the target track
\param new_type the new handler four character type
\return error if any
*/
GF_Err gf_isom_set_media_type(GF_ISOFile *isom_file, u32 trackNumber, u32 new_type);

/*! changes the type of the sampleDescriptionBox
\warning This may completely breaks the parsing of the media track
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param new_type the new four character code type of the smaple description
\return error if any
*/
GF_Err gf_isom_set_media_subtype(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 new_type);

/*! sets a track in an alternate group
\param isom_file the target ISO file
\param trackNumber the target track
\param groupId the alternate group ID
\return error if any
*/
GF_Err gf_isom_set_alternate_group_id(GF_ISOFile *isom_file, u32 trackNumber, u32 groupId);

/*! adds chapter info:

\param isom_file the target ISO file
\param trackNumber the target track. If 0, the chapter info is added to the movie, otherwise to the track
\param timestamp the chapter start time in milliseconds. Chapters are added in order to the file. If a chapter with same timestamp
	is found, its name is updated but no entry is created.
\param name the chapter name. If NULL, defaults to 'Chapter N'
\return error if any
*/
GF_Err gf_isom_add_chapter(GF_ISOFile *isom_file, u32 trackNumber, u64 timestamp, char *name);

/*! deletes copyright
\param isom_file the target ISO file
\param trackNumber the target track
\param index the 1-based index of the copyright notice to remove, or 0 to remove all chapters
\return error if any
*/
GF_Err gf_isom_remove_chapter(GF_ISOFile *isom_file, u32 trackNumber, u32 index);

/*! updates or inserts a new edit in the track time line. Edits are used to modify
the media normal timing. EditTime and EditDuration are expressed in movie timescale
\note If a segment with EditTime already exists, it is erase
\note If there is a segment before this new one, its duration is adjust to match EditTime of the new segment
\warning The first segment always have an EditTime of 0. You should insert an empty or dwelled segment first

\param isom_file the target ISO file
\param trackNumber the target track
\param EditTime the start of the edit in movie timescale
\param EditDuration the duration of the edit in movie timecale
\param MediaTime the corresponding media time of the start of the edit, in media timescale. -1 for empty edits
\param EditMode the edit mode
\return error if any, GF_EOS if empty edit was inserted 
*/
GF_Err gf_isom_set_edit(GF_ISOFile *isom_file, u32 trackNumber, u64 EditTime, u64 EditDuration, u64 MediaTime, GF_ISOEditType EditMode);



/*! updates or inserts a new edit in the track time line. Edits are used to modify
the media normal timing. EditTime and EditDuration are expressed in movie timescale
\note If a segment with EditTime already exists, it is erase
\note If there is a segment before this new one, its duration is adjust to match EditTime of the new segment
\warning The first segment always have an EditTime of 0. You should insert an empty or dwelled segment first

\param isom_file the target ISO file
\param trackNumber the target track
\param EditTime the start of the edit in movie timescale
\param EditDuration the duration of the edit in movie timecale
\param MediaTime the corresponding media time of the start of the edit, in media timescale. -1 for empty edits
\param MediaRate a 16.16 rate (0x10000 means normal playback)
\return error if any
*/
GF_Err gf_isom_set_edit_with_rate(GF_ISOFile *isom_file, u32 trackNumber, u64 EditTime, u64 EditDuration, u64 MediaTime, u32 MediaRate);


/*! same as \ref gf_isom_set_edit except only modifies duration type and mediaType
\param isom_file the target ISO file
\param trackNumber the target track
\param edit_index the 1-based index of the edit to update
\param EditDuration duration of the edit in movie timescale
\param MediaTime the corresponding media time of the start of the edit, in media timescale. -1 for empty edits
\param EditMode the edit mode
\return error if any
*/
GF_Err gf_isom_modify_edit(GF_ISOFile *isom_file, u32 trackNumber, u32 edit_index, u64 EditDuration, u64 MediaTime, GF_ISOEditType EditMode);

/*! same as \ref gf_isom_modify_edit except only appends new segment
\param isom_file the target ISO file
\param trackNumber the target track
\param EditDuration duration of the edit in movie timescale
\param MediaTime the corresponding media time of the start of the edit, in media timescale. -1 for empty edits
\param EditMode the edit mode
\return error if any
*/
GF_Err gf_isom_append_edit(GF_ISOFile *isom_file, u32 trackNumber, u64 EditDuration, u64 MediaTime, GF_ISOEditType EditMode);

/*! removes all edits in the track
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_remove_edits(GF_ISOFile *isom_file, u32 trackNumber);

/*! removes the given edit. If this is not the last segment, the next segment duration is updated to maintain a continous timeline
\param isom_file the target ISO file
\param trackNumber the target track
\param edit_index the 1-based index of the edit to update
\return error if any
*/
GF_Err gf_isom_remove_edit(GF_ISOFile *isom_file, u32 trackNumber, u32 edit_index);

/*! updates edit list after track edition. All edit entries with a duration or media starttime larger than the media duration are clamped to media duration
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_update_edit_list_duration(GF_ISOFile *isom_file, u32 trackNumber);


/*! remove track, moov or file-level UUID box of matching type
\param isom_file the target ISO file
\param trackNumber the target track for the UUID box; if 0, removes from movie; if 0xFFFFFFFF, removes from file
\param UUID the UUID of the box to remove
\return error if any
*/
GF_Err gf_isom_remove_uuid(GF_ISOFile *isom_file, u32 trackNumber, bin128 UUID);

/*! adds track, moov or file-level UUID box
\param isom_file the target ISO file
\param trackNumber the target track for the UUID box; if 0, removes from movie; if 0xFFFFFFFF, removes from file
\param UUID the UUID of the box to remove
\param data the data to add, may be NULL
\param size the size of the data to add, shall be 0 when data is NULL
\return error if any
*/
GF_Err gf_isom_add_uuid(GF_ISOFile *isom_file, u32 trackNumber, bin128 UUID, const u8 *data, u32 size);

/*! uses a compact track version for sample size. This is not usually recommended
except for speech codecs where the track has a lot of small samples
compaction is done automatically while writing based on the track's sample sizes
\param isom_file the target ISO file
\param trackNumber the target track for the udta box; if 0, add the udta to the movie;
\param CompactionOn if set to GF_TRUE, compact size tables are used; otherwise regular size tables are used
\return error if any
*/
GF_Err gf_isom_use_compact_size(GF_ISOFile *isom_file, u32 trackNumber, Bool CompactionOn);

/*! disabled brand rewrite for file, usually done for temporary import in an existing file
\param isom_file the target ISO file
\param do_disable if true, brand rewrite is disabled, otherwise enabled
\return error if any
*/
GF_Err gf_isom_disable_brand_rewrite(GF_ISOFile *isom_file, Bool do_disable);

/*! sets the brand of the movie
\note this automatically adds the major brand to the set of alternate brands if not present
\param isom_file the target ISO file
\param MajorBrand four character code of the major brand to set
\param MinorVersion version of the brand
\return error if any
*/
GF_Err gf_isom_set_brand_info(GF_ISOFile *isom_file, u32 MajorBrand, u32 MinorVersion);

/*! adds or removes an alternate brand for the movie.
\note When removing an alternate brand equal to the major brand, the major brand is updated with the first alternate brand remaining, or 'isom' otherwise
\param isom_file the target ISO file
\param Brand four character code of the brand to add or remove
\param AddIt if set to GF_TRUE, the brand is added, otherwise it is removed
\return error if any
*/
GF_Err gf_isom_modify_alternate_brand(GF_ISOFile *isom_file, u32 Brand, Bool AddIt);

/*! removes all alternate brands except major brand
\param isom_file the target ISO file
\return error if any
*/
GF_Err gf_isom_reset_alt_brands(GF_ISOFile *isom_file);

/*! removes all alternate brands except major brand
\param isom_file the target ISO file
\param leave_empty if GF_TRUE, does not create a default alternate brand matching the major brand
\return error if any
*/
GF_Err gf_isom_reset_alt_brands_ex(GF_ISOFile *isom_file, Bool leave_empty);

/*! set sample dependency flags - see ISO/IEC 14496-12 and \ref gf_filter_pck_set_dependency_flags
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleNumber the target sample number
\param isLeading indicates that the sample is a leading picture
\param dependsOn indicates the sample dependency towards other samples
\param dependedOn indicates the sample dependency from other samples
\param redundant indicates that the sample contains redundant coding
\return error if any
*/
GF_Err gf_isom_set_sample_flags(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 isLeading, u32 dependsOn, u32 dependedOn, u32 redundant);

/*! sets size information of a sample description of a visual track
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param Width the width in pixels
\param Height the height in pixels
\return error if any
*/
GF_Err gf_isom_set_visual_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 Width, u32 Height);

/*! sets bit depth of a sample description of a visual track (for uncompressed media usually)
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param bitDepth the bit depth of each pixel (eg 24 for RGB, 32 for RGBA)
\return error if any
*/
GF_Err gf_isom_set_visual_bit_depth(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u16 bitDepth);

/*! sets a visual track layout info
\param isom_file the target ISO file
\param trackNumber the target track number
\param width the track width in pixels
\param height the track height in pixels
\param translation_x the horizontal translation (from the left) of the track in the movie canvas, expressed as 16.16 fixed point float
\param translation_y the vertical translation (from the top) of the track in the movie canvas, expressed as 16.16 fixed point float
\param layer z order of the track on the canvas
\return error if any
*/
GF_Err gf_isom_set_track_layout_info(GF_ISOFile *isom_file, u32 trackNumber, u32 width, u32 height, s32 translation_x, s32 translation_y, s16 layer);

/*! sets track matrix
\param isom_file the target ISO file
\param trackNumber the target track number
\param matrix the transformation matrix of the track on the movie canvas; all coeficients are expressed as 16.16 floating points
\return error if any
 */
GF_Err gf_isom_set_track_matrix(GF_ISOFile *isom_file, u32 trackNumber, s32 matrix[9]);

/*! sets the pixel aspect ratio for a sample description
\note the aspect ratio is expressed as hSpacing divided by vSpacing; 2:1 means pixel is twice as wide as it is high
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param hSpacing horizontal spacing of the aspect ratio; a value of 0 removes PAR; negative value means 1
\param vSpacing vertical spacing of the aspect ratio; a value of 0 removes PAR; negative value means 1
\param force_par if set, forces PAR to 1:1 when hSpacing=vSpacing; otherwise removes PAR when hSpacing=vSpacing
\return error if any
*/
GF_Err gf_isom_set_pixel_aspect_ratio(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, s32 hSpacing, s32 vSpacing, Bool force_par);

/*! sets clean aperture (crop window, see ISO/IEC 14496-12) for a sample description
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param cleanApertureWidthN nominator of clean aperture horizontal size
\param cleanApertureWidthD denominator of clean aperture horizontal size
\param cleanApertureHeightN nominator of clean aperture vertical size
\param cleanApertureHeightD denominator of clean aperture vertical size
\param horizOffN nominator of horizontal offset of clean aperture center minus (width-1)/2 (eg 0 sets center to center of video)
\param horizOffD denominator of horizontal offset of clean aperture center minus (width-1)/2 (eg 0 sets center to center of video)
\param vertOffN nominator of vertical offset of clean aperture center minus (height-1)/2 (eg 0 sets center to center of video)
\param vertOffD denominator of vertical offset of clean aperture center minus (height-1)/2 (eg 0 sets center to center of video)
\return error if any
*/
GF_Err gf_isom_set_clean_aperture(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 cleanApertureWidthN, u32 cleanApertureWidthD, u32 cleanApertureHeightN, u32 cleanApertureHeightD, s32 horizOffN, u32 horizOffD, s32 vertOffN, u32 vertOffD);

/*! updates track aperture information for QT/ProRes
\param isom_file the target ISO file
\param trackNumber the target track number
\param remove if GF_TRUE, remove track aperture information, otherwise updates it
\return error if any
*/
GF_Err gf_isom_update_aperture_info(GF_ISOFile *isom_file, u32 trackNumber, Bool remove);


/*! sets high dynamic range information for a sample description
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param mdcv the mastering display colour volume to set, if NULL removes the info
\param clli the content light level to set, if NULL removes the info
\return error if any
*/
GF_Err gf_isom_set_high_dynamic_range_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_MasteringDisplayColourVolumeInfo *mdcv, GF_ContentLightLevelInfo *clli);

/*! force Dolby Vision profile: mainly used when the bitstream doesn't contain all the necessary information
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param dvcc the Dolby Vision configuration
\return error if any
*/
GF_Err gf_isom_set_dolby_vision_profile(GF_ISOFile* isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_DOVIDecoderConfigurationRecord *dvcc);


/*! sets image sequence coding constraints (mostly used for HEIF image files)
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param remove if set to GF_TRUE, removes coding constraints
\param all_ref_pics_intra indicates if all reference pictures are intra frames
\param intra_pred_used indicates if intra prediction is used
\param max_ref_per_pic indicates the max number of reference images per picture
\return error if any
 */
GF_Err gf_isom_set_image_sequence_coding_constraints(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, Bool remove, Bool all_ref_pics_intra, Bool intra_pred_used, u32 max_ref_per_pic);

/*! sets image sequence alpha flag (mostly used for HEIF image files). The alpha flag indicates the image sequence is an alpha plane
or has an alpha channel
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param remove if set to GF_TRUE, removes coding constraints
\return error if any
*/
GF_Err gf_isom_set_image_sequence_alpha(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, Bool remove);

/*! sets colour information for a sample description
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param colour_type the four character code of the colour type to set (nclc, nclx, prof, ricc); if 0, removes all color info
\param colour_primaries the colour primaries for nclc/nclx as defined in ISO/IEC 23001-8
\param transfer_characteristics the colour primaries for nclc/nclx as defined in ISO/IEC 23001-8
\param matrix_coefficients the colour primaries for nclc/nclx as defined in ISO/IEC 23001-8
\param full_range_flag the colour primaries for nclc as defined in ISO/IEC 23001-8
\param icc_data the icc data pto set for prof and ricc types
\param icc_size the size of the icc data
\return error if any
*/
GF_Err gf_isom_set_visual_color_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 colour_type, u16 colour_primaries, u16 transfer_characteristics, u16 matrix_coefficients, Bool full_range_flag, u8 *icc_data, u32 icc_size);


/*! Audio Sample Description signaling mode*/
typedef enum {
	/*! use ISOBMF sample entry v0*/
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_NOT_SET = 0,
	/*! use ISOBMF sample entry v0*/
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_BS,
	/*! use ISOBMF sample entry v0 and forces channel count to 2*/
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_2,
	/*! use ISOBMF sample entry v1*/
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_MPEG,
	/*! use QTFF sample entry v1*/
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF
} GF_AudioSampleEntryImportMode;


/*! sets audio format  information for a sample description
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param sampleRate the audio sample rate
\param nbChannels the number of audio channels
\param bitsPerSample the number of bits per sample, mostly used for raw audio
\param asemode type of audio entry signaling desired
\return error if any
*/
GF_Err gf_isom_set_audio_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 sampleRate, u32 nbChannels, u8 bitsPerSample, GF_AudioSampleEntryImportMode asemode);


/*! sets audio channel and object layout  information for a sample description, ISOBMFF style
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param layout the layout information
\return error if any
*/
GF_Err gf_isom_set_audio_layout(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_AudioChannelLayout *layout);

/*! sets CTS unpack mode (used for B-frames & like): in unpack mode, each sample uses one entry in CTTS tables

\param isom_file the target ISO file
\param trackNumber the target track number
\param unpack if GF_TRUE, sets unpack on, creating a ctts table if none found; if GF_FALSE, sets unpack off and repacks all table info
\return error if any
*/
GF_Err gf_isom_set_cts_packing(GF_ISOFile *isom_file, u32 trackNumber, Bool unpack);

/*! shifts all CTS with the given offset. This MUST be called in unpack mode only
\param isom_file the target ISO file
\param trackNumber the target track number
\param offset_shift CTS offset shift in media timescale
\return error if any
*/
GF_Err gf_isom_shift_cts_offset(GF_ISOFile *isom_file, u32 trackNumber, s32 offset_shift);

/*! enables negative composition offset in track
\note this will compute the composition to decode time information
\param isom_file the target ISO file
\param trackNumber the target track
\param use_negative_offsets if GF_TRUE, negative offsets are used, otherwise they are disabled
\return error if any
*/
GF_Err gf_isom_set_composition_offset_mode(GF_ISOFile *isom_file, u32 trackNumber, Bool use_negative_offsets);

/*! enables negative composition offset in track and shift offsets

\param isom_file the target ISO file
\param trackNumber the target track
\param ctts_shift shif CTS offsets by the given time in media timescale if positive offsets only are used
\return error if any
*/
GF_Err gf_isom_set_ctts_v1(GF_ISOFile *isom_file, u32 trackNumber, u32 ctts_shift);


/*! sets language for a track
\param isom_file the target ISO file
\param trackNumber the target track number
\param code 3-character code or BCP-47 code media language
\return error if any
*/
GF_Err gf_isom_set_media_language(GF_ISOFile *isom_file, u32 trackNumber, char *code);

/*! gets the ID of the last created track
\param isom_file the target ISO file
\return the last created track ID
*/
GF_ISOTrackID gf_isom_get_last_created_track_id(GF_ISOFile *isom_file);

/*! applies a box patch to the file. See examples in gpac test suite, media/boxpatch/
\param isom_file the target ISO file
\param trackID the ID of the track to patch, in case one of the box patch applies to a track
\param box_patch_filename the name of the file containing the box patches
\param for_fragments indicates if the patch targets movie fragments or regular moov
\return error if any
*/
GF_Err gf_isom_apply_box_patch(GF_ISOFile *isom_file, GF_ISOTrackID trackID, const char *box_patch_filename, Bool for_fragments);

/*! sets track magic number
\param isom_file the target ISO file
\param trackNumber the target track
\param magic the magic number to set; magic number is not written to file
\return error if any
*/
GF_Err gf_isom_set_track_magic(GF_ISOFile *isom_file, u32 trackNumber, u64 magic);

/*! sets track index in moov
\param isom_file the target ISO file
\param trackNumber the target track
\param index the 1-based index to set. Tracks will be reordered after this!
\param track_num_changed callback function used to notify track changes during the call to this function
\param udta opaque user data for the callback function
\return error if any
*/
GF_Err gf_isom_set_track_index(GF_ISOFile *isom_file, u32 trackNumber, u32 index, void (*track_num_changed)(void *udta, u32 old_track_num, u32 new_track_num), void *udta);

/*! removes a sample description with the given index
\warning This does not remove any added samples for that stream description, nor rewrite the sample to chunk and other boxes referencing the sample description index !
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description to remove
\return error if any
*/
GF_Err gf_isom_remove_stream_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! updates average and max bitrate of a sample description
if both average_bitrate and max_bitrate are 0, this removes any bitrate information
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description
\param average_bitrate the average bitrate of the media for that sample description
\param max_bitrate the maximum bitrate of the media for that sample description
\param decode_buffer_size the decoder buffer size in bytes for that sample description
\return error if any
*/
GF_Err gf_isom_update_bitrate(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 average_bitrate, u32 max_bitrate, u32 decode_buffer_size);


/*! track clone flags*/
typedef enum
{
	/*! set this flag to keep data reference entries while cloning track*/
	GF_ISOM_CLONE_TRACK_KEEP_DREF = 1,
	/*! set this flag to avoid cloning track as a QT track while cloning track*/
	GF_ISOM_CLONE_TRACK_NO_QT = 1<<1,
	/*! drop track ID while importing*/
	GF_ISOM_CLONE_TRACK_DROP_ID = 1<<2,
	/*! reset media duration when cloning */
	GF_ISOM_CLONE_RESET_DURATION = 1<<3
} GF_ISOTrackCloneFlags;

/*! clones a track. This clones everything except media data and sample info (DTS, CTS, RAPs, etc...), and also clones sample descriptions
\param orig_file the source ISO file
\param orig_track the source track
\param dest_file the destination ISO file
\param flags flags to use during clone
\param dest_track set to the track number of cloned track
\return error if any
*/
GF_Err gf_isom_clone_track(GF_ISOFile *orig_file, u32 orig_track, GF_ISOFile *dest_file, GF_ISOTrackCloneFlags flags, u32 *dest_track);


/*! sets the GroupID of a track (only used for optimized interleaving). By setting GroupIDs
you can specify the storage order for media data of a group of streams. This is useful
for BIFS presentation so that static resources of the scene can be downloaded before BIFS

\param isom_file the target ISO file
\param trackNumber the target track
\param GroupID the desired group ID
\return error if any
*/
GF_Err gf_isom_set_track_interleaving_group(GF_ISOFile *isom_file, u32 trackNumber, u32 GroupID);

/*! sets the priority of a track within a Group (used for optimized interleaving and hinting).
This allows tracks to be stored before other within a same group, for instance the
hint track data can be stored just before the media data, reducing disk seeking

\param isom_file the target ISO file
\param trackNumber the target track
\param InversePriority the desired priority. For a same time, within a group of tracks, the track with the lowest InversePriority will
be written first
\return error if any
*/
GF_Err gf_isom_set_track_priority_in_group(GF_ISOFile *isom_file, u32 trackNumber, u32 InversePriority);

/*! sets the maximum chunk size for a track
\param isom_file the target ISO file
\param trackNumber the target track
\param maxChunkSize the maximum chunk size in bytes
\return error if any
*/
GF_Err gf_isom_hint_max_chunk_size(GF_ISOFile *isom_file, u32 trackNumber, u32 maxChunkSize);

/*! sets the maximum chunk duration for a track
\param isom_file the target ISO file
\param trackNumber the target track
\param maxChunkDur the maximum chunk duration in media timescale
\return error if any
*/
GF_Err gf_isom_hint_max_chunk_duration(GF_ISOFile *isom_file, u32 trackNumber, u32 maxChunkDur);

/*! sets up interleaving for storage (shortcut for storeage mode + interleave_time)
\param isom_file the target ISO file
\param TimeInSec the desired interleaving time in seconds
\return error if any
*/
GF_Err gf_isom_make_interleave(GF_ISOFile *isom_file, Double TimeInSec);

/*! sets up interleaving for storage (shortcut for storeage mode + interleave_time)
\param isom_file the target ISO file
\param fTimeInSec the desired interleaving time in seconds, as a fraction
\return error if any
*/
GF_Err gf_isom_make_interleave_ex(GF_ISOFile *isom_file, GF_Fraction *fTimeInSec);

/*! sets progress callback when writing a file
\param isom_file the target ISO file
\param progress_cbk the progress callback function
\param progress_cbk_udta opaque data passed to the progress callback function
*/
void gf_isom_set_progress_callback(GF_ISOFile *isom_file, void (*progress_cbk)(void *udta, u64 nb_done, u64 nb_total), void *progress_cbk_udta);

/*! Callback function to receive new data blocks
\param usr_data user callback, as passed to \ref gf_isom_set_write_callback
\param block data block to write
\param block_size data block size in bytes
\param sample_cbk_data callback data of sample or NULL
\param sample_cbk_magic callback magic of sample or 0
\return error if any
*/
typedef GF_Err (*gf_isom_on_block_out)(void *usr_data, u8 *block, u32 block_size, void *sample_cbk_data, u32 sample_cbk_magic);

/*! Callback function to receive new data blocks, only used in non-fragmented mode:
 -  to patch mdat size
 - to inject moov for GF_ISOM_STORE_FASTSTART mode
\param usr_data user callback, as passed to \ref gf_isom_set_write_callback
\param block data block to write
\param block_size data block size in bytes
\param block_offset offset in file for block to patch
\param is_insert if GF_TRUE, indicates the bytes must be inserted at the given offset. Otherwise bytes are to be replaced
\return error if any
*/
typedef GF_Err (*gf_isom_on_block_patch)(void *usr_data, u8 *block, u32 block_size, u64 block_offset, Bool is_insert);

/*! Callback function to indicate the last call to \ref gf_isom_on_block_out is about to be produced for a segment, unused for non-fragmented or non-dash cases
 \param usr_data user callback, as passed to \ref gf_isom_set_write_callback
*/
typedef void (*gf_isom_on_last_block_start)(void *usr_data);

/*! sets write callback functions for in-memory file writing
\param isom_file the target ISO file
\param on_block_out the block write callback function, mandatory
\param on_block_patch the block patch callback function, may be NULL if only fragmented files or very small files are being produced
\param on_last_block_start called before writing the last block of a sequence of movie fragments
\param usr_data opaque user data passed to callback functions
\param block_size desired block size in bytes
\return error if any
*/
GF_Err gf_isom_set_write_callback(GF_ISOFile *isom_file,
			gf_isom_on_block_out on_block_out,
			gf_isom_on_block_patch on_block_patch,
			gf_isom_on_last_block_start on_last_block_start,
 			void *usr_data,
 			u32 block_size);

/*! checks if file will use in-place rewriting or not
\param isom_file the target ISO file
\return GF_TRUE if in-place rewrite is used, GF_FALSE otherwise
*/
Bool gf_isom_is_inplace_rewrite(GF_ISOFile *isom_file);

/*! Disables inplace rewrite. Once in-place rewrite  is disabled, the file can no longer be rewrittten in place.

 In-place rewriting allows editing the file structure (ftyp, moov and meta boxes) without modifying the media data size.

 In-place rewriting is disabled for any of the following:
 - specifying a storage mode using  \ref gf_isom_set_storage_mode
 - removing or adding tracks or items
 - removing, adding or updating samples
 - using stdout, redirect file "_gpac_isobmff_redirect",  memory file " gmem://"

In-place rewriting is enabled by default on files open in edit mode.

\param isom_file the target ISO file
*/
void gf_isom_disable_inplace_rewrite(GF_ISOFile *isom_file);

/*! sets amount of bytes to reserve after moov for future in-place editing. This may be ignored depending on the final write mode
\param isom_file the target ISO file
\param padding amount of bytes to reserve
\return error if any
*/
GF_Err gf_isom_set_inplace_padding(GF_ISOFile *isom_file, u32 padding);

/*! @} */

#endif // GPAC_DISABLE_ISOM_WRITE

/*!
\addtogroup isomp4sys_grp ISOBMFF MPEG-4 Systems
\ingroup iso_grp

MPEG-4 Systems extensions
@{
*/


/*! MPEG-4 ProfileAndLevel codes*/
typedef enum
{
	/*! Audio PL*/
	GF_ISOM_PL_AUDIO,
	/*! Visual PL*/
	GF_ISOM_PL_VISUAL,
	/*! Graphics PL*/
	GF_ISOM_PL_GRAPHICS,
	/*! Scene PL*/
	GF_ISOM_PL_SCENE,
	/*! OD PL*/
	GF_ISOM_PL_OD,
	/*! MPEG-J PL*/
	GF_ISOM_PL_MPEGJ,
	/*! not a profile, just set/unset inlineFlag*/
	GF_ISOM_PL_INLINE,
} GF_ISOProfileLevelType;

/*! gets MPEG-4 subtype of a sample description entry (eg, mp4a, mp4v, enca, encv, resv, etc...)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\return the media type FOUR CHAR code type of an MPEG4 media, or 0 if not MPEG-4 subtype
 */
u32 gf_isom_get_mpeg4_subtype(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! fetches the root OD of a file  (can be NULL, OD or IOD, you have to check its tag)
\param isom_file the target ISO file
\return the OD/IOD if any. Caller must destroy the descriptor
*/
GF_Descriptor *gf_isom_get_root_od(GF_ISOFile *isom_file);

/*! disable OD conversion from ISOM internal to regular OD tags
\param isom_file the target ISO file
\param disable if TRUE, ODs and ESDs will not be converted
*/
void gf_isom_disable_odf_conversion(GF_ISOFile *isom_file, Bool disable);

/*! checks the presence of a track in rood OD/IOD
\param isom_file the target ISO file
\param trackNumber the target track
\return 0: NO, 1: YES, 2: ERROR*/
u8 gf_isom_is_track_in_root_od(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the GF_ESD given the sampleDescriptionIndex
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\return the ESD associated to the sample description index, or NULL if error or not supported. Caller must destroy the ESD*/
GF_ESD *gf_isom_get_esd(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets the decoderConfigDescriptor given the sampleDescriptionIndex
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\return the decoder configuration descriptor associated to the sample description index, or NULL if error or not supported. Caller must destroy the descriptor
*/
GF_DecoderConfig *gf_isom_get_decoder_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! sets default TrackID (or ES_ID) for clock references.
\param isom_file the target ISO file
\param trackNumber the target track to set as a clock reference. If 0, default sync track ID is reseted and will be reassigned at next ESD fetch*/
void gf_isom_set_default_sync_track(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the profile and level value for MPEG-4 streams
\param isom_file the target ISO file
\param PL_Code the target profile to query file
\return the profile and level value, 0xFF if not defined
*/
u8 gf_isom_get_pl_indication(GF_ISOFile *isom_file, GF_ISOProfileLevelType PL_Code);

/*! finds the first ObjectDescriptor using the given track by inspecting all OD tracks
\param isom_file the target ISO file
\param trackNumber the target track
\return the OD ID if dound, 0 otherwise*/
u32 gf_isom_find_od_id_for_track(GF_ISOFile *isom_file, u32 trackNumber);

/*! sets a profile and level indication for the movie iod (created if needed)
\note Use for MPEG-4 Systems only
if the flag is ProfileLevel is 0 this means the movie doesn't require
the specific codec (equivalent to 0xFF value in MPEG profiles)
\param isom_file the target ISO file
\param PL_Code the profile and level code to set
\param ProfileLevel the profile and level value to set
\return error if any
*/
GF_Err gf_isom_set_pl_indication(GF_ISOFile *isom_file, GF_ISOProfileLevelType PL_Code, u8 ProfileLevel);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! sets the rootOD ID of the movie if you need it. By default, movies are created without root ODs
\note Use for MPEG-4 Systems only
\param isom_file the target ISO file
\param OD_ID ID to assign to the root OD/IOD
\return error if any
*/
GF_Err gf_isom_set_root_od_id(GF_ISOFile *isom_file, u32 OD_ID);

/*! sets the rootOD URL of the movie if you need it (only needed to create an empty file pointing
to external resource)
\note Use for MPEG-4 Systems only
\param isom_file the target ISO file
\param url_string the URL to assign to the root OD/IOD
\return error if any
*/
GF_Err gf_isom_set_root_od_url(GF_ISOFile *isom_file, const char *url_string);

/*! removes the root OD
\note Use for MPEG-4 Systems only
\param isom_file the target ISO file
\return error if any
*/
GF_Err gf_isom_remove_root_od(GF_ISOFile *isom_file);

/*! adds a system descriptor to the OD of the movie
\note Use for MPEG-4 Systems only
\param isom_file the target ISO file
\param theDesc the descriptor to add
\return error if any
*/
GF_Err gf_isom_add_desc_to_root_od(GF_ISOFile *isom_file, const GF_Descriptor *theDesc);

/*! adds a track to the root OD
\note Use for MPEG-4 Systems only
\param isom_file the target ISO file
\param trackNumber the track to add to the root OD
\return error if any
*/
GF_Err gf_isom_add_track_to_root_od(GF_ISOFile *isom_file, u32 trackNumber);

/*! removes a track to the root OD
\note Use for MPEG-4 Systems only
\param isom_file the target ISO file
\param trackNumber the track to remove from the root OD
\return error if any
*/
GF_Err gf_isom_remove_track_from_root_od(GF_ISOFile *isom_file, u32 trackNumber);


/*! creates a new MPEG-4 sample description in a track

\note Used for MPEG-4 Systems, AAC and MPEG-4 Visual (part 2)

\param isom_file the target ISO file
\param trackNumber the target track number
\param esd the ESD to use for that sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to index of the new sample description
\return error if any
*/
GF_Err gf_isom_new_mpeg4_description(GF_ISOFile *isom_file, u32 trackNumber, const GF_ESD *esd, const char *URLname, const char *URNname, u32 *outDescriptionIndex);

/*! changes an MPEG-4 sample description
\note Used for MPEG-4 Systems, AAC and MPEG-4 Visual (part 2)
\warning This will replace the whole ESD
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description
\param newESD the new ESD to use for that sample description
\return error if any
*/
GF_Err gf_isom_change_mpeg4_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, const GF_ESD *newESD);

/*! adds an MPEG-4 systems descriptor to the ESD of a sample description
\note Used for MPEG-4 Systems, AAC and MPEG-4 Visual (part 2)
\warning This will replace the whole ESD
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description
\param theDesc the descriptor to add to the ESD of the sample description
\return error if any
*/
GF_Err gf_isom_add_desc_to_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, const GF_Descriptor *theDesc);

/*! clones IOD PLs from orig to dest if any
\param orig_file the source ISO file
\param dest_file the destination ISO file
\return error if any
*/
GF_Err gf_isom_clone_pl_indications(GF_ISOFile *orig_file, GF_ISOFile *dest_file);

/*deletes chapter (1-based index, index 0 for all)*/
GF_Err gf_isom_remove_chapter(GF_ISOFile *the_file, u32 trackNumber, u32 index);

/*! associates a given SL config with a given ESD while extracting the OD information
This is useful while reading the IOD / OD stream of an MP4 file. Note however that
only full AUs are extracted, therefore the calling application must SL-packetize the streams

\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex set to the sample description index corresponding to this sample (optional, can be NULL)
\param slConfig the SL configuration descriptor to set. The descriptor is copied by the API for further use. A NULL pointer will result
in using the default SLConfig (predefined = 2) remapped to predefined = 0
\return error if any
*/
GF_Err gf_isom_set_extraction_slc(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, const GF_SLConfig *slConfig);

#endif //GPAC_DISABLE_ISOM_WRITE


/*! @} */

/*!
\addtogroup isostsd_grp ISOBMFF Sample Descriptions
\ingroup iso_grp

Sample Description functions are used to query and set codec parameters of a track

@{
*/

/*! Unknown sample description*/
typedef struct
{
	/*! codec tag is the containing box's tag, 0 if UUID is used*/
	u32 codec_tag;
	/*! entry UUID if no tag is used*/
	bin128 UUID;
	/*! codec version*/
	u16 version;
	/*! codec revision*/
	u16 revision;
	/*! vendor four character code*/
	u32 vendor_code;

	/*! temporal quality, video codecs only*/
	u32 temporal_quality;
	/*! spatial quality, video codecs only*/
	u32 spatial_quality;
	/*! width in pixels, video codecs only*/
	u16 width;
	/*! height in pixels, video codecs only*/
	u16 height;
	/*! horizontal resolution as 16.16 fixed point, video codecs only*/
	u32 h_res;
	/*! vertical resolution as 16.16 fixed point, video codecs only*/
	u32 v_res;
	/*! bit depth resolution in bits, video codecs only*/
	u16 depth;
	/*! color table, video codecs only*/
	u16 color_table_index;
	/*! compressor name, video codecs only*/
	char compressor_name[33];

	/*! sample rate, audio codecs only*/
	u32 samplerate;
	/*! number of channels, audio codecs only*/
	u16 nb_channels;
	/*! bits per sample, audio codecs only*/
	u16 bits_per_sample;
	/*! indicates if QTFF signaling should be used, audio codecs only*/
	Bool is_qtff;
	/*! for lpcm only, indicates format flags*/
	u32 lpcm_flags;

	/*optional, sample description specific configuration*/
	u8 *extension_buf;
	/*optional, sample description specific size*/
	u32 extension_buf_size;
	/*optional, wraps sample description specific data into a box if given type*/
	u32 ext_box_wrap;
} GF_GenericSampleDescription;

/*! gets an unknown sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\return generic sample description information, or NULL if error
*/
GF_GenericSampleDescription *gf_isom_get_generic_sample_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets the decoder configuration of a JP2 file
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\param out_dsi set to the decoder configuration - shall be freed by user
\param out_size set to the decoder configuration size
\return error if any
*/
GF_Err gf_isom_get_jp2_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u8 **out_dsi, u32 *out_size);



/*! gets RVC (Reconvigurable Video Coding) config of a track for a given sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index (1-based)
\param rvc_predefined set to a predefined value of RVC
\param data set to the RVC config buffer if not predefined, NULL otherwise
\param size set to the RVC config buffer size
\param mime set to the associated mime type of the stream
\return error if any*/
GF_Err gf_isom_get_rvc_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u16 *rvc_predefined, u8 **data, u32 *size, const char **mime);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! sets the RVC config for the given track sample description
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description index
\param rvc_predefined the predefined RVC configuration code, 0 if not predefined
\param mime the associated mime type of the video
\param data the RVC configuration data; ignored if rvc_predefined is not 0
\param size the size of the RVC configuration data; ignored if rvc_predefined is not 0
\return error if any
*/
GF_Err gf_isom_set_rvc_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u16 rvc_predefined, char *mime, u8 *data, u32 size);


/*! updates fields of given visual sample description - these fields are reserved in ISOBMFF, this should only be used for QT, see QTFF
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description
\param revision revision of the sample description format
\param vendor four character code of the vendor
\param temporalQ temporal quality
\param spatialQ spatial quality
\param horiz_res horizontal resolution as 16.16 fixed point number
\param vert_res vertical resolution as 16.16 fixed point number
\param frames_per_sample number of frames per media samples
\param compressor_name human readable name for the compressor
\param color_table_index color table index, use -1 if no color table (most common case)
\return error if any
*/
GF_Err gf_isom_update_video_sample_entry_fields(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u16 revision, u32 vendor, u32 temporalQ, u32 spatialQ, u32 horiz_res, u32 vert_res, u16 frames_per_sample, const char *compressor_name, s16 color_table_index);

/*! updates a sample description from a serialized sample description box. Only child boxes are removed in the process
\param isom_file the target ISO file
\param trackNumber the target track number
\param sampleDescriptionIndex the target sample description
\param data a serialized sample description box
\param size size of the serialized sample description
\return error if any
*/
GF_Err gf_isom_update_sample_description_from_template(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u8 *data, u32 size);


/*! creates a new unknown StreamDescription in the file.
\note use this to store media not currently supported by the ISO media format or media types not implemented in this library
\param isom_file the target ISO file
\param trackNumber the target track
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param udesc generic sample description information to use
\param outDescriptionIndex set to index of the new sample description
\return error if any
*/
GF_Err gf_isom_new_generic_sample_description(GF_ISOFile *isom_file, u32 trackNumber, const char *URLname, const char *URNname, GF_GenericSampleDescription *udesc, u32 *outDescriptionIndex);

/*! clones a sample description without inspecting media types
\param isom_file the destination ISO file
\param trackNumber the destination track
\param orig_file the source ISO file
\param orig_track the source track
\param orig_desc_index the source sample description to clone
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to index of the new sample description
\return error if any
*/
GF_Err gf_isom_clone_sample_description(GF_ISOFile *isom_file, u32 trackNumber, GF_ISOFile *orig_file, u32 orig_track, u32 orig_desc_index, const char *URLname, const char *URNname, u32 *outDescriptionIndex);

/*! gets the sample description template of a track. This serializes sample description box
\param isom_file the destination ISO file
\param trackNumber the destination track
\param sampleDescriptionIndex the target sample description
\param output will be set to a newly allocated buffer containing the serialized box - caller shall free it
\param output_size will be set to the size of the allocated buffer
\return error if any
*/
GF_Err gf_isom_get_stsd_template(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u8 **output, u32 *output_size);

#endif // GPAC_DISABLE_ISOM_WRITE

/*! checks if sample descriptions are the same. This does include self-contained checking and reserved flags. The specific media cfg (DSI & co) is not analysed, only a memory comparaison is done
\param f1 the first ISO file
\param tk1 the first track
\param sdesc_index1 the first sample description
\param f2 the second ISO file
\param tk2 the second track
\param sdesc_index2 the second sample description
\return GF_TRUE if sample descriptions match, GF_FALSE otherwise
*/
Bool gf_isom_is_same_sample_description(GF_ISOFile *f1, u32 tk1, u32 sdesc_index1, GF_ISOFile *f2, u32 tk2, u32 sdesc_index2);


/*! Generic 3GP/3GP2 config record*/
typedef struct
{
	/*GF_4CC record type, one fo the above GF_ISOM_SUBTYPE_3GP_ * subtypes*/
	u32 type;
	/*4CC vendor name*/
	u32 vendor;
	/*codec version*/
	u8 decoder_version;
	/*number of sound frames per IsoMedia sample, >0 and <=15. The very last sample may contain less frames. */
	u8 frames_per_sample;
	/*H263 ONLY - Level*/
	u8 H263_level;
	/*H263 Profile*/
	u8 H263_profile;
	/*AMR(WB) ONLY - num of mode for the codec*/
	u16 AMR_mode_set;
	/*AMR(WB) ONLY - changes in codec mode per sample*/
	u8 AMR_mode_change_period;
} GF_3GPConfig;


/*! gets a 3GPP sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the 3GP config for this sample description, NULL if not a 3GPP track
*/
GF_3GPConfig *gf_isom_3gp_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);
#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates a 3GPP sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param config the 3GP config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_3gp_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_3GPConfig *config, const char *URLname, const char *URNname, u32 *outDescriptionIndex);
/*! updates the 3GPP config - subtypes shall NOT differ
\param isom_file the target ISO file
\param trackNumber the target track
\param config the 3GP config for this sample description
\param sampleDescriptionIndex the target sample description index
\return error if any
*/
GF_Err gf_isom_3gp_config_update(GF_ISOFile *isom_file, u32 trackNumber, GF_3GPConfig *config, u32 sampleDescriptionIndex);
#endif	/*GPAC_DISABLE_ISOM_WRITE*/


/*! gets AVC config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the AVC config - user is responsible for deleting it
*/
GF_AVCConfig *gf_isom_avc_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);
/*! gets SVC config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the SVC config - user is responsible for deleting it
*/
GF_AVCConfig *gf_isom_svc_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);
/*! gets MVC config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the SVC config - user is responsible for deleting it
*/
GF_AVCConfig *gf_isom_mvc_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! AVC familiy type*/
typedef enum
{
	/*! not an AVC codec*/
	GF_ISOM_AVCTYPE_NONE=0,
	/*! AVC only*/
	GF_ISOM_AVCTYPE_AVC_ONLY,
	/*! AVC+SVC in same track*/
	GF_ISOM_AVCTYPE_AVC_SVC,
	/*! SVC only*/
	GF_ISOM_AVCTYPE_SVC_ONLY,
	/*! AVC+MVC in same track*/
	GF_ISOM_AVCTYPE_AVC_MVC,
	/*! SVC only*/
	GF_ISOM_AVCTYPE_MVC_ONLY,
} GF_ISOMAVCType;

/*! gets the AVC family type for a sample description
\param isom_file the target ISO file
\param trackNumber the target hint track
\param sampleDescriptionIndex the target sample description index
\return the type of AVC media
*/
GF_ISOMAVCType gf_isom_get_avc_svc_type(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! HEVC family type*/
typedef enum
{
	/*! not an HEVC codec*/
	GF_ISOM_HEVCTYPE_NONE=0,
	/*! HEVC only*/
	GF_ISOM_HEVCTYPE_HEVC_ONLY,
	/*! HEVC+LHVC in same track*/
	GF_ISOM_HEVCTYPE_HEVC_LHVC,
	/*! LHVC only*/
	GF_ISOM_HEVCTYPE_LHVC_ONLY,
} GF_ISOMHEVCType;

/*! gets the HEVC family type for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the type of HEVC media
*/
GF_ISOMHEVCType gf_isom_get_hevc_lhvc_type(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets HEVC config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the HEVC config - user is responsible for deleting it
*/
GF_HEVCConfig *gf_isom_hevc_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);
/*! gets LHVC config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the LHVC config - user is responsible for deleting it
*/
GF_HEVCConfig *gf_isom_lhvc_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! VVC family type*/
typedef enum
{
	/*! not an VVC codec*/
	GF_ISOM_VVCTYPE_NONE=0,
	/*! VVC only*/
	GF_ISOM_VVCTYPE_ONLY,
	/*! VVC subpicture track*/
	GF_ISOM_VVCTYPE_SUBPIC,
	/*! VVC non-VCL only*/
	GF_ISOM_VVCTYPE_NVCL,
} GF_ISOMVVCType;

/*! gets the VVC family type for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the type of VVC media
*/
GF_ISOMVVCType gf_isom_get_vvc_type(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets VVC config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the VVC config - user is responsible for deleting it
*/
GF_VVCConfig *gf_isom_vvc_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets AV1 config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the AV1 config - user is responsible for deleting it
*/
GF_AV1Config *gf_isom_av1_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets VP8/9 config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the VP8/9 config - user is responsible for deleting it
*/
GF_VPConfig *gf_isom_vp_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets DOVI config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the DOVI config - user is responsible for deleting it
*/
GF_DOVIDecoderConfigurationRecord* gf_isom_dovi_config_get(GF_ISOFile* isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! checks if some tracks in file needs layer reconstruction
\param isom_file the target ISO file
\return GF_TRUE if track dependencies implying extractors or implicit reconstruction are found, GF_FALSE otherwise
*/
Bool gf_isom_needs_layer_reconstruction(GF_ISOFile *isom_file);

/*! NALU extract modes and flags*/
typedef enum
{
	/*! all extractors are rewritten*/
	GF_ISOM_NALU_EXTRACT_DEFAULT = 0,
	/*! all extractors are skipped but NALU data from this track is kept*/
	GF_ISOM_NALU_EXTRACT_LAYER_ONLY,
	/*! all extractors are kept (untouched sample) - used for dumping modes*/
	GF_ISOM_NALU_EXTRACT_INSPECT,
	/*! above mode is applied and PPS/SPS/... are appended in the front of every IDR*/
	GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG = 1<<16,
	/*! above mode is applied and all start codes are rewritten (xPS inband as well)*/
	GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG = 2<<17,
	/*! above mode is applied and VDRD NAL unit is inserted before SVC slice*/
	GF_ISOM_NALU_EXTRACT_VDRD_FLAG = 1<<18,
	/*! all extractors are skipped and only tile track data is kept*/
	GF_ISOM_NALU_EXTRACT_TILE_ONLY = 1<<19
} GF_ISONaluExtractMode;

/*! sets the NALU extraction mode for this track
\param isom_file the target ISO file
\param trackNumber the target track
\param nalu_extract_mode the NALU extraction mode to set
\return error if any
*/
GF_Err gf_isom_set_nalu_extract_mode(GF_ISOFile *isom_file, u32 trackNumber, GF_ISONaluExtractMode nalu_extract_mode);
/*! gets the NALU extraction mode for this track
\param isom_file the target ISO file
\param trackNumber the target track
\return the NALU extraction mode used
*/
GF_ISONaluExtractMode gf_isom_get_nalu_extract_mode(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the composition offset shift if any for track using negative ctts
\param isom_file the target ISO file
\param trackNumber the target track
\return the composition offset shift or 0
*/
s32 gf_isom_get_composition_offset_shift(GF_ISOFile *isom_file, u32 trackNumber);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates a new AVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param cfg the AVC config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_avc_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_AVCConfig *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex);
/*! updates an AVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index to update
\param cfg the AVC config for this sample description
\return error if any
*/
GF_Err gf_isom_avc_config_update(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_AVCConfig *cfg);

/*! creates a new SVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param cfg the SVC config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_svc_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_AVCConfig *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex);
/*! updates an SVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index to update
\param cfg the AVC config for this sample description
\param is_additional if set, the SVCConfig will be added to the AVC sample description, otherwise the sample description will be SVC-only
\return error if any
*/
GF_Err gf_isom_svc_config_update(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_AVCConfig *cfg, Bool is_additional);
/*! deletes an SVC sample description
\warning Associated samples if any are NOT deleted
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index to delete
\return error if any
*/
GF_Err gf_isom_svc_config_del(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! creates a new MVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param cfg the SVC config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_mvc_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_AVCConfig *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex);

/*! updates an MVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index to update
\param cfg the AVC config for this sample description
\param is_additional if set, the MVCConfig will be added to the AVC sample description, otherwise the sample description will be MVC-only
\return error if any
*/
GF_Err gf_isom_mvc_config_update(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_AVCConfig *cfg, Bool is_additional);

/*! deletes an MVC sample description
\warning Associated samples if any are NOT deleted
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index to delete
\return error if any
*/
GF_Err gf_isom_mvc_config_del(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! sets avc3 entry type (inband SPS/PPS) instead of avc1 (SPS/PPS in avcC box)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param keep_xps if set to GF_TRUE, keeps parameter set in the configuration record otherwise removes them
\return error if any
*/
GF_Err gf_isom_avc_set_inband_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, Bool keep_xps);

/*! sets hev1 entry type (inband SPS/PPS) instead of hvc1 (SPS/PPS in hvcC box)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param keep_xps if set to GF_TRUE, keeps parameter set in the configuration record otherwise removes them
\return error if any
*/
GF_Err gf_isom_hevc_set_inband_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, Bool keep_xps);

/*! sets lhe1 entry type instead of lhc1 but keep lhcC box intact
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return error if any
*/
GF_Err gf_isom_lhvc_force_inband_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! sets hvt1 entry type (tile track) or hev2/hvc2 type if is_base_track is set. It is the use responsibility to set the tbas track reference to the base hevc track
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param cfg may be set to the tile track configuration to indicate sub-profile of the tile, or NULL
\param is_base_track if set to GF_TRUE, indicates this is a tile base track, otherwise this is a tile track
\return error if any
*/
GF_Err gf_isom_hevc_set_tile_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_HEVCConfig *cfg, Bool is_base_track);

/*! creates a new HEVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param cfg the HEVC config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_hevc_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_HEVCConfig *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex);

/*! updates an HEVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index to update
\param cfg the HEVC config for this sample description
\return error if any
*/
GF_Err gf_isom_hevc_config_update(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_HEVCConfig *cfg);

/*! Updates L-HHVC config*/
typedef enum {
	//! changes track type to LHV1/LHE1: no base nor extractors in track, just enhancement layers
	GF_ISOM_LEHVC_ONLY = 0,
	//! changes track type to HVC2/HEV2: base and extractors/enh. in track
	GF_ISOM_LEHVC_WITH_BASE,
	//! changes track type to HVC1/HEV1 with additional cfg: base and enh. in track no extractors
	GF_ISOM_LEHVC_WITH_BASE_BACKWARD,
	//! changes track type to HVC2/HEV2 for tile base tracks
	GF_ISOM_HEVC_TILE_BASE,
} GF_ISOMLHEVCTrackType;

/*! updates an HEVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index to update
\param cfg the LHVC config for this sample description
\param track_type indicates the LHVC track type to set
\return error if any
*/
GF_Err gf_isom_lhvc_config_update(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_HEVCConfig *cfg, GF_ISOMLHEVCTrackType track_type);

/*! sets nalu size length
\warning Any previously added samples must be rewritten by the caller
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index to update
\param nalu_size_length the new NALU size length in bytes
\return error if any
*/
GF_Err gf_isom_set_nalu_length_field(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 nalu_size_length);


/*! creates a new VVC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param cfg the VVC config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_vvc_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_VVCConfig *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex);

/*! sets vvi1 entry type (inband SPS/PPS) instead of vvc1 (SPS/PPS in hvcC box)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param keep_xps if set to GF_TRUE, keeps parameter set in the configuration record otherwise removes them
\return error if any
*/
GF_Err gf_isom_vvc_set_inband_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, Bool keep_xps);

/*! updates vvcC configuration
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param cfg new config to set
\return error if any
*/
GF_Err gf_isom_vvc_config_update(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_VVCConfig *cfg);

/*! creates new VPx config
\param isom_file the target ISO file
\param trackNumber the target track
\param cfg the VPx config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\param vpx_type four character code of entry ('vp08', 'vp09' or 'vp10')
\return error if any
*/
GF_Err gf_isom_vp_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_VPConfig *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex, u32 vpx_type);


/*! creates new AV1 config
\param isom_file the target ISO file
\param trackNumber the target track
\param cfg the AV1 config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_av1_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_AV1Config *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex);


#endif /*GPAC_DISABLE_ISOM_WRITE*/


/*! Sample entry description for 3GPP DIMS*/
typedef struct
{
	/*! profile*/
	u8 profile;
	/*! level*/
	u8 level;
	/*! number of components in path*/
	u8 pathComponents;
	/*! full request*/
	Bool fullRequestHost;
	/*! stream type*/
	Bool streamType;
	/*! has redundant sample (carousel)*/
	u8 containsRedundant;
	/*! text encoding string*/
	const char *textEncoding;
	/*! content encoding string*/
	const char *contentEncoding;
	/*! script string*/
	const char *content_script_types;
	/*! mime type string*/
	const char *mime_type;
	/*! xml schema location string*/
	const char *xml_schema_loc;
} GF_DIMSDescription;

/*! gets a DIMS sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param desc set to the DIMS description
\return error if any
*/
GF_Err gf_isom_get_dims_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_DIMSDescription *desc);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates a DIMS sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param desc the DIMS config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_new_dims_description(GF_ISOFile *isom_file, u32 trackNumber, GF_DIMSDescription *desc, const char *URLname, const char *URNname, u32 *outDescriptionIndex);
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/*! gets a UDTS Specific Configuration sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param cfg set to the UDTS Specific Configuration
\return error if any
*/
GF_Err gf_isom_get_udts_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_UDTSConfig *cfg);


/*! gets an AC3 sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return AC-3 config
*/
GF_AC3Config *gf_isom_ac3_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates an AC3 or EAC3 sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param cfg the AC3 config for this sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_ac3_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_AC3Config *cfg, const char *URLname, const char *URNname, u32 *outDescriptionIndex);

/*! updates an AC3 or EAC3 sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param cfg the AC3 config for this sample description
\return error if any
*/
GF_Err gf_isom_ac3_config_update(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_AC3Config *cfg);

#endif /*GPAC_DISABLE_ISOM_WRITE*/

/*! gets TrueHD  sample description info
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param format_info set to the format info - may be NULL
\param peak_data_rate set to the peak data rate info - may be NULL
\return error if any
*/
GF_Err gf_isom_truehd_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *format_info, u32 *peak_data_rate);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates a FLAC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param format_info TrueHD format info
\param peak_data_rate TrueHD peak data rate
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_truehd_config_new(GF_ISOFile *isom_file, u32 trackNumber, char *URLname, char *URNname, u32 format_info, u32 peak_data_rate, u32 *outDescriptionIndex);
#endif

/*! gets a FLAC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param dsi set to the flac decoder config - shall be freeed by caller
\param dsi_size set to the size of the flac decoder config
\return error if any
*/
GF_Err gf_isom_flac_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u8 **dsi, u32 *dsi_size);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates a FLAC sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param metadata the flac decoder config buffer
\param metadata_size the size of flac decoder config
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_flac_config_new(GF_ISOFile *isom_file, u32 trackNumber, u8 *metadata, u32 metadata_size, const char *URLname, const char *URNname, u32 *outDescriptionIndex);
#endif /*GPAC_DISABLE_ISOM_WRITE*/

/*! gets a OPUS  sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param dsi set to the OPUS decoder config (without OpusHead tag), may be NULL - shall be freeed by caller
\param dsi_size set to the size of the OPUS decoder config, may be NULL
\return error if any
*/
GF_Err gf_isom_opus_config_get(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u8 **dsi, u32 *dsi_size);

/*! gets a OPUS  sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param opcfg opus config to get
\return error if any
*/
GF_Err gf_isom_opus_config_get_desc(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_OpusConfig *opcfg);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates a new opus  sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param cfg the opus stream configuration
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_opus_config_new(GF_ISOFile *isom_file, u32 trackNumber, GF_OpusConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex);
#endif

#ifndef GPAC_DISABLE_ISOM_WRITE

/*! creates a motion jpeg 2000 sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\param dsi the jpeg2000 decoder config buffer
\param dsi_len the size of jpeg2000 decoder config
\return error if any
*/
GF_Err gf_isom_new_mj2k_description(GF_ISOFile *isom_file, u32 trackNumber, const char *URLname, const char *URNname, u32 *outDescriptionIndex, u8 *dsi, u32 dsi_len);
#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef GPAC_DISABLE_ISOM_WRITE

/*! creates a time code metadata sample description
\note frames_per_counter_tick<0 disables counter flag but signals frames_per_tick
\param isom_file the target ISO file
\param trackNumber the target track
\param fps_num the frame rate numerator
\param fps_den the frame rate denumerator (frame rate numerator will be track media timescale)
\param frames_per_counter_tick if not 0, enables counter mode (sample data is an counter) and use this value as number of frames per counter tick. Otherwise, disables counter mode (sample data write h,m,s,frames)
\param is_drop indicates that the time code in samples is a drop timecode
\param is_counter indicates that the counter flag should be set
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_tmcd_config_new(GF_ISOFile *isom_file, u32 trackNumber, u32 fps_num, u32 fps_den, s32 frames_per_counter_tick, Bool is_drop, Bool is_counter, u32 *outDescriptionIndex);

#endif /*GPAC_DISABLE_ISOM_WRITE*/

/*! gets information of a time code metadata sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param tmcd_flags set to the timecode description flags
\param tmcd_fps_num set to fps numerator of timecode description
\param tmcd_fps_den set to fps denominator of timecode description
\param tmcd_fpt set to the ticks per second for counter mode (tmcd_flags & 0x1)
\return error if any
*/
GF_Err gf_isom_get_tmcd_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *tmcd_flags, u32 *tmcd_fps_num, u32 *tmcd_fps_den, u32 *tmcd_fpt);

/*! gets information of a raw PCM  sample description, ISOBMFF style
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param flags set to the pcm config flags (0: big endian, 1: little endian), may be NULL
\param pcm_size  set to PCM sample size (per channel, 16, 24, 32, 64, may be NULL
\return error if any
*/
GF_Err gf_isom_get_pcm_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *flags, u32 *pcm_size);

/*! gets information of a raw PCM  sample description, QT style (lpcm codecid)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param sample_rate set to the pcm sample rate, may be NULL
\param nb_channels set to the pcm channel count, may be NULL
\param flags set to the pcm config flags (1: float, 2: big endian, 4: signed, other flags cf QTFF), may be NULL
\param pcm_size  set to PCM sample size (per channel, 16, 24, 32, 64, may be NULL
\return error if any
*/
GF_Err gf_isom_get_lpcm_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, Double *sample_rate, u32 *nb_channels, u32 *flags, u32 *pcm_size);


#ifndef GPAC_DISABLE_ISOM_WRITE

/*! creates a MPHA  sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\param dsi the MPEGH audio config (payload of mhaC box):  byte[0]=1 (config version) ,byte[1]=ProfileLevel,  byte[2]=channel layout, byte[3],byte[4]: the size of what follows the rest being a mpegh3daConfig
\param dsi_size the size of the MPEGH audio config
\param mha_subtype mha1/mha2:/mhm1/mhm2 subtype to use
\return error if any
*/
GF_Err gf_isom_new_mpha_description(GF_ISOFile *isom_file, u32 trackNumber, const char *URLname, const char *URNname, u32 *outDescriptionIndex, u8 *dsi, u32 dsi_size, u32 mha_subtype);
#endif

/*! gets compatible profile list for mpegh entry
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param nb_compatible_profiles set to the number of compatible profiles returned
\return array of compatible profiles, NULL if none found
*/
const u8 *gf_isom_get_mpegh_compatible_profiles(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *nb_compatible_profiles);


#ifndef GPAC_DISABLE_ISOM_WRITE
/*! sets compatible profile list for mpegh entry
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param profiles array of compatible profiles, NULL to remove compatible profiles
\param nb_compatible_profiles  number of compatible profiles in list, 0 to remove compatible profiles
\return error if any
*/
GF_Err gf_isom_set_mpegh_compatible_profiles(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, const u32 *profiles, u32 nb_compatible_profiles);
#endif

/*! structure holding youtube 360 video info
- cf https://github.com/google/spatial-media/blob/master/docs/spherical-video-v2-rfc.md#stereoscopic-3d-video-box-st3d
 */
typedef struct
{
	/*! stereo type  holding youtube 360 video info*/
	u32 stereo_type;
	/*! 0: unknown (not present), 1: cube map, 2: EQR, 3: mesh*/
	u32 projection_type;
	/*! metadata about 3D software creator*/
	const char *meta_data;
	/*! indicate default pause is present*/
	Bool pose_present;
	/*! default pause yaw as 16.16 fixed point*/
	u32 yaw;
	/*! default pause pitch as 16.16 fixed point*/
	u32 pitch;
	/*! default pause roll as 16.16 fixed point*/
	u32 roll;

	/*! cube map layout*/
	u32 layout;
	/*! cube map padding*/
	u32 padding;

	/*! EQR top crop pos in frame, in pixels*/
	u32 top;
	/*! EQR bottom crop pos in frame, in pixels*/
	u32 bottom;
	/*! EQR left crop pos in frame, in pixels*/
	u32 left;
	/*! EQR right crop pos in frame, in pixels*/
	u32 right;

} GF_ISOM_Y3D_Info;


/*! gets youtube 3D/360 info
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param info filled with 3D info
\return error if any, GF_NOT_FOUND if no 3D/360 or setero info
*/
GF_Err gf_isom_get_y3d_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_ISOM_Y3D_Info *info);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! sets youtube 3D/360 info
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param info  3D info to set
\return error if any
*/
GF_Err gf_isom_set_y3d_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_ISOM_Y3D_Info *info);
#endif

/*! @} */


/*!
\addtogroup isofragred_grp Fragmented ISOBMFF Read
\ingroup iso_grp

This describes function specific to fragmented ISOBMF files

@{
*/

/*! checks if a movie file is fragmented
\param isom_file the target ISO file
\return GF_FALSE if movie isn't fragmented, GF_TRUE otherwise
*/
Bool gf_isom_is_fragmented(GF_ISOFile *isom_file);

/*! checks if a movie file is fragmented
\param isom_file the target ISO file
\param TrackID the target track
\return GF_FALSE if track isn't fragmented, GF_TRUE otherwise*/
Bool gf_isom_is_track_fragmented(GF_ISOFile *isom_file, GF_ISOTrackID TrackID);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

/*! checks if a file has a top styp box
\param isom_file the target ISO file
\param brand set to the major brand of the styp box
\param version set to version of the styp box
\return GF_TRUE of the file has a styp box, GF_FALSE otherwise
*/
Bool gf_isom_has_segment(GF_ISOFile *isom_file, u32 *brand, u32 *version);
/*! gets number of movie fragments in the file
\param isom_file the target ISO file
\returns number of movie fragments in the file, 0 if none
*/
u32 gf_isom_segment_get_fragment_count(GF_ISOFile *isom_file);
/*! gets number of track fragments in the indicated movie fragment
\param isom_file the target ISO file
\param moof_index the target movie fragment  (1-based index)
\return number of track fragments, 0 if none
*/
u32 gf_isom_segment_get_track_fragment_count(GF_ISOFile *isom_file, u32 moof_index);
/*! get the track fragment decode time of a track fragment
\param isom_file the target ISO file
\param moof_index the target movie fragment (1-based index)
\param traf_index the target track fragment (1-based index)
\param decode_time set to the track fragment decode time if present, 0 otherwise
\return the track ID of the track fragment
*/
u32 gf_isom_segment_get_track_fragment_decode_time(GF_ISOFile *isom_file, u32 moof_index, u32 traf_index, u64 *decode_time);

/*! get the movie fragment size, i.e. the size of moof, mdat and related boxes before moof/mdat

\param isom_file the target ISO file
\param moof_index the target movie fragment (1-based index)
\param moof_size set to moof box size, may be NULL
\return the movie fragemnt size
*/
u64 gf_isom_segment_get_fragment_size(GF_ISOFile *isom_file, u32 moof_index, u32 *moof_size);

/*! enables single moof mode. In single moof mode, file is parsed only one moof/mdat at a time
   in order to proceed to next moof, \ref gf_isom_reset_data_offset must be called to parse the next moof
\param isom_file the target ISO file
\param mode if GF_TRUE, enables single moof mode; otherwise disables it
*/
void gf_isom_set_single_moof_mode(GF_ISOFile *isom_file, Bool mode);

/*! gets closest file offset for the given time, when the file uses an segment index (sidx)
\param isom_file the target ISO file
\param start_time the start time in seconds
\param offset set to the file offset of the segment containing the desired time
\return error if any
*/
GF_Err gf_isom_get_file_offset_for_time(GF_ISOFile *isom_file, Double start_time, u64 *offset);

/*! gets sidx duration, when the file uses an segment index (sidx)
\param isom_file the target ISO file
\param sidx_dur set to the total duration documented in the segment index
\param sidx_timescale set timescale used to represent the duration in the segment index
\return error if any
*/
GF_Err gf_isom_get_sidx_duration(GF_ISOFile *isom_file, u64 *sidx_dur, u32 *sidx_timescale);


/*! refreshes a fragmented file
A file being downloaded may be a fragmented file. In this case only partial info
is available once the file is successfully open (gf_isom_open_progressive), and since there is
no information wrt number fragments (which could actually be generated on the fly
at the sender side), you must call this function on regular basis in order to
load newly downloaded fragments. Note this may result in Track/Movie duration changes
and SampleCount change too ...

This function should also be called when using memory read (gmem://) to refresh the underlying bitstream after appendin data to your blob.
In the case where the file is not fragmented, no further box parsing will be done.

\param isom_file the target ISO file
\param MissingBytes set to the number of missing bytes to parse the last incomplete top-level box found
\param new_location if set, the previous bitstream is changed to this new location, otherwise it is refreshed (disk flush)
\return error if any
*/
GF_Err gf_isom_refresh_fragmented(GF_ISOFile *isom_file, u64 *MissingBytes, const char *new_location);

/*! gets the current track fragment decode time of the track (the one of the last fragment parsed).
\param isom_file the target ISO file
\param trackNumber the target track
\return the track fragment decode time in media timescale
*/
u64 gf_isom_get_current_tfdt(GF_ISOFile *isom_file, u32 trackNumber);

/*! gets the estimated DTS of the first sample of the next segment for SmoothStreaming files (no tfdt, no tfxd)
\param isom_file the target ISO file
\param trackNumber the target track
\return the next track fragment decode time in media timescale
*/
u64 gf_isom_get_smooth_next_tfdt(GF_ISOFile *isom_file, u32 trackNumber);

/*! checks if the movie is a smooth streaming recomputed initial movie
\param isom_file the target ISO file
\return GF_TRUE if the file init segment (moov) was generated from external meta-data (smooth streaming)
*/
Bool gf_isom_is_smooth_streaming_moov(GF_ISOFile *isom_file);


/*! gets default values of samples in a track to use for track fragments default. Each variable is optional and
if set will contain the default value for this track samples

\param isom_file the target ISO file
\param trackNumber the target track
\param defaultDuration set to the default duration of samples, 0 if not computable
\param defaultSize set to the default size of samples, 0 if not computable
\param defaultDescriptionIndex set to the default sample description index of samples, 0 if not computable
\param defaultRandomAccess set to the default sync flag of samples, 0 if not computable
\param defaultPadding set to the default padding bits of samples, 0 if not computable
\param defaultDegradationPriority set to the default degradation priority of samples, 0 if not computable
\return error if any*/
GF_Err gf_isom_get_fragment_defaults(GF_ISOFile *isom_file, u32 trackNumber,
                                     u32 *defaultDuration, u32 *defaultSize, u32 *defaultDescriptionIndex,
                                     u32 *defaultRandomAccess, u8 *defaultPadding, u16 *defaultDegradationPriority);



/*! gets last UTC/timestamp values indicated for the reference track in the file if any (pfrt box)
\param isom_file the target ISO file
\param refTrackID set to the ID of the reference track used by the pfrt box
\param ntp set to the NTP timestamp found
\param timestamp set to the corresponding media timestamp in refTrackID timescale
\param reset_info if GF_TRUE, discards current NTP mapping info; this will trigger parsing of the next prft box found. If not set, subsequent pfrt boxes will not be parsed until the function is called with reset_info=GF_TRUE
\return GF_FALSE if no info found, GF_TRUE if OK
*/
Bool gf_isom_get_last_producer_time_box(GF_ISOFile *isom_file, GF_ISOTrackID *refTrackID, u64 *ntp, u64 *timestamp, Bool reset_info);

/*! enables storage of traf templates (serialized sidx/moof/traf without trun/senc) at segment boundaries
This is mostly used to recreate identical segment information when refragmenting a file
\param isom_file the target ISO file
*/
void gf_isom_enable_traf_map_templates(GF_ISOFile *isom_file);
/*! get byte range of root sidx if any
\param isom_file the target ISO file
\param start set to start offset (0=first byte) of the root sidx
\param end set to end offset (0 if no sidx) of the root sidx
\return true if success
*/
Bool gf_isom_get_root_sidx_offsets(GF_ISOFile *isom_file, u64 *start, u64 *end);

/*! Segment boundary information*/
typedef struct
{
	/*! fragment start offset*/
	u64 frag_start;
	/*! mdat end offset*/
	u64 mdat_end;
	/*segment start offset plus one:
		0 if regular fragment, 1 if dash segment, offset indicates start of segment (styp or sidx)
		if sidx, it is written in the moof_template
	*/
	u64 seg_start_plus_one;

	/*! serialized array of styp (if present) sidx (if present) and moof with only the current traf*/
	const u8 *moof_template;
	/*! size of serialized buffer*/
	u32 moof_template_size;
	/*! sidx start, 0 if absent*/
	u64 sidx_start;
	/*! sidx end, 0 if absent*/
	u64 sidx_end;
	/*! DTS of first sample in this fragment fotr the queried track*/
	u64 first_dts;
} GF_ISOFragmentBoundaryInfo;

/*! checks if a sample is a fragment start
Only use this function if \ref gf_isom_enable_traf_map_templates has been called
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNum the target sample number
\param frag_info filled with information on fragment boundaries (optional - can be NULL)
\return GF_TRUE if this sample was the first sample of a traf in the fragmented source file, GF_FALSE otherwise*/
Bool gf_isom_sample_is_fragment_start(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNum, GF_ISOFragmentBoundaryInfo *frag_info);

/*! releases current movie segment. This closes the associated file IO object.
\note seeking in the file is no longer possible when tables are rested
\warning The sample count is not reseted after the release of tables. use \ref gf_isom_reset_tables for this

\param isom_file the target ISO file
\param reset_tables if set, sample information for all tracks setup as segment are destroyed, along with all PSSH boxes. This allows keeping the memory footprint low when playing segments.
\return error if any
*/
GF_Err gf_isom_release_segment(GF_ISOFile *isom_file, Bool reset_tables);

#endif //GPAC_DISABLE_ISOM_FRAGMENTS


/*! resets sample information for all tracks setup. This allows keeping the memory footprint low when playing DASH/CMAF segments
\note seeking in the file is then no longer possible
\param isom_file the target ISO file
\param reset_sample_count if GF_TRUE, sets sample count of all tracks back to 0
\return error if any
*/
GF_Err gf_isom_reset_tables(GF_ISOFile *isom_file, Bool reset_sample_count);

/*! sets the offset for parsing from the input buffer to 0 (used to reclaim input buffer)
\param isom_file the target ISO file
\param top_box_start set to the byte offset in the source buffer of the first top level box, may be NULL
\return error if any
*/
GF_Err gf_isom_reset_data_offset(GF_ISOFile *isom_file, u64 *top_box_start);

/*! Flags for gf_isom_open_segment*/
typedef enum
{
	/*! do not check for movie fragment sequence number*/
	GF_ISOM_SEGMENT_NO_ORDER_FLAG = 1,
	/*! the segment contains a scalable layer of the last opened segment*/
	GF_ISOM_SEGMENT_SCALABLE_FLAG = 1<<1,
} GF_ISOSegOpenMode;

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

/*! opens a new segment file. Access to samples in previous segments is no longer possible
if end_range>start_range, restricts the URL to the given byterange when parsing

\param isom_file the target ISO file
\param fileName the file name of the new segment to open
\param start_range the start offset in bytes in the file of the segment data
\param end_range the end offset in bytes in the file of the segment data
\param flags flags to use when opening the segment
\return error if any
*/
GF_Err gf_isom_open_segment(GF_ISOFile *isom_file, const char *fileName, u64 start_range, u64 end_range, GF_ISOSegOpenMode flags);

/*! returns the track ID of the track containing the highest enhancement layer for the given base track
\param isom_file the target ISO file
\param for_base_track the number of the base track
\return the track ID of the highest enahnacement track
*/
GF_ISOTrackID gf_isom_get_highest_track_in_scalable_segment(GF_ISOFile *isom_file, u32 for_base_track);

/*! resets internal info (track fragement decode time, number of samples, next moof number)used with fragments and segment.
\note This should be called when seeking (with keep_sample_count=0) or when loading a media segments with the same timing as the previously loaded segment
\param isom_file the target ISO file
\param keep_sample_count if GF_TRUE, does not reset the sample count on tracks
*/
void gf_isom_reset_fragment_info(GF_ISOFile *isom_file, Bool keep_sample_count);

/*! resets sample count to 0 and next moof number to 0. When doing scalable media, should be called before opening the segment containing
the base layer in order to make sure the sample count base number is always the same (ie 1) on all tracks
\param isom_file the target ISO file
*/
void gf_isom_reset_sample_count(GF_ISOFile *isom_file);
/*! resets moof sequence number to 0
\param isom_file the target ISO file
*/
void gf_isom_reset_seq_num(GF_ISOFile *isom_file);

/*! gets the duration of movie+fragments
\param isom_file the target ISO file
\return the duration in movie timescale, 0 if unknown or if error*/
u64 gf_isom_get_fragmented_duration(GF_ISOFile *isom_file);

/*! gets the number of fragments or segments when the file is opened in \ref GF_ISOM_OPEN_READ_DUMP mode
\param isom_file the target ISO file
\param segments_only if set to GF_TRUE, counts segments (sidx), otherwise counts fragments
\return the number of segments or fragments
*/
u32 gf_isom_get_fragments_count(GF_ISOFile *isom_file, Bool segments_only);

/*! gets total sample number and duration when the file is opened in \ref GF_ISOM_OPEN_READ_DUMP mode
\param isom_file the target ISO file
\param trackID the ID of the target track
\param nb_samples set to the number of samples in the track
\param duration set to the total duration in media timescale
\return error if any
*/
GF_Err gf_isom_get_fragmented_samples_info(GF_ISOFile *isom_file, GF_ISOTrackID trackID, u32 *nb_samples, u64 *duration);

/*! gets the number of the next moof to be produced
\param isom_file the target ISO file
\return number of the next moof
*/
u32 gf_isom_get_next_moof_number(GF_ISOFile *isom_file);

/*! @} */
#endif //GPAC_DISABLE_ISOM_FRAGMENTS


/*!
\addtogroup isoudta_grp ISOBMFF UserData Manipulation
\ingroup iso_grp

				User Data Manipulation

You can add specific typed data to either a track or the movie: the UserData
	The type must be formatted as a FourCC if you have a registered 4CC type
	but the usual is to set a UUID (128 bit ID for box type) which never conflict
	with existing structures in the format
		To manipulate a UUID user data set the UserDataType to 0 and specify a valid UUID.
Otherwise the UUID parameter is ignored
		Several items with the same ID or UUID can be added (this allows you to store any
	kind/number of private information under a unique ID / UUID)

@{
*/

/*! gets number of udta (user data) entries of a movie or track
\param isom_file the target ISO file
\param trackNumber the target track if not 0; if 0, the movie udta is checked
\return the number of entries in UDTA*/
u32 gf_isom_get_udta_count(GF_ISOFile *isom_file, u32 trackNumber);

/*! checks type of a given udta entry
\param isom_file the target ISO file
\param trackNumber the target track if not 0; if 0, the movie udta is checked
\param udta_idx 1-based index of the user data to query
\param UserDataType set to the four character code of the user data entry (optional, can be NULL)
\param UUID set to the UUID of the user data entry (optional, can be NULL)
\return error if any*/
GF_Err gf_isom_get_udta_type(GF_ISOFile *isom_file, u32 trackNumber, u32 udta_idx, u32 *UserDataType, bin128 *UUID);

/*! gets the number of UserDataItems with the same ID / UUID in the desired track or movie
\param isom_file the target ISO file
\param trackNumber the target track if not 0; if 0, the movie udta is checked
\param UserDataType the four character code of the user data entry to query
\param UUID the UUID of the user data entry
\return number of UDTA entries with the given type*/
u32 gf_isom_get_user_data_count(GF_ISOFile *isom_file, u32 trackNumber, u32 UserDataType, bin128 UUID);

/*! gets the UserData for the specified item from the track or the movie
\param isom_file the target ISO file
\param trackNumber the target track if not 0; if 0, the movie udta is checked
\param UserDataType the four character code of the user data entry to query
\param UUID the UUID of the user data entry
\param UserDataIndex 1-based index of the user data of the given type to fetch. If 0, all boxes with type==UserDataType will be serialized (including box header and size) in the output buffer
\param userData set to a newly allocated buffer containing the serialized data - shall be freed by caller, you must pass (userData != NULL && *userData=NULL)
\param userDataSize set to the size of the allocated buffer
\return error if any*/
GF_Err gf_isom_get_user_data(GF_ISOFile *isom_file, u32 trackNumber, u32 UserDataType, bin128 UUID, u32 UserDataIndex, u8 **userData, u32 *userDataSize);

#ifndef GPAC_DISABLE_ISOM_WRITE

/*! adds a user data item in the desired track or in the movie
\param isom_file the target ISO file
\param trackNumber the target track for the user data; if 0, adds user data to the movie
\param UserDataType the user data four character code type
\param UUID the user data UUID
\param data the data to add, may be NULL
\param size the size of the data to add, shall be 0 when data is NULL
\return error if any
*/
GF_Err gf_isom_add_user_data(GF_ISOFile *isom_file, u32 trackNumber, u32 UserDataType, bin128 UUID, u8 *data, u32 size);

/*! removes all user data items from a track or movie
\param isom_file the target ISO file
\param trackNumber the target track for the user data; if 0, adds user data to the movie
\param UserDataType the user data four character code type
\param UUID the user data UUID
\return error if any
*/
GF_Err gf_isom_remove_user_data(GF_ISOFile *isom_file, u32 trackNumber, u32 UserDataType, bin128 UUID);

/*! removes a user data item from a track or movie
use the UDAT read functions to get the item index
\param isom_file the target ISO file
\param trackNumber the target track for the user data; if 0, adds user data to the movie
\param UserDataType the user data four character code type
\param UUID the user data UUID
\param UserDataIndex the 1-based index of the user data item to remove - see \ref gf_isom_get_user_data_count
\return error if any
*/
GF_Err gf_isom_remove_user_data_item(GF_ISOFile *isom_file, u32 trackNumber, u32 UserDataType, bin128 UUID, u32 UserDataIndex);

/*! adds a user data item in a track or movie using a serialzed buffer of ISOBMFF boxes
\param isom_file the target ISO file
\param trackNumber the target track for the udta box; if 0, add the udta to the movie;
\param data the serialized udta box to add, shall not be NULL
\param size the size of the data to add
\return error if any
*/
GF_Err gf_isom_add_user_data_boxes(GF_ISOFile *isom_file, u32 trackNumber, u8 *data, u32 size);

/*! gets serialized user data box of a movie
\param isom_file the destination ISO file
\param output will be set to a newly allocated buffer containing the serialized box - caller shall free it
\param output_size will be set to the size of the allocated buffer
\return error if any
*/
GF_Err gf_isom_get_raw_user_data(GF_ISOFile *isom_file, u8 **output, u32 *output_size);

#endif //GPAC_DISABLE_ISOM_WRITE

/*! @} */


#if !defined(GPAC_DISABLE_ISOM_FRAGMENTS) && !defined(GPAC_DISABLE_ISOM_WRITE)

/*!
\addtogroup isofragwrite_grp Fragmented ISOBMFF Writing
\ingroup iso_grp

			Movie Fragments Writing API
		Movie Fragments is a feature of ISO media files for fragmentation
	of a presentation meta-data and interleaving with its media data.
	This enables faster http fast start for big movies, and also reduces the risk
	of data loss in case of a recording crash, because meta data and media data
	can be written to disk at regular times
		This API provides simple function calls to setup such a movie and write it
	The process implies:
		1- creating a movie in the usual way (track, stream descriptions, (IOD setup
	copyright, ...)
		2- possibly add some samples in the regular fashion
		3- setup track fragments for all track that will be written in a fragmented way
	(note that you can create/write a track that has no fragmentation at all)
		4- finalize the movie for fragmentation (this will flush all meta-data and
	any media-data added to disk, ensuring all vital information for the presentation
	is stored on file and not lost in case of crash/poweroff)

	  then 5-6 as often as desired
		5- start a new movie fragment
		6- add samples to each setup track


  IMPORTANT NOTES:
		* Movie Fragments can only be used in GF_ISOM_OPEN_WRITE mode (capturing)
  and no editing functionalities can be used
		* the fragmented movie API uses TrackID and not TrackNumber

@{
*/

/*! flag indicating default samples are sync*/
#define GF_ISOM_FRAG_DEF_IS_SYNC 1
/*! flag indicating a sync sample table shall be added in the track - cf CMAF rules*/
#define GF_ISOM_FRAG_USE_SYNC_TABLE (1<<1)

/*! sets up a track for fragmentation by specifying some default values for storage efficiency
\note If all the defaults are 0, traf flags will always be used to signal them.
\param isom_file the target ISO file
\param TrackID ID of the target track
\param DefaultSampleDescriptionIndex the default description used by samples in this track
\param DefaultSampleDuration default duration of samples in this track
\param DefaultSampleSize default size of samples in this track (0 if unknown)
\param DefaultSampleSyncFlags combination of GF_ISOM_FRAG_* flags
\param DefaultSamplePadding default padding bits for samples in this track
\param DefaultDegradationPriority default degradation priority for samples in this track
\param force_traf_flags if GF_TRUE, will ignore these default in each traf but will still write them in moov
\return error if any
*/
GF_Err gf_isom_setup_track_fragment(GF_ISOFile *isom_file, GF_ISOTrackID TrackID,
                                    u32 DefaultSampleDescriptionIndex,
                                    u32 DefaultSampleDuration,
                                    u32 DefaultSampleSize,
                                    u8 DefaultSampleSyncFlags,
                                    u8 DefaultSamplePadding,
                                    u16 DefaultDegradationPriority,
									Bool force_traf_flags);

/*! changes the default parameters of an existing trak fragment
\warning This should not be used if samples have already been added

\param isom_file the target ISO file
\param TrackID ID of the target track
\param DefaultSampleDescriptionIndex the default description used by samples in this track
\param DefaultSampleDuration default duration of samples in this track
\param DefaultSampleSize default size of samples in this track (0 if unknown)
\param DefaultSampleIsSync default key-flag (RAP) of samples in this track
\param DefaultSamplePadding default padding bits for samples in this track
\param DefaultDegradationPriority default degradation priority for samples in this track
\param force_traf_flags if GF_TRUE, will ignore these default in each traf but will still write them in moov
\return error if any
*/
GF_Err gf_isom_change_track_fragment_defaults(GF_ISOFile *isom_file, GF_ISOTrackID TrackID,
        u32 DefaultSampleDescriptionIndex,
        u32 DefaultSampleDuration,
        u32 DefaultSampleSize,
        u8 DefaultSampleIsSync,
        u8 DefaultSamplePadding,
        u16 DefaultDegradationPriority,
        u8 force_traf_flags);

/*! flushes data to disk and prepare movie fragmentation
\param isom_file the target ISO file
\param media_segment_type 0 if no segments, 1 if regular segment, 2 if single segment
\param mvex_after_tracks forces writing mvex box after track boxes
\return error if any
*/
GF_Err gf_isom_finalize_for_fragment(GF_ISOFile *isom_file, u32 media_segment_type, Bool mvex_after_tracks);

/*! sets the duration of the movie in case of movie fragments
\param isom_file the target ISO file
\param duration the complete duration (movie and all fragments) in movie timescale
\param remove_mehd force removal of mehd box, only setting mvhd.duration to 0
\return error if any
*/
GF_Err gf_isom_set_movie_duration(GF_ISOFile *isom_file, u64 duration, Bool remove_mehd);

/*! fragment creatio option*/
typedef enum
{
	/*! moof is stored before mdat - will require temporary storage of data in memory*/
	GF_ISOM_FRAG_MOOF_FIRST = 1,
#ifdef GF_ENABLE_CTRN
	/*! use compact fragment syntax*/
	GF_ISOM_FRAG_USE_COMPACT = 1<<1,
#endif
} GF_ISOStartFragmentFlags;
/*! starts a new movie fragment
\param isom_file the target ISO file
\param moof_first if GF_TRUE, the moof will be written before the mdat
\return error if any
*/
GF_Err gf_isom_start_fragment(GF_ISOFile *isom_file, GF_ISOStartFragmentFlags moof_first);

/*! starts a new segment in the file
\param isom_file the target ISO file
\param SegName if not NULL, the output will be written in the SegName file. If NULL, segment will be created in same file as movie. The special name "_gpac_isobmff_redirect" is used to indicate that segment shall be written to a memory buffer passed to callback function set through \ref gf_isom_set_write_callback
\param memory_mode if set, all samples writing is done in memory rather than on disk. Ignored in callback mode
\return error if any
*/
GF_Err gf_isom_start_segment(GF_ISOFile *isom_file, const char *SegName, Bool memory_mode);

/*! sets the baseMediaDecodeTime of the first sample of the given track
\param isom_file the target ISO file
\param TrackID ID of the target track
\param decode_time the decode time in media timescale
\return error if any
*/
GF_Err gf_isom_set_traf_base_media_decode_time(GF_ISOFile *isom_file, GF_ISOTrackID TrackID, u64 decode_time);

/*! enables mfra (movie fragment random access computing) when writing movie fragments
\note this should only be used when generating segments in a single file
\param isom_file the target ISO file
\return error if any
*/
GF_Err gf_isom_enable_mfra(GF_ISOFile *isom_file);

/*! sets Microsoft Smooth Streaming traf 'tfxd' box info, written at the end of each traf
\param isom_file the target ISO file
\param reference_track_ID ID of the reference track giving the media timescale
\param decode_traf_time decode time of the first sample in the segment in media timescale (hardcoded to 10MHz in Smooth)
\param traf_duration duration of all samples in the traf in media timescale (hardcoded to 10MHz in Smooth)
\return error if any
*/
GF_Err gf_isom_set_traf_mss_timeext(GF_ISOFile *isom_file, GF_ISOTrackID reference_track_ID, u64 decode_traf_time, u64 traf_duration);

/*! closes current segment, producing a segment index box if desired
\param isom_file the target ISO file
\param subsegs_per_sidx number of subsegments per sidx box; a negative value disables sidx, 0 forces a single sidx for the segment (or subsegment)
\param referenceTrackID the ID of the track used as a reference for the segment index box
\param ref_track_decode_time the decode time fo the first sample in the reference track for this segment
\param timestamp_shift the constant difference between media time and presentation time (derived from edit list)
\param ref_track_next_cts the CTS of the first sample in the reference track in the next segment
\param daisy_chain_sidx if GF_TRUE, indicates chained sidx shall be used. Otherwise, an array of indexes is used
\param use_ssix if GF_TRUE, produces an ssix box using I-frames as first level and all other frames as second level
\param last_segment indicates if this is the last segment of the session
\param close_segment_handle if set to GF_TRUE, the associated file if any will be closed
\param segment_marker_4cc a four character code used to insert an empty box at the end of the saegment with the given type. If 0, no such box is inserted
\param index_start_range set to the start offset in bytes of the segment in the media file
\param index_end_range set to the end offset in bytes of the segment in the media file
\param out_seg_size set to the segment size in bytes (optional, can be NULL)
\return error if any
*/
GF_Err gf_isom_close_segment(GF_ISOFile *isom_file, s32 subsegs_per_sidx, GF_ISOTrackID referenceTrackID, u64 ref_track_decode_time, s32 timestamp_shift, u64 ref_track_next_cts, Bool daisy_chain_sidx, Bool use_ssix, Bool last_segment, Bool close_segment_handle, u32 segment_marker_4cc, u64 *index_start_range, u64 *index_end_range, u64 *out_seg_size);

/*! writes any pending fragment to file for low-latency output.
\warning This shall only be used if no SIDX is used: subsegs_per_sidx<0 or flushing all fragments before calling \ref gf_isom_close_segment

\param isom_file the target ISO file
\param last_segment indicates if this is the last segment of the session
\return error if any
*/
GF_Err gf_isom_flush_fragments(GF_ISOFile *isom_file, Bool last_segment);

/*! sets fragment prft box info, written just before the moof
\param isom_file the target ISO file
\param reference_track_ID the ID of the track used as a reference for media timestamps
\param ntp absolute NTP time
\param timestamp media time corresponding to the NTP time, in reference track media timescale
\return error if any
*/
GF_Err gf_isom_set_fragment_reference_time(GF_ISOFile *isom_file, GF_ISOTrackID reference_track_ID, u64 ntp, u64 timestamp);

/*! writes an empty sidx in the current movie.

The SIDX will be forced to have nb_segs entries, and nb_segs shall match the number of calls to
\ref gf_isom_close_segment that will follow.
This avoids wasting time and disk space moving data around. Once \ref gf_isom_close_segment has then been called nb_segs times,
the pre-allocated SIDX is destroyed and successive calls to \ref gf_isom_close_segment will create their own sidx, unless gf_isom_allocate_sidx is called again.

\param isom_file the target ISO file
\param subsegs_per_sidx reserved to 0, currently ignored
\param daisy_chain_sidx reserved to 0, currently ignored
\param nb_segs number of entries in the segment index
\param frags_per_segment reserved, currently ignored
\param start_range set to the start offset in bytes of the segment index box
\param end_range set to the end offset in bytes of the segment index box
\param use_ssix if GF_TRUE, produces an ssix box using I-frames as first level and all other frames as second level
\return error if any
*/
GF_Err gf_isom_allocate_sidx(GF_ISOFile *isom_file, s32 subsegs_per_sidx, Bool daisy_chain_sidx, u32 nb_segs, u32 *frags_per_segment, u32 *start_range, u32 *end_range, Bool use_ssix);

/*! sets up track fragment defaults using the given template. The template shall be a serialized array of one or more trex boxes

\param isom_file the target ISO file
\param TrackID ID of the target track
\param boxes serialized array of trex boxes
\param boxes_size size of the serialized array
\param force_traf_flags if GF_TRUE, will ignore these default in each traf but will still write them in moov
\return error if any
*/
GF_Err gf_isom_setup_track_fragment_template(GF_ISOFile *isom_file, GF_ISOTrackID TrackID, u8 *boxes, u32 boxes_size, u8 force_traf_flags);

#ifdef GF_ENABLE_CTRN
/*! enables track fragment inheriting from a given traf.
This shall only be set when the inherited traf shares exactly the same syntax except the sample sizes, this library does not compute which
sample values can be inherited

\param isom_file the target ISO file
\param TrackID ID of the target track
\param BaseTrackID ID of the track from which sample values are inherited in track fragments
\return error if any
*/
GF_Err gf_isom_enable_traf_inherit(GF_ISOFile *isom_file, GF_ISOTrackID TrackID, GF_ISOTrackID BaseTrackID);
#endif

/*! Track fragment options*/
typedef enum
{
	/*! indicates that the track fragment has no samples but still has a duration
	(silence-detection in audio codecs, ...).
	param: indicates duration*/
	GF_ISOM_TRAF_EMPTY,
	/*! I-Frame detection: this can reduce file size by detecting I-frames and
	optimizing sample flags (padding, priority, ..)
	param: on/off (0/1)*/
	GF_ISOM_TRAF_RANDOM_ACCESS,
	/*! activate data cache on track fragment. This is useful when writing interleaved
	media from a live source (typically audio-video), and greatly reduces file size
	param: Number of samples (> 1) to cache before disk flushing. You shouldn't try
	to cache too many samples since this will load your memory. base that on FPS/SR*/
	GF_ISOM_TRAF_DATA_CACHE,
	/*! forces moof base offsets when traf based offsets would be chosen
	param: on/off (0/1)*/
	GF_ISOM_TFHD_FORCE_MOOF_BASE_OFFSET,
	/*! use sdtp box in traf rather than storing sample deps in trun entry. param values are:
		0: disables sdtp
		1: enables sdtp and disables sample dependency flags in trun
		2: enables sdtp and also use sample dependency flags in trun
	*/
	GF_ISOM_TRAF_USE_SAMPLE_DEPS_BOX,
	/*! forces new trun at next sample add
	param: ignored*/
	GF_ISOM_TRUN_FORCE,
	/*! sets interleave group ID of the  next sample add. Samples with lower interleave ID will be stored first, creating new trun whenever a new group is detected
	This will enable data cache
	param: interleave ID*/
	GF_ISOM_TRUN_SET_INTERLEAVE_ID,
	/*! store truns before sample encryption and sample groups info
 	param: 1 to store before and follow CMAF (recommended?) order, 0, to store after*/
	GF_ISOM_TRAF_TRUNS_FIRST,
	/*! forces trun v1
	param: on/off (0/1)*/
	GF_ISOM_TRAF_TRUN_V1,
	/*force usage of 64 bits in tfdt and in per-segment sidx*/
	GF_ISOM_TRAF_USE_LARGE_TFDT
} GF_ISOTrackFragmentOption;

/*! sets a track fragment option. Options can be set at the beginning of each new fragment only, and for the
lifetime of the fragment
\param isom_file the target ISO file
\param TrackID ID of the target track
\param Code the option type to set
\param param the option value
\return error if any
*/
GF_Err gf_isom_set_fragment_option(GF_ISOFile *isom_file, GF_ISOTrackID TrackID, GF_ISOTrackFragmentOption Code, u32 param);

/*! adds a sample to a fragmented track

\param isom_file the target ISO file
\param TrackID destination track
\param sample sample to add
\param sampleDescriptionIndex sample description for this sample. If 0, the default one
is used
\param Duration sample duration; the sample duration MUST be provided at least for the last sample (for intermediate samples, it is recomputed internally by the lib)
\param PaddingBits padding bits for the sample, or 0
\param DegradationPriority for the sample, or 0
\param redundantCoding indicates this is samples acts as a sync shadow point
\return error if any
*/
GF_Err gf_isom_fragment_add_sample(GF_ISOFile *isom_file, GF_ISOTrackID TrackID, const GF_ISOSample *sample,
                                   u32 sampleDescriptionIndex,
                                   u32 Duration, u8 PaddingBits, u16 DegradationPriority, Bool redundantCoding);

/*! appends data into last sample of track for video fragments/other media
\warning This shall not be used with OD tracks
\param isom_file the target ISO file
\param TrackID destination track
\param data the data to append
\param data_size the size of the data to append
\param PaddingBits padding bits for the sample, or 0
\return error if any
*/
GF_Err gf_isom_fragment_append_data(GF_ISOFile *isom_file, GF_ISOTrackID TrackID, u8 *data, u32 data_size, u8 PaddingBits);


/*! sets side information for common encryption for the last added sample
\param isom_file the target ISO file
\param trackID the ID of the target track
\param sai_b buffer containing the SAI information of the sample
\param sai_b_size size of the SAI buffer. If sai_b is NULL or sai_b_size is 0, add a clear SAI data
\param use_subsample indicates if the media uses CENC subsamples
\param use_saio_32bit indicates if 32-bit saio shall be used
\param use_multikey indicates if multikey is in use (required to tag saiz/saio boxes)
\return error if any
*/
GF_Err gf_isom_fragment_set_cenc_sai(GF_ISOFile *isom_file, GF_ISOTrackID trackID, u8 *sai_b, u32 sai_b_size, Bool use_subsample, Bool use_saio_32bit, Bool use_multikey);

#endif // !defined(GPAC_DISABLE_ISOM_FRAGMENTS) && !defined(GPAC_DISABLE_ISOM_WRITE)

#if !defined(GPAC_DISABLE_ISOM_WRITE)

/*! clones PSSH data between two files
\param dst_file the target ISO file
\param src_file the source ISO file
\param in_moof if GF_TRUE, indicates the pssh should be cloned in current moof box
\return error if any
*/
GF_Err gf_isom_clone_pssh(GF_ISOFile *dst_file, GF_ISOFile *src_file, Bool in_moof);

#endif


#if !defined(GPAC_DISABLE_ISOM_FRAGMENTS) && !defined(GPAC_DISABLE_ISOM_WRITE)

/*! sets roll information for a sample in a track fragment
\param isom_file the target ISO file
\param trackID the ID of the target track
\param sample_number the sample number of the last sample
\param roll_type indicate the sample roll type
\param roll_distance set to the roll distance for a roll sample
\return error if any
*/
GF_Err gf_isom_fragment_set_sample_roll_group(GF_ISOFile *isom_file, GF_ISOTrackID trackID, u32 sample_number, GF_ISOSampleRollType roll_type, s16 roll_distance);

/*! sets rap information for a sample in a track fragment
\param isom_file the target ISO file
\param trackID the ID of the target track
\param sample_number_in_frag the sample number of the sample in the traf
\param is_rap set to GF_TRUE to indicate the sample is a RAP sample (open-GOP), GF_FALSE otherwise
\param num_leading_samples set to the number of leading pictures for a RAP sample
\return error if any
*/
GF_Err gf_isom_fragment_set_sample_rap_group(GF_ISOFile *isom_file, GF_ISOTrackID trackID, u32 sample_number_in_frag, Bool is_rap, u32 num_leading_samples);

/*! sets sample dependency flags in a track fragment - see ISO/IEC 14496-12 and \ref gf_filter_pck_set_dependency_flags
\param isom_file the target ISO file
\param trackID the ID of the target track
\param is_leading indicates that the sample is a leading picture
\param dependsOn indicates the sample dependency towards other samples
\param dependedOn indicates the sample dependency from other samples
\param redundant indicates that the sample contains redundant coding
\return error if any
*/
GF_Err gf_isom_fragment_set_sample_flags(GF_ISOFile *isom_file, GF_ISOTrackID trackID, u32 is_leading, u32 dependsOn, u32 dependedOn, u32 redundant);



/*! adds sample auxiliary data

\param isom_file the target ISO file
\param trackID the ID of the target track
\param sample_number_in_frag the sample number in the current fragment. Must be equal or larger to last auxiliary added
\param aux_type auxiliary sample data type, shall not be 0
\param aux_info auxiliary sample data specific info type, may be 0
\param data data to add
\param size size of data to add
\return error if any
*/
GF_Err gf_isom_fragment_set_sample_aux_info(GF_ISOFile *isom_file, u32 trackID, u32 sample_number_in_frag, u32 aux_type, u32 aux_info, u8 *data, u32 size);


/*! sets the number of the next moof to be produced
\param isom_file the target ISO file
\param value the number of the next moof
*/
void gf_isom_set_next_moof_number(GF_ISOFile *isom_file, u32 value);


/*! @} */
#endif// !defined(GPAC_DISABLE_ISOM_FRAGMENTS) && !defined(GPAC_DISABLE_ISOM_WRITE)


/*!
\addtogroup isortp_grp ISOBMFF RTP Hinting
\ingroup iso_grp

@{
*/

/*! supported hint formats - ONLY RTP now*/
typedef enum
{
	/*! RTP hint type*/
	GF_ISOM_HINT_RTP = GF_4CC('r', 't', 'p', ' '),
} GF_ISOHintFormat;

#if !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_ISOM_HINTING)

/*! sets up a hint track based on the hint format
\warning This function MUST be called after creating a new hint track and before any other calls on this track
\param isom_file the target ISO file
\param trackNumber the target hint track
\param HintType the desired hint type
\return error if any
*/
GF_Err gf_isom_setup_hint_track(GF_ISOFile *isom_file, u32 trackNumber, GF_ISOHintFormat HintType);

/*! creates a HintDescription for the HintTrack
\param isom_file the target ISO file
\param trackNumber the target hint track
\param HintTrackVersion version of hint track
\param LastCompatibleVersion last compatible version of hint track
\param Rely flag indicating whether a reliable transport protocol is desired/required
for data transport
	0: not desired (UDP/IP). NB: most RTP streaming servers only support UDP/IP for data
	1: preferable (TCP/IP if possible or UDP/IP)
	2: required (TCP/IP only)
\param HintDescriptionIndex is set to the newly created hint sample description index
\return error if any
*/
GF_Err gf_isom_new_hint_description(GF_ISOFile *isom_file, u32 trackNumber, s32 HintTrackVersion, s32 LastCompatibleVersion, u8 Rely, u32 *HintDescriptionIndex);

/*! starts a new sample for the hint track. A sample is just a collection of packets
the transmissionTime is indicated in the media timeScale of the hint track
\param isom_file the target ISO file
\param trackNumber the target hint track
\param HintDescriptionIndex the target hint sample description index
\param TransmissionTime the target transmission time in hint media timescale
\return error if any
*/
GF_Err gf_isom_begin_hint_sample(GF_ISOFile *isom_file, u32 trackNumber, u32 HintDescriptionIndex, u32 TransmissionTime);

/*! ends an hint sample once all your packets for this sample are done
\param isom_file the target ISO file
\param trackNumber the target hint track
\param IsRandomAccessPoint set to GF_TRUE if you want to indicate that this is a random access point in the stream
\return error if any
*/
GF_Err gf_isom_end_hint_sample(GF_ISOFile *isom_file, u32 trackNumber, u8 IsRandomAccessPoint);


/*!
		PacketHandling functions
		Data can be added at the end or at the beginning of the current packet
		by setting AtBegin to 1 the data will be added at the beginning
		This allows constructing the packet payload before any meta-data
*/

/*! adds a blank chunk of data in the sample that is skipped while streaming
\param isom_file the target ISO file
\param trackNumber the target hint track
\param AtBegin indicates if the blank chunk should be at the end or at the beginning of the hint packet
\return error if any
*/
GF_Err gf_isom_hint_blank_data(GF_ISOFile *isom_file, u32 trackNumber, u8 AtBegin);

/*! adds a chunk of data in the packet that is directly copied while streaming
\note DataLength MUST BE <= 14 bytes, and you should only use this function
to add small blocks of data (encrypted parts, specific headers, ...)
\param isom_file the target ISO file
\param trackNumber the target hint track
\param data buffer to add to the RTP packet
\param dataLength size of buffer to add to the RTP packet
\param AtBegin indicates if the blank chunk should be at the end or at the beginning of the hint packet
\return error if any
*/
GF_Err gf_isom_hint_direct_data(GF_ISOFile *isom_file, u32 trackNumber, u8 *data, u32 dataLength, u8 AtBegin);

/*! adds a reference to some sample data in the packet
\note if you want to reference a previous HintSample in the hintTrack, you will have to parse the sample yourself ...

\param isom_file the target ISO file
\param trackNumber the target hint track
\param SourceTrackID the ID of the track where the referenced sample is
\param SampleNumber the sample number containing the data to be added
\param DataLength the length of bytes to copy in the packet
\param offsetInSample the offset in bytes in the sample at which to begin copying data
\param extra_data only used when the sample is actually the sample that will contain this packet
(useful to store en encrypted version of a packet only available while streaming)
	In this case, set SourceTrackID to the HintTrack ID and SampleNumber to 0
	In this case, the DataOffset MUST BE NULL and length will indicate the extra_data size
\param AtBegin indicates if the blank chunk should be at the end or at the beginning of the hint packet
\return error if any
*/
GF_Err gf_isom_hint_sample_data(GF_ISOFile *isom_file, u32 trackNumber, GF_ISOTrackID SourceTrackID, u32 SampleNumber, u16 DataLength, u32 offsetInSample, u8 *extra_data, u8 AtBegin);


/*! adds a reference to some stream description data in the packet (headers, ...)

\param isom_file the target ISO file
\param trackNumber the target hint track
\param SourceTrackID the ID of the track where the referenced sample is
\param sampleDescriptionIndex the index of the stream description in the desired track
\param DataLength the length of bytes to copy in the packet
\param offsetInDescription the offset in bytes in the description at which to begin copying data. Since it is far from being obvious / interoperable what this offset is, we recommend not using this function and injecting the data instead using \ref gf_isom_hint_direct_data.
\param AtBegin indicates if the blank chunk should be at the end or at the beginning of the hint packet
\return error if any
*/
GF_Err gf_isom_hint_sample_description_data(GF_ISOFile *isom_file, u32 trackNumber, GF_ISOTrackID SourceTrackID, u32 sampleDescriptionIndex, u16 DataLength, u32 offsetInDescription, u8 AtBegin);


/*! creates a new RTP packet in the HintSample. If a previous packet was created,
it is stored in the hint sample and a new packet is created.

\param isom_file the target ISO file
\param trackNumber the target hint track
\param relativeTime RTP time offset of this packet in the HintSample if any - in hint track
time scale. Used for data smoothing by servers.
\param PackingBit the 'P' bit of the RTP packet header
\param eXtensionBit the'X' bit of the RTP packet header
\param MarkerBit the 'M' bit of the RTP packet header
\param PayloadType the payload type, on 7 bits, format 0x0XXXXXXX
\param disposable_packet indicates if this packet can be skipped by a server
\param IsRepeatedPacket indicates if this is a duplicate packet of a previous one and can be skipped by a server
\param SequenceNumber the RTP base sequence number of the packet. Because of support for repeated packets, you have to set the sequence number yourself.
\return error if any
*/
GF_Err gf_isom_rtp_packet_begin(GF_ISOFile *isom_file, u32 trackNumber, s32 relativeTime, u8 PackingBit, u8 eXtensionBit, u8 MarkerBit, u8 PayloadType, u8 disposable_packet, u8 IsRepeatedPacket, u16 SequenceNumber);

/*! sets the flags of the RTP packet
\param isom_file the target ISO file
\param trackNumber the target hint track
\param PackingBit the 'P' bit of the RTP packet header
\param eXtensionBit the'X' bit of the RTP packet header
\param MarkerBit the 'M' bit of the RTP packet header
\param disposable_packet indicates if this packet can be skipped by a server
\param IsRepeatedPacket indicates if this is a duplicate packet of a previous one and can be skipped by a server
\return error if any*/
GF_Err gf_isom_rtp_packet_set_flags(GF_ISOFile *isom_file, u32 trackNumber, u8 PackingBit, u8 eXtensionBit, u8 MarkerBit, u8 disposable_packet, u8 IsRepeatedPacket);

/*! sets the time offset of this packet. This enables packets to be placed in the hint track
in decoding order, but have their presentation time-stamp in the transmitted
packet in a different order. Typically used for MPEG video with B-frames
\param isom_file the target ISO file
\param trackNumber the target hint track
\param timeOffset time offset in RTP media timescale
\return error if any
*/
GF_Err gf_isom_rtp_packet_set_offset(GF_ISOFile *isom_file, u32 trackNumber, s32 timeOffset);


/*! sets the RTP TimeScale that the server use to send packets
some RTP payloads may need a specific timeScale that is not the timeScale in the file format
the default timeScale choosen by the API is the MediaTimeScale of the hint track
\param isom_file the target ISO file
\param trackNumber the target hint track
\param HintDescriptionIndex the target hint sample description index
\param TimeScale the RTP timescale to use
\return error if any
*/
GF_Err gf_isom_rtp_set_timescale(GF_ISOFile *isom_file, u32 trackNumber, u32 HintDescriptionIndex, u32 TimeScale);

/*! sets the RTP TimeOffset that the server will add to the packets
if not set, the server adds a random offset
\param isom_file the target ISO file
\param trackNumber the target hint track
\param HintDescriptionIndex the target hint sample description index
\param TimeOffset the time offset in RTP timescale
\return error if any
*/
GF_Err gf_isom_rtp_set_time_offset(GF_ISOFile *isom_file, u32 trackNumber, u32 HintDescriptionIndex, u32 TimeOffset);

/*! sets the RTP SequenceNumber Offset that the server will add to the packets
if not set, the server adds a random offset
\param isom_file the target ISO file
\param trackNumber the target hint track
\param HintDescriptionIndex the target hint sample description index
\param SequenceNumberOffset the sequence number offset
\return error if any
*/
GF_Err gf_isom_rtp_set_time_sequence_offset(GF_ISOFile *isom_file, u32 trackNumber, u32 HintDescriptionIndex, u32 SequenceNumberOffset);

/*! adds an SDP line to the SDP container at the track level (media-specific SDP info)
\note the CRLF end of line for SDP is automatically inserted
\param isom_file the target ISO file
\param trackNumber the target hint track
\param text the SDP text to add the target hint track
\return error if any
*/
GF_Err gf_isom_sdp_add_track_line(GF_ISOFile *isom_file, u32 trackNumber, const char *text);
/*! removes all SDP info at the track level
\param isom_file the target ISO file
\param trackNumber the target hint track
\return error if any
*/
GF_Err gf_isom_sdp_clean_track(GF_ISOFile *isom_file, u32 trackNumber);

/*! adds an SDP line to the SDP container at the movie level (presentation SDP info)
\note The CRLF end of line for SDP is automatically inserted
\param isom_file the target ISO file
\param text the SDP text to add the target hint track
\return error if any
*/
GF_Err gf_isom_sdp_add_line(GF_ISOFile *isom_file, const char *text);
/*! removes all SDP info at the movie level
\param isom_file the target ISO file
\return error if any
*/
GF_Err gf_isom_sdp_clean(GF_ISOFile *isom_file);

#endif// !defined(GPAC_DISABLE_ISOM_WRITE) && !defined(GPAC_DISABLE_ISOM_HINTING)


#ifndef GPAC_DISABLE_ISOM_HINTING

#ifndef GPAC_DISABLE_ISOM_DUMP
/*! dumps RTP hint samples structure into XML trace file
\param isom_file the target ISO file
\param trackNumber the target track
\param SampleNum the target sample number
\param trace the file object to dump to
\return error if any
*/
GF_Err gf_isom_dump_hint_sample(GF_ISOFile *isom_file, u32 trackNumber, u32 SampleNum, FILE * trace);
#endif

/*! gets SDP info at the movie level
\param isom_file the target ISO file
\param sdp set to the sdp text, including a null-terminating character - do not modify
\param length set to the sdp length, not including the null-terminating character
\return error if any
*/
GF_Err gf_isom_sdp_get(GF_ISOFile *isom_file, const char **sdp, u32 *length);
/*! gets SDP info at the track level
\param isom_file the target ISO file
\param trackNumber the target track
\param sdp set to the sdp text, including a null-terminating character - do not modify
\param length set to the sdp length, not including the null-terminating character
\return error if any
*/
GF_Err gf_isom_sdp_track_get(GF_ISOFile *isom_file, u32 trackNumber, const char **sdp, u32 *length);
/*! gets number of payload type defines for an RTP hint track
\param isom_file the target ISO file
\param trackNumber the target track
\return the number of payload types defined
*/
u32 gf_isom_get_payt_count(GF_ISOFile *isom_file, u32 trackNumber);
/*! gets payload type information for an RTP hint track
\param isom_file the target ISO file
\param trackNumber the target track
\param index the payload type 1_based index
\param payID set to the ID of the payload type
\return the sdp fmtp attribute describing the payload
*/
const char *gf_isom_get_payt_info(GF_ISOFile *isom_file, u32 trackNumber, u32 index, u32 *payID);


#endif /*GPAC_DISABLE_ISOM_HINTING*/


/*! @} */

/*!
\addtogroup isotxt_grp Subtitles and Timed Text
\ingroup iso_grp

@{
*/


/*! sets streaming text reading mode (MPEG-4 text vs 3GPP)
\param isom_file the target ISO file
\param do_convert is set, all text samples will be retrieved as TTUs and ESD will be emulated for text tracks
\return error if any
*/
GF_Err gf_isom_text_set_streaming_mode(GF_ISOFile *isom_file, Bool do_convert);


#ifndef GPAC_DISABLE_ISOM_DUMP
/*! text track export type*/
typedef enum {
	/*! dump as TTXT XML*/
	GF_TEXTDUMPTYPE_TTXT = 0,
	/*! dump as TTXT XML with box */
	GF_TEXTDUMPTYPE_TTXT_BOXES,
	/*! dump as SRT*/
	GF_TEXTDUMPTYPE_SRT,
	/*! dump as SVG*/
	GF_TEXTDUMPTYPE_SVG,
	/*! dump as TTXT chapters (omits empty text samples)*/
	GF_TEXTDUMPTYPE_TTXT_CHAP,
	/*! dump as OGG chapters*/
	GF_TEXTDUMPTYPE_OGG_CHAP,
	/*! dump as Zoom chapters*/
	GF_TEXTDUMPTYPE_ZOOM_CHAP
} GF_TextDumpType;
/*! dumps a text track to a file
\param isom_file the target ISO file
\param trackNumber the target track
\param dump the file object to write to (binary open mode)
\param dump_type the dump type mode
\return error if any
*/
GF_Err gf_isom_text_dump(GF_ISOFile *isom_file, u32 trackNumber, FILE *dump, GF_TextDumpType dump_type);
#endif

/*! gets encoded TX3G box (text sample description for 3GPP text streams) as needed by RTP or other standards:
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param sidx_offset if 0, the sidx will NOT be written before the encoded TX3G. If not 0, the sidx will be written before the encoded TX3G, with the given offset. Offset sshould be at least 128 for most common usage of TX3G (RTP, MPEG-4 timed text, etc)
\param tx3g set to a newly allocated buffer containing the encoded tx3g - to be freed by caller
\param tx3g_size set to the size of the encoded config
\return error if any
*/
GF_Err gf_isom_text_get_encoded_tx3g(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 sidx_offset, u8 **tx3g, u32 *tx3g_size);

/*! sets TX3G flags for forced samples
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param force_type if 0, no forced subs are present. If 1, some forced subs are present; if 2, all samples are forced subs
\return error if any
*/
GF_Err gf_isom_set_forced_text(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 force_type);

/*! text sample formatting*/
typedef struct _3gpp_text_sample GF_TextSample;
/*! creates text sample handle
\return a newly allocated text sample
*/
GF_TextSample *gf_isom_new_text_sample();
/*! destroys text sample handle
\param tx_samp the target text sample
*/
void gf_isom_delete_text_sample(GF_TextSample *tx_samp);

/*! generic subtitle sample formatting*/
typedef struct _generic_subtitle_sample GF_GenericSubtitleSample;
/*! creates generic subtitle sample handle
\return a newly allocated generic subtitle sample
*/
GF_GenericSubtitleSample *gf_isom_new_generic_subtitle_sample();
/*! destroys generic subtitle sample handle
\param generic_subtitle_samp the target generic subtitle sample
*/
void gf_isom_delete_generic_subtitle_sample(GF_GenericSubtitleSample *generic_subtitle_samp);

#ifndef GPAC_DISABLE_VTT
/*! creates new WebVTT config
\param isom_file the target ISO file
\param trackNumber the target track
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\param config the WebVTT configuration string
\return error if any
*/
GF_Err gf_isom_new_webvtt_description(GF_ISOFile *isom_file, u32 trackNumber, const char *URLname, const char *URNname, u32 *outDescriptionIndex, const char *config);
#endif

/*! gets WebVTT config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return the WebVTT configuration string
*/
const char *gf_isom_get_webvtt_config(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets simple streaming text config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param mime set to the mime type (optional, can be NULL)
\param encoding set to the text encoding type (optional, can be NULL)
\param config set to the WebVTT configuration string (optional, can be NULL)
\return error if any
*/
GF_Err gf_isom_stxt_get_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, const char **mime, const char **encoding, const char **config);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates new simple streaming text config
\param isom_file the target ISO file
\param trackNumber the target track
\param type the four character code of the simple text sample description (sbtt, stxt, mett)
\param mime the mime type
\param encoding the text encoding, if any
\param config the configuration string, if any
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_new_stxt_description(GF_ISOFile *isom_file, u32 trackNumber, u32 type, const char *mime, const char *encoding, const char *config, u32 *outDescriptionIndex);

#endif // GPAC_DISABLE_ISOM_WRITE

/*! gets XML streaming text config for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param xmlnamespace set to the XML namespace (optional, can be NULL)
\param xml_schema_loc set to the XML schema location (optional, can be NULL)
\param mimes set to the associated mime(s) types (optional, can be NULL)
\return error if any
*/
GF_Err gf_isom_xml_subtitle_get_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex,
        const char **xmlnamespace, const char **xml_schema_loc, const char **mimes);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates a new XML streaming text config
\param isom_file the target ISO file
\param trackNumber the target track
\param xmlnamespace the XML namespace
\param xml_schema_loc the XML schema location (optional, can be NULL)
\param auxiliary_mimes the associated mime(s) types (optional, can be NULL)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_new_xml_subtitle_description(GF_ISOFile *isom_file, u32 trackNumber,
        const char *xmlnamespace, const char *xml_schema_loc, const char *auxiliary_mimes,
        u32 *outDescriptionIndex);
#endif // GPAC_DISABLE_ISOM_WRITE


/*! gets MIME parameters  (type/subtype + codecs and profiles) associated with a sample descritpion
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\return MIME param if present, NULL otherwise
*/
const char *gf_isom_subtitle_get_mime(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! gets MIME parameters associated with a sample descritpion
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param full_mime MIME param (type/subtype + codecs and profiles) to set, if NULL removes MIME parameter info
\return error if any
*/
GF_Err gf_isom_subtitle_set_mime(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, const char *full_mime);
#endif


/*! gets XML metadata for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param xmlnamespace set to the XML namespace (optional, can be NULL)
\param schema_loc set to the XML schema location (optional, can be NULL)
\param content_encoding set to the content encoding string (optional, can be NULL)
\return error if any
*/
GF_Err gf_isom_get_xml_metadata_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, const char **xmlnamespace, const char **schema_loc, const char **content_encoding);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates a new timed metadata sample description for this track
\param isom_file the target ISO file
\param trackNumber the target track
\param xmlnamespace the XML namespace
\param schema_loc the XML schema location (optional, can be NULL)
\param content_encoding the content encoding string (optional, can be NULL)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_new_xml_metadata_description(GF_ISOFile *isom_file, u32 trackNumber, const char *xmlnamespace, const char *schema_loc, const char *content_encoding, u32 *outDescriptionIndex);
#endif /*GPAC_DISABLE_ISOM_WRITE*/




/*! text flags operation type*/
typedef enum
{
	GF_ISOM_TEXT_FLAGS_OVERWRITE = 0,
	GF_ISOM_TEXT_FLAGS_TOGGLE,
	GF_ISOM_TEXT_FLAGS_UNTOGGLE,
} GF_TextFlagsMode;
/*! sets text display flags according to given mode.
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index. If 0, sets the flags for all text descriptions
\param flags the flag to set
\param op_type the flag toggle mode
\return error if any
*/
GF_Err gf_isom_text_set_display_flags(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 flags, GF_TextFlagsMode op_type);

/*! gets text description of a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the target sample description index
\param out_desc set to a newly allocated text sample descriptor - shall be freeed by user
\return error if any
*/
GF_Err gf_isom_get_text_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, GF_TextSampleDescriptor **out_desc);

#ifndef GPAC_DISABLE_ISOM_WRITE

/*! creates a new TextSampleDescription in the file.
\param isom_file the target ISO file
\param trackNumber the target track
\param desc the text sample description
\param URLname URL value of the data reference, NULL if no data reference (media in the file)
\param URNname URN value of the data reference, NULL if no data reference (media in the file)
\param outDescriptionIndex set to the index of the created sample description
\return error if any
*/
GF_Err gf_isom_new_text_description(GF_ISOFile *isom_file, u32 trackNumber, GF_TextSampleDescriptor *desc, const char *URLname, const char *URNname, u32 *outDescriptionIndex);

/*! resets text sample content
\param tx_samp the target text sample
\return error if any
*/
GF_Err gf_isom_text_reset(GF_TextSample * tx_samp);
/*! resets text sample styles but keep text
\param tx_samp the target text sample
\return error if any
*/
GF_Err gf_isom_text_reset_styles(GF_TextSample *tx_samp);

/*! appends text to sample - text_len is the number of bytes to be written from text_data. This allows
handling UTF8 and UTF16 strings in a transparent manner
\param tx_samp the target text sample
\param text_data the text data to add
\param text_len the size of the data to add
\return error if any
*/
GF_Err gf_isom_text_add_text(GF_TextSample *tx_samp, char *text_data, u32 text_len);
/*! appends style modifyer to sample
\param tx_samp the target text sample
\param rec the style record to add
\return error if any
*/
GF_Err gf_isom_text_add_style(GF_TextSample *tx_samp, GF_StyleRecord *rec);
/*! appends highlight modifier for the sample
\param tx_samp the target text sample
\param start_char first char highlighted,
\param end_char first char not highlighted
\return error if any
*/
GF_Err gf_isom_text_add_highlight(GF_TextSample *tx_samp, u16 start_char, u16 end_char);

/*! sets highlight color for the whole sample
\param tx_samp the target text sample
\param argb color value
\return error if any
*/
GF_Err gf_isom_text_set_highlight_color(GF_TextSample *tx_samp, u32 argb);
/*! appends a new karaoke sequence in the sample
\param tx_samp the target text sample
\param start_time karaoke start time expressed in text stream timescale, but relative to the sample media time
\return error if any
*/
GF_Err gf_isom_text_add_karaoke(GF_TextSample *tx_samp, u32 start_time);
/*! appends a new segment in the current karaoke sequence - you must build sequences in order to be compliant
\param tx_samp the target text sample
\param end_time segment end time expressed in text stream timescale, but relative to the sample media time
\param start_char first char highlighted,
\param end_char first char not highlighted
\return error if any
*/
GF_Err gf_isom_text_set_karaoke_segment(GF_TextSample *tx_samp, u32 end_time, u16 start_char, u16 end_char);
/*! sets scroll delay for the whole sample (scrolling is enabled through GF_TextSampleDescriptor.DisplayFlags)
\param tx_samp the target text sample
\param scroll_delay delay for scrolling expressed in text stream timescale
\return error if any
*/
GF_Err gf_isom_text_set_scroll_delay(GF_TextSample *tx_samp, u32 scroll_delay);
/*! appends hyperlinking for the sample
\param tx_samp the target text sample
\param URL UTF-8 url
\param altString UTF-8 hint (tooltip, ...) for end user
\param start_char first char hyperlinked,
\param end_char first char not hyperlinked
\return error if any
*/
GF_Err gf_isom_text_add_hyperlink(GF_TextSample *tx_samp, char *URL, char *altString, u16 start_char, u16 end_char);
/*! sets current text box (display pos&size within the text track window) for the sample
\param tx_samp the target text sample
\param top top coordinate of box
\param left left coordinate of box
\param bottom bottom coordinate of box
\param right right coordinate of box
\return error if any
*/
GF_Err gf_isom_text_set_box(GF_TextSample *tx_samp, s16 top, s16 left, s16 bottom, s16 right);
/*! appends blinking for the sample
\param tx_samp the target text sample
\param start_char first char blinking,
\param end_char first char not blinking
\return error if any
*/
GF_Err gf_isom_text_add_blink(GF_TextSample *tx_samp, u16 start_char, u16 end_char);
/*! sets wrap flag for the sample
\param tx_samp the target text sample
\param wrap_flags text wrap flags - currently only 0 (no wrap) and 1 ("soft wrap") are allowed in 3GP
\return error if any
*/
GF_Err gf_isom_text_set_wrap(GF_TextSample *tx_samp, u8 wrap_flags);

/*! sets force for the sample
\param tx_samp the target text sample
\param is_forced for ce sample if TRUE
\return error if any
*/
GF_Err gf_isom_text_set_forced(GF_TextSample *tx_samp, Bool is_forced);

/*! formats sample as a regular GF_ISOSample payload in a bitstream object.
\param tx_samp the target text sample
\param bs thetarget bitstream
\return error if any
*/
GF_Err gf_isom_text_sample_write_bs(const GF_TextSample *tx_samp, GF_BitStream *bs);


/*! formats sample as a regular GF_ISOSample.
The resulting sample will always be marked as random access
\param tx_samp the target text sample
\return the corresponding serialized ISO sample
*/
GF_ISOSample *gf_isom_text_to_sample(const GF_TextSample *tx_samp);

/*! gets the serialized size of the text sample
\param tx_samp the target text sample
\return the serialized size
*/
u32 gf_isom_text_sample_size(GF_TextSample *tx_samp);

/*! creates a new XML subtitle sample
\return a new XML subtitle sample
*/
GF_GenericSubtitleSample *gf_isom_new_xml_subtitle_sample();
/*! deletes an XML subtitle sample
\param subt_samp the target XML subtitle sample
*/
void gf_isom_delete_xml_subtitle_sample(GF_GenericSubtitleSample *subt_samp);
/*! resets content of an XML subtitle sample
\param subt_samp the target XML subtitle sample
\return error if any
*/
GF_Err gf_isom_xml_subtitle_reset(GF_GenericSubtitleSample *subt_samp);
/*! the corresponding serialized ISO sample
\param subt_samp the target XML subtitle sample
\return the corresponding serialized ISO sample
*/
GF_ISOSample *gf_isom_xml_subtitle_to_sample(GF_GenericSubtitleSample *subt_samp);
/*! appends text to an XML subtitle sample
\param subt_samp the target XML subtitle sample
\param text_data the UTF-8 or UTF-16 data to add
\param text_len the size of the text to add in bytes
\return error if any
*/
GF_Err gf_isom_xml_subtitle_sample_add_text(GF_GenericSubtitleSample *subt_samp, char *text_data, u32 text_len);


#endif	/*GPAC_DISABLE_ISOM_WRITE*/

/*! @} */


/*!
\addtogroup isocrypt_grp Content Protection
\ingroup iso_grp

@{
*/

#endif // GPAC_DISABLE_ISOM

/*! DRM related code points*/
enum
{
	/*! Storage location of CENC sample auxiliary in PSEC UUID box*/
	GF_ISOM_BOX_UUID_PSEC = GF_4CC( 'P', 'S', 'E', 'C' ),
	/*! Storage location of CENC sample auxiliary in senc box*/
	GF_ISOM_BOX_TYPE_SENC = GF_4CC( 's', 'e', 'n', 'c'),
	/*! PSSH box type */
	GF_ISOM_BOX_TYPE_PSSH = GF_4CC( 'p', 's', 's', 'h'),
	/*! ISMA Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_ISMACRYP_SCHEME	= GF_4CC( 'i', 'A', 'E', 'C' ),
	/*! OMA DRM Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_OMADRM_SCHEME = GF_4CC('o','d','k','m'),
	/*! CENC AES-CTR Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_CENC_SCHEME	= GF_4CC('c','e','n','c'),
	/*! CENC AES-CBC Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_CBC_SCHEME = GF_4CC('c','b','c','1'),
	/*! Adobe Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_ADOBE_SCHEME = GF_4CC('a','d','k','m'),
	/*! CENC AES-CTR Pattern Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_CENS_SCHEME	= GF_4CC('c','e','n','s'),
	/*! CENC AES-CBC Pattern Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_CBCS_SCHEME	= GF_4CC('c','b','c','s'),
	/*! PIFF Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_PIFF_SCHEME	= GF_4CC('p','i','f','f'),
	/*! CENC sensitive encryption */
	GF_ISOM_SVE1_SCHEME	= GF_4CC('s','v','e','1'),
};


/*! flags for GF_ISMASample*/
typedef enum
{
	/*! signals the stream the sample belongs to uses selective encryption*/
	GF_ISOM_ISMA_USE_SEL_ENC = 1,
	/*! signals the sample is encrypted*/
	GF_ISOM_ISMA_IS_ENCRYPTED = 2,
} GF_ISOISMACrypFlags;


#ifndef GPAC_DISABLE_ISOM

/*! checks if a track is encrypted or protected
\param isom_file the target ISO file
\param trackNumber the target track
\return GF_TRUE if track is protected, GF_FALSE otherwise*/
Bool gf_isom_is_track_encrypted(GF_ISOFile *isom_file, u32 trackNumber);



/*! ISMA sample*/
typedef struct
{
	/*! IV in ISMACryp is Byte Stream Offset*/
	u64 IV;
	/*! IV size in bytes, repeated from sampleDesc for convenience*/
	u8 IV_length;
	/*! key indicator*/
	u8 *key_indicator;
	/*! key indicator size, repeated from sampleDesc for convenience*/
	u8 KI_length;
	/*! payload size*/
	u32 dataLength;
	/*! payload*/
	u8 *data;
	/*! flags*/
	u32 flags;
} GF_ISMASample;
/*! creates a new empty ISMA sample
\return a new empty ISMA sample
*/
GF_ISMASample *gf_isom_ismacryp_new_sample();

/*! delete an ISMA sample.
\note the buffer content will be destroyed by default. If you wish to keep the buffer, set dataLength to 0 in the sample before deleting it
\param samp the target ISMA sample
*/
void gf_isom_ismacryp_delete_sample(GF_ISMASample *samp);

/*! decodes ISMACryp sample based on all info in ISMACryp sample description
\param data sample data
\param dataLength sample data size in bytes
\param use_selective_encryption set to GF_TRUE if sample uses selective encryption
\param KI_length set to the size in bytes of the key indicator - 0 means no key roll
\param IV_length set to the size in bytes of the initialization vector
\return a newly allocated ISMA sample with the parsed data
*/
GF_ISMASample *gf_isom_ismacryp_sample_from_data(u8 *data, u32 dataLength, Bool use_selective_encryption, u8 KI_length, u8 IV_length);

/*! decodes ISMACryp sample based on sample and its descrition index
\param isom_file the target ISO file
\param trackNumber the target track
\param samp the sample to decode
\param sampleDescriptionIndex the sample description index of the sample to decode
\return the ISMA sample or NULL if not an ISMA sample or error
*/
GF_ISMASample *gf_isom_get_ismacryp_sample(GF_ISOFile *isom_file, u32 trackNumber, const GF_ISOSample *samp, u32 sampleDescriptionIndex);

/*! checks if sample description is protected or not
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index. If 0, checks all sample descriptions for protected ones
\return scheme protection 4CC or 0 if not protected*/
u32 gf_isom_is_media_encrypted(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! checks if sample description is protected with ISMACryp
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\return GF_TRUE if ISMA protection is used*/
Bool gf_isom_is_ismacryp_media(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! checks if sample description is protected with OMA DRM
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\return GF_TRUE if OMA DRM protection is used*/
Bool gf_isom_is_omadrm_media(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets OMA DRM configuration - all output parameters are optional and may be NULL
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param outOriginalFormat four character code of the unprotected sample description
\param outSchemeType set to four character code of the protection scheme type
\param outSchemeVersion set to scheme protection version
\param outContentID set to associated ID of content
\param outRightsIssuerURL set to the rights issuer (license server) URL
\param outTextualHeaders set to OMA textual headers
\param outTextualHeadersLen set to the size in bytes of OMA textual headers
\param outPlaintextLength set to the size in bytes of clear data in file
\param outEncryptionType set to the OMA encryption type used
\param outSelectiveEncryption set to GF_TRUE if sample description uses selective encryption
\param outIVLength set to the size of the initialization vector
\param outKeyIndicationLength set to the size of the key indicator
\return error if any
*/
GF_Err gf_isom_get_omadrm_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat,
                               u32 *outSchemeType, u32 *outSchemeVersion,
                               const char **outContentID, const char **outRightsIssuerURL, const char **outTextualHeaders, u32 *outTextualHeadersLen, u64 *outPlaintextLength, u32 *outEncryptionType, Bool *outSelectiveEncryption, u32 *outIVLength, u32 *outKeyIndicationLength);

/*! retrieves ISMACryp info for the given track & SDI - all output parameters are optional - URIs SHALL NOT BE MODIFIED BY USER

\note outSelectiveEncryption, outIVLength and outKeyIndicationLength are usually not needed to decode an ISMA sample when using \ref gf_isom_get_ismacryp_sample

\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param outOriginalFormat set to orginal unprotected media format
\param outSchemeType set to 4CC of protection scheme (GF_ISOM_ISMACRYP_SCHEME = iAEC in ISMACryp 1.0)
\param outSchemeVersion set to version of protection scheme (1 in ISMACryp 1.0)
\param outSchemeURI set to URI location of scheme
\param outKMS_URI set to URI location of key management system - only valid with ISMACryp 1.0
\param outSelectiveEncryption set to whether sample-based encryption is used in media - only valid with ISMACryp 1.0
\param outIVLength set to length of Initial Vector - only valid with ISMACryp 1.0
\param outKeyIndicationLength set to length of key indicator - only valid with ISMACryp 1.0
\return error if any
*/
GF_Err gf_isom_get_ismacryp_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, const char **outSchemeURI, const char **outKMS_URI, Bool *outSelectiveEncryption, u32 *outIVLength, u32 *outKeyIndicationLength);

/*! gets original format four character code type of a protected media sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index. If 0, checks all sample descriptions for a protected one
\param outOriginalFormat set to orginal unprotected media format
\return error if any
*/
GF_Err gf_isom_get_original_format_type(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat);

#ifndef GPAC_DISABLE_ISOM_WRITE

/*! creates ISMACryp protection info for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param scheme_type 4CC of protection scheme (GF_ISOM_ISMACRYP_SCHEME = iAEC in ISMACryp 1.0)
\param scheme_version version of protection scheme (1 in ISMACryp 1.0)
\param scheme_uri URI location of scheme
\param kms_URI URI location of key management system - only valid with ISMACryp 1.0
\param selective_encryption whether sample-based encryption is used in media - only valid with ISMACryp 1.0
\param KI_length length of key indicator - only valid with ISMACryp 1.0
\param IV_length length of Initial Vector - only valid with ISMACryp 1.0
\return error if any
*/
GF_Err gf_isom_set_ismacryp_protection(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 scheme_type,
                                       u32 scheme_version, char *scheme_uri, char *kms_URI,
                                       Bool selective_encryption, u32 KI_length, u32 IV_length);

/*! changes scheme URI and/or KMS URI for crypted files. Other params cannot be changed once the media is crypted
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param scheme_uri new scheme URI, or NULL to keep previous
\param kms_uri new KMS URI, or NULL to keep previous
\return error if any
*/
GF_Err gf_isom_change_ismacryp_protection(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, char *scheme_uri, char *kms_uri);


/*! creates OMA DRM protection for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param contentID associated ID of content
\param kms_URI the rights issuer (license server) URL
\param encryption_type the OMA encryption type used
\param plainTextLength the size in bytes of clear data in file
\param textual_headers OMA textual headers
\param textual_headers_len the size in bytes of OMA textual headers
\param selective_encryption GF_TRUE if sample description uses selective encryption
\param KI_length the size of the key indicator
\param IV_length the size of the initialization vector
\return error if any
*/
GF_Err gf_isom_set_oma_protection(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex,
                                  char *contentID, char *kms_URI, u32 encryption_type, u64 plainTextLength, char *textual_headers, u32 textual_headers_len,
                                  Bool selective_encryption, u32 KI_length, u32 IV_length);

/*! creates a generic protection for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param scheme_type 4CC of protection scheme (GF_ISOM_ISMACRYP_SCHEME = iAEC in ISMACryp 1.0)
\param scheme_version version of protection scheme (1 in ISMACryp 1.0)
\param scheme_uri URI location of scheme
\param kms_URI the rights issuer (license server) URL
\return error if any
*/
GF_Err gf_isom_set_generic_protection(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 scheme_type, u32 scheme_version, char *scheme_uri, char *kms_URI);

/*! allocates storage for CENC side data in a senc box
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_cenc_allocate_storage(GF_ISOFile *isom_file, u32 trackNumber);

/*! allocates storage for CENC side data in a PIFF senc UUID box
\param isom_file the target ISO file
\param trackNumber the target track
\param AlgorithmID algorith ID, usually 0
\param IV_size the size of the init vector
\param KID the default Key ID
\return error if any
*/
GF_Err gf_isom_piff_allocate_storage(GF_ISOFile *isom_file, u32 trackNumber, u32 AlgorithmID, u8 IV_size, bin128 KID);


/*! adds cenc SAI for the last sample added to a track
\param isom_file the target ISO file
\param trackNumber the target track
\param container_type the code of the container (currently 'senc' for CENC or 'PSEC' for smooth)
\param buf the SAI buffer
\param len the size of the SAI buffer. If buf is NULL or len is 0, this adds an unencrypted entry (not written to file)
\param use_subsamples if GF_TRUE, the media format uses CENC subsamples
\param use_saio_32bit forces usage of 32-bit saio boxes
\param is_multi_key indicates if multi key is in use (required to tag saio and saiz boxes)
\return error if any
*/
GF_Err gf_isom_track_cenc_add_sample_info(GF_ISOFile *isom_file, u32 trackNumber, u32 container_type, u8 *buf, u32 len, Bool use_subsamples, Bool use_saio_32bit, Bool is_multi_key);



/*! creates CENC protection for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param scheme_type 4CC of protection scheme (GF_ISOM_ISMACRYP_SCHEME = iAEC in ISMACryp 1.0)
\param scheme_version version of protection scheme (1 in ISMACryp 1.0)
\param default_IsEncrypted default isEncrypted flag
\param default_crypt_byte_block default crypt block size for pattern encryption
\param default_skip_byte_block default skip block size for pattern encryption
\param key_info key descriptor formatted as a multi-key info (cf GF_PROP_PID_CENC_KEY)
\param key_info_size key descriptor size
\return error if any
*/
GF_Err gf_isom_set_cenc_protection(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 scheme_type,
                                   u32 scheme_version, u32 default_IsEncrypted, u32 default_crypt_byte_block, u32 default_skip_byte_block,
								    u8 *key_info, u32 key_info_size);


/*! creates CENC protection for a multi-key sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param scheme_type 4CC of protection scheme (GF_ISOM_ISMACRYP_SCHEME = iAEC in ISMACryp 1.0)
\param scheme_version version of protection scheme (1 in ISMACryp 1.0)
\param default_IsEncrypted default isEncrypted flag
\param default_crypt_byte_block default crypt block size for pattern encryption
\param default_skip_byte_block default skip block size for pattern encryption
\param key_info key  info (cf CENC and GF_PROP_PID_CENC_KEY)
\param key_info_size key info size
\return error if any
*/
GF_Err gf_isom_set_cenc_protection_mkey(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 scheme_type,
                                   u32 scheme_version, u32 default_IsEncrypted, u32 default_crypt_byte_block, u32 default_skip_byte_block,
								    u8 *key_info, u32 key_info_size);


/*! adds PSSH info for a file, can be called several time per system ID
\param isom_file the target ISO file
\param systemID the ID of the protection system
\param version the version of the protection system
\param KID_count the number of key IDs
\param KID the list of key IDs
\param data opaque data for the protection system
\param len size of the opaque data
\param pssh_mode 0: regular PSSH in moov, 1: PIFF PSSH in moov, 2: regular PSSH in meta
\return error if any
*/
GF_Err gf_cenc_set_pssh(GF_ISOFile *isom_file, bin128 systemID, u32 version, u32 KID_count, bin128 *KID, u8 *data, u32 len, u32 pssh_mode);

/*! removes CENC senc box info
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_remove_samp_enc_box(GF_ISOFile *isom_file, u32 trackNumber);
/*! removes all CENC sample groups
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_remove_samp_group_box(GF_ISOFile *isom_file, u32 trackNumber);

#endif //GPAC_DISABLE_ISOM_WRITE

/*! checks if sample description is protected with Adobe systems
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\return GF_TRUE if ADOBE protection is used
*/
Bool gf_isom_is_adobe_protection_media(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*! gets adobe protection information for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param outOriginalFormat set to orginal unprotected media format
\param outSchemeType set to 4CC of protection scheme (GF_ISOM_ISMACRYP_SCHEME = iAEC in ISMACryp 1.0)
\param outSchemeVersion set to version of protection scheme (1 in ISMACryp 1.0)
\param outMetadata set to adobe metadata string
\return GF_TRUE if ADOBE protection is used
*/
GF_Err gf_isom_get_adobe_protection_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, const char **outMetadata);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! creates an adobe protection for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param scheme_type 4CC of protection scheme (GF_ISOM_ISMACRYP_SCHEME = iAEC in ISMACryp 1.0)
\param scheme_version version of protection scheme (1 in ISMACryp 1.0)
\param is_selective_enc indicates if selective encryption is used
\param metadata metadata information
\param len size of metadata information in bytes
\return error if any
*/
GF_Err gf_isom_set_adobe_protection(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 scheme_type, u32 scheme_version, Bool is_selective_enc, char *metadata, u32 len);

#endif /*GPAC_DISABLE_ISOM_WRITE*/

/*! checks of sample description is protected with CENC
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index. If 0, checks all sample descriptions for protected ones
\return GF_TRUE if sample protection is CENC
*/
Bool gf_isom_is_cenc_media(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex);
/*! gets CENC information of a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param outOriginalFormat set to orginal unprotected media format
\param outSchemeType set to 4CC of protection scheme (GF_ISOM_ISMACRYP_SCHEME = iAEC in ISMACryp 1.0)
\param outSchemeVersion set to version of protection scheme (1 in ISMACryp 1.0)
\return error if any
*/
GF_Err gf_isom_get_cenc_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion);


/*! gets CENC auxiliary info of a sample as a buffer
\note the serialized buffer format is exactly a CencSampleAuxiliaryDataFormat

\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample
\param sampleDescIndex the sample description index
\param container_type is type of box which contains the sample auxiliary information. Now we have two type: GF_ISOM_BOX_UUID_PSEC and GF_ISOM_BOX_TYPE_SENC
\param out_buffer set to a newly allocated buffer, or reallocated buffer if not NULL
\param outSize set to the size of the serialized buffer. If an existing buffer was passed, the passed value shall be the allocated buffer size (the returned value is still the buffer size)
\return error if any
*/
GF_Err gf_isom_cenc_get_sample_aux_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 sampleDescIndex, u32 *container_type, u8 **out_buffer, u32 *outSize);

/*! gets CENC default info for a sample description
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleDescriptionIndex the sample description index
\param container_type set to the container type of SAI data
\param default_IsEncrypted set to default isEncrypted flag
\param crypt_byte_block set to default crypt block size for pattern encryption
\param skip_byte_block set to default skip block size for pattern encryption
\param key_info set to multikey descriptor (cf CENC and GF_PROP_PID_CENC_KEY)
\param key_info_size set to multikey descriptor size
\return error if any
*/
GF_Err gf_isom_cenc_get_default_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *container_type, Bool *default_IsEncrypted, u32 *crypt_byte_block, u32 *skip_byte_block, const u8 **key_info, u32 *key_info_size);

/*! gets the number of PSSH defined
\param isom_file the target ISO file
\return number of PSSH defined
*/
u32 gf_isom_get_pssh_count(GF_ISOFile *isom_file);

/*! gets PSS info
\param isom_file the target ISO file
\param pssh_index 1-based index of PSSH to query, see \ref gf_isom_get_pssh_count
\param SystemID set to the protection system ID
\param version set to the protection system version
\param KID_count set to the number of key IDs defined
\param KIDs array of defined key IDs
\param private_data set to a buffer containing system ID private data
\param private_data_size set to the size of the system ID private data
\return error if any
*/
GF_Err gf_isom_get_pssh_info(GF_ISOFile *isom_file, u32 pssh_index, bin128 SystemID, u32 *version, u32 *KID_count, const bin128 **KIDs, const u8 **private_data, u32 *private_data_size);

#ifndef GPAC_DISABLE_ISOM_DUMP
/*! dumps ismacrypt protection of sample descriptions to xml trace
\param isom_file the target ISO file
\param trackNumber the target track
\param trace the file object to dump to
\return error if any
*/
GF_Err gf_isom_dump_ismacryp_protection(GF_ISOFile *isom_file, u32 trackNumber, FILE * trace);
/*! dumps ismacrypt sample to xml trace
\param isom_file the target ISO file
\param trackNumber the target track
\param SampleNum the target sample number
\param trace the file object to dump to
\return error if any
*/
GF_Err gf_isom_dump_ismacryp_sample(GF_ISOFile *isom_file, u32 trackNumber, u32 SampleNum, FILE *trace);
#endif

/*! gets CENC configuration for a given sample
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number
\param IsEncrypted set to GF_TRUE if the sample is encrypted, GF_FALSE otherwise (optional can be NULL)
\param crypt_byte_block set to crypt block count for pattern encryption (optional can be NULL)
\param skip_byte_block set to skip block count for pattern encryption (optional can be NULL)
\param key_info set to key descriptor (cf GF_PROP_PID_CENC_KEY)
\param key_info_size set to key descriptor size
\return error if any
*/
GF_Err gf_isom_get_sample_cenc_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, Bool *IsEncrypted, u32 *crypt_byte_block, u32 *skip_byte_block, const u8 **key_info, u32 *key_info_size);

/*! @} */

/*!
\addtogroup isometa_grp Meta and Image File Format
\ingroup iso_grp

@{
*/


/*! gets meta type
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\return 0 if no meta found, or four char code of meta (eg, "mp21", "smil", ...)*/
u32 gf_isom_get_meta_type(GF_ISOFile *isom_file, Bool root_meta, u32 track_num);

/*! checks if the meta has an XML container (note that XML data can also be included as items).
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\return 0 (no XML or error), 1 (XML text), 2 (BinaryXML, eg BiM) */
u32 gf_isom_has_meta_xml(GF_ISOFile *isom_file, Bool root_meta, u32 track_num);

/*! extracts XML (if any) from given meta
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param outName output file path and location for writing
\param is_binary indicates if XML is Bim or regular XML
\return error if any
*/
GF_Err gf_isom_extract_meta_xml(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, char *outName, Bool *is_binary);

/*! checks the number of items in a meta
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\return number of items*/
u32 gf_isom_get_meta_item_count(GF_ISOFile *isom_file, Bool root_meta, u32 track_num);

/*! gets item info for the given item
\note When an item is fully contained in file, both item_url and item_urn are set to NULL

\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_num 1-based index of item to query
\param itemID set to item ID in file (optional, can be NULL)
\param type set to item 4CC type
\param protection_scheme set to 0 if not protected, or scheme type used if item is protected. If protected but scheme type not present, set to 'unkw'
\param protection_scheme_version set to 0 if not protected, or scheme version used if item is protected
\param is_self_reference set to item is the file itself
\param item_name set to the item name (optional, can be NULL)
\param item_mime_type set to the item mime type (optional, can be NULL)
\param item_encoding set to the item content encoding type (optional, can be NULL)
\param item_url set to the URL of external resource containing this item data if any.
\param item_urn set to the URN of external resource containing this item data if any.
\return error if any
*/
GF_Err gf_isom_get_meta_item_info(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_num,
                                  u32 *itemID, u32 *type, u32 *protection_scheme, u32 *protection_scheme_version, Bool *is_self_reference,
                                  const char **item_name, const char **item_mime_type, const char **item_encoding,
                                  const char **item_url, const char **item_urn);

/*! gets item flags for the given item

\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_num 1-based index of item to query
\return item flags
*/
u32 gf_isom_get_meta_item_flags(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_num);

/*! gets item index from item ID
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_ID ID of the item to search
\return item index if found, 0 otherwise
*/
u32 gf_isom_get_meta_item_by_id(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_ID);

/*! extracts an item from given meta
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_num 1-based index of item to query
\param dump_file_name if NULL, uses item name for dumping, otherwise dumps in given file object (binary write mode)
\return error if any
*/
GF_Err gf_isom_extract_meta_item(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_num, const char *dump_file_name);

/*! extracts item from given meta in memory
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_id the ID of the item to dump
\param out_data set to allocated buffer containing the item, shall be freeed by user
\param out_size set to the size of the allocated buffer
\param out_alloc_size set to the allocated size of the buffer (this allows passing an existing buffer without always reallocating it)
\param mime_type set to the mime type of the item
\param use_annex_b for image items based on NALU formats (AVC, HEVC) indicates to extract the data as Annex B format (with start codes)
\return error if any
*/
GF_Err gf_isom_extract_meta_item_mem(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_id, u8 **out_data, u32 *out_size, u32 *out_alloc_size, const char **mime_type, Bool use_annex_b);


/*! fetch CENC info for an item
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_id the ID of the item to dump
\param is_protected set to GF_TRUE if item is protected
\param skip_byte_block set to skip_byte_block or 0 if no pattern
\param crypt_byte_block set to crypt_byte_block or 0 if no pattern
\param key_info set to key info
\param key_info_size set to key info size
\param aux_info_type_parameter set to the CENC auxiliary type param of SAI data
\param sai_out_data set to allocated buffer containing the item, shall be freeed by user - may be NULL to only retrieve the info
\param sai_out_size set to the size of the allocated buffer - may be NULL if  sai_out_data is NULL
\param sai_out_alloc_size set to the allocated size of the buffer (this allows passing an existing buffer without always reallocating it) - may be NULL if  sai_out_data is NULL
\return error if any
*/
GF_Err gf_isom_extract_meta_item_get_cenc_info(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_id, Bool *is_protected,
	u32 *skip_byte_block, u32 *crypt_byte_block, const u8 **key_info, u32 *key_info_size, u32 *aux_info_type_parameter,
	u8 **sai_out_data, u32 *sai_out_size, u32 *sai_out_alloc_size);

/*! gets primary item ID
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\return primary item ID, 0 if none found (primary can also be stored through meta XML)*/
u32 gf_isom_get_meta_primary_item_id(GF_ISOFile *isom_file, Bool root_meta, u32 track_num);

/*! gets number of references of a given type from a given item ID
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param from_id item ID to check
\param type reference type to check
\return number of referenced items*/
u32 gf_isom_meta_get_item_ref_count(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 from_id, u32 type);

/*! gets ID  of reference of a given type and index from a given item ID
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param from_id item ID to check
\param type reference type to check
\param ref_idx 1-based index of reference to check
\return ID if the referred item*/
u32 gf_isom_meta_get_item_ref_id(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 from_id, u32 type, u32 ref_idx);

/*! gets number of references of a given type to a given item ID
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param to_id item ID to check
\param type reference type to check
\return number of referenced items*/
u32 gf_isom_meta_item_has_ref(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 to_id, u32 type);


/*! item tile mode*/
typedef enum {
	/*! not a tile item*/
	TILE_ITEM_NONE = 0,
	/*! a tile item without base*/
	TILE_ITEM_ALL_NO_BASE,
	/*! a tile item with base*/
	TILE_ITEM_ALL_BASE,
	/*! a tile item grid*/
	TILE_ITEM_ALL_GRID,
	/*! a tile item single*/
	TILE_ITEM_SINGLE
} GF_TileItemMode;

/*! Image overlay offset properties*/
typedef struct {
	u32 horizontal;
	u32 vertical;
} GF_ImageItemOverlayOffset;

/*! Image protection item properties*/
typedef struct
{
	u32 scheme_type;
	u32 scheme_version;
	u32 crypt_byte_block;
	u32 skip_byte_block;
	const u8 *key_info;
	u32 key_info_size;
	const u8 *sai_data;
	u32 sai_data_size;
} GF_ImageItemProtection;

/*! Image item properties*/
typedef struct
{
	/*! width in pixels*/
	u32 width;
	/*! height in pixless*/
	u32 height;
	/*! pixel aspect ratio numerator*/
	u32 hSpacing;
	/*! pixel aspect ratio denominator*/
	u32 vSpacing;
	/*! horizontal offset in pixels*/
	u32 hOffset;
	/*! vertical offset in pixels*/
	u32 vOffset;
	/*! angle in radians*/
	u32 angle;
	/*! mirroring axis: 0 = not set, 1 = vertical, 2 = horizontal*/
	u32 mirror;
	/*! hidden flag*/
	Bool hidden;
	/*! clean aperture */
	u32 clap_wnum, clap_wden, clap_hnum, clap_hden, clap_hoden, clap_voden;
	s32 clap_honum, clap_vonum;
	/*! pointer to configuration box*/
	void *config;
	/*! tile item mode*/
	GF_TileItemMode tile_mode;
	/*! tile number */
	u32 single_tile_number;
	/*! time for importing*/
	Double time;
	/*! end time for importing*/
	Double end_time;
	/*! step time between imports*/
	Double step_time;
	/*! sample num for importing*/
	u32 sample_num;
	/*! file containg iCC data for importing*/
	char iccPath[GF_MAX_PATH];
	/*! is alpha*/
	Bool alpha;
	/*! is depth*/
	Bool depth;
	/*! number of channels*/
	u8 num_channels;
	/*! bits per channels in bits*/
	u32 bits_per_channel[3];
	/*! number of columns in grid*/
	u32 num_grid_columns;
	/*! number of rows in grid*/
	u32 num_grid_rows;
	/*! number of overlayed images*/
	u32 overlay_count;
	/*! overlay offsets*/
	GF_ImageItemOverlayOffset *overlay_offsets;
	/*! canvas overlay color*/
	u32 overlay_canvas_fill_value_r;
	u32 overlay_canvas_fill_value_g;
	u32 overlay_canvas_fill_value_b;
	u32 overlay_canvas_fill_value_a;
	/*! protection info, NULL if item is not protected*/
	GF_ImageItemProtection *cenc_info;
	/*! If set, reference image from sample sample_num (same file data used for sample and item)*/
	Bool use_reference;
	/*ID of item to use as source*/
	u32 item_ref_id;
	/*if set, copy all properties of source item*/
	Bool copy_props;
	/*only set when importing non-ref from ISOBMF*/
	GF_ISOFile *src_file;
	Bool auto_grid;
	Double auto_grid_ratio;
	/*AV1 layer sizes except last layer - set during import*/
	u32 av1_layer_size[3];
	/*AV1 operation point index*/
	u8 av1_op_index;

	/*interlace type - uncv*/
	u8 interlace_type;

	const char *aux_urn;
	const u8 *aux_data;
	u32 aux_data_len;

	//serialized box array config, only used for creating item
	u8 *config_ba;
	u32 config_ba_size;

} GF_ImageItemProperties;


#ifndef GPAC_DISABLE_ISOM_WRITE

/*! sets meta type (four char int, eg "mp21", ...), creating a meta box if not found
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param metaType the type of meta to create. If 0, removes the meta box
\return error if any
*/
GF_Err gf_isom_set_meta_type(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 metaType);

/*! removes meta XML info if any
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\return error if any
*/
GF_Err gf_isom_remove_meta_xml(GF_ISOFile *isom_file, Bool root_meta, u32 track_num);

/*! sets meta XML data from file or memory - erase any previously (Binary)XML info
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param XMLFileName the XML file to import as XML item, or NULL if data is specified
\param data buffer containing XML data, or NULL if file is specified
\param data_size size of buffer in bytes, ignored if file is specified
\param IsBinaryXML indicates if the content of the XML file is binary XML (BIM) or not
\return error if any
*/
GF_Err gf_isom_set_meta_xml(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, char *XMLFileName, unsigned char *data, u32 data_size, Bool IsBinaryXML);

/*! gets next available item ID in a meta
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_id set to the next available item ID
\return error if any
*/
GF_Err gf_isom_meta_get_next_item_id(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 *item_id);

/*! adds an item to a meta box from file
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param self_reference if GF_TRUE, indicates that the item is in fact the entire container file
\param resource_path path to the file to add
\param item_name name of the item
\param item_id ID of the item, can be 0
\param item_type four character code of item type
\param mime_type mime type of the item, can be NULL
\param content_encoding content encoding of the item, can be NULL
\param URL URL of the item for external data reference (data is not contained in meta parent file)
\param URN URN of the item for external data reference (data is not contained in meta parent file)
\param image_props image properties information for image items
\return error if any
*/
GF_Err gf_isom_add_meta_item(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, Bool self_reference, char *resource_path, const char *item_name, u32 item_id, u32 item_type, const char *mime_type, const char *content_encoding, const char *URL, const char *URN, GF_ImageItemProperties *image_props);

#endif //GPAC_DISABLE_ISOM

/*! item extend description*/
typedef struct
{
	/*! offset of extent in file*/
	u64 extent_offset;
	/*! size of extent*/
	u64 extent_length;
	/*! index of extent*/
	u64 extent_index;
#ifndef GPAC_DISABLE_ISOM_WRITE
	/*! for storage only, original offset in source file*/
	u64 original_extent_offset;
#endif
} GF_ItemExtentEntry;

#ifndef GPAC_DISABLE_ISOM_WRITE

/*! adds an item to a meta box from memory
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_name name of the item
\param item_id ID of the item, can be NULL, can be 0 in input, set to item ID after call
\param item_type four character code of item type
\param mime_type mime type of the item, can be NULL
\param content_encoding content encoding of the item, can be NULL
\param image_props image properties information for image items
\param data buffer containing the item data
\param data_len size of item data buffer in bytes
\param item_extent_refs list of item extend description, or NULL
\return error if any
*/
GF_Err gf_isom_add_meta_item_memory(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, const char *item_name, u32 *item_id, u32 item_type, const char *mime_type, const char *content_encoding, GF_ImageItemProperties *image_props, char *data, u32 data_len, GF_List *item_extent_refs);

/*! adds an item to a meta box as a reference to a sample
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_name name of the item
\param item_id ID of the item, can be 0
\param item_type four character code of item type
\param mime_type mime type of the item, can be NULL
\param content_encoding content encoding of the item, can be NULL
\param image_props image properties information for image items
\param tk_id source track ID
\param sample_num number of sample to reference
\return error if any
*/
GF_Err gf_isom_add_meta_item_sample_ref(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, const char *item_name, u32 *item_id, u32 item_type, const char *mime_type, const char *content_encoding, GF_ImageItemProperties *image_props, GF_ISOTrackID tk_id, u32 sample_num);

/*! creates an image grid item
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if meta_track_number is 0
\param meta_track_number if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_name name of the item
\param item_id ID of the item, can be 0
\param image_props image properties information for image items
\return error if any
*/
GF_Err gf_isom_iff_create_image_grid_item(GF_ISOFile *isom_file, Bool root_meta, u32 meta_track_number, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props);

/*! creates an image overlay item
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if meta_track_number is 0
\param meta_track_number if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_name name of the item
\param item_id ID of the item, can be 0
\param image_props image properties information for image items
\return error if any
*/
GF_Err gf_isom_iff_create_image_overlay_item(GF_ISOFile *isom_file, Bool root_meta, u32 meta_track_number, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props);

/*! creates an image identity item
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if meta_track_number is 0
\param meta_track_number if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_name name of the item
\param item_id ID of the item, can be 0
\param image_props image properties information for image items
\return error if any
*/
GF_Err gf_isom_iff_create_image_identity_item(GF_ISOFile *isom_file, Bool root_meta, u32 meta_track_number, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props);

/*! creates image item(s) from samples of a media track
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if meta_track_number is 0
\param meta_track_number if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param media_track track number to import samples from
\param item_name name of the item
\param item_id ID of the item, can be 0
\param image_props image properties information for image items
\param item_extent_refs list of item extend description, or NULL
\return error if any
*/
GF_Err gf_isom_iff_create_image_item_from_track(GF_ISOFile *isom_file, Bool root_meta, u32 meta_track_number, u32 media_track, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props, GF_List *item_extent_refs);

/*! removes item from meta
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_num 1-based index of the item to remove
\param keep_refs do not modify item reference, typically used when replacing an item
\param keep_props keep property association for properties with 4CC listed in keep_props (coma-seprated list)
\return error if any
*/
GF_Err gf_isom_remove_meta_item(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_num, Bool keep_refs, const char *keep_props);

/*! sets the given item as the primary one
\warning This SHALL NOT be used if the meta has a valid XML data
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_num 1-based index of the item to remove
\return error if any
*/
GF_Err gf_isom_set_meta_primary_item(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_num);

/*! adds an item reference to another item
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param from_id ID of item the reference is from
\param to_id ID of item the reference is to
\param type four character code of reference
\param ref_index set to the 1-based index of the reference
\return error if any
*/
GF_Err gf_isom_meta_add_item_ref(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 from_id, u32 to_id, u32 type, u64 *ref_index);

/*! adds the item to the given group
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_id ID of item to add
\param group_id ID of group, 0 if needs to be determined from the file
\param group_type four character code of group
\return error if any
*/
GF_Err gf_isom_meta_add_item_group(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_id, u32 group_id, u32 group_type);

#endif /*GPAC_DISABLE_ISOM_WRITE*/

/*! gets image item properties
\param isom_file the target ISO file
\param root_meta if GF_TRUE uses meta at the file, otherwise uses meta at the movie level if track number is 0
\param track_num if GF_TRUE and root_meta is GF_FALSE, uses meta at the track level
\param item_id ID of the item
\param out_image_props set to the image properties information of the item
\param unmapped_props will contain all properties (box) not mapped to image properties. May be NULL. DO NOT DESTROY the content of the list

\return error if any
*/
GF_Err gf_isom_get_meta_image_props(GF_ISOFile *isom_file, Bool root_meta, u32 track_num, u32 item_id, GF_ImageItemProperties *out_image_props, GF_List *unmapped_props);

/*! @} */


#endif //GPAC_DISABLE_ISOM

/*!
\addtogroup isotags_grp iTunes tagging
\ingroup iso_grp

@{
*/

/*! iTunes info tags */
typedef enum
{
	/*probe is only used to check if iTunes info are present*/
	GF_ISOM_ITUNE_PROBE = 0,
	/*all is only used to remove all tags*/
	GF_ISOM_ITUNE_RESET = 1,
	GF_ISOM_ITUNE_NAME 				= GF_4CC( 0xA9, 'n', 'a', 'm' ),
	GF_ISOM_ITUNE_ARTIST 			= GF_4CC( 0xA9, 'A', 'R', 'T' ),
	GF_ISOM_ITUNE_ALBUM_ARTIST 		= GF_4CC( 'a', 'A', 'R', 'T' ),
	GF_ISOM_ITUNE_ALBUM				= GF_4CC( 0xA9, 'a', 'l', 'b' ),
	GF_ISOM_ITUNE_GROUP 			= GF_4CC( 0xA9, 'g', 'r', 'p' ),
	GF_ISOM_ITUNE_WRITER 			= GF_4CC( 0xA9, 'w', 'r', 't' ),
	GF_ISOM_ITUNE_COMMENT 			= GF_4CC( 0xA9, 'c', 'm', 't' ),
	GF_ISOM_ITUNE_GENRE_USER		= GF_4CC( 0xA9, 'g', 'e', 'n'),
	GF_ISOM_ITUNE_GENRE 			= GF_4CC( 'g', 'n', 'r', 'e' ),
	GF_ISOM_ITUNE_CREATED 			= GF_4CC( 0xA9, 'd', 'a', 'y' ),
	GF_ISOM_ITUNE_TRACKNUMBER 		= GF_4CC( 't', 'r', 'k', 'n' ),
	GF_ISOM_ITUNE_DISK 				= GF_4CC( 'd', 'i', 's', 'k' ),
	GF_ISOM_ITUNE_TEMPO 			= GF_4CC( 't', 'm', 'p', 'o' ),
	GF_ISOM_ITUNE_COMPILATION 		= GF_4CC( 'c', 'p', 'i', 'l' ),
	GF_ISOM_ITUNE_TV_SHOW 			= GF_4CC( 't', 'v', 's', 'h'),
	GF_ISOM_ITUNE_TV_EPISODE 		= GF_4CC( 't', 'v', 'e', 'n'),
	GF_ISOM_ITUNE_TV_SEASON 		= GF_4CC( 't', 'v', 's', 'n'),
	GF_ISOM_ITUNE_TV_EPISODE_NUM 	= GF_4CC( 't', 'v', 'e', 's'),
	GF_ISOM_ITUNE_TV_NETWORK 		= GF_4CC( 't', 'v', 'n', 'n'),
	GF_ISOM_ITUNE_DESCRIPTION	 	= GF_4CC( 'd', 'e', 's', 'c' ),
	GF_ISOM_ITUNE_LONG_DESCRIPTION	= GF_4CC( 'l', 'd', 'e', 's'),
	GF_ISOM_ITUNE_LYRICS 			= GF_4CC( 0xA9, 'l', 'y', 'r' ),
	GF_ISOM_ITUNE_SORT_NAME 		= GF_4CC( 's', 'o', 'n', 'm' ),
	GF_ISOM_ITUNE_SORT_ARTIST 		= GF_4CC( 's', 'o', 'a', 'r' ),
	GF_ISOM_ITUNE_SORT_ALB_ARTIST 	= GF_4CC( 's', 'o', 'a', 'a' ),
	GF_ISOM_ITUNE_SORT_ALBUM	 	= GF_4CC( 's', 'o', 'a', 'l' ),
	GF_ISOM_ITUNE_SORT_COMPOSER	 	= GF_4CC( 's', 'o', 'c', 'o' ),
	GF_ISOM_ITUNE_SORT_SHOW	 		= GF_4CC( 's', 'o', 's', 'n' ),
	GF_ISOM_ITUNE_COVER_ART 		= GF_4CC( 'c', 'o', 'v', 'r' ),
	GF_ISOM_ITUNE_COPYRIGHT 		= GF_4CC( 'c', 'p', 'r', 't' ),
	GF_ISOM_ITUNE_TOOL 				= GF_4CC( 0xA9, 't', 'o', 'o' ),
	GF_ISOM_ITUNE_ENCODER 			= GF_4CC( 0xA9, 'e', 'n', 'c' ),
	GF_ISOM_ITUNE_PURCHASE_DATE 	= GF_4CC( 'p', 'u', 'r', 'd' ),
	GF_ISOM_ITUNE_PODCAST		 	= GF_4CC( 'p', 'c', 's', 't' ),
	GF_ISOM_ITUNE_PODCAST_URL	 	= GF_4CC( 'p', 'u', 'r', 'l' ),
	GF_ISOM_ITUNE_KEYWORDS 			= GF_4CC( 'k', 'y', 'y', 'w'),
	GF_ISOM_ITUNE_CATEGORY 			= GF_4CC( 'c', 'a', 't', 'g'),
	GF_ISOM_ITUNE_HD_VIDEO 			= GF_4CC( 'h', 'd', 'v', 'd'),
	GF_ISOM_ITUNE_MEDIA_TYPE 		= GF_4CC( 's', 't', 'i', 'k'),
	GF_ISOM_ITUNE_RATING	 		= GF_4CC( 'r', 't', 'n', 'g'),
	GF_ISOM_ITUNE_GAPLESS 			= GF_4CC( 'p', 'g', 'a', 'p' ),
	GF_ISOM_ITUNE_COMPOSER 		= GF_4CC( 0xA9, 'c', 'o', 'm' ),
	GF_ISOM_ITUNE_TRACK 		= GF_4CC( 0xA9, 't', 'r', 'k' ),
	GF_ISOM_ITUNE_CONDUCTOR 	= GF_4CC( 0xA9, 'c', 'o', 'n' ),

	GF_ISOM_ITUNE_ART_DIRECTOR 	= GF_4CC( 0xA9, 'a', 'r', 'd' ),
	GF_ISOM_ITUNE_ARRANGER	 	= GF_4CC( 0xA9, 'a', 'r', 'g' ),
	GF_ISOM_ITUNE_LYRICIST	 	= GF_4CC( 0xA9, 'a', 'u', 't' ),
	GF_ISOM_ITUNE_COPY_ACK	 	= GF_4CC( 0xA9, 'c', 'a', 'k' ),
	GF_ISOM_ITUNE_SONG_DESC	 	= GF_4CC( 0xA9, 'd', 'e', 's' ),
	GF_ISOM_ITUNE_DIRECTOR	 	= GF_4CC( 0xA9, 'd', 'i', 'r' ),
	GF_ISOM_ITUNE_EQ_PRESET	 	= GF_4CC( 0xA9, 'e', 'q', 'u' ),
	GF_ISOM_ITUNE_LINER_NOTES 	= GF_4CC( 0xA9, 'l', 'n', 't' ),
	GF_ISOM_ITUNE_REC_COMPANY 	= GF_4CC( 0xA9, 'm', 'a', 'k' ),
	GF_ISOM_ITUNE_ORIG_ARTIST 	= GF_4CC( 0xA9, 'o', 'p', 'e' ),
	GF_ISOM_ITUNE_PHONO_RIGHTS 	= GF_4CC( 0xA9, 'p', 'h', 'g' ),
	GF_ISOM_ITUNE_PRODUCER	 	= GF_4CC( 0xA9, 'p', 'r', 'd' ),
	GF_ISOM_ITUNE_PERFORMER	 	= GF_4CC( 0xA9, 'p', 'r', 'f' ),
	GF_ISOM_ITUNE_PUBLISHER	 	= GF_4CC( 0xA9, 'p', 'u', 'b' ),
	GF_ISOM_ITUNE_SOUND_ENG	 	= GF_4CC( 0xA9, 's', 'n', 'e' ),
	GF_ISOM_ITUNE_SOLOIST	 	= GF_4CC( 0xA9, 's', 'o', 'l' ),
	GF_ISOM_ITUNE_CREDITS	 	= GF_4CC( 0xA9, 's', 'r', 'c' ),
	GF_ISOM_ITUNE_THANKS	 	= GF_4CC( 0xA9, 't', 'h', 'x' ),
	GF_ISOM_ITUNE_ONLINE	 	= GF_4CC( 0xA9, 'u', 'r', 'l' ),
	GF_ISOM_ITUNE_EXEC_PRODUCER	= GF_4CC( 0xA9, 'x', 'p', 'd' ),
	GF_ISOM_ITUNE_LOCATION	 	= GF_4CC( 0xA9, 'x', 'y', 'z' ),


	GF_ISOM_ITUNE_ITUNES_DATA 	= GF_4CC( '-', '-', '-', '-' ),

	/* not mapped:
Purchase Account 	apID 	UTF-8 string 		iTunesAccount (read only)
Account Type 	akID 	8-bit integer 	Identifies the iTunes Store account type 	iTunesAccountType (read only)
	cnID 	32-bit integer 	iTunes Catalog ID, used for combing SD and HD encodes in iTunes 	cnID
Country Code 	sfID 	32-bit integer 	Identifies in which iTunes Store a file was bought 	iTunesCountry (read only)
	atID 	32-bit integer 	Use? 	atID
	plID 	64-bit integer 	Use?
	geID 	32-bit integer 	Use? 	geID
	©st3 	UTF-8 string 	Use?
	*/

} GF_ISOiTunesTag;

#ifndef GPAC_DISABLE_ISOM

/*! gets the given itunes tag info.
\warning 'genre' may be coded by ID, the libisomedia doesn't translate the ID. In such a case, the result data is set to NULL and the data_len to the genre ID

\param isom_file the target ISO file
\param tag the tag to query
\param data set to the tag data pointer - do not modify
\param data_len set to the size of the tag data
\return error if any (GF_URL_ERROR if no tag is present in the file)
*/
GF_Err gf_isom_apple_get_tag(GF_ISOFile *isom_file, GF_ISOiTunesTag tag, const u8 **data, u32 *data_len);

/*! enumerate itunes tags.

\param isom_file the target ISO file
\param idx 0-based index of the tag to get
\param out_tag set to the tag code
\param data set to the tag data pointer - do not modify
\param data_len set to the size of the tag data. Data is set to NULL and data_size to 1 if the associated tag has no data
\param out_int_val set to the int/bool/frac numerator type for known tags, in which case data is set to NULL
\param out_int_val2 set to the frac denominator for known tags, in which case data is set to NULL
\param out_flags set to the flags value of the data container box
\return error if any (GF_URL_ERROR if no more tags)
*/
GF_Err gf_isom_apple_enum_tag(GF_ISOFile *isom_file, u32 idx, GF_ISOiTunesTag *out_tag, const u8 **data, u32 *data_len, u64 *out_int_val, u32 *out_int_val2, u32 *out_flags);


/*! enumerate WMA tags.

\param isom_file the target ISO file
\param idx 0-based index of the tag to get
\param out_tag set to the tag name
\param data set to the tag data pointer - do not modify
\param data_len set to the size of the tag data
\param version  set to the WMA tag version
\param data_type set to the WMA data type
\return error if any (GF_URL_ERROR if no more tags)
*/
GF_Err gf_isom_wma_enum_tag(GF_ISOFile *isom_file, u32 idx, char **out_tag, const u8 **data, u32 *data_len, u32 *version, u32 *data_type);

/*! QT key types */
typedef enum
{
	GF_QT_KEY_OPAQUE=0,
	GF_QT_KEY_UTF8=1,
	GF_QT_KEY_UTF16_BE=2,
	GF_QT_KEY_JIS=3,
	GF_QT_KEY_UTF8_SORT=4,
	GF_QT_KEY_UTF16_SORT=5,
	GF_QT_KEY_JPEG=13,
	GF_QT_KEY_PNG=14,
	GF_QT_KEY_SIGNED_VSIZE=21,
	GF_QT_KEY_UNSIGNED_VSIZE=22,
	GF_QT_KEY_FLOAT=23,
	GF_QT_KEY_DOUBLE=24,
	GF_QT_KEY_BMP=27,
	GF_QT_KEY_METABOX=28,
	GF_QT_KEY_SIGNED_8=65,
	GF_QT_KEY_SIGNED_16=66,
	GF_QT_KEY_SIGNED_32=67,
	GF_QT_KEY_POINTF=70,
	GF_QT_KEY_SIZEF=71,
	GF_QT_KEY_RECTF=72,
	GF_QT_KEY_SIGNED_64=74,
	GF_QT_KEY_UNSIGNED_8=75,
	GF_QT_KEY_UNSIGNED_16=76,
	GF_QT_KEY_UNSIGNED_32=77,
	GF_QT_KEY_UNSIGNED_64=78,
	GF_QT_KEY_MATRIXF=79,

	//used to remove a key
	GF_QT_KEY_REMOVE=0xFFFFFFFF
} GF_QTKeyType;


/*! QT userdata key*/
typedef struct
{
	/*! key name*/
	const char *name;
	/*! key namespace 4CC*/
	u32 ns;

	/*! key type*/
	GF_QTKeyType type;
	union {
		/*! UTF-8 string, for GF_QT_KEY_UTF8 and GF_QT_KEY_UTF8_SORT */
		const char *string;
		/*! data, for unsupported types, image types, UTF16 types and metabox */
		struct _tag_data {
			/*! data */
			const u8 *data;
			/*! size */
			u32 data_len;
		} data;
		/*! unsigned integer value*/
		u64 uint;
		/*! signed integer value*/
		s64 sint;
		/*! number value for GF_QT_KEY_FLOAT and GF_QT_KEY_DOUBLE*/
		Double number;
		/*! 2D float value, for GF_QT_KEY_POINTF and GF_QT_KEY_SIZEFF*/
		struct _tag_vec2 {
			/*! x-coord*/
			Float x;
			/*! y-coord*/
			Float y;
		} pos_size;
		/*! 4D value, for GF_QT_KEY_RECTF*/
		struct _tag_rec {
			/*! x-coord*/
			Float x;
			/*! y-coord*/
			Float y;
			/*! width*/
			Float w;
			/*! height*/
			Float h;
		} rect;
		/*! 2x3 matrix */
		Double matrix[9];
	} value;
} GF_QT_UDTAKey;

/*! enumerate QT keys tags.

\param isom_file the target ISO file
\param idx 0-based index of the tag to get
\param out_key key to be filled with key at given index
\return error if any (GF_URL_ERROR if no more tags)
*/
GF_Err gf_isom_enum_udta_keys(GF_ISOFile *isom_file, u32 idx, GF_QT_UDTAKey *out_key);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! sets the given tag info.

\param isom_file the target ISO file
\param tag the tag to set
\param data tag data buffer or string to parse
\param data_len size of the tag data buffer. If data is NULL and and data_len not  0, removes the given tag
\param int_val value for integer/boolean tags. If data and data_len are set, parse data as string  to get the value
\param int_val2 value for fractional  tags. If data and data_len are set, parse data as string to get the value
\return error if any
*/
GF_Err gf_isom_apple_set_tag(GF_ISOFile *isom_file, GF_ISOiTunesTag tag, const u8 *data, u32 data_len, u64 int_val, u32 int_val2);


/*! sets the given WMA tag info (only string tags are supported).

\param isom_file the target ISO file
\param name name of the tag to set
\param value string value to set
\return error if any
*/
GF_Err gf_isom_wma_set_tag(GF_ISOFile *isom_file, char *name, char *value);

/*! sets key (QT style metadata)
\param isom_file the target ISO file
\param key the key to use. if NULL, removes ALL keys
\return error if any
*/
GF_Err gf_isom_set_qt_key(GF_ISOFile *isom_file, GF_QT_UDTAKey *key);

/*! sets compatibility tag on AVC tracks (needed by iPod to play files... hurray for standards)
\param isom_file the target ISO file
\param trackNumber the target track
\return error if any
*/
GF_Err gf_isom_set_ipod_compatible(GF_ISOFile *isom_file, u32 trackNumber);

#endif /*GPAC_DISABLE_ISOM_WRITE*/

/*! @} */

/*!
\addtogroup isogrp_grp Track Groups
\ingroup iso_grp

@{
*/

/*! gets the number of switching groups declared in this track if any
\param isom_file the target ISO file
\param trackNumber the target track
\param alternateGroupID alternate group id of track if speciifed, 0 otherwise
\param nb_groups set to number of switching groups defined for this track
\return error if any
*/
GF_Err gf_isom_get_track_switch_group_count(GF_ISOFile *isom_file, u32 trackNumber, u32 *alternateGroupID, u32 *nb_groups);

/*! get the list of criteria (expressed as 4CC IDs, cf 3GPP TS 26.244)
\param isom_file the target ISO file
\param trackNumber the track number
\param group_index the 1-based index of the group to inspect
\param switchGroupID set to the ID of the switch group if any, 0 otherwise (alternate-only group)
\param criteriaListSize set to the number of criteria items in returned list
\return list of criteria (four character codes, cf 3GPP TS 26.244) for the switch group
*/
const u32 *gf_isom_get_track_switch_parameter(GF_ISOFile *isom_file, u32 trackNumber, u32 group_index, u32 *switchGroupID, u32 *criteriaListSize);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! sets a new (switch) group for this track
\param isom_file the target ISO file
\param trackNumber the target track
\param trackRefGroup number of a track belonging to the same alternate group. If 0, a new alternate group will be created for this track
\param is_switch_group if set, indicates that a switch group identifier shall be assigned to the created group. Otherwise, the criteria list is associated with the entire alternate group
\param switchGroupID set to the ID of the switch group. On input, specifies the desired switchGroupID to use; if value is 0, next available switchGroupID in file is used. On output, is set to the switchGroupID used.
\param criteriaList list of four character codes used as criteria - cf 3GPP TS 26.244
\param criteriaListCount number of criterias in list
\return error if any
*/
GF_Err gf_isom_set_track_switch_parameter(GF_ISOFile *isom_file, u32 trackNumber, u32 trackRefGroup, Bool is_switch_group, u32 *switchGroupID, u32 *criteriaList, u32 criteriaListCount);

/*! resets track switch group information
\param isom_file the target ISO file
\param trackNumber the target track
\param reset_all_group if GF_TRUE, resets the entire alternate group this track belongs to; otherwise, resets switch group for the track only
\return error if any
*/
GF_Err gf_isom_reset_track_switch_parameter(GF_ISOFile *isom_file, u32 trackNumber, Bool reset_all_group);

/*! resets all track switch group information in the entire movie
\param isom_file the target ISO file
\return error if any
*/
GF_Err gf_isom_reset_switch_parameters(GF_ISOFile *isom_file);

/*! sets track in group of a given type and ID
\param isom_file the target ISO file
\param trackNumber the target track
\param track_group_id ID of the track group
\param group_type four character code of the track group
\param do_add if GF_FALSE, track is removed from that group, otherwise it is added
\return error if any
*/
GF_Err gf_isom_set_track_group(GF_ISOFile *isom_file, u32 trackNumber, u32 track_group_id, u32 group_type, Bool do_add);

#endif /*GPAC_DISABLE_ISOM_WRITE*/

/*! @} */

/*!
\addtogroup isosubs_grp Subsamples
\ingroup iso_grp

@{
*/

/*! gets serialized subsample info for the sample
The buffer is formatted as N times [(u32)flags(u32)sub_size(u32)codec_param(u8)priority(u8) discardable]
If several subsample info are present, they are gathered by flags

\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number
\param osize set to output buffer size
\return the serialized buffer, or NULL oif no associated subsample*/
u8 *gf_isom_sample_get_subsamples_buffer(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 *osize);

/*! checks if a sample has subsample information
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number. Set to 0 to check for presence of subsample info (will return 1 or 0 in this case)
\param flags the subsample flags to query (may be 0)
\return the number of subsamples in the given sample for the given flags*/
u32 gf_isom_sample_has_subsamples(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 flags);

/*! gets subsample information on a sample
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number
\param flags the subsample flags to query (may be 0)
\param subSampleNumber the 1-based index of the subsample (see \ref gf_isom_sample_has_subsamples)
\param size set to the subsample size
\param priority set to the subsample priority
\param reserved set to the subsample reserved value (may be used by derived specifications)
\param discardable set to GF_TRUE if subsample is discardable
\return error if any*/
GF_Err gf_isom_sample_get_subsample(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 flags, u32 subSampleNumber, u32 *size, u8 *priority, u32 *reserved, Bool *discardable);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*! adds subsample information to a given sample. Subsample information shall be added in increasing order of sampleNumbers, insertion of information is not supported

\note it is possible to  add subsample information for samples not yet added to the file
\note specifying 0 as subSampleSize will remove the last subsample information if any
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number
\param flags the subsample flags to query (may be 0)
\param subSampleSize size of the subsample. If 0, this will remove the last subsample information if any
\param priority the subsample priority
\param reserved the subsample reserved value (may be used by derived specifications)
\param discardable indicates if the subsample is discardable
\return error if any*/
GF_Err gf_isom_add_subsample(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 flags, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable);


#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
/*! adds subsample information for the latest sample added to the current track fragment
\param isom_file the target ISO file
\param TrackID the ID of the target track
\param flags the subsample flags to query (may be 0)
\param subSampleSize size of the subsample. If 0, this will remove the last subsample information if any
\param priority the subsample priority
\param reserved the subsample reserved value (may be used by derived specifications)
\param discardable indicates if the subsample is discardable
\return error if any*/
GF_Err gf_isom_fragment_add_subsample(GF_ISOFile *isom_file, GF_ISOTrackID TrackID, u32 flags, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable);
#endif // GPAC_DISABLE_ISOM_FRAGMENTS

#endif //GPAC_DISABLE_ISOM_WRITE

/*! @} */

/*!
\addtogroup isosgdp_grp Sample Groups
\ingroup iso_grp

@{
*/

/*! defined sample groups in GPAC*/
enum {
	GF_ISOM_SAMPLE_GROUP_ROLL = GF_4CC( 'r', 'o', 'l', 'l'),
	GF_ISOM_SAMPLE_GROUP_PROL = GF_4CC( 'p', 'r', 'o', 'l'),
	GF_ISOM_SAMPLE_GROUP_RAP = GF_4CC( 'r', 'a', 'p', ' ' ),
	GF_ISOM_SAMPLE_GROUP_SEIG = GF_4CC( 's', 'e', 'i', 'g' ),
	GF_ISOM_SAMPLE_GROUP_OINF = GF_4CC( 'o', 'i', 'n', 'f'),
	GF_ISOM_SAMPLE_GROUP_LINF = GF_4CC( 'l', 'i', 'n', 'f'),
	GF_ISOM_SAMPLE_GROUP_TRIF = GF_4CC( 't', 'r', 'i', 'f' ),
	GF_ISOM_SAMPLE_GROUP_NALM = GF_4CC( 'n', 'a', 'l', 'm'),
	GF_ISOM_SAMPLE_GROUP_TELE = GF_4CC( 't', 'e', 'l', 'e'),
	GF_ISOM_SAMPLE_GROUP_SAP = GF_4CC( 's', 'a', 'p', ' '),
	GF_ISOM_SAMPLE_GROUP_ALST = GF_4CC( 'a', 'l', 's', 't'),
	GF_ISOM_SAMPLE_GROUP_RASH = GF_4CC( 'r', 'a', 's', 'h'),
	GF_ISOM_SAMPLE_GROUP_AVLL = GF_4CC( 'a', 'v', 'l', 'l'), //p15
	GF_ISOM_SAMPLE_GROUP_AVSS = GF_4CC( 'a', 'v', 's', 's'), //p15
	GF_ISOM_SAMPLE_GROUP_DTRT = GF_4CC( 'd', 't', 'r', 't'), //p15
	GF_ISOM_SAMPLE_GROUP_MVIF = GF_4CC( 'm', 'v', 'i', 'f'), //p15
	GF_ISOM_SAMPLE_GROUP_SCIF = GF_4CC( 's', 'c', 'i', 'f'), //p15
	GF_ISOM_SAMPLE_GROUP_SCNM = GF_4CC( 's', 'c', 'n', 'm'), //p15
	GF_ISOM_SAMPLE_GROUP_STSA = GF_4CC( 's', 't', 's', 'a'), //p15
	GF_ISOM_SAMPLE_GROUP_TSAS = GF_4CC( 't', 's', 'a', 's'), //p15
	GF_ISOM_SAMPLE_GROUP_SYNC = GF_4CC( 's', 'y', 'n', 'c'), //p15
	GF_ISOM_SAMPLE_GROUP_TSCL = GF_4CC( 't', 's', 'c', 'l'), //p15
	GF_ISOM_SAMPLE_GROUP_VIPR = GF_4CC( 'v', 'i', 'p', 'r'), //p15
	GF_ISOM_SAMPLE_GROUP_LBLI = GF_4CC( 'l', 'b', 'l', 'i'), //p15
	GF_ISOM_SAMPLE_GROUP_3GAG = GF_4CC( '3', 'g', 'a', 'g'), //3gpp
	GF_ISOM_SAMPLE_GROUP_AVCB = GF_4CC( 'a', 'v', 'c', 'b'), //avif
	GF_ISOM_SAMPLE_GROUP_SPOR = GF_4CC( 's', 'p', 'o', 'r'), //p15
	GF_ISOM_SAMPLE_GROUP_SULM = GF_4CC( 's', 'u', 'l', 'm'), //p15
	GF_ISOM_SAMPLE_GROUP_ESGH = GF_4CC( 'e', 's', 'g', 'h'), //p12
	GF_ISOM_SAMPLE_GROUP_ILCE = GF_4CC( 'i', 'l', 'c', 'e'), //uncv
};

/*! gets 'rap ' and 'roll' group info for the given sample
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number
\param is_rap set to GF_TRUE if sample is a rap (open gop), GF_FALSE otherwise
\param roll_type set to GF_ISOM_SAMPLE_ROLL if sample has roll information, GF_ISOM_SAMPLE_PREROLL if sample has preroll information, GF_ISOM_SAMPLE_ROLL_NONE otherwise
\param roll_distance if sample has roll information, set to roll distance
\return error if any*/
GF_Err gf_isom_get_sample_rap_roll_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, Bool *is_rap, GF_ISOSampleRollType *roll_type, s32 *roll_distance);

/*! returns opaque data of sample group
\param isom_file the target ISO file
\param trackNumber the target track
\param sample_group_description_index index of sample group description entry to query
\param grouping_type four character code of grouping type of sample group description to query
\param default_index set to the default index for this sample group description if any, 0 otherwise (no defaults)
\param data set to the internal sample group description data buffer
\param size set to size of the sample group description data buffer
\return GF_TRUE if found, GF_FALSE otherwise*/
Bool gf_isom_get_sample_group_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sample_group_description_index, u32 grouping_type, u32 *default_index, const u8 **data, u32 *size);

/*! gets sample group description index for a given sample and grouping type.
\param isom_file the target ISO file
\param trackNumber the target track
\param sample_number sample number to query
\param grouping_type four character code of grouping type of sample group description to query
\param grouping_type_parameter  grouping type parameter of sample group description to query
\param sampleGroupDescIndex set to the 1-based sample group description index, or 0 if no sample group of this type is associated
\return error if any
*/
GF_Err gf_isom_get_sample_to_group_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sample_number, u32 grouping_type, u32 grouping_type_parameter, u32 *sampleGroupDescIndex);

/*! checks if a track as a CENC seig sample group used for key rolling
\param isom_file the target ISO file
\param trackNumber the target track
\param has_selective set to TRUE if some entries describe unprotected samples - may be NULL
\param has_roll set to TRUE if more than one key defined - may be NULL
\return GF_TRUE if found, GF_FALSE otherwise*/
Bool gf_isom_has_cenc_sample_group(GF_ISOFile *isom_file, u32 trackNumber, Bool *has_selective, Bool *has_roll);

/*! gets HEVC tiling info
\param isom_file the target ISO file
\param trackNumber the target track
\param sample_group_description_index index of sample group description entry to query
\param default_sample_group_index set to the default index for this sample group description if any, 0 otherwise (no defaults)
\param id set to the tile group ID
\param independent set to independent flag of the tile group (0: not constrained, 1: constrained in layer, 2: all intra slices)
\param full_frame set to GF_TRUE if the tile corresponds to the entire picture
\param x set to the horizontal position in pixels
\param y set to the vertical position in pixels
\param w set to the width in pixels
\param h set to the height in pixels
\return GF_TRUE if found, GF_FALSE otherwise*/
Bool gf_isom_get_tile_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sample_group_description_index, u32 *default_sample_group_index, u32 *id, u32 *independent, Bool *full_frame, u32 *x, u32 *y, u32 *w, u32 *h);


/*! enumerates custom sample groups (not natively supported by this library) for a given sample
\param isom_file the target ISO file
\param trackNumber the target track
\param sample_number the target sample
\param sgrp_idx the current index. Must be set to 0 on first call, incremented by this call on each success, must not be NULL
\param sgrp_type set to the grouping type, or set to 0 if no more sample group descriptions, must not be NULL
\param sgrp_flags set to the grouping flags, (0x1: static_group_description, 0x2: static_mapping)
\param sgrp_parameter set to the grouping_type_parameter or 0 if not defined
\param sgrp_data set to the sample group description data, may be NULL - MUST be freed by caller
\param sgrp_size set to the sample group description size, may be NULL
\return error if any
*/
GF_Err gf_isom_enum_sample_group(GF_ISOFile *isom_file, u32 trackNumber, u32 sample_number, u32 *sgrp_idx, u32 *sgrp_type, u32 *sgrp_flags, u32 *sgrp_parameter, u8 **sgrp_data, u32 *sgrp_size);

/*! enumerates custom sample auxiliary data (not natively supported by this library) for a given sample
\param isom_file the target ISO file
\param trackNumber the target track
\param sample_number the target sample
\param sai_idx the current index. Must be et to 0 on first call, incremented by this call on each success, must not be NULL
\param sai_type set to the grouping type, or set to 0 if no more sample group descriptions, must not be NULL
\param sai_parameter set to the grouping_type_parameter or 0 if not defined
\param sai_data set (allocated) to the sample group description data, must not be NULL and must be freed by caller
\param sai_size set to the sample group description size, must not be NULL
\return error if any
*/
GF_Err gf_isom_enum_sample_aux_data(GF_ISOFile *isom_file, u32 trackNumber, u32 sample_number, u32 *sai_idx, u32 *sai_type, u32 *sai_parameter, u8 **sai_data, u32 *sai_size);

#ifndef GPAC_DISABLE_ISOM_WRITE

/*! sets rap flag for sample_number - this is used by non-IDR RAPs in AVC (also in USAC) were SYNC flag (stss table) cannot be used
\warning Sample group info MUST be added in order (no insertion in the tables)

\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number
\param is_rap indicates if the sample is a RAP (open gop) sample
\param num_leading_samples indicates the number of leading samples (samples after this RAP that have dependences on samples before this RAP and hence should be discarded when tuning in)
\return error if any
*/
GF_Err gf_isom_set_sample_rap_group(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, Bool is_rap, u32 num_leading_samples);

/*! sets roll_distance info for sample_number (number of frames before (<0) or after (>0) this sample to have a complete refresh of the decoded data (used by GDR in AVC)

\warning Sample group info MUST be added in order (no insertion in the tables)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number. If 0, assumes last added sample. If 0xFFFFFFFF, marks all samples as belonging to the roll group
\param roll_type indicates  the sample roll recovery type
\param roll_distance indicates the roll distance before a correct decoding is produced
\return error if any
*/
GF_Err gf_isom_set_sample_roll_group(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, GF_ISOSampleRollType roll_type, s16 roll_distance);

/*! sets encryption group for a sample number
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number
\param isEncrypted isEncrypted flag
\param crypt_byte_block crypt block size for pattern encryption, can be 0
\param skip_byte_block skip block size for pattern encryption, can be 0
\param key_info multikey descriptor (cf CENC and GF_PROP_PID_CENC_KEY)
\param key_info_size multikey descriptor size
\return error if any
*/
GF_Err gf_isom_set_sample_cenc_group(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u8 isEncrypted, u32 crypt_byte_block, u32 skip_byte_block, u8 *key_info, u32 key_info_size);


/*! sets a sample using the default CENC parameters in a CENC saig sample group SEIG, creating a sample group description if needed (when seig is already defined)
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number
\return error if any
*/
GF_Err gf_isom_set_sample_cenc_default_group(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber);

/*! adds the given blob as a sample group description entry of the given grouping type.
\param isom_file the target ISO file
\param trackNumber the target track
\param grouping_type the four character code of the grouping type
\param data the payload of the sample group description
\param data_size the size of the payload
\param is_default if GF_TRUE, thie created entry will be marked as the default entry for the sample group description
\param sampleGroupDescriptionIndex is set to the sample group description index (optional, can be NULL)
\return error if any
*/
GF_Err gf_isom_add_sample_group_info(GF_ISOFile *isom_file, u32 trackNumber, u32 grouping_type, void *data, u32 data_size, Bool is_default, u32 *sampleGroupDescriptionIndex);

/*! removes a sample group description of the give grouping type, if found
\param isom_file the target ISO file
\param trackNumber the target track
\param grouping_type the four character code of the grouping type
\return error if any
*/
GF_Err gf_isom_remove_sample_group(GF_ISOFile *isom_file, u32 trackNumber, u32 grouping_type);

/*! adds the given blob as a sample group description entry of the given grouping type for the given sample.
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number.Use 0 for setting sample group info to last sample in a track fragment
\param grouping_type the four character code of the grouping type
\param grouping_type_parameter associated grouping type parameter (usually 0)
\param data the payload of the sample group description
\param data_size the size of the payload
\param sgpd_flags flags for sgpd: 1: static description, 2, static mapping, 1<<30: essential sample group, 1<<31: default sample description
\return error if any
*/
GF_Err gf_isom_set_sample_group_description(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 grouping_type, u32 grouping_type_parameter, void *data, u32 data_size, u32 sgpd_flags);


/*! adds a sample to the given sample group
\param isom_file the target ISO file
\param trackNumber the target track
\param sampleNumber the target sample number
\param grouping_type the four character code of the grouping type
\param sampleGroupDescriptionIndex the 1-based index of the sample group description entry
\param grouping_type_parameter the grouping type paramter (see ISO/IEC 14496-12)
\return error if any
*/
GF_Err gf_isom_add_sample_info(GF_ISOFile *isom_file, u32 trackNumber, u32 sampleNumber, u32 grouping_type, u32 sampleGroupDescriptionIndex, u32 grouping_type_parameter);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
/*! sets sample group descriptions storage in trafs and not in initial movie (Smooth compatibility)
\param isom_file the target ISO file
\return error if any*/
GF_Err gf_isom_set_sample_group_in_traf(GF_ISOFile *isom_file);
#endif

#endif // GPAC_DISABLE_ISOM_WRITE


/*! @} */

#endif /*GPAC_DISABLE_ISOM*/

#ifdef __cplusplus
}
#endif


#endif	/*_GF_ISOMEDIA_H_*/


