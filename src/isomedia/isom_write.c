/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2021
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
#include <gpac/iso639.h>


#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)

GF_Err CanAccessMovie(GF_ISOFile *movie, GF_ISOOpenMode Mode)
{
	if (!movie) return GF_BAD_PARAM;
	if (movie->openMode < Mode) return GF_ISOM_INVALID_MODE;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) return GF_ISOM_INVALID_MODE;
#endif
	return GF_OK;
}

static GF_Err unpack_track(GF_TrackBox *trak)
{
	GF_Err e = GF_OK;
	if (!trak->is_unpacked) {
		e = stbl_UnpackOffsets(trak->Media->information->sampleTable);
		if (e) return e;
		e = stbl_unpackCTS(trak->Media->information->sampleTable);
		trak->is_unpacked = GF_TRUE;
	}
	return e;
}


GF_Err FlushCaptureMode(GF_ISOFile *movie)
{
	GF_Err e;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) {
		if (!movie->editFileMap) return GF_ISOM_INVALID_MODE;
		return GF_OK;
	}
	/*make sure nothing was added*/
	if (gf_bs_get_position(movie->editFileMap->bs)) return GF_OK;

	if (!strcmp(movie->fileName, "_gpac_isobmff_redirect")) {
		if (!movie->on_block_out) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] Missing output block callback, cannot write\n"));
			return GF_BAD_PARAM;
		}

		gf_bs_del(movie->editFileMap->bs);
		movie->editFileMap->bs = gf_bs_new_cbk(movie->on_block_out, movie->on_block_out_usr_data, movie->on_block_out_block_size);
	}

	/*add all first boxes*/
	if (movie->brand) {
		e = gf_isom_box_size((GF_Box *)movie->brand);
		if (e) return e;
		e = gf_isom_box_write((GF_Box *)movie->brand, movie->editFileMap->bs);
		if (e) return e;
	}
	if (movie->pdin) {
		e = gf_isom_box_size((GF_Box *)movie->pdin);
		if (e) return e;
		e = gf_isom_box_write((GF_Box *)movie->pdin, movie->editFileMap->bs);
		if (e) return e;
	}
	movie->mdat->bsOffset = gf_bs_get_position(movie->editFileMap->bs);

	/*we have a trick here: the data will be stored on the fly, so the first
	thing in the file is the MDAT. As we don't know if we have a large file (>4 GB) or not
	do as if we had one and write 16 bytes: 4 (type) + 4 (size) + 8 (largeSize)...*/
	gf_bs_write_long_int(movie->editFileMap->bs, 0, 64);
	gf_bs_write_long_int(movie->editFileMap->bs, 0, 64);
	return GF_OK;
}

static GF_Err CheckNoData(GF_ISOFile *movie)
{
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_OK;
	if (gf_bs_get_position(movie->editFileMap->bs)) return GF_BAD_PARAM;
	return GF_OK;
}

/**************************************************************
					File Writing / Editing
**************************************************************/
//quick function to add an IOD/OD to the file if not present (iods is optional)
GF_Err AddMovieIOD(GF_MovieBox *moov, u8 isIOD)
{
	GF_Descriptor *od;
	GF_ObjectDescriptorBox *iods;

	//do we have an IOD ?? If not, create one.
	if (moov->iods) return GF_OK;

	if (isIOD) {
		od = gf_odf_desc_new(GF_ODF_ISOM_IOD_TAG);
	} else {
		od = gf_odf_desc_new(GF_ODF_ISOM_OD_TAG);
	}
	if (!od) return GF_OUT_OF_MEM;
	((GF_IsomObjectDescriptor *)od)->objectDescriptorID = 1;

	iods = (GF_ObjectDescriptorBox *) gf_isom_box_new_parent(&moov->child_boxes, GF_ISOM_BOX_TYPE_IODS);
	if (!iods) return GF_OUT_OF_MEM;
	iods->descriptor = od;
	return moov_on_child_box((GF_Box*)moov, (GF_Box *)iods, GF_FALSE);
}

//add a track to the root OD
GF_EXPORT
GF_Err gf_isom_add_track_to_root_od(GF_ISOFile *movie, u32 trackNumber)
{
	GF_Err e;
	GF_ES_ID_Inc *inc;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (!movie->moov->iods) AddMovieIOD(movie->moov, 0);

	if (gf_isom_is_track_in_root_od(movie, trackNumber) == 1) return GF_OK;

	inc = (GF_ES_ID_Inc *) gf_odf_desc_new(GF_ODF_ESD_INC_TAG);
	inc->trackID = gf_isom_get_track_id(movie, trackNumber);
	if (!inc->trackID) {
		gf_odf_desc_del((GF_Descriptor *)inc);
		return movie->LastError;
	}
	if ( (movie->LastError = gf_isom_add_desc_to_root_od(movie, (GF_Descriptor *)inc) ) ) {
		return movie->LastError;
	}
	gf_odf_desc_del((GF_Descriptor *)inc);
	return GF_OK;
}

//remove the root OD
GF_EXPORT
GF_Err gf_isom_remove_root_od(GF_ISOFile *movie)
{
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	if (!movie->moov || !movie->moov->iods) return GF_OK;
	gf_isom_box_del_parent(&movie->moov->child_boxes, (GF_Box *)movie->moov->iods);
	movie->moov->iods = NULL;
	return GF_OK;
}

//remove a track to the root OD
GF_EXPORT
GF_Err gf_isom_remove_track_from_root_od(GF_ISOFile *movie, u32 trackNumber)
{
	GF_List *esds;
	GF_ES_ID_Inc *inc;
	u32 i;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	if (!movie->moov) return GF_OK;

	if (!gf_isom_is_track_in_root_od(movie, trackNumber)) return GF_OK;

	if (!movie->moov->iods) {
		e = AddMovieIOD(movie->moov, 0);
		if (e) return e;
	}
	switch (movie->moov->iods->descriptor->tag) {
	case GF_ODF_ISOM_IOD_TAG:
		esds = ((GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor)->ES_ID_IncDescriptors;
		break;
	case GF_ODF_ISOM_OD_TAG:
		esds = ((GF_IsomObjectDescriptor *)movie->moov->iods->descriptor)->ES_ID_IncDescriptors;
		break;
	default:
		return GF_ISOM_INVALID_FILE;
	}

	//get the desc
	i=0;
	while ((inc = (GF_ES_ID_Inc*)gf_list_enum(esds, &i))) {
		if (inc->trackID == (u32) gf_isom_get_track_id(movie, trackNumber)) {
			gf_odf_desc_del((GF_Descriptor *)inc);
			gf_list_rem(esds, i-1);
			break;
		}
	}
	//we don't remove the iod for P&Ls and other potential info
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_creation_time(GF_ISOFile *movie, u64 ctime, u64 mtime)
{
	if (!movie || !movie->moov) return GF_BAD_PARAM;
	movie->moov->mvhd->creationTime = ctime;
	movie->moov->mvhd->modificationTime = mtime;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_track_creation_time(GF_ISOFile *movie,u32 trackNumber, u64 ctime, u64 mtime)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	trak->Header->creationTime = ctime;
	trak->Header->modificationTime = mtime;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_media_creation_time(GF_ISOFile *movie,u32 trackNumber, u64 ctime, u64 mtime)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media || !trak->Media->mediaHeader) return GF_ISOM_INVALID_FILE;

	trak->Media->mediaHeader->creationTime = ctime;
	trak->Media->mediaHeader->modificationTime = mtime;
	return GF_OK;
}

//sets the enable flag of a track
GF_EXPORT
GF_Err gf_isom_set_track_enabled(GF_ISOFile *movie, u32 trackNumber, Bool enableTrack)
{
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (enableTrack) {
		trak->Header->flags |= 1;
	} else {
		trak->Header->flags &= ~1;
	}
	return GF_OK;
}

//sets the enable flag of a track
GF_EXPORT
GF_Err gf_isom_set_track_flags(GF_ISOFile *movie, u32 trackNumber, u32 flags, GF_ISOMTrackFlagOp op)
{
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (op==GF_ISOM_TKFLAGS_ADD)
		trak->Header->flags |= flags;
	else if (op==GF_ISOM_TKFLAGS_REM)
		trak->Header->flags &= ~flags;
	else
		trak->Header->flags = flags;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_media_language(GF_ISOFile *movie, u32 trackNumber, char *code)
{
	GF_Err e;
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !code) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	// Old language-storage processing
	// if the new code is on 3 chars, we use it
	// otherwise, we find the associated 3 chars code and use it
	if (strlen(code) == 3) {
		memcpy(trak->Media->mediaHeader->packedLanguage, code, sizeof(char)*3);
	} else {
		s32 lang_idx;
		const char *code_3cc;
		lang_idx = gf_lang_find(code);
		if (lang_idx == -1) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("The given code is not a valid one: %s, using 'und' as 3-letter code\n", code));
			code_3cc = "und";
		} else {
			code_3cc = gf_lang_get_3cc(lang_idx);
		}
		memcpy(trak->Media->mediaHeader->packedLanguage, code_3cc, sizeof(char)*3);
	}

	// New language-storage processing
	// change the code in the extended language box (if any)
	// otherwise add an extended language box only if the given code is not 3 chars
	{
		u32 i, count;
		GF_ExtendedLanguageBox *elng;
		elng = NULL;
		count = gf_list_count(trak->Media->child_boxes);
		for (i = 0; i < count; i++) {
			GF_Box *box = (GF_Box *)gf_list_get(trak->Media->child_boxes, i);
			if (box->type == GF_ISOM_BOX_TYPE_ELNG) {
				elng = (GF_ExtendedLanguageBox *)box;
				break;
			}
		}
		if (!elng && (strlen(code) > 3)) {
			elng = (GF_ExtendedLanguageBox *)gf_isom_box_new_parent(&trak->Media->child_boxes, GF_ISOM_BOX_TYPE_ELNG);
			if (!elng) return GF_OUT_OF_MEM;
		}
		if (elng) {
			if (elng->extended_language) {
				gf_free(elng->extended_language);
			}
			elng->extended_language = gf_strdup(code);
		}
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	return GF_OK;
}

static GF_Err gf_isom_set_root_iod(GF_ISOFile *movie)
{
	GF_IsomInitialObjectDescriptor *iod;
	GF_IsomObjectDescriptor *od;
	GF_Err e;
	
	e = gf_isom_insert_moov(movie);
	if (e) return e;
	if (!movie->moov->iods) {
		AddMovieIOD(movie->moov, 1);
		return GF_OK;
	}
	//if OD, switch to IOD
	if (movie->moov->iods->descriptor->tag == GF_ODF_ISOM_IOD_TAG) return GF_OK;
	od = (GF_IsomObjectDescriptor *) movie->moov->iods->descriptor;
	iod = (GF_IsomInitialObjectDescriptor*)gf_malloc(sizeof(GF_IsomInitialObjectDescriptor));
	if (!iod) return GF_OUT_OF_MEM;

	memset(iod, 0, sizeof(GF_IsomInitialObjectDescriptor));

	iod->ES_ID_IncDescriptors = od->ES_ID_IncDescriptors;
	od->ES_ID_IncDescriptors = NULL;
	//not used in root OD
	iod->ES_ID_RefDescriptors = NULL;
	iod->extensionDescriptors = od->extensionDescriptors;
	od->extensionDescriptors = NULL;
	iod->IPMP_Descriptors = od->IPMP_Descriptors;
	od->IPMP_Descriptors = NULL;
	iod->objectDescriptorID = od->objectDescriptorID;
	iod->OCIDescriptors = od->OCIDescriptors;
	od->OCIDescriptors = NULL;
	iod->tag = GF_ODF_ISOM_IOD_TAG;
	iod->URLString = od->URLString;
	od->URLString = NULL;

	gf_odf_desc_del((GF_Descriptor *) od);
	movie->moov->iods->descriptor = (GF_Descriptor *)iod;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_add_desc_to_root_od(GF_ISOFile *movie, const GF_Descriptor *theDesc)
{
	GF_Err e;
	GF_Descriptor *desc, *dupDesc;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (!movie->moov->iods) {
		e = AddMovieIOD(movie->moov, 0);
		if (e) return e;
	}
	if (theDesc->tag==GF_ODF_IPMP_TL_TAG) gf_isom_set_root_iod(movie);

	desc = movie->moov->iods->descriptor;
	//the type of desc is handled at the OD/IOD level, we'll be notified
	//if the desc is not allowed
	switch (desc->tag) {
	case GF_ODF_ISOM_IOD_TAG:
	case GF_ODF_ISOM_OD_TAG:
		//duplicate the desc
		e = gf_odf_desc_copy((GF_Descriptor *)theDesc, &dupDesc);
		if (e) return e;
		//add it (MUST BE  (I)OD level desc)
		movie->LastError = gf_odf_desc_add_desc(desc, dupDesc);
		if (movie->LastError) gf_odf_desc_del((GF_Descriptor *)dupDesc);
		break;
	default:
		movie->LastError = GF_ISOM_INVALID_FILE;
		break;
	}
	return movie->LastError;
}


GF_EXPORT
GF_Err gf_isom_set_timescale(GF_ISOFile *movie, u32 timeScale)
{
	GF_TrackBox *trak;
	u32 i;
	GF_Err e;
	if (!timeScale) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (movie->moov->mvhd->timeScale == timeScale) return GF_OK;

	/*rewrite all durations and edit lists*/
	movie->moov->mvhd->duration *= timeScale;
	movie->moov->mvhd->duration /= movie->moov->mvhd->timeScale;
	if (movie->moov->mvex && movie->moov->mvex->mehd) {
		movie->moov->mvex->mehd->fragment_duration *= timeScale;
		movie->moov->mvex->mehd->fragment_duration /= movie->moov->mvhd->timeScale;
	}

	i=0;
	while ((trak = (GF_TrackBox*)gf_list_enum(movie->moov->trackList, &i))) {
		trak->Header->duration *= timeScale;
		trak->Header->duration /= movie->moov->mvhd->timeScale;

		if (trak->editBox && trak->editBox->editList) {
			u32 j, count = gf_list_count(trak->editBox->editList->entryList);
			for (j=0; j<count; j++) {
				GF_EdtsEntry *ent = (GF_EdtsEntry *)gf_list_get(trak->editBox->editList->entryList, j);
				ent->segmentDuration *= timeScale;
				ent->segmentDuration /= movie->moov->mvhd->timeScale;
			}
		}
	}
	if (movie->moov->mvex && movie->moov->mvex->mehd) {
		movie->moov->mvex->mehd->fragment_duration *= timeScale;
		movie->moov->mvex->mehd->fragment_duration /= movie->moov->mvhd->timeScale;
	}
	movie->moov->mvhd->timeScale = timeScale;
	movie->interleavingTime = timeScale;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_set_pl_indication(GF_ISOFile *movie, GF_ISOProfileLevelType PL_Code, u8 ProfileLevel)
{
	GF_IsomInitialObjectDescriptor *iod;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	e = gf_isom_set_root_iod(movie);
	if (e) return e;

	iod = (GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor;

	switch (PL_Code) {
	case GF_ISOM_PL_AUDIO:
		iod->audio_profileAndLevel = ProfileLevel;
		break;
	case GF_ISOM_PL_GRAPHICS:
		iod->graphics_profileAndLevel = ProfileLevel;
		break;
	case GF_ISOM_PL_OD:
		iod->OD_profileAndLevel = ProfileLevel;
		break;
	case GF_ISOM_PL_SCENE:
		iod->scene_profileAndLevel = ProfileLevel;
		break;
	case GF_ISOM_PL_VISUAL:
		iod->visual_profileAndLevel = ProfileLevel;
		break;
	case GF_ISOM_PL_INLINE:
		iod->inlineProfileFlag = ProfileLevel ? 1 : 0;
		break;
	default:
		break;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_root_od_id(GF_ISOFile *movie, u32 OD_ID)
{
	GF_Err e;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	e = gf_isom_insert_moov(movie);
	if (e) return e;
	if (!movie->moov->iods) {
		e = AddMovieIOD(movie->moov, 0);
		if (e) return e;
	}

	switch (movie->moov->iods->descriptor->tag) {
	case GF_ODF_ISOM_OD_TAG:
		((GF_IsomObjectDescriptor *)movie->moov->iods->descriptor)->objectDescriptorID = OD_ID;
		break;
	case GF_ODF_ISOM_IOD_TAG:
		((GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor)->objectDescriptorID = OD_ID;
		break;
	default:
		return GF_ISOM_INVALID_FILE;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_root_od_url(GF_ISOFile *movie, const char *url_string)
{
	GF_Err e;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (!movie->moov->iods) {
		e = AddMovieIOD(movie->moov, 0);
		if (e) return e;
	}

	switch (movie->moov->iods->descriptor->tag) {
	case GF_ODF_ISOM_OD_TAG:
		if (((GF_IsomObjectDescriptor *)movie->moov->iods->descriptor)->URLString) gf_free(((GF_IsomObjectDescriptor *)movie->moov->iods->descriptor)->URLString);
		((GF_IsomObjectDescriptor *)movie->moov->iods->descriptor)->URLString = url_string ? gf_strdup(url_string) : NULL;
		break;
	case GF_ODF_ISOM_IOD_TAG:
		if (((GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor)->URLString) gf_free(((GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor)->URLString);
		((GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor)->URLString = url_string ? gf_strdup(url_string) : NULL;
		break;
	default:
		return GF_ISOM_INVALID_FILE;
	}
	return GF_OK;
}

GF_EXPORT
GF_ISOTrackID gf_isom_get_last_created_track_id(GF_ISOFile *movie)
{
	return movie ? movie->last_created_track_id : 0;
}


GF_EXPORT
GF_Err gf_isom_load_extra_boxes(GF_ISOFile *movie, u8 *moov_boxes, u32 moov_boxes_size, Bool udta_only)
{
	GF_BitStream *bs;

	GF_Err e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	e = gf_isom_insert_moov(movie);
	if (e) return e;

	bs = gf_bs_new(moov_boxes, moov_boxes_size, GF_BITSTREAM_READ);

	//we may have terminators in some QT files (4 bytes set to 0 ...)
	while (gf_bs_available(bs) >= 8) {
		GF_Box *a_box;
		e = gf_isom_box_parse_ex((GF_Box**)&a_box, bs, GF_ISOM_BOX_TYPE_MOOV, GF_FALSE);
		if (e || !a_box) goto exit;

		if (a_box->type == GF_ISOM_BOX_TYPE_UDTA) {
			if (movie->moov->udta) gf_isom_box_del_parent(&movie->moov->child_boxes, (GF_Box*)movie->moov->udta);
			movie->moov->udta = (GF_UserDataBox*) a_box;

			if (!movie->moov->child_boxes) movie->moov->child_boxes = gf_list_new();
			gf_list_add(movie->moov->child_boxes, a_box);

		} else if (!udta_only && (a_box->type!=GF_ISOM_BOX_TYPE_PSSH) ) {
			if (!movie->moov->child_boxes) movie->moov->child_boxes = gf_list_new();
			gf_list_add(movie->moov->child_boxes, a_box);
		} else {
			gf_isom_box_del(a_box);
		}
	}
exit:
	gf_bs_del(bs);
	return e;
}

//creates a new Track. If trackID = 0, the trackID is chosen by the API
//returns the track number or 0 if error
GF_EXPORT
u32 gf_isom_new_track_from_template(GF_ISOFile *movie, GF_ISOTrackID trakID, u32 MediaType, u32 TimeScale, u8 *tk_box, u32 tk_box_size, Bool udta_only)
{
	GF_Err e;
	u64 now;
	u8 isHint;
	GF_TrackBox *trak;
	GF_TrackHeaderBox *tkhd;
	GF_MediaBox *mdia;
	GF_UserDataBox *udta = NULL;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) {
		gf_isom_set_last_error(movie, e);
		return 0;
	}
	e = gf_isom_insert_moov(movie);
	if (e) return e;


	isHint = 0;
	//we're creating a hint track... it's the same, but mode HAS TO BE EDIT
	if (MediaType == GF_ISOM_MEDIA_HINT) {
//		if (movie->openMode != GF_ISOM_OPEN_EDIT) return 0;
		isHint = 1;
	}

	mdia = NULL;
	tkhd = NULL;
	trak = NULL;
	if (trakID) {
		//check if we are in ES_ID boundaries
		if (!isHint && (trakID > 0xFFFF)) {
			gf_isom_set_last_error(movie, GF_BAD_PARAM);
			return 0;
		}
		//here we should look for available IDs ...
		if (!RequestTrack(movie->moov, trakID)) return 0;
	} else {
		trakID = movie->moov->mvhd->nextTrackID;
		if (!trakID) trakID = 1;
		/*ESIDs are on 16 bits*/
		if (! isHint && (trakID > 0xFFFF)) trakID = 1;

		while (1) {
			if (RequestTrack(movie->moov, trakID)) break;
			trakID += 1;
			if (trakID == 0xFFFFFFFF) break;
		}
		if (trakID == 0xFFFFFFFF) {
			gf_isom_set_last_error(movie, GF_BAD_PARAM);
			return 0;
		}
		if (! isHint && (trakID > 0xFFFF)) {
			gf_isom_set_last_error(movie, GF_BAD_PARAM);
			return 0;
		}
	}

	if (tk_box) {
		GF_BitStream *bs = gf_bs_new(tk_box, tk_box_size, GF_BITSTREAM_READ);
		gf_bs_set_cookie(bs, GF_ISOM_BS_COOKIE_NO_LOGS|GF_ISOM_BS_COOKIE_CLONE_TRACK);

		e = gf_isom_box_parse_ex((GF_Box**)&trak, bs, GF_ISOM_BOX_TYPE_MOOV, GF_FALSE);
		gf_bs_del(bs);
		if (e) trak = NULL;
		else if (udta_only) {
			udta = trak->udta;
			trak->udta = NULL;
			gf_isom_box_del((GF_Box*)trak);
		} else {
			Bool tpl_ok = GF_TRUE;
			if (!trak->Header || !trak->Media || !trak->Media->handler || !trak->Media->mediaHeader || !trak->Media->information) tpl_ok = GF_FALSE;

			else {
				if (!MediaType) MediaType = trak->Media->handler->handlerType;
				e = NewMedia(&trak->Media, MediaType, TimeScale);
				if (e) tpl_ok = GF_FALSE;
			}
			if (!tpl_ok) {
				udta = trak->udta;
				trak->udta = NULL;
				gf_isom_box_del((GF_Box*)trak);
			}
		}
	}
	now = gf_isom_get_mp4time();
	if (!trak) {
		//OK, now create a track...
		trak = (GF_TrackBox *) gf_isom_box_new_parent(&movie->moov->child_boxes, GF_ISOM_BOX_TYPE_TRAK);
		if (!trak) {
			gf_isom_set_last_error(movie, GF_OUT_OF_MEM);
			return 0;
		}
		tkhd = (GF_TrackHeaderBox *) gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_TKHD);
		if (!tkhd) {
			gf_isom_set_last_error(movie, GF_OUT_OF_MEM);
			return 0;
		}

		//OK, set up the media trak
		e = NewMedia(&mdia, MediaType, TimeScale);
		if (e) {
			gf_isom_box_del((GF_Box *)mdia);
			return 0;
		}
		assert(trak->child_boxes);
		gf_list_add(trak->child_boxes, mdia);

		//OK, add this media to our track
		mdia->mediaTrack = trak;

		e = trak_on_child_box((GF_Box*)trak, (GF_Box *) tkhd, GF_FALSE);
		if (e) goto err_exit;
		e = trak_on_child_box((GF_Box*)trak, (GF_Box *) mdia, GF_FALSE);
		if (e) goto err_exit;
		tkhd->trackID = trakID;

		if (gf_sys_is_test_mode() ) {
			tkhd->creationTime = 0;
			mdia->mediaHeader->creationTime = 0;
		} else {
			tkhd->creationTime = now;
			mdia->mediaHeader->creationTime = now;
		}

	} else {
		tkhd = trak->Header;
		tkhd->trackID = trakID;
		mdia = trak->Media;
		mdia->mediaTrack = trak;
		mdia->mediaHeader->timeScale = TimeScale;
		if (mdia->handler->handlerType != MediaType) {
			mdia->handler->handlerType = MediaType;
			tkhd->width = 0;
			tkhd->height = 0;
			tkhd->volume = 0;
		} else {
			MediaType = 0;
		}
		trak->Header->duration = 0;
		mdia->mediaHeader->duration = 0;

		if (!movie->moov->child_boxes) movie->moov->child_boxes = gf_list_new();
		gf_list_add(movie->moov->child_boxes, trak);
	}
	if (MediaType) {
		//some default properties for Audio, Visual or private tracks
		switch (MediaType) {
		case GF_ISOM_MEDIA_VISUAL:
		case GF_ISOM_MEDIA_AUXV:
		case GF_ISOM_MEDIA_PICT:
		case GF_ISOM_MEDIA_SCENE:
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
			/*320-240 pix in 16.16*/
			tkhd->width = 0x01400000;
			tkhd->height = 0x00F00000;
			break;
		case GF_ISOM_MEDIA_AUDIO:
			tkhd->volume = 0x0100;
			break;
		}
	}
	movie->last_created_track_id = tkhd->trackID;
	
	if (!movie->keep_utc && !gf_sys_is_test_mode() ) {
		tkhd->modificationTime = now;
	 	mdia->mediaHeader->modificationTime = now;
	}

	//OK, add our trak
	e = moov_on_child_box((GF_Box*)movie->moov, (GF_Box *)trak, GF_FALSE);
	if (e) goto err_exit;
	//set the new ID available
	if (trakID+1> movie->moov->mvhd->nextTrackID)
		movie->moov->mvhd->nextTrackID = trakID+1;

	trak->udta = udta;

	//and return our track number
	return gf_isom_get_track_by_id(movie, trakID);

err_exit:
	//tkhd is registered with track and will be destroyed there
	if (trak) gf_isom_box_del((GF_Box *)trak);
	if (mdia) gf_isom_box_del((GF_Box *)mdia);
	return 0;
}

GF_EXPORT
u32 gf_isom_new_track(GF_ISOFile *movie, GF_ISOTrackID trakID, u32 MediaType, u32 TimeScale)
{
	return gf_isom_new_track_from_template(movie, trakID, MediaType, TimeScale, NULL, 0, GF_FALSE);
}

GF_EXPORT
GF_Err gf_isom_remove_stream_description(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_SampleEntryBox *entry;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	entry = (GF_SampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex - 1);
	if (!entry) return GF_BAD_PARAM;
	gf_list_rem(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex - 1);
	gf_isom_box_del((GF_Box *)entry);
	return GF_OK;
}

//Create a new StreamDescription in the file. The URL and URN are used to describe external media
GF_EXPORT
GF_Err gf_isom_new_mpeg4_description(GF_ISOFile *movie,
                                     u32 trackNumber,
                                     const GF_ESD *esd,
                                     const char *URLname,
                                     const char *URNname,
                                     u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_ESD *new_esd;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media ||
	        !esd || !esd->decoderConfig ||
	        !esd->slConfig) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) return e;
	}
	//duplicate our desc
	e = gf_odf_desc_copy((GF_Descriptor *)esd, (GF_Descriptor **)&new_esd);
	if (e) return e;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	e = Track_SetStreamDescriptor(trak, 0, dataRefIndex, new_esd, outDescriptionIndex);
	if (e) {
		gf_odf_desc_del((GF_Descriptor *)new_esd);
		return e;
	}
	return e;
}

GF_Err gf_isom_flush_chunk(GF_TrackBox *trak, Bool is_final)
{
	GF_Err e;
	u64 data_offset;
	u32 sample_number;
	u8 *chunk_data;
	u32 chunk_size, chunk_alloc;
	if (!trak->chunk_cache) return GF_OK;

	gf_bs_get_content_no_truncate(trak->chunk_cache, &chunk_data, &chunk_size, &chunk_alloc);

	data_offset = gf_isom_datamap_get_offset(trak->Media->information->dataHandler);

	e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, chunk_data, chunk_size);
	if (e) return e;

	sample_number = 1 + trak->Media->information->sampleTable->SampleSize->sampleCount;
	sample_number -= trak->nb_samples_in_cache;

	e = stbl_AddChunkOffset(trak->Media, sample_number, trak->chunk_stsd_idx, data_offset, trak->nb_samples_in_cache);

	if (is_final) {
		gf_free(chunk_data);
		gf_bs_del(trak->chunk_cache);
		trak->chunk_cache = NULL;
	} else {
		gf_bs_reassign_buffer(trak->chunk_cache, chunk_data, chunk_alloc);
	}
	return e;
}

static GF_Err trak_add_sample(GF_ISOFile *movie, GF_TrackBox *trak, const GF_ISOSample *sample, u32 descIndex, u64 data_offset, u32 syncShadowSampleNum)
{
	Bool skip_data = GF_FALSE;
	GF_Err e;

	//faststart mode with interleaving time, cache data until we have a full chunk
	if ((movie->storageMode==GF_ISOM_STORE_FASTSTART) && movie->interleavingTime) {
		Bool flush_chunk = GF_FALSE;
		u64 stime = sample->DTS;
		stime *= movie->moov->mvhd->timeScale;
		stime /= trak->Media->mediaHeader->timeScale;

		if (stime - trak->first_dts_chunk > movie->interleavingTime)
			flush_chunk = GF_TRUE;

		if (movie->next_flush_chunk_time < stime)
			flush_chunk = GF_TRUE;

		if (trak->chunk_stsd_idx != descIndex)
			flush_chunk = GF_TRUE;

		if (trak->Media->information->sampleTable->MaxChunkSize && trak->Media->information->sampleTable->MaxChunkSize < trak->chunk_cache_size + sample->dataLength)
			flush_chunk = GF_TRUE;

		if (flush_chunk) {
			movie->next_flush_chunk_time = stime + movie->interleavingTime;
			if (trak->chunk_cache) {
				e = gf_isom_flush_chunk(trak, GF_FALSE);
				if (e) return e;
			}
			trak->nb_samples_in_cache = 0;
			trak->chunk_cache_size = 0;
			trak->first_dts_chunk = stime;
		}
		if (!trak->chunk_cache)
			trak->chunk_cache = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_data(trak->chunk_cache, sample->data, sample->dataLength);
		trak->nb_samples_in_cache += sample->nb_pack ? sample->nb_pack : 1;
		trak->chunk_cache_size += sample->dataLength;
		trak->chunk_stsd_idx = descIndex;

		skip_data = GF_TRUE;
	}

	e = Media_AddSample(trak->Media, data_offset, sample, descIndex, syncShadowSampleNum);
	if (e) return e;

	if (!skip_data && sample->dataLength) {
		e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, sample->data, sample->dataLength);
		if (e) return e;
	}

	return GF_OK;
}

//Add samples to a track. Use streamDescriptionIndex to specify the desired stream (if several)
GF_EXPORT
GF_Err gf_isom_add_sample(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, const GF_ISOSample *sample)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	u32 dataRefIndex;
	u64 data_offset;
	u32 descIndex;
	GF_DataEntryURLBox *Dentry;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = FlushCaptureMode(movie);
	if (e) return e;

	e = unpack_track(trak);
	if (e) return e;

	//OK, add the sample
	//1- Get the streamDescriptionIndex and dataRefIndex
	//not specified, get the latest used...
	descIndex = StreamDescriptionIndex;
	if (!StreamDescriptionIndex) {
		descIndex = trak->Media->information->sampleTable->currentEntryIndex;
	}
	e = Media_GetSampleDesc(trak->Media, descIndex, &entry, &dataRefIndex);
	if (e) return e;
	if (!entry || !dataRefIndex) return GF_BAD_PARAM;
	//set the current to this one
	trak->Media->information->sampleTable->currentEntryIndex = descIndex;


	//get this dataRef and return false if not self contained
	Dentry = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->child_boxes, dataRefIndex - 1);
	if (!Dentry || Dentry->flags != 1) return GF_BAD_PARAM;

	//Open our data map. We are adding stuff, so use EDIT
	e = gf_isom_datamap_open(trak->Media, dataRefIndex, 1);
	if (e) return e;

	//Get the offset...
	data_offset = gf_isom_datamap_get_offset(trak->Media->information->dataHandler);

	/*rewrite OD frame*/
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		GF_ISOSample *od_sample = NULL;

		e = Media_ParseODFrame(trak->Media, sample, &od_sample);
		if (e) return e;

		e = trak_add_sample(movie, trak, od_sample, descIndex, data_offset, 0);

		if (od_sample)
			gf_isom_sample_del(&od_sample);
	} else {
		e = trak_add_sample(movie, trak, sample, descIndex, data_offset, 0);
	}
	if (e) return e;


	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	return SetTrackDuration(trak);
}

GF_EXPORT
GF_Err gf_isom_add_sample_shadow(GF_ISOFile *movie, u32 trackNumber, GF_ISOSample *sample)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_ISOSample *prev;
	GF_SampleEntryBox *entry;
	u32 dataRefIndex;
	u64 data_offset;
	u32 descIndex;
	u32 sampleNum, prevSampleNum;
	GF_DataEntryURLBox *Dentry;
	Bool offset_times = GF_FALSE;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !sample) return GF_BAD_PARAM;

	e = FlushCaptureMode(movie);
	if (e) return e;

	e = unpack_track(trak);
	if (e) return e;

	e = stbl_findEntryForTime(trak->Media->information->sampleTable, sample->DTS, 0, &sampleNum, &prevSampleNum);
	if (e) return e;
	/*we need the EXACT match*/
	if (!sampleNum) return GF_BAD_PARAM;

	prev = gf_isom_get_sample_info(movie, trackNumber, sampleNum, &descIndex, NULL);
	if (!prev) return gf_isom_last_error(movie);
	/*for conformance*/
	if (sample->DTS==prev->DTS) offset_times = GF_TRUE;
	gf_isom_sample_del(&prev);

	e = Media_GetSampleDesc(trak->Media, descIndex, &entry, &dataRefIndex);
	if (e) return e;
	if (!entry || !dataRefIndex) return GF_BAD_PARAM;
	trak->Media->information->sampleTable->currentEntryIndex = descIndex;

	//get this dataRef and return false if not self contained
	Dentry = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->child_boxes, dataRefIndex - 1);
	if (!Dentry || Dentry->flags != 1) return GF_BAD_PARAM;

	//Open our data map. We are adding stuff, so use EDIT
	e = gf_isom_datamap_open(trak->Media, dataRefIndex, 1);
	if (e) return e;

	data_offset = gf_isom_datamap_get_offset(trak->Media->information->dataHandler);
	if (offset_times) sample->DTS += 1;

	/*REWRITE ANY OD STUFF*/
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		GF_ISOSample *od_sample = NULL;
		e = Media_ParseODFrame(trak->Media, sample, &od_sample);
		if (e) return e;

		e = trak_add_sample(movie, trak, od_sample, descIndex, data_offset, sampleNum);
		if (od_sample)
			gf_isom_sample_del(&od_sample);
	} else {
		e = trak_add_sample(movie, trak, sample, descIndex, data_offset, sampleNum);
	}
	if (e) return e;
	if (offset_times) sample->DTS -= 1;

	//OK, update duration
	e = Media_SetDuration(trak);
	if (e) return e;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	return SetTrackDuration(trak);
}

