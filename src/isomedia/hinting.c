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


GF_Box *ghnt_New()
{
	GF_HintSampleEntryBox *tmp = (GF_HintSampleEntryBox *) malloc(sizeof(GF_HintSampleEntryBox));
	if (tmp == NULL) return NULL;
	memset(tmp, 0, sizeof(GF_HintSampleEntryBox));
	tmp->HintDataTable = gf_list_new();
	if (!tmp->HintDataTable) {
		free(tmp);
		return NULL;
	}
	//this type is used internally for protocols that share the same base entry
	//currently only RTP uses this, but a flexMux could use this entry too...
	tmp->type = GF_ISOM_BOX_TYPE_GHNT;
	tmp->HintTrackVersion = 1;
	tmp->LastCompatibleVersion = 1;
	return (GF_Box *)tmp;
}

void ghnt_del(GF_Box *s)
{
	GF_HintSampleEntryBox *ptr;
	
	ptr = (GF_HintSampleEntryBox *)s;
	gf_isom_box_array_del(ptr->HintDataTable);
	if (ptr->w_sample) Del_HintSample(ptr->w_sample);
	free(ptr);
}

GF_Err ghnt_Read(GF_Box *s, GF_BitStream *bs, u64 *read)
{
	GF_Box *a;
	u64 sr;
	GF_Err e;
	GF_HintSampleEntryBox *ptr = (GF_HintSampleEntryBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	gf_bs_read_data(bs, ptr->reserved, 6);
	ptr->dataReferenceIndex = gf_bs_read_u16(bs);
	*read += 8;
	ptr->HintTrackVersion = gf_bs_read_u16(bs);
	ptr->LastCompatibleVersion = gf_bs_read_u16(bs);
	ptr->MaxPacketSize = gf_bs_read_u32(bs);
	*read += 8;

	while (*read < ptr->size) {
		e = gf_isom_parse_box(&a, bs, &sr);
		if (e) return e;
		e = gf_list_add(ptr->HintDataTable, a);
		if (e) return e;
		*read += a->size;
	}
	if (*read != ptr->size) return GF_ISOM_INVALID_FILE;
	return GF_OK;
}

#ifndef GPAC_READ_ONLY

GF_Err ghnt_Write(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HintSampleEntryBox *ptr = (GF_HintSampleEntryBox *)s;

	e = gf_isom_box_write_header(s, bs);
	if (e) return e;
	gf_bs_write_data(bs, ptr->reserved, 6);
	gf_bs_write_u16(bs, ptr->dataReferenceIndex);
	gf_bs_write_u16(bs, ptr->HintTrackVersion);
	gf_bs_write_u16(bs, ptr->LastCompatibleVersion);
	gf_bs_write_u32(bs, ptr->MaxPacketSize);
	return gf_isom_box_array_write(s, ptr->HintDataTable, bs);
}

GF_Err ghnt_Size(GF_Box *s)
{
	GF_Err e;
	GF_HintSampleEntryBox *ptr = (GF_HintSampleEntryBox *)s;
	
	e = gf_isom_box_get_size(s);
	if (e) return e;
	ptr->size += 16;
	e = gf_isom_box_array_size(s, ptr->HintDataTable);
	if (e) return e;
	return GF_OK;
}


#endif	//GPAC_READ_ONLY


GF_HintSample *New_HintSample(u32 ProtocolType)
{
	GF_HintSample *tmp;
	u8 type;

	switch (ProtocolType) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
		type = GF_ISMO_HINT_RTP;
		break;
	default:
		return NULL;
	}
	tmp = malloc(sizeof(GF_HintSample));
	tmp->packetTable = gf_list_new();
	tmp->AdditionalData = NULL;
	tmp->dataLength = 0;
	tmp->HintType = type;
	tmp->TransmissionTime = 0;
	tmp->reserved = 0;
	return tmp;
}

void Del_HintSample(GF_HintSample *ptr)
{
	GF_HintPacket *pck;

	while (gf_list_count(ptr->packetTable)) {
		pck = gf_list_get(ptr->packetTable, 0);
		Del_HintPacket(ptr->HintType, pck);
		gf_list_rem(ptr->packetTable, 0);
	}
	gf_list_del(ptr->packetTable);
	if (ptr->AdditionalData) free(ptr->AdditionalData);
	free(ptr);
}

