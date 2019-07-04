/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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

//Get the sample number
GF_Err stbl_findEntryForTime(GF_SampleTableBox *stbl, u64 DTS, u8 useCTS, u32 *sampleNumber, u32 *prevSampleNumber)
{
	u32 i, j, curSampNum, count;
	s32 CTSOffset;
	u64 curDTS;
	GF_SttsEntry *ent;
	(*sampleNumber) = 0;
	(*prevSampleNumber) = 0;

	if (!stbl->TimeToSample) return GF_ISOM_INVALID_FILE;
	/*if (!stbl->CompositionOffset) useCTS = 0;
	FIXME: CTS is ALWAYS disabled for now to make sure samples are fetched in
	decoding order. */
	useCTS = 0;

	//our cache
	if (stbl->TimeToSample->r_FirstSampleInEntry &&
	        (DTS >= stbl->TimeToSample->r_CurrentDTS) ) {
		//if we're using CTS, we don't really know whether we're in the good entry or not
		//(eg, the real DTS of the sample could be in a previous entry
		i = stbl->TimeToSample->r_currentEntryIndex;
		curDTS = stbl->TimeToSample->r_CurrentDTS;
		curSampNum = stbl->TimeToSample->r_FirstSampleInEntry;
	} else {
		i = 0;
		curDTS = stbl->TimeToSample->r_CurrentDTS = 0;
		curSampNum = stbl->TimeToSample->r_FirstSampleInEntry = 1;
		stbl->TimeToSample->r_currentEntryIndex = 0;
	}

	//we need to validate our cache if we are using CTS because of B-frames and co...
	if (i && useCTS) {
		while (1) {
			stbl_GetSampleCTS(stbl->CompositionOffset, curSampNum, &CTSOffset);
			//we're too far, rewind
			if ( i && (curDTS + CTSOffset > DTS) ) {
				ent = &stbl->TimeToSample->entries[i];
				curSampNum -= ent->sampleCount;
				curDTS -= (u64)ent->sampleDelta * ent->sampleCount;
				i --;
			} else if (!i) {
				//beginning of the table, no choice
				curDTS = stbl->TimeToSample->r_CurrentDTS = 0;
				curSampNum = stbl->TimeToSample->r_FirstSampleInEntry = 1;
				stbl->TimeToSample->r_currentEntryIndex = 0;
				break;
			} else {
				//OK now we're good
				break;
			}
		}
	}

	//look for the DTS from this entry
	count = stbl->TimeToSample->nb_entries;
	for (; i<count; i++) {
		ent = &stbl->TimeToSample->entries[i];
		if (useCTS) {
			stbl_GetSampleCTS(stbl->CompositionOffset, curSampNum, &CTSOffset);
		} else {
			CTSOffset = 0;
		}
		for (j=0; j<ent->sampleCount; j++) {
			if (curDTS + CTSOffset >= DTS) goto entry_found;
			curSampNum += 1;
			curDTS += ent->sampleDelta;
		}
		//we're switching to the next entry, update the cache!
		stbl->TimeToSample->r_CurrentDTS += (u64)ent->sampleCount * ent->sampleDelta;
		stbl->TimeToSample->r_currentEntryIndex += 1;
		stbl->TimeToSample->r_FirstSampleInEntry += ent->sampleCount;
	}
	//return as is
	return GF_OK;

entry_found:
	//do we have the exact time ?
	if (curDTS + CTSOffset == DTS) {
		(*sampleNumber) = curSampNum;
	}
	//if we match the exact DTS also select this sample
	else if (curDTS == DTS) {
		(*sampleNumber) = curSampNum;
	} else {
		//exception for the first sample (we need to "load" the playback)
		if (curSampNum != 1) {
			(*prevSampleNumber) = curSampNum - 1;
		} else {
			(*prevSampleNumber) = 1;
		}
	}
	return GF_OK;
}

