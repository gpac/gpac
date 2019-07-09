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

#ifndef _GF_ISOMEDIA_DEV_H_
#define _GF_ISOMEDIA_DEV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/isomedia.h>


enum
{
	//internal code type for unknown boxes
	GF_ISOM_BOX_TYPE_UNKNOWN = GF_4CC( 'U', 'N', 'K', 'N' ),

	GF_ISOM_BOX_TYPE_CO64	= GF_4CC( 'c', 'o', '6', '4' ),
	GF_ISOM_BOX_TYPE_STCO	= GF_4CC( 's', 't', 'c', 'o' ),
	GF_ISOM_BOX_TYPE_CTTS	= GF_4CC( 'c', 't', 't', 's' ),
	GF_ISOM_BOX_TYPE_CPRT	= GF_4CC( 'c', 'p', 'r', 't' ),
	GF_ISOM_BOX_TYPE_KIND	= GF_4CC( 'k', 'i', 'n', 'd' ),
	GF_ISOM_BOX_TYPE_CHPL	= GF_4CC( 'c', 'h', 'p', 'l' ),
	GF_ISOM_BOX_TYPE_URL	= GF_4CC( 'u', 'r', 'l', ' ' ),
	GF_ISOM_BOX_TYPE_URN	= GF_4CC( 'u', 'r', 'n', ' ' ),
	GF_ISOM_BOX_TYPE_DINF	= GF_4CC( 'd', 'i', 'n', 'f' ),
	GF_ISOM_BOX_TYPE_DREF	= GF_4CC( 'd', 'r', 'e', 'f' ),
	GF_ISOM_BOX_TYPE_STDP	= GF_4CC( 's', 't', 'd', 'p' ),
	GF_ISOM_BOX_TYPE_EDTS	= GF_4CC( 'e', 'd', 't', 's' ),
	GF_ISOM_BOX_TYPE_ELST	= GF_4CC( 'e', 'l', 's', 't' ),
	GF_ISOM_BOX_TYPE_UUID	= GF_4CC( 'u', 'u', 'i', 'd' ),
	GF_ISOM_BOX_TYPE_FREE	= GF_4CC( 'f', 'r', 'e', 'e' ),
	GF_ISOM_BOX_TYPE_HDLR	= GF_4CC( 'h', 'd', 'l', 'r' ),
	GF_ISOM_BOX_TYPE_GMHD	= GF_4CC( 'g', 'm', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_HMHD	= GF_4CC( 'h', 'm', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_HINT	= GF_4CC( 'h', 'i', 'n', 't' ),
	GF_ISOM_BOX_TYPE_MDIA	= GF_4CC( 'm', 'd', 'i', 'a' ),
	GF_ISOM_BOX_TYPE_ELNG	= GF_4CC( 'e', 'l', 'n', 'g' ),
	GF_ISOM_BOX_TYPE_MDAT	= GF_4CC( 'm', 'd', 'a', 't' ),
	GF_ISOM_BOX_TYPE_IDAT	= GF_4CC( 'i', 'd', 'a', 't' ),
	GF_ISOM_BOX_TYPE_MDHD	= GF_4CC( 'm', 'd', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_MINF	= GF_4CC( 'm', 'i', 'n', 'f' ),
	GF_ISOM_BOX_TYPE_MOOV	= GF_4CC( 'm', 'o', 'o', 'v' ),
	GF_ISOM_BOX_TYPE_MVHD	= GF_4CC( 'm', 'v', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_STSD	= GF_4CC( 's', 't', 's', 'd' ),
	GF_ISOM_BOX_TYPE_STSZ	= GF_4CC( 's', 't', 's', 'z' ),
	GF_ISOM_BOX_TYPE_STZ2	= GF_4CC( 's', 't', 'z', '2' ),
	GF_ISOM_BOX_TYPE_STBL	= GF_4CC( 's', 't', 'b', 'l' ),
	GF_ISOM_BOX_TYPE_STSC	= GF_4CC( 's', 't', 's', 'c' ),
	GF_ISOM_BOX_TYPE_STSH	= GF_4CC( 's', 't', 's', 'h' ),
	GF_ISOM_BOX_TYPE_SKIP	= GF_4CC( 's', 'k', 'i', 'p' ),
	GF_ISOM_BOX_TYPE_SMHD	= GF_4CC( 's', 'm', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_STSS	= GF_4CC( 's', 't', 's', 's' ),
	GF_ISOM_BOX_TYPE_STTS	= GF_4CC( 's', 't', 't', 's' ),
	GF_ISOM_BOX_TYPE_TRAK	= GF_4CC( 't', 'r', 'a', 'k' ),
	GF_ISOM_BOX_TYPE_TKHD	= GF_4CC( 't', 'k', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_TREF	= GF_4CC( 't', 'r', 'e', 'f' ),
	GF_ISOM_BOX_TYPE_STRK	= GF_4CC( 's', 't', 'r', 'k' ),
	GF_ISOM_BOX_TYPE_STRI	= GF_4CC( 's', 't', 'r', 'i' ),
	GF_ISOM_BOX_TYPE_STRD	= GF_4CC( 's', 't', 'r', 'd' ),
	GF_ISOM_BOX_TYPE_STSG	= GF_4CC( 's', 't', 's', 'g' ),

	GF_ISOM_BOX_TYPE_UDTA	= GF_4CC( 'u', 'd', 't', 'a' ),
	GF_ISOM_BOX_TYPE_VMHD	= GF_4CC( 'v', 'm', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_FTYP	= GF_4CC( 'f', 't', 'y', 'p' ),
	GF_ISOM_BOX_TYPE_PADB	= GF_4CC( 'p', 'a', 'd', 'b' ),
	GF_ISOM_BOX_TYPE_PDIN	= GF_4CC( 'p', 'd', 'i', 'n' ),
	GF_ISOM_BOX_TYPE_SDTP	= GF_4CC( 's', 'd', 't', 'p' ),
	GF_ISOM_BOX_TYPE_CSLG	= GF_4CC( 'c', 's', 'l', 'g' ),

	GF_ISOM_BOX_TYPE_SBGP	= GF_4CC( 's', 'b', 'g', 'p' ),
	GF_ISOM_BOX_TYPE_SGPD	= GF_4CC( 's', 'g', 'p', 'd' ),
	GF_ISOM_BOX_TYPE_SAIZ	= GF_4CC( 's', 'a', 'i', 'z' ),
	GF_ISOM_BOX_TYPE_SAIO	= GF_4CC( 's', 'a', 'i', 'o' ),
	GF_ISOM_BOX_TYPE_MFRA	= GF_4CC( 'm', 'f', 'r', 'a' ),
	GF_ISOM_BOX_TYPE_MFRO	= GF_4CC( 'm', 'f', 'r', 'o' ),
	GF_ISOM_BOX_TYPE_TFRA	= GF_4CC( 't', 'f', 'r', 'a' ),

	GF_ISOM_BOX_TYPE_TENC	= GF_4CC( 't', 'e', 'n', 'c' ),

	//track group
	GF_ISOM_BOX_TYPE_TRGR	= GF_4CC( 't', 'r', 'g', 'r' ),
	//track group types
	GF_ISOM_BOX_TYPE_TRGT	= GF_4CC( 't', 'r', 'g', 't' ),
	GF_ISOM_BOX_TYPE_MSRC	= GF_4CC( 'm', 's', 'r', 'c' ),
	GF_ISOM_BOX_TYPE_CSTG	= GF_4CC( 'c', 's', 't', 'g' ),
	GF_ISOM_BOX_TYPE_STER	= GF_4CC( 's', 't', 'e', 'r' ),

	/*Adobe's protection boxes*/
	GF_ISOM_BOX_TYPE_ADKM	= GF_4CC( 'a', 'd', 'k', 'm' ),
	GF_ISOM_BOX_TYPE_AHDR	= GF_4CC( 'a', 'h', 'd', 'r' ),
	GF_ISOM_BOX_TYPE_ADAF	= GF_4CC( 'a', 'd', 'a', 'f' ),
	GF_ISOM_BOX_TYPE_APRM	= GF_4CC( 'a', 'p', 'r', 'm' ),
	GF_ISOM_BOX_TYPE_AEIB	= GF_4CC( 'a', 'e', 'i', 'b' ),
	GF_ISOM_BOX_TYPE_AKEY	= GF_4CC( 'a', 'k', 'e', 'y' ),
	GF_ISOM_BOX_TYPE_FLXS	= GF_4CC( 'f', 'l', 'x', 's' ),

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	/*Movie Fragments*/
	GF_ISOM_BOX_TYPE_MVEX	= GF_4CC( 'm', 'v', 'e', 'x' ),
	GF_ISOM_BOX_TYPE_MEHD	= GF_4CC( 'm', 'e', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_TREX	= GF_4CC( 't', 'r', 'e', 'x' ),
	GF_ISOM_BOX_TYPE_TREP	= GF_4CC( 't', 'r', 'e', 'p' ),
	GF_ISOM_BOX_TYPE_MOOF	= GF_4CC( 'm', 'o', 'o', 'f' ),
	GF_ISOM_BOX_TYPE_MFHD	= GF_4CC( 'm', 'f', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_TRAF	= GF_4CC( 't', 'r', 'a', 'f' ),
	GF_ISOM_BOX_TYPE_TFHD	= GF_4CC( 't', 'f', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_TRUN	= GF_4CC( 't', 'r', 'u', 'n' ),
#endif


	/*MP4 extensions*/
	GF_ISOM_BOX_TYPE_DPND	= GF_4CC( 'd', 'p', 'n', 'd' ),
	GF_ISOM_BOX_TYPE_IODS	= GF_4CC( 'i', 'o', 'd', 's' ),
	GF_ISOM_BOX_TYPE_ESDS	= GF_4CC( 'e', 's', 'd', 's' ),
	GF_ISOM_BOX_TYPE_MPOD	= GF_4CC( 'm', 'p', 'o', 'd' ),
	GF_ISOM_BOX_TYPE_SYNC	= GF_4CC( 's', 'y', 'n', 'c' ),
	GF_ISOM_BOX_TYPE_IPIR	= GF_4CC( 'i', 'p', 'i', 'r' ),

	GF_ISOM_BOX_TYPE_NMHD	= GF_4CC( 'n', 'm', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_STHD	= GF_4CC( 's', 't', 'h', 'd' ),
	/*reseved
	GF_ISOM_BOX_TYPE_SDHD	= GF_4CC( 's', 'd', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_ODHD	= GF_4CC( 'o', 'd', 'h', 'd' ),
	GF_ISOM_BOX_TYPE_CRHD	= GF_4CC( 'c', 'r', 'h', 'd' ),
	*/
	GF_ISOM_BOX_TYPE_MP4S	= GF_4CC( 'm', 'p', '4', 's' ),
	GF_ISOM_BOX_TYPE_MP4A	= GF_4CC( 'm', 'p', '4', 'a' ),
	GF_ISOM_BOX_TYPE_MP4V	= GF_4CC( 'm', 'p', '4', 'v' ),


	/*AVC / H264 extension*/
	GF_ISOM_BOX_TYPE_AVCC	= GF_4CC( 'a', 'v', 'c', 'C' ),
	GF_ISOM_BOX_TYPE_BTRT	= GF_4CC( 'b', 't', 'r', 't' ),
	GF_ISOM_BOX_TYPE_M4DS	= GF_4CC( 'm', '4', 'd', 's' ),
	GF_ISOM_BOX_TYPE_PASP	= GF_4CC( 'p', 'a', 's', 'p' ),
	GF_ISOM_BOX_TYPE_CLAP	= GF_4CC( 'c', 'l', 'a', 'p' ),
	GF_ISOM_BOX_TYPE_AVC1	= GF_4CC( 'a', 'v', 'c', '1' ),
	GF_ISOM_BOX_TYPE_AVC2	= GF_4CC( 'a', 'v', 'c', '2' ),
	GF_ISOM_BOX_TYPE_AVC3	= GF_4CC( 'a', 'v', 'c', '3' ),
	GF_ISOM_BOX_TYPE_AVC4	= GF_4CC( 'a', 'v', 'c', '4' ),
	GF_ISOM_BOX_TYPE_SVCC	= GF_4CC( 's', 'v', 'c', 'C' ),
	GF_ISOM_BOX_TYPE_SVC1	= GF_4CC( 's', 'v', 'c', '1' ),
	GF_ISOM_BOX_TYPE_SVC2	= GF_4CC( 's', 'v', 'c', '2' ),
	GF_ISOM_BOX_TYPE_MVCC	= GF_4CC( 'm', 'v', 'c', 'C' ),
	GF_ISOM_BOX_TYPE_MVC1	= GF_4CC( 'm', 'v', 'c', '1' ),
	GF_ISOM_BOX_TYPE_MVC2	= GF_4CC( 'm', 'v', 'c', '2' ),
	GF_ISOM_BOX_TYPE_MHC1	= GF_4CC( 'm', 'h', 'c', '1' ),
	GF_ISOM_BOX_TYPE_MHV1	= GF_4CC( 'm', 'h', 'v', '1' ),

	GF_ISOM_BOX_TYPE_HVCC	= GF_4CC( 'h', 'v', 'c', 'C' ),
	GF_ISOM_BOX_TYPE_HVC1	= GF_4CC( 'h', 'v', 'c', '1' ),
	GF_ISOM_BOX_TYPE_HEV1	= GF_4CC( 'h', 'e', 'v', '1' ),
	GF_ISOM_BOX_TYPE_HVT1	= GF_4CC( 'h', 'v', 't', '1' ),
	GF_ISOM_BOX_TYPE_MVCI	= GF_4CC( 'm', 'v', 'c', 'i' ),
	GF_ISOM_BOX_TYPE_MVCG	= GF_4CC( 'm', 'v', 'c', 'g' ),
	GF_ISOM_BOX_TYPE_VWID	= GF_4CC( 'v', 'w', 'i', 'd' ),

	GF_ISOM_BOX_TYPE_HVC2	= GF_4CC( 'h', 'v', 'c', '2' ),
	GF_ISOM_BOX_TYPE_HEV2	= GF_4CC( 'h', 'e', 'v', '2' ),
	GF_ISOM_BOX_TYPE_LHV1	= GF_4CC( 'l', 'h', 'v', '1' ),
	GF_ISOM_BOX_TYPE_LHE1	= GF_4CC( 'l', 'h', 'e', '1' ),
	GF_ISOM_BOX_TYPE_LHT1	= GF_4CC( 'l', 'h', 't', '1' ),

	GF_ISOM_BOX_TYPE_LHVC	= GF_4CC( 'l', 'h', 'v', 'C' ),

	GF_ISOM_BOX_TYPE_AV1C = GF_4CC('a', 'v', '1', 'C'),
	GF_ISOM_BOX_TYPE_AV01 = GF_4CC('a', 'v', '0', '1'),

	/*WebM*/
	GF_ISOM_BOX_TYPE_VPCC = GF_4CC('v', 'p', 'c', 'C'),
	GF_ISOM_BOX_TYPE_VP08 = GF_4CC('v', 'p', '0', '8'),
	GF_ISOM_BOX_TYPE_VP09 = GF_4CC('v', 'p', '0', '9'),
	GF_ISOM_BOX_TYPE_SMDM = GF_4CC('S', 'm', 'D', 'm'),
	GF_ISOM_BOX_TYPE_COLL = GF_4CC('C', 'o', 'L', 'L'),

	/*Opus*/
	GF_ISOM_BOX_TYPE_OPUS = GF_4CC('O', 'p', 'u', 's'),
	GF_ISOM_BOX_TYPE_DOPS = GF_4CC('d', 'O', 'p', 's'),

	/*LASeR extension*/
	GF_ISOM_BOX_TYPE_LSRC	= GF_4CC( 'l', 's', 'r', 'C' ),
	GF_ISOM_BOX_TYPE_LSR1	= GF_4CC( 'l', 's', 'r', '1' ),

	/*3GPP extensions*/
	GF_ISOM_BOX_TYPE_DAMR	= GF_4CC( 'd', 'a', 'm', 'r' ),
	GF_ISOM_BOX_TYPE_D263	= GF_4CC( 'd', '2', '6', '3' ),
	GF_ISOM_BOX_TYPE_DEVC	= GF_4CC( 'd', 'e', 'v', 'c' ),
	GF_ISOM_BOX_TYPE_DQCP	= GF_4CC( 'd', 'q', 'c', 'p' ),
	GF_ISOM_BOX_TYPE_DSMV	= GF_4CC( 'd', 's', 'm', 'v' ),
	GF_ISOM_BOX_TYPE_TSEL	= GF_4CC( 't', 's', 'e', 'l' ),

	/* 3GPP Adaptive Streaming extensions */
	GF_ISOM_BOX_TYPE_STYP	= GF_4CC( 's', 't', 'y', 'p' ),
	GF_ISOM_BOX_TYPE_TFDT	= GF_4CC( 't', 'f', 'd', 't' ),
	GF_ISOM_BOX_TYPE_SIDX	= GF_4CC( 's', 'i', 'd', 'x' ),
	GF_ISOM_BOX_TYPE_SSIX	= GF_4CC( 's', 's', 'i', 'x' ),
	GF_ISOM_BOX_TYPE_LEVA   = GF_4CC( 'l', 'e', 'v', 'a' ),
	GF_ISOM_BOX_TYPE_PCRB	= GF_4CC( 'p', 'c', 'r', 'b' ),
	GF_ISOM_BOX_TYPE_EMSG	= GF_4CC( 'e', 'm', 's', 'g' ),

	/*3GPP text / MPEG-4 StreamingText*/
	GF_ISOM_BOX_TYPE_FTAB	= GF_4CC( 'f', 't', 'a', 'b' ),
	GF_ISOM_BOX_TYPE_TX3G	= GF_4CC( 't', 'x', '3', 'g' ),
	GF_ISOM_BOX_TYPE_STYL	= GF_4CC( 's', 't', 'y', 'l' ),
	GF_ISOM_BOX_TYPE_HLIT	= GF_4CC( 'h', 'l', 'i', 't' ),
	GF_ISOM_BOX_TYPE_HCLR	= GF_4CC( 'h', 'c', 'l', 'r' ),
	GF_ISOM_BOX_TYPE_KROK	= GF_4CC( 'k', 'r', 'o', 'k' ),
	GF_ISOM_BOX_TYPE_DLAY	= GF_4CC( 'd', 'l', 'a', 'y' ),
	GF_ISOM_BOX_TYPE_HREF	= GF_4CC( 'h', 'r', 'e', 'f' ),
	GF_ISOM_BOX_TYPE_TBOX	= GF_4CC( 't', 'b', 'o', 'x' ),
	GF_ISOM_BOX_TYPE_BLNK	= GF_4CC( 'b', 'l', 'n', 'k' ),
	GF_ISOM_BOX_TYPE_TWRP	= GF_4CC( 't', 'w', 'r', 'p' ),

	/* ISO Base Media File Format Extensions for MPEG-21 */
	GF_ISOM_BOX_TYPE_META	= GF_4CC( 'm', 'e', 't', 'a' ),
	GF_ISOM_BOX_TYPE_XML	= GF_4CC( 'x', 'm', 'l', ' ' ),
	GF_ISOM_BOX_TYPE_BXML	= GF_4CC( 'b', 'x', 'm', 'l' ),
	GF_ISOM_BOX_TYPE_ILOC	= GF_4CC( 'i', 'l', 'o', 'c' ),
	GF_ISOM_BOX_TYPE_PITM	= GF_4CC( 'p', 'i', 't', 'm' ),
	GF_ISOM_BOX_TYPE_IPRO	= GF_4CC( 'i', 'p', 'r', 'o' ),
	GF_ISOM_BOX_TYPE_INFE	= GF_4CC( 'i', 'n', 'f', 'e' ),
	GF_ISOM_BOX_TYPE_IINF	= GF_4CC( 'i', 'i', 'n', 'f' ),
	GF_ISOM_BOX_TYPE_IREF	= GF_4CC( 'i', 'r', 'e', 'f' ),
	GF_ISOM_BOX_TYPE_ENCA	= GF_4CC( 'e', 'n', 'c', 'a' ),
	GF_ISOM_BOX_TYPE_ENCV	= GF_4CC( 'e', 'n', 'c', 'v' ),
	GF_ISOM_BOX_TYPE_RESV	= GF_4CC( 'r', 'e', 's', 'v' ),
	GF_ISOM_BOX_TYPE_ENCT	= GF_4CC( 'e', 'n', 'c', 't' ),
	GF_ISOM_BOX_TYPE_ENCS	= GF_4CC( 'e', 'n', 'c', 's' ),
	GF_ISOM_BOX_TYPE_ENCF	= GF_4CC( 'e', 'n', 'c', 'f' ),
	GF_ISOM_BOX_TYPE_ENCM	= GF_4CC( 'e', 'n', 'c', 'm' ),
	GF_ISOM_BOX_TYPE_SINF	= GF_4CC( 's', 'i', 'n', 'f' ),
	GF_ISOM_BOX_TYPE_RINF	= GF_4CC( 'r', 'i', 'n', 'f' ),
	GF_ISOM_BOX_TYPE_FRMA	= GF_4CC( 'f', 'r', 'm', 'a' ),
	GF_ISOM_BOX_TYPE_SCHM	= GF_4CC( 's', 'c', 'h', 'm' ),
	GF_ISOM_BOX_TYPE_SCHI	= GF_4CC( 's', 'c', 'h', 'i' ),

	GF_ISOM_BOX_TYPE_STVI	= GF_4CC( 's', 't', 'v', 'i' ),


	GF_ISOM_BOX_TYPE_METX	= GF_4CC( 'm', 'e', 't', 'x' ),
	GF_ISOM_BOX_TYPE_METT	= GF_4CC( 'm', 'e', 't', 't' ),

	/* ISMA 1.0 Encryption and Authentication V 1.0 */
	GF_ISOM_BOX_TYPE_IKMS	= GF_4CC( 'i', 'K', 'M', 'S' ),
	GF_ISOM_BOX_TYPE_ISFM	= GF_4CC( 'i', 'S', 'F', 'M' ),
	GF_ISOM_BOX_TYPE_ISLT	= GF_4CC( 'i', 'S', 'L', 'T' ),

	/* Hinting boxes */
	GF_ISOM_BOX_TYPE_RTP_STSD	= GF_4CC( 'r', 't', 'p', ' ' ),
	GF_ISOM_BOX_TYPE_SRTP_STSD	= GF_4CC( 's', 'r', 't', 'p' ),
	GF_ISOM_BOX_TYPE_FDP_STSD	= GF_4CC( 'f', 'd', 'p', ' ' ),
	GF_ISOM_BOX_TYPE_RRTP_STSD	= GF_4CC( 'r', 'r', 't', 'p' ),
	GF_ISOM_BOX_TYPE_RTCP_STSD	= GF_4CC( 'r', 't', 'c', 'p' ),
	GF_ISOM_BOX_TYPE_HNTI	= GF_4CC( 'h', 'n', 't', 'i' ),
	GF_ISOM_BOX_TYPE_RTP	= GF_4CC( 'r', 't', 'p', ' ' ),
	GF_ISOM_BOX_TYPE_SDP	= GF_4CC( 's', 'd', 'p', ' ' ),
	GF_ISOM_BOX_TYPE_HINF	= GF_4CC( 'h', 'i', 'n', 'f' ),
	GF_ISOM_BOX_TYPE_NAME	= GF_4CC( 'n', 'a', 'm', 'e' ),
	GF_ISOM_BOX_TYPE_TRPY	= GF_4CC( 't', 'r', 'p', 'y' ),
	GF_ISOM_BOX_TYPE_NUMP	= GF_4CC( 'n', 'u', 'm', 'p' ),
	GF_ISOM_BOX_TYPE_TOTL	= GF_4CC( 't', 'o', 't', 'l' ),
	GF_ISOM_BOX_TYPE_NPCK	= GF_4CC( 'n', 'p', 'c', 'k' ),
	GF_ISOM_BOX_TYPE_TPYL	= GF_4CC( 't', 'p', 'y', 'l' ),
	GF_ISOM_BOX_TYPE_TPAY	= GF_4CC( 't', 'p', 'a', 'y' ),
	GF_ISOM_BOX_TYPE_MAXR	= GF_4CC( 'm', 'a', 'x', 'r' ),
	GF_ISOM_BOX_TYPE_DMED	= GF_4CC( 'd', 'm', 'e', 'd' ),
	GF_ISOM_BOX_TYPE_DIMM	= GF_4CC( 'd', 'i', 'm', 'm' ),
	GF_ISOM_BOX_TYPE_DREP	= GF_4CC( 'd', 'r', 'e', 'p' ),
	GF_ISOM_BOX_TYPE_TMIN	= GF_4CC( 't', 'm', 'i', 'n' ),
	GF_ISOM_BOX_TYPE_TMAX	= GF_4CC( 't', 'm', 'a', 'x' ),
	GF_ISOM_BOX_TYPE_PMAX	= GF_4CC( 'p', 'm', 'a', 'x' ),
	GF_ISOM_BOX_TYPE_DMAX	= GF_4CC( 'd', 'm', 'a', 'x' ),
	GF_ISOM_BOX_TYPE_PAYT	= GF_4CC( 'p', 'a', 'y', 't' ),
	GF_ISOM_BOX_TYPE_RELY	= GF_4CC( 'r', 'e', 'l', 'y' ),
	GF_ISOM_BOX_TYPE_TIMS	= GF_4CC( 't', 'i', 'm', 's' ),
	GF_ISOM_BOX_TYPE_TSRO	= GF_4CC( 't', 's', 'r', 'o' ),
	GF_ISOM_BOX_TYPE_SNRO	= GF_4CC( 's', 'n', 'r', 'o' ),
	GF_ISOM_BOX_TYPE_RTPO	= GF_4CC( 'r', 't', 'p', 'o' ),
	GF_ISOM_BOX_TYPE_TSSY	= GF_4CC( 't', 's', 's', 'y' ),
	GF_ISOM_BOX_TYPE_RSSR	= GF_4CC( 'r', 's', 's', 'r' ),
	GF_ISOM_BOX_TYPE_SRPP	= GF_4CC( 's', 'r', 'p', 'p' ),

	//FEC boxes
	GF_ISOM_BOX_TYPE_FIIN	= GF_4CC( 'f', 'i', 'i', 'n' ),
	GF_ISOM_BOX_TYPE_PAEN	= GF_4CC( 'p', 'a', 'e', 'n' ),
	GF_ISOM_BOX_TYPE_FPAR	= GF_4CC( 'f', 'p', 'a', 'r' ),
	GF_ISOM_BOX_TYPE_FECR	= GF_4CC( 'f', 'e', 'c', 'r' ),
	GF_ISOM_BOX_TYPE_SEGR	= GF_4CC( 's', 'e', 'g', 'r' ),
	GF_ISOM_BOX_TYPE_GITN	= GF_4CC( 'g', 'i', 't', 'n' ),
	GF_ISOM_BOX_TYPE_FIRE	= GF_4CC( 'f', 'i', 'r', 'e' ),
	GF_ISOM_BOX_TYPE_FDSA	= GF_4CC( 'f', 'd', 's', 'a' ),
	GF_ISOM_BOX_TYPE_FDPA	= GF_4CC( 'f', 'd', 'p', 'a' ),
	GF_ISOM_BOX_TYPE_EXTR	= GF_4CC( 'e', 'x', 't', 'r' ),

	/*internal type for track and item references*/
	GF_ISOM_BOX_TYPE_REFT	= GF_4CC( 'R', 'E', 'F', 'T' ),
	GF_ISOM_BOX_TYPE_REFI	= GF_4CC( 'R', 'E', 'F', 'I'),
	GF_ISOM_BOX_TYPE_GRPT	= GF_4CC( 'G', 'R', 'P', 'T'),

#ifndef GPAC_DISABLE_ISOM_ADOBE
	/* Adobe extensions */
	GF_ISOM_BOX_TYPE_ABST	= GF_4CC( 'a', 'b', 's', 't' ),
	GF_ISOM_BOX_TYPE_AFRA	= GF_4CC( 'a', 'f', 'r', 'a' ),
	GF_ISOM_BOX_TYPE_ASRT	= GF_4CC( 'a', 's', 'r', 't' ),
	GF_ISOM_BOX_TYPE_AFRT	= GF_4CC( 'a', 'f', 'r', 't' ),
#endif

	/* Apple extensions */

	GF_ISOM_BOX_TYPE_ILST	= GF_4CC( 'i', 'l', 's', 't' ),
	GF_ISOM_BOX_TYPE_0xA9NAM	= GF_4CC( 0xA9, 'n', 'a', 'm' ),
	GF_ISOM_BOX_TYPE_0xA9CMT	= GF_4CC( 0xA9, 'c', 'm', 't' ),
	GF_ISOM_BOX_TYPE_0xA9DAY	= GF_4CC( 0xA9, 'd', 'a', 'y' ),
	GF_ISOM_BOX_TYPE_0xA9ART	= GF_4CC( 0xA9, 'A', 'R', 'T' ),
	GF_ISOM_BOX_TYPE_0xA9TRK	= GF_4CC( 0xA9, 't', 'r', 'k' ),
	GF_ISOM_BOX_TYPE_0xA9ALB	= GF_4CC( 0xA9, 'a', 'l', 'b' ),
	GF_ISOM_BOX_TYPE_0xA9COM	= GF_4CC( 0xA9, 'c', 'o', 'm' ),
	GF_ISOM_BOX_TYPE_0xA9WRT	= GF_4CC( 0xA9, 'w', 'r', 't' ),
	GF_ISOM_BOX_TYPE_0xA9TOO	= GF_4CC( 0xA9, 't', 'o', 'o' ),
	GF_ISOM_BOX_TYPE_0xA9CPY	= GF_4CC( 0xA9, 'c', 'p', 'y' ),
	GF_ISOM_BOX_TYPE_0xA9DES	= GF_4CC( 0xA9, 'd', 'e', 's' ),
	GF_ISOM_BOX_TYPE_0xA9GEN	= GF_4CC( 0xA9, 'g', 'e', 'n' ),
	GF_ISOM_BOX_TYPE_0xA9GRP	= GF_4CC( 0xA9, 'g', 'r', 'p' ),
	GF_ISOM_BOX_TYPE_0xA9ENC	= GF_4CC( 0xA9, 'e', 'n', 'c' ),
	GF_ISOM_BOX_TYPE_aART		= GF_4CC( 'a', 'A', 'R', 'T' ),
	GF_ISOM_BOX_TYPE_PGAP = GF_4CC( 'p', 'g', 'a', 'p' ),
	GF_ISOM_BOX_TYPE_GNRE	= GF_4CC( 'g', 'n', 'r', 'e' ),
	GF_ISOM_BOX_TYPE_DISK	= GF_4CC( 'd', 'i', 's', 'k' ),
	GF_ISOM_BOX_TYPE_TRKN	= GF_4CC( 't', 'r', 'k', 'n' ),
	GF_ISOM_BOX_TYPE_TMPO	= GF_4CC( 't', 'm', 'p', 'o' ),
	GF_ISOM_BOX_TYPE_CPIL	= GF_4CC( 'c', 'p', 'i', 'l' ),
	GF_ISOM_BOX_TYPE_COVR	= GF_4CC( 'c', 'o', 'v', 'r' ),
	GF_ISOM_BOX_TYPE_iTunesSpecificInfo	= GF_4CC( '-', '-', '-', '-' ),
	GF_ISOM_BOX_TYPE_DATA	= GF_4CC( 'd', 'a', 't', 'a' ),

	GF_ISOM_HANDLER_TYPE_MDIR	= GF_4CC( 'm', 'd', 'i', 'r' ),
	GF_ISOM_BOX_TYPE_CHAP	= GF_4CC( 'c', 'h', 'a', 'p' ),
	GF_ISOM_BOX_TYPE_TEXT	= GF_4CC( 't', 'e', 'x', 't' ),

	/*OMA (P)DCF boxes*/
	GF_ISOM_BOX_TYPE_OHDR	= GF_4CC( 'o', 'h', 'd', 'r' ),
	GF_ISOM_BOX_TYPE_GRPI	= GF_4CC( 'g', 'r', 'p', 'i' ),
	GF_ISOM_BOX_TYPE_MDRI	= GF_4CC( 'm', 'd', 'r', 'i' ),
	GF_ISOM_BOX_TYPE_ODTT	= GF_4CC( 'o', 'd', 't', 't' ),
	GF_ISOM_BOX_TYPE_ODRB	= GF_4CC( 'o', 'd', 'r', 'b' ),
	GF_ISOM_BOX_TYPE_ODKM	= GF_4CC( 'o', 'd', 'k', 'm' ),
	GF_ISOM_BOX_TYPE_ODAF	= GF_4CC( 'o', 'd', 'a', 'f' ),

	/*3GPP DIMS */
	GF_ISOM_BOX_TYPE_DIMS	= GF_4CC( 'd', 'i', 'm', 's' ),
	GF_ISOM_BOX_TYPE_DIMC	= GF_4CC( 'd', 'i', 'm', 'C' ),
	GF_ISOM_BOX_TYPE_DIST	= GF_4CC( 'd', 'i', 'S', 'T' ),


	GF_ISOM_BOX_TYPE_AC3	= GF_4CC( 'a', 'c', '-', '3' ),
	GF_ISOM_BOX_TYPE_DAC3	= GF_4CC( 'd', 'a', 'c', '3' ),
	GF_ISOM_BOX_TYPE_EC3	= GF_4CC( 'e', 'c', '-', '3' ),
	GF_ISOM_BOX_TYPE_DEC3	= GF_4CC( 'd', 'e', 'c', '3' ),
	GF_ISOM_BOX_TYPE_DVCC	= GF_4CC( 'd', 'v', 'c', 'C' ),
	GF_ISOM_BOX_TYPE_DVHE	= GF_4CC( 'd', 'v', 'h', 'e' ),

	GF_ISOM_BOX_TYPE_SUBS	= GF_4CC( 's', 'u', 'b', 's' ),

	GF_ISOM_BOX_TYPE_RVCC	= GF_4CC( 'r', 'v', 'c', 'c' ),

	GF_ISOM_BOX_TYPE_VTTC_CONFIG	= GF_4CC( 'v', 't', 't', 'C' ),
	GF_ISOM_BOX_TYPE_VTCC_CUE	= GF_4CC( 'v', 't', 't', 'c' ),
	GF_ISOM_BOX_TYPE_VTTE	= GF_4CC( 'v', 't', 't', 'e' ),
	GF_ISOM_BOX_TYPE_VTTA	= GF_4CC( 'v', 't', 't', 'a' ),
	GF_ISOM_BOX_TYPE_CTIM	= GF_4CC( 'c', 't', 'i', 'm' ),
	GF_ISOM_BOX_TYPE_IDEN	= GF_4CC( 'i', 'd', 'e', 'n' ),
	GF_ISOM_BOX_TYPE_STTG	= GF_4CC( 's', 't', 't', 'g' ),
	GF_ISOM_BOX_TYPE_PAYL	= GF_4CC( 'p', 'a', 'y', 'l' ),
	GF_ISOM_BOX_TYPE_WVTT	= GF_4CC( 'w', 'v', 't', 't' ),

	GF_ISOM_BOX_TYPE_STPP	= GF_4CC( 's', 't', 'p', 'p' ),
	GF_ISOM_BOX_TYPE_SBTT	= GF_4CC( 's', 'b', 't', 't' ),

	GF_ISOM_BOX_TYPE_STXT	= GF_4CC( 's', 't', 'x', 't' ),
	GF_ISOM_BOX_TYPE_TXTC	= GF_4CC( 't', 'x', 't', 'C' ),

	GF_ISOM_BOX_TYPE_PRFT   = GF_4CC( 'p', 'r', 'f', 't' ),

	/* Image File Format Boxes */
	GF_ISOM_BOX_TYPE_ISPE   = GF_4CC( 'i', 's', 'p', 'e' ),
	GF_ISOM_BOX_TYPE_COLR   = GF_4CC( 'c', 'o', 'l', 'r' ),
	GF_ISOM_BOX_TYPE_PIXI   = GF_4CC( 'p', 'i', 'x', 'i' ),
	GF_ISOM_BOX_TYPE_RLOC   = GF_4CC( 'r', 'l', 'o', 'c' ),
	GF_ISOM_BOX_TYPE_IROT   = GF_4CC( 'i', 'r', 'o', 't' ),
	GF_ISOM_BOX_TYPE_IPCO   = GF_4CC( 'i', 'p', 'c', 'o' ),
	GF_ISOM_BOX_TYPE_IPRP   = GF_4CC( 'i', 'p', 'r', 'p' ),
	GF_ISOM_BOX_TYPE_IPMA   = GF_4CC( 'i', 'p', 'm', 'a' ),
	GF_ISOM_BOX_TYPE_GRPL   = GF_4CC( 'g', 'r', 'p', 'l'),
	GF_ISOM_BOX_TYPE_CCST	= GF_4CC( 'c', 'c', 's', 't' ),
	GF_ISOM_BOX_TYPE_AUXC	= GF_4CC( 'a', 'u', 'x', 'C' ),
	GF_ISOM_BOX_TYPE_AUXI	= GF_4CC( 'a', 'u', 'x', 'i' ),
	GF_ISOM_BOX_TYPE_OINF	= GF_4CC( 'o', 'i', 'n', 'f' ),
	GF_ISOM_BOX_TYPE_TOLS	= GF_4CC( 't', 'o', 'l', 's' ),

	/* MIAF Boxes */
	GF_ISOM_BOX_TYPE_CLLI	= GF_4CC('c', 'l', 'l', 'i'),
	GF_ISOM_BOX_TYPE_MDCV	= GF_4CC('m', 'd', 'c', 'v'),

	GF_ISOM_BOX_TYPE_ALTR	= GF_4CC( 'a', 'l', 't', 'r' ),

	/*ALL INTERNAL BOXES - NEVER WRITTEN TO FILE!!*/

	/*generic handlers*/
	GF_ISOM_BOX_TYPE_GNRM	= GF_4CC( 'G', 'N', 'R', 'M' ),
	GF_ISOM_BOX_TYPE_GNRV	= GF_4CC( 'G', 'N', 'R', 'V' ),
	GF_ISOM_BOX_TYPE_GNRA	= GF_4CC( 'G', 'N', 'R', 'A' ),
	/*base constructor of all hint formats (currently only RTP uses it)*/
	GF_ISOM_BOX_TYPE_GHNT	= GF_4CC( 'g', 'h', 'n', 't' ),
	/*for compatibility with old files hinted for DSS - needs special parsing*/
	GF_ISOM_BOX_TYPE_VOID	= GF_4CC( 'V', 'O', 'I', 'D' ),

	/*MS Smooth - these are actually UUID boxes*/
	GF_ISOM_BOX_UUID_PSSH	= GF_4CC( 'P', 'S', 'S', 'H' ),
	GF_ISOM_BOX_UUID_MSSM   = GF_4CC( 'M', 'S', 'S', 'M' ), /*Stream Manifest box*/
	GF_ISOM_BOX_UUID_TENC	= GF_4CC( 'T', 'E', 'N', 'C' ),
	GF_ISOM_BOX_UUID_TFRF	= GF_4CC( 'T', 'F', 'R', 'F' ),
	GF_ISOM_BOX_UUID_TFXD	= GF_4CC( 'T', 'F', 'X', 'D' ),

	GF_ISOM_BOX_TYPE_MP3	= GF_4CC( '.', 'm', 'p', '3' ),

	GF_ISOM_BOX_TYPE_TRIK	= GF_4CC( 't', 'r', 'i', 'k' ),
	GF_ISOM_BOX_TYPE_BLOC	= GF_4CC( 'b', 'l', 'o', 'c' ),
	GF_ISOM_BOX_TYPE_AINF	= GF_4CC( 'a', 'i', 'n', 'f' ),

	/*JPEG2000*/
	GF_ISOM_BOX_TYPE_MJP2	= GF_4CC('m','j','p','2'),
	GF_ISOM_BOX_TYPE_IHDR	= GF_4CC('i','h','d','r'),
	GF_ISOM_BOX_TYPE_JP  	= GF_4CC('j','P',' ',' '),
	GF_ISOM_BOX_TYPE_JP2H	= GF_4CC('j','p','2','h'),
	GF_ISOM_BOX_TYPE_JP2K	= GF_4CC('j','p','2','k'),

	GF_ISOM_BOX_TYPE_JPEG	= GF_4CC('j','p','e','g'),
	GF_ISOM_BOX_TYPE_PNG 	= GF_4CC('p','n','g',' '),

	/* apple QT box */
	GF_QT_BOX_TYPE_ALIS = GF_4CC('a','l','i','s'),
	GF_QT_BOX_TYPE_WIDE = GF_4CC('w','i','d','e'),
	GF_QT_BOX_TYPE_GMIN	= GF_4CC( 'g', 'm', 'i', 'n' ),
	GF_QT_BOX_TYPE_TAPT	= GF_4CC( 't', 'a', 'p', 't' ),
	GF_QT_BOX_TYPE_CLEF	= GF_4CC( 'c', 'l', 'e', 'f' ),
	GF_QT_BOX_TYPE_PROF	= GF_4CC( 'p', 'r', 'o', 'f' ),
	GF_QT_BOX_TYPE_ENOF	= GF_4CC( 'e', 'n', 'o', 'f' ),
	GF_QT_BOX_TYPE_WAVE = GF_4CC('w','a','v','e'),
	GF_QT_BOX_TYPE_CHAN = GF_4CC('c','h','a','n'),
	GF_QT_BOX_TYPE_TERMINATOR 	= 0,
	GF_QT_BOX_TYPE_ENDA = GF_4CC('e','n','d','a'),
	GF_QT_BOX_TYPE_FRMA = GF_4CC('f','r','m','a'),
	GF_QT_BOX_TYPE_TMCD = GF_4CC('t','m','c','d'),
	GF_QT_BOX_TYPE_NAME = GF_4CC('n','a','m','e'),
	GF_QT_BOX_TYPE_TCMI = GF_4CC('t','c','m','i'),
	GF_QT_BOX_TYPE_FIEL = GF_4CC('f','i','e','l'),
	GF_QT_BOX_TYPE_GAMA = GF_4CC('g','a','m','a'),
	GF_QT_BOX_TYPE_CHRM = GF_4CC('c','h','r','m'),

	/* from drm_sample.c */
	GF_ISOM_BOX_TYPE_264B 	= GF_4CC('2','6','4','b'),
	GF_ISOM_BOX_TYPE_265B 	= GF_4CC('2','6','5','b'),
	GF_ISOM_BOX_TYPE_AV1B 	= GF_4CC('a','v','1','b'),
	GF_ISOM_BOX_TYPE_VP9B   = GF_4CC('v','p','9','b'),

	/*MPEG-H 3D audio*/
	GF_ISOM_BOX_TYPE_MHA1 	= GF_4CC('m','h','a','1'),
	GF_ISOM_BOX_TYPE_MHA2 	= GF_4CC('m','h','a','2'),
	GF_ISOM_BOX_TYPE_MHM1 	= GF_4CC('m','h','m','1'),
	GF_ISOM_BOX_TYPE_MHM2 	= GF_4CC('m','h','m','2'),
	GF_ISOM_BOX_TYPE_MHAC 	= GF_4CC('m','h','a','C'),


	GF_ISOM_BOX_TYPE_AUXV 	= GF_4CC('A','U','X','V'),

	GF_ISOM_BOX_TYPE_FLAC	= GF_4CC( 'f', 'L', 'a', 'C' ),
	GF_ISOM_BOX_TYPE_DFLA	= GF_4CC( 'd', 'f', 'L', 'a' ),

};

enum
{
	GF_ISOM_SAMPLE_ENTRY_UNKN = 0,
	GF_ISOM_SAMPLE_ENTRY_VIDEO = GF_4CC('v','i','d','e'),
	GF_ISOM_SAMPLE_ENTRY_AUDIO = GF_4CC('a','u','d','i')
};

#ifndef GPAC_DISABLE_ISOM


#if defined(GPAC_DISABLE_ISOM_FRAGMENTS) && !defined(GPAC_DISABLE_ISOM_ADOBE)
#define GPAC_DISABLE_ISOM_ADOBE
#endif

//internal flags (up to 16)
//if flag is set, position checking of child boxes is ignored
#define GF_ISOM_ORDER_FREEZE 1

	/*the default size is 64, cause we need to handle large boxes...

	the child_boxes container is by default NOT created. When parsing a box and sub-boxes are detected, the list is created.
	This list is destroyed before calling the final box destructor
	This list is automatically taken into account during size() and write() functions, and
	gf_isom_box_write shall not be called in XXXX_box_write on any box registered with the child list

	the full box version field is moved in the base box since we also need some internal flags, as u16 to
	also be used for audio and video sample entries version field
	*/
#define GF_ISOM_BOX			\
	u32 type;				\
	u64 size;				\
	const struct box_registry_entry *registry;\
	GF_List *child_boxes; 	\
	u16 internal_flags;		\
	u16 version;				\

#define GF_ISOM_FULL_BOX		\
	GF_ISOM_BOX			\
	u32 flags;			\

#define GF_ISOM_UUID_BOX	\
	GF_ISOM_BOX			\
	u8 uuid[16];		\
	u32 internal_4cc;		\

typedef struct
{
	GF_ISOM_BOX
} GF_Box;

typedef struct
{
	GF_ISOM_FULL_BOX
} GF_FullBox;

typedef struct
{
	GF_ISOM_UUID_BOX
} GF_UUIDBox;


#define ISOM_DECL_BOX_ALLOC(__TYPE, __4cc)	__TYPE *tmp; \
	GF_SAFEALLOC(tmp, __TYPE);	\
	if (tmp==NULL) return NULL;	\
	tmp->type = __4cc;

#define ISOM_DECREASE_SIZE(__ptr, bytes)	if (__ptr->size < (bytes) ) {\
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isom] not enough bytes in box %s: %d left, reading %d (file %s, line %d)\n", gf_4cc_to_str(__ptr->type), __ptr->size, (bytes), __FILE__, __LINE__ )); \
			return GF_ISOM_INVALID_FILE; \
		}\
		__ptr->size -= bytes; \

/*constructor*/
GF_Box *gf_isom_box_new(u32 boxType);
//some boxes may have different syntax based on container. Use this constructor for this case
GF_Box *gf_isom_box_new_ex(u32 boxType, u32 parentType, Bool skip_logs, Bool is_root_box);

GF_Err gf_isom_box_write(GF_Box *ptr, GF_BitStream *bs);
GF_Err gf_isom_box_read(GF_Box *ptr, GF_BitStream *bs);
void gf_isom_box_del(GF_Box *ptr);
GF_Err gf_isom_box_size(GF_Box *ptr);

GF_Err gf_isom_clone_box(GF_Box *src, GF_Box **dst);

GF_Err gf_isom_box_parse(GF_Box **outBox, GF_BitStream *bs);
GF_Err gf_isom_box_array_read(GF_Box *s, GF_BitStream *bs, GF_Err (*check_child_box)(GF_Box *par, GF_Box *b));
GF_Err gf_isom_box_array_read_ex(GF_Box *parent, GF_BitStream *bs, GF_Err (*check_child_box)(GF_Box *par, GF_Box *b), u32 parent_type);

GF_Err gf_isom_box_parse_ex(GF_Box **outBox, GF_BitStream *bs, u32 parent_type, Bool is_root_box);

//writes box header - shall be called at the beginning of each xxxx_Write function
//this function is not factorized in order to let box serializer modify box type before writing
GF_Err gf_isom_box_write_header(GF_Box *ptr, GF_BitStream *bs);

//writes box header then version+flags
GF_Err gf_isom_full_box_write(GF_Box *s, GF_BitStream *bs);

void gf_isom_box_array_del(GF_List *other_boxes);
GF_Err gf_isom_box_array_write(GF_Box *parent, GF_List *list, GF_BitStream *bs);
GF_Err gf_isom_box_array_size(GF_Box *parent, GF_List *list);

void gf_isom_check_position(GF_Box *s, GF_Box *child, u32 *pos);
void gf_isom_check_position_list(GF_Box *s, GF_List *childlist, u32 *pos);

Bool gf_box_valid_in_parent(GF_Box *a, const char *parent_4cc);

void gf_isom_box_array_del_parent(GF_List **other_boxes, GF_List *boxlist);

void gf_isom_box_freeze_order(GF_Box *box);


typedef struct
{
	GF_ISOM_BOX
	/*note: the data is NEVER loaded to the mdat in this lib*/
	u64 dataSize;
	/* store the file offset when parsing to access the raw data */
	u64 bsOffset;
	u8 *data;
} GF_MediaDataBox;

typedef struct
{
  u64 time;
  u64 moof_offset;
  u32 traf_number;
  u32 trun_number;
  u32 sample_number;
} GF_RandomAccessEntry;

typedef struct
{
  GF_ISOM_FULL_BOX
  GF_ISOTrackID track_id;
  u8 traf_bits;
  u8 trun_bits;
  u8 sample_bits;
  u32 nb_entries;
  GF_RandomAccessEntry *entries;
} GF_TrackFragmentRandomAccessBox;

typedef struct
{
  GF_ISOM_FULL_BOX
	u32 container_size;
} GF_MovieFragmentRandomAccessOffsetBox;

typedef struct
{
  GF_ISOM_BOX
  GF_List* tfra_list;
  GF_MovieFragmentRandomAccessOffsetBox *mfro;
} GF_MovieFragmentRandomAccessBox;

typedef struct
{
	GF_ISOM_BOX
	u8 *data;
	u32 dataSize;
	u32 original_4cc;
} GF_UnknownBox;

typedef struct
{
	GF_ISOM_UUID_BOX
	u8 *data;
	u32 dataSize;
} GF_UnknownUUIDBox;

u32 gf_isom_solve_uuid_box(u8 *UUID);

typedef struct
{
	GF_ISOM_FULL_BOX
	u64 creationTime;
	u64 modificationTime;
	u32 timeScale;
	u64 duration;
	u64 original_duration;
	GF_ISOTrackID nextTrackID;
	u32 preferredRate;
	u16 preferredVolume;
	char reserved[10];
	u32 matrixA;
	u32 matrixB;
	u32 matrixU;
	u32 matrixC;
	u32 matrixD;
	u32 matrixV;
	u32 matrixW;
	u32 matrixX;
	u32 matrixY;
	u32 previewTime;
	u32 previewDuration;
	u32 posterTime;
	u32 selectionTime;
	u32 selectionDuration;
	u32 currentTime;
} GF_MovieHeaderBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_Descriptor *descriptor;
} GF_ObjectDescriptorBox;

/*used for entry list*/
typedef struct
{
	u64 segmentDuration;
	s64 mediaTime;
	u32 mediaRate;
	Bool was_empty_dur;
} GF_EdtsEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_List *entryList;
} GF_EditListBox;

typedef struct
{
	GF_ISOM_BOX
	GF_EditListBox *editList;
} GF_EditBox;


/*used to classify boxes in the UserData GF_Box*/
typedef struct
{
	u32 boxType;
	u8 uuid[16];
	GF_List *boxes;
} GF_UserDataMap;

typedef struct
{
	GF_ISOM_BOX
	GF_List *recordList;
} GF_UserDataBox;

typedef struct
{
	GF_ISOM_BOX
	GF_MovieHeaderBox *mvhd;
	GF_ObjectDescriptorBox *iods;
	GF_UserDataBox *udta;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	struct __tag_mvex_box *mvex;
#endif
	/*meta box if any*/
	struct __tag_meta_box *meta;
	/*track boxes*/
	GF_List *trackList;

	GF_ISOFile *mov;

	Bool mvex_after_traks;

} GF_MovieBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u64 creationTime;
	u64 modificationTime;
	GF_ISOTrackID trackID;
	u32 reserved1;
	u64 duration;
	u32 reserved2[2];
	u16 layer;
	u16 alternate_group;
	u16 volume;
	u16 reserved3;
	s32 matrix[9];
	u32 width, height;
} GF_TrackHeaderBox;

typedef struct
{
	GF_ISOM_BOX
} GF_TrackReferenceBox;


typedef struct {
	GF_ISOM_BOX
	GF_List *groups;
} GF_TrackGroupBox;

typedef struct {
	GF_ISOM_FULL_BOX
	u32 group_type;
	u32 track_group_id;
} GF_TrackGroupTypeBox;

typedef struct
{
	GF_ISOM_BOX
	GF_UserDataBox *udta;
	GF_TrackHeaderBox *Header;
	struct __tag_media_box *Media;
	GF_EditBox *editBox;
	GF_TrackReferenceBox *References;
	/*meta box if any*/
	struct __tag_meta_box *meta;
	GF_TrackGroupBox *groups;

	GF_MovieBox *moov;
	/*private for media padding*/
	u32 padding_bytes;
	/*private for editing*/
	Bool is_unpacked;
	/*private for checking dependency*/
	u32 originalFile;
	u32 originalID;

	//not sure about piff (not supposed to be stored in moov), but senc is in track according to CENC
	struct __sample_encryption_box *sample_encryption;

	/*private for SVC/MVC extractors resolution*/
	s32 extractor_mode;
	Bool has_base_layer;
	u32 pack_num_samples;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	u64 dts_at_seg_start;
	u32 sample_count_at_seg_start;
	Bool first_traf_merged;
	Bool present_in_scalable_segment;
#endif
} GF_TrackBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u64 creationTime;
	u64 modificationTime;
	u32 timeScale;
	u64 duration, original_duration;
	char packedLanguage[4];
	u16 reserved;
} GF_MediaHeaderBox;


typedef struct
{
	GF_ISOM_FULL_BOX
	u32 reserved1;
	u32 handlerType;
	u8 reserved2[12];
	char *nameUTF8;
	Bool store_counted_string;
} GF_HandlerBox;

typedef struct __tag_media_box
{
	GF_ISOM_BOX
	GF_TrackBox *mediaTrack;
	GF_MediaHeaderBox *mediaHeader;
	GF_HandlerBox *handler;
	struct __tag_media_info_box *information;
	u64 BytesMissing;

	//all the following are only used for NALU-based tracks
	//NALU reader
	GF_BitStream *nalu_parser;

	GF_BitStream *nalu_out_bs;
	GF_BitStream *nalu_ps_bs;
	u8 *in_sample_buffer;
	u32 in_sample_buffer_alloc;
	u8 *tmp_nal_copy_buffer;
	u32 tmp_nal_copy_buffer_alloc;

	GF_ISOSample *extracted_samp;
	GF_BitStream *extracted_bs;

} GF_MediaBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	char *extended_language;
} GF_ExtendedLanguageBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u64 reserved;
} GF_VideoMediaHeaderBox;

typedef struct
{
	GF_ISOM_FULL_BOX

	u16 graphics_mode;
	u16 op_color_red;
	u16 op_color_green;
	u16 op_color_blue;
	u16 balance;
	u16 reserved;
} GF_GenericMediaHeaderInfoBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u16 balance;
	u16 reserved;
} GF_SoundMediaHeaderBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	/*this is used for us INTERNALLY*/
	u32 subType;
	u32 maxPDUSize;
	u32 avgPDUSize;
	u32 maxBitrate;
	u32 avgBitrate;
	u32 slidingAverageBitrate;
} GF_HintMediaHeaderBox;

typedef struct
{
	GF_ISOM_FULL_BOX
} GF_MPEGMediaHeaderBox;

typedef struct
{
	GF_ISOM_FULL_BOX
} GF_SubtitleMediaHeaderBox;

typedef struct
{
	GF_ISOM_FULL_BOX
} GF_ODMediaHeaderBox;

typedef struct
{
	GF_ISOM_FULL_BOX
} GF_OCRMediaHeaderBox;

typedef struct
{
	GF_ISOM_FULL_BOX
} GF_SceneMediaHeaderBox;


typedef struct
{
	GF_ISOM_FULL_BOX

	u32 width;
	u32 height;
} GF_ApertureBox;

typedef struct
{
	GF_ISOM_FULL_BOX
} GF_DataReferenceBox;

typedef struct
{
	GF_ISOM_BOX
	GF_DataReferenceBox *dref;
} GF_DataInformationBox;

#define GF_ISOM_DATAENTRY_FIELDS	\
	char *location;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_ISOM_DATAENTRY_FIELDS
} GF_DataEntryBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_ISOM_DATAENTRY_FIELDS
} GF_DataEntryURLBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_ISOM_DATAENTRY_FIELDS
} GF_DataEntryAliasBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_ISOM_DATAENTRY_FIELDS
	char *nameURN;
} GF_DataEntryURNBox;

