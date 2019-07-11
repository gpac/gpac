/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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

#include <gpac/internal/isomedia_dev.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_ISOM

GF_TrackBox *GetTrackbyID(GF_MovieBox *moov, GF_ISOTrackID TrackID)
{
	GF_TrackBox *trak;
	u32 i;
	if (!moov) return NULL;
	i=0;
	while ((trak = (GF_TrackBox *)gf_list_enum(moov->trackList, &i))) {
		if (trak->Header->trackID == TrackID) return trak;
	}
	return NULL;
}

GF_TrackBox *gf_isom_get_track(GF_MovieBox *moov, u32 trackNumber)
{
	GF_TrackBox *trak;
	if (!moov || !trackNumber || (trackNumber > gf_list_count(moov->trackList))) return NULL;
	trak = (GF_TrackBox*)gf_list_get(moov->trackList, trackNumber - 1);
	return trak;

}

//get the number of a track given its ID
//return 0 if not found error
u32 gf_isom_get_tracknum_from_id(GF_MovieBox *moov, GF_ISOTrackID trackID)
{
	u32 i;
	GF_TrackBox *trak;
	i=0;
	while ((trak = (GF_TrackBox *)gf_list_enum(moov->trackList, &i))) {
		if (trak->Header->trackID == trackID) return i;
	}
	return 0;
}

//extraction of the ESD from the track
GF_Err GetESD(GF_MovieBox *moov, GF_ISOTrackID trackID, u32 StreamDescIndex, GF_ESD **outESD)
{
	GF_Err e;
	GF_ESD *esd;
	u32 track_num = 0;
	u32 k;
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak, *OCRTrack;
	GF_TrackReferenceTypeBox *dpnd;
	GF_SLConfig *slc;
	GF_MPEGSampleEntryBox *entry;

	if (!moov) return GF_ISOM_INVALID_FILE;

	track_num = gf_isom_get_tracknum_from_id(moov, trackID);
	dpnd = NULL;
	*outESD = NULL;

	trak = gf_isom_get_track(moov, track_num);
	if (!trak) return GF_ISOM_INVALID_FILE;

	e = Media_GetESD(trak->Media, StreamDescIndex, &esd, 0);
	if (e) return e;

	e = Media_GetSampleDesc(trak->Media, StreamDescIndex, (GF_SampleEntryBox **) &entry, NULL);
	if (e) return e;
	//set the ID
	esd->ESID = trackID;

	//find stream dependencies: dpnd, sbas and scal
	for (k=0; k<3; k++) {
		u32 ref = GF_ISOM_BOX_TYPE_DPND;
		if (k==1) ref = GF_ISOM_REF_BASE;
		else if (k==2) ref = GF_ISOM_REF_SCAL;

		e = Track_FindRef(trak, ref , &dpnd);
		if (e) return e;
		if (dpnd) {
			//ONLY ONE STREAM DEPENDENCY IS ALLOWED
			if (!k && (dpnd->trackIDCount != 1)) return GF_ISOM_INVALID_MEDIA;
			//fix the spec: where is the index located ??
			esd->dependsOnESID = dpnd->trackIDs[0];
			break;
		} else {
			esd->dependsOnESID = 0;
		}
	}

	if (trak->udta) {
		GF_UserDataMap *map;
		u32 i = 0;
		while ((map = (GF_UserDataMap*)gf_list_enum(trak->udta->recordList, &i))) {
			if (map->boxType == GF_ISOM_BOX_TYPE_AUXV) {
				GF_Descriptor *d = gf_odf_desc_new(GF_ODF_AUX_VIDEO_DATA);
				gf_list_add(esd->extensionDescriptors, d);
				break;
			}
		}
	}

	//OK, get the OCR (in a REAL MP4File, OCR is 0 in ESD and is specified through track reference
	dpnd = NULL;
	OCRTrack = NULL;
	//find OCR dependencies
	e = Track_FindRef(trak, GF_ISOM_BOX_TYPE_SYNC, &dpnd);
	if (e) return e;
	if (dpnd) {
		if (dpnd->trackIDCount != 1) return GF_ISOM_INVALID_MEDIA;
		esd->OCRESID = dpnd->trackIDs[0];
		OCRTrack = gf_isom_get_track_from_id(trak->moov, dpnd->trackIDs[0]);

		while (OCRTrack) {
			/*if we have a dependency on a track that doesn't have OCR dep, remove that dependency*/
			e = Track_FindRef(OCRTrack, GF_ISOM_BOX_TYPE_SYNC, &dpnd);
			if (e || !dpnd || !dpnd->trackIDCount) {
				OCRTrack = NULL;
				goto default_sync;
			}
			/*this is explicit desync*/
			if (dpnd && ((dpnd->trackIDs[0]==0) || (dpnd->trackIDs[0]==OCRTrack->Header->trackID))) break;
			/*loop in OCRs, break it*/
			if (esd->ESID == (u16) OCRTrack->Header->trackID) {
				OCRTrack = NULL;
				goto default_sync;
			}
			/*check next*/
			OCRTrack = gf_isom_get_track_from_id(trak->moov, dpnd->trackIDs[0]);
		}
		if (!OCRTrack) goto default_sync;
	} else {
default_sync:
		/*all tracks are sync'ed by default*/
		if (trak->moov->mov->es_id_default_sync<0) {
			if (esd->OCRESID)
				trak->moov->mov->es_id_default_sync = esd->OCRESID;
			else
				trak->moov->mov->es_id_default_sync = esd->ESID;
		}
		if (trak->moov->mov->es_id_default_sync) esd->OCRESID = (u16) trak->moov->mov->es_id_default_sync;
		/*cf ESD writer*/
		if (esd->OCRESID == esd->ESID) esd->OCRESID = 0;
	}



	//update the IPI stuff if needed
	if (esd->ipiPtr != NULL) {
		dpnd = NULL;
		e = Track_FindRef(trak, GF_ISOM_BOX_TYPE_IPIR, &dpnd);
		if (e) return e;
		if (dpnd) {
			if (esd->ipiPtr->tag != GF_ODF_ISOM_IPI_PTR_TAG) return GF_ISOM_INVALID_FILE;
			//OK, retrieve the ID: the IPI_ES_Id is currently the ref track
			esd->ipiPtr->IPI_ES_Id = dpnd->trackIDs[esd->ipiPtr->IPI_ES_Id - 1];
			//and change the tag
			esd->ipiPtr->tag = GF_ODF_IPI_PTR_TAG;
		} else {
			return GF_ISOM_INVALID_FILE;
		}
	}

	if ((trak->Media->mediaHeader->packedLanguage[0] != 'u')
	        || (trak->Media->mediaHeader->packedLanguage[1] != 'n')
	        || (trak->Media->mediaHeader->packedLanguage[2] != 'd') ) {
		if (!esd->langDesc) esd->langDesc = (GF_Language *) gf_odf_desc_new(GF_ODF_LANG_TAG);

		esd->langDesc->langCode = trak->Media->mediaHeader->packedLanguage[0];
		esd->langDesc->langCode <<= 8;
		esd->langDesc->langCode |= trak->Media->mediaHeader->packedLanguage[1];
		esd->langDesc->langCode <<= 8;
		esd->langDesc->langCode |= trak->Media->mediaHeader->packedLanguage[2];
	}


	{
		u16 rvc_predefined;
		u8 *rvc_cfg_data;
		const char *mime_type;
		u32 rvc_cfg_size;
		e = gf_isom_get_rvc_config(moov->mov, track_num, 1, &rvc_predefined, &rvc_cfg_data, &rvc_cfg_size, &mime_type);
		if (e==GF_OK) {
			if (rvc_predefined) {
				esd->decoderConfig->predefined_rvc_config = rvc_predefined;
			} else {
				esd->decoderConfig->rvc_config = (GF_DefaultDescriptor *) gf_odf_desc_new(GF_ODF_DSI_TAG);
				if (mime_type && !strcmp(mime_type, "application/rvc-config+xml+gz") ) {
#if !defined(GPAC_DISABLE_CORE_TOOLS) && !defined(GPAC_DISABLE_ZLIB)
					gf_gz_decompress_payload(rvc_cfg_data, rvc_cfg_size, &esd->decoderConfig->rvc_config->data, &esd->decoderConfig->rvc_config->dataLength);
					gf_free(rvc_cfg_data);
#endif
				} else {
					esd->decoderConfig->rvc_config->data = rvc_cfg_data;
					esd->decoderConfig->rvc_config->dataLength = rvc_cfg_size;
				}
			}
		}
	}


	/*normally all files shall be stored with predefined=SLPredef_MP4, but of course some are broken (philips)
	so we just check the ESD_URL. If set, use the given cfg, otherwise always rewrite it*/
	if (esd->URLString != NULL) {
		*outESD = esd;
		return GF_OK;
	}

	//if we are in publishing mode and we have an SLConfig specified, use it as is
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_MP4V:
		slc = ((GF_MPEGVisualSampleEntryBox *)entry)->slc;
		break;
	case GF_ISOM_BOX_TYPE_MP4A:
		slc = ((GF_MPEGAudioSampleEntryBox *)entry)->slc;
		break;
	case GF_ISOM_BOX_TYPE_MP4S:
		slc = entry->slc;
		break;
	default:
		slc = NULL;
		break;
	}
	if (slc) {
		gf_odf_desc_del((GF_Descriptor *)esd->slConfig);
		gf_odf_desc_copy((GF_Descriptor *)slc, (GF_Descriptor **)&esd->slConfig);
		*outESD = esd;
		return GF_OK;
	}
	//otherwise use the regular mapping

	//this is a desc for a media in the file, let's rewrite some param
	esd->slConfig->timestampLength = 32;
	esd->slConfig->timestampResolution = trak->Media->mediaHeader->timeScale;
	//NO OCR from MP4File streams (eg, constant OC Res one)
	esd->slConfig->OCRLength = 0;
	esd->slConfig->OCRResolution = 0;
