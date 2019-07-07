/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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
 *	\file <gpac/isomedia.h>
 *	\brief ISOBMFF parsing and writing library.
 */

/*!
 *	\addtogroup iso_grp ISO Base Media File
 *	\ingroup isobmf_grp
 *	\brief ISOBMF, 3GPP, AVC and HEVC file format utilities.
 *
 *This section documents the reading and writing of ISOBMF, 3GPP, AVC and HEVC file format  using GPAC.
 *	@{
 */

#include <gpac/tools.h>


/********************************************************************
				FILE FORMAT CONSTANTS
********************************************************************/

/*Modes for file opening
		NOTE 1: All the READ function in this API can be used in EDIT/WRITE mode.
However, some unexpected errors or values may happen in that case, depending
on how much modifications you made (timing, track with 0 samples, ...)
		On the other hand, none of the EDIT/WRITE functions will work in
READ mode.
		NOTE 2: The output structure of a edited file will sometimes be different
from the original file, but the media-data and meta-data will be identical.
The only change happens in the file media-data container(s) during edition
		NOTE 3: when editing the file, you MUST set the final name of the modified file
to something different. This API doesn't allow file overwriting.
*/
enum
{
	/*Opens file for dumping: same as read-only but keeps all movie fragments info untouched*/
	GF_ISOM_OPEN_READ_DUMP = 0,
	/*Opens a file in READ ONLY mode*/
	GF_ISOM_OPEN_READ,
	/*Opens a file in WRITE ONLY mode. Media Data is captured on the fly and storage mode is always flat (moov at end).
	// In this mode, the editing functions are disabled.*/
	GF_ISOM_OPEN_WRITE,
	/*Opens an existing file in EDIT mode*/
	GF_ISOM_OPEN_EDIT,
	/*Creates a new file in EDIT mode*/
	GF_ISOM_WRITE_EDIT,
	/*Opens an existing file for fragment concatenation*/
	GF_ISOM_OPEN_CAT_FRAGMENTS,
};

/*Movie Options for file writing*/
enum
{
	/*FLAT: the MediaData (MPEG4 ESs) is stored at the beginning of the file*/
	GF_ISOM_STORE_FLAT = 1,
	/*STREAMABLE: the MetaData (File Info) is stored at the beginning of the file
	for fast access during download*/
	GF_ISOM_STORE_STREAMABLE,
	/*INTERLEAVED: Same as STREAMABLE, plus the media data is mixed by chunk  of fixed duration*/
	GF_ISOM_STORE_INTERLEAVED,
	/*INTERLEAVED +DRIFT: Same as INTERLEAVED, and adds time drift control to avoid creating too long chunks*/
	GF_ISOM_STORE_DRIFT_INTERLEAVED,
	/*tightly interleaves samples based on their DTS, therefore allowing better placement of samples in the file.
	This is used for both http interleaving and Hinting optimizations*/
	GF_ISOM_STORE_TIGHT

};

/*Some track may depend on other tracks for several reasons. They reference these tracks
through the following Reference Types*/
enum
{
	/*ref type for the OD track dependencies*/
	GF_ISOM_REF_OD			= GF_4CC( 'm', 'p', 'o', 'd' ),
	/*ref type for stream dependencies*/
	GF_ISOM_REF_DECODE = GF_4CC( 'd', 'p', 'n', 'd' ),
	/*ref type for OCR (Object Clock Reference) dependencies*/
	GF_ISOM_REF_OCR				= GF_4CC( 's', 'y', 'n', 'c' ),
	/*ref type for IPI (Intellectual Property Information) dependencies*/
	GF_ISOM_REF_IPI				= GF_4CC( 'i', 'p', 'i', 'r' ),
	/*this track describes the referenced tr*/
	GF_ISOM_REF_META		= GF_4CC( 'c', 'd', 's', 'c' ),
	/*ref type for Hint tracks*/
	GF_ISOM_REF_HINT		= GF_4CC( 'h', 'i', 'n', 't' ),
	/*ref type for QT Chapter tracks*/
	GF_ISOM_REF_CHAP		= GF_4CC( 'c', 'h', 'a', 'p' ),
	/*ref type for the SVC and SHVC tracks*/
	GF_ISOM_REF_BASE = GF_4CC( 's', 'b', 'a', 's' ),
	GF_ISOM_REF_SCAL = GF_4CC( 's', 'c', 'a', 'l' ),
	GF_ISOM_REF_TBAS = GF_4CC( 't', 'b', 'a', 's' ),
	GF_ISOM_REF_SABT = GF_4CC( 's', 'a', 'b', 't' ),
	GF_ISOM_REF_OREF = GF_4CC( 'o', 'r', 'e', 'f' ),
	//this track uses fonts carried/defined in the referenced track
	GF_ISOM_REF_FONT = GF_4CC( 'f', 'o', 'n', 't' ),
	//this track depends on the referenced hint track, i.e., it should only be used if the referenced hint track is used.
	GF_ISOM_REF_HIND = GF_4CC( 'h', 'i', 'n', 'd' ),
	//this track contains auxiliary depth video information for the referenced video track
	GF_ISOM_REF_VDEP = GF_4CC( 'v', 'd', 'e', 'p' ),
	//this track contains auxiliary parallax video information for the referenced video track
	GF_ISOM_REF_VPLX = GF_4CC( 'v', 'p', 'l', 'x' ),
	//this track contains subtitle, timed text or overlay graphical information for the referenced track or any track in the alternate group to which the track belongs, if any
	GF_ISOM_REF_SUBT = GF_4CC( 's', 'u', 'b', 't' ),
	//thumbail track
	GF_ISOM_REF_THUMB = GF_4CC( 't', 'h', 'm', 'b' ),
	/*DRC*/
	//additionnal audio track
	GF_ISOM_REF_ADDA			= GF_4CC( 'a', 'd', 'd', 'a' ),
	//DRC metadata
	GF_ISOM_REF_ADRC			= GF_4CC( 'a', 'd', 'r', 'c' ),
	//item->track location
	GF_ISOM_REF_ILOC			= GF_4CC( 'i', 'l', 'o', 'c' ),
	//AVC dep stream
	GF_ISOM_REF_AVCP			= GF_4CC( 'a', 'v', 'c', 'p' ),
	//AVC switch to
	GF_ISOM_REF_SWTO			= GF_4CC( 's', 'w', 't', 'o' ),
	//AVC switch from
	GF_ISOM_REF_SWFR			= GF_4CC( 's', 'w', 'f', 'r' ),

	//Time code
	GF_ISOM_REF_TMCD			= GF_4CC( 't', 'm', 'c', 'd' ),
	//Structural dependency
	GF_ISOM_REF_CDEP			= GF_4CC( 'c', 'd', 'e', 'p' ),
	//transcript
	GF_ISOM_REF_SCPT			= GF_4CC( 's', 'c', 'p', 't' ),
	//nonprimary source description
	GF_ISOM_REF_SSRC			= GF_4CC( 's', 's', 'r', 'c' ),
	//layer audio track dependency
	GF_ISOM_REF_LYRA			= GF_4CC( 'l', 'y', 'r', 'a' ),

	/* File Delivery Item Information Extension */
	GF_ISOM_REF_FDEL			= GF_4CC( 'f', 'd', 'e', 'l' ),

};

//defined sample groups in GPAC
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
	GF_ISOM_SAMPLE_GROUP_AVCB = GF_4CC( 'a', 'v', 'c', 'b'), //3gpp
};

/*Track Edition flag*/
enum {
	/*empty segment in the track (no media for this segment)*/
	GF_ISOM_EDIT_EMPTY		=	0x00,
	/*dwelled segment in the track (one media sample for this segment)*/
	GF_ISOM_EDIT_DWELL		=	0x01,
	/*normal segment in the track*/
	GF_ISOM_EDIT_NORMAL		=	0x02
};

/*Generic Media Types (YOU HAVE TO USE ONE OF THESE TYPES FOR COMPLIANT ISO MEDIA FILES)*/
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
	GF_ISOM_MEDIA_QTVR		= GF_4CC( 'q', 't', 'v', 'r' )
};


/*DRM related code points*/
enum
{
	GF_ISOM_BOX_UUID_PSEC	= GF_4CC( 'P', 'S', 'E', 'C' ),
	GF_ISOM_BOX_TYPE_SENC	= GF_4CC( 's', 'e', 'n', 'c'),
	GF_ISOM_BOX_TYPE_PSSH	= GF_4CC( 'p', 's', 's', 'h'),

	/* Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_ISMACRYP_SCHEME	= GF_4CC( 'i', 'A', 'E', 'C' ),
	/* Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_OMADRM_SCHEME	= GF_4CC('o','d','k','m'),

	/* Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_CENC_SCHEME	= GF_4CC('c','e','n','c'),

	/* Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_CBC_SCHEME	= GF_4CC('c','b','c','1'),

	/* Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_ADOBE_SCHEME	= GF_4CC('a','d','k','m'),

	/* Pattern Encryption Scheme Type in the SchemeTypeInfoBox */
	GF_ISOM_CENS_SCHEME	= GF_4CC('c','e','n','s'),
	GF_ISOM_CBCS_SCHEME	= GF_4CC('c','b','c','s'),
};


/*specific media sub-types - you shall make sure the media sub type is what you expect*/
enum
{
	/*reserved, internal use in the lib. Indicates the track complies to MPEG-4 system
	specification, and the usual OD framework tools may be used*/
	GF_ISOM_SUBTYPE_MPEG4		= GF_4CC( 'M', 'P', 'E', 'G' ),

	/*reserved, internal use in the lib. Indicates the track is of GF_ISOM_SUBTYPE_MPEG4
	but it is encrypted.*/
	GF_ISOM_SUBTYPE_MPEG4_CRYP	= GF_4CC( 'E', 'N', 'C', 'M' ),

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

	/*AV1 media type*/
	GF_ISOM_SUBTYPE_AV01 = GF_4CC('a', 'v', '0', '1'),

	/*Opus media type*/
	GF_ISOM_SUBTYPE_OPUS = GF_4CC('O', 'p', 'u', 's'),
	GF_ISOM_SUBTYPE_FLAC = GF_4CC( 'f', 'L', 'a', 'C' ),

	/* VP */
	GF_ISOM_SUBTYPE_VP08 = GF_4CC('v', 'p', '0', '8'),
	GF_ISOM_SUBTYPE_VP09 = GF_4CC('v', 'p', '0', '9'),
	GF_ISOM_SUBTYPE_VP10 = GF_4CC('v', 'p', '1', '0'),

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

	GF_ISOM_SUBTYPE_LSR1		= GF_4CC( 'l', 's', 'r', '1' ),
	GF_ISOM_SUBTYPE_WVTT		= GF_4CC( 'w', 'v', 't', 't' ),
	GF_ISOM_SUBTYPE_STXT		= GF_4CC( 's', 't', 'x', 't' ),
	GF_ISOM_SUBTYPE_STPP		= GF_4CC( 's', 't', 'p', 'p' ),
	GF_ISOM_SUBTYPE_SBTT		= GF_4CC( 's', 'b', 't', 't' ),
	GF_ISOM_SUBTYPE_METT		= GF_4CC( 'm', 'e', 't', 't' ),
	GF_ISOM_SUBTYPE_METX		= GF_4CC( 'm', 'e', 't', 'x' ),
	GF_ISOM_SUBTYPE_TX3G		= GF_4CC( 't', 'x', '3', 'g' ),
	GF_ISOM_SUBTYPE_TEXT		= GF_4CC( 't', 'e', 'x', 't' ),


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


	/* on-screen colours */
	GF_ISOM_SUBTYPE_NCLX 		= GF_4CC( 'n', 'c', 'l', 'x' ),
	GF_ISOM_SUBTYPE_NCLC 		= GF_4CC( 'n', 'c', 'l', 'c' ),
	GF_ISOM_SUBTYPE_PROF 		= GF_4CC( 'p', 'r', 'o', 'f' ),
	GF_ISOM_SUBTYPE_RICC 		= GF_4CC( 'r', 'I', 'C', 'C' ),

	/* QT audio codecs */
	GF_QT_SUBTYPE_RAW 	= GF_4CC('r','a','w',' '),
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
	GF_QT_SUBTYPE_APCF	= GF_4CC( 'a', 'p', 'c', 'f' ),
	GF_QT_SUBTYPE_AP4X	= GF_4CC( 'a', 'p', '4', 'x' ),
	GF_QT_SUBTYPE_AP4H	= GF_4CC( 'a', 'p', '4', 'h' ),
};




/*direction for sample search (including SyncSamples search)
Function using search allways specify the desired time in composition (presentation) time

		(Sample N-1)	DesiredTime		(Sample N)

FORWARD: will give the next sample given the desired time (eg, N)
BACKWARD: will give the previous sample given the desired time (eg, N-1)
SYNCFORWARD: will search from the desired point in time for a sync sample if any
		If no sync info, behaves as FORWARD
SYNCBACKWARD: will search till the desired point in time for a sync sample if any
		If no sync info, behaves as BACKWARD
SYNCSHADOW: use the sync shadow information to retrieve the sample.
		If no SyncShadow info, behave as SYNCBACKWARD
*/
enum
{
	GF_ISOM_SEARCH_FORWARD		=	1,
	GF_ISOM_SEARCH_BACKWARD		=	2,
	GF_ISOM_SEARCH_SYNC_FORWARD	=	3,
	GF_ISOM_SEARCH_SYNC_BACKWARD	=	4,
	GF_ISOM_SEARCH_SYNC_SHADOW		=	5
};

/*Predefined File Brand codes (MPEG-4 and JPEG2000)*/
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
	GF_ISOM_BRAND_MIF1 = GF_4CC('m', 'i', 'f', '1'),
	GF_ISOM_BRAND_HEIC = GF_4CC('h', 'e', 'i', 'c'),

	/*other iso media brands */
	GF_ISOM_BRAND_ISO1 = GF_4CC( 'i', 's', 'o', '1' ),
	GF_ISOM_BRAND_ISO3 = GF_4CC( 'i', 's', 'o', '3' ),
	GF_ISOM_BRAND_ISO5 = GF_4CC( 'i', 's', 'o', '5' ),
	GF_ISOM_BRAND_ISO6 = GF_4CC( 'i', 's', 'o', '6' ),

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


	/* from ismacryp.c */
	/* OMA DCF DRM Format 2.0 (OMA-TS-DRM-DCF-V2_0-20060303-A) */
	GF_ISOM_BRAND_ODCF = GF_4CC('o','d','c','f'),
	/* OMA PDCF DRM Format 2.1 (OMA-TS-DRM-DCF-V2_1-20070724-C) */
	GF_ISOM_BRAND_OPF2 = GF_4CC('o','p','f','2'),

};


/*MPEG-4 ProfileAndLevel codes*/
enum
{
	GF_ISOM_PL_AUDIO,
	GF_ISOM_PL_VISUAL,
	GF_ISOM_PL_GRAPHICS,
	GF_ISOM_PL_SCENE,
	GF_ISOM_PL_OD,
	GF_ISOM_PL_MPEGJ,
	/*not a profile, just set/unset inlineFlag*/
	GF_ISOM_PL_INLINE,
};

#ifndef GPAC_DISABLE_ISOM

#include <gpac/mpeg4_odf.h>

/*the isomedia file*/
typedef struct __tag_isom GF_ISOFile;

/*a track ID value - just a 32 bit value but typedefed for API safety*/
typedef u32 GF_ISOTrackID;

/*Random Access Point flag*/
typedef enum {
	RAP_REDUNDANT = -1,
	RAP_NO = 0,
	RAP = 1,
	SAP_TYPE_1 = 1,
	SAP_TYPE_2 = 2,
	SAP_TYPE_3 = 3,
	SAP_TYPE_4 = 4,
	SAP_TYPE_5 = 5,
	SAP_TYPE_6 = 6
} SAPType;

/*media sample object*/
typedef struct
{
	/*data size*/
	u32 dataLength;
	/*data with padding if requested*/
	u8 *data;
	/*decoding time*/
	u64 DTS;
	/*relative offset for composition if needed*/
	s32 CTS_Offset;
	SAPType IsRAP;

	/*allocated data size - used only when using static sample in gf_isom_get_sample_ex*/
	u32 alloc_size;
	
	/*number of packed samples in this sample. If 0 or 1, only 1 sample is present
	only used for constant size and constant duration samples*/
	u32 nb_pack;
} GF_ISOSample;


/*creates a new empty sample*/
GF_ISOSample *gf_isom_sample_new();

/*delete a sample. NOTE:the buffer content will be destroyed by default.
if you wish to keep the buffer, set dataLength to 0 in the sample
before deleting it
the pointer is set to NULL after deletion*/
void gf_isom_sample_del(GF_ISOSample **samp);

/********************************************************************
				GENERAL API FUNCTIONS
********************************************************************/

/*get the last fatal error that occured in the file
ANY FUNCTION OF THIS API WON'T BE PROCESSED IF THE FILE HAS AN ERROR
Note: some function may return an error while the movie has no error
the last error is a FatalError, and is not always set if a bad
param is specified...*/
GF_Err gf_isom_last_error(GF_ISOFile *the_file);

/*indicates if target file is an IsoMedia file
  returns 1 if it is a non-special file, 2 if an init segment, 3 if a media segment,
  returns 0 otherwise*/
u32 gf_isom_probe_file(const char *fileName);

/*indicates if target file is an IsoMedia file
  returns 1 if it is a non-special file, 2 if an init segment, 3 if a media segment,
  returns 0 otherwise*/