typedef struct
{
	u32 sampleCount;
	u32 sampleDelta;
} GF_SttsEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_SttsEntry *entries;
	u32 nb_entries, alloc_size;

#ifndef GPAC_DISABLE_ISOM_WRITE
	/*cache for WRITE*/
	u32 w_currentSampleNum;
	u64 w_LastDTS;
#endif
	/*cache for READ*/
	u32 r_FirstSampleInEntry;
	u32 r_currentEntryIndex;
	u64 r_CurrentDTS;

	//stats for read
	u32 max_ts_delta;
} GF_TimeToSampleBox;


/*TO CHECK - it could be reasonnable to only use 16bits for both count and offset*/
typedef struct
{
	u32 sampleCount;
	s32 decodingOffset;
} GF_DttsEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_DttsEntry *entries;
	u32 nb_entries, alloc_size;

#ifndef GPAC_DISABLE_ISOM_WRITE
	u32 w_LastSampleNumber;
	/*force one sample per entry*/
	Bool unpack_mode;
#endif
	/*Cache for read*/
	u32 r_currentEntryIndex;
	u32 r_FirstSampleInEntry;

	//stats for read
	s32 max_ts_delta;
} GF_CompositionOffsetBox;


#define GF_ISOM_SAMPLE_ENTRY_FIELDS		\
	GF_ISOM_UUID_BOX					\
	u16 dataReferenceIndex;				\
	char reserved[ 6 ];					\
	u32 internal_type;					\