GF_Err Read_HintSample(GF_HintSample *ptr, GF_BitStream *bs, u32 sampleSize)
{
	u16 entryCount, i;
	GF_HintPacket *pck;
	GF_Err e;
	u64 sizeIn, sizeOut;

	sizeIn = gf_bs_available(bs);

	entryCount = gf_bs_read_u16(bs);
	ptr->reserved = gf_bs_read_u16(bs);

	for (i = 0; i < entryCount; i++) {
		pck = New_HintPacket(ptr->HintType);
		e = Read_HintPacket(ptr->HintType, pck, bs);
		if (e) return e;
		gf_list_add(ptr->packetTable, pck);
	}

	sizeOut = gf_bs_available(bs) - sizeIn;

	//do we have some more data after the packets ??
	if ((u32)sizeOut < sampleSize) {
		ptr->dataLength = sampleSize - (u32)sizeOut;
		ptr->AdditionalData = malloc(sizeof(char) * ptr->dataLength);
		gf_bs_read_data(bs, ptr->AdditionalData, ptr->dataLength);
	}
	return GF_OK;
}


#ifndef GPAC_READ_ONLY

u32 Write_HintSample(GF_HintSample *ptr, GF_BitStream *bs)
{
	u32 count, i;
	GF_HintPacket *pck;
	GF_Err e;

	count = gf_list_count(ptr->packetTable);
	gf_bs_write_u16(bs, count);
	gf_bs_write_u16(bs, ptr->reserved);
	//write the packet table
	for (i=0; i<count; i++) {
		pck = gf_list_get(ptr->packetTable, i);
		e = Write_HintPacket(ptr->HintType, pck, bs);
		if (e) return e;
	}
	//write additional data
	if (ptr->AdditionalData) {
		gf_bs_write_data(bs, ptr->AdditionalData, ptr->dataLength);
	}
	return GF_OK;
}


u32 Size_HintSample(GF_HintSample *ptr)
{
	u32 size, count, i;
	GF_HintPacket *pck;

	size = 4;
	count = gf_list_count(ptr->packetTable);
	for (i=0; i<count; i++) {
		pck = gf_list_get(ptr->packetTable, i);
		size += Size_HintPacket(ptr->HintType, pck);
	}
	size += ptr->dataLength;
	return size;
}

#endif



GF_HintPacket *New_HintPacket(u8 HintType)
{
	switch (HintType) {
	case GF_ISMO_HINT_RTP:
		return (GF_HintPacket *) New_RTPPacket();

	default:
		return NULL;
	}
}

void Del_HintPacket(u8 HintType, GF_HintPacket *ptr)
{
	switch (HintType) {
	case GF_ISMO_HINT_RTP:
		Del_RTPPacket((GF_RTPPacket *)ptr);
		break;
	default:
		break;
	}
}

GF_Err Read_HintPacket(u8 HintType, GF_HintPacket *ptr, GF_BitStream *bs)
{
	switch (HintType) {
	case GF_ISMO_HINT_RTP:
		return Read_RTPPacket((GF_RTPPacket *)ptr, bs);
	default:
		return GF_NOT_SUPPORTED;
	}
}

#ifndef GPAC_READ_ONLY

GF_Err Write_HintPacket(u8 HintType, GF_HintPacket *ptr, GF_BitStream *bs)
{
	switch (HintType) {
	case GF_ISMO_HINT_RTP:
		return Write_RTPPacket((GF_RTPPacket *)ptr, bs);
	default:
		return GF_NOT_SUPPORTED;
	}
}

u32 Size_HintPacket(u8 HintType, GF_HintPacket *ptr)
{
	switch (HintType) {
	case GF_ISMO_HINT_RTP:
		return Size_RTPPacket((GF_RTPPacket *)ptr);
	default:
		return 0;
	}
}

