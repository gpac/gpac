/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG-4 ObjectDescriptor sub-project
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

#include <gpac/internal/odf_dev.h>


//
//	Constructor
//
GF_Descriptor *gf_odf_new_slc(u8 predef)
{
	GF_SLConfig *newDesc = (GF_SLConfig *) gf_malloc(sizeof(GF_SLConfig));
	if (!newDesc) return NULL;
	memset(newDesc, 0, sizeof(GF_SLConfig));
	newDesc->tag = GF_ODF_SLC_TAG;
	newDesc->predefined = predef;
	if (predef) gf_odf_slc_set_pref(newDesc);
	newDesc->useTimestampsFlag = 1;

	return (GF_Descriptor *)newDesc;
}

//
//	Destructor
//
GF_Err gf_odf_del_slc(GF_SLConfig *sl)
{
	if (!sl) return GF_BAD_PARAM;
	gf_free(sl);
	return GF_OK;
}

//
//	Set the SL to the ISO predefined value
//
GF_EXPORT
GF_Err gf_odf_slc_set_pref(GF_SLConfig *sl)
{
	if (! sl) return GF_BAD_PARAM;

	switch (sl->predefined) {

	case SLPredef_MP4:
		sl->useAccessUnitStartFlag = 0;
		sl->useAccessUnitEndFlag = 0;
		//each packet is an AU, and we need RAP signaling
		sl->useRandomAccessPointFlag = 1;
		sl->hasRandomAccessUnitsOnlyFlag = 0;
		sl->usePaddingFlag = 0;
		//in MP4 file, we use TimeStamps
		sl->useTimestampsFlag = 1;
		sl->useIdleFlag = 0;
		sl->durationFlag = 0;
		sl->timestampLength = 0;
		sl->OCRLength = 0;
		sl->AULength = 0;
		sl->instantBitrateLength = 0;
		sl->degradationPriorityLength = 0;
		sl->AUSeqNumLength = 0;
		sl->packetSeqNumLength = 0;
		break;

	case SLPredef_Null:
		sl->useAccessUnitStartFlag = 0;
		sl->useAccessUnitEndFlag = 0;
		sl->useRandomAccessPointFlag = 0;
		sl->hasRandomAccessUnitsOnlyFlag = 0;
		sl->usePaddingFlag = 0;
		sl->useTimestampsFlag = 0;
		sl->useIdleFlag = 0;
		sl->AULength = 0;
		sl->degradationPriorityLength = 0;
		sl->AUSeqNumLength = 0;
		sl->packetSeqNumLength = 0;

		//for MPEG4 IP
		sl->timestampResolution = 1000;
		sl->timestampLength = 32;
		break;
	/*handle all unknown predefined as predef-null*/
	default:
		sl->useAccessUnitStartFlag = 0;
		sl->useAccessUnitEndFlag = 0;
		sl->useRandomAccessPointFlag = 0;
		sl->hasRandomAccessUnitsOnlyFlag = 0;
		sl->usePaddingFlag = 0;
		sl->useTimestampsFlag = 1;
		sl->useIdleFlag = 0;
		sl->AULength = 0;
		sl->degradationPriorityLength = 0;
		sl->AUSeqNumLength = 0;
		sl->packetSeqNumLength = 0;

		sl->timestampResolution = 1000;
		sl->timestampLength = 32;
		break;
	}

	return GF_OK;
}


//this function gets the real amount of bytes needed to store the timeStamp
static u32 GetTSbytesLen(GF_SLConfig *sl)
{
	u32 TSlen, TSbytes;
	if (! sl) return 0;

	TSlen = sl->timestampLength * 2;
	TSbytes = TSlen / 8;
	TSlen = TSlen % 8;
	if (TSlen) TSbytes += 1;
	return TSbytes;
}

