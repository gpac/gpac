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
#include <gpac/maths.h>

void InitSL_RTP(GF_SLConfig *slc);




GF_EXPORT
GP_RTPPacketizer *gf_rtp_builder_new(u32 rtp_payt, GF_SLConfig *slc, u32 flags,
                                     void *cbk_obj,
                                     void (*OnNewPacket)(void *cbk, GF_RTPHeader *header),
                                     void (*OnPacketDone)(void *cbk, GF_RTPHeader *header),
                                     void (*OnDataReference)(void *cbk, u32 payload_size, u32 offset_from_orig),
                                     void (*OnData)(void *cbk, u8 *data, u32 data_size, Bool is_head)
                                    )
{
	GP_RTPPacketizer *tmp;
	if (!rtp_payt || !cbk_obj | !OnPacketDone) return NULL;

	GF_SAFEALLOC(tmp, GP_RTPPacketizer);
	if (!tmp) return NULL;

	if (slc) {
		memcpy(&tmp->sl_config, slc, sizeof(GF_SLConfig));
	} else {
		memset(&tmp->sl_config, 0, sizeof(GF_SLConfig));
		tmp->sl_config.useTimestampsFlag = 1;
		tmp->sl_config.timestampLength = 32;
	}
	tmp->OnNewPacket = OnNewPacket;
	tmp->OnDataReference = OnDataReference;
	tmp->OnData = OnData;
	tmp->cbk_obj = cbk_obj;
	tmp->OnPacketDone = OnPacketDone;
	tmp->rtp_payt = rtp_payt;
	tmp->flags = flags;
	//default init
	tmp->sl_header.AU_sequenceNumber = 1;
	tmp->sl_header.packetSequenceNumber = 1;

	//we assume we start on a new AU
	tmp->sl_header.accessUnitStartFlag = 1;
	return tmp;
}

GF_EXPORT
void gf_rtp_builder_del(GP_RTPPacketizer *builder)
{
	if (!builder) return;

	if (builder->payload) gf_bs_del(builder->payload);
	if (builder->pck_hdr) gf_bs_del(builder->pck_hdr);
	gf_free(builder);
}

GF_EXPORT
GF_Err gf_rtp_builder_process(GP_RTPPacketizer *builder, u8 *data, u32 data_size, u8 IsAUEnd, u32 FullAUSize, u32 duration, u8 descIndex)
{
	if (!builder) return GF_BAD_PARAM;

	switch (builder->rtp_payt) {
	case GF_RTP_PAYT_MPEG4:
		return gp_rtp_builder_do_mpeg4(builder, data, data_size, IsAUEnd, FullAUSize);
#ifndef GPAC_DISABLE_AV_PARSERS
	case GF_RTP_PAYT_MPEG12_VIDEO:
		return gp_rtp_builder_do_mpeg12_video(builder, data, data_size, IsAUEnd, FullAUSize);
#endif
	case GF_RTP_PAYT_MPEG12_AUDIO:
		return gp_rtp_builder_do_mpeg12_audio(builder, data, data_size, IsAUEnd, FullAUSize);
	case GF_RTP_PAYT_H263:
		return gp_rtp_builder_do_h263(builder, data, data_size, IsAUEnd, FullAUSize);
	case GF_RTP_PAYT_AMR:
	case GF_RTP_PAYT_AMR_WB:
		return gp_rtp_builder_do_amr(builder, data, data_size, IsAUEnd, FullAUSize);
	case GF_RTP_PAYT_3GPP_TEXT:
		return gp_rtp_builder_do_tx3g(builder, data, data_size, IsAUEnd, FullAUSize, duration, descIndex);
	case GF_RTP_PAYT_H264_AVC:
	case GF_RTP_PAYT_H264_SVC:
		return gp_rtp_builder_do_avc(builder, data, data_size, IsAUEnd, FullAUSize);
	case GF_RTP_PAYT_QCELP:
		return gp_rtp_builder_do_qcelp(builder, data, data_size, IsAUEnd, FullAUSize);
	case GF_RTP_PAYT_EVRC_SMV:
		return gp_rtp_builder_do_smv(builder, data, data_size, IsAUEnd, FullAUSize);
	case GF_RTP_PAYT_LATM:
		return gp_rtp_builder_do_latm(builder, data, data_size, IsAUEnd, FullAUSize, duration);
#if GPAC_ENABLE_3GPP_DIMS_RTP
	case GF_RTP_PAYT_3GPP_DIMS:
		return gp_rtp_builder_do_dims(builder, data, data_size, IsAUEnd, FullAUSize, duration);
#endif
	case GF_RTP_PAYT_AC3:
		return gp_rtp_builder_do_ac3(builder, data, data_size, IsAUEnd, FullAUSize);
	case GF_RTP_PAYT_HEVC:
	case GF_RTP_PAYT_LHVC:
		return gp_rtp_builder_do_hevc(builder, data, data_size, IsAUEnd, FullAUSize);
	default:
		return GF_NOT_SUPPORTED;
	}
}


