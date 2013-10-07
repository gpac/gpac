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
#include <gpac/network.h>

#ifndef GPAC_DISABLE_ISOM

/**************************************************************
		Some Local functions for movie creation
**************************************************************/
GF_Err gf_isom_parse_root_box(GF_Box **outBox, GF_BitStream *bs, u64 *bytesExpected, Bool progressive_mode);

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
GF_Err MergeTrack(GF_TrackBox *trak, GF_TrackFragmentBox *traf, u64 moof_offset, Bool is_first_merge);

GF_Err MergeFragment(GF_MovieFragmentBox *moof, GF_ISOFile *mov)
{
	GF_Err e;
	u32 i, j;
	u64 MaxDur;
	GF_TrackFragmentBox *traf;
	GF_TrackBox *trak;
	
	MaxDur = 0;

	//we shall have a MOOV and its MVEX BEFORE any MOOF
	if (!mov->moov || !mov->moov->mvex) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Error: %s not received before merging fragment\n", mov->moov ? "mvex" : "moov" ));
		return GF_ISOM_INVALID_FILE;
	}
	//and all fragments must be continous - we do not throw an error as we may still want to be able to concatenate dependent representations in DASH and
	//we will likely a-have R1(moofSN 1, 3, 5, 7) plus R2(moofSN 2, 4, 6, 8) 
	if (mov->NextMoofNumber && (mov->NextMoofNumber >= moof->mfhd->sequence_number)) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Warning: wrong sequence number: got %d but last one was %d\n", moof->mfhd->sequence_number, mov->NextMoofNumber));
//		return GF_ISOM_INVALID_FILE;
	}

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

		if (!trak || !traf->trex) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Error: Cannot find fragment track with ID %d\n", traf->tfhd->trackID));
			return GF_ISOM_INVALID_FILE;
		}

		e = MergeTrack(trak, traf, mov->current_top_box_start, !trak->first_traf_merged);
		if (e) return e;

		trak->present_in_scalable_segment = 1;

		//update trak duration
		SetTrackDuration(trak);
		if (trak->Header->duration > MaxDur) 
			MaxDur = trak->Header->duration;

		trak->first_traf_merged = 1;
	}

	mov->NextMoofNumber = moof->mfhd->sequence_number;
	//update movie duration
	if (mov->moov->mvhd->duration < MaxDur) mov->moov->mvhd->duration = MaxDur;
	return GF_OK;
}

#endif

