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

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_HINTING)

GF_Box *ghnt_box_new()
{
	GF_HintSampleEntryBox *tmp;
	GF_SAFEALLOC(tmp, GF_HintSampleEntryBox);
	if (tmp == NULL) return NULL;

	//this type is used internally for protocols that share the same base entry
	//currently only RTP uses this, but a flexMux could use this entry too...
	tmp->type = GF_ISOM_BOX_TYPE_GHNT;
	tmp->HintTrackVersion = 1;
	tmp->LastCompatibleVersion = 1;
	return (GF_Box *)tmp;
}

void ghnt_box_del(GF_Box *s)
{
	GF_HintSampleEntryBox *ptr;

	ptr = (GF_HintSampleEntryBox *)s;
	if (ptr->hint_sample) gf_isom_hint_sample_del(ptr->hint_sample);
	gf_free(ptr);
}

GF_Err ghnt_box_read(GF_Box *s, GF_BitStream *bs)
{
	GF_Err e;
	GF_HintSampleEntryBox *ptr = (GF_HintSampleEntryBox *)s;
	if (ptr == NULL) return GF_BAD_PARAM;

	if (ptr->size < 16) return GF_ISOM_INVALID_FILE;

	e = gf_isom_base_sample_entry_read((GF_SampleEntryBox *)ptr, bs);
	if (e) return e;

	ptr->HintTrackVersion = gf_bs_read_u16(bs);
	ptr->LastCompatibleVersion = gf_bs_read_u16(bs);

	ptr->size -= 12;
	if ((s->type == GF_ISOM_BOX_TYPE_RTP_STSD) || (s->type == GF_ISOM_BOX_TYPE_SRTP_STSD) || (s->type == GF_ISOM_BOX_TYPE_RRTP_STSD) || (s->type == GF_ISOM_BOX_TYPE_RTCP_STSD)) {
		ptr->MaxPacketSize = gf_bs_read_u32(bs);
		ptr->size -= 4;
	} else if (s->type == GF_ISOM_BOX_TYPE_FDP_STSD) {
		ptr->partition_entry_ID = gf_bs_read_u16(bs);
		ptr->FEC_overhead = gf_bs_read_u16(bs);
		ptr->size -= 4;

	}
	return gf_isom_box_array_read(s, bs, NULL);
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err ghnt_box_write(GF_Box *s, GF_BitStream *bs)
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
	return GF_OK;
}

GF_Err ghnt_box_size(GF_Box *s)
{
	GF_HintSampleEntryBox *ptr = (GF_HintSampleEntryBox *)s;
	ptr->size += 16;
	return GF_OK;
}


#endif /*GPAC_DISABLE_ISOM_WRITE*/


GF_HintSample *gf_isom_hint_sample_new(u32 ProtocolType)
{
	GF_HintSample *tmp;
	switch (ProtocolType) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		break;
	case GF_ISOM_BOX_TYPE_FDP_STSD:
		return (GF_HintSample *) gf_isom_box_new(GF_ISOM_BOX_TYPE_FDSA);
		break;
	default:
		return NULL;
	}
	GF_SAFEALLOC(tmp, GF_HintSample);
	if (!tmp) return NULL;
	tmp->packetTable = gf_list_new();
	tmp->hint_subtype = ProtocolType;
	return tmp;
}