/*base sample entry box - used by some generic media sample descriptions of QT*/
typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS
} GF_SampleEntryBox;

void gf_isom_sample_entry_init(GF_SampleEntryBox *ptr);
void gf_isom_sample_entry_predestroy(GF_SampleEntryBox *ptr);
GF_Err gf_isom_base_sample_entry_read(GF_SampleEntryBox *ptr, GF_BitStream *bs);

typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS
	/*box type as specified in the file (not this box's type!!)*/
	u32 EntryType;

	u8 *data;
	u32 data_size;
} GF_GenericSampleEntryBox;

typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS

	u32 flags;
	u32 timescale;
	u32 frame_duration;
	u8 frames_per_sec;
} GF_TimeCodeSampleEntryBox;

typedef struct
{
	GF_ISOM_FULL_BOX

    u16 text_font;
    u16 text_face;
    u16 text_size;
    u16 text_color_red, text_color_green, text_color_blue;
    u16 back_color_red, back_color_green, back_color_blue;
    char *font;
} GF_TimeCodeMediaInformationBox;

typedef struct
{
	GF_ISOM_BOX

    u8 field_count;
    u8 field_order;
} GF_FieldInfoBox;

typedef struct
{
	GF_ISOM_BOX
    u32 gama;
} GF_GamaInfoBox;

