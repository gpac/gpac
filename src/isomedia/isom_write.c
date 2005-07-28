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

#ifndef GPAC_READ_ONLY

GF_Err CanAccessMovie(GF_ISOFile *movie, u32 Mode)
{
	if (!movie) return GF_BAD_PARAM;
	if (movie->openMode < Mode) return GF_ISOM_INVALID_MODE;

#ifndef	GF_ISOM_NO_FRAGMENTS
	if (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) return GF_ISOM_INVALID_MODE;
#endif
	return GF_OK;
}

GF_Err moov_AddBox(GF_MovieBox *ptr, GF_Box *a);
GF_Err trak_AddBox(GF_TrackBox *ptr, GF_Box *a);
GF_Err udta_AddBox(GF_UserDataBox *ptr, GF_Box *a);
GF_Err reftype_AddRefTrack(GF_TrackReferenceTypeBox *ref, u32 trackID, u16 *outRefIndex);
GF_Err tref_AddBox(GF_TrackReferenceBox *ptr, GF_Box *a);

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
	return moov_AddBox(moov, (GF_Box *)iods);
}

//add a track to the root OD
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
	for (i=0; i<gf_list_count(esds); i++) {
		inc = (GF_ES_ID_Inc*)gf_list_get(esds, i);
		if (inc->trackID == gf_isom_get_track_id(movie, trackNumber)) {
			gf_odf_desc_del((GF_Descriptor *)inc);
			gf_list_rem(esds, i);
			break;
		}
	}
	//we don't remove the iod for P&Ls and other potential info
	return GF_OK;
}

//sets the enable flag of a track
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

GF_Err gf_isom_set_media_language(GF_ISOFile *movie, u32 trackNumber, char *three_char_code)
{
	GF_Err e;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	memcpy(trak->Media->mediaHeader->packedLanguage, three_char_code, sizeof(char)*3);
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
	iod = (GF_IsomInitialObjectDescriptor*)malloc(sizeof(GF_IsomInitialObjectDescriptor));
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
		if (((GF_IsomObjectDescriptor *)movie->moov->iods->descriptor)->URLString) free(((GF_IsomObjectDescriptor *)movie->moov->iods->descriptor)->URLString);
		((GF_IsomObjectDescriptor *)movie->moov->iods->descriptor)->URLString = url_string ? strdup(url_string) : NULL;
		break;
	case GF_ODF_ISOM_IOD_TAG:
		if (((GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor)->URLString) free(((GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor)->URLString);
		((GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor)->URLString = url_string ? strdup(url_string) : NULL;
		break;
	default:
		return GF_ISOM_INVALID_FILE;
	}
	return GF_OK;
}



//creates a new Track. If trackID = 0, the trackID is chosen by the API
//returns the track number or 0 if error
u32 gf_isom_new_track(GF_ISOFile *movie, u32 trakID, u32 MediaType, u32 TimeScale)
{
	GF_Err e;
	u64 now;
	u8 isHint;
	GF_TrackBox *trak;
	GF_TrackHeaderBox *tkhd;
	GF_MediaBox *mdia;
	void gf_isom_box_del(GF_Box *ptr);

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) {
		gf_isom_set_last_error(movie, e);
		return 0;
	}
	gf_isom_insert_moov(movie);


	isHint = 0;
	//we're creating a hint track... it's the same, but mode HAS TO BE EDIT
	if (MediaType == GF_ISOM_MEDIA_HINT) {
		if (movie->openMode != GF_ISOM_OPEN_EDIT) return 0;
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

	e = trak_AddBox(trak, (GF_Box *) tkhd); if (e) goto err_exit;
	e = trak_AddBox(trak, (GF_Box *) mdia); if (e) goto err_exit;
	tkhd->trackID = trakID;

	
	//some default properties for Audio, Visual or private tracks
	if (MediaType == GF_ISOM_MEDIA_VISUAL) {
		/*320-240 pix in 16.16*/
		tkhd->width = 0x01400000;
		tkhd->height = 0x00F00000;
	} else if (MediaType == GF_ISOM_MEDIA_AUDIO) {
		tkhd->volume = 0x0100;
	}

	mdia->mediaHeader->creationTime = mdia->mediaHeader->modificationTime = now;
	trak->Header->creationTime = trak->Header->modificationTime = now;

	//OK, add our trak
	e = moov_AddBox(movie->moov, (GF_Box *)trak); if (e) goto err_exit;
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
	GF_TrackReferenceTypeBox *dpnd;
	GF_TrackReferenceBox *tref;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || 
		!esd || !esd->decoderConfig || 
		!esd->slConfig) return GF_BAD_PARAM;

	dpnd = NULL;
	tref = NULL;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	//duplicate our desc
	e = gf_odf_desc_copy((GF_Descriptor *)esd, (GF_Descriptor **)&new_esd);
	if (e) return e;;
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

	//REWRITE ANY OD STUFF
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		e = Media_ParseODFrame(trak->Media, sample);
		if (e) return e;
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


	//get this dataRef and return false if not self contained
	Dentry = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->boxList, dataRefIndex - 1);
	if (!Dentry || Dentry->flags != 1) return GF_BAD_PARAM;

	//Open our data map. We are adding stuff, so use EDIT
	e = gf_isom_datamap_open(trak->Media, dataRefIndex, 1);
	if (e) return e;

	//Get the offset...
	data_offset = gf_isom_datamap_get_offset(trak->Media->information->dataHandler);
	//add the meta data
	e = Media_AddSample(trak->Media, data_offset, sample, descIndex, 0);
	if (e) return e;
	//add the media data
	e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, sample->data, sample->dataLength);
	if (e) return e;
	//OK, update duration
	e = Media_SetDuration(trak);
	if (e) return e;
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

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !sample) return GF_BAD_PARAM;

	e = FlushCaptureMode(movie);
	if (e) return e;

	e = unpack_track(trak);
	if (e) return e;

	/*REWRITE ANY OD STUFF*/
	if (trak->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		e = Media_ParseODFrame(trak->Media, sample);
		if (e) return e;
	}

	e = findEntryForTime(trak->Media->information->sampleTable, sample->DTS, 0, &sampleNum, &prevSampleNum);
	if (e) return e;
	/*we need the EXACT match*/
	if (!sampleNum) return GF_BAD_PARAM;

	prev = gf_isom_get_sample_info(movie, trackNumber, sampleNum, &descIndex, NULL);
	if (!prev) return gf_isom_last_error(movie);
	gf_isom_sample_del(&prev);

	e = Media_GetSampleDesc(trak->Media, descIndex, &entry, &dataRefIndex);
	if (e) return e;
	if (!entry || !dataRefIndex) return GF_BAD_PARAM;
	trak->Media->information->sampleTable->currentEntryIndex = descIndex;

	//get this dataRef and return false if not self contained
	Dentry = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->boxList, dataRefIndex - 1);
	if (!Dentry || Dentry->flags != 1) return GF_BAD_PARAM;

	//Open our data map. We are adding stuff, so use EDIT
	e = gf_isom_datamap_open(trak->Media, dataRefIndex, 1);
	if (e) return e;

	data_offset = gf_isom_datamap_get_offset(trak->Media->information->dataHandler);
	e = Media_AddSample(trak->Media, data_offset, sample, descIndex, sampleNum);
	if (e) return e;
	//add the media data
	e = gf_isom_datamap_add_data(trak->Media->information->dataHandler, sample->data, sample->dataLength);
	if (e) return e;

	//OK, update duration
	e = Media_SetDuration(trak);
	if (e) return e;
	trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	return SetTrackDuration(trak);
}