void gf_isom_hint_sample_del(GF_HintSample *ptr)
{
	GF_HintPacket *pck;

	if (ptr->hint_subtype==GF_ISOM_BOX_TYPE_FDP_STSD) {
		gf_isom_box_del((GF_Box*)ptr);
		return;
	}

	while (gf_list_count(ptr->packetTable)) {
		pck = (GF_HintPacket *)gf_list_get(ptr->packetTable, 0);
		gf_isom_hint_pck_del(pck);
		gf_list_rem(ptr->packetTable, 0);
	}
	gf_list_del(ptr->packetTable);
	if (ptr->AdditionalData) gf_free(ptr->AdditionalData);

	if (ptr->sample_cache) {
		while (gf_list_count(ptr->sample_cache)) {
			GF_HintDataCache *hdc = (GF_HintDataCache *)gf_list_get(ptr->sample_cache, 0);
			gf_list_rem(ptr->sample_cache, 0);
			if (hdc->samp) gf_isom_sample_del(&hdc->samp);
			gf_free(hdc);
		}
		gf_list_del(ptr->sample_cache);
	}
	if (ptr->extra_data)
		gf_isom_box_del((GF_Box*)ptr->extra_data);
	if (ptr->child_boxes)
		gf_isom_box_array_del(ptr->child_boxes);

	gf_free(ptr);
}

GF_Err gf_isom_hint_sample_read(GF_HintSample *ptr, GF_BitStream *bs, u32 sampleSize)
{
	u16 i;
	u32 type;
	GF_HintPacket *pck;
	GF_Err e;
	char *szName = (ptr->hint_subtype==GF_ISOM_BOX_TYPE_RTCP_STSD) ? "RTCP" : "RTP";
	u64 sizeIn, sizeOut;

	sizeIn = gf_bs_available(bs);

	switch (ptr->hint_subtype) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		break;
	case GF_ISOM_BOX_TYPE_FDP_STSD:
		ptr->size = gf_bs_read_u32(bs);
		type = gf_bs_read_u32(bs);
		if (type != GF_ISOM_BOX_TYPE_FDSA) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso] invalid FDT sample, top box type %s not fdsa\n", gf_4cc_to_str(type) ));
			return GF_ISOM_INVALID_MEDIA;
		}
		return gf_isom_box_read((GF_Box*)ptr, bs);
	default:
		return GF_NOT_SUPPORTED;
	}

	ptr->packetCount = gf_bs_read_u16(bs);
	ptr->reserved = gf_bs_read_u16(bs);
	if (ptr->packetCount>=sampleSize) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso] broken %s sample: %d packet_count indicated but only %d bytes in samples\n", szName, ptr->packetCount, sampleSize));
		return GF_ISOM_INVALID_MEDIA;
	}
	
	for (i = 0; i < ptr->packetCount; i++) {
		if (! gf_bs_available(bs) ) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso] %s hint sample has no more data but still %d entries to read\n", szName, ptr->packetCount-i));
			return GF_ISOM_INVALID_MEDIA;
		}
		pck = gf_isom_hint_pck_new(ptr->hint_subtype);
		pck->trackID = ptr->trackID;
		pck->sampleNumber = ptr->sampleNumber;
		gf_list_add(ptr->packetTable, pck);

		e = gf_isom_hint_pck_read(pck, bs);
		if (e) return e;
	}

	if (ptr->hint_subtype==GF_ISOM_BOX_TYPE_RTCP_STSD) return GF_OK;


	sizeOut = gf_bs_available(bs) - sizeIn;

	//do we have some more data after the packets ??
	if ((u32)sizeOut < sampleSize) {
		ptr->dataLength = sampleSize - (u32)sizeOut;
		ptr->AdditionalData = (char*)gf_malloc(sizeof(char) * ptr->dataLength);
		gf_bs_read_data(bs, ptr->AdditionalData, ptr->dataLength);
	}
	return GF_OK;
}


#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_hint_sample_write(GF_HintSample *ptr, GF_BitStream *bs)
{
	u32 count, i;
	GF_HintPacket *pck;
	GF_Err e;

	if (ptr->hint_subtype==GF_ISOM_BOX_TYPE_FDP_STSD) {
		e = gf_isom_box_size((GF_Box*)ptr);
		if (!e) e = gf_isom_box_write((GF_Box*)ptr, bs);
		return e;
	}

	count = gf_list_count(ptr->packetTable);
	gf_bs_write_u16(bs, count);
	gf_bs_write_u16(bs, ptr->reserved);
	//write the packet table
	for (i=0; i<count; i++) {
		pck = (GF_HintPacket *)gf_list_get(ptr->packetTable, i);
		e = gf_isom_hint_pck_write(pck, bs);
		if (e) return e;
	}
	//write additional data
	if (ptr->AdditionalData) {
		gf_bs_write_data(bs, ptr->AdditionalData, ptr->dataLength);
	}
	return GF_OK;
}


