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
#include <gpac/network.h>

/**************************************************************
		Some Local functions for movie creation
**************************************************************/
GF_Err gf_isom_parse_root_box(GF_Box **outBox, GF_BitStream *bs, u64 *bytesExpected);

#ifndef	GPAC_ISOM_NO_FRAGMENTS

GF_Err MergeFragment(GF_MovieFragmentBox *moof, GF_ISOFile *mov)
{
	u32 i, j;
	u64 MaxDur;
	GF_TrackFragmentBox *traf;
	GF_TrackBox *trak;

	GF_Err MergeTrack(GF_TrackBox *trak, GF_TrackFragmentBox *traf, u64 *moof_offset);
	
	MaxDur = 0;

	//we shall have a MOOV and its MVEX BEFORE any MOOF
	if (!mov->moov || !mov->moov->mvex) return GF_ISOM_INVALID_FILE;
	//and all fragments must be continous
	if (mov->NextMoofNumber + 1 != moof->mfhd->sequence_number) return GF_ISOM_INVALID_FILE;

	i=0;
	while ((traf = (GF_TrackFragmentBox*)gf_list_enum(moof->TrackList, &i))) {
		if (!traf->tfhd) {
			trak = NULL;
			traf->trex = NULL;
		} else {
			trak = gf_isom_get_track_from_id(mov->moov, traf->tfhd->trackID);
			j=0;
			while ((traf->trex = (GF_TrackExtendsBox*)gf_list_enum(mov->moov->mvex->TrackExList, &j))) {
				if (traf->trex->trackID == traf->tfhd->trackID) break;
				traf->trex = NULL;
			}
		}
		if (!trak || !traf->trex) return GF_ISOM_INVALID_FILE;

		//NB we can modify the movie data-offset info since we are in the middle of
		//parsing an box, so next box readin will reset it...
		MergeTrack(trak, traf, &mov->current_top_box_start);

		//update trak duration
		SetTrackDuration(trak);
		if (trak->Header->duration > MaxDur) 
			MaxDur = trak->Header->duration;
	}

	mov->NextMoofNumber += 1;
	//update movie duration
	if (mov->moov->mvhd->duration < MaxDur) mov->moov->mvhd->duration = MaxDur;
	return GF_OK;
}

#endif