GF_Err gf_isom_append_sample_data(GF_ISOFile *movie, u32 trackNumber, unsigned char *data, u32 data_size)
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
	Dentry = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->boxList, dataRefIndex - 1);
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
	Dentry =(GF_DataEntryURLBox*) gf_list_get(trak->Media->information->dataInformation->dref->boxList, dataRefIndex - 1);
	if (Dentry->flags == 1) return GF_BAD_PARAM;

	//add the meta data
	e = Media_AddSample(trak->Media, dataOffset, sample, descIndex, 0);
	if (e) return e;

	trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	//OK, update duration
	e = Media_SetDuration(trak);
	if (e) return e;
	return SetTrackDuration(trak);

}

//set the duration of the last media sample. If not set, the duration of the last sample is the
//duration of the previous one if any, or 1000 (default value).
GF_Err gf_isom_set_last_sample_duration(GF_ISOFile *movie, u32 trackNumber, u32 duration)
{
	GF_TrackBox *trak;
	GF_SttsEntry *ent;
	u64 mdur;
	GF_Err e;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	mdur = trak->Media->mediaHeader->duration;
	//get the last entry
	ent = (GF_SttsEntry*)gf_list_get(trak->Media->information->sampleTable->TimeToSample->entryList, gf_list_count(trak->Media->information->sampleTable->TimeToSample->entryList)-1);
	if (!ent) return GF_BAD_PARAM;

	mdur -= ent->sampleDelta;
	mdur += duration;
	//we only have one sample
	if (ent->sampleCount == 1) {
		ent->sampleDelta = duration;
	} else {
		if (ent->sampleDelta == duration) return GF_OK;
		ent->sampleCount -= 1;
		ent = (GF_SttsEntry*)malloc(sizeof(GF_SttsEntry));
		ent->sampleCount = 1;
		ent->sampleDelta = duration;
		//add this entry
		gf_list_add(trak->Media->information->sampleTable->TimeToSample->entryList, ent);
		//and update the write cache
		trak->Media->information->sampleTable->TimeToSample->w_currentEntry = ent;
		trak->Media->information->sampleTable->TimeToSample->w_currentSampleNum = trak->Media->information->sampleTable->SampleSize->sampleCount;
	}
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
		e = Media_ParseODFrame(trak->Media, sample);
		if (e) return e;
	}
	//OK, update it
	e = Media_UpdateSample(trak->Media, sampleNumber, sample, data_only);
	if (e) return e;

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

	trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	return GF_OK;
}


