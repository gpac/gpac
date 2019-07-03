/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / IETF RTP/RTSP/SDP sub-project
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

#include <gpac/internal/ietf_dev.h>

#ifndef GPAC_DISABLE_STREAMING

#include <gpac/constants.h>

//get the size of the RSLH section given the GF_SLHeader and the SLMap
static u32 gf_rtp_build_au_hdr_size(GP_RTPPacketizer *builder, GF_SLHeader *slh)
{
	u32 nbBits = 0;

	/*sel enc*/
	if (builder->flags & GP_RTP_PCK_SELECTIVE_ENCRYPTION) nbBits += 8;
	/*Note: ISMACryp ALWAYS indicates IV (BSO) and KEYIDX, even when sample is not encrypted. This is
	quite a waste when using selective encryption....*/

	/*IV*/
	nbBits += builder->first_sl_in_rtp ? 8*builder->slMap.IV_length : 8*builder->slMap.IV_delta_length;
	/*keyIndicator*/
	if (builder->first_sl_in_rtp || (builder->flags & GP_RTP_PCK_KEY_IDX_PER_AU)) {
		nbBits += 8*builder->slMap.KI_length;
	}

	/*no input header specified, compute the MAX size*/
	if (!slh) {
		/*size length*/
		if (!builder->slMap.ConstantSize) nbBits += builder->slMap.SizeLength;
		/*AU index length*/
		nbBits += builder->first_sl_in_rtp ? builder->slMap.IndexLength : builder->slMap.IndexDeltaLength;
		/*CTS flag*/
		if (builder->slMap.CTSDeltaLength) {
			nbBits += 1;
			/*all non-first packets have the CTS written if asked*/
			if (!builder->first_sl_in_rtp) nbBits += builder->slMap.CTSDeltaLength;
		}
		if (builder->slMap.DTSDeltaLength) nbBits += 1 + builder->slMap.DTSDeltaLength;
		if (builder->flags & GP_RTP_PCK_SELECTIVE_ENCRYPTION) nbBits += 8;
		return nbBits;
	}

	/*size length*/
	if (!builder->slMap.ConstantSize) nbBits += builder->slMap.SizeLength;

	/*AU index*/
	if (builder->first_sl_in_rtp) {
		if (builder->slMap.IndexLength) nbBits += builder->slMap.IndexLength;
	} else {
		if (builder->slMap.IndexDeltaLength) nbBits += builder->slMap.IndexDeltaLength;
	}

	/*CTS Flag*/
	if (builder->slMap.CTSDeltaLength) {
		/*CTS not written if first SL*/
		if (builder->first_sl_in_rtp) slh->compositionTimeStampFlag = 0;
		/*but CTS flag is always written*/
		nbBits += 1;
	} else {
		slh->compositionTimeStampFlag = 0;
	}
	/*CTS*/
	if (slh->compositionTimeStampFlag) nbBits += builder->slMap.CTSDeltaLength;

	/*DTS Flag*/
	if (builder->slMap.DTSDeltaLength) {
		nbBits += 1;
	} else {
		slh->decodingTimeStampFlag = 0;
	}
	/*DTS*/
	if (slh->decodingTimeStampFlag) nbBits += builder->slMap.DTSDeltaLength;
	/*RAP indication*/
	if (builder->slMap.RandomAccessIndication) nbBits ++;
	/*streamState indication*/
	nbBits += builder->slMap.StreamStateIndication;

	return nbBits;
}