u32 gf_isom_probe_file_range(const char *fileName, u64 start_range, u64 end_range);

/*indicates if target file is an IsoMedia file
  returns 1 if it is a non-special file, 2 if an init segment, 3 if a media segment,
  returns 0 otherwise (non recognized or too short)*/
u32 gf_isom_probe_data(const u8*inBuf, u32 inSize);

/*Opens an isoMedia File.
If fileName is NULL data will be written in memory ; write with gf_isom_write() ; use gf_isom_get_bs() to get the data ; use gf_isom_delete() to delete the internal data.
tmp_dir: for the 2 edit modes only, specifies a location for temp file. If NULL, the library will use the default
OS temporary file management schemes.*/
GF_ISOFile *gf_isom_open(const char *fileName, u32 OpenMode, const char *tmp_dir);

/*close the file, write it if new/edited - equivalent to gf_isom_write()+gf_isom_delete()*/
GF_Err gf_isom_close(GF_ISOFile *the_file);

/*write the file without deleting (gf_isom_delete())*/
GF_Err gf_isom_write(GF_ISOFile *movie);

/*get access to the data bitstream  - see gf_isom_open()*/
GF_Err gf_isom_get_bs(GF_ISOFile *movie, GF_BitStream **out_bs);

/*delete the movie without saving it.*/
void gf_isom_delete(GF_ISOFile *the_file);

/*Get the mode of an open file*/
u8 gf_isom_get_mode(GF_ISOFile *the_file);

Bool gf_isom_is_JPEG2000(GF_ISOFile *mov);

u64 gf_isom_get_file_size(GF_ISOFile *the_file);

Bool gf_isom_moov_first(GF_ISOFile *movie);

/*freeze order of the current box tree in the file.
By default the library always reorder boxes in the recommended order in the various specifications implemented.
New created tracks or meta items will not have a frozen order of boxes, but the function can be called several time*/
GF_Err gf_isom_freeze_order(GF_ISOFile *file);

/*sets write cache size for files when creating them. If size is 0, writing
only relies on the underlying OS fwrite/fgetc
If movie is NULL, assigns the default write cache size for any new movie*/
GF_Err gf_isom_set_output_buffering(GF_ISOFile *movie, u32 size);

/*when reading a file, indicates that file data is missing the indicated bytes*/
GF_Err gf_isom_set_byte_offset(GF_ISOFile *file, u64 byte_offset);

/********************************************************************
				STREAMING API FUNCTIONS
********************************************************************/
/*open a movie that can be uncomplete in READ_ONLY mode
to use for http streaming & co

NOTE: you must buffer the data to a local file, this mode DOES NOT handle
http/ftp/... streaming

start_range and end_range restricts the media byte range in the URL (used by DASH)
if 0 or end_range<=start_range, the entire URL is used when parsing

BytesMissing is the predicted number of bytes missing for the file to be loaded
Note that if the file is not optimized for streaming, this number is not accurate
If the movie is successfully loaded (the_file non-NULL), BytesMissing is zero
*/
GF_Err gf_isom_open_progressive(const char *fileName, u64 start_range, u64 end_range, GF_ISOFile **the_file, u64 *BytesMissing);

/*If requesting a sample fails with error GF_ISOM_INCOMPLETE_FILE, use this function
to get the number of bytes missing to retrieve the sample*/
u64 gf_isom_get_missing_bytes(GF_ISOFile *the_file, u32 trackNumber);


/*Fragmented movie extensions*/

/*return 0 if movie isn't fragmented, 1 otherwise*/
u32 gf_isom_is_fragmented(GF_ISOFile *the_file);
/*return 0 if track isn't fragmented, 1 otherwise*/
u32 gf_isom_is_track_fragmented(GF_ISOFile *the_file, GF_ISOTrackID TrackID);

/*a file being downloaded may be a fragmented file. In this case only partial info
is available once the file is successfully open (gf_isom_open_progressive), and since there is
no information wrt number fragments (which could actually be generated on the fly
at the sender side), you must call this function on regular bases in order to
load newly downloaded fragments. Note this may result in Track/Movie duration changes
and SampleCount change too ...

if new_location is set, the previous bitstream is changed to this new location, otherwise it is refreshed (disk flush)

*/
GF_Err gf_isom_refresh_fragmented(GF_ISOFile *the_file, u64 *MissingBytes, const char *new_location);

/*check if file has movie info, eg has tracks & dynamic media. Some files may just use
the base IsoMedia structure without "moov" container*/
Bool gf_isom_has_movie(GF_ISOFile *file);

/* check if the file has a top styp box and returns the brand and version of the first styp found */
Bool gf_isom_has_segment(GF_ISOFile *file, u32 *brand, u32 *version);
/*get number of movie fragments in the file*/
u32 gf_isom_segment_get_fragment_count(GF_ISOFile *file);
/*get number of track fragments in the indicated movie fragment (1-based index)*/
u32 gf_isom_segment_get_track_fragment_count(GF_ISOFile *file, u32 moof_index);
/*returns the track ID and get the tfdt of the given track fragment (1-based index) in the indicated movie fragment (1-based index)*/
u32 gf_isom_segment_get_track_fragment_decode_time(GF_ISOFile *file, u32 moof_index, u32 traf_index, u64 *decode_time);

/* Indicates that we want to parse only one moof/mdat at a time
   in order to proceed to next moof, call gf_isom_reset_data_offset
*/
void gf_isom_set_single_moof_mode(GF_ISOFile *file, Bool mode);
/********************************************************************
				READING API FUNCTIONS
********************************************************************/

/*return the number of tracks in the movie, or -1 if error*/
u32 gf_isom_get_track_count(GF_ISOFile *the_file);

/*return the timescale of the movie, 0 if error*/
u32 gf_isom_get_timescale(GF_ISOFile *the_file);

/*return the computed duration of the movie given the media in the sample tables, 0 if error*/
u64 gf_isom_get_duration(GF_ISOFile *the_file);

/*return the duration of the movie as written in the file, regardless of the media data*/
u64 gf_isom_get_original_duration(GF_ISOFile *movie);

/*return the creation info of the movie*/
GF_Err gf_isom_get_creation_time(GF_ISOFile *the_file, u64 *creationTime, u64 *modificationTime);

/*return the trackID of track number n, or 0 if error*/
GF_ISOTrackID gf_isom_get_track_id(GF_ISOFile *the_file, u32 trackNumber);

/*return the track number of the track of specified ID, or 0 if error*/
u32 gf_isom_get_track_by_id(GF_ISOFile *the_file, GF_ISOTrackID trackID);

/*return the original trackID of the track number n, or 0 if error*/
GF_ISOTrackID gf_isom_get_track_original_id(GF_ISOFile *movie, u32 trackNumber);

/*gets the enable flag of a track 0: NO, 1: yes, 2: error*/
u8 gf_isom_is_track_enabled(GF_ISOFile *the_file, u32 trackNumber);

/*gets track flags*/
u32 gf_isom_get_track_flags(GF_ISOFile *the_file, u32 trackNumber);

/* determines if the track is encrypted 0: NO, 1: yes, 2: error*/
Bool gf_isom_is_track_encrypted(GF_ISOFile *the_file, u32 trackNumber);

/*get the track duration return 0 if bad param*/
u64 gf_isom_get_track_duration(GF_ISOFile *the_file, u32 trackNumber);

/*return the media type FOUR CHAR code type of the media*/
u32 gf_isom_get_media_type(GF_ISOFile *the_file, u32 trackNumber);