//	if (OCRTrack) esd->slConfig->OCRResolution = OCRTrack->Media->mediaHeader->timeScale;

	stbl = trak->Media->information->sampleTable;
	// a little optimization here: if all our samples are sync,
	//set the RAPOnly to true... for external users...
	if (! stbl->SyncSample) {
		if (
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		    moov->mvex &&
#endif
		    (esd->decoderConfig->streamType==GF_STREAM_VISUAL)) {
			esd->slConfig->hasRandomAccessUnitsOnlyFlag = 0;
			esd->slConfig->useRandomAccessPointFlag = 1;
			if (trak->moov->mov->openMode!=GF_ISOM_OPEN_READ)
				stbl->SyncSample = (GF_SyncSampleBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSS);
		} else {
			esd->slConfig->hasRandomAccessUnitsOnlyFlag = 1;
			esd->slConfig->useRandomAccessPointFlag = 0;
		}
	} else {
		esd->slConfig->hasRandomAccessUnitsOnlyFlag = 0;
		//signal we are NOT using sync points if no info is present in the table
		esd->slConfig->useRandomAccessPointFlag = stbl->SyncSample->nb_entries ? 1 : 0;
	}
	//do we have DegradationPriority ?
	if (stbl->DegradationPriority) {
		esd->slConfig->degradationPriorityLength = 15;
	} else {
		esd->slConfig->degradationPriorityLength = 0;
	}
	//paddingBits
	if (stbl->PaddingBits) {
		esd->slConfig->usePaddingFlag = 1;
	}
	//change to support reflecting OD streams
	esd->slConfig->useAccessUnitEndFlag = 1;
	esd->slConfig->useAccessUnitStartFlag = 1;

	//signal we do have padding flag (since we only use logical SL packet
	//the user can decide whether to use the info or not
	esd->slConfig->usePaddingFlag = stbl->PaddingBits ? 1 : 0;

	//same with degradation priority
	esd->slConfig->degradationPriorityLength = stbl->DegradationPriority ? 32 : 0;

	//this new SL will be OUT OF THE FILE. Let's set its predefined to 0
	esd->slConfig->predefined = 0;


	*outESD = esd;
	return GF_OK;
}


//extraction of the ESD from the track for the given time
GF_Err GetESDForTime(GF_MovieBox *moov, GF_ISOTrackID trackID, u64 CTS, GF_ESD **outESD)
{
	GF_Err e;
	u32 sampleDescIndex;
	GF_TrackBox *trak;

	trak = gf_isom_get_track(moov, gf_isom_get_tracknum_from_id(moov, trackID));
	if (!trak) return GF_ISOM_INVALID_FILE;

	e = Media_GetSampleDescIndex(trak->Media, CTS, &sampleDescIndex );
	if (e) return e;
	return GetESD(moov, trackID, sampleDescIndex, outESD);
}


GF_Err Track_FindRef(GF_TrackBox *trak, u32 ReferenceType, GF_TrackReferenceTypeBox **dpnd)
{
	GF_TrackReferenceBox *ref;
	GF_TrackReferenceTypeBox *a;
	u32 i;
	if (! trak) return GF_BAD_PARAM;
	if (! trak->References) {
		*dpnd = NULL;
		return GF_OK;
	}
	ref = trak->References;
	i=0;
	while ((a = (GF_TrackReferenceTypeBox *)gf_list_enum(ref->child_boxes, &i))) {
		if (a->reference_type == ReferenceType) {
			*dpnd = a;
			return GF_OK;
		}
	}
	*dpnd = NULL;
	return GF_OK;
}

Bool Track_IsMPEG4Stream(u32 HandlerType)
{
	switch (HandlerType) {
	case GF_ISOM_MEDIA_VISUAL:
    case GF_ISOM_MEDIA_AUXV:
    case GF_ISOM_MEDIA_PICT:
	case GF_ISOM_MEDIA_AUDIO:
	case GF_ISOM_MEDIA_SUBPIC:
	case GF_ISOM_MEDIA_OD:
	case GF_ISOM_MEDIA_OCR:
	case GF_ISOM_MEDIA_SCENE:
	case GF_ISOM_MEDIA_MPEG7:
	case GF_ISOM_MEDIA_OCI:
	case GF_ISOM_MEDIA_IPMP:
	case GF_ISOM_MEDIA_MPEGJ:
	case GF_ISOM_MEDIA_ESM:
		return 1;
	/*Timedtext is NOT an MPEG-4 stream*/
	default:
		/*consider xxsm as MPEG-4 handlers*/
		if ( (((HandlerType>>8) & 0xFF)== 's') && ((HandlerType& 0xFF)== 'm'))
			return 1;
		return 0;
	}
}


GF_Err SetTrackDuration(GF_TrackBox *trak)
{
	u64 trackDuration;
	u32 i;
	GF_EdtsEntry *ent;
	GF_EditListBox *elst;
	GF_Err e;

	//the total duration is the media duration: adjust it in case...
	e = Media_SetDuration(trak);
	if (e) return e;

	//assert the timeScales are non-NULL
	if (!trak->moov->mvhd->timeScale || !trak->Media->mediaHeader->timeScale) return GF_ISOM_INVALID_FILE;
	trackDuration = (trak->Media->mediaHeader->duration * trak->moov->mvhd->timeScale) / trak->Media->mediaHeader->timeScale;

	//if we have an edit list, the duration is the sum of all the editList
	//entries' duration (always expressed in MovieTimeScale)
	if (trak->editBox && trak->editBox->editList) {
		trackDuration = 0;
		elst = trak->editBox->editList;
		i=0;
		while ((ent = (GF_EdtsEntry*)gf_list_enum(elst->entryList, &i))) {
			trackDuration += ent->segmentDuration;
		}
	}
	if (!trackDuration) {
		trackDuration = (trak->Media->mediaHeader->duration * trak->moov->mvhd->timeScale) / trak->Media->mediaHeader->timeScale;
	}
	trak->Header->duration = trackDuration;
	if (!trak->moov->mov->keep_utc && !gf_sys_is_test_mode() )
		trak->Header->modificationTime = gf_isom_get_mp4time();
	return GF_OK;
}


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