GF_EXPORT
GF_Err gf_isom_append_sample_data(GF_ISOFile *movie, u32 trackNumber, u8 *data, u32 data_size)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	u32 dataRefIndex;
	u32 descIndex;
	GF_DataEntryURLBox *Dentry;

	if (!data_size) return GF_OK;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_OD) return GF_BAD_PARAM;

	//OK, add the sample
	descIndex = trak->Media->information->sampleTable->currentEntryIndex;

	e = Media_GetSampleDesc(trak->Media, descIndex, &entry, &dataRefIndex);
	if (e) return e;
	if (!entry || !dataRefIndex) return GF_BAD_PARAM;

	//get this dataRef and return false if not self contained
	Dentry = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->child_boxes, dataRefIndex - 1);
	if (!Dentry || Dentry->flags != 1) return GF_BAD_PARAM;

	//Open our data map. We are adding stuff, so use EDIT
	e = gf_isom_datamap_open(trak->Media, dataRefIndex, 1);
	if (e) return e;

	//add the media data
	if (trak->chunk_cache) {
		gf_bs_write_data(trak->chunk_cache, data, data_size);
		trak->chunk_cache_size += data_size;
	} else {
		e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, data, data_size);
		if (e) return e;
	}
	//update data size
	return stbl_SampleSizeAppend(trak->Media->information->sampleTable->SampleSize, data_size);
}


//Add sample reference to a track. The SampleOffset is the offset of the data in the referenced file
//you must have created a StreamDescription with URL or URN specifying your referenced file
//the data offset specifies the beginning of the chunk
//Use streamDescriptionIndex to specify the desired stream (if several)
GF_EXPORT
GF_Err gf_isom_add_sample_reference(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, GF_ISOSample *sample, u64 dataOffset)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	u32 dataRefIndex;
	u32 descIndex;
	GF_DataEntryURLBox *Dentry;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = unpack_track(trak);
	if (e) return e;

	//OD is not allowed as a data ref
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		return GF_BAD_PARAM;
	}
	//OK, add the sample
	//1- Get the streamDescriptionIndex and dataRefIndex
	//not specified, get the latest used...
	descIndex = StreamDescriptionIndex;
	if (!StreamDescriptionIndex) {
		descIndex = trak->Media->information->sampleTable->currentEntryIndex;
	}
	e = Media_GetSampleDesc(trak->Media, descIndex, &entry, &dataRefIndex);
	if (e) return e;
	if (!entry || !dataRefIndex) return GF_BAD_PARAM;
	//set the current to this one
	trak->Media->information->sampleTable->currentEntryIndex = descIndex;


	//get this dataRef and return false if self contained
	Dentry =(GF_DataEntryURLBox*) gf_list_get(trak->Media->information->dataInformation->dref->child_boxes, dataRefIndex - 1);
	if (Dentry->flags == 1) return GF_BAD_PARAM;

	//add the meta data
	e = Media_AddSample(trak->Media, dataOffset, sample, descIndex, 0);
	if (e) return e;

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	//OK, update duration
	e = Media_SetDuration(trak);
	if (e) return e;
	return SetTrackDuration(trak);

}

//set the duration of the last media sample. If not set, the duration of the last sample is the
//duration of the previous one if any, or 1000 (default value).
static GF_Err gf_isom_set_last_sample_duration_internal(GF_ISOFile *movie, u32 trackNumber, u64 dur_num, u32 dur_den, u32 mode)
{
	GF_TrackBox *trak;
	GF_SttsEntry *ent;
	GF_TimeToSampleBox *stts;
	u64 mdur;
	u32 duration;
	GF_Err e;
	Bool is_patch = GF_FALSE;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (mode==0) {
		duration = (u32) dur_num;
	} else if (mode==1) {
		duration = (u32) dur_num;
		if (dur_den) {
			duration *= trak->Media->mediaHeader->timeScale;
			duration /= dur_den;
		}
	} else {
		is_patch = GF_TRUE;
	}
	mdur = trak->Media->mediaHeader->duration;
	stts = trak->Media->information->sampleTable->TimeToSample;
	if (!stts->nb_entries) return GF_BAD_PARAM;

	if (is_patch) {
		u32 i, avg_dur, nb_samp=0;
		u64 cum_dur=0;
		for (i=0; i<stts->nb_entries; i++) {
			ent = (GF_SttsEntry*) &stts->entries[i];
			cum_dur += ent->sampleCount*ent->sampleDelta;
			nb_samp += ent->sampleCount;
		}
		if (cum_dur <= dur_num || !nb_samp) return GF_OK;
		avg_dur = (u32) (dur_num / nb_samp);

		for (i=0; i<stts->nb_entries; i++) {
			ent = (GF_SttsEntry*) &stts->entries[i];
			ent->sampleDelta = avg_dur;
		}
		stts->w_LastDTS = dur_num - avg_dur;
		return GF_OK;
	}
	//get the last entry
	ent = (GF_SttsEntry*) &stts->entries[stts->nb_entries-1];
	if ((mode==1) && !duration && !dur_den) {
		//same as previous, nothing to adjust
		if (ent->sampleCount>1) return GF_OK;
		if (stts->nb_entries==1) return GF_OK;
		duration = stts->entries[stts->nb_entries-2].sampleDelta;
	}

	mdur -= ent->sampleDelta;
	mdur += duration;

	//we only have one sample
	if (ent->sampleCount == 1) {
		ent->sampleDelta = (u32) duration;
		if (mode && (stts->nb_entries>1) && (stts->entries[stts->nb_entries-2].sampleDelta==duration)) {
			stts->entries[stts->nb_entries-2].sampleCount++;
			stts->nb_entries--;
			//and update the write cache
			stts->w_currentSampleNum = trak->Media->information->sampleTable->SampleSize->sampleCount;
		}
	} else {
		if (ent->sampleDelta == duration) return GF_OK;
		ent->sampleCount -= 1;

		if (stts->nb_entries==stts->alloc_size) {
			stts->alloc_size++;
			stts->entries = (GF_SttsEntry*)gf_realloc(stts->entries, sizeof(GF_SttsEntry)*stts->alloc_size);
			if (!stts->entries) return GF_OUT_OF_MEM;
		}
		stts->entries[stts->nb_entries].sampleCount = 1;
		stts->entries[stts->nb_entries].sampleDelta = (u32) duration;
		stts->nb_entries++;
		//and update the write cache
		stts->w_currentSampleNum = trak->Media->information->sampleTable->SampleSize->sampleCount;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	trak->Media->mediaHeader->duration = mdur;
	return SetTrackDuration(trak);
}

GF_EXPORT
GF_Err gf_isom_set_last_sample_duration(GF_ISOFile *movie, u32 trackNumber, u32 duration)
{
	return gf_isom_set_last_sample_duration_internal(movie, trackNumber, duration, 0, 0);
}

GF_EXPORT
GF_Err gf_isom_patch_last_sample_duration(GF_ISOFile *movie, u32 trackNumber, u64 next_dts)
{
	return gf_isom_set_last_sample_duration_internal(movie, trackNumber, next_dts, 0, 2);
}

GF_EXPORT
GF_Err gf_isom_set_last_sample_duration_ex(GF_ISOFile *movie, u32 trackNumber, u32 dur_num, u32 dur_den)
{
	return gf_isom_set_last_sample_duration_internal(movie, trackNumber, dur_num, dur_den, 1);
}

//update a sample data in the media. Note that the sample MUST exists
GF_EXPORT
GF_Err gf_isom_update_sample(GF_ISOFile *movie, u32 trackNumber, u32 sampleNumber, GF_ISOSample *sample, Bool data_only)
{
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_EDIT);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = unpack_track(trak);
	if (e) return e;

	//block for hint tracks
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_HINT) return GF_BAD_PARAM;

	//REWRITE ANY OD STUFF
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		GF_ISOSample *od_sample = NULL;
		e = Media_ParseODFrame(trak->Media, sample, &od_sample);
		if (!e) e = Media_UpdateSample(trak->Media, sampleNumber, od_sample, data_only);
		if (od_sample) gf_isom_sample_del(&od_sample);
	} else {
		e = Media_UpdateSample(trak->Media, sampleNumber, sample, data_only);
	}
	if (e) return e;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	gf_isom_disable_inplace_rewrite(movie);
	return GF_OK;
}

//update a sample data in the media. Note that the sample MUST exists,
//that sample->data MUST be NULL and sample->dataLength must be NON NULL;
GF_EXPORT
GF_Err gf_isom_update_sample_reference(GF_ISOFile *movie, u32 trackNumber, u32 sampleNumber, GF_ISOSample *sample, u64 data_offset)
{
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_EDIT);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	//block for hint tracks
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_HINT) return GF_BAD_PARAM;

	if (!sampleNumber || !sample) return GF_BAD_PARAM;

	e = unpack_track(trak);
	if (e) return e;

	//OD is not allowed as a data ref
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		return GF_BAD_PARAM;
	}
	//OK, update it
	e = Media_UpdateSampleReference(trak->Media, sampleNumber, sample, data_offset);
	if (e) return e;

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	return GF_OK;
}


//Remove a given sample
GF_EXPORT
GF_Err gf_isom_remove_sample(GF_ISOFile *movie, u32 trackNumber, u32 sampleNumber)
{
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_EDIT);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !sampleNumber || (sampleNumber > trak->Media->information->sampleTable->SampleSize->sampleCount) )
		return GF_BAD_PARAM;

	//block for hint tracks
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_HINT) return GF_BAD_PARAM;

	e = unpack_track(trak);
	if (e) return e;
	//do NOT change the order DTS, CTS, size chunk

	//remove DTS
	e = stbl_RemoveDTS(trak->Media->information->sampleTable, sampleNumber, 1, trak->Media->mediaHeader->timeScale);
	if (e) return e;
	//remove CTS if any
	if (trak->Media->information->sampleTable->CompositionOffset) {
		e = stbl_RemoveCTS(trak->Media->information->sampleTable, sampleNumber, 1);
		if (e) return e;
	}
	//remove size
	e = stbl_RemoveSize(trak->Media->information->sampleTable, sampleNumber, 1);
	if (e) return e;
	//remove sampleToChunk and chunk
	e = stbl_RemoveChunk(trak->Media->information->sampleTable, sampleNumber, 1);
	if (e) return e;
	//remove sync
	if (trak->Media->information->sampleTable->SyncSample) {
		e = stbl_RemoveRAP(trak->Media->information->sampleTable, sampleNumber);
		if (e) return e;
	}
	//remove sample dep
	if (trak->Media->information->sampleTable->SampleDep) {
		e = stbl_RemoveRedundant(trak->Media->information->sampleTable, sampleNumber, 1);
		if (e) return e;
	}
	//remove shadow
	e = stbl_RemoveShadow(trak->Media->information->sampleTable, sampleNumber);
	if (e) return e;

	//remove padding
	e = stbl_RemovePaddingBits(trak->Media->information->sampleTable, sampleNumber);
	if (e) return e;

	e = stbl_RemoveSubSample(trak->Media->information->sampleTable, sampleNumber);
	if (e) return e;

	e = stbl_RemoveSampleGroup(trak->Media->information->sampleTable, sampleNumber);
	if (e) return e;

	gf_isom_disable_inplace_rewrite(movie);

	return SetTrackDuration(trak);
}


GF_EXPORT
GF_Err gf_isom_set_final_name(GF_ISOFile *movie, char *filename)
{
	GF_Err e;
	if (!movie ) return GF_BAD_PARAM;

	//if mode is not OPEN_EDIT file was created under the right name
	e = CanAccessMovie(movie, GF_ISOM_OPEN_EDIT);
	if (e) return e;

	if (filename) {
		//we don't allow file overwriting
		if ( (movie->openMode == GF_ISOM_OPEN_EDIT)
		        && movie->fileName && !strcmp(filename, movie->fileName))
			return GF_BAD_PARAM;
		if (movie->finalName) gf_free(movie->finalName);
		movie->finalName = gf_strdup(filename);
		if (!movie->finalName) return GF_OUT_OF_MEM;
		gf_isom_disable_inplace_rewrite(movie);
	}
	return GF_OK;
}

//Add a system descriptor to the ESD of a stream(EDIT or WRITE mode only)
GF_EXPORT
GF_Err gf_isom_add_desc_to_description(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, const GF_Descriptor *theDesc)
{
	GF_IPIPtr *ipiD;
	GF_Err e;
	u16 tmpRef;
	GF_TrackBox *trak;
	GF_Descriptor *desc;
	GF_ESD *esd;
	GF_TrackReferenceBox *tref;
	GF_TrackReferenceTypeBox *dpnd;
	GF_MPEGVisualSampleEntryBox *entry;
	u32 msubtype;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	/*GETS NATIVE DESCRIPTOR ONLY*/
	e = Media_GetESD(trak->Media, StreamDescriptionIndex, &esd, GF_TRUE);
	if (e) return e;

	entry = gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	msubtype = entry->type;
	if ((msubtype==GF_ISOM_BOX_TYPE_ENCV) || (msubtype==GF_ISOM_BOX_TYPE_ENCA))
		gf_isom_get_original_format_type(movie, trackNumber, StreamDescriptionIndex, &msubtype);

	//duplicate the desc
	e = gf_odf_desc_copy((GF_Descriptor *)theDesc, &desc);
	if (e) return e;

	//and add it to the ESD EXCEPT IPI PTR (we need to translate from ES_ID to TrackID!!!
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	switch (desc->tag) {
	case GF_ODF_IPI_PTR_TAG:
		goto insertIPI;
	default:
		break;
	}

	if ((msubtype==GF_ISOM_BOX_TYPE_MP4S) || (msubtype==GF_ISOM_BOX_TYPE_MP4V) || (msubtype==GF_ISOM_BOX_TYPE_MP4A)) {
		return gf_odf_desc_add_desc((GF_Descriptor *)esd, desc);
	}

	if (trak->Media->handler->handlerType!=GF_ISOM_MEDIA_VISUAL) {
		gf_odf_desc_del(desc);
		return GF_NOT_SUPPORTED;
	}
	GF_MPEG4ExtensionDescriptorsBox *mdesc = (GF_MPEG4ExtensionDescriptorsBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_M4DS);
	if (!mdesc) {
		mdesc = (GF_MPEG4ExtensionDescriptorsBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_M4DS);
	}
	return gf_list_add(mdesc->descriptors, desc);

insertIPI:
	if (esd->ipiPtr) {
		gf_odf_desc_del((GF_Descriptor *) esd->ipiPtr);
		esd->ipiPtr = NULL;
	}

	ipiD = (GF_IPIPtr *) desc;
	//find a tref
	if (!trak->References) {
		tref = (GF_TrackReferenceBox *) gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_TREF);
		if (!tref) return GF_OUT_OF_MEM;
		e = trak_on_child_box((GF_Box*)trak, (GF_Box *)tref, GF_FALSE);
		if (e) return e;
	}
	tref = trak->References;

	e = Track_FindRef(trak, GF_ISOM_REF_IPI, &dpnd);
	if (e) return e;
	if (!dpnd) {
		tmpRef = 0;
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new_parent(&tref->child_boxes, GF_ISOM_BOX_TYPE_REFT);
		if (!dpnd) return GF_OUT_OF_MEM;
		dpnd->reference_type = GF_ISOM_BOX_TYPE_IPIR;
		e = reftype_AddRefTrack(dpnd, ipiD->IPI_ES_Id, &tmpRef);
		if (e) return e;
		//and replace the tag and value...
		ipiD->IPI_ES_Id = tmpRef;
		ipiD->tag = GF_ODF_ISOM_IPI_PTR_TAG;
	} else {
		//Watch out! ONLY ONE IPI dependency is allowed per stream
		dpnd->trackIDCount = 1;
		dpnd->trackIDs[0] = ipiD->IPI_ES_Id;
		//and replace the tag and value...
		ipiD->IPI_ES_Id = 1;
		ipiD->tag = GF_ODF_ISOM_IPI_PTR_TAG;
	}
	//and add the desc to the esd...
	return gf_odf_desc_add_desc((GF_Descriptor *)esd, desc);
}


//use carefully. Very useful when you made a lot of changes (IPMP, IPI, OCI, ...)
//THIS WILL REPLACE THE WHOLE DESCRIPTOR ...
GF_EXPORT
GF_Err gf_isom_change_mpeg4_description(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, const GF_ESD *newESD)
{
	GF_Err e;
	GF_ESD *esd;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;

	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	//duplicate our desc
	e = gf_odf_desc_copy((GF_Descriptor *)newESD, (GF_Descriptor **)&esd);
	if (e) return e;
	e = Track_SetStreamDescriptor(trak, StreamDescriptionIndex, entry->dataReferenceIndex, esd, NULL);
	if (e != GF_OK) {
		gf_odf_desc_del((GF_Descriptor *) esd);
	}
	return e;
}

GF_EXPORT
GF_Err gf_isom_set_visual_info(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 Width, u32 Height)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) {
		return movie->LastError = GF_ISOM_INVALID_FILE;
	}
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	//valid for MPEG visual, JPG and 3GPP H263
	if (entry->internal_type == GF_ISOM_SAMPLE_ENTRY_VIDEO) {
		((GF_VisualSampleEntryBox*)entry)->Width = Width;
		((GF_VisualSampleEntryBox*)entry)->Height = Height;
		trak->Header->width = Width<<16;
		trak->Header->height = Height<<16;
		return GF_OK;
	} else if (trak->Media->handler->handlerType==GF_ISOM_MEDIA_SCENE) {
		trak->Header->width = Width<<16;
		trak->Header->height = Height<<16;
		return GF_OK;
	} else {
		return GF_BAD_PARAM;
	}
}

GF_EXPORT
GF_Err gf_isom_set_visual_bit_depth(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u16 bitDepth)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_VISUAL:
	case GF_ISOM_MEDIA_PICT:
	case GF_ISOM_MEDIA_AUXV:
		break;
	default:
		return GF_OK;
	}

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) {
		return movie->LastError = GF_ISOM_INVALID_FILE;
	}
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_MPEGVisualSampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	entry->bit_depth = bitDepth;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_pixel_aspect_ratio(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, s32 hSpacing, s32 vSpacing, Bool force_par)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_BAD_PARAM;

	GF_PixelAspectRatioBox *pasp = (GF_PixelAspectRatioBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_PASP);
	if (!hSpacing || !vSpacing || ((hSpacing == vSpacing) && !force_par))  {
		if (pasp) gf_isom_box_del_parent(&entry->child_boxes, (GF_Box *)pasp);
		return GF_OK;
	}
	if (!pasp) {
		pasp = (GF_PixelAspectRatioBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_PASP);
		if (!pasp) return GF_OUT_OF_MEM;
	}
	pasp->hSpacing = (u32) hSpacing;
	pasp->vSpacing = (u32) vSpacing;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_visual_color_info(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 colour_type, u16 colour_primaries, u16 transfer_characteristics, u16 matrix_coefficients, Bool full_range_flag, u8 *icc_data, u32 icc_size)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;
	GF_ColourInformationBox *clr=NULL;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_OK;

	clr = (GF_ColourInformationBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_COLR);
	if (!colour_type) {
		if (clr) gf_isom_box_del_parent(&entry->child_boxes, (GF_Box *)clr);
		return GF_OK;
	}
	if (!clr) {
		clr = (GF_ColourInformationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_COLR);
		if (!clr) return GF_OUT_OF_MEM;
	}
	clr->colour_type = colour_type;
	clr->colour_primaries = colour_primaries;
	clr->transfer_characteristics = transfer_characteristics;
	clr->matrix_coefficients = matrix_coefficients;
	clr->full_range_flag = full_range_flag;
	if (clr->opaque) gf_free(clr->opaque);
	clr->opaque = NULL;
	clr->opaque_size = 0;
	if ((colour_type==GF_ISOM_SUBTYPE_RICC) || (colour_type==GF_ISOM_SUBTYPE_PROF)) {
		clr->opaque_size = icc_data ? icc_size : 0;
		if (clr->opaque_size) {
			clr->opaque = gf_malloc(sizeof(char)*clr->opaque_size);
			if (!clr->opaque) return GF_OUT_OF_MEM;
			memcpy(clr->opaque, icc_data, sizeof(char)*clr->opaque_size);
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_dolby_vision_profile(GF_ISOFile* movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 dv_profile)
{
	GF_Err e;
	GF_TrackBox* trak;
	GF_SampleEntryBox* entry;
	GF_SampleDescriptionBox* stsd;
	GF_DOVIConfigurationBox* dovi = NULL;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox*)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_OK;

	dovi = ((GF_MPEGVisualSampleEntryBox*)entry)->dovi_config;
	if (!dv_profile) {
		if (dovi) gf_isom_box_del((GF_Box*)dovi);
		((GF_MPEGVisualSampleEntryBox*)entry)->dovi_config = NULL;
		return GF_OK;
	}
	if (!dovi) {
		dovi = (GF_DOVIConfigurationBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_DVCC);
		if (!dovi) return GF_OUT_OF_MEM;
		((GF_MPEGVisualSampleEntryBox*)entry)->dovi_config = dovi;
	}
	entry->type = GF_ISOM_BOX_TYPE_DVHE;
	dovi->DOVIConfig.dv_profile = dv_profile;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_high_dynamic_range_info(GF_ISOFile* movie, u32 trackNumber, u32 StreamDescriptionIndex, GF_MasteringDisplayColourVolumeInfo* mdcv, GF_ContentLightLevelInfo* clli)
{
	GF_Err e;
	GF_TrackBox* trak;
	GF_SampleEntryBox* entry;
	GF_SampleDescriptionBox* stsd;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox*)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_BAD_PARAM;

	GF_MasteringDisplayColourVolumeBox *mdcvb = (GF_MasteringDisplayColourVolumeBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_MDCV);
	if (!mdcvb) {
		mdcvb = (GF_MasteringDisplayColourVolumeBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_MDCV);
		if (!mdcvb) return GF_OUT_OF_MEM;
	}
	mdcvb->mdcv = *mdcv;

	/*clli*/
	GF_ContentLightLevelBox *cllib = (GF_ContentLightLevelBox *)gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_CLLI);
	if (!cllib) {
		cllib = (GF_ContentLightLevelBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_CLLI);
		if (!cllib) return GF_OUT_OF_MEM;
	}
	cllib->clli = *clli;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_clean_aperture(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 cleanApertureWidthN, u32 cleanApertureWidthD, u32 cleanApertureHeightN, u32 cleanApertureHeightD, u32 horizOffN, u32 horizOffD, u32 vertOffN, u32 vertOffD)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_BAD_PARAM;

	GF_CleanApertureBox *clap = (GF_CleanApertureBox *)gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_CLAP);
	if (!cleanApertureHeightD || !cleanApertureWidthD || !horizOffD || !vertOffD) {
		if (clap) gf_isom_box_del_parent(&entry->child_boxes, (GF_Box*)clap);
		return GF_OK;
	}
	if (!clap) {
		clap = (GF_CleanApertureBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_CLAP);
		if (!clap) return GF_OUT_OF_MEM;
	}

	clap->cleanApertureWidthN = cleanApertureWidthN;
	clap->cleanApertureWidthD = cleanApertureWidthD;
	clap->cleanApertureHeightN = cleanApertureHeightN;
	clap->cleanApertureHeightD = cleanApertureHeightD;
	clap->horizOffN = horizOffN;
	clap->horizOffD = horizOffD;
	clap->vertOffN = vertOffN;
	clap->vertOffD = vertOffD;
	return GF_OK;
}

#include <gpac/maths.h>
GF_Err gf_isom_update_aperture_info(GF_ISOFile *movie, u32 trackNumber, Bool remove)
{
	GF_Err e;
	GF_Box *box, *enof, *prof, *clef;
	GF_TrackBox *trak;
	GF_VisualSampleEntryBox *ventry;
	GF_PixelAspectRatioBox *pasp;
	GF_CleanApertureBox *clap;
	u32 j, hspacing, vspacing, clap_width_num, clap_width_den, clap_height_num, clap_height_den, high, low;
	Double width, height;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (remove) {
		if (trak->Aperture) {
			gf_isom_box_del(trak->Aperture);
			trak->Aperture = NULL;
		}
		return GF_OK;
	}
	enof = prof = clef = NULL;
	if (!trak->Aperture) {
		trak->Aperture = gf_isom_box_new_parent(&trak->child_boxes, GF_QT_BOX_TYPE_TAPT);
		if (!trak->Aperture) return GF_OUT_OF_MEM;
	}
	if (!trak->Aperture->child_boxes) {
		trak->Aperture->child_boxes = gf_list_new();
		if (!trak->Aperture->child_boxes)
			return GF_OUT_OF_MEM;
	}

	j=0;
	while ( (box = gf_list_enum(trak->Aperture->child_boxes, &j))) {
		switch (box->type) {
		case GF_QT_BOX_TYPE_CLEF: clef = box; break;
		case GF_QT_BOX_TYPE_PROF: prof = box; break;
		case GF_QT_BOX_TYPE_ENOF: enof = box; break;
		}
	}
	if (!clef) {
		clef = gf_isom_box_new(GF_QT_BOX_TYPE_CLEF);
		if (!clef) return GF_OUT_OF_MEM;
		gf_list_add(trak->Aperture->child_boxes, clef);
	}
	if (!enof) {
		enof = gf_isom_box_new(GF_QT_BOX_TYPE_ENOF);
		if (!enof) return GF_OUT_OF_MEM;
		gf_list_add(trak->Aperture->child_boxes, enof);
	}
	if (!prof) {
		prof = gf_isom_box_new(GF_QT_BOX_TYPE_PROF);
		if (!prof) return GF_OUT_OF_MEM;
		gf_list_add(trak->Aperture->child_boxes, prof);
	}

	ventry = (GF_VisualSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, 0);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (ventry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (ventry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_BAD_PARAM;

	pasp = (GF_PixelAspectRatioBox *) gf_isom_box_find_child(ventry->child_boxes, GF_ISOM_BOX_TYPE_PASP);
	hspacing = vspacing = 0;
	if (pasp) {
		hspacing = pasp->hSpacing;
		vspacing = pasp->vSpacing;
	}
	clap_width_num = ventry->Width;
	clap_width_den = 1;
	clap_height_num = ventry->Height;
	clap_height_den = 1;
	clap = (GF_CleanApertureBox *) gf_isom_box_find_child(ventry->child_boxes, GF_ISOM_BOX_TYPE_CLAP);
	if (clap) {
		clap_width_num = clap->cleanApertureWidthN;
		clap_width_den = clap->cleanApertureWidthD;
		clap_height_num = clap->cleanApertureHeightN;
		clap_height_den = clap->cleanApertureHeightD;
	}
	//enof: encoded pixels in 16.16
	((GF_ApertureBox *)enof)->width = (ventry->Width)<<16;
	((GF_ApertureBox *)enof)->height = (ventry->Height)<<16;

	//prof: encoded pixels + pasp in 16.16
	width = (Float) (ventry->Width * hspacing);
	width /= vspacing;
	high = (u32) floor((Float)width);
	low = (u32) ( 0xFFFF * (width - (Double)high) );
	((GF_ApertureBox *)prof)->width = (high)<<16 | low;
	((GF_ApertureBox *)prof)->height = (ventry->Height)<<16;

	//clef: encoded pixels + pasp + clap in 16.16
	width = (Double) (clap_width_num * hspacing);
	width /= clap_width_den * vspacing;
	height = (Float) clap_height_num;
	height /= clap_height_den;

	high = (u32) floor((Float)width);
	low = (u32) (0xFFFF * (width - (Double)high));
	((GF_ApertureBox *)clef)->width = (high)<<16 | low;
	high = (u32) floor((Float)height);
	low = (u32) (0xFFFF * (height - (Double)high));
	((GF_ApertureBox *)clef)->height = (high)<<16 | low;


	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_image_sequence_coding_constraints(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, Bool remove, Bool all_ref_pics_intra, Bool intra_pred_used, u32 max_ref_per_pic)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_BAD_PARAM;

	GF_CodingConstraintsBox*ccst = (GF_CodingConstraintsBox*) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_CCST);
	if (remove)  {
		if (ccst) gf_isom_box_del_parent(&entry->child_boxes, (GF_Box*)ccst);
		return GF_OK;
	}
	if (!ccst) {
		ccst = (GF_CodingConstraintsBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_CCST);
		if (!ccst) return GF_OUT_OF_MEM;
	}
	ccst->all_ref_pics_intra = all_ref_pics_intra;
	ccst->intra_pred_used = intra_pred_used;
	ccst->max_ref_per_pic = max_ref_per_pic;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_image_sequence_alpha(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, Bool remove)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_BAD_PARAM;

	GF_AuxiliaryTypeInfoBox *auxi = (GF_AuxiliaryTypeInfoBox *)gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_AUXI);
	if (remove)  {
		if (auxi) gf_isom_box_del_parent(&entry->child_boxes, (GF_Box*)auxi);
		return GF_OK;
	}
	if (!auxi) {
		auxi = (GF_AuxiliaryTypeInfoBox*)gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_AUXI);
		if (!auxi) return GF_OUT_OF_MEM;
	}
	auxi->aux_track_type = gf_strdup("urn:mpeg:mpegB:cicp:systems:auxiliary:alpha");
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_audio_info(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 sampleRate, u32 nbChannels, u8 bitsPerSample, GF_AudioSampleEntryImportMode asemode)
{
	GF_Err e;
	u32 i, old_qtff_mode=GF_ISOM_AUDIO_QTFF_NONE;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_AudioSampleEntryBox*aud_entry;
	GF_SampleDescriptionBox *stsd;
	GF_Box *wave_box = NULL;
	GF_Box *gf_isom_audio_sample_get_audio_codec_cfg_box(GF_AudioSampleEntryBox *ptr);
	GF_Box *codec_ext = NULL;
#if 0
	GF_ChannelLayoutInfoBox *chan=NULL;
#endif
	GF_OriginalFormatBox *frma=NULL;
	GF_ChromaInfoBox *enda=NULL;
	GF_ESDBox *esds=NULL;
	GF_Box *terminator=NULL;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) {
		return movie->LastError = GF_ISOM_INVALID_FILE;
	}
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return GF_BAD_PARAM;
	aud_entry = (GF_AudioSampleEntryBox*) entry;

	if (entry->type==GF_ISOM_BOX_TYPE_MLPA) {
		aud_entry->samplerate_hi = sampleRate>>16;
		aud_entry->samplerate_lo = sampleRate & 0x0000FFFF;
	} else {
		aud_entry->samplerate_hi = sampleRate;
		aud_entry->samplerate_lo = 0;
	}
	aud_entry->bitspersample = bitsPerSample;

	switch (asemode) {
	case GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_2:
		stsd->version = 0;
		aud_entry->version = 0;
		aud_entry->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
		aud_entry->channel_count = 2;
		break;
	case GF_IMPORT_AUDIO_SAMPLE_ENTRY_NOT_SET:
	case GF_IMPORT_AUDIO_SAMPLE_ENTRY_v0_BS:
		stsd->version = 0;
		aud_entry->version = 0;
		aud_entry->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
		aud_entry->channel_count = nbChannels;
		break;
	case GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_MPEG:
		stsd->version = 1;
		aud_entry->version = 1;
		aud_entry->qtff_mode = GF_ISOM_AUDIO_QTFF_NONE;
		aud_entry->channel_count = nbChannels;
		break;
	case GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF:
		stsd->version = 0;
		aud_entry->version = 1;
		aud_entry->channel_count = nbChannels;
		old_qtff_mode = aud_entry->qtff_mode;
		if (aud_entry->qtff_mode != GF_ISOM_AUDIO_QTFF_ON_EXT_VALID)
			aud_entry->qtff_mode = GF_ISOM_AUDIO_QTFF_ON_NOEXT;
		break;
	}

	aud_entry->compression_id = 0;

	//check for wave+children and chan for QTFF or remove them for isobmff
	for (i=0; i<gf_list_count(aud_entry->child_boxes); i++) {
		GF_Box *b = gf_list_get(aud_entry->child_boxes, i);
		if ((b->type != GF_QT_BOX_TYPE_WAVE) && (b->type != GF_QT_BOX_TYPE_CHAN) ) continue;
		if (asemode==GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF) {
			if (b->type == GF_QT_BOX_TYPE_WAVE) wave_box = b;
#if 0
			else chan = (GF_ChannelLayoutInfoBox *)b;
#endif

		} else {
			gf_isom_box_del_parent(&aud_entry->child_boxes, b);
			i--;
		}
	}

	//TODO: insert channelLayout for ISOBMFF
	if (asemode!=GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_QTFF) return GF_OK;

	if (aud_entry->type==GF_ISOM_BOX_TYPE_MP4A)
		aud_entry->compression_id = -2;

	if (!aud_entry->child_boxes) aud_entry->child_boxes = gf_list_new();

#if 0
	if (!chan) {
		chan = (GF_ChannelLayoutInfoBox *) gf_isom_box_new_parent(&aud_entry->child_boxes, GF_QT_BOX_TYPE_CHAN);
	}
	//TODO, proper channel mapping
	chan->layout_tag = (nbChannels==2) ? 6750210 : 6553601;
