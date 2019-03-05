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

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_HINTING)

Bool IsHintTrack(GF_TrackBox *trak)
{
	if (trak->Media->handler->handlerType != GF_ISOM_MEDIA_HINT) return GF_FALSE;
	//QT doesn't specify any InfoHeader on HintTracks
	if (trak->Media->information->InfoHeader
	        && (trak->Media->information->InfoHeader->type != GF_ISOM_BOX_TYPE_HMHD)
	        && (trak->Media->information->InfoHeader->type != GF_ISOM_BOX_TYPE_NMHD)
		)
		return GF_FALSE;

	return GF_TRUE;
}

u32 GetHintFormat(GF_TrackBox *trak)
{
	GF_HintMediaHeaderBox *hmhd = (GF_HintMediaHeaderBox *)trak->Media->information->InfoHeader;
	if (!hmhd || !hmhd->subType) {
		GF_Box *a = (GF_Box *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, 0);
		if (!hmhd) return a ? a->type : 0;
		if (a) hmhd->subType = a->type;
	}
	return hmhd->subType;
}

Bool CheckHintFormat(GF_TrackBox *trak, u32 HintType)
{
	if (!IsHintTrack(trak)) return GF_FALSE;
	if (GetHintFormat(trak) != HintType) return GF_FALSE;
	return GF_TRUE;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err AdjustHintInfo(GF_HintSampleEntryBox *entry, u32 HintSampleNumber)
{
	u32 offset, count, i, size;
	GF_HintPacket *pck;
	GF_Err e;

	offset = gf_isom_hint_sample_size(entry->hint_sample) - entry->hint_sample->dataLength;
	count = gf_list_count(entry->hint_sample->packetTable);

	for (i=0; i<count; i++) {
		pck = (GF_HintPacket *)gf_list_get(entry->hint_sample->packetTable, i);
		if (offset && entry->hint_sample->dataLength) {
			//adjust any offset in this packet
			e = gf_isom_hint_pck_offset(pck, offset, HintSampleNumber);
			if (e) return e;
		}
		//adjust the max packet size for this sample entry...
		size = gf_isom_hint_pck_length(pck);
		if (entry->MaxPacketSize < size) entry->MaxPacketSize = size;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_setup_hint_track(GF_ISOFile *movie, u32 trackNumber, u32 HintType)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_TrackReferenceBox *tref;
	GF_TrackReferenceTypeBox *dpnd;
	GF_HintMediaHeaderBox *hmhd;
	//UDTA related ...
	GF_UserDataBox *udta;


	//what do we support
	switch (HintType) {
	case GF_ISOM_HINT_RTP:
		break;
	default:
		return GF_NOT_SUPPORTED;
	}
	e = CanAccessMovie(movie, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return gf_isom_last_error(movie);

	//check we have a hint ...
	if ( !IsHintTrack(trak)) {
		return GF_BAD_PARAM;
	}
	hmhd = (GF_HintMediaHeaderBox *)trak->Media->information->InfoHeader;
	//make sure the subtype was not already defined
	if (hmhd->subType) return GF_BAD_PARAM;
	//store the HintTrack format for later use...
	hmhd->subType = HintType;


	//hint tracks always have a tref and everything ...
	if (!trak->References) {
		if (!trak->References) {
			tref = (GF_TrackReferenceBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TREF);
			e = trak_AddBox((GF_Box*)trak, (GF_Box *)tref);
			if (e) return e;
		}
	}
	tref = trak->References;

	//do we have a hint reference on this trak ???
	e = Track_FindRef(trak, GF_ISOM_BOX_TYPE_HINT, &dpnd);
	if (e) return e;
	//if yes, return false (existing hint track...)
	if (dpnd) return GF_BAD_PARAM;

	//create our dep
	dpnd = (GF_TrackReferenceTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_REFT);
	dpnd->reference_type = GF_ISOM_BOX_TYPE_HINT;
	e = tref_AddBox((GF_Box*)tref, (GF_Box *) dpnd);
	if (e) return e;

	//for RTP, we need to do some UDTA-related stuff...
	if (HintType != GF_ISOM_HINT_RTP) return GF_OK;

	if (!trak->udta) {
		//create one
		udta = (GF_UserDataBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA);
		e = trak_AddBox((GF_Box*)trak, (GF_Box *) udta);
		if (e) return e;
	}
	udta = trak->udta;

	//HNTI
	e = udta_AddBox((GF_Box *)udta, gf_isom_box_new(GF_ISOM_BOX_TYPE_HNTI));
	if (e) return e;

	/*
		//NAME
		e = udta_AddBox((GF_Box *)udta, gf_isom_box_new(GF_ISOM_BOX_TYPE_NAME));
		if (e) return e;
		//HINF
		return udta_AddBox((GF_Box *)udta, gf_isom_box_new(GF_ISOM_BOX_TYPE_HINF));
	*/
	return GF_OK;
}

//to use with internally supported protocols
GF_EXPORT
GF_Err gf_isom_new_hint_description(GF_ISOFile *the_file, u32 trackNumber, s32 HintTrackVersion, s32 LastCompatibleVersion, u8 Rely, u32 *HintDescriptionIndex)
{
	GF_Err e;
	u32 drefIndex;
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *hdesc;
	GF_RelyHintBox *relyA;

	e = CanAccessMovie(the_file, GF_ISOM_OPEN_WRITE);
	if (e) return e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	*HintDescriptionIndex = 0;
	if (!trak || !IsHintTrack(trak)) return GF_BAD_PARAM;

	//OK, create a new HintSampleDesc
	hdesc = (GF_HintSampleEntryBox *) gf_isom_box_new(GetHintFormat(trak));

	if (HintTrackVersion > 0) hdesc->HintTrackVersion = HintTrackVersion;
	if (LastCompatibleVersion > 0) hdesc->LastCompatibleVersion = LastCompatibleVersion;

	//create a data reference - WE ONLY DEAL WITH SELF-CONTAINED HINT TRACKS
	e = Media_CreateDataRef(the_file, trak->Media->information->dataInformation->dref, NULL, NULL, &drefIndex);
	if (e) return e;
	hdesc->dataReferenceIndex = drefIndex;

	//add the entry to our table...
	e = stsd_AddBox((GF_Box*)trak->Media->information->sampleTable->SampleDescription, (GF_Box *) hdesc);
	if (e) return e;
	*HintDescriptionIndex = gf_list_count(trak->Media->information->sampleTable->SampleDescription->other_boxes);

	//RTP needs a default timeScale... use the media one.
	if (CheckHintFormat(trak, GF_ISOM_HINT_RTP)) {
		e = gf_isom_rtp_set_timescale(the_file, trackNumber, *HintDescriptionIndex, trak->Media->mediaHeader->timeScale);
		if (e) return e;
	}
	if (!Rely) return GF_OK;

	//we need a rely box (common to all protocols)
	relyA = (GF_RelyHintBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_RELY);
	if (Rely == 1) {
		relyA->prefered = 1;
	} else {
		relyA->required = 1;
	}
	return gf_isom_box_add_default((GF_Box*)hdesc, (GF_Box*)relyA);
}


/*******************************************************************
					RTP WRITING API
*******************************************************************/

//sets the RTP TimeScale
GF_EXPORT
GF_Err gf_isom_rtp_set_timescale(GF_ISOFile *the_file, u32 trackNumber, u32 HintDescriptionIndex, u32 TimeScale)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *hdesc;
	u32 i, count;
	GF_TSHintEntryBox *ent;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return GF_BAD_PARAM;

	//OK, create a new HintSampleDesc
	hdesc = (GF_HintSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, HintDescriptionIndex - 1);
	count = gf_list_count(hdesc->other_boxes);

	for (i=0; i< count; i++) {
		ent = (GF_TSHintEntryBox *)gf_list_get(hdesc->other_boxes, i);
		if (ent->type == GF_ISOM_BOX_TYPE_TIMS) {
			ent->timeScale = TimeScale;
			return GF_OK;
		}
	}
	//we have to create a new entry...
	ent = (GF_TSHintEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TIMS);
	ent->timeScale = TimeScale;
	return gf_isom_box_add_default((GF_Box*) hdesc, (GF_Box*) ent);
}

//sets the RTP TimeOffset that the server will add to the packets
//if not set, the server adds a random offset
GF_EXPORT
GF_Err gf_isom_rtp_set_time_offset(GF_ISOFile *the_file, u32 trackNumber, u32 HintDescriptionIndex, u32 TimeOffset)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *hdesc;
	u32 i, count;
	GF_TimeOffHintEntryBox *ent;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return GF_BAD_PARAM;

	//OK, create a new HintSampleDesc
	hdesc = (GF_HintSampleEntryBox *) gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, HintDescriptionIndex - 1);
	count = gf_list_count(hdesc->other_boxes);

	for (i=0; i< count; i++) {
		ent = (GF_TimeOffHintEntryBox *)gf_list_get(hdesc->other_boxes, i);
		if (ent->type == GF_ISOM_BOX_TYPE_TSRO) {
			ent->TimeOffset = TimeOffset;
			return GF_OK;
		}
	}
	//we have to create a new entry...
	ent = (GF_TimeOffHintEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_TSRO);
	ent->TimeOffset = TimeOffset;

	return gf_isom_box_add_default((GF_Box *)hdesc->other_boxes, (GF_Box *)ent);
}