//Remove a given sample
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
		if (movie->finalName) free(movie->finalName);
		movie->finalName = strdup(filename);
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
		e = trak_AddBox(trak, (GF_Box *)tref);
		if (e) return e;
	}
	tref = trak->References;

	e = Track_FindRef(trak, GF_ISOM_REF_IPI, &dpnd);
	if (e) return e;
	if (!dpnd) {
		tmpRef = 0;
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_IPIR);
		e = tref_AddBox(tref, (GF_Box *) dpnd);
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

	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->boxList)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->boxList, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;

	trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	//duplicate our desc
	e = gf_odf_desc_copy((GF_Descriptor *)newESD, (GF_Descriptor **)&esd);
	if (e) return e;
	return Track_SetStreamDescriptor(trak, StreamDescriptionIndex, entry->dataReferenceIndex, esd, NULL);
}

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
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->boxList)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = gf_list_get(stsd->boxList, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
	trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	//valid for MPEG visual, JPG and 3GPP H263
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_MP4V:
	case GF_ISOM_SUBTYPE_3GP_H263:
	case GF_ISOM_BOX_TYPE_AVC1:
		((GF_VisualSampleEntryBox*)entry)->Width = Width;
		((GF_VisualSampleEntryBox*)entry)->Height = Height;
		trak->Header->width = Width<<16;
		trak->Header->height = Height<<16;
		return GF_OK;
	/*check BIFS*/
	default:
		if (trak->Media->handler->handlerType==GF_ISOM_MEDIA_BIFS) {
			trak->Header->width = Width<<16;
			trak->Header->height = Height<<16;
			return GF_OK;
		}
		return GF_BAD_PARAM;
	}
}

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
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->boxList)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = gf_list_get(stsd->boxList, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;
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
GF_Err gf_isom_set_storage_mode(GF_ISOFile *movie, u8 storageMode)
{
	GF_Err e;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	switch (storageMode) {
	case GF_ISOM_STORE_FLAT:
	case GF_ISOM_STORE_STREAMABLE:
	case GF_ISOM_STORE_INTERLEAVED:
		movie->storageMode = storageMode;
		return GF_OK;
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
GF_Err gf_isom_set_edit_segment(GF_ISOFile *movie, u32 trackNumber, u32 EditTime, u32 EditDuration, u32 MediaTime, u8 EditMode)
{
	GF_TrackBox *trak;
	GF_EditBox *edts;
	GF_EditListBox *elst;
	GF_EdtsEntry *ent, *newEnt;
	u32 i;
	GF_Err e;
	u64 startTime;
	GF_Err edts_AddBox(GF_EditBox *ptr, GF_Box *a);

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	edts = trak->editBox;
	if (! edts) {
		edts = (GF_EditBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_EDTS);
		if (!edts) return GF_OUT_OF_MEM;
		trak_AddBox(trak, (GF_Box *)edts);
	}
	elst = edts->editList;
	if (!elst) {
		elst = (GF_EditListBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_ELST);
		if (!elst) return GF_OUT_OF_MEM;
		edts_AddBox(edts, (GF_Box *)elst);
	}

	startTime = 0;
	ent = NULL;
	//get the prev entry to this startTime if any
	for (i = 0; i < gf_list_count(elst->entryList); i++) {
		ent = (GF_EdtsEntry*)gf_list_get(elst->entryList, i);
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
		free(ent);
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
	next_ent = gf_list_get(trak->editBox->editList->entryList, seg_index-1);
	if (next_ent) next_ent->segmentDuration += ent->segmentDuration;
	free(ent);
	return SetTrackDuration(trak);
}


GF_Err gf_isom_append_edit_segment(GF_ISOFile *movie, u32 trackNumber, u32 EditDuration, u32 MediaTime, u8 EditMode)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_EdtsEntry *ent;
	GF_Err edts_AddBox(GF_EditBox *ptr, GF_Box *a);
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!trak->editBox) {
		GF_EditBox *edts = (GF_EditBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_EDTS);
		if (!edts) return GF_OUT_OF_MEM;
		trak_AddBox(trak, (GF_Box *)edts);
	}
	if (!trak->editBox->editList) {
		GF_EditListBox *elst = (GF_EditListBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_ELST);
		if (!elst) return GF_OUT_OF_MEM;
		edts_AddBox(trak->editBox, (GF_Box *)elst);
	}
	ent = malloc(sizeof(GF_EdtsEntry));
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

GF_Err gf_isom_modify_edit_segment(GF_ISOFile *movie, u32 trackNumber, u32 seg_index, u32 EditDuration, u32 MediaTime, u8 EditMode)
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
		for (i=0; i<gf_list_count(ESDs); i++) {
			inc = (GF_ES_ID_Inc*)gf_list_get(ESDs, i);
			if (inc->trackID == the_trak->Header->trackID) {
				gf_odf_desc_del((GF_Descriptor *)inc);
				gf_list_rem(ESDs, i);
				i--;
			}
		}
	}

	//remove the track from the movie
	gf_list_del_item(movie->moov->trackList, the_trak);

	//rewrite any OD tracks
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		trak = (GF_TrackBox*)gf_list_get(movie->moov->trackList, i);
		if (trak->Media->handler->handlerType != GF_ISOM_MEDIA_OD) continue;
		//this is an OD track...
		j = gf_isom_get_sample_count(movie, i+1);
		for (k=0; k < j; k++) {
			//getting the sample will remove the references to the deleted track in the output OD frame
			samp = gf_isom_get_sample(movie, i+1, k+1, &descIndex);
			if (!samp) break;
			//so let's update with the new OD frame ! If the sample is empty, remove it
			if (!samp->dataLength) {
				e = gf_isom_remove_sample(movie, i+1, k+1);
				if (e) return e;
			} else {
				e = gf_isom_update_sample(movie, i+1, k+1, samp, 1);
				if (e) return e;
			}
			//and don't forget to delete the sample
			gf_isom_sample_del(&samp);
		}
	}

	//remove the track ref from any "tref" box in all tracks (except the one to delete ;)
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		trak = (GF_TrackBox*)gf_list_get(movie->moov->trackList, i);
		if (trak == the_trak) continue;
		if (! trak->References || ! gf_list_count(trak->References->boxList)) continue;
		for (j=0; j<gf_list_count(trak->References->boxList); j++) {
			tref = (GF_TrackReferenceTypeBox*)gf_list_get(trak->References->boxList, j);
			found = 0;
			for (k=0; k<tref->trackIDCount; k++) {
				if (tref->trackIDs[k] == the_trak->Header->trackID) found++;
			}
			if (!found) continue;
			//no more refs, remove this ref_type
			if (found == tref->trackIDCount) {
				gf_isom_box_del((GF_Box *)tref);
				gf_list_rem(trak->References->boxList, j);
				j--;
			} else {
				newRefs = (u32*)malloc(sizeof(u32) * (tref->trackIDCount - found));
				found = 0;
				for (k = 0; k < tref->trackIDCount; k++) {
					if (tref->trackIDs[k] != the_trak->Header->trackID) {
						newRefs[k-found] = tref->trackIDs[k];
					} else {
						found++;
					}
				}
				free(tref->trackIDs);
				tref->trackIDs = newRefs;
				tref->trackIDCount -= found;
			}
		}
		//a little opt: remove the ref box if empty...
		if (! gf_list_count(trak->References->boxList)) {
			gf_isom_box_del((GF_Box *)trak->References);
			trak->References = NULL;
		}
	}

	//delete the track
	gf_isom_box_del((GF_Box *)the_trak);
	
	/*update next track ID*/
	movie->moov->mvhd->nextTrackID = 0;
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		trak = gf_list_get(movie->moov->trackList, i);
		if (trak->Header->trackID>movie->moov->mvhd->nextTrackID)
			movie->moov->mvhd->nextTrackID = trak->Header->trackID;
	}
	return GF_OK;
}


GF_Err gf_isom_set_copyright(GF_ISOFile *movie, const char *threeCharCode, char *notice)
{
	GF_Err e;
	GF_CopyrightBox *ptr;
	GF_UserDataMap *map;
	u32 count, i;
	GF_UserDataMap *udta_getEntry(GF_UserDataBox *ptr, u32 boxType);
	
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	if (!notice || !threeCharCode) return GF_BAD_PARAM;

	gf_isom_insert_moov(movie);

	if (!movie->moov->udta) {
		e = moov_AddBox(movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		if (e) return e;
	}
	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_CPRT);
	
	if (map) {
		//try to find one in our language...
		count = gf_list_count(map->boxList);
		for (i=0; i<count; i++) {
			ptr = (GF_CopyrightBox*)gf_list_get(map->boxList, i);
			if (!strcmp(threeCharCode, (const char *) ptr->packedLanguageCode)) {
				free(ptr->notice);
				ptr->notice = (char*)malloc(sizeof(char) * (strlen(notice) + 1));
				strcpy(ptr->notice, notice);
				return GF_OK;
			}
		}
	}
	//nope, create one
	ptr = (GF_CopyrightBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_CPRT);

	memcpy(ptr->packedLanguageCode, threeCharCode, 4);
	ptr->notice = (char*)malloc(sizeof(char) * (strlen(notice)+1));
	strcpy(ptr->notice, notice);
	return udta_AddBox(movie->moov->udta, (GF_Box *) ptr);
}

