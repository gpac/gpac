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

//adds a DTS in the table and get the sample number of this new sample
//we could return an error if a sample with the same DTS already exists
//but this is not true for QT or MJ2K, only for MP4...
//we assume the authoring tool tries to create a compliant MP4 file.
GF_Err stbl_AddDTS(GF_SampleTableBox *stbl, u64 DTS, u32 *sampleNumber, u32 LastAUDefDuration)
{
	u32 i, j, sampNum;
	u64 *DTSs, *newDTSs, curDTS;
	GF_SttsEntry *ent;

	GF_TimeToSampleBox *stts = stbl->TimeToSample;

	//We don't update the reading cache when adding a sample

	*sampleNumber = 0;
	//if we don't have an entry, that's the first one...
	if (! gf_list_count(stts->entryList)) {
		//assert the first DTS is 0. If not, that will break the whole file
		if (DTS) return GF_BAD_PARAM;
		ent = (GF_SttsEntry*)malloc(sizeof(GF_SttsEntry));
		if (!ent) return GF_OUT_OF_MEM;
		ent->sampleCount = 1;
		ent->sampleDelta = LastAUDefDuration;
		stts->w_currentEntry = ent;
		stts->w_currentSampleNum = (*sampleNumber) = 1;
		return gf_list_add(stts->entryList, ent);
	}

	//check the last DTS...
	if (DTS > stts->w_LastDTS) {
		//OK, we're adding at the end
		if (DTS == stts->w_LastDTS + stts->w_currentEntry->sampleDelta) {
			stts->w_currentEntry->sampleCount ++;
			stts->w_currentSampleNum ++;
			(*sampleNumber) = stts->w_currentSampleNum;
			stts->w_LastDTS = DTS;
			return GF_OK;
		}
		//we need to split the entry
		if (stts->w_currentEntry->sampleCount == 1) {
			//use this one and adjust...
			stts->w_currentEntry->sampleDelta = (u32) (DTS - stts->w_LastDTS);
			stts->w_currentEntry->sampleCount ++;
			stts->w_currentSampleNum ++;
			stts->w_LastDTS = DTS;
			(*sampleNumber) = stts->w_currentSampleNum;
			return GF_OK;
		}
		//we definitely need to split the entry ;)
		stts->w_currentEntry->sampleCount --;
		ent = (GF_SttsEntry*)malloc(sizeof(GF_SttsEntry));
		if (!ent) return GF_OUT_OF_MEM;
		ent->sampleCount = 2;
		ent->sampleDelta = (u32) (DTS - stts->w_LastDTS);
		stts->w_LastDTS = DTS;
		stts->w_currentSampleNum ++;
		(*sampleNumber) = stts->w_currentSampleNum;
		stts->w_currentEntry = ent;
		return gf_list_add(stts->entryList, ent);
	}


	//unpack the DTSs...
	DTSs = (u64*)malloc(sizeof(u64) * stbl->SampleSize->sampleCount);
	if (!DTSs) return GF_OUT_OF_MEM;
	curDTS = 0;
	sampNum = 0;
	ent = NULL;
	i=0;
	while ((ent = (GF_SttsEntry *)gf_list_enum(stts->entryList, &i))) {
		for (j = 0; j<ent->sampleCount; j++) {
			DTSs[sampNum] = curDTS;
			curDTS += ent->sampleDelta;
			sampNum ++;
		}
	}
	//delete the table..
	while (gf_list_count(stts->entryList)) {
		ent = (GF_SttsEntry*)gf_list_get(stts->entryList, 0);
		free(ent);
		gf_list_rem(stts->entryList, 0);
	}

	//create the new DTSs
	newDTSs = (u64*)malloc(sizeof(u64) * (stbl->SampleSize->sampleCount + 1));
	if (!newDTSs) return GF_OUT_OF_MEM;
	i = 0;
	while (i < stbl->SampleSize->sampleCount) {
		if (DTSs[i] > DTS) break;
		newDTSs[i] = DTSs[i];
		i++;
	}
	//if we add a sample with the same DTS as an existing one, it's added after.
	newDTSs[i] = DTS;
	*sampleNumber = i+1;
	for (; i<stbl->SampleSize->sampleCount; i++) {
		newDTSs[i+1] = DTSs[i];
	}
	free(DTSs);

	//rewrite the table
	ent = (GF_SttsEntry*)malloc(sizeof(GF_SttsEntry));
	if (!ent) return GF_OUT_OF_MEM;
	ent->sampleCount = 0;
	ent->sampleDelta = (u32) newDTSs[1];
	i = 0;
	while (1) {
		if (i == stbl->SampleSize->sampleCount) {
			//and by default, our last sample has the same delta as the prev
			ent->sampleCount++;
			gf_list_add(stts->entryList, ent);
			break;
		}
		if (newDTSs[i+1] - newDTSs[i] == ent->sampleDelta) {
			ent->sampleCount += 1;
		} else {
			gf_list_add(stts->entryList, ent);
			ent = (GF_SttsEntry*)malloc(sizeof(GF_SttsEntry));
			if (!ent) return GF_OUT_OF_MEM;
			ent->sampleCount = 1;
			ent->sampleDelta = (u32) (newDTSs[i+1] - newDTSs[i]);
		}
		i++;
	}
	free(newDTSs);
	//reset the cache to the end
	stts->w_currentEntry = ent;
	stts->w_currentSampleNum = stbl->SampleSize->sampleCount + 1;
	return GF_OK;
}

GF_Err AddCompositionOffset(GF_CompositionOffsetBox *ctts, u32 offset)
{
	GF_DttsEntry *entry;
	if (!ctts) return GF_BAD_PARAM;

	entry = ctts->w_currentEntry;
	if ( (entry == NULL) || (entry->decodingOffset != offset) ) {
		entry = (GF_DttsEntry *) malloc(sizeof(GF_DttsEntry));
		if (!entry) return GF_OUT_OF_MEM;
		entry->sampleCount = 1;
		entry->decodingOffset = offset;
		gf_list_add(ctts->entryList, entry);
		ctts->w_currentEntry = entry;
	} else {
		entry->sampleCount++;
	}
	ctts->w_LastSampleNumber++;
	return GF_OK;
}

//adds a CTS offset for a new sample
GF_Err stbl_AddCTS(GF_SampleTableBox *stbl, u32 sampleNumber, u32 CTSoffset)
{
	GF_DttsEntry *ent;
	u32 i, j, count, sampNum, *CTSs, *newCTSs;

	GF_CompositionOffsetBox *ctts = stbl->CompositionOffset;

	/*in unpack mode we're sure to have 1 ctts entry per sample*/
	if (ctts->unpack_mode) {
		ent = (GF_DttsEntry *) malloc(sizeof(GF_DttsEntry));
		if (!ent) return GF_OUT_OF_MEM;
		ent->sampleCount = 1;
		ent->decodingOffset = CTSoffset;
		return gf_list_add(ctts->entryList, ent);
	}
	/*move to last entry*/
	if (!ctts->w_currentEntry) {
		ctts->w_LastSampleNumber = 0;
		count = gf_list_count(ctts->entryList);
		for (i=0; i<count; i++) {
			ctts->w_currentEntry = (GF_DttsEntry *)gf_list_get(ctts->entryList, i);
			ctts->w_LastSampleNumber += ctts->w_currentEntry->sampleCount;
		}
	}

	//check if we're working in order...
	if (ctts->w_LastSampleNumber < sampleNumber) {
		//add some 0 till we get to the sample
		while (ctts->w_LastSampleNumber + 1 != sampleNumber) {
			AddCompositionOffset(ctts, 0);
		}
		return AddCompositionOffset(ctts, CTSoffset);
	}

	//NOPE we are inserting a sample...
	CTSs = (u32*)malloc(sizeof(u32) * stbl->SampleSize->sampleCount);
	if (!CTSs) return GF_OUT_OF_MEM;
	sampNum = 0;
	i=0;
	while ((ent = (GF_DttsEntry *)gf_list_enum(ctts->entryList, &i))) {
		for (j = 0; j<ent->sampleCount; j++) {
			CTSs[sampNum] = ent->decodingOffset;
			sampNum ++;
		}
	}
	
	//delete the entries
	while (gf_list_count(ctts->entryList)) {
		ent = (GF_DttsEntry*)gf_list_get(ctts->entryList, 0);
		free(ent);
		gf_list_rem(ctts->entryList, 0);
	}
	
	//create the new CTS
	newCTSs = (u32*)malloc(sizeof(u32) * (stbl->SampleSize->sampleCount + 1));
	if (!newCTSs) return GF_OUT_OF_MEM;
	j = 0;
	for (i = 0; i < stbl->SampleSize->sampleCount; i++) {
		if (i+1 == sampleNumber) {
			newCTSs[i] = CTSoffset;
			j = 1;
		}
		newCTSs[i+j] = CTSs[i];
	}
	free(CTSs);

	//rewrite the table
	ent = (GF_DttsEntry*)malloc(sizeof(GF_DttsEntry));
	if (!ent) return GF_OUT_OF_MEM;
	ent->sampleCount = 1;
	ent->decodingOffset = newCTSs[0];
	i = 1;
	while (1) {
		if (i == stbl->SampleSize->sampleCount) {
			gf_list_add(ctts->entryList, ent);
			break;
		}
		if (newCTSs[i] == ent->decodingOffset) {
			ent->sampleCount += 1;
		} else {
			gf_list_add(ctts->entryList, ent);
			ent = (GF_DttsEntry*)malloc(sizeof(GF_DttsEntry));
			if (!ent) return GF_OUT_OF_MEM;
			ent->sampleCount = 1;
			ent->decodingOffset = newCTSs[i];
		}
		i++;
	}
	free(newCTSs);
	//reset the cache to the end
	ctts->w_currentEntry = ent;
	//we've inserted a sample, therefore the last sample (n) has now number n+1
	//we cannot use SampleCount because we have probably skipped some samples
	//(we're calling AddCTS only if the sample gas a CTSOffset !!!)
	ctts->w_LastSampleNumber += 1;
	return GF_OK;
}