//sets the RTP SequenceNumber Offset that the server will add to the packets
//if not set, the server adds a random offset
GF_EXPORT
GF_Err gf_isom_rtp_set_time_sequence_offset(GF_ISOFile *the_file, u32 trackNumber, u32 HintDescriptionIndex, u32 SequenceNumberOffset)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *hdesc;
	u32 i, count;
	GF_SeqOffHintEntryBox *ent;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return GF_BAD_PARAM;

	//OK, create a new HintSampleDesc
	hdesc = (GF_HintSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->other_boxes, HintDescriptionIndex - 1);
	count = gf_list_count(hdesc->other_boxes);

	for (i=0; i< count; i++) {
		ent = (GF_SeqOffHintEntryBox *)gf_list_get(hdesc->other_boxes, i);
		if (ent->type == GF_ISOM_BOX_TYPE_SNRO) {
			ent->SeqOffset = SequenceNumberOffset;
			return GF_OK;
		}
	}
	//we have to create a new entry...
	ent = (GF_SeqOffHintEntryBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SNRO);
	ent->SeqOffset = SequenceNumberOffset;
	return gf_isom_box_add_default((GF_Box *)hdesc->other_boxes, (GF_Box *)ent);
}

//Starts a new sample for the hint track. A sample is just a collection of packets
//the transmissionTime is indicated in the media timeScale of the hint track
GF_EXPORT
GF_Err gf_isom_begin_hint_sample(GF_ISOFile *the_file, u32 trackNumber, u32 HintDescriptionIndex, u32 TransmissionTime)
{
	GF_TrackBox *trak;
	u32 descIndex, dataRefIndex;
	GF_HintSample *samp;
	GF_HintSampleEntryBox *entry;
	GF_Err e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !IsHintTrack(trak)) return GF_BAD_PARAM;

	//assert we're increasing the timing...
	if (trak->Media->information->sampleTable->TimeToSample->w_LastDTS > TransmissionTime) return GF_BAD_PARAM;

	//store the descIndex for this sample
	descIndex = HintDescriptionIndex;
	if (!HintDescriptionIndex) {
		descIndex = trak->Media->information->sampleTable->currentEntryIndex;
	}
	e = Media_GetSampleDesc(trak->Media, descIndex, (GF_SampleEntryBox **) &entry, &dataRefIndex);
	if (e) return e;
	if (!entry || !dataRefIndex) return GF_BAD_PARAM;
	//set the current to this one if no packet is used
	if (entry->hint_sample) return GF_BAD_PARAM;
	trak->Media->information->sampleTable->currentEntryIndex = descIndex;

	//create a new sample based on the protocol type of the hint description entry
	samp = gf_isom_hint_sample_new(entry->type);
	if (!samp) return GF_NOT_SUPPORTED;

	//OK, let's store the time of this sample
	samp->TransmissionTime = TransmissionTime;
	//OK, set our sample in the entry...
	entry->hint_sample = samp;
	return GF_OK;
}

