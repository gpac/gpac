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

#ifndef GPAC_DISABLE_ISOM

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

GF_TrackExtendsBox *GetTrex(GF_MovieBox *moov, u32 TrackID)
{
	u32 i;
	GF_TrackExtendsBox *trex;
	i=0;
	while ((trex = (GF_TrackExtendsBox *)gf_list_enum(moov->mvex->TrackExList, &i))) {
		if (trex->trackID == TrackID) return trex;
	}
	return NULL;
}

GF_TrackExtensionPropertiesBox *GetTrep(GF_MovieBox *moov, u32 TrackID)
{
	u32 i;
	GF_TrackExtensionPropertiesBox *trep;
	i=0;
	while ((trep = (GF_TrackExtensionPropertiesBox*) gf_list_enum(moov->mvex->TrackExPropList, &i))) {
		if (trep->trackID == TrackID) return trep;
	}
	return NULL;
}

GF_TrackFragmentBox *GetTraf(GF_ISOFile *mov, u32 TrackID)
{
	u32 i;
	GF_TrackFragmentBox *traf;
	if (!mov->moof) return NULL;

	//reverse browse the TRAFs, as there may be more than one per track ...
	for (i=gf_list_count(mov->moof->TrackList); i>0; i--) {
		traf = (GF_TrackFragmentBox *)gf_list_get(mov->moof->TrackList, i-1);
		if (traf->tfhd->trackID == TrackID) return traf;
	}
	return NULL;
}


#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err gf_isom_set_movie_duration(GF_ISOFile *movie, u64 duration)
{
	if (!movie->moov->mvex) return GF_BAD_PARAM;
	if (!movie->moov->mvex->mehd) {
		movie->moov->mvex->mehd = (GF_MovieExtendsHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MEHD);
	}
	movie->moov->mvex->mehd->fragment_duration = duration;
	movie->moov->mvhd->duration = 0;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_finalize_for_fragment(GF_ISOFile *movie, u32 media_segment_type)
{
	GF_Err e;
	u32 i;
	Bool store_file = 1;
	GF_TrackExtendsBox *trex;
	if (!movie || !movie->moov) return GF_BAD_PARAM;

	if (movie->openMode==GF_ISOM_OPEN_CAT_FRAGMENTS) {
		/*from now on we are in write mode*/
		movie->openMode = GF_ISOM_OPEN_WRITE;
		store_file = 0;
		movie->append_segment = 1;
	} else {
		movie->NextMoofNumber = 1;
	}

	//this is only allowed in write mode
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	if (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) return GF_OK;
	movie->FragmentsFlags = 0;

	if (store_file) {
		/*"dash" brand: this is a DASH Initialization Segment*/
		gf_isom_modify_alternate_brand(movie, GF_4CC('d','a','s','h'), 1);

		if (!movie->moov->mvex->mehd || !movie->moov->mvex->mehd->fragment_duration) {
			//update durations
			gf_isom_get_duration(movie);
		}

		i=0;
		while ((trex = (GF_TrackExtendsBox *)gf_list_enum(movie->moov->mvex->TrackExList, &i))) {
			GF_TrackExtensionPropertiesBox *trep;
			if (trex->type != GF_ISOM_BOX_TYPE_TREX) continue;
			trep = GetTrep(movie->moov, trex->trackID);

			if (!trep) {
				trep = (GF_TrackExtensionPropertiesBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_TREP);
				trep->trackID = trex->trackID;
				gf_list_add(movie->moov->mvex->TrackExPropList, trep);
			}

			if (trex->track->Media->information->sampleTable->CompositionToDecode) {

				if (!trex->track->Media->information->sampleTable->SampleSize || ! trex->track->Media->information->sampleTable->SampleSize->sampleCount) {
					gf_list_add(trep->other_boxes, trex->track->Media->information->sampleTable->CompositionToDecode);
					trex->track->Media->information->sampleTable->CompositionToDecode = NULL;
				} else {
					GF_CompositionToDecodeBox *cslg;

					//clone it!
					GF_SAFEALLOC(cslg, GF_CompositionToDecodeBox);
					memcpy(cslg, trex->track->Media->information->sampleTable->CompositionToDecode, sizeof(GF_CompositionToDecodeBox) );
					cslg->other_boxes = gf_list_new();
					gf_list_add(trep->other_boxes, trex->track->Media->information->sampleTable->CompositionToDecode);
				}
			}

			if (movie->moov->mvex->mehd && movie->moov->mvex->mehd->fragment_duration) {
				trex->track->Header->duration = 0;
				Media_SetDuration(trex->track);
			}
		}

		//write movie
		e = WriteToFile(movie);
		if (e) return e;
	}

	//make sure we do have all we need. If not this is not an error, just consider
	//the file closed
	if (!movie->moov->mvex || !gf_list_count(movie->moov->mvex->TrackExList)) return GF_OK;

	i=0;
	while ((trex = (GF_TrackExtendsBox *)gf_list_enum(movie->moov->mvex->TrackExList, &i))) {
		if (!trex->trackID || !gf_isom_get_track_from_id(movie->moov, trex->trackID)) return GF_IO_ERR;
		//we could also check all our data refs are local but we'll do that at run time
		//in order to allow a mix of both (remote refs in MOOV and local in MVEX)

		//one thing that MUST be done is OD cross-dependancies. The movie fragment spec
		//is broken here, since it cannot allow dynamic insertion of new ESD and their
		//dependancies
	}

	//ok we are fine - note the data map is created at the begining
	if (i) movie->FragmentsFlags |= GF_ISOM_FRAG_WRITE_READY;

	if (media_segment_type) {
		movie->use_segments = 1;
		movie->moof_list = gf_list_new();
	}

	/*set brands for segment*/

	/*"msdh": it's a media segment */
	gf_isom_set_brand_info(movie, GF_4CC('m','s','d','h'), 0);
	/*remove all brands	*/
	gf_isom_reset_alt_brands(movie);
	/*
		msdh: it's a media segment
		sims: it's a media segment with an SSIX
		msix: it's a media segment with an index
		lmsg: it's the last media segment
	*/

	return GF_OK;
}

GF_Err gf_isom_change_track_fragment_defaults(GF_ISOFile *movie, u32 TrackID,
        u32 DefaultSampleDescriptionIndex,
        u32 DefaultSampleDuration,
        u32 DefaultSampleSize,
        u8 DefaultSampleIsSync,
        u8 DefaultSamplePadding,
        u16 DefaultDegradationPriority)
{
	GF_MovieExtendsBox *mvex;
	GF_TrackExtendsBox *trex;
	GF_TrackBox *trak;

	if (!movie || !movie->moov) return GF_BAD_PARAM;
	//this is only allowed in write mode
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	trak = gf_isom_get_track_from_id(movie->moov, TrackID);
	if (!trak) return GF_BAD_PARAM;

	mvex = movie->moov->mvex;
	if (!mvex) return GF_BAD_PARAM;

	trex = GetTrex(movie->moov, TrackID);
	if (!trex)  return GF_BAD_PARAM;

	trex->def_sample_desc_index = DefaultSampleDescriptionIndex;
	trex->def_sample_duration = DefaultSampleDuration;
	trex->def_sample_size = DefaultSampleSize;
	trex->def_sample_flags = GF_ISOM_FORMAT_FRAG_FLAGS(DefaultSamplePadding, DefaultSampleIsSync, DefaultDegradationPriority);

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_setup_track_fragment(GF_ISOFile *movie, u32 TrackID,
                                    u32 DefaultSampleDescriptionIndex,
                                    u32 DefaultSampleDuration,
                                    u32 DefaultSampleSize,
                                    u8 DefaultSampleIsSync,
                                    u8 DefaultSamplePadding,
                                    u16 DefaultDegradationPriority)
{
	GF_MovieExtendsBox *mvex;
	GF_TrackExtendsBox *trex;
	GF_TrackBox *trak;

	if (!movie || !movie->moov) return GF_BAD_PARAM;
	//this is only allowed in write mode
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;
	//and only at setup
	if (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) return GF_BAD_PARAM;


	trak = gf_isom_get_track_from_id(movie->moov, TrackID);
	if (!trak) return GF_BAD_PARAM;

	//create MVEX if needed
	if (!movie->moov->mvex) {
		mvex = (GF_MovieExtendsBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MVEX);
		moov_AddBox((GF_Box*)movie->moov, (GF_Box *) mvex);
	} else {
		mvex = movie->moov->mvex;
	}
	if (!mvex->mehd) {
		mvex->mehd = (GF_MovieExtendsHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MEHD);
	}

	trex = GetTrex(movie->moov, TrackID);
	if (!trex) {
		trex = (GF_TrackExtendsBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TREX);
		trex->trackID = TrackID;
		mvex_AddBox((GF_Box*)mvex, (GF_Box *) trex);
	}
	trex->track = trak;
	return gf_isom_change_track_fragment_defaults(movie, TrackID, DefaultSampleDescriptionIndex, DefaultSampleDuration, DefaultSampleSize, DefaultSampleIsSync, DefaultSamplePadding, DefaultDegradationPriority);
}


u32 GetNumUsedValues(GF_TrackFragmentBox *traf, u32 value, u32 index)
{
	u32 i, j, NumValue = 0;
	GF_TrackFragmentRunBox *trun;
	GF_TrunEntry *ent;

	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		j=0;
		while ((ent = (GF_TrunEntry *)gf_list_enum(trun->entries, &j))) {
			switch (index) {
			case 1:
				if (value == ent->Duration) NumValue ++;
				break;
			case 2:
				if (value == ent->size) NumValue ++;
				break;
			case 3:
				if (value == ent->flags) NumValue ++;
				break;
			}
		}
	}
	return NumValue;
}


void ComputeFragmentDefaults(GF_TrackFragmentBox *traf)
{
	u32 i, j, MaxNum, DefValue, ret;
	GF_TrackFragmentRunBox *trun;
	GF_TrunEntry *ent;

	//Duration default
	MaxNum = DefValue = 0;
	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		j=0;
		while ((ent = (GF_TrunEntry *)gf_list_enum(trun->entries, &j))) {
			ret = GetNumUsedValues(traf, ent->Duration, 1);
			if (ret>MaxNum) {
				//at least 2 duration, specify for all
				if (MaxNum) {
					DefValue = 0;
					goto escape_duration;
				}
				MaxNum = ret;
				DefValue = ent->Duration;
			}
		}
	}
escape_duration:
	//store if #
	if (DefValue && (DefValue != traf->trex->def_sample_duration)) {
		traf->tfhd->def_sample_duration = DefValue;
	}

	//Size default
	MaxNum = DefValue = 0;
	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		j=0;
		while ((ent = (GF_TrunEntry*)gf_list_enum(trun->entries, &j))) {
			ret = GetNumUsedValues(traf, ent->size, 2);
			if (ret>MaxNum || (ret==1)) {
				//at least 2 sizes so we must specify all sizes
				if (MaxNum) {
					DefValue = 0;
					goto escape_size;
				}
				MaxNum = ret;
				DefValue = ent->size;
			}
		}
	}