GF_Err stbl_repackCTS(GF_CompositionOffsetBox *ctts)
{
	GF_DttsEntry *entry, *next;
	GF_List *newTable;
	u32 i, count;
	if (!ctts->unpack_mode) return GF_OK;
	ctts->unpack_mode = 0;

	count = gf_list_count(ctts->entryList);
	if (!count) return GF_OK;
	newTable = gf_list_new();
	entry = (GF_DttsEntry *)gf_list_get(ctts->entryList, 0);
	ctts->w_LastSampleNumber = entry->sampleCount;

	gf_list_add(newTable, entry);
	for (i=1; i<count; i++) {
		next = (GF_DttsEntry *)gf_list_get(ctts->entryList, i);
		ctts->w_LastSampleNumber += next->sampleCount;
		if (entry->decodingOffset != next->decodingOffset) {
			entry = next;
			gf_list_add(newTable, entry);
			ctts->w_currentEntry = entry;
		} else {
			entry->sampleCount += next->sampleCount;
			free(next);
		}
	} 
	gf_list_del(ctts->entryList);
	ctts->entryList = newTable;
	return GF_OK;
}

GF_Err stbl_unpackCTS(GF_SampleTableBox *stbl)
{
	GF_DttsEntry *entry, *next;
	u32 i, j, remain;
	GF_CompositionOffsetBox *ctts;
	GF_List *newTable;
	ctts = stbl->CompositionOffset;
	if (ctts->unpack_mode) return GF_OK;
	ctts->unpack_mode = 1;
	newTable = gf_list_new();

	i=0;
	while ((entry = (GF_DttsEntry *)gf_list_enum(ctts->entryList, &i))) {
		gf_list_add(newTable, entry);
		for (j=1; j<entry->sampleCount; j++) {
			next = (GF_DttsEntry *)malloc(sizeof(GF_DttsEntry));
			if (!next) return GF_OUT_OF_MEM;
			next->decodingOffset = entry->decodingOffset;
			next->sampleCount = 1;
			gf_list_add(newTable, next);
		}
		entry->sampleCount = 1;
	}
	gf_list_del(ctts->entryList);
	ctts->entryList = newTable;
	remain = stbl->SampleSize->sampleCount - gf_list_count(ctts->entryList);
	while (remain) {
		entry = (GF_DttsEntry *)malloc(sizeof(GF_DttsEntry));
		if (!entry) return GF_OUT_OF_MEM;
		entry->decodingOffset = 0;
		entry->sampleCount = 1;
		gf_list_add(ctts->entryList, entry);
		remain--;
	}
	return GF_OK;
}

//add size
GF_Err stbl_AddSize(GF_SampleSizeBox *stsz, u32 sampleNumber, u32 size)
{
	u32 i, k;
	u32 *newSizes;
	if (!stsz || !size || !sampleNumber) return GF_BAD_PARAM;

	if (sampleNumber > stsz->sampleCount + 1) return GF_BAD_PARAM;

	//all samples have the same size
	if (stsz->sizes == NULL) {
		//1 first sample added in NON COMPACT MODE
		if (! stsz->sampleCount && (stsz->type != GF_ISOM_BOX_TYPE_STZ2) ) {
			stsz->sampleCount = 1;
			stsz->sampleSize = size;
			return GF_OK;
		}
		//2- sample has the same size
		if (stsz->sampleSize == size) {
			stsz->sampleCount++;
			return GF_OK;
		}
		//3- no, need to alloc a size table
		stsz->sizes = (u32*)malloc(sizeof(u32) * (stsz->sampleCount + 1));
		if (!stsz->sizes) return GF_OUT_OF_MEM;
		stsz->alloc_size = stsz->sampleCount + 1;

		k = 0;
		for (i = 0 ; i < stsz->sampleCount; i++) {
			if (i + 1 == sampleNumber) {
				stsz->sizes[i + k] = size;
				k = 1;
			}
			stsz->sizes[i+k] = stsz->sampleSize;
		}
		//this if we append a new sample
		if (stsz->sampleCount + 1 == sampleNumber) {
			stsz->sizes[stsz->sampleCount] = size;
		}
		stsz->sampleSize = 0;
		stsz->sampleCount++;
		return GF_OK;
	}


	/*append*/
	if (stsz->sampleCount + 1 == sampleNumber) {
		if (!stsz->alloc_size) stsz->alloc_size = stsz->sampleCount;
		if (stsz->sampleCount == stsz->alloc_size) {
			stsz->alloc_size += 50;
			stsz->sizes = realloc(stsz->sizes, sizeof(u32)*(stsz->alloc_size) );
			if (!stsz->sizes) return GF_OUT_OF_MEM;
		}
		stsz->sizes[stsz->sampleCount] = size;
	} else {
		newSizes = (u32*)malloc(sizeof(u32)*(1 + stsz->sampleCount) );
		if (!newSizes) return GF_OUT_OF_MEM;
		k = 0;
		for (i = 0; i < stsz->sampleCount; i++) {
			if (i + 1 == sampleNumber) {
				newSizes[i + k] = size;
				k = 1;
			}
			newSizes[i + k] = stsz->sizes[i];
		}
		free(stsz->sizes);
		stsz->sizes = newSizes;
		stsz->alloc_size = 1 + stsz->sampleCount;
	}
	stsz->sampleCount++;
	return GF_OK;
}


GF_Err stbl_AddRAP(GF_SyncSampleBox *stss, u32 sampleNumber)
{
	u32 i, k;
	u32 *newNumbers;

	if (!stss || !sampleNumber) return GF_BAD_PARAM;

	if (stss->sampleNumbers == NULL) {
		stss->sampleNumbers = (u32*)malloc(sizeof(u32));
		if (!stss->sampleNumbers) return GF_OUT_OF_MEM;
		stss->sampleNumbers[0] = sampleNumber;
		stss->entryCount = 1;
		return GF_OK;
	}


	if (stss->sampleNumbers[stss->entryCount-1] < sampleNumber) {
		stss->sampleNumbers = realloc(stss->sampleNumbers, sizeof(u32) * (stss->entryCount + 1));
		if (!stss->sampleNumbers) return GF_OUT_OF_MEM;
		stss->sampleNumbers[stss->entryCount] = sampleNumber;
	} else {
		newNumbers = (u32*)malloc(sizeof(u32) * (stss->entryCount + 1));
		if (!newNumbers) return GF_OUT_OF_MEM;
		//the table is in increasing order of sampleNumber
		k = 0;
		for (i = 0; i < stss->entryCount; i++) {
			if (stss->sampleNumbers[i] >= sampleNumber) {
				newNumbers[i + k] = sampleNumber;
				k = 1;
			}
			newNumbers[i + k] = stss->sampleNumbers[i] + k;
		}
		free(stss->sampleNumbers);
		stss->sampleNumbers = newNumbers;
	}
	//update our list
	stss->entryCount ++;
	return GF_OK;
}

GF_Err stbl_AddRedundant(GF_SampleTableBox *stbl, u32 sampleNumber)
{
	GF_SampleDependencyTypeBox *sdtp;

	if (stbl->SampleDep == NULL) {
		stbl->SampleDep = (GF_SampleDependencyTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SDTP);
		if (!stbl->SampleDep) return GF_OUT_OF_MEM;
	}
	sdtp = stbl->SampleDep;
	if (sdtp->sampleCount + 1 < sampleNumber) {
		u32 missed = sampleNumber-1 - sdtp->sampleCount;
		sdtp->sample_info = (u8*) realloc(sdtp->sample_info, sizeof(u8) * (sdtp->sampleCount+missed) );
		while (missed) {
			u8 isRAP;
			if (stbl->SyncSample) stbl_GetSampleRAP(stbl->SyncSample, sdtp->sampleCount+1, &isRAP, NULL, NULL);
			else isRAP = 1;
			sdtp->sample_info[sdtp->sampleCount] = isRAP ? 0x20 : 0;
			sdtp->sampleCount++;
			missed--;
		}
	}

	sdtp->sample_info = (u8*) realloc(sdtp->sample_info, sizeof(u8) * (sdtp->sampleCount + 1));
	if (!sdtp->sample_info) return GF_OUT_OF_MEM;
	if (sdtp->sampleCount < sampleNumber) {
		sdtp->sample_info[sdtp->sampleCount] = 0x29;
	} else {
		u32 snum = sampleNumber-1;
		memmove(sdtp->sample_info+snum+1, sdtp->sample_info+snum, sizeof(u8) * (sdtp->sampleCount - snum) );
		sdtp->sample_info[snum] = 0x29;
	}
	//update our list
	sdtp->sampleCount ++;
	return GF_OK;
}