GF_Err Offset_HintPacket(u8 HintType, GF_HintPacket *ptr, u32 offset, u32 HintSampleNumber)
{
	switch (HintType) {
	case GF_ISMO_HINT_RTP:
		return Offset_RTPPacket((GF_RTPPacket *)ptr, offset, HintSampleNumber);
	default:
		return GF_NOT_SUPPORTED;
	}
}

GF_Err AddDTE_HintPacket(u8 HintType, GF_HintPacket *ptr, GF_GenericDTE *dte, u8 AtBegin)
{
	switch (HintType) {
	case GF_ISMO_HINT_RTP:
		if (AtBegin)
			return gf_list_insert( ((GF_RTPPacket *)ptr)->DataTable, dte, 0);
		else
			return gf_list_add( ((GF_RTPPacket *)ptr)->DataTable, dte);

	default:
		return GF_NOT_SUPPORTED;
	}
}

u32 Length_HintPacket(u8 HintType, GF_HintPacket *ptr)
{
	switch (HintType) {
	case GF_ISMO_HINT_RTP:
		return Length_RTPPacket((GF_RTPPacket *)ptr);
	default:
		return 0;
	}
}


#endif



/********************************************************************
		Creation of DataTable entries in the RTP sample
********************************************************************/

GF_GenericDTE *New_EmptyDTE()
{
	GF_EmptyDTE *dte = malloc(sizeof(GF_EmptyDTE));
	dte->source = 0;
	return (GF_GenericDTE *)dte;
}

GF_GenericDTE *New_ImmediateDTE()
{
	GF_ImmediateDTE *dte = malloc(sizeof(GF_ImmediateDTE));
	dte->source = 1;
	memset(dte->data, 0, 14);
	dte->dataLength = 0;
	return (GF_GenericDTE *)dte;
}

GF_GenericDTE *New_SampleDTE()
{
	GF_SampleDTE *dte = malloc(sizeof(GF_SampleDTE));
	dte->source = 2;
	//can be -1 in QT , so init at -2
	dte->trackRefIndex = -2;
	dte->dataLength = 0;
	dte->sampleNumber = 0;
	dte->samplesPerComp = 1;
	dte->byteOffset = 0;
	dte->bytesPerComp = 1;
	return (GF_GenericDTE *)dte;
}

GF_GenericDTE *New_StreamDescDTE()
{
	GF_StreamDescDTE *dte = malloc(sizeof(GF_StreamDescDTE));
	dte->source = 3;
	dte->byteOffset = 0;
	dte->dataLength = 0;
	dte->reserved = 0;
	dte->streamDescIndex = 0;
	//can be -1 in QT , so init at -2
	dte->trackRefIndex = -2;
	return (GF_GenericDTE *)dte;
}

//creation of DTEs
GF_GenericDTE *NewDTE(u8 type)
{
	switch (type) {
	case 0:
		return New_EmptyDTE();
	case 1:
		return New_ImmediateDTE();
	case 2:
		return New_SampleDTE();
	case 3:
		return New_StreamDescDTE();
	default:
		return NULL;
	}
}

/********************************************************************
		Deletion of DataTable entries in the RTP sample
********************************************************************/
void Del_EmptyDTE(GF_EmptyDTE *dte)
{ free(dte); }

void Del_ImmediateDTE(GF_ImmediateDTE *dte)
{ free(dte); }

void Del_SampleDTE(GF_SampleDTE *dte)
{ free(dte); }

void Del_StreamDescDTE(GF_StreamDescDTE *dte)
{ free(dte); }

//deletion of DTEs
void DelDTE(GF_GenericDTE *dte)
{
	switch (dte->source) {
	case 0:
		Del_EmptyDTE((GF_EmptyDTE *)dte);
		break;
	case 1:
		Del_ImmediateDTE((GF_ImmediateDTE *)dte);
		break;
	case 2:
		Del_SampleDTE((GF_SampleDTE *)dte);
		break;
	case 3:
		Del_StreamDescDTE((GF_StreamDescDTE *)dte);
		break;
	default:
		return;
	}
}



/********************************************************************
		Reading of DataTable entries in the RTP sample
********************************************************************/
GF_Err Read_EmptyDTE(GF_EmptyDTE *dte, GF_BitStream *bs)
{
	char empty[15];
	//empty but always 15 bytes !!!
	gf_bs_read_data(bs, empty, 15);
	return GF_OK;
}