GF_Err gf_isom_parse_movie_boxes(GF_ISOFile *mov, u64 *bytesMissing)
{
	GF_Box *a;
	u64 totSize;
	GF_Err e = GF_OK;

	totSize = 0;


#ifndef	GPAC_ISOM_NO_FRAGMENTS
	/*restart from where we stoped last*/
	totSize = mov->current_top_box_start;
	gf_bs_seek(mov->movieFileMap->bs, mov->current_top_box_start);
#endif


	/*while we have some data, parse our boxes*/
	while (gf_bs_available(mov->movieFileMap->bs)) {
		*bytesMissing = 0;
#ifndef	GPAC_ISOM_NO_FRAGMENTS
		mov->current_top_box_start = gf_bs_get_position(mov->movieFileMap->bs);
#endif

		e = gf_isom_parse_root_box(&a, mov->movieFileMap->bs, bytesMissing);

		if (e >= 0) {
			e = GF_OK;
		} else if (e == GF_ISOM_INCOMPLETE_FILE) {
			/*our mdat is uncomplete, only valid for READ ONLY files...*/
			if (mov->openMode != GF_ISOM_OPEN_READ) {
				return GF_ISOM_INVALID_FILE;
			}
			return e;
		} else {
			return e;
		}

		switch (a->type) {
		/*MOOV box*/
		case GF_ISOM_BOX_TYPE_MOOV:
			if (mov->moov) return GF_ISOM_INVALID_FILE;
			mov->moov = (GF_MovieBox *)a;
			/*set our pointer to the movie*/
			mov->moov->mov = mov;
			e = gf_list_add(mov->TopBoxes, a);
			if (e) return e;
			totSize += a->size;
			break;

		/*META box*/
		case GF_ISOM_BOX_TYPE_META:
			if (mov->meta) return GF_ISOM_INVALID_FILE;
			mov->meta = (GF_MetaBox *)a;
			e = gf_list_add(mov->TopBoxes, a);
			if (e) return e;
			totSize += a->size;
			break;

		/*we only keep the MDAT in READ for dump purposes*/
		case GF_ISOM_BOX_TYPE_MDAT:
			totSize += a->size;
			if (mov->openMode == GF_ISOM_OPEN_READ) {
				if (!mov->mdat) {
					mov->mdat = (GF_MediaDataBox *) a;
					e = gf_list_add(mov->TopBoxes, mov->mdat);
					if (e) return e;
				}
#ifndef	GPAC_ISOM_NO_FRAGMENTS
				else if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) gf_list_add(mov->TopBoxes, a);
#endif
				else gf_isom_box_del(a);
			}
			/*if we don't have any MDAT yet, create one (edit-write mode)
			We only work with one mdat, but we're puting it at the place
			of the first mdat found when opening a file for editing*/
			else if (!mov->mdat && (mov->openMode != GF_ISOM_OPEN_READ)) {
				gf_isom_box_del(a);
				mov->mdat = (GF_MediaDataBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MDAT);
				e = gf_list_add(mov->TopBoxes, mov->mdat);
				if (e) return e;
			} else {
				gf_isom_box_del(a);
			}
			break;
		case GF_ISOM_BOX_TYPE_FTYP:
			/*ONE AND ONLY ONE FTYP*/
			if (mov->brand) {
				gf_isom_box_del(a);
				return GF_ISOM_INVALID_FILE;
			}
			mov->brand = (GF_FileTypeBox *)a;
			totSize += a->size;
			e = gf_list_add(mov->TopBoxes, a);
			break;

		case GF_ISOM_BOX_TYPE_PDIN:
			/*ONE AND ONLY ONE PDIN*/
			if (mov->pdin) {
				gf_isom_box_del(a);
				return GF_ISOM_INVALID_FILE;
			}
			mov->pdin = (GF_ProgressiveDownloadBox *) a;
			totSize += a->size;
			e = gf_list_add(mov->TopBoxes, a);
			break;


#ifndef	GPAC_ISOM_NO_FRAGMENTS
		case GF_ISOM_BOX_TYPE_MOOF:
			((GF_MovieFragmentBox *)a)->mov = mov;

			totSize += a->size;
			/*read & debug: store at root level*/
			if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) {
				gf_list_add(mov->TopBoxes, a);
			} else {
				/*merge all info*/
				e = MergeFragment((GF_MovieFragmentBox *)a, mov);
				gf_isom_box_del(a);
			}
			break;
#endif
		case GF_4CC('j','P',' ',' '):
		{
			GF_UnknownBox *box = (GF_UnknownBox*)a;
			u8 *c = box->data;
			if ((box->dataSize==4) 
				&& (GF_4CC(c[0],c[1],c[2],c[3])==(u32)0x0D0A870A)) 
				 mov->is_jp2 = 1;

			gf_isom_box_del(a);
		}
			break;

		default:
			totSize += a->size;
			e = gf_list_add(mov->TopBoxes, a);
			break;
		}
	}

	/*we need at least moov or meta*/
	if (!mov->moov && !mov->meta) return GF_ISOM_INVALID_FILE;
	/*we MUST have movie header*/
	if (mov->moov && !mov->moov->mvhd) return GF_ISOM_INVALID_FILE;
	/*we MUST have meta handler*/
	if (mov->meta && !mov->meta->handler) return GF_ISOM_INVALID_FILE;

#ifndef GPAC_READ_ONLY

	if (mov->moov) {
		/*set the default interleaving time*/
		mov->interleavingTime = mov->moov->mvhd->timeScale;

#ifndef	GPAC_ISOM_NO_FRAGMENTS
		/*not in open mode and successfully loaded the entire file, destroy all fragment
		FIXME: we may need to keet it when trying http streaming of fragments...*/
		if (!(mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) && mov->moov->mvex) {
			gf_isom_box_del((GF_Box *)mov->moov->mvex);
			mov->moov->mvex = NULL;
		}
#endif

	}
#endif

	return GF_OK;
}