//this function is always called in INCREASING order of shadow sample numbers
GF_Err stbl_AddShadow(GF_ShadowSyncBox *stsh, u32 sampleNumber, u32 shadowNumber)
{
	GF_StshEntry *ent;
	u32 i, count;
	count = gf_list_count(stsh->entries);
	for (i=0; i<count; i++) {
		ent = (GF_StshEntry*)gf_list_get(stsh->entries, i);
		if (ent->shadowedSampleNumber == shadowNumber) {
			ent->syncSampleNumber = sampleNumber;
			return GF_OK;
		} else if (ent->shadowedSampleNumber > shadowNumber) break;
	}
	ent = (GF_StshEntry*)malloc(sizeof(GF_StshEntry));
	if (!ent) return GF_OUT_OF_MEM;
	ent->shadowedSampleNumber = shadowNumber;
	ent->syncSampleNumber = sampleNumber;
	if (i == gf_list_count(stsh->entries)) {
		return gf_list_add(stsh->entries, ent);
	} else {
		return gf_list_insert(stsh->entries, ent, i ? i-1 : 0);
	}
}

//used in edit/write, where sampleNumber == chunkNumber
GF_Err stbl_AddChunkOffset(GF_MediaBox *mdia, u32 sampleNumber, u32 StreamDescIndex, u64 offset)
{
	GF_SampleTableBox *stbl;
	GF_ChunkOffsetBox *stco;
	GF_ChunkLargeOffsetBox *co64;
	GF_StscEntry *ent, *tmp;
	u32 count, i, k, *newOff;
	u64 *newLarge;

	stbl = mdia->information->sampleTable;

	count = gf_list_count(mdia->information->sampleTable->SampleToChunk->entryList);

	if (count + 1 < sampleNumber ) return GF_BAD_PARAM;

	ent = (GF_StscEntry*)malloc(sizeof(GF_StscEntry));
	if (!ent) return GF_OUT_OF_MEM;
	ent->isEdited = 0;
	if (Media_IsSelfContained(mdia, StreamDescIndex)) ent->isEdited = 1;
	ent->sampleDescriptionIndex = StreamDescIndex;
	ent->samplesPerChunk = 1;

	//we know 1 chunk == 1 sample, so easy...
	ent->firstChunk = sampleNumber;
	ent->nextChunk = sampleNumber + 1;

	//add the offset to the chunk...
	//and we change our offset
	if (stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
		stco = (GF_ChunkOffsetBox *)stbl->ChunkOffset;
		//if the new offset is a large one, we have to rewrite our table entry by entry (32->64 bit conv)...
		if (offset > 0xFFFFFFFF) {
			co64 = (GF_ChunkLargeOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CO64);
			co64->entryCount = stco->entryCount + 1;
			co64->offsets = (u64*)malloc(sizeof(u64) * co64->entryCount);
			if (!co64->offsets) return GF_OUT_OF_MEM;
			k = 0;
			for (i=0;i<stco->entryCount; i++) {
				if (i + 1 == sampleNumber) {
					co64->offsets[i] = offset;
					k = 1;
				}
				co64->offsets[i+k] = (u64) stco->offsets[i];
			}
			if (!k) co64->offsets[co64->entryCount - 1] = offset;
			gf_isom_box_del(stbl->ChunkOffset);
			stbl->ChunkOffset = (GF_Box *) co64;
		} else {
			//no, we can use this one.
			if (sampleNumber > stco->entryCount) {
				if (!stco->alloc_size) stco->alloc_size = stco->entryCount;
				if (stco->entryCount == stco->alloc_size) {
					stco->alloc_size += 50;
					stco->offsets = (u32*)realloc(stco->offsets, sizeof(u32) * stco->alloc_size);
					if (!stco->offsets) return GF_OUT_OF_MEM;
				}
				stco->offsets[stco->entryCount] = (u32) offset;
				stco->entryCount += 1;
			} else {
				//nope. we're inserting
				newOff = (u32*)malloc(sizeof(u32) * (stco->entryCount + 1));
				if (!newOff) return GF_OUT_OF_MEM;
				k=0;
				for (i=0; i<stco->entryCount; i++) {
					if (i+1 == sampleNumber) {
						newOff[i] = (u32) offset;
						k=1;
					}
					newOff[i+k] = stco->offsets[i];
				}
				free(stco->offsets);
				stco->offsets = newOff;
				stco->entryCount ++;
				stco->alloc_size = stco->entryCount;
			}
		}
	} else {
		//use large offset...
		co64 = (GF_ChunkLargeOffsetBox *)stbl->ChunkOffset;
		if (sampleNumber > co64->entryCount) {
			if (!co64->alloc_size) co64->alloc_size = co64->entryCount;
			if (co64->entryCount == co64->alloc_size) {
				co64->alloc_size += 50;
				co64->offsets = (u64*)realloc(co64->offsets, sizeof(u64) * co64->alloc_size);
				if (!co64->offsets) return GF_OUT_OF_MEM;
			}
			co64->offsets[co64->entryCount] = offset;
			co64->entryCount += 1;
		} else {
			//nope. we're inserting
			newLarge = (u64*)malloc(sizeof(u64) * (co64->entryCount + 1));
			if (!newLarge) return GF_OUT_OF_MEM;
			k=0;
			for (i=0; i<co64->entryCount; i++) {
				if (i+1 == sampleNumber) {
					newLarge[i] = offset;
					k=1;
				}
				newLarge[i+k] = co64->offsets[i];
			}
			free(co64->offsets);
			co64->offsets = newLarge;
			co64->entryCount++;
			co64->alloc_size++;
		}
	}

	//OK, now if we've inserted a chunk, update the sample to chunk info...
	if (sampleNumber == count + 1) {
		ent->nextChunk = count + 1;
		if (stbl->SampleToChunk->currentEntry)
			stbl->SampleToChunk->currentEntry->nextChunk = ent->firstChunk;
		stbl->SampleToChunk->currentEntry = ent;
		stbl->SampleToChunk->currentIndex = count;
		stbl->SampleToChunk->firstSampleInCurrentChunk = sampleNumber;
		//write - edit mode: sample number = chunk number
		stbl->SampleToChunk->currentChunk = sampleNumber;
		stbl->SampleToChunk->ghostNumber = 1;
		return gf_list_add(stbl->SampleToChunk->entryList, ent);
	}
	for (i = sampleNumber - 1; i<count; i++) {
		tmp = (GF_StscEntry*)gf_list_get(stbl->SampleToChunk->entryList, i);
		if (tmp) tmp->firstChunk +=1;
	}
	return gf_list_insert(stbl->SampleToChunk->entryList, ent, sampleNumber-1);
}




GF_Err stbl_SetChunkOffset(GF_MediaBox *mdia, u32 sampleNumber, u64 offset)
{
	GF_StscEntry *ent;
	u32 i;
	GF_ChunkLargeOffsetBox *co64;
	GF_SampleTableBox *stbl = mdia->information->sampleTable;

	if (!sampleNumber || !stbl) return GF_BAD_PARAM;

	ent = (GF_StscEntry*)gf_list_get(stbl->SampleToChunk->entryList, sampleNumber - 1);

	//we edit our entry if self contained
	if (Media_IsSelfContained(mdia, ent->sampleDescriptionIndex))
		ent->isEdited = 1;

	//and we change our offset
	if (stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
		//if the new offset is a large one, we have to rewrite our table...
		if (offset > 0xFFFFFFFF) {
			co64 = (GF_ChunkLargeOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CO64);
			co64->entryCount = ((GF_ChunkOffsetBox *)stbl->ChunkOffset)->entryCount;
			co64->offsets = (u64*)malloc(sizeof(u64)*co64->entryCount);
			if (!co64->offsets) return GF_OUT_OF_MEM;
			for (i=0;i<co64->entryCount; i++) {
				co64->offsets[i] = (u64) ((GF_ChunkOffsetBox *)stbl->ChunkOffset)->offsets[i];
			}
			co64->offsets[ent->firstChunk - 1] = offset;
			gf_isom_box_del(stbl->ChunkOffset);
			stbl->ChunkOffset = (GF_Box *) co64;
			return GF_OK;
		}
		((GF_ChunkOffsetBox *)stbl->ChunkOffset)->offsets[ent->firstChunk - 1] = (u32) offset;
	} else {
		((GF_ChunkLargeOffsetBox *)stbl->ChunkOffset)->offsets[ent->firstChunk - 1] = offset;
	}
	return GF_OK;
}


