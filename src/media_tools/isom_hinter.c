/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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

#include <gpac/internal/media_dev.h>
#include <gpac/base_coding.h>
#include <gpac/mpeg4_odf.h>
#include <gpac/constants.h>
#include <gpac/math.h>
#include <gpac/ietf.h>

#ifndef GPAC_DISABLE_ISOM

void gf_media_get_sample_average_infos(GF_ISOFile *file, u32 Track, u32 *avgSize, u32 *MaxSize, u32 *TimeDelta, u32 *maxCTSDelta, u32 *const_duration, u32 *bandwidth)
{
	u32 i, count, ts_diff;
	u64 prevTS, tdelta;
	Double bw;
	GF_ISOSample *samp;

	*avgSize = *MaxSize = 0;
	*TimeDelta = 0;
	*maxCTSDelta = 0;
	bw = 0;
	prevTS = 0;
	tdelta = 0;

	count = gf_isom_get_sample_count(file, Track);
	*const_duration = 0;

	for (i=0; i<count; i++) {
		samp = gf_isom_get_sample_info(file, Track, i+1, NULL, NULL);
		//get the size
		*avgSize += samp->dataLength;
		if (*MaxSize < samp->dataLength) *MaxSize = samp->dataLength;
		ts_diff = (u32) (samp->DTS+samp->CTS_Offset - prevTS);
		//get the time
		tdelta += ts_diff;

		if (i==1) {
			*const_duration = ts_diff;
		} else if ( (i<count-1) && (*const_duration != ts_diff) ) {
			*const_duration = 0;
		}

		prevTS = samp->DTS+samp->CTS_Offset;
		bw += 8*samp->dataLength;
		
		//get the CTS delta
		if ((samp->CTS_Offset>=0) && ((u32)samp->CTS_Offset > *maxCTSDelta))
			*maxCTSDelta = samp->CTS_Offset;
		gf_isom_sample_del(&samp);
	}
	if (count>1) *TimeDelta = (u32) (tdelta/ (count-1) );
	else *TimeDelta = (u32) tdelta;
	*avgSize /= count;
	bw *= gf_isom_get_media_timescale(file, Track);
	bw /= (s64) gf_isom_get_media_duration(file, Track);
	bw /= 1000;
	(*bandwidth) = (u32) (bw+0.5);

	//delta is NOT an average, we need to know exactly how many bits are
	//needed to encode CTS-DTS for ANY samples
}


#ifndef GPAC_DISABLE_ISOM_HINTING

/*RTP track hinter*/
struct __tag_isom_hinter
{
	GF_ISOFile *file;
	/*IDs are kept for mp4 hint sample building*/
	u32 TrackNum, TrackID, HintTrack, HintID;
	/*current Hint sample and associated RTP time*/
	u32 HintSample, RTPTime;

	/*track has composition time offset*/
	Bool has_ctts;
	/*remember if first SL packet in RTP packet is RAP*/
	u8 SampleIsRAP;
	u32 base_offset_in_sample;
	u32 OrigTimeScale;
	/*rtp builder*/
	GP_RTPPacketizer *rtp_p;

	u32 bandwidth, nb_chan;

	/*NALU size for H264/AVC*/
	u32 avc_nalu_size;

	/*stats*/
	u32 TotalSample, CurrentSample;
};


/*
	offset for group ID for hint tracks in SimpleAV mode when all media data
	is copied to the hint track (no use interleaving hint and original in this case)
	this offset is applied internally by the track hinter. Thus you shouldn't
	specify a GroupID >= OFFSET_HINT_GROUP_ID if you want the lib to perform efficient
	interleaving in any cases (referenced or copied media)
*/
#define OFFSET_HINT_GROUP_ID	0x8000

void MP4T_DumpSDP(GF_ISOFile *file, const char *name)
{
	const char *sdp;
	u32 size, i;
	FILE *f;

	f = gf_f64_open(name, "wt");
	//get the movie SDP
	gf_isom_sdp_get(file, &sdp, &size);
	gf_fwrite(sdp, size, 1, f);
	fprintf(f, "\r\n");

	//then tracks
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (gf_isom_get_media_type(file, i+1) != GF_ISOM_MEDIA_HINT) continue;
		gf_isom_sdp_track_get(file, i+1, &sdp, &size);
		gf_fwrite(sdp, size, 1, f);
	}
	fclose(f);
}


void InitSL_RTP(GF_SLConfig *slc)
{
	memset(slc, 0, sizeof(GF_SLConfig));
	slc->tag = GF_ODF_SLC_TAG;
	slc->useTimestampsFlag = 1;
	slc->timestampLength = 32;
}

void InitSL_NULL(GF_SLConfig *slc)
{
	memset(slc, 0, sizeof(GF_SLConfig));
	slc->tag = GF_ODF_SLC_TAG;
	slc->predefined = 0x01;
}



void MP4T_OnPacketDone(void *cbk, GF_RTPHeader *header)
{
	u8 disposable;
	GF_RTPHinter *tkHint = (GF_RTPHinter *)cbk;
	if (!tkHint || !tkHint->HintSample) return;
	assert(header->TimeStamp == tkHint->RTPTime);
	
	disposable = 0;
	if (tkHint->avc_nalu_size) {
		disposable = tkHint->rtp_p->avc_non_idr ? 1 : 0;
	}
	/*for all other, assume that CTS=DTS means B-frame -> disposable*/
	else if (tkHint->has_ctts && (tkHint->rtp_p->sl_header.compositionTimeStamp==tkHint->rtp_p->sl_header.decodingTimeStamp)) {
		disposable = 1;
	}

	gf_isom_rtp_packet_set_flags(tkHint->file, tkHint->HintTrack, 0, 0, header->Marker, disposable, 0);
}


void MP4T_OnDataRef(void *cbk, u32 payload_size, u32 offset_from_orig)
{
	GF_RTPHinter *tkHint = (GF_RTPHinter *)cbk;
	if (!tkHint || !payload_size) return;

	/*add reference*/
	gf_isom_hint_sample_data(tkHint->file, tkHint->HintTrack, tkHint->TrackID,
			tkHint->CurrentSample, (u16) payload_size, offset_from_orig + tkHint->base_offset_in_sample, 
			NULL, 0);
}

void MP4T_OnData(void *cbk, char *data, u32 data_size, Bool is_header)
{
	u8 at_begin;
	GF_RTPHinter *tkHint = (GF_RTPHinter *)cbk;
	if (!data_size) return;

	at_begin = is_header ? 1 : 0;
	if (data_size <= 14) {
		gf_isom_hint_direct_data(tkHint->file, tkHint->HintTrack, data, data_size, at_begin);
	} else {
		gf_isom_hint_sample_data(tkHint->file, tkHint->HintTrack, tkHint->HintID, 0, (u16) data_size, 0, data, at_begin);
	}
}