//Get the Size of a given sample
GF_Err stbl_GetSampleSize(GF_SampleSizeBox *stsz, u32 SampleNumber, u32 *Size)
{
	if (!stsz || !SampleNumber || SampleNumber > stsz->sampleCount) return GF_BAD_PARAM;

	(*Size) = 0;

	if (stsz->sampleSize && (stsz->type != GF_ISOM_BOX_TYPE_STZ2)) {
		(*Size) = stsz->sampleSize;
	} else if (stsz->sizes) {
		(*Size) = stsz->sizes[SampleNumber - 1];
	}
	return GF_OK;
}



//Get the CTS offset of a given sample
GF_Err stbl_GetSampleCTS(GF_CompositionOffsetBox *ctts, u32 SampleNumber, s32 *CTSoffset)
{
	u32 i;

	(*CTSoffset) = 0;
	//test on SampleNumber is done before
	if (!ctts || !SampleNumber) return GF_BAD_PARAM;

	if (ctts->r_FirstSampleInEntry && (ctts->r_FirstSampleInEntry < SampleNumber) ) {
		i = ctts->r_currentEntryIndex;
	} else {
		ctts->r_FirstSampleInEntry = 1;
		ctts->r_currentEntryIndex = 0;
		i = 0;
	}
	for (; i< ctts->nb_entries; i++) {
		if (SampleNumber < ctts->r_FirstSampleInEntry + ctts->entries[i].sampleCount) break;
		//update our cache
		ctts->r_currentEntryIndex += 1;
		ctts->r_FirstSampleInEntry += ctts->entries[i].sampleCount;
	}
	//no ent, set everything to 0...
	if (i==ctts->nb_entries) return GF_OK;
	/*asked for a sample not in table - this means CTTS is 0 (that's due to out internal packing construction of CTTS)*/
	if (SampleNumber >= ctts->r_FirstSampleInEntry + ctts->entries[i].sampleCount) return GF_OK;
	(*CTSoffset) = ctts->entries[i].decodingOffset;
	return GF_OK;
}

//Get the DTS of a sample
GF_Err stbl_GetSampleDTS_and_Duration(GF_TimeToSampleBox *stts, u32 SampleNumber, u64 *DTS, u32 *duration)
{
	u32 i, j, count;
	GF_SttsEntry *ent;

	(*DTS) = 0;
	if (duration) {
		*duration = 0;
	}
	if (!stts || !SampleNumber) return GF_BAD_PARAM;

	ent = NULL;
	//use our cache
	count = stts->nb_entries;
	if (stts->r_FirstSampleInEntry
	        && (stts->r_FirstSampleInEntry <= SampleNumber)
	        //this is for read/write access
	        && (stts->r_currentEntryIndex < count) ) {

		i = stts->r_currentEntryIndex;
	} else {
		i = stts->r_currentEntryIndex = 0;
		stts->r_FirstSampleInEntry = 1;
		stts->r_CurrentDTS = 0;
	}

	for (; i < count; i++) {
		ent = &stts->entries[i];

		//in our entry
		if (ent->sampleCount + stts->r_FirstSampleInEntry >= 1 + SampleNumber) {
			j = SampleNumber - stts->r_FirstSampleInEntry;
			goto found;
		}

		//update our cache
		stts->r_CurrentDTS += (u64)ent->sampleCount * ent->sampleDelta;
		stts->r_currentEntryIndex += 1;
		stts->r_FirstSampleInEntry += ent->sampleCount;
	}
//	if (SampleNumber >= stts->r_FirstSampleInEntry + ent->sampleCount) return GF_BAD_PARAM;

	//no ent, this is really weird. Let's assume the DTS is then what is written in the table
	if (!ent || (i == count)) {
		(*DTS) = stts->r_CurrentDTS;
		if (duration) *duration = ent ? ent->sampleDelta : 0;
	}
	return GF_OK;

found:
	(*DTS) = stts->r_CurrentDTS + j * (u64) ent->sampleDelta;
	if (duration) *duration = ent->sampleDelta;
	return GF_OK;
}