GF_Err gf_isom_add_chapter(GF_ISOFile *movie, u32 trackNumber, u64 timestamp, char *name)
{
	GF_Err e;
	GF_ChapterListBox *ptr;
    u32 i, count;
	GF_ChapterEntry *ce;
	GF_UserDataBox *udta;
	GF_UserDataMap *map;
	GF_UserDataMap *udta_getEntry(GF_UserDataBox *ptr, u32 boxType);

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	gf_isom_insert_moov(movie);

	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) {
			e = trak_AddBox(trak, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
			if (e) return e;
		}
		udta = trak->udta;
	} else {
		if (!movie->moov->udta) {
			e = moov_AddBox(movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
			if (e) return e;
		}
		udta = movie->moov->udta;
	}

	ptr = NULL;
	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_CHPL);
	if (!map) {
		ptr = (GF_ChapterListBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_CHPL);
		e = udta_AddBox(udta, (GF_Box *) ptr);
		if (e) return e;
	} else {
		ptr = gf_list_get(map->boxList, 0);
	}
	/*this may happen if original MP4 is not properly formatted*/
	if (!ptr) {
		ptr = (GF_ChapterListBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_CHPL);
		gf_list_add(map->boxList, ptr);
	}

	GF_SAFEALLOC(ce, sizeof(GF_ChapterEntry));
	ce->start_time = timestamp * 10000L;
	ce->name = name ? strdup(name) : NULL;

	/*insert in order*/	
	count = gf_list_count(ptr->list);
	for (i=0; i<count; i++) {
		GF_ChapterEntry *ace = gf_list_get(ptr->list, i);
		if (ace->start_time == ce->start_time) {
			if (ace->name) free(ace->name);
			ace->name = ce->name;
			free(ce);
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
	GF_UserDataMap *udta_getEntry(GF_UserDataBox *ptr, u32 boxType);
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(movie);

	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) {
			e = trak_AddBox(trak, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
			if (e) return e;
		}
		udta = trak->udta;
	} else {
		if (!movie->moov->udta) {
			e = moov_AddBox(movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
			if (e) return e;
		}
		udta = movie->moov->udta;
	}

	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_CHPL);
	if (!map) return GF_OK;
	ptr = gf_list_get(map->boxList, 0);
	if (!ptr) return GF_OK;
	
	if (index) {
		ce = gf_list_get(ptr->list, index-1);
		if (!ce) return GF_BAD_PARAM;
		if (ce->name) free(ce->name);
		free(ce);
		gf_list_rem(ptr->list, index-1);
	} else {
		while (gf_list_count(ptr->list)) {
			ce = gf_list_get(ptr->list, 0);
			if (ce->name) free(ce->name);
			free(ce);
			gf_list_rem(ptr->list, 0);
		}
	}
	if (!gf_list_count(ptr->list)) {
		gf_list_del_item(udta->recordList, map);
		gf_isom_box_array_del(map->boxList);
		free(map);
	}
	return GF_OK;
}

GF_Err gf_isom_remove_copyright(GF_ISOFile *movie, u32 index)
{
	GF_Err e;
	GF_CopyrightBox *ptr;
	GF_UserDataMap *map;
	u32 count;
	GF_UserDataMap *udta_getEntry(GF_UserDataBox *ptr, u32 boxType);
	
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(movie);

	if (!index) return GF_BAD_PARAM;
	if (!movie->moov->udta) return GF_OK;

	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_CPRT);
	if (!map) return GF_OK;

	count = gf_list_count(map->boxList);
	if (index>count) return GF_BAD_PARAM;

	ptr = (GF_CopyrightBox*)gf_list_get(map->boxList, index-1);
	if (ptr) {
		gf_list_rem(map->boxList, index-1);
		if (ptr->notice) free(ptr->notice);
		free(ptr);
	}
	/*last copyright, remove*/
	if (!gf_list_count(map->boxList)) {
		gf_list_del_item(movie->moov->udta->recordList, map);
		gf_list_del(map->boxList);
		free(map);
	}
	return GF_OK;
}



GF_Err gf_isom_set_watermark(GF_ISOFile *movie, bin128 UUID, u8* data, u32 length)
{
	GF_Err e;
	GF_WatermarkBox *ptr;
	GF_UserDataMap *map;
	u32 count, i;
	GF_UserDataMap *udta_getEntry(GF_UserDataBox *ptr, u32 boxType);
	
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	gf_isom_insert_moov(movie);
	if (!movie->moov->udta) {
		e = moov_AddBox(movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		if (e) return e;
	}
	
	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_UUID);
	if (map) {
		count = gf_list_count(map->boxList);
		for(i=0; i<count; i++){
			ptr = (GF_WatermarkBox *)gf_list_get(map->boxList, i);
			
			if(!memcmp(UUID, ptr->uuid, 16)){
				free(ptr->data);
				ptr->data = (char*)malloc(length);
				memcpy(ptr->data, data, length);
				ptr->dataSize = length;
				return GF_OK;
			}
		}
	}
	//nope, create one
	ptr = (GF_WatermarkBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_UUID);

	memcpy(ptr->uuid, UUID, 16);
	ptr->data = (char*)malloc(length);
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
			free(stsz->sizes);
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
		if (stsz->sizes) free(stsz->sizes);
		stsz->sizes = malloc(sizeof(u32)*stsz->sampleCount);
		memset(stsz->sizes, stsz->sampleSize, sizeof(u32));
	}
	//set the SampleSize to 0 while the file is open
	stsz->sampleSize = 0;
	stsz->type = GF_ISOM_BOX_TYPE_STZ2;
	return GF_OK;
}



GF_Err gf_isom_set_brand_info(GF_ISOFile *movie, u32 MajorBrand, u32 MinorVersion)
{
	u32 i, *p;
	GF_Err e;

	if (!MajorBrand) return GF_BAD_PARAM;
	
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	e = CheckNoData(movie);
	if (e) return e;


	if (!movie->brand) {
		movie->brand = (GF_FileTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_FTYP);
		gf_list_add(movie->TopBoxes, movie->brand);
	}
	
	movie->brand->majorBrand = MajorBrand;
	movie->brand->minorVersion = MinorVersion;

	if (!movie->brand->altBrand) {
		movie->brand->altBrand = malloc(sizeof(u32));
		movie->brand->altBrand[0] = MajorBrand;
		movie->brand->altCount = 1;
		return GF_OK;
	}

	//if brand already present don't change anything
	for (i=0; i<movie->brand->altCount; i++) {
		if (movie->brand->altBrand[i] == MajorBrand) return GF_OK;
	}
	p = malloc(sizeof(u32)*(movie->brand->altCount + 1));
	if (!p) return GF_OUT_OF_MEM;
	memcpy(p, movie->brand->altBrand, sizeof(u32)*movie->brand->altCount);
	p[movie->brand->altCount] = MajorBrand;
	movie->brand->altCount += 1;
	free(movie->brand->altBrand);
	movie->brand->altBrand = p;
	return GF_OK;
}