GF_Err stbl_SetSampleCTS(GF_SampleTableBox *stbl, u32 sampleNumber, u32 offset)
{
	u32 i, j, sampNum, *CTSs;
	GF_DttsEntry *ent;

	GF_CompositionOffsetBox *ctts = stbl->CompositionOffset;

	
	//if we're setting the CTS of a sample we've skipped...
	if (ctts->w_LastSampleNumber < sampleNumber) {
		//add some 0 till we get to the sample
		while (ctts->w_LastSampleNumber + 1 != sampleNumber) {
			AddCompositionOffset(ctts, 0);
		}
		return AddCompositionOffset(ctts, offset);
	}
	if (ctts->unpack_mode) {
		GF_DttsEntry *dtts = (GF_DttsEntry *)gf_list_get(ctts->entryList, sampleNumber-1);
		if (!dtts) return GF_BAD_PARAM;
		dtts->decodingOffset = offset;
		return GF_OK;
	}

	//NOPE we are inserting a sample...
	CTSs = (u32*)malloc(sizeof(u32) * ctts->w_LastSampleNumber);
	if (!CTSs) return GF_OUT_OF_MEM;
	sampNum = 0;
	i=0;
	while ((ent = (GF_DttsEntry *)gf_list_enum(ctts->entryList, &i))) {
		for (j = 0; j<ent->sampleCount; j++) {
			if (sampNum + 1 == sampleNumber) {
				CTSs[sampNum] = offset;
			} else {
				CTSs[sampNum] = ent->decodingOffset;
			}
			sampNum ++;
		}
	}
	//delete the entries
	while (gf_list_count(ctts->entryList)) {
		ent = (GF_DttsEntry*)gf_list_get(ctts->entryList, 0);
		free(ent);
		gf_list_rem(ctts->entryList, 0);
	}

	//rewrite the table
	ent = (GF_DttsEntry*)malloc(sizeof(GF_DttsEntry));
	if (!ent) return GF_OUT_OF_MEM;
	ent->sampleCount = 1;
	ent->decodingOffset = CTSs[0];
	i = 1;
	//reset the read cache (entry insertion)
	ctts->r_currentEntryIndex = 1;
	ctts->r_FirstSampleInEntry = 1;
	while (1) {
		if (i == ctts->w_LastSampleNumber) {
			gf_list_add(ctts->entryList, ent);
			break;
		}
		if (CTSs[i] == ent->decodingOffset) {
			ent->sampleCount += 1;
		} else {
			gf_list_add(ctts->entryList, ent);
			ent = (GF_DttsEntry*)malloc(sizeof(GF_DttsEntry));
			if (!ent) return GF_OUT_OF_MEM;
			ent->sampleCount = 1;
			ent->decodingOffset = CTSs[i];
			ctts->r_FirstSampleInEntry = i;
		}
		if (i==sampleNumber) ctts->r_currentEntryIndex = gf_list_count(ctts->entryList) + 1;
		i++;
	}
	free(CTSs);
	return GF_OK;
}

GF_Err stbl_SetSampleSize(GF_SampleSizeBox *stsz, u32 SampleNumber, u32 size)
{
	u32 i;
	if (!SampleNumber || (stsz->sampleCount < SampleNumber)) return GF_BAD_PARAM;

	if (stsz->sampleSize) {
		if (stsz->sampleSize == size) return GF_OK;
		if (stsz->sampleCount == 1) {
			stsz->sampleSize = size;
			return GF_OK;
		}
		//nope, we have to rewrite a table
		stsz->sizes = (u32*)malloc(sizeof(u32)*stsz->sampleCount);
		if (!stsz->sizes) return GF_OUT_OF_MEM;
		for (i=0; i<stsz->sampleCount; i++) stsz->sizes[i] = stsz->sampleSize;
		stsz->sampleSize = 0;
	}
	stsz->sizes[SampleNumber - 1] = size;
	return GF_OK;
}


GF_Err stbl_SetSampleRAP(GF_SyncSampleBox *stss, u32 SampleNumber, u8 isRAP)
{
	u32 i, j, k, *newNum, nextSamp;

	nextSamp = 0;
	//check if we have already a sync sample
	for (i = 0; i < stss->entryCount; i++) {
		if (stss->sampleNumbers[i] == SampleNumber) {
			if (isRAP) return GF_OK;
			//remove it...
			newNum = (u32*)malloc(sizeof(u32) * (stss->entryCount-1));
			if (!newNum) return GF_OUT_OF_MEM;
			k = 0;
			for (j=0; j<stss->entryCount; j++) {
				if (stss->sampleNumbers[j] == SampleNumber) {
					k=1;
				} else {
					newNum[j-k] = stss->sampleNumbers[j];
				}
			}
			stss->entryCount -=1;
			free(stss->sampleNumbers);
			stss->sampleNumbers = newNum;
			return GF_OK;
		}
		if (stss->sampleNumbers[i] > SampleNumber) break;
	}
	//we need to insert a RAP somewhere if RAP ...
	if (!isRAP) return GF_OK;
	newNum = (u32*)malloc(sizeof(u32) * (stss->entryCount + 1));
	if (!newNum) return GF_OUT_OF_MEM;
	k = 0;
	for (j = 0 ; j<stss->entryCount; j++) {
		if (j == i) {
			newNum[j] = SampleNumber;
			k = 1;
		}
		newNum[j+k] = stss->sampleNumbers[j];
	}
	if (!k) {
		newNum[stss->entryCount] = SampleNumber;
	}
	free(stss->sampleNumbers);
	stss->sampleNumbers = newNum;
	stss->entryCount ++;
	return GF_OK;
}

GF_Err stbl_SetRedundant(GF_SampleTableBox *stbl, u32 sampleNumber)
{
	if (stbl->SampleDep->sampleCount < sampleNumber) {
		return stbl_AddRedundant(stbl, sampleNumber);
	} else {
		stbl->SampleDep->sample_info[sampleNumber-1] = 0x29;
		return GF_OK;
	}
}

GF_Err stbl_SetSyncShadow(GF_ShadowSyncBox *stsh, u32 sampleNumber, u32 syncSample)
{
	u32 i, count;
	GF_StshEntry *ent;

	count = gf_list_count(stsh->entries);
	for (i=0; i<count; i++) {
		ent = (GF_StshEntry*)gf_list_get(stsh->entries, i);
		if (ent->shadowedSampleNumber == sampleNumber) {
			ent->syncSampleNumber = syncSample;
			return GF_OK;
		}
		if (ent->shadowedSampleNumber > sampleNumber) break;
	}
	//we need a new one...
	ent = (GF_StshEntry*)malloc(sizeof(GF_StshEntry));
	if (!ent) return GF_OUT_OF_MEM;
	ent->shadowedSampleNumber = sampleNumber;
	ent->syncSampleNumber = syncSample;
	//insert or append ?
	if (i == gf_list_count(stsh->entries)) {
		//don't update the cache ...
		return gf_list_add(stsh->entries, ent);
	} else {
		//update the cache
		stsh->r_LastEntryIndex = i;
		stsh->r_LastFoundSample = sampleNumber;
		return gf_list_insert(stsh->entries, ent, i);
	}
}


//always called before removing the sample from SampleSize
GF_Err stbl_RemoveDTS(GF_SampleTableBox *stbl, u32 sampleNumber, u32 LastAUDefDuration)
{
	u64 *DTSs, curDTS;
	u32 i, j, k, sampNum;
	GF_SttsEntry *ent;
	GF_TimeToSampleBox *stts;

	stts = stbl->TimeToSample;

	//gasp, we're removing the only sample: empty the sample table 
	if (stbl->SampleSize->sampleCount == 1) {
		if (gf_list_count(stts->entryList)) {
			gf_list_rem(stts->entryList, 0);
		}
		//update the reading cache
		stts->r_FirstSampleInEntry = stts->r_currentEntryIndex = 0;
		stts->r_CurrentDTS = 0;
		return GF_OK;
	}
	//unpack the DTSs...
	DTSs = (u64*)malloc(sizeof(u64) * (stbl->SampleSize->sampleCount - 1));
	if (!DTSs) return GF_OUT_OF_MEM;
	curDTS = 0;
	sampNum = 0;
	ent = NULL;
	k=0;
	i=0;
	while ((ent = (GF_SttsEntry *)gf_list_enum(stts->entryList, &i))) {
		for (j = 0; j<ent->sampleCount; j++) {
			if (sampNum == sampleNumber - 1) {
				k=1;
			} else {
				DTSs[sampNum-k] = curDTS;
			}
			curDTS += ent->sampleDelta;
			sampNum ++;
		}
	}
	//delete the table..
	while (gf_list_count(stts->entryList)) {
		ent = (GF_SttsEntry*)gf_list_get(stts->entryList, 0);
		free(ent);
		gf_list_rem(stts->entryList, 0);
	}

	//rewrite the table
	ent = (GF_SttsEntry*)malloc(sizeof(GF_SttsEntry));
	if (!ent) return GF_OUT_OF_MEM;
	ent->sampleCount = 0;
	gf_list_add(stts->entryList, ent);
	if (stbl->SampleSize->sampleCount == 2) {
		ent->sampleDelta = LastAUDefDuration;
	} else {
		ent->sampleDelta = (u32) DTSs[1];
		DTSs[0] = 0;
	}
	i = 0;
	while (1) {
		if (i+2 == stbl->SampleSize->sampleCount) {
			//and by default, our last sample has the same delta as the prev
			ent->sampleCount++;
			break;
		}
		if (DTSs[i+1] - DTSs[i] == ent->sampleDelta) {
			ent->sampleCount += 1;
		} else {
			ent = (GF_SttsEntry*)malloc(sizeof(GF_SttsEntry));
			if (!ent) return GF_OUT_OF_MEM;
			ent->sampleCount = 1;
			ent->sampleDelta = (u32) (DTSs[i+1] - DTSs[i]);
			gf_list_add(stts->entryList, ent);
		}
		i++;
	}
	stts->w_LastDTS = DTSs[stbl->SampleSize->sampleCount - 2];
	free(DTSs);
	//reset write the cache to the end
	stts->w_currentEntry = ent;
	stts->w_currentSampleNum = stbl->SampleSize->sampleCount - 1;
	//reset read the cache to the begining
	stts->r_FirstSampleInEntry = stts->r_currentEntryIndex = 0;
	stts->r_CurrentDTS = 0;
	return GF_OK;
}