#endif

	codec_ext = gf_isom_audio_sample_get_audio_codec_cfg_box((GF_AudioSampleEntryBox *)aud_entry);
	if (!codec_ext) return GF_OK;

	if (!wave_box) {
		wave_box = gf_isom_box_new_parent(&aud_entry->child_boxes, GF_QT_BOX_TYPE_WAVE);
	}

	for (i=0; i<gf_list_count(wave_box->child_boxes); i++) {
		GF_Box *b = gf_list_get(wave_box->child_boxes, i);
		switch (b->type) {
		case GF_QT_BOX_TYPE_ENDA:
			enda = (GF_ChromaInfoBox *)b;
			break;
		case GF_QT_BOX_TYPE_FRMA:
			frma = (GF_OriginalFormatBox *)b;
			break;
		case GF_ISOM_BOX_TYPE_ESDS:
			esds = (GF_ESDBox *)b;
			break;
		case GF_ISOM_BOX_TYPE_UNKNOWN:
			if ( ((GF_UnknownBox*)b)->original_4cc == 0)
				terminator = b;
			break;
		case 0:
			terminator = b;
			break;
		}
	}
	if (!wave_box->child_boxes) wave_box->child_boxes = gf_list_new();

	//do not use new_parent, we do this manually to ensure the order
	aud_entry->qtff_mode = old_qtff_mode ? old_qtff_mode : GF_ISOM_AUDIO_QTFF_ON_NOEXT;
	if (!frma) {
		frma = (GF_OriginalFormatBox *)gf_isom_box_new(GF_QT_BOX_TYPE_FRMA);
	} else {
		gf_list_del_item(wave_box->child_boxes, frma);
	}
	gf_list_add(wave_box->child_boxes, frma);

	if (esds) gf_list_del_item(wave_box->child_boxes, esds);
	if (!esds && (aud_entry->type==GF_ISOM_BOX_TYPE_MP4A) && ((GF_MPEGAudioSampleEntryBox*)aud_entry)->esd) {
		gf_list_del_item(entry->child_boxes, (GF_Box *) ((GF_MPEGAudioSampleEntryBox*)aud_entry)->esd);
		gf_list_add(wave_box->child_boxes, (GF_Box *) ((GF_MPEGAudioSampleEntryBox*)aud_entry)->esd);
	}

	if (!enda) {
		enda = (GF_ChromaInfoBox *)gf_isom_box_new(GF_QT_BOX_TYPE_ENDA);
	} else {
		gf_list_del_item(wave_box->child_boxes, enda);
	}
	enda->chroma=1;
	gf_list_add(wave_box->child_boxes, enda);

	if (!terminator) {
		terminator = gf_isom_box_new(0);
	} else {
		gf_list_del_item(wave_box->child_boxes, terminator);
	}
	gf_list_add(wave_box->child_boxes, terminator);

	if (aud_entry->type==GF_ISOM_BOX_TYPE_GNRA) {
		frma->data_format = ((GF_GenericAudioSampleEntryBox*) aud_entry)->EntryType;
	} else {
		frma->data_format = aud_entry->type;
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_audio_layout(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescriptionIndex, GF_AudioChannelLayout *layout)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_AudioSampleEntryBox*aud_entry;
	GF_SampleDescriptionBox *stsd;
	GF_ChannelLayoutBox *chnl;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) {
		return movie->LastError = GF_ISOM_INVALID_FILE;
	}
	if (!sampleDescriptionIndex || sampleDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, sampleDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return GF_BAD_PARAM;
	aud_entry = (GF_AudioSampleEntryBox*) entry;
	if (aud_entry->qtff_mode) {
		u32 sr = aud_entry->samplerate_hi;
		if (aud_entry->type==GF_ISOM_BOX_TYPE_MLPA) {
			sr <<= 16;
			sr |= aud_entry->samplerate_lo;
		}
		e = gf_isom_set_audio_info(movie, trackNumber, sampleDescriptionIndex, sr, aud_entry->channel_count, (u8) aud_entry->bitspersample, GF_IMPORT_AUDIO_SAMPLE_ENTRY_v1_MPEG);
		if (e) return e;
	}
	chnl = (GF_ChannelLayoutBox *) gf_isom_box_find_child(aud_entry->child_boxes, GF_ISOM_BOX_TYPE_CHNL);
	if (!chnl) {
		chnl = (GF_ChannelLayoutBox *)gf_isom_box_new_parent(&aud_entry->child_boxes, GF_ISOM_BOX_TYPE_CHNL);
		if (!chnl) return GF_OUT_OF_MEM;
	}
	aud_entry->channel_count = layout->channels_count;
	memcpy(&chnl->layout, layout, sizeof(GF_AudioChannelLayout));
	return GF_OK;
}

//set the storage mode of a file (FLAT, STREAMABLE, INTERLEAVED)
GF_EXPORT
GF_Err gf_isom_set_storage_mode(GF_ISOFile *movie, GF_ISOStorageMode storageMode)
{
	GF_Err e;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	switch (storageMode) {
	case GF_ISOM_STORE_FLAT:
	case GF_ISOM_STORE_STREAMABLE:
	case GF_ISOM_STORE_INTERLEAVED:
	case GF_ISOM_STORE_DRIFT_INTERLEAVED:
	case GF_ISOM_STORE_TIGHT:
	case GF_ISOM_STORE_FASTSTART:
		movie->storageMode = storageMode;
		//specifying a storage mode disables inplace rewrite
		gf_isom_disable_inplace_rewrite(movie);
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


GF_EXPORT
GF_Err gf_isom_enable_compression(GF_ISOFile *file, GF_ISOCompressMode compress_mode, Bool force_compress)
{
	if (!file) return GF_BAD_PARAM;
	file->compress_mode = compress_mode;
	file->force_compress = force_compress;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_force_64bit_chunk_offset(GF_ISOFile *file, Bool set_on)
{
	if (!file) return GF_BAD_PARAM;
	file->force_co64 = set_on;
	return GF_OK;
}


//update or insert a new edit segment in the track time line. Edits are used to modify
//the media normal timing. EditTime and EditDuration are expressed in Movie TimeScale
//If a segment with EditTime already exists, IT IS ERASED
static GF_Err gf_isom_set_edit_internal(GF_ISOFile *movie, u32 trackNumber, u64 EditTime, u64 EditDuration, u64 MediaTime, u32 media_rate, GF_ISOEditType EditMode)
{
	GF_TrackBox *trak;
	GF_EditBox *edts;
	GF_EditListBox *elst;
	GF_EdtsEntry *ent, *newEnt;
	u32 i;
	GF_Err e;
	u64 startTime;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	edts = trak->editBox;
	if (! edts) {
		edts = (GF_EditBox *) gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_EDTS);
		if (!edts) return GF_OUT_OF_MEM;
		trak_on_child_box((GF_Box*)trak, (GF_Box *)edts, GF_FALSE);
	}
	elst = edts->editList;
	if (!elst) {
		elst = (GF_EditListBox *) gf_isom_box_new_parent(&edts->child_boxes, GF_ISOM_BOX_TYPE_ELST);
		if (!elst) return GF_OUT_OF_MEM;
		edts_on_child_box((GF_Box*)edts, (GF_Box *)elst, GF_FALSE);
	}

	startTime = 0;
	ent = NULL;
	//get the prev entry to this startTime if any
	i=0;
	while ((ent = (GF_EdtsEntry *)gf_list_enum(elst->entryList, &i))) {
		if ( (startTime <= EditTime) && (startTime + ent->segmentDuration > EditTime) )
			goto found;
		startTime += ent->segmentDuration;
	}

	//not found, add a new entry and adjust the prev one if any
	if (!ent) {
		newEnt = CreateEditEntry(EditDuration, MediaTime, EditMode);
		if (!newEnt) return GF_OUT_OF_MEM;
		if (EditMode==GF_ISOM_EDIT_NORMAL+1) {
			newEnt->mediaRate = media_rate;
		}
		gf_list_add(elst->entryList, newEnt);
		return SetTrackDuration(trak);
	}

	startTime -= ent->segmentDuration;

found:

	//if same time, we erase the current one...
	if (startTime == EditTime) {
		ent->segmentDuration = EditDuration;
		if (EditMode==GF_ISOM_EDIT_NORMAL+1) {
			ent->mediaRate = media_rate;
			ent->mediaTime = MediaTime;
		} else {
			switch (EditMode) {
			case GF_ISOM_EDIT_EMPTY:
				ent->mediaRate = 0x10000;
				ent->mediaTime = -1;
				break;
			case GF_ISOM_EDIT_DWELL:
				ent->mediaRate = 0;
				ent->mediaTime = MediaTime;
				break;
			default:
				ent->mediaRate = 0x10000;
				ent->mediaTime = MediaTime;
				break;
			}
		}
		return SetTrackDuration(trak);
	}

	//adjust so that the prev ent leads to EntryTime
	//Note: we don't change the next one as it is unknown to us in
	//a lot of case (the author's changes)
	ent->segmentDuration = EditTime - startTime;
	newEnt = CreateEditEntry(EditDuration, MediaTime, EditMode);
	if (!newEnt) return GF_OUT_OF_MEM;
	if (EditMode==GF_ISOM_EDIT_NORMAL+1) {
		newEnt->mediaRate = media_rate;
		newEnt->mediaTime = MediaTime;
	}
	//is it the last entry ???
	if (i >= gf_list_count(elst->entryList) - 1) {
		//add the new entry at the end
		gf_list_add(elst->entryList, newEnt);
		return SetTrackDuration(trak);
	} else {
		//insert after the current entry (which is i)
		gf_list_insert(elst->entryList, newEnt, i+1);
		return SetTrackDuration(trak);
	}
}

GF_EXPORT
GF_Err gf_isom_set_edit(GF_ISOFile *movie, u32 trackNumber, u64 EditTime, u64 EditDuration, u64 MediaTime, GF_ISOEditType EditMode)
{
	return gf_isom_set_edit_internal(movie, trackNumber, EditTime, EditDuration, MediaTime, 0, EditMode);
}

GF_EXPORT
GF_Err gf_isom_set_edit_with_rate(GF_ISOFile *movie, u32 trackNumber, u64 EditTime, u64 EditDuration, u64 MediaTime, u32 media_rate)
{
	return gf_isom_set_edit_internal(movie, trackNumber, EditTime, EditDuration, MediaTime, media_rate, GF_ISOM_EDIT_NORMAL+1);

}

//remove the edit segments for the whole track
GF_EXPORT
GF_Err gf_isom_remove_edits(GF_ISOFile *movie, u32 trackNumber)
{
	GF_Err e;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!trak->editBox || !trak->editBox->editList) return GF_OK;

	while (gf_list_count(trak->editBox->editList->entryList)) {
		GF_EdtsEntry *ent = (GF_EdtsEntry*)gf_list_get(trak->editBox->editList->entryList, 0);
		gf_free(ent);
		e = gf_list_rem(trak->editBox->editList->entryList, 0);
		if (e) return e;
	}
	//then delete the GF_EditBox...
	gf_isom_box_del_parent(&trak->child_boxes, (GF_Box *)trak->editBox);
	trak->editBox = NULL;
	return SetTrackDuration(trak);
}


//remove the edit segments for the whole track
GF_EXPORT
GF_Err gf_isom_remove_edit(GF_ISOFile *movie, u32 trackNumber, u32 seg_index)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_EdtsEntry *ent, *next_ent;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !seg_index) return GF_BAD_PARAM;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!trak->editBox || !trak->editBox->editList) return GF_OK;
	if (gf_list_count(trak->editBox->editList->entryList)<=1) return gf_isom_remove_edits(movie, trackNumber);

	ent = (GF_EdtsEntry*) gf_list_get(trak->editBox->editList->entryList, seg_index-1);
	gf_list_rem(trak->editBox->editList->entryList, seg_index-1);
	next_ent = (GF_EdtsEntry *)gf_list_get(trak->editBox->editList->entryList, seg_index-1);
	if (next_ent) next_ent->segmentDuration += ent->segmentDuration;
	gf_free(ent);
	return SetTrackDuration(trak);
}


GF_EXPORT
GF_Err gf_isom_append_edit(GF_ISOFile *movie, u32 trackNumber, u64 EditDuration, u64 MediaTime, GF_ISOEditType EditMode)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_EdtsEntry *ent;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!trak->editBox) {
		GF_EditBox *edts = (GF_EditBox *) gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_EDTS);
		if (!edts) return GF_OUT_OF_MEM;
		trak_on_child_box((GF_Box*)trak, (GF_Box *)edts, GF_FALSE);
		assert(trak->editBox);
	}
	if (!trak->editBox->editList) {
		GF_EditListBox *elst = (GF_EditListBox *) gf_isom_box_new_parent(&trak->editBox->child_boxes, GF_ISOM_BOX_TYPE_ELST);
		if (!elst) return GF_OUT_OF_MEM;
		edts_on_child_box((GF_Box*)trak->editBox, (GF_Box *)elst, GF_FALSE);
		assert(trak->editBox->editList);
	}
	ent = (GF_EdtsEntry *)gf_malloc(sizeof(GF_EdtsEntry));
	if (!ent) return GF_OUT_OF_MEM;

	ent->segmentDuration = EditDuration;
	switch (EditMode) {
	case GF_ISOM_EDIT_EMPTY:
		ent->mediaRate = 0x10000;
		ent->mediaTime = -1;
		break;
	case GF_ISOM_EDIT_DWELL:
		ent->mediaRate = 0;
		ent->mediaTime = MediaTime;
		break;
	default:
		ent->mediaRate = 0x10000;
		ent->mediaTime = MediaTime;
		break;
	}
	gf_list_add(trak->editBox->editList->entryList, ent);
	return SetTrackDuration(trak);
}

GF_EXPORT
GF_Err gf_isom_modify_edit(GF_ISOFile *movie, u32 trackNumber, u32 seg_index, u64 EditDuration, u64 MediaTime, GF_ISOEditType EditMode)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_EdtsEntry *ent;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !seg_index) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!trak->editBox || !trak->editBox->editList) return GF_OK;
	if (gf_list_count(trak->editBox->editList->entryList)<seg_index) return GF_BAD_PARAM;
	ent = (GF_EdtsEntry*) gf_list_get(trak->editBox->editList->entryList, seg_index-1);

	ent->segmentDuration = EditDuration;
	switch (EditMode) {
	case GF_ISOM_EDIT_EMPTY:
		ent->mediaRate = 0x10000;
		ent->mediaTime = -1;
		break;
	case GF_ISOM_EDIT_DWELL:
		ent->mediaRate = 0;
		ent->mediaTime = MediaTime;
		break;
	default:
		ent->mediaRate = 0x10000;
		ent->mediaTime = MediaTime;
		break;
	}
	return SetTrackDuration(trak);
}

//removes the desired track
GF_EXPORT
GF_Err gf_isom_remove_track(GF_ISOFile *movie, u32 trackNumber)
{
	GF_Err e;
	GF_TrackBox *the_trak, *trak;
	GF_TrackReferenceTypeBox *tref;
	u32 i, j, k, descIndex;
	GF_ISOTrackID *newRefs;
	u8 found;
	GF_ISOSample *samp;
	the_trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!the_trak) return GF_BAD_PARAM;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (movie->moov->iods && movie->moov->iods->descriptor) {
		GF_Descriptor *desc;
		GF_ES_ID_Inc *inc;
		GF_List *ESDs;
		desc = movie->moov->iods->descriptor;
		if (desc->tag == GF_ODF_ISOM_IOD_TAG) {
			ESDs = ((GF_IsomInitialObjectDescriptor *)desc)->ES_ID_IncDescriptors;
		} else if (desc->tag == GF_ODF_ISOM_OD_TAG) {
			ESDs = ((GF_IsomObjectDescriptor *)desc)->ES_ID_IncDescriptors;
		} else {
			return GF_ISOM_INVALID_FILE;
		}

		//remove the track ref from the root OD if any
		i=0;
		while ((inc = (GF_ES_ID_Inc *)gf_list_enum(ESDs, &i))) {
			if (inc->trackID == the_trak->Header->trackID) {
				gf_odf_desc_del((GF_Descriptor *)inc);
				i--;
				gf_list_rem(ESDs, i);
			}
		}
	}

	//remove the track from the movie
	gf_list_del_item(movie->moov->trackList, the_trak);

	//rewrite any OD tracks
	i=0;
	while ((trak = (GF_TrackBox *)gf_list_enum(movie->moov->trackList, &i))) {
		if (trak->Media->handler->handlerType != GF_ISOM_MEDIA_OD) continue;
		//this is an OD track...
		j = gf_isom_get_sample_count(movie, i);
		for (k=0; k < j; k++) {
			//getting the sample will remove the references to the deleted track in the output OD frame
			samp = gf_isom_get_sample(movie, i, k+1, &descIndex);
			if (!samp) break;
			//so let's update with the new OD frame ! If the sample is empty, remove it
			if (!samp->dataLength) {
				e = gf_isom_remove_sample(movie, i, k+1);
				if (e) return e;
			} else {
				e = gf_isom_update_sample(movie, i, k+1, samp, GF_TRUE);
				if (e) return e;
			}
			//and don't forget to delete the sample
			gf_isom_sample_del(&samp);
		}
	}

	//remove the track ref from any "tref" box in all tracks, except the one to delete
	//note that we don't touch scal references, as we don't want to rewrite AVC/HEVC samples ...
	i=0;
	while ((trak = (GF_TrackBox *)gf_list_enum(movie->moov->trackList, &i))) {
		if (trak == the_trak) continue;
		if (! trak->References || ! gf_list_count(trak->References->child_boxes)) continue;

		j=0;
		while ((tref = (GF_TrackReferenceTypeBox *)gf_list_enum(trak->References->child_boxes, &j))) {
			if (tref->reference_type==GF_ISOM_REF_SCAL)
				continue;

			found = 0;
			for (k=0; k<tref->trackIDCount; k++) {
				if (tref->trackIDs[k] == the_trak->Header->trackID) found++;
			}
			if (!found) continue;
			//no more refs, remove this ref_type
			if (found == tref->trackIDCount) {
				gf_isom_box_del_parent(&trak->References->child_boxes, (GF_Box *)tref);
				j--;
			} else {
				newRefs = (GF_ISOTrackID*)gf_malloc(sizeof(GF_ISOTrackID) * (tref->trackIDCount - found));
				if (!newRefs) return GF_OUT_OF_MEM;
				found = 0;
				for (k = 0; k < tref->trackIDCount; k++) {
					if (tref->trackIDs[k] != the_trak->Header->trackID) {
						newRefs[k-found] = tref->trackIDs[k];
					} else {
						found++;
					}
				}
				gf_free(tref->trackIDs);
				tref->trackIDs = newRefs;
				tref->trackIDCount -= found;
			}
		}
		//a little opt: remove the ref box if empty...
		if (! gf_list_count(trak->References->child_boxes)) {
			gf_isom_box_del_parent(&trak->child_boxes, (GF_Box *)trak->References);
			trak->References = NULL;
		}
	}

	gf_isom_disable_inplace_rewrite(movie);

	//delete the track
	gf_isom_box_del_parent(&movie->moov->child_boxes, (GF_Box *)the_trak);

	/*update next track ID*/
	movie->moov->mvhd->nextTrackID = 0;
	i=0;
	while ((trak = (GF_TrackBox *)gf_list_enum(movie->moov->trackList, &i))) {
		if (trak->Header->trackID>movie->moov->mvhd->nextTrackID)
			movie->moov->mvhd->nextTrackID = trak->Header->trackID;
	}

	if (!gf_list_count(movie->moov->trackList)) {
		gf_list_del_item(movie->TopBoxes, movie->moov);
		gf_isom_box_del((GF_Box *)movie->moov);
		movie->moov = NULL;
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_set_copyright(GF_ISOFile *movie, const char *threeCharCode, char *notice)
{
	GF_Err e;
	GF_CopyrightBox *ptr;
	GF_UserDataMap *map;
	u32 count, i;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!notice || !threeCharCode) return GF_BAD_PARAM;

	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (!movie->moov->udta) {
		e = moov_on_child_box((GF_Box*)movie->moov, gf_isom_box_new_parent(&movie->moov->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
		if (e) return e;
	}
	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_CPRT, NULL);

	if (map) {
		//try to find one in our language...
		count = gf_list_count(map->boxes);
		for (i=0; i<count; i++) {
			ptr = (GF_CopyrightBox*)gf_list_get(map->boxes, i);
			if (!strcmp(threeCharCode, (const char *) ptr->packedLanguageCode)) {
				gf_free(ptr->notice);
				ptr->notice = (char*)gf_malloc(sizeof(char) * (strlen(notice) + 1));
				if (!ptr->notice) return GF_OUT_OF_MEM;
				strcpy(ptr->notice, notice);
				return GF_OK;
			}
		}
	}
	//nope, create one
	ptr = (GF_CopyrightBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_CPRT);
	if (!ptr) return GF_OUT_OF_MEM;

	memcpy(ptr->packedLanguageCode, threeCharCode, 4);
	ptr->notice = (char*)gf_malloc(sizeof(char) * (strlen(notice)+1));
	if (!ptr->notice) return GF_OUT_OF_MEM;
	strcpy(ptr->notice, notice);
	return udta_on_child_box((GF_Box *)movie->moov->udta, (GF_Box *) ptr, GF_FALSE);
}

GF_EXPORT
GF_Err gf_isom_add_track_kind(GF_ISOFile *movie, u32 trackNumber, const char *schemeURI, const char *value)
{
	GF_Err e;
	GF_KindBox *ptr;
	GF_UserDataBox *udta;
	GF_UserDataMap *map;
	u32 i, count;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) {
			e = trak_on_child_box((GF_Box*)trak, gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
			if (e) return e;
		}
		udta = trak->udta;
	} else {
		return GF_BAD_PARAM;
	}

	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_KIND, NULL);
	if (map) {
		count = gf_list_count(map->boxes);
		for (i=0; i<count; i++) {
			GF_Box *b = (GF_Box *)gf_list_get(map->boxes, i);
			if (b->type == GF_ISOM_BOX_TYPE_KIND) {
				GF_KindBox *kb = (GF_KindBox *)b;
				if (!strcmp(kb->schemeURI, schemeURI) &&
				        ((value && kb->value && !strcmp(value, kb->value)) || (!value && !kb->value))) {
					// Already there
					return GF_OK;
				}
			}
		}
	}

	ptr = (GF_KindBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_KIND);
	if (e) return e;

	ptr->schemeURI = gf_strdup(schemeURI);
	if (value) ptr->value = gf_strdup(value);
	return udta_on_child_box((GF_Box *)udta, (GF_Box *) ptr, GF_FALSE);
}

GF_EXPORT
GF_Err gf_isom_remove_track_kind(GF_ISOFile *movie, u32 trackNumber, const char *schemeURI, const char *value)
{
	GF_Err e;
	GF_UserDataBox *udta;
	GF_UserDataMap *map;
	u32 i;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) {
			e = trak_on_child_box((GF_Box*)trak, gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
			if (e) return e;
		}
		udta = trak->udta;
	} else {
		return GF_OK;
	}
	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_KIND, NULL);
	if (map) {
		for (i=0; i<gf_list_count(map->boxes); i++) {
			GF_Box *b = (GF_Box *)gf_list_get(map->boxes, i);
			if (b->type == GF_ISOM_BOX_TYPE_KIND) {
				GF_KindBox *kb = (GF_KindBox *)b;
				if (!schemeURI ||
				        (!strcmp(kb->schemeURI, schemeURI) &&
				         ((value && kb->value && !strcmp(value, kb->value)) || (!value && !kb->value)))) {
					gf_isom_box_del_parent(&map->boxes, b);
					i--;
				}
			}
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_add_chapter(GF_ISOFile *movie, u32 trackNumber, u64 timestamp, char *name)
{
	GF_Err e;
	GF_ChapterListBox *ptr;
	u32 i, count;
	GF_ChapterEntry *ce;
	GF_UserDataBox *udta;
	GF_UserDataMap *map;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) {
			e = trak_on_child_box((GF_Box*)trak, gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
			if (e) return e;
		}
		udta = trak->udta;
	} else {
		if (!movie->moov->udta) {
			e = moov_on_child_box((GF_Box*)movie->moov, gf_isom_box_new_parent(&movie->moov->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
			if (e) return e;
		}
		udta = movie->moov->udta;
	}

	ptr = NULL;
	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_CHPL, NULL);
	if (!map) {
		ptr = (GF_ChapterListBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_CHPL);
		e = udta_on_child_box((GF_Box *)udta, (GF_Box *) ptr, GF_FALSE);
		if (e) return e;
		map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_CHPL, NULL);
	} else {
		ptr = (GF_ChapterListBox*)gf_list_get(map->boxes, 0);
	}
	if (!map) return GF_OUT_OF_MEM;

	/*this may happen if original MP4 is not properly formatted*/
	if (!ptr) {
		ptr = (GF_ChapterListBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_CHPL);
		if (!ptr) return GF_OUT_OF_MEM;
		gf_list_add(map->boxes, ptr);
	}

	GF_SAFEALLOC(ce, GF_ChapterEntry);
	if (!ce) return GF_OUT_OF_MEM;

	ce->start_time = timestamp * 10000L;
	ce->name = name ? gf_strdup(name) : NULL;

	/*insert in order*/
	count = gf_list_count(ptr->list);
	for (i=0; i<count; i++) {
		GF_ChapterEntry *ace = (GF_ChapterEntry *)gf_list_get(ptr->list, i);
		if (ace->start_time == ce->start_time) {
			if (ace->name) gf_free(ace->name);
			ace->name = ce->name;
			gf_free(ce);
			return GF_OK;
		}
		if (ace->start_time >= ce->start_time)
			return gf_list_insert(ptr->list, ce, i);
	}
	return gf_list_add(ptr->list, ce);
}

GF_EXPORT
GF_Err gf_isom_remove_chapter(GF_ISOFile *movie, u32 trackNumber, u32 index)
{
	GF_Err e;
	GF_ChapterListBox *ptr;
	GF_ChapterEntry *ce;
	GF_UserDataBox *udta;
	GF_UserDataMap *map;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) return GF_OK;
		udta = trak->udta;
	} else {
		if (!movie->moov->udta) return GF_OK;
		udta = movie->moov->udta;
	}

	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_CHPL, NULL);
	if (!map) return GF_OK;
	ptr = (GF_ChapterListBox*)gf_list_get(map->boxes, 0);
	if (!ptr) return GF_OK;

	if (index) {
		ce = (GF_ChapterEntry *)gf_list_get(ptr->list, index-1);
		if (!ce) return GF_BAD_PARAM;
		if (ce->name) gf_free(ce->name);
		gf_free(ce);
		gf_list_rem(ptr->list, index-1);
	} else {
		while (gf_list_count(ptr->list)) {
			ce = (GF_ChapterEntry *)gf_list_get(ptr->list, 0);
			if (ce->name) gf_free(ce->name);
			gf_free(ce);
			gf_list_rem(ptr->list, 0);
		}
	}
	if (!gf_list_count(ptr->list)) {
		gf_list_del_item(udta->recordList, map);
		gf_isom_box_array_del(map->boxes);
		gf_free(map);
	}
	return GF_OK;
}

#if 0 //unused
GF_Err gf_isom_remove_copyright(GF_ISOFile *movie, u32 index)
{
	GF_Err e;
	GF_CopyrightBox *ptr;
	GF_UserDataMap *map;
	u32 count;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	e = gf_isom_insert_moov(movie);
	if (e) return e;

	if (!index) return GF_BAD_PARAM;
	if (!movie->moov->udta) return GF_OK;

	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_CPRT, NULL);
	if (!map) return GF_OK;

	count = gf_list_count(map->boxes);
	if (index>count) return GF_BAD_PARAM;

	ptr = (GF_CopyrightBox*)gf_list_get(map->boxes, index-1);
	if (ptr) {
		gf_list_rem(map->boxes, index-1);
		if (ptr->notice) gf_free(ptr->notice);
		gf_free(ptr);
	}
	/*last copyright, remove*/
	if (!gf_list_count(map->boxes)) {
		gf_list_del_item(movie->moov->udta->recordList, map);
		gf_list_del(map->boxes);
		gf_free(map);
	}
	return GF_OK;
}

GF_Err gf_isom_set_watermark(GF_ISOFile *movie, bin128 UUID, u8* data, u32 length)
{
	GF_Err e;
	GF_UnknownUUIDBox *ptr;
	GF_UserDataMap *map;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	e = gf_isom_insert_moov(movie);
	if (e) return e;
	if (!movie->moov->udta) {
		e = moov_on_child_box((GF_Box*)movie->moov, gf_isom_box_new_parent(&movie->moov->child_boxes, GF_ISOM_BOX_TYPE_UDTA));
		if (e) return e;
	}

	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_UUID, (bin128 *) & UUID);
	if (map) {
		ptr = (GF_UnknownUUIDBox *)gf_list_get(map->boxes, 0);
		if (ptr) {
			gf_free(ptr->data);
			ptr->data = (char*)gf_malloc(length);
			if (!ptr->data) return GF_OUT_OF_MEM;
			memcpy(ptr->data, data, length);
			ptr->dataSize = length;
			return GF_OK;
		}
	}
	//nope, create one
	ptr = (GF_UnknownUUIDBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_UUID);
	if (!ptr) return GF_OUT_OF_MEM;

	memcpy(ptr->uuid, UUID, 16);
	ptr->data = (char*)gf_malloc(length);
	if (!ptr->data) return GF_OUT_OF_MEM;
	memcpy(ptr->data, data, length);
	ptr->dataSize = length;
	return udta_on_child_box((GF_Box *)movie->moov->udta, (GF_Box *) ptr);
}
#endif

//set the interleaving time of media data (INTERLEAVED mode only)
//InterleaveTime is in MovieTimeScale
GF_EXPORT
GF_Err gf_isom_set_interleave_time(GF_ISOFile *movie, u32 InterleaveTime)
{
	GF_Err e;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!InterleaveTime || !movie->moov) return GF_OK;
	movie->interleavingTime = InterleaveTime;
	return GF_OK;
}