u32 gf_isom_hint_sample_size(GF_HintSample *ptr)
{
	u32 size, count, i;
	GF_HintPacket *pck;

	if (ptr->hint_subtype==GF_ISOM_BOX_TYPE_FDP_STSD) {
		gf_isom_box_size((GF_Box*)ptr);
		size = (u32) ptr->size;
	} else {
		size = 4;
		count = gf_list_count(ptr->packetTable);
		for (i=0; i<count; i++) {
			pck = (GF_HintPacket *)gf_list_get(ptr->packetTable, i);
			size += gf_isom_hint_pck_size(pck);
		}
		size += ptr->dataLength;
	}
	return size;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_HintPacket *gf_isom_hint_pck_new(u32 HintType)
{
	GF_HintPacket *pck;
	switch (HintType) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		pck = (GF_HintPacket *) gf_isom_hint_rtp_new();
		if (pck) pck->hint_subtype = HintType;
		return pck;
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		pck = (GF_HintPacket *) gf_isom_hint_rtcp_new();
		if (pck) pck->hint_subtype = HintType;
		return pck;
	default:
		return NULL;
	}
}

void gf_isom_hint_pck_del(GF_HintPacket *ptr)
{
	if (!ptr) return;
	switch (ptr->hint_subtype) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		gf_isom_hint_rtp_del((GF_RTPPacket *)ptr);
		break;
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		gf_isom_hint_rtcp_del((GF_RTCPPacket *)ptr);
		break;
	default:
		break;
	}
}

GF_Err gf_isom_hint_pck_read(GF_HintPacket *ptr, GF_BitStream *bs)
{
	if (!ptr) return GF_BAD_PARAM;
	switch (ptr->hint_subtype) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		return gf_isom_hint_rtp_read((GF_RTPPacket *)ptr, bs);
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		return gf_isom_hint_rtcp_read((GF_RTCPPacket *)ptr, bs);
	default:
		return GF_NOT_SUPPORTED;
	}
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_hint_pck_write(GF_HintPacket *ptr, GF_BitStream *bs)
{
	if (!ptr) return GF_BAD_PARAM;
	switch (ptr->hint_subtype) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		return gf_isom_hint_rtp_write((GF_RTPPacket *)ptr, bs);
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		return gf_isom_hint_rtcp_write((GF_RTCPPacket *)ptr, bs);
	default:
		return GF_NOT_SUPPORTED;
	}
}

u32 gf_isom_hint_pck_size(GF_HintPacket *ptr)
{
	if (!ptr) return GF_BAD_PARAM;
	switch (ptr->hint_subtype) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		return gf_isom_hint_rtp_size((GF_RTPPacket *)ptr);
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		return gf_isom_hint_rtcp_size((GF_RTCPPacket *)ptr);
	default:
		return 0;
	}
}

GF_Err gf_isom_hint_pck_offset(GF_HintPacket *ptr, u32 offset, u32 HintSampleNumber)
{
	if (!ptr) return GF_BAD_PARAM;
	switch (ptr->hint_subtype) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		return gf_isom_hint_rtp_offset((GF_RTPPacket *)ptr, offset, HintSampleNumber);
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		return GF_BAD_PARAM;
	default:
		return GF_NOT_SUPPORTED;
	}
}

GF_Err gf_isom_hint_pck_add_dte(GF_HintPacket *ptr, GF_GenericDTE *dte, u8 AtBegin)
{
	if (!ptr) return GF_BAD_PARAM;
	switch (ptr->hint_subtype) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		if (AtBegin)
			return gf_list_insert( ((GF_RTPPacket *)ptr)->DataTable, dte, 0);
		else
			return gf_list_add( ((GF_RTPPacket *)ptr)->DataTable, dte);

	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		return GF_BAD_PARAM;
	default:
		return GF_NOT_SUPPORTED;
	}
}