/*return the media type FOUR CHAR code type of the media*/
u32 gf_isom_get_media_subtype(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

/*return the media type FOUR CHAR code type of an MPEG4 media (eg, mp4a, mp4v, enca, encv, resv, etc...)
returns 0 if not MPEG-4 subtype*/
u32 gf_isom_get_mpeg4_subtype(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

/*Get the media (composition) time given the absolute time in the Movie
mediaTime is set to 0 if the media is not playing at that time (empty time segment)*/
GF_Err gf_isom_get_media_time(GF_ISOFile *the_file, u32 trackNumber, u32 movieTime, u64 *MediaTime);

/*Get the number of "streams" stored in the media - a media can have several stream descriptions...*/
u32 gf_isom_get_sample_description_count(GF_ISOFile *the_file, u32 trackNumber);

/*Get the stream description index (eg, the ESD) for a given time IN MEDIA TIMESCALE
return 0 if error or if empty*/
u32 gf_isom_get_sample_description_index(GF_ISOFile *the_file, u32 trackNumber, u64 for_time);

/*returns 1 if samples refering to the given stream description are present in the file
0 otherwise*/
Bool gf_isom_is_self_contained(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*get the media duration (without edit) based on sample table return 0 if no samples (URL streams)*/
u64 gf_isom_get_media_duration(GF_ISOFile *the_file, u32 trackNumber);

/*get the media duration (without edit) as indicated in the file (regardless of sample tables)*/
u64 gf_isom_get_media_original_duration(GF_ISOFile *movie, u32 trackNumber);

/*Get the timeScale of the media. */
u32 gf_isom_get_media_timescale(GF_ISOFile *the_file, u32 trackNumber);

/*gets min, average and max maximum chunk durations (each of them s optional) of the track in media timescale*/
GF_Err gf_isom_get_chunks_infos(GF_ISOFile *movie, u32 trackNumber, u32 *dur_min, u32 *dur_avg, u32 *dur_max, u32 *size_min, u32 *size_avg, u32 *size_max);

/*Get the HandlerDescription name. The outName must be:
		 (outName != NULL && *outName == NULL)
the handler name is the string version of the MediaTypes*/
GF_Err gf_isom_get_handler_name(GF_ISOFile *the_file, u32 trackNumber, const char **outName);

/*Check a DataReference of this track (index >= 1)
A Data Reference allows to construct a file without integrating the media data*/
GF_Err gf_isom_check_data_reference(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex);

/*get the location of the data. If URL && URN are NULL, the data is in this file
both strings are const: don't free them.*/
GF_Err gf_isom_get_data_reference(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, const char **outURL, const char **outURN);

/*Get the number of samples - return 0 if error*/
u32 gf_isom_get_sample_count(GF_ISOFile *the_file, u32 trackNumber);

/*Get constant sample size, or 0 if size not constant*/
u32 gf_isom_get_constant_sample_size(GF_ISOFile *the_file, u32 trackNumber);

/*Get constant sample duration, or 0 if duration not constant*/
u32 gf_isom_get_constant_sample_duration(GF_ISOFile *the_file, u32 trackNumber);

/*sets max audio sample packing in a single ISOSample*/
Bool gf_isom_enable_raw_pack(GF_ISOFile *the_file, u32 trackNumber, u32 pack_num_samples);

/*returns total amount of media bytes in track*/
u64 gf_isom_get_media_data_size(GF_ISOFile *the_file, u32 trackNumber);

/*It may be desired to fetch samples with a bigger allocated buffer than their real size, in case the decoder
reads more data than available. This sets the amount of extra bytes to allocate when reading samples from this track
NOTE: the dataLength of the sample does NOT include padding*/
GF_Err gf_isom_set_sample_padding(GF_ISOFile *the_file, u32 trackNumber, u32 padding_bytes);

/*return a sample given its number, and set the StreamDescIndex of this sample
this index allows to retrieve the stream description if needed (2 media in 1 track)
return NULL if error*/
GF_ISOSample *gf_isom_get_sample(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *StreamDescriptionIndex);

//same as gf_isom_get_sample but fills in , potentially reallocating buffers, the static_sample passed as argument
GF_ISOSample *gf_isom_get_sample_ex(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex, GF_ISOSample *static_sample);

/*same as gf_isom_get_sample but doesn't fetch media data
@StreamDescriptionIndex (optional): set to stream description index
@data_offset (optional): set to sample start offset in file.

	  NOTE: when both StreamDescriptionIndex and data_offset are NULL, only DTS, CTS_Offset and RAP indications are
retrieved (faster)
*/
GF_ISOSample *gf_isom_get_sample_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *StreamDescriptionIndex, u64 *data_offset);

//same as gf_isom_get_sample_info but uses a static allocated sample as input
GF_ISOSample *gf_isom_get_sample_info_ex(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex, u64 *data_offset, GF_ISOSample *static_sample);

/*retrieves given sample DTS*/
u64 gf_isom_get_sample_dts(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber);

/*returns sample duration in media timeScale*/
u32 gf_isom_get_sample_duration(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber);

/*returns sample size in bytes*/
u32 gf_isom_get_sample_size(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber);

/*returns max size of any sample in track*/
u32 gf_isom_get_max_sample_size(GF_ISOFile *the_file, u32 trackNumber);

/*returns average size of sample in track*/
u32 gf_isom_get_avg_sample_size(GF_ISOFile *the_file, u32 trackNumber);
/*returns max sample delta in track*/
u32 gf_isom_get_max_sample_delta(GF_ISOFile *the_file, u32 trackNumber);
/*returns max sample cts offset in track*/
u32 gf_isom_get_max_sample_cts_offset(GF_ISOFile *the_file, u32 trackNumber);

/*returns constant sample duration or 0 if not constant*/
u32 gf_isom_get_sample_const_duration(GF_ISOFile *the_file, u32 trackNumber);

/*returns sync flag of sample*/
u8 gf_isom_get_sample_sync(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber);

GF_Err gf_isom_get_sample_flags(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *is_leading, u32 *dependsOn, u32 *dependedOn, u32 *redundant);

/*gets a sample given a desired decoding time IN MEDIA TIME SCALE
and set the StreamDescIndex of this sample
this index allows to retrieve the stream description if needed (2 media in 1 track)
return GF_EOS if the desired time exceeds the media duration
WARNING: the sample may not be sync even though the sync was requested (depends on the media and the editList)
the SampleNum is optional. If non-NULL, will contain the sampleNumber*/
GF_Err gf_isom_get_sample_for_media_time(GF_ISOFile *the_file, u32 trackNumber, u64 desiredTime, u32 *StreamDescriptionIndex, u8 SearchMode, GF_ISOSample **sample, u32 *SampleNum, u64 *data_offset);

/*retrieves given sample DTS*/
u32 gf_isom_get_sample_from_dts(GF_ISOFile *the_file, u32 trackNumber, u64 dts);

/*get the current tfdt of the track - this can be used to adjust sample time queries when edit list are used*/
u64 gf_isom_get_current_tfdt(GF_ISOFile *the_file, u32 trackNumber);

//returns true if the file init segment (moov) was generated from external meta-data (smooth streaming)
Bool gf_isom_is_smooth_streaming_moov(GF_ISOFile *the_file);

/*Track Edition functions*/

/*return a sample given a desired time in the movie. MovieTime is IN MEDIA TIME SCALE , handles edit list.
and set the StreamDescIndex of this sample
this index allows to retrieve the stream description if needed (2 media in 1 track)
sample must be set to NULL before calling.

result Sample is NULL if an error occured
if no sample is playing, an empty sample is returned with no data and a DTS set to MovieTime when serching in sync modes
if no sample is playing, the closest sample in the edit time-line is returned when serching in regular modes

WARNING: the sample may not be sync even though the sync was requested (depends on the media and the editList)

Note: this function will handle re-timestamping the sample according to the mapping  of the media time-line
on the track time-line. The sample TSs (DTS / CTS offset) are expressed in MEDIA TIME SCALE
(to match the media stream TS resolution as indicated in media header / SLConfig)

sampleNumber is optional and gives the number of the sample in the media
*/
GF_Err gf_isom_get_sample_for_movie_time(GF_ISOFile *the_file, u32 trackNumber, u64 movieTime, u32 *StreamDescriptionIndex, u8 SearchMode, GF_ISOSample **sample, u32 *sampleNumber, u64 *data_offset);

/*return 1 if true edit list, 0 if no edit list or if time-shifting only edit list, in which case mediaOffset is set to the CTS of the first sample to present at presentation time 0
A negative value implies that the samples with CTS between 0 and mediaOffset should not be presented (skip)
A positive value value implies that there is nothing to present between 0 and CTS (hold)*/
Bool gf_isom_get_edit_list_type(GF_ISOFile *the_file, u32 trackNumber, s64 *mediaOffset);

/*get the number of edited segment*/
u32 gf_isom_get_edit_segment_count(GF_ISOFile *the_file, u32 trackNumber);

/*get the desired segment information*/
GF_Err gf_isom_get_edit_segment(GF_ISOFile *the_file, u32 trackNumber, u32 SegmentIndex, u64 *EditTime, u64 *SegmentDuration, u64 *MediaTime, u8 *EditMode);

/*get the number of languages for the copyright*/
u32 gf_isom_get_copyright_count(GF_ISOFile *the_file);
/*get the copyright and its language code given the index*/
GF_Err gf_isom_get_copyright(GF_ISOFile *the_file, u32 Index, const char **threeCharCodes, const char **notice);
/*get the opaque watermark info if any - returns GF_NOT_SUPPORTED if not present*/
GF_Err gf_isom_get_watermark(GF_ISOFile *the_file, bin128 UUID, u8** data, u32* length);

/*get the number of chapter for movie or track if trackNumber !=0*/
u32 gf_isom_get_chapter_count(GF_ISOFile *the_file, u32 trackNumber);
/*get the given movie or track (trackNumber!=0) chapter time and name - index is 1-based
@chapter_time: retrieves start time in milliseconds - may be NULL.
@name: retrieves chapter name - may be NULL - SHALL NOT be destroyed by user
*/
GF_Err gf_isom_get_chapter(GF_ISOFile *the_file, u32 trackNumber, u32 Index, u64 *chapter_time, const char **name);

/*
return 0 if the media has no sync point info (eg, all samples are RAPs)
return 1 if the media has sync points (eg some samples are RAPs)
return 2 if the media has empty sync point info (eg no samples are RAPs). This will likely only happen
			in scalable context
*/
u8 gf_isom_has_sync_points(GF_ISOFile *the_file, u32 trackNumber);

/*returns number of sync points*/
u32 gf_isom_get_sync_point_count(GF_ISOFile *the_file, u32 trackNumber);

/*
returns 1 if the track uses unsigned compositionTime offsets (B-frames or similar)
returns 2 if the track uses signed compositionTime offsets (B-frames or similar)
returns 0 if the track does not use compositionTime offsets (CTS == DTS)
*/
u32 gf_isom_has_time_offset(GF_ISOFile *the_file, u32 trackNumber);

/*returns the shift from composition time to decode time for that track if indicated, or 0 if not found
adding shift to CTS guarantees that the shifted CTS is always greater than the DTS for any sample*/
s64 gf_isom_get_cts_to_dts_shift(GF_ISOFile *the_file, u32 trackNumber);

/*returns 1 if the track has sync shadow samples*/
Bool gf_isom_has_sync_shadows(GF_ISOFile *the_file, u32 trackNumber);

/*returns 1 if the track has sample dep indications*/
Bool gf_isom_has_sample_dependency(GF_ISOFile *the_file, u32 trackNumber);

/*rough estimation of file size, only works for completely self-contained files and without fragmentation
for the current time*/
u64 gf_isom_estimate_size(GF_ISOFile *the_file);

u32 gf_isom_get_next_alternate_group_id(GF_ISOFile *movie);


/*
		MPEG-4 Systems extensions
*/

/*check if files has root OD/IOD or not*/
Bool gf_isom_has_root_od(GF_ISOFile *the_file);

/*return the root Object descriptor of the movie (can be NULL, OD or IOD, you have to check its tag)
YOU HAVE TO DELETE THE DESCRIPTOR
*/
GF_Descriptor *gf_isom_get_root_od(GF_ISOFile *the_file);

/*check the presence of a track in IOD. 0: NO, 1: YES, 2: ERROR*/
u8 gf_isom_is_track_in_root_od(GF_ISOFile *the_file, u32 trackNumber);

/*Get the GF_ESD given the StreamDescriptionIndex - YOU HAVE TO DELETE THE DESCRIPTOR*/
GF_ESD *gf_isom_get_esd(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex);

/*Get the decoderConfigDescriptor given the StreamDescriptionIndex - YOU HAVE TO DELETE THE DESCRIPTOR*/
GF_DecoderConfig *gf_isom_get_decoder_config(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex);

/*sets default TrackID (or ES_ID) for clock references. If trackNumber is 0, default sync track ID is reseted
and will be reassigned at next ESD fetch*/
void gf_isom_set_default_sync_track(GF_ISOFile *file, u32 trackNumber);

/*Return the number of track references of a track for a given ReferenceType - return -1 if error*/
s32 gf_isom_get_reference_count(GF_ISOFile *the_file, u32 trackNumber, u32 referenceType);

/*Return the referenced track number for a track and a given ReferenceType and Index
return -1 if error, 0 if the reference is a NULL one, or the trackNumber
*/
GF_Err gf_isom_get_reference(GF_ISOFile *the_file, u32 trackNumber, u32 referenceType, u32 referenceIndex, u32 *refTrack);

/*Return the referenced track ID for a track and a given ReferenceType and Index
return -1 if error, 0 if the reference is a NULL one, or the trackNumber
*/
GF_Err gf_isom_get_reference_ID(GF_ISOFile *the_file, u32 trackNumber, u32 referenceType, u32 referenceIndex, GF_ISOTrackID *refTrackID);

/*Return referenceIndex if the given track has a reference to the given TreckID of a given ReferenceType, 0 otherwise*/
u32 gf_isom_has_track_reference(GF_ISOFile *movie, u32 trackNumber, u32 referenceType, GF_ISOTrackID refTrackID);

u8 gf_isom_get_pl_indication(GF_ISOFile *the_file, u8 PL_Code);

/*locates the first ObjectDescriptor using the given track by inspecting any OD tracks*/
u32 gf_isom_find_od_for_track(GF_ISOFile *file, u32 track);

/*returns file name*/
const char *gf_isom_get_filename(GF_ISOFile *the_file);

/*
		Update of the Reading API for IsoMedia Version 2
*/

/*retrieves the brand of the file. The brand is introduced in V2 to differenciate
MP4, MJPEG2000 and QT while indicating compatibilities
the brand is one of the above defined code, or any other registered brand

minorVersion is an optional parameter (can be set to NULL) ,
		"informative integer for the minor version of the major brand"
AlternateBrandsCount is an optional parameter (can be set to NULL) ,
	giving the number of compatible brands.

	The function will set brand to 0 if no brand indication is found in the file
*/
GF_Err gf_isom_get_brand_info(GF_ISOFile *the_file, u32 *brand, u32 *minorVersion, u32 *AlternateBrandsCount);

/*gets an alternate brand indication. BrandIndex is 1-based
Note that the Major brand should always be indicated in the alternate brands*/
GF_Err gf_isom_get_alternate_brand(GF_ISOFile *the_file, u32 BrandIndex, u32 *brand);

/*gets the internal list of brands. DO NOT MODIFY the content*/
const u32 *gf_isom_get_brands(GF_ISOFile *movie);

/*get the number of padding bits at the end of a given sample if any*/
GF_Err gf_isom_get_sample_padding_bits(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u8 *NbBits);
/*indicates whether the track samples use padding bits or not*/
Bool gf_isom_has_padding_bits(GF_ISOFile *the_file, u32 trackNumber);

/*returns width and height of the given visual sample desc - error if not a visual track*/
GF_Err gf_isom_get_visual_info(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u32 *Width, u32 *Height);

/*returns samplerate (no SBR when applicable), channels and bps of the given audio track - error if not a audio track*/
GF_Err gf_isom_get_audio_info(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u32 *SampleRate, u32 *Channels, u32 *bitsPerSample);

/*returns track visual info - all coord values are expressed as 16.16 fixed point floats*/
GF_Err gf_isom_get_track_layout_info(GF_ISOFile *the_file, u32 trackNumber, u32 *width, u32 *height, s32 *translation_x, s32 *translation_y, s16 *layer);

/*returns track matrix info - all coord values are expressed as 16.16 fixed point floats*/
GF_Err gf_isom_get_track_matrix(GF_ISOFile *the_file, u32 trackNumber, u32 matrix[9]);

/*returns aspect ratio for the given visual sample desc - error if not a visual track*/
GF_Err gf_isom_get_pixel_aspect_ratio(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u32 *hSpacing, u32 *vSpacing);

/*returns color info for the given visual sample desc - error if not a visual track*/
GF_Err gf_isom_get_color_info(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 *colour_type, u16 *colour_primaries, u16 *transfer_characteristics, u16 *matrix_coefficients, Bool *full_range_flag);

/*gets RVC config of given sample description*/
GF_Err gf_isom_get_rvc_config(GF_ISOFile *movie, u32 track, u32 sampleDescriptionIndex, u16 *rvc_predefined, u8 **data, u32 *size, const char **mime);

/*
	User Data Manipulation (cf write API too)
*/

//returns the number of entries in UDTA of the track if trackNumber is not 0, or of the movie otherwise
u32 gf_isom_get_udta_count(GF_ISOFile *movie, u32 trackNumber);

//returns the type (box 4CC and UUID if any) of the given entry in UDTA of the track if trackNumber is not 0, or of the movie otherwise. udta_idx is 1-based index.
GF_Err gf_isom_get_udta_type(GF_ISOFile *movie, u32 trackNumber, u32 udta_idx, u32 *UserDataType, bin128 *UUID);

/* Gets the number of UserDataItems with the same ID / UUID in the desired track or
in the movie if trackNumber is set to 0*/
u32 gf_isom_get_user_data_count(GF_ISOFile *the_file, u32 trackNumber, u32 UserDataType, bin128 UUID);
/* Gets the UserData for the specified item from the track or the movie if trackNumber is set to 0
data is allocated by the function and is yours to free
you musty pass (userData != NULL && *userData=NULL)

if UserDataIndex is 0, all boxes with type==UserDataType will be serialized (including box header and size) in the buffer
*/
GF_Err gf_isom_get_user_data(GF_ISOFile *the_file, u32 trackNumber, u32 UserDataType, bin128 UUID, u32 UserDataIndex, u8 **userData, u32 *userDataSize);


/*gets the media language code (3 chars if old files, longer if BCP-47 */
GF_Err gf_isom_get_media_language(GF_ISOFile *the_file, u32 trackNumber, char **lang);

/* gets the i-th track kind (0-based) */
u32 gf_isom_get_track_kind_count(GF_ISOFile *the_file, u32 trackNumber);
GF_Err gf_isom_get_track_kind(GF_ISOFile *the_file, u32 trackNumber, u32 index, char **scheme, char **value);

/*Unknown sample description*/
typedef struct
{
	/*codec tag is the containing box's tag, 0 if UUID is used*/
	u32 codec_tag;
	/*entry UUID if no tag is used*/
	bin128 UUID;

	u16 version;
	u16 revision;
	u32 vendor_code;

	/*video codecs only*/
	u32 temporal_quality;
	u32 spatial_quality;
	u16 width, height;
	u32 h_res, v_res;
	u16 depth;
	u16 color_table_index;
	char compressor_name[33];

	/*audio codecs only*/
	u32 samplerate;
	u16 nb_channels;
	u16 bits_per_sample;
	Bool is_qtff;

	/*if present*/
	u8 *extension_buf;
	u32 extension_buf_size;
} GF_GenericSampleDescription;

/*returns wrapper for unknown entries - you must delete it yourself*/
GF_GenericSampleDescription *gf_isom_get_generic_sample_description(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex);

/*retrieves default values for a track fragment. Each variable is optional and
if set will contain the default value for this track samples*/
GF_Err gf_isom_get_fragment_defaults(GF_ISOFile *the_file, u32 trackNumber,
                                     u32 *defaultDuration, u32 *defaultSize, u32 *defaultDescriptionIndex,
                                     u32 *defaultRandomAccess, u8 *defaultPadding, u16 *defaultDegradationPriority);


/*returns 1 if file is single AV (max one audio, one video, one text and basic od/bifs)*/
Bool gf_isom_is_single_av(GF_ISOFile *file);

/*returns TRUE if this is a video track type (vide, auxv, pict, ...)*/
Bool gf_isom_is_video_subtype(u32 mtype);

/*guess which std this file refers to. return value:
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
u32 gf_isom_guess_specification(GF_ISOFile *file);
/*keeps UTC edit times when storing*/
void gf_isom_keep_utc_times(GF_ISOFile *file, Bool keep_utc);

/*gets last UTC/timestamp values indicated for the reference track in the file if any. Returns 0 if no info found*/
Bool gf_isom_get_last_producer_time_box(GF_ISOFile *file, GF_ISOTrackID *refTrackID, u64 *ntp, u64 *timestamp, Bool reset_info);

/*gets the nalu_length_field size used for this sample description if NALU-based (AVC/HEVC/...), else returns 0 */
u32 gf_isom_get_nalu_length_field(GF_ISOFile *file, u32 track, u32 StreamDescriptionIndex);

/*gets max/average rate info as indicated in ESDS or BTRT boxes. If not found all values are set to 0
if sampleDescIndex is 0, gather for all sample descriptions*/
GF_Err gf_isom_get_bitrate(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescIndex, u32 *average_bitrate, u32 *max_bitrate, u32 *decode_buffer_size);

/*returns true if this sample was the first sample of a traf in a fragmented file, false otherwise*/
Bool gf_isom_sample_was_traf_start(GF_ISOFile *movie, u32 trackNumber, u32 sampleNum);

GF_Err gf_isom_get_jp2_config(GF_ISOFile *movie, u32 trackNumber, u32 sampleDesc, u8 **out_dsi, u32 *out_size);

#ifndef GPAC_DISABLE_ISOM_WRITE


/********************************************************************
				EDITING/WRITING API FUNCTIONS
********************************************************************/

/*set the timescale of the movie*/
GF_Err gf_isom_set_timescale(GF_ISOFile *the_file, u32 timeScale);

//loads the set of top-level boxes in moov udta and child boxes. UDTA will be replaced if already present
GF_Err gf_isom_load_extra_boxes(GF_ISOFile *movie, u8 *moov_boxes, u32 moov_boxes_size, Bool udta_only);

/*creates a new Track. If trackID = 0, the trackID is chosen by the API
returns the track number or 0 if error*/
u32 gf_isom_new_track(GF_ISOFile *the_file, GF_ISOTrackID trackID, u32 MediaType, u32 TimeScale);

/*creates a new Track from an encoded trak box. If udta_only, only keeps the udta box from the template. If trackID = 0, the trackID is chosen by the API
returns the track number or 0 if error*/
u32 gf_isom_new_track_from_template(GF_ISOFile *movie, GF_ISOTrackID trakID, u32 MediaType, u32 TimeScale, u8 *tk_box, u32 tk_box_size, Bool udta_only);

/*removes the desired track - internal cross dependencies will be updated.
WARNING: any OD streams with references to this track through  ODUpdate, ESDUpdate, ESDRemove commands
will be rewritten*/
GF_Err gf_isom_remove_track(GF_ISOFile *the_file, u32 trackNumber);

/*sets the enable flag of a track*/
GF_Err gf_isom_set_track_enabled(GF_ISOFile *the_file, u32 trackNumber, u8 enableTrack);

typedef enum
{
	GF_ISOM_TKFLAGS_SET = 0,
	GF_ISOM_TKFLAGS_REM,
	GF_ISOM_TKFLAGS_ADD,
} GF_ISOMTrackFlagOp;

GF_Err gf_isom_set_track_flags(GF_ISOFile *movie, u32 trackNumber, u32 flags, GF_ISOMTrackFlagOp op);

/*sets creationTime and modificationTime of the movie to the specified date*/
GF_Err gf_isom_set_creation_time(GF_ISOFile *movie, u64 time);
/*sets creationTime and modificationTime of the track to the specified date*/
GF_Err gf_isom_set_track_creation_time(GF_ISOFile *movie,u32 trackNumber, u64 time);

/*changes the trackID - all track references present in the file are updated
returns error if trackID is already in used in the file*/
GF_Err gf_isom_set_track_id(GF_ISOFile *the_file, u32 trackNumber, GF_ISOTrackID trackID);

/*force to rewrite all dependencies when trackID changes*/
GF_Err gf_isom_rewrite_track_dependencies(GF_ISOFile *movie, u32 trackNumber);

/*Add samples to a track. Use streamDescriptionIndex to specify the desired stream (if several)*/
GF_Err gf_isom_add_sample(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, const GF_ISOSample *sample);

//copies all sample dependency, subSample and sample group information from the given sampleNumber in source file to the last added sample in dest file
GF_Err gf_isom_copy_sample_info(GF_ISOFile *dst, u32 dst_track, GF_ISOFile *src, u32 src_track, u32 sampleNumber);

/*Add sync shadow sample to a track.
- There must be a regular sample with the same DTS.
- Sync Shadow samples MUST be RAP
- Currently, adding sync shadow must be done in order (no sample insertion)
*/
GF_Err gf_isom_add_sample_shadow(GF_ISOFile *the_file, u32 trackNumber, GF_ISOSample *sample);

/*add data to current sample in the track. Use this function for media with
fragmented options such as MPEG-4 video packets. This will update the data size.
CANNOT be used with OD media type*/
GF_Err gf_isom_append_sample_data(GF_ISOFile *the_file, u32 trackNumber, u8 *data, u32 data_size);

/*Add sample references to a track. The dataOffset is the offset of the data in the referenced file
you MUST have created a StreamDescription with URL or URN specifying your referenced file
Use streamDescriptionIndex to specify the desired stream (if several)*/
GF_Err gf_isom_add_sample_reference(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, GF_ISOSample *sample, u64 dataOffset);

/*set the duration of the last media sample. If not set, the duration of the last sample is the
duration of the previous one if any, or media TimeScale (default value). This does not modify the edit list if any,
you must modify this using gf_isom_set_edit_segment*/
GF_Err gf_isom_set_last_sample_duration(GF_ISOFile *the_file, u32 trackNumber, u32 duration);

/*patches last stts entry to make sure the cumulated duration equals the given next_dts value*/
GF_Err gf_isom_patch_last_sample_duration(GF_ISOFile *movie, u32 trackNumber, u64 next_dts);

/*sets a track reference*/
GF_Err gf_isom_set_track_reference(GF_ISOFile *the_file, u32 trackNumber, u32 referenceType, GF_ISOTrackID ReferencedTrackID);

/*removes all track references*/
GF_Err gf_isom_remove_track_references(GF_ISOFile *the_file, u32 trackNumber);

/*sets track handler name. name is either NULL (reset), a UTF-8 formatted string or a UTF8 file
resource in the form "file://path/to/file_utf8" */
GF_Err gf_isom_set_handler_name(GF_ISOFile *the_file, u32 trackNumber, const char *nameUTF8);

/*Update the sample size table - this is needed when using @gf_isom_append_sample_data in case the resulting samples
are of same sizes (typically in 3GP speech tracks)*/
GF_Err gf_isom_refresh_size_info(GF_ISOFile *file, u32 trackNumber);

/*return the duration of the movie, 0 if error*/
GF_Err gf_isom_update_duration(GF_ISOFile *the_file);

/*Update Sample functions*/

/*update a given sample of the media.
@data_only: if set, only the sample data is updated, not other info*/
GF_Err gf_isom_update_sample(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, GF_ISOSample *sample, Bool data_only);

/*update a sample reference in the media. Note that the sample MUST exists,
that sample->data MUST be NULL and sample->dataLength must be NON NULL;*/
GF_Err gf_isom_update_sample_reference(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, GF_ISOSample *sample, u64 data_offset);

/*Remove a given sample*/
GF_Err gf_isom_remove_sample(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber);

/*changes media time scale - if force_rescale is 1, only the media timescale is changed but media times are not updated */
GF_Err gf_isom_set_media_timescale(GF_ISOFile *the_file, u32 trackNumber, u32 new_timescale, Bool force_rescale);

/*set the save file name of the (edited) movie.
If the movie is edited, the default fileName is avp_#openName)
NOTE: you cannot save an edited file under the same name (overwrite not allowed)
If the movie is created (WRITE mode), the default filename is #openName*/
GF_Err gf_isom_set_final_name(GF_ISOFile *the_file, char *filename);


/*set the storage mode of a file (FLAT, STREAMABLE, INTERLEAVED)*/
GF_Err gf_isom_set_storage_mode(GF_ISOFile *the_file, u8 storageMode);

/*set the interleaving time of media data (INTERLEAVED mode only)
InterleaveTime is in MovieTimeScale*/
GF_Err gf_isom_set_interleave_time(GF_ISOFile *the_file, u32 InterleaveTime);

/*forces usage of 64 bit chunk offsets*/
void gf_isom_force_64bit_chunk_offset(GF_ISOFile *the_file, Bool set_on);

/*set the copyright in one language.*/
GF_Err gf_isom_set_copyright(GF_ISOFile *the_file, const char *threeCharCode, char *notice);

/*add a kind type to the track */
GF_Err gf_isom_add_track_kind(GF_ISOFile *movie, u32 trackNumber, const char *schemeURI, const char *value);
/*removes a kind type to the track, all if NULL params */
GF_Err gf_isom_remove_track_kind(GF_ISOFile *movie, u32 trackNumber, const char *schemeURI, const char *value);

/*changes the handler type of the media*/
GF_Err gf_isom_set_media_type(GF_ISOFile *movie, u32 trackNumber, u32 new_type);

/*changes the type of the sampleDescriptionBox - USE AT YOUR OWN RISK, the file may not be understood afterwards*/
GF_Err gf_isom_set_media_subtype(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescriptionIndex, u32 new_type);

GF_Err gf_isom_set_alternate_group_id(GF_ISOFile *movie, u32 trackNumber, u32 groupId);

/*add chapter info:
if trackNumber is 0, the chapter info is added to the movie, otherwise to the track
@timestamp: chapter start time in milliseconds. Chapters are added in order to the file. If a chapter with same timestamp
	is found, its name is updated but no entry is created.
@name: chapter name. If NULL, defaults to 'Chapter N'
*/
GF_Err gf_isom_add_chapter(GF_ISOFile *the_file, u32 trackNumber, u64 timestamp, char *name);

/*deletes copyright (1-based index, index 0 for all)*/
GF_Err gf_isom_remove_chapter(GF_ISOFile *the_file, u32 trackNumber, u32 index);

/*Track Edition functions - used to change the normal playback of the media if desired
NOTE: IT IS THE USER RESPONSABILITY TO CREATE A CONSISTENT TIMELINE FOR THE TRACK
This API provides the basic hooks and some basic consistency checking
but can not check the desired functionality of the track edits
*/

/*update or insert a new edit segment in the track time line. Edits are used to modify
the media normal timing. EditTime and EditDuration are expressed in Movie TimeScale
If a segment with EditTime already exists, IT IS ERASED
if there is a segment before this new one, its duration is adjust to match EditTime of
the new segment
WARNING: The first segment always have an EditTime of 0. You should insert an empty or dwelled segment first.*/
GF_Err gf_isom_set_edit_segment(GF_ISOFile *the_file, u32 trackNumber, u64 EditTime, u64 EditDuration, u64 MediaTime, u8 EditMode);

/*same as above except only modifies duration type and mediaType*/
GF_Err gf_isom_modify_edit_segment(GF_ISOFile *the_file, u32 trackNumber, u32 seg_index, u64 EditDuration, u64 MediaTime, u8 EditMode);
/*same as above except only appends new segment*/
GF_Err gf_isom_append_edit_segment(GF_ISOFile *the_file, u32 trackNumber, u64 EditDuration, u64 MediaTime, u8 EditMode);

/*remove the edit segments for the whole track*/
GF_Err gf_isom_remove_edit_segments(GF_ISOFile *the_file, u32 trackNumber);

/*remove the given edit segment (1-based index). If this is not the last segment, the next segment duration
is updated to maintain a continous timeline*/
GF_Err gf_isom_remove_edit_segment(GF_ISOFile *the_file, u32 trackNumber, u32 seg_index);

/*Updates edit list after track edition: all edit entries with aduration or media starttime larger than the media duration are clamped to media duration*/
GF_Err gf_isom_update_edit_list_duration(GF_ISOFile *file, u32 track);

/*
				User Data Manipulation

		You can add specific typed data to either a track or the movie: the UserData
	The type must be formated as a FourCC if you have a registered 4CC type
	but the usual is to set a UUID (128 bit ID for box type) which never conflict
	with existing structures in the format
		To manipulate a UUID user data set the UserDataType to 0 and specify a valid UUID.
Otherwise the UUID parameter is ignored
		Several items with the same ID or UUID can be added (this allows you to store any
	kind/number of private information under a unique ID / UUID)
*/
/*Add a user data item in the desired track or in the movie if TrackNumber is 0*/
GF_Err gf_isom_add_user_data(GF_ISOFile *the_file, u32 trackNumber, u32 UserDataType, bin128 UUID, u8 *data, u32 DataLength);

/*remove all user data items from the desired track or from the movie if TrackNumber is 0*/
GF_Err gf_isom_remove_user_data(GF_ISOFile *the_file, u32 trackNumber, u32 UserDataType, bin128 UUID);

/*remove a user data item from the desired track or from the movie if TrackNumber is 0
use the UDAT read functions to get the item index*/
GF_Err gf_isom_remove_user_data_item(GF_ISOFile *the_file, u32 trackNumber, u32 UserDataType, bin128 UUID, u32 UserDataIndex);

/*remove track, moov (trackNumber=0) or file-level (trackNumber=0xFFFFFFFF) UUID box of matching type*/
GF_Err gf_isom_remove_uuid(GF_ISOFile *movie, u32 trackNumber, bin128 UUID);
/*adds track, moov (trackNumber=0) or file-level (trackNumber=0xFFFFFFFF) UUID box of given type*/
GF_Err gf_isom_add_uuid(GF_ISOFile *movie, u32 trackNumber, bin128 UUID, const u8 *data, u32 data_size);

/*Add a user data item in the desired track or in the movie if TrackNumber is 0, using a serialzed buffer of ISOBMFF boxes*/
GF_Err gf_isom_add_user_data_boxes(GF_ISOFile *the_file, u32 trackNumber, u8 *data, u32 DataLength);

/*
		Update of the Writing API for IsoMedia Version 2
*/

/*use a compact track version for sample size. This is not usually recommended
except for speech codecs where the track has a lot of small samples
compaction is done automatically while writing based on the track's sample sizes*/
GF_Err gf_isom_use_compact_size(GF_ISOFile *the_file, u32 trackNumber, u8 CompactionOn);

/*sets the brand of the movie*/
GF_Err gf_isom_set_brand_info(GF_ISOFile *the_file, u32 MajorBrand, u32 MinorVersion);

/*adds or remove an alternate brand for the movie*/
GF_Err gf_isom_modify_alternate_brand(GF_ISOFile *the_file, u32 Brand, u8 AddIt);

/*removes all alternate brands except major brand*/
GF_Err gf_isom_reset_alt_brands(GF_ISOFile *movie);

GF_Err gf_isom_set_sample_flags(GF_ISOFile *file, u32 track, u32 sampleNumber, u32 isLeading, u32 dependsOn, u32 dependedOn, u32 redundant);

/*since v2 you must specify w/h of video tracks for authoring tools (no decode the video cfg / first sample)*/
GF_Err gf_isom_set_visual_info(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u32 Width, u32 Height);
/*since v2 you must specify w/h of video tracks for authoring tools (no decode the video cfg / first sample)*/
GF_Err gf_isom_set_visual_bit_depth(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u16 bitDepth);

/*mainly used for 3GPP text since most ISO-based formats ignore these (except MJ2K)
all coord values are expressed as 16.16 fixed point floats*/
GF_Err gf_isom_set_track_layout_info(GF_ISOFile *the_file, u32 trackNumber, u32 width, u32 height, s32 translation_x, s32 translation_y, s16 layer);

/*sets track matrix - all coordinates are expressed as 16.16 floating points*/
GF_Err gf_isom_set_track_matrix(GF_ISOFile *the_file, u32 trackNumber, s32 matrix[9]);

GF_Err gf_isom_set_pixel_aspect_ratio(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u32 hSpacing, u32 vSpacing, Bool force_par);

GF_Err gf_isom_set_clean_aperture(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 cleanApertureWidthN, u32 cleanApertureWidthD, u32 cleanApertureHeightN, u32 cleanApertureHeightD, u32 horizOffN, u32 horizOffD, u32 vertOffN, u32 vertOffD);

typedef struct ___MasteringDisplayColourVolume GF_MasteringDisplayColourVolumeInfo;
typedef struct __ContentLightLevel GF_ContentLightLevelInfo;
GF_Err gf_isom_set_high_dynamic_range_info(GF_ISOFile* movie, u32 trackNumber, u32 StreamDescriptionIndex, GF_MasteringDisplayColourVolumeInfo *mdcv, GF_ContentLightLevelInfo *clli);

GF_Err gf_isom_set_image_sequence_coding_constraints(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, Bool remove, Bool all_ref_pics_intra, Bool intra_pred_used, u32 max_ref_per_pic);
GF_Err gf_isom_set_image_sequence_alpha(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, Bool remove);

/*passing colour_type=0 will remove all color info*/
GF_Err gf_isom_set_visual_color_info(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 colour_type, u16 colour_primaries, u16 transfer_characteristics, u16 matrix_coefficients, Bool full_range_flag, u8 *icc_data, u32 icc_size);

/*set SR & nbChans for audio description*/
typedef enum {
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_NOT_SET = 0,
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_BS,
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_2,
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_MPEG,
	GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF
} GF_AudioSampleEntryImportMode;

GF_Err gf_isom_set_audio_info(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u32 sampleRate, u32 nbChannels, u8 bitsPerSample, GF_AudioSampleEntryImportMode asemode);

/*set CTS unpack mode (used for B-frames & like): in unpack mode, each sample uses one entry in CTTS tables
unpack=0: set unpack on - !!creates a CTTS table if none found!!
unpack=1: set unpack off and repacks all table info
*/
GF_Err gf_isom_set_cts_packing(GF_ISOFile *the_file, u32 trackNumber, Bool unpack);

/*shift all CTS with the given offset - MUST be called in unpack mode only*/
GF_Err gf_isom_shift_cts_offset(GF_ISOFile *the_file, u32 trackNumber, s32 offset_shift);

/*set 3-char or BCP-47 code media language*/
GF_Err gf_isom_set_media_language(GF_ISOFile *the_file, u32 trackNumber, char *code);

/*sets the RVC config for the given track sample description*/
GF_Err gf_isom_set_rvc_config(GF_ISOFile *movie, u32 track, u32 sampleDescriptionIndex, u16 rvc_predefined, char *mime, u8 *data, u32 size);

GF_ISOTrackID gf_isom_get_last_created_track_id(GF_ISOFile *movie);

GF_Err gf_isom_apply_box_patch(GF_ISOFile *file, GF_ISOTrackID trackID, char *box_patch_filename);


/*
			MPEG-4 Extensions
*/

/*set a profile and level indication for the movie iod (created if needed)
if the flag is ProfileLevel is 0 this means the movie doesn't require
the specific codec (equivalent to 0xFF value in MPEG profiles)*/
GF_Err gf_isom_set_pl_indication(GF_ISOFile *the_file, u8 PL_Code, u8 ProfileLevel);

/*set the rootOD ID of the movie if you need it. By default, movies are created without root ODs*/
GF_Err gf_isom_set_root_od_id(GF_ISOFile *the_file, u32 OD_ID);

/*set the rootOD URL of the movie if you need it (only needed to create empty file pointing
to external ressource)*/
GF_Err gf_isom_set_root_od_url(GF_ISOFile *the_file, char *url_string);

/*remove the root OD*/
GF_Err gf_isom_remove_root_od(GF_ISOFile *the_file);

/*Add a system descriptor to the OD of the movie*/
GF_Err gf_isom_add_desc_to_root_od(GF_ISOFile *the_file, GF_Descriptor *theDesc);

/*add a track to the root OD*/
GF_Err gf_isom_add_track_to_root_od(GF_ISOFile *the_file, u32 trackNumber);

/*remove a track to the root OD*/
GF_Err gf_isom_remove_track_from_root_od(GF_ISOFile *the_file, u32 trackNumber);

/*removes the stream descritpion with the given index - this does not remove any added samples for that stream description !!*/
GF_Err gf_isom_remove_stream_description(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex);

/*Create a new StreamDescription (GF_ESD) in the file. The URL and URN are used to
describe external media, this will creat a data reference for the media*/
GF_Err gf_isom_new_mpeg4_description(GF_ISOFile *the_file, u32 trackNumber, GF_ESD *esd, char *URLname, char *URNname, u32 *outDescriptionIndex);

/*use carefully. Very useful when you made a lot of changes (IPMP, IPI, OCI, ...)
THIS WILL REPLACE THE WHOLE DESCRIPTOR ...*/
GF_Err gf_isom_change_mpeg4_description(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, GF_ESD *newESD);

/*Add a system descriptor to the ESD of a stream - you have to delete the descriptor*/
GF_Err gf_isom_add_desc_to_description(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, GF_Descriptor *theDesc);

/*updates average and max bitrate - if 0 for max and avg, removes bitrate info*/
GF_Err gf_isom_update_bitrate(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescriptionIndex, u32 average_bitrate, u32 max_bitrate, u32 decode_buffer_size);

/*updates fields of given visual sample description - these fields are reserved in ISOBMFF, this should only be used for QT*/
GF_Err gf_isom_update_video_sample_entry_fields(GF_ISOFile *file, u32 track, u32 stsd_idx, u16 revision, u32 vendor, u32 temporalQ, u32 spatialQ, u32 horiz_res, u32 vert_res, u16 frames_per_sample, char *compressor_name, s16 color_table_index);

GF_Err gf_isom_update_sample_description_from_template(GF_ISOFile *file, u32 track, u32 sampleDescriptionIndex, u8 *data, u32 size);

/*Default extensions*/

/*Create a new unknown StreamDescription in the file. The URL and URN are used to
describe external media, this will creat a data reference for the media
use this to store media not currently supported by the ISO media format
*/
GF_Err gf_isom_new_generic_sample_description(GF_ISOFile *the_file, u32 trackNumber, char *URLname, char *URNname, GF_GenericSampleDescription *udesc, u32 *outDescriptionIndex);

/*
special shortcut for stream description cloning from a given input file (this avoids inspecting for media type)
@the_file, @trackNumber: destination file and track
@orig_file, @orig_track, @orig_desc_index: orginal file, track and sample description
@URLname, @URNname, @outDescriptionIndex: same usage as with gf_isom_new_mpeg4_description
*/
GF_Err gf_isom_clone_sample_description(GF_ISOFile *the_file, u32 trackNumber, GF_ISOFile *orig_file, u32 orig_track, u32 orig_desc_index, char *URLname, char *URNname, u32 *outDescriptionIndex);


#define GF_ISOM_CLONE_TRACK_KEEP_DREF	1
#define GF_ISOM_CLONE_TRACK_NO_QT	1<<1

/*special shortcut: clones a track (everything except media data and sample info (DTS, CTS, RAPs, etc...)
also clones sampleDescriptions
@keep_data_ref: if set, all data references are kept (local ones become external pointing to orig_file name), otherwise all external data refs are removed (track media data will be self-contained)
@dest_track: track number of cloned track*/
GF_Err gf_isom_clone_track(GF_ISOFile *orig_file, u32 orig_track, GF_ISOFile *dest_file, u32 flags, u32 *dest_track);
/*special shortcut: clones IOD PLs from orig to dest if any*/
GF_Err gf_isom_clone_pl_indications(GF_ISOFile *orig, GF_ISOFile *dest);

GF_Err gf_isom_get_track_template(GF_ISOFile *file, u32 track, u8 **output, u32 *output_size);

GF_Err gf_isom_get_trex_template(GF_ISOFile *file, u32 track, u8 **output, u32 *output_size);

GF_Err gf_isom_get_stsd_template(GF_ISOFile *file, u32 track, u32 stsd_idx, u8 **output, u32 *output_size);

GF_Err gf_isom_get_raw_user_data(GF_ISOFile *file, u8 **output, u32 *output_size);


/*returns true if same set of sample description in both tracks - this does include self-contained checking
and reserved flags. The specific media cfg (DSI & co) is not analysed, only
a brutal memory comparaison is done*/
Bool gf_isom_is_same_sample_description(GF_ISOFile *f1, u32 tk1, u32 sdesc_index1, GF_ISOFile *f2, u32 tk2, u32 sdesc_index2);

/* sample information for all tracks setup are reset. This allows keeping the memory
footprint low when playing segments. Note however that seeking in the file is then no longer possible*/
GF_Err gf_isom_reset_tables(GF_ISOFile *movie, Bool reset_sample_count);
/* sets the offset for parsing from the input buffer to 0 (used to reclaim input buffer)*/
GF_Err gf_isom_reset_data_offset(GF_ISOFile *movie, u64 *top_box_start);

/*releases current movie segment - this closes the associated file IO object.
If reset_tables is set, sample information for all tracks setup as segment are destroyed, along with all PSSH boxes. This allows keeping the memory
footprint low when playing segments. Note however that seeking in the file is then no longer; possible
WARNING - the sample count is not reset after the release of tables. This means you need to keep counting samples.*/
GF_Err gf_isom_release_segment(GF_ISOFile *movie, Bool reset_tables);

/*Flags for gf_isom_open_segment*/
enum
{
	/*FLAT: the MediaData (MPEG4 ESs) is stored at the beginning of the file*/
	GF_ISOM_SEGMENT_NO_ORDER_FLAG = 1,
	GF_ISOM_SEGMENT_SCALABLE_FLAG = 1<<1,
};

/*opens a new segment file. Access to samples in previous segments is no longer possible
if end_range>start_range, restricts the URL to the given byterange when parsing*/
GF_Err gf_isom_open_segment(GF_ISOFile *movie, const char *fileName, u64 start_range, u64 end_range, u32 flags);

/*returns track ID of the traf containing the highest enhancement layer for the given base track*/
u32 gf_isom_get_highest_track_in_scalable_segment(GF_ISOFile *movie, u32 for_base_track);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

/*
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
*/

/*
setup a track for fragmentation by specifying some default values for
storage efficiency
*TrackID: track identifier
*DefaultStreamDescriptionIndex: the default description used by samples in this track
*DefaultSampleDuration: default duration of samples in this track
*DefaultSampleSize: default size of samples in this track (0 if unknown)
*DefaultSampleIsSync: default key-flag (RAP) of samples in this track
*DefaultSamplePadding: default padding bits for samples in this track
*DefaultDegradationPriority: default degradation priority for samples in this track
*force_traf_flags: if 1, will ignore these default in each traf but will still write them in moov

If all the defaults are 0, traf flags will alwaus be used to signal them.
*/
GF_Err gf_isom_setup_track_fragment(GF_ISOFile *the_file, GF_ISOTrackID TrackID,
                                    u32 DefaultStreamDescriptionIndex,
                                    u32 DefaultSampleDuration,
                                    u32 DefaultSampleSize,
                                    u8 DefaultSampleIsSync,
                                    u8 DefaultSamplePadding,
                                    u16 DefaultDegradationPriority,
									u8 force_traf_flags);

/*change the default parameters of an existing trak fragment - should not be used if samples have
already been added - semantics are the same as in gf_isom_setup_track_fragment*/
GF_Err gf_isom_change_track_fragment_defaults(GF_ISOFile *movie, GF_ISOTrackID TrackID,
        u32 DefaultSampleDescriptionIndex,
        u32 DefaultSampleDuration,
        u32 DefaultSampleSize,
        u8 DefaultSampleIsSync,
        u8 DefaultSamplePadding,
        u16 DefaultDegradationPriority,
        u8 force_traf_flags);

/*flushes data to disk and prepare movie fragmentation
@media_segment_type: 0 if no segments, 1 if regular segment, 2 if single segment*/
GF_Err gf_isom_finalize_for_fragment(GF_ISOFile *the_file, u32 media_segment_type, Bool mvex_after_tracks);

/*sets the duration of the movie in case of movie fragments*/
GF_Err gf_isom_set_movie_duration(GF_ISOFile *movie, u64 duration);

/*starts a new movie fragment - if force_cache is set, fragment metadata will be written before
fragment media data for all tracks*/
GF_Err gf_isom_start_fragment(GF_ISOFile *movie, Bool moof_first);

/*starts a new segment in the file. If SegName is given, the output will be written in the SegName file. If memory_mode is set, all samples writing is done in memory rather than on disk*/
GF_Err gf_isom_start_segment(GF_ISOFile *movie, const char *SegName, Bool memory_mode);

/*sets the baseMediaDecodeTime of the first sample of the given track*/
GF_Err gf_isom_set_traf_base_media_decode_time(GF_ISOFile *movie, GF_ISOTrackID TrackID, u64 decode_time);

/*enables mfra when writing movie fragments*/
GF_Err gf_isom_enable_mfra(GF_ISOFile *file);

/*sets Microsoft Smooth Streaming traf 'tfxd' box info, written at the end of each traf*/
GF_Err gf_isom_set_traf_mss_timeext(GF_ISOFile *movie, GF_ISOTrackID reference_track_ID, u64 ntp_in_10mhz, u64 traf_duration_in_10mhz);

/*closes current segment - if fragments_per_sidx is <0, no sidx is used - if fragments_per_sidx is ==0, a single sidx is used
timestamp_shift is the constant difference between media time and presentation time (derived from edit list)
out_seg_size is optional, holds the segment size in bytes
*/
GF_Err gf_isom_close_segment(GF_ISOFile *movie, s32 subsegs_per_sidx, GF_ISOTrackID referenceTrackID, u64 ref_track_decode_time, s32 timestamp_shift, u64 ref_track_next_cts, Bool daisy_chain_sidx, Bool use_ssix, Bool last_segment, Bool close_segment_handle, u32 segment_marker_4cc, u64 *index_start_range, u64 *index_end_range, u64 *out_seg_size);

/*writes any pending fragment to file for low-latency output. shall only be used if no SIDX is used (subsegs_per_sidx<0 or flushing all fragments before calling gf_isom_close_segment)*/
GF_Err gf_isom_flush_fragments(GF_ISOFile *movie, Bool last_segment);

//gets name of current segment (or last segment if called between close_segment and start_segment)
const char *gf_isom_get_segment_name(GF_ISOFile *movie);

/*sets fragment prft box info, written just before the moof*/
GF_Err gf_isom_set_fragment_reference_time(GF_ISOFile *movie, GF_ISOTrackID reference_track_ID, u64 ntp, u64 timestamp);

/*writes an empty sidx in the current movie. The SIDX will be forced to have nb_segs entries - nb_segs shall match the number of calls to
gf_isom_close_segment that will follow. This avoids wasting time and disk space moving data around. Once gf_isom_close_segment has then been called nb_segs times,
the pre-allocated SIDX is destroyed and sucessive calls to gf_isom_close_segment will create their own sidx (unless gf_isom_allocate_sidx is called again).
frags_per_sidx, daisy_chain_sidx and frags_per_segment are currently ignored and reserved for future usages where multiple SIDX could be written
if not NULL, start_range and end_range will contain the byte range of the SIDX box in the movie*/
GF_Err gf_isom_allocate_sidx(GF_ISOFile *movie, s32 subsegs_per_sidx, Bool daisy_chain_sidx, u32 nb_segs, u32 *frags_per_segment, u32 *start_range, u32 *end_range, Bool use_ssix);

GF_Err gf_isom_setup_track_fragment_template(GF_ISOFile *movie, GF_ISOTrackID TrackID, u8 *boxes, u32 boxes_size, u8 force_traf_flags);


enum
{
	/*indicates that the track fragment has no samples but still has a duration
	(silence-detection in audio codecs, ...).
	param: indicates duration*/
	GF_ISOM_TRAF_EMPTY,
	/*I-Frame detection: this can reduce file size by detecting I-frames and
	optimizing sample flags (padding, priority, ..)
	param: on/off (0/1)*/
	GF_ISOM_TRAF_RANDOM_ACCESS,
	/*activate data cache on track fragment. This is useful when writing interleaved
	media from a live source (typically audio-video), and greatly reduces file size
	param: Number of samples (> 1) to cache before disk flushing. You shouldn't try
	to cache too many samples since this will load your memory. base that on FPS/SR*/
	GF_ISOM_TRAF_DATA_CACHE,
	/*forces moof base offsets when traf based offsets would be chosen
	param: on/off (0/1)*/
	GF_ISOM_TFHD_FORCE_MOOF_BASE_OFFSET
};

/*set options. Options can be set at the beginning of each new fragment only, and for the
lifetime of the fragment*/
GF_Err gf_isom_set_fragment_option(GF_ISOFile *the_file, GF_ISOTrackID TrackID, u32 Code, u32 param);


/*adds a sample to a fragmented track

*TrackID: destination track
*sample: sample to add
*StreamDescriptionIndex: stream description for this sample. If 0, the default one
is used
*Duration: sample duration.
Note: because of the interleaved nature of the meta/media data, the sample duration
MUST be provided (in case of regular tracks, this was computed internally by the lib)
*PaddingBits: padding bits for the sample, or 0
*DegradationPriority for the sample, or 0
*redundantCoding: indicates this is samples acts as a sync shadow point

*/

GF_Err gf_isom_fragment_add_sample(GF_ISOFile *the_file, GF_ISOTrackID TrackID, const GF_ISOSample *sample,
                                   u32 StreamDescriptionIndex,
                                   u32 Duration, u8 PaddingBits, u16 DegradationPriority, Bool redundantCoding);

/*appends data into last sample of track for video fragments/other media
CANNOT be used with OD tracks*/
GF_Err gf_isom_fragment_append_data(GF_ISOFile *the_file, GF_ISOTrackID TrackID, u8 *data, u32 data_size, u8 PaddingBits);

/*reset internal info used with fragments and segment. Should be called when seeking (with keep_sample_count=0) or when loading a media segments with the same timing as the previously loaded segment*/
void gf_isom_reset_fragment_info(GF_ISOFile *movie, Bool keep_sample_count);

/*reset sample count to 0 and next moof number to 0. When doint scalable media, should be called before opening the segment containing
the base layer in order to make sure the sample count base number is always the same (ie 1) on all tracks*/
void gf_isom_reset_sample_count(GF_ISOFile *movie);

void gf_isom_reset_seq_num(GF_ISOFile *movie);

/*return the duration of the movie+fragments if known, 0 if error*/
u64 gf_isom_get_fragmented_duration(GF_ISOFile *movie);
/*returns the number of sidx boxes*/
u32 gf_isom_get_fragments_count(GF_ISOFile *movie, Bool segments_only);
/*gets total sample number and duration*/
GF_Err gf_isom_get_fragmented_samples_info(GF_ISOFile *movie, GF_ISOTrackID trackID, u32 *nb_samples, u64 *duration);

GF_Err gf_isom_fragment_set_cenc_sai(GF_ISOFile *output, GF_ISOTrackID trackID, u32 IV_size, u8 *sai_b, u32 sai_b_size, Bool use_subsample, Bool use_saio_32bit);

GF_Err gf_isom_clone_pssh(GF_ISOFile *output, GF_ISOFile *input, Bool in_moof);

GF_Err gf_isom_fragment_set_sample_roll_group(GF_ISOFile *movie, GF_ISOTrackID trackID, u32 sample_number, Bool is_roll, s16 roll_distance);
GF_Err gf_isom_fragment_set_sample_rap_group(GF_ISOFile *movie, GF_ISOTrackID trackID, u32 sample_number_in_frag, Bool is_rap, u32 num_leading_samples);
GF_Err gf_isom_fragment_set_sample_flags(GF_ISOFile *movie, GF_ISOTrackID trackID, u32 is_leading, u32 dependsOn, u32 dependedOn, u32 redundant);

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/

/******************************************************************
		GENERIC Publishing API
******************************************************************/


/*set the GroupID of a track (only used for optimized interleaving). By setting GroupIDs
you can specify the storage order for media data of a group of streams. This is useful
for BIFS presentation so that static resources of the scene can be downloaded before BIFS*/
GF_Err gf_isom_set_track_interleaving_group(GF_ISOFile *the_file, u32 trackNumber, u32 GroupID);

/*set the priority of a track within a Group (used for optimized interleaving and hinting).
This allows tracks to be stored before other within a same group, for instance the
hint track data can be stored just before the media data, reducing disk seeking
for a same time, within a group of tracks, the track with the lowest inversePriority will
be written first*/
GF_Err gf_isom_set_track_priority_in_group(GF_ISOFile *the_file, u32 trackNumber, u32 InversePriority);

GF_Err gf_isom_hint_max_chunk_size(GF_ISOFile *the_file, u32 trackNumber, u32 maxChunkSize);

/*associate a given SL config with a given ESD while extracting the OD information
all the SL params must be fixed by the calling app!
The SLConfig is stored by the API for further use. A NULL pointer will result
in using the default SLConfig (predefined = 2) remapped to predefined = 0
This is useful while reading the IOD / OD stream of an MP4 file. Note however that
only full AUs are extracted, therefore the calling application must SL-packetize the streams*/
GF_Err gf_isom_set_extraction_slc(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, GF_SLConfig *slConfig);

/*setup interleaving for storage (shortcut for storeage mode + interleave_time)*/
GF_Err gf_isom_make_interleave(GF_ISOFile *mp4file, Double TimeInSec);

void gf_isom_set_progress_callback(GF_ISOFile *file, void (*progress_cbk)(void *udta, u64 nb_done, u64 nb_total), void *progress_cbk_udta);

/******************************************************************
		GENERIC HINTING WRITING API
******************************************************************/

/*supported hint formats - ONLY RTP now*/
enum
{
	GF_ISOM_HINT_RTP = GF_4CC('r', 't', 'p', ' '),
};

#ifndef GPAC_DISABLE_ISOM_HINTING


/*Setup the resources based on the hint format
This function MUST be called after creating a new hint track and before
any other calls on this track*/
GF_Err gf_isom_setup_hint_track(GF_ISOFile *the_file, u32 trackNumber, u32 HintType);

/*Create a HintDescription for the HintTrack
the rely flag indicates whether a reliable transport protocol is desired/required
for data transport
	0: not desired (UDP/IP). NB: most RTP streaming servers only support UDP/IP for data
	1: preferable (TCP/IP if possible or UDP/IP)
	2: required (TCP/IP only)
The HintDescriptionIndex is set, to be used when creating a HINT sample
*/
GF_Err gf_isom_new_hint_description(GF_ISOFile *the_file, u32 trackNumber, s32 HintTrackVersion, s32 LastCompatibleVersion, u8 Rely, u32 *HintDescriptionIndex);

/*Starts a new sample for the hint track. A sample is just a collection of packets
the transmissionTime is indicated in the media timeScale of the hint track*/
GF_Err gf_isom_begin_hint_sample(GF_ISOFile *the_file, u32 trackNumber, u32 HintDescriptionIndex, u32 TransmissionTime);

/*stores the hint sample in the file once all your packets for this sample are done
set IsRandomAccessPoint if you want to indicate that this is a random access point
in the stream*/
GF_Err gf_isom_end_hint_sample(GF_ISOFile *the_file, u32 trackNumber, u8 IsRandomAccessPoint);


/******************************************************************
		PacketHandling functions
		Data can be added at the end or at the beginning of the current packet
		by setting AtBegin to 1 the data will be added at the beginning
		This allows constructing the packet payload before any meta-data
******************************************************************/

/*adds a blank chunk of data in the sample that is skipped while streaming*/
GF_Err gf_isom_hint_blank_data(GF_ISOFile *the_file, u32 trackNumber, u8 AtBegin);

/*adds a chunk of data in the packet that is directly copied while streaming
NOTE: dataLength MUST BE <= 14 bytes, and you should only use this function
to add small blocks of data (encrypted parts, specific headers, ...)*/
GF_Err gf_isom_hint_direct_data(GF_ISOFile *the_file, u32 trackNumber, u8 *data, u32 dataLength, u8 AtBegin);

/*adds a reference to some sample data in the packet
SourceTrackID: the ID of the track where the referenced sample is
SampleNumber: the sample number containing the data to be added
DataLength: the length of bytes to copy in the packet
offsetInSample: the offset in bytes in the sample at which to begin copying data

extra_data: only used when the sample is actually the sample that will contain this packet
(useful to store en encrypted version of a packet only available while streaming)
	In this case, set SourceTrackID to the HintTrack ID and SampleNumber to 0
	In this case, the DataOffset MUST BE NULL and length will indicate the extra_data size

Note that if you want to reference a previous HintSample in the hintTrack, you will
have to parse the sample yourself ...
*/
GF_Err gf_isom_hint_sample_data(GF_ISOFile *the_file, u32 trackNumber, GF_ISOTrackID SourceTrackID, u32 SampleNumber, u16 DataLength, u32 offsetInSample, u8 *extra_data, u8 AtBegin);


/*adds a reference to some stream description data in the packet (headers, ...)
SourceTrackID: the ID of the track where the referenced sample is
StreamDescriptionIndex: the index of the stream description in the desired track
DataLength: the length of bytes to copy in the packet
offsetInDescription: the offset in bytes in the description at which to begin copying data

Since it is far from being obvious what this offset is, we recommend not using this
function. The ISO Media Format specification is currently being updated to solve
this issue*/
GF_Err gf_isom_hint_sample_description_data(GF_ISOFile *the_file, u32 trackNumber, GF_ISOTrackID SourceTrackID, u32 StreamDescriptionIndex, u16 DataLength, u32 offsetInDescription, u8 AtBegin);


/******************************************************************
		RTP SPECIFIC WRITING API
******************************************************************/

/*Creates a new RTP packet in the HintSample. If a previous packet was created,
it is stored in the hint sample and a new packet is created.
- relativeTime: RTP time offset of this packet in the HintSample if any - in hint track
time scale. Used for data smoothing by servers.
- PackingBit: the 'P' bit of the RTP packet header
- eXtensionBit: the'X' bit of the RTP packet header
- MarkerBit: the 'M' bit of the RTP packet header
- PayloadType: the payload type, on 7 bits, format 0x0XXXXXXX
- B_frame: indicates if this is a B-frame packet. Can be skipped by a server
- IsRepeatedPacket: indicates if this is a duplicate packet of a previous one.
Can be skipped by a server
- SequenceNumber: the RTP base sequence number of the packet. Because of support for repeated
packets, you have to set the sequence number yourself.*/
GF_Err gf_isom_rtp_packet_begin(GF_ISOFile *the_file, u32 trackNumber, s32 relativeTime, u8 PackingBit, u8 eXtensionBit, u8 MarkerBit, u8 PayloadType, u8 B_frame, u8 IsRepeatedPacket, u16 SequenceNumber);

/*set the flags of the RTP packet*/
GF_Err gf_isom_rtp_packet_set_flags(GF_ISOFile *the_file, u32 trackNumber, u8 PackingBit, u8 eXtensionBit, u8 MarkerBit, u8 disposable_packet, u8 IsRepeatedPacket);

/*set the time offset of this packet. This enables packets to be placed in the hint track
in decoding order, but have their presentation time-stamp in the transmitted
packet in a different order. Typically used for MPEG video with B-frames
*/
GF_Err gf_isom_rtp_packet_set_offset(GF_ISOFile *the_file, u32 trackNumber, s32 timeOffset);


/*set some specific info in the HintDescription for RTP*/

/*sets the RTP TimeScale that the server use to send packets
some RTP payloads may need a specific timeScale that is not the timeScale in the file format
the default timeScale choosen by the API is the MediaTimeScale of the hint track*/
GF_Err gf_isom_rtp_set_timescale(GF_ISOFile *the_file, u32 trackNumber, u32 HintDescriptionIndex, u32 TimeScale);
/*sets the RTP TimeOffset that the server will add to the packets
if not set, the server adds a random offset*/
GF_Err gf_isom_rtp_set_time_offset(GF_ISOFile *the_file, u32 trackNumber, u32 HintDescriptionIndex, u32 TimeOffset);
/*sets the RTP SequenceNumber Offset that the server will add to the packets
if not set, the server adds a random offset*/
GF_Err gf_isom_rtp_set_time_sequence_offset(GF_ISOFile *the_file, u32 trackNumber, u32 HintDescriptionIndex, u32 SequenceNumberOffset);



/******************************************************************
		SDP SPECIFIC WRITING API
******************************************************************/
/*add an SDP line to the SDP container at the track level (media-specific SDP info)
NOTE: the \r\n end of line for SDP is automatically inserted*/
GF_Err gf_isom_sdp_add_track_line(GF_ISOFile *the_file, u32 trackNumber, const char *text);
/*remove all SDP info at the track level*/
GF_Err gf_isom_sdp_clean_track(GF_ISOFile *the_file, u32 trackNumber);

/*add an SDP line to the SDP container at the movie level (presentation SDP info)
NOTE: the \r\n end of line for SDP is automatically inserted*/
GF_Err gf_isom_sdp_add_line(GF_ISOFile *the_file, const char *text);
/*remove all SDP info at the movie level*/
GF_Err gf_isom_sdp_clean(GF_ISOFile *the_file);

#endif /*GPAC_DISABLE_ISOM_HINTING*/


#endif	/*GPAC_DISABLE_ISOM_WRITE*/

#ifndef GPAC_DISABLE_ISOM_DUMP

/*dumps file structures into XML trace file */
GF_Err gf_isom_dump(GF_ISOFile *file, FILE *trace, Bool skip_init);
/*gets number of implemented boxes in GPAC. There can be several times the same type returned due to variation of the box (versions or flags)*/
u32 gf_isom_get_num_supported_boxes();
/*gets 4cc of box given its index. Index 0 is GPAC internal unknown box handler.*/
u32 gf_isom_get_supported_box_type(u32 idx);
/*prints default box syntax of box given its index. Index 0 is GPAC internal unknown box handler*/
GF_Err gf_isom_dump_supported_box(u32 idx, FILE * trace);

#endif /*GPAC_DISABLE_ISOM_DUMP*/


#ifndef GPAC_DISABLE_ISOM_HINTING

#ifndef GPAC_DISABLE_ISOM_DUMP
/*dumps RTP hint samples structure into XML trace file
	@trackNumber, @SampleNum: hint track and hint sample number
	@trace: output
*/
GF_Err gf_isom_dump_hint_sample(GF_ISOFile *the_file, u32 trackNumber, u32 SampleNum, FILE * trace);
#endif

/*Get SDP info at the movie level*/
GF_Err gf_isom_sdp_get(GF_ISOFile *the_file, const char **sdp, u32 *length);
/*Get SDP info at the track level*/
GF_Err gf_isom_sdp_track_get(GF_ISOFile *the_file, u32 trackNumber, const char **sdp, u32 *length);

u32 gf_isom_get_payt_count(GF_ISOFile *the_file, u32 trackNumber);
const char *gf_isom_get_payt_info(GF_ISOFile *the_file, u32 trackNumber, u32 index, u32 *payID);


#endif /*GPAC_DISABLE_ISOM_HINTING*/



/*
				3GPP specific extensions
	NOTE: MPEG-4 OD Framework cannot be used with 3GPP files.
	Stream Descriptions are not GF_ESD, just generic config options as specified in this file
*/

/*Generic 3GP/3GP2 config record*/
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

	/*H263 ONLY - Level and profile*/
	u8 H263_level, H263_profile;

	/*AMR(WB) ONLY - num of mode for the codec*/
	u16 AMR_mode_set;
	/*AMR(WB) ONLY - changes in codec mode per sample*/
	u8 AMR_mode_change_period;
} GF_3GPConfig;


/*return the 3GP config for this tream description, NULL if not a 3GPP track*/
GF_3GPConfig *gf_isom_3gp_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex);
#ifndef GPAC_DISABLE_ISOM_WRITE
/*create the track config*/
GF_Err gf_isom_3gp_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_3GPConfig *config, char *URLname, char *URNname, u32 *outDescriptionIndex);
/*update the track config - subtypes shall NOT differ*/
GF_Err gf_isom_3gp_config_update(GF_ISOFile *the_file, u32 trackNumber, GF_3GPConfig *config, u32 DescriptionIndex);
#endif	/*GPAC_DISABLE_ISOM_WRITE*/