GF_Err Read_ImmediateDTE(GF_ImmediateDTE *dte, GF_BitStream *bs)
{
	dte->dataLength = gf_bs_read_u8(bs);
	if (dte->dataLength > 14) return GF_ISOM_INVALID_FILE;
	gf_bs_read_data(bs, dte->data, dte->dataLength);
	gf_bs_read_data(bs, dte->data, 14 - dte->dataLength);
	return GF_OK;
}

GF_Err Read_SampleDTE(GF_SampleDTE *dte, GF_BitStream *bs)
{
	dte->trackRefIndex = gf_bs_read_u8(bs);
	dte->dataLength = gf_bs_read_u16(bs);
	dte->sampleNumber = gf_bs_read_u32(bs);
	dte->byteOffset = gf_bs_read_u32(bs);
	dte->bytesPerComp = gf_bs_read_u16(bs);
	dte->samplesPerComp = gf_bs_read_u16(bs);
	return GF_OK;
}

GF_Err Read_StreamDescDTE(GF_StreamDescDTE *dte, GF_BitStream *bs)
{
	dte->trackRefIndex = gf_bs_read_u8(bs);
	dte->dataLength = gf_bs_read_u16(bs);
	dte->streamDescIndex = gf_bs_read_u32(bs);
	dte->byteOffset = gf_bs_read_u32(bs);
	dte->reserved = gf_bs_read_u32(bs);
	return GF_OK;
}

GF_Err ReadDTE(GF_GenericDTE *dte, GF_BitStream *bs)
{
	switch (dte->source) {
	case 0:
		//nothing to o, it is an empty entry
		return Read_EmptyDTE((GF_EmptyDTE *)dte, bs);
	case 1:
		return Read_ImmediateDTE((GF_ImmediateDTE *)dte, bs);
	case 2:
		return Read_SampleDTE((GF_SampleDTE *)dte, bs);
	case 3:
		return Read_StreamDescDTE((GF_StreamDescDTE *)dte, bs);
	default:
		return GF_ISOM_INVALID_FILE;
	}
}

/********************************************************************
		Writing of DataTable entries in the RTP sample
********************************************************************/
GF_Err Write_EmptyDTE(GF_EmptyDTE *dte, GF_BitStream *bs)
{
	gf_bs_write_u8(bs, dte->source);
	//empty but always 15 bytes !!!
	gf_bs_write_data(bs, "empty hint DTE", 15);
	return GF_OK;
}

GF_Err Write_ImmediateDTE(GF_ImmediateDTE *dte, GF_BitStream *bs)
{
	char data[14];
	gf_bs_write_u8(bs, dte->source);
	gf_bs_write_u8(bs, dte->dataLength);
	gf_bs_write_data(bs, dte->data, dte->dataLength);
	if (dte->dataLength < 14) {
		memset(data, 0, 14);
		gf_bs_write_data(bs, data, 14 - dte->dataLength);
	}
	return GF_OK;
}

GF_Err Write_SampleDTE(GF_SampleDTE *dte, GF_BitStream *bs)
{
	gf_bs_write_u8(bs, dte->source);
	gf_bs_write_u8(bs, dte->trackRefIndex);
	gf_bs_write_u16(bs, dte->dataLength);
	gf_bs_write_u32(bs, dte->sampleNumber);
	gf_bs_write_u32(bs, dte->byteOffset);
	gf_bs_write_u16(bs, dte->bytesPerComp);
	gf_bs_write_u16(bs, dte->samplesPerComp);
	return GF_OK;
}

GF_Err Write_StreamDescDTE(GF_StreamDescDTE *dte, GF_BitStream *bs)
{
	gf_bs_write_u8(bs, dte->source);

	gf_bs_write_u8(bs, dte->trackRefIndex);
	gf_bs_write_u16(bs, dte->dataLength);
	gf_bs_write_u32(bs, dte->streamDescIndex);
	gf_bs_write_u32(bs, dte->byteOffset);
	gf_bs_write_u32(bs, dte->reserved);
	return GF_OK;
}