//use a compact track version for sample size. This is not usually recommended
//except for speech codecs where the track has a lot of small samples
//compaction is done automatically while writing based on the track's sample sizes
GF_EXPORT
GF_Err gf_isom_use_compact_size(GF_ISOFile *movie, u32 trackNumber, Bool CompactionOn)
{
	GF_TrackBox *trak;
	u32 i, size;
	GF_SampleSizeBox *stsz;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (!trak->Media || !trak->Media->information
	        || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->SampleSize)
		return GF_ISOM_INVALID_FILE;

	stsz = trak->Media->information->sampleTable->SampleSize;

	//switch to regular table
	if (!CompactionOn) {
		if (stsz->type == GF_ISOM_BOX_TYPE_STSZ) return GF_OK;
		stsz->type = GF_ISOM_BOX_TYPE_STSZ;
		//invalidate the sampleSize and recompute it
		stsz->sampleSize = 0;
		if (!stsz->sampleCount) return GF_OK;
		//if the table is empty we can only assume the track is empty (no size indication)
		if (!stsz->sizes) return GF_OK;
		size = stsz->sizes[0];
		//check whether the sizes are all the same or not
		for (i=1; i<stsz->sampleCount; i++) {
			if (size != stsz->sizes[i]) {
				size = 0;
				break;
			}
		}
		if (size) {
			gf_free(stsz->sizes);
			stsz->sizes = NULL;
			stsz->sampleSize = size;
		}
		return GF_OK;
	}

	//switch to compact table
	if (stsz->type == GF_ISOM_BOX_TYPE_STZ2) return GF_OK;
	//fill the table. Although it seems weird , this is needed in case of edition
	//after the function is called. NOte however than we force regular table
	//at write time if all samples are of same size
	if (stsz->sampleSize) {
		//this is a weird table indeed ;)
		if (stsz->sizes) gf_free(stsz->sizes);
		stsz->sizes = (u32*) gf_malloc(sizeof(u32)*stsz->sampleCount);
		if (!stsz->sizes) return GF_OUT_OF_MEM;
		memset(stsz->sizes, stsz->sampleSize, sizeof(u32));
	}
	//set the SampleSize to 0 while the file is open
	stsz->sampleSize = 0;
	stsz->type = GF_ISOM_BOX_TYPE_STZ2;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_set_brand_info(GF_ISOFile *movie, u32 MajorBrand, u32 MinorVersion)
{
	u32 i, *p;

	if (!MajorBrand) return GF_BAD_PARAM;

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (! (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY)) {
		GF_Err e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
		if (e) return e;

		e = CheckNoData(movie);
		if (e) return e;
	}
#endif

	if (!movie->brand) {
		movie->brand = (GF_FileTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_FTYP);
		if (!movie->brand) return GF_OUT_OF_MEM;
		gf_list_add(movie->TopBoxes, movie->brand);
	}

	movie->brand->majorBrand = MajorBrand;
	movie->brand->minorVersion = MinorVersion;

	if (!movie->brand->altBrand) {
		movie->brand->altBrand = (u32*)gf_malloc(sizeof(u32));
		if (!movie->brand->altBrand) return GF_OUT_OF_MEM;
		movie->brand->altBrand[0] = MajorBrand;
		movie->brand->altCount = 1;
		return GF_OK;
	}

	//if brand already present don't change anything
	for (i=0; i<movie->brand->altCount; i++) {
		if (movie->brand->altBrand[i] == MajorBrand) return GF_OK;
	}
	p = (u32*)gf_malloc(sizeof(u32)*(movie->brand->altCount + 1));
	if (!p) return GF_OUT_OF_MEM;
	memcpy(p, movie->brand->altBrand, sizeof(u32)*movie->brand->altCount);
	p[movie->brand->altCount] = MajorBrand;
	movie->brand->altCount += 1;
	gf_free(movie->brand->altBrand);
	movie->brand->altBrand = p;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_modify_alternate_brand(GF_ISOFile *movie, u32 Brand, Bool AddIt)
{
	u32 i, k, *p;

	if (!Brand) return GF_BAD_PARAM;

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (! (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY)) {
		GF_Err e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
		if (e) return e;

		e = CheckNoData(movie);
		if (e) return e;
	}
#endif

	if (!movie->brand && AddIt) {
		movie->brand = (GF_FileTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_FTYP);
		if (!movie->brand) return GF_OUT_OF_MEM;
		gf_list_add(movie->TopBoxes, movie->brand);
	}
	if (!AddIt && !movie->brand) return GF_OK;

	//do not mofify major one
	if (!AddIt && movie->brand->majorBrand == Brand) return GF_OK;

	if (!AddIt && movie->brand->altCount == 1) {
		//fixes it in case
		movie->brand->altBrand[0] = movie->brand->majorBrand;
		return GF_OK;
	}
	//check for the brand
	for (i=0; i<movie->brand->altCount; i++) {
		if (movie->brand->altBrand[i] == Brand) goto found;
	}
	//Not found
	if (!AddIt) return GF_OK;
	//add it
	p = (u32*)gf_malloc(sizeof(u32)*(movie->brand->altCount + 1));
	if (!p) return GF_OUT_OF_MEM;
	if (movie->brand->altBrand) {
		memcpy(p, movie->brand->altBrand, sizeof(u32)*movie->brand->altCount);
		gf_free(movie->brand->altBrand);
	}
	p[movie->brand->altCount] = Brand;
	movie->brand->altCount += 1;
	movie->brand->altBrand = p;
	return GF_OK;

found:

	//found
	if (AddIt) return GF_OK;
	assert(movie->brand->altCount>1);

	//remove it
	p = (u32*)gf_malloc(sizeof(u32)*(movie->brand->altCount - 1));
	if (!p) return GF_OUT_OF_MEM;
	k = 0;
	for (i=0; i<movie->brand->altCount; i++) {
		if (movie->brand->altBrand[i] == Brand) continue;
		else {
			p[k] = movie->brand->altBrand[i];
			k++;
		}
	}
	movie->brand->altCount -= 1;
	gf_free(movie->brand->altBrand);
	movie->brand->altBrand = p;
	return GF_OK;
}


GF_Err gf_isom_reset_alt_brands_ex(GF_ISOFile *movie, Bool leave_empty)
{
	u32 *p;

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (! (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY)) {
		GF_Err e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
		if (e) return e;

		e = CheckNoData(movie);
		if (e) return e;
	}
#endif

	if (!movie->brand) {
		movie->brand = (GF_FileTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_FTYP);
		if (!movie->brand) return GF_OUT_OF_MEM;
		gf_list_add(movie->TopBoxes, movie->brand);
	}
	gf_free(movie->brand->altBrand);
	if (leave_empty) {
		movie->brand->altCount = 0;
		movie->brand->altBrand = NULL;
	} else {
		p = (u32*)gf_malloc(sizeof(u32));
		if (!p) return GF_OUT_OF_MEM;
		p[0] = movie->brand->majorBrand;
		movie->brand->altCount = 1;
		movie->brand->altBrand = p;
	}
	return GF_OK;
}
GF_EXPORT
GF_Err gf_isom_reset_alt_brands(GF_ISOFile *movie)
{
	return gf_isom_reset_alt_brands_ex(movie, GF_FALSE);
}

#if 0 //unused
GF_Err gf_isom_set_sample_padding_bits(GF_ISOFile *movie, u32 trackNumber, u32 sampleNumber, u8 NbBits)
{
	GF_TrackBox *trak;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || NbBits > 7) return GF_BAD_PARAM;

	//set Padding info
	return stbl_SetPaddingBits(trak->Media->information->sampleTable, sampleNumber, NbBits);
}
#endif

GF_EXPORT
GF_Err gf_isom_remove_user_data_item(GF_ISOFile *movie, u32 trackNumber, u32 UserDataType, bin128 UUID, u32 UserDataIndex)
{
	GF_UserDataMap *map;
	GF_Box *a;
	u32 i;
	bin128 t;
	GF_Err e;
	GF_TrackBox *trak;
	GF_UserDataBox *udta;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (UserDataType == GF_ISOM_BOX_TYPE_UUID) UserDataType = 0;
	memset(t, 1, 16);

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		udta = trak->udta;
	} else {
		udta = movie->moov->udta;
	}
	if (!udta) return GF_BAD_PARAM;
	if (!UserDataIndex) return GF_BAD_PARAM;

	i=0;
	while ((map = (GF_UserDataMap*)gf_list_enum(udta->recordList, &i))) {
		if ((map->boxType == GF_ISOM_BOX_TYPE_UUID)  && !memcmp(map->uuid, UUID, 16)) goto found;
		else if (map->boxType == UserDataType) goto found;
	}
	//not found
	return GF_OK;

found:

	if (UserDataIndex > gf_list_count(map->boxes) ) return GF_BAD_PARAM;
	//delete the box
	a = (GF_Box*)gf_list_get(map->boxes, UserDataIndex-1);
	gf_isom_box_del_parent(&map->boxes, a);

	//remove the map if empty
	if (!gf_list_count(map->boxes)) {
		gf_list_rem(udta->recordList, i-1);
		gf_isom_box_array_del(map->boxes);
		gf_free(map);
	}
	//but we keep the UDTA no matter what
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_remove_user_data(GF_ISOFile *movie, u32 trackNumber, u32 UserDataType, bin128 UUID)
{
	GF_UserDataMap *map;
	u32 i;
	GF_Err e;
	bin128 t;
	GF_TrackBox *trak;
	GF_UserDataBox *udta;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (UserDataType == GF_ISOM_BOX_TYPE_UUID) UserDataType = 0;
	memset(t, 1, 16);

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_EOS;
		udta = trak->udta;
	} else {
		udta = movie->moov->udta;
	}
	//do not return any error if no udta
	if (!udta) return GF_EOS;

	i=0;
	while ((map = (GF_UserDataMap*)gf_list_enum(udta->recordList, &i))) {
		if ((map->boxType == GF_ISOM_BOX_TYPE_UUID)  && !memcmp(map->uuid, UUID, 16)) goto found;
		else if (map->boxType == UserDataType) goto found;
	}
	//not found
	return GF_OK;

found:

	gf_list_rem(udta->recordList, i-1);
	gf_isom_box_array_del(map->boxes);
	gf_free(map);

	//but we keep the UDTA no matter what
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_add_user_data(GF_ISOFile *movie, u32 trackNumber, u32 UserDataType, bin128 UUID, u8 *data, u32 DataLength)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_UserDataBox *udta;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (UserDataType == GF_ISOM_BOX_TYPE_UUID) UserDataType = 0;

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) trak_on_child_box((GF_Box*)trak, gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
		udta = trak->udta;
	} else {
		if (!movie->moov->udta) moov_on_child_box((GF_Box*)movie->moov, gf_isom_box_new_parent(&movie->moov->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
		udta = movie->moov->udta;
	}
	if (!udta) return GF_OUT_OF_MEM;

	//create a default box
	if (UserDataType) {
		GF_UnknownBox *a = (GF_UnknownBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_UNKNOWN);
		if (!a) return GF_OUT_OF_MEM;
		a->original_4cc = UserDataType;
		if (DataLength) {
			a->data = (char*)gf_malloc(sizeof(char)*DataLength);
			if (!a->data) return GF_OUT_OF_MEM;
			memcpy(a->data, data, DataLength);
			a->dataSize = DataLength;
		}
		return udta_on_child_box((GF_Box *)udta, (GF_Box *) a, GF_FALSE);
	} else {
		GF_UnknownUUIDBox *a = (GF_UnknownUUIDBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_UUID);
		if (!a) return GF_OUT_OF_MEM;
		memcpy(a->uuid, UUID, 16);
		if (DataLength) {
			a->data = (char*)gf_malloc(sizeof(char)*DataLength);
			if (!a->data) return GF_OUT_OF_MEM;
			memcpy(a->data, data, DataLength);
			a->dataSize = DataLength;
		}
		return udta_on_child_box((GF_Box *)udta, (GF_Box *) a, GF_FALSE);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_add_user_data_boxes(GF_ISOFile *movie, u32 trackNumber, u8 *data, u32 DataLength)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_UserDataBox *udta;
	GF_BitStream *bs;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) trak_on_child_box((GF_Box*)trak, gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
		udta = trak->udta;
	} else {
		if (!movie->moov) return GF_BAD_PARAM;
		if (!movie->moov->udta) moov_on_child_box((GF_Box*)movie->moov, gf_isom_box_new_parent(&movie->moov->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
		udta = movie->moov->udta;
	}
	if (!udta) return GF_OUT_OF_MEM;

	bs = gf_bs_new(data, DataLength, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		GF_Box *a;
		e = gf_isom_box_parse(&a, bs);
		if (e) break;
		e = udta_on_child_box((GF_Box *)udta, a, GF_FALSE);
		if (e) break;
	}
	gf_bs_del(bs);
	return e;
}

GF_EXPORT
GF_Err gf_isom_clone_pl_indications(GF_ISOFile *orig, GF_ISOFile *dest)
{
	GF_IsomInitialObjectDescriptor *iod_d;
	if (!orig || !dest) return GF_BAD_PARAM;
	if (!orig->moov->iods || !orig->moov->iods->descriptor) return GF_OK;
	if (orig->moov->iods->descriptor->tag != GF_ODF_ISOM_IOD_TAG) return GF_OK;

	AddMovieIOD(dest->moov, 1);
	gf_odf_desc_del((GF_Descriptor *)dest->moov->iods->descriptor);
	gf_odf_desc_copy((GF_Descriptor *)orig->moov->iods->descriptor, (GF_Descriptor **)&dest->moov->iods->descriptor);
	iod_d = (GF_IsomInitialObjectDescriptor *) dest->moov->iods->descriptor;
	while (gf_list_count(iod_d->ES_ID_IncDescriptors)) {
		GF_Descriptor *d = (GF_Descriptor *)gf_list_get(iod_d->ES_ID_IncDescriptors, 0);
		gf_list_rem(iod_d->ES_ID_IncDescriptors, 0);
		gf_odf_desc_del(d);
	}
	while (gf_list_count(iod_d->ES_ID_RefDescriptors)) {
		GF_Descriptor *d = (GF_Descriptor *)gf_list_get(iod_d->ES_ID_RefDescriptors, 0);
		gf_list_rem(iod_d->ES_ID_RefDescriptors, 0);
		gf_odf_desc_del(d);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_clone_box(GF_Box *src, GF_Box **dst)
{
	GF_Err e;
	u8 *data;
	u32 data_size;
	GF_BitStream *bs;

	if (*dst) {
		gf_isom_box_del(*dst);
		*dst=NULL;
	}
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (!bs) return GF_OUT_OF_MEM;
	e = gf_isom_box_size( (GF_Box *) src);
	if (!e) e = gf_isom_box_write((GF_Box *) src, bs);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	if (e) {
		if (data) gf_free(data);
		return e;
	}
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	if (!bs) {
		if (data) gf_free(data);
		return GF_OUT_OF_MEM;
	}
	e = gf_isom_box_parse(dst, bs);
	gf_bs_del(bs);
	gf_free(data);
	return e;
}

#if 0 //unused
/*clones the entire movie file to destination. Tracks can be cloned if clone_tracks is set, in which case hint tracks can be
kept if keep_hint_tracks is set
if keep_pssh, all pssh boxes will be kept
fragment information (mvex) is not kept*/
GF_Err gf_isom_clone_movie(GF_ISOFile *orig_file, GF_ISOFile *dest_file, Bool clone_tracks, Bool keep_hint_tracks, Bool keep_pssh)
{
	GF_Err e;
	u32 i;
	GF_Box *box;

	e = CanAccessMovie(dest_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (orig_file->brand) {
		gf_list_del_item(dest_file->TopBoxes, dest_file->brand);
		gf_isom_box_del((GF_Box *)dest_file->brand);
		dest_file->brand = NULL;
		gf_isom_clone_box((GF_Box *)orig_file->brand, (GF_Box **)&dest_file->brand);
		if (dest_file->brand) gf_list_add(dest_file->TopBoxes, dest_file->brand);
	}

	if (orig_file->meta) {
		gf_list_del_item(dest_file->TopBoxes, dest_file->meta);
		gf_isom_box_del((GF_Box *)dest_file->meta);
		dest_file->meta = NULL;
		/*fixme - check imports*/
		gf_isom_clone_box((GF_Box *)orig_file->meta, (GF_Box **)&dest_file->meta);
		if (dest_file->meta) gf_list_add(dest_file->TopBoxes, dest_file->meta);
	}
	if (orig_file->moov) {
		u32 i, dstTrack;
		GF_Box *iods;
		GF_List *tracks = gf_list_new();
		GF_List *old_tracks = orig_file->moov->trackList;
		orig_file->moov->trackList = tracks;
		iods = (GF_Box*)orig_file->moov->iods;
		orig_file->moov->iods = NULL;
		e = gf_isom_clone_box((GF_Box *)orig_file->moov, (GF_Box **)&dest_file->moov);
		if (e) {
			gf_list_del(tracks);
			orig_file->moov->trackList = old_tracks;
			return e;
		}
		orig_file->moov->trackList = old_tracks;
		gf_list_del(tracks);
		orig_file->moov->iods = (GF_ObjectDescriptorBox*)iods;
		gf_list_add(dest_file->TopBoxes, dest_file->moov);

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		if (dest_file->moov->mvex) {
			gf_isom_box_del_parent(&dest_file->moov->child_boxes, (GF_Box *)dest_file->moov->mvex);
			dest_file->moov->mvex = NULL;
		}
#endif

		if (clone_tracks) {
			for (i=0; i<gf_list_count(orig_file->moov->trackList); i++) {
				GF_TrackBox *trak = (GF_TrackBox*)gf_list_get( orig_file->moov->trackList, i);
				if (!trak) continue;
				if (keep_hint_tracks || (trak->Media->handler->handlerType != GF_ISOM_MEDIA_HINT)) {
					e = gf_isom_clone_track(orig_file, i+1, dest_file, 0, &dstTrack);
					if (e) return e;
				}
			}
			if (iods)
				gf_isom_clone_box((GF_Box *)orig_file->moov->iods, (GF_Box **)dest_file->moov->iods);
		} else {
			dest_file->moov->mvhd->nextTrackID = 1;
			gf_isom_clone_pl_indications(orig_file, dest_file);
		}
		dest_file->moov->mov = dest_file;
	}

	if (!keep_pssh) {
		i=0;
		while ((box = (GF_Box*)gf_list_get(dest_file->moov->child_boxes, i++))) {
			if (box->type == GF_ISOM_BOX_TYPE_PSSH) {
				i--;
				gf_isom_box_del_parent(&dest_file->moov->child_boxes, box);
			}
		}
	}

	//duplicate other boxes
	i=0;
	while ((box = (GF_Box*)gf_list_get(orig_file->TopBoxes, i++))) {
		switch(box->type) {
		case GF_ISOM_BOX_TYPE_MOOV:
		case GF_ISOM_BOX_TYPE_META:
		case GF_ISOM_BOX_TYPE_MDAT:
		case GF_ISOM_BOX_TYPE_FTYP:
		case GF_ISOM_BOX_TYPE_PDIN:
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		case GF_ISOM_BOX_TYPE_STYP:
		case GF_ISOM_BOX_TYPE_SIDX:
		case GF_ISOM_BOX_TYPE_SSIX:
		case GF_ISOM_BOX_TYPE_MOOF:
#endif
		case GF_ISOM_BOX_TYPE_JP:
			break;

		case GF_ISOM_BOX_TYPE_PSSH:
			if (!keep_pssh)
				break;

		default:
		{
			GF_Box *box2 = NULL;
			gf_isom_clone_box(box, &box2);
			gf_list_add(dest_file->TopBoxes, box2);
		}
		break;
		}
	}

	return GF_OK;
}
#endif


GF_EXPORT
GF_Err gf_isom_get_raw_user_data(GF_ISOFile *file, u8 **output, u32 *output_size)
{
	GF_BitStream *bs;
	GF_Err e;
	GF_Box *b;
	u32 i;

	*output = NULL;
	*output_size = 0;
	if (!file || !file->moov || (!file->moov->udta && !file->moov->child_boxes)) return GF_OK;
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	if (file->moov->udta) {
		e = gf_isom_box_size( (GF_Box *) file->moov->udta);
		if (e) goto exit;
		e = gf_isom_box_write((GF_Box *) file->moov->udta, bs);
		if (e) goto exit;
	}
	e = GF_OK;
	i=0;
	while ((b = gf_list_enum(file->moov->child_boxes, &i))) {
		switch (b->type) {
		case GF_ISOM_BOX_TYPE_TRAK:
		case GF_ISOM_BOX_TYPE_MVHD:
		case GF_ISOM_BOX_TYPE_MVEX:
		case GF_ISOM_BOX_TYPE_IODS:
		case GF_ISOM_BOX_TYPE_META:
			continue;
		}
		e = gf_isom_box_size( (GF_Box *) b);
		if (e) goto exit;
		e = gf_isom_box_write((GF_Box *) b, bs);
		if (e) goto exit;
	}

	gf_bs_get_content(bs, output, output_size);

exit:
	gf_bs_del(bs);
	return e;
}

GF_EXPORT
GF_Err gf_isom_get_track_template(GF_ISOFile *file, u32 track, u8 **output, u32 *output_size)
{
	GF_TrackBox *trak;
	GF_BitStream *bs;
	GF_DataReferenceBox *dref;
	GF_SampleTableBox *stbl, *stbl_temp;
	GF_SampleEncryptionBox *senc;
	u32 i, count;

	*output = NULL;
	*output_size = 0;
	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(file, track);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	//don't serialize dref
	dref = NULL;
	if (trak->Media->information->dataInformation) {
		dref = trak->Media->information->dataInformation->dref;
		trak->Media->information->dataInformation->dref = NULL;
		gf_list_del_item(trak->Media->information->dataInformation->child_boxes, dref);
	}

	//don't serialize stbl but create a temp one
	stbl_temp = (GF_SampleTableBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STBL);
	if (!stbl_temp->child_boxes) stbl_temp->child_boxes = gf_list_new();
	stbl = trak->Media->information->sampleTable;
	gf_list_del_item(trak->Media->information->child_boxes, stbl);

	trak->Media->information->sampleTable = stbl_temp;
	gf_list_add(trak->Media->information->child_boxes, stbl_temp);

	/*do not clone sampleDescription table but create an empty one*/
	stbl_temp->SampleDescription = (GF_SampleDescriptionBox *) gf_isom_box_new_parent(&stbl_temp->child_boxes, GF_ISOM_BOX_TYPE_STSD);

	/*clone sampleGroups description tables if any*/
	stbl_temp->sampleGroupsDescription = stbl->sampleGroupsDescription;
	count = gf_list_count(stbl->sampleGroupsDescription);
	for (i=0;i<count; i++) {
		GF_Box *b = gf_list_get(stbl->sampleGroupsDescription, i);
		gf_list_add(stbl_temp->child_boxes, b);
	}
	/*clone CompositionToDecode table, we may remove it later*/
	stbl_temp->CompositionToDecode = stbl->CompositionToDecode;
	gf_list_add(stbl_temp->child_boxes, stbl->CompositionToDecode);


	//don't serialize senc
	senc = trak->sample_encryption;
	if (senc) {
		assert(trak->child_boxes);
		gf_list_del_item(trak->child_boxes, senc);
		trak->sample_encryption = NULL;
	}

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_isom_box_size( (GF_Box *) trak);
	gf_isom_box_write((GF_Box *) trak, bs);
	gf_bs_get_content(bs, output, output_size);
	gf_bs_del(bs);

	//restore our pointers
	if (dref) {
		trak->Media->information->dataInformation->dref = dref;
		gf_list_add(trak->Media->information->dataInformation->child_boxes, dref);
	}
	trak->Media->information->sampleTable = stbl;
	gf_list_add(trak->Media->information->child_boxes, stbl);
	gf_list_del_item(trak->Media->information->child_boxes, stbl_temp);
	if (senc) {
		trak->sample_encryption = senc;
		gf_list_add(trak->child_boxes, senc);
	}

	stbl_temp->sampleGroupsDescription = NULL;
	count = gf_list_count(stbl->sampleGroupsDescription);
	for (i=0;i<count; i++) {
		GF_Box *b = gf_list_get(stbl->sampleGroupsDescription, i);
		gf_list_del_item(stbl_temp->child_boxes, b);
	}

	stbl_temp->CompositionToDecode = NULL;
	gf_list_del_item(stbl_temp->child_boxes, stbl->CompositionToDecode);
	gf_isom_box_del((GF_Box *)stbl_temp);
	return GF_OK;

}

GF_EXPORT
GF_Err gf_isom_get_trex_template(GF_ISOFile *file, u32 track, u8 **output, u32 *output_size)
{
	GF_TrackBox *trak;
	GF_BitStream *bs;
	u32 i;
	GF_TrackExtendsBox *trex = NULL;
	GF_TrackExtensionPropertiesBox *trexprop = NULL;

	*output = NULL;
	*output_size = 0;
	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(file, track);
	if (!trak || !trak->Media) return GF_BAD_PARAM;
	if (!file->moov->mvex) return GF_NOT_FOUND;
	for (i=0; i<gf_list_count(file->moov->mvex->TrackExList); i++) {
		trex = gf_list_get(file->moov->mvex->TrackExList, i);
		if (trex->trackID == trak->Header->trackID) break;
		trex = NULL;
	}
	if (!trex) return GF_NOT_FOUND;

	for (i=0; i<gf_list_count(file->moov->mvex->TrackExPropList); i++) {
		trexprop = gf_list_get(file->moov->mvex->TrackExPropList, i);
		if (trexprop->trackID== trak->Header->trackID) break;
		trexprop = NULL;
	}
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_isom_box_size( (GF_Box *) trex);
	gf_isom_box_write((GF_Box *) trex, bs);

	if (trexprop) {
		gf_isom_box_size( (GF_Box *) trexprop);
		gf_isom_box_write((GF_Box *) trexprop, bs);
	}
	gf_bs_get_content(bs, output, output_size);
	gf_bs_del(bs);

	return GF_OK;

}

GF_EXPORT
GF_Err gf_isom_get_stsd_template(GF_ISOFile *file, u32 track, u32 stsd_idx, u8 **output, u32 *output_size)
{
	GF_TrackBox *trak;
	GF_BitStream *bs;
	GF_Box *ent;

	*output = NULL;
	*output_size = 0;
	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(file, track);
	if (!trak || !stsd_idx || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->SampleDescription) return GF_BAD_PARAM;

	ent = gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, stsd_idx-1);
	if (!ent) return GF_BAD_PARAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_isom_box_size( (GF_Box *) ent);
	gf_isom_box_write((GF_Box *) ent, bs);
	gf_bs_get_content(bs, output, output_size);
	gf_bs_del(bs);
	return GF_OK;

}


GF_EXPORT
GF_Err gf_isom_clone_track(GF_ISOFile *orig_file, u32 orig_track, GF_ISOFile *dest_file, GF_ISOTrackCloneFlags flags, u32 *dest_track)
{
	GF_TrackBox *trak, *new_tk;
	GF_BitStream *bs;
	u8 *data;
	const u8 *buffer;
	u32 data_size;
	u32 i, count;
	GF_Err e;
	GF_SampleTableBox *stbl, *stbl_temp;
	GF_SampleEncryptionBox *senc;

	e = CanAccessMovie(dest_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	e = gf_isom_insert_moov(dest_file);
	if (e) return e;

	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(orig_file, orig_track);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	stbl_temp = (GF_SampleTableBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STBL);
	if (!stbl_temp->child_boxes) stbl_temp->child_boxes = gf_list_new();

	trak->Media->information->sampleTable = stbl_temp;
	gf_list_add(trak->Media->information->child_boxes, stbl_temp);
	gf_list_del_item(trak->Media->information->child_boxes, stbl);

	if (!stbl_temp->child_boxes) stbl_temp->child_boxes = gf_list_new();

	/*clone sampleDescription table*/
	stbl_temp->SampleDescription = stbl->SampleDescription;
	gf_list_add(stbl_temp->child_boxes, stbl->SampleDescription);
	/*also clone sampleGroups description tables if any*/
	stbl_temp->sampleGroupsDescription = stbl->sampleGroupsDescription;
	count = gf_list_count(stbl->sampleGroupsDescription);
	for (i=0; i<count; i++) {
		GF_Box *b = gf_list_get(stbl->sampleGroupsDescription, i);
		gf_list_add(stbl_temp->child_boxes, b);
	}
	/*clone CompositionToDecode table, we may remove it later*/
	stbl_temp->CompositionToDecode = stbl->CompositionToDecode;
	gf_list_add(stbl_temp->child_boxes, stbl->CompositionToDecode);

	senc = trak->sample_encryption;
	if (senc) {
		assert(trak->child_boxes);
		gf_list_del_item(trak->child_boxes, senc);
		trak->sample_encryption = NULL;
	}

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_isom_box_size( (GF_Box *) trak);
	gf_isom_box_write((GF_Box *) trak, bs);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	if (flags & GF_ISOM_CLONE_TRACK_NO_QT)
		gf_bs_set_cookie(bs, GF_ISOM_BS_COOKIE_QT_CONV | GF_ISOM_BS_COOKIE_CLONE_TRACK);
	else
		gf_bs_set_cookie(bs, GF_ISOM_BS_COOKIE_CLONE_TRACK);

	e = gf_isom_box_parse((GF_Box **) &new_tk, bs);
	gf_bs_del(bs);
	gf_free(data);

	trak->Media->information->sampleTable = stbl;
	gf_list_del_item(trak->Media->information->child_boxes, stbl_temp);
	gf_list_add(trak->Media->information->child_boxes, stbl);

	if (senc) {
		trak->sample_encryption = senc;
		gf_list_add(trak->child_boxes, senc);
	}
	gf_list_del_item(stbl_temp->child_boxes, stbl_temp->SampleDescription);
	stbl_temp->SampleDescription = NULL;

	count = gf_list_count(stbl->sampleGroupsDescription);
	for (i=0; i<count; i++) {
		GF_Box *b = gf_list_get(stbl->sampleGroupsDescription, i);
		gf_list_del_item(stbl_temp->child_boxes, b);
	}
	stbl_temp->sampleGroupsDescription = NULL;

	gf_list_del_item(stbl_temp->child_boxes, stbl_temp->CompositionToDecode);
	stbl_temp->CompositionToDecode = NULL;
	gf_isom_box_del((GF_Box *)stbl_temp);

	if (e) {
		if (new_tk) gf_isom_box_del((GF_Box *)new_tk);
		return e;
	}

	gf_isom_disable_inplace_rewrite(dest_file);

	/*create default boxes*/
	stbl = new_tk->Media->information->sampleTable;
	stbl->ChunkOffset = gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STCO);
	if (!stbl->ChunkOffset) return GF_OUT_OF_MEM;
	stbl->SampleSize = (GF_SampleSizeBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSZ);
	if (!stbl->SampleSize) return GF_OUT_OF_MEM;
	stbl->SampleToChunk = (GF_SampleToChunkBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSC);
	if (!stbl->SampleToChunk) return GF_OUT_OF_MEM;
	stbl->TimeToSample = (GF_TimeToSampleBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STTS);
	if (!stbl->TimeToSample) return GF_OUT_OF_MEM;

	/*check trackID validity before adding track*/
	if (gf_isom_get_track_by_id(dest_file, new_tk->Header->trackID)) {
		u32 ID = 1;
		while (1) {
			if (RequestTrack(dest_file->moov, ID)) break;
			ID += 1;
			if (ID == 0xFFFFFFFF) break;
		}
		new_tk->Header->trackID = ID;
	}
	if (!dest_file->moov->child_boxes) dest_file->moov->child_boxes = gf_list_new();
	gf_list_add(dest_file->moov->child_boxes, new_tk);
	moov_on_child_box((GF_Box*)dest_file->moov, (GF_Box *)new_tk, GF_FALSE);

	/*set originalID*/
	new_tk->originalID = trak->Header->trackID;
	/*set originalFile*/
	buffer = gf_isom_get_filename(orig_file);
	new_tk->originalFile = gf_crc_32(buffer, (u32) strlen(buffer));

	/*rewrite edit list segmentDuration to new movie timescale*/
	if (dest_file->moov->mvhd->timeScale != orig_file->moov->mvhd->timeScale) {
		Double ts_scale = dest_file->moov->mvhd->timeScale;
		ts_scale /= orig_file->moov->mvhd->timeScale;
		new_tk->Header->duration = (u64) (new_tk->Header->duration * ts_scale);
		if (new_tk->editBox && new_tk->editBox->editList) {
			count = gf_list_count(new_tk->editBox->editList->entryList);
			for (i=0; i<count; i++) {
				GF_EdtsEntry *ent = (GF_EdtsEntry *)gf_list_get(new_tk->editBox->editList->entryList, i);
				ent->segmentDuration = (u64) (ent->segmentDuration * ts_scale);
			}
		}
	}

	if (!new_tk->Media->information->dataInformation->dref) return GF_BAD_PARAM;

	/*reset data ref*/
	if (! (flags & GF_ISOM_CLONE_TRACK_KEEP_DREF) ) {
		GF_SampleEntryBox *entry;
		Bool use_alis = GF_FALSE;
		if (! (flags & GF_ISOM_CLONE_TRACK_NO_QT)) {
			GF_Box *b = gf_list_get(new_tk->Media->information->dataInformation->dref->child_boxes, 0);
			if (b && b->type==GF_QT_BOX_TYPE_ALIS)
				use_alis = GF_TRUE;
		}
		gf_isom_box_array_del(new_tk->Media->information->dataInformation->dref->child_boxes);
		new_tk->Media->information->dataInformation->dref->child_boxes = gf_list_new();
		/*update data ref*/
		entry = (GF_SampleEntryBox*)gf_list_get(new_tk->Media->information->sampleTable->SampleDescription->child_boxes, 0);
		if (entry) {
			u32 dref;
			Media_CreateDataRef(dest_file, new_tk->Media->information->dataInformation->dref, use_alis ?  "alis" : NULL, NULL, &dref);
			entry->dataReferenceIndex = dref;
		}
	} else {
		for (i=0; i<gf_list_count(new_tk->Media->information->dataInformation->dref->child_boxes); i++) {
			GF_DataEntryBox *dref_entry = (GF_DataEntryBox *)gf_list_get(new_tk->Media->information->dataInformation->dref->child_boxes, i);
			if (dref_entry->flags & 1) {
				dref_entry->flags &= ~1;
				e = Media_SetDrefURL((GF_DataEntryURLBox *)dref_entry, orig_file->fileName, dest_file->finalName);
				if (e) return e;
			}
		}
	}

	*dest_track = gf_list_count(dest_file->moov->trackList);

	if (dest_file->moov->mvhd->nextTrackID<= new_tk->Header->trackID)
		dest_file->moov->mvhd->nextTrackID = new_tk->Header->trackID+1;

	return GF_OK;
}

#if 0
/*clones all sampleDescription entries in new track, after an optional reset of existing entries*/
GF_Err gf_isom_clone_sample_descriptions(GF_ISOFile *the_file, u32 trackNumber, GF_ISOFile *orig_file, u32 orig_track, Bool reset_existing)
{
	u32 i;
	GF_TrackBox *dst_trak, *src_trak;
	GF_Err e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	dst_trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!dst_trak || !dst_trak->Media) return GF_BAD_PARAM;
	src_trak = gf_isom_get_track_from_file(orig_file, orig_track);
	if (!src_trak || !src_trak->Media) return GF_BAD_PARAM;

	if (reset_existing) {
		gf_isom_box_array_del(dst_trak->Media->information->sampleTable->SampleDescription->child_boxes);
		dst_trak->Media->information->sampleTable->SampleDescription->child_boxes = gf_list_new();
	}

	for (i=0; i<gf_list_count(src_trak->Media->information->sampleTable->SampleDescription->child_boxes); i++) {
		u32 outDesc;
		e = gf_isom_clone_sample_description(the_file, trackNumber, orig_file, orig_track, i+1, NULL, NULL, &outDesc);
		if (e) break;
	}
	return e;
}
#endif


GF_EXPORT
GF_Err gf_isom_clone_sample_description(GF_ISOFile *the_file, u32 trackNumber, GF_ISOFile *orig_file, u32 orig_track, u32 orig_desc_index, const char *URLname, const char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_BitStream *bs;
	u8 *data;
	u32 data_size;
	GF_Box *entry;
	GF_Err e;
	u32 dataRefIndex;
    u32 mtype;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(orig_file, orig_track);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	entry = (GF_Box*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, orig_desc_index-1);
	if (!entry) return GF_BAD_PARAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_isom_box_size(entry);
	gf_isom_box_write(entry, bs);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	e = gf_isom_box_parse(&entry, bs);
	gf_bs_del(bs);
	gf_free(data);
	if (e) return e;

	/*get new track and insert clone*/
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) goto exit;

	/*get or create the data ref*/
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) goto exit;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) goto exit;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	/*overwrite dref*/
	((GF_SampleEntryBox *)entry)->dataReferenceIndex = dataRefIndex;
	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, entry);
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);

	/*also clone track w/h info*/
    mtype = gf_isom_get_media_type(the_file, trackNumber);
	if (gf_isom_is_video_handler_type(mtype) ) {
		gf_isom_set_visual_info(the_file, trackNumber, (*outDescriptionIndex), ((GF_VisualSampleEntryBox*)entry)->Width, ((GF_VisualSampleEntryBox*)entry)->Height);
	}
	return e;

exit:
	gf_isom_box_del(entry);
	return e;
}

GF_EXPORT
GF_Err gf_isom_new_generic_sample_description(GF_ISOFile *movie, u32 trackNumber, const char *URLname, const char *URNname, GF_GenericSampleDescription *udesc, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !udesc) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(movie, trak->Media->information->dataInformation->dref, (char *)URLname, (char *)URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (gf_isom_is_video_handler_type(trak->Media->handler->handlerType)) {
		GF_GenericVisualSampleEntryBox *entry;
		//create a new entry
		entry = (GF_GenericVisualSampleEntryBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRV);
		if (!entry) return GF_OUT_OF_MEM;

		if (!udesc->codec_tag) {
			entry->EntryType = GF_ISOM_BOX_TYPE_UUID;
			memcpy(entry->uuid, udesc->UUID, sizeof(bin128));
		} else {
			entry->EntryType = udesc->codec_tag;
		}
		if (entry->EntryType == 0) {
			gf_isom_box_del((GF_Box *)entry);
			return GF_NOT_SUPPORTED;
		}

		entry->dataReferenceIndex = dataRefIndex;
		entry->vendor = udesc->vendor_code;
		entry->version = udesc->version;
		entry->revision = udesc->revision;
		entry->temporal_quality = udesc->temporal_quality;
		entry->spatial_quality = udesc->spatial_quality;
		entry->Width = udesc->width;
		entry->Height = udesc->height;
		strcpy(entry->compressor_name, udesc->compressor_name);
		entry->color_table_index = -1;
		entry->frames_per_sample = 1;
		entry->horiz_res = udesc->h_res ? udesc->h_res : 0x00480000;
		entry->vert_res = udesc->v_res ? udesc->v_res : 0x00480000;
		entry->bit_depth = udesc->depth ? udesc->depth : 0x18;
		if (udesc->extension_buf && udesc->extension_buf_size) {
			entry->data = (char*)gf_malloc(sizeof(char) * udesc->extension_buf_size);
			if (!entry->data) {
				gf_isom_box_del((GF_Box *) entry);
				return GF_OUT_OF_MEM;
			}
			memcpy(entry->data, udesc->extension_buf, udesc->extension_buf_size);
			entry->data_size = udesc->extension_buf_size;
		}
		e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, entry);
	}
	else if (trak->Media->handler->handlerType==GF_ISOM_MEDIA_AUDIO) {
		GF_GenericAudioSampleEntryBox *gena;
		//create a new entry
		gena = (GF_GenericAudioSampleEntryBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRA);
		if (!gena) return GF_OUT_OF_MEM;

		if (!udesc->codec_tag) {
			gena->EntryType = GF_ISOM_BOX_TYPE_UUID;
			memcpy(gena->uuid, udesc->UUID, sizeof(bin128));
		} else {
			gena->EntryType = udesc->codec_tag;
		}
		if (gena->EntryType == 0) {
			gf_isom_box_del((GF_Box *)gena);
			return GF_NOT_SUPPORTED;
		}

		gena->dataReferenceIndex = dataRefIndex;
		gena->vendor = udesc->vendor_code;
		gena->version = udesc->version;
		gena->revision = udesc->revision;
		gena->bitspersample = udesc->bits_per_sample ? udesc->bits_per_sample : 16;
		gena->channel_count = udesc->nb_channels ? udesc->nb_channels : 2;
		gena->samplerate_hi = udesc->samplerate;
		gena->samplerate_lo = 0;
		gena->qtff_mode = udesc->is_qtff ? GF_ISOM_AUDIO_QTFF_ON_NOEXT : GF_ISOM_AUDIO_QTFF_NONE;

		if (udesc->extension_buf && udesc->extension_buf_size) {
			gena->data = (char*)gf_malloc(sizeof(char) * udesc->extension_buf_size);
			if (!gena->data) {
				gf_isom_box_del((GF_Box *) gena);
				return GF_OUT_OF_MEM;
			}
			memcpy(gena->data, udesc->extension_buf, udesc->extension_buf_size);
			gena->data_size = udesc->extension_buf_size;
		}
		e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, gena);
	}
	else {
		GF_GenericSampleEntryBox *genm;
		//create a new entry
		genm = (GF_GenericSampleEntryBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRM);
		if (!genm) return GF_OUT_OF_MEM;

		if (!udesc->codec_tag) {
			genm->EntryType = GF_ISOM_BOX_TYPE_UUID;
			memcpy(genm->uuid, udesc->UUID, sizeof(bin128));
		} else {
			genm->EntryType = udesc->codec_tag;
		}
		if (genm->EntryType == 0) {
			gf_isom_box_del((GF_Box *)genm);
			return GF_NOT_SUPPORTED;
		}

		genm->dataReferenceIndex = dataRefIndex;
		if (udesc->extension_buf && udesc->extension_buf_size) {
			genm->data = (char*)gf_malloc(sizeof(char) * udesc->extension_buf_size);
			if (!genm->data) {
				gf_isom_box_del((GF_Box *) genm);
				return GF_OUT_OF_MEM;
			}
			memcpy(genm->data, udesc->extension_buf, udesc->extension_buf_size);
			genm->data_size = udesc->extension_buf_size;
		}
		e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->child_boxes, genm);
	}
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	return e;
}