u32 gf_isom_hint_pck_length(GF_HintPacket *ptr)
{
	if (!ptr) return 0;
	switch (ptr->hint_subtype) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		return gf_isom_hint_rtp_length((GF_RTPPacket *)ptr);
	case GF_ISOM_BOX_TYPE_RTCP_STSD:
		return gf_isom_hint_rtcp_length((GF_RTCPPacket *)ptr);
	default:
		return 0;
	}
}


#endif /*GPAC_DISABLE_ISOM_WRITE*/



/********************************************************************
		Creation of DataTable entries in the RTP sample
********************************************************************/

GF_GenericDTE *New_EmptyDTE()
{
	GF_EmptyDTE *dte = (GF_EmptyDTE *)gf_malloc(sizeof(GF_EmptyDTE));
	dte->source = 0;
	return (GF_GenericDTE *)dte;
}

GF_GenericDTE *New_ImmediateDTE()
{
	GF_ImmediateDTE *dte;
	GF_SAFEALLOC(dte, GF_ImmediateDTE);
	if (dte) {
		dte->source = 1;
		dte->dataLength = 0;
	}
	return (GF_GenericDTE *)dte;
}

GF_GenericDTE *New_SampleDTE()
{
	GF_SampleDTE *dte = (GF_SampleDTE *)gf_malloc(sizeof(GF_SampleDTE));
	dte->source = 2;
	//can be -1 in QT , so init at -2
	dte->trackRefIndex = (s8) -2;
	dte->dataLength = 0;
	dte->sampleNumber = 0;
	dte->samplesPerComp = 1;
	dte->byteOffset = 0;
	dte->bytesPerComp = 1;
	return (GF_GenericDTE *)dte;
}

GF_GenericDTE *New_StreamDescDTE()
{
	GF_StreamDescDTE *dte = (GF_StreamDescDTE *)gf_malloc(sizeof(GF_StreamDescDTE));
	dte->source = 3;
	dte->byteOffset = 0;
	dte->dataLength = 0;
	dte->reserved = 0;
	dte->streamDescIndex = 0;
	//can be -1 in QT , so init at -2
	dte->trackRefIndex = (s8) -2;
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
{
	gf_free(dte);
}

void Del_ImmediateDTE(GF_ImmediateDTE *dte)
{
	gf_free(dte);
}

void Del_SampleDTE(GF_SampleDTE *dte)
{
	gf_free(dte);
}

void Del_StreamDescDTE(GF_StreamDescDTE *dte)
{
	gf_free(dte);
}

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
	if (dte->dataLength < 14) gf_bs_skip_bytes(bs, 14 - dte->dataLength);
	return GF_OK;
}

GF_Err Read_SampleDTE(GF_SampleDTE *dte, GF_BitStream *bs)
{
	dte->trackRefIndex = (s8) gf_bs_read_u8(bs);
	dte->dataLength = gf_bs_read_u16(bs);
	dte->sampleNumber = gf_bs_read_u32(bs);
	dte->byteOffset = gf_bs_read_u32(bs);
	dte->bytesPerComp = gf_bs_read_u16(bs);
	dte->samplesPerComp = gf_bs_read_u16(bs);
	if (dte->bytesPerComp != 1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso] hint packet constructor with bytesperblock %d, not 1\n", dte->bytesPerComp));
	}
	if (dte->samplesPerComp != 1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso] hint packet constructor with samplesperblock %d, not 1\n", dte->bytesPerComp));
	}
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
	if (sDTE->trackRefIndex != (s8) -1) return GF_OK;
	//and in the same sample
	if (sDTE->sampleNumber != HintSampleNumber) return GF_OK;
	sDTE->byteOffset += offset;
	return GF_OK;
}