typedef struct
{
	GF_ISOM_BOX
    u16 chroma;
} GF_ChromaInfoBox;

typedef struct
{
    u32 label;
    u32 flags;
    Float coordinates[3];
} GF_AudioChannelDescription;

typedef struct
{
	GF_ISOM_FULL_BOX

    u32 layout_tag;
    u32 bitmap;
    u32 num_audio_description;
    GF_AudioChannelDescription *audio_descs;
} GF_ChannelLayoutInfoBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_ESD *desc;
} GF_ESDBox;

typedef struct
{
	GF_ISOM_BOX
	u32 bufferSizeDB;
	u32 maxBitrate;
	u32 avgBitrate;
} GF_BitRateBox;

GF_BitRateBox *gf_isom_sample_entry_get_bitrate(GF_SampleEntryBox *ent, Bool create);

typedef struct
{
	GF_ISOM_BOX
	GF_List *descriptors;
} GF_MPEG4ExtensionDescriptorsBox;

/*for most MPEG4 media */
typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS
	GF_ESDBox *esd;
	/*used for hinting when extracting the OD stream...*/
	GF_SLConfig *slc;
} GF_MPEGSampleEntryBox;

typedef struct
{
	GF_ISOM_BOX
	u8 *hdr;
	u32 hdr_size;
} GF_LASERConfigurationBox;


typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS

	GF_LASERConfigurationBox *lsr_config;
	GF_MPEG4ExtensionDescriptorsBox *descr;

	/*used for hinting when extracting the OD stream...*/
	GF_SLConfig *slc;
} GF_LASeRSampleEntryBox;

/*rewrites avcC based on the given esd - this destroys the esd*/
GF_Err LSR_UpdateESD(GF_LASeRSampleEntryBox *lsr, GF_ESD *esd);

typedef struct
{
	GF_ISOM_BOX
	u32 hSpacing;
	u32 vSpacing;
} GF_PixelAspectRatioBox;

typedef struct
{
	GF_ISOM_BOX
	u32 cleanApertureWidthN;
	u32 cleanApertureWidthD;
	u32 cleanApertureHeightN;
	u32 cleanApertureHeightD;
	u32 horizOffN;
	u32 horizOffD;
	u32 vertOffN;
	u32 vertOffD;
} GF_CleanApertureBox;


typedef struct __ContentLightLevel {
	GF_ISOM_BOX
	u16 max_content_light_level;
	u16 max_pic_average_light_level;
} GF_ContentLightLevelBox;

typedef struct ___MasteringDisplayColourVolume {
	GF_ISOM_BOX
	struct {
		u16 x;
		u16 y;
	} display_primaries[3];
	u16 white_point_x;
	u16 white_point_y;
	u32 max_display_mastering_luminance;
	u32 min_display_mastering_luminance;
} GF_MasteringDisplayColourVolumeBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	Bool all_ref_pics_intra;
	Bool intra_pred_used;
	u32 max_ref_per_pic;
	u32 reserved;
} GF_CodingConstraintsBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	char *aux_track_type;
} GF_AuxiliaryTypeInfoBox;

typedef struct
{
	GF_ISOM_BOX
	u16 predefined_rvc_config;
	u32 rvc_meta_idx;
} GF_RVCConfigurationBox;


typedef struct {
	GF_ISOM_BOX
	Bool is_jp2;

	u32 colour_type;
	u16 colour_primaries;
	u16 transfer_characteristics;
	u16 matrix_coefficients;
	Bool full_range_flag;
	u8 *opaque;
	u32 opaque_size;

	u8 method, precedence, approx;
} GF_ColourInformationBox;


//do NOT extend this structure with boxes, children boxes shall go into the other_box field of the parent
#define GF_ISOM_VISUAL_SAMPLE_ENTRY		\
	GF_ISOM_SAMPLE_ENTRY_FIELDS			\
	u16 revision;						\
	u32 vendor;							\
	u32 temporal_quality;				\
	u32 spatial_quality;				\
	u16 Width, Height;					\
	u32 horiz_res, vert_res;			\
	u32 entry_data_size;				\
	u16 frames_per_sample;				\
	char compressor_name[33];			\
	u16 bit_depth;						\
	s16 color_table_index;				\
	struct __tag_protect_box *rinf; 	\

typedef struct
{
	GF_ISOM_VISUAL_SAMPLE_ENTRY
} GF_VisualSampleEntryBox;

void gf_isom_video_sample_entry_init(GF_VisualSampleEntryBox *ent);
GF_Err gf_isom_video_sample_entry_read(GF_VisualSampleEntryBox *ptr, GF_BitStream *bs);
#ifndef GPAC_DISABLE_ISOM_WRITE
void gf_isom_video_sample_entry_write(GF_VisualSampleEntryBox *ent, GF_BitStream *bs);
void gf_isom_video_sample_entry_size(GF_VisualSampleEntryBox *ent);
#endif

void gf_isom_sample_entry_predestroy(GF_SampleEntryBox *ptr);


GF_Box *gf_isom_box_find_child(GF_List *parent_child_list, u32 code);
void gf_isom_box_del_parent(GF_List **parent_child_list, GF_Box*b);
GF_Box *gf_isom_box_new_parent(GF_List **parent_child_list, u32 code);
Bool gf_isom_box_check_unique(GF_List *children, GF_Box *a);

typedef struct
{
	GF_ISOM_BOX
	GF_AVCConfig *config;
} GF_AVCConfigurationBox;

typedef struct
{
	GF_ISOM_BOX
	GF_HEVCConfig *config;
} GF_HEVCConfigurationBox;

typedef struct
{
	GF_ISOM_BOX
	GF_AV1Config *config;
} GF_AV1ConfigurationBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_VPConfig *config;
} GF_VPConfigurationBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u16 primaryRChromaticity_x;
	u16 primaryRChromaticity_y;
	u16 primaryGChromaticity_x;
	u16 primaryGChromaticity_y;
	u16 primaryBChromaticity_x;
	u16 primaryBChromaticity_y;
	u16 whitePointChromaticity_x;
	u16 whitePointChromaticity_y;
	u32 luminanceMax;
	u32 luminanceMin;
} GF_SMPTE2086MasteringDisplayMetadataBox;

typedef struct {
	GF_ISOM_FULL_BOX
		u16 maxCLL;
	u16 maxFALL;
} GF_VPContentLightLevelBox;

typedef struct {
	u8 dv_version_major;
	u8 dv_version_minor;
	u8 dv_profile; //7 bits
	u8 dv_level;   //6 bits
	Bool rpu_present_flag;
	Bool el_present_flag;
	Bool bl_present_flag;
	//const unsigned int (32)[5] reserved = 0;
} GF_DOVIDecoderConfigurationRecord;

typedef struct {
	GF_ISOM_BOX
	GF_DOVIDecoderConfigurationRecord DOVIConfig;
} GF_DOVIConfigurationBox;

/*typedef struct { //extends Box('hvcE')
	GF_ISOM_BOX
	GF_HEVCConfig HEVCConfig;
} GF_DolbyVisionELHEVCConfigurationBox;*/

typedef struct { //extends HEVCSampleEntry('dvhe')
	GF_DOVIConfigurationBox config;
	//TODO: GF_DolbyVisionELHEVCConfigurationBox ELConfig; // optional
} GF_DolbyVisionHEVCSampleEntry;

typedef struct
{
	GF_ISOM_BOX
	GF_3GPConfig cfg;
} GF_3GPPConfigBox;

typedef struct
{
	GF_ISOM_BOX

	u32 width, height;
	u16 nb_comp;
	u8 bpc, Comp, UnkC, IPR;
} GF_J2KImageHeaderBox;

typedef struct
{
	GF_ISOM_BOX
	GF_J2KImageHeaderBox *ihdr;
	GF_ColourInformationBox *colr;
} GF_J2KHeaderBox;

typedef struct
{
	GF_ISOM_VISUAL_SAMPLE_ENTRY
	GF_ESDBox *esd;
	/*used for Publishing*/
	GF_SLConfig *slc;

	/*avc extensions - we merged with regular 'mp4v' box to handle isma E&A signaling of AVC*/
	GF_AVCConfigurationBox *avc_config;
	GF_AVCConfigurationBox *svc_config;
	GF_AVCConfigurationBox *mvc_config;
	/*hevc extension*/
	GF_HEVCConfigurationBox *hevc_config;
	GF_HEVCConfigurationBox *lhvc_config;
	/*av1 extension*/
	GF_AV1ConfigurationBox *av1_config;
	/*vp8-9 extension*/
	GF_VPConfigurationBox *vp_config;
	/*vp8-9 extension*/
	GF_J2KHeaderBox *jp2h;

	/*internally emulated esd*/
	GF_ESD *emul_esd;

	//3GPP
	GF_3GPPConfigBox *cfg_3gpp;

	/*iPod's hack*/
	GF_UnknownUUIDBox *ipod_ext;

} GF_MPEGVisualSampleEntryBox;

static const u8 GF_ISOM_IPOD_EXT[][16] = { { 0x6B, 0x68, 0x40, 0xF2, 0x5F, 0x24, 0x4F, 0xC5, 0xBA, 0x39, 0xA5, 0x1B, 0xCF, 0x03, 0x23, 0xF3} };

Bool gf_isom_is_nalu_based_entry(GF_MediaBox *mdia, GF_SampleEntryBox *_entry);
GF_Err gf_isom_nalu_sample_rewrite(GF_MediaBox *mdia, GF_ISOSample *sample, u32 sampleNumber, GF_MPEGVisualSampleEntryBox *entry);

/*this is the default visual sdst (to handle unknown media)*/
typedef struct
{
	GF_ISOM_VISUAL_SAMPLE_ENTRY
	/*box type as specified in the file (not this box's type!!)*/
	u32 EntryType;
	/*opaque description data (ESDS in MP4, SMI in SVQ3, ...)*/
	u8 *data;
	u32 data_size;
} GF_GenericVisualSampleEntryBox;


#define GF_ISOM_AUDIO_SAMPLE_ENTRY	\
	GF_ISOM_SAMPLE_ENTRY_FIELDS		\
	u16 revision;					\
	u32 vendor;						\
	u16 channel_count;				\
	u16 bitspersample;				\
	u16 compression_id;				\
	u16 packet_size;				\
	u32 is_qtff;					\
	u16 samplerate_hi;				\
	u16 samplerate_lo;				\
	u8 extensions[36];				\


typedef struct
{
	GF_ISOM_AUDIO_SAMPLE_ENTRY
} GF_AudioSampleEntryBox;

void gf_isom_audio_sample_entry_init(GF_AudioSampleEntryBox *ptr);
GF_Err gf_isom_audio_sample_entry_read(GF_AudioSampleEntryBox *ptr, GF_BitStream *bs);
#ifndef GPAC_DISABLE_ISOM_WRITE
void gf_isom_audio_sample_entry_write(GF_AudioSampleEntryBox *ptr, GF_BitStream *bs);
void gf_isom_audio_sample_entry_size(GF_AudioSampleEntryBox *ptr);
#endif

typedef struct
{
	GF_ISOM_BOX
	GF_AC3Config cfg;
} GF_AC3ConfigBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u8 *data;
	u32 dataSize;
} GF_FLACConfigBox;

typedef struct
{
	GF_ISOM_BOX

	/*OpusSpecificBox*/
	/*u8 version;              //1, field included in base box structure */
	u8 OutputChannelCount;   //same value as the *Output Channel Count* field in the identification header defined in Ogg Opus [3]
	u16 PreSkip;             //The value of the PreSkip field shall be at least 80 milliseconds' worth of PCM samples even when removing any number of Opus samples which may or may not contain the priming samples. The PreSkip field is not used for discarding the priming samples at the whole playback at all since it is informative only, and that task falls on the Edit List Box.
	u32 InputSampleRate;     //The InputSampleRate field shall be set to the same value as the *Input Sample Rate* field in the identification header defined in Ogg Opus
	s16 OutputGain;          //The OutputGain field shall be set to the same value as the *Output Gain* field in the identification header define in Ogg Opus [3]. Note that the value is stored as 8.8 fixed-point.
	u8 ChannelMappingFamily; //The ChannelMappingFamily field shall be set to the same value as the *Channel Mapping Family* field in the identification header defined in Ogg Opus [3]. Note that the value 255 may be used for an alternative to map channels by ISO Base Media native mapping. The details are described in 4.5.1.

	u8 StreamCount; // The StreamCount field shall be set to the same value as the *Stream Count* field in the identification header defined in Ogg Opus [3].
	u8 CoupledCount; // The CoupledCount field shall be set to the same value as the *Coupled Count* field in the identification header defined in Ogg Opus [3].
	u8 ChannelMapping[255]; // The ChannelMapping field shall be set to the same octet string as *Channel Mapping* field in the identi- fication header defined in Ogg Opus [3].

	/*for internal box use only*/
//	int channels;
} GF_OpusSpecificBox;

GF_Err gf_isom_opus_config_new(GF_ISOFile *the_file, u32 trackNumber, GF_OpusSpecificBox *cfg, char *URLname, char *URNname, u32 *outDescriptionIndex);

typedef struct
{
	GF_ISOM_BOX
	u8 configuration_version;
	u8 mha_pl_indication;
	u8 reference_channel_layout;
	u16 mha_config_size;
	char *mha_config;
} GF_MHAConfigBox;


typedef struct
{
	GF_ISOM_AUDIO_SAMPLE_ENTRY
	//for MPEG4 audio
	GF_ESDBox *esd;
	GF_SLConfig *slc;
	//for 3GPP audio
	GF_3GPPConfigBox *cfg_3gpp;

	//for AC3/EC3 audio
	GF_AC3ConfigBox *cfg_ac3;

	//for Opus
	GF_OpusSpecificBox *cfg_opus;

	//for MPEG-H audio
	GF_MHAConfigBox *cfg_mha;

	//for FLAC
	GF_FLACConfigBox *cfg_flac;

} GF_MPEGAudioSampleEntryBox;

/*this is the default visual sdst (to handle unknown media)*/
typedef struct
{
	GF_ISOM_AUDIO_SAMPLE_ENTRY
	/*box type as specified in the file (not this box's type!!)*/
	u32 EntryType;
	/*opaque description data (ESDS in MP4, ...)*/
	u8 *data;
	u32 data_size;
} GF_GenericAudioSampleEntryBox;


typedef struct
{
	GF_ISOM_FULL_BOX
	u8 profile;
	u8 level;
	u8 pathComponents;
	Bool fullRequestHost;
	Bool streamType;
	u8 containsRedundant;
	char *textEncoding;
	char *contentEncoding;
} GF_DIMSSceneConfigBox;

typedef struct
{
	GF_ISOM_BOX
	char *content_script_types;
} GF_DIMSScriptTypesBox;

typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS
	GF_DIMSSceneConfigBox *config;
	GF_DIMSScriptTypesBox *scripts;
} GF_DIMSSampleEntryBox;


typedef struct
{
	GF_ISOM_FULL_BOX
	char *config;
} GF_TextConfigBox;

/*base metadata sample entry box for METT, METX, SBTT, STXT and STPP*/
typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS
	char *content_encoding;	//optional
	char *mime_type; //for anything except metx
	char *xml_namespace;	//for metx and sttp only
	char *xml_schema_loc;	// for metx and sttp only
	GF_TextConfigBox *config; //optional for anything except metx and sttp
} GF_MetaDataSampleEntryBox;


typedef struct
{
	u8 entry_type;
	union {
		u32 trackID;
		u32 output_view_id;
		u32 start_view_id;
	};
	union {
		u16 tierID;
		u16 view_count;
	};
} MVCIEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 multiview_group_id;
	u16 num_entries;
	MVCIEntry *entries;
} GF_MultiviewGroupBox;

typedef struct
{
	u8 dep_comp_idc;
	u16 ref_view_id;
} ViewIDRefViewEntry;

typedef struct
{
	u16 view_id;
	u16 view_order_index;
	u8 texture_in_stream;
	u8 texture_in_track;
	u8 depth_in_stream;
	u8 depth_in_track;
	u8 base_view_type;
	u16 num_ref_views;
	ViewIDRefViewEntry *view_refs;
} ViewIDEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	u8 min_temporal_id;
	u8 max_temporal_id;
	u16 num_views;
	ViewIDEntry *views;
} GF_ViewIdentifierBox;


typedef struct
{
	GF_ISOM_FULL_BOX
} GF_SampleDescriptionBox;


typedef struct
{
	GF_ISOM_FULL_BOX
	/*if this is the compact version, sample size is actually fieldSize*/
	u32 sampleSize;
	u32 sampleCount;
	u32 alloc_size;
	u32 *sizes;
	//stats for read
	u32 max_size;
	u64 total_size;
	u32 total_samples;
} GF_SampleSizeBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 nb_entries;
	u32 alloc_size;
	u32 *offsets;
} GF_ChunkOffsetBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 nb_entries;
	u32 alloc_size;
	u64 *offsets;
} GF_ChunkLargeOffsetBox;

typedef struct
{
	u32 firstChunk;
	u32 nextChunk;
	u32 samplesPerChunk;
	u32 sampleDescriptionIndex;
	u8 isEdited;
} GF_StscEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_StscEntry *entries;
	u32 alloc_size, nb_entries;

	/*0-based cache for READ. In WRITE mode, we always have 1 sample per chunk so no need for a cache*/
	u32 currentIndex;
	/*first sample number in this chunk*/
	u32 firstSampleInCurrentChunk;
	u32 currentChunk;
	u32 ghostNumber;

	u32 w_lastSampleNumber;
	u32 w_lastChunkNumber;
} GF_SampleToChunkBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 alloc_size, nb_entries;
	u32 *sampleNumbers;
	/*cache for READ mode (in write we realloc no matter what)*/
	u32 r_LastSyncSample;
	/*0-based index in the array*/
	u32 r_LastSampleIndex;
} GF_SyncSampleBox;

typedef struct
{
	u32 shadowedSampleNumber;
	s32 syncSampleNumber;
} GF_StshEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_List *entries;
	/*Cache for read mode*/
	u32 r_LastEntryIndex;
	u32 r_LastFoundSample;
} GF_ShadowSyncBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 nb_entries;
	u16 *priorities;
} GF_DegradationPriorityBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 SampleCount;
	u8 *padbits;
} GF_PaddingBitsBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 sampleCount;
	/*each dep type is packed on 1 byte*/
	u8 *sample_info;
} GF_SampleDependencyTypeBox;


typedef struct
{
	u32 subsample_size;
	u8 subsample_priority;
	u8 discardable;
	u32 reserved;
} GF_SubSampleEntry;

typedef struct
{
	u32 sample_delta;
	GF_List *SubSamples;
} GF_SubSampleInfoEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_List *Samples;
} GF_SubSampleInformationBox;

Bool gf_isom_get_subsample_types(GF_ISOFile *movie, u32 track, u32 subs_index, u32 *flags);
u32  gf_isom_sample_get_subsample_entry(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 entry_index, GF_SubSampleInfoEntry **sub_sample);
#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err gf_isom_add_subsample_info(GF_SubSampleInformationBox *sub_samples, u32 sampleNumber, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable);
#endif

/* Use to relate the composition and decoding timeline when signed composition is used*/
typedef struct
{
	GF_ISOM_FULL_BOX

	s32 compositionToDTSShift;
	s32 leastDecodeToDisplayDelta;
	s32 greatestDecodeToDisplayDelta;
	s32 compositionStartTime;
	s32 compositionEndTime;
} GF_CompositionToDecodeBox;