GF_Err stbl_GetSampleDTS(GF_TimeToSampleBox *stts, u32 SampleNumber, u64 *DTS)
{
	return stbl_GetSampleDTS_and_Duration(stts, SampleNumber, DTS, NULL);
}
//Retrieve closes RAP for a given sample - if sample is RAP, sets the RAP flag
GF_Err stbl_GetSampleRAP(GF_SyncSampleBox *stss, u32 SampleNumber, SAPType *IsRAP, u32 *prevRAP, u32 *nextRAP)
{
	u32 i;
	if (prevRAP) *prevRAP = 0;
	if (nextRAP) *nextRAP = 0;

	(*IsRAP) = RAP_NO;
	if (!stss || !SampleNumber) return GF_BAD_PARAM;

	if (stss->r_LastSyncSample && (stss->r_LastSyncSample < SampleNumber) ) {
		i = stss->r_LastSampleIndex;
	} else {
		i = 0;
	}
	for (; i < stss->nb_entries; i++) {
		//get the entry
		if (stss->sampleNumbers[i] == SampleNumber) {
			//update the cache
			stss->r_LastSyncSample = SampleNumber;
			stss->r_LastSampleIndex = i;
			(*IsRAP) = RAP;
		}
		else if (stss->sampleNumbers[i] > SampleNumber) {
			if (nextRAP) *nextRAP = stss->sampleNumbers[i];
			return GF_OK;
		}
		if (prevRAP) *prevRAP = stss->sampleNumbers[i];
	}
	return GF_OK;
}

GF_Err stbl_SearchSAPs(GF_SampleTableBox *stbl, u32 SampleNumber, SAPType *IsRAP, u32 *prevRAP, u32 *nextRAP)
{
	u32 i, j, count, count2;
	assert(prevRAP);
	assert(nextRAP);
	(*prevRAP) = 0;
	(*nextRAP) = 0;
	(*IsRAP) = RAP_NO;

	if (!stbl->sampleGroups || !stbl->sampleGroupsDescription) return GF_OK;

	count = gf_list_count(stbl->sampleGroups);
	count2 = gf_list_count(stbl->sampleGroupsDescription);
	for (i=0; i<count; i++) {
		GF_SampleGroupDescriptionBox *sgdp = NULL;
		Bool is_rap_group = 0;
		s32 roll_distance = 0;
		u32 first_sample_in_entry, last_sample_in_entry;
		GF_SampleGroupBox *sg = gf_list_get(stbl->sampleGroups, i);
		switch (sg->grouping_type) {
		case GF_ISOM_SAMPLE_GROUP_RAP:
		case GF_ISOM_SAMPLE_GROUP_SYNC:
			is_rap_group = 1;
			break;
		case GF_ISOM_SAMPLE_GROUP_ROLL:
			break;
		default:
			continue;
		}
		for (j=0; j<count2; j++) {
			sgdp = gf_list_get(stbl->sampleGroupsDescription, j);
			if (sgdp->grouping_type==sg->grouping_type) break;
			sgdp = NULL;
		}
		if (! sgdp) continue;

		first_sample_in_entry=1;
		for (j=0; j<sg->entry_count; j++) {
			u32 first_rap_in_entry, last_rap_in_entry;
			last_sample_in_entry = first_sample_in_entry + sg->sample_entries[j].sample_count - 1;

			/*samples in this entry are not RAPs, continue*/
			if (! sg->sample_entries[j].group_description_index) {
				first_sample_in_entry += sg->sample_entries[j].sample_count;
				continue;
			}
			if (!is_rap_group) {
				GF_RollRecoveryEntry *entry = gf_list_get(sgdp->group_descriptions, sg->sample_entries[j].group_description_index - 1);
				roll_distance = entry ? entry->roll_distance : 0;
			}

			/*we consider the first sample in a roll or rap group entry to be the RAP (eg, we have to decode from this sample anyway)
			except if roll_distance is strictly negative in which case we have to rewind our sample numbers from roll_distance*/
			if (roll_distance < 0) {
				if ((s32) first_sample_in_entry + roll_distance>=0) first_rap_in_entry = first_sample_in_entry + roll_distance;
				else first_rap_in_entry = 0;

				if ((s32) last_sample_in_entry + roll_distance>=0) last_rap_in_entry = last_sample_in_entry + roll_distance;
				else last_rap_in_entry = 0;
			} else {
				first_rap_in_entry = first_sample_in_entry;
				last_rap_in_entry = last_sample_in_entry;
			}

			/*store previous & next sample RAP - note that we do not store the closest previous RAP, only the first of the previous RAP group
			as RAPs are usually isolated this should not be an issue*/
			if (prevRAP && (first_rap_in_entry <= SampleNumber)) {
				*prevRAP = first_rap_in_entry;
			}
			if (nextRAP) {
				*nextRAP = last_rap_in_entry;
			}

			/*sample lies in this (rap) group, it is rap*/
			if (is_rap_group) {
				if ((first_rap_in_entry <= SampleNumber) && (SampleNumber <= last_rap_in_entry)) {
					(*IsRAP) = RAP;
					return GF_OK;
				}
			} else {
				/*prevRAP or nextRAP matches SampleNumber, sample is RAP*/
				if ((*prevRAP == SampleNumber) || (*nextRAP == SampleNumber)) {
					(*IsRAP) = RAP;
					return GF_OK;
				}
			}

			/*first sample in entry is after our target sample, abort*/
			if (first_rap_in_entry > SampleNumber) {
				break;
			}
			first_sample_in_entry += sg->sample_entries[j].sample_count;
		}
	}
	return GF_OK;
}