GF_RTPPacket *gf_isom_hint_rtp_new()
{
	GF_RTPPacket *tmp;
	GF_SAFEALLOC(tmp, GF_RTPPacket);
	if (!tmp) return NULL;
	tmp->TLV = gf_list_new();
	tmp->DataTable = gf_list_new();
	return tmp;
}

void gf_isom_hint_rtp_del(GF_RTPPacket *ptr)
{
	GF_GenericDTE *p;
	//the DTE
	while (gf_list_count(ptr->DataTable)) {
		p = (GF_GenericDTE *)gf_list_get(ptr->DataTable, 0);
		DelDTE(p);
		gf_list_rem(ptr->DataTable, 0);
	}
	gf_list_del(ptr->DataTable);
	//the TLV
	gf_isom_box_array_del(ptr->TLV);
	gf_free(ptr);
}

GF_Err gf_isom_hint_rtp_read(GF_RTPPacket *ptr, GF_BitStream *bs)
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
			e = gf_isom_box_parse(&a, bs);
			if (e) return e;
			gf_list_add(ptr->TLV, a);
			tempSize += (u32) a->size;
		}
		if (tempSize != TLVsize) return GF_ISOM_INVALID_FILE;
	}

	//read the DTEs
	for (i=0; i<count; i++) {
		Bool add_it = 0;
		type = gf_bs_read_u8(bs);
		dte = NewDTE(type);
		if (!dte) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso] invalid DTE code %d in hint sample %d of trackID %d\n", type, ptr->sampleNumber, ptr->trackID));
			return GF_ISOM_INVALID_FILE;
		}
		e = ReadDTE(dte, bs);
		if (e) return e;
		/*little opt, remove empty dte*/
		switch (type) {
		case 1:
			if ( ((GF_ImmediateDTE *)dte)->dataLength) add_it = 1;
			break;
		case 2:
			if ( ((GF_SampleDTE *)dte)->dataLength) add_it = 1;
			break;
		case 3:
			if ( ((GF_StreamDescDTE *)dte)->dataLength) add_it = 1;
			break;
		}
		if (add_it)
			gf_list_add(ptr->DataTable, dte);
		else
			DelDTE(dte);
	}
	return GF_OK;
}

GF_Err gf_isom_hint_rtp_offset(GF_RTPPacket *ptr, u32 offset, u32 HintSampleNumber)
{
	u32 count, i;
	GF_GenericDTE *dte;
	GF_Err e;

	count = gf_list_count(ptr->DataTable);
	for (i=0; i<count; i++) {
		dte = (GF_GenericDTE *)gf_list_get(ptr->DataTable, i);
		e = OffsetDTE(dte, offset, HintSampleNumber);
		if (e) return e;
	}
	return GF_OK;
}

//Gets the REAL size of the packet once rebuild, but without CSRC fields in the
//header
u32 gf_isom_hint_rtp_length(GF_RTPPacket *ptr)
{
	u32 size, count, i;
	GF_GenericDTE *dte;

	//64 bit header
	size = 8;
	//32 bit SSRC
	size += 4;
	count = gf_list_count(ptr->DataTable);
	for (i=0; i<count; i++) {
		dte = (GF_GenericDTE *)gf_list_get(ptr->DataTable, i);
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


#ifndef GPAC_DISABLE_ISOM_WRITE

u32 gf_isom_hint_rtp_size(GF_RTPPacket *ptr)
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

GF_Err gf_isom_hint_rtp_write(GF_RTPPacket *ptr, GF_BitStream *bs)
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
		dte = (GF_GenericDTE *)gf_list_get(ptr->DataTable, i);
		e = WriteDTE(dte, bs);
		if (e) return e;
	}
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



GF_RTCPPacket *gf_isom_hint_rtcp_new()
{
	GF_RTCPPacket *tmp;
	GF_SAFEALLOC(tmp, GF_RTCPPacket);
	return tmp;
}

void gf_isom_hint_rtcp_del(GF_RTCPPacket *ptr)
{
	if(ptr->data) gf_free(ptr->data);
	gf_free(ptr);
}

GF_Err gf_isom_hint_rtcp_read(GF_RTCPPacket *ptr, GF_BitStream *bs)
{
	//RTCP Header

	ptr->Version = gf_bs_read_int(bs, 2);
	ptr->Padding = gf_bs_read_int(bs, 1);
	ptr->Count = gf_bs_read_int(bs, 5);
	ptr->PayloadType = gf_bs_read_u8(bs);
	ptr->length = 4 * gf_bs_read_u16(bs);
	if (ptr->length<4) return GF_ISOM_INVALID_MEDIA;

	//remove header size
	if (gf_bs_available(bs) < ptr->length) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso] RTCP hint packet has more data (%d) than available\n", ptr->length ));
		return GF_ISOM_INVALID_MEDIA;
	}
	ptr->data = gf_malloc(sizeof(char) * ptr->length);
	gf_bs_read_data(bs, ptr->data, ptr->length);
	return GF_OK;
}