void MP4T_OnNewPacket(void *cbk, GF_RTPHeader *header)
{
	s32 res;
	GF_RTPHinter *tkHint = (GF_RTPHinter *)cbk;
	if (!tkHint) return;

	res = (s32) (tkHint->rtp_p->sl_header.compositionTimeStamp - tkHint->rtp_p->sl_header.decodingTimeStamp);
	assert( !res || tkHint->has_ctts);
	/*do we need a new sample*/
	if (!tkHint->HintSample || (tkHint->RTPTime != header->TimeStamp)) {
		/*close current sample*/
		if (tkHint->HintSample) gf_isom_end_hint_sample(tkHint->file, tkHint->HintTrack, tkHint->SampleIsRAP);

		/*start new sample: We use DTS as the sampling instant (RTP TS) to make sure
		all packets are sent in order*/
		gf_isom_begin_hint_sample(tkHint->file, tkHint->HintTrack, 1, header->TimeStamp-res);
		tkHint->HintSample ++;
		tkHint->RTPTime = header->TimeStamp;
		tkHint->SampleIsRAP = tkHint->rtp_p->sl_config.hasRandomAccessUnitsOnlyFlag ? 1 : tkHint->rtp_p->sl_header.randomAccessPointFlag;
	}
	/*create an RTP Packet with the appropriated marker flag - note: the flags are temp ones, 
	they are set when the full packet is signaled (to handle multi AUs per RTP)*/
	gf_isom_rtp_packet_begin(tkHint->file, tkHint->HintTrack, 0, 0, 0, header->Marker, header->PayloadType, 0, 0, header->SequenceNumber);
	/*Add the delta TS to make sure RTP TS is indeed the CTS (sampling time)*/
	if (res) gf_isom_rtp_packet_set_offset(tkHint->file, tkHint->HintTrack, res);
}