//get the number of "ghost chunk" (implicit chunks described by an entry)
void GetGhostNum(GF_StscEntry *ent, u32 EntryIndex, u32 count, GF_SampleTableBox *stbl)
{
	GF_StscEntry *nextEnt;
	GF_ChunkOffsetBox *stco;
	GF_ChunkLargeOffsetBox *co64;
	u32 ghostNum = 1;

	if (!ent->nextChunk) {
		if (EntryIndex+1 == count) {
			//not specified in the spec, what if the last sample to chunk is no written?
			if (stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
				stco = (GF_ChunkOffsetBox *)stbl->ChunkOffset;
				ghostNum = (stco->nb_entries > ent->firstChunk) ? (1 + stco->nb_entries - ent->firstChunk) : 1;
			} else {
				co64 = (GF_ChunkLargeOffsetBox *)stbl->ChunkOffset;
				ghostNum = (co64->nb_entries > ent->firstChunk) ? (1 + co64->nb_entries - ent->firstChunk) : 1;
			}
		} else {
			//this is an unknown case due to edit mode...
			nextEnt = &stbl->SampleToChunk->entries[EntryIndex+1];
			ghostNum = nextEnt->firstChunk - ent->firstChunk;
		}
	} else {
		ghostNum = (ent->nextChunk > ent->firstChunk) ? (ent->nextChunk - ent->firstChunk) : 1;
	}
	stbl->SampleToChunk->ghostNumber = ghostNum;
}