//stores the hint sample in the file
//set IsRandomAccessPoint if you want to indicate that this is a random access point
//in the stream
GF_EXPORT
GF_Err gf_isom_end_hint_sample(GF_ISOFile *the_file, u32 trackNumber, u8 IsRandomAccessPoint)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *entry;
	u32 dataRefIndex;
	GF_Err e;
	GF_BitStream *bs;
	GF_ISOSample *samp;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !IsHintTrack(trak)) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, trak->Media->information->sampleTable->currentEntryIndex, (GF_SampleEntryBox **) &entry, &dataRefIndex);
	if (e) return e;
	if (!entry->hint_sample) return GF_BAD_PARAM;

	//first of all, we need to adjust the offset for data referenced IN THIS hint sample
	//and get some PckSize
	e = AdjustHintInfo(entry, trak->Media->information->sampleTable->SampleSize->sampleCount + 1);
	if (e) return e;

	//ok, let's write the sample
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	e = gf_isom_hint_sample_write(entry->hint_sample, bs);
	if (e) {
		gf_bs_del(bs);
		return e;
	}
	samp = gf_isom_sample_new();
	samp->CTS_Offset = 0;
	samp->IsRAP = IsRandomAccessPoint;
	samp->DTS = entry->hint_sample->TransmissionTime;
	//get the sample
	gf_bs_get_content(bs, &samp->data, &samp->dataLength);
	gf_bs_del(bs);

	//finally add the sample
	e = gf_isom_add_sample(the_file, trackNumber, trak->Media->information->sampleTable->currentEntryIndex, samp);
	gf_isom_sample_del(&samp);

	//and delete the sample in our entry ...
	gf_isom_hint_sample_del(entry->hint_sample);
	entry->hint_sample = NULL;
	return e;
}