GF_Err gf_isom_modify_alternate_brand(GF_ISOFile *movie, u32 Brand, u8 AddIt)
{
	u32 i, k, *p;
	GF_Err e;
	
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	if (!Brand || !movie->brand) return GF_BAD_PARAM;

	e = CheckNoData(movie);
	if (e) return e;

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
	p = malloc(sizeof(u32)*(movie->brand->altCount + 1));
	if (!p) return GF_OUT_OF_MEM;
	memcpy(p, movie->brand->altBrand, sizeof(u32)*movie->brand->altCount);
	p[movie->brand->altCount] = Brand;
	movie->brand->altCount += 1;
	free(movie->brand->altBrand);
	movie->brand->altBrand = p;
	return GF_OK;

found:

	//found
	if (AddIt) return GF_OK;
	assert(movie->brand->altCount>1);

	//remove it
	p = malloc(sizeof(u32)*(movie->brand->altCount - 1));
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
	free(movie->brand->altBrand);
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

	for (i=0; i<gf_list_count(udta->recordList); i++) {
		map = gf_list_get(udta->recordList, i);
		if ((map->boxType == GF_ISOM_BOX_TYPE_UUID)  && !memcmp(map->uuid, UUID, 16)) goto found;
		else if (map->boxType == UserDataType) goto found;
	}
	//not found
	return GF_OK;

found:

	if (UserDataIndex > gf_list_count(map->boxList) ) return GF_BAD_PARAM;
	//delete the box
	a = gf_list_get(map->boxList, UserDataIndex-1);
	
	gf_list_rem(map->boxList, UserDataIndex-1);
	gf_isom_box_del(a);

	//remove the map if empty
	if (!gf_list_count(map->boxList)) {
		gf_list_rem(udta->recordList, i);
		gf_isom_box_array_del(map->boxList);
		free(map);
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

	for (i=0; i<gf_list_count(udta->recordList); i++) {
		map = gf_list_get(udta->recordList, i);
		if ((map->boxType == GF_ISOM_BOX_TYPE_UUID)  && !memcmp(map->uuid, UUID, 16)) goto found;
		else if (map->boxType == UserDataType) goto found;
	}
	//not found
	return GF_OK;

found:

	gf_list_rem(udta->recordList, i);
	gf_isom_box_array_del(map->boxList);
	free(map);

	//but we keep the UDTA no matter what
	return GF_OK;
}

GF_Err gf_isom_add_user_data(GF_ISOFile *movie, u32 trackNumber, u32 UserDataType, bin128 UUID, char *data, u32 DataLength)
{
	GF_UnknownBox *a;
	bin128 t;
	GF_Err e;
	GF_TrackBox *trak;
	GF_UserDataBox *udta;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	if (UserDataType == GF_ISOM_BOX_TYPE_UUID) UserDataType = 0;

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) trak_AddBox(trak, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		udta = trak->udta;
	} else {
		if (!movie->moov->udta) moov_AddBox(movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		udta = movie->moov->udta;
	}
	if (!udta) return GF_OUT_OF_MEM;

	//create a default box
	if (UserDataType) {
		a = (GF_UnknownBox *) gf_isom_box_new(UserDataType);
	} else {
		a = (GF_UnknownBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_UUID);
		memcpy(a->uuid, UUID, 16);
	}

	memset(t, 1, 16);
	if ( (a->type != GF_ISOM_BOX_TYPE_UUID) && memcmp(a->uuid, t, 16)) {
		gf_isom_box_del((GF_Box *)a);
		return GF_BAD_PARAM;
	}
	a->data = malloc(sizeof(char)*DataLength);
	memcpy(a->data, data, DataLength);
	a->dataSize = DataLength;

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
		GF_Descriptor *d = gf_list_get(iod_d->ES_ID_IncDescriptors, 0);
		gf_list_rem(iod_d->ES_ID_IncDescriptors, 0);
		gf_odf_desc_del(d);
	}
	while (gf_list_count(iod_d->ES_ID_RefDescriptors)) {
		GF_Descriptor *d = gf_list_get(iod_d->ES_ID_RefDescriptors, 0);
		gf_list_rem(iod_d->ES_ID_RefDescriptors, 0);
		gf_odf_desc_del(d);
	}
	return GF_OK;
}



GF_Err gf_isom_clone_track(GF_ISOFile *orig_file, u32 orig_track, GF_ISOFile *dest_file, Bool keep_data_ref, u32 *dest_track)
{
	GF_TrackBox *trak, *new_tk;
	GF_BitStream *bs;
	unsigned char *data;
	u32 data_size;
	GF_Err e;
	GF_SampleEntryBox *entry;
	GF_SampleTableBox *stbl, *stbl_temp;
	
	e = CanAccessMovie(dest_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	gf_isom_insert_moov(dest_file);

	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(orig_file, orig_track);
	if (!trak || !trak->Media) return GF_BAD_PARAM;
	if (gf_list_count(trak->Media->information->sampleTable->SampleDescription->boxList)>1) return GF_NOT_SUPPORTED;

	stbl = trak->Media->information->sampleTable;
	stbl_temp = (GF_SampleTableBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STBL);
	stbl_temp->SampleDescription = stbl->SampleDescription;
	trak->Media->information->sampleTable = stbl_temp;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_isom_box_size( (GF_Box *) trak);
	gf_isom_box_write((GF_Box *) trak, bs);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	e = gf_isom_parse_box((GF_Box **) &new_tk, bs);
	gf_bs_del(bs);
	free(data);
	trak->Media->information->sampleTable = stbl;

	stbl_temp->SampleDescription = NULL;
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

	moov_AddBox(dest_file->moov, (GF_Box *)new_tk);

	/*reset data ref*/
	if (!keep_data_ref) {
		gf_isom_box_array_del(new_tk->Media->information->dataInformation->dref->boxList);
		new_tk->Media->information->dataInformation->dref->boxList = gf_list_new();
		/*update data ref*/
		entry = gf_list_get(new_tk->Media->information->sampleTable->SampleDescription->boxList, 0);
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


GF_Err gf_isom_clone_sample_description(GF_ISOFile *the_file, u32 trackNumber, GF_ISOFile *orig_file, u32 orig_track, u32 orig_desc_index, char *URLname, char *URNname, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_BitStream *bs;
	unsigned char *data;
	u32 data_size;
	GF_Box *entry;
	GF_Err e;
	u32 dataRefIndex;
	GF_TrackReferenceTypeBox *dpnd;
	GF_TrackReferenceBox *tref;
	
	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	/*get orig sample desc and clone it*/
	trak = gf_isom_get_track_from_file(orig_file, orig_track);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	entry = gf_list_get(trak->Media->information->sampleTable->SampleDescription->boxList, orig_desc_index-1);
	if (!entry) return GF_BAD_PARAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

	gf_isom_box_size(entry);
	gf_isom_box_write(entry, bs);
	gf_bs_get_content(bs, &data, &data_size);
	gf_bs_del(bs);
	bs = gf_bs_new(data, data_size, GF_BITSTREAM_READ);
	e = gf_isom_parse_box(&entry, bs);
	gf_bs_del(bs);
	free(data);
	if (e) return e;

	/*get new track and insert clone*/
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) goto exit;
	dpnd = NULL;
	tref = NULL;

	/*get or create the data ref*/
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) goto exit;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) goto exit;
	}
	trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();
	/*overwrite dref*/
	((GF_SampleEntryBox *)entry)->dataReferenceIndex = dataRefIndex;
	e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->boxList, entry);
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->boxList);

	/*also clone track w/h info*/	
	if (gf_isom_get_media_type(the_file, trackNumber) == GF_ISOM_MEDIA_VISUAL) {
		gf_isom_set_visual_info(the_file, trackNumber, (*outDescriptionIndex), ((GF_VisualSampleEntryBox*)entry)->Width, ((GF_VisualSampleEntryBox*)entry)->Height);
	}
	return e;
	
