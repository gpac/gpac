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

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)

GF_Err CanAccessMovie(GF_ISOFile *movie, u32 Mode)
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
		trak->is_unpacked = 1;
	}
	return e;
}


GF_Err FlushCaptureMode(GF_ISOFile *movie)
{
	GF_Err e;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_OK;
	/*make sure nothing was added*/
	if (gf_bs_get_position(movie->editFileMap->bs)) return GF_OK;

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

	/*we have a trick here: the data will be stored on the fly, so the first
	thing in the file is the MDAT. As we don't know if we have a large file (>4 GB) or not
	do as if we had one and write 16 bytes: 4 (type) + 4 (size) + 8 (largeSize)...*/
	gf_bs_write_int(movie->editFileMap->bs, 0, 128);
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

	iods = (GF_ObjectDescriptorBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_IODS);
	iods->descriptor = od;
	return moov_AddBox((GF_Box*)moov, (GF_Box *)iods);
}

//add a track to the root OD
GF_EXPORT
GF_Err gf_isom_add_track_to_root_od(GF_ISOFile *movie, u32 trackNumber)
{
	GF_Err e;
	GF_ES_ID_Inc *inc;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(movie);
	
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
	gf_isom_box_del((GF_Box *)movie->moov->iods);
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

	if (!movie->moov->iods) AddMovieIOD(movie->moov, 0);

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
		if (inc->trackID == gf_isom_get_track_id(movie, trackNumber)) {
			gf_odf_desc_del((GF_Descriptor *)inc);
			gf_list_rem(esds, i-1);
			break;
		}
	}
	//we don't remove the iod for P&Ls and other potential info
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_creation_time(GF_ISOFile *movie, u64 time)
{
	if (!movie || !movie->moov) return GF_BAD_PARAM;
	movie->moov->mvhd->creationTime = time;
	movie->moov->mvhd->modificationTime = time;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_track_creation_time(GF_ISOFile *movie,u32 trackNumber, u64 time)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	trak->Header->creationTime = time;
	trak->Header->modificationTime = time;
	return GF_OK;
}


//sets the enable flag of a track
GF_EXPORT
GF_Err gf_isom_set_track_enabled(GF_ISOFile *movie, u32 trackNumber, u8 enableTrack)
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

GF_EXPORT
GF_Err gf_isom_set_media_language(GF_ISOFile *movie, u32 trackNumber, char *three_char_code)
{
	GF_Err e;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	memcpy(trak->Media->mediaHeader->packedLanguage, three_char_code, sizeof(char)*3);
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	return GF_OK;
}

static void gf_isom_set_root_iod(GF_ISOFile *movie)
{
	GF_IsomInitialObjectDescriptor *iod;
	GF_IsomObjectDescriptor *od;

	gf_isom_insert_moov(movie);
	if (!movie->moov->iods) {
		AddMovieIOD(movie->moov, 1);
		return;
	}
	//if OD, switch to IOD
	if (movie->moov->iods->descriptor->tag == GF_ODF_ISOM_IOD_TAG) return;
	od = (GF_IsomObjectDescriptor *) movie->moov->iods->descriptor;
	iod = (GF_IsomInitialObjectDescriptor*)gf_malloc(sizeof(GF_IsomInitialObjectDescriptor));
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
}

GF_Err gf_isom_add_desc_to_root_od(GF_ISOFile *movie, GF_Descriptor *theDesc)
{
	GF_Err e;
	GF_Descriptor *desc, *dupDesc;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(movie);

	if (!movie->moov->iods) AddMovieIOD(movie->moov, 0);
	if (theDesc->tag==GF_ODF_IPMP_TL_TAG) gf_isom_set_root_iod(movie);

	desc = movie->moov->iods->descriptor;
	//the type of desc is handled at the OD/IOD level, we'll be notified
	//if the desc is not allowed
	switch (desc->tag) {
	case GF_ODF_ISOM_IOD_TAG:
	case GF_ODF_ISOM_OD_TAG:
		//duplicate the desc
		e = gf_odf_desc_copy(theDesc, &dupDesc);
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
	GF_Err e;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(movie);
	
	movie->moov->mvhd->timeScale = timeScale;
	movie->interleavingTime = timeScale;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_set_pl_indication(GF_ISOFile *movie, u8 PL_Code, u8 ProfileLevel)
{
	GF_IsomInitialObjectDescriptor *iod;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	gf_isom_set_root_iod(movie);
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
	}
	return GF_OK;
}


GF_Err gf_isom_set_root_od_id(GF_ISOFile *movie, u32 OD_ID)
{
	GF_Err e;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	gf_isom_insert_moov(movie);
	if (!movie->moov->iods) AddMovieIOD(movie->moov, 0);

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

GF_Err gf_isom_set_root_od_url(GF_ISOFile *movie, char *url_string)
{
	GF_Err e;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(movie);

	if (!movie->moov->iods) AddMovieIOD(movie->moov, 0);
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



//creates a new Track. If trackID = 0, the trackID is chosen by the API
//returns the track number or 0 if error
GF_EXPORT
u32 gf_isom_new_track(GF_ISOFile *movie, u32 trakID, u32 MediaType, u32 TimeScale)
{
	GF_Err e;
	u64 now;
	u8 isHint;
	GF_TrackBox *trak;
	GF_TrackHeaderBox *tkhd;
	GF_MediaBox *mdia;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) {
		gf_isom_set_last_error(movie, e);
		return 0;
	}
	gf_isom_insert_moov(movie);


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
	
	//OK, now create a track...
	trak = (GF_TrackBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TRAK);
	if (!trak) {
		gf_isom_set_last_error(movie, GF_OUT_OF_MEM);
		return 0;
	}
	tkhd = (GF_TrackHeaderBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TKHD);
	if (!tkhd) {
		gf_isom_set_last_error(movie, GF_OUT_OF_MEM);
		gf_isom_box_del((GF_Box *)trak);
		return 0;
	}
	now = gf_isom_get_mp4time();
	tkhd->creationTime = now;
	if (!movie->keep_utc)
		tkhd->modificationTime = now;
	//OK, set up the media trak
	e = NewMedia(&mdia, MediaType, TimeScale);
	if (e) {
		gf_isom_box_del((GF_Box *)mdia);
		gf_isom_box_del((GF_Box *)trak);
		gf_isom_box_del((GF_Box *)tkhd);
		return 0;
	}
	//OK, add this media to our track
	mdia->mediaTrack = trak;

	e = trak_AddBox((GF_Box*)trak, (GF_Box *) tkhd); if (e) goto err_exit;
	e = trak_AddBox((GF_Box*)trak, (GF_Box *) mdia); if (e) goto err_exit;
	tkhd->trackID = trakID;

	
	//some default properties for Audio, Visual or private tracks
	switch (MediaType) {
	case GF_ISOM_MEDIA_VISUAL:
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

	mdia->mediaHeader->creationTime = mdia->mediaHeader->modificationTime = now;
	trak->Header->creationTime = trak->Header->modificationTime = now;

	//OK, add our trak
	e = moov_AddBox((GF_Box*)movie->moov, (GF_Box *)trak); if (e) goto err_exit;
	//set the new ID available
	if (trakID+1> movie->moov->mvhd->nextTrackID) 
		movie->moov->mvhd->nextTrackID = trakID+1;

	//and return our track number
	return gf_isom_get_track_by_id(movie, trakID);

err_exit:
	if (tkhd) gf_isom_box_del((GF_Box *)tkhd);
	if (trak) gf_isom_box_del((GF_Box *)trak);
	if (mdia) gf_isom_box_del((GF_Box *)mdia);
	return 0;
}


//Create a new StreamDescription in the file. The URL and URN are used to describe external media
GF_EXPORT
GF_Err gf_isom_new_mpeg4_description(GF_ISOFile *movie,
							   u32 trackNumber, 
							   GF_ESD *esd, 
							   char *URLname, 
							   char *URNname, 
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
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
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
	if (new_esd->URLString) {

	}
	return e;
}

//Add samples to a track. Use streamDescriptionIndex to specify the desired stream (if several)
GF_EXPORT
GF_Err gf_isom_add_sample(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, GF_ISOSample *sample)
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
	Dentry = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->other_boxes, dataRefIndex - 1);
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
		if (!e) e = Media_AddSample(trak->Media, data_offset, od_sample, descIndex, 0);
		if (!e) e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, od_sample->data, od_sample->dataLength);
		if (od_sample) gf_isom_sample_del(&od_sample);
	} else {
		e = Media_AddSample(trak->Media, data_offset, sample, descIndex, 0);
		if (!e && sample->dataLength) e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, sample->data, sample->dataLength);
	}
	if (e) return e;

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	return SetTrackDuration(trak);
}

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
	Bool offset_times = 0;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !sample) return GF_BAD_PARAM;

	e = FlushCaptureMode(movie);
	if (e) return e;

	e = unpack_track(trak);
	if (e) return e;

	e = findEntryForTime(trak->Media->information->sampleTable, sample->DTS, 0, &sampleNum, &prevSampleNum);
	if (e) return e;
	/*we need the EXACT match*/
	if (!sampleNum) return GF_BAD_PARAM;

	prev = gf_isom_get_sample_info(movie, trackNumber, sampleNum, &descIndex, NULL);
	if (!prev) return gf_isom_last_error(movie);
	/*for conformance*/
	if (sample->DTS==prev->DTS) offset_times = 1;
	gf_isom_sample_del(&prev);

	e = Media_GetSampleDesc(trak->Media, descIndex, &entry, &dataRefIndex);
	if (e) return e;
	if (!entry || !dataRefIndex) return GF_BAD_PARAM;
	trak->Media->information->sampleTable->currentEntryIndex = descIndex;

	//get this dataRef and return false if not self contained
	Dentry = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->other_boxes, dataRefIndex - 1);
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
		if (!e) e = Media_AddSample(trak->Media, data_offset, od_sample, descIndex, sampleNum);
		if (!e) e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, od_sample->data, od_sample->dataLength);
		if (od_sample) gf_isom_sample_del(&od_sample);
	} else {
		e = Media_AddSample(trak->Media, data_offset, sample, descIndex, sampleNum);
		if (!e) e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, sample->data, sample->dataLength);
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

GF_Err gf_isom_set_sample_rap(GF_ISOFile *movie, u32 trackNumber)
{
	GF_SampleTableBox *stbl;
	GF_Err e;
	GF_TrackBox *trak;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	stbl = trak->Media->information->sampleTable;
	if (!stbl->SyncSample) stbl->SyncSample = (GF_SyncSampleBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSS);
	return stbl_AddRAP(stbl->SyncSample, stbl->SampleSize->sampleCount);

}

GF_Err gf_isom_append_sample_data(GF_ISOFile *movie, u32 trackNumber, char *data, u32 data_size)
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
	Dentry = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->other_boxes, dataRefIndex - 1);
	if (!Dentry || Dentry->flags != 1) return GF_BAD_PARAM;

	//Open our data map. We are adding stuff, so use EDIT
	e = gf_isom_datamap_open(trak->Media, dataRefIndex, 1);
	if (e) return e;

	//add the media data
	e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, data, data_size);
	if (e) return e;
	//update data size
	return stbl_SampleSizeAppend(trak->Media->information->sampleTable->SampleSize, data_size);
}


