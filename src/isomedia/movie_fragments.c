/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005 
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


#ifndef GPAC_DISABLE_ISOM_WRITE

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

GF_Err gf_isom_finalize_for_fragment(GF_ISOFile *movie, Bool use_segments)
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
		//update durations
		gf_isom_get_duration(movie);

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

	if (use_segments) {
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

	trex = GetTrex(movie->moov, TrackID);
	if (!trex) {
		trex = (GF_TrackExtendsBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TREX);
		trex->trackID = TrackID;
		mvex_AddBox((GF_Box*)mvex, (GF_Box *) trex);
	}
	trex->track = trak;
	trex->def_sample_desc_index = DefaultSampleDescriptionIndex;
	trex->def_sample_duration = DefaultSampleDuration;
	trex->def_sample_size = DefaultSampleSize;
	trex->def_sample_flags = GF_ISOM_FORMAT_FRAG_FLAGS(DefaultSamplePadding, DefaultSampleIsSync, DefaultDegradationPriority);

	return GF_OK;
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
		traf->tfhd->flags = 0;
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

Bool moof_get_rap_time_offset(GF_MovieFragmentBox *moof, u32 refTrackID, u32 *rap_delta)
{
	u32 i, j, delta;
	GF_TrunEntry *ent;
	GF_TrackFragmentBox *traf;
	GF_TrackFragmentRunBox *trun;
	*rap_delta = 0;
	for (i=0; i<gf_list_count(moof->TrackList); i++) {
		traf = gf_list_get(moof->TrackList, i);
		if (traf->tfhd->trackID==refTrackID) break;
		traf=NULL;
	}
	if (!traf) return 0;

	delta = 0;
	i=0;
	while ((trun = gf_list_enum(traf->TrackRuns, &i))) {
		if (trun->flags & GF_ISOM_TRUN_FIRST_FLAG) {
			if (GF_ISOM_GET_FRAG_SYNC(trun->flags)) {
				ent = gf_list_get(trun->entries, 0);
				*rap_delta = delta + ent->CTS_Offset;
				return 1;
			}
		}
		j=0;
		while ((ent = gf_list_enum(trun->entries, &j))) {
			if (GF_ISOM_GET_FRAG_SYNC(ent->flags)) {
				*rap_delta = delta + ent->CTS_Offset;
				return 1;
			}
			delta += ent->Duration;
		}
	}
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
			duration += ent->Duration;
		}
	}
	return duration;
}

u32 moof_get_earliest_cts(GF_MovieFragmentBox *moof, u32 refTrackID)
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
	u64 moof_start;
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
		movie->moof->mdat = gf_malloc(sizeof(char) * movie->moof->mdat_size);
		if (!movie->moof->mdat) return GF_OUT_OF_MEM;
		gf_bs_seek(bs, movie->segment_start);
		/*write mdat size*/
		gf_bs_write_u32(bs, (u32) movie->moof->mdat_size);
		gf_bs_seek(bs, movie->segment_start);
		gf_bs_read_data(bs, movie->moof->mdat, movie->moof->mdat_size);
		gf_bs_seek(bs, movie->segment_start);
		gf_bs_truncate(bs);
		return GF_OK;
	}


	moof_start = gf_bs_get_position(bs);
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