//always called before removing the sample from SampleSize
GF_Err stbl_RemoveCTS(GF_SampleTableBox *stbl, u32 sampleNumber)
{
	u32 *CTSs;
	u32 sampNum, i, j, k, count;
	GF_DttsEntry *ent;
	GF_CompositionOffsetBox *ctts;

	ctts = stbl->CompositionOffset;

	//last one...
	if (stbl->SampleSize->sampleCount == 1) {
		gf_isom_box_del((GF_Box *) ctts);
		stbl->CompositionOffset = NULL;
		return GF_OK;
	}

	//the number of entries is NOT ALWAYS the number of samples !
	//instead, use the cache
	//first case, we're removing a sample that was not added yet
	if (sampleNumber > ctts->w_LastSampleNumber) return GF_OK;
	//No, the sample was here...
	//this is the only one we have.
	if (ctts->w_LastSampleNumber == 1) {
		gf_isom_box_del((GF_Box *) ctts);
		stbl->CompositionOffset = NULL;
		return GF_OK;
	}
	CTSs = (u32*)malloc(sizeof(u32) * (ctts->w_LastSampleNumber - 1));
	if (!CTSs) return GF_OUT_OF_MEM;
	sampNum = 0;
	k = 0;
	count = gf_list_count(ctts->entryList);
	for (i=0; i<count; i++) {
		ent = (GF_DttsEntry*)gf_list_get(ctts->entryList, i);
		for (j = 0; j<ent->sampleCount; j++) {
			if (sampNum + 1 == sampleNumber) {
				k = 1;
			} else {
				CTSs[sampNum-k] = ent->decodingOffset;
			}
			sampNum ++;
		}
	}
	
	//delete the entries
	while (gf_list_count(ctts->entryList)) {
		ent = (GF_DttsEntry*)gf_list_get(ctts->entryList, 0);
		free(ent);
		gf_list_rem(ctts->entryList, 0);
	}
	

	//rewrite the table
	ent = (GF_DttsEntry*)malloc(sizeof(GF_DttsEntry));
	if (!ent) return GF_OUT_OF_MEM;
	ent->sampleCount = 1;
	ent->decodingOffset = CTSs[0];
	i = 1;
	while (1) {
		if (i+1 == ctts->w_LastSampleNumber) {
			gf_list_add(ctts->entryList, ent);
			break;
		}
		if (CTSs[i] == ent->decodingOffset) {
			ent->sampleCount += 1;
		} else {
			gf_list_add(ctts->entryList, ent);
			ent = (GF_DttsEntry*)malloc(sizeof(GF_DttsEntry));
			if (!ent) return GF_OUT_OF_MEM;
			ent->sampleCount = 1;
			ent->decodingOffset = CTSs[i];
		}
		i++;
	}
	free(CTSs);
	//reset the cache to the end
	ctts->w_currentEntry = ent;
	//we've removed a sample, therefore the last sample (n) has now number n-1
	//we cannot use SampleCount because we have probably skipped some samples
	//(we're calling AddCTS only if the sample gas a CTSOffset !!!)
	ctts->w_LastSampleNumber -= 1;
	return GF_OK;
}

GF_Err stbl_RemoveSize(GF_SampleSizeBox *stsz, u32 sampleNumber)
{
	u32 *newSizes;
	u32 i, k, prevSize;

	//last sample
	if (stsz->sampleCount == 1) {
		if (stsz->sizes) free(stsz->sizes);
		stsz->sizes = NULL;
		stsz->sampleCount = 0;
		return GF_OK;
	}
	//one single size
	if (stsz->sampleSize) {
		stsz->sampleCount -= 1;
		return GF_OK;
	}
	if (sampleNumber == stsz->sampleCount) {
		stsz->sizes = (u32*) realloc(stsz->sizes, sizeof(u32) * (stsz->sampleCount - 1));
		stsz->sampleCount--;
		return GF_OK;
	}
	//reallocate and check size by the way...
	newSizes = (u32*)malloc(sizeof(u32) * (stsz->sampleCount - 1));
	if (!newSizes) return GF_OUT_OF_MEM;
	if (sampleNumber == 1) {
		prevSize = stsz->sizes[1];
	} else {
		prevSize = stsz->sizes[0];
	}
	k=0;
	for (i=0; i<stsz->sampleCount; i++) {
		if (i+1 == sampleNumber) {
			k=1;
		} else {
			newSizes[i-k] = stsz->sizes[i];
		}
	}
	//only in non-compact mode
	free(stsz->sizes);
	stsz->sizes = newSizes;
	stsz->sampleSize = 0;
	stsz->sampleCount -= 1;
	return GF_OK;
}

//always called after removing the sample from SampleSize
GF_Err stbl_RemoveChunk(GF_SampleTableBox *stbl, u32 sampleNumber)
{
	u32 i, k, count;
	u32 *offsets;
	u64 *Loffsets;
	GF_StscEntry *ent;

	//remove the entry in SampleToChunk (1 <-> 1 in edit mode)
	ent = (GF_StscEntry*)gf_list_get(stbl->SampleToChunk->entryList, sampleNumber - 1);
	gf_list_rem(stbl->SampleToChunk->entryList, sampleNumber - 1);
	free(ent);

	//update the firstchunk info
	count=gf_list_count(stbl->SampleToChunk->entryList);
	for (i = sampleNumber - 1; i < count; i++) {
		ent = (GF_StscEntry*)gf_list_get(stbl->SampleToChunk->entryList, i);
		ent->firstChunk -= 1;
		ent->nextChunk -= 1;
	}
	//update the cache
	stbl->SampleToChunk->firstSampleInCurrentChunk = 1;
	stbl->SampleToChunk->currentEntry = (GF_StscEntry*)gf_list_get(stbl->SampleToChunk->entryList, 0);
	stbl->SampleToChunk->currentIndex = 0;
	stbl->SampleToChunk->currentChunk = 1;
	stbl->SampleToChunk->ghostNumber = 1;

	//realloc the chunk offset
	if (stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
		if (!stbl->SampleSize->sampleCount) {
			free(((GF_ChunkOffsetBox *)stbl->ChunkOffset)->offsets);
			((GF_ChunkOffsetBox *)stbl->ChunkOffset)->offsets = NULL;
			((GF_ChunkOffsetBox *)stbl->ChunkOffset)->entryCount = 0;
			return GF_OK;
		}
		offsets = (u32*)malloc(sizeof(u32) * (stbl->SampleSize->sampleCount));
		if (!offsets) return GF_OUT_OF_MEM;
		k=0;
		for (i=0; i<stbl->SampleSize->sampleCount+1; i++) {
			if (i+1 == sampleNumber) {
				k=1;
			} else {
				offsets[i-k] = ((GF_ChunkOffsetBox *)stbl->ChunkOffset)->offsets[i];
			}
		}
		free(((GF_ChunkOffsetBox *)stbl->ChunkOffset)->offsets);
		((GF_ChunkOffsetBox *)stbl->ChunkOffset)->offsets = offsets;
		((GF_ChunkOffsetBox *)stbl->ChunkOffset)->entryCount -= 1;
	} else {
		if (!stbl->SampleSize->sampleCount) {
			free(((GF_ChunkLargeOffsetBox *)stbl->ChunkOffset)->offsets);
			((GF_ChunkLargeOffsetBox *)stbl->ChunkOffset)->offsets = NULL;
			((GF_ChunkLargeOffsetBox *)stbl->ChunkOffset)->entryCount = 0;
			return GF_OK;
		}

		Loffsets = (u64*)malloc(sizeof(u64) * (stbl->SampleSize->sampleCount));
		if (!Loffsets) return GF_OUT_OF_MEM;
		k=0;
		for (i=0; i<stbl->SampleSize->sampleCount+1; i++) {
			if (i+1 == sampleNumber) {
				k=1;
			} else {
				Loffsets[i-k] = ((GF_ChunkLargeOffsetBox *)stbl->ChunkOffset)->offsets[i];
			}
		}
		free(((GF_ChunkLargeOffsetBox *)stbl->ChunkOffset)->offsets);
		((GF_ChunkLargeOffsetBox *)stbl->ChunkOffset)->offsets = Loffsets;
		((GF_ChunkLargeOffsetBox *)stbl->ChunkOffset)->entryCount -= 1;
	}
	return GF_OK;
}


GF_Err stbl_RemoveRAP(GF_SampleTableBox *stbl, u32 sampleNumber)
{
	u32 i;

	GF_SyncSampleBox *stss = stbl->SyncSample;
	//we remove the only one around...
	if (stss->entryCount == 1) {
		if (stss->sampleNumbers[0] != sampleNumber) return GF_OK;
		//free our numbers but don't delete (all samples are NON-sync
		free(stss->sampleNumbers);
		stss->sampleNumbers = NULL;
		stss->r_LastSampleIndex = stss->r_LastSyncSample = 0;
		stss->entryCount = 0;
		return GF_OK;
	}
	//the real pain is that we may actually not have to change anything..
	for (i=0; i<stss->entryCount; i++) {
		if (sampleNumber == stss->sampleNumbers[i]) goto found;
	}
	//nothing to do
	return GF_OK;

found:
	//a small opt: the sample numbers are in order...
	i++;
	for (;i<stss->entryCount; i++) {
		stss->sampleNumbers[i-1] = stss->sampleNumbers[i];
	}
	//and just realloc
	stss->sampleNumbers = (u32*)realloc(stss->sampleNumbers, sizeof(u32) * (stss->entryCount-1));
	stss->entryCount -= 1;
	return GF_OK;
}