//adds a blank chunk of data in the sample that is skipped while streaming
GF_EXPORT
GF_Err gf_isom_hint_blank_data(GF_ISOFile *the_file, u32 trackNumber, u8 AtBegin)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *entry;
	u32 count;
	GF_HintPacket *pck;
	GF_EmptyDTE *dte;
	GF_Err e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !IsHintTrack(trak)) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, trak->Media->information->sampleTable->currentEntryIndex, (GF_SampleEntryBox **) &entry, &count);
	if (e) return e;
	if (!entry->hint_sample) return GF_BAD_PARAM;
	count = gf_list_count(entry->hint_sample->packetTable);
	if (!count) return GF_BAD_PARAM;
	pck = (GF_HintPacket *)gf_list_get(entry->hint_sample->packetTable, count - 1);

	dte = (GF_EmptyDTE *) NewDTE(0);
	return gf_isom_hint_pck_add_dte(pck, (GF_GenericDTE *)dte, AtBegin);
}


//adds a chunk of data (max 14 bytes) in the packet that is directly copied
//while streaming
GF_EXPORT
GF_Err gf_isom_hint_direct_data(GF_ISOFile *the_file, u32 trackNumber, char *data, u32 dataLength, u8 AtBegin)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *entry;
	u32 count;
	GF_HintPacket *pck;
	GF_ImmediateDTE *dte;
	GF_Err e;
	u32 offset = 0;

	if (!dataLength) return GF_OK;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !IsHintTrack(trak) || (dataLength > 14)) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, trak->Media->information->sampleTable->currentEntryIndex, (GF_SampleEntryBox **) &entry, &count);
	if (e) return e;
	if (!entry->hint_sample) return GF_BAD_PARAM;
	count = gf_list_count(entry->hint_sample->packetTable);
	if (!count) return GF_BAD_PARAM;
	pck = (GF_HintPacket *)gf_list_get(entry->hint_sample->packetTable, count - 1);

	dte = (GF_ImmediateDTE *) NewDTE(1);
	memcpy(dte->data, data + offset, dataLength);
	dte->dataLength = dataLength;
	return gf_isom_hint_pck_add_dte(pck, (GF_GenericDTE *)dte, AtBegin);
}

GF_EXPORT
GF_Err gf_isom_hint_sample_data(GF_ISOFile *the_file, u32 trackNumber, u32 SourceTrackID, u32 SampleNumber, u16 DataLength, u32 offsetInSample, char *extra_data, u8 AtBegin)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *entry;
	u32 count;
	u16 refIndex;
	GF_HintPacket *pck;
	GF_SampleDTE *dte;
	GF_Err e;
	GF_TrackReferenceTypeBox *hint;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !IsHintTrack(trak)) return GF_BAD_PARAM;


	e = Media_GetSampleDesc(trak->Media, trak->Media->information->sampleTable->currentEntryIndex, (GF_SampleEntryBox **) &entry, &count);
	if (e) return e;
	if (!entry->hint_sample) return GF_BAD_PARAM;
	count = gf_list_count(entry->hint_sample->packetTable);
	if (!count) return GF_BAD_PARAM;
	pck = (GF_HintPacket *)gf_list_get(entry->hint_sample->packetTable, count - 1);

	dte = (GF_SampleDTE *) NewDTE(2);

	dte->dataLength = DataLength;
	dte->sampleNumber = SampleNumber;
	dte->byteOffset = offsetInSample;

	//we're getting data from another track
	if (SourceTrackID != trak->Header->trackID) {
		//get (or set) the track reference index
		e = Track_FindRef(trak, GF_ISOM_REF_HINT, &hint);
		if (e) return e;
		e = reftype_AddRefTrack(hint, SourceTrackID, &refIndex);
		if (e) return e;
		//WARNING: IN QT, MUST BE 0-based !!!
		dte->trackRefIndex = (u8) (refIndex - 1);
	} else {
		//we're in the hint track
		dte->trackRefIndex = (s8) -1;
		//basic check...
		if (SampleNumber > trak->Media->information->sampleTable->SampleSize->sampleCount + 1) {
			DelDTE((GF_GenericDTE *)dte);
			return GF_BAD_PARAM;
		}

		//are we in the current sample ??
		if (!SampleNumber || (SampleNumber == trak->Media->information->sampleTable->SampleSize->sampleCount + 1)) {
			//we adding some stuff in the current sample ...
			dte->byteOffset += entry->hint_sample->dataLength;
			entry->hint_sample->AdditionalData = (char*)gf_realloc(entry->hint_sample->AdditionalData, sizeof(char) * (entry->hint_sample->dataLength + DataLength));
			if (AtBegin) {
				if (entry->hint_sample->dataLength)
					memmove(entry->hint_sample->AdditionalData + DataLength, entry->hint_sample->AdditionalData, entry->hint_sample->dataLength);
				memcpy(entry->hint_sample->AdditionalData, extra_data, DataLength);
				/*offset existing DTE*/
				gf_isom_hint_pck_offset(pck, DataLength, SampleNumber);
			} else {
				memcpy(entry->hint_sample->AdditionalData + entry->hint_sample->dataLength, extra_data, DataLength);
			}
			entry->hint_sample->dataLength += DataLength;
			//and set the sample number ...
			dte->sampleNumber = trak->Media->information->sampleTable->SampleSize->sampleCount + 1;
		}
	}
	//OK, add the entry
	return gf_isom_hint_pck_add_dte(pck, (GF_GenericDTE *)dte, AtBegin);
}