GF_Err MergeTrack(GF_TrackBox *trak, GF_TrackFragmentBox *traf, GF_MovieFragmentBox *moof_box, u64 moof_offset, u64 *cumulated_offset, Bool is_first_merge)
{
	u32 i, j, chunk_size, track_num;
	u64 base_offset, data_offset, traf_duration;
	u32 def_duration, DescIndex, def_size, def_flags;
	u32 duration, size, flags, cts_offset, prev_trun_data_offset;
	u8 pad, sync;
	u16 degr;
	Bool first_samp_in_traf=GF_TRUE;
	u8 *moof_template=NULL;
	u32 moof_template_size=0;
	Bool is_seg_start = GF_FALSE;
	u64 seg_start=0, frag_start=0;
	GF_TrackFragmentRunBox *trun;
	GF_TrunEntry *ent;

	void stbl_AppendTime(GF_SampleTableBox *stbl, u32 duration, u32 nb_pack);
	void stbl_AppendSize(GF_SampleTableBox *stbl, u32 size, u32 nb_pack);
	void stbl_AppendChunk(GF_SampleTableBox *stbl, u64 offset);
	void stbl_AppendSampleToChunk(GF_SampleTableBox *stbl, u32 DescIndex, u32 samplesInChunk);
	void stbl_AppendCTSOffset(GF_SampleTableBox *stbl, s32 CTSOffset);
	void stbl_AppendRAP(GF_SampleTableBox *stbl, u8 isRap);
	void stbl_AppendPadding(GF_SampleTableBox *stbl, u8 padding);
	void stbl_AppendDegradation(GF_SampleTableBox *stbl, u16 DegradationPriority);

	if (trak->Header->trackID != traf->tfhd->trackID) return GF_OK;

	//setup all our defaults
	DescIndex = (traf->tfhd->flags & GF_ISOM_TRAF_SAMPLE_DESC) ? traf->tfhd->sample_desc_index : traf->trex->def_sample_desc_index;
	if (!DescIndex) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] default sample description set to 0, likely broken ! Fixing to 1\n" ));
		DescIndex = 1;
	}

	def_duration = (traf->tfhd->flags & GF_ISOM_TRAF_SAMPLE_DUR) ? traf->tfhd->def_sample_duration : traf->trex->def_sample_duration;
	def_size = (traf->tfhd->flags & GF_ISOM_TRAF_SAMPLE_SIZE) ? traf->tfhd->def_sample_size : traf->trex->def_sample_size;
	def_flags = (traf->tfhd->flags & GF_ISOM_TRAF_SAMPLE_FLAGS) ? traf->tfhd->def_sample_flags : traf->trex->def_sample_flags;

	//locate base offset, by default use moof (dash-like)
	base_offset = moof_offset;
	//explicit base offset, use it
	if (traf->tfhd->flags & GF_ISOM_TRAF_BASE_OFFSET)
		base_offset = traf->tfhd->base_data_offset;
	//no moof offset and no explicit offset, the offset is the end of the last written chunk of
	//the previous traf. For the first traf, *cumulated_offset is actually moof offset
	else if (!(traf->tfhd->flags & GF_ISOM_MOOF_BASE_OFFSET))
		base_offset = *cumulated_offset;

	chunk_size = 0;
	prev_trun_data_offset = 0;
	data_offset = 0;
	traf_duration = 0;

	/*in playback mode*/
	if (traf->tfdt && is_first_merge) {
#ifndef GPAC_DISABLE_LOG
		if (trak->moov->mov->NextMoofNumber && trak->present_in_scalable_segment && trak->sample_count_at_seg_start && (trak->dts_at_seg_start != traf->tfdt->baseMediaDecodeTime)) {
			s32 drift = (s32) ((s64) traf->tfdt->baseMediaDecodeTime - (s64)trak->dts_at_seg_start);
			if (drift<0)  {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Warning: TFDT timing "LLD" less than cumulated timing "LLD" - using tfdt\n", traf->tfdt->baseMediaDecodeTime, trak->dts_at_seg_start ));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[iso file] TFDT timing "LLD" higher than cumulated timing "LLD" (last sample got extended in duration)\n", traf->tfdt->baseMediaDecodeTime, trak->dts_at_seg_start ));
			}
		}