escape_size:
	//store if #
	if (DefValue && (DefValue != traf->trex->def_sample_size)) {
		traf->tfhd->def_sample_size = DefValue;
	}

	//Flags default
	MaxNum = DefValue = 0;
	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		j=0;
		while ((ent = (GF_TrunEntry*)gf_list_enum(trun->entries, &j))) {
			ret = GetNumUsedValues(traf, ent->flags, 3);
			if (ret>MaxNum) {
				MaxNum = ret;
				DefValue = ent->flags;
			}
		}
	}
	//store if #
	if (DefValue && (DefValue != traf->trex->def_sample_flags)) {
		traf->tfhd->def_sample_flags = DefValue;
	}
}


GF_Err gf_isom_set_fragment_option(GF_ISOFile *movie, u32 TrackID, u32 Code, u32 Param)
{
	GF_TrackFragmentBox *traf;
	if (!movie || !movie->moov) return GF_BAD_PARAM;
	//this is only allowed in write mode
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	traf = GetTraf(movie, TrackID);
	if (!traf) return GF_BAD_PARAM;
	switch (Code) {
	case GF_ISOM_TRAF_EMPTY:
		traf->tfhd->EmptyDuration = Param;
		break;
	case GF_ISOM_TRAF_RANDOM_ACCESS:
		traf->tfhd->IFrameSwitching = Param;
		break;
	case GF_ISOM_TRAF_DATA_CACHE:
		//don't cache only one sample ...
		traf->DataCache = Param > 1 ? Param : 0;
		break;
	}
	return GF_OK;
}

//#define USE_BASE_DATA_OFFSET

void update_trun_offsets(GF_ISOFile *movie, s32 offset)
{
#ifndef USE_BASE_DATA_OFFSET
	GF_TrackFragmentRunBox *trun;
	u32 i, j;
	GF_TrackFragmentBox *traf;
	i=0;
	while ((traf = gf_list_enum(movie->moof->TrackList, &i))) {
		/*remove base data*/
		traf->tfhd->base_data_offset = 0;
		j=0;
		while ((trun =  gf_list_enum(traf->TrackRuns, &j))) {
			if (j==1) {
				trun->data_offset += offset;
			} else {
				trun->data_offset = 0;
			}
		}
	}
#endif
}

u32 UpdateRuns(GF_ISOFile *movie, GF_TrackFragmentBox *traf)
{
	u32 sampleCount, i, j, RunSize, UseDefaultSize, RunDur, UseDefaultDur, RunFlags, NeedFlags, UseDefaultFlag, UseCTS, count;
	GF_TrackFragmentRunBox *trun;
	GF_TrunEntry *ent, *first_ent;

	sampleCount = 0;

#ifndef USE_BASE_DATA_OFFSET
	if (movie->use_segments) {
		traf->tfhd->flags = GF_ISOM_MOOF_BASE_OFFSET;
	} else
#endif
	{
		traf->tfhd->flags = GF_ISOM_TRAF_BASE_OFFSET;
	}

	//empty runs
	if (traf->tfhd->EmptyDuration) {
		while (gf_list_count(traf->TrackRuns)) {
			trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, 0);
			gf_list_rem(traf->TrackRuns, 0);
			gf_isom_box_del((GF_Box *)trun);
		}
		traf->tfhd->flags = GF_ISOM_TRAF_DUR_EMPTY;
		if (traf->tfhd->EmptyDuration != traf->trex->def_sample_duration) {
			traf->tfhd->def_sample_duration = traf->tfhd->EmptyDuration;
			traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_DUR;
		}
		return 0;
	}


	UseDefaultSize = 0;
	UseDefaultDur = 0;
	UseDefaultFlag = 0;

	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		RunSize = 0;
		RunDur = 0;
		RunFlags = 0;
		UseCTS = 0;
		NeedFlags = 0;

		first_ent = NULL;
		//process all samples in run
		count = gf_list_count(trun->entries);
		for (j=0; j<count; j++) {
			ent = (GF_TrunEntry*)gf_list_get(trun->entries, j);
			if (!j) {
				first_ent = ent;
				RunSize = ent->size;
				RunDur = ent->Duration;
			}
			//we may have one entry only ...
			if (j || (count==1)) {
				//flags are only after first entry
				if (j==1 || (count==1) ) RunFlags = ent->flags;

				if (ent->size != RunSize) RunSize = 0;
				if (ent->Duration != RunDur) RunDur = 0;
				if (j && (RunFlags != ent->flags)) NeedFlags = 1;
			}
			if (ent->CTS_Offset) UseCTS = 1;
		}
		//empty list
		if (!first_ent) {
			i--;
			gf_list_rem(traf->TrackRuns, i);
			continue;
		}
		trun->sample_count = gf_list_count(trun->entries);
		trun->flags = 0;

		//size checking
		//constant size, check if this is from current fragment default or global default
		if (RunSize && (traf->trex->def_sample_size == RunSize)) {
			if (!UseDefaultSize) UseDefaultSize = 2;
			else if (UseDefaultSize==1) RunSize = 0;
		} else if (RunSize && (traf->tfhd->def_sample_size == RunSize)) {
			if (!UseDefaultSize) UseDefaultSize = 1;
			else if (UseDefaultSize==2) RunSize = 0;
		}
		//we could check for single entry runs and set the default size in the tfhd but
		//that's no bit saving...
		else {
			RunSize=0;
		}

		if (!RunSize) trun->flags |= GF_ISOM_TRUN_SIZE;

		//duration checking
		if (RunDur && (traf->trex->def_sample_duration == RunDur)) {
			if (!UseDefaultDur) UseDefaultDur = 2;
			else if (UseDefaultDur==1) RunDur = 0;
		} else if (RunDur && (traf->tfhd->def_sample_duration == RunDur)) {
			if (!UseDefaultDur) UseDefaultDur = 1;
			else if (UseDefaultDur==2) RunDur = 0;
		}
		if (!RunDur) trun->flags |= GF_ISOM_TRUN_DURATION;

		//flag checking
		if (!NeedFlags) {
			if (RunFlags == traf->trex->def_sample_flags) {
				if (!UseDefaultFlag) UseDefaultFlag = 2;
				else if (UseDefaultFlag==1) NeedFlags = 1;
			} else if (RunFlags == traf->tfhd->def_sample_flags) {
				if (!UseDefaultFlag) UseDefaultFlag = 1;
				else if(UseDefaultFlag==2) NeedFlags = 1;
			}
		}
		if (NeedFlags) {
			//one flags entry per sample only
			trun->flags |= GF_ISOM_TRUN_FLAGS;
		} else {
			//indicated in global setup
			if (first_ent->flags == traf->trex->def_sample_flags) {
				if (!UseDefaultFlag) UseDefaultFlag = 2;
				else if (UseDefaultFlag==1) trun->flags |= GF_ISOM_TRUN_FIRST_FLAG;
			}
			//indicated in local setup
			else if (first_ent->flags == traf->tfhd->def_sample_flags) {
				if (!UseDefaultFlag) UseDefaultFlag = 1;
				else if (UseDefaultFlag==2) trun->flags |= GF_ISOM_TRUN_FIRST_FLAG;
			}
			//explicit store
			else {
				trun->flags |= GF_ISOM_TRUN_FIRST_FLAG;
			}
		}

		//CTS flag
		if (UseCTS) trun->flags |= GF_ISOM_TRUN_CTS_OFFSET;

		//run data offset if the offset indicated is 0 (first sample in this MDAT) don't
		//indicate it
		if (trun->data_offset) trun->flags |= GF_ISOM_TRUN_DATA_OFFSET;

		sampleCount += trun->sample_count;
	}

	//last update TRAF flags
	if (UseDefaultSize==1) traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_SIZE;
	if (UseDefaultDur==1) traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_DUR;
	if (UseDefaultFlag==1) traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_FLAGS;
	if (traf->tfhd->sample_desc_index && traf->tfhd->sample_desc_index != traf->trex->def_sample_desc_index) traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_DESC;


	return sampleCount;
}