//Add sample reference to a track. The SampleOffset is the offset of the data in the referenced file
//you must have created a StreamDescription with URL or URN specifying your referenced file
//the data offset specifies the begining of the chunk
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
	Dentry =(GF_DataEntryURLBox*) gf_list_get(trak->Media->information->dataInformation->dref->other_boxes, dataRefIndex - 1);
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
GF_EXPORT
GF_Err gf_isom_set_last_sample_duration(GF_ISOFile *movie, u32 trackNumber, u32 duration)
{
	GF_TrackBox *trak;
	GF_SttsEntry *ent;
	GF_TimeToSampleBox *stts;
	u64 mdur;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	mdur = trak->Media->mediaHeader->duration;
	stts = trak->Media->information->sampleTable->TimeToSample;
	if (!stts->nb_entries) return GF_BAD_PARAM;
	//get the last entry
	ent = (GF_SttsEntry*) &stts->entries[stts->nb_entries-1];

	mdur -= ent->sampleDelta;
	mdur += duration;
	//we only have one sample
	if (ent->sampleCount == 1) {
		ent->sampleDelta = duration;
	} else {
		if (ent->sampleDelta == duration) return GF_OK;
		ent->sampleCount -= 1;

		if (stts->nb_entries==stts->alloc_size) {
			stts->alloc_size++;
			stts->entries = gf_realloc(stts->entries, sizeof(GF_SttsEntry)*stts->alloc_size);
			if (!stts->entries) return GF_OUT_OF_MEM;
		}
		stts->entries[stts->nb_entries].sampleCount = 1;
		stts->entries[stts->nb_entries].sampleDelta = duration;
		stts->nb_entries++;
		//and update the write cache
		stts->w_currentSampleNum = trak->Media->information->sampleTable->SampleSize->sampleCount;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	trak->Media->mediaHeader->duration = mdur;
	return SetTrackDuration(trak);
}

//update a sample data in the media. Note that the sample MUST exists
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
	return GF_OK;
}

//update a sample data in the media. Note that the sample MUST exists,
//that sample->data MUST be NULL and sample->dataLength must be NON NULL;
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

	//remove DTS
	e = stbl_RemoveDTS(trak->Media->information->sampleTable, sampleNumber, trak->Media->mediaHeader->timeScale);
	if (e) return e;
	//remove CTS if any
	if (trak->Media->information->sampleTable->CompositionOffset) {
		e = stbl_RemoveCTS(trak->Media->information->sampleTable, sampleNumber);
		if (e) return e;
	}
	//remove size
	e = stbl_RemoveSize(trak->Media->information->sampleTable->SampleSize, sampleNumber);
	if (e) return e;
	//remove sampleToChunk and chunk
	e = stbl_RemoveChunk(trak->Media->information->sampleTable, sampleNumber);
	if (e) return e;
	//remove sync
	if (trak->Media->information->sampleTable->SyncSample) {
		e = stbl_RemoveRAP(trak->Media->information->sampleTable, sampleNumber);
		if (e) return e;
	}
	//remove sample dep
	if (trak->Media->information->sampleTable->SampleDep) {
		e = stbl_RemoveRedundant(trak->Media->information->sampleTable, sampleNumber);
		if (e) return e;
	}
	//remove shadow
	if (trak->Media->information->sampleTable->ShadowSync) {
		e = stbl_RemoveShadow(trak->Media->information->sampleTable->ShadowSync, sampleNumber);
		if (e) return e;
	}
	//remove padding
	e = stbl_RemovePaddingBits(trak->Media->information->sampleTable, sampleNumber);
	if (e) return e;
	
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
	}
	return GF_OK;
}

//Add a system descriptor to the ESD of a stream(EDIT or WRITE mode only)
GF_Err gf_isom_add_desc_to_description(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, GF_Descriptor *theDesc)
{
	GF_IPIPtr *ipiD;
	GF_Err e;
	u16 tmpRef;
	GF_TrackBox *trak;
	GF_Descriptor *desc;
	GF_ESD *esd;
	GF_TrackReferenceBox *tref;
	GF_TrackReferenceTypeBox *dpnd;
	
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	/*GETS NATIVE DESCRIPTOR ONLY*/
	e = Media_GetESD(trak->Media, StreamDescriptionIndex, &esd, 1);
	if (e) return e;

	//duplicate the desc
	e = gf_odf_desc_copy(theDesc, &desc);
	if (e) return e;

	//and add it to the ESD EXCEPT IPI PTR (we need to translate from ES_ID to TrackID!!!
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	switch (desc->tag) {
	case GF_ODF_IPI_PTR_TAG:
		goto insertIPI;
	
	default:
		return gf_odf_desc_add_desc((GF_Descriptor *)esd, desc);
	}

	
insertIPI:
	if (esd->ipiPtr) {
		gf_odf_desc_del((GF_Descriptor *) esd->ipiPtr);
		esd->ipiPtr = NULL;
	}

	ipiD = (GF_IPIPtr *) desc;	
	//find a tref
	if (!trak->References) {
		tref = (GF_TrackReferenceBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TREF);
		e = trak_AddBox((GF_Box*)trak, (GF_Box *)tref);
		if (e) return e;
	}
	tref = trak->References;

	e = Track_FindRef(trak, GF_ISOM_REF_IPI, &dpnd);
	if (e) return e;
	if (!dpnd) {
		tmpRef = 0;
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_REFT);
		dpnd->reference_type = GF_ISOM_BOX_TYPE_IPIR;
		e = tref_AddBox((GF_Box*)tref, (GF_Box *) dpnd);
		if (e) return e;
		e = reftype_AddRefTrack(dpnd, ipiD->IPI_ES_Id, &tmpRef);
		if (e) return e;
		//and replace the tag and value...
		ipiD->IPI_ES_Id = tmpRef;
		ipiD->tag = GF_ODF_ISOM_IPI_PTR_TAG;
	} else {
		//Watch out! ONLY ONE IPI dependancy is allowed per stream
		dpnd->trackIDCount = 1;
		dpnd->trackIDs[0] = ipiD->IPI_ES_Id;
		//and replace the tag and value...
		ipiD->IPI_ES_Id = 1;
		ipiD->tag = GF_ODF_ISOM_IPI_PTR_TAG;
	}
	//and add the desc to the esd...
	return gf_odf_desc_add_desc((GF_Descriptor *)esd, desc);
}


//use carefully. Very usefull when you made a lot of changes (IPMP, IPI, OCI, ...)
//THIS WILL REPLACE THE WHOLE DESCRIPTOR ...
GF_EXPORT
GF_Err gf_isom_change_mpeg4_description(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, GF_ESD *newESD)
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

	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->other_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->other_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;

	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	//duplicate our desc
	e = gf_odf_desc_copy((GF_Descriptor *)newESD, (GF_Descriptor **)&esd);
	if (e) return e;
	return Track_SetStreamDescriptor(trak, StreamDescriptionIndex, entry->dataReferenceIndex, esd, NULL);
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
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->other_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->other_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	//valid for MPEG visual, JPG and 3GPP H263
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_MP4V:
	case GF_ISOM_SUBTYPE_3GP_H263:
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_HVC1: 
	case GF_ISOM_BOX_TYPE_HEV1: 
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_SHC1:
	case GF_ISOM_BOX_TYPE_SHV1:
		((GF_VisualSampleEntryBox*)entry)->Width = Width;
		((GF_VisualSampleEntryBox*)entry)->Height = Height;
		trak->Header->width = Width<<16;
		trak->Header->height = Height<<16;
		return GF_OK;
	/*check BIFS*/
	default:
		if (trak->Media->handler->handlerType==GF_ISOM_MEDIA_SCENE) {
			trak->Header->width = Width<<16;
			trak->Header->height = Height<<16;
			return GF_OK;
		}
		return GF_BAD_PARAM;
	}
}

GF_Err gf_isom_set_pixel_aspect_ratio(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 hSpacing, u32 vSpacing)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_VisualSampleEntryBox*vent;
	GF_SampleDescriptionBox *stsd;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->other_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->other_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_MP4V:
	case GF_ISOM_SUBTYPE_3GP_H263:
	case GF_ISOM_BOX_TYPE_AVC1:
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_HVC1: 
	case GF_ISOM_BOX_TYPE_HEV1: 
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_SHC1:
	case GF_ISOM_BOX_TYPE_SHV1:
		break;
	default:
		return GF_BAD_PARAM;
	}

	vent = (GF_VisualSampleEntryBox*)entry;
	if (!hSpacing || !vSpacing) {
		if (vent->pasp) gf_isom_box_del((GF_Box*)vent->pasp);
		vent->pasp = NULL;
		return GF_OK;
	}
	if (!vent->pasp) vent->pasp = (GF_PixelAspectRatioBox*)gf_isom_box_new(GF_ISOM_BOX_TYPE_PASP);
	vent->pasp->hSpacing = hSpacing;
	vent->pasp->vSpacing = vSpacing;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_audio_info(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 sampleRate, u32 nbChannels, u8 bitsPerSample)
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
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->other_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->other_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_MP4A:
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
		((GF_AudioSampleEntryBox*)entry)->samplerate_hi = sampleRate;
		((GF_AudioSampleEntryBox*)entry)->samplerate_lo = 0;
		((GF_AudioSampleEntryBox*)entry)->channel_count = nbChannels;
		((GF_AudioSampleEntryBox*)entry)->bitspersample = bitsPerSample;
		return GF_OK;
	/*check BIFS*/
	default:
		return GF_BAD_PARAM;
	}
}

//set the storage mode of a file (FLAT, STREAMABLE, INTERLEAVED)
GF_EXPORT
GF_Err gf_isom_set_storage_mode(GF_ISOFile *movie, u8 storageMode)
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
		movie->storageMode = storageMode;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}


//update or insert a new edit segment in the track time line. Edits are used to modify
//the media normal timing. EditTime and EditDuration are expressed in Movie TimeScale
//If a segment with EditTime already exists, IT IS ERASED
GF_EXPORT
GF_Err gf_isom_set_edit_segment(GF_ISOFile *movie, u32 trackNumber, u64 EditTime, u64 EditDuration, u64 MediaTime, u8 EditMode)
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
		edts = (GF_EditBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_EDTS);
		if (!edts) return GF_OUT_OF_MEM;
		trak_AddBox((GF_Box*)trak, (GF_Box *)edts);
	}
	elst = edts->editList;
	if (!elst) {
		elst = (GF_EditListBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_ELST);
		if (!elst) return GF_OUT_OF_MEM;
		edts_AddBox((GF_Box*)edts, (GF_Box *)elst);
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
		gf_list_add(elst->entryList, newEnt);
		return SetTrackDuration(trak);
	}
	
	startTime -= ent->segmentDuration;

found:
	
	//if same time, we erase the current one...
	if (startTime == EditTime) {
		ent->segmentDuration = EditDuration;
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
		return SetTrackDuration(trak);
	}

	//adjust so that the prev ent leads to EntryTime
	//Note: we don't change the next one as it is unknown to us in
	//a lot of case (the author's changes)
	ent->segmentDuration = EditTime - startTime;
	newEnt = CreateEditEntry(EditDuration, MediaTime, EditMode);
	if (!newEnt) return GF_OUT_OF_MEM;
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

//remove the edit segments for the whole track
GF_EXPORT
GF_Err gf_isom_remove_edit_segments(GF_ISOFile *movie, u32 trackNumber)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_EdtsEntry *ent;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!trak->editBox || !trak->editBox->editList) return GF_OK;

	while (gf_list_count(trak->editBox->editList->entryList)) {
		ent = (GF_EdtsEntry*)gf_list_get(trak->editBox->editList->entryList, 0);
		gf_free(ent);
		e = gf_list_rem(trak->editBox->editList->entryList, 0);
		if (e) return e;
	}
	//then delete the GF_EditBox...
	gf_isom_box_del((GF_Box *)trak->editBox);
	trak->editBox = NULL;
	return SetTrackDuration(trak);
}