typedef struct
{
	GF_ISOM_FULL_BOX

	u32 aux_info_type;
	u32 aux_info_type_parameter;

	u8 default_sample_info_size;
	u32 sample_count, sample_alloc;
	u8 *sample_info_size;

	u32 cached_sample_num;
	u32 cached_prev_size;
} GF_SampleAuxiliaryInfoSizeBox;

typedef struct
{
	GF_ISOM_FULL_BOX

	u32 aux_info_type;
	u32 aux_info_type_parameter;

	u8 default_sample_info_size;
	u32 entry_count;  //1 or stco / trun count
	u64 *offsets;

	u64 offset_first_offset_field;

	u32 total_size;
	u8 *cached_data;
} GF_SampleAuxiliaryInfoOffsetBox;

typedef struct
{
	u32 nb_entries, nb_alloc;
	u32 *sample_num;
} GF_TrafToSampleMap;

typedef struct
{
	GF_ISOM_BOX
	GF_TimeToSampleBox *TimeToSample;
	GF_CompositionOffsetBox *CompositionOffset;
	GF_CompositionToDecodeBox *CompositionToDecode;
	GF_SyncSampleBox *SyncSample;
	GF_SampleDescriptionBox *SampleDescription;
	GF_SampleSizeBox *SampleSize;
	GF_SampleToChunkBox *SampleToChunk;
	/*untyped, to handle 32 bits and 64 bits chunkOffsets*/
	GF_Box *ChunkOffset;
	GF_ShadowSyncBox *ShadowSync;
	GF_DegradationPriorityBox *DegradationPriority;
	GF_PaddingBitsBox *PaddingBits;
	GF_SampleDependencyTypeBox *SampleDep;

	GF_TrafToSampleMap *traf_map;

	GF_List *sub_samples;

	GF_List *sampleGroups;
	GF_List *sampleGroupsDescription;
	u32 nb_sgpd_in_stbl;
	u32 nb_stbl_boxes;

	GF_List *sai_sizes;
	GF_List *sai_offsets;

	u32 MaxSamplePerChunk, MaxChunkSize;
	u16 groupID;
	u16 trackPriority;
	u32 currentEntryIndex;

	Bool no_sync_found;
	Bool skip_sample_groups;
} GF_SampleTableBox;

void stbl_AppendTrafMap(GF_SampleTableBox *stbl);

typedef struct __tag_media_info_box
{
	GF_ISOM_BOX
	GF_DataInformationBox *dataInformation;
	GF_SampleTableBox *sampleTable;
	GF_Box *InfoHeader;
	struct __tag_data_map *scalableDataHandler;
	struct __tag_data_map *dataHandler;
	u32 dataEntryIndex;
} GF_MediaInformationBox;

GF_Err stbl_SetDependencyType(GF_SampleTableBox *stbl, u32 sampleNumber, u32 isLeading, u32 dependsOn, u32 dependedOn, u32 redundant);
GF_Err stbl_AppendDependencyType(GF_SampleTableBox *stbl, u32 isLeading, u32 dependsOn, u32 dependedOn, u32 redundant);
GF_Err stbl_AddDependencyType(GF_SampleTableBox *stbl, u32 sampleNumber, u32 isLeading, u32 dependsOn, u32 dependedOn, u32 redundant);

typedef struct
{
	GF_ISOM_BOX
	u8 *data;
	u32 dataSize;
	u32 original_4cc;
} GF_FreeSpaceBox;

typedef struct
{
	GF_ISOM_BOX
} GF_WideBox; /*Apple*/

typedef struct
{
	GF_ISOM_FULL_BOX
	char packedLanguageCode[4];
	char *notice;
} GF_CopyrightBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	char *schemeURI;
	char *value;
} GF_KindBox;


typedef struct
{
	char *name;
	u64 start_time;
} GF_ChapterEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_List *list;
} GF_ChapterListBox;

typedef struct
{
	GF_ISOM_BOX
	u32 reference_type;
	u32 trackIDCount;
	GF_ISOTrackID *trackIDs;
} GF_TrackReferenceTypeBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 grouping_type;
	u32 group_id;
	u32 entity_id_count;
	u32 *entity_ids;
} GF_EntityToGroupTypeBox;

typedef struct
{
	GF_ISOM_BOX
	u32 majorBrand;
	u32 minorVersion;
	u32 altCount;
	u32 *altBrand;
} GF_FileTypeBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 *rates;
	u32 *times;
	u32 count;
} GF_ProgressiveDownloadBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 switch_group;
	u32 alternate_group;
	GF_ISOTrackID sub_track_id;
	u64 attribute_count;
	u32 *attribute_list;
} GF_SubTrackInformationBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 grouping_type;
	u16 nb_groups;
	u32 *group_description_index;
} GF_SubTrackSampleGroupBox;

typedef struct
{
	GF_ISOM_BOX
	GF_SubTrackInformationBox *info;
	GF_Box *strd;
} GF_SubTrackBox;

/*
	3GPP streaming text boxes
*/

typedef struct
{
	GF_ISOM_BOX
	u32 entry_count;
	GF_FontRecord *fonts;
} GF_FontTableBox;

typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS				\
	u32 displayFlags;
	s8 horizontal_justification;
	s8 vertical_justification;
	/*ARGB*/
	u32 back_color;
	GF_BoxRecord default_box;
	GF_StyleRecord	default_style;
	GF_FontTableBox *font_table;
} GF_Tx3gSampleEntryBox;

/*Apple specific*/
typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS				\
	u32 displayFlags;
	u32 textJustification;
	char background_color[6], foreground_color[6];
	GF_BoxRecord default_box;
	u16 fontNumber;
	u16 fontFace;
	char reserved1[8];
	u8 reserved2;
	u16 reserved3;
	char *textName; /*font name*/
} GF_TextSampleEntryBox;

typedef struct
{
	GF_ISOM_BOX
	u32 entry_count;
	GF_StyleRecord *styles;
} GF_TextStyleBox;

typedef struct
{
	GF_ISOM_BOX
	u16 startcharoffset;
	u16 endcharoffset;
} GF_TextHighlightBox;

typedef struct
{
	GF_ISOM_BOX
	/*ARGB*/
	u32 hil_color;
} GF_TextHighlightColorBox;

typedef struct
{
	u32 highlight_endtime;
	u16 start_charoffset;
	u16 end_charoffset;
} KaraokeRecord;

typedef struct
{
	GF_ISOM_BOX
	u32 highlight_starttime;
	u16 nb_entries;
	KaraokeRecord *records;
} GF_TextKaraokeBox;

typedef struct
{
	GF_ISOM_BOX
	u32 scroll_delay;
} GF_TextScrollDelayBox;

typedef struct
{
	GF_ISOM_BOX
	u16 startcharoffset;
	u16 endcharoffset;
	char *URL;
	char *URL_hint;
} GF_TextHyperTextBox;

typedef struct
{
	GF_ISOM_BOX
	GF_BoxRecord box;
} GF_TextBoxBox;

typedef struct
{
	GF_ISOM_BOX
	u16 startcharoffset;
	u16 endcharoffset;
} GF_TextBlinkBox;

typedef struct
{
	GF_ISOM_BOX
	u8 wrap_flag;
} GF_TextWrapBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 switchGroup;
	u32 *attributeList;
	u32 attributeListCount;
} GF_TrackSelectionBox;

/*
	MPEG-21 extensions
*/
typedef struct
{
	GF_ISOM_FULL_BOX
	char *xml;
} GF_XMLBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 data_length;
	u8 *data;
} GF_BinaryXMLBox;

typedef struct
{
	u64 extent_offset;
	u64 extent_length;
	u64 extent_index;
#ifndef GPAC_DISABLE_ISOM_WRITE
	/*for storage only*/
	u64 original_extent_offset;
#endif
} GF_ItemExtentEntry;

typedef struct
{
	u16 item_ID;
	u16 construction_method;
	u16 data_reference_index;
	u64 base_offset;
#ifndef GPAC_DISABLE_ISOM_WRITE
	/*for storage only*/
	u64 original_base_offset;
#endif
	GF_List *extent_entries;
} GF_ItemLocationEntry;

void iloc_entry_del(GF_ItemLocationEntry *location);

typedef struct
{
	GF_ISOM_FULL_BOX
	u8 offset_size;
	u8 length_size;
	u8 base_offset_size;
	u8 index_size;
	GF_List *location_entries;
} GF_ItemLocationBox;

typedef	struct
{
	GF_ISOM_FULL_BOX
	u16 item_ID;
} GF_PrimaryItemBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_List *protection_information;
} GF_ItemProtectionBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u16 item_ID;
	u16 item_protection_index;
	u32 item_type;
	/*zero-terminated strings*/
	char *item_name;
	char *content_type;
	char *content_encoding;
	// needed to actually read the resource file, but not written in the MP21 file.
	char *full_path;
	// if not 0, full_path is actually the data to write.
	u32 data_len;
} GF_ItemInfoEntryBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_List *item_infos;
} GF_ItemInfoBox;

typedef struct
{
	GF_ISOM_BOX
	u32 reference_type;
	u32 from_item_id;
	u32 reference_count;
	u32 *to_item_IDs;
} GF_ItemReferenceTypeBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_List *references;
} GF_ItemReferenceBox;

typedef struct
{
	GF_ISOM_BOX
	u32 data_format;
} GF_OriginalFormatBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 scheme_type;
	u32 scheme_version;
	char *URI;
} GF_SchemeTypeBox;

/*ISMACryp specific*/
typedef struct
{
	GF_ISOM_FULL_BOX
	/*zero-terminated string*/
	char *URI;
} GF_ISMAKMSBox;

/*ISMACryp specific*/
typedef struct
{
	GF_ISOM_BOX
	u64 salt;
} GF_ISMACrypSaltBox;

/*ISMACryp specific*/
typedef struct __isma_format_box
{
	GF_ISOM_FULL_BOX
	u8 selective_encryption;
	u8 key_indicator_length;
	u8 IV_length;
} GF_ISMASampleFormatBox;

typedef struct
{
	GF_ISOM_BOX
	GF_ISMAKMSBox *ikms;
	GF_ISMASampleFormatBox *isfm;
	GF_ISMACrypSaltBox *islt;
	struct __oma_kms_box *odkm;
	struct __cenc_tenc_box *tenc;
	struct __piff_tenc_box *piff_tenc;
	struct __adobe_drm_key_management_system_box *adkm;
} GF_SchemeInformationBox;

typedef struct __tag_protect_box
{
	GF_ISOM_BOX
	GF_OriginalFormatBox *original_format;
	GF_SchemeTypeBox *scheme_type;
	GF_SchemeInformationBox *info;
} GF_ProtectionSchemeInfoBox;
typedef struct __tag_protect_box GF_RestrictedSchemeInfoBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_List *descriptors;
} GF_IPMPInfoBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_IPMP_ToolList *ipmp_tools;
	GF_List *descriptors;
} GF_IPMPControlBox;

typedef struct {
	GF_ISOM_BOX
} GF_ItemPropertyContainerBox;

typedef struct {
	GF_ISOM_BOX
	GF_ItemPropertyContainerBox *property_container;
	struct __item_association_box *property_association;
} GF_ItemPropertiesBox;

typedef struct {
	GF_ISOM_BOX
} GF_GroupListBox;

typedef struct __tag_meta_box
{
	GF_ISOM_FULL_BOX
	GF_HandlerBox *handler;
	GF_PrimaryItemBox *primary_resource;
	GF_DataInformationBox *file_locations;
	GF_ItemLocationBox *item_locations;
	GF_ItemProtectionBox *protections;
	GF_ItemInfoBox *item_infos;
	GF_IPMPControlBox *IPMP_control;
	GF_ItemPropertiesBox *item_props;
	GF_ItemReferenceBox *item_refs;
} GF_MetaBox;

typedef struct
{
	GF_ISOM_FULL_BOX

	u32 single_view_allowed;
	u32 stereo_scheme;
	u32 sit_len;
	char *stereo_indication_type;
} GF_StereoVideoBox;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

/*V2 boxes - Movie Fragments*/

typedef struct
{
	GF_ISOM_FULL_BOX
	u64 fragment_duration;
} GF_MovieExtendsHeaderBox;


typedef struct __tag_mvex_box
{
	GF_ISOM_BOX
	GF_List *TrackExList;
	GF_List *TrackExPropList;
	GF_MovieExtendsHeaderBox *mehd;
	GF_ISOFile *mov;
} GF_MovieExtendsBox;

/*the TrackExtends contains default values for the track fragments*/
typedef struct
{
	GF_ISOM_FULL_BOX
	GF_ISOTrackID trackID;
	u32 def_sample_desc_index;
	u32 def_sample_duration;
	u32 def_sample_size;
	u32 def_sample_flags;
	GF_TrackBox *track;

	Bool cannot_use_default;
	
	GF_TrackFragmentRandomAccessBox *tfra;
} GF_TrackExtendsBox;

/*the TrackExtends contains default values for the track fragments*/
typedef struct
{
	GF_ISOM_FULL_BOX
	GF_ISOTrackID trackID;
} GF_TrackExtensionPropertiesBox;

/*indicates the seq num of this fragment*/
typedef struct
{
	GF_ISOM_FULL_BOX
	u32 sequence_number;
} GF_MovieFragmentHeaderBox;

/*MovieFragment is a container IN THE FILE, contains 1 fragment*/
typedef struct
{
	GF_ISOM_BOX
	GF_MovieFragmentHeaderBox *mfhd;
	GF_List *TrackList;
	GF_List *PSSHs;
	GF_ISOFile *mov;
	/*offset in the file of moof or mdat (whichever comes first) for this fragment*/
	u64 fragment_offset;
	u32 mdat_size;
	u8 *mdat;

	//temp storage of prft box
	GF_ISOTrackID reference_track_ID;
	u64 ntp, timestamp;
} GF_MovieFragmentBox;


/*FLAGS for TRAF*/
enum
{
	GF_ISOM_TRAF_BASE_OFFSET	=	0x01,
	GF_ISOM_TRAF_SAMPLE_DESC	=	0x02,
	GF_ISOM_TRAF_SAMPLE_DUR	=	0x08,
	GF_ISOM_TRAF_SAMPLE_SIZE	=	0x10,
	GF_ISOM_TRAF_SAMPLE_FLAGS	=	0x20,
	GF_ISOM_TRAF_DUR_EMPTY	=	0x10000,
	GF_ISOM_MOOF_BASE_OFFSET	=	0x20000,
};

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_ISOTrackID trackID;
	/* all the following are optional fields */
	u64 base_data_offset;
	u32 sample_desc_index;
	u32 def_sample_duration;
	u32 def_sample_size;
	u32 def_sample_flags;
	u32 EmptyDuration;
	u8 IFrameSwitching;
} GF_TrackFragmentHeaderBox;


typedef struct
{
	GF_ISOM_FULL_BOX
	u64 baseMediaDecodeTime;
} GF_TFBaseMediaDecodeTimeBox;

typedef struct
{
	GF_ISOM_BOX
	GF_TrackFragmentHeaderBox *tfhd;
	GF_List *TrackRuns;
	/*keep a pointer to default flags*/
	GF_TrackExtendsBox *trex;
	GF_SampleDependencyTypeBox *sdtp;

//	GF_SubSampleInformationBox *subs;
	GF_List *sub_samples;

	GF_List *sampleGroups;
	GF_List *sampleGroupsDescription;

	GF_List *sai_sizes;
	GF_List *sai_offsets;

	//can be senc or PIFF psec
	struct __sample_encryption_box *sample_encryption;
	struct __traf_mss_timeext_box *tfxd; /*similar to PRFT but for Smooth Streaming*/

	/*when data caching is on*/
	u32 DataCache;
	GF_TFBaseMediaDecodeTimeBox *tfdt;

	u64 moof_start_in_bs;
} GF_TrackFragmentBox;

/*FLAGS for TRUN : specify what is written in the SampleTable of TRUN*/
enum
{
	GF_ISOM_TRUN_DATA_OFFSET	= 0x01,
	GF_ISOM_TRUN_FIRST_FLAG		= 0x04,
	GF_ISOM_TRUN_DURATION		= 0x100,
	GF_ISOM_TRUN_SIZE			= 0x200,
	GF_ISOM_TRUN_FLAGS			= 0x400,
	GF_ISOM_TRUN_CTS_OFFSET		= 0x800
};

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 sample_count;
	/*the following are optional fields */
	s32 data_offset; /* unsigned for version 0 */
	u32 first_sample_flags;
	/*can be empty*/
	GF_List *entries;

	/*in write mode with data caching*/
	GF_BitStream *cache;
} GF_TrackFragmentRunBox;

typedef struct
{
	u32 Duration;
	u32 size;
	u32 flags;
	s32 CTS_Offset;

	/*internal*/
	u32 SAP_type;
	u64 dts;
	u32 nb_pack;
} GF_TrunEntry;

typedef struct
{
	GF_ISOM_BOX
	u32 majorBrand;
	u32 minorVersion;
	u32 altCount;
	u32 *altBrand;
} GF_SegmentTypeBox;

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS*/


/*RTP Hint Track Sample Entry*/
typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS
	u16 HintTrackVersion;
	u16 LastCompatibleVersion;
	u32 MaxPacketSize;
//	GF_List *HintDataTable;
	/*this is where we store the current RTP sample in read/write mode*/
	struct __tag_hint_sample *hint_sample;
	/*current hint sample in read mode, 1-based (0 is reset)*/
	u32 cur_sample;
	u32 pck_sn, ts_offset, ssrc;
	GF_TrackReferenceTypeBox *hint_ref;

	//for FEC
	u16 partition_entry_ID, FEC_overhead;
} GF_HintSampleEntryBox;


typedef struct
{
	GF_ISOM_BOX
	u32 subType;
	char *sdpText;
} GF_RTPBox;

typedef struct
{
	GF_ISOM_BOX
	char *sdpText;
} GF_SDPBox;

typedef struct
{
	GF_ISOM_BOX
	s32 timeOffset;
} GF_RTPOBox;

typedef struct
{
	GF_ISOM_BOX
	/*contains GF_SDPBox if in track, GF_RTPBox if in movie*/
	GF_Box *SDP;
} GF_HintTrackInfoBox;

typedef struct
{
	GF_ISOM_BOX
	u8 reserved;
	u8 preferred;
	u8 required;
} GF_RelyHintBox;

/***********************************************************
			data entry tables for RTP
***********************************************************/
typedef struct
{
	GF_ISOM_BOX
	u32 timeScale;
} GF_TSHintEntryBox;

typedef struct
{
	GF_ISOM_BOX
	u32 TimeOffset;
} GF_TimeOffHintEntryBox;

typedef struct
{
	GF_ISOM_BOX
	u32 SeqOffset;
} GF_SeqOffHintEntryBox;



/***********************************************************
			hint track information boxes for RTP
***********************************************************/

/*Total number of bytes that will be sent, including 12-byte RTP headers, but not including any network headers*/
typedef struct
{
	GF_ISOM_BOX
	u64 nbBytes;
} GF_TRPYBox;

/*32-bits version of trpy used in Darwin*/
typedef struct
{
	GF_ISOM_BOX
	u32 nbBytes;
} GF_TOTLBox;

/*Total number of network packets that will be sent*/
typedef struct
{
	GF_ISOM_BOX
	u64 nbPackets;
} GF_NUMPBox;

/*32-bits version of nump used in Darwin*/
typedef struct
{
	GF_ISOM_BOX
	u32 nbPackets;
} GF_NPCKBox;