static u32 moof_get_sap_info(GF_MovieFragmentBox *moof, u32 refTrackID, u32 *sap_delta, Bool *starts_with_sap)
{
	u32 i, j, count, delta, earliest_cts, sap_type, sap_sample_num, cur_sample;
	Bool first = 1;
	GF_TrunEntry *ent;
	GF_TrackFragmentBox *traf=NULL;
	GF_TrackFragmentRunBox *trun;
	sap_type = 0;
	*sap_delta = 0;
	*starts_with_sap = GF_FALSE;
	for (i=0; i<gf_list_count(moof->TrackList); i++) {
		traf = gf_list_get(moof->TrackList, i);
		if (traf->tfhd->trackID==refTrackID) break;
		traf=NULL;
	}
	if (!traf) return sap_type;
	earliest_cts = 0;

	/*first check if we have a roll/rap sample in this traf, and mark its sample count*/
	sap_type = 0;
	sap_sample_num = 0;
	/*check RAP and ROLL*/
	count = traf->sampleGroups ? gf_list_count(traf->sampleGroups) : 0;
	for (i=0; i<count; i++) {
		GF_SampleGroupBox *sg;
		u32 j, first_sample;
		Bool rap_type = GF_FALSE;
		sg = gf_list_get(traf->sampleGroups, i);

		switch (sg->grouping_type) {
		case GF_4CC('r','a','p',' '):
			rap_type = GF_TRUE;
			break;
		case GF_4CC('r','o','l','l'):
			break;
		default:
			continue;
		}
		/*first entry is SAP*/
		first_sample = 1;
		for (j=0; j<sg->entry_count; j++) {
			if (! sg->sample_entries[j].group_description_index) {
				first_sample += sg->sample_entries[j].sample_count;
				continue;
			}
			if (!j) {
				*starts_with_sap = GF_TRUE;
				sap_sample_num = 0;
			}
			if (!sap_sample_num || (sap_sample_num>first_sample)) {
				sap_type = rap_type ? 3 : 4;
				sap_sample_num = first_sample;
			}
			break;
		}
	}

	/*then browse all samples, looking for SYNC flag or sap_sample_num*/
	cur_sample = 1;
	delta = 0;
	i=0;
	while ((trun = gf_list_enum(traf->TrackRuns, &i))) {
		if (trun->flags & GF_ISOM_TRUN_FIRST_FLAG) {
			if (GF_ISOM_GET_FRAG_SYNC(trun->flags)) {
				ent = gf_list_get(trun->entries, 0);
				if (!delta) earliest_cts = ent->CTS_Offset;
				*sap_delta = delta + ent->CTS_Offset - ent->CTS_Offset;
				*starts_with_sap = first;
				sap_type = ent->SAP_type;
				return sap_type;
			}
		}
		j=0;
		while ((ent = gf_list_enum(trun->entries, &j))) {
			if (!delta) earliest_cts = ent->CTS_Offset;

			if (GF_ISOM_GET_FRAG_SYNC(ent->flags)) {
				*sap_delta = delta + ent->CTS_Offset - earliest_cts;
				*starts_with_sap = first;
				sap_type = ent->SAP_type;
				return sap_type;
			}
			/*we found our roll or rap sample*/
			if (cur_sample==sap_sample_num) {
				*sap_delta = delta + ent->CTS_Offset - earliest_cts;
				return sap_type;
			}
			delta += ent->Duration;
			first = 0;
			cur_sample++;
		}
	}
	/*not found*/
	return 0;
}

u32 moof_get_duration(GF_MovieFragmentBox *moof, u32 refTrackID)
{
	u32 i, j, duration;
	GF_TrunEntry *ent;
	GF_TrackFragmentBox *traf = NULL;
	GF_TrackFragmentRunBox *trun;
	for (i=0; i<gf_list_count(moof->TrackList); i++) {
		traf = gf_list_get(moof->TrackList, i);
		if (traf->tfhd->trackID==refTrackID) break;
		traf=NULL;
	}
	if (!traf) return 0;

	duration = 0;
	i=0;
	while ((trun = gf_list_enum(traf->TrackRuns, &i))) {
		j=0;
		while ((ent = gf_list_enum(trun->entries, &j))) {
			if (ent->flags & GF_ISOM_TRAF_SAMPLE_DUR)
				duration += ent->Duration;
			else
				duration += traf->trex->def_sample_duration;
		}
	}
	return duration;
}

static u32 moof_get_earliest_cts(GF_MovieFragmentBox *moof, u32 refTrackID)
{
	u32 i, j, cts, duration;
	GF_TrunEntry *ent;
	GF_TrackFragmentBox *traf=NULL;
	GF_TrackFragmentRunBox *trun;
	for (i=0; i<gf_list_count(moof->TrackList); i++) {
		traf = gf_list_get(moof->TrackList, i);
		if (traf->tfhd->trackID==refTrackID) break;
		traf=NULL;
	}
	if (!traf) return 0;

	duration = 0;
	cts = 0xFFFFFFFF;
	i=0;
	while ((trun = gf_list_enum(traf->TrackRuns, &i))) {
		j=0;
		while ((ent = gf_list_enum(trun->entries, &j))) {
			if (duration + ent->CTS_Offset < cts)
				cts = duration + ent->CTS_Offset;
			duration += ent->Duration;
		}
	}
	return cts;
}

GF_Err StoreFragment(GF_ISOFile *movie, Bool load_mdat_only, s32 data_offset_diff, u32 *moof_size)
{
	GF_Err e;
	u64 moof_start, pos;
	u32 size, i, s_count, mdat_size;
	s32 offset;
	char *buffer;
	GF_TrackFragmentBox *traf;
	GF_TrackFragmentRunBox *trun;
	GF_BitStream *bs;
	if (!movie->moof) return GF_OK;

	bs = movie->editFileMap->bs;
	if (!movie->moof_first) load_mdat_only = 0;
	mdat_size = 0;
	//1 - flush all caches
	i=0;
	while ((traf = (GF_TrackFragmentBox*)gf_list_enum(movie->moof->TrackList, &i))) {
		if (!traf->DataCache) continue;
		s_count = gf_list_count(traf->TrackRuns);
		if (!s_count) continue;
		trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, s_count-1);
		if (!trun->cache || !trun->sample_count) continue;

		//update offset
		trun->data_offset = (u32) (gf_bs_get_position(bs) - movie->moof->fragment_offset - 8);
		//write cache
		gf_bs_get_content(trun->cache, &buffer, &size);
		gf_bs_write_data(bs, buffer, size);
		gf_bs_del(trun->cache);
		gf_free(buffer);
		trun->cache = NULL;
		traf->DataCache=0;
	}

	if (load_mdat_only) {
		u64 pos = gf_bs_get_position(bs);
		//we assume we never write large MDATs in fragment mode which should always be true
		movie->moof->mdat_size = (u32) (pos - movie->moof->fragment_offset);

		if (movie->segment_bs) {
			gf_bs_seek(bs, 0);
			/*write mdat size*/
			gf_bs_write_u32(bs, (u32) movie->moof->mdat_size);
			/*and get internal buffer*/
			gf_bs_seek(bs, movie->moof->mdat_size);
			gf_bs_get_content(bs, &movie->moof->mdat, &movie->moof->mdat_size);

			gf_bs_del(bs);
			bs = movie->editFileMap->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		} else {
			u64 offset = movie->segment_start;
			gf_bs_seek(bs, offset);
			/*write mdat size*/
			gf_bs_write_u32(bs, (u32) movie->moof->mdat_size);

			movie->moof->mdat = gf_malloc(sizeof(char) * movie->moof->mdat_size);
			if (!movie->moof->mdat) return GF_OUT_OF_MEM;

			gf_bs_seek(bs, offset);
			gf_bs_read_data(bs, movie->moof->mdat, movie->moof->mdat_size);

			gf_bs_seek(bs, offset);
			gf_bs_truncate(bs);
		}

		return GF_OK;
	}

	moof_start = gf_bs_get_position(bs);

	if (movie->moof->ntp) {
		moof_start += 8*4;
	}

	//2- update MOOF MDAT header
	if (!movie->moof->mdat) {
		gf_bs_seek(bs, movie->moof->fragment_offset);
		//we assume we never write large MDATs in fragment mode which should always be true
		mdat_size = (u32) (moof_start - movie->moof->fragment_offset);
		gf_bs_write_u32(bs, (u32) mdat_size);
		gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_MDAT);
		gf_bs_seek(bs, moof_start);
	}

	/*estimate moof size and shift trun offsets*/
#ifndef USE_BASE_DATA_OFFSET
	offset = 0;
	if (movie->use_segments) {
		e = gf_isom_box_size((GF_Box *) movie->moof);
		offset = (s32) movie->moof->size;
		/*mdat size & type*/
		offset += 8;
		update_trun_offsets(movie, offset);
	}
#endif

	//3- clean our traf's
	i=0;
	while ((traf = (GF_TrackFragmentBox*) gf_list_enum(movie->moof->TrackList, &i))) {
		//compute default settings for the TRAF
		ComputeFragmentDefaults(traf);
		//updates all trun and set all flags, INCLUDING TRAF FLAGS (durations, ...)
		s_count = UpdateRuns(movie, traf);
		//empty fragment destroy it
		if (!traf->tfhd->EmptyDuration && !s_count) {
			i--;
			gf_list_rem(movie->moof->TrackList, i);
			gf_isom_box_del((GF_Box *) traf);
			continue;
		}
	}

	buffer = NULL;
	/*rewind bitstream and load mdat in memory */
	if (movie->moof_first && !movie->moof->mdat) {
		buffer = gf_malloc(sizeof(char)*mdat_size);
		gf_bs_seek(bs, movie->moof->fragment_offset);
		gf_bs_read_data(bs, buffer, mdat_size);
		/*back to mdat start and erase with moov*/
		gf_bs_seek(bs, movie->moof->fragment_offset);
		gf_bs_truncate(bs);
	}

	//4- Write moof
	e = gf_isom_box_size((GF_Box *) movie->moof);
	if (e) return e;
	/*moof first, update traf headers - THIS WILL IMPACT THE MOOF SIZE IF WE
	DECIDE NOT TO USE THE DATA-OFFSET FLAG*/
	if (movie->moof_first
#ifndef USE_BASE_DATA_OFFSET
	        && !movie->use_segments
#endif
	   ) {
		i=0;
		while ((traf = (GF_TrackFragmentBox*)gf_list_enum(movie->moof->TrackList, &i))) {
			/*offset increases by moof size*/
			traf->tfhd->base_data_offset += movie->moof->size;
			traf->tfhd->base_data_offset += data_offset_diff;
		}
	}