//remove the edit segments for the whole track
GF_Err gf_isom_remove_edit_segment(GF_ISOFile *movie, u32 trackNumber, u32 seg_index)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_EdtsEntry *ent, *next_ent;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !seg_index) return GF_BAD_PARAM;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!trak->editBox || !trak->editBox->editList) return GF_OK;
	if (gf_list_count(trak->editBox->editList->entryList)<=1) return gf_isom_remove_edit_segments(movie, trackNumber);

	ent = (GF_EdtsEntry*) gf_list_get(trak->editBox->editList->entryList, seg_index-1);
	gf_list_rem(trak->editBox->editList->entryList, seg_index-1);
	next_ent = (GF_EdtsEntry *)gf_list_get(trak->editBox->editList->entryList, seg_index-1);
	if (next_ent) next_ent->segmentDuration += ent->segmentDuration;
	gf_free(ent);
	return SetTrackDuration(trak);
}


GF_EXPORT
GF_Err gf_isom_append_edit_segment(GF_ISOFile *movie, u32 trackNumber, u64 EditDuration, u64 MediaTime, u8 EditMode)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_EdtsEntry *ent;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!trak->editBox) {
		GF_EditBox *edts = (GF_EditBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_EDTS);
		if (!edts) return GF_OUT_OF_MEM;
		trak_AddBox((GF_Box*)trak, (GF_Box *)edts);
	}
	if (!trak->editBox->editList) {
		GF_EditListBox *elst = (GF_EditListBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_ELST);
		if (!elst) return GF_OUT_OF_MEM;
		edts_AddBox((GF_Box*)trak->editBox, (GF_Box *)elst);
	}
	ent = (GF_EdtsEntry *)gf_malloc(sizeof(GF_EdtsEntry));
	if (!ent) return GF_OUT_OF_MEM;

	ent->segmentDuration = EditDuration;
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
	gf_list_add(trak->editBox->editList->entryList, ent);
	return SetTrackDuration(trak);
}

GF_EXPORT
GF_Err gf_isom_modify_edit_segment(GF_ISOFile *movie, u32 trackNumber, u32 seg_index, u64 EditDuration, u64 MediaTime, u8 EditMode)
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
	return SetTrackDuration(trak);
}

//removes the desired track
GF_EXPORT
GF_Err gf_isom_remove_track(GF_ISOFile *movie, u32 trackNumber)
{
	GF_Err e;
	GF_TrackBox *the_trak, *trak;
	GF_TrackReferenceTypeBox *tref;
	u32 i, j, k, *newRefs, descIndex;
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
				e = gf_isom_update_sample(movie, i, k+1, samp, 1);
				if (e) return e;
			}
			//and don't forget to delete the sample
			gf_isom_sample_del(&samp);
		}
	}

	//remove the track ref from any "tref" box in all tracks (except the one to delete ;)
	i=0;
	while ((trak = (GF_TrackBox *)gf_list_enum(movie->moov->trackList, &i))) {
		if (trak == the_trak) continue;
		if (! trak->References || ! gf_list_count(trak->References->other_boxes)) continue;
		
		j=0;
		while ((tref = (GF_TrackReferenceTypeBox *)gf_list_enum(trak->References->other_boxes, &j))) {
			found = 0;
			for (k=0; k<tref->trackIDCount; k++) {
				if (tref->trackIDs[k] == the_trak->Header->trackID) found++;
			}
			if (!found) continue;
			//no more refs, remove this ref_type
			if (found == tref->trackIDCount) {
				gf_isom_box_del((GF_Box *)tref);
				j--;
				gf_list_rem(trak->References->other_boxes, j);
			} else {
				newRefs = (u32*)gf_malloc(sizeof(u32) * (tref->trackIDCount - found));
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
		if (! gf_list_count(trak->References->other_boxes)) {
			gf_isom_box_del((GF_Box *)trak->References);
			trak->References = NULL;
		}
	}

	//delete the track
	gf_isom_box_del((GF_Box *)the_trak);
	
	/*update next track ID*/
	movie->moov->mvhd->nextTrackID = 0;
	i=0;
	while ((trak = (GF_TrackBox *)gf_list_enum(movie->moov->trackList, &i))) {
		if (trak->Header->trackID>movie->moov->mvhd->nextTrackID)
			movie->moov->mvhd->nextTrackID = trak->Header->trackID;
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

	gf_isom_insert_moov(movie);

	if (!movie->moov->udta) {
		e = moov_AddBox((GF_Box*)movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		if (e) return e;
	}
	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_CPRT, NULL);
	
	if (map) {
		//try to find one in our language...
		count = gf_list_count(map->other_boxes);
		for (i=0; i<count; i++) {
			ptr = (GF_CopyrightBox*)gf_list_get(map->other_boxes, i);
			if (!strcmp(threeCharCode, (const char *) ptr->packedLanguageCode)) {
				gf_free(ptr->notice);
				ptr->notice = (char*)gf_malloc(sizeof(char) * (strlen(notice) + 1));
				strcpy(ptr->notice, notice);
				return GF_OK;
			}
		}
	}
	//nope, create one
	ptr = (GF_CopyrightBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_CPRT);

	memcpy(ptr->packedLanguageCode, threeCharCode, 4);
	ptr->notice = (char*)gf_malloc(sizeof(char) * (strlen(notice)+1));
	strcpy(ptr->notice, notice);
	return udta_AddBox(movie->moov->udta, (GF_Box *) ptr);
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

	gf_isom_insert_moov(movie);

	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) {
			e = trak_AddBox((GF_Box*)trak, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
			if (e) return e;
		}
		udta = trak->udta;
	} else {
		if (!movie->moov->udta) {
			e = moov_AddBox((GF_Box*)movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
			if (e) return e;
		}
		udta = movie->moov->udta;
	}

	ptr = NULL;
	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_CHPL, NULL);
	if (!map) {
		ptr = (GF_ChapterListBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_CHPL);
		e = udta_AddBox(udta, (GF_Box *) ptr);
		if (e) return e;
	} else {
		ptr = (GF_ChapterListBox*)gf_list_get(map->other_boxes, 0);
	}
	/*this may happen if original MP4 is not properly formatted*/
	if (!ptr) {
		ptr = (GF_ChapterListBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_CHPL);
		gf_list_add(map->other_boxes, ptr);
	}

	GF_SAFEALLOC(ce, GF_ChapterEntry);
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


GF_Err gf_isom_remove_chapter(GF_ISOFile *movie, u32 trackNumber, u32 index)
{
	GF_Err e;
	GF_ChapterListBox *ptr;
	GF_ChapterEntry *ce;
	GF_UserDataBox *udta;
	GF_UserDataMap *map;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(movie);

	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) {
			e = trak_AddBox((GF_Box*)trak, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
			if (e) return e;
		}
		udta = trak->udta;
	} else {
		if (!movie->moov->udta) {
			e = moov_AddBox((GF_Box*)movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
			if (e) return e;
		}
		udta = movie->moov->udta;
	}

	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_CHPL, NULL);
	if (!map) return GF_OK;
	ptr = (GF_ChapterListBox*)gf_list_get(map->other_boxes, 0);
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
		gf_isom_box_array_del(map->other_boxes);
		gf_free(map);
	}
	return GF_OK;
}

GF_Err gf_isom_remove_copyright(GF_ISOFile *movie, u32 index)
{
	GF_Err e;
	GF_CopyrightBox *ptr;
	GF_UserDataMap *map;
	u32 count;
	
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(movie);

	if (!index) return GF_BAD_PARAM;
	if (!movie->moov->udta) return GF_OK;

	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_CPRT, NULL);
	if (!map) return GF_OK;

	count = gf_list_count(map->other_boxes);
	if (index>count) return GF_BAD_PARAM;

	ptr = (GF_CopyrightBox*)gf_list_get(map->other_boxes, index-1);
	if (ptr) {
		gf_list_rem(map->other_boxes, index-1);
		if (ptr->notice) gf_free(ptr->notice);
		gf_free(ptr);
	}
	/*last copyright, remove*/
	if (!gf_list_count(map->other_boxes)) {
		gf_list_del_item(movie->moov->udta->recordList, map);
		gf_list_del(map->other_boxes);
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

	gf_isom_insert_moov(movie);
	if (!movie->moov->udta) {
		e = moov_AddBox((GF_Box*)movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		if (e) return e;
	}
	
	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_UUID, (bin128 *) & UUID);
	if (map) {
		ptr = (GF_UnknownUUIDBox *)gf_list_get(map->other_boxes, 0);
		if (ptr) {
			gf_free(ptr->data);
			ptr->data = (char*)gf_malloc(length);
			memcpy(ptr->data, data, length);
			ptr->dataSize = length;
			return GF_OK;
		}
	}
	//nope, create one
	ptr = (GF_UnknownUUIDBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_UUID);
	memcpy(ptr->uuid, UUID, 16);
	ptr->data = (char*)gf_malloc(length);
	memcpy(ptr->data, data, length);
	ptr->dataSize = length;
	return udta_AddBox(movie->moov->udta, (GF_Box *) ptr);
}

//set the interleaving time of media data (INTERLEAVED mode only)
//InterleaveTime is in MovieTimeScale
GF_Err gf_isom_set_interleave_time(GF_ISOFile *movie, u32 InterleaveTime)
{
	GF_Err e;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!InterleaveTime || !movie->moov) return GF_OK;
	movie->interleavingTime = InterleaveTime;
	return GF_OK;
}

u32 gf_isom_get_interleave_time(GF_ISOFile *movie)
{
	return movie ? movie->interleavingTime : 0;
}

//set the storage mode of a file (FLAT, STREAMABLE, INTERLEAVED)
u8 gf_isom_get_storage_mode(GF_ISOFile *movie)
{
	return movie ? movie->storageMode : 0;
}




//use a compact track version for sample size. This is not usually recommended 
//except for speech codecs where the track has a lot of small samples
//compaction is done automatically while writing based on the track's sample sizes
GF_Err gf_isom_use_compact_size(GF_ISOFile *movie, u32 trackNumber, u8 CompactionOn)
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
		gf_list_add(movie->TopBoxes, movie->brand);
	}
	
	movie->brand->majorBrand = MajorBrand;
	movie->brand->minorVersion = MinorVersion;

	if (!movie->brand->altBrand) {
		movie->brand->altBrand = (u32*)gf_malloc(sizeof(u32));
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
GF_Err gf_isom_modify_alternate_brand(GF_ISOFile *movie, u32 Brand, u8 AddIt)
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
		gf_list_add(movie->TopBoxes, movie->brand);
	}

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
	memcpy(p, movie->brand->altBrand, sizeof(u32)*movie->brand->altCount);
	p[movie->brand->altCount] = Brand;
	movie->brand->altCount += 1;
	gf_free(movie->brand->altBrand);
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


GF_Err gf_isom_reset_alt_brands(GF_ISOFile *movie)
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
		gf_list_add(movie->TopBoxes, movie->brand);
	}
	p = (u32*)gf_malloc(sizeof(u32));
	if (!p) return GF_OUT_OF_MEM;
	p[0] = movie->brand->majorBrand;
	movie->brand->altCount = 1;
	gf_free(movie->brand->altBrand);
	movie->brand->altBrand = p;
	return GF_OK;
}

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

	if (UserDataIndex > gf_list_count(map->other_boxes) ) return GF_BAD_PARAM;
	//delete the box
	a = (GF_Box*)gf_list_get(map->other_boxes, UserDataIndex-1);
	
	gf_list_rem(map->other_boxes, UserDataIndex-1);
	gf_isom_box_del(a);

	//remove the map if empty
	if (!gf_list_count(map->other_boxes)) {
		gf_list_rem(udta->recordList, i-1);
		gf_isom_box_array_del(map->other_boxes);
		gf_free(map);
	}
	//but we keep the UDTA no matter what
	return GF_OK;
}

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
		if (!trak) return GF_BAD_PARAM;
		udta = trak->udta;
	} else {
		udta = movie->moov->udta;
	}
	if (!udta) return GF_BAD_PARAM;

	i=0;
	while ((map = (GF_UserDataMap*)gf_list_enum(udta->recordList, &i))) {
		if ((map->boxType == GF_ISOM_BOX_TYPE_UUID)  && !memcmp(map->uuid, UUID, 16)) goto found;
		else if (map->boxType == UserDataType) goto found;
	}
	//not found
	return GF_OK;