GF_EXPORT
GF_RTPHinter *gf_hinter_track_new(GF_ISOFile *file, u32 TrackNum, 
							u32 Path_MTU, u32 max_ptime, u32 default_rtp_rate, u32 flags, u8 PayloadID, 
							Bool copy_media, u32 InterleaveGroupID, u8 InterleaveGroupPriority, GF_Err *e)
{

	GF_SLConfig my_sl;
	u32 descIndex, MinSize, MaxSize, avgTS, streamType, oti, const_dur, nb_ch, maxDTSDelta;
	u8 OfficialPayloadID;
	u32 TrackMediaSubType, TrackMediaType, hintType, nbEdts, required_rate, force_dts_delta, avc_nalu_size, PL_ID, bandwidth, IV_length, KI_length;
	const char *url, *urn;
	char *mpeg4mode;
	Bool is_crypted, has_mpeg4_mapping;
	GF_RTPHinter *tmp;
	GF_ESD *esd;

	*e = GF_BAD_PARAM;
	if (!file || !TrackNum || !gf_isom_get_track_id(file, TrackNum)) return NULL;

	if (!gf_isom_get_sample_count(file, TrackNum)) {
		*e = GF_OK;
		return NULL;
	}
	*e = GF_NOT_SUPPORTED;
	nbEdts = gf_isom_get_edit_segment_count(file, TrackNum);
	if (nbEdts>1) {
		u64 et, sd, mt;
		u8 em;
		gf_isom_get_edit_segment(file, TrackNum, 1, &et, &sd, &mt, &em);
		if ((nbEdts>2) || (em!=GF_ISOM_EDIT_EMPTY)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[rtp hinter] Cannot hint track whith EditList\n"));
			return NULL;
		}
	}
	if (nbEdts) gf_isom_remove_edit_segments(file, TrackNum);

	if (!gf_isom_is_track_enabled(file, TrackNum)) return NULL;

	/*by default NO PL signaled*/
	PL_ID = 0;
	OfficialPayloadID = 0;
	force_dts_delta = 0;
	streamType = oti = 0;
	mpeg4mode = NULL;
	required_rate = 0;
	is_crypted = 0;
	IV_length = KI_length = 0;
	oti = 0;
	nb_ch = 0;
	avc_nalu_size = 0;
	has_mpeg4_mapping = 1;
	TrackMediaType = gf_isom_get_media_type(file, TrackNum);
	TrackMediaSubType = gf_isom_get_media_subtype(file, TrackNum, 1);
	
	/*for max compatibility with QT*/
	if (!default_rtp_rate) default_rtp_rate = 90000;

	/*timed-text is a bit special, we support multiple stream descriptions & co*/
	if ( (TrackMediaType==GF_ISOM_MEDIA_TEXT) || (TrackMediaType==GF_ISOM_MEDIA_SUBT)) {
		hintType = GF_RTP_PAYT_3GPP_TEXT;
		oti = GPAC_OTI_TEXT_MPEG4;
		streamType = GF_STREAM_TEXT;
		/*fixme - this works cos there's only one PL for text in mpeg4 at the current time*/
		PL_ID = 0x10;
	} else {
		if (gf_isom_get_sample_description_count(file, TrackNum) > 1) return NULL;

		TrackMediaSubType = gf_isom_get_media_subtype(file, TrackNum, 1);
		switch (TrackMediaSubType) {
		case GF_ISOM_SUBTYPE_MPEG4_CRYP: 
			is_crypted = 1;
		case GF_ISOM_SUBTYPE_MPEG4:
			esd = gf_isom_get_esd(file, TrackNum, 1);
			hintType = GF_RTP_PAYT_MPEG4;
			if (esd) {
				streamType = esd->decoderConfig->streamType;
				oti = esd->decoderConfig->objectTypeIndication;
				if (esd->URLString) hintType = 0;
				/*AAC*/
				if ((streamType==GF_STREAM_AUDIO) && esd->decoderConfig->decoderSpecificInfo
				/*(nb: we use mpeg4 for MPEG-2 AAC)*/
				&& ((oti==GPAC_OTI_AUDIO_AAC_MPEG4) || (oti==GPAC_OTI_AUDIO_AAC_MPEG4) || (oti==GPAC_OTI_AUDIO_AAC_MPEG2_MP) || (oti==GPAC_OTI_AUDIO_AAC_MPEG2_LCP) || (oti==GPAC_OTI_AUDIO_AAC_MPEG2_SSRP)) ) {

					u32 sample_rate;
					GF_M4ADecSpecInfo a_cfg;
					gf_m4a_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &a_cfg);
					nb_ch = a_cfg.nb_chan;
					sample_rate = a_cfg.base_sr;
					PL_ID = a_cfg.audioPL;
					switch (a_cfg.base_object_type) {
					case GF_M4A_AAC_MAIN:
					case GF_M4A_AAC_LC:
						if (flags & GP_RTP_PCK_USE_LATM_AAC) {
							hintType = GF_RTP_PAYT_LATM;
							break;
						}
					case GF_M4A_AAC_SBR:
					case GF_M4A_AAC_PS:
					case GF_M4A_AAC_LTP:
					case GF_M4A_AAC_SCALABLE:
					case GF_M4A_ER_AAC_LC:
					case GF_M4A_ER_AAC_LTP:
					case GF_M4A_ER_AAC_SCALABLE:
						mpeg4mode = "AAC";
						break;
					case GF_M4A_CELP:
					case GF_M4A_ER_CELP:
						mpeg4mode = "CELP";
						break;
					}
					required_rate = sample_rate;
				}
				/*MPEG1/2 audio*/
				else if ((streamType==GF_STREAM_AUDIO) && ((oti==GPAC_OTI_AUDIO_MPEG2_PART3) || (oti==GPAC_OTI_AUDIO_MPEG1))) {
					u32 sample_rate;
					if (!is_crypted) {
						GF_ISOSample *samp = gf_isom_get_sample(file, TrackNum, 1, NULL);
						u32 hdr = GF_4CC((u8)samp->data[0], (u8)samp->data[1], (u8)samp->data[2], (u8)samp->data[3]);
						nb_ch = gf_mp3_num_channels(hdr);
						sample_rate = gf_mp3_sampling_rate(hdr);
						gf_isom_sample_del(&samp);
						hintType = GF_RTP_PAYT_MPEG12_AUDIO;
						/*use official RTP/AVP payload type*/
						OfficialPayloadID = 14;
						required_rate = 90000;
					}
					/*encrypted MP3 must be sent through MPEG-4 generic to signal all ISMACryp stuff*/
					else {
						u8 bps;
						gf_isom_get_audio_info(file, TrackNum, 1, &sample_rate, &nb_ch, &bps);
						required_rate = sample_rate;
					}
				}
				/*QCELP audio*/
				else if ((streamType==GF_STREAM_AUDIO) && (oti==GPAC_OTI_AUDIO_13K_VOICE)) {
					hintType = GF_RTP_PAYT_QCELP;
					OfficialPayloadID = 12;
					required_rate = 8000;
					streamType = GF_STREAM_AUDIO;
					nb_ch = 1;
				}
				/*EVRC/SVM audio*/
				else if ((streamType==GF_STREAM_AUDIO) && ((oti==GPAC_OTI_AUDIO_EVRC_VOICE) || (oti==GPAC_OTI_AUDIO_SMV_VOICE)) ) {
					hintType = GF_RTP_PAYT_EVRC_SMV;
					required_rate = 8000;
					streamType = GF_STREAM_AUDIO;
					nb_ch = 1;
				}
				/*visual streams*/
				else if (streamType==GF_STREAM_VISUAL) {
					if (oti==GPAC_OTI_VIDEO_MPEG4_PART2) {
						GF_M4VDecSpecInfo dsi;
						gf_m4v_get_config(esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength, &dsi);
						PL_ID = dsi.VideoPL;
					}
					/*MPEG1/2 video*/
					if ( ((oti>=GPAC_OTI_VIDEO_MPEG2_SIMPLE) && (oti<=GPAC_OTI_VIDEO_MPEG2_422)) || (oti==GPAC_OTI_VIDEO_MPEG1)) {
						if (!is_crypted) {
							hintType = GF_RTP_PAYT_MPEG12_VIDEO;
							OfficialPayloadID = 32;
						}
					}
					/*for ISMA*/
					if (is_crypted) {
						/*that's another pain with ISMACryp, even if no B-frames the DTS is signaled...*/
						if (oti==GPAC_OTI_VIDEO_MPEG4_PART2) force_dts_delta = 22;
						else if ((oti==GPAC_OTI_VIDEO_AVC) || (oti==GPAC_OTI_VIDEO_SVC)) {
							flags &= ~GP_RTP_PCK_USE_MULTI;
							force_dts_delta = 22;
						}
						flags |= GP_RTP_PCK_SIGNAL_RAP | GP_RTP_PCK_SIGNAL_TS;
					}

					required_rate = default_rtp_rate;
				}
				/*systems streams*/
				else if (gf_isom_has_sync_shadows(file, TrackNum) || gf_isom_has_sample_dependency(file, TrackNum)) {
					flags |= GP_RTP_PCK_SYSTEMS_CAROUSEL;
				}
				gf_odf_desc_del((GF_Descriptor*)esd);
			}
			break;
		case GF_ISOM_SUBTYPE_3GP_H263:
			hintType = GF_RTP_PAYT_H263;
			required_rate = 90000;
			streamType = GF_STREAM_VISUAL;
			OfficialPayloadID = 34;
			/*not 100% compliant (short header is missing) but should still work*/
			oti = GPAC_OTI_VIDEO_MPEG4_PART2;
			PL_ID = 0x01;
			break;
		case GF_ISOM_SUBTYPE_3GP_AMR:
			required_rate = 8000;
			hintType = GF_RTP_PAYT_AMR;
			streamType = GF_STREAM_AUDIO;
			has_mpeg4_mapping = 0;
			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_3GP_AMR_WB:
			required_rate = 16000;
			hintType = GF_RTP_PAYT_AMR_WB;
			streamType = GF_STREAM_AUDIO;
			has_mpeg4_mapping = 0;
			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_AVC_H264:
		case GF_ISOM_SUBTYPE_AVC2_H264:
		case GF_ISOM_SUBTYPE_AVC3_H264:
		case GF_ISOM_SUBTYPE_AVC4_H264:
		case GF_ISOM_SUBTYPE_SVC_H264:
		{
			GF_AVCConfig *avcc = gf_isom_avc_config_get(file, TrackNum, 1);
			GF_AVCConfig *svcc = gf_isom_svc_config_get(file, TrackNum, 1);
			required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
			hintType = GF_RTP_PAYT_H264_AVC;
			if (TrackMediaSubType==GF_ISOM_SUBTYPE_SVC_H264)
				hintType = GF_RTP_PAYT_H264_SVC;
			streamType = GF_STREAM_VISUAL;
			avc_nalu_size = avcc ? avcc->nal_unit_size : svcc->nal_unit_size;
			oti = GPAC_OTI_VIDEO_AVC;
			PL_ID = 0x0F;
			gf_odf_avc_cfg_del(avcc);
			gf_odf_avc_cfg_del(svcc);
		}
			break;
		case GF_ISOM_SUBTYPE_HVC1:
		case GF_ISOM_SUBTYPE_HEV1:
		case GF_ISOM_SUBTYPE_HVC2:
		case GF_ISOM_SUBTYPE_HEV2:
		{
			GF_HEVCConfig *hevcc = NULL;
			hevcc = gf_isom_hevc_config_get(file, TrackNum, 1);
			required_rate = 90000;	/* "90 kHz clock rate MUST be used"*/
			hintType = GF_RTP_PAYT_HEVC;			
			streamType = GF_STREAM_VISUAL;
			avc_nalu_size = hevcc->nal_unit_size;
			oti = GPAC_OTI_VIDEO_HEVC;
			PL_ID = 0x0F;
			flags |= GP_RTP_PCK_USE_MULTI;
			gf_odf_hevc_cfg_del(hevcc);
			break;
		}
			break;
		case GF_ISOM_SUBTYPE_3GP_QCELP:
			required_rate = 8000;
			hintType = GF_RTP_PAYT_QCELP;
			streamType = GF_STREAM_AUDIO;
			oti = GPAC_OTI_AUDIO_13K_VOICE;
			OfficialPayloadID = 12;
			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_3GP_EVRC:
		case GF_ISOM_SUBTYPE_3GP_SMV:
			required_rate = 8000;
			hintType = GF_RTP_PAYT_EVRC_SMV;
			streamType = GF_STREAM_AUDIO;
			oti = (TrackMediaSubType==GF_ISOM_SUBTYPE_3GP_EVRC) ? GPAC_OTI_AUDIO_EVRC_VOICE : GPAC_OTI_AUDIO_SMV_VOICE;
			nb_ch = 1;
			break;
		case GF_ISOM_SUBTYPE_3GP_DIMS:
			hintType = GF_RTP_PAYT_3GPP_DIMS;
			streamType = GF_STREAM_SCENE;
			break;
		case GF_ISOM_SUBTYPE_AC3:
			hintType = GF_RTP_PAYT_AC3;
			streamType = GF_STREAM_AUDIO;
			gf_isom_get_audio_info(file, TrackNum, 1, NULL, &nb_ch, NULL);
			break;
		default:
			/*ERROR*/
			hintType = 0;
			break;
		}
	}

	/*not hintable*/
	if (!hintType) return NULL;
	/*we only support self-contained files for hinting*/
	gf_isom_get_data_reference(file, TrackNum, 1, &url, &urn);
	if (url || urn) return NULL;
	
	*e = GF_OUT_OF_MEM;
	GF_SAFEALLOC(tmp, GF_RTPHinter);
	if (!tmp) return NULL;

	/*override hinter type if requested and possible*/
	if (has_mpeg4_mapping && (flags & GP_RTP_PCK_FORCE_MPEG4)) {
		hintType = GF_RTP_PAYT_MPEG4;
		avc_nalu_size = 0;
	}
	/*use static payload ID if enabled*/
	else if (OfficialPayloadID && (flags & GP_RTP_PCK_USE_STATIC_ID) ) {
		PayloadID = OfficialPayloadID;
	}

	tmp->file = file;
	tmp->TrackNum = TrackNum;
	tmp->avc_nalu_size = avc_nalu_size;
	tmp->nb_chan = nb_ch;

	/*spatial scalability check*/
	tmp->has_ctts = gf_isom_has_time_offset(file, TrackNum);

	/*get sample info*/
	gf_media_get_sample_average_infos(file, TrackNum, &MinSize, &MaxSize, &avgTS, &maxDTSDelta, &const_dur, &bandwidth);

	/*systems carousel: we need at least IDX and RAP signaling*/
	if (flags & GP_RTP_PCK_SYSTEMS_CAROUSEL) {
		flags |= GP_RTP_PCK_SIGNAL_RAP;
	}

	/*update flags in MultiSL*/
	if (flags & GP_RTP_PCK_USE_MULTI) {
		if (MinSize != MaxSize) flags |= GP_RTP_PCK_SIGNAL_SIZE;
		if (!const_dur) flags |= GP_RTP_PCK_SIGNAL_TS;
	}
	if (tmp->has_ctts) flags |= GP_RTP_PCK_SIGNAL_TS;

	/*default SL for RTP */
	InitSL_RTP(&my_sl);

	my_sl.timestampResolution = gf_isom_get_media_timescale(file, TrackNum);
	/*override clockrate if set*/
	if (required_rate) {
		Double sc = required_rate;
		sc /= my_sl.timestampResolution;
		maxDTSDelta = (u32) (maxDTSDelta*sc);
		my_sl.timestampResolution = required_rate;
	}
	/*switch to RTP TS*/
	max_ptime = (u32) (max_ptime * my_sl.timestampResolution / 1000);

	my_sl.AUSeqNumLength = gf_get_bit_size(gf_isom_get_sample_count(file, TrackNum));
	my_sl.CUDuration = const_dur;

	if (gf_isom_has_sync_points(file, TrackNum)) {
		my_sl.useRandomAccessPointFlag = 1;
	} else {
		my_sl.useRandomAccessPointFlag = 0;
		my_sl.hasRandomAccessUnitsOnlyFlag = 1;
	}

	if (is_crypted) {
		Bool use_sel_enc;
		gf_isom_get_ismacryp_info(file, TrackNum, 1, NULL, NULL, NULL, NULL, NULL, &use_sel_enc, &IV_length, &KI_length);
		if (use_sel_enc) flags |= GP_RTP_PCK_SELECTIVE_ENCRYPTION;
	}

	// in case a different timescale was provided
	tmp->OrigTimeScale = gf_isom_get_media_timescale(file, TrackNum);
	tmp->rtp_p = gf_rtp_builder_new(hintType, &my_sl, flags, tmp, 
								MP4T_OnNewPacket, MP4T_OnPacketDone, 
								/*if copy, no data ref*/
								copy_media ? NULL : MP4T_OnDataRef, 
								MP4T_OnData);

	//init the builder
	gf_rtp_builder_init(tmp->rtp_p, PayloadID, Path_MTU, max_ptime,
					   streamType, oti, PL_ID, MinSize, MaxSize, avgTS, maxDTSDelta, IV_length, KI_length, mpeg4mode);

	/*ISMA compliance is a pain...*/
	if (force_dts_delta) tmp->rtp_p->slMap.DTSDeltaLength = force_dts_delta;


	/*		Hint Track Setup	*/
	tmp->TrackID = gf_isom_get_track_id(file, TrackNum);
	tmp->HintID = tmp->TrackID + 65535;
	while (gf_isom_get_track_by_id(file, tmp->HintID)) tmp->HintID++;

	tmp->HintTrack = gf_isom_new_track(file, tmp->HintID, GF_ISOM_MEDIA_HINT, my_sl.timestampResolution);
	gf_isom_setup_hint_track(file, tmp->HintTrack, GF_ISOM_HINT_RTP);
	/*create a hint description*/
	gf_isom_new_hint_description(file, tmp->HintTrack, -1, -1, 0, &descIndex);
	gf_isom_rtp_set_timescale(file, tmp->HintTrack, descIndex, my_sl.timestampResolution);

	if (hintType==GF_RTP_PAYT_MPEG4) {
		tmp->rtp_p->slMap.ObjectTypeIndication = oti;
		/*set this SL for extraction.*/
		gf_isom_set_extraction_slc(file, TrackNum, 1, &my_sl);
	}
	tmp->bandwidth = bandwidth;

	/*set interleaving*/
	gf_isom_set_track_group(file, TrackNum, InterleaveGroupID);
	if (!copy_media) {
		/*if we don't copy data set hint track and media track in the same group*/
		gf_isom_set_track_group(file, tmp->HintTrack, InterleaveGroupID);
	} else {
		gf_isom_set_track_group(file, tmp->HintTrack, InterleaveGroupID + OFFSET_HINT_GROUP_ID);
	}
	/*use user-secified priority*/
	InterleaveGroupPriority*=2;
	gf_isom_set_track_priority_in_group(file, TrackNum, InterleaveGroupPriority+1);
	gf_isom_set_track_priority_in_group(file, tmp->HintTrack, InterleaveGroupPriority);