/*AVC/H264 extensions - GF_AVCConfig is defined in mpeg4_odf.h*/

/*gets uncompressed AVC config - user is responsible for deleting it*/
GF_AVCConfig *gf_isom_avc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);
/*gets uncompressed SVC config - user is responsible for deleting it*/
GF_AVCConfig *gf_isom_svc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);
/*gets uncompressed MVC config - user is responsible for deleting it*/
GF_AVCConfig *gf_isom_mvc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

typedef enum
{
	GF_ISOM_AVCTYPE_NONE=0,
	GF_ISOM_AVCTYPE_AVC_ONLY,
	GF_ISOM_AVCTYPE_AVC_SVC,
	GF_ISOM_AVCTYPE_SVC_ONLY,
	GF_ISOM_AVCTYPE_AVC_MVC,
	GF_ISOM_AVCTYPE_MVC_ONLY,
} GF_ISOMAVCType;

u32 gf_isom_get_avc_svc_type(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);


typedef enum
{
	GF_ISOM_HEVCTYPE_NONE=0,
	GF_ISOM_HEVCTYPE_HEVC_ONLY,
	GF_ISOM_HEVCTYPE_HEVC_LHVC,
	GF_ISOM_HEVCTYPE_LHVC_ONLY,
} GF_ISOMHEVCType;