exit:
	gf_isom_box_del(entry);
	return e;
}


GF_Err gf_isom_new_generic_sample_description(GF_ISOFile *movie, u32 trackNumber, u32 entry_type, bin128 entry_UUID, char *URLname, char *URNname, GF_GenericSampleDescription *udesc, u32 *outDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Err e;
	u32 dataRefIndex;
	GF_TrackReferenceTypeBox *dpnd;
	GF_TrackReferenceBox *tref;

	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !udesc) return GF_BAD_PARAM;

	dpnd = NULL;
	tref = NULL;

	//get or create the data ref
	e = Media_FindDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
	if (e) return e;
	if (!dataRefIndex) {
		e = Media_CreateDataRef(trak->Media->information->dataInformation->dref, URLname, URNname, &dataRefIndex);
		if (e) return e;
	}
	trak->Media->mediaHeader->modificationTime = gf_isom_get_mp4time();

	if (trak->Media->handler->type==GF_ISOM_MEDIA_VISUAL) {
		GF_GenericVisualSampleEntryBox *entry;
		//create a new entry
		entry = (GF_GenericVisualSampleEntryBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRV);
		if (!entry) return GF_OUT_OF_MEM;

		if (!entry_type) {
			entry->EntryType = GF_ISOM_BOX_TYPE_UUID;
			memcpy(entry->uuid, entry_UUID, sizeof(bin128));
		} else {
			entry->EntryType = entry_type;
		}
		entry->dataReferenceIndex = dataRefIndex;
		entry->vendor = udesc->vendor_code;
		entry->version = udesc->version;
		entry->revision = udesc->revision;
		entry->temporal_quality = udesc->temporal_quality;
		entry->spacial_quality = udesc->spacial_quality;
		entry->Width = udesc->width;
		entry->Height = udesc->height;
		entry->bit_depth = udesc->bitsPerSample;
		strcpy(entry->compressor_name, udesc->szCompressorName);
		entry->color_table_index = -1;
		entry->frames_per_sample = 1;
		entry->horiz_res = udesc->h_res;
		entry->vert_res = udesc->v_res;
		if (udesc->extension_buf && udesc->extension_buf_size) {
			entry->data = malloc(sizeof(unsigned char) * udesc->extension_buf_size);
			if (!entry->data) {
				gf_isom_box_del((GF_Box *) entry);
				return GF_OUT_OF_MEM;
			}
			memcpy(entry->data, udesc->extension_buf, udesc->extension_buf_size);
			entry->data_size = udesc->extension_buf_size;
		}
		e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->boxList, entry);
	}
	else if (trak->Media->handler->type==GF_ISOM_MEDIA_AUDIO) {
		GF_GenericAudioSampleEntryBox *gena;
		//create a new entry
		gena = (GF_GenericAudioSampleEntryBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRA);
		if (!gena) return GF_OUT_OF_MEM;

		if (!entry_type) {
			gena->EntryType = GF_ISOM_BOX_TYPE_UUID;
			memcpy(gena->uuid, entry_UUID, sizeof(bin128));
		} else {
			gena->EntryType = entry_type;
		}
		gena->dataReferenceIndex = dataRefIndex;
		gena->vendor = udesc->vendor_code;
		gena->version = udesc->version;
		gena->revision = udesc->revision;
		gena->bitspersample = udesc->bitsPerSample;
		gena->channel_count = udesc->NumChannels;
		gena->samplerate_hi = udesc->SampleRate>>16;
		gena->samplerate_lo = udesc->SampleRate & 0xFF;

		if (udesc->extension_buf && udesc->extension_buf_size) {
			gena->data = malloc(sizeof(unsigned char) * udesc->extension_buf_size);
			if (!gena->data) {
				gf_isom_box_del((GF_Box *) gena);
				return GF_OUT_OF_MEM;
			}
			memcpy(gena->data, udesc->extension_buf, udesc->extension_buf_size);
			gena->data_size = udesc->extension_buf_size;
		}
		e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->boxList, gena);
	}
	else {
		GF_GenericSampleEntryBox *genm;
		//create a new entry
		genm = (GF_GenericSampleEntryBox*) gf_isom_box_new(GF_ISOM_BOX_TYPE_GNRM);
		if (!genm) return GF_OUT_OF_MEM;

		if (!entry_type) {
			genm->EntryType = GF_ISOM_BOX_TYPE_UUID;
			memcpy(genm->uuid, entry_UUID, sizeof(bin128));
		} else {
			genm->EntryType = entry_type;
		}
		genm->dataReferenceIndex = dataRefIndex;
		if (udesc->extension_buf && udesc->extension_buf_size) {
			genm->data = malloc(sizeof(unsigned char) * udesc->extension_buf_size);
			if (!genm->data) {
				gf_isom_box_del((GF_Box *) genm);
				return GF_OUT_OF_MEM;
			}
			memcpy(genm->data, udesc->extension_buf, udesc->extension_buf_size);
			genm->data_size = udesc->extension_buf_size;
		}
		e = gf_list_add(trak->Media->information->sampleTable->SampleDescription->boxList, genm);
	}
	*outDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->boxList);
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

	entry = gf_list_get(trak->Media->information->sampleTable->SampleDescription->boxList, StreamDescriptionIndex-1);
	if (!entry) return GF_BAD_PARAM;
	if (entry->type == GF_ISOM_BOX_TYPE_GNRV) {
		entry->vendor = udesc->vendor_code;
		entry->version = udesc->version;
		entry->revision = udesc->revision;
		entry->temporal_quality = udesc->temporal_quality;
		entry->spacial_quality = udesc->spacial_quality;
		entry->Width = udesc->width;
		entry->Height = udesc->height;
		entry->bit_depth = udesc->bitsPerSample;
		strcpy(entry->compressor_name, udesc->szCompressorName);
		entry->color_table_index = -1;
		entry->frames_per_sample = 1;
		entry->horiz_res = udesc->h_res;
		entry->vert_res = udesc->v_res;
		if (entry->data) free(entry->data);
		entry->data = NULL;
		entry->data_size = 0;
		if (udesc->extension_buf && udesc->extension_buf_size) {
			entry->data = malloc(sizeof(unsigned char) * udesc->extension_buf_size);
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
		gena->bitspersample = udesc->bitsPerSample;
		gena->channel_count = udesc->NumChannels;
		gena->samplerate_hi = udesc->SampleRate>>16;
		gena->samplerate_lo = udesc->SampleRate & 0xFF;
		if (gena->data) free(gena->data);
		gena->data = NULL;
		gena->data_size = 0;

		if (udesc->extension_buf && udesc->extension_buf_size) {
			gena->data = malloc(sizeof(unsigned char) * udesc->extension_buf_size);
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
		if (genm->data) free(genm->data);
		genm->data = NULL;
		genm->data_size = 0;

		if (udesc->extension_buf && udesc->extension_buf_size) {
			genm->data = malloc(sizeof(unsigned char) * udesc->extension_buf_size);
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
	entry = gf_list_get(trak->Media->information->sampleTable->SampleDescription->boxList, streamDescIndex-1);
	if (!entry) return GF_BAD_PARAM;
	gf_list_rem(trak->Media->information->sampleTable->SampleDescription->boxList, streamDescIndex-1);
	gf_isom_box_del(entry);
	return GF_OK;
}


//sets a track reference
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
		e = trak_AddBox(trak, (GF_Box *) tref);
		if (e) return e;
	}
	//find a ref of the given type
	e = Track_FindRef(trak, referenceType, &dpnd);
	if (!dpnd) {
		dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new(referenceType);
		e = tref_AddBox(tref, (GF_Box *)dpnd);
		if (e) return e;
	}
	//add the ref
	return reftype_AddRefTrack(dpnd, ReferencedTrackID, NULL);
}

//removes a track reference
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
		for (i=0; i<gf_list_count(tref->boxList); i++) {
			tmp = gf_list_get(tref->boxList, i);
			if (tmp==dpnd) {
				gf_list_rem(tref->boxList, i);
				gf_isom_box_del((GF_Box *) dpnd);
				return GF_OK;
			}
		}
	}
	k = 0;
	newIDs = malloc(sizeof(u32)*(dpnd->trackIDCount-1));
	for (i=0; i<dpnd->trackIDCount; i++) {
		if (i+1 != ReferenceIndex) {
			newIDs[k] = dpnd->trackIDs[i];
			k++;
		}
	}
	free(dpnd->trackIDs);
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
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		a_trak = gf_list_get(movie->moov->trackList, i);
		if (!a_trak->References) continue;
		for (j=0; j<gf_list_count(a_trak->References->boxList); j++) {
			ref = gf_list_get(a_trak->References->boxList, j);
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
		u32 i;
		GF_IsomObjectDescriptor *od = (GF_IsomObjectDescriptor *)movie->moov->iods->descriptor;
		for (i=0; i<gf_list_count(od->ES_ID_IncDescriptors); i++) {
			GF_ES_ID_Inc *inc = gf_list_get(od->ES_ID_IncDescriptors, i);
			if (inc->trackID==trak->Header->trackID) inc->trackID = trackID;
		}
	}

	trak->Header->trackID = trackID;
	return GF_OK;
}