//use carefully. Very useful when you made a lot of changes (IPMP, IPI, OCI, ...)
//THIS WILL REPLACE THE WHOLE DESCRIPTOR ...
#if 0 //unused
/*change the data field of an unknown sample description*/
GF_Err gf_isom_change_generic_sample_description(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, GF_GenericSampleDescription *udesc)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_GenericVisualSampleEntryBox *entry;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !StreamDescriptionIndex) return GF_BAD_PARAM;

	entry = (GF_GenericVisualSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	if (entry->type == GF_ISOM_BOX_TYPE_GNRV) {
		entry->vendor = udesc->vendor_code;
		entry->version = udesc->version;
		entry->revision = udesc->revision;
		entry->temporal_quality = udesc->temporal_quality;
		entry->spatial_quality = udesc->spatial_quality;
		entry->Width = udesc->width;
		entry->Height = udesc->height;
		strcpy(entry->compressor_name, udesc->compressor_name);
		entry->color_table_index = -1;
		entry->frames_per_sample = 1;
		entry->horiz_res = udesc->h_res ? udesc->h_res : 0x00480000;
		entry->vert_res = udesc->v_res ? udesc->v_res : 0x00480000;
		entry->bit_depth = udesc->depth ? udesc->depth : 0x18;
		if (entry->data) gf_free(entry->data);
		entry->data = NULL;
		entry->data_size = 0;
		if (udesc->extension_buf && udesc->extension_buf_size) {
			entry->data = (char*)gf_malloc(sizeof(char) * udesc->extension_buf_size);
			if (!entry->data) {
				gf_isom_box_del((GF_Box *) entry);
				return GF_OUT_OF_MEM;
			}
			memcpy(entry->data, udesc->extension_buf, udesc->extension_buf_size);
			entry->data_size = udesc->extension_buf_size;
		}
		return GF_OK;
	} else if (entry->type == GF_ISOM_BOX_TYPE_GNRA) {
		GF_GenericAudioSampleEntryBox *gena = (GF_GenericAudioSampleEntryBox *)entry;
		gena->vendor = udesc->vendor_code;
		gena->version = udesc->version;
		gena->revision = udesc->revision;
		gena->bitspersample = udesc->bits_per_sample ? udesc->bits_per_sample : 16;
		gena->channel_count = udesc->nb_channels ? udesc->nb_channels : 2;
		gena->samplerate_hi = udesc->samplerate;
		gena->samplerate_lo = 0;
		if (gena->data) gf_free(gena->data);
		gena->data = NULL;
		gena->data_size = 0;

		if (udesc->extension_buf && udesc->extension_buf_size) {
			gena->data = (char*)gf_malloc(sizeof(char) * udesc->extension_buf_size);
			if (!gena->data) {
				gf_isom_box_del((GF_Box *) gena);
				return GF_OUT_OF_MEM;
			}
			memcpy(gena->data, udesc->extension_buf, udesc->extension_buf_size);
			gena->data_size = udesc->extension_buf_size;
		}
		return GF_OK;
	} else if (entry->type == GF_ISOM_BOX_TYPE_GNRM) {
		GF_GenericSampleEntryBox *genm = (GF_GenericSampleEntryBox *)entry;
		if (genm->data) gf_free(genm->data);
		genm->data = NULL;
		genm->data_size = 0;

		if (udesc->extension_buf && udesc->extension_buf_size) {
			genm->data = (char*)gf_malloc(sizeof(char) * udesc->extension_buf_size);
			if (!genm->data) {
				gf_isom_box_del((GF_Box *) genm);
				return GF_OUT_OF_MEM;
			}
			memcpy(genm->data, udesc->extension_buf, udesc->extension_buf_size);
			genm->data_size = udesc->extension_buf_size;
		}
		return GF_OK;
	}
	return GF_BAD_PARAM;
}
#endif

#if 0
/*removes given stream description*/
GF_Err gf_isom_remove_sample_description(GF_ISOFile *movie, u32 trackNumber, u32 streamDescIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_Box *entry;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !streamDescIndex) return GF_BAD_PARAM;
	entry = (GF_Box*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, streamDescIndex-1);
	if (!entry) return GF_BAD_PARAM;
	gf_list_rem(trak->Media->information->sampleTable->SampleDescription->child_boxes, streamDescIndex-1);
	gf_isom_box_del(entry);
	return GF_OK;
}
#endif

//sets a track reference
GF_EXPORT
GF_Err gf_isom_set_track_reference(GF_ISOFile *the_file, u32 trackNumber, u32 referenceType, GF_ISOTrackID ReferencedTrackID)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_TrackReferenceBox *tref;
	GF_TrackReferenceTypeBox *dpnd;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	//no tref, create one
	tref = trak->References;
	if (!tref) {
		tref = (GF_TrackReferenceBox *) gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_TREF);
		if (!tref) return GF_OUT_OF_MEM;
		e = trak_on_child_box((GF_Box*)trak, (GF_Box *) tref, GF_FALSE);
		if (e) return e;
	}
	//find a ref of the given type
	e = Track_FindRef(trak, referenceType, &dpnd);
	if (e) return e;

	if (!dpnd) {
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new_parent(&tref->child_boxes, GF_ISOM_BOX_TYPE_REFT);
		if (!dpnd) return GF_OUT_OF_MEM;
		dpnd->reference_type = referenceType;
	}
	//add the ref
	return reftype_AddRefTrack(dpnd, ReferencedTrackID, NULL);
}

GF_EXPORT
GF_Err gf_isom_purge_track_reference(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	GF_TrackReferenceTypeBox *ref;
	u32 i=0;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	//no tref, nothing to remove
	if (!trak->References) return GF_OK;

	while ((ref = gf_list_enum(trak->References->child_boxes, &i))) {
		u32 k;
		if (!ref->reference_type) continue;

		for (k=0; k<ref->trackIDCount; k++) {
			u32 tk = gf_isom_get_track_by_id(the_file, ref->trackIDs[k]);
			if (!tk) {
				memmove(&ref->trackIDs[k], &ref->trackIDs[k+1], ref->trackIDCount-k-1);
				k--;
				ref->trackIDCount--;
			}
		}
		if (!ref->trackIDCount) {
			i--;
			gf_isom_box_del_parent(&trak->References->child_boxes, (GF_Box *) ref);
		}
	}
	if (!trak->References->child_boxes || !gf_list_count(trak->References->child_boxes)) {
		gf_isom_box_del_parent(&trak->child_boxes, (GF_Box *) trak->References);
		trak->References = NULL;
	}
	return GF_OK;
}

//sets a track reference
GF_EXPORT
GF_Err gf_isom_remove_track_references(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (trak->References) {
		gf_isom_box_del_parent(&trak->child_boxes, (GF_Box *)trak->References);
		trak->References = NULL;
	}
	return GF_OK;
}

GF_Err gf_isom_remove_track_reference(GF_ISOFile *isom_file, u32 trackNumber, u32 ref_type)
{
	GF_TrackBox *trak;
	u32 i=0;
	GF_TrackReferenceTypeBox *ref;
	trak = gf_isom_get_track_from_file(isom_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (!trak->References) return GF_OK;
	while ((ref = gf_list_enum(trak->References->child_boxes, &i))) {
		if (ref->reference_type == ref_type) {
			gf_isom_box_del_parent(&trak->References->child_boxes, (GF_Box *)ref);
			break;
		}
	}
	return GF_OK;

}

//changes track ID
GF_EXPORT
GF_Err gf_isom_set_track_id(GF_ISOFile *movie, u32 trackNumber, GF_ISOTrackID trackID)
{
	GF_TrackReferenceTypeBox *ref;
	GF_TrackBox *trak, *a_trak;
	u32 i, j, k;

	if (!movie) return GF_BAD_PARAM;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (trak && (trak->Header->trackID==trackID)) return GF_OK;
	a_trak = gf_isom_get_track_from_id(movie->moov, trackID);
	if (!trak || a_trak) return GF_BAD_PARAM;

	if (movie->moov->mvhd->nextTrackID<=trackID)
		movie->moov->mvhd->nextTrackID = trackID;

	/*rewrite all dependencies*/
	i=0;
	while ((a_trak = (GF_TrackBox*)gf_list_enum(movie->moov->trackList, &i))) {
		if (!a_trak->References) continue;
		j=0;
		while ((ref = (GF_TrackReferenceTypeBox *)gf_list_enum(a_trak->References->child_boxes, &j))) {
			for (k=0; k<ref->trackIDCount; k++) {
				if (ref->trackIDs[k]==trak->Header->trackID) {
					ref->trackIDs[k] = trackID;
					break;
				}
			}
		}
	}

	/*and update IOD if any*/
	if (movie->moov->iods && movie->moov->iods->descriptor) {
		GF_ES_ID_Inc *inc;
		GF_IsomObjectDescriptor *od = (GF_IsomObjectDescriptor *)movie->moov->iods->descriptor;

		i=0;
		while ((inc = (GF_ES_ID_Inc*)gf_list_enum(od->ES_ID_IncDescriptors, &i))) {
			if (inc->trackID==trak->Header->trackID) inc->trackID = trackID;
		}
	}
	trak->Header->trackID = trackID;
	return GF_OK;
}

/*force to rewrite all dependencies when the trackID of referenced track changes*/
GF_EXPORT
GF_Err gf_isom_rewrite_track_dependencies(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackReferenceTypeBox *ref;
	GF_TrackBox *trak, *a_trak;
	u32 i, k;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak)
		return GF_BAD_PARAM;
	if (!trak->References)
		return GF_OK;

	i=0;
	while ((ref = (GF_TrackReferenceTypeBox *)gf_list_enum(trak->References->child_boxes, &i))) {
		for (k=0; k < ref->trackIDCount; k++) {
			a_trak = gf_isom_get_track_from_original_id(movie->moov, ref->trackIDs[k], trak->originalFile);
			if (a_trak) {
				ref->trackIDs[k] = a_trak->Header->trackID;
			} else {
				a_trak = gf_isom_get_track_from_id(movie->moov, ref->trackIDs[k]);
				/*we should have a track with no original ID (not imported) - should we rewrite the dependency ?*/
				if (! a_trak || a_trak->originalID) return GF_BAD_PARAM;
			}
		}
	}

	return GF_OK;
}

#if 0 //unused

/*! changes the sample description index of a sample
\param isom_file the destination ISO file
\param trackNumber the destination track
\param sampleNum the target sample number
\param fnewSampleDescIndex the new sample description index to assign to the sample
\return error if any
*/
GF_EXPORT
GF_Err gf_isom_change_sample_desc_index(GF_ISOFile *the_file, u32 trackNumber, u32 sample_number, u32 newSampleDescIndex)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !sample_number || !newSampleDescIndex) return GF_BAD_PARAM;
	if (!trak->is_unpacked) {
		unpack_track(trak);
	}
	if (!trak->Media->information->sampleTable->SampleToChunk) return GF_BAD_PARAM;
	if (trak->Media->information->sampleTable->SampleToChunk->nb_entries < sample_number) return GF_BAD_PARAM;
	trak->Media->information->sampleTable->SampleToChunk->entries[sample_number-1].sampleDescriptionIndex = newSampleDescIndex;
	return GF_OK;
}

/*modify CTS offset of a given sample (used for B-frames) - MUST be called in unpack mode only*/
GF_EXPORT
GF_Err gf_isom_modify_cts_offset(GF_ISOFile *the_file, u32 trackNumber, u32 sample_number, u32 offset)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->CompositionOffset) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->CompositionOffset->unpack_mode) return GF_BAD_PARAM;
	/*we're in unpack mode: one entry per sample*/
	trak->Media->information->sampleTable->CompositionOffset->entries[sample_number - 1].decodingOffset = offset;
	return GF_OK;
}
#endif

GF_EXPORT
GF_Err gf_isom_shift_cts_offset(GF_ISOFile *the_file, u32 trackNumber, s32 offset_shift)
{
	u32 i;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->CompositionOffset) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->CompositionOffset->unpack_mode) return GF_BAD_PARAM;

	for (i=0; i<trak->Media->information->sampleTable->CompositionOffset->nb_entries; i++) {
		/*we're in unpack mode: one entry per sample*/
		trak->Media->information->sampleTable->CompositionOffset->entries[i].decodingOffset -= offset_shift;
	}
	return GF_OK;
}

#if 0 //unused
GF_Err gf_isom_remove_cts_info(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_SampleTableBox *stbl;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (!stbl->CompositionOffset) return GF_OK;

	gf_isom_box_del_parent(&stbl->child_boxes, (GF_Box *)stbl->CompositionOffset);
	stbl->CompositionOffset = NULL;
	return GF_OK;
}
#endif

GF_EXPORT
GF_Err gf_isom_set_cts_packing(GF_ISOFile *the_file, u32 trackNumber, Bool unpack)
{
	GF_Err e;
	GF_Err stbl_repackCTS(GF_CompositionOffsetBox *ctts);
	GF_Err stbl_unpackCTS(GF_SampleTableBox *stbl);
	GF_SampleTableBox *stbl;

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (unpack) {
		if (!stbl->CompositionOffset) {
			stbl->CompositionOffset = (GF_CompositionOffsetBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_CTTS);
			if (!stbl->CompositionOffset) return GF_OUT_OF_MEM;
		}
		e = stbl_unpackCTS(stbl);
	} else {
		if (!stbl->CompositionOffset) return GF_OK;
		e = stbl_repackCTS(stbl->CompositionOffset);
	}
	if (e) return e;
	return SetTrackDuration(trak);
}

GF_EXPORT
GF_Err gf_isom_set_track_matrix(GF_ISOFile *the_file, u32 trackNumber, s32 matrix[9])
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Header) return GF_BAD_PARAM;
	memcpy(trak->Header->matrix, matrix, sizeof(trak->Header->matrix));
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_track_layout_info(GF_ISOFile *the_file, u32 trackNumber, u32 width, u32 height, s32 translation_x, s32 translation_y, s16 layer)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Header) return GF_BAD_PARAM;
	trak->Header->width = width;
	trak->Header->height = height;
	trak->Header->matrix[6] = translation_x;
	trak->Header->matrix[7] = translation_y;
	trak->Header->layer = layer;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_media_timescale(GF_ISOFile *the_file, u32 trackNumber, u32 newTS, u32 new_tsinc, u32 force_rescale_type)
{
	Double scale;
	u32 old_ts_inc=0;
	u32 old_timescale;
	GF_TrackBox *trak;
	GF_SampleTableBox *stbl;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->mediaHeader) return GF_BAD_PARAM;
	if ((trak->Media->mediaHeader->timeScale==newTS) && !new_tsinc)
		return GF_EOS;

	if (!newTS) newTS = trak->Media->mediaHeader->timeScale;
	scale = newTS;
	scale /= trak->Media->mediaHeader->timeScale;
	old_timescale = trak->Media->mediaHeader->timeScale;
	trak->Media->mediaHeader->timeScale = newTS;

	stbl = trak->Media->information->sampleTable;
	if (new_tsinc) {
		u32 i;
		if (!stbl->TimeToSample || !stbl->TimeToSample->nb_entries)
			return GF_BAD_PARAM;

		for (i=0; i<stbl->TimeToSample->nb_entries; i++) {
			if (!old_ts_inc)
				old_ts_inc = stbl->TimeToSample->entries[i].sampleDelta;
			else if (old_ts_inc<stbl->TimeToSample->entries[i].sampleDelta)
				old_ts_inc = stbl->TimeToSample->entries[i].sampleDelta;
		}

		if ((old_timescale==newTS) && (old_ts_inc==new_tsinc))
			return GF_EOS;

		if (!force_rescale_type)
			force_rescale_type = 1;
		else if (force_rescale_type==2) {
			gf_free(stbl->TimeToSample->entries);
			stbl->TimeToSample->alloc_size = 1;
			stbl->TimeToSample->nb_entries = 1;
			stbl->TimeToSample->entries = gf_malloc(sizeof(GF_SttsEntry));
			stbl->TimeToSample->entries[0].sampleDelta = new_tsinc;
			stbl->TimeToSample->entries[0].sampleCount = stbl->SampleSize->sampleCount;
		}


		for (i=0; i<stbl->TimeToSample->nb_entries; i++) {
			stbl->TimeToSample->entries[i].sampleDelta = new_tsinc;
		}

		if (stbl->CompositionOffset) {
			for (i=0; i<stbl->CompositionOffset->nb_entries; i++) {
				u32 old_offset = stbl->CompositionOffset->entries[i].decodingOffset;
				if (force_rescale_type==2) {
					u32 val = old_offset ;
					//get number of TS delta
					old_offset /= old_ts_inc;
					if (old_offset * old_ts_inc < val)
						old_offset++;
					old_offset *= new_tsinc;
				} else {
					old_offset *= new_tsinc;
					old_offset /= old_ts_inc;
				}
				stbl->CompositionOffset->entries[i].decodingOffset = old_offset;
			}
		}

#define RESCALE_TSVAL(_tsval) {\
			s64 val = ((s64) _tsval) * new_tsinc;\
			val /= old_ts_inc;\
			_tsval = (s32) val;\
		}

		if (stbl->CompositionToDecode) {
			RESCALE_TSVAL(stbl->CompositionToDecode->compositionEndTime)
			RESCALE_TSVAL(stbl->CompositionToDecode->compositionStartTime)
			RESCALE_TSVAL(stbl->CompositionToDecode->compositionToDTSShift)
			RESCALE_TSVAL(stbl->CompositionToDecode->greatestDecodeToDisplayDelta)
			RESCALE_TSVAL(stbl->CompositionToDecode->leastDecodeToDisplayDelta)
		}
		if (trak->editBox) {
			GF_EdtsEntry *ent;
			i=0;
			while ((ent = (GF_EdtsEntry*)gf_list_enum(trak->editBox->editList->entryList, &i))) {
				RESCALE_TSVAL(ent->mediaTime)
			}
		}
#undef RESCALE_TSVAL

		return SetTrackDuration(trak);
	}

	//rescale timings
	u32 i, k, idx, last_delta;
	u64 cur_dts;
	u64*DTSs = NULL;
	s64*CTSs = NULL;

	if (trak->editBox) {
		GF_EdtsEntry *ent;
		i=0;
		while ((ent = (GF_EdtsEntry*)gf_list_enum(trak->editBox->editList->entryList, &i))) {
			ent->mediaTime = (u32) (scale*ent->mediaTime);
		}
	}
	if (! stbl || !stbl->TimeToSample || !stbl->TimeToSample->nb_entries) {
		return SetTrackDuration(trak);
	}

	idx = 0;
	cur_dts = 0;
	//unpack the DTSs
	DTSs = (u64*)gf_malloc(sizeof(u64) * (stbl->SampleSize->sampleCount) );
	if (!DTSs) return GF_OUT_OF_MEM;

	CTSs = NULL;
	if (stbl->CompositionOffset) {
		CTSs = (s64*)gf_malloc(sizeof(u64) * (stbl->SampleSize->sampleCount) );
		if (!CTSs) return GF_OUT_OF_MEM;
	}

	for (i=0; i<stbl->TimeToSample->nb_entries; i++) {
		for (k=0; k<stbl->TimeToSample->entries[i].sampleCount; k++) {
			cur_dts += stbl->TimeToSample->entries[i].sampleDelta;
			DTSs[idx] = (u64) (cur_dts * scale);

			if (stbl->CompositionOffset) {
				s32 cts_o;
				stbl_GetSampleCTS(stbl->CompositionOffset, idx+1, &cts_o);
				CTSs[idx] = (s64) ( ((s64) cur_dts + cts_o) * scale);
			}
			idx++;
		}
	}
	last_delta = (u32) (stbl->TimeToSample->entries[stbl->TimeToSample->nb_entries-1].sampleDelta * scale);

	//repack DTS
	if (stbl->SampleSize->sampleCount) {
		stbl->TimeToSample->entries = gf_realloc(stbl->TimeToSample->entries, sizeof(GF_SttsEntry)*stbl->SampleSize->sampleCount);
		memset(stbl->TimeToSample->entries, 0, sizeof(GF_SttsEntry)*stbl->SampleSize->sampleCount);
		stbl->TimeToSample->entries[0].sampleDelta = (u32) DTSs[0];
		stbl->TimeToSample->entries[0].sampleCount = 1;
		idx=0;
		for (i=1; i< stbl->SampleSize->sampleCount - 1; i++) {
			if (DTSs[i+1] - DTSs[i] == stbl->TimeToSample->entries[idx].sampleDelta) {
				stbl->TimeToSample->entries[idx].sampleCount++;
			} else {
				idx++;
				stbl->TimeToSample->entries[idx].sampleDelta = (u32) ( DTSs[i+1] - DTSs[i] );
				stbl->TimeToSample->entries[idx].sampleCount=1;
			}
		}
		if (stbl->SampleSize->sampleCount > 1) {
			//add the sample delta for the last sample
			if (stbl->TimeToSample->entries[idx].sampleDelta == last_delta) {
				stbl->TimeToSample->entries[idx].sampleCount++;
			} else {
				idx++;
				stbl->TimeToSample->entries[idx].sampleDelta = last_delta;
				stbl->TimeToSample->entries[idx].sampleCount=1;
			}

			stbl->TimeToSample->nb_entries = idx+1;
			stbl->TimeToSample->entries = gf_realloc(stbl->TimeToSample->entries, sizeof(GF_SttsEntry)*stbl->TimeToSample->nb_entries);
		}
	}

	if (CTSs && stbl->SampleSize->sampleCount>0) {
		//repack CTS
		stbl->CompositionOffset->entries = gf_realloc(stbl->CompositionOffset->entries, sizeof(GF_DttsEntry)*stbl->SampleSize->sampleCount);
		memset(stbl->CompositionOffset->entries, 0, sizeof(GF_DttsEntry)*stbl->SampleSize->sampleCount);
		stbl->CompositionOffset->entries[0].decodingOffset = (s32) (CTSs[0] - DTSs[0]);
		stbl->CompositionOffset->entries[0].sampleCount = 1;
		idx=0;
		for (i=1; i< stbl->SampleSize->sampleCount; i++) {
			s32 cts_o = (s32) (CTSs[i] - DTSs[i]);
			if (cts_o == stbl->CompositionOffset->entries[idx].decodingOffset) {
				stbl->CompositionOffset->entries[idx].sampleCount++;
			} else {
				idx++;
				stbl->CompositionOffset->entries[idx].decodingOffset = cts_o;
				stbl->CompositionOffset->entries[idx].sampleCount=1;
			}
		}
		stbl->CompositionOffset->nb_entries = idx+1;
		stbl->CompositionOffset->entries = gf_realloc(stbl->CompositionOffset->entries, sizeof(GF_DttsEntry)*stbl->CompositionOffset->nb_entries);

		gf_free(CTSs);
	}
	gf_free(DTSs);

	if (stbl->CompositionToDecode) {
		stbl->CompositionToDecode->compositionEndTime = (s32) (stbl->CompositionToDecode->compositionEndTime * scale);
		stbl->CompositionToDecode->compositionStartTime = (s32)(stbl->CompositionToDecode->compositionStartTime * scale);
		stbl->CompositionToDecode->compositionToDTSShift = (s32)(stbl->CompositionToDecode->compositionToDTSShift * scale);
		stbl->CompositionToDecode->greatestDecodeToDisplayDelta = (s32)(stbl->CompositionToDecode->greatestDecodeToDisplayDelta * scale);
		stbl->CompositionToDecode->leastDecodeToDisplayDelta = (s32)(stbl->CompositionToDecode->leastDecodeToDisplayDelta * scale);
	}

	return SetTrackDuration(trak);
}

GF_EXPORT
Bool gf_isom_box_equal(GF_Box *a, GF_Box *b)
{
	Bool ret;
	u8 *data1, *data2;
	u32 data1_size, data2_size;
	GF_BitStream *bs;

	if (a == b) return GF_TRUE;
	if (!a || !b) return GF_FALSE;

	data1 = data2 = NULL;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_isom_box_size(a);
	gf_isom_box_write(a, bs);
	gf_bs_get_content(bs, &data1, &data1_size);
	gf_bs_del(bs);

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_isom_box_size(b);
	gf_isom_box_write(b, bs);
	gf_bs_get_content(bs, &data2, &data2_size);
	gf_bs_del(bs);

	ret = GF_FALSE;
	if (data1_size == data2_size) {
		ret = (memcmp(data1, data2, sizeof(char)*data1_size) == 0) ? GF_TRUE : GF_FALSE;
	}
	gf_free(data1);
	gf_free(data2);
	return ret;
}

GF_EXPORT
Bool gf_isom_is_same_sample_description(GF_ISOFile *f1, u32 tk1, u32 sdesc_index1, GF_ISOFile *f2, u32 tk2, u32 sdesc_index2)
{
	u32 i, count;
	GF_TrackBox *trak1, *trak2;
	GF_ESD *esd1, *esd2;
	Bool need_memcmp, ret;
	GF_Box *a, *b;

	/*get orig sample desc and clone it*/
	trak1 = gf_isom_get_track_from_file(f1, tk1);
	if (!trak1 || !trak1->Media) return GF_FALSE;
	trak2 = gf_isom_get_track_from_file(f2, tk2);
	if (!trak2 || !trak2->Media) return GF_FALSE;

	if (trak1->Media->handler->handlerType != trak2->Media->handler->handlerType) return GF_FALSE;
	count = gf_list_count(trak1->Media->information->sampleTable->SampleDescription->child_boxes);
	if (count != gf_list_count(trak2->Media->information->sampleTable->SampleDescription->child_boxes)) {
		if (!sdesc_index1 && !sdesc_index2) return GF_FALSE;
	}

	need_memcmp = GF_TRUE;
	for (i=0; i<count; i++) {
		GF_Box *ent1 = (GF_Box *)gf_list_get(trak1->Media->information->sampleTable->SampleDescription->child_boxes, i);
		GF_Box *ent2 = (GF_Box *)gf_list_get(trak2->Media->information->sampleTable->SampleDescription->child_boxes, i);

		if (sdesc_index1) ent1 = (GF_Box *)gf_list_get(trak1->Media->information->sampleTable->SampleDescription->child_boxes, sdesc_index1 - 1);
		if (sdesc_index2) ent2 = (GF_Box *)gf_list_get(trak2->Media->information->sampleTable->SampleDescription->child_boxes, sdesc_index2 - 1);

		if (!ent1 || !ent2) return GF_FALSE;
		if (ent1->type != ent2->type) return GF_FALSE;

		switch (ent1->type) {
		/*for MPEG-4 streams, only compare decSpecInfo (bitrate may not be the same but that's not an issue)*/
		case GF_ISOM_BOX_TYPE_MP4S:
		case GF_ISOM_BOX_TYPE_MP4A:
		case GF_ISOM_BOX_TYPE_MP4V:
		case GF_ISOM_BOX_TYPE_ENCA:
		case GF_ISOM_BOX_TYPE_ENCV:
		case GF_ISOM_BOX_TYPE_RESV:
		case GF_ISOM_BOX_TYPE_ENCS:
			Media_GetESD(trak1->Media, sdesc_index1 ? sdesc_index1 : i+1, &esd1, GF_TRUE);
			Media_GetESD(trak2->Media, sdesc_index2 ? sdesc_index2 : i+1, &esd2, GF_TRUE);
			if (!esd1 || !esd2) continue;
			need_memcmp = GF_FALSE;
			if (esd1->decoderConfig->streamType != esd2->decoderConfig->streamType) return GF_FALSE;
			if (esd1->decoderConfig->objectTypeIndication != esd2->decoderConfig->objectTypeIndication) return GF_FALSE;
			if (!esd1->decoderConfig->decoderSpecificInfo && esd2->decoderConfig->decoderSpecificInfo) return GF_FALSE;
			if (esd1->decoderConfig->decoderSpecificInfo && !esd2->decoderConfig->decoderSpecificInfo) return GF_FALSE;
			if (!esd1->decoderConfig->decoderSpecificInfo || !esd2->decoderConfig->decoderSpecificInfo) continue;
			if (memcmp(esd1->decoderConfig->decoderSpecificInfo->data, esd2->decoderConfig->decoderSpecificInfo->data, sizeof(char)*esd1->decoderConfig->decoderSpecificInfo->dataLength)!=0) return GF_FALSE;
			break;
		case GF_ISOM_BOX_TYPE_HVT1:
			return GF_TRUE;
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
		case GF_ISOM_BOX_TYPE_AV01:
		case GF_ISOM_BOX_TYPE_VVC1:
		case GF_ISOM_BOX_TYPE_VVI1:
		{
			GF_MPEGVisualSampleEntryBox *avc1 = (GF_MPEGVisualSampleEntryBox *)ent1;
			GF_MPEGVisualSampleEntryBox *avc2 = (GF_MPEGVisualSampleEntryBox *)ent2;

			if (avc1->hevc_config)
				a = (GF_Box *) avc1->hevc_config;
			else if (avc1->lhvc_config)
				a = (GF_Box *) avc1->lhvc_config;
			else if (avc1->svc_config)
				a = (GF_Box *) avc1->svc_config;
			else if (avc1->mvc_config)
				a = (GF_Box *) avc1->mvc_config;
			else if (avc1->av1_config)
				a = (GF_Box *)avc1->av1_config;
			else
				a = (GF_Box *) avc1->avc_config;

			if (avc2->hevc_config)
				b = (GF_Box *) avc2->hevc_config;
			else if (avc2->lhvc_config)
				b = (GF_Box *) avc2->lhvc_config;
			else if (avc2->svc_config)
				b = (GF_Box *) avc2->svc_config;
			else if (avc2->mvc_config)
				b = (GF_Box *) avc2->mvc_config;
			else if (avc2->av1_config)
				b = (GF_Box *)avc2->av1_config;
			else
				b = (GF_Box *) avc2->avc_config;

			return gf_isom_box_equal(a,b);
		}
		break;
		case GF_ISOM_BOX_TYPE_LSR1:
		{
			GF_LASeRSampleEntryBox *lsr1 = (GF_LASeRSampleEntryBox *)ent1;
			GF_LASeRSampleEntryBox *lsr2 = (GF_LASeRSampleEntryBox *)ent2;
			if (lsr1->lsr_config && lsr2->lsr_config
			        && lsr1->lsr_config->hdr && lsr2->lsr_config->hdr
			        && (lsr1->lsr_config->hdr_size==lsr2->lsr_config->hdr_size)
			        && !memcmp(lsr1->lsr_config->hdr, lsr2->lsr_config->hdr, lsr2->lsr_config->hdr_size)
			   ) {
				return GF_TRUE;
			}
			return GF_FALSE;
		}
		break;
#ifndef GPAC_DISABLE_VTT
		case GF_ISOM_BOX_TYPE_WVTT:
		{
			GF_WebVTTSampleEntryBox *wvtt1 = (GF_WebVTTSampleEntryBox *)ent1;
			GF_WebVTTSampleEntryBox *wvtt2 = (GF_WebVTTSampleEntryBox *)ent2;
			if (wvtt1->config && wvtt2->config &&
			        (wvtt1->config->string && wvtt2->config->string && !strcmp(wvtt1->config->string, wvtt2->config->string))) {
				return GF_TRUE;
			}
			return GF_FALSE;
		}
		break;
#endif
		case GF_ISOM_BOX_TYPE_STPP:
		{
			GF_MetaDataSampleEntryBox *stpp1 = (GF_MetaDataSampleEntryBox *)ent1;
			GF_MetaDataSampleEntryBox *stpp2 = (GF_MetaDataSampleEntryBox *)ent2;
			if (stpp1->xml_namespace && stpp2->xml_namespace && !strcmp(stpp1->xml_namespace, stpp2->xml_namespace)) {
				return GF_TRUE;
			}
			return GF_FALSE;
		}
		break;
		case GF_ISOM_BOX_TYPE_SBTT:
		{
			return GF_FALSE;
		}
		break;
		case GF_ISOM_BOX_TYPE_STXT:
		{
			GF_MetaDataSampleEntryBox *stxt1 = (GF_MetaDataSampleEntryBox *)ent1;
			GF_MetaDataSampleEntryBox *stxt2 = (GF_MetaDataSampleEntryBox *)ent2;
			if (stxt1->mime_type && stxt2->mime_type &&
			        ( (!stxt1->config && !stxt2->config) ||
			          (stxt1->config && stxt2->config && stxt1->config->config && stxt2->config->config &&
			           !strcmp(stxt1->config->config, stxt2->config->config)))) {
				return GF_TRUE;
			}
			return GF_FALSE;
		}
		case GF_ISOM_BOX_TYPE_MP3:
		case GF_QT_SUBTYPE_RAW_AUD:
		case GF_QT_SUBTYPE_TWOS:
		case GF_QT_SUBTYPE_SOWT:
		case GF_QT_SUBTYPE_FL32:
		case GF_QT_SUBTYPE_FL64:
		case GF_QT_SUBTYPE_IN24:
		case GF_QT_SUBTYPE_IN32:
		case GF_QT_SUBTYPE_ULAW:
		case GF_QT_SUBTYPE_ALAW:
		case GF_QT_SUBTYPE_ADPCM:
		case GF_QT_SUBTYPE_IMA_ADPCM:
		case GF_QT_SUBTYPE_DVCA:
		case GF_QT_SUBTYPE_QDMC:
		case GF_QT_SUBTYPE_QDMC2:
		case GF_QT_SUBTYPE_QCELP:
		case GF_QT_SUBTYPE_kMP3:
			return GF_TRUE;
		case GF_QT_SUBTYPE_APCH:
		case GF_QT_SUBTYPE_APCO:
		case GF_QT_SUBTYPE_APCN:
		case GF_QT_SUBTYPE_APCS:
		case GF_QT_SUBTYPE_AP4X:
		case GF_QT_SUBTYPE_AP4H:
		case GF_QT_SUBTYPE_RAW_VID:
		case GF_QT_SUBTYPE_YUYV:
		case GF_QT_SUBTYPE_UYVY:
		case GF_QT_SUBTYPE_YUV444:
		case GF_QT_SUBTYPE_YUVA444:
		case GF_QT_SUBTYPE_YUV422_10:
		case GF_QT_SUBTYPE_YUV444_10:
		case GF_QT_SUBTYPE_YUV422_16:
		case GF_QT_SUBTYPE_YUV420:
		case GF_QT_SUBTYPE_I420:
		case GF_QT_SUBTYPE_IYUV:
		case GF_QT_SUBTYPE_YV12:
		case GF_QT_SUBTYPE_YVYU:
		case GF_QT_SUBTYPE_RGBA:
		case GF_QT_SUBTYPE_ABGR:
			return GF_TRUE;
		}

		if (sdesc_index1 && sdesc_index2) break;
	}
	if (!need_memcmp) return GF_TRUE;
	a = (GF_Box *)trak1->Media->information->sampleTable->SampleDescription;
	b = (GF_Box *)trak2->Media->information->sampleTable->SampleDescription;
	//we ignore all bitrate boxes when comparing the box, disable their writing
	gf_isom_registry_disable(GF_ISOM_BOX_TYPE_BTRT, GF_TRUE);
	ret = gf_isom_box_equal(a,b);
	//re-enable btrt writing
	gf_isom_registry_disable(GF_ISOM_BOX_TYPE_BTRT, GF_FALSE);

	return ret;
}