#if 0
	/*QT FF: not setting these flags = server uses a random offset*/
	gf_isom_rtp_set_time_offset(file, tmp->HintTrack, 1, 0);
	/*we don't use seq offset for maintainance pruposes*/
	gf_isom_rtp_set_time_sequence_offset(file, tmp->HintTrack, 1, 0);
#endif
	*e = GF_OK;
	return tmp;
}

GF_EXPORT
u32 gf_hinter_track_get_bandwidth(GF_RTPHinter *tkHinter)
{
	return tkHinter->bandwidth;
}

GF_EXPORT
u32 gf_hinter_track_get_flags(GF_RTPHinter *tkHinter)
{
	return tkHinter->rtp_p->flags;
}
GF_EXPORT
void gf_hinter_track_get_payload_name(GF_RTPHinter *tkHinter, char *payloadName)
{
	char mediaName[30];
	gf_rtp_builder_get_payload_name(tkHinter->rtp_p, payloadName, mediaName);
}

GF_EXPORT
void gf_hinter_track_del(GF_RTPHinter *tkHinter)
{
	if (!tkHinter) return;

	if (tkHinter->rtp_p) gf_rtp_builder_del(tkHinter->rtp_p);
	gf_free(tkHinter);
}

GF_EXPORT
GF_Err gf_hinter_track_process(GF_RTPHinter *tkHint)
{
	GF_Err e;
	u32 i, descIndex, duration;
	u64 ts;
	u8 PadBits;
	Double ft;
	GF_ISOSample *samp;

	tkHint->HintSample = tkHint->RTPTime = 0;

	tkHint->TotalSample = gf_isom_get_sample_count(tkHint->file, tkHint->TrackNum);
	ft = tkHint->rtp_p->sl_config.timestampResolution;
	ft /= tkHint->OrigTimeScale;
	
	e = GF_OK;
	for (i=0; i<tkHint->TotalSample; i++) {
		samp = gf_isom_get_sample(tkHint->file, tkHint->TrackNum, i+1, &descIndex);
		if (!samp) return GF_IO_ERR;

		//setup SL
		tkHint->CurrentSample = i + 1;

		/*keep same AU indicator if sync shadow - TODO FIXME: this assumes shadows are placed interleaved with 
		the track content which is the case for GPAC scene carousel generation, but may not always be true*/
		if (samp->IsRAP==2) {
			tkHint->rtp_p->sl_header.AU_sequenceNumber -= 1;
			samp->IsRAP = 1;
		}

		ts = (u64) (ft * (s64) (samp->DTS+samp->CTS_Offset));
		tkHint->rtp_p->sl_header.compositionTimeStamp = ts;

		ts = (u64) (ft * (s64)(samp->DTS));
		tkHint->rtp_p->sl_header.decodingTimeStamp = ts;
		tkHint->rtp_p->sl_header.randomAccessPointFlag = samp->IsRAP;

		tkHint->base_offset_in_sample = 0;
		/*crypted*/
		if (tkHint->rtp_p->slMap.IV_length) {
			GF_ISMASample *s = gf_isom_get_ismacryp_sample(tkHint->file, tkHint->TrackNum, samp, descIndex);
			/*one byte take for selective_enc flag*/
			if (s->flags & GF_ISOM_ISMA_USE_SEL_ENC) tkHint->base_offset_in_sample += 1;
			if (s->flags & GF_ISOM_ISMA_IS_ENCRYPTED) tkHint->base_offset_in_sample += s->IV_length + s->KI_length;
			gf_free(samp->data);
			samp->data = s->data;
			samp->dataLength = s->dataLength;
			gp_rtp_builder_set_cryp_info(tkHint->rtp_p, s->IV, (char*)s->key_indicator, (s->flags & GF_ISOM_ISMA_IS_ENCRYPTED) ? 1 : 0);
			s->data = NULL;
			s->dataLength = 0;
			gf_isom_ismacryp_delete_sample(s);
		}

		if (tkHint->rtp_p->sl_config.usePaddingFlag) {
			gf_isom_get_sample_padding_bits(tkHint->file, tkHint->TrackNum, i+1, &PadBits);
			tkHint->rtp_p->sl_header.paddingBits = PadBits;
		} else {
			tkHint->rtp_p->sl_header.paddingBits = 0;
		}
		
		duration = gf_isom_get_sample_duration(tkHint->file, tkHint->TrackNum, i+1);
		ts = (u32) (ft * (s64) (duration));

		/*unpack nal units*/
		if (tkHint->avc_nalu_size) {
			u32 v, size;
			u32 remain = samp->dataLength;
			char *ptr = samp->data;

			tkHint->rtp_p->sl_header.accessUnitStartFlag = 1;
			tkHint->rtp_p->sl_header.accessUnitEndFlag = 0;
			while (remain) {
				size = 0;
				v = tkHint->avc_nalu_size;
				while (v) {
					size |= (u8) *ptr;
					ptr++;
					remain--;
					v-=1;
					if (v) size<<=8;
				}
				tkHint->base_offset_in_sample = samp->dataLength-remain;
				if (remain < size) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[rtp hinter] Broken AVC nalu encapsulation: NALU size is %d but only %d bytes left in sample %d\n", size, remain, tkHint->CurrentSample));
					break;
				}
				remain -= size;
				tkHint->rtp_p->sl_header.accessUnitEndFlag = remain ? 0 : 1;
				e = gf_rtp_builder_process(tkHint->rtp_p, ptr, size, (u8) !remain, samp->dataLength, duration, (u8) (descIndex + GF_RTP_TX3G_SIDX_OFFSET) );
				ptr += size;
				tkHint->rtp_p->sl_header.accessUnitStartFlag = 0;
			}
		} else {
			e = gf_rtp_builder_process(tkHint->rtp_p, samp->data, samp->dataLength, 1, samp->dataLength, duration, (u8) (descIndex + GF_RTP_TX3G_SIDX_OFFSET) );
		}
		tkHint->rtp_p->sl_header.packetSequenceNumber += 1;

		//signal some progress
		gf_set_progress("Hinting", tkHint->CurrentSample, tkHint->TotalSample);

		tkHint->rtp_p->sl_header.AU_sequenceNumber += 1;
		gf_isom_sample_del(&samp);

		if (e) return e;
	}

	//flush
	gf_rtp_builder_process(tkHint->rtp_p, NULL, 0, 1, 0, 0, 0);

	gf_isom_end_hint_sample(tkHint->file, tkHint->HintTrack, (u8) tkHint->SampleIsRAP);
	return GF_OK;
}