/*write the AU header section - return the nb of BITS written for AU header*/
u32 gf_rtp_build_au_hdr_write(GP_RTPPacketizer *builder, u32 PayloadSize, u32 RTP_TS)
{
	u32 nbBits = 0;
	s32 delta;

	/*selective encryption*/
	if (builder->flags & GP_RTP_PCK_SELECTIVE_ENCRYPTION) {
		gf_bs_write_int(builder->pck_hdr, builder->is_encrypted, 1);
		gf_bs_write_int(builder->pck_hdr, 0, 7);
		nbBits = 8;
	}
	/*IV*/
	if (builder->first_sl_in_rtp) {
		if (builder->slMap.IV_length) {
			gf_bs_write_long_int(builder->pck_hdr, builder->IV, 8*builder->slMap.IV_length);
			nbBits += 8*builder->slMap.IV_length;
		}
	} else if (builder->slMap.IV_delta_length) {
		/*NOT SUPPORTED!!! this only applies to interleaving*/
	}
	/*key*/
	if (builder->slMap.KI_length) {
		if (builder->first_sl_in_rtp || (builder->flags & GP_RTP_PCK_KEY_IDX_PER_AU)) {
			if (builder->key_indicator) gf_bs_write_data(builder->pck_hdr, builder->key_indicator, builder->slMap.KI_length);
			else gf_bs_write_int(builder->pck_hdr, 0, 8*builder->slMap.KI_length);
			nbBits += 8*builder->slMap.KI_length;
		}
	}


	/*size length*/
	if (builder->slMap.ConstantSize) {
		if (PayloadSize != builder->slMap.ConstantSize) return nbBits;
	} else if (builder->slMap.SizeLength) {
		/*write the AU size - if not enough bytes (real-time cases) set size to 0*/
		if (builder->sl_header.accessUnitLength >= (1<<builder->slMap.SizeLength)) {
			gf_bs_write_int(builder->pck_hdr, 0, builder->slMap.SizeLength);
		} else {
			gf_bs_write_int(builder->pck_hdr, builder->sl_header.accessUnitLength, builder->slMap.SizeLength);
		}
		nbBits += builder->slMap.SizeLength;
	}
	/*AU index*/
	if (builder->first_sl_in_rtp) {
		if (builder->slMap.IndexLength) {
			gf_bs_write_int(builder->pck_hdr, builder->sl_header.AU_sequenceNumber, builder->slMap.IndexLength);
			nbBits += builder->slMap.IndexLength;
		}
	} else {
		if (builder->slMap.IndexDeltaLength) {
			//check interleaving, otherwise force default (which is currently always the case)
			delta = builder->sl_header.AU_sequenceNumber - builder->last_au_sn;
			delta -= 1;
			gf_bs_write_int(builder->pck_hdr, delta, builder->slMap.IndexDeltaLength);
			nbBits += builder->slMap.IndexDeltaLength;
		}
	}

	/*CTS Flag*/
	if (builder->slMap.CTSDeltaLength) {
		if (builder->first_sl_in_rtp) {
			builder->sl_header.compositionTimeStampFlag = 0;
			builder->sl_header.compositionTimeStamp = RTP_TS;
		}
		gf_bs_write_int(builder->pck_hdr, builder->sl_header.compositionTimeStampFlag, 1);
		nbBits += 1;
	}
	/*CTS*/
	if (builder->sl_header.compositionTimeStampFlag) {
		delta = (u32) builder->sl_header.compositionTimeStamp - RTP_TS;
		gf_bs_write_int(builder->pck_hdr, delta, builder->slMap.CTSDeltaLength);
		nbBits += builder->slMap.CTSDeltaLength;
	}
	/*DTS Flag*/
	if (builder->slMap.DTSDeltaLength) {
		gf_bs_write_int(builder->pck_hdr, builder->sl_header.decodingTimeStampFlag, 1);
		nbBits += 1;
	}
	/*DTS*/
	if (builder->sl_header.decodingTimeStampFlag) {
		delta = (u32) (builder->sl_header.compositionTimeStamp - builder->sl_header.decodingTimeStamp);
		gf_bs_write_int(builder->pck_hdr, delta, builder->slMap.DTSDeltaLength);
		nbBits += builder->slMap.DTSDeltaLength;
	}
	/*RAP indication*/
	if (builder->slMap.RandomAccessIndication) {
		gf_bs_write_int(builder->pck_hdr, builder->sl_header.randomAccessPointFlag, 1);
		nbBits ++;
	}
	/*stream state - write AUSeqNum*/
	if (builder->slMap.StreamStateIndication) {
		gf_bs_write_int(builder->pck_hdr, builder->sl_header.AU_sequenceNumber, builder->slMap.StreamStateIndication);
		nbBits += builder->slMap.StreamStateIndication;
	}

	return nbBits;
}