//
//		Reader
//
GF_Err gf_odf_read_slc(GF_BitStream *bs, GF_SLConfig *sl, u32 DescSize)
{
	GF_Err e;
	u32 nbBytes = 0;

	if (!sl) return GF_BAD_PARAM;

	//APPLE fix
	if (!DescSize) {
		sl->predefined = SLPredef_MP4;
		return gf_odf_slc_set_pref(sl);
	}

	sl->predefined = gf_bs_read_int(bs, 8);
	nbBytes += 1;

	/*MPEG4 IP fix*/
	if (!sl->predefined && nbBytes==DescSize) {
		sl->predefined = SLPredef_Null;
		gf_odf_slc_set_pref(sl);
		return GF_OK;
	}

	if (sl->predefined) {
		//predefined configuration
		e = gf_odf_slc_set_pref(sl);
		if (e) return e;
	} else {
		sl->useAccessUnitStartFlag = gf_bs_read_int(bs, 1);
		sl->useAccessUnitEndFlag = gf_bs_read_int(bs, 1);
		sl->useRandomAccessPointFlag = gf_bs_read_int(bs, 1);
		sl->hasRandomAccessUnitsOnlyFlag = gf_bs_read_int(bs, 1);
		sl->usePaddingFlag = gf_bs_read_int(bs, 1);
		sl->useTimestampsFlag = gf_bs_read_int(bs, 1);
		sl->useIdleFlag = gf_bs_read_int(bs, 1);
		sl->durationFlag = gf_bs_read_int(bs, 1);
		sl->timestampResolution = gf_bs_read_int(bs, 32);
		sl->OCRResolution = gf_bs_read_int(bs, 32);

		sl->timestampLength = gf_bs_read_int(bs, 8);
		if (sl->timestampLength > 64) return GF_ODF_INVALID_DESCRIPTOR;

		sl->OCRLength = gf_bs_read_int(bs, 8);
		if (sl->OCRLength > 64) return GF_ODF_INVALID_DESCRIPTOR;

		sl->AULength = gf_bs_read_int(bs, 8);
		if (sl->AULength > 32) return GF_ODF_INVALID_DESCRIPTOR;

		sl->instantBitrateLength = gf_bs_read_int(bs, 8);
		sl->degradationPriorityLength = gf_bs_read_int(bs, 4);
		sl->AUSeqNumLength = gf_bs_read_int(bs, 5);
		if (sl->AUSeqNumLength > 16) return GF_ODF_INVALID_DESCRIPTOR;
		sl->packetSeqNumLength = gf_bs_read_int(bs, 5);
		if (sl->packetSeqNumLength > 16) return GF_ODF_INVALID_DESCRIPTOR;

		/*reserved = */gf_bs_read_int(bs, 2);
		nbBytes += 15;
	}

	if (sl->durationFlag) {
		sl->timeScale = gf_bs_read_int(bs, 32);
		sl->AUDuration = gf_bs_read_int(bs, 16);
		sl->CUDuration = gf_bs_read_int(bs, 16);
		nbBytes += 8;
	}
	if (! sl->useTimestampsFlag) {
		sl->startDTS = gf_bs_read_long_int(bs, sl->timestampLength);
		sl->startCTS = gf_bs_read_long_int(bs, sl->timestampLength);
		nbBytes += GetTSbytesLen(sl);
	}

	if (DescSize != nbBytes) return GF_ODF_INVALID_DESCRIPTOR;
	return GF_OK;
}


//
//	Size
//
GF_Err gf_odf_size_slc(GF_SLConfig *sl, u32 *outSize)
{
	if (! sl) return GF_BAD_PARAM;

	*outSize = 1;
	if (! sl->predefined)	*outSize += 15;
	if (sl->durationFlag)	*outSize += 8;
	if (! sl->useTimestampsFlag) *outSize += GetTSbytesLen(sl);
	return GF_OK;
}

//
//	Writer
//
GF_Err gf_odf_write_slc(GF_BitStream *bs, GF_SLConfig *sl)
{
	GF_Err e;
	u32 size;
	if (! sl) return GF_BAD_PARAM;

	e = gf_odf_size_descriptor((GF_Descriptor *)sl, &size);
	if (e) return e;
	e = gf_odf_write_base_descriptor(bs, sl->tag, size);
	if (e) return e;

	gf_bs_write_int(bs, sl->predefined, 8);
	if (! sl->predefined) {
		gf_bs_write_int(bs, sl->useAccessUnitStartFlag, 1);
		gf_bs_write_int(bs, sl->useAccessUnitEndFlag, 1);
		gf_bs_write_int(bs, sl->useRandomAccessPointFlag, 1);
		gf_bs_write_int(bs, sl->hasRandomAccessUnitsOnlyFlag, 1);
		gf_bs_write_int(bs, sl->usePaddingFlag, 1);
		gf_bs_write_int(bs, sl->useTimestampsFlag, 1);
		gf_bs_write_int(bs, sl->useIdleFlag, 1);
		gf_bs_write_int(bs, sl->durationFlag, 1);
		gf_bs_write_int(bs, sl->timestampResolution, 32);
		gf_bs_write_int(bs, sl->OCRResolution, 32);
		gf_bs_write_int(bs, sl->timestampLength, 8);
		gf_bs_write_int(bs, sl->OCRLength, 8);
		gf_bs_write_int(bs, sl->AULength, 8);
		gf_bs_write_int(bs, sl->instantBitrateLength, 8);
		gf_bs_write_int(bs, sl->degradationPriorityLength, 4);
		gf_bs_write_int(bs, sl->AUSeqNumLength, 5);
		gf_bs_write_int(bs, sl->packetSeqNumLength, 5);
		gf_bs_write_int(bs, 3, 2);	//reserved: 0b11 == 3
	}
	if (sl->durationFlag) {
		gf_bs_write_int(bs, sl->timeScale, 32);
		gf_bs_write_int(bs, sl->AUDuration, 16);
		gf_bs_write_int(bs, sl->CUDuration, 16);
	}
	if (! sl->useTimestampsFlag) {
		gf_bs_write_long_int(bs, sl->startDTS, sl->timestampLength);
		gf_bs_write_long_int(bs, sl->startCTS, sl->timestampLength);
	}

	return GF_OK;
}