GF_ISOFile *gf_isom_new_movie()
{
	GF_ISOFile *mov = (GF_ISOFile*)malloc(sizeof(GF_ISOFile));
	if (mov == NULL) {
		gf_isom_set_last_error(NULL, GF_OUT_OF_MEM);
		return NULL;
	}
	memset(mov, 0, sizeof(GF_ISOFile));

	/*init the boxes*/
	mov->TopBoxes = gf_list_new();
	if (!mov->TopBoxes) {
		gf_isom_set_last_error(NULL, GF_OUT_OF_MEM);
		free(mov);
		return NULL;
	}
	
	/*default storage mode is flat*/
	mov->storageMode = GF_ISOM_STORE_FLAT;
	return mov;
}

//Create and parse the movie for READ - EDIT only
GF_ISOFile *gf_isom_open_file(const char *fileName, u32 OpenMode, const char *tmp_dir)
{
	GF_Err e;
	u64 bytes;
	GF_ISOFile *mov = gf_isom_new_movie();
	if (! mov) return NULL;

	mov->fileName = strdup(fileName);
	mov->openMode = OpenMode;

	if ( (OpenMode == GF_ISOM_OPEN_READ) || (OpenMode == GF_ISOM_OPEN_READ_DUMP) ) {
		//always in read ...
		mov->openMode = GF_ISOM_OPEN_READ;
		mov->es_id_default_sync = -1;
		//for open, we do it the regular way and let the GF_DataMap assign the appropriate struct
		//this can be FILE (the only one supported...) as well as remote 
		//(HTTP, ...),not suported yet
		//the bitstream IS PART OF the GF_DataMap
		//as this is read-only, use a FileMapping. this is the only place where 
		//we use file mapping
		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_READ_ONLY, &mov->movieFileMap);
		if (e) {
			gf_isom_set_last_error(NULL, e);
			gf_isom_delete_movie(mov);
			return NULL;
		}

#ifndef	GPAC_ISOM_NO_FRAGMENTS
		if (OpenMode == GF_ISOM_OPEN_READ_DUMP) mov->FragmentsFlags |= GF_ISOM_FRAG_READ_DEBUG;
#endif

	} else {

#ifdef GPAC_READ_ONLY
		//not allowed for READ_ONLY lib
		gf_isom_delete_movie(mov);
		gf_isom_set_last_error(NULL, GF_ISOM_INVALID_MODE);
		return NULL;

#else

		//set a default output name for edited file
		mov->finalName = (char*)malloc(strlen(fileName) + 5);
		if (!mov->finalName) {
			gf_isom_set_last_error(NULL, GF_OUT_OF_MEM);
			gf_isom_delete_movie(mov);
			return NULL;
		}
		strcpy(mov->finalName, "out_");
		strcat(mov->finalName, fileName);

		//open the original file with edit tag
		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_EDIT, &mov->movieFileMap);
		//if the file doesn't exist, we assume it's wanted and create one from scratch
		if (e) {
			gf_isom_set_last_error(NULL, e);
			gf_isom_delete_movie(mov);
			return NULL;
		}
		//and create a temp fileName for the edit
		e = gf_isom_datamap_new("mp4_tmp_edit", tmp_dir, GF_ISOM_DATA_MAP_WRITE, & mov->editFileMap);
		if (e) {
			gf_isom_set_last_error(NULL, e);
			gf_isom_delete_movie(mov);
			return NULL;
		}
		mov->es_id_default_sync = -1;

#endif
	}

	//OK, let's parse the movie...
	mov->LastError = gf_isom_parse_movie_boxes(mov, &bytes);
	if (mov->LastError) {
		gf_isom_set_last_error(NULL, mov->LastError);
		gf_isom_delete_movie(mov);
		return NULL;
	}
	return mov;
}


u64 gf_isom_get_mp4time()
{
	u32 calctime, msec;
	u64 ret;
	gf_utc_time_since_1970(&calctime, &msec);
	calctime += GF_ISOM_MAC_TIME_OFFSET;
	ret = calctime;
	return ret;
}