GF_Err gf_isom_parse_movie_boxes(GF_ISOFile *mov, u64 *bytesMissing, Bool progressive_mode)
{
	GF_Box *a;
	u64 totSize;
	GF_Err e = GF_OK;

	totSize = 0;


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (mov->single_moof_mode && mov->single_moof_state == 2) {
		return e;
	}

	/*restart from where we stopped last*/
	totSize = mov->current_top_box_start;
	gf_bs_seek(mov->movieFileMap->bs, mov->current_top_box_start);

#endif


	/*while we have some data, parse our boxes*/
	while (gf_bs_available(mov->movieFileMap->bs)) {
		*bytesMissing = 0;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		mov->current_top_box_start = gf_bs_get_position(mov->movieFileMap->bs);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Current top box start before parsing %d\n", mov->current_top_box_start));
#endif

		e = gf_isom_parse_root_box(&a, mov->movieFileMap->bs, bytesMissing, progressive_mode);

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
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
            if (mov->moov->mvex) mov->moov->mvex->mov = mov;
#endif
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
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
				else if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) gf_list_add(mov->TopBoxes, a);
#endif
				else gf_isom_box_del(a);
			}
			/*if we don't have any MDAT yet, create one (edit-write mode)
			We only work with one mdat, but we're puting it at the place
			of the first mdat found when opening a file for editing*/
			else if (!mov->mdat && (mov->openMode != GF_ISOM_OPEN_READ) && (mov->openMode != GF_ISOM_OPEN_CAT_FRAGMENTS)) {
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


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		case GF_ISOM_BOX_TYPE_STYP:
		{
			u32 brand = ((GF_SegmentTypeBox *)a)->majorBrand;
			switch (brand) {
			case GF_4CC('s', 'i', 's', 'x'):
			case GF_4CC('r', 'i', 's', 'x'):
			case GF_4CC('s', 's', 's', 's'):
				mov->is_index_segment = GF_TRUE;
				break;
			default:
				break;
			}
		}
		/*fall-through*/

		case GF_ISOM_BOX_TYPE_SIDX:
			totSize += a->size;
			if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) {
				e = gf_list_add(mov->TopBoxes, a);
			} else {
				gf_isom_box_del(a);
			}
			break;

		case GF_ISOM_BOX_TYPE_MOOF:
			if (mov->single_moof_mode) {
				mov->single_moof_state++;
				if (mov->single_moof_state > 1) {
					gf_isom_box_del(a);
					return GF_OK;
				}
			}
			((GF_MovieFragmentBox *)a)->mov = mov;

			totSize += a->size;
            mov->moof = (GF_MovieFragmentBox *) a;
			/*read & debug: store at root level*/
			if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) {
                u32 k;
				gf_list_add(mov->TopBoxes, a);
				/*also update pointers to trex for debug*/
				if (mov->moov) {
					for (k=0; k<gf_list_count(mov->moof->TrackList); k++) {
						GF_TrackFragmentBox *traf = gf_list_get(mov->moof->TrackList, k);
						if (traf->tfhd) {
							GF_TrackBox *trak = gf_isom_get_track_from_id(mov->moov, traf->tfhd->trackID);
							u32 j=0;
							while ((traf->trex = (GF_TrackExtendsBox*)gf_list_enum(mov->moov->mvex->TrackExList, &j))) {
								if (traf->trex->trackID == traf->tfhd->trackID) {
									if (!traf->trex->track) traf->trex->track = trak;
									break;
								}
								traf->trex = NULL;
							}
						}
					}
				}
			} else if (mov->openMode==GF_ISOM_OPEN_CAT_FRAGMENTS) {
				mov->NextMoofNumber = mov->moof->mfhd->sequence_number+1;
				mov->moof = NULL;
				gf_isom_box_del(a);
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
			u8 *c = (u8 *) box->data;
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

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		/*remember where we left, in case we append an entire number of movie fragments*/
		mov->current_top_box_start = gf_bs_get_position(mov->movieFileMap->bs);
#endif
	}

	/*we need at least moov or meta*/
	if (!mov->moov && !mov->meta 
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
        && !mov->moof && !mov->is_index_segment
#endif
        ) return GF_ISOM_INVALID_FILE;
	/*we MUST have movie header*/
	if (mov->moov && !mov->moov->mvhd) return GF_ISOM_INVALID_FILE;
	/*we MUST have meta handler*/
	if (mov->meta && !mov->meta->handler) return GF_ISOM_INVALID_FILE;

#ifndef GPAC_DISABLE_ISOM_WRITE

	if (mov->moov) {
		/*set the default interleaving time*/
		mov->interleavingTime = mov->moov->mvhd->timeScale;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		/*in edit mode with successfully loaded fragments, delete all fragment signaling since
		file is no longer fragmented*/
		if ((mov->openMode > GF_ISOM_OPEN_READ) && (mov->openMode != GF_ISOM_OPEN_CAT_FRAGMENTS) && mov->moov->mvex) {
			gf_isom_box_del((GF_Box *)mov->moov->mvex);
			mov->moov->mvex = NULL;
		}
#endif

	}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

	return GF_OK;
}