GF_Err stbl_RemoveRedundant(GF_SampleTableBox *stbl, u32 SampleNumber)
{
	u32 i;

	if (!stbl->SampleDep) return GF_OK;
	if (stbl->SampleDep->sampleCount < SampleNumber) return GF_BAD_PARAM;

	i = stbl->SampleDep->sampleCount - SampleNumber;
	if (i) memmove(&stbl->SampleDep->sample_info[SampleNumber-1], & stbl->SampleDep->sample_info[SampleNumber], sizeof(u8)*i);
	stbl->SampleDep->sample_info = (u8*)realloc(stbl->SampleDep->sample_info, sizeof(u8) * (stbl->SampleDep->sampleCount-1));
	stbl->SampleDep->sampleCount-=1;
	return GF_OK;
}

GF_Err stbl_RemoveShadow(GF_ShadowSyncBox *stsh, u32 sampleNumber)
{
	u32 i;
	GF_StshEntry *ent;

	//we loop for the whole chain cause the spec doesn't say if we can have several
	//shadows for 1 sample...
	i=0;
	while ((ent = (GF_StshEntry *)gf_list_enum(stsh->entries, &i))) {
		if (ent->shadowedSampleNumber == sampleNumber) {
			i--;
			gf_list_rem(stsh->entries, i);
		}
	}
	//reset the cache
	stsh->r_LastEntryIndex = 0;
	stsh->r_LastFoundSample = 0;
	return GF_OK;
}


GF_Err stbl_SetPaddingBits(GF_SampleTableBox *stbl, u32 SampleNumber, u8 bits)
{
	u8 *p;
	//make sure the sample is a good one
	if (SampleNumber > stbl->SampleSize->sampleCount) return GF_BAD_PARAM;

	//create the table
	if (!stbl->PaddingBits) stbl->PaddingBits = (GF_PaddingBitsBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_FADB);

	//alloc
	if (!stbl->PaddingBits->padbits || !stbl->PaddingBits->SampleCount) {
		stbl->PaddingBits->SampleCount = stbl->SampleSize->sampleCount;
		stbl->PaddingBits->padbits = (u8*)malloc(sizeof(u8)*stbl->PaddingBits->SampleCount);
		if (!stbl->PaddingBits->padbits) return GF_OUT_OF_MEM;
		memset(stbl->PaddingBits->padbits, 0, sizeof(u8)*stbl->PaddingBits->SampleCount);
	}
	//realloc (this is needed in case n out of k samples get padding added)
	if (stbl->PaddingBits->SampleCount < stbl->SampleSize->sampleCount) {
		p = (u8*)malloc(sizeof(u8) * stbl->SampleSize->sampleCount);
		if (!p) return GF_OUT_OF_MEM;
		//set everything to 0
		memset(p, 0, stbl->SampleSize->sampleCount);
		//copy our previous table
		memcpy(p, stbl->PaddingBits->padbits, stbl->PaddingBits->SampleCount);
		free(stbl->PaddingBits->padbits);
		stbl->PaddingBits->padbits = p;
		stbl->PaddingBits->SampleCount = stbl->SampleSize->sampleCount;
	}
	stbl->PaddingBits->padbits[SampleNumber-1] = bits;
	return GF_OK;
}

GF_Err stbl_RemovePaddingBits(GF_SampleTableBox *stbl, u32 SampleNumber)
{
	u8 *p;
	u32 i, k;

	if (!stbl->PaddingBits) return GF_OK;
	if (stbl->PaddingBits->SampleCount < SampleNumber) return GF_BAD_PARAM;

	//last sample - remove the table
	if (stbl->PaddingBits->SampleCount == 1) {
		gf_isom_box_del((GF_Box *) stbl->PaddingBits);
		stbl->PaddingBits = NULL;
		return GF_OK;
	}

	//reallocate and check size by the way...
	p = (u8 *)malloc(sizeof(u8) * (stbl->PaddingBits->SampleCount - 1));
	if (!p) return GF_OUT_OF_MEM;

	k=0;
	for (i=0; i<stbl->PaddingBits->SampleCount; i++) {
		if (i+1 != SampleNumber) {
			p[k] = stbl->PaddingBits->padbits[i];
			k++;
		}
	}
	
	stbl->PaddingBits->SampleCount -= 1;
	free(stbl->PaddingBits->padbits);
	stbl->PaddingBits->padbits = p;
	return GF_OK;
}


GF_Err stbl_AddSampleFragment(GF_SampleTableBox *stbl, u32 sampleNumber, u16 size)
{
	GF_Err e;
	u32 i, count;
	GF_StsfEntry *ent;
	GF_SampleFragmentBox *stsf;		
	stsf = stbl->Fragments;

	if (!stsf) {
		//create table if any
		stsf = (GF_SampleFragmentBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSF);
		if (!stsf) return GF_OUT_OF_MEM;
		e = stbl_AddBox(stbl, (GF_Box *) stsf);
		if (e) return e;
	}

	//check cache
	if (!stsf->w_currentEntry || (stsf->w_currentEntry->SampleNumber < sampleNumber)) {
		stsf->w_currentEntry = NULL;
		stsf->w_currentEntryIndex = 0;
	}
	i = stsf->w_currentEntryIndex;

	count = gf_list_count(stsf->entryList);
	for (; i<count; i++) {
		ent = (GF_StsfEntry *)gf_list_get(stsf->entryList, i);
		if (ent->SampleNumber > sampleNumber) {
			ent = (GF_StsfEntry *)malloc(sizeof(GF_StsfEntry));
			if (!ent) return GF_OUT_OF_MEM;
			memset(ent, 0, sizeof(GF_StsfEntry));
			ent->SampleNumber = sampleNumber;
			gf_list_insert(stsf->entryList, ent, i);
			stsf->w_currentEntry = ent;
			stsf->w_currentEntryIndex = i;
			goto ent_found;
		}
		else if (ent->SampleNumber == sampleNumber) {
			stsf->w_currentEntry = ent;
			stsf->w_currentEntryIndex = i;
			goto ent_found;
		}
	}
	//if we get here add a new entry
	GF_SAFEALLOC(ent, GF_StsfEntry);
	ent->SampleNumber = sampleNumber;
	gf_list_add(stsf->entryList, ent);
	stsf->w_currentEntry = ent;
	stsf->w_currentEntryIndex = gf_list_count(stsf->entryList)-1;

ent_found:
	if (!ent->fragmentCount) {
		ent->fragmentCount = 1;
		ent->fragmentSizes = (u16*)malloc(sizeof(u16));
		if (!ent->fragmentSizes) return GF_OUT_OF_MEM;
		ent->fragmentSizes[0] = size;
		return GF_OK;
	}
	ent->fragmentSizes = (u16*)realloc(ent->fragmentSizes, sizeof(u16) * (ent->fragmentCount+1));
	if (!ent->fragmentSizes) return GF_OUT_OF_MEM;
	ent->fragmentSizes[ent->fragmentCount] = size;
	ent->fragmentCount += 1;

	return GF_OK;
}

GF_Err stbl_RemoveSampleFragments(GF_SampleTableBox *stbl, u32 sampleNumber)
{
	u32 i;
	GF_StsfEntry *ent;
	GF_SampleFragmentBox *stsf = stbl->Fragments;

	i=0;
	while ((ent = (GF_StsfEntry *)gf_list_enum(stsf->entryList, &i))) {
		if (ent->SampleNumber == sampleNumber) {
			gf_list_rem(stsf->entryList, i-1);
			if (ent->fragmentSizes) free(ent->fragmentSizes);
			free(ent);
			goto exit;
		}
	}
exit:
	//empty table, remove it
	if (!gf_list_count(stsf->entryList)) {
		stbl->Fragments = NULL;
		gf_isom_box_del((GF_Box *)stsf);
	}
	return GF_OK;
}

GF_Err stbl_SampleSizeAppend(GF_SampleSizeBox *stsz, u32 data_size)
{
	u32 i;
	if (!stsz || !stsz->sampleCount) return GF_BAD_PARAM;
	
	//we must realloc our table
	if (stsz->sampleSize) {
		stsz->sizes = (u32*)malloc(sizeof(u32)*stsz->sampleCount);
		if (!stsz->sizes) return GF_OUT_OF_MEM;
		for (i=0; i<stsz->sampleCount; i++) stsz->sizes[i] = stsz->sampleSize;
		stsz->sampleSize = 0;
	}
	stsz->sizes[stsz->sampleCount-1] += data_size;
	return GF_OK;
}

#endif	//GPAC_READ_ONLY



void stbl_AppendTime(GF_SampleTableBox *stbl, u32 duration)
{
	GF_SttsEntry *ent;
	u32 count;
	count = gf_list_count(stbl->TimeToSample->entryList);
	if (count) {
		ent = (GF_SttsEntry *)gf_list_get(stbl->TimeToSample->entryList, count-1);
		if (ent->sampleDelta == duration) {
			ent->sampleCount += 1;
			return;
		}
	}
	//nope need a new entry
	ent = (GF_SttsEntry *)malloc(sizeof(GF_SttsEntry));
	if (!ent) return;
	ent->sampleCount = 1;
	ent->sampleDelta = duration;
	gf_list_add(stbl->TimeToSample->entryList, ent);
}