void gf_isom_delete_movie(GF_ISOFile *mov)
{
	//these are our two main files
	if (mov->movieFileMap) gf_isom_datamap_del(mov->movieFileMap);

#ifndef GPAC_READ_ONLY
	if (mov->editFileMap) {
		gf_isom_datamap_del(mov->editFileMap);
	}
	if (mov->finalName) free(mov->finalName);
#endif

	gf_isom_box_array_del(mov->TopBoxes);

	if (mov->fileName) free(mov->fileName);
	free(mov);
}

GF_TrackBox *gf_isom_get_track_from_id(GF_MovieBox *moov, u32 trackID) 
{
	u32 i, count;
	GF_TrackBox *trak;
	if (!moov || !trackID) return NULL;

	count = gf_list_count(moov->trackList);
	for (i = 0; i<count; i++) {
		trak = (GF_TrackBox*)gf_list_get(moov->trackList, i);
		if (trak->Header->trackID == trackID) return trak;
	}
	return NULL;
}

GF_TrackBox *gf_isom_get_track_from_file(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	if (!movie) return NULL;
	trak = gf_isom_get_track(movie->moov, trackNumber);
	if (!trak) movie->LastError = GF_BAD_PARAM;
	return trak;
}


//WARNING: MOVIETIME IS EXPRESSED IN MEDIA TS
GF_Err GetMediaTime(GF_TrackBox *trak, u64 movieTime, u64 *MediaTime, s64 *SegmentStartTime, s64 *MediaOffset, u8 *useEdit)
{
	GF_Err e;
	u32 i;
	u64 time, lastSampleTime, m_time;
	s64 mtime;
	GF_EdtsEntry *ent;
	Double scale_ts;
	u32 sampleNumber, prevSampleNumber;
	u64 firstDTS;
	GF_SampleTableBox *stbl = trak->Media->information->sampleTable;

	*useEdit = 1;
	*MediaTime = 0;
	//no segment yet...
	*SegmentStartTime = -1;
	*MediaOffset = -1;
	if (!trak->moov->mvhd->timeScale || !trak->Media->mediaHeader->timeScale) {
		return GF_ISOM_INVALID_FILE;
	}

	//no samples...
	if (!stbl->SampleSize->sampleCount) {
		lastSampleTime = 0;
	} else {
		lastSampleTime = trak->Media->mediaHeader->duration;
	}

	//No edits, 1 to 1 mapping
	if (! trak->editBox || !trak->editBox->editList) {
		*MediaTime = movieTime;
		//check this is in our media time line
		if (*MediaTime > lastSampleTime) *MediaTime = lastSampleTime;
		*useEdit = 0;
		return GF_OK;
	}
	//browse the edit list and get the time
	scale_ts = trak->moov->mvhd->timeScale;
	scale_ts /= trak->Media->mediaHeader->timeScale;
	scale_ts *= ((s64)movieTime + 1);
	m_time = (u64) (scale_ts);

	time = 0;
	ent = NULL;
	i=0;
	while ((ent = (GF_EdtsEntry *)gf_list_enum(trak->editBox->editList->entryList, &i))) {
		if (time + ent->segmentDuration > m_time) {
			goto ent_found;
		}
		time += ent->segmentDuration;
	}
	//we had nothing in the list (strange file but compliant...)
	//return the 1 to 1 mapped vale of the last media sample
	if (!ent) {
		*MediaTime = movieTime;
		//check this is in our media time line
		if (*MediaTime > lastSampleTime) *MediaTime = lastSampleTime;
		*useEdit = 0;
		return GF_OK;
	}
	//request for a bigger time that what we can give: return the last sample (undefined behavior...)
	*MediaTime = lastSampleTime;
	return GF_OK;

ent_found:
	//OK, we found our entry, set the SegmentTime
	*SegmentStartTime = time;

	//we request an empty list, there's no media here...
	if (ent->mediaTime < 0) {
		*MediaTime = 0;
		return GF_OK;
	}
	//we request a dwell edit
	if (! ent->mediaRate) {
		*MediaTime = ent->mediaTime;
		//no media offset
		*MediaOffset = 0;
		*useEdit = 2;
		return GF_OK;
	}
	
	/*WARNING: this can be "-1" when doing searchForward mode (to prevent jumping to next entry)*/
	mtime = ent->mediaTime + movieTime - (time * trak->Media->mediaHeader->timeScale / trak->moov->mvhd->timeScale);
	if (mtime<0) mtime = 0;
	*MediaTime = (u64) mtime;

#if 0
	//
	//Sanity check: is the requested time valid ? This is to cope with wrong EditLists
	//we have the translated time, but we need to make sure we have a sample at this time ...
	//we have to find a COMPOSITION time
	e = findEntryForTime(stbl, (u32) *MediaTime, 1, &sampleNumber, &prevSampleNumber);
	if (e) return e;
	
	//first case: our time is after the last sample DTS (it's a broken editList somehow)
	//set the media time to the last sample
	if (!sampleNumber && !prevSampleNumber) {
		*MediaTime = lastSampleTime;
		return GF_OK;
	}
	//get the appropriated sample
	if (!sampleNumber) sampleNumber = prevSampleNumber;

	stbl_GetSampleDTS(stbl->TimeToSample, sampleNumber, &DTS);
	CTS = 0;
	if (stbl->CompositionOffset) stbl_GetSampleCTS(stbl->CompositionOffset, sampleNumber, &CTS);
#endif

	//now get the entry sample (the entry time gives the CTS, and we need the DTS
	e = findEntryForTime(stbl, (u32) ent->mediaTime, 1, &sampleNumber, &prevSampleNumber);
	if (e) return e;

	//oops, the mediaTime indicates a sample that is not in our media !
	if (!sampleNumber && !prevSampleNumber) {
		*MediaTime = lastSampleTime;
		return GF_ISOM_INVALID_FILE;
	}
	if (!sampleNumber) sampleNumber = prevSampleNumber;

	stbl_GetSampleDTS(stbl->TimeToSample, sampleNumber, &firstDTS);

	//and store the "time offset" of the desired sample in this segment
	//this is weird, used to rebuild the timeStamp when reading from the track, not the
	//media ...
	*MediaOffset = firstDTS;
	return GF_OK;
}