//Compute the #params of the slMap
GF_EXPORT
void gf_rtp_builder_init(GP_RTPPacketizer *builder, u8 PayloadType, u32 PathMTU, u32 max_ptime,
                         u32 StreamType, u32 codecid, u32 PL_ID,
                         u32 avgSize, u32 maxSize,
                         u32 avgTS, u32 maxDTS,
                         u32 IV_length, u32 KI_length,
                         char *pref_mode)
{
	u32 k, ismacrypt_flags;

	memset(&builder->slMap, 0, sizeof(GP_RTPSLMap));
	builder->Path_MTU = PathMTU;
	builder->PayloadType = PayloadType;
	builder->slMap.StreamType = StreamType;
	builder->slMap.CodecID = codecid;
	builder->slMap.PL_ID = PL_ID;
	builder->max_ptime = max_ptime;
	if (pref_mode) strcpy(builder->slMap.mode, pref_mode);


	//some cst vars
	builder->rtp_header.Version = 2;
	builder->rtp_header.PayloadType = builder->PayloadType;

	/*our max config is with 1 packet only (SingleSL)*/
	builder->first_sl_in_rtp = GF_TRUE;
	/*no AUX data*/
	builder->slMap.AuxiliaryDataSizeLength = 0;


	/*just compute max aggregation size*/
	switch (builder->rtp_payt) {
	case GF_RTP_PAYT_QCELP:
	case GF_RTP_PAYT_EVRC_SMV:
	case GF_RTP_PAYT_AMR:
	case GF_RTP_PAYT_AMR_WB:
	{
		u32 nb_pck = 1;
		u32 block_size = 160;
		/*compute max frames per packet - if no avg size, use max size per codec*/
		if (builder->flags & GP_RTP_PCK_USE_MULTI) {
			if (builder->rtp_payt == GF_RTP_PAYT_QCELP) {
				if (!avgSize) avgSize = 35;
				nb_pck = (PathMTU-1) / avgSize;	/*one-byte header*/
				if (nb_pck>10) nb_pck=10;	/*cf RFC2658*/
			} else if (builder->rtp_payt == GF_RTP_PAYT_EVRC_SMV) {
				if (!avgSize) avgSize = 23;
				nb_pck = (PathMTU) / avgSize;
				if (nb_pck>32) nb_pck=32;	/*cf RFC3558*/
			} else if (builder->rtp_payt == GF_RTP_PAYT_AMR_WB) {
				if (!avgSize) avgSize = 61;
				nb_pck = (PathMTU-1) / avgSize;
				block_size = 320;
			} else {
				if (!avgSize) avgSize = 32;
				nb_pck = (PathMTU-1) / avgSize;
			}
			if (max_ptime) {
				u32 max_pck = max_ptime / block_size;
				if (nb_pck > max_pck) nb_pck = max_pck;
			}
		}
		if (nb_pck<=1) {
			builder->flags &= ~(GP_RTP_PCK_USE_MULTI|GP_RTP_PCK_USE_INTERLEAVING);
			builder->auh_size = 1;
		} else {
			builder->auh_size = nb_pck;
		}
		/*remove all MPEG-4 and ISMA flags */
		builder->flags &= 0x07;
	}
	return;
	case GF_RTP_PAYT_LATM:
	case GF_RTP_PAYT_MPEG4:
		break;
	default:
		/*remove all MPEG-4 and ISMA flags */
		builder->flags &= 0x07;
		/*disable aggregation for visual streams, except for AVC where STAP/MTAP can be used*/
		if (StreamType==GF_STREAM_VISUAL) {
			if ((codecid != GF_CODECID_AVC) && (codecid != GF_CODECID_SVC) && (codecid != GF_CODECID_MVC) && (codecid != GF_CODECID_HEVC) && (codecid != GF_CODECID_LHVC)) {
				builder->flags &= ~GP_RTP_PCK_USE_MULTI;
			}
		}
		else if (avgSize && (PathMTU <= avgSize) ) {
			builder->flags &= ~GP_RTP_PCK_USE_MULTI;
		}
		return;
	}

	builder->slMap.IV_length = IV_length;
	builder->slMap.KI_length = KI_length;

	ismacrypt_flags = 0;
	if (builder->flags & GP_RTP_PCK_SELECTIVE_ENCRYPTION) ismacrypt_flags |= GP_RTP_PCK_SELECTIVE_ENCRYPTION;
	if (builder->flags & GP_RTP_PCK_KEY_IDX_PER_AU) ismacrypt_flags |= GP_RTP_PCK_KEY_IDX_PER_AU;

	/*mode setup*/
	if (!strnicmp(builder->slMap.mode, "AAC", 3)) {
		builder->flags = GP_RTP_PCK_USE_MULTI | GP_RTP_PCK_SIGNAL_SIZE | GP_RTP_PCK_SIGNAL_AU_IDX | ismacrypt_flags;
		/*if (builder->flags & GP_RTP_PCK_USE_INTERLEAVING) */
		builder->slMap.ConstantDuration = avgTS;

		/*AAC LBR*/
		if (maxSize < 63) {
			strcpy(builder->slMap.mode, "AAC-lbr");
			builder->slMap.IndexLength = builder->slMap.IndexDeltaLength = 2;
			builder->slMap.SizeLength = 6;
		}
		/*AAC HBR*/
		else {
			strcpy(builder->slMap.mode, "AAC-hbr");
			builder->slMap.IndexLength = builder->slMap.IndexDeltaLength = 3;
			builder->slMap.SizeLength = 13;
		}
		goto check_header;
	}
	if (!strnicmp(builder->slMap.mode, "CELP", 4)) {
		/*CELP-cbr*/
		if (maxSize == avgSize) {
			/*reset flags (interleaving forbidden)*/
			builder->flags = GP_RTP_PCK_USE_MULTI | ismacrypt_flags;
			strcpy(builder->slMap.mode, "CELP-cbr");
			builder->slMap.ConstantSize = avgSize;
			builder->slMap.ConstantDuration = avgTS;
		}
		/*CELP VBR*/
		else {
			strcpy(builder->slMap.mode, "CELP-vbr");
			builder->slMap.IndexLength = builder->slMap.IndexDeltaLength = 2;
			builder->slMap.SizeLength = 6;
			/*if (builder->flags & GP_RTP_PCK_USE_INTERLEAVING) */builder->slMap.ConstantDuration = avgTS;
			builder->flags = GP_RTP_PCK_USE_MULTI | GP_RTP_PCK_SIGNAL_SIZE | GP_RTP_PCK_SIGNAL_AU_IDX | ismacrypt_flags;
		}
		goto check_header;
	}

	/*generic setup by flags*/

	/*size*/
	if (builder->flags & GP_RTP_PCK_SIGNAL_SIZE) {
		if (avgSize==maxSize) {
			builder->slMap.SizeLength = 0;
			builder->slMap.ConstantSize = maxSize;
		} else {
			builder->slMap.SizeLength = gf_get_bit_size(maxSize ? maxSize : PathMTU);
			builder->slMap.ConstantSize = 0;
		}
	} else {
		builder->slMap.SizeLength = 0;
		if (builder->flags & GP_RTP_PCK_USE_MULTI)
			builder->slMap.ConstantSize = (avgSize==maxSize) ? maxSize : 0;
		else
			builder->slMap.ConstantSize = 0;
	}

	/*single SL per RTP*/
	if (!(builder->flags & GP_RTP_PCK_USE_MULTI)) {
		if ( builder->sl_config.AUSeqNumLength && (builder->flags & GP_RTP_PCK_SIGNAL_AU_IDX)) {
			builder->slMap.IndexLength = builder->sl_config.AUSeqNumLength;
		} else {
			builder->slMap.IndexLength = 0;
		}
		/*one packet per RTP so no delta*/
		builder->slMap.IndexDeltaLength = 0;
		builder->slMap.IV_delta_length = 0;

		/*CTS Delta is always 0 since we have one SL packet per RTP*/
		builder->slMap.CTSDeltaLength = 0;

		/*DTS Delta depends on the video type*/
		if ((builder->flags & GP_RTP_PCK_SIGNAL_TS) && maxDTS )
			builder->slMap.DTSDeltaLength = gf_get_bit_size(maxDTS);
		else
			builder->slMap.DTSDeltaLength = 0;

		/*RAP*/
		if (builder->sl_config.useRandomAccessPointFlag && (builder->flags & GP_RTP_PCK_SIGNAL_RAP)) {
			builder->slMap.RandomAccessIndication = GF_TRUE;
		} else {
			builder->slMap.RandomAccessIndication = GF_FALSE;
		}

		/*stream state*/
		if (builder->flags & GP_RTP_PCK_SYSTEMS_CAROUSEL) {
			if (!builder->sl_config.AUSeqNumLength) builder->sl_config.AUSeqNumLength = 4;
			builder->slMap.StreamStateIndication = builder->sl_config.AUSeqNumLength;
		}
		goto check_header;
	}

	/*this is the avg samples we can store per RTP packet*/
	k = PathMTU / avgSize;
	if (k<=1) {
		builder->flags &= ~GP_RTP_PCK_USE_MULTI;
		/*keep TS signaling for B-frames (eg never default to M4V-ES when B-frames are present)*/
		//builder->flags &= ~GP_RTP_PCK_SIGNAL_TS;
		builder->flags &= ~GP_RTP_PCK_SIGNAL_SIZE;
		builder->flags &= ~GP_RTP_PCK_SIGNAL_AU_IDX;
		builder->flags &= ~GP_RTP_PCK_USE_INTERLEAVING;
		builder->flags &= ~GP_RTP_PCK_KEY_IDX_PER_AU;
		gf_rtp_builder_init(builder, PayloadType, PathMTU, max_ptime, StreamType, codecid, PL_ID, avgSize, maxSize, avgTS, maxDTS, IV_length, KI_length, pref_mode);
		return;
	}

	/*multiple SL per RTP - check if we have to send TS*/
	builder->slMap.ConstantDuration = builder->sl_config.CUDuration;
	if (!builder->slMap.ConstantDuration) {
		builder->flags |= GP_RTP_PCK_SIGNAL_TS;
	}
	/*if we have a constant duration and are not writting TSs, make sure we write AU IDX when interleaving*/
	else if (! (builder->flags & GP_RTP_PCK_SIGNAL_TS) && (builder->flags & GP_RTP_PCK_USE_INTERLEAVING)) {
		builder->flags |= GP_RTP_PCK_SIGNAL_AU_IDX;
	}

	if (builder->flags & GP_RTP_PCK_SIGNAL_TS) {
		/*compute CTS delta*/
		builder->slMap.CTSDeltaLength = gf_get_bit_size(k*avgTS);

		/*compute DTS delta. Delta is ALWAYS from the CTS of the same sample*/
		if (maxDTS)
			builder->slMap.DTSDeltaLength = gf_get_bit_size(maxDTS);
		else
			builder->slMap.DTSDeltaLength = 0;
	}

	if ((builder->flags & GP_RTP_PCK_SIGNAL_AU_IDX) && builder->sl_config.AUSeqNumLength) {
		builder->slMap.IndexLength = builder->sl_config.AUSeqNumLength;
		/*and k-1 AUs in Delta*/
		builder->slMap.IndexDeltaLength = (builder->flags & GP_RTP_PCK_USE_INTERLEAVING) ? gf_get_bit_size(k-1) : 0;
	}
	else if (builder->flags & GP_RTP_PCK_SYSTEMS_CAROUSEL) {
		if (!builder->sl_config.AUSeqNumLength) builder->sl_config.AUSeqNumLength = 4;
		builder->slMap.StreamStateIndication = builder->sl_config.AUSeqNumLength;
	}

	/*RAP*/
	if (builder->sl_config.useRandomAccessPointFlag && (builder->flags & GP_RTP_PCK_SIGNAL_RAP)) {
		builder->slMap.RandomAccessIndication = GF_TRUE;
	} else {
		builder->slMap.RandomAccessIndication = GF_FALSE;
	}

check_header:

	/*IV delta only if interleaving (otherwise reconstruction from IV is trivial)*/
	if (IV_length && (builder->flags & GP_RTP_PCK_USE_INTERLEAVING)) {
		builder->slMap.IV_delta_length = gf_get_bit_size(maxSize);
	}
	/*ISMACryp video mode*/
	if ((builder->slMap.StreamType==GF_STREAM_VISUAL) && (builder->slMap.CodecID==GF_CODECID_MPEG4_PART2)
	        && (builder->flags & GP_RTP_PCK_SIGNAL_RAP) && builder->slMap.IV_length
	        && !(builder->flags & GP_RTP_PCK_SIGNAL_AU_IDX) && !(builder->flags & GP_RTP_PCK_SIGNAL_SIZE)
	        /*shall have SignalTS*/
	        && (builder->flags & GP_RTP_PCK_SIGNAL_TS) && !(builder->flags & GP_RTP_PCK_USE_MULTI)
	   ) {
		strcpy(builder->slMap.mode, "mpeg4-video");
	}
	/*ISMACryp AVC video mode*/
	else if ((builder->slMap.StreamType==GF_STREAM_VISUAL) && (builder->slMap.CodecID==GF_CODECID_AVC)
	         && (builder->flags & GP_RTP_PCK_SIGNAL_RAP) && builder->slMap.IV_length
	         && !(builder->flags & GP_RTP_PCK_SIGNAL_AU_IDX) && !(builder->flags & GP_RTP_PCK_SIGNAL_SIZE)
	         /*shall have SignalTS*/
	         && (builder->flags & GP_RTP_PCK_SIGNAL_TS) && !(builder->flags & GP_RTP_PCK_USE_MULTI)
	        ) {
		strcpy(builder->slMap.mode, "avc-video");
	}

	/*check if we use AU header or not*/
	if (!builder->slMap.SizeLength
	        && !builder->slMap.IndexLength
	        && !builder->slMap.IndexDeltaLength
	        && !builder->slMap.DTSDeltaLength
	        && !builder->slMap.CTSDeltaLength
	        && !builder->slMap.RandomAccessIndication
	        && !builder->slMap.IV_length
	        && !builder->slMap.KI_length
	   ) {
		builder->has_AU_header = GF_FALSE;
	} else {
		builder->has_AU_header = GF_TRUE;
	}
}