#ifndef USE_BASE_DATA_OFFSET
	else if (movie->use_segments) {
		if (offset != (movie->moof->size+8)) {
			offset = (s32) (movie->moof->size + 8 - offset);
			update_trun_offsets(movie, offset);
			e = gf_isom_box_size((GF_Box *) movie->moof);
		}
	}
#endif


	if (movie->moof->ntp) {
		gf_bs_write_u32(bs, 8*4);
		gf_bs_write_u32(bs, GF_4CC('p','r','f','t') );
		gf_bs_write_u8(bs, 1);
		gf_bs_write_u24(bs, 0);
		gf_bs_write_u32(bs, movie->moof->reference_track_ID);
		gf_bs_write_u64(bs, movie->moof->ntp);
		gf_bs_write_u64(bs, movie->moof->timestamp);
	}

	pos = gf_bs_get_position(bs);
	i=0;
	while ((traf = gf_list_enum(movie->moof->TrackList, &i))) {
		traf->moof_start_in_bs = pos;
	}

	e = gf_isom_box_write((GF_Box *) movie->moof, bs);
	if (e) return e;

	//rewrite mdat after moof
	if (movie->moof->mdat) {
		gf_bs_write_data(bs, movie->moof->mdat, movie->moof->mdat_size);
		gf_free(movie->moof->mdat);
		movie->moof->mdat = NULL;
	} else if (buffer) {
		gf_bs_write_data(bs, buffer, mdat_size);
		gf_free(buffer);
	}

	if (moof_size) *moof_size = (u32) movie->moof->size;

	if (!movie->use_segments) {
		gf_isom_box_del((GF_Box *) movie->moof);
		movie->moof = NULL;
	}
	return GF_OK;
}

static GF_Err sidx_rewrite(GF_SegmentIndexBox *sidx, GF_BitStream *bs, u64 start_pos)
{
	GF_Err e;
	u64 pos = gf_bs_get_position(bs);
	/*write sidx*/
	gf_bs_seek(bs, start_pos);
	e = gf_isom_box_write((GF_Box *) sidx, bs);
	gf_bs_seek(bs, pos);
	return e;
}

GF_Err gf_isom_allocate_sidx(GF_ISOFile *movie, s32 subsegs_per_sidx, Bool daisy_chain_sidx, u32 nb_segs, u32 *frags_per_segment, u32 *start_range, u32 *end_range)
{
	GF_BitStream *bs;
	GF_Err e;

	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;
	if (movie->root_sidx) return GF_BAD_PARAM;
	if (movie->moof) return GF_BAD_PARAM;
	if (gf_list_count(movie->moof_list)) return GF_BAD_PARAM;

	movie->root_sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
	/*we don't write anything between sidx and following moov*/
	movie->root_sidx->first_offset = 0;

	/*for now we only store one ref per subsegment and don't support daisy-chaining*/
	movie->root_sidx->nb_refs = nb_segs;

	movie->root_sidx->refs = gf_malloc(sizeof(GF_SIDXReference) * movie->root_sidx->nb_refs);
	memset(movie->root_sidx->refs, 0, sizeof(GF_SIDXReference) * movie->root_sidx->nb_refs);

	movie->root_sidx_index = 0;

	/*remember start of sidx*/
	movie->root_sidx_offset = gf_bs_get_position(movie->editFileMap->bs);

	bs = movie->editFileMap->bs;

	e = gf_isom_box_size((GF_Box *) movie->root_sidx);
	if (e) return e;
	e = gf_isom_box_write((GF_Box *) movie->root_sidx, bs);
	if (e) return e;

	if (start_range) *start_range = (u32) movie->root_sidx_offset;
	if (end_range) *end_range = (u32) gf_bs_get_position(bs)-1;

	return GF_OK;
}