/*Total number of bytes that will be sent, not including 12-byte RTP headers*/
typedef struct
{
	GF_ISOM_BOX
	u64 nbBytes;
} GF_NTYLBox;

/*32-bits version of tpyl used in Darwin*/
typedef struct
{
	GF_ISOM_BOX
	u32 nbBytes;
} GF_TPAYBox;

/*Maximum data rate in bits per second.*/
typedef struct
{
	GF_ISOM_BOX
	u32 granularity;
	u32 maxDataRate;
} GF_MAXRBox;


/*Total number of bytes from the media track to be sent*/
typedef struct
{
	GF_ISOM_BOX
	u64 nbBytes;
} GF_DMEDBox;

/*Number of bytes of immediate data to be sent*/
typedef struct
{
	GF_ISOM_BOX
	u64 nbBytes;
} GF_DIMMBox;


/*Number of bytes of repeated data to be sent*/
typedef struct
{
	GF_ISOM_BOX
	u64 nbBytes;
} GF_DREPBox;

/*Smallest relative transmission time, in milliseconds. signed integer for smoothing*/
typedef struct
{
	GF_ISOM_BOX
	s32 minTime;
} GF_TMINBox;

/*Largest relative transmission time, in milliseconds.*/
typedef struct
{
	GF_ISOM_BOX
	s32 maxTime;
} GF_TMAXBox;

/*Largest packet, in bytes, including 12-byte RTP header*/
typedef struct
{
	GF_ISOM_BOX
	u32 maxSize;
} GF_PMAXBox;

/*Longest packet duration, in milliseconds*/
typedef struct
{
	GF_ISOM_BOX
	u32 maxDur;
} GF_DMAXBox;

/*32-bit payload type number, followed by rtpmap payload string */
typedef struct
{
	GF_ISOM_BOX
	u32 payloadCode;
	char *payloadString;
} GF_PAYTBox;


typedef struct
{
	GF_ISOM_BOX
	char *string;
} GF_NameBox;

typedef struct
{
	GF_ISOM_BOX
} GF_HintInfoBox;

typedef struct
{
	GF_ISOM_BOX
	u8 timestamp_sync;
} GF_TimeStampSynchronyBox;

typedef struct
{
	GF_ISOM_BOX
	u32 ssrc;
} GF_ReceivedSsrcBox;


typedef struct
{
	GF_ISOM_FULL_BOX
	u32 encryption_algorithm_rtp;
	u32 encryption_algorithm_rtcp;
	u32 integrity_algorithm_rtp;
	u32 integrity_algorithm_rtcp;

	GF_SchemeTypeBox *scheme_type;
	GF_SchemeInformationBox *info;
} GF_SRTPProcessBox;

/*Apple extension*/

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 reserved;
	u8 *data;
	u32 dataSize;
	Bool qt_style;
} GF_DataBox;

typedef struct
{
	GF_ISOM_BOX
	GF_DataBox *data;
} GF_ListItemBox;

typedef struct
{
	GF_ISOM_BOX
} GF_ItemListBox;

/*DECE*/
typedef struct
{
	u8 pic_type;
	u8 dependency_level;
} GF_TrickPlayBoxEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 entry_count;
	GF_TrickPlayBoxEntry *entries;
} GF_TrickPlayBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u8  baseLocation[256];
	u8 basePurlLocation[256];
} GF_BaseLocationBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 profile_version;
	char *APID;
} GF_AssetInformationBox;

/*OMA (P)DCF extensions*/
typedef struct
{
	GF_ISOM_FULL_BOX
	u8 EncryptionMethod;
	u8 PaddingScheme;
	u64 PlaintextLength;
	char *ContentID;
	char *RightsIssuerURL;
	char *TextualHeaders;
	u32 TextualHeadersLen;
} GF_OMADRMCommonHeaderBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u8 GKEncryptionMethod;
	char *GroupID;
	u16 GKLength;
	char *GroupKey;
} GF_OMADRMGroupIDBox;

typedef struct
{
	GF_ISOM_BOX
} GF_OMADRMMutableInformationBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	char TransactionID[16];
} GF_OMADRMTransactionTrackingBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u8 *oma_ro;
	u32 oma_ro_size;
} GF_OMADRMRightsObjectBox;

/*identical*/
typedef struct __isma_format_box GF_OMADRMAUFormatBox;

typedef struct __oma_kms_box
{
	GF_ISOM_FULL_BOX
	GF_OMADRMCommonHeaderBox *hdr;
	GF_OMADRMAUFormatBox *fmt;
} GF_OMADRMKMSBox;

typedef struct
{
	Bool reference_type;
	u32 reference_size;
	u32 subsegment_duration;
	Bool starts_with_SAP;
	u32 SAP_type;
	u32 SAP_delta_time;
} GF_SIDXReference;

typedef struct __sidx_box
{
	GF_ISOM_FULL_BOX

	u32 reference_ID;
	u32 timescale;
	u64 earliest_presentation_time;
	u64 first_offset;
	u32 nb_refs;
	GF_SIDXReference *refs;
} GF_SegmentIndexBox;

typedef struct
{
	u8 level;
	u32 range_size;
} GF_SubsegmentRangeInfo;

typedef struct
{
	u32 range_count;
	GF_SubsegmentRangeInfo *ranges;
} GF_SubsegmentInfo;

typedef struct __ssix_box
{
	GF_ISOM_FULL_BOX

	u32 subsegment_count;
	GF_SubsegmentInfo *subsegments;
} GF_SubsegmentIndexBox;

typedef struct
{
	GF_ISOTrackID track_id;
	Bool padding_flag;
	u8 type;
	u32 grouping_type;
	u32 grouping_type_parameter;
	GF_ISOTrackID sub_track_id;
} GF_LevelAssignment;

typedef struct __leva_box
{
	GF_ISOM_FULL_BOX

	u32 level_count;
	GF_LevelAssignment *levels;
} GF_LevelAssignmentBox;

typedef struct __pcrInfo_box
{
	GF_ISOM_BOX
	u32	subsegment_count;
	u64 *pcr_values;
} GF_PcrInfoBox;

#ifndef GPAC_DISABLE_ISOM_ADOBE

/*Adobe specific boxes*/
typedef struct
{
	u64 time;
	u64 offset;
} GF_AfraEntry;

typedef struct
{
	u64 time;
	u32 segment;
	u32 fragment;
	u64 afra_offset;
	u64 offset_from_afra;
} GF_GlobalAfraEntry;

typedef struct __adobe_frag_random_access_box
{
	GF_ISOM_FULL_BOX
	Bool long_ids;
	Bool long_offsets;
	Bool global_entries;
	u8 reserved;
	u32 time_scale;
	u32 entry_count;
	GF_List *local_access_entries;
	u32 global_entry_count;
	GF_List *global_access_entries;
} GF_AdobeFragRandomAccessBox;

typedef struct __adobe_bootstrap_info_box
{
	GF_ISOM_FULL_BOX
	u32 bootstrapinfo_version;
	u8 profile;
	Bool live;
	Bool update;
	u8 reserved;
	u32 time_scale;
	u64 current_media_time;
	u64 smpte_time_code_offset;
	char *movie_identifier;
	u8 server_entry_count;
	GF_List *server_entry_table;
	u8 quality_entry_count;
	GF_List *quality_entry_table;
	char *drm_data;
	char *meta_data;
	//entries in these two lists are NOT registered with the box child_boxes because of the inbetween 8 bits !!
	u8 segment_run_table_count;
	GF_List *segment_run_table_entries;
	u8 fragment_run_table_count;
	GF_List *fragment_run_table_entries;
} GF_AdobeBootstrapInfoBox;

typedef struct
{
	u32 first_segment;
	u32 fragment_per_segment;
} GF_AdobeSegmentRunEntry;

typedef struct __adobe_segment_run_table_box
{
	GF_ISOM_FULL_BOX
	u8 quality_entry_count;
	GF_List *quality_segment_url_modifiers;
	u32 segment_run_entry_count;
	GF_List *segment_run_entry_table;
} GF_AdobeSegmentRunTableBox;

typedef struct
{
	u32 first_fragment;
	u64 first_fragment_timestamp;
	u32 fragment_duration;
	u8 discontinuity_indicator;
} GF_AdobeFragmentRunEntry;

typedef struct __adobe_fragment_run_table_box
{
	GF_ISOM_FULL_BOX
	u32 timescale;
	u8 quality_entry_count;
	GF_List *quality_segment_url_modifiers;
	u32 fragment_run_entry_count;
	GF_List *fragment_run_entry_table;
} GF_AdobeFragmentRunTableBox;

#endif /*GPAC_DISABLE_ISOM_ADOBE*/


/***********************************************************
			Sample Groups
***********************************************************/
typedef struct
{
	u32 sample_count;
	u32 group_description_index;
} GF_SampleGroupEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 grouping_type;
	u32 grouping_type_parameter;

	u32 entry_count;
	GF_SampleGroupEntry *sample_entries;

} GF_SampleGroupBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 grouping_type;
	u32 default_length;

	u32 default_description_index;
	GF_List *group_descriptions;
} GF_SampleGroupDescriptionBox;

/*default entry */
typedef struct
{
	u32 length;
	u8 *data;
} GF_DefaultSampleGroupDescriptionEntry;

/*VisualRandomAccessEntry - 'rap ' type*/
typedef struct
{
	u8 num_leading_samples_known;
	u8 num_leading_samples;
} GF_VisualRandomAccessEntry;

/*RollRecoveryEntry - 'roll' and prol type*/
typedef struct
{
	s16 roll_distance;
} GF_RollRecoveryEntry;

/*TemporalLevelEntry - 'tele' type*/
typedef struct
{
	Bool level_independently_decodable;
} GF_TemporalLevelEntry;

/*SAPEntry - 'sap ' type*/
typedef struct
{
	Bool dependent_flag;
	u8 SAP_type;
} GF_SAPEntry;

/*SAPEntry - 'sync' type*/
typedef struct
{
	u8 NALU_type;
} GF_SYNCEntry;

/*Operating Points Information - 'oinf' type*/
typedef struct
{
	u16 scalability_mask;
	GF_List* profile_tier_levels;
	GF_List* operating_points;
	GF_List* dependency_layers;
} GF_OperatingPointsInformation;

GF_OperatingPointsInformation *gf_isom_oinf_new_entry();
void gf_isom_oinf_del_entry(void *entry);
GF_Err gf_isom_oinf_read_entry(void *entry, GF_BitStream *bs);
GF_Err gf_isom_oinf_write_entry(void *entry, GF_BitStream *bs);
u32 gf_isom_oinf_size_entry(void *entry);
Bool gf_isom_get_oinf_info(GF_ISOFile *file, u32 trackNumber, GF_OperatingPointsInformation **ptr);


/*Operating Points Information - 'oinf' type*/
typedef struct
{
	u8 layer_id;
	u8 min_TemporalId;
	u8 max_TemporalId;
	u8 sub_layer_presence_flags;
} LHVCLayerInfoItem;

typedef struct
{
	GF_List* num_layers_in_track;
} GF_LHVCLayerInformation;

GF_LHVCLayerInformation *gf_isom_linf_new_entry();
void gf_isom_linf_del_entry(void *entry);
GF_Err gf_isom_linf_read_entry(void *entry, GF_BitStream *bs);
GF_Err gf_isom_linf_write_entry(void *entry, GF_BitStream *bs);
u32 gf_isom_linf_size_entry(void *entry);
Bool gf_isom_get_linf_info(GF_ISOFile *file, u32 trackNumber, GF_LHVCLayerInformation **ptr);


#define MAX_LHEVC_LAYERS	64

typedef struct
{
	u8 general_profile_space, general_tier_flag, general_profile_idc, general_level_idc;
	u32 general_profile_compatibility_flags;
	u64 general_constraint_indicator_flags;
} LHEVC_ProfileTierLevel;

typedef struct
{
	u8 ptl_idx;
	u8 layer_id;
	Bool is_outputlayer, is_alternate_outputlayer;
} LHEVC_LayerInfo;

typedef struct
{
	u16 output_layer_set_idx;
	u8 max_temporal_id;
	u8 layer_count;
	LHEVC_LayerInfo layers_info[MAX_LHEVC_LAYERS];
	u16 minPicWidth, minPicHeight, maxPicWidth, maxPicHeight;
	u8 maxChromaFormat, maxBitDepth;
	Bool frame_rate_info_flag, bit_rate_info_flag;
	u16 avgFrameRate;
	u8 constantFrameRate;
	u32 maxBitRate, avgBitRate;
} LHEVC_OperatingPoint;


typedef struct
{
	u8 dependent_layerID;
	u8 num_layers_dependent_on;
	u8 dependent_on_layerID[MAX_LHEVC_LAYERS];
	u8 dimension_identifier[16];
} LHEVC_DependentLayer;



/*
		CENC stuff
*/

/*CENCSampleEncryptionGroupEntry - 'seig' type*/
typedef struct
{
	u8 crypt_byte_block, skip_byte_block;
	u8 IsProtected;
	u8 Per_Sample_IV_size;
	u8 constant_IV_size;
	bin128 KID;
	bin128 constant_IV;
} GF_CENCSampleEncryptionGroupEntry;

typedef struct
{
	GF_ISOM_FULL_BOX

	bin128 SystemID;
	u32 KID_count;
	bin128 *KIDs;
	u32 private_data_size;
	u8 *private_data;
} GF_ProtectionSystemHeaderBox;

typedef struct __cenc_tenc_box
{
	GF_ISOM_FULL_BOX

	u8 crypt_byte_block, skip_byte_block;
	u8 isProtected;
	u8 Per_Sample_IV_Size;
	bin128 KID;
	u8 constant_IV_size;
	bin128 constant_IV;
} GF_TrackEncryptionBox;

typedef struct __piff_tenc_box
{
	GF_ISOM_UUID_BOX
	/*u8 version; field in included in base box version */
	u32 flags;

	u32 AlgorithmID;
	u8 IV_size;
	bin128 KID;
} GF_PIFFTrackEncryptionBox;

typedef struct
{
	GF_ISOM_UUID_BOX
	/*u8 version; field in included in base box version */
	u32 flags;

	bin128 SystemID;
	u32 private_data_size;
	u8 *private_data;
} GF_PIFFProtectionSystemHeaderBox;


typedef struct __sample_encryption_box
{
	GF_ISOM_UUID_BOX
	/*u8 version; field in included in base box version */
	u32 flags;

	Bool is_piff;

	GF_List *samp_aux_info; /*GF_CENCSampleAuxInfo*/
	u64 bs_offset;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	/*pointer to container traf*/
	GF_TrackFragmentBox *traf;
#endif
	/*pointer to associated saio*/
	GF_SampleAuxiliaryInfoSizeBox *cenc_saiz;
	GF_SampleAuxiliaryInfoOffsetBox *cenc_saio;


	u32 AlgorithmID;
	u8 IV_size;
	bin128 KID;

} GF_SampleEncryptionBox;

typedef struct __traf_mss_timeext_box
{
	GF_ISOM_UUID_BOX
	/*u8 version; field in included in base box version */
	u32 flags;

	u64 absolute_time_in_track_timescale;
	u64 fragment_duration_in_track_timescale;
} GF_MSSTimeExtBox;

GF_SampleEncryptionBox *gf_isom_create_piff_psec_box(u8 version, u32 flags, u32 AlgorithmID, u8 IV_size, bin128 KID);
GF_SampleEncryptionBox * gf_isom_create_samp_enc_box(u8 version, u32 flags);

void gf_isom_cenc_get_default_info_ex(GF_TrackBox *trak, u32 sampleDescriptionIndex, u32 *container_type, Bool *default_IsEncrypted, u8 *default_IV_size, bin128 *default_KID, u8 *constant_IV_size, bin128 *constant_IV, u8 *crypt_byte_block, u8 *skip_byte_block);


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
GF_Err gf_isom_get_sample_cenc_info_ex(GF_TrackBox *trak, GF_TrackFragmentBox *traf, GF_SampleEncryptionBox *senc, u32 sample_number, Bool *IsEncrypted, u8 *IV_size, bin128 *KID, u8 *crypt_byte_block, u8 *skip_byte_block, u8 *constant_IV_size, bin128 *constant_IV);
GF_Err senc_Parse(GF_BitStream *bs, GF_TrackBox *trak, GF_TrackFragmentBox *traf, GF_SampleEncryptionBox *ptr);
#else
GF_Err gf_isom_get_sample_cenc_info_ex(GF_TrackBox *trak, void *traf, uGF_SampleEncryptionBox *senc, u32 sample_number, Bool *IsEncrypted, u8 *IV_size, bin128 *KID,
										u8 *crypt_byte_block, u8 *skip_byte_block, u8 *constant_IV_size, bin128 *constant_IV);
GF_Err senc_Parse(GF_BitStream *bs, GF_TrackBox *trak, void *traf, GF_SampleEncryptionBox *ptr);
#endif

/*
	Boxes for Adobe's protection scheme
*/
typedef struct __adobe_enc_info_box
{
	GF_ISOM_FULL_BOX
	char *enc_algo; /*spec: The encryption algorithm shall be 'AES-CBC'*/
	u8 key_length;
} GF_AdobeEncryptionInfoBox;

typedef struct __adobe_flash_access_params_box
{
	GF_ISOM_BOX
	u8 *metadata; /*base-64 encoded metadata used by the DRM client to retrieve decrypted key*/
} GF_AdobeFlashAccessParamsBox;

typedef struct __adobe_key_info_box
{
	GF_ISOM_FULL_BOX
	GF_AdobeFlashAccessParamsBox * params; /*spec: APSParamsBox will no longer be produced by conformaing applications*/
} GF_AdobeKeyInfoBox;

typedef struct __adobe_std_enc_params_box
{
	GF_ISOM_FULL_BOX
	GF_AdobeEncryptionInfoBox *enc_info;
	GF_AdobeKeyInfoBox *key_info;
} GF_AdobeStdEncryptionParamsBox;

typedef struct __adobe_drm_header_box
{
	GF_ISOM_FULL_BOX
	GF_AdobeStdEncryptionParamsBox *std_enc_params;
	//AdobeSignatureBox *signature; /*AdobeSignatureBox is not described*/
} GF_AdobeDRMHeaderBox;


typedef struct __adobe_drm_au_format_box
{
	GF_ISOM_FULL_BOX
	u8 selective_enc;
	u8 IV_length;
} GF_AdobeDRMAUFormatBox;

typedef struct __adobe_drm_key_management_system_box
{
	GF_ISOM_FULL_BOX
	GF_AdobeDRMHeaderBox *header;
	GF_AdobeDRMAUFormatBox *au_format;
} GF_AdobeDRMKeyManagementSystemBox;


typedef struct
{
	GF_ISOM_FULL_BOX
	GF_ISOTrackID refTrackID;
	u64 ntp, timestamp;
} GF_ProducerReferenceTimeBox;

/* Image File Format Structures */
typedef struct {
	GF_ISOM_FULL_BOX
	u32 image_width;
	u32 image_height;
} GF_ImageSpatialExtentsPropertyBox;

typedef struct {
	GF_ISOM_FULL_BOX
	u8 num_channels;
	u8 *bits_per_channel;
} GF_PixelInformationPropertyBox;

typedef struct {
	GF_ISOM_FULL_BOX
	u32 horizontal_offset;
	u32 vertical_offset;
} GF_RelativeLocationPropertyBox;

typedef struct {
	GF_ISOM_BOX
	u8 angle;
} GF_ImageRotationBox;

typedef struct {
	u32 item_id;
	GF_List *essential;
	GF_List *property_index;
} GF_ItemPropertyAssociationEntry;

typedef struct __item_association_box {
	GF_ISOM_FULL_BOX
	GF_List *entries;
} GF_ItemPropertyAssociationBox;