found:

	gf_list_rem(udta->recordList, i-1);
	gf_isom_box_array_del(map->other_boxes);
	gf_free(map);

	//but we keep the UDTA no matter what
	return GF_OK;
}

GF_Err gf_isom_add_user_data(GF_ISOFile *movie, u32 trackNumber, u32 UserDataType, bin128 UUID, char *data, u32 DataLength)
{
	GF_UnknownBox *a;
	GF_Err e;
	GF_TrackBox *trak;
	GF_UserDataBox *udta;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	if (UserDataType == GF_ISOM_BOX_TYPE_UUID) UserDataType = 0;

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) trak_AddBox((GF_Box*)trak, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		udta = trak->udta;
	} else {
		if (!movie->moov->udta) moov_AddBox((GF_Box*)movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		udta = movie->moov->udta;
	}
	if (!udta) return GF_OUT_OF_MEM;

	//create a default box
	if (UserDataType) {
		a = (GF_UnknownBox *) gf_isom_box_new(UserDataType);
	} else {
		a = (GF_UnknownBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_UUID);
		memcpy( ((GF_UUIDBox*)a)->uuid, UUID, 16);
	}

	if (DataLength) {
		a->data = (char*)gf_malloc(sizeof(char)*DataLength);
		memcpy(a->data, data, DataLength);
		a->dataSize = DataLength;
	}
	return udta_AddBox(udta, (GF_Box *) a);
}


GF_Err gf_isom_add_sample_fragment(GF_ISOFile *movie, u32 trackNumber, u32 sampleNumber, u16 FragmentSize)
{
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !sampleNumber || !FragmentSize) return GF_BAD_PARAM;

	//set Padding info
	return stbl_AddSampleFragment(trak->Media->information->sampleTable, sampleNumber, FragmentSize);
}


GF_EXPORT
GF_Err gf_isom_remove_sample_fragment(GF_ISOFile *movie, u32 trackNumber, u32 sampleNumber)
{
	GF_TrackBox *trak;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	return stbl_RemoveSampleFragments(trak->Media->information->sampleTable, sampleNumber);
}

GF_Err gf_isom_remove_sample_fragments(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (trak->Media->information->sampleTable->Fragments) {
		gf_isom_box_del((GF_Box *)trak->Media->information->sampleTable->Fragments);
		trak->Media->information->sampleTable->Fragments = NULL;
	}
	return GF_OK;
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

GF_Err gf_isom_clone_box(GF_Box *src, GF_Box **dst)
{
	GF_Err e;
	char *data;
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
	if (e) return e;
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	if (!bs) return GF_OUT_OF_MEM;
	e = gf_isom_parse_box(dst, bs);
	gf_bs_del(bs);
	gf_free(data);
	return e;
}

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
		gf_isom_clone_box((GF_Box *)orig_file->moov, (GF_Box **)&dest_file->moov);
		orig_file->moov->trackList = old_tracks;
		gf_list_del(tracks);
		orig_file->moov->iods = (GF_ObjectDescriptorBox*)iods;
		gf_list_add(dest_file->TopBoxes, dest_file->moov);

		if (clone_tracks) { 
			for (i=0; i<gf_list_count(orig_file->moov->trackList); i++) {
				GF_TrackBox *trak = gf_list_get( orig_file->moov->trackList, i);
				if (!trak) continue;
				if (keep_hint_tracks || (trak->Media->handler->handlerType != GF_ISOM_MEDIA_HINT)) {
					e = gf_isom_clone_track(orig_file, i+1, dest_file, 1, &dstTrack);
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
		while ((box = (GF_Box*)gf_list_get(dest_file->moov->other_boxes, i++))) {
			if (box->type == GF_ISOM_BOX_TYPE_PSSH) {
				i--;
				gf_list_rem(dest_file->moov->other_boxes, i);
				gf_isom_box_del(box);
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
			case GF_ISOM_BOX_TYPE_MOOF:
#endif
			case GF_4CC('j','P',' ',' '):
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


GF_EXPORT
GF_Err gf_isom_clone_track(GF_ISOFile *orig_file, u32 orig_track, GF_ISOFile *dest_file, Bool keep_data_ref, u32 *dest_track)
{
	GF_TrackBox *trak, *new_tk;
	GF_BitStream *bs;
	char *data;
	const char *buffer;
	u32 data_size;
	Double ts_scale;
	GF_Err e;
	GF_SampleEntryBox *entry;
	GF_SampleTableBox *stbl, *stbl_temp;
	
	e = CanAccessMovie(dest_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(dest_file);

	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(orig_file, orig_track);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	stbl_temp = (GF_SampleTableBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STBL);
	/*clone sampleDescription table*/
	stbl_temp->SampleDescription = stbl->SampleDescription;
	/*also clone sampleGroups description tables if any*/
	stbl_temp->sampleGroupsDescription = stbl->sampleGroupsDescription;
	trak->Media->information->sampleTable = stbl_temp;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_isom_box_size( (GF_Box *) trak);
	gf_isom_box_write((GF_Box *) trak, bs);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	e = gf_isom_parse_box((GF_Box **) &new_tk, bs);
	gf_bs_del(bs);
	gf_free(data);
	trak->Media->information->sampleTable = stbl;

	stbl_temp->SampleDescription = NULL;
	stbl_temp->sampleGroupsDescription = NULL;
	gf_isom_box_del((GF_Box *)stbl_temp);
	if (e) return e;

	/*create default boxes*/
	stbl = new_tk->Media->information->sampleTable;
	stbl->ChunkOffset = gf_isom_box_new(GF_ISOM_BOX_TYPE_STCO);
	stbl->SampleSize = (GF_SampleSizeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSZ);
	stbl->SampleToChunk = (GF_SampleToChunkBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSC);
	stbl->TimeToSample = (GF_TimeToSampleBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STTS);

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

	moov_AddBox((GF_Box*)dest_file->moov, (GF_Box *)new_tk);

	/*set originalID*/
	new_tk->originalID = trak->Header->trackID;
	/*set originalFile*/
	buffer = gf_isom_get_filename(orig_file);
	new_tk->originalFile = gf_crc_32(buffer, sizeof(buffer));

	/*rewrite edit list segmentDuration to new movie timescale*/
	ts_scale = dest_file->moov->mvhd->timeScale;
	ts_scale /= orig_file->moov->mvhd->timeScale;
	new_tk->Header->duration = (u64) (s64) ((s64) new_tk->Header->duration * ts_scale);
	if (new_tk->editBox && new_tk->editBox->editList) {
		u32 i, count = gf_list_count(new_tk->editBox->editList->entryList);
		for (i=0; i<count; i++) {
			GF_EdtsEntry *ent = (GF_EdtsEntry *)gf_list_get(new_tk->editBox->editList->entryList, i);
			ent->segmentDuration = (u64) (s64) ((s64) ent->segmentDuration * ts_scale);
		}
	}

	/*reset data ref*/
	if (!keep_data_ref) {
		gf_isom_box_array_del(new_tk->Media->information->dataInformation->dref->other_boxes);
		new_tk->Media->information->dataInformation->dref->other_boxes = gf_list_new();
		/*update data ref*/
		entry = (GF_SampleEntryBox*)gf_list_get(new_tk->Media->information->sampleTable->SampleDescription->other_boxes, 0);
		if (entry) {
			u32 dref;
			Media_CreateDataRef(new_tk->Media->information->dataInformation->dref, NULL, NULL, &dref);
			entry->dataReferenceIndex = dref;
		}
	}

	*dest_track = gf_list_count(dest_file->moov->trackList);

	if (dest_file->moov->mvhd->nextTrackID<= new_tk->Header->trackID) 
		dest_file->moov->mvhd->nextTrackID = new_tk->Header->trackID+1;

	return GF_OK;
}

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
		gf_isom_box_array_del(dst_trak->Media->information->sampleTable->SampleDescription->other_boxes);
		dst_trak->Media->information->sampleTable->SampleDescription->other_boxes = gf_list_new();
	}

	for (i=0; i<gf_list_count(src_trak->Media->information->sampleTable->SampleDescription->other_boxes); i++) {
		u32 outDesc;
		e = gf_isom_clone_sample_description(the_file, trackNumber, orig_file, orig_track, i+1, NULL, NULL, &outDesc);
		if (e) break;
	}
	return e;
}


GF_EXPORT
GF_Err gf_isom_clone_sample_description(GF_ISOFile *the_file, u32 trackNumber, GF_ISOFile *orig_file, u32 orig_track, u32 orig_desc_index, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_BitStream *bs;
	char *data;
	u32 data_size;
	GF_Box *entry;
	GF_Err e;
	u32 dataRefIndex;
	
	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(orig_file, orig_track);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	entry = (GF_Box*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, orig_desc_index-1);
	if (!entry) return GF_BAD_PARAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_isom_box_size(entry);
	gf_isom_box_write(entry, bs);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	e = gf_isom_parse_box(&entry, bs);
	gf_bs_del(bs);
	gf_free(data);
	if (e) return e;

	/*get new track and insert clone*/
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) goto exit;

	/*get or create the data ref*/
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) goto exit;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) goto exit;
	}
	if (!the_file->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	/*overwrite dref*/
	((GF_SampleEntryBox *)entry)->dataReferenceIndex = dataRefIndex;
	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, entry);
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);

	/*also clone track w/h info*/	
	if (gf_isom_get_media_type(the_file, trackNumber) == GF_ISOM_MEDIA_VISUAL) {
		gf_isom_set_visual_info(the_file, trackNumber, (*outDescriptionIndex), ((GF_VisualSampleEntryBox*)entry)->Width, ((GF_VisualSampleEntryBox*)entry)->Height);
	}
	return e;
	
exit:
	gf_isom_box_del(entry);
	return e;
}


GF_Err gf_isom_new_generic_sample_description(GF_ISOFile *movie, u32 trackNumber, char *URLname, char *URNname, GF_GenericSampleDescription *udesc, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !udesc) return GF_BAD_PARAM;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (trak->Media->handler->handlerType==GF_ISOM_MEDIA_VISUAL) {
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
		e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, entry);
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
		gena->dataReferenceIndex = dataRefIndex;
		gena->vendor = udesc->vendor_code;
		gena->version = udesc->version;
		gena->revision = udesc->revision;
		gena->bitspersample = udesc->bits_per_sample ? udesc->bits_per_sample : 16;
		gena->channel_count = udesc->nb_channels ? udesc->nb_channels : 2;
		gena->samplerate_hi = udesc->samplerate>>16;
		gena->samplerate_lo = udesc->samplerate & 0xFF;

		if (udesc->extension_buf && udesc->extension_buf_size) {
			gena->data = (char*)gf_malloc(sizeof(char) * udesc->extension_buf_size);
			if (!gena->data) {
				gf_isom_box_del((GF_Box *) gena);
				return GF_OUT_OF_MEM;
			}
			memcpy(gena->data, udesc->extension_buf, udesc->extension_buf_size);
			gena->data_size = udesc->extension_buf_size;
		}
		e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, gena);
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
		e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, genm);
	}
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);
	return e;
}

//use carefully. Very usefull when you made a lot of changes (IPMP, IPI, OCI, ...)
//THIS WILL REPLACE THE WHOLE DESCRIPTOR ...
GF_Err gf_isom_change_generic_sample_description(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, GF_GenericSampleDescription *udesc)
{
	GF_TrackBox *trak;
	GF_Err e;
	GF_GenericVisualSampleEntryBox *entry;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !StreamDescriptionIndex) return GF_BAD_PARAM;

	entry = (GF_GenericVisualSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, StreamDescriptionIndex-1);
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
		gena->samplerate_hi = udesc->samplerate>>16;
		gena->samplerate_lo = udesc->samplerate & 0xFF;
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
	entry = (GF_Box*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, streamDescIndex-1);
	if (!entry) return GF_BAD_PARAM;
	gf_list_rem(trak->Media->information->sampleTable->SampleDescription->other_boxes, streamDescIndex-1);
	gf_isom_box_del(entry);
	return GF_OK;
}