u32 gf_isom_get_hevc_lhvc_type(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

/*gets HEVC config - user is responsible for deleting it*/
GF_HEVCConfig *gf_isom_hevc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);
/*gets LHVC config - user is responsible for deleting it*/
GF_HEVCConfig *gf_isom_lhvc_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

/*gets AV1 config - user is responsible for deleting it*/
GF_AV1Config *gf_isom_av1_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

/*gets VP config - user is responsible for deleting it*/
GF_VPConfig *gf_isom_vp_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

/*return true if track dependencies implying extractors or implicit reconstruction are found*/
Bool gf_isom_needs_layer_reconstruction(GF_ISOFile *file);

enum
{
	/*all extractors are rewritten*/
	GF_ISOM_NALU_EXTRACT_DEFAULT = 0,
	/*all extractors are skipped but NALU data from this track is kept*/
	GF_ISOM_NALU_EXTRACT_LAYER_ONLY,
	/*all extractors are kept (untouched sample) - used for dumping modes*/
	GF_ISOM_NALU_EXTRACT_INSPECT,
	/*above mode is applied and PPS/SPS/... are appended in the front of every IDR*/
	GF_ISOM_NALU_EXTRACT_INBAND_PS_FLAG = 1<<16,
	/*above mode is applied and all start codes are rewritten (xPS inband as well)*/
	GF_ISOM_NALU_EXTRACT_ANNEXB_FLAG = 2<<17,
	/*above mode is applied and VDRD NAL unit is inserted before SVC slice*/
	GF_ISOM_NALU_EXTRACT_VDRD_FLAG = 1<<18,
	/*all extractors are skipped and only tile track data is kept*/
	GF_ISOM_NALU_EXTRACT_TILE_ONLY = 1<<19
};