typedef struct {
	GF_ISOM_FULL_BOX
	char *aux_urn;
	u32 data_size;
	u8 *data;
} GF_AuxiliaryTypePropertyBox;

typedef struct {
	GF_ISOM_FULL_BOX

	GF_OperatingPointsInformation *oinf;
} GF_OINFPropertyBox;


typedef struct {
	GF_ISOM_FULL_BOX

	u16 target_ols_index;
} GF_TargetOLSPropertyBox;

/*flute hint track boxes*/
typedef struct
{
	u16 block_count;
	u32 block_size;
} FilePartitionEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 itemID;
	u16 packet_payload_size;
	u8 FEC_encoding_ID;
	u16 FEC_instance_ID;
	u16 max_source_block_length;
	u16 encoding_symbol_length;
	u16 max_number_of_encoding_symbols;
	char *scheme_specific_info;
	u32 nb_entries;
	FilePartitionEntry *entries;
} FilePartitionBox;

typedef struct
{
	u32 item_id;
	u32 symbol_count;
} FECReservoirEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	u32 nb_entries;
	FECReservoirEntry *entries;
} FECReservoirBox;

typedef struct
{
	u32 nb_groups;
	u32 *group_ids;
	u32 nb_channels;
	u32 *channels;
} SessionGroupEntry;

typedef struct
{
	GF_ISOM_BOX
	u16 num_session_groups;
	SessionGroupEntry *session_groups;
} FDSessionGroupBox;

typedef struct
{
	u32 group_id;
	char *name;
} GroupIdNameEntry;

typedef struct
{
	GF_ISOM_FULL_BOX
	u16 nb_entries;
	GroupIdNameEntry *entries;
} GroupIdToNameBox;


typedef struct
{
	u32 item_id;
	u32 symbol_count;
} FileReservoirEntry;


typedef struct
{
	GF_ISOM_FULL_BOX
	u32 nb_entries;
	FileReservoirEntry *entries;
} FileReservoirBox;

typedef struct
{
	GF_ISOM_BOX
	FilePartitionBox *blocks_and_symbols;
	FECReservoirBox *FEC_symbol_locations;
	FileReservoirBox *File_symbol_locations;
} FDPartitionEntryBox;

typedef struct
{
	GF_ISOM_FULL_BOX
	GF_List *partition_entries;
	FDSessionGroupBox *session_info;
	GroupIdToNameBox *group_id_to_name;
} FDItemInformationBox;


/*
		Data Map (media storage) stuff
*/

/*regular file IO*/
#define GF_ISOM_DATA_FILE         0x01
/*File Mapping object, read-only mode on complete files (no download)*/
#define GF_ISOM_DATA_FILE_MAPPING 0x02
/*External file object. Needs implementation*/
#define GF_ISOM_DATA_FILE_EXTERN  0x03
/*regular memory IO*/
#define GF_ISOM_DATA_MEM          0x04

/*Data Map modes*/
enum
{
	/*read mode*/
	GF_ISOM_DATA_MAP_READ = 1,
	/*write mode*/
	GF_ISOM_DATA_MAP_WRITE = 2,
	/*the following modes are just ways of signaling extended functionalities
	edit mode, to make sure the file is here, set to GF_ISOM_DATA_MAP_READ afterwards*/
	GF_ISOM_DATA_MAP_EDIT = 3,
	/*read-only access to the movie file: we create a file mapping object
	mode is set to GF_ISOM_DATA_MAP_READ afterwards*/
	GF_ISOM_DATA_MAP_READ_ONLY = 4,
	/*write-only access at the end of the movie - only used for movie fragments concatenation*/
	GF_ISOM_DATA_MAP_CAT = 5,
};

/*this is the DataHandler structure each data handler has its own bitstream*/
#define GF_ISOM_BASE_DATA_HANDLER	\
	u8	type;		\
	u64	curPos;		\
	u8	mode;		\
	GF_BitStream *bs;\
	u64 last_read_offset;\
	char *szName;

typedef struct __tag_data_map
{
	GF_ISOM_BASE_DATA_HANDLER
} GF_DataMap;

typedef struct
{
	GF_ISOM_BASE_DATA_HANDLER
	FILE *stream;
	Bool is_stdout;
	Bool last_acces_was_read;
#ifndef GPAC_DISABLE_ISOM_WRITE
	char *temp_file;
#endif
} GF_FileDataMap;

/*file mapping handler. used if supported, only on read mode for complete files  (not in file download)*/
typedef struct
{
	GF_ISOM_BASE_DATA_HANDLER
	char *name;
	u64 file_size;
	u8 *byte_map;
	u64 byte_pos;
} GF_FileMappingDataMap;

GF_Err gf_isom_datamap_new(const char *location, const char *parentPath, u8 mode, GF_DataMap **outDataMap);
void gf_isom_datamap_del(GF_DataMap *ptr);
GF_Err gf_isom_datamap_open(GF_MediaBox *minf, u32 dataRefIndex, u8 Edit);
void gf_isom_datamap_close(GF_MediaInformationBox *minf);
u32 gf_isom_datamap_get_data(GF_DataMap *map, u8 *buffer, u32 bufferLength, u64 Offset);

/*File-based data map*/
GF_DataMap *gf_isom_fdm_new(const char *sPath, u8 mode);
void gf_isom_fdm_del(GF_FileDataMap *ptr);
u32 gf_isom_fdm_get_data(GF_FileDataMap *ptr, u8 *buffer, u32 bufferLength, u64 fileOffset);

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_DataMap *gf_isom_fdm_new_temp(const char *sTempPath);
#endif

/*file-mapping, read only*/
GF_DataMap *gf_isom_fmo_new(const char *sPath, u8 mode);
void gf_isom_fmo_del(GF_FileMappingDataMap *ptr);
u32 gf_isom_fmo_get_data(GF_FileMappingDataMap *ptr, u8 *buffer, u32 bufferLength, u64 fileOffset);

#ifndef GPAC_DISABLE_ISOM_WRITE
u64 gf_isom_datamap_get_offset(GF_DataMap *map);
GF_Err gf_isom_datamap_add_data(GF_DataMap *ptr, u8 *data, u32 dataSize);
#endif

void gf_isom_datamap_flush(GF_DataMap *map);

/*
		Movie stuff
*/


/*time def for MP4/QT/MJ2K files*/
#define GF_ISOM_MAC_TIME_OFFSET 2082844800

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
#define GF_ISOM_FORMAT_FRAG_FLAGS(pad, sync, deg) ( ( (pad) << 17) | ( ( !(sync) ) << 16) | (deg) );
#define GF_ISOM_GET_FRAG_PAD(flag) ( (flag) >> 17) & 0x7
#define GF_ISOM_GET_FRAG_SYNC(flag) ( ! ( ( (flag) >> 16) & 0x1))
#define GF_ISOM_GET_FRAG_DEG(flag)	(flag) & 0x7FFF

#define GF_ISOM_GET_FRAG_LEAD(flag) ( (flag) >> 26) & 0x3
#define GF_ISOM_GET_FRAG_DEPENDS(flag) ( (flag) >> 24) & 0x3
#define GF_ISOM_GET_FRAG_DEPENDED(flag) ( (flag) >> 22) & 0x3
#define GF_ISOM_GET_FRAG_REDUNDANT(flag) ( (flag) >> 20) & 0x3

#define GF_ISOM_GET_FRAG_DEPEND_FLAGS(lead, depends, depended, redundant) ( (lead<<26) | (depends<<24) | (depended<<22) | (redundant<<20) )
#define GF_ISOM_RESET_FRAG_DEPEND_FLAGS(flags) flags = flags & 0xFFFFF

GF_TrackExtendsBox *GetTrex(GF_MovieBox *moov, GF_ISOTrackID TrackID);
#endif

enum
{
	GF_ISOM_FRAG_WRITE_READY	=	0x01,
	GF_ISOM_FRAG_READ_DEBUG		=	0x02,
};

/*this is our movie object*/
struct __tag_isom {
	/*the last fatal error*/
	GF_Err LastError;
	/*the original filename*/
	char *fileName;
	/*the original file in read/edit, and also used in fragments mode
	once the first moov has been written
	Nota: this API doesn't allow fragments BEFORE the MOOV in order
	to make easily parsable files (note there could be some data (mdat) before
	the moov*/
	GF_DataMap *movieFileMap;

#ifndef GPAC_DISABLE_ISOM_WRITE
	/*the final file name*/
	char *finalName;
	/*the file where we store edited samples (for READ_WRITE and WRITE mode only)*/
	GF_DataMap *editFileMap;
	/*the interleaving time for dummy mode (in movie TimeScale)*/
	u32 interleavingTime;
	GF_ISOTrackID last_created_track_id;
#endif

	u8 openMode;
	u8 storageMode;
	/*if true 3GPP text streams are read as MPEG-4 StreamingText*/
	u8 convert_streaming_text;
	u8 is_jp2;
	u8 force_co64;

	Bool keep_utc;
	/*main boxes for fast access*/
	/*moov*/
	GF_MovieBox *moov;
	/*our MDAT box (one and only one when we store the file)*/
	GF_MediaDataBox *mdat;
	/*file brand (since v2, NULL means mp4 v1)*/
	GF_FileTypeBox *brand;
	/*progressive download info*/
	GF_ProgressiveDownloadBox *pdin;
	/*meta box if any*/
	GF_MetaBox *meta;

	u64 read_byte_offset;

	void (*progress_cbk)(void *udta, u64 nb_done, u64 nb_total);
	void *progress_cbk_udta;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	u32 FragmentsFlags, NextMoofNumber;
	/*active fragment*/
	GF_MovieFragmentBox *moof;
	/*in WRITE mode, this is the current MDAT where data is written*/
	/*in READ mode this is the last valid file position before a gf_isom_box_read failed*/
	u64 current_top_box_start;
	u64 segment_start;

	GF_List *moof_list;
	Bool use_segments, moof_first, append_segment, styp_written, force_moof_base_offset;

	/*used when building single-indexed self initializing media segments*/
	GF_SegmentIndexBox *root_sidx;
	u64 root_sidx_offset;
	u32 root_sidx_index;
	Bool dyn_root_sidx;
	GF_SubsegmentIndexBox *root_ssix;

	Bool is_index_segment;

	GF_BitStream *segment_bs;
	/* 0: no moof found yet, 1: 1 moof found, 2: next moof found */
	Bool single_moof_mode;
	u32 single_moof_state;

	Bool sample_groups_in_traf;

	/* optional mfra box used in write mode */
	GF_MovieFragmentRandomAccessBox *mfra;

#endif
	GF_ProducerReferenceTimeBox *last_producer_ref_time;

	/*this contains ALL the root boxes excepts fragments*/
	GF_List *TopBoxes;

	/*default track for sync of MPEG4 streams - this is the first accessed stream without OCR info - only set in READ mode*/
	s32 es_id_default_sync;

	Bool is_smooth;

	GF_Err (*on_block_out)(void *usr_data, u8 *block, u32 block_size);
	GF_Err (*on_block_patch)(void *usr_data, u8 *block, u32 block_size, u64 block_offset);
	void *on_block_out_usr_data;
	u32 on_block_out_block_size;
	//in block disptach mode we don't have the full file, keep the position
	u64 fragmented_file_pos;

	u32 nb_box_init_seg;
};

/*time function*/
u64 gf_isom_get_mp4time();
/*set the last error of the file. if file is NULL, set the static error (used for IO errors*/
void gf_isom_set_last_error(GF_ISOFile *the_file, GF_Err error);
GF_Err gf_isom_parse_movie_boxes(GF_ISOFile *mov, u64 *bytesMissing, Bool progressive_mode);
GF_ISOFile *gf_isom_new_movie();
/*Movie and Track access functions*/
GF_TrackBox *gf_isom_get_track_from_file(GF_ISOFile *the_file, u32 trackNumber);
GF_TrackBox *gf_isom_get_track(GF_MovieBox *moov, u32 trackNumber);
GF_TrackBox *gf_isom_get_track_from_id(GF_MovieBox *moov, GF_ISOTrackID trackID);
GF_TrackBox *gf_isom_get_track_from_original_id(GF_MovieBox *moov, u32 originalID, u32 originalFile);
u32 gf_isom_get_tracknum_from_id(GF_MovieBox *moov, GF_ISOTrackID trackID);
/*open a movie*/
GF_ISOFile *gf_isom_open_file(const char *fileName, u32 OpenMode, const char *tmp_dir);
/*close and delete a movie*/
void gf_isom_delete_movie(GF_ISOFile *mov);

GF_Err gf_isom_set_write_callback(GF_ISOFile *mov,
 			GF_Err (*on_block_out)(void *cbk, u8 *data, u32 block_size),
			GF_Err (*on_block_patch)(void *usr_data, u8 *block, u32 block_size, u64 block_offset),
 			void *usr_data,
 			u32 block_size);

/*StreamDescription reconstruction Functions*/
GF_Err GetESD(GF_MovieBox *moov, GF_ISOTrackID trackID, u32 StreamDescIndex, GF_ESD **outESD);
GF_Err GetESDForTime(GF_MovieBox *moov, GF_ISOTrackID trackID, u64 CTS, GF_ESD **outESD);
GF_Err Media_GetSampleDesc(GF_MediaBox *mdia, u32 SampleDescIndex, GF_SampleEntryBox **out_entry, u32 *dataRefIndex);
GF_Err Media_GetSampleDescIndex(GF_MediaBox *mdia, u64 DTS, u32 *sampleDescIndex);
/*get esd for given sample desc -
	@true_desc_only: if true doesn't emulate desc and returns native ESD,
				otherwise emulates if needed/possible (TimedText) and return a hard copy of the desc
*/
GF_Err Media_GetESD(GF_MediaBox *mdia, u32 sampleDescIndex, GF_ESD **esd, Bool true_desc_only);
Bool Track_IsMPEG4Stream(u32 HandlerType);
Bool IsMP4Description(u32 entryType);
/*Find a reference of a given type*/
GF_Err Track_FindRef(GF_TrackBox *trak, u32 ReferenceType, GF_TrackReferenceTypeBox **dpnd);
/*Time and sample*/
GF_Err GetMediaTime(GF_TrackBox *trak, Bool force_non_empty, u64 movieTime, u64 *MediaTime, s64 *SegmentStartTime, s64 *MediaOffset, u8 *useEdit, u64 *next_edit_start_plus_one);
GF_Err Media_GetSample(GF_MediaBox *mdia, u32 sampleNumber, GF_ISOSample **samp, u32 *sampleDescriptionIndex, Bool no_data, u64 *out_offset);
GF_Err Media_CheckDataEntry(GF_MediaBox *mdia, u32 dataEntryIndex);
GF_Err Media_FindSyncSample(GF_SampleTableBox *stbl, u32 searchFromTime, u32 *sampleNumber, u8 mode);
GF_Err Media_RewriteODFrame(GF_MediaBox *mdia, GF_ISOSample *sample);
GF_Err Media_FindDataRef(GF_DataReferenceBox *dref, char *URLname, char *URNname, u32 *dataRefIndex);
Bool Media_IsSelfContained(GF_MediaBox *mdia, u32 StreamDescIndex);

typedef enum
{
	ISOM_DREF_MIXED = 0,
	ISOM_DREF_SELF,
	ISOM_DREF_EXT,
} GF_ISOMDataRefAllType;
GF_ISOMDataRefAllType Media_SelfContainedType(GF_MediaBox *mdia);

GF_TrackBox *GetTrackbyID(GF_MovieBox *moov, GF_ISOTrackID TrackID);

/*check the TimeToSample for the given time and return the Sample number
if the entry is not found, return the closest sampleNumber in prevSampleNumber and 0 in sampleNumber
if the DTS required is after all DTSs in the list, set prevSampleNumber and SampleNumber to 0
useCTS specifies that we're looking for a composition time
*/
GF_Err stbl_findEntryForTime(GF_SampleTableBox *stbl, u64 DTS, u8 useCTS, u32 *sampleNumber, u32 *prevSampleNumber);
/*Reading of the sample tables*/
GF_Err stbl_GetSampleSize(GF_SampleSizeBox *stsz, u32 SampleNumber, u32 *Size);
GF_Err stbl_GetSampleCTS(GF_CompositionOffsetBox *ctts, u32 SampleNumber, s32 *CTSoffset);
GF_Err stbl_GetSampleDTS(GF_TimeToSampleBox *stts, u32 SampleNumber, u64 *DTS);
GF_Err stbl_GetSampleDTS_and_Duration(GF_TimeToSampleBox *stts, u32 SampleNumber, u64 *DTS, u32 *duration);

/*find a RAP or set the prev / next RAPs if vars are passed*/
GF_Err stbl_GetSampleRAP(GF_SyncSampleBox *stss, u32 SampleNumber, SAPType *IsRAP, u32 *prevRAP, u32 *nextRAP);
/*same as above but only look for open-gop RAPs and GDR (roll)*/
GF_Err stbl_SearchSAPs(GF_SampleTableBox *stbl, u32 SampleNumber, SAPType *IsRAP, u32 *prevRAP, u32 *nextRAP);
GF_Err stbl_GetSampleInfos(GF_SampleTableBox *stbl, u32 sampleNumber, u64 *offset, u32 *chunkNumber, u32 *descIndex, GF_StscEntry **scsc_entry);
GF_Err stbl_GetSampleShadow(GF_ShadowSyncBox *stsh, u32 *sampleNumber, u32 *syncNum);
GF_Err stbl_GetPaddingBits(GF_PaddingBitsBox *padb, u32 SampleNumber, u8 *PadBits);
GF_Err stbl_GetSampleDepType(GF_SampleDependencyTypeBox *stbl, u32 SampleNumber, u32 *isLeading, u32 *dependsOn, u32 *dependedOn, u32 *redundant);


/*unpack sample2chunk and chunk offset so that we have 1 sample per chunk (edition mode only)*/
GF_Err stbl_UnpackOffsets(GF_SampleTableBox *stbl);
GF_Err stbl_unpackCTS(GF_SampleTableBox *stbl);
GF_Err SetTrackDuration(GF_TrackBox *trak);
GF_Err Media_SetDuration(GF_TrackBox *trak);

/*rewrites 3GP samples desc as MPEG-4 ESD*/
GF_Err gf_isom_get_ttxt_esd(GF_MediaBox *mdia, GF_ESD **out_esd);
/*inserts TTU header - only used when conversion to StreamingText is on*/
GF_Err gf_isom_rewrite_text_sample(GF_ISOSample *samp, u32 sampleDescriptionIndex, u32 sample_dur);

GF_UserDataMap *udta_getEntry(GF_UserDataBox *ptr, u32 box_type, bin128 *uuid);

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err FlushCaptureMode(GF_ISOFile *movie);
GF_Err CanAccessMovie(GF_ISOFile *movie, u32 Mode);
GF_ISOFile *gf_isom_create_movie(const char *fileName, u32 OpenMode, const char *tmp_dir);
void gf_isom_insert_moov(GF_ISOFile *file);

GF_Err WriteToFile(GF_ISOFile *movie, Bool for_fragments);
GF_Err Track_SetStreamDescriptor(GF_TrackBox *trak, u32 StreamDescriptionIndex, u32 DataReferenceIndex, GF_ESD *esd, u32 *outStreamIndex);
u8 RequestTrack(GF_MovieBox *moov, GF_ISOTrackID TrackID);
/*Track-Media setup*/
GF_Err NewMedia(GF_MediaBox **mdia, u32 MediaType, u32 TimeScale);
GF_Err Media_ParseODFrame(GF_MediaBox *mdia, const GF_ISOSample *sample, GF_ISOSample **od_samp);
GF_Err Media_AddSample(GF_MediaBox *mdia, u64 data_offset, const GF_ISOSample *sample, u32 StreamDescIndex, u32 syncShadowNumber);
GF_Err Media_CreateDataRef(GF_ISOFile *file, GF_DataReferenceBox *dref, char *URLname, char *URNname, u32 *dataRefIndex);
GF_Err Media_SetDrefURL(GF_DataEntryURLBox *dref_entry, const char *origName, const char *finalName);