GF_Err gf_isom_close_segment(GF_ISOFile *movie, s32 frags_per_sidx, u32 referenceTrackID, u64 ref_track_decode_time, Bool daisy_chain_sidx, Bool last_segment)
{
	GF_SegmentIndexBox *sidx=NULL;
	GF_SegmentIndexBox *prev_sidx=NULL;
	GF_SegmentIndexBox *root_sidx=NULL;
	u64 sidx_start, sidx_end;
	Bool first_sidx = 0;
	Bool no_sidx = 0;
	u32 count, nb_subsegs=0, idx, cur_dur, sidx_dur, sidx_idx;
	u64 last_top_box_pos, root_prev_offset, local_sidx_start, local_sidx_end;
	GF_TrackBox *trak = NULL;
	GF_Err e;
	sidx_start = sidx_end = 0;
	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	count = gf_list_count(movie->moov->mvex->TrackExList);
	if (!count) return GF_BAD_PARAM;

	count = gf_list_count(movie->moof_list);
	if (!count) return GF_OK;
	/*store fragment*/
	if (movie->moof) {
		e = StoreFragment(movie, 1, 0, NULL);
		if (e) return e;
	}
	gf_bs_seek(movie->editFileMap->bs, movie->segment_start);
	gf_bs_truncate(movie->editFileMap->bs);

	if (referenceTrackID) {
		trak = gf_isom_get_track_from_id(movie->moov, referenceTrackID);

		/*modify brands STYP*/

		/*"msix" brand: this is a DASH Initialization Segment*/
		gf_isom_modify_alternate_brand(movie, GF_4CC('m','s','i','x'), 1);
		if (last_segment) {
			/*"lmsg" brand: this is the last DASH Segment*/
			gf_isom_modify_alternate_brand(movie, GF_4CC('l','m','s','g'), 1);
		}
	}
	/*write STYP*/
	movie->brand->type = GF_ISOM_BOX_TYPE_STYP;
    e = gf_isom_box_size((GF_Box *) movie->brand);
    if (e) return e;
    e = gf_isom_box_write((GF_Box *) movie->brand, movie->editFileMap->bs);
    if (e) return e;

	if (frags_per_sidx < 0) {
		referenceTrackID = 0;
		frags_per_sidx = 0;
		no_sidx = 1;
	}

	/*prepare SIDX: we write a blank SIDX box with the right number of entries, and will rewrite it later on*/
	if (referenceTrackID) {
		Bool is_root_sidx=0;
		sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
		sidx->reference_ID = referenceTrackID;
		sidx->timescale = trak->Media->mediaHeader->timeScale;
		/*we don't write anything between sidx and following moov*/
		sidx->first_offset = 0;
		sidx->earliest_presentation_time = ref_track_decode_time;

		/*we consider that each fragment is a subsegment - this could be controled by another parameter*/
		if (!frags_per_sidx) {
			nb_subsegs = 1;
			frags_per_sidx = count;
		} else {
			nb_subsegs = count/frags_per_sidx;
			if (nb_subsegs*frags_per_sidx<count) nb_subsegs++;
		}

		/*single SIDX, reference all fragments*/
		if (nb_subsegs==1) {
			sidx->nb_refs = frags_per_sidx;
			daisy_chain_sidx = 0;
		} 
		/*daisy-chain SIDX, reference all fragments plus next */
		else if (daisy_chain_sidx) {
			sidx->nb_refs = frags_per_sidx + 1;
			/*we will have to adjust earliest cpresentation time*/
			first_sidx = 1;
		}
		/*root SIDX referencing all subsegments*/
		else {
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
	
		count = idx = 0;

		if (is_root_sidx) {
			root_sidx = sidx;
			sidx = NULL;
		}
	}


	sidx_idx = 0;
	sidx_dur = 0;
	last_top_box_pos = root_prev_offset = sidx_end;
	local_sidx_start = local_sidx_end = 0;

	/*cumulated segments duration since start of the sidx */
	cur_dur = 0;
	e = GF_OK;
	while (gf_list_count(movie->moof_list)) {
		s32 offset_diff;
		u32 moof_size;
		u32 dur;

		movie->moof = gf_list_get(movie->moof_list, 0);
		gf_list_rem(movie->moof_list, 0);

		if (!root_sidx && sidx && first_sidx) {
			first_sidx = 0;
			sidx->earliest_presentation_time = ref_track_decode_time + moof_get_earliest_cts(movie->moof, referenceTrackID);
		}

		/*hierarchical or daisy-chain SIDXs*/
		if (!no_sidx && !sidx && (root_sidx || daisy_chain_sidx) ) {
			sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
			sidx->reference_ID = referenceTrackID;
			sidx->timescale = trak->Media->mediaHeader->timeScale;
			sidx->earliest_presentation_time = ref_track_decode_time + sidx_dur + moof_get_earliest_cts(movie->moof, referenceTrackID);

			/*we don't write anything between sidx and following moov*/
			sidx->first_offset = 0;
			sidx->nb_refs = frags_per_sidx;
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
		}

		offset_diff = (s32) (gf_bs_get_position(movie->editFileMap->bs) - movie->moof->fragment_offset);
		movie->moof->fragment_offset = gf_bs_get_position(movie->editFileMap->bs);

		if (!e) {
			e = StoreFragment(movie, 0, offset_diff, &moof_size);

			if (sidx) {
				/*we refer to next moof*/
				sidx->refs[idx].reference_type = 0;
				sidx->refs[idx].contains_RAP = moof_get_rap_time_offset(movie->moof, referenceTrackID, & sidx->refs[idx].RAP_delta_time);
				if (sidx->refs[idx].contains_RAP) {
					sidx->refs[idx].RAP_delta_time += cur_dur;

					if (root_sidx && !root_sidx->refs[sidx_idx].contains_RAP) {
						root_sidx->refs[sidx_idx].contains_RAP = 1;
						root_sidx->refs[sidx_idx].RAP_delta_time = sidx->refs[idx].RAP_delta_time + sidx_dur;
					}
					else if (prev_sidx && !prev_sidx->refs[prev_sidx->nb_refs - 1].contains_RAP) {
						prev_sidx->refs[prev_sidx->nb_refs - 1].contains_RAP = 1;
						prev_sidx->refs[prev_sidx->nb_refs - 1].RAP_delta_time = sidx->refs[idx].RAP_delta_time;
					}

				}
				
				dur = moof_get_duration(movie->moof, referenceTrackID);
				sidx->refs[idx].subsegment_duration += dur;
				cur_dur += dur;
				/*reference size is end of the moof we just wrote minus last_box_pos*/
				sidx->refs[idx].reference_size = (u32) ( gf_bs_get_position(movie->editFileMap->bs) - last_top_box_pos) ;
				last_top_box_pos = gf_bs_get_position(movie->editFileMap->bs);
				idx++;

				count++;
				/*switching to next SIDX*/
				if (count==frags_per_sidx) {

					if (root_sidx) {
						root_sidx->refs[sidx_idx].reference_size = (u32) (gf_bs_get_position(movie->editFileMap->bs) - local_sidx_start);
						root_sidx->refs[sidx_idx].subsegment_duration = cur_dur;
						if (!sidx_idx) {
							root_sidx->earliest_presentation_time = sidx->earliest_presentation_time;
						}

						sidx_rewrite(sidx, movie->editFileMap->bs, local_sidx_start);
						gf_isom_box_del((GF_Box*)sidx);
						sidx = NULL;
					} else if (daisy_chain_sidx) {
						if (prev_sidx) {
							if (prev_sidx->refs[prev_sidx->nb_refs - 1].contains_RAP) {
								prev_sidx->refs[prev_sidx->nb_refs - 1].contains_RAP = 1;
								prev_sidx->refs[prev_sidx->nb_refs - 1].RAP_delta_time += sidx_dur;
							}
							prev_sidx->refs[prev_sidx->nb_refs - 1].subsegment_duration = sidx_dur;
							prev_sidx->refs[prev_sidx->nb_refs - 1].reference_size = (u32) (gf_bs_get_position(movie->editFileMap->bs) - local_sidx_start);
							prev_sidx->refs[prev_sidx->nb_refs - 1].reference_type = 1;
							sidx_rewrite(prev_sidx, movie->editFileMap->bs, sidx_start);
							gf_isom_box_del((GF_Box*)prev_sidx);

							sidx_start = local_sidx_start;
							sidx_end = local_sidx_end;
						}
						nb_subsegs--;
						prev_sidx = sidx;
						sidx = NULL;
					}
					sidx_dur += cur_dur;
					cur_dur = 0;
					count = 0;
					idx=0;
					sidx_idx++;
				}
			}
		} 
		gf_isom_box_del((GF_Box *) movie->moof);	
		movie->moof = NULL;
	}

	if (sidx) {
		sidx_rewrite(sidx, movie->editFileMap->bs, sidx_start);
		gf_isom_box_del((GF_Box*)sidx);
	}
	if (prev_sidx) {
		sidx_rewrite(prev_sidx, movie->editFileMap->bs, sidx_start);
		gf_isom_box_del((GF_Box*)prev_sidx);
	}
	if (root_sidx) {
		sidx_rewrite(root_sidx, movie->editFileMap->bs, sidx_start);
		gf_isom_box_del((GF_Box*)root_sidx);
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

GF_Err gf_isom_close_fragments(GF_ISOFile *movie)
{
	if (movie->use_segments) {
		return gf_isom_close_segment(movie, 0, 0, 0, 0, 1);
	} else {
		return StoreFragment(movie, 0, 0, NULL);
	}
}

GF_Err gf_isom_start_segment(GF_ISOFile *movie, char *SegName)
{
	GF_Err e;
	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	if (gf_list_count(movie->moof_list)) 
		return GF_BAD_PARAM;

	movie->append_segment = 0;
	/*update segment file*/
	if (SegName) {
		gf_isom_datamap_del(movie->editFileMap);
		e = gf_isom_datamap_new(SegName, NULL, GF_ISOM_DATA_MAP_WRITE, & movie->editFileMap);
		movie->segment_start = 0;
		return e;
	}	
	assert(gf_list_count(movie->moof_list) == 0);
	movie->segment_start = gf_bs_get_position(movie->editFileMap->bs);
	/*if movieFileMap is not null, we are concatenating segments to the original move, force a copy*/
	if (movie->movieFileMap)
		movie->append_segment = 1;
	return GF_OK;
}


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
 	
    /* Inserting free data for testing HTTP streaming */
#if 0
	if (0) {
        GF_FreeSpaceBox *fb = (GF_FreeSpaceBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_FREE);
        fb->dataSize = free_data_insert_size;
        //gf_list_add(movie->TopBoxes, fb);
	    e = gf_isom_box_size((GF_Box *) fb);
	    if (e) return e;
	    e = gf_isom_box_write((GF_Box *) fb, movie->editFileMap->bs);
	    if (e) return e;
        gf_isom_box_del((GF_Box *)fb);
    }
#endif

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


GF_Err gf_isom_fragment_add_sample(GF_ISOFile *movie, u32 TrackID, GF_ISOSample *sample, u32 DescIndex, 
								 u32 Duration,
								 u8 PaddingBits, u16 DegradationPriority)
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
	gf_list_add(trun->entries, ent);
	
	trun->sample_count += 1;

	//rewrite OD frames
	if (traf->trex->track->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		//this may fail if depandancies are not well done ...
		Media_ParseODFrame(traf->trex->track->Media, sample, &od_sample);
		sample = od_sample;
	}

	//finally write the data
	if (!traf->DataCache) {
		gf_bs_write_data(movie->editFileMap->bs, sample->data, sample->dataLength);
	} else if (trun->cache) {
		gf_bs_write_data(trun->cache, sample->data, sample->dataLength);
	} else {
		return GF_BAD_PARAM;
	}
	if (od_sample) gf_isom_sample_del(&od_sample);
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

GF_Err gf_isom_fragment_copy_subsample(GF_ISOFile *dest, u32 TrackID, GF_ISOFile *orig, u32 track, u32 sampleNumber)
{
	u32 i, count, last_sample;
	GF_SampleEntry *sub_sample;
	GF_Err e;
	GF_TrackFragmentBox *traf;
	if (!dest->moof || !(dest->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	if (! gf_isom_sample_get_subsample_entry(orig, track, sampleNumber, &sub_sample)) return GF_OK;

	traf = GetTraf(dest, TrackID);
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

GF_Err gf_isom_finalize_for_fragment(GF_ISOFile *the_file)
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
								 u32 Duration,
								 u8 PaddingBits, u16 DegradationPriority)
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