GF_EXPORT
u64 gf_isom_estimate_size(GF_ISOFile *movie)
{
	GF_Err e;
	GF_Box *a;
	u32 i, count;
	u64 mdat_size;
	if (!movie || !movie->moov) return 0;

	mdat_size = 0;
	count = gf_list_count(movie->moov->trackList);
	for (i=0; i<count; i++) {
		mdat_size += gf_isom_get_media_data_size(movie, i+1);
	}
	if (mdat_size) {
		mdat_size += 8;
		if (mdat_size > 0xFFFFFFFF) mdat_size += 8;
	}

	i=0;
	while ((a = (GF_Box*)gf_list_enum(movie->TopBoxes, &i))) {
		e = gf_isom_box_size(a);
		if (e == GF_OK)
			mdat_size += a->size;
	}
	return mdat_size;
}


//set shadowing on/off
#if 0 //unused
GF_Err gf_isom_remove_sync_shadows(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	GF_SampleTableBox *stbl;

	if (movie->openMode == GF_ISOM_OPEN_READ) return GF_ISOM_INVALID_MODE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (stbl->ShadowSync) {
		gf_isom_box_del_parent(&stbl->child_boxes, (GF_Box *) stbl->ShadowSync);
		stbl->ShadowSync = NULL;
	}
	return GF_OK;
}

/*Use this function to do the shadowing if you use shadowing.
the sample to be shadowed MUST be a non-sync sample (ignored if not)
the sample shadowing must be a Sync sample (error if not)*/
GF_Err gf_isom_set_sync_shadow(GF_ISOFile *movie, u32 trackNumber, u32 sampleNumber, u32 syncSample)
{
	GF_TrackBox *trak;
	GF_SampleTableBox *stbl;
	GF_ISOSAPType isRAP;
	GF_Err e;

	if (movie->openMode == GF_ISOM_OPEN_READ) return GF_ISOM_INVALID_MODE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !sampleNumber || !syncSample) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (!stbl->ShadowSync) {
		stbl->ShadowSync = (GF_ShadowSyncBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSH);
		if (!stbl->ShadowSync) return GF_OUT_OF_MEM;
	}

	//if no sync, skip
	if (!stbl->SyncSample) return GF_OK;
	//else set the sync shadow.
	//if the sample is sync, ignore
	e = stbl_GetSampleRAP(stbl->SyncSample, sampleNumber, &isRAP, NULL, NULL);
	if (e) return e;
	if (isRAP) return GF_OK;
	//if the shadowing sample is not sync, error
	e = stbl_GetSampleRAP(stbl->SyncSample, syncSample, &isRAP, NULL, NULL);
	if (e) return e;
	if (!isRAP) return GF_BAD_PARAM;

	return stbl_SetSyncShadow(stbl->ShadowSync, sampleNumber, syncSample);
}
#endif

//set the GroupID of a track (only used for interleaving)
GF_EXPORT
GF_Err gf_isom_set_track_interleaving_group(GF_ISOFile *movie, u32 trackNumber, u32 GroupID)
{
	GF_TrackBox *trak;

	if (movie->openMode != GF_ISOM_OPEN_EDIT) return GF_ISOM_INVALID_MODE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !GroupID) return GF_BAD_PARAM;

	trak->Media->information->sampleTable->groupID = GroupID;
	return GF_OK;
}


//set the Priority of a track within a Group (only used for tight interleaving)
//Priority ranges from 1 to 9
GF_EXPORT
GF_Err gf_isom_set_track_priority_in_group(GF_ISOFile *movie, u32 trackNumber, u32 Priority)
{
	GF_TrackBox *trak;

	if (movie->openMode != GF_ISOM_OPEN_EDIT) return GF_ISOM_INVALID_MODE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !Priority) return GF_BAD_PARAM;

	trak->Media->information->sampleTable->trackPriority = Priority > 255 ? 255 : Priority;
	return GF_OK;
}

//set the max SamplesPerChunk (for file optimization)
GF_EXPORT
GF_Err gf_isom_hint_max_chunk_size(GF_ISOFile *movie, u32 trackNumber, u32 maxChunkSize)
{
	GF_TrackBox *trak;

	if (movie->openMode == GF_ISOM_OPEN_READ) return GF_ISOM_INVALID_MODE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !maxChunkSize) return GF_BAD_PARAM;

	trak->Media->information->sampleTable->MaxChunkSize = maxChunkSize;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_set_extraction_slc(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, const GF_SLConfig *slConfig)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_Err e;
	GF_SLConfig **slc;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, StreamDescriptionIndex, &entry, NULL);
	if (e) return e;

	//we must be sure we are not using a remote ESD
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_MP4S:
		if (((GF_MPEGSampleEntryBox *)entry)->esd->desc->slConfig->predefined != SLPredef_MP4) return GF_BAD_PARAM;
		slc = & ((GF_MPEGSampleEntryBox *)entry)->slc;
		break;
	case GF_ISOM_BOX_TYPE_MP4A:
		if (((GF_MPEGAudioSampleEntryBox *)entry)->esd->desc->slConfig->predefined != SLPredef_MP4) return GF_BAD_PARAM;
		slc = & ((GF_MPEGAudioSampleEntryBox *)entry)->slc;
		break;
	case GF_ISOM_BOX_TYPE_MP4V:
		if (((GF_MPEGVisualSampleEntryBox *)entry)->esd->desc->slConfig->predefined != SLPredef_MP4) return GF_BAD_PARAM;
		slc = & ((GF_MPEGVisualSampleEntryBox *)entry)->slc;
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (*slc) {
		gf_odf_desc_del((GF_Descriptor *)*slc);
		*slc = NULL;
	}
	if (!slConfig) return GF_OK;
	//finally duplicate the SL
	return gf_odf_desc_copy((GF_Descriptor *) slConfig, (GF_Descriptor **) slc);
}

#if 0 //unused
GF_Err gf_isom_get_extraction_slc(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, GF_SLConfig **slConfig)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_Err e;
	GF_SLConfig *slc;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, StreamDescriptionIndex, &entry, NULL);
	if (e) return e;

	//we must be sure we are not using a remote ESD
	slc = NULL;
	*slConfig = NULL;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_MP4S:
		if (((GF_MPEGSampleEntryBox *)entry)->esd->desc->slConfig->predefined != SLPredef_MP4) return GF_BAD_PARAM;
		slc = ((GF_MPEGSampleEntryBox *)entry)->slc;
		break;
	case GF_ISOM_BOX_TYPE_MP4A:
		if (((GF_MPEGAudioSampleEntryBox *)entry)->esd->desc->slConfig->predefined != SLPredef_MP4) return GF_BAD_PARAM;
		slc = ((GF_MPEGAudioSampleEntryBox *)entry)->slc;
		break;
	case GF_ISOM_BOX_TYPE_MP4V:
		if (((GF_MPEGVisualSampleEntryBox *)entry)->esd->desc->slConfig->predefined != SLPredef_MP4) return GF_BAD_PARAM;
		slc = ((GF_MPEGVisualSampleEntryBox *)entry)->slc;
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (!slc) return GF_OK;
	//finally duplicate the SL
	return gf_odf_desc_copy((GF_Descriptor *) slc, (GF_Descriptor **) slConfig);
}

u32 gf_isom_get_track_group(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;
	return trak->Media->information->sampleTable->groupID;
}

u32 gf_isom_get_track_priority_in_group(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;
	return trak->Media->information->sampleTable->trackPriority;
}
#endif


GF_EXPORT
GF_Err gf_isom_make_interleave_ex(GF_ISOFile *file, GF_Fraction *fTimeInSec)
{
	GF_Err e;
	u64 itime;
	if (!file || !fTimeInSec->den || (fTimeInSec->num<=0)) return GF_BAD_PARAM;

	itime = (u64) fTimeInSec->num;
	itime *= gf_isom_get_timescale(file);
	itime /= fTimeInSec->den;
	if (file->storageMode==GF_ISOM_STORE_FASTSTART) {
		return gf_isom_set_interleave_time(file, (u32) itime);
	}
	if (gf_isom_get_mode(file) < GF_ISOM_OPEN_EDIT) return GF_BAD_PARAM;
	e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_DRIFT_INTERLEAVED);
	if (e) return e;
	return gf_isom_set_interleave_time(file, (u32) itime);
}

GF_EXPORT
GF_Err gf_isom_make_interleave(GF_ISOFile *file, Double TimeInSec)
{
	GF_Fraction f;
	f.num = (s32) (TimeInSec * 1000);
	f.den = 1000;
	return gf_isom_make_interleave_ex(file, &f);

}
GF_EXPORT
GF_Err gf_isom_set_handler_name(GF_ISOFile *the_file, u32 trackNumber, const char *nameUTF8)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (trak->Media->handler->nameUTF8) gf_free(trak->Media->handler->nameUTF8);
	trak->Media->handler->nameUTF8 = NULL;

	if (!nameUTF8) return GF_OK;

	if (!strnicmp(nameUTF8, "file://", 7)) {
		u8 BOM[4];
		FILE *f = gf_fopen(nameUTF8+7, "rb");
		u64 size;
		if (!f) return GF_URL_ERROR;
		size = gf_fsize(f);
		if (3!=gf_fread(BOM, 3, f)) {
			gf_fclose(f);
			return GF_CORRUPTED_DATA;
		}
		/*skip BOM if any*/
		if ((BOM[0]==0xEF) && (BOM[1]==0xBB) && (BOM[2]==0xBF)) size -= 3;
		else if ((BOM[0]==0xEF) || (BOM[0]==0xFF)) {
			gf_fclose(f);
			return GF_BAD_PARAM;
		}
		else gf_fseek(f, 0, SEEK_SET);
		trak->Media->handler->nameUTF8 = (char*)gf_malloc(sizeof(char)*(size_t)(size+1));
		if (!trak->Media->handler->nameUTF8) {
			gf_fclose(f);
			return GF_OUT_OF_MEM;
		}
		size = gf_fread(trak->Media->handler->nameUTF8, (size_t)size, f);
		trak->Media->handler->nameUTF8[size] = 0;
		gf_fclose(f);
	} else {
		u32 i, j, len;
		char szOrig[1024], szLine[1024];
		strcpy(szOrig, nameUTF8);
		j=0;
		len = (u32) strlen(szOrig);
		for (i=0; i<len; i++) {
			if (szOrig[i] & 0x80) {
				/*non UTF8 (likely some win-CP)*/
				if ( (szOrig[i+1] & 0xc0) != 0x80) {
					szLine[j] = 0xc0 | ( (szOrig[i] >> 6) & 0x3 );
					j++;
					szOrig[i] &= 0xbf;
				}
				/*UTF8 2 bytes char */
				else if ( (szOrig[i] & 0xe0) == 0xc0) {
					szLine[j] = szOrig[i];
					i++;
					j++;
				}
				/*UTF8 3 bytes char */
				else if ( (szOrig[i] & 0xf0) == 0xe0) {
					szLine[j] = szOrig[i];
					i++;
					j++;
					szLine[j] = szOrig[i];
					i++;
					j++;
				}
				/*UTF8 4 bytes char */
				else if ( (szOrig[i] & 0xf8) == 0xf0) {
					szLine[j] = szOrig[i];
					i++;
					j++;
					szLine[j] = szOrig[i];
					i++;
					j++;
					szLine[j] = szOrig[i];
					i++;
					j++;
				}
			}
			szLine[j] = szOrig[i];
			j++;
		}
		szLine[j] = 0;
		trak->Media->handler->nameUTF8 = gf_strdup(szLine);
	}
	return GF_OK;
}

#if 0 //unused
/*clones root OD from input to output file, without copying root OD track references*/
GF_Err gf_isom_clone_root_od(GF_ISOFile *input, GF_ISOFile *output)
{
	GF_List *esds;
	GF_Err e;
	u32 i;
	GF_Descriptor *desc;

	e = gf_isom_remove_root_od(output);
	if (e) return e;
	if (!input->moov || !input->moov->iods || !input->moov->iods->descriptor) return GF_OK;
	e = gf_isom_insert_moov(output);
	if (e) return e;
	e = AddMovieIOD(output->moov, 0);
	if (e) return e;
	if (output->moov->iods->descriptor) gf_odf_desc_del(output->moov->iods->descriptor);
	output->moov->iods->descriptor = NULL;
	gf_odf_desc_copy(input->moov->iods->descriptor, &output->moov->iods->descriptor);

	switch (output->moov->iods->descriptor->tag) {
	case GF_ODF_ISOM_IOD_TAG:
		esds = ((GF_IsomInitialObjectDescriptor *)output->moov->iods->descriptor)->ES_ID_IncDescriptors;
		break;
	case GF_ODF_ISOM_OD_TAG:
		esds = ((GF_IsomObjectDescriptor *)output->moov->iods->descriptor)->ES_ID_IncDescriptors;
		break;
	default:
		return GF_ISOM_INVALID_FILE;
	}

	//get the desc
	i=0;
	while ((desc = (GF_Descriptor*)gf_list_enum(esds, &i))) {
		gf_odf_desc_del(desc);
		gf_list_rem(esds, i-1);
	}
	return GF_OK;
}
#endif

GF_EXPORT
GF_Err gf_isom_set_media_type(GF_ISOFile *movie, u32 trackNumber, u32 new_type)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !new_type) return GF_BAD_PARAM;
	trak->Media->handler->handlerType = new_type;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_media_subtype(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescriptionIndex, u32 new_type)
{
	GF_SampleEntryBox*entry;
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !sampleDescriptionIndex || !new_type) return GF_BAD_PARAM;

	entry = (GF_SampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDescriptionIndex - 1);
	if (!entry) return GF_BAD_PARAM;
	entry->type = new_type;
	return GF_OK;
}


#if 0 //unused
GF_Err gf_isom_set_JPEG2000(GF_ISOFile *mov, Bool set_on)
{
	if (!mov) return GF_BAD_PARAM;
	mov->is_jp2 = set_on;
	return GF_OK;
}
#endif

GF_Err gf_isom_remove_uuid(GF_ISOFile *movie, u32 trackNumber, bin128 UUID)
{
	u32 i, count;
	GF_List *list;

	if (trackNumber==(u32) -1) {
		if (!movie) return GF_BAD_PARAM;
		list = movie->TopBoxes;
	} else if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		list = trak->child_boxes;
	} else {
		if (!movie) return GF_BAD_PARAM;
		list = movie->moov->child_boxes;
	}

	count = list ? gf_list_count(list) : 0;
	for (i=0; i<count; i++) {
		GF_UnknownUUIDBox *uuid = (GF_UnknownUUIDBox *)gf_list_get(list, i);
		if (uuid->type != GF_ISOM_BOX_TYPE_UUID) continue;
		if (memcmp(UUID, uuid->uuid, sizeof(bin128))) continue;
		gf_list_rem(list, i);
		i--;
		count--;
		gf_isom_box_del((GF_Box*)uuid);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_add_uuid(GF_ISOFile *movie, u32 trackNumber, bin128 UUID, const u8 *data, u32 data_size)
{
	GF_List *list;
    u32 btype;
	GF_Box *box;
	GF_UnknownUUIDBox *uuidb;

	if (data_size && !data) return GF_BAD_PARAM;
	if (trackNumber==(u32) -1) {
		if (!movie) return GF_BAD_PARAM;
		list = movie->TopBoxes;
	} else if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->child_boxes) trak->child_boxes = gf_list_new();
		list = trak->child_boxes;
	} else {
		if (!movie) return GF_BAD_PARAM;
		if (!movie->moov->child_boxes) movie->moov->child_boxes = gf_list_new();
		list = movie->moov->child_boxes;
	}
    btype = gf_isom_solve_uuid_box((char *) UUID);
    if (!btype) btype = GF_ISOM_BOX_TYPE_UUID;
    box = gf_isom_box_new(btype);
    if (!box) return GF_OUT_OF_MEM;
	uuidb = (GF_UnknownUUIDBox*)box;
	uuidb->internal_4cc = gf_isom_solve_uuid_box((char *) UUID);
	memcpy(uuidb->uuid, UUID, sizeof(bin128));
	uuidb->dataSize = data_size;
	if (data_size) {
		uuidb->data = (char*)gf_malloc(sizeof(char)*data_size);
		if (!uuidb->data) return GF_OUT_OF_MEM;
		memcpy(uuidb->data, data, sizeof(char)*data_size);
	}
	gf_list_add(list, uuidb);
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_apple_set_tag(GF_ISOFile *mov, GF_ISOiTunesTag tag, const u8 *data, u32 data_len, u64 int_val, u32 int_val2)
{
	GF_Err e;
	GF_ItemListBox *ilst;
	GF_MetaBox *meta;
	GF_ListItemBox *info;
	u32 btype, i, itype;
	s32 tag_idx;
	u32 n=0, d=0;
	u8 loc_data[10];
	u32 int_flags = 0x15;
	GF_DataBox *dbox;

	e = CanAccessMovie(mov, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	tag_idx = gf_itags_find_by_itag(tag);
	if (tag_idx<0) {
		itype = GF_ITAG_STR;
	} else {
		itype = gf_itags_get_type(tag_idx);
	}
	meta = (GF_MetaBox *) gf_isom_create_meta_extensions(mov, GF_FALSE);
	if (!meta) return GF_BAD_PARAM;

	ilst = gf_ismo_locate_box(meta->child_boxes, GF_ISOM_BOX_TYPE_ILST, NULL);
	if (!ilst) {
		ilst = (GF_ItemListBox *) gf_isom_box_new_parent(&meta->child_boxes, GF_ISOM_BOX_TYPE_ILST);
	}

	if (tag==GF_ISOM_ITUNE_RESET) {
		gf_isom_box_del_parent(&meta->child_boxes, (GF_Box *) ilst);
		//if last, delete udta - we may still have a handler box remaining
		if ((gf_list_count(meta->child_boxes) <= 1) && (gf_list_count(mov->moov->udta->recordList)==1)) {
			gf_isom_box_del_parent(&mov->moov->child_boxes, (GF_Box *) mov->moov->udta);
			mov->moov->udta = NULL;
		}
		return GF_OK;
	}

	if (tag==GF_ISOM_ITUNE_GENRE) {
		if (!int_val && data) {
			int_val = gf_id3_get_genre_tag(data);
			if (int_val) {
				data = NULL;
				data_len = 0;
				itype = GF_ITAG_INT16;
				int_flags = 0;
			}
		}
		btype = data ? GF_ISOM_ITUNE_GENRE_USER : GF_ISOM_ITUNE_GENRE;
	} else {
		btype = tag;
	}
	/*remove tag*/
	i = 0;
	while ((info = (GF_ListItemBox*)gf_list_enum(ilst->child_boxes, &i))) {
		if (info->type==btype) {
			gf_isom_box_del_parent(&ilst->child_boxes, (GF_Box *) info);
			info = NULL;
			break;
		}
		if (info->type==GF_ISOM_BOX_TYPE_UNKNOWN) {
			GF_UnknownBox *u = (GF_UnknownBox *) info;
			if (u->original_4cc==btype) {
				gf_isom_box_del_parent(&ilst->child_boxes, (GF_Box *) info);
				info = NULL;
				break;
			}
		}
	}

	if (!data && data_len) {
		if (!gf_list_count(ilst->child_boxes) )
			gf_isom_box_del_parent(&meta->child_boxes, (GF_Box *) ilst);
		return GF_OK;
	}

	info = (GF_ListItemBox *)gf_isom_box_new(btype);
	if (info == NULL) return GF_OUT_OF_MEM;

	dbox = (GF_DataBox *)gf_isom_box_new_parent(&info->child_boxes, GF_ISOM_BOX_TYPE_DATA);
	if (!dbox) {
		gf_isom_box_del((GF_Box *)info);
		return GF_OUT_OF_MEM;
	}
	if (info->type!=GF_ISOM_BOX_TYPE_UNKNOWN) {
		info->data = dbox;
	}

	switch (itype) {
	case GF_ITAG_FRAC6:
	case GF_ITAG_FRAC8:
		if (data && data_len) {
			if (sscanf(data, "%u/%u", &n, &d) != 2) {
				n = d = 0;
				if (sscanf(data, "%u", &n) != 1)
					n = 0;
			}
		} else {
			n = (u32) int_val;
			d = int_val2;
		}
		if (n) {
			memset(loc_data, 0, sizeof(char) * 8);
			data_len = (itype == GF_ITAG_FRAC6) ? 6 : 8;
			loc_data[3] = n;
			loc_data[2] = n >> 8;
			loc_data[5] = d;
			loc_data[4] = d >> 8;
			data = loc_data;
		} else {
			data = NULL;
		}
		dbox->flags = 0x15;
		break;
	case GF_ITAG_BOOL:
		loc_data[0] = 0;
		if (data && data_len) {
			if ( !strcmp(data, "yes") || !strcmp(data, "1") || !strcmp(data, "true"))
				loc_data[0] = 1;
		} else {
			loc_data[0] = int_val ? 1 : 0;
		}
		data = loc_data;
		data_len = 0;
		dbox->flags = int_flags;
		break;
	case GF_ITAG_INT16:
		loc_data[0] = 0;
		if (data && data_len) int_val = atoi(data);
		loc_data[1] = (u8) int_val;
		loc_data[0] = (u8) (int_val>>8);
		data = loc_data;
		data_len = 2;
		dbox->flags = int_flags;
		break;
	case GF_ITAG_INT32:
		loc_data[0] = 0;
		if (data && data_len) int_val = atoi(data);
		loc_data[3] = (u8) int_val;
		loc_data[2] = (u8) (int_val>>8);
		loc_data[1] = (u8) (int_val>>16);
		loc_data[0] = (u8) (int_val>>24);
		data = loc_data;
		data_len = 4;
		dbox->flags = int_flags;
		break;
	case GF_ITAG_INT64:
		loc_data[0] = 0;
		if (data && data_len) sscanf(data, LLU, &int_val);
		loc_data[7] = (u8) int_val;
		loc_data[6] = (u8) (int_val>>8);
		loc_data[5] = (u8) (int_val>>16);
		loc_data[4] = (u8) (int_val>>24);
		loc_data[3] = (u8) (int_val>>32);
		loc_data[2] = (u8) (int_val>>40);
		loc_data[1] = (u8) (int_val>>48);
		loc_data[0] = (u8) (int_val>>56);
		data = loc_data;
		data_len = 4;
		dbox->flags = int_flags;
		break;
	default:
		dbox->flags = 1;
		break;
	}

	if (!data) return GF_BAD_PARAM;


	if (tag==GF_ISOM_ITUNE_COVER_ART) {
		info->data->flags = 0;
		/*check for PNG sig*/
		if ((data_len>4) && (data[0] == 0x89) && (data[1] == 0x50) && (data[2] == 0x4E) && (data[3] == 0x47) ) {
			info->data->flags = 14;
		}
		else if ((data_len>4) && (data[0] == 0xFF) && (data[1] == 0xD8) && (data[2] == 0xFF) && (data[3] == 0xE0) ) {
			info->data->flags = 13;
		}
		else if ((data_len>3) && (data[0] == 'G') && (data[1] == 'I') && (data[2] == 'F') ) {
			info->data->flags = 12;
		}
	}

	dbox->dataSize = data_len;
	dbox->data = (char*)gf_malloc(sizeof(char)*data_len);
	if (!dbox->data) return GF_OUT_OF_MEM;
	memcpy(dbox->data, data, sizeof(char)*data_len);

	if (!info && !gf_list_count(ilst->child_boxes) ) {
		gf_isom_box_del_parent(&meta->child_boxes, (GF_Box *) ilst);
		return GF_OK;
	}
	if (!ilst->child_boxes) ilst->child_boxes = gf_list_new();
	
	return gf_list_add(ilst->child_boxes, info);
}

#include <gpac/utf.h>

GF_EXPORT
GF_Err gf_isom_wma_set_tag(GF_ISOFile *mov, char *name, char *value)
{
	GF_Err e;
	GF_XtraTag *tag=NULL;
	u32 count, i;
	GF_XtraBox *xtra;

	e = CanAccessMovie(mov, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	gf_isom_create_meta_extensions(mov, GF_FALSE);

	xtra = (GF_XtraBox *) gf_isom_create_meta_extensions(mov, GF_TRUE);
	if (!xtra) return GF_BAD_PARAM;

	count = gf_list_count(xtra->tags);
	for (i=0; i<count; i++) {
		tag = gf_list_get(xtra->tags, i);
		if (name && tag->name && !strcmp(tag->name, name)) {

		} else {
			tag = NULL;
			continue;
		}

		if (!value) {
			gf_list_rem(xtra->tags, i);
			gf_free(tag->name);
			if (tag->prop_value) gf_free(tag->prop_value);
			gf_free(tag);
			return GF_OK;
		}
		gf_free(tag->prop_value);
		tag->prop_value = 0;
	}
	if (!tag) {
		if (!name) return GF_OK;

		GF_SAFEALLOC(tag, GF_XtraTag);
		tag->name = gf_strdup(name);
		tag->prop_type = 0;
		tag->flags = 1;
		gf_list_add(xtra->tags, tag);
	}

	u32 len = (u32) strlen(value);
	tag->prop_value = gf_malloc(sizeof(u16) * (len+1) );
	memset(tag->prop_value, 0, sizeof(u16) * (len+1) );
	if (len) {
		u32 _len = (u32) gf_utf8_mbstowcs((u16 *) tag->prop_value, len, (const char **) &value);
		if (_len != (u32) -1) {
			tag->prop_value[2 * _len] = 0;
			tag->prop_value[2 * _len + 1] = 0;
		}
		tag->prop_size = 2 * _len + 2;
	} else {
		tag->prop_size = 2;
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_set_alternate_group_id(GF_ISOFile *movie, u32 trackNumber, u32 groupId)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	trak->Header->alternate_group = groupId;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_set_track_switch_parameter(GF_ISOFile *movie, u32 trackNumber, u32 trackRefGroup, Bool is_switch_group, u32 *switchGroupID, u32 *criteriaList, u32 criteriaListCount)
{
	GF_TrackSelectionBox *tsel;
	GF_TrackBox *trak;
	GF_UserDataMap *map;
	GF_Err e;
	u32 alternateGroupID = 0;
	u32 next_switch_group_id = 0;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !switchGroupID) return GF_BAD_PARAM;


	if (trackRefGroup) {
		GF_TrackBox *trak_ref = gf_isom_get_track_from_file(movie, trackRefGroup);
		if (trak_ref != trak) {
			if (!trak_ref || !trak_ref->Header->alternate_group) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Track %d has not an alternate group - skipping\n", trak_ref ? trak_ref->Header->trackID : 0));
				return GF_BAD_PARAM;
			}
			alternateGroupID = trak_ref->Header->alternate_group;
		} else {
			alternateGroupID = trak->Header->alternate_group;
		}
	}
	if (!alternateGroupID) {
		/*there is a function for this ....*/
		if (trak->Header->alternate_group) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("Track %d has already an alternate group - skipping\n", trak->Header->trackID));
			return GF_BAD_PARAM;
		}
		alternateGroupID = gf_isom_get_next_alternate_group_id(movie);
	}

	if (is_switch_group) {
		u32 i=0;
		while (i< gf_isom_get_track_count(movie) ) {
			//locate first available ID
			GF_TrackBox *a_trak = gf_isom_get_track_from_file(movie, i+1);

			if (a_trak->udta) {
				u32 j, count;
				map = udta_getEntry(a_trak->udta, GF_ISOM_BOX_TYPE_TSEL, NULL);
				if (map) {
					count = gf_list_count(map->boxes);
					for (j=0; j<count; j++) {
						tsel = (GF_TrackSelectionBox*)gf_list_get(map->boxes, j);

						if (*switchGroupID) {
							if (tsel->switchGroup==next_switch_group_id) {
								if (a_trak->Header->alternate_group != alternateGroupID) return GF_BAD_PARAM;
							}
						} else {
							if (tsel->switchGroup && (tsel->switchGroup>=next_switch_group_id) )
								next_switch_group_id = tsel->switchGroup;
						}
					}
				}

			}
			i++;
		}
		if (! *switchGroupID) *switchGroupID = next_switch_group_id+1;
	}


	trak->Header->alternate_group = alternateGroupID;

	tsel = NULL;
	if (*switchGroupID) {
		if (!trak->udta) {
			e = trak_on_child_box((GF_Box*)trak, gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
			if (e) return e;
		}

		map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_TSEL, NULL);

		/*locate tsel box with no switch group*/
		if (map)  {
			u32 j, count = gf_list_count(map->boxes);
			for (j=0; j<count; j++) {
				tsel = (GF_TrackSelectionBox*)gf_list_get(map->boxes, j);
				if (tsel->switchGroup == *switchGroupID) break;
				tsel = NULL;
			}
		}
		if (!tsel) {
			tsel = (GF_TrackSelectionBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_TSEL);
			if (!tsel) return GF_OUT_OF_MEM;
			e = udta_on_child_box((GF_Box *)trak->udta, (GF_Box *) tsel, GF_FALSE);
			if (e) return e;
		}

		tsel->switchGroup = *switchGroupID;
		tsel->attributeListCount = criteriaListCount;
		if (tsel->attributeList) gf_free(tsel->attributeList);
		tsel->attributeList = (u32*)gf_malloc(sizeof(u32)*criteriaListCount);
		if (!tsel->attributeList) return GF_OUT_OF_MEM;
		memcpy(tsel->attributeList, criteriaList, sizeof(u32)*criteriaListCount);
	}
	return GF_OK;
}

void reset_tsel_box(GF_TrackBox *trak)
{
	GF_UserDataMap *map;
	trak->Header->alternate_group = 0;
	map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_TSEL, NULL);
	if (map) {
		gf_list_del_item(trak->udta->recordList, map);
		gf_isom_box_array_del(map->boxes);
		gf_free(map);
	}

}

GF_EXPORT
GF_Err gf_isom_reset_track_switch_parameter(GF_ISOFile *movie, u32 trackNumber, Bool reset_all_group)
{
	GF_TrackBox *trak;
	u32 alternateGroupID = 0;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Header->alternate_group) return GF_OK;

	alternateGroupID = trak->Header->alternate_group;
	if (reset_all_group) {
		u32 i=0;
		while (i< gf_isom_get_track_count(movie) ) {
			//locate first available ID
			GF_TrackBox *a_trak = gf_isom_get_track_from_file(movie, i+1);
			if (a_trak->Header->alternate_group == alternateGroupID) reset_tsel_box(a_trak);
			i++;
		}
	} else {
		reset_tsel_box(trak);
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_reset_switch_parameters(GF_ISOFile *movie)
{
	u32 i=0;
	while (i< gf_isom_get_track_count(movie) ) {
		//locate first available ID
		GF_TrackBox *a_trak = gf_isom_get_track_from_file(movie, i+1);
		reset_tsel_box(a_trak);
		i++;
	}
	return GF_OK;
}


GF_Err gf_isom_add_subsample(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 flags, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable)
{
	u32 i, count;
	GF_SubSampleInformationBox *sub_samples;
	GF_TrackBox *trak;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, track);
	if (!trak || !trak->Media || !trak->Media->information->sampleTable)
		return GF_BAD_PARAM;

	if (!trak->Media->information->sampleTable->sub_samples) {
		trak->Media->information->sampleTable->sub_samples=gf_list_new();
	}

	sub_samples = NULL;
	count = gf_list_count(trak->Media->information->sampleTable->sub_samples);
	for (i=0; i<count; i++) {
		sub_samples = gf_list_get(trak->Media->information->sampleTable->sub_samples, i);
		if (sub_samples->flags==flags) break;
		sub_samples = NULL;
	}
	if (!sub_samples) {
		sub_samples = (GF_SubSampleInformationBox *) gf_isom_box_new_parent(&trak->Media->information->sampleTable->child_boxes, GF_ISOM_BOX_TYPE_SUBS);
		if (!sub_samples) return GF_OUT_OF_MEM;
		gf_list_add(trak->Media->information->sampleTable->sub_samples, sub_samples);
		sub_samples->version = (subSampleSize>0xFFFF) ? 1 : 0;
		sub_samples->flags = flags;
	}
	return gf_isom_add_subsample_info(sub_samples, sampleNumber, subSampleSize, priority, reserved, discardable);
}


GF_EXPORT
GF_Err gf_isom_set_rvc_config(GF_ISOFile *movie, u32 track, u32 sampleDescriptionIndex, u16 rvc_predefined, char *mime, u8 *data, u32 size)
{
	GF_MPEGVisualSampleEntryBox *entry;
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, track);
	if (!trak) return GF_BAD_PARAM;


	entry = (GF_MPEGVisualSampleEntryBox *) gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDescriptionIndex-1);
	if (!entry ) return GF_BAD_PARAM;
	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_BAD_PARAM;

	GF_RVCConfigurationBox *rvcc = (GF_RVCConfigurationBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_RVCC);
	if (rvcc && rvcc->rvc_meta_idx) {
		gf_isom_remove_meta_item(movie, GF_FALSE, track, rvcc->rvc_meta_idx);
		rvcc->rvc_meta_idx = 0;
	}

	if (!rvcc) {
		rvcc = (GF_RVCConfigurationBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_RVCC);
		if (!rvcc) return GF_OUT_OF_MEM;
	}
	rvcc->predefined_rvc_config = rvc_predefined;
	if (!rvc_predefined) {
		u32 it_id=0;
		e = gf_isom_set_meta_type(movie, GF_FALSE, track, GF_META_TYPE_RVCI);
		if (e) return e;
		gf_isom_modify_alternate_brand(movie, GF_ISOM_BRAND_ISO2, GF_TRUE);
		e = gf_isom_add_meta_item_memory(movie, GF_FALSE, track, "rvcconfig.xml", &it_id, GF_META_ITEM_TYPE_MIME, mime, NULL, NULL, data, size, NULL);
		if (e) return e;
		rvcc->rvc_meta_idx = gf_isom_get_meta_item_count(movie, GF_FALSE, track);
	}
	return GF_OK;
}