GF_Err gf_isom_set_nalu_extract_mode(GF_ISOFile *the_file, u32 trackNumber, u32 nalu_extract_mode);
u32 gf_isom_get_nalu_extract_mode(GF_ISOFile *the_file, u32 trackNumber);


#ifndef GPAC_DISABLE_ISOM_WRITE
/*creates new AVC config*/
GF_Err gf_isom_avc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex);
/*updates AVC config*/
GF_Err gf_isom_avc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg);
/*updates SVC config. If is_additional is set, the SVCConfig will be added to the AVC sample description, otherwise the sample description will be SVC-only*/
GF_Err gf_isom_svc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg, Bool is_additional);
/*creates new SVC config*/
GF_Err gf_isom_svc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex);
/*deletes SVC config*/
GF_Err gf_isom_svc_config_del(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

/*updates MVC config. If is_additional is set, the MVCConfig will be added to the AVC sample description, otherwise the sample description will be MVC-only*/
GF_Err gf_isom_mvc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_AVCConfig *cfg, Bool is_additional);
/*creates new MVC config*/
GF_Err gf_isom_mvc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex);
/*deletes MVC config*/
GF_Err gf_isom_mvc_config_del(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

/*sets avc3 entry type (inband SPS/PPS) instead of avc1 (SPS/PPS in avcC box)*/
GF_Err gf_isom_avc_set_inband_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, Bool keep_xps);

/*sets hev1 entry type (inband SPS/PPS) instead of hvc1 (SPS/PPS in hvcC box)*/
GF_Err gf_isom_hevc_set_inband_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, Bool keep_xps);

/*sets lhe1 entry type instead of lhc1 but keep lhcC box intact*/
GF_Err gf_isom_lhvc_force_inband_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex);

/*sets hvt1 entry type (tile track) or hev2/hvc2 type if is_base_track is set - cfg may be set to indicate sub-profile of the tile. It is the use responsability to set the tbas track reference to the base hevc track*/
GF_Err gf_isom_hevc_set_tile_config(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_HEVCConfig *cfg, Bool is_base_track);


/*creates new HEVC config*/
GF_Err gf_isom_hevc_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_HEVCConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex);
/*updates HEVC config*/
GF_Err gf_isom_hevc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_HEVCConfig *cfg);

/*updates L-HHVC config*/
typedef enum {
	//changes track type to LHV1/LHE1: no base nor extractors in track, just enhancement layers
	GF_ISOM_LEHVC_ONLY = 0,
	//changes track type to HVC2/HEV2: base and extractors/enh. in track
	GF_ISOM_LEHVC_WITH_BASE,
	//changes track type to HVC1/HEV1 with additionnal cfg: base and enh. in track no extractors
	GF_ISOM_LEHVC_WITH_BASE_BACKWARD,
	//changes track type to HVC2/HEV2 for tile base tracks
	GF_ISOM_HEVC_TILE_BASE,
} GF_ISOMLHEVCTrackType;
GF_Err gf_isom_lhvc_config_update(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex, GF_HEVCConfig *cfg, GF_ISOMLHEVCTrackType track_type);

//sets nalu size length - samles must be rewritten by the caller
GF_Err gf_isom_set_nalu_length_field(GF_ISOFile *file, u32 track, u32 StreamDescriptionIndex, u32 nalu_size_length);

/*creates new VPx config*/
GF_Err gf_isom_vp_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_VPConfig *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex, u32 vpx_type);


/*creates new AV1 config*/
GF_Err gf_isom_av1_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AV1Config *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex);


#endif /*GPAC_DISABLE_ISOM_WRITE*/


/*
	3GP timed text handling

	NOTE: currently only writing API is developped, the reading one is not used in MPEG-4 since
	MPEG-4 maps 3GP timed text to MPEG-4 Streaming Text (part 17)
*/

/*set streamihng text reading mode: if do_convert is set, all text samples will be retrieved as TTUs
and ESD will be emulated for text tracks.*/
GF_Err gf_isom_text_set_streaming_mode(GF_ISOFile *the_file, Bool do_convert);


#ifndef GPAC_DISABLE_ISOM_DUMP
/*exports text track to given format
@dump_type: 0 for TTXT, 1 for srt, 2 for SVG
*/
typedef enum {
	GF_TEXTDUMPTYPE_TTXT = 0,
	GF_TEXTDUMPTYPE_TTXT_BOXES = 1,
	GF_TEXTDUMPTYPE_SRT  = 2,
	GF_TEXTDUMPTYPE_SVG  = 3,
} GF_TextDumpType;
GF_Err gf_isom_text_dump(GF_ISOFile *the_file, u32 track, FILE *dump, GF_TextDumpType dump_type);
#endif

/*returns encoded TX3G box (text sample description for 3GPP text streams) as needed by RTP or other standards:
	@sidx: 1-based stream description index
	@sidx_offset:
		if 0, the sidx will NOT be written before the encoded TX3G
		if not 0, the sidx will be written before the encoded TX3G, with the given offset. Offset sshould be at
		least 128 for most commmon usage of TX3G (RTP, MPEG-4 timed text, etc)

*/
GF_Err gf_isom_text_get_encoded_tx3g(GF_ISOFile *file, u32 track, u32 sidx, u32 sidx_offset, u8 **tx3g, u32 *tx3g_size);

/*checks if this text description is already inserted
@outDescIdx: set to 0 if not found, or descIndex
@same_style, @same_box: indicates if default styles and box are used
*/
GF_Err gf_isom_text_has_similar_description(GF_ISOFile *the_file, u32 trackNumber, GF_TextSampleDescriptor *desc, u32 *outDescIdx, Bool *same_box, Bool *same_styles);

/*text sample formatting*/
typedef struct _3gpp_text_sample GF_TextSample;
/*creates text sample handle*/
GF_TextSample *gf_isom_new_text_sample();
/*destroy text sample handle*/
void gf_isom_delete_text_sample(GF_TextSample *tx_samp);

/*generic subtitle sample formatting*/
typedef struct _generic_subtitle_sample GF_GenericSubtitleSample;
/*creates generic subtitle sample handle*/
GF_GenericSubtitleSample *gf_isom_new_generic_subtitle_sample();
/*destroy generic subtitle sample handle*/
void gf_isom_delete_generic_subtitle_sample(GF_GenericSubtitleSample *generic_subtitle_samp);

#ifndef GPAC_DISABLE_VTT
GF_Err gf_isom_new_webvtt_description(GF_ISOFile *movie, u32 trackNumber, char *URLname, char *URNname, u32 *outDescriptionIndex, const char *config);
GF_Err gf_isom_update_webvtt_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, const char *config);
#endif
const char *gf_isom_get_webvtt_config(GF_ISOFile *file, u32 track, u32 descriptionIndex);

GF_Err gf_isom_stxt_get_description(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, const char **mime, const char **encoding, const char **config);
GF_Err gf_isom_new_stxt_description(GF_ISOFile *movie, u32 trackNumber, u32 type, const char *mime, const char *encoding, const char *config, u32 *outDescriptionIndex);
GF_Err gf_isom_update_stxt_description(GF_ISOFile *movie, u32 trackNumber, const char *encoding, const char *config, u32 DescriptionIndex);

GF_Err gf_isom_xml_subtitle_get_description(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex,
        const char **xmlnamespace, const char **xml_schema_loc, const char **mimes);
GF_Err gf_isom_new_xml_subtitle_description(GF_ISOFile  *movie, u32 trackNumber,
        const char *xmlnamespace, const char *xml_schema_loc, const char *auxiliary_mimes,
        u32 *outDescriptionIndex);
GF_Err gf_isom_update_xml_subtitle_description(GF_ISOFile *movie, u32 trackNumber,
        u32 descriptionIndex, GF_GenericSubtitleSampleDescriptor *desc);