//Gets the REAL size of the packet once rebuild, but without CSRC fields in the
//header
u32 gf_isom_hint_rtcp_length(GF_RTCPPacket *ptr)
{
	return 4 * (ptr->length + 1);
}


#ifndef GPAC_DISABLE_ISOM_WRITE

u32 gf_isom_hint_rtcp_size(GF_RTCPPacket *ptr)
{
	return 4 * (ptr->length + 1);
}

GF_Err gf_isom_hint_rtcp_write(GF_RTCPPacket *ptr, GF_BitStream *bs)
{
	//RTP Header
	gf_bs_write_int(bs, ptr->Version, 2);
	gf_bs_write_int(bs, ptr->Padding, 1);
	gf_bs_write_int(bs, ptr->Count, 5);
	gf_bs_write_u8(bs, ptr->PayloadType);
	gf_bs_write_u16(bs, 4*ptr->length);
	gf_bs_write_data(bs, ptr->data, ptr->length);
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/



#if 0 //unused

/*small hint reader - performs data caching*/

/*resets hint reading parameters, returns an error if the hint type is not supported for reading
packet sequence number is always reseted to 0
@sample_start: indicates from where the packets should be read (regular 1-based sample number)
@ts_offset: constant offset for timestamps, must be expressed in media timescale (which is the hint timescale).
	usually 0 (no offset)
@sn_offset: offset for packet sequence number (first packet will have a SN of 1 + sn_offset)
	usually 0
@ssrc: sync source identifier for RTP
*/

GF_Err gf_isom_reset_hint_reader(GF_ISOFile *the_file, u32 trackNumber, u32 sample_start, u32 ts_offset, u32 sn_offset, u32 ssrc)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_HintSampleEntryBox *entry;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (!sample_start) return GF_BAD_PARAM;
	if (sample_start>=trak->Media->information->sampleTable->SampleSize->sampleCount) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, 1, (GF_SampleEntryBox **) &entry, NULL);
	if (e) return e;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		break;
	default:
		return GF_NOT_SUPPORTED;
	}

	entry->hint_ref = NULL;
	e = Track_FindRef(trak, GF_ISOM_REF_HINT, &entry->hint_ref);
	if (e) return e;

	entry->cur_sample = sample_start;
	entry->pck_sn = 1 + sn_offset;
	entry->ssrc = ssrc;
	entry->ts_offset = ts_offset;
	if (entry->hint_sample) gf_isom_hint_sample_del(entry->hint_sample);
	entry->hint_sample = NULL;
	return GF_OK;
}