void stbl_AppendSize(GF_SampleTableBox *stbl, u32 size)
{
	u32 i;

	if (!stbl->SampleSize->sampleCount) {
		stbl->SampleSize->sampleSize = size;
		stbl->SampleSize->sampleCount = 1;
		return;
	}
	if (stbl->SampleSize->sampleSize && (stbl->SampleSize->sampleSize==size)) {
		stbl->SampleSize->sampleCount += 1;
		return;
	}
	//realloc
	if (stbl->SampleSize->sizes) {
		stbl->SampleSize->sizes = (u32 *)realloc(stbl->SampleSize->sizes, sizeof(u32)*(stbl->SampleSize->sampleCount+1));
		if (!stbl->SampleSize->sizes) return;
	} else {
		stbl->SampleSize->sizes = (u32 *)malloc(sizeof(u32)*(stbl->SampleSize->sampleCount+1));
		for (i=0; i<stbl->SampleSize->sampleCount;i++) 
			stbl->SampleSize->sizes[i] = stbl->SampleSize->sampleSize;
	}
	stbl->SampleSize->sampleSize = 0;
	stbl->SampleSize->sizes[stbl->SampleSize->sampleCount] = size;
	stbl->SampleSize->sampleCount += 1;
}

void stbl_AppendChunk(GF_SampleTableBox *stbl, u64 offset)
{
	GF_ChunkOffsetBox *stco;
	GF_ChunkLargeOffsetBox *co64;
	u32 *new_offsets, i;
	u64 *off_64;

	//we may have to convert the table...
	if (stbl->ChunkOffset->type==GF_ISOM_BOX_TYPE_STCO) {
		stco = (GF_ChunkOffsetBox *)stbl->ChunkOffset;

		if (offset>0xFFFFFFFF) {
			co64 = (GF_ChunkLargeOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CO64);
			co64->entryCount = stco->entryCount + 1;
			co64->offsets = (u64*)malloc(sizeof(u64) * co64->entryCount);
			if (!co64->offsets) return;
			for (i=0; i<stco->entryCount; i++) co64->offsets[i] = stco->offsets[i];
			co64->offsets[i] = offset;
			gf_isom_box_del(stbl->ChunkOffset);
			stbl->ChunkOffset = (GF_Box *) co64;
			return;
		}
		//we're fine
		new_offsets = (u32*)malloc(sizeof(u32)*(stco->entryCount+1));
		if (!new_offsets) return;
		for (i=0; i<stco->entryCount; i++) new_offsets[i] = stco->offsets[i];
		new_offsets[i] = (u32) offset;
		if (stco->offsets) free(stco->offsets);
		stco->offsets = new_offsets;
		stco->entryCount += 1;
	}
	//large offsets
	else {
		co64 = (GF_ChunkLargeOffsetBox *)stbl->ChunkOffset;
		off_64 = (u64*)malloc(sizeof(u32)*(co64->entryCount+1));
		if (!off_64) return;
		for (i=0; i<co64->entryCount; i++) off_64[i] = co64->offsets[i];
		off_64[i] = offset;
		if (co64->offsets) free(co64->offsets);
		co64->offsets = off_64;
		co64->entryCount += 1;
	}
}

void stbl_AppendSampleToChunk(GF_SampleTableBox *stbl, u32 DescIndex, u32 samplesInChunk)
{
	u32 count, nextChunk;
	GF_StscEntry *ent;

	count = gf_list_count(stbl->SampleToChunk->entryList);
	nextChunk = ((GF_ChunkOffsetBox *) stbl->ChunkOffset)->entryCount;

	if (count) {
		ent = (GF_StscEntry *)gf_list_get(stbl->SampleToChunk->entryList, count-1);
		//good we can use this one
		if ( (ent->sampleDescriptionIndex == DescIndex) && (ent->samplesPerChunk==samplesInChunk)) 
			return;

		//set the next chunk btw ...
		ent->nextChunk = nextChunk;
	}
	//ok we need a new entry - this assumes this function is called AFTER AppendChunk
	GF_SAFEALLOC(ent, GF_StscEntry);
	ent->firstChunk = nextChunk;
	ent->sampleDescriptionIndex = DescIndex;
	ent->samplesPerChunk = samplesInChunk;
	gf_list_add(stbl->SampleToChunk->entryList, ent);
}

//called AFTER AddSize
void stbl_AppendRAP(GF_SampleTableBox *stbl, u8 isRap)
{
	u32 *new_raps, i;

	//no sync table
	if (!stbl->SyncSample) {
		//all samples RAP - no table
		if (isRap) return;

		//nope, create one
		stbl->SyncSample = (GF_SyncSampleBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSS);
		if (stbl->SampleSize->sampleCount > 1) {
			stbl->SyncSample->sampleNumbers = (u32*)malloc(sizeof(u32) * (stbl->SampleSize->sampleCount-1));
			if (!stbl->SyncSample->sampleNumbers) return;
			for (i=0; i<stbl->SampleSize->sampleCount-1; i++) 
				stbl->SyncSample->sampleNumbers[i] = i+1;

		}
		stbl->SyncSample->entryCount = stbl->SampleSize->sampleCount-1;
		return;
	}
	if (!isRap) return;

	new_raps = (u32*)malloc(sizeof(u32) * (stbl->SyncSample->entryCount + 1));
	if (!new_raps) return;
	for (i=0; i<stbl->SyncSample->entryCount; i++) new_raps[i] = stbl->SyncSample->sampleNumbers[i];
	new_raps[i] = stbl->SampleSize->sampleCount;
	if (stbl->SyncSample->sampleNumbers) free(stbl->SyncSample->sampleNumbers);
	stbl->SyncSample->sampleNumbers = new_raps;
	stbl->SyncSample->entryCount += 1;
}

void stbl_AppendPadding(GF_SampleTableBox *stbl, u8 padding)
{
	u32 i;
	u8 *pad_bits;
	if (!stbl->PaddingBits) stbl->PaddingBits = (GF_PaddingBitsBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_FADB);

	pad_bits = (u8*)malloc(sizeof(u8) * stbl->SampleSize->sampleCount);
	if (!pad_bits) return;
	memset(pad_bits, 0, sizeof(pad_bits));
//	for (i=0; i<stbl->SampleSize->sampleCount; i++) pad_bits[i] = 0;
	for (i=0; i<stbl->PaddingBits->SampleCount; i++) pad_bits[i] = stbl->PaddingBits->padbits[i];
	pad_bits[stbl->SampleSize->sampleCount-1] = padding;
	if (stbl->PaddingBits->padbits) free(stbl->PaddingBits->padbits);
	stbl->PaddingBits->padbits = pad_bits;
	stbl->PaddingBits->SampleCount = stbl->SampleSize->sampleCount;
}

void stbl_AppendCTSOffset(GF_SampleTableBox *stbl, u32 CTSOffset)
{
	u32 count;
	GF_DttsEntry *ent;

	if (!stbl->CompositionOffset) stbl->CompositionOffset = (GF_CompositionOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CTTS);

	count = gf_list_count(stbl->CompositionOffset->entryList);
	if (count) {
		ent = (GF_DttsEntry *)gf_list_get(stbl->CompositionOffset->entryList, count-1);
		if (ent->decodingOffset == CTSOffset) {
			ent->sampleCount ++;
			return;
		}
	}
	ent = (GF_DttsEntry *)malloc(sizeof(GF_DttsEntry));
	if (!ent) return;
	ent->sampleCount = 1;
	ent->decodingOffset = CTSOffset;
	gf_list_add(stbl->CompositionOffset->entryList, ent);
}

void stbl_AppendDegradation(GF_SampleTableBox *stbl, u16 DegradationPriority)
{
	if (!stbl->DegradationPriority) stbl->DegradationPriority = (GF_DegradationPriorityBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STDP);

	stbl->DegradationPriority->priorities = (u16 *)realloc(stbl->DegradationPriority->priorities, sizeof(u16) * stbl->SampleSize->sampleCount);
	stbl->DegradationPriority->priorities[stbl->SampleSize->sampleCount-1] = DegradationPriority;
	stbl->DegradationPriority->entryCount = stbl->SampleSize->sampleCount;
}

void stbl_AppendDepType(GF_SampleTableBox *stbl, u32 DepType)
{
	if (!stbl->SampleDep) stbl->SampleDep= (GF_SampleDependencyTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SDTP);
	stbl->SampleDep->sample_info = (u8*)realloc(stbl->SampleDep->sample_info, sizeof(u8)*stbl->SampleSize->sampleCount );
	stbl->SampleDep->sample_info[stbl->SampleDep->sampleCount] = DepType;
	stbl->SampleDep->sampleCount = stbl->SampleSize->sampleCount;

}