GF_Err gp_rtp_builder_do_mpeg4(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize)
{
	u8 *sl_buffer, *payl_buffer;
	u32 sl_buffer_size, payl_buffer_size;
	u32 auh_size_tmp, bytesLeftInPacket, infoSize, pckSize;
	u64 pos;
	u8 flush_pck, no_split;

	flush_pck = 0;

	bytesLeftInPacket = data_size;
	/*flush everything*/
	if (!data) {
		if (builder->payload) goto flush_packet;
		return GF_OK;
	}
	if (builder->payload && builder->force_flush) goto flush_packet;

	//go till done
	while (bytesLeftInPacket) {
		no_split = 0;

		if (builder->sl_header.accessUnitStartFlag) {
			//init SL
			if (builder->sl_header.compositionTimeStamp != builder->sl_header.decodingTimeStamp) {
				builder->sl_header.decodingTimeStampFlag = 1;
			}
			builder->sl_header.compositionTimeStampFlag = 1;
			builder->sl_header.accessUnitLength = FullAUSize;

			//init some vars - based on the available size and the TS
			//we decide if we go with the same RTP TS serie or not
			if (builder->payload) {
				//don't store more than what we can (that is 2^slMap->CTSDelta - 1)
				if ( (builder->flags & GP_RTP_PCK_SIGNAL_TS)
				        && (builder->sl_header.compositionTimeStamp - builder->rtp_header.TimeStamp >= (u32) ( 1 << builder->slMap.CTSDeltaLength) ) ) {
					goto flush_packet;
				}
				//don't split AU if # TS , start a new RTP pck
				if (builder->sl_header.compositionTimeStamp != builder->rtp_header.TimeStamp)
					no_split = 1;
			}
		}

		/*new RTP Packet*/
		if (!builder->payload) {
			/*first SL in RTP*/
			builder->first_sl_in_rtp = GF_TRUE;

			/*if this is the end of an AU we will set it to 0 as soon as an AU is splitted*/
			builder->rtp_header.Marker = 1;
			builder->rtp_header.PayloadType = builder->PayloadType;
			builder->rtp_header.SequenceNumber += 1;

			builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
			/*prepare the mapped headers*/
			builder->pck_hdr = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			builder->payload = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			builder->bytesInPacket = 0;

			/*in multiSL there is a MSLHSize structure on 2 bytes*/
			builder->auh_size = 0;
			if (builder->has_AU_header) {
				builder->auh_size = 16;
				gf_bs_write_int(builder->pck_hdr, 0, 16);
			}
			flush_pck = 0;
			/*and create packet*/
			builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
		}

		//make sure we are not interleaving too much - this should never happen actually
		if (builder->slMap.IndexDeltaLength
		        && !builder->first_sl_in_rtp
		        && (builder->sl_header.AU_sequenceNumber - builder->last_au_sn >= (u32) 1<<builder->slMap.IndexDeltaLength)) {
			//we cannot write this packet here
			goto flush_packet;
		}
		/*check max ptime*/
		if (builder->max_ptime && ( (u32) builder->sl_header.compositionTimeStamp >= builder->rtp_header.TimeStamp + builder->max_ptime) )
			goto flush_packet;

		auh_size_tmp = gf_rtp_build_au_hdr_size(builder, &builder->sl_header);

		infoSize = auh_size_tmp + builder->auh_size;
		infoSize /= 8;
		/*align*/
		if ( (builder->auh_size + auh_size_tmp) % 8) infoSize += 1;


		if (bytesLeftInPacket + infoSize + builder->bytesInPacket <= builder->Path_MTU) {
			//End of our data chunk
			pckSize = bytesLeftInPacket;
			builder->sl_header.accessUnitEndFlag = IsAUEnd;

			builder->auh_size += auh_size_tmp;

			builder->sl_header.paddingFlag = builder->sl_header.paddingBits ? 1 : 0;
		} else {

			//AU cannot fit in packet. If no split, start a new packet
			if (no_split) goto flush_packet;

			builder->auh_size += auh_size_tmp;

			pckSize = builder->Path_MTU - (infoSize + builder->bytesInPacket);
			//that's the end of the rtp packet
			flush_pck = 1;
			//but not of the AU -> marker is 0
			builder->rtp_header.Marker = 0;
		}

		gf_rtp_build_au_hdr_write(builder, pckSize, builder->rtp_header.TimeStamp);

		//notify the user of our data structure
		if (builder->OnDataReference)
			builder->OnDataReference(builder->cbk_obj, pckSize, data_size - bytesLeftInPacket);
		else
			gf_bs_write_data(builder->payload, data + (data_size - bytesLeftInPacket), pckSize);


		bytesLeftInPacket -= pckSize;
		builder->bytesInPacket += pckSize;
		/*update IV*/
		builder->IV += pckSize;
		builder->sl_header.paddingFlag = 0;
		builder->sl_header.accessUnitStartFlag = 0;

		//we are splitting a payload, auto increment SL seq num
		if (bytesLeftInPacket) {
			builder->sl_header.packetSequenceNumber += 1;
		} else if (! (builder->flags & GP_RTP_PCK_USE_MULTI) ) {
			builder->rtp_header.Marker = 1;
			flush_pck = 1;
		}

		//first SL in RTP is done
		builder->first_sl_in_rtp = GF_FALSE;

		//store current sl
		builder->last_au_sn = builder->sl_header.AU_sequenceNumber;

		if (!flush_pck) continue;

		//done with the packet
flush_packet:

		gf_bs_align(builder->pck_hdr);

		/*no aux data yet*/
		if (builder->slMap.AuxiliaryDataSizeLength)	{
			//write RSLH after the MSLH
			gf_bs_write_int(builder->pck_hdr, 0, builder->slMap.AuxiliaryDataSizeLength);
		}
		/*rewrite the size header*/
		if (builder->has_AU_header) {
			pos = gf_bs_get_position(builder->pck_hdr);
			gf_bs_seek(builder->pck_hdr, 0);
			builder->auh_size -= 16;
			gf_bs_write_int(builder->pck_hdr, builder->auh_size, 16);
			gf_bs_seek(builder->pck_hdr, pos);
		}

		sl_buffer = NULL;
		gf_bs_get_content(builder->pck_hdr, &sl_buffer, &sl_buffer_size);
		//delete our bitstream
		gf_bs_del(builder->pck_hdr);
		builder->pck_hdr = NULL;

		payl_buffer = NULL;
		payl_buffer_size = 0;
		if (!builder->OnDataReference)
			gf_bs_get_content(builder->payload, &payl_buffer, &payl_buffer_size);

		gf_bs_del(builder->payload);
		builder->payload = NULL;

		/*notify header*/
		builder->OnData(builder->cbk_obj, sl_buffer, sl_buffer_size, GF_TRUE);
		/*notify payload*/
		if (payl_buffer) {
			builder->OnData(builder->cbk_obj, payl_buffer, payl_buffer_size, GF_FALSE);
			gf_free(payl_buffer);
		}
		/*flush packet*/
		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		gf_free(sl_buffer);
	}
	//packet is done, update AU markers
	if (IsAUEnd) {
		builder->sl_header.accessUnitStartFlag = 1;
		builder->sl_header.accessUnitEndFlag = 0;
	}
	return GF_OK;
}