GF_Err WriteDTE(GF_GenericDTE *dte, GF_BitStream *bs)
{
	switch (dte->source) {
	case 0:
		//nothing to do, it is an empty entry
		return Write_EmptyDTE((GF_EmptyDTE *)dte, bs);
	case 1:
		return Write_ImmediateDTE((GF_ImmediateDTE *)dte, bs);
	case 2:
		return Write_SampleDTE((GF_SampleDTE *)dte, bs);
	case 3:
		return Write_StreamDescDTE((GF_StreamDescDTE *)dte, bs);
	default:
		return GF_ISOM_INVALID_FILE;
	}
}

GF_Err OffsetDTE(GF_GenericDTE *dte, u32 offset, u32 HintSampleNumber)
{
	GF_SampleDTE *sDTE;
	//offset shifting is only true for intra sample reference
	switch (dte->source) {
	case 2:
		break;
	default:
		return GF_OK;
	}
	
	sDTE = (GF_SampleDTE *)dte;
	//we only adjust for intra HintTrack reference
	if (sDTE->trackRefIndex != -1) return GF_OK;
	//and in the same sample
	if (sDTE->sampleNumber != HintSampleNumber) return GF_OK;
	sDTE->byteOffset += offset;
	return GF_OK;
}

GF_RTPPacket *New_RTPPacket()
{
	GF_RTPPacket *tmp = malloc(sizeof(GF_RTPPacket));
	tmp->TLV = gf_list_new();
	tmp->DataTable = gf_list_new();
	tmp->B_bit = tmp->M_bit = tmp->P_bit = tmp->payloadType = tmp->payloadType = tmp->R_bit = tmp->X_bit = 0;
	tmp->relativeTransTime = 0;
	tmp->SequenceNumber = 0;
	return tmp;
}

void Del_RTPPacket(GF_RTPPacket *ptr)
{
	GF_GenericDTE *p;
	//the DTE
	while (gf_list_count(ptr->DataTable)) {
		p = gf_list_get(ptr->DataTable, 0);
		DelDTE(p);
		gf_list_rem(ptr->DataTable, 0);
	}
	gf_list_del(ptr->DataTable);
	//the TLV
	gf_isom_box_array_del(ptr->TLV);
	free(ptr);
}

GF_Err Read_RTPPacket(GF_RTPPacket *ptr, GF_BitStream *bs)
{
	GF_Err e;
	u8 hasTLV, type;
	u16 i, count;
	u32 TLVsize, tempSize;
	GF_GenericDTE *dte;
	GF_Box *a;

	ptr->relativeTransTime = gf_bs_read_u32(bs);
	//RTP Header
	//1- reserved fields
	gf_bs_read_int(bs, 2);
	ptr->P_bit = gf_bs_read_int(bs, 1);
	ptr->X_bit = gf_bs_read_int(bs, 1);
	gf_bs_read_int(bs, 4);
	ptr->M_bit = gf_bs_read_int(bs, 1);
	ptr->payloadType = gf_bs_read_int(bs, 7);

	ptr->SequenceNumber = gf_bs_read_u16(bs);
	gf_bs_read_int(bs, 13);
	hasTLV = gf_bs_read_int(bs, 1);
	ptr->B_bit = gf_bs_read_int(bs, 1);
	ptr->R_bit = gf_bs_read_int(bs, 1);
	count = gf_bs_read_u16(bs);
	
	//read the TLV
	if (hasTLV) {
		tempSize = 4;	//TLVsize includes its field length 
		TLVsize = gf_bs_read_u32(bs);
		while (tempSize < TLVsize) {
			u64 sr;
			e = gf_isom_parse_box(&a, bs, &sr);
			if (e) return e;
			gf_list_add(ptr->TLV, a);
			tempSize += (u32) a->size;
		}
		if (tempSize != TLVsize) return GF_ISOM_INVALID_FILE;
	}

	//read the DTEs
	for (i=0; i<count; i++) {
		type = gf_bs_read_u8(bs);
		dte = NewDTE(type);
		e = ReadDTE(dte, bs);
		if (e) return e;
		gf_list_add(ptr->DataTable, dte);
	}
	return GF_OK;
}