GF_Err GetNextMediaTime(GF_TrackBox *trak, u64 movieTime, u64 *OutMovieTime)
{
	u32 i;
	u64 time;
	GF_EdtsEntry *ent;

	*OutMovieTime = 0;
	if (! trak->editBox || !trak->editBox->editList) return GF_BAD_PARAM;

	time = 0;
	ent = NULL;
	i=0;
	while ((ent = (GF_EdtsEntry *)gf_list_enum(trak->editBox->editList->entryList, &i))) {
		if (time * trak->Media->mediaHeader->timeScale >= movieTime * trak->moov->mvhd->timeScale) {
			/*skip empty edits*/
			if (ent->mediaTime >= 0) {
				*OutMovieTime = time * trak->Media->mediaHeader->timeScale / trak->moov->mvhd->timeScale;
				if (*OutMovieTime>0) *OutMovieTime -= 1;
				return GF_OK;
			}
		}
		time += ent->segmentDuration;
	}
	//request for a bigger time that what we can give: return the last sample (undefined behavior...)
	*OutMovieTime = trak->moov->mvhd->duration;
	return GF_EOS;
}

GF_Err GetPrevMediaTime(GF_TrackBox *trak, u64 movieTime, u64 *OutMovieTime)
{
	u32 i;
	u64 time;
	GF_EdtsEntry *ent;

	*OutMovieTime = 0;
	if (! trak->editBox || !trak->editBox->editList) return GF_BAD_PARAM;

	time = 0;
	ent = NULL;
	i=0;
	while ((ent = (GF_EdtsEntry *)gf_list_enum(trak->editBox->editList->entryList, &i))) {
		if (ent->mediaTime == -1) {
			if ( (time + ent->segmentDuration) * trak->Media->mediaHeader->timeScale >= movieTime * trak->moov->mvhd->timeScale) {
				*OutMovieTime = time * trak->Media->mediaHeader->timeScale / trak->moov->mvhd->timeScale;
				return GF_OK;
			}
			continue;
		}
		/*get the first entry whose end is greater than or equal to the desired time*/
		time += ent->segmentDuration;
		if ( time * trak->Media->mediaHeader->timeScale >= movieTime * trak->moov->mvhd->timeScale) {
			*OutMovieTime = time * trak->Media->mediaHeader->timeScale / trak->moov->mvhd->timeScale;
			return GF_OK;
		}
	}
	*OutMovieTime = 0;
	return GF_OK;
}