GF_ISOFile *gf_isom_new_movie()
{
	GF_ISOFile *mov = (GF_ISOFile*)gf_malloc(sizeof(GF_ISOFile));
	if (mov == NULL) {
		gf_isom_set_last_error(NULL, GF_OUT_OF_MEM);
		return NULL;
	}
	memset(mov, 0, sizeof(GF_ISOFile));

	/*init the boxes*/
	mov->TopBoxes = gf_list_new();
	if (!mov->TopBoxes) {
		gf_isom_set_last_error(NULL, GF_OUT_OF_MEM);
		gf_free(mov);
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

	mov->fileName = gf_strdup(fileName);
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

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		if (OpenMode == GF_ISOM_OPEN_READ_DUMP) mov->FragmentsFlags |= GF_ISOM_FRAG_READ_DEBUG;
#endif

	} else {

#ifdef GPAC_DISABLE_ISOM_WRITE
		//not allowed for READ_ONLY lib
		gf_isom_delete_movie(mov);
		gf_isom_set_last_error(NULL, GF_ISOM_INVALID_MODE);
		return NULL;

#else

		//set a default output name for edited file
		mov->finalName = (char*)gf_malloc(strlen(fileName) + 5);
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
	mov->LastError = gf_isom_parse_movie_boxes(mov, &bytes, 0);
	if (mov->LastError) {
		gf_isom_set_last_error(NULL, mov->LastError);
		gf_isom_delete_movie(mov);
		return NULL;
	}

	if (OpenMode == GF_ISOM_OPEN_CAT_FRAGMENTS) {
		gf_isom_datamap_del(mov->movieFileMap);
		/*reopen the movie file map in cat mode*/
		e = gf_isom_datamap_new(fileName, tmp_dir, GF_ISOM_DATA_MAP_CAT, & mov->movieFileMap);
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

#ifndef GPAC_DISABLE_ISOM_WRITE
	if (mov->editFileMap) {
		gf_isom_datamap_del(mov->editFileMap);
	}
	if (mov->finalName) gf_free(mov->finalName);
#endif

	gf_isom_box_array_del(mov->TopBoxes);
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	gf_isom_box_array_del(mov->moof_list);
#endif

	if (mov->fileName) gf_free(mov->fileName);
	gf_free(mov);
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

GF_TrackBox *gf_isom_get_track_from_original_id(GF_MovieBox *moov, u32 originalID, u32 originalFile) 
{
	u32 i, count;
	GF_TrackBox *trak;
	if (!moov || !originalID) return NULL;

	count = gf_list_count(moov->trackList);
	for (i = 0; i<count; i++) {
		trak = (GF_TrackBox*)gf_list_get(moov->trackList, i);
		if ((trak->originalFile == originalFile) && (trak->originalID == originalID)) return trak;
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
GF_Err GetMediaTime(GF_TrackBox *trak, Bool force_non_empty, u64 movieTime, u64 *MediaTime, s64 *SegmentStartTime, s64 *MediaOffset, u8 *useEdit)
{
#if 0
	GF_Err e;
	u32 sampleNumber, prevSampleNumber;
	u64 firstDTS;
#endif
	u32 i;
	Bool last_is_empty = 0;
	u64 time, lastSampleTime;
	s64 mtime;
	GF_EdtsEntry *ent;
	Double scale_ts;
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
		if ((*MediaTime > lastSampleTime)
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			&& !trak->moov->mov->use_segments
#endif
		   ) {
			*MediaTime = lastSampleTime;
		}
		*useEdit = 0;
		return GF_OK;
	}
	//browse the edit list and get the time
	scale_ts = trak->Media->mediaHeader->timeScale;
	scale_ts /= trak->moov->mvhd->timeScale;

	time = 0;
	ent = NULL;
	i=0;
	while ((ent = (GF_EdtsEntry *)gf_list_enum(trak->editBox->editList->entryList, &i))) {
		if ( (time + ent->segmentDuration) * scale_ts > movieTime) {
			if (!force_non_empty || (ent->mediaTime >= 0)) 
				goto ent_found;
		}
		time += ent->segmentDuration;
		last_is_empty = ent->segmentDuration ? 0 : 1;
	}

	if (last_is_empty) {
		ent = (GF_EdtsEntry *)gf_list_last(trak->editBox->editList->entryList);
		if (ent->mediaRate==1) {
			*MediaTime = movieTime + ent->mediaTime;
		} else {
			ent = (GF_EdtsEntry *)gf_list_get(trak->editBox->editList->entryList, 0);
			if (ent->mediaRate==-1) {
				u64 dur = (u64) (ent->segmentDuration * scale_ts);
				*MediaTime = (movieTime > dur) ? (movieTime-dur) : 0;
			}
		}
		*useEdit = 0;
		return GF_OK;
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
	*MediaOffset = ent->mediaTime;

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

	//now get the entry sample (the entry time gives the CTS, and we need the DTS
	e = findEntryForTime(stbl, (u32) ent->mediaTime, 0, &sampleNumber, &prevSampleNumber);
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
#endif
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

#ifndef GPAC_DISABLE_ISOM_WRITE

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
	if (!file->keep_utc)
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
		mov->fileName = gf_strdup(fileName);
		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_WRITE, & mov->editFileMap);
		if (e) goto err_exit;
		
		/*brand is set to ISOM by default - it may be touched until sample data is added to track*/
		gf_isom_set_brand_info( (GF_ISOFile *) mov, GF_ISOM_BRAND_ISOM, 1);
	} else {
		//we are in EDIT mode but we are creating the file -> temp file
		mov->finalName = (char*)gf_malloc(strlen(fileName) + 1);
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

	ent = (GF_EdtsEntry*)gf_malloc(sizeof(GF_EdtsEntry));
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

GF_Err gf_isom_add_subsample_info(GF_SubSampleInformationBox *sub_samples, u32 sampleNumber, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable)
{
	u32 i, count, last_sample;
	GF_SubSampleInfoEntry *pSamp;
	GF_SubSampleEntry *pSubSamp;

	pSamp = NULL;
	last_sample = 0;
	count = gf_list_count(sub_samples->Samples);
	for (i=0; i<count; i++) {
		pSamp = (GF_SubSampleInfoEntry*) gf_list_get(sub_samples->Samples, i);
		/*TODO - do we need to support insertion of subsample info ?*/
		if (last_sample + pSamp->sample_delta > sampleNumber) return GF_NOT_SUPPORTED; 
		if (last_sample + pSamp->sample_delta == sampleNumber) break; 
		last_sample += pSamp->sample_delta;
		pSamp = NULL;
	}

	if (!pSamp) {
		GF_SAFEALLOC(pSamp, GF_SubSampleInfoEntry);
		if (!pSamp) return GF_OUT_OF_MEM;
		pSamp->SubSamples = gf_list_new();
		if (!pSamp->SubSamples ) {
			gf_free(pSamp);
			return GF_OUT_OF_MEM;
		}
		pSamp->sample_delta = sampleNumber - last_sample;
		gf_list_add(sub_samples->Samples, pSamp);
	}

	if ((subSampleSize>0xFFFF) && !sub_samples->version) {
		sub_samples->version = 1;
	}
	/*remove last subsample info*/
	if (!subSampleSize) {
		pSubSamp = gf_list_last(pSamp->SubSamples);
		gf_list_rem_last(pSamp->SubSamples);
		gf_free(pSubSamp);
		if (!gf_list_count(pSamp->SubSamples)) {
			gf_list_del_item(sub_samples->Samples, pSamp);
			gf_list_del(pSamp->SubSamples);
			gf_free(pSamp);
		}
		return GF_OK;
	}
	/*add subsample*/
	GF_SAFEALLOC(pSubSamp, GF_SubSampleEntry);
	if (!pSubSamp) return GF_OUT_OF_MEM;
	pSubSamp->subsample_size = subSampleSize;
	pSubSamp->subsample_priority = priority;
	pSubSamp->reserved = reserved;
	pSubSamp->discardable = discardable;
	return gf_list_add(pSamp->SubSamples, pSubSamp);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

u32 gf_isom_sample_get_subsample_entry(GF_ISOFile *movie, u32 track, u32 sampleNumber, GF_SubSampleInfoEntry **sub_sample)
{
	u32 i, count, last_sample;
	GF_SubSampleInformationBox *sub_samples;
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, track);
	if (sub_sample) *sub_sample = NULL;
	if (!track) return 0;
	if (!trak->Media || !trak->Media->information->sampleTable) return 0;
	sub_samples = trak->Media->information->sampleTable->SubSamples;
	if (!sub_samples) return 0;
	last_sample = 0;
	count = gf_list_count(sub_samples->Samples);
	for (i=0; i<count; i++) {
		GF_SubSampleInfoEntry *pSamp = (GF_SubSampleInfoEntry *) gf_list_get(sub_samples->Samples, i);
		if (last_sample + pSamp->sample_delta == sampleNumber) {
			if (sub_sample) *sub_sample = pSamp;
			return gf_list_count(pSamp->SubSamples);
		}
		last_sample += pSamp->sample_delta;
	}
	return 0;
}
#endif /*GPAC_DISABLE_ISOM*/