typedef enum
{
	GF_ISOM_TEXT_FLAGS_OVERWRITE = 0,
	GF_ISOM_TEXT_FLAGS_TOGGLE,
	GF_ISOM_TEXT_FLAGS_UNTOGGLE,
} GF_TextFlagsMode;
//sets text display flags according to given mode. If SampleDescriptionIndex is 0, sets the flags for all text descriptions.
GF_Err gf_isom_text_set_display_flags(GF_ISOFile *file, u32 track, u32 SampleDescriptionIndex, u32 flags, GF_TextFlagsMode op_type);

#ifndef GPAC_DISABLE_ISOM_WRITE

/*Create a new TextSampleDescription in the file.
The URL and URN are used to describe external media, this will create a data reference for the media
GF_TextSampleDescriptor is defined in mpeg4_odf.h
*/
GF_Err gf_isom_new_text_description(GF_ISOFile *the_file, u32 trackNumber, GF_TextSampleDescriptor *desc, char *URLname, char *URNname, u32 *outDescriptionIndex);
/*change the text sample description*/
GF_Err gf_isom_update_text_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, GF_TextSampleDescriptor *desc);

/*reset text sample content*/
GF_Err gf_isom_text_reset(GF_TextSample * tx_samp);
/*reset text sample styles but keep text*/
GF_Err gf_isom_text_reset_styles(GF_TextSample * samp);

/*sets UTF16 marker for text data. This MUST be called on an empty sample. If text data added later
on (cf below) is not formatted as UTF16 data(2 bytes char) the resulting text sample won't be compliant,
but this library WON'T WARN*/
GF_Err gf_isom_text_set_utf16_marker(GF_TextSample * samp);
/*append text to sample - text_len is the number of bytes to be written from text_data. This allows
handling UTF8 and UTF16 strings in a transparent manner*/
GF_Err gf_isom_text_add_text(GF_TextSample * tx_samp, char *text_data, u32 text_len);
/*append style modifyer to sample*/
GF_Err gf_isom_text_add_style(GF_TextSample * tx_samp, GF_StyleRecord *rec);
/*appends highlight modifier for the sample
	@start_char: first char highlighted,
	@end_char: first char not highlighted*/
GF_Err gf_isom_text_add_highlight(GF_TextSample * samp, u16 start_char, u16 end_char);
/*sets highlight color for the whole sample*/
GF_Err gf_isom_text_set_highlight_color(GF_TextSample * samp, u8 r, u8 g, u8 b, u8 a);
GF_Err gf_isom_text_set_highlight_color_argb(GF_TextSample * samp, u32 argb);
/*appends a new karaoke sequence in the sample
	@start_time: karaoke start time expressed in text stream timescale, but relative to the sample media time
*/
GF_Err gf_isom_text_add_karaoke(GF_TextSample * samp, u32 start_time);
/*appends a new segment in the current karaoke sequence - you must build sequences in order to be compliant
	@end_time: segment end time expressed in text stream timescale, but relative to the sample media time
	@start_char: first char highlighted,
	@end_char: first char not highlighted
*/
GF_Err gf_isom_text_set_karaoke_segment(GF_TextSample * samp, u32 end_time, u16 start_char, u16 end_char);
/*sets scroll delay for the whole sample (scrolling is enabled through GF_TextSampleDescriptor.DisplayFlags)
	@scroll_delay: delay for scrolling expressed in text stream timescale
*/
GF_Err gf_isom_text_set_scroll_delay(GF_TextSample * samp, u32 scroll_delay);
/*appends hyperlinking for the sample
	@URL: ASCII url
	@altString: ASCII hint (tooltip, ...) for end user
	@start_char: first char hyperlinked,
	@end_char: first char not hyperlinked
*/
GF_Err gf_isom_text_add_hyperlink(GF_TextSample * samp, char *URL, char *altString, u16 start_char, u16 end_char);
/*sets current text box (display pos&size within the text track window) for the sample*/
GF_Err gf_isom_text_set_box(GF_TextSample * samp, s16 top, s16 left, s16 bottom, s16 right);
/*appends blinking for the sample
	@start_char: first char blinking,
	@end_char: first char not blinking
*/
GF_Err gf_isom_text_add_blink(GF_TextSample * samp, u16 start_char, u16 end_char);
/*sets wrap flag for the sample - currently only 0 (no wrap) and 1 ("soft wrap") are allowed in 3GP*/
GF_Err gf_isom_text_set_wrap(GF_TextSample * samp, u8 wrap_flags);

/*formats sample as a regular GF_ISOSample. The resulting sample will always be marked as random access
text sample content is kept untouched*/
GF_ISOSample *gf_isom_text_to_sample(GF_TextSample * tx_samp);
GF_Err gf_isom_text_sample_write_bs(GF_TextSample *tx_samp, GF_BitStream *bs);

u32 gf_isom_text_sample_size(GF_TextSample *samp);


GF_GenericSubtitleSample *gf_isom_new_xml_subtitle_sample();
void gf_isom_delete_xml_subtitle_sample(GF_GenericSubtitleSample * samp);
GF_Err gf_isom_xml_subtitle_reset(GF_GenericSubtitleSample *samp);
GF_ISOSample *gf_isom_xml_subtitle_to_sample(GF_GenericSubtitleSample * tx_samp);
GF_Err gf_isom_xml_subtitle_sample_add_text(GF_GenericSubtitleSample *samp, char *text_data, u32 text_len);

#endif	/*GPAC_DISABLE_ISOM_WRITE*/

/*****************************************************
		ISMACryp Samples
*****************************************************/
/*flags for GF_ISMASample*/
enum
{
	/*signals the stream the sample belongs to uses selective encryption*/
	GF_ISOM_ISMA_USE_SEL_ENC = 1,
	/*signals the sample is encrypted*/
	GF_ISOM_ISMA_IS_ENCRYPTED = 2,
};

typedef struct
{
	/*IV in ISMACryp is Byte Stream Offset*/
	u64 IV;
	u8 IV_length;/*repeated from sampleDesc for convenience*/
	u8 *key_indicator;
	u8 KI_length;/*repeated from sampleDesc for convenience*/
	u32 dataLength;
	u8 *data;
	u32 flags;
} GF_ISMASample;
/**
 * creates a new empty ISMA sample
 */
GF_ISMASample *gf_isom_ismacryp_new_sample();

/*delete an ISMA sample. NOTE:the buffers content will be destroyed by default.
if you wish to keep the buffer, set dataLength to 0 in the sample before deleting it*/
void gf_isom_ismacryp_delete_sample(GF_ISMASample *samp);

/*decodes ISMACryp sample based on all info in ISMACryp sample description*/
GF_ISMASample *gf_isom_ismacryp_sample_from_data(u8 *data, u32 dataLength, Bool use_selective_encryption, u8 KI_length, u8 IV_length);
/*rewrites samp content from s content*/
GF_Err gf_isom_ismacryp_sample_to_sample(GF_ISMASample *s, GF_ISOSample *dest);

/*decodes ISMACryp sample based on sample and its descrition index - returns NULL if not an ISMA sample
Note: input sample is NOT destroyed*/
GF_ISMASample *gf_isom_get_ismacryp_sample(GF_ISOFile *the_file, u32 trackNumber, GF_ISOSample *samp, u32 sampleDescriptionIndex);

/*returns whether the given media is a protected one or not - return scheme protection 4CC*/
u32 gf_isom_is_media_encrypted(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*returns whether the given media is a protected ISMACryp one or not*/
Bool gf_isom_is_ismacryp_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex);

/*returns whether the given media is a protected ISMACryp one or not*/
Bool gf_isom_is_omadrm_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex);

GF_Err gf_isom_get_omadrm_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat,
                               u32 *outSchemeType, u32 *outSchemeVersion,
                               const char **outContentID, const char **outRightsIssuerURL, const char **outTextualHeaders, u32 *outTextualHeadersLen, u64 *outPlaintextLength, u32 *outEncryptionType, Bool *outSelectiveEncryption, u32 *outIVLength, u32 *outKeyIndicationLength);
/*retrieves ISMACryp info for the given track & SDI - all output parameters are optional - URIs SHALL NOT BE MODIFIED BY USER
	@outOriginalFormat: retrieves orginal protected media format - usually GF_ISOM_SUBTYPE_MPEG4
	@outSchemeType: retrieves 4CC of protection scheme (GF_ISOM_ISMACRYP_SCHEME = iAEC in ISMACryp 1.0)
	outSchemeVersion: retrieves version of protection scheme (1 in ISMACryp 1.0)
	outSchemeURI: retrieves URI location of scheme
	outKMS_URI: retrieves URI location of key management system - only valid with ISMACryp 1.0
	outSelectiveEncryption: specifies whether sample-based encryption is used in media - only valid with ISMACryp 1.0
	outIVLength: specifies length of Initial Vector - only valid with ISMACryp 1.0
	outKeyIndicationLength: specifies length of key indicator - only valid with ISMACryp 1.0

  outSelectiveEncryption, outIVLength and outKeyIndicationLength are usually not needed to decode an
  ISMA sample when using gf_isom_get_ismacryp_sample fct above
*/
GF_Err gf_isom_get_ismacryp_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, const char **outSchemeURI, const char **outKMS_URI, Bool *outSelectiveEncryption, u32 *outIVLength, u32 *outKeyIndicationLength);

/*returns original format type of a protected media file*/
GF_Err gf_isom_get_original_format_type(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat);

typedef struct
{
	//16 bit stored but we use 32
	u32 bytes_clear_data;
	u32 bytes_encrypted_data;
} GF_CENCSubSampleEntry;

typedef struct __cenc_sample_aux_info
{
	u8 IV_size; //0, 8 or 16; it MUST NOT be written to file
	bin128 IV; /*can be 0, 64 or 128 bits - if 64, bytes 0-7 are used and 8-15 are 0-padded*/
	u16 subsample_count;
	GF_CENCSubSampleEntry *subsamples;
} GF_CENCSampleAuxInfo;

#ifndef GPAC_DISABLE_ISOM_WRITE
/*removes protection info (does not perform decryption :), for ISMA, OMA and CENC*/
GF_Err gf_isom_remove_track_protection(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex);

/*creates ISMACryp protection info (does not perform encryption :)*/
GF_Err gf_isom_set_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type,
                                       u32 scheme_version, char *scheme_uri, char *kms_URI,
                                       Bool selective_encryption, u32 KI_length, u32 IV_length);

/*change scheme URI and/or KMS URI for crypted files. Other params cannot be changed once the media is crypted
	@scheme_uri: new scheme URI, or NULL to keep previous
	@kms_uri: new KMS URI, or NULL to keep previous
*/
GF_Err gf_isom_change_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, char *scheme_uri, char *kms_uri);


GF_Err gf_isom_set_oma_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index,
                                  char *contentID, char *kms_URI, u32 encryption_type, u64 plainTextLength, char *textual_headers, u32 textual_headers_len,
                                  Bool selective_encryption, u32 KI_length, u32 IV_length);

GF_Err gf_isom_set_generic_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type, u32 scheme_version, char *scheme_uri, char *kms_URI);

GF_Err gf_isom_cenc_allocate_storage(GF_ISOFile *the_file, u32 trackNumber, u32 container_type, u32 AlgorithmID, u8 IV_size, bin128 KID);

//if buf is NULL but len is given, this adds an unencrypted entry. otherwise, buf && len represent the sai cenc info to add
GF_Err gf_isom_track_cenc_add_sample_info(GF_ISOFile *the_file, u32 trackNumber, u32 container_type, u8 IV_size, u8 *buf, u32 len, Bool use_subsamples, u8 *clear_IV, Bool use_saio_32bit);



GF_Err gf_isom_set_cenc_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type,
                                   u32 scheme_version, u32 default_IsEncrypted, u8 default_IV_size, bin128 default_KID,
								   u8 default_crypt_byte_block, u8 default_skip_byte_block,
								   u8 default_constant_IV_size, bin128 default_constant_IV);

GF_Err gf_cenc_set_pssh(GF_ISOFile *mp4, bin128 systemID, u32 version, u32 KID_count, bin128 *KID, u8 *data, u32 len);

GF_Err gf_isom_remove_cenc_saiz(GF_ISOFile *the_file, u32 trackNumber);
GF_Err gf_isom_remove_cenc_saio(GF_ISOFile *the_file, u32 trackNumber);
GF_Err gf_isom_remove_samp_enc_box(GF_ISOFile *the_file, u32 trackNumber);
GF_Err gf_isom_remove_samp_group_box(GF_ISOFile *the_file, u32 trackNumber);
GF_Err gf_isom_remove_pssh_box(GF_ISOFile *the_file);

Bool gf_isom_is_adobe_protection_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex);
GF_Err gf_isom_get_adobe_protection_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, const char **outMetadata);
GF_Err gf_isom_set_adobe_protection(GF_ISOFile *the_file, u32 trackNumber, u32 desc_index, u32 scheme_type, u32 scheme_version, Bool is_selective_enc, char *metadata, u32 len);

void gf_isom_ipmpx_remove_tool_list(GF_ISOFile *the_file);

#endif /*GPAC_DISABLE_ISOM_WRITE*/

Bool gf_isom_is_cenc_media(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex);
GF_Err gf_isom_get_cenc_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *outOriginalFormat, u32 *outSchemeType, u32 *outSchemeVersion, u32 *outIVLength);

void gf_isom_cenc_samp_aux_info_del(GF_CENCSampleAuxInfo *samp_aux_info);

/*container_type is type of box which contains the sample auxiliary information. Now we have two type: GF_ISOM_BOX_UUID_PSEC and GF_ISOM_BOX_TYPE_SENC*/
GF_Err gf_isom_cenc_get_sample_aux_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, GF_CENCSampleAuxInfo **sai, u32 *container_type);

/*container_type is type of box which contains the sample auxiliary information. Now we have two type: GF_ISOM_BOX_UUID_PSEC and GF_ISOM_BOX_TYPE_SENC
alloc/realloc output buffer, containing IV on IV_size bytes, then subsample count if any an [clear_bytes(u16), crypt_bytes(u32)] subsamples*/
GF_Err gf_isom_cenc_get_sample_aux_info_buffer(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *container_type, u8 **out_buffer, u32 *outSize);

void gf_isom_cenc_get_default_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex, u32 *container_type, Bool *default_IsEncrypted, u8 *default_IV_size, bin128 *default_KID, u8 *constant_IV_size, bin128 *constant_IV, u8 *crypt_byte_block, u8 *skip_byte_block);

Bool gf_isom_cenc_is_pattern_mode(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex);

u32 gf_isom_get_pssh_count(GF_ISOFile *file);
/*index is 1-based, all pointers shall not be free*/
GF_Err gf_isom_get_pssh_info(GF_ISOFile *file, u32 pssh_index, bin128 SystemID, u32 *version, u32 *KID_count, const bin128 **KIDs, const u8 **private_data, u32 *private_data_size);

GF_Err gf_isom_get_pssh(GF_ISOFile *file, u32 pssh_index, u8 **pssh_data, u32 *pssh_size);


#ifndef GPAC_DISABLE_ISOM_DUMP
/*xml dumpers*/
GF_Err gf_isom_dump_ismacryp_protection(GF_ISOFile *the_file, u32 trackNumber, FILE * trace);
GF_Err gf_isom_dump_ismacryp_sample(GF_ISOFile *the_file, u32 trackNumber, u32 SampleNum, FILE *trace);
#endif


/********************************************************************
				GENERAL META API FUNCTIONS

	  Meta can be stored at several places in the file layout:
		* root level (like moov, ftyp and co)
		* moov level
		* track level
	Meta API uses the following parameters for all functions:

	 gf_isom_*_meta_*(GF_ISOFile *file, Bool root_meta, u32 track_num, ....) with:
		@root_meta: if set, accesses file root meta
		@track_num: if root_meta not set, specifies whether the target meta is at the
			moov level (track_num=0) or at the track level.

********************************************************************/

/*gets meta type. Returned value: 0 if no meta found, or four char code of meta (eg, "mp21", "smil", ...)*/
u32 gf_isom_get_meta_type(GF_ISOFile *file, Bool root_meta, u32 track_num);

/*indicates if the meta has an XML container (note that XML data can also be included as items).
return value: 0 (no XML or error), 1 (XML text), 2 (BinaryXML, eg BiM) */
u32 gf_isom_has_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num);

/*extracts XML (if any) from given meta
	@outName: output file path and location for writing
	@is_binary: indicates if XML is Bim or regular XML
*/
GF_Err gf_isom_extract_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num, char *outName, Bool *is_binary);

/*returns number of items described in this meta*/
u32 gf_isom_get_meta_item_count(GF_ISOFile *file, Bool root_meta, u32 track_num);

/*gets item info for the given item
	@item_num: 1-based index of item to query
	@itemID (optional): item ID in file
	@type: item 4CC type
	@is_self_reference: item is the file itself
	@item_name (optional): item name
	@item_mime_type (optional): item mime type
	@item_encoding (optional): item content encoding type
	@item_url, @item_urn (optional): url/urn of external resource containing this item data if any.
		When item is fully contained in file, these are set to NULL

*/
GF_Err gf_isom_get_meta_item_info(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_num,
                                  u32 *itemID, u32 *type, u32 *protection_idx, Bool *is_self_reference,
                                  const char **item_name, const char **item_mime_type, const char **item_encoding,
                                  const char **item_url, const char **item_urn);


/*gets item idx from item ID*/
u32 gf_isom_get_meta_item_by_id(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_ID);

