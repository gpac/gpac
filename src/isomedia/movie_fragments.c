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

#ifndef	GPAC_ISOM_NO_FRAGMENTS

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


#ifndef GPAC_READ_ONLY

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

GF_Err gf_isom_finalize_for_fragment(GF_ISOFile *movie)
{
	GF_Err e;
	u32 i;
	GF_TrackExtendsBox *trex;
	if (!movie || !movie->moov) return GF_BAD_PARAM;
	//this is only allowed in write mode
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	if (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) return GF_OK;
	movie->FragmentsFlags = 0;

	//update durations
	gf_isom_get_duration(movie);

	//write movie
	e = WriteToFile(movie);
	if (e) return e;

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
	movie->NextMoofNumber = 1;
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


u32 UpdateRuns(GF_TrackFragmentBox *traf)
{
	u32 sampleCount, i, j, RunSize, UseDefaultSize, RunDur, UseDefaultDur, RunFlags, NeedFlags, UseDefaultFlag, UseCTS, count;
	GF_TrackFragmentRunBox *trun;
	GF_TrunEntry *ent, *first_ent;
	
	sampleCount = 0;


	//traf data offset - we ALWAYS use data offset indication when writting otherwise
	//we would need to have one TRUN max in a TRAF for offset reconstruction or store
	//all TRUN in memory before writting :( Anyway it is much safer to indicate the
	//base offset of each traf rather than using offset aggregation rules specified 
	//in the std
	traf->tfhd->flags = GF_ISOM_TRAF_BASE_OFFSET;

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



GF_Err StoreFragment(GF_ISOFile *movie)
{
	GF_Err e;
	u64 moof_start;
	u32 size, i, s_count;
	char *buffer;
	GF_TrackFragmentBox *traf;
	GF_TrackFragmentRunBox *trun;
	GF_BitStream *bs;
	if (!movie->moof) return GF_OK;

	bs = movie->editFileMap->bs;

	//1- flush all caches
	i=0;
	while ((traf = (GF_TrackFragmentBox*)gf_list_enum(movie->moof->TrackList, &i))) {
		if (!traf->DataCache) continue;
		s_count = gf_list_count(traf->TrackRuns);
		if (!s_count) continue;
		trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, s_count-1);
		if (!trun->cache || !trun->sample_count) continue;
		
		//update offset
		trun->data_offset = (u32) (gf_bs_get_position(movie->editFileMap->bs) - movie->current_top_box_start - 8);
		//write cache
		gf_bs_get_content(trun->cache, &buffer, &size);
		gf_bs_write_data(movie->editFileMap->bs, buffer, size);
		gf_bs_del(trun->cache);
		free(buffer);
		trun->cache = NULL;
	}
	//2- update MOOF MDAT header
	moof_start = gf_bs_get_position(bs);

	//start of MDAT
	gf_bs_seek(bs, movie->current_top_box_start);
	//we assume we never write large MDATs in fragment mode which should always be true
	size = (u32) (moof_start - movie->current_top_box_start);
	gf_bs_write_u32(bs, (u32) size);
	gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_MDAT);
	gf_bs_seek(bs, moof_start);

	//3- clean our traf's
	i=0;
	while ((traf = (GF_TrackFragmentBox*) gf_list_enum(movie->moof->TrackList, &i))) {
		//compute default settings for the TRAF
		ComputeFragmentDefaults(traf);
		//updates all trun and set all flags, INCLUDING TRAF FLAGS (durations, ...)
		s_count = UpdateRuns(traf);
		//empty fragment destroy it
		if (!traf->tfhd->EmptyDuration && !s_count) {
			i--;
			gf_list_rem(movie->moof->TrackList, i);
			gf_isom_box_del((GF_Box *) traf);
			continue;
		}
	}

	//4- Write moof
	e = gf_isom_box_size((GF_Box *) movie->moof);
	if (e) return e;
	e = gf_isom_box_write((GF_Box *) movie->moof, bs);
	if (e) return e;
	
	//5- destroy our moof
	gf_isom_box_del((GF_Box *) movie->moof);
	movie->moof = NULL;
	movie->NextMoofNumber ++;
	return GF_OK;
}