//sets a track reference
GF_EXPORT
GF_Err gf_isom_set_track_reference(GF_ISOFile *the_file, u32 trackNumber, u32 referenceType, u32 ReferencedTrackID)
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
		tref = (GF_TrackReferenceBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TREF);
		e = trak_AddBox((GF_Box*)trak, (GF_Box *) tref);
		if (e) return e;
	}
	//find a ref of the given type
	e = Track_FindRef(trak, referenceType, &dpnd);
	if (!dpnd) {
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_REFT);
		dpnd->reference_type = referenceType;
		e = tref_AddBox((GF_Box*)tref, (GF_Box *)dpnd);
		if (e) return e;
	}
	//add the ref
	return reftype_AddRefTrack(dpnd, ReferencedTrackID, NULL);
}

//removes a track reference
GF_EXPORT
GF_Err gf_isom_remove_track_reference(GF_ISOFile *the_file, u32 trackNumber, u32 referenceType, u32 ReferenceIndex)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_TrackReferenceBox *tref;
	GF_TrackReferenceTypeBox *dpnd, *tmp;
	u32 i, k, *newIDs;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !ReferenceIndex) return GF_BAD_PARAM;

	//no tref, nothing to remove
	tref = trak->References;
	if (!tref) return GF_OK;
	//find a ref of the given type otherwise return
	e = Track_FindRef(trak, referenceType, &dpnd);
	if (e || !dpnd) return GF_OK;
	//remove the ref
	if (ReferenceIndex > dpnd->trackIDCount) return GF_BAD_PARAM;
	//last one
	if (dpnd->trackIDCount==1) {
		i=0;
		while ((tmp = (GF_TrackReferenceTypeBox *)gf_list_enum(tref->other_boxes, &i))) {
			if (tmp==dpnd) {
				gf_list_rem(tref->other_boxes, i-1);
				gf_isom_box_del((GF_Box *) dpnd);
				return GF_OK;
			}
		}
	}
	k = 0;
	newIDs = (u32*)gf_malloc(sizeof(u32)*(dpnd->trackIDCount-1));
	for (i=0; i<dpnd->trackIDCount; i++) {
		if (i+1 != ReferenceIndex) {
			newIDs[k] = dpnd->trackIDs[i];
			k++;
		}
	}
	gf_free(dpnd->trackIDs);
	dpnd->trackIDCount -= 1;
	dpnd->trackIDs = newIDs;
	return GF_OK;
}


//changes track ID
GF_Err gf_isom_set_track_id(GF_ISOFile *movie, u32 trackNumber, u32 trackID)
{
	GF_TrackReferenceTypeBox *ref;
	GF_TrackBox *trak, *a_trak;
	u32 i, j, k;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (trak && (trak->Header->trackID==trackID)) return GF_OK;
	a_trak = gf_isom_get_track_from_id(movie->moov, trackID);
	if (!movie || !trak || a_trak) return GF_BAD_PARAM;

	if (movie->moov->mvhd->nextTrackID<=trackID)
		movie->moov->mvhd->nextTrackID = trackID;

	/*rewrite all dependencies*/
	i=0;
	while ((a_trak = (GF_TrackBox*)gf_list_enum(movie->moov->trackList, &i))) {
		if (!a_trak->References) continue;
		j=0;
		while ((ref = (GF_TrackReferenceTypeBox *)gf_list_enum(a_trak->References->other_boxes, &j))) {
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
		u32 i = 0;
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
	while ((ref = (GF_TrackReferenceTypeBox *)gf_list_enum(trak->References->other_boxes, &i))) {
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

GF_Err gf_isom_remove_cts_info(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->CompositionOffset) return GF_OK;
	
	gf_isom_box_del((GF_Box *)trak->Media->information->sampleTable->CompositionOffset);
	trak->Media->information->sampleTable->CompositionOffset = NULL;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_cts_packing(GF_ISOFile *the_file, u32 trackNumber, Bool unpack)
{
	GF_Err e;
	GF_Err stbl_repackCTS(GF_CompositionOffsetBox *ctts);
	GF_Err stbl_unpackCTS(GF_SampleTableBox *stbl);

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (unpack) {
		if (!trak->Media->information->sampleTable->CompositionOffset) trak->Media->information->sampleTable->CompositionOffset = (GF_CompositionOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CTTS);
		e = stbl_unpackCTS(trak->Media->information->sampleTable);
	} else {
		if (!trak->Media->information->sampleTable->CompositionOffset) return GF_OK;
		e = stbl_repackCTS(trak->Media->information->sampleTable->CompositionOffset);
	}
	if (e) return e;
	return SetTrackDuration(trak);
}

GF_EXPORT
GF_Err gf_isom_set_track_matrix(GF_ISOFile *the_file, u32 trackNumber, u32 matrix[9])
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

GF_Err gf_isom_set_track_name(GF_ISOFile *the_file, u32 trackNumber, char *name)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (trak->name) gf_free(trak->name);
	trak->name = NULL;
	if (name) trak->name = gf_strdup(name);
	return GF_OK;
}
const char *gf_isom_get_track_name(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return NULL;
	return trak->name;
}


GF_Err gf_isom_store_movie_config(GF_ISOFile *movie, Bool remove_all)
{
	u32 i, count, len;
	char *data;
	GF_BitStream *bs;
	bin128 binID;
	if (movie == NULL) return GF_BAD_PARAM;

	gf_isom_remove_user_data(movie, 0, GF_4CC('G','P','A','C'), binID);
	count = gf_isom_get_track_count(movie);
	for (i=0; i<count; i++) gf_isom_remove_user_data(movie, i+1, GF_4CC('G','P','A','C'), binID);

	if (remove_all) return GF_OK;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	/*update movie: storage mode and interleaving type*/
	gf_bs_write_u8(bs, 0xFE); /*marker*/
	gf_bs_write_u8(bs, movie->storageMode);
	gf_bs_write_u32(bs, movie->interleavingTime);
	gf_bs_get_content(bs, &data, &len);
	gf_bs_del(bs);
	gf_isom_add_user_data(movie, 0, GF_4CC('G','P','A','C'), binID, data, len);
	gf_free(data);
	/*update tracks: interleaving group/priority and track edit name*/
	for (i=0; i<count; i++) {
		u32 j;
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, i+1);
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u8(bs, 0xFE);	/*marker*/
		gf_bs_write_u32(bs, trak->Media->information->sampleTable->groupID);
		gf_bs_write_u32(bs, trak->Media->information->sampleTable->trackPriority);
		len = trak->name ? (u32) strlen(trak->name) : 0;
		gf_bs_write_u32(bs, len);
		for (j=0; j<len; j++) gf_bs_write_u8(bs, trak->name[j]);
		gf_bs_get_content(bs, &data, &len);
		gf_bs_del(bs);
		gf_isom_add_user_data(movie, i+1, GF_4CC('G','P','A','C'), binID, data, len);
		gf_free(data);
	}
	return GF_OK;
}


GF_Err gf_isom_load_movie_config(GF_ISOFile *movie)
{
	u32 i, count, len;
	char *data;
	GF_BitStream *bs;
	Bool found_cfg;
	bin128 binID;
	if (movie == NULL) return GF_BAD_PARAM;

	found_cfg = 0;
	/*restore movie*/
	count = gf_isom_get_user_data_count(movie, 0, GF_4CC('G','P','A','C'), binID);
	for (i=0; i<count; i++) {
		data = NULL;
		gf_isom_get_user_data(movie, 0, GF_4CC('G','P','A','C'), binID, i+1, &data, &len);
		if (!data) continue;
		/*check marker*/
		if ((unsigned char) data[0] != 0xFE) {
			gf_free(data);
			continue;
		}
		bs = gf_bs_new(data, len, GF_BITSTREAM_READ);
		gf_bs_read_u8(bs);	/*marker*/
		movie->storageMode = gf_bs_read_u8(bs);
		movie->interleavingTime = gf_bs_read_u32(bs);
		gf_bs_del(bs);
		gf_free(data);
		found_cfg = 1;
		break;
	}

	for (i=0; i<gf_isom_get_track_count(movie); i++) {
		u32 j;
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, i+1);
		count = gf_isom_get_user_data_count(movie, i+1, GF_4CC('G','P','A','C'), binID);
		for (j=0; j<count; j++) {
			data = NULL;
			gf_isom_get_user_data(movie, i+1, GF_4CC('G','P','A','C'), binID, j+1, &data, &len);
			if (!data) continue;
			/*check marker*/
			if ((unsigned char) data[0] != 0xFE) {
				gf_free(data);
				continue;
			}
			bs = gf_bs_new(data, len, GF_BITSTREAM_READ);
			gf_bs_read_u8(bs);	/*marker*/
			trak->Media->information->sampleTable->groupID = gf_bs_read_u32(bs);
			trak->Media->information->sampleTable->trackPriority = gf_bs_read_u32(bs);
			len = gf_bs_read_u32(bs);
			if (len) {
				u32 k;
				trak->name = (char*)gf_malloc(sizeof(char)*(len+1));
				for (k=0;k<len;k++) trak->name[k] = gf_bs_read_u8(bs);
				trak->name[k] = 0;
			}
			gf_bs_del(bs);
			gf_free(data);
			found_cfg = 1;
			break;
		}
	}
	return found_cfg ? GF_OK : GF_NOT_SUPPORTED;
}

GF_EXPORT
GF_Err gf_isom_set_media_timescale(GF_ISOFile *the_file, u32 trackNumber, u32 newTS, Bool force_rescale)
{
	Double scale;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media | !trak->Media->mediaHeader) return GF_BAD_PARAM;
	if (trak->Media->mediaHeader->timeScale==newTS) return GF_OK;

	scale = newTS;
	scale /= trak->Media->mediaHeader->timeScale;
	trak->Media->mediaHeader->timeScale = newTS;
	if (!force_rescale) {
		if (trak->editBox) {
			GF_EdtsEntry *ent;
			u32 i=0;
			while ((ent = (GF_EdtsEntry*)gf_list_enum(trak->editBox->editList->entryList, &i))) {
				ent->mediaTime = (u32) (scale*ent->mediaTime);
			}
		}
		if (trak->Media->information->sampleTable) {
			u32 i;
			GF_SampleTableBox *stbl = trak->Media->information->sampleTable;
			if (stbl->TimeToSample) {
				for (i=0; i<stbl->TimeToSample->nb_entries; i++) {
					stbl->TimeToSample->entries[i].sampleDelta = (u32) (scale * stbl->TimeToSample->entries[i].sampleDelta);
				}
			}
			if (stbl->CompositionOffset) {
				for (i=0; i<stbl->CompositionOffset->nb_entries; i++) {
					stbl->CompositionOffset->entries[i].decodingOffset = (u32) (scale * stbl->CompositionOffset->entries[i].decodingOffset);
				}
			}
		}
	}
	return SetTrackDuration(trak);
}