/*for now not exported*/
/*expands sampleGroup table for the given grouping type and sample_number. If sample_number is 0, just appends an entry at the end of the table*/
static GF_Err gf_isom_add_sample_group_entry(GF_List *sampleGroups, u32 sample_number, u32 grouping_type, u32 grouping_type_parameter, u32 sampleGroupDescriptionIndex, GF_List *parent, GF_SampleTableBox *stbl)
{
	GF_SampleGroupBox *sgroup = NULL;
	u32 i, count, last_sample_in_entry;
	Bool all_samples = GF_FALSE;
	assert(sampleGroups);
	count = gf_list_count(sampleGroups);
	for (i=0; i<count; i++) {
		sgroup = (GF_SampleGroupBox*)gf_list_get(sampleGroups, i);
		if (sgroup->grouping_type==grouping_type) break;
		sgroup = NULL;
	}
	if (!sgroup) {
		sgroup = (GF_SampleGroupBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SBGP);
		if (!sgroup) return GF_OUT_OF_MEM;
		sgroup->grouping_type = grouping_type;
		sgroup->grouping_type_parameter = grouping_type_parameter;
//		gf_list_add(sampleGroups, sgroup);
		//crude patch to align old arch and filters
		gf_list_insert(sampleGroups, sgroup, 0);
		assert(parent);
		gf_list_add(parent, sgroup);
	}
	/*used in fragments, means we are adding the last sample*/
	if (!sample_number) {
		sample_number = 1;
		if (sgroup->entry_count) {
			for (i=0; i<sgroup->entry_count; i++) {
				sample_number += sgroup->sample_entries[i].sample_count;
			}
		}
	} else if (sample_number==(u32) -1) {
		all_samples = GF_TRUE;
		sample_number = 1;
	}

	if (!sgroup->entry_count) {
		u32 idx = 0;
		sgroup->entry_count = (sample_number>1) ? 2 : 1;
		sgroup->sample_entries = (GF_SampleGroupEntry*)gf_malloc(sizeof(GF_SampleGroupEntry) * sgroup->entry_count );
		if (!sgroup->sample_entries) return GF_OUT_OF_MEM;
		if (sample_number>1) {
			sgroup->sample_entries[0].sample_count = sample_number-1;
			sgroup->sample_entries[0].group_description_index = sampleGroupDescriptionIndex ? 0 : 1;
			idx = 1;
		}
		sgroup->sample_entries[idx].sample_count = 1;
		sgroup->sample_entries[idx].group_description_index = sampleGroupDescriptionIndex;
		if (all_samples && stbl) {
			sgroup->sample_entries[idx].sample_count = stbl->SampleSize->sampleCount;
		}
		return GF_OK;
	}
	if (all_samples && stbl) {
		sgroup->entry_count = 1;
		sgroup->sample_entries[0].group_description_index = sampleGroupDescriptionIndex;
		sgroup->sample_entries[0].sample_count = stbl->SampleSize->sampleCount;
		return GF_OK;
	}
	last_sample_in_entry = 0;
	for (i=0; i<sgroup->entry_count; i++) {
		/*TODO*/
		if (last_sample_in_entry + sgroup->sample_entries[i].sample_count > sample_number) return GF_NOT_SUPPORTED;
		last_sample_in_entry += sgroup->sample_entries[i].sample_count;
	}

	if (last_sample_in_entry == sample_number) {
		if (sgroup->sample_entries[sgroup->entry_count-1].group_description_index==sampleGroupDescriptionIndex)
			return GF_OK;
		else
			return GF_NOT_SUPPORTED;
	}

	if ((sgroup->sample_entries[sgroup->entry_count-1].group_description_index==sampleGroupDescriptionIndex) && (last_sample_in_entry+1==sample_number)) {
		sgroup->sample_entries[sgroup->entry_count-1].sample_count++;
		return GF_OK;
	}
	/*last entry was an empty desc (no group associated), just add the number of samples missing until new one, then add new one*/
	if (! sgroup->sample_entries[sgroup->entry_count-1].group_description_index) {
		sgroup->sample_entries[sgroup->entry_count-1].sample_count += sample_number - 1 - last_sample_in_entry;
		sgroup->sample_entries = (GF_SampleGroupEntry*)gf_realloc(sgroup->sample_entries, sizeof(GF_SampleGroupEntry) * (sgroup->entry_count + 1) );
		sgroup->sample_entries[sgroup->entry_count].sample_count = 1;
		sgroup->sample_entries[sgroup->entry_count].group_description_index = sampleGroupDescriptionIndex;
		sgroup->entry_count++;
		return GF_OK;
	}
	/*we are adding a sample with no desc, add entry at the end*/
	if (!sampleGroupDescriptionIndex || (sample_number - 1 - last_sample_in_entry==0) ) {
		sgroup->sample_entries = (GF_SampleGroupEntry*)gf_realloc(sgroup->sample_entries, sizeof(GF_SampleGroupEntry) * (sgroup->entry_count + 1) );
		sgroup->sample_entries[sgroup->entry_count].sample_count = 1;
		sgroup->sample_entries[sgroup->entry_count].group_description_index = sampleGroupDescriptionIndex;
		sgroup->entry_count++;
		return GF_OK;
	}
	/*need to insert two entries ...*/
	sgroup->sample_entries = (GF_SampleGroupEntry*)gf_realloc(sgroup->sample_entries, sizeof(GF_SampleGroupEntry) * (sgroup->entry_count + 2) );

	sgroup->sample_entries[sgroup->entry_count].sample_count = sample_number - 1 - last_sample_in_entry;
	sgroup->sample_entries[sgroup->entry_count].group_description_index = 0;

	sgroup->sample_entries[sgroup->entry_count+1].sample_count = 1;
	sgroup->sample_entries[sgroup->entry_count+1].group_description_index = sampleGroupDescriptionIndex;
	sgroup->entry_count+=2;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
static GF_SampleGroupDescriptionBox *get_sgdp(GF_SampleTableBox *stbl, GF_TrackFragmentBox *traf, u32 grouping_type, Bool *is_traf_sgdp)
#else
static GF_SampleGroupDescriptionBox *get_sgdp(GF_SampleTableBox *stbl, void *traf, u32 grouping_type, Bool *is_traf_sgdp)
#endif /* GPAC_DISABLE_ISOM_FRAGMENTS */
{
	GF_List *groupList=NULL;
	GF_List **parent=NULL;
	GF_SampleGroupDescriptionBox *sgdesc=NULL;
	u32 count, i;

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (!stbl && traf && traf->trex->track->Media->information->sampleTable)
		stbl = traf->trex->track->Media->information->sampleTable;
#endif
	if (stbl) {
		if (!stbl->sampleGroupsDescription && !traf)
			stbl->sampleGroupsDescription = gf_list_new();

		groupList = stbl->sampleGroupsDescription;
		if (is_traf_sgdp) *is_traf_sgdp = GF_FALSE;
		parent = &stbl->child_boxes;

		count = gf_list_count(groupList);
		for (i=0; i<count; i++) {
			sgdesc = (GF_SampleGroupDescriptionBox*)gf_list_get(groupList, i);
			if (sgdesc->grouping_type==grouping_type) break;
			sgdesc = NULL;
		}
	}
	
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	/*look in stbl or traf for sample sampleGroupsDescription*/
	if (!sgdesc && traf) {
		if (!traf->sampleGroupsDescription)
			traf->sampleGroupsDescription = gf_list_new();
		groupList = traf->sampleGroupsDescription;
		parent = &traf->child_boxes;
		if (is_traf_sgdp) *is_traf_sgdp = GF_TRUE;

		count = gf_list_count(groupList);
		for (i=0; i<count; i++) {
			sgdesc = (GF_SampleGroupDescriptionBox*)gf_list_get(groupList, i);
			if (sgdesc->grouping_type==grouping_type) break;
			sgdesc = NULL;
		}
	}
#endif

	if (!sgdesc) {
		sgdesc = (GF_SampleGroupDescriptionBox *) gf_isom_box_new_parent(parent, GF_ISOM_BOX_TYPE_SGPD);
		if (!sgdesc) return NULL;
		sgdesc->grouping_type = grouping_type;
		assert(groupList);
		gf_list_add(groupList, sgdesc);
	}
	return sgdesc;
}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
static GF_Err gf_isom_set_sample_group_info_ex(GF_SampleTableBox *stbl, GF_TrackFragmentBox *traf, u32 sample_number, u32 grouping_type, u32 grouping_type_parameter, void *udta, void *(*sg_create_entry)(void *udta), Bool (*sg_compare_entry)(void *udta, void *entry))
#else
static GF_Err gf_isom_set_sample_group_info_ex(GF_SampleTableBox *stbl, void *traf, u32 sample_number, u32 grouping_type, u32 grouping_type_parameter, void *udta, void *(*sg_create_entry)(void *udta), Bool (*sg_compare_entry)(void *udta, void *entry))
#endif /* GPAC_DISABLE_ISOM_FRAGMENTS */
{
	GF_List *groupList, *parent;
	void *entry;
	Bool is_traf_sgpd;
	GF_SampleGroupDescriptionBox *sgdesc = NULL;
	u32 i, entry_idx;

	if (!stbl && !traf) return GF_BAD_PARAM;

	sgdesc = get_sgdp(stbl, traf, grouping_type, &is_traf_sgpd);
	if (!sgdesc) return GF_OUT_OF_MEM;

	entry = NULL;
	if (sg_compare_entry) {
		for (i=0; i<gf_list_count(sgdesc->group_descriptions); i++) {
			entry = gf_list_get(sgdesc->group_descriptions, i);
			if (sg_compare_entry(udta, entry)) break;
			entry = NULL;
		}
	}
	if (!entry && sg_create_entry) {
		entry = sg_create_entry(udta);
		if (!entry) return GF_IO_ERR;
		if (traf && !is_traf_sgpd) {
			sgdesc = get_sgdp(NULL, traf, grouping_type, &is_traf_sgpd);
		}
		gf_list_add(sgdesc->group_descriptions, entry);
	}
	if (!entry)
		entry_idx = 0;
	else
		entry_idx = 1 + gf_list_find(sgdesc->group_descriptions, entry);

	/*look in stbl or traf for sample sampleGroups*/
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (traf) {
		if (!traf->sampleGroups)
			traf->sampleGroups = gf_list_new();
		groupList = traf->sampleGroups;
		parent = traf->child_boxes;
		if (entry_idx && is_traf_sgpd)
			entry_idx |= 0x10000;
	} else
#endif
	{
		if (!stbl->sampleGroups)
			stbl->sampleGroups = gf_list_new();
		groupList = stbl->sampleGroups;
		parent = stbl->child_boxes;
	}

	return gf_isom_add_sample_group_entry(groupList, sample_number, grouping_type, grouping_type_parameter, entry_idx, parent, stbl);
}

/*for now not exported*/
static GF_Err gf_isom_set_sample_group_info(GF_ISOFile *movie, u32 track, u32 trafID, u32 sample_number, u32 grouping_type, u32 grouping_type_parameter, void *udta, void *(*sg_create_entry)(void *udta), Bool (*sg_compare_entry)(void *udta, void *entry))
{
	GF_Err e;
	GF_TrackBox *trak=NULL;
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	GF_TrackFragmentBox *traf=NULL;
#endif
	if (!trafID && (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY)) {
		trak = gf_isom_get_track_from_file(movie, track);
		if (!trak) return GF_BAD_PARAM;
		trafID = trak->Header->trackID;
	}

	if (trafID) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		if (!movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) )
			return GF_BAD_PARAM;

		traf = gf_isom_get_traf(movie, trafID);
#else
		return GF_NOT_SUPPORTED;
#endif

	} else if (track) {
		e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
		if (e) return e;

		trak = gf_isom_get_track_from_file(movie, track);
		if (!trak) return GF_BAD_PARAM;
	}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	return gf_isom_set_sample_group_info_ex(trak ? trak->Media->information->sampleTable : NULL, traf, sample_number, grouping_type, grouping_type_parameter, udta, sg_create_entry, sg_compare_entry);
#else
	return gf_isom_set_sample_group_info_ex(trak->Media->information->sampleTable, sample_number, grouping_type, grouping_type_parameter, udta, sg_create_entry, sg_compare_entry);
#endif

}


GF_EXPORT
GF_Err gf_isom_add_sample_group_info(GF_ISOFile *movie, u32 track, u32 grouping_type, void *data, u32 data_size, Bool is_default, u32 *sampleGroupDescriptionIndex)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_DefaultSampleGroupDescriptionEntry *entry=NULL;
	GF_SampleGroupDescriptionBox *sgdesc = NULL;

	if (sampleGroupDescriptionIndex) *sampleGroupDescriptionIndex = 0;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, track);
	if (!trak) return GF_BAD_PARAM;


	sgdesc = get_sgdp(trak->Media->information->sampleTable, NULL, grouping_type, NULL);
	if (!sgdesc) return GF_OUT_OF_MEM;

	if (grouping_type==GF_ISOM_SAMPLE_GROUP_OINF) {
		GF_OperatingPointsInformation *ptr = gf_isom_oinf_new_entry();
		GF_BitStream *bs=gf_bs_new(data, data_size, GF_BITSTREAM_READ);
		e = gf_isom_oinf_read_entry(ptr, bs);
		gf_bs_del(bs);
		if (e) {
			gf_isom_oinf_del_entry(ptr);
			return e;
		}
		e = gf_list_add(sgdesc->group_descriptions, ptr);
		if (e) return e;
		entry = (GF_DefaultSampleGroupDescriptionEntry *) ptr;
	} else if (grouping_type==GF_ISOM_SAMPLE_GROUP_LINF) {
		GF_LHVCLayerInformation *ptr = gf_isom_linf_new_entry();
		GF_BitStream *bs=gf_bs_new(data, data_size, GF_BITSTREAM_READ);
		e = gf_isom_linf_read_entry(ptr, bs);
		gf_bs_del(bs);
		if (e) {
			gf_isom_linf_del_entry(ptr);
			return e;
		}
		e = gf_list_add(sgdesc->group_descriptions, ptr);
		if (e) return e;
		entry = (GF_DefaultSampleGroupDescriptionEntry *) ptr;
	} else {
		u32 i, count=gf_list_count(sgdesc->group_descriptions);
		for (i=0; i<count; i++) {
			GF_DefaultSampleGroupDescriptionEntry *ent = gf_list_get(sgdesc->group_descriptions, i);
			if ((ent->length == data_size) && !memcmp(ent->data, data, data_size)) {
				entry = ent;
				break;
			}
			entry=NULL;
		}
		if (!entry) {
			GF_SAFEALLOC(entry, GF_DefaultSampleGroupDescriptionEntry);
			if (!entry) return GF_OUT_OF_MEM;
			entry->data = (u8*)gf_malloc(sizeof(char) * data_size);
			if (!entry->data) {
				gf_free(entry);
				return GF_OUT_OF_MEM;
			}
			entry->length = data_size;
			memcpy(entry->data, data, sizeof(char) * data_size);
			e = gf_list_add(sgdesc->group_descriptions, entry);
			if (e) {
				gf_free(entry->data);
				gf_free(entry);
				return e;
			}
		}
	}


	if (is_default) {
		sgdesc->default_description_index = 1 + gf_list_find(sgdesc->group_descriptions, entry);
		sgdesc->version = 2;
	}
	if (sampleGroupDescriptionIndex) *sampleGroupDescriptionIndex = 1 + gf_list_find(sgdesc->group_descriptions, entry);

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_remove_sample_group(GF_ISOFile *movie, u32 track, u32 grouping_type)
{
	GF_Err e;
	GF_TrackBox *trak;
	u32 count, i;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, track);
	if (!trak) return GF_BAD_PARAM;

	if (trak->Media->information->sampleTable->sampleGroupsDescription) {
		count = gf_list_count(trak->Media->information->sampleTable->sampleGroupsDescription);
		for (i=0; i<count; i++) {
			GF_SampleGroupDescriptionBox *sgdesc = (GF_SampleGroupDescriptionBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroupsDescription, i);
			if (sgdesc->grouping_type==grouping_type) {
				gf_isom_box_del_parent(&trak->Media->information->sampleTable->child_boxes, (GF_Box*)sgdesc);
				gf_list_rem(trak->Media->information->sampleTable->sampleGroupsDescription, i);
				i--;
				count--;
			}
		}
	}
	if (trak->Media->information->sampleTable->sampleGroups) {
		count = gf_list_count(trak->Media->information->sampleTable->sampleGroups);
		for (i=0; i<count; i++) {
			GF_SampleGroupBox *sgroup = gf_list_get(trak->Media->information->sampleTable->sampleGroups, i);
			if (sgroup->grouping_type==grouping_type) {
				gf_isom_box_del_parent(&trak->Media->information->sampleTable->child_boxes, (GF_Box*)sgroup);
				gf_list_rem(trak->Media->information->sampleTable->sampleGroups, i);
				i--;
				count--;
			}
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_add_sample_info(GF_ISOFile *movie, u32 track, u32 sample_number, u32 grouping_type, u32 sampleGroupDescriptionIndex, u32 grouping_type_parameter)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_List *groupList;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, track);
	if (!trak) return GF_BAD_PARAM;

	if (!trak->Media->information->sampleTable->sampleGroups)
		trak->Media->information->sampleTable->sampleGroups = gf_list_new();

	groupList = trak->Media->information->sampleTable->sampleGroups;
	return gf_isom_add_sample_group_entry(groupList, sample_number, grouping_type, grouping_type_parameter, sampleGroupDescriptionIndex, trak->Media->information->sampleTable->child_boxes, trak->Media->information->sampleTable);
}

void *sg_rap_create_entry(void *udta)
{
	GF_VisualRandomAccessEntry *entry;
	u32 *num_leading_samples = (u32 *) udta;
	assert(udta);
	GF_SAFEALLOC(entry, GF_VisualRandomAccessEntry);
	if (!entry) return NULL;
	entry->num_leading_samples = *num_leading_samples;
	entry->num_leading_samples_known = entry->num_leading_samples ? 1 : 0;
	return entry;
}

Bool sg_rap_compare_entry(void *udta, void *entry)
{
	u32 *num_leading_samples = (u32 *) udta;
	if (*num_leading_samples == ((GF_VisualRandomAccessEntry *)entry)->num_leading_samples) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_isom_set_sample_rap_group(GF_ISOFile *movie, u32 track, u32 sample_number, Bool is_rap, u32 num_leading_samples)
{
	return gf_isom_set_sample_group_info(movie, track, 0, sample_number, GF_ISOM_SAMPLE_GROUP_RAP, 0, &num_leading_samples, is_rap ? sg_rap_create_entry : NULL, is_rap ? sg_rap_compare_entry : NULL);
}

GF_Err gf_isom_fragment_set_sample_rap_group(GF_ISOFile *movie, GF_ISOTrackID trackID, u32 sample_number_in_frag, Bool is_rap, u32 num_leading_samples)
{
	return gf_isom_set_sample_group_info(movie, 0, trackID, sample_number_in_frag, GF_ISOM_SAMPLE_GROUP_RAP, 0, &num_leading_samples, is_rap ? sg_rap_create_entry : NULL, is_rap ? sg_rap_compare_entry : NULL);
}



void *sg_roll_create_entry(void *udta)
{
	GF_RollRecoveryEntry *entry;
	s16 *roll_distance = (s16 *) udta;
	GF_SAFEALLOC(entry, GF_RollRecoveryEntry);
	if (!entry) return NULL;
	entry->roll_distance = *roll_distance;
	return entry;
}

Bool sg_roll_compare_entry(void *udta, void *entry)
{
	s16 *roll_distance = (s16 *) udta;
	if (*roll_distance == ((GF_RollRecoveryEntry *)entry)->roll_distance) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_isom_set_sample_roll_group(GF_ISOFile *movie, u32 track, u32 sample_number, GF_ISOSampleRollType roll_type, s16 roll_distance)
{
	u32 grp_type = (roll_type>=GF_ISOM_SAMPLE_PREROLL) ? GF_ISOM_SAMPLE_GROUP_PROL : GF_ISOM_SAMPLE_GROUP_ROLL;
	if (roll_type==GF_ISOM_SAMPLE_PREROLL_NONE)
		roll_type = 0;

	return gf_isom_set_sample_group_info(movie, track, 0, sample_number, grp_type, 0, &roll_distance, roll_type ? sg_roll_create_entry : NULL, roll_type ? sg_roll_compare_entry : NULL);
}

GF_EXPORT
GF_Err gf_isom_fragment_set_sample_roll_group(GF_ISOFile *movie, GF_ISOTrackID trackID, u32 sample_number_in_frag, GF_ISOSampleRollType roll_type, s16 roll_distance)
{
	u32 grp_type = (roll_type>=GF_ISOM_SAMPLE_PREROLL) ? GF_ISOM_SAMPLE_GROUP_PROL : GF_ISOM_SAMPLE_GROUP_ROLL;
	if (roll_type==GF_ISOM_SAMPLE_PREROLL_NONE)
		roll_type = 0;

	return gf_isom_set_sample_group_info(movie, 0, trackID, sample_number_in_frag, grp_type, 0, &roll_distance, roll_type ? sg_roll_create_entry : NULL, roll_type ? sg_roll_compare_entry : NULL);
}


void *sg_encryption_create_entry(void *udta)
{
	GF_CENCSampleEncryptionGroupEntry *entry, *from_entry;
	GF_SAFEALLOC(entry, GF_CENCSampleEncryptionGroupEntry);
	if (!entry) return NULL;
	from_entry = (GF_CENCSampleEncryptionGroupEntry *)udta;
	memcpy(entry, from_entry, sizeof(GF_CENCSampleEncryptionGroupEntry) );
	entry->key_info = gf_malloc(sizeof(u8) * entry->key_info_size);
	if (!entry->key_info) {
		gf_free(entry);
		return NULL;
	}
	memcpy(entry->key_info, from_entry->key_info, entry->key_info_size);
	return entry;
}

Bool sg_encryption_compare_entry(void *udta, void *_entry)
{
	GF_CENCSampleEncryptionGroupEntry *entry = (GF_CENCSampleEncryptionGroupEntry *)_entry;
	GF_CENCSampleEncryptionGroupEntry *with_entry = (GF_CENCSampleEncryptionGroupEntry *)udta;

	if (entry->IsProtected != with_entry->IsProtected) return GF_FALSE;
	if (entry->skip_byte_block != with_entry->skip_byte_block) return GF_FALSE;
	if (entry->crypt_byte_block != with_entry->crypt_byte_block) return GF_FALSE;
	if (entry->key_info_size != with_entry->key_info_size) return GF_FALSE;

	if (!memcmp(entry->key_info, with_entry->key_info, with_entry->key_info_size))
		return GF_TRUE;
	return GF_FALSE;
}


/*sample encryption information group can be in stbl or traf*/
GF_EXPORT
GF_Err gf_isom_set_sample_cenc_group(GF_ISOFile *movie, u32 track, u32 sample_number, u8 isEncrypted, u8 crypt_byte_block, u8 skip_byte_block, u8 *key_info, u32 key_info_size)
{
	GF_CENCSampleEncryptionGroupEntry entry;
	if (!key_info || (key_info_size<19))
		return GF_BAD_PARAM;

	memset(&entry, 0, sizeof(GF_CENCSampleEncryptionGroupEntry));
	entry.crypt_byte_block = crypt_byte_block;
	entry.skip_byte_block = skip_byte_block;
	entry.IsProtected = isEncrypted;
	entry.key_info = key_info;
	entry.key_info_size = key_info_size;

	return gf_isom_set_sample_group_info(movie, track, 0, sample_number, GF_ISOM_SAMPLE_GROUP_SEIG, 0, &entry, sg_encryption_create_entry, sg_encryption_compare_entry);
}



GF_EXPORT
GF_Err gf_isom_set_sample_cenc_default_group(GF_ISOFile *movie, u32 track, u32 sample_number)
{
	return gf_isom_set_sample_group_info(movie, track, 0, sample_number, GF_ISOM_SAMPLE_GROUP_SEIG, 0, NULL, NULL, NULL);
}

GF_Err gf_isom_force_ctts(GF_ISOFile *file, u32 track)
{
	GF_TrackBox *trak;
	GF_Err e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
 	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;
	if (trak->Media->information->sampleTable->CompositionOffset) return GF_OK;

	trak->Media->information->sampleTable->CompositionOffset = (GF_CompositionOffsetBox *) gf_isom_box_new_parent(&trak->Media->information->sampleTable->child_boxes, GF_ISOM_BOX_TYPE_CTTS);
	if (!trak->Media->information->sampleTable->CompositionOffset) return GF_OUT_OF_MEM;
	trak->Media->information->sampleTable->CompositionOffset->nb_entries = 1;
	trak->Media->information->sampleTable->CompositionOffset->entries = gf_malloc(sizeof(GF_DttsEntry));
	trak->Media->information->sampleTable->CompositionOffset->entries[0].decodingOffset = 0;
	trak->Media->information->sampleTable->CompositionOffset->entries[0].sampleCount = 	trak->Media->information->sampleTable->SampleSize->sampleCount;
	return GF_OK;
}

GF_Err gf_isom_set_ctts_v1(GF_ISOFile *file, u32 track, u32 ctts_shift)
{
	u32 i, shift;
	u64 duration;
	GF_CompositionOffsetBox *ctts;
	GF_CompositionToDecodeBox *cslg;
	s32 leastCTTS, greatestCTTS;
	GF_TrackBox *trak;
	GF_Err e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

 	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;

	ctts = trak->Media->information->sampleTable->CompositionOffset;
	shift = ctts->version ? ctts_shift : ctts->entries[0].decodingOffset;
	leastCTTS = GF_INT_MAX;
	greatestCTTS = 0;
	for (i=0; i<ctts->nb_entries; i++) {
		if (!ctts->version)
			ctts->entries[i].decodingOffset -= shift;

		if ((s32)ctts->entries[i].decodingOffset < leastCTTS)
			leastCTTS = ctts->entries[i].decodingOffset;
		if ((s32)ctts->entries[i].decodingOffset > greatestCTTS)
			greatestCTTS = ctts->entries[i].decodingOffset;
	}
	if (!ctts->version) {
		ctts->version = 1;
		//if we had edit lists, shift all media times by the given amount
		if (trak->editBox && trak->editBox->editList) {
			for (i=0; i<gf_list_count(trak->editBox->editList->entryList); i++) {
				GF_EdtsEntry *ent = (GF_EdtsEntry*)gf_list_get(trak->editBox->editList->entryList, i);
				//empty edit
				if (ent->mediaTime<0) continue;
				if (ent->mediaTime>=shift) ent->mediaTime -= shift;
				else ent->mediaTime = 0;
				//no offset and last entry, trash edit
				if (!ent->mediaTime && (gf_list_count(trak->editBox->editList->entryList)==1)) {
					gf_isom_box_del_parent(&trak->child_boxes, (GF_Box *)trak->editBox);
					trak->editBox = NULL;
					break;
				}
			}
			SetTrackDuration(trak);
		}
	}

	if (!trak->Media->information->sampleTable->CompositionToDecode) {
		trak->Media->information->sampleTable->CompositionToDecode = (GF_CompositionToDecodeBox *) gf_isom_box_new_parent(&trak->Media->information->sampleTable->child_boxes, GF_ISOM_BOX_TYPE_CSLG);
		if (!trak->Media->information->sampleTable->CompositionToDecode)
			return GF_OUT_OF_MEM;
	}

	cslg = trak->Media->information->sampleTable->CompositionToDecode;
	if (cslg) {
		cslg->compositionToDTSShift = shift;
		cslg->leastDecodeToDisplayDelta = leastCTTS;
		cslg->greatestDecodeToDisplayDelta = greatestCTTS;
		cslg->compositionStartTime = 0;
		/*for our use case (first CTS set to 0), the composition end time is the media duration if it fits on 32 bits*/
		duration = gf_isom_get_media_duration(file, track);
		cslg->compositionEndTime = (duration<0x7FFFFFFF) ? (s32) duration : 0;
	}

	gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_ISO4, GF_TRUE);
	return GF_OK;
}

static GF_Err gf_isom_set_ctts_v0(GF_ISOFile *file, GF_TrackBox *trak)
{
	u32 i;
	s32 shift;
	GF_CompositionOffsetBox *ctts;
	GF_CompositionToDecodeBox *cslg;

	ctts = trak->Media->information->sampleTable->CompositionOffset;

	if (!trak->Media->information->sampleTable->CompositionToDecode)
	{
		shift = 0;
		for (i=0; i<ctts->nb_entries; i++) {
			if (-ctts->entries[i].decodingOffset > shift)
				shift = -ctts->entries[i].decodingOffset;
		}
		if (shift > 0)
		{
			for (i=0; i<ctts->nb_entries; i++) {
				ctts->entries[i].decodingOffset += shift;
			}
		}
	}
	else
	{
		cslg = trak->Media->information->sampleTable->CompositionToDecode;
		shift = cslg->compositionToDTSShift;
		for (i=0; i<ctts->nb_entries; i++) {
			ctts->entries[i].decodingOffset += shift;
		}
		gf_isom_box_del_parent(&trak->Media->information->sampleTable->child_boxes, (GF_Box *)cslg);
		trak->Media->information->sampleTable->CompositionToDecode = NULL;
	}
	if (shift>0) {
		//no edits, insert one
		if (! trak->editBox) {
			u64 dur = trak->Media->mediaHeader->duration;
			dur *= file->moov->mvhd->timeScale;
			dur /= trak->Media->mediaHeader->timeScale;
			gf_isom_set_edit(file, gf_list_find(file->moov->trackList, trak)+1, 0, dur, shift, GF_ISOM_EDIT_NORMAL);
		} else {
			//otherwise shift media times in all entries
			for (i=0; i<gf_list_count(trak->editBox->editList->entryList); i++) {
				GF_EdtsEntry *ent = (GF_EdtsEntry*)gf_list_get(trak->editBox->editList->entryList, i);
				//empty edit
				if (ent->mediaTime<0) continue;
				ent->mediaTime += shift;
			}
			SetTrackDuration(trak);
		}
	}
	ctts->version = 0;
	gf_isom_modify_alternate_brand(file, GF_ISOM_BRAND_ISO4, GF_FALSE);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_composition_offset_mode(GF_ISOFile *file, u32 track, Bool use_negative_offsets)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_CompositionOffsetBox *ctts;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;

	ctts = trak->Media->information->sampleTable->CompositionOffset;
	if (!ctts) {
		if (!use_negative_offsets && trak->Media->information->sampleTable->CompositionToDecode) {
			gf_isom_box_del_parent(&trak->Media->information->sampleTable->child_boxes, (GF_Box *)trak->Media->information->sampleTable->CompositionToDecode);
			trak->Media->information->sampleTable->CompositionToDecode = NULL;
		}
		return GF_OK;
	}

	if (use_negative_offsets) {
		return gf_isom_set_ctts_v1(file, track, 0);
	} else {
		if (ctts->version==0) return GF_OK;
		return gf_isom_set_ctts_v0(file, trak);
	}
}

#if 0 //unused
GF_Err gf_isom_set_sync_table(GF_ISOFile *file, u32 track)
{
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;

	if (!trak->Media->information->sampleTable->SyncSample) {
		trak->Media->information->sampleTable->SyncSample = (GF_SyncSampleBox *) gf_isom_box_new_parent(&trak->Media->information->sampleTable->child_boxes, GF_ISOM_BOX_TYPE_STSS);

		if (!trak->Media->information->sampleTable->SyncSample)
			return GF_OUT_OF_MEM;
	}
	return GF_OK;
}
#endif

GF_Err gf_isom_set_sample_flags(GF_ISOFile *file, u32 track, u32 sampleNumber, u32 isLeading, u32 dependsOn, u32 dependedOn, u32 redundant)
{
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;
	return stbl_SetDependencyType(trak->Media->information->sampleTable, sampleNumber, isLeading, dependsOn, dependedOn, redundant);
}

#if 0 //unused
GF_Err gf_isom_sample_set_dep_info(GF_ISOFile *file, u32 track, u32 sampleNumber, u32 isLeading, u32 dependsOn, u32 dependedOn, u32 redundant)
{
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;

	return stbl_AddDependencyType(trak->Media->information->sampleTable, sampleNumber, isLeading, dependsOn, dependedOn, redundant);
}
#endif

GF_EXPORT
GF_Err gf_isom_copy_sample_info(GF_ISOFile *dst, u32 dst_track, GF_ISOFile *src, u32 src_track, u32 sampleNumber)
{
	u32 i, count, idx, dst_sample_num, subs_flags;
	GF_SubSampleInfoEntry *sub_sample;
	GF_Err e;
	GF_TrackBox *src_trak, *dst_trak;

	src_trak = gf_isom_get_track_from_file(src, src_track);
	if (!src_trak) return GF_BAD_PARAM;

	dst_trak = gf_isom_get_track_from_file(dst, dst_track);
	if (!dst_trak) return GF_BAD_PARAM;

	dst_sample_num = dst_trak->Media->information->sampleTable->SampleSize->sampleCount;

	/*modify depends flags*/
	if (src_trak->Media->information->sampleTable->SampleDep) {
		u32 isLeading, dependsOn, dependedOn, redundant;

		isLeading = dependsOn = dependedOn = redundant = 0;

		e = stbl_GetSampleDepType(src_trak->Media->information->sampleTable->SampleDep, sampleNumber, &isLeading, &dependsOn, &dependedOn, &redundant);
		if (e) return e;

		e = stbl_AppendDependencyType(dst_trak->Media->information->sampleTable, isLeading, dependsOn, dependedOn, redundant);
		if (e) return e;
	}

	/*copy subsample info if any*/
	idx=1;
	while (gf_isom_get_subsample_types(src, src_track, idx, &subs_flags)) {
		GF_SubSampleInformationBox *dst_subs=NULL;
		idx++;

		if ( ! gf_isom_sample_get_subsample_entry(src, src_track, sampleNumber, subs_flags, &sub_sample))
			continue;

		/*create subsample if needed*/
		if (!dst_trak->Media->information->sampleTable->sub_samples) {
			dst_trak->Media->information->sampleTable->sub_samples = gf_list_new();
		}
		count = gf_list_count(dst_trak->Media->information->sampleTable->sub_samples);
		for (i=0; i<count; i++) {
			dst_subs = gf_list_get(dst_trak->Media->information->sampleTable->sub_samples, i);
			if (dst_subs->flags==subs_flags) break;
			dst_subs=NULL;
		}
		if (!dst_subs) {
			dst_subs = (GF_SubSampleInformationBox *) gf_isom_box_new_parent(&dst_trak->Media->information->sampleTable->child_boxes, GF_ISOM_BOX_TYPE_SUBS);
			if (!dst_subs) return GF_OUT_OF_MEM;
			dst_subs->version=0;
			dst_subs->flags = subs_flags;
			gf_list_add(dst_trak->Media->information->sampleTable->sub_samples, dst_subs);
		}

		count = gf_list_count(sub_sample->SubSamples);
		for (i=0; i<count; i++) {
			GF_SubSampleEntry *entry = (GF_SubSampleEntry*)gf_list_get(sub_sample->SubSamples, i);
			e = gf_isom_add_subsample_info(dst_subs, dst_sample_num, entry->subsample_size, entry->subsample_priority, entry->reserved, entry->discardable);
			if (e) return e;
		}
	}

	/*copy sampleToGroup info if any*/
	count = 0;
	if (src_trak->Media->information->sampleTable->sampleGroups)
		count = gf_list_count(src_trak->Media->information->sampleTable->sampleGroups);

	for (i=0; i<count; i++) {
		GF_SampleGroupBox *sg;
		u32 j, k, default_index;
		u32 first_sample_in_entry, last_sample_in_entry, group_desc_index_src, group_desc_index_dst;
		first_sample_in_entry = 1;

		sg = (GF_SampleGroupBox*)gf_list_get(src_trak->Media->information->sampleTable->sampleGroups, i);
		for (j=0; j<sg->entry_count; j++) {
			last_sample_in_entry = first_sample_in_entry + sg->sample_entries[j].sample_count - 1;
			if ((sampleNumber<first_sample_in_entry) || (sampleNumber>last_sample_in_entry)) {
				first_sample_in_entry = last_sample_in_entry+1;
				continue;
			}

			if (!dst_trak->Media->information->sampleTable->sampleGroups)
				dst_trak->Media->information->sampleTable->sampleGroups = gf_list_new();

			group_desc_index_src = group_desc_index_dst = sg->sample_entries[j].group_description_index;

			if (group_desc_index_src) {
				GF_SampleGroupDescriptionBox *sgd_src, *sgd_dst;
				GF_DefaultSampleGroupDescriptionEntry *sgde_src, *sgde_dst;

				group_desc_index_dst = 0;
				//check that the sample group description exists !!
				sgde_src = gf_isom_get_sample_group_info_entry(src, src_trak, sg->grouping_type, sg->sample_entries[j].group_description_index, &default_index, &sgd_src);

				if (!sgde_src) break;

				if (!dst_trak->Media->information->sampleTable->sampleGroupsDescription)
					dst_trak->Media->information->sampleTable->sampleGroupsDescription = gf_list_new();

				sgd_dst = NULL;
				for (k=0; k< gf_list_count(dst_trak->Media->information->sampleTable->sampleGroupsDescription); k++) {
					sgd_dst = gf_list_get(dst_trak->Media->information->sampleTable->sampleGroupsDescription, k);
					if (sgd_dst->grouping_type==sgd_src->grouping_type) break;
					sgd_dst = NULL;
				}
				if (!sgd_dst) {
					gf_isom_clone_box( (GF_Box *) sgd_src, (GF_Box **) &sgd_dst);
					if (!sgd_dst) return GF_OUT_OF_MEM;
					gf_list_add(dst_trak->Media->information->sampleTable->sampleGroupsDescription, sgd_dst);
				}

				//find the same entry
				for (k=0; k<gf_list_count(sgd_dst->group_descriptions); k++) {
					sgde_dst = gf_list_get(sgd_dst->group_descriptions, i);
					if (gf_isom_is_identical_sgpd(sgde_src, sgde_dst, sgd_src->grouping_type)) {
						group_desc_index_dst = k+1;
						break;
					}
				}
				if (!group_desc_index_dst) {
					GF_SampleGroupDescriptionBox *cloned=NULL;
					gf_isom_clone_box( (GF_Box *) sgd_src, (GF_Box **)  &cloned);
					if (!cloned) return GF_OUT_OF_MEM;
					sgde_dst = gf_list_get(cloned->group_descriptions, group_desc_index_dst);
					gf_list_rem(cloned->group_descriptions, group_desc_index_dst);
					gf_isom_box_del( (GF_Box *) cloned);
					gf_list_add(sgd_dst->group_descriptions, sgde_dst);
					group_desc_index_dst = gf_list_count(sgd_dst->group_descriptions);
				}
			}


			/*found our sample, add it to trak->sampleGroups*/
			e = gf_isom_add_sample_group_entry(dst_trak->Media->information->sampleTable->sampleGroups, dst_sample_num, sg->grouping_type, sg->grouping_type_parameter, group_desc_index_dst, dst_trak->Media->information->sampleTable->child_boxes, NULL);
			if (e) return e;
			break;
		}
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_text_set_display_flags(GF_ISOFile *file, u32 track, u32 desc_index, u32 flags, GF_TextFlagsMode op_type)
{
	u32 i;
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;

	for (i=0; i < gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes); i++) {
		GF_Tx3gSampleEntryBox *txt;
		if (desc_index && (i+1 != desc_index)) continue;

		txt = (GF_Tx3gSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, i);
		if (txt->type != GF_ISOM_BOX_TYPE_TX3G) continue;

		switch (op_type) {
		case GF_ISOM_TEXT_FLAGS_TOGGLE:
			txt->displayFlags |= flags;
			break;
		case GF_ISOM_TEXT_FLAGS_UNTOGGLE:
			txt->displayFlags &= ~flags;
			break;
		default:
			txt->displayFlags = flags;
			break;
		}
	}
	return GF_OK;

}


GF_EXPORT
GF_Err gf_isom_update_duration(GF_ISOFile *movie)
{
	u32 i;
	u64 maxDur;
	GF_TrackBox *trak;

	if (!movie || !movie->moov) return GF_BAD_PARAM;

	//if file was open in Write or Edit mode, recompute the duration
	//the duration of a movie is the MaxDuration of all the tracks...

	maxDur = 0;
	i=0;
	while ((trak = (GF_TrackBox *)gf_list_enum(movie->moov->trackList, &i))) {
		if( (movie->LastError = SetTrackDuration(trak))	) return movie->LastError;
		if (trak->Header && (trak->Header->duration > maxDur))
			maxDur = trak->Header->duration;
	}
	movie->moov->mvhd->duration = maxDur;

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_update_edit_list_duration(GF_ISOFile *file, u32 track)
{
	u32 i;
	u64 trackDuration;
	GF_EdtsEntry *ent;
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;


	//the total duration is the media duration: adjust it in case...
	e = Media_SetDuration(trak);
	if (e) return e;

	//assert the timeScales are non-NULL
	if (!trak->moov->mvhd->timeScale || !trak->Media->mediaHeader->timeScale) return GF_ISOM_INVALID_FILE;
	trackDuration = (trak->Media->mediaHeader->duration * trak->moov->mvhd->timeScale) / trak->Media->mediaHeader->timeScale;

	//if we have an edit list, the duration is the sum of all the editList
	//entries' duration (always expressed in MovieTimeScale)
	if (trak->editBox && trak->editBox->editList) {
		u64 editListDuration = 0;
		GF_EditListBox *elst = trak->editBox->editList;
		i=0;
		while ((ent = (GF_EdtsEntry*)gf_list_enum(elst->entryList, &i))) {
			if (ent->segmentDuration > trackDuration)
				ent->segmentDuration = trackDuration;
			if (!ent->segmentDuration) {
				u64 diff;
				ent->segmentDuration = trackDuration;
				if (ent->mediaTime>0) {
					diff = ent->mediaTime;
					diff *= trak->moov->mvhd->timeScale;
					diff /= trak->Media->mediaHeader->timeScale;
					if (diff < ent->segmentDuration)
						ent->segmentDuration -= diff;
					/*
					else
						diff = 0;
					*/
				}
			}
			if ((ent->mediaTime>=0) && ((u64) ent->mediaTime>=trak->Media->mediaHeader->duration)) {
				ent->mediaTime = trak->Media->mediaHeader->duration;
			}
			editListDuration += ent->segmentDuration;
		}
		trackDuration = editListDuration;
	}
	if (!trackDuration) {
		trackDuration = (trak->Media->mediaHeader->duration * trak->moov->mvhd->timeScale) / trak->Media->mediaHeader->timeScale;
	}
	trak->Header->duration = trackDuration;

	return GF_OK;

}


GF_EXPORT
GF_Err gf_isom_clone_pssh(GF_ISOFile *output, GF_ISOFile *input, Bool in_moof) {
	GF_Box *a;
	u32 i;
	i = 0;

	while ((a = (GF_Box *)gf_list_enum(input->moov->child_boxes, &i))) {
		if (a->type == GF_ISOM_BOX_TYPE_PSSH) {
			GF_List **child_boxes = &output->moov->child_boxes;
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			if (in_moof)
				child_boxes = &output->moof->child_boxes;
#endif

			GF_ProtectionSystemHeaderBox *pssh = (GF_ProtectionSystemHeaderBox *)gf_isom_box_new_parent(child_boxes, GF_ISOM_BOX_TYPE_PSSH);
			if (!pssh) return GF_OUT_OF_MEM;
			memmove(pssh->SystemID, ((GF_ProtectionSystemHeaderBox *)a)->SystemID, 16);
			if (((GF_ProtectionSystemHeaderBox *)a)->KIDs && ((GF_ProtectionSystemHeaderBox *)a)->KID_count > 0) {
				pssh->KID_count = ((GF_ProtectionSystemHeaderBox *)a)->KID_count;
				pssh->KIDs = (bin128 *)gf_malloc(pssh->KID_count*sizeof(bin128));
				if (!pssh->KIDs) return GF_OUT_OF_MEM;
				memmove(pssh->KIDs, ((GF_ProtectionSystemHeaderBox *)a)->KIDs, pssh->KID_count*sizeof(bin128));
			}
			pssh->private_data_size = ((GF_ProtectionSystemHeaderBox *)a)->private_data_size;
			pssh->private_data = (u8 *)gf_malloc(pssh->private_data_size*sizeof(char));
			if (!pssh->private_data) return GF_OUT_OF_MEM;
			memmove(pssh->private_data, ((GF_ProtectionSystemHeaderBox *)a)->private_data, pssh->private_data_size);
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_track_group(GF_ISOFile *file, u32 track_number, u32 track_group_id, u32 group_type, Bool do_add)
{
	u32 i, j;
	GF_TrackGroupTypeBox *trgt;
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(file, track_number);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->groups) trak->groups = (GF_TrackGroupBox*) gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_TRGR);
	if (!trak->groups) return GF_OUT_OF_MEM;

	for (j=0; j<gf_list_count(file->moov->trackList); j++) {
		GF_TrackBox *a_trak = gf_list_get(file->moov->trackList, j);
		if (!a_trak->groups) continue;

		for (i=0; i<gf_list_count(a_trak->groups->groups); i++) {
			trgt = gf_list_get(a_trak->groups->groups, i);

			if (trgt->track_group_id==track_group_id) {
				if (trgt->group_type != group_type) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("A track with same group ID is already defined for different group type %s\n", gf_4cc_to_str(trgt->group_type) ));
					return GF_BAD_PARAM;
				}
				if (a_trak==trak) {
					if (!do_add) {
						gf_list_rem(trak->groups->groups, i);
						gf_isom_box_del_parent(&trak->groups->child_boxes, (GF_Box *)trgt);
					}
					return GF_OK;
				}
			}
		}
	}
	//not found, add new group
	trgt = (GF_TrackGroupTypeBox*) gf_isom_box_new_parent(&trak->groups->child_boxes, GF_ISOM_BOX_TYPE_TRGT);
	if (!trgt) return GF_OUT_OF_MEM;
	trgt->track_group_id = track_group_id;
	trgt->group_type = group_type;
	return gf_list_add(trak->groups->groups, trgt);
}

GF_EXPORT
GF_Err gf_isom_set_nalu_length_field(GF_ISOFile *file, u32 track, u32 StreamDescriptionIndex, u32 nalu_size_length)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_MPEGVisualSampleEntryBox *ve;
	GF_SampleDescriptionBox *stsd;

	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd || !StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return GF_BAD_PARAM;
	}

	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (!entry || ! gf_isom_is_nalu_based_entry(trak->Media, entry)) {
		return GF_BAD_PARAM;
	}

	ve = (GF_MPEGVisualSampleEntryBox*)entry;
	if (ve->avc_config) ve->avc_config->config->nal_unit_size = nalu_size_length;
	if (ve->svc_config) ve->svc_config->config->nal_unit_size = nalu_size_length;
	if (ve->hevc_config) ve->hevc_config->config->nal_unit_size = nalu_size_length;
	if (ve->lhvc_config) ve->lhvc_config->config->nal_unit_size = nalu_size_length;
	return GF_OK;
}