void gp_rtp_builder_set_cryp_info(GP_RTPPacketizer *builder, u64 IV, char *key_indicator, Bool is_encrypted)
{
	if (!key_indicator) {
		if (builder->key_indicator) {
			/*force flush if no provision for keyIndicator per AU*/
			builder->force_flush = (builder->flags & GP_RTP_PCK_KEY_IDX_PER_AU) ? GF_FALSE : GF_TRUE;
			gf_free(builder->key_indicator);
			builder->key_indicator = NULL;
		}
	} else if (!builder->key_indicator
	           ||
	           memcmp(builder->key_indicator, key_indicator, sizeof(char)*builder->slMap.KI_length)
	          ) {
		/*force flush if no provision for keyIndicator per AU*/
		builder->force_flush = (builder->flags & GP_RTP_PCK_KEY_IDX_PER_AU) ? GF_FALSE : GF_TRUE;

		if (!builder->key_indicator) builder->key_indicator = (char *) gf_malloc(sizeof(char)*builder->slMap.KI_length);
		memcpy(builder->key_indicator, key_indicator, sizeof(char)*builder->slMap.KI_length);
	}
	if (builder->IV != IV) {
		builder->IV = IV;
		if (builder->slMap.IV_delta_length && (builder->slMap.IV_delta_length < gf_get_bit_size((u32) (IV - builder->first_AU_IV) ))) {
			builder->first_AU_IV = IV;
			builder->force_flush = GF_TRUE;
		}
	}
	builder->is_encrypted = is_encrypted;
}