#endif
		trak->dts_at_seg_start = traf->tfdt->baseMediaDecodeTime;
	}

	if (is_first_merge && trak->moov->mov->signal_frag_bounds) {
		GF_MovieFragmentBox *moof_clone = NULL;
		gf_isom_box_freeze_order((GF_Box *)moof_box);
		gf_isom_clone_box((GF_Box *)moof_box, (GF_Box **)&moof_clone);

		if (moof_clone) {
			GF_BitStream *bs;
			for (i=0; i<gf_list_count(moof_clone->TrackList); i++) {
				GF_TrackFragmentBox *traf_clone = gf_list_get(moof_clone->TrackList, i);
				gf_isom_box_array_reset_parent(&traf_clone->child_boxes, traf_clone->TrackRuns);
				gf_isom_box_array_reset_parent(&traf_clone->child_boxes, traf_clone->sampleGroups);
				gf_isom_box_array_reset_parent(&traf_clone->child_boxes, traf_clone->sampleGroupsDescription);
				gf_isom_box_array_reset_parent(&traf_clone->child_boxes, traf_clone->sub_samples);
				gf_isom_box_array_reset_parent(&traf_clone->child_boxes, traf_clone->sai_offsets);
				gf_isom_box_array_reset_parent(&traf_clone->child_boxes, traf_clone->sai_sizes);
				if (traf_clone->sample_encryption) {
					gf_isom_box_del_parent(&traf_clone->child_boxes, (GF_Box *) traf_clone->sample_encryption);
					traf_clone->sample_encryption = NULL;
				}
				if (traf_clone->sdtp) {
					gf_isom_box_del_parent(&traf_clone->child_boxes, (GF_Box *) traf_clone->sdtp);
					traf_clone->sdtp = NULL;
				}
			}
			gf_isom_box_size((GF_Box *)moof_clone);
			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	 		if (trak->moov->mov->seg_styp) {
				gf_isom_box_size(trak->moov->mov->seg_styp);
				gf_isom_box_write(trak->moov->mov->seg_styp, bs);
				is_seg_start = GF_TRUE;
				seg_start = trak->moov->mov->styp_start_offset;
			}
	 		if (trak->moov->mov->root_sidx) {
	 			is_seg_start = GF_TRUE;
				gf_isom_box_size((GF_Box *)trak->moov->mov->root_sidx);
				gf_isom_box_write((GF_Box *)trak->moov->mov->root_sidx, bs);
				seg_start = trak->moov->mov->sidx_start_offset;
			}
	 		if (trak->moov->mov->seg_ssix) {
				gf_isom_box_size(trak->moov->mov->seg_ssix);
				gf_isom_box_write(trak->moov->mov->seg_ssix, bs);
				seg_start = trak->moov->mov->sidx_start_offset;
			}

			frag_start = trak->moov->mov->current_top_box_start;
			gf_isom_box_write((GF_Box *)moof_clone, bs);
			gf_isom_box_del((GF_Box*)moof_clone);

			gf_bs_get_content(bs, &moof_template, &moof_template_size);
			gf_bs_del(bs);
		}
	}

	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		//merge the run
		for (j=0; j<trun->sample_count; j++) {
			ent = (GF_TrunEntry*)gf_list_get(trun->entries, j);

			if (!ent) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Track %d doesn't have enough trun entries (%d) compared to sample count (%d) in run\n", traf->trex->trackID, gf_list_count(trun->entries), trun->sample_count ));
				break;
			}
			size = def_size;
			duration = def_duration;
			flags = def_flags;

			if (ent) {
				if (trun->flags & GF_ISOM_TRUN_DURATION) duration = ent->Duration;
				if (trun->flags & GF_ISOM_TRUN_SIZE) size = ent->size;
				if (trun->flags & GF_ISOM_TRUN_FLAGS) {
					flags = ent->flags;
				} else if (!j && (trun->flags & GF_ISOM_TRUN_FIRST_FLAG)) {
					flags = trun->first_sample_flags;
				}
			}
			//add size first
			stbl_AppendSize(trak->Media->information->sampleTable, size, ent->nb_pack);
			//then TS
			stbl_AppendTime(trak->Media->information->sampleTable, duration, ent->nb_pack);

			//add chunk on first sample
			if (!j) {
				data_offset = base_offset;
				//we have an explicit data offset for this trun
				if (trun->flags & GF_ISOM_TRUN_DATA_OFFSET) {
					data_offset += trun->data_offset;
					/*reset chunk size since data is now relative to this trun*/
					chunk_size = 0;
					/*remember this data offset for following trun*/
					prev_trun_data_offset = trun->data_offset;
				}
				//we had an explicit data offset for the previous trun, use it + chunk size
				else if (prev_trun_data_offset) {
					/*data offset is previous chunk size plus previous offset of the trun*/
					data_offset += prev_trun_data_offset + chunk_size;
				}
				//no explicit data offset, continuous data after last data in previous chunk
				else {
					data_offset += chunk_size;
				}
				stbl_AppendChunk(trak->Media->information->sampleTable, data_offset);
				//then sampleToChunk
				stbl_AppendSampleToChunk(trak->Media->information->sampleTable,
				                         DescIndex, trun->sample_count);
			}
			chunk_size += size;

			if (first_samp_in_traf) {
				first_samp_in_traf = GF_FALSE;
				stbl_AppendTrafMap(trak->Media->information->sampleTable, is_seg_start, seg_start, frag_start, moof_template, moof_template_size);
				//do not deallocate, the memory is now owned by traf map
				moof_template = NULL;
				moof_template_size = 0;
			}
			if (ent->nb_pack) {
				j+= ent->nb_pack-1;
				traf_duration += ent->nb_pack*duration;
				continue;
			}

			traf_duration += duration;

			//CTS
			cts_offset = (trun->flags & GF_ISOM_TRUN_CTS_OFFSET) ? ent->CTS_Offset : 0;
			stbl_AppendCTSOffset(trak->Media->information->sampleTable, cts_offset);

			//flags
			sync = GF_ISOM_GET_FRAG_SYNC(flags);
			if (trak->Media->information->sampleTable->no_sync_found && sync) {
				trak->Media->information->sampleTable->no_sync_found = 0;
			}
			stbl_AppendRAP(trak->Media->information->sampleTable, sync);
			pad = GF_ISOM_GET_FRAG_PAD(flags);
			if (pad) stbl_AppendPadding(trak->Media->information->sampleTable, pad);
			degr = GF_ISOM_GET_FRAG_DEG(flags);
			if (degr) stbl_AppendDegradation(trak->Media->information->sampleTable, degr);

			stbl_AppendDependencyType(trak->Media->information->sampleTable, GF_ISOM_GET_FRAG_LEAD(flags), GF_ISOM_GET_FRAG_DEPENDS(flags), GF_ISOM_GET_FRAG_DEPENDED(flags), GF_ISOM_GET_FRAG_REDUNDANT(flags));
		}
	}
	if (traf_duration && trak->editBox && trak->editBox->editList) {
		for (i=0; i<gf_list_count(trak->editBox->editList->entryList); i++) {
			GF_EdtsEntry *ent = gf_list_get(trak->editBox->editList->entryList, i);
			if (ent->was_empty_dur) {
				u64 extend_dur = traf_duration;
				extend_dur *= trak->moov->mvhd->timeScale;
				extend_dur /= trak->Media->mediaHeader->timeScale;
				ent->segmentDuration += extend_dur;
			}
			else if (!ent->segmentDuration) {
				ent->was_empty_dur = GF_TRUE;
				if ((s64) traf_duration > ent->mediaTime)
					traf_duration -= ent->mediaTime;
				else
					traf_duration = 0;

				ent->segmentDuration = traf_duration;
				ent->segmentDuration *= trak->moov->mvhd->timeScale;
				ent->segmentDuration /= trak->Media->mediaHeader->timeScale;
			}

		}
	}

	//in any case, update the cumulated offset
	//this will handle hypothetical files mixing MOOF offset and implicit non-moof offset
	*cumulated_offset = data_offset + chunk_size;

	/*merge sample groups*/
	if (traf->sampleGroups) {
		GF_List *groups;
		GF_List *groupDescs;
		Bool is_identical_sgpd = GF_TRUE;
		u32 *new_idx = NULL;

		if (!trak->Media->information->sampleTable->sampleGroups)
			trak->Media->information->sampleTable->sampleGroups = gf_list_new();

		if (!trak->Media->information->sampleTable->sampleGroupsDescription)
			trak->Media->information->sampleTable->sampleGroupsDescription = gf_list_new();

		groupDescs = trak->Media->information->sampleTable->sampleGroupsDescription;
		for (i=0; i<gf_list_count(traf->sampleGroupsDescription); i++) {
			GF_SampleGroupDescriptionBox *new_sgdesc = NULL;
			GF_SampleGroupDescriptionBox *sgdesc = gf_list_get(traf->sampleGroupsDescription, i);
			for (j=0; j<gf_list_count(groupDescs); j++) {
				new_sgdesc = gf_list_get(groupDescs, j);
				if (new_sgdesc->grouping_type==sgdesc->grouping_type) break;
				new_sgdesc = NULL;
			}
			/*new description, move it to our sample table*/
			if (!new_sgdesc) {
				gf_list_add(groupDescs, sgdesc);
				gf_list_add(trak->Media->information->sampleTable->child_boxes, sgdesc);
				gf_list_rem(traf->sampleGroupsDescription, i);
				gf_list_del_item(traf->child_boxes, sgdesc);
				i--;
			}
			/*merge descriptions*/
			else {
				u32 count;

				is_identical_sgpd = gf_isom_is_identical_sgpd(new_sgdesc, sgdesc, 0);
				if (is_identical_sgpd)
					continue;

				new_idx = (u32 *)gf_malloc(gf_list_count(sgdesc->group_descriptions)*sizeof(u32));
				count = 0;
				while (gf_list_count(sgdesc->group_descriptions)) {
					void *sgpd_entry = gf_list_get(sgdesc->group_descriptions, 0);
					Bool new_entry = GF_TRUE;

					for (j = 0; j < gf_list_count(new_sgdesc->group_descriptions); j++) {
						void *ptr = gf_list_get(new_sgdesc->group_descriptions, j);
						if (gf_isom_is_identical_sgpd(sgpd_entry, ptr, new_sgdesc->grouping_type)) {
							new_idx[count] = j + 1;
							count ++;
							new_entry = GF_FALSE;
							gf_free(sgpd_entry);
							break;
						}
					}

					if (new_entry) {
						gf_list_add(new_sgdesc->group_descriptions, sgpd_entry);
						new_idx[count] = gf_list_count(new_sgdesc->group_descriptions);
						count ++;
					}

					gf_list_rem(sgdesc->group_descriptions, 0);
				}
			}
		}

		groups = trak->Media->information->sampleTable->sampleGroups;
		for (i=0; i<gf_list_count(traf->sampleGroups); i++) {
			GF_SampleGroupBox *stbl_group = NULL;
			GF_SampleGroupBox *frag_group = gf_list_get(traf->sampleGroups, i);


			for (j=0; j<gf_list_count(groups); j++) {
				stbl_group = gf_list_get(groups, j);
				if ((frag_group->grouping_type==stbl_group->grouping_type) && (frag_group->grouping_type_parameter==stbl_group->grouping_type_parameter))
					break;
				stbl_group = NULL;
			}
			if (!stbl_group) {
				stbl_group = (GF_SampleGroupBox *) gf_isom_box_new_parent(&trak->Media->information->sampleTable->child_boxes, GF_ISOM_BOX_TYPE_SBGP);
				stbl_group->grouping_type = frag_group->grouping_type;
				stbl_group->grouping_type_parameter = frag_group->grouping_type_parameter;
				stbl_group->version = frag_group->version;
				gf_list_add(groups, stbl_group);
			}

			if (is_identical_sgpd) {
				//adjust sgpd index: in traf index start at 0x1001
				for (j = 0; j < frag_group->entry_count; j++)
					frag_group->sample_entries[j].group_description_index &= 0x0FFFF;
				if (frag_group->entry_count && stbl_group->entry_count &&
				        (frag_group->sample_entries[0].group_description_index==stbl_group->sample_entries[stbl_group->entry_count-1].group_description_index)
				   ) {
					stbl_group->sample_entries[stbl_group->entry_count - 1].sample_count += frag_group->sample_entries[0].sample_count;
					if (frag_group->entry_count>1) {
						stbl_group->sample_entries = gf_realloc(stbl_group->sample_entries, sizeof(GF_SampleGroupEntry) * (stbl_group->entry_count + frag_group->entry_count - 1));
						memcpy(&stbl_group->sample_entries[stbl_group->entry_count], &frag_group->sample_entries[1], sizeof(GF_SampleGroupEntry) * (frag_group->entry_count - 1));
						stbl_group->entry_count += frag_group->entry_count - 1;
					}
				} else {
					stbl_group->sample_entries = gf_realloc(stbl_group->sample_entries, sizeof(GF_SampleGroupEntry) * (stbl_group->entry_count + frag_group->entry_count));
					memcpy(&stbl_group->sample_entries[stbl_group->entry_count], &frag_group->sample_entries[0], sizeof(GF_SampleGroupEntry) * frag_group->entry_count);
					stbl_group->entry_count += frag_group->entry_count;
				}
			} else {
				stbl_group->sample_entries = gf_realloc(stbl_group->sample_entries, sizeof(GF_SampleGroupEntry) * (stbl_group->entry_count + frag_group->entry_count));
				//adjust sgpd index
				for (j = 0; j < frag_group->entry_count; j++)
					frag_group->sample_entries[j].group_description_index = new_idx[j];
				memcpy(&stbl_group->sample_entries[stbl_group->entry_count], &frag_group->sample_entries[0], sizeof(GF_SampleGroupEntry) * frag_group->entry_count);
				stbl_group->entry_count += frag_group->entry_count;
			}
		}

		if (new_idx) gf_free(new_idx);
	}

	/*content is encrypted*/
	track_num = gf_isom_get_tracknum_from_id(trak->moov, trak->Header->trackID);
	if (gf_isom_is_cenc_media(trak->moov->mov, track_num, 1)
		|| traf->sample_encryption) {
		/*Merge sample auxiliary encryption information*/
		GF_SampleEncryptionBox *senc = NULL;
		GF_List *sais = NULL;
		u32 scheme_type;
		gf_isom_get_cenc_info(trak->moov->mov, track_num, 1, NULL, &scheme_type, NULL, NULL);

		if (traf->sample_encryption) {
			for (i = 0; i < gf_list_count(trak->Media->information->sampleTable->child_boxes); i++) {
				GF_Box *a = (GF_Box *)gf_list_get(trak->Media->information->sampleTable->child_boxes, i);
				if (a->type != traf->sample_encryption->type) continue;

				if ((a->type ==GF_ISOM_BOX_TYPE_UUID) && (((GF_UUIDBox *)a)->internal_4cc == GF_ISOM_BOX_UUID_PSEC)) {
					senc = (GF_SampleEncryptionBox *)a;
					break;
				}
				else if (a->type ==GF_ISOM_BOX_TYPE_SENC) {
					senc = (GF_SampleEncryptionBox *)a;
					break;
				}
			}
			if (!senc && trak->sample_encryption)
				senc = trak->sample_encryption;

			if (!senc) {
				if (traf->sample_encryption->is_piff) {
					senc = (GF_SampleEncryptionBox *)gf_isom_create_piff_psec_box(1, 0x2, 0, 0, NULL);
				} else {
					senc = gf_isom_create_samp_enc_box(1, 0x2);
				}

				if (!trak->Media->information->sampleTable->child_boxes) trak->Media->information->sampleTable->child_boxes = gf_list_new();

				trak->sample_encryption = senc;
				if (!trak->child_boxes) trak->child_boxes = gf_list_new();
				gf_list_add(trak->child_boxes, senc);
			}

			sais = traf->sample_encryption->samp_aux_info;
		}

		/*get sample auxiliary information by saiz/saio rather than by parsing senc box*/
		if (gf_isom_cenc_has_saiz_saio_traf(traf, scheme_type)) {
			u32 size, nb_saio;
			u32 aux_info_type;
			u64 offset;
			GF_Err e;
			Bool is_encrypted;
			GF_SampleAuxiliaryInfoOffsetBox *saio = NULL;
			GF_SampleAuxiliaryInfoSizeBox *saiz = NULL;

			offset = nb_saio = 0;

			for (i = 0; i < gf_list_count(traf->sai_offsets); i++) {
				saio = (GF_SampleAuxiliaryInfoOffsetBox *)gf_list_get(traf->sai_offsets, i);
				aux_info_type = saio->aux_info_type;
				if (!aux_info_type) aux_info_type = scheme_type;

				/*if we have only 1 sai_offsets, assume that its type is cenc*/
				if ((aux_info_type == GF_ISOM_CENC_SCHEME) || (aux_info_type == GF_ISOM_CBC_SCHEME) ||
					(aux_info_type == GF_ISOM_CENS_SCHEME) || (aux_info_type == GF_ISOM_CBCS_SCHEME) ||
					(gf_list_count(traf->sai_offsets) == 1)) {
					offset = saio->offsets[0] + moof_offset;
					nb_saio = saio->entry_count;
					break;
				}
			}
			for (i = 0; i < gf_list_count(traf->sai_sizes); i++) {
				saiz = (GF_SampleAuxiliaryInfoSizeBox *)gf_list_get(traf->sai_sizes, i);
				aux_info_type = saiz->aux_info_type;
				if (!aux_info_type) aux_info_type = scheme_type;
				/*if we have only 1 sai_sizes, assume that its type is cenc*/
				if ((aux_info_type == GF_ISOM_CENC_SCHEME) || (aux_info_type == GF_ISOM_CBC_SCHEME) ||
					(aux_info_type == GF_ISOM_CENS_SCHEME) || (aux_info_type == GF_ISOM_CBCS_SCHEME) ||
					(gf_list_count(traf->sai_sizes) == 1)) {
					break;
				}
			}
			if (saiz && saio) {
				for (i = 0; i < saiz->sample_count; i++) {
					GF_CENCSampleAuxInfo *sai;

					u64 cur_position;
					if (nb_saio != 1)
						offset = saio->offsets[i] + moof_offset;
					size = saiz->default_sample_info_size ? saiz->default_sample_info_size : saiz->sample_info_size[i];


					cur_position = gf_bs_get_position(trak->moov->mov->movieFileMap->bs);
					gf_bs_seek(trak->moov->mov->movieFileMap->bs, offset);

					GF_SAFEALLOC(sai, GF_CENCSampleAuxInfo);

					e = gf_isom_get_sample_cenc_info_ex(trak, traf, senc, i+1, &is_encrypted, &sai->IV_size, NULL, NULL, NULL, NULL, NULL);
					if (e) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isobmf] could not get cenc info for sample %d: %s\n", i+1, gf_error_to_string(e) ));
						return e;
					}

					if (is_encrypted) {
						gf_bs_read_data(trak->moov->mov->movieFileMap->bs, (char *)sai->IV, sai->IV_size);
						if (size > sai->IV_size) {
							sai->subsample_count = gf_bs_read_u16(trak->moov->mov->movieFileMap->bs);
							sai->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(sizeof(GF_CENCSubSampleEntry)*sai->subsample_count);
							for (j = 0; j < sai->subsample_count; j++) {
								sai->subsamples[j].bytes_clear_data = gf_bs_read_u16(trak->moov->mov->movieFileMap->bs);
								sai->subsamples[j].bytes_encrypted_data = gf_bs_read_u32(trak->moov->mov->movieFileMap->bs);
							}
						}
					} else {
						sai->IV_size=0;
						sai->subsample_count=0;
					}

					gf_bs_seek(trak->moov->mov->movieFileMap->bs, cur_position);

					gf_list_add(senc->samp_aux_info, sai);
					if (sai->subsample_count) senc->flags = 0x00000002;
					gf_isom_cenc_merge_saiz_saio(senc, trak->Media->information->sampleTable, offset, size);
					if (nb_saio == 1)
						offset += size;
				}
			}
		} else if (sais) {
			for (i = 0; i < gf_list_count(sais); i++) {
				GF_CENCSampleAuxInfo *sai, *new_sai;

				sai = (GF_CENCSampleAuxInfo *)gf_list_get(sais, i);

				new_sai = (GF_CENCSampleAuxInfo *)gf_malloc(sizeof(GF_CENCSampleAuxInfo));
				new_sai->IV_size = sai->IV_size;
				memmove((char *)new_sai->IV, (const char*)sai->IV, 16);
				new_sai->subsample_count = sai->subsample_count;
				new_sai->subsamples = (GF_CENCSubSampleEntry *)gf_malloc(new_sai->subsample_count*sizeof(GF_CENCSubSampleEntry));
				memmove(new_sai->subsamples, sai->subsamples, new_sai->subsample_count*sizeof(GF_CENCSubSampleEntry));

				gf_list_add(senc->samp_aux_info, new_sai);
				if (sai->subsample_count) senc->flags = 0x00000002;
			}
		} else if (traf->sample_encryption) {
			senc_Parse(trak->moov->mov->movieFileMap->bs, trak, traf, traf->sample_encryption);
			trak->sample_encryption->AlgorithmID = traf->sample_encryption->AlgorithmID;
			if (!trak->sample_encryption->IV_size)
				trak->sample_encryption->IV_size = traf->sample_encryption->IV_size;
			if (!trak->sample_encryption->samp_aux_info) trak->sample_encryption->samp_aux_info = gf_list_new();
			gf_list_transfer(trak->sample_encryption->samp_aux_info, traf->sample_encryption->samp_aux_info);
		}
	}
	return GF_OK;
}