GF_Err gf_isom_modify_cts_offset(GF_ISOFile *the_file, u32 trackNumber, u32 sample_number, u32 offset)
{
	GF_DttsEntry *ent;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->CompositionOffset) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->CompositionOffset->unpack_mode) return GF_BAD_PARAM;
	ent = gf_list_get(trak->Media->information->sampleTable->CompositionOffset->entryList, sample_number - 1);
	if (!ent) return GF_BAD_PARAM;
	ent->decodingOffset = offset;
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

GF_Err gf_isom_set_cts_packing(GF_ISOFile *the_file, u32 trackNumber, Bool unpack)
{
	GF_Err stbl_repackCTS(GF_CompositionOffsetBox *ctts);
	GF_Err stbl_unpackCTS(GF_SampleTableBox *stbl);

	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (unpack) {
		if (!trak->Media->information->sampleTable->CompositionOffset) trak->Media->information->sampleTable->CompositionOffset = (GF_CompositionOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CTTS);
		return stbl_unpackCTS(trak->Media->information->sampleTable);
	}
	if (!trak->Media->information->sampleTable->CompositionOffset) return GF_OK;
	return stbl_repackCTS(trak->Media->information->sampleTable->CompositionOffset);
}

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
	if (trak->name) free (trak->name);
	trak->name = NULL;
	if (name) trak->name = strdup(name);
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

	gf_isom_remove_user_data(movie, 0, GF_FOUR_CHAR_INT('G','P','A','C'), binID);
	count = gf_isom_get_track_count(movie);
	for (i=0; i<count; i++) gf_isom_remove_user_data(movie, i+1, GF_FOUR_CHAR_INT('G','P','A','C'), binID);

	if (remove_all) return GF_OK;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	/*update movie: storage mode and interleaving type*/
	gf_bs_write_u8(bs, 0xFE); /*marker*/
	gf_bs_write_u8(bs, movie->storageMode);
	gf_bs_write_u32(bs, movie->interleavingTime);
	gf_bs_get_content(bs, (unsigned char **) &data, &len);
	gf_bs_del(bs);
	gf_isom_add_user_data(movie, 0, GF_FOUR_CHAR_INT('G','P','A','C'), binID, data, len);
	free(data);
	/*update tracks: interleaving group/priority and track edit name*/
	for (i=0; i<count; i++) {
		u32 j;
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, i+1);
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_u8(bs, 0xFE);	/*marker*/
		gf_bs_write_u32(bs, trak->Media->information->sampleTable->groupID);
		gf_bs_write_u32(bs, trak->Media->information->sampleTable->trackPriority);
		len = trak->name ? strlen(trak->name) : 0;
		gf_bs_write_u32(bs, len);
		for (j=0; j<len; j++) gf_bs_write_u8(bs, trak->name[j]);
		gf_bs_get_content(bs, (unsigned char **) &data, &len);
		gf_bs_del(bs);
		gf_isom_add_user_data(movie, i+1, GF_FOUR_CHAR_INT('G','P','A','C'), binID, data, len);
		free(data);
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
	count = gf_isom_get_user_data_count(movie, 0, GF_FOUR_CHAR_INT('G','P','A','C'), binID);
	for (i=0; i<count; i++) {
		data = NULL;
		gf_isom_get_user_data(movie, 0, GF_FOUR_CHAR_INT('G','P','A','C'), binID, i+1, &data, &len);
		if (!data) continue;
		/*check marker*/
		if ((unsigned char) data[0] != 0xFE) {
			free(data);
			continue;
		}
		bs = gf_bs_new(data, len, GF_BITSTREAM_READ);
		gf_bs_read_u8(bs);	/*marker*/
		movie->storageMode = gf_bs_read_u8(bs);
		movie->interleavingTime = gf_bs_read_u32(bs);
		gf_bs_del(bs);
		free(data);
		found_cfg = 1;
		break;
	}

	for (i=0; i<gf_isom_get_track_count(movie); i++) {
		u32 j;
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, i+1);
		count = gf_isom_get_user_data_count(movie, i+1, GF_FOUR_CHAR_INT('G','P','A','C'), binID);
		for (j=0; j<count; j++) {
			data = NULL;
			gf_isom_get_user_data(movie, i+1, GF_FOUR_CHAR_INT('G','P','A','C'), binID, j+1, &data, &len);
			if (!data) continue;
			/*check marker*/
			if ((unsigned char) data[0] != 0xFE) {
				free(data);
				continue;
			}
			bs = gf_bs_new(data, len, GF_BITSTREAM_READ);
			gf_bs_read_u8(bs);	/*marker*/
			trak->Media->information->sampleTable->groupID = gf_bs_read_u32(bs);
			trak->Media->information->sampleTable->trackPriority = gf_bs_read_u32(bs);
			len = gf_bs_read_u32(bs);
			if (len) {
				u32 k;
				trak->name = malloc(sizeof(char)*(len+1));
				for (k=0;k<len;k++) trak->name[k] = gf_bs_read_u8(bs);
				trak->name[k] = 0;
			}
			gf_bs_del(bs);
			free(data);
			found_cfg = 1;
			break;
		}
	}
	return found_cfg ? GF_OK : GF_NOT_SUPPORTED;
}