static GF_Err gf_isom_load_next_hint_sample(GF_ISOFile *the_file, u32 trackNumber, GF_TrackBox *trak, GF_HintSampleEntryBox *entry)
{
	GF_BitStream *bs;
	u32 descIdx;
	GF_ISOSample *samp;

	if (!entry->cur_sample) return GF_BAD_PARAM;
	if (entry->cur_sample>trak->Media->information->sampleTable->SampleSize->sampleCount) return GF_EOS;

	samp = gf_isom_get_sample(the_file, trackNumber, entry->cur_sample, &descIdx);
	if (!samp) return GF_IO_ERR;
	entry->cur_sample++;

	if (entry->hint_sample) gf_isom_hint_sample_del(entry->hint_sample);

	bs = gf_bs_new(samp->data, samp->dataLength, GF_BITSTREAM_READ);
	entry->hint_sample = gf_isom_hint_sample_new(entry->type);
	gf_isom_hint_sample_read(entry->hint_sample, bs, samp->dataLength);
	gf_bs_del(bs);
	entry->hint_sample->TransmissionTime = samp->DTS;
	gf_isom_sample_del(&samp);
	entry->hint_sample->sample_cache = gf_list_new();
	return GF_OK;
}

static GF_ISOSample *gf_isom_get_data_sample(GF_HintSample *hsamp, GF_TrackBox *trak, u32 sample_num)
{
	GF_ISOSample *samp;
	GF_HintDataCache *hdc;
	u32 i, count;
	count = gf_list_count(hsamp->sample_cache);
	for (i=0; i<count; i++) {
		hdc = (GF_HintDataCache *)gf_list_get(hsamp->sample_cache, i);
		if ((hdc->sample_num==sample_num) && (hdc->trak==trak)) return hdc->samp;
	}

	samp = gf_isom_sample_new();
	Media_GetSample(trak->Media, sample_num, &samp, &i, 0, NULL);
	if (!samp) return NULL;
	GF_SAFEALLOC(hdc, GF_HintDataCache);
	if (!hdc) return NULL;
	hdc->samp = samp;
	hdc->sample_num = sample_num;
	hdc->trak = trak;
	/*we insert all new samples, since they're more likely to be fetched next (except for audio
	interleaving and other multiplex)*/
	gf_list_insert(hsamp->sample_cache, hdc, 0);
	return samp;
}