#endif


#ifndef GPAC_DISABLE_ISOM_WRITE

//used to check if a TrackID is available
u8 RequestTrack(GF_MovieBox *moov, GF_ISOTrackID TrackID)
{
	u32 i;
	GF_TrackBox *trak;

	i=0;
	while ((trak = (GF_TrackBox *)gf_list_enum(moov->trackList, &i))) {
		if (trak->Header->trackID == TrackID) {
			gf_isom_set_last_error(moov->mov, GF_BAD_PARAM);
			return 0;
		}
	}
	return 1;
}

GF_Err Track_RemoveRef(GF_TrackBox *trak, u32 ReferenceType)
{
	GF_TrackReferenceBox *ref;
	GF_Box *a;
	u32 i;
	if (! trak) return GF_BAD_PARAM;
	if (! trak->References) return GF_OK;
	ref = trak->References;
	i=0;
	while ((a = (GF_Box *)gf_list_enum(ref->child_boxes, &i))) {
		if (a->type == ReferenceType) {
			gf_isom_box_del_parent(&ref->child_boxes, a);
			return GF_OK;
		}
	}
	return GF_OK;
}

GF_Err NewMedia(GF_MediaBox **mdia, u32 MediaType, u32 TimeScale)
{
	GF_MediaHeaderBox *mdhd;
	GF_Box *mediaInfo;
	GF_HandlerBox *hdlr;
	GF_MediaInformationBox *minf;
	GF_DataInformationBox *dinf;
	GF_SampleTableBox *stbl;
	GF_DataReferenceBox *dref;
	char *str="";

	GF_Err e;

	if (!mdia) return GF_BAD_PARAM;

	minf = *mdia ? (*mdia)->information : NULL;
	mdhd = *mdia ? (*mdia)->mediaHeader : NULL;
	hdlr = *mdia ? (*mdia)->handler : NULL;
	dinf =  minf ? minf->dataInformation : NULL;
	stbl = minf ? minf->sampleTable : NULL;
	dref = dinf ? dinf->dref : NULL;
	mediaInfo = minf ? minf->InfoHeader : NULL;

	//first create the media
	if (!*mdia) *mdia = (GF_MediaBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MDIA);
	if (!mdhd) {
		mdhd = (GF_MediaHeaderBox *) gf_isom_box_new_parent( & ((*mdia)->child_boxes), GF_ISOM_BOX_TYPE_MDHD);
		e = mdia_on_child_box((GF_Box*)*mdia, (GF_Box *) mdhd);
		if (e) goto err_exit;
	}
	if (!hdlr) {
		hdlr = (GF_HandlerBox *) gf_isom_box_new_parent(& ((*mdia)->child_boxes), GF_ISOM_BOX_TYPE_HDLR);
		e = mdia_on_child_box((GF_Box*)*mdia, (GF_Box *) hdlr);
		if (e) goto err_exit;
	}
	if (!minf) {
		minf = (GF_MediaInformationBox *) gf_isom_box_new_parent(& ((*mdia)->child_boxes), GF_ISOM_BOX_TYPE_MINF);
		e = mdia_on_child_box((GF_Box*)*mdia, (GF_Box *) minf);
		if (e) goto err_exit;
	}
	if (!dinf) {
		dinf = (GF_DataInformationBox *) gf_isom_box_new_parent(&minf->child_boxes, GF_ISOM_BOX_TYPE_DINF);
		e = minf_on_child_box((GF_Box*)minf, (GF_Box *) dinf);
		if (e) goto err_exit;
	}

	if (!mediaInfo) {
		//"handler name" is for debugging purposes. Let's stick our name here ;)
		switch (MediaType) {
		case GF_ISOM_MEDIA_VISUAL:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_VMHD);
			str = "GPAC ISO Video Handler";
			break;
		case GF_ISOM_MEDIA_AUXV:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_VMHD);
			str = "GPAC ISO Auxiliary Video Handler";
			break;
		case GF_ISOM_MEDIA_PICT:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_VMHD);
			str = "GPAC ISO Picture Sequence Handler";
			break;
		case GF_ISOM_MEDIA_AUDIO:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_SMHD);
			str = "GPAC ISO Audio Handler";
			break;
		case GF_ISOM_MEDIA_HINT:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_HMHD);
			str = "GPAC ISO Hint Handler";
			break;
		case GF_ISOM_MEDIA_META:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC Timed MetaData Handler";
			break;
		case GF_ISOM_MEDIA_OD:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC MPEG-4 OD Handler";
			break;
		case GF_ISOM_MEDIA_OCR:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC MPEG-4 OCR Handler";
			break;
		case GF_ISOM_MEDIA_SCENE:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC MPEG-4 Scene Description Handler";
			break;
		case GF_ISOM_MEDIA_MPEG7:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC MPEG-4 MPEG-7 Handler";
			break;
		case GF_ISOM_MEDIA_OCI:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC MPEG-4 OCI Handler";
			break;
		case GF_ISOM_MEDIA_IPMP:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC MPEG-4 IPMP Handler";
			break;
		case GF_ISOM_MEDIA_MPEGJ:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC MPEG-4 MPEG-J Handler";
			break;
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC Streaming Text Handler";
			break;
		case GF_ISOM_MEDIA_MPEG_SUBT:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_STHD);
			str = "GPAC MPEG Subtitle Handler";
			break;
		case GF_ISOM_MEDIA_DIMS:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_VMHD);
			MediaType = GF_ISOM_MEDIA_SCENE;
			str = "GPAC DIMS Handler";
			break;
		default:
			mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_NMHD);
			str = "GPAC IsoMedia Handler";
			break;
		}
		if (!minf->child_boxes) minf->child_boxes = gf_list_new();
		gf_list_add(minf->child_boxes, mediaInfo);

		e = minf_on_child_box((GF_Box*)minf, (GF_Box *) mediaInfo);
		if (e) goto err_exit;
	}

	mdhd->timeScale = TimeScale;
	hdlr->handlerType = MediaType;
	if (!hdlr->nameUTF8)
		hdlr->nameUTF8 = gf_strdup(str);

	if (!dref) {
		//Create a data reference WITHOUT DATA ENTRY (we don't know anything yet about the media Data)
		dref = (GF_DataReferenceBox *) gf_isom_box_new_parent(&dinf->child_boxes, GF_ISOM_BOX_TYPE_DREF);
		e = dinf_on_child_box((GF_Box*)dinf, (GF_Box *)dref);
		if (e) goto err_exit;
	}

	if (!stbl) {
		//first set-up the sample table...
		stbl = (GF_SampleTableBox *) gf_isom_box_new_parent(&minf->child_boxes, GF_ISOM_BOX_TYPE_STBL);

		e = minf_on_child_box((GF_Box*)minf, (GF_Box *) stbl);
		if (e) goto err_exit;
	}
	if (!stbl->SampleDescription)
		stbl->SampleDescription = (GF_SampleDescriptionBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSD);

	//by default create a regular table, 32 but offset and normal sample size
	if (!stbl->ChunkOffset)
		stbl->ChunkOffset = gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STCO);

	if (!stbl->SampleSize)
		stbl->SampleSize = (GF_SampleSizeBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSZ);

	if (!stbl->SampleToChunk)
		stbl->SampleToChunk = (GF_SampleToChunkBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSC);

	if (!stbl->TimeToSample)
		stbl->TimeToSample = (GF_TimeToSampleBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STTS);

	if (!stbl->SampleDescription)
		stbl->SampleDescription = (GF_SampleDescriptionBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSD);

	return GF_OK;