GF_EXPORT
Bool gf_rtp_builder_get_payload_name(GP_RTPPacketizer *rtpb, char *szPayloadName, char *szMediaName)
{
	u32 flags = rtpb->flags;

	switch (rtpb->rtp_payt) {
	case GF_RTP_PAYT_MPEG4:
		if ((rtpb->slMap.StreamType==GF_STREAM_VISUAL) && (rtpb->slMap.CodecID==GF_CODECID_MPEG4_PART2)) {
			strcpy(szMediaName, "video");
			/*ISMACryp video*/
			if ( (flags & GP_RTP_PCK_SIGNAL_RAP) && rtpb->slMap.IV_length
			        && !(flags & GP_RTP_PCK_SIGNAL_AU_IDX) && !(flags & GP_RTP_PCK_SIGNAL_SIZE)
			        && (flags & GP_RTP_PCK_SIGNAL_TS) && !(flags & GP_RTP_PCK_USE_MULTI)
			   )
			{
				strcpy(szPayloadName, "enc-mpeg4-generic");
				return GF_TRUE;
			}
			/*mpeg4-generic*/
			if ( (flags & GP_RTP_PCK_SIGNAL_RAP) || (flags & GP_RTP_PCK_SIGNAL_AU_IDX) || (flags & GP_RTP_PCK_SIGNAL_SIZE)
			        || (flags & GP_RTP_PCK_SIGNAL_TS) || (flags & GP_RTP_PCK_USE_MULTI) ) {
				strcpy(szPayloadName, "mpeg4-generic");
				return GF_TRUE;
			} else {
				strcpy(szPayloadName, "MP4V-ES");
				return GF_TRUE;
			}
		}
		/*for all other types*/
		if (rtpb->slMap.StreamType==GF_STREAM_AUDIO) strcpy(szMediaName, "audio");
		else if (rtpb->slMap.StreamType==GF_STREAM_MPEGJ) strcpy(szMediaName, "application");
		else strcpy(szMediaName, "video");
		strcpy(szPayloadName, rtpb->slMap.IV_length ? "enc-mpeg4-generic" : "mpeg4-generic");
		return GF_TRUE;
	case GF_RTP_PAYT_MPEG12_VIDEO:
		strcpy(szMediaName, "video");
		strcpy(szPayloadName, "MPV");
		return GF_TRUE;
	case GF_RTP_PAYT_MPEG12_AUDIO:
		strcpy(szMediaName, "audio");
		strcpy(szPayloadName, "MPA");
		return GF_TRUE;
	case GF_RTP_PAYT_H263:
		strcpy(szMediaName, "video");
		strcpy(szPayloadName, "H263-1998");
		return GF_TRUE;
	case GF_RTP_PAYT_AMR:
		strcpy(szMediaName, "audio");
		strcpy(szPayloadName, "AMR");
		return GF_TRUE;
	case GF_RTP_PAYT_AMR_WB:
		strcpy(szMediaName, "audio");
		strcpy(szPayloadName, "AMR-WB");
		return GF_TRUE;
	case GF_RTP_PAYT_3GPP_TEXT:
		strcpy(szMediaName, "text");
		strcpy(szPayloadName, "3gpp-tt");
		return GF_TRUE;
	case GF_RTP_PAYT_H264_AVC:
		strcpy(szMediaName, "video");
		strcpy(szPayloadName, "H264");
		return GF_TRUE;
	case GF_RTP_PAYT_QCELP:
		strcpy(szMediaName, "audio");
		strcpy(szPayloadName, "QCELP");
		return GF_TRUE;
	case GF_RTP_PAYT_EVRC_SMV:
		strcpy(szMediaName, "audio");
		strcpy(szPayloadName, (rtpb->slMap.CodecID==0xA0) ? "EVRC" : "SMV");
		/*header-free version*/
		if (rtpb->auh_size<=1) strcat(szPayloadName, "0");
		return GF_TRUE;
	case GF_RTP_PAYT_LATM:
		strcpy(szMediaName, "audio");
		strcpy(szPayloadName, "MP4A-LATM");
		return GF_TRUE;
#if GPAC_ENABLE_3GPP_DIMS_RTP
	case GF_RTP_PAYT_3GPP_DIMS:
		strcpy(szMediaName, "video");
		strcpy(szPayloadName, "richmedia+xml");
		return GF_TRUE;
#endif
	case GF_RTP_PAYT_AC3:
		strcpy(szMediaName, "audio");
		strcpy(szPayloadName, "ac3");
		return GF_TRUE;
	case GF_RTP_PAYT_H264_SVC:
		strcpy(szMediaName, "video");
		strcpy(szPayloadName, "H264-SVC");
		return GF_TRUE;
	case GF_RTP_PAYT_HEVC:
		strcpy(szMediaName, "video");
		strcpy(szPayloadName, "H265");
		return GF_TRUE;
	case GF_RTP_PAYT_LHVC:
		strcpy(szMediaName, "video");
		strcpy(szPayloadName, "H265-SHVC");
		return GF_TRUE;
	default:
		strcpy(szMediaName, "");
		strcpy(szPayloadName, "");
		return GF_FALSE;
	}
	return GF_FALSE;
}