static u32 write_nalu_config_array(char *sdpLine, GF_List *nalus)
{
	u32 i, count, b64s;
	char b64[200];

	count = gf_list_count(nalus);
	for (i=0; i<count; i++) {
		GF_AVCConfigSlot *sl = (GF_AVCConfigSlot *)gf_list_get(nalus, i);
		b64s = gf_base64_encode(sl->data, sl->size, b64, 200);
		b64[b64s]=0;
		strcat(sdpLine, b64);
		if (i+1<count) strcat(sdpLine, ",");
	}
	return count;
}

static void write_avc_config(char *sdpLine, GF_AVCConfig *avcc, GF_AVCConfig *svcc)
{
	u32 count = 0;

	if (avcc) count += gf_list_count(avcc->sequenceParameterSets) + gf_list_count(avcc->pictureParameterSets) + gf_list_count(avcc->sequenceParameterSetExtensions);
	if (svcc) count += gf_list_count(svcc->sequenceParameterSets) + gf_list_count(svcc->pictureParameterSets);
	if (!count) return;

	strcat(sdpLine, "; sprop-parameter-sets=");

	if (avcc) {
		count = write_nalu_config_array(sdpLine, avcc->sequenceParameterSets);
		if (count) strcat(sdpLine, ",");
		count = write_nalu_config_array(sdpLine, avcc->sequenceParameterSetExtensions);
		if (count) strcat(sdpLine, ",");
		count = write_nalu_config_array(sdpLine, avcc->pictureParameterSets);
		if (count) strcat(sdpLine, ",");
	}
		
	if (svcc) {
		count = write_nalu_config_array(sdpLine, svcc->sequenceParameterSets);
		if (count) strcat(sdpLine, ",");
		count = write_nalu_config_array(sdpLine, svcc->pictureParameterSets);
		if (count) strcat(sdpLine, ",");
	}
	count = (u32) strlen(sdpLine);
	if (sdpLine[count-1] == ',')
		sdpLine[count-1] = 0;
}