/*reads next hint packet. ALl packets are read in transmission (decoding) order
returns an error if not supported, or GF_EOS when no more packets are available
currently only RTP reader is supported
@pck_data, @pck_size: output packet data (must be freed by caller) - contains all info to be sent
	on the wire, eg for RTP contains the RTP header and the data
@disposable (optional): indicates that the packet can be dropped when late (B-frames & co)
@repeated (optional): indicates this is a repeated packet (same one has already been sent)
@trans_ts (optional): indicates the transmission time of the packet, expressed in hint timescale, taking into account
the ts_offset specified in gf_isom_reset_hint_reader. Depending on packets this may not be the same
as the hint sample timestamp + ts_offset, some packets may need to be sent earlier (B-frames)
@sample_num (optional): indicates hint sample number the packet belongs to
*/
GF_Err gf_isom_next_hint_packet(GF_ISOFile *the_file, u32 trackNumber, char **pck_data, u32 *pck_size, Bool *disposable, Bool *repeated, u32 *trans_ts, u32 *sample_num)
{
	GF_RTPPacket *pck;
	GF_Err e;
	GF_BitStream *bs;
	GF_TrackBox *trak, *ref_trak;
	GF_HintSampleEntryBox *entry;
	u32 i, count, ts;
	s32 cts_off;

	*pck_data = NULL;
	*pck_size = 0;
	if (trans_ts) *trans_ts = 0;
	if (disposable) *disposable = 0;
	if (repeated) *repeated = 0;
	if (sample_num) *sample_num = 0;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	e = Media_GetSampleDesc(trak->Media, 1, (GF_SampleEntryBox **) &entry, NULL);
	if (e) return e;
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_RTP_STSD:
	case GF_ISOM_BOX_TYPE_SRTP_STSD:
	case GF_ISOM_BOX_TYPE_RRTP_STSD:
		break;
	default:
		return GF_NOT_SUPPORTED;
	}

	if (!entry->hint_sample) {
		e = gf_isom_load_next_hint_sample(the_file, trackNumber, trak, entry);
		if (e) return e;
	}
	pck = (GF_RTPPacket *)gf_list_get(entry->hint_sample->packetTable, 0);
	gf_list_rem(entry->hint_sample->packetTable, 0);
	if (!pck) return GF_BAD_PARAM;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	/*write RTP header*/
	gf_bs_write_int(bs, 2, 2);	/*version*/
	gf_bs_write_int(bs, pck->P_bit, 1);	/*P bit*/
	gf_bs_write_int(bs, pck->X_bit, 1);	/*X bit*/
	gf_bs_write_int(bs, 0, 4);	/*CSRC count*/
	gf_bs_write_int(bs, pck->M_bit, 1);	/*M bit*/
	gf_bs_write_int(bs, pck->payloadType, 7);	/*payt*/
	gf_bs_write_u16(bs, entry->pck_sn);	/*seq num*/
	entry->pck_sn++;

	/*look for CTS offset in TLV*/
	cts_off = 0;
	count = gf_list_count(pck->TLV);
	for (i=0; i<count; i++) {
		GF_RTPOBox *rtpo = (GF_RTPOBox *)gf_list_get(pck->TLV, i);
		if (rtpo->type == GF_ISOM_BOX_TYPE_RTPO) {
			cts_off = rtpo->timeOffset;
			break;
		}
	}
	/*TS - TODO check TS wrapping*/
	ts = (u32) (entry->hint_sample->TransmissionTime + pck->relativeTransTime + entry->ts_offset + cts_off);
	gf_bs_write_u32(bs, ts );
	gf_bs_write_u32(bs, entry->ssrc);	/*SSRC*/

	/*then build all data*/
	count = gf_list_count(pck->DataTable);
	for (i=0; i<count; i++) {
		GF_GenericDTE *dte = (GF_GenericDTE *)gf_list_get(pck->DataTable, i);
		switch (dte->source) {
		/*empty*/
		case 0:
			break;
		/*immediate data*/
		case 1:
			gf_bs_write_data(bs, ((GF_ImmediateDTE *)dte)->data, ((GF_ImmediateDTE *)dte)->dataLength);
			break;
		/*sample data*/
		case 2:
		{
			GF_ISOSample *samp;
			GF_SampleDTE *sdte = (GF_SampleDTE *)dte;
			/*get track if not this one*/
			if (sdte->trackRefIndex != (s8) -1) {
				if (!entry->hint_ref || !entry->hint_ref->trackIDs) {
					gf_isom_hint_rtp_del(pck);
					gf_bs_del(bs);
					return GF_ISOM_INVALID_FILE;
				}
				ref_trak = gf_isom_get_track_from_id(trak->moov, entry->hint_ref->trackIDs[(u32)sdte->trackRefIndex]);
			} else {
				ref_trak = trak;
			}
			samp = gf_isom_get_data_sample(entry->hint_sample, ref_trak, sdte->sampleNumber);
			if (!samp) {
				gf_isom_hint_rtp_del(pck);
				gf_bs_del(bs);
				return GF_IO_ERR;
			}
			gf_bs_write_data(bs, samp->data + sdte->byteOffset, sdte->dataLength);
		}
		break;
		/*sample desc data - currently NOT SUPPORTED !!!*/
		case 3:
			break;
		}
	}
	if (trans_ts) *trans_ts = ts;
	if (disposable) *disposable = pck->B_bit;
	if (repeated) *repeated = pck->R_bit;
	if (sample_num) *sample_num = entry->cur_sample-1;

	gf_bs_get_content(bs, pck_data, pck_size);
	gf_bs_del(bs);
	gf_isom_hint_rtp_del(pck);
	if (!gf_list_count(entry->hint_sample->packetTable)) {
		gf_isom_hint_sample_del(entry->hint_sample);
		entry->hint_sample = NULL;
	}
	return GF_OK;
}

#endif


#endif /* !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_HINTING)*/