GF_EXPORT
Bool gf_isom_is_same_sample_description(GF_ISOFile *f1, u32 tk1, u32 sdesc_index1, GF_ISOFile *f2, u32 tk2, u32 sdesc_index2)
{
	u32 i, count;
	GF_TrackBox *trak1, *trak2;
	GF_BitStream *bs;
	char *data1, *data2;
	u32 data1_size, data2_size;
	GF_ESD *esd1, *esd2;
	GF_Box *a;
	Bool ret, need_memcmp;
	
	/*get orig sample desc and clone it*/
	trak1 = gf_isom_get_track_from_file(f1, tk1);
	if (!trak1 || !trak1->Media) return 0;
	trak2 = gf_isom_get_track_from_file(f2, tk2);
	if (!trak2 || !trak2->Media) return 0;

	if (trak1->Media->handler->handlerType != trak2->Media->handler->handlerType) return 0;
	count = gf_list_count(trak1->Media->information->sampleTable->SampleDescription->other_boxes);
	if (count != gf_list_count(trak2->Media->information->sampleTable->SampleDescription->other_boxes)) {
		if (!sdesc_index1 && !sdesc_index2) return 0;
	}

	need_memcmp = 1;
	for (i=0; i<count; i++) {
		GF_Box *ent1 = (GF_Box *)gf_list_get(trak1->Media->information->sampleTable->SampleDescription->other_boxes, i);
		GF_Box *ent2 = (GF_Box *)gf_list_get(trak2->Media->information->sampleTable->SampleDescription->other_boxes, i);

		if (sdesc_index1) ent1 = (GF_Box *)gf_list_get(trak1->Media->information->sampleTable->SampleDescription->other_boxes, sdesc_index1 - 1);
		if (sdesc_index2) ent2 = (GF_Box *)gf_list_get(trak2->Media->information->sampleTable->SampleDescription->other_boxes, sdesc_index2 - 1);

		if (!ent1 || !ent2) return 0;
		if (ent1->type != ent2->type) return 0;

		switch (ent1->type) {
		/*for MPEG-4 streams, only compare decSpecInfo (bitrate may not be the same but that's not an issue)*/
		case GF_ISOM_BOX_TYPE_MP4S:
		case GF_ISOM_BOX_TYPE_MP4A:
		case GF_ISOM_BOX_TYPE_MP4V:
		case GF_ISOM_BOX_TYPE_ENCA:
		case GF_ISOM_BOX_TYPE_ENCV:
		case GF_ISOM_BOX_TYPE_ENCS:
			Media_GetESD(trak1->Media, sdesc_index1 ? sdesc_index1 : i+1, &esd1, 1);
			Media_GetESD(trak2->Media, sdesc_index2 ? sdesc_index2 : i+1, &esd2, 1);
			if (!esd1 || !esd2) continue;
			need_memcmp = 0;
			if (esd1->decoderConfig->streamType != esd2->decoderConfig->streamType) return 0;
			if (esd1->decoderConfig->objectTypeIndication != esd2->decoderConfig->objectTypeIndication) return 0;
			if (!esd1->decoderConfig->decoderSpecificInfo && esd2->decoderConfig->decoderSpecificInfo) return 0;
			if (esd1->decoderConfig->decoderSpecificInfo && !esd2->decoderConfig->decoderSpecificInfo) return 0;
			if (!esd1->decoderConfig->decoderSpecificInfo || !esd2->decoderConfig->decoderSpecificInfo) continue;
			if (memcmp(esd1->decoderConfig->decoderSpecificInfo->data, esd2->decoderConfig->decoderSpecificInfo->data, sizeof(char)*esd1->decoderConfig->decoderSpecificInfo->dataLength)!=0) return 0;
			break;
		case GF_ISOM_BOX_TYPE_AVC1:
		case GF_ISOM_BOX_TYPE_AVC2:
		case GF_ISOM_BOX_TYPE_AVC3:
		case GF_ISOM_BOX_TYPE_AVC4:
		case GF_ISOM_BOX_TYPE_SVC1:
		case GF_ISOM_BOX_TYPE_HVC1:
		case GF_ISOM_BOX_TYPE_HEV1:
		case GF_ISOM_BOX_TYPE_HVC2:
		case GF_ISOM_BOX_TYPE_HEV2:
		case GF_ISOM_BOX_TYPE_SHC1:
		case GF_ISOM_BOX_TYPE_SHV1:
		{
			GF_MPEGVisualSampleEntryBox *avc1 = (GF_MPEGVisualSampleEntryBox *)ent1;
			GF_MPEGVisualSampleEntryBox *avc2 = (GF_MPEGVisualSampleEntryBox *)ent2;
			data1 = data2 = NULL;

			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			if (avc1->hevc_config)
				a = (GF_Box *) avc1->hevc_config;
			else if (avc1->svc_config)
				a = (GF_Box *) avc1->svc_config;
			else
				a = (GF_Box *) avc1->avc_config;
			gf_isom_box_size(a);
			gf_isom_box_write(a, bs);
			gf_bs_get_content(bs, &data1, &data1_size);
			gf_bs_del(bs);

			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			if (avc2->hevc_config)
				a = (GF_Box *) avc2->hevc_config;
			else if (avc2->svc_config)
				a = (GF_Box *) avc2->svc_config;
			else
				a = (GF_Box *) avc2->avc_config;
			gf_isom_box_size(a);
			gf_isom_box_write(a, bs);
			gf_bs_get_content(bs, &data2, &data2_size);
			gf_bs_del(bs);
			
			ret = 0;
			if (data1_size == data2_size) {
				ret = (memcmp(data1, data2, sizeof(char)*data1_size)==0) ? 1 : 0;
			}
			gf_free(data1);
			gf_free(data2);
			return ret;
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
				return 1;
			}
			return 0;
		}
			break;
		case GF_ISOM_BOX_TYPE_WVTT:
			{
				GF_WebVTTSampleEntryBox *wvtt1 = (GF_WebVTTSampleEntryBox *)ent1;
				GF_WebVTTSampleEntryBox *wvtt2 = (GF_WebVTTSampleEntryBox *)ent2;
				if (wvtt1->config && wvtt2->config && 
					(wvtt1->config->string && wvtt1->config->string && !strcmp(wvtt1->config->string, wvtt2->config->string))) {
						return 1;
				}
				return 0;
			}
            break;
		case GF_ISOM_BOX_TYPE_STSE:
			{
				GF_SimpleTextSampleEntryBox *stse1 = (GF_SimpleTextSampleEntryBox *)ent1;
				GF_SimpleTextSampleEntryBox *stse2 = (GF_SimpleTextSampleEntryBox *)ent2;
				if (stse1->mime_type && stse2->mime_type && 
					( (!stse1->config && !stse2->config) ||
					  (stse1->config && stse2->config && stse1->config->string && stse2->config->string && 
					    !strcmp(stse1->config->string, stse2->config->string)))) {
						return 1;
				}
				return 0;
			}
            break;
		}

		if (sdesc_index1 && sdesc_index2) break;
	}
	if (!need_memcmp) return 1;
	ret = 0;

	data1 = data2 = NULL;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	a = (GF_Box *)trak1->Media->information->sampleTable->SampleDescription;
	gf_isom_box_size(a);
	gf_isom_box_write(a, bs);
	gf_bs_get_content(bs, &data1, &data1_size);
	gf_bs_del(bs);

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	a = (GF_Box *)trak2->Media->information->sampleTable->SampleDescription;
	gf_isom_box_size(a);
	gf_isom_box_write(a, bs);
	gf_bs_get_content(bs, &data2, &data2_size);
	gf_bs_del(bs);
	
	if (data1_size == data2_size) {
		ret = (memcmp(data1, data2, sizeof(char)*data1_size)==0) ? 1 : 0;
	}
	gf_free(data1);
	gf_free(data2);
	return ret;
}

GF_EXPORT
u64 gf_isom_estimate_size(GF_ISOFile *movie)
{
	GF_Err e;
	GF_Box *a;
	u32 i, count;
	u64 mdat_size;
	if (!movie) return 0;

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
		assert (e == GF_OK);
		mdat_size += a->size;
	}
	return mdat_size;
}


//set shadowing on/off
GF_Err gf_isom_remove_sync_shadows(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	GF_SampleTableBox *stbl;

	if (movie->openMode == GF_ISOM_OPEN_READ) return GF_ISOM_INVALID_MODE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (stbl->ShadowSync) {
		gf_isom_box_del((GF_Box *) stbl->ShadowSync);
		stbl->ShadowSync = NULL;
	}
	return GF_OK;
}

//fill the sync shadow table
GF_Err gf_isom_set_sync_shadow(GF_ISOFile *movie, u32 trackNumber, u32 sampleNumber, u32 syncSample)
{
	GF_TrackBox *trak;
	GF_SampleTableBox *stbl;
	u8 isRAP;
	GF_Err e;

	if (movie->openMode == GF_ISOM_OPEN_READ) return GF_ISOM_INVALID_MODE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !sampleNumber || !syncSample) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (!stbl->ShadowSync) stbl->ShadowSync = (GF_ShadowSyncBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSH);

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

//set the GroupID of a track (only used for interleaving)
GF_Err gf_isom_set_track_group(GF_ISOFile *movie, u32 trackNumber, u32 GroupID)
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
GF_Err gf_isom_set_max_samples_per_chunk(GF_ISOFile *movie, u32 trackNumber, u32 maxSamplesPerChunk)
{
	GF_TrackBox *trak;

	if (movie->openMode == GF_ISOM_OPEN_READ) return GF_ISOM_INVALID_MODE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !maxSamplesPerChunk) return GF_BAD_PARAM;

	trak->Media->information->sampleTable->MaxSamplePerChunk = maxSamplesPerChunk;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_set_extraction_slc(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, GF_SLConfig *slConfig)
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


GF_EXPORT
GF_Err gf_isom_make_interleave(GF_ISOFile *file, Double TimeInSec)
{
	GF_Err e;
	if (gf_isom_get_mode(file) < GF_ISOM_OPEN_EDIT) return GF_BAD_PARAM;
	e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_INTERLEAVED);
	if (e) return e;
	return gf_isom_set_interleave_time(file, (u32) (TimeInSec * gf_isom_get_timescale(file)));
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
		FILE *f = gf_f64_open(nameUTF8+7, "rb");
		u64 size;
		if (!f) return GF_URL_ERROR;
		gf_f64_seek(f, 0, SEEK_END);
		size = gf_f64_tell(f);
		gf_f64_seek(f, 0, SEEK_SET);
		if (3!=fread(BOM, sizeof(char), 3, f)){
			fclose(f);
			return GF_CORRUPTED_DATA;
		}
		/*skip BOM if any*/
		if ((BOM[0]==0xEF) && (BOM[1]==0xBB) && (BOM[2]==0xBF)) size -= 3;
		else if ((BOM[0]==0xEF) || (BOM[0]==0xFF)) {
			fclose(f);
			return GF_BAD_PARAM;
		}
		else gf_f64_seek(f, 0, SEEK_SET);
		trak->Media->handler->nameUTF8 = (char*)gf_malloc(sizeof(char)*(size_t)(size+1));
		size = fread(trak->Media->handler->nameUTF8, sizeof(char), (size_t)size, f);
		trak->Media->handler->nameUTF8[size] = 0;
		fclose(f);
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
					szLine[j] = szOrig[i]; i++; j++;
				}
				/*UTF8 3 bytes char */
				else if ( (szOrig[i] & 0xf0) == 0xe0) {
					szLine[j] = szOrig[i]; i++; j++;
					szLine[j] = szOrig[i]; i++; j++;
				}
				/*UTF8 4 bytes char */
				else if ( (szOrig[i] & 0xf8) == 0xf0) {
					szLine[j] = szOrig[i]; i++; j++;
					szLine[j] = szOrig[i]; i++; j++;
					szLine[j] = szOrig[i]; i++; j++;
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

GF_Err gf_isom_clone_root_od(GF_ISOFile *input, GF_ISOFile *output)
{
	GF_List *esds;
	GF_Err e;
	u32 i;
	GF_Descriptor *desc;

	e = gf_isom_remove_root_od(output);
	if (e) return e;
	if (!input->moov || !input->moov->iods || !input->moov->iods->descriptor) return GF_OK;
	gf_isom_insert_moov(output);
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

	entry = (GF_SampleEntryBox*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, sampleDescriptionIndex - 1);
	if (!entry) return GF_BAD_PARAM;
	entry->type = new_type;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_set_JPEG2000(GF_ISOFile *mov, Bool set_on)
{
	if (!mov) return GF_BAD_PARAM;
	mov->is_jp2 = set_on;
	return GF_OK;
}

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
		list = trak->other_boxes;
	} else {
		if (!movie) return GF_BAD_PARAM;
		list = movie->moov->other_boxes;
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


GF_Err gf_isom_add_uuid(GF_ISOFile *movie, u32 trackNumber, bin128 UUID, char *data, u32 data_size)
{
	GF_List *list;
	GF_UnknownUUIDBox *uuid;

	if (!data_size || !data) return GF_OK;

	if (trackNumber==(u32) -1) {
		if (!movie) return GF_BAD_PARAM;
		list = movie->TopBoxes;
	} else if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->other_boxes) trak->other_boxes = gf_list_new();
		list = trak->other_boxes;
	} else {
		if (!movie) return GF_BAD_PARAM;
		if (!movie->moov->other_boxes) movie->moov->other_boxes = gf_list_new();
		list = movie->moov->other_boxes;
	}
	
	GF_SAFEALLOC(uuid, GF_UnknownUUIDBox);
	uuid->type = GF_ISOM_BOX_TYPE_UUID;
	memcpy(uuid->uuid, UUID, sizeof(bin128));
	uuid->dataSize = data_size;
	uuid->data = (char*)gf_malloc(sizeof(char)*data_size);
	memcpy(uuid->data, data, sizeof(char)*data_size);
	gf_list_add(list, uuid);
	return GF_OK;
}