GF_EXPORT
GF_Err gf_hinter_track_finalize(GF_RTPHinter *tkHint, Bool AddSystemInfo)
{
	u32 Width, Height;
	GF_ESD *esd;
	char sdpLine[20000];
	char mediaName[30], payloadName[30];

	Width = Height = 0;
	gf_isom_sdp_clean_track(tkHint->file, tkHint->TrackNum);
	if (gf_isom_get_media_type(tkHint->file, tkHint->TrackNum) == GF_ISOM_MEDIA_VISUAL)
		gf_isom_get_visual_info(tkHint->file, tkHint->TrackNum, 1, &Width, &Height);

	gf_rtp_builder_get_payload_name(tkHint->rtp_p, payloadName, mediaName);

	/*TODO- extract out of rtp_p for future live tools*/
	sprintf(sdpLine, "m=%s 0 RTP/%s %d", mediaName, tkHint->rtp_p->slMap.IV_length ? "SAVP" : "AVP", tkHint->rtp_p->PayloadType);
	gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	if (tkHint->bandwidth) {
		sprintf(sdpLine, "b=AS:%d", tkHint->bandwidth);
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}
	if (tkHint->nb_chan) {
		sprintf(sdpLine, "a=rtpmap:%d %s/%d/%d", tkHint->rtp_p->PayloadType, payloadName, tkHint->rtp_p->sl_config.timestampResolution, tkHint->nb_chan);
	} else {
		sprintf(sdpLine, "a=rtpmap:%d %s/%d", tkHint->rtp_p->PayloadType, payloadName, tkHint->rtp_p->sl_config.timestampResolution);
	}
	gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	/*control for MPEG-4*/
	if (AddSystemInfo) {
		sprintf(sdpLine, "a=mpeg4-esid:%d", gf_isom_get_track_id(tkHint->file, tkHint->TrackNum));
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}
	/*control for QTSS/DSS*/
	sprintf(sdpLine, "a=control:trackID=%d", gf_isom_get_track_id(tkHint->file, tkHint->HintTrack));
	gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);

	/*H263 extensions*/
	if (tkHint->rtp_p->rtp_payt == GF_RTP_PAYT_H263) {
		sprintf(sdpLine, "a=cliprect:0,0,%d,%d", Height, Width);
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}
	/*AMR*/
	else if ((tkHint->rtp_p->rtp_payt == GF_RTP_PAYT_AMR) || (tkHint->rtp_p->rtp_payt == GF_RTP_PAYT_AMR_WB)) {
		sprintf(sdpLine, "a=fmtp:%d octet-align=1", tkHint->rtp_p->PayloadType);
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}
	/*Text*/
	else if (tkHint->rtp_p->rtp_payt == GF_RTP_PAYT_3GPP_TEXT) {
		gf_media_format_ttxt_sdp(tkHint->rtp_p, payloadName, sdpLine, tkHint->file, tkHint->TrackNum);
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}
	/*EVRC/SMV in non header-free mode*/
	else if ((tkHint->rtp_p->rtp_payt == GF_RTP_PAYT_EVRC_SMV) && (tkHint->rtp_p->auh_size>1)) {
		sprintf(sdpLine, "a=fmtp:%d maxptime=%d", tkHint->rtp_p->PayloadType, tkHint->rtp_p->auh_size*20);
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}
	/*H264/AVC*/
	else if ((tkHint->rtp_p->rtp_payt == GF_RTP_PAYT_H264_AVC) || (tkHint->rtp_p->rtp_payt == GF_RTP_PAYT_H264_SVC))  {
		GF_AVCConfig *avcc = gf_isom_avc_config_get(tkHint->file, tkHint->TrackNum, 1);
		GF_AVCConfig *svcc = gf_isom_svc_config_get(tkHint->file, tkHint->TrackNum, 1);
		/*TODO - check syntax for SVC (might be some extra signaling)*/

		if (avcc) {
			sprintf(sdpLine, "a=fmtp:%d profile-level-id=%02X%02X%02X; packetization-mode=1", tkHint->rtp_p->PayloadType, avcc->AVCProfileIndication, avcc->profile_compatibility, avcc->AVCLevelIndication);
		} else {
			sprintf(sdpLine, "a=fmtp:%d profile-level-id=%02X%02X%02X; packetization-mode=1", tkHint->rtp_p->PayloadType, svcc->AVCProfileIndication, svcc->profile_compatibility, svcc->AVCLevelIndication);
		}

		write_avc_config(sdpLine, avcc, svcc);

		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
		gf_odf_avc_cfg_del(avcc);
		gf_odf_avc_cfg_del(svcc);
	}
	/*MPEG-4 decoder config*/
	else if (tkHint->rtp_p->rtp_payt==GF_RTP_PAYT_MPEG4) {
		esd = gf_isom_get_esd(tkHint->file, tkHint->TrackNum, 1);

		if (esd && esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo && esd->decoderConfig->decoderSpecificInfo->data) {
			gf_rtp_builder_format_sdp(tkHint->rtp_p, payloadName, sdpLine, esd->decoderConfig->decoderSpecificInfo->data, esd->decoderConfig->decoderSpecificInfo->dataLength);
		} else {
			gf_rtp_builder_format_sdp(tkHint->rtp_p, payloadName, sdpLine, NULL, 0);
		}
		if (esd) gf_odf_desc_del((GF_Descriptor *)esd);

		if (tkHint->rtp_p->slMap.IV_length) {
			const char *kms;
			gf_isom_get_ismacryp_info(tkHint->file, tkHint->TrackNum, 1, NULL, NULL, NULL, NULL, &kms, NULL, NULL, NULL);
			if (!strnicmp(kms, "(key)", 5) || !strnicmp(kms, "(ipmp)", 6) || !strnicmp(kms, "(uri)", 5)) {
				strcat(sdpLine, "; ISMACrypKey=");
			} else {
				strcat(sdpLine, "; ISMACrypKey=(uri)");
			}
			strcat(sdpLine, kms);
		}

		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}
	/*MPEG-4 Audio LATM*/
	else if (tkHint->rtp_p->rtp_payt==GF_RTP_PAYT_LATM) { 
		GF_BitStream *bs; 
		char *config_bytes; 
		u32 config_size; 
 
		/* form config string */ 
		bs = gf_bs_new(NULL, 32, GF_BITSTREAM_WRITE); 
		gf_bs_write_int(bs, 0, 1); /* AudioMuxVersion */ 
		gf_bs_write_int(bs, 1, 1); /* all streams same time */ 
		gf_bs_write_int(bs, 0, 6); /* numSubFrames */ 
		gf_bs_write_int(bs, 0, 4); /* numPrograms */ 
		gf_bs_write_int(bs, 0, 3); /* numLayer */ 
 
		/* audio-specific config */ 
		esd = gf_isom_get_esd(tkHint->file, tkHint->TrackNum, 1); 
		if (esd && esd->decoderConfig && esd->decoderConfig->decoderSpecificInfo) { 
			/*PacketVideo patch: don't signal SBR and PS stuff, not allowed in LATM with audioMuxVersion=0*/
			gf_bs_write_data(bs, esd->decoderConfig->decoderSpecificInfo->data, MIN(esd->decoderConfig->decoderSpecificInfo->dataLength, 2) ); 
		} 
		if (esd) gf_odf_desc_del((GF_Descriptor *)esd); 
 
		/* other data */ 
		gf_bs_write_int(bs, 0, 3); /* frameLengthType */ 
		gf_bs_write_int(bs, 0xff, 8); /* latmBufferFullness */ 
		gf_bs_write_int(bs, 0, 1); /* otherDataPresent */ 
		gf_bs_write_int(bs, 0, 1); /* crcCheckPresent */ 
		gf_bs_get_content(bs, &config_bytes, &config_size); 
		gf_bs_del(bs); 
 
		gf_rtp_builder_format_sdp(tkHint->rtp_p, payloadName, sdpLine, config_bytes, config_size); 
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine); 
		gf_free(config_bytes); 
	}
	/*3GPP DIMS*/
	else if (tkHint->rtp_p->rtp_payt==GF_RTP_PAYT_3GPP_DIMS) { 
		GF_DIMSDescription dims;
		char fmt[200];
		gf_isom_get_visual_info(tkHint->file, tkHint->TrackNum, 1, &Width, &Height);

		gf_isom_get_dims_description(tkHint->file, tkHint->TrackNum, 1, &dims);
		sprintf(sdpLine, "a=fmtp:%d Version-profile=%d", tkHint->rtp_p->PayloadType, dims.profile);
		if (! dims.fullRequestHost) {
			strcat(sdpLine, ";useFullRequestHost=0");
			sprintf(fmt, ";pathComponents=%d", dims.pathComponents);
			strcat(sdpLine, fmt);
		}
		if (!dims.streamType) strcat(sdpLine, ";stream-type=secondary");
		if (dims.containsRedundant == 1) strcat(sdpLine, ";contains-redundant=main");
		else if (dims.containsRedundant == 2) strcat(sdpLine, ";contains-redundant=redundant");

		if (dims.textEncoding && strlen(dims.textEncoding)) {
			strcat(sdpLine, ";text-encoding=");
			strcat(sdpLine, dims.textEncoding);
		}
		if (dims.contentEncoding && strlen(dims.contentEncoding)) {
			strcat(sdpLine, ";content-coding=");
			strcat(sdpLine, dims.contentEncoding);
		}
		if (dims.content_script_types && strlen(dims.content_script_types) ) {
			strcat(sdpLine, ";content-script-types=");
			strcat(sdpLine, dims.contentEncoding);
		}
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}
	/*extensions for some mobile phones*/
	if (Width && Height) {
		sprintf(sdpLine, "a=framesize:%d %d-%d", tkHint->rtp_p->PayloadType, Width, Height);
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}

	esd = gf_isom_get_esd(tkHint->file, tkHint->TrackNum, 1);
	if (esd && esd->decoderConfig && (esd->decoderConfig->rvc_config || esd->decoderConfig->predefined_rvc_config)) {
		if (esd->decoderConfig->predefined_rvc_config) {
			sprintf(sdpLine, "a=rvc-config-predef:%d", esd->decoderConfig->predefined_rvc_config);
		} else {
			/*temporary ...*/
			if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_SVC)) {
				sprintf(sdpLine, "a=rvc-config:%s", "http://download.tsi.telecom-paristech.fr/gpac/RVC/rvc_config_avc.xml");
			} else {
				sprintf(sdpLine, "a=rvc-config:%s", "http://download.tsi.telecom-paristech.fr/gpac/RVC/rvc_config_sp.xml");
			}
		}
		gf_isom_sdp_add_track_line(tkHint->file, tkHint->HintTrack, sdpLine);
	}
	if (esd) gf_odf_desc_del((GF_Descriptor *)esd);

	gf_isom_set_track_enabled(tkHint->file, tkHint->HintTrack, 1);
	return GF_OK;
}