GF_EXPORT
GF_Err gf_isom_hint_sample_description_data(GF_ISOFile *the_file, u32 trackNumber, u32 SourceTrackID, u32 StreamDescriptionIndex, u16 DataLength, u32 offsetInDescription, u8 AtBegin)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *entry;
	u32 count;
	u16 refIndex;
	GF_HintPacket *pck;
	GF_StreamDescDTE *dte;
	GF_Err e;
	GF_TrackReferenceTypeBox *hint;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !IsHintTrack(trak)) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, trak->Media->information->sampleTable->currentEntryIndex, (GF_SampleEntryBox **) &entry, &count);
	if (e) return e;
	if (!entry->hint_sample) return GF_BAD_PARAM;
	count = gf_list_count(entry->hint_sample->packetTable);
	if (!count) return GF_BAD_PARAM;
	pck = (GF_HintPacket *)gf_list_get(entry->hint_sample->packetTable, count - 1);

	dte = (GF_StreamDescDTE *) NewDTE(3);
	dte->byteOffset = offsetInDescription;
	dte->dataLength = DataLength;
	dte->streamDescIndex = StreamDescriptionIndex;
	if (SourceTrackID == trak->Header->trackID) {
		dte->trackRefIndex = (s8) -1;
	} else {
		//get (or set) the track reference index
		e = Track_FindRef(trak, GF_ISOM_REF_HINT, &hint);
		if (e) return e;
		e = reftype_AddRefTrack(hint, SourceTrackID, &refIndex);
		if (e) return e;
		//WARNING: IN QT, MUST BE 0-based !!!
		dte->trackRefIndex = (u8) (refIndex - 1);
	}
	return gf_isom_hint_pck_add_dte(pck, (GF_GenericDTE *)dte, AtBegin);
}

GF_EXPORT
GF_Err gf_isom_rtp_packet_set_flags(GF_ISOFile *the_file, u32 trackNumber,
                                    u8 PackingBit,
                                    u8 eXtensionBit,
                                    u8 MarkerBit,
                                    u8 disposable_packet,
                                    u8 IsRepeatedPacket)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *entry;
	GF_RTPPacket *pck;
	u32 dataRefIndex, ind;
	GF_Err e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, trak->Media->information->sampleTable->currentEntryIndex, (GF_SampleEntryBox **) &entry, &dataRefIndex);
	if (e) return e;
	if (!entry->hint_sample) return GF_BAD_PARAM;

	ind = gf_list_count(entry->hint_sample->packetTable);
	if (!ind) return GF_BAD_PARAM;
	pck = (GF_RTPPacket *)gf_list_get(entry->hint_sample->packetTable, ind-1);

	pck->P_bit = PackingBit ? 1 : 0;
	pck->X_bit = eXtensionBit ? 1 : 0;
	pck->M_bit = MarkerBit ? 1 : 0;
	pck->B_bit = disposable_packet ? 1 : 0;
	pck->R_bit = IsRepeatedPacket ? 1 : 0;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_rtp_packet_begin(GF_ISOFile *the_file, u32 trackNumber,
                                s32 relativeTime,
                                u8 PackingBit,
                                u8 eXtensionBit,
                                u8 MarkerBit,
                                u8 PayloadType,
                                u8 B_frame,
                                u8 IsRepeatedPacket,
                                u16 SequenceNumber)
{
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *entry;
	GF_RTPPacket *pck;
	u32 dataRefIndex;
	GF_Err e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, trak->Media->information->sampleTable->currentEntryIndex, (GF_SampleEntryBox **) &entry, &dataRefIndex);
	if (e) return e;
	if (!entry->hint_sample) return GF_BAD_PARAM;

	pck = (GF_RTPPacket *) gf_isom_hint_pck_new(entry->type);

	pck->P_bit = PackingBit ? 1 : 0;
	pck->X_bit = eXtensionBit ? 1 : 0;
	pck->M_bit = MarkerBit ? 1 : 0;
	pck->payloadType = PayloadType;
	pck->SequenceNumber = SequenceNumber;
	pck->B_bit = B_frame ? 1 : 0;
	pck->R_bit = IsRepeatedPacket ? 1 : 0;
	pck->relativeTransTime = relativeTime;
	return gf_list_add(entry->hint_sample->packetTable, pck);
}


