/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / SL header file
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

#ifndef _GF_SYNC_LAYER_H_
#define _GF_SYNC_LAYER_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
 *	\file <gpac/sync_layer.h>
 *	\brief MPEG-4 Object Descriptor Framework Sync Layer.
 */
	
/*!
 *	\ingroup odf_grp
 *	\brief MPEG-4 Object Descriptor Framework  Sync Layer
 *
 *This section documents the MPEG-4 OD  Sync Layer used in GPAC.
 *	@{
 */

/*the Sync Layer config descriptor*/
typedef struct
{
	/*base descriptor*/
	u8 tag;

	u8 predefined;
	u8 useAccessUnitStartFlag;
	u8 useAccessUnitEndFlag;
	u8 useRandomAccessPointFlag;
	u8 hasRandomAccessUnitsOnlyFlag;
	u8 usePaddingFlag;
	u8 useTimestampsFlag;
	u8 useIdleFlag;
	u8 durationFlag;
	u32 timestampResolution;
	u32 OCRResolution;
	u8 timestampLength;
	u8 OCRLength;
	u8 AULength;
	u8 instantBitrateLength;
	u8 degradationPriorityLength;
	u8 AUSeqNumLength;
	u8 packetSeqNumLength;
	u32 timeScale;
	u16 AUDuration;
	u16 CUDuration;
	u64 startDTS;
	u64 startCTS;
	//internal
	Bool no_dts_signaling;
	u32 carousel_version;
} GF_SLConfig;

/***************************************
			SLConfig Tag
***************************************/
enum
{
	SLPredef_Null = 0x01,
	SLPredef_MP4 = 0x02,
};

/*set SL predefined (assign all fields according to sl->predefined value)*/
GF_Err gf_odf_slc_set_pref(GF_SLConfig *sl);


typedef struct
{
	u8 accessUnitStartFlag;
	u8 accessUnitEndFlag;
	u8 paddingFlag;
	u8 randomAccessPointFlag;
	u8 OCRflag;
	u8 idleFlag;
	u8 decodingTimeStampFlag;
	u8 compositionTimeStampFlag;
	u8 instantBitrateFlag;
	u8 degradationPriorityFlag;

	u8 paddingBits;
	u16 packetSequenceNumber;
	u64 objectClockReference;
	u16 AU_sequenceNumber;
	u64 decodingTimeStamp;
	u64 compositionTimeStamp;
	u16 accessUnitLength;
	u32 instantBitrate;
	u16 degradationPriority;

	/*Everything below this comment is internal to GPAC*/

	/*this is NOT part of standard SL, only used internally: signals duration of access unit if known
	this is useful for streams with very random updates, to prevent buffering for instance a subtitle stream
	which is likely to have no updates during the first minutes... expressed in media timescale*/
	u32 au_duration;
	/*ISMACryp extensions*/
	u8 isma_encrypted;
	u64 isma_BSO;
	/*CENC extensions*/
	u8 cenc_encrypted;
	u8 IV_size;

	//for CENC pattern encryption mode
	u8 crypt_byte_block, skip_byte_block;
	u8 constant_IV_size;
	bin128 constant_IV;
	/*version_number are pushed from m2ts sections to the mpeg4sl layer so as to handle mpeg4 stream dependencies*/
	u8 m2ts_version_number_plus_one;
	//0: not mpeg-2 TS PCR, 1: MEPG-2 TS PCR, 2: MPEG-2 TS PCR with discontinuity
	u8 m2ts_pcr;
	/* HTML5 MSE Packet info */
	s64 timeStampOffset;
	//ntp at sender/producer side for this packet, 0 otherwise
	u64 sender_ntp;
	//set for AUs which should be decodedd but not presented during seek
	u8 seekFlag;

	u32 samplerate, channels;
} GF_SLHeader;


/*packetize SL-PDU. If PDU is NULL or size 0, only writes the SL header*/
void gf_sl_packetize(GF_SLConfig* slConfig, GF_SLHeader *Header, u8 *PDU, u32 size, u8 **outPacket, u32 *OutSize);
/*gets SL header size in bytes*/
u32 gf_sl_get_header_size(GF_SLConfig* slConfig, GF_SLHeader *Header);

/*depacketize SL-PDU*/
void gf_sl_depacketize(GF_SLConfig *slConfig, GF_SLHeader *Header, const u8 *PDU, u32 PDULength, u32 *HeaderLen);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif	/*_GF_SYNC_LAYER_H_*/