GF_EXPORT
Bool gf_hinter_can_embbed_data(char *data, u32 data_size, u32 streamType)
{
	char data64[5000];
	u32 size64;

	size64 = gf_base64_encode(data, data_size, data64, 5000);
	if (!size64) return 0;
	switch (streamType) {
	case GF_STREAM_OD:
		size64 += (u32) strlen("data:application/mpeg4-od-au;base64,");
		break;
	case GF_STREAM_SCENE:
		size64 += (u32) strlen("data:application/mpeg4-bifs-au;base64,");
		break;
	default:
		/*NOT NORMATIVE*/
		size64 += (u32) strlen("data:application/mpeg4-es-au;base64,");
		break;
	}
	if (size64>=255) return 0;
	return 1;
}


GF_EXPORT
GF_Err gf_hinter_finalize(GF_ISOFile *file, u32 IOD_Profile, u32 bandwidth)
{
	u32 i, sceneT, odT, descIndex, size, size64;
	GF_InitialObjectDescriptor *iod;
	GF_SLConfig slc;
	GF_ESD *esd;
	GF_ISOSample *samp;
	Bool remove_ocr;
	char *buffer;
	char buf64[5000], sdpLine[2300];


	gf_isom_sdp_clean(file);

	if (bandwidth) {
		sprintf(buf64, "b=AS:%d", bandwidth);
		gf_isom_sdp_add_line(file, buf64);
	}
	//xtended attribute for copyright
	sprintf(buf64, "a=x-copyright: %s", "MP4/3GP File hinted with GPAC " GPAC_FULL_VERSION " (C)2000-2005 - http://gpac.sourceforge.net");
	gf_isom_sdp_add_line(file, buf64);

	if (IOD_Profile == GF_SDP_IOD_NONE) return GF_OK;

	odT = sceneT = 0;
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (!gf_isom_is_track_in_root_od(file, i+1)) continue;
		switch (gf_isom_get_media_type(file,i+1)) {
		case GF_ISOM_MEDIA_OD:
			odT = i+1;
			break;
		case GF_ISOM_MEDIA_SCENE:
			sceneT = i+1;
			break;
		}
	}
	remove_ocr = 0;
	if (IOD_Profile == GF_SDP_IOD_ISMA_STRICT) {
		IOD_Profile = GF_SDP_IOD_ISMA;
		remove_ocr = 1;
	}

	/*if we want ISMA like iods, we need at least BIFS */
	if ( (IOD_Profile == GF_SDP_IOD_ISMA) && !sceneT ) return GF_BAD_PARAM;

	/*do NOT change PLs, we assume they are correct*/
	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(file);
	if (!iod) return GF_NOT_SUPPORTED;

	/*rewrite an IOD with good SL config - embbed data if possible*/
	if (IOD_Profile == GF_SDP_IOD_ISMA) {
		Bool is_ok = 1;
		while (gf_list_count(iod->ESDescriptors)) {
			esd = (GF_ESD*)gf_list_get(iod->ESDescriptors, 0);
			gf_odf_desc_del((GF_Descriptor *) esd);
			gf_list_rem(iod->ESDescriptors, 0);
		}


		/*get OD esd, and embbed stream data if possible*/
		if (odT) {
			esd = gf_isom_get_esd(file, odT, 1);
			if (gf_isom_get_sample_count(file, odT)==1) {
				samp = gf_isom_get_sample(file, odT, 1, &descIndex);
				if (gf_hinter_can_embbed_data(samp->data, samp->dataLength, GF_STREAM_OD)) {
					InitSL_NULL(&slc);
					slc.predefined = 0;
					slc.hasRandomAccessUnitsOnlyFlag = 1;
					slc.timeScale = slc.timestampResolution = gf_isom_get_media_timescale(file, odT);	
					slc.OCRResolution = 1000;
					slc.startCTS = samp->DTS+samp->CTS_Offset;
					slc.startDTS = samp->DTS;
					//set the SL for future extraction
					gf_isom_set_extraction_slc(file, odT, 1, &slc);

					size64 = gf_base64_encode(samp->data, samp->dataLength, buf64, 2000);
					buf64[size64] = 0;
					sprintf(sdpLine, "data:application/mpeg4-od-au;base64,%s", buf64);

					esd->decoderConfig->avgBitrate = 0;
					esd->decoderConfig->bufferSizeDB = samp->dataLength;
					esd->decoderConfig->maxBitrate = 0;
					size64 = (u32) strlen(sdpLine)+1;
					esd->URLString = (char*)gf_malloc(sizeof(char) * size64);
					strcpy(esd->URLString, sdpLine);
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[rtp hinter] OD sample too large to be embedded in IOD - ISMA disabled\n"));
					is_ok = 0;
				}
				gf_isom_sample_del(&samp);
			}
			if (remove_ocr) esd->OCRESID = 0;
			else if (esd->OCRESID == esd->ESID) esd->OCRESID = 0;
			
			//OK, add this to our IOD
			gf_list_add(iod->ESDescriptors, esd);
		}

		esd = gf_isom_get_esd(file, sceneT, 1);
		if (gf_isom_get_sample_count(file, sceneT)==1) {
			samp = gf_isom_get_sample(file, sceneT, 1, &descIndex);
			if (gf_hinter_can_embbed_data(samp->data, samp->dataLength, GF_STREAM_SCENE)) {

				slc.timeScale = slc.timestampResolution = gf_isom_get_media_timescale(file, sceneT);	
				slc.OCRResolution = 1000;
				slc.startCTS = samp->DTS+samp->CTS_Offset;
				slc.startDTS = samp->DTS;
				//set the SL for future extraction
				gf_isom_set_extraction_slc(file, sceneT, 1, &slc);
				//encode in Base64 the sample
				size64 = gf_base64_encode(samp->data, samp->dataLength, buf64, 2000);
				buf64[size64] = 0;
				sprintf(sdpLine, "data:application/mpeg4-bifs-au;base64,%s", buf64);

				esd->decoderConfig->avgBitrate = 0;
				esd->decoderConfig->bufferSizeDB = samp->dataLength;
				esd->decoderConfig->maxBitrate = 0;
				esd->URLString = (char*)gf_malloc(sizeof(char) * (strlen(sdpLine)+1));
				strcpy(esd->URLString, sdpLine);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[rtp hinter] Scene description sample too large to be embedded in IOD - ISMA disabled\n"));
				is_ok = 0;
			}
			gf_isom_sample_del(&samp);
		}
		if (remove_ocr) esd->OCRESID = 0;
		else if (esd->OCRESID == esd->ESID) esd->OCRESID = 0;

		gf_list_add(iod->ESDescriptors, esd);

		if (is_ok) {
			u32 has_a, has_v, has_i_a, has_i_v;
			has_a = has_v = has_i_a = has_i_v = 0;
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				esd = gf_isom_get_esd(file, i+1, 1);
				if (!esd) continue;
				if (esd->decoderConfig->streamType==GF_STREAM_VISUAL) {
					if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_MPEG4_PART2) has_i_v ++;
					else has_v++;
				} else if (esd->decoderConfig->streamType==GF_STREAM_AUDIO) {
					if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_AUDIO_AAC_MPEG4) has_i_a ++;
					else has_a++;
				}
				gf_odf_desc_del((GF_Descriptor *)esd);
			}
			/*only 1 MPEG-4 visual max and 1 MPEG-4 audio max for ISMA compliancy*/
			if (!has_v && !has_a && (has_i_v<=1) && (has_i_a<=1)) {
				sprintf(sdpLine, "a=isma-compliance:1,1.0,1");
				gf_isom_sdp_add_line(file, sdpLine);
			}
		}
	}

	//encode the IOD
	buffer = NULL;
	size = 0;
	gf_odf_desc_write((GF_Descriptor *) iod, &buffer, &size);
	gf_odf_desc_del((GF_Descriptor *)iod);

	//encode in Base64 the iod
	size64 = gf_base64_encode(buffer, size, buf64, 2000);
	buf64[size64] = 0;
	gf_free(buffer);

	sprintf(sdpLine, "a=mpeg4-iod:\"data:application/mpeg4-iod;base64,%s\"", buf64);
	gf_isom_sdp_add_line(file, sdpLine);

	return GF_OK;
}


#endif /*GPAC_DISABLE_ISOM_HINTING*/

#endif /*GPAC_DISABLE_ISOM*/