//Get the offset, descIndex and chunkNumber of a sample...
GF_Err stbl_GetSampleInfos(GF_SampleTableBox *stbl, u32 sampleNumber, u64 *offset, u32 *chunkNumber, u32 *descIndex, GF_StscEntry **out_ent)
{
	GF_Err e;
	u32 i, k, offsetInChunk, size;
	GF_ChunkOffsetBox *stco;
	GF_ChunkLargeOffsetBox *co64;
	GF_StscEntry *ent;

	(*offset) = 0;
	(*chunkNumber) = (*descIndex) = 0;
	if (out_ent) (*out_ent) = NULL;
	if (!stbl || !sampleNumber) return GF_BAD_PARAM;
	if (!stbl->ChunkOffset || !stbl->SampleToChunk) return GF_ISOM_INVALID_FILE;

	if (stbl->SampleToChunk->nb_entries == stbl->SampleSize->sampleCount) {
		ent = &stbl->SampleToChunk->entries[sampleNumber-1];
		if (!ent) return GF_BAD_PARAM;
		(*descIndex) = ent->sampleDescriptionIndex;
		(*chunkNumber) = sampleNumber;
		if (out_ent) *out_ent = ent;
		if ( stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
			stco = (GF_ChunkOffsetBox *)stbl->ChunkOffset;
			(*offset) = (u64) stco->offsets[sampleNumber - 1];
		} else {
			co64 = (GF_ChunkLargeOffsetBox *)stbl->ChunkOffset;
			(*offset) = co64->offsets[sampleNumber - 1];
		}
		return GF_OK;
	}

	//check our cache
	if (stbl->SampleToChunk->firstSampleInCurrentChunk &&
	        (stbl->SampleToChunk->firstSampleInCurrentChunk < sampleNumber)) {

		i = stbl->SampleToChunk->currentIndex;
//		ent = gf_list_get(stbl->SampleToChunk->entryList, i);
		ent = &stbl->SampleToChunk->entries[stbl->SampleToChunk->currentIndex];
		GetGhostNum(ent, i, stbl->SampleToChunk->nb_entries, stbl);
		k = stbl->SampleToChunk->currentChunk;
	} else {
		i = 0;
		stbl->SampleToChunk->currentIndex = 0;
		stbl->SampleToChunk->currentChunk = 1;
		stbl->SampleToChunk->firstSampleInCurrentChunk = 1;
		ent = &stbl->SampleToChunk->entries[0];
		GetGhostNum(ent, 0, stbl->SampleToChunk->nb_entries, stbl);
		k = stbl->SampleToChunk->currentChunk;
	}

	//first get the chunk
	for (; i < stbl->SampleToChunk->nb_entries; i++) {
		//browse from the current chunk we're browsing from index 1
		for (; k <= stbl->SampleToChunk->ghostNumber; k++) {
			if ((stbl->SampleToChunk->firstSampleInCurrentChunk <= sampleNumber)
				&& (stbl->SampleToChunk->firstSampleInCurrentChunk + ent->samplesPerChunk > sampleNumber)
			) {
				goto sample_found;
			}

			//nope, get to next chunk
			stbl->SampleToChunk->firstSampleInCurrentChunk += ent->samplesPerChunk;
			stbl->SampleToChunk->currentChunk ++;
		}
		//not in this entry, get the next entry if not the last one
		if (i+1 != stbl->SampleToChunk->nb_entries) {
			ent = &stbl->SampleToChunk->entries[i+1];
			//update the GhostNumber
			GetGhostNum(ent, i+1, stbl->SampleToChunk->nb_entries, stbl);
			//update the entry in our cache
			stbl->SampleToChunk->currentIndex = i+1;
			stbl->SampleToChunk->currentChunk = 1;
			k = 1;
		}
	}
	//if we get here, gasp, the sample was not found
	return GF_ISOM_INVALID_FILE;

sample_found:

	(*descIndex) = ent->sampleDescriptionIndex;
	(*chunkNumber) = ent->firstChunk + stbl->SampleToChunk->currentChunk - 1;
	if (out_ent) *out_ent = ent;
	if (! *chunkNumber)
		return GF_ISOM_INVALID_FILE;
	
	//ok, get the size of all the previous samples in the chunk
	offsetInChunk = 0;
	//constant size
	if (stbl->SampleSize->sampleSize) {
		u32 diff = sampleNumber - stbl->SampleToChunk->firstSampleInCurrentChunk;
		offsetInChunk += diff * stbl->SampleSize->sampleSize;
	} else {
		//warning, firstSampleInChunk is at least 1 - not 0
		for (i = stbl->SampleToChunk->firstSampleInCurrentChunk; i < sampleNumber; i++) {
			e = stbl_GetSampleSize(stbl->SampleSize, i, &size);
			if (e) return e;
			offsetInChunk += size;
		}
	}
	//OK, that's the size of our offset in the chunk
	//now get the chunk
	if ( stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
		stco = (GF_ChunkOffsetBox *)stbl->ChunkOffset;
		if (stco->nb_entries < (*chunkNumber) ) return GF_ISOM_INVALID_FILE;
		(*offset) = (u64) stco->offsets[(*chunkNumber) - 1] + (u64) offsetInChunk;
	} else {
		co64 = (GF_ChunkLargeOffsetBox *)stbl->ChunkOffset;
		if (co64->nb_entries < (*chunkNumber) ) return GF_ISOM_INVALID_FILE;
		(*offset) = co64->offsets[(*chunkNumber) - 1] + (u64) offsetInChunk;
	}
	return GF_OK;
}