GF_Err gp_rtp_builder_do_avc(GP_RTPPacketizer *builder, u8 *nalu, u32 nalu_size, u8 IsAUEnd, u32 FullAUSize)
{
	u32 do_flush, bytesLeft, size, nal_type;
	char shdr[2];
	char stap_hdr;

	do_flush = 0;
	if (!nalu) do_flush = 1;
	/*we only do STAP or SINGLE modes*/
	else if (builder->sl_header.accessUnitStartFlag) do_flush = 1;
	/*we must NOT fragment a NALU*/
	else if (builder->bytesInPacket + nalu_size >= builder->Path_MTU) do_flush = 2;
	/*aggregation is disabled*/
	else if (! (builder->flags & GP_RTP_PCK_USE_MULTI) ) do_flush = 2;

	if (builder->bytesInPacket && do_flush) {
		builder->rtp_header.Marker = (do_flush==1) ? 1 : 0;
		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		builder->bytesInPacket = 0;
	}

	if (!nalu) return GF_OK;

	/*need a new RTP packet*/
	if (!builder->bytesInPacket) {
		builder->rtp_header.PayloadType = builder->PayloadType;
		builder->rtp_header.Marker = 0;
		builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
		builder->rtp_header.SequenceNumber += 1;
		builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
		builder->avc_non_idr = GF_TRUE;
	}

	/*check NAL type to see if disposable or not*/
	nal_type = nalu[0] & 0x1F;
	switch (nal_type) {
	case GF_AVC_NALU_NON_IDR_SLICE:
	case GF_AVC_NALU_ACCESS_UNIT:
	case GF_AVC_NALU_END_OF_SEQ:
	case GF_AVC_NALU_END_OF_STREAM:
	case GF_AVC_NALU_FILLER_DATA:
		break;
	default:
		builder->avc_non_idr = GF_FALSE;
		break;
	}

	/*at this point we're sure the NALU fits in current packet OR must be splitted*/

	/*pb: we don't know if next NALU from this AU will be small enough to fit in the packet, so we always
	go for stap...*/
	if (builder->bytesInPacket+nalu_size<builder->Path_MTU) {
		Bool use_stap = GF_TRUE;
		/*if this is the AU end and no NALU in packet, go for single mode*/
		if (IsAUEnd && !builder->bytesInPacket) use_stap = GF_FALSE;

		if (use_stap) {
			/*declare STAP-A NAL*/
			if (!builder->bytesInPacket) {
				/*copy over F and NRI from first nal in packet and assign type*/
				stap_hdr = (nalu[0] & 0xE0) | 24;
				builder->OnData(builder->cbk_obj, (char *) &stap_hdr, 1, GF_FALSE);
				builder->bytesInPacket = 1;
			}
			/*add NALU size*/
			shdr[0] = nalu_size>>8;
			shdr[1] = nalu_size&0x00ff;
			builder->OnData(builder->cbk_obj, (char *)shdr, 2, GF_FALSE);
			builder->bytesInPacket += 2;
		}
		/*add data*/
		if (builder->OnDataReference)
			builder->OnDataReference(builder->cbk_obj, nalu_size, 0);
		else
			builder->OnData(builder->cbk_obj, nalu, nalu_size, GF_FALSE);

		builder->bytesInPacket += nalu_size;

		if (IsAUEnd) {
			builder->rtp_header.Marker = 1;
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;
		}
	}
	/*fragmentation unit*/
	else {
		u32 offset;
		assert(nalu_size>=builder->Path_MTU);
		assert(!builder->bytesInPacket);
		/*FU payload doesn't have the NAL hdr*/
		bytesLeft = nalu_size - 1;
		offset = 1;
		while (bytesLeft) {
			if (2 + bytesLeft > builder->Path_MTU) {
				size = builder->Path_MTU - 2;
			} else {
				size = bytesLeft;
			}

			/*copy over F and NRI from nal in packet and assign type*/
			shdr[0] = (nalu[0] & 0xE0) | 28;
			/*copy over NAL type from nal and set start bit and end bit*/
			shdr[1] = (nalu[0] & 0x1F);
			/*start bit*/
			if (offset==1) shdr[1] |= 0x80;
			/*end bit*/
			else if (size == bytesLeft) shdr[1] |= 0x40;

			builder->OnData(builder->cbk_obj, (char *)shdr, 2, GF_FALSE);

			/*add data*/
			if (builder->OnDataReference)
				builder->OnDataReference(builder->cbk_obj, size, offset);
			else
				builder->OnData(builder->cbk_obj, nalu+offset, size, GF_FALSE);

			offset += size;
			bytesLeft -= size;

			/*flush no matter what (FUs cannot be agreggated)*/
			builder->rtp_header.Marker = (IsAUEnd && !bytesLeft) ? 1 : 0;
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;

			if (bytesLeft) {
				builder->rtp_header.PayloadType = builder->PayloadType;
				builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
				builder->rtp_header.SequenceNumber += 1;
				builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
			}
		}
	}

	return GF_OK;
}