/*allocates and writes the SL-PDU (Header + PDU) given the SLConfig and the GF_SLHeader
for this PDU. AUs must be split in PDUs by another process if needed (packetizer).*/
GF_EXPORT
void gf_sl_packetize(GF_SLConfig* slConfig,
                     GF_SLHeader *Header,
                     u8 *PDU,
                     u32 size,
                     u8 **outPacket,
                     u32 *OutSize)
{
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	*OutSize = 0;
	if (!bs) return;

	if (slConfig->useAccessUnitStartFlag) gf_bs_write_int(bs, Header->accessUnitStartFlag, 1);
	if (slConfig->useAccessUnitEndFlag) gf_bs_write_int(bs, Header->accessUnitEndFlag, 1);
	if (slConfig->OCRLength > 0) gf_bs_write_int(bs, Header->OCRflag, 1);
	if (slConfig->useIdleFlag) gf_bs_write_int(bs, Header->idleFlag, 1);
	if (slConfig->usePaddingFlag) {
		gf_bs_write_int(bs, Header->paddingFlag, 1);
		if (Header->paddingFlag) gf_bs_write_int(bs, Header->paddingBits, 3);
	}
	if (! Header->idleFlag && (! Header->paddingFlag || Header->paddingBits != 0)) {
		if (slConfig->packetSeqNumLength > 0) gf_bs_write_int(bs, Header->packetSequenceNumber, slConfig->packetSeqNumLength);
		if (slConfig->degradationPriorityLength > 0) {
			gf_bs_write_int(bs, Header->degradationPriorityFlag, 1);
			if (Header->degradationPriorityFlag) gf_bs_write_int(bs, Header->degradationPriority, slConfig->degradationPriorityLength);
		}
		if (Header->OCRflag) gf_bs_write_long_int(bs, Header->objectClockReference, slConfig->OCRLength);
		if (Header->accessUnitStartFlag) {
			if (slConfig->useRandomAccessPointFlag) gf_bs_write_int(bs, Header->randomAccessPointFlag, 1);
			if (slConfig->AUSeqNumLength > 0) gf_bs_write_int(bs, Header->AU_sequenceNumber, slConfig->AUSeqNumLength);
			if (slConfig->useTimestampsFlag) {
				gf_bs_write_int(bs, Header->decodingTimeStampFlag, 1);
				gf_bs_write_int(bs, Header->compositionTimeStampFlag, 1);
			}
			if (slConfig->instantBitrateLength > 0) gf_bs_write_int(bs, Header->instantBitrateFlag, 1);
			if (Header->decodingTimeStampFlag) gf_bs_write_long_int(bs, Header->decodingTimeStamp, slConfig->timestampLength);
			if (Header->compositionTimeStampFlag) gf_bs_write_long_int(bs, Header->compositionTimeStamp, slConfig->timestampLength);
			if (slConfig->AULength > 0) gf_bs_write_int(bs, Header->accessUnitLength, slConfig->AULength);
			if (Header->instantBitrateFlag) gf_bs_write_int(bs, Header->instantBitrate, slConfig->instantBitrateLength);
		}
	}
	/*done with the Header, Alin*/
	gf_bs_align(bs);

	/*write the PDU - already byte aligned with stuffing (paddingBits in SL Header)*/
	if (PDU && size)
		gf_bs_write_data(bs, PDU, size);

	gf_bs_align(bs);
	gf_bs_get_content(bs, outPacket, OutSize);
	gf_bs_del(bs);
}