GF_Err stbl_GetSampleShadow(GF_ShadowSyncBox *stsh, u32 *sampleNumber, u32 *syncNum)
{
	u32 i, count;
	GF_StshEntry *ent;

	if (stsh->r_LastFoundSample && (stsh->r_LastFoundSample <= *sampleNumber)) {
		i = stsh->r_LastEntryIndex;
	} else {
		i = 0;
		stsh->r_LastFoundSample = 1;
	}

	ent = NULL;
	(*syncNum) = 0;

	count = gf_list_count(stsh->entries);
	for (; i<count; i++) {
		ent = (GF_StshEntry*)gf_list_get(stsh->entries, i);
		//we get the exact desired sample !
		if (ent->shadowedSampleNumber == *sampleNumber) {
			(*syncNum) = ent->syncSampleNumber;
			stsh->r_LastFoundSample = *sampleNumber;
			stsh->r_LastEntryIndex = i;
			return GF_OK;
		} else if (ent->shadowedSampleNumber > *sampleNumber) {
			//do we have an entry before ? If not, there is no shadowing available
			//for this sample
			if (!i) return GF_OK;
			//ok, indicate the previous ShadowedSample
			ent = (GF_StshEntry*)gf_list_get(stsh->entries, i-1);
			(*syncNum) = ent->syncSampleNumber;
			//change the sample number
			(*sampleNumber) = ent->shadowedSampleNumber;
			//reset the cache to the last ShadowedSample
			stsh->r_LastEntryIndex = i-1;
			stsh->r_LastFoundSample = ent->shadowedSampleNumber;
		}
	}
	stsh->r_LastEntryIndex = i-1;
	stsh->r_LastFoundSample = ent ? ent->shadowedSampleNumber : 0;
	return GF_OK;
}



GF_Err stbl_GetPaddingBits(GF_PaddingBitsBox *padb, u32 SampleNumber, u8 *PadBits)
{
	if (!PadBits) return GF_BAD_PARAM;
	*PadBits = 0;
	if (!padb || !padb->padbits) return GF_OK;
	//the spec says "should" not shall. return 0 padding
	if (padb->SampleCount < SampleNumber) return GF_OK;
	*PadBits = padb->padbits[SampleNumber-1];
	return GF_OK;
}

//Set the RAP flag of a sample
GF_Err stbl_GetSampleDepType(GF_SampleDependencyTypeBox *sdep, u32 SampleNumber, u32 *isLeading, u32 *dependsOn, u32 *dependedOn, u32 *redundant)
{
	u8 flag;

	assert(dependsOn && dependedOn && redundant);
	*dependsOn = *dependedOn = *redundant = 0;

	if (SampleNumber > sdep->sampleCount) {
		return GF_OK;
	}
	flag = sdep->sample_info[SampleNumber-1];
	*isLeading = (flag >> 6) & 3;
	*dependsOn = (flag >> 4) & 3;
	*dependedOn = (flag >> 2) & 3;
	*redundant = (flag) & 3;
	return GF_OK;
}


#endif /*GPAC_DISABLE_ISOM*/