GF_Err gp_rtp_builder_do_hevc(GP_RTPPacketizer *builder, u8 *nalu, u32 nalu_size, u8 IsAUEnd, u32 FullAUSize)
{
	u32 do_flush, bytesLeft, size;

	do_flush = 0;
	if (!nalu) do_flush = 1;
	else if (builder->sl_header.accessUnitStartFlag) do_flush = 1;
	/*we must NOT fragment a NALU*/
	else if (builder->bytesInPacket + nalu_size + 4 >= builder->Path_MTU) do_flush = 2; //2 bytes PayloadHdr for AP + 2 bytes NAL size
	/*aggregation is disabled*/
	else if (! (builder->flags & GP_RTP_PCK_USE_MULTI) ) do_flush = 2;

	if (builder->bytesInPacket && do_flush) {
		builder->rtp_header.Marker = (do_flush==1) ? 1 : 0;
		/*insert payload_hdr in case of AP*/
		if (strlen(builder->hevc_payload_hdr)) {
			builder->OnData(builder->cbk_obj, (char *)builder->hevc_payload_hdr, 2, GF_TRUE);
			memset(builder->hevc_payload_hdr, 0, 2);
		}
		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		builder->bytesInPacket = 0;
	}

	if (!nalu) return GF_OK;

	/*need a new RTP packet*/
	if (!builder->bytesInPacket) {
		builder->rtp_header.PayloadType = builder->PayloadType;
		builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
		builder->rtp_header.SequenceNumber += 1;
		builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
	}

	/*at this point we're sure the NALU fits in current packet OR must be splitted*/
	/*check that we should use single NALU packet mode or aggreation packets mode*/
	if (builder->bytesInPacket+nalu_size+4 < builder->Path_MTU) {
		Bool use_AP = (builder->flags & GP_RTP_PCK_USE_MULTI) ? GF_TRUE : GF_FALSE;
		/*if this is the AU end and no NALU in packet, go for single NALU packet mode*/
		if (IsAUEnd && !builder->bytesInPacket) use_AP = GF_FALSE;

		if (use_AP) {
			char nal_s[2];
			/*declare PayloadHdr for AP*/
			if (!builder->bytesInPacket) {
				/*copy F bit and assign type*/
				builder->hevc_payload_hdr[0] = (nalu[0] & 0x81) | (48 << 1);
				/*copy LayerId and TID*/
				builder->hevc_payload_hdr[1] = nalu[1];
			}
			else {
				/*F bit of AP is 0 if the F nit of each aggreated NALU is 0; otherwise its must be 1*/
				/*LayerId and TID must ne the lowest value of LayerId and TID of all aggreated NALU*/
				u8 cur_LayerId, cur_TID, new_LayerId, new_TID;

				builder->hevc_payload_hdr[0] |= (nalu[0] & 0x80);
				cur_LayerId = ((builder->hevc_payload_hdr[0] & 0x01) << 5) + ((builder->hevc_payload_hdr[1] & 0xF8) >> 3);
				new_LayerId = ((nalu[0] & 0x01) << 5) + ((nalu[1] & 0xF8) >> 3);
				if (cur_LayerId > new_LayerId) {
					builder->hevc_payload_hdr[0] = (builder->hevc_payload_hdr[0] & 0xFE) | (nalu[0] & 0x01);
					builder->hevc_payload_hdr[1] = (builder->hevc_payload_hdr[1] & 0x07) | (nalu[1] & 0xF8);
				}
				cur_TID = builder->hevc_payload_hdr[1] & 0x07;
				new_TID = nalu[1] & 0x07;
				if (cur_TID > new_TID) {
					builder->hevc_payload_hdr[1] = (builder->hevc_payload_hdr[1] & 0xF8) | (nalu[1] & 0x07);
				}
			}

			/*add NALU size*/
			nal_s[0] = nalu_size>>8;
			nal_s[1] = nalu_size&0x00ff;
			builder->OnData(builder->cbk_obj, (char *)nal_s, 2, GF_FALSE);
			builder->bytesInPacket += 2;
		}
		/*add data*/
		if (builder->OnDataReference)
			builder->OnDataReference(builder->cbk_obj, nalu_size, 0);
		else
			builder->OnData(builder->cbk_obj, nalu, nalu_size, GF_FALSE);

		builder->bytesInPacket += nalu_size;

		if (IsAUEnd) {
			builder->rtp_header.Marker = 1;
			if (strlen(builder->hevc_payload_hdr)) {
				builder->OnData(builder->cbk_obj, (char *)builder->hevc_payload_hdr, 2, GF_TRUE);
				memset(builder->hevc_payload_hdr, 0, 2);
			}
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;
		}
	}
	/*fragmentation unit*/
	else {
		u32 offset;
		char payload_hdr[2];
		char shdr;

		assert(nalu_size + 4 >=builder->Path_MTU);
		assert(!builder->bytesInPacket);

		/*FU payload doesn't have the NAL hdr (2 bytes*/
		bytesLeft = nalu_size - 2;
		offset = 2;
		while (bytesLeft) {
			if (3 + bytesLeft > builder->Path_MTU) {
				size = builder->Path_MTU - 3;
			} else {
				size = bytesLeft;
			}

			/*declare PayloadHdr for FU*/
			memset(payload_hdr, 0, 2);
			/*copy F bit and assign type*/
			payload_hdr[0] = (nalu[0] & 0x81) | (49 << 1);
			/*copy LayerId and TID*/
			payload_hdr[1] = nalu[1];
			builder->OnData(builder->cbk_obj, (char *)payload_hdr, 2, GF_FALSE);

			/*declare FU header*/
			shdr = 0;
			/*assign type*/
			shdr |= (nalu[0] & 0x7E) >> 1;
			/*start bit*/
			if (offset==2) shdr |= 0x80;
			/*end bit*/
			else if (size == bytesLeft) shdr |= 0x40;

			builder->OnData(builder->cbk_obj, &shdr, 1, GF_FALSE);

			/*add data*/
			if (builder->OnDataReference)
				builder->OnDataReference(builder->cbk_obj, size, offset);
			else
				builder->OnData(builder->cbk_obj, nalu+offset, size, GF_FALSE);

			offset += size;
			bytesLeft -= size;

			/*flush no matter what (FUs cannot be agreggated)*/
			builder->rtp_header.Marker = (IsAUEnd && !bytesLeft) ? 1 : 0;
			builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
			builder->bytesInPacket = 0;

			if (bytesLeft) {
				builder->rtp_header.PayloadType = builder->PayloadType;
				builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
				builder->rtp_header.SequenceNumber += 1;
				builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
			}
		}
	}
	return GF_OK;
}