GF_EXPORT
GF_Err gf_rtp_builder_format_sdp(GP_RTPPacketizer *builder, char *payload_name, char *sdpLine, char *dsi, u32 dsi_size)
{
	char buffer[20000], dsiString[20000];
	u32 i, k;
	Bool is_first = GF_TRUE;

	if ((builder->rtp_payt!=GF_RTP_PAYT_MPEG4) && (builder->rtp_payt!=GF_RTP_PAYT_LATM) ) return GF_BAD_PARAM;

#define SDP_ADD_INT(_name, _val) { if (!is_first) strcat(sdpLine, "; "); sprintf(buffer, "%s=%d", _name, _val); strcat(sdpLine, buffer); is_first = 0;}
#define SDP_ADD_STR(_name, _val) { if (!is_first) strcat(sdpLine, "; "); sprintf(buffer, "%s=%s", _name, _val); strcat(sdpLine, buffer); is_first = 0;}

	sprintf(sdpLine, "a=fmtp:%d ", builder->PayloadType);

	/*mandatory fields*/
	if (builder->slMap.PL_ID) SDP_ADD_INT("profile-level-id", builder->slMap.PL_ID);

	if (builder->rtp_payt == GF_RTP_PAYT_LATM) SDP_ADD_INT("cpresent", 0);

	if (dsi && dsi_size) {
		k = 0;
		for (i=0; i<dsi_size; i++) {
			sprintf(&dsiString[k], "%02x", (unsigned char) dsi[i]);
			k+=2;
		}
		dsiString[k] = 0;
		SDP_ADD_STR("config", dsiString);
	}
	if (!strcmp(payload_name, "MP4V-ES") || (builder->rtp_payt == GF_RTP_PAYT_LATM) ) return GF_OK;

	SDP_ADD_INT("streamType", builder->slMap.StreamType);
	if (strcmp(builder->slMap.mode, "") && strcmp(builder->slMap.mode, "default")) {
		SDP_ADD_STR("mode", builder->slMap.mode);
	} else {
		SDP_ADD_STR("mode", "generic");
	}

	/*optional fields*/
	if (builder->slMap.CodecID) SDP_ADD_INT("objectType", builder->slMap.CodecID);
	if (builder->slMap.ConstantSize) SDP_ADD_INT("constantSize", builder->slMap.ConstantSize);
	if (builder->slMap.ConstantDuration) SDP_ADD_INT("constantDuration", builder->slMap.ConstantDuration);
	if (builder->slMap.maxDisplacement) SDP_ADD_INT("maxDisplacement", builder->slMap.maxDisplacement);
	if (builder->slMap.deinterleaveBufferSize) SDP_ADD_INT("de-interleaveBufferSize", builder->slMap.deinterleaveBufferSize);
	if (builder->slMap.SizeLength) SDP_ADD_INT("sizeLength", builder->slMap.SizeLength);
	if (builder->slMap.IndexLength) SDP_ADD_INT("indexLength", builder->slMap.IndexLength);
	if (builder->slMap.IndexDeltaLength) SDP_ADD_INT("indexDeltaLength", builder->slMap.IndexDeltaLength);
	if (builder->slMap.CTSDeltaLength) SDP_ADD_INT("CTSDeltaLength", builder->slMap.CTSDeltaLength);
	if (builder->slMap.DTSDeltaLength) SDP_ADD_INT("DTSDeltaLength", builder->slMap.DTSDeltaLength);
	if (builder->slMap.RandomAccessIndication) SDP_ADD_INT("randomAccessIndication", builder->slMap.RandomAccessIndication);
	if (builder->slMap.StreamStateIndication) SDP_ADD_INT("streamStateIndication", builder->slMap.StreamStateIndication);
	if (builder->slMap.AuxiliaryDataSizeLength) SDP_ADD_INT("auxiliaryDataSizeLength", builder->slMap.AuxiliaryDataSizeLength);

	/*ISMACryp config*/
	if (builder->slMap.IV_length) {
		/*don't write default*/
		/*SDP_ADD_STR("ISMACrypCryptoSuite", "AES_CTR_128");*/
		if (builder->flags & GP_RTP_PCK_SELECTIVE_ENCRYPTION) SDP_ADD_INT("ISMACrypSelectiveEncryption", 1);
		SDP_ADD_INT("ISMACrypIVLength", builder->slMap.IV_length);
		if (builder->slMap.IV_delta_length) SDP_ADD_INT("ISMACrypDeltaIVLength", builder->slMap.IV_delta_length);
		if (builder->slMap.KI_length) SDP_ADD_INT("ISMACrypKeyIndicatorLength", builder->slMap.KI_length);
		if (builder->flags & GP_RTP_PCK_KEY_IDX_PER_AU) SDP_ADD_INT("ISMACrypKeyIndicatorPerAU", 1);
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_STREAMING*/