/*Apple extensions*/

GF_EXPORT
GF_Err gf_isom_apple_set_tag(GF_ISOFile *mov, u32 tag, const char *data, u32 data_len)
{
	GF_BitStream *bs;
	GF_Err e;
	GF_ItemListBox *ilst;
	GF_MetaBox *meta;
	GF_ListItemBox *info;
	u32 btype, i;


	e = CanAccessMovie(mov, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	meta = gf_isom_apple_create_meta_extensions(mov);
	if (!meta) return GF_BAD_PARAM;

	ilst = gf_ismo_locate_box(meta->other_boxes, GF_ISOM_BOX_TYPE_ILST, NULL);
	if (!ilst) return GF_NOT_SUPPORTED;

	if (tag==GF_ISOM_ITUNE_GENRE) {
		btype = data ? GF_ISOM_BOX_TYPE_0xA9GEN : GF_ISOM_BOX_TYPE_GNRE;
	} else {
		btype = tag;
	}
	/*remove tag*/
	i = 0;
	while ((info = gf_list_enum(ilst->other_boxes, &i))) {
		if (info->type==btype) {
			gf_list_rem(ilst->other_boxes, i-1);
			gf_isom_box_del((GF_Box *) info);
			break;
		}
	}

	if (data != NULL) {
		info = (GF_ListItemBox *)gf_isom_box_new(btype);
		if (info == NULL) return GF_OUT_OF_MEM;
		switch (btype) {
		case GF_ISOM_BOX_TYPE_TRKN:
		case GF_ISOM_BOX_TYPE_DISK:
		case GF_ISOM_BOX_TYPE_GNRE:
			info->data->flags = 0x0;
			break;
		case GF_ISOM_BOX_TYPE_PGAP:
		case GF_ISOM_BOX_TYPE_CPIL:
			info->data->flags = 0x15;
			break;
		default:
			info->data->flags = 0x1;
			break;
		}
		if (tag==GF_ISOM_ITUNE_COVER_ART) {
			if (data_len & 0x80000000) {
				data_len = (data_len & 0x7FFFFFFF);
				info->data->flags = 14;
			} else {
				info->data->flags = 13;
			}
		}
		info->data->dataSize = data_len;
		info->data->data = (char*)gf_malloc(sizeof(char)*data_len);
		memcpy(info->data->data , data, sizeof(char)*data_len);
	} 
	else if (data_len && (tag==GF_ISOM_ITUNE_GENRE)) {
		info = (GF_ListItemBox *)gf_isom_box_new(btype);
		if (info == NULL) return GF_OUT_OF_MEM;
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u16(bs, data_len);
		gf_bs_get_content(bs, & info->data->data, &info->data->dataSize);
		info->data->flags = 0x0;
		gf_bs_del(bs);
	} else if (data_len && (tag==GF_ISOM_ITUNE_COMPILATION)) {
		info = (GF_ListItemBox *)gf_isom_box_new(btype);
		if (info == NULL) return GF_OUT_OF_MEM;
		info->data->data = (char*)gf_malloc(sizeof(char));
		info->data->data[0] = 1;
		info->data->dataSize = 1;
		info->data->flags = 21;
	} else if (data_len && (tag==GF_ISOM_ITUNE_TEMPO)) {
		info = (GF_ListItemBox *)gf_isom_box_new(btype);
		if (info == NULL) return GF_OUT_OF_MEM;
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u16(bs, data_len);
		gf_bs_get_content(bs, &info->data->data, &info->data->dataSize);
		info->data->flags = 0x15;
		gf_bs_del(bs);
	}
	return gf_list_add(ilst->other_boxes, info);
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
			if (!trak_ref || !trak_ref->Header->alternate_group) return GF_BAD_PARAM;
			alternateGroupID = trak_ref->Header->alternate_group;
		} else {
			alternateGroupID = trak->Header->alternate_group;
		}
	} 
	if (!alternateGroupID) {
		/*there is a function for this ....*/
		if (trak->Header->alternate_group) return GF_BAD_PARAM;
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
					count = gf_list_count(map->other_boxes);
					for (j=0; j<count; j++) {
						tsel = gf_list_get(map->other_boxes, j);

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
			e = trak_AddBox((GF_Box*)trak, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
			if (e) return e;
		}

		map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_TSEL, NULL);

		/*locate tsel box with no switch group*/
		if (map)  {
			u32 j, count = gf_list_count(map->other_boxes);
			for (j=0; j<count; j++) {
				tsel = gf_list_get(map->other_boxes, j);
				if (tsel->switchGroup == *switchGroupID) break;
				tsel = NULL;
			}
		}
		if (!tsel) {
			tsel = (GF_TrackSelectionBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_TSEL);
			e = udta_AddBox(trak->udta, (GF_Box *) tsel);
			if (e) return e;
		}

		tsel->switchGroup = *switchGroupID;
		tsel->attributeListCount = criteriaListCount;
		if (tsel->attributeList) gf_free(tsel->attributeList);
		tsel->attributeList = gf_malloc(sizeof(u32)*criteriaListCount);
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
		gf_isom_box_array_del(map->other_boxes);
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

GF_Err gf_isom_timed_meta_data_config_new(GF_ISOFile *movie, u32 trackNumber, Bool is_xml, char *mime_or_namespace, char *content_encoding, char *schema_loc, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_MetaDataSampleEntryBox *metad;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !mime_or_namespace)
		return GF_BAD_PARAM;

	if (trak->Media->handler->handlerType==GF_ISOM_MEDIA_META)
		return GF_BAD_PARAM;
	
	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	if (!movie->keep_utc)
		trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	metad = (GF_MetaDataSampleEntryBox*) gf_isom_box_new(is_xml ? GF_ISOM_BOX_TYPE_METX : GF_ISOM_BOX_TYPE_METT);
	if (!metad) return GF_OUT_OF_MEM;

	metad->dataReferenceIndex = dataRefIndex;
	metad->mime_type_or_namespace = gf_strdup(mime_or_namespace);
	if (content_encoding) metad->content_encoding = gf_strdup(content_encoding);
	if (schema_loc) metad->xml_schema_loc = gf_strdup(schema_loc);

	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->other_boxes, metad);
	if (outDescriptionIndex) *outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);
	return e;
}


GF_Err gf_isom_add_subsample(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable)
{
	GF_SubSampleInformationBox *sub_samples;
	GF_TrackBox *trak;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(movie, track);
	if (!trak || !trak->Media || !trak->Media->information->sampleTable)
		return GF_BAD_PARAM;
	
	if (!trak->Media->information->sampleTable->SubSamples) {
		trak->Media->information->sampleTable->SubSamples = (GF_SubSampleInformationBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SUBS);
		sub_samples = trak->Media->information->sampleTable->SubSamples;
		sub_samples->version = (subSampleSize>0xFFFF) ? 1 : 0;
	} else {
		sub_samples = trak->Media->information->sampleTable->SubSamples;
	}
	return gf_isom_add_subsample_info(sub_samples, sampleNumber, subSampleSize, priority, reserved, discardable);
}


GF_EXPORT
GF_Err gf_isom_set_rvc_config(GF_ISOFile *movie, u32 track, u32 sampleDescriptionIndex, u16 rvc_predefined, char *mime, char *data, u32 size)
{
	GF_MPEGVisualSampleEntryBox *entry;
	GF_Err e;
	GF_TrackBox *trak;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, track);
	if (!trak) return GF_BAD_PARAM;


	entry = (GF_MPEGVisualSampleEntryBox *) gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, sampleDescriptionIndex-1);
	if (!entry ) return GF_BAD_PARAM;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_MP4V:
	case GF_ISOM_BOX_TYPE_AVC1: 
	case GF_ISOM_BOX_TYPE_AVC2:
	case GF_ISOM_BOX_TYPE_AVC3:
	case GF_ISOM_BOX_TYPE_AVC4:
	case GF_ISOM_BOX_TYPE_SVC1:
	case GF_ISOM_BOX_TYPE_ENCV:
	case GF_ISOM_BOX_TYPE_HVC1: 
	case GF_ISOM_BOX_TYPE_HEV1: 
	case GF_ISOM_BOX_TYPE_HVC2:
	case GF_ISOM_BOX_TYPE_HEV2:
	case GF_ISOM_BOX_TYPE_SHC1:
	case GF_ISOM_BOX_TYPE_SHV1:
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (entry->rvcc && entry->rvcc->rvc_meta_idx) {
		gf_isom_remove_meta_item(movie, 0, track, entry->rvcc->rvc_meta_idx);
		entry->rvcc->rvc_meta_idx = 0;
	}

	if (!entry->rvcc) {
		entry->rvcc = (GF_RVCConfigurationBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_RVCC);
	}
	entry->rvcc->predefined_rvc_config = rvc_predefined;
	if (!rvc_predefined) {
		e = gf_isom_set_meta_type(movie, 0, track, GF_4CC('r','v','c','i'));
		if (e) return e;
		gf_isom_modify_alternate_brand(movie, GF_ISOM_BRAND_ISO2, 1);		
		e = gf_isom_add_meta_item_memory(movie, 0, track, "rvcconfig.xml", mime, NULL, data, size);
		if (e) return e;
		entry->rvcc->rvc_meta_idx = gf_isom_get_meta_item_count(movie, 0, track);
	}
	return GF_OK;
}