//set the time offset of this packet. This enables packets to be placed in the hint track
//in decoding order, but have their presentation time-stamp in the transmitted
//packet be in a different order. Typically used for MPEG video with B-frames
GF_EXPORT
GF_Err gf_isom_rtp_packet_set_offset(GF_ISOFile *the_file, u32 trackNumber, s32 timeOffset)
{
	GF_RTPOBox *rtpo;
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *entry;
	GF_RTPPacket *pck;
	u32 dataRefIndex, i;
	GF_Err e;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, trak->Media->information->sampleTable->currentEntryIndex, (GF_SampleEntryBox **) &entry, &dataRefIndex);
	if (e) return e;
	if (!entry->hint_sample) return GF_BAD_PARAM;

	pck = (GF_RTPPacket *)gf_list_get(entry->hint_sample->packetTable, gf_list_count(entry->hint_sample->packetTable) - 1);
	if (!pck) return GF_BAD_PARAM;

	//look in the TLV
	i=0;
	while ((rtpo = (GF_RTPOBox *)gf_list_enum(pck->TLV, &i))) {
		if (rtpo->type == GF_ISOM_BOX_TYPE_RTPO) {
			rtpo->timeOffset = timeOffset;
			return GF_OK;
		}
	}
	//not found, add it
	rtpo = (GF_RTPOBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_RTPO);
	rtpo->timeOffset = timeOffset;

	return gf_list_add(pck->TLV, rtpo);
}


static void AddSDPLine(GF_List *list, char *sdp_text, Bool is_movie_sdp)
{
	const char *sdp_order;
	u32 i, count = gf_list_count(list);
	char fc = sdp_text[0];

	sdp_order = (is_movie_sdp) ? "vosiuepcbzkatr" : "micbka";
	for (i=0; i<count; i++) {
		char *l = (char *)gf_list_get(list, i);
		char *s1 = (char *)strchr(sdp_order, l[0]);
		char *s2 = (char *)strchr(sdp_order, fc);
		if (s1 && s2 && (strlen(s2)>strlen(s1))) {
			gf_list_insert(list, sdp_text, i);
			return;
		}
	}
	gf_list_add(list, sdp_text);
}

static void ReorderSDP(char *sdp_text, Bool is_movie_sdp)
{
	char *cur, b;
	GF_List *lines = gf_list_new();
	cur = sdp_text;
	while (cur) {
		char *st = strstr(cur, "\r\n");
		assert(st);
		st += 2;
		if (!st[0]) {
			AddSDPLine(lines, gf_strdup(cur), is_movie_sdp);
			break;
		}
		b = st[0];
		st[0] = 0;
		AddSDPLine(lines, gf_strdup(cur), is_movie_sdp);
		st[0] = b;
		cur = st;
	}
	strcpy(sdp_text, "");
	while (gf_list_count(lines)) {
		char *cur = (char *)gf_list_get(lines, 0);
		gf_list_rem(lines, 0);
		strcat(sdp_text, cur);
		gf_free(cur);
	}
	gf_list_del(lines);
}