GF_Err gf_isom_set_temp_dir(GF_ISOFile *movie, const char *tmpdir)
{
	GF_Err e;
	if (!movie) return GF_BAD_PARAM;
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;
	if (!movie->editFileMap || gf_bs_get_position(movie->editFileMap->bs)) return GF_BAD_PARAM;

	gf_isom_datamap_del(movie->editFileMap);
	movie->editFileMap = NULL;
	return gf_isom_datamap_new("mp4_tmp_edit", tmpdir, GF_ISOM_DATA_MAP_WRITE, & movie->editFileMap);
}

GF_Err gf_isom_set_media_timescale(GF_ISOFile *the_file, u32 trackNumber, u32 newTS)
{
	Double scale;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media | !trak->Media->mediaHeader) return GF_BAD_PARAM;
	if (trak->Media->mediaHeader->timeScale==newTS) return GF_OK;

	scale = newTS;
	scale /= trak->Media->mediaHeader->timeScale;
	trak->Media->mediaHeader->timeScale = newTS;
	if (trak->editBox) {
		u32 i;
		for (i=0; i<gf_list_count(trak->editBox->editList->entryList); i++) {
			GF_EdtsEntry *ent = (GF_EdtsEntry*)gf_list_get(trak->editBox->editList->entryList, i);
			ent->mediaTime = (u32) (scale*ent->mediaTime);
		}
	}
	return SetTrackDuration(trak);
}



GF_Err gf_isom_is_same_sample_description(GF_ISOFile *f1, u32 tk1, GF_ISOFile *f2, u32 tk2)
{
	u32 i;
	GF_TrackBox *trak1, *trak2;
	GF_BitStream *bs;
	unsigned char *data1, *data2;
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
	if (gf_list_count(trak1->Media->information->sampleTable->SampleDescription->boxList) != gf_list_count(trak2->Media->information->sampleTable->SampleDescription->boxList)) return 0;

	need_memcmp = 1;

	for (i=0; i<gf_list_count(trak1->Media->information->sampleTable->SampleDescription->boxList); i++) {
		GF_Box *ent1 = gf_list_get(trak1->Media->information->sampleTable->SampleDescription->boxList, i);
		GF_Box *ent2 = gf_list_get(trak2->Media->information->sampleTable->SampleDescription->boxList, i);
		if (ent1->type != ent2->type) return 0;

		switch (ent1->type) {
		/*for MPEG-4 streams, only compare decSpecInfo (bitrate may not be the same but that's not an issue)*/
		case GF_ISOM_BOX_TYPE_MP4S:
		case GF_ISOM_BOX_TYPE_MP4A:
		case GF_ISOM_BOX_TYPE_MP4V:
		case GF_ISOM_BOX_TYPE_ENCA:
		case GF_ISOM_BOX_TYPE_ENCV:
		case GF_ISOM_BOX_TYPE_ENCS:
			Media_GetESD(trak1->Media, i+1, &esd1, 1);
			Media_GetESD(trak2->Media, i+1, &esd2, 1);
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
		{
			GF_AVCSampleEntryBox *avc1 = (GF_AVCSampleEntryBox *)ent1;
			GF_AVCSampleEntryBox *avc2 = (GF_AVCSampleEntryBox *)ent2;
			data1 = data2 = NULL;

			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			a = (GF_Box *)avc1->avc_config;
			gf_isom_box_size(a);
			gf_isom_box_write(a, bs);
			gf_bs_get_content(bs, &data1, &data1_size);
			gf_bs_del(bs);

			bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			a = (GF_Box *)avc2->avc_config;
			gf_isom_box_size(a);
			gf_isom_box_write(a, bs);
			gf_bs_get_content(bs, &data2, &data2_size);
			gf_bs_del(bs);
			
			ret = 0;
			if (data1_size == data2_size) {
				ret = (memcmp(data1, data2, sizeof(char)*data1_size)==0) ? 1 : 0;
			}
			free(data1);
			free(data2);
			return ret;
		}
			break;
		}
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
	free(data1);
	free(data2);
	return ret;
}

u64 gf_isom_estimate_size(GF_ISOFile *movie)
{
	GF_Err e;
	u32 i;
	u64 mdat_size;
	if (!movie) return 0;

	mdat_size = 0;
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		mdat_size += gf_isom_get_media_data_size(movie, i+1);
	}
	mdat_size += 8;
	if (mdat_size > 0xFFFFFFFF) mdat_size += 8;

	for (i=0; i<gf_list_count(movie->TopBoxes); i++) {
		GF_Box *a = gf_list_get(movie->TopBoxes, i);
		e = gf_isom_box_size(a);
		mdat_size += a->size;
	}
	return mdat_size;
}


//set shadowing on/off
GF_Err gf_isom_set_sync_shadow_enabled(GF_ISOFile *movie, u32 trackNumber, u8 SyncShadowEnabled)
{
	GF_TrackBox *trak;
	GF_SampleTableBox *stbl;

	if (movie->openMode == GF_ISOM_OPEN_READ) return GF_ISOM_INVALID_MODE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;
	if (SyncShadowEnabled) {
		if (!stbl->ShadowSync) stbl->ShadowSync = (GF_ShadowSyncBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSH);
	} else {
		if (stbl->ShadowSync) gf_isom_box_del((GF_Box *) stbl->ShadowSync);
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
	if (!stbl->ShadowSync) return GF_BAD_PARAM;

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
	if (!trak) return GF_BAD_PARAM;
	return trak->Media->information->sampleTable->trackPriority;
	return GF_OK;
}


GF_Err gf_isom_make_interleave(GF_ISOFile *file, Double TimeInSec)
{
	GF_Err e;
	if (gf_isom_get_mode(file) < GF_ISOM_OPEN_EDIT) return GF_BAD_PARAM;
	e = gf_isom_set_storage_mode(file, GF_ISOM_STORE_INTERLEAVED);
	if (e) return e;
	return gf_isom_set_interleave_time(file, (u32) (TimeInSec * gf_isom_get_timescale(file)));
}

#endif	//GPAC_READ_ONLY