static GF_Err gf_isom_write_styp(GF_ISOFile *movie, Bool last_segment)
{
	GF_Err e = GF_OK;
	/*write STYP if we write to a different file or if we write the last segment*/
	if (!movie->append_segment && !movie->segment_start && !movie->styp_written) {

		/*modify brands STYP*/

		/*"msix" brand: this is a DASH Initialization Segment*/
		gf_isom_modify_alternate_brand(movie, GF_4CC('m','s','i','x'), 1);
		if (last_segment) {
			/*"lmsg" brand: this is the last DASH Segment*/
			gf_isom_modify_alternate_brand(movie, GF_4CC('l','m','s','g'), 1);
		}

		movie->brand->type = GF_ISOM_BOX_TYPE_STYP;
		e = gf_isom_box_size((GF_Box *) movie->brand);
		if (e) return e;
		e = gf_isom_box_write((GF_Box *) movie->brand, movie->editFileMap->bs);
		if (e) return e;

		movie->styp_written = 1;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_flush_fragments(GF_ISOFile *movie, Bool last_segment)
{
	GF_BitStream *temp_bs = NULL;
	GF_Err e;

	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	/*flush our fragment (store in mem)*/
	if (movie->moof) {
		e = StoreFragment(movie, 1, 0, NULL);
		if (e) return e;
	}

	if (movie->segment_bs) {
		temp_bs = movie->editFileMap->bs;
		movie->editFileMap->bs = movie->segment_bs;
	}

	gf_bs_seek(movie->editFileMap->bs, movie->segment_start);
	gf_bs_truncate(movie->editFileMap->bs);

	/*write styp to file if needed*/
	e = gf_isom_write_styp(movie, last_segment);
	if (e) return e;

	/*write all pending fragments to file*/
	while (gf_list_count(movie->moof_list)) {
		s32 offset_diff;
		u32 moof_size;

		movie->moof = gf_list_get(movie->moof_list, 0);
		gf_list_rem(movie->moof_list, 0);

		offset_diff = (s32) (gf_bs_get_position(movie->editFileMap->bs) - movie->moof->fragment_offset);
		movie->moof->fragment_offset = gf_bs_get_position(movie->editFileMap->bs);

		e = StoreFragment(movie, 0, offset_diff, &moof_size);

		gf_isom_box_del((GF_Box *) movie->moof);
		movie->moof = NULL;
	}

	/*append mode: store fragment at the end of the regular movie bitstream, and delete the temp bitstream*/
	if (movie->append_segment) {
		char bloc[1024];
		u32 seg_size = (u32) gf_bs_get_size(movie->editFileMap->bs);
		gf_bs_seek(movie->editFileMap->bs, 0);
		while (seg_size) {
			u32 size = gf_bs_read_data(movie->editFileMap->bs, bloc, (seg_size>1024) ? 1024 : seg_size);
			gf_bs_write_data(movie->movieFileMap->bs, bloc, size);
			seg_size -= size;
		}
		gf_isom_datamap_flush(movie->movieFileMap);

		gf_isom_datamap_del(movie->editFileMap);
		movie->editFileMap = gf_isom_fdm_new_temp(NULL);
	} else {
		gf_isom_datamap_flush(movie->editFileMap);
	}
	movie->segment_start = gf_bs_get_position(movie->editFileMap->bs);

	if (temp_bs) {
		movie->segment_bs = movie->editFileMap->bs;
		movie->editFileMap->bs = temp_bs;
	}
	return GF_OK;
}

typedef struct
{
	GF_SegmentIndexBox *sidx;
	u64 start_offset, end_offset;
} SIDXEntry;

static u64 get_presentation_time(u64 media_time, s32 ts_shift)
{
	if ((ts_shift<0) && (media_time < -ts_shift)) {
		media_time = 0;
	} else {
		media_time += ts_shift;
	}
	return media_time ;
}

GF_EXPORT
GF_Err gf_isom_close_segment(GF_ISOFile *movie, s32 subsegments_per_sidx, u32 referenceTrackID, u64 ref_track_decode_time, s32 ts_shift, u64 ref_track_next_cts, Bool daisy_chain_sidx, Bool last_segment, u32 segment_marker_4cc, u64 *index_start_range, u64 *index_end_range)
{
	GF_SegmentIndexBox *sidx=NULL;
	GF_SegmentIndexBox *root_sidx=NULL;
	GF_List *daisy_sidx = NULL;
	u64 sidx_start, sidx_end;
	Bool first_frag_in_subseg;
	Bool no_sidx = 0;
	u32 count, idx, cur_dur, sidx_dur, sidx_idx, idx_offset, frag_count;
	u64 last_top_box_pos, root_prev_offset, local_sidx_start, local_sidx_end, prev_earliest_cts;
	GF_TrackBox *trak = NULL;
	GF_Err e;
	/*number of subsegment in this segment (eg nb references in the first SIDX found)*/
	u32 nb_subsegs=0;
	/*number of subsegment per sidx (eg number of references of any sub-SIDX*/
	u32 subseg_per_sidx;
	/*number of fragments per subsegment*/
	u32 frags_per_subseg;
	/*number of fragments per subsidx*/
	u32 frags_per_subsidx;

	sidx_start = sidx_end = 0;

	if (index_start_range) *index_start_range = 0;
	if (index_end_range) *index_end_range = 0;

	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	count = gf_list_count(movie->moov->mvex->TrackExList);
	if (!count) return GF_BAD_PARAM;

	/*store fragment*/
	if (movie->moof) {
		e = StoreFragment(movie, 1, 0, NULL);
		if (e) return e;
	}
	/*restore final bitstream*/
	if (movie->segment_bs) {
		gf_bs_del(movie->editFileMap->bs);
		movie->editFileMap->bs = movie->segment_bs;
		movie->segment_bs = NULL;
	}

	count = gf_list_count(movie->moof_list);
	if (!count) {
		/*append segment marker box*/
		if (segment_marker_4cc) {
			if (movie->append_segment) {
				gf_bs_write_u32(movie->movieFileMap->bs, 8);	//write size field
				gf_bs_write_u32(movie->movieFileMap->bs, segment_marker_4cc); //write box type field
			} else {
				gf_bs_write_u32(movie->editFileMap->bs, 8);	//write size field
				gf_bs_write_u32(movie->editFileMap->bs, segment_marker_4cc); //write box type field
			}
		}
		return GF_OK;
	}

	gf_bs_seek(movie->editFileMap->bs, movie->segment_start);
	gf_bs_truncate(movie->editFileMap->bs);

	idx_offset = 0;

	if (referenceTrackID) {
		trak = gf_isom_get_track_from_id(movie->moov, referenceTrackID);
	}

	if (subsegments_per_sidx < 0) {
		referenceTrackID = 0;
		subsegments_per_sidx = 0;
		no_sidx = 1;
	}

	e = gf_isom_write_styp(movie, last_segment);
	if (e) return e;

	frags_per_subseg = 0;
	subseg_per_sidx = 0;
	frags_per_subsidx = 0;

	prev_earliest_cts = 0;

	if (daisy_chain_sidx)
		daisy_sidx = gf_list_new();

	/*prepare SIDX: we write a blank SIDX box with the right number of entries, and will rewrite it later on*/
	if (referenceTrackID) {
		Bool is_root_sidx=0;

		prev_earliest_cts = get_presentation_time( ref_track_decode_time + moof_get_earliest_cts(gf_list_get(movie->moof_list, 0), referenceTrackID), ts_shift);

		if (movie->root_sidx) {
			sidx = movie->root_sidx;
		} else {
			sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
		}
		sidx->reference_ID = referenceTrackID;
		sidx->timescale = trak->Media->mediaHeader->timeScale;
		/*we don't write anything between sidx and following moov*/
		sidx->first_offset = 0;

		/*we allocated our sidx to have one ref per "segment" (eg per call to close_segment)*/
		if (movie->root_sidx) {
			if (!movie->root_sidx_index) {
				sidx->earliest_presentation_time = prev_earliest_cts;
			}
			nb_subsegs = 1;
			frags_per_subseg = count;
			frags_per_subsidx = count;
			subseg_per_sidx = 1;
			daisy_chain_sidx = 0;

			idx_offset = movie->root_sidx_index;
			sidx_end = gf_bs_get_position(movie->editFileMap->bs);
		} else {
			sidx->earliest_presentation_time = prev_earliest_cts;

			/*if more subsegments requested than fragments available, make a single sidx*/
			if ((s32) count <= subsegments_per_sidx)
				subsegments_per_sidx = 0;

			if (daisy_chain_sidx && (subsegments_per_sidx<2))
				subsegments_per_sidx = 2;

			/*single SIDX, each fragment is a subsegment and we reference all subsegments*/
			if (!subsegments_per_sidx) {
				nb_subsegs = count;
				/*we consider that each fragment is a subsegment - this could be controled by another parameter*/
				frags_per_subseg = 1;
				frags_per_subsidx = count;
				subseg_per_sidx = count;

				sidx->nb_refs = nb_subsegs;
				daisy_chain_sidx = 0;
			}
			/*daisy-chain SIDX: each SIDX describes a subsegment made of frags_per_subseg fragments plus next */
			else if (daisy_chain_sidx) {
				frags_per_subsidx = count/subsegments_per_sidx;
				if (frags_per_subsidx * subsegments_per_sidx < count) frags_per_subsidx++;

				nb_subsegs = subsegments_per_sidx;

				/*we consider that each fragment is a subsegment - this could be controled by another parameter*/
				frags_per_subseg = 1;
				subseg_per_sidx = frags_per_subsidx / frags_per_subseg;
				if (subseg_per_sidx * frags_per_subseg < frags_per_subsidx) subseg_per_sidx++;

				sidx->nb_refs = subseg_per_sidx + 1;
			}
			/*hierarchical SIDX*/
			else {
				frags_per_subsidx = count/subsegments_per_sidx;
				if (frags_per_subsidx * subsegments_per_sidx < count) frags_per_subsidx++;

				nb_subsegs = subsegments_per_sidx;

				/*we consider that each fragment is a subsegment - this could be controled by another parameter*/
				frags_per_subseg = 1;
				subseg_per_sidx = frags_per_subsidx / frags_per_subseg;
				if (subseg_per_sidx * frags_per_subseg < frags_per_subsidx) subseg_per_sidx++;

				sidx->nb_refs = nb_subsegs;
				is_root_sidx = 1;
			}

			sidx->refs = gf_malloc(sizeof(GF_SIDXReference)*sidx->nb_refs);
			memset(sidx->refs, 0, sizeof(GF_SIDXReference)*sidx->nb_refs);

			/*remember start of sidx*/
			sidx_start = gf_bs_get_position(movie->editFileMap->bs);

			e = gf_isom_box_size((GF_Box *) sidx);
			if (e) return e;
			e = gf_isom_box_write((GF_Box *) sidx, movie->editFileMap->bs);
			if (e) return e;

			sidx_end = gf_bs_get_position(movie->editFileMap->bs);

			if (daisy_sidx) {
				SIDXEntry *entry;
				GF_SAFEALLOC(entry, SIDXEntry);
				entry->sidx = sidx;
				entry->start_offset = sidx_start;
				gf_list_add(daisy_sidx, entry);
			}
		}

		if (is_root_sidx) {
			root_sidx = sidx;
			sidx = NULL;
		}
		count = idx = 0;
	}


	last_top_box_pos = root_prev_offset = sidx_end;
	sidx_idx = 0;
	sidx_dur = 0;
	local_sidx_start = local_sidx_end = 0;

	/*cumulated segments duration since start of the sidx */
	frag_count = frags_per_subsidx;
	cur_dur = 0;
	first_frag_in_subseg = 1;
	e = GF_OK;
	while (gf_list_count(movie->moof_list)) {
		s32 offset_diff;
		u32 moof_size;

		movie->moof = gf_list_get(movie->moof_list, 0);
		gf_list_rem(movie->moof_list, 0);

		/*hierarchical or daisy-chain SIDXs*/
		if (!no_sidx && !sidx && (root_sidx || daisy_chain_sidx) ) {
			u32 subsegments_remaining;
			sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
			sidx->reference_ID = referenceTrackID;
			sidx->timescale = trak->Media->mediaHeader->timeScale;
			sidx->earliest_presentation_time = get_presentation_time( ref_track_decode_time + sidx_dur + moof_get_earliest_cts(movie->moof, referenceTrackID), ts_shift);

			frag_count = frags_per_subsidx;

			/*last segment, add only one ref*/
			subsegments_remaining = 1 + gf_list_count(movie->moof_list);
			if (subseg_per_sidx*frags_per_subseg > subsegments_remaining) {
				subseg_per_sidx = subsegments_remaining / frags_per_subseg;
				if (subseg_per_sidx * frags_per_subseg < subsegments_remaining) subseg_per_sidx++;
			}
			/*we don't write anything between sidx and following moov*/
			sidx->first_offset = 0;
			sidx->nb_refs = subseg_per_sidx;
			if (daisy_chain_sidx && (nb_subsegs>1)) {
				sidx->nb_refs += 1;
			}
			sidx->refs = gf_malloc(sizeof(GF_SIDXReference)*sidx->nb_refs);
			memset(sidx->refs, 0, sizeof(GF_SIDXReference)*sidx->nb_refs);

			if (root_sidx)
				root_sidx->refs[sidx_idx].reference_type = 1;

			/*remember start of sidx*/
			local_sidx_start = gf_bs_get_position(movie->editFileMap->bs);

			/*write it*/
			e = gf_isom_box_size((GF_Box *) sidx);
			if (e) return e;
			e = gf_isom_box_write((GF_Box *) sidx, movie->editFileMap->bs);
			if (e) return e;

			local_sidx_end = gf_bs_get_position(movie->editFileMap->bs);

			/*adjust prev offset*/
			last_top_box_pos = local_sidx_end;

			if (daisy_sidx) {
				SIDXEntry *entry;
				GF_SAFEALLOC(entry, SIDXEntry);
				entry->sidx = sidx;
				entry->start_offset = local_sidx_start;
				gf_list_add(daisy_sidx, entry);
			}
		}

		offset_diff = (s32) (gf_bs_get_position(movie->editFileMap->bs) - movie->moof->fragment_offset);
		movie->moof->fragment_offset = gf_bs_get_position(movie->editFileMap->bs);

		if (!e) {
			e = StoreFragment(movie, 0, offset_diff, &moof_size);


			if (sidx) {
				u32 cur_index = idx_offset + idx;

				/*do not compute earliest CTS if single segment sidx since we only have set the info for one subsegment*/
				if (!movie->root_sidx && first_frag_in_subseg) {
					u64 first_cts = get_presentation_time( ref_track_decode_time + sidx_dur + cur_dur +  moof_get_earliest_cts(movie->moof, referenceTrackID), ts_shift);
					u32 subseg_dur = (u32) (first_cts - prev_earliest_cts);
					if (cur_index) {
						sidx->refs[cur_index-1].subsegment_duration = subseg_dur;
						if (root_sidx) root_sidx->refs[sidx_idx].subsegment_duration += subseg_dur;
					}
					prev_earliest_cts = first_cts;
					first_frag_in_subseg = 0;
				}


				/*we refer to next moof*/
				sidx->refs[cur_index].reference_type = 0;
				if (!sidx->refs[cur_index].SAP_type) {
					sidx->refs[cur_index].SAP_type = moof_get_sap_info(movie->moof, referenceTrackID, & sidx->refs[cur_index].SAP_delta_time, & sidx->refs[cur_index].starts_with_SAP);
					if (sidx->refs[cur_index].SAP_type) {
						if (root_sidx && !root_sidx->refs[sidx_idx].SAP_type) {
							root_sidx->refs[sidx_idx].SAP_type = sidx->refs[cur_index].SAP_type;
							root_sidx->refs[sidx_idx].SAP_delta_time = sidx->refs[cur_index].SAP_delta_time;
							root_sidx->refs[sidx_idx].starts_with_SAP = sidx->refs[cur_index].starts_with_SAP;
						}
					}
				}
				cur_dur += moof_get_duration(movie->moof, referenceTrackID);

				/*reference size is end of the moof we just wrote minus last_box_pos*/
				sidx->refs[cur_index].reference_size += (u32) ( gf_bs_get_position(movie->editFileMap->bs) - last_top_box_pos) ;
				last_top_box_pos = gf_bs_get_position(movie->editFileMap->bs);

				count++;

				/*we are switching subsegment*/
				frag_count--;

				if (count==frags_per_subseg) {
					count = 0;
					first_frag_in_subseg = 1;
					idx++;
				}

				/*switching to next SIDX*/
				if ((idx==subseg_per_sidx) || !frag_count) {
					u32 subseg_dur;
					u64 next_cts;
					/*update last ref duration*/
					if (gf_list_count(movie->moof_list)) {
						next_cts = get_presentation_time( ref_track_decode_time + sidx_dur + cur_dur + moof_get_earliest_cts(gf_list_get(movie->moof_list, 0), referenceTrackID), ts_shift);
					} else {
						next_cts = get_presentation_time( ref_track_next_cts, ts_shift);
					}
					subseg_dur = (u32) (next_cts - prev_earliest_cts);
					if (movie->root_sidx) {
						sidx->refs[idx_offset].subsegment_duration = subseg_dur;
					}
					/*if daisy chain and not the last sidx, we have an extra entry at the end*/
					else if (daisy_chain_sidx && (nb_subsegs>1)) {
						sidx->refs[sidx->nb_refs - 2].subsegment_duration = subseg_dur;
					} else {
						sidx->refs[sidx->nb_refs-1].subsegment_duration = subseg_dur;
					}
					if (root_sidx) root_sidx->refs[sidx_idx].subsegment_duration += subseg_dur;

					if (root_sidx) {
						root_sidx->refs[sidx_idx].reference_size = (u32) (gf_bs_get_position(movie->editFileMap->bs) - local_sidx_start);
						if (!sidx_idx) {
							root_sidx->earliest_presentation_time = sidx->earliest_presentation_time;
						}
						sidx_rewrite(sidx, movie->editFileMap->bs, local_sidx_start);
						gf_isom_box_del((GF_Box*)sidx);
						sidx = NULL;
					} else if (daisy_chain_sidx) {
						SIDXEntry *entry = gf_list_last(daisy_sidx);
						entry->end_offset = gf_bs_get_position(movie->editFileMap->bs);
						nb_subsegs--;
						sidx = NULL;
					}
					sidx_dur += cur_dur;
					cur_dur = 0;
					count = 0;
					idx=0;
					if (movie->root_sidx)
						movie->root_sidx_index++;
					sidx_idx++;
				}
			}
		}
		gf_isom_box_del((GF_Box *) movie->moof);
		movie->moof = NULL;
	}

	/*append segment marker box*/
	if (segment_marker_4cc) {
		gf_bs_write_u32(movie->editFileMap->bs, 8);	//write size field
		gf_bs_write_u32(movie->editFileMap->bs, segment_marker_4cc); //write box type field
	}

	if (movie->root_sidx) {
		if (last_segment) {
			assert(movie->root_sidx_index == movie->root_sidx->nb_refs);

			sidx_rewrite(movie->root_sidx, movie->editFileMap->bs, movie->root_sidx_offset);
			gf_isom_box_del((GF_Box*) movie->root_sidx);
			movie->root_sidx = NULL;
		}
		return GF_OK;
	}

	if (sidx) {
		assert(!root_sidx);
		sidx_rewrite(sidx, movie->editFileMap->bs, sidx_start);
		gf_isom_box_del((GF_Box*)sidx);
	}
	if (daisy_sidx) {
		u32 i, j;
		u64 last_entry_end_offset = 0;
		u32 count = gf_list_count(daisy_sidx);
		for (i=count; i>1; i--) {
			SIDXEntry *entry = gf_list_get(daisy_sidx, i-2);
			SIDXEntry *next_entry = gf_list_get(daisy_sidx, i-1);

			if (!last_entry_end_offset) {
				last_entry_end_offset = next_entry->end_offset;
				/*rewrite last sidx*/
				sidx_rewrite(next_entry->sidx, movie->editFileMap->bs, next_entry->start_offset);
			}
			/*copy over SAP info for last item (which points to next item !)*/
			entry->sidx->refs[entry->sidx->nb_refs-1] = next_entry->sidx->refs[0];
			/*and rewrite reference type, size and dur*/
			entry->sidx->refs[entry->sidx->nb_refs-1].reference_type = 1;
			entry->sidx->refs[entry->sidx->nb_refs-1].reference_size = (u32) (last_entry_end_offset - next_entry->start_offset);
			entry->sidx->refs[entry->sidx->nb_refs-1].subsegment_duration = 0;
			for (j=0; j<next_entry->sidx->nb_refs; j++) {
				entry->sidx->refs[entry->sidx->nb_refs-1].subsegment_duration += next_entry->sidx->refs[j].subsegment_duration;
			}
			sidx_rewrite(entry->sidx, movie->editFileMap->bs, entry->start_offset);
		}
		while (gf_list_count(daisy_sidx)) {
			SIDXEntry *entry = gf_list_last(daisy_sidx);
			gf_isom_box_del((GF_Box*)entry->sidx);
			gf_free(entry);
			gf_list_rem_last(daisy_sidx);
		}
		gf_list_del(daisy_sidx);
	}
	if (root_sidx) {
		sidx_rewrite(root_sidx, movie->editFileMap->bs, sidx_start);
		gf_isom_box_del((GF_Box*)root_sidx);
	}

	if ((root_sidx || sidx) && !daisy_chain_sidx) {
		if (index_start_range) *index_start_range = sidx_start;
		if (index_end_range) *index_end_range = sidx_end - 1;
	}

	if (movie->append_segment) {
		char bloc[1024];
		u32 seg_size = (u32) gf_bs_get_size(movie->editFileMap->bs);
		gf_bs_seek(movie->editFileMap->bs, 0);
		while (seg_size) {
			u32 size = gf_bs_read_data(movie->editFileMap->bs, bloc, (seg_size>1024) ? 1024 : seg_size);
			gf_bs_write_data(movie->movieFileMap->bs, bloc, size);
			seg_size -= size;
		}
		gf_isom_datamap_del(movie->editFileMap);
		movie->editFileMap = gf_isom_fdm_new_temp(NULL);
	}

	return e;
}

GF_EXPORT
GF_Err gf_isom_close_fragments(GF_ISOFile *movie)
{
	if (movie->use_segments) {
		return gf_isom_close_segment(movie, 0, 0, 0, 0, 0, 0, 1, 0, NULL, NULL);
	} else {
		return StoreFragment(movie, 0, 0, NULL);
	}
}

GF_EXPORT
GF_Err gf_isom_start_segment(GF_ISOFile *movie, const char *SegName, Bool memory_mode)
{
	GF_Err e;
	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	if (gf_list_count(movie->moof_list))
		return GF_BAD_PARAM;

	movie->segment_bs = NULL;
	movie->append_segment = 0;
	/*update segment file*/
	if (SegName) {
		gf_isom_datamap_del(movie->editFileMap);
		e = gf_isom_datamap_new(SegName, NULL, GF_ISOM_DATA_MAP_WRITE, & movie->editFileMap);
		movie->segment_start = 0;
		movie->styp_written = 0;
		if (e) return e;
	} else {
		assert(gf_list_count(movie->moof_list) == 0);
		movie->segment_start = gf_bs_get_position(movie->editFileMap->bs);
		/*if movieFileMap is not null, we are concatenating segments to the original movie, force a copy*/
		if (movie->movieFileMap)
			movie->append_segment = 1;
	}

	/*create a memory bitstream for all file IO until final flush*/
	if (memory_mode) {
		movie->segment_bs = movie->editFileMap->bs;
		movie->editFileMap->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_fragment_reference_time(GF_ISOFile *movie, u32 reference_track_ID, u64 ntp, u64 timestamp)
{
	if (!movie->moof) return GF_BAD_PARAM;
	movie->moof->reference_track_ID = reference_track_ID;
	movie->moof->ntp = ntp;
	movie->moof->timestamp = timestamp;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_start_fragment(GF_ISOFile *movie, Bool moof_first)
{
	u32 i, count;
	GF_TrackExtendsBox *trex;
	GF_TrackFragmentBox *traf;
	GF_Err e;
	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) )
		return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	count = gf_list_count(movie->moov->mvex->TrackExList);
	if (!count)
		return GF_BAD_PARAM;

	/*always force cached mode when writing movie segments*/
	if (movie->use_segments) moof_first = 1;
	movie->moof_first = moof_first;

	//store existing fragment
	if (movie->moof) {
		e = StoreFragment(movie, movie->use_segments ? 1 : 0, 0, NULL);
		if (e) return e;
	}

	//create new fragment
	movie->moof = (GF_MovieFragmentBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MOOF);
	movie->moof->mfhd = (GF_MovieFragmentHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MFHD);
	movie->moof->mfhd->sequence_number = movie->NextMoofNumber;
	movie->NextMoofNumber ++;
	if (movie->use_segments)
		gf_list_add(movie->moof_list, movie->moof);


	/*remember segment offset*/
	movie->moof->fragment_offset = gf_bs_get_position(movie->editFileMap->bs);
	/*prepare MDAT*/
	gf_bs_write_u32(movie->editFileMap->bs, 0);
	gf_bs_write_u32(movie->editFileMap->bs, GF_ISOM_BOX_TYPE_MDAT);

	//we create a TRAF for each setup track, unused ones will be removed at store time
	for (i=0; i<count; i++) {
		trex = (GF_TrackExtendsBox*)gf_list_get(movie->moov->mvex->TrackExList, i);
		traf = (GF_TrackFragmentBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TRAF);
		traf->trex = trex;
		traf->tfhd = (GF_TrackFragmentHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TFHD);
		traf->tfhd->trackID = trex->trackID;
		//add 8 bytes (MDAT size+type) to avoid the data_offset in the first trun
		traf->tfhd->base_data_offset = movie->moof->fragment_offset + 8;
		gf_list_add(movie->moof->TrackList, traf);
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_clone_pssh(GF_ISOFile *output, GF_ISOFile *input, Bool in_moof) {
	GF_Box *a;
	u32 i;
	i = 0;

	while ((a = (GF_Box *)gf_list_enum(input->moov->other_boxes, &i))) {
		if (a->type == GF_ISOM_BOX_TYPE_PSSH) {
			GF_ProtectionSystemHeaderBox *pssh = (GF_ProtectionSystemHeaderBox *)pssh_New();
			memmove(pssh->SystemID, ((GF_ProtectionSystemHeaderBox *)a)->SystemID, 16);
			pssh->KID_count = ((GF_ProtectionSystemHeaderBox *)a)->KID_count;
			pssh->KIDs = (bin128 *)gf_malloc(pssh->KID_count*sizeof(bin128));
			memmove(pssh->KIDs, ((GF_ProtectionSystemHeaderBox *)a)->KIDs, pssh->KID_count*sizeof(bin128));
			pssh->private_data_size = ((GF_ProtectionSystemHeaderBox *)a)->private_data_size;
			pssh->private_data = (u8 *)gf_malloc(pssh->private_data_size*sizeof(char));
			memmove(pssh->private_data, ((GF_ProtectionSystemHeaderBox *)a)->private_data, pssh->private_data_size);

			gf_isom_box_add_default(in_moof ? (GF_Box*)output->moof : (GF_Box*)output->moov, (GF_Box*)pssh);
		}
	}
	return GF_OK;
}

u32 GetRunSize(GF_TrackFragmentRunBox *trun)
{
	u32 i, size;
	GF_TrunEntry *ent;
	size = 0;
	i=0;
	while ((ent = (GF_TrunEntry*)gf_list_enum(trun->entries, &i))) {
		size += ent->size;
	}
	return size;
}

GF_EXPORT
GF_Err gf_isom_fragment_add_sample(GF_ISOFile *movie, u32 TrackID, GF_ISOSample *sample, u32 DescIndex,
                                   u32 Duration, u8 PaddingBits, u16 DegradationPriority, Bool redundant_coding)
{
	u32 count, buffer_size;
	char *buffer;
	u64 pos;
	GF_ISOSample *od_sample = NULL;
	GF_TrunEntry *ent;
	GF_TrackFragmentBox *traf, *traf_2;
	GF_TrackFragmentRunBox *trun;
	if (!movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) || !sample)
		return GF_BAD_PARAM;

	traf = GetTraf(movie, TrackID);
	if (!traf)
		return GF_BAD_PARAM;

	if (!traf->tfhd->sample_desc_index) traf->tfhd->sample_desc_index = DescIndex ? DescIndex : traf->trex->def_sample_desc_index;

	pos = gf_bs_get_position(movie->editFileMap->bs);


	//WARNING: we change stream description, create a new TRAF
	if ( DescIndex && (traf->tfhd->sample_desc_index != DescIndex)) {
		//if we're caching flush the current run
		if (traf->DataCache) {
			count = gf_list_count(traf->TrackRuns);
			if (count) {
				trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
				trun->data_offset = (u32) (pos - movie->moof->fragment_offset - 8);
				gf_bs_get_content(trun->cache, &buffer, &buffer_size);
				gf_bs_write_data(movie->editFileMap->bs, buffer, buffer_size);
				gf_bs_del(trun->cache);
				trun->cache = NULL;
				gf_free(buffer);
			}
		}
		traf_2 = (GF_TrackFragmentBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TRAF);
		traf_2->trex = traf->trex;
		traf_2->tfhd = (GF_TrackFragmentHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TFHD);
		traf_2->tfhd->trackID = traf->tfhd->trackID;
		//keep the same offset
		traf_2->tfhd->base_data_offset = movie->moof->fragment_offset + 8;
		gf_list_add(movie->moof->TrackList, traf_2);

		//duplicate infos
		traf_2->tfhd->IFrameSwitching = traf->tfhd->IFrameSwitching;
		traf_2->DataCache  = traf->DataCache;
		traf_2->tfhd->sample_desc_index  = DescIndex;

		//switch them ...
		traf = traf_2;
	}

	pos = gf_bs_get_position(movie->editFileMap->bs);
	//add TRUN entry
	count = gf_list_count(traf->TrackRuns);
	if (count) {
		trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
		//check data offset when no caching as trun entries shall ALWAYS be contiguous samples
		if (!traf->DataCache && (movie->moof->fragment_offset + 8 + trun->data_offset + GetRunSize(trun) != pos) )
			count = 0;

		//check I-frame detection
		if (traf->tfhd->IFrameSwitching && sample->IsRAP)
			count = 0;

		if (traf->DataCache && (traf->DataCache==trun->sample_count) )
			count = 0;

		//if data cache is on and we're changing TRUN, store the cache and update data offset
		if (!count && traf->DataCache) {
			trun->data_offset = (u32) (pos - movie->moof->fragment_offset - 8);
			gf_bs_get_content(trun->cache, &buffer, &buffer_size);
			gf_bs_write_data(movie->editFileMap->bs, buffer, buffer_size);
			gf_bs_del(trun->cache);
			trun->cache = NULL;
			gf_free(buffer);
		}
	}

	//new run
	if (!count) {
		trun = (GF_TrackFragmentRunBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TRUN);
		//store data offset (we have the 8 btyes offset of the MDAT)
		trun->data_offset = (u32) (pos - movie->moof->fragment_offset - 8);
		gf_list_add(traf->TrackRuns, trun);

		//if we use data caching, create a bitstream
		if (traf->DataCache)
			trun->cache = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	}

	GF_SAFEALLOC(ent, GF_TrunEntry);
	ent->CTS_Offset = sample->CTS_Offset;
	ent->Duration = Duration;
	ent->size = sample->dataLength;
	ent->flags = GF_ISOM_FORMAT_FRAG_FLAGS(PaddingBits, sample->IsRAP, DegradationPriority);
	if (sample->IsRAP) {
		ent->flags |= GF_ISOM_GET_FRAG_DEPEND_FLAGS(0, 2, 0, (redundant_coding ? 1 : 0) );
		ent->SAP_type = sample->IsRAP;
	}
	gf_list_add(trun->entries, ent);

	if (sample->CTS_Offset<0) {
		trun->version = 1;
	}
	trun->sample_count += 1;

	//rewrite OD frames
	if (traf->trex->track->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		//this may fail if depandancies are not well done ...
		Media_ParseODFrame(traf->trex->track->Media, sample, &od_sample);
		sample = od_sample;
	}

	//finally write the data
	if (!traf->DataCache) {
		if (!gf_bs_write_data(movie->editFileMap->bs, sample->data, sample->dataLength)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso fragment] Could not add a sample with a size if %u bytes (no DataCache)\n", sample->dataLength));
			return GF_OUT_OF_MEM;
		}
	} else if (trun->cache) {
		if (!gf_bs_write_data(trun->cache, sample->data, sample->dataLength)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso fragment] Could not add a sample with a size if %u bytes (with cache)\n", sample->dataLength));
			return GF_OUT_OF_MEM;
		}
	} else {
		return GF_BAD_PARAM;
	}
	if (od_sample) gf_isom_sample_del(&od_sample);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_fragment_add_sai(GF_ISOFile *output, GF_ISOFile *input, u32 TrackID, u32 SampleNum)
{
	u32 trackNum;
	GF_Err e = GF_OK;

	trackNum = gf_isom_get_track_by_id(input, TrackID);
	if (gf_isom_is_cenc_media(input, trackNum, 1)) {
		GF_CENCSampleAuxInfo *sai;
		GF_TrackFragmentBox  *traf = GetTraf(output, TrackID);
		GF_TrackBox  *src_trak = gf_isom_get_track_from_file(input, TrackID);
		u32 boxType;
		GF_SampleEncryptionBox *senc;
		u8 IV_size;
		u32 IsEncrypted;

		if (!traf)  return GF_BAD_PARAM;

		sai = NULL;
		gf_isom_get_sample_cenc_info(input, trackNum, SampleNum, &IsEncrypted, &IV_size, NULL);
		e = gf_isom_cenc_get_sample_aux_info(input, trackNum, SampleNum, &sai, &boxType);
		if (e) return e;
		sai->IV_size = IV_size;

		switch (boxType) {
		case GF_ISOM_BOX_UUID_PSEC:
			if (!traf->piff_sample_encryption) {
				GF_PIFFSampleEncryptionBox *psec = (GF_PIFFSampleEncryptionBox *) src_trak->Media->information->sampleTable->piff_psec;
				if (!psec) return GF_ISOM_INVALID_FILE;
				traf->piff_sample_encryption = gf_isom_create_piff_psec_box(1, 0, psec->AlgorithmID, psec->IV_size, psec->KID);
				traf->piff_sample_encryption->traf = traf;
			}

			if (!traf->piff_sample_encryption) {
				return GF_IO_ERR;
			}
			senc = (GF_SampleEncryptionBox *) traf->piff_sample_encryption;
			break;
		case GF_ISOM_BOX_TYPE_SENC:
			if (!traf->sample_encryption) {
				traf->sample_encryption = gf_isom_create_samp_enc_box(0, 0);
				traf->sample_encryption->traf = traf;
			}

			if (!traf->sample_encryption) {
				return GF_IO_ERR;
			}
			senc = (GF_SampleEncryptionBox *) traf->sample_encryption;
			break;
		default:
			return GF_NOT_SUPPORTED;
		}

		gf_list_add(senc->samp_aux_info, sai);
		if (sai->subsample_count) senc->flags = 0x00000002;

		gf_isom_cenc_set_saiz_saio(senc, NULL, traf, IsEncrypted ? IV_size+2+6*sai->subsample_count : 0);

	}

	return GF_OK;
}


GF_Err gf_isom_fragment_append_data(GF_ISOFile *movie, u32 TrackID, char *data, u32 data_size, u8 PaddingBits)
{
	u32 count;
	u8 rap;
	u16 degp;
	GF_TrunEntry *ent;
	GF_TrackFragmentBox *traf;
	GF_TrackFragmentRunBox *trun;
	if (!movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	traf = GetTraf(movie, TrackID);
	if (!traf || !traf->tfhd->sample_desc_index) return GF_BAD_PARAM;

	//add TRUN entry
	count = gf_list_count(traf->TrackRuns);
	if (!count) return GF_BAD_PARAM;

	trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
	count = gf_list_count(trun->entries);
	if (!count) return GF_BAD_PARAM;
	ent = (GF_TrunEntry *)gf_list_get(trun->entries, count-1);
	ent->size += data_size;

	rap = GF_ISOM_GET_FRAG_SYNC(ent->flags);
	degp = GF_ISOM_GET_FRAG_DEG(ent->flags);
	ent->flags = GF_ISOM_FORMAT_FRAG_FLAGS(PaddingBits, rap, degp);

	//finally write the data
	if (!traf->DataCache) {
		gf_bs_write_data(movie->editFileMap->bs, data, data_size);
	} else if (trun->cache) {
		gf_bs_write_data(trun->cache, data, data_size);
	} else {
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

GF_Err gf_isom_fragment_add_subsample(GF_ISOFile *movie, u32 TrackID, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable)
{
	u32 i, count, last_sample;
	GF_TrackFragmentBox *traf;
	if (!movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	traf = GetTraf(movie, TrackID);
	if (!traf || !traf->tfhd->sample_desc_index) return GF_BAD_PARAM;

	/*compute last sample number in traf*/
	last_sample = 0;
	count = gf_list_count(traf->TrackRuns);
	for (i=0; i<count; i++) {
		GF_TrackFragmentRunBox *trun = gf_list_get(traf->TrackRuns, i);
		last_sample += trun->sample_count;
	}

	if (!traf->subs) {
		traf->subs = (GF_SubSampleInformationBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SUBS);
		traf->subs->version = (subSampleSize>0xFFFF) ? 1 : 0;
	}
	return gf_isom_add_subsample_info(traf->subs, last_sample, subSampleSize, priority, reserved, discardable);
}

GF_Err gf_isom_fragment_copy_subsample(GF_ISOFile *dest, u32 TrackID, GF_ISOFile *orig, u32 track, u32 sampleNumber, Bool sgpd_in_traf)
{
	u32 i, count, last_sample;
	GF_SubSampleInfoEntry *sub_sample;
	GF_Err e;
	GF_TrackBox *trak;
	GF_TrackFragmentBox *traf;
	GF_TrunEntry *ent;
	GF_TrackFragmentRunBox *trun;
	if (!dest->moof || !(dest->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	traf = GetTraf(dest, TrackID);
	if (!traf || !traf->tfhd->sample_desc_index) return GF_BAD_PARAM;

	trak = gf_isom_get_track_from_file(orig, track);
	if (!trak) return GF_BAD_PARAM;

	/*modify depends flags*/
	if (trak->Media->information->sampleTable->SampleDep) {
		u32 dependsOn, dependedOn, redundant;

		dependsOn = dependedOn = redundant = 0;
		count = gf_list_count(traf->TrackRuns);
		if (!count) return GF_BAD_PARAM;
		trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
		count = gf_list_count(trun->entries);
		if (!count) return GF_BAD_PARAM;

		ent = (GF_TrunEntry *)gf_list_get(trun->entries, count-1);
		e = stbl_GetSampleDepType(trak->Media->information->sampleTable->SampleDep, sampleNumber, &dependsOn, &dependedOn, &redundant);
		if (e) return e;

		GF_ISOM_RESET_FRAG_DEPEND_FLAGS(ent->flags);
		ent->flags |= GF_ISOM_GET_FRAG_DEPEND_FLAGS(0, dependsOn, dependedOn, redundant);
	}

	/*copy subsample info if any*/
	if ( gf_isom_sample_get_subsample_entry(orig, track, sampleNumber, &sub_sample)) {
		if (!traf || !traf->tfhd->sample_desc_index) return GF_BAD_PARAM;

		/*compute last sample number in traf*/
		last_sample = 0;
		count = gf_list_count(traf->TrackRuns);
		for (i=0; i<count; i++) {
			GF_TrackFragmentRunBox *trun = gf_list_get(traf->TrackRuns, i);
			last_sample += trun->sample_count;
		}

		/*create subsample if needed*/
		if (!traf->subs) {
			traf->subs = (GF_SubSampleInformationBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SUBS);
			traf->subs->version = 0;
		}


		count = gf_list_count(sub_sample->SubSamples);
		for (i=0; i<count; i++) {
			GF_SubSampleEntry *entry = gf_list_get(sub_sample->SubSamples, i);
			e = gf_isom_add_subsample_info(traf->subs, last_sample, entry->subsample_size, entry->subsample_priority, entry->reserved, entry->discardable);
			if (e) return e;
		}
	}
	/*copy sampleToGroup info if any*/
	if (trak->Media->information->sampleTable->sampleGroups) {
		count = gf_list_count(trak->Media->information->sampleTable->sampleGroups);
		for (i=0; i<count; i++) {
			GF_SampleGroupBox *sg;
			u32 j;
			u32 first_sample_in_entry, last_sample_in_entry;
			first_sample_in_entry = 1;

			sg = gf_list_get(trak->Media->information->sampleTable->sampleGroups, i);
			for (j=0; j<sg->entry_count; j++) {
				last_sample_in_entry = first_sample_in_entry + sg->sample_entries[j].sample_count - 1;
				if ((sampleNumber<first_sample_in_entry) || (sampleNumber>last_sample_in_entry)) {
					first_sample_in_entry = last_sample_in_entry+1;
					continue;
				}

				if (!traf->sampleGroups)
					traf->sampleGroups = gf_list_new();

				/*found our sample, add it to trak->sampleGroups*/
				e = gf_isom_copy_sample_group_entry_to_traf(traf, trak->Media->information->sampleTable, sg->grouping_type, sg->sample_entries[j].group_description_index, sgpd_in_traf);
				break;
			}
		}
	}
	return GF_OK;
}


#endif	/*GPAC_DISABLE_ISOM_WRITE*/

GF_EXPORT
u32 gf_isom_is_track_fragmented(GF_ISOFile *movie, u32 TrackID)
{
	if (!movie || !movie->moov || !movie->moov->mvex) return 0;
	return (GetTrex(movie->moov, TrackID) != NULL) ? 1 : 0;
}

GF_EXPORT
u32 gf_isom_is_fragmented(GF_ISOFile *movie)
{
	if (!movie || !movie->moov) return 0;
	/* By default if the Moov has an mvex, the file is fragmented */
	if (movie->moov->mvex) return 1;
	/* deprecated old code */
	/*only check moof number (in read mode, mvex can be deleted on the fly)*/
	//return movie->NextMoofNumber ? 1 : 0;
	return 0;
}

GF_EXPORT
GF_Err gf_isom_set_traf_base_media_decode_time(GF_ISOFile *movie, u32 TrackID, u64 decode_time)
{
	GF_TrackFragmentBox *traf;
	if (!movie || !movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	traf = GetTraf(movie, TrackID);
	if (!traf) return GF_BAD_PARAM;

	if (!traf->tfdt) {
		traf->tfdt = (GF_TFBaseMediaDecodeTimeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TFDT);
		if (!traf->tfdt) return GF_OUT_OF_MEM;
	}
	traf->tfdt->baseMediaDecodeTime = decode_time;
	return GF_OK;
}

#else

GF_Err gf_isom_finalize_for_fragment(GF_ISOFile *the_file, u32 media_segment_type)
{
	return GF_NOT_SUPPORTED;
}

GF_Err gf_isom_setup_track_fragment(GF_ISOFile *the_file, u32 TrackID,
                                    u32 DefaultSampleDescriptionIndex,
                                    u32 DefaultSampleDuration,
                                    u32 DefaultSampleSize,
                                    u8 DefaultSampleIsSync,
                                    u8 DefaultSamplePadding,
                                    u16 DefaultDegradationPriority)
{
	return GF_NOT_SUPPORTED;
}

GF_Err gf_isom_set_fragment_option(GF_ISOFile *the_file, u32 TrackID, u32 Code, u32 Param)
{
	return GF_NOT_SUPPORTED;
}

GF_Err gf_isom_start_fragment(GF_ISOFile *the_file, u32 free_data_insert_size)
{
	return GF_NOT_SUPPORTED;
}

GF_Err gf_isom_fragment_add_sample(GF_ISOFile *the_file, u32 TrackID, GF_ISOSample *sample, u32 DescIndex,
                                   u32 Duration, u8 PaddingBits, u16 DegradationPriority, Bool redCoded)
{
	return GF_NOT_SUPPORTED;
}


GF_EXPORT
u32 gf_isom_is_track_fragmented(GF_ISOFile *the_file, u32 TrackID)
{
	return 0;
}

GF_EXPORT
u32 gf_isom_is_fragmented(GF_ISOFile *the_file)
{
	return 0;
}

GF_Err gf_isom_fragment_add_subsample(GF_ISOFile *movie, u32 TrackID, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable)
{
	return GF_NOT_SUPPORTED;
}

GF_Err gf_isom_fragment_copy_subsample(GF_ISOFile *dest, u32 TrackID, GF_ISOFile *orig, u32 track, u32 sampleNumber)
{
	return GF_NOT_SUPPORTED;
}

GF_Err gf_isom_set_traf_base_media_decode_time(GF_ISOFile *movie, u32 TrackID, u64 decode_time)
{
	return GF_NOT_SUPPORTED;
}

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS)*/


void gf_isom_set_next_moof_number(GF_ISOFile *movie, u32 value)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (movie) movie->NextMoofNumber = value;
#endif
}

u32 gf_isom_get_next_moof_number(GF_ISOFile *movie)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (movie) return movie->NextMoofNumber;
#endif
	return 0;
}

#endif /*GPAC_DISABLE_ISOM*/