GF_Err gf_isom_set_sample_group_in_traf(GF_ISOFile *file)
{
	GF_Err e;
	e = CanAccessMovie(file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	file->sample_groups_in_traf = GF_TRUE;
	return GF_OK;
}

GF_EXPORT
void gf_isom_set_progress_callback(GF_ISOFile *file, void (*progress_cbk)(void *udta, u64 nb_done, u64 nb_total), void *progress_cbk_udta)
{
	if (file) {
		file->progress_cbk = progress_cbk;
		file->progress_cbk_udta = progress_cbk_udta;

	}
}

GF_Err gf_isom_update_video_sample_entry_fields(GF_ISOFile *file, u32 track, u32 stsd_idx, u16 revision, u32 vendor, u32 temporalQ, u32 spatialQ, u32 horiz_res, u32 vert_res, u16 frames_per_sample, const char *compressor_name, s16 color_table_index)
{

	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *vid_ent;

	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(file, track);
	if (!trak || !stsd_idx) return GF_BAD_PARAM;

	if (!trak->Media || !trak->Media->handler || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->SampleDescription)
		return GF_ISOM_INVALID_FILE;

	switch (trak->Media->handler->handlerType) {
	case GF_ISOM_MEDIA_VISUAL:
    case GF_ISOM_MEDIA_AUXV:
    case GF_ISOM_MEDIA_PICT:
    	break;
	default:
		return GF_BAD_PARAM;
	}
	vid_ent = gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, stsd_idx-1);
	if (!vid_ent)
		return GF_BAD_PARAM;

	vid_ent->revision = revision;
	vid_ent->vendor = vendor;
	vid_ent->temporal_quality = temporalQ;
	vid_ent->spatial_quality = spatialQ;
	vid_ent->horiz_res = horiz_res;
	vid_ent->vert_res = vert_res;
	vid_ent->frames_per_sample = frames_per_sample;
	if (compressor_name)
		strncpy(vid_ent->compressor_name, compressor_name, 32);

	vid_ent->color_table_index = color_table_index;
	return GF_OK;
}


GF_Err gf_isom_update_sample_description_from_template(GF_ISOFile *file, u32 track, u32 sampleDescriptionIndex, u8 *data, u32 size)
{
	GF_BitStream *bs;
	GF_TrackBox *trak;
	GF_Box *ent, *tpl_ent;
	GF_Err e;
	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(file, track);
	if (!trak || !sampleDescriptionIndex) return GF_BAD_PARAM;

	if (!trak->Media || !trak->Media->handler || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->SampleDescription)
		return GF_ISOM_INVALID_FILE;

	ent = gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDescriptionIndex-1);
	if (!ent) return GF_BAD_PARAM;

	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
//	e = gf_isom_box_parse(&tpl_ent, bs);
	e = gf_isom_box_parse_ex (&tpl_ent, bs, GF_ISOM_BOX_TYPE_STSD, GF_FALSE);
	gf_bs_del(bs);
	if (e) return e;

	while (gf_list_count(tpl_ent->child_boxes)) {
		u32 j=0;
		Bool found = GF_FALSE;
		GF_Box *abox = gf_list_pop_front(tpl_ent->child_boxes);

		switch (abox->type) {
		case GF_ISOM_BOX_TYPE_SINF:
		case GF_ISOM_BOX_TYPE_RINF:
		case GF_ISOM_BOX_TYPE_BTRT:
			found = GF_TRUE;
			break;
		}

		if (found) {
			gf_isom_box_del(abox);
			continue;
		}
		
		if (!ent->child_boxes) ent->child_boxes = gf_list_new();
		for (j=0; j<gf_list_count(ent->child_boxes); j++) {
			GF_Box *b = gf_list_get(ent->child_boxes, j);
			if (b->type == abox->type) {
				found = GF_TRUE;
				break;
			}
		}
		if (!found) {
			gf_list_add(ent->child_boxes, abox);
		} else {
			gf_isom_box_del(abox);
		}
	}
	gf_isom_box_del(tpl_ent);

	//patch for old export
	GF_Box *abox = gf_isom_box_find_child(ent->child_boxes, GF_ISOM_BOX_TYPE_SINF);
	if (abox) {
		gf_list_del_item(ent->child_boxes, abox);
		gf_list_add(ent->child_boxes, abox);
	}
	return GF_OK;
}

#include <gpac/xml.h>
GF_EXPORT
GF_Err gf_isom_apply_box_patch(GF_ISOFile *file, GF_ISOTrackID globalTrackID, const char *box_patch_filename, Bool for_fragments)
{
	GF_Err e;
	GF_DOMParser *dom;
	u32 i;
	GF_XMLNode *root;
	u8 *box_data=NULL;
	u32 box_data_size;
	if (!file || !box_patch_filename) return GF_BAD_PARAM;
	dom = gf_xml_dom_new();
	if (strstr(box_patch_filename, "<?xml")) {
		e = gf_xml_dom_parse_string(dom, (char *) box_patch_filename);
	} else {
		e = gf_xml_dom_parse(dom, box_patch_filename, NULL, NULL);
	}
	if (e) goto err_exit;

	root = gf_xml_dom_get_root(dom);
	if (!root || strcmp(root->name, "GPACBOXES")) {
		e = GF_NON_COMPLIANT_BITSTREAM;
		goto err_exit;
	}

	//compute size of each child boxes to freeze the order
	if (for_fragments) {
		u32 count = file->moof ? gf_list_count(file->moof->child_boxes) : 0;
		for (i=0; i<count; i++) {
			GF_Box *box = gf_list_get(file->moof->child_boxes, i);
			if (!(box->internal_flags & GF_ISOM_ORDER_FREEZE)) {
				gf_isom_box_size(box);
				gf_isom_box_freeze_order(box);
			}
		}
	} else {
		for (i=0; i<gf_list_count(file->TopBoxes); i++) {
			GF_Box *box = gf_list_get(file->TopBoxes, i);
			if (!(box->internal_flags & GF_ISOM_ORDER_FREEZE)) {
				gf_isom_box_size(box);
				gf_isom_box_freeze_order(box);
			}
		}
	}

	for (i=0; i<gf_list_count(root->content); i++) {
		u32 j;
		u32 path_len;
		Bool essential_prop=GF_FALSE;
		u32 trackID=globalTrackID;
		u32 item_id=trackID;
		Bool is_frag_box;
		char *box_path=NULL;
		GF_Box *parent_box = NULL;
		GF_XMLNode *box_edit = gf_list_get(root->content, i);
		if (!box_edit->name || strcmp(box_edit->name, "Box")) continue;

		for (j=0; j<gf_list_count(box_edit->attributes);j++) {
			GF_XMLAttribute *att = gf_list_get(box_edit->attributes, j);
			if (!strcmp(att->name, "path")) box_path = att->value;
			else if (!strcmp(att->name, "essential")) {
				if (!strcmp(att->value, "yes") || !strcmp(att->value, "true") || !strcmp(att->value, "1")) {
					essential_prop=GF_TRUE;
				}
			}
			else if (!strcmp(att->name, "itemID"))
				item_id = atoi(att->value);
			else if (!globalTrackID && !strcmp(att->name, "trackID"))
				trackID = atoi(att->value);
		}

		if (!box_path) continue;
		path_len = (u32) strlen(box_path);

		is_frag_box = !strncmp(box_path, "traf", 4) ? GF_TRUE : GF_FALSE;

		if (for_fragments && !is_frag_box) continue;
		else if (!for_fragments && is_frag_box) continue;

		gf_xml_parse_bit_sequence(box_edit, box_patch_filename, &box_data, &box_data_size);
		if (box_data_size && (box_data_size<4) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] Wrong BS specification for box, shall either be empty or at least 4 bytes for box type\n"));
			e = GF_NON_COMPLIANT_BITSTREAM;
			goto err_exit;
		}

		while (box_path && (path_len>=4)) {
			u32 parent_list_box_type;
			GF_List **parent_list;
			u32 box_type = GF_4CC(box_path[0],box_path[1],box_path[2],box_path[3]);
			GF_Box *box=NULL;
			GF_BitStream *bs;
			s32 insert_pos = -1;
			box_path+=4;
			path_len-=4;

			if (!parent_box) {
				box=gf_isom_box_find_child(file->TopBoxes, box_type);
				if (!box) {
					if (box_type==GF_ISOM_BOX_TYPE_TRAK) {
						if (trackID) {
							box = (GF_Box *) gf_isom_get_track_from_file(file, gf_isom_get_track_by_id(file, trackID) );
						}
						if (!box && gf_list_count(file->moov->trackList)==1) {
							box = gf_list_get(file->moov->trackList, 0);
						}
					}
					else if (box_type==GF_ISOM_BOX_TYPE_TRAF) {
						if (trackID) {
							box = (GF_Box *) gf_isom_get_traf(file, trackID);
						}
						if (!box && file->moof && gf_list_count(file->moof->TrackList)==1) {
							box = gf_list_get(file->moof->TrackList, 0);
						}
					}
				}
				if (!box) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] Cannot locate box type %s at root or as track\n", gf_4cc_to_str(box_type) ));
					e = GF_BAD_PARAM;
					goto err_exit;
				}
			} else {
				box = gf_isom_box_find_child(parent_box->child_boxes, box_type);
				if (!box) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] Cannot locate box type %s at child of %s\n", gf_4cc_to_str(box_type), gf_4cc_to_str(parent_box->type)));
					e = GF_BAD_PARAM;
					goto err_exit;
				}
			}
			// '.' is child access
			if (path_len && (box_path[0]=='.')) {
				box_path += 1;
				path_len-=1;
				parent_box = box;
				continue;
			}

			if (parent_box && !parent_box->child_boxes) parent_box->child_boxes = gf_list_new();
			parent_list = parent_box ? &parent_box->child_boxes : &file->TopBoxes;
			parent_list_box_type = parent_box ? parent_box->type : 0;

			// '+' is append after, '-' is insert before
			if (path_len && ((box_path[0]=='-') || (box_path[0]=='+')) ) {
				s32 idx = gf_list_find(*parent_list, box);
				assert(idx>=0);
				if (box_path[0]=='+') insert_pos = idx+1;
				else insert_pos = idx;
			}
			else if (path_len) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] Invalid path %s, expecting either '-', '+' or '.' as separators\n", box_path));
				e = GF_NON_COMPLIANT_BITSTREAM;
				goto err_exit;
			}

			if (!box_data) {
				if (insert_pos>=0) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ISOBMFF] Invalid path %s for box removal, ignoring position\n", box_path));
				}
				switch (box->type) {
				case GF_ISOM_BOX_TYPE_MOOV:
					file->moov = NULL;
					break;
				case GF_ISOM_BOX_TYPE_MDAT:
					file->mdat = NULL;
					break;
				case GF_ISOM_BOX_TYPE_PDIN:
					file->pdin = NULL;
					break;
				case GF_ISOM_BOX_TYPE_FTYP:
					file->brand = NULL;
					break;
				case GF_ISOM_BOX_TYPE_META:
					if ((GF_Box *) file->meta == box) file->meta = NULL;
					break;
				}
				if (parent_box) {
					gf_isom_box_remove_from_parent(parent_box, box);
				}
				gf_isom_box_del_parent(parent_list, box);
			} else {
				u32 size;

				bs = gf_bs_new(box_data, box_data_size, GF_BITSTREAM_READ);
				size = gf_bs_read_u32(bs);
				if (size != box_data_size) {
					GF_UnknownBox *new_box = (GF_UnknownBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_UNKNOWN);
					new_box->original_4cc = size;
					new_box->dataSize = (u32) gf_bs_available(bs);
					new_box->data = gf_malloc(sizeof(u8)*new_box->dataSize);
					gf_bs_read_data(bs, new_box->data, new_box->dataSize);
					if (insert_pos<0) {
						gf_list_add(box->child_boxes, new_box);
						insert_pos = gf_list_find(box->child_boxes, new_box);
					} else {
						gf_list_insert(*parent_list, new_box, insert_pos);
					}

					if (parent_box && (parent_box->type==GF_ISOM_BOX_TYPE_IPRP)) {
						GF_ItemPropertyAssociationBox *ipma = (GF_ItemPropertyAssociationBox *) gf_isom_box_find_child(parent_box->child_boxes, GF_ISOM_BOX_TYPE_IPMA);
						if (!item_id) {
							GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[ISOBMFF] Inserting box in ipco without itemID, no association added\n"));
						} else if (ipma) {
							u32 nb_asso, k;
							GF_ItemPropertyAssociationEntry *entry = NULL;
							nb_asso = gf_list_count(ipma->entries);
							for (k=0; k<nb_asso;k++) {
								entry = gf_list_get(ipma->entries, k);
								if (entry->item_id==item_id) break;
								entry = NULL;
							}
							if (!entry) {
								GF_SAFEALLOC(entry, GF_ItemPropertyAssociationEntry);
								if (!entry) return GF_OUT_OF_MEM;
								gf_list_add(ipma->entries, entry);
								entry->item_id = item_id;
							}
							entry->associations = gf_realloc(entry->associations, sizeof(GF_ItemPropertyAssociationSlot) * (entry->nb_associations+1));
							entry->associations[entry->nb_associations].essential = essential_prop;
							entry->associations[entry->nb_associations].index = 1+insert_pos;
							entry->nb_associations++;
						}
					}
				} else {
					u32 box_idx = 0;

					gf_bs_seek(bs, 0);
					while (gf_bs_available(bs)) {
						GF_Box *new_box;
						e = gf_isom_box_parse_ex(&new_box, bs, (insert_pos<0) ? box->type : parent_list_box_type, parent_box ? GF_FALSE : GF_TRUE);
						if (e) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] failed to parse box\n", box_path));
							gf_bs_del(bs);
							goto err_exit;
						}
						if (insert_pos<0) {
							gf_list_add(box->child_boxes, new_box);
						} else {
							gf_list_insert(*parent_list, new_box, insert_pos+box_idx);
							box_idx++;
						}
					}
				}
				gf_bs_del(bs);

			}
			gf_free(box_data);
			box_data = NULL;
			box_path = NULL;
		}
	}

err_exit:

	gf_xml_dom_del(dom);
	if (box_data) gf_free(box_data);
	return e;
}

GF_EXPORT
GF_Err gf_isom_set_track_magic(GF_ISOFile *movie, u32 trackNumber, u64 magic)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	trak->magic = magic;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_track_index(GF_ISOFile *movie, u32 trackNumber, u32 index, void (*track_num_changed)(void *udta, u32 old_track_num, u32 new_track_num), void *udta)
{
	u32 i, count;
	GF_List *tracks;
	u32 prev_index=0, prev_pos=0;
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !index) return GF_BAD_PARAM;
	trak->index = index;
	tracks = gf_list_new();
	count = gf_list_count(movie->moov->trackList);
	//sort tracks in new list
	for (i=0; i<count; i++) {
		GF_TrackBox *a_tk = gf_list_get(movie->moov->trackList, i);
		if (!a_tk->index) {
			gf_list_insert(tracks, a_tk, 0);
		} else if (a_tk->index < prev_index) {
			gf_list_insert(tracks, a_tk, prev_pos);
		} else {
			gf_list_add(tracks, a_tk);
		}
		prev_pos = gf_list_count(tracks) - 1;
		prev_index = a_tk->index;
	}
	if (gf_list_count(tracks) != count) {
		gf_list_del(tracks);
		return GF_OUT_OF_MEM;
	}
	if (track_num_changed) {
		for (i=0; i<count; i++) {
			GF_TrackBox *a_tk = gf_list_get(tracks, i);
			s32 old_pos = gf_list_find(movie->moov->trackList, a_tk);
			assert(old_pos>=0);
			if (old_pos != i)
				track_num_changed(udta, old_pos+1, i+1);
		}
	}
	gf_list_del(movie->moov->trackList);
	movie->moov->trackList = tracks;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_ipod_compatible(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_MPEGVisualSampleEntryBox *entry;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;
	entry = (GF_MPEGVisualSampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, 0);
	if (!entry) return GF_OK;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_MVC1:
	case GF_ISOM_BOX_TYPE_HVC1:
	case GF_ISOM_BOX_TYPE_HEV1:
	case GF_ISOM_BOX_TYPE_HVT1:
		break;
	default:
		return GF_OK;
	}

	if (!entry->ipod_ext) {
		entry->ipod_ext = (GF_UnknownUUIDBox *) gf_isom_box_new_parent(&entry->child_boxes, GF_ISOM_BOX_TYPE_UUID);
		if (!entry->ipod_ext) return GF_OUT_OF_MEM;
	}
	memcpy(entry->ipod_ext->uuid, GF_ISOM_IPOD_EXT, sizeof(u8)*16);
	entry->ipod_ext->dataSize = 4;
	entry->ipod_ext->data = gf_malloc(sizeof(u8)*4);
	if (!entry->ipod_ext->data) return GF_OUT_OF_MEM;
	memset(entry->ipod_ext->data, 0, sizeof(u8)*4);
	return GF_OK;
}

GF_EXPORT
Bool gf_isom_is_inplace_rewrite(GF_ISOFile *movie)
{
	if (!movie) return GF_FALSE;
	if (!movie->no_inplace_rewrite) {
		//things where added to the file, no inplace rewrite
		if (movie->editFileMap && gf_bs_get_size(movie->editFileMap->bs))
			movie->no_inplace_rewrite = GF_TRUE;
		//block redirect (used by mp4mx), no inplace rewrite
		else if (movie->on_block_out || !strcmp(movie->finalName, "_gpac_isobmff_redirect"))
			movie->no_inplace_rewrite = GF_TRUE;
		//stdout redirect, no inplace rewrite
		else if (!strcmp(movie->finalName, "std"))
			movie->no_inplace_rewrite = GF_TRUE;
		//new file, no inplace rewrite
		else if (!movie->fileName)
			movie->no_inplace_rewrite = GF_TRUE;
	}
	if (movie->no_inplace_rewrite) return GF_FALSE;

	return GF_TRUE;
}

GF_EXPORT
void gf_isom_disable_inplace_rewrite(GF_ISOFile *movie)
{
	if (movie)
		movie->no_inplace_rewrite = GF_TRUE;
}


GF_Err gf_isom_set_y3d_info(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescriptionIndex, GF_ISOM_Y3D_Info *info)
{
	GF_Err e;
	u32 proj_type;
	GF_SampleEntryBox *ent;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !info) return GF_BAD_PARAM;

	ent = gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDescriptionIndex-1);
	if (!ent) return GF_BAD_PARAM;

	if (info->projection_type > GF_PROJ360_EQR) return GF_NOT_SUPPORTED;

	GF_Stereo3DBox *st3d = (GF_Stereo3DBox *) gf_isom_box_find_child(ent->child_boxes, GF_ISOM_BOX_TYPE_ST3D);
	if (st3d) {
		if (info->stereo_type) {
			st3d->stereo_type = info->stereo_type;
		} else {
			gf_isom_box_del_parent(&ent->child_boxes, (GF_Box *) st3d);
		}
	} else if (info->stereo_type) {
		st3d = (GF_Stereo3DBox *) gf_isom_box_new_parent(&ent->child_boxes, GF_ISOM_BOX_TYPE_ST3D);
		if (!st3d) return GF_OUT_OF_MEM;
		st3d->stereo_type = info->stereo_type;
	}


	GF_Box *sv3d = gf_isom_box_find_child(ent->child_boxes, GF_ISOM_BOX_TYPE_SV3D);
	if (sv3d && !info->projection_type) {
		gf_isom_box_del_parent(&ent->child_boxes, sv3d);
		return GF_OK;
	}

	if (!sv3d && !info->projection_type) {
		return GF_OK;
	}
	if (!sv3d) {
		sv3d = gf_isom_box_new_parent(&ent->child_boxes, GF_ISOM_BOX_TYPE_SV3D);
		if (!sv3d) return GF_OUT_OF_MEM;
	}

	//svhd mandatory
	GF_SphericalVideoInfoBox *svhd = (GF_SphericalVideoInfoBox *) gf_isom_box_find_child(sv3d->child_boxes, GF_ISOM_BOX_TYPE_SVHD);
	if (svhd) {
		if (svhd->string) gf_free(svhd->string);
	} else {
		svhd = (GF_SphericalVideoInfoBox *) gf_isom_box_new_parent(&sv3d->child_boxes, GF_ISOM_BOX_TYPE_SVHD);
		if (!svhd) return GF_OUT_OF_MEM;
	}
	svhd->string = gf_strdup(info->meta_data ? info->meta_data : "");

	//proj mandatory
	GF_Box *proj = gf_isom_box_find_child(sv3d->child_boxes, GF_ISOM_BOX_TYPE_PROJ);
	if (!proj) {
		proj = (GF_Box *) gf_isom_box_new_parent(&sv3d->child_boxes, GF_ISOM_BOX_TYPE_PROJ);
		if (!proj) return GF_OUT_OF_MEM;
	}

	GF_ProjectionHeaderBox *projh = (GF_ProjectionHeaderBox *) gf_isom_box_find_child(proj->child_boxes, GF_ISOM_BOX_TYPE_PRHD);
	//prj header mandatory
	if (!projh) {
		projh = (GF_ProjectionHeaderBox *) gf_isom_box_new_parent(&proj->child_boxes, GF_ISOM_BOX_TYPE_PRHD);
		if (!projh) return GF_OUT_OF_MEM;
	}
	projh->yaw = info->yaw;
	projh->pitch = info->pitch;
	projh->roll = info->roll;

	proj_type = (info->projection_type==GF_PROJ360_CUBE_MAP) ? GF_ISOM_BOX_TYPE_CBMP : GF_ISOM_BOX_TYPE_EQUI;

	GF_ProjectionTypeBox *projt = (GF_ProjectionTypeBox *) gf_isom_box_find_child(proj->child_boxes, proj_type);
	if (!projt) {
		projt = (GF_ProjectionTypeBox *) gf_isom_box_new_parent(&proj->child_boxes, proj_type);
		if (!projt) return GF_OUT_OF_MEM;
	}
	if (info->projection_type==GF_PROJ360_CUBE_MAP) {
		projt->layout = info->layout;
		projt->padding = info->padding;
	} else {
		projt->bounds_top = info->top;
		projt->bounds_bottom = info->bottom;
		projt->bounds_left = info->left;
		projt->bounds_right = info->right;
	}

	//remove other ones
	GF_Box *b = gf_isom_box_new_parent(&proj->child_boxes, GF_ISOM_BOX_TYPE_MSHP);
	if (b) gf_isom_box_del_parent(&proj->child_boxes, b);
	if (info->projection_type==GF_PROJ360_CUBE_MAP) {
		b = gf_isom_box_new_parent(&proj->child_boxes, GF_ISOM_BOX_TYPE_EQUI);
		if (b) gf_isom_box_del_parent(&proj->child_boxes, b);
	} else {
		b = gf_isom_box_new_parent(&proj->child_boxes, GF_ISOM_BOX_TYPE_EQUI);
		if (b) gf_isom_box_del_parent(&proj->child_boxes, b);

	}
	return GF_OK;
}


#endif	/*!defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)*/