GF_Err gf_isom_start_fragment(GF_ISOFile *movie)
{
	u32 i, count;
	GF_TrackExtendsBox *trex;
	GF_TrackFragmentBox *traf;
	GF_Err e;
	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	count = gf_list_count(movie->moov->mvex->TrackExList);
	if (!count) return GF_BAD_PARAM;

	//store fragment
	if (movie->moof) {
		e = StoreFragment(movie);
		if (e) return e;
	}
 	
	//format MDAT
	movie->current_top_box_start = gf_bs_get_position(movie->editFileMap->bs);
	gf_bs_write_u32(movie->editFileMap->bs, 0);
	gf_bs_write_u32(movie->editFileMap->bs, GF_ISOM_BOX_TYPE_MDAT);

	//create new fragment
	movie->moof = (GF_MovieFragmentBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MOOF);
	movie->moof->mfhd = (GF_MovieFragmentHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MFHD);
	movie->moof->mfhd->sequence_number = movie->NextMoofNumber;
	
	//we create a TRAF for each setup track, unused ones will be removed at store time
	for (i=0; i<count; i++) {
		trex = (GF_TrackExtendsBox*)gf_list_get(movie->moov->mvex->TrackExList, i);
		traf = (GF_TrackFragmentBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TRAF);
		traf->trex = trex;
		traf->tfhd = (GF_TrackFragmentHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TFHD);
		traf->tfhd->trackID = trex->trackID;
		//add 8 bytes (MDAT size+type) to avoid the data_offset in the first trun
		traf->tfhd->base_data_offset = movie->current_top_box_start + 8;
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
	if (!movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) || !sample) return GF_BAD_PARAM;

	traf = GetTraf(movie, TrackID);
	if (!traf) return GF_BAD_PARAM;

	if (!traf->tfhd->sample_desc_index) traf->tfhd->sample_desc_index = DescIndex ? DescIndex : traf->trex->def_sample_desc_index;

	pos = gf_bs_get_position(movie->editFileMap->bs);

	
	//WARNING: we change stream description, create a new TRAF
	if ( DescIndex && (traf->tfhd->sample_desc_index != DescIndex)) {
		//if we're caching flush the current run
		if (traf->DataCache) {
			count = gf_list_count(traf->TrackRuns);
			if (count) {
				trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
				trun->data_offset = (u32) (pos - movie->current_top_box_start - 8);
				gf_bs_get_content(trun->cache, &buffer, &buffer_size);
				gf_bs_write_data(movie->editFileMap->bs, buffer, buffer_size);
				gf_bs_del(trun->cache);
				trun->cache = NULL;
				free(buffer);
			}
		}
		traf_2 = (GF_TrackFragmentBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TRAF);
		traf_2->trex = traf->trex;
		traf_2->tfhd = (GF_TrackFragmentHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TFHD);
		traf_2->tfhd->trackID = traf->tfhd->trackID;
		//keep the same offset
		traf_2->tfhd->base_data_offset = movie->current_top_box_start + 8;
		gf_list_add(movie->moof->TrackList, traf_2);

		//duplicate infos
		traf_2->tfhd->IFrameSwitching = traf->tfhd->IFrameSwitching;
		traf_2->DataCache  = traf->DataCache;
		traf_2->tfhd->sample_desc_index  = DescIndex;

		//switch them ...
		traf = traf_2;
	}


	//add TRUN entry
	count = gf_list_count(traf->TrackRuns);
	if (count) {
		trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
		//check data offset when no caching as trun entries shall ALWAYS be contiguous samples
		if (!traf->DataCache && (movie->current_top_box_start + 8 + trun->data_offset + GetRunSize(trun) != pos) )
			count = 0;
		
		//check I-frame detection
		if (traf->tfhd->IFrameSwitching && sample->IsRAP) 
			count = 0;

		if (traf->DataCache && (traf->DataCache==trun->sample_count) )
			count = 0;

		//if data cache is on and we're changing TRUN, store the cache and update data offset
		if (!count && traf->DataCache) {
			trun->data_offset = (u32) (pos - movie->current_top_box_start - 8);
			gf_bs_get_content(trun->cache, &buffer, &buffer_size);
			gf_bs_write_data(movie->editFileMap->bs, buffer, buffer_size);
			gf_bs_del(trun->cache);
			trun->cache = NULL;
			free(buffer);
		}
	}

	//new run
	if (!count) {
		trun = (GF_TrackFragmentRunBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TRUN);
		//store data offset (we have the 8 btyes offset of the MDAT)
		trun->data_offset = (u32) (pos - movie->current_top_box_start - 8);
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


#endif	//GPAC_READ_ONLY

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
	/*only check moof number (in read mode, mvex can be deleted on the fly)*/
	return movie->NextMoofNumber ? 1 : 0;
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

GF_Err gf_isom_start_fragment(GF_ISOFile *the_file)
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

#endif