void latm_flush(GP_RTPPacketizer *builder)
{
	if (builder->bytesInPacket) {
		builder->OnPacketDone(builder->cbk_obj, &builder->rtp_header);
		builder->bytesInPacket = 0;
	}
	builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
}

GF_Err gp_rtp_builder_do_latm(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize, u32 duration)
{
	u32 size, latm_hdr_size, i, data_offset;
	Bool fragmented;
	unsigned char *latm_hdr;

	if (!data) {
		latm_flush(builder);
		return GF_OK;
	}

	if ((builder->flags & GP_RTP_PCK_USE_MULTI) && builder->max_ptime) {
		if ((u32) builder->sl_header.compositionTimeStamp + duration >= builder->rtp_header.TimeStamp + builder->max_ptime)
			latm_flush(builder);
	}
	/*compute max size for frame, flush current if this doesn't fit*/
	latm_hdr_size = (data_size / 255) + 1;
	if (latm_hdr_size+data_size > builder->Path_MTU - builder->bytesInPacket) {
		latm_flush(builder);
	}

	data_offset = 0;
	fragmented = GF_FALSE;
	while (data_size > 0) {
		latm_hdr_size = (data_size / 255) + 1;
		/*fragmenting*/
		if (latm_hdr_size + data_size > builder->Path_MTU) {
			assert(!builder->bytesInPacket);
			fragmented = GF_TRUE;
			latm_hdr_size = (builder->Path_MTU / 255) + 1;
			size = builder->Path_MTU - latm_hdr_size;
			builder->rtp_header.Marker = 0;
		}
		/*last fragment or full AU*/
		else {
			fragmented = GF_FALSE;
			size = data_size;
			builder->rtp_header.Marker = 1;
		}
		data_size -= size;

		/*create new RTP Packet if needed*/
		if (!builder->bytesInPacket) {
			builder->rtp_header.SequenceNumber += 1;
			builder->rtp_header.TimeStamp = (u32) builder->sl_header.compositionTimeStamp;
			builder->OnNewPacket(builder->cbk_obj, &builder->rtp_header);
		}

		/* compute AudioMuxUnit header */
		latm_hdr_size = (size / 255) + 1;
		latm_hdr = (unsigned char *)gf_malloc( sizeof(char) * latm_hdr_size);
		for (i=0; i<latm_hdr_size-1; i++)  latm_hdr[i] = 255;
		latm_hdr[latm_hdr_size-1] = size % 255;

		/*add LATM header IN ORDER in case we aggregate audioMuxElements in RTP*/
		builder->OnData(builder->cbk_obj, (char*) latm_hdr, latm_hdr_size, GF_FALSE);
		builder->bytesInPacket += latm_hdr_size;
		gf_free(latm_hdr);

		/*add payload*/
		if (builder->OnDataReference) {
			builder->OnDataReference(builder->cbk_obj, size, data_offset);
		} else
			builder->OnData(builder->cbk_obj, data, size, GF_FALSE);

		builder->bytesInPacket += size;

		data_offset += size;

		/*fragmented AU, always flush packet*/
		if (!builder->rtp_header.Marker) latm_flush(builder);
	}
	/*if the AU has been fragmented or we don't use RTP aggregation, flush*/
	if (! (builder->flags & GP_RTP_PCK_USE_MULTI) ) fragmented = GF_TRUE;
	if (fragmented) latm_flush(builder);

	return GF_OK;
}


#endif /*GPAC_DISABLE_STREAMING*/