GF_Err Offset_RTPPacket(GF_RTPPacket *ptr, u32 offset, u32 HintSampleNumber)
{
	u32 count, i;
	GF_GenericDTE *dte;
	GF_Err e;
	
	count = gf_list_count(ptr->DataTable);

	for (i=0; i<count; i++) {
		dte = gf_list_get(ptr->DataTable, i);
		e = OffsetDTE(dte, offset, HintSampleNumber);
		if (e) return e;
	}
	return GF_OK;
}

//Gets the REAL size of the packet once rebuild, but without CSRC fields in the
//header
u32 Length_RTPPacket(GF_RTPPacket *ptr)
{
	u32 size, count, i;
	GF_GenericDTE *dte;

	//64 bit header
	size = 8;
	//32 bit SSRC
	size += 4;
	count = gf_list_count(ptr->DataTable);
	for (i=0; i<count; i++) {
		dte = gf_list_get(ptr->DataTable, i);
		switch (dte->source) {
		case 0:
			break;
		case 1:
			size += ((GF_ImmediateDTE *)dte)->dataLength;
			break;
		case 2:
			size += ((GF_SampleDTE *)dte)->dataLength;
			break;
		case 3:
			size += ((GF_StreamDescDTE *)dte)->dataLength;
			break;
		}
	}
	return size;
}


#ifndef GPAC_READ_ONLY

u32 Size_RTPPacket(GF_RTPPacket *ptr)
{
	GF_Box none;
	u32 size, count;
	//the RTP Header size and co
	size = 12;
	//the extra table size
	count = gf_list_count(ptr->TLV);
	if (count) {
		none.size = 4;	//WE INCLUDE THE SIZE FIELD LENGTH
		none.type = 0;
		//REMEMBER THAT TLV ENTRIES ARE 4-BYTES ALIGNED !!!
		gf_isom_box_array_size(&none, ptr->TLV);
		size += (u32) none.size;
	}
	//the DTE (each entry is 16 bytes)
	count = gf_list_count(ptr->DataTable);
	size += count * 16;
	return size;
}

GF_Err Write_RTPPacket(GF_RTPPacket *ptr, GF_BitStream *bs)
{
	GF_Err e;
	u32 TLVcount, DTEcount, i;
	GF_Box none;
	GF_GenericDTE *dte;

	gf_bs_write_u32(bs, ptr->relativeTransTime);
	//RTP Header
//	gf_bs_write_int(bs, 2, 2);
	//version is 2
	gf_bs_write_int(bs, 2, 2);
	gf_bs_write_int(bs, ptr->P_bit, 1);
	gf_bs_write_int(bs, ptr->X_bit, 1);
	gf_bs_write_int(bs, 0, 4);
	gf_bs_write_int(bs, ptr->M_bit, 1);
	gf_bs_write_int(bs, ptr->payloadType, 7);

	gf_bs_write_u16(bs, ptr->SequenceNumber);
	gf_bs_write_int(bs, 0, 13);
	TLVcount = gf_list_count(ptr->TLV);
	DTEcount = gf_list_count(ptr->DataTable);
	gf_bs_write_int(bs, TLVcount ? 1 : 0, 1);
	gf_bs_write_int(bs, ptr->B_bit, 1);
	gf_bs_write_int(bs, ptr->R_bit, 1);

	gf_bs_write_u16(bs, DTEcount);

	if (TLVcount) {
		//first write the size of the table ...
		none.size = 4;	//WE INCLUDE THE SIZE FIELD LENGTH
		none.type = 0;
		gf_isom_box_array_size(&none, ptr->TLV);
		gf_bs_write_u32(bs, (u32) none.size);
		e = gf_isom_box_array_write(&none, ptr->TLV, bs);
		if (e) return e;
	}
	//write the DTE...
	for (i = 0; i < DTEcount; i++) {
		dte = gf_list_get(ptr->DataTable, i);
		e = WriteDTE(dte, bs);
		if (e) return e;
	}
	return GF_OK;
}

#endif	//GPAC_READ_ONLY