/*update a media sample. ONLY in edit mode*/
GF_Err Media_UpdateSample(GF_MediaBox *mdia, u32 sampleNumber, GF_ISOSample *sample, Bool data_only);
GF_Err Media_UpdateSampleReference(GF_MediaBox *mdia, u32 sampleNumber, GF_ISOSample *sample, u64 data_offset);
/*addition in the sample tables*/
GF_Err stbl_AddDTS(GF_SampleTableBox *stbl, u64 DTS, u32 *sampleNumber, u32 LastAUDefDuration, u32 nb_pack_samples);
GF_Err stbl_AddCTS(GF_SampleTableBox *stbl, u32 sampleNumber, s32 CTSoffset);
GF_Err stbl_AddSize(GF_SampleSizeBox *stsz, u32 sampleNumber, u32 size, u32 nb_pack_samples);
GF_Err stbl_AddRAP(GF_SyncSampleBox *stss, u32 sampleNumber);
GF_Err stbl_AddShadow(GF_ShadowSyncBox *stsh, u32 sampleNumber, u32 shadowNumber);
GF_Err stbl_AddChunkOffset(GF_MediaBox *mdia, u32 sampleNumber, u32 StreamDescIndex, u64 offset, u32 nb_pack_samples);
/*NB - no add for padding, this is done only through SetPaddingBits*/

GF_Err stbl_AddSampleFragment(GF_SampleTableBox *stbl, u32 sampleNumber, u16 size);

/*update of the sample table
all these functions are called in edit and we always have 1 sample per chunk*/
GF_Err stbl_SetChunkOffset(GF_MediaBox *mdia, u32 sampleNumber, u64 offset);
GF_Err stbl_SetSampleCTS(GF_SampleTableBox *stbl, u32 sampleNumber, s32 offset);
GF_Err stbl_SetSampleSize(GF_SampleSizeBox *stsz, u32 SampleNumber, u32 size);
GF_Err stbl_SetSampleRAP(GF_SyncSampleBox *stss, u32 SampleNumber, u8 isRAP);
GF_Err stbl_SetSyncShadow(GF_ShadowSyncBox *stsh, u32 sampleNumber, u32 syncSample);
GF_Err stbl_SetPaddingBits(GF_SampleTableBox *stbl, u32 SampleNumber, u8 bits);
/*for adding fragmented samples*/
GF_Err stbl_SampleSizeAppend(GF_SampleSizeBox *stsz, u32 data_size);
/*writing of the final chunk info in edit mode*/
GF_Err stbl_SetChunkAndOffset(GF_SampleTableBox *stbl, u32 sampleNumber, u32 StreamDescIndex, GF_SampleToChunkBox *the_stsc, GF_Box **the_stco, u64 data_offset, Bool forceNewChunk, u32 nb_samp);
/*EDIT LIST functions*/
GF_EdtsEntry *CreateEditEntry(u64 EditDuration, u64 MediaTime, u8 EditMode);

GF_Err stbl_SetRedundant(GF_SampleTableBox *stbl, u32 sampleNumber);
GF_Err stbl_AddRedundant(GF_SampleTableBox *stbl, u32 sampleNumber);

/*REMOVE functions*/
GF_Err stbl_RemoveDTS(GF_SampleTableBox *stbl, u32 sampleNumber, u32 LastAUDefDuration);
GF_Err stbl_RemoveCTS(GF_SampleTableBox *stbl, u32 sampleNumber);
GF_Err stbl_RemoveSize(GF_SampleSizeBox *stsz, u32 sampleNumber);
GF_Err stbl_RemoveChunk(GF_SampleTableBox *stbl, u32 sampleNumber);
GF_Err stbl_RemoveRAP(GF_SampleTableBox *stbl, u32 sampleNumber);
GF_Err stbl_RemoveShadow(GF_ShadowSyncBox *stsh, u32 sampleNumber);
GF_Err stbl_RemovePaddingBits(GF_SampleTableBox *stbl, u32 SampleNumber);
GF_Err stbl_RemoveSampleFragments(GF_SampleTableBox *stbl, u32 sampleNumber);
GF_Err stbl_RemoveRedundant(GF_SampleTableBox *stbl, u32 SampleNumber);
GF_Err stbl_RemoveSubSample(GF_SampleTableBox *stbl, u32 SampleNumber);
GF_Err stbl_RemoveSampleGroup(GF_SampleTableBox *stbl, u32 SampleNumber);

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
GF_Err gf_isom_close_fragments(GF_ISOFile *movie);
#endif

Bool gf_isom_is_identical_sgpd(void *ptr1, void *ptr2, u32 grouping_type);

GF_Err gf_isom_flush_sidx(GF_ISOFile *movie, u32 sidx_max_size);

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_DefaultSampleGroupDescriptionEntry * gf_isom_get_sample_group_info_entry(GF_ISOFile *the_file, GF_TrackBox *trak, u32 grouping_type, u32 sample_description_index, u32 *default_index, GF_SampleGroupDescriptionBox **out_sgdp);

GF_Err GetNextMediaTime(GF_TrackBox *trak, u64 movieTime, u64 *OutMovieTime);
GF_Err GetPrevMediaTime(GF_TrackBox *trak, u64 movieTime, u64 *OutMovieTime);

Bool IsHintTrack(GF_TrackBox *trak);
Bool CheckHintFormat(GF_TrackBox *trak, u32 HintType);
u32 GetHintFormat(GF_TrackBox *trak);

/*locate a box by its type or UUID*/
GF_ItemListBox *gf_ismo_locate_box(GF_List *list, u32 boxType, bin128 UUID);

GF_Err moov_on_child_box(GF_Box *ptr, GF_Box *a);
GF_Err trak_on_child_box(GF_Box *ptr, GF_Box *a);
GF_Err mvex_on_child_box(GF_Box *ptr, GF_Box *a);
GF_Err stsd_on_child_box(GF_Box *ptr, GF_Box *a);
GF_Err hnti_on_child_box(GF_Box *hnti, GF_Box *a);
GF_Err udta_on_child_box(GF_Box *ptr, GF_Box *a);
GF_Err edts_on_child_box(GF_Box *s, GF_Box *a);
GF_Err stdp_box_read(GF_Box *s, GF_BitStream *bs);
GF_Err stbl_on_child_box(GF_Box *ptr, GF_Box *a);
GF_Err sdtp_box_read(GF_Box *s, GF_BitStream *bs);
GF_Err dinf_on_child_box(GF_Box *s, GF_Box *a);
GF_Err minf_on_child_box(GF_Box *s, GF_Box *a);
GF_Err mdia_on_child_box(GF_Box *s, GF_Box *a);
GF_Err traf_on_child_box(GF_Box *s, GF_Box *a);

/*rewrites avcC based on the given esd - this destroys the esd*/
GF_Err AVC_HEVC_UpdateESD(GF_MPEGVisualSampleEntryBox *avc, GF_ESD *esd);
void AVC_RewriteESDescriptorEx(GF_MPEGVisualSampleEntryBox *avc, GF_MediaBox *mdia);
void AVC_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *avc);
void HEVC_RewriteESDescriptorEx(GF_MPEGVisualSampleEntryBox *hevc, GF_MediaBox *mdia);
void HEVC_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *hevc);
void VP9_RewriteESDescriptorEx(GF_MPEGVisualSampleEntryBox *vp9, GF_MediaBox *mdia);
void VP9_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *vp9);
void AV1_RewriteESDescriptorEx(GF_MPEGVisualSampleEntryBox *av1, GF_MediaBox *mdia);
void AV1_RewriteESDescriptor(GF_MPEGVisualSampleEntryBox *av1);
GF_Err reftype_AddRefTrack(GF_TrackReferenceTypeBox *ref, GF_ISOTrackID trackID, u16 *outRefIndex);
GF_XMLBox *gf_isom_get_meta_xml(GF_ISOFile *file, Bool root_meta, u32 track_num, Bool *is_binary);
Bool gf_isom_cenc_has_saiz_saio_track(GF_SampleTableBox *stbl, u32 scheme_type);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
Bool gf_isom_cenc_has_saiz_saio_traf(GF_TrackFragmentBox *traf, u32 scheme_type);
void gf_isom_cenc_set_saiz_saio(GF_SampleEncryptionBox *senc, GF_SampleTableBox *stbl, GF_TrackFragmentBox  *traf, u32 len, Bool saio_32bits);
#endif
void gf_isom_cenc_merge_saiz_saio(GF_SampleEncryptionBox *senc, GF_SampleTableBox *stbl, u64 offset, u32 len);

void gf_isom_parse_trif_info(const u8 *data, u32 size, u32 *id, u32 *independent, Bool *full_picture, u32 *x, u32 *y, u32 *w, u32 *h);

Bool gf_isom_is_encrypted_entry(u32 entryType);

#ifndef GPAC_DISABLE_ISOM_HINTING

/*
		Hinting stuff
*/

/*****************************************************
		RTP Data Entries
*****************************************************/

typedef struct
{
	u8 sender_current_time_present;
	u8 expected_residual_time_present;
	u8 session_close_bit;
	u8 object_close_bit;
	u16 transport_object_identifier;
} GF_LCTheaderTemplate;

typedef struct
{
	u8 header_extension_type;
	u8 content[3];
	u32 data_length;
	u8 *data;
} GF_LCTheaderExtension;

typedef struct
{
	GF_ISOM_BOX

	GF_LCTheaderTemplate info;
	u16 header_ext_count;
	GF_LCTheaderExtension *headers;

	GF_List *constructors;
} GF_FDpacketBox;


typedef struct
{
	GF_ISOM_BOX

	u8 FEC_encoding_ID;
	u16 FEC_instance_ID;
	u16 source_block_number;
	u16 encoding_symbol_ID;
} GF_FECInformationBox;


typedef struct
{
	GF_ISOM_BOX
	//not registered with child list !!
	GF_FECInformationBox *feci;
	u32 data_length;
	u8 *data;
} GF_ExtraDataBox;


#define GF_ISMO_BASE_DTE_ENTRY	\
	u8 source;

typedef struct
{
	GF_ISMO_BASE_DTE_ENTRY
} GF_GenericDTE;

typedef struct
{
	GF_ISMO_BASE_DTE_ENTRY
} GF_EmptyDTE;

typedef struct
{
	GF_ISMO_BASE_DTE_ENTRY
	u8 dataLength;
	char data[14];
} GF_ImmediateDTE;

typedef struct
{
	GF_ISMO_BASE_DTE_ENTRY
	s8 trackRefIndex;
	u32 sampleNumber;
	u16 dataLength;
	u32 byteOffset;
	u16 bytesPerComp;
	u16 samplesPerComp;
} GF_SampleDTE;

typedef struct
{
	GF_ISMO_BASE_DTE_ENTRY
	s8 trackRefIndex;
	u32 streamDescIndex;
	u16 dataLength;
	u32 byteOffset;
	u32 reserved;
} GF_StreamDescDTE;

GF_GenericDTE *NewDTE(u8 type);
void DelDTE(GF_GenericDTE *dte);
GF_Err ReadDTE(GF_GenericDTE *dte, GF_BitStream *bs);
GF_Err WriteDTE(GF_GenericDTE *dte, GF_BitStream *bs);
GF_Err OffsetDTE(GF_GenericDTE *dte, u32 offset, u32 HintSampleNumber);

/*****************************************************
		RTP Sample
*****************************************************/

/*data cache when reading*/
typedef struct __tag_hint_data_cache
{
	GF_ISOSample *samp;
	GF_TrackBox *trak;
	u32 sample_num;
} GF_HintDataCache;

typedef struct __tag_hint_sample
{
	//for samples deriving from box
	GF_ISOM_BOX

	/*contains 4cc of hint track sample entry*/
	u32 hint_subtype;
	u16 packetCount;
	u16 reserved;
	GF_List *packetTable;
	u8 *AdditionalData;
	u32 dataLength;
	/*used internally for hinting*/
	u64 TransmissionTime;
	/*for read only, used to store samples fetched while building packets*/
	GF_List *sample_cache;

	//for dump
	GF_ISOTrackID trackID;
	u32 sampleNumber;

	GF_ExtraDataBox *extra_data;
} GF_HintSample;

GF_HintSample *gf_isom_hint_sample_new(u32 ProtocolType);
void gf_isom_hint_sample_del(GF_HintSample *ptr);
GF_Err gf_isom_hint_sample_read(GF_HintSample *ptr, GF_BitStream *bs, u32 sampleSize);
GF_Err gf_isom_hint_sample_write(GF_HintSample *ptr, GF_BitStream *bs);
u32 gf_isom_hint_sample_size(GF_HintSample *ptr);


/*****************************************************
		Hint Packets (generic packet for future protocol support)
*****************************************************/
#define GF_ISOM_BASE_PACKET			\
	u32 hint_subtype, sampleNumber;	\
	GF_ISOTrackID trackID;\
	s32 relativeTransTime;


typedef struct
{
	GF_ISOM_BASE_PACKET
} GF_HintPacket;

GF_HintPacket *gf_isom_hint_pck_new(u32 HintType);
void gf_isom_hint_pck_del(GF_HintPacket *ptr);
GF_Err gf_isom_hint_pck_read(GF_HintPacket *ptr, GF_BitStream *bs);
GF_Err gf_isom_hint_pck_write(GF_HintPacket *ptr, GF_BitStream *bs);
u32 gf_isom_hint_pck_size(GF_HintPacket *ptr);
GF_Err gf_isom_hint_pck_offset(GF_HintPacket *ptr, u32 offset, u32 HintSampleNumber);
GF_Err gf_isom_hint_pck_add_dte(GF_HintPacket *ptr, GF_GenericDTE *dte, u8 AtBegin);
/*get the size of the packet AS RECONSTRUCTED BY THE SERVER (without CSRC)*/
u32 gf_isom_hint_pck_length(GF_HintPacket *ptr);

/*the RTP packet*/
typedef struct
{
	GF_ISOM_BASE_PACKET

	/*RTP Header*/
	u8 P_bit;
	u8 X_bit;
	u8 M_bit;
	/*on 7 bits */
	u8 payloadType;
	u16 SequenceNumber;
	/*Hinting flags*/
	u8 B_bit;
	u8 R_bit;
	/*ExtraInfos TLVs - not really used */
	GF_List *TLV;
	/*DataTable - contains the DTEs...*/
	GF_List *DataTable;
} GF_RTPPacket;

GF_RTPPacket *gf_isom_hint_rtp_new();
void gf_isom_hint_rtp_del(GF_RTPPacket *ptr);
GF_Err gf_isom_hint_rtp_read(GF_RTPPacket *ptr, GF_BitStream *bs);
GF_Err gf_isom_hint_rtp_write(GF_RTPPacket *ptr, GF_BitStream *bs);
u32 gf_isom_hint_rtp_size(GF_RTPPacket *ptr);
GF_Err gf_isom_hint_rtp_offset(GF_RTPPacket *ptr, u32 offset, u32 HintSampleNumber);
u32 gf_isom_hint_rtp_length(GF_RTPPacket *ptr);


/*the RTP packet*/
typedef struct
{
	GF_ISOM_BASE_PACKET

	//RTCP report
	u8 Version, Padding, Count, PayloadType;
	u32 length;
	u8 *data;
} GF_RTCPPacket;

GF_RTCPPacket *gf_isom_hint_rtcp_new();
void gf_isom_hint_rtcp_del(GF_RTCPPacket *ptr);
GF_Err gf_isom_hint_rtcp_read(GF_RTCPPacket *ptr, GF_BitStream *bs);
GF_Err gf_isom_hint_rtcp_write(GF_RTCPPacket *ptr, GF_BitStream *bs);
u32 gf_isom_hint_rtcp_size(GF_RTCPPacket *ptr);
u32 gf_isom_hint_rtcp_length(GF_RTCPPacket *ptr);


#endif


struct _3gpp_text_sample
{
	char *text;
	u32 len;

	GF_TextStyleBox *styles;
	/*at most one of these*/
	GF_TextHighlightColorBox *highlight_color;
	GF_TextScrollDelayBox *scroll_delay;
	GF_TextBoxBox *box;
	GF_TextWrapBox *wrap;

	GF_List *others;
	GF_TextKaraokeBox *cur_karaoke;
};

GF_TextSample *gf_isom_parse_texte_sample(GF_BitStream *bs);
GF_TextSample *gf_isom_parse_texte_sample_from_data(u8 *data, u32 dataLength);

struct _generic_subtitle_sample
{
	char *text;
	u32 len;
};
GF_GenericSubtitleSample *gf_isom_parse_generic_subtitle_sample(GF_BitStream *bs);
GF_GenericSubtitleSample *gf_isom_parse_generic_subtitle_sample_from_data(u8 *data, u32 dataLength);


/*do not throw fatal errors if boxes are duplicated, just warn and remove extra ones*/
#define ERROR_ON_DUPLICATED_BOX(__abox, __parent) {	\
		char __ptype[5];\
		strcpy(__ptype, gf_4cc_to_str(__parent->type) );\
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] extra box %s found in %s, deleting\n", gf_4cc_to_str(__abox->type), __ptype)); \
		gf_isom_box_del_parent(& (__parent->child_boxes), __abox);\
		return GF_OK;\
	}


#ifndef GPAC_DISABLE_VTT

GF_Err gf_isom_update_webvtt_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex, const char *config);
GF_ISOSample *gf_isom_webvtt_to_sample(void *samp);

typedef struct
{
	GF_ISOM_BOX
	char *string;
} GF_StringBox;

typedef struct
{
	GF_ISOM_SAMPLE_ENTRY_FIELDS
	GF_StringBox *config;
} GF_WebVTTSampleEntryBox;

GF_WebVTTSampleEntryBox *gf_webvtt_isom_get_description(GF_ISOFile *movie, u32 trackNumber, u32 descriptionIndex);

#endif /* GPAC_DISABLE_VTT */

//exported for sgpd comparison in traf merge
void sgpd_write_entry(u32 grouping_type, void *entry, GF_BitStream *bs);
Bool gf_isom_box_equal(GF_Box *a, GF_Box *b);
GF_Box *gf_isom_clone_config_box(GF_Box *box);

GF_Err gf_isom_box_dump(void *ptr, FILE * trace);
GF_Err gf_isom_box_array_dump(GF_List *list, FILE * trace);

void gf_isom_registry_disable(u32 boxCode, Bool disable);

/*Apple extensions*/
GF_MetaBox *gf_isom_apple_get_meta_extensions(GF_ISOFile *mov);

#ifndef GPAC_DISABLE_ISOM_WRITE
GF_MetaBox *gf_isom_apple_create_meta_extensions(GF_ISOFile *mov);
#endif /*GPAC_DISABLE_ISOM_WRITE*/


#ifndef GPAC_DISABLE_ISOM_DUMP
GF_Err gf_isom_box_dump_ex(void *ptr, FILE * trace, u32 box_4cc);
GF_Err gf_isom_box_dump_start(GF_Box *a, const char *name, FILE * trace);
void gf_isom_box_dump_done(const char *name, GF_Box *ptr, FILE *trace);
Bool gf_isom_box_is_file_level(GF_Box *s);
#endif

GF_Box *boxstring_new_with_data(u32 type, const char *string, GF_List **parent);

GF_Err gf_isom_read_null_terminated_string(GF_Box *s, GF_BitStream *bs, u64 size, char **out_str);

#endif //GPAC_DISABLE_ISOM

#ifdef __cplusplus
}
#endif

#endif //_GF_ISOMEDIA_DEV_H_