#ifndef GPAC_READ_ONLY

void gf_isom_insert_moov(GF_ISOFile *file)
{
	u64 now;
	GF_MovieHeaderBox *mvhd;
	if (file->moov) return;

	//OK, create our boxes (mvhd, iods, ...)
	file->moov = (GF_MovieBox *) moov_New();
	file->moov->mov = file;
	//Header SetUp
	now = gf_isom_get_mp4time();
	mvhd = (GF_MovieHeaderBox *) mvhd_New();
	mvhd->creationTime = now;
	mvhd->modificationTime = now;
	mvhd->nextTrackID = 1;
	//600 is our default movie TimeScale
	mvhd->timeScale = 600;

	file->interleavingTime = mvhd->timeScale;
	moov_AddBox((GF_Box*)file->moov, (GF_Box *)mvhd);
	gf_list_add(file->TopBoxes, file->moov);
}

//Create the movie for WRITE only
GF_ISOFile *gf_isom_create_movie(const char *fileName, u32 OpenMode, const char *tmp_dir)
{
	GF_Err e;

	GF_ISOFile *mov = gf_isom_new_movie();
	if (!mov) return NULL;
	mov->openMode = OpenMode;
	//then set up our movie

	//in WRITE, the input dataMap is ALWAYS NULL
	mov->movieFileMap = NULL;

	//but we have the edit one
	if (OpenMode == GF_ISOM_OPEN_WRITE) {
		//THIS IS NOT A TEMP FILE, WRITE mode is used for "live capture"
		//this file will be the final file...
		mov->fileName = strdup(fileName);
		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_WRITE, & mov->editFileMap);
		if (e) goto err_exit;
		
		/*brand is set to ISOM by default - it may be touched until sample data is added to track*/
		gf_isom_set_brand_info( (GF_ISOFile *) mov, GF_ISOM_BRAND_ISOM, 1);
	} else {
		//we are in EDIT mode but we are creating the file -> temp file
		mov->finalName = (char*)malloc(strlen(fileName) + 1);
		strcpy(mov->finalName, fileName);
		e = gf_isom_datamap_new("mp4_tmp_edit", tmp_dir, GF_ISOM_DATA_MAP_WRITE, & mov->editFileMap);
		if (e) {
			gf_isom_set_last_error(NULL, e);
			gf_isom_delete_movie(mov);
			return NULL;
		}
		//brand is set to ISOM by default
		gf_isom_set_brand_info( (GF_ISOFile *) mov, GF_ISOM_BRAND_ISOM, 1);
	}

	//create an MDAT
	mov->mdat = (GF_MediaDataBox *) mdat_New();
	gf_list_add(mov->TopBoxes, mov->mdat);
	
	//default behaviour is capture mode, no interleaving (eg, no rewrite of mdat)
	mov->storageMode = GF_ISOM_STORE_FLAT;
	return mov;

err_exit:
	gf_isom_set_last_error(NULL, e);
	if (mov) gf_isom_delete_movie(mov);
	return NULL;
}

GF_EdtsEntry *CreateEditEntry(u64 EditDuration, u64 MediaTime, u8 EditMode)
{
	GF_EdtsEntry *ent;

	ent = (GF_EdtsEntry*)malloc(sizeof(GF_EdtsEntry));
	if (!ent) return NULL;

	switch (EditMode) {
	case GF_ISOM_EDIT_EMPTY:
		ent->mediaRate = 1;
		ent->mediaTime = -1;
		break;

	case GF_ISOM_EDIT_DWELL:
		ent->mediaRate = 0;
		ent->mediaTime = MediaTime;
		break;
	default:
		ent->mediaRate = 1;
		ent->mediaTime = MediaTime;
		break;
	}
	ent->segmentDuration = EditDuration;
	return ent;
}

#endif	//GPAC_READ_ONLY