err_exit:
	if (mdhd) gf_isom_box_del_parent(& ((*mdia)->child_boxes), (GF_Box *)mdhd);
	if (minf) gf_isom_box_del_parent(& ((*mdia)->child_boxes), (GF_Box *)minf);
	if (hdlr) {
		gf_isom_box_del_parent(& ((*mdia)->child_boxes) , (GF_Box *)hdlr);
	}
	return e;

}

GF_Err Track_SetStreamDescriptor(GF_TrackBox *trak, u32 StreamDescriptionIndex, u32 DataReferenceIndex, GF_ESD *esd, u32 *outStreamIndex)
{
	GF_Err e;
	GF_MPEGSampleEntryBox *entry;
	GF_MPEGVisualSampleEntryBox *entry_v;
	GF_MPEGAudioSampleEntryBox *entry_a;
	GF_TrackReferenceBox *tref;
	GF_TrackReferenceTypeBox *dpnd;
	u16 tmpRef;

	entry = NULL;
	tref = NULL;

	if (!trak || !esd || (!outStreamIndex && !DataReferenceIndex) ) return GF_BAD_PARAM;
	if (!Track_IsMPEG4Stream(trak->Media->handler->handlerType)) return GF_ISOM_INVALID_MEDIA;


	esd->ESID = 0;
	//set SL to predefined if no url
	if (esd->URLString == NULL) {
		if (!esd->slConfig) esd->slConfig = (GF_SLConfig*) gf_odf_desc_new(GF_ODF_SLC_TAG);
		esd->slConfig->predefined = SLPredef_MP4;
		esd->slConfig->durationFlag = 0;
		esd->slConfig->useTimestampsFlag = 1;
	}

	//get the REF box if needed
	if (esd->dependsOnESID || (esd->OCRESID  && (esd->OCRESID != trak->moov->mov->es_id_default_sync)) ) {
		if (!trak->References) {
			tref = (GF_TrackReferenceBox *) gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_TREF);
			e = trak_on_child_box((GF_Box*)trak, (GF_Box *)tref);
			if (e) return e;
		}
		tref = trak->References;
	}

	//Update Stream dependencies
	e = Track_FindRef(trak, GF_ISOM_REF_DECODE, &dpnd);
	if (e) return e;

	if (!dpnd && esd->dependsOnESID) {
		e = Track_FindRef(trak, GF_ISOM_REF_BASE, &dpnd);
		if (e) return e;
	}

	if (!dpnd && esd->dependsOnESID) {
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new_parent(&tref->child_boxes, GF_ISOM_BOX_TYPE_REFT);
		dpnd->reference_type = GF_ISOM_BOX_TYPE_DPND;
		e = reftype_AddRefTrack(dpnd, esd->dependsOnESID, NULL);
		if (e) return e;
	} else if (dpnd && !esd->dependsOnESID) {
		Track_RemoveRef(trak, GF_ISOM_BOX_TYPE_DPND);
	}
	esd->dependsOnESID = 0;

	//Update GF_Clock dependencies
	e = Track_FindRef(trak, GF_ISOM_REF_OCR, &dpnd);
	if (e) return e;
	if (!dpnd && esd->OCRESID && (esd->OCRESID != trak->moov->mov->es_id_default_sync)) {
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new_parent(&tref->child_boxes, GF_ISOM_BOX_TYPE_REFT);
		dpnd->reference_type = GF_ISOM_BOX_TYPE_SYNC;
		e = reftype_AddRefTrack(dpnd, esd->OCRESID, NULL);
		if (e) return e;
	} else if (dpnd && !esd->OCRESID) {
		Track_RemoveRef(trak, GF_ISOM_BOX_TYPE_SYNC);
	} else if (dpnd && esd->OCRESID) {
		if (dpnd->trackIDCount != 1) return GF_ISOM_INVALID_MEDIA;
		dpnd->trackIDs[0] = esd->OCRESID;
	}
	esd->OCRESID = 0;

	//brand new case: we have to change the IPI desc
	if (esd->ipiPtr) {
		e = Track_FindRef(trak, GF_ISOM_REF_IPI, &dpnd);
		if (e) return e;
		if (!dpnd) {
			tmpRef = 0;
			dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new_parent(&tref->child_boxes, GF_ISOM_BOX_TYPE_REFT);
			dpnd->reference_type = GF_ISOM_BOX_TYPE_IPIR;
			e = reftype_AddRefTrack(dpnd, esd->ipiPtr->IPI_ES_Id, &tmpRef);
			if (e) return e;
			//and replace the tag and value...
			esd->ipiPtr->IPI_ES_Id = tmpRef;
			esd->ipiPtr->tag = GF_ODF_ISOM_IPI_PTR_TAG;
		} else {
			//Watch out! ONLY ONE IPI dependency is allowed per stream
			if (dpnd->trackIDCount != 1) return GF_ISOM_INVALID_MEDIA;
			//if an existing one is there, what shall we do ???
			//donno, erase it
			dpnd->trackIDs[0] = esd->ipiPtr->IPI_ES_Id;
			//and replace the tag and value...
			esd->ipiPtr->IPI_ES_Id = 1;
			esd->ipiPtr->tag = GF_ODF_ISOM_IPI_PTR_TAG;
		}
	}

	/*don't store the lang desc in ESD, use the media header language info*/
	if (esd->langDesc) {
		trak->Media->mediaHeader->packedLanguage[0] = (esd->langDesc->langCode>>16)&0xFF;
		trak->Media->mediaHeader->packedLanguage[1] = (esd->langDesc->langCode>>8)&0xFF;
		trak->Media->mediaHeader->packedLanguage[2] = (esd->langDesc->langCode)&0xFF;
		gf_odf_desc_del((GF_Descriptor *)esd->langDesc);
		esd->langDesc = NULL;
	}

	//we have a streamDescritpionIndex, use it
	if (StreamDescriptionIndex) {
		u32 entry_type;
		entry = (GF_MPEGSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex - 1);
		if (!entry) return GF_ISOM_INVALID_FILE;

		entry_type = entry->type;
		GF_ProtectionSchemeInfoBox *sinf = (GF_ProtectionSchemeInfoBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_SINF);
		if (sinf && sinf->original_format) entry_type = sinf->original_format->data_format;
		
		switch (entry_type) {
		case GF_ISOM_BOX_TYPE_MP4S:
			//OK, delete the previous ESD
			gf_odf_desc_del((GF_Descriptor *) entry->esd->desc);
			entry->esd->desc = esd;
			break;
		case GF_ISOM_BOX_TYPE_MP4V:
			entry_v = (GF_MPEGVisualSampleEntryBox*) entry;
			//OK, delete the previous ESD
			gf_odf_desc_del((GF_Descriptor *) entry_v->esd->desc);
			entry_v->esd->desc = esd;
			break;
		case GF_ISOM_BOX_TYPE_MP4A:
			entry_a = (GF_MPEGAudioSampleEntryBox*) entry;
            if (entry_a->esd) { // some non-conformant files may not have an ESD ...
                //OK, delete the previous ESD
                gf_odf_desc_del((GF_Descriptor *) entry_a->esd->desc);
                entry_a->esd->desc = esd;
            }
			break;
		case GF_ISOM_BOX_TYPE_AVC1:
		case GF_ISOM_BOX_TYPE_AVC2:
		case GF_ISOM_BOX_TYPE_AVC3:
		case GF_ISOM_BOX_TYPE_AVC4:
		case GF_ISOM_BOX_TYPE_SVC1:
		case GF_ISOM_BOX_TYPE_MVC1:
		case GF_ISOM_BOX_TYPE_HVC1:
		case GF_ISOM_BOX_TYPE_HEV1:
		case GF_ISOM_BOX_TYPE_HVC2:
		case GF_ISOM_BOX_TYPE_HEV2:
		case GF_ISOM_BOX_TYPE_LHE1:
		case GF_ISOM_BOX_TYPE_LHV1:
		case GF_ISOM_BOX_TYPE_HVT1:
			e = AVC_HEVC_UpdateESD((GF_MPEGVisualSampleEntryBox*)entry, esd);
			if (e) return e;
			break;
		case GF_ISOM_BOX_TYPE_LSR1:
			e = LSR_UpdateESD((GF_LASeRSampleEntryBox*)entry, esd);
			if (e) return e;
			break;
		case GF_ISOM_BOX_TYPE_AV01:
		case GF_ISOM_BOX_TYPE_AV1C:
		case GF_ISOM_BOX_TYPE_OPUS:
		case GF_ISOM_BOX_TYPE_DOPS:
		case GF_ISOM_BOX_TYPE_STXT:
		case GF_ISOM_BOX_TYPE_WVTT:
		case GF_ISOM_BOX_TYPE_STPP:
			if (esd) gf_odf_desc_del((GF_Descriptor *) esd);
			e = GF_OK;
			break;

		default:
			//silently fail, not an MPEG-4 esd
			gf_odf_desc_del((GF_Descriptor *) esd);
			return GF_OK;
		}
	} else {
		//need to check we're not in URL mode where only ONE description is allowed...
		StreamDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
		if (StreamDescriptionIndex) {
			entry = (GF_MPEGSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex - 1);
			if (!entry) return GF_ISOM_INVALID_FILE;
			if (entry->esd && entry->esd->desc->URLString) return GF_BAD_PARAM;
		}

		//OK, check the handler and create the entry
		switch (trak->Media->handler->handlerType) {
        case GF_ISOM_MEDIA_AUXV:
        case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_VISUAL:
			if ((esd->decoderConfig->objectTypeIndication==GF_CODECID_AVC) || (esd->decoderConfig->objectTypeIndication==GF_CODECID_SVC) || (esd->decoderConfig->objectTypeIndication==GF_CODECID_MVC)) {
				entry_v = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_AVC1);
				if (!entry_v) return GF_OUT_OF_MEM;
				e = AVC_HEVC_UpdateESD((GF_MPEGVisualSampleEntryBox*)entry_v, esd);
				if (e) return  e;
			} else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_HEVC) {
				entry_v = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_HVC1);
				if (!entry_v) return GF_OUT_OF_MEM;
				e = AVC_HEVC_UpdateESD((GF_MPEGVisualSampleEntryBox*)entry_v, esd);
				if (e) return  e;
			} else {
				entry_v = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MP4V);
				if (!entry_v) return GF_OUT_OF_MEM;
				entry_v->esd = (GF_ESDBox *) gf_isom_box_new_parent(&entry_v->child_boxes, GF_ISOM_BOX_TYPE_ESDS);
				entry_v->esd->desc = esd;
			}

			//type cast possible now
			entry = (GF_MPEGSampleEntryBox*) entry_v;
			break;
		case GF_ISOM_MEDIA_AUDIO:
			if (esd->decoderConfig->objectTypeIndication == GF_CODECID_OPUS) {
				GF_MPEGAudioSampleEntryBox *opus = (GF_MPEGAudioSampleEntryBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_OPUS);
				if (!opus) return GF_OUT_OF_MEM;
				opus->cfg_opus = (GF_OpusSpecificBox *)gf_isom_box_new_parent(&opus->child_boxes, GF_ISOM_BOX_TYPE_DOPS);
				entry = (GF_MPEGSampleEntryBox*)opus;
			} else if (esd->decoderConfig->objectTypeIndication == GF_CODECID_AC3) {
				GF_MPEGAudioSampleEntryBox *ac3 = (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_AC3);
				if (!ac3) return GF_OUT_OF_MEM;
				ac3->cfg_ac3 = (GF_AC3ConfigBox *) gf_isom_box_new_parent(&ac3->child_boxes, GF_ISOM_BOX_TYPE_DAC3);
				entry = (GF_MPEGSampleEntryBox*) ac3;
			} else if (esd->decoderConfig->objectTypeIndication==GF_CODECID_EAC3) {
				GF_MPEGAudioSampleEntryBox *eac3 = (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_EC3);
				if (!eac3) return GF_OUT_OF_MEM;
				eac3->cfg_ac3 = (GF_AC3ConfigBox *) gf_isom_box_new_parent(&eac3->child_boxes, GF_ISOM_BOX_TYPE_DEC3);
				entry = (GF_MPEGSampleEntryBox*) eac3;
			} else {
				entry_a = (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MP4A);
				if (!entry_a) return GF_OUT_OF_MEM;
				entry_a->samplerate_hi = trak->Media->mediaHeader->timeScale;
				entry_a->esd = (GF_ESDBox *) gf_isom_box_new_parent(&entry_a->child_boxes, GF_ISOM_BOX_TYPE_ESDS);
				entry_a->esd->desc = esd;
				//type cast possible now
				entry = (GF_MPEGSampleEntryBox*) entry_a;
			}
			break;
		default:
			if ((esd->decoderConfig->streamType==0x03) && (esd->decoderConfig->objectTypeIndication==0x09)) {
				entry = (GF_MPEGSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_LSR1);
				if (!entry) return GF_OUT_OF_MEM;
				e = LSR_UpdateESD((GF_LASeRSampleEntryBox*)entry, esd);
				if (e) return  e;
			} else {
				entry = (GF_MPEGSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MP4S);
				entry->esd = (GF_ESDBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_ESDS);
				entry->esd->desc = esd;
			}
			break;
		}
		entry->dataReferenceIndex = DataReferenceIndex;

		if (!trak->Media->information->sampleTable->SampleDescription->child_boxes)
			trak->Media->information->sampleTable->SampleDescription->child_boxes = gf_list_new();
		gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, entry);
		
		e = stsd_on_child_box((GF_Box*)trak->Media->information->sampleTable->SampleDescription, (GF_Box *) entry);
		if (e) return e;
		if(outStreamIndex) *outStreamIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	}
	return GF_OK;
}

#endif	/*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM*/