/*extracts item from given meta
	@item_num: 1-based index of item to query
	@dump_file_name: if NULL, use item name for dumping
*/
GF_Err gf_isom_extract_meta_item(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_num, const char *dump_file_name);

/*extracts item from given meta in memory
	@item_num: 1-based index of item to query
*/
GF_Err gf_isom_extract_meta_item_mem(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, u8 **out_data, u32 *out_size, u32 *out_alloc_size, const char **mime_type, Bool use_annex_b);

/*retirves primary item ID, 0 if none found (primary can also be stored through meta XML)*/
u32 gf_isom_get_meta_primary_item_id(GF_ISOFile *file, Bool root_meta, u32 track_num);

#ifndef GPAC_DISABLE_ISOM_WRITE

/*sets meta type (four char int, eg "mp21", ...
	Creates a meta box if none found
	if metaType is 0, REMOVES META
*/
GF_Err gf_isom_set_meta_type(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 metaType);

/*removes meta XML info if any*/
GF_Err gf_isom_remove_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num);

/*set meta XML data from file - erase any previously (Binary)XML info*/
GF_Err gf_isom_set_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num, char *XMLFileName, Bool IsBinaryXML);
/*set meta XML data from memory - erase any previously (Binary)XML info*/
GF_Err gf_isom_set_meta_xml_memory(GF_ISOFile *file, Bool root_meta, u32 track_num, u8 *data, u32 data_size, Bool IsBinaryXML);

typedef enum {
	TILE_ITEM_NONE = 0,
	TILE_ITEM_ALL_NO_BASE,
	TILE_ITEM_ALL_BASE,
	TILE_ITEM_ALL_GRID,
	TILE_ITEM_SINGLE
} GF_TileItemMode;

typedef struct
{
	u32 width, height;
	u32 hSpacing, vSpacing;
	u32 hOffset, vOffset;
	u32 angle;
	Bool hidden;
	void *config;
	GF_TileItemMode tile_mode;
	u32 single_tile_number;
	double time;
	char iccPath[GF_MAX_PATH];
	Bool alpha;
	u8 num_channels;
	u8 bits_per_channel[3];
} GF_ImageItemProperties;

GF_Err gf_isom_meta_get_next_item_id(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 *item_id);

GF_Err gf_isom_add_meta_item(GF_ISOFile *file, Bool root_meta, u32 track_num, Bool self_reference, char *resource_path, const char *item_name, u32 item_id, u32 item_type, const char *mime_type, const char *content_encoding, const char *URL, const char *URN, GF_ImageItemProperties *imgprop);
/*same as above excepts take the item directly in memory*/
GF_Err gf_isom_add_meta_item_memory(GF_ISOFile *file, Bool root_meta, u32 track_num, const char *item_name, u32 item_id, u32 item_type, const char *mime_type, const char *content_encoding, GF_ImageItemProperties *image_props, char *data, u32 data_len, GF_List *item_extent_refs);

GF_Err gf_isom_iff_create_image_item_from_track(GF_ISOFile *movie, Bool root_meta, u32 meta_track_number, u32 track, const char *item_name, u32 item_id, GF_ImageItemProperties *image_props, GF_List *item_extent_refs);

/*removes item from meta*/
GF_Err gf_isom_remove_meta_item(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_num);

/*sets the given item as the primary one. You SHALL NOT use this if the meta has a valid XML data*/
GF_Err gf_isom_set_meta_primary_item(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_num);

GF_Err gf_isom_meta_add_item_ref(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 from_id, u32 to_id, u32 type, u64 *ref_index);

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Err gf_isom_get_meta_image_props(GF_ISOFile *file, Bool root_meta, u32 track_num, u32 item_id, GF_ImageItemProperties *prop);


/********************************************************************
				Timed Meta-Data extensions
********************************************************************/

GF_Err gf_isom_get_xml_metadata_description(GF_ISOFile *file, u32 track, u32 sampleDescription, const char **_namespace, const char **schema_loc, const char **content_encoding);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*create a new timed metat data sample description for this track*/
GF_Err gf_isom_new_xml_metadata_description(GF_ISOFile *movie, u32 trackNumber, const char *_namespace, const char *schema_loc, const char *content_encoding, u32 *outDescriptionIndex);
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/********************************************************************
				iTunes info tags
********************************************************************/
enum
{
	/*probe is only used to check if iTunes info are present*/
	GF_ISOM_ITUNE_PROBE = 0,
	/*all is only used to remove all tags*/
	GF_ISOM_ITUNE_ALL = 1,
	GF_ISOM_ITUNE_ALBUM	= GF_4CC( 0xA9, 'a', 'l', 'b' ),
	GF_ISOM_ITUNE_ARTIST = GF_4CC( 0xA9, 'A', 'R', 'T' ),
	GF_ISOM_ITUNE_COMMENT = GF_4CC( 0xA9, 'c', 'm', 't' ),
	GF_ISOM_ITUNE_COMPILATION = GF_4CC( 'c', 'p', 'i', 'l' ),
	GF_ISOM_ITUNE_COMPOSER = GF_4CC( 0xA9, 'c', 'o', 'm' ),
	GF_ISOM_ITUNE_COVER_ART = GF_4CC( 'c', 'o', 'v', 'r' ),
	GF_ISOM_ITUNE_CREATED = GF_4CC( 0xA9, 'd', 'a', 'y' ),
	GF_ISOM_ITUNE_DISK = GF_4CC( 'd', 'i', 's', 'k' ),
	GF_ISOM_ITUNE_TOOL = GF_4CC( 0xA9, 't', 'o', 'o' ),
	GF_ISOM_ITUNE_GENRE = GF_4CC( 'g', 'n', 'r', 'e' ),
	GF_ISOM_ITUNE_GROUP = GF_4CC( 0xA9, 'g', 'r', 'p' ),
	GF_ISOM_ITUNE_ITUNES_DATA = GF_4CC( '-', '-', '-', '-' ),
	GF_ISOM_ITUNE_NAME = GF_4CC( 0xA9, 'n', 'a', 'm' ),
	GF_ISOM_ITUNE_TEMPO = GF_4CC( 't', 'm', 'p', 'o' ),
	GF_ISOM_ITUNE_TRACK = GF_4CC( 0xA9, 't', 'r', 'k' ),
	GF_ISOM_ITUNE_TRACKNUMBER = GF_4CC( 't', 'r', 'k', 'n' ),
	GF_ISOM_ITUNE_WRITER = GF_4CC( 0xA9, 'w', 'r', 't' ),
	GF_ISOM_ITUNE_ENCODER = GF_4CC( 0xA9, 'e', 'n', 'c' ),
	GF_ISOM_ITUNE_ALBUM_ARTIST = GF_4CC( 'a', 'A', 'R', 'T' ),
	GF_ISOM_ITUNE_GAPLESS = GF_4CC( 'p', 'g', 'a', 'p' ),
};
/*get the given tag info.
!! 'genre' may be coded by ID, the libisomedia doesn't translate the ID. In such a case, the result data is set to NULL
and the data_len to the genre ID
returns GF_URL_ERROR if no tag is present in the file
*/
GF_Err gf_isom_apple_get_tag(GF_ISOFile *mov, u32 tag, const u8 **data, u32 *data_len);
#ifndef GPAC_DISABLE_ISOM_WRITE
/*set the given tag info. If data and data_len are 0, removes the given tag
For 'genre', data may be NULL in which case the genre ID taken from the data_len parameter
*/
GF_Err gf_isom_apple_set_tag(GF_ISOFile *mov, u32 tag, const u8 *data, u32 data_len);

/*sets compatibility tag on AVC tracks (needed by iPod to play files... hurray for standards)*/
GF_Err gf_isom_set_ipod_compatible(GF_ISOFile *the_file, u32 trackNumber);
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/*3GPP Alternate Group API - (c) 2007 Telecom ParisTech*/

/*gets the number of switching groups declared in this track if any:
trackNumber: track number
alternateGroupID: alternate group id of track if speciifed, 0 otherwise
nb_groups: number of switching groups defined for this track
*/
GF_Err gf_isom_get_track_switch_group_count(GF_ISOFile *movie, u32 trackNumber, u32 *alternateGroupID, u32 *nb_groups);

/*returns the list of criteria (expressed as 4CC IDs, cf 3GPP TS 26.244)
trackNumber: track number
group_index: 1-based index of the group to inspect
switchGroupID: ID of the switch group if any, 0 otherwise (alternate-only group)
criteriaListSize: number of criteria items in returned list
*/
const u32 *gf_isom_get_track_switch_parameter(GF_ISOFile *movie, u32 trackNumber, u32 group_index, u32 *switchGroupID, u32 *criteriaListSize);

#ifndef GPAC_DISABLE_ISOM_WRITE
/*sets a new (switch) group for this track
trackNumber: track
trackRefGroup: number of a track belonging to the same alternate group. If 0, a new alternate group will be created for this track
is_switch_group: if set, indicates that a switch group identifier shall be assigned to the created group. Otherwise, the criteria list is associated with the entire alternate group
switchGroupID: SHALL NOT BE NULL
	input: specifies the desired switchGroupID to use. If value is 0, next available switchGroupID in file is used.
	output: indicates the switchGroupID used.
criteriaList, criteriaListCount: criteria list and size. Criterias are expressed as 4CC IDs, cf 3GPP TS 26.244
*/
GF_Err gf_isom_set_track_switch_parameter(GF_ISOFile *movie, u32 trackNumber, u32 trackRefGroup, Bool is_switch_group, u32 *switchGroupID, u32 *criteriaList, u32 criteriaListCount);

/*resets track switch group information for the track or for the entire alternate group this track belongs to if reset_all_group is set*/
GF_Err gf_isom_reset_track_switch_parameter(GF_ISOFile *movie, u32 trackNumber, Bool reset_all_group);

/*resets ALL track switch group information in the entire movie*/
GF_Err gf_isom_reset_switch_parameters(GF_ISOFile *movie);

#endif /*GPAC_DISABLE_ISOM_WRITE*/


typedef struct
{
	u8 profile;
	u8 level;
	u8 pathComponents;
	Bool fullRequestHost;
	Bool streamType;
	u8 containsRedundant;
	const char *textEncoding;
	const char *contentEncoding;
	const char *content_script_types;
	const char *mime_type;
	const char *xml_schema_loc;
} GF_DIMSDescription;

GF_Err gf_isom_get_dims_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, GF_DIMSDescription *desc);
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err gf_isom_new_dims_description(GF_ISOFile *movie, u32 trackNumber, GF_DIMSDescription *desc, char *URLname, char *URNname, u32 *outDescriptionIndex);
GF_Err gf_isom_update_dims_description(GF_ISOFile *movie, u32 trackNumber, GF_DIMSDescription *desc, char *URLname, char *URNname, u32 DescriptionIndex);
#endif /*GPAC_DISABLE_ISOM_WRITE*/



struct __ec3_stream
{
	u8 fscod;
	u8 bsid;
	u8 bsmod;
	u8 acmod;
	u8 lfon;
	/*only for EC3*/
	u8 nb_dep_sub;
	u8 chan_loc;
};

/*AC3 config record*/
typedef struct
{
	u8 is_ec3;
	u8 nb_streams; //1 for AC3, max 8 for EC3
	u16 brcode; //if AC3 is bitrate code, otherwise cumulated datarate of EC3 streams
	struct __ec3_stream streams[8];
} GF_AC3Config;

GF_AC3Config *gf_isom_ac3_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex);


#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err gf_isom_ac3_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_AC3Config *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex);
#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_Err gf_isom_flac_config_get(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, u8 **dsi, u32 *dsi_size);

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err gf_isom_flac_config_new(GF_ISOFile *the_file, u32 trackNumber, u8 *metadata, u32 metadata_size, char *URLname, char *URNname, u32 *outDescriptionIndex);
#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err gf_isom_new_mj2k_description(GF_ISOFile *the_file, u32 trackNumber, char *URLname, char *URNname, u32 *outDescriptionIndex, u8 *dsi, u32 dsi_len);
#endif /*GPAC_DISABLE_ISOM_WRITE*/


/*returns the number of subsamples in the given sample for the given flags*/
u32 gf_isom_sample_has_subsamples(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 flags);
GF_Err gf_isom_sample_get_subsample(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 flags, u32 subSampleNumber, u32 *size, u8 *priority, u32 *reserved, Bool *discardable);
#ifndef GPAC_DISABLE_ISOM_WRITE
/*adds subsample information to a given sample. Subsample information shall be added in increasing order of sampleNumbers, insertion of information is not supported.
Note that you may add subsample information for samples not yet added to the file
specifying 0 as subSampleSize will remove the last subsample information if any*/
GF_Err gf_isom_add_subsample(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 flags, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable);
#endif
/*add subsample information for the latest sample added to the current track fragment*/
GF_Err gf_isom_fragment_add_subsample(GF_ISOFile *movie, GF_ISOTrackID TrackID, u32 flags, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable);

/*gets the number of the next moof to be produced*/
u32 gf_isom_get_next_moof_number(GF_ISOFile *movie);
/*Sets the number of the next moof to be produced*/
void gf_isom_set_next_moof_number(GF_ISOFile *movie, u32 value);

GF_Err gf_isom_set_sample_group_in_traf(GF_ISOFile *file);

/*returns 'rap ' and 'roll' group info for the given sample*/
GF_Err gf_isom_get_sample_rap_roll_info(GF_ISOFile *the_file, u32 trackNumber, u32 sample_number, Bool *is_rap, Bool *has_roll, s32 *roll_distance);

/*returns opaque data of sample group*/
Bool gf_isom_get_sample_group_info(GF_ISOFile *the_file, u32 trackNumber, u32 sample_description_index, u32 grouping_type, u32 *default_index, const u8 **data, u32 *size);

Bool gf_isom_has_cenc_sample_group(GF_ISOFile *the_file, u32 trackNumber);

/*returns tile info */
Bool gf_isom_get_tile_info(GF_ISOFile *file, u32 trackNumber, u32 sample_description_index, u32 *default_sample_group_index, u32 *id, u32 *independent, Bool *full_frame, u32 *x, u32 *y, u32 *w, u32 *h);

/*sample groups information*/
#ifndef GPAC_DISABLE_ISOM_WRITE
/*sets rap flag for sample_number - this is used by non-IDR RAPs in AVC (also in USAC) were SYNC flag (stss table) cannot be used
num_leading_sample is the number of samples to after this RAP that have dependences on samples before this RAP and hence should be discarded
- currently sample group info MUST be added in order (no insertion in the tables)*/
GF_Err gf_isom_set_sample_rap_group(GF_ISOFile *movie, u32 track, u32 sample_number, Bool is_rap, u32 num_leading_samples);
/*sets roll_distance info for sample_number (number of frames before (<0) or after (>0) this sample to have a complete refresh of the decoded data (used by GDR in AVC)
- currently sample group info MUST be added in order (no insertion in the tables)*/
GF_Err gf_isom_set_sample_roll_group(GF_ISOFile *movie, u32 track, u32 sample_number, Bool is_roll, s16 roll_distance);

/*set encryption group for a sample_number; see GF_CENCSampleEncryptionGroupEntry for the parameters*/
GF_Err gf_isom_set_sample_cenc_group(GF_ISOFile *movie, u32 track, u32 sample_number, u8 isEncrypted, u8 IV_size, bin128 KeyID,
									u8 crypt_byte_block, u8 skip_byte_block, u8 constant_IV_size, bin128 constant_IV);

GF_Err gf_isom_set_sample_cenc_default_group(GF_ISOFile *movie, u32 track, u32 sample_number);

GF_Err gf_isom_set_composition_offset_mode(GF_ISOFile *file, u32 track, Bool use_negative_offsets);

GF_Err gf_isom_set_ctts_v1(GF_ISOFile *file, u32 track, u32 ctts_shift);

//adds the given blob as a sample group description of the given grouping type. If default is set, the sample grouping will be marked as default.
//sampleGroupDescriptionIndex is optional, used to retrieve the index
GF_Err gf_isom_add_sample_group_info(GF_ISOFile *movie, u32 track, u32 grouping_type, void *data, u32 data_size, Bool is_default, u32 *sampleGroupDescriptionIndex);

//remove a sample group description of the give grouping type (if found)
GF_Err gf_isom_remove_sample_group(GF_ISOFile *movie, u32 track, u32 grouping_type);

//tags the sample in the grouping adds the given blob as a sample group description of the given grouping type. If default is set, the sample grouping will be marked as default
GF_Err gf_isom_add_sample_info(GF_ISOFile *movie, u32 track, u32 sample_number, u32 grouping_type, u32 sampleGroupDescriptionIndex, u32 grouping_type_parameter);


//sets track in group of type group_type and id track_group_id. If do_add is GF_FALSE, track is removed from that group
GF_Err gf_isom_set_track_group(GF_ISOFile *file, u32 track_number, u32 track_group_id, u32 group_type, Bool do_add);

#endif

GF_Err gf_isom_get_sample_cenc_info(GF_ISOFile *movie, u32 track, u32 sample_number, Bool *IsEncrypted, u8 *IV_size, bin128 *KID,
									u8 *crypt_byte_block, u8 *skip_byte_block, u8 *constant_IV_size, bin128 *constant_IV);


GF_Err gf_isom_get_text_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, GF_TextSampleDescriptor **out_desc);

#endif /*GPAC_DISABLE_ISOM*/

/*! @} */


#ifdef __cplusplus
}
#endif


#endif	/*_GF_ISOMEDIA_H_*/