/*allocates and writes the SL-PDU (Header + PDU) given the SLConfig and the GF_SLHeader
for this PDU. AUs must be split in PDUs by another process if needed (packetizer).*/
GF_EXPORT
u32 gf_sl_get_header_size(GF_SLConfig* slConfig, GF_SLHeader *Header)
{
	u32 nb_bits = 0;

	if (slConfig->useAccessUnitStartFlag) nb_bits++;
	if (slConfig->useAccessUnitEndFlag) nb_bits++;
	if (slConfig->OCRLength > 0) nb_bits++;
	if (slConfig->useIdleFlag) nb_bits++;
	if (slConfig->usePaddingFlag) {
		nb_bits++;
		if (Header->paddingFlag) nb_bits+=3;
	}
	if (!Header->idleFlag && (! Header->paddingFlag || Header->paddingBits != 0)) {
		if (slConfig->packetSeqNumLength > 0) nb_bits += slConfig->packetSeqNumLength;
		if (slConfig->degradationPriorityLength > 0) {
			nb_bits++;
			if (Header->degradationPriorityFlag) nb_bits += slConfig->degradationPriorityLength;
		}
		if (Header->OCRflag) nb_bits += slConfig->OCRLength;
		if (Header->accessUnitStartFlag) {
			if (slConfig->useRandomAccessPointFlag) nb_bits++;
			if (slConfig->AUSeqNumLength > 0) nb_bits += slConfig->AUSeqNumLength;
			if (slConfig->useTimestampsFlag) {
				nb_bits++;
				nb_bits++;
			}
			if (slConfig->instantBitrateLength > 0) nb_bits++;
			if (Header->decodingTimeStampFlag) nb_bits += slConfig->timestampLength;
			if (Header->compositionTimeStampFlag) nb_bits += slConfig->timestampLength;
			if (slConfig->AULength > 0) nb_bits += slConfig->AULength;
			if (Header->instantBitrateFlag) nb_bits += slConfig->instantBitrateLength;
		}
	}
	while (nb_bits%8) nb_bits++;
	return nb_bits/8;
}


GF_EXPORT
void gf_sl_depacketize (GF_SLConfig *slConfig, GF_SLHeader *Header, const u8 *PDU, u32 PDULength, u32 *HeaderLen)
{
	GF_BitStream *bs;
	*HeaderLen = 0;
	if (!Header) return;
	//reset the input header
	memset(Header, 0, sizeof(GF_SLHeader));

	bs = gf_bs_new(PDU, PDULength, GF_BITSTREAM_READ);
	if (!bs) return;

	if (slConfig->useAccessUnitStartFlag) Header->accessUnitStartFlag = gf_bs_read_int(bs, 1);
	if (slConfig->useAccessUnitEndFlag) Header->accessUnitEndFlag = gf_bs_read_int(bs, 1);
	if ( !slConfig->useAccessUnitStartFlag && !slConfig->useAccessUnitEndFlag) {
		Header->accessUnitStartFlag = 1;
		Header->accessUnitEndFlag = 1;
	}
	if (slConfig->OCRLength > 0) Header->OCRflag = gf_bs_read_int(bs, 1);
	if (slConfig->useIdleFlag) Header->idleFlag = gf_bs_read_int(bs, 1);
	if (slConfig->usePaddingFlag) {
		Header->paddingFlag = gf_bs_read_int(bs, 1);
		if (Header->paddingFlag) Header->paddingBits = gf_bs_read_int(bs, 3);
	}
	if (!Header->paddingFlag || (Header->paddingBits != 0)) {

		if (slConfig->packetSeqNumLength > 0) Header->packetSequenceNumber = gf_bs_read_int(bs, slConfig->packetSeqNumLength);
		if (slConfig->degradationPriorityLength > 0) {
			Header->degradationPriorityFlag = gf_bs_read_int(bs, 1);
			if (Header->degradationPriorityFlag) Header->degradationPriority = gf_bs_read_int(bs, slConfig->degradationPriorityLength);
		}
		if (Header->OCRflag) Header->objectClockReference = gf_bs_read_long_int(bs, slConfig->OCRLength);
		if (Header->accessUnitStartFlag) {
			if (slConfig->useRandomAccessPointFlag) Header->randomAccessPointFlag = gf_bs_read_int(bs, 1);
			if (slConfig->AUSeqNumLength > 0) Header->AU_sequenceNumber = gf_bs_read_int(bs, slConfig->AUSeqNumLength);
			if (slConfig->useTimestampsFlag) {
				Header->decodingTimeStampFlag = gf_bs_read_int(bs, 1);
				Header->compositionTimeStampFlag = gf_bs_read_int(bs, 1);
			}
			if (slConfig->instantBitrateLength > 0) Header->instantBitrateFlag = gf_bs_read_int(bs, 1);
			if (Header->decodingTimeStampFlag) Header->decodingTimeStamp = gf_bs_read_long_int(bs, slConfig->timestampLength);
			if (Header->compositionTimeStampFlag) Header->compositionTimeStamp = gf_bs_read_long_int(bs, slConfig->timestampLength);
			if (slConfig->AULength > 0) Header->accessUnitLength = gf_bs_read_int(bs, slConfig->AULength);
			if (Header->instantBitrateFlag) Header->instantBitrate = gf_bs_read_int(bs, slConfig->instantBitrateLength);
		}
	}
	gf_bs_align(bs);
	*HeaderLen = (u32) gf_bs_get_position(bs);
	gf_bs_del(bs);
}