//add an SDP line to the SDP container at the track level (media-specific SDP info)
GF_EXPORT
GF_Err gf_isom_sdp_add_track_line(GF_ISOFile *the_file, u32 trackNumber, const char *text)
{
	GF_TrackBox *trak;
	GF_UserDataMap *map;
	GF_HintTrackInfoBox *hnti;
	GF_SDPBox *sdp;
	GF_Err e;
	char *buf;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	//currently, only RTP hinting supports SDP
	if (!CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return GF_BAD_PARAM;

	map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_HNTI, NULL);
	if (!map) return GF_ISOM_INVALID_FILE;

	//we should have only one HNTI in the UDTA
	if (gf_list_count(map->other_boxes) != 1) return GF_ISOM_INVALID_FILE;

	hnti = (GF_HintTrackInfoBox *)gf_list_get(map->other_boxes, 0);
	if (!hnti->SDP) {
		e = hnti_AddBox((GF_Box*)hnti, gf_isom_box_new(GF_ISOM_BOX_TYPE_SDP));
		if (e) return e;
	}
	sdp = (GF_SDPBox *) hnti->SDP;

	if (!sdp->sdpText) {
		sdp->sdpText = (char *)gf_malloc(sizeof(char) * (strlen(text) + 3));
		strcpy(sdp->sdpText, text);
		strcat(sdp->sdpText, "\r\n");
		return GF_OK;
	}
	buf = (char *)gf_malloc(sizeof(char) * (strlen(sdp->sdpText) + strlen(text) + 3));
	strcpy(buf, sdp->sdpText);
	strcat(buf, text);
	strcat(buf, "\r\n");
	gf_free(sdp->sdpText);
	ReorderSDP(buf, GF_FALSE);
	sdp->sdpText = buf;
	return GF_OK;
}

//remove all SDP info at the track level
GF_EXPORT
GF_Err gf_isom_sdp_clean_track(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	GF_UserDataMap *map;
	GF_HintTrackInfoBox *hnti;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	//currently, only RTP hinting supports SDP
	if (!CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return GF_BAD_PARAM;

	map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_HNTI, NULL);
	if (!map) return GF_ISOM_INVALID_FILE;

	//we should have only one HNTI in the UDTA
	if (gf_list_count(map->other_boxes) != 1) return GF_ISOM_INVALID_FILE;

	hnti = (GF_HintTrackInfoBox *)gf_list_get(map->other_boxes, 0);
	if (!hnti->SDP) return GF_OK;
	//and free the SDP
	gf_free(((GF_SDPBox *)hnti->SDP)->sdpText);
	((GF_SDPBox *)hnti->SDP)->sdpText = NULL;
	return GF_OK;
}


//add an SDP line to the SDP container at the movie level (presentation SDP info)
//NOTE: the \r\n end of line for SDP is automatically inserted
GF_EXPORT
GF_Err gf_isom_sdp_add_line(GF_ISOFile *movie, const char *text)
{
	GF_UserDataMap *map;
	GF_RTPBox *rtp;
	GF_Err e;
	GF_HintTrackInfoBox *hnti;
	char *buf;

	if (!movie->moov) return GF_BAD_PARAM;

	//check if we have a udta ...
	if (!movie->moov->udta) {
		e = moov_AddBox((GF_Box*)movie->moov, gf_isom_box_new(GF_ISOM_BOX_TYPE_UDTA));
		if (e) return e;
	}
	//find a hnti in the udta
	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_HNTI, NULL);
	if (!map) {
		e = udta_AddBox((GF_Box *)movie->moov->udta, gf_isom_box_new(GF_ISOM_BOX_TYPE_HNTI));
		if (e) return e;
		map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_HNTI, NULL);
	}

	//there should be one and only one hnti
	if (!gf_list_count(map->other_boxes) ) {
		e = udta_AddBox((GF_Box *)movie->moov->udta, gf_isom_box_new(GF_ISOM_BOX_TYPE_HNTI));
		if (e) return e;
	}
	else if (gf_list_count(map->other_boxes) < 1) return GF_ISOM_INVALID_FILE;

	hnti = (GF_HintTrackInfoBox *)gf_list_get(map->other_boxes, 0);

	if (!hnti->SDP) {
		GF_Box *a = gf_isom_box_new_ex(GF_ISOM_BOX_TYPE_RTP, GF_ISOM_BOX_TYPE_HNTI);
		hnti_AddBox((GF_Box*)hnti, a);
	}
	rtp = (GF_RTPBox *) hnti->SDP;

	if (!rtp->sdpText) {
		rtp->sdpText = (char*)gf_malloc(sizeof(char) * (strlen(text) + 3));
		strcpy(rtp->sdpText, text);
		strcat(rtp->sdpText, "\r\n");
		return GF_OK;
	}
	buf = (char*)gf_malloc(sizeof(char) * (strlen(rtp->sdpText) + strlen(text) + 3));
	strcpy(buf, rtp->sdpText);
	strcat(buf, text);
	strcat(buf, "\r\n");
	gf_free(rtp->sdpText);
	ReorderSDP(buf, GF_TRUE);
	rtp->sdpText = buf;
	return GF_OK;
}