GF_Err gf_isom_add_sample_group_entry(GF_List *sampleGroups, u32 sample_number, u32 grouping_type, u32 sampleGroupDescriptionIndex)
{
	GF_SampleGroupBox *sgroup = NULL;
	u32 i, count, last_sample_in_entry;

	count = gf_list_count(sampleGroups);
	for (i=0; i<count; i++) {
		sgroup = gf_list_get(sampleGroups, i);
		if (sgroup->grouping_type==grouping_type) break;
		sgroup = NULL;
	}
	if (!sgroup) {
		sgroup = (GF_SampleGroupBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SBGP);
		sgroup->grouping_type = grouping_type;
		gf_list_add(sampleGroups, sgroup);
	}
	/*used in fragments, means we are adding the last sample*/
	if (!sample_number) {
		sample_number = 1;
		if (sgroup->entry_count) {
			for (i=0; i<sgroup->entry_count; i++) {
				sample_number += sgroup->sample_entries[i].sample_count;
			}
		}
	}

	if (!sgroup->entry_count) {
		u32 idx = 0;
		sgroup->entry_count = (sample_number>1) ? 2 : 1;
		sgroup->sample_entries = gf_malloc(sizeof(GF_SampleGroupEntry) * sgroup->entry_count );
		if (sample_number>1) {
			sgroup->sample_entries[0].sample_count = sample_number-1;
			sgroup->sample_entries[0].group_description_index = 0;
			idx = 1;
		}
		sgroup->sample_entries[idx].sample_count = 1;
		sgroup->sample_entries[idx].group_description_index = sampleGroupDescriptionIndex;
		return GF_OK;
	}
	last_sample_in_entry = 0;
	for (i=0; i<sgroup->entry_count; i++) {
		/*TODO*/
		if (last_sample_in_entry + sgroup->sample_entries[i].sample_count > sample_number) return GF_NOT_SUPPORTED;
		last_sample_in_entry += sgroup->sample_entries[i].sample_count;
	}

	if ((sgroup->sample_entries[sgroup->entry_count-1].group_description_index==sampleGroupDescriptionIndex) && (last_sample_in_entry+1==sample_number)) {
		sgroup->sample_entries[sgroup->entry_count-1].sample_count++;
		return GF_OK;
	}
	/*last entry was an empty desc (no group associated), just add the number of samples missing until new one, then add new one*/
	if (! sgroup->sample_entries[sgroup->entry_count-1].group_description_index) {
		sgroup->sample_entries[sgroup->entry_count-1].sample_count += sample_number - 1 - last_sample_in_entry;
		sgroup->sample_entries = gf_realloc(sgroup->sample_entries, sizeof(GF_SampleGroupEntry) * (sgroup->entry_count + 1) );
		sgroup->sample_entries[sgroup->entry_count].sample_count = 1;
		sgroup->sample_entries[sgroup->entry_count].group_description_index = sampleGroupDescriptionIndex;
		sgroup->entry_count++;
		return GF_OK;
	}
	/*we are adding a sample with no desc, add entry at the end*/
	if (!sampleGroupDescriptionIndex) {
		sgroup->sample_entries = gf_realloc(sgroup->sample_entries, sizeof(GF_SampleGroupEntry) * (sgroup->entry_count + 1) );
		sgroup->sample_entries[sgroup->entry_count].sample_count = 1;
		sgroup->sample_entries[sgroup->entry_count].group_description_index = 0;
		sgroup->entry_count++;
		return GF_OK;
	}
	/*need to insert two entries ...*/
	sgroup->sample_entries = gf_realloc(sgroup->sample_entries, sizeof(GF_SampleGroupEntry) * (sgroup->entry_count + 2) );

	sgroup->sample_entries[sgroup->entry_count].sample_count = sample_number - 1 - last_sample_in_entry;
	sgroup->sample_entries[sgroup->entry_count].group_description_index = 0;

	sgroup->sample_entries[sgroup->entry_count+1].sample_count = 1;
	sgroup->sample_entries[sgroup->entry_count+1].group_description_index = sampleGroupDescriptionIndex;
	sgroup->entry_count+=2;
	return GF_OK;
}

/*for now not exported*/
static GF_Err gf_isom_set_sample_group_info(GF_ISOFile *movie, u32 track, u32 sample_number, u32 grouping_type, void *udta, void *(*sg_create_entry)(void *udta), Bool (*sg_compare_entry)(void *udta, void *entry))
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_List *groupList;
	void *entry;
	GF_SampleGroupDescriptionBox *sgdesc = NULL;
	u32 i, count, entry_idx;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, track);
	if (!trak) return GF_BAD_PARAM;

	/*look in stbl for sample sampleGroupsDescription*/
	if (!trak->Media->information->sampleTable->sampleGroupsDescription)
		trak->Media->information->sampleTable->sampleGroupsDescription = gf_list_new();

	groupList = trak->Media->information->sampleTable->sampleGroupsDescription;
	count = gf_list_count(groupList);
	for (i=0; i<count; i++) {
		sgdesc = gf_list_get(groupList, i);
		if (sgdesc->grouping_type==grouping_type) break;
		sgdesc = NULL;
	}
	if (!sgdesc) {
		sgdesc = (GF_SampleGroupDescriptionBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SGPD);
		sgdesc->grouping_type = grouping_type;
		gf_list_add(groupList, sgdesc);
	}
	entry = NULL;
	for (i=0; i<gf_list_count(sgdesc->group_descriptions); i++) {
		entry = gf_list_get(sgdesc->group_descriptions, i);
		if (sg_compare_entry(udta, entry)) break;
		entry = NULL;
	}
	if (!entry) {
		entry = sg_create_entry(udta);
		if (!entry) return GF_IO_ERR;
		gf_list_add(sgdesc->group_descriptions, entry);
	}

	entry_idx = 1 + gf_list_find(sgdesc->group_descriptions, entry);

	/*look in stbl for sample sampleGroups*/
	if (!trak->Media->information->sampleTable->sampleGroups)
		trak->Media->information->sampleTable->sampleGroups = gf_list_new();

	groupList = trak->Media->information->sampleTable->sampleGroups;

	return gf_isom_add_sample_group_entry(trak->Media->information->sampleTable->sampleGroups, sample_number, grouping_type, entry_idx);
}

void *sg_rap_create_entry(void *udta)
{
	GF_VisualRandomAccessEntry *entry;
	u32 *num_leading_samples = (u32 *) udta;
	assert(udta);
	GF_SAFEALLOC(entry, GF_VisualRandomAccessEntry);
	entry->num_leading_samples = *num_leading_samples;
	entry->num_leading_samples_known = entry->num_leading_samples ? 1 : 0;
	return entry;
}

Bool sg_rap_compare_entry(void *udta, void *entry)
{
	u32 *num_leading_samples = (u32 *) udta;
	if (*num_leading_samples == ((GF_VisualRandomAccessEntry *)entry)->num_leading_samples) return 1;
	return 0;
}

GF_Err gf_isom_set_sample_rap_group(GF_ISOFile *movie, u32 track, u32 sample_number, u32 num_leading_samples)
{
	return gf_isom_set_sample_group_info(movie, track, sample_number, GF_4CC( 'r', 'a', 'p', ' ' ), &num_leading_samples, sg_rap_create_entry, sg_rap_compare_entry);
}



void *sg_roll_create_entry(void *udta)
{
	GF_RollRecoveryEntry *entry;
	s16 *roll_distance = (s16 *) udta;
	GF_SAFEALLOC(entry, GF_RollRecoveryEntry);
	entry->roll_distance = *roll_distance;
	return entry;
}

Bool sg_roll_compare_entry(void *udta, void *entry)
{
	s16 *roll_distance = (s16 *) udta;
	if (*roll_distance == ((GF_RollRecoveryEntry *)entry)->roll_distance) return 1;
	return 0;
}

GF_Err gf_isom_set_sample_roll_group(GF_ISOFile *movie, u32 track, u32 sample_number, s16 roll_distance)
{
	return gf_isom_set_sample_group_info(movie, track, sample_number, GF_4CC( 'r', 'o', 'l', 'l' ), &roll_distance, sg_roll_create_entry, sg_roll_compare_entry);
}

void *sg_encryption_create_entry(void *udta)
{
	GF_CENCSampleEncryptionGroupEntry *entry;
	GF_BitStream *bs;
	GF_SAFEALLOC(entry, GF_CENCSampleEncryptionGroupEntry);
	bs = gf_bs_new((char *) udta, sizeof(GF_CENCSampleEncryptionGroupEntry), GF_BITSTREAM_READ);
	entry->IsEncrypted = gf_bs_read_u32(bs);
	entry->IV_size = gf_bs_read_u8(bs);
	gf_bs_read_data(bs, (char *)entry->KID, 16);
	gf_bs_del(bs);
	return entry;
}

Bool sg_encryption_compare_entry(void *udta, void *entry)
{
	u32 isEncrypted;
	u8 IV_size;
	bin128 KID;
	GF_BitStream *bs;
	GF_CENCSampleEncryptionGroupEntry *seig = (GF_CENCSampleEncryptionGroupEntry *)entry;
	bs = gf_bs_new((char *) udta, sizeof(GF_CENCSampleEncryptionGroupEntry), GF_BITSTREAM_READ);
	isEncrypted = gf_bs_read_u32(bs);
	IV_size = gf_bs_read_u8(bs);
	gf_bs_read_data(bs, (char *)KID, 16);
	gf_bs_del(bs);
	if ((isEncrypted == seig->IsEncrypted) && (IV_size == seig->IV_size) && (!strncmp((const char *)KID, (const char *)seig->KID, 16))) return 1;
	return 0;
}

/*sample encryption information group can be in stbl or traf*/
GF_EXPORT
GF_Err gf_isom_set_sample_cenc_group(GF_ISOFile *movie, u32 track, u32 sample_number, Bool isEncrypted, u8 IV_size, bin128 KeyID)
{
	char seig[21];
	if ((IV_size!=0) && (IV_size!=4) && (IV_size!=8)) return GF_BAD_PARAM;

	seig[0] = seig[1] = 0;
	seig[2] = isEncrypted ? 1 : 0;
	seig[3] = IV_size;
	memcpy(seig + 5, KeyID, sizeof(bin128));

	return gf_isom_set_sample_group_info(movie, track, sample_number, GF_4CC( 's', 'e', 'i', 'g' ), seig, sg_encryption_create_entry, sg_encryption_compare_entry);
}

static GF_Err gf_isom_set_ctts_v1(GF_ISOFile *file, u32 track, GF_TrackBox *trak)
{
	u32 i, shift;
	u64 duration;
	GF_CompositionOffsetBox *ctts;
	GF_CompositionToDecodeBox *cslg;
	s32 leastCTTS, greatestCTTS;

	ctts = trak->Media->information->sampleTable->CompositionOffset;
	shift = ctts->entries[0].decodingOffset;
	leastCTTS = 0;
	greatestCTTS = 0; 
	for (i=0; i<ctts->nb_entries; i++) {
		ctts->entries[i].decodingOffset -= shift;
		if ((s32)ctts->entries[i].decodingOffset < leastCTTS)
			leastCTTS = ctts->entries[i].decodingOffset;
		if ((s32)ctts->entries[i].decodingOffset > greatestCTTS)
			greatestCTTS = ctts->entries[i].decodingOffset;
	}
	ctts->version = 1;


	if (!trak->Media->information->sampleTable->CompositionToDecode)
		trak->Media->information->sampleTable->CompositionToDecode = (GF_CompositionToDecodeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CSLG);

	cslg = trak->Media->information->sampleTable->CompositionToDecode;

	cslg->compositionToDTSShift = -leastCTTS;
	cslg->leastDecodeToDisplayDelta = leastCTTS;
	cslg->greatestDecodeToDisplayDelta = greatestCTTS;
	cslg->compositionStartTime = 0;
	/*for our use case (first CTS set to 0), the composition end time is the media duration if it fits on 32 bits*/
	duration = gf_isom_get_media_duration(file, track);
	cslg->compositionEndTime = (duration<0x7FFFFFFF) ? (s32) duration : 0;

	gf_isom_set_brand_info(file, GF_ISOM_BRAND_ISO4, 0);
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
		gf_isom_box_del((GF_Box *)cslg);
		trak->Media->information->sampleTable->CompositionToDecode = NULL;
	}
	ctts->version = 0;
	gf_isom_set_brand_info(file, GF_ISOM_BRAND_ISOM, 1);
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
	if (!ctts) return GF_OK;

	if (use_negative_offsets) {
		if (ctts->version==1) return GF_OK;
		return gf_isom_set_ctts_v1(file, track, trak);
	} else {
		if (ctts->version==0) return GF_OK;
		return gf_isom_set_ctts_v0(file, trak);
	}
}

#endif	/*!defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)*/