//This functions unpack the offset for easy editing, eg each sample
//is contained in one chunk...
GF_Err stbl_UnpackOffsets(GF_SampleTableBox *stbl)
{
	GF_Err e;
	u8 isEdited;
	u32 i, chunkNumber, sampleDescIndex;
	u64 dataOffset;
	GF_StscEntry *ent;
	GF_ChunkOffsetBox *stco_tmp;
	GF_ChunkLargeOffsetBox *co64_tmp;
	GF_SampleToChunkBox *stsc_tmp;

	if (!stbl) return GF_ISOM_INVALID_FILE;

	//we should have none of the mandatory boxes (allowed in the spec)
	if (!stbl->ChunkOffset && !stbl->SampleDescription && !stbl->SampleSize && !stbl->SampleToChunk && !stbl->TimeToSample)
		return GF_OK;
	/*empty track (just created)*/
	if (!stbl->SampleToChunk && !stbl->TimeToSample) return GF_OK;

	//or all the mandatory ones ...
	if (!stbl->ChunkOffset || !stbl->SampleDescription || !stbl->SampleSize || !stbl->SampleToChunk || !stbl->TimeToSample)
		return GF_ISOM_INVALID_FILE;

	//do we need to unpack? Not if we have only one sample per chunk.
	if (stbl->SampleSize->sampleCount == gf_list_count(stbl->SampleToChunk->entryList)) return GF_OK;

	//check the offset type and create a new table...
	if (stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
		co64_tmp = NULL;
		stco_tmp = (GF_ChunkOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STCO);
		if (!stco_tmp) return GF_OUT_OF_MEM;
		stco_tmp->entryCount = stbl->SampleSize->sampleCount;
		stco_tmp->offsets = (u32*)malloc(stco_tmp->entryCount * sizeof(u32));
		if (!stco_tmp->offsets) {
			gf_isom_box_del((GF_Box*)stco_tmp);
			return GF_OUT_OF_MEM;
		}
	} else if (stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_CO64) {
		stco_tmp = NULL;
		co64_tmp = (GF_ChunkLargeOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CO64);
		if (!co64_tmp) return GF_OUT_OF_MEM;
		co64_tmp->entryCount = stbl->SampleSize->sampleCount;
		co64_tmp->offsets = (u64*)malloc(co64_tmp->entryCount * sizeof(u64));
		if (!co64_tmp->offsets) {
			gf_isom_box_del((GF_Box*)co64_tmp);
			return GF_OUT_OF_MEM;
		}
	} else {
		return GF_ISOM_INVALID_FILE;
	}

	//create a new SampleToChunk table
	stsc_tmp = (GF_SampleToChunkBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSC);

	ent = NULL;
	//OK write our two tables...
	for (i = 0; i < stbl->SampleSize->sampleCount; i++) {
		//get the data info for the sample
		e = stbl_GetSampleInfos(stbl, i+1, &dataOffset, &chunkNumber, &sampleDescIndex, &isEdited);
		if (e) goto err_exit;
		ent = (GF_StscEntry*)malloc(sizeof(GF_StscEntry));
		if (!ent) return GF_OUT_OF_MEM;
		ent->isEdited = 0;
		ent->sampleDescriptionIndex = sampleDescIndex;
		//here's the trick: each sample is in ONE chunk
		ent->firstChunk = i+1;
		ent->nextChunk = i+2;
		ent->samplesPerChunk = 1;
		e = gf_list_add(stsc_tmp->entryList, ent);
		if (e) goto err_exit;
		if (stco_tmp) {
			stco_tmp->offsets[i] = (u32) dataOffset;
		} else {
			co64_tmp->offsets[i] = dataOffset;
		}
	}
	//close the list
	if (ent) ent->nextChunk = 0;
	

	//done, remove our previous tables
	gf_isom_box_del(stbl->ChunkOffset);
	gf_isom_box_del((GF_Box *)stbl->SampleToChunk);
	//and set these ones...
	if (stco_tmp) {
		stbl->ChunkOffset = (GF_Box *)stco_tmp;
	} else {
		stbl->ChunkOffset = (GF_Box *)co64_tmp;
	}
	stbl->SampleToChunk = stsc_tmp;
	stbl->SampleToChunk->currentEntry = (GF_StscEntry*)gf_list_get(stbl->SampleToChunk->entryList, 0);
	stbl->SampleToChunk->currentIndex = 0;
	stbl->SampleToChunk->currentChunk = 0;
	stbl->SampleToChunk->firstSampleInCurrentChunk = 0;
	return GF_OK;

err_exit:
	if (stco_tmp) gf_isom_box_del((GF_Box *) stco_tmp);
	if (co64_tmp) gf_isom_box_del((GF_Box *) co64_tmp);
	if (stsc_tmp) gf_isom_box_del((GF_Box *) stsc_tmp);
	return e;
}

#ifndef GPAC_READ_ONLY



static GFINLINE GF_Err stbl_AddOffset(GF_Box **a, u64 offset)
{
	GF_ChunkOffsetBox *stco;
	GF_ChunkLargeOffsetBox *co64;
	u32 i;

	if ((*a)->type == GF_ISOM_BOX_TYPE_STCO) {
		stco = (GF_ChunkOffsetBox *) *a;
		//if dataOffset is bigger than 0xFFFFFFFF, move to LARGE offset
		if (offset > 0xFFFFFFFF) {
			co64 = (GF_ChunkLargeOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CO64);
			if (!co64) return GF_OUT_OF_MEM;
			co64->entryCount = stco->entryCount + 1;
			co64->offsets = (u64*)malloc(co64->entryCount * sizeof(u64));
			if (!co64->offsets) {
				gf_isom_box_del((GF_Box *)co64);
				return GF_OUT_OF_MEM;
			}
			for (i = 0; i< co64->entryCount - 1; i++) {
				co64->offsets[i] = (u64) stco->offsets[i];
			}
			co64->offsets[i] = offset;
			//delete the box...
			gf_isom_box_del(*a);
			*a = (GF_Box *)co64;
			return GF_OK;
		}
		//OK, stick with regular...
		stco->offsets = (u32*)realloc(stco->offsets, (stco->entryCount + 1) * sizeof(u32));
		if (!stco->offsets) return GF_OUT_OF_MEM;
		stco->offsets[stco->entryCount] = (u32) offset;
		stco->entryCount += 1;
	} else {
		//this is a large offset
		co64 = (GF_ChunkLargeOffsetBox *) *a;
		co64->offsets = (u64*)realloc(co64->offsets, (co64->entryCount + 1) * sizeof(u64));
		if (!co64->offsets) return GF_OUT_OF_MEM;
		co64->offsets[co64->entryCount] = offset;
		co64->entryCount += 1;
	}
	return GF_OK;
}

//This function packs the offset after easy editing, eg samples
//are re-arranged in chunks according to the chunkOffsets
//NOTE: this has to be called once interleaving or whatever is done and 
//the final MDAT is written!!!
GF_Err stbl_SetChunkAndOffset(GF_SampleTableBox *stbl, u32 sampleNumber, u32 StreamDescIndex, GF_SampleToChunkBox *the_stsc, GF_Box **the_stco, u64 data_offset, u8 forceNewChunk)
{
	GF_Err e;
	u32 count;
	u8 newChunk;
	GF_StscEntry *ent, *newEnt;

	if (!stbl) return GF_ISOM_INVALID_FILE;

	newChunk = 0;
	//do we need a new chunk ??? For that, we need
	//1 - make sure this sample data is contiguous to the prev one

	//force new chunk is set during writing (flat / interleaved)
	//it is set to 1 when data is not contiguous in the media (eg, interleaving)
	//when writing flat files, it is never used
	if (forceNewChunk) newChunk = 1;

	//2 - make sure we have the table inited (i=0)
	if (! the_stsc->currentEntry) {
		newChunk = 1;
	} else {
	//3 - make sure we do not exceed the MaxSamplesPerChunk and we have the same descIndex
		if (StreamDescIndex != the_stsc->currentEntry->sampleDescriptionIndex) 
			newChunk = 1;
		if (stbl->MaxSamplePerChunk && the_stsc->currentEntry->samplesPerChunk == stbl->MaxSamplePerChunk) 
			newChunk = 1;
	}

	//no need for a new chunk
	if (!newChunk) {
		the_stsc->currentEntry->samplesPerChunk += 1;
		return GF_OK;
	}

	//OK, we have to create a new chunk...
	count = gf_list_count(the_stsc->entryList);
	//check if we can remove the current sampleToChunk entry (same properties)
	if (count > 1) {
		ent = (GF_StscEntry*)gf_list_get(the_stsc->entryList, count - 2);
		if ( (ent->sampleDescriptionIndex == the_stsc->currentEntry->sampleDescriptionIndex)
			&& (ent->samplesPerChunk == the_stsc->currentEntry->samplesPerChunk)
			) {
			//OK, it's the same SampleToChunk, so delete it
			ent->nextChunk = the_stsc->currentEntry->firstChunk;
			free(the_stsc->currentEntry);
			gf_list_rem(the_stsc->entryList, count - 1);
			the_stsc->currentEntry = ent;
		}
	}

	//add our offset
	e = stbl_AddOffset(the_stco, data_offset);
	if (e) return e;

	//create a new entry (could be the first one, BTW)
	newEnt = (GF_StscEntry*)malloc(sizeof(GF_StscEntry));
	if (!newEnt) return GF_OUT_OF_MEM;
	//get the first chunk value
	if ((*the_stco)->type == GF_ISOM_BOX_TYPE_STCO) {
		newEnt->firstChunk = ((GF_ChunkOffsetBox *) (*the_stco) )->entryCount;
	} else {
		newEnt->firstChunk = ((GF_ChunkLargeOffsetBox *) (*the_stco) )->entryCount;
	}
	newEnt->sampleDescriptionIndex = StreamDescIndex;
	newEnt->samplesPerChunk = 1;
	newEnt->nextChunk = 0;
	gf_list_add(the_stsc->entryList, newEnt);
	//if we already have an entry, adjust its next chunk to point to our new chunk
	if (the_stsc->currentEntry)
		the_stsc->currentEntry->nextChunk = newEnt->firstChunk;
	the_stsc->currentEntry = newEnt;
	return GF_OK;
}


GF_Err gf_isom_refresh_size_info(GF_ISOFile *file, u32 trackNumber)
{
	u32 i, size;
	GF_TrackBox *trak;
	GF_SampleSizeBox *stsz;
	trak = gf_isom_get_track_from_file(file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsz = trak->Media->information->sampleTable->SampleSize;
	if (stsz->sampleSize || !stsz->sampleCount) return GF_OK;

	size = stsz->sizes[0];
	for (i=1; i<stsz->sampleCount; i++) {
		if (stsz->sizes[i] != size) {
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

#endif //GPAC_READ_ONLY