//remove all SDP info at the movie level
GF_EXPORT
GF_Err gf_isom_sdp_clean(GF_ISOFile *movie)
{
	GF_UserDataMap *map;
	GF_HintTrackInfoBox *hnti;

	//check if we have a udta ...
	if (!movie->moov || !movie->moov->udta) return GF_OK;

	//find a hnti in the udta
	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_HNTI, NULL);
	if (!map) return GF_OK;

	//there should be one and only one hnti
	if (gf_list_count(map->other_boxes) != 1) return GF_ISOM_INVALID_FILE;
	hnti = (GF_HintTrackInfoBox *)gf_list_get(map->other_boxes, 0);

	//remove and destroy the entry
	gf_list_rem(map->other_boxes, 0);
	gf_isom_box_del((GF_Box *)hnti);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

GF_EXPORT
GF_Err gf_isom_sdp_get(GF_ISOFile *movie, const char **sdp, u32 *length)
{
	GF_UserDataMap *map;
	GF_HintTrackInfoBox *hnti;
	GF_RTPBox *rtp;
	*length = 0;
	*sdp = NULL;
	if (!movie || !movie->moov) return GF_BAD_PARAM;
	//check if we have a udta ...
	if (!movie->moov->udta) return GF_OK;

	//find a hnti in the udta
	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_HNTI, NULL);
	if (!map) return GF_OK;

	//there should be one and only one hnti
	if (gf_list_count(map->other_boxes) != 1) return GF_ISOM_INVALID_FILE;
	hnti = (GF_HintTrackInfoBox *)gf_list_get(map->other_boxes, 0);

	if (!hnti->SDP) return GF_OK;
	rtp = (GF_RTPBox *) hnti->SDP;

	*length = (u32) strlen(rtp->sdpText);
	*sdp = rtp->sdpText;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_sdp_track_get(GF_ISOFile *the_file, u32 trackNumber, const char **sdp, u32 *length)
{
	GF_TrackBox *trak;
	GF_UserDataMap *map;
	GF_HintTrackInfoBox *hnti;
	GF_SDPBox *sdpa;

	*sdp = NULL;
	*length = 0;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->udta) return GF_OK;

	map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_HNTI, NULL);
	if (!map) return GF_ISOM_INVALID_FILE;

	//we should have only one HNTI in the UDTA
	if (gf_list_count(map->other_boxes) != 1) return GF_ISOM_INVALID_FILE;

	hnti = (GF_HintTrackInfoBox *)gf_list_get(map->other_boxes, 0);
	if (!hnti->SDP) return GF_OK;
	sdpa = (GF_SDPBox *) hnti->SDP;

	*length = (u32) strlen(sdpa->sdpText);
	*sdp = sdpa->sdpText;
	return GF_OK;
}


GF_EXPORT
u32 gf_isom_get_payt_count(GF_ISOFile *the_file, u32 trackNumber)
{
	u32 i, count;
	GF_TrackBox *trak;
	GF_UserDataMap *map;
	GF_HintInfoBox *hinf;
	GF_PAYTBox *payt;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	if (!CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return 0;
	map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_HINF, NULL);
	if (!map) return 0;
	if (gf_list_count(map->other_boxes) != 1) return 0;

	hinf = (GF_HintInfoBox *)gf_list_get(map->other_boxes, 0);
	count = 0;
	i = 0;
	while ((payt = (GF_PAYTBox*)gf_list_enum(hinf->other_boxes, &i))) {
		if (payt->type == GF_ISOM_BOX_TYPE_PAYT) count++;
	}
	return count;
}

GF_EXPORT
const char *gf_isom_get_payt_info(GF_ISOFile *the_file, u32 trackNumber, u32 index, u32 *payID)
{
	u32 i, count;
	GF_TrackBox *trak;
	GF_UserDataMap *map;
	GF_HintInfoBox *hinf;
	GF_PAYTBox *payt;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !index) return NULL;

	if (!CheckHintFormat(trak, GF_ISOM_HINT_RTP)) return NULL;
	map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_HINF, NULL);
	if (!map) return NULL;
	if (gf_list_count(map->other_boxes) != 1) return NULL;

	hinf = (GF_HintInfoBox *)gf_list_get(map->other_boxes, 0);
	count = 0;
	i = 0;
	while ((payt = (GF_PAYTBox*)gf_list_enum(hinf->other_boxes, &i))) {
		if (payt->type == GF_ISOM_BOX_TYPE_PAYT) {
			count++;
			if (count == index) {
				if (payID) *payID=payt->payloadCode;
				return payt->payloadString;
			}
		}
	}
	return NULL;
}

#endif /*!defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_HINTING)*/

