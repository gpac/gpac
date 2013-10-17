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

#include <gpac/internal/isomedia_dev.h>
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_ISOM

GF_TrackBox *GetTrackbyID(GF_MovieBox *moov, u32 TrackID)
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
u32 gf_isom_get_tracknum_from_id(GF_MovieBox *moov, u32 trackID)
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
GF_Err GetESD(GF_MovieBox *moov, u32 trackID, u32 StreamDescIndex, GF_ESD **outESD)
{
	GF_Err e;
	GF_ESD *esd;
	u32 track_num = 0;
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak, *OCRTrack;
	GF_TrackReferenceTypeBox *dpnd;
	GF_SLConfig *slc;
	GF_MPEGSampleEntryBox *entry;

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
	
	//find stream dependencies
	e = Track_FindRef(trak, GF_ISOM_BOX_TYPE_DPND , &dpnd);
	if (e) return e;
	if (dpnd) {
		//ONLY ONE STREAM DEPENDENCY IS ALLOWED
		if (dpnd->trackIDCount != 1) return GF_ISOM_INVALID_MEDIA;
		//fix the spec: where is the index located ??
		esd->dependsOnESID = dpnd->trackIDs[0];
	} else {
		esd->dependsOnESID = 0;
	}

	if (trak->udta) {
		GF_UserDataMap *map;
		u32 i = 0;
		while ((map = (GF_UserDataMap*)gf_list_enum(trak->udta->recordList, &i))) {
			if (map->boxType == GF_4CC('A','U','X','V')) {
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
			if (esd->ESID == OCRTrack->Header->trackID) {
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

		esd->langDesc->langCode = trak->Media->mediaHeader->packedLanguage[0]; esd->langDesc->langCode <<= 8;
		esd->langDesc->langCode |= trak->Media->mediaHeader->packedLanguage[1]; esd->langDesc->langCode <<= 8;
		esd->langDesc->langCode |= trak->Media->mediaHeader->packedLanguage[2];
	}


	{
		u16 rvc_predefined;
		char *rvc_cfg_data;
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
		if (moov->mvex && (esd->decoderConfig->streamType==GF_STREAM_VISUAL)) {
			esd->slConfig->hasRandomAccessUnitsOnlyFlag = 0;
			esd->slConfig->useRandomAccessPointFlag = 1;
			stbl->SyncSample = (GF_SyncSampleBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSS);
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
GF_Err GetESDForTime(GF_MovieBox *moov, u32 trackID, u64 CTS, GF_ESD **outESD)
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
	while ((a = (GF_TrackReferenceTypeBox *)gf_list_enum(ref->other_boxes, &i))) {
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
	if (!trak->moov->mov->keep_utc)
		trak->Header->modificationTime = gf_isom_get_mp4time();
	return GF_OK;
}


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS

GF_Err MergeTrack(GF_TrackBox *trak, GF_TrackFragmentBox *traf, u64 moof_offset, Bool is_first_merge)
{
	u32 i, j, chunk_size;
	u64 base_offset, data_offset;
	u32 def_duration, DescIndex, def_size, def_flags;
	u32 duration, size, flags, cts_offset, prev_trun_data_offset;
	u8 pad, sync;
	u16 degr;
	GF_TrackFragmentRunBox *trun;
	GF_TrunEntry *ent;

	void stbl_AppendTime(GF_SampleTableBox *stbl, u32 duration);
	void stbl_AppendSize(GF_SampleTableBox *stbl, u32 size);
	void stbl_AppendChunk(GF_SampleTableBox *stbl, u64 offset);
	void stbl_AppendSampleToChunk(GF_SampleTableBox *stbl, u32 DescIndex, u32 samplesInChunk);
	void stbl_AppendCTSOffset(GF_SampleTableBox *stbl, u32 CTSOffset);
	void stbl_AppendRAP(GF_SampleTableBox *stbl, u8 isRap);
	void stbl_AppendPadding(GF_SampleTableBox *stbl, u8 padding);
	void stbl_AppendDegradation(GF_SampleTableBox *stbl, u16 DegradationPriority);

    if (trak->Header->trackID != traf->tfhd->trackID) return GF_OK;

	//setup all our defaults
	DescIndex = (traf->tfhd->flags & GF_ISOM_TRAF_SAMPLE_DESC) ? traf->tfhd->sample_desc_index : traf->trex->def_sample_desc_index;
	def_duration = (traf->tfhd->flags & GF_ISOM_TRAF_SAMPLE_DUR) ? traf->tfhd->def_sample_duration : traf->trex->def_sample_duration;
	def_size = (traf->tfhd->flags & GF_ISOM_TRAF_SAMPLE_SIZE) ? traf->tfhd->def_sample_size : traf->trex->def_sample_size;
	def_flags = (traf->tfhd->flags & GF_ISOM_TRAF_SAMPLE_FLAGS) ? traf->tfhd->def_sample_flags : traf->trex->def_sample_flags;

	//locate base offset
	base_offset = (traf->tfhd->flags & GF_ISOM_TRAF_BASE_OFFSET) ? traf->tfhd->base_data_offset : moof_offset;

	chunk_size = 0;
	prev_trun_data_offset = 0;

	/*in playback mode*/
	if (traf->tfdt && is_first_merge) {
#ifndef GPAC_DISABLE_LOG
		if (trak->present_in_scalable_segment && trak->sample_count_at_seg_start && (trak->dts_at_seg_start != traf->tfdt->baseMediaDecodeTime)) {
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

	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		//merge the run
		for (j=0;j<trun->sample_count; j++) {
			ent = (GF_TrunEntry*)gf_list_get(trun->entries, j);
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
			stbl_AppendSize(trak->Media->information->sampleTable, size);
			//then TS
			stbl_AppendTime(trak->Media->information->sampleTable, duration);
			//add chunk on first sample
			if (!j) {
				data_offset = base_offset;
				//aggregated offset if base-data-offset-present is not set AND if default-base-is-moof is not set
				if (!(traf->tfhd->flags & GF_ISOM_TRAF_BASE_OFFSET) && !(traf->tfhd->flags & GF_ISOM_MOOF_BASE_OFFSET) ) 
					data_offset += chunk_size;

				if (trun->flags & GF_ISOM_TRUN_DATA_OFFSET) {
					data_offset += trun->data_offset;
					/*reset chunk size since data is now relative to this trun*/
					chunk_size = 0;
					/*remember this data offset for following trun*/
					prev_trun_data_offset = trun->data_offset;
				} else {
					/*data offset is previous chunk size plus previous offset of the trun*/
					data_offset += prev_trun_data_offset;
				}
				stbl_AppendChunk(trak->Media->information->sampleTable, data_offset);
				//then sampleToChunk
				stbl_AppendSampleToChunk(trak->Media->information->sampleTable, 
					DescIndex, trun->sample_count);
			}
			chunk_size += size;
			

			//CTS
			cts_offset = (trun->flags & GF_ISOM_TRUN_CTS_OFFSET) ? ent->CTS_Offset : 0;
			stbl_AppendCTSOffset(trak->Media->information->sampleTable, cts_offset);
			
			//flags
			sync = GF_ISOM_GET_FRAG_SYNC(flags);
			stbl_AppendRAP(trak->Media->information->sampleTable, sync);
			pad = GF_ISOM_GET_FRAG_PAD(flags);
			if (pad) stbl_AppendPadding(trak->Media->information->sampleTable, pad);
			degr = GF_ISOM_GET_FRAG_DEG(flags);
			if (degr) stbl_AppendDegradation(trak->Media->information->sampleTable, degr);
		}
	}
	/*merge sample groups*/
	if (traf->sampleGroups) {
		GF_List *groups;
		GF_List *groupDescs;
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
				gf_list_rem(traf->sampleGroupsDescription, i);
				i--;
			}
			/*merge descriptions*/
			else {
				u32 idx = gf_list_count(new_sgdesc->group_descriptions);
				for (j=idx; j<gf_list_count(sgdesc->group_descriptions); j++) {
					void *ptr = gf_list_get(sgdesc->group_descriptions, j);
					if (ptr) {
						gf_list_add(new_sgdesc->group_descriptions, ptr);
						gf_list_rem(sgdesc->group_descriptions, j);
						j--;
					}
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
				stbl_group = (GF_SampleGroupBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SBGP);
				stbl_group->grouping_type = frag_group->grouping_type;
				stbl_group->grouping_type_parameter = frag_group->grouping_type_parameter;
				stbl_group->version = frag_group->version;
				gf_list_add(groups, stbl_group);
			}
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
		}
	}
	return GF_OK;
}

#endif


#ifndef GPAC_DISABLE_ISOM_WRITE

//used to check if a TrackID is available
u8 RequestTrack(GF_MovieBox *moov, u32 TrackID)
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
	while ((a = (GF_Box *)gf_list_enum(ref->other_boxes, &i))) {
		if (a->type == ReferenceType) {
			gf_isom_box_del(a);
			gf_list_rem(ref->other_boxes, i-1);
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
	char *str;

	GF_Err e;

	if (*mdia || !mdia) return GF_BAD_PARAM;

	e = GF_OK;
	minf = NULL;
	mdhd = NULL;
	hdlr = NULL;
	dinf = NULL;
	stbl = NULL;
	dref = NULL;
	mediaInfo = NULL;

	//first create the media
	*mdia = (GF_MediaBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MDIA);
	mdhd = (GF_MediaHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MDHD);

	//"handler name" is for debugging purposes. Let's stick our name here ;)
	switch (MediaType) {
	case GF_ISOM_MEDIA_VISUAL:
		mediaInfo = gf_isom_box_new(GF_ISOM_BOX_TYPE_VMHD);
		str = "GPAC ISO Video Handler";
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
	hdlr = (GF_HandlerBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_HDLR);
	minf = (GF_MediaInformationBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MINF);

	mdhd->timeScale = TimeScale;
	hdlr->handlerType = MediaType;
	hdlr->nameUTF8 = gf_strdup(str);

	//first set-up the sample table...
	stbl = (GF_SampleTableBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STBL);
	dinf = (GF_DataInformationBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_DINF);
	stbl->SampleDescription = (GF_SampleDescriptionBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSD);
	stbl->ChunkOffset = gf_isom_box_new(GF_ISOM_BOX_TYPE_STCO);
	//by default create a regular table
	stbl->SampleSize = (GF_SampleSizeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSZ);
	stbl->SampleToChunk = (GF_SampleToChunkBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSC);
	stbl->TimeToSample = (GF_TimeToSampleBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STTS);

	//Create a data reference WITHOUT DATA ENTRY (we don't know anything yet about the media Data)
	dref = (GF_DataReferenceBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_DREF);
	e = dinf_AddBox((GF_Box*)dinf, (GF_Box *)dref); if (e) goto err_exit;

	e = minf_AddBox((GF_Box*)minf, (GF_Box *) mediaInfo); if (e) goto err_exit;
	e = minf_AddBox((GF_Box*)minf, (GF_Box *) stbl); if (e) goto err_exit;
	e = minf_AddBox((GF_Box*)minf, (GF_Box *) dinf); if (e) goto err_exit;

	e = mdia_AddBox((GF_Box*)*mdia, (GF_Box *) mdhd); if (e) goto err_exit;
	e = mdia_AddBox((GF_Box*)*mdia, (GF_Box *) minf); if (e) goto err_exit;
	e = mdia_AddBox((GF_Box*)*mdia, (GF_Box *) hdlr); if (e) goto err_exit;

	return GF_OK;

err_exit:
	if (mdhd) gf_isom_box_del((GF_Box *)mdhd);
	if (minf) gf_isom_box_del((GF_Box *)minf);
	if (hdlr) {
		if (hdlr->nameUTF8) gf_free(hdlr->nameUTF8);
		gf_isom_box_del((GF_Box *)hdlr);
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
	if (esd->dependsOnESID  || esd->OCRESID ) {
		if (!trak->References) {
			tref = (GF_TrackReferenceBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TREF);
			e = trak_AddBox((GF_Box*)trak, (GF_Box *)tref);
			if (e) return e;
		}
		tref = trak->References;
	}

	//Update Stream dependancies
	e = Track_FindRef(trak, GF_ISOM_REF_DECODE, &dpnd);
	if (e) return e;
	if (!dpnd && esd->dependsOnESID) {
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_REFT);
		dpnd->reference_type = GF_ISOM_BOX_TYPE_DPND;
		e = tref_AddBox((GF_Box*)tref, (GF_Box *) dpnd);
		if (e) return e;
		e = reftype_AddRefTrack(dpnd, esd->dependsOnESID, NULL);
		if (e) return e;
	} else if (dpnd && !esd->dependsOnESID) {
		Track_RemoveRef(trak, GF_ISOM_BOX_TYPE_DPND);
	}
	esd->dependsOnESID = 0;

	//Update GF_Clock dependancies
	e = Track_FindRef(trak, GF_ISOM_REF_OCR, &dpnd);
	if (e) return e;
	if (!dpnd && esd->OCRESID) {
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_REFT);
		dpnd->reference_type = GF_ISOM_BOX_TYPE_SYNC;
		e = tref_AddBox((GF_Box*)tref, (GF_Box *) dpnd);
		if (e) return e;
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
			dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_REFT);
			dpnd->reference_type = GF_ISOM_BOX_TYPE_IPIR;
			e = tref_AddBox((GF_Box*)tref, (GF_Box *) dpnd);
			if (e) return e;
			e = reftype_AddRefTrack(dpnd, esd->ipiPtr->IPI_ES_Id, &tmpRef);
			if (e) return e;
			//and replace the tag and value...
			esd->ipiPtr->IPI_ES_Id = tmpRef;
			esd->ipiPtr->tag = GF_ODF_ISOM_IPI_PTR_TAG;
		} else {
			//Watch out! ONLY ONE IPI dependancy is allowed per stream
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
		entry = (GF_MPEGSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, StreamDescriptionIndex - 1);
		if (!entry) return GF_ISOM_INVALID_FILE;

		switch (entry->type) {
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
			//OK, delete the previous ESD
			gf_odf_desc_del((GF_Descriptor *) entry_a->esd->desc);
			entry_a->esd->desc = esd;
			break;
		case GF_ISOM_BOX_TYPE_AVC1:
		case GF_ISOM_BOX_TYPE_AVC2:
		case GF_ISOM_BOX_TYPE_AVC3:
		case GF_ISOM_BOX_TYPE_AVC4:
		case GF_ISOM_BOX_TYPE_SVC1:
		case GF_ISOM_BOX_TYPE_HVC1: 
		case GF_ISOM_BOX_TYPE_HEV1: 
			e = AVC_HEVC_UpdateESD((GF_MPEGVisualSampleEntryBox*)entry, esd);
			if (e) return e;
			break;
		case GF_ISOM_BOX_TYPE_LSR1:
			e = LSR_UpdateESD((GF_LASeRSampleEntryBox*)entry, esd);
			if (e) return e;
			break;
		case GF_ISOM_BOX_TYPE_STSE:
		case GF_ISOM_BOX_TYPE_WVTT:
            /* TODO */
            assert(0);
			break;
			
		default:
			//gf_odf_desc_del((GF_Descriptor *) esd);
			break;
		}
	} else {
		//need to check we're not in URL mode where only ONE description is allowed...
		StreamDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);
		if (StreamDescriptionIndex) {
			entry = (GF_MPEGSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, StreamDescriptionIndex - 1);
			if (!entry) return GF_ISOM_INVALID_FILE;
			if (entry->esd && entry->esd->desc->URLString) return GF_BAD_PARAM;
		}

		//OK, check the handler and create the entry
		switch (trak->Media->handler->handlerType) {
		case GF_ISOM_MEDIA_VISUAL:
			if ((esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_AVC) || (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_SVC)){
				entry_v = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_AVC1);
				if (!entry_v) return GF_OUT_OF_MEM;
				e = AVC_HEVC_UpdateESD((GF_MPEGVisualSampleEntryBox*)entry_v, esd);
			} else if (esd->decoderConfig->objectTypeIndication==GPAC_OTI_VIDEO_HEVC) {
				entry_v = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_HVC1);
				if (!entry_v) return GF_OUT_OF_MEM;
				e = AVC_HEVC_UpdateESD((GF_MPEGVisualSampleEntryBox*)entry_v, esd);
				
			} else {
				entry_v = (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MP4V);
				if (!entry_v) return GF_OUT_OF_MEM;
				entry_v->esd = (GF_ESDBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_ESDS);
				entry_v->esd->desc = esd;
			}

			//type cast possible now
			entry = (GF_MPEGSampleEntryBox*) entry_v;
			break;
		case GF_ISOM_MEDIA_AUDIO:
			entry_a = (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MP4A);
			entry_a->samplerate_hi = trak->Media->mediaHeader->timeScale;
			if (!entry_a) return GF_OUT_OF_MEM;
			entry_a->esd = (GF_ESDBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_ESDS);
			entry_a->esd->desc = esd;
			//type cast possible now
			entry = (GF_MPEGSampleEntryBox*) entry_a;
			break;
		default:
			if ((esd->decoderConfig->streamType==0x03) && (esd->decoderConfig->objectTypeIndication==0x09)) {
				entry = (GF_MPEGSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_LSR1);
				if (!entry) return GF_OUT_OF_MEM;
				e = LSR_UpdateESD((GF_LASeRSampleEntryBox*)entry, esd);
			} else {
				entry = (GF_MPEGSampleEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MP4S);
				entry->esd = (GF_ESDBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_ESDS);
				entry->esd->desc = esd;
			}
			break;
		}
		entry->dataReferenceIndex = DataReferenceIndex;

		//and add the entry to our table...
		e = stsd_AddBox(trak->Media->information->sampleTable->SampleDescription, (GF_Box *) entry);
		if (e) return e;
		if(outStreamIndex) *outStreamIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);
	}
	return GF_OK;
}

#endif	/*GPAC_DISABLE_ISOM_WRITE*/

#endif /*GPAC_DISABLE_ISOM*/
